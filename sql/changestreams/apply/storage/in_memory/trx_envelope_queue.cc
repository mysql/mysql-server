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

#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>  // std::this_thread::sleep_for
#include <utility>

#include "my_dbug.h"                                 // DBUG_EXECUTE_IF
#include "mysql/components/services/log_builtins.h"  // LogErr
#include "mysql/my_loglevel.h"                       // ERROR_LEVEL
#include "mysqld_error.h"                            // ER_LOG_PRINTF_MSG
#include "sql/changestreams/apply/resource/resource_monitor.h"  // Resource_entry
#include "sql/current_thd.h"                                    // current_thd
#include "sql/sql_class.h"                                      // THD

namespace mysql::csa {

Trx_envelope_queue::Trx_envelope_queue(std::size_t memory_limit,
                                       std::size_t spill_threshold,
                                       Resource_entry *memory_resource,
                                       std::string relay_log_dir,
                                       std::string channel_name,
                                       Resource_entry *spill_disk_resource,
                                       std::size_t channel_instance_id)
    : m_memory_limit(memory_limit),
      m_spill_threshold(spill_threshold),
      m_relay_log_dir(std::move(relay_log_dir)),
      m_channel_name(std::move(channel_name)),
      m_memory_resource(memory_resource),
      m_spill_disk_resource(spill_disk_resource),
      m_channel_instance_id(channel_instance_id) {
  // The caller registered m_memory_resource with memory_limit.
  assert(m_memory_resource == nullptr ||
         m_memory_resource->m_limit.load() ==
             static_cast<long long>(m_memory_limit));
  // A spill disk entry exists only for a non-zero relay_log_space_limit.
  assert(m_spill_disk_resource == nullptr ||
         m_spill_disk_resource->m_limit.load() > 0);
}

Trx_envelope_queue::~Trx_envelope_queue() {
  // No memory-bytes assert here: m_envelopes.empty() implies every Trx_payload
  // (and its reservation) is gone.
  assert(m_envelopes.empty());
}

Envelope_path Trx_envelope_queue::classify(std::size_t trx_length) const {
  // Routing only; the memory limit is applied in acquire_admission().
  return (trx_length > m_spill_threshold) ? Envelope_path::SPILL
                                          : Envelope_path::MEMORY;
}

std::size_t Trx_envelope_queue::bytes_used() const {
  // The clamp hides the transient dip of available while a failed optimistic
  // reserve rolls back, and any oversized-request overshoot.
  if (m_memory_resource == nullptr) return 0;  // tracking off: nothing reserved
  const long long avail = m_memory_resource->m_available.load();
  const long long limit = m_memory_resource->m_limit.load();
  const long long used = limit - avail;
  if (used <= 0) return 0;
  if (used > limit) return static_cast<std::size_t>(limit);
  return static_cast<std::size_t>(used);
}

bool Trx_envelope_queue::try_reserve(std::size_t trx_length,
                                     Envelope_path path) {
  // The soft spill rule lets an oversized spill transaction in once usage
  // drops below the limit, so it never waits forever (see acquire_admission()).
  if (path == Envelope_path::SPILL) {
    return m_spill_disk_resource == nullptr ||
           Resource_instance_monitor::try_lock_resource_soft(
               *m_spill_disk_resource, trx_length);
  }
  return m_memory_resource == nullptr ||
         Resource_instance_monitor::try_lock_resource(*m_memory_resource,
                                                      trx_length);
}

bool Trx_envelope_queue::acquire_admission(std::size_t trx_length,
                                           Envelope_path path) {
  // Stop is checked before each attempt, so nothing is reserved after STOP.
  // On the server wait_until_admissible() ran first, so the first attempt
  // succeeds; the retries serve the add_bytes() test hook.
  for (;;) {
    if (m_receiver_stopped.load(std::memory_order_acquire)) return true;
    if (try_reserve(trx_length, path)) return false;
#ifndef NDEBUG
    ++m_admission_polls;
#endif
    std::this_thread::sleep_for(kAdmissionPollInterval);
  }
}

bool Trx_envelope_queue::is_admissible(std::size_t trx_length) const {
  // The rules of try_reserve() on the classify() path, without reserving.
  if (classify(trx_length) == Envelope_path::SPILL) {
    return m_spill_disk_resource == nullptr ||
           Resource_instance_monitor::would_lock_soft(*m_spill_disk_resource,
                                                      trx_length);
  }
  return m_memory_resource == nullptr ||
         Resource_instance_monitor::would_lock(*m_memory_resource, trx_length);
}

bool Trx_envelope_queue::wait_until_admissible(std::size_t trx_length) const {
  // Fits now, or the limit is off: no wait.
  if (is_admissible(trx_length)) return false;
  // Once the rule holds it stays true until this thread reserves (see the
  // single-reserver invariant in the header). Unit tests run without a THD.
  const THD *thd = current_thd;
  for (;;) {
    if (m_receiver_stopped.load(std::memory_order_acquire) ||
        (thd != nullptr && thd->is_killed())) {
      return true;  // stop requested or thread killed
    }
    if (is_admissible(trx_length)) return false;
    std::this_thread::sleep_for(kAdmissionPollInterval);
  }
}

Transaction_envelope *Trx_envelope_queue::enqueue(
    std::size_t trx_length, bool is_trx,
    std::shared_ptr<Format_description_log_event> fde, bool receiver_held) {
  // The MEMORY-path destination created below needs a non-null FDE.
  assert(fde != nullptr);
  // The queue owns the placement decision: pick the path first.
  const Envelope_path path = classify(trx_length);

  // create_payload() admits (reserves and may wait) outside m_queue_mutex;
  // true means a receiver stop, so abort the enqueue.
  auto owned = std::make_unique<Transaction_envelope>(trx_length, path);
  if (owned->create_payload(is_trx, std::move(fde), this)) {
    return nullptr;  // stop requested during admission: abort the enqueue.
  }
  // Truncate a failed provisioning before publish (see validate_payload()).
  owned->validate_payload();
  // Hold before publish, so neither the commit hook nor the sweep can free
  // what the receiver still writes.
  if (receiver_held) owned->hold_for_receiver();

  Transaction_envelope *envelope = owned.get();
  {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_envelopes.push_back(std::move(owned));
    ++m_insert_seqno;
    assert_cursor_invariant();
  }
  // Wake a consumer that may be blocked because dispatch_seqno == insert_seqno.
  m_not_empty_cv.notify_one();
  return envelope;
}

Transaction_envelope *Trx_envelope_queue::dispatch_next() {
  // Entry point for callers that do not already hold m_queue_mutex.
  std::unique_lock<std::mutex> lock(m_queue_mutex);
  return dispatch_next_locked(lock);
}

Transaction_envelope *Trx_envelope_queue::dispatch_next_locked(
    std::unique_lock<std::mutex> &lock) {
  // The caller owns m_queue_mutex and keeps owning it across this call.
  assert(lock.owns_lock());
  assert(lock.mutex() == &m_queue_mutex);

  // Bounded wait (see kDispatchPollInterval): a worker apply error with no
  // further transactions must still stop the applier, and the run loop must
  // run its progress checks while idle.
  const bool has_work_or_stop =
      m_not_empty_cv.wait_for(lock, kDispatchPollInterval, [this] {
        return m_applier_stopped.load(std::memory_order_acquire) ||
               m_dispatch_seqno < m_insert_seqno;
      });

  // Idle timeout: return nothing without setting any flag, so the run loop
  // simply polls again.
  if (!has_work_or_stop) {
    return nullptr;
  }

  // On applier stop, advance no cursor and hand back nothing.
  if (m_applier_stopped.load(std::memory_order_acquire)) {
    return nullptr;
  }

  // The next envelope to dispatch sits at index dispatch_seqno - commit_seqno.
  const std::size_t index =
      static_cast<std::size_t>(m_dispatch_seqno - m_commit_seqno);
  Transaction_envelope *envelope = m_envelopes[index].get();
  assert(envelope != nullptr);
  ++m_dispatch_seqno;
  assert_cursor_invariant();

  return envelope;
}

bool Trx_envelope_queue::sweep_committed() {
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  return sweep_committed_locked();
}

bool Trx_envelope_queue::sweep_committed_locked() {
  // Debug injection of a deque/cursor desync, which SQL cannot produce. It
  // skips the assert of the real guard below so a debug build shows the
  // release-mode stop; the shared log prefix lets one test assertion match.
  DBUG_EXECUTE_IF("imr_force_applier_queue_desync", {
    if (!m_applier_error) {
      LogErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
             "[In-memory relay log] applier queue structural inconsistency "
             "(debug-injected via imr_force_applier_queue_desync); stopping "
             "the applier");
      m_applier_error = true;
    }
    return true;
  });

  // Caller holds m_queue_mutex. Pop the contiguous reclaimable head.
  while (!m_envelopes.empty()) {
    // Never advance the commit mark past the dispatch cursor.
    if (m_commit_seqno >= m_dispatch_seqno) {
      break;
    }
    assert(m_envelopes.front() != nullptr);
    // Lock order: queue mutex, then the envelope mutex.
    if (!m_envelopes.front()->is_reclaimable()) break;
    // The deque holds exactly (commit_seqno, insert_seqno]. A mismatch means
    // the deque and cursors desynchronized; stop the applier rather than pop a
    // head out of sync with the commit mark.
    if (m_envelopes.size() != m_insert_seqno - m_commit_seqno) {
      // Log the counters once and latch the error, so the reader reports it as
      // Last_SQL_Error and the coordinator stops cleanly.
      if (!m_applier_error) {
        char msg[384];
        snprintf(msg, sizeof(msg),
                 "[In-memory relay log] applier queue structural "
                 "inconsistency: deque size=%zu, insert_seqno=%llu, "
                 "commit_seqno=%llu, dispatch_seqno=%llu (expected deque size "
                 "== insert_seqno - commit_seqno); stopping the applier",
                 m_envelopes.size(),
                 static_cast<unsigned long long>(m_insert_seqno),
                 static_cast<unsigned long long>(m_commit_seqno),
                 static_cast<unsigned long long>(m_dispatch_seqno));
        LogErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, msg);
        m_applier_error = true;
      }
      assert(false);  // still aborts loudly in debug builds
      return true;
    }
    // Dropping the entry destroys the envelope (and its Trx_payload, if any).
    m_envelopes.pop_front();
    ++m_commit_seqno;
    assert_cursor_invariant();
  }
  return false;
}

Transaction_envelope *Trx_envelope_queue::sweep_and_dispatch() {
  std::unique_lock<std::mutex> lock(m_queue_mutex);
  // Skip envelopes already final: committed out of order in a prior session,
  // or truncated by the receiver. A later sweep reclaims them.
  for (;;) {
    if (sweep_committed_locked()) {
      // Corrupt cursors: stop the applier. The coordinator, the only
      // m_not_empty_cv waiter, runs this, so no wake-up is needed; workers see
      // the flag on their next batch poll.
      m_applier_stopped.store(true, std::memory_order_release);
      return nullptr;
    }
    Transaction_envelope *envelope = dispatch_next_locked(lock);
    if (envelope == nullptr) {
      return nullptr;  // stop
    }
    if (envelope->state() == Envelope_state::ACTIVE) {
      return envelope;
    }
  }
}

std::uint64_t Trx_envelope_queue::commit_seqno() const {
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  return m_commit_seqno;
}

std::uint64_t Trx_envelope_queue::dispatch_seqno() const {
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  return m_dispatch_seqno;
}

std::uint64_t Trx_envelope_queue::insert_seqno() const {
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  return m_insert_seqno;
}

std::size_t Trx_envelope_queue::queue_length() const {
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  return m_envelopes.size();
}

void Trx_envelope_queue::add_bytes(std::size_t n) {
  // Test-only; the caller keeps usage within the limit, so it must fit.
  if (m_memory_resource == nullptr) return;  // tracking off
  const bool reserved [[maybe_unused]] =
      Resource_instance_monitor::try_lock_resource(*m_memory_resource, n);
  assert(reserved || n == 0);
}

void Trx_envelope_queue::release_bytes(std::size_t n) {
  // Called by ~Trx_payload, possibly on the applier thread; a spill payload
  // passes 0.
  if (m_memory_resource == nullptr) return;  // tracking off
  Resource_instance_monitor::release_resource(*m_memory_resource, n);
}

void Trx_envelope_queue::release_spill_reservation(std::size_t n) {
  // Called by ~Spill_file_writer, possibly under m_queue_mutex (sweep/reset),
  // so it takes no lock.
  if (m_spill_disk_resource == nullptr) return;  // spill disk limit off
  Resource_instance_monitor::release_resource(*m_spill_disk_resource, n);
}

void Trx_envelope_queue::stop(Scope scope) {
  if (scope == Scope::RECEIVER) {
    // Admission polls the flag every 1 ms, so no wake-up is needed.
    m_receiver_stopped.store(true, std::memory_order_release);
  }
  if (scope == Scope::APPLIER) {
    // Set under the mutex of m_not_empty_cv, so the wake-up cannot be lost.
    // Batch waits poll the flag within kApplierStopPollInterval.
    {
      std::lock_guard<std::mutex> lock(m_queue_mutex);
      m_applier_stopped.store(true, std::memory_order_release);
    }
    m_not_empty_cv.notify_all();
  }
}

bool Trx_envelope_queue::applier_error() const {
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  return m_applier_error;
}

void Trx_envelope_queue::resume(Scope scope) {
  // Re-enable a role stopped at the end of a previous session.
  if (scope == Scope::RECEIVER) {
    // Atomic release store; no waiter exists at resume time (attach/start).
    m_receiver_stopped.store(false, std::memory_order_release);
  }
  if (scope == Scope::APPLIER) {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_applier_stopped.store(false, std::memory_order_release);
    m_applier_error = false;  // clear the corruption latch for the new session
    m_dispatch_seqno = m_commit_seqno;
    // A delete that failed while the applier was stopped does not stop it.
    m_spill_delete_failed.store(false);
  }
}

void Trx_envelope_queue::reset() {
  // Precondition: both roles are detached.
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  m_envelopes.clear();
  m_commit_seqno = 0;
  m_dispatch_seqno = 0;
  m_insert_seqno = 0;
  m_applier_error = false;
  // A failed delete during the clear above only logs its warning.
  m_spill_delete_failed.store(false);
  assert_cursor_invariant();  // empty deque with 0/0/0 cursors
  // Clearing m_envelopes destroyed every payload, releasing its reservation,
  // and every spill writer, which updated the spill gauges. So the gauges are
  // not force-zeroed: a non-zero value means a lost decrement.
  assert(m_spill_bytes_used.load() == 0);
  assert(m_spill_file_count.load() == 0);
}

}  // namespace mysql::csa
