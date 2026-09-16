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

#ifndef MYSQL_SCHEDULER_TASK_REGISTRY_H
#define MYSQL_SCHEDULER_TASK_REGISTRY_H

#include <optional>
#include <vector>
#include "mysql/allocators/allocator.h"
#include "mysql/concurrency/cache_line_size.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/scheduler/logger_stream.h"
#include "mysql/scheduler/task_registry_psi.h"

namespace mysql::scheduler {

/// @brief This template maintains the registry of tasks, with specific
/// object types. Those objects can be accessed simultaneously by different
/// threads, as long as different threads access different task ids.
template <typename Task_id_type, typename Task_object_type>
class Task_registry {
 public:
  using Task_object = Task_object_type;
  using Task_object_ptr = std::unique_ptr<Task_object>;
  using Mt_key = concurrency::Mutex_key;
  using Mutex = concurrency::Mutex;

  static constexpr std::size_t cache_line_size =
      mysql::concurrency::detail::hardware_destructive_interference_size;

  static constexpr std::size_t default_capacity = 16384;

  Task_registry(Task_registry_psi psi_params = {})
      : m_tasks(mysql::allocators::Allocator<Entry_ptr>(
            psi_params.memory_resource)),
        m_psi(psi_params) {}

  void init(std::size_t capacity) {
    if (m_initialized) {
      deinit();
      m_initialized = false;
    }
    m_capacity = capacity;
    init_tasks();
  }

  Task_registry(std::size_t capacity, Task_registry_psi psi_params = {})
      : m_capacity(capacity),
        m_tasks(mysql::allocators::Allocator<Entry_ptr>(
            psi_params.memory_resource)),
        m_psi(psi_params) {
    init_tasks();
  }

  void deinit() {
    if (m_initialized) {
      m_initialized = false;
      m_tasks.clear();
    }
  }

  ~Task_registry() { deinit(); }

  /// @brief Calls 'func' on a given task object
  /// @param id Task id
  /// @param func function to be applied on a given registered task
  /// @return true in case task was active and apply succeeded, false otherwise
  template <typename T>
  [[nodiscard]] bool apply(Task_id_type id, T &&func) {
    assert(m_initialized);
    auto &entry_ptr = get_entry(id);
    std::scoped_lock scope_guard(entry_ptr->m_lock);
    if (!entry_ptr->m_active.test()) {
      return false;
    }
    func(entry_ptr->m_obj);
    return true;
  }

  /// @brief Unconditionally activates and calls 'func' on a given task object
  /// @param id Task id
  /// @param func function to be applied on a given registered task
  template <typename T>
  void activate_and_apply(Task_id_type id, T &&func) {
    assert(m_initialized);
    auto &entry_ptr = get_entry(id);
    std::scoped_lock scope_guard(entry_ptr->m_lock);
    entry_ptr->m_active.test_and_set();
    func(entry_ptr->m_obj);
  }

  [[nodiscard]] bool check_active(Task_id_type id) {
    assert(m_initialized);
    auto &entry_ptr = get_entry(id);
    return entry_ptr->m_active.test();
  }

  /// @brief Calls 'func' on each registered task object
  /// @details Don't call when a new task can be activated, also, this is
  /// ineffective, but we don't expect to call this
  /// @param func function to be applied on each, active task
  template <typename T>
  void apply_on_active(T &&func) {
    assert(m_initialized);
    for (std::size_t id = 0; id < m_tasks.size(); ++id) {
      auto &entry_ptr = m_tasks[id];
      // here: we don't lock if inactive
      if (entry_ptr->m_active.test()) {
        func(entry_ptr->m_obj);
      }
    }
  }

  /// @brief Function that registers a task, making it active
  [[nodiscard]] bool register_entry(Task_id_type id, Task_object &&obj) {
    assert(m_initialized);
    auto &entry_ptr = get_entry(id);
    std::scoped_lock scope_guard(entry_ptr->m_lock);
    if (entry_ptr->m_active.test()) {
      return false;
    }
    entry_ptr->m_id = id;
    entry_ptr->m_obj = std::move(obj);
    entry_ptr->m_active.test_and_set();
    return true;
  }

  /// @brief Function activates already registered entry
  [[nodiscard]] bool activate_entry(Task_id_type id) {
    return activate_or_wait(id);
  }

  /// @brief Function activates already registered entry, if entry is active
  /// it waits until its state changes to inactive.
  [[nodiscard]] bool activate_or_wait(Task_id_type id) {
    assert(m_initialized);
    auto &entry_ptr = get_entry(id);
    do {
      if (!entry_ptr->m_active.test_and_set()) break;
      std::this_thread::yield();
    } while (true);
    entry_ptr->m_id = id;
    return true;
  }

  /// @brief Gets ref of internal object
  /// @param id Task id
  /// @return copy of internal task object
  [[nodiscard]] const Task_object &get_ref(Task_id_type id) {
    assert(m_initialized);
    auto &entry_ptr = get_entry(id);
    entry_ptr->m_lock.lock();
    const Task_object &res = entry_ptr->m_obj;
    entry_ptr->m_lock.unlock();
    return res;
  }

  /// @brief Unprotected access to task object. Firstly obtain the guard
  [[nodiscard]] Task_object &get(Task_id_type id) {
    assert(m_initialized);
    auto &entry_ptr = get_entry(id);
    return entry_ptr->m_obj;
  }

  [[nodiscard]] auto scoped_lock(Task_id_type id) {
    assert(m_initialized);
    auto &entry_ptr = get_entry(id);
    return std::scoped_lock(entry_ptr->m_lock);
  }

  [[nodiscard]] Task_object &lock(Task_id_type id) {
    assert(m_initialized);
    auto &entry_ptr = get_entry(id);
    entry_ptr->m_lock.lock();
    return entry_ptr->m_obj;
  }

  void unlock(Task_id_type id) {
    assert(m_initialized);
    auto &entry_ptr = get_entry(id);
    entry_ptr->m_lock.unlock();
  }

  /// @brief Function that unregisters a task, making it inactive
  [[nodiscard]] bool deactivate_entry(Task_id_type id) {
    return deactivate_entry(id, [](Task_object &) -> bool { return true; });
  }

  /// @brief Function that unregisters a task, making it inactive, with functor
  /// @param id Id of the task
  /// @param func Functor to call on the task object before deactivation,
  /// returns bool indicating whether to deactivate
  template <typename Functor>
  [[nodiscard]] bool deactivate_entry(Task_id_type id, Functor &&func) {
    assert(m_initialized);
    auto &entry_ptr = get_entry(id);
    if (!entry_ptr->m_active.test()) {
      return false;
    }
    bool should_deactivate = func(entry_ptr->m_obj);
    if (should_deactivate) {
      entry_ptr->m_active.clear();
      return true;
    }
    return false;
  }

  /// @brief Function that unregisters a task, making it inactive, returns
  /// handled object if deactivation succeeded
  /// @return Handled object if deactivation succeeded, empty object otherwise.
  [[nodiscard]] std::optional<Task_object> get_and_deactivate_entry(
      Task_id_type id) {
    assert(m_initialized);
    auto &entry_ptr = get_entry(id);
    if (!entry_ptr->m_active.test() || entry_ptr->m_id != id) {
      return {};
    }
    auto object(entry_ptr->m_obj);
    entry_ptr->m_active.clear();
    return object;
  }

 private:
  void init_tasks() {
    assert(!m_initialized);
    for (std::size_t idx = 0; idx < m_capacity; ++idx) {
      m_tasks.emplace_back(new Entry(m_psi.key_mutex_entry));
    }
    assert(m_tasks.size() == m_capacity);
    m_initialized = true;
  }

  struct alignas(cache_line_size) Entry {
    Entry([[maybe_unused]] Mt_key key_mt_entry = 0)
        : m_lock(MYSQL_CONCURRENCY_DEFINE_MT_PSI_KEY(key_mt_entry)) {}
    Task_id_type m_id{0};
    Task_object m_obj;
    Mutex m_lock;
    std::atomic_flag m_active = ATOMIC_FLAG_INIT;
    char padding[(cache_line_size - (sizeof(m_id) + sizeof(m_obj) +
                                     sizeof(m_lock) + sizeof(m_active)) %
                                        cache_line_size) %
                 cache_line_size];
  };

  using Entry_ptr = std::unique_ptr<Entry>;

  [[nodiscard]] Entry_ptr &get_entry(Task_id_type id) {
    assert(m_initialized);
    auto hash_value = id.get() % m_tasks.size();
    assert(m_tasks[hash_value]);
    return m_tasks[hash_value];
  }

  std::size_t m_capacity{0};
  std::vector<Entry_ptr, mysql::allocators::Allocator<Entry_ptr>> m_tasks;
  bool m_initialized{false};
  Task_registry_psi m_psi;
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_TASK_REGISTRY_H
