#ifndef THR_MUTEX_INCLUDED
#define THR_MUTEX_INCLUDED

/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  MySQL mutex implementation.

  There are three "layers":
  1) native_mutex_*()
       Functions that map directly down to OS primitives.
       Windows    - CriticalSection
       Other OSes - pthread
  2) my_mutex_*()
       Functions that implement SAFE_MUTEX (default for debug),
       Otherwise native_mutex_*() is used.
  3) mysql_mutex_*()
       Functions that include Performance Schema instrumentation.
       See include/mysql/psi/mysql_thread.h
*/

#include <my_global.h>
#include "my_thread.h"

C_MODE_START

#ifdef _WIN32
typedef CRITICAL_SECTION native_mutex_t;
typedef int native_mutexattr_t;
#else
typedef pthread_mutex_t native_mutex_t;
typedef pthread_mutexattr_t native_mutexattr_t;
#endif

/* Define mutex types, see my_thr_init.c */
#define MY_MUTEX_INIT_SLOW   NULL

/* Can be set in /usr/include/pthread.h */
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
extern native_mutexattr_t my_fast_mutexattr;
#define MY_MUTEX_INIT_FAST &my_fast_mutexattr
#else
#define MY_MUTEX_INIT_FAST   NULL
#endif

/* Can be set in /usr/include/pthread.h */
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
extern native_mutexattr_t my_errorcheck_mutexattr;
#define MY_MUTEX_INIT_ERRCHK &my_errorcheck_mutexattr
#else
#define MY_MUTEX_INIT_ERRCHK   NULL
#endif

static inline int native_mutex_init(native_mutex_t *mutex,
                                    const native_mutexattr_t *attr)
{
#ifdef _WIN32
  InitializeCriticalSection(mutex);
  return 0;
#else
  return pthread_mutex_init(mutex, attr);
#endif
}

static inline int native_mutex_lock(native_mutex_t *mutex)
{
#ifdef _WIN32
  EnterCriticalSection(mutex);
  return 0;
#else
  return pthread_mutex_lock(mutex);
#endif
}

static inline int native_mutex_trylock(native_mutex_t *mutex)
{
#ifdef _WIN32
  if (TryEnterCriticalSection(mutex))
  {
    /* Don't allow recursive lock */
    if (mutex->RecursionCount > 1){
      LeaveCriticalSection(mutex);
      return EBUSY;
    }
    return 0;
  }
  return EBUSY;
#else
  return pthread_mutex_trylock(mutex);
#endif
}

static inline int native_mutex_unlock(native_mutex_t *mutex)
{
#ifdef _WIN32
  LeaveCriticalSection(mutex);
  return 0;
#else
  return pthread_mutex_unlock(mutex);
#endif
}

static inline int native_mutex_destroy(native_mutex_t *mutex)
{
#ifdef _WIN32
  DeleteCriticalSection(mutex);
  return 0;
#else
  return pthread_mutex_destroy(mutex);
#endif
}


#ifdef SAFE_MUTEX
/* safe_mutex adds checking to mutex for easier debugging */
typedef struct st_safe_mutex_t
{
  native_mutex_t global, mutex;
  const char *file;
  uint line, count;
  my_thread_t thread;
} my_mutex_t;

void safe_mutex_global_init();
int safe_mutex_init(my_mutex_t *mp, const native_mutexattr_t *attr,
                    const char *file, uint line);
int safe_mutex_lock(my_mutex_t *mp, my_bool try_lock, const char *file, uint line);
int safe_mutex_unlock(my_mutex_t *mp, const char *file, uint line);
int safe_mutex_destroy(my_mutex_t *mp, const char *file, uint line);

static inline void safe_mutex_assert_owner(const my_mutex_t *mp)
{
  DBUG_ASSERT(mp->count > 0 &&
              my_thread_equal(my_thread_self(), mp->thread));
}

static inline void safe_mutex_assert_not_owner(const my_mutex_t *mp)
{
  DBUG_ASSERT(!mp->count ||
              !my_thread_equal(my_thread_self(), mp->thread));
}

#else
typedef native_mutex_t my_mutex_t;
#endif

static inline int my_mutex_init(my_mutex_t *mp, const native_mutexattr_t *attr
#ifdef SAFE_MUTEX
                                , const char *file, uint line
#endif
                                )
{
#ifdef SAFE_MUTEX
  return safe_mutex_init(mp, attr, file, line);
#else
  return native_mutex_init(mp, attr);
#endif
}

static inline int my_mutex_lock(my_mutex_t *mp
#ifdef SAFE_MUTEX
                                , const char *file, uint line
#endif
                                )
{
#ifdef SAFE_MUTEX
  return safe_mutex_lock(mp, FALSE, file, line);
#else
  return native_mutex_lock(mp);
#endif
}

static inline int my_mutex_trylock(my_mutex_t *mp
#ifdef SAFE_MUTEX
                                   , const char *file, uint line
#endif
                                   )
{
#ifdef SAFE_MUTEX
  return safe_mutex_lock(mp, TRUE, file, line);
#else
  return native_mutex_trylock(mp);
#endif
}

static inline int my_mutex_unlock(my_mutex_t *mp
#ifdef SAFE_MUTEX
                                  , const char *file, uint line
#endif
                                  )
{
#ifdef SAFE_MUTEX
  return safe_mutex_unlock(mp, file, line);
#else
  return native_mutex_unlock(mp);
#endif
}

static inline int my_mutex_destroy(my_mutex_t *mp
#ifdef SAFE_MUTEX
                                   , const char *file, uint line
#endif
                                   )
{
#ifdef SAFE_MUTEX
  return safe_mutex_destroy(mp, file, line);
#else
  return native_mutex_destroy(mp);
#endif
}

C_MODE_END

#endif /* THR_MUTEX_INCLUDED */
