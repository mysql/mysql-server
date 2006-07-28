/*
 * Copyright (c)  2000
 * SWsoft  company
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

#define main main1
#define mysql_unix_port mysql_inix_port1
#define mysql_port mysql_port1

extern "C"
{
  extern unsigned long max_allowed_packet, net_buffer_length;
}

#if defined(__WIN__) && !defined(USING_CMAKE)
#include "../sql/mysqld.cpp"
#else
#include "../sql/mysqld.cc"
#endif

int check_user(THD *thd, enum enum_server_command command, 
	       const char *passwd, uint passwd_len, const char *db,
	       bool check_count);
void thd_init_client_charset(THD *thd, uint cs_number);

C_MODE_START

#include <mysql.h>
#undef ER
#include "errmsg.h"
#include <sql_common.h>
#include "embedded_priv.h"

static my_bool emb_read_query_result(MYSQL *mysql);


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
  strmake(net->last_error, ei->info, sizeof(net->last_error));
  memcpy(net->sqlstate, ei->sqlstate, sizeof(net->sqlstate));
  my_free((gptr) data, MYF(0));
}

static my_bool
emb_advanced_command(MYSQL *mysql, enum enum_server_command command,
		     const char *header, ulong header_length,
		     const char *arg, ulong arg_length, my_bool skip_check,
                     MYSQL_STMT *stmt)
{
  my_bool result= 1;
  THD *thd=(THD *) mysql->thd;
  NET *net= &mysql->net;

  thd->clear_data_list();
  /* Check that we are calling the client functions in right order */
  if (mysql->status != MYSQL_STATUS_READY)
  {
    strmov(net->last_error,
	   ER(net->last_errno=CR_COMMANDS_OUT_OF_SYNC));
    return 1;
  }

  /* Clear result variables */
  thd->clear_error();
  mysql->affected_rows= ~(my_ulonglong) 0;
  mysql->field_count= 0;
  net->last_errno= 0;
  mysql->current_stmt= stmt;

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

  thd->net.no_send_error= 0;
  result= dispatch_command(command, thd, (char *) arg, arg_length + 1);
  thd->cur_data= 0;

  if (!skip_check)
    result= thd->net.last_errno ? -1 : 0;

  return result;
}

static void emb_flush_use_result(MYSQL *mysql)
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
emb_read_rows(MYSQL *mysql, MYSQL_FIELD *mysql_fields __attribute__((unused)),
	      unsigned int fields __attribute__((unused)))
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
  my_free((gptr) res,MYF(0));
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
    my_free((gptr) res,MYF(0));
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
  mysql->fields= res->embedded_info->fields_list;
  mysql->affected_rows= res->embedded_info->affected_rows;
  mysql->insert_id= res->embedded_info->insert_id;
  mysql->net.last_errno= 0;
  mysql->net.last_error[0]= 0;
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
    my_free((gptr) res, MYF(0));

  return 0;
}

static int emb_stmt_execute(MYSQL_STMT *stmt)
{
  DBUG_ENTER("emb_stmt_execute");
  char header[5];
  THD *thd;

  int4store(header, stmt->stmt_id);
  header[4]= stmt->flags;
  thd= (THD*)stmt->mysql->thd;
  thd->client_param_count= stmt->param_count;
  thd->client_params= stmt->params;
  if (emb_advanced_command(stmt->mysql, COM_STMT_EXECUTE,0,0,
                           header, sizeof(header), 1, stmt) ||
      emb_read_query_result(stmt->mysql))
  {
    NET *net= &stmt->mysql->net;
    set_stmt_errmsg(stmt, net->last_error, net->last_errno, net->sqlstate);
    DBUG_RETURN(1);
  }
  stmt->affected_rows= stmt->mysql->affected_rows;
  stmt->insert_id= stmt->mysql->insert_id;
  stmt->server_status= stmt->mysql->server_status;

  DBUG_RETURN(0);
}

int emb_read_binary_rows(MYSQL_STMT *stmt)
{
  MYSQL_DATA *data;
  if (!(data= emb_read_rows(stmt->mysql, 0, 0)))
    return 1;
  stmt->result= *data;
  my_free((char *) data, MYF(0));
  set_stmt_errmsg(stmt, stmt->mysql->net.last_error,
                  stmt->mysql->net.last_errno, stmt->mysql->net.sqlstate);
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
    set_stmt_errmsg(stmt, mysql->net.last_error,
                    mysql->net.last_errno, mysql->net.sqlstate);
    return 1;
  }

  thd->cur_data= res;
  mysql->warning_count= res->embedded_info->warning_count;
  mysql->server_status= res->embedded_info->server_status;
  mysql->net.last_errno= 0;
  mysql->net.last_error[0]= 0;

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
  thread_count--;
  thd->store_globals();
  delete thd;
  mysql->thd=0;
}

static const char * emb_read_statistics(MYSQL *mysql)
{
  THD *thd= (THD*)mysql->thd;
  return thd->net.last_error;
}


static MYSQL_RES * emb_store_result(MYSQL *mysql)
{
  return mysql_store_result(mysql);
}

int emb_read_change_user_result(MYSQL *mysql, 
				char *buff __attribute__((unused)),
				const char *passwd __attribute__((unused)))
{
  return mysql_errno(mysql);
}

MYSQL_METHODS embedded_methods= 
{
  emb_read_query_result,
  emb_advanced_command,
  emb_read_rows,
  emb_store_result,
  emb_fetch_lengths, 
  emb_flush_use_result,
  emb_list_fields,
  emb_read_prepare_result,
  emb_stmt_execute,
  emb_read_binary_rows,
  emb_unbuffered_fetch,
  emb_free_embedded_thd,
  emb_read_statistics,
  emb_read_query_result,
  emb_read_change_user_result,
  emb_read_rows_from_cursor
};

/*
  Make a copy of array and the strings array points to
*/

char **copy_arguments(int argc, char **argv)
{
  uint length= 0;
  char **from, **res, **end= argv+argc;

  for (from=argv ; from != end ; from++)
    length+= strlen(*from);

  if ((res= (char**) my_malloc(sizeof(argv)*(argc+1)+length+argc,
			       MYF(MY_WME))))
  {
    char **to= res, *to_str= (char*) (res+argc+1);
    for (from=argv ; from != end ;)
    {
      *to++= to_str;
      to_str= strmov(to_str, *from++)+1;
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
  int *argcp;
  char ***argvp;
  int fake_argc = 1;
  char *fake_argv[] = { (char *)"", 0 };
  const char *fake_groups[] = { "server", "embedded", 0 };
  my_bool acl_error;
  if (argc)
  {
    argcp= &argc;
    argvp= (char***) &argv;
  }
  else
  {
    argcp= &fake_argc;
    argvp= (char ***) &fake_argv;
  }
  if (!groups)
    groups= (char**) fake_groups;

  my_progname= (char *)"mysql_embedded";

  /*
    Perform basic logger initialization logger. Should be called after
    MY_INIT, as it initializes mutexes. Log tables are inited later.
  */
  logger.init_base();

  if (init_common_variables("my", *argcp, *argvp, (const char **)groups))
  {
    mysql_server_end();
    return 1;
  }
    
  /* Get default temporary directory */
  opt_mysql_tmpdir=getenv("TMPDIR");	/* Use this if possible */
#if defined( __WIN__) || defined(OS2)
  if (!opt_mysql_tmpdir)
    opt_mysql_tmpdir=getenv("TEMP");
  if (!opt_mysql_tmpdir)
    opt_mysql_tmpdir=getenv("TMP");
#endif
  if (!opt_mysql_tmpdir || !opt_mysql_tmpdir[0])
    opt_mysql_tmpdir=(char*) P_tmpdir;		/* purecov: inspected */

  umask(((~my_umask) & 0666));
  if (init_server_components())
  {
    mysql_server_end();
    return 1;
  }

  error_handler_hook = my_message_sql;

  acl_error= 0;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (!(acl_error= acl_init(opt_noacl)) &&
      !opt_noacl)
    (void) grant_init();
#endif
  if (acl_error || my_tz_init((THD *)0, default_tz_name, opt_bootstrap))
  {
    mysql_server_end();
    return 1;
  }

  init_max_user_conn();
  init_update_queries();

#ifdef HAVE_DLOPEN
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (!opt_noacl)
#endif
    udf_init();
#endif

  (void) thr_setconcurrency(concurrency);	// 10 by default

  if (
#ifdef HAVE_BERKELEY_DB
      (have_berkeley_db == SHOW_OPTION_YES) ||
#endif
      (flush_time && flush_time != ~(ulong) 0L))
  {
    pthread_t hThread;
    if (pthread_create(&hThread,&connection_attrib,handle_manager,0))
      sql_print_error("Warning: Can't create thread to manage maintenance");
  }

  if (opt_init_file)
  {
    if (read_init_file(opt_init_file))
    {
      mysql_server_end();
      return 1;
    }
  }

  return 0;
}

void end_embedded_server()
{
  my_free((char*) copy_arguments_ptr, MYF(MY_ALLOW_ZERO_PTR));
  copy_arguments_ptr=0;
  clean_up(0);
}


void init_embedded_mysql(MYSQL *mysql, int client_flag)
{
  THD *thd = (THD *)mysql->thd;
  thd->mysql= mysql;
  mysql->server_version= server_version;
  init_alloc_root(&mysql->field_alloc, 8192, 0);
}

void *create_embedded_thd(int client_flag)
{
  THD * thd= new THD;
  thd->thread_id= thread_id++;

  thd->thread_stack= (char*) &thd;
  if (thd->store_globals())
  {
    fprintf(stderr,"store_globals failed.\n");
    goto err;
  }

  thd->mysys_var= my_thread_var;
  thd->dbug_thread_id= my_thread_id();

/* TODO - add init_connect command execution */

  if (thd->variables.max_join_size == HA_POS_ERROR)
    thd->options |= OPTION_BIG_SELECTS;
  thd->proc_info=0;				// Remove 'login'
  thd->command=COM_SLEEP;
  thd->version=refresh_version;
  thd->set_time();
  thd->init_for_queries();
  thd->client_capabilities= client_flag;

  thd->db= NULL;
  thd->db_length= 0;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  thd->security_ctx->db_access= DB_ACLS;
  thd->security_ctx->master_access= ~NO_ACCESS;
#endif
  thd->cur_data= 0;
  thd->first_data= 0;
  thd->data_tail= &thd->first_data;
  bzero((char*) &thd->net, sizeof(thd->net));

  thread_count++;
  return thd;
err:
  delete(thd);
  return NULL;
}


#ifdef NO_EMBEDDED_ACCESS_CHECKS
int check_embedded_connection(MYSQL *mysql, const char *db)
{
  int result;
  THD *thd= (THD*)mysql->thd;
  thd_init_client_charset(thd, mysql->charset->number);
  thd->update_charset();
  Security_context *sctx= thd->security_ctx;
  sctx->host_or_ip= sctx->host= (char*) my_localhost;
  strmake(sctx->priv_host, (char*) my_localhost,  MAX_HOSTNAME-1);
  sctx->priv_user= sctx->user= my_strdup(mysql->user, MYF(0));
  result= check_user(thd, COM_CONNECT, NULL, 0, db, true);
  emb_read_query_result(mysql);
  return result;
}

#else
int check_embedded_connection(MYSQL *mysql, const char *db)
{
  THD *thd= (THD*)mysql->thd;
  Security_context *sctx= thd->security_ctx;
  int result;
  char scramble_buff[SCRAMBLE_LENGTH];
  int passwd_len;

  thd_init_client_charset(thd, mysql->charset->number);
  thd->update_charset();
  if (mysql->options.client_ip)
  {
    sctx->host= my_strdup(mysql->options.client_ip, MYF(0));
    sctx->ip= my_strdup(sctx->host, MYF(0));
  }
  else
    sctx->host= (char*)my_localhost;
  sctx->host_or_ip= sctx->host;

  if (acl_check_host(sctx->host, sctx->ip))
  {
    result= ER_HOST_NOT_PRIVILEGED;
    goto err;
  }

  sctx->user= my_strdup(mysql->user, MYF(0));
  if (mysql->passwd && mysql->passwd[0])
  {
    memset(thd->scramble, 55, SCRAMBLE_LENGTH); // dummy scramble
    thd->scramble[SCRAMBLE_LENGTH]= 0;
    scramble(scramble_buff, thd->scramble, mysql->passwd);
    passwd_len= SCRAMBLE_LENGTH;
  }
  else
    passwd_len= 0;

  if((result= check_user(thd, COM_CONNECT, 
			 scramble_buff, passwd_len, db, true)))
     goto err;

  return 0;
err:
  {
    NET *net= &mysql->net;
    memcpy(net->last_error, thd->net.last_error, sizeof(net->last_error));
    memcpy(net->sqlstate, thd->net.sqlstate, sizeof(net->sqlstate));
  }
  return result;
}
#endif

C_MODE_END

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

void THD::clear_error()
{
  net.last_error[0]= 0;
  net.last_errno= 0;
  net.report_error= 0;
}

static char *dup_str_aux(MEM_ROOT *root, const char *from, uint length,
			 CHARSET_INFO *fromcs, CHARSET_INFO *tocs)
{
  uint32 dummy32;
  uint dummy_err;
  char *result;

  /* 'tocs' is set 0 when client issues SET character_set_results=NULL */
  if (tocs && String::needs_conversion(0, fromcs, tocs, &dummy32))
  {
    uint new_len= (tocs->mbmaxlen * length) / fromcs->mbminlen + 1;
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
  if (!my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
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


/*
  stores server_status and warning_count in the current
  query result structures

  SYNOPSIS
  write_eof_packet()
  thd		current thread

  NOTES
    should be called to after we get the recordset-result

*/

static void write_eof_packet(THD *thd)
{
  /*
    The following test should never be true, but it's better to do it
    because if 'is_fatal_error' is set the server is not going to execute
    other queries (see the if test in dispatch_command / COM_QUERY)
  */
  if (thd->is_fatal_error)
    thd->server_status&= ~SERVER_MORE_RESULTS_EXISTS;
  thd->cur_data->embedded_info->server_status= thd->server_status;
  /*
    Don't send warn count during SP execution, as the warn_list
    is cleared between substatements, and mysqltest gets confused
  */
  thd->cur_data->embedded_info->warning_count=
    (thd->spcont ? 0 : min(thd->total_warn_count, 65535));
}


/*
  allocs new query result and initialises Protocol::alloc

  SYNOPSIS
  Protocol::begin_dataset()

  RETURN
    0 if success
    1 if memory allocation failed
*/

int Protocol::begin_dataset()
{
  MYSQL_DATA *data= thd->alloc_new_dataset();
  if (!data)
    return 1;
  alloc= &data->alloc;
  init_alloc_root(alloc,8192,0);	/* Assume rowlength < 8192 */
  alloc->min_malloc=sizeof(MYSQL_ROWS);
  return 0;
}


/*
  remove last row of current recordset

  SYNOPSIS
  Protocol_simple::remove_last_row()

  NOTES
    does the loop from the beginning of the current recordset to
    the last record and cuts it off.
    Not supposed to be frequently called.
*/

void Protocol_simple::remove_last_row()
{
  MYSQL_DATA *data= thd->cur_data;
  MYSQL_ROWS **last_row_hook= &data->data;
  uint count= data->rows;
  DBUG_ENTER("Protocol_simple::remove_last_row");
  while (--count)
    last_row_hook= &(*last_row_hook)->next;

  *last_row_hook= 0;
  data->embedded_info->prev_ptr= last_row_hook;
  data->rows--;

  DBUG_VOID_RETURN;
}


bool Protocol::send_fields(List<Item> *list, uint flags)
{
  List_iterator_fast<Item> it(*list);
  Item                     *item;
  MYSQL_FIELD              *client_field;
  MEM_ROOT                 *field_alloc;
  CHARSET_INFO             *thd_cs= thd->variables.character_set_results;
  CHARSET_INFO             *cs= system_charset_info;
  MYSQL_DATA               *data;
  DBUG_ENTER("send_fields");

  if (!thd->mysql)            // bootstrap file handling
    DBUG_RETURN(0);

  if (begin_dataset())
    goto err;

  data= thd->cur_data;
  data->fields= field_count= list->elements;
  field_alloc= &data->alloc;

  if (!(client_field= data->embedded_info->fields_list= 
	(MYSQL_FIELD*)alloc_root(field_alloc, sizeof(MYSQL_FIELD)*field_count)))
    goto err;

  while ((item= it++))
  {
    Send_field server_field;
    item->make_field(&server_field);

    /* Keep things compatible for old clients */
    if (server_field.type == MYSQL_TYPE_VARCHAR)
      server_field.type= MYSQL_TYPE_VAR_STRING;

    client_field->db= dup_str_aux(field_alloc, server_field.db_name,
                                  strlen(server_field.db_name), cs, thd_cs);
    client_field->table= dup_str_aux(field_alloc, server_field.table_name,
                                     strlen(server_field.table_name), cs, thd_cs);
    client_field->name= dup_str_aux(field_alloc, server_field.col_name,
                                    strlen(server_field.col_name), cs, thd_cs);
    client_field->org_table= dup_str_aux(field_alloc, server_field.org_table_name,
                                         strlen(server_field.org_table_name), cs, thd_cs);
    client_field->org_name= dup_str_aux(field_alloc, server_field.org_col_name,
                                        strlen(server_field.org_col_name), cs, thd_cs);
    if (item->collation.collation == &my_charset_bin || thd_cs == NULL)
    {
      /* No conversion */
      client_field->charsetnr= server_field.charsetnr;
      client_field->length= server_field.length;
    }
    else
    {
      /* With conversion */
      client_field->charsetnr= thd_cs->number;
      uint char_len= server_field.length / item->collation.collation->mbmaxlen;
      client_field->length= char_len * thd_cs->mbmaxlen;
    }
    client_field->type=   server_field.type;
    client_field->flags= server_field.flags;
    client_field->decimals= server_field.decimals;
    client_field->db_length=		strlen(client_field->db);
    client_field->table_length=		strlen(client_field->table);
    client_field->name_length=		strlen(client_field->name);
    client_field->org_name_length=	strlen(client_field->org_name);
    client_field->org_table_length=	strlen(client_field->org_table);

    client_field->catalog= dup_str_aux(field_alloc, "def", 3, cs, thd_cs);
    client_field->catalog_length= 3;

    if (INTERNAL_NUM_FIELD(client_field))
      client_field->flags|= NUM_FLAG;

    if (flags & (int) Protocol::SEND_DEFAULTS)
    {
      char buff[80];
      String tmp(buff, sizeof(buff), default_charset_info), *res;

      if (!(res=item->val_str(&tmp)))
      {
	client_field->def_length= 0;
	client_field->def= strmake_root(field_alloc, "",0);
      }
      else
      {
	client_field->def_length= res->length();
	client_field->def= strmake_root(field_alloc, res->ptr(),
					client_field->def_length);
      }
    }
    else
      client_field->def=0;
    client_field->max_length= 0;
    ++client_field;
  }

  if (flags & SEND_EOF)
    write_eof_packet(thd);

  DBUG_RETURN(prepare_for_send(list));
 err:
  my_error(ER_OUT_OF_RESOURCES, MYF(0));        /* purecov: inspected */
  DBUG_RETURN(1);				/* purecov: inspected */
}

bool Protocol::write()
{
  if (!thd->mysql)            // bootstrap file handling
    return false;

  *next_field= 0;
  return false;
}

bool Protocol_prep::write()
{
  MYSQL_ROWS *cur;
  MYSQL_DATA *data= thd->cur_data;

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

void
send_ok(THD *thd,ha_rows affected_rows,ulonglong id,const char *message)
{
  DBUG_ENTER("send_ok");
  MYSQL_DATA *data;
  MYSQL *mysql= thd->mysql;
  
  if (!mysql)            // bootstrap file handling
    DBUG_VOID_RETURN;
  if (thd->net.no_send_ok)	// hack for re-parsing queries
    DBUG_VOID_RETURN;
  if (!(data= thd->alloc_new_dataset()))
    return;
  data->embedded_info->affected_rows= affected_rows;
  data->embedded_info->insert_id= id;
  if (message)
    strmake(data->embedded_info->info, message,
            sizeof(data->embedded_info->info)-1);

  write_eof_packet(thd);
  thd->cur_data= 0;
  DBUG_VOID_RETURN;
}

void
send_eof(THD *thd)
{
  write_eof_packet(thd);
  thd->cur_data= 0;
}


void net_send_error_packet(THD *thd, uint sql_errno, const char *err)
{
  MYSQL_DATA *data= thd->cur_data ? thd->cur_data : thd->alloc_new_dataset();
  struct embedded_query_result *ei= data->embedded_info;

  ei->last_errno= sql_errno;
  strmake(ei->info, err, sizeof(ei->info)-1);
  strmov(ei->sqlstate, mysql_errno_to_sqlstate(sql_errno));
  thd->cur_data= 0;
}


void Protocol_simple::prepare_for_resend()
{
  MYSQL_ROWS *cur;
  MYSQL_DATA *data= thd->cur_data;
  DBUG_ENTER("send_data");

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
  DBUG_VOID_RETURN;
}

bool Protocol_simple::store_null()
{
  *(next_field++)= NULL;
  ++next_mysql_field;
  return false;
}

bool Protocol::net_store_data(const char *from, uint length)
{
  char *field_buf;
  if (!thd->mysql)            // bootstrap file handling
    return false;

  if (!(field_buf=alloc_root(alloc, length + sizeof(uint) + 1)))
    return true;
  *(uint *)field_buf= length;
  *next_field= field_buf + sizeof(uint);
  memcpy(*next_field, from, length);
  (*next_field)[length]= 0;
  if (next_mysql_field->max_length < length)
    next_mysql_field->max_length=length;
  ++next_field;
  ++next_mysql_field;
  return false;
}

