/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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


#include <ndb_global.h>

#include <NdbCondition.h>
#include <NdbMutex.h>
#include <NdbMem.h>

static int init = 0;
#ifdef HAVE_CLOCK_GETTIME
static int clock_id = CLOCK_REALTIME;
#endif

void
NdbCondition_initialize(int need_monotonic)
{
#if defined HAVE_CLOCK_GETTIME && defined HAVE_PTHREAD_CONDATTR_SETCLOCK && \
    defined CLOCK_MONOTONIC
  
  int res, condattr_init = 0;
  pthread_cond_t tmp;
  pthread_condattr_t attr;

  init = 1;

  if (!need_monotonic)
    return;

  if ((res = pthread_condattr_init(&attr)) != 0)
    goto nogo;

  condattr_init = 1;
  
  if ((res = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)) != 0)
    goto nogo;

  if ((res = pthread_cond_init(&tmp, &attr)) != 0)
    goto nogo;

  pthread_condattr_destroy(&attr);
  pthread_cond_destroy(&tmp);

  clock_id = CLOCK_MONOTONIC;
  
  return;
  
nogo:
  if (condattr_init)
  {
    pthread_condattr_destroy(&attr);
  }
  
  clock_id = CLOCK_REALTIME;
  fprintf(stderr, 
          "Failed to use CLOCK_MONOTONIC for pthread_condition res: %u\n", 
          res);
  fflush(stderr);
  return;
#else
  init = 1;
#endif
}

int
NdbCondition_Init(struct NdbCondition* ndb_cond)
{
  int result;

  assert(init); /* Make sure library has been initialized */

#if defined HAVE_CLOCK_GETTIME && defined HAVE_PTHREAD_CONDATTR_SETCLOCK && \
    defined CLOCK_MONOTONIC
  if (clock_id == CLOCK_MONOTONIC)
  {
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    result = pthread_cond_init(&ndb_cond->cond, &attr);
    pthread_condattr_destroy(&attr);
  }
  else
  {
    result = pthread_cond_init(&ndb_cond->cond, NULL);
  }
#else
  result = pthread_cond_init(&ndb_cond->cond, NULL);
#endif
  assert(result==0);
  return result;
}


struct NdbCondition*
NdbCondition_Create(void)
{
  struct NdbCondition* tmpCond;

  tmpCond = (struct NdbCondition*)NdbMem_Allocate(sizeof(struct NdbCondition));

  if (tmpCond == NULL)
    return NULL;

  (void)NdbCondition_Init(tmpCond);
  return tmpCond;
}

int 
NdbCondition_Wait(struct NdbCondition* p_cond,
                  NdbMutex* p_mutex)
{
  int result;

  if (p_cond == NULL || p_mutex == NULL)
    return 1;
  
#ifdef NDB_MUTEX_STAT
  result = pthread_cond_wait(&p_cond->cond, &p_mutex->mutex);
#else
  result = pthread_cond_wait(&p_cond->cond, p_mutex);
#endif
  
  return result;
}

int 
NdbCondition_WaitTimeout(struct NdbCondition* p_cond,
                         NdbMutex* p_mutex,
                         int msecs)
{
  struct timespec abstime; 

  NdbCondition_ComputeAbsTime(&abstime, msecs);
  return NdbCondition_WaitTimeoutAbs(p_cond, p_mutex, &abstime);
}

void
NdbCondition_ComputeAbsTime(struct timespec * abstime, unsigned msecs)
{
  int secs = 0;
#ifndef NDB_WIN
#ifdef HAVE_CLOCK_GETTIME
  clock_gettime(clock_id, abstime);
#else
  {
    struct timeval tick_time;
    gettimeofday(&tick_time, 0);
    abstime->tv_sec  = tick_time.tv_sec;
    abstime->tv_nsec = tick_time.tv_usec * 1000;
  }
#endif

  if(msecs >= 1000){
    secs  = msecs / 1000;
    msecs = msecs % 1000;
  }

  abstime->tv_sec  += secs;
  abstime->tv_nsec += msecs * 1000000;
  if (abstime->tv_nsec >= 1000000000) {
    abstime->tv_sec  += 1;
    abstime->tv_nsec -= 1000000000;
  }
#else
  set_timespec_nsec(*abstime,msecs*1000000ULL);
#endif
}

int
NdbCondition_WaitTimeoutAbs(struct NdbCondition* p_cond,
                            NdbMutex* p_mutex,
                            const struct timespec * abstime)
{
#ifdef NDB_WIN
  struct timespec tmp = *abstime;
  abstime = &tmp;
#endif

  if (p_cond == NULL || p_mutex == NULL)
    return 1;

#ifdef NDB_MUTEX_STAT
  return pthread_cond_timedwait(&p_cond->cond, &p_mutex->mutex, abstime);
#else
  return pthread_cond_timedwait(&p_cond->cond, p_mutex, abstime);
#endif
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

