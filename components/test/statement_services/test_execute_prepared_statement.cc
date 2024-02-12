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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <sstream>  // for ostringstream
#include <string>
#include <vector>

#include "include/mysql/components/services/bits/stored_program_bits.h"
#include "mysql/components/services/defs/mysql_string_defs.h"

#include "field_types.h"
#include "mysql.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "scope_guard.h"
#include "sql/sql_udf.h"
#include "template_utils.h"
#include "utils.h"

#include "my_byteorder.h"

REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_factory);
REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_execute);
REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_execute_direct);
REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_metadata);
REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_bind);
REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_get_string);
REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_get_time);
REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_get_double);
REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_get_unsigned_integer);
REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_get_integer);
REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_diagnostics);
REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_resultset_metadata);
REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_result);
REQUIRES_SERVICE_PLACEHOLDER(mysql_stmt_attributes);

REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(mysql_udf_metadata);
#if !defined(NDEBUG)
REQUIRES_SERVICE_PLACEHOLDER(mysql_debug_keyword_service);
REQUIRES_SERVICE_PLACEHOLDER(mysql_debug_sync_service);
#endif

BEGIN_COMPONENT_PROVIDES(test_execute_prepared_statement)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_execute_prepared_statement)
REQUIRES_SERVICE(mysql_stmt_factory), REQUIRES_SERVICE(mysql_stmt_execute),
    REQUIRES_SERVICE(mysql_stmt_execute_direct),
    REQUIRES_SERVICE(mysql_stmt_metadata), REQUIRES_SERVICE(mysql_stmt_bind),
    REQUIRES_SERVICE(mysql_stmt_get_string),
    REQUIRES_SERVICE(mysql_stmt_get_time),
    REQUIRES_SERVICE(mysql_stmt_get_double),
    REQUIRES_SERVICE(mysql_stmt_get_unsigned_integer),
    REQUIRES_SERVICE(mysql_stmt_get_integer),
    REQUIRES_SERVICE(mysql_stmt_diagnostics),
    REQUIRES_SERVICE(mysql_stmt_resultset_metadata),
    REQUIRES_SERVICE(mysql_stmt_result),
    REQUIRES_SERVICE(mysql_stmt_attributes), REQUIRES_SERVICE(udf_registration),
    REQUIRES_SERVICE(mysql_udf_metadata),
#if !defined(NDEBUG)
    REQUIRES_SERVICE(mysql_debug_keyword_service),
    REQUIRES_SERVICE(mysql_debug_sync_service),
#endif
    END_COMPONENT_REQUIRES();

auto execute_statement(my_h_statement statement, unsigned char *error,
                       char *result, unsigned long *length) -> char * {
  if (SERVICE_PLACEHOLDER(mysql_stmt_execute)->execute(statement) != 0) {
    return handle_error(statement, error, result, length);
  }
  return nullptr;
}

static auto test_execute_prepared_statement(UDF_INIT *, UDF_ARGS *arguments,
                                            char *result, unsigned long *length,
                                            unsigned char *,
                                            unsigned char *error) -> char * {
  *error = 1;

  auto statement = my_h_statement{nullptr};
  auto query =
      mysql_cstring_with_length{arguments->args[0], strlen(arguments->args[0])};

  if (SERVICE_PLACEHOLDER(mysql_stmt_factory)->init(&statement) != 0) return {};

  Scope_guard free_statement_guard(
      [&] { SERVICE_PLACEHOLDER(mysql_stmt_factory)->close(statement); });

  auto rows_per_fetch = size_t{3};
  auto prefetch_row_name =
      mysql_cstring_with_length{"prefetch_rows", strlen("prefetch_rows")};
  if (SERVICE_PLACEHOLDER(mysql_stmt_attributes)
          ->set(statement, prefetch_row_name,
                static_cast<const void *>(&rows_per_fetch)) != 0)
    return {};

  auto num_rows_per_fetch = size_t{};
  if (SERVICE_PLACEHOLDER(mysql_stmt_attributes)
          ->get(statement, prefetch_row_name, &num_rows_per_fetch) != 0)
    return {};

  auto buffer_capacity = size_t{};
  auto buffer_capacity_name =
      mysql_cstring_with_length{"buffer_capacity", strlen("buffer_capacity")};
  if (SERVICE_PLACEHOLDER(mysql_stmt_attributes)
          ->get(statement, buffer_capacity_name, &buffer_capacity) != 0)
    return {};

  char *charset_name = nullptr;
  auto charset_name_name =
      mysql_cstring_with_length{"charset_name", strlen("charset_name")};
  if (SERVICE_PLACEHOLDER(mysql_stmt_attributes)
          ->get(statement, charset_name_name, &charset_name) != 0)
    return {};

  if (SERVICE_PLACEHOLDER(mysql_stmt_execute)->prepare(query, statement) != 0)
    return handle_error(statement, error, result, length);

// For testing calling prepare 2nd time and calling execute_direct for prepared
// stmt
#if !defined(NDEBUG)
  if (SERVICE_PLACEHOLDER(mysql_debug_keyword_service)
          ->lookup_debug_keyword("second_prepare")) {
    auto second_query = "SELECT * FROM mle_db.my_table WHERE col_c = ?";
    if (SERVICE_PLACEHOLDER(mysql_stmt_execute)
            ->prepare({second_query, strlen(second_query)}, statement) != 0)
      return handle_error(statement, error, result, length);
  }

  if (SERVICE_PLACEHOLDER(mysql_debug_keyword_service)
          ->lookup_debug_keyword("second_execute_direct")) {
    auto second_query = "SELECT * FROM mle_db.my_table";
    if (SERVICE_PLACEHOLDER(mysql_stmt_execute_direct)
            ->execute({second_query, strlen(second_query)}, statement) != 0)
      return handle_error(statement, error, result, length);
  }
#endif

  assert(num_rows_per_fetch == 3);

#if !defined(NDEBUG)
  // For testing setting/getting attribute after execute
  if (SERVICE_PLACEHOLDER(mysql_debug_keyword_service)
          ->lookup_debug_keyword("attribute_set_after_prepare")) {
    if (SERVICE_PLACEHOLDER(mysql_stmt_attributes)
            ->set(statement, prefetch_row_name,
                  reinterpret_cast<const void *>(3)) != 0) {
      return {};
    }
  }

  if (SERVICE_PLACEHOLDER(mysql_debug_keyword_service)
          ->lookup_debug_keyword("attribute_get_after_prepare")) {
    if (SERVICE_PLACEHOLDER(mysql_stmt_attributes)
            ->get(statement, prefetch_row_name, &num_rows_per_fetch) != 0) {
      return {};
    }
  }

  assert(num_rows_per_fetch == 3);
#endif

  auto num_parameters = uint32_t{};
  if (SERVICE_PLACEHOLDER(mysql_stmt_metadata)
          ->param_count(statement, &num_parameters) != 0)
    return {};

// For testing setting and getting parameter
#if !defined(NDEBUG)
  if (SERVICE_PLACEHOLDER(mysql_debug_keyword_service)
          ->lookup_debug_keyword("parameter_set")) {
    auto value = 12;
    if (SERVICE_PLACEHOLDER(mysql_stmt_bind)
            ->bind_param(statement, num_parameters, false,
                         MYSQL_SP_ARG_TYPE_LONG, false, &value,
                         sizeof(long long), nullptr, 0) != 0) {
      return {};
    }
  }
  if (SERVICE_PLACEHOLDER(mysql_debug_keyword_service)
          ->lookup_debug_keyword("parameter_get")) {
    auto value = uint64_t{};
    if (SERVICE_PLACEHOLDER(mysql_stmt_metadata)
            ->param_metadata(statement, num_parameters, "type", &value) != 0) {
      return {};
    }
  }
#endif

  auto param_index = size_t{0};
  for (auto arg_index = size_t{1}; arg_index < arguments->arg_count;
       arg_index++) {
    switch (arguments->arg_type[arg_index]) {
      case STRING_RESULT: {
        auto value = arguments->args[arg_index];
        if (SERVICE_PLACEHOLDER(mysql_stmt_bind)
                ->bind_param(statement, param_index, false,
                             MYSQL_SP_ARG_TYPE_VARCHAR, false, value,
                             strlen(value), nullptr, 0) != 0) {
          return {};
        }
        break;
      }
      case INT_RESULT: {
        auto value = uint8korr(
            reinterpret_cast<unsigned char *>(arguments->args[arg_index]));
        if (SERVICE_PLACEHOLDER(mysql_stmt_bind)
                ->bind_param(statement, param_index, false,
                             MYSQL_SP_ARG_TYPE_LONGLONG, false, &value,
                             sizeof(long long), nullptr, 0) != 0) {
          return {};
        }
        break;
      }
      case REAL_RESULT: {
        auto value = float8get(
            reinterpret_cast<unsigned char *>(arguments->args[arg_index]));
        if (SERVICE_PLACEHOLDER(mysql_stmt_bind)
                ->bind_param(statement, param_index, false,
                             MYSQL_SP_ARG_TYPE_DOUBLE, false, &value,
                             sizeof(double), nullptr, 0) != 0) {
          return {};
        }
        break;
      }
      default:
        break;
    }
    ++param_index;
  }

#if !defined(NDEBUG)
  // The following bind calls are just for code coverage. The query from the
  // test is "SELECT * FROM mle_db.my_table WHERE col_c = ?". The calls are
  // expected to fail as param_index = 1 which is >= number of parameters.
  if (SERVICE_PLACEHOLDER(mysql_debug_keyword_service)
          ->lookup_debug_keyword("code_coverage")) {
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_DECIMAL,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_TINY,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_SHORT,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_FLOAT,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_NULL,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_TIMESTAMP,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_INT24,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_TIME,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_YEAR,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_NEWDATE,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_BIT, false,
                     nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_TIMESTAMP2,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_DATETIME2,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_TIME2,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true,
                     MYSQL_SP_ARG_TYPE_TYPED_ARRAY, false, nullptr,
                     sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_INVALID,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_BOOL,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_JSON,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_NEWDECIMAL,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_ENUM,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_SET, false,
                     nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_TINY_BLOB,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true,
                     MYSQL_SP_ARG_TYPE_MEDIUM_BLOB, false, nullptr,
                     sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_LONG_BLOB,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_BLOB,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_VAR_STRING,
                     false, nullptr, sizeof(double), nullptr, 0);
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, MYSQL_SP_ARG_TYPE_GEOMETRY,
                     false, nullptr, sizeof(double), nullptr, 0);
    // Invalid enum to cover the default case
    SERVICE_PLACEHOLDER(mysql_stmt_bind)
        ->bind_param(statement, param_index, true, 2222, false, nullptr,
                     sizeof(double), nullptr, 0);
  }
#endif

  if (auto exec_result = execute_statement(statement, error, result, length);
      exec_result != nullptr) {
    return exec_result;
  }

#if !defined(NDEBUG)
  // For testing setting/getting attribute after execute
  if (SERVICE_PLACEHOLDER(mysql_debug_keyword_service)
          ->lookup_debug_keyword("attribute_set_after_execute")) {
    if (SERVICE_PLACEHOLDER(mysql_stmt_attributes)
            ->set(statement, prefetch_row_name,
                  reinterpret_cast<const void *>(3)) != 0) {
      return {};
    }
  }
  if (SERVICE_PLACEHOLDER(mysql_debug_keyword_service)
          ->lookup_debug_keyword("attribute_get")) {
    if (SERVICE_PLACEHOLDER(mysql_stmt_attributes)
            ->get(statement, prefetch_row_name, &num_rows_per_fetch) != 0) {
      return {};
    }
  }
#endif

// For testing binding parameter
#if !defined(NDEBUG)
  if (SERVICE_PLACEHOLDER(mysql_debug_keyword_service)
          ->lookup_debug_keyword("second_bind")) {
    auto value = std::string{"13"};
    if (SERVICE_PLACEHOLDER(mysql_stmt_bind)
            ->bind_param(statement, 0, false, MYSQL_SP_ARG_TYPE_VARCHAR, false,
                         value.data(), value.length(), nullptr, 0) != 0) {
      return {};
    }

    if (auto exec_result = execute_statement(statement, error, result, length);
        exec_result != nullptr) {
      return exec_result;
    }
  }
#endif

  auto field_count = uint32_t{};
  if (SERVICE_PLACEHOLDER(mysql_stmt_resultset_metadata)
          ->field_count(statement, &field_count) != 0)
    return {};

  if (field_count == 0) {
    *error = 0;
    auto output = handle_non_select_statement_result(statement, error);
    return print_output(result, length, output);
  }

  auto has_next_cursor = false;
  auto num_result_sets = size_t{};
  auto header_rows = std::vector<std::vector<std::string>>{};
  auto full_result = std::vector<std::vector<std::vector<std::string>>>{};
  do {
    ++num_result_sets;

    auto num_fields = uint32_t{};
    if (SERVICE_PLACEHOLDER(mysql_stmt_resultset_metadata)
            ->field_count(statement, &num_fields) != 0)
      return {};

    // Parse header
    auto headers = parse_headers(num_fields, statement, error);
    header_rows.push_back(headers);

    // Parse rows
    auto data_rows = parse_rows(statement, num_fields, error);
    full_result.push_back(data_rows);

    // Get next result set
    if (SERVICE_PLACEHOLDER(mysql_stmt_result)
            ->next_result(statement, &has_next_cursor) != 0)
      return {};
  } while (has_next_cursor);

  *error = 0;
  auto output = std::string{};

// For testing getting attribute
#if !defined(NDEBUG)
  if (SERVICE_PLACEHOLDER(mysql_debug_keyword_service)
          ->lookup_debug_keyword("attribute_get")) {
    output += std::to_string(num_rows_per_fetch) + "\n";
  }
#endif

  for (auto i = size_t{}; i < num_result_sets; i++) {
    auto header_row = header_rows[i];
    auto data_rows = full_result[i];
    output += string_from_result(header_row, data_rows) + "\n";
  }

  return print_output(result, length, output);
}

auto test_execute_prepared_statement_init(UDF_INIT *udf_init, UDF_ARGS *,
                                          char *) -> bool {
  if (SERVICE_PLACEHOLDER(mysql_udf_metadata)
          ->result_set(udf_init, "charset", const_cast<char *>("utf8mb4")))
    return 1;
  return 0;
}

static auto init() -> mysql_service_status_t {
  Udf_func_string udf = test_execute_prepared_statement;
  if (SERVICE_PLACEHOLDER(udf_registration)
          ->udf_register("test_execute_prepared_statement", STRING_RESULT,
                         reinterpret_cast<Udf_func_any>(udf),
                         reinterpret_cast<Udf_func_init>(
                             test_execute_prepared_statement_init),
                         nullptr)) {
    fprintf(stderr, "Can't register the test_execute_prepared_statement UDF\n");
    return 1;
  }

  return 0;
}

static auto deinit() -> mysql_service_status_t {
  int was_present = 0;
  if (SERVICE_PLACEHOLDER(udf_registration)
          ->udf_unregister("test_execute_prepared_statement", &was_present))
    fprintf(stderr,
            "Can't unregister the test_execute_prepared_statement UDF\n");
  return 0; /* success */
}

BEGIN_COMPONENT_METADATA(test_execute_prepared_statement)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_execute_prepared_statement,
                  "mysql:test_execute_prepared_statement")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_execute_prepared_statement)
    END_DECLARE_LIBRARY_COMPONENTS
