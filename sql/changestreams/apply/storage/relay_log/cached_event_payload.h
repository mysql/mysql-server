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

#ifndef MYSQL_CSA_RELAY_LOG_CACHED_EVENT_PAYLOAD_H
#define MYSQL_CSA_RELAY_LOG_CACHED_EVENT_PAYLOAD_H

#include <memory>
#include "sql/binlog_reader.h"  // Default_binlog_event_allocator
#include "sql/changestreams/apply/storage/relay_log/ireader_event.h"
#include "sql/log_event.h"  // Log_event

namespace mysql::csa {

/// @brief Implementation of the IReader_event which keeps event raw
/// payload until decoded
class Cached_event_payload : public IReader_event {
 public:
  /// Is constructed using payload data/metadata and associated fde
  /// @param payload Event payload data and metadata returned by the reader
  /// @param fde Current source's fde
  Cached_event_payload(const Event_payload &payload,
                       std::shared_ptr<Log_event> fde);

  /// @brief Decode function, which decodes payload and returns Log event
  /// smart pointer
  /// @return Log event smart pointer
  std::shared_ptr<Log_event> decode() override;

  /// @brief Resets event state
  void reset(const Format_description_log_event *) override;

 private:
  /// Event data, owning pointer returned by the reader (legacy design)
  uint8_t *m_data;
  /// Event length
  std::size_t m_length;
  /// Information on whether to verify checksum after decoding
  bool m_verify_checksum{false};
  /// Owning pointer of the FDE base
  std::shared_ptr<Log_event> m_current_fde;
  /// Non-owning pointer to FDE
  Format_description_log_event *m_fde_ptr;
  /// @brief Allocator used to allocate event data in the reader, we use
  /// a separate object, since 'Default_binlog_event_allocator' methods do not
  /// use object state in any way
  Default_binlog_event_allocator m_allocator;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_RELAY_LOG_CACHED_EVENT_PAYLOAD_H
