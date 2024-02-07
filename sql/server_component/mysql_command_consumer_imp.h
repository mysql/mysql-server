/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_COMMAND_CONSUMER_IMP_H
#define MYSQL_COMMAND_CONSUMER_IMP_H

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_command_consumer.h>
#include "include/sql_common.h"

struct Dom_ctx {
  MYSQL *m_mysql = nullptr;
  MYSQL_DATA **m_result = nullptr;
  MYSQL_DATA *m_data = nullptr;
  MYSQL_FIELD *m_fields = nullptr;
  unsigned int m_field_column_count = 0;
  unsigned int m_cur_field_num = 0;
  uint32_t m_sql_errno = 0;
  MYSQL_ROWS *m_cur_row = nullptr;
  MYSQL_ROWS **m_prev_ptr = nullptr;
  std::string *m_message = nullptr;
  std::string *m_err_msg = nullptr;
  std::string *m_sqlstate = nullptr;
};

/*
 The default consumer works like document object
 model (DOM) parser i.e. first it fetches the
 entire result set from server and stores that in local buffer.
 Consumer then extracts the rows from the local buffer.
 Therefore, this consumer is not suitable if result set is huge.
 */
class mysql_command_consumer_dom_imp {
 public:
  static DEFINE_BOOL_METHOD(start, (SRV_CTX_H * srv_ctx_h, MYSQL_H *mysql_h));

  static DEFINE_BOOL_METHOD(start_result_metadata,
                            (SRV_CTX_H srv_ctx_h, unsigned int num_cols,
                             unsigned int flags,
                             const char *const collation_name));
  static DEFINE_BOOL_METHOD(field_metadata,
                            (SRV_CTX_H srv_ctx_h, struct Field_metadata *field,
                             const char *const collation_name));
  static DEFINE_BOOL_METHOD(end_result_metadata,
                            (SRV_CTX_H srv_ctx_h, unsigned int server_status,
                             unsigned int warn_count));

  static DEFINE_BOOL_METHOD(start_row, (SRV_CTX_H srv_ctx_h));
  static DEFINE_BOOL_METHOD(abort_row, (SRV_CTX_H srv_ctx_h));
  static DEFINE_BOOL_METHOD(end_row, (SRV_CTX_H srv_ctx_h));

  static DEFINE_METHOD(void, handle_ok,
                       (SRV_CTX_H srv_ctx_h, unsigned int server_status,
                        unsigned int statement_warn_count,
                        unsigned long long affected_rows,
                        unsigned long long last_insert_id,
                        const char *const message));
  static DEFINE_METHOD(void, handle_error,
                       (SRV_CTX_H srv_ctx_h, uint sql_errno,
                        const char *const err_msg, const char *const sqlstate));

  /* get_null */
  static DEFINE_BOOL_METHOD(get, (SRV_CTX_H));
  /* get_integer */
  static DEFINE_BOOL_METHOD(get, (SRV_CTX_H, long long));
  /* get_longlong */
  static DEFINE_BOOL_METHOD(get, (SRV_CTX_H, long long, unsigned int));
  /* get_decimal */
  static DEFINE_BOOL_METHOD(get, (SRV_CTX_H, const DECIMAL_T_H));
  /* get_double */
  static DEFINE_BOOL_METHOD(get, (SRV_CTX_H, double, unsigned int decimals));
  static DEFINE_BOOL_METHOD(get_date, (SRV_CTX_H, const MYSQL_TIME_H value));
  static DEFINE_BOOL_METHOD(get_time, (SRV_CTX_H, const MYSQL_TIME_H value,
                                       unsigned int precision));
  static DEFINE_BOOL_METHOD(get_datetime, (SRV_CTX_H, const MYSQL_TIME_H value,
                                           unsigned int decimals));
  static DEFINE_BOOL_METHOD(get_string,
                            (SRV_CTX_H, const char *const value, size_t length,
                             const char *const collation_name));
  static DEFINE_METHOD(void, client_capabilities,
                       (SRV_CTX_H, unsigned long *capabilities));

  static DEFINE_BOOL_METHOD(error, (SRV_CTX_H, uint32_t *err_num,
                                    const char **error_msg));

  static DEFINE_METHOD(void, end, (SRV_CTX_H srv_ctx_h));

 private:
  static DEFINE_BOOL_METHOD(store_data,
                            (SRV_CTX_H, const char *data, size_t length));
};

#endif  // MYSQL_COMMAND_CONSUMER_IMP_H
