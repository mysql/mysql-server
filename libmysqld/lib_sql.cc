/*
 * Copyright (c)  2000, 2014
 * SWsoft  company
 *
 * Modifications copyright (c) 2001, 2015. Oracle and/or its affiliates.
 * All rights reserved.
 *
 * This material is provided "as is", with absolutely no warranty expressed
 * or implied. Any use is at your own risk.
 *
 * Permission to use or copy this software for any purpose is hereby granted 
 * without fee, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 *

  This code was modified by the MySQL team
*/

/*
  The following is needed to not cause conflicts when we include mysqld.cc
*/

extern "C"
{
  extern unsigned long max_allowed_packet, net_buffer_length;
}

#include "../sql/mysqld.cc"

extern "C" {

#include <mysql.h>
#undef ER
#include "errmsg.h"
#include "embedded_priv.h"

} // extern "C"

#include <algorithm>

using std::min;
using std::max;

extern "C" {

extern unsigned int mysql_server_last_errno;
extern char mysql_server_last_error[MYSQL_ERRMSG_SIZE];
static my_bool emb_read_query_result(MYSQL *mysql);


void unireg_clear(int exit_code)
{
  DBUG_ENTER("unireg_clear");
  clean_up(!opt_help && (exit_code || !opt_bootstrap)); /* purecov: inspected */
  my_end(opt_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
  DBUG_VOID_RETURN;
}

/*
  Wrapper error handler for embedded server to call client/server error 
  handler based on whether thread is in client/server context
*/

static void embedded_error_handler(uint error, const char *str, myf MyFlags)
{
  DBUG_ENTER("embedded_error_handler");

  /* 
    If current_thd is NULL, it means restore_global has been called and 
    thread is in client context, then call client error handler else call 
    server error handler.
  */
  DBUG_RETURN(current_thd ? my_message_sql(error, str, MyFlags):
              my_message_stderr(error, str, MyFlags));
}


/*
  Reads error information from the MYSQL_DATA and puts
  it into proper MYSQL members

  SYNOPSIS
    embedded_get_error()
    mysql        connection handler
    data         query result

  NOTES
    after that function error information will be accessible
       with usual functions like mysql_error()
    data is my_free-d in this function
    most of the data is stored in data->embedded_info structure
*/

void embedded_get_error(MYSQL *mysql, MYSQL_DATA *data)
{
  NET *net= &mysql->net;
  struct embedded_query_result *ei= data->embedded_info;
  net->last_errno= ei->last_errno;
  strmake(net->last_error, ei->info, sizeof(net->last_error)-1);
  memcpy(net->sqlstate, ei->sqlstate, sizeof(net->sqlstate));
  mysql->server_status= ei->server_status;
  free_root(&data->alloc, MYF(0));
  my_free(data);
}

static my_bool
emb_advanced_command(MYSQL *mysql, enum enum_server_command command,
		     const uchar *header, size_t header_length,
		     const uchar *arg, size_t arg_length, my_bool skip_check,
                     MYSQL_STMT *stmt)
{
  my_bool result= 1;
  THD *thd=(THD *) mysql->thd;
  NET *net= &mysql->net;
  my_bool stmt_skip= stmt ? stmt->state != MYSQL_STMT_INIT_DONE : FALSE;
  union COM_DATA com_data;

  if (!thd)
  {
    /* Do "reconnect" if possible */
    if (mysql_reconnect(mysql) || stmt_skip)
      return 1;
    thd= (THD *) mysql->thd;
  }

#if defined(ENABLED_PROFILING)
  thd->profiling.start_new_query();
#endif

  thd->clear_data_list();
  /* Check that we are calling the client functions in right order */
  if (mysql->status != MYSQL_STATUS_READY)
  {
    set_mysql_error(mysql, CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate);
    result= 1;
    goto end;
  }

  /* Clear result variables */
  thd->clear_error();
  thd->get_stmt_da()->reset_diagnostics_area();
  mysql->affected_rows= ~(my_ulonglong) 0;
  mysql->field_count= 0;
  net_clear_error(net);
  thd->current_stmt= stmt;

  thd->thread_stack= (char*) &thd;
  thd->store_globals();				// Fix if more than one connect
  /* 
     We have to call free_old_query before we start to fill mysql->fields 
     for new query. In the case of embedded server we collect field data
     during query execution (not during data retrieval as it is in remote
     client). So we have to call free_old_query here
  */
  free_old_query(mysql);

  thd->extra_length= arg_length;
  thd->extra_data= (char *)arg;
  if (header)
  {
    arg= header;
    arg_length= header_length;
  }

  thd->get_protocol_classic()->create_command(&com_data, command,
                                              (uchar *) arg, arg_length);
  result= dispatch_command(thd, &com_data, command);
  thd->cur_data= 0;

  if (!skip_check)
    result= thd->is_error() ? -1 : 0;

#if defined(ENABLED_PROFILING)
  thd->profiling.finish_current_query();
#endif

end:
  thd->restore_globals();
  return result;
}

static void emb_flush_use_result(MYSQL *mysql, my_bool)
{
  THD *thd= (THD*) mysql->thd;
  if (thd->cur_data)
  {
    free_rows(thd->cur_data);
    thd->cur_data= 0;
  }
  else if (thd->first_data)
  {
    MYSQL_DATA *data= thd->first_data;
    thd->first_data= data->embedded_info->next;
    free_rows(data);
  }
}


/*
  reads dataset from the next query result

  SYNOPSIS
  emb_read_rows()
  mysql		connection handle
  other parameters are not used

  NOTES
    It just gets next MYSQL_DATA from the result's queue

  RETURN
    pointer to MYSQL_DATA with the coming recordset
*/

static MYSQL_DATA *
emb_read_rows(MYSQL *mysql, MYSQL_FIELD *mysql_fields MY_ATTRIBUTE((unused)),
	      unsigned int fields MY_ATTRIBUTE((unused)))
{
  MYSQL_DATA *result= ((THD*)mysql->thd)->cur_data;
  ((THD*)mysql->thd)->cur_data= 0;
  if (result->embedded_info->last_errno)
  {
    embedded_get_error(mysql, result);
    return NULL;
  }
  *result->embedded_info->prev_ptr= NULL;
  return result;
}


static MYSQL_FIELD *emb_list_fields(MYSQL *mysql)
{
  MYSQL_DATA *res;
  if (emb_read_query_result(mysql))
    return 0;
  res= ((THD*) mysql->thd)->cur_data;
  ((THD*) mysql->thd)->cur_data= 0;
  mysql->field_alloc= res->alloc;
  my_free(res);
  mysql->status= MYSQL_STATUS_READY;
  return mysql->fields;
}

static my_bool emb_read_prepare_result(MYSQL *mysql, MYSQL_STMT *stmt)
{
  THD *thd= (THD*) mysql->thd;
  MYSQL_DATA *res;

  stmt->stmt_id= thd->client_stmt_id;
  stmt->param_count= thd->client_param_count;
  stmt->field_count= 0;
  mysql->warning_count= thd->get_stmt_da()->current_statement_cond_count();

  if (thd->first_data)
  {
    if (emb_read_query_result(mysql))
      return 1;
    stmt->field_count= mysql->field_count;
    mysql->status= MYSQL_STATUS_READY;
    res= thd->cur_data;
    thd->cur_data= NULL;
    if (!(mysql->server_status & SERVER_STATUS_AUTOCOMMIT))
      mysql->server_status|= SERVER_STATUS_IN_TRANS;

    stmt->fields= mysql->fields;
    stmt->mem_root= res->alloc;
    mysql->fields= NULL;
    my_free(res);
  }

  return 0;
}

/**************************************************************************
  Get column lengths of the current row
  If one uses mysql_use_result, res->lengths contains the length information,
  else the lengths are calculated from the offset between pointers.
**************************************************************************/

static void emb_fetch_lengths(ulong *to, MYSQL_ROW column,
			      unsigned int field_count)
{ 
  MYSQL_ROW end;

  for (end=column + field_count; column != end ; column++,to++)
    *to= *column ? *(uint *)((*column) - sizeof(uint)) : 0;
}

static my_bool emb_read_query_result(MYSQL *mysql)
{
  THD *thd= (THD*) mysql->thd;
  MYSQL_DATA *res= thd->first_data;
  DBUG_ASSERT(!thd->cur_data);
  thd->first_data= res->embedded_info->next;
  if (res->embedded_info->last_errno &&
      !res->embedded_info->fields_list)
  {
    embedded_get_error(mysql, res);
    return 1;
  }

  mysql->warning_count= res->embedded_info->warning_count;
  mysql->server_status= res->embedded_info->server_status;
  mysql->field_count= res->fields;
  if (!(mysql->fields= res->embedded_info->fields_list))
  {
    mysql->affected_rows= res->embedded_info->affected_rows;
    mysql->insert_id= res->embedded_info->insert_id;
  }
  net_clear_error(&mysql->net);
  mysql->info= 0;

  if (res->embedded_info->info[0])
  {
    strmake(mysql->info_buffer, res->embedded_info->info, MYSQL_ERRMSG_SIZE-1);
    mysql->info= mysql->info_buffer;
  }

  if (res->embedded_info->fields_list)
  {
    mysql->status=MYSQL_STATUS_GET_RESULT;
    thd->cur_data= res;
  }
  else
    my_free(res);

  return 0;
}

#define HEADER_SIZE 9
static int emb_stmt_execute(MYSQL_STMT *stmt)
{
  DBUG_ENTER("emb_stmt_execute");
  /*
    Header size is made similar to non-embedded library.
    It should be consistent across embedded and non-embedded
    libraries.
  */
  uchar header[HEADER_SIZE];
  THD *thd;
  my_bool res;

  int4store(header, stmt->stmt_id);
  header[4]= (uchar) stmt->flags;
  /*
    Dummy value is stored in the last 4 bytes of the header
    to make it consistent with non-embedded library.
  */
  int4store(header + 5, 1);
  thd= (THD*)stmt->mysql->thd;
  thd->client_param_count= stmt->param_count;
  thd->client_params= stmt->params;

  res= MY_TEST(emb_advanced_command(stmt->mysql, COM_STMT_EXECUTE, 0, 0,
                                    header, sizeof(header), 1, stmt) ||
               emb_read_query_result(stmt->mysql));
  stmt->affected_rows= stmt->mysql->affected_rows;
  stmt->insert_id= stmt->mysql->insert_id;
  stmt->server_status= stmt->mysql->server_status;
  if (res)
  {
    NET *net= &stmt->mysql->net;
    set_stmt_errmsg(stmt, net);
    DBUG_RETURN(1);
  }
  else if (stmt->mysql->status == MYSQL_STATUS_GET_RESULT)
           stmt->mysql->status= MYSQL_STATUS_STATEMENT_GET_RESULT;
  DBUG_RETURN(0);
}

int emb_read_binary_rows(MYSQL_STMT *stmt)
{
  MYSQL_DATA *data;
  if (!(data= emb_read_rows(stmt->mysql, 0, 0)))
  {
    set_stmt_errmsg(stmt, &stmt->mysql->net);
    return 1;
  }
  stmt->result= *data;
  my_free(data);
  set_stmt_errmsg(stmt, &stmt->mysql->net);
  return 0;
}

int emb_read_rows_from_cursor(MYSQL_STMT *stmt)
{
  MYSQL *mysql= stmt->mysql;
  THD *thd= (THD*) mysql->thd;
  MYSQL_DATA *res= thd->first_data;
  DBUG_ASSERT(!thd->first_data->embedded_info->next);
  thd->first_data= 0;
  if (res->embedded_info->last_errno)
  {
    embedded_get_error(mysql, res);
    set_stmt_errmsg(stmt, &mysql->net);
    return 1;
  }

  thd->cur_data= res;
  mysql->warning_count= res->embedded_info->warning_count;
  mysql->server_status= res->embedded_info->server_status;
  net_clear_error(&mysql->net);

  return emb_read_binary_rows(stmt);
}

int emb_unbuffered_fetch(MYSQL *mysql, char **row)
{
  THD *thd= (THD*) mysql->thd;
  MYSQL_DATA *data= thd->cur_data;
  if (data && data->embedded_info->last_errno)
  {
    embedded_get_error(mysql, data);
    thd->cur_data= 0;
    return 1;
  }
  if (!data || !data->data)
  {
    *row= NULL;
    if (data)
    {
      thd->cur_data= thd->first_data;
      thd->first_data= data->embedded_info->next;
      free_rows(data);
    }
  }
  else
  {
    *row= (char *)data->data->data;
    data->data= data->data->next;
  }
  return 0;
}

static void emb_free_embedded_thd(MYSQL *mysql)
{
  THD *thd= (THD*)mysql->thd;
  thd->clear_data_list();
  thd->store_globals();
  thd->release_resources();
  Global_THD_manager::get_instance()->remove_thd(thd);
  delete thd;
  mysql->thd=0;
}

static const char * emb_read_statistics(MYSQL *mysql)
{
  THD *thd= (THD*)mysql->thd;
  return thd->is_error() ? thd->get_stmt_da()->message_text() : "";
}


static MYSQL_RES * emb_store_result(MYSQL *mysql)
{
  return mysql_store_result(mysql);
}

int emb_read_change_user_result(MYSQL *mysql)
{
  mysql->net.read_pos= (uchar*)""; // fake an OK packet
  return mysql_errno(mysql) ? static_cast<int>packet_error :
                              1 /* length of the OK packet */;
}

MYSQL_METHODS embedded_methods= 
{
  emb_read_query_result,
  emb_advanced_command,
  emb_read_rows,
  emb_store_result,
  emb_fetch_lengths, 
  emb_flush_use_result,
  emb_read_change_user_result,
  emb_list_fields,
  emb_read_prepare_result,
  emb_stmt_execute,
  emb_read_binary_rows,
  emb_unbuffered_fetch,
  emb_free_embedded_thd,
  emb_read_statistics,
  emb_read_query_result,
  emb_read_rows_from_cursor,
  free_rows
};

/*
  Make a copy of array and the strings array points to
*/

char **copy_arguments(int argc, char **argv)
{
  size_t length= 0;
  char **from, **res, **end= argv+argc;

  for (from=argv ; from != end ; from++)
    length+= strlen(*from);

  if ((res= (char**) my_malloc(PSI_NOT_INSTRUMENTED,
                               sizeof(argv)*(argc+1)+length+argc,
			       MYF(MY_WME))))
  {
    char **to= res, *to_str= (char*) (res+argc+1);
    for (from=argv ; from != end ;)
    {
      *to++= to_str;
      to_str= my_stpcpy(to_str, *from++)+1;
    }
    *to= 0;					// Last ptr should be null
  }
  return res;
}

char **		copy_arguments_ptr= 0;

int init_embedded_server(int argc, char **argv, char **groups)
{
  /*
    This mess is to allow people to call the init function without
    having to mess with a fake argv
   */
  int *argcp= NULL;
  char ***argvp= NULL;
  int fake_argc= 1;
  char *fake_argv[2];
  char **foo= &fake_argv[0];
  char fake_server[]= "server";
  char fake_embedded[]= "embedded";
  char *fake_groups[]= { fake_server, fake_embedded, NULL };
  char fake_name[]= "fake_name";
  my_bool acl_error;

  if (my_thread_init())
    return 1;

  if (argc)
  {
    argcp= &argc;
    argvp= &argv;
  }
  else
  {
    fake_argv[0]= fake_name;
    fake_argv[1]= NULL;

    argcp= &fake_argc;
    argvp= &foo;
  }
  if (!groups)
    groups= fake_groups;

  my_progname= "mysql_embedded";

  /*
    Perform basic query log initialization. Should be called after
    MY_INIT, as it initializes mutexes.
  */
  query_logger.init();

  orig_argc= *argcp;
  orig_argv= *argvp;
  if (load_defaults("my", (const char **)groups, argcp, argvp))
    return 1;
  defaults_argc= *argcp;
  defaults_argv= *argvp;
  remaining_argc= *argcp;
  remaining_argv= *argvp;

  /* Must be initialized early for comparison of options name */
  system_charset_info= &my_charset_utf8_general_ci;
  sys_var_init();

  if (handle_early_options() != 0)
  {
    return 1;
  }

  ulong requested_open_files_dummy;
  adjust_related_options(&requested_open_files_dummy);

  if (init_common_variables())
  {
    mysql_server_end();
    return 1;
  }

  mysql_data_home= mysql_real_data_home;
  mysql_data_home_len= mysql_real_data_home_len;

  /* Get default temporary directory */
  opt_mysql_tmpdir=getenv("TMPDIR");	/* Use this if possible */
#if defined(_WIN32)
  if (!opt_mysql_tmpdir)
    opt_mysql_tmpdir=getenv("TEMP");
  if (!opt_mysql_tmpdir)
    opt_mysql_tmpdir=getenv("TMP");
#endif
  if (!opt_mysql_tmpdir || !opt_mysql_tmpdir[0])
    opt_mysql_tmpdir= const_cast<char*>(DEFAULT_TMPDIR); /* purecov: inspected*/

  init_ssl();
  umask(((~my_umask) & 0666));
  if (init_server_components())
  {
    mysql_server_end();
    return 1;
  }

  /*
    Each server should have one UUID. We will create it automatically, if it
    does not exist.
   */
  if (!opt_bootstrap && init_server_auto_options())
  {
    mysql_server_end();
    return 1;
  }

  /* 
    set error_handler_hook to embedded_error_handler wrapper.
  */
  error_handler_hook= embedded_error_handler;

  acl_error= 0;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  acl_error= acl_init(opt_noacl) || grant_init(opt_noacl);
#endif
  if (acl_error || my_tz_init((THD *)0, default_tz_name, opt_bootstrap))
  {
    mysql_server_end();
    return 1;
  }

  init_max_user_conn();
  init_update_queries();

  if (!opt_bootstrap)
    servers_init(0);

  start_handle_manager();

  // FIXME initialize binlog_filter and rpl_filter if not already done
  //       corresponding delete is in clean_up()
  if(!binlog_filter) binlog_filter = new Rpl_filter;
  if(!rpl_filter) rpl_filter = new Rpl_filter;

  if (opt_init_file)
  {
    if (read_init_file(opt_init_file))
    {
      mysql_server_end();
      return 1;
    }
  }

  execute_ddl_log_recovery();

  start_processing_signals();

#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
  /* engine specific hook, to be made generic */
  if (ndb_wait_setup_func && ndb_wait_setup_func(opt_ndb_wait_setup))
  {
    sql_print_warning("NDB : Tables not available after %lu seconds."
                      "  Consider increasing --ndb-wait-setup value",
                      opt_ndb_wait_setup);
  }
#endif

  return 0;
}

void end_embedded_server()
{
  my_free(copy_arguments_ptr);
  copy_arguments_ptr=0;
  clean_up(0);
}


void init_embedded_mysql(MYSQL *mysql, int client_flag)
{
  THD *thd = (THD *)mysql->thd;
  thd->mysql= mysql;
  mysql->server_version= server_version;
  mysql->client_flag= client_flag;
  init_alloc_root(PSI_NOT_INSTRUMENTED, &mysql->field_alloc, 8192, 0);
}

/**
  @brief Initialize a new THD for a connection in the embedded server

  @param client_flag  Client capabilities which this thread supports
  @return pointer to the created THD object

  @todo
  This function copies code from several places in the server, including
  create_new_thread(), and prepare_new_connection_state().  This should
  be refactored to avoid code duplication.
*/
void *create_embedded_thd(int client_flag)
{
  THD * thd= new THD;
  thd->set_new_thread_id();

  thd->thread_stack= (char*) &thd;
  if (thd->store_globals())
  {
    my_message_local(ERROR_LEVEL, "store_globals failed.");
    goto err;
  }
  lex_start(thd);

  /* TODO - add init_connect command execution */

  if (thd->variables.max_join_size == HA_POS_ERROR)
    thd->variables.option_bits |= OPTION_BIG_SELECTS;
  thd->proc_info=0;				// Remove 'login'
  thd->set_command(COM_SLEEP);
  thd->set_time();
  thd->init_for_queries();
  thd->get_protocol_classic()->set_client_capabilities(client_flag);
  thd->real_id= my_thread_self();

  thd->reset_db(NULL_CSTR);
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  thd->security_context()->set_db_access(DB_ACLS);
  thd->security_context()->set_master_access(~NO_ACCESS);
#endif
  thd->cur_data= 0;
  thd->first_data= 0;
  thd->data_tail= &thd->first_data;
  thd->get_protocol_classic()->wipe_net();
  Global_THD_manager::get_instance()->add_thd(thd);
  return thd;
err:
  delete(thd);
  return NULL;
}


static void
emb_transfer_connect_attrs(MYSQL *mysql)
{
#ifdef HAVE_PSI_THREAD_INTERFACE
  if (mysql->options.extension &&
      mysql->options.extension->connection_attributes_length)
  {
    uchar *buf, *ptr;
    THD *thd= (THD*)mysql->thd;
    size_t length= mysql->options.extension->connection_attributes_length;

    /* 9 = max length of the serialized length */
    ptr= buf= (uchar *) my_alloca(length + 9);
    send_client_connect_attrs(mysql, buf);
    net_field_length_ll(&ptr);
    PSI_THREAD_CALL(set_thread_connect_attrs)((char *) ptr, length, thd->charset());
  }
#endif
}


#ifdef NO_EMBEDDED_ACCESS_CHECKS
int check_embedded_connection(MYSQL *mysql, const char *db)
{
  int result;
  LEX_CSTRING db_lex_cstr= to_lex_cstring(db);
  THD *thd= (THD*)mysql->thd;

  /* the server does the same as the client */
  mysql->server_capabilities= mysql->client_flag;

  thd_init_client_charset(thd, mysql->charset->number);
  thd->update_charset();
  Security_context *sctx= thd->security_context();
  sctx->set_host_ptr(my_localhost, strlen(my_localhost));
  sctx->set_host_or_ip_ptr(sctx->host().str, sctx->host().length);
  sctx->assign_priv_user(mysql->user, strlen(mysql->user));
  sctx->assign_user(mysql->user, strlen(mysql->user));
  sctx->assign_proxy_user("", 0);
  sctx->assign_priv_host(my_localhost, strlen(my_localhost));
  sctx->set_master_access(GLOBAL_ACLS);       // Full rights
  emb_transfer_connect_attrs(mysql);
  /* Change database if necessary */
  if (!(result= (db && db[0] && mysql_change_db(thd, db_lex_cstr, false))))
    my_ok(thd);
  thd->send_statement_status();
  emb_read_query_result(mysql);
  return result;
}

#else
int check_embedded_connection(MYSQL *mysql, const char *db)
{
  /*
    we emulate a COM_CHANGE_USER user here,
    it's easier than to emulate the complete 3-way handshake
  */
  char *buf, *end;
  NET *net= &mysql->net;
  THD *thd= (THD*)mysql->thd;
  Security_context *sctx= thd->security_context();
  size_t connect_attrs_len=
    (mysql->server_capabilities & CLIENT_CONNECT_ATTRS &&
     mysql->options.extension) ?
    mysql->options.extension->connection_attributes_length : 0;

  buf= my_alloca(USERNAME_LENGTH + SCRAMBLE_LENGTH + 1 + 2*NAME_LEN + 2 +
                 connect_attrs_len + 2);
  if (mysql->options.client_ip)
  {
    sctx->set_host(my_strdup(mysql->options.client_ip, MYF(0)));
    sctx->set_ip(my_strdup(sctx->get_host()->ptr(), MYF(0)));
  }
  else
    sctx->set_host((char*)my_localhost);
  sctx->host_or_ip= sctx->host->ptr();

  if (acl_check_host(sctx->get_host()->ptr(), sctx->get_ip()->ptr()))
    goto err;

  /* construct a COM_CHANGE_USER packet */
  end= strmake(buf, mysql->user, USERNAME_LENGTH) + 1;

  memset(thd->scramble, 55, SCRAMBLE_LENGTH); // dummy scramble
  thd->scramble[SCRAMBLE_LENGTH]= 0;

  if (mysql->passwd && mysql->passwd[0])
  {
    *end++= SCRAMBLE_LENGTH;
    scramble(end, thd->scramble, mysql->passwd);
    end+= SCRAMBLE_LENGTH;
  }
  else
    *end++= 0;

  end= strmake(end, db ? db : "", NAME_LEN) + 1;

  int2store(end, (ushort) mysql->charset->number);
  end+= 2;

  end= strmake(end, "mysql_native_password", NAME_LEN) + 1;

  /* the server does the same as the client */
  mysql->server_capabilities= mysql->client_flag;

  end= (char *) send_client_connect_attrs(mysql, (uchar *) end);

  /* acl_authenticate() takes the data from thd->net->read_pos */
  thd->get_protocol_classic()->get_net()->read_pos= (uchar *)buf;

  if (acl_authenticate(thd, COM_CHANGE_USER))
  {
    x_free(thd->security_context()->user().str);
    goto err;
  }
  return 0;
err:
  strmake(net->last_error, thd->main_da.message(), sizeof(net->last_error)-1);
  memcpy(net->sqlstate,
         mysql_errno_to_sqlstate(thd->main_da.sql_errno()),
         sizeof(net->sqlstate)-1);
  return 1;
}
#endif

} // extern "C"


void THD::clear_data_list()
{
  while (first_data)
  {
    MYSQL_DATA *data= first_data;
    first_data= data->embedded_info->next;
    free_rows(data);
  }
  data_tail= &first_data;
  free_rows(cur_data);
  cur_data= 0;
}


static char *dup_str_aux(MEM_ROOT *root, const char *from, size_t length,
			 const CHARSET_INFO *fromcs, const CHARSET_INFO *tocs)
{
  size_t dummy_offset;
  uint dummy_err;
  char *result;

  /* 'tocs' is set 0 when client issues SET character_set_results=NULL */
  if (tocs && String::needs_conversion(0, fromcs, tocs, &dummy_offset))
  {
    size_t new_len= (tocs->mbmaxlen * length) / fromcs->mbminlen + 1;
    result= (char *)alloc_root(root, new_len);
    length= copy_and_convert(result, new_len,
                             tocs, from, length, fromcs, &dummy_err);
  }
  else
  {
    result= (char *)alloc_root(root, length + 1);
    memcpy(result, from, length);
  }

  result[length]= 0;
  return result;
}


/*
  creates new result and hooks it to the list

  SYNOPSIS
  alloc_new_dataset()

  NOTES
    allocs the MYSQL_DATA + embedded_query_result couple
    to store the next query result,
    links these two and attach it to the THD::data_tail

  RETURN
    pointer to the newly created query result
*/

MYSQL_DATA *THD::alloc_new_dataset()
{
  MYSQL_DATA *data;
  struct embedded_query_result *emb_data;
  if (!my_multi_malloc(PSI_NOT_INSTRUMENTED,
                       MYF(MY_WME | MY_ZEROFILL),
                       &data, sizeof(*data),
                       &emb_data, sizeof(*emb_data),
                       NULL))
    return NULL;

  emb_data->prev_ptr= &data->data;
  cur_data= data;
  *data_tail= data;
  data_tail= &emb_data->next;
  data->embedded_info= emb_data;
  return data;
}


/**
  Stores server_status and warning_count in the current
  query result structures.

  @param thd            current thread

  @note Should be called after we get the recordset-result.
*/

static
bool
write_eof_packet(THD *thd, uint server_status, uint statement_warn_count)
{
  if (!thd->mysql)            // bootstrap file handling
    return FALSE;
  /*
    The following test should never be true, but it's better to do it
    because if 'is_fatal_error' is set the server is not going to execute
    other queries (see the if test in dispatch_command / COM_QUERY)
  */
  if (thd->is_fatal_error)
    thd->server_status&= ~SERVER_MORE_RESULTS_EXISTS;
  thd->cur_data->embedded_info->server_status= server_status;
  /*
    Don't send warn count during SP execution, as the warn_list
    is cleared between substatements, and mysqltest gets confused
  */
  thd->cur_data->embedded_info->warning_count=
    (thd->sp_runtime_ctx ? 0 : min(statement_warn_count, 65535U));
  return FALSE;
}


/*
  allocs new query result and initialises Protocol::alloc

  SYNOPSIS
  Protocol::begin_dataset()

  RETURN
    0 if success
    1 if memory allocation failed
*/

int Protocol_classic::begin_dataset()
{
  MYSQL_DATA *data= m_thd->alloc_new_dataset();
  if (!data)
    return 1;
  alloc= &data->alloc;
  init_alloc_root(PSI_NOT_INSTRUMENTED, alloc, 8192, 0); /* Assume rowlength < 8192 */
  alloc->min_malloc=sizeof(MYSQL_ROWS);
  return 0;
}


/*
  remove last row of current recordset

  SYNOPSIS
  Protocol_text::abort_row()

  NOTES
    does the loop from the beginning of the current recordset to
    the last record and cuts it off.
    Not supposed to be frequently called.
*/

void
Protocol_text::abort_row()
{
  MYSQL_DATA *data= m_thd->cur_data;
  MYSQL_ROWS **last_row_hook= &data->data;
  my_ulonglong count= data->rows;
  DBUG_ENTER("Protocol_text::abort_row");
  while (--count)
    last_row_hook= &(*last_row_hook)->next;

  *last_row_hook= 0;
  data->embedded_info->prev_ptr= last_row_hook;
  data->rows--;

  DBUG_VOID_RETURN;
}


bool
Protocol_classic::send_field_metadata(Send_field *server_field,
                                      const CHARSET_INFO *item_charset)
{
  DBUG_ENTER("send_field_metadata");
  const CHARSET_INFO       *thd_cs= m_thd->variables.character_set_results;
  const CHARSET_INFO       *cs= system_charset_info;

  /* Keep things compatible for old clients */
  if (server_field->type == MYSQL_TYPE_VARCHAR)
     server_field->type= MYSQL_TYPE_VAR_STRING;

  client_field->def = 0;
  client_field->def_length= 0;
  client_field->max_length = 0;

  client_field->db= dup_str_aux(field_alloc, server_field->db_name,
                                strlen(server_field->db_name),
                                cs, thd_cs);
  client_field->table= dup_str_aux(field_alloc, server_field->table_name,
                                   strlen(server_field->table_name),
                                   cs, thd_cs);
  client_field->name= dup_str_aux(field_alloc, server_field->col_name,
                                  strlen(server_field->col_name), cs, thd_cs);
  client_field->org_table= dup_str_aux(field_alloc,
                                       server_field->org_table_name,
                                       strlen(server_field->org_table_name),
                                       cs, thd_cs);
  client_field->org_name= dup_str_aux(field_alloc, server_field->org_col_name,
                                      strlen(server_field->org_col_name), cs,
                                      thd_cs);
  if (item_charset == &my_charset_bin || thd_cs == NULL)
  {
    /* No conversion */
    client_field->charsetnr= item_charset->number;
    client_field->length= server_field->length;
  }
  else
  {
    uint max_char_len;
    /* With conversion */
    client_field->charsetnr= thd_cs->number;
    max_char_len= (server_field->type >= (int) MYSQL_TYPE_TINY_BLOB &&
                   server_field->type <= (int) MYSQL_TYPE_BLOB) ?
                   server_field->length / item_charset->mbminlen :
                   server_field->length / item_charset->mbmaxlen;
    client_field->length= char_to_byte_length_safe(max_char_len,
                                                   thd_cs->mbmaxlen);
  }
  client_field->type= server_field->type;
  client_field->flags= server_field->flags;
  client_field->decimals= server_field->decimals;
  client_field->db_length=strlen(client_field->db);
  client_field->table_length=strlen(client_field->table);
  client_field->name_length=strlen(client_field->name);
  client_field->org_name_length=strlen(client_field->org_name);
  client_field->org_table_length=strlen(client_field->org_table);

  client_field->catalog= dup_str_aux(field_alloc, "def", 3, cs, thd_cs);
  client_field->catalog_length= 3;

  if (IS_NUM(client_field->type))
    client_field->flags|= NUM_FLAG;

  ++client_field;
  DBUG_RETURN(0);
}


void Protocol_classic::send_string_metadata(String* item_str)
{
  if (!item_str)
  {
    client_field->def_length= 0;
    client_field->def= strmake_root(field_alloc, "", 0);
  }
  else
  {
    client_field->def_length= item_str->length();
    client_field->def= strmake_root(field_alloc, item_str->ptr(),
                                    client_field->def_length);
  }
}


bool Protocol_classic::end_row()
{
  DBUG_ENTER("Protocol_classic::end_row");
  if (!m_thd->mysql)            // bootstrap file handling
    DBUG_RETURN(FALSE);

  *next_field= 0;
  DBUG_RETURN(FALSE);
}


bool Protocol_classic::start_result_metadata(uint num_cols, uint flags,
                                             const CHARSET_INFO *cs)
{
  MYSQL_DATA               *data;
  DBUG_ENTER("start_result_metadata");
  result_cs= (CHARSET_INFO *)cs;

  if (!m_thd->mysql)            // bootstrap file handling
    DBUG_RETURN(false);

  if (begin_dataset())
    DBUG_RETURN(true);

  send_metadata= true;
  sending_flags= flags;
  data= m_thd->cur_data;
  data->fields= field_count= num_cols;
  field_alloc= &data->alloc;

  client_field=
    (MYSQL_FIELD *) alloc_root(field_alloc, sizeof(MYSQL_FIELD) * field_count);
  data->embedded_info->fields_list= client_field;

  DBUG_RETURN(client_field == NULL);
}


bool Protocol_classic::end_result_metadata()
{
  send_metadata= false;
  if (sending_flags & SEND_EOF)
    return write_eof_packet(m_thd, m_thd->server_status,
      m_thd->get_stmt_da()->current_statement_cond_count());
  return false;
}


bool Protocol_binary::end_row()
{
  MYSQL_ROWS *cur;
  MYSQL_DATA *data= m_thd->cur_data;

  data->rows++;
  if (!(cur= (MYSQL_ROWS *)alloc_root(alloc,
                                      sizeof(MYSQL_ROWS)+packet->length())))
  {
    my_error(ER_OUT_OF_RESOURCES,MYF(0));
    return true;
  }
  cur->data= (MYSQL_ROW)(((char *)cur) + sizeof(MYSQL_ROWS));
  memcpy(cur->data, packet->ptr()+1, packet->length()-1);
  cur->length= packet->length();       /* To allow us to do sanity checks */

  *data->embedded_info->prev_ptr= cur;
  data->embedded_info->prev_ptr= &cur->next;
  cur->next= 0;

  return false;
}


/**
  Embedded library implementation of OK response.

  This function is used by the server to write 'OK' packet to
  the "network" when the server is compiled as an embedded library.
  Since there is no network in the embedded configuration,
  a different implementation is necessary.
  Instead of marshalling response parameters to a network representation
  and then writing it to the socket, here we simply copy the data to the
  corresponding client-side connection structures. 

  @sa Server implementation of net_send_ok in protocol_classic.cc for
  description of the arguments.

  @return
    @retval TRUE An error occurred
    @retval FALSE Success
*/

bool
net_send_ok(THD *thd,
            uint server_status, uint statement_warn_count,
            ulonglong affected_rows, ulonglong id, const char *message,
            bool eof_identifier MY_ATTRIBUTE((unused)))
{
  DBUG_ENTER("emb_net_send_ok");
  MYSQL_DATA *data;
  MYSQL *mysql= thd->mysql;

  if (!mysql)            // bootstrap file handling
    DBUG_RETURN(FALSE);
  if (!(data= thd->alloc_new_dataset()))
    DBUG_RETURN(TRUE);
  data->embedded_info->affected_rows= affected_rows;
  data->embedded_info->insert_id= id;
  if (message)
    strmake(data->embedded_info->info, message,
            sizeof(data->embedded_info->info)-1);

  bool error= write_eof_packet(thd, server_status, statement_warn_count);
  thd->cur_data= 0;
  DBUG_RETURN(error);
}


/**
  Embedded library implementation of EOF response.

  @sa net_send_ok

  @return
    @retval TRUE  An error occurred
    @retval FALSE Success
*/

bool
net_send_eof(THD *thd, uint server_status, uint statement_warn_count)
{
  bool error= write_eof_packet(thd, server_status, statement_warn_count);
  thd->cur_data= 0;
  return error;
}


bool net_send_error_packet(THD *thd, uint sql_errno, const char *err,
                           const char *sqlstate)
{
  uint error;
  char converted_err[MYSQL_ERRMSG_SIZE];
  MYSQL_DATA *data= thd->cur_data;
  struct embedded_query_result *ei;

  if (!thd->mysql)            // bootstrap file handling
  {
    my_message_local(ERROR_LEVEL, "%d  %s", sql_errno, err);
    return TRUE;
  }

  if (!data)
    data= thd->alloc_new_dataset();

  ei= data->embedded_info;
  ei->last_errno= sql_errno;
  convert_error_message(converted_err, sizeof(converted_err),
                        thd->variables.character_set_results,
                        err, strlen(err),
                        system_charset_info, &error);
  /* Converted error message is always null-terminated. */
  strmake(ei->info, converted_err, sizeof(ei->info)-1);
  my_stpcpy(ei->sqlstate, sqlstate);
  ei->server_status= thd->server_status;
  thd->cur_data= 0;
  return FALSE;
}


void Protocol_binary::start_row()
{
  MYSQL_DATA *data= m_thd->cur_data;
  next_mysql_field= data->embedded_info->fields_list;
  packet->length(bit_fields + 1);
  memset(const_cast<char*>(packet->ptr()), 0, 1 + bit_fields);
  field_pos= 0;
}


void Protocol_text::start_row()
{
  MYSQL_ROWS *cur;
  MYSQL_DATA *data= m_thd->cur_data;
  DBUG_ENTER("Protocol_text::start_row");

  if (!m_thd->mysql)            // bootstrap file handling
    DBUG_VOID_RETURN;

  data->rows++;
  if (!(cur= (MYSQL_ROWS *)alloc_root(alloc, sizeof(MYSQL_ROWS)+(field_count + 1) * sizeof(char *))))
  {
    my_error(ER_OUT_OF_RESOURCES,MYF(0));
    DBUG_VOID_RETURN;
  }
  cur->data= (MYSQL_ROW)(((char *)cur) + sizeof(MYSQL_ROWS));

  *data->embedded_info->prev_ptr= cur;
  data->embedded_info->prev_ptr= &cur->next;
  next_field=cur->data;
  next_mysql_field= data->embedded_info->fields_list;
#ifndef DBUG_OFF
  field_pos= 0;
#endif

  DBUG_VOID_RETURN;
}


bool Protocol_text::store_null()
{
  if (!m_thd->mysql)            // bootstrap file handling
    return false;

  *(next_field++)= NULL;
  ++next_mysql_field;
  return false;
}


bool Protocol_binary::net_store_data(const uchar *from, size_t length)
{
  if (!m_thd->mysql)            // bootstrap file handling
    return 0;

  size_t packet_length= packet->length();
  /*
     The +9 comes from that strings of length longer than 16M require
     9 bytes to be stored (see net_store_length).
  */
  if (packet_length + 9 + length > packet->alloced_length() &&
      packet->mem_realloc(packet_length + 9 + length))
    return 1;
  uchar *to= net_store_length((uchar*)packet->ptr() + packet_length, length);
  memcpy(to, from, length);
  packet->length((uint)(to + length - (uchar*)packet->ptr()));
  if (next_mysql_field->max_length < length)
    next_mysql_field->max_length= length;
  ++next_mysql_field;
  return 0;
}

bool Protocol_binary::net_store_data(const uchar *from, size_t length,
                                     const CHARSET_INFO *from_cs,
                                     const CHARSET_INFO *to_cs)
{
  uint dummy_errors;
  /* Calculate maxumum possible result length */
  size_t conv_length= to_cs->mbmaxlen * length / from_cs->mbminlen;

  if (!m_thd->mysql)            // bootstrap file handling
    return 0;

  if (conv_length > 250)
  {
    /*
      For strings with conv_length greater than 250 bytes
      we don't know how many bytes we will need to store length: one or two,
      because we don't know result length until conversion is done.
      For example, when converting from utf8 (mbmaxlen=3) to latin1,
      conv_length=300 means that the result length can vary between 100 to 300.
      length=100 needs one byte, length=300 needs to bytes.

      Thus conversion directly to "packet" is not worthy.
      Let's use "convert" as a temporary buffer.
    */
    return (convert->copy((const char*)from, length, from_cs,
                          to_cs, &dummy_errors) ||
            net_store_data((const uchar*)convert->ptr(), convert->length()));
  }
  size_t packet_length= packet->length();
  size_t new_length= packet_length + conv_length + 1;

  if (new_length > packet->alloced_length() && packet->mem_realloc(new_length))
    return 1;

  char *length_pos= (char*) packet->ptr() + packet_length;
  char *to= length_pos + 1;

  to+= length= copy_and_convert(to, conv_length, to_cs,
                                (const char*)from, length, from_cs,
                                &dummy_errors);

  net_store_length((uchar*)length_pos, to - length_pos - 1);
  packet->length((uint)(to - packet->ptr()));
  if (next_mysql_field->max_length < length)
    next_mysql_field->max_length= length;
  ++next_mysql_field;
  return 0;
}


bool Protocol_classic::net_store_data(const uchar *from, size_t length)
{
  char *field_buf;
  if (!m_thd->mysql)            // bootstrap file handling
    return FALSE;

  if (!(field_buf= (char*) alloc_root(alloc, length + sizeof(uint) + 1)))
    return TRUE;
  *(uint *)field_buf= length;
  *next_field= field_buf + sizeof(uint);
  memcpy((uchar*) *next_field, from, length);
  (*next_field)[length]= 0;
  if (next_mysql_field->max_length < length)
    next_mysql_field->max_length=length;
  ++next_field;
  ++next_mysql_field;
  return FALSE;
}


void error_log_print(enum loglevel level MY_ATTRIBUTE((unused)),
                     const char *format, va_list argsi)
{
  my_vsnprintf(mysql_server_last_error, sizeof(mysql_server_last_error),
               format, argsi);
  mysql_server_last_errno= CR_UNKNOWN_ERROR;
}


bool Protocol_classic::net_store_data(const uchar *from, size_t length,
                                      const CHARSET_INFO *from_cs,
                                      const CHARSET_INFO *to_cs)
{
  size_t conv_length= to_cs->mbmaxlen * length / from_cs->mbminlen;
  uint dummy_error;
  char *field_buf;
  if (!m_thd->mysql)            // bootstrap file handling
    return false;

  if (!(field_buf= (char*) alloc_root(alloc, conv_length + sizeof(uint) + 1)))
    return true;
  *next_field= field_buf + sizeof(uint);
  length= copy_and_convert(*next_field, conv_length, to_cs,
                           (const char*) from, length, from_cs, &dummy_error);
  *(uint *) field_buf= length;
  (*next_field)[length]= 0;
  if (next_mysql_field->max_length < length)
    next_mysql_field->max_length= length;
  ++next_field;
  ++next_mysql_field;
  return false;
}

bool Protocol_classic::connection_alive()
{
  //for embedded protocol connection is always on
  return true;
}
