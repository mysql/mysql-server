/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_THREAD_H
#define NDB_THREAD_H

#include <ndb_global.h>
#include <my_thread_local.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define UNDEFINED_PROCESSOR_SET 0xFFFF

typedef enum NDB_THREAD_PRIO_ENUM {
  NDB_THREAD_PRIO_HIGHEST,
  NDB_THREAD_PRIO_HIGH,
  NDB_THREAD_PRIO_MEAN,
  NDB_THREAD_PRIO_LOW,
  NDB_THREAD_PRIO_LOWEST
} NDB_THREAD_PRIO;

typedef enum NDB_THREAD_TLS_ENUM {
  NDB_THREAD_TLS_JAM,           /* Jam buffer pointer. */
  NDB_THREAD_TLS_THREAD,        /* Thread self pointer. */
  NDB_THREAD_TLS_NDB_THREAD,    /* NDB thread pointer */
  NDB_THREAD_TLS_MAX
} NDB_THREAD_TLS;

typedef void* (NDB_THREAD_FUNC)(void*);
typedef void* NDB_THREAD_ARG;
typedef size_t NDB_THREAD_STACKSIZE;

struct NdbThread;

/*
  Method to block/unblock thread from receiving KILL signal with
  signum set in g_ndb_shm_signum in a portable manner.
*/
#ifdef NDB_SHM_TRANSPORTER
void NdbThread_set_shm_sigmask(my_bool block);
#endif

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
 * * returnvalue: true = succeded, false = failed
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
 *   
 * *  
 */
int NdbThread_SetConcurrencyLevel(int level);

/**
 * Get "Tid" for thread
 *   should only be used for printing etc...
 *   return -1, if not supported on platform
 */
int NdbThread_GetTid(struct NdbThread*);

/**
 * Yield to normal time-share prio and back to real-time prio for
 * real-time threads
 */
int NdbThread_yield_rt(struct NdbThread*, my_bool high_prio);

/**
 * Set Scheduler for thread
 */
int NdbThread_SetScheduler(struct NdbThread*, my_bool rt_prio, my_bool high_prio);

/**
 * Lock/Unlock Thread to CPU
 */
struct NdbCpuSet;
struct processor_set_handler;

int NdbThread_LockCreateCPUSet(const Uint32 *cpu_ids,
                               Uint32 num_cpus,
                               struct NdbCpuSet **cpu_set);
void NdbThread_LockDestroyCPUSet(struct NdbCpuSet *cpu_set);
int NdbThread_LockCPUSet(struct NdbThread*,
                         struct NdbCpuSet *cpu_set,
                         const struct processor_set_handler *cpu_set_key);
int NdbThread_LockCPU(struct NdbThread*,
                      Uint32 cpu,
                      const struct processor_set_handler *cpu_set_key);
int NdbThread_UnlockCPU(struct NdbThread*);
const struct processor_set_handler*
  NdbThread_LockGetCPUSetKey(struct NdbThread*);

/**
 * Fetch and set thread-local storage entry.
 */
void *NdbThread_GetTlsKey(NDB_THREAD_TLS key);
void NdbThread_SetTlsKey(NDB_THREAD_TLS key, void *value);
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

#ifdef	__cplusplus
}
#endif

#endif
