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


#include "NdbCondition.h"
#include <pthread.h>
#include <sys/types.h>
#include <malloc.h>

#include <NdbMutex.h>

#include "NdbConditionOSE.h"
struct NdbCondition
{
  PROCESS condserv_pid;
};


OS_PROCESS(ndbcond_serv){

  union SIGNAL* sig;
  union SIGNAL* sig2;

  static const SIGSELECT sel_signal[] = {2, NDBCOND_SIGNAL, NDBCOND_BROADCAST};
  static const SIGSELECT sel_cond[] = {2, NDBCOND_WAIT, NDBCOND_WAITTIMEOUT};

  for(;;){
    /* Receive condition wait signal */
    sig = receive((SIGSELECT*)sel_cond);
    if (sig != NIL){
      switch (sig->sigNo){

      case NDBCOND_WAIT:
        /* Wait for a SIGNAL or BROADCAST from anyone */
        sig2 = receive((SIGSELECT*)sel_signal);
        if (sig2 != NIL){ 
          switch(sig2->sigNo){

          case NDBCOND_SIGNAL:
            ((struct NdbCondWait*)sig)->status = NDBCOND_SIGNALED;
            /* Send signal back to the one waiting for this condition */
            send(&sig, sender(&sig));
            break;
          case NDBCOND_BROADCAST:
            /* Not handled yet */
            assert(1==0);
            break;
          default:
            assert(1==0);
            break;
          }         
          free_buf(&sig2);
        }
        break;

      case NDBCOND_WAITTIMEOUT:
        /* Wait for a SIGNAL or BROADCAST from anyone */
        sig2 = receive_w_tmo(((struct NdbCondWaitTimeout*)sig)->timeout, (SIGSELECT*)sel_signal);
        if (sig2 != NIL){ 
          switch(sig2->sigNo){

          case NDBCOND_SIGNAL:
            ((struct NdbCondWaitTimeout*)sig)->status = NDBCOND_SIGNALED;
            /* Send signal back to the one waiting for this condition */
            send(&sig, sender(&sig));
            break;
          case NDBCOND_BROADCAST:
            /* Not handled yet */
            assert(1==0);
            break;
          default:
            assert(1==0);
            break;
          }         
          free_buf(&sig2);
        }else{
          ((struct NdbCondWaitTimeout*)sig)->status = NDBCOND_TIMEOUT;
          send(&sig, sender(&sig)); 
        }
        break;

      default:
        assert(1==0);
        break;
      
      }
    }

  } 
}


struct NdbCondition* 
NdbCondition_Create(void)
{
  struct NdbCondition* tmpCond;
 
  
  tmpCond = (struct NdbCondition*)malloc(sizeof(struct NdbCondition));
  
  if (tmpCond == NULL)
    return NULL;

  /**
   * Start this process with a quite high 
   * priority, we want it to be responsive 
   */ 
  tmpCond->condserv_pid = create_process(OS_PRI_PROC,    /* Process type */
                                         "ndbcond_serv", /* Name */
                                         ndbcond_serv,   /* Entry point */
                                         2048,           /* Stack size */
                                         10,             /* Priority */
                                         0,              /* Time slice */
                                         get_bid(current_process()), /* Block */
                                         NULL,           /* Redir table */
                                         0,
                                         0);  
  
  start(tmpCond->condserv_pid);

  return tmpCond;
}


int 
NdbCondition_Wait(struct NdbCondition* p_cond,
                  NdbMutex* p_mutex)
{
  static const SIGSELECT sel_cond[] = {1, NDBCOND_WAIT};
  union SIGNAL* sig;
  int result;
  if (p_cond == NULL || p_mutex == NULL)
    return 0;
  
  sig = alloc(sizeof(struct NdbCondWait), NDBCOND_WAIT);
  send(&sig, p_cond->condserv_pid);

  NdbMutex_Unlock(p_mutex);
  
  result = 1;
  while(NIL == (sig = receive_from((OSTIME)-1, (SIGSELECT*)sel_cond, p_cond->condserv_pid)));
  if (sig != NIL){
    if (sig->sigNo == NDBCOND_WAIT){
      /* Condition is signaled */
      result = 0;
    }else{
      assert(1==0);
    }
    free_buf(&sig);
    
  }
  NdbMutex_Lock(p_mutex);

  return result;
}


int 
NdbCondition_WaitTimeout(struct NdbCondition* p_cond,
                         NdbMutex* p_mutex,
                         int msecs){
  static const SIGSELECT sel_cond[] = {1, NDBCOND_WAITTIMEOUT};
  union SIGNAL* sig;
  int result;  
  if (p_cond == NULL || p_mutex == NULL)
    return 0;
  
  sig = alloc(sizeof(struct NdbCondWaitTimeout), NDBCOND_WAITTIMEOUT);
  ((struct NdbCondWaitTimeout*)sig)->timeout = msecs;
  send(&sig, p_cond->condserv_pid);

  NdbMutex_Unlock(p_mutex);
  
  result = 1;
  while(NIL == (sig = receive_from((OSTIME)-1, (SIGSELECT*)sel_cond, p_cond->condserv_pid)));
  if (sig != NIL){
    if (sig->sigNo == NDBCOND_WAITTIMEOUT){
      /* Condition is signaled */
      result = 0;
    }else{
      assert(1==0);
    }
    free_buf(&sig);
    
  }
  
  NdbMutex_Lock(p_mutex);
  
  return result;
}


int
NdbCondition_Signal(struct NdbCondition* p_cond){

  union SIGNAL* sig;
  if (p_cond == NULL)
    return 1;

  sig = alloc(sizeof(struct NdbCondSignal), NDBCOND_SIGNAL);
  send(&sig, p_cond->condserv_pid);
                             
  return 0;
}


int NdbCondition_Broadcast(struct NdbCondition* p_cond)
{
  union SIGNAL* sig;
  if (p_cond == NULL)
    return 1;

  sig = alloc(sizeof(struct NdbCondBroadcast), NDBCOND_BROADCAST);
  send(&sig, p_cond->condserv_pid);
                             
  return 0;
}


int NdbCondition_Destroy(struct NdbCondition* p_cond)
{
  if (p_cond == NULL)
    return 1;

  kill_proc(p_cond->condserv_pid);
  free(p_cond);

  return 0;
}

