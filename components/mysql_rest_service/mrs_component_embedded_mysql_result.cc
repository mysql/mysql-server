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

#include "mrs_component_embedded_mysql_result.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "components/mysql_rest_service/mrs_component.h"
#include "my_time.h"
#include "mysql/components/services/bits/mysql_field_types_bits.h"
#include "mysql/components/services/bits/stored_program_bits.h"
#include "mysql/strings/dtoa.h"
#include "mysqlrouter/component/http_server_component.h"
#include "string_with_len.h"

namespace mrs_component {

namespace {

constexpr size_t kStmtMinBufferCapacity = 1024 * 1024;
constexpr size_t k_stmt_prefetch_rows = 1;
constexpr const char k_empty[] = "";
char k_catalog[] = "def";

const char *nonnull_cstr(const char *value) { return value ? value : k_empty; }

unsigned int cstr_length(const char *value) {
  return static_cast<unsigned int>(std::strlen(nonnull_cstr(value)));
}

template <typename T>
bool fetch_field_info(my_h_field field, const char *name, T *value) {
  return stmt_resultset_metadata->field_info(field, name, value) == 0;
}

bool is_stmt_integer_type(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_TINY:
      [[fallthrough]];
    case MYSQL_TYPE_SHORT:
      [[fallthrough]];
    case MYSQL_TYPE_LONG:
      [[fallthrough]];
    case MYSQL_TYPE_LONGLONG:
      [[fallthrough]];
    case MYSQL_TYPE_INT24:
      [[fallthrough]];
    case MYSQL_TYPE_YEAR:
      [[fallthrough]];
    case MYSQL_TYPE_BOOL:
      return true;
    default:
      return false;
  }
}

bool is_stmt_temporal_type(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_TIMESTAMP:
      [[fallthrough]];
    case MYSQL_TYPE_DATE:
      [[fallthrough]];
    case MYSQL_TYPE_TIME:
      [[fallthrough]];
    case MYSQL_TYPE_DATETIME:
      [[fallthrough]];
    case MYSQL_TYPE_NEWDATE:
      [[fallthrough]];
    case MYSQL_TYPE_TIMESTAMP2:
      [[fallthrough]];
    case MYSQL_TYPE_DATETIME2:
      [[fallthrough]];
    case MYSQL_TYPE_TIME2:
      return true;
    default:
      return false;
  }
}

std::string format_stmt_time(const mle_time &time, uint decimals) {
  MYSQL_TIME mysql_time{};
  mysql_time.year = time.year;
  mysql_time.month = time.month;
  mysql_time.day = time.day;
  mysql_time.hour = time.hour;
  mysql_time.minute = time.minute;
  mysql_time.second = time.second;
  mysql_time.second_part = time.second_part;
  mysql_time.neg = time.neg;
  mysql_time.time_zone_displacement = time.time_zone_displacement;
  mysql_time.time_type = static_cast<enum_mysql_timestamp_type>(time.time_type);

  char buffer[MAX_DATE_STRING_REP_LENGTH]{};
  const auto length = my_TIME_to_str(mysql_time, buffer, decimals);
  return std::string(buffer, length);
}

std::string format_stmt_floating_point(double value, enum_field_types type,
                                       uint decimals) {
  char buffer[FLOATING_POINT_BUFFER + 1]{};
  size_t length = 0;
  if (decimals < DECIMAL_NOT_SPECIFIED) {
    length = my_fcvt(value, decimals, buffer, nullptr);
  } else {
    const auto arg_type =
        type == MYSQL_TYPE_FLOAT ? MY_GCVT_ARG_FLOAT : MY_GCVT_ARG_DOUBLE;
    length = my_gcvt(value, arg_type, FLOATING_POINT_BUFFER, buffer, nullptr);
  }
  return std::string(buffer, length);
}

std::optional<unsigned long> fixed_bind_data_length(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_LONG:
      return static_cast<unsigned long>(sizeof(int32_t));
    case MYSQL_TYPE_DOUBLE:
      return static_cast<unsigned long>(sizeof(double));
    case MYSQL_TYPE_LONGLONG:
      return static_cast<unsigned long>(sizeof(int64_t));
    default:
      return std::nullopt;
  }
}

struct StmtField {
  MYSQL_FIELD mysql_field{};
  bool is_unsigned{false};
};

class OwnedResultRow : public mysqlrouter::MySQLSession::ResultRow {
 public:
  explicit OwnedResultRow(std::vector<std::optional<std::string>> values)
      : mysqlrouter::MySQLSession::ResultRow(make_empty_row(values.size())),
        values_{std::move(values)} {
    for (size_t i = 0; i < values_.size(); ++i) {
      (*this)[i] = values_[i] ? values_[i]->c_str() : nullptr;
    }
  }

  size_t get_data_size(size_t i) const override {
    return values_[i] ? values_[i]->size() : 0;
  }

 private:
  static mysqlrouter::MySQLSession::Row make_empty_row(size_t size) {
    return mysqlrouter::MySQLSession::Row(size, nullptr);
  }

  std::vector<std::optional<std::string>> values_;
};

class CommandResultRow : public mysqlrouter::MySQLSession::ResultRow {
 public:
  CommandResultRow(mysqlrouter::MySQLSession::Row row, const ulong *lengths)
      : mysqlrouter::MySQLSession::ResultRow(std::move(row)),
        lengths_{lengths} {}

  size_t get_data_size(size_t i) const override { return lengths_[i]; }

 private:
  const ulong *lengths_;
};

bool fetch_stmt_field(my_h_statement stmt, uint32_t column_index,
                      StmtField *field) {
  my_h_field stmt_field = nullptr;
  if (stmt_resultset_metadata->fetch_field(stmt, column_index, &stmt_field) !=
      0) {
    return false;
  }

  mysql_field_type_t mysql_type = MYSQL_FIELD_TYPE_INVALID;
  const char *col_name = nullptr;
  const char *org_col_name = nullptr;
  const char *db_name = nullptr;
  const char *table_name = nullptr;
  const char *org_table_name = nullptr;
  uint flags = 0;
  ulong length = 0;
  uint decimals = 0;
  uint charsetnr = 0;
  bool is_unsigned = false;
  if (!fetch_field_info(stmt_field, "mysql_type", &mysql_type) ||
      !fetch_field_info(stmt_field, "col_name", &col_name) ||
      !fetch_field_info(stmt_field, "org_col_name", &org_col_name) ||
      !fetch_field_info(stmt_field, "db_name", &db_name) ||
      !fetch_field_info(stmt_field, "table_name", &table_name) ||
      !fetch_field_info(stmt_field, "org_table_name", &org_table_name) ||
      !fetch_field_info(stmt_field, "flags", &flags) ||
      !fetch_field_info(stmt_field, "length", &length) ||
      !fetch_field_info(stmt_field, "decimals", &decimals) ||
      !fetch_field_info(stmt_field, "charsetnr", &charsetnr) ||
      !fetch_field_info(stmt_field, "is_unsigned", &is_unsigned)) {
    return false;
  }

  auto &mysql_field = field->mysql_field;
  field->is_unsigned = is_unsigned;
  mysql_field.type = static_cast<enum_field_types>(mysql_type);
  mysql_field.name = const_cast<char *>(nonnull_cstr(col_name));
  mysql_field.org_name = const_cast<char *>(nonnull_cstr(org_col_name));
  mysql_field.table = const_cast<char *>(nonnull_cstr(table_name));
  mysql_field.org_table = const_cast<char *>(nonnull_cstr(org_table_name));
  mysql_field.db = const_cast<char *>(nonnull_cstr(db_name));
  mysql_field.catalog = k_catalog;
  mysql_field.def = nullptr;
  mysql_field.length = length;
  // Prepared-statement metadata leaves max_length unset until rows are stored.
  mysql_field.max_length = 0;
  mysql_field.name_length = cstr_length(col_name);
  mysql_field.org_name_length = cstr_length(org_col_name);
  mysql_field.table_length = cstr_length(table_name);
  mysql_field.org_table_length = cstr_length(org_table_name);
  mysql_field.db_length = cstr_length(db_name);
  mysql_field.catalog_length = 3;
  mysql_field.def_length = 0;
  mysql_field.flags = flags;
  mysql_field.decimals = static_cast<unsigned int>(decimals);
  mysql_field.charsetnr = static_cast<unsigned int>(charsetnr);
  mysql_field.extension = nullptr;

  return true;
}

bool fetch_stmt_fields(my_h_statement stmt, uint32_t num_fields,
                       std::vector<StmtField> *fields) {
  fields->resize(num_fields);
  for (uint32_t i = 0; i < num_fields; ++i) {
    if (!fetch_stmt_field(stmt, i, &(*fields)[i])) return false;
  }

  return true;
}

std::optional<std::string> fetch_stmt_column(my_h_row row,
                                             uint32_t column_index,
                                             const StmtField &field) {
  bool is_null = false;
  const auto type = field.mysql_field.type;
  if (type == MYSQL_TYPE_BIT) {
    mysql_cstring_with_length value{};
    if (stmt_get_string->get(row, column_index, &value, &is_null) != 0) {
      throw std::runtime_error("Failed to fetch bit column");
    }
    if (is_null) return std::nullopt;
    return std::string(value.str, value.length);
  }

  if (is_stmt_integer_type(type)) {
    if (field.is_unsigned) {
      uint64_t value = 0;
      if (stmt_get_unsigned_integer->get(row, column_index, &value, &is_null) !=
          0) {
        throw std::runtime_error("Failed to fetch unsigned integer column");
      }
      if (is_null) return std::nullopt;
      return std::to_string(value);
    }
    int64_t value = 0;
    if (stmt_get_integer->get(row, column_index, &value, &is_null) != 0) {
      throw std::runtime_error("Failed to fetch integer column");
    }
    if (is_null) return std::nullopt;
    return std::to_string(value);
  }

  if (is_stmt_temporal_type(type)) {
    mle_time value{};
    if (stmt_get_time->get(row, column_index, &value, &is_null) != 0) {
      throw std::runtime_error("Failed to fetch temporal column");
    }
    if (is_null) return std::nullopt;
    return format_stmt_time(value, field.mysql_field.decimals);
  }

  switch (type) {
    case MYSQL_TYPE_FLOAT:
      [[fallthrough]];
    case MYSQL_TYPE_DOUBLE: {
      double value = 0.0;
      if (stmt_get_double->get(row, column_index, &value, &is_null) != 0) {
        throw std::runtime_error("Failed to fetch floating point column");
      }
      if (is_null) return std::nullopt;
      return format_stmt_floating_point(value, type,
                                        field.mysql_field.decimals);
    }
    default: {
      mysql_cstring_with_length value{};
      if (stmt_get_string->get(row, column_index, &value, &is_null) != 0) {
        throw std::runtime_error("Failed to fetch string column");
      }
      if (is_null) return std::nullopt;
      return std::string(value.str, value.length);
    }
  }
}

std::unique_ptr<OwnedResultRow> make_stmt_row(
    my_h_row row, const std::vector<StmtField> &fields) {
  std::vector<std::optional<std::string>> values;
  values.reserve(fields.size());
  for (uint32_t i = 0; i < fields.size(); ++i) {
    values.emplace_back(fetch_stmt_column(row, i, fields[i]));
  }
  return std::make_unique<OwnedResultRow>(std::move(values));
}

bool has_statement_error(my_h_statement stmt) {
  uint64_t error_number = 0;
  return stmt_diagnostics->error_number(stmt, &error_number) == 0 &&
         error_number != 0;
}

bool has_command_error(MYSQL_H mysql_h) {
  unsigned int error_number = 0;
  return cmd_error_info->sql_errno(mysql_h, &error_number) != 0 ||
         error_number != 0;
}

class CommandResultGuard {
 public:
  explicit CommandResultGuard(MYSQL_RES_H result) : result_{result} {}
  ~CommandResultGuard() {
    if (result_ != nullptr) cmd_query_result->free_result(result_);
  }

  CommandResultGuard(const CommandResultGuard &) = delete;
  CommandResultGuard &operator=(const CommandResultGuard &) = delete;

 private:
  MYSQL_RES_H result_;
};

mysqlrouter::MySQLSession::Row make_command_row(MYSQL_ROW_H row,
                                                unsigned int num_fields) {
  mysqlrouter::MySQLSession::Row values;
  values.reserve(num_fields);
  for (unsigned int i = 0; i < num_fields; ++i) {
    values.emplace_back(row[i]);
  }
  return values;
}

}  // namespace

void configure_statement_handle(my_h_statement stmt) {
  const auto response_body_capacity =
      HttpServerComponent::get_instance()
          .get_effective_max_response_body_size();
  const auto buffer_capacity = std::max(
      kStmtMinBufferCapacity, static_cast<size_t>(response_body_capacity) + 1);

  mysql_cstring_with_length buffer_capacity_name{
      STRING_WITH_LEN("buffer_capacity")};
  mysql_cstring_with_length prefetch_rows_name{
      STRING_WITH_LEN("prefetch_rows")};
  if (stmt_attributes->set(stmt, buffer_capacity_name, &buffer_capacity) != 0 ||
      stmt_attributes->set(stmt, prefetch_rows_name, &k_stmt_prefetch_rows) !=
          0) {
    throw std::runtime_error("Failed to configure embedded statement handle");
  }
}

uint64_t mysql_bind_type_to_statement_type(enum_field_types type) {
  // MySQLSession callers pass libmysql MYSQL_BIND types. The component
  // statement service expects stored-program type bits.
  switch (type) {
    case MYSQL_TYPE_LONG:
      return MYSQL_SP_ARG_TYPE_LONG;
    case MYSQL_TYPE_DOUBLE:
      return MYSQL_SP_ARG_TYPE_DOUBLE;
    case MYSQL_TYPE_NULL:
      return MYSQL_SP_ARG_TYPE_NULL;
    case MYSQL_TYPE_TIMESTAMP:
      return MYSQL_SP_ARG_TYPE_TIMESTAMP;
    case MYSQL_TYPE_LONGLONG:
      return MYSQL_SP_ARG_TYPE_LONGLONG;
    case MYSQL_TYPE_VARCHAR:
      return MYSQL_SP_ARG_TYPE_VARCHAR;
    case MYSQL_TYPE_TINY_BLOB:
      return MYSQL_SP_ARG_TYPE_TINY_BLOB;
    case MYSQL_TYPE_BLOB:
      return MYSQL_SP_ARG_TYPE_BLOB;
    case MYSQL_TYPE_VAR_STRING:
      return MYSQL_SP_ARG_TYPE_VAR_STRING;
    case MYSQL_TYPE_STRING:
      return MYSQL_SP_ARG_TYPE_STRING;
    default:
      throw std::runtime_error("Unsupported prepared statement bind type");
  }
}

unsigned long mysql_bind_data_length(const MYSQL_BIND &bind) {
  if (bind.is_null != nullptr && *bind.is_null) return 0;
  if (bind.length != nullptr && *bind.length != 0) return *bind.length;

  auto fixed_length = fixed_bind_data_length(bind.buffer_type);
  if (fixed_length.has_value() && bind.buffer != nullptr) {
    return *fixed_length;
  }

  return bind.length != nullptr ? *bind.length : bind.buffer_length;
}

std::unique_ptr<mysqlrouter::MySQLSession::ResultRow> clone_result_row(
    const mysqlrouter::MySQLSession::ResultRow &row) {
  std::vector<std::optional<std::string>> values;
  values.reserve(row.size());
  for (size_t i = 0; i < row.size(); ++i) {
    if (row[i] == nullptr) {
      values.emplace_back(std::nullopt);
      continue;
    }
    values.emplace_back(std::string(row[i], row.get_data_size(i)));
  }

  return std::make_unique<OwnedResultRow>(std::move(values));
}

StatementResultStatus process_statement_result(
    my_h_statement stmt,
    const mysqlrouter::MySQLSession::ResultRowProcessor &processor,
    const mysqlrouter::MySQLSession::FieldValidator &validator) {
  uint32_t num_fields = 0;
  if (stmt_resultset_metadata->field_count(stmt, &num_fields) != 0) {
    if (!has_statement_error(stmt)) {
      validator(0, nullptr);
      return StatementResultStatus::kNoResult;
    }
    return StatementResultStatus::kError;
  }

  if (num_fields == 0) {
    validator(0, nullptr);
    return StatementResultStatus::kResult;
  }

  std::vector<StmtField> fields;
  if (!fetch_stmt_fields(stmt, num_fields, &fields)) {
    return StatementResultStatus::kError;
  }

  std::vector<MYSQL_FIELD> mysql_fields;
  mysql_fields.reserve(fields.size());
  for (const auto &field : fields) mysql_fields.push_back(field.mysql_field);
  validator(num_fields, mysql_fields.data());

  my_h_row row = nullptr;
  while (true) {
    if (stmt_result->fetch(stmt, &row) != 0) {
      return StatementResultStatus::kError;
    }
    if (row == nullptr) break;

    auto owned_row = make_stmt_row(row, fields);
    if (!processor(*owned_row)) break;
  }

  return StatementResultStatus::kResult;
}

CommandResultStatus process_command_result(
    MYSQL_H mysql_h,
    const mysqlrouter::MySQLSession::ResultRowProcessor &processor,
    const mysqlrouter::MySQLSession::FieldValidator &validator) {
  MYSQL_RES_H result = reinterpret_cast<MYSQL_RES_H>(1);
  if (cmd_query_result->store_result(mysql_h, &result) != 0) {
    return result == nullptr && !has_command_error(mysql_h)
               ? CommandResultStatus::kNoResult
               : CommandResultStatus::kError;
  }

  CommandResultGuard result_guard{result};

  unsigned int num_fields = 0;
  if (cmd_field_info->num_fields(result, &num_fields) != 0) {
    return CommandResultStatus::kError;
  }

  MYSQL_FIELD_H *field_handles = nullptr;
  if (cmd_field_info->fetch_fields(result, &field_handles) != 0) {
    return CommandResultStatus::kError;
  }
  auto *fields = reinterpret_cast<MYSQL_FIELD *>(field_handles);
  validator(num_fields, fields);

  MYSQL_ROW_H row = nullptr;
  while (true) {
    if (cmd_query_result->fetch_row(result, &row) != 0) {
      return CommandResultStatus::kError;
    }
    if (row == nullptr) break;

    ulong *lengths = nullptr;
    if (cmd_query_result->fetch_lengths(result, &lengths) != 0 ||
        lengths == nullptr) {
      return CommandResultStatus::kError;
    }

    CommandResultRow command_row{make_command_row(row, num_fields), lengths};
    if (!processor(command_row)) break;
  }

  return CommandResultStatus::kResult;
}

}  // namespace mrs_component
