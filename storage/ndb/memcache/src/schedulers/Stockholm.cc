/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
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
/* System headers */
/* C++ files must define __STDC_FORMAT_MACROS in order to get PRIu64 */
#define __STDC_FORMAT_MACROS 
#include <inttypes.h>
#include <stdio.h>

/* Memcache headers */
#include "memcached/types.h"
#include <memcached/extension_loggers.h>

/* NDB Memcache headers */
#include "Stockholm.h"
#include "workitem.h"
#include "ndb_worker.h"

extern EXTENSION_LOGGER_DESCRIPTOR *logger;

class commit_thread_spec {
public:
  commit_thread_spec(Scheduler_stockholm *s, int i): sched(s), cluster_id(i) {};
  Scheduler_stockholm *sched;
  int cluster_id;
};

extern "C" {
  void * run_stockholm_commit_thread(void *);
}


void Scheduler_stockholm::init(int my_thread, 
                               const scheduler_options *options) {
  const Configuration & conf = get_Configuration();

  /* How many NDB instances are needed per cluster? */
  for(unsigned int c = 0 ; c < conf.nclusters ; c++) {
    ClusterConnectionPool *pool = conf.getConnectionPoolById(c);
    double total_ndb_objects = conf.figureInFlightTransactions(c);
    cluster[c].nInst = (int) total_ndb_objects / options->nthreads;
    DEBUG_PRINT("cluster %d: %d TPS @ %d usec RTT ==> %d NDB instances.",
                c, conf.max_tps, pool->usec_rtt, cluster[c].nInst);
  }
  
  // Get the ConnQueryPlanSet and NDB instances for each cluster.
  for(unsigned int c = 0 ; c < conf.nclusters ; c++) {
    cluster[c].instances = (NdbInstance**) 
      calloc(cluster[c].nInst, sizeof(NdbInstance *));
    
    ClusterConnectionPool *pool = conf.getConnectionPoolById(c);
    Ndb_cluster_connection *conn = pool->getPooledConnection(my_thread);

    cluster[c].plan_set = new ConnQueryPlanSet(conn, conf.nprefixes);
    cluster[c].plan_set->buildSetForConfiguration(&conf, c);

    cluster[c].nextFree = NULL;    
    for(int i = 0; i < cluster[c].nInst ; i++) {
      NdbInstance *inst = new NdbInstance(conn, 1);
      cluster[c].instances[i] = inst;
      inst->next = cluster[c].nextFree;
      cluster[c].nextFree = inst;
    }

    logger->log(LOG_WARNING, 0, "Pipeline %d using %u Ndb instances for Cluster %u.\n",
                my_thread, cluster[c].nInst, c);
  }


  /* Hoard a transaction (an API connect record) for each Ndb object.  This
     first call to startTransaction() will send TC_SEIZEREQ and wait for a 
     reply, but later at runtime startTransaction() should return immediately.
     TODO? Start one tx on each data node.
  */
  QueryPlan *plan;
//  const KeyPrefix *default_prefix = conf.getDefaultPrefix();  // TODO: something
  for(unsigned int c = 0 ; c < conf.nclusters ; c++) {
    const KeyPrefix *prefix = conf.getNextPrefixForCluster(c, NULL); 
    if(prefix) {
      NdbTransaction ** txlist;
      txlist = ( NdbTransaction **) calloc(cluster[c].nInst, sizeof(NdbTransaction *));
      // Open them all.
      for(int i = 0 ; i < cluster[c].nInst ; i++) {
        plan = cluster[c].plan_set->getPlanForPrefix(prefix);
        txlist[i] = cluster[c].instances[i]->db->startTransaction();
      }
      // Close them all.
      for(int i = 0 ; i < cluster[c].nInst ; i++) {
        txlist[i]->close();
      }    
      // Free the list.
      free(txlist);
    }
  }  

  /* Allocate and initialize a workqueue for each cluster.
     The engine thread will add items to this queue, and the commit thread will 
     consume them. 
  */
  for(unsigned int c = 0 ; c < conf.nclusters; c++) {
    cluster[c].queue = (struct workqueue *) malloc(sizeof(struct workqueue));
    workqueue_init(cluster[c].queue, 8192, 1);
  }  
}


void Scheduler_stockholm::attach_thread(thread_identifier * parent) {
  pipeline = parent->pipeline;
  const Configuration & conf = get_Configuration();

  logger->log(LOG_WARNING, 0, "Pipeline %d attached to Stockholm scheduler; "
              "launching %d commit thread%s.\n", pipeline->id, conf.nclusters,
              conf.nclusters == 1 ? "" : "s");

  for(unsigned int c = 0 ; c < conf.nclusters; c++) {
    cluster[c].stats.cycles = 0;
    cluster[c].stats.commit_thread_vtime = 0;

    // Launch the commit thread
    commit_thread_spec * spec = new commit_thread_spec(this, c);
    pthread_create(& cluster[c].commit_thread_id, NULL, 
                   run_stockholm_commit_thread, (void *) spec);
  }
}                                     


void Scheduler_stockholm::shutdown() {
  const Configuration & conf = get_Configuration();

  /* Shut down the workqueues */
  for(unsigned int c = 0 ; c < conf.nclusters; c++)
    workqueue_abort(cluster[c].queue);
  
  /* Close all of the Ndbs */
  for(unsigned int c = 0 ; c < conf.nclusters; c++) {
    for(int i = 0 ; i < cluster[c].nInst ; i++) {
      delete cluster[c].instances[i];
    }
  }  
}


ENGINE_ERROR_CODE Scheduler_stockholm::schedule(workitem *newitem) {
  NdbInstance *inst;
  int c;
  const Configuration & conf = get_Configuration();
  
  /* Fetch the config for its key prefix */
  const KeyPrefix *pfx = conf.getPrefixByInfo(newitem->prefix_info);

  if(newitem->prefix_info.prefix_id) {
    DEBUG_PRINT("prefix %d: \"%s\" Table: %s  Value Cols: %d", 
                newitem->prefix_info.prefix_id, pfx->prefix, 
                pfx->table->table_name, pfx->table->nvaluecols);
  }
      
  /* From here on we will work mainly with the suffix part of the key. */
  newitem->base.nsuffix = newitem->base.nkey - pfx->prefix_len;  
  if(newitem->base.nsuffix == 0) return ENGINE_EINVAL; // key too short
  
  c = newitem->prefix_info.cluster_id;
  
  if (cluster[c].nextFree)
  {
    inst = cluster[c].nextFree;
    cluster[c].nextFree = inst->next;
  }
  else
  {
    return ENGINE_TMPFAIL;
  }
  
  inst->link_workitem(newitem);
  
  // Fetch the query plan for this prefix.
  newitem->plan = cluster[c].plan_set->getPlanForPrefix(pfx);
  if(! newitem->plan) return ENGINE_FAILED;
  
  // Build the NDB transaction
  op_status_t op_status = worker_prepare_operation(newitem);
  ENGINE_ERROR_CODE response_code;
  
  switch(op_status) {
    case op_async_prepared:
    case op_async_sent:
      workqueue_add(cluster[c].queue, newitem); // place item on queue
      response_code = ENGINE_EWOULDBLOCK;
      break;
    case op_not_supported:
      response_code = ENGINE_ENOTSUP;
      break;
    case op_failed:
      response_code = ENGINE_FAILED;
      break;
    case op_overflow:
      response_code = ENGINE_E2BIG;  // ENGINE_FAILED ?
      break;
  }

  return response_code;
}


void Scheduler_stockholm::release(workitem *item) {
  DEBUG_ENTER();
  NdbInstance* inst = item->ndb_instance;
  
  if(inst) {    
    inst->unlink_workitem(item);
    int c = item->prefix_info.cluster_id;
    inst->next = cluster[c].nextFree;
    cluster[c].nextFree = inst;
  }
}


void Scheduler_stockholm::add_stats(const char *stat_key, 
                                    ADD_STAT add_stat, 
                                    const void * cookie) {
  char key[128];
  char val[128];
  int klen, vlen;
  const Configuration & conf = get_Configuration();

  if(strncasecmp(stat_key, "reconf", 6) == 0) {
    add_stat("Reconf", 6, "unsupported", 11, cookie);
    return;
  }
  
  for(unsigned int c = 0 ; c < conf.nclusters; c++) {
    klen = sprintf(key, "pipeline_%d_cluster_%d_commit_cycles", pipeline->id, c);
    vlen = sprintf(val, "%"PRIu64, cluster[c].stats.cycles);
    add_stat(key, klen, val, vlen, cookie);
    
    klen = sprintf(key, "pipeline_%d_cluster_%d_commit_thread_time", pipeline->id, c);
    vlen = sprintf(val, "%"PRIu64, cluster[c].stats.commit_thread_vtime);
    add_stat(key, klen, val, vlen, cookie);  
  }
}


#define STAT_INTERVAL 50

void * run_stockholm_commit_thread(void *s) {
  commit_thread_spec *spec = (commit_thread_spec *) s;
  spec->sched->run_ndb_commit_thread(spec->cluster_id);
  delete spec;
  return 0;
}

/* 
  Stockholm version of the commit_thread.  
  Get an item off the workqueue, and call pollNdb() on that item.
 */
void * Scheduler_stockholm::run_ndb_commit_thread(int c) {
  workitem *item;
  int polled;
  
  DEBUG_ENTER();
  
  while(1) {
    /* Wait for something to appear on the queue */
    item = (workitem *) workqueue_consumer_wait(cluster[c].queue);

    if(item == NULL) break;  /* queue has been shut down and emptied */
    
    /* Send & poll for response; reschedule if needed */
    do {
      item->base.reschedule = 0;
      polled = item->ndb_instance->db->sendPollNdb(10, 1, 1);
    } while(item->base.reschedule || ! polled);

    DEBUG_ASSERT(polled == 1);  // i.e. not > 1
    
    /* Now that sendPollNdb() has returned, it is OK to notify_io_complete(),
       which will trigger the worker thread to release the Ndb instance. */ 
    pipeline->engine->server.cookie->notify_io_complete(item->cookie, ENGINE_SUCCESS);
    
    if(! (cluster[c].stats.cycles++ % STAT_INTERVAL)) 
      cluster[c].stats.commit_thread_vtime = get_thread_vtime();
  } 

  return NULL;
}


