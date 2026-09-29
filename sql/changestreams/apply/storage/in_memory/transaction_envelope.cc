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

#include <cassert>
#include <utility>

namespace mysql::csa {

Transaction_envelope::Transaction_envelope(std::uint64_t stream_seqno,
                                           std::size_t trx_length,
                                           Envelope_path path)
    : m_trx_length(trx_length), m_stream_seqno(stream_seqno), m_path(path) {}

bool Transaction_envelope::is_committed() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_committed;
}

bool Transaction_envelope::commit() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_committed) {
    // Repeated commit is invalid.
    return true;
  }
  // Committed and truncated are mutually exclusive.
  assert(!m_truncated);
  m_committed = true;
  // Resetting the payload releases bytes and wakes a blocked receiver.
  m_payload.reset();
  return false;
}

bool Transaction_envelope::is_truncated() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_truncated;
}

void Transaction_envelope::set_truncated() {
  std::lock_guard<std::mutex> lock(m_mutex);
  // The receiver truncates only the still-open, uncommitted transaction.
  assert(!m_committed);
  m_truncated = true;
}

void Transaction_envelope::attach_payload(
    std::unique_ptr<Trx_payload> payload) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_payload = std::move(payload);
}

void Transaction_envelope::create_memory_destination(
    bool is_trx, std::shared_ptr<Format_description_log_event> fde,
    Trx_envelope_queue *owner_queue) {
  // Build the empty MEMORY-path payload.
  attach_payload(Trx_payload::create_memory(is_trx, std::move(fde),
                                            m_trx_length, owner_queue, this));
}

void Transaction_envelope::create_spill_destination(
    bool is_trx, std::shared_ptr<Format_description_log_event> fde,
    Trx_envelope_queue *owner_queue) {
  // Build the empty SPILL-path payloa.
  attach_payload(
      Trx_payload::create_spill(is_trx, std::move(fde), owner_queue, this));
}

Trx_payload *Transaction_envelope::payload() {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_payload.get();
}

void Transaction_envelope::reset_payload() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_payload.reset();
}

Streaming_event_sink *Transaction_envelope::current_sink() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_payload ? m_payload->sink() : nullptr;
}

}  // namespace mysql::csa
