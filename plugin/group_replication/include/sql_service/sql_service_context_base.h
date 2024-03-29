/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#ifndef SQL_SERVICE_CONTEXT_BASE_INCLUDE
#define SQL_SERVICE_CONTEXT_BASE_INCLUDE

#include <mysql/plugin.h>
#include <mysql/service_command.h>

#include "my_inttypes.h"
#include "plugin/group_replication/include/sql_service/sql_resultset.h"

class Sql_service_context_base {
 public:
  /** The sql service callbacks that will call the below virtual methods*/
  static const st_command_service_cbs sql_service_callbacks;

  /**
    Sql_service_context_base constructor
    resets all variables
  */
  Sql_service_context_base() = default;

  virtual ~Sql_service_context_base() = default;

  /** Getting metadata **/
  /**
    Indicates start of metadata for the result set

    @param num_cols Number of fields being sent
    @param flags    Flags to alter the metadata sending
    @param resultcs Charset of the result set

    @retval 1  Error
    @retval 0  OK
  */
  virtual int start_result_metadata(uint num_cols, uint flags,
                                    const CHARSET_INFO *resultcs) = 0;

  /**
    Field metadata is provided via this callback

    @param field   Field's metadata (see field.h)
    @param charset Field's charset

    @retval 1  Error
    @retval 0  OK
  */
  virtual int field_metadata(struct st_send_field *field,
                             const CHARSET_INFO *charset) = 0;

  /**
    Indicates end of metadata for the result set

    @param server_status   Status of server (see mysql_com.h SERVER_STATUS_*)
    @param warn_count      Number of warnings thrown during execution

    @retval 1  Error
    @retval 0  OK
  */
  virtual int end_result_metadata(uint server_status, uint warn_count) = 0;

  /**
    Indicates the beginning of a new row in the result set/metadata

    @retval 1  Error
    @retval 0  OK
  */
  virtual int start_row() = 0;

  /**
    Indicates end of the row in the result set/metadata

    @retval 1  Error
    @retval 0  OK
  */
  virtual int end_row() = 0;

  /**
    An error occurred during execution

    @details This callback indicates that an error occurreded during command
    execution and the partial row should be dropped. Server will raise error
    and return.
  */
  virtual void abort_row() = 0;

  /**
    Return client's capabilities (see mysql_com.h, CLIENT_*)

    @return Bitmap of client's capabilities
  */
  virtual ulong get_client_capabilities() = 0;

  /** Getting data **/
  /**
    Receive NULL value from server

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  virtual int get_null() = 0;

  /**
    Get TINY/SHORT/LONG value from server

    @param value         Value received

    @note In order to know which type exactly was received, the plugin must
    track the metadata that was sent just prior to the result set.

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  virtual int get_integer(longlong value) = 0;

  /**
    Get LONGLONG value from server

    @param value         Value received
    @param is_unsigned   TRUE <=> value is unsigned

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  virtual int get_longlong(longlong value, uint is_unsigned) = 0;

  /**
    Receive DECIMAL value from server

    @param value Value received

    @return status
      @retval 1  Error
      @retval 0  OK
   */
  virtual int get_decimal(const decimal_t *value) = 0;

  /**

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  virtual int get_double(double value, uint32 decimals) = 0;

  /**
    Get DATE value from server

    @param value    Value received

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  virtual int get_date(const MYSQL_TIME *value) = 0;

  /**
    Get TIME value from server

    @param value    Value received
    @param decimals Number of decimals

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  virtual int get_time(const MYSQL_TIME *value, uint decimals) = 0;

  /**
    Get DATETIME value from server

    @param value    Value received
    @param decimals Number of decimals

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  virtual int get_datetime(const MYSQL_TIME *value, uint decimals) = 0;

  /**
    Get STRING value from server

    @param value   Value received
    @param length  Value's length
    @param valuecs Value's charset

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  virtual int get_string(const char *const value, size_t length,
                         const CHARSET_INFO *const valuecs) = 0;

  /** Getting execution status **/
  /**
    Command ended with success

    @param server_status        Status of server (see mysql_com.h,
                                SERVER_STATUS_*)
    @param statement_warn_count Number of warnings thrown during execution
    @param affected_rows        Number of rows affected by the command
    @param last_insert_id       Last insert id being assigned during execution
    @param message              A message from server
  */
  virtual void handle_ok(uint server_status, uint statement_warn_count,
                         ulonglong affected_rows, ulonglong last_insert_id,
                         const char *const message) = 0;

  /**
    Command ended with ERROR

    @param sql_errno Error code
    @param err_msg   Error message
    @param sqlstate  SQL state corresponding to the error code
  */
  virtual void handle_error(uint sql_errno, const char *const err_msg,
                            const char *const sqlstate) = 0;

  /**
   Session was shutdown while command was running
  */
  virtual void shutdown(int flag) = 0;

  /**
   Check if the connection is still alive.
   It should always return true unless the protocol closed the connection.
  */
  virtual bool connection_alive() = 0;

 private:
  static int sql_start_result_metadata(void *ctx, uint num_cols, uint flags,
                                       const CHARSET_INFO *resultcs) {
    return ((Sql_service_context_base *)ctx)
        ->start_result_metadata(num_cols, flags, resultcs);
  }

  static int sql_field_metadata(void *ctx, struct st_send_field *field,
                                const CHARSET_INFO *charset) {
    return ((Sql_service_context_base *)ctx)->field_metadata(field, charset);
  }

  static int sql_end_result_metadata(void *ctx, uint server_status,
                                     uint warn_count) {
    return ((Sql_service_context_base *)ctx)
        ->end_result_metadata(server_status, warn_count);
  }

  static int sql_start_row(void *ctx) {
    return ((Sql_service_context_base *)ctx)->start_row();
  }

  static int sql_end_row(void *ctx) {
    return ((Sql_service_context_base *)ctx)->end_row();
  }

  static void sql_abort_row(void *ctx) {
    return ((Sql_service_context_base *)ctx)
        ->abort_row(); /* purecov: inspected */
  }

  static ulong sql_get_client_capabilities(void *ctx) {
    return ((Sql_service_context_base *)ctx)->get_client_capabilities();
  }

  static int sql_get_null(void *ctx) {
    return ((Sql_service_context_base *)ctx)->get_null();
  }

  static int sql_get_integer(void *ctx, longlong value) {
    return ((Sql_service_context_base *)ctx)->get_integer(value);
  }

  static int sql_get_longlong(void *ctx, longlong value, uint is_unsigned) {
    return ((Sql_service_context_base *)ctx)->get_longlong(value, is_unsigned);
  }

  static int sql_get_decimal(void *ctx, const decimal_t *value) {
    return ((Sql_service_context_base *)ctx)->get_decimal(value);
  }

  static int sql_get_double(void *ctx, double value, uint32 decimals) {
    return ((Sql_service_context_base *)ctx)->get_double(value, decimals);
  }

  static int sql_get_date(void *ctx, const MYSQL_TIME *value) {
    return ((Sql_service_context_base *)ctx)->get_date(value);
  }

  static int sql_get_time(void *ctx, const MYSQL_TIME *value, uint decimals) {
    return ((Sql_service_context_base *)ctx)->get_time(value, decimals);
  }

  static int sql_get_datetime(void *ctx, const MYSQL_TIME *value,
                              uint decimals) {
    return ((Sql_service_context_base *)ctx)->get_datetime(value, decimals);
  }

  static int sql_get_string(void *ctx, const char *const value, size_t length,
                            const CHARSET_INFO *const valuecs) {
    return ((Sql_service_context_base *)ctx)
        ->get_string(value, length, valuecs);
  }

  static void sql_handle_ok(void *ctx, uint server_status,
                            uint statement_warn_count, ulonglong affected_rows,
                            ulonglong last_insert_id,
                            const char *const message) {
    return ((Sql_service_context_base *)ctx)
        ->handle_ok(server_status, statement_warn_count, affected_rows,
                    last_insert_id, message);
  }

  static void sql_handle_error(void *ctx, uint sql_errno,
                               const char *const err_msg,
                               const char *const sqlstate) {
    return ((Sql_service_context_base *)ctx)
        ->handle_error(sql_errno, err_msg, sqlstate);
  }

  static void sql_shutdown(void *ctx, int flag) {
    return ((Sql_service_context_base *)ctx)->shutdown(flag);
  }

  static bool sql_connection_alive(void *ctx) {
    return ((Sql_service_context_base *)ctx)->connection_alive();
  }
};

#endif  // SQL_SERVICE_CONTEXT_BASE_INCLUDE
