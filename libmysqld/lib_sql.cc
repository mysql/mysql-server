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
#include "global.h"
#include "my_pthread.h"
#include "sys/types.h"
#include "../regex/regex.h"
#include "my_sys.h"

#define main main1
#define mysql_unix_port mysql_inix_port1
#define mysql_port mysql_port1
#define net_read_timeout net_read_timeout1
#define net_write_timeout net_write_timeout1
#define changeable_vars changeable_vars1
//#define mysql_tmpdir mysql_tmpdir1

extern "C"
{
#include "mysql_com.h"
#include "lib_vio.c"
}


class THD;

static int 
check_connections1(THD * thd);

static bool 
check_user(THD *thd, enum_server_command command,const char *user, const char *passwd, const char *db, bool check_count);

static int
check_connections2(THD * thd);

extern void free_defaults(char ** argv);
void free_defaults_internal(char ** argv){if (argv) free_defaults(argv);}
#define free_defaults free_defaults_internal

char mysql_data_home[FN_REFLEN];
char * get_mysql_data_home(){return mysql_data_home;};
#define mysql_data_home mysql_data_home_internal
#include "../sql/mysqld.cc"

#define SCRAMBLE_LENGTH 8
extern "C" {

/*
void
free_defaults(char ** argv) {};
void
load_defaults(const char *, const char **, int *, char ***) {};
*/

char *
get_mysql_home(){ return mysql_home;};
char *
get_mysql_real_data_home(){ return mysql_real_data_home;};


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



void 
lib_connection_phase(NET * net, int phase)
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
}
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
    v = vio_new(0,VIO_CLOSED,0);
    net->vio = v;
  }
  if (v)
  {
    v -> dest_thd = thd;
    /* v -> dest_net = &thd->net;	XXX: Probably not needed? */
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
  if (thd->options & OPTION_ANSI_MODE)
    thd->client_capabilities|=CLIENT_IGNORE_SPACE;

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











static bool check_user(THD *thd,enum_server_command command, const char *user,
		       const char *passwd, const char *db, bool check_count)
{
  NET *net= &thd->net;
  thd->db=0;

  if (!(thd->user = my_strdup(user, MYF(0))))
  {
    send_error(net,ER_OUT_OF_RESOURCES);
    return 1;
  }
  thd->master_access=acl_getroot(thd->host, thd->ip, thd->user,
				 passwd, thd->scramble, &thd->priv_user,
				 protocol_version == 9 ||
				 !(thd->client_capabilities &
				   CLIENT_LONG_PASSWORD));
  DBUG_PRINT("general",
	     ("Capabilities: %d  packet_length: %d  Host: '%s'  User: '%s'  Using password: %s  Access: %u  db: '%s'",
	      thd->client_capabilities, thd->max_packet_length,
	      thd->host ? thd->host : thd->ip, thd->priv_user,
	      passwd[0] ? "yes": "no",
	      thd->master_access, thd->db ? thd->db : "*none*"));
  if (thd->master_access & NO_ACCESS)
  {
    net_printf(net, ER_ACCESS_DENIED_ERROR,
	       thd->user,
	       thd->host ? thd->host : thd->ip,
	       passwd[0] ? ER(ER_YES) : ER(ER_NO));
    mysql_log.write(thd,COM_CONNECT,ER(ER_ACCESS_DENIED_ERROR),
		    thd->user,
		    thd->host ? thd->host : thd->ip ? thd->ip : "unknown ip",
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
		  thd->host ? thd->host : thd->ip ? thd->ip : "unknown ip",
		  db ? db : (char*) "");
  thd->db_access=0;
  if (db && db[0])
    return test(mysql_change_db(thd,db));
  else
    send_ok(net);				// Ready to handle questions
  return 0;					// ok
}


extern "C"{
void mysql_server_init(int argc, char **argv, const char **groups)
{
  char hostname[FN_REFLEN];

  /* This mess is to allow people to call the init function without
   * having to mess with a fake argv */
  int *argcp;
  char ***argvp;
  int fake_argc = 1;
  char *fake_argv[] = { (char *)"", 0 };
  const char *fake_groups[] = { "server", 0 };
  if (argc)
  {
    argcp = &argc;
    argvp = &argv;
  }
  else
  {
    argcp = &fake_argc;
    argvp = (char ***)&fake_argv;
  }
  if (!groups)
      groups = fake_groups;

  my_umask=0660;		// Default umask for new files
  my_umask_dir=0700;		// Default umask for new directories
  MY_INIT((char *)"mysqld");		// init my_sys library & pthreads
  tzset();			// Set tzname

  start_time=time((time_t*) 0);
#ifdef HAVE_TZNAME
#if defined(HAVE_LOCALTIME_R) && defined(_REENTRANT)
  {
    struct tm tm_tmp;
    localtime_r(&start_time,&tm_tmp);
    strmov(time_zone,tzname[tm_tmp.tm_isdst == 1 ? 1 : 0]);
  }
#else
  {
    struct tm *start_tm;
    start_tm=localtime(&start_time);
    strmov(time_zone=tzname[start_tm->tm_isdst == 1 ? 1 : 0]);
  }
#endif
#endif

  if (gethostname(hostname,sizeof(hostname)-4) < 0)
    strmov(hostname,"mysql");
  strmov(pidfile_name,hostname);
  strmov(strcend(pidfile_name,'.'),".pid");	// Add extension
#ifdef DEMO_VERSION
  strcat(server_version,"-demo");
#endif
#ifdef SHAREWARE_VERSION
  strcat(server_version,"-shareware");
#endif
#ifndef DBUG_OFF
  strcat(server_version,"-debug");
#endif
  strcat(server_version,"-library-ver");
#ifdef _CUSTOMSTARTUPCONFIG_
  if (_cust_check_startup())
  {
    /* _cust_check_startup will report startup failure error */
    exit( 1 );
  }
#endif
  load_defaults("my", groups, argcp, argvp);
  defaults_argv=*argvp;
  mysql_tmpdir=getenv("TMPDIR");	/* Use this if possible */
#ifdef __WIN__
  if (!mysql_tmpdir)
    mysql_tmpdir=getenv("TEMP");
  if (!mysql_tmpdir)
    mysql_tmpdir=getenv("TMP");
#endif
  if (!mysql_tmpdir || !mysql_tmpdir[0])
    mysql_tmpdir=strdup((char*) P_tmpdir);
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
  (void) pthread_mutex_init(&LOCK_binlog_update, MY_MUTEX_INIT_FAST);	// QQ NOT USED
  (void) pthread_mutex_init(&LOCK_slave, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_server_id, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_user_conn, MY_MUTEX_INIT_FAST);
  (void) pthread_cond_init(&COND_thread_count,NULL);
  (void) pthread_cond_init(&COND_refresh,NULL);
  (void) pthread_cond_init(&COND_thread_cache,NULL);
  (void) pthread_cond_init(&COND_flush_thread_cache,NULL);
  (void) pthread_cond_init(&COND_manager,NULL);
  (void) pthread_cond_init(&COND_binlog_update, NULL);
  (void) pthread_cond_init(&COND_slave_stopped, NULL);
  (void) pthread_cond_init(&COND_slave_start, NULL);

  if (set_default_charset_by_name(default_charset, MYF(MY_WME)))
    unireg_abort(1);
  charsets_list = list_charsets(MYF(MY_COMPILED_SETS|MY_CONFIG_SETS));


  if (!(opt_specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),CONNECT_PRIOR);
  /* Parameter for threads created for connections */
  (void) pthread_attr_init(&connection_attrib);
  (void) pthread_attr_setdetachstate(&connection_attrib,
				     PTHREAD_CREATE_DETACHED);
  pthread_attr_setstacksize(&connection_attrib,thread_stack);

  if (!(opt_specialflag & SPECIAL_NO_PRIOR))
    my_pthread_attr_setprio(&connection_attrib,WAIT_PRIOR);
  pthread_attr_setscope(&connection_attrib, PTHREAD_SCOPE_SYSTEM);

#ifdef SET_RLIMIT_NOFILE
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
  select_thread=pthread_self();
  select_thread_in_use=1;

  /*
  ** We have enough space for fiddling with the argv, continue
  */
  umask(((~my_umask) & 0666));
//  strcpy(mysql_real_data_home, "/usr/local");
  //if (my_setwd(mysql_real_data_home,MYF(MY_WME)))
  //{
  //  unireg_abort(1);				/* purecov: inspected */
  //}
  //mysql_data_home[0]=FN_CURLIB;		// all paths are relative from here
  //mysql_data_home[1]=0;

  strcpy(get_mysql_data_home(), mysql_real_data_home);

  //server_init();
  table_cache_init();
  hostname_cache_init();
  sql_cache_init();
  randominit(&sql_rand,(ulong) start_time,(ulong) start_time/2);
  reset_floating_point_exceptions();
  init_thr_lock();

  /* Setup log files */
  if (opt_log)
    open_log(&mysql_log, hostname, opt_logname, ".log", LOG_NORMAL);
  if (opt_update_log)
    open_log(&mysql_update_log, hostname, opt_update_logname, "",
	     LOG_NEW);
  if (opt_bin_log)
  {
    if(server_id)
      {
	if (!opt_bin_logname)
	  {
	    char tmp[FN_REFLEN];
	    strnmov(tmp,hostname,FN_REFLEN-5);
	    strmov(strcend(tmp,'.'),"-bin");
	    opt_bin_logname=my_strdup(tmp,MYF(MY_WME));
	  }
	mysql_bin_log.set_index_file_name(opt_binlog_index_name);
	open_log(&mysql_bin_log, hostname, opt_bin_logname, "-bin",
		 LOG_BIN);
      }
    else
      sql_print_error("Server id is not set - binary logging disabled");
  }
  
  if (opt_slow_log)
    open_log(&mysql_slow_log, hostname, opt_slow_logname, "-slow.log",
	     LOG_NORMAL);
  if (ha_init())
  {
    sql_print_error("Can't init databases");
    exit(1);
  }
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
//  init_signals();				// Creates pidfile
//SWSOFT+
	opt_noacl = 1;
  if (acl_init(opt_noacl))
  {
    select_thread_in_use=0;
    (void) pthread_kill(signal_thread,MYSQL_KILL_SIGNAL);
    exit(1);
  }
  if (!opt_noacl)
    (void) grant_init();

#ifdef HAVE_DLOPEN
  if (!opt_noacl)
    udf_init();
#endif

  if (opt_bootstrap)
  {
    int error=bootstrap(stdin);
    end_thr_alarm();				// Don't allow alarms
    unireg_abort(error ? 1 : 0);
  }
  if (opt_init_file)
  {
    if (read_init_file(opt_init_file))
    {
      end_thr_alarm();				// Don't allow alarms
      unireg_abort(1);
    }
  }
  (void) thr_setconcurrency(concurrency);	// 10 by default

  if (flush_time && flush_time != ~(ulong) 0L)
  {
    pthread_t hThread;
    if (pthread_create(&hThread,&connection_attrib,handle_manager,0))
      sql_print_error("Warning: Can't create thread to manage maintenance");
  }

  // slave thread
  if(master_host)
  {
    if(server_id)
      {
	pthread_t hThread;
	if(!opt_skip_slave_start &&
	   pthread_create(&hThread, &connection_attrib, handle_slave, 0))
	  sql_print_error("Warning: Can't create thread to handle slave");
      }
    else
      sql_print_error("Server id is not set, slave thread will not be started");
  }

  //printf(ER(ER_READY),my_progname,server_version,"");
  //printf("%s initialized.\n", server_version);
  fflush(stdout);
}

void mysql_server_end()
{
  /* (void) pthread_attr_destroy(&connection_attrib); */

  DBUG_PRINT("quit",("Exiting main thread"));

#ifdef EXTRA_DEBUG
  sql_print_error("Before Lock_thread_count");
#endif
  (void) pthread_mutex_lock(&LOCK_thread_count);
  select_thread_in_use=0;			// For close_connections
  (void) pthread_cond_broadcast(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);
#ifdef EXTRA_DEBUG
  sql_print_error("After lock_thread_count");
#endif

  /* Wait until cleanup is done */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  while (!ready_to_exit)
  {
    pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  my_thread_end();
}

my_bool mysql_thread_init()
{
#ifdef THREAD
    return my_thread_init();
#else
    return 0;
#endif
}

void mysql_thread_end()
{
#ifdef THREAD
    my_thread_end();
#endif
}

void start_embedded_connection(NET * net)
{
    start_embedded_conn1(net);
}
//====================================================================
}
int embedded_do_command(NET * net)
{
    THD * thd = (THD *) net ->vio;
    do_command(thd);	
    return 0;
}



