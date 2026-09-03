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

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_IMR_RUNTIME_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_IMR_RUNTIME_H

#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "sql/changestreams/apply/storage/in_memory/imr_types.h"
#include "sql/changestreams/apply/storage/in_memory/queued_transaction_writer.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"

class Format_description_log_event;
class Master_info;
class Relay_log_info;

namespace mysql::csa {

class Streaming_event_sink;

/// @brief Channel-lifetime runtime object for the in-memory relay log.
///
/// Owns the channel's Trx_envelope_queue. Owned by the Imr_service runtime
/// map; Relay_log_info caches a non-owning pointer (Relay_log_info::imr()).
/// Imr_service::reconcile() creates, rebuilds and destroys it with the
/// channel's configuration, and ~Relay_log_info() removes it. Thread start and
/// stop only attach and detach the receiver and applier roles. An instance
/// exists exactly when the channel's persisted IMR selection is ON and the
/// channel is CSA-enabled.
///
/// The queue is held by value for a stable address, so every Trx_payload keeps
/// a plain back-pointer to it without reference counting. The runtime is
/// rebuilt only with both roles detached and the queue empty.
class Imr_runtime {
 public:
  /// @brief Build the runtime and its empty queue.
  ///
  /// @param config Validated configuration: memory limit and spill threshold.
  /// @param relay_log_dir The server's relay log directory; spill files go to
  ///        its @c \#in_memory_relaylog_temp_files subdirectory. Empty in unit
  ///        tests that never spill.
  /// @param memory_resource The channel's memory resource entry; nullptr when
  ///        resource tracking is off.
  /// @param channel_name Embedded in spill file names (see
  ///        Spill_file_writer::kFileNamePrefix). Empty for the default channel.
  /// @param spill_disk_resource The spill disk resource entry, registered with
  ///        @c relay_log_space_limit; nullptr when that limit is 0. A global,
  ///        so passed separately from @p config.
  /// @param channel_instance_id For the CSA statistics the commit hooks
  ///        update. 0 for unit tests.
  Imr_runtime(const Imr_config &config, std::string relay_log_dir,
              Resource_entry *memory_resource, std::string channel_name = {},
              Resource_entry *spill_disk_resource = nullptr,
              std::size_t channel_instance_id = 0)
      : m_queue(config.memory_limit, config.spill_threshold, memory_resource,
                std::move(relay_log_dir), std::move(channel_name),
                spill_disk_resource, channel_instance_id) {}

  /// @brief Destroy the runtime and its queue. Both roles must be detached and
  /// the receiver's open group closed (asserted), or a replication thread
  /// would keep using a freed queue.
  ~Imr_runtime();

  /// @brief The transitions Imr_service::reconcile() can take for a channel's
  /// runtime. Destroy and rebuild happen only with both roles detached.
  enum class Reconcile_action {
    kLeave,    ///< A matching runtime exists: leave it (queue identity intact).
    kDestroy,  ///< No runtime wanted: drop any existing one.
    kRebuild,  ///< A runtime exists but its bounds changed: replace it.
    kCreate    ///< A runtime is wanted and none exists yet: build one.
  };

  /// @brief Pure decision table for Imr_service::reconcile(), testable without
  /// a Relay_log_info.
  ///
  /// @param want_runtime The channel's persisted IMR selection is ON and it is
  ///        CSA-enabled.
  /// @param has_runtime A runtime already exists for the channel.
  /// @param config_matches The existing runtime's queue bounds equal the
  ///        requested ones. Ignored unless @p has_runtime.
  /// @return kDestroy when no runtime is wanted; otherwise kLeave (matching
  ///         runtime), kRebuild (different bounds) or kCreate (none exists).
  static Reconcile_action reconcile_action(bool want_runtime, bool has_runtime,
                                           bool config_matches);

  // The queue is neither copyable nor movable, and so is this runtime.
  Imr_runtime(const Imr_runtime &) = delete;
  Imr_runtime &operator=(const Imr_runtime &) = delete;
  Imr_runtime(Imr_runtime &&) = delete;
  Imr_runtime &operator=(Imr_runtime &&) = delete;

  using Scope = Trx_envelope_queue::Scope;

  /// @brief Attach the calling thread as the role's owner and clear the role's
  /// stop flag, so the role's first call is served.
  ///
  /// Called by the role's thread in its first run-lock section, under the role
  /// mutex (see m_role_mutex). A role has at most one owner: a second thread
  /// for the same role, possible in the start window, is refused and nothing
  /// changes.
  ///
  /// @retval false attached; the calling thread owns the role.
  /// @retval true  refused: the role is already attached.
  [[nodiscard]] bool attach_role(Scope scope);

  /// @brief Detach the role if the calling thread owns it, and reset the
  /// channel's volatile state if it was the last role attached.
  ///
  /// Called by the role's thread in its exit path, once it no longer uses the
  /// queue. Under the role mutex, in order: truncate the receiver's open group
  /// and drop the FDE snapshot (RECEIVER), or sweep the committed head
  /// (APPLIER); set the role's stop flag; clear the owner (last, so
  /// fully_detached() means the thread is done with the queue); reset() if
  /// fully_detached(). A no-op for a thread that does not own the role, so a
  /// refused thread's exit path leaves the owner's role alone.
  ///
  /// @param mi The channel's Master_info, for reset(). Unit tests pass
  ///        nullptr, which skips the reset.
  /// @retval true  this call detached the role.
  /// @retval false no-op: not attached, or not owned by the calling thread.
  bool detach_role(Scope scope, Master_info *mi);

  /// @brief Whether the calling thread owns the role. Lock-free.
  ///
  /// A relaxed load suffices: only the owner writes its own id, and a thread
  /// always sees its own stores. A recycled thread id cannot match: the owner
  /// clears the field in its exit section under its run lock, and the role's
  /// next thread is started by a START that takes that run lock afterwards.
  bool is_role_owner(Scope scope) const;

  /// @brief Whether a thread owns the role. For asserts and tests: a decision
  /// that acts on it must hold the role mutex or both run locks.
  bool is_attached(Scope scope) const;

  /// @brief Set the role's stop flag and wake its parked waiter
  /// (Trx_envelope_queue::stop()). Idempotent, takes no lock, and is safe
  /// before the role's thread joins. A request made before the role attaches
  /// is cleared by that attach.
  void request_stop(Scope scope);

  // ---------------------------------------------------------------------------
  // Receiver write path. Only the RECEIVER owner (the IO thread) may call these
  // (debug-asserted), because they use the open group without a lock. The
  // group's hold on its envelope keeps the sink alive, even after a worker has
  // committed the transaction. truncate_receiver_group() checks ownership in
  // all builds, because the exit path of a refused second IO thread reaches it.
  // ---------------------------------------------------------------------------

  /// @brief Take the FDE snapshot that every later open_transaction() hands
  /// out, as a copy of @p fde (see m_fde_snapshot).
  ///
  /// queue_event() calls it under LOCK_log right after it installs a source
  /// FDE event as the receiver's FDE. The source sends one at the start of
  /// every binlog file it sends, so each connection gets one before its first
  /// transaction.
  ///
  /// @param fde The receiver's new FDE; not referenced after the call.
  void set_receiver_fde(const Format_description_log_event &fde);

  /// @brief Open a transaction group at the GTID event; the new group's sink
  /// becomes receiver_sink(). A group still open is truncated first.
  ///
  /// @param fde The receiver's current FDE. It is not given to the
  ///        transaction, which gets m_fde_snapshot. It is used for a debug
  ///        check that the snapshot still matches, and is copied when no
  ///        snapshot was taken yet (asserted in debug builds).
  /// @param trx_length The transaction's declared size, from the GTID event.
  /// @param is_trx Whether the admitted unit is a real transaction.
  /// @retval false opened; receiver_sink() != nullptr.
  /// @retval true  a stop was requested during admission, or the spill file
  ///               could not be provisioned; receiver_sink() is null and the
  ///               cause is in last_write_failure().
  bool open_transaction(const Format_description_log_event &fde,
                        std::size_t trx_length, bool is_trx = true);

  /// @brief Whether open_transaction() for @p trx_length would be admitted now
  /// without waiting (Trx_envelope_queue::is_admissible()). Read-only.
  bool is_admissible(std::size_t trx_length) const {
    return m_queue.is_admissible(trx_length);
  }

  /// @brief Wait, with no lock held, until open_transaction() for
  /// @p trx_length would be admitted without waiting, a receiver stop is
  /// requested, or the IO thread is killed. Reserves nothing.
  ///
  /// queue_event() runs open_transaction() under LOCK_log, so the IO thread
  /// calls this first and never holds LOCK_log while waiting for the applier.
  /// If the transaction does not fit, a still-open group is truncated first, as
  /// open_transaction() would do, so the receiver never waits on bytes it holds
  /// itself. The wait is Trx_envelope_queue::wait_until_admissible().
  ///
  /// @param trx_length The declared size from the GTID event.
  /// @retval false open_transaction() would be admitted now.
  /// @retval true  stop requested or thread killed while waiting. After a stop
  ///               the next open_transaction() reports it; after a KILL
  ///               handle_slave_io() logs the relay-log-space error and exits
  ///               without queueing the event.
  bool wait_for_admission(std::size_t trx_length);

  /// @brief Append one received event's raw bytes to the open group. The
  /// terminal event also seals the group and drops the sink.
  ///
  /// @param buf Transient event bytes (copied in by the sink).
  /// @param len Number of bytes at @p buf.
  /// @param is_terminal Whether this event terminates (seals) the group.
  /// @retval false the event was appended.
  /// @retval true  no group is open, or a spill-file write failed (group
  ///               truncated, reason in last_write_failure()). Either way
  ///               receiver_sink() is null afterwards.
  bool append_event(const char *buf, std::size_t len, bool is_terminal);

  /// @brief Truncate the open group: mark its stream truncated and drop the
  /// sink. A no-op when no group is open or when the calling thread does not
  /// own RECEIVER.
  void truncate_receiver_group();

  /// @brief The open group's sink, or nullptr when no group is open.
  /// Non-owning view; the sink lives in the queued envelope's payload and is
  /// kept alive by the receiver hold on its envelope.
  Streaming_event_sink *receiver_sink() const {
    assert(is_role_owner(Scope::RECEIVER));
    return m_receiver_group.sink;
  }

  /// @brief Why the last open_transaction() / append_event() failed.
  const Receiver_write_failure &last_write_failure() const {
    assert(is_role_owner(Scope::RECEIVER));
    return m_last_write_failure;
  }

  /// @brief The owned queue; non-owning pointer, valid for this runtime's
  /// lifetime.
  Trx_envelope_queue *queue() { return &m_queue; }

  /// @brief Bytes currently held by memory-path payloads, for the
  /// @c In_Memory_Relay_Log_Space field of SHOW REPLICA STATUS.
  std::size_t bytes_used() const { return m_queue.bytes_used(); }

  /// @brief Number of transaction envelopes in the queue, for the
  /// @c In_Memory_Queue_Length field of SHOW REPLICA STATUS.
  std::size_t queue_length() const { return m_queue.queue_length(); }

  /// @brief Current number of the channel's spill temp files on disk, for the
  /// @c SPILL_FILE_COUNT metric column.
  std::size_t spill_file_count() const { return m_queue.spill_file_count(); }

  /// @brief Current total on-disk bytes of the channel's spill temp files, for
  /// the @c SPILL_BYTES_USED metric column.
  std::size_t spill_bytes_used() const { return m_queue.spill_bytes_used(); }

  /// @brief Whether neither role is attached. Owners are cleared only in the
  /// threads' exit paths, so for a caller holding both run locks this means
  /// both threads are stopped and nothing still observes the queue. The
  /// queue's stop flags are separate: they are set earlier to wake the threads.
  bool fully_detached() const {
    return !is_attached(Scope::RECEIVER) && !is_attached(Scope::APPLIER);
  }

  /// @brief Discard the channel's volatile receiver state: first the
  /// received-GTID state (clear_received_gtid_state()), then the queue
  /// contents and cursors.
  ///
  /// The two must go together: the queue is volatile, so after its envelopes
  /// are dropped Retrieved_Gtid_Set would otherwise advertise transactions that
  /// exist nowhere on this server (Received_transaction_set,
  /// channel_get_gtid_set_to_apply()).
  ///
  /// Precondition: fully_detached(). Called by detach_role() for the last role
  /// out, under the role mutex. detach_role(RECEIVER) has already closed the
  /// open group (asserted).
  ///
  /// @param mi The channel's Master_info (its rli supplies the
  ///        Retrieved_Gtid_Set).
  void reset(Master_info *mi);
  /// @brief Clear the channel's received-GTID state, which describes the
  /// in-memory queue.
  ///
  /// Clears, in Relay_log_info::purge_relay_logs() order, the transaction
  /// boundary parser, the receiver GTID monitoring info (under
  /// Master_info::data_lock), and Retrieved_Gtid_Set with its TSID map (under
  /// the RLI TSID write lock). They go together because clearing the TSID map
  /// invalidates the sidnos the monitoring info can hold.
  ///
  /// Precondition: both replication threads are stopped, and the caller holds
  /// neither Master_info::data_lock nor the RLI TSID lock. Called by reset()
  /// and by Imr_service::reconcile() on runtime creation.
  ///
  /// @param mi The channel's Master_info (its rli supplies the
  ///        Retrieved_Gtid_Set).
  static void clear_received_gtid_state(Master_info *mi);

 private:
  /// The channel's transaction queue. Held by value for a stable address; see
  /// the class note on payload back-pointers.
  Trx_envelope_queue m_queue;

  /// The role mutex: orders a role's attach against the other role's detach
  /// and last-out reset().
  ///
  /// Each thread attaches and detaches its role under its own run lock only.
  /// Without this mutex a thread could attach between the other thread's
  /// "last role out" check and its reset(), and the reset would clear state
  /// it already uses. Holding it across detach, fully_detached() and reset()
  /// also makes the check exact: exactly one of the two threads sees no role
  /// attached. Only attach_role() and detach_role() take it; request_stop()
  /// takes no lock.
  ///
  /// Lock order: a thread's run lock, then this mutex, then the locks taken
  /// by detach_role() and reset() (Master_info::data_lock, the relay log TSID
  /// lock, the queue's own mutexes). Take it nowhere else.
  std::mutex m_role_mutex;

  /// The receiver's open group (envelope and sink), both null outside a
  /// group. RECEIVER owner only, no lock; detach_role(RECEIVER) closes it
  /// before it clears the owner. The sink is owned by the envelope's payload.
  Open_group m_receiver_group;

  /// Why the last receiver write call failed. RECEIVER owner only, no lock.
  Receiver_write_failure m_last_write_failure;

  /// The read-only copy of the receiver's FDE that every new transaction gets.
  ///
  /// Workers decode without LOCK_log, while the receiver's FDE is written in
  /// place under LOCK_log: Format_description_log_event::write() switches its
  /// checksum algorithm, and queue_event() overrides it to decode a source
  /// Rotate. So transactions get a copy, never the receiver's object.
  ///
  /// set_receiver_fde() takes it for every source FDE event. Those events are
  /// the only way the receiver's decoding fields change between transactions,
  /// which open_transaction() checks in debug builds. The FDE built at connect
  /// only decodes the fake Rotate and the first FDE event the source sends; no
  /// transaction is opened under it.
  ///
  /// RECEIVER owner only, no lock of its own: set under LOCK_log, released in
  /// detach_role(RECEIVER).
  std::shared_ptr<Format_description_log_event> m_fde_snapshot;

  /// The thread that owns each role, or std::thread::id{} when detached.
  /// Written only by the owning thread, under the role mutex, with release
  /// stores (attach_role(), detach_role()); atomic so other threads can read.
  std::atomic<std::thread::id> m_receiver_owner{};
  std::atomic<std::thread::id> m_applier_owner{};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_IMR_RUNTIME_H
