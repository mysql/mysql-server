/* Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

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
#include <my_sys.h>

#include <my_atomic.h>

/*
  checks that the current build of atomic ops
  can run on this machine

  RETURN
    ATOMIC_xxx values, see my_atomic.h
*/
int my_atomic_initialize()
{
  compile_time_assert(sizeof(intptr) == sizeof(void *));
  /* currently the only thing worth checking is SMP/UP issue */
#ifdef MY_ATOMIC_MODE_DUMMY
  return my_getncpus() == 1 ? MY_ATOMIC_OK : MY_ATOMIC_NOT_1CPU;
#else
  return MY_ATOMIC_OK;
#endif
}

#ifdef SAFE_MUTEX
#undef pthread_mutex_init
#undef pthread_mutex_destroy
#undef pthread_mutex_lock
#undef pthread_mutex_unlock

void plain_pthread_mutex_init(safe_mutex_t *m)
{
  pthread_mutex_init(& m->mutex, NULL);
}

void plain_pthread_mutex_destroy(safe_mutex_t *m)
{
  pthread_mutex_destroy(& m->mutex);
}

void plain_pthread_mutex_lock(safe_mutex_t *m)
{
  pthread_mutex_lock(& m->mutex);
}

void plain_pthread_mutex_unlock(safe_mutex_t *m)
{
  pthread_mutex_unlock(& m->mutex);
}

#endif


