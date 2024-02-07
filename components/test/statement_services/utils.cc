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

#include "utils.h"
#include <optional>
#include <sstream>  // for ostringstream

extern REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_factory);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_execute);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_execute_direct);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_metadata);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_bind);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_get_string);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_get_time);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_get_double);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_get_unsigned_integer);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_get_integer);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_diagnostics);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_resultset_metadata);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_result);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_attributes);

extern REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_udf_metadata);
#if !defined(NDEBUG)
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_debug_keyword_service);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_debug_sync_service);
#endif

auto handle_non_select_statement_result(my_h_statement statement,
                                        unsigned char *error) -> std::string {
  *error = 1;
  auto num_affected_rows = uint64_t{};
  if (SERVICE_PLACEHOLDER(mysql_stmt_diagnostics)
          ->affected_rows(statement, &num_affected_rows) != 0) {
    return {};
  }
  auto last_insert_id = uint64_t{};
  if (SERVICE_PLACEHOLDER(mysql_stmt_diagnostics)
          ->insert_id(statement, &last_insert_id) != 0) {
    return {};
  }
  *error = 0;
  return "Number of affected rows: " + std::to_string(num_affected_rows) +
         "\nLast insert id: " + std::to_string(last_insert_id);
}
auto parse_headers(uint64_t num_fields, my_h_statement statement,
                   unsigned char *error) -> std::vector<std::string> {
  *error = 1;
  auto header_row = std::vector<std::string>{};
  auto field = my_h_field{nullptr};
  auto field_name = static_cast<char *>(nullptr);
  auto charset_name = static_cast<char *>(nullptr);
  auto collation_name = static_cast<char *>(nullptr);
  for (auto j = size_t{}; j < num_fields; j++) {
    if (SERVICE_PLACEHOLDER(mysql_stmt_resultset_metadata)
            ->fetch_field(statement, j, &field) != 0)
      return {};
    if (SERVICE_PLACEHOLDER(mysql_stmt_resultset_metadata)
            ->field_info(field, "col_name", &field_name) != 0)
      return {};
    if (SERVICE_PLACEHOLDER(mysql_stmt_resultset_metadata)
            ->field_info(field, "charset_name", &charset_name) != 0)
      return {};
    if (SERVICE_PLACEHOLDER(mysql_stmt_resultset_metadata)
            ->field_info(field, "collation_name", &collation_name) != 0)
      return {};
    auto header = std::string{field_name};
    header_row.push_back(header);
  }
  *error = 0;
  return header_row;
}

auto get_field_type(my_h_statement statement, size_t index,
                    unsigned char *error) -> uint64_t {
  auto field = my_h_field{nullptr};
  if (SERVICE_PLACEHOLDER(mysql_stmt_resultset_metadata)
          ->fetch_field(statement, index, &field) != 0) {
    *error = 1;
    return {};
  }
  auto field_type = uint64_t{};
  if (SERVICE_PLACEHOLDER(mysql_stmt_resultset_metadata)
          ->field_info(field, "type", &field_type) != 0) {
    *error = 1;
    return {};
  }
  return field_type;
}

auto parse_value_at_index(uint64_t field_type, my_h_row row, size_t index)
    -> std::string {
  auto str = mysql_cstring_with_length{};
  auto int_val = int64_t{};
  auto float_val = double{};
  auto is_null = bool{};
  switch (field_type) {
    case MYSQL_SP_ARG_TYPE_VARCHAR:
    case MYSQL_SP_ARG_TYPE_STRING:
      if (SERVICE_PLACEHOLDER(mysql_stmt_get_string)
              ->get(row, index, &str, &is_null) != 0)
        return {};
      if (is_null) return {};
      return std::string(str.str, str.length);
    case MYSQL_SP_ARG_TYPE_TINY:
    case MYSQL_SP_ARG_TYPE_SHORT:
    case MYSQL_SP_ARG_TYPE_LONG:
    case MYSQL_SP_ARG_TYPE_INT24:
    case MYSQL_SP_ARG_TYPE_LONGLONG:
      if (SERVICE_PLACEHOLDER(mysql_stmt_get_integer)
              ->get(row, index, &int_val, &is_null) != 0)
        return {};
      if (is_null) return {};
      return std::to_string(int_val);
    case MYSQL_SP_ARG_TYPE_FLOAT:
    case MYSQL_SP_ARG_TYPE_DOUBLE:
      if (SERVICE_PLACEHOLDER(mysql_stmt_get_double)
              ->get(row, index, &float_val, &is_null) != 0)
        return {};
      if (is_null) return {};
      return std::to_string(float_val);
    default:
      if (SERVICE_PLACEHOLDER(mysql_stmt_get_string)
              ->get(row, index, &str, &is_null) != 0)
        return {};
      if (is_null) return {};
      return std::string(str.str, str.length);
  };
}

auto fetch_statement_row(my_h_statement statement) -> std::optional<my_h_row> {
  auto row = static_cast<my_h_row>(nullptr);
  if (SERVICE_PLACEHOLDER(mysql_stmt_result)->fetch(statement, &row) != 0)
    return {};
  return row;
}
auto fetch_data_row(my_h_statement statement, my_h_row row, size_t fields_count,
                    unsigned char *error) -> std::vector<std::string> {
  auto result = std::vector<std::string>{};
  for (auto i = 0ULL; i < fields_count; i++) {
    auto field_type = get_field_type(statement, i, error);
    auto value = parse_value_at_index(field_type, row, i);
    result.push_back(value);
  }
  return result;
}
auto parse_rows(my_h_statement statement, size_t fields_count,
                unsigned char *error) -> std::vector<std::vector<std::string>> {
  auto result = std::vector<std::vector<std::string>>{};
  for (auto row = fetch_statement_row(statement); row && *row;
       row = fetch_statement_row(statement)) {
    result.push_back(fetch_data_row(statement, *row, fields_count, error));
  }
  return result;
}

// Make a string where values are separated by separator
auto string_from_vector(const std::vector<std::string> &values,
                        const std::string separator) -> std::string {
  auto temp = std::string{};
  auto first = true;
  for (const auto &header : values) {
    if (first) {
      temp += header;
      first = false;
    } else {
      temp += separator + header;
    }
  }
  return temp;
}
auto string_from_result(std::vector<std::string> &header_row,
                        std::vector<std::vector<std::string>> &data_rows)
    -> std::string {
  auto header_string = string_from_vector(header_row, "\t");
  auto row_strs = std::vector<std::string>{};
  for (const auto &r : data_rows) {
    auto row_str = string_from_vector(r, "\t");
    row_strs.push_back(row_str);
  }
  auto result_string = string_from_vector(row_strs, "\n");
  return header_string + "\n" + result_string;
}

auto print_output(char *result, unsigned long *length, std::string message)
    -> char * {
  snprintf(result, 255, "%s", message.data());
  *length = message.length();
  return result;
}

auto handle_error(my_h_statement statement, unsigned char *error, char *result,
                  unsigned long *length) -> char * {
  *error = 1;
  auto error_number = uint64_t{};
  auto sql_state = mysql_cstring_with_length{};
  auto sql_errmsg = mysql_cstring_with_length{};

  if (SERVICE_PLACEHOLDER(mysql_stmt_diagnostics)
          ->error_number(statement, &error_number) ||
      SERVICE_PLACEHOLDER(mysql_stmt_diagnostics)
          ->error(statement, &sql_errmsg) ||
      SERVICE_PLACEHOLDER(mysql_stmt_diagnostics)
          ->sqlstate(statement, &sql_state)) {
    // Setting error = 0 so that the error message is displayed as result
    *error = 0;
    std::string error_msg =
        "Error in getting the error from the DA. This probably means there is "
        "an error at the service layer.";
    return print_output(result, length, error_msg);
  }
  auto error_msg =
      "Error no: " + std::to_string(error_number) +
      " Error state is: " + std::string{sql_state.str, sql_state.length} +
      " Error message is: " + std::string{sql_errmsg.str, sql_errmsg.length};
  return print_output(result, length, error_msg);
}
