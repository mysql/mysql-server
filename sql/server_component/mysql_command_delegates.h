/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef MYSQL_COMMAND_DELEGATES_H
#define MYSQL_COMMAND_DELEGATES_H

#include <include/decimal.h>
#include <include/my_compiler.h>
#include <include/mysql.h>
#include <include/mysql/service_command.h>
#include <mysql/components/my_service.h>
#include <mysql/components/services/mysql_admin_session.h>
#include <mysql/components/services/mysql_command_consumer.h>
#include <mysql/strings/m_ctype.h>

class Command_delegate {
 public:
  Command_delegate(void *srv, SRV_CTX_H srv_ctx_h);
  virtual ~Command_delegate();

  Command_delegate(const Command_delegate &) = default;
  Command_delegate(Command_delegate &&) = default;
  Command_delegate &operator=(const Command_delegate &) = default;
  Command_delegate &operator=(Command_delegate &&) = default;

  const st_command_service_cbs *callbacks() const {
    static const st_command_service_cbs cbs = {
        &Command_delegate::call_start_result_metadata,
        &Command_delegate::call_field_metadata,
        &Command_delegate::call_end_result_metadata,
        &Command_delegate::call_start_row,
        &Command_delegate::call_end_row,
        &Command_delegate::call_abort_row,
        &Command_delegate::call_get_client_capabilities,
        &Command_delegate::call_get_null,
        &Command_delegate::call_get_integer,
        &Command_delegate::call_get_longlong,
        &Command_delegate::call_get_decimal,
        &Command_delegate::call_get_double,
        &Command_delegate::call_get_date,
        &Command_delegate::call_get_time,
        &Command_delegate::call_get_datetime,
        &Command_delegate::call_get_string,
        &Command_delegate::call_handle_ok,
        &Command_delegate::call_handle_error,
        &Command_delegate::call_shutdown,
        nullptr};
    return &cbs;
  }

 protected:
  void *m_srv;
  SRV_CTX_H m_srv_ctx_h;
  st_command_service_cbs m_callbacks;

 public:
  /*** Getting metadata ***/
  /*
    Indicates beginning of metadata for the result set

    @param num_cols Number of fields being sent
    @param flags    Flags to alter the metadata sending
    @param resultcs Charset of the result set

    @returns
    true  an error occurred, server will abort the command
    false ok
  */
  virtual int start_result_metadata(uint32_t num_cols, uint32_t flags,
                                    const CHARSET_INFO *resultcs) = 0;

  /*
    Field metadata is provided via this callback

    @param field   Field's metadata (see field.h)
    @param charset Field's charset

    @returns
    true  an error occurred, server will abort the command
    false ok
  */
  virtual int field_metadata(struct st_send_field *field,
                             const CHARSET_INFO *charset) = 0;

  /*
    Indicates end of metadata for the result set

    @returns
    true  an error occurred, server will abort the command
    false ok
  */
  virtual int end_result_metadata(uint server_status, uint warn_count) = 0;

  /*
    Indicates the beginning of a new row in the result set/metadata

    @returns
    true  an error occurred, server will abort the command
    false ok
  */
  virtual int start_row() = 0;

  /*
   Indicates the end of the current row in the result set/metadata

   @returns
   true  an error occurred, server will abort the command
   false ok
  */
  virtual int end_row() = 0;

  /*
    An error occurred during execution

    @details This callback indicates that an error occurreded during command
    execution and the partial row should be dropped. Server will raise error
    and return.

    @returns
    true  an error occurred, server will abort the command
    false ok
  */
  virtual void abort_row() = 0;

  /*
    Return client's capabilities (see mysql_com.h, CLIENT_*)

    @return Bitmap of client's capabilities
  */
  virtual ulong get_client_capabilities() = 0;

  /****** Getting data ******/
  /*
    Receive NULL value from server

    @returns
    true  an error occurred, server will abort the command
    false ok
  */
  virtual int get_null() = 0;

  /*
    Get TINY/SHORT/LONG value from server

    @param value         Value received

    @note In order to know which type exactly was received, the plugin must
    track the metadata that was sent just prior to the result set.

    @returns
    true  an error occurred, server will abort the command
    false ok
  */
  virtual int get_integer(longlong value) = 0;

  /*
    Get LONGLONG value from server

    @param value         Value received
    @param unsigned_flag true <=> value is unsigned

    @returns
    true  an error occurred, server will abort the command
    false ok
  */
  virtual int get_longlong(longlong value, uint32_t unsigned_flag) = 0;

  /*
    Receive DECIMAL value from server

    @param value Value received

    @returns
    true  an error occurred, server will abort the command
    false ok
  */
  virtual int get_decimal(const decimal_t *value) = 0;

  /*
    Get FLOAT/DOUBLE from server

    @param value    Value received
    @param decimals Number of decimals

    @note In order to know which type exactly was received, the plugin must
    track the metadata that was sent just prior to the result set.

    @returns
    true  an error occurred, server will abort the command
    false ok
  */
  virtual int get_double(double value, uint32 decimals) = 0;

  /*
    Get DATE value from server

    @param value    Value received

    @returns
    true  an error occurred during storing, server will abort the command
    false ok
  */
  virtual int get_date(const MYSQL_TIME *value) = 0;

  /*
    Get TIME value from server

    @param value    Value received
    @param decimals Number of decimals

    @returns
    true  an error occurred during storing, server will abort the command
    false ok
  */
  virtual int get_time(const MYSQL_TIME *value, uint decimals) = 0;

  /*
    Get DATETIME value from server

    @param value    Value received
    @param decimals Number of decimals

    @returns
    true  an error occurred during storing, server will abort the command
    false ok
  */
  virtual int get_datetime(const MYSQL_TIME *value, uint decimals) = 0;

  /*
    Get STRING value from server

    @param value   Value received
    @param length  Value's length
    @param valuecs Value's charset

    @returns
    true  an error occurred, server will abort the command
    false ok
  */
  virtual int get_string(const char *const value, size_t length,
                         const CHARSET_INFO *const valuecs) = 0;

  /****** Getting execution status ******/
  /*
    Command ended with success

    @param server_status        Status of server (see mysql_com.h,
    SERVER_STATUS_*)
    @param statement_warn_count Number of warnings thrown during execution
    @param affected_rows        Number of rows affected by the command
    @param last_insert_id       Last insert id being assigned during execution
    @param message              A message from server
  */
  virtual void handle_ok(unsigned int server_status,
                         unsigned int statement_warn_count,
                         unsigned long long affected_rows,
                         unsigned long long last_insert_id,
                         const char *const message) = 0;

  /*
    Command ended with ERROR

    @param sql_errno Error code
    @param err_msg   Error message
    @param sqlstate  SQL state correspongin to the error code
  */
  virtual void handle_error(uint sql_errno, const char *const err_msg,
                            const char *const sqlstate) = 0;
  /*
    Session was shutdown while command was running
  */
  virtual void shutdown(int flag [[maybe_unused]]) { return; }

 private:
  static int call_start_result_metadata(void *ctx, uint num_cols, uint flags,
                                        const CHARSET_INFO *resultcs) {
    assert(ctx);
    Command_delegate *self = static_cast<Command_delegate *>(ctx);
    return self->start_result_metadata(num_cols, flags, resultcs);
  }

  static int call_field_metadata(void *ctx, struct st_send_field *field,
                                 const CHARSET_INFO *charset) {
    assert(ctx);
    return static_cast<Command_delegate *>(ctx)->field_metadata(field, charset);
  }

  static int call_end_result_metadata(void *ctx, uint server_status,
                                      uint warn_count) {
    assert(ctx);
    Command_delegate *self = static_cast<Command_delegate *>(ctx);
    const int tmp = self->end_result_metadata(server_status, warn_count);
    return tmp;
  }

  static int call_start_row(void *ctx) {
    assert(ctx);
    Command_delegate *self = static_cast<Command_delegate *>(ctx);
    return self->start_row();
  }

  static int call_end_row(void *ctx) {
    assert(ctx);
    Command_delegate *self = static_cast<Command_delegate *>(ctx);
    return self->end_row();
  }

  static void call_abort_row(void *ctx) {
    assert(ctx);
    static_cast<Command_delegate *>(ctx)->abort_row();
  }

  static ulong call_get_client_capabilities(void *ctx) {
    assert(ctx);
    return static_cast<Command_delegate *>(ctx)->get_client_capabilities();
  }

  static int call_get_null(void *ctx) {
    assert(ctx);
    return static_cast<Command_delegate *>(ctx)->get_null();
  }

  static int call_get_integer(void *ctx, longlong value) {
    assert(ctx);
    return static_cast<Command_delegate *>(ctx)->get_integer(value);
  }

  static int call_get_longlong(void *ctx, longlong value,
                               unsigned int unsigned_flag) {
    assert(ctx);
    return static_cast<Command_delegate *>(ctx)->get_longlong(value,
                                                              unsigned_flag);
  }

  static int call_get_decimal(void *ctx, const decimal_t *value) {
    assert(ctx);
    return static_cast<Command_delegate *>(ctx)->get_decimal(value);
  }

  static int call_get_double(void *ctx, double value, unsigned int decimals) {
    assert(ctx);
    return static_cast<Command_delegate *>(ctx)->get_double(value, decimals);
  }

  static int call_get_date(void *ctx, const MYSQL_TIME *value) {
    assert(ctx);
    return static_cast<Command_delegate *>(ctx)->get_date(value);
  }

  static int call_get_time(void *ctx, const MYSQL_TIME *value,
                           unsigned int decimals) {
    assert(ctx);
    return static_cast<Command_delegate *>(ctx)->get_time(value, decimals);
  }

  static int call_get_datetime(void *ctx, const MYSQL_TIME *value,
                               unsigned int decimals) {
    assert(ctx);
    return static_cast<Command_delegate *>(ctx)->get_datetime(value, decimals);
  }

  static int call_get_string(void *ctx, const char *const value, size_t length,
                             const CHARSET_INFO *const valuecs) {
    assert(ctx);
    return static_cast<Command_delegate *>(ctx)->get_string(value, length,
                                                            valuecs);
  }

  static void call_handle_ok(void *ctx, uint server_status,
                             uint statement_warn_count, ulonglong affected_rows,
                             ulonglong last_insert_id,
                             const char *const message) {
    assert(ctx);
    auto context = static_cast<Command_delegate *>(ctx);
    context->handle_ok(server_status, statement_warn_count, affected_rows,
                       last_insert_id, message);
  }

  static void call_handle_error(void *ctx, uint sql_errno,
                                const char *const err_msg,
                                const char *const sqlstate) {
    assert(ctx);
    static_cast<Command_delegate *>(ctx)->handle_error(sql_errno, err_msg,
                                                       sqlstate);
  }

  static void call_shutdown(void *ctx, int flag) {
    assert(ctx);
    static_cast<Command_delegate *>(ctx)->shutdown(flag);
  }
};

class Callback_command_delegate : public Command_delegate {
 public:
  Callback_command_delegate(void *srv, SRV_CTX_H srv_ctx_h);

  int start_result_metadata(uint32_t num_cols, uint32_t flags,
                            const CHARSET_INFO *resultcs) override;

  int field_metadata(struct st_send_field *field,
                     const CHARSET_INFO *charset) override;

  int end_result_metadata(uint server_status, uint warn_count) override;

  enum cs_text_or_binary representation() const {
    return CS_BINARY_REPRESENTATION;
  }

  int start_row() override;

  int end_row() override;

  void abort_row() override;

  ulong get_client_capabilities() override;

  /****** Getting data ******/
  int get_null() override;

  int get_integer(longlong value) override;

  int get_longlong(longlong value, unsigned int unsigned_flag) override;

  int get_decimal(const decimal_t *value) override;

  int get_double(double value, unsigned int decimals) override;

  int get_date(const MYSQL_TIME *value) override;

  int get_time(const MYSQL_TIME *value, unsigned int decimals) override;

  int get_datetime(const MYSQL_TIME *value, unsigned int decimals) override;

  int get_string(const char *const value, size_t length,
                 const CHARSET_INFO *const valuecs) override;

  void handle_ok(unsigned int server_status, unsigned int statement_warn_count,
                 unsigned long long affected_rows,
                 unsigned long long last_insert_id,
                 const char *const message) override;

  void handle_error(uint sql_errno, const char *const err_msg,
                    const char *const sqlstate) override;
};

#endif  // MYSQL_COMMAND_DELEGATES_H
