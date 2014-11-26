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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef MY_THREAD_LOCAL_INCLUDED
#define MY_THREAD_LOCAL_INCLUDED

C_MODE_START

#ifdef _WIN32
typedef DWORD thread_local_key_t;
#else
typedef pthread_key_t thread_local_key_t;
#endif

static inline int my_create_thread_local_key(thread_local_key_t *key,
                                             void (*destructor)(void *))
{
#ifdef _WIN32
  *key= TlsAlloc();
  return (*key == TLS_OUT_OF_INDEXES);
#else
  return pthread_key_create(key, destructor);
#endif
}

static inline int my_delete_thread_local_key(thread_local_key_t key)
{
#ifdef _WIN32
  return !TlsFree(key);
#else
  return pthread_key_delete(key);
#endif
}

static inline void* my_get_thread_local(thread_local_key_t key)
{
#ifdef _WIN32
  return TlsGetValue(key);
#else
  return pthread_getspecific(key);
#endif
}

static inline int my_set_thread_local(thread_local_key_t key,
                                      void *value)
{
#ifdef _WIN32
  return !TlsSetValue(key, value);
#else
  return pthread_setspecific(key, value);
#endif
}


/* All thread specific variables are in the following struct */
struct st_my_thread_var
{
  int thr_errno;
#if defined(_WIN32)
/*
  thr_winerr is used for returning the original OS error-code in Windows,
  my_osmaperr() returns EINVAL for all unknown Windows errors, hence we
  preserve the original Windows Error code in thr_winerr.
*/
  int thr_winerr;
#endif
  mysql_cond_t suspend;
  mysql_mutex_t mutex;
  mysql_mutex_t * volatile current_mutex;
  mysql_cond_t * volatile current_cond;
  my_thread_id id;
  int volatile abort;
  struct st_my_thread_var *next,**prev;
  void *opt_info;
  void  *stack_ends_here;
#ifndef DBUG_OFF
  void *dbug;
#endif
};

struct st_my_thread_var *mysys_thread_var();

int set_mysys_thread_var(struct st_my_thread_var *mysys_var);

#ifndef DBUG_OFF
/* Return pointer to DBUG for holding current state */
void **my_thread_var_dbug();
#endif

#define my_errno mysys_thread_var()->thr_errno

C_MODE_END

#endif // MY_THREAD_LOCAL_INCLUDED
