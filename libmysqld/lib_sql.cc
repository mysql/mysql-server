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

static int fake_argc= 1;
static char *fake_argv[]= {(char *)"", 0};
static const char *fake_groups[] = { "server", "embedded", 0 };

#if defined (__WIN__)
#include "../sql/mysqld.cpp"
#else
#include "../sql/mysqld.cc"
#endif

int check_user(THD *thd, enum enum_server_command command, 
	       const char *passwd, uint passwd_len, const char *db,
	       bool check_count);
C_MODE_START
#include <mysql.h>
#undef ER
#include "errmsg.h"
#include <sql_common.h>

static my_bool  org_my_init_done;
my_bool         server_inited;

static my_bool STDCALL
emb_advanced_command(MYSQL *mysql, enum enum_server_command command,
		     const char *header, ulong header_length,
		     const char *arg, ulong arg_length, my_bool skip_check)
{
  my_bool result= 1;
  THD *thd=(THD *) mysql->thd;
  NET *net= &mysql->net;

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

  result= dispatch_command(command, thd, (char *) arg, arg_length + 1);

  if (!skip_check)
    result= thd->net.last_errno ? -1 : 0;

  if ((net->last_errno= thd->net.last_errno))
  {
    memcpy(net->last_error, thd->net.last_error, sizeof(net->last_error));
    memcpy(net->sqlstate, thd->net.sqlstate, sizeof(net->sqlstate));
  }
  else
  {
    net->last_error[0]= 0;
    strmov(net->sqlstate, not_error_sqlstate);
  }
  mysql->warning_count= ((THD*)mysql->thd)->total_warn_count;
  return result;
}

static MYSQL_DATA * STDCALL 
emb_read_rows(MYSQL *mysql, MYSQL_FIELD *mysql_fields __attribute__((unused)),
	      unsigned int fields __attribute__((unused)))
{
  MYSQL_DATA *result= ((THD*)mysql->thd)->data;
  if (!result)
  {
    if (!(result=(MYSQL_DATA*) my_malloc(sizeof(MYSQL_DATA),
					 MYF(MY_WME | MY_ZEROFILL))))
    {
      NET *net = &mysql->net;
      net->last_errno=CR_OUT_OF_MEMORY;
      strmov(net->sqlstate, unknown_sqlstate);
      strmov(net->last_error,ER(net->last_errno));
      return NULL;
    }    
    return result;
  }
  *result->prev_ptr= NULL;
  ((THD*)mysql->thd)->data= NULL;
  return result;
}

static MYSQL_FIELD * STDCALL emb_list_fields(MYSQL *mysql)
{
  return mysql->fields;
}

static my_bool STDCALL emb_read_prepare_result(MYSQL *mysql, MYSQL_STMT *stmt)
{
  THD *thd= (THD*)mysql->thd;
  if (mysql->net.last_errno)
    return 1;
  stmt->stmt_id= thd->client_stmt_id;
  stmt->param_count= thd->client_param_count;
  stmt->field_count= mysql->field_count;

  if (stmt->field_count != 0)
  {
    if (!(mysql->server_status & SERVER_STATUS_AUTOCOMMIT))
      mysql->server_status|= SERVER_STATUS_IN_TRANS;

    stmt->fields= mysql->fields;
    stmt->mem_root= mysql->field_alloc;
    mysql->fields= NULL;
  }

  return 0;
}

/**************************************************************************
  Get column lengths of the current row
  If one uses mysql_use_result, res->lengths contains the length information,
  else the lengths are calculated from the offset between pointers.
**************************************************************************/

static void STDCALL emb_fetch_lengths(ulong *to, MYSQL_ROW column, unsigned int field_count)
{ 
  MYSQL_ROW end;

  for (end=column + field_count; column != end ; column++,to++)
    *to= *column ? *(uint *)((*column) - sizeof(uint)) : 0;
}

static my_bool STDCALL emb_mysql_read_query_result(MYSQL *mysql)
{
  if (mysql->net.last_errno)
    return -1;

  if (mysql->field_count)
    mysql->status=MYSQL_STATUS_GET_RESULT;

  return 0;
}

static int STDCALL emb_stmt_execute(MYSQL_STMT *stmt)
{
  DBUG_ENTER("emb_stmt_execute");
  THD *thd= (THD*)stmt->mysql->thd;
  thd->client_param_count= stmt->param_count;
  thd->client_params= stmt->params;
  if (thd->data)
  {
    free_rows(thd->data);
    thd->data= 0;
  }
  if (emb_advanced_command(stmt->mysql, COM_EXECUTE,0,0,
			   (const char*)&stmt->stmt_id,sizeof(stmt->stmt_id),1)
      || emb_mysql_read_query_result(stmt->mysql))
  {
    NET *net= &stmt->mysql->net;
    set_stmt_errmsg(stmt, net->last_error, net->last_errno, net->sqlstate);
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

MYSQL_DATA *emb_read_binary_rows(MYSQL_STMT *stmt)
{
  return emb_read_rows(stmt->mysql, 0, 0);
}

int STDCALL emb_unbuffered_fetch(MYSQL *mysql, char **row)
{
  MYSQL_DATA *data= ((THD*)mysql->thd)->data;
  if (!data || !data->data)
  {
    *row= NULL;
    if (data)
    {
      free_rows(data);
      ((THD*)mysql->thd)->data= NULL;
    }
  }
  else
  {
    *row= (char *)data->data->data;
    data->data= data->data->next;
  }
  return 0;
}

static void STDCALL emb_free_embedded_thd(MYSQL *mysql)
{
  THD *thd= (THD*)mysql->thd;
  if (thd->data)
    free_rows(thd->data);
  thread_count--;
  delete thd;
}

static const char * STDCALL emb_read_statistic(MYSQL *mysql)
{
  THD *thd= (THD*)mysql->thd;
  return thd->net.last_error;
}

MYSQL_METHODS embedded_methods= 
{
  emb_mysql_read_query_result,
  emb_advanced_command,
  emb_read_rows,
  mysql_store_result,
  emb_fetch_lengths, 
  emb_list_fields,
  emb_read_prepare_result,
  emb_stmt_execute,
  emb_read_binary_rows,
  emb_unbuffered_fetch,
  emb_free_embedded_thd,
  emb_read_statistic
};

C_MODE_END

void THD::clear_error()
{
  net.last_error[0]= 0;
  net.last_errno= 0;
  net.report_error= 0;
}

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


extern "C"
{

char **		copy_arguments_ptr= 0; 

int STDCALL mysql_server_init(int argc, char **argv, char **groups)
{
  char glob_hostname[FN_REFLEN];

  /* This mess is to allow people to call the init function without
   * having to mess with a fake argv */
  int *argcp;
  char ***argvp;
  int fake_argc = 1;
  char *fake_argv[] = { (char *)"", 0 };
  const char *fake_groups[] = { "server", "embedded", 0 };
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


  /* Only call MY_INIT() if it hasn't been called before */
  if (!server_inited)
  {
    server_inited=1;
    org_my_init_done=my_init_done;
  }
  if (!org_my_init_done)
  {
    MY_INIT((char *)"mysql_embedded");	// init my_sys library & pthreads
  }

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

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (acl_init((THD *)0, opt_noacl))
  {
    mysql_server_end();
    return 1;
  }
  if (!opt_noacl)
    (void) grant_init((THD *)0);

#endif

  init_max_user_conn();
  init_update_queries();

#ifdef HAVE_DLOPEN
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (!opt_noacl)
#endif
    udf_init();
#endif

  if (opt_bin_log)
  {
    if (!opt_bin_logname)
    {
      char tmp[FN_REFLEN];
      /* TODO: The following should be using fn_format();  We just need to
	 first change fn_format() to cut the file name if it's too long.
      */
      strmake(tmp,glob_hostname,FN_REFLEN-5);
      strmov(strcend(tmp,'.'),"-bin");
      opt_bin_logname=my_strdup(tmp,MYF(MY_WME));
    }
    open_log(&mysql_bin_log, glob_hostname, opt_bin_logname, "-bin",
	     opt_binlog_index_name, LOG_BIN, 0, 0, max_binlog_size);
    using_update_log=1;
  }

  (void) thr_setconcurrency(concurrency);	// 10 by default

  if (
#ifdef HAVE_BERKELEY_DB
      !berkeley_skip ||
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

  /*
    Update mysqld variables from client variables if set
    The client variables are set also by get_one_option() in mysqld.cc
  */
  if (max_allowed_packet)
    global_system_variables.max_allowed_packet= max_allowed_packet;
  if (net_buffer_length)
    global_system_variables.net_buffer_length= net_buffer_length;
  return 0;
}

void STDCALL mysql_server_end()
{
  my_free((char*) copy_arguments_ptr, MYF(MY_ALLOW_ZERO_PTR));
  copy_arguments_ptr=0;
  clean_up(0);
  /* If library called my_init(), free memory allocated by it */
  if (!org_my_init_done)
    my_end(0);
}

} /* extern "C" */

C_MODE_START
void init_embedded_mysql(MYSQL *mysql, int client_flag, char *db)
{
  THD *thd = (THD *)mysql->thd;
  thd->mysql= mysql;
  mysql->server_version= server_version;
}

void *create_embedded_thd(int client_flag, char *db)
{
  THD * thd= new THD;
  thd->thread_id= thread_id++;

  if (thd->store_globals())
  {
    fprintf(stderr,"store_globals failed.\n");
    return NULL;
  }

  thd->mysys_var= my_thread_var;
  thd->dbug_thread_id= my_thread_id();
  thd->thread_stack= (char*) &thd;

  thd->proc_info=0;				// Remove 'login'
  thd->command=COM_SLEEP;
  thd->version=refresh_version;
  thd->set_time();
  init_sql_alloc(&thd->mem_root,8192,8192);
  thd->client_capabilities= client_flag;

  thd->db= db;
  thd->db_length= db ? strip_sp(db) : 0;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  thd->db_access= DB_ACLS;
  thd->master_access= ~NO_ACCESS;
#endif
  thd->net.query_cache_query= 0;

  thd->data= 0;

  thread_count++;
  return thd;
}

#ifndef NO_EMBEDDED_ACCESS_CHECKS
int check_embedded_connection(MYSQL *mysql)
{
  THD *thd= (THD*)mysql->thd;
  int result;
  char scramble_buff[SCRAMBLE_LENGTH];
  int passwd_len;

  thd->host= mysql->options.client_ip ?
    mysql->options.client_ip : (char*)my_localhost;
  thd->ip= thd->host;
  thd->host_or_ip= thd->host;

  if (acl_check_host(thd->host,thd->ip))
  {
    result= ER_HOST_NOT_PRIVILEGED;
    goto err;
  }

  thd->user= mysql->user;
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
			 scramble_buff, passwd_len, thd->db, true)))
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

bool Protocol::send_fields(List<Item> *list, uint flag)
{
  List_iterator_fast<Item> it(*list);
  Item                     *item;
  MYSQL_FIELD              *client_field;
  MYSQL                    *mysql= thd->mysql;
  MEM_ROOT                 *field_alloc;
  
  DBUG_ENTER("send_fields");

  if (!mysql)            // bootstrap file handling
    DBUG_RETURN(0);

  field_count= list->elements;
  field_alloc= &mysql->field_alloc;
  if (!(client_field= thd->mysql->fields= 
	(MYSQL_FIELD *)alloc_root(field_alloc, 
				  sizeof(MYSQL_FIELD) * field_count)))
    goto err;

  while ((item= it++))
  {
    Send_field server_field;
    item->make_field(&server_field);

    client_field->db=	  strdup_root(field_alloc, server_field.db_name);
    client_field->table=  strdup_root(field_alloc, server_field.table_name);
    client_field->name=   strdup_root(field_alloc, server_field.col_name);
    client_field->org_table= strdup_root(field_alloc, server_field.org_table_name);
    client_field->org_name=  strdup_root(field_alloc, server_field.org_col_name);
    client_field->length= server_field.length;
    client_field->type=   server_field.type;
    client_field->flags= server_field.flags;
    client_field->decimals= server_field.decimals;
    client_field->db_length=		strlen(client_field->db);
    client_field->table_length=		strlen(client_field->table);
    client_field->name_length=		strlen(client_field->name);
    client_field->org_name_length=	strlen(client_field->org_name);
    client_field->org_table_length=	strlen(client_field->org_table);
    client_field->charsetnr=		server_field.charsetnr;
    
    if (INTERNAL_NUM_FIELD(client_field))
      client_field->flags|= NUM_FLAG;

    if (flag & 2)
    {
      char buff[80];
      String tmp(buff, sizeof(buff), default_charset_info), *res;

      if (!(res=item->val_str(&tmp)))
	client_field->def= strdup_root(field_alloc, "");
      else
	client_field->def= strdup_root(field_alloc, tmp.ptr());
    }
    else
      client_field->def=0;
    client_field->max_length= 0;
    ++client_field;
  }
  thd->mysql->field_count= field_count;

  DBUG_RETURN(prepare_for_send(list));
 err:
  send_error(thd, ER_OUT_OF_RESOURCES);	/* purecov: inspected */
  DBUG_RETURN(1);				/* purecov: inspected */
}

bool Protocol::send_records_num(List<Item> *list, ulonglong records)
{
  return false;
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
  MYSQL_DATA *data= thd->data;

  if (!data)
  {
    if (!(data= (MYSQL_DATA*) my_malloc(sizeof(MYSQL_DATA),
					MYF(MY_WME | MY_ZEROFILL))))
      return true;
    
    alloc= &data->alloc;
    init_alloc_root(alloc,8192,0);	/* Assume rowlength < 8192 */
    alloc->min_malloc=sizeof(MYSQL_ROWS);
    data->rows=0;
    data->fields=field_count;
    data->prev_ptr= &data->data;
    thd->data= data;
  }

  data->rows++;
  if (!(cur= (MYSQL_ROWS *)alloc_root(alloc, sizeof(MYSQL_ROWS)+packet->length())))
  {
    my_error(ER_OUT_OF_RESOURCES,MYF(0));
    return true;
  }
  cur->data= (MYSQL_ROW)(((char *)cur) + sizeof(MYSQL_ROWS));
  memcpy(cur->data, packet->ptr()+1, packet->length()-1);

  *data->prev_ptr= cur;
  data->prev_ptr= &cur->next;
  cur->next= 0;
  
  return false;
}

void
send_ok(THD *thd,ha_rows affected_rows,ulonglong id,const char *message)
{
  DBUG_ENTER("send_ok");
  MYSQL *mysql= current_thd->mysql;
  if (!mysql)            // bootstrap file handling
    DBUG_VOID_RETURN;
  mysql->affected_rows= affected_rows;
  mysql->insert_id= id;
  if (message)
    strmake(thd->net.last_error, message, sizeof(thd->net.last_error)-1);
  DBUG_VOID_RETURN;
}

void
send_eof(THD *thd, bool no_flush)
{
}

void Protocol_simple::prepare_for_resend()
{
  MYSQL_ROWS *cur;
  MYSQL_DATA *data= thd->data;

  DBUG_ENTER("send_data");

  if (!data)
  {
    if (!(data= (MYSQL_DATA*) my_malloc(sizeof(MYSQL_DATA),
					MYF(MY_WME | MY_ZEROFILL))))
      goto err;
    
    alloc= &data->alloc;
    init_alloc_root(alloc,8192,0);	/* Assume rowlength < 8192 */
    alloc->min_malloc=sizeof(MYSQL_ROWS);
    data->rows=0;
    data->fields=field_count;
    data->prev_ptr= &data->data;
    thd->data= data;
  }

  data->rows++;
  if (!(cur= (MYSQL_ROWS *)alloc_root(alloc, sizeof(MYSQL_ROWS)+(field_count + 1) * sizeof(char *))))
  {
    my_error(ER_OUT_OF_RESOURCES,MYF(0));
    DBUG_VOID_RETURN;
  }
  cur->data= (MYSQL_ROW)(((char *)cur) + sizeof(MYSQL_ROWS));

  *data->prev_ptr= cur;
  data->prev_ptr= &cur->next;
  next_field=cur->data;
  next_mysql_field= thd->mysql->fields;
err:
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

#if 0
/* The same as Protocol::net_store_data but does the converstion
*/
bool Protocol::convert_str(const char *from, uint length)
{
  if (!(*next_field=alloc_root(alloc, length + 1)))
    return true;
  convert->store_dest(*next_field, from, length);
  (*next_field)[length]= 0;
  if (next_mysql_field->max_length < length)
    next_mysql_field->max_length=length;
  ++next_field;
  ++next_mysql_field;

  return false;
}
#endif

bool setup_params_data(st_prep_stmt *stmt)
{                                       
  THD *thd= stmt->thd;
  List<Item> &params= thd->lex.param_list;
  List_iterator<Item> param_iterator(params);
  Item_param *param;
  ulong param_no= 0;
  MYSQL_BIND *client_param= thd->client_params;

  DBUG_ENTER("setup_params_data");

  for (;(param= (Item_param *)param_iterator++); client_param++)
  {       
    setup_param_functions(param, client_param->buffer_type);
    if (!param->long_data_supplied)
    {
      if (*client_param->is_null)
        param->maybe_null= param->null_value= 1;
      else
      {
	uchar *buff= (uchar*)client_param->buffer;
        param->maybe_null= param->null_value= 0;
        param->setup_param_func(param,&buff, 
				client_param->length ? 
				*client_param->length : 
				client_param->buffer_length);
      }
    }
    param_no++;
  }
  DBUG_RETURN(0);
}

bool setup_params_data_withlog(st_prep_stmt *stmt)
{                                       
  THD *thd= stmt->thd;
  List<Item> &params= thd->lex.param_list;
  List_iterator<Item> param_iterator(params);
  Item_param *param;
  MYSQL_BIND *client_param= thd->client_params;

  DBUG_ENTER("setup_params_data");

  String str, *res, *query= new String(stmt->query->alloced_length());  
  query->copy(*stmt->query);
  
  ulong param_no= 0;  
  uint32 length= 0;

  for (;(param= (Item_param *)param_iterator++); client_param++)
  {       
    setup_param_functions(param, client_param->buffer_type);
    if (param->long_data_supplied)
      res= param->query_val_str(&str);       
    
    else
    {
      if (*client_param->is_null)
      {
        param->maybe_null= param->null_value= 1;
        res= &my_null_string;
      }
      else
      {
	uchar *buff= (uchar*)client_param->buffer;
        param->maybe_null= param->null_value= 0;
        param->setup_param_func(param,&buff,
				client_param->length ? 
				*client_param->length : 
				client_param->buffer_length);
        res= param->query_val_str(&str);
      }
    }
    if (query->replace(param->pos_in_query+length, 1, *res))
      DBUG_RETURN(1);
    
    length+= res->length()-1;
    param_no++;
  }
  
  if (alloc_query(stmt->thd, (char *)query->ptr(), query->length()+1))
    DBUG_RETURN(1);
  
  query->free();
  DBUG_RETURN(0);
}

