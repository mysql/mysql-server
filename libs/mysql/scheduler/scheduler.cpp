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

#include "mysql/scheduler/scheduler.h"

#include <mutex>

namespace mysql::scheduler {

Scheduler::~Scheduler() { deinit(); }

void Scheduler::deinit() {
  if (m_deinitialized.exchange(true)) {
    return;
  }

  end_execution();
  if (m_scheduler_thread.joinable()) {
    wait_for_scheduler_thread_to_stop();
    m_scheduler_thread.join();
  }
  synchronize();
  m_status.store(Scheduler_status::exit);
}

Scheduler::Scheduler(Thread_pool_ptr shared_thread_pool,
                     Scheduler_clock_ptr shared_clock,
                     Dependency_tracker_ptr dependency_tracker, int instance_id,
                     std::size_t allowed_task_count, Scheduler_psi psi_params)
    : m_mutex_notification(
          MYSQL_CONCURRENCY_DEFINE_MT_PSI_KEY(psi_params.key_mt_sched)),
      m_mutex_tasks(
          MYSQL_CONCURRENCY_DEFINE_MT_PSI_KEY(psi_params.key_mt_sched)),
      m_mutex_phases(
          MYSQL_CONCURRENCY_DEFINE_MT_PSI_KEY(psi_params.key_mt_sched)),
      m_cv_scheduler(
          MYSQL_CONCURRENCY_DEFINE_CV_PSI_KEY(psi_params.key_cv_sched)),
      m_mutex_end(MYSQL_CONCURRENCY_DEFINE_MT_PSI_KEY(psi_params.key_mt_end)),
      m_cv_end(MYSQL_CONCURRENCY_DEFINE_CV_PSI_KEY(psi_params.key_cv_end)),
      m_notified_tasks(std::min<std::size_t>(allowed_task_count, 8192UL)),
      m_instance_id(instance_id),
      m_stat_monitor(Statistics_monitor::get(instance_id)),
      m_allowed_task_count(allowed_task_count),
      m_psi(psi_params) {
  m_thread_pool = shared_thread_pool;
  m_scheduler_clock = shared_clock;
  m_scheduler_thread = MDEF_CREATE_THREAD(psi_params.key_th_scheduler,
                                          &Scheduler::run_main_thread, this);
  if (!m_scheduler_thread.joinable()) {
    m_scheduler_thread_active = false;
    m_is_error.test_and_set();
  }
  m_dependencies = std::move(dependency_tracker);
}

void Scheduler::wait_for_scheduler_thread_to_stop() {
  std::unique_lock<Mutex_type> lock(m_mutex_end);
  m_cv_end.wait(lock, [this]() -> bool { return !m_scheduler_thread_active; });
}

void Scheduler::end_execution() {
  m_scheduler_active.store(false);
  notify_scheduler();
}

bool Scheduler::add_dependency(const Task_id &predecessor,
                               const Task_id &successor) {
  return m_dependencies->add_dependency(predecessor, successor);
}

bool Scheduler::are_phase_queues_empty() const {
  // lock m_mutex_scheduler before
  for (const auto &entry : m_task_phases) {
    if (entry.second.size() != 0) {
      return false;
    }
  }
  return true;
}

namespace {
bool is_queue_ready(const auto &queue, const auto &clock,
                    bool is_phase = false) {
  return !queue.empty() && queue.top().get_task_delay() <= clock->now() &&
         (!is_phase || queue.top().is_phase_ready());
}
}  // namespace

bool Scheduler::is_task_phase_ready() const {
  // lock m_mutex_scheduler before
  for (const auto &entry : m_task_phases) {
    auto &queue = entry.second;
    auto &clock = entry.first;
    if (is_queue_ready(queue, clock, true)) {
      return true;
    }
  }
  return false;
}

bool Scheduler::is_task_ready() const {
  // lock m_mutex_scheduler before
  return is_queue_ready(this->m_tasks, m_scheduler_clock, false);
}

void Scheduler::run_main_thread() {
  while (true) {
    using Thread_task_type = Thread_pool<Task_return_type>::Task_type;
    std::vector<std::pair<Task_id, Thread_task_type>> ready_tasks;

    auto thp_queue_size = m_thread_pool->queue_size();

    m_stat_monitor.get()
        .get_stat(Statistics_map::thp_queue_size)
        .store(thp_queue_size, 0);

    {
      std::unique_lock<Mutex_type> lock(m_mutex_notification);
      auto stop_waiting = [this]() -> bool { return m_notification.load(); };
      concurrency::set_stage(m_psi.key_stage_waiting);
      bool cv_code = m_cv_scheduler.wait_for(
          lock, std::chrono::microseconds(100), stop_waiting);
      if (cv_code == false) {
        if (!m_wait_for_task_limit) {
          ++m_timeouts;
        }
      }
      m_notification = false;
    }

    // handle stop without error / stop now
    bool end_now = is_stop_requested();
    if (m_scheduler_active.load() == false || end_now) {
      m_notification = true;
      if (!end_now) {
        std::scoped_lock<Mutex_type, Mutex_type> lock(m_mutex_tasks,
                                                      m_mutex_phases);
        end_now = this->m_tasks.empty() && are_phase_queues_empty() &&
                  m_tasks_waiting_for_deps.size() == 0;
      }
      if (end_now) {
        concurrency::set_stage(m_psi.key_stage_stopping);
        m_status.store(Scheduler_status::exiting_scheduler_thread);
        scheduler_clean_up();
        std::scoped_lock<Mutex_type> lock(m_mutex_end);
        m_scheduler_thread_active = false;
        m_cv_end.notify_all();
        concurrency::set_stage(m_psi.key_stage_stopped);
        return;  // endpoint
      }
    }

    auto push_ready_task = [](auto &ready_tasks, Scheduled_task task) -> auto{
      Thread_task_type packaged_task(
          [task_func = std::move(task.m_task),
           repeatable_task = std::move(task.m_repeatable_task),
           dispatch_reason = task.get_dispatch_reason()](
              unsigned int thread_id) mutable -> Task_return_type {
            Scoped_dispatch_reason scoped_reason(dispatch_reason);
            (void)repeatable_task;
            return task_func(thread_id);
          });
      ready_tasks.push_back(
          std::make_pair(task.m_task_id, std::move(packaged_task)));
    };

    concurrency::set_stage(m_psi.key_stage_check_dependencies);

    // Process ready tasks
    while (!m_notified_tasks.is_empty()) {
      auto ready_id_val = m_notified_tasks.pop();
      Task_id ready_id(ready_id_val);
      auto it = m_tasks_waiting_for_deps.find(ready_id);
      if (it != m_tasks_waiting_for_deps.end()) {
        push_ready_task(ready_tasks, it->second);
        m_tasks_waiting_for_deps.erase(it);
      }
    }

    concurrency::set_stage(m_psi.key_stage_check_stage_queues);

    {  // process phases
      std::lock_guard<Mutex_type> lock(m_mutex_phases);
      for (auto &entry : m_task_phases) {
        const auto &clock = entry.first;
        auto &queue = entry.second;
        bool do_unblock = m_unblock_request.exchange(false);
        while (!queue.empty() && queue.top().get_task_delay() <= clock->now() &&
               (queue.top().is_phase_ready() || do_unblock)) {
          auto task = queue.top();
          task.set_dispatch_reason(queue.top().is_phase_ready()
                                       ? Dispatch_reason::normal
                                       : Dispatch_reason::unblocked);
          queue.pop();
          push_ready_task(ready_tasks, task);
        }
      }
    }

    {  // process enqueued tasks
      std::lock_guard<Mutex_type> lock(m_mutex_tasks);
      auto now_time = m_scheduler_clock->now();

      while (!m_tasks.empty() && m_tasks.top().get_task_delay() <= now_time) {
        Scheduled_task task(m_tasks.top());
        m_tasks.pop();
        if (m_dependencies->check_ready(task.m_task_id)) {
          if (task.is_enqueued_by_scheduler()) {
            std::ignore = enqueue_phase(
                task.m_schedule, task.m_schedule->get_task_delay(),
                task.get_repeatable_task(), Constants::scheduler_thread_id,
                task.m_schedule->get_phase_id() + 1);
          }
          push_ready_task(ready_tasks, task);
        } else {
          m_tasks_waiting_for_deps.emplace(
              std::piecewise_construct, std::forward_as_tuple(task.m_task_id),
              std::forward_as_tuple(task));
        }
      }
    }

    concurrency::set_stage(m_psi.key_stage_enqueueing_ready_tasks);

    for (auto &&func : ready_tasks) {
      m_thread_pool->enqueue(std::move(func.second));
    }
    std::this_thread::yield();
  }
}

bool Scheduler::is_error() const { return m_is_error.test(); }

void Scheduler::scheduler_clean_up() {
  if (is_stop_requested()) {
    // Remove pending queued representations. The shared task counter guard
    // decrements the scheduled task count when the last representation dies.
    m_tasks_waiting_for_deps.clear();
    {
      std::lock_guard<Mutex_type> lock(m_mutex_phases);
      for (auto &phase_queue : m_task_phases) {
        while (!phase_queue.second.empty()) {
          phase_queue.second.pop();
        }
      }
    }
    {
      std::lock_guard<Mutex_type> lock(m_mutex_tasks);
      while (!m_tasks.empty()) {
        m_tasks.pop();
      }
    }
  }
}

void Scheduler::handle_error() {
  m_is_error.test_and_set();
  notify_scheduler();
}

Scheduler::Task_queue_type &Scheduler::get_phase_queue(
    Scheduler_clock_ptr phase_clock) {
  auto iterator = m_task_phases.find(phase_clock);
  assert(iterator != m_task_phases.end());
  return iterator->second;
}

Scheduler::Mutex_type &Scheduler::get_phase_queue_lock(
    Scheduler_clock_ptr phase_clock) {
  auto iterator = m_task_phases_locks.find(phase_clock);
  assert(iterator != m_task_phases_locks.end());
  return iterator->second;
}

bool Scheduler::register_phase(Scheduler_clock_ptr phase_clock) {
  std::lock_guard<Mutex_type> lock(m_mutex_phases);
  bool inserted{false};
  std::tie(std::ignore, inserted) = m_task_phases_locks.emplace(
      std::piecewise_construct, std::forward_as_tuple(phase_clock),
      std::forward_as_tuple(
          MYSQL_CONCURRENCY_DEFINE_MT_PSI_KEY(m_psi.key_mt_phases)));
  if (!inserted) {
    return inserted;
  }
  std::tie(std::ignore, inserted) =
      m_task_phases.emplace(phase_clock, Task_queue_type());
  return inserted;
}

bool Scheduler::enqueue_helper(Scheduled_task &&task) {
  {
    std::lock_guard<Mutex_type> lock(m_mutex_tasks);
    if (is_stop_requested()) {
      return false;
    }
    m_tasks.push(std::move(task));
  }
  notify_scheduler();
  return true;
}

void Scheduler::synchronize_active(bool print_checkpoint, std::size_t expected,
                                   bool force) {
  std::size_t iter_idle = 0;
  std::size_t prev = m_scheduled_tasks_cnt.load();
  std::chrono::time_point<std::chrono::system_clock> time_same_value;
  long int cp_ms = 100;
  std::size_t allowed_idle_iters = 100000;
  assert(m_scheduler_clock);
  while (prev > expected && (force || !is_stop_requested())) {
    auto current = m_scheduled_tasks_cnt.load();
    if (prev != current) {
      iter_idle = 0;
      time_same_value = std::chrono::system_clock::now();
    } else if (++iter_idle % allowed_idle_iters == 0 &&
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now() - time_same_value)
                       .count() > cp_ms) {
      iter_idle = 0;
      time_same_value = std::chrono::system_clock::now();
      if (print_checkpoint) {
        std::cout << "queue size is: " << m_thread_pool->queue_size()
                  << " current tasks waiting: " << current
                  << " dependencies queue size: "
                  << m_tasks_waiting_for_deps.size()
                  << " queue size: " << m_tasks.size()
                  << " scheduler active flag: " << m_scheduler_active
                  << " now: " << m_scheduler_clock->now() << std::endl;
      }
      if (m_scheduler_clock->try_unblock()) {
        notify_scheduler();
      }
    }
    prev = current;
    std::this_thread::yield();
  }
}

void Scheduler::synchronize_active_partial(bool print_checkpoint) {
  m_wait_for_task_limit = true;
  // Use hysteresis to avoid long producer stalls at the task limit.
  // Once throttling is triggered, wait only until we drain to 75% of capacity.
  auto sync_cnt = (m_allowed_task_count.load() * 3) / 4;
  synchronize_active(print_checkpoint, sync_cnt, false);
  m_wait_for_task_limit = false;
}

bool Scheduler::synchronize(bool force, bool print_checkpoint) {
  if (!force && is_stop_requested()) {
    return false;  // not synchronized - false
  }
  synchronize_active(print_checkpoint, 0, force);
  return m_scheduled_tasks_cnt == 0;  // synchronized if 0
}

void Scheduler::synchronize_partial(bool print_checkpoint) {
  synchronize_active_partial(print_checkpoint);
}

std::size_t Scheduler::get_timeouts() const { return m_timeouts.load(); }

void Scheduler::notify_scheduler() {
  bool was_notified = m_notification.exchange(true);
  if (!was_notified) {
    std::lock_guard<Mutex_type> lock(m_mutex_notification);
    m_cv_scheduler.notify_one();
  }
}

Task_result Scheduler::callback_phase(Task_schedule_ptr schedule,
                                      bool task_error, unsigned int phase_id) {
  auto exec_time = schedule->get_phase_delay(phase_id);
  auto id = schedule->get_id();
  if (task_error) {
    handle_error();
    // task will be released in "callback"
    return Task_result::fatal_error;
  }

  bool time_advanced = schedule->get_phase_clock(phase_id)->tick(id, exec_time);
  if (time_advanced) {
    notify_scheduler();
  }
  return Task_result::success;
}

std::size_t Scheduler::get_scheduled_tasks_count() const {
  return m_scheduled_tasks_cnt.load();
}

void Scheduler::stop_now() {
  m_stop_now = true;
  end_execution();
}

bool Scheduler::is_stop_requested() const { return is_error() || m_stop_now; }

bool Scheduler::ensure_space() {
  if (m_scheduled_tasks_cnt.load() < m_allowed_task_count.load()) {
    return true;
  }
  m_wait_for_task_limit = true;
  auto sync_cnt = (m_allowed_task_count.load() - 1);
  synchronize_active(false, sync_cnt, false);
  m_wait_for_task_limit = false;
  return m_scheduled_tasks_cnt.load() < m_allowed_task_count.load();
}

void Scheduler::request_unblock() {
  m_unblock_request.store(true);
  notify_scheduler();
}

}  // namespace mysql::scheduler
