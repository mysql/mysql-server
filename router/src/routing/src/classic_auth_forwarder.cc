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

#include "classic_auth_forwarder.h"

#include <memory>  // unique_ptr
#include <system_error>

#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "auth_digest.h"
#include "classic_auth_caching_sha2.h"
#include "classic_auth_cleartext.h"
#include "classic_auth_native.h"
#include "classic_auth_sha256_password.h"
#include "classic_frame.h"
#include "harness_assert.h"
#include "hexify.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqld_error.h"  // mysql-server error-codes
#include "mysqlrouter/classic_protocol_wire.h"
#include "openssl_version.h"

IMPORT_LOG_FUNCTIONS()

using mysql_harness::hexify;

class AuthGenericForwarder : public Processor {
 public:
  AuthGenericForwarder(MysqlRoutingClassicConnection *conn,
                       std::string auth_method_name,
                       std::string initial_server_auth_data,
                       bool in_handshake = false)
      : Processor(conn),
        auth_method_name_{std::move(auth_method_name)},
        initial_server_auth_data_{std::move(initial_server_auth_data)},
        stage_{in_handshake ? Stage::Response : Stage::Init} {}

  enum class Stage {
    Init,

    ClientData,
    AuthData,

    Response,

    Error,
    Ok,

    Done,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

 private:
  stdx::expected<Result, std::error_code> init();
  stdx::expected<Result, std::error_code> client_data();
  stdx::expected<Result, std::error_code> auth_data();
  stdx::expected<Result, std::error_code> response();
  stdx::expected<Result, std::error_code> error();
  stdx::expected<Result, std::error_code> ok();

  stdx::expected<Result, std::error_code> send_password();

  std::string auth_method_name_;
  std::string initial_server_auth_data_;
  std::string password_;

  Stage stage_;
};

stdx::expected<Processor::Result, std::error_code>
AuthGenericForwarder::process() {
  switch (stage()) {
    case Stage::Init:
      return init();
    case Stage::ClientData:
      return client_data();
    case Stage::Response:
      return response();
    case Stage::AuthData:
      return auth_data();
    case Stage::Error:
      return error();
    case Stage::Ok:
      return ok();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
AuthGenericForwarder::init() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = connection()->client_protocol();

  trace(Tracer::Event().stage("generic::forward::switch: " + auth_method_name_ +
                              "\n" + hexify(initial_server_auth_data_)));

  auto send_res = ClassicFrame::send_msg<
      classic_protocol::message::server::AuthMethodSwitch>(
      dst_channel, dst_protocol,
      {auth_method_name_, initial_server_auth_data_});
  if (!send_res) return send_client_failed(send_res.error());

  stage(Stage::ClientData);
  return Result::SendToClient;
}

stdx::expected<Processor::Result, std::error_code>
AuthGenericForwarder::client_data() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection()->client_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::client::AuthMethodData>(
          src_channel, src_protocol);
  if (!msg_res) return recv_client_failed(msg_res.error());

  trace(Tracer::Event().stage(
      "generic::forward::client:\n" +
      mysql_harness::hexify(msg_res->auth_method_data())));

  // if it isn't a public-key request, it is a fast-auth.
  stage(Stage::Response);

  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
AuthGenericForwarder::response() {
  // ERR|OK|EOF|other
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  // ensure the recv_buf has at last frame-header (+ msg-byte)
  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_server_failed(read_res.error());

  const uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Ok = ClassicFrame::cmd_byte<classic_protocol::message::server::Ok>(),
    Error = ClassicFrame::cmd_byte<classic_protocol::message::server::Error>(),
    AuthData = ClassicFrame::cmd_byte<
        classic_protocol::message::server::AuthMethodData>(),
  };

  switch (Msg{msg_type}) {
    case Msg::AuthData:
      stage(Stage::AuthData);
      return Result::Again;
    case Msg::Ok:
      stage(Stage::Ok);
      return Result::Again;
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
  }

  // if there is another packet, dump its payload for now.
  auto &recv_buf = src_channel->recv_plain_buffer();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);

  log_debug("received unexpected message from server in %s:\n%s",
            auth_method_name_.c_str(), hexify(recv_buf).c_str());

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code>
AuthGenericForwarder::auth_data() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = connection()->server_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::AuthMethodData>(
          dst_channel, dst_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("generic::forward::data\n" +
                              hexify(msg_res->auth_method_data())));
  stage(Stage::ClientData);

  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code> AuthGenericForwarder::ok() {
  stage(Stage::Done);

  trace(Tracer::Event().stage("generic::forward::ok"));

  // leave the message in the queue for the AuthForwarder.
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
AuthGenericForwarder::error() {
  stage(Stage::Done);

  trace(Tracer::Event().stage("generic::forward::error"));

  // leave the message in the queue for the AuthForwarder.
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> AuthForwarder::process() {
  switch (stage()) {
    case Stage::Init:
      return init();
    case Stage::AuthMethodSwitch:
      return auth_method_switch();
    case Stage::Response:
      return response();
    case Stage::Error:
      return error();
    case Stage::Ok:
      return ok();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> AuthForwarder::init() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->server_channel();
  auto *src_protocol = connection()->server_protocol();
  auto *dst_protocol = connection()->client_protocol();

  // ensure the recv_buf has at last frame-header (+ msg-byte)
  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_server_failed(read_res.error());

  const uint8_t msg_type = src_protocol->current_msg_type().value();

  if (msg_type == ClassicFrame::cmd_byte<
                      classic_protocol::message::server::AuthMethodSwitch>()) {
    stage(Stage::AuthMethodSwitch);
    return Result::Again;
  }

  auto auth_method_name = dst_protocol->auth_method_name();
  auto initial_auth_method_data = src_protocol->auth_method_data();

  // handle the pre-auth-plugin capabilities.
  if (auth_method_name.empty()) {
    auth_method_name =
        src_protocol->shared_capabilities().test(
            classic_protocol::capabilities::pos::secure_connection)
            ? AuthNativePassword::kName
            : "old_password";
  }

  trace(Tracer::Event().stage("auth::forwarder::direct: " + auth_method_name));

  if (auth_method_name == AuthSha256Password::kName) {
    connection()->push_processor(std::make_unique<AuthSha256Forwarder>(
        connection(), initial_auth_method_data, true));
  } else if (auth_method_name == AuthCachingSha2Password::kName) {
    connection()->push_processor(std::make_unique<AuthCachingSha2Forwarder>(
        connection(), initial_auth_method_data, true));
  } else if (auth_method_name == AuthNativePassword::kName) {
    connection()->push_processor(std::make_unique<AuthNativeForwarder>(
        connection(), initial_auth_method_data, true));
  } else if (auth_method_name == AuthCleartextPassword::kName) {
    connection()->push_processor(std::make_unique<AuthCleartextForwarder>(
        connection(), initial_auth_method_data, true));
  } else {
    connection()->push_processor(std::make_unique<AuthGenericForwarder>(
        connection(), auth_method_name, initial_auth_method_data, true));
  }

  stage(Stage::Response);
  return Result::Again;
}

// server wants to switch to another auth-method.
stdx::expected<Processor::Result, std::error_code>
AuthForwarder::auth_method_switch() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->server_channel();
  auto *src_protocol = connection()->server_protocol();
  auto *dst_protocol = connection()->client_protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::message::server::AuthMethodSwitch>(src_channel,
                                                           src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto msg = std::move(*msg_res);

  src_protocol->auth_method_name(msg.auth_method());
  src_protocol->auth_method_data(msg.auth_method_data());
  dst_protocol->auth_method_name(msg.auth_method());
  dst_protocol->auth_method_data(msg.auth_method_data());

  trace(Tracer::Event().stage("auth::forwarder::switch: " + msg.auth_method()));

  discard_current_msg(src_channel, src_protocol);

  if (msg.auth_method() == AuthSha256Password::kName) {
    if (dst_protocol->password().has_value()) {
      connection()->push_processor(std::make_unique<AuthSha256Sender>(
          connection(), msg.auth_method_data(),
          dst_protocol->password().value()));
    } else {
      connection()->push_processor(std::make_unique<AuthSha256Forwarder>(
          connection(), msg.auth_method_data()));
    }
  } else if (msg.auth_method() == AuthCachingSha2Password::kName) {
    if (dst_protocol->password().has_value()) {
      connection()->push_processor(std::make_unique<AuthCachingSha2Sender>(
          connection(), msg.auth_method_data(),
          dst_protocol->password().value()));
    } else {
      connection()->push_processor(std::make_unique<AuthCachingSha2Forwarder>(
          connection(), msg.auth_method_data()));
    }
  } else if (msg.auth_method() == AuthNativePassword::kName) {
    if (dst_protocol->password().has_value()) {
      connection()->push_processor(std::make_unique<AuthNativeSender>(
          connection(), msg.auth_method_data(),
          dst_protocol->password().value()));
    } else {
      connection()->push_processor(std::make_unique<AuthNativeForwarder>(
          connection(), msg.auth_method_data()));
    }
  } else if (msg.auth_method() == AuthCleartextPassword::kName) {
    if (dst_protocol->password().has_value()) {
      connection()->push_processor(std::make_unique<AuthCleartextSender>(
          connection(), msg.auth_method_data(),
          dst_protocol->password().value()));
    } else {
      connection()->push_processor(std::make_unique<AuthCleartextForwarder>(
          connection(), msg.auth_method_data()));
    }
  } else {
    connection()->push_processor(std::make_unique<AuthGenericForwarder>(
        connection(), msg.auth_method(), msg.auth_method_data()));
  }

  stage(Stage::Response);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> AuthForwarder::response() {
  // ERR|OK|EOF|other
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  // ensure the recv_buf has at last frame-header (+ msg-byte)
  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_server_failed(read_res.error());

  const uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Ok = ClassicFrame::cmd_byte<classic_protocol::message::server::Ok>(),
    Error = ClassicFrame::cmd_byte<classic_protocol::message::server::Error>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Ok:
      stage(Stage::Ok);
      return Result::Again;
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
  }

  trace(Tracer::Event().stage("auth::forwarder::response"));

  // if there is another packet, dump its payload for now.
  auto &recv_buf = src_channel->recv_plain_buffer();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);

  log_debug("received unexpected message from server in auth:\n%s",
            hexify(recv_buf).c_str());

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code> AuthForwarder::ok() {
  stage(Stage::Done);

  trace(Tracer::Event().stage("auth::forwarder::ok"));

  // leave the message in the queue for the caller.
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> AuthForwarder::error() {
  stage(Stage::Done);

  trace(Tracer::Event().stage("auth::forwarder::error"));

  // leave the message in the queue for the caller.
  return Result::Again;
}
