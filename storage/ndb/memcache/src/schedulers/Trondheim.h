/*
 Copyright (c) 2015, Oracle and/or its affiliates. All rights
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

#ifndef NDBMEMCACHE_TRONDHEIM_SCHEDULER_H
#define NDBMEMCACHE_TRONDHEIM_SCHEDULER_H

#ifndef __cplusplus
#error "This file is for C++ only"
#endif

#include <memcached/types.h>

#include "ndbmemcache_config.h"
#include "workitem.h"
#include "Scheduler.h"
#include "KeyPrefix.h"
#include "ConnQueryPlanSet.h"
#include "GlobalConfigManager.h"
#include "SchedulerConfigManager.h"


/* 
 *              Trondheim Scheduler
 *
 * The Trondheim scheduler pairs one Ndb thread (per cluster) with each
 * libevent worker thread.
 * Each Ndb thread uses a single Ndb object, and runs this loop:
 *        Fetch items from workqueue
 *        Poll for results
 *        Prepare operations
 *        Notify completions
  */


extern "C" {
  void * run_ndb_thread(void *v);
}

class Trondheim {
public:
  class Global;              // global singleton
  class Worker;              // one object per memcached worker thread
  class WorkerConnection;    // one object per {worker,connection} pair
};


class Trondheim::Global : public GlobalConfigManager {
public:
  Global(const scheduler_options *);
  ~Global();
  WorkerConnection ** getWorkerConnectionPtr(int thd, int cluster) const {
    return (WorkerConnection **) getSchedulerConfigManagerPtr(thd, cluster);
  }
  void shutdown();
};


/* For each libevent worker thread in memcached, there is a Worker
*/
class Trondheim::Worker : public Scheduler {
public:
  Worker() {};
  ~Worker();
  void init(int threadnum, const scheduler_options *options);
  void attach_thread(thread_identifier *)                                   {} ;
  ENGINE_ERROR_CODE schedule(workitem *);
  void prepare(NdbTransaction *, NdbTransaction::ExecType, NdbAsynchCallback, 
               workitem *, prepare_flags);
  void close(NdbTransaction *, workitem *);
  void release(workitem *);
  void add_stats(const char *, ADD_STAT, const void *);
  void shutdown();
  bool global_reconfigure(Configuration *);

protected:
  WorkerConnection * getConnection(int cluster_id) const {
    return * (global->getWorkerConnectionPtr(id, cluster_id));
  }

private:
  int id;
  Trondheim::Global * global;
};


/* For each {connection, worker} tuple there is a WorkerConnection 
*/
class Trondheim::WorkerConnection : public SchedulerConfigManager {
  friend class Trondheim::Global;
  friend class Trondheim::Worker;
  friend void * run_ndb_thread(void *);

public:
  WorkerConnection(int, int);
  ~WorkerConnection();
  void close(NdbTransaction *, workitem *);
  void release(NdbInstance *);

protected:
  void shutdown();
  void start();
  void * runNdbThread();
  ENGINE_ERROR_CODE schedule(workitem *);

private:
  int pending_ops;
  struct workqueue * queue;
  Ndb *ndb;
  pthread_t ndb_thread_id;
  bool running;
};

inline void Trondheim::Worker::close(NdbTransaction *tx, workitem *item) {
  getConnection(item->prefix_info.cluster_id)->close(tx, item);
}


#endif
