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

#include "sql/changestreams/apply/storage/relay_log/cached_event.h"

#include <cassert>

namespace mysql::csa {

Cached_event::Cached_event(const std::shared_ptr<Log_event> &ev)
    : m_data(ev), orig_buf(m_data->temp_buf) {
  ev->claim_memory_ownership(false);
  // re-claim memory before event destruction
  ev->claim_at_destruction(true);
}

std::shared_ptr<Log_event> Cached_event::decode() { return m_data; }

void Cached_event::reset(const Format_description_log_event *fde) {
  /// Prepare old event for destruction
  char *buf = orig_buf;
  m_data->temp_buf = nullptr;
  m_data->m_free_temp_buf_in_destructor = false;
  Log_event *new_event{nullptr};
  switch (m_data->get_type_code()) {
    case mysql::binlog::event::BEGIN_LOAD_QUERY_EVENT:
      new_event = new Begin_load_query_log_event(buf, fde);
      break;
    case mysql::binlog::event::EXECUTE_LOAD_QUERY_EVENT:
      new_event = new Execute_load_query_log_event(buf, fde);
      break;
    case mysql::binlog::event::VIEW_CHANGE_EVENT:
      new_event = new View_change_log_event(buf, fde);
      break;
    case mysql::binlog::event::QUERY_EVENT:
      new_event =
          new Query_log_event(buf, fde, mysql::binlog::event::QUERY_EVENT);
      break;
    case mysql::binlog::event::ROWS_QUERY_LOG_EVENT:
      new_event = new Rows_query_log_event(buf, fde);
      break;
    case mysql::binlog::event::XID_EVENT:
      new_event = new Xid_log_event(buf, fde);
      break;
    case mysql::binlog::event::TABLE_MAP_EVENT:
      new_event = new Table_map_log_event(buf, fde);
      break;
    case mysql::binlog::event::WRITE_ROWS_EVENT:
      new_event = new Write_rows_log_event(buf, fde);
      break;
    case mysql::binlog::event::UPDATE_ROWS_EVENT:
      new_event = new Update_rows_log_event(buf, fde);
      break;
    case mysql::binlog::event::DELETE_ROWS_EVENT:
      new_event = new Delete_rows_log_event(buf, fde);
      break;
    case mysql::binlog::event::XA_PREPARE_LOG_EVENT:
      new_event = new XA_prepare_log_event(buf, fde);
      break;
    case mysql::binlog::event::PARTIAL_UPDATE_ROWS_EVENT:
      new_event = new Update_rows_log_event(buf, fde);
      break;
    case mysql::binlog::event::TRANSACTION_PAYLOAD_EVENT:
      new_event = new Transaction_payload_log_event(buf, fde);
      break;
    case mysql::binlog::event::INCIDENT_EVENT:
      new_event = new Incident_log_event(buf, fde);
      break;
    case mysql::binlog::event::IGNORABLE_LOG_EVENT:
      break;
    case mysql::binlog::event::ANONYMOUS_GTID_LOG_EVENT:
    case mysql::binlog::event::GTID_LOG_EVENT:
    case mysql::binlog::event::GTID_TAGGED_LOG_EVENT: {
      new_event = new Gtid_log_event(buf, fde);
      break;
    }
    default:
      // this should not happen
      assert(0);
      break;
  }
  if (new_event) {
    new_event->claim_at_destruction(false);
    new_event->register_temp_buf(buf, true);
  }
  m_data.reset(new_event);
}

}  // namespace mysql::csa
