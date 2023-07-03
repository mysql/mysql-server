/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "classic_lazy_connect.h"

#include "classic_change_user.h"
#include "classic_connect.h"
#include "classic_connection.h"
#include "classic_greeting.h"
#include "classic_init_schema.h"
#include "classic_query.h"
#include "classic_reset_connection.h"
#include "hexify.h"

using mysql_harness::hexify;

stdx::expected<Processor::Result, std::error_code> LazyConnector::process() {
  switch (stage()) {
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
    case Stage::Authenticated:
      return authenticated();
    case Stage::SetSchema:
      return set_schema();
    case Stage::SetVars:
      return set_vars();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> LazyConnector::connect() {
  trace(Tracer::Event().stage("connect::connect"));

  auto *socket_splicer = connection()->socket_splicer();
  auto &server_conn = socket_splicer->server_conn();

  if (!server_conn.is_open()) {
    stage(Stage::Connected);

    // creates a fresh connection or takes one from the pool.
    connection()->push_processor(
        std::make_unique<ConnectProcessor>(connection()));
  } else {
    stage(Stage::Done);  // there still is a connection open, nothing to do.
  }

  return Result::Again;
}

/**
 * the handshake part.
 */
stdx::expected<Processor::Result, std::error_code> LazyConnector::connected() {
  auto *socket_splicer = connection()->socket_splicer();
  auto &server_conn = socket_splicer->server_conn();
  auto client_protocol = connection()->client_protocol();
  auto server_protocol = connection()->server_protocol();

  if (!server_conn.is_open()) {
    trace(Tracer::Event().stage("connect::not_connected"));
    // looks like connection failed, leave.
    stage(Stage::Done);
    return Result::Again;
  }

  /*
   * if the connection is from the pool, we need a change user.
   */
  if (server_protocol->server_greeting()) {
    connection()->client_greeting_sent(true);

    if (!in_handshake_ &&
        ((client_protocol->username() == server_protocol->username()) &&
         (client_protocol->sent_attributes() ==
          server_protocol->sent_attributes()))) {
      // it is ok if the schema differs, it will be handled later set_schema()
      connection()->push_processor(
          std::make_unique<ResetConnectionSender>(connection()));
    } else {
      connection()->push_processor(
          std::make_unique<ChangeUserSender>(connection(), in_handshake_));
    }
  } else {
    connection()->push_processor(
        std::make_unique<ServerGreetor>(connection(), in_handshake_));
  }

  stage(Stage::Authenticated);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::authenticated() {
  trace(Tracer::Event().stage("connect::authenticated"));

  if (!connection()->authenticated()) {
    stage(Stage::Done);
  } else {
    stage(Stage::SetVars);
  }
  return Result::Again;
}

void set_session_var(std::string &q, const std::string &key, const Value &val) {
  if (q.empty()) {
    q = "SET ";
  } else {
    q += ",\n    ";
  }

  q += "@@SESSION." + key + " = " + val.to_string();
}

void set_session_var_if_not_set(
    std::string &q, const ExecutionContext::SystemVariables &sysvars,
    const std::string &key, const Value &value) {
  if (sysvars.get(key) == Value(std::nullopt)) {
    set_session_var(q, key, value);
  }
}

void set_session_var_or_value(std::string &q,
                              const ExecutionContext::SystemVariables &sysvars,
                              const std::string &key,
                              const Value &default_value) {
  auto value = sysvars.get(key);
  if (value == Value(std::nullopt)) {
    set_session_var(q, key, default_value);
  } else {
    set_session_var(q, key, value);
  }
}

stdx::expected<Processor::Result, std::error_code> LazyConnector::set_vars() {
  auto &sysvars = connection()->execution_context().system_variables();

  std::string q;

  const auto need_session_trackers =
      connection()->context().connection_sharing() &&
      connection()->greeting_from_router();

  // must be first, to track all variables that are set.
  if (need_session_trackers) {
    set_session_var_or_value(q, sysvars, "session_track_system_variables",
                             Value("*"));
  } else {
    auto var = sysvars.get("session_track_system_variables");
    if (var != Value(std::nullopt)) {
      set_session_var(q, "session_track_system_variables", var);
    }
  }

  for (auto var : sysvars) {
    if (var.first == "session_track_system_variables") continue;

    set_session_var(q, var.first, var.second);
  }

  if (need_session_trackers) {
    set_session_var_if_not_set(q, sysvars, "session_track_gtids",
                               Value("OWN_GTID"));
    set_session_var_if_not_set(q, sysvars, "session_track_transaction_info",
                               Value("CHARACTERISTICS"));
    set_session_var_if_not_set(q, sysvars, "session_track_state_change",
                               Value("ON"));
  }

  if (!q.empty()) {
    connection()->push_processor(
        std::make_unique<QuerySender>(connection(), q));
  }

  stage(Stage::SetSchema);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> LazyConnector::set_schema() {
  auto client_schema = connection()->client_protocol()->schema();
  auto server_schema = connection()->server_protocol()->schema();

  if (!client_schema.empty() && (client_schema != server_schema)) {
    connection()->push_processor(
        std::make_unique<InitSchemaSender>(connection(), client_schema));
  }

  stage(Stage::Done);
  return Result::Again;
}
