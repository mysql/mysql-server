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
 */
#include "embedded_priv.h"
#include "sys/types.h"
#include "../regex/regex.h"
#include "my_sys.h"

/*
  The following is needed to not cause conflicts when we include mysqld.cc
*/

#define main main1
#define mysql_unix_port mysql_inix_port1
#define mysql_port mysql_port1
#if !defined(__WIN__)
#define net_read_timeout net_read_timeout1
#define net_write_timeout net_write_timeout1
#endif
#define changeable_vars changeable_vars1

extern "C"
{
#include "mysql_com.h"
#include "lib_vio.c"
}


class THD;

static int check_connections1(THD * thd);
static int check_connections2(THD * thd);
static bool check_user(THD *thd, enum_server_command command,
		       const char *user, const char *passwd, const char *db,
		       bool check_count);
void free_defaults_internal(char ** argv) {if (argv) free_defaults(argv);}
#define free_defaults free_defaults_internal

#if defined (__WIN__)
#include "../sql/mysqld.cpp"
#else
#include "../sql/mysqld.cc"
#endif

#define SCRAMBLE_LENGTH 8
C_MODE_START

char * get_mysql_home(){ return mysql_home;};
char * get_mysql_real_data_home(){ return mysql_real_data_home;};


bool lib_dispatch_command(enum enum_server_command command, NET *net,
			  const char *arg, ulong length)
{
  THD *thd=(THD *) net->vio->dest_thd;
  thd->store_globals();				// Fix if more than one connect
  thd->net.last_error[0]=0;			// Clear error message
  thd->net.last_errno=0;

  net_new_transaction(&thd->net);
  return dispatch_command(command, thd, (char *) arg, length + 1);
}

#ifdef _DUMMY
void lib_connection_phase(NET * net, int phase)
{
  THD * thd;
  thd = (THD *)(net->vio->dest_thd);
  if (thd)
  {
    switch (phase)
    {
    case 2: 
      check_connections2(thd);
      break;
    }
  }
}
C_MODE_END


void start_embedded_conn1(NET * net)
{
  THD * thd = new THD;
  my_net_init(&thd->net,NULL);
  /* if (protocol_version>9) */
  thd->net.return_errno=1;
  thd->thread_id = thread_id++;

  Vio * v  = net->vio;
  if (!v)
  {
    v = vio_new(0,VIO_BUFFER,0);
    net->vio = v;
  }
  if (v)
  {
    v -> dest_thd = thd;
  } 
  thd->net.vio = v;
  if (thd->store_globals())
  {
    fprintf(stderr,"store_globals failed.\n");
    return;
  }

  thd->mysys_var=my_thread_var;
  thd->dbug_thread_id=my_thread_id();
  thd->thread_stack= (char*) &thd;

  if (thd->max_join_size == HA_POS_ERROR)
    thd->options |= OPTION_BIG_SELECTS;

  thd->proc_info=0;				// Remove 'login'
  thd->command=COM_SLEEP;
  thd->version=refresh_version;
  thd->set_time();
  init_sql_alloc(&thd->mem_root,8192,8192);

  check_connections1(thd);
}

static int
check_connections1(THD *thd)
{
  uint connect_errors=0;
  NET *net= &thd->net;
  /*
  ** store the connection details
  */
  DBUG_PRINT("info", (("check_connections called by thread %d"),
	     thd->thread_id));
  DBUG_PRINT("general",("New connection received on %s",
			vio_description(net->vio)));
  if (!thd->host)                           // If TCP/IP connection
  {
      thd->host=(char*) localhost;
  }
  else /* Hostname given means that the connection was on a socket */
  {
    DBUG_PRINT("general",("Host: %s",thd->host));
    thd->ip=0;
    bzero((char*) &thd->remote,sizeof(struct sockaddr));
  }
  //vio_keepalive(net->vio, TRUE);

  /* nasty, but any other way? */
  uint pkt_len = 0;

    char buff[80],*end;
    int client_flags = CLIENT_LONG_FLAG | CLIENT_CONNECT_WITH_DB |
	               CLIENT_TRANSACTIONS;
    LINT_INIT(pkt_len);

    end=strmov(buff,server_version)+1;
    int4store((uchar*) end,thd->thread_id);
    end+=4;
    memcpy(end,thd->scramble,SCRAMBLE_LENGTH+1);
    end+=SCRAMBLE_LENGTH +1;
    int2store(end,client_flags);
    end[2]=MY_CHARSET_CURRENT;

#define MIN_HANDSHAKE_SIZE	6

    int2store(end+3,thd->server_status);
    bzero(end+5,13);
    end+=18;
    if (net_write_command(net,protocol_version, buff,
			  (uint) (end-buff))) 
    {
      inc_host_errors(&thd->remote.sin_addr);
      return(ER_HANDSHAKE_ERROR);
    }
   return 0; 
}

static int
check_connections2(THD * thd)
{
  uint connect_errors=0;
  uint pkt_len = 0;
  NET * net = &thd -> net;
  if (protocol_version>9) net -> return_errno=1;

  if ( (pkt_len=my_net_read(net)) == packet_error ||
       pkt_len < MIN_HANDSHAKE_SIZE)
  {
    inc_host_errors(&thd->remote.sin_addr);
    return(ER_HANDSHAKE_ERROR);
  }

#ifdef _CUSTOMCONFIG_
#include "_cust_sql_parse.h"
#endif
  if (connect_errors)
    reset_host_errors(&thd->remote.sin_addr);
  if (thd->packet.alloc(net_buffer_length))
    return(ER_OUT_OF_RESOURCES);

  thd->client_capabilities=uint2korr(net->read_pos);

  thd->max_packet_length=uint3korr(net->read_pos+2);
  char *user=   (char*) net->read_pos+5;
  char *passwd= strend(user)+1;
  char *db=0;
  if (passwd[0] && strlen(passwd) != SCRAMBLE_LENGTH)
    return ER_HANDSHAKE_ERROR;
  if (thd->client_capabilities & CLIENT_CONNECT_WITH_DB)
    db=strend(passwd)+1;
  if (thd->client_capabilities & CLIENT_INTERACTIVE)
    thd->inactive_timeout=net_interactive_timeout;
  if (thd->client_capabilities & CLIENT_TRANSACTIONS)
    thd->net.return_status= &thd->server_status;
  net->timeout=net_read_timeout;
  if (check_user(thd,COM_CONNECT, user, passwd, db, 1))
    return (-1);
  thd->password=test(passwd[0]);
  return 0;
}
#else
C_MODE_END
#endif /* _DUMMY */

static bool check_user(THD *thd,enum_server_command command, const char *user,
		       const char *passwd, const char *db, bool check_count)
{
  NET *net= &thd->net;
  USER_RESOURCES ur;
  thd->db=0;

  if (!(thd->user = my_strdup(user, MYF(0))))
  {
    send_error(net,ER_OUT_OF_RESOURCES);
    return 1;
  }
  thd->master_access=acl_getroot(thd, thd->host, thd->ip, thd->user,
				 passwd, thd->scramble, &thd->priv_user,
				 protocol_version == 9 ||
				 !(thd->client_capabilities &
				   CLIENT_LONG_PASSWORD),&ur);
  DBUG_PRINT("info",
	     ("Capabilities: %d  packet_length: %d  Host: '%s'  User: '%s'  Using password: %s  Access: %u  db: '%s'",
	      thd->client_capabilities, thd->max_packet_length,
	      thd->host_or_ip, thd->priv_user,
	      passwd[0] ? "yes": "no",
	      thd->master_access, thd->db ? thd->db : "*none*"));
  if (thd->master_access & NO_ACCESS)
  {
    net_printf(net, ER_ACCESS_DENIED_ERROR,
	       thd->user,
	       thd->host_or_ip,
	       passwd[0] ? ER(ER_YES) : ER(ER_NO));
    mysql_log.write(thd,COM_CONNECT,ER(ER_ACCESS_DENIED_ERROR),
		    thd->user,
		    thd->host_or_ip,
		    passwd[0] ? ER(ER_YES) : ER(ER_NO));
    return(1);					// Error already given
  }
  if (check_count)
  {
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    bool tmp=(thread_count - delayed_insert_threads >= max_connections &&
	      !(thd->master_access & PROCESS_ACL));
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
    if (tmp)
    {						// Too many connections
      send_error(net, ER_CON_COUNT_ERROR);
      return(1);
    }
  }
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
    send_ok(net);				// Ready to handle questions
  return 0;					// ok
}


extern "C"
{

static my_bool inited, org_my_init_done;

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
    argcp = &argc;
    argvp = (char***) &argv;
  }
  else
  {
    argcp = &fake_argc;
    argvp = (char ***) &fake_argv;
  }
  if (!groups)
      groups = (char**) fake_groups;

  my_umask=0660;		// Default umask for new files
  my_umask_dir=0700;		// Default umask for new directories

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

  tzset();			// Set tzname

  start_time=time((time_t*) 0);
#ifdef HAVE_TZNAME
#if defined(HAVE_LOCALTIME_R) && defined(_REENTRANT)
  {
    struct tm tm_tmp;
    localtime_r(&start_time,&tm_tmp);
    strmov(time_zone,tzname[tm_tmp.tm_isdst != 0 ? 1 : 0]);
  }
#else
  {
    struct tm *start_tm;
    start_tm=localtime(&start_time);
    strmov(time_zone,tzname[start_tm->tm_isdst != 0 ? 1 : 0]);
  }
#endif
#endif

  if (gethostname(glob_hostname,sizeof(glob_hostname)-4) < 0)
    strmov(glob_hostname,"mysql");
#ifndef DBUG_OFF
  strcat(server_version,"-debug");
#endif
  strcat(server_version,"-embedded");
  load_defaults("my", (const char **) groups, argcp, argvp);
  defaults_argv=*argvp;
  mysql_tmpdir=getenv("TMPDIR");	/* Use this if possible */
#if defined( __WIN__) || defined(OS2)
  if (!mysql_tmpdir)
    mysql_tmpdir=getenv("TEMP");
  if (!mysql_tmpdir)
    mysql_tmpdir=getenv("TMP");
#endif
  if (!mysql_tmpdir || !mysql_tmpdir[0])
    mysql_tmpdir=(char*) P_tmpdir;		/* purecov: inspected */
  set_options();
  get_options(*argcp, *argvp);

  if (opt_log || opt_update_log || opt_slow_log || opt_bin_log)
    strcat(server_version,"-log");
  DBUG_PRINT("info",("%s  Ver %s for %s on %s\n",my_progname,
		     server_version, SYSTEM_TYPE,MACHINE_TYPE));

  /* These must be set early */

  (void) pthread_mutex_init(&LOCK_mysql_create_db,MY_MUTEX_INIT_SLOW);
  (void) pthread_mutex_init(&LOCK_Acl,MY_MUTEX_INIT_SLOW);
  (void) pthread_mutex_init(&LOCK_grant,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_open,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_thread_count,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_mapped_file,MY_MUTEX_INIT_SLOW);
  (void) pthread_mutex_init(&LOCK_status,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_error_log,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_delayed_insert,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_delayed_status,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_delayed_create,MY_MUTEX_INIT_SLOW);
  (void) pthread_mutex_init(&LOCK_manager,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_crypt,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_bytes_sent,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_bytes_received,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_timezone,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_user_conn, MY_MUTEX_INIT_FAST);
  (void) pthread_cond_init(&COND_thread_count,NULL);
  (void) pthread_cond_init(&COND_refresh,NULL);
  (void) pthread_cond_init(&COND_thread_cache,NULL);
  (void) pthread_cond_init(&COND_flush_thread_cache,NULL);
  (void) pthread_cond_init(&COND_manager,NULL);
  (void) pthread_cond_init(&COND_binlog_update, NULL);

  if (set_default_charset_by_name(default_charset, MYF(MY_WME)))
  {
    mysql_server_end();
    return 1;
  }
  charsets_list = list_charsets(MYF(MY_COMPILED_SETS|MY_CONFIG_SETS));

  /* Parameter for threads created for connections */
  (void) pthread_attr_init(&connection_attrib);
  (void) pthread_attr_setdetachstate(&connection_attrib,
				     PTHREAD_CREATE_DETACHED);
  pthread_attr_setstacksize(&connection_attrib,thread_stack);
  pthread_attr_setscope(&connection_attrib, PTHREAD_SCOPE_SYSTEM);

#if defined( SET_RLIMIT_NOFILE) || defined( OS2)
  /* connections and databases neads lots of files */
  {
    uint wanted_files=10+(uint) max(max_connections*5,
				    max_connections+table_cache_size*2);
    uint files=set_maximum_open_files(wanted_files);
    if (files && files < wanted_files)		// Some systems return 0
    {
      max_connections=	(ulong) min((files-10),max_connections);
      table_cache_size= (ulong) max((files-10-max_connections)/2,64);
      DBUG_PRINT("warning",
		 ("Changed limits: max_connections: %ld  table_cache: %ld",
		  max_connections,table_cache_size));
      sql_print_error("Warning: Changed limits: max_connections: %ld  table_cache: %ld",max_connections,table_cache_size);
    }
  }
#endif
  unireg_init(opt_specialflag); /* Set up extern variabels */
  init_errmessage();		/* Read error messages from file */
  lex_init();
  item_init();
  mysys_uses_curses=0;
#ifdef USE_REGEX
  regex_init();
#endif
  if (use_temp_pool && bitmap_init(&temp_pool,1024,1))
  {
    mysql_server_end();
    return 1;
  }

  /*
  ** We have enough space for fiddling with the argv, continue
  */
  umask(((~my_umask) & 0666));
  table_cache_init();
  hostname_cache_init();
  /*sql_cache_init();*/
  randominit(&sql_rand,(ulong) start_time,(ulong) start_time/2);
  reset_floating_point_exceptions();
  init_thr_lock();
  init_slave_list();

  /* Setup log files */
  if (opt_log)
    open_log(&mysql_log, glob_hostname, opt_logname, ".log", LOG_NORMAL);
  if (opt_update_log)
  {
    open_log(&mysql_update_log, glob_hostname, opt_update_logname, "",
	     LOG_NEW);
    using_update_log=1;
  }
  if (opt_bin_log)
  {
    if (!opt_bin_logname)
    {
      char tmp[FN_REFLEN];
      strmake(tmp,glob_hostname,FN_REFLEN-5);
      strmov(strcend(tmp,'.'),"-bin");
      opt_bin_logname=my_strdup(tmp,MYF(MY_WME));
    }
    mysql_bin_log.set_index_file_name(opt_binlog_index_name);
    open_log(&mysql_bin_log, glob_hostname, opt_bin_logname, "-bin",
	     LOG_BIN);
    using_update_log=1;
  }

  if (opt_slow_log)
    open_log(&mysql_slow_log, glob_hostname, opt_slow_logname, "-slow.log",
	     LOG_NORMAL);
  if (ha_init())
  {
    sql_print_error("Can't init databases");
    exit(1);
  }
  ha_key_cache();
#ifdef HAVE_MLOCKALL
  if (locked_in_memory && !geteuid())
  {
    ha_key_cache();
    if (mlockall(MCL_CURRENT))
    {
      sql_print_error("Warning: Failed to lock memory. Errno: %d\n",errno);
    }
    else
      locked_in_memory=1;
  }
#else
  locked_in_memory=0;
#endif    

  if (opt_myisam_log)
    (void) mi_log( 1 );
  ft_init_stopwords(ft_precompiled_stopwords);       /* SerG */

  /*
    init signals & alarm
    After this we can't quit by a simple unireg_abort
  */
  error_handler_hook = my_message_sql;
  if (pthread_key_create(&THR_THD,NULL) || pthread_key_create(&THR_NET,NULL) ||
      pthread_key_create(&THR_MALLOC,NULL))
  {
    sql_print_error("Can't create thread-keys");
    exit(1);
  }
  opt_noacl = 1;				// No permissions
  if (acl_init(opt_noacl))
  {
    mysql_server_end();
    return 1;
  }

#ifdef HAVE_DLOPEN
  if (!opt_noacl)
    udf_init();
#endif

  (void) thr_setconcurrency(concurrency);	// 10 by default

  if (
#ifdef HAVE_BERKELEY_DB
      !berkeley_skip ||
#endif
      (flush_time && flush_time != ~(ulong) 0L))
  {
    pthread_t hThread;
    if (pthread_create(&hThread,&connection_attrib,handle_manager,0))
    {
      sql_print_error("Warning: Can't create thread to manage maintenance");
      mysql_server_end();
      return 1;
    }
  }
  return 0;
}


void STDCALL mysql_server_end()
{
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

void start_embedded_connection(NET * net)
{
//  start_embedded_conn1(net);
}

void end_embedded_connection(NET * net)
{
  THD *thd = (THD *) net->vio->dest_thd;
  delete thd;
}

} /* extern "C" */

NET *get_mysql_net(MYSQL *mysql)
{
  return &((THD *)mysql->net.vio->dest_thd)->net;
}

void init_embedded_mysql(MYSQL *mysql, int client_flag, char *db)
{
  THD *thd = (THD *)mysql->net.vio->dest_thd;
  mysql->reconnect= 1;			/* Reconnect as default */
  mysql->server_status= SERVER_STATUS_AUTOCOMMIT;

  mysql->protocol_version= ::protocol_version;
  mysql->thread_id= thd->thread_id;
  strmake(mysql->scramble_buff, thd->scramble, 8);
  mysql->server_capabilities= CLIENT_LONG_FLAG | CLIENT_CONNECT_WITH_DB |
	               CLIENT_TRANSACTIONS;
  mysql->server_language= MY_CHARSET_CURRENT;
  mysql->server_status= thd->server_status;
  mysql->client_flag= client_flag | mysql->options.client_flag;
  mysql->db= db;
  thd->mysql= mysql;
}

static int embedded_thd_net_init(NET *net, unsigned char *buff)
{
  net->buff = buff;
  if (net_buffer_length > max_allowed_packet)
    max_allowed_packet= net_buffer_length;
  net->buff_end= net->buff+(net->max_packet=net_buffer_length);
  net->vio= NULL;
  net->no_send_ok= 0;
  net->error=0; net->return_errno=0; net->return_status=0;
  net->timeout=(uint) net_read_timeout;		/* Timeout for read */
  net->pkt_nr= net->compress_pkt_nr=0;
  net->write_pos= net->read_pos = net->buff;
  net->last_error[0]= 0;
  net->compress= 0; 
  net->reading_or_writing= 0;
  net->where_b = net->remain_in_buf= 0;
  net->last_errno= 0;
  net->query_cache_query= 0;
  return 0;
}

void *create_embedded_thd(Vio *vio, unsigned char *buff, int client_flag, char *db)
{
  THD * thd= new THD;
  embedded_thd_net_init(&thd->net, buff);

  /* if (protocol_version>9) */
  thd->net.return_errno=1;
  thd->thread_id= thread_id++;

  if (thd->store_globals())
  {
    fprintf(stderr,"store_globals failed.\n");
    return NULL;
  }

  thd->mysys_var= my_thread_var;
  thd->dbug_thread_id= my_thread_id();
  thd->thread_stack= (char*) &thd;

  if (thd->max_join_size == HA_POS_ERROR)
    thd->options |= OPTION_BIG_SELECTS;

  thd->proc_info=0;				// Remove 'login'
  thd->command=COM_SLEEP;
  thd->version=refresh_version;
  thd->set_time();
  init_sql_alloc(&thd->mem_root,8192,8192);
  thd->client_capabilities= client_flag;
  thd->max_packet_length= max_allowed_packet;
  thd->net.vio = vio;

  if (thd->client_capabilities & CLIENT_INTERACTIVE)
    thd->inactive_timeout= net_interactive_timeout;
  if (thd->client_capabilities & CLIENT_TRANSACTIONS)
    thd->net.return_status= &thd->server_status;

  thd->db= db;
  thd->db_length= db ? strip_sp(db) : 0;
  thd->db_access= DB_ACLS;
  thd->master_access= ~NO_ACCESS;

  return thd;
}

bool send_fields(THD *thd, List<Item> &list, uint flag)
{
  List_iterator_fast<Item> it(list);
  Item                     *item;
  MEM_ROOT                 *alloc;
  MYSQL_FIELD              *field, *client_field;
  unsigned int             field_count= list.elements;
  MYSQL                    *mysql= thd->mysql;
  
  if (!(mysql->result=(MYSQL_RES*) my_malloc(sizeof(MYSQL_RES)+
				      sizeof(ulong) * field_count,
				      MYF(MY_WME | MY_ZEROFILL))))
    goto err;

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
  send_error(&thd->net,ER_OUT_OF_RESOURCES);	/* purecov: inspected */
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
  String                   *packet= &thd->packet;
  MYSQL                    *mysql= thd->mysql;
  MYSQL_DATA               *result= mysql->result->data;
  MYSQL_ROWS               **prev_ptr= &mysql->result->data->data;
  MYSQL_ROWS               *cur;
  MEM_ROOT                 *alloc= &mysql->result->data->alloc;
  char                     *to;
  int                      n_fields= items.elements;
  uchar                    *cp;
  MYSQL_FIELD              *mysql_fields= mysql->result->fields;
  MYSQL_ROW                cur_field, end_field;
  ulong                    len;


  DBUG_ENTER("send_data");

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(0);
  }

  thd->packet.length(0);
  while ((item=li++))
  {
    if (item->send(thd, packet))
    {
      packet->free();
      my_error(ER_OUT_OF_RESOURCES, MYF(0));
      DBUG_RETURN(1);
    }
  }

  result->rows++;
  if (!(cur= (MYSQL_ROWS *)alloc_root(alloc, sizeof(MYSQL_ROWS))) ||
      !(cur->data= (MYSQL_ROW)alloc_root(alloc, 
					 (n_fields + 1) * sizeof(char *) + packet->length())))
  {
    my_error(ER_OUT_OF_RESOURCES,MYF(0));
    DBUG_RETURN(1);
  }

  *result->prev_ptr= cur;
  result->prev_ptr= &cur->next;
  to= (char*) (cur->data+n_fields+1);
  cp= (uchar *)packet->ptr();
  end_field= cur->data + n_fields;

  for (cur_field=cur->data; cur_field<end_field; ++cur_field, ++mysql_fields)
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

bool do_command(THD *thd)
{
  MYSQL *mysql= thd->mysql;

  char *packet;
  uint old_timeout;
  ulong packet_length;
  NET *net;
  enum enum_server_command command;
  DBUG_ENTER("do_command");

  net= &thd->net;
  thd->current_tablenr=0;

  packet=0;
  old_timeout=net->timeout;
  net->timeout=(uint) thd->inactive_timeout;	// Wait max for 8 hours
  net->last_error[0]=0;				// Clear error message
  net->last_errno=0;

  net_new_transaction(net);
  if ((packet_length=my_net_read(net)) == packet_error)
  {
     DBUG_PRINT("info",("Got error reading command from socket %s",
			vio_description(net->vio) ));
    return TRUE;
  }
  else
  {
    packet=(char*) net->read_pos;
    command = (enum enum_server_command) (uchar) packet[0];
    DBUG_PRINT("info",("Command on %s = %d (%s)",
		       vio_description(net->vio), command,
		       command_name[command]));
  }
  net->timeout=old_timeout;			// Timeout for writing
  DBUG_RETURN(dispatch_command(command,thd, packet+1, (uint) packet_length));
}

void
send_ok(NET *net,ha_rows affected_rows,ulonglong id,const char *message)
{
  if (net->no_send_ok)				// hack for re-parsing queries
    return;

  DBUG_ENTER("send_ok");
  MYSQL *mysql= current_thd->mysql;
  mysql->affected_rows= affected_rows;
  mysql->insert_id= id;
  if (net->return_status)
    mysql->server_status= *net->return_status;
  if (message)
  {
    strmake(net->last_error, message, sizeof(net->last_error));
    mysql->info= net->last_error;
  }
  DBUG_VOID_RETURN;
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
  if (!(cur= (MYSQL_ROWS *)alloc_root(alloc, sizeof(MYSQL_ROWS))) ||
      !(cur->data= 
	(MYSQL_ROW)alloc_root(alloc, 
			      (n_fields + 1) * sizeof(char *) + data_len)))
  {
    my_error(ER_OUT_OF_RESOURCES,MYF(0));
    DBUG_RETURN(1);
  }

  *result->prev_ptr= cur;
  result->prev_ptr= &cur->next;
  to= (char*) (cur->data+n_fields+1);
  cp= (uchar *)data;
  end_field= cur->data + n_fields + 1;
  
  for (cur_field=cur->data; cur_field<end_field; ++cur_field, ++mysql_fields)
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
  DBUG_RETURN(0);
}

