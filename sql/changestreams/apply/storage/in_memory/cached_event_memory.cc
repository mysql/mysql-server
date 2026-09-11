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

#include "sql/changestreams/apply/storage/in_memory/cached_event_memory.h"

#include <cassert>
#include <cstring>
#include <utility>

#include "sql/binlog_reader.h"  // Default_binlog_event_allocator, binlog_event_deserialize
#include "sql/log_event.h"  // Format_description_log_event / Log_event

namespace mysql::csa {

Cached_event_memory::Cached_event_memory(
    const char *buf, std::size_t len,
    std::shared_ptr<Format_description_log_event> fde, bool verify_checksum)
    : m_bytes(reinterpret_cast<const unsigned char *>(buf),
              reinterpret_cast<const unsigned char *>(buf) + len),
      m_fde(std::move(fde)),
      m_verify_checksum(verify_checksum) {
  m_fde_ptr = dynamic_cast<Format_description_log_event *>(m_fde.get());
  assert(m_fde_ptr != nullptr);
}

std::shared_ptr<Log_event> Cached_event_memory::decode() {
  // Decode from a fresh copy: the Log_event takes ownership
  // of this copy (register_temp_buf DELEGATE) and frees it when destroyed.
  Default_binlog_event_allocator allocator;
  unsigned char *owned = allocator.allocate(m_bytes.size());
  if (owned == nullptr) return {};
  std::memcpy(owned, m_bytes.data(), m_bytes.size());

  Log_event *event = nullptr;
  Binlog_read_error read_status = binlog_event_deserialize(
      owned, m_bytes.size(), m_fde_ptr, m_verify_checksum, &event);
  if (read_status.has_error()) {
    allocator.deallocate(owned);
    return {};
  }
  event->register_temp_buf(
      reinterpret_cast<char *>(owned),
      Default_binlog_event_allocator::DELEGATE_MEMORY_TO_EVENT_OBJECT);
  return std::shared_ptr<Log_event>(event);
}

void Cached_event_memory::reset(const Format_description_log_event *) {}

}  // namespace mysql::csa
