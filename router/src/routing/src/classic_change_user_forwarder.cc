/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
#include "router_require.h"

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
    case Stage::FetchUserAttrs:
      return fetch_user_attrs();
    case Stage::FetchUserAttrsDone:
      return fetch_user_attrs_done();
    case Stage::SendAuthOk:
      return send_auth_ok();
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
  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::ChangeUser>(
      src_channel, src_protocol, src_protocol.server_capabilities());
  if (!msg_res) {
    if (msg_res.error().category() ==
        make_error_code(classic_protocol::codec_errc::invalid_input)
            .category()) {
      // a codec error.

      discard_current_msg(src_conn);

      auto send_res = ClassicFrame::send_msg<
          classic_protocol::borrowed::message::server::Error>(
          src_conn, {1047, "Unknown command", "08S01"});
      if (!send_res) return send_client_failed(send_res.error());

      stage(Stage::Done);
      return Result::SendToClient;
    }
    return recv_client_failed(msg_res.error());
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("change_user::command"));
  }

  src_protocol.username(std::string(msg_res->username()));
  src_protocol.schema(std::string(msg_res->schema()));
  src_protocol.attributes(std::string(msg_res->attributes()));
  src_protocol.password(std::nullopt);
  src_protocol.auth_method_name(std::string(msg_res->auth_method_name()));

  discard_current_msg(src_conn);

  // disable the tracer for change-user as the previous users
  // 'ROUTER SET trace = 1' should influence _this_ users change-user
  connection()->events().active(false);

  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    stage(Stage::Connect);
  } else {
    // connection to the server exists, create a new ChangeUser command (don't
    // forward the client's as is) as the attributes need to be modified.
    connection()->push_processor(std::make_unique<ChangeUserSender>(
        connection(), true /* in-handshake */,
        [this](const classic_protocol::message::server::Error &err) {
          reconnect_error(err);
        },
        nullptr));

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
  return socket_reconnect_start(nullptr);
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::connected() {
  auto &server_conn = connection()->server_conn();
  auto &server_protocol = server_conn.protocol();

  if (!server_conn.is_open()) {
    auto &src_conn = connection()->client_conn();

    // take the client::command from the connection.
    auto recv_res = ClassicFrame::ensure_has_full_frame(src_conn);
    if (!recv_res) return recv_client_failed(recv_res.error());

    discard_current_msg(src_conn);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("change_user::connect::error"));
    }

    stage(Stage::Done);
    return reconnect_send_error_msg(src_conn);
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("change_user::connected"));
  }

  if (server_protocol.server_greeting()) {
    // from pool.
    connection()->push_processor(std::make_unique<ChangeUserSender>(
        connection(), true,
        [this](const classic_protocol::message::server::Error &err) {
          reconnect_error(err);
        },
        nullptr));
  } else {
    // connector, but not greeted yet.
    connection()->push_processor(std::make_unique<ServerGreetor>(
        connection(), true,
        [this](const classic_protocol::message::server::Error &err) {
          reconnect_error(err);
        },
        nullptr));
  }

  stage(Stage::Response);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::response() {
  auto &dst_conn = connection()->client_conn();

  if (!connection()->authenticated()) {
    stage(Stage::Error);

    return reconnect_send_error_msg(dst_conn);
  }

  stage(Stage::FetchUserAttrs);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::fetch_user_attrs() {
  if (!connection()->context().router_require_enforce()) {
    stage(Stage::SendAuthOk);
    return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::fetch_user_attrs"));
  }

  RouterRequireFetcher::push_processor(
      connection(), required_connection_attributes_fetcher_result_);

  stage(Stage::FetchUserAttrsDone);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::fetch_user_attrs_done() {
  auto &dst_conn = connection()->client_conn();
  auto &dst_channel = dst_conn.channel();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::fetch_user_attrs::done"));
  }

  if (!required_connection_attributes_fetcher_result_) {
    auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::Error>(
            dst_conn, {1045, "Access denied", "28000"});
    if (!send_res) return stdx::unexpected(send_res.error());

    stage(Stage::Error);
    return Result::Again;
  }

  auto enforce_res = RouterRequire::enforce(
      dst_channel, *required_connection_attributes_fetcher_result_);
  if (!enforce_res) {
    auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::Error>(
            dst_conn, {1045, "Access denied", "28000"});
    if (!send_res) return stdx::unexpected(send_res.error());

    stage(Stage::Error);
    return Result::SendToClient;
  }

  stage(Stage::SendAuthOk);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::send_auth_ok() {
  auto &dst_conn = connection()->client_conn();
  auto &dst_protocol = dst_conn.protocol();

  // tell the client that everything is ok.
  auto send_res =
      ClassicFrame::send_msg<classic_protocol::borrowed::message::server::Ok>(
          dst_conn, {0, 0, dst_protocol.status_flags(), 0});
  if (!send_res) return stdx::unexpected(send_res.error());

  stage(Stage::Ok);
  return Result::SendToClient;
}

stdx::expected<Processor::Result, std::error_code> ChangeUserForwarder::ok() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("change_user::ok"));
  }

  connection()->reset_to_initial();

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

  // after the error the server will close the server connection.
  auto &server_conn = connection()->server_conn();

  (void)server_conn.close();

  stage(Stage::Done);

  return Result::Again;
}
