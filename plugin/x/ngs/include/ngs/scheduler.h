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

#ifndef _NGS_SCHEDULER_H_
#define _NGS_SCHEDULER_H_

#include <atomic>
#include <list>
#include <string>
#include <vector>

#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs/thread.h"

namespace ngs {
// Scheduler with dynamic thread pool.
class Scheduler_dynamic {
 public:
  class Monitor_interface {
   public:
    virtual ~Monitor_interface() {}

    virtual void on_worker_thread_create() = 0;
    virtual void on_worker_thread_destroy() = 0;
    virtual void on_task_start() = 0;
    virtual void on_task_end() = 0;
  };

  typedef ngs::function<void()> Task;

  Scheduler_dynamic(const char *name,
                    PSI_thread_key thread_key = PSI_NOT_INSTRUMENTED);
  virtual ~Scheduler_dynamic();

  virtual void launch();
  virtual void stop();
  virtual unsigned int set_num_workers(unsigned int n);
  void set_idle_worker_timeout(unsigned long long milliseconds);
  bool post(Task *task);
  bool post(const Task &task);
  bool post_and_wait(const Task &task);

  virtual bool thread_init() { return true; }
  virtual void thread_end();

  void set_monitor(Monitor_interface *monitor);

  bool is_worker_thread(my_thread_t thread_id);
  bool is_running();
  void join_terminating_workers();

 private:
  template <typename Element_type>
  class lock_list {
   public:
    lock_list() : m_access_mutex(KEY_mutex_x_lock_list_access) {}

    bool empty() {
      MUTEX_LOCK(guard, m_access_mutex);
      return m_list.empty();
    }

    bool push(const Element_type &t) {
      MUTEX_LOCK(guard, m_access_mutex);
      m_list.push_back(t);
      return true;
    }

    bool pop(Element_type &result) {
      MUTEX_LOCK(guard, m_access_mutex);
      if (m_list.empty()) return false;

      result = m_list.front();

      m_list.pop_front();
      return true;
    }

    bool remove_if(Element_type &result,
                   ngs::function<bool(Element_type &)> matches) {
      MUTEX_LOCK(guard, m_access_mutex);
      for (typename std::list<Element_type>::iterator it = m_list.begin();
           it != m_list.end(); ++it) {
        if (matches(*it)) {
          result = *it;
          m_list.erase(it);
          return true;
        }
      }

      return false;
    }

   private:
    Mutex m_access_mutex;
    std::list<Element_type> m_list;
  };

  Scheduler_dynamic(const Scheduler_dynamic &);
  Scheduler_dynamic &operator=(const Scheduler_dynamic &);

  static void *worker_proxy(void *data);
  void *worker();

  void create_thread();
  void create_min_num_workers();

  static bool thread_id_matches(Thread_t &thread, my_thread_t id) {
    return thread.thread == id;
  }

  bool wait_if_idle_then_delete_worker(ulonglong &thread_waiting_started);
  int32 increase_workers_count();
  int32 decrease_workers_count();
  int32 increase_tasks_count();
  int32 decrease_tasks_count();

  const std::string m_name;
  Mutex m_worker_pending_mutex;
  Cond m_worker_pending_cond;
  Mutex m_thread_exit_mutex;
  Cond m_thread_exit_cond;
  Mutex m_post_mutex;
  volatile std::atomic<int32> m_is_running;
  volatile std::atomic<int32> m_min_workers_count;
  volatile std::atomic<int32> m_workers_count;
  volatile std::atomic<int32> m_tasks_count;
  volatile std::atomic<int64> m_idle_worker_timeout;  // milliseconds
  lock_list<Task *> m_tasks;
  lock_list<Thread_t> m_threads;
  lock_list<my_thread_t> m_terminating_workers;
  ngs::Memory_instrumented<Monitor_interface>::Unique_ptr m_monitor;
  PSI_thread_key m_thread_key;
};
}  // namespace ngs

#endif
