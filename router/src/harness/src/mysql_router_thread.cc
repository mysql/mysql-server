/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql_router_thread.h"

#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <errno.h>
#include <process.h>
#include <signal.h>
#include <stdlib.h>
#endif

namespace mysql_harness {

static inline int mysql_router_thread_attr_init(
    mysql_router_thread_attr_t *attr) {
#ifdef _WIN32
  attr->dwStackSize = 0;
  /* Set to joinable by default to match Linux */
  attr->detachstate = MYSQL_ROUTER_THREAD_CREATE_JOINABLE;
  return 0;
#else
  return pthread_attr_init(attr);
#endif
}

static inline int mysql_router_thread_attr_setstacksize(
    mysql_router_thread_attr_t *attr, size_t stacksize) {
#ifdef _WIN32
  attr->dwStackSize = (DWORD)stacksize;
  return 0;
#else
  return pthread_attr_setstacksize(attr, stacksize);
#endif
}

static inline int mysql_router_thread_attr_setdetachstate(
    mysql_router_thread_attr_t *attr, int detachstate) {
#ifdef _WIN32
  attr->detachstate = detachstate;
  return 0;
#else
  return pthread_attr_setdetachstate(attr, detachstate);
#endif
}

/**
 * @brief checks if thread pointed by thread was started
 */
static inline bool mysql_router_thread_started(
    const mysql_router_thread_handle *thread) {
#ifndef _WIN32
  return thread->thread != 0;
#else
  return thread->handle != INVALID_HANDLE_VALUE;
#endif
}

/**
 * @brief checks if thread pointed by attr is joinable
 *
 * @throw std::runtime_error if cannot check if thread is joinable
 */
static inline bool mysql_router_thread_joinable(
    const mysql_router_thread_attr_t *attr) {
#ifndef _WIN32
  int detachstate;

  int rc = pthread_attr_getdetachstate(attr, &detachstate);
  if (rc) throw std::runtime_error("Failed to check if thread is joinable");

  return detachstate == MYSQL_ROUTER_THREAD_CREATE_JOINABLE;
#else
  return attr->detachstate == MYSQL_ROUTER_THREAD_CREATE_JOINABLE;
#endif
}

#ifdef _WIN32
struct thread_start_parameter {
  my_start_routine func;
  void *arg;
};

static unsigned int __stdcall win_thread_start(void *p) {
  struct thread_start_parameter *par = (struct thread_start_parameter *)p;
  my_start_routine func = par->func;
  void *arg = par->arg;
  free(p);
  (*func)(arg);
  return 0;
}
#endif

int mysql_router_thread_create(mysql_router_thread_handle *thread,
                               const mysql_router_thread_attr_t *attr,
                               my_start_routine func, void *arg) {
#ifndef _WIN32
  return pthread_create(&thread->thread, attr, func, arg);
#else
  struct thread_start_parameter *par;
  unsigned int stack_size;

  par = (struct thread_start_parameter *)malloc(sizeof(*par));
  if (!par) goto error_return;

  par->func = func;
  par->arg = arg;
  stack_size = attr ? attr->dwStackSize : kDefaultStackSizeInKiloBytes;

  thread->handle =
      (HANDLE)_beginthreadex(NULL, stack_size, win_thread_start, par, 0,
                             (unsigned int *)&thread->thread);

  if (thread->handle) {
    /* Note that JOINABLE is default, so attr == NULL => JOINABLE. */
    if (attr && attr->detachstate == MYSQL_ROUTER_THREAD_CREATE_DETACHED) {
      /*
        Close handles for detached threads right away to avoid leaking
        handles. For joinable threads we need the handle during
        mysql_router_thread_join. It will be closed there.
      */
      CloseHandle(thread->handle);
      thread->handle = NULL;
    }
    return 0;
  }

  free(par);

error_return:
  thread->thread = 0;
  thread->handle = NULL;
  return 1;
#endif
}

int mysql_router_thread_join(mysql_router_thread_handle *thread,
                             void **value_ptr) {
#ifndef _WIN32
  return pthread_join(thread->thread, value_ptr);
#else
  UNREFERENCED_PARAMETER(value_ptr);
  DWORD ret;
  int result = 0;
  ret = WaitForSingleObject(thread->handle, INFINITE);
  if (ret != WAIT_OBJECT_0) {
    result = 1;
  }
  if (thread->handle) CloseHandle(thread->handle);
  thread->thread = 0;
  thread->handle = NULL;
  return result;
#endif
}

MySQLRouterThread::MySQLRouterThread(size_t thread_stack_size) {
  mysql_router_thread_attr_init(&thread_attr_);

  int res = mysql_router_thread_attr_setstacksize(&thread_attr_,
                                                  thread_stack_size << 10);
  if (res)
    throw std::runtime_error("Failed to adjust stack size, result code=" +
                             std::to_string(res));
}

void MySQLRouterThread::run(thread_function run_thread, void *args_ptr,
                            bool detach) {
  if (detach) {
    mysql_router_thread_attr_setdetachstate(
        &thread_attr_, MYSQL_ROUTER_THREAD_CREATE_DETACHED);
  } else {
    should_join_ = true;
  }

  int ret = mysql_router_thread_create(&thread_handle_, &thread_attr_,
                                       run_thread, args_ptr);
  if (ret) throw std::runtime_error("Cannot create Thread");
}

void MySQLRouterThread::join() {
  if (mysql_router_thread_started(&thread_handle_) &&
      mysql_router_thread_joinable(&thread_attr_)) {
    mysql_router_thread_join(&thread_handle_, nullptr);
  }
  should_join_ = false;
}

MySQLRouterThread::~MySQLRouterThread() {
  if (should_join_) join();
}

}  // namespace mysql_harness
