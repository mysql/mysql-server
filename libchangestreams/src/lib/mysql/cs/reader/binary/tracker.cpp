/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "libchangestreams/include/mysql/cs/reader/binary/tracker.h"
#include "libbinlogevents/include/control_events.h"
#include "libbinlogevents/include/statement_events.h"
#include "libchangestreams/include/mysql/cs/reader/state.h"
#include "my_byteorder.h"
#include "my_config.h"
namespace cs::reader::binary {

Tracker::Tracker() {
  //
  // Create a format description event with the version of the server
  // that this was built with.
  //
  // NOTE: We should have binlogevents and this changestreams align
  //       with the version of the server. And Format_description_event
  //       should be created with the version of the binlogevents, not
  //       necessarily the server itself. (If they go hand-in-hand
  //       chances are that they are are synchronized most of the time,
  //       but it is not mandatory.)
  m_fde = std::make_unique<binary_log::Format_description_event>(
      BINLOG_VERSION, VERSION /* server version */);
}

bool Tracker::track_and_update(std::shared_ptr<State> state,
                               const std::vector<uint8_t> &data) {
  binary_log::Log_event_basic_info ev_info{};
  auto buffer = reinterpret_cast<const char *>(data.data());
  auto was_inside_transaction{m_trx_boundary_parser.is_inside_transaction()};
  auto ev_type{
      static_cast<binary_log::Log_event_type>(data[EVENT_TYPE_OFFSET])};

#ifndef NDEBUG
  BAPI_ASSERT(
      (!was_inside_transaction && m_current_gtid_event_buffer.empty()) ||
      (was_inside_transaction && !m_current_gtid_event_buffer.empty()));
#endif

  switch (ev_type) {
    case binary_log::GTID_LOG_EVENT: {
      BAPI_ASSERT(!was_inside_transaction);
      BAPI_ASSERT(m_current_gtid_event_buffer.empty());
      // save the gtid buffer, so we can instantiate the event later and
      // add the gtid to the set of received gtids once the event that
      // terminates the transaction is received
      m_current_gtid_event_buffer.assign(buffer, data.size());
      break;
    }
    case binary_log::FORMAT_DESCRIPTION_EVENT: {
      auto next_fde = std::make_unique<binary_log::Format_description_event>(
          buffer, m_fde.get());
      m_fde = std::move(next_fde);
      if (!m_fde->header()->get_is_valid()) return true;
      break;
    }
    case binary_log::QUERY_EVENT: {
      // TODO: (optimization)
      // consider not decoding the entire event to extract the query
      binary_log::Query_event qev{buffer, m_fde.get(), ev_type};
      if (!qev.header()->get_is_valid()) return true;
      ev_info.query = qev.query;
      ev_info.query_length = strlen(qev.query);
      break;
    }
    default:
      break;
  }

  // fill in the log_event_info struct to feed to the boundary parser
  ev_info.event_type = ev_type;

  uint16_t flags{0};
  memcpy(&flags, buffer + FLAGS_OFFSET, sizeof(flags));
  ev_info.ignorable_event = le16toh(flags) & LOG_EVENT_IGNORABLE_F;

  // update the boundary parser
  m_trx_boundary_parser.feed_event(ev_info, false);

  // if we hit an error in the transaction boundary parser
  if (m_trx_boundary_parser.is_error()) {
    return true; /* purecov: inspected */
  }

  // if transaction was terminated, store the current gtid in the
  // received gtid set
  if (was_inside_transaction &&
      m_trx_boundary_parser.is_not_inside_transaction()) {
    BAPI_ASSERT(!m_current_gtid_event_buffer.empty());
    // the event received terminated the transaction - save the gtid
    binary_log::Gtid_event gev{m_current_gtid_event_buffer.c_str(),
                               m_fde.get()};
    if (!gev.header()->get_is_valid()) return true;
    binary_log::gtids::Gtid gtid(gev.get_uuid(), gev.get_gno());
    state->add_gtid(gtid);
    m_current_gtid_event_buffer.clear();
  }

  return false;
}

}  // namespace cs::reader::binary