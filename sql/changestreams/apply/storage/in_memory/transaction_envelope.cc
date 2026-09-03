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

#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"

#include <utility>

#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/resource/statistics_map.h"  // Statistics_map
#include "sql/changestreams/apply/storage/common/streaming_event_sink.h"
#include "sql/current_thd.h"  // current_thd
#include "sql/sql_class.h"    // THD

namespace mysql::csa {

Transaction_envelope::Transaction_envelope(std::size_t trx_length,
                                           Envelope_path path)
    : m_trx_length(trx_length), m_path(path) {}

Envelope_state Transaction_envelope::state() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_state;
}

bool Transaction_envelope::is_committed() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_state == Envelope_state::COMMITTED;
}

bool Transaction_envelope::commit() {
  std::lock_guard<std::mutex> lock(m_mutex);
  // A repeated commit, or a skipped transaction the receiver truncated first:
  // its GTID is already executed, so there is nothing to commit or count.
  if (m_state != Envelope_state::ACTIVE) return true;
  m_state = Envelope_state::COMMITTED;
  // A held payload (mid-stream skip, or commit before the receiver's release)
  // is freed by release_receiver().
  if (!m_receiver_held) m_payload.reset();
  return false;
}

bool Transaction_envelope::is_truncated() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_state == Envelope_state::TRUNCATED;
}

void Transaction_envelope::set_truncated() {
  std::lock_guard<std::mutex> lock(m_mutex);
  // The first final state wins: a mid-stream skip may have committed it first,
  // and its GTID is already executed.
  if (m_state == Envelope_state::ACTIVE) m_state = Envelope_state::TRUNCATED;
}

void Transaction_envelope::hold_for_receiver() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_receiver_held = true;
}

void Transaction_envelope::release_receiver() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_receiver_held) return;
  m_receiver_held = false;
  // Free a payload kept by an earlier commit. Destroying it never takes the
  // Fetchable_transaction mutex, so the commit hook's lock order (that mutex,
  // then m_mutex) holds.
  if (m_state == Envelope_state::COMMITTED) m_payload.reset();
}

bool Transaction_envelope::is_reclaimable() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return !m_receiver_held && m_state != Envelope_state::ACTIVE;
}

bool Transaction_envelope::is_receiver_held() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_receiver_held;
}

bool Transaction_envelope::create_payload(
    bool is_trx, std::shared_ptr<Format_description_log_event> fde,
    Trx_envelope_queue *owner_queue) {
  // The factory admits; nullptr means a receiver stop, nothing reserved.
  std::unique_ptr<Trx_payload> payload;
  if (m_path == Envelope_path::SPILL) {
    // Reserves spill disk space, held by the spill file's writer.
    payload = Trx_payload::create_spill(is_trx, std::move(fde), m_trx_length,
                                        owner_queue, this);
  } else {
    payload = Trx_payload::create_memory(is_trx, std::move(fde), m_trx_length,
                                         owner_queue, this);
  }
  if (payload == nullptr) {
    return true;  // receiver stop during admission: abort, nothing reserved.
  }
  // Serialized against commit() and current_sink().
  std::lock_guard<std::mutex> lock(m_mutex);
  m_payload = std::move(payload);
  return false;
}

void Transaction_envelope::validate_payload() {
  // Truncate before publish: otherwise a worker could get the broken group and
  // report a spurious applier error. See the header.
  const Streaming_event_sink *sink = current_sink();
  if (sink != nullptr && sink->has_io_error()) {
    set_truncated();
  }
}

Trx_payload *Transaction_envelope::payload() {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_payload.get();
}

Streaming_event_sink *Transaction_envelope::current_sink() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_payload ? m_payload->sink() : nullptr;
}

void commit_envelope_on_success(Transaction_envelope *envelope,
                                const Fetchable_transaction *trx,
                                std::size_t stats_instance_id,
                                bool spill_path) {
  // Skip the hook during a killed worker's commit-order fail-through.
  const THD *thd = current_thd;
  if (thd != nullptr && thd->is_killed()) return;
  if (envelope == nullptr) return;
  // The caller holds the trx mutex under which the receiver sets truncation.
  // Truncated: the envelope may be swept. Aborted: it is re-dispatched later.
  if (trx != nullptr && (trx->is_truncated() || trx->is_aborted())) return;
  // Read before commit(): it may free the byte source that called this.
  const std::size_t trx_length = envelope->trx_length();
  // Counts once (a repeat commit() returns true), skipped GTIDs included.
  if (!envelope->commit())
    Statistics_map::count_imr_completed(stats_instance_id, spill_path,
                                        trx_length);
}

}  // namespace mysql::csa
