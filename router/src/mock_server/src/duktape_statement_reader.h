/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQLD_MOCK_DUKTAPE_STATEMENT_READER_INCLUDED
#define MYSQLD_MOCK_DUKTAPE_STATEMENT_READER_INCLUDED

#include <map>
#include <string>

#include "mock_session.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/mock_server_global_scope.h"
#include "statement_reader.h"

namespace server_mock {

class DuktapeStatementReaderFactory {
 public:
  DuktapeStatementReaderFactory(
      std::string filename, std::vector<std::string> module_prefixes,
      std::map<std::string, std::string> session,
      std::shared_ptr<MockServerGlobalScope> global_scope)
      : filename_{std::move(filename)},
        module_prefixes_{std::move(module_prefixes)},
        session_{std::move(session)},
        global_scope_{std::move(global_scope)} {}

  class FailedStatementReader : public StatementReaderBase {
   public:
    FailedStatementReader(std::string what) : what_{std::move(what)} {}

    void handle_statement(const std::string & /* statement */,
                          ProtocolBase * /* protocol */) override {
      throw std::logic_error("this should not be called.");
    }

    std::chrono::microseconds get_default_exec_time() override { return {}; }

    std::vector<AsyncNotice> get_async_notices() override { return {}; }

    stdx::expected<classic_protocol::message::server::Greeting, std::error_code>
    server_greeting(bool /* with_tls */) override {
      return stdx::make_unexpected(
          make_error_code(std::errc::no_such_file_or_directory));
    }

    stdx::expected<handshake_data, ErrorResponse> handshake() override {
      return stdx::make_unexpected(ErrorResponse(1064, what_, "HY000"));
    }

    std::chrono::microseconds server_greeting_exec_time() override {
      return {};
    }

    void set_session_ssl_info(const SSL * /* ssl */) override {}

   private:
    std::string what_;
  };

  std::unique_ptr<StatementReaderBase> operator()();

 private:
  std::string filename_;
  std::vector<std::string> module_prefixes_;
  std::map<std::string, std::string> session_;
  std::shared_ptr<MockServerGlobalScope> global_scope_;
};

class DuktapeStatementReader : public StatementReaderBase {
 public:
  enum class HandshakeState { INIT, GREETED, AUTH_SWITCHED, DONE };

  DuktapeStatementReader(std::string filename,
                         std::vector<std::string> module_prefixes,
                         std::map<std::string, std::string> session_data,
                         std::shared_ptr<MockServerGlobalScope> shared_globals);

  DuktapeStatementReader(const DuktapeStatementReader &) = delete;
  DuktapeStatementReader(DuktapeStatementReader &&);

  DuktapeStatementReader &operator=(const DuktapeStatementReader &) = delete;
  DuktapeStatementReader &operator=(DuktapeStatementReader &&);

  /**
   * handle the clients statement
   *
   * @param statement statement-text of the current clients
   *                  COM_QUERY/StmtExecute
   * @param protocol protocol to send response to
   */
  void handle_statement(const std::string &statement,
                        ProtocolBase *protocol) override;

  std::chrono::microseconds get_default_exec_time() override;

  ~DuktapeStatementReader() override;

  std::vector<AsyncNotice> get_async_notices() override;

  stdx::expected<classic_protocol::message::server::Greeting, std::error_code>
  server_greeting(bool with_tls) override;

  stdx::expected<handshake_data, ErrorResponse> handshake() override;

  std::chrono::microseconds server_greeting_exec_time() override;

  void set_session_ssl_info(const SSL *ssl) override;

 private:
  struct Pimpl;
  std::unique_ptr<Pimpl> pimpl_;
  bool has_notices_{false};

  HandshakeState handshake_state_{HandshakeState::INIT};
};
}  // namespace server_mock

#endif
