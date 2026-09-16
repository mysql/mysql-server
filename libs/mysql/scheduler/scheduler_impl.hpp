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

#include <chrono>
#include <iostream>
#include <memory>
#include <type_traits>

#include <string>

#include "mysql/scheduler/logger_stream.h"
#include "mysql/scheduler/scheduler.h"

namespace mysql::scheduler {

namespace detail {

template <class F, class Tid, class Tuple, std::size_t... I>
constexpr decltype(auto) apply_with_thread_id_impl(F &&f, Tid &&tid, Tuple &&t,
                                                   std::index_sequence<I...>) {
  return std::invoke(std::forward<F>(f), std::forward<Tid>(tid),
                     std::get<I>(std::forward<Tuple>(t))...);
}

template <class F, class Tid, class Tuple, std::size_t... I>
constexpr decltype(auto) apply_with_thread_id(F &&f, Tid &&tid, Tuple &&t) {
  return apply_with_thread_id_impl(
      std::forward<F>(f), std::forward<Tid>(tid), std::forward<Tuple>(t),
      std::make_index_sequence<std::tuple_size_v<std::decay_t<Tuple>>>{});
}

}  // namespace detail

template <typename FType, typename... Args>
bool Scheduler::enqueue_internal(Task_schedule_ptr schedule, FType &&task,
                                 Args &&...args) {
  if (m_scheduled_tasks_cnt == m_allowed_task_count) {
    concurrency::set_stage(m_psi.key_reached_max_task_limit);
    this->synchronize_partial();
  }
  auto task_delay = schedule->get_task_delay();
  while (true) {
    if (m_scheduler_clock->add_time(schedule->get_id(), task_delay)) {
      break;  // ok
    }
    concurrency::set_stage(m_psi.key_stage_wait_clock_queue);
    this->synchronize_partial();
  }

  ++m_scheduled_tasks_cnt;

  using Decayed_FType = std::decay_t<FType>;
  auto task_shared = std::make_shared<Decayed_FType>(std::forward<FType>(task));

  constexpr bool may_error_out = requires(Decayed_FType & t) { t.is_error(); };

  std::tuple<Args...> tuple_args =
      std::forward_as_tuple(std::forward<Args>(args)...);

  auto repeatable_lambda = [this, schedule, task_shared,
                            tuple_args = std::move(tuple_args)](
                               unsigned int thread_id) mutable -> bool {
    auto &time_exec_stat =
        m_stat_monitor.get().get_stat(Statistics_map::sched_task_exec_time);
    time_exec_stat.start_time(thread_id);
    detail::apply_with_thread_id(*task_shared, thread_id, tuple_args);
    time_exec_stat.stop_time(thread_id);
    bool is_error = false;
    if constexpr (may_error_out) {
      is_error = task_shared->is_error();
    }
    return is_error;
  };

  auto repeatable_task = std::make_shared<Repeatable_task_state>(
      m_scheduled_tasks_cnt, std::move(repeatable_lambda));

  auto exec_task = [this, schedule, task_delay, repeatable_task](
                       unsigned int thread_id) mutable -> Task_result {
    auto is_error = repeatable_task->execute(thread_id);
    return this->callback(schedule, task_delay, is_error, repeatable_task,
                          thread_id);
  };

  Scheduled_task scheduled(schedule->get_id(), std::move(exec_task),
                           repeatable_task, schedule, schedule->get_phase_id());
  if (!schedule->is_finished()) {
    scheduled.set_enqueued_by_scheduler();
  }
  if (!enqueue_helper(std::move(scheduled))) {
    return false;
  }
  return true;
}

template <typename Repeatable_task_type>
bool Scheduler::enqueue_phase(
    Task_schedule_ptr schedule, Scheduler_clock::Time_point_t task_delay,
    std::shared_ptr<Repeatable_task_type> repeatable_task,
    unsigned int current_thread_id, unsigned int phase_id) {
  auto &phase_clock = schedule->get_phase_clock(phase_id);
  auto &phase_queue = get_phase_queue(phase_clock);
  // auto & phase_mt = get_phase_queue_lock(phase_clock);
  auto phase_delay = schedule->get_phase_delay(phase_id);
  [[maybe_unused]] auto subscribed =
      phase_clock->add_time(schedule->get_id(), phase_delay);
  assert(subscribed);
  bool self_enqueue = (current_thread_id == Constants::scheduler_thread_id);

  auto phase_task = [this, schedule, task_delay, repeatable_task,
                     phase_id](unsigned int thread_id) mutable -> Task_result {
    bool is_error = repeatable_task->execute(thread_id);
    auto result = this->callback_phase(schedule, is_error, phase_id);
    if (result != Task_result::success) {
      is_error = true;
    }
    // hack for start-to-start is to call () twice, first call is
    // an after start callback
    if (schedule->get_phase_clock(phase_id)->get_type() ==
            Scheduler_dependency_type::start_to_start &&
        result == Task_result::success) {
      is_error = repeatable_task->execute(thread_id);
    }
    return this->callback(schedule, task_delay, is_error, repeatable_task,
                          thread_id);
  };

  Scheduled_task scheduled(schedule->get_id(), std::move(phase_task),
                           repeatable_task, schedule, phase_id);
  {
    if (!self_enqueue) {
      m_mutex_phases.lock();
      if (is_error() || m_stop_now) {
        m_mutex_phases.unlock();
        return false;
      }
    }
    phase_queue.push(std::move(scheduled));
    if (!self_enqueue) {
      m_mutex_phases.unlock();
    }
  }
  notify_scheduler();
  return true;
}

template <typename Repeatable_task_type>
Task_result Scheduler::callback(
    Task_schedule_ptr schedule, Scheduler_clock::Time_point_t task_delay,
    bool task_error, std::shared_ptr<Repeatable_task_type> repeatable_task,
    unsigned int current_thread_id) {
  if (task_error) {
    handle_error();
    return Task_result::fatal_error;
  }
  if (schedule->next()) {
    if (!schedule->is_enqueued_by_scheduler()) {
      [[maybe_unused]] auto enqueued =
          enqueue_phase(schedule, task_delay, repeatable_task,
                        current_thread_id, schedule->get_phase_id());
      if (!enqueued) {
        return Task_result::fatal_error;
      }
    }
    schedule->set_phase_ready();
    notify_scheduler();
    return Task_result::success;
  }
  auto id = schedule->get_id();
  auto unblocked_tasks = m_dependencies->mark_dependency_met(id, true);
  for (auto &entry_id : unblocked_tasks) {
    m_notified_tasks.push(entry_id.get());
  }
  bool time_advanced = m_scheduler_clock->tick(id, task_delay);
  if (time_advanced || !unblocked_tasks.empty()) {
    notify_scheduler();
  }
  // At this point, we are not using internal data structures anymore, we
  // can notify scheduler that we are done with this task
  return Task_result::success;
}

template <typename FType, typename... Args>
bool Scheduler::enqueue(Task_schedule_ptr schedule, FType &&task,
                        Args &&...args) {
  const auto &task_id = schedule->get_id();
  [[maybe_unused]] bool activated = m_dependencies->activate_task(task_id);
  assert(activated);
  bool is_ok = enqueue_internal(schedule, std::forward<FType>(task),
                                std::forward<Args>(args)...);
  return is_ok;
}

template <typename FType, typename... Args>
bool Scheduler::enqueue_after(const Task_id &predecessor,
                              Task_schedule_ptr schedule, FType &&task,
                              Args &&...args) {
  const auto &task_id = schedule->get_id();
  [[maybe_unused]] bool activated = m_dependencies->activate_task(task_id);
  assert(activated);
  [[maybe_unused]] auto dep_registered = add_dependency(predecessor, task_id);
  assert(dep_registered);
  bool is_ok = enqueue_internal(schedule, std::forward<FType>(task),
                                std::forward<Args>(args)...);
  return is_ok;
}

}  // namespace mysql::scheduler
