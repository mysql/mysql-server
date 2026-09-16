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

#ifndef MYSQL_CSA_RESOURCE_MONITOR_H
#define MYSQL_CSA_RESOURCE_MONITOR_H

#include <mysql/psi/psi_stage.h>
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include "mysql/scheduler/constants.h"

namespace mysql::csa {

struct Resource_entry;
struct Locked_resource;

class Resource_instance_monitor;
using Resource_instance_monitor_ref =
    std::reference_wrapper<Resource_instance_monitor>;

/// @brief Manages per-channel resources for the Change Stream Applier (CSA).
/// Allows registering named resources and locking/releasing them.
/// @details This class provides a registry for named resources, each with a
/// configurable limit on maximum available units. Resources can be acquired
/// (locked) using blocking (infinite wait) or non-blocking methods (try lock),
/// and released. It is designed for concurrent access, using atomic operations
/// for counts and spinning with sleep for waiting. One instance is
/// created per channel via Resource_monitor::get(instance_id).
///
/// Usage assumptions:
/// 1. To avoid deadlocks between locking applier resources by different threads
/// and transaction locks, a resource for a transaction should be locked before
/// this transaction starts.
/// 2. All resources must be registered before first call to concurrent methods
/// (register_resource is non-concurrent)
///
/// Main functions:
/// - register_resource - for named resource registraction
/// - acquire_resource - get resource lock object (releases at destruction)
///
/// Key concepts of resource acquisition:
/// - Each resource has a 'limit' (maximum available units) set at registration.
/// - 'available' tracks current free units.
/// - For requests <= limit, standard semaphore-like behavior (wait for
/// available >= amount).
/// - For requests > limit, wait for available equal to limit
/// - All waiting uses spinning with a constant sleep (1ms) to avoid
/// indefinite blocks.
/// - Suitable for resources like channel memory limit in the applier.
///
class Resource_instance_monitor {
 public:
  /// @brief Registers a named resource with the given limit (maximum
  /// available)
  /// @param name The name of the resource.
  /// @param limit The maximum available count for this resource.
  /// @param stage Optional PSI stage to set during waiting for lock.
  void register_resource(const std::string &name, std::size_t limit,
                         PSI_stage_info *stage = nullptr);

  /// @brief Function locks resource and returns resource guard that will
  /// release the resource if locked upon destruction
  /// @param name The name of the resource.
  /// @param amount The amount to lock.
  /// @return Locked_resource object
  Locked_resource acquire_resource(const std::string &name, std::size_t amount);

  /// @brief Releases the specified amount of the named resource. Wait-free.
  /// @param name The name of the resource.
  /// @param amount The amount to release.
  void release_resource(const std::string &name, std::size_t amount);

  /// @brief Waits until the requested resource amount is available or returns
  /// an error. Wait-free in case resource is available in the first check.
  /// @param name The name of the resource.
  /// @param amount The amount to lock.
  /// @return True when successfully locked. False if exited before obtaining.
  bool lock_resource(const std::string &name, std::size_t amount);

  /// @brief Releases the specified amount of the named resource. Wait-free.
  /// @param resource Resource handle
  /// @param amount The amount to release.
  static void release_resource(Resource_entry &resource, std::size_t amount);

  /// @brief Waits until the requested resource amount is available or returns
  /// an error. Wait-free in case resource is available in the first check.
  /// @param resource Resource handle
  /// @param amount The amount to lock.
  /// @return True when successfully locked. False if exited before obtaining.
  static bool lock_resource(Resource_entry &resource, std::size_t amount);

  Resource_instance_monitor() = default;

 private:
  /// Internal function that optimistically tries to lock resource without
  /// spinning. Performs a single atomic attempt: subtracts the amount and
  /// succeeds if the previous available was >= amount or == limit (even for
  /// amount > limit). Wait-free; rolls back on failure.
  /// @param resource_entry Resource handle
  /// @param amount Amount of the resource to lock
  /// @return True on success, false when could not lock resource.
  static bool try_lock_resource_internal(Resource_entry &resource_entry,
                                         std::size_t amount);

  using Resource_map =
      std::unordered_map<std::string, std::unique_ptr<Resource_entry>>;
  Resource_map m_resources;
};

/// @brief Singleton Resource Monitor for all channels/instances.
/// Provides one Resource_instance_monitor per channel.
class Resource_monitor {
 public:
  /// @brief Get the Resource_instance_monitor for the given channel/instance
  /// ID.
  /// @param instance_id The ID of the channel/instance.
  /// @return Reference to the Resource_instance_monitor.
  static Resource_instance_monitor &get(std::size_t instance_id);

 protected:
  Resource_monitor() = default;

  using Instances_map = std::array<Resource_instance_monitor,
                                   scheduler::Constants::max_instances>;

  /// Instances for separate channels, may be accessed concurrently between
  /// channel threads
  static Instances_map m_instances;
};

/// Resource guard class, used to check if resource was successfully locked and
/// release resource in destructor
struct Locked_resource {
 public:
  /// Default construct
  Locked_resource() = default;
  /// Construct
  /// @param resource Resource handle
  /// @param requested The requested amount
  /// @param locked True if successfully locked
  Locked_resource(Resource_entry *resource, std::size_t requested, bool locked);
  /// Check if resource is locked
  bool is_locked() const;
  /// Unlocks resource if locked
  ~Locked_resource();

  // disable copy-move semantics
  Locked_resource(Locked_resource &&) noexcept = delete;
  Locked_resource &operator=(Locked_resource &&) noexcept = delete;
  Locked_resource(const Locked_resource &) = delete;
  Locked_resource &operator=(const Locked_resource &) = delete;

 private:
  /// Resource handle
  Resource_entry *m_resource{nullptr};
  /// Requested amount
  std::size_t m_requested_amount{0};
  /// True if successfully locked
  bool m_is_locked{false};
};

/// @brief Entry for a named resource, managing available count and
/// synchronization.
struct Resource_entry {
  /// Currently available amount of the resource
  std::atomic<long long> m_available{0};
  /// Resource limit
  std::atomic<long long> m_limit{0};
  /// Waiting stage set only if the requested amount is higher than currently
  /// available and thread waits for the resource to be released
  PSI_stage_info *m_stage{nullptr};
  /// Default constructible
  Resource_entry() = default;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_RESOURCE_MONITOR_H
