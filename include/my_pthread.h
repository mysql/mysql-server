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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/* Defines to make different thread packages compatible */

#ifndef _my_pthread_h
#define _my_pthread_h

#include "my_global.h"                          /* myf */

#ifndef ETIME
#define ETIME ETIMEDOUT				/* For FreeBSD */
#endif

#ifdef  __cplusplus
#define EXTERNC extern "C"
extern "C" {
#else
#define EXTERNC
#endif /* __cplusplus */ 

#if defined(_WIN32)
typedef DWORD		 pthread_t;
#define pthread_self() GetCurrentThreadId()
#define pthread_handler_t EXTERNC void * __cdecl
#define pthread_equal(A,B) ((A) == (B))
#endif

/*
  Ideally we should make mysql_thread.h, my_pthread.h and the following 3
  header files self contained and include them where they are needed and
  not "everywhere".
*/
#include "thr_mutex.h"
#include "thr_cond.h"
#include "thr_rwlock.h"

#if defined(_WIN32)
/*
  Existing mysql_thread_create() or pthread_create() does not work well
  in windows platform when threads are joined because
  A)during thread creation, thread handle is not stored.
  B)during thread join, thread handle is retrieved using OpenThread().
    OpenThread() does not behave properly when thread to be joined is already
    exited.
  Use pthread_create_get_handle() and pthread_join_with_handle() function
  instead of mysql_thread_create() function for windows joinable threads.
*/

typedef struct thread_attr {
    DWORD dwStackSize ;
    DWORD dwCreatingFlag ;
} pthread_attr_t ;
typedef void * (__cdecl *pthread_handler)(void *);

int pthread_create(pthread_t *, const pthread_attr_t *, pthread_handler, void *);
int pthread_attr_init(pthread_attr_t *connect_att);
int pthread_attr_setstacksize(pthread_attr_t *connect_att, size_t stack);
int pthread_attr_getstacksize(pthread_attr_t *connect_att, size_t *stack);
int pthread_attr_destroy(pthread_attr_t *connect_att);

typedef volatile LONG my_pthread_once_t;
#define MY_PTHREAD_ONCE_INIT  0
#define MY_PTHREAD_ONCE_INPROGRESS 1
#define MY_PTHREAD_ONCE_DONE 2

int my_pthread_once(my_pthread_once_t *once_control,void (*init_routine)(void));

/**
  Create thread.

  Existing mysql_thread_create does not work well in windows platform
  when threads are joined. Use pthread_create_get_handle() and
  pthread_join_with_handle() function instead of mysql_thread_create()
  function for windows.

  @param thread_id    reference to pthread object
  @param attr         reference to pthread attribute
  @param func         pthread handler function
  @param param        parameters to pass to newly created thread
  @param out_handle   output parameter to get newly created thread handle

  @return int
    @retval 0 success
    @retval 1 failure
*/
int pthread_create_get_handle(pthread_t *thread_id,
                              const pthread_attr_t *attr,
                              pthread_handler func, void *param,
                              HANDLE *out_handle);

/**
  Get thread HANDLE.
  @param thread      reference to pthread object
  @return int
    @retval !NULL    valid thread handle
    @retval NULL     failure
*/
HANDLE pthread_get_handle(pthread_t thread_id);

/**
  Wait for thread termination.

  @param handle       handle of the thread to wait for

  @return  int
    @retval 0 success
    @retval 1 failure
*/
int pthread_join_with_handle(HANDLE handle);

void pthread_exit(void *a);

/*
  Existing pthread_join() does not work well in windows platform when
  threads are joined because
  A)during thread creation thread handle is not stored.
  B)during thread join, thread handle is retrieved using OpenThread().
    OpenThread() does not behave properly when thread to be joined is already
    exited.

  Use pthread_create_get_handle() and pthread_join_with_handle()
  function instead for windows joinable threads.
*/
int pthread_join(pthread_t thread, void **value_ptr);
int pthread_cancel(pthread_t thread);
extern int pthread_dummy(int);

#ifndef ETIMEDOUT
#define ETIMEDOUT 145		    /* Win32 doesn't have this */
#endif

#define pthread_key(T,V)  DWORD V
#define pthread_key_create(A,B) ((*A=TlsAlloc())==0xFFFFFFFF)
#define pthread_key_delete(A) TlsFree(A)
#define my_pthread_setspecific_ptr(T,V) (!TlsSetValue((T),(V)))
#define pthread_setspecific(A,B) (!TlsSetValue((A),(B)))
#define pthread_getspecific(A) (TlsGetValue(A))
#define my_pthread_getspecific(T,A) ((T) TlsGetValue(A))
#define my_pthread_getspecific_ptr(T,V) ((T) TlsGetValue(V))

#define pthread_kill(A,B) pthread_dummy((A) ? 0 : ESRCH)

static inline int pthread_attr_getguardsize(pthread_attr_t *attr,
                                            size_t *guardsize)
{
  *guardsize= 0;
  return 0;
}

/* Dummy defines for easier code */
#define pthread_attr_setdetachstate(A,B) pthread_dummy(0)
#define pthread_attr_setscope(A,B) pthread_dummy(0)
#define pthread_yield() SwitchToThread()

#else /* Normal threads */

#include <pthread.h>
#include <sched.h>
#ifdef HAVE_SYNCH_H
#include <synch.h>
#endif

#define pthread_key(T,V) pthread_key_t V
#define my_pthread_getspecific_ptr(T,V) my_pthread_getspecific(T,(V))
#define my_pthread_setspecific_ptr(T,V) pthread_setspecific(T,(void*) (V))
#define pthread_handler_t EXTERNC void *
typedef void *(* pthread_handler)(void *);

#define my_pthread_once_t pthread_once_t
#define MY_PTHREAD_ONCE_INIT PTHREAD_ONCE_INIT
#define my_pthread_once(C,F) pthread_once(C,F)
#define my_pthread_getspecific(A,B) ((A) pthread_getspecific(B))

#if !defined(HAVE_PTHREAD_YIELD_ZERO_ARG)
/* no pthread_yield() available */
#define pthread_yield() sched_yield()
#endif

#endif /* defined(_WIN32) */

/* Define mutex types, see my_thr_init.c */
#define MY_MUTEX_INIT_SLOW   NULL
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
extern native_mutexattr_t my_fast_mutexattr;
#define MY_MUTEX_INIT_FAST &my_fast_mutexattr
#else
#define MY_MUTEX_INIT_FAST   NULL
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
extern native_mutexattr_t my_errorcheck_mutexattr;
#define MY_MUTEX_INIT_ERRCHK &my_errorcheck_mutexattr
#else
#define MY_MUTEX_INIT_ERRCHK   NULL
#endif

#ifndef ESRCH
/* Define it to something */
#define ESRCH 1
#endif

typedef ulong my_thread_id;

extern my_bool my_thread_global_init(void);
extern void my_thread_global_reinit(void);
extern void my_thread_global_end(void);
extern my_bool my_thread_init(void);
extern void my_thread_end(void);
extern const char *my_thread_name(void);
extern my_thread_id my_thread_dbug_id(void);

#ifndef DEFAULT_THREAD_STACK
#if SIZEOF_CHARP > 4
/*
  MySQL can survive with 32K, but some glibc libraries require > 128K stack
  To resolve hostnames. Also recursive stored procedures needs stack.
*/
#define DEFAULT_THREAD_STACK	(256*1024L)
#else
#define DEFAULT_THREAD_STACK	(192*1024)
#endif
#endif

#ifdef MYSQL_SERVER
#ifndef MYSQL_DYNAMIC_PLUGIN
#include <pfs_thread_provider.h>
#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVER */

#include <mysql/psi/mysql_thread.h>

/* All thread specific variables are in the following struct */
#define THREAD_NAME_SIZE 10
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
  my_bool init;
  struct st_my_thread_var *next,**prev;
  void *opt_info;
  void  *stack_ends_here;
#ifndef DBUG_OFF
  void *dbug;
  char name[THREAD_NAME_SIZE+1];
#endif
};

extern struct st_my_thread_var *_my_thread_var(void) __attribute__ ((const));
extern int set_mysys_var(struct st_my_thread_var *mysys_var);
extern void **my_thread_var_dbug();
#define my_thread_var (_my_thread_var())
#define my_errno my_thread_var->thr_errno

#if defined(_WIN32)
#define my_winerr my_thread_var->thr_winerr
#endif

#ifdef  __cplusplus
}
#endif
#endif /* _my_ptread_h */
