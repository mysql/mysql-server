/*
 Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public Licensein
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

#include <my_config.h>
#include <stdio.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/errno.h>
#define __STDC_FORMAT_MACROS 
#include <inttypes.h>

/* Memcache headers */
#include "memcached/types.h"
#include <memcached/extension_loggers.h>

#include "timing.h"
#include "debug.h"
#include "Configuration.h"
#include "thread_identifier.h"
#include "workitem.h"
#include "ndb_worker.h"
#include "ndb_engine_errors.h"
#include "ndb_error_logger.h"

#include "S_sched.h"

extern "C" {
  void * run_send_thread(void *v);
  void * run_poll_thread(void *v);
}

extern EXTENSION_LOGGER_DESCRIPTOR *logger;

/* Scheduler Global singleton */
static S::SchedulerGlobal * s_global;

/* SchedulerGlobal methods */
S::SchedulerGlobal::SchedulerGlobal(int _nthreads) :
  GlobalConfigManager(_nthreads)
{
}


void S::SchedulerGlobal::init(const scheduler_options *sched_opts) {
  DEBUG_ENTER_METHOD("S::SchedulerGlobal::init");

  /* Set member variables */
  config_string = sched_opts->config_string;
  parse_config_string(nthreads, config_string);
  options.max_clients = sched_opts->max_clients;

  /* Fetch or initialize clusters */
  nclusters = conf->nclusters;
  clusters = new Cluster * [nclusters];
  for(int i = 0 ; i < nclusters ; i++) {
    ClusterConnectionPool *pool = conf->getConnectionPoolById(i);
    Cluster *c = (Cluster *) pool->getCustomData();
    if(c == 0) {
      c = new Cluster(this, i);
      pool->setCustomData(c);
    }
    clusters[i] = c;
    c->nreferences += 1;
  }

  /* Initialize the WorkerConnections */
  for(int t = 0 ; t < nthreads ; t++) {
    for(int c = 0 ; c < nclusters ; c++) {
      WorkerConnection **wc_cell = getWorkerConnectionPtr(t, c);
      * wc_cell = new WorkerConnection(this, t, c);
    }
  }

  /* Build Configurations for WorkerConnections */
  configureSchedulers();

  /* Start the send & poll threads for each connection */
  for(int i = 0 ; i < nclusters ; i++) 
    clusters[i]->startThreads();
  
  /* Log message for startup */
  logger->log(LOG_WARNING, 0, "Scheduler: starting for %d cluster%s; "
              "c%d,f%d,g%d,t%d", nclusters, nclusters == 1 ? "" : "s",
              options.n_connections, options.force_send, 
              options.auto_grow, options.send_timer);

  /* Now Running */
  running = true;
}


void S::SchedulerGlobal::shutdown() {
  if(running) {
    logger->log(LOG_INFO, 0, "Shutting down scheduler.");

    /* First shut down each WorkerConnection */
    for(int i = 0; i < nclusters ; i++) {
      for(int j = 0; j < options.n_worker_threads; j++) {
        S::WorkerConnection *wc = * (getWorkerConnectionPtr(j, i));
        wc->sendqueue->abort();
      }
    }

    /* Release each Cluster (and its Connections) */
    for(int i = 0; i < nclusters ; i++) {
      Cluster *c = clusters[i];
      if ( --(c->nreferences) == 0) {
        delete c;
        conf->getConnectionPoolById(i)->setCustomData(0);
      }
    }

    /* Then actually delete each WorkerConnection */
    for(int i = 0; i < nclusters ; i++) {
      for(int j = 0; j < options.n_worker_threads; j++) {
        delete * (getWorkerConnectionPtr(j, i));
        * (getWorkerConnectionPtr(j, i)) = 0;
      }
    }
    
    /* Shutdown now */
    logger->log(LOG_WARNING, 0, "Shutdown completed.");
    running = false;
  }
}


void S::SchedulerGlobal::parse_config_string(int nthreads, const char *str) {

  /* Initialize the configuration default values */
  options.n_worker_threads = nthreads;
  options.n_connections = 0;   // 0 = n_connections based on db-stored config
  options.force_send = 0;      // 0 = force send always off
  options.send_timer = 1;      // 1 = 1 ms. timer in send thread
  options.auto_grow = 1;       // 1 = allow NDB instance pool to grow on demand

  if(str) {
    const char *s = str;
    char letter;
    int value;
    
    /* tolerate a ':' at the start of the string */
    if( *s == ':') s++;
    
    while(*s != '\0' && sscanf(s, "%c%d", &letter, &value) == 2) {
      switch(letter) {
        case 'c':
          options.n_connections = value;
          break;
        case 'f':
          options.force_send = value;
          break;
        case 'g':
          options.auto_grow = value;
          break;
        case 't':
          options.send_timer = value;
          break;
      }
      /* Skip over the part just read */
      s += 1;                   // the letter
      while(isdigit(*s)) s++;   // the value
      
      /* Now tolerate a comma */
      if(*s == ',') s++;
    }
  }
  
  /* Test validity of configuration */
  if(options.force_send < 0 || options.force_send > 2) {
    logger->log(LOG_WARNING, 0, "Invalid scheduler configuration.\n");
    assert(options.force_send >= 0 || options.force_send <= 2);
  }
  if(options.n_connections < 0 || options.n_connections > 4) {
    logger->log(LOG_WARNING, 0, "Invalid scheduler configuration.\n");
    assert(options.n_connections >= 0 && options.n_connections <= 4);
  }
  if(options.send_timer < 1 || options.send_timer > 10) {
    logger->log(LOG_WARNING, 0, "Invalid scheduler configuration.\n");
    assert(options.send_timer >= 1 && options.send_timer <= 10);
  }
  if(options.auto_grow < 0 || options.auto_grow > 1) {
    logger->log(LOG_WARNING, 0, "Invalid scheduler configuration.\n");
    assert(options.auto_grow == 0 || options.auto_grow == 1);
  }
}


void S::SchedulerGlobal::add_stats(const char *stat_key,
                                   ADD_STAT add_stat, 
                                   const void *cookie) {
  if(strncasecmp(stat_key, "reconf", 6) == 0) {
    WorkerConnection ** wc = getWorkerConnectionPtr(0,0);
    (* wc)->add_stats(stat_key, add_stat, cookie);
  }
  else {
    DEBUG_PRINT(" scheduler");
    for(int c = 0 ; c < nclusters ; c++) {
      clusters[c]->add_stats(stat_key, add_stat, cookie);
    }
  }
}


/* SchedulerWorker methods */

void S::SchedulerWorker::init(int my_thread, 
                              const scheduler_options * options) {
  /* On the first call in, initialize the SchedulerGlobal.
   * This will start the send & poll threads for each connection.
   */
  if(my_thread == 0) {
    s_global = new SchedulerGlobal(options->nthreads);
    s_global->init(options);
  }
  
  /* Initialize member variables */
  id = my_thread;
}


void S::SchedulerWorker::shutdown() {
  if(id == 0)
    s_global->shutdown();
}


S::SchedulerWorker::~SchedulerWorker() {
  if(id == 0)
    delete s_global;
}


ENGINE_ERROR_CODE S::SchedulerWorker::schedule(workitem *item) {
  int c = item->prefix_info.cluster_id;
  ENGINE_ERROR_CODE response_code;
  NdbInstance *inst = 0;
  const KeyPrefix *pfx;
  S::WorkerConnection *wc;

  wc = * (s_global->getWorkerConnectionPtr(id, c));
  if(wc == 0) return ENGINE_FAILED;
    
  if(wc->freelist) {                 /* Get the next NDB from the freelist. */
    inst = wc->freelist;
    wc->freelist = inst->next;
  }
  else {                             /* No free NDBs. */
    if(wc->sendqueue->is_aborted()) {
      return ENGINE_TMPFAIL;
    }
    else {                           /* Try to make an NdbInstance on the fly */
      inst = wc->newNdbInstance();
      if(inst) {
        log_app_error(& AppError29024_autogrow);
      }
      else {
        /* We have hit a hard maximum.  Eventually Scheduler::io_completed() 
           will run _in this thread_ and return an NDB to the freelist.  
           But no other thread can free one, so here we return an error. 
         */
        log_app_error(& AppError29002_NoNDBs);
        return ENGINE_TMPFAIL;
      }
    }
  }
  
  assert(inst);
  inst->link_workitem(item);

  // Fetch the query plan for this prefix.
  pfx = wc->setQueryPlanInWorkitem(item);
  if(! item->plan) {
    DEBUG_PRINT("getPlanForPrefix() failure");
    return ENGINE_FAILED;
  }
  
  // Build the NDB transaction
  op_status_t op_status = worker_prepare_operation(item);

  // Success; put the workitem on the send queue and return ENGINE_EWOULDBLOCK.
  if(op_status == op_prepared) {
    /* Put the prepared item onto a send queue */
    wc->sendqueue->produce(inst);
    DEBUG_PRINT("%d.%d placed on send queue.", id, inst->wqitem->id);
    
    /* This locking is explained in run_ndb_send_thread() */
    if(pthread_mutex_trylock( & wc->conn->sem.lock) == 0) {  // try the lock
      wc->conn->sem.counter++;                               // increment
      pthread_cond_signal( & wc->conn->sem.not_zero);        // signal
      pthread_mutex_unlock( & wc->conn->sem.lock);           // release
    }
        
    response_code = ENGINE_EWOULDBLOCK;
  }
  else {
    /* Status is not op_prepared, but rather some error status */
    response_code = item->status->status;
  }
  
  return response_code;
}


void S::SchedulerWorker::prepare(NdbTransaction * tx, 
                                 NdbTransaction::ExecType execType, 
                                 NdbAsynchCallback callback, 
                                 workitem * item, prepare_flags flags) { 
  tx->executeAsynchPrepare(execType, callback, (void *) item);
  if(flags == RESCHEDULE) item->base.reschedule = 1;
}


void S::SchedulerWorker::close(NdbTransaction *tx, workitem *item) {
  Uint64 nwaits_pre, nwaits_post;
  Ndb * & ndb = item->ndb_instance->db;

  nwaits_pre  = ndb->getClientStat(Ndb::WaitExecCompleteCount);
  tx->close();
  nwaits_post = ndb->getClientStat(Ndb::WaitExecCompleteCount);

  if(nwaits_post > nwaits_pre) 
    log_app_error(& AppError29023_SyncClose);
}


/* Release the resources used by an operation.  
   Unlink the NdbInstance from the workitem, and return it to the free list 
   (or free it, if the scheduler is shutting down).
*/
void S::SchedulerWorker::release(workitem *item) {
  DEBUG_ENTER();
  NdbInstance *inst = item->ndb_instance;
  
  if(inst) {
    inst->unlink_workitem(item);
    int c = item->prefix_info.cluster_id;
    S::WorkerConnection * wc = * (s_global->getWorkerConnectionPtr(id, c));
    if(wc && ! wc->sendqueue->is_aborted()) {
      inst->next = wc->freelist;
      wc->freelist = inst;
      // DEBUG_PRINT("Returned NdbInstance to freelist.");
    }
    else {
      /* We are in the midst of shutting down (and possibly reconfiguring) */
      delete inst;
    }
  }
}


bool S::SchedulerWorker::global_reconfigure(Configuration *new_cf) {
  return s_global->reconfigure(new_cf);
}


void S::SchedulerWorker::add_stats(const char *stat_key,
                                   ADD_STAT add_stat, 
                                   const void *cookie) {
  s_global->add_stats(stat_key, add_stat, cookie);
}


/* Cluster methods */
S::Cluster::Cluster(SchedulerGlobal *global, int _id) : 
  threads_started(false),
  cluster_id(_id),
  nreferences(0)
{
  DEBUG_PRINT("%d", cluster_id);
  
  /* How many cluster connections are wanted? 
     If options.n_connections is zero (the default) we want one connection
     per 50,000 desired TPS.  (The default for TPS is 100,000 -- so, two 
     connections).   But if a number is specified in the config, use that 
     instead. 
  */        
  if(global->options.n_connections) {
    nconnections = global->options.n_connections;
  }
  else {  
    const int connection_tps = 50000;
    nconnections = global->conf->max_tps / connection_tps;
    if(global->conf->max_tps % connection_tps) nconnections += 1;
  }
  assert(nconnections > 0);

  /* Get our connection pool */
  ClusterConnectionPool *pool = global->conf->getConnectionPoolById(cluster_id);

  /* Some NDB Cluster Connections are already open; 
     if we want more, try to add them now. */
  // TODO: If you reconfigure too many times you run out of connections ...
  DEBUG_PRINT("Cluster %d, have %d connection(s), want %d", 
              cluster_id, pool->getPoolSize(), nconnections);
  for(int i = pool->getPoolSize(); i < nconnections ; i ++) {
    Ndb_cluster_connection * c = pool->addPooledConnection();
    if(c == 0) {
      /* unable to create any more connections */
      nconnections = i;
      break;
    }
  }

  logger->log(LOG_WARNING, 0, "Scheduler: using %d connection%s to cluster %d\n",
              nconnections, nconnections == 1 ? "" : "s", cluster_id);

  /* Instantiate the Connection objects */
  connections = new S::Connection * [nconnections];
  for(int i = 0; i < nconnections ; i++) {
    connections[i] = new S::Connection(*this, i);
  }
}


void S::Cluster::startThreads() {
  /* Threads are started only once and persist across reconfiguration.
     But, this method will be called again for each reconf. */
  if(threads_started == false) {
    for(int i = 0 ; i < nconnections; i++) {
      connections[i]->startThreads();
    }
    threads_started = true;
  }
}


S::Cluster::~Cluster() {
  DEBUG_PRINT("Shutting down cluster %d", cluster_id);
  for(int i = 0; i < nconnections ; i++) {
    delete connections[i];
  }
}


S::WorkerConnection ** S::Cluster::getWorkerConnectionPtr(int thd) const {
  return s_global->getWorkerConnectionPtr(thd, cluster_id);
}


void S::Cluster::add_stats(const char *stat_key,
                           ADD_STAT add_stat, 
                           const void *cookie) {
  for(int c = 0 ; c < nconnections ; c++) {
    connections[c]->add_stats(stat_key, add_stat, cookie);
  }
}  


/* WorkerConnection methods */


NdbInstance * S::WorkerConnection::newNdbInstance() {
  NdbInstance *inst = 0;
  if(instances.current < instances.max) {
    inst = new NdbInstance(conn->conn, 2);
    instances.current++;
    inst->id = ((id.thd + 1) * 10000) + instances.current;
  }
  return inst;
}


S::WorkerConnection::WorkerConnection(SchedulerGlobal *global,
                                      int thd_id, int cluster_id) :
  SchedulerConfigManager(thd_id, cluster_id)
{
  S::Cluster *cl = global->clusters[cluster_id];  

  id.thd = thd_id;
  id.cluster = cluster_id;
  id.conn = thd_id % cl->nconnections;  // round-robin assignment
  conn = cl->connections[id.conn];
  id.node = conn->node_id;

  /* How many NDB instances to start initially */
  instances.initial = conn->instances.initial / conn->n_workers;

  /* Maximum size of send queue, and upper bound on NDB instances */
  instances.max = conn->instances.max / conn->n_workers;

  /* Build the freelist */
  freelist = 0;
  for(instances.current = 0; instances.current < instances.initial; ) {
    NdbInstance *inst = newNdbInstance();
    inst->next = freelist;
    freelist = inst;
  }

  DEBUG_PRINT("Cluster %d, connection %d (node %d), worker %d: %d NDBs.", 
              id.cluster, id.conn, id.node, id.thd, instances.current);
  
  /* Initialize the sendqueue */
  sendqueue = new Queue<NdbInstance>(instances.max);
  
  /* Hoard a transaction (an API connect record) for each Ndb object.  This
   * first call to startTransaction() will send TC_SEIZEREQ and wait for a 
   * reply, but later at runtime startTransaction() should return immediately.
   */
  NdbTransaction ** txlist = new NdbTransaction * [instances.current];
  int i = 0;

  // Open them all.
  for(NdbInstance *inst = freelist; inst != 0 ;inst=inst->next, i++) {
    NdbTransaction *tx;
    tx = inst->db->startTransaction();
    if(! tx) log_ndb_error(inst->db->getNdbError());
    txlist[i] = tx;
  }

  // Close them all.
  for(i = 0 ; i < instances.current ; i++) {
    if(txlist[i])
      txlist[i]->close();
  }

  // Free the list.
  delete[] txlist;
}


S::WorkerConnection::~WorkerConnection() {
  DEBUG_ENTER_METHOD("S::WorkerConnection::~WorkerConnection");

  /* Delete all of the Ndbs that are not currently in use */
  NdbInstance *inst = freelist;
  while(inst != 0) {
    NdbInstance *next = inst->next;
    delete inst;
    inst = next;
  }
    
  /* Delete the sendqueue */
  delete sendqueue;
}


/* Connection methods */

S::Connection::Connection(S::Cluster & _cl, int _id) : 
  cluster(_cl), id(_id)
{
  S::SchedulerGlobal *global = s_global;
  Configuration *conf = global->conf;
  n_total_workers = global->options.n_worker_threads;
  
  /* Get the connection pool for my cluster */
  ClusterConnectionPool *pool = conf->getConnectionPoolById(cluster.cluster_id);

  /* Get my connection from the pool */
  conn = pool->getPooledConnection(id);
  node_id = conn->node_id();

  /* Set the timer on the adaptive send thread */
  conn->set_max_adaptive_send_time(global->options.send_timer);

  /* How many worker threads will use this connection? */
  n_workers = global->options.n_worker_threads / cluster.nconnections;
  if(n_total_workers % cluster.nconnections > id) n_workers += 1;  

  /* How many NDB objects are needed for the desired performance? */
  double total_ndb_objects = conf->figureInFlightTransactions(cluster.cluster_id);
  instances.initial = (int) (total_ndb_objects / cluster.nconnections);
  while(instances.initial % n_workers) instances.initial++; // round up

  /* The maximum number of NDB objects.
   * This is used to configure hard limits on the size of the waitgroup, 
   * the sentqueue, and the reschedulequeue -- and it will not be 
   * possible to increase those limits during online reconfig. 
   */
  instances.max = instances.initial;
  // allow the pool to grow on demand? 
  if(global->options.auto_grow)
    instances.max = (int) (instances.max * 1.6);
  // max_clients imposes a hard upper limit
  if(instances.max > (global->options.max_clients / cluster.nconnections))
    instances.max = global->options.max_clients / cluster.nconnections;
  // instances.initial might also be subject to the max_clients limit
  if(instances.initial > instances.max) 
    instances.initial = instances.max;
  
  /* Get a multi-wait Poll Group */
  pollgroup = conn->create_ndb_wait_group(instances.max);
      
  /* Initialize the statistics */
  stats.sent_operations = 0;
  stats.batches = 0;
  stats.timeout_races = 0;
  
  /* Initialize the semaphore */
  pthread_mutex_init(& sem.lock, NULL);
  init_condition_var(& sem.not_zero);
  sem.counter = 0;
    
  /* Initialize the queues for sent and resceduled items */
  sentqueue = new Queue<NdbInstance>(instances.max);
  reschedulequeue = new Queue<NdbInstance>(instances.max);
}


void S::Connection::startThreads() {
  /* Start the poll thread */
  pthread_create( & poll_thread_id, NULL, run_poll_thread, (void *) this);
  
  /* Start the send thread */
  pthread_create( & send_thread_id, NULL, run_send_thread, (void *) this);
}
  

S::Connection::~Connection() {
  /* Shut down a connection. 
     The send thread should send everything in its queue. 
     The poll thread should wait for everything in its waitgroup.
     Then they should both shut down. 
  */
  DEBUG_ENTER_METHOD("S::Connection::~Connection");
  pthread_join(send_thread_id, NULL);
  DEBUG_PRINT("Cluster %d connection %d send thread has quit.", 
              cluster.cluster_id, id);

  pthread_join(poll_thread_id, NULL); 
  DEBUG_PRINT("Cluster %d connection %d poll thread has quit.", 
              cluster.cluster_id, id);

  /* Delete the queues */
  assert(sentqueue->is_aborted());
  delete sentqueue;
  delete reschedulequeue;

  /* Delete the semaphore */
  pthread_cond_destroy(& sem.not_zero);
  pthread_mutex_destroy(& sem.lock);
  
  /* Release the multiwait group */
  conn->release_ndb_wait_group(pollgroup);
}


void S::Connection::add_stats(const char *stat_key,
                              ADD_STAT add_stat, 
                              const void *cookie) {
  char key[128];
  char val[128];
  int klen, vlen;

  klen = sprintf(key, "cl%d.conn%d.sent_operations", cluster.cluster_id, id);
  vlen = sprintf(val, "%llu", stats.sent_operations);
  add_stat(key, klen, val, vlen, cookie);
  
  klen = sprintf(key, "cl%d.conn%d.batches", cluster.cluster_id, id);
  vlen = sprintf(val, "%llu", stats.batches);
  add_stat(key, klen, val, vlen, cookie);

  klen = sprintf(key, "cl%d.conn%d.timeout_races", cluster.cluster_id, id);
  vlen = sprintf(val, "%llu", stats.timeout_races);
  add_stat(key, klen, val, vlen, cookie);
  
  klen = sprintf(key, "cl%d.conn%d.instances.initial", cluster.cluster_id, id);
  vlen = sprintf(val, "%d", instances.initial);
  add_stat(key, klen, val, vlen, cookie);

  klen = sprintf(key, "cl%d.conn%d.instances.max", cluster.cluster_id, id);
  vlen = sprintf(val, "%d", instances.max);
  add_stat(key, klen, val, vlen, cookie);
}                              
  

/* 
 Some design features of the send thread
 
 1:  When a worker thread has an item ready to send, it tries to acquire
 the mutex and post to the semaphore.  The send thread sleeps on the 
 semaphore's condition variable waiting for a worker to post to it.
 But if a worker thread finds the mutex already locked, it simply 
 skips posting the semaphore; some other thread must be posting anyway.
 This sets up a possible race where a worker may queue an item but the 
 send thread misses it.  Therefore the send thread always sets a timeout
 when waiting, and always examines the queues after the timer expires.
 
 2: The occurence of the race described above is recorded in the 
 stats.timeout_races counter.
 
 3: How long is the timeout? It varies from a low value when the server is
 busy to a high one when idle.  Also, when busy, we try to reduce the number
 of calls to gettimeofday() or clock_gettime() to one per timeout_msec ms
 rather than one per iteration.
*/
void * S::Connection::run_ndb_send_thread() {
  /* Set thread identity */
  thread_identifier tid;
  tid.pipeline = 0;
  snprintf(tid.name, THD_ID_NAME_LEN,
           "cl%d.conn%d.send", cluster.cluster_id, id);
  set_thread_id(&tid);
  
  DEBUG_ENTER();

  NdbInstance *readylist;     /* list of items fetched from queues */
  int nready = 0;             /* number of items on the readylist */
  int nsent = 0;              /* number sent in this iteration */
  int c_wait = 0;             /* return value from pthread_cond_timedwait() */
  struct timespec timer;
  const int timeout_min = 200;   /* "busy" server timeout */
  const int timeout_max = 3200;  /* "idle" server timeout */
  int timeout_msec = timeout_min;  
  int shutting_down = 0;

  while(1) {
    if(nsent == 0) {  /* nothing sent last time through the loop */
      if(shutting_down) {
        sentqueue->abort();
        pollgroup->wakeup();
        return 0;
      }
      
      if(timeout_msec < timeout_max) {
        timeout_msec *= 2;  /* progress from "busy" towards "idle" */
      }
      timespec_get_time(& timer);
      timespec_add_msec(& timer, timeout_msec);
    }
    
    /* Acquire the semaphore */
    pthread_mutex_lock(& sem.lock);
    if(sem.counter == 0) {
      c_wait = pthread_cond_timedwait(& sem.not_zero, & sem.lock, & timer);
    }
    sem.counter = 0;
    pthread_mutex_unlock(& sem.lock);
    
    /* There are several queues that may have NDBs ready for sending. 
       Examine all of them, and consolidate all of the ready NDBs into a 
       single list. */
    nready = 0;
    readylist = 0;
    
    /* First check the reschedule queue */
    nready += get_operations_from_queue(& readylist, reschedulequeue);

    /* Then the worker thread queues */
    for(int w = id; w < n_total_workers; w += cluster.nconnections) {
      S::WorkerConnection *wc = * (cluster.getWorkerConnectionPtr(w));
      DEBUG_ASSERT(wc->id.conn == id);
      nready += get_operations_from_queue(& readylist, wc->sendqueue);
      if(wc->sendqueue->is_aborted()) {
        shutting_down = 1;
      }
    }

    /* Now walk the readylist.  Send pending operations from the NDBs there,
       then place them on the sent-items queue for the poll thread. */
    nsent = 0;
    if(nready) {
      for(NdbInstance *inst = readylist; inst != NULL ; inst = inst->next) {
        int force = 0;
        if(nready == 1 && s_global->options.force_send == 1) {
          force = 1; // force-send the last item in the list
        }
        
        /* Send the operations */
        inst->db->sendPreparedTransactions(force);
        DEBUG_PRINT("Sent %d.%d", inst->wqitem->pipeline->id, inst->wqitem->id);
      
        /* Give the instance to the poll thread */
        sentqueue->produce(inst);
        
        nsent += 1;
        nready -= 1;
      }
      
      stats.batches += 1;
      stats.sent_operations += nsent;
      if(c_wait == ETIMEDOUT) {
        stats.timeout_races += 1;
      }

      pollgroup->wakeup();
 
      timeout_msec = timeout_min;  /* we are now "busy" */
    }
  }  
}


int S::Connection::get_operations_from_queue(NdbInstance **readylist,
                                             Queue<NdbInstance> *q) {
  int n = 0;
  NdbInstance *inst;  
  while((inst = q->consume()) != NULL) {
    assert(inst->db);
    inst->next = *readylist;
    *readylist = inst;
    n++;
  }
  return n;
}


void * S::Connection::run_ndb_poll_thread() {
  /* Set thread identity */
  thread_identifier tid;
  tid.pipeline = 0;
  snprintf(tid.name, THD_ID_NAME_LEN,
           "cl%d.conn%d.poll", cluster.cluster_id, id);
  set_thread_id(&tid);
  
  DEBUG_ENTER();

  NdbInstance *inst;
  Ndb ** ready_list;
  int wait_timeout_millisec = 5000;
  int min_ready;
  int in_flight = 0;
  
  while(1) {
    if(in_flight == 0 && sentqueue->is_aborted()) {
      return 0;
    }

    int n_added = 0;
    /* Add new NDBs to the poll group */
    while((inst = sentqueue->consume()) != 0) {
      assert(inst->db);
      inst->next = 0;
      DEBUG_PRINT(" ** adding %d.%d to wait group ** ",
                  inst->wqitem->pipeline->id, inst->wqitem->id);
      pollgroup->addNdb(inst->db);
      n_added++;
      in_flight++;
    }

    /* What's the minimum number of ready Ndb's to wake up for? */
    int n = n_added / 4;
    min_ready = n > 0 ? n : 1;
        
    /* Wait until something is ready to poll */
    int nwaiting = pollgroup->wait(ready_list, wait_timeout_millisec, min_ready);

    /* Poll the ones that are ready */
    if(nwaiting > 0) {
      for(int i = 0; i < nwaiting ; i++) {
        in_flight--;
        assert(in_flight >= 0);
        Ndb *db = ready_list[i];
        inst = (NdbInstance *) db->getCustomData();
        DEBUG_PRINT("Polling %d.%d", inst->wqitem->pipeline->id, inst->wqitem->id);
        db->pollNdb(0, 1);

        if(inst->wqitem->base.reschedule) {
          DEBUG_PRINT("Rescheduling %d.%d", inst->wqitem->pipeline->id, inst->wqitem->id);
          inst->wqitem->base.reschedule = 0;        
          reschedulequeue->produce(inst);  // Put it on the reschedule queue
          if(pthread_mutex_trylock( & sem.lock) == 0) {
            sem.counter++;
            pthread_cond_signal(& sem.not_zero);  // Ping the send thread
            pthread_mutex_unlock(& sem.lock); 
          }
        }
        else {
          // Scheduler yielded. Notify memcached that the operation is complete.
          DEBUG_PRINT("item_io_complete for %d.%d",
                      inst->wqitem->pipeline->id, inst->wqitem->id);
          item_io_complete(inst->wqitem);
        }
      }
    }
  }
  return 0; /* not reached */
  return 0; /* not reached */
}


void * run_send_thread(void *v) {
  S::Connection *c = (S::Connection *) v;
  return c->run_ndb_send_thread();
}

void * run_poll_thread(void *v) {
  S::Connection *c = (S::Connection *) v;
  return c->run_ndb_poll_thread();
}


