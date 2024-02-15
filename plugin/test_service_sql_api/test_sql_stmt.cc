/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#define LOG_COMPONENT_TAG "test_sql_stmt"

#include <fcntl.h>
#include <mysql/plugin.h>
#include <stdlib.h>
#include <sys/types.h>
#include <vector>

#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>

#include "m_string.h"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"  // my_write, my_malloc
#include "mysql_com.h"
#include "sql_string.h" /* STRING_PSI_MEMORY_KEY */
#include "template_utils.h"

struct CHARSET_INFO;

/* purecov: begin inspected */
static const char *log_filename = "test_sql_stmt";

#define STRING_BUFFER_SIZE 1024
#define LARGE_STRING_BUFFER_SIZE 1024

#define WRITE_STR(format)                                                 \
  {                                                                       \
    const size_t blen = snprintf(buffer, sizeof(buffer), "%s", (format)); \
    my_write(outfile, (uchar *)buffer, blen, MYF(0));                     \
    /*pctx->log.append(buffer, blen); */                                  \
  }

#define WRITE_VAL(format, value)                                             \
  {                                                                          \
    const size_t blen = snprintf(buffer, sizeof(buffer), (format), (value)); \
    my_write(outfile, (uchar *)buffer, blen, MYF(0));                        \
    /* pctx->log.append(buffer, blen); */                                    \
  }

#define WRITE_VAL2(format, value1, value2)                              \
  {                                                                     \
    const size_t blen =                                                 \
        snprintf(buffer, sizeof(buffer), (format), (value1), (value2)); \
    my_write(outfile, (uchar *)buffer, blen, MYF(0));                   \
    /* pctx->log.append(buffer, blen); */                               \
  }

static const char *sep =
    "========================================================================"
    "\n";

#define WRITE_SEP() \
  my_write(outfile, pointer_cast<const uchar *>(sep), strlen(sep), MYF(0))

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;
static File outfile;

#define SIZEOF_SQL_STR_VALUE 256

static void print_cmd(enum_server_command cmd, COM_DATA *data);
static char *fieldflags2str(uint f);
static const char *fieldtype2str(enum enum_field_types type);
static void dump_decoded_server_status(const char *prefix, uint server_status);

class Column {
 public:
  Column(const char *db_name, const char *table_name,
         const char *org_table_name, const char *col_name,
         const char *org_col_name, unsigned long length, unsigned int charsetnr,
         unsigned int flags, unsigned int decimals, enum_field_types type)
      : db_name(db_name),
        table_name(table_name),
        org_table_name(org_table_name),
        col_name(col_name),
        org_col_name(org_col_name),
        length(length),
        charsetnr(charsetnr),
        flags(flags),
        decimals(decimals),
        type(type) {}

  std::vector<std::string> row_values;
  std::string db_name;
  std::string table_name;
  std::string org_table_name;
  std::string col_name;
  std::string org_col_name;
  unsigned long length;
  unsigned int charsetnr;
  unsigned int flags;
  unsigned int decimals;
  enum_field_types type;

  void dump_column_meta() {
    char buffer[STRING_BUFFER_SIZE];

    WRITE_VAL("\t\t[meta][field] db name: %s\n", db_name.c_str());
    WRITE_VAL("\t\t[meta][field] table name: %s\n", table_name.c_str());
    WRITE_VAL("\t\t[meta][field] org table name: %s\n", org_table_name.c_str());
    WRITE_VAL("\t\t[meta][field] col name: %s\n", col_name.c_str());
    WRITE_VAL("\t\t[meta][field] org col name: %s\n", org_col_name.c_str());
    WRITE_VAL("\t\t[meta][field] length: %u\n", (uint)length);
    WRITE_VAL("\t\t[meta][field] charsetnr: %u\n", charsetnr);

    WRITE_VAL("\t\t[meta][field] flags: %u", flags);
    if (flags) WRITE_VAL(" (%s)", fieldflags2str(flags));
    WRITE_STR("\n");

    WRITE_VAL("\t\t[meta][field] decimals: %u\n", decimals);

    WRITE_VAL2("\t\t[meta][field] type: %s (%u)\n", fieldtype2str(type), type);
    WRITE_STR("\n");
  }

  void dump_row(size_t row_number) {
    char buffer[STRING_BUFFER_SIZE];
    WRITE_VAL2("\t\t[data][%s.%s]", table_name.c_str(), col_name.c_str());
    WRITE_VAL2("[%3u][%s]\n", (uint)row_values[row_number].length(),
               row_values[row_number].c_str());
  }
};

class Table {
 public:
  uint num_cols;
  uint num_rows;
  const CHARSET_INFO *cs_info;
  std::vector<Column> columns;

 public:
  Table(uint num_cols, const CHARSET_INFO *cs_info)
      : num_cols(num_cols), num_rows(0), cs_info(cs_info) {}

  void dump_table() {
    char buffer[STRING_BUFFER_SIZE];

    if (!num_cols) {
      WRITE_STR("\t[meta] no columns\n");
      return;
    }
    for (auto &&column : columns) column.dump_column_meta();

    WRITE_STR("\n");
    if (!cs_info) {
      WRITE_STR("\t[meta] no charset\n");
      return;
    } else {
      WRITE_VAL("\t[meta][charset result] number: %d\n", cs_info->number);
      WRITE_VAL("\t[meta][charset result] name: %s\n", cs_info->csname);
      WRITE_VAL("\t[meta][charset result] collation: %s\n",
                cs_info->m_coll_name);
      WRITE_VAL("\t[meta][charset result] sort order: %s\n",
                cs_info->sort_order);
      WRITE_STR("\n");
    }

    for (size_t i = 0; i < num_rows; i++) {
      size_t col = 0;
      for (auto &&column : columns) {
        WRITE_VAL("\t[meta] current col: %zu\n", col);
        col++;
        column.dump_row(i);
      }
      WRITE_STR("\n");
    }
  }
};

class Server_context {
 public:
  std::vector<Table> tables;
  uint current_col;
  uint current_row;

  ulong stmt_id;
  enum_server_command cmd;

  uint server_status;
  uint warn_count;
  uint affected_rows;
  uint last_insert_id;
  std::string message;

  uint sql_errno;
  std::string err_msg;
  std::string sqlstate;

  std::string log;

  Server_context()
      : current_col(0),
        current_row(0),
        server_status(0),
        warn_count(0),
        affected_rows(0),
        last_insert_id(0),
        sql_errno(0) {
    err_msg.clear();
    sqlstate.clear();
    message.clear();
    log.clear();
  }
  void dump_closing_ok() {
    char buffer[STRING_BUFFER_SIZE];

    dump_decoded_server_status("\t[end] server status: ", server_status);
    WRITE_VAL("\t[end] warning count:  %u\n", warn_count);
    WRITE_VAL("\t[end] affected rows:  %u\n", affected_rows);
    WRITE_VAL("\t[end] last insert id: %u\n", last_insert_id);
    WRITE_VAL("\t[end] message: %s\n", message.c_str());
  }

  void dump_closing_error() {
    char buffer[STRING_BUFFER_SIZE];

    WRITE_VAL2("[%u][%s]", sql_errno, sqlstate.c_str());
    WRITE_VAL("[%s]\n", err_msg.c_str());
  }
};

static void dump_decoded_server_status(const char *prefix, uint server_status) {
  char buffer[STRING_BUFFER_SIZE];
  WRITE_STR(prefix);
  WRITE_VAL("%u\n", server_status);
  WRITE_STR(prefix);
  for (int i = 0; i < 30; i++) {
    const uint flag = 1 << i;
    if (server_status & flag) {
#define FLAG_DELIMITER " "
      switch (flag) {
        case SERVER_STATUS_IN_TRANS:
          WRITE_STR("IN_TRANS" FLAG_DELIMITER);
          break;
        case SERVER_STATUS_AUTOCOMMIT:
          WRITE_STR("AUTOCOMMIT" FLAG_DELIMITER);
          break;
        case SERVER_MORE_RESULTS_EXISTS:
          WRITE_STR("MORE_RESULTS_EXISTS" FLAG_DELIMITER);
          break;
        case SERVER_QUERY_NO_GOOD_INDEX_USED:
          WRITE_STR("QUERY_NO_GOOD_INDEX_USED" FLAG_DELIMITER);
          break;
        case SERVER_QUERY_NO_INDEX_USED:
          WRITE_STR("QUERY_NO_INDEX_USED" FLAG_DELIMITER);
          break;
        case SERVER_STATUS_CURSOR_EXISTS:
          WRITE_STR("CURSOR_EXISTS" FLAG_DELIMITER);
          break;
        case SERVER_STATUS_LAST_ROW_SENT:
          WRITE_STR("LAST_ROW_SENT" FLAG_DELIMITER);
          break;
        case SERVER_STATUS_DB_DROPPED:
          WRITE_STR("DB_DROPPED" FLAG_DELIMITER);
          break;
        case SERVER_STATUS_NO_BACKSLASH_ESCAPES:
          WRITE_STR("NO_BACKSLASH_ESCAPES" FLAG_DELIMITER);
          break;
        case SERVER_STATUS_METADATA_CHANGED:
          WRITE_STR("METADATA_CHANGED" FLAG_DELIMITER);
          break;
        case SERVER_QUERY_WAS_SLOW:
          WRITE_STR("QUERY_WAS_SLOW" FLAG_DELIMITER);
          break;
        case SERVER_PS_OUT_PARAMS:
          WRITE_STR("PS_OUT_PARAMS" FLAG_DELIMITER);
          break;
        case SERVER_STATUS_IN_TRANS_READONLY:
          WRITE_STR("IN_TRANS_READONLY" FLAG_DELIMITER);
          break;
        case SERVER_SESSION_STATE_CHANGED:
          WRITE_STR("STATE_CHANGED" FLAG_DELIMITER);
          break;
        default:
          // Add a new flag defined in mysql_com.h above to fix this
          WRITE_VAL("UNKNOWN_%u\n", flag);
      }
#undef FLAG_DELIMITER
    }
  }
  WRITE_STR("\n");
}

static int handle_start_column_metadata(void *pctx, uint num_cols, uint,
                                        const CHARSET_INFO *resultcs) {
  Server_context *ctx = (Server_context *)pctx;
  char buffer[STRING_BUFFER_SIZE];
  WRITE_STR("handle_start_column_metadata\n");
  DBUG_TRACE;
  DBUG_PRINT("info", ("resultcs->number: %d", resultcs->number));
  DBUG_PRINT("info", ("resultcs->csname: %s", resultcs->csname));
  DBUG_PRINT("info", ("resultcs->m_coll_name: %s", resultcs->m_coll_name));

  ctx->tables.push_back(Table(num_cols, resultcs));
  ctx->current_col = 0;

  return false;
}

static int handle_send_column_metadata(void *pctx, struct st_send_field *field,
                                       const CHARSET_INFO *) {
  Server_context *ctx = (Server_context *)pctx;
  //  char buffer[STRING_BUFFER_SIZE];
  //  WRITE_STR("handle_send_column_metadata\n");
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

  ctx->tables.back().columns.push_back(
      Column(field->db_name, field->table_name, field->org_table_name,
             field->col_name, field->org_col_name, field->length,
             field->charsetnr, field->flags, field->decimals, field->type));
  ctx->current_col++;
  return false;
}

static int handle_end_column_metadata(void *pctx, uint server_status,
                                      uint warn_count) {
  char buffer[STRING_BUFFER_SIZE];
  Server_context *ctx = (Server_context *)pctx;
  DBUG_TRACE;
  ctx->server_status = server_status;
  ctx->warn_count = warn_count;

  ctx->current_row = 0;

  WRITE_STR("handle_end_column_metadata\n");
  return false;
}

static int handle_start_row(void *pctx) {
  Server_context *ctx = (Server_context *)pctx;
  char buffer[STRING_BUFFER_SIZE];
  WRITE_STR("handle_start_row\n");
  DBUG_TRACE;
  ctx->current_col = 0;
  return false;
}

static int handle_end_row(void *pctx) {
  Server_context *ctx = (Server_context *)pctx;
  char buffer[STRING_BUFFER_SIZE];
  DBUG_TRACE;
  WRITE_STR("handle_end_row\n");

  // Get the generated statement id
  if (ctx->cmd == COM_STMT_PREPARE && ctx->current_row == 0 &&
      ctx->tables.size() == 1 && ctx->tables[0].columns.size() == 4 &&
      ctx->tables[0].columns[0].row_values.size() == 1) {
    ctx->stmt_id =
        std::stoul(ctx->tables[0].columns[0].row_values[0], nullptr, 10);
  }
  ctx->tables.back().num_rows++;
  ctx->current_row++;
  return false;
}

static void handle_abort_row(void *) {
  char buffer[STRING_BUFFER_SIZE];
  WRITE_STR("handle_abort_row\n");
  DBUG_TRACE;
}

static ulong get_client_capabilities(void *) {
  DBUG_TRACE;
  return CLIENT_PS_MULTI_RESULTS | CLIENT_MULTI_RESULTS;
}

static int handle_store_null(void *pctx) {
  Server_context *ctx = (Server_context *)pctx;
  //  WRITE_STR("handle_store_null\n");
  DBUG_TRACE;
  const uint col = ctx->current_col;
  ctx->current_col++;
  ctx->tables.back().columns[col].row_values.push_back("[NULL]");

  return false;
}

static int handle_store_integer(void *pctx, longlong value) {
  char buffer[LARGE_STRING_BUFFER_SIZE];
  Server_context *ctx = (Server_context *)pctx;
  DBUG_TRACE;
  const uint col = ctx->current_col;
  ctx->current_col++;

  const size_t len = snprintf(buffer, sizeof(buffer), "%lld", value);

  ctx->tables.back().columns[col].row_values.push_back(
      std::string(buffer, len));

  return false;
}

static int handle_store_longlong(void *pctx, longlong value, uint is_unsigned) {
  char buffer[LARGE_STRING_BUFFER_SIZE];
  Server_context *ctx = (Server_context *)pctx;
  DBUG_TRACE;
  const uint col = ctx->current_col;
  ctx->current_col++;

  const size_t len =
      snprintf(buffer, sizeof(buffer), is_unsigned ? "%llu" : "%lld", value);

  ctx->tables.back().columns[col].row_values.push_back(
      std::string(buffer, len));

  return false;
}

static const char *test_decimal_as_string(char *buff, const decimal_t *val,
                                          int *length) {
  if (!val) return "NULL";
  (void)decimal2string(val, buff, length);
  return buff;
}

static int handle_store_decimal(void *pctx, const decimal_t *value) {
  char buffer[LARGE_STRING_BUFFER_SIZE]{};
  Server_context *ctx = (Server_context *)pctx;
  DBUG_TRACE;
  const uint col = ctx->current_col;
  ctx->current_col++;

  int len = SIZEOF_SQL_STR_VALUE;
  test_decimal_as_string(buffer, value, &len);
  ctx->tables.back().columns[col].row_values.push_back(
      std::string(buffer, len));

  return false;
}

static int handle_store_double(void *pctx, double value, uint32) {
  char buffer[LARGE_STRING_BUFFER_SIZE];
  Server_context *ctx = (Server_context *)pctx;
  DBUG_TRACE;
  const uint col = ctx->current_col;
  ctx->current_col++;

  const size_t len = snprintf(buffer, sizeof(buffer), "%3.7g", value);
  ctx->tables.back().columns[col].row_values.push_back(
      std::string(buffer, len));

  return false;
}

static int handle_store_date(void *pctx, const MYSQL_TIME *value) {
  char buffer[LARGE_STRING_BUFFER_SIZE];
  Server_context *ctx = (Server_context *)pctx;
  DBUG_TRACE;
  const uint col = ctx->current_col;
  ctx->current_col++;

  const size_t len =
      snprintf(buffer, sizeof(buffer), "%s%4d-%02d-%02d", value->neg ? "-" : "",
               value->year, value->month, value->day);

  ctx->tables.back().columns[col].row_values.push_back(
      std::string(buffer, len));

  return false;
}

static int handle_store_time(void *pctx, const MYSQL_TIME *value, uint) {
  char buffer[LARGE_STRING_BUFFER_SIZE];
  Server_context *ctx = (Server_context *)pctx;
  DBUG_TRACE;
  const uint col = ctx->current_col;
  ctx->current_col++;

  const size_t len = snprintf(
      buffer, sizeof(buffer), "%s%02d:%02d:%02d", value->neg ? "-" : "",
      value->day ? (value->day * 24 + value->hour) : value->hour, value->minute,
      value->second);
  ctx->tables.back().columns[col].row_values.push_back(
      std::string(buffer, len));
  return false;
}

static int handle_store_datetime(void *pctx, const MYSQL_TIME *value, uint) {
  char buffer[LARGE_STRING_BUFFER_SIZE];
  Server_context *ctx = (Server_context *)pctx;
  DBUG_TRACE;
  const uint col = ctx->current_col;
  ctx->current_col++;

  const size_t len =
      snprintf(buffer, sizeof(buffer), "%s%4d-%02d-%02d %02d:%02d:%02d",
               value->neg ? "-" : "", value->year, value->month, value->day,
               value->hour, value->minute, value->second);

  ctx->tables.back().columns[col].row_values.push_back(
      std::string(buffer, len));

  return false;
}

static int handle_store_string(void *pctx, const char *const value,
                               size_t length, const CHARSET_INFO *const) {
  Server_context *ctx = (Server_context *)pctx;
  DBUG_TRACE;
  const uint col = ctx->current_col;
  ctx->current_col++;

  ctx->tables.back().columns[col].row_values.push_back(
      std::string(value, length));

  return false;
}

static void handle_ok(void *pctx, uint server_status, uint statement_warn_count,
                      ulonglong affected_rows, ulonglong last_insert_id,
                      const char *const message) {
  Server_context *ctx = (Server_context *)pctx;
  char buffer[STRING_BUFFER_SIZE];
  WRITE_STR("handle_ok\n");
  DBUG_TRACE;
  ctx->sql_errno = 0;
  ctx->sqlstate.clear();
  ctx->err_msg.clear();
  /* This could be an EOF */
  ctx->server_status = server_status;
  ctx->warn_count = statement_warn_count;
  ctx->affected_rows = affected_rows;
  ctx->last_insert_id = last_insert_id;
  if (message) ctx->message.assign(message);

  WRITE_STR("<<<<<<<<<<<< Current context >>>>>>>>>>>>>>>\n");
  for (auto &&table : ctx->tables) {
    table.dump_table();
  }
  ctx->dump_closing_ok();
  WRITE_STR("<<<<<<<<<<<<>>>>>>>>>>>>>>>\n");
}

static void handle_error(void *pctx, uint sql_errno, const char *const err_msg,
                         const char *const sqlstate) {
  char buffer[LARGE_STRING_BUFFER_SIZE];
  Server_context *ctx = (Server_context *)pctx;
  WRITE_STR("handle_error\n");
  DBUG_TRACE;
  /// was setting current_row size to 0...
  if (!ctx->tables.empty()) ctx->tables.pop_back();

  ctx->sql_errno = sql_errno;
  ctx->sqlstate.assign(sqlstate);
  ctx->err_msg.assign(err_msg);

  ctx->dump_closing_error();
}

static void handle_shutdown(void *, int) {
  char buffer[STRING_BUFFER_SIZE];
  WRITE_STR("handle_shutdown\n");
  DBUG_TRACE;
}

const struct st_command_service_cbs protocol_callbacks = {
    handle_start_column_metadata,
    handle_send_column_metadata,
    handle_end_column_metadata,
    handle_start_row,
    handle_end_row,
    handle_abort_row,
    get_client_capabilities,
    handle_store_null,
    handle_store_integer,
    handle_store_longlong,
    handle_store_decimal,
    handle_store_double,
    handle_store_date,
    handle_store_time,
    handle_store_datetime,
    handle_store_string,
    handle_ok,
    handle_error,
    handle_shutdown,
    nullptr,
};

/****************************************************************************************/
#define WRITE_DASHED_LINE() \
  WRITE_STR(                \
      "------------------------------------------------------------------\n");

#define WRITE_HASHED_LINE() \
  WRITE_STR(                \
      "##################################################################\n");

static const char *fieldtype2str(enum enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_BIT:
      return "BIT";
    case MYSQL_TYPE_BLOB:
      return "BLOB";
    case MYSQL_TYPE_BOOL:
      return "BOOL";
    case MYSQL_TYPE_DATE:
      return "DATE";
    case MYSQL_TYPE_DATETIME:
      return "DATETIME";
    case MYSQL_TYPE_INVALID:
      return "?-invalid-?";
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
  static char buf[LARGE_STRING_BUFFER_SIZE];
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

static void set_query_in_com_data(union COM_DATA *cmd, const char *query) {
  memset(cmd, 0, sizeof(union COM_DATA));
  cmd->com_query.query = query;
  cmd->com_query.length = strlen(query);
}

static void run_cmd(MYSQL_SESSION session, enum_server_command cmd,
                    COM_DATA *data, Server_context *ctx,
                    bool generates_result_set, void *p [[maybe_unused]]) {
  char buffer[STRING_BUFFER_SIZE];
  WRITE_DASHED_LINE();

  enum cs_text_or_binary txt_or_bin = CS_TEXT_REPRESENTATION;

  WRITE_STR("[CS_TEXT_REPRESENTATION]\n");
again:
  print_cmd(cmd, data);
  ctx->cmd = cmd;
  const int fail = command_service_run_command(
      session, cmd, data, &my_charset_utf8mb3_general_ci, &protocol_callbacks,
      txt_or_bin, ctx);
  if (fail) {
    LogPluginErrMsg(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "run_statement code: %d\n",
                    fail);
    return;
  }

  if (generates_result_set && txt_or_bin == CS_TEXT_REPRESENTATION) {
    txt_or_bin = CS_BINARY_REPRESENTATION;
    WRITE_STR("[CS_BINARY_REPRESENTATION]\n");
    goto again;
  }
  WRITE_DASHED_LINE();
}

static void print_cmd(enum_server_command cmd, COM_DATA *data) {
  char buffer[STRING_BUFFER_SIZE];
  switch (cmd) {
    case COM_INIT_DB:
      WRITE_VAL("COM_INIT_DB: db_name[%s]\n", data->com_init_db.db_name);
      break;
    case COM_QUERY:
      WRITE_VAL("COM_QUERY: query[%s]\n", data->com_query.query);
      break;
    case COM_STMT_PREPARE:
      WRITE_VAL("COM_STMT_PREPARE: query[%s]\n", data->com_stmt_prepare.query);
      break;
    case COM_STMT_EXECUTE:
      WRITE_VAL("COM_STMT_EXECUTE: stmt_id [%lu]\n",
                data->com_stmt_execute.stmt_id);
      break;
    case COM_STMT_SEND_LONG_DATA:
      WRITE_VAL("COM_STMT_SEND_LONG_DATA: stmt_id [%lu]\n",
                data->com_stmt_send_long_data.stmt_id);
      break;
    case COM_STMT_CLOSE:
      WRITE_VAL("COM_STMT_CLOSE: stmt_id [%u]\n", data->com_stmt_close.stmt_id);
      break;
    case COM_STMT_RESET:
      WRITE_VAL("COM_STMT_RESET: stmt_id [%u]\n", data->com_stmt_reset.stmt_id);
      break;
    case COM_STMT_FETCH:
      WRITE_VAL("COM_STMT_FETCH: stmt_id [%lu]\n",
                data->com_stmt_fetch.stmt_id);
      break;
    default:
      WRITE_STR("NOT FOUND: add command to print_cmd\n");
  }
}

static void setup_test(MYSQL_SESSION session, void *p) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];

  Server_context ctx;
  COM_DATA cmd;

  WRITE_STR("CHANGE DATABASE\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_init_db.db_name = "test";
  cmd.com_init_db.length = strlen("test");
  run_cmd(session, COM_INIT_DB, &cmd, &ctx, false, p);

  WRITE_STR("CREATE TABLE\n");
  set_query_in_com_data(&cmd,
                        "CREATE TABLE t1 (a INT, b INT, c INT, UNIQUE (A), "
                        "UNIQUE(B))");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  WRITE_STR("INSERT VALUES INTO THE TABLE\n");
  set_query_in_com_data(&cmd,
                        "INSERT INTO t1 VALUES"
                        "(1, 12, 1111), (2, 11, 2222),"
                        "(3, 10, 3333), (4, 9, 4444),"
                        "(5, 8, 5555), (6, 7, 6666),"
                        "(7, 6, 7777), (8, 5, -1111),"
                        "(9, 4, -2222), (10, 3, -3333),"
                        "(11, 2, -4444), (12, 1, -5555)");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  set_query_in_com_data(&cmd,
                        "CREATE PROCEDURE proc_set_out_params("
                        "   OUT v_str_1 CHAR(32), "
                        "   OUT v_dbl_1 DOUBLE(4, 2), "
                        "   OUT v_dec_1 DECIMAL(6, 3), "
                        "   OUT v_int_1 INT)"
                        "BEGIN "
                        "   SET v_str_1 = 'test_1'; "
                        "   SET v_dbl_1 = 12.34; "
                        "   SET v_dec_1 = 567.891; "
                        "   SET v_int_1 = 2345; "
                        "END");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  set_query_in_com_data(
      &cmd,
      "CREATE PROCEDURE verify_user_variables_are_null(v_str_1 CHAR(32), "
      "   v_dbl_1 DOUBLE(4, 2), "
      "   v_dec_1 DECIMAL(6, 3), "
      "   v_int_1 INT)"
      "BEGIN "
      "DECLARE unexpected CONDITION FOR SQLSTATE '45000'; "
      " IF v_str_1 is not null THEN "
      "   SIGNAL unexpected; "
      " ELSEIF v_dbl_1 is not null THEN "
      "   SIGNAL unexpected; "
      " ELSEIF v_dec_1 is not null THEN "
      "   SIGNAL unexpected; "
      " ELSEIF v_int_1 is not null THEN "
      "   SIGNAL unexpected; "
      " END IF;"
      "END");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  set_query_in_com_data(
      &cmd,
      "CREATE PROCEDURE verify_user_variables_are_set(v_str_1 CHAR(32), "
      "   v_dbl_1 DOUBLE(4, 2), "
      "   v_dec_1 DECIMAL(6, 3), "
      "   v_int_1 INT)"
      "BEGIN "
      "DECLARE unexpected CONDITION FOR SQLSTATE '45000'; "
      " IF v_str_1 != 'test_1' THEN "
      "   SIGNAL unexpected; "
      " ELSEIF v_dbl_1 != 12.34 THEN "
      "   SIGNAL unexpected; "
      " ELSEIF v_dec_1 != 567.891 THEN "
      "   SIGNAL unexpected; "
      " ELSEIF v_int_1 != 2345 THEN "
      "   SIGNAL unexpected; "
      " END IF;"
      "END");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);
}

static void test_1(MYSQL_SESSION session, void *p) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];

  Server_context ctx;
  COM_DATA cmd;

  WRITE_STR("CREATE PREPARED STATEMENT\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_prepare.query = "SELECT * from t1 where a > ? and b < ?";
  cmd.com_stmt_prepare.length = strlen(cmd.com_stmt_prepare.query);
  run_cmd(session, COM_STMT_PREPARE, &cmd, &ctx, false, p);

  WRITE_STR("EXECUTE PREPARED STATEMENT WITH PARAMETERS AND CURSOR\n");

  PS_PARAM params[2];
  memset(params, 0, sizeof(params));
  params[0].type = MYSQL_TYPE_STRING;
  params[0].unsigned_type = false;
  params[0].null_bit = false;
  params[0].value = (const unsigned char *)"5";
  params[0].length = 1;

  params[1].type = MYSQL_TYPE_STRING;
  params[1].unsigned_type = false;
  params[1].null_bit = false;
  params[1].value = (const unsigned char *)"20";
  params[1].length = 2;

  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_execute.stmt_id = ctx.stmt_id;
  cmd.com_stmt_execute.open_cursor = true;
  cmd.com_stmt_execute.has_new_types = false;
  cmd.com_stmt_execute.parameters = params;
  cmd.com_stmt_execute.parameter_count = 2;
  cmd.com_stmt_execute.has_new_types = true;

  run_cmd(session, COM_STMT_EXECUTE, &cmd, &ctx, false, p);

  WRITE_STR("EXECUTE PREPARED STATEMENT WITH WRONG NO OF PARAM\n");
  cmd.com_stmt_execute.parameter_count = 1;
  run_cmd(session, COM_STMT_EXECUTE, &cmd, &ctx, false, p);

  WRITE_STR("FETCH ONE ROW FROM THE CURSOR\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_fetch.stmt_id = ctx.stmt_id;
  cmd.com_stmt_fetch.num_rows = 1;
  run_cmd(session, COM_STMT_FETCH, &cmd, &ctx, false, p);

  WRITE_STR("FETCH TWO ROWS FROM THE CURSOR\n");
  cmd.com_stmt_fetch.num_rows = 2;
  run_cmd(session, COM_STMT_FETCH, &cmd, &ctx, false, p);

  WRITE_STR("CLOSE THE STATEMENT\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_close.stmt_id = ctx.stmt_id;
  run_cmd(session, COM_STMT_CLOSE, &cmd, &ctx, false, p);

  WRITE_STR("CLOSE NON-EXISTING STATEMENT\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_close.stmt_id = 100001;
  run_cmd(session, COM_STMT_CLOSE, &cmd, &ctx, false, p);

  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_fetch.stmt_id = ctx.stmt_id;
  cmd.com_stmt_fetch.num_rows = 1;
  WRITE_STR("TRY TO FETCH ONE ROW FROM A DEALLOCATED(CLOSED) PS\n");
  run_cmd(session, COM_STMT_FETCH, &cmd, &ctx, false, p);
}

static void test_2(MYSQL_SESSION session, void *p) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];

  Server_context ctx;
  COM_DATA cmd;

  WRITE_STR("CREATE PREPARED STATEMENT\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_prepare.query = "SELECT * from t1 where a > ? and b < ?";
  cmd.com_stmt_prepare.length = strlen(cmd.com_stmt_prepare.query);
  run_cmd(session, COM_STMT_PREPARE, &cmd, &ctx, false, p);

  PS_PARAM params[2];
  memset(params, 0, sizeof(params));
  params[0].type = MYSQL_TYPE_STRING;
  params[0].unsigned_type = false;
  params[0].null_bit = false;
  params[0].value = (const unsigned char *)"4";
  params[0].length = 1;

  params[1].type = MYSQL_TYPE_STRING;
  params[1].unsigned_type = false;
  params[1].null_bit = false;
  params[1].value = (const unsigned char *)"7";
  params[1].length = 1;

  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_execute.stmt_id = ctx.stmt_id;
  cmd.com_stmt_execute.parameters = params;
  cmd.com_stmt_execute.parameter_count = 2;
  cmd.com_stmt_execute.has_new_types = true;
  cmd.com_stmt_execute.open_cursor = true;

  WRITE_STR("EXECUTE THE PS FOR OPEN CURSOR\n");
  run_cmd(session, COM_STMT_EXECUTE, &cmd, &ctx, false, p);

  WRITE_STR("FETCH ONE ROW\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_fetch.stmt_id = ctx.stmt_id;
  cmd.com_stmt_fetch.num_rows = 1;
  run_cmd(session, COM_STMT_FETCH, &cmd, &ctx, false, p);

  WRITE_STR("RESET THE STATEMENT\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_reset.stmt_id = ctx.stmt_id;
  run_cmd(session, COM_STMT_RESET, &cmd, &ctx, false, p);

  WRITE_STR("RESET NON-EXISTING STATEMENT\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_reset.stmt_id = 199999;
  run_cmd(session, COM_STMT_RESET, &cmd, &ctx, false, p);

  WRITE_STR("TRY TO FETCH ONE ROW FROM THE PS WITH REMOVED CURSOR\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_fetch.num_rows = 1;
  cmd.com_stmt_fetch.stmt_id = ctx.stmt_id;
  run_cmd(session, COM_STMT_FETCH, &cmd, &ctx, false, p);

  WRITE_STR("CLOSE THE STATEMENT\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_close.stmt_id = ctx.stmt_id;
  run_cmd(session, COM_STMT_CLOSE, &cmd, &ctx, false, p);
}

static void test_3(MYSQL_SESSION session, void *p) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];

  Server_context ctx;
  COM_DATA cmd;

  WRITE_STR("CREATE PREPARED STATEMENT\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_prepare.query = "SELECT * from t1 where a > ? and b > ?";
  cmd.com_stmt_prepare.length = strlen(cmd.com_stmt_prepare.query);
  run_cmd(session, COM_STMT_PREPARE, &cmd, &ctx, false, p);

  PS_PARAM params[2];
  memset(params, 0, sizeof(params));
  params[0].type = MYSQL_TYPE_STRING;
  params[0].unsigned_type = false;
  params[0].null_bit = false;
  params[0].value = (const unsigned char *)"2";
  params[0].length = 1;

  params[1].type = MYSQL_TYPE_STRING;
  params[1].unsigned_type = false;
  params[1].null_bit = false;
  params[1].value = (const unsigned char *)"3";
  params[1].length = 1;

  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_execute.stmt_id = ctx.stmt_id;
  cmd.com_stmt_execute.parameter_count = 2;
  cmd.com_stmt_execute.parameters = params;
  cmd.com_stmt_execute.open_cursor = false;
  cmd.com_stmt_execute.has_new_types = true;

  WRITE_STR("EXECUTE THE PS WITHOUT CURSOR\n");
  run_cmd(session, COM_STMT_EXECUTE, &cmd, &ctx, false, p);

  WRITE_STR("TRY TO FETCH ONE ROW FROM A PS WITHOUT CURSOR\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_fetch.num_rows = 1;
  cmd.com_stmt_fetch.stmt_id = ctx.stmt_id;
  run_cmd(session, COM_STMT_FETCH, &cmd, &ctx, false, p);

  WRITE_STR("TRY TO RESET THE CURSOR FROM A PS WITHOUT CURSOR\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_reset.stmt_id = ctx.stmt_id;
  run_cmd(session, COM_STMT_RESET, &cmd, &ctx, false, p);

  WRITE_STR("TRY TO CLOSE THE CURSOR FROM A PS WITHOUT CURSOR\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_close.stmt_id = ctx.stmt_id;
  run_cmd(session, COM_STMT_CLOSE, &cmd, &ctx, false, p);
}

static void test_4(MYSQL_SESSION session, void *p) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];
  uchar param_buff[STRING_BUFFER_SIZE];
  uchar *pos = param_buff;

  Server_context ctx;
  COM_DATA cmd;

  WRITE_STR("CREATE TABLE\n");
  set_query_in_com_data(&cmd,
                        "CREATE TABLE t2"
                        "("
                        " c1  tinyint,"
                        " c2  smallint,"
                        " c3  mediumint,"
                        " c4  int,"
                        " c5  integer,"
                        " c6  bigint,"
                        " c7  float,"
                        " c8  double,"
                        " c9 date)");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  WRITE_STR("CREATE PREPARED STATEMENT\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_prepare.query =
      "INSERT INTO t2(c1, c2, c3, c4, c5, c6, c7, c8, c9) "
      "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)";
  cmd.com_stmt_prepare.length = strlen(cmd.com_stmt_prepare.query);
  run_cmd(session, COM_STMT_PREPARE, &cmd, &ctx, false, p);

  WRITE_STR("EXECUTE PREPARED STATEMENT WITH PARAMETERS AND CURSOR\n");
  PS_PARAM multi_param[9];
  memset(multi_param, 0, sizeof(multi_param));
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_execute.stmt_id = ctx.stmt_id;
  cmd.com_stmt_execute.open_cursor = false;
  cmd.com_stmt_execute.has_new_types = true;
  cmd.com_stmt_execute.parameters = multi_param;

  int8 i8_data = 1;
  int16 i16_data = 1;
  int32 i32_data = 10;
  longlong i64_data = 20;
  float f_data = 2;
  double d_data = 6575.001;

  char date_t[4];
  int2store(date_t, 1988);
  date_t[2] = 12;
  date_t[3] = 20;

  /*tinyint*/
  multi_param[0].null_bit = false;
  multi_param[0].length = sizeof(int8);
  multi_param[0].type = MYSQL_TYPE_TINY;
  multi_param[0].unsigned_type = false;

  /*smallint*/
  multi_param[1].null_bit = false;
  multi_param[1].length = sizeof(int16);
  multi_param[1].type = MYSQL_TYPE_SHORT;
  multi_param[1].unsigned_type = false;

  /*mediumint*/
  multi_param[2].null_bit = false;
  multi_param[2].length = sizeof(int32);
  multi_param[2].type = MYSQL_TYPE_LONG;
  multi_param[2].unsigned_type = false;

  /*int*/
  multi_param[3].null_bit = false;
  multi_param[3].length = sizeof(int32);
  multi_param[3].type = MYSQL_TYPE_LONG;
  multi_param[3].unsigned_type = false;

  /*integer*/
  multi_param[4].null_bit = false;
  multi_param[4].length = sizeof(int32);
  multi_param[4].type = MYSQL_TYPE_LONG;
  multi_param[4].unsigned_type = false;

  /*bigint*/
  multi_param[5].null_bit = false;
  multi_param[5].length = sizeof(int64);
  multi_param[5].type = MYSQL_TYPE_LONGLONG;
  multi_param[5].unsigned_type = false;

  /*float*/
  multi_param[6].null_bit = false;
  multi_param[6].length = sizeof(float);
  multi_param[6].type = MYSQL_TYPE_FLOAT;
  multi_param[6].unsigned_type = false;

  /*double*/
  multi_param[7].null_bit = false;
  multi_param[7].length = sizeof(double);
  multi_param[7].type = MYSQL_TYPE_DOUBLE;
  multi_param[7].unsigned_type = false;

  /*date*/
  multi_param[8].null_bit = false;
  multi_param[8].length = 4;
  multi_param[8].type = MYSQL_TYPE_DATE;
  multi_param[8].unsigned_type = false;

  while (i8_data < 10) {
    multi_param[0].value = (const unsigned char *)&i8_data;

    int2store(pos, i16_data);
    multi_param[1].value = (const unsigned char *)pos;
    pos += 2;

    int4store(pos, i32_data);
    multi_param[2].value = (const unsigned char *)pos;
    pos += 4;

    int4store(pos, i32_data);
    multi_param[3].value = (const unsigned char *)pos;
    pos += 4;

    int4store(pos, i32_data);
    multi_param[4].value = (const unsigned char *)pos;
    pos += 4;

    int8store(pos, i64_data);
    multi_param[5].value = (const unsigned char *)pos;
    pos += 8;

    float4store(pos, f_data);
    multi_param[6].value = (const unsigned char *)pos;
    pos += 4;

    float8store(pos, d_data);
    multi_param[7].value = (const unsigned char *)pos;
    pos += sizeof(double);

    multi_param[8].value = (const unsigned char *)&date_t;
    cmd.com_stmt_execute.has_new_types = ((i8_data % 2 == 0));
    cmd.com_stmt_execute.parameter_count = 9;
    run_cmd(session, COM_STMT_EXECUTE, &cmd, &ctx, false, p);
    i8_data++;
    i16_data++;
    i32_data++;
    i64_data++;
    f_data++;
    d_data++;
  }

  set_query_in_com_data(&cmd, "SELECT * FROM t2");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_close.stmt_id = ctx.stmt_id;
  run_cmd(session, COM_STMT_CLOSE, &cmd, &ctx, false, p);
}

static void test_5(MYSQL_SESSION session, void *p) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];

  Server_context ctx;
  COM_DATA cmd;

  WRITE_STR("CREATE TABLE\n");
  set_query_in_com_data(&cmd,
                        "CREATE TABLE test_long_data(col1 int, "
                        "col2 long varchar)");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  WRITE_STR("CREATE PREPARED STATEMENT\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_prepare.query =
      "INSERT INTO test_long_data(col1, col2) VALUES(?, ?)";
  cmd.com_stmt_prepare.length = strlen(cmd.com_stmt_prepare.query);
  run_cmd(session, COM_STMT_PREPARE, &cmd, &ctx, false, p);

  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_send_long_data.stmt_id = ctx.stmt_id;
  cmd.com_stmt_send_long_data.param_number = 1;
  cmd.com_stmt_send_long_data.length = 8;
  cmd.com_stmt_send_long_data.longdata =
      const_cast<uchar *>(pointer_cast<const uchar *>("Catalin "));
  WRITE_STR("SEND PARAMETER AS COM_STMT_SEND_LONG_DATA\n");
  run_cmd(session, COM_STMT_SEND_LONG_DATA, &cmd, &ctx, false, p);

  cmd.com_stmt_send_long_data.stmt_id = ctx.stmt_id;
  // Append data to the same parameter
  cmd.com_stmt_send_long_data.param_number = 1;
  cmd.com_stmt_send_long_data.length = 8;
  cmd.com_stmt_send_long_data.longdata =
      const_cast<uchar *>(pointer_cast<const uchar *>("Besleaga"));
  WRITE_STR("APPEND TO THE SAME COLUMN\n");
  run_cmd(session, COM_STMT_SEND_LONG_DATA, &cmd, &ctx, false, p);

  PS_PARAM param[3];
  memset(param, 0, sizeof(param));
  param[0].null_bit = false;
  param[0].length = sizeof(int32);
  param[0].type = MYSQL_TYPE_LONG;
  param[0].unsigned_type = false;
  uchar long_data[4];
  int4store(long_data, 4);
  param[0].value = (const unsigned char *)long_data;

  param[1].null_bit = false;
  param[1].length = 0;
  param[1].value = nullptr;
  param[1].type = MYSQL_TYPE_STRING;
  param[1].unsigned_type = false;

  param[2].null_bit = false;
  param[2].length = 0;
  param[2].value = nullptr;
  param[2].type = MYSQL_TYPE_STRING;
  param[2].unsigned_type = false;

  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_execute.stmt_id = ctx.stmt_id;
  cmd.com_stmt_execute.open_cursor = false;
  cmd.com_stmt_execute.has_new_types = true;
  cmd.com_stmt_execute.parameters = param;
  cmd.com_stmt_execute.parameter_count = 2;
  WRITE_STR("EXECUTE PS WITH LONG DATA CURSOR\n");
  run_cmd(session, COM_STMT_EXECUTE, &cmd, &ctx, false, p);

  set_query_in_com_data(&cmd, "SELECT * from test_long_data");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  // Send long data to non existing prepared statement
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_send_long_data.stmt_id = 199999;
  cmd.com_stmt_send_long_data.param_number = 1;
  cmd.com_stmt_send_long_data.length = 8;
  cmd.com_stmt_send_long_data.longdata =
      const_cast<uchar *>(pointer_cast<const uchar *>("12345"));
  WRITE_STR("APPEND TO A NON EXISTING STATEMENT\n");
  run_cmd(session, COM_STMT_SEND_LONG_DATA, &cmd, &ctx, false, p);
  WRITE_STR("ERRORS ONLY SHOW AT FIRST EXECUTION OF COM_STMT_EXECUTE\n");
  run_cmd(session, COM_STMT_EXECUTE, &cmd, &ctx, false, p);

  // Send long data to non existing parameter
  cmd.com_stmt_send_long_data.stmt_id = ctx.stmt_id;
  cmd.com_stmt_send_long_data.param_number = 15;
  cmd.com_stmt_send_long_data.length = 8;
  cmd.com_stmt_send_long_data.longdata =
      const_cast<uchar *>(pointer_cast<const uchar *>("12345"));
  WRITE_STR("APPEND DATA TO NON EXISTING PARAMETER\n");
  run_cmd(session, COM_STMT_SEND_LONG_DATA, &cmd, &ctx, false, p);
  WRITE_STR("ERRORS ONLY SHOW AT FIRST EXECUTION OF COM_STMT_EXECUTE\n");
  run_cmd(session, COM_STMT_EXECUTE, &cmd, &ctx, false, p);

  WRITE_STR("TRY TO CLOSE THE CURSOR FROM A PS WITHOUT CURSOR\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_close.stmt_id = ctx.stmt_id;
  run_cmd(session, COM_STMT_CLOSE, &cmd, &ctx, false, p);
}

#define STRING_SIZE 30

static void test_6(MYSQL_SESSION session, void *p) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];

  Server_context ctx;
  COM_DATA cmd;

  set_query_in_com_data(&cmd,
                        "CREATE TABLE t3(a1 INT, a2 CHAR(32), "
                        "a3 DOUBLE(4, 2), a4 DECIMAL(3, 1))");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  set_query_in_com_data(&cmd,
                        "CREATE TABLE t4(b0 INT, b1 INT, b2 CHAR(32), "
                        "b3 DOUBLE(4, 2), b4 DECIMAL(3, 1))");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  set_query_in_com_data(&cmd,
                        "INSERT INTO t3 VALUES"
                        "(1, '11', 12.34, 56.7), "
                        "(2, '12', 56.78, 90.1), "
                        "(3, '13', 23.45, 67.8)");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  set_query_in_com_data(&cmd,
                        "INSERT INTO t4 VALUES"
                        "(100, 10, '110', 70.70, 10.1), "
                        "(200, 20, '120', 80.80, 20.2), "
                        "(300, 30, '130', 90.90, 30.3)");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  set_query_in_com_data(&cmd,
                        "CREATE PROCEDURE p1("
                        "   IN v0 INT, "
                        "   OUT v_str_1 CHAR(32), "
                        "   OUT v_dbl_1 DOUBLE(4, 2), "
                        "   OUT v_dec_1 DECIMAL(6, 3), "
                        "   OUT v_int_1 INT, "
                        "   IN v1 INT, "
                        "   INOUT v_str_2 CHAR(64), "
                        "   INOUT v_dbl_2 DOUBLE(5, 3), "
                        "   INOUT v_dec_2 DECIMAL(7, 4), "
                        "   INOUT v_int_2 INT)"
                        "BEGIN "
                        "   SET v0 = -1; "
                        "   SET v1 = -1; "
                        "   SET v_str_1 = 'test_1'; "
                        "   SET v_dbl_1 = 12.34; "
                        "   SET v_dec_1 = 567.891; "
                        "   SET v_int_1 = 2345; "
                        "   SET v_str_2 = 'test_2'; "
                        "   SET v_dbl_2 = 67.891; "
                        "   SET v_dec_2 = 234.6789; "
                        "   SET v_int_2 = 6789; "
                        "   SELECT * FROM t3; "
                        "   SELECT * FROM t4; "
                        "END");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_prepare.query = "CALL p1(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
  cmd.com_stmt_prepare.length = strlen(cmd.com_stmt_prepare.query);
  run_cmd(session, COM_STMT_PREPARE, &cmd, &ctx, false, p);

  //  ---------------------------------------------------------------
  char str_data[20][STRING_SIZE];
  double dbl_data[20];
  char dec_data[20][STRING_SIZE];
  int int_data[20];
  PS_PARAM ps_params[STRING_SIZE];

  memset(str_data, 0, sizeof(str_data));
  memset(dbl_data, 0, sizeof(dbl_data));
  memset(dec_data, 0, sizeof(dec_data));
  memset(int_data, 0, sizeof(int_data));

  memset(ps_params, 0, sizeof(ps_params));

  /* - v0 -- INT */

  ps_params[0].type = MYSQL_TYPE_LONG;
  ps_params[0].value = (const unsigned char *)&int_data[0];
  ps_params[0].length = sizeof(int32);
  ps_params[0].unsigned_type = false;
  ps_params[0].null_bit = false;

  /* - v_str_1 -- CHAR(32) */

  ps_params[1].type = MYSQL_TYPE_STRING;
  ps_params[1].value = (const unsigned char *)str_data[0];
  ps_params[1].length = STRING_SIZE;
  ps_params[1].unsigned_type = false;
  ps_params[1].null_bit = false;

  /* - v_dbl_1 -- DOUBLE */

  ps_params[2].type = MYSQL_TYPE_DOUBLE;
  ps_params[2].value = (const unsigned char *)&dbl_data[0];
  ps_params[2].length = STRING_SIZE;
  ps_params[2].unsigned_type = false;
  ps_params[2].null_bit = false;

  /* - v_dec_1 -- DECIMAL */

  ps_params[3].type = MYSQL_TYPE_NEWDECIMAL;
  ps_params[3].value = (const unsigned char *)dec_data[0];
  ps_params[3].length = STRING_SIZE;
  ps_params[3].unsigned_type = false;
  ps_params[3].null_bit = false;

  /* - v_int_1 -- INT */

  ps_params[4].type = MYSQL_TYPE_LONG;
  ps_params[4].value = (const unsigned char *)&int_data[0];
  ps_params[4].length = STRING_SIZE;
  ps_params[4].unsigned_type = false;
  ps_params[4].null_bit = false;

  /* - v1 -- INT */

  ps_params[5].type = MYSQL_TYPE_LONG;
  ps_params[5].value = (const unsigned char *)&int_data[0];
  ps_params[5].length = STRING_SIZE;
  ps_params[5].unsigned_type = false;
  ps_params[5].null_bit = false;

  /* - v_str_2 -- CHAR(32) */

  ps_params[6].type = MYSQL_TYPE_STRING;
  ps_params[6].value = (const unsigned char *)str_data[0];
  ps_params[6].length = STRING_SIZE;
  ps_params[6].unsigned_type = false;
  ps_params[6].null_bit = false;

  /* - v_dbl_2 -- DOUBLE */

  ps_params[7].type = MYSQL_TYPE_DOUBLE;
  ps_params[7].value = (const unsigned char *)&dbl_data[0];
  ps_params[7].length = STRING_SIZE;
  ps_params[7].unsigned_type = false;
  ps_params[7].null_bit = false;

  /* - v_dec_2 -- DECIMAL */

  ps_params[8].type = MYSQL_TYPE_DECIMAL;
  ps_params[8].value = (const unsigned char *)dec_data[0];
  ps_params[8].length = STRING_SIZE;
  ps_params[8].unsigned_type = false;
  ps_params[8].null_bit = false;

  /* - v_int_2 -- INT */

  ps_params[9].type = MYSQL_TYPE_LONG;
  ps_params[9].value = (const unsigned char *)&int_data[0];
  ps_params[9].length = STRING_SIZE;
  ps_params[9].unsigned_type = false;
  ps_params[9].null_bit = false;

  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_execute.parameters = ps_params;
  cmd.com_stmt_execute.open_cursor = false;
  cmd.com_stmt_execute.stmt_id = ctx.stmt_id;
  cmd.com_stmt_execute.has_new_types = true;
  cmd.com_stmt_execute.parameter_count = 10;
  run_cmd(session, COM_STMT_EXECUTE, &cmd, &ctx, false, p);

  WRITE_STR("CLOSE PS\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_close.stmt_id = ctx.stmt_id;
  run_cmd(session, COM_STMT_CLOSE, &cmd, &ctx, false, p);
}

static void test_7(MYSQL_SESSION session, void *p) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];

  Server_context ctx;
  COM_DATA cmd;

  WRITE_STR("CREATE PREPARED STATEMENT\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_prepare.query = "SELECT CONCAT(9< ?)";
  cmd.com_stmt_prepare.length = strlen(cmd.com_stmt_prepare.query);
  run_cmd(session, COM_STMT_PREPARE, &cmd, &ctx, false, p);

  WRITE_STR("EXECUTE PREPARED STATEMENT WITH PARAMETERS AND CURSOR\n");

  PS_PARAM params[1];
  memset(params, 0, sizeof(params));
  params[0].type = MYSQL_TYPE_JSON;
  params[0].unsigned_type = false;
  params[0].null_bit = false;
  params[0].value = (const unsigned char *)"{}";
  params[0].length = 2;

  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_execute.stmt_id = ctx.stmt_id;
  cmd.com_stmt_execute.open_cursor = true;
  cmd.com_stmt_execute.has_new_types = false;
  cmd.com_stmt_execute.parameters = params;
  cmd.com_stmt_execute.parameter_count = 1;
  cmd.com_stmt_execute.has_new_types = true;

  run_cmd(session, COM_STMT_EXECUTE, &cmd, &ctx, false, p);

  WRITE_STR("CLOSE PS\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_close.stmt_id = ctx.stmt_id;
  run_cmd(session, COM_STMT_CLOSE, &cmd, &ctx, false, p);
}

static void test_8(MYSQL_SESSION session, void *p) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];

  Server_context ctx;
  COM_DATA cmd;

  WRITE_STR("RESET VARIABLES THAT ARE GOING TO BE USED FOR OUT-PARAMS\n");
  set_query_in_com_data(
      &cmd, "SET @my_v1=null, @my_v2=null, @my_v3=null, @my_v4=null");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  ctx.tables.clear();
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_prepare.query = "CALL proc_set_out_params(?, ?, ?, ?)";
  cmd.com_stmt_prepare.length = strlen(cmd.com_stmt_prepare.query);
  run_cmd(session, COM_STMT_PREPARE, &cmd, &ctx, false, p);

  WRITE_STR("EXECUTE PREPARED STATEMENT WITH PARAMETERS\n");

  PS_PARAM params[4];
  memset(params, 0, sizeof(params));
  const std::string values[4]{"@my_v1", "@my_v2", "@my_v3", "@my_v4"};
  params[0].type = MYSQL_TYPE_STRING;
  params[0].unsigned_type = false;
  params[0].null_bit = false;
  params[0].value = (const unsigned char *)values[0].c_str();
  params[0].length = values[0].length();
  params[1].type = MYSQL_TYPE_STRING;
  params[1].unsigned_type = false;
  params[1].null_bit = false;
  params[1].value = (const unsigned char *)values[1].c_str();
  params[1].length = values[1].length();
  params[2].type = MYSQL_TYPE_STRING;
  params[2].unsigned_type = false;
  params[2].null_bit = false;
  params[2].value = (const unsigned char *)values[2].c_str();
  params[2].length = values[2].length();
  params[3].type = MYSQL_TYPE_STRING;
  params[3].unsigned_type = false;
  params[3].null_bit = false;
  params[3].value = (const unsigned char *)values[3].c_str();
  params[3].length = values[3].length();

  ctx.tables.clear();
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_execute.stmt_id = ctx.stmt_id;
  cmd.com_stmt_execute.open_cursor = false;
  cmd.com_stmt_execute.has_new_types = false;
  cmd.com_stmt_execute.parameters = params;
  cmd.com_stmt_execute.parameter_count = 4;
  cmd.com_stmt_execute.has_new_types = true;
  run_cmd(session, COM_STMT_EXECUTE, &cmd, &ctx, false, p);

  WRITE_STR(
      "VERIFY THAT VARIABLES ARE STILL NULL AND OUT PRAMETERS WERE TRANSFERED "
      "IN METADATA\n");

  if (ctx.tables.size() != 1 || ctx.tables[0].columns.size() != 4) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Protocol didn't send the out-parameters to the user");
    return;
  }

  ctx.tables.clear();
  set_query_in_com_data(
      &cmd,
      "CALL verify_user_variables_are_null(@my_v1, @my_v2, @my_v3, @my_v4)");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  if (ctx.sql_errno) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Call to 'verify_user_variables_are_null' failed, one of the "
                 "provided user variables may be invalid");
    return;
  }

  WRITE_STR("CLOSE PS\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_close.stmt_id = ctx.stmt_id;
  ctx.tables.clear();
  run_cmd(session, COM_STMT_CLOSE, &cmd, &ctx, false, p);
}

static void test_9(MYSQL_SESSION session, void *p) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];

  Server_context ctx;
  COM_DATA cmd;

  WRITE_STR("RESET VARIABLES THAT ARE GOING TO BE USED FOR OUT-PARAMS\n");
  set_query_in_com_data(
      &cmd, "SET @my_v1=null, @my_v2=null, @my_v3=null, @my_v4=null");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  ctx.tables.clear();
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_prepare.query =
      "CALL proc_set_out_params(@my_v1, @my_v2, @my_v3, @my_v4)";
  cmd.com_stmt_prepare.length = strlen(cmd.com_stmt_prepare.query);
  run_cmd(session, COM_STMT_PREPARE, &cmd, &ctx, false, p);

  WRITE_STR("EXECUTE PREPARED STATEMENT WITHOUT PARAMETERS\n");

  ctx.tables.clear();
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_execute.stmt_id = ctx.stmt_id;
  cmd.com_stmt_execute.open_cursor = false;
  cmd.com_stmt_execute.has_new_types = false;
  cmd.com_stmt_execute.parameters = nullptr;
  cmd.com_stmt_execute.parameter_count = 0;
  cmd.com_stmt_execute.has_new_types = true;
  run_cmd(session, COM_STMT_EXECUTE, &cmd, &ctx, false, p);

  WRITE_STR(
      "VERIFY THAT VARIABLES ARE SET AND OUT PRAMETERS WERE NOT TRANSFERED "
      "IN METADATA\n");

  if (ctx.tables.size() != 0) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Protocol send the out-parameters to the user");
    return;
  }

  ctx.tables.clear();
  set_query_in_com_data(
      &cmd,
      "CALL verify_user_variables_are_set(@my_v1, @my_v2, @my_v3, @my_v4)");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  if (ctx.sql_errno) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Call to 'verify_user_variables_are_set' failed, one of the "
                 "provided user variables may be invalid");
    return;
  }

  WRITE_STR("CLOSE PS\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_close.stmt_id = ctx.stmt_id;
  ctx.tables.clear();
  run_cmd(session, COM_STMT_CLOSE, &cmd, &ctx, false, p);
}

static void test_10(MYSQL_SESSION session, void *p) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];

  Server_context ctx;
  COM_DATA cmd;

  WRITE_STR("RESET VARIABLES THAT ARE GOING TO BE USED FOR OUT-PARAMS\n");
  set_query_in_com_data(
      &cmd, "SET @my_v1=null, @my_v2=null, @my_v3=null, @my_v4=null");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  ctx.tables.clear();
  set_query_in_com_data(
      &cmd, "PREPARE stmt FROM 'CALL proc_set_out_params(?, ?, ?, ?)'");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  WRITE_STR("EXECUTE PREPARED STATEMENT WITHOUT PARAMETERS\n");

  ctx.tables.clear();
  set_query_in_com_data(&cmd,
                        "EXECUTE stmt USING @my_v1, @my_v2, @my_v3, @my_v4");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  WRITE_STR(
      "VERIFY THAT VARIABLES ARE SET AND OUT PRAMETERS WERE NOT TRANSFERED "
      "IN METADATA\n");

  if (ctx.tables.size() != 0) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Protocol send the out-parameters to the user");
    return;
  }

  ctx.tables.clear();
  set_query_in_com_data(
      &cmd,
      "CALL verify_user_variables_are_set(@my_v1, @my_v2, @my_v3, @my_v4)");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);

  if (ctx.sql_errno) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Call to 'verify_user_variables_are_set' failed, one of the "
                 "provided user variables may be invalid");
    return;
  }

  WRITE_STR("CLOSE PS\n");
  ctx.tables.clear();
  set_query_in_com_data(&cmd, "DEALLOCATE PREPARE stmt;");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);
}

static void test_11(MYSQL_SESSION session, void *p) {
  DBUG_ENTER("test_11");
  char buffer[STRING_BUFFER_SIZE];

  Server_context ctx;
  COM_DATA cmd;

  WRITE_STR("CREATE PREPARED STATEMENT\n");
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_prepare.query = "SELECT * from t1 where a = ?";
  cmd.com_stmt_prepare.length = strlen(cmd.com_stmt_prepare.query);
  run_cmd(session, COM_STMT_PREPARE, &cmd, &ctx, false, p);

  PS_PARAM params[1];
  memset(params, 0, sizeof(params));
  params[0].type = MYSQL_TYPE_INVALID;
  params[0].unsigned_type = false;
  params[0].null_bit = false;
  params[0].value = (const unsigned char *)"invalid";
  params[0].length = 1;

  memset(&cmd, 0, sizeof(cmd));
  cmd.com_stmt_execute.stmt_id = ctx.stmt_id;
  cmd.com_stmt_execute.parameter_count = 1;
  cmd.com_stmt_execute.parameters = params;
  cmd.com_stmt_execute.open_cursor = false;
  cmd.com_stmt_execute.has_new_types = true;

  WRITE_STR("EXECUTE THE PS WITH INVALID PARAMETER TYPE\n");
  run_cmd(session, COM_STMT_EXECUTE, &cmd, &ctx, false, p);

  params[0].type = MYSQL_TYPE_BOOL;
  params[0].unsigned_type = false;
  params[0].null_bit = false;
  params[0].value = (const unsigned char *)"bool";
  params[0].length = 1;

  WRITE_STR("EXECUTE THE PS WITH BOOL PARAMETER TYPE\n");
  run_cmd(session, COM_STMT_EXECUTE, &cmd, &ctx, false, p);

  DBUG_VOID_RETURN;
}

static void tear_down_test(MYSQL_SESSION session, void *p) {
  DBUG_TRACE;

  Server_context ctx;
  COM_DATA cmd;

  set_query_in_com_data(&cmd, "DROP TABLE IF EXISTS t1");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);
  set_query_in_com_data(&cmd, "DROP TABLE IF EXISTS t2");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);
  set_query_in_com_data(&cmd, "DROP TABLE IF EXISTS test_long_data");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);
  set_query_in_com_data(&cmd, "DROP TABLE IF EXISTS t3");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);
  set_query_in_com_data(&cmd, "DROP TABLE IF EXISTS t4");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);
  set_query_in_com_data(&cmd, "DROP PROCEDURE IF EXISTS p1");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);
  set_query_in_com_data(&cmd, "DROP PROCEDURE IF EXISTS proc_set_out_params");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);
  set_query_in_com_data(
      &cmd, "DROP PROCEDURE IF EXISTS verify_user_variables_are_null");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);
  set_query_in_com_data(
      &cmd, "DROP PROCEDURE IF EXISTS verify_user_variables_are_set");
  run_cmd(session, COM_QUERY, &cmd, &ctx, false, p);
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

struct my_stmt_tests_st {
  const char *name;
  void (*function)(MYSQL_SESSION, void *);
};

static struct my_stmt_tests_st my_tests[] = {
    {"test COM_STMT_EXECUTE and FETCH AFTER CLOSE", test_1},
    {"Test COM_STMT_EXECUTE with cursor", test_2},
    {"Test COM_STMT_EXECUTE without cursor", test_3},
    {"Test ps with different data-types", test_4},
    {"Test COM_STMT_SEND_LONG_DATA", test_5},
    {"Test COM_STMT_EXECUTE with SELECT nested in CALL", test_6},
    {"Test COM_STMT_EXECUTE with wrong data type", test_7},
    {"Test COM_STMT_EXECUTE with out-params as placeholders", test_8},
    {"Test COM_STMT_EXECUTE with out-params as variables", test_9},
    {"Test COM_QUERY with out-params as placeholders", test_10},
    {"Test COM_STMT_EXECUTE with wrong parameters", test_11},
    {nullptr, nullptr}};

static void test_sql(void *p) {
  DBUG_TRACE;

  char buffer[LARGE_STRING_BUFFER_SIZE];

  /* Session declarations */
  MYSQL_SESSION session;

  /* Open session 1: Must pass */
  WRITE_STR("[srv_session_open]\n");
  session = srv_session_open(nullptr, nullptr);
  if (!session) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "srv_session_open failed");
    goto end;
  }

  switch_user(session, user_privileged);
  setup_test(session, p);

  struct my_stmt_tests_st *fptr;
  for (fptr = my_tests; fptr->name; fptr++) {
    WRITE_HASHED_LINE()
    WRITE_VAL("%s\n", fptr->name);
    WRITE_HASHED_LINE()
    (*fptr->function)(session, p);
  }

  tear_down_test(session, p);
  /* close session 1: Must pass */
  WRITE_STR("[srv_session_close]\n");
  if (srv_session_close(session))
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "srv_session_close failed.");

end:
  return;
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

static void test_in_spawned_thread(void *p, void (*test_function)(void *)) {
  my_thread_attr_t attr; /* Thread attributes */
  my_thread_attr_init(&attr);
  (void)my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);

  // Default stack size may be too small.
  size_t stacksize = 0;
  my_thread_attr_getstacksize(&attr, &stacksize);
  if (stacksize < my_thread_stack_size)
    my_thread_attr_setstacksize(&attr, my_thread_stack_size);

  struct test_thread_context context;

  context.p = p;
  context.thread_finished = false;
  context.test_function = test_function;

  /* now create the thread and call test_session within the thread. */
  if (my_thread_create(&(context.thread), &attr, test_sql_threaded_wrapper,
                       &context) != 0)
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

static struct st_mysql_daemon test_sql_service_plugin = {
    MYSQL_DAEMON_INTERFACE_VERSION};

/*
  Plugin library descriptor
*/

mysql_declare_plugin(test_daemon){
    MYSQL_DAEMON_PLUGIN,
    &test_sql_service_plugin,
    "test_sql_stmt",
    PLUGIN_AUTHOR_ORACLE,
    "Tests prepared statements",
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
/* purecov: end */
