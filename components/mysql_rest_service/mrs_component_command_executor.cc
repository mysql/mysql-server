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

#include "mrs_component_command_executor.h"
#include "mrs_component.h"
#include "mrs_component_log.h"

#include "field_types.h"
#include "my_sys.h"
#include "mysql/components/services/bits/my_err_bits.h"
#include "mysql/components/services/bits/mysql_field_types_bits.h"
#include "mysql/components/services/mysql_current_thread_reader.h"
#include "mysql/mysql_lex_string.h"
#include "mysql/strings/m_ctype.h"

namespace mrs_component {

Command_executor Command_executor::make_with_current_ctx(Mode mode) {
  return {mode, false};
}

Command_executor Command_executor::make_with_explicit_ctx(std::string user,
                                                          std::string host,
                                                          Mode mode) {
  return {mode, true, std::move(user), std::move(host)};
}

Command_executor::Command_executor(Mode mode, bool threaded_mode,
                                   std::optional<std::string> connect_user,
                                   std::optional<std::string> connect_host)
    : m_mode(mode), m_explicit_ctx_mode(threaded_mode) {
  if (threaded_mode) {
    if (!connect_user || connect_user->empty() || !connect_host ||
        connect_host->empty()) {
      throw std::runtime_error(
          "Explicit mode requires non-empty user and host");
    }
    m_connect_user = *connect_user;
    m_connect_host = *connect_host;
  }

  try {
    connect();  // init thread ctx & mysql_h & init m_stmt
  } catch (...) {
    cleanup();
    throw;
  }
}

Command_executor::~Command_executor() { cleanup(); }

void Command_executor::connect() {
  // Read caller thread and get its security context
  if (!is_explicit_mode()) {
    MYSQL_THD curr_thd = nullptr;
    MYSQL_LEX_CSTRING user_{}, host_{};

    if (mysql_service_mysql_current_thread_reader->get(&curr_thd) != 0) {
      throw std::runtime_error("Failed to get current thread context");
    }

    // Get current thd security context and user/host
    if (thread_security_ctx->get(curr_thd, &m_security_context) != 0) {
      throw std::runtime_error("Failed to get current thread security context");
    }

    if (thread_security_ctx_opts->get(m_security_context, "priv_user",
                                      &user_) != 0) {
      throw std::runtime_error("Failed to get user from security context");
    }
    m_connect_user.assign(user_.str, user_.length);

    if (thread_security_ctx_opts->get(m_security_context, "priv_host",
                                      &host_) != 0) {
      throw std::runtime_error("Failed to get host from security context");
    }
    m_connect_host.assign(host_.str, host_.length);
  }

  try {
    // Bootstrap and metadata readiness must work at the connection limit.
    m_command_service_session.connect(m_connect_user, m_connect_host,
                                      is_explicit_mode(), 0,
                                      /*bypass_max_connections=*/true);
  } catch (const Mysql_command_service_session::Connection_error &e) {
    throw Connection_error(e.what(), e.error_code());
  } catch (...) {
    throw std::runtime_error("Failed to connect to MySQL server");
  }

  // Set the SQL mode and enable autocommit
  const char *sql_mode = "SET @@SESSION.sql_mode='NO_ENGINE_SUBSTITUTION'";
  if (!m_command_service_session.reset() ||
      !m_command_service_session.autocommit(true) ||
      cmd_query->query(m_command_service_session.handle(), sql_mode,
                       strlen(sql_mode)) != 0) {
    throw std::runtime_error("Failed to initialize MySQL connection");
  }

  // If Prepared Statement, init statement factory
  if (m_mode == Mode::PreparedStatement) {
    if (stmt_factory->init(&m_stmt) != 0 || (m_stmt == nullptr)) {
      throw std::runtime_error("Failed to init MySQL statement factory");
    }
  }
}

void Command_executor::cleanup() {
  if (m_stmt != nullptr) {
    stmt_factory->close(m_stmt);
    m_stmt = nullptr;
  }
  if (m_mysql_res != nullptr) {
    cmd_query_result->free_result(m_mysql_res);
    m_mysql_res = nullptr;
  }
  m_command_service_session.close();
}

void Command_executor::reset() noexcept {
  // Clear stored stmt text
  m_query.clear();

  // Clear any pending resultset
  if (m_mysql_res != nullptr) {
    cmd_query_result->free_result(m_mysql_res);
    m_mysql_res = nullptr;
  }

  // Reset prepared statement buffers
  if (m_mode == Mode::PreparedStatement && m_stmt != nullptr) {
    stmt_execute->reset(m_stmt);
  }
}

void Command_executor::set_query(const std::string &query_text) {
  // Always start from a clean slate
  reset();

  m_query = query_text;

  if (m_mode == Mode::Query) return;

  mysql_cstring_with_length q{.str = m_query.c_str(), .length = m_query.size()};
  MRS_LOG_DEBUG("Preparing query: %s", q.str);

  if (stmt_execute->prepare(q, m_stmt) != 0) {
    throw std::runtime_error(get_last_error());
  }
}

std::string Command_executor::fetch_column_as_string(my_h_statement stmt,
                                                     my_h_row row,
                                                     uint32_t col_index) {
  // Fetch field metadata
  my_h_field field = nullptr;
  if (stmt_resultset_metadata->fetch_field(stmt, col_index, &field) != 0) {
    throw std::runtime_error("Failed to fetch field metadata");
  }

  mysql_field_type_t mysql_type = MYSQL_FIELD_TYPE_INVALID;
  if (stmt_resultset_metadata->field_info(field, "mysql_type", &mysql_type) !=
      0) {
    throw std::runtime_error("Failed to get column type");
  }
  auto type = static_cast<enum_field_types>(mysql_type);

  bool is_unsigned = false;
  if (stmt_resultset_metadata->field_info(field, "is_unsigned", &is_unsigned) !=
      0) {
    throw std::runtime_error("Failed to get column signedness");
  }

  bool is_null = false;

  switch (type) {
    case MYSQL_TYPE_LONGLONG:
      [[fallthrough]];
    case MYSQL_TYPE_LONG:
      [[fallthrough]];
    case MYSQL_TYPE_INT24:
      [[fallthrough]];
    case MYSQL_TYPE_SHORT:
      [[fallthrough]];
    case MYSQL_TYPE_TINY: {
      if (is_unsigned) {
        uint64_t value;
        if (stmt_get_unsigned_integer->get(row, col_index, &value, &is_null) !=
            0) {
          throw std::runtime_error("Failed to fetch unsigned integer column");
        }
        return is_null ? "NULL" : std::to_string(value);
      } else {
        int64_t value;
        if (stmt_get_integer->get(row, col_index, &value, &is_null) != 0) {
          throw std::runtime_error("Failed to fetch signed integer column");
        }
        return is_null ? "NULL" : std::to_string(value);
      }
    }

    case MYSQL_TYPE_FLOAT:
      [[fallthrough]];
    case MYSQL_TYPE_DOUBLE: {
      double value;
      if (stmt_get_double->get(row, col_index, &value, &is_null) != 0) {
        throw std::runtime_error("Failed to fetch float/double column");
      }
      return is_null ? "NULL" : std::to_string(value);
    }

    case MYSQL_TYPE_STRING:
      [[fallthrough]];
    case MYSQL_TYPE_VAR_STRING:
      [[fallthrough]];
    case MYSQL_TYPE_VARCHAR:
      [[fallthrough]];
    case MYSQL_TYPE_JSON:
      [[fallthrough]];
    case MYSQL_TYPE_BLOB: {
      mysql_cstring_with_length value{};
      if (stmt_get_string->get(row, col_index, &value, &is_null) != 0) {
        throw std::runtime_error("Failed to fetch string column");
      }
      return is_null ? "NULL" : std::string(value.str, value.length);
    }

    default:
      return "<unsupported_type>";
  }
}

void Command_executor::execute(Resultset *result) {
  if (m_mode == Mode::PreparedStatement) {
    execute_statement(result);
  } else {
    execute_query(result);
  }
}

void Command_executor::execute_statement(Resultset *result) {
  if (stmt_execute->execute(m_stmt) != 0) {
    uint64_t err_code = 0;
    stmt_diagnostics->error_number(m_stmt, &err_code);
    m_last_error_code = static_cast<uint>(err_code);
    throw std::runtime_error(get_last_error());
  }

  uint64_t num_rows;
  stmt_diagnostics->affected_rows(m_stmt, &num_rows);

  // If result is not needed return right away
  if (!result) return;

  uint32_t num_fields = 0;
  if (stmt_resultset_metadata->field_count(m_stmt, &num_fields) != 0) {
    throw std::runtime_error("Failed to fetch column count");
  }

  my_h_row row = nullptr;
  while (true) {
    if (stmt_result->fetch(m_stmt, &row) != 0) {
      throw std::runtime_error(get_last_error());
    }
    if (row == nullptr) break;

    Row row_data;
    for (uint32_t i = 0; i < num_fields; ++i) {
      row_data.emplace_back(fetch_column_as_string(m_stmt, row, i));
    }

    result->emplace_back(std::move(row_data));
  }
}

void Command_executor::execute_query(Resultset *result) {
  MRS_LOG_DEBUG("Executing query: %s", m_query.c_str());

  if (cmd_query->query(m_command_service_session.handle(), m_query.c_str(),
                       m_query.length()) != 0) {
    m_last_error_code = m_command_service_session.last_errno();
    if (m_last_error_code == 0) {
      MRS_LOG_ERROR("Failed to retrieve error code from query: %s",
                    m_query.c_str());
    }

    throw std::runtime_error(m_command_service_session.last_error());
  }

  // If result is not needed return right away
  if (result == nullptr) return;

  m_mysql_res = reinterpret_cast<MYSQL_RES_H>(1);
  if (cmd_query_result->store_result(m_command_service_session.handle(),
                                     &m_mysql_res) != 0) {
    // If mysql_res was set to nullptr the result set was empty
    if (m_mysql_res == nullptr) return;

    // Otherwise, we must reset mysql_res to avoid problems in cleanup
    m_mysql_res = nullptr;
    throw std::runtime_error("Failed to retrieve resultset");
  }

  // Prepare to fetch rows
  MYSQL_ROW_H row = nullptr;
  unsigned int num_column = 0;

  // Get column count
  if (cmd_field_info->num_fields(m_mysql_res, &num_column) != 0) {
    throw std::runtime_error("Failed to fetch resultset metadata");
  }

  while (true) {
    if (cmd_query_result->fetch_row(m_mysql_res, &row) != 0) {
      throw std::runtime_error("Cannot fetch row");
    }
    if (row == nullptr) break;

    Row row_data;
    for (unsigned int j = 0; j < num_column; ++j) {
      row_data.emplace_back((row[j] != nullptr) ? row[j] : "NULL");
    }
    result->push_back(std::move(row_data));
  }

  // Free result memory
  cmd_query_result->free_result(m_mysql_res);
  m_mysql_res = nullptr;
}

void Command_executor::bind_param(uint32_t index, const std::string &value,
                                  uint64_t type, bool is_unsigned) {
  if (m_mode != Mode::PreparedStatement) throw std::logic_error("Invalid mode");

  if (stmt_bind->bind_param(m_stmt, index, false, type, is_unsigned,
                            value.data(), value.size(), nullptr, 0) != 0) {
    throw std::runtime_error(get_last_error());
  }
}

bool Command_executor::fetch(my_h_row *row) {
  if (m_mode != Mode::PreparedStatement) throw std::logic_error("Invalid mode");

  if (stmt_result->fetch(m_stmt, row) != 0) {
    throw std::runtime_error(get_last_error());
  }
  return (*row != nullptr);
}

uint64_t Command_executor::affected_rows() {
  uint64_t num_rows = 0;
  if (m_mode == Mode::PreparedStatement) {
    stmt_diagnostics->affected_rows(m_stmt, &num_rows);
    return num_rows;
  }

  cmd_query->affected_rows(m_command_service_session.handle(), &num_rows);

  return num_rows;
}

uint64_t Command_executor::last_insert_id() {
  if (m_mode != Mode::PreparedStatement) throw std::logic_error("Invalid mode");

  uint64_t id = 0;
  if (stmt_diagnostics->insert_id(m_stmt, &id) != 0) {
    throw std::runtime_error(get_last_error());
  }

  return id;
}

std::string Command_executor::escape_string(const std::string &input) {
  const size_t len = input.length();
  std::string escaped(2 * len + 1, '\0');  // max possible expansion

  const size_t escaped_len =
      escape_string_for_mysql(&my_charset_utf8mb3_general_ci, escaped.data(),
                              escaped.size(), input.data(), len);
  escaped.resize(escaped_len);
  return escaped;
}

std::string Command_executor::get_last_error() const {
  mysql_cstring_with_length err{};

  if (m_mode == Mode::PreparedStatement) {
    stmt_diagnostics->error(m_stmt, &err);
    return {err.str, err.length};
  }

  return m_command_service_session.last_error();
}

std::optional<uint64_t> Command_executor::get_uint64_result() {
  if (m_mode != Mode::PreparedStatement) {
    throw std::logic_error(
        "get_uint64_result() only valid in PreparedStatement mode");
  }

  my_h_row row = nullptr;
  if (!fetch(&row) || row == nullptr) {
    return std::nullopt;
  }

  uint64_t val = 0;
  bool is_null = false;
  if (stmt_get_unsigned_integer->get(row, 0, &val, &is_null) != 0 || is_null) {
    throw std::runtime_error("Failed to retrieve uint64 result");
  }

  return val;
}

std::optional<int64_t> Command_executor::get_int64_result() {
  if (m_mode != Mode::PreparedStatement) {
    throw std::logic_error(
        "get_int64_result() only valid in PreparedStatement mode");
  }

  my_h_row row = nullptr;
  if (!fetch(&row) || row == nullptr) {
    return std::nullopt;
  }

  int64_t val = 0;
  bool is_null = false;
  if (stmt_get_integer->get(row, 0, &val, &is_null) != 0 || is_null) {
    throw std::runtime_error("Failed to retrieve int64 result");
  }

  return val;
}

std::optional<double> Command_executor::get_double_result() {
  if (m_mode != Mode::PreparedStatement) {
    throw std::logic_error(
        "get_double_result() only valid in PreparedStatement mode");
  }

  my_h_row row = nullptr;
  if (!fetch(&row) || row == nullptr) {
    return std::nullopt;
  }

  double val = 0.0;
  bool is_null = false;
  if (stmt_get_double->get(row, 0, &val, &is_null) != 0 || is_null) {
    throw std::runtime_error("Failed to retrieve double result");
  }

  return val;
}

std::optional<std::string> Command_executor::get_string_result() {
  if (m_mode != Mode::PreparedStatement) {
    throw std::logic_error(
        "get_string_result() only valid in PreparedStatement mode");
  }

  my_h_row row = nullptr;
  if (!fetch(&row) || row == nullptr) {
    return std::nullopt;
  }

  mysql_cstring_with_length val{};
  bool is_null = false;
  if (stmt_get_string->get(row, 0, &val, &is_null) != 0 || is_null) {
    throw std::runtime_error("Failed to retrieve string result");
  }

  return std::string(val.str, val.length);
}

}  // namespace mrs_component
