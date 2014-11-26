/* Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include <my_global.h>
#include "my_pthread.h"

#if defined(SAFE_MUTEX)
/* This makes a wrapper for mutex handling to make it easier to debug mutex */

static my_bool safe_mutex_inited= FALSE;

/**
  While it looks like this function is pointless, it makes it possible to
  catch usage of global static mutexes. Since the order of construction of
  global objects in different compilation units is undefined, this is
  quite useful.
*/
void safe_mutex_global_init(void)
{
  safe_mutex_inited= TRUE;
}


int safe_mutex_init(my_mutex_t *mp, const native_mutexattr_t *attr,
		    const char *file, uint line)
{
  DBUG_ASSERT(safe_mutex_inited);
  memset(mp, 0, sizeof(*mp));
  native_mutex_init(&mp->global,MY_MUTEX_INIT_ERRCHK);
  native_mutex_init(&mp->mutex,attr);
  /* Mark that mutex is initialized */
  mp->file= file;
  mp->line= line;
  return 0;
}


int safe_mutex_lock(my_mutex_t *mp, my_bool try_lock,
                    const char *file, uint line)
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

  native_mutex_lock(&mp->global);
  if (mp->count > 0)
  {
    if (try_lock)
    {
      native_mutex_unlock(&mp->global);
      return EBUSY;
    }
    else if (pthread_equal(pthread_self(),mp->thread))
    {
      fprintf(stderr,
              "safe_mutex: Trying to lock mutex at %s, line %d, when the"
              " mutex was already locked at %s, line %d in thread T@%u\n",
              file,line,mp->file, mp->line, mysys_thread_var()->id);
      fflush(stderr);
      abort();
    }
  }
  native_mutex_unlock(&mp->global);

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
    error= native_mutex_trylock(&mp->mutex);
    if (error == EBUSY)
      return error;
  }
  else
    error= native_mutex_lock(&mp->mutex);

  if (error || (error=native_mutex_lock(&mp->global)))
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
  native_mutex_unlock(&mp->global);
  return error;
}


int safe_mutex_unlock(my_mutex_t *mp, const char *file, uint line)
{
  int error;
  native_mutex_lock(&mp->global);
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
  error=native_mutex_unlock(&mp->mutex);
  if (error)
  {
    fprintf(stderr,"safe_mutex: Got error: %d (%d) when trying to unlock mutex at %s, line %d\n", error, errno, file, line);
    fflush(stderr);
    abort();
  }
  native_mutex_unlock(&mp->global);
  return error;
}


int safe_mutex_destroy(my_mutex_t *mp, const char *file, uint line)
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
  if (native_mutex_destroy(&mp->global))
    error=1;
  if (native_mutex_destroy(&mp->mutex))
    error=1;
  mp->file= 0;					/* Mark destroyed */
  return error;
}

#elif defined(MY_PTHREAD_FASTMUTEX)

static ulong mutex_delay(ulong delayloops)
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

int my_pthread_fastmutex_init(my_mutex_t *mp,
                              const native_mutexattr_t *attr)
{
  if ((cpu_count > 1) && (attr == MY_MUTEX_INIT_FAST))
    mp->spins= MY_PTHREAD_FASTMUTEX_SPINS; 
  else
    mp->spins= 0;
  mp->rng_state= 1;
  return native_mutex_init(&mp->mutex, attr); 
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

static double park_rng(my_mutex_t *mp)
{
  mp->rng_state= ((my_ulonglong)mp->rng_state * 279470273U) % 4294967291U;
  return (mp->rng_state / 2147483647.0);
}

int my_pthread_fastmutex_lock(my_mutex_t *mp)
{
  int   res;
  uint  i;
  uint  maxdelay= MY_PTHREAD_FASTMUTEX_DELAY;

  for (i= 0; i < mp->spins; i++)
  {
    res= native_mutex_trylock(&mp->mutex);

    if (res == 0)
      return 0;

    if (res != EBUSY)
      return res;

    mutex_delay(maxdelay);
    maxdelay += park_rng(mp) * MY_PTHREAD_FASTMUTEX_DELAY + 1;
  }
  return native_mutex_lock(&mp->mutex);
}


void fastmutex_global_init(void)
{
#ifdef _SC_NPROCESSORS_CONF
  cpu_count= sysconf(_SC_NPROCESSORS_CONF);
#endif
}

#endif /* defined(MY_PTHREAD_FASTMUTEX) && !defined(SAFE_MUTEX) */
