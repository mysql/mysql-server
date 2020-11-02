/*
  Copyright (c) 2019, 2020, Oracle and/or its affiliates.

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

#include "classic_mock_session.h"

#include <openssl/bio.h>
#include <openssl/x509.h>
#include <array>
#include <chrono>
#include <memory>
#include <system_error>
#include <thread>

#include <openssl/ssl.h>

#include "harness_assert.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"
#include "mysqlrouter/classic_protocol.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "router/src/mock_server/src/statement_reader.h"

IMPORT_LOG_FUNCTIONS()

namespace server_mock {

stdx::expected<size_t, std::error_code> MySQLClassicProtocol::read_packet(
    std::vector<uint8_t> &payload) {
  std::array<char, 4> hdr_buf_storage;

  net::mutable_buffer hdr_buf = net::buffer(hdr_buf_storage);

  read_buffer(hdr_buf);

  auto decode_res = classic_protocol::decode<classic_protocol::frame::Header>(
      net::buffer(hdr_buf_storage), {});
  if (!decode_res) {
    return decode_res.get_unexpected();
  }

  auto hdr_frame = decode_res.value();

  auto hdr = hdr_frame.second;

  if (hdr.payload_size() == 0xffffff) {
    return stdx::make_unexpected(
        make_error_code(std::errc::operation_not_supported));
  }

  seq_no_ = hdr.seq_id() + 1;

  payload.resize(hdr.payload_size());
  net::mutable_buffer payload_buf = net::buffer(payload);

  read_buffer(payload_buf);

  return payload.size();
}

void MySQLClassicProtocol::send_packet(const std::vector<uint8_t> &payload) {
  send_buffer(net::buffer(payload));
}

bool MySQLServerMockSessionClassic::process_handshake() {
  bool is_first_packet = true;

  while (!killed()) {
    std::vector<uint8_t> payload;
    if (!is_first_packet) {
      auto read_res = protocol_->read_packet(payload);
      if (!read_res) {
        throw std::system_error(read_res.error());
      }
    }
    is_first_packet = false;
    if (true == handle_handshake(payload)) {
      // handshake is done
      return true;
    }
  }

  return false;
}

bool MySQLServerMockSessionClassic::process_statements() {
  while (!killed()) {
    std::vector<uint8_t> payload;

    auto read_res = protocol_->read_packet(payload);
    if (!read_res) {
      throw std::system_error(read_res.error());
    }

    if (payload.size() == 0) {
      throw std::system_error(make_error_code(std::errc::bad_message));
    }

    auto cmd = payload[0];
    switch (cmd) {
      case classic_protocol::Codec<
          classic_protocol::message::client::Query>::cmd_byte(): {
        // skip the first
        std::string statement_received(std::next(payload.begin()),
                                       payload.end());

        try {
          json_reader_->handle_statement(statement_received, protocol_);
        } catch (const std::exception &e) {
          // handling statement failed. Return the error to the client
          std::this_thread::sleep_for(json_reader_->get_default_exec_time());
          log_error("executing statement failed: %s", e.what());
          protocol_->send_error(
              ER_PARSE_ERROR,
              std::string("executing statement failed: ") + e.what());

          // assume the connection is broken
          return true;
        }
      } break;
      case classic_protocol::Codec<
          classic_protocol::message::client::Quit>::cmd_byte():
        log_info("received QUIT command from the client");
        return true;
      default:
        log_info("received unsupported command from the client: %d", cmd);
        std::this_thread::sleep_for(json_reader_->get_default_exec_time());
        protocol_->send_error(ER_PARSE_ERROR,
                              "Unsupported command: " + std::to_string(cmd));
    }
  }

  return true;
}

void MySQLClassicProtocol::send_auth_fast_message() {
  std::vector<uint8_t> buf;

  auto encode_res = classic_protocol::encode<
      classic_protocol::frame::Frame<classic_protocol::wire::FixedInt<1>>>(
      {seq_no_++, {3}}, shared_capabilities(), net::dynamic_buffer(buf));

  send_packet(buf);
}

void MySQLClassicProtocol::send_auth_switch_message(
    const classic_protocol::message::server::AuthMethodSwitch &msg) {
  std::vector<uint8_t> buf;

  auto encode_res = classic_protocol::encode<classic_protocol::frame::Frame<
      classic_protocol::message::server::AuthMethodSwitch>>(
      {seq_no_++, msg}, shared_capabilities(), net::dynamic_buffer(buf));

  send_packet(buf);
}

void MySQLClassicProtocol::send_server_greeting(
    const classic_protocol::message::server::Greeting &greeting) {
  std::vector<uint8_t> buf;

  server_capabilities_ = greeting.capabilities();

  auto encode_res = classic_protocol::encode<classic_protocol::frame::Frame<
      classic_protocol::message::server::Greeting>>(
      {seq_no_++, greeting}, server_capabilities(), net::dynamic_buffer(buf));

  send_packet(buf);
}

stdx::expected<std::string, std::error_code> cert_get_name(X509_NAME *name) {
  std::unique_ptr<BIO, decltype(&BIO_free)> bio{BIO_new(BIO_s_mem()),
                                                &BIO_free};
  // X509_NAME_oneline() is a legacy function and supposed to be not used for
  // new apps, but the server uses it, so we do to get the same serialization.
#if 0
  int res = X509_NAME_print_ex(bio.get(), name, 0, XN_FLAG_ONELINE);
  if (res <= 0) {
    return stdx::make_unexpected(make_tls_error());
  }

  BUF_MEM *buf;

  BIO_get_mem_ptr(bio.get(), &buf);

  return {stdx::in_place, buf->data, buf->data + buf->length};
#else
  std::array<char, 256> buf;

  return {stdx::in_place, X509_NAME_oneline(name, buf.data(), buf.size())};
#endif
}

stdx::expected<std::string, std::error_code> cert_get_subject_name(X509 *cert) {
  return cert_get_name(X509_get_subject_name(cert));
}

stdx::expected<std::string, std::error_code> cert_get_issuer_name(X509 *cert) {
  return cert_get_name(X509_get_issuer_name(cert));
}

bool MySQLServerMockSessionClassic::authenticate(
    const std::vector<uint8_t> &client_auth_method_data) {
  auto account_data_res = json_reader_->account();
  if (!account_data_res) {
    return false;
  }

  auto account = account_data_res.value();

  if (account.username.has_value()) {
    if (account.username.value() != protocol_->username()) {
      return false;
    }
  }

  if (account.password.has_value()) {
    if (!protocol_->authenticate(
            protocol_->auth_method_name(), protocol_->auth_method_data(),
            account.password.value(), client_auth_method_data)) {
      return false;
    }
  }

  if (account.cert_required) {
    auto *ssl = protocol_->ssl();

    std::unique_ptr<X509, decltype(&X509_free)> client_cert{
        SSL_get_peer_certificate(ssl), &X509_free};
    if (!client_cert) {
      log_info("cert required, no cert received.");
      return false;
    }

    if (account.cert_subject.has_value()) {
      auto subject_res = cert_get_subject_name(client_cert.get());
      if (!subject_res) {
        throw std::system_error(subject_res.error(), "cert_get_subject_name");
      }
      log_debug("client-cert::subject: %s", subject_res.value().c_str());

      if (account.cert_subject.value() != subject_res.value()) {
        return false;
      }
    }

    if (account.cert_issuer.has_value()) {
      auto issuer_res = cert_get_issuer_name(client_cert.get());
      if (!issuer_res) {
        throw std::system_error(issuer_res.error(), "cert_get_issuer_name");
      }
      log_debug("client-cert::issuer: %s", issuer_res.value().c_str());

      if (account.cert_issuer.value() != issuer_res.value()) {
        return false;
      }
    }

    const auto verify_res = SSL_get_verify_result(protocol_->ssl());

    if (verify_res != X509_V_OK) {
      log_info("ssl-verify failed: %ld", verify_res);
      return false;
    }
  }

  return true;
}

bool MySQLServerMockSessionClassic::handle_handshake(
    const std::vector<uint8_t> &payload) {
  switch (state()) {
    case HandshakeState::INIT: {
      auto greeting_res = json_reader_->server_greeting(with_tls_);
      if (!greeting_res) {
        protocol_->send_error(0, greeting_res.error().message(), "28000");
        break;
      }

      std::this_thread::sleep_for(json_reader_->server_greeting_exec_time());

      protocol_->send_server_greeting(greeting_res.value());

      state(HandshakeState::GREETED);

      break;
    }
    case HandshakeState::GREETED: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::message::client::Greeting>(
              net::buffer(payload), protocol_->server_capabilities());

      if (!decode_res) {
        throw std::system_error(decode_res.error(),
                                "decoding client greeting failed");
      }

      const auto greeting = std::move(decode_res.value().second);

      protocol_->client_capabilities(greeting.capabilities());

      if (protocol_->shared_capabilities().test(
              classic_protocol::capabilities::pos::ssl) &&
          !protocol_->is_tls()) {
        protocol_->init_tls();

        auto tls_accept_res = protocol_->tls_accept();
        if (!tls_accept_res) {
          throw std::system_error(tls_accept_res.error());
        }

        auto *ssl = protocol_->ssl();
        json_reader_->set_session_ssl_info(ssl);

        break;
      }

      protocol_->username(greeting.username());

      if (greeting.capabilities().test(
              classic_protocol::capabilities::pos::plugin_auth)) {
        protocol_->auth_method_name(greeting.auth_method_name());
      } else {
        // 4.1 or so
        protocol_->auth_method_name(MySQLNativePassword::name);
      }

      if (protocol_->auth_method_name() == CachingSha2Password::name) {
        // auth_response() should be empty
        //
        // ask for the real full authentication
        protocol_->auth_method_data(std::string(20, 'a'));

        protocol_->send_auth_switch_message(
            {protocol_->auth_method_name(),
             protocol_->auth_method_data() + std::string(1, '\0')});

        state(HandshakeState::AUTH_SWITCHED);
      } else if (protocol_->auth_method_name() == MySQLNativePassword::name ||
                 protocol_->auth_method_name() == ClearTextPassword::name) {
        // authenticate wants a vector<uint8_t>
        auto client_auth_method_data = greeting.auth_method_data();
        std::vector<uint8_t> auth_method_data_vec(
            client_auth_method_data.begin(), client_auth_method_data.end());

        if (!authenticate(auth_method_data_vec)) {
          protocol_->send_error(ER_ACCESS_DENIED_ERROR,  // 1045
                                "Access Denied for user '" +
                                    protocol_->username() + "'@'localhost'",
                                "28000");
          state(HandshakeState::DONE);
          break;
        }

        protocol_->send_ok();

        state(HandshakeState::DONE);
      } else {
        protocol_->send_error(0, "unknown auth-method", "28000");

        state(HandshakeState::DONE);
      }

      break;
    }
    case HandshakeState::AUTH_SWITCHED: {
      // empty password is signaled by {0},
      // -> authenticate expects {}
      // -> client expects OK, instead of AUTH_FAST in this case
      if (authenticate(payload == std::vector<uint8_t>{0}
                           ? std::vector<uint8_t>{}
                           : payload)) {
        if (protocol_->auth_method_name() == CachingSha2Password::name) {
          // caching-sha2-password is special and needs the auth-fast state

          protocol_->send_auth_fast_message();
          protocol_->send_ok();
        } else {
          protocol_->send_ok();
        }

        state(HandshakeState::DONE);
      } else {
        protocol_->send_error(ER_ACCESS_DENIED_ERROR,
                              "Access Denied for user '" +
                                  protocol_->username() + "'@'localhost'",
                              "28000");
        state(HandshakeState::DONE);
      }
      break;
    }
    case HandshakeState::DONE: {
      break;
    }
  }

  return (state() == HandshakeState::DONE);
}

void MySQLClassicProtocol::send_error(const uint16_t error_code,
                                      const std::string &error_msg,
                                      const std::string &sql_state) {
  std::vector<uint8_t> buf;
  auto encode_res = classic_protocol::encode<
      classic_protocol::frame::Frame<classic_protocol::message::server::Error>>(
      {seq_no_++, {error_code, error_msg, sql_state}}, shared_capabilities(),
      net::dynamic_buffer(buf));

  if (!encode_res) {
    //
    return;
  }

  send_packet(buf);
}

void MySQLClassicProtocol::send_ok(const uint64_t affected_rows,
                                   const uint64_t last_insert_id,
                                   const uint16_t server_status,
                                   const uint16_t warning_count) {
  std::vector<uint8_t> buf;
  auto encode_res = classic_protocol::encode<
      classic_protocol::frame::Frame<classic_protocol::message::server::Ok>>(
      {seq_no_++,
       {affected_rows, last_insert_id, server_status, warning_count}},
      shared_capabilities(), net::dynamic_buffer(buf));

  if (!encode_res) {
    //
    return;
  }

  send_packet(buf);
}

void MySQLClassicProtocol::send_resultset(
    const ResultsetResponse &response,
    const std::chrono::microseconds delay_ms) {
  std::vector<uint8_t> buf;

  const auto shared_caps = shared_capabilities();

  auto encode_res = classic_protocol::encode<
      classic_protocol::frame::Frame<classic_protocol::wire::VarInt>>(
      {seq_no_++, {static_cast<long>(response.columns.size())}}, shared_caps,
      net::dynamic_buffer(buf));
  if (!encode_res) {
    //
    return;
  }

  for (const auto &column : response.columns) {
    encode_res = classic_protocol::encode<classic_protocol::frame::Frame<
        classic_protocol::message::server::ColumnMeta>>(
        {seq_no_++, column}, shared_caps, net::dynamic_buffer(buf));
    if (!encode_res) {
      //
      return;
    }
  }

  encode_res = classic_protocol::encode<
      classic_protocol::frame::Frame<classic_protocol::message::server::Eof>>(
      {seq_no_++, {}}, shared_caps, net::dynamic_buffer(buf));
  if (!encode_res) {
    //
    return;
  }

  std::this_thread::sleep_for(delay_ms);
  send_packet(buf);
  buf.clear();

  for (size_t i = 0; i < response.rows.size(); ++i) {
    std::vector<stdx::expected<std::string, void>> fields;

    auto const &row = response.rows[i];

    for (size_t f{}; f < response.columns.size(); ++f) {
      fields.push_back(row[f]);
    }

    encode_res = classic_protocol::encode<
        classic_protocol::frame::Frame<classic_protocol::message::server::Row>>(
        {seq_no_++, {fields}}, shared_caps, net::dynamic_buffer(buf));
    if (!encode_res) {
      //
      return;
    }

    send_packet(buf);
    buf.clear();
  }

  encode_res = classic_protocol::encode<
      classic_protocol::frame::Frame<classic_protocol::message::server::Eof>>(
      {seq_no_++, {}}, shared_caps, net::dynamic_buffer(buf));
  if (!encode_res) {
    //
    return;
  }

  send_packet(buf);
  buf.clear();
}

}  // namespace server_mock
