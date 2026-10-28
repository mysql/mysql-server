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

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_IMR_SERVICE_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_IMR_SERVICE_H

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>

class Master_info;
class Relay_log_info;

namespace mysql::csa {

class Imr_runtime;

/// @brief Outcome of validating a requested in-memory relay-log configuration
/// against the accepted option bounds.
///
/// When @c ok is false, the caller raises @c error_code with @c reason as its
/// single %s argument, the client-visible error of a failed CHANGE REPLICATION
/// SOURCE. When @c ok is true, @c error_code is 0 and @c reason is empty.
struct Imr_options_validation {
  /// Whether the requested (effective, post-statement) bounds are acceptable.
  bool ok{true};
  /// The client-facing error code to raise when @c ok is false; 0 otherwise.
  /// Always ER_CRST_IN_MEMORY_RELAYLOG_INVALID_CONFIG on failure.
  int error_code{0};
  /// The single %s argument for @c error_code, naming the offending value(s)
  /// and the violated bound; empty when @c ok is true.
  std::string reason;
};

/// @brief Server-lifetime service for the in-memory relay log.
///
/// Holds the IMR option bounds and their validation (validate_options()), the
/// startup spill-storage setup (init_spill_storage()) and the map that owns
/// each channel's Imr_runtime. A single instance, reached through the global
/// @c imr_service handle (sql/rpl_replica.h), lives from init_replica() to
/// shutdown.
///
/// Never consulted on a per-event path: the receiver and applier reach their
/// runtime through rli->imr(). The map is mutated only under the channel_map
/// write lock (see m_runtimes).
class Imr_service {
 public:
  /// @brief Construct the single service instance. Reached only through the
  /// global @c imr_service handle.
  Imr_service() = default;
  /// @brief Destroy the service. Every runtime should already be removed with
  /// its channel. Out-of-line so Imr_runtime is a complete type here.
  ~Imr_service();
  Imr_service(const Imr_service &) = delete;
  Imr_service &operator=(const Imr_service &) = delete;

  /// @name Option bounds (bytes)
  /// Accepted ranges for the IMR options:
  ///   IN_MEMORY_RELAYLOG_LIMIT           : [kLimitMin, kLimitMax]
  ///   IN_MEMORY_RELAYLOG_SPILL_THRESHOLD : [kSpillThresholdMin, LIMIT)
  /// unsigned int so kLimitMax (UINT_MAX) is the same on LP64 and LLP64 and
  /// fits the persisted ulong. Callers pass unsigned long long so oversize
  /// requests are rejected, not truncated.
  /// @{
  static constexpr unsigned int kLimitMin = 32U * 1024 * 1024;  // 32 MiB
  static constexpr unsigned int kLimitMax = 4294967295U;        // 4 GiB-1
  static constexpr unsigned int kSpillThresholdMin = 8U * 1024 * 1024;  // 8 MiB
  /// @}

  /// @brief Validate a requested pair of in-memory relay-log bounds.
  ///
  /// The caller passes the effective post-statement values (the requested
  /// value, or the persisted one when the option is unspecified), so an option
  /// set on its own is still checked against its peer.
  ///
  /// Checks, in order, first failure wins:
  ///   1. When @p limit_specified, @p effective_limit must lie within
  ///      [kLimitMin, kLimitMax].
  ///   2. When @p spill_threshold_specified, @p effective_spill_threshold must
  ///      be at least kSpillThresholdMin.
  ///   3. @p effective_spill_threshold must be strictly below
  ///      @p effective_limit.
  ///
  /// @param limit_specified Whether the statement set IN_MEMORY_RELAYLOG_LIMIT.
  /// @param effective_limit The post-statement memory limit in bytes.
  /// @param spill_threshold_specified Whether the statement set
  ///        IN_MEMORY_RELAYLOG_SPILL_THRESHOLD.
  /// @param effective_spill_threshold The post-statement spill threshold in
  ///        bytes.
  /// @return An Imr_options_validation; on failure it carries the client error
  ///         code and reason.
  static Imr_options_validation validate_options(
      bool limit_specified, unsigned long long effective_limit,
      bool spill_threshold_specified,
      unsigned long long effective_spill_threshold);

  // ---------------------------------------------------------------------------
  // Runtime lifecycle. reconcile() and remove() are the only mutators of
  // m_runtimes; both keep rli->imr() in sync and require the channel_map write
  // lock.
  // ---------------------------------------------------------------------------

  /// @brief Create, destroy or rebuild the channel's runtime to match its
  /// persisted IMR selection and memory bounds.
  ///
  /// Called at mi/rli init (also at START REPLICA) and at the end of CHANGE
  /// REPLICATION SOURCE, under the channel_map write lock. A thread may be
  /// running then, but only kLeave is possible: every option that changes the
  /// IMR selection or bounds requires both threads to be stopped. Never
  /// destroys or rebuilds a runtime with a role attached. Creating a runtime
  /// clears the channel's Retrieved_Gtid_Set, because the new queue is empty.
  ///
  /// @param rli The channel's Relay_log_info: the source of the persisted
  ///        selection, bounds, CSA state and instance id. Must be non-null.
  void reconcile(Relay_log_info *rli);

  /// @brief Remove the channel's runtime: clear rli->imr() and erase the map
  /// entry. Called only from ~Relay_log_info(), while @p rli is alive and both
  /// roles are detached, under the channel_map write lock. A no-op on a
  /// channel with no runtime.
  ///
  /// @param rli The channel's Relay_log_info. Null is tolerated (no-op).
  void remove(Relay_log_info *rli);

  // ---------------------------------------------------------------------------
  // Per-thread attach/detach, forwarded to the channel's Imr_runtime (see
  // Imr_runtime::attach_role() and detach_role()). No-ops on a classic channel
  // (no runtime).
  // ---------------------------------------------------------------------------

  /// @brief Attach the IO thread as the receiver (Imr_runtime::attach_role()).
  /// Called by the IO thread right after init_replica_thread(), in the
  /// run-lock section that sets slave_running (handle_slave_io()).
  ///
  /// @retval false attached, or a classic channel (no runtime).
  /// @retval true  refused: another thread owns the receiver role. Nothing
  ///               changed; the caller reports and exits.
  [[nodiscard]] static bool attach_receiver(Master_info *mi);

  /// @brief Request the receiver to stop (Imr_runtime::request_stop()). Takes
  /// no lock, so it is safe without mi->run_lock and before the IO thread has
  /// joined. The caller sets abort_slave first, since a later attach clears
  /// this request.
  static void request_receiver_stop(Master_info *mi);

  /// @brief Detach the receiver role (Imr_runtime::detach_role()). Called by
  /// the IO thread in its exit path, under mi->run_lock, right after it clears
  /// slave_running. A no-op unless the calling thread owns the role.
  static void detach_receiver(Master_info *mi);

  /// @brief Attach the SQL thread as the applier (Imr_runtime::attach_role()).
  /// Called by the SQL thread right after init_replica_thread(), in the
  /// run-lock section that sets slave_running (handle_slave_sql()).
  ///
  /// @retval false attached, or a classic channel (no runtime).
  /// @retval true  refused: another thread owns the applier role. Nothing
  ///               changed; the caller reports and exits.
  [[nodiscard]] static bool attach_applier(Master_info *mi);

  /// @brief Request the applier to stop (Imr_runtime::request_stop()). Takes
  /// no lock, so it is safe without rli->run_lock and before the SQL thread
  /// has joined. The caller sets abort_slave first, since a later attach
  /// clears this request.
  static void request_applier_stop(Master_info *mi);

  /// @brief Detach the applier role (Imr_runtime::detach_role()). Called by
  /// the SQL thread in its exit path, after its CSA workers are gone, under
  /// rli->run_lock, right after it clears slave_running. A no-op unless the
  /// calling thread owns the role.
  static void detach_applier(Master_info *mi);

  // ---------------------------------------------------------------------------
  // Spill-storage setup, run once at server start.
  // ---------------------------------------------------------------------------

  /// @brief Provision the server's spill storage, from init_replica() after
  /// channel configuration is loaded and before any receiver can spill.
  ///
  /// Purges orphan spill files (prefix @c imr_sp_, non-recursive; a missing
  /// directory is not an error) from the server's single spill directory under
  /// the relay_log_basename directory, then ensures that directory exists,
  /// whether or not any channel selects IMR. If the purge cannot list the
  /// directory or delete a stale file, spill_cleanup_failed() becomes true; if
  /// the directory is unusable, spill_dir_unusable() does. Either way the cause
  /// is logged and START of an IMR channel is rejected until the server is
  /// restarted with the problem fixed. The server itself still starts.
  ///
  /// The caller must hold channel_map's write lock (init_replica() does).
  void init_spill_storage();

  /// @brief Whether the startup purge could not list the spill directory or
  /// delete a stale spill file. check_replica_configuration_errors() then
  /// rejects starting an IMR channel's threads. Set once, before any
  /// replication thread or client session exists, so readers need no lock.
  bool spill_cleanup_failed() const { return m_spill_cleanup_failed; }

  /// @brief Whether init_spill_storage() found the spill directory unusable.
  /// Set once at startup, with the same effect as spill_cleanup_failed().
  bool spill_dir_unusable() const { return m_spill_dir_unusable; }

  /// @brief Number of channels that currently have a runtime. Used to check
  /// that shutdown removed every runtime with its channel.
  std::size_t runtime_count() const;

 private:
  /// Owning map of per-channel runtimes, keyed by channel_instance_id. A
  /// runtime is destroyed by erasing its entry, so no entry holds a null
  /// pointer. Protected by the channel_map write lock (each mutator asserts
  /// it); the receiver, applier and status paths use rli->imr() instead.
  std::unordered_map<std::size_t, std::unique_ptr<Imr_runtime>> m_runtimes;

  /// See spill_cleanup_failed().
  bool m_spill_cleanup_failed{false};

  /// See spill_dir_unusable().
  bool m_spill_dir_unusable{false};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_IMR_SERVICE_H
