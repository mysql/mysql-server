/*
 Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights
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

/* Scheduler Global singleton */
static Scheduler73::Global * s_global;

/* SchedulerGlobal methods */
Scheduler73::Global::Global(int _nthreads) :
  GlobalConfigManager(_nthreads)
{
}


void Scheduler73::Global::init(const scheduler_options *sched_opts) {
  DEBUG_ENTER_METHOD("Scheduler73::Global::init");

  /* Set member variables */
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

  /* Initialize the WorkerConnections */
  for(int t = 0 ; t < nthreads ; t++) {
    for(int c = 0 ; c < nclusters ; c++) {
      WorkerConnection **wc_cell = getWorkerConnectionPtr(t, c);
      * wc_cell = new WorkerConnection(this, clusters[c], t, nthreads);
    }
  }

  configureSchedulers();

  /* Start the wait thread for each connection */
  for(int i = 0 ; i < nclusters ; i++) 
    clusters[i]->startThreads();
  
  /* Log message for startup */
  logger->log(LOG_WARNING, 0, "Scheduler 73: starting ...");

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



void Scheduler73::Global::shutdown() {
  if(running) {
    logger->log(LOG_INFO, 0, "Shutting down scheduler 73.");

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
    
    /* Shutdown now */
    logger->log(LOG_WARNING, 0, "Scheduler 73 shutdown completed.");
    running = false;
  }
}


void Scheduler73::Global::add_stats(const char *stat_key,
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

void Scheduler73::Worker::init(int my_thread, 
                              const scheduler_options * options) {
  /* On the first call in, initialize the SchedulerGlobal.
   */
  if(my_thread == 0) {
    s_global = new Global(options->nthreads);
    s_global->init(options);
  }
  
  /* Initialize member variables */
  id = my_thread;
}


void Scheduler73::Worker::shutdown() {
  if(id == 0)
    s_global->shutdown();
}


Scheduler73::Worker::~Worker() {
  if(id == 0)
    delete s_global;
}


ENGINE_ERROR_CODE Scheduler73::Worker::schedule(workitem *item) {
  int c = item->prefix_info.cluster_id;
  WorkerConnection *wc = * (s_global->getWorkerConnectionPtr(id, c));
  return wc->schedule(item);
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


void Scheduler73::Worker::close(NdbTransaction *tx, workitem *item) {
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
void Scheduler73::Worker::release(workitem *item) {
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


bool Scheduler73::Worker::global_reconfigure(Configuration *new_cf) {
  return s_global->reconfigure(new_cf);
}


void Scheduler73::Worker::add_stats(const char *stat_key,
                                    ADD_STAT add_stat, 
                                    const void *cookie) {
  s_global->add_stats(stat_key, add_stat, cookie);
}


/* WorkerConnection methods */


Scheduler73::WorkerConnection::WorkerConnection(Global *global,
                                                Cluster * _cl, 
                                                int _worker_id,
                                                int nthreads) :
  SchedulerConfigManager(_worker_id, _cl->id),
  cluster(_cl)
{
  /* How many NDB instances to start initially */
  instances.initial = cluster->instances.initial / nthreads;

  /* Upper bound on NDB instances */
  instances.max = global->options.max_clients / nthreads;

  /* Build the freelist */
  freelist = 0;
  for(instances.current = 0; instances.current < instances.initial; ) {
    NdbInstance *inst = newNdbInstance();
    inst->next = freelist;
    freelist = inst;
  }

  DEBUG_PRINT("Cluster %d / worker %d: %d NDBs.", 
              cluster->id, thread, instances.current);
  
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


inline NdbInstance * Scheduler73::WorkerConnection::newNdbInstance() {
  NdbInstance * inst = new NdbInstance(cluster->ndb_conn, 2);
  if(inst) {
    instances.current++;
    inst->id = ((worker_id + 1) * 10000) + instances.current;
  }
  return inst;
}


ENGINE_ERROR_CODE Scheduler73::WorkerConnection::schedule(workitem *item) {
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
  setQueryPlanInWorkitem(item);
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
    /* Status is not op_prepared, but rather some error status */
    response_code = item->status->status;
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
      DEBUG_PRINT_DETAIL("Polling %d.%d", inst->wqitem->pipeline->id, inst->wqitem->id);
      db->pollNdb(0, 1);

      if(inst->wqitem->base.reschedule) {
        DEBUG_PRINT_DETAIL("Rescheduling %d.%d", inst->wqitem->pipeline->id, inst->wqitem->id);
        inst->wqitem->base.reschedule = 0;
        if(s_global->options.separate_send) 
          db->sendPreparedTransactions(false);
        pollgroup->push(db);
      }
      else {     // Operation is complete
        item_io_complete(inst->wqitem);
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

