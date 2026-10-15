/* Copyright (c) 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mysql/binlog/event/large_transaction_header_event.h"

#include <utility>

#include "mysql/binlog/event/control_events.h"  // Format_description_event
#include "mysql/binlog/event/event_reader_macros.h"

/**
  @file
  @brief Deserialization of Large_transaction_header_event. The matching
  serialization lives in the server class under sql/log_event.*
*/

namespace mysql::binlog::event {

Large_transaction_header_event::Large_transaction_header_event(
    const char *buf, const Format_description_event *fde)
    : Binary_log_event(&buf, fde) {
  BAPI_ENTER(
      "Large_transaction_header_event::"
      "Large_transaction_header_event(const char*, ...)");
  uint8_t body_length = 0;
  const char *body = nullptr;
  READER_TRY_INITIALIZATION;
  READER_ASSERT_POSITION(fde->common_header_len);

  READER_TRY_SET(m_version, read<uint8_t>);
  if (m_version == 0 || m_version > kVersion) {
    READER_THROW("Invalid Large_transaction_header version");
  }
  READER_TRY_SET(m_terminating_event_offset, read<uint64_t>);
  /* Read unconditionally: a truncated event missing the type byte is
     reported as a read error rather than silently defaulting the type. */
  READER_TRY_SET(m_terminating_event_type, read<uint8_t>);
  READER_TRY_SET(body_length, read<uint8_t>);
  if (body_length > kMaxTerminatingEventBodyLength) {
    READER_THROW("Invalid Large_transaction_header terminating event body");
  }
  READER_TRY_SET(body, ptr, body_length);
  m_terminating_event_body.assign(body, body_length);

  /* The remainder of the body is padding; its contents are ignored. */
  m_padding_size = READER_CALL(available_to_read);

  READER_CATCH_ERROR;
  BAPI_VOID_RETURN;
}

Large_transaction_header_event::Large_transaction_header_event(
    uint64_t terminating_event_offset, uint8_t terminating_event_type,
    std::string terminating_event_body, uint64_t padding_size)
    : Binary_log_event(LARGE_TRANSACTION_HEADER_EVENT),
      m_terminating_event_offset(terminating_event_offset),
      m_terminating_event_type(terminating_event_type),
      m_terminating_event_body(std::move(terminating_event_body)),
      m_padding_size(padding_size) {}

#ifndef HAVE_MYSYS
void Large_transaction_header_event::print_event_info(std::ostream &info) {
  info << "terminating event offset " << m_terminating_event_offset;
}

void Large_transaction_header_event::print_long_info(std::ostream &info) {
  info << "Timestamp: " << header()->when.tv_sec;
  info << "\tVersion: " << static_cast<unsigned>(m_version);
  info << "\tTerminating event offset: " << m_terminating_event_offset;
  info << "\tTerminating event type: "
       << static_cast<unsigned>(m_terminating_event_type);
  info << "\tTerminating event body: " << m_terminating_event_body.size()
       << " bytes";
  info << "\tPadding: " << m_padding_size << " bytes";
  info << "\n";
}
#endif

}  // namespace mysql::binlog::event
