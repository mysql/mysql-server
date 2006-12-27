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


#include <ndb_global.h>

#include <NdbCondition.h>
#include <NdbThread.h>
#include <NdbMutex.h>
#include <NdbMem.h>

struct NdbCondition
{
  pthread_cond_t cond;
};



struct NdbCondition* 
NdbCondition_Create(void)
{
  struct NdbCondition* tmpCond;
  int result;
  
  tmpCond = (struct NdbCondition*)NdbMem_Allocate(sizeof(struct NdbCondition));
  
  if (tmpCond == NULL)
    return NULL;
  
  result = pthread_cond_init(&tmpCond->cond, NULL);
  
  assert(result==0);
  return tmpCond;
}



int 
NdbCondition_Wait(struct NdbCondition* p_cond,
                  NdbMutex* p_mutex)
{
  int result;

  if (p_cond == NULL || p_mutex == NULL)
    return 1;
  
  result = pthread_cond_wait(&p_cond->cond, p_mutex);
  
  return result;
}

int 
NdbCondition_WaitTimeout(struct NdbCondition* p_cond,
                         NdbMutex* p_mutex,
                         int msecs){
  int result;
  struct timespec abstime; 
  int secs = 0;
  
  if (p_cond == NULL || p_mutex == NULL)
    return 1;
  
#ifdef HAVE_CLOCK_GETTIME
  clock_gettime(CLOCK_REALTIME, &abstime);
#else
  {
    struct timeval tick_time;
    gettimeofday(&tick_time, 0);
    abstime.tv_sec  = tick_time.tv_sec;
    abstime.tv_nsec = tick_time.tv_usec * 1000;
  }
#endif

  if(msecs >= 1000){
    secs  = msecs / 1000;
    msecs = msecs % 1000;
  }

  abstime.tv_sec  += secs;
  abstime.tv_nsec += msecs * 1000000;
  if (abstime.tv_nsec >= 1000000000) {
    abstime.tv_sec  += 1;
    abstime.tv_nsec -= 1000000000;
  }
    
  result = pthread_cond_timedwait(&p_cond->cond, p_mutex, &abstime);
  
  return result;
}

int 
NdbCondition_Signal(struct NdbCondition* p_cond){
  int result;

  if (p_cond == NULL)
    return 1;

  result = pthread_cond_signal(&p_cond->cond);
                             
  return result;
}


int NdbCondition_Broadcast(struct NdbCondition* p_cond)
{
  int result;

  if (p_cond == NULL)
    return 1;

  result = pthread_cond_broadcast(&p_cond->cond);
                             
  return result;
}


int NdbCondition_Destroy(struct NdbCondition* p_cond)
{
  int result;

  if (p_cond == NULL)
    return 1;

  result = pthread_cond_destroy(&p_cond->cond);
  free(p_cond);

  return 0;
}

