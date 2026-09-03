/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include "sql/server_component/mysql_command_consumer_imp.h"
#include <mysql/components/minimal_chassis.h>
#include <limits>
#include "include/my_sys.h"
#include "include/my_thread_local.h"
#include "include/my_time.h"
#include "include/mysql/strings/int2str.h"
#include "include/mysqld_error.h"
#include "my_alloc.h"
#include "mysql/service_mysql_alloc.h"
#include "sql-common/my_decimal.h"
#include "sql/server_component/mysql_command_services_imp.h"

PSI_memory_key key_memory_cc_MYSQL_DATA;
PSI_memory_key key_memory_cc_MYSQL;

DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::start,
                   (SRV_CTX_H * srv_ctx_h, MYSQL_H *mysql_h)) {
  try {
    auto *ctx = (Dom_ctx *)my_malloc(key_memory_cc_MYSQL, sizeof(Dom_ctx),
                                     MYF(MY_WME | MY_ZEROFILL));
    if (ctx == nullptr || mysql_h == nullptr) return true;
    auto *mysql_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    ctx->m_mysql = mysql_handle->mysql;
    *srv_ctx_h = reinterpret_cast<SRV_CTX_H>(ctx);
    auto *mcs_extn = MYSQL_COMMAND_SERVICE_EXTN(ctx->m_mysql);
    mcs_extn->consumer_srv_data = reinterpret_cast<SRV_CTX_H>(ctx);
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
    *ctx->m_sqlstate = not_error_sqlstate;
    ctx->m_cur_row_data = new std::string();
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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
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
    const size_t size = sizeof(MYSQL_FIELD) * num_cols;

    ctx->m_fields = ctx->m_mysql->fields =
        (MYSQL_FIELD *)ctx->m_mysql->field_alloc->Alloc(size);
    if (!ctx->m_mysql->fields) {
      my_error(ER_DA_OOM, MYF(0));
      my_free(ctx->m_mysql->field_alloc);
      return true;
    }
    memset(ctx->m_fields, 0, sizeof(MYSQL_FIELD) * num_cols);
    ctx->m_data->fields = ctx->m_mysql->field_count = num_cols;

    if (ctx->m_cur_field_capacity < num_cols) {
      if (ctx->m_cur_field_offsets != nullptr) {
        my_free(ctx->m_cur_field_offsets);
        ctx->m_cur_field_offsets = nullptr;
      }
      ctx->m_cur_field_capacity = 0;

      if (num_cols > 0) {
        /*
          Reuse one offset array while materializing rows. Non-NULL fields store
          their byte offset into m_cur_row_data, and max size_t means no field
          data has been staged and is materialized as SQL NULL.
        */
        ctx->m_cur_field_offsets = static_cast<size_t *>(my_malloc(
            key_memory_cc_MYSQL, num_cols * sizeof(size_t), MYF(MY_WME)));
        if (ctx->m_cur_field_offsets == nullptr) {
          my_error(ER_DA_OOM, MYF(0));
          return true;
        }
        ctx->m_cur_field_capacity = num_cols;
      }
    }

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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    /* The field metadata strings are part of the query context and will be
       freed after query execution. Copy them into MYSQL::field_alloc so
       mysql_store_result() or mysql_use_result() transfers their ownership to
       MYSQL_RES together with the MYSQL_FIELD array.
    */
    MEM_ROOT *mem_root = ctx->m_mysql->field_alloc;
    /*
      Keep MYSQL_FIELD string metadata compatible with results materialized by
      libmysql: the catalog is "def", and every copied string has its byte
      length recorded in the corresponding length member.
    */
    constexpr char catalog[] = "def";
    ctx->m_fields->catalog_length = sizeof(catalog) - 1;
    ctx->m_fields->catalog =
        strmake_root(mem_root, catalog, ctx->m_fields->catalog_length);
    ctx->m_fields->db_length = strlen(field->db_name);
    ctx->m_fields->db =
        strmake_root(mem_root, field->db_name, ctx->m_fields->db_length);
    ctx->m_fields->table_length = strlen(field->table_name);
    ctx->m_fields->table =
        strmake_root(mem_root, field->table_name, ctx->m_fields->table_length);
    ctx->m_fields->org_table_length = strlen(field->org_table_name);
    ctx->m_fields->org_table = strmake_root(mem_root, field->org_table_name,
                                            ctx->m_fields->org_table_length);
    ctx->m_fields->name_length = strlen(field->col_name);
    ctx->m_fields->name =
        strmake_root(mem_root, field->col_name, ctx->m_fields->name_length);
    ctx->m_fields->org_name_length = strlen(field->org_col_name);
    ctx->m_fields->org_name = strmake_root(mem_root, field->org_col_name,
                                           ctx->m_fields->org_name_length);
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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    ctx->m_cur_row =
        (MYSQL_ROWS *)ctx->m_data->alloc->Alloc(sizeof(MYSQL_ROWS));
    if (ctx->m_cur_row == nullptr) {
      my_error(ER_DA_OOM, MYF(0));
      free_rows(ctx->m_data);
      return true;
    }
    *ctx->m_prev_ptr = ctx->m_cur_row;
    const auto field_count = ctx->m_data->fields;

    if (field_count > 0 && (ctx->m_cur_field_offsets == nullptr ||
                            ctx->m_cur_field_capacity < field_count)) {
      free_rows(ctx->m_data);
      my_error(ER_DA_OOM, MYF(0));
      return true;
    }

    /*
      Stage field bytes for this row first. end_row() copies the staged bytes
      into the result MEM_ROOT and converts the offsets into MYSQL_ROW pointers.
    */
    ctx->m_cur_row_data->clear();
    for (unsigned int i = 0; i < field_count; ++i) {
      ctx->m_cur_field_offsets[i] = std::numeric_limits<size_t>::max();
    }

    ctx->m_cur_row->data = nullptr;
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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    auto count = ctx->m_data->rows;
    MYSQL_ROWS **last_row_hook = &ctx->m_data->data;
    while (--count) last_row_hook = &(*last_row_hook)->next;
    *last_row_hook = nullptr;
    ctx->m_prev_ptr = last_row_hook;
    ctx->m_cur_row = nullptr;
    ctx->m_cur_row_data->clear();
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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;

    /*
      Materialize one row as:

      [field pointers][extra end pointer][non-NULL field bytes + trailing NULs]

      SQL NULL fields remain nullptr. The extra end pointer lets
      csi_fetch_lengths() derive the last non-NULL field length.
    */
    const auto field_count = ctx->m_data->fields;
    const size_t row_data_size = ctx->m_cur_row_data->size();
    const size_t pointer_count = static_cast<size_t>(field_count) + 1;
    const size_t row_data_offset = pointer_count * sizeof(char *);
    if (row_data_size > std::numeric_limits<size_t>::max() - row_data_offset) {
      my_error(ER_DA_OOM, MYF(0));
      return true;
    }
    auto *row = static_cast<MYSQL_ROW>(
        ctx->m_data->alloc->Alloc(row_data_offset + row_data_size));
    if (row == nullptr) {
      my_error(ER_DA_OOM, MYF(0));
      return true;
    }

    ctx->m_cur_row->data = row;
    char *field_data = reinterpret_cast<char *>(row + pointer_count);
    if (row_data_size > 0)
      memcpy(field_data, ctx->m_cur_row_data->data(), row_data_size);

    const size_t null_offset = std::numeric_limits<size_t>::max();
    for (unsigned int i = 0; i < field_count; ++i) {
      if (ctx->m_cur_field_offsets[i] == null_offset) {
        row[i] = nullptr;
        continue;
      }

      row[i] = field_data + ctx->m_cur_field_offsets[i];
    }
    row[field_count] = field_data + row_data_size;
    ctx->m_cur_row_data->clear();

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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return;
    *ctx->m_err_msg = err_msg ? err_msg : "";
    ctx->m_sql_errno = sql_errno;
    *ctx->m_sqlstate = sqlstate ? sqlstate : "";
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return;
  }
}

/* get_null */
DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::get, (SRV_CTX_H srv_ctx_h)) {
  try {
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    const auto field_index = ctx->m_cur_field_num;
    /*
      SQL NULL fields have no staged bytes. Keep the NULL offset sentinel so
      end_row() materializes this column as a nullptr MYSQL_ROW entry.
    */
    ctx->m_cur_field_offsets[field_index] = std::numeric_limits<size_t>::max();
    ++ctx->m_cur_field_num;
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
}

/* get_integer */
DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::get,
                   (SRV_CTX_H srv_ctx_h, long long value)) {
  try {
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    char buff[MY_INT64_NUM_DECIMAL_DIGITS + 1];
    const char *end = longlong10_to_str(value, buff, -10);
    const size_t int_length = end - buff;
    const bool ret = store_data(srv_ctx_h, buff, int_length);
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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    char buff[MY_INT64_NUM_DECIMAL_DIGITS + 1];
    const char *end = longlong10_to_str(value, buff, unsigned_flag ? 10 : -10);
    const size_t length = end - buff;
    const bool ret = store_data(srv_ctx_h, buff, length);
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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    const auto *value = (const decimal_t *)(decimal);
    if (ctx == nullptr) return true;
    char buff[DECIMAL_MAX_STR_LENGTH + 1];
    int string_length = DECIMAL_MAX_STR_LENGTH + 1;
    decimal2string(value, buff, &string_length);
    const bool ret_val = store_data(srv_ctx_h, buff, string_length) != 0;
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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    char buffer[FLOATING_POINT_BUFFER + 1];
    size_t length;
    if (decimals < DECIMAL_NOT_SPECIFIED)
      length = my_fcvt(value, decimals, buffer, nullptr);
    else
      length = my_gcvt(value, MY_GCVT_ARG_FLOAT, FLOATING_POINT_BUFFER, buffer,
                       nullptr);
    const bool ret_val = store_data(srv_ctx_h, buffer, length);
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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    const auto *value = (const MYSQL_TIME *)(time);
    char buff[MAX_DATE_STRING_REP_LENGTH];
    const size_t length = my_date_to_str(*value, buff);
    const bool ret_val = store_data(srv_ctx_h, buff, length);
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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    const auto *value = (const MYSQL_TIME *)(time);
    char buff[MAX_DATE_STRING_REP_LENGTH];
    const size_t length = my_time_to_str(*value, buff, precision);
    const bool ret_val = store_data(srv_ctx_h, buff, length);
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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    const auto *value = (const MYSQL_TIME *)(time);
    char buff[MAX_DATE_STRING_REP_LENGTH];
    const size_t length = my_datetime_to_str(*value, buff, precision);
    const bool ret_val = store_data(srv_ctx_h, buff, length);
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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return true;
    const bool ret = store_data(srv_ctx_h, value, length);
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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return;
    *capabilities = ctx->m_mysql->server_capabilities;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return;
  }
}
DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::error,
                   (SRV_CTX_H srv_ctx_h, unsigned int *err_num,
                    const char **error_msg)) {
  try {
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
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
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    if (ctx == nullptr) return;
    auto *mcs_extn = MYSQL_COMMAND_SERVICE_EXTN(ctx->m_mysql);
    if (mcs_extn != nullptr) mcs_extn->consumer_srv_data = nullptr;
    // Free MYSQL_FIELD buffer allocated in start_result_metadata()
    if (ctx->m_mysql && ctx->m_mysql->field_alloc) {
      ctx->m_mysql->field_alloc->Clear();
      my_free(ctx->m_mysql->field_alloc);
      ctx->m_mysql->field_alloc = nullptr;
      ctx->m_mysql->fields = nullptr;
    }

    /*
      mysql_free_result() normally releases result data. If the consumer is
      ended first, release it here and clear both extension aliases so later
      cleanup cannot observe freed storage.
    */
    if (*ctx->m_result) {
      (*ctx->m_result)->alloc->Clear();
      my_free((*ctx->m_result)->alloc);
      my_free(*ctx->m_result);
      *ctx->m_result = nullptr;
    }
    if (mcs_extn != nullptr) mcs_extn->use_result_cursor = nullptr;
    if (ctx->m_cur_field_offsets != nullptr) my_free(ctx->m_cur_field_offsets);
    delete ctx->m_cur_row_data;
    delete ctx->m_message;
    delete ctx->m_err_msg;
    delete ctx->m_sqlstate;
    my_free(ctx);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return;
  }
}

DEFINE_BOOL_METHOD(mysql_command_consumer_dom_imp::store_data,
                   (SRV_CTX_H srv_ctx_h, const char *data, size_t length)) {
  try {
    auto *ctx = reinterpret_cast<Dom_ctx *>(srv_ctx_h);
    assert(ctx);
    if (length > std::numeric_limits<ulong>::max() ||
        length == std::numeric_limits<size_t>::max()) {
      my_error(ER_DA_OOM, MYF(0));
      return true;
    }

    const auto field_index = ctx->m_cur_field_num;
    /*
      Store the field bytes plus a trailing NUL byte. The saved offset is
      converted to a MYSQL_ROW pointer in end_row().
    */
    const size_t field_offset = ctx->m_cur_row_data->size();
    if (length >= ctx->m_cur_row_data->max_size() - field_offset) {
      my_error(ER_DA_OOM, MYF(0));
      return true;
    }

    const size_t required_capacity = field_offset + length + 1;
    if (ctx->m_cur_row_data->capacity() < required_capacity)
      ctx->m_cur_row_data->reserve(required_capacity);

    if (length > 0) ctx->m_cur_row_data->append(data, length);
    ctx->m_cur_row_data->push_back('\0');
    ctx->m_cur_field_offsets[field_index] = field_offset;
    ctx->m_cur_row->length += length;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return true;
  }
  return false;
}
