/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#define LOG_COMPONENT_TAG "test_sql_reset_connection"

#include <fcntl.h>
#include <mysql/plugin.h>
#include <stdlib.h>
#include <sys/types.h>
#include <memory>

#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/udf_registration.h>
#include <mysql/service_srv_session_info.h>
#include <mysqld_error.h>

#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"  // my_write, my_malloc
#include "mysql_com.h"
#include "template_utils.h"

#define STRING_BUFFER 256

static File outfile;

static void WRITE_STR(const char *format) {
  char buffer[STRING_BUFFER];
  snprintf(buffer, sizeof(buffer), "%s", format);
  my_write(outfile, (uchar *)buffer, strlen(buffer), MYF(0));
}

template <typename T>
void WRITE_VAL(const char *format, T value) {
  char buffer[STRING_BUFFER];
  snprintf(buffer, sizeof(buffer), format, value);
  my_write(outfile, (uchar *)buffer, strlen(buffer), MYF(0));
}

template <typename T1, typename T2>
void WRITE_VAL2(const char *format, T1 value1, T2 value2) {
  char buffer[STRING_BUFFER];
  snprintf(buffer, sizeof(buffer), format, value1, value2);
  my_write(outfile, (uchar *)buffer, strlen(buffer), MYF(0));
}

static const char *sep =
    "=======================================================================\n";

#define WRITE_SEP() \
  my_write(outfile, pointer_cast<const uchar *>(sep), strlen(sep), MYF(0))

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

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
  st_send_field_n sql_field[8];
  char sql_str_value[8][8][256];
  size_t sql_str_len[8][8];

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
    memset(&sql_field, 0, 8 * sizeof(st_send_field_n));
    memset(&sql_str_value, 0, 8 * 8 * 256 * sizeof(char));
    memset(&sql_str_len, 0, 8 * 8 * sizeof(size_t));

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
  auto pctx = (struct st_plugin_ctx *)ctx;
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
  auto pctx = (struct st_plugin_ctx *)ctx;
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

  return false;
}

static int sql_get_double(void *ctx, double value, uint32 /*decimals*/) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  DBUG_TRACE;
  uint row = pctx->num_rows;
  uint col = pctx->current_col;
  pctx->current_col++;

  size_t len = snprintf(pctx->sql_str_value[row][col],
                        sizeof(pctx->sql_str_value[row][col]), "%3.7g", value);

  pctx->sql_str_len[row][col] = len;

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

  return false;
}

static int sql_get_time(void *ctx, const MYSQL_TIME *value, uint /*decimals*/) {
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

  return false;
}

static int sql_get_datetime(void *ctx, const MYSQL_TIME *value,
                            uint /*decimals*/) {
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

static void get_data_str(struct st_plugin_ctx *pctx) {
  WRITE_STR(
      "-----------------------------------------------------------------\n");
  for (uint col = 0; col < pctx->num_cols; col++) {
    WRITE_VAL("%s ", pctx->sql_field[col].col_name);
    WRITE_VAL2("%s(%u)\t", fieldtype2str(pctx->sql_field[col].type),
               pctx->sql_field[col].type);
  }
  WRITE_STR("\n");

  for (uint row = 0; row < pctx->num_rows; row++) {
    for (uint col = 0; col < pctx->num_cols; col++) {
      WRITE_VAL2("%s%s", pctx->sql_str_value[row][col],
                 col < pctx->num_cols - 1 ? "\t\t\t" : "\n");
    }
  }
}

static void query_execute(MYSQL_SESSION session, st_plugin_ctx *pctx,
                          const std::string &query) {
  WRITE_VAL("%s\n", query.c_str());
  pctx->reset();

  COM_DATA cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_query.query = query.c_str();
  cmd.com_query.length = query.size();
  if (command_service_run_command(session, COM_QUERY, &cmd,
                                  &my_charset_utf8mb3_general_ci, &sql_cbs,
                                  CS_TEXT_REPRESENTATION, pctx)) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "fail query execution - %d:%s",
                 pctx->sql_errno, pctx->err_msg);
    return;
  }
  if (pctx->num_cols) get_data_str(pctx);
}

struct Thread_data {
  void *p;
  void (*proc)(void *p);
};

static void *test_session_thread(void *ctxt) {
  auto thread_data = (Thread_data *)ctxt;

  if (srv_session_init_thread(thread_data->p))
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "srv_session_init_thread failed.");

  thread_data->proc(thread_data->p);

  srv_session_deinit_thread();

  return nullptr;
}

void test_execute_in_thread(void *p, void (*proc)(void *p)) {
  Thread_data thread_data{p, proc};

  my_thread_handle thread_handle;
  my_thread_attr_t attr;
  my_thread_attr_init(&attr);
  (void)my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);

  if (my_thread_create(&thread_handle, &attr,
                       (void *(*)(void *))test_session_thread,
                       &thread_data) != 0) {
    WRITE_STR("Could not create test services thread!\n");
    exit(1);
  }
  void *ret;
  my_thread_join(&thread_handle, &ret);
}

static void ensure_api_ok(const char *function, int result) {
  if (result != 0) {
    WRITE_VAL2("ERROR calling %s: returned %i\n", function, result);
  }
}

static void ensure_api_not_null(const char *function, void *result) {
  if (!result) {
    WRITE_VAL("ERROR calling %s: returned NULL\n", function);
  }
}

#define ENSURE_API_OK(call) ensure_api_ok(__FUNCTION__, (call));
#define ENSURE_API_NOT_NULL(call) ensure_api_not_null(__FUNCTION__, (call));

static void reset_connection(MYSQL_SESSION st_session, st_plugin_ctx *pctx) {
  COM_DATA cmd;
  ENSURE_API_OK(command_service_run_command(
      st_session, COM_RESET_CONNECTION, &cmd, &my_charset_utf8mb3_general_ci,
      &sql_cbs, CS_TEXT_REPRESENTATION, pctx));
}

static void session_error_cb(void *, unsigned int sql_errno,
                             const char *err_msg) {
  WRITE_STR("default error handler called\n");
  WRITE_VAL("sql_errno = %i\n", sql_errno);
  WRITE_VAL("errmsg = %s\n", err_msg);
}

static void test_com_reset_connection(void *p) {
  DBUG_TRACE;

  WRITE_STR("COM_RESET_CONNECTION\n");

  MYSQL_SESSION st_session;
  ENSURE_API_NOT_NULL(st_session = srv_session_open(session_error_cb, p));

  my_thread_id session_id = srv_session_info_get_session_id(st_session);

  std::unique_ptr<st_plugin_ctx> ctx(new st_plugin_ctx());
  query_execute(st_session, ctx.get(), "set @secret = 123");
  query_execute(st_session, ctx.get(), "select @secret");
  reset_connection(st_session, ctx.get());
  query_execute(st_session, ctx.get(), "select @secret");

  WRITE_VAL("Has session ID changed: %i\n",
            srv_session_info_get_session_id(st_session) != session_id);

  ENSURE_API_OK(srv_session_close(st_session));
}

static void test_com_reset_connection_from_another_session(void *p) {
  DBUG_TRACE;

  WRITE_STR("COM_RESET_CONNECTION from another session\n");

  MYSQL_SESSION st_session;
  ENSURE_API_NOT_NULL(st_session = srv_session_open(NULL, p));

  my_thread_id session_id = srv_session_info_get_session_id(st_session);

  std::unique_ptr<st_plugin_ctx> ctx(new st_plugin_ctx());
  query_execute(st_session, ctx.get(), "set @another_secret = 456");
  query_execute(st_session, ctx.get(), "select @another_secret");
  WRITE_STR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
  query_execute(st_session, ctx.get(), "do reset_connection()");
  WRITE_STR("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
  query_execute(st_session, ctx.get(), "select @another_secret");

  WRITE_VAL("Has session ID changed: %i\n",
            srv_session_info_get_session_id(st_session) != session_id);

  ENSURE_API_OK(srv_session_close(st_session));
}

static void test_sql(void *p) {
  DBUG_TRACE;

  WRITE_SEP();
  test_execute_in_thread(p, test_com_reset_connection);
  WRITE_SEP();
  test_execute_in_thread(p, test_com_reset_connection_from_another_session);
  WRITE_SEP();
}

static void create_log_file(const char *log_name) {
  char filename[FN_REFLEN];

  fn_format(filename, log_name, "", ".log",
            MY_REPLACE_EXT | MY_UNPACK_FILENAME);
  unlink(filename);
  outfile = my_open(filename, O_CREAT | O_RDWR, MYF(0));
}

static const char *log_filename = "test_sql_reset_connection";

namespace {
void *plg = nullptr;

using Udf_registrator = my_service<SERVICE_TYPE(udf_registration)>;

bool reset_connection_init(UDF_INIT *, UDF_ARGS *args, char *) {
  return args->arg_count != 0;
}

long long reset_connection_exe(UDF_INIT *, UDF_ARGS *, unsigned char *,
                               unsigned char *) {
  DBUG_TRACE;
  test_execute_in_thread(plg, test_com_reset_connection);
  return 0;
}

void register_udf_reset_connection() {
  DBUG_TRACE;
  auto reg = mysql_plugin_registry_acquire();
  {
    Udf_registrator udf_reg{"udf_registration", reg};
    if (udf_reg.is_valid()) {
      udf_reg->udf_register(
          "reset_connection", INT_RESULT,
          reinterpret_cast<Udf_func_any>(reset_connection_exe),
          reset_connection_init, nullptr);
    } else {
      LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "fail udf registartion");
    }
  }
  mysql_plugin_registry_release(reg);
}

void unregister_udf_reset_connection() {
  DBUG_TRACE;
  auto reg = mysql_plugin_registry_acquire();
  {
    Udf_registrator udf_reg{"udf_registration", reg};
    if (udf_reg.is_valid()) {
      int was_present = 0;
      udf_reg->udf_unregister("reset_connection", &was_present);
    }
  }
  mysql_plugin_registry_release(reg);
}
}  // namespace

static int test_sql_service_plugin_init(void *p) {
  create_log_file(log_filename);
  DBUG_TRACE;
  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) return 1;
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Installation.");

  plg = p;
  register_udf_reset_connection();

  /* Test of service: sql */
  test_sql(p);

  return 0;
}

static int test_sql_service_plugin_deinit(void *p [[maybe_unused]]) {
  DBUG_TRACE;
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Uninstallation.");

  unregister_udf_reset_connection();

  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
  my_close(outfile, MYF(0));
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
    "test_sql_reset_connection",
    PLUGIN_AUTHOR_ORACLE,
    "Test sql reset connection",
    PLUGIN_LICENSE_GPL,
    test_sql_service_plugin_init,   /* Plugin Init */
    nullptr,                        /* Plugin Check uninstall */
    test_sql_service_plugin_deinit, /* Plugin Deinit */
    0x0100 /* 1.0 */,
    nullptr, /* status variables                */
    nullptr, /* system variables                */
    nullptr, /* config options                  */
    0,       /* flags                           */
} mysql_declare_plugin_end;
