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
#include <utility>

namespace mysql::csa {

Trx_envelope_queue::Trx_envelope_queue(std::size_t memory_limit,
                                       std::size_t spill_threshold,
                                       std::string relay_log_dir)
    : m_memory_limit(memory_limit),
      m_spill_threshold(spill_threshold),
      m_relay_log_dir(std::move(relay_log_dir)) {}

Trx_envelope_queue::~Trx_envelope_queue() {
  assert(m_envelopes.empty());
  assert(m_bytes_used.load() == 0);
}

Admission Trx_envelope_queue::classify(std::size_t trx_length) const {
  if (trx_length > m_spill_threshold) {
    return Admission::SPILL;
  }
  const std::size_t used = m_bytes_used.load();
  if (used + trx_length <= m_memory_limit) {
    return Admission::MEMORY;
  }
  return Admission::WOULD_BLOCK;
}

bool Trx_envelope_queue::acquire_admission(std::size_t trx_length) {
  std::unique_lock<std::mutex> lock(m_mem_mutex);
  // Block while the reservation would exceed the limit and no stop is pending.
  m_mem_cv.wait(lock, [this, trx_length] {
    return m_receiver_stopped ||
           m_bytes_used.load() + trx_length <= m_memory_limit;
  });
  if (m_receiver_stopped) {
    return true;  // stop requested: reserve nothing, report failure.
  }
  return false;
}

Transaction_envelope *Trx_envelope_queue::enqueue(
    std::size_t trx_length, bool is_trx,
    std::shared_ptr<Format_description_log_event> fde) {
  // The MEMORY-path destination created below needs a non-null FDE.
  assert(fde != nullptr);
  // The queue owns the admission + placement decision. Pick the path first.
  const Envelope_path path = (classify(trx_length) == Admission::SPILL)
                                 ? Envelope_path::SPILL
                                 : Envelope_path::MEMORY;

  // Memory path: block until the reservation fits, or bail out on stop. The
  // spill path is outside the memory budget, so it never blocks here.
  if (path == Envelope_path::MEMORY && acquire_admission(trx_length)) {
    return nullptr;  // stop requested while blocked: abort the enqueue.
  }

  // Read the cursor without m_queue_mutex: as the sole producer.
  const std::uint64_t stream_seqno = m_insert_seqno + 1;

  // Build the envelope and its destination outside m_queue_mutex, then publish
  // the finished object.
  auto owned =
      std::make_unique<Transaction_envelope>(stream_seqno, trx_length, path);
  if (path == Envelope_path::MEMORY) {
    // Reserves trx_length bytes (admission was acquired above).
    owned->create_memory_destination(is_trx, std::move(fde), this);
  } else {
    // Spill path: outside the memory budget, so no admission was acquired.
    // Provisions the private spill file under the channel's relay log directory
    // and reserves zero bytes.
    owned->create_spill_destination(is_trx, std::move(fde), this);
  }
  Transaction_envelope *envelope = owned.get();

  {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    assert(m_insert_seqno + 1 == stream_seqno);  // single-producer guard
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

  // Block until there is an undispatched envelope or a stop is requested.
  m_not_empty_cv.wait(lock, [this] {
    return m_applier_stopped || m_dispatch_seqno < m_insert_seqno;
  });

  // On applier stop, advance no cursor and hand back nothing.
  if (m_applier_stopped) {
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

bool Trx_envelope_queue::sweep_committed(bool need_lock) {
  if (need_lock) {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    return sweep_committed(false);
  }
  // Caller holds m_queue_mutex. Drop the contiguous
  // envelopes that are either committed or truncated.
  while (!m_envelopes.empty()) {
    // Never advance the commit mark past the dispatch cursor.
    if (m_commit_seqno >= m_dispatch_seqno) {
      break;
    }
    assert(m_envelopes.front() != nullptr);
    // is_committed()/is_truncated() each take the per-envelope mutex (lock
    // order queue -> envelope). Reclaim a head that reached either state.
    if (!m_envelopes.front()->is_committed() &&
        !m_envelopes.front()->is_truncated()) {
      break;
    }
    // The head must be the envelope at the commit mark.
    if (m_envelopes.front()->stream_seqno() != m_commit_seqno + 1) {
      assert(false);  // loud in debug builds
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
  // (1) sweep the finalized head envelope (committed, truncated)
  // (2) dispatch the next envelope
  // (3) skip any finalized one due to committed out of order in a
  // prior session, or truncated by the receiver (wait sweep in next run).
  for (;;) {
    // Sweep the committed head prefix, reclaiming finished slots.
    if (sweep_committed(/*need_lock=*/false)) {
      // Structural inconsistency (corrupt cursors).
      m_applier_stopped = true;
      return nullptr;
    }
    // (2) Dispatch the next envelope under the lock we hold;
    Transaction_envelope *envelope = dispatch_next_locked(lock);
    if (envelope == nullptr) {
      return nullptr;  // stop
    }
    // (3) Return only a live transaction.
    if (!envelope->is_committed() && !envelope->is_truncated()) {
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
  // Atomic update, no queue-level mutex held.
  m_bytes_used.fetch_add(n);
}

void Trx_envelope_queue::release_bytes(std::size_t n) {
  m_bytes_used.fetch_sub(n);
  // Wake blocked admitters so they can re-check the limit.
  {
    std::lock_guard<std::mutex> lock(m_mem_mutex);
  }
  m_mem_cv.notify_all();
}

void Trx_envelope_queue::stop(Scope scope) {
  if (scope == Scope::RECEIVER || scope == Scope::ALL) {
    // Wake an IO thread parked in acquire_admission() so enqueue().
    // The flag and m_mem_cv share m_mem_mutex.
    {
      std::lock_guard<std::mutex> lock(m_mem_mutex);
      m_receiver_stopped = true;
    }
    m_mem_cv.notify_all();
  }
  if (scope == Scope::APPLIER || scope == Scope::ALL) {
    // Wake a coordinator parked in dispatch_next().
    // The flag and m_not_empty_cv share m_queue_mutex.
    {
      std::lock_guard<std::mutex> lock(m_queue_mutex);
      m_applier_stopped = true;
    }
    m_not_empty_cv.notify_all();
  }
}

bool Trx_envelope_queue::is_stopped() const {
  // Fully stopped == both roles stopped.
  bool receiver_stopped;
  bool applier_stopped;
  {
    std::lock_guard<std::mutex> lock(m_mem_mutex);
    receiver_stopped = m_receiver_stopped;
  }
  {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    applier_stopped = m_applier_stopped;
  }
  return receiver_stopped && applier_stopped;
}

void Trx_envelope_queue::resume(Scope scope) {
  // Re-enable a role stopped at the end of a previous session.
  if (scope == Scope::RECEIVER || scope == Scope::ALL) {
    std::lock_guard<std::mutex> lock(m_mem_mutex);
    m_receiver_stopped = false;
  }
  if (scope == Scope::APPLIER || scope == Scope::ALL) {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_applier_stopped = false;
    m_dispatch_seqno = m_commit_seqno;
  }
}

void Trx_envelope_queue::reset() {
  // Precondition: no producer or consumer is attached (both replication threads
  // stopped and joined)
  std::lock_guard<std::mutex> lock(m_queue_mutex);
  // Drop every entry and zero the cursors.
  m_envelopes.clear();
  m_commit_seqno = 0;
  m_dispatch_seqno = 0;
  m_insert_seqno = 0;
  assert_cursor_invariant();  // empty deque with 0/0/0 cursors
  assert(m_bytes_used.load() == 0);
}

}  // namespace mysql::csa
