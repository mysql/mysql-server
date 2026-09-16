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

#ifndef MYSQL_SCHEDULER_TASK_REGISTRY_MULTI_H
#define MYSQL_SCHEDULER_TASK_REGISTRY_MULTI_H

#include <atomic>
#include <cassert>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include "mysql/concurrency/cache_line_size.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/concurrency/spin_lock_mutex.h"
#include "mysql/scheduler/task_id.h"

namespace mysql::scheduler {

/// @brief This template class provides a concurrent registry for tasks that
/// handles hash conflicts by allowing multiple task entries to coexist in the
/// same hash bucket. It implements thread-safe access to task objects
/// using a bucketed hash table with locks per bucket.
///
/// Threads do not contend when they operate on separate buckets. When a hash
/// conflict occurs, threads use spin locks to lock requested resources
/// (bucket).
///
/// Spin locks excel when locks are held for very brief periods, such as in this
/// registry, where operations like checking task activation or updating
/// entries are quick atomic operations. The spin lock avoids expensive context
/// switches that std::mutex would incur.
///
/// Each bucket (determined by task ID hash) contains an unordered_map mapping
/// task IDs to their associated objects and state. This design resolves
/// collisions that could occur with simple hashing, ensuring that tasks with
/// IDs mapping to the same bucket index can be stored and accessed efficiently.
///
/// Key features:
/// - Hash conflict resolution: Multiple tasks per bucket using
/// std::unordered_map.
/// - Thread-safe: Each bucket has its own mutex.
/// - Limited API: Only essential operations for task management.
/// - Template-based: Supports various task ID and object types.
///
/// Usage example:
/// @code
/// Task_registry_multi<Task_id, Task_info> registry(1000);
/// registry.activate(task_id, Task_info{...});
/// registry.apply(task_id, [](Task_info& info) { /* operate on info */ });
/// registry.deactivate(task_id);
/// @endcode
template <typename Task_id_type, typename Task_object_type>
class Task_registry_multi {
 public:
  using Task_object = Task_object_type;
  using Mutex = concurrency::Spin_lock_mutex;

  static constexpr std::size_t default_capacity = 16384;

  /// @brief Constructs buckets using defined capacity
  Task_registry_multi(std::size_t capacity = default_capacity) {
    m_buckets.reserve(capacity);
    for (std::size_t idx = 0; idx < capacity; ++idx) {
      m_buckets.emplace_back(std::make_unique<Bucket>());
    }
  }

  /// Destruct registry, deinitialize if applicable
  ~Task_registry_multi() = default;

  /// @brief Calls 'func' on a given task object if active
  /// @param id Task id
  /// @param func function to be applied on a given registered task
  /// @return true if task was found and active, false otherwise
  template <typename T>
  [[nodiscard]] bool apply(Task_id_type id, T &&func) {
    auto &bucket = get_bucket(id);
    std::scoped_lock scope_guard(bucket->m_lock);
    auto it = bucket->m_entries.find(id);
    if (it != bucket->m_entries.end()) {
      func(it->second.m_obj);
      return true;
    }
    return false;
  }

  /// @brief Registers and activates a task
  /// @param id Task id
  /// @param obj Task object
  /// @return true if the previous state was inactive (i.e., the task was newly
  /// registered), false if the previous state was active (task already present)
  /// @throws std::bad_alloc If memory allocation for the new entry fails
  [[nodiscard]] bool activate(Task_id_type id, Task_object &&obj) {
    auto &bucket = get_bucket(id);
    std::scoped_lock scope_guard(bucket->m_lock);
    auto map_entry = bucket->m_entries.emplace(
        std::piecewise_construct, std::forward_as_tuple(id),
        std::forward_as_tuple(std::move(obj)));
    if (map_entry.second) {
      bucket->m_num_entries.fetch_add(1, std::memory_order_relaxed);
    }
    return map_entry.second;
  }

  /// @brief Deactivates a task
  /// @param id Task id
  /// @return true if deactivated, false if not found or not active
  [[nodiscard]] bool deactivate(Task_id_type id) {
    auto &bucket = get_bucket(id);
    std::scoped_lock scope_guard(bucket->m_lock);
    size_t erased = bucket->m_entries.erase(id);
    if (erased != 0) {
      bucket->m_num_entries.fetch_sub(1, std::memory_order_relaxed);
    }
    return erased != 0;
  }

  /// @brief Checks if the bucket for the given task ID is active (contains any
  /// tasks)
  /// @param id Task id
  /// @return true if bucket has at least one active task, false otherwise
  [[nodiscard]] bool bucket_active(Task_id_type id) const {
    size_t hash_value = id.get() % m_buckets.size();
    return m_buckets[hash_value]->m_num_entries.load(
               std::memory_order_relaxed) > 0;
  }

 private:
  /// Each bucket is capable of handling many subentries. This structure
  /// represents a bucket subentry, which is visible to the caller as
  /// registry entry. Access to entry is protected with a bucket lock - all
  /// entries in the same bucket share the same lock. Entries are put into
  /// the same bucket when they share the same registry hash (hash conflict).
  struct Sub_entry {
    /// Internal object, type defined by the caller
    Task_object m_obj;
    /// Construct from object
    /// @param obj Object handled by the Task_registry_multi
    /// Access to the object is protected with registry bucket lock
    /// (Subentries share the same lock)
    Sub_entry(Task_object &&obj) : m_obj(std::move(obj)) {}
  };

  /// Represents a bucket, capable of handling many subentries. Access to the
  /// bucket is protected with internal lock
  struct alignas(
      concurrency::detail::hardware_destructive_interference_size) Bucket {
    Bucket() : m_lock(), m_num_entries(0) {}
    std::unordered_map<Task_id_type, Sub_entry> m_entries;
    Mutex m_lock;
    std::atomic<size_t> m_num_entries{0};
  };

  using Bucket_ptr = std::unique_ptr<Bucket>;

  /// @brief Obtains the bucket handle handling a given id, for internal use
  /// @param id Task id
  /// @return Bucket handle
  [[nodiscard]] Bucket_ptr &get_bucket(Task_id_type id) {
    auto hash_value = id.get() % m_buckets.size();
    assert(m_buckets[hash_value]);
    return m_buckets[hash_value];
  }

  /// Buckets, synchronized independently
  std::vector<Bucket_ptr> m_buckets;
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_TASK_REGISTRY_MULTI_H
