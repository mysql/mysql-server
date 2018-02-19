/*****************************************************************************

Copyright (c) 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/os0thread-create.h
 The interface to the threading wrapper

 Created 2016-May-17 Sunny Bains
 *******************************************************/

#ifndef os0thread_create_h
#define os0thread_create_h

#include "os0thread.h"
#include "univ.i"

#include <my_thread.h>
#include <atomic>
#include <functional>

/** Maximum number of threads inside InnoDB */
extern ulint srv_max_n_threads;

/** Number of threads active. */
extern std::atomic_int os_thread_count;

/** Initializes OS thread management data structures. */
inline void os_thread_open() { /* No op */
}

/** Check if there are threads active.
@return true if the thread count > 0. */
inline bool os_thread_any_active() {
  return (os_thread_count.load(std::memory_order_relaxed) > 0);
}

/** Frees OS thread management data structures. */
inline void os_thread_close() {
  if (os_thread_any_active()) {
    ib::warn() << "Some (" << os_thread_count.load(std::memory_order_relaxed)
               << ") threads are still active";
  }
}

/** Wrapper for a callable, it will count the number of registered
Runnable instances and will register the thread executing the callable
with the PFS and the Server threading infrastructure. */
class Runnable {
 public:
#ifdef UNIV_PFS_THREAD
  /** Constructor for the Runnable object.
  @param[in]	pfs_key		Performance schema key */
  explicit Runnable(mysql_pfs_key_t pfs_key) : m_pfs_key(pfs_key) {}
#else
  /** Constructor for the Runnable object.
  @param[in]	pfs_key		Performance schema key (ignored) */
  explicit Runnable(mysql_pfs_key_t) {}
#endif /* UNIV_PFS_THREAD */

 public:
  /** Method to execute the callable
  @param[in]	f		Callable object
  @param[in]	args		Variable number of args to F */
  template <typename F, typename... Args>
  void operator()(F &&f, Args &&... args) {
    preamble();

    auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

    task();

    epilogue();
  }

 private:
  /** Register the thread with the server */
  void preamble() {
    my_thread_init();

#if defined(UNIV_PFS_THREAD) && !defined(UNIV_HOTBACKUP)
    if (m_pfs_key.m_value != PFS_NOT_INSTRUMENTED.m_value) {
      PSI_thread *psi;

      psi = PSI_THREAD_CALL(new_thread)(m_pfs_key.m_value, nullptr, 0);

      PSI_THREAD_CALL(set_thread_os_id)(psi);
      PSI_THREAD_CALL(set_thread)(psi);
    }
#endif /* UNIV_PFS_THREAD && !UNIV_HOTBACKUP */

    std::atomic_thread_fence(std::memory_order_release);

    int old;

    old = os_thread_count.fetch_add(1, std::memory_order_relaxed);

    ut_a(old <= static_cast<int>(srv_max_n_threads) - 1);
  }

  /** Deregister the thread */
  void epilogue() {
    std::atomic_thread_fence(std::memory_order_release);

    int old;

    old = os_thread_count.fetch_sub(1, std::memory_order_relaxed);
    ut_a(old > 0);

    my_thread_end();

#if defined(UNIV_PFS_THREAD) && !defined(UNIV_HOTBACKUP)
    if (m_pfs_key.m_value != PFS_NOT_INSTRUMENTED.m_value) {
      PSI_THREAD_CALL(delete_current_thread)();
    }
#endif /* UNIV_PFS_THREAD && !UNIV_HOTBACKUP */
  }

 private:
#ifdef UNIV_PFS_THREAD
  /** Performance schema key */
  const mysql_pfs_key_t m_pfs_key;
#endif /* UNIV_PFS_THREAD */
};

/** Create a detached thread
@param[in]	pfs_key		Performance schema thread key
@param[in]	f		Callable instance
@param[in]	args		zero or more args */
template <typename F, typename... Args>
void create_detached_thread(mysql_pfs_key_t pfs_key, F &&f, Args &&... args) {
  std::thread t(Runnable(pfs_key), f, args...);

  t.detach();
}

#ifdef UNIV_PFS_THREAD
#define os_thread_create(...) create_detached_thread(__VA_ARGS__)
#else
#define os_thread_create(k, ...) create_detached_thread(0, __VA_ARGS__)
#endif /* UNIV_PFS_THREAD */

/** Parallel for loop over a container.
@param[in]	pfs_key		Performance schema thread key
@param[in]	c		Container to iterate over in parallel
@param[in]	n		Number of threads to create
@param[in]	f		Callable instance
@param[in]	args		zero or more args */
template <typename Container, typename F, typename... Args>
void par_for(mysql_pfs_key_t pfs_key, const Container &c, size_t n, F &&f,
             Args &&... args) {
  if (c.empty()) {
    return;
  }

  size_t slice = (n > 0) ? c.size() / n : 0;

  using Workers = std::vector<std::thread>;

  Workers workers;

  for (size_t i = 0; i < n; ++i) {
    auto b = c.begin() + (i * slice);
    auto e = b + slice;

    workers.push_back(std::thread{Runnable{pfs_key}, f, b, e, i, args...});
  }

  f(c.begin() + (n * slice), c.end(), n, args...);

  for (auto &worker : workers) {
    worker.join();
  }
}

#if defined(UNIV_PFS_THREAD) && !defined(UNIV_HOTBACKUP)
#define par_for(...) par_for(__VA_ARGS__)
#else
#define par_for(k, ...) par_for(0, __VA_ARGS__)
#endif /* UNIV_PFS_THREAD */

#endif /* !os0thread_create_h */
