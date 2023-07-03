/* Copyright (c) 2022, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_COMMAND_CONSUMER_H
#define MYSQL_COMMAND_CONSUMER_H

#include <mysql/components/service.h>
#include <mysql/components/services/mysql_command_services.h>
#include <cstddef>

DEFINE_SERVICE_HANDLE(SRV_CTX_H);
DEFINE_SERVICE_HANDLE(DECIMAL_T_H);
DEFINE_SERVICE_HANDLE(MYSQL_TIME_H);

/**
  The Field_metadata has the information about the field, which is used
  by field_metadata() service api.
*/
struct Field_metadata {
  const char *db_name;
  const char *table_name;
  const char *org_table_name;
  const char *col_name;
  const char *org_col_name;
  unsigned long length;
  unsigned int charsetnr;
  unsigned int flags;
  unsigned int decimals;
  int type;
};

/**
  An implementation of these services will be called as the data resulting from
  calling mysql_query() service are produced by the server.

  Most often the default implementation of this service that will cache the data
  into a memory structure into the MYSQL handle will be used. But this will only
  work if the resultset expected is relatively small.

  @note The default implementation will allow accessing the data after the query
  execution. This will not be possible with a custom method.

  If one wants to avoid the memory caching and process the data SAX style as
  they are returned by the SQL execution define, they should provide their own
  implementation of this service that will process the data differently at the
  time of their production.
*/

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for start and end.
*/
BEGIN_SERVICE_DEFINITION(mysql_text_consumer_factory_v1)

/**
  Prepares the data handle(srv_ctx_h), i.e allocates and prepares
  the Dom_ctx object and assigned to the srv_ctx_h.

  @param[out] srv_ctx_h Dom_ctx data handle.
  @param mysql_h mysql handle used to prepare srv_ctx_h

  @return status of operation
  @retval false success. srv_ctx_h was prepared.
  @retval true failure. OOM or invalid mysql_h.
*/
DECLARE_BOOL_METHOD(start, (SRV_CTX_H * srv_ctx_h, MYSQL_H *mysql_h));

/**
  Deallocates the memory allocated for data handle in start api.

  @param srv_ctx_h dom data handle(Dom_ctx type), which needs to be
                   deallocated.

  @return status of operation
  @retval void
*/
DECLARE_METHOD(void, end, (SRV_CTX_H srv_ctx_h));

END_SERVICE_DEFINITION(mysql_text_consumer_factory_v1)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for start_result_metadata, field_metadata,
  and end_result_metadata.
*/
BEGIN_SERVICE_DEFINITION(mysql_text_consumer_metadata_v1)

/**
  Indicates beginning of metadata for the result set

  @param srv_ctx_h Dom_ctx data handle
  @param num_cols Number of fields being sent
  @param flags    Flags to alter the metadata sending
  @param collation_name Charset of the result set

  @return status of operation
  @retval false success. srv_ctx_h rows were prepared.
  @retval true failure. OOM or invalid srv_ctx_h.
*/
DECLARE_BOOL_METHOD(start_result_metadata,
                    (SRV_CTX_H srv_ctx_h, unsigned int num_cols,
                     unsigned int flags, const char *const collation_name));

/**
  Field metadata is provided to srv_ctx_h via this service api

  @param srv_ctx_h Dom_ctx data handle
  @param field   Field's metadata (see field.h)
  @param collation_name Field's charset

  @return status of operation
  @retval false success. srv_ctx_h field information prepared.
  @retval true failure. invalid srv_ctx_h.
*/
DECLARE_BOOL_METHOD(field_metadata,
                    (SRV_CTX_H srv_ctx_h, struct Field_metadata *field,
                     const char *const collation_name));

/**
  Indicates end of metadata for the result set

  @param srv_ctx_h Dom_ctx data handle.
  @param server_status server status.
  @param warn_count warning count of current stmt.

  @return status of operation
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(end_result_metadata,
                    (SRV_CTX_H srv_ctx_h, unsigned int server_status,
                     unsigned warn_count));

END_SERVICE_DEFINITION(mysql_text_consumer_metadata_v1)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for start_row, abort_row
  and end_row.
*/
BEGIN_SERVICE_DEFINITION(mysql_text_consumer_row_factory_v1)

/**
  Indicates the beginning of a new row in the result set/metadata

  @param srv_ctx_h Dom_ctx data handle.

  @return status of operation
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(start_row, (SRV_CTX_H srv_ctx_h));

/**
  An error occurred during execution

  @details This api indicates that an error occurred during command
  execution and the partial row should be dropped. Server will raise error
  and return.

  @param srv_ctx_h Dom_ctx data handle.

  @return status of operation
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(abort_row, (SRV_CTX_H srv_ctx_h));

/**
  Indicates the end of the current row in the result set/metadata

  @param srv_ctx_h Dom_ctx data handle.

  @return status of operation
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(end_row, (SRV_CTX_H srv_ctx_h));
END_SERVICE_DEFINITION(mysql_text_consumer_row_factory_v1)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for handle_ok, handle_error
  and error.
*/
BEGIN_SERVICE_DEFINITION(mysql_text_consumer_error_v1)

/**
  Command ended with success

  @param srv_ctx_h            Dom_ctx data handle.
  @param server_status        Status of server (see mysql_com.h,
  SERVER_STATUS_*)
  @param statement_warn_count Number of warnings thrown during execution
  @param affected_rows        Number of rows affected by the command
  @param last_insert_id       Last insert id being assigned during execution
  @param message              A message from server
*/
DECLARE_METHOD(void, handle_ok,
               (SRV_CTX_H srv_ctx_h, unsigned int server_status,
                unsigned int statement_warn_count,
                unsigned long long affected_rows,
                unsigned long long last_insert_id, const char *const message));

/**
  Command ended with ERROR, updating the error info into srv_ctx_h.

  @param srv_ctx_h Dom_ctx data handle.
  @param sql_errno Error code
  @param err_msg   Error message
  @param sqlstate  SQL state corresponding to the error code
*/
DECLARE_METHOD(void, handle_error,
               (SRV_CTX_H srv_ctx_h, unsigned int sql_errno,
                const char *const err_msg, const char *const sqlstate));
/**
  Getting the error info from srv_ctx_h.

  @param srv_ctx_h Dom_ctx data handle.
  @param err_num Error code
  @param error_msg   Error message

  @return status of operation
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(error, (SRV_CTX_H srv_ctx_h, unsigned int *err_num,
                            const char **error_msg));

END_SERVICE_DEFINITION(mysql_text_consumer_error_v1)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for get_null.
*/
BEGIN_SERVICE_DEFINITION(mysql_text_consumer_get_null_v1)

/**
  Receive nullptr value from server and store into srv_ctx_h data.

  @param srv_ctx_h Dom_ctx data handle.

  @return status of operation
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(get, (SRV_CTX_H srv_ctx_h));

END_SERVICE_DEFINITION(mysql_text_consumer_get_null_v1)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for get_integer.
*/
BEGIN_SERVICE_DEFINITION(mysql_text_consumer_get_integer_v1)

/**
  Get TINY/SHORT/LONG value from server and store into srv_ctx_h data.

  @param srv_ctx_h Dom_ctx data handle.
  @param value     Value received

  @return status of operation
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(get, (SRV_CTX_H srv_ctx_h, long long value));

END_SERVICE_DEFINITION(mysql_text_consumer_get_integer_v1)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for get_longlong.
*/
BEGIN_SERVICE_DEFINITION(mysql_text_consumer_get_longlong_v1)

/**
  Get LONGLONG value from server and store into srv_ctx_h data.

  @param srv_ctx_h Dom_ctx data handle.
  @param value     Value received
  @param unsigned_flag true <=> value is unsigned

  @return status of operation
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(get, (SRV_CTX_H srv_ctx_h, long long value,
                          unsigned int unsigned_flag));

END_SERVICE_DEFINITION(mysql_text_consumer_get_longlong_v1)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for get_decimal.
*/
BEGIN_SERVICE_DEFINITION(mysql_text_consumer_get_decimal_v1)

/**
  Get DECIMAL value from server and store into srv_ctx_h data.

  @param srv_ctx_h Dom_ctx data handle.
  @param value     Value received

  @return status of operation
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(get, (SRV_CTX_H srv_ctx_h, const DECIMAL_T_H value));

END_SERVICE_DEFINITION(mysql_text_consumer_get_decimal_v1)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for get_double.
  and error.
*/
BEGIN_SERVICE_DEFINITION(mysql_text_consumer_get_double_v1)

/**
  Get FLOAT/DOUBLE value from server and store into srv_ctx_h data.

  @param srv_ctx_h Dom_ctx data handle.
  @param value     Value received
  @param decimals  Number of decimals

  @return status of operation
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(get,
                    (SRV_CTX_H srv_ctx_h, double value, unsigned int decimals));

END_SERVICE_DEFINITION(mysql_text_consumer_get_double_v1)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for get_date, get_time
  and get_datatime.
*/
BEGIN_SERVICE_DEFINITION(mysql_text_consumer_get_date_time_v1)

/**
  Get DATE value from server and store into srv_ctx_h data.

  @param srv_ctx_h Dom_ctx data handle.
  @param value     Value received

  @return status of operation
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(get_date, (SRV_CTX_H srv_ctx_h, const MYSQL_TIME_H value));

/**
  Get TIME value from server and store into srv_ctx_h data.

  @param srv_ctx_h Dom_ctx data handle.
  @param value     Value received
  @param precision Number of decimals

  @return status of operation
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(get_time, (SRV_CTX_H srv_ctx_h, const MYSQL_TIME_H value,
                               unsigned int precision));

/**
  Get DATETIME value from server and store into srv_ctx_h data.

  @param srv_ctx_h Dom_ctx data handle.
  @param value     Value received
  @param precision Number of decimals

  @return status of operation
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(get_datatime,
                    (SRV_CTX_H srv_ctx_h, const MYSQL_TIME_H value,
                     unsigned int precision));

END_SERVICE_DEFINITION(mysql_text_consumer_get_date_time_v1)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for get_string.
*/
BEGIN_SERVICE_DEFINITION(mysql_text_consumer_get_string_v1)

/**
  Get STRING value from server and store into srv_ctx_h data.

  @param srv_ctx_h Dom_ctx data handle.
  @param value     Value received
  @param length    Value's length
  @param collation_name   Value's charset

  @return status of operation
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(get_string,
                    (SRV_CTX_H srv_ctx_h, const char *const value,
                     size_t length, const char *const collation_name));

END_SERVICE_DEFINITION(mysql_text_consumer_get_string_v1)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for client_capabilities.
*/
BEGIN_SERVICE_DEFINITION(mysql_text_consumer_client_capabilities_v1)

/*
  Stores server's capabilities into the OUT param.

  @param srv_ctx_h Dom_ctx data handle.
  @param[OUT] capabilities contains the server capabilities value.
*/
DECLARE_METHOD(void, client_capabilities,
               (SRV_CTX_H srv_ctx_h, unsigned long *capabilities));

END_SERVICE_DEFINITION(mysql_text_consumer_client_capabilities_v1)

#endif  // MYSQL_COMMAND_CONSUMER_H
