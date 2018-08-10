/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_HARNESS_MYSQL_ROUTER_THREAD_INCLUDED
#define MYSQL_HARNESS_MYSQL_ROUTER_THREAD_INCLUDED

#include "harness_export.h"

#ifndef MYSQL_ABI_CHECK
#if defined(_WIN32)
#include <stdlib.h>
#include <windows.h>
#else
#include <pthread.h>  // IWYU pragma: export
#include <sched.h>    // IWYU pragma: export
#include <stdexcept>
#endif
#endif /* MYSQL_ABI_CHECK */

namespace mysql_harness {

static const size_t kDefaultStackSizeInKiloBytes = 1024;

#ifdef _WIN32
typedef DWORD mysql_router_thread_t;
typedef struct thread_attr {
  DWORD dwStackSize;
  int detachstate;
} mysql_router_thread_attr_t;
#else
typedef pthread_t mysql_router_thread_t;
typedef pthread_attr_t mysql_router_thread_attr_t;
#endif

struct mysql_router_thread_handle {
  mysql_router_thread_t thread{0};
#ifdef _WIN32
  HANDLE handle{INVALID_HANDLE_VALUE};
#endif
};

#ifdef _WIN32
#define MYSQL_ROUTER_THREAD_CREATE_JOINABLE 0
#define MYSQL_ROUTER_THREAD_CREATE_DETACHED 1
typedef void *(__cdecl *my_start_routine)(void *);
#else
#define MYSQL_ROUTER_THREAD_CREATE_JOINABLE PTHREAD_CREATE_JOINABLE
#define MYSQL_ROUTER_THREAD_CREATE_DETACHED PTHREAD_CREATE_DETACHED
typedef void *(*my_start_routine)(void *);
#endif

/**
 * @brief MySQLRouterThread provides higher level interface to managing threads.
 */
class HARNESS_EXPORT MySQLRouterThread {
 public:
  using thread_function = void *(void *);

  /**
   * Allocates memory for thread of execution.
   *
   * @param thread_stack_size the memory size allocated to thread's stack
   *
   * @throw std::runtime_error if cannot adjust thread size
   */
  MySQLRouterThread(
      size_t thread_stack_size = mysql_harness::kDefaultStackSizeInKiloBytes);

  /**
   * Execute run_thread function in thread of execution.
   *
   * @param run_thread the pointer to the function that is executed in thread.
   * It has to be non-member void*(void*) function
   * @param args_ptr pointer to run_thread parameter
   * @param detach true if thread is detached, false if thread is joinable
   *
   * @throw std::runtime_error if cannot create new thread of execution
   */
  void run(thread_function run_thread, void *args_ptr, bool detach = false);

  /**
   * Waits for a thread to finish its execution
   */
  void join();

  /**
   * Waits for a thread to finish its execution if thread is joinable and join
   * wasn't called.
   */
  ~MySQLRouterThread();

 private:
  /** @brief handle to the thread */
  mysql_harness::mysql_router_thread_handle thread_handle_;

  /** @brief attribute of thread */
  mysql_harness::mysql_router_thread_attr_t thread_attr_;

  /** @brief true if thread is joinable but join wasn't called, false otherwise
   */
  bool should_join_ = false;
};

}  // namespace mysql_harness

#endif  // end of MYSQL_HARNESS_MYSQL_ROUTER_THREAD_INCLUDED
