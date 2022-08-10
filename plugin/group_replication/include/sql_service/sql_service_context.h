/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#ifndef SQL_SERVICE_CONTEXT_INCLUDE
#define SQL_SERVICE_CONTEXT_INCLUDE

#include <stddef.h>

#include "my_inttypes.h"
#include "plugin/group_replication/include/sql_service/sql_service_context_base.h"

class Sql_service_context : public Sql_service_context_base {
 public:
  Sql_service_context(Sql_resultset *rset) : resultset(rset) {
    if (rset != nullptr) resultset->clear();
  }

  ~Sql_service_context() override = default;

  /** Getting metadata **/
  /**
    Indicates start of metadata for the result set

    @param num_cols Number of fields being sent
    @param flags    Flags to alter the metadata sending
    @param resultcs Charset of the result set

    @retval 1  Error
    @retval 0  OK
  */
  int start_result_metadata(uint num_cols, uint flags,
                            const CHARSET_INFO *resultcs) override;

  /**
    Field metadata is provided via this callback

    @param field   Field's metadata (see field.h)
    @param charset Field's charset

    @retval 1  Error
    @retval 0  OK
  */
  int field_metadata(struct st_send_field *field,
                     const CHARSET_INFO *charset) override;

  /**
    Indicates end of metadata for the result set

    @param server_status   Status of server (see mysql_com.h SERVER_STATUS_*)
    @param warn_count      Number of warnings thrown during execution

    @retval 1  Error
    @retval 0  OK
  */
  int end_result_metadata(uint server_status, uint warn_count) override;

  /**
    Indicates the beginning of a new row in the result set/metadata

    @retval 1  Error
    @retval 0  OK
  */
  int start_row() override;

  /**
    Indicates end of the row in the result set/metadata

    @retval 1  Error
    @retval 0  OK
  */
  int end_row() override;

  /**
    An error occurred during execution

    @details This callback indicates that an error occurreded during command
    execution and the partial row should be dropped. Server will raise error
    and return.
  */
  void abort_row() override;

  /**
    Return client's capabilities (see mysql_com.h, CLIENT_*)

    @return Bitmap of client's capabilities
  */
  ulong get_client_capabilities() override;

  /** Getting data **/
  /**
    Receive NULL value from server

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  int get_null() override;

  /**
    Get TINY/SHORT/LONG value from server

    @param value         Value received

    @note In order to know which type exactly was received, the plugin must
    track the metadata that was sent just prior to the result set.

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  int get_integer(longlong value) override;

  /**
    Get LONGLONG value from server

    @param value         Value received
    @param is_unsigned   TRUE <=> value is unsigned

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  int get_longlong(longlong value, uint is_unsigned) override;

  /**
    Receive DECIMAL value from server

    @param value Value received

    @return status
      @retval 1  Error
      @retval 0  OK
   */
  int get_decimal(const decimal_t *value) override;

  /**
    Receive DOUBLE value from server

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  int get_double(double value, uint32 decimals) override;

  /**
    Get DATE value from server

    @param value    Value received

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  int get_date(const MYSQL_TIME *value) override;

  /**
    Get TIME value from server

    @param value    Value received
    @param decimals Number of decimals

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  int get_time(const MYSQL_TIME *value, uint decimals) override;

  /**
    Get DATETIME value from server

    @param value    Value received
    @param decimals Number of decimals

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  int get_datetime(const MYSQL_TIME *value, uint decimals) override;

  /**
    Get STRING value from server

    @param value   Value received
    @param length  Value's length
    @param valuecs Value's charset

    @return status
      @retval 1  Error
      @retval 0  OK
  */
  int get_string(const char *const value, size_t length,
                 const CHARSET_INFO *const valuecs) override;

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
  void handle_ok(uint server_status, uint statement_warn_count,
                 ulonglong affected_rows, ulonglong last_insert_id,
                 const char *const message) override;

  /**
    Command ended with ERROR

    @param sql_errno Error code
    @param err_msg   Error message
    @param sqlstate  SQL state correspongin to the error code
  */
  void handle_error(uint sql_errno, const char *const err_msg,
                    const char *const sqlstate) override;

  /**
    Session was shutdown while command was running
  */
  void shutdown(int flag) override;

  /**
     Check if the connection is still alive.
  */
  bool connection_alive() override { return true; }

 private:
  /* executed command result store */
  Sql_resultset *resultset;
};

#endif  // SQL_SERVICE_CONTEXT_INCLUDE
