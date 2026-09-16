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

#include "mysql/scheduler/statistics_map.h"
#include "mysql/scheduler/statistics_monitor.h"
#include "mysql/scheduler/thread_pool.h"

namespace mysql::scheduler {

template <class T, std::size_t t_queue_size>
Thread_pool<T, t_queue_size>::Thread_pool(unsigned int thread_num,
                                          unsigned int instance_id,
                                          Thread_pool_psi psi_params)
    : m_workers(Thread_allocator(psi_params.memory_resource)),
      m_tasks(psi_params.memory_resource),
      m_workers_num(thread_num),
      m_instance_id(instance_id),
      m_psi(psi_params) {
  m_workers.reserve(thread_num);
  for (unsigned int i = 0; i < m_workers_num; ++i) {
    m_end_execution.emplace(std::piecewise_construct, std::forward_as_tuple(i),
                            std::forward_as_tuple(false));
  }
}

template <class T, std::size_t t_queue_size>
bool Thread_pool<T, t_queue_size>::init() {
  if (m_initialized) return false;

  for (auto &[_, end_execution] : m_end_execution) {
    end_execution.store(false);
  }
  for (unsigned int i = 0; i < m_workers_num; i++) {
    try {
      auto worker = MDEF_CREATE_THREAD(m_psi.key_thread_worker,
                                       &Thread_pool::run_worker, this, i);
      if (has_creation_error(worker)) {
        deinit();
        return true;
      }
      m_workers.push_back(std::move(worker));
    } catch (const std::system_error &) {
      deinit();
      return true;
    }
  }
  m_initialized = true;
  return false;
}

template <class T, std::size_t t_queue_size>
void Thread_pool<T, t_queue_size>::deinit() {
  end_execution();
  for (auto &worker : m_workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  m_workers.clear();
  m_initialized = false;
}

template <class T, std::size_t t_queue_size>
void Thread_pool<T, t_queue_size>::enqueue(Task_type &&task) {
  m_tasks.enqueue(std::move(task));
}

template <class T, std::size_t t_queue_size>
std::string Thread_pool<T, t_queue_size>::print_queue_state() const {
  return m_tasks.print_queue_state();
}

template <class T, std::size_t t_queue_size>
std::size_t Thread_pool<T, t_queue_size>::queue_size() const {
  return m_tasks.size();
}

template <class T, std::size_t t_queue_size>
void Thread_pool<T, t_queue_size>::run_worker(
    [[maybe_unused]] unsigned int thread_id) {
  auto &stat_monitor = Statistics_monitor::get(m_instance_id);
  stat_monitor.get_stat(Statistics_map::thp_thread_internal_id)
      .store(concurrency::fetch_thread_mysql_id(thread_id), thread_id);
  auto &task_time_stat =
      stat_monitor.get_stat(Statistics_map::thp_task_exec_time);
  auto &worker_time_stat =
      stat_monitor.get_stat(Statistics_map::thp_worker_exec_time);
  worker_time_stat.start_time(thread_id);

  concurrency::set_stage(m_psi.key_stage_waiting);

  while (true) {
    auto stop_waiting = [this, &thread_id]() -> bool {
      return m_end_execution[thread_id].load();
    };
    auto [task, valid] = m_tasks.dequeue(stop_waiting);
    if (!valid) {
      break;
    }
    concurrency::set_stage(m_psi.key_stage_executing);
    task_time_stat.start_time(thread_id);
    task(thread_id);
    task_time_stat.stop_time(thread_id);
    concurrency::set_stage(m_psi.key_stage_waiting);
  }
  worker_time_stat.stop_time(thread_id);
  concurrency::set_stage(m_psi.key_stage_stopped);
}

template <class T, std::size_t t_queue_size>
Thread_pool<T, t_queue_size>::~Thread_pool() {
  deinit();
}

template <class T, std::size_t t_queue_size>
void Thread_pool<T, t_queue_size>::end_execution() {
  for (unsigned int i = 0; i < m_workers_num; i++) {
    m_end_execution[i].store(true);
  }
  m_tasks.notify_all();
}

template <class T, std::size_t t_queue_size>
std::size_t Thread_pool<T, t_queue_size>::size() const {
  return m_workers.size();
}

template <class T, std::size_t t_queue_size>
bool Thread_pool<T, t_queue_size>::has_creation_error(const Thread &thread) {
#ifdef STANDALONE_LIBS_MYSQL
  (void)thread;
  return false;
#else
  return thread.creation_error_code() != 0;
#endif
}

}  // namespace mysql::scheduler
