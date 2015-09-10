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

#ifdef SAFE_MUTEX

#include "thr_cond.h"
#include "my_thread_local.h"

int safe_cond_wait(native_cond_t *cond, my_mutex_t *mp,
                   const char *file, uint line)
{
  int error;
  native_mutex_lock(&mp->global);
  if (mp->count == 0)
  {
    fprintf(stderr,"safe_mutex: Trying to cond_wait on a unlocked mutex at %s, line %d\n",file,line);
    fflush(stderr);
    abort();
  }
  if (!my_thread_equal(my_thread_self(),mp->thread))
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
  native_mutex_unlock(&mp->global);
  error= native_cond_wait(cond,&mp->mutex);
  native_mutex_lock(&mp->global);
  if (error)
  {
    fprintf(stderr,"safe_mutex: Got error: %d (%d) when doing a safe_mutex_wait at %s, line %d\n", error, errno, file, line);
    fflush(stderr);
    abort();
  }
  mp->thread= my_thread_self();
  if (mp->count++)
  {
#ifndef DBUG_OFF
    fprintf(stderr,
	    "safe_mutex:  Count was %d in thread 0x%x when locking mutex at %s, line %d\n",
	    mp->count-1, my_thread_var_id(), file, line);
    fflush(stderr);
#endif
    abort();
  }
  mp->file= file;
  mp->line=line;
  native_mutex_unlock(&mp->global);
  return error;
}


int safe_cond_timedwait(native_cond_t *cond, my_mutex_t *mp,
                        const struct timespec *abstime,
                        const char *file, uint line)
{
  int error;
  native_mutex_lock(&mp->global);
  if (mp->count != 1 || !my_thread_equal(my_thread_self(),mp->thread))
  {
    fprintf(stderr,"safe_mutex: Trying to cond_wait at %s, line %d on a not hold mutex\n",file,line);
    fflush(stderr);
    abort();
  }
  mp->count--;					/* Mutex will be released */
  native_mutex_unlock(&mp->global);
  error= native_cond_timedwait(cond,&mp->mutex,abstime);
#ifdef EXTRA_DEBUG
  if (error && (error != EINTR && error != ETIMEDOUT && error != ETIME))
  {
    fprintf(stderr,"safe_mutex: Got error: %d (%d) when doing a safe_cond_timedwait at %s, line %d\n", error, errno, file, line);
  }
#endif
  native_mutex_lock(&mp->global);
  mp->thread= my_thread_self();
  if (mp->count++)
  {
#ifndef DBUG_OFF
    fprintf(stderr,
	    "safe_mutex:  Count was %d in thread 0x%x when locking mutex at %s, line %d (error: %d (%d))\n",
	    mp->count-1, my_thread_var_id(), file, line, error, error);
    fflush(stderr);
#endif
    abort();
  }
  mp->file= file;
  mp->line=line;
  native_mutex_unlock(&mp->global);
  return error;
}

#endif
