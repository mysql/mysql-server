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

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_TRX_ENVELOPE_QUEUE_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_TRX_ENVELOPE_QUEUE_H

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

#include "sql/changestreams/apply/storage/in_memory/imr_types.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"

class Format_description_log_event;

namespace mysql::csa {

struct Resource_entry;

/// @brief Per-channel in-memory FIFO of transaction envelopes with polling
/// admission control against the channel's resource entries.
///
/// The queue owns its envelopes in a @c std::deque in enqueue order and tracks
/// progress with three monotonic cursors: @c commit_seqno (the committed head),
/// @c dispatch_seqno (the next envelope to hand to a worker), and
/// @c insert_seqno (the tail). A single mutex, @c m_queue_mutex, guards the
/// deque, the cursors, and all structural changes.
///
/// Each transaction's commit is done by its worker under the per-envelope
/// mutex, so a committing worker never contends on @c m_queue_mutex.
class Trx_envelope_queue {
 public:
  /// @brief The replication role a lifecycle operation targets.
  ///
  /// The receiver waits in admission and the applier in dispatch_next(), so
  /// stop()/resume() act on one role without disturbing the other.
  enum class Scope { RECEIVER, APPLIER };

  /// @brief Construct the queue with its per-channel memory bounds.
  ///
  /// @param memory_limit Hard bound on bytes held by memory-path payloads
  ///        (IN_MEMORY_RELAYLOG_LIMIT).
  /// @param spill_threshold Transactions larger than this take the spill path
  ///        (IN_MEMORY_RELAYLOG_SPILL_THRESHOLD).
  /// @param relay_log_dir The server's relay log directory; spill files go in
  ///        its @c \#in_memory_relaylog_temp_files subdirectory. Empty only
  ///        for callers that never spill (memory-only unit tests).
  /// @param channel_name The owning channel's name, embedded in spill file
  ///        names (see Spill_file_writer::kFileNamePrefix).
  /// @param spill_disk_resource The channel's @c in_memory_relaylog_spill_disk
  ///        entry (relay_log_space_limit), used by spill-path admission (see
  ///        acquire_admission()). nullptr means relay_log_space_limit = 0.
  ///        Same lifetime rule as @p memory_resource.
  /// @param memory_resource The channel's @c in_memory_relaylog_memory entry,
  ///        registered with @p memory_limit. nullptr means tracking is off: no
  ///        limit and bytes_used() reads 0. Must outlive the queue; registering
  ///        the same name again frees the entry, so do that only when the
  ///        queue is being destroyed and holds no reservation.
  /// @param channel_instance_id The channel's instance id, used by the commit
  ///        hook to count the transaction in CSA statistics. 0 for unit tests.
  Trx_envelope_queue(std::size_t memory_limit, std::size_t spill_threshold,
                     Resource_entry *memory_resource,
                     std::string relay_log_dir = {},
                     std::string channel_name = {},
                     Resource_entry *spill_disk_resource = nullptr,
                     std::size_t channel_instance_id = 0);

  Trx_envelope_queue(const Trx_envelope_queue &) = delete;
  Trx_envelope_queue &operator=(const Trx_envelope_queue &) = delete;
  Trx_envelope_queue(Trx_envelope_queue &&) = delete;
  Trx_envelope_queue &operator=(Trx_envelope_queue &&) = delete;

  /// @brief Destroy the queue. It must outlive its payloads, which call
  /// release_bytes() on it when destroyed.
  ~Trx_envelope_queue();

  // --- Producer (receiver) admission API ---

  /// @brief Choose the store path for a transaction at the GTID event.
  ///
  /// @param trx_length The transaction's declared byte size.
  /// @retval Envelope_path::SPILL when @p trx_length exceeds the spill
  ///         threshold.
  /// @retval Envelope_path::MEMORY otherwise. The memory limit is applied in
  ///         acquire_admission(), not here.
  Envelope_path classify(std::size_t trx_length) const;

  /// @brief Reserve budget for a transaction of @p trx_length on @p path, or
  /// reject it once a receiver stop is requested.
  ///
  /// Memory path: a lock-free reserve against the memory entry. When the limit
  /// is reached it polls every 1 ms on the receiver-stop flag; it does not use
  /// the monitor's blocking wait, which ends only on THD kill, not on
  /// STOP REPLICA IO_THREAD.
  /// Spill path: reserves against the spill disk entry with the soft rule
  /// (Resource_instance_monitor::try_lock_resource_soft()), polling the same
  /// way. The reservation goes to the Spill_file_writer, which releases it
  /// after deleting the file. Spill waits are not counted in block metrics.
  ///
  /// @param trx_length The transaction's declared byte size.
  /// @param path The store path chosen by classify().
  /// @retval false admitted; @p trx_length is reserved unless the path's limit
  ///         is off.
  /// @retval true  a receiver stop was requested; nothing was reserved.
  bool acquire_admission(std::size_t trx_length, Envelope_path path);

  /// @brief Whether acquire_admission() would admit @p trx_length now.
  /// Applies the path's rule without reserving and ignores the stop flag;
  /// true when the path's limit is off.
  ///
  /// @param trx_length The transaction's declared byte size.
  bool is_admissible(std::size_t trx_length) const;

  /// @brief Wait, reserving nothing and holding no lock, until
  /// acquire_admission() would admit @p trx_length.
  ///
  /// The receiver calls it before taking the relay log's LOCK_log, so it never
  /// waits for the applier while holding that lock. It polls every 1 ms and
  /// ends on a receiver stop or when the calling THD is killed.
  ///
  /// Single-reserver invariant: only the IO thread reserves on the memory and
  /// spill disk entries (the add_bytes() test hook aside); other threads only
  /// release, and classify() picks the same path for the same length. So once
  /// this returns false, the next acquire_admission() for the same
  /// @p trx_length succeeds on its first attempt.
  ///
  /// @param trx_length The transaction's declared byte size.
  /// @retval false acquire_admission() would admit it now.
  /// @retval true  a receiver stop was requested, or the thread was killed.
  bool wait_until_admissible(std::size_t trx_length) const;

#ifndef NDEBUG
  /// @brief Debug only: number of 1 ms polls acquire_admission() made because
  /// a reservation did not fit. Unchanged after a successful
  /// wait_until_admissible().
  std::size_t admission_polls() const { return m_admission_polls.load(); }
#endif

  /// @brief Admit a transaction at the GTID event and append its envelope.
  ///
  /// Admission (see acquire_admission()) runs while building the payload,
  /// outside @c m_queue_mutex. The envelope is then appended under the lock
  /// and a consumer blocked in dispatch_next() is woken.
  ///
  /// @param trx_length The transaction's declared byte size.
  /// @param is_trx Whether the unit is a real transaction (true) or a
  ///        standalone/administrative event group (false).
  /// @param fde The active Format_description_log_event; shared ownership keeps
  ///        it alive after a later FD event. Must be non-null.
  /// @param receiver_held True when the receiver writes the events afterwards
  ///        (open_transaction()). The envelope is held from before it is
  ///        published, so neither commit nor sweep frees it under the receiver.
  /// @return A non-owning pointer to the appended envelope, or nullptr if a
  ///         receiver stop was requested during admission.
  Transaction_envelope *enqueue(
      std::size_t trx_length, bool is_trx,
      std::shared_ptr<Format_description_log_event> fde,
      bool receiver_held = false);

  // --- Consumer (coordinator) API ---

  /// @brief Wait for the envelope at @c dispatch_seqno, return it and advance
  /// @c dispatch_seqno.
  ///
  /// The wait is bounded by kDispatchPollInterval. Returns nullptr without
  /// advancing any cursor on stop or idle timeout. Takes @c m_queue_mutex.
  ///
  /// @retval nullptr a stop was requested, or nothing arrived in time.
  /// @return otherwise, a non-owning pointer to the dispatched envelope, valid
  ///         until the coordinator sweeps it.
  Transaction_envelope *dispatch_next();

  /// @brief Coordinator sweep: pop the contiguous reclaimable head prefix.
  ///
  /// Pops while the head is reclaimable (see
  /// Transaction_envelope::is_reclaimable()) and @c commit_seqno is below
  /// @c dispatch_seqno, so the commit mark never overtakes the dispatch cursor.
  /// Takes @c m_queue_mutex.
  ///
  /// @retval false success: the committed head prefix (possibly empty) was
  ///         swept and the cursor invariant holds.
  /// @retval true  failure: a structural inconsistency was found (the deque
  ///         size disagrees with insert_seqno - commit_seqno); the sweep stops
  ///         and leaves the queue unchanged from that point.
  bool sweep_committed();

  /// @brief Coordinator step: sweep the head, then wait for and dispatch the
  /// next ACTIVE envelope, all under one @c m_queue_mutex hold.
  ///
  /// The reader's per-iteration entry point, which makes it the only sweeper
  /// and dispatcher. Envelopes already committed or truncated are skipped. On
  /// a structural inconsistency it asserts in debug; in release it sets the
  /// applier stop flag (also ending the workers' batch waits) and returns
  /// nullptr, so the coordinator never runs on corrupt cursors.
  ///
  /// @retval nullptr stop, idle timeout or inconsistency; only the sweep's
  ///         @c commit_seqno advance may have changed.
  /// @return otherwise, a non-owning pointer to the dispatched envelope, valid
  ///         until a later sweep.
  Transaction_envelope *sweep_and_dispatch();

  // --- Cursors (read under m_queue_mutex) ---

  /// @brief The commit low-water mark: number of envelopes swept from the head.
  std::uint64_t commit_seqno() const;

  /// @brief The number of envelopes dispatched to workers so far.
  std::uint64_t dispatch_seqno() const;

  /// @brief The number of envelopes ever enqueued.
  std::uint64_t insert_seqno() const;

  /// @brief The current number of queue-owned envelopes (undispatched,
  /// in flight, or committed but unswept): @c insert_seqno - commit_seqno.
  std::size_t queue_length() const;

  // --- Memory accounting (atomic, lock-free fast path) ---

  /// @brief Test-only out-of-band reservation of @p n bytes. Production must
  /// not call it on another thread: that breaks the single-reserver invariant
  /// (see wait_until_admissible()).
  void add_bytes(std::size_t n);

  /// @brief Release @p n bytes to the memory entry. Wait-free and without a
  /// notify: a receiver polling in admission sees the freed budget next poll.
  void release_bytes(std::size_t n);

  /// @brief Bytes held by admitted memory-path payloads, derived from the
  /// memory entry as clamp(limit - available, 0..limit). 0 when tracking is
  /// off.
  std::size_t bytes_used() const;

  /// @brief Live number of the channel's spill files on disk
  /// (SPILL_FILE_COUNT). Maintained by Spill_file_writer.
  std::size_t spill_file_count() const { return m_spill_file_count.load(); }

  /// @brief Live on-disk bytes of the channel's spill files
  /// (SPILL_BYTES_USED). Maintained by Spill_file_writer.
  std::size_t spill_bytes_used() const { return m_spill_bytes_used.load(); }

  // --- Spill disk accounting (called by Spill_file_writer) ---
  //
  // Atomics without the queue mutex: updates run on the receiver thread, but a
  // writer's destructor (sweep/reset) may run on another thread.

  /// @brief Add @p n bytes to the live spill on-disk byte gauge.
  void add_spill_bytes(std::size_t n) { m_spill_bytes_used.fetch_add(n); }
  /// @brief Subtract @p n bytes from the live spill on-disk byte gauge.
  void release_spill_bytes(std::size_t n) { m_spill_bytes_used.fetch_sub(n); }
  /// @brief Return a spill admission's reservation of @p n bytes once its file
  /// is deleted. No-op when the spill disk limit is off.
  void release_spill_reservation(std::size_t n);
  /// @brief Increment the live spill file-count gauge.
  void inc_spill_files() { m_spill_file_count.fetch_add(1); }
  /// @brief Decrement the live spill file-count gauge.
  void dec_spill_files() { m_spill_file_count.fetch_sub(1); }

  /// @brief Record a failed spill file delete; no lock (may run in reset()).
  void set_spill_delete_failed() { m_spill_delete_failed.store(true); }
  /// @brief Return and clear the failed-delete record, for the applier.
  bool take_spill_delete_failed() {
    return m_spill_delete_failed.exchange(false);
  }
  /// @brief Whether a failed spill file delete is recorded, for tests.
  bool spill_delete_failed() const { return m_spill_delete_failed.load(); }

  /// @brief The hard per-channel memory bound. Immutable.
  std::size_t memory_limit() const { return m_memory_limit; }

  /// @brief The per-channel spill threshold. Immutable.
  std::size_t spill_threshold() const { return m_spill_threshold; }

  /// @brief The server's relay log directory, parent of the spill directory.
  /// Immutable; empty when none was supplied.
  const std::string &relay_log_dir() const { return m_relay_log_dir; }

  /// @brief The owning channel's name, used in spill file names. Immutable;
  /// empty for the default channel.
  const std::string &channel_name() const { return m_channel_name; }

  /// @brief The channel's CSA statistics instance id. Immutable; 0 for tests.
  std::size_t channel_instance_id() const { return m_channel_instance_id; }

  // --- Lifecycle ---

  /// @brief Request stop for one role; the other role is not disturbed.
  ///
  /// RECEIVER sets the receiver stop flag, so admission aborts and enqueue()
  /// returns nullptr. APPLIER sets the applier stop flag under @c m_queue_mutex
  /// and wakes dispatch_next(); workers in a batch wait see it on their next
  /// poll. Re-enable with resume().
  void stop(Scope scope);

  /// @brief Whether the sweep found the deque and cursors out of sync. The
  /// reader reports it as Last_SQL_Error.
  bool applier_error() const;

  /// @brief Re-enable a stopped role for a new session. Resuming the applier
  /// also clears the failed spill delete record.
  void resume(Scope scope);

  /// @brief Empty the queue and zero the cursors for reuse by a new session.
  void reset();

  /// Poll interval of an IMR batch wait while the applier runs.
  static constexpr std::chrono::milliseconds kApplierStopPollInterval{100};

  /// The applier stop flag (m_applier_stopped), read lock-free by the IMR
  /// batch waits and the reader's job drop check.
  const std::atomic<bool> *applier_stop_flag() const {
    return &m_applier_stopped;
  }

 private:
  /// @brief Body of dispatch_next(); the caller owns @c m_queue_mutex.
  ///
  /// @param lock Owns @c m_queue_mutex on entry and on return.
  /// @retval nullptr applier stopped or idle timeout; no cursor was advanced.
  /// @return otherwise, a non-owning pointer to the dispatched envelope.
  Transaction_envelope *dispatch_next_locked(
      std::unique_lock<std::mutex> &lock);

  /// @brief Body of sweep_committed(); caller holds @c m_queue_mutex.
  bool sweep_committed_locked();

  /// @brief One reservation attempt with the rules is_admissible() checks.
  /// No wait, no stop check.
  /// @retval true  reserved, or the path's limit is off.
  /// @retval false the path's limit is reached; nothing was reserved.
  bool try_reserve(std::size_t trx_length, Envelope_path path);

  /// @brief Assert the cursor ordering invariant. Caller holds m_queue_mutex.
  ///
  /// Checks @c 0 <= commit_seqno <= dispatch_seqno <= insert_seqno and that the
  /// deque size equals @c insert_seqno - commit_seqno.
  void assert_cursor_invariant() const {
    assert(m_commit_seqno <= m_dispatch_seqno);
    assert(m_dispatch_seqno <= m_insert_seqno);
    assert(m_envelopes.size() == m_insert_seqno - m_commit_seqno);
  }

  // --- FIFO structure + cursors (guarded by m_queue_mutex) ---

  mutable std::mutex m_queue_mutex;  ///< Guards deque + cursors + structure.
  std::condition_variable m_not_empty_cv;  ///< Consumer waits when drained.

  /// Bound on the idle wait in dispatch_next_locked(), so Csa_service::run
  /// regains control to re-check worker errors and applier progress.
  static constexpr std::chrono::milliseconds kDispatchPollInterval{100};

  /// Poll interval of the receiver's admission waits.
  static constexpr std::chrono::milliseconds kAdmissionPollInterval{1};

  /// Envelopes in enqueue order. unique_ptr keeps each address stable until
  /// the sweep pops (and destroys) it.
  std::deque<std::unique_ptr<Transaction_envelope>> m_envelopes;

  std::uint64_t m_commit_seqno{0};    ///< Head / commit low-water mark (count).
  std::uint64_t m_dispatch_seqno{0};  ///< Number of envelopes dispatched.
  std::uint64_t m_insert_seqno{0};    ///< Tail / number ever enqueued.

  /// Applier stop flag: set by stop(APPLIER) and on a structural inconsistency,
  /// cleared by resume(APPLIER); starts set until the applier attaches.
  /// Written only under m_queue_mutex, so the m_not_empty_cv predicate cannot
  /// miss a stop. Atomic so batch waits can poll it lock-free.
  std::atomic<bool> m_applier_stopped{true};

  /// Set by the sweep on a deque/cursor mismatch, so the reader can tell a
  /// corruption stop from a normal stop. Guarded by m_queue_mutex.
  bool m_applier_error{false};

  // --- Memory accounting ---

  /// Hard per-channel memory bound.
  const std::size_t m_memory_limit;
  /// Size above which a trx spills to disk.
  const std::size_t m_spill_threshold;
  /// Channel relay log dir (spill file parent).
  const std::string m_relay_log_dir;
  /// Owning channel name (spill file name part).
  const std::string m_channel_name;
  /// The channel's memory entry; nullptr when tracking is off. Held directly
  /// so the receiver and applier never read the monitor's name map, which the
  /// SQL thread writes when it registers its resources at start.
  Resource_entry *const m_memory_resource;
  /// The channel's spill disk entry; nullptr when relay_log_space_limit is 0.
  /// Held directly, like m_memory_resource.
  Resource_entry *const m_spill_disk_resource;
  /// The owning channel's instance id (see channel_instance_id()).
  const std::size_t m_channel_instance_id;

  /// Live spill file count and on-disk bytes; they return to 0 as files are
  /// unlinked. Read lock-free by the metrics service.
  std::atomic<std::size_t> m_spill_file_count{0};
  std::atomic<std::size_t> m_spill_bytes_used{0};

  /// A spill file delete failed; cleared by resume(APPLIER) and reset().
  std::atomic<bool> m_spill_delete_failed{false};

  /// Set by stop(RECEIVER); polled lock-free by both admission loops, so no
  /// wake-up is needed.
  std::atomic<bool> m_receiver_stopped{true};

#ifndef NDEBUG
  /// Debug only: see admission_polls().
  std::atomic<std::size_t> m_admission_polls{0};
#endif
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_TRX_ENVELOPE_QUEUE_H
