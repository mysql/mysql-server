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

#ifndef NDBMEMCACHE_STOCKHOLM_SCHEDULER_H
#define NDBMEMCACHE_STOCKHOLM_SCHEDULER_H

#ifndef __cplusplus
#error "This file is for C++ only"
#endif

#include <memcached/types.h>

#include "ndbmemcache_config.h"
#include "workitem.h"
#include "Scheduler.h"
#include "KeyPrefix.h"
#include "ConnQueryPlanSet.h"


/* 
 *              Stockholm Scheduler 
 *
 * The Stockholm scheduler runs in two threads and uses a large number of 
 * Ndb objects. 
 */
class Scheduler_stockholm : public Scheduler {
public:
  Scheduler_stockholm() {};
  ~Scheduler_stockholm() {};
  void init(int threadnum, const scheduler_options *options);
  void attach_thread(thread_identifier *);
  ENGINE_ERROR_CODE schedule(workitem *);
  void yield(workitem *) const;                                       // inlined
  void reschedule(workitem *) const;                                  // inlined
  void release(workitem *);
  void add_stats(const char *, ADD_STAT, const void *);
  void shutdown();
  void * run_ndb_commit_thread(int cluster_id);
  bool global_reconfigure(Configuration *) { return false; } ;

private:  
  ndb_pipeline *pipeline;
  struct {
    struct workqueue *queue; 
    struct sched_stats_stockholm { 
      uint64_t cycles;      /* total number of loops in the commit thread */
      uint64_t commit_thread_vtime;
    } stats;      
    pthread_t commit_thread_id;
    ConnQueryPlanSet * plan_set;
    NdbInstance **instances;
    int nInst;
    NdbInstance *nextFree;
  } cluster[MAX_CLUSTERS];
};


inline void Scheduler_stockholm::reschedule(workitem *item) const {
  item->base.reschedule = 1;
}


inline void Scheduler_stockholm::yield(workitem *item) const { } 


#endif

