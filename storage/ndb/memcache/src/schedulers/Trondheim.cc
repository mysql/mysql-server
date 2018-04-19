/*
 Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */


#include <stdio.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/errno.h>
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

#include "Trondheim.h"


extern EXTENSION_LOGGER_DESCRIPTOR *logger;

/* Scheduler Global singleton */
static Trondheim::Global * s_global;


/* SchedulerGlobal methods */

Trondheim::Global::Global(const scheduler_options *sched_opts) :
  GlobalConfigManager(sched_opts->nthreads)
{
  DEBUG_ENTER();

  /* Initialize the WorkerConnections */
  for(int t = 0 ; t < nthreads ; t++) {
    for(int c = 0 ; c < nclusters ; c++) {
      WorkerConnection **wc_cell = getWorkerConnectionPtr(t, c);
      * wc_cell = new WorkerConnection(c, t);
    }
  }

  /* Give the WorkerConnections their configurations */
  configureSchedulers();

  /* Log message for startup */
  logger->log(LOG_WARNING, 0, "Initializing Trondheim scheduler.\n");

  /* Start the WorkerConnections */
  for(int t = 0 ; t < nthreads ; t++) {
    for(int c = 0 ; c < nclusters ; c++) {
      WorkerConnection **wc_cell = getWorkerConnectionPtr(t, c);
      (* wc_cell)->start();
    }
  }
}


void Trondheim::Global::shutdown() {
  /* Shutdown the WorkerConnections */
  for(int t = 0 ; t < nthreads ; t++) {
    for(int c = 0 ; c < nclusters ; c++) {
      WorkerConnection **wc_cell = getWorkerConnectionPtr(t, c);
      (* wc_cell)->shutdown();
    }
  }
}


Trondheim::Global::~Global() {
  /* Release each WorkerConnection */
  for(int i = 0; i < nclusters ; i++) {
    for(int j = 0; j < nthreads ; j++) {
      WorkerConnection *wc = * (getWorkerConnectionPtr(j, i));
      delete wc;
    }
  }

  logger->log(LOG_WARNING, 0, "Shutdown completed.");
}


/* SchedulerWorker methods */

void Trondheim::Worker::init(int my_thread,
                             const scheduler_options * options) {
  /* On the first call in, initialize the SchedulerGlobal.
   */
  if(my_thread == 0) {
    s_global = new Global(options);
  }
  
  /* Initialize member variables */
  id = my_thread;
  global = s_global;
}


void Trondheim::Worker::shutdown() {
  if(id == 0)
    s_global->shutdown();
}


Trondheim::Worker::~Worker() {
  if(id == 0)
    delete s_global;
}


ENGINE_ERROR_CODE Trondheim::Worker::schedule(workitem *item) {
  /* Get the appropriate WorkerConnection */
  WorkerConnection *wc = getConnection(item->prefix_info.cluster_id);
  if(wc == 0) return ENGINE_FAILED;

  /* Let the WorkerConnection schedule the item */
  return wc->schedule(item);
}


void Trondheim::Worker::add_stats(const char *stat_key,
                                  ADD_STAT add_stat,
                                  const void *cookie) {
  /* Let the first connection supply the stats */
  getConnection(0)->add_stats(stat_key, add_stat, cookie);
}


void Trondheim::Worker::prepare(NdbTransaction * tx,
                                NdbTransaction::ExecType execType,
                                NdbAsynchCallback callback,
                                workitem * item, prepare_flags flags) {
  tx->executeAsynchPrepare(execType, callback, (void *) item);
  if(flags == RESCHEDULE) item->base.reschedule = 1;
}


void Trondheim::Worker::release(workitem *item) {
  DEBUG_ENTER();
}


bool Trondheim::Worker::global_reconfigure(Configuration *cf) {
  return s_global->reconfigure(cf);
}


/* WorkerConnection methods */

void * run_ndb_thread(void *v) {
  Trondheim::WorkerConnection *wc = (Trondheim::WorkerConnection *) v;
  return wc->runNdbThread();
}


Trondheim::WorkerConnection::WorkerConnection(int _cluster_id,
                                              int _worker_id) :
  SchedulerConfigManager(_worker_id, _cluster_id),
  pending_ops(0),
  ndb(0),
  running(false)
{
  /* Allocate the workqueue */
  queue = (struct workqueue *) malloc(sizeof(struct workqueue));
}

Trondheim::WorkerConnection::~WorkerConnection() {
  free(queue);
}


void Trondheim::WorkerConnection::start() {
  /* Get the NDB */
  ndb = new Ndb(ndb_connection);

  /* An Ndb can handle a maximum of 1024 transactions */
  ndb->init(1024);

  /* Initialize the workqueue.  Since the Ndb is limited to 1024 transactions, 
     limit the workqueue to that same number.
  */
  workqueue_init(queue, 1024, 1);

  /* Hoard a bunch of transactions (API connect records).
     Set optimized_node_selection to zero so that these transactions are started
     round-robin.  Memcached PK operations will be started locally to the data
     using the key as a hint to startTransaction.
  */
  ndb_connection->set_optimized_node_selection(0);
  NdbTransaction * tx_array[128];
  for(int i = 0 ; i < 128 ; i++) {
    NdbTransaction * tx = ndb->startTransaction();
    tx_array[i] = tx;
  }
  for(int i = 0 ; i < 128 ; i++) {
    if(tx_array[i]) tx_array[i]->close();
  }

  /* Start the Ndb thread */
  running = true;  // set in advance so the thread doesn't shut down
  pthread_create( & ndb_thread_id, NULL, run_ndb_thread, (void *) this);
}


void Trondheim::WorkerConnection::shutdown() {
  running = false;
  workqueue_abort(queue);
  pthread_join(ndb_thread_id, NULL);
}


ENGINE_ERROR_CODE Trondheim::WorkerConnection::schedule(workitem *item) {
  setQueryPlanInWorkitem(item);
  if(! item->plan) {
    DEBUG_PRINT("setQueryPlanInWorkitem() failed");
    return ENGINE_FAILED;
  }

  workqueue_add(queue, item);
  return ENGINE_EWOULDBLOCK;
}


void Trondheim::WorkerConnection::close(NdbTransaction *tx, workitem *item) {
  tx->close();
  assert(pending_ops > 0);
  pending_ops--;
  DEBUG_PRINT("notify io complete, status: %d [%s], item %d.%d [%d pending]",
              (int) item->status->status, item->status->comment,
              thread, item->id, pending_ops);
  item_io_complete(item);
}


void * Trondheim::WorkerConnection::runNdbThread() {
  thread_identifier tid;
  snprintf(tid.name, THD_ID_NAME_LEN, "cluster%d.pipeline%d.ndb", cluster, thread);
  set_thread_id(&tid);

  DEBUG_ENTER();

  struct workitem * current_ops[1024];  // Workitems fetched from queue
  int n_current_ops;

  while(running) {
    /* This loop may sleep waiting for a newly queued item, or it may
       sleep in pollNdb(), but it will never sleep in both places.
    */

    // 1: FETCH ITEMS FROM WORKQUEUE
    /* Quickly fetch workitems that are already queued */
    n_current_ops = 0;
    while(workqueue_consumer_poll(queue)) {
      current_ops[n_current_ops++] = (workitem *) workqueue_consumer_wait(queue);
    }

    /* If none fetched, and nothing is pending on the network, sleep until one 
       arrives */
    if(n_current_ops == 0 && pending_ops == 0) {
      current_ops[n_current_ops++] = (workitem *) workqueue_consumer_wait(queue);
    }

    // 2: POLL FOR NDB RESULTS
    if(pending_ops) {
      /* Wait for half of pending operations, or maximum of 1 millisecond */
      int min_complete = (pending_ops > 3 ? pending_ops / 2 : 1);
      ndb->pollNdb(1, min_complete);      // callbacks will run
    }

    // 3: PREPARE NEW OPERATIONS
    for(int i = 0 ; i < n_current_ops ; i++) {
      workitem * item = current_ops[i];

      if(item) {
        /* Set the Ndb in the workitem */
        item->ndb_instance = new NdbInstance(ndb, item);

        /* Build & Prepare Operations */
        op_status_t op_status = worker_prepare_operation(item);
        if(op_status == op_prepared) {
          pending_ops++;                  // This will be decremented by close()
        } else {                          // Error condition
          item_io_complete(item);
        }
      } else {
        /* A null workqueue item means the queue has been shut down */
        running = false;
        break;
      }
    }

    // 4: SEND OPERATIONS
    ndb->sendPreparedTransactions();
  }

  /* After shutdown, wait up to 100 msec for in-flight operations */
  for(int n = 10; pending_ops && n; n--) {
    ndb->sendPollNdb(10, pending_ops, 1);
  }

  workqueue_destroy(queue);

  return 0;
}


