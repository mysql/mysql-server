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
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

#include "sql/changestreams/apply/storage/in_memory/in_memory_types.h"
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"

class Format_description_log_event;

namespace mysql::csa {

/// @brief Per-channel in-memory FIFO of transaction envelopes with an atomic
/// memory-usage counter and blocking admission control.
///
/// The queue owns its envelopes in a @c std::deque in enqueue order and tracks
/// progress with three monotonic cursors: @c commit_seqno (the committed head),
/// @c dispatch_seqno (the next envelope to hand to a worker), and
/// @c insert_seqno (the tail). A single mutex, @c m_queue_mutex, guards the
/// deque, the cursors, and all structural changes.
///
/// Each transaction's commit is done by its worker under the per-envelope mutex,
/// so a committing worker never contends on @c m_queue_mutex.
class Trx_envelope_queue {
 public:
  /// @brief The replication role a lifecycle operation targets.
  ///
  /// The queue has two independent sets of waiters: the receiver (IO thread)
  /// parked in acquire_admission(), and the applier (coordinator) parked in
  /// dispatch_next(). @c Scope lets stop()/resume() target one role without
  /// disturbing the other, so a single-thread STOP/START wakes only that role.
  /// @c ALL targets both.
  enum class Scope { RECEIVER, APPLIER, ALL };

  /// @brief Construct the queue with its per-channel memory bounds.
  ///
  /// @param memory_limit The hard per-channel memory bound
  ///        (IN_MEMORY_RELAYLOG_LIMIT) on bytes held by memory-path payloads.
  /// @param spill_threshold The per-channel size
  ///        (IN_MEMORY_RELAYLOG_SPILL_THRESHOLD) above which a transaction is
  ///        routed to the spill path instead of the memory path.
  /// @param relay_log_dir The channel's relay log directory. Spill files are
  ///        created in its @c in_memory_relaylog_temp_files subdirectory.
  ///        Defaults to empty for callers that never spill (e.g. memory-only
  ///        unit tests); a real directory is required before spilling.
  Trx_envelope_queue(std::size_t memory_limit, std::size_t spill_threshold,
                     std::string relay_log_dir = {});

  Trx_envelope_queue(const Trx_envelope_queue &) = delete;
  Trx_envelope_queue &operator=(const Trx_envelope_queue &) = delete;
  Trx_envelope_queue(Trx_envelope_queue &&) = delete;
  Trx_envelope_queue &operator=(Trx_envelope_queue &&) = delete;

  /// @brief Destroy the queue, asserting it outlives all its payloads.
  ///
  /// Every Trx_payload holds a back-pointer to this queue and calls
  /// release_bytes() on it when destroyed.
  ~Trx_envelope_queue();

  // --- Producer (receiver) admission API ---

  /// @brief Decide the store path for a transaction of @p trx_length at the
  /// GTID event.
  ///
  /// @param trx_length The transaction's declared byte size from the GTID
  ///        event.
  /// @retval Admission::SPILL when @p trx_length is greater than the spill
  ///         threshold.
  /// @retval Admission::MEMORY when @p trx_length fits the spill threshold and
  ///         bytes_used() + trx_length is within the memory limit.
  /// @retval Admission::WOULD_BLOCK otherwise.
  Admission classify(std::size_t trx_length) const;

  /// @brief Block until a memory-path admission of @p trx_length fits under the
  /// memory limit.
  ///
  /// Waits while bytes_used() + trx_length exceeds the memory limit and no stop
  /// has been requested, re-checking on each wakeup..
  ///
  /// @param trx_length The transaction's declared byte size.
  /// @retval false success: admission is acquired (the reservation now fits).
  /// @retval true  failure: a stop was requested; no bytes were reserved.
  bool acquire_admission(std::size_t trx_length);

  /// @brief Admit a transaction at the GTID event and append its queue entry.
  ///
  ///   1. classifies the store path (SPILL if trx_length > spill_threshold,
  ///      else MEMORY);
  ///   2. for the memory path, blocks the caller (the IO thread) while
  ///      bytes_used() + trx_length exceeds the memory limit — the intended
  ///      back-pressure, like a classic relay log stalling the receiver when out
  ///      of space — returning early only on stop;
  ///   3. builds a new Transaction_envelope with a fresh, increasing
  ///      @c stream_seqno and attaches its empty destination;
  ///   4. under @c m_queue_mutex, appends the envelope, bumps @c insert_seqno,
  ///      and wakes a consumer blocked in dispatch_next().
  ///
  /// @param trx_length The transaction's declared byte size from the GTID
  ///        event.
  /// @param is_trx Whether the admitted unit is a real transaction (true) or a
  ///        standalone/administrative event group (false).
  /// @param fde The active Format_description_log_event. Shared ownership keeps
  ///        it alive for in-flight transactions even after a later FD event
  ///        replaces the receiver's current one. Must be non-null.
  /// @return A non-owning pointer to the appended envelope, or nullptr if a stop
  ///         was requested while blocked in admission (the receiver then aborts
  ///         the enqueue).
  Transaction_envelope *enqueue(std::size_t trx_length, bool is_trx,
                                std::shared_ptr<Format_description_log_event> fde);

  // --- Consumer (coordinator) API ---

  /// @brief Block until the envelope at @c dispatch_seqno is available, then
  /// return it and advance @c dispatch_seqno.
  ///
  /// Waits while @c dispatch_seqno == @c insert_seqno (nothing to dispatch) and
  /// no stop is pending. On success, returns the next envelope and increments
  /// @c dispatch_seqno. On stop, returns nullptr without advancing any cursor.
  ///
  /// Use this from callers that do NOT already hold @c m_queue_mutex。
  ///
  /// @retval nullptr a stop was requested while blocked or on entry.
  /// @return otherwise, a non-owning pointer to the dispatched envelope, valid
  ///         until the coordinator sweeps it.
  Transaction_envelope *dispatch_next();

  /// @brief Coordinator sweep: dequeue the contiguous committed head prefix.
  ///
  /// While the head envelope is committed, pops it and advances @c commit_seqno.
  /// Stops at the first uncommitted head or an empty deque.
  ///
  /// It also stops before advancing @c commit_seqno past @c dispatch_seqno, so
  /// the commit mark never overtakes the dispatch cursor.
  ///
  /// @param need_lock @c true (default) to acquire @c m_queue_mutex here; pass
  ///        @c false when the caller already holds it (e.g.
  ///        @c sweep_and_dispatch()).
  /// @retval false success: the committed head prefix (possibly empty) was swept
  ///         and the cursor invariant holds.
  /// @retval true  failure: a structural inconsistency was found (head
  ///         stream_seqno != commit_seqno + 1); the sweep stops and leaves the
  ///         queue unchanged from that point.
  bool sweep_committed(bool need_lock = true);

  /// @brief Coordinator step: sweep the committed head prefix, then block for
  /// and dispatch the next envelope — all under one @c m_queue_mutex hold.
  ///
  /// The single entry point the applier's reader uses per iteration. In one lock
  /// hold it (1) sweeps the committed head prefix, (2) waits until an
  /// undispatched envelope exists or a stop is requested, then (3) dispatches
  /// that envelope or returns @c nullptr on stop. Folding sweep and dispatch
  /// together makes the reader the sole sweeper and dispatcher.
  ///
  /// If the sweep finds a structural inconsistency (head @c stream_seqno !=
  /// @c commit_seqno + 1), it asserts in debug and, in release, stops the
  /// applier and returns @c nullptr so the coordinator loop exits rather than
  /// running on inconsistent cursors.
  ///
  /// @retval nullptr a stop was requested (or forced by an inconsistency); only
  ///         the sweep's @c commit_seqno advance may have changed.
  /// @return otherwise, a non-owning pointer to the dispatched envelope, valid
  ///         until a later sweep.
  Transaction_envelope *sweep_and_dispatch();

  // --- Cursors (read under m_queue_mutex) ---

  /// @brief The commit low-water mark: number of envelopes swept from the head.
  std::uint64_t commit_seqno() const;

  /// @brief The number of envelopes dispatched to workers so far.
  std::uint64_t dispatch_seqno() const;

  /// @brief The number of envelopes ever enqueued (the last assigned
  /// @c stream_seqno).
  std::uint64_t insert_seqno() const;

  /// @brief The current number of queue-owned transaction envelopes.
  ///
  /// Counts undispatched, in-flight, and committed-but-unswept envelopes. Equals
  /// @c insert_seqno - commit_seqno.
  std::size_t queue_length() const;

  // --- Memory accounting (atomic, lock-free fast path) ---

  /// @brief Atomically add @p n bytes to the memory-usage counter.
  ///
  /// Called by the Trx_payload constructor; a single atomic update, no queue
  /// mutex held.
  void add_bytes(std::size_t n);

  /// @brief Atomically subtract @p n bytes from the memory-usage counter and
  /// wake all threads blocked in acquire_admission().
  ///
  /// Called by the Trx_payload destructor.
  void release_bytes(std::size_t n);

  /// @brief The current number of bytes held by admitted memory-path payloads.
  std::size_t bytes_used() const { return m_bytes_used.load(); }

  /// @brief The hard per-channel memory bound the queue was constructed with.
  /// Immutable for the queue's lifetime.
  std::size_t memory_limit() const { return m_memory_limit; }

  /// @brief The per-channel spill threshold the queue was constructed with.
  /// Immutable for the queue's lifetime.
  std::size_t spill_threshold() const { return m_spill_threshold; }

  /// @brief The channel's relay log directory, under which spill files are
  /// created (in the @c in_memory_relaylog_temp_files subdirectory). Immutable;
  /// empty when no spill directory was supplied.
  const std::string &relay_log_dir() const { return m_relay_log_dir; }

  // --- Lifecycle ---

  /// @brief Request stop for one role (or both) and wake its parked waiters.
  ///
  /// @c stop(Scope::RECEIVER) sets the receiver stop flag and wakes an IO thread
  /// parked in acquire_admission(), so enqueue() aborts with @c nullptr.
  /// @c stop(Scope::APPLIER) sets the applier stop flag and wakes a coordinator
  /// parked in dispatch_next(), so it returns @c nullptr. @c stop(Scope::ALL)
  /// (the default) does both. Each flag shares the lock of the condition
  /// variable it gates, so the flag write and the waiter's check stay ordered.
  ///
  /// Stopping one role leaves the other's waiters undisturbed, which makes a
  /// single-thread STOP safe. Re-enable a stopped role with resume().
  void stop(Scope scope = Scope::ALL);

  /// @brief Whether the queue is fully stopped.
  ///
  /// A single-role stop does NOT make this return true.
  bool is_stopped() const;

  /// @brief Re-enable a stopped role (or both) so a new session can enqueue() /
  /// dispatch_next() again.
  void resume(Scope scope = Scope::ALL);

  /// @brief Return the queue to the empty state (clear @c m_envelopes, zero
  /// all three cursors) so the same instance can be reused for a new session.
  void reset();

 private:
  /// @brief The body of @c dispatch_next(), run under a lock the caller owns.
  ///
  /// Blocks until an undispatched envelope exists or the applier is stopped,
  /// then hands out the envelope at @c dispatch_seqno and advances that cursor.
  ///
  /// @param lock A @c std::unique_lock owning @c m_queue_mutex on entry; still
  ///        owns it on return (asserted in debug builds).
  /// @retval nullptr the applier is stopped — no cursor was advanced.
  /// @return otherwise, a non-owning pointer to the dispatched envelope.
  Transaction_envelope *dispatch_next_locked(
      std::unique_lock<std::mutex> &lock);

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

  /// Envelopes in enqueue order, held through unique_ptr so each envelope's
  /// address stays stable from enqueue until it is dropped at sweep.
  /// Popping the head destroys the envelope.
  std::deque<std::unique_ptr<Transaction_envelope>> m_envelopes;

  std::uint64_t m_commit_seqno{0};    ///< Head / commit low-water mark (count).
  std::uint64_t m_dispatch_seqno{0};  ///< Number of envelopes dispatched.
  std::uint64_t m_insert_seqno{0};    ///< Tail / number ever enqueued.

  /// Applier (coordinator) stop flag, doubling as the applier's attach state.
  /// Guarded by m_queue_mutex.
  bool m_applier_stopped{true};

  // --- Memory accounting ---

  std::size_t m_memory_limit;     ///< Hard per-channel memory bound.
  std::size_t m_spill_threshold;  ///< Size above which a trx spills to disk.
  std::string m_relay_log_dir;  ///< Channel relay log dir (spill file parent).

  std::atomic<std::size_t> m_bytes_used{0};  ///< Bytes held by live payloads.

  /// Mutable so the const is_stopped() can read m_receiver_stopped under it.
  mutable std::mutex m_mem_mutex;  ///< Pairs with m_mem_cv for blocking admission.
  std::condition_variable m_mem_cv;  ///< Woken on release_bytes()/stop().

  /// Receiver (IO thread) stop flag, doubling as the receiver's attach state.
  /// Guarded by m_mem_mutex.
  bool m_receiver_stopped{true};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_TRX_ENVELOPE_QUEUE_H
