/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <ndb_global.h>
#include <NdbThread.h>
#include <pthread.h>
#include <NdbMem.h>

#define MAX_THREAD_NAME 16

/*#define USE_PTHREAD_EXTRAS*/

struct NdbThread 
{ 
  pthread_t thread;
  char thread_name[MAX_THREAD_NAME];
};



struct NdbThread* NdbThread_Create(NDB_THREAD_FUNC *p_thread_func,
                      NDB_THREAD_ARG *p_thread_arg,
  		      const NDB_THREAD_STACKSIZE thread_stack_size,
		      const char* p_thread_name,
                      NDB_THREAD_PRIO thread_prio)
{
  struct NdbThread* tmpThread;
  int result;
  pthread_attr_t thread_attr;

  (void)thread_prio; /* remove warning for unused parameter */

  if (p_thread_func == NULL)
    return 0;

  tmpThread = (struct NdbThread*)NdbMem_Allocate(sizeof(struct NdbThread));
  if (tmpThread == NULL)
    return NULL;

  strnmov(tmpThread->thread_name,p_thread_name,sizeof(tmpThread->thread_name));

  pthread_attr_init(&thread_attr);
  pthread_attr_setstacksize(&thread_attr, thread_stack_size);
#ifdef USE_PTHREAD_EXTRAS
  /* Guard stack overflow with a 2k databuffer */
  pthread_attr_setguardsize(&thread_attr, 2048);
#endif

#ifdef PTHREAD_CREATE_JOINABLE /* needed on SCO */
  pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
#endif
  result = pthread_create(&tmpThread->thread, 
			  &thread_attr,
  		          p_thread_func,
  		          p_thread_arg);
  assert(result==0);

  pthread_attr_destroy(&thread_attr);
  return tmpThread;
}


void NdbThread_Destroy(struct NdbThread** p_thread)
{
  if (*p_thread != NULL){    
    free(* p_thread); 
    * p_thread = 0;
  }
}


int NdbThread_WaitFor(struct NdbThread* p_wait_thread, void** status)
{
  int result;

  if (p_wait_thread == NULL)
    return 0;

  if (p_wait_thread->thread == 0)
    return 0;

  result = pthread_join(p_wait_thread->thread, status);
  
  return result;
}


void NdbThread_Exit(int status)
{
  pthread_exit(&status);
}


int NdbThread_SetConcurrencyLevel(int level)
{
#ifdef USE_PTHREAD_EXTRAS
  return pthread_setconcurrency(level);
#else
  (void)level; /* remove warning for unused parameter */
  return 0;
#endif
}
