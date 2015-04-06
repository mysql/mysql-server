/*
 Copyright (c) 2013, 2015 Oracle and/or its affiliates. All rights
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

#ifndef NDBMEMCACHE_73_SCHEDULER_H
#define NDBMEMCACHE_73_SCHEDULER_H

#ifndef __cplusplus
#error "This file is for C++ only"
#endif

#include <memcached/types.h>

#include <NdbWaitGroup.hpp>

#include "ndbmemcache_config.h"
#include "workitem.h"
#include "Scheduler.h"
#include "GlobalConfigManager.h"

/* 
 *  7.3 Scheduler 
 *
 *  This is designed to take advantage of the ATC (API Thread Contention) fixes
 *  in MySQL Cluster 7.3.
 *
 *  There is no send queue; workers themselves send transactions.
 *  Like the S scheduler, it must support online reconfiguration.
 *  With thread contention fixed, it should use only one connection 
 *  to each cluster.
 *        
 */

extern "C" {
  void * run_ndb_wait_thread(void *v);
}

class Scheduler73 {
public:
  class Global;              // global singleton
  class Cluster;             // one object per each cluster
  class Worker;              // one object per memcached worker thread
  class WorkerConnection;    // one object per {worker,connection} pair
};


class Scheduler73::Global : public GlobalConfigManager {
  friend class Cluster;

public:
  Global(int);
  ~Global() {};
  void init(const scheduler_options *options);
  void add_stats(const char *, ADD_STAT, const void *);
  void shutdown();
  WorkerConnection ** getWorkerConnectionPtr(int thd, int cluster) const {
    return (WorkerConnection **) getSchedulerConfigManagerPtr(thd, cluster);
  }

  Cluster ** clusters;
  
  struct options {
    int max_clients;       /** memcached max allowed connections */
    bool separate_send;
  } options;

private:
  bool running;

private:
  void parse_config_string(const char *config_string);
};

/* For each connected cluster, there is one Cluster,
 * which manages an NdbWaitGroup and a wait thread.
 */
class Scheduler73::Cluster {
  friend void * run_ndb_wait_thread(void *);
  friend class Global;
  friend class SchedulerWorker;
  friend class WorkerConnection;

protected:
  Cluster(Scheduler73::Global *, int id);
  ~Cluster();
  void add_stats(const char *, ADD_STAT, const void *);
  WorkerConnection ** getWorkerConnectionPtr(int thd) const;  
  void * run_wait_thread(void);
  void startThreads();
   
  Ndb_cluster_connection * ndb_conn;
  bool running;
  int id;
  int node_id;
  NdbWaitGroup *pollgroup;
  struct {
    int initial;         /* start with this many NDB instances */
  } instances; 
  pthread_t wait_thread_id;  
};


/* For each {connection, worker} tuple there is a WorkerConnection 
*/
class Scheduler73::WorkerConnection : public SchedulerConfigManager {
public:
  WorkerConnection(Global *, Cluster *, int thd_id, int nthreads);
  ~WorkerConnection();
  void shutdown();
  ENGINE_ERROR_CODE schedule(workitem *);
  void release(NdbInstance *);
  
private:
  NdbInstance * newNdbInstance();
  
  struct {
    int initial;
    int current;
    int max;
  } instances;
  Scheduler73::Cluster *cluster;
  int worker_id;
  NdbInstance *freelist;
};


class Scheduler73::Worker : public Scheduler {
public:
  Worker() {};
  ~Worker();
  void init(int threadnum, const scheduler_options *options);
  void attach_thread(thread_identifier *) {};
  ENGINE_ERROR_CODE schedule(workitem *);
  void prepare(NdbTransaction *, NdbTransaction::ExecType, NdbAsynchCallback, 
               workitem *, prepare_flags);
  void close(NdbTransaction *, workitem *);
  void release(workitem *);
  void add_stats(const char *, ADD_STAT, const void *);
  void shutdown();
  bool global_reconfigure(Configuration *);

private:
  int id;
  Scheduler73::Global * global;
};


#endif

