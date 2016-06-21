/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <stdlib.h>
#include <my_global.h>
#include "my_sys.h"                             // my_write, my_malloc
#include <mysql/plugin.h>
#include "mysql_com.h"
#include "m_string.h"

static const char *log_filename= "test_sql_lock";

#define STRING_BUFFER_SIZE 512

#define WRITE_STR(format) \
  { \
    my_snprintf(buffer, sizeof(buffer), (format)); \
    my_write(outfile, (uchar*) buffer, strlen(buffer), MYF(0)); \
  }


#define WRITE_VAL(format,value) \
  { \
    my_snprintf(buffer,sizeof(buffer), (format), (value)); \
    my_write(outfile,(uchar*)buffer, strlen(buffer), MYF(0)); \
  }

#define WRITE_VAL2(format,value1, value2) \
  { \
    my_snprintf(buffer, sizeof(buffer), (format), (value1), (value2)); \
    my_write(outfile,(uchar*) buffer, strlen(buffer), MYF(0)); \
  }

static const char *sep = "========================================================================\n";


#define WRITE_SEP() my_write(outfile, (uchar*)sep, strlen(sep), MYF(0))

static File outfile;

struct st_send_field_n
{
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
  int    intg, frac, len;
  my_bool sign;
  decimal_digit_t buf[256];
};


struct st_plugin_ctx
{
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
  st_plugin_ctx()
  {
    reset();
  }

  void reset()
  {
    resultcs= NULL;
    server_status= 0;
    current_col= 0;
    warn_count= 0;
    num_cols= 0;
    num_rows= 0;
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

    server_status= 0;
    warn_count= 0;
    affected_rows= 0;
    last_insert_id= 0;
    memset(&message, 0, sizeof(message));

    sql_errno= 0;
    memset(&err_msg, 0, sizeof(err_msg));
    memset(&sqlstate, 0, sizeof(sqlstate));
  }
};


static int sql_start_result_metadata(void *ctx, uint num_cols, uint flags,
                                     const CHARSET_INFO *resultcs)
{
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_start_result_metadata");
  DBUG_PRINT("info",("resultcs->number: %d", resultcs->number));
  DBUG_PRINT("info",("resultcs->csname: %s", resultcs->csname));
  DBUG_PRINT("info",("resultcs->name: %s", resultcs->name));
  pctx->num_cols= num_cols;
  pctx->resultcs= resultcs;
  pctx->current_col= 0;
  DBUG_RETURN(false);
}


static int sql_field_metadata(void *ctx, struct st_send_field *field,
                              const CHARSET_INFO *charset)
{
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  st_send_field_n *cfield= &pctx->sql_field[pctx->current_col];
  DBUG_ENTER("sql_field_metadata");
  DBUG_PRINT("info",("field->db_name: %s", field->db_name));
  DBUG_PRINT("info",("field->table_name: %s", field->table_name));
  DBUG_PRINT("info",("field->org_table_name: %s", field->org_table_name));
  DBUG_PRINT("info",("field->col_name: %s", field->col_name));
  DBUG_PRINT("info",("field->org_col_name: %s", field->org_col_name));
  DBUG_PRINT("info",("field->length: %d", (int)field->length));
  DBUG_PRINT("info",("field->charsetnr: %d", (int)field->charsetnr));
  DBUG_PRINT("info",("field->flags: %d", (int)field->flags));
  DBUG_PRINT("info",("field->decimals: %d", (int)field->decimals));
  DBUG_PRINT("info",("field->type: %d", (int)field->type));

  strcpy(cfield->db_name,        (char*)field->db_name);
  strcpy(cfield->table_name,     (char*)field->table_name);
  strcpy(cfield->org_table_name, (char*)field->org_table_name);
  strcpy(cfield->col_name,       (char*)field->col_name);
  strcpy(cfield->org_col_name,   (char*)field->org_col_name);
  cfield->length=    field->length;
  cfield->charsetnr= field->charsetnr;
  cfield->flags=     field->flags;
  cfield->decimals=  field->decimals;
  cfield->type=      field->type;

  pctx->current_col++;
  DBUG_RETURN(false);
}


static int sql_end_result_metadata(void *ctx, uint server_status,
                                   uint warn_count)
{
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_end_result_metadata");
  pctx->meta_server_status= server_status;
  pctx->meta_warn_count= warn_count;
  pctx->num_rows= 0;
  DBUG_RETURN(false);
};


static int sql_start_row(void *ctx)
{
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_start_row");
  pctx->current_col= 0;
  DBUG_RETURN(false);
}


static int sql_end_row(void *ctx)
{
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_end_row");
  pctx->num_rows++;
  DBUG_RETURN(false);
}


static void sql_abort_row(void *ctx)
{
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_abort_row");
  pctx->current_col= 0;
  DBUG_VOID_RETURN;
}


static ulong sql_get_client_capabilities(void *ctx)
{
  DBUG_ENTER("sql_get_client_capabilities");
  DBUG_RETURN(0);
}


static int sql_get_null(void *ctx)
{
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_get_null");
  uint row= pctx->num_rows;
  uint col= pctx->current_col;
  pctx->current_col++;

  strncpy(pctx->sql_str_value[row][col], "[NULL]", sizeof("[NULL]")-1);
  pctx->sql_str_len[row][col]=  sizeof("[NULL]")-1;

  DBUG_RETURN(false);
}


static int sql_get_integer(void * ctx, longlong value)
{
  char buffer[1024];
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_get_integer");
  uint row= pctx->num_rows;
  uint col= pctx->current_col;
  pctx->current_col++;

  size_t len= my_snprintf(buffer, sizeof(buffer), "%d", value);

  strncpy(pctx->sql_str_value[row][col], buffer, len);
  pctx->sql_str_len[row][col]= len;
  pctx->sql_int_value[row][col]= value;

  DBUG_RETURN(false);
}


static int sql_get_longlong(void * ctx, longlong value, uint is_unsigned)
{
  char buffer[1024];
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_get_longlong");
  uint row= pctx->num_rows;
  uint col= pctx->current_col;
  pctx->current_col++;

  size_t len= my_snprintf(buffer, sizeof(buffer),
                          is_unsigned? "%llu":"%lld", value);

  strncpy(pctx->sql_str_value[row][col], buffer, len);
  pctx->sql_str_len[row][col]= len;
  pctx->sql_longlong_value[row][col]= value;
  pctx->sql_is_unsigned[row][col]= is_unsigned;

  DBUG_RETURN(false);
}


static int sql_get_decimal(void * ctx, const decimal_t * value)
{
  char buffer[1024];
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_get_decimal");
  uint row= pctx->num_rows;
  uint col= pctx->current_col;
  pctx->current_col++;

  size_t len= my_snprintf(buffer, sizeof(buffer),
                          "%s%d.%d(%d)[%s]",
                          value->sign? "+":"-",
                          value->intg, value->frac, value->len,
                          value->buf);

  strncpy(pctx->sql_str_value[row][col], buffer, len);

  pctx->sql_str_len[row][col]= len;
  pctx->sql_decimal_value[row][col].intg= value->intg;
  pctx->sql_decimal_value[row][col].frac= value->frac;
  pctx->sql_decimal_value[row][col].len = value->len ;
  pctx->sql_decimal_value[row][col].sign=  value->sign;
  memset((void*)pctx->sql_decimal_value[row][col].buf, '\0',(int)value->len);
  memcpy((void*)pctx->sql_decimal_value[row][col].buf, (void*)value->buf,(int)value->len);

  DBUG_RETURN(false);
}


static int sql_get_double(void * ctx, double value, uint32 decimals)
{
  char buffer[1024];
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_get_double");
  uint row= pctx->num_rows;
  uint col= pctx->current_col;
  pctx->current_col++;

  size_t len= my_snprintf(buffer, sizeof(buffer), "%3.7g", value);

  strncpy(pctx->sql_str_value[row][col], buffer, len);
  pctx->sql_str_len[row][col]= len;

  pctx->sql_double_value[row][col]= value;
  pctx->sql_double_decimals[row][col]= decimals;
  
  DBUG_RETURN(false);
}


static int sql_get_date(void * ctx, const MYSQL_TIME * value)
{
  char buffer[1024];
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_get_date");
  uint row= pctx->num_rows;
  uint col= pctx->current_col;
  pctx->current_col++;

  size_t len= my_snprintf(buffer, sizeof(buffer),
                          "%s%4d-%02d-%02d",
                          value->neg? "-":"",
                          value->year, value->month, value->day);

  strncpy(pctx->sql_str_value[row][col], buffer, len);
  pctx->sql_str_len[row][col]= len;

  pctx->sql_date_value[row][col].year=        value->year;
  pctx->sql_date_value[row][col].month=       value->month;
  pctx->sql_date_value[row][col].day=         value->day;

  pctx->sql_date_value[row][col].hour=        value->hour;
  pctx->sql_date_value[row][col].minute=      value->minute;
  pctx->sql_date_value[row][col].second=      value->second;
  pctx->sql_date_value[row][col].second_part= value->second_part;
  pctx->sql_date_value[row][col].neg=         value->neg;

  DBUG_RETURN(false);
}


static int sql_get_time(void * ctx, const MYSQL_TIME * value, uint decimals)
{
  char buffer[1024];
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_get_time");
  uint row= pctx->num_rows;
  uint col= pctx->current_col;
  pctx->current_col++;

  size_t len= my_snprintf(buffer, sizeof(buffer),
                          "%s%02d:%02d:%02d",
                          value->neg? "-":"",
                          value->day? (value->day*24 + value->hour):value->hour,
                          value->minute, value->second);

  strncpy(pctx->sql_str_value[row][col], buffer, len);
  pctx->sql_str_len[row][col]= len;

  pctx->sql_time_value[row][col].year=        value->year;
  pctx->sql_time_value[row][col].month=       value->month;
  pctx->sql_time_value[row][col].day=         value->day;

  pctx->sql_time_value[row][col].hour=        value->hour;
  pctx->sql_time_value[row][col].minute=      value->minute;
  pctx->sql_time_value[row][col].second=      value->second;
  pctx->sql_time_value[row][col].second_part= value->second_part;
  pctx->sql_time_value[row][col].neg=         value->neg;
  pctx->sql_time_decimals[row][col]=          decimals;

  DBUG_RETURN(false);
}


static int sql_get_datetime(void * ctx, const MYSQL_TIME * value, uint decimals)
{
  char buffer[1024];
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_get_datetime");
  uint row= pctx->num_rows;
  uint col= pctx->current_col;
  pctx->current_col++;

  size_t len= my_snprintf(buffer, sizeof(buffer),
                          "%s%4d-%02d-%02d %02d:%02d:%02d",
                          value->neg? "-":"",
                          value->year, value->month, value->day,
                          value->hour, value->minute, value->second);

  strncpy(pctx->sql_str_value[row][col], buffer, len);
  pctx->sql_str_len[row][col]= len;

  pctx->sql_datetime_value[row][col].year=        value->year;
  pctx->sql_datetime_value[row][col].month=       value->month;
  pctx->sql_datetime_value[row][col].day=         value->day;

  pctx->sql_datetime_value[row][col].hour=        value->hour;
  pctx->sql_datetime_value[row][col].minute=      value->minute;
  pctx->sql_datetime_value[row][col].second=      value->second;
  pctx->sql_datetime_value[row][col].second_part= value->second_part;
  pctx->sql_datetime_value[row][col].neg=         value->neg;
  pctx->sql_datetime_decimals[row][col]=          decimals;

  DBUG_RETURN(false);
}


static int sql_get_string(void * ctx, const char * const value, size_t length,
                          const CHARSET_INFO * const valuecs)
{
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_get_string");
  uint row= pctx->num_rows;
  uint col= pctx->current_col;
  pctx->current_col++;

  strncpy(pctx->sql_str_value[row][col], value, length);
  pctx->sql_str_len[row][col]= length;

  DBUG_RETURN(false);
}


static void sql_handle_ok(void * ctx,
                          uint server_status, uint statement_warn_count,
                          ulonglong affected_rows, ulonglong last_insert_id,
                          const char * const message)
{
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_handle_ok");
  /* This could be an EOF */
  if (!pctx->num_cols)
    pctx->num_rows= 0;
  pctx->server_status=  server_status;
  pctx->warn_count=     statement_warn_count;
  pctx->affected_rows=  affected_rows;
  pctx->last_insert_id= last_insert_id;
  if (message)
    strncpy(pctx->message, message, sizeof(pctx->message));

  DBUG_VOID_RETURN;
}


static void sql_handle_error(void * ctx, uint sql_errno,
                             const char * const err_msg,
                             const char * const sqlstate)
{
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  DBUG_ENTER("sql_handle_error");
  pctx->sql_errno=sql_errno;
  if (pctx->sql_errno)
  {
    strcpy(pctx->err_msg,(char *)err_msg);
    strcpy(pctx->sqlstate,(char*)sqlstate);
  }
  pctx->num_rows= 0;
  DBUG_VOID_RETURN;
}


static void sql_shutdown(void *ctx, int shutdown_server)
{
  DBUG_ENTER("sql_shutdown");
  DBUG_VOID_RETURN;
}


const struct st_command_service_cbs sql_cbs= {
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
};


static void get_data_str(void * ctx)
{
  char buffer[STRING_BUFFER_SIZE];
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;

  for (uint col= 0; col < pctx->num_cols; col++)
  {
    if (col)
      WRITE_STR("\t");
    WRITE_VAL("%s", pctx->sql_field[col].col_name);
  }
  WRITE_STR("\n");

  for (uint row= 0; row < pctx->num_rows; row++)
  {
    for (uint col= 0; col < pctx->num_cols; col++)
    {
      if (col)
        WRITE_STR("\t\t");
      WRITE_VAL("%s", pctx->sql_str_value[row][col]);
    }
    WRITE_STR("\n");
  }
}


static void handle_error(void * ctx)
{
  char buffer[STRING_BUFFER_SIZE];
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;

  /* handle_error */ 
  if (pctx->sql_errno)
  {
    WRITE_VAL("error: %d\n",pctx->sql_errno);
    WRITE_VAL("error msg: %s\n",pctx->err_msg);
  }
}

static void exec_test_cmd(MYSQL_SESSION session, const char *test_cmd, void *p, struct st_plugin_ctx * ctx)
{
  char buffer[STRING_BUFFER_SIZE];
  COM_DATA cmd;

  WRITE_VAL("%s\n", test_cmd);
  cmd.com_query.query= (char *)test_cmd;
  cmd.com_query.length= strlen(cmd.com_query.query);

  ctx->reset();
  int fail= command_service_run_command(session,COM_QUERY,&cmd, &my_charset_utf8_general_ci,
                                        &sql_cbs, CS_TEXT_REPRESENTATION, ctx);

  if (fail)
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "test_sql_lock-ret code : %d", fail);
  else
  {
    if (ctx->num_cols)
      get_data_str(ctx);
    handle_error(ctx);
  }
}


static void test_isolation_levels(void *p)
{
  char buffer[STRING_BUFFER_SIZE];
  MYSQL_SESSION session_1, session_2;

  DBUG_ENTER("test_sql");
  struct st_plugin_ctx *plugin_ctx= new st_plugin_ctx();
  
  /* Open session 1 and session 2 */  
  WRITE_STR("\nOpening Session 1\n");
  session_1= srv_session_open(NULL,plugin_ctx);
  if (!session_1)
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "open session_1 failed.");

  WRITE_STR("Opening Session 2\n");
  session_2= srv_session_open(NULL,plugin_ctx);
  if (!session_2)
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "open session_2 failed.");

  WRITE_STR("\n");

  /* Isolation Level : READ COMMITED */
  WRITE_STR("===================================================================\n");
  WRITE_STR("Isolation Level : READ COMMITTED\n");
  WRITE_STR("===================================================================\n");

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 1 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_1, "SET AUTOCOMMIT = 0", p, plugin_ctx);
  exec_test_cmd(session_1, "SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED", p, plugin_ctx);
  exec_test_cmd(session_1, "INSERT INTO test.t1 VALUES (8,4)", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 2 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_2, "SET AUTOCOMMIT = 0", p, plugin_ctx);
  exec_test_cmd(session_2, "SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED", p, plugin_ctx);
  exec_test_cmd(session_2, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 1 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_1, "COMMIT", p, plugin_ctx);
  exec_test_cmd(session_1, "SET AUTOCOMMIT = 1", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 2 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_2, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);
  exec_test_cmd(session_2, "COMMIT", p, plugin_ctx);
  exec_test_cmd(session_2, "SET AUTOCOMMIT = 1", p, plugin_ctx);
  exec_test_cmd(session_2, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);

  /* Isolation Level : READ COMMITED */
  WRITE_STR("\n");
  WRITE_STR("===================================================================\n");
  WRITE_STR("Isolation Level : READ UNCOMMITTED\n");
  WRITE_STR("===================================================================\n");

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 1 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_1, "SET AUTOCOMMIT = 0", p, plugin_ctx);
  exec_test_cmd(session_1, "SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED", p, plugin_ctx);
  exec_test_cmd(session_1, "INSERT INTO test.t1 VALUES (9,5)", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 2 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_2, "SET AUTOCOMMIT = 0", p, plugin_ctx);
  exec_test_cmd(session_2, "SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED", p, plugin_ctx);
  exec_test_cmd(session_2, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 1 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_1, "COMMIT", p, plugin_ctx);
  exec_test_cmd(session_1, "SET AUTOCOMMIT = 1", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 2 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_2, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);
  exec_test_cmd(session_2, "COMMIT", p, plugin_ctx);
  exec_test_cmd(session_2, "SET AUTOCOMMIT = 1", p, plugin_ctx);
  exec_test_cmd(session_2, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);

  /* Isolation Level : REPEATABLE READ */
  WRITE_STR("\n");
  WRITE_STR("===================================================================\n");
  WRITE_STR("Isolation Level : REPEATABLE READ\n");
  WRITE_STR("===================================================================\n");

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 1 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_1, "SET AUTOCOMMIT = 0", p, plugin_ctx);
  exec_test_cmd(session_1, "SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ", p, plugin_ctx);
  exec_test_cmd(session_1, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 2 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_2, "SET AUTOCOMMIT = 0", p, plugin_ctx);
  exec_test_cmd(session_2, "SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ", p, plugin_ctx);
  exec_test_cmd(session_2, "INSERT INTO test.t1 VALUES (10,6)", p, plugin_ctx);
  exec_test_cmd(session_2, "COMMIT", p, plugin_ctx);
  exec_test_cmd(session_2, "SET AUTOCOMMIT = 1", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 1 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_1, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);
  exec_test_cmd(session_1, "COMMIT", p, plugin_ctx);
  exec_test_cmd(session_1, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);
  exec_test_cmd(session_1, "SET AUTOCOMMIT = 1", p, plugin_ctx);

  /* Isolation Level : SERIALIZABLE */
  WRITE_STR("\n");
  WRITE_STR("===================================================================\n");
  WRITE_STR("Isolation Level : SERIALIZABLE\n");
  WRITE_STR("===================================================================\n");

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 1 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_1, "SET AUTOCOMMIT = 0", p, plugin_ctx);
  exec_test_cmd(session_1, "SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE", p, plugin_ctx);
  exec_test_cmd(session_1, "INSERT INTO test.t1 VALUES (11,7)", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 2 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_2, "SET AUTOCOMMIT = 0", p, plugin_ctx);
  exec_test_cmd(session_2, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 1 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_1, "COMMIT", p, plugin_ctx);
  exec_test_cmd(session_1, "SET AUTOCOMMIT = 1", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 2 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_2, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);
  exec_test_cmd(session_2, "COMMIT", p, plugin_ctx);
  exec_test_cmd(session_2, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);
  exec_test_cmd(session_2, "SET AUTOCOMMIT = 1", p, plugin_ctx);

  /* Locked by a SQL transaction */
  WRITE_STR("\n");
  WRITE_STR("===================================================================\n");
  WRITE_STR("Locking done by a SQL transaction\n");
  WRITE_STR("===================================================================\n");

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 1 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_1, "BEGIN", p, plugin_ctx);
  exec_test_cmd(session_1, "DELETE FROM test.t1 WHERE c1 = 11", p, plugin_ctx);
  exec_test_cmd(session_1, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 2 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_2, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 1 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_1, "COMMIT", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 2 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_2, "SELECT COUNT(*) FROM test.t1", p, plugin_ctx);

  /* Close session 1 */
  WRITE_STR("\n");
  WRITE_STR("Closing Session 1\n");
  if (srv_session_close(session_1))
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "close session_1 failed.");

  /* Close session 2 */
  WRITE_STR("Closing Session 2\n");
  if (srv_session_close(session_2))
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "close session_2 failed.");

  delete plugin_ctx;
  DBUG_VOID_RETURN;
}


static void test_locking(void *p)
{
  char buffer[STRING_BUFFER_SIZE];
  MYSQL_SESSION session_1, session_2, session_3;

  DBUG_ENTER("test_sql");
  struct st_plugin_ctx *plugin_ctx= new st_plugin_ctx();

  /* Open session 1 and session 2 */
  WRITE_STR("\nOpening Session 1\n");
  session_1= srv_session_open(NULL, plugin_ctx);
  if (!session_1)
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "open session_1 failed.");

  WRITE_STR("Opening Session 2\n");
  session_2= srv_session_open(NULL, plugin_ctx);
  if (!session_2)
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "open session_2 failed.");

  WRITE_STR("Opening Session 3\n");
  session_3= srv_session_open(NULL, plugin_ctx);
  if (!session_3)
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "open session_3 failed.");

  /* Locking */
  WRITE_STR("===================================================================\n");
  WRITE_STR("Locking using LOCK TABLE\n");
  WRITE_STR("===================================================================\n");

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 1 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_1, "LOCK TABLE test.t1 READ", p, plugin_ctx);
  /* Following statement should return an error because table t1 is locked with a READ lock */
  exec_test_cmd(session_1, "/*statement should return an error because table t1 is locked with a READ lock*/\nINSERT INTO test.t1 VALUES (11,7)", p, plugin_ctx);
  exec_test_cmd(session_1, "SHOW OPEN TABLES FROM test LIKE 't1'", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 2 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_2, "LOCK TABLE test.t2 WRITE", p, plugin_ctx);
  exec_test_cmd(session_2, "SHOW OPEN TABLES FROM test LIKE 't1'", p, plugin_ctx);
  exec_test_cmd(session_2, "SHOW OPEN TABLES FROM test LIKE 't2'", p, plugin_ctx);

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 1 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_1, "SHOW OPEN TABLES FROM test LIKE 't2'", p, plugin_ctx);

  /* Close session 1 */  
  WRITE_STR("\nClosing Session 1\n");
  if (srv_session_close(session_1))
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "close session_1 failed.");

  WRITE_STR("===================================================================\n");
  WRITE_STR("Check session 1's lock after its close\n");
  WRITE_STR("===================================================================\n");

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 2 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_2, "SHOW OPEN TABLES FROM test LIKE 't1'", p, plugin_ctx);
  exec_test_cmd(session_2, "SHOW OPEN TABLES FROM test LIKE 't2'", p, plugin_ctx);

  /* Close session 2 */
  WRITE_STR("\nClosing Session 2\n\n");
  if (srv_session_close(session_2))
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "close session_2 failed.");

  WRITE_STR("===================================================================\n");
  WRITE_STR("Check session 2's lock after its close\n");
  WRITE_STR("===================================================================\n");

  WRITE_STR("-------------------------------------------------------------------\n");
  WRITE_STR("Session 3 :\n");
  WRITE_STR("-------------------------------------------------------------------\n");
  exec_test_cmd(session_3, "SHOW OPEN TABLES FROM test LIKE 't1'", p, plugin_ctx);
  exec_test_cmd(session_3, "SHOW OPEN TABLES FROM test LIKE 't2'", p, plugin_ctx);

  /* Close session 3 */
  WRITE_STR("\nClosing Session 3\n\n");
  if (srv_session_close(session_3))
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "close session_3 failed.");

  delete plugin_ctx;
  DBUG_VOID_RETURN;
}


static void test_sql(void *p)
{
  test_isolation_levels(p);
  test_locking(p);
}


struct test_thread_context
{
  my_thread_handle thread;
  void *p;
  bool thread_finished;
  void (*test_function)(void *);
};


static void* test_sql_threaded_wrapper(void *param)
{
  char buffer[STRING_BUFFER_SIZE];
  struct test_thread_context *context= (struct test_thread_context*) param;

  WRITE_SEP();
  WRITE_STR("init thread\n");
  if (srv_session_init_thread(context->p))
    my_plugin_log_message(&context->p, MY_ERROR_LEVEL, "srv_session_init_thread failed.");

  context->test_function(context->p);

  WRITE_STR("deinit thread\n");
  srv_session_deinit_thread();

  context->thread_finished= true;
  return NULL;
}


static void create_log_file(const char * log_name)
{
  char filename[FN_REFLEN];

  fn_format(filename, log_name, "", ".log",
            MY_REPLACE_EXT | MY_UNPACK_FILENAME);
  unlink(filename);
  outfile= my_open(filename, O_CREAT|O_RDWR, MYF(0));
}


static void test_in_spawned_thread(void *p, void (*test_function)(void *))
{
  my_thread_attr_t attr;          /* Thread attributes */
  my_thread_attr_init(&attr);
  (void) my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);

  struct test_thread_context context;

  context.p= p;
  context.thread_finished= false;
  context.test_function= test_function;

  /* now create the thread and call test_session within the thread. */
  if (my_thread_create(&(context.thread), &attr, test_sql_threaded_wrapper, &context) != 0)
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "Could not create test session thread");
  else
    my_thread_join(&context.thread, NULL);
}

static int test_sql_service_plugin_init(void *p)
{
  char buffer[STRING_BUFFER_SIZE];
  DBUG_ENTER("test_sql_service_plugin_init");
  my_plugin_log_message(&p, MY_INFORMATION_LEVEL, "Installation.");

  create_log_file(log_filename);

  WRITE_SEP();
  WRITE_STR("Test in a server thread\n");
  test_sql(p);

  /* Test in a new thread */
  WRITE_STR("Follows threaded run\n");
  test_in_spawned_thread(p, test_sql);

  my_close(outfile, MYF(0));

  DBUG_RETURN(0);
}


static int test_sql_service_plugin_deinit(void *p)
{
  DBUG_ENTER("test_sql_service_plugin_deinit");
  my_plugin_log_message(&p, MY_INFORMATION_LEVEL, "Uninstallation.");
  DBUG_RETURN(0);
}


struct st_mysql_daemon test_sql_service_plugin=
{ MYSQL_DAEMON_INTERFACE_VERSION };


/*
  Plugin library descriptor
*/

mysql_declare_plugin(test_daemon)
{
  MYSQL_DAEMON_PLUGIN,
  &test_sql_service_plugin,
  "test_sql_lock",
  "Pavan Naik, Andrey Hristov",
  "Test SQL locking mechanisms",
  PLUGIN_LICENSE_GPL,
  test_sql_service_plugin_init,   /* Plugin Init */
  test_sql_service_plugin_deinit, /* Plugin Deinit */
  0x0100,                         /* 1.0 */
  NULL,                           /* status variables */
  NULL,                           /* system variables */
  NULL,                           /* config options */
  0,                              /* flags */
}
mysql_declare_plugin_end;
