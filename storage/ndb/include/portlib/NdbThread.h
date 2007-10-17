/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NDB_THREAD_H
#define NDB_THREAD_H

#include <ndb_global.h>

#ifdef	__cplusplus
extern "C" {
#endif

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
 *  * thread_stack_size: stack size for this thread
 *  * p_thread_name: pointer to name of this thread
 *  * returnvalue: pointer to the created thread
 */
struct NdbThread* NdbThread_Create(NDB_THREAD_FUNC *p_thread_func,
                      NDB_THREAD_ARG *p_thread_arg,
  		      const NDB_THREAD_STACKSIZE thread_stack_size,
		      const char* p_thread_name,
                      NDB_THREAD_PRIO thread_prio);

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


#ifdef	__cplusplus
}
#endif

#endif









