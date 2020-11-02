/*
  Copyright (c) 2018, 2020, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLD_MOCK_STATEMENT_READER_INCLUDED
#define MYSQLD_MOCK_STATEMENT_READER_INCLUDED

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "mysql/harness/tls_server_context.h"
#include "mysql_protocol_common.h"
#include "mysqlrouter/classic_protocol_constants.h"

#include "authentication.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol_message.h"

namespace server_mock {

/** @brief Vector for keeping has_value|string representation of the values
 *         of the single row (ordered by column)
 **/
using RowValueType = std::vector<stdx::expected<std::string, void>>;

/** @brief Keeps result data for single SQL statement that returns
 *         resultset.
 **/
struct ResultsetResponse {
  std::vector<classic_protocol::message::server::ColumnMeta> columns;
  std::vector<RowValueType> rows;
};

using OkResponse = classic_protocol::message::server::Ok;
using ErrorResponse = classic_protocol::message::server::Error;

struct AsyncNotice {
  // how many milliseconds after the client connects this Notice
  // should be sent to the client
  std::chrono::milliseconds send_offset_ms;
  unsigned type;
  bool is_local;  // true = local, false = global
  std::string payload;
};

class ProtocolBase {
 public:
  ProtocolBase(net::ip::tcp::socket &&client_sock,
               net::impl::socket::native_handle_type wakeup_fd,
               TlsServerContext &tls_ctx);

  virtual ~ProtocolBase() = default;

  // throws std::system_error
  virtual void send_error(const uint16_t error_code,
                          const std::string &error_msg,
                          const std::string &sql_state = "HY000") = 0;

  void send_error(const ErrorResponse &resp) {
    send_error(resp.error_code(), resp.message(), resp.sql_state());
  }

  // throws std::system_error
  virtual void send_ok(const uint64_t affected_rows = 0,
                       const uint64_t last_insert_id = 0,
                       const uint16_t server_status = 0,
                       const uint16_t warning_count = 0) = 0;

  void send_ok(const OkResponse &resp) {
    send_ok(resp.affected_rows(), resp.last_insert_id(),
            resp.status_flags().to_ulong(), resp.warning_count());
  }

  // throws std::system_error
  virtual void send_resultset(const ResultsetResponse &response,
                              const std::chrono::microseconds delay_ms) = 0;

  void read_buffer(net::mutable_buffer &buf);

  void send_buffer(net::const_buffer buf);

  stdx::expected<bool, std::error_code> socket_has_data(
      std::chrono::milliseconds timeout);

  const net::ip::tcp::socket &client_socket() const { return client_socket_; }

  void username(const std::string &username) { username_ = username; }

  std::string username() const { return username_; }

  void auth_method_name(const std::string &auth_method_name) {
    auth_method_name_ = auth_method_name;
  }

  std::string auth_method_name() const { return auth_method_name_; }

  void auth_method_data(const std::string &auth_method_data) {
    auth_method_data_ = auth_method_data;
  }

  std::string auth_method_data() const { return auth_method_data_; }

  static bool authenticate(const std::string &auth_method_name,
                           const std::string &auth_method_data,
                           const std::string &password,
                           const std::vector<uint8_t> &auth_response);

  void init_tls() {
    ssl_.reset(SSL_new(tls_ctx_.get()));
    SSL_set_fd(ssl_.get(), client_socket_.native_handle());
  }

  bool is_tls() { return bool(ssl_); }

  const SSL *ssl() const { return ssl_.get(); }

  stdx::expected<void, std::error_code> tls_accept();

 private:
  net::ip::tcp::socket client_socket_;
  net::impl::socket::native_handle_type
      wakeup_fd_;  // socket to interrupt blocking polls

  std::string username_{};
  std::string auth_method_name_{};
  std::string auth_method_data_{};

  TlsServerContext &tls_ctx_;

  class SSL_Deleter {
   public:
    void operator()(SSL *v) { SSL_free(v); }
  };

  std::unique_ptr<SSL, SSL_Deleter> ssl_;
};

class StatementReaderBase {
 public:
  /** @brief Returns the data about the next statement from the
   *         json file. If there is no more statements it returns
   *         empty statement.
   **/
  virtual void handle_statement(const std::string &statement,
                                ProtocolBase *protocol) = 0;

  /** @brief Returns the default execution time in microseconds. If
   *         no default execution time is provided in json file, then
   *         0 microseconds is returned.
   **/
  virtual std::chrono::microseconds get_default_exec_time() = 0;

  virtual std::vector<AsyncNotice> get_async_notices() = 0;

  virtual stdx::expected<classic_protocol::message::server::Greeting,
                         std::error_code>
  server_greeting(bool with_tls) = 0;

  struct account_data {
    stdx::expected<std::string, void> username;
    stdx::expected<std::string, void> password;
    bool cert_required{false};
    stdx::expected<std::string, void> cert_subject;
    stdx::expected<std::string, void> cert_issuer;
  };

  virtual stdx::expected<account_data, std::error_code> account() = 0;

  virtual std::chrono::microseconds server_greeting_exec_time() = 0;

  virtual void set_session_ssl_info(const SSL *ssl) = 0;

  virtual ~StatementReaderBase() = default;
};

}  // namespace server_mock

#endif
