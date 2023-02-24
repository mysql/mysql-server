/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "classic_change_user_forwarder.h"

#include <optional>

#include "classic_auth.h"
#include "classic_auth_caching_sha2.h"
#include "classic_auth_cleartext.h"
#include "classic_auth_forwarder.h"
#include "classic_auth_native.h"
#include "classic_auth_sha256_password.h"
#include "classic_change_user_sender.h"
#include "classic_connect.h"
#include "classic_connection_base.h"
#include "classic_frame.h"
#include "classic_greeting_forwarder.h"  // ServerGreetor
#include "classic_query_sender.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql-server error-codes

IMPORT_LOG_FUNCTIONS()

using namespace std::string_literals;
using namespace std::string_view_literals;

/**
 * forward the change-user message flow.
 *
 * Expected overall flow:
 *
 * @code
 * c->s: COM_CHANGE_USER
 * alt fast-path
 * alt
 * c<-s: Error
 * else
 * c<-s: Ok
 * end
 * else auth-method-switch
 * c<-s: auth-method-switch
 * c->s: auth-method-data
 * loop more data
 * c<-s: auth-method-data
 * opt
 * c->s: auth-method-data
 * end
 * end
 * alt
 * c<-s: Error
 * else
 * c<-s: Ok
 * end
 * end
 * @endcode
 *
 * If there is no server connection, it is created on demand.
 */
stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
    case Stage::Response:
      return response();
    case Stage::Ok:
      return ok();
    case Stage::Error:
      return error();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::command() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::ChangeUser>(
      src_channel, src_protocol, src_protocol->server_capabilities());
  if (!msg_res) {
    if (msg_res.error().category() ==
        make_error_code(classic_protocol::codec_errc::invalid_input)
            .category()) {
      // a codec error.

      discard_current_msg(src_channel, src_protocol);

      auto send_res = ClassicFrame::send_msg<
          classic_protocol::borrowed::message::server::Error>(
          src_channel, src_protocol, {1047, "Unknown command", "08S01"});
      if (!send_res) return send_client_failed(send_res.error());

      stage(Stage::Done);
      return Result::SendToClient;
    }
    return recv_client_failed(msg_res.error());
  }

  src_protocol->username(std::string(msg_res->username()));
  src_protocol->schema(std::string(msg_res->schema()));
  src_protocol->attributes(std::string(msg_res->attributes()));
  src_protocol->password(std::nullopt);
  src_protocol->auth_method_name(std::string(msg_res->auth_method_name()));

  discard_current_msg(src_channel, src_protocol);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("change_user::command"));
  }

  auto &server_conn = connection()->socket_splicer()->server_conn();
  if (!server_conn.is_open()) {
    stage(Stage::Connect);
  } else {
    // connection to the server exists, create a new ChangeUser command (don't
    // forward the client's as is) as the attributes need to be modified.
    connection()->push_processor(std::make_unique<ChangeUserSender>(
        connection(), true /* in-handshake */,
        [this](const classic_protocol::message::server::Error &err) {
          reconnect_error(err);
        }));

    stage(Stage::Response);
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::connect() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("change_user::connect"));
  }

  stage(Stage::Connected);

  // connect or take connection from pool
  //
  // don't use LazyConnector here as it would call authenticate with the old
  // user and then switch to the new one in a 2nd ChangeUser.
  return socket_reconnect_start();
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::connected() {
  auto &server_conn = connection()->socket_splicer()->server_conn();
  auto *server_protocol = connection()->server_protocol();

  if (!server_conn.is_open()) {
    auto *socket_splicer = connection()->socket_splicer();
    auto *src_channel = socket_splicer->client_channel();
    auto *src_protocol = connection()->client_protocol();

    // take the client::command from the connection.
    auto recv_res =
        ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);
    if (!recv_res) return recv_client_failed(recv_res.error());

    discard_current_msg(src_channel, src_protocol);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("change_user::connect::error"));
    }

    stage(Stage::Done);
    return reconnect_send_error_msg(src_channel, src_protocol);
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("change_user::connected"));
  }

  if (server_protocol->server_greeting()) {
    // from pool.
    connection()->push_processor(std::make_unique<ChangeUserSender>(
        connection(), true,
        [this](const classic_protocol::message::server::Error &err) {
          reconnect_error(err);
        }));
  } else {
    // connector, but not greeted yet.
    connection()->push_processor(std::make_unique<ServerGreetor>(
        connection(), true,
        [this](const classic_protocol::message::server::Error &err) {
          reconnect_error(err);
        }));
  }

  stage(Stage::Response);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::response() {
  if (!connection()->authenticated()) {
    auto *socket_splicer = connection()->socket_splicer();
    auto *src_channel = socket_splicer->client_channel();
    auto *src_protocol = connection()->client_protocol();

    stage(Stage::Error);

    return reconnect_send_error_msg(src_channel, src_protocol);
  }

  stage(Stage::Ok);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> ChangeUserForwarder::ok() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("change_user::ok"));
  }

  // allow connection sharing again.
  connection()->connection_sharing_allowed_reset();

  // clear the warnings
  connection()->execution_context().diagnostics_area().warnings().clear();

  // clear the prepared statements.
  connection()->client_protocol()->prepared_statements().clear();

  if (connection()->context().connection_sharing() &&
      connection()->greeting_from_router()) {
    // if connection sharing is enabled in the config, enable the
    // session-tracker.
    connection()->push_processor(std::make_unique<QuerySender>(connection(), R"(
SET @@SESSION.session_track_schema           = 'ON',
    @@SESSION.session_track_system_variables = '*',
    @@SESSION.session_track_transaction_info = 'CHARACTERISTICS',
    @@SESSION.session_track_gtids            = 'OWN_GTID',
    @@SESSION.session_track_state_change     = 'ON')"));

    stage(Stage::Done);
  } else {
    stage(Stage::Done);
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::error() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("change_user::error"));
  }

  auto *socket_splicer = connection()->socket_splicer();

  // after the error the server will close the server connection.
  auto &server_conn = socket_splicer->server_conn();

  (void)server_conn.close();

  stage(Stage::Done);

  return Result::Again;
}
