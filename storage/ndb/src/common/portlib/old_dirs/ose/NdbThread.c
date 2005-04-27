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


#include "NdbThread.h"
#include <pthread.h>
#include <malloc.h>
#include <string.h>
#include <NdbOut.hpp>

#define MAX_THREAD_NAME 16


struct NdbThread 
{ 
  PROCESS pid;
  char thread_name[MAX_THREAD_NAME];
};

#define NDBTHREAD_SIGBASE  4010
   
#define NDBTHREAD_START           (NDBTHREAD_SIGBASE + 1)  /* !-SIGNO(struct NdbThreadStart)-! */  

struct NdbThreadStart
{
  SIGSELECT sigNo;
  NDB_THREAD_FUNC* func;
  NDB_THREAD_ARG arg;
};

struct NdbThreadStopped
{
  SIGSELECT sigNo;
};

union SIGNAL 
{
  SIGSELECT sigNo;
  struct NdbThreadStart          threadStart;
  struct NdbThreadStopped        threadStopped;
};

OS_PROCESS(thread_starter){
  static const SIGSELECT sel_start[] = {1, NDBTHREAD_START};
  struct NdbThreadStart* sigstart;
  union SIGNAL* sig;

  /* Receive function adress and params */
  sig = receive((SIGSELECT*)sel_start);
  if (sig != NIL){
    if (sig->sigNo == NDBTHREAD_START){
      sigstart = ((struct NdbThreadStart*)sig);
      /* Execute function with arg */
      (*sigstart->func)(sigstart->arg);
    }else{
      assert(1==0);
    }
    free_buf(&sig);
  }
}

struct NdbThread* NdbThread_Create(NDB_THREAD_FUNC* p_thread_func,
                      NDB_THREAD_ARG *p_thread_arg,
  		      const NDB_THREAD_STACKSIZE thread_stack_size,
		      const char* p_thread_name,
                      NDB_THREAD_PRIO thread_prio)
{
  struct NdbThread* tmpThread;
  union SIGNAL* sig;
  int ose_prio;

  if (p_thread_func == NULL)
    return 0;

  tmpThread = (struct NdbThread*)malloc(sizeof(struct NdbThread));
  if (tmpThread == NULL)
    return NULL;

  strncpy((char*)&tmpThread->thread_name, p_thread_name, MAX_THREAD_NAME);

  switch(thread_prio){
  case NDB_THREAD_PRIO_HIGHEST:
    ose_prio = 1;
    break;
  case NDB_THREAD_PRIO_HIGH:
    ose_prio = 10;
    break;
  case NDB_THREAD_PRIO_MEAN:
    ose_prio = 16;
    break;
  case NDB_THREAD_PRIO_LOW:
    ose_prio = 23;
    break;
  case NDB_THREAD_PRIO_LOWEST:
    ose_prio = 31;
    break;
  default:
    return NULL;
    break;
  }

  /* Create process */
  tmpThread->pid = create_process(OS_PRI_PROC,    /* Process type */
                                  (char*)p_thread_name,  /* Name */
                                  thread_starter,  /* Entry point */
                                  thread_stack_size, /* Stack size */
                                  ose_prio,              /* Priority */
                                  0,              /* Time slice */
                                  get_bid(current_process()), /* Block */
                                  NULL,           /* Redir table */
                                  0,
                                  0);

  /* Send params to process */ 
  sig = alloc(sizeof(struct NdbThreadStart), NDBTHREAD_START);
  ((struct NdbThreadStart*)sig)->func = p_thread_func;
  ((struct NdbThreadStart*)sig)->arg = p_thread_arg;
  send(&sig, tmpThread->pid);

  /* Enable NDB_HOME environment variable for the thread */
  {
    /* Hardcoded NDB_HOME...*/
    char* ndb_home_env = get_env(current_process(), "NDB_HOME");
    if (ndb_home_env != NULL)
    {
      /* Set NDB_HOME */
      int rc = set_env(tmpThread->pid, "NDB_HOME", ndb_home_env);
      if (rc != 0)
      {
	/* Not really a problem */
      }
    } /* Enable NDB_HOME */
  }

  /* Start process */
  start(tmpThread->pid);

  return tmpThread;
}



void NdbThread_Destroy(struct NdbThread** p_thread)
{
  free(* p_thread); * p_thread = 0;
}


int NdbThread_WaitFor(struct NdbThread* p_wait_thread, void** status)
{
  while(hunt(p_wait_thread->thread_name, 0, NULL, NULL) != 0) 
    delay(1000);

  * status = 0;
  
  return 0;
}


void NdbThread_Exit(int a)
{
  kill_proc(current_process());
}


int NdbThread_SetConcurrencyLevel(int level)
{
  return 0;
}

