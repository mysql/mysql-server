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
#ifndef NDBMEMCACHE_SCHEDULER_H
#define NDBMEMCACHE_SCHEDULER_H

#include "ndbmemcache_global.h"
#include <memcached/types.h>

#include "thread_identifier.h"


typedef struct {
  int nthreads;                /* number of worker threads */
  int max_clients;             /* maximum number of client connections */
  const char * config_string;  /* scheduler-specific configuration string */
} scheduler_options;       


#ifdef __cplusplus

/* Forward declarations */
class Configuration;
class workitem;

/* Scheduler is an interface */

class Scheduler {
protected:
  virtual ~Scheduler() {};
  
public:
  /* Public Interface */
  Scheduler() {};
  
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

  /** Before an NDB callback function completes, it must call either 
      reschedule() or yield().  yield() indicates that work is comlpete. */
  virtual void yield(workitem *) const = 0;

  /** Before an NDB callback function completes, it must call either 
      reschedule() or yield(). reschedule() indicates to that the workitem 
      requires the scheduler to send & poll an additional operation. */
  virtual void reschedule(workitem *) const = 0;
 
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
