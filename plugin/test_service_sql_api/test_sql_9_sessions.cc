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

#define LOG_COMPONENT_TAG "test_sql_9_sessions"

#include <mysql/plugin.h>
#include <stdlib.h>

#include "my_sys.h"  // my_write, my_malloc

#define STRING_BUFFER 256

#define WRITE_STR(format)                         \
  snprintf(buffer, sizeof(buffer), "%s", format); \
  my_write(outfile, (uchar *)buffer, strlen(buffer), MYF(0))
#define WRITE_VAL(format, value)                   \
  snprintf(buffer, sizeof(buffer), format, value); \
  my_write(outfile, (uchar *)buffer, strlen(buffer), MYF(0))
#define WRITE_VAL_2(format, value1, value2)                 \
  snprintf(buffer, sizeof(buffer), format, value1, value2); \
  my_write(outfile, (uchar *)buffer, strlen(buffer), MYF(0))

File outfile;

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

char buffer[STRING_BUFFER];
const CHARSET_INFO *sql_resultcs = NULL;
uint sql_num_meta_rows = 0;
uint sql_num_rows = 0;
uint col_count = 0;
uint sql_num_cols = 0;
uint sql_flags = 0;
st_send_field_n sql_field[64];
MYSQL_SESSION session[9];
void *plugin_ctx = NULL;
bool session_ret = false;
bool fail = false;
COM_DATA cmd;

int row_count = 0;
static int sql_start_result_metadata(void *ctx, uint num_cols, uint flags,
                                     const CHARSET_INFO *resultcs) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("resultcs->number: %d", resultcs->number));
  DBUG_PRINT("info", ("resultcs->csname: %s", resultcs->csname));
  DBUG_PRINT("info", ("resultcs->name: %s", resultcs->name));
  row_count = 0;
  sql_num_cols = num_cols;
  sql_resultcs = resultcs;
  return false;
};

static int sql_field_metadata(void *ctx, struct st_send_field *field,
                              const CHARSET_INFO *charset) {
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
  strcpy(sql_field[col_count].db_name, (char *)field->db_name);
  strcpy(sql_field[col_count].table_name, (char *)field->table_name);
  strcpy(sql_field[col_count].org_table_name, (char *)field->org_table_name);
  strcpy(sql_field[col_count].col_name, (char *)field->col_name);
  strcpy(sql_field[col_count].org_col_name, (char *)field->org_col_name);
  sql_field[col_count].length = field->length;
  sql_field[col_count].charsetnr = field->charsetnr;
  sql_field[col_count].flags = field->flags;
  sql_field[col_count].decimals = field->decimals;
  sql_field[col_count].type = field->type;
  col_count++;
  return false;
};

static int sql_end_result_metadata(void *ctx, uint server_status,
                                   uint warn_count) {
  DBUG_TRACE;
  sql_num_meta_rows = row_count;
  row_count = 0;
  return false;
};

static int sql_start_row(void *ctx) {
  DBUG_TRACE;
  col_count = 0;
  return false;
};

static int sql_end_row(void *ctx) {
  DBUG_TRACE;
  row_count++;
  return false;
};

static void sql_abort_row(void *ctx) {
  DBUG_TRACE;
  col_count = 0;
};

static ulong sql_get_client_capabilities(void *ctx) {
  DBUG_TRACE;
  return 0;
};

static int sql_get_null(void *ctx) {
  DBUG_TRACE;
  return false;
};

longlong test_int[64][64];

static int sql_get_integer(void *ctx, longlong value) {
  DBUG_TRACE;
  test_int[col_count][row_count] = value;
  col_count++;
  return false;
};

longlong test_longlong[64][64];
uint test_is_unsigned[64][64];

static int sql_get_longlong(void *ctx, longlong value, uint is_unsigned) {
  DBUG_TRACE;
  test_longlong[col_count][row_count] = value;
  test_is_unsigned[col_count][row_count] = is_unsigned;
  col_count++;
  return false;
};

struct st_test_decimal_t {
  int intg, frac, len;
  bool sign;
  decimal_digit_t buf[256];
} test_decimal[64][64];
char test_dec_str[256][64][64];

static int sql_get_decimal(void *ctx, const decimal_t *value) {
  DBUG_TRACE;
  test_decimal[col_count][row_count].intg = value->intg;
  test_decimal[col_count][row_count].frac = value->frac;
  test_decimal[col_count][row_count].len = value->len;
  test_decimal[col_count][row_count].sign = value->sign;
  memset((void *)test_decimal[col_count][row_count].buf, '\0', (int)value->len);
  memcpy((void *)test_decimal[col_count][row_count].buf, (void *)value->buf,
         (int)value->len);
  //  decimal2string(value, test_dec_str[col_count][row_count],
  //                (int*)&value->len, value->frac,
  //                value->intg+value->frac,'\0');
  col_count++;
  return false;
};

double test_double[64][64];
uint32 test_decimals[64][64];
static int sql_get_double(void *ctx, double value, uint32 decimals) {
  DBUG_TRACE;
  test_double[col_count][row_count] = value;
  test_decimals[col_count][row_count] = decimals;
  col_count++;
  return false;
};

struct st_test_date {
  unsigned int year, month, day, hour, minute, second;
  unsigned long second_part; /**< microseconds */
  bool neg;
  enum enum_mysql_timestamp_type time_type;
} test_date[64][64];

static int sql_get_date(void *ctx, const MYSQL_TIME *value) {
  DBUG_TRACE;
  test_date[col_count][row_count].year = value->year;
  test_date[col_count][row_count].month = value->month;
  test_date[col_count][row_count].day = value->day;

  test_date[col_count][row_count].hour = value->hour;
  test_date[col_count][row_count].minute = value->minute;
  test_date[col_count][row_count].second = value->second;
  test_date[col_count][row_count].second_part = value->second_part;
  test_date[col_count][row_count].neg = value->neg;
  col_count++;
  return false;
};

struct st_my_time {
  unsigned int year, month, day, hour, minute, second;
  unsigned long second_part; /**< microseconds */
  bool neg;
  enum enum_mysql_timestamp_type time_type;
  uint decimals;
} test_time[64][64];

static int sql_get_time(void *ctx, const MYSQL_TIME *value, uint decimals) {
  DBUG_TRACE;
  test_time[col_count][row_count].year = value->year;
  test_time[col_count][row_count].month = value->month;
  test_time[col_count][row_count].day = value->day;

  test_time[col_count][row_count].hour = value->hour;
  test_time[col_count][row_count].minute = value->minute;
  test_time[col_count][row_count].second = value->second;
  test_time[col_count][row_count].second_part = value->second_part;
  test_time[col_count][row_count].neg = value->neg;
  test_time[col_count][row_count].decimals = decimals;
  col_count++;
  return false;
};

struct st_my_datetime {
  unsigned int year, month, day, hour, minute, second;
  unsigned long second_part; /**< microseconds */
  bool neg;
  enum enum_mysql_timestamp_type time_type;
  uint decimals;
} test_datetime[64][64];

static int sql_get_datetime(void *ctx, const MYSQL_TIME *value, uint decimals) {
  DBUG_TRACE;
  test_datetime[col_count][row_count].year = value->year;
  test_datetime[col_count][row_count].month = value->month;
  test_datetime[col_count][row_count].day = value->day;

  test_datetime[col_count][row_count].hour = value->hour;
  test_datetime[col_count][row_count].minute = value->minute;
  test_datetime[col_count][row_count].second = value->second;
  test_datetime[col_count][row_count].second_part = value->second_part;
  test_datetime[col_count][row_count].neg = value->neg;
  test_datetime[col_count][row_count].decimals = decimals;
  col_count++;
  return false;
};

char sql_str_value[1024][64][64];
size_t sql_str_len[64][64];
static int sql_get_string(void *ctx, const char *const value, size_t length,
                          const CHARSET_INFO *const valuecs) {
  DBUG_TRACE;
  strncpy(sql_str_value[col_count][row_count], value, length);
  sql_str_len[col_count][row_count] = length;
  col_count++;
  return false;
};

uint sql_server_status = 0;
char msg[1024];
uint sql_warn_count = 0;
ulonglong sql_affected_rows = 0;
static void sql_handle_ok(void *ctx, uint server_status,
                          uint statement_warn_count, ulonglong affected_rows,
                          ulonglong last_insert_id, const char *const message) {
  DBUG_TRACE;
  sql_server_status = server_status;
  //  strcpy(msg,(char*)message);
  sql_affected_rows = affected_rows;
  sql_num_rows = row_count;
  row_count = 0;
};

uint sql_error = 0;
char sql_errormsg[128];
char sql_state[64];
static void sql_handle_error(void *ctx, uint sql_errno,
                             const char *const err_msg,
                             const char *const sqlstate) {
  DBUG_TRACE;
  sql_error = sql_errno;
  if (sql_error) {
    strcpy(sql_errormsg, (char *)err_msg);
    strcpy(sql_state, (char *)sqlstate);
  }
  sql_num_rows = row_count;
  row_count = 0;
};

static void sql_shutdown(void *ctx, int shutdown_server) { DBUG_TRACE; };

struct st_protocol_cbs sql_cbs = {sql_start_result_metadata,
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
                                  sql_shutdown};

const st_protocol_cbs *p_sql_cbs = &sql_cbs;
MYSQL_PROTOCOL select_prot;

/****************************************************************************************/

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

static void get_data_str() {
  /* start metadata */
  WRITE_VAL("num_cols: %d\n", sql_num_cols);
  /* get values */
  WRITE_STR(
      "-----------------------------------------------------------------\n");
  WRITE_VAL("nb rows: %d\n", sql_num_rows);
  for (uint col_count = 0; col_count < sql_num_cols; col_count++) {
    WRITE_VAL("%s  ", sql_field[col_count].col_name);
  }
  WRITE_STR("\n");
  for (uint col_count = 0; col_count < sql_num_cols; col_count++) {
    WRITE_VAL_2("%s(%u) ", fieldtype2str(sql_field[col_count].type),
                sql_field[col_count].type);
  }
  WRITE_STR("\n");
  WRITE_STR("Write a string\n");
  for (uint row_count = 0; row_count < sql_num_rows; row_count++) {
    for (uint col_count = 0; col_count < sql_num_cols; col_count++) {
      WRITE_VAL("%s  ", sql_str_value[col_count][row_count]);
    }
    WRITE_STR("\n");
  }
  WRITE_STR("\n");
}

static void handle_error() {
  /* handle_ok/error */
  if (sql_error) {
    WRITE_VAL("error: %d\n", sql_error);
    WRITE_VAL("error msg: %s\n", sql_errormsg);
  } else {
    WRITE_VAL("affected rows: %d\n", sql_affected_rows);
    WRITE_VAL("server status: %d\n", sql_server_status);
    WRITE_VAL("warn count: %d\n", sql_warn_count);
    //           WRITE_VAL("message: %s\n",msg);
  }
}

static void exec_test_cmd(MYSQL_SESSION session, const char *test_cmd,
                          void *p) {
  WRITE_VAL("%s\n", test_cmd);
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_query.query = (char *)test_cmd;
  cmd.com_query.length = strlen(cmd.com_query.query);
  fail = command_service_run_command(session, select_prot, COM_QUERY, &cmd,
                                     &my_charset_utf8mb3_general_ci);
  if (fail)
    LogPluginErrMsg(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                    "test_sql_9_sessions - ret code : %d", fail);
  else {
    if (ctx->num_cols) get_data_str();
    handle_error();
  }
}

/****************************************************************************************/

int test_sql(void *p) {
  DBUG_TRACE;

  /* Open session 1 */
  WRITE_STR(
      "-----------------------------------------------------------------\n");
  WRITE_STR("Open session_1\n");
  session_1 = srv_session_open(NULL, plugin_ctx);
  if (session_1 == 0) {
    WRITE_STR("open session_1 failed.\n");
  }
  select_prot = command_service_init_protocol(p_sql_cbs, CS_TEXT_REPRESENTATION,
                                              plugin_ctx);
  /* 1. statement */
  WRITE_STR(
      "-----------------------------------------------------------------\n");
  WRITE_STR("Session 1: ");
  exec_test_cmd(session_1, "SELECT * FROM test.t_int", p);
  /* Open session 2 */
  WRITE_STR(
      "-----------------------------------------------------------------\n");
  WRITE_STR("Open session_2\n");
  WRITE_STR("Session 2: Open\n");
  session_2 = srv_session_open(NULL, plugin_ctx);
  if (session_2 == 0) {
    WRITE_STR("open session_2 failed.\n");
  }
  /* 2. statement */
  WRITE_STR(
      "-----------------------------------------------------------------\n");
  WRITE_STR("Session 1: ");
  exec_test_cmd(session_1, "SELECT * FROM test.t_bigint", p);
  /* 3. statement */
  WRITE_STR(
      "-----------------------------------------------------------------\n");
  WRITE_STR("Session 2: ");
  exec_test_cmd(session_2, "SELECT * FROM test.t_real", p);
  /* close session 1 */
  WRITE_STR(
      "-----------------------------------------------------------------\n");
  WRITE_STR("close session_1.\n");
  session_ret = srv_session_close(session_1);
  if (session_ret) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "Close session_1 failed.");
  }
  session_1 = nullptr;
  /* 4. statement */
  WRITE_STR(
      "-----------------------------------------------------------------\n");
  WRITE_STR("Session 2: ");
  exec_test_cmd(session_2, "SELECT * FROM test.t_date", p);
  /* free protocol */
  /* 4. statement */
  WRITE_STR(
      "-----------------------------------------------------------------\n");
  WRITE_STR("Session 1: ");
  exec_test_cmd(session_1, "SELECT * FROM test.t_date", p);
  /* free protocol */
  WRITE_STR(
      "-----------------------------------------------------------------\n");
  WRITE_STR("command_service_free protocol.\n");
  command_service_free_protocol(select_prot);
  /* close session 2 */
  WRITE_STR(
      "-----------------------------------------------------------------\n");
  WRITE_STR("close session_2.\n");
  session_ret = srv_session_close(session_2);
  if (session_ret) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "Close session_2 failed.");
  }
  return session_ret;
}

static void create_log_file(const char *log_name) {
  char filename[FN_REFLEN];

  fn_format(filename, log_name, "", ".log",
            MY_REPLACE_EXT | MY_UNPACK_FILENAME);
  unlink(filename);
  outfile = my_open(filename, O_CREAT | O_RDWR, MYF(0));
}

static int test_sql_service_plugin_init(void *p) {
  DBUG_TRACE;
  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) return 1;
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Installation.");

  create_log_file("test_sql_2_sessions");

  /* Test of service: sql */
  test_sql(p);

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
    "test_sql_9_sessions",
    PLUGIN_AUTHOR_ORACLE,
    "Test sql in 9 parallel sessions",
    PLUGIN_LICENSE_GPL,
    test_sql_service_plugin_init,   /* Plugin Init */
    test_sql_service_plugin_deinit, /* Plugin Deinit */
    0x0100 /* 1.0 */,
    NULL, /* status variables                */
    NULL, /* system variables                */
    NULL, /* config options                  */
    0,    /* flags                           */
} mysql_declare_plugin_end;
