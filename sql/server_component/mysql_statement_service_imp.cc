/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "mysql_statement_service_imp.h"
#include <mysql/components/service_implementation.h>
#include <sql/current_thd.h>
#include <sql/statement/statement.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <variant>
#include "field_types.h"
#include "lex_string.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/com_data.h"
#include "mysql/components/services/bits/mle_time_bits.h"
#include "mysql/components/services/bits/stored_program_bits.h"  // stored_program_argument_type
#include "mysql/components/services/defs/mysql_string_defs.h"
#include "mysql/components/services/mysql_statement_service.h"
#include "mysql_time.h"
#include "sql/item.h"

REQUIRES_SERVICE_PLACEHOLDER(mysql_current_thread_reader);

constexpr auto MYSQL_SUCCESS = 0;
constexpr auto MYSQL_FAILURE = 1;

struct Service_statement {
  size_t capacity = 500;
  size_t num_rows_per_fetch = 1;
  bool use_thd_protocol = false;
  std::string charset_name = "utf8mb4";
  Statement_handle *stmt{nullptr};

  ~Service_statement() { delete stmt; }
};

DEFINE_BOOL_METHOD(mysql_stmt_factory_imp::init,
                   (my_h_statement * stmt_handle)) {
  try {
    auto *statement = new Service_statement();
    *stmt_handle = reinterpret_cast<my_h_statement>(statement);
  } catch (...) {
    return MYSQL_FAILURE;
  }
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_factory_imp::close,
                   (my_h_statement stmt_handle)) {
  auto *statement = reinterpret_cast<Service_statement *>(stmt_handle);
  if (statement == nullptr) return MYSQL_FAILURE;

  delete statement;
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_metadata_imp::param_count,
                   (my_h_statement stmt_handle, uint32_t *parameter_count)) {
  auto *statement = reinterpret_cast<Service_statement *>(stmt_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;
  if (!statement->is_prepared_statement()) return MYSQL_FAILURE;

  auto prepared_statement = static_cast<Prepared_statement_handle *>(statement);
  *parameter_count = prepared_statement->get_param_count();
  return MYSQL_SUCCESS;
}

auto enum_field_type_to_int(enum_field_types field_type) -> uint64_t {
  switch (field_type) {
    case MYSQL_TYPE_DECIMAL:
      return MYSQL_SP_ARG_TYPE_DECIMAL;
    case MYSQL_TYPE_TINY:
      return MYSQL_SP_ARG_TYPE_TINY;
    case MYSQL_TYPE_SHORT:
      return MYSQL_SP_ARG_TYPE_SHORT;
    case MYSQL_TYPE_LONG:
      return MYSQL_SP_ARG_TYPE_LONG;
    case MYSQL_TYPE_FLOAT:
      return MYSQL_SP_ARG_TYPE_FLOAT;
    case MYSQL_TYPE_DOUBLE:
      return MYSQL_SP_ARG_TYPE_DOUBLE;
    case MYSQL_TYPE_NULL:
      return MYSQL_SP_ARG_TYPE_NULL;
    case MYSQL_TYPE_TIMESTAMP:
      return MYSQL_SP_ARG_TYPE_TIMESTAMP;
    case MYSQL_TYPE_LONGLONG:
      return MYSQL_SP_ARG_TYPE_LONGLONG;
    case MYSQL_TYPE_INT24:
      return MYSQL_SP_ARG_TYPE_INT24;
    case MYSQL_TYPE_DATE:
      return MYSQL_SP_ARG_TYPE_DATE;
    case MYSQL_TYPE_TIME:
      return MYSQL_SP_ARG_TYPE_TIME;
    case MYSQL_TYPE_DATETIME:
      return MYSQL_SP_ARG_TYPE_DATETIME;
    case MYSQL_TYPE_YEAR:
      return MYSQL_SP_ARG_TYPE_YEAR;
    case MYSQL_TYPE_NEWDATE:
      return MYSQL_SP_ARG_TYPE_NEWDATE;
    case MYSQL_TYPE_VARCHAR:
      return MYSQL_SP_ARG_TYPE_VARCHAR;
    case MYSQL_TYPE_BIT:
      return MYSQL_SP_ARG_TYPE_BIT;
    case MYSQL_TYPE_TIMESTAMP2:
      return MYSQL_SP_ARG_TYPE_TIMESTAMP2;
    case MYSQL_TYPE_DATETIME2:
      return MYSQL_SP_ARG_TYPE_DATETIME2;
    case MYSQL_TYPE_TIME2:
      return MYSQL_SP_ARG_TYPE_TIME2;
    case MYSQL_TYPE_TYPED_ARRAY:
      return MYSQL_SP_ARG_TYPE_TYPED_ARRAY;
    case MYSQL_TYPE_INVALID:
      return MYSQL_SP_ARG_TYPE_INVALID;
    case MYSQL_TYPE_BOOL:
      return MYSQL_SP_ARG_TYPE_BOOL;
    case MYSQL_TYPE_JSON:
      return MYSQL_SP_ARG_TYPE_JSON;
    case MYSQL_TYPE_NEWDECIMAL:
      return MYSQL_SP_ARG_TYPE_NEWDECIMAL;
    case MYSQL_TYPE_ENUM:
      return MYSQL_SP_ARG_TYPE_ENUM;
    case MYSQL_TYPE_SET:
      return MYSQL_SP_ARG_TYPE_SET;
    case MYSQL_TYPE_TINY_BLOB:
      return MYSQL_SP_ARG_TYPE_TINY_BLOB;
    case MYSQL_TYPE_MEDIUM_BLOB:
      return MYSQL_SP_ARG_TYPE_MEDIUM_BLOB;
    case MYSQL_TYPE_LONG_BLOB:
      return MYSQL_SP_ARG_TYPE_LONG_BLOB;
    case MYSQL_TYPE_BLOB:
      return MYSQL_SP_ARG_TYPE_BLOB;
    case MYSQL_TYPE_VAR_STRING:
      return MYSQL_SP_ARG_TYPE_VAR_STRING;
    case MYSQL_TYPE_STRING:
      return MYSQL_SP_ARG_TYPE_STRING;
    case MYSQL_TYPE_GEOMETRY:
      return MYSQL_SP_ARG_TYPE_GEOMETRY;
    default:
      return MYSQL_SP_ARG_TYPE_INVALID;
  }
}

DEFINE_BOOL_METHOD(mysql_stmt_metadata_imp::param_metadata,
                   (my_h_statement stmt_handle, uint32_t index,
                    const char *metadata, void *data)) {
  auto *statement = reinterpret_cast<Service_statement *>(stmt_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;
  if (!statement->is_prepared_statement()) return MYSQL_FAILURE;

  auto *prepared_statement =
      static_cast<Prepared_statement_handle *>(statement);
  Item_param *param = prepared_statement->get_parameter(index);
  if (param == nullptr) return MYSQL_FAILURE;

  if (strcmp(metadata, "null_bit") == 0) {
    *static_cast<bool *>(data) = param->null_value;
    return MYSQL_SUCCESS;
  } else if (strcmp(metadata, "type") == 0) {
    *static_cast<uint64_t *>(data) = enum_field_type_to_int(param->data_type());
    return MYSQL_SUCCESS;
  } else if (strcmp(metadata, "is_unsigned") == 0) {
    *static_cast<bool *>(data) = param->unsigned_flag;
    return MYSQL_SUCCESS;
  }

  return MYSQL_FAILURE;
}

MYSQL_TIME convert_to_mysql_time(const mle_time &value) {
  auto result = MYSQL_TIME{};
  result.year = value.year;
  result.month = value.month;
  result.day = value.day;
  result.hour = value.hour;
  result.minute = value.minute;
  result.second = value.second;
  result.second_part = value.second_part;
  result.time_zone_displacement = value.time_zone_displacement;
  switch (value.time_type) {
    case MYSQL_TIMESTAMP_TYPE_DATE:
      result.time_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_DATE;
      break;
    case MYSQL_TIMESTAMP_TYPE_TIME:
      result.time_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_TIME;
      break;
    case MYSQL_TIMESTAMP_TYPE_DATETIME:
      result.time_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_DATETIME;
      break;
    case MYSQL_TIMESTAMP_TYPE_DATETIME_TZ:
      result.time_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_DATETIME_TZ;
      break;
    default:
      result.time_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_NONE;
  }
  return result;
}

auto int_to_enum_field_type(uint64_t type) -> std::optional<enum_field_types> {
  switch (type) {
    case MYSQL_SP_ARG_TYPE_DECIMAL:
      return enum_field_types::MYSQL_TYPE_DECIMAL;
    case MYSQL_SP_ARG_TYPE_TINY:
      return enum_field_types::MYSQL_TYPE_TINY;
    case MYSQL_SP_ARG_TYPE_SHORT:
      return enum_field_types::MYSQL_TYPE_SHORT;
    case MYSQL_SP_ARG_TYPE_LONG:
      return enum_field_types::MYSQL_TYPE_LONG;
    case MYSQL_SP_ARG_TYPE_FLOAT:
      return enum_field_types::MYSQL_TYPE_FLOAT;
    case MYSQL_SP_ARG_TYPE_DOUBLE:
      return enum_field_types::MYSQL_TYPE_DOUBLE;
    case MYSQL_SP_ARG_TYPE_NULL:
      return enum_field_types::MYSQL_TYPE_NULL;
    case MYSQL_SP_ARG_TYPE_TIMESTAMP:
      return enum_field_types::MYSQL_TYPE_TIMESTAMP;
    case MYSQL_SP_ARG_TYPE_LONGLONG:
      return enum_field_types::MYSQL_TYPE_LONGLONG;
    case MYSQL_SP_ARG_TYPE_INT24:
      return enum_field_types::MYSQL_TYPE_INT24;
    case MYSQL_SP_ARG_TYPE_DATE:
      return enum_field_types::MYSQL_TYPE_DATE;
    case MYSQL_SP_ARG_TYPE_TIME:
      return enum_field_types::MYSQL_TYPE_TIME;
    case MYSQL_SP_ARG_TYPE_DATETIME:
      return enum_field_types::MYSQL_TYPE_DATETIME;
    case MYSQL_SP_ARG_TYPE_YEAR:
      return enum_field_types::MYSQL_TYPE_YEAR;
    case MYSQL_SP_ARG_TYPE_NEWDATE:
      return enum_field_types::MYSQL_TYPE_NEWDATE;
    case MYSQL_SP_ARG_TYPE_VARCHAR:
      return enum_field_types::MYSQL_TYPE_VARCHAR;
    case MYSQL_SP_ARG_TYPE_BIT:
      return enum_field_types::MYSQL_TYPE_BIT;
    case MYSQL_SP_ARG_TYPE_TIMESTAMP2:
      return enum_field_types::MYSQL_TYPE_TIMESTAMP2;
    case MYSQL_SP_ARG_TYPE_DATETIME2:
      return enum_field_types::MYSQL_TYPE_DATETIME2;
    case MYSQL_SP_ARG_TYPE_TIME2:
      return enum_field_types::MYSQL_TYPE_TIME2;
    case MYSQL_SP_ARG_TYPE_TYPED_ARRAY:
      return enum_field_types::MYSQL_TYPE_TYPED_ARRAY;
    case MYSQL_SP_ARG_TYPE_INVALID:
      return enum_field_types::MYSQL_TYPE_INVALID;
    case MYSQL_SP_ARG_TYPE_BOOL:
      return enum_field_types::MYSQL_TYPE_BOOL;
    case MYSQL_SP_ARG_TYPE_JSON:
      return enum_field_types::MYSQL_TYPE_JSON;
    case MYSQL_SP_ARG_TYPE_NEWDECIMAL:
      return enum_field_types::MYSQL_TYPE_NEWDECIMAL;
    case MYSQL_SP_ARG_TYPE_ENUM:
      return enum_field_types::MYSQL_TYPE_ENUM;
    case MYSQL_SP_ARG_TYPE_SET:
      return enum_field_types::MYSQL_TYPE_SET;
    case MYSQL_SP_ARG_TYPE_TINY_BLOB:
      return enum_field_types::MYSQL_TYPE_TINY_BLOB;
    case MYSQL_SP_ARG_TYPE_MEDIUM_BLOB:
      return enum_field_types::MYSQL_TYPE_MEDIUM_BLOB;
    case MYSQL_SP_ARG_TYPE_LONG_BLOB:
      return enum_field_types::MYSQL_TYPE_LONG_BLOB;
    case MYSQL_SP_ARG_TYPE_BLOB:
      return enum_field_types::MYSQL_TYPE_BLOB;
    case MYSQL_SP_ARG_TYPE_VAR_STRING:
      return enum_field_types::MYSQL_TYPE_VAR_STRING;
    case MYSQL_SP_ARG_TYPE_STRING:
      return enum_field_types::MYSQL_TYPE_STRING;
    case MYSQL_SP_ARG_TYPE_GEOMETRY:
      return enum_field_types::MYSQL_TYPE_GEOMETRY;
    default:
      return {};
  }
}

auto is_temporal_type(uint64_t type) -> bool {
  switch (type) {
    case MYSQL_SP_ARG_TYPE_TIMESTAMP:
      return true;
    case MYSQL_SP_ARG_TYPE_DATE:
      return true;
    case MYSQL_SP_ARG_TYPE_TIME:
      return true;
    case MYSQL_SP_ARG_TYPE_DATETIME:
      return true;
    case MYSQL_SP_ARG_TYPE_NEWDATE:
      return true;
    case MYSQL_SP_ARG_TYPE_TIMESTAMP2:
      return true;
    case MYSQL_SP_ARG_TYPE_DATETIME2:
      return true;
    case MYSQL_SP_ARG_TYPE_TIME2:
      return true;
    default:
      return false;
  }
}

DEFINE_BOOL_METHOD(mysql_stmt_bind_imp::bind_param,
                   (my_h_statement stmt_handle, uint32_t index, bool is_null,
                    uint64_t type, bool is_unsigned, const void *data,
                    unsigned long data_length, const char *name,
                    unsigned long name_length)) {
  auto *statement = reinterpret_cast<Service_statement *>(stmt_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;
  if (!statement->is_prepared_statement()) return MYSQL_FAILURE;

  auto *prepared_statement =
      static_cast<Prepared_statement_handle *>(statement);
  bool is_temporal = is_temporal_type(type);
  auto field_type = int_to_enum_field_type(type);
  if (!field_type) return MYSQL_FAILURE;

  if (is_temporal && !is_null) {
    auto temporal_data = MYSQL_TIME{};
    temporal_data = convert_to_mysql_time(*static_cast<const mle_time *>(data));
    return prepared_statement->set_parameter(
        index, is_null, *field_type, is_unsigned, &temporal_data,
        sizeof(temporal_data), name, name_length);
  }

  if (prepared_statement->set_parameter(index, is_null, *field_type,
                                        is_unsigned, data, data_length, name,
                                        name_length))
    return MYSQL_FAILURE;

  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_attributes_imp::get,
                   (my_h_statement stmt_handle, mysql_cstring_with_length name,
                    void *value)) {
  auto *service_stmt = reinterpret_cast<Service_statement *>(stmt_handle);
  auto *statement = service_stmt->stmt;
  if (statement == nullptr) {
    if (strncmp(name.str, "buffer_capacity", name.length) == 0) {
      *reinterpret_cast<size_t *>(value) = service_stmt->capacity;
    } else if (strncmp(name.str, "prefetch_rows", name.length) == 0) {
      *reinterpret_cast<size_t *>(value) = service_stmt->num_rows_per_fetch;
    } else if (strncmp(name.str, "use_thd_protocol", name.length) == 0) {
      *reinterpret_cast<bool *>(value) = service_stmt->use_thd_protocol;
    } else if (strncmp(name.str, "charset_name", name.length) == 0) {
      *reinterpret_cast<const char **>(value) =
          service_stmt->charset_name.data();
      return MYSQL_SUCCESS;
    } else {
      assert(false);
      return MYSQL_FAILURE;
    }
    return MYSQL_SUCCESS;

  } else {
    if (strncmp(name.str, "buffer_capacity", name.length) == 0) {
      *reinterpret_cast<size_t *>(value) = statement->get_capacity();
    } else if (strncmp(name.str, "prefetch_rows", name.length) == 0) {
      *reinterpret_cast<size_t *>(value) = statement->get_num_rows_per_fetch();
    } else if (strncmp(name.str, "use_thd_protocol", name.length) == 0) {
      *reinterpret_cast<bool *>(value) = statement->is_using_thd_protocol();
    } else if (strncmp(name.str, "charset_name", name.length) == 0) {
      auto expected_charset = statement->get_expected_charset();
      if (expected_charset == nullptr) return MYSQL_FAILURE;
      *reinterpret_cast<const char **>(value) = expected_charset;
      return MYSQL_SUCCESS;
    } else {
      assert(false);
      return MYSQL_FAILURE;
    }
    return MYSQL_SUCCESS;
  }
}

DEFINE_BOOL_METHOD(mysql_stmt_attributes_imp::set,
                   (my_h_statement stmt_handle, mysql_cstring_with_length name,
                    const void *value)) {
  auto *service_stmt = reinterpret_cast<Service_statement *>(stmt_handle);
  auto *statement = service_stmt->stmt;

  // Statement is not null if only it has been executed or prepared before. Set
  // attribute is not allowed when statement has been executed or prepared
  if (statement != nullptr) return MYSQL_FAILURE;

  if (strncmp(name.str, "buffer_capacity", name.length) == 0) {
    service_stmt->capacity = *static_cast<const size_t *>(value);
  } else if (strncmp(name.str, "prefetch_rows", name.length) == 0) {
    service_stmt->num_rows_per_fetch = *static_cast<const size_t *>(value);
  } else if (strncmp(name.str, "use_thd_protocol", name.length) == 0) {
    service_stmt->use_thd_protocol = *static_cast<const bool *>(value);
  } else if (strncmp(name.str, "charset_name", name.length) == 0) {
    auto parsed_value = *static_cast<const mysql_cstring_with_length *>(value);
    service_stmt->charset_name =
        std::string{parsed_value.str, parsed_value.length};
  } else {
    assert(false);
    return MYSQL_FAILURE;
  }
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_execute_imp::prepare,
                   (mysql_cstring_with_length query,
                    my_h_statement stmt_handle)) {
  auto service_stmt = reinterpret_cast<Service_statement *>(stmt_handle);
  Prepared_statement_handle *statement = nullptr;
  if (service_stmt->stmt != nullptr) {
    auto stmt = static_cast<Statement_handle *>(service_stmt->stmt);
    if (!stmt->is_prepared_statement()) return MYSQL_FAILURE;

    // Prepare has already been called
    delete service_stmt->stmt;
    service_stmt->stmt = nullptr;
  }
  try {
    statement =
        new Prepared_statement_handle(current_thd, query.str, query.length);
  } catch (...) {
    return MYSQL_FAILURE;
  }

  if (statement == nullptr) return MYSQL_FAILURE;
  service_stmt->stmt = statement;

  statement->set_capacity(service_stmt->capacity);
  statement->set_num_rows_per_fetch(service_stmt->num_rows_per_fetch);
  statement->set_use_thd_protocol(service_stmt->use_thd_protocol);
  statement->set_expected_charset(service_stmt->charset_name.data());
  if (statement->prepare()) return MYSQL_FAILURE;
  return MYSQL_SUCCESS;
}

bool execute_prepared_statement(Prepared_statement_handle *statement) {
  if (statement->execute()) return MYSQL_FAILURE;

  if (statement->is_cursor_open()) {
    // For prepared statement, we call fetch to get the initial row(s) for
    // SELECT statements
    if (statement->fetch()) return MYSQL_FAILURE;
  }
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_execute_imp::execute,
                   (my_h_statement stmt_handle)) {
  auto statement = reinterpret_cast<Service_statement *>(stmt_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;
  if (!statement->is_prepared_statement()) return MYSQL_FAILURE;

  auto prepared_statement = static_cast<Prepared_statement_handle *>(statement);
  return execute_prepared_statement(prepared_statement);
}

DEFINE_BOOL_METHOD(mysql_stmt_execute_imp::reset,
                   (my_h_statement stmt_handle)) {
  auto service_stmt = reinterpret_cast<Service_statement *>(stmt_handle);
  auto statement = service_stmt->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;

  // Note: not support calling reset for regular statements
  if (!statement->is_prepared_statement()) return MYSQL_FAILURE;

  auto prepared_statement = static_cast<Prepared_statement_handle *>(statement);
  if (prepared_statement->reset()) return MYSQL_FAILURE;
  return MYSQL_SUCCESS;
}

bool execute_regular_statement(Regular_statement_handle *statement) {
  if (statement->execute()) return MYSQL_FAILURE;
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_execute_direct_imp::execute,
                   (mysql_cstring_with_length query,
                    my_h_statement stmt_handle)) {
  auto service_stmt = reinterpret_cast<Service_statement *>(stmt_handle);
  if (service_stmt->stmt != nullptr) {
    auto stmt = static_cast<Statement_handle *>(service_stmt->stmt);
    if (stmt->is_prepared_statement()) return MYSQL_FAILURE;

    // Execute_direct has already been called
    delete service_stmt->stmt;
    service_stmt->stmt = nullptr;
  }
  Regular_statement_handle *statement = nullptr;
  try {
    statement =
        new Regular_statement_handle(current_thd, query.str, query.length);
  } catch (...) {
    return MYSQL_FAILURE;
  }
  if (statement == nullptr) return MYSQL_FAILURE;
  service_stmt->stmt = statement;

  statement->set_capacity(service_stmt->capacity);
  statement->set_num_rows_per_fetch(service_stmt->num_rows_per_fetch);
  statement->set_use_thd_protocol(service_stmt->use_thd_protocol);
  statement->set_expected_charset(service_stmt->charset_name.data());
  return execute_regular_statement(statement);
}

DEFINE_BOOL_METHOD(mysql_stmt_result_imp::next_result,
                   (my_h_statement stmt_handle, bool *has_next)) {
  auto *statement = reinterpret_cast<Service_statement *>(stmt_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;
  auto result_set = statement->get_current_result_set();
  if (result_set == nullptr) {
    *has_next = false;
    return MYSQL_FAILURE;
  }
  *has_next = result_set->has_next();
  if (*has_next) statement->next_result_set();
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_result_imp::fetch,
                   (my_h_statement stmt_handle, my_h_row *row)) {
  auto *statement = reinterpret_cast<Service_statement *>(stmt_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;
  auto *result = statement->get_current_result_set();

  if (result == nullptr || result->get_rows() == nullptr) return MYSQL_FAILURE;

  if (result->is_last_row()) {
    *row = nullptr;

    if (!statement->is_prepared_statement()) return MYSQL_SUCCESS;

    // If this is a prepared statement, we may need to fetch to get more rows
    auto prepared_statement =
        dynamic_cast<Prepared_statement_handle *>(statement);
    // If cursor is closed, then all rows are already fetched.
    if (!prepared_statement->is_cursor_open()) return MYSQL_SUCCESS;

    if (prepared_statement->fetch()) return MYSQL_FAILURE;
  }

  // If fetch cannot get more row, get_next_row() returns nullptr.
  auto r = result->get_next_row();
  *row = reinterpret_cast<my_h_row>(r);
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_resultset_metadata_imp::fetch_field,
                   (my_h_statement stmt_handle, uint32_t column_index,
                    my_h_field *field)) {
  auto *statement = reinterpret_cast<Service_statement *>(stmt_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;
  auto *result = statement->get_current_result_set();
  if (result == nullptr) return MYSQL_FAILURE;
  auto *fields = result->get_fields();
  if (fields == nullptr) return MYSQL_FAILURE;
  *field = reinterpret_cast<my_h_field>(fields->get_column(column_index));
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_resultset_metadata_imp::field_count,
                   (my_h_statement stmt_handle, uint32_t *num_fields)) {
  auto *statement = reinterpret_cast<Service_statement *>(stmt_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;
  auto *result = statement->get_current_result_set();
  if (result == nullptr) {
    *num_fields = 0;
    return MYSQL_FAILURE;
  }
  *num_fields = result->get_field_count();
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_resultset_metadata_imp::field_info,
                   (my_h_field field, const char *name, void *value)) {
  auto *column = reinterpret_cast<Column_metadata *>(field);
  if (column == nullptr) return MYSQL_FAILURE;

  if (strcmp(name, "org_col_name") == 0) {
    *reinterpret_cast<char const **>(value) = column->original_col_name;
  } else if (strcmp(name, "db_name") == 0) {
    *reinterpret_cast<char const **>(value) = column->database_name;
  } else if (strcmp(name, "table_name") == 0) {
    *reinterpret_cast<char const **>(value) = column->table_name;
  } else if (strcmp(name, "org_table_name") == 0) {
    *reinterpret_cast<char const **>(value) = column->original_table_name;
  } else if (strcmp(name, "charsetnr") == 0) {
    *reinterpret_cast<uint *>(value) = column->charsetnr;
  } else if (strcmp(name, "charset_name") == 0) {
    *reinterpret_cast<char const **>(value) =
        get_charset(column->charsetnr, MYF(0))->csname;
  } else if (strcmp(name, "collation_name") == 0) {
    *reinterpret_cast<char const **>(value) =
        get_collation_name(column->charsetnr);
  } else if (strcmp(name, "flags") == 0) {
    *reinterpret_cast<uint *>(value) = column->flags;
  } else if (strcmp(name, "decimals") == 0) {
    *reinterpret_cast<uint *>(value) = column->decimals;
  } else if (strcmp(name, "is_unsigned") == 0) {
    *reinterpret_cast<bool *>(value) = column->flags & UNSIGNED_FLAG;
  } else if (strcmp(name, "is_zerofill") == 0) {
    *reinterpret_cast<bool *>(value) = column->flags & ZEROFILL_FLAG;
  } else if (strcmp(name, "col_name") == 0) {
    *reinterpret_cast<char const **>(value) = column->column_name;
  } else if (strcmp(name, "type") == 0) {
    auto enum_type = enum_field_type_to_int(column->type);
    if (enum_type == MYSQL_SP_ARG_TYPE_INVALID) return MYSQL_FAILURE;
    *reinterpret_cast<uint64_t *>(value) = enum_type;
  } else {
    assert(false);
    return MYSQL_FAILURE;
  }
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_diagnostics_imp::error_id,
                   (my_h_statement resource_handle, uint64_t *error_id)) {
  auto *statement =
      reinterpret_cast<Service_statement *>(resource_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;
  if (!statement->is_error()) return MYSQL_FAILURE;

  *error_id = statement->get_last_errno();
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_diagnostics_imp::error,
                   (my_h_statement resource_handle,
                    mysql_cstring_with_length *error_message)) {
  auto *statement =
      reinterpret_cast<Service_statement *>(resource_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;
  if (!statement->is_error()) return MYSQL_FAILURE;

  *error_message = {statement->get_last_error(),
                    strlen(statement->get_last_error())};
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_diagnostics_imp::sqlstate,
                   (my_h_statement resource_handle,
                    mysql_cstring_with_length *sqlstate_error_message)) {
  auto *statement =
      reinterpret_cast<Service_statement *>(resource_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;
  if (!statement->is_error()) return MYSQL_FAILURE;

  *sqlstate_error_message = {statement->get_mysql_state(),
                             strlen(statement->get_mysql_state())};
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_diagnostics_imp::num_warnings,
                   (my_h_statement resource_handle, uint32_t *count)) {
  auto *statement =
      reinterpret_cast<Service_statement *>(resource_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;

  *count = statement->warning_count();
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_diagnostics_imp::affected_rows,
                   (my_h_statement stmt_handle, uint64_t *num_rows)) {
  auto *statement = reinterpret_cast<Service_statement *>(stmt_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;

  auto *result_set = statement->get_current_result_set();
  if (result_set == nullptr) return MYSQL_FAILURE;

  *num_rows = result_set->get_num_affected_rows();
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_diagnostics_imp::insert_id,
                   (my_h_statement stmt_handle, uint64_t *retval)) {
  auto *statement = reinterpret_cast<Service_statement *>(stmt_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;

  auto *result_set = statement->get_current_result_set();
  if (result_set == nullptr) return MYSQL_FAILURE;

  *retval = result_set->get_last_insert_id();
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_diagnostics_imp::get_warning,
                   (my_h_statement resource_handle, uint32_t warning_index,
                    my_h_warning *warning)) {
  auto *statement =
      reinterpret_cast<Service_statement *>(resource_handle)->stmt;
  if (statement == nullptr) return MYSQL_FAILURE;

  assert(warning_index < statement->warning_count());
  if (warning_index >= statement->warning_count()) {
    return MYSQL_FAILURE;
  }
  Warning *warnings = statement->get_warnings();
  if (warnings == nullptr) return MYSQL_FAILURE;
  *warning = reinterpret_cast<my_h_warning>(warnings + warning_index);
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_diagnostics_imp::warning_level,
                   (my_h_warning warning, uint32_t *level)) {
  auto *warn = reinterpret_cast<Warning *>(warning);
  if (warn == nullptr) return MYSQL_FAILURE;

  *level = warn->m_level;
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_diagnostics_imp::warning_code,
                   (my_h_warning warning, uint32_t *code)) {
  auto *warn = reinterpret_cast<Warning *>(warning);
  if (warn == nullptr) return MYSQL_FAILURE;

  *code = warn->m_code;
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_diagnostics_imp::warning_message,
                   (my_h_warning warning,
                    mysql_cstring_with_length *error_message)) {
  auto *warn = reinterpret_cast<Warning *>(warning);
  if (warn == nullptr) return MYSQL_FAILURE;

  *error_message = {warn->m_message, strlen(warn->m_message)};
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_get_integer_imp::get,
                   (my_h_row row_handle, uint32_t column_index, int64_t *data,
                    bool *is_null)) {
  auto *row = reinterpret_cast<Row<value_t> *>(row_handle);
  if (row == nullptr) return MYSQL_FAILURE;
  const auto *column = row->get_column(column_index);
  if (column == nullptr) return MYSQL_FAILURE;

  auto *value = std::get_if<int64_t *>(column);
  if (value != nullptr) {
    *data = **value;
    *is_null = false;
  } else {
    *is_null = true;
  }
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_get_unsigned_integer_imp::get,
                   (my_h_row row_handle, uint32_t column_index, uint64_t *data,
                    bool *is_null)) {
  auto *row = reinterpret_cast<Row<value_t> *>(row_handle);
  if (row == nullptr) return MYSQL_FAILURE;
  const auto *column = row->get_column(column_index);
  if (column == nullptr) return MYSQL_FAILURE;

  auto *value = std::get_if<uint64_t *>(column);
  if (value != nullptr) {
    *data = **value;
    *is_null = false;
  } else {
    *is_null = true;
  }
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_get_double_imp::get,
                   (my_h_row row_handle, uint32_t column_index, double *data,
                    bool *is_null)) {
  auto *row = reinterpret_cast<Row<value_t> *>(row_handle);
  if (row == nullptr) return MYSQL_FAILURE;
  const auto *column = row->get_column(column_index);
  if (column == nullptr) return MYSQL_FAILURE;

  auto *value = std::get_if<double *>(column);
  if (value != nullptr) {
    *data = **value;
    *is_null = false;
  } else {
    *is_null = true;
  }
  return MYSQL_SUCCESS;
}

auto convert_to_mle_time(const MYSQL_TIME &value) -> mle_time {
  auto result = mle_time{};
  result.year = value.year;
  result.month = value.month;
  result.day = value.day;
  result.hour = value.hour;
  result.minute = value.minute;
  result.second = value.second;
  result.second_part = value.second_part;
  result.time_zone_displacement = value.time_zone_displacement;
  switch (value.time_type) {
    case MYSQL_TIMESTAMP_TYPE_DATE:
      result.time_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_DATE;
      break;
    case MYSQL_TIMESTAMP_TYPE_TIME:
      result.time_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_TIME;
      break;
    case MYSQL_TIMESTAMP_TYPE_DATETIME:
      result.time_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_DATETIME;
      break;
    case MYSQL_TIMESTAMP_TYPE_DATETIME_TZ:
      result.time_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_DATETIME_TZ;
      break;
    default:
      result.time_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_NONE;
  }
  return result;
}

DEFINE_BOOL_METHOD(mysql_stmt_get_time_imp::get,
                   (my_h_row row_handle, uint32_t column_index, mle_time *time,
                    bool *is_null)) {
  auto *row = reinterpret_cast<Row<value_t> *>(row_handle);
  if (row == nullptr) return MYSQL_FAILURE;
  const auto *column = row->get_column(column_index);
  if (column == nullptr) return MYSQL_FAILURE;

  auto *value = std::get_if<MYSQL_TIME *>(column);
  if (value != nullptr) {
    *time = convert_to_mle_time(**value);
    *is_null = false;
  } else {
    *is_null = true;
  }
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stmt_get_string_imp::get,
                   (my_h_row row_handle, uint32_t column_index,
                    mysql_cstring_with_length *data, bool *is_null)) {
  auto *row = reinterpret_cast<Row<value_t> *>(row_handle);
  if (row == nullptr) return MYSQL_FAILURE;
  auto *column = row->get_column(column_index);
  if (column == nullptr) return MYSQL_FAILURE;

  auto *value = std::get_if<char *>(column);
  if (value != nullptr) {
    *data = {*value, strlen(*value)};
    *is_null = false;
  } else {
    *is_null = true;
  }
  return MYSQL_SUCCESS;
}
