/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "sql/server_component/mysql_command_consumer_imp.h"
#include <mysql/components/minimal_chassis.h>
#include "include/my_byteorder.h"
#include "include/my_sys.h"
#include "include/my_time.h"
#include "include/mysql/service_command.h"
#include "include/mysqld_error.h"
#include "m_ctype.h"
#include "sql/my_decimal.h"
#include "sql/server_component/mysql_command_services_imp.h"
#include "sql_string.h"  // String

PSI_memory_key key_memory_cc_MYSQL_DATA;
PSI_memory_key key_memory_cc_MYSQL;

DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::start,
                   (SRV_CTX_H * srv_ctx_h, MYSQL_H *mysql_h)) {
  try {
    Dom_ctx *ctx = (Dom_ctx *)my_malloc(key_memory_cc_MYSQL, sizeof(Dom_ctx),
                                        MYF(MY_WME | MY_ZEROFILL));
    if (ctx == nullptr || mysql_h == nullptr) return true;
    Mysql_handle *mysql_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    ctx->m_mysql = mysql_handle->mysql;
    *srv_ctx_h = reinterpret_cast<SRV_CTX_H>(ctx);
    auto mcs_extn = MYSQL_COMMAND_SERVICE_EXTN(ctx->m_mysql);
    mcs_extn->consumer_srv_data = reinterpret_cast<SRV_CTX_H *>(ctx);
    ctx->m_result = &mcs_extn->data;
    if (!(*ctx->m_result = (MYSQL_DATA *)my_malloc(
              key_memory_cc_MYSQL_DATA, sizeof(MYSQL_DATA),
              MYF(MY_WME | MY_ZEROFILL))) ||
        !((*ctx->m_result)->alloc = (MEM_ROOT *)my_malloc(
              key_memory_cc_MYSQL_DATA, 8192, /* Assume rowlength < 8192 */
              MYF(MY_WME | MY_ZEROFILL)))) {
      my_error(ER_DA_OOM, MYF(0));
      free_rows(*ctx->m_result);
      my_free(ctx);
      return true;
    }
    ctx->m_message = new std::string();
    ctx->m_err_msg = new std::string();
    ctx->m_sqlstate = new std::string();
    ctx->m_data = *ctx->m_result;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::start_result_metadata,
                   (SRV_CTX_H srv_ctx_h, unsigned int num_cols, unsigned int,
                    const char *const)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    if (ctx->m_mysql->field_alloc == nullptr) {
      ctx->m_mysql->field_alloc = (MEM_ROOT *)my_malloc(
          key_memory_cc_MYSQL, 8192, MYF(MY_WME | MY_ZEROFILL));
      /* Assume rowlength < 8192 */
      if (ctx->m_mysql->field_alloc == nullptr) {
        ctx->m_sql_errno = my_errno();
        *ctx->m_err_msg = "Out of memory";
        return true;
      }
    }

    ctx->m_field_column_count = (protocol_41(ctx->m_mysql) ? 7 : 5);
    size_t size = sizeof(MYSQL_FIELD) * num_cols;

    ctx->m_fields = ctx->m_mysql->fields =
        (MYSQL_FIELD *)ctx->m_mysql->field_alloc->Alloc(size);
    if (!ctx->m_mysql->fields) {
      my_error(ER_DA_OOM, MYF(0));
      my_free(ctx->m_mysql->field_alloc);
      return true;
    }
    memset(ctx->m_fields, 0, sizeof(MYSQL_FIELD) * num_cols);
    ctx->m_data->fields = ctx->m_mysql->field_count = num_cols;

    // Prepare for rows
    ctx->m_prev_ptr = &ctx->m_data->data;
    ctx->m_data->rows = 0;
    ctx->m_mysql->status = MYSQL_STATUS_GET_RESULT;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::field_metadata,
                   (SRV_CTX_H srv_ctx_h, struct Field_metadata *field,
                    const char *const)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    /* The field metadata strings are part of the query context and will be
       freed after query execution. Copy the metadata strings into the result
       set context so they can be accessed upon return to the caller.
    */
    MEM_ROOT *mem_root = (*ctx->m_result)->alloc;
    ctx->m_fields->db =
        strmake_root(mem_root, field->db_name, strlen(field->db_name));
    ctx->m_fields->table =
        strmake_root(mem_root, field->table_name, strlen(field->table_name));
    ctx->m_fields->org_table = strmake_root(mem_root, field->org_table_name,
                                            strlen(field->org_table_name));
    ctx->m_fields->name =
        strmake_root(mem_root, field->col_name, strlen(field->col_name));
    ctx->m_fields->org_name = strmake_root(mem_root, field->org_col_name,
                                           strlen(field->org_col_name));
    ctx->m_fields->length = field->length;
    ctx->m_fields->charsetnr = field->charsetnr;
    ctx->m_fields->flags = field->flags;
    ctx->m_fields->decimals = field->decimals;
    ctx->m_fields->type = static_cast<enum_field_types>(field->type);
    ctx->m_fields++;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::end_result_metadata,
                   (SRV_CTX_H, unsigned int, unsigned int)) {
  return false;
}

DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::start_row,
                   (SRV_CTX_H srv_ctx_h)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    ctx->m_cur_row =
        (MYSQL_ROWS *)ctx->m_data->alloc->Alloc(sizeof(MYSQL_ROWS));
    if (ctx->m_cur_row == nullptr) {
      my_error(ER_DA_OOM, MYF(0));
      free_rows(ctx->m_data);
      return true;
    }
    *ctx->m_prev_ptr = ctx->m_cur_row;
    ctx->m_cur_row->data = (MYSQL_ROW)ctx->m_data->alloc->Alloc(
        ctx->m_data->fields * sizeof(char *));
    if (ctx->m_cur_row->data == nullptr) {
      free_rows(ctx->m_data);
      my_error(ER_DA_OOM, MYF(0));
      return true;
    }
    ctx->m_cur_row->length = 0;
    ctx->m_cur_row->next = nullptr;
    ctx->m_cur_field_num = 0;
    ctx->m_data->rows++;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::abort_row,
                   (SRV_CTX_H srv_ctx_h)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    auto count = ctx->m_data->rows;
    MYSQL_ROWS **last_row_hook = &ctx->m_data->data;
    while (--count) last_row_hook = &(*last_row_hook)->next;
    *last_row_hook = nullptr;
    ctx->m_prev_ptr = last_row_hook;
    ctx->m_data->rows--;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::end_row,
                   (SRV_CTX_H srv_ctx_h)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    ctx->m_prev_ptr = &ctx->m_cur_row->next;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

DEFINE_METHOD(void, mysql_command_consumer_dom_imp::handle_ok,
              (SRV_CTX_H srv_ctx_h, unsigned int server_status,
               unsigned int statement_warn_count,
               unsigned long long affected_rows,
               unsigned long long last_insert_id, const char *const message)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return;
    ctx->m_mysql->affected_rows = affected_rows;
    ctx->m_mysql->warning_count = statement_warn_count;
    ctx->m_mysql->server_status = server_status;
    ctx->m_mysql->free_me = true;
    ctx->m_mysql->insert_id = last_insert_id;
    *ctx->m_message = message ? message : "";
    return;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return;
  }
}

DEFINE_METHOD(void, mysql_command_consumer_dom_imp::handle_error,
              (SRV_CTX_H srv_ctx_h, unsigned int sql_errno,
               const char *const err_msg, const char *const sqlstate)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return;
    *ctx->m_err_msg = err_msg ? err_msg : "";
    ctx->m_sql_errno = sql_errno;
    *ctx->m_sqlstate = sqlstate ? sqlstate : "";
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return;
  }
  return;
}

/* get_null */
DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::get, (SRV_CTX_H srv_ctx_h)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    char buff[5] = {"NULL"}; /* Store 'NULL' as null value into the data */
    bool ret = store_data(srv_ctx_h, buff, 4);
    ++ctx->m_cur_field_num;
    return ret;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

/* get_integer */
DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::get,
                   (SRV_CTX_H srv_ctx_h, long long value)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    char buff[MY_INT64_NUM_DECIMAL_DIGITS + 1];
    const char *end = longlong10_to_str(value, buff, -10);
    const size_t int_length = end - buff;
    bool ret = store_data(srv_ctx_h, buff, int_length);
    ++ctx->m_cur_field_num;
    return ret;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

/* get_longlong */
DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::get,
                   (SRV_CTX_H srv_ctx_h, long long value,
                    unsigned int unsigned_flag)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    char buff[MY_INT64_NUM_DECIMAL_DIGITS + 1];
    const char *end = longlong10_to_str(value, buff, unsigned_flag ? 10 : -10);
    const size_t length = end - buff;
    bool ret = store_data(srv_ctx_h, buff, length);
    ++ctx->m_cur_field_num;
    return ret;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

/* get_decimal */
DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::get,
                   (SRV_CTX_H srv_ctx_h, const DECIMAL_T_H decimal)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    auto value = (const decimal_t *)(decimal);
    if (ctx == nullptr) return true;
    char buff[DECIMAL_MAX_STR_LENGTH + 1];
    int string_length = DECIMAL_MAX_STR_LENGTH + 1;
    String str(buff, sizeof(buff), &my_charset_bin);
    decimal2string(value, buff, &string_length);
    bool ret_val = store_data(srv_ctx_h, str.ptr(), str.length());
    ++ctx->m_cur_field_num;
    return ret_val;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

/* get_double */
DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::get,
                   (SRV_CTX_H srv_ctx_h, double value, unsigned int decimals)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    char buffer[FLOATING_POINT_BUFFER + 1];
    size_t length;
    if (decimals < DECIMAL_NOT_SPECIFIED)
      length = my_fcvt(value, decimals, buffer, nullptr);
    else
      length = my_gcvt(value, MY_GCVT_ARG_FLOAT, FLOATING_POINT_BUFFER, buffer,
                       nullptr);
    bool ret_val = store_data(srv_ctx_h, buffer, length);
    ++ctx->m_cur_field_num;
    return ret_val;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::get_date,
                   (SRV_CTX_H srv_ctx_h, const MYSQL_TIME_H time)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    auto value = (const MYSQL_TIME *)(time);
    char buff[MAX_DATE_STRING_REP_LENGTH];
    size_t length = my_date_to_str(*value, buff);
    bool ret_val = store_data(srv_ctx_h, buff, length);
    ++ctx->m_cur_field_num;
    return ret_val;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}
DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::get_time,
                   (SRV_CTX_H srv_ctx_h, const MYSQL_TIME_H time,
                    unsigned int precision)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    auto value = (const MYSQL_TIME *)(time);
    char buff[MAX_DATE_STRING_REP_LENGTH];
    size_t length = my_time_to_str(*value, buff, precision);
    bool ret_val = store_data(srv_ctx_h, buff, length);
    ++ctx->m_cur_field_num;
    return ret_val;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}
DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::get_datetime,
                   (SRV_CTX_H srv_ctx_h, const MYSQL_TIME_H time,
                    unsigned int precision)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    auto value = (const MYSQL_TIME *)(time);
    char buff[MAX_DATE_STRING_REP_LENGTH];
    size_t length = my_datetime_to_str(*value, buff, precision);
    bool ret_val = store_data(srv_ctx_h, buff, length);
    ++ctx->m_cur_field_num;
    return ret_val;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}
DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::get_string,
                   (SRV_CTX_H srv_ctx_h, const char *const value, size_t length,
                    const char *const)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    bool ret = store_data(srv_ctx_h, value, length);
    ++(ctx->m_cur_field_num);
    return ret;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

DEFINE_METHOD(void, mysql_command_consumer_dom_imp::client_capabilities,
              (SRV_CTX_H srv_ctx_h, unsigned long *capabilities)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return;
    *capabilities = ctx->m_mysql->server_capabilities;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return;
  }
  return;
}
DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::error,
                   (SRV_CTX_H srv_ctx_h, unsigned int *err_num,
                    const char **error_msg)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr || err_num == nullptr || error_msg == nullptr)
      return true;
    *err_num = ctx->m_sql_errno;
    *error_msg = ctx->m_err_msg->data();
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}

DEFINE_METHOD(void, mysql_command_consumer_dom_imp::end,
              (SRV_CTX_H srv_ctx_h)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return;
    /* The m_result is freed by
       free_result->mysql_free_result()->free_rows() api.
       In non result cases, it has to be freed here. */
    if (*ctx->m_result) {
      (*ctx->m_result)->alloc->Clear();
      my_free((*ctx->m_result)->alloc);
      my_free(*ctx->m_result);
    }
    delete ctx->m_message;
    delete ctx->m_err_msg;
    delete ctx->m_sqlstate;
    my_free(ctx);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return;
  }
  return;
}

DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::store_data,
                   (SRV_CTX_H srv_ctx_h, const char *data, size_t length)) {
  try {
    Dom_ctx *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    assert(ctx);
    char *&field_buf = ctx->m_cur_row->data[ctx->m_cur_field_num];
    field_buf = (char *)ctx->m_data->alloc->Alloc(length + 1);
    if (field_buf == nullptr) {
      my_error(ER_DA_OOM, MYF(0));
      return true;
    }
    memcpy(field_buf, (const uchar *)data, length);
    field_buf[length] = '\0';
    ctx->m_cur_row->length += length;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}
