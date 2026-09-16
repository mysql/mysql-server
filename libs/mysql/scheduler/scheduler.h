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

#ifndef MYSQL_SCHEDULER_SCHEDULER_H
#define MYSQL_SCHEDULER_SCHEDULER_H

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <list>
#include <memory>
#include <queue>
#include <string>
#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/concurrency/thread.h"
#include "mysql/scheduler/base_dependency_tracker.h"
#include "mysql/scheduler/scheduled_task.h"
#include "mysql/scheduler/scheduler_clock.h"
#include "mysql/scheduler/scheduler_psi.h"
#include "mysql/scheduler/task_id.h"
#include "mysql/scheduler/task_result.h"
#include "mysql/scheduler/task_schedule.h"
#include "mysql/scheduler/thread_pool.h"
#include "mysql/scheduler/time.h"
#include "sql/containers/integrals_lockfree_queue.h"

namespace mysql::scheduler {

enum class Sync_method { active };

enum class Scheduler_status {
  idle,
  check_dependencies,
  check_queue,
  scheduling,
  exiting_scheduler_thread,
  exited_scheduler_thread,
  final_sync,
  exit
};

/// @class Scheduler
/// @brief Main scheduling class
class Scheduler {
 public:
  using Task_return_type = Task_result;
  using Thread_pool_type = Thread_pool<Task_return_type>;
  using Thread_pool_ptr = std::shared_ptr<Thread_pool_type>;
  using Scheduled_task_type = Scheduled_task;
  using Mutex_type = concurrency::Mutex;
  using Task_queue_type = std::priority_queue<Scheduled_task>;

  // choose synchronization method
  static constexpr Sync_method sync_method{Sync_method::active};

  /// @brief Destructor
  virtual ~Scheduler();

  /// @brief Ends the scheduler if stop was not yet requested
  void deinit();

  /// @brief Copying constructor is deleted
  Scheduler(const Scheduler &src) = delete;

  /// @brief Constructor
  /// @param shared_thread_pool Shared thread pool
  /// @param shared_clock Shared scheduler clock
  /// @param dependency_tracker Dependency tracker
  /// @param instance_id Unique instance id for statistics monitoring
  /// @param allowed_task_count Sets the number of ongoing tasks limit to
  /// selected number. If
  /// the number of tasks reaches this number, scheduler will synchronize
  /// to reach 25% of allowed_count tasks
  /// @param psi_params PSI parameters
  Scheduler(
      Thread_pool_ptr shared_thread_pool, Scheduler_clock_ptr shared_clock,
      Dependency_tracker_ptr dependency_tracker, int instance_id = 0,
      std::size_t allowed_task_count = std::numeric_limits<std::size_t>::max(),
      Scheduler_psi psi_params = {});

  /// @brief Enqueues a task
  /// @details Function adding task to the scheduler
  /// @tparam FType Functor type
  /// @tparam Args Functor arguments parameter pack type
  /// @param schedule Pointer to task schedule
  /// @param task Functor to execute
  /// @param args Functor arguments
  /// @returns True on success, false otherwise
  template <typename FType, typename... Args>
  bool enqueue(Task_schedule_ptr schedule, FType &&task, Args &&...args);

  /// @brief Enqueues a task that should execute after a predecessor task
  /// finishes
  /// @details Function adding task to the scheduler
  /// @tparam FType Functor type
  /// @tparam Args Functor arguments parameter pack type
  /// @param predecessor Task id that should execute prior to the \a task
  /// @param schedule Pointer to task schedule
  /// @param task Functor to execute
  /// @param args Functor arguments
  /// @returns True on success, false otherwise
  template <typename FType, typename... Args>
  bool enqueue_after(const Task_id &predecessor, Task_schedule_ptr schedule,
                     FType &&task, Args &&...args);

  /// @brief This function waits for currently fired tasks to finish (blocking)
  /// @param force If enabled, wait for tasks even in case of error or stop
  /// requested
  /// @param print_checkpoint Prints diagnostics during waiting
  /// @retval true Successfully synchronized
  /// @retval false Stopped while waiting
  bool synchronize(bool force = true, bool print_checkpoint = false);

  /// @brief Access "timeouts" observability statistics - the number of times
  /// scheduler timed out while waiting for tasks to schedule
  std::size_t get_timeouts() const;

  /// @brief Internal function to check on whether error has been set
  bool is_error() const;

  /// @brief Obtain the number of scheduled, ongoing tasks
  /// @return the number of scheduled, ongoing tasks
  std::size_t get_scheduled_tasks_count() const;

  /// @brief Register task phase run according to the given clock
  /// @param phase_clock Phase clock for the task phase
  /// @return True if phase was registered. False otherwise
  bool register_phase(Scheduler_clock_ptr phase_clock);

  /// @brief Instruct the scheduler to stop now and withdraw all work
  void stop_now();

  /// @brief Function used to notify scheduler thread by the other thread
  /// (enqueueing thread / worker poll thread)
  void notify_scheduler();

  /// @brief Ensure space for the next task (eliminates possible wait in the
  /// enqueue)
  /// @return True if space is available. False otherwise.
  bool ensure_space();

  /// @brief This function requests unblocking of the scheduler. After this
  /// call, the scheduler will try to unblock the workflow
  void request_unblock();

 protected:
  using Task_future = std::future<Task_result>;
  using Futures_list = std::list<Task_future>;

  /// @brief add_dependency
  /// @param predecessor Predecessor task ID
  /// @param successor Sucessor task ID
  /// @details Add dependency function
  /// @retval true Dependency added successfully
  /// @retval false Failed to add dependency
  [[nodiscard]] bool add_dependency(const Task_id &predecessor,
                                    const Task_id &successor);

  /// @brief Enqueues a task identified by the \a task_id
  /// @details Function adding task to the scheduler
  /// @tparam FType Functor type
  /// @tparam Args Functor arguments parameter pack type
  /// @param schedule Pointer to task schedule
  /// @param task Functor to execute
  /// @param args Functor arguments
  /// @returns Task identifier
  /// @return True if enqueue succeeded, false otherwise
  template <typename FType, typename... Args>
  bool enqueue_internal(Task_schedule_ptr schedule, FType &&task,
                        Args &&...args);

  /// @brief end_execution
  void end_execution();

  /// @brief Waits for scheduler thread to finish its work
  void wait_for_scheduler_thread_to_stop();

  /// @brief Scheduler run function
  /// @details Function for the thread which schedules tasks for execution
  void run_main_thread();

  /// @brief Checks phase queues and return true if all are empty
  /// @return True if all phase queues are empty, false otherwise
  bool are_phase_queues_empty() const;

  /// @brief Checks whether any phase of the task is ready for execution
  /// @return True if any task phase is ready for execution, false otherwise
  bool is_task_phase_ready() const;

  /// @brief Function used to notify that task ended its execution
  /// @param schedule Task schedule, containing current phase information
  /// @param task_delay Delay of the whole task w.r.t. scheduler clock
  /// @param task_error True if enqueued task returned an error, false otherwise
  /// @param repeatable_task Shared repeatable task state
  /// @param current_thread_id THP id of the currently executing thread
  /// @return Task result
  template <typename Repeatable_task_type>
  Task_result callback(Task_schedule_ptr schedule,
                       Scheduler_clock::Time_point_t task_delay,
                       bool task_error,
                       std::shared_ptr<Repeatable_task_type> repeatable_task,
                       unsigned int current_thread_id);

  /// @brief Function used to notify that task phase ended its execution
  /// @param schedule Task schedule
  /// @param task_error True if enqueued task returned an error, false otherwise
  /// @param phase_id Executed phase sequence number
  /// @return Task result
  Task_result callback_phase(Task_schedule_ptr schedule, bool task_error,
                             unsigned int phase_id);

  /// @brief Function that enqueues next task phase
  /// @param schedule Task schedule, containing current phase information
  /// @param task_delay Delay of the whole task w.r.t. scheduler clock
  /// @param repeatable_task Shared repeatable task state for this phase
  /// @param current_thread_id THP id of the currently executing thread
  /// @param phase_id Enqueued phase sequence number
  template <typename Repeatable_task_type>
  bool enqueue_phase(Task_schedule_ptr schedule,
                     Scheduler_clock::Time_point_t task_delay,
                     std::shared_ptr<Repeatable_task_type> repeatable_task,
                     unsigned int current_thread_id, unsigned int phase_id);

  void wait_until_done();

  /// @brief Worker error handling function
  void handle_error();

  /// @brief Scheduler error handling function
  void scheduler_clean_up();

  /// @brief Enqueue helper
  /// @details Puts Scheduled task object into the task queue
  /// @param task Scheduled task rvalue reference
  /// @return True if enqueue succeeded
  bool enqueue_helper(Scheduled_task &&task);

  /// @brief Synchronizes the scheduler by actively waiting for finished tasks
  /// count
  /// @param print_checkpoint Prints diagnostics during waiting
  /// @param expected Expected scheduled task count after synchronization
  /// @param force When true, force synchronization regardless of errors / stop
  /// requests
  void synchronize_active(bool print_checkpoint, std::size_t expected,
                          bool force);

  /// @brief Waits for percent of tasks to finish. This function is called
  /// to reduce contention if clock queue is full. Instead of syncing
  /// after one pop(), we are allowing the scheduler to process portion
  /// of tasks. To boost performance, we wait only for a percent of tasks
  /// to finish. Partial waiting is available only for the active
  /// synchronization method, which is a default one
  /// @param print_checkpoint Prints diagnostics during waiting
  void synchronize_partial(bool print_checkpoint = false);

  /// @brief Actively waits for percent of tasks to finish.
  /// This function is called
  /// to reduce contention if clock queue is full. Instead of syncing
  /// after one pop(), we are allowing the scheduler to process portion
  /// of tasks. To boost performance, we wait only for a percent of tasks
  /// to finish
  /// @param print_checkpoint Prints diagnostics during waiting
  void synchronize_active_partial(bool print_checkpoint);

  Task_queue_type &get_phase_queue(Scheduler_clock_ptr phase_clock);
  Mutex_type &get_phase_queue_lock(Scheduler_clock_ptr phase_clock);

  /// @brief Helper function to check if the task at the top of the task queue
  /// is ready
  /// @return True in case a task is ready
  bool is_task_ready() const;

  /// Internal function to check if immediate stop was requested, externally or
  /// due to an error
  /// @return True if scheduler needs to stop now, false otherwise
  bool is_stop_requested() const;

 private:
  /// This is a queue which holds task ready for scheduling. The lower
  /// scheduled time for a task, the higher the task priority. This
  /// queue is used by 2 threads, task provider and scheduler main thread.
  /// We synchronize it with m_mutex_scheduler
  std::priority_queue<Scheduled_task> m_tasks;
  /// Variable indicating that scheduler thread finished its execution,
  /// protected with m_mutex_end
  bool m_scheduler_thread_active{true};

  /// Mutex protecting access to scheduler notifications
  mutable Mutex_type m_mutex_notification;
  /// Mutex protecting access to m_tasks, data structure populated by the
  /// enqueuing thread
  mutable Mutex_type m_mutex_tasks;
  /// This mutex protect access to phase
  mutable Mutex_type m_mutex_phases;
  /// @brief True if scheduler has been notified. Used to release mutex before
  /// calling notify on cv, which would cause a thread to wake up and block
  /// immediately
  /// @details Protected by m_mutex_scheduler
  std::atomic<bool> m_notification{false};
  /// Cv used by the scheduler main thread to wait on, when no task is
  /// available or tasks in m_task queue are not read to execute
  concurrency::Condition_variable m_cv_scheduler;
  /// Mutex used to finish execution of the Scheduler, protects
  /// m_scheduler_thread_active and it is used together with m_cv_end
  mutable Mutex_type m_mutex_end;
  /// cv used by a thread requesting scheduler to finish its work
  concurrency::Condition_variable m_cv_end;

  /// Internal map that keeps tasks waiting for dependencies defined in the
  /// Dependency Graph (distinct from time dependencies implemented in the
  /// Scheduler clock)
  std::unordered_map<Task_id, Scheduled_task> m_tasks_waiting_for_deps;
  /// Lock-free queue of task IDs that are ready for execution
  container::Integrals_lockfree_queue<uint64_t> m_notified_tasks;

  using Thread_type = concurrency::Thread;

  /// Scheduler main thread
  Thread_type m_scheduler_thread;
  /// Variable that checks whether scheduler should run (true) or
  /// end its execution (false)
  std::atomic<bool> m_scheduler_active{true};
  /// Ensures deinit() is executed only once
  std::atomic<bool> m_deinitialized{false};
  /// Variable that instructs scheduler to withdraw all work and stop
  std::atomic<bool> m_stop_now{false};

  /// The number of scheduled tasks
  std::atomic<std::size_t> m_scheduled_tasks_cnt{0};

  /// Scheduler clock
  Scheduler_clock_ptr m_scheduler_clock;

  /// Tasks may be divided into phases, each phase works according to a
  /// defined clock, registered with "register_phase" function. This map
  /// holds all registered phases: phase clock (key) and associated phase
  /// queue, in which task phases wait for their phase dependencies (defined
  /// by the clock) to finish. As an example, task may be divided into
  /// "apply" phase and "commit" phase. "commit" phase works with
  /// additional start-to-start commit order dependencies (register workers
  /// in order of their commit order).
  std::unordered_map<Scheduler_clock_ptr, Task_queue_type> m_task_phases;
  std::unordered_map<Scheduler_clock_ptr, Mutex_type> m_task_phases_locks;

  /// Task dependencies, used mainly to implement dependencies between
  /// events in one transaction or between commit event and parent transaction
  /// last event (to disallow a transaction to enter a commit when a parent
  /// transaction, i.e. commit order parent id, did not finish its execution
  Dependency_tracker_ptr m_dependencies;
  /// Thread pool with threads that execute tasks
  Thread_pool_ptr m_thread_pool;
  std::atomic_flag m_is_error = ATOMIC_FLAG_INIT;
  /// Observability variables
  std::atomic<Scheduler_status> m_status{Scheduler_status::idle};
  std::atomic<std::size_t> m_timeouts{0};
  /// @brief Instance id
  unsigned int m_instance_id{0};
  /// Statistics monitoring object for the current instance
  Statistics_instance_monitor_ref m_stat_monitor;
  /// Maximum allowed number of tasks active in the scheduler. Above this
  /// threshold, provider will block and wait (synchronize_partial)
  std::atomic<std::size_t> m_allowed_task_count{
      std::numeric_limits<std::size_t>::max()};
  /// Information on whether enqueueing thread is waiting for available
  /// task count limit. We allow for dirty reads.
  bool m_wait_for_task_limit{false};
  /// Stores unblock requests from external threads, will allow N task to enter
  /// phase
  std::atomic<bool> m_unblock_request{0};
  /// PSI parameters
  Scheduler_psi m_psi;
};

}  // namespace mysql::scheduler

#include "mysql/scheduler/scheduler_impl.hpp"

#endif  // MYSQL_SCHEDULER_SCHEDULER_H
