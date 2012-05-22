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

#ifndef NDBMEMCACHE_S_SCHEDULER_H
#define NDBMEMCACHE_S_SCHEDULER_H

#ifndef __cplusplus
#error "This file is for C++ only"
#endif

#include <memcached/types.h>

#include <NdbWaitGroup.hpp>

#include "ndbmemcache_config.h"
#include "Scheduler.h"
#include "KeyPrefix.h"
#include "ConnQueryPlanSet.h"
#include "Queue.h"

/* 
 *
 * This scheduler uses many Ndb objects and runs in three threads:
 *    the memcache worker thread prepares transactions
 *    the send thread sends them 
 *    the poll thread waits for them to complete and then polls them.
 *
 *  Class S::SchedulerWorker implents the Scheduler interface
 */

class S {
public:
  class SchedulerGlobal;     // a global singleton
  class SchedulerWorker;     // one object per memcached worker thread 
  class Cluster;             // one object for each cluster
  class Connection;          // one object per connection to a cluster
  class WorkerConnection;    // one object per {worker,connection} pair
};


/* The SchedulerGlobal singleton
*/ 
class S::SchedulerGlobal {
public:
  SchedulerGlobal(Configuration *);
  ~SchedulerGlobal() {};
  void init(const scheduler_options *options);
  void add_stats(const char *, ADD_STAT, const void *);
  void reconfigure(Configuration *);
  void shutdown();
  WorkerConnection ** getWorkerConnectionPtr(int thd, int cluster) const {
    return & workerConnections[(thd * nclusters) + cluster];
  }

  Configuration *conf;
  int generation;
  int nthreads;
  int nclusters;
  const char * config_string;
  struct ndb_engine *engine;  
  Cluster ** clusters;
  
  struct {
    int n_worker_threads;  /** number of memcached worker threads */
    int n_connections;     /** preferred number of NDB cluster connections */
    int force_send;        /** how to use NDB force-send */
    int send_timer;        /** milliseconds to set for adaptive send timer */
    int auto_grow;         /** whether to allow NDB instance pool to grow */
    int max_clients;       /** memcached max allowed connections */
  } options;

private:
  WorkerConnection ** workerConnections;
  void parse_config_string(int threads, const char *config_string);
  bool running;
};


/* S::SchedulerWorker implements the Scheduler interface. 
   There will be one SchedulerWorker for each memcached worker thread, 
   and attached to each NDB request pipeline.
 */
class S::SchedulerWorker : public Scheduler {  
public:  
  SchedulerWorker() {};
  ~SchedulerWorker() {};
  void init(int threadnum, const scheduler_options * sched_opts);
  void attach_thread(thread_identifier *);
  ENGINE_ERROR_CODE schedule(workitem *);
  void yield(workitem *) const {};
  void reschedule(workitem *) const;
  void release(workitem *);
  void add_stats(const char *, ADD_STAT, const void *);
  void shutdown();
  bool global_reconfigure(Configuration *);
  
private:
  int id;
  ndb_pipeline *pipeline;
  SchedulerGlobal * m_global;
};


/* For each connected cluster, there is one S::Cluster 
*/
class S::Cluster {
public:
  Cluster(SchedulerGlobal *, int id);
  ~Cluster();
  void add_stats(const char *, ADD_STAT, const void *);
  WorkerConnection ** getWorkerConnectionPtr(int thd) const;  
  void startThreads();
   
  bool threads_started;
  int cluster_id;
  int nconnections;
  int nreferences;
  Connection ** connections;
};


/* For each Ndb_cluster_connection, there is one instance of Connection, 
   which runs a send thread and a poll thread.
*/
class S::Connection {
  friend class S::SchedulerWorker;
  friend class S::WorkerConnection;

public:
  Connection(Cluster &, int connection_id);
  ~Connection();
  void add_stats(const char *, ADD_STAT, const void *);
  void startThreads();

  /* These are not intended to be part of the public API, but are marked as
     public so that they can be called from C code in pthread_create(): */
  void * run_ndb_send_thread();
  void * run_ndb_poll_thread();

private:
  const Cluster & cluster;
  Ndb_cluster_connection * conn;
  NdbWaitGroup *pollgroup;
  Queue<NdbInstance> * sentqueue;
  Queue<NdbInstance> * reschedulequeue;
  int id;
  int node_id;
  int n_total_workers;   /* same as SchedulerGlobal::options.n_worker_threads */
  int n_workers;         /* number of workers for this connection */
  struct {
    int initial;         /* start with this many NDB instances */
    int max;             /* scale up to this many */
  } instances; 
  pthread_t send_thread_id;
  pthread_t poll_thread_id;  
  struct {
    pthread_mutex_t lock;
    pthread_cond_t not_zero;
    unsigned int counter;
  } sem;
  struct {
    Uint64 sent_operations;
    Uint64 batches;
    Uint64 timeout_races;
  } stats;

  int get_operations_from_queue(NdbInstance **readylist, Queue<NdbInstance> *q);
};


/* For each {connection, worker} tuple there is a WorkerConnection 
*/
class S::WorkerConnection {
public:
  WorkerConnection(SchedulerGlobal *, int thd_id, int cluster_id);
  ~WorkerConnection();
  void shutdown();
  void reconfigure(Configuration *);
  NdbInstance * newNdbInstance();

  struct { 
    int thd           : 8;
    int cluster       : 8;
    int conn          : 8;
    unsigned int node : 8;
  } id;
  struct {
    int initial;
    int current;
    int max;
  } instances;
  S::Connection *conn;
  ConnQueryPlanSet *plan_set, *old_plan_set;
  NdbInstance *freelist;
  Queue<NdbInstance> * sendqueue;
};

#endif
