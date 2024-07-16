/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQLD_MOCK_X_MOCK_SESSION_INCLUDED
#define MYSQLD_MOCK_X_MOCK_SESSION_INCLUDED

#include <memory>
#include <mutex>
#include <set>

#include "mysql/harness/logging/logger.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql_server_mock.h"
#include "router/src/mock_server/src/mock_session.h"
#include "router/src/mock_server/src/statement_reader.h"
#include "x_protocol_decoder.h"
#include "x_protocol_encoder.h"

namespace server_mock {

class MySQLXProtocol : public ProtocolBase {
 public:
  using ProtocolBase::ProtocolBase;

  stdx::expected<size_t, std::error_code> decode_frame(
      std::vector<uint8_t> &payload);

  stdx::expected<std::tuple<uint8_t, size_t>, std::error_code> recv_header(
      const net::const_buffer &buf);

  stdx::expected<std::pair<xcl::XProtocol::Client_message_type_id,
                           std::unique_ptr<xcl::XProtocol::Message>>,
                 std::error_code>
  decode_single_message(const std::vector<uint8_t> &payload);

  // throws std::system_error
  void encode_error(const ErrorResponse &msg) override;

  // throws std::system_error
  void encode_ok(const OkResponse &msg) override;

  // throws std::system_error
  void encode_resultset(const ResultsetResponse &response) override;

  void encode_message(const xcl::XProtocol::Server_message_type_id msg_id,
                      const xcl::XProtocol::Message &msg);

  void encode_async_notice(const AsyncNotice &async_notice);

  Mysqlx::Notice::Frame notice_frame;
  stdx::expected<std::unique_ptr<xcl::XProtocol::Message>, std::string>
  gr_state_changed_from_json(const std::string &json_string);

  stdx::expected<std::unique_ptr<xcl::XProtocol::Message>, std::string>
  get_notice_message(const unsigned id, const std::string &payload);

 private:
  XProtocolEncoder protocol_encoder_;
  XProtocolDecoder protocol_decoder_;
};

class MySQLServerMockSessionX : public MySQLServerMockSession {
 public:
  using clock_type = std::chrono::steady_clock;

  MySQLServerMockSessionX(
      ProtocolBase::socket_type client_sock,
      ProtocolBase::endpoint_type client_ep, TlsServerContext &tls_server_ctx,
      std::unique_ptr<StatementReaderBase> statement_processor,
      const bool debug_mode, bool with_tls);

  void run() override;

  void cancel() override { protocol_.cancel(); }

  void terminate() override { protocol_.terminate(); }

  /**
   * encode all async notices.
   *
   * @param start_time start time
   *
   * @retval true one or more notice was added
   * @retval false no notice encoded.
   */
  bool encode_due_async_notices(const clock_type::time_point &start_time);

  /**
   * expiry of the notice.
   *
   * @return when the next notice needs to be sent.
   * @retval {} if no expiry
   */
  clock_type::time_point notice_expiry() const;

 private:
  void greeting();
  void handshake();
  void idle();
  void finish();
  void notices();

  void send_response_then_handshake();
  void send_response_then_first_idle();
  void send_response_then_idle();
  void send_response_then_disconnect();
  void send_notice_then_notices();

  std::vector<AsyncNotice> async_notices_;
  MySQLXProtocol protocol_;

  bool with_tls_{false};

  clock_type::time_point start_time_{};

  net::steady_timer notice_timer_{protocol_.io_context()};

  mysql_harness::logging::DomainLogger logger_;
};

}  // namespace server_mock

#endif  // MYSQLD_MOCK_X_MOCK_SESSION_INCLUDED
