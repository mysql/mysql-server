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
static char inited, org_my_init_done;

#if defined (__WIN__)
#include "../sql/mysqld.cpp"
#else
#include "../sql/mysqld.cc"
#endif

#define SCRAMBLE_LENGTH 8
C_MODE_START
#include <mysql.h>
#include "errmsg.h"

static int check_connections1(THD * thd);
static int check_connections2(THD * thd);
static bool check_user(THD *thd, enum_server_command command,
		       const char *user, const char *passwd, const char *db,
		       bool check_count);
char * get_mysql_home(){ return mysql_home;};
char * get_mysql_real_data_home(){ return mysql_real_data_home;};

my_bool simple_command(MYSQL *mysql,enum enum_server_command command, const char *arg,
	       ulong length, my_bool skipp_check)
{
  my_bool result= 1;
  THD *thd=(THD *) mysql->thd;

  /* Check that we are calling the client functions in right order */
  if (mysql->status != MYSQL_STATUS_READY)
  {
    strmov(thd->net.last_error,ER(thd->net.last_errno=CR_COMMANDS_OUT_OF_SYNC));
    return 1;
  }

  /* Clear result variables */
  thd->net.last_error[0]=0;
  thd->net.last_errno=0;
  mysql->affected_rows= ~(my_ulonglong) 0;

  thd->store_globals();				// Fix if more than one connect
  result= dispatch_command(command, thd, (char *) arg, length + 1);

  if (!skipp_check)
    result= thd->net.last_errno ? -1 : 0;

  mysql->last_error= thd->net.last_error;
  mysql->last_errno= thd->net.last_errno;
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
  USER_RESOURCES ur;
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

ulong		max_allowed_packet, net_buffer_length;
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
  if (!inited)
  {
    inited=1;
    org_my_init_done=my_init_done;
  }
  if (!org_my_init_done)
  {
    MY_INIT((char *)"mysql_embedded");	// init my_sys library & pthreads
  }

  if (init_common_variables("my", argc, argv, (const char **)groups))
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

  if (init_thread_environement())
  {
    mysql_server_end();
    return 1;
  }

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
	     opt_binlog_index_name, LOG_BIN);
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

my_bool STDCALL mysql_thread_init()
{
#ifdef THREAD
  return my_thread_init();
#else
  return 0;
#endif
}

void STDCALL mysql_thread_end()
{
#ifdef THREAD
  my_thread_end();
#endif
}
} /* extern "C" */

C_MODE_START
void init_embedded_mysql(MYSQL *mysql, int client_flag, char *db)
{
  THD *thd = (THD *)mysql->thd;
  thd->mysql= mysql;
  mysql->last_error= thd->net.last_error;
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

bool send_fields(THD *thd, List<Item> &list, uint flag)
{
  List_iterator_fast<Item> it(list);
  Item                     *item;
  MEM_ROOT                 *alloc;
  MYSQL_FIELD              *field, *client_field;
  unsigned int             field_count= list.elements;
  MYSQL                    *mysql= thd->mysql;
  
  if (!(mysql->result=(MYSQL_RES*) my_malloc(sizeof(MYSQL_RES)+
				      sizeof(ulong) * (field_count + 1),
				      MYF(MY_WME | MY_ZEROFILL))))
    goto err;
  mysql->result->lengths= (ulong *)(mysql->result + 1);

  mysql->field_count=field_count;
  alloc= &mysql->field_alloc;
  field= (MYSQL_FIELD *)alloc_root(alloc, sizeof(MYSQL_FIELD)*list.elements);
  if (!field)
    goto err;

  client_field= field;
  while ((item= it++))
  {
    Send_field server_field;
    item->make_field(&server_field);

    client_field->table=  strdup_root(alloc, server_field.table_name);
    client_field->name=   strdup_root(alloc,server_field.col_name);
    client_field->length= server_field.length;
    client_field->type=   server_field.type;
    client_field->flags= server_field.flags;
    client_field->decimals= server_field.decimals;

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

  return 0;
 err:
  send_error(thd, ER_OUT_OF_RESOURCES);	/* purecov: inspected */
  return 1;					/* purecov: inspected */
}

/* Get the length of next field. Change parameter to point at fieldstart */
static ulong
net_field_length(uchar **packet)
{
  reg1 uchar *pos= *packet;
  if (*pos < 251)
  {
    (*packet)++;
    return (ulong) *pos;
  }
  if (*pos == 251)
  {
    (*packet)++;
    return NULL_LENGTH;
  }
  if (*pos == 252)
  {
    (*packet)+=3;
    return (ulong) uint2korr(pos+1);
  }
  if (*pos == 253)
  {
    (*packet)+=4;
    return (ulong) uint3korr(pos+1);
  }
  (*packet)+=9;					/* Must be 254 when here */
  return (ulong) uint4korr(pos+1);
}

bool select_send::send_data(List<Item> &items)
{
  List_iterator_fast<Item> li(items);
  Item                     *item;
  MYSQL_ROWS               *cur;
  int                      n_fields= items.elements;
  ulong                    len;
  CONVERT                  *convert= thd->variables.convert_set;
  CHARSET_INFO             *charset_info= thd->packet.charset();
  MYSQL_DATA               *result= thd->mysql->result->data;
  MEM_ROOT                 *alloc= &result->alloc;
  MYSQL_ROW                cur_field;
  MYSQL_FIELD              *mysql_fields= thd->mysql->result->fields;

  DBUG_ENTER("send_data");

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(0);
  }

  result->rows++;
  if (!(cur= (MYSQL_ROWS *)alloc_root(alloc, sizeof(MYSQL_ROWS)+(n_fields + 1) * sizeof(char *))))
  {
    my_error(ER_OUT_OF_RESOURCES,MYF(0));
    DBUG_RETURN(1);
  }
  cur->data= (MYSQL_ROW)(((char *)cur) + sizeof(MYSQL_ROWS));

  *result->prev_ptr= cur;
  result->prev_ptr= &cur->next;
  cur_field=cur->data;
  for (item=li++; item; item=li++, cur_field++, mysql_fields++)
  {
    if (item->embedded_send(convert, charset_info, alloc, cur_field, &len))
    {
      my_error(ER_OUT_OF_RESOURCES,MYF(0));
      DBUG_RETURN(1);
    }
    if (mysql_fields->max_length < len)
      mysql_fields->max_length=len;
  }

  *cur_field= 0;

  DBUG_RETURN(0);
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
/*  static char eof_buff[1]= { (char) 254 };
  NET *net= &thd->net;
  DBUG_ENTER("send_eof");
  if (net->vio != 0)
  {
    if (!no_flush && (thd->client_capabilities & CLIENT_PROTOCOL_41))
    {
      char buff[5];
      uint tmp= min(thd->total_warn_count, 65535);
      buff[0]=254;
      int2store(buff+1, tmp);
      int2store(buff+3, 0);			// No flags yet
      VOID(my_net_write(net,buff,5));
      VOID(net_flush(net));
    }
    else
    {
      VOID(my_net_write(net,eof_buff,1));
      if (!no_flush)
	VOID(net_flush(net));
    }
  }
  DBUG_VOID_RETURN;
*/
}


int embedded_send_row(THD *thd, int n_fields, char *data, int data_len)
{
  MYSQL                    *mysql= thd->mysql;
  MYSQL_DATA               *result= mysql->result->data;
  MYSQL_ROWS               **prev_ptr= &mysql->result->data->data;
  MYSQL_ROWS               *cur;
  MEM_ROOT                 *alloc= &mysql->result->data->alloc;
  char                     *to;
  uchar                    *cp;
  MYSQL_FIELD              *mysql_fields= mysql->result->fields;
  MYSQL_ROW                cur_field, end_field;
  ulong                    len;

  DBUG_ENTER("embedded_send_row");

  result->rows++;
  if (!(cur= (MYSQL_ROWS *)alloc_root(alloc, sizeof(MYSQL_ROWS) + (n_fields + 1) * sizeof(MYSQL_ROW) + data_len)))
  {
    my_error(ER_OUT_OF_RESOURCES,MYF(0));
    DBUG_RETURN(1);
  }
  cur->data= (MYSQL_ROW)(cur + 1);

  *result->prev_ptr= cur;
  result->prev_ptr= &cur->next;
  to= (char*) (cur->data+n_fields+1);
  cp= (uchar *)data;
  end_field= cur->data + n_fields;
  
  for (cur_field=cur->data; cur_field<end_field; cur_field++, mysql_fields++)
  {
    if ((len= (ulong) net_field_length(&cp)) == NULL_LENGTH)
    {
      *cur_field = 0;
    }
    else
    {
      *cur_field= to;
      memcpy(to,(char*) cp,len);
      to[len]=0;
      to+=len+1;
      cp+=len;
      if (mysql_fields->max_length < len)
	mysql_fields->max_length=len;
    }
  }

  *cur_field= to;

  DBUG_RETURN(0);
}

uint STDCALL mysql_warning_count(MYSQL *mysql)
{
  return ((THD *)mysql->thd)->total_warn_count;
}
