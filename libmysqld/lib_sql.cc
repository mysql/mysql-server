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
static char org_my_init_done;
char server_inited;

#if defined (__WIN__)
#include "../sql/mysqld.cpp"
#else
#include "../sql/mysqld.cc"
#endif

C_MODE_START
#include <mysql.h>
#include "errmsg.h"
#include <sql_common.h>

static int check_connections1(THD * thd);
static int check_connections2(THD * thd);
static bool check_user(THD *thd, enum_server_command command,
		       const char *user, const char *passwd, const char *db,
		       bool check_count);
char * get_mysql_home(){ return mysql_home;};
char * get_mysql_real_data_home(){ return mysql_real_data_home;};

my_bool STDCALL
emb_advanced_command(MYSQL *mysql, enum enum_server_command command,
		     const char *header, ulong header_length,
		     const char *arg, ulong arg_length, my_bool skip_check)
{
  my_bool result= 1;
  THD *thd=(THD *) mysql->thd;

  /* Check that we are calling the client functions in right order */
  if (mysql->status != MYSQL_STATUS_READY)
  {
    strmov(thd->net.last_error,
	   ER(thd->net.last_errno=CR_COMMANDS_OUT_OF_SYNC));
    return 1;
  }

  /* Clear result variables */
  thd->clear_error();
  mysql->affected_rows= ~(my_ulonglong) 0;
  mysql->field_count= 0;

  thd->store_globals();				// Fix if more than one connect
  free_old_query(mysql);
  result= dispatch_command(command, thd, (char *) arg, arg_length + 1);

  if (!skip_check)
    result= thd->net.last_errno ? -1 : 0;

  if ((mysql->net.last_errno= thd->net.last_errno))
  {
    memcpy(mysql->net.last_error, thd->net.last_error, 
	   sizeof(mysql->net.last_error));
    memcpy(mysql->net.sqlstate, thd->net.sqlstate, 
	   sizeof(mysql->net.sqlstate));
  }
  mysql->warning_count= ((THD*)mysql->thd)->total_warn_count;
  return result;
}

C_MODE_END

void THD::clear_error()
{
  net.last_error[0]= 0;
  net.last_errno= 0;
  net.report_error= 0;
}

static bool check_user(THD *thd,enum_server_command command, const char *user,
		       const char *passwd, const char *db, bool check_count)
{
  thd->db=0;

  if (!(thd->user = my_strdup(user, MYF(0))))
  {
    send_error(thd,ER_OUT_OF_RESOURCES);
    return 1;
  }
  thd->master_access= ~0L;			// No user checking
  thd->priv_user= thd->user;
  mysql_log.write(thd,command,
		  (thd->priv_user == thd->user ?
		   (char*) "%s@%s on %s" :
		   (char*) "%s@%s as anonymous on %s"),
		  user,
		  thd->host_or_ip,
		  db ? db : (char*) "");
  thd->db_access=0;
  if (db && db[0])
    return test(mysql_change_db(thd,db));
  else
    send_ok(thd);				// Ready to handle questions
  return 0;					// ok
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

  opt_noacl = 1;				// No permissions
  if (acl_init((THD *)0, opt_noacl))
  {
    mysql_server_end();
    return 1;
  }
  if (!opt_noacl)
    (void) grant_init((THD *)0);
  init_max_user_conn();
  init_update_queries();

#ifdef HAVE_DLOPEN
  if (!opt_noacl)
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
#ifdef THREAD
  /* Don't call my_thread_end() if the application is using MY_INIT() */
  if (!org_my_init_done)
    my_thread_end();
#endif
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
  thd->db_access= DB_ACLS;
  thd->master_access= ~NO_ACCESS;
  thd->net.query_cache_query= 0;

  return thd;
}

C_MODE_END

bool Protocol::send_fields(List<Item> *list, uint flag)
{
  List_iterator_fast<Item> it(*list);
  Item                     *item;
  MYSQL_FIELD              *field, *client_field;
  MYSQL                    *mysql= thd->mysql;
  
  DBUG_ENTER("send_fields");

  field_count= list->elements;
  if (!(mysql->result=(MYSQL_RES*) my_malloc(sizeof(MYSQL_RES)+
				      sizeof(ulong) * (field_count + 1),
				      MYF(MY_WME | MY_ZEROFILL))))
    goto err;
  mysql->result->lengths= (ulong *)(mysql->result + 1);

  mysql->field_count=field_count;
  alloc= &mysql->field_alloc;
  field= (MYSQL_FIELD *)alloc_root(alloc, sizeof(MYSQL_FIELD) * field_count);
  if (!field)
    goto err;

  client_field= field;
  while ((item= it++))
  {
    Send_field server_field;
    item->make_field(&server_field);

    client_field->db=	  strdup_root(alloc, server_field.db_name);
    client_field->table=  strdup_root(alloc, server_field.table_name);
    client_field->name=   strdup_root(alloc, server_field.col_name);
    client_field->org_table= strdup_root(alloc, server_field.org_table_name);
    client_field->org_name=  strdup_root(alloc, server_field.org_col_name);
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
	client_field->def= strdup_root(alloc, "");
      else
	client_field->def= strdup_root(alloc, tmp.ptr());
    }
    else
      client_field->def=0;
    client_field->max_length= 0;
    ++client_field;
  }
  mysql->result->fields = field;

  if (!(mysql->result->data= (MYSQL_DATA*) my_malloc(sizeof(MYSQL_DATA),
				       MYF(MY_WME | MY_ZEROFILL))))
    goto err;

  init_alloc_root(&mysql->result->data->alloc,8192,0);	/* Assume rowlength < 8192 */
  mysql->result->data->alloc.min_malloc=sizeof(MYSQL_ROWS);
  mysql->result->data->rows=0;
  mysql->result->data->fields=field_count;
  mysql->result->field_count=field_count;
  mysql->result->data->prev_ptr= &mysql->result->data->data;

  mysql->result->field_alloc=	mysql->field_alloc;
  mysql->result->current_field=0;
  mysql->result->current_row=0;

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
  *next_field= 0;
  return false;
}

void
send_ok(THD *thd,ha_rows affected_rows,ulonglong id,const char *message)
{
  DBUG_ENTER("send_ok");
  MYSQL *mysql= current_thd->mysql;
  mysql->affected_rows= affected_rows;
  mysql->insert_id= id;
  if (message)
  {
    strmake(thd->net.last_error, message, sizeof(thd->net.last_error)-1);
  }
  DBUG_VOID_RETURN;
}

void
send_eof(THD *thd, bool no_flush)
{
}

void Protocol_simple::prepare_for_resend()
{
  MYSQL_ROWS               *cur;
  MYSQL_DATA               *result= thd->mysql->result->data;

  DBUG_ENTER("send_data");

  alloc= &result->alloc;
  result->rows++;
  if (!(cur= (MYSQL_ROWS *)alloc_root(alloc, sizeof(MYSQL_ROWS)+(field_count + 1) * sizeof(char *))))
  {
    my_error(ER_OUT_OF_RESOURCES,MYF(0));
    DBUG_VOID_RETURN;
  }
  cur->data= (MYSQL_ROW)(((char *)cur) + sizeof(MYSQL_ROWS));

  *result->prev_ptr= cur;
  result->prev_ptr= &cur->next;
  next_field=cur->data;
  next_mysql_field= thd->mysql->result->fields;

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
  if (!(*next_field=alloc_root(alloc, length + 1)))
    return true;
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
