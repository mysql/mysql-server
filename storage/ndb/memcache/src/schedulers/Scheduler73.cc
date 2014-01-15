/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
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

#include "Scheduler73.h"


extern EXTENSION_LOGGER_DESCRIPTOR *logger;

/* Lock that protects online reconfiguration */
static pthread_rwlock_t reconf_lock = PTHREAD_RWLOCK_INITIALIZER;

/* Scheduler Global singleton */
static Scheduler73::Global * s_global;

/* Global scheduler generation number */
static int sched_generation_number;

/* SchedulerGlobal methods */
Scheduler73::Global::Global(Configuration *cf) : 
  conf(cf) 
{ 
  generation = sched_generation_number;
}


void Scheduler73::Global::init(const scheduler_options *sched_opts) {
  DEBUG_ENTER_METHOD("Scheduler73::Global::init");

  /* Set member variables */
  nthreads = sched_opts->nthreads;
  options.max_clients = sched_opts->max_clients;
  parse_config_string(sched_opts->config_string);

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
  }

  /* Initialize the list that will hold WorkerConnections */
  workerConnections = (WorkerConnection **) calloc(sizeof(void *), nthreads * nclusters);  
    
  /* Initialize the WorkerConnections */
  for(int t = 0 ; t < nthreads ; t++) {
    for(int c = 0 ; c < nclusters ; c++) {
      WorkerConnection **wc_cell = getWorkerConnectionPtr(t, c);
      * wc_cell = new WorkerConnection(this, clusters[c], t);
    }
  }
    
  /* Start the wait thread for each connection */
  for(int i = 0 ; i < nclusters ; i++) 
    clusters[i]->startThreads();
  
  /* Log message for startup */
  logger->log(LOG_WARNING, 0, "Scheduler: starting ...");

  /* Now Running */
  running = true;
}

void Scheduler73::Global::parse_config_string(const char *str) {

  /* Initialize the configuration default values */
  options.separate_send = true;
  if(str) {
    const char *s = str;
    char letter;
    int value;
    
    /* tolerate a ':' at the start of the string */
    if( *s == ':') s++;
    
    while(*s != '\0' && sscanf(s, "%c%d", &letter, &value) == 2) {
      switch(letter) {
        case 's':
          options.separate_send = value;
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
}



void Scheduler73::Global::reconfigure(Configuration * new_cf) {
  conf = new_cf;
  generation++;
  
  for(int i = 0; i < nclusters ; i++) {
    for(int j = 0; j < nthreads ; j++) {
      WorkerConnection *wc = * (getWorkerConnectionPtr(j, i));
      wc->reconfigure(new_cf);
    }
  }
}


void Scheduler73::Global::shutdown() {
  if(running) {
    logger->log(LOG_INFO, 0, "Shutting down scheduler.");

    /* Release each WorkerConnection */
    for(int i = 0; i < nclusters ; i++) {
      for(int j = 0; j < nthreads ; j++) {
        WorkerConnection *wc = * (getWorkerConnectionPtr(j, i));
        delete wc;
      }
    }

    /* Release each Cluster */
    for(int i = 0; i < nclusters ; i++) {
      delete clusters[i];
      conf->getConnectionPoolById(i)->setCustomData(0);
    }
    
    /* Then free the list of WorkerConnections */
    free(workerConnections);

    /* Shutdown now */
    logger->log(LOG_WARNING, 0, "Shutdown completed.");
    running = false;
  }
}


void Scheduler73::Global::add_stats(const char *stat_key,
                                    ADD_STAT add_stat, 
                                    const void *cookie) {
  if(strncasecmp(stat_key, "reconf", 6) == 0) {
    char gen_number_buffer[16];
    const char *status;
    char *gen = gen_number_buffer;
        
    if(pthread_rwlock_tryrdlock(& reconf_lock) == 0) {
      status = "Running";
      snprintf(gen, 16, "%d", generation);
      pthread_rwlock_unlock(& reconf_lock);
    }
    else {
      status = "Loading";
      snprintf(gen, 16, "%d", generation + 1);
    }
    add_stat(status, strlen(status), gen, strlen(gen), cookie);
    DEBUG_PRINT(" reconf; %s %s", status, gen);
  }
  else {
    DEBUG_PRINT(" scheduler");
    for(int c = 0 ; c < nclusters ; c++) {
      clusters[c]->add_stats(stat_key, add_stat, cookie);
    }
  }
}


/* SchedulerWorker methods */

void Scheduler73::Worker::init(int my_thread, 
                              const scheduler_options * options) {
  /* On the first call in, initialize the SchedulerGlobal.
   */
  if(my_thread == 0) {
    sched_generation_number = 1;
    s_global = new Global(& get_Configuration());
    s_global->init(options);
  }
  
  /* Initialize member variables */
  id = my_thread;
}


void Scheduler73::Worker::shutdown() {
  s_global->shutdown();
}


void Scheduler73::Worker::attach_thread(thread_identifier *parent) {
  DEBUG_ENTER();
  
  pipeline = parent->pipeline;
  
  if(id == 0) {
    s_global->engine = pipeline->engine;
  }
  
  logger->log(LOG_WARNING, 0, "Pipeline %d attached to 7.3 scheduler.\n", id);
}


ENGINE_ERROR_CODE Scheduler73::Worker::schedule(workitem *item) {
  int c = item->prefix_info.cluster_id;
  WorkerConnection *wc;
  const KeyPrefix *pfx;
  
  DEBUG_PRINT("SchedulerWorker / config gen. %d", s_global->generation);

  /* ACQUIRE READ LOCK */
  if(pthread_rwlock_rdlock(& reconf_lock) == 0) {
    wc = * (s_global->getWorkerConnectionPtr(id, c));
    pfx = s_global->conf->getPrefixByInfo(item->prefix_info);
    pthread_rwlock_unlock(& reconf_lock);
  }
  else {
    log_app_error(& AppError29001_ReconfLock);
    return ENGINE_TMPFAIL;
  }
  /* READ LOCK RELEASED */
  
  item->base.nsuffix = item->base.nkey - pfx->prefix_len;
 
  if(wc == 0) return ENGINE_FAILED;

  return wc->schedule(pfx, item);
}


void Scheduler73::Worker::prepare(NdbTransaction * tx, 
                                  NdbTransaction::ExecType execType, 
                                  NdbAsynchCallback callback, 
                                  workitem * item, prepare_flags flags) { 
  Ndb *ndb = tx->getNdb();

  Uint64 nwaitsPre = ndb->getClientStat(Ndb::WaitExecCompleteCount);

  if(s_global->options.separate_send)
    tx->executeAsynchPrepare(execType, callback, (void *) item);
  else
    tx->executeAsynch(execType, callback, (void *) item);
    
  Uint64 nwaitsPost = ndb->getClientStat(Ndb::WaitExecCompleteCount);
  assert(nwaitsPost == nwaitsPre);

  if(flags == RESCHEDULE) item->base.reschedule = 1;
}


/* Release the resources used by an operation.  
   Unlink the NdbInstance from the workitem, and return it to the free list 
   (or free it, if the scheduler is shutting down).
*/
void Scheduler73::Worker::release(workitem *item) {
  DEBUG_ENTER();
  NdbInstance *inst = item->ndb_instance;
  
  if(inst) {
    inst->unlink_workitem(item);
    int c = item->prefix_info.cluster_id;
    WorkerConnection * wc = * (s_global->getWorkerConnectionPtr(id, c));
    if(wc) {
      wc->release(inst);
    }
    else {
      /* We are in the midst of shutting down (and possibly reconfiguring) */
      delete inst;
    }
  }
}


/* This is a partial implementation of online reconfiguration.
   It can replace KeyPrefix mappings, but not add a cluster at runtime 
   (nor will it catch an attempt to do so -- which will eventually lead to
   a crash after a getWorkerConnectionPtr()).    
*/
bool Scheduler73::Worker::global_reconfigure(Configuration *new_cf) {
  bool r = false;
  
  if(pthread_rwlock_wrlock(& reconf_lock) == 0) {
    s_global->reconfigure(new_cf);
    pthread_rwlock_unlock(& reconf_lock);
    r = true;
  }
  return r;
}


void Scheduler73::Worker::add_stats(const char *stat_key,
                                    ADD_STAT add_stat, 
                                    const void *cookie) {
  s_global->add_stats(stat_key, add_stat, cookie);
}


/* WorkerConnection methods */


Scheduler73::WorkerConnection::WorkerConnection(Global *global,
                                                Cluster * _cl, 
                                                int _worker_id) :
  cluster(_cl),
  worker_id(_worker_id)
{
  Configuration *conf = global->conf;

  /* Build the plan_set and all QueryPlans */
  old_plan_set = 0;
  plan_set = new ConnQueryPlanSet(cluster->ndb_conn, conf->nprefixes);
  plan_set->buildSetForConfiguration(conf, cluster->id);

  /* How many NDB instances to start initially */
  instances.initial = cluster->instances.initial / global->nthreads;

  /* Upper bound on NDB instances */
  instances.max = global->options.max_clients / global->nthreads;

  /* Build the freelist */
  freelist = 0;
  for(instances.current = 0; instances.current < instances.initial; ) {
    NdbInstance *inst = newNdbInstance();
    inst->next = freelist;
    freelist = inst;
  }

  DEBUG_PRINT("Cluster %d / worker %d: %d NDBs.", 
              cluster->id, worker_id, instances.current);
  
  /* Hoard a transaction (an API connect record) for each Ndb object.  This
   * first call to startTransaction() will send TC_SEIZEREQ and wait for a 
   * reply, but later at runtime startTransaction() should return immediately.
   * Also, pre-build a QueryPlan for each NDB instance.
   */
  QueryPlan *plan;
  const KeyPrefix *prefix = conf->getNextPrefixForCluster(cluster->id, NULL);
  if(prefix) {
    NdbTransaction ** txlist = new NdbTransaction * [instances.current];
    int i = 0;

    // Open them all.
    for(NdbInstance *inst = freelist; inst != 0 ;inst=inst->next, i++) {
      NdbTransaction *tx;
      plan = plan_set->getPlanForPrefix(prefix);
      tx = inst->db->startTransaction();
      if(! tx) logger->log(LOG_WARNING, 0, inst->db->getNdbError().message);
      txlist[i] = tx;
    }
    
    // Close them all.
    for(i = 0 ; i < instances.current ; i++) {
      txlist[i]->close();
    }    
    
    // Free the list.
    delete[] txlist;
  }
}


inline NdbInstance * Scheduler73::WorkerConnection::newNdbInstance() {
  NdbInstance * inst = new NdbInstance(cluster->ndb_conn, 2);
  if(inst) {
    instances.current++;
    inst->id = ((worker_id + 1) * 10000) + instances.current;
  }
  return inst;
}


void Scheduler73::WorkerConnection::reconfigure(Configuration *new_cf) {
  if(old_plan_set) {  /* Garbage collect the old old plans */
    delete old_plan_set;
  }
  old_plan_set = plan_set;
  
  ConnQueryPlanSet *new_plans = 
    new ConnQueryPlanSet(cluster->ndb_conn, new_cf->nprefixes);
  new_plans->buildSetForConfiguration(new_cf, cluster->id);
  
  plan_set = new_plans;
}


ENGINE_ERROR_CODE Scheduler73::WorkerConnection::schedule(const KeyPrefix *pfx,
                                                          workitem *item) {
  ENGINE_ERROR_CODE response_code;
  NdbInstance *inst = 0;

  if(freelist) {                 /* Get the next NDB from the freelist. */
    inst = freelist;
    freelist = inst->next;
  }
  else if(instances.current < instances.max) {
    inst = newNdbInstance();
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
  
  assert(inst);
  inst->link_workitem(item);
  
  // Fetch the query plan for this prefix.
  item->plan = plan_set->getPlanForPrefix(pfx);
  if(! item->plan) {
    DEBUG_PRINT("getPlanForPrefix() failure");
    return ENGINE_FAILED;
  }
  
  // Build the NDB transaction
  op_status_t op_status = worker_prepare_operation(item);

  if(op_status == op_prepared) {
    /* Success */
   if(s_global->options.separate_send) 
     inst->db->sendPreparedTransactions(false);
    cluster->pollgroup->push(inst->db);
    cluster->pollgroup->wakeup();
    response_code = ENGINE_EWOULDBLOCK;
  }
  else {  
    switch(op_status) {
     case op_not_supported:
        DEBUG_PRINT("op_status is op_not_supported");
        response_code = ENGINE_ENOTSUP;
        break;
      case op_bad_key:
        DEBUG_PRINT("op_status is op_bad_key");
        response_code = ENGINE_EINVAL;
        break;
      case op_overflow:
        DEBUG_PRINT("op_status is op_overflow");
        response_code = ENGINE_E2BIG;
        break;
      case op_failed:
        DEBUG_PRINT("op_status is op_failed");
        response_code = ENGINE_FAILED;
        break;
      default:
        DEBUG_PRINT("UNEXPECTED: op_status is %d", op_status);
        response_code = ENGINE_FAILED;
    }
  }
  
  return response_code;
}


inline void Scheduler73::WorkerConnection::release(NdbInstance *inst) {
  inst->next = freelist;
  freelist = inst;
}


Scheduler73::WorkerConnection::~WorkerConnection() {
  DEBUG_ENTER_METHOD("WorkerConnection::~WorkerConnection");

  /* Delete all of the Ndbs that are not currently in use */
  NdbInstance *inst = freelist;
  while(inst != 0) {
    NdbInstance *next = inst->next;
    delete inst;
    inst = next;
  }
    
  /* Delete the current QueryPlans (and maybe the previous ones, too) */
  delete plan_set;
  if(old_plan_set) {
    delete old_plan_set;
  }
}


/* Cluster methods */
Scheduler73::Cluster::Cluster(Global *global, int _id) : 
  running(false),
  id(_id)
{
  DEBUG_PRINT("%d", id);
  Configuration *conf = global->conf;
  ndb_conn = conf->getConnectionPoolById(id)->getMainConnection();
  node_id = ndb_conn->node_id();

  /* Set the timer on the adaptive send thread */
  ndb_conn->set_max_adaptive_send_time(1);

  /* How many NDB objects are needed for the desired performance? */
  instances.initial = (int) conf->figureInFlightTransactions(id);
  while(instances.initial % global->nthreads) instances.initial++; // round up

  /* Get a multi-wait Poll Group */
  pollgroup = ndb_conn->create_ndb_wait_group(instances.initial);
}


/* Threads are started only once and persist across reconfiguration.
   But, this method will be called again for each reconf. */
void Scheduler73::Cluster::startThreads() {
  if(running == false) {
    running = true;
    pthread_create( & wait_thread_id, NULL, run_ndb_wait_thread, (void *) this);
  }
}


void Scheduler73::Cluster::add_stats(const char *stat_key,
                                     ADD_STAT add_stat, 
                                     const void *cookie) {
  return;
}
  

Scheduler73::WorkerConnection ** Scheduler73::Cluster::getWorkerConnectionPtr(int thd) const {
  return s_global->getWorkerConnectionPtr(thd, id);
}


void * Scheduler73::Cluster::run_wait_thread() {
  /* Set thread identity */
  thread_identifier tid;
  tid.pipeline = 0;
  snprintf(tid.name, THD_ID_NAME_LEN, "cl%d.wait", id);
  set_thread_id(&tid);
  
  DEBUG_ENTER();

  NdbInstance *inst;
  int wait_timeout_millisec = 5000;
  
  while(running) {        
    /* Wait until something is ready to poll */
    int nwaiting = pollgroup->wait(wait_timeout_millisec, 25);

    /* Poll the ones that are ready */
    while(nwaiting-- > 0) {
      Ndb *db = pollgroup->pop();
      inst = (NdbInstance *) db->getCustomData();
      DEBUG_PRINT("Polling %d.%d", inst->wqitem->pipeline->id, inst->wqitem->id);
      db->pollNdb(0, 1);

      if(inst->wqitem->base.reschedule) {
        DEBUG_PRINT("Rescheduling %d.%d", inst->wqitem->pipeline->id, inst->wqitem->id);
        inst->wqitem->base.reschedule = 0;
        if(s_global->options.separate_send) 
          db->sendPreparedTransactions(false);
        pollgroup->push(db);
      }
      else {     // Operation is complete
        const SERVER_COOKIE_API *api = s_global->engine->server.cookie;
        api->notify_io_complete(inst->wqitem->cookie, ENGINE_SUCCESS);
      }
    }
  }
  return 0;
}


Scheduler73::Cluster::~Cluster() {
  DEBUG_PRINT("Shutting down cluster %d", id);
  running = false;
  pollgroup->wakeup();
  pthread_join(wait_thread_id, NULL); 
  ndb_conn->release_ndb_wait_group(pollgroup);
}


void * run_ndb_wait_thread(void *v) {
  Scheduler73::Cluster *c = (Scheduler73::Cluster *) v;
  return c->run_wait_thread();
}

