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

#define LOG_COMPONENT_TAG "test_sql_complex"

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

struct CHARSET_INFO;

static const char *log_filename = "test_sql_complex";

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

struct st_test_statement {
  const char *db;
  bool generates_result_set;
  const char *query;
};

static struct st_test_statement test_query_plan[] = {
    /* DB    RESULT                    QUERY   */
    {nullptr, true, "SELECT 'first complex command' as a"},
    {nullptr, false, "CREATE DATABASE test1"},
    {nullptr, false, "USE test"},
    {"test1", false,
     "CREATE TABLE test_inserts("
     " a INT UNSIGNED, b VARCHAR(100), c DOUBLE, d INT, e FLOAT,"
     "f DATETIME, g DATE, h TIME, i TINYINT, k TINYINT UNSIGNED,"
     "l SMALLINT, m SMALLINT UNSIGNED, n MEDIUMINT, o MEDIUMINT UNSIGNED,"
     "p INTEGER, q INTEGER UNSIGNED, r BIGINT, s BIGINT UNSIGNED,"
     "t YEAR, u DECIMAL(5,2) UNSIGNED, v DECIMAL(5,2),"
     " PRIMARY KEY(a), INDEX(d));"},
    {"test1", false,
     "INSERT INTO test_inserts VALUES (1, 'one', 1.23, -1, 11.2323, "
     "'2014-07-06 07:06:05', '1980-02-19', '-830:12:23',"
     " 127, 255, 32767, 65535, 8388607, 16777215, 2147483647, 4294967295, "
     "9223372036854775807, 18446744073709551615,"
     "1901, 999.99, -999.99)"},
    {"test1", false,
     "INSERT INTO test_inserts VALUES (2, 'two', 2.34, -2, 22.3434, "
     "'2015-07-06 21:22:23', '2014-06-05', '356:22:33',"
     " -128, 0, -32768, 32768, -8388608, 8388607, -2147483648, 0, "
     "-9223372036854775808, 18446744073709551615,"
     "2039, 123.45, -543.21)"},
    {"test1", false,
     "INSERT INTO test_inserts VALUES (3, 'three',3.45,-3, 33.4545, "
     "'2016-09-12 11:12:13', '2013-05-04', '821:33:44',"
     " -1, 128, -1, 65534, -1, 16777214, 1, 2, 3, 4,"
     "2155, 222.22, -567.89)"},
    {"test1", true, "SELECT * FROM test1.test_inserts ORDER BY a"},
    {"test1", false, "DELETE FROM test_inserts WHERE a=2"},
    {"test1", true, "SELECT * FROM test_inserts ORDER BY a"},
    {"test1", false, "TRUNCATE test_inserts"},
    {"test1", true, "SELECT * FROM test_inserts ORDER BY a"},
    // prepared statements via SQL
    {"test1", false, "PREPARE ps1 FROM 'select 1'"},
    {"test1", false, "EXECUTE ps1"},
    {"test1", false, "DEALLOCATE PREPARE ps1"},

    {"test1", false, "CREATE TABLE tbl (a INT)"},
    {"test1", false, "PREPARE ps1 FROM 'INSERT INTO tbl VALUES (1), (2), (3)'"},
    {"test1", false, "EXECUTE ps1"},
    {"test1", false, "DEALLOCATE PREPARE ps1"},
    {"test1", false, "SELECT IF(SUM(a)=6, 'OK:)', 'FAIL:(') FROM tbl"},
    // statements generating different errors
    {"test1", true, "DEALLOCATE PREPARE ps1"},
    {"test1", true, "garbage"},
    {"test1", true, "SELECT b FROM tbl"},
    {"test1", true, "ALTER USER bogus@remotehost PASSWORD EXPIRE"},
    {"test1", false, "CREATE TABLE tbld (d TIME)"},
    {"test1", true, "INSERT INTO tbld VALUES ('43141231')"},
    // statements generating warnings
    {"test1", false, "SELECT 1/0"},
    // statements generating info text
    {"test1", false, "UPDATE tbl SET a=5"},
    // transactions
    {"test1", false, "START TRANSACTION"},
    {"test1", false, "UPDATE tbl SET a=4"},
    {"test1", false, "ROLLBACK"},
    {"test1", false, "SELECT IF(SUM(a) = 15, 'OK', 'FAIL') FROM tbl"},
    {"test1", false, "START TRANSACTION"},
    {"test1", false, "UPDATE tbl SET a=4"},
    {"test1", false, "COMMIT"},
    {"test1", false, "SELECT IF(SUM(a) = 12, 'OK', 'FAIL') FROM tbl"},
    {"test1", false, "START TRANSACTION READ ONLY"},
    {"test1", false, "UPDATE tbl SET a=2"},
    {"test1", false, "COMMIT"},
    {"test1", false, "SELECT IF(SUM(4) = 12, 'OK', 'FAIL') FROM tbl"},
    {"test1", false, "SET autocommit=0"},
    {"test1", false, "UPDATE tbl SET a=2"},
    {"test1", false, "ROLLBACK"},
    {"test1", false, "SELECT IF(SUM(4) = 12, 'OK', 'FAIL') FROM tbl"},
    {"test1", true, "set @a=((2) in (select a from tbl))"},
    {"test1", true, "SELECT @a"},
    {"test1", true, "set @b=42"},
    {"test1", true, "SELECT @b"},
    {"test1", true, "SELECT @non_existing"},
    // empty cmd
    {"test1", true, ""},

    {"test1", false, "DROP TABLE tbl"},
    {"test1", false, "DROP TABLE tbld"},
    {"test1", false, "DROP DATABASE test1"},
};

#define STRING_BUFFER_SIZE 512

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

static File outfile;

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

#define SIZEOF_SQL_STR_VALUE 256

struct st_plugin_ctx {
  const CHARSET_INFO *resultcs;
  uint meta_server_status;
  uint meta_warn_count;
  uint current_col;
  uint num_cols;
  uint num_rows;
  st_send_field_n sql_field[64];
  char sql_str_value[64][64][SIZEOF_SQL_STR_VALUE];
  size_t sql_str_len[64][64];

  uint server_status;
  uint warn_count;
  uint affected_rows;
  uint last_insert_id;
  char message[1024];

  uint sql_errno;
  char err_msg[1024];
  char sqlstate[6];

  std::string log;
  st_plugin_ctx() { reset(); }

  void reset() {
    resultcs = nullptr;
    meta_server_status = 0;
    meta_warn_count = 0;
    server_status = 0;
    current_col = 0;
    warn_count = 0;
    num_cols = 0;
    num_rows = 0;
    memset(&sql_field, 0, 64 * sizeof(st_send_field_n));
    memset(&sql_str_value, 0, 64 * 64 * SIZEOF_SQL_STR_VALUE * sizeof(char));
    memset(&sql_str_len, 0, 64 * 64 * sizeof(size_t));

    server_status = 0;
    warn_count = 0;
    affected_rows = 0;
    last_insert_id = 0;
    memset(&message, 0, sizeof(message));

    sql_errno = 0;
    memset(&err_msg, 0, sizeof(err_msg));
    memset(&sqlstate, 0, sizeof(sqlstate));

    log.clear();
  }
};

static int sql_start_result_metadata(void *ctx, uint num_cols, uint,
                                     const CHARSET_INFO *resultcs) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  //  WRITE_STR("sql_start_result_metadata\n");
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
  //  WRITE_STR("sql_field_metadata\n");
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
  //  WRITE_STR("sql_end_result_metadata\n");
  DBUG_TRACE;
  pctx->meta_server_status = server_status;
  pctx->meta_warn_count = warn_count;
  pctx->num_rows = 0;
  return false;
}

static int sql_start_row(void *ctx) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  //  WRITE_STR("sql_start_row\n");
  DBUG_TRACE;
  pctx->current_col = 0;
  return false;
}

static int sql_end_row(void *ctx) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  //  WRITE_STR("sql_end_row\n");
  DBUG_TRACE;
  pctx->num_rows++;
  return false;
}

static void sql_abort_row(void *) { DBUG_TRACE; }

static ulong sql_get_client_capabilities(void *) {
  DBUG_TRACE;
  return 0;
}

static int sql_get_null(void *ctx) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  //  WRITE_STR("sql_get_null\n");
  DBUG_TRACE;
  const uint row = pctx->num_rows;
  const uint col = pctx->current_col;
  pctx->current_col++;

  memcpy(pctx->sql_str_value[row][col], "[NULL]", sizeof("[NULL]"));
  pctx->sql_str_len[row][col] = sizeof("[NULL]") - 1;

  return false;
}

static int sql_get_integer(void *ctx, longlong value) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  //  WRITE_STR("sql_get_integer\n");
  DBUG_TRACE;
  const uint row = pctx->num_rows;
  const uint col = pctx->current_col;
  pctx->current_col++;

  const size_t len =
      snprintf(pctx->sql_str_value[row][col],
               sizeof(pctx->sql_str_value[row][col]), "%lld", value);

  pctx->sql_str_len[row][col] = len;

  return false;
}

static int sql_get_longlong(void *ctx, longlong value, uint is_unsigned) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  //  WRITE_STR("sql_get_longlong\n");
  DBUG_TRACE;
  const uint row = pctx->num_rows;
  const uint col = pctx->current_col;
  pctx->current_col++;

  const size_t len = snprintf(pctx->sql_str_value[row][col],
                              sizeof(pctx->sql_str_value[row][col]),
                              is_unsigned ? "%llu" : "%lld", value);

  pctx->sql_str_len[row][col] = len;

  return false;
}

static const char *test_decimal_as_string(char *buff, const decimal_t *val,
                                          int *length) {
  if (!val) return "NULL";
  (void)decimal2string(val, buff, length);
  return buff;
}

static int sql_get_decimal(void *ctx, const decimal_t *value) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  //  WRITE_STR("sql_get_decimal\n");
  DBUG_TRACE;
  const uint row = pctx->num_rows;
  const uint col = pctx->current_col;
  pctx->current_col++;

  int len = SIZEOF_SQL_STR_VALUE;
  test_decimal_as_string(pctx->sql_str_value[row][col], value, &len);
  pctx->sql_str_len[row][col] = len;

  return false;
}

static int sql_get_double(void *ctx, double value, uint32) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  //  WRITE_STR("sql_get_double\n");
  DBUG_TRACE;
  const uint row = pctx->num_rows;
  const uint col = pctx->current_col;
  pctx->current_col++;

  const size_t len =
      snprintf(pctx->sql_str_value[row][col],
               sizeof(pctx->sql_str_value[row][col]), "%3.7g", value);

  pctx->sql_str_len[row][col] = len;

  return false;
}

static int sql_get_date(void *ctx, const MYSQL_TIME *value) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  //  WRITE_STR("sql_get_date\n");
  DBUG_TRACE;
  const uint row = pctx->num_rows;
  const uint col = pctx->current_col;
  pctx->current_col++;

  const size_t len =
      snprintf(pctx->sql_str_value[row][col],
               sizeof(pctx->sql_str_value[row][col]), "%s%4d-%02d-%02d",
               value->neg ? "-" : "", value->year, value->month, value->day);
  pctx->sql_str_len[row][col] = len;

  return false;
}

static int sql_get_time(void *ctx, const MYSQL_TIME *value, uint) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  //  WRITE_STR("sql_get_time\n");
  DBUG_TRACE;
  const uint row = pctx->num_rows;
  const uint col = pctx->current_col;
  pctx->current_col++;

  const size_t len = snprintf(
      pctx->sql_str_value[row][col], sizeof(pctx->sql_str_value[row][col]),
      "%s%02d:%02d:%02d", value->neg ? "-" : "",
      value->day ? (value->day * 24 + value->hour) : value->hour, value->minute,
      value->second);

  pctx->sql_str_len[row][col] = len;
  return false;
}

static int sql_get_datetime(void *ctx, const MYSQL_TIME *value, uint) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  //  WRITE_STR("sql_get_datetime\n");
  DBUG_TRACE;
  const uint row = pctx->num_rows;
  const uint col = pctx->current_col;
  pctx->current_col++;

  const size_t len = snprintf(
      pctx->sql_str_value[row][col], sizeof(pctx->sql_str_value[row][col]),
      "%s%4d-%02d-%02d %02d:%02d:%02d", value->neg ? "-" : "", value->year,
      value->month, value->day, value->hour, value->minute, value->second);

  pctx->sql_str_len[row][col] = len;

  return false;
}

static int sql_get_string(void *ctx, const char *const value, size_t length,
                          const CHARSET_INFO *const) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  //  WRITE_STR("sql_get_string\n");
  DBUG_TRACE;
  const uint row = pctx->num_rows;
  const uint col = pctx->current_col;
  pctx->current_col++;

  strncpy(pctx->sql_str_value[row][col], value, length);
  pctx->sql_str_len[row][col] = length;

  return false;
}

static void sql_handle_ok(void *ctx, uint server_status,
                          uint statement_warn_count, ulonglong affected_rows,
                          ulonglong last_insert_id, const char *const message) {
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  //  WRITE_STR("sql_handle_ok\n");
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
  char buffer[1024];
  struct st_plugin_ctx *pctx = (struct st_plugin_ctx *)ctx;
  //  WRITE_STR("sql_handle_error\n");
  DBUG_TRACE;
  WRITE_VAL2("[%u][%s]", sql_errno, sqlstate);
  WRITE_VAL("[%s]", err_msg);
  pctx->num_rows = 0;
}

static void sql_shutdown(void *, int) { DBUG_TRACE; }

const struct st_command_service_cbs protocol_callbacks = {
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

/****************************************************************************************/
#define WRITE_DASHED_LINE() \
  WRITE_STR(                \
      "------------------------------------------------------------------\n");

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

static void dump_meta_field(st_send_field_n field) {
  char buffer[STRING_BUFFER_SIZE];

  WRITE_VAL("\t\t\t[meta][field] db name: %s\n", field.db_name);
  WRITE_VAL("\t\t\t[meta][field] table name: %s\n", field.table_name);
  WRITE_VAL("\t\t\t[meta][field] org table name: %s\n", field.org_table_name);
  WRITE_VAL("\t\t\t[meta][field] col name: %s\n", field.col_name);
  WRITE_VAL("\t\t\t[meta][field] org col name: %s\n", field.org_col_name);
  WRITE_VAL("\t\t\t[meta][field] length: %u\n", (uint)field.length);
  WRITE_VAL("\t\t\t[meta][field] charsetnr: %u\n", field.charsetnr);

  WRITE_VAL("\t\t\t[meta][field] flags: %u", field.flags);
  if (field.flags) WRITE_VAL(" (%s)", fieldflags2str(field.flags));
  WRITE_STR("\n");

  WRITE_VAL("\t\t\t[meta][field] decimals: %u\n", field.decimals);

  WRITE_VAL2("\t\t\t[meta][field] type: %s (%u)\n", fieldtype2str(field.type),
             field.type);
}

static void dump_cs_info(const CHARSET_INFO *cs) {
  char buffer[STRING_BUFFER_SIZE];
  if (!cs) {
    WRITE_STR("\t\t[meta] no charset\n");
    return;
  }

  WRITE_VAL("\t\t[meta][charset result] number: %d\n", cs->number);
  WRITE_VAL("\t\t[meta][charset result] name: %s\n", cs->csname);
  WRITE_VAL("\t\t[meta][charset result] collation: %s\n", cs->m_coll_name);
  WRITE_VAL("\t\t[meta][charset result] sort order: %s\n", cs->sort_order);
}

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

static void dump_meta_info(struct st_plugin_ctx *ctx) {
  char buffer[STRING_BUFFER_SIZE];

  WRITE_VAL("\t\t[meta] rows: %u\n", ctx->num_rows);
  WRITE_VAL("\t\t[meta] cols: %u\n", ctx->num_cols);
  dump_decoded_server_status("\t\t[meta] server status: ",
                             ctx->meta_server_status);
  WRITE_VAL("\t\t[meta] warning count: %u\n", ctx->meta_warn_count);
  WRITE_STR("\n");

  if (!ctx->num_cols) {
    WRITE_STR("\t\t[meta] no columns\n");
  } else
    for (uint col = 0; col < ctx->num_cols; col++) {
      dump_meta_field(ctx->sql_field[col]);
      WRITE_STR("\n");
    }

  WRITE_STR("\n");

  dump_cs_info(ctx->resultcs);
}

static void dump_result_set(struct st_plugin_ctx *ctx) {
  char buffer[STRING_BUFFER_SIZE];

  if (!ctx->num_rows) WRITE_STR("\t\t[data] no rows\n");

  for (uint row = 0; row < ctx->num_rows; row++) {
    if (row) WRITE_STR("\n");
    for (uint col = 0; col < ctx->num_cols; col++) {
      WRITE_VAL2("\t\t[data][%s.%s]", ctx->sql_field[col].table_name,
                 ctx->sql_field[col].col_name);
      WRITE_VAL2("[%3u][%s]\n", (uint)ctx->sql_str_len[row][col],
                 ctx->sql_str_value[row][col]);
    }
  }
}

static void dump_closing_ok(struct st_plugin_ctx *ctx) {
  char buffer[STRING_BUFFER_SIZE];

  dump_decoded_server_status("\t\t[end] server status: ", ctx->server_status);
  WRITE_VAL("\t\t[end] warning count:  %u\n", ctx->warn_count);
  WRITE_VAL("\t\t[end] affected rows:  %u\n", ctx->affected_rows);
  WRITE_VAL("\t\t[end] last insert id: %u\n", ctx->last_insert_id);
  WRITE_VAL("\t\t[end] message: %s\n", ctx->message);
}

static void set_query_in_com_data(const char *query, union COM_DATA *cmd) {
  char buffer[STRING_BUFFER_SIZE];

  memset(cmd, 0, sizeof(union COM_DATA));
  cmd->com_query.query = query;
  cmd->com_query.length = strlen(query);
  WRITE_VAL2("EXECUTING:[%u][%s]\n", cmd->com_query.length, query);
}

static void run_statement(MYSQL_SESSION session, const char *query,
                          struct st_plugin_ctx *ctx, bool generates_result_set,
                          void *p [[maybe_unused]]) {
  char buffer[STRING_BUFFER_SIZE];
  COM_DATA cmd;

  WRITE_DASHED_LINE();
  set_query_in_com_data(query, &cmd);

  enum cs_text_or_binary txt_or_bin = CS_TEXT_REPRESENTATION;

  WRITE_STR("[CS_TEXT_REPRESENTATION]\n");
again:
  ctx->reset();
  const int fail = command_service_run_command(
      session, COM_QUERY, &cmd, &my_charset_utf8mb3_general_ci,
      &protocol_callbacks, txt_or_bin, ctx);
  if (fail) {
    LogPluginErrMsg(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "run_statement code: %d\n",
                    fail);
    return;
  }

  dump_meta_info(ctx);
  WRITE_STR("\n");

  dump_result_set(ctx);
  WRITE_STR("\n");

  dump_closing_ok(ctx);

  if (generates_result_set && txt_or_bin == CS_TEXT_REPRESENTATION) {
    txt_or_bin = CS_BINARY_REPRESENTATION;
    WRITE_STR("[CS_BINARY_REPRESENTATION]\n");
    goto again;
  }
}

void static change_current_db(MYSQL_SESSION session, const char *db,
                              struct st_plugin_ctx *ctx,
                              void *p [[maybe_unused]]) {
  COM_DATA cmd;
  cmd.com_init_db.db_name = db;
  cmd.com_init_db.length = strlen(db);

  ctx->reset();
  const int fail = command_service_run_command(
      session, COM_INIT_DB, &cmd, &my_charset_utf8mb3_general_ci,
      &protocol_callbacks, CS_TEXT_REPRESENTATION, ctx);
  if (fail)
    LogPluginErrMsg(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "change db code: %d\n",
                    fail);
}

static void test_selects(MYSQL_SESSION session, void *p) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];

  struct st_plugin_ctx *plugin_ctx = new st_plugin_ctx();

  const char *last_db = nullptr;
  const size_t stmt_count =
      sizeof(test_query_plan) / sizeof(test_query_plan[0]);
  for (size_t i = 0; i < stmt_count; i++) {
    /* Change current DB if needed */
    if (last_db != test_query_plan[i].db) {
      last_db = test_query_plan[i].db;

      change_current_db(session, last_db ? last_db : "", plugin_ctx, p);
    }
    run_statement(session, test_query_plan[i].query, plugin_ctx,
                  test_query_plan[i].generates_result_set, p);
  }

  WRITE_DASHED_LINE();

  delete plugin_ctx;
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

static void test_sql(void *p) {
  DBUG_TRACE;

  char buffer[STRING_BUFFER_SIZE];

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

  test_selects(session, p);

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
    "test_sql_complex",
    PLUGIN_AUTHOR_ORACLE,
    "Test sql complex",
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
