/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQLD_MOCK_CLASSIC_MOCK_SESSION_INCLUDED
#define MYSQLD_MOCK_CLASSIC_MOCK_SESSION_INCLUDED

#include "mock_session.h"

#include <memory>  // unique_ptr

#include <openssl/ssl.h>

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"

#include "mysql/harness/tls_context.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_message.h"

namespace server_mock {

class MySQLClassicProtocol : public ProtocolBase {
 public:
  using ProtocolBase::ProtocolBase;

  stdx::expected<size_t, std::error_code> read_packet(
      std::vector<uint8_t> &payload);

  // throws std::system_error
  void encode_error(const ErrorResponse &msg) override;

  // throws std::system_error
  void encode_ok(const uint64_t affected_rows = 0,
                 const uint64_t last_insert_id = 0,
                 const uint16_t server_status = 0,
                 const uint16_t warning_count = 0) override;

  // throws std::system_error
  void encode_resultset(const ResultsetResponse &response) override;

  void encode_auth_fast_message();

  void encode_auth_switch_message(
      const classic_protocol::message::server::AuthMethodSwitch &msg);

  void encode_server_greeting(
      const classic_protocol::message::server::Greeting &greeting);

  void seq_no(uint8_t no) { seq_no_ = no; }

  uint8_t seq_no() const { return seq_no_; }

  classic_protocol::capabilities::value_type server_capabilities() const {
    return server_capabilities_;
  }

  void server_capabilities(classic_protocol::capabilities::value_type v) {
    server_capabilities_ = v;
  }

  classic_protocol::capabilities::value_type client_capabilities() const {
    return client_capabilities_;
  }

  void client_capabilities(classic_protocol::capabilities::value_type v) {
    client_capabilities_ = v;
  }

  classic_protocol::capabilities::value_type shared_capabilities() const {
    return client_capabilities_ & server_capabilities_;
  }

 private:
  uint8_t seq_no_{0};

  classic_protocol::capabilities::value_type server_capabilities_{};
  classic_protocol::capabilities::value_type client_capabilities_{};
};

class MySQLServerMockSessionClassic : public MySQLServerMockSession {
 public:
  MySQLServerMockSessionClassic(
      MySQLClassicProtocol protocol,
      std::unique_ptr<StatementReaderBase> statement_processor,
      const bool debug_mode, const bool with_tls)
      : MySQLServerMockSession(std::move(statement_processor), debug_mode),
        protocol_{std::move(protocol)},
        with_tls_{with_tls} {}

  void run() override;

  void cancel() override { protocol_.cancel(); }

 private:
  void server_greeting();
  void client_greeting();
  void auth_switched();
  void idle();
  void send_response_then_idle();
  void send_response_then_disconnect();
  void finish();

  bool authenticate(const std::vector<uint8_t> &client_auth_method_data);

  MySQLClassicProtocol protocol_;

  bool with_tls_{false};
};

}  // namespace server_mock

#endif  // MYSQLD_MOCK_CLASSIC_MOCK_SESSION_INCLUDED
