/*
 Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.
 
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
  ~Scheduler_stockholm();
  void init(int threadnum, const scheduler_options *options);
  void attach_thread(thread_identifier *);
  ENGINE_ERROR_CODE schedule(workitem *);
  void prepare(NdbTransaction *, NdbTransaction::ExecType, NdbAsynchCallback, 
               workitem *, prepare_flags);
  void close(NdbTransaction *, workitem *);
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


#endif

