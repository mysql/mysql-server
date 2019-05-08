/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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


#include <ndb_global.h>

#include <NdbMutex.h>

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
#include "NdbMutex_DeadlockDetector.h"
#endif

#ifdef NDB_MUTEX_STAT
static FILE * statout = 0;
#endif

#if defined NDB_MUTEX_STAT || defined NDB_MUTEX_DEADLOCK_DETECTOR
#define NDB_MUTEX_STRUCT
#endif

void
NdbMutex_SysInit()
{
#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  NdbMutex_DeadlockDetectorInit();
#endif
}

void
NdbMutex_SysEnd()
{
#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  NdbMutex_DeadlockDetectorEnd();
#endif
}

NdbMutex* NdbMutex_Create()
{
  return NdbMutex_CreateWithName(0);
}

NdbMutex* NdbMutex_CreateWithName(const char * name)
{
  NdbMutex* pNdbMutex;
  int result;

  pNdbMutex = (NdbMutex*)malloc(sizeof(NdbMutex));
  if (pNdbMutex == NULL)
    return NULL;

  result = NdbMutex_InitWithName(pNdbMutex, name);
  if (result == 0)
  {
    return pNdbMutex;
  }
  free(pNdbMutex);
  return 0;
}

static
int NdbMutex_InitWithName_local(NdbMutex* pNdbMutex,
                                const char * name,
                                Uint32 shared);

int NdbMutex_Init(NdbMutex* pNdbMutex)
{
  return NdbMutex_InitWithName_local(pNdbMutex, 0, 0);
}

int NdbMutex_InitWithName(NdbMutex* pNdbMutex, const char * name)
{
  return NdbMutex_InitWithName_local(pNdbMutex, name, 0);
}

int NdbMutex_Init_Shared(NdbMutex *pNdbMutex)
{
  return NdbMutex_InitWithName_local(pNdbMutex, 0, 1);
}

static
int NdbMutex_InitWithName_local(NdbMutex* pNdbMutex,
                                const char * name,
                                Uint32 shared)
{
  int result;
  native_mutex_t * p;
  DBUG_ENTER("NdbMutex_Init");
  (void)name;

#ifdef NDB_MUTEX_STRUCT
  bzero(pNdbMutex, sizeof(NdbMutex));
  p = &pNdbMutex->mutex;

#ifdef NDB_MUTEX_STAT
  pNdbMutex->min_lock_wait_time_ns = ~(Uint64)0;
  pNdbMutex->min_hold_time_ns = ~(Uint64)0;
  if (name == 0)
  {
    snprintf(pNdbMutex->name, sizeof(pNdbMutex->name), "%p",
             pNdbMutex);
  }
  else
  {
    snprintf(pNdbMutex->name, sizeof(pNdbMutex->name), "%p:%s",
             pNdbMutex, name);
  }
  if (getenv("NDB_MUTEX_STAT") != 0)
  {
    statout = stdout;
  }
#endif

#else
  p = pNdbMutex;
#endif

#if defined(VM_TRACE) && \
  defined(HAVE_PTHREAD_MUTEXATTR_INIT) && \
  defined(HAVE_PTHREAD_MUTEXATTR_SETTYPE) && \
  defined(HAVE_PTHREAD_MUTEXATTR_SETPSHARED)

  {
    pthread_mutexattr_t t;
    pthread_mutexattr_init(&t);
    pthread_mutexattr_settype(&t, PTHREAD_MUTEX_ERRORCHECK);
    if (shared)
      pthread_mutexattr_setpshared(&t, PTHREAD_PROCESS_SHARED);
    result = pthread_mutex_init(p, &t);
    assert(result == 0);
    pthread_mutexattr_destroy(&t);
  }
#else
#if defined(HAVE_PTHREAD_MUTEXATTR_INIT) && \
    defined(HAVE_PTHREAD_MUTEXATTR_SETPSHARED)
  {
    pthread_mutexattr_t t;
    pthread_mutexattr_init(&t);
    pthread_mutexattr_setpshared(&t, PTHREAD_PROCESS_SHARED);
    if (shared)
      result = pthread_mutex_init(p, &t);
    else
      result = pthread_mutex_init(p, 0);

    require(result == 0);
    pthread_mutexattr_destroy(&t);
  }
#else
    if (shared)
      result = 1;
    else 
      result = native_mutex_init(p, 0);
#endif
#endif

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  if (result == 0)
  {
    ndb_mutex_created(pNdbMutex);
  }
#endif
  DBUG_RETURN(result);
}

int NdbMutex_Deinit(NdbMutex* p_mutex)
{
  int result;

  if (p_mutex == NULL)
    return -1;

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  ndb_mutex_destoyed(p_mutex);
#endif

#ifdef NDB_MUTEX_STRUCT
  result = pthread_mutex_destroy(&p_mutex->mutex);
#else
  result = native_mutex_destroy(p_mutex);
#endif

  return result;
}

int NdbMutex_Destroy(NdbMutex* p_mutex)
{
  int result;

  if (p_mutex == NULL)
    return -1;
  result = NdbMutex_Deinit(p_mutex);
  memset(p_mutex, 0xff, sizeof(NdbMutex));
  free(p_mutex);
  return result;
}

#ifdef NDB_MUTEX_STAT
static
inline
Uint64
now()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static
void
dumpstat(NdbMutex* p)
{
  if (statout != 0)
  {
    fprintf(statout,
            "%s : "
            " lock [ cnt: %u con: %u wait: [ min: %llu avg: %llu max: %llu ] ]"
            " trylock [ ok: %u nok: %u ]"
            " hold: [ min: %llu avg: %llu max: %llu ]\n",
            p->name,
            p->cnt_lock,
            p->cnt_lock_contention,
            p->min_lock_wait_time_ns,
            p->cnt_lock_contention ?
            p->sum_lock_wait_time_ns / p->cnt_lock_contention : 0,
            p->max_lock_wait_time_ns,
            p->cnt_trylock_ok,
            p->cnt_trylock_nok,
            p->min_hold_time_ns,
            (p->cnt_lock + p->cnt_trylock_ok) ?
            p->sum_hold_time_ns / (p->cnt_lock + p->cnt_trylock_ok) : 0,
            p->max_hold_time_ns);
  }
  p->cnt_lock = 0;
  p->cnt_lock_contention = 0;
  p->cnt_trylock_ok = 0;
  p->cnt_trylock_nok = 0;
  p->min_lock_wait_time_ns = ~(Uint64)0;
  p->sum_lock_wait_time_ns = 0;
  p->max_lock_wait_time_ns = 0;
  p->min_hold_time_ns = ~(Uint64)0;
  p->sum_hold_time_ns = 0;
  p->max_hold_time_ns = 0;
}

#endif

int NdbMutex_Lock(NdbMutex* p_mutex)
{
  int result;

  if (p_mutex == NULL)
    return -1;

#ifdef NDB_MUTEX_STAT
  {
    Uint64 stop;
    if ((result = pthread_mutex_trylock(&p_mutex->mutex)) == 0)
    {
      stop = now();
    }
    else
    {
      Uint64 start = now();
      assert(result == EBUSY);
      result = pthread_mutex_lock(&p_mutex->mutex);
      stop = now();
      p_mutex->cnt_lock_contention++;
      Uint64 t = (stop - start);
      p_mutex->sum_lock_wait_time_ns += t;
      if (t < p_mutex->min_lock_wait_time_ns)
        p_mutex->min_lock_wait_time_ns = t;
      if (t > p_mutex->max_lock_wait_time_ns)
        p_mutex->max_lock_wait_time_ns = t;
    }
    p_mutex->cnt_lock++;
    p_mutex->lock_start_time_ns = stop;
  }
#elif defined NDB_MUTEX_STRUCT
  result = pthread_mutex_lock(&p_mutex->mutex);
#else
  result = native_mutex_lock(p_mutex);
#endif
  assert(result == 0);

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  ndb_mutex_locked(p_mutex);
#endif

  return result;
}


int NdbMutex_Unlock(NdbMutex* p_mutex)
{
  int result;

  if (p_mutex == NULL)
    return -1;

#ifdef NDB_MUTEX_STAT
  {
    Uint64 stop = now() - p_mutex->lock_start_time_ns;
    p_mutex->sum_hold_time_ns += stop;
    if (stop < p_mutex->min_hold_time_ns)
      p_mutex->min_hold_time_ns = stop;
    if (stop > p_mutex->max_hold_time_ns)
      p_mutex->max_hold_time_ns = stop;
    result = pthread_mutex_unlock(&p_mutex->mutex);
    if (((p_mutex->sum_hold_time_ns + p_mutex->sum_lock_wait_time_ns)
         >= 3*1000000000ULL) ||
        p_mutex->cnt_lock >= 16384 ||
        p_mutex->cnt_trylock_ok >= 16384)
    {
      dumpstat(p_mutex);
    }
  }
#elif defined NDB_MUTEX_STRUCT
  result = pthread_mutex_unlock(&p_mutex->mutex);
#else
  result = native_mutex_unlock(p_mutex);
#endif
  assert(result == 0);

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  ndb_mutex_unlocked(p_mutex);
#endif

  return result;
}


int NdbMutex_Trylock(NdbMutex* p_mutex)
{
  int result;

  if (p_mutex == NULL)
    return -1;

#ifdef NDB_MUTEX_STAT
  result = pthread_mutex_trylock(&p_mutex->mutex);
  if (result == 0)
  {
    p_mutex->cnt_trylock_ok++;
    p_mutex->lock_start_time_ns = now();
  }
  else
  {
    __sync_fetch_and_add(&p_mutex->cnt_trylock_nok, 1);
  }
#elif defined NDB_MUTEX_STRUCT
  result = pthread_mutex_trylock(&p_mutex->mutex);
#else
  result = native_mutex_trylock(p_mutex);
#endif
#ifndef NDEBUG
  if (result && result != EBUSY)
  {
    fprintf(stderr, "NdbMutex_TryLock, unexpected result %d returned from "
            "pthread_mutex_trylock: '%s'\n", result, strerror(result));
  }
#endif
  assert(result == 0 || result == EBUSY);

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  if (result == 0)
  {
    ndb_mutex_try_locked(p_mutex);
  }
#endif


  return result;
}

