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

#ifndef MYSQL_SCHEDULER_THREAD_POOL_H
#define MYSQL_SCHEDULER_THREAD_POOL_H

#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <system_error>
#include <vector>
#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/locking_queue.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/concurrency/sync_bounded_queue.h"
#include "mysql/concurrency/thread.h"
#include "mysql/psi/mysql_cond.h"
#include "mysql/scheduler/thread_pool_psi.h"

namespace mysql::scheduler {

/// @brief MySQL wrapper for a condition variable, template which may be
/// specialized with a specific implementation of a condition variable, e.g.
/// MySQL condition variable, and satisfying the following requirements:
template <class T, std::size_t t_queue_size = 8192>
class Thread_pool {
 public:
  /// @brief Thread type
  using Thread = concurrency::Thread;
  /// @brief Runnable task type, future is obtained by the caller
  using Task_type = std::packaged_task<T(unsigned int)>;
  /// @brief Queue type used to synchronize a thread supplying task to this
  /// thread pool and worker threads. Worker threads are consumers of this queue
  /// type
  using Queue_type = concurrency::Sync_bounded_queue<Task_type, t_queue_size>;
  using St_key = mysql::concurrency::Stage_key;
  using Th_key = mysql::concurrency::Thread_key;
  using Thread_allocator = mysql::allocators::Allocator<Thread>;

  /// @brief Constructs a thread pool
  /// @param thread_num Numbers of threads in a thread pool, set as default to
  /// the number of concurrent threads supported by the implementation
  /// @param instance_id THP will gather statistics for this instance id
  /// @param psi_params PSI parameters
  Thread_pool(unsigned int thread_num = std::thread::hardware_concurrency(),
              unsigned int instance_id = 0, Thread_pool_psi psi_params = {});

  /// @brief Initializes the thread pool by starting worker threads.
  /// @retval false Success
  /// @retval true Failure
  [[nodiscard]] bool init();

  /// @brief Deinitializes the thread pool and joins all worker threads.
  void deinit();

  /// @brief Enqueues task for execution
  /// @param task Functor to be executed by the thread pool
  void enqueue(Task_type &&task);

  /// @brief Destructor
  virtual ~Thread_pool();

  /// @brief Notifies all threads to end execution in a gracious way.
  /// When finished, Thread_pool object is ready to be destroyed.
  /// This function is called in destructor.
  void end_execution();

  // disable copy-move semantics
  Thread_pool(Thread_pool &&) noexcept = delete;
  Thread_pool &operator=(Thread_pool &&) noexcept = delete;
  Thread_pool(const Thread_pool &) = delete;
  Thread_pool &operator=(const Thread_pool &) = delete;

  /// @brief Prints worker queue state, debug function
  /// @return String with internal queue state
  std::string print_queue_state() const;

  /// @brief Get an estimation of number of elements in the worker queue
  /// @return The number of elements in the worker queue
  std::size_t queue_size() const;

  /// @brief Obtains worker pool size
  /// @return The number of workers available in the pool
  std::size_t size() const;

 private:
  /// @brief Function executed by each thread
  void run_worker(unsigned int thread_id);

  /// @brief Checks whether worker thread construction failed.
  /// @param thread Worker thread object.
  /// @return True on failure, false otherwise.
  static bool has_creation_error(const Thread &thread);

  /// Thread container
  std::vector<Thread, Thread_allocator> m_workers;
  /// Variable indicating the end of execution, allows threads to stop in
  /// a gracious way
  std::unordered_map<unsigned int, std::atomic<bool>> m_end_execution;
  /// Enqueued, ready to be executed tasks
  Queue_type m_tasks;
  /// The number of threads in the thread pool
  unsigned int m_workers_num;
  /// @brief Instance id
  unsigned int m_instance_id{0};
  /// PSI parameters
  Thread_pool_psi m_psi;
  /// Whether init() completed successfully.
  bool m_initialized{false};
};

}  // namespace mysql::scheduler

#include "mysql/scheduler/thread_pool_impl.hpp"

#endif  // MYSQL_SCHEDULER_THREAD_POOL_H
