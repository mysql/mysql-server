/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "thr_mutex.h"
#include "my_thread_local.h"

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
  native_mutex_lock(&mp->global);
  if (!mp->file)
  {
    native_mutex_unlock(&mp->global);
    fprintf(stderr,
	    "safe_mutex: Trying to lock uninitialized mutex at %s, line %d\n",
	    file, line);
    fflush(stderr);
    abort();
  }

  if (mp->count > 0)
  {
    if (try_lock)
    {
      native_mutex_unlock(&mp->global);
      return EBUSY;
    }
    else if (my_thread_equal(my_thread_self(),mp->thread))
    {
#ifndef DBUG_OFF
      fprintf(stderr,
              "safe_mutex: Trying to lock mutex at %s, line %d, when the"
              " mutex was already locked at %s, line %d in thread T@%u\n",
              file, line, mp->file, mp->line, my_thread_var_id());
      fflush(stderr);
#endif
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
  mp->thread= my_thread_self();
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
  if (!my_thread_equal(my_thread_self(),mp->thread))
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
  native_mutex_lock(&mp->global);
  if (!mp->file)
  {
    native_mutex_unlock(&mp->global);
    fprintf(stderr,
	    "safe_mutex: Trying to destroy uninitialized mutex at %s, line %d\n",
	    file, line);
    fflush(stderr);
    abort();
  }
  if (mp->count != 0)
  {
    native_mutex_unlock(&mp->global);
    fprintf(stderr,"safe_mutex: Trying to destroy a mutex that was locked at %s, line %d at %s, line %d\n",
	    mp->file,mp->line, file, line);
    fflush(stderr);
    abort();
  }
  native_mutex_unlock(&mp->global);
  if (native_mutex_destroy(&mp->global))
    error=1;
  if (native_mutex_destroy(&mp->mutex))
    error=1;
  mp->file= 0;					/* Mark destroyed */
  return error;
}

#endif /* SAFE_MUTEX */
