/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include "sql_plugin.h"                         // st_plugin_int

#define STRING_BUFFER_SIZE 256

static const char *sep = "======================================================\n";

#define WRITE_SEP() my_write(outfile, (uchar*)sep, strlen(sep), MYF(0))

static File outfile;

void WRITE_STR(const char *format)
{
  char buffer[STRING_BUFFER_SIZE];
  my_snprintf(buffer,sizeof(buffer),format);
  my_write(outfile,(uchar*)buffer,strlen(buffer),MYF(0));
}


template<typename T>
void WRITE_VAL(const char *format, T value)
{
  char buffer[STRING_BUFFER_SIZE];
  my_snprintf(buffer,sizeof(buffer),format,value);
  my_write(outfile,(uchar*)buffer,strlen(buffer),MYF(0));
}


template<typename T1, typename T2>
void WRITE_VAL(const char *format, T1 value1, T2 value2)
{
  char buffer[STRING_BUFFER_SIZE];
  my_snprintf(buffer,sizeof(buffer),format,value1,value2);
  my_write(outfile,(uchar*)buffer,strlen(buffer),MYF(0));
}


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
}


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
};


static ulong sql_get_client_capabilities(void *ctx){
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

  strcpy(pctx->sql_str_value[row][col], "[NULL]");
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
};


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
    strcpy(pctx->err_msg, err_msg);
    strcpy(pctx->sqlstate, sqlstate);
  }
  pctx->num_rows= 0;
  DBUG_VOID_RETURN;
}


static void sql_shutdown(void *ctx, int shutdown_server)
{
  DBUG_ENTER("sql_shutdown");
  int *crashme = NULL;
  *crashme = 0;
  if (shutdown_server)
    WRITE_STR("SERVER IS SHUTTING DOWN!!!\n");

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


static void get_data_integer(void * ctx)
{
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;

  /* get values */
  if ((pctx->num_rows > 0) && (pctx->num_cols > 0))
  {
    for (uint col_count= 0; col_count < pctx->num_cols; col_count++)
    {
      WRITE_VAL("%s  ", pctx->sql_field[col_count].col_name);
    }
    WRITE_STR("\n");

    for (uint row_count= 0; row_count < pctx->num_rows; row_count++)
    {
      for (uint col_count= 0; col_count < pctx->num_cols; col_count++)
      {
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


static void handle_error(void * ctx)
{
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;

  /* handle_ok/error */ 
  if (pctx->sql_errno)
  {
     WRITE_VAL("error: %d\n",pctx->sql_errno);
     WRITE_VAL("error msg: %s\n",pctx->err_msg);
  }
  else
  {
     WRITE_VAL("affected rows : %d\n",pctx->affected_rows);
     WRITE_VAL("server status : %d\n",pctx->server_status);
     WRITE_VAL("warn count    : %d\n",pctx->warn_count);
  }
}

static bool callback_called= false;

static void error_callback(void *ctx, unsigned int sql_errno, const char *err_msg)
{
  WRITE_VAL("ERROR %d : %s\n", sql_errno, err_msg);
  callback_called= true;
}


static void exec_test_cmd(MYSQL_SESSION session, const char *test_cmd, void *p, void * ctx)
{
  WRITE_VAL("%s\n",test_cmd);
  struct st_plugin_ctx *pctx= (struct st_plugin_ctx*) ctx;
  COM_DATA cmd;

  pctx->reset();
  cmd.com_query.query= (char *)test_cmd;
  cmd.com_query.length= strlen(cmd.com_query.query);
  int fail= command_service_run_command(session,COM_QUERY,&cmd, &my_charset_utf8_general_ci,
                                        &sql_cbs, CS_BINARY_REPRESENTATION, ctx);
  if (fail)
  {
    if (callback_called == false)
      my_plugin_log_message(&p, MY_ERROR_LEVEL, "Callback not called");
    else
      callback_called= false;
  }
  else
  {
    get_data_integer(ctx);
    handle_error(ctx);
  }
}


enum enum_init_cycle
{
  FIRST_INIT= 1,
  SECOND_INIT= 2
};

static enum enum_init_cycle plugin_init_cycle= FIRST_INIT;

static void test_sql(void *p)
{
  DBUG_ENTER("test_sql");
  struct st_plugin_ctx *plugin_ctx= new st_plugin_ctx();
  MYSQL_SESSION session;

  /* Opening a Session */
  WRITE_SEP();
  WRITE_STR("Opening a Session\n");
  session= srv_session_open(error_callback, plugin_ctx);
  if (!session)
  {
    /*
      We hope that this will always be okay. During the restart we will get here
      before the server has started fully
    */
    if (srv_session_server_is_available() || !callback_called)
    {
      my_plugin_log_message(&p, MY_ERROR_LEVEL, "srv_session_open failed");
      delete plugin_ctx;
      DBUG_VOID_RETURN;
    }

    while (!srv_session_server_is_available())
      my_sleep(500);
    plugin_init_cycle= SECOND_INIT;
  }

  WRITE_SEP();
  if (plugin_init_cycle == FIRST_INIT)
  {
    while (srv_session_server_is_available())
      my_sleep(500);

    WRITE_STR("Apparently the server is shutting down\n");
  }
  exec_test_cmd(session, "SELECT 1, 2, 3 FROM DUAL", p, plugin_ctx);

  /* Close Session */
  WRITE_VAL("\nClosing Session. Plugin init cycle = %d\n\n", plugin_init_cycle);

  srv_session_close(session);
  delete plugin_ctx;

  DBUG_VOID_RETURN;
}


struct test_services_context
{
  my_thread_handle test_services_thread;
  void *p;
};


static void* test_sql_threaded_wrapper(void *param)
{
  struct test_services_context *context= (struct test_services_context*) param;

  if (srv_session_init_thread(context->p))
    my_plugin_log_message(&context->p, MY_ERROR_LEVEL, "srv_session_init_thread failed.");

  test_sql(context->p);

  srv_session_deinit_thread();

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


static int test_sql_service_plugin_init(void *p)
{
  DBUG_ENTER("test_sql_service_plugin_init");
  my_plugin_log_message(&p, MY_INFORMATION_LEVEL, "Installation.");

  create_log_file("test_sql_shutdown");
  
  struct test_services_context *context;
  my_thread_attr_t attr;          /* Thread attributes */
  my_thread_attr_init(&attr);
  (void) my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);

  context= (struct test_services_context *)
        my_malloc(PSI_INSTRUMENT_ME,
        sizeof(struct test_services_context), MYF(0));
  context->p= p;

  callback_called= false;
  /* now create the thread and call test_session within the thread. */
  if (my_thread_create(&context->test_services_thread, &attr, test_sql_threaded_wrapper, context) != 0)
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "srv_session_init_thread failed.");
  else
    ((struct st_plugin_int *) p)->data= (void*) context;

  /* Now we can shut down the server */
  DBUG_RETURN(0);
}


static int test_sql_service_plugin_deinit(void *p)
{
  struct test_services_context *con= (struct test_services_context *) ((struct st_plugin_int *) p)->data;
  DBUG_ENTER("test_sql_service_plugin_deinit");

  my_plugin_log_message(&p, MY_INFORMATION_LEVEL, "Uninstallation.");

  WRITE_STR("SERVER SHUTDOWN\n");

  if (con)
  {
    void *dummy_retval;
    my_thread_cancel(&con->test_services_thread);
    my_thread_join(&con->test_services_thread, &dummy_retval);
  }
  my_close(outfile, MYF(0));
  my_free(con);
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
  "test_sql_shutdown",
  "Horst Hunger, Andrey Hristov",
  "Test SQL shutdown",
  PLUGIN_LICENSE_GPL,
  test_sql_service_plugin_init,   /* Plugin Init      */
  test_sql_service_plugin_deinit, /* Plugin Deinit    */
  0x0100,                         /* 1.0              */
  NULL,                           /* status variables */
  NULL,                           /* system variables */
  NULL,                           /* config options   */
  0,                              /* flags            */
}
mysql_declare_plugin_end;
