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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/* Defines to make different thread packages compatible */

#ifndef MY_THREAD_INCLUDED
#define MY_THREAD_INCLUDED

#include "my_global.h"              /* my_bool */

#if !defined(_WIN32)
#include <pthread.h>
#endif

#ifndef ETIME
#define ETIME ETIMEDOUT             /* For FreeBSD */
#endif

#ifndef ETIMEDOUT
#define ETIMEDOUT 145		    /* Win32 doesn't have this */
#endif

/*
  MySQL can survive with 32K, but some glibc libraries require > 128K stack
  To resolve hostnames. Also recursive stored procedures needs stack.
*/
#if SIZEOF_CHARP > 4
#define DEFAULT_THREAD_STACK	(256*1024L)
#else
#define DEFAULT_THREAD_STACK	(192*1024)
#endif

#ifdef  __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

C_MODE_START

#ifdef _WIN32
typedef volatile LONG    my_thread_once_t;
typedef DWORD            my_thread_t;
typedef struct thread_attr
  { DWORD dwStackSize; } my_thread_attr_t;
typedef void * (__cdecl *my_start_routine)(void *);
#define MY_THREAD_ONCE_INIT       0
#define MY_THREAD_ONCE_INPROGRESS 1
#define MY_THREAD_ONCE_DONE       2
#else
typedef pthread_once_t   my_thread_once_t;
typedef pthread_t        my_thread_t;
typedef pthread_attr_t   my_thread_attr_t;
typedef void *(* my_start_routine)(void *);
#define MY_THREAD_ONCE_INIT       PTHREAD_ONCE_INIT
#endif

typedef struct st_my_thread_handle
{
  my_thread_t thread;
#ifdef _WIN32
  HANDLE handle;
#endif
} my_thread_handle;

int my_thread_once(my_thread_once_t *once_control, void (*init_routine)(void));

static inline my_thread_t my_thread_self()
{
#ifdef _WIN32
  return GetCurrentThreadId();
#else
  return pthread_self();
#endif
}

static inline int my_thread_equal(my_thread_t t1, my_thread_t t2)
{
#ifdef _WIN32
  return t1 == t2;
#else
  return pthread_equal(t1, t2);
#endif
}

static inline int my_thread_attr_init(my_thread_attr_t *attr)
{
#ifdef _WIN32
  attr->dwStackSize= 0;
  return 0;
#else
  return pthread_attr_init(attr);
#endif
}

static inline int my_thread_attr_destroy(my_thread_attr_t *attr)
{
#ifdef _WIN32
  memset(attr, 0, sizeof(*attr));
  return 0;
#else
  return pthread_attr_destroy(attr);
#endif
}

static inline int my_thread_attr_setstacksize(my_thread_attr_t *attr,
                                              size_t stacksize)
{
#ifdef _WIN32
  attr->dwStackSize= (DWORD)stacksize;
  return 0;
#else
  return pthread_attr_setstacksize(attr, stacksize);
#endif
}

static inline int my_thread_attr_getstacksize(my_thread_attr_t *attr,
                                              size_t *stacksize)
{
#ifdef _WIN32
  *stacksize= (size_t)attr->dwStackSize;
  return 0;
#else
  return pthread_attr_getstacksize(attr, stacksize);
#endif
}

static inline void my_thread_yield()
{
#ifdef _WIN32
  SwitchToThread();
#else
  sched_yield();
#endif
}

int my_thread_create(my_thread_handle *thread, const my_thread_attr_t *attr,
                     my_start_routine func, void *arg);
int my_thread_join(my_thread_handle *thread, void **value_ptr);
int my_thread_cancel(my_thread_handle *thread);
void my_thread_exit(void *value_ptr);


extern my_bool my_thread_global_init();
extern void my_thread_global_reinit();
extern void my_thread_global_end();
extern my_bool my_thread_init();
extern void my_thread_end();

C_MODE_END

#endif /* MY_THREAD_INCLUDED */
