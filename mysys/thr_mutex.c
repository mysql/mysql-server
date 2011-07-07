/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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

/* This makes a wrapper for mutex handling to make it easier to debug mutex */

#include <my_global.h>
#if defined(TARGET_OS_LINUX) && !defined (__USE_UNIX98)
#define __USE_UNIX98			/* To get rw locks under Linux */
#endif
#if defined(SAFE_MUTEX)
#undef SAFE_MUTEX			/* Avoid safe_mutex redefinitions */
#include "mysys_priv.h"
#include "my_static.h"
#include <m_string.h>

#ifndef DO_NOT_REMOVE_THREAD_WRAPPERS
/* Remove wrappers */
#undef pthread_mutex_t
#undef pthread_mutex_init
#undef pthread_mutex_lock
#undef pthread_mutex_unlock
#undef pthread_mutex_destroy
#undef pthread_cond_wait
#undef pthread_cond_timedwait
#ifdef HAVE_NONPOSIX_PTHREAD_MUTEX_INIT
#define pthread_mutex_init(a,b) my_pthread_mutex_init((a),(b))
#endif
#endif /* DO_NOT_REMOVE_THREAD_WRAPPERS */

/* Not instrumented */
static pthread_mutex_t THR_LOCK_mutex;
static ulong safe_mutex_count= 0;		/* Number of mutexes created */
#ifdef SAFE_MUTEX_DETECT_DESTROY
static struct st_safe_mutex_info_t *safe_mutex_root= NULL;
#endif

void safe_mutex_global_init(void)
{
  pthread_mutex_init(&THR_LOCK_mutex,MY_MUTEX_INIT_FAST);
}


int safe_mutex_init(safe_mutex_t *mp,
		    const pthread_mutexattr_t *attr __attribute__((unused)),
		    const char *file,
		    uint line)
{
  bzero((char*) mp,sizeof(*mp));
  pthread_mutex_init(&mp->global,MY_MUTEX_INIT_ERRCHK);
  pthread_mutex_init(&mp->mutex,attr);
  /* Mark that mutex is initialized */
  mp->file= file;
  mp->line= line;

#ifdef SAFE_MUTEX_DETECT_DESTROY
  /*
    Monitor the freeing of mutexes.  This code depends on single thread init
    and destroy
  */
  if ((mp->info= (safe_mutex_info_t *) malloc(sizeof(safe_mutex_info_t))))
  {
    struct st_safe_mutex_info_t *info =mp->info;

    info->init_file= file;
    info->init_line= line;
    info->prev= NULL;
    info->next= NULL;

    pthread_mutex_lock(&THR_LOCK_mutex);
    if ((info->next= safe_mutex_root))
      safe_mutex_root->prev= info;
    safe_mutex_root= info;
    safe_mutex_count++;
    pthread_mutex_unlock(&THR_LOCK_mutex);
  }
#else
  pthread_mutex_lock(&THR_LOCK_mutex);
  safe_mutex_count++;
  pthread_mutex_unlock(&THR_LOCK_mutex);
#endif /* SAFE_MUTEX_DETECT_DESTROY */
  return 0;
}


int safe_mutex_lock(safe_mutex_t *mp, my_bool try_lock, const char *file, uint line)
{
  int error;
  if (!mp->file)
  {
    fprintf(stderr,
	    "safe_mutex: Trying to lock unitialized mutex at %s, line %d\n",
	    file, line);
    fflush(stderr);
    abort();
  }

  pthread_mutex_lock(&mp->global);
  if (mp->count > 0)
  {
    if (try_lock)
    {
      pthread_mutex_unlock(&mp->global);
      return EBUSY;
    }
    else if (pthread_equal(pthread_self(),mp->thread))
    {
      fprintf(stderr,
              "safe_mutex: Trying to lock mutex at %s, line %d, when the"
              " mutex was already locked at %s, line %d in thread %s\n",
              file,line,mp->file, mp->line, my_thread_name());
      fflush(stderr);
      abort();
    }
  }
  pthread_mutex_unlock(&mp->global);

  /*
    If we are imitating trylock(), we need to take special
    precautions.

    - We cannot use pthread_mutex_lock() only since another thread can
      overtake this thread and take the lock before this thread
      causing pthread_mutex_trylock() to hang. In this case, we should
      just return EBUSY. Hence, we use pthread_mutex_trylock() to be
      able to return immediately.

    - We cannot just use trylock() and continue execution below, since
      this would generate an error and abort execution if the thread
      was overtaken and trylock() returned EBUSY . In this case, we
      instead just return EBUSY, since this is the expected behaviour
      of trylock().
   */
  if (try_lock)
  {
    error= pthread_mutex_trylock(&mp->mutex);
    if (error == EBUSY)
      return error;
  }
  else
    error= pthread_mutex_lock(&mp->mutex);

  if (error || (error=pthread_mutex_lock(&mp->global)))
  {
    fprintf(stderr,"Got error %d when trying to lock mutex at %s, line %d\n",
	    error, file, line);
    fflush(stderr);
    abort();
  }
  mp->thread= pthread_self();
  if (mp->count++)
  {
    fprintf(stderr,"safe_mutex: Error in thread libray: Got mutex at %s, \
line %d more than 1 time\n", file,line);
    fflush(stderr);
    abort();
  }
  mp->file= file;
  mp->line=line;
  pthread_mutex_unlock(&mp->global);
  return error;
}


int safe_mutex_unlock(safe_mutex_t *mp,const char *file, uint line)
{
  int error;
  pthread_mutex_lock(&mp->global);
  if (mp->count == 0)
  {
    fprintf(stderr,"safe_mutex: Trying to unlock mutex that wasn't locked at %s, line %d\n            Last used at %s, line: %d\n",
	    file,line,mp->file ? mp->file : "",mp->line);
    fflush(stderr);
    abort();
  }
  if (!pthread_equal(pthread_self(),mp->thread))
  {
    fprintf(stderr,"safe_mutex: Trying to unlock mutex at %s, line %d  that was locked by another thread at: %s, line: %d\n",
	    file,line,mp->file,mp->line);
    fflush(stderr);
    abort();
  }
  mp->thread= 0;
  mp->count--;
#ifdef __WIN__
  pthread_mutex_unlock(&mp->mutex);
  error=0;
#else
  error=pthread_mutex_unlock(&mp->mutex);
  if (error)
  {
    fprintf(stderr,"safe_mutex: Got error: %d (%d) when trying to unlock mutex at %s, line %d\n", error, errno, file, line);
    fflush(stderr);
    abort();
  }
#endif /* __WIN__ */
  pthread_mutex_unlock(&mp->global);
  return error;
}


int safe_cond_wait(pthread_cond_t *cond, safe_mutex_t *mp, const char *file,
		   uint line)
{
  int error;
  pthread_mutex_lock(&mp->global);
  if (mp->count == 0)
  {
    fprintf(stderr,"safe_mutex: Trying to cond_wait on a unlocked mutex at %s, line %d\n",file,line);
    fflush(stderr);
    abort();
  }
  if (!pthread_equal(pthread_self(),mp->thread))
  {
    fprintf(stderr,"safe_mutex: Trying to cond_wait on a mutex at %s, line %d  that was locked by another thread at: %s, line: %d\n",
	    file,line,mp->file,mp->line);
    fflush(stderr);
    abort();
  }

  if (mp->count-- != 1)
  {
    fprintf(stderr,"safe_mutex:  Count was %d on locked mutex at %s, line %d\n",
	    mp->count+1, file, line);
    fflush(stderr);
    abort();
  }
  pthread_mutex_unlock(&mp->global);
  error=pthread_cond_wait(cond,&mp->mutex);
  pthread_mutex_lock(&mp->global);
  if (error)
  {
    fprintf(stderr,"safe_mutex: Got error: %d (%d) when doing a safe_mutex_wait at %s, line %d\n", error, errno, file, line);
    fflush(stderr);
    abort();
  }
  mp->thread=pthread_self();
  if (mp->count++)
  {
    fprintf(stderr,
	    "safe_mutex:  Count was %d in thread 0x%lx when locking mutex at %s, line %d\n",
	    mp->count-1, my_thread_dbug_id(), file, line);
    fflush(stderr);
    abort();
  }
  mp->file= file;
  mp->line=line;
  pthread_mutex_unlock(&mp->global);
  return error;
}


int safe_cond_timedwait(pthread_cond_t *cond, safe_mutex_t *mp,
                        const struct timespec *abstime,
                        const char *file, uint line)
{
  int error;
  pthread_mutex_lock(&mp->global);
  if (mp->count != 1 || !pthread_equal(pthread_self(),mp->thread))
  {
    fprintf(stderr,"safe_mutex: Trying to cond_wait at %s, line %d on a not hold mutex\n",file,line);
    fflush(stderr);
    abort();
  }
  mp->count--;					/* Mutex will be released */
  pthread_mutex_unlock(&mp->global);
  error=pthread_cond_timedwait(cond,&mp->mutex,abstime);
#ifdef EXTRA_DEBUG
  if (error && (error != EINTR && error != ETIMEDOUT && error != ETIME))
  {
    fprintf(stderr,"safe_mutex: Got error: %d (%d) when doing a safe_mutex_timedwait at %s, line %d\n", error, errno, file, line);
  }
#endif
  pthread_mutex_lock(&mp->global);
  mp->thread=pthread_self();
  if (mp->count++)
  {
    fprintf(stderr,
	    "safe_mutex:  Count was %d in thread 0x%lx when locking mutex at %s, line %d (error: %d (%d))\n",
	    mp->count-1, my_thread_dbug_id(), file, line, error, error);
    fflush(stderr);
    abort();
  }
  mp->file= file;
  mp->line=line;
  pthread_mutex_unlock(&mp->global);
  return error;
}


int safe_mutex_destroy(safe_mutex_t *mp, const char *file, uint line)
{
  int error=0;
  if (!mp->file)
  {
    fprintf(stderr,
	    "safe_mutex: Trying to destroy unitialized mutex at %s, line %d\n",
	    file, line);
    fflush(stderr);
    abort();
  }
  if (mp->count != 0)
  {
    fprintf(stderr,"safe_mutex: Trying to destroy a mutex that was locked at %s, line %d at %s, line %d\n",
	    mp->file,mp->line, file, line);
    fflush(stderr);
    abort();
  }
#ifdef __WIN__ 
  pthread_mutex_destroy(&mp->global);
  pthread_mutex_destroy(&mp->mutex);
#else
  if (pthread_mutex_destroy(&mp->global))
    error=1;
  if (pthread_mutex_destroy(&mp->mutex))
    error=1;
#endif
  mp->file= 0;					/* Mark destroyed */

#ifdef SAFE_MUTEX_DETECT_DESTROY
  if (mp->info)
  {
    struct st_safe_mutex_info_t *info= mp->info;
    pthread_mutex_lock(&THR_LOCK_mutex);

    if (info->prev)
      info->prev->next = info->next;
    else
      safe_mutex_root = info->next;
    if (info->next)
      info->next->prev = info->prev;
    safe_mutex_count--;

    pthread_mutex_unlock(&THR_LOCK_mutex);
    free(info);
    mp->info= NULL;				/* Get crash if double free */
  }
#else
  pthread_mutex_lock(&THR_LOCK_mutex);
  safe_mutex_count--;
  pthread_mutex_unlock(&THR_LOCK_mutex);
#endif /* SAFE_MUTEX_DETECT_DESTROY */
  return error;
}


/*
  Free global resources and check that all mutex has been destroyed

  SYNOPSIS
    safe_mutex_end()
    file		Print errors on this file

  NOTES
    We can't use DBUG_PRINT() here as we have in my_end() disabled
    DBUG handling before calling this function.

   In MySQL one may get one warning for a mutex created in my_thr_init.c
   This is ok, as this thread may not yet have been exited.
*/

void safe_mutex_end(FILE *file __attribute__((unused)))
{
  if (!safe_mutex_count)			/* safetly */
    pthread_mutex_destroy(&THR_LOCK_mutex);
#ifdef SAFE_MUTEX_DETECT_DESTROY
  if (!file)
    return;

  if (safe_mutex_count)
  {
    fprintf(file, "Warning: Not destroyed mutex: %lu\n", safe_mutex_count);
    (void) fflush(file);
  }
  {
    struct st_safe_mutex_info_t *ptr;
    for (ptr= safe_mutex_root ; ptr ; ptr= ptr->next)
    {
      fprintf(file, "\tMutex initiated at line %4u in '%s'\n",
	      ptr->init_line, ptr->init_file);
      (void) fflush(file);
    }
  }
#endif /* SAFE_MUTEX_DETECT_DESTROY */
}

#endif /* SAFE_MUTEX */

#if defined(MY_PTHREAD_FASTMUTEX) && !defined(SAFE_MUTEX)

#include "mysys_priv.h"
#include "my_static.h"
#include <m_string.h>

#include <m_ctype.h>
#include <hash.h>
#include <myisampack.h>
#include <mysys_err.h>
#include <my_sys.h>

#undef pthread_mutex_t
#undef pthread_mutex_init
#undef pthread_mutex_lock
#undef pthread_mutex_trylock
#undef pthread_mutex_unlock
#undef pthread_mutex_destroy
#undef pthread_cond_wait
#undef pthread_cond_timedwait

ulong mutex_delay(ulong delayloops)
{
  ulong	i;
  volatile ulong j;

  j = 0;

  for (i = 0; i < delayloops * 50; i++)
    j += i;

  return(j); 
}	

#define MY_PTHREAD_FASTMUTEX_SPINS 8
#define MY_PTHREAD_FASTMUTEX_DELAY 4

static int cpu_count= 0;

int my_pthread_fastmutex_init(my_pthread_fastmutex_t *mp,
                              const pthread_mutexattr_t *attr)
{
  if ((cpu_count > 1) && (attr == MY_MUTEX_INIT_FAST))
    mp->spins= MY_PTHREAD_FASTMUTEX_SPINS; 
  else
    mp->spins= 0;
  mp->rng_state= 1;
  return pthread_mutex_init(&mp->mutex, attr); 
}

/**
  Park-Miller random number generator. A simple linear congruential
  generator that operates in multiplicative group of integers modulo n.

  x_{k+1} = (x_k g) mod n

  Popular pair of parameters: n = 2^32 − 5 = 4294967291 and g = 279470273.
  The period of the generator is about 2^31.
  Largest value that can be returned: 2147483646 (RAND_MAX)

  Reference:

  S. K. Park and K. W. Miller
  "Random number generators: good ones are hard to find"
  Commun. ACM, October 1988, Volume 31, No 10, pages 1192-1201.
*/

static double park_rng(my_pthread_fastmutex_t *mp)
{
  mp->rng_state= ((my_ulonglong)mp->rng_state * 279470273U) % 4294967291U;
  return (mp->rng_state / 2147483647.0);
}

int my_pthread_fastmutex_lock(my_pthread_fastmutex_t *mp)
{
  int   res;
  uint  i;
  uint  maxdelay= MY_PTHREAD_FASTMUTEX_DELAY;

  for (i= 0; i < mp->spins; i++)
  {
    res= pthread_mutex_trylock(&mp->mutex);

    if (res == 0)
      return 0;

    if (res != EBUSY)
      return res;

    mutex_delay(maxdelay);
    maxdelay += park_rng(mp) * MY_PTHREAD_FASTMUTEX_DELAY + 1;
  }
  return pthread_mutex_lock(&mp->mutex);
}


void fastmutex_global_init(void)
{
#ifdef _SC_NPROCESSORS_CONF
  cpu_count= sysconf(_SC_NPROCESSORS_CONF);
#endif
}
  
#endif /* defined(MY_PTHREAD_FASTMUTEX) && !defined(SAFE_MUTEX) */ 
