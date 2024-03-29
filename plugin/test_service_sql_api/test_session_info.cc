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

#define LOG_COMPONENT_TAG "test_session_info"

#include <fcntl.h>
#include <mysql/plugin.h>
#include <stdlib.h>
#include <sys/types.h>

#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_psi_config.h"
#include "my_sys.h"  // my_write, my_malloc
#include "mysql/components/services/bits/psi_thread_bits.h"
#include "mysql/psi/mysql_thread.h"
#include "mysql_com.h"
#include "sql_string.h" /* STRING_PSI_MEMORY_KEY */
#include "template_utils.h"
#include "violite.h"

#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>

static const char *log_filename = "test_session_info";

static File outfile;

#define STRING_BUFFER_SIZE 1100

#define WRITE_STR(format)                                       \
  {                                                             \
    snprintf(buffer, sizeof(buffer), "%s", (format));           \
    my_write(outfile, (uchar *)buffer, strlen(buffer), MYF(0)); \
  }

#define WRITE_VAL(format, value)                                \
  {                                                             \
    snprintf(buffer, sizeof(buffer), (format), (value));        \
    my_write(outfile, (uchar *)buffer, strlen(buffer), MYF(0)); \
  }

#define WRITE_VAL2(format, value1, value2)                          \
  {                                                                 \
    snprintf(buffer, sizeof(buffer), (format), (value1), (value2)); \
    my_write(outfile, (uchar *)buffer, strlen(buffer), MYF(0));     \
  }

static const char *sep =
    "=========================================================================="
    "==================\n";

#define WRITE_SEP() \
  my_write(outfile, pointer_cast<const uchar *>(sep), strlen(sep), MYF(0))

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

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

/* Session declarations */
COM_DATA cmd;

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
  DBUG_PRINT("info", ("resultcs->name: %s", resultcs->m_coll_name));
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

static void get_data_str(void *ctx) {
  char buffer[STRING_BUFFER_SIZE];
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;

  /* get values */
  if ((pctx->num_rows > 0) && (pctx->num_cols > 0)) {
    for (uint col_count = 0; col_count < pctx->num_cols; col_count++) {
      WRITE_VAL("%s  ", pctx->sql_field[col_count].col_name);
    }
    WRITE_STR("\n");

    for (uint row_count = 0; row_count < pctx->num_rows; row_count++) {
      for (uint col_count = 0; col_count < pctx->num_cols; col_count++) {
        WRITE_VAL("%s  ", pctx->sql_str_value[row_count][col_count]);
      }
      WRITE_STR("\n");
    }
    WRITE_STR("\n");

    /* Metadata */
    WRITE_VAL("num_cols      : %d\n", pctx->num_cols);
    WRITE_VAL("nb rows       : %d\n", pctx->num_rows);
  }
}

static void handle_error(void *ctx) {
  char buffer[STRING_BUFFER_SIZE];
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  /* handle_ok/error */
  WRITE_VAL("error     : %d\n", pctx->sql_errno);
  WRITE_VAL("error msg : %s\n", pctx->err_msg);
}

#define EXEC_TEST_CMD(s, q, p, ctx) \
  exec_test_cmd((s), (q), (p), (ctx), false, __FUNCTION__, __LINE__)
#define EXEC_TEST_CMD_EX(s, q, p, ctx, err) \
  exec_test_cmd((s), (q), (p), (ctx), (err), __FUNCTION__, __LINE__)

static void exec_test_cmd(MYSQL_SESSION session, const char *query,
                          void *p [[maybe_unused]], void *ctx,
                          bool expect_error, const char *func, uint line) {
  char buffer[STRING_BUFFER_SIZE];
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  pctx->reset();
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_query.query = query;
  cmd.com_query.length = strlen(cmd.com_query.query);
  WRITE_VAL("%s\n", query);
  int fail = command_service_run_command(session, COM_QUERY, &cmd,
                                         &my_charset_utf8mb3_general_ci,
                                         &sql_cbs, CS_TEXT_REPRESENTATION, ctx);
  if (fail) {
    srv_session_close(session);
    if (!expect_error)
      LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                   "test_session_info - ret code : %d at %s:%u", fail, func,
                   line);
  } else if (pctx->sql_errno)
    handle_error(ctx);
  else if (expect_error)
    LogPluginErrMsg(
        ERROR_LEVEL, ER_LOG_PRINTF_MSG,
        "test_session_info - expected error but command did not fail at %s:%u",
        func, line);
  else {
    if (pctx->num_cols) get_data_str(ctx);
    WRITE_VAL("affected rows : %d\n", pctx->affected_rows);
    WRITE_VAL("server status : %d\n", pctx->server_status);
    WRITE_VAL("warn count    : %d\n", pctx->warn_count);
  }
  WRITE_STR("\n");
}

static void test_com_init_db(void *p [[maybe_unused]], MYSQL_SESSION st_session,
                             const char *db_name) {
  char buffer[STRING_BUFFER_SIZE];
  DBUG_TRACE;

  struct st_plugin_ctx *plugin_ctx = new st_plugin_ctx();

  COM_DATA cmd;

  LEX_CSTRING lex_db_name = srv_session_info_get_current_db(st_session);
  WRITE_VAL("current_db before init_db : %s\n", lex_db_name.str);

  cmd.com_init_db.db_name = db_name;
  cmd.com_init_db.length = strlen(db_name);

  int fail = command_service_run_command(
      st_session, COM_INIT_DB, &cmd, &my_charset_utf8mb3_general_ci, &sql_cbs,
      CS_TEXT_REPRESENTATION, plugin_ctx);

  if (fail) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "run_statement code: %d\n",
                 fail);
    delete plugin_ctx;
    return;
  }

  lex_db_name = srv_session_info_get_current_db(st_session);
  WRITE_VAL("current_db after init_db  : %s\n", lex_db_name.str);

  delete plugin_ctx;
}

static void test_sql(void *p) {
  char buffer[STRING_BUFFER_SIZE + 1];
  char buffer_query[STRING_BUFFER_SIZE];

  DBUG_TRACE;
  MYSQL_SESSION session_1, session_2, session_3;
  struct st_plugin_ctx *plugin_ctx = new st_plugin_ctx();

  /* Opening session 1 */
  WRITE_STR("Opening Session 1\n");
  session_1 = srv_session_open(NULL, plugin_ctx);
  if (!session_1)
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "Opening Session 1 failed");
  else
    switch_user(session_1, user_privileged);

  /* Opening session 2 */
  WRITE_STR("Opening Session 2\n");
  session_2 = srv_session_open(NULL, plugin_ctx);
  if (!session_2)
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "Opening Session 2 failed");
  else
    switch_user(session_2, user_privileged);

  /* srv_session_info_get_thd and srv_session_info_get_session_id*/
  /* Session 1 */
  WRITE_SEP();
  WRITE_STR(
      "Session 1 : srv_session_info_get_thd and "
      "srv_session_info_get_session_id\n");
  WRITE_SEP();
  my_thread_id session_1_id = srv_session_info_get_session_id(session_1);
  my_thread_id session_2_id = srv_session_info_get_session_id(session_2);
  MYSQL_THD thd = srv_session_info_get_thd(session_1);
  unsigned long thd_id = thd_get_thread_id(thd);
  if (thd_id != session_1_id) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Thread handler id is NOT equal to session id "
                 "srv_session_info_get_session_id(session_1)");
    return;
  } else {
    WRITE_STR(
        "Thread handler id IS equal to session id returned by "
        "srv_session_info_get_session_id(Session_1)\n\n");
  }

  /* Session 2 */
  WRITE_SEP();
  WRITE_STR(
      "Session 2 : srv_session_info_get_thd and "
      "srv_session_info_get_session_id\n");
  WRITE_SEP();
  thd = srv_session_info_get_thd(session_2);
  thd_id = thd_get_thread_id(thd);

  if (thd_id != session_2_id) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Thread handler id is NOT equal to session id "
                 "srv_session_info_get_session_id(session_2)");
    delete plugin_ctx;
    return;
  } else {
    WRITE_STR(
        "Thread handler id IS equal to session id returned by "
        "srv_session_info_get_session_id(Session_2)\n\n");
  }

  /* All information from performance_schema  */
  snprintf(buffer_query, sizeof(buffer_query),
           "SELECT "
           "name,type,processlist_id,processlist_user,processlist_host,"
           "processlist_db,processlist_command,processlist_state,processlist_"
           "info,`role`,instrumented,history,connection_type FROM "
           "performance_schema.threads WHERE processlist_id =  %u",
           session_1_id);
  EXEC_TEST_CMD(session_1, buffer_query, p, plugin_ctx);

  snprintf(buffer_query, sizeof(buffer_query),
           "SELECT "
           "name,type,processlist_id,processlist_user,processlist_host,"
           "processlist_db,processlist_command,processlist_state,processlist_"
           "info,`role`,instrumented,history,connection_type FROM "
           "performance_schema.threads WHERE processlist_id =  %u",
           session_2_id);
  EXEC_TEST_CMD(session_2, buffer_query, p, plugin_ctx);

  /* srv_session_info_get_current_db */
  /* Session 1 */
  WRITE_SEP();
  WRITE_STR("Session 1 : srv_session_info_get_current_db\n");
  WRITE_SEP();
  EXEC_TEST_CMD(session_1, "/*Session_1*/ SHOW TABLES LIKE '%slave%'", p,
                plugin_ctx);
  test_com_init_db(p, session_1, "mysql");
  WRITE_STR("\n");
  EXEC_TEST_CMD(session_1, "/*Session_1*/ SHOW TABLES LIKE '%slave%'", p,
                plugin_ctx);

  EXEC_TEST_CMD(session_1, "/*Session_1*/ USE information_schema", p,
                plugin_ctx);
  LEX_CSTRING db_name = srv_session_info_get_current_db(session_1);
  WRITE_VAL("current_db after 'USE db_name' command : %s\n\n", db_name.str);

  test_com_init_db(p, session_1, "test");
  WRITE_STR("\n");
  EXEC_TEST_CMD(session_1, "/*Session_1*/ SHOW TABLES", p, plugin_ctx);
  /* Session 2 */
  WRITE_SEP();
  WRITE_STR("Session 2 : srv_session_info_get_current_db\n");
  WRITE_SEP();
  EXEC_TEST_CMD(session_2, "/*Session_2*/ SHOW TABLES LIKE '%slave%'", p,
                plugin_ctx);
  test_com_init_db(p, session_2, "mysql");
  WRITE_STR("\n");
  WRITE_STR("Session 2's view\n");
  EXEC_TEST_CMD(session_2, "/*Session_2*/ SHOW TABLES LIKE '%slave%'", p,
                plugin_ctx);

  WRITE_STR("Session 2's view\n");
  EXEC_TEST_CMD(session_2, "/*Session_2*/ USE information_schema", p,
                plugin_ctx);
  db_name = srv_session_info_get_current_db(session_2);
  WRITE_VAL("current_db after 'USE db_name' command : %s\n\n", db_name.str);

  test_com_init_db(p, session_2, "test");
  WRITE_STR("\n");
  WRITE_STR("Session 2's view\n");
  EXEC_TEST_CMD(session_2, "/*Session_2*/ SHOW TABLES", p, plugin_ctx);

  /* srv_session_info_set/get_client_port */
  /* Session 1 */
  WRITE_SEP();
  WRITE_STR("Session 1 : srv_session_info_set/get_client_port\n");
  WRITE_SEP();
  WRITE_VAL("Port before srv_session_info_set_client_port : %d\n",
            srv_session_info_get_client_port(session_1));
  srv_session_info_set_client_port(session_1, 100);
  WRITE_VAL("Port after srv_session_info_set_client_port  : %d\n\n",
            srv_session_info_get_client_port(session_1));

  WRITE_STR("Session 1's view\n");
  EXEC_TEST_CMD(session_1,
                "/*Session_1*/ SELECT host FROM INFORMATION_SCHEMA.PROCESSLIST "
                "WHERE info LIKE 'PLUGIN%' ORDER BY id",
                p, plugin_ctx);
  WRITE_STR("Session 2's view\n");
  EXEC_TEST_CMD(session_2,
                "/*Session_2*/ SELECT host FROM INFORMATION_SCHEMA.PROCESSLIST "
                "WHERE info LIKE 'PLUGIN%' ORDER BY id",
                p, plugin_ctx);

  /* Session 2 */
  WRITE_SEP();
  WRITE_STR("Session 2 : srv_session_info_set/get_client_port\n");
  WRITE_SEP();
  WRITE_VAL("Port before srv_session_info_set_client_port : %d\n",
            srv_session_info_get_client_port(session_2));
  srv_session_info_set_client_port(session_2, 200);
  WRITE_VAL("Port after srv_session_info_set_client_port  : %d\n\n",
            srv_session_info_get_client_port(session_2));

  WRITE_STR("Session 1's view\n");
  EXEC_TEST_CMD(session_1,
                "/*Session_1*/ SELECT host FROM INFORMATION_SCHEMA.PROCESSLIST "
                "WHERE info LIKE 'PLUGIN%' ORDER BY id",
                p, plugin_ctx);
  WRITE_STR("Session 2's view\n");
  EXEC_TEST_CMD(session_2,
                "/*Session_2*/ SELECT host FROM INFORMATION_SCHEMA.PROCESSLIST "
                "WHERE info LIKE 'PLUGIN%' ORDER BY id",
                p, plugin_ctx);

  /* srv_session_info_set_connection_type */
  /* Session 1 */
  WRITE_SEP();
  WRITE_STR("Session 1 : srv_session_info_set_connection_type\n");
  WRITE_SEP();
  snprintf(buffer_query, sizeof(buffer_query),
           "SELECT CONNECTION_TYPE, CONNECTION_TYPE IS NULL FROM "
           "performance_schema.threads WHERE PROCESSLIST_ID =  %u "
           "/*session_1_id*/",
           session_1_id);
  WRITE_STR("Session 1's view\n");
  EXEC_TEST_CMD(session_1, buffer_query, p, plugin_ctx);

  WRITE_STR("Setting NO_VIO_TYPE on session_1\n");
  if (0 == srv_session_info_set_connection_type(session_1, NO_VIO_TYPE))
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "srv_session_info_set_connection_type(NO_VIO_TYPE) "
                 "should fail but did not");

  snprintf(buffer_query, sizeof(buffer_query),
           "SELECT CONNECTION_TYPE FROM performance_schema.threads WHERE "
           "PROCESSLIST_ID =  %u /*session_1_id*/",
           session_1_id);
  WRITE_STR("Session 1's view\n");
  EXEC_TEST_CMD(session_1, buffer_query, p, plugin_ctx);

  WRITE_STR("Setting VIO_TYPE_TCPIP on session_1\n");
  srv_session_info_set_connection_type(session_1, VIO_TYPE_TCPIP);
  snprintf(buffer_query, sizeof(buffer_query),
           "SELECT CONNECTION_TYPE FROM performance_schema.threads WHERE "
           "PROCESSLIST_ID =  %u /*session_1_id*/",
           session_1_id);
  WRITE_STR("Session 1's view\n");
  EXEC_TEST_CMD(session_1, buffer_query, p, plugin_ctx);

  WRITE_STR("Setting VIO_TYPE_NAMEDPIPE on session_1\n");
  srv_session_info_set_connection_type(session_1, VIO_TYPE_NAMEDPIPE);
  snprintf(buffer_query, sizeof(buffer_query),
           "SELECT CONNECTION_TYPE FROM performance_schema.threads WHERE "
           "PROCESSLIST_ID =  %u /*session_1_id*/",
           session_1_id);
  WRITE_STR("Session 1's view\n");
  EXEC_TEST_CMD(session_1, buffer_query, p, plugin_ctx);

  /* Session 2 */
  WRITE_SEP();
  WRITE_STR("Session 2 : srv_session_info_set_connection_type\n");
  WRITE_SEP();
  snprintf(buffer_query, sizeof(buffer_query),
           "SELECT CONNECTION_TYPE FROM performance_schema.threads WHERE "
           "PROCESSLIST_ID =  %u /*session_2_id*/",
           session_2_id);
  WRITE_STR("Session 2's view\n");
  EXEC_TEST_CMD(session_2, buffer_query, p, plugin_ctx);

  /* Now test with SSL/TLS */
  WRITE_STR("Setting VIO_TYPE_SSL on session_2\n");
  srv_session_info_set_connection_type(session_2, VIO_TYPE_SSL);
  /* Don't delete the following. We check if set of type on detached session
   * will affect PFS. The thread should be SSL */
  WRITE_STR("Setting VIO_TYPE_TCPIP on session_1\n");
  srv_session_info_set_connection_type(session_1, VIO_TYPE_TCPIP);
  snprintf(buffer_query, sizeof(buffer_query),
           "SELECT CONNECTION_TYPE FROM performance_schema.threads WHERE "
           "PROCESSLIST_ID =  %u /*session_2_id*/",
           session_2_id);
  WRITE_STR("Session 2's view\n");
  EXEC_TEST_CMD(session_2, buffer_query, p, plugin_ctx);

  srv_session_info_set_connection_type(session_2, VIO_TYPE_SHARED_MEMORY);
  snprintf(buffer_query, sizeof(buffer_query),
           "SELECT CONNECTION_TYPE FROM performance_schema.threads WHERE "
           "PROCESSLIST_ID =  %u  /*session_2_id*/",
           session_2_id);
  WRITE_STR("Session 2's view\n");
  EXEC_TEST_CMD(session_2, buffer_query, p, plugin_ctx);

  /* srv_session_info_killed */
  /* Session 1 */
  WRITE_SEP();
  WRITE_STR("BEFORE kill of Session 1\n");
  WRITE_SEP();
  EXEC_TEST_CMD(session_1,
                "SELECT ID, USER, HOST, DB, COMMAND, INFO FROM "
                "INFORMATION_SCHEMA.PROCESSLIST WHERE info LIKE 'PLUGIN%' "
                "ORDER BY id",
                p, plugin_ctx);
  WRITE_SEP();
  WRITE_VAL("srv_session_info_killed(Session_1) : %d\n",
            srv_session_info_killed(session_1));
  WRITE_VAL("srv_session_info_killed(Session_2) : %d\n",
            srv_session_info_killed(session_2));

  WRITE_SEP();
  WRITE_STR("Killing Session 1\n");
  snprintf(buffer_query, sizeof(buffer_query),
           "KILL CONNECTION %u /*session_1_id*/", session_1_id);
  EXEC_TEST_CMD(session_2, buffer_query, p, plugin_ctx);

  WRITE_SEP();
  WRITE_STR("AFTER kill of Session 1\n");
  WRITE_SEP();
  EXEC_TEST_CMD_EX(session_1,
                   "SELECT ID, USER, HOST, DB, COMMAND, INFO FROM "
                   "INFORMATION_SCHEMA.PROCESSLIST WHERE info LIKE 'PLUGIN%' "
                   "ORDER BY id",
                   p, plugin_ctx, true);
  session_1 = nullptr;  // session_1 is closed in EXEC_TEST_CMD_EX.
  WRITE_SEP();
  WRITE_VAL("srv_session_info_killed(Session 1) : %d\n",
            srv_session_info_killed(session_1));
  WRITE_VAL("srv_session_info_killed(Session 2) : %d\n",
            srv_session_info_killed(session_2));

  /* Close session 1 */
  WRITE_SEP();
  WRITE_STR("Closing Session 1\n");
  if (srv_session_close(session_1))
    WRITE_STR(
        "Closing Session 1 failed as expected. It was already closed by "
        "EXEC_TEST_CMD\n");

  WRITE_SEP();
  WRITE_STR("Get/Set session info with closed session(Session 1)\n");
  WRITE_SEP();

  db_name = srv_session_info_get_current_db(session_1);

  WRITE_VAL("srv_session_info_get_thd             : %d\n",
            (bool)srv_session_info_get_thd(session_1));
  WRITE_VAL("srv_session_info_get_session_id      : %d\n",
            srv_session_info_get_session_id(session_1));
  WRITE_VAL("srv_session_info_set_client_port     : %d\n",
            srv_session_info_set_client_port(session_1, 11111));
  WRITE_VAL("srv_session_info_get_client_port     : %d\n",
            srv_session_info_get_client_port(session_1));
  WRITE_VAL("srv_session_info_get_current_db      : %s\n", db_name.str);
  WRITE_VAL(
      "srv_session_info_set_connection_type : %d\n",
      srv_session_info_set_connection_type(session_1, VIO_TYPE_SHARED_MEMORY));
  WRITE_STR("\n");

  WRITE_SEP();
  EXEC_TEST_CMD_EX(session_1,
                   "SELECT ID, USER, HOST, DB, COMMAND, INFO FROM "
                   "INFORMATION_SCHEMA.PROCESSLIST WHERE info LIKE 'PLUGIN%' "
                   "ORDER BY id",
                   p, plugin_ctx, true);
  WRITE_SEP();
  WRITE_STR("Perform KILL QUERY and suicide (KILL CONNECTION) on Session 2\n");
  WRITE_SEP();
  snprintf(buffer_query, sizeof(buffer_query), "KILL QUERY %i /*session_2_id*/",
           session_2_id);
  WRITE_VAL("%s\n", buffer_query);
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_query.query = buffer_query;
  cmd.com_query.length = strlen(buffer_query);

  int fail = command_service_run_command(
      session_2, COM_QUERY, &cmd, &my_charset_utf8mb3_general_ci, &sql_cbs,
      CS_TEXT_REPRESENTATION, plugin_ctx);

  if (fail) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "run_statement code: %d\n",
                 fail);
    delete plugin_ctx;
    return;
  }

  WRITE_VAL("srv_session_info_killed(Session 2) : %d\n",
            srv_session_info_killed(session_2));

  snprintf(buffer_query, sizeof(buffer_query),
           "KILL CONNECTION %i  /*session_2_id*/", session_2_id);
  WRITE_VAL("%s\n", buffer_query);
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_query.query = buffer_query;
  cmd.com_query.length = strlen(buffer_query);

  fail = command_service_run_command(session_2, COM_QUERY, &cmd,
                                     &my_charset_utf8mb3_general_ci, &sql_cbs,
                                     CS_TEXT_REPRESENTATION, plugin_ctx);

  if (fail) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "run_statement code: %d\n",
                 fail);
    delete plugin_ctx;
    return;
  }

  WRITE_VAL("srv_session_info_killed(Session 2) : %d\n",
            srv_session_info_killed(session_2));

  db_name = srv_session_info_get_current_db(session_2);

  WRITE_SEP();
  WRITE_STR("Get/Set session info with killed session(Session 2)\n");
  WRITE_SEP();
  WRITE_VAL("srv_session_info_get_thd             : %d\n",
            (bool)srv_session_info_get_thd(session_2));
  WRITE_VAL("srv_session_info_get_session_id      : %d\n",
            srv_session_info_get_session_id(session_2));
  WRITE_VAL("srv_session_info_set_client_port     : %d\n",
            srv_session_info_set_client_port(session_2, 11111));
  WRITE_VAL("srv_session_info_get_client_port     : %d\n",
            srv_session_info_get_client_port(session_2));
  WRITE_VAL("srv_session_info_get_current_db      : %s\n", db_name.str);
  WRITE_VAL(
      "srv_session_info_set_connection_type : %d\n",
      srv_session_info_set_connection_type(session_2, VIO_TYPE_SHARED_MEMORY));
  WRITE_STR("\n");

  session_3 = srv_session_open(NULL, plugin_ctx);
  if (!session_3)
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "Opening Session 3 failed");
  else {
    switch_user(session_3, user_privileged);

    WRITE_SEP();
    WRITE_STR(
        "Session 2 got killed but not closed, thus it will appear in the "
        "processlist as Killed\n");
    WRITE_SEP();
    EXEC_TEST_CMD(session_3,
                  "/*Session 3*/SELECT ID, USER, HOST, DB, COMMAND, INFO FROM "
                  "INFORMATION_SCHEMA.PROCESSLIST WHERE info LIKE 'PLUGIN%' "
                  "ORDER BY id",
                  p, plugin_ctx);

    WRITE_STR("Closing Session 2\n");
    if (srv_session_close(session_2))
      LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "Closing Session 2 failed");

    WRITE_STR("Closing Session 3\n");
    if (srv_session_close(session_3))
      LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "Closing Session 3 failed");
  }

  delete plugin_ctx;
}

struct test_thread_context {
  my_thread_handle thread;
  void *p;
  bool thread_finished;
  void (*test_function)(void *);
};

static void *test_sql_threaded_wrapper(void *param) {
  char buffer[STRING_BUFFER_SIZE];
  struct test_thread_context *context = (struct test_thread_context *)param;

  WRITE_SEP();
  WRITE_STR("init thread\n");
  if (srv_session_init_thread(context->p))
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "srv_session_init_thread failed.");

  context->test_function(context->p);

  WRITE_STR("deinit thread\n");
  srv_session_deinit_thread();

  context->thread_finished = true;
  return nullptr;
}

static void create_log_file(const char *log_name) {
  char filename[FN_REFLEN];

  fn_format(filename, log_name, "", ".log",
            MY_REPLACE_EXT | MY_UNPACK_FILENAME);
  unlink(filename);
  outfile = my_open(filename, O_CREAT | O_RDWR, MYF(0));
}

#ifdef HAVE_PSI_INTERFACE
static PSI_thread_key key_thread_session_info = PSI_NOT_INSTRUMENTED;
static PSI_thread_info session_info_threads[] = {
    {&key_thread_session_info, "session_info", "session_info", 0, 0,
     PSI_DOCUMENT_ME}};
#endif  // HAVE_PSI_INTERFACE

static void test_in_spawned_thread(void *p, void (*test_function)(void *)) {
  my_thread_attr_t attr; /* Thread attributes */
  my_thread_attr_init(&attr);
  (void)my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);

  struct test_thread_context context;

  context.p = p;
  context.thread_finished = false;
  context.test_function = test_function;

  /* now create the thread and call test_session within the thread. */
  if (mysql_thread_create(key_thread_session_info, &(context.thread), &attr,
                          test_sql_threaded_wrapper, &context) != 0)
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Could not create test session thread");
  else
    my_thread_join(&context.thread, nullptr);
}

static int test_sql_service_plugin_init(void *p) {
  char buffer[STRING_BUFFER_SIZE];
  DBUG_TRACE;
  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) return 1;
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Installation.");

  create_log_file(log_filename);

#ifdef HAVE_PSI_INTERFACE
  const char *const category = "test_service_sql";

  mysql_thread_register(category, session_info_threads,
                        static_cast<int>(array_elements(session_info_threads)));
#endif  // HAVE_PSI_INTERFACE

  WRITE_SEP();
  WRITE_STR("Test in a server thread\n");
  test_sql(p);

  /* Test in a new thread */
  WRITE_STR("Follows threaded run\n");
  test_in_spawned_thread(p, test_sql);

  my_close(outfile, MYF(0));

  return 0;
}

static int test_sql_service_plugin_deinit(void *p [[maybe_unused]]) {
  DBUG_TRACE;
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Uninstallation.");
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
  return 0;
}

struct st_mysql_daemon test_sql_service_plugin = {
    MYSQL_DAEMON_INTERFACE_VERSION};

/*
  Plugin library descriptor
*/

mysql_declare_plugin(test_daemon){
    MYSQL_DAEMON_PLUGIN,
    &test_sql_service_plugin,
    "test_session_info",
    PLUGIN_AUTHOR_ORACLE,
    "Test session information",
    PLUGIN_LICENSE_GPL,
    test_sql_service_plugin_init,   /* Plugin Init      */
    nullptr,                        /* Plugin Check uninstall */
    test_sql_service_plugin_deinit, /* Plugin Deinit    */
    0x0100,                         /* 1.0              */
    nullptr,                        /* status variables */
    nullptr,                        /* system variables */
    nullptr,                        /* config options   */
    0,                              /* flags            */
} mysql_declare_plugin_end;
