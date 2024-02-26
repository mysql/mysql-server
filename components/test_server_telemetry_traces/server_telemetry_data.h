/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef TEST_SERVER_TELEMETRY_DATA_INCLUDED
#define TEST_SERVER_TELEMETRY_DATA_INCLUDED

#include "mysql/components/services/mysql_thd_store_service.h"
#include "server_telemetry_helpers.h"

#include <cstddef>
#include <string>
#include <vector>

namespace test_telemetry {

bool register_server_telemetry_slot(FileLogger &log);
void unregister_server_telemetry_slot(FileLogger &log);

/**
 Tracks telemetry accounting for a single SQL statement.
*/
struct Statement_Data {
  std::string app_ctx{};
  std::string call_ctx{};
  bool traced{false};
};

/**
 Tracks telemetry accounting for a single session.
 Component creates an object of this type on session connect,
 and destroys it on session disconnect event.
 Class maintains the statement stack, tracking statement currently executed
 within this session and its sub-statements. Parent (ancestor) statements are
 being stored deeper in the stack. Note that parent (ancestor) statements may be
 missing in the stack if they were already discarded by the telemetry component
 callback call (so full stack for some statement may be incomplete, with bottom
 item(s) already discarded). Once some statement was discarded from tracing,
 subsequent callbacks in call order (tm_stmt_start, tm_stmt_notify_qa,
 tm_stmt_end) won't be called anymore for this statement. Also important,
 tm_stmt_notify_qa is only called for top-level statements. We also maintain
 "traced" flag each statement to help us telling us if the root parent statement
 was approved to be traced or not (within tm_stmt_start and tm_stmt_notify_qa
 steps). If the root parent statement should not be traced, we won't trace
 its sub-statements either.
*/
class Session_data {
 public:
  static Session_data *get(MYSQL_THD thd);
  static void set(MYSQL_THD thd, Session_data *data, FileLogger &log);

  static Session_data *create();
  static void destroy(Session_data *data);

  inline unsigned long stmt_stack_depth() const {
    return (unsigned long)m_stmt_stack.size();
  }

  void discard_stmt();

  std::vector<Statement_Data> m_stmt_stack{};
};

}  // namespace test_telemetry

#endif /* TEST_SERVER_TELEMETRY_DATA_INCLUDED */
