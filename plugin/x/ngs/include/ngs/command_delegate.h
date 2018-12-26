/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_COMMAND_DELEGATE_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_COMMAND_DELEGATE_H_

#include "decimal.h"
#include "m_ctype.h"
#include "my_compiler.h"
#include "mysql/service_command.h"
#include "plugin/x/ngs/include/ngs/protocol_encoder.h"

namespace ngs {
class Command_delegate {
 public:
  struct Info {
    uint64_t affected_rows{0};
    uint64_t last_insert_id{0};
    uint32_t num_warnings{0};
    std::string message;
    uint32_t server_status{0};
  };

  struct Field_type {
    enum_field_types type;
    unsigned int flags;
  };
  typedef std::vector<Field_type> Field_types;

  Command_delegate() { reset(); }
  virtual ~Command_delegate() {}

  ngs::Error_code get_error() const {
    if (m_sql_errno == 0)
      return ngs::Error_code();
    else
      return ngs::Error_code(m_sql_errno, m_err_msg, m_sqlstate);
  }

  const Info &get_info() const { return m_info; }
  void set_field_types(const Field_types &field_types) {
    m_field_types = field_types;
  }
  const Field_types &get_field_types() const { return m_field_types; }

  bool killed() const { return m_killed; }
  bool got_eof() const { return m_got_eof; }

  virtual void reset() {
    m_info = Info();
    m_sql_errno = 0;
    m_killed = false;
    m_streaming_metadata = false;
    m_field_types.clear();
    m_got_eof = false;
  }

  const st_command_service_cbs *callbacks() const {
    static st_command_service_cbs cbs = {
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
        &Command_delegate::call_shutdown};
    return &cbs;
  }

  virtual enum cs_text_or_binary representation() const = 0;

 protected:
  Info m_info;
  Field_types m_field_types;
  uint m_sql_errno;
  std::string m_err_msg;
  std::string m_sqlstate;

  st_command_service_cbs m_callbacks;

  bool m_killed;
  bool m_streaming_metadata;
  bool m_got_eof;

 public:
  /*** Getting metadata ***/
  /*
    Indicates beginning of metadata for the result set

    @param num_cols Number of fields being sent
    @param flags    Flags to alter the metadata sending
    @param resultcs Charset of the result set

    @returns
    true  an error occured, server will abort the command
    false ok
  */
  virtual int start_result_metadata(
      uint num_cols MY_ATTRIBUTE((unused)), uint flags MY_ATTRIBUTE((unused)),
      const CHARSET_INFO *resultcs MY_ATTRIBUTE((unused))) {
    m_field_types.clear();
    return false;
  }

  /*
    Field metadata is provided via this callback

    @param field   Field's metadata (see field.h)
    @param charset Field's charset

    @returns
    true  an error occured, server will abort the command
    false ok
  */
  virtual int field_metadata(struct st_send_field *field,
                             const CHARSET_INFO *charset
                                 MY_ATTRIBUTE((unused))) {
    Field_type type = {field->type, field->flags};
    m_field_types.push_back(type);

    return false;
  }

  /*
    Indicates end of metadata for the result set

    @returns
    true  an error occured, server will abort the command
    false ok
  */
  virtual int end_result_metadata(uint server_status MY_ATTRIBUTE((unused)),
                                  uint warn_count MY_ATTRIBUTE((unused))) {
    return false;
  }

  /*
    Indicates the beginning of a new row in the result set/metadata

    @returns
    true  an error occured, server will abort the command
    false ok
  */
  virtual int start_row() { return false; }

  /*
   Indicates the end of the current row in the result set/metadata

   @returns
   true  an error occured, server will abort the command
   false ok
  */
  virtual int end_row() { return false; }

  /*
    An error occured during execution

    @details This callback indicates that an error occureded during command
    execution and the partial row should be dropped. Server will raise error
    and return.

    @returns
    true  an error occured, server will abort the command
    false ok
  */
  virtual void abort_row() {}

  /*
    Return client's capabilities (see mysql_com.h, CLIENT_*)

    @return Bitmap of client's capabilities
  */
  virtual ulong get_client_capabilities() { return 0; }

  /****** Getting data ******/
  /*
    Receive NULL value from server

    @returns
    true  an error occured, server will abort the command
    false ok
  */
  virtual int get_null() { return false; }

  /*
    Get TINY/SHORT/LONG value from server

    @param value         Value received

    @note In order to know which type exactly was received, the plugin must
    track the metadata that was sent just prior to the result set.

    @returns
    true  an error occured, server will abort the command
    false ok
  */
  virtual int get_integer(longlong value MY_ATTRIBUTE((unused))) {
    return false;
  }

  /*
    Get LONGLONG value from server

    @param value         Value received
    @param unsigned_flag true <=> value is unsigned

    @returns
    true  an error occured, server will abort the command
    false ok
  */
  virtual int get_longlong(longlong value MY_ATTRIBUTE((unused)),
                           uint unsigned_flag MY_ATTRIBUTE((unused))) {
    return false;
  }

  /*
    Receive DECIMAL value from server

    @param value Value received

    @returns
    true  an error occured, server will abort the command
    false ok
  */
  virtual int get_decimal(const decimal_t *value MY_ATTRIBUTE((unused))) {
    return false;
  }

  /*
    Get FLOAT/DOUBLE from server

    @param value    Value received
    @param decimals Number of decimals

    @note In order to know which type exactly was received, the plugin must
    track the metadata that was sent just prior to the result set.

    @returns
    true  an error occured, server will abort the command
    false ok
  */
  virtual int get_double(double value MY_ATTRIBUTE((unused)),
                         uint32 decimals MY_ATTRIBUTE((unused))) {
    return false;
  }

  /*
    Get DATE value from server

    @param value    Value received

    @returns
    true  an error occured during storing, server will abort the command
    false ok
  */
  virtual int get_date(const MYSQL_TIME *value MY_ATTRIBUTE((unused))) {
    return false;
  }

  /*
    Get TIME value from server

    @param value    Value received
    @param decimals Number of decimals

    @returns
    true  an error occured during storing, server will abort the command
    false ok
  */
  virtual int get_time(const MYSQL_TIME *value MY_ATTRIBUTE((unused)),
                       uint decimals MY_ATTRIBUTE((unused))) {
    return false;
  }

  /*
    Get DATETIME value from server

    @param value    Value received
    @param decimals Number of decimals

    @returns
    true  an error occured during storing, server will abort the command
    false ok
  */
  virtual int get_datetime(const MYSQL_TIME *value MY_ATTRIBUTE((unused)),
                           uint decimals MY_ATTRIBUTE((unused))) {
    return false;
  }

  /*
    Get STRING value from server

    @param value   Value received
    @param length  Value's length
    @param valuecs Value's charset

    @returns
    true  an error occured, server will abort the command
    false ok
  */
  virtual int get_string(const char *const value MY_ATTRIBUTE((unused)),
                         size_t length MY_ATTRIBUTE((unused)),
                         const CHARSET_INFO *const valuecs
                             MY_ATTRIBUTE((unused))) {
    return false;
  }

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
  virtual void handle_ok(uint server_status, uint statement_warn_count,
                         ulonglong affected_rows, ulonglong last_insert_id,
                         const char *const message) {
    m_info.server_status = server_status;
    m_info.num_warnings = statement_warn_count;
    m_info.affected_rows = affected_rows;
    m_info.last_insert_id = last_insert_id;
    m_info.message = message ? message : "";
  }

  /*
   Command ended with ERROR

   @param sql_errno Error code
   @param err_msg   Error message
   @param sqlstate  SQL state correspongin to the error code
  */
  virtual void handle_error(uint sql_errno, const char *const err_msg,
                            const char *const sqlstate) {
    m_sql_errno = sql_errno;
    m_err_msg = err_msg ? err_msg : "";
    m_sqlstate = sqlstate ? sqlstate : "";
  }

  /*
   Session was shutdown while command was running

  */
  virtual void shutdown(int flag MY_ATTRIBUTE((unused))) { m_killed = true; }

 private:
  static int call_start_result_metadata(void *ctx, uint num_cols, uint flags,
                                        const CHARSET_INFO *resultcs) {
    Command_delegate *self = static_cast<Command_delegate *>(ctx);
    self->m_streaming_metadata = true;
    return self->start_result_metadata(num_cols, flags, resultcs);
  }

  static int call_field_metadata(void *ctx, struct st_send_field *field,
                                 const CHARSET_INFO *charset) {
    return static_cast<Command_delegate *>(ctx)->field_metadata(field, charset);
  }

  static int call_end_result_metadata(void *ctx, uint server_status,
                                      uint warn_count) {
    Command_delegate *self = static_cast<Command_delegate *>(ctx);
    int tmp = self->end_result_metadata(server_status, warn_count);
    self->m_streaming_metadata = false;
    return tmp;
  }

  static int call_start_row(void *ctx) {
    Command_delegate *self = static_cast<Command_delegate *>(ctx);
    if (self->m_streaming_metadata) return false;
    return self->start_row();
  }

  static int call_end_row(void *ctx) {
    Command_delegate *self = static_cast<Command_delegate *>(ctx);
    if (self->m_streaming_metadata) return false;
    return self->end_row();
  }

  static void call_abort_row(void *ctx) {
    static_cast<Command_delegate *>(ctx)->abort_row();
  }

  static ulong call_get_client_capabilities(void *ctx) {
    return static_cast<Command_delegate *>(ctx)->get_client_capabilities();
  }

  static int call_get_null(void *ctx) {
    return static_cast<Command_delegate *>(ctx)->get_null();
  }

  static int call_get_integer(void *ctx, longlong value) {
    return static_cast<Command_delegate *>(ctx)->get_integer(value);
  }

  static int call_get_longlong(void *ctx, longlong value, uint unsigned_flag) {
    return static_cast<Command_delegate *>(ctx)->get_longlong(value,
                                                              unsigned_flag);
  }

  static int call_get_decimal(void *ctx, const decimal_t *value) {
    return static_cast<Command_delegate *>(ctx)->get_decimal(value);
  }

  static int call_get_double(void *ctx, double value, uint32 decimals) {
    return static_cast<Command_delegate *>(ctx)->get_double(value, decimals);
  }

  static int call_get_date(void *ctx, const MYSQL_TIME *value) {
    return static_cast<Command_delegate *>(ctx)->get_date(value);
  }

  static int call_get_time(void *ctx, const MYSQL_TIME *value, uint decimals) {
    return static_cast<Command_delegate *>(ctx)->get_time(value, decimals);
  }

  static int call_get_datetime(void *ctx, const MYSQL_TIME *value,
                               uint decimals) {
    return static_cast<Command_delegate *>(ctx)->get_datetime(value, decimals);
  }

  static int call_get_string(void *ctx, const char *const value, size_t length,
                             const CHARSET_INFO *const valuecs) {
    return static_cast<Command_delegate *>(ctx)->get_string(value, length,
                                                            valuecs);
  }

  static void call_handle_ok(void *ctx, uint server_status,
                             uint statement_warn_count, ulonglong affected_rows,
                             ulonglong last_insert_id,
                             const char *const message) {
    static_cast<Command_delegate *>(ctx)->m_got_eof = (message == NULL);

    static_cast<Command_delegate *>(ctx)->handle_ok(
        server_status, statement_warn_count, affected_rows, last_insert_id,
        message);
  }

  static void call_handle_error(void *ctx, uint sql_errno,
                                const char *const err_msg,
                                const char *const sqlstate) {
    static_cast<Command_delegate *>(ctx)->handle_error(sql_errno, err_msg,
                                                       sqlstate);
  }

  static void call_shutdown(void *ctx, int flag) {
    static_cast<Command_delegate *>(ctx)->shutdown(flag);
  }
};
}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_COMMAND_DELEGATE_H_
