<<<<<<< HEAD
/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD:mysys/my_thr_init.cc
/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.
=======
/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
>>>>>>> pr/231

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_thr_init.cc
  Functions to handle initialization and allocation of all mysys & debug
  thread variables.
*/

<<<<<<< HEAD:mysys/my_thr_init.cc
#include <stdlib.h>
#include <sys/types.h>
=======
#include "mysys_priv.h"
#include "my_sys.h"
#include <m_string.h>
#include <signal.h>
#include "my_thread_local.h"

static my_bool THR_KEY_mysys_initialized= FALSE;
static my_bool my_thread_global_init_done= FALSE;
#ifndef NDEBUG
static uint    THR_thread_count= 0;
static uint    my_thread_end_wait_time= 5;
static my_thread_id thread_id= 0;
static thread_local_key_t THR_KEY_mysys;
#endif
static thread_local_key_t THR_KEY_myerrno;
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
#ifdef _WIN32
#include <signal.h>
#endif
#include <time.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_loglevel.h"
#include "my_macros.h"
#include "my_psi_config.h"
#include "my_sys.h"
#include "my_systime.h"
#include "my_thread.h"
#include "my_thread_local.h"
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/mysql_thread.h"
#include "mysql/psi/psi_thread.h"
#include "mysys/mysys_priv.h"
#include "mysys_err.h"
#include "thr_mutex.h"

static bool my_thread_global_init_done = false;
#ifndef NDEBUG
static uint THR_thread_count = 0;
static Timeout_type my_thread_end_wait_time = 5;
static my_thread_id thread_id = 0;
struct st_my_thread_var;
static thread_local st_my_thread_var *THR_mysys = nullptr;
#endif
static thread_local int THR_myerrno = 0;
#ifdef _WIN32
static thread_local int THR_winerrno = 0;
#endif

<<<<<<< HEAD:mysys/my_thr_init.cc
mysql_mutex_t THR_LOCK_myisam_mmap;
mysql_mutex_t THR_LOCK_myisam;
mysql_mutex_t THR_LOCK_heap;
mysql_mutex_t THR_LOCK_malloc;
mysql_mutex_t THR_LOCK_open;
mysql_mutex_t THR_LOCK_lock;
mysql_mutex_t THR_LOCK_net;
mysql_mutex_t THR_LOCK_charset;
<<<<<<< HEAD
#ifndef NDEBUG
=======
#ifndef DBUG_OFF
=======
mysql_mutex_t THR_LOCK_malloc, THR_LOCK_open,
              THR_LOCK_lock, THR_LOCK_myisam, THR_LOCK_heap,
              THR_LOCK_net, THR_LOCK_charset,
              THR_LOCK_myisam_mmap;
#ifndef NDEBUG
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
>>>>>>> pr/231
mysql_mutex_t THR_LOCK_threads;
mysql_cond_t THR_COND_threads;
#endif

#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
native_mutexattr_t my_fast_mutexattr;
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
native_mutexattr_t my_errorcheck_mutexattr;
#endif
#ifdef _WIN32
static void install_sigabrt_handler();
#endif

<<<<<<< HEAD
#ifndef NDEBUG
=======
<<<<<<< HEAD:mysys/my_thr_init.cc
#ifndef DBUG_OFF
>>>>>>> pr/231
struct st_my_thread_var {
=======
#ifndef NDEBUG
struct st_my_thread_var
{
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
  my_thread_id id;
  struct CODE_STATE *dbug;
};

<<<<<<< HEAD:mysys/my_thr_init.cc
static struct st_my_thread_var *mysys_thread_var() { return THR_mysys; }

static int set_mysys_thread_var(struct st_my_thread_var *mysys_var) {
  THR_mysys = mysys_var;
  return 0;
=======
static struct st_my_thread_var *mysys_thread_var()
{
  assert(THR_KEY_mysys_initialized);
  return  (struct st_my_thread_var*)my_get_thread_local(THR_KEY_mysys);
}


static int set_mysys_thread_var(struct st_my_thread_var *mysys_var)
{
  assert(THR_KEY_mysys_initialized);
  return my_set_thread_local(THR_KEY_mysys, mysys_var);
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
}
#endif

/**
  Re-initialize components initialized early with @c my_thread_global_init.
  Some mutexes were initialized before the instrumentation.
  Destroy + create them again, now that the instrumentation
  is in place.
  This is safe, since this function() is called before creating new threads,
  so the mutexes are not in use.
*/

<<<<<<< HEAD:mysys/my_thr_init.cc
void my_thread_global_reinit() {
<<<<<<< HEAD
  assert(my_thread_global_init_done);
=======
  DBUG_ASSERT(my_thread_global_init_done);
=======
void my_thread_global_reinit()
{
  assert(my_thread_global_init_done);
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
>>>>>>> pr/231

#ifdef HAVE_PSI_INTERFACE
  my_init_mysys_psi_keys();
#endif

  mysql_mutex_destroy(&THR_LOCK_heap);
  mysql_mutex_init(key_THR_LOCK_heap, &THR_LOCK_heap, MY_MUTEX_INIT_FAST);

  mysql_mutex_destroy(&THR_LOCK_net);
  mysql_mutex_init(key_THR_LOCK_net, &THR_LOCK_net, MY_MUTEX_INIT_FAST);

  mysql_mutex_destroy(&THR_LOCK_myisam);
  mysql_mutex_init(key_THR_LOCK_myisam, &THR_LOCK_myisam, MY_MUTEX_INIT_SLOW);

  mysql_mutex_destroy(&THR_LOCK_malloc);
  mysql_mutex_init(key_THR_LOCK_malloc, &THR_LOCK_malloc, MY_MUTEX_INIT_FAST);

  mysql_mutex_destroy(&THR_LOCK_open);
  mysql_mutex_init(key_THR_LOCK_open, &THR_LOCK_open, MY_MUTEX_INIT_FAST);

  mysql_mutex_destroy(&THR_LOCK_charset);
  mysql_mutex_init(key_THR_LOCK_charset, &THR_LOCK_charset, MY_MUTEX_INIT_FAST);

#ifndef NDEBUG
  mysql_mutex_destroy(&THR_LOCK_threads);
  mysql_mutex_init(key_THR_LOCK_threads, &THR_LOCK_threads, MY_MUTEX_INIT_FAST);

  mysql_cond_destroy(&THR_COND_threads);
  mysql_cond_init(key_THR_COND_threads, &THR_COND_threads);
#endif
}

/**
  initialize thread environment

  @retval  false  ok
  @retval  true   error
*/

bool my_thread_global_init() {
  if (my_thread_global_init_done) return false;
  my_thread_global_init_done = true;

#if defined(SAFE_MUTEX)
  safe_mutex_global_init(); /* Must be called early */
#endif

#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
  /*
    Set mutex type to "fast" a.k.a "adaptive"

    In this case the thread may steal the mutex from some other thread
    that is waiting for the same mutex.  This will save us some
    context switches but may cause a thread to 'starve forever' while
    waiting for the mutex (not likely if the code within the mutex is
    short).
  */
  pthread_mutexattr_init(&my_fast_mutexattr);
  pthread_mutexattr_settype(&my_fast_mutexattr, PTHREAD_MUTEX_ADAPTIVE_NP);
#endif

#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
  /*
    Set mutex type to "errorcheck"
  */
  pthread_mutexattr_init(&my_errorcheck_mutexattr);
  pthread_mutexattr_settype(&my_errorcheck_mutexattr, PTHREAD_MUTEX_ERRORCHECK);
#endif

<<<<<<< HEAD:mysys/my_thr_init.cc
=======
  assert(! THR_KEY_mysys_initialized);
#ifndef NDEBUG
  if ((pth_ret= my_create_thread_local_key(&THR_KEY_mysys, NULL)) != 0)
  { /* purecov: begin inspected */
    my_message_local(ERROR_LEVEL, "Can't initialize threads: error %d",
                     pth_ret);
    /* purecov: end */
    return TRUE;
  }
#endif
  if ((pth_ret= my_create_thread_local_key(&THR_KEY_myerrno, NULL)) != 0)
  { /* purecov: begin inspected */
    my_message_local(ERROR_LEVEL, "Can't initialize threads: error %d",
                     pth_ret);
    /* purecov: end */
    return TRUE;
  }
#ifdef _WIN32
  if ((pth_ret= my_create_thread_local_key(&THR_KEY_winerrno, NULL)) != 0)
  { /* purecov: begin inspected */
    my_message_local(ERROR_LEVEL, "Can't initialize threads: error %d",
                     pth_ret);
    /* purecov: end */
    return TRUE;
  }
#endif
  THR_KEY_mysys_initialized= TRUE;

>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
  mysql_mutex_init(key_THR_LOCK_malloc, &THR_LOCK_malloc, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_THR_LOCK_open, &THR_LOCK_open, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_THR_LOCK_charset, &THR_LOCK_charset, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_THR_LOCK_lock, &THR_LOCK_lock, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_THR_LOCK_myisam, &THR_LOCK_myisam, MY_MUTEX_INIT_SLOW);
  mysql_mutex_init(key_THR_LOCK_myisam_mmap, &THR_LOCK_myisam_mmap,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_THR_LOCK_heap, &THR_LOCK_heap, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_THR_LOCK_net, &THR_LOCK_net, MY_MUTEX_INIT_FAST);
#ifndef NDEBUG
  mysql_mutex_init(key_THR_LOCK_threads, &THR_LOCK_threads, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_THR_COND_threads, &THR_COND_threads);
#endif

  return false;
}

<<<<<<< HEAD:mysys/my_thr_init.cc
void my_thread_global_end() {
<<<<<<< HEAD
#ifndef NDEBUG
=======
#ifndef DBUG_OFF
=======

void my_thread_global_end()
{
#ifndef NDEBUG
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
>>>>>>> pr/231
  struct timespec abstime;
  bool all_threads_killed = true;

  set_timespec(&abstime, my_thread_end_wait_time);
  mysql_mutex_lock(&THR_LOCK_threads);
  while (THR_thread_count > 0) {
    int error =
        mysql_cond_timedwait(&THR_COND_threads, &THR_LOCK_threads, &abstime);
    if (is_timeout(error)) {
#ifndef _WIN32
      /*
        We shouldn't give an error here, because if we don't have
        pthread_kill(), programs like mysqld can't ensure that all threads
        are killed when we enter here.
      */
      if (THR_thread_count) /* purecov: begin inspected */
        my_message_local(ERROR_LEVEL, EE_FAILED_TO_KILL_ALL_THREADS,
                         THR_thread_count);
        /* purecov: end */
#endif
      all_threads_killed = false;
      break;
    }
  }
  mysql_mutex_unlock(&THR_LOCK_threads);
#endif

<<<<<<< HEAD:mysys/my_thr_init.cc
=======
  assert(THR_KEY_mysys_initialized);
#ifndef NDEBUG
  my_delete_thread_local_key(THR_KEY_mysys);
#endif
  my_delete_thread_local_key(THR_KEY_myerrno);
#ifdef _WIN32
  my_delete_thread_local_key(THR_KEY_winerrno);
#endif
  THR_KEY_mysys_initialized= FALSE;
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
  pthread_mutexattr_destroy(&my_fast_mutexattr);
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
  pthread_mutexattr_destroy(&my_errorcheck_mutexattr);
#endif
  mysql_mutex_destroy(&THR_LOCK_malloc);
  mysql_mutex_destroy(&THR_LOCK_open);
  mysql_mutex_destroy(&THR_LOCK_lock);
  mysql_mutex_destroy(&THR_LOCK_myisam);
  mysql_mutex_destroy(&THR_LOCK_myisam_mmap);
  mysql_mutex_destroy(&THR_LOCK_heap);
  mysql_mutex_destroy(&THR_LOCK_net);
  mysql_mutex_destroy(&THR_LOCK_charset);
<<<<<<< HEAD
#ifndef NDEBUG
=======
<<<<<<< HEAD:mysys/my_thr_init.cc
#ifndef DBUG_OFF
>>>>>>> pr/231
  if (all_threads_killed) {
=======
#ifndef NDEBUG
  if (all_threads_killed)
  {
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
    mysql_mutex_destroy(&THR_LOCK_threads);
    mysql_cond_destroy(&THR_COND_threads);
  }
#endif

  my_thread_global_init_done = false;
}

/**
  Allocate thread specific memory for the thread, used by mysys and dbug

  @note This function may called multiple times for a thread, for example
  if one uses my_init() followed by mysql_server_init().

  @retval false  ok
  @retval true   Fatal error; mysys/dbug functions can't be used
*/

<<<<<<< HEAD:mysys/my_thr_init.cc
extern "C" bool my_thread_init() {
<<<<<<< HEAD
#ifndef NDEBUG
=======
#ifndef DBUG_OFF
=======
my_bool my_thread_init()
{
#ifndef NDEBUG
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
>>>>>>> pr/231
  struct st_my_thread_var *tmp;
#endif

  if (!my_thread_global_init_done)
    return true; /* cannot proceed with uninitialized library */

#ifdef _WIN32
  install_sigabrt_handler();
#endif

<<<<<<< HEAD
#ifndef NDEBUG
=======
<<<<<<< HEAD:mysys/my_thr_init.cc
#ifndef DBUG_OFF
>>>>>>> pr/231
  if (mysys_thread_var()) return false;
=======
#ifndef NDEBUG
  if (mysys_thread_var())
    return FALSE;
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c

  if (!(tmp = (struct st_my_thread_var *)calloc(1, sizeof(*tmp)))) return true;

  mysql_mutex_lock(&THR_LOCK_threads);
  tmp->id = ++thread_id;
  ++THR_thread_count;
  mysql_mutex_unlock(&THR_LOCK_threads);
  set_mysys_thread_var(tmp);
#endif

  return false;
}

/**
  Deallocate memory used by the thread for book-keeping

  @note This may be called multiple times for a thread.
  This happens for example when one calls 'mysql_server_init()'
  mysql_server_end() and then ends with a mysql_end().
*/

<<<<<<< HEAD:mysys/my_thr_init.cc
extern "C" void my_thread_end() {
#ifndef NDEBUG
  struct st_my_thread_var *tmp = mysys_thread_var();
=======
void my_thread_end()
{
#ifndef NDEBUG
  struct st_my_thread_var *tmp= mysys_thread_var();
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
#endif

#ifdef HAVE_PSI_THREAD_INTERFACE
  /*
    Remove the instrumentation for this thread.
    This must be done before trashing st_my_thread_var,
    because the LF_HASH depends on it.
  */
  PSI_THREAD_CALL(delete_current_thread)();
#endif

<<<<<<< HEAD
#if !defined(NDEBUG)
=======
<<<<<<< HEAD:mysys/my_thr_init.cc
#if !defined(DBUG_OFF)
>>>>>>> pr/231
  if (tmp) {
=======
#if !defined(NDEBUG)
  if (tmp)
  {
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
    /* tmp->dbug is allocated inside DBUG library */
    if (tmp->dbug) {
      DBUG_POP();
      free(tmp->dbug);
      tmp->dbug = nullptr;
    }
    free(tmp);

    /*
      Decrement counter for number of running threads. We are using this
      in my_thread_global_end() to wait until all threads have called
      my_thread_end and thus freed all memory they have allocated in
      my_thread_init() and DBUG_xxxx
    */
    mysql_mutex_lock(&THR_LOCK_threads);
<<<<<<< HEAD
    assert(THR_thread_count != 0);
=======
<<<<<<< HEAD:mysys/my_thr_init.cc
    DBUG_ASSERT(THR_thread_count != 0);
>>>>>>> pr/231
    if (--THR_thread_count == 0) mysql_cond_signal(&THR_COND_threads);
=======
    assert(THR_thread_count != 0);
    if (--THR_thread_count == 0)
      mysql_cond_signal(&THR_COND_threads);
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
    mysql_mutex_unlock(&THR_LOCK_threads);
  }
  set_mysys_thread_var(nullptr);
#endif
}

int my_errno() { return THR_myerrno; }

void set_my_errno(int my_errno) { THR_myerrno = my_errno; }

#ifdef _WIN32
int thr_winerr() { return THR_winerrno; }

void set_thr_winerr(int winerr) { THR_winerrno = winerr; }
#endif

<<<<<<< HEAD
#ifndef NDEBUG
=======
<<<<<<< HEAD:mysys/my_thr_init.cc
#ifndef DBUG_OFF
>>>>>>> pr/231
my_thread_id my_thread_var_id() { return mysys_thread_var()->id; }
=======

#ifndef NDEBUG
my_thread_id my_thread_var_id()
{
  return mysys_thread_var()->id;
}
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c

void set_my_thread_var_id(my_thread_id id) { mysys_thread_var()->id = id; }

<<<<<<< HEAD:mysys/my_thr_init.cc
CODE_STATE **my_thread_var_dbug() {
  struct st_my_thread_var *tmp = THR_mysys;
<<<<<<< HEAD
  return tmp ? &tmp->dbug : nullptr;
=======
=======
void set_my_thread_var_id(my_thread_id id)
{
  mysys_thread_var()->id= id;
}


struct _db_code_state_ **my_thread_var_dbug()
{
  struct st_my_thread_var *tmp;
  /*
    Instead of enforcing assert(THR_KEY_mysys_initialized) here,
    which causes any DBUG_ENTER and related traces to fail when
    used in init / cleanup code, we are more tolerant:
    using DBUG_ENTER / DBUG_PRINT / DBUG_RETURN
    when the dbug instrumentation is not in place will do nothing.
  */
  if (! THR_KEY_mysys_initialized)
    return NULL;
  tmp= mysys_thread_var();
>>>>>>> upstream/cluster-7.6:mysys/my_thr_init.c
  return tmp ? &tmp->dbug : NULL;
>>>>>>> pr/231
}
#endif /* NDEBUG */

#ifdef _WIN32
/*
  In Visual Studio 2005 and later, default SIGABRT handler will overwrite
  any unhandled exception filter set by the application  and will try to
  call JIT debugger. This is not what we want, this we calling __debugbreak
  to stop in debugger, if process is being debugged or to generate
  EXCEPTION_BREAKPOINT and then handle_segfault will do its magic.
*/

static void my_sigabrt_handler(int sig [[maybe_unused]]) { __debugbreak(); }

static void install_sigabrt_handler() {
  /*abort() should not override our exception filter*/
  _set_abort_behavior(0, _CALL_REPORTFAULT);
  signal(SIGABRT, my_sigabrt_handler);
}
#endif
