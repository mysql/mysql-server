/* Copyright (c) 2003-2005, 2007 MySQL AB


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */


#ifndef NDB_THREAD_H
#define NDB_THREAD_H

#include <ndb_global.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define THREAD_CONTAINER_SIZE 128
typedef enum NDB_THREAD_PRIO_ENUM {
  NDB_THREAD_PRIO_HIGHEST,
  NDB_THREAD_PRIO_HIGH,
  NDB_THREAD_PRIO_MEAN,
  NDB_THREAD_PRIO_LOW,
  NDB_THREAD_PRIO_LOWEST
} NDB_THREAD_PRIO;

typedef void* (NDB_THREAD_FUNC)(void*);
typedef void* NDB_THREAD_ARG;
typedef size_t NDB_THREAD_STACKSIZE;

#ifdef HAVE_LINUX_SCHEDULING
  typedef pid_t NDB_TID_TYPE;
#else
#ifdef HAVE_SOLARIS_AFFINITY
  typedef id_t NDB_TID_TYPE;
#else
  typedef int NDB_TID_TYPE;
#endif
#endif

#ifdef HAVE_LINUX_SCHEDULING
  typedef pid_t NDB_THAND_TYPE;
#else
#ifdef HAVE_PTHREAD_SELF
  typedef pthread_t NDB_THAND_TYPE;
#else
  typedef int NDB_THAND_TYPE;
#endif
#endif

/**
 * Three functions that are used in conjunctions with SocketClient
 * threads and SocketServer threads. In the NDB kernel they are
 * used to set real-time properties and lock threads to CPU's. In
 * the NDB API they are dummy subroutines.
 * ndb_thread_add_thread_id is used before starting run method in thread.
 * ndb_thread_remove_thread_id is called immediately after returning from
 * run-method in thread.
 * ndb_thread_fill_thread_object is called before calling
 * ndb_thread_add_thread_id to prepare parameters.
 */
void*
ndb_thread_add_thread_id(void *param);

void*
ndb_thread_remove_thread_id(void *param);

void
ndb_thread_fill_thread_object(void *param, uint *len, my_bool server);

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

struct NdbThread* NdbThread_CreateWithFunc(NDB_THREAD_FUNC *p_thread_func,
                      NDB_THREAD_ARG *p_thread_arg,
  		      const NDB_THREAD_STACKSIZE thread_stack_size,
		      const char* p_thread_name,
                      NDB_THREAD_PRIO thread_prio,
                      NDB_THREAD_FUNC *start_func,
                      NDB_THREAD_ARG start_obj,
                      size_t start_obj_len,
                      NDB_THREAD_FUNC *end_func,
                      NDB_THREAD_ARG end_obj,
                      size_t end_obj_len);

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
 * Get thread id
 */
NDB_TID_TYPE NdbThread_getThreadId();

/**
 * Get thread handle
 */
NDB_THAND_TYPE NdbThread_getThreadHandle();

/**
 * Set Scheduler for pid
 */
int NdbThread_SetScheduler(NDB_THAND_TYPE threadHandle, my_bool rt_prio,
                           my_bool high_prio);

/**
 * Lock Thread to CPU
 */
int NdbThread_LockCPU(NDB_TID_TYPE threadId, Uint32 cpu_id);

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









