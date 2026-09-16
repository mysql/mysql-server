// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include "mysql/concurrency/thread_srv.h"

namespace mysql::concurrency {

namespace detail {

using Func_t = std::function<void(void)>;

/// @brief Function launching the thread function and matching interface of the
/// my_start_routine
inline static void *launch_handler_thread(void *arg) {
  auto *func = reinterpret_cast<Func_t *>(arg);
  my_thread_init();
  (*func)();
  my_thread_end();
  delete func;
  return nullptr;
}

}  // namespace detail

template <class Callable, class... Args>
Thread::Thread(Thread_key thread_key, Callable &&run_func, Args &&...args) {
  my_thread_attr_init(&m_thread_attr);
  my_thread_attr_setdetachstate(&m_thread_attr, MY_THREAD_CREATE_JOINABLE);
  // launch_handler_thread becomes the owner of func_p, it will
  // delete it
  auto *func_p = new detail::Func_t(
      std::bind(std::move(run_func), std::forward<Args>(args)...));
  m_creation_error_code =
      mysql_thread_create(thread_key, &m_thread_handle, &m_thread_attr,
                          detail::launch_handler_thread, func_p);
  if (m_creation_error_code != 0) {
    delete func_p;
  }
}

template <class Callable, class... Args>
Thread::Thread(Callable &&run_func, Args &&...args) {
  my_thread_attr_init(&m_thread_attr);
  my_thread_attr_setdetachstate(&m_thread_attr, MY_THREAD_CREATE_JOINABLE);
  // launch_handler_thread becomes the owner of func_p, it will
  // delete it
  auto *func_p = new detail::Func_t(
      std::bind(std::move(run_func), std::forward<Args>(args)...));
  m_creation_error_code =
      mysql_thread_create(m_thread_key, &m_thread_handle, &m_thread_attr,
                          detail::launch_handler_thread, func_p);
  if (m_creation_error_code != 0) {
    delete func_p;
  }
}

}  // namespace mysql::concurrency
