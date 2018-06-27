/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <stddef.h>

#include "my_inttypes.h"
#include "my_psi_config.h"
#include "my_rdtsc.h"
#include "plugin/x/ngs/include/ngs/log.h"
#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs/scheduler.h"
#include "plugin/x/ngs/include/ngs_common/bind.h"

using namespace ngs;

const uint64_t MILLI_TO_NANO = 1000000;
const ulonglong TIME_VALUE_NOT_VALID = 0;

Scheduler_dynamic::Scheduler_dynamic(const char *name,
                                     PSI_thread_key thread_key)
    : m_name(name),
      m_worker_pending_mutex(KEY_mutex_x_scheduler_dynamic_worker_pending),
      m_worker_pending_cond(KEY_cond_x_scheduler_dynamic_worker_pending),
      m_thread_exit_mutex(KEY_mutex_x_scheduler_dynamic_thread_exit),
      m_thread_exit_cond(KEY_cond_x_scheduler_dynamic_thread_exit),
      m_is_running(0),
      m_min_workers_count(1),
      m_workers_count(0),
      m_tasks_count(0),
      m_idle_worker_timeout(60 * 1000),
      m_thread_key(thread_key) {}

Scheduler_dynamic::~Scheduler_dynamic() { stop(); }

void Scheduler_dynamic::launch() {
  int32 int_0 = 0;
  if (m_is_running.compare_exchange_strong(int_0, 1)) {
    create_min_num_workers();
    log_debug("Scheduler \"%s\" started.", m_name.c_str());
  }
}

void Scheduler_dynamic::create_min_num_workers() {
  MUTEX_LOCK(lock, m_worker_pending_mutex);

  while (is_running() && m_workers_count.load() < m_min_workers_count.load()) {
    create_thread();
  }
}

unsigned int Scheduler_dynamic::set_num_workers(unsigned int n) {
  log_debug("Scheduler '%s', set number of threads to %u", m_name.c_str(), n);
  m_min_workers_count.store(n);
  try {
    create_min_num_workers();
  } catch (const std::exception &e) {
    /* 'log_debug' isn't defined while building release executable.
     'e' object is used only by log_debug. Below line disables
     warnings about unused variables.*/
    (void)e;
    log_debug("Exception in set minimal number of workers \"%s\"", e.what());

    const int32 m = m_workers_count.load();
    log_warning(ER_XPLUGIN_FAILED_TO_SET_MIN_NUMBER_OF_WORKERS, n, m);
    m_min_workers_count.store(m);
    return m;
  }
  return n;
}

void Scheduler_dynamic::set_idle_worker_timeout(
    unsigned long long milliseconds) {
  m_idle_worker_timeout.store(milliseconds);
  m_worker_pending_cond.broadcast(m_worker_pending_mutex);
}

void Scheduler_dynamic::stop() {
  int32 int_1 = 1;
  if (m_is_running.compare_exchange_strong(int_1, 0)) {
    while (m_tasks.empty() == false) {
      Task *task = NULL;

      if (m_tasks.pop(task)) ngs::free_object(task);
    }

    m_worker_pending_cond.broadcast(m_worker_pending_mutex);

    {
      MUTEX_LOCK(lock, m_thread_exit_mutex);
      while (m_workers_count.load())
        m_thread_exit_cond.wait(m_thread_exit_mutex);
    }

    Thread_t thread;
    while (m_threads.pop(thread)) {
      ngs::thread_join(&thread, NULL);
    }

    log_debug("Scheduler \"%s\" stopped.", m_name.c_str());
  }
}

// NOTE: Scheduler takes ownership of the task and deletes it after
//       completion with delete operator.
bool Scheduler_dynamic::post(Task *task) {
  if (is_running() == false || task == NULL) return false;

  {
    MUTEX_LOCK(lock, m_worker_pending_mutex);

    log_debug("Scheduler '%s', post task", m_name.c_str());

    if (increase_tasks_count() >= m_workers_count.load()) {
      try {
        create_thread();
      } catch (std::exception &e) {
        log_error(ER_XPLUGIN_EXCEPTION_IN_TASK_SCHEDULER, e.what());
        decrease_tasks_count();
        return false;
      }
    }
  }

  while (m_tasks.push(task) == false) {
  }
  m_worker_pending_cond.signal(m_worker_pending_mutex);

  return true;
}

bool Scheduler_dynamic::post(const Task &task) {
  Task *copy_task = ngs::allocate_object<Task>(task);

  if (post(copy_task)) return true;

  ngs::free_object(copy_task);

  return false;
}

bool Scheduler_dynamic::post_and_wait(const Task &task_to_be_posted) {
  Wait_for_signal future;

  {
    ngs::Scheduler_dynamic::Task task =
        ngs::bind(&Wait_for_signal::Signal_when_done::execute,
                  ngs::allocate_shared<ngs::Wait_for_signal::Signal_when_done>(
                      ngs::ref(future), task_to_be_posted));

    if (!post(task)) {
      log_error(ER_XPLUGIN_TASK_SCHEDULING_FAILED);
      return false;
    }
  }

  future.wait();

  return true;
}

// NOTE: Scheduler takes ownership of monitor.
void Scheduler_dynamic::set_monitor(Monitor_interface *monitor) {
  m_monitor.reset(monitor);
}

void *Scheduler_dynamic::worker_proxy(void *data) {
  return reinterpret_cast<Scheduler_dynamic *>(data)->worker();
}

void Scheduler_dynamic::thread_end() {
#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_THREAD_CALL(delete_current_thread)();
#endif
}

bool Scheduler_dynamic::wait_if_idle_then_delete_worker(
    ulonglong &thread_waiting_started) {
  MUTEX_LOCK(lock, m_worker_pending_mutex);

  if (TIME_VALUE_NOT_VALID == thread_waiting_started) {
    thread_waiting_started = my_timer_milliseconds();
  }

  if (!is_running()) return false;

  if (!m_tasks.empty()) return false;

  const int64 thread_waiting_for_delta_ms =
      my_timer_milliseconds() - thread_waiting_started;

  if (thread_waiting_for_delta_ms < m_idle_worker_timeout) {
    // Some implementations may signal a condition variable without
    // any reason. We need to write the time when the thread went to idle state
    // state and monitor it!
    const int result = m_worker_pending_cond.timed_wait(
        m_worker_pending_mutex,
        (m_idle_worker_timeout - thread_waiting_for_delta_ms) * MILLI_TO_NANO);

    if (!is_timeout(result)) return false;
  } else {
    // Lets invalidate the timeout, if the thread won't die
    // in next iteration then we should reinitialize the start-of-idle value
    thread_waiting_started = TIME_VALUE_NOT_VALID;
  }

  if (m_workers_count.load() > m_min_workers_count.load()) {
    decrease_workers_count();
    return true;
  }

  return false;
}

void *Scheduler_dynamic::worker() {
  bool worker_active = true;
  if (thread_init()) {
    ulonglong thread_waiting_time = TIME_VALUE_NOT_VALID;
    while (is_running()) {
      bool task_available = false;

      try {
        Task *task = NULL;

        while (is_running() && m_tasks.empty() == false &&
               task_available == false) {
          task_available = m_tasks.pop(task);
        }

        if (task_available && task) {
          ngs::Memory_instrumented<Task>::Unique_ptr task_ptr(task);
          thread_waiting_time = TIME_VALUE_NOT_VALID;

          (*task_ptr)();
        }
      } catch (std::exception &e) {
        log_error(ER_XPLUGIN_EXCEPTION_IN_EVENT_LOOP, m_name.c_str(), e.what());
      }

      if (task_available) {
        decrease_tasks_count();
      } else {
        if (wait_if_idle_then_delete_worker(thread_waiting_time)) {
          worker_active = false;

          break;
        }
      }
    }
    thread_end();
  }

  {
    MUTEX_LOCK(lock_exit, m_thread_exit_mutex);
    MUTEX_LOCK(lock_workers, m_worker_pending_mutex);
    if (worker_active) decrease_workers_count();
    m_thread_exit_cond.signal();
  }

  m_terminating_workers.push(my_thread_self());

  return NULL;
}

void Scheduler_dynamic::join_terminating_workers() {
  my_thread_t tid;
  while (m_terminating_workers.pop(tid)) {
    Thread_t thread;
    if (m_threads.remove_if(thread,
                            ngs::bind(Scheduler_dynamic::thread_id_matches,
                                      ngs::placeholders::_1, tid))) {
      ngs::thread_join(&thread, NULL);
    }
  }
}

void Scheduler_dynamic::create_thread() {
  if (is_running()) {
    Thread_t thread;
    log_debug("Scheduler '%s', create threads", m_name.c_str());

    ngs::thread_create(m_thread_key, &thread, worker_proxy, this);
    increase_workers_count();
    m_threads.push(thread);
  }
}

bool Scheduler_dynamic::is_running() { return m_is_running.load() != 0; }

int32 Scheduler_dynamic::increase_workers_count() {
  if (m_monitor) m_monitor->on_worker_thread_create();

  return ++m_workers_count;
}

int32 Scheduler_dynamic::decrease_workers_count() {
  if (m_monitor) m_monitor->on_worker_thread_destroy();

  return --m_workers_count;
}

int32 Scheduler_dynamic::increase_tasks_count() {
  if (m_monitor) m_monitor->on_task_start();

  return ++m_tasks_count;
}

int32 Scheduler_dynamic::decrease_tasks_count() {
  if (m_monitor) m_monitor->on_task_end();

  return --m_tasks_count;
}
