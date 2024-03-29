/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_THREAD_H
#define NDB_THREAD_H

#include <ndb_global.h>

/* Error codes for Locking to CPUs and CPU sets */
#define BIND_CPU_NOT_SUPPORTED_ERROR 31999
#define CPU_SET_MIX_EXCLUSIVE_ERROR 31998
#define EXCLUSIVE_CPU_SET_NOT_SUPPORTED_ERROR 31997
#define NON_EXCLUSIVE_CPU_SET_NOT_SUPPORTED_ERROR 31996
#define CPU_ID_OUT_OF_RANGE_ERROR 31995
#define CPU_ID_MISSING_ERROR 31994
#define SET_THREAD_PRIO_NOT_SUPPORTED_ERROR 31993
#define SET_THREAD_PRIO_OUT_OF_RANGE_ERROR 31992

typedef enum NDB_THREAD_PRIO_ENUM {
  NDB_THREAD_PRIO_HIGHEST,
  NDB_THREAD_PRIO_HIGH,
  NDB_THREAD_PRIO_MEAN,
  NDB_THREAD_PRIO_LOW,
  NDB_THREAD_PRIO_LOWEST
} NDB_THREAD_PRIO;


/* NDB thread pointer. */
struct NdbThread;
extern thread_local NdbThread* NDB_THREAD_TLS_NDB_THREAD;

extern "C" typedef void* (NDB_THREAD_FUNC)(void*);
typedef void* NDB_THREAD_ARG;
typedef size_t NDB_THREAD_STACKSIZE;

struct NdbThread;

/**
 * Create a thread
 *
 *  * p_thread_func: pointer of the function to run in the thread
 *  * p_thread_arg: pointer to argument to be passed to the thread 
 *  * thread_stack_size: stack size for this thread, 0 => default size
 *  * p_thread_name: pointer to name of this thread
 *  * returnvalue: pointer to the created thread
 */
struct NdbThread* NdbThread_Create(NDB_THREAD_FUNC *p_thread_func,
                                   NDB_THREAD_ARG *p_thread_arg,
                                   const NDB_THREAD_STACKSIZE thread_stack_size,
                                   const char* p_thread_name,
                                   NDB_THREAD_PRIO thread_prio);


/**
 * Create a thread-object for "main" that can be used with
 *   other NdbThread_*-functions
 */
struct NdbThread* NdbThread_CreateObject(const char * name);

/**
 * Create a thread object handled by other portability system
 * where we want to use Ndb for CPU locking.
 */
struct NdbThread* NdbThread_CreateLockObject(int tid);

/**
 * Destroy a thread
 *  Deallocates memory for thread
 *  And NULL the pointer
 *
 */
void NdbThread_Destroy(struct NdbThread** p_thread);
 
/**
 * Waitfor a thread, suspend the execution of the calling thread
 * until the wait_thread_id completes
 *
 * * p_wait_thread, pointer to the thread to wait for
 * * status: exit code from thread waited for
 * * returnvalue: true = succeeded, false = failed
 */
int NdbThread_WaitFor(struct NdbThread* p_wait_thread, void** status);      

/**
 * Exit thread, terminates the calling thread 
 *   
 * *  status: exit code
 */
void NdbThread_Exit(void *status);

/**
 * Set thread concurrency level
 * Deprecated function and no longer used other than in old
 * program parts.
 * *  
 */
int NdbThread_SetConcurrencyLevel(int level);

/**
 * Get "Tid" for thread
 * This is used in many OS interfaces and also used for printing.
 *   return -1, if not supported on platform
 */
int NdbThread_GetTid(struct NdbThread*);

/**
 * Yield to normal time-share prio and back to real-time prio for
 * real-time threads
 */
int NdbThread_yield_rt(struct NdbThread*, bool high_prio);

/**
 * Set Scheduler for thread
 * This sets real-time priority of the thread.
 */
int NdbThread_SetScheduler(struct NdbThread*, bool rt_prio, bool high_prio);

/**
 * Set Thread priority for thread
 *
 * SetThreadPrioNormal is equal to SetThreadPrio(thread, 5);
 *
 * Currently we support 11 levels from 0 to 10. 10 is a bit special level
 * on Solaris which indicates to Solaris that we want to own a CPU core.
 * On Windows, prio 10 indicates a special real-time priority. 
 */
#define NO_THREAD_PRIO_USED 11
#define MAX_THREAD_PRIO_NUMBER 10
int NdbThread_SetThreadPrio(struct NdbThread*, unsigned int prio);
int NdbThread_SetThreadPrioNormal(struct NdbThread*);

/**
 * Lock/Unlock Thread to CPU
 * These calls are support routines to the calls defined in
 * NdbLockCpuUtil.c. These calls handle all the portability
 * aspects.
 */

/**
 * Both of the below structs are defined on higher layers in the
 * API. They are however stored on the thread object in this
 * layer, so we need to be able to store pointers to those.
 */
struct NdbCpuSet;
struct processor_set_handler;

/**
 * Create a CPU set for later use in locking call.
 * The CPU set is a non-exclusive CPU set.
 */
int NdbThread_LockCreateCPUSet(const Uint32 *cpu_ids,
                               Uint32 num_cpus,
                               struct NdbCpuSet **cpu_set);

/**
 * Create a CPU set for later use in locking call.
 * The CPU set is an exclusive CPU set. This means that
 * no other threads can use this set of CPUs for any
 * activity. This is currently only supported on Solaris.
 */
int NdbThread_LockCreateCPUSetExclusive(const Uint32 *cpu_ids,
                                        Uint32 num_cpus,
                                        struct NdbCpuSet **cpu_set);

/**
 * Destroy a CPU set previously used to lock CPUs, this is destruction
 * of a non-exclusive CPU set.
 */
void NdbThread_LockDestroyCPUSet(struct NdbCpuSet *cpu_set);

/**
 * Destroy a CPU set previously used to lock CPUs, this is destruction
 * of an exclusive CPU set.
 */
void NdbThread_LockDestroyCPUSetExclusive(struct NdbCpuSet *cpu_set);

/**
 * Make the locking call for this particular thread to a previously set
 * up CPU set which is non-exclusive.
 */
int NdbThread_LockCPUSet(struct NdbThread*,
                         struct NdbCpuSet *cpu_set,
                         const struct processor_set_handler *cpu_set_key);

/**
 * Make the locking call for this particular thread to a previously set
 * up CPU set which is exclusive.
 */
int NdbThread_LockCPUSetExclusive(struct NdbThread*,
                                  struct NdbCpuSet *cpu_set,
                                  const struct processor_set_handler *cpu_set_key);

/**
 * Lock a thread to a specific CPU in an non-exclusive manner.
 */
int NdbThread_LockCPU(struct NdbThread*,
                      Uint32 cpu,
                      const struct processor_set_handler *cpu_set_key);

/**
 * Restore the old locking (if any) that was in place before the first call
 * to lock to CPU sets.
 */
int NdbThread_UnlockCPU(struct NdbThread*);

/**
 * Unassign thread from CPU set (only needed on Windows)
 */
void NdbThread_UnassignFromCPUSet(struct NdbThread*,
                                  struct NdbCpuSet *cpu_set);

/**
 * Retrieve CPU set key from portability layer.
 */
const struct processor_set_handler*
  NdbThread_LockGetCPUSetKey(struct NdbThread*);

/* Get my own NdbThread pointer */
struct NdbThread *NdbThread_GetNdbThread();

/**
 * Set properties for NDB_THREAD_PRIO_HIGH
 *
 * NOTE 1: should be set *prior* to starting thread
 * NOTE 2: if these properties *can* be applied is not checked
 *         if they can not, then it will be silently ignored
 *
 * @param spec <fifo | rr>[,<prio>]
 *
 * @return 0  - parse ok
 *  return -1 - Invalid spec
 */
int NdbThread_SetHighPrioProperties(const char * spec);

/**
 * Clear Unix signal mask of thread
 */
void NdbThread_ClearSigMask();

/**
 * Check if CPU is available for our use, used to avoid using CPUs in
 * automatic CPU locking that the process is not supposed to use. If
 * we use ThreadConfig then the user have decided to use those anyways,
 * so in that case we don't care.
 */
bool
NdbThread_IsCPUAvailable(Uint32 cpu_id);

#endif
