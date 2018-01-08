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
#ifndef NDBMEMCACHE_SCHEDULER_H
#define NDBMEMCACHE_SCHEDULER_H

#include "ndbmemcache_global.h"
#include <memcached/types.h>
#include "thread_identifier.h"


typedef struct scheduler_options_st {
  int nthreads;                /* number of worker threads */
  int max_clients;             /* maximum number of client connections */
  const char * config_string;  /* scheduler-specific configuration string */
} scheduler_options;       

typedef enum {
  YIELD = 0,
  RESCHEDULE = 1,
} prepare_flags;

#ifdef __cplusplus

/* Forward declarations */
class Configuration;

/* Scheduler is an interface */

class Scheduler {

public:
  /* Public Interface */
  Scheduler() {};

  virtual ~Scheduler() {};

  /* Static class method calls prepare on a workitem */
  static void execute(NdbTransaction *, NdbTransaction::ExecType, 
                      NdbAsynchCallback, struct workitem *, 
                      prepare_flags flags);
 
  /** init() is the called from the main thread, 
      after configuration has been read. 
      threadnum: which thread this scheduler will eventually attach to 
      options: struct specifying run-time options   
  */
  virtual void init(int threadnum, const scheduler_options * options) = 0;
                    
  /** attach_thread() is called from each thread 
      at pipeline initialization time. */
  virtual void attach_thread(thread_identifier *) = 0;

  /** schedule() is called from the NDB Engine thread when a workitem
      is ready to be queued for further async processing.  It will obtain
      an Ndb object for the operation and send the workitem to be executed. */ 
  virtual ENGINE_ERROR_CODE schedule(workitem *) = 0;

  /** prepare() is a callback into the scheduler, and wraps whatever set of Ndb
        asynch execute calls is appropriate for the scheduler. 
        It is called from Scheduler::execute().  */
  virtual void prepare(NdbTransaction *, 
                       NdbTransaction::ExecType, NdbAsynchCallback, 
                       workitem *, prepare_flags flags) = 0;

  /** close() is a callback into the scheduler to close a transaction. */
  virtual void close(NdbTransaction *, workitem *) = 0;

  /** release() is called from the NDB Engine thread after an operation has
       completed.  It allows the scheduler to release any resources (such as
       the Ndb object) that were allocated in schedule(). */
  virtual void release(workitem *) = 0;
  
  /** add_stats() allows the engine to delegate certain statistics
      to the scheduler. */
  virtual void add_stats(const char *key, ADD_STAT, const void *) = 0;

  /** Shut down a scheduler. */
  virtual void shutdown() = 0;

  /** global_reconfigure() is a single call requesting *every* scheduler 
      instance to replace its current Configuration with a new one. This returns 
      true on success,  which implies that any pointer the scheduler had held to 
      the previous configuration is released.  If the scheduler is not able to 
      perform the online configuration change, it should return false.
   */
  virtual bool global_reconfigure(Configuration *new_config) = 0;

                      
};
#endif

#endif
