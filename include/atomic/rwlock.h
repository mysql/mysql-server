#ifndef ATOMIC_RWLOCK_INCLUDED
#define ATOMIC_RWLOCK_INCLUDED

/* Copyright (c) 2006, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "my_pthread.h"

typedef struct {pthread_mutex_t rw;} my_atomic_rwlock_t;

/*
  we're using read-write lock macros but map them to mutex locks, and they're
  faster. Still, having semantically rich API we can change the
  underlying implementation, if necessary.
*/
#define my_atomic_rwlock_destroy(name)     pthread_mutex_destroy(& (name)->rw)
#define my_atomic_rwlock_init(name)        pthread_mutex_init(& (name)->rw, 0)
#define my_atomic_rwlock_rdlock(name)      pthread_mutex_lock(& (name)->rw)
#define my_atomic_rwlock_wrlock(name)      pthread_mutex_lock(& (name)->rw)
#define my_atomic_rwlock_rdunlock(name)    pthread_mutex_unlock(& (name)->rw)
#define my_atomic_rwlock_wrunlock(name)    pthread_mutex_unlock(& (name)->rw)

static inline int my_atomic_cas32(int32 volatile *a, int32 *cmp, int32 set)
{
  int ret= (*a == *cmp);
  if (ret)
    *a= set;
  else
    *cmp=*a;
  return ret;
}

static inline int my_atomic_cas64(int64 volatile *a, int64 *cmp, int64 set)
{
  int ret= (*a == *cmp);
  if (ret)
    *a= set;
  else
    *cmp=*a;
  return ret;
}

static inline int my_atomic_casptr(void * volatile *a, void **cmp, void *set)
{
  int ret= (*a == *cmp);
  if (ret)
    *a= set;
  else
    *cmp=*a;
  return ret;
}

static inline int32 my_atomic_add32(int32 volatile *a, int32 v)
{
  int32 sav= *a;
  *a+= v;
  v= sav;
  return v;
}

static inline int64 my_atomic_add64(int64 volatile *a, int64 v)
{
  int64 sav= *a;
  *a+= v;
  v= sav;
  return v;
}

static inline int32 my_atomic_fas32(int32 volatile *a, int32 v)
{
  int32 sav= *a;
  *a= v;
  v= sav;
  return v;
}

static inline int64 my_atomic_fas64(int64 volatile *a, int64 v)
{
  int64 sav= *a;
  *a= v;
  v= sav;
  return v;
}

static inline int32 my_atomic_load32(int32 volatile *a)
{
  return *a;
}

static inline int64 my_atomic_load64(int64 volatile *a)
{
  return *a;
}

static inline void my_atomic_store32(int32 volatile *a, int32 v)
{
  *a= v;
}

static inline void my_atomic_store64(int64 volatile *a, int64 v)
{
  *a= v;
}

static inline void my_atomic_storeptr(void * volatile *a, void *v)
{
  *a= v;
}

#endif /* ATOMIC_RWLOCK_INCLUDED */
