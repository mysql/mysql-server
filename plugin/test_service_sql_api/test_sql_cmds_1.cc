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
#include <mysql/service_srv_session_info.h>
#include "sql_string.h" /* STRING_PSI_MEMORY_KEY */

#define STRING_BUFFER 256

static const char *sep = "======================================================\n";

#define WRITE_SEP() my_write(outfile, (uchar*)sep, strlen(sep), MYF(0))

static File outfile;

void WRITE_STR(const char *format)
{
  char buffer[STRING_BUFFER];
  my_snprintf(buffer,sizeof(buffer),format);
  my_write(outfile,(uchar*)buffer,strlen(buffer),MYF(0));
}

template<typename T>
void WRITE_VAL(const char *format, T value)
{
  char buffer[STRING_BUFFER];
  my_snprintf(buffer,sizeof(buffer),format,value);
  my_write(outfile,(uchar*)buffer,strlen(buffer),MYF(0));
}

template<typename T1, typename T2>
void WRITE_VAL(const char *format, T1 value1, T2 value2)
{
  char buffer[STRING_BUFFER];
  my_snprintf(buffer,sizeof(buffer),format,value1,value2);
  my_write(outfile,(uchar*)buffer,strlen(buffer),MYF(0));
}


static const char *user_localhost = "localhost";
static const char *user_local = "127.0.0.1";
static const char *user_db= "";
static const char *user_privileged= "root";
static const char *user_ordinary= "ordinary";

static void switch_user(MYSQL_SESSION session, const char *user)
{
  MYSQL_SECURITY_CONTEXT sc;
  thd_get_security_context(srv_session_info_get_thd(session), &sc);
  security_context_lookup(sc, user, user_localhost, user_local, user_db);
}


static void ensure_api_ok(const char *function, int result)
{
  if (result != 0)
  {
    WRITE_VAL("ERROR calling %s: returned %i\n", function, result);
  }
}

static void ensure_api_not_null(const char *function, void *result)
{
  if (!result)
  {
    WRITE_VAL("ERROR calling %s: returned NULL\n", function);
  }
}
#define ENSURE_API_OK(call) ensure_api_ok(__FUNCTION__, (call));
#define ENSURE_API_NOT_NULL(call) ensure_api_not_null(__FUNCTION__, (call));


struct Callback_data
{
  int err;
  std::string errmsg;
  std::string sqlstate;
  bool error_called;

  int server_status;
  uint warn_count;
  uint affected_rows;
  uint last_insert_id;
  std::string message;

  int shutdown;
  bool shutdown_called;

  Callback_data() { reset(); }

  void reset()
  {
    error_called = false;
    errmsg.clear();
    sqlstate.clear();
    message.clear();
    err = 0;
    server_status = 0;
    warn_count = 0;
    affected_rows = 0;
    last_insert_id = 0;
    shutdown = 0;
    shutdown_called = 0;
  }
};


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

const CHARSET_INFO *sql_resultcs= NULL;
uint sql_num_meta_rows= 0;
uint sql_num_rows= 0;
uint col_count= 0;
uint sql_num_cols= 0;
uint sql_flags= 0;
st_send_field_n sql_field[64][64];

int row_count= 0;


static int sql_start_result_metadata(void *ctx, uint num_cols, uint flags,
                                     const CHARSET_INFO *resultcs)
{
  DBUG_ENTER("sql_start_result_metadata");
  DBUG_PRINT("info",("resultcs->number: %d", resultcs->number));
  DBUG_PRINT("info",("resultcs->csname: %s", resultcs->csname));
  DBUG_PRINT("info",("resultcs->name: %s", resultcs->name));
  row_count= 0;
  sql_num_cols= num_cols;
  sql_resultcs= resultcs;
  DBUG_RETURN(false);
}

static int sql_field_metadata(void *ctx, struct st_send_field *field,
                              const CHARSET_INFO *charset)
{
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
  strcpy(sql_field[col_count][row_count].db_name,(char*)field->db_name);
  strcpy(sql_field[col_count][row_count].table_name,(char*)field->table_name);
  strcpy(sql_field[col_count][row_count].org_table_name,(char*)field->org_table_name);
  strcpy(sql_field[col_count][row_count].col_name,(char*)field->col_name);
  strcpy(sql_field[col_count][row_count].org_col_name,(char*)field->org_col_name);
  sql_field[col_count][row_count].length= field->length;
  sql_field[col_count][row_count].charsetnr= field->charsetnr;
  sql_field[col_count][row_count].flags= field->flags;
  sql_field[col_count][row_count].decimals= field->decimals;
  sql_field[col_count][row_count].type= field->type;
  DBUG_RETURN(false);
}

static int sql_end_result_metadata(void *ctx, uint server_status, uint warn_count)
{
  DBUG_ENTER("sql_end_result_metadata");
  sql_num_meta_rows= row_count;
  row_count= 0;
  DBUG_RETURN(false);
}

static int sql_start_row(void *ctx)
{
  DBUG_ENTER("sql_start_row");
  col_count= 0;
  DBUG_RETURN(false);
}

static int sql_end_row(void *ctx)
{
  DBUG_ENTER("sql_end_row");
  row_count++;
  DBUG_RETURN(false);
}

static void sql_abort_row(void *ctx)
{
  DBUG_ENTER("sql_abort_row");
  DBUG_VOID_RETURN;
}

static ulong sql_get_client_capabilities(void *ctx)
{
  DBUG_ENTER("sql_get_client_capabilities");
  DBUG_RETURN(0);
}

static int sql_get_null(void *ctx)
{
  DBUG_ENTER("sql_get_null");
  DBUG_RETURN(false);
}

static int sql_get_integer(void * ctx, longlong value)
{
  DBUG_ENTER("sql_get_integer");
  DBUG_RETURN(false);
}

static int sql_get_longlong(void * ctx, longlong value, uint is_unsigned)
{
  DBUG_ENTER("sql_get_longlong");
  DBUG_RETURN(false);
}

static int sql_get_decimal(void * ctx, const decimal_t * value)
{
  DBUG_ENTER("sql_get_decimal");
  DBUG_RETURN(false);
}

static int sql_get_double(void * ctx, double value, uint32 decimals)
{
  DBUG_ENTER("sql_get_double");
  DBUG_RETURN(false);
}

static int sql_get_date(void * ctx, const MYSQL_TIME * value)
{
  DBUG_ENTER("sql_get_date");
  DBUG_RETURN(false);
}

static int sql_get_time(void * ctx, const MYSQL_TIME * value, uint decimals)
{
  DBUG_ENTER("sql_get_time");
  DBUG_RETURN(false);
}

static int sql_get_datetime(void * ctx, const MYSQL_TIME * value, uint decimals)
{
  DBUG_ENTER("sql_get_datetime");
  DBUG_RETURN(false);
}

char sql_str_value[64][64][256];
size_t sql_str_len[64][64];

static int sql_get_string(void * ctx,
	                        const char * const value, size_t length,
		                      const CHARSET_INFO * const valuecs)
{
  DBUG_ENTER("sql_get_string");
  strncpy(sql_str_value[col_count][row_count],value,length);
  sql_str_len[col_count][row_count]= length;
  col_count++;
  DBUG_RETURN(false);
}

static void sql_handle_ok(void * ctx,
                          uint server_status, uint statement_warn_count,
                          ulonglong affected_rows, ulonglong last_insert_id,
                          const char * const message)
{
  DBUG_ENTER("sql_handle_ok");

  Callback_data *cbd = (Callback_data*)ctx;

  cbd->server_status = server_status;
  cbd->warn_count = statement_warn_count;
  cbd->affected_rows = affected_rows;
  cbd->last_insert_id = last_insert_id;
  cbd->message = message ? message : "";

  DBUG_VOID_RETURN;
}

static void sql_handle_error(void * ctx, uint sql_errno, const char * const err_msg,
	                           const char * const sqlstate)
{
  DBUG_ENTER("sql_handle_error");
  Callback_data *cbd = (Callback_data*)ctx;
  WRITE_VAL("ERROR %i %s\n", sql_errno, err_msg);
  cbd->error_called = true;
  cbd->err = sql_errno;
  cbd->errmsg = err_msg ? err_msg : "";
  cbd->sqlstate = sqlstate ? sqlstate : "";
  DBUG_VOID_RETURN;
}

static void sql_shutdown(void *ctx, int shutdown_server)
{
  DBUG_ENTER("sql_shutdown");
  Callback_data *cbd = (Callback_data*)ctx;

  cbd->shutdown = shutdown_server;
  cbd->shutdown_called = true;
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

/****************************************************************************************/

static void test_com_query(void *p)
{
  DBUG_ENTER("test_com_query");

  /* Session declarations */
  MYSQL_SESSION st_session;
  void *plugin_ctx=NULL;
  bool session_ret= false;
  my_bool fail= false;
  COM_DATA cmd;
  Callback_data cbd;

  WRITE_STR("COM_QUERY");

  /* Open session 1: Must pass */
  st_session= srv_session_open(NULL,plugin_ctx);
  if (!st_session) {
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "srv_session_open failed.");
  }
  else
    switch_user(st_session, user_privileged);

  WRITE_STR("-----------------------------------------------------------------\n");
  memset(&sql_str_value, 0, 64 * 64 * 256 * sizeof(char));
  memset(&sql_str_len, 0, 64 * 64 * sizeof(size_t));
  cmd.com_query.query= "SELECT id,info FROM information_schema.processlist";
  cmd.com_query.length= strlen(cmd.com_query.query);
  WRITE_VAL("%s\n", cmd.com_query.query);
  fail= command_service_run_command(st_session,
                                    COM_QUERY,
                                    &cmd,
                                    &my_charset_utf8_general_ci,
                                    &sql_cbs,
                                    CS_TEXT_REPRESENTATION,
                                    &cbd);
  if (fail)
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "sql_simple ret code: %d\n", fail);
  else
  {
    /* get values */
    WRITE_STR("-----------------------------------------------------------------\n");
    WRITE_VAL("%s\t\%s\n",sql_field[0][0].col_name,
                sql_field[0][1].col_name);
    for (uint row= 0; row < sql_num_rows; row++)
    {
      for (uint col=0; col < sql_num_cols; col++)
      {
        WRITE_VAL("%s\n",sql_str_value[col][row]);
      }
    }
    /* start metadata */
    WRITE_VAL("num_cols: %d\n", sql_num_cols);
    /* end metadata */
    if (cbd.err)
    {
      WRITE_VAL("error: %d\n", cbd.err);
      WRITE_VAL("error msg: %s\n", cbd.errmsg.c_str());
    }
    else
    {
      WRITE_VAL("server status: %d\n", cbd.server_status);
      WRITE_VAL("warn count: %d\n", cbd.warn_count);
      //           WRITE_VAL("messsage: %s\n",msg);
    }
  }

  /* 2. statement */
  WRITE_STR("-----------------------------------------------------------------\n");
  memset(&sql_str_value, 0, 64 * 64 * 256 * sizeof(char));
  memset(&sql_str_len, 0, 64 * 64 * sizeof(size_t));
  cmd.com_query.query= "SELECT * FROM information_schema.global_variables WHERE variable_name LIKE 'INNODB_READ_IO_THREADS'";
  cmd.com_query.length= strlen(cmd.com_query.query);
  WRITE_VAL("%s\n", cmd.com_query.query);
  cbd.reset();
  fail= command_service_run_command(st_session,
                                    COM_QUERY,
                                    &cmd,
                                    &my_charset_utf8_general_ci,
                                    &sql_cbs, CS_TEXT_REPRESENTATION, &cbd);
  if (fail)
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "sql_simple ret code: %d\n", fail);
  else
  {
    /* get values */
    WRITE_STR("-----------------------------------------------------------------\n");
    WRITE_VAL("%s\t\%s\n", sql_field[0][0].col_name, sql_field[0][1].col_name);
    for (uint row=0; row < sql_num_rows; row++)
    {
      for (uint col=0; col < sql_num_cols; col+=2)
      {
        WRITE_VAL("%s\t\%s\n",sql_str_value[col][row], sql_str_value[col + 1][row]);
      }
    }
  }
  /* start metadata */
  WRITE_VAL("num_cols: %d\n",sql_num_cols);
  /* end metadata */
  if (cbd.err)
  {
    WRITE_VAL("error: %d\n",cbd.err);
    WRITE_VAL("error msg: %s\n",cbd.errmsg.c_str());
  }
  else
  {
    WRITE_VAL("server status: %d\n",cbd.server_status);
    WRITE_VAL("warn count: %d\n",cbd.warn_count);
  }

  // 3. statement must fail
  cbd.reset();
  cmd.com_query.query = "garbage";
  cmd.com_query.length = strlen(cmd.com_query.query);

  ENSURE_API_OK(command_service_run_command(st_session,COM_QUERY,&cmd,&my_charset_utf8_general_ci,
                                            &sql_cbs, CS_TEXT_REPRESENTATION, &cbd));

  WRITE_VAL("error after bad SQL: %i: %s\n", cbd.err, cbd.errmsg.c_str());

  /* close session 1: Must pass */
  WRITE_STR("srv_session_close.\n");
  session_ret= srv_session_close(st_session);
  if (session_ret)
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "srv_session_close failed.");

  DBUG_VOID_RETURN;
}


static int test_com_init_db(void *p)
{
  DBUG_ENTER("test_com_init_db");

  MYSQL_SESSION st_session;

  ENSURE_API_NOT_NULL(st_session = srv_session_open(NULL, p));

  if (st_session)
     switch_user(st_session, user_privileged);
  COM_DATA cmd;

  LEX_CSTRING db_name= srv_session_info_get_current_db(st_session);
  WRITE_VAL("current_db before init_db : %s\n", db_name.str);

  cmd.com_init_db.db_name = "mysql";
  cmd.com_init_db.length = strlen("mysql");
  Callback_data cbd;
  ENSURE_API_OK(command_service_run_command(st_session, COM_INIT_DB, &cmd,
                                            &my_charset_utf8_general_ci,
                                            &sql_cbs, CS_TEXT_REPRESENTATION,
                                            &cbd));

  db_name= srv_session_info_get_current_db(st_session);
  WRITE_VAL("current_db after init_db  : %s\n", db_name.str);

  ENSURE_API_OK(srv_session_close(st_session));

  DBUG_RETURN(0);
}


/*
static int test_com_list_fields(void *p)
{
  DBUG_ENTER("test_com_list_fields");

  MYSQL_SESSION st_session;

  ENSURE_API_NOT_NULL(st_session = srv_session_open(NULL, p));

  COM_DATA cmd;

  cmd.com_init_db.db_name = "mysql";
  cmd.com_init_db.length = strlen("mysql");
  ENSURE_API_OK(command_service_run_command(st_session, COM_INIT_DB, &cmd, &my_charset_utf8_general_ci,
                                            &sql_cbs, CS_TEXT_REPRESENTATION, p));

  WRITE_VAL("switched default db to: %s\n", srv_session_info_get_current_db(st_session));


  WRITE_STR("field_list\n");
  cmd.com_field_list.table_name = (unsigned char*)"user";
  cmd.com_field_list.table_name_length = strlen((const char*)cmd.com_field_list.table_name);
  cmd.com_field_list.query = (unsigned char*)"%";
  cmd.com_field_list.query_length = strlen((const char*)cmd.com_field_list.query);
  ENSURE_API_OK(command_service_run_command(st_session, COM_FIELD_LIST, &cmd, &my_charset_utf8_general_ci,
                                            &sql_cbs, CS_TEXT_REPRESENTATION, p));

  WRITE_STR("-----------------------------------------------------------------\n");
  for (uint row_count=0;row_count < sql_num_rows;row_count++){
    for (uint col_count=0;col_count < sql_num_cols;col_count+=2){
      WRITE_VAL("%s\t\%s\n",sql_str_value[col_count][row_count],
                  sql_str_value[col_count+1][row_count]);
    }
  }

  ENSURE_API_OK(srv_session_close(st_session));

  DBUG_RETURN(0);
}
*/


struct Test_data
{
  void *p;
  MYSQL_SESSION session;
  native_mutex_t mutex;
  native_cond_t cond;
  int ready;

  Test_data()
  {
    ready = 0;
    native_cond_init(&cond);
    native_mutex_init(&mutex, NULL);
  }

  ~Test_data()
  {
    native_cond_destroy(&cond);
    native_mutex_destroy(&mutex);
  }

  void wait(int value)
  {
    native_mutex_lock(&mutex);
    while (ready < value)
        native_cond_wait(&cond, &mutex);
    native_mutex_unlock(&mutex);
  }

  void go()
  {
    native_mutex_lock(&mutex);
    ready++;
    native_cond_signal(&cond);
    native_mutex_unlock(&mutex);
  }
};

static void* test_session_thread(Test_data *tdata)
{
  COM_DATA cmd;
  Callback_data cbdata;

  if (srv_session_init_thread(tdata->p))
    my_plugin_log_message(&tdata->p, MY_ERROR_LEVEL, "srv_session_init_thread failed.");

  WRITE_VAL("session is dead? %i\n", thd_killed(srv_session_info_get_thd(tdata->session)));

  cmd.com_query.query = "select sleep(10)";
  cmd.com_query.length = strlen("select sleep(10)");

  WRITE_VAL("Executing %s\n", cmd.com_query.query);

  tdata->go();

  int r = command_service_run_command(tdata->session, COM_QUERY, &cmd, &my_charset_utf8_general_ci,
                                      &sql_cbs, CS_TEXT_REPRESENTATION, &cbdata);
  WRITE_VAL("Killed run_command return value: %i\n", r);

  WRITE_VAL("thread shutdown: %i (%s)\n", cbdata.shutdown, cbdata.shutdown_called ? "yes":"no");
  WRITE_VAL("thread error: %i\n", cbdata.err);
  WRITE_VAL("thread error msg: %s\n", cbdata.errmsg.c_str());

  WRITE_VAL("session is dead (after)? %i\n", thd_killed(srv_session_info_get_thd(tdata->session)));

  srv_session_detach(tdata->session);

  srv_session_deinit_thread();

  return NULL;
}


static void session_error_cb(void *ctx, unsigned int sql_errno, const char *err_msg)
{
  WRITE_STR("default error handler called\n");
  WRITE_VAL("sql_errno = %i\n", sql_errno);
  WRITE_VAL("errmsg = %s\n", err_msg);
}


static int test_query_kill(void *p)
{
  DBUG_ENTER("test_query_kill");

  MYSQL_SESSION st_session;

  WRITE_STR("test_query_kill\n");

  ENSURE_API_NOT_NULL(st_session = srv_session_open(NULL, p));

  switch_user(st_session, user_privileged);
  MYSQL_SESSION st_session_victim;
  ENSURE_API_NOT_NULL(st_session_victim = srv_session_open(session_error_cb, p));

  Test_data tdata;

  tdata.p= p;
  tdata.session = st_session_victim;

  my_thread_handle thread_handle;
  {
    my_thread_attr_t attr;

    my_thread_attr_init(&attr);
    (void) my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);

    if (my_thread_create(&thread_handle, &attr, (void *(*)(void *))test_session_thread, &tdata) != 0)
    {
      WRITE_STR("Could not create test services thread!\n");
      exit(1);
    }
  }

  // wait for thread to be ready
  tdata.wait(1);

  COM_DATA cmd;
  Callback_data cbd;

  sleep(1);
  char buffer[200];
  my_snprintf(buffer, sizeof(buffer), "kill query %i", srv_session_info_get_session_id(st_session_victim));
  WRITE_STR("run KILL QUERY\n");
  cmd.com_query.query = buffer;
  cmd.com_query.length = strlen(buffer);
  ENSURE_API_OK(command_service_run_command(st_session, COM_QUERY, &cmd, &my_charset_utf8_general_ci,
                                           &sql_cbs, CS_TEXT_REPRESENTATION, &cbd));

  void *ret;
  my_thread_join(&thread_handle, &ret);
  WRITE_STR("OK\n");

  ENSURE_API_OK(srv_session_close(st_session));
  ENSURE_API_OK(srv_session_close(st_session_victim));

  DBUG_RETURN(0);
}


static int test_com_process_kill(void *p)
{
  DBUG_ENTER("test_com_process_kill");

  MYSQL_SESSION st_session;
  Callback_data cbd;

  WRITE_STR("COM_KILL\n");

  ENSURE_API_NOT_NULL(st_session = srv_session_open(NULL, p));

  switch_user(st_session, user_privileged);
  MYSQL_SESSION st_session_victim;
  ENSURE_API_NOT_NULL(st_session_victim = srv_session_open(session_error_cb, p));

  WRITE_VAL("session is dead? %i\n", thd_killed(srv_session_info_get_thd(st_session_victim)));

  COM_DATA cmd;

  cmd.com_kill.id = srv_session_info_get_session_id(st_session_victim);
  ENSURE_API_OK(command_service_run_command(st_session, COM_PROCESS_KILL, &cmd, &my_charset_utf8_general_ci,
                                            &sql_cbs, CS_TEXT_REPRESENTATION, &cbd));

  WRITE_VAL("session is dead now? %i\n", thd_killed(srv_session_info_get_thd(st_session_victim)));

  ENSURE_API_OK(srv_session_close(st_session));
  ENSURE_API_OK(srv_session_close(st_session_victim));

  DBUG_RETURN(0);
}


static int test_priv(void *p)
{
  DBUG_ENTER("test_priv");

  MYSQL_SESSION root_session;
  Callback_data cbd;
  COM_DATA cmd;

  WRITE_STR("COM_QUERY with priv\n");

  ENSURE_API_NOT_NULL(root_session = srv_session_open(NULL, p));

  switch_user(root_session, user_privileged);

  cmd.com_query.query = "create user ordinary@localhost";
  cmd.com_query.length = strlen(cmd.com_query.query);
  ENSURE_API_OK(command_service_run_command(root_session, COM_QUERY, &cmd, &my_charset_utf8_general_ci,
                                            &sql_cbs, CS_TEXT_REPRESENTATION, &cbd));
  WRITE_VAL("create user as root: %i %s\n", cbd.err, cbd.errmsg.c_str());

  WRITE_STR("now try as ordinary user\n");
  {
    MYSQL_SESSION ordinary_session;
    ENSURE_API_NOT_NULL(ordinary_session = srv_session_open(NULL, p));
    switch_user(ordinary_session, user_ordinary);

    cbd.reset();
    cmd.com_query.query = "create user bogus@localhost";
    cmd.com_query.length = strlen(cmd.com_query.query);
    ENSURE_API_OK(command_service_run_command(ordinary_session, COM_QUERY, &cmd, &my_charset_utf8_general_ci,
                                            &sql_cbs, CS_TEXT_REPRESENTATION, &cbd));

    WRITE_VAL("create user supposed to fail: %i %s\n", cbd.err, cbd.errmsg.c_str());

    ENSURE_API_OK(srv_session_close(ordinary_session));
  }

  cbd.reset();
  cmd.com_query.query = "drop user ordinary@localhost";
  cmd.com_query.length = strlen(cmd.com_query.query);
  ENSURE_API_OK(command_service_run_command(root_session, COM_QUERY, &cmd, &my_charset_utf8_general_ci,
                                            &sql_cbs, CS_TEXT_REPRESENTATION, &cbd));
  WRITE_VAL("drop user as root: %i %s\n", cbd.err, cbd.errmsg.c_str());


  ENSURE_API_OK(srv_session_close(root_session));

  DBUG_RETURN(0);
}


static void test_sql(void *p)
{
  DBUG_ENTER("test_sql");
  my_plugin_log_message(&p, MY_INFORMATION_LEVEL, "Installation.");

  WRITE_SEP();
  test_com_query(p);
  WRITE_SEP();
  test_com_init_db(p);
  WRITE_SEP();
//  test_com_list_fields(p);
//  WRITE_SEP();
  test_com_process_kill(p);
  WRITE_SEP();
  test_query_kill(p);
  WRITE_SEP();
  test_priv(p);

  DBUG_VOID_RETURN;
}

static void create_log_file(const char * log_name)
{
  char filename[FN_REFLEN];

  fn_format(filename, log_name, "", ".log",
            MY_REPLACE_EXT | MY_UNPACK_FILENAME);
  unlink(filename);
  outfile= my_open(filename, O_CREAT|O_RDWR, MYF(0));
}

static const char *log_filename= "test_sql_cmds_1";


static int test_sql_service_plugin_init(void *p)
{
  DBUG_ENTER("test_sql_service_plugin_init");
  my_plugin_log_message(&p, MY_INFORMATION_LEVEL, "Installation.");

  create_log_file(log_filename);

  /* Test of service: sql */
  test_sql(p);

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
{ MYSQL_DAEMON_INTERFACE_VERSION  };

/*
  Plugin library descriptor
*/

mysql_declare_plugin(test_daemon)
{
  MYSQL_DAEMON_PLUGIN,
  &test_sql_service_plugin,
  "test_sql_cmds_1",
  "Horst Hunger, Andrey Hristov",
  "Test sql service commands",
  PLUGIN_LICENSE_GPL,
  test_sql_service_plugin_init, /* Plugin Init */
  test_sql_service_plugin_deinit, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL,                       /* config options                  */
  0,                          /* flags                           */
}
mysql_declare_plugin_end;
