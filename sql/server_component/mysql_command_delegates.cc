/* Copyright (c) 2022, Oracle and/or its affiliates.
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

#include "mysql_command_delegates.h"
#include "include/my_byteorder.h"
#include "include/my_sys.h"
#include "include/my_time.h"
#include "include/mysqld_error.h"
#include "include/sql_common.h"
#include "mysql_command_services_imp.h"
#include "sql/my_decimal.h"

Command_delegate::Command_delegate(void *srv, SRV_CTX_H srv_ctx_h)
    : m_srv(srv), m_srv_ctx_h(srv_ctx_h), m_callbacks() {
  assert(srv != nullptr);
  assert(m_srv_ctx_h != nullptr);
}

Command_delegate::~Command_delegate() {}

Callback_command_delegate::Callback_command_delegate(void *srv,
                                                     SRV_CTX_H srv_ctx_h)
    : Command_delegate(srv, srv_ctx_h) {}

int Callback_command_delegate::start_result_metadata(
    uint32_t num_cols, uint32_t flags, const CHARSET_INFO *resultcs) {
  return ((class mysql_command_consumer_refs *)(m_srv))
      ->metadata_srv->start_result_metadata(m_srv_ctx_h, num_cols, flags,
                                            resultcs->csname);
}

int Callback_command_delegate::field_metadata(struct st_send_field *field,
                                              const CHARSET_INFO *charset) {
  assert(field);
  Field_metadata fm{field->db_name,        field->table_name,
                    field->org_table_name, field->col_name,
                    field->org_col_name,   field->length,
                    field->charsetnr,      field->flags,
                    field->decimals,       field->type};
  return ((class mysql_command_consumer_refs *)(m_srv))
      ->metadata_srv->field_metadata(m_srv_ctx_h, &fm, charset->csname);
}

int Callback_command_delegate::end_result_metadata(uint server_status,
                                                   uint warn_count) {
  return ((class mysql_command_consumer_refs *)(m_srv))
      ->metadata_srv->end_result_metadata(m_srv_ctx_h, server_status,
                                          warn_count);
}

int Callback_command_delegate::start_row() {
  return ((class mysql_command_consumer_refs *)(m_srv))
      ->row_factory_srv->start_row(m_srv_ctx_h);
}

int Callback_command_delegate::end_row() {
  return ((class mysql_command_consumer_refs *)(m_srv))
      ->row_factory_srv->end_row(m_srv_ctx_h);
}

void Callback_command_delegate::abort_row() {
  ((class mysql_command_consumer_refs *)(m_srv))
      ->row_factory_srv->abort_row(m_srv_ctx_h);
}

ulong Callback_command_delegate::get_client_capabilities() {
  unsigned long capabilities;
  ((class mysql_command_consumer_refs *)(m_srv))
      ->client_capabilities_srv->client_capabilities(m_srv_ctx_h,
                                                     &capabilities);
  return capabilities;
}

/****** Getting data ******/
int Callback_command_delegate::get_null() {
  return ((class mysql_command_consumer_refs *)(m_srv))
      ->get_null_srv->get(m_srv_ctx_h);
}

int Callback_command_delegate::get_integer(longlong value) {
  return ((class mysql_command_consumer_refs *)(m_srv))
      ->get_integer_srv->get(m_srv_ctx_h, value);
}

int Callback_command_delegate::get_longlong(longlong value,
                                            unsigned int unsigned_flag) {
  return ((class mysql_command_consumer_refs *)(m_srv))
      ->get_longlong_srv->get(m_srv_ctx_h, value, unsigned_flag);
}

int Callback_command_delegate::get_decimal(const decimal_t *value) {
  decimal_t *decimal = const_cast<decimal_t *>(value);
  return ((class mysql_command_consumer_refs *)(m_srv))
      ->get_decimal_srv->get(m_srv_ctx_h,
                             reinterpret_cast<DECIMAL_T_H>(decimal));
}

int Callback_command_delegate::get_double(double value, unsigned int decimals) {
  return ((class mysql_command_consumer_refs *)(m_srv))
      ->get_double_srv->get(m_srv_ctx_h, value, decimals);
}

int Callback_command_delegate::get_date(const MYSQL_TIME *value) {
  MYSQL_TIME *date = const_cast<MYSQL_TIME *>(value);
  return ((class mysql_command_consumer_refs *)(m_srv))
      ->get_date_time_srv->get_date(m_srv_ctx_h,
                                    reinterpret_cast<MYSQL_TIME_H>(date));
}

int Callback_command_delegate::get_time(const MYSQL_TIME *value,
                                        unsigned int precision) {
  MYSQL_TIME *time = const_cast<MYSQL_TIME *>(value);
  return ((class mysql_command_consumer_refs *)(m_srv))
      ->get_date_time_srv->get_time(
          m_srv_ctx_h, reinterpret_cast<MYSQL_TIME_H>(time), precision);
}

int Callback_command_delegate::get_datetime(const MYSQL_TIME *value,
                                            unsigned int precision) {
  MYSQL_TIME *date_time = const_cast<MYSQL_TIME *>(value);
  return ((class mysql_command_consumer_refs *)(m_srv))
      ->get_date_time_srv->get_datatime(
          m_srv_ctx_h, reinterpret_cast<MYSQL_TIME_H>(date_time), precision);
}

int Callback_command_delegate::get_string(const char *const value,
                                          size_t length,
                                          const CHARSET_INFO *const valuecs) {
  return ((class mysql_command_consumer_refs *)(m_srv))
      ->get_string_srv->get_string(m_srv_ctx_h, value, length, valuecs->csname);
}

void Callback_command_delegate::handle_ok(unsigned int server_status,
                                          unsigned int statement_warn_count,
                                          unsigned long long affected_rows,
                                          unsigned long long last_insert_id,
                                          const char *const message) {
  ((class mysql_command_consumer_refs *)(m_srv))
      ->error_srv->handle_ok(m_srv_ctx_h, server_status, statement_warn_count,
                             affected_rows, last_insert_id, message);
}

/*
 Command ended with ERROR

 @param sql_errno Error code
 @param err_msg   Error message
 @param sqlstate  SQL state corresponding to the error code
*/
void Callback_command_delegate::handle_error(uint sql_errno,
                                             const char *const err_msg,
                                             const char *const sqlstate) {
  ((class mysql_command_consumer_refs *)(m_srv))
      ->error_srv->handle_error(m_srv_ctx_h, sql_errno, err_msg, sqlstate);
}
