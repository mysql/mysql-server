/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef MRS_COMPONENT_COMMAND_EXECUTOR_H
#define MRS_COMPONENT_COMMAND_EXECUTOR_H

#include "components/mysql_rest_service/mrs_component.h"
#include "components/mysql_rest_service/mrs_component_mysql_command_service_session.h"
#include "mysql/components/services/mysql_command_services.h"
#include "mysql/components/services/mysql_statement_service.h"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace mrs_component {

using Row = std::vector<std::string>;
using Resultset = std::vector<Row>;

/**
  Executes text queries and prepared statements through component services.
*/
class Command_executor {
 public:
  enum class Mode : uint8_t { Query, PreparedStatement };

  class Connection_error : public std::runtime_error {
   public:
    Connection_error(const std::string &message, unsigned int error_code)
        : std::runtime_error(message), m_error_code(error_code) {}

    [[nodiscard]] unsigned int error_code() const { return m_error_code; }

   private:
    unsigned int m_error_code;
  };

  // Factory methods
  static Command_executor make_with_current_ctx(Mode mode);
  static Command_executor make_with_explicit_ctx(std::string user,
                                                 std::string host, Mode mode);

  ~Command_executor();

  Command_executor() = delete;
  Command_executor(const Command_executor &) = delete;
  Command_executor &operator=(const Command_executor &) = delete;
  Command_executor(Command_executor &&) = delete;
  Command_executor &operator=(Command_executor &&) = delete;

  // Common to prepared statements and queries
  void set_query(const std::string &query_text);
  void execute(Resultset *result = nullptr);

  // For prepared statements only
  void bind_param(uint32_t index, const std::string &value, uint64_t type,
                  bool is_unsigned = false);
  void bind_param(uint32_t index, const char *value, uint64_t type,
                  bool is_unsigned = false) {
    bind_param(index, std::string(value), type, is_unsigned);
  }
  template <typename T>
  void bind_param(uint32_t index, const T &value, uint64_t type,
                  bool is_unsigned = false) {
    if (m_mode != Mode::PreparedStatement) {
      throw std::logic_error("Invalid mode");
    }

    if (stmt_bind->bind_param(m_stmt, index, false, type, is_unsigned, &value,
                              sizeof(T), nullptr, 0) != 0) {
      throw std::runtime_error(get_last_error());
    }
  }

  bool fetch(my_h_row *row);
  uint64_t affected_rows();
  uint64_t last_insert_id();

  // Escape string
  std::string escape_string(const std::string &input);

  // Getters
  [[nodiscard]] bool is_explicit_mode() const { return m_explicit_ctx_mode; }
  [[nodiscard]] Mode mode() const { return m_mode; }
  [[nodiscard]] const std::string &get_query() const { return m_query; }
  [[nodiscard]] std::string get_last_error() const;
  [[nodiscard]] int get_last_error_code() const { return m_last_error_code; }

  // Result getters for Prepared Statements
  std::optional<uint64_t> get_uint64_result();
  std::optional<int64_t> get_int64_result();
  std::optional<double> get_double_result();
  std::optional<std::string> get_string_result();

 private:
  Command_executor(Mode mode, bool threaded_mode,
                   std::optional<std::string> connect_user = std::nullopt,
                   std::optional<std::string> connect_host = std::nullopt);

  // Internal setup and exec
  void connect();
  void cleanup();
  // Clears per-statement state but keeps the connection alive
  void reset() noexcept;
  void execute_query(Resultset *result);
  void execute_statement(Resultset *result);

  // Aux methods
  std::string fetch_column_as_string(my_h_statement stmt, my_h_row row,
                                     uint32_t col_index);

  std::string m_query;
  Mode m_mode;
  bool m_explicit_ctx_mode = false;

  // TODO(miguel): Add a wrapper classes to hold m_stmt and m_mysql_res to
  // manage the life-cycle of those
  Mysql_command_service_session m_command_service_session;
  my_h_statement m_stmt = nullptr;    // handle for statement service
  MYSQL_RES_H m_mysql_res = nullptr;  // handle for cmd service query result

  Security_context_handle m_security_context{
      nullptr};  // handle for session security context
  std::string m_connect_user;
  std::string m_connect_host;

  unsigned int m_last_error_code{0};
};

}  // namespace mrs_component

#endif  // MRS_COMPONENT_COMMAND_EXECUTOR_H
