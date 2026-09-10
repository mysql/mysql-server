/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "health_monitor_thread.h"

#include "my_thread.h"

#include "health_monitor.h"
#include "health_monitor_lock.h"
#include "health_monitor_log.h"

namespace hm {

Thread::Thread(const char *name, const char *instr_name, const char *os_name,
               const char *desc)
    : m_name{name},
      m_instr_name{instr_name},
      m_os_name{os_name},
      m_description{desc} {
  PSI_THREAD_CALL(register_thread("health", &m_thread_info, 1));
  mysql_cond_register("health", &m_cond_info, 1);
  mysql_cond_init(m_cond_key, &m_wait);
  mysql_mutex_register("health", &m_mutex_info, 1);
  mysql_mutex_init(m_mutex_key, &m_wait_lock, nullptr);
}

Thread::~Thread() {
  stop_thread();
  mysql_cond_destroy(&m_wait);
  mysql_mutex_destroy(&m_wait_lock);
}

int Thread::start() {
  auto locked = std::unique_lock<std::mutex>(m_command_lock);
  return start_thread();
}

void Thread::stop() {
  auto locked = std::unique_lock<std::mutex>(m_command_lock);
  stop_thread();
}

/**
  Main thread loop.
*/
void Thread::run_process() {
  pre_process();

  mysql_mutex_lock(&m_wait_lock);
  bool done{m_stop};
  mysql_mutex_unlock(&m_wait_lock);

  while (!done) {
    process();
    mysql_mutex_lock(&m_wait_lock);
    done = m_stop;
    if (!done) {
      mysql_cond_wait(&m_wait, &m_wait_lock);
      done = m_stop;
    }
    mysql_mutex_unlock(&m_wait_lock);
  }

  post_process();
}

/**
  Cleanup thread instrumentation.
*/
void Thread::cleanup() { PSI_THREAD_CALL(delete_current_thread)(); }

/**
  Signal the processing loop to end the current cycle.
*/
void Thread::signal() { mysql_cond_signal(&m_wait); }

int Thread::join(my_thread_handle *thread, void **value_ptr) {
  return pthread_join(thread->thread, value_ptr);
}

bool Thread::joinable(const my_thread_handle &thread) {
  return thread.thread != 0;
}

/**
  Static routine for thread instrumentation interface.
*/
static void *start_routine(void *arg) {
  auto *thread = reinterpret_cast<Thread *>(arg);
  if (thread != nullptr) {
    thread->run_process();
    thread->cleanup();
  }
  return nullptr;
}

int Thread::start_thread() {
  if (m_thread_running) return 0;
  stop_thread();
  {
    const mutex_guard wait_lock(&m_wait_lock);
    m_stop = false;
  }
  const int error = PSI_THREAD_CALL(spawn_thread)(m_thread_key, 0, &m_thread,
                                                  nullptr, start_routine, this);
  if (error != 0) {
    log_error("%s: Failed to start", name());
  } else {
    m_thread_running = true;
  }
  return error;
}

void Thread::stop_thread() {
  if (!m_thread_running) return;
  request_thread_stop();
  join_thread();
}

/** Request that the worker stop without waiting for it to exit. */
void Thread::request_thread_stop() {
  if (!m_thread_running) return;
  {
    const mutex_guard wait_lock(&m_wait_lock);
    m_stop = true;
  }
  mysql_cond_signal(&m_wait);
}

/** Wait for a stopped worker to exit. */
void Thread::join_thread() {
  if (!m_thread_running) return;
  if (joinable(m_thread)) {
    join(&m_thread, nullptr);
  }
  m_thread_running = false;
}

/** Return true when worker shutdown has been requested. */
bool Thread::stop_requested() {
  const mutex_guard wait_lock(&m_wait_lock);
  return m_stop;
}

}  // namespace hm
