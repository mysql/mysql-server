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

#ifndef MYSQL_STATEMENT_SERVICE_H
#define MYSQL_STATEMENT_SERVICE_H

#include <mysql/components/service.h>
#include <stddef.h>
#include <stdint.h>
#include <cstddef>
#include "mysql/components/services/bits/mle_time_bits.h"
#include "mysql/components/services/bits/stored_program_bits.h"
#include "mysql/components/services/defs/mysql_string_defs.h"

DEFINE_SERVICE_HANDLE(my_h_statement);
DEFINE_SERVICE_HANDLE(my_h_row);
DEFINE_SERVICE_HANDLE(my_h_field);
DEFINE_SERVICE_HANDLE(my_h_warning);

/**
 * @note Users of the service can set the expected charset e.g
 * SERVICE_PLACEHOLDER(mysql_stmt_attributes)->set("charset_name", "utf8mb4");
 * The service expects all the input strings to be in this charset and all the
 * output strings from the service will be in this charset.
 *
 */

// clang-format off
/**
 @ingroup group_components_services_inventory

 A service that provides the API to create, and deallocate a
 statement. The statement can be either a regular or a prepared statement.

 Usage example for prepared statement:
    my_h_statement statement = nullptr;
    SERVICE_PLACEHOLDER(mysql_stmt_factory)->init(&statement);

    SERVICE_PLACEHOLDER(mysql_stmt_attributes)->set("charset_name", "utf8mb4");

    SERVICE_PLACEHOLDER(mysql_stmt_execute)->prepare("SELECT * FROM ?", statement);

    uint32_t parameter_count;
    SERVICE_PLACEHOLDER(mysql_stmt_metadata)->param_count(statement, &parameter_count);

    // For first parameter
    SERVICE_PLACEHOLDER(mysql_stmt_bind)->bind_param(statement, 0, false,
                                      MYSQL_TYPE_LONG, false, 4, 1, nullptr, 0);

    SERVICE_PLACEHOLDER(mysql_stmt_execute)->execute(statement)

    my_h_row row = nullptr;
    SERVICE_PLACEHOLDER(mysql_stmt_result)->fetch(statement, &row);
    SERVICE_PLACEHOLDER(mysql_stmt_factory)->close(statement)

  Usage example for regular statement:
    my_h_statement statement = nullptr;
    SERVICE_PLACEHOLDER(mysql_stmt_factory)->init(&statement);

    SERVICE_PLACEHOLDER(mysql_stmt_attributes)->set("charset_name", "utf8mb4");

    SERVICE_PLACEHOLDER(mysql_stmt_execute_direct)->execute("SELECT * FROM my_table", statement);

    my_h_row row = nullptr;
    SERVICE_PLACEHOLDER(mysql_stmt_result)->fetch(statement, &row);
    SERVICE_PLACEHOLDER(mysql_stmt_factory)->close(statement)
 */

// clang-format on

BEGIN_SERVICE_DEFINITION(mysql_stmt_factory)
/**
  @brief Construct a new statement object.

  @note if the statement already exists, it is overwritten.

  @param [out] statement A handle to the statement
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(init, (my_h_statement * statement));

/**
  @brief Close and clean up resource related to a statement.

  @param [in] statement A handle to the statement
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(close, (my_h_statement statement));
END_SERVICE_DEFINITION(mysql_stmt_factory)

/**
 @ingroup group_components_services_inventory

 A service that provides the API to execute prepared statements.

 Usage example:
  // Prepare a statement
  my_h_statement statement = nullptr;
  SERVICE_PLACEHOLDER(mysql_stmt_factory)->init(&statement);
  SERVICE_PLACEHOLDER(mysql_stmt_execute)->prepare("SELECT * FROM my_table WHERE
 col_c = ?", statement)
  SERVICE_PLACEHOLDER(mysql_stmt_bind)->bind_param(statement, 0, false,
                                      MYSQL_TYPE_LONG, false, 4, 1, nullptr, 0);

  // To reset the parameter to bind it with new values
  SERVICE_PLACEHOLDER(mysql_stmt_execute)->reset(statement)

  // Execute the statement
  SERVICE_PLACEHOLDER(mysql_stmt_execute)->execute(statement)


*/
BEGIN_SERVICE_DEFINITION(mysql_stmt_execute)
/**
  @brief Execute the prepared statement.

  @note Execute will fail if all parameters are not bound.
  @note To execute regular statement, use mysql_stmt_execute_direct::execute
  instead.

  @param [in] statement A handle to the statement
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(execute, (my_h_statement statement));

/**
  @brief Prepare the statement with the query.

  @note Re-preparation is not allowed after the statement has been created. To
  prepare a new query, create a new statement instead.
  @note Calling this on regular statements would fail.

  @param [in] query The query string to be prepared
  @param [in] statement A handle to the statement
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(prepare, (mysql_cstring_with_length query,
                              my_h_statement statement));

/**
  @brief For prepared statements, calling reset would reset the binding
  parameters, close the cursor if one is open and frees the result sets.
  For regular statements, calling reset is not supported. To close
  and clean up the statement, use mysql_stmt_factory::close.

  @note For prepared statements, the statement still exists, only the result
  sets are destroyed.
  @note Calling this on regular statements would fail.

  @param [in] statement A handle to the statement
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(reset, (my_h_statement statement));

END_SERVICE_DEFINITION(mysql_stmt_execute)

BEGIN_SERVICE_DEFINITION(mysql_stmt_execute_direct)
/**
  @brief Execute the regular statement with the specified query using execute
  direct.

  @param [in] query The query string to be executed
  @param [in] statement A handle to the statement
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(execute, (mysql_cstring_with_length query,
                              my_h_statement statement));
END_SERVICE_DEFINITION(mysql_stmt_execute_direct)

// clang-format off
/**
 @ingroup group_components_services_inventory

 A service that provides the API to bind the parameters in prepared statements.

 Usage example:

   // For first parameter
   SERVICE_PLACEHOLDER(mysql_stmt_bind)->bind_param(statement, 0, false,
                                              MYSQL_TYPE_LONG, false, 4, 1, nullptr, 0);

*/

// clang-format on
BEGIN_SERVICE_DEFINITION(mysql_stmt_bind)
/**
  @brief Bind a parameter

  @note Calling bind with regular statements results in failure.
  @note Bound values are cached. To update only some parameters, call bind on
  these, the rest will use the cached values.
  @note Calling bind with the same index multiple times, only the last value is
  kept.

  @param [in] statement A handle to the statement
  @param [in] index 0-based index of the parameter to be bound
  @param [in] is_null Whether the parameter can be null
  @param [in] type Type of the parameter. List of supported types: Table 6.1 at
  https://dev.mysql.com/doc/c-api/8.0/en/c-api-prepared-statement-type-codes.html
  @param [in] bool Whether the value is unsigned
  @param [in] data The data argument is the value for the member
  @param [in] data_length Length of the data
  @param [in] name Name of the parameter
  @param [in] name_length Name length of the parameter
  @return Status of the performed operation
  @retval false success
  @retval true failure

 */
DECLARE_BOOL_METHOD(bind_param, (my_h_statement statement, uint32_t index,
                                 bool is_null, uint64_t type, bool is_unsigned,
                                 const void *data, unsigned long data_length,
                                 const char *name, unsigned long name_length));

END_SERVICE_DEFINITION(mysql_stmt_bind)

// clang-format off
/**
 @ingroup group_components_services_inventory

 A service that provides the API to manage and get info about a result set including fetch row(s)
 from a result set, get next result set. A result set contains the result of an execution.
 It is a list of rows, each row is a list of values where each value corresponds to a column.
 
 Usage example:
    // Iterate over the rows in a result set for result set with data (num_fields != 0)
    // For in-depth example, check test_execute_prepared_statement.cc and test_execute_regular_statement.cc
    my_h_row row = nullptr;
    do {
        SERVICE_PLACEHOLDER(mysql_stmt_result)->fetch(statement, &row);
        if (row == nullptr) {
          break;
        }
        // Get data from a row using row services
    } while (true)

    // Iterate over result sets
    auto has_next_result_set = bool{};
    do {
      SERVICE_PLACEHOLDER(mysql_stmt_result)->next_result(statement, &has_next_result_set);
    } while(has_next_result_set);

 */
// clang-format on

BEGIN_SERVICE_DEFINITION(mysql_stmt_result)

/**
  @brief Check if there is more result set and move to next result set if there
  is.

  @param [in] statement A handle to the statement
  @param [out] has_next Whether there is more result set
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(next_result, (my_h_statement statement, bool *has_next));

/**
  @brief Fetch one row from the current result set

  @note If there is no more rows, the returned row handle will be a nullptr.
  This is not considered a failure case.

  @param [in] statement A handle to the statement
  @param [out] row The row handle
  @return Status of the performed operation
  @retval false success if there is no error in fetching the row including no
  more rows.
  @retval true failure in case of any error in fetching the row
 */
DECLARE_BOOL_METHOD(fetch, (my_h_statement statement, my_h_row *row));

END_SERVICE_DEFINITION(mysql_stmt_result)

// clang-format off
/**
 @ingroup group_components_services_inventory

 A service that provides the API to get the errors and warnings including
 fetching the warning, getting error/warning number, error/warning level,
 error/warning message, SQL state. In addition, for INSERT/UPDATE/DELETE, 
 the service provides the API to get the number of affected rows and
 last insert ID.

   Usage example:
    // For INSERT/UPDATE/DELETE/... statements, to get the number of affected rows
    // and last insert id.

    uint64_t num_affected_rows;
    SERVICE_PLACEHOLDER(mysql_stmt_diagnostics)->affected_rows(statement,
                                                      &num_affected_rows)

    auto last_insert_id = uint64_t{};
    SERVICE_PLACEHOLDER(mysql_stmt_diagnostics)->insert_id(statement,
                                                        &last_insert_id)

    // To get the diagnostics information
    auto error_number = uint64_t{};
    SERVICE_PLACEHOLDER(mysql_stmt_diagnostics)->error_id(statement,
                                                                &error_number);
    char const *sql_errmsg = nullptr;
    SERVICE_PLACEHOLDER(mysql_stmt_diagnostics)->error(statement,
                                                                 &sql_errmsg);
    char const *sql_state = nullptr;
    SERVICE_PLACEHOLDER(mysql_stmt_diagnostics)->get_sql_state(statement,
                                                             &sql_state);
    auto warning_count = uint32_t{};
    SERVICE_PLACEHOLDER(mysql_stmt_diagnostics)->num_warnings(statement,
                                                              &warning_count);

    for(size_t warn_index = 0; warn_index < warning_count; warn_index++) {
        my_h_warning warning = nullptr;
        SERVICE_PLACEHOLDER(mysql_stmt_diagnostics)->get_warning(statement,
                                                      warn_index, &warning);
        auto level = uint32_t{};
        SERVICE_PLACEHOLDER(mysql_stmt_diagnostics)->warning_level(warning,
                                                                      &level);

        // Similarly for code and message
    }

 */
// clang-format on
BEGIN_SERVICE_DEFINITION(mysql_stmt_diagnostics)
/**
  @brief Get the number of affected rows for DDL e.g. UPDATE/DELETE/INSERT
  statements.

  @param [in] statement A handle to the statement
  @param [out] num_rows Number of rows affected
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(affected_rows,
                    (my_h_statement statement, uint64_t *num_rows));
/**
  @brief Get the last insert id which is the ID generated for an AUTO_INCREMENT
  column.

  @param [in] statement A handle to the statement
  @param [out] last_id The last insert id
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(insert_id, (my_h_statement statement, uint64_t *last_id));
/**
  @brief Get the error number of the last error.

  @param [in] statement The statement handle
  @param [out] error_number The error number
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(error_number,
                    (my_h_statement statement, uint64_t *error_number));

/**
  @brief Get the error message of the last error.

  @param [in] statement The statement handle
  @param [out] error_message The error message
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(error, (my_h_statement statement,
                            mysql_cstring_with_length *error_message));

/**
  @brief Get SQLSTATE error code for the last error similar to
  https://dev.mysql.com/doc/c-api/8.0/en/mysql-stmt-sqlstate.html

  @param [in] statement The statement handle
  @param [out] sqlstate Stores the SQLSTATE status of the most
              recently executed SQL stmt.
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(sqlstate, (my_h_statement statement,
                               mysql_cstring_with_length *sqlstate));

/**
  @brief Get the number of warnings of the recently invoked statement.

  @param [in] statement The statement handle
  @param [out] count The number of warnings
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(num_warnings, (my_h_statement statement, uint32_t *count));

/**
  @brief Get the warning at the index.

  @param [in] statement The statement handle
  @param [in] warning_index 0-based index of the warning
  @param [out] warning The warning
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(get_warning,
                    (my_h_statement statement, uint32_t warning_index,
                     my_h_warning *warning));

/**
  @brief Get the severity level of the warning.

  @param [in] warning The warning
  @param [out] level The level
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(warning_level, (my_h_warning warning, uint32_t *level));

/**
  @brief Get the code of the warning.

  @param [in] warning The warning
  @param [out] code The code
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(warning_code, (my_h_warning warning, uint32_t *code));

/**
  @brief Get the message of the warning.

  @param [in] warning The warning
  @param [out] code The message
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(warning_message,
                    (my_h_warning warning,
                     mysql_cstring_with_length *error_message));
END_SERVICE_DEFINITION(mysql_stmt_diagnostics)

BEGIN_SERVICE_DEFINITION(mysql_stmt_attributes)

// clang-format off
/**
  @brief Get the current value for a statement attribute.

  @param [in] statement A handle to the statement
  @param [in] name Attribute name to get

  The following attributes are supported:
  - Buffer capacity of the statement ("buffer_capacity" of the input size_t type, default: 500)
  - Number of prefetch rows for prepared statements ("prefetch_rows" of the input size_t type, default: 1)
  - Expected charset name ("charset_name" of the input mysql_cstring_with_length type, default: utf8mb4)
  - Use the existing protocol from thd ("use_thd_protocol" of the input bool type, default: false)

  @param [out] value The value of the attribute
  @retval false success
  @retval true failure
 */
// clang-format on

DECLARE_BOOL_METHOD(get, (my_h_statement statement,
                          mysql_cstring_with_length name, void *value));

/**
  @brief Set the value of a statement attribute.

  @note Setting the attribute is only allowed after init and before
  prepare/execute.

  @param [in] statement A handle to the statement
  @param [in] name Attribute name to be set
  @param [in] value The value of the attribute
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(set, (my_h_statement statement,
                          mysql_cstring_with_length name, const void *value));

END_SERVICE_DEFINITION(mysql_stmt_attributes)

// clang-format off
/**
 @ingroup group_components_services_inventory

  A service that provides the API to get information about statement metadata
  including the number of the parameters in a prepared statement and their metatdata.

    // For prepared statement, to get the number of parameters
    uint64_t num_parameters;
    SERVICE_PLACEHOLDER(mysql_stmt_metadata)->param_count(statement,
                                                      &num_parameters)


 */
// clang-format on

BEGIN_SERVICE_DEFINITION(mysql_stmt_metadata)

/**
  @brief Get the number of parameters in the prepared statement.

  @note Calling this on regular statements would fail.

  @param [in] statement A handle to the statement
  @param [out] parameter_count The number of parameters
  @return Status of the performed operation
  @retval false success
  @retval true failure

 */
DECLARE_BOOL_METHOD(param_count,
                    (my_h_statement statement, uint32_t *parameter_count));

// clang-format off

/**
  @brief Get the metadata of a parameter in a prepared statement. The metadata specifies 
  whether the parameter can be null, the type of the parameter, whether the value is signed.

  For example, in this query, "SELECT * FROM table WHERE col = ?", we need to know the parameter
  specified by '?' is null or not, its type, and if it is integer, whether it is signed. MySQL
  will provide this information and this function is to get these info.

  auto is_nullable = bool{};
  if (SERVICE_PLACEHOLDER(mysql_stmt_metadata)
          ->param_metadata(statement, index, "null_bit", &is_nullable))
    return {};
  auto sql_type = Sql_type{};
  if (SERVICE_PLACEHOLDER(mysql_stmt_metadata)
          ->param_metadata(statement, index, "type", &sql_type))
    return {};
  auto is_unsigned = bool{};
  if (SERVICE_PLACEHOLDER(mysql_stmt_metadata)
          ->param_metadata(statement, index, "is_unsigned", &is_unsigned))
    return {};

  @note Calling this on regular statements would fail.

  @param [in] statement A handle to the statement
  @param [in] index 0-based index of the parameter
  @param [in] metadata The metadata argument is the attribute that you want to get

  The following attributes are supported:

  - Parameter is null ("null_bit" of the returned bool type)
  - Parameter type ("type" of the returned uint64_t type)
  - Parameter is unsigned ("is_unsigned" of the returned bool type)

  @param [out] data The data argument is the value for the member
  @return Status of the performed operation
  @retval false success
  @retval true failure

 */
// clang-format on

DECLARE_BOOL_METHOD(param_metadata, (my_h_statement statement, uint32_t index,
                                     const char *metadata, void *data));
END_SERVICE_DEFINITION(mysql_stmt_metadata)

// clang-format off
/**
 @ingroup group_components_services_inventory

 A service that provides the API to get information about a field or column in a result 
 set including get the number of fields, fetch a field and get information of a field.
 More info: https://dev.mysql.com/doc/c-api/8.0/en/mysql-stmt-field-count.html

  Usage example:

  auto num_fields = uint32_t{};
  SERVICE_PLACEHOLDER(mysql_stmt_resultset_metadata)->field_count(statement, &num_fields);

  my_h_field field = nullptr;
  for(size_t field_index = 0; field_index < num_fields; field_index++) {
    SERVICE_PLACEHOLDER(mysql_stmt_resultset_metadata)->fetch_field(statement,
                                                              field_index, &field);

    char* field_name = nullptr;
    SERVICE_PLACEHOLDER(mysql_stmt_resultset_metadata)->field_info(field,
                                             "col_name", &field_name);

    // Do similarly for other field metadata
  }
 */
// clang-format on
BEGIN_SERVICE_DEFINITION(mysql_stmt_resultset_metadata)

/**
  @brief Get the field handle by the index.

  @param [in] statement A handle to the statement
  @param [in] column_index 0-based index of the column
  @param [out] field The field handle
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(fetch_field, (my_h_statement statement,
                                  uint32_t column_index, my_h_field *field));

/**
  @brief Get the number of fields in the current result set.

  @param [in] statement A handle to the statement
  @param [out] num_fields The number of fields in this result set
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(field_count,
                    (my_h_statement statement, uint32_t *num_fields));

// clang-format off
/**
  @brief Get the information about the field.

  @param [in] field The field to get information from
  @param [in] name Name of the attributes to get from the field

  Currently, following attributes are supported:

  - Column name ("col_name" of the returned const char* type)
  - Original column name ("org_col_name" of the returned const char* type)
  - Database name ("db_name" of the returned const char* type)
  - Table name ("table_name" of the returned const char* type)
  - Original table name ("org_table_name" of the returned const char* type)
  - Field type ("type" of the returned uint64_t type)
  - Charset number ("charsetnr" of the returned uint type)
  - Charset name ("charset_name" of the returned const char* type)
  - Collation name ("collation_name" of the returned const char* type)
  - Field flags ("flags" of the returned uint type)
  - Field decimals ("decimals" of the returned uint type)
  - Whether the field is unsigned ("is_unsigned" of the returned bool type)
  - Whether the field is zerofill ("is_zerofill" of the returned bool type)

  @param [out] data The returned information
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
// clang-format on

DECLARE_BOOL_METHOD(field_info,
                    (my_h_field field, const char *name, void *data));
END_SERVICE_DEFINITION(mysql_stmt_resultset_metadata)

// clang-format off
/**
  @ingroup group_components_services_inventory

  A service that provides the API for get integer.

  Usage example:

  autp int_val = int64_t{};
  bool is_null;
  SERVICE_PLACEHOLDER(mysql_stmt_get_integer)->get(row, column_index,
                                                        &int_val, &is_null)
*/
// clang-format on

BEGIN_SERVICE_DEFINITION(mysql_stmt_get_integer)
/**
  @brief Get signed integer at column_index from a row.

  @param [in] row The row handle
  @param [in] column_index 0-based index at which the data is extracted
  @param [out] data The extracted integer
  @param [out] is_null Flag to indicate if value is null
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(get, (my_h_row row, uint32_t column_index, int64_t *data,
                          bool *is_null));
END_SERVICE_DEFINITION(mysql_stmt_get_integer)

/**
  @ingroup group_components_services_inventory

  A service that provides the API for get unsigned integer.
*/
BEGIN_SERVICE_DEFINITION(mysql_stmt_get_unsigned_integer)
/**
  @brief Get unsigned integer at column_index from a row.

  @param [in] row The row handle
  @param [in] column_index 0-based index at which the data is extracted
  @param [out] data The extracted unsigned integer
  @param [out] is_null Flag to indicate if value is null
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(get, (my_h_row row, uint32_t column_index, uint64_t *data,
                          bool *is_null));
END_SERVICE_DEFINITION(mysql_stmt_get_unsigned_integer)

/**
  @ingroup group_components_services_inventory

  A service that provides the API for get double.
*/
BEGIN_SERVICE_DEFINITION(mysql_stmt_get_double)
/**
  @brief Get float at column_index from a row.

  @param [in] row The row handle
  @param [in] column_index 0-based index at which the data is extracted
  @param [out] data The extracted double
  @param [out] is_null Flag to indicate if value is null
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(get, (my_h_row row, uint32_t column_index, double *data,
                          bool *is_null));
END_SERVICE_DEFINITION(mysql_stmt_get_double)

/**
  @ingroup group_components_services_inventory

  A service that provides the API for get time value from a row.

  auto result = Time_t{};
  auto is_null = false;

  if (SERVICE_PLACEHOLDER(mysql_stmt_get_time)
          ->get(row, column_index, &result.hour, &result.minute, &result.second,
                &result.microseconds, &result.negative,
                &is_null) == MYSQL_FAILURE)
    return {};
*/

BEGIN_SERVICE_DEFINITION(mysql_stmt_get_time)
/**
  @brief Get time at column_index from a row.

  @param [in] row The row handle
  @param [in] column_index 0-based index at which the data is extracted
  @param [out] time structure representing temporal type
  @param [out] is_null Flag to indicate if value is null
  @return Status of the performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(get, (my_h_row row, uint32_t column_index, mle_time *time,
                          bool *is_null));
END_SERVICE_DEFINITION(mysql_stmt_get_time)

/**
  @ingroup group_components_services_inventory

  A service that provides the API for get string value from a row.

  auto result = mysql_cstring_with_length{};
  auto is_null = false;
  if (SERVICE_PLACEHOLDER(mysql_stmt_get_string)
          ->get(row, column_index, &result, &is_null) == MYSQL_FAILURE) {
    return {};
  }
*/
BEGIN_SERVICE_DEFINITION(mysql_stmt_get_string)
/**
  @brief Get string at column_index from a row.

  @param [in] row The row handle
  @param [in] column_index 0-based index at which the data is extracted
  @param [out] data The extracted string
  @param [out] is_null Flag to indicate if value is null
  @return Status of the performed operation
  @retval false success
  @retval true failure
 */
DECLARE_BOOL_METHOD(get, (my_h_row row, uint32_t column_index,
                          mysql_cstring_with_length *data, bool *is_null));
END_SERVICE_DEFINITION(mysql_stmt_get_string)

#endif /* MYSQL_STATEMENT_SERVICE_H */
