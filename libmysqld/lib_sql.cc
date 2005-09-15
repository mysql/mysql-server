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

void embedded_get_error(MYSQL *mysql)
{
  THD *thd=(THD *) mysql->thd;
  NET *net= &mysql->net;
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
}

static my_bool
emb_advanced_command(MYSQL *mysql, enum enum_server_command command,
		     const char *header, ulong header_length,
		     const char *arg, ulong arg_length, my_bool skip_check)
{
  my_bool result= 1;
  THD *thd=(THD *) mysql->thd;
  NET *net= &mysql->net;

  if (thd->data)
  {
    free_rows(thd->data);
    thd->data= 0;
  }
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

  /*
    If mysql->field_count is set it means the parsing of the query was OK
    and metadata was returned (see Protocol::send_fields).
    In this case we postpone the error to be returned in mysql_stmt_store_result
    (see emb_read_rows) to behave just as standalone server.
  */
  if (!mysql->field_count)
    embedded_get_error(mysql);
  mysql->server_status= thd->server_status;
  mysql->warning_count= ((THD*)mysql->thd)->total_warn_count;
  return result;
}

static void emb_flush_use_result(MYSQL *mysql)
{
  MYSQL_DATA *data= ((THD*)(mysql->thd))->data;

  if (data)
  {
    free_rows(data);
    ((THD*)(mysql->thd))->data= NULL;
  }
}

static MYSQL_DATA *
emb_read_rows(MYSQL *mysql, MYSQL_FIELD *mysql_fields __attribute__((unused)),
	      unsigned int fields __attribute__((unused)))
{
  MYSQL_DATA *result= ((THD*)mysql->thd)->data;
  embedded_get_error(mysql);
  if (mysql->net.last_errno)
    return NULL;
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

static MYSQL_FIELD *emb_list_fields(MYSQL *mysql)
{
  return mysql->fields;
}

static my_bool emb_read_prepare_result(MYSQL *mysql, MYSQL_STMT *stmt)
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

static void emb_fetch_lengths(ulong *to, MYSQL_ROW column,
			      unsigned int field_count)
{ 
  MYSQL_ROW end;

  for (end=column + field_count; column != end ; column++,to++)
    *to= *column ? *(uint *)((*column) - sizeof(uint)) : 0;
}

static my_bool emb_mysql_read_query_result(MYSQL *mysql)
{
  if (mysql->net.last_errno)
    return -1;

  if (mysql->field_count)
    mysql->status=MYSQL_STATUS_GET_RESULT;

  return 0;
}

static int emb_stmt_execute(MYSQL_STMT *stmt)
{
  DBUG_ENTER("emb_stmt_execute");
  char header[4];
  int4store(header, stmt->stmt_id);
  THD *thd= (THD*)stmt->mysql->thd;
  thd->client_param_count= stmt->param_count;
  thd->client_params= stmt->params;
  if (emb_advanced_command(stmt->mysql, COM_STMT_EXECUTE,0,0,
                           header, sizeof(header), 1) ||
      emb_mysql_read_query_result(stmt->mysql))
  {
    NET *net= &stmt->mysql->net;
    set_stmt_errmsg(stmt, net->last_error, net->last_errno, net->sqlstate);
    DBUG_RETURN(1);
  }
  stmt->affected_rows= stmt->mysql->affected_rows;
  stmt->insert_id= stmt->mysql->insert_id;
  DBUG_RETURN(0);
}

int emb_read_binary_rows(MYSQL_STMT *stmt)
{
  MYSQL_DATA *data;
  if (!(data= emb_read_rows(stmt->mysql, 0, 0)))
    return 1;
  stmt->result= *data;
  my_free((char *) data, MYF(0));
  return 0;
}

int emb_unbuffered_fetch(MYSQL *mysql, char **row)
{
  MYSQL_DATA *data= ((THD*)mysql->thd)->data;
  embedded_get_error(mysql);
  if (mysql->net.last_errno)
    return mysql->net.last_errno;
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

static void emb_free_embedded_thd(MYSQL *mysql)
{
  THD *thd= (THD*)mysql->thd;
  if (thd->data)
    free_rows(thd->data);
  thread_count--;
  delete thd;
  mysql->thd=0;
}

static const char * emb_read_statistics(MYSQL *mysql)
{
  THD *thd= (THD*)mysql->thd;
  return thd->net.last_error;
}


static MYSQL_RES * emb_mysql_store_result(MYSQL *mysql)
{
  return mysql_store_result(mysql);
}

my_bool emb_next_result(MYSQL *mysql)
{
  THD *thd= (THD*)mysql->thd;
  DBUG_ENTER("emb_next_result");

  if (emb_advanced_command(mysql, COM_QUERY,0,0,
			   thd->query_rest.ptr(),thd->query_rest.length(),1) ||
      emb_mysql_read_query_result(mysql))
    DBUG_RETURN(1);

  DBUG_RETURN(0);				/* No more results */
}

int emb_read_change_user_result(MYSQL *mysql, 
				char *buff __attribute__((unused)),
				const char *passwd __attribute__((unused)))
{
  return mysql_errno(mysql);
}

MYSQL_METHODS embedded_methods= 
{
  emb_mysql_read_query_result,
  emb_advanced_command,
  emb_read_rows,
  emb_mysql_store_result,
  emb_fetch_lengths, 
  emb_flush_use_result,
  emb_list_fields,
  emb_read_prepare_result,
  emb_stmt_execute,
  emb_read_binary_rows,
  emb_unbuffered_fetch,
  emb_free_embedded_thd,
  emb_read_statistics,
  emb_next_result,
  emb_read_change_user_result
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
  if (!(acl_error= acl_init((THD *)0, opt_noacl)) &&
      !opt_noacl)
    (void) grant_init((THD *)0);
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
    goto err;
  }

  thd->mysys_var= my_thread_var;
  thd->dbug_thread_id= my_thread_id();
  thd->thread_stack= (char*) &thd;

/* TODO - add init_connect command execution */

  if (thd->variables.max_join_size == HA_POS_ERROR)
    thd->options |= OPTION_BIG_SELECTS;
  thd->proc_info=0;				// Remove 'login'
  thd->command=COM_SLEEP;
  thd->version=refresh_version;
  thd->set_time();
  thd->init_for_queries();
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
err:
  delete(thd);
  return NULL;
}

#ifdef NO_EMBEDDED_ACCESS_CHECKS
int check_embedded_connection(MYSQL *mysql)
{
  THD *thd= (THD*)mysql->thd;
  st_security_context *sctx= thd->security_ctx;
  sctx->host_or_ip= sctx->host= (char*)my_localhost;
  sctx->priv_user= sctx->user= my_strdup(mysql->user, MYF(0));
  return check_user(thd, COM_CONNECT, NULL, 0, thd->db, true);
}

#else
int check_embedded_connection(MYSQL *mysql)
{
  THD *thd= (THD*)mysql->thd;
  int result;
  char scramble_buff[SCRAMBLE_LENGTH];
  int passwd_len;

  if (mysql->options.client_ip)
  {
    thd->host= my_strdup(mysql->options.client_ip, MYF(0));
    thd->ip= my_strdup(thd->host, MYF(0));
  }
  else
    thd->host= (char*)my_localhost;
  thd->host_or_ip= thd->host;

  if (acl_check_host(thd->host,thd->ip))
  {
    result= ER_HOST_NOT_PRIVILEGED;
    goto err;
  }

  thd->user= my_strdup(mysql->user, MYF(0));
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


bool Protocol::send_fields(List<Item> *list, uint flags)
{
  List_iterator_fast<Item> it(*list);
  Item                     *item;
  MYSQL_FIELD              *client_field;
  MYSQL                    *mysql= thd->mysql;
  MEM_ROOT                 *field_alloc;
  CHARSET_INFO             *thd_cs= thd->variables.character_set_results;
  CHARSET_INFO             *cs= system_charset_info;
  
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
  thd->mysql->field_count= field_count;

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
  cur->length= packet->length();       /* To allow us to do sanity checks */

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
  {
    strmake(thd->net.last_error, message, sizeof(thd->net.last_error)-1);
    mysql->info= thd->net.last_error;
  }
  DBUG_VOID_RETURN;
}

void
send_eof(THD *thd)
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

