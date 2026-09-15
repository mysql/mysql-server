/* Copyright (c) 2026, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mrs_component_embedded_mysql_session.h"

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "components/mysql_rest_service/mrs_component.h"
#include "components/mysql_rest_service/mrs_component_embedded_mysql_result.h"
#include "components/mysql_rest_service/mrs_component_mysql_command_service_session.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs_component {

namespace {

using MySQLSession = mysqlrouter::MySQLSession;
using ResultRowProcessor = MySQLSession::ResultRowProcessor;
using FieldValidator = MySQLSession::FieldValidator;
using OnResultSetEnd = MySQLSession::OnResultSetEnd;

constexpr int k_connect_attempts = 3;
constexpr auto k_connect_retry_delay = std::chrono::milliseconds{250};

std::pair<std::string, std::string> split_auth_id(const std::string &auth_id) {
  const auto pos = auth_id.rfind('@');
  if (pos == std::string::npos || pos == 0 || pos + 1 >= auth_id.size()) {
    throw std::runtime_error("Invalid MySQL auth-id: " + auth_id);
  }

  return {auth_id.substr(0, pos), auth_id.substr(pos + 1)};
}

bool is_retryable_connect_error(const std::exception &e) {
  constexpr std::string_view k_embedded_connect_error{
      "Failed to connect embedded MySQL handle"};
  constexpr std::string_view k_command_connect_error{
      "Failed to connect MySQL command handle"};
  const std::string_view error{e.what()};
  return error.find(k_embedded_connect_error) != std::string_view::npos ||
         error.find(k_command_connect_error) != std::string_view::npos;
}

class CommandSessionStateIteratorGuard {
 public:
  explicit CommandSessionStateIteratorGuard(
      MYSQL_COMMAND_SESSION_STATE_ITERATOR_H iterator)
      : iterator_{iterator} {}

  ~CommandSessionStateIteratorGuard() {
    if (iterator_ != nullptr) cmd_session_state->deinit(iterator_);
  }

  CommandSessionStateIteratorGuard(const CommandSessionStateIteratorGuard &) =
      delete;
  CommandSessionStateIteratorGuard &operator=(
      const CommandSessionStateIteratorGuard &) = delete;

 private:
  MYSQL_COMMAND_SESSION_STATE_ITERATOR_H iterator_;
};

class StatementHandleGuard {
 public:
  explicit StatementHandleGuard(my_h_statement statement)
      : statement_{statement} {}

  ~StatementHandleGuard() {
    if (statement_ != nullptr) stmt_factory->close(statement_);
  }

  StatementHandleGuard(const StatementHandleGuard &) = delete;
  StatementHandleGuard &operator=(const StatementHandleGuard &) = delete;

  my_h_statement release() { return std::exchange(statement_, nullptr); }

 private:
  my_h_statement statement_;
};

std::string mysql_cstring_to_string(const mysql_cstring_with_length &value) {
  if (value.str == nullptr) return {};
  return {value.str, value.length};
}

std::vector<std::string> read_command_session_state(
    MYSQL_H mysql_h, mysql_command_session_state_type type) {
  std::vector<std::string> result;
  MYSQL_COMMAND_SESSION_STATE_ITERATOR_H iterator = nullptr;

  auto status = cmd_session_state->init(mysql_h, type, &iterator);
  if (status != MYSQL_COMMAND_SESSION_STATE_OK) return result;

  CommandSessionStateIteratorGuard iterator_guard{iterator};

  while (true) {
    size_t length = 0;
    status = cmd_session_state->get_next(iterator, nullptr, 0, &length);
    if (status == MYSQL_COMMAND_SESSION_STATE_END) break;
    if (status != MYSQL_COMMAND_SESSION_STATE_BUFFER_TOO_SMALL) return {};

    std::string value(length, '\0');
    status = cmd_session_state->get_next(iterator, value.data(), value.size(),
                                         &length);
    if (status != MYSQL_COMMAND_SESSION_STATE_OK) return {};

    value.resize(length);
    result.emplace_back(std::move(value));
  }

  return result;
}

class EmbeddedMysqlSession : public MySQLSession {
 public:
  using MySQLSession::connect;
  using MySQLSession::query_one;

  explicit EmbeddedMysqlSession(bool bypass_max_connections)
      : bypass_max_connections_{bypass_max_connections} {}
  ~EmbeddedMysqlSession() override { cleanup(); }

  EmbeddedMysqlSession(const EmbeddedMysqlSession &) = delete;
  EmbeddedMysqlSession &operator=(const EmbeddedMysqlSession &) = delete;
  EmbeddedMysqlSession(EmbeddedMysqlSession &&) = delete;
  EmbeddedMysqlSession &operator=(EmbeddedMysqlSession &&) = delete;

  void connect(const std::string &host, unsigned int port,
               const std::string &auth_id,
               const mysql_harness::SecureString &password,
               const std::string &unix_socket,
               const std::string &default_schema, int connect_timeout,
               int read_timeout, unsigned long extra_client_flags) override {
    if (!password.empty()) {
      throw std::logic_error(
          "Embedded MySQL session does not authenticate passwords");
    }

    (void)host;
    (void)port;
    (void)unix_socket;
    (void)connect_timeout;
    (void)read_timeout;

    cleanup();
    default_schema_ = default_schema;
    extra_client_flags_ = extra_client_flags | CLIENT_PROTOCOL_41 |
                          CLIENT_SESSION_TRACK | CLIENT_DEPRECATE_EOF |
                          CLIENT_MULTI_RESULTS;
    const auto [connect_user, connect_host] = split_auth_id(auth_id);

    // Loader startup can race slightly with post-restart internal handle
    // readiness in component mode. Retry only that attach path.
    for (int attempt = 1; attempt <= k_connect_attempts; ++attempt) {
      try {
        init_command_session(connect_user, connect_host);
        initialize_session_state();
        return;
      } catch (const std::exception &e) {
        cleanup();
        if (attempt == k_connect_attempts || !is_retryable_connect_error(e)) {
          throw;
        }
        std::this_thread::sleep_for(k_connect_retry_delay);
      }
    }
  }

  void change_user(const std::string &user,
                   const mysql_harness::SecureString &password,
                   const std::string &db) override {
    connect(std::string{}, 0, user, password, std::string{}, db, 0, 0,
            extra_client_flags_);
  }

  void disconnect() override { cleanup(); }

  void reset() override {
    close_prepared_statements();

    if (!command_service_session_.reset() ||
        !command_service_session_.autocommit(true)) {
      throw_query_error();
    }
  }

  uint64_t prepare(const std::string &query) override {
    ensure_connected();
    last_error_is_statement_ = false;

    my_h_statement stmt = nullptr;
    if (stmt_factory->init(&stmt) != 0 || stmt == nullptr) {
      throw std::runtime_error("Failed to initialize embedded MySQL statement");
    }
    StatementHandleGuard stmt_guard{stmt};

    configure_statement_handle(stmt);

    mysql_cstring_with_length stmt_query{query.c_str(), query.size()};
    if (stmt_execute->prepare(stmt_query, stmt) != 0) {
      refresh_statement_error(stmt);
      throw make_query_preparation_error(query, last_error_code_,
                                         last_error_message_);
    }

    const auto statement_id = next_statement_id_++;
    statements_.emplace(statement_id, stmt_guard.release());
    return statement_id;
  }

  void prepare_execute_with_bind_parameters(
      uint64_t ps_id, std::vector<MYSQL_BIND> bind_parameters,
      const ResultRowProcessor &processor, const FieldValidator &validator,
      const OnResultSetEnd &on_resultset_end) override {
    last_error_is_statement_ = false;
    auto stmt = statement_for(ps_id);

    uint32_t parameter_count = 0;
    if (stmt_metadata->param_count(stmt, &parameter_count) != 0) {
      throw_statement_error(stmt,
                            "Error reading embedded MySQL statement metadata");
    }
    if (bind_parameters.size() != static_cast<size_t>(parameter_count)) {
      throw std::logic_error(
          "Invalid embedded MySQL statement parameter count");
    }

    for (uint32_t i = 0; i < parameter_count; ++i) {
      bind_statement_parameter(stmt, i, bind_parameters[i]);
    }

    if (stmt_execute->execute(stmt) != 0) {
      throw_statement_error(stmt, "Error executing embedded MySQL statement");
    }

    while (true) {
      const auto result_status =
          process_statement_result(stmt, processor, validator);
      if (result_status == StatementResultStatus::kError) {
        throw_statement_error(stmt,
                              "Error reading embedded MySQL statement result");
      }

      on_resultset_end();

      bool has_next = false;
      if (stmt_result->next_result(stmt, &has_next) != 0) {
        throw_statement_error(
            stmt, "Error advancing embedded MySQL statement result");
      }
      if (!has_next) break;
    }
  }

  void prepare_remove(uint64_t ps_id) override {
    auto it = statements_.find(ps_id);
    if (it == statements_.end()) return;

    stmt_factory->close(it->second);
    statements_.erase(it);
  }

  void execute(const std::string &query) override {
    const ResultRowProcessor processor = [](const ResultRow &) { return true; };
    const FieldValidator validator = [](unsigned, MYSQL_FIELD *) {};
    execute_command_query(query, processor, validator);
  }

  void query(const std::string &query, const ResultRowProcessor &processor,
             const FieldValidator &validator) override {
    execute_command_query(query, processor, validator);
  }

  std::unique_ptr<MySQLSession::ResultRow> query_one(
      const std::string &query, const FieldValidator &validator) override {
    std::unique_ptr<MySQLSession::ResultRow> result;
    bool first_row = true;
    const ResultRowProcessor processor = [&result,
                                          &first_row](const ResultRow &row) {
      if (!first_row) return false;

      result = clone_result_row(row);
      first_row = false;
      return false;
    };

    this->query(query, processor, validator);
    return result;
  }

  int ping() override { return 0; }

  bool execute_nb(const std::string &query) override {
    (void)query;
    throw std::logic_error(
        "Embedded MySQL session does not support nonblocking execution");
  }

  uint64_t last_insert_id() noexcept override {
    auto *mysql_handle = get_handle();
    return mysql_handle ? mysql_insert_id(mysql_handle) : 0;
  }

  uint64_t affected_rows() noexcept override {
    auto mysql_h = command_service_session_.handle();
    uint64_t rows = 0;
    if (mysql_h != nullptr && cmd_query->affected_rows(mysql_h, &rows) == 0) {
      return rows;
    }
    return 0;
  }

  unsigned warning_count() noexcept override {
    auto *mysql_handle = get_handle();
    return mysql_handle ? mysql_warning_count(mysql_handle) : 0;
  }

  std::string quote(const std::string &s, char qchar = '\'') const override {
    auto *mysql_handle = const_cast<EmbeddedMysqlSession *>(this)->get_handle();
    if (mysql_handle == nullptr) {
      throw std::logic_error("Embedded MySQL session is not connected");
    }

    std::string result(s.length() * 2 + 3, '\0');
    result[0] = qchar;
    const auto escaped = mysql_real_escape_string_quote(
        mysql_handle, &result[1], s.c_str(), s.length(), qchar);
    result.resize(escaped + 2);
    result[escaped + 1] = qchar;
    return result;
  }

  bool is_connected() noexcept override {
    return command_service_session_.is_connected();
  }

  const char *last_sqlstate() override {
    if (last_error_is_statement_) return last_sqlstate_.c_str();

    last_sqlstate_ = command_service_session_.last_sqlstate();
    return last_sqlstate_.c_str();
  }

  const char *last_error() override {
    if (last_error_is_statement_) return last_error_message_.c_str();

    last_error_message_ = command_service_session_.last_error();
    return last_error_message_.c_str();
  }

  unsigned int last_errno() override {
    if (last_error_is_statement_) return last_error_code_;

    return command_service_session_.last_errno();
  }

  const char *ssl_cipher() override {
    auto *mysql_handle = get_handle();
    return mysql_handle ? mysql_get_ssl_cipher(mysql_handle) : nullptr;
  }

  bool has_data_on_socket() override { return false; }

  std::vector<std::string> get_session_tracker_data(
      enum enum_session_state_type type) override {
    auto mysql_h = command_service_session_.handle();
    if (mysql_h == nullptr) return {};

    return read_command_session_state(
        mysql_h, static_cast<mysql_command_session_state_type>(type));
  }

  MYSQL *get_handle() override { return command_service_session_.mysql(); }

  bool is_ssl_session_reused() override { return false; }

  unsigned long server_version() override {
    auto *mysql_handle = get_handle();
    return mysql_handle ? mysql_get_server_version(mysql_handle) : 0;
  }

 private:
  void init_command_session(const std::string &connect_user,
                            const std::string &connect_host) {
    try {
      command_service_session_.connect(connect_user, connect_host, true,
                                       extra_client_flags_,
                                       bypass_max_connections_);
    } catch (const Mysql_command_service_session::Connection_error &e) {
      throw MySQLSession::Error(e.what(), e.error_code(), e.what());
    }
  }

  void initialize_session_state() {
    if (!command_service_session_.reset() ||
        !command_service_session_.autocommit(true)) {
      refresh_query_error();
      throw std::runtime_error(last_error_message_);
    }

    // mysql_reset_connection() clears session state but does not restore this
    // wrapper's configured default schema.
    initialize_default_schema();
  }

  void initialize_default_schema() {
    if (!default_schema_.empty()) {
      mysqlrouter::sqlstring use_schema{"USE !"};
      use_schema << default_schema_;
      execute(use_schema);
    }
  }

  void ensure_connected() {
    if (!command_service_session_.is_connected()) {
      throw std::logic_error("Embedded MySQL session is not connected");
    }
  }

  void clear_command_result_metadata() {
    auto *mysql_handle = get_handle();
    if (mysql_handle == nullptr) return;

    mysql_handle->field_count = 0;
    mysql_handle->fields = nullptr;
  }

  void refresh_query_error() {
    last_error_is_statement_ = false;
    last_error_code_ = command_service_session_.last_errno();
    last_error_message_ = command_service_session_.last_error();
    last_sqlstate_ = command_service_session_.last_sqlstate();
  }

  void refresh_statement_error(my_h_statement stmt) {
    last_error_is_statement_ = true;

    uint64_t error_code = 0;
    if (stmt_diagnostics->error_number(stmt, &error_code) == 0) {
      last_error_code_ = static_cast<unsigned int>(error_code);
    } else {
      last_error_code_ = 0;
    }

    mysql_cstring_with_length error{};
    if (stmt_diagnostics->error(stmt, &error) == 0) {
      last_error_message_ = mysql_cstring_to_string(error);
    } else {
      last_error_message_.clear();
    }

    mysql_cstring_with_length sqlstate{};
    if (stmt_diagnostics->sqlstate(stmt, &sqlstate) == 0) {
      last_sqlstate_ = mysql_cstring_to_string(sqlstate);
    } else {
      last_sqlstate_.clear();
    }
  }

  [[noreturn]] void throw_last_error() const {
    throw MySQLSession::Error(last_error_message_, last_error_code_,
                              last_error_message_);
  }

  [[noreturn]] void throw_query_error() {
    refresh_query_error();
    throw_last_error();
  }

  [[noreturn]] void throw_query_error(const std::string &query) {
    refresh_query_error();
    throw make_query_execution_error(query, last_error_code_,
                                     last_error_message_);
  }

  [[noreturn]] void throw_statement_error(my_h_statement stmt,
                                          std::string_view context) {
    refresh_statement_error(stmt);
    if (last_error_message_.empty()) last_error_message_ = std::string(context);

    throw MySQLSession::Error(std::string(context) + ": " + last_error_message_,
                              last_error_code_, last_error_message_);
  }

  my_h_statement statement_for(uint64_t ps_id) const {
    auto it = statements_.find(ps_id);
    if (it == statements_.end()) {
      throw std::logic_error("Unknown embedded MySQL prepared statement id");
    }

    return it->second;
  }

  void bind_statement_parameter(my_h_statement stmt, uint32_t index,
                                const MYSQL_BIND &bind) {
    const bool is_null = bind.is_null != nullptr && *bind.is_null;
    if (stmt_bind->bind_param(
            stmt, index, is_null,
            mysql_bind_type_to_statement_type(bind.buffer_type),
            bind.is_unsigned, is_null ? nullptr : bind.buffer,
            mysql_bind_data_length(bind), nullptr, 0) != 0) {
      throw_statement_error(stmt, "Error binding embedded MySQL statement");
    }
  }

  void execute_command_query(const std::string &query,
                             const ResultRowProcessor &processor,
                             const FieldValidator &validator) {
    ensure_connected();
    last_error_is_statement_ = false;
    clear_command_result_metadata();

    auto mysql_h = command_service_session_.handle();
    if (cmd_query->query(mysql_h, query.data(), query.length()) != 0) {
      throw_query_error(query);
    }

    unsigned int field_count = 0;
    if (cmd_field_info->field_count(mysql_h, &field_count) != 0) {
      throw_query_error(query);
    }
    if (field_count == 0) return;

    const auto result_status =
        process_command_result(mysql_h, processor, validator);
    if (result_status == CommandResultStatus::kError) {
      throw_query_error(query);
    }
  }

  void close_prepared_statements() noexcept {
    for (auto &statement : statements_) stmt_factory->close(statement.second);
    statements_.clear();
  }

  void cleanup() {
    close_prepared_statements();
    command_service_session_.close();
    last_error_is_statement_ = false;
  }

  const bool bypass_max_connections_;
  Mysql_command_service_session command_service_session_;
  uint64_t next_statement_id_{1};
  std::unordered_map<uint64_t, my_h_statement> statements_;
  unsigned int last_error_code_{0};
  bool last_error_is_statement_{false};
  std::string last_error_message_;
  std::string last_sqlstate_;
  std::string default_schema_;
  unsigned long extra_client_flags_{0};
};

}  // namespace

std::unique_ptr<mysqlrouter::MySQLSession> make_embedded_mysql_session(
    bool bypass_max_connections) {
  return std::make_unique<EmbeddedMysqlSession>(bypass_max_connections);
}

}  // namespace mrs_component
