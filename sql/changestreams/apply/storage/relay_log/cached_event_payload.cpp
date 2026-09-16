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

#include "sql/changestreams/apply/storage/relay_log/cached_event_payload.h"

#include <cassert>

namespace mysql::csa {

Cached_event_payload::Cached_event_payload(const Event_payload &payload,
                                           std::shared_ptr<Log_event> fde)
    : m_data(payload.m_data),
      m_length(payload.m_length),
      m_verify_checksum(payload.m_verify_checksum),
      m_current_fde(fde) {
  m_fde_ptr = dynamic_cast<Format_description_log_event *>(m_current_fde.get());
  assert(m_fde_ptr != nullptr);
}

// nothing to reset, event may be read again
void Cached_event_payload::reset(const Format_description_log_event *) {}

std::shared_ptr<Log_event> Cached_event_payload::decode() {
  if (m_data == nullptr) {
    return std::shared_ptr<Log_event>();
  }
  Log_event *event = nullptr;

  Binlog_read_error read_status = binlog_event_deserialize(
      m_data, m_length, m_fde_ptr, m_verify_checksum, &event);
  if (read_status.has_error()) {
    m_allocator.deallocate(m_data);
    return std::shared_ptr<Log_event>();
    m_data = nullptr;
  }
  // pass m_data ownership to Log_event object
  event->register_temp_buf(
      reinterpret_cast<char *>(m_data),
      Default_binlog_event_allocator::DELEGATE_MEMORY_TO_EVENT_OBJECT);
  // we no longer own the data
  m_data = nullptr;
  return std::shared_ptr<Log_event>(event);
}

}  // namespace mysql::csa
