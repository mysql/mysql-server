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

#define LOG_COMPONENT_TAG "test_sql_errors"

#include <fcntl.h>
#include <mysql/plugin.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>

#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"  // my_write, my_malloc
#include "mysql_com.h"
#include "sql_string.h" /* STRING_PSI_MEMORY_KEY */
#include "template_utils.h"

static const char *log_filename = "test_sql_errors";

#define MAX_SESSIONS 128

#define STRING_BUFFER_SIZE 1024

static const char *sep =
    "========================================================================"
    "\n";

#define WRITE_SEP() \
  my_write(outfile, pointer_cast<const uchar *>(sep), strlen(sep), MYF(0))

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

static File outfile;

static void WRITE_STR(const char *format) {
  char buffer[STRING_BUFFER_SIZE];
  snprintf(buffer, sizeof(buffer), "%s", format);
  my_write(outfile, (uchar *)buffer, strlen(buffer), MYF(0));
}

template <typename T>
void WRITE_VAL(const char *format, T value) {
  char buffer[STRING_BUFFER_SIZE];
  snprintf(buffer, sizeof(buffer), format, value);
  my_write(outfile, (uchar *)buffer, strlen(buffer), MYF(0));
}

template <typename T1, typename T2>
void WRITE_VAL(const char *format, T1 value1, T2 value2) {
  char buffer[STRING_BUFFER_SIZE];
  snprintf(buffer, sizeof(buffer), format, value1, value2);
  my_write(outfile, (uchar *)buffer, strlen(buffer), MYF(0));
}

static const char *user_localhost = "localhost";
static const char *user_local = "127.0.0.1";
static const char *user_db = "";
static const char *user_privileged = "root";
// static const char *user_ordinary= "ordinary";

static void switch_user(MYSQL_SESSION session, const char *user) {
  MYSQL_SECURITY_CONTEXT sc;
  thd_get_security_context(srv_session_info_get_thd(session), &sc);
  security_context_lookup(sc, user, user_localhost, user_local, user_db);
}

struct st_send_field_n {
  char db_name[256];
  char table_name[256];
  char org_table_name[256];
  char col_name[256];
  char org_col_name[256];
  unsigned long length;
  unsigned int charsetnr;
  unsigned int flags;
  unsigned int decimals;
  enum_field_types type;
};

struct st_decimal_n {
  int intg, frac, len;
  bool sign;
  decimal_digit_t buf[256];
};

struct st_plugin_ctx {
  const CHARSET_INFO *resultcs;
  uint meta_server_status;
  uint meta_warn_count;
  uint current_col;
  uint num_cols;
  uint num_rows;
  st_send_field_n sql_field[64];
  char sql_str_value[64][64][256];
  size_t sql_str_len[64][64];
  longlong sql_int_value[64][64];
  longlong sql_longlong_value[64][64];
  uint sql_is_unsigned[64][64];
  st_decimal_n sql_decimal_value[64][64];
  double sql_double_value[64][64];
  uint32 sql_double_decimals[64][64];
  MYSQL_TIME sql_date_value[64][64];
  MYSQL_TIME sql_time_value[64][64];
  uint sql_time_decimals[64][64];
  MYSQL_TIME sql_datetime_value[64][64];
  uint sql_datetime_decimals[64][64];

  uint server_status;
  uint warn_count;
  uint affected_rows;
  uint last_insert_id;
  char message[1024];

  uint sql_errno;
  char err_msg[1024];
  char sqlstate[6];
  st_plugin_ctx() { reset(); }

  void reset() {
    resultcs = nullptr;
    server_status = 0;
    current_col = 0;
    warn_count = 0;
    num_cols = 0;
    num_rows = 0;
    memset(&sql_field, 0, 64 * sizeof(st_send_field_n));
    memset(&sql_str_value, 0, 64 * 64 * 256 * sizeof(char));
    memset(&sql_str_len, 0, 64 * 64 * sizeof(size_t));
    memset(&sql_int_value, 0, 64 * 64 * sizeof(longlong));
    memset(&sql_longlong_value, 0, 64 * 64 * sizeof(longlong));
    memset(&sql_is_unsigned, 0, 64 * 64 * sizeof(uint));
    memset(&sql_decimal_value, 0, 64 * 64 * sizeof(st_decimal_n));
    memset(&sql_double_value, 0, 64 * 64 * sizeof(double));
    memset(&sql_double_decimals, 0, 64 * 64 * sizeof(uint32));
    memset(&sql_date_value, 0, 64 * 64 * sizeof(MYSQL_TIME));
    memset(&sql_time_value, 0, 64 * 64 * sizeof(MYSQL_TIME));
    memset(&sql_time_decimals, 0, 64 * 64 * sizeof(uint));
    memset(&sql_datetime_value, 0, 64 * 64 * sizeof(MYSQL_TIME));
    memset(&sql_datetime_decimals, 0, 64 * 64 * sizeof(uint));

    server_status = 0;
    warn_count = 0;
    affected_rows = 0;
    last_insert_id = 0;
    memset(&message, 0, sizeof(message));

    sql_errno = 0;
    memset(&err_msg, 0, sizeof(err_msg));
    memset(&sqlstate, 0, sizeof(sqlstate));
  }
};

static int sql_start_result_metadata(void *ctx, uint num_cols, uint,
                                     const CHARSET_INFO *resultcs) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  DBUG_PRINT("info", ("resultcs->number: %d", resultcs->number));
  DBUG_PRINT("info", ("resultcs->csname: %s", resultcs->csname));
  DBUG_PRINT("info", ("resultcs->m_coll_name: %s", resultcs->m_coll_name));
  pctx->num_cols = num_cols;
  pctx->resultcs = resultcs;
  pctx->current_col = 0;
  return false;
}

static int sql_field_metadata(void *ctx, struct st_send_field *field,
                              const CHARSET_INFO *) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  st_send_field_n *cfield = &pctx->sql_field[pctx->current_col];
  DBUG_TRACE;
  DBUG_PRINT("info", ("field->db_name: %s", field->db_name));
  DBUG_PRINT("info", ("field->table_name: %s", field->table_name));
  DBUG_PRINT("info", ("field->org_table_name: %s", field->org_table_name));
  DBUG_PRINT("info", ("field->col_name: %s", field->col_name));
  DBUG_PRINT("info", ("field->org_col_name: %s", field->org_col_name));
  DBUG_PRINT("info", ("field->length: %d", (int)field->length));
  DBUG_PRINT("info", ("field->charsetnr: %d", (int)field->charsetnr));
  DBUG_PRINT("info", ("field->flags: %d", (int)field->flags));
  DBUG_PRINT("info", ("field->decimals: %d", (int)field->decimals));
  DBUG_PRINT("info", ("field->type: %d", (int)field->type));

  strcpy(cfield->db_name, field->db_name);
  strcpy(cfield->table_name, field->table_name);
  strcpy(cfield->org_table_name, field->org_table_name);
  strcpy(cfield->col_name, field->col_name);
  strcpy(cfield->org_col_name, field->org_col_name);
  cfield->length = field->length;
  cfield->charsetnr = field->charsetnr;
  cfield->flags = field->flags;
  cfield->decimals = field->decimals;
  cfield->type = field->type;

  pctx->current_col++;
  return false;
}

static int sql_end_result_metadata(void *ctx, uint server_status,
                                   uint warn_count) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  pctx->meta_server_status = server_status;
  pctx->meta_warn_count = warn_count;
  pctx->num_rows = 0;
  return false;
}

static int sql_start_row(void *ctx) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  pctx->current_col = 0;
  return false;
}

static int sql_end_row(void *ctx) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  pctx->num_rows++;
  return false;
}

static void sql_abort_row(void *ctx) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  pctx->current_col = 0;
}

static ulong sql_get_client_capabilities(void *) {
  DBUG_TRACE;
  return 0;
}

static int sql_get_null(void *ctx) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  uint row = pctx->num_rows;
  uint col = pctx->current_col;
  pctx->current_col++;

  memcpy(pctx->sql_str_value[row][col], "[NULL]", sizeof("[NULL]"));
  pctx->sql_str_len[row][col] = sizeof("[NULL]") - 1;

  return false;
}

static int sql_get_integer(void *ctx, longlong value) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  uint row = pctx->num_rows;
  uint col = pctx->current_col;
  pctx->current_col++;

  size_t len = snprintf(pctx->sql_str_value[row][col],
                        sizeof(pctx->sql_str_value[row][col]), "%lld", value);
  pctx->sql_str_len[row][col] = len;
  pctx->sql_int_value[row][col] = value;

  return false;
}

static int sql_get_longlong(void *ctx, longlong value, uint is_unsigned) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  uint row = pctx->num_rows;
  uint col = pctx->current_col;
  pctx->current_col++;

  size_t len = snprintf(pctx->sql_str_value[row][col],
                        sizeof(pctx->sql_str_value[row][col]),
                        is_unsigned ? "%llu" : "%lld", value);

  pctx->sql_str_len[row][col] = len;
  pctx->sql_longlong_value[row][col] = value;
  pctx->sql_is_unsigned[row][col] = is_unsigned;

  return false;
}

static int sql_get_decimal(void *ctx, const decimal_t *value) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  uint row = pctx->num_rows;
  uint col = pctx->current_col;
  pctx->current_col++;

  size_t len = snprintf(pctx->sql_str_value[row][col],
                        sizeof(pctx->sql_str_value[row][col]),
                        "%s%d.%d(%d)[%s]", value->sign ? "+" : "-", value->intg,
                        value->frac, value->len, (char *)value->buf);
  pctx->sql_str_len[row][col] = len;
  pctx->sql_decimal_value[row][col].intg = value->intg;
  pctx->sql_decimal_value[row][col].frac = value->frac;
  pctx->sql_decimal_value[row][col].len = value->len;
  pctx->sql_decimal_value[row][col].sign = value->sign;
  memset((void *)pctx->sql_decimal_value[row][col].buf, '\0', (int)value->len);
  memcpy((void *)pctx->sql_decimal_value[row][col].buf, (void *)value->buf,
         (int)value->len);

  return false;
}

static int sql_get_double(void *ctx, double value, uint32 decimals) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  uint row = pctx->num_rows;
  uint col = pctx->current_col;
  pctx->current_col++;

  size_t len = snprintf(pctx->sql_str_value[row][col],
                        sizeof(pctx->sql_str_value[row][col]), "%3.7g", value);

  pctx->sql_str_len[row][col] = len;

  pctx->sql_double_value[row][col] = value;
  pctx->sql_double_decimals[row][col] = decimals;

  return false;
}

static int sql_get_date(void *ctx, const MYSQL_TIME *value) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  uint row = pctx->num_rows;
  uint col = pctx->current_col;
  pctx->current_col++;

  size_t len =
      snprintf(pctx->sql_str_value[row][col],
               sizeof(pctx->sql_str_value[row][col]), "%s%4d-%02d-%02d",
               value->neg ? "-" : "", value->year, value->month, value->day);
  pctx->sql_str_len[row][col] = len;

  pctx->sql_date_value[row][col].year = value->year;
  pctx->sql_date_value[row][col].month = value->month;
  pctx->sql_date_value[row][col].day = value->day;

  pctx->sql_date_value[row][col].hour = value->hour;
  pctx->sql_date_value[row][col].minute = value->minute;
  pctx->sql_date_value[row][col].second = value->second;
  pctx->sql_date_value[row][col].second_part = value->second_part;
  pctx->sql_date_value[row][col].neg = value->neg;

  return false;
}

static int sql_get_time(void *ctx, const MYSQL_TIME *value, uint decimals) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  uint row = pctx->num_rows;
  uint col = pctx->current_col;
  pctx->current_col++;

  size_t len = snprintf(
      pctx->sql_str_value[row][col], sizeof(pctx->sql_str_value[row][col]),
      "%s%02d:%02d:%02d", value->neg ? "-" : "",
      value->day ? (value->day * 24 + value->hour) : value->hour, value->minute,
      value->second);

  pctx->sql_str_len[row][col] = len;

  pctx->sql_time_value[row][col].year = value->year;
  pctx->sql_time_value[row][col].month = value->month;
  pctx->sql_time_value[row][col].day = value->day;

  pctx->sql_time_value[row][col].hour = value->hour;
  pctx->sql_time_value[row][col].minute = value->minute;
  pctx->sql_time_value[row][col].second = value->second;
  pctx->sql_time_value[row][col].second_part = value->second_part;
  pctx->sql_time_value[row][col].neg = value->neg;
  pctx->sql_time_decimals[row][col] = decimals;

  return false;
}

static int sql_get_datetime(void *ctx, const MYSQL_TIME *value, uint decimals) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  uint row = pctx->num_rows;
  uint col = pctx->current_col;
  pctx->current_col++;

  size_t len = snprintf(
      pctx->sql_str_value[row][col], sizeof(pctx->sql_str_value[row][col]),
      "%s%4d-%02d-%02d %02d:%02d:%02d", value->neg ? "-" : "", value->year,
      value->month, value->day, value->hour, value->minute, value->second);

  pctx->sql_str_len[row][col] = len;

  pctx->sql_datetime_value[row][col].year = value->year;
  pctx->sql_datetime_value[row][col].month = value->month;
  pctx->sql_datetime_value[row][col].day = value->day;

  pctx->sql_datetime_value[row][col].hour = value->hour;
  pctx->sql_datetime_value[row][col].minute = value->minute;
  pctx->sql_datetime_value[row][col].second = value->second;
  pctx->sql_datetime_value[row][col].second_part = value->second_part;
  pctx->sql_datetime_value[row][col].neg = value->neg;
  pctx->sql_datetime_decimals[row][col] = decimals;

  return false;
}

static int sql_get_string(void *ctx, const char *const value, size_t length,
                          const CHARSET_INFO *const) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  uint row = pctx->num_rows;
  uint col = pctx->current_col;
  pctx->current_col++;

  strncpy(pctx->sql_str_value[row][col], value, length);
  pctx->sql_str_len[row][col] = length;

  return false;
}

static void sql_handle_ok(void *ctx, uint server_status,
                          uint statement_warn_count, ulonglong affected_rows,
                          ulonglong last_insert_id, const char *const message) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  /* This could be an EOF */
  if (!pctx->num_cols) pctx->num_rows = 0;
  pctx->server_status = server_status;
  pctx->warn_count = statement_warn_count;
  pctx->affected_rows = affected_rows;
  pctx->last_insert_id = last_insert_id;
  if (message) strncpy(pctx->message, message, sizeof(pctx->message) - 1);
  pctx->message[sizeof(pctx->message) - 1] = '\0';
}

static void sql_handle_error(void *ctx, uint sql_errno,
                             const char *const err_msg,
                             const char *const sqlstate) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  pctx->sql_errno = sql_errno;
  if (pctx->sql_errno) {
    strcpy(pctx->err_msg, err_msg);
    strcpy(pctx->sqlstate, sqlstate);
  }
  pctx->num_rows = 0;
}

static void sql_shutdown(void *, int) { DBUG_TRACE; }

const struct st_command_service_cbs sql_cbs = {
    sql_start_result_metadata,
    sql_field_metadata,
    sql_end_result_metadata,
    sql_start_row,
    sql_end_row,
    sql_abort_row,
    sql_get_client_capabilities,
    sql_get_null,
    sql_get_integer,
    sql_get_longlong,
    sql_get_decimal,
    sql_get_double,
    sql_get_date,
    sql_get_time,
    sql_get_datetime,
    sql_get_string,
    sql_handle_ok,
    sql_handle_error,
    sql_shutdown,
    nullptr,
};

static const char *fieldtype2str(enum enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_BIT:
      return "BIT";
    case MYSQL_TYPE_BLOB:
      return "BLOB";
    case MYSQL_TYPE_DATE:
      return "DATE";
    case MYSQL_TYPE_DATETIME:
      return "DATETIME";
    case MYSQL_TYPE_NEWDECIMAL:
      return "NEWDECIMAL";
    case MYSQL_TYPE_DECIMAL:
      return "DECIMAL";
    case MYSQL_TYPE_DOUBLE:
      return "DOUBLE";
    case MYSQL_TYPE_ENUM:
      return "ENUM";
    case MYSQL_TYPE_FLOAT:
      return "FLOAT";
    case MYSQL_TYPE_GEOMETRY:
      return "GEOMETRY";
    case MYSQL_TYPE_INT24:
      return "INT24";
    case MYSQL_TYPE_LONG:
      return "LONG";
    case MYSQL_TYPE_LONGLONG:
      return "LONGLONG";
    case MYSQL_TYPE_LONG_BLOB:
      return "LONG_BLOB";
    case MYSQL_TYPE_MEDIUM_BLOB:
      return "MEDIUM_BLOB";
    case MYSQL_TYPE_NEWDATE:
      return "NEWDATE";
    case MYSQL_TYPE_NULL:
      return "NULL";
    case MYSQL_TYPE_SET:
      return "SET";
    case MYSQL_TYPE_SHORT:
      return "SHORT";
    case MYSQL_TYPE_STRING:
      return "STRING";
    case MYSQL_TYPE_TIME:
      return "TIME";
    case MYSQL_TYPE_TIMESTAMP:
      return "TIMESTAMP";
    case MYSQL_TYPE_TINY:
      return "TINY";
    case MYSQL_TYPE_TINY_BLOB:
      return "TINY_BLOB";
    case MYSQL_TYPE_VARCHAR:
      return "VARCHAR";
    case MYSQL_TYPE_VAR_STRING:
      return "VAR_STRING";
    case MYSQL_TYPE_YEAR:
      return "YEAR";
    default:
      return "?-unknown-?";
  }
}

static char *fieldflags2str(uint f) {
  static char buf[1024];
  char *s = buf;
  *s = 0;

#define ff2s_check_flag(X)    \
  if (f & X##_FLAG) {         \
    s = my_stpcpy(s, #X " "); \
    f &= ~X##_FLAG;           \
  }
  ff2s_check_flag(NOT_NULL);
  ff2s_check_flag(PRI_KEY);
  ff2s_check_flag(UNIQUE_KEY);
  ff2s_check_flag(MULTIPLE_KEY);
  ff2s_check_flag(BLOB);
  ff2s_check_flag(UNSIGNED);
  ff2s_check_flag(ZEROFILL);
  ff2s_check_flag(BINARY);
  ff2s_check_flag(ENUM);
  ff2s_check_flag(AUTO_INCREMENT);
  ff2s_check_flag(TIMESTAMP);
  ff2s_check_flag(SET);
  ff2s_check_flag(NO_DEFAULT_VALUE);
  ff2s_check_flag(NUM);
  ff2s_check_flag(PART_KEY);
  ff2s_check_flag(GROUP);
  ff2s_check_flag(UNIQUE);
  ff2s_check_flag(BINCMP);
  ff2s_check_flag(ON_UPDATE_NOW);
#undef ff2s_check_flag
  if (f) sprintf(s, " unknown=0x%04x", f);
  return buf;
}

static void get_data_str(struct st_plugin_ctx *pctx) {
  /* start metadata */
  WRITE_VAL("num_cols: %d\n", pctx->num_cols);
  WRITE_VAL("nb rows: %d\n", pctx->num_rows);

  /* get values */
  for (uint col = 0; col < pctx->num_cols; col++) {
    WRITE_VAL("%s  ", pctx->sql_field[col].col_name);
  }
  WRITE_STR("\n");

  for (uint col = 0; col < pctx->num_cols; col++) {
    WRITE_VAL("%s(%u) ", fieldtype2str(pctx->sql_field[col].type),
              pctx->sql_field[col].type);
  }
  WRITE_STR("\n");

  for (uint col = 0; col < pctx->num_cols; col++) {
    WRITE_VAL("(%s) ", fieldflags2str(pctx->sql_field[col].flags));
  }
  WRITE_STR("\n");

  WRITE_STR("Write a string\n");
  for (uint row = 0; row < pctx->num_rows; row++) {
    for (uint col = 0; col < pctx->num_cols; col++) {
      WRITE_VAL("%s  ", pctx->sql_str_value[row][col]);
    }
    WRITE_STR("\n");
  }
  WRITE_STR("\n");
}

static void handle_error(struct st_plugin_ctx *pctx) {
  /* handle_ok/error */
  if (pctx->sql_errno) {
    WRITE_VAL("error     : %d\n", pctx->sql_errno);
    WRITE_VAL("error msg : %s\n", pctx->err_msg);
  } else {
    WRITE_VAL("affected rows : %d\n", pctx->affected_rows);
    WRITE_VAL("server status : %d\n", pctx->server_status);
    WRITE_VAL("warn count    : %d\n", pctx->warn_count);
  }
}

static void exec_test_cmd(MYSQL_SESSION session, const char *test_cmd,
                          void *p [[maybe_unused]], void *ctx) {
  WRITE_VAL("%s\n", test_cmd);
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  COM_DATA cmd;
  pctx->reset();
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_query.query = test_cmd;
  cmd.com_query.length = strlen(cmd.com_query.query);
  int fail = command_service_run_command(session, COM_QUERY, &cmd,
                                         &my_charset_utf8mb3_general_ci,
                                         &sql_cbs, CS_TEXT_REPRESENTATION, ctx);
  if (fail) {
    if (!srv_session_close(session))
      LogPluginErrMsg(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                      "test_sql_2_sessions - ret code : %d", fail);
  } else {
    if (pctx->num_cols) get_data_str(pctx);
    handle_error(pctx);
  }
}

/* SQL (system) variable to control number of sessions                    */
/* Only effective at start od mysqld by setting it as option --loose-...  */
int nb_sessions;
static MYSQL_SYSVAR_INT(nb_sessions, nb_sessions, PLUGIN_VAR_RQCMDARG,
                        "number of sessions", nullptr, nullptr, 1, 1, 500, 0);

static SYS_VAR *test_services_sysvars[] = {MYSQL_SYSVAR(nb_sessions), nullptr};

static void test_sql(void *p) {
  DBUG_TRACE;
  struct st_plugin_ctx *plugin_ctx = new st_plugin_ctx();

  /* Session declarations */
  MYSQL_SESSION session_1, session_2;

  /* Opening session 1 */
  WRITE_STR("Opening Session 1\n");
  session_1 = srv_session_open(NULL, plugin_ctx);
  if (!session_1)
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "Open session_1 failed.");
  else
    switch_user(session_1, user_privileged);

  /* Statement 1 */
  WRITE_SEP();
  WRITE_STR("Session 1 : \n");
  WRITE_SEP();
  exec_test_cmd(session_1, "SELECT * FROM test.t_int", p, plugin_ctx);

  /* Opening session 1 a second time
  WRITE_STR("Opening Session 1 a second time\n");
  session_1= srv_session_open(NULL,plugin_ctx);
  switch_user(session_1, user_privileged);
  if (!session_1)
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,  "Open session_1 failed.");
*/
  /* Opening session 2 */
  WRITE_STR("\nOpening Session 2\n");
  session_2 = srv_session_open(NULL, plugin_ctx);
  if (!session_2)
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "Open session_2 failed.");
  else
    switch_user(session_2, user_privileged);

  /* Statement 2 */
  WRITE_SEP();
  WRITE_STR("Session 1 : \n");
  WRITE_SEP();
  exec_test_cmd(session_1, "SELECT * FROM test.t_bigint", p, plugin_ctx);

  /* Statement 3 */
  WRITE_SEP();
  WRITE_STR("Session 2 : \n");
  WRITE_SEP();
  exec_test_cmd(session_2, "SELECT * FROM test.t_real", p, plugin_ctx);

  /* Close session 1 */
  WRITE_STR("\nClose Session 1\n");
  if (srv_session_close(session_1))
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "Close session_1 failed.");
  session_1 = nullptr;

  /* Statement 4 */
  WRITE_SEP();
  WRITE_STR("Session 2 : \n");
  WRITE_SEP();
  exec_test_cmd(session_2, "SELECT * FROM test.t_date", p, plugin_ctx);

  /* Statement 5 */
  WRITE_SEP();
  WRITE_STR("Session 1 (already closed): \n");
  WRITE_SEP();
  exec_test_cmd(session_1, "SELECT * FROM test.t_date", p, plugin_ctx);

  /* Close session 2 */
  WRITE_STR("\nClose Session 2\n");
  if (srv_session_close(session_2))
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "Close session_2 failed.");
  session_2 = nullptr;

  /* Close session 2 */
  WRITE_STR("\nClose Session 2 again\n");
  srv_session_close(session_2);

  delete plugin_ctx;
}

static void test_session_only_open(void *p [[maybe_unused]]) {
  DBUG_TRACE;

  MYSQL_SESSION sessions[MAX_SESSIONS];

  /* Disabled open without close as these 5 sessions stay open until server
     shutdow, That violates the rules of a valid regression test. */
  /* Open sessions: Must pass */
  for (int i = 0; i < nb_sessions; i++) {
    WRITE_VAL("srv_session_open %d\n", i);
    sessions[i] = srv_session_open(NULL, NULL);
    if (!sessions[i]) {
      LogPluginErrMsg(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                      "srv_session_open_%d failed.", i);
    } else {
      LogPluginErrMsg(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                      "Opened session %d .", i);
    }
  }
}

struct test_thread_context {
  my_thread_handle thread;
  void *p;
  bool thread_finished;
  void (*test_function)(void *);
};

static void create_log_file(const char *log_name) {
  char filename[FN_REFLEN];

  fn_format(filename, log_name, "", ".log",
            MY_REPLACE_EXT | MY_UNPACK_FILENAME);
  unlink(filename);
  outfile = my_open(filename, O_CREAT | O_RDWR, MYF(0));
}

static void *test_sql_threaded_wrapper(void *param) {
  struct test_thread_context *context = (struct test_thread_context *)param;

  WRITE_SEP();
  WRITE_STR("init thread\n");
  if (srv_session_init_thread(context->p))
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "srv_session_init_thread failed.");

  context->test_function(context->p);

  srv_session_deinit_thread();

  context->thread_finished = true;
  return nullptr;
}

static void test_in_spawned_thread(void *p, void (*test_function)(void *)) {
  my_thread_attr_t attr; /*  Thread attributes */
  my_thread_attr_init(&attr);
  (void)my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);

  struct test_thread_context context;

  context.p = p;
  context.thread_finished = false;
  context.test_function = test_function;

  /*   now create the thread and call test_session within the thread. */
  if (my_thread_create(&(context.thread), &attr, test_sql_threaded_wrapper,
                       &context) != 0)
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Could not create test session thread");
  else
    my_thread_join(&context.thread, nullptr);
}

static int test_session_service_plugin_init(void *p) {
  DBUG_TRACE;

  create_log_file(log_filename);

  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) return 1;
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Installation.");

  WRITE_SEP();
  WRITE_STR("Test in a server thread\n");
  test_sql(p);

  /* Test in a new thread */
  WRITE_STR("Follows threaded run\n");
  test_in_spawned_thread(p, test_sql);

  /* Test of service: sessions */
  WRITE_STR("Test in a server thread without closing sessions\n");
  test_session_only_open(p);

  return 0;
}

static int test_session_service_plugin_deinit(void *p [[maybe_unused]]) {
  DBUG_TRACE;
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Uninstallation.");
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
  my_close(outfile, MYF(0));
  return 0;
}

struct st_mysql_daemon test_session_service_plugin = {
    MYSQL_DAEMON_INTERFACE_VERSION};

/*
  Plugin library descriptor
*/

mysql_declare_plugin(test_daemon){
    MYSQL_DAEMON_PLUGIN,
    &test_session_service_plugin,
    "test_sql_errors",
    PLUGIN_AUTHOR_ORACLE,
    "Test error cases",
    PLUGIN_LICENSE_GPL,
    test_session_service_plugin_init,   /* Plugin Init      */
    nullptr,                            /* Plugin Check uninstall    */
    test_session_service_plugin_deinit, /* Plugin Deinit    */
    0x0100,                             /* 1.0              */
    nullptr,                            /* status variables */
    test_services_sysvars,              /* system variables */
    nullptr,                            /* config options   */
    0,                                  /* flags            */
} mysql_declare_plugin_end;
