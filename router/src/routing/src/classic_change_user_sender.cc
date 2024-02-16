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

#include "classic_change_user_sender.h"

#include <optional>

#include "classic_auth.h"
#include "classic_auth_caching_sha2.h"
#include "classic_auth_cleartext.h"
#include "classic_auth_forwarder.h"
#include "classic_auth_native.h"
#include "classic_auth_sha256_password.h"
#include "classic_connect.h"
#include "classic_connection_base.h"
#include "classic_frame.h"
#include "hexify.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql-server error-codes
#include "tracer.h"

IMPORT_LOG_FUNCTIONS()

using namespace std::string_literals;
using namespace std::string_view_literals;

stdx::expected<Processor::Result, std::error_code> ChangeUserSender::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::InitialResponse:
      return initial_response();
    case Stage::FinalResponse:
      return final_response();
    case Stage::Ok:
      return ok();
    case Stage::Error:
      return error();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

/**
 * router specific connection attributes.
 *
 * @param[in] ssl pointer to a SSL struct of the client connection. May be
 * nullptr.
 */
static std::vector<std::pair<std::string, std::string>>
client_ssl_connection_attributes(const SSL *ssl) {
  if (ssl == nullptr) return {};

  return {{"_client_ssl_cipher", SSL_get_cipher_name(ssl)},
          {"_client_ssl_version", SSL_get_version(ssl)}};
}

/**
 * splice two vectors together.
 *
 * appends all elements of other to the vector v.
 */
template <class T>
std::vector<T> vector_splice(std::vector<T> v, const std::vector<T> &other) {
  v.insert(v.end(), other.begin(), other.end());
  return v;
}

/**
 * verify connection attributes are sane.
 *
 * connection attributes are a key-value-key-value-...
 *
 * - decodes as var-string
 * - each key must have a value
 */
static stdx::expected<void, std::error_code>
classic_proto_verify_connection_attributes(const std::string &attrs) {
  // track if each key has a matching value.
  bool is_key{true};
  auto attr_buf = net::buffer(attrs);

  while (net::buffer_size(attr_buf) != 0) {
    const auto decode_res =
        classic_protocol::decode<classic_protocol::wire::VarString>(attr_buf,
                                                                    {});
    if (!decode_res) return stdx::unexpected(decode_res.error());

    const auto bytes_read = decode_res->first;
    const auto kv = decode_res->second;

    attr_buf += bytes_read;

    // toggle the key/value tracker.
    is_key = !is_key;
  }

  // if the last key doesn't have a value, fail
  if (!is_key || net::buffer_size(attr_buf) != 0) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  return {};
}

static stdx::expected<size_t, std::error_code> classic_proto_append_attribute(
    std::string &attrs_buf, const std::string &key, const std::string &value) {
  auto encode_res =
      classic_protocol::encode(classic_protocol::wire::VarString(key), {},
                               net::dynamic_buffer(attrs_buf));
  if (!encode_res) {
    return stdx::unexpected(encode_res.error());
  }

  size_t encoded_bytes = encode_res.value();

  encode_res =
      classic_protocol::encode(classic_protocol::wire::VarString(value), {},
                               net::dynamic_buffer(attrs_buf));
  if (!encode_res) {
    return stdx::unexpected(encode_res.error());
  }

  encoded_bytes += encode_res.value();

  return encoded_bytes;
}

/**
 * remove trailing \0 in a string_view.
 *
 * returns the original string-view, if there is no trailing NUL-char.
 */
static auto strip_trailing_null(std::string_view s) {
  if (s.empty()) return s;

  if (s.back() == '\0') s.remove_suffix(1);

  return s;
}

/**
 * merge connection attributes.
 *
 * - verifies the connection attributes
 * - appends new attributes.
 * - sets attributes back to the client-greeting-msg
 *
 * @returns bytes appended on success, std::error_code on error.
 */
static stdx::expected<std::string, std::error_code>
classic_proto_decode_and_add_connection_attributes(
    std::string attrs,
    const std::vector<std::pair<std::string, std::string>> &extra_attributes) {
  // add attributes if they are sane.
  const auto verify_res = classic_proto_verify_connection_attributes(attrs);
  if (!verify_res) return stdx::unexpected(verify_res.error());

  for (const auto &attr : extra_attributes) {
    const auto append_res =
        classic_proto_append_attribute(attrs, attr.first, attr.second);
    if (!append_res) return stdx::unexpected(append_res.error());
  }

  return {attrs};
}

static std::optional<std::string> scramble_them_all(
    std::string_view auth_method, std::string_view nonce,
    std::string_view pwd) {
  if (auth_method == AuthCachingSha2Password::kName) {
    return AuthCachingSha2Password::scramble(nonce, pwd);
  } else if (auth_method == AuthNativePassword::kName) {
    return AuthNativePassword::scramble(nonce, pwd);
  } else if (auth_method == AuthSha256Password::kName) {
    return AuthSha256Password::scramble(nonce, pwd);
  } else if (auth_method == AuthCleartextPassword::kName) {
    return AuthCleartextPassword::scramble(nonce, pwd);
  } else {
    return std::nullopt;
  }
}

static classic_protocol::message::client::ChangeUser change_user_for_reuse(
    Channel &src_channel, ClientSideClassicProtocolState &src_protocol,
    [[maybe_unused]] ServerSideClassicProtocolState &dst_protocol,
    std::vector<std::pair<std::string, std::string>>
        initial_connection_attributes) {
  harness_assert(src_protocol.client_greeting().has_value());
  harness_assert(dst_protocol.server_greeting().has_value());

  const auto append_attrs_res =
      classic_proto_decode_and_add_connection_attributes(
          src_protocol.attributes(),
          vector_splice(initial_connection_attributes,
                        client_ssl_connection_attributes(src_channel.ssl())));
  // if decode/append fails forward the attributes as is. The server should
  // fail too.
  auto attrs = append_attrs_res.value_or(src_protocol.attributes());

  if (src_protocol.password().has_value()) {
    // scramble with the server's auth-data to trigger a fast-auth.

    auto pwd = *(src_protocol.password());

    // if the password set and not empty, rehash it.
    if (auto scramble_res = scramble_them_all(
            src_protocol.auth_method_name(),
            strip_trailing_null(dst_protocol.auth_method_data()), pwd)) {
      return {
          src_protocol.username(),                      // username
          *scramble_res,                                // auth_method_data
          src_protocol.schema(),                        // schema
          src_protocol.client_greeting()->collation(),  // collation
          src_protocol.auth_method_name(),              // auth_method_name
          attrs,                                        // attributes
      };
    }
  }

  return {
      src_protocol.username(),                      // username
      "",                                           // auth_method_data
      src_protocol.schema(),                        // schema
      src_protocol.client_greeting()->collation(),  // collation
      "switch_me_if_you_can",                       // auth_method_name
      attrs,                                        // attributes
  };
}

stdx::expected<Processor::Result, std::error_code> ChangeUserSender::command() {
  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->server_conn();
  auto &dst_protocol = dst_conn.protocol();

  change_user_msg_ =
      change_user_for_reuse(src_channel, src_protocol, dst_protocol,
                            src_conn.initial_connection_attributes());
#if 0
  std::cerr << __LINE__ << ": username: " << change_user_msg_->username()
            << "\n"
            << ".. schema: " << change_user_msg_->schema() << "\n"
            << ".. auth-method-name: " << change_user_msg_->auth_method_name()
            << "\n"
            << ".. auth-method-data: "
            << mysql_harness::hexify(change_user_msg_->auth_method_data())
            << "\n"
      //
      ;
#endif

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("change_user::command"));
  }

  trace_event_command_ = trace_span(parent_event_, "mysql/change_user");

  dst_protocol.seq_id(0xff);  // reset seq-id

  auto send_res = ClassicFrame::send_msg(dst_conn, *change_user_msg_);
  if (!send_res) return send_server_failed(send_res.error());

  stage(Stage::InitialResponse);
  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserSender::initial_response() {
  connection()->push_processor(std::make_unique<AuthForwarder>(connection()));

  stage(Stage::FinalResponse);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserSender::final_response() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) return recv_server_failed(read_res.error());

  const uint8_t msg_type = src_protocol.current_msg_type().value();

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

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("change_user::response"));
  }

  return stdx::unexpected(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code> ChangeUserSender::ok() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->client_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Ok>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto msg = *msg_res;

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("change_user::ok"));
  }

  if (auto *ev = trace_span(trace_event_command_, "mysql/response")) {
    ClassicFrame::trace_set_attributes(ev, src_protocol, msg);

    trace_span_end(ev);
  }

  trace_command_end(trace_event_command_);

  if (!msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()), src_protocol.shared_capabilities());
    if (!track_res) {
      // ignore
    }
  }

  dst_protocol.status_flags(msg.status_flags());

  connection()->authenticated(true);

  src_protocol.username(change_user_msg_->username());
  dst_protocol.username(change_user_msg_->username());
  src_protocol.schema(change_user_msg_->schema());
  dst_protocol.schema(change_user_msg_->schema());
  src_protocol.sent_attributes(change_user_msg_->attributes());
  dst_protocol.sent_attributes(change_user_msg_->attributes());

  stage(Stage::Done);

  discard_current_msg(src_conn);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> ChangeUserSender::error() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::Error>(src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto msg = *msg_res;

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("change_user::error: " +
                                   std::string(msg.message())));
  }

  if (auto *ev = trace_span(trace_event_command_, "mysql/response")) {
    ClassicFrame::trace_set_attributes(ev, src_protocol, msg);

    trace_span_end(ev);
  }

  trace_command_end(trace_event_command_);

  connection()->authenticated(false);

  stage(Stage::Done);

  on_error_({msg.error_code(), std::string(msg.message()),
             std::string(msg.sql_state())});

  discard_current_msg(src_conn);
  return Result::Again;
}
