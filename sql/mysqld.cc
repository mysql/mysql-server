/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysql_priv.h"
#include <m_ctype.h>
#include <my_dir.h>
#include "sql_acl.h"
#include "slave.h"
#include "sql_repl.h"
#include "repl_failsafe.h"
#include "stacktrace.h"
#ifdef HAVE_BERKELEY_DB
#include "ha_berkeley.h"
#endif
#ifdef HAVE_INNOBASE_DB
#include "ha_innodb.h"
#endif
#include "ha_myisam.h"
#include <nisam.h>
#include <thr_alarm.h>
#include <ft_global.h>

#ifndef DBUG_OFF
#define ONE_THREAD
#endif

#ifdef SAFEMALLOC
#define SHUTDOWN_THD shutdown_th=pthread_self();
#define MAIN_THD main_th=pthread_self();
#define SIGNAL_THD signal_th=pthread_self();
#else
#define SHUTDOWN_THD
#define MAIN_THD
#define SIGNAL_THD
#endif

/* stack traces are only supported on linux intel */
#if defined(__linux__)  && defined(__i386__) && defined(USE_PSTACK)
#define	HAVE_STACK_TRACE_ON_SEGV
#include "../pstack/pstack.h"
char pstack_file_name[80];
#endif /* __linux__ */

#if defined(HAVE_DEC_3_2_THREADS) || defined(SIGNALS_DONT_BREAK_READ)
#define HAVE_CLOSE_SERVER_SOCK 1
#endif  

extern "C" {					// Because of SCO 3.2V4.2
#include <errno.h>
#include <sys/stat.h>
#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__				// Skip warnings in getopt.h
#endif
#include <my_getopt.h>
#ifdef HAVE_SYSENT_H
#include <sysent.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>				// For getpwent
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif

#if defined(OS2)
#  include <sys/un.h>
#elif !defined( __WIN__)
#include <sys/resource.h>
#ifdef HAVE_SYS_UN_H
#  include <sys/un.h>
#endif
#include <netdb.h>
#ifdef HAVE_SELECT_H
#  include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/utsname.h>
#endif /* __WIN__ */

#ifdef HAVE_LIBWRAP
#include <tcpd.h>
#include <syslog.h>
#ifdef NEED_SYS_SYSLOG_H
#include <sys/syslog.h>
#endif /* NEED_SYS_SYSLOG_H */
int allow_severity = LOG_INFO;
int deny_severity = LOG_WARNING;

#ifdef __STDC__
#define my_fromhost(A)	   fromhost(A)
#define my_hosts_access(A) hosts_access(A)
#define my_eval_client(A)  eval_client(A)
#else
#define my_fromhost(A)	   fromhost()
#define my_hosts_access(A) hosts_access()
#define my_eval_client(A)  eval_client()
#endif
#endif /* HAVE_LIBWRAP */

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifdef _AIX41
int initgroups(const char *,unsigned int);
#endif

#if defined(__FreeBSD__) && defined(HAVE_IEEEFP_H)
#include <ieeefp.h>
#ifdef HAVE_FP_EXCEPT				// Fix type conflict
typedef fp_except fp_except_t;
#endif

  /* We can't handle floating point expections with threads, so disable
     this on freebsd
  */

inline void reset_floating_point_exceptions()
{
  /* Don't fall for overflow, underflow,divide-by-zero or loss of precision */
#if defined(__i386__)
  fpsetmask(~(FP_X_INV | FP_X_DNML | FP_X_OFL | FP_X_UFL | FP_X_DZ |
	      FP_X_IMP));
#else
 fpsetmask(~(FP_X_INV |             FP_X_OFL | FP_X_UFL | FP_X_DZ |
	     FP_X_IMP));
#endif
}
#else
#define reset_floating_point_exceptions()
#endif /* __FreeBSD__ && HAVE_IEEEFP_H */

} /* cplusplus */


#if defined(HAVE_LINUXTHREADS)
#define THR_KILL_SIGNAL SIGINT
#else
#define THR_KILL_SIGNAL SIGUSR2		// Can't use this with LinuxThreads
#endif

#ifdef HAVE_GLIBC2_STYLE_GETHOSTBYNAME_R
#include <sys/types.h>
#else
#include <my_pthread.h>			// For thr_setconcurency()
#endif
#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_NOFILE) && !defined(HAVE_mit_thread)
#define SET_RLIMIT_NOFILE
#endif

#ifdef SOLARIS
extern "C" int gethostname(char *name, int namelen);
#endif

#define MYSQL_KILL_SIGNAL SIGTERM

#ifndef DBUG_OFF
static const char* default_dbug_option=IF_WIN("d:t:i:O,\\mysqld.trace",
					      "d:t:i:o,/tmp/mysqld.trace");
#endif

#ifdef __NT__
static char szPipeName [ 257 ];
static SECURITY_ATTRIBUTES saPipeSecurity;
static SECURITY_DESCRIPTOR sdPipeDescriptor;
static HANDLE hPipe = INVALID_HANDLE_VALUE;
static pthread_cond_t COND_handler_count;
static uint handler_count;
static bool opt_enable_named_pipe = 0;
#endif
#ifdef __WIN__
static bool opt_console=0,start_mode=0;
#endif

/* Set prefix for windows binary */
#ifdef __WIN__
#undef MYSQL_SERVER_SUFFIX
#ifdef __NT__
#if defined(HAVE_INNOBASE_DB) || defined(HAVE_BERKELEY_DB)
#define MYSQL_SERVER_SUFFIX "-max-nt"
#else
#define MYSQL_SERVER_SUFFIX "-nt"
#endif /* ...DB */
#elif defined(HAVE_INNOBASE_DB) || defined(HAVE_BERKELEY_DB)
#define MYSQL_SERVER_SUFFIX "-max"
#else
#define MYSQL_SERVER_SUFFIX ""
#endif /* __NT__ */
#endif

#ifdef HAVE_BERKELEY_DB
SHOW_COMP_OPTION have_berkeley_db=SHOW_OPTION_YES;
#else
SHOW_COMP_OPTION have_berkeley_db=SHOW_OPTION_NO;
#endif
#ifdef HAVE_INNOBASE_DB
SHOW_COMP_OPTION have_innodb=SHOW_OPTION_YES;
#else
SHOW_COMP_OPTION have_innodb=SHOW_OPTION_NO;
#endif
#ifdef HAVE_ISAM
SHOW_COMP_OPTION have_isam=SHOW_OPTION_YES;
#else
SHOW_COMP_OPTION have_isam=SHOW_OPTION_NO;
#endif
#ifdef USE_RAID
SHOW_COMP_OPTION have_raid=SHOW_OPTION_YES;
#else
SHOW_COMP_OPTION have_raid=SHOW_OPTION_NO;
#endif
#ifdef HAVE_OPENSSL
SHOW_COMP_OPTION have_openssl=SHOW_OPTION_YES;
#else
SHOW_COMP_OPTION have_openssl=SHOW_OPTION_NO;
#endif
#ifdef HAVE_BROKEN_REALPATH
SHOW_COMP_OPTION have_symlink=SHOW_OPTION_NO;
#else
SHOW_COMP_OPTION have_symlink=SHOW_OPTION_YES;
#endif
#ifdef HAVE_QUERY_CACHE
SHOW_COMP_OPTION have_query_cache=SHOW_OPTION_YES;
#else
SHOW_COMP_OPTION have_query_cache=SHOW_OPTION_NO;
#endif

bool opt_large_files= sizeof(my_off_t) > 4;

/*
  Variables to store startup options
*/

bool opt_skip_slave_start = 0; // If set, slave is not autostarted
/*
  If set, some standard measures to enforce slave data integrity will not
  be performed
*/
bool opt_reckless_slave = 0; 

ulong back_log, connect_timeout, concurrency;
char mysql_home[FN_REFLEN], pidfile_name[FN_REFLEN], time_zone[30];
bool opt_log, opt_update_log, opt_bin_log, opt_slow_log;
bool opt_disable_networking=0, opt_skip_show_db=0;
my_bool opt_local_infile, opt_external_locking, opt_slave_compressed_protocol;

static bool opt_do_pstack = 0;
static ulong opt_specialflag=SPECIAL_ENGLISH;

static ulong opt_myisam_block_size;
static my_socket unix_sock= INVALID_SOCKET,ip_sock= INVALID_SOCKET;
static my_string opt_logname=0,opt_update_logname=0,
       opt_binlog_index_name = 0,opt_slow_logname=0;

static char* mysql_home_ptr= mysql_home;
static char* pidfile_name_ptr= pidfile_name;
static pthread_t select_thread;
static bool opt_noacl, opt_bootstrap=0, opt_myisam_log=0;
bool opt_sql_bin_update = 0, opt_log_slave_updates = 0;
bool opt_safe_user_create = 0, opt_no_mix_types = 0;
my_bool opt_safe_show_db=0, lower_case_table_names, opt_old_rpl_compat;
my_bool opt_show_slave_auth_info;
volatile bool  mqh_used = 0;
FILE *bootstrap_file=0;
int segfaulted = 0; // ensure we do not enter SIGSEGV handler twice

/*
  If sql_bin_update is true, SQL_LOG_UPDATE and SQL_LOG_BIN are kept in sync,
  and are treated as aliases for each other
*/

static bool kill_in_progress=FALSE;
static struct rand_struct sql_rand;
static int cleanup_done;
static char **defaults_argv;
char glob_hostname[FN_REFLEN];

#include "sslopt-vars.h"
#ifdef HAVE_OPENSSL
char *des_key_file = 0;
struct st_VioSSLAcceptorFd *ssl_acceptor_fd= 0;
#endif /* HAVE_OPENSSL */

I_List <i_string_pair> replicate_rewrite_db;
I_List<i_string> replicate_do_db, replicate_ignore_db;
// allow the user to tell us which db to replicate and which to ignore
I_List<i_string> binlog_do_db, binlog_ignore_db;

/* if we guessed server_id , we need to know about it */
ulong server_id= 0;			// Must be long becasue of set_var.cc
bool server_id_supplied = 0;

uint mysql_port;
uint test_flags = 0, select_errors=0, dropping_tables=0,ha_open_options=0;
uint volatile thread_count=0, thread_running=0, kill_cached_threads=0,
	      wake_thread=0;
ulong thd_startup_options=(OPTION_UPDATE_LOG | OPTION_AUTO_IS_NULL |
			   OPTION_BIN_LOG | OPTION_QUOTE_SHOW_CREATE );
uint protocol_version=PROTOCOL_VERSION;
struct system_variables global_system_variables;
struct system_variables max_system_variables;
ulong keybuff_size,table_cache_size,
      thread_stack,
      thread_stack_min,what_to_log= ~ (1L << (uint) COM_TIME),
      query_buff_size, mysqld_net_retry_count,
      slow_launch_time = 2L,
      slave_open_temp_tables=0,
      open_files_limit=0, max_binlog_size;
ulong com_stat[(uint) SQLCOM_END], com_other;
ulong slave_net_timeout;
ulong thread_cache_size=0, binlog_cache_size=0, max_binlog_cache_size=0;
ulong query_cache_size=0;
#ifdef HAVE_QUERY_CACHE
ulong query_cache_limit=0;
Query_cache query_cache;
#endif

volatile ulong cached_thread_count=0;

// replication parameters, if master_host is not NULL, we are a slave
my_string master_user = (char*) "test", master_password = 0, master_host=0,
  master_info_file = (char*) "master.info",
  relay_log_info_file = (char*) "relay-log.info",
  master_ssl_key=0, master_ssl_cert=0;
my_string report_user = 0, report_password = 0, report_host=0;
 
const char *localhost=LOCAL_HOST;
const char *delayed_user="DELAYED";
uint master_port = MYSQL_PORT, master_connect_retry = 60;
uint report_port = MYSQL_PORT;
bool master_ssl = 0;

ulong master_retry_count=0;
ulong bytes_sent = 0L, bytes_received = 0L;

bool opt_endinfo,using_udf_functions, locked_in_memory;
bool opt_using_transactions, using_update_log;
bool volatile abort_loop,select_thread_in_use,grant_option;
bool volatile ready_to_exit,shutdown_in_progress;
ulong refresh_version=1L,flush_version=1L;	/* Increments on each reload */
ulong query_id=1L,long_query_count,aborted_threads,
      aborted_connects,delayed_insert_timeout,delayed_insert_limit,
      delayed_queue_size,delayed_insert_threads,delayed_insert_writes,
      delayed_rows_in_use,delayed_insert_errors,flush_time, thread_created;
ulong filesort_rows, filesort_range_count, filesort_scan_count;
ulong filesort_merge_passes;
ulong select_range_check_count, select_range_count, select_scan_count;
ulong select_full_range_join_count,select_full_join_count;
ulong specialflag=0,opened_tables=0,created_tmp_tables=0,
      created_tmp_disk_tables=0;
ulong max_connections,max_insert_delayed_threads,max_used_connections,
      max_connect_errors, max_user_connections = 0;
ulong thread_id=1L,current_pid;
ulong slow_launch_threads = 0;
  
char mysql_real_data_home[FN_REFLEN],
     language[LIBLEN],reg_ext[FN_EXTLEN],
     mysql_charsets_dir[FN_REFLEN], *charsets_list,
     blob_newline,f_fyllchar,max_sort_char,*mysqld_user,*mysqld_chroot,
     *opt_init_file;
char *language_ptr= language;
#ifndef EMBEDDED_LIBRARY
char mysql_data_home_buff[2], *mysql_data_home=mysql_data_home_buff;
bool mysql_embedded=0;
#else
char *mysql_data_home=mysql_real_data_home;
bool mysql_embedded=1;
#endif

char *opt_bin_logname = 0; // this one needs to be seen in sql_parse.cc
char *opt_relay_logname = 0, *opt_relaylog_index_name=0;
char server_version[SERVER_VERSION_LENGTH]=MYSQL_SERVER_VERSION;
const char *first_keyword="first";
const char **errmesg;			/* Error messages */
const char *myisam_recover_options_str="OFF";
const char *sql_mode_str="OFF";
ulong rpl_recovery_rank=0;

my_string mysql_unix_port=NULL, opt_mysql_tmpdir=NULL, mysql_tmpdir=NULL;
ulong my_bind_addr;			/* the address we bind to */
DATE_FORMAT dayord;
double log_10[32];			/* 10 potences */
I_List<THD> threads,thread_cache;
time_t start_time;

ulong opt_sql_mode = 0L;
const char *sql_mode_names[] =
{ "REAL_AS_FLOAT", "PIPES_AS_CONCAT", "ANSI_QUOTES", "IGNORE_SPACE",
  "SERIALIZE","ONLY_FULL_GROUP_BY", "NO_UNSIGNED_SUBTRACTION",NullS };
TYPELIB sql_mode_typelib= {array_elements(sql_mode_names)-1,"",
			   sql_mode_names};

MY_BITMAP temp_pool;
bool use_temp_pool=0;

pthread_key(MEM_ROOT*,THR_MALLOC);
pthread_key(THD*, THR_THD);
pthread_key(NET*, THR_NET);
pthread_mutex_t LOCK_mysql_create_db, LOCK_Acl, LOCK_open, LOCK_thread_count,
		LOCK_mapped_file, LOCK_status, LOCK_grant,
		LOCK_error_log,
		LOCK_delayed_insert, LOCK_delayed_status, LOCK_delayed_create,
		LOCK_crypt, LOCK_bytes_sent, LOCK_bytes_received,
	        LOCK_server_id, LOCK_global_system_variables,
		LOCK_user_conn, LOCK_slave_list, LOCK_active_mi;

pthread_cond_t COND_refresh,COND_thread_count, COND_slave_stopped,
	       COND_slave_start;
pthread_cond_t COND_thread_cache,COND_flush_thread_cache;
pthread_t signal_thread;
pthread_attr_t connection_attrib;

#ifdef __WIN__
#undef	 getpid
#include <process.h>
#if !defined(EMBEDDED_LIBRARY)
HANDLE hEventShutdown;
static char *event_name;
#include "nt_servc.h"
static	 NTService  Service;	      // Service object for WinNT
#endif
#endif

#ifdef OS2
pthread_cond_t eventShutdown;
#endif

static void start_signal_handler(void);
static void *signal_hand(void *arg);
static void set_options(void);
static void get_options(int argc,char **argv);
static char *get_relative_path(const char *path);
static void fix_paths(void);
static pthread_handler_decl(handle_connections_sockets,arg);
static pthread_handler_decl(kill_server_thread,arg);
static int bootstrap(FILE *file);
static void close_server_sock();
static bool read_init_file(char *file_name);
#ifdef __NT__
static pthread_handler_decl(handle_connections_namedpipes,arg);
#endif
extern pthread_handler_decl(handle_slave,arg);
#ifdef SET_RLIMIT_NOFILE
static uint set_maximum_open_files(uint max_file_limit);
#endif
static ulong find_bit_type(const char *x, TYPELIB *bit_lib);

/****************************************************************************
** Code to end mysqld
****************************************************************************/

static void close_connections(void)
{
#ifdef EXTRA_DEBUG
  int count=0;
#endif
  NET net;
  DBUG_ENTER("close_connections");

  /* Clear thread cache */
  kill_cached_threads++;
  flush_thread_cache();

  /* kill flush thread */
  (void) pthread_mutex_lock(&LOCK_manager);
  if (manager_thread_in_use)
  {
    DBUG_PRINT("quit",("killing manager thread: %lx",manager_thread));
   (void) pthread_cond_signal(&COND_manager);
  }
  (void) pthread_mutex_unlock(&LOCK_manager);

  /* kill connection thread */
#if !defined(__WIN__) && !defined(__EMX__) && !defined(OS2)
  DBUG_PRINT("quit",("waiting for select thread: %lx",select_thread));
  (void) pthread_mutex_lock(&LOCK_thread_count);

  while (select_thread_in_use)
  {
    struct timespec abstime;
    int error;
    LINT_INIT(error);
#ifndef DONT_USE_THR_ALARM
    if (pthread_kill(select_thread,THR_CLIENT_ALARM))
      break;					// allready dead
#endif
    set_timespec(abstime, 2);
    for (uint tmp=0 ; tmp < 10 ; tmp++)
    {
      error=pthread_cond_timedwait(&COND_thread_count,&LOCK_thread_count,
				   &abstime);
      if (error != EINTR)
	break;
    }
#ifdef EXTRA_DEBUG
    if (error != 0 && !count++)
      sql_print_error("Got error %d from pthread_cond_timedwait",error);
#endif
    close_server_sock();
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);
#endif /* __WIN__ */


  /* Abort listening to new connections */
  DBUG_PRINT("quit",("Closing sockets"));
  if ( !opt_disable_networking )
  {
    if (ip_sock != INVALID_SOCKET)
    {
      (void) shutdown(ip_sock,2);
      (void) closesocket(ip_sock);
      ip_sock= INVALID_SOCKET;
    }
  }
#ifdef __NT__
  if (hPipe != INVALID_HANDLE_VALUE && opt_enable_named_pipe)
  {
    HANDLE temp;
    DBUG_PRINT( "quit", ("Closing named pipes") );
     
    /* Create connection to the handle named pipe handler to break the loop */
    if ((temp = CreateFile(szPipeName,
			   GENERIC_READ | GENERIC_WRITE,
			   0,
			   NULL,
			   OPEN_EXISTING,
			   0,
			   NULL )) != INVALID_HANDLE_VALUE)
    {
      WaitNamedPipe(szPipeName, 1000);
      DWORD dwMode = PIPE_READMODE_BYTE | PIPE_WAIT;
      SetNamedPipeHandleState(temp, &dwMode, NULL, NULL);
      CancelIo(temp);
      DisconnectNamedPipe(temp);
      CloseHandle(temp);
    }
  }
#endif
#ifdef HAVE_SYS_UN_H
  if (unix_sock != INVALID_SOCKET)
  {
    (void) shutdown(unix_sock,2);
    (void) closesocket(unix_sock);
    (void) unlink(mysql_unix_port);
    unix_sock= INVALID_SOCKET;
  }
#endif
  end_thr_alarm();			 // Don't allow alarms
  end_slave();

  /* First signal all threads that it's time to die */

  THD *tmp;
  (void) pthread_mutex_lock(&LOCK_thread_count); // For unlink from list

  I_List_iterator<THD> it(threads);
  while ((tmp=it++))
  {
    DBUG_PRINT("quit",("Informing thread %ld that it's time to die",
		       tmp->thread_id));
    tmp->killed=1;
    if (tmp->mysys_var)
    {
      tmp->mysys_var->abort=1;
      pthread_mutex_lock(&tmp->mysys_var->mutex);
      if (tmp->mysys_var->current_cond)
      {
	pthread_mutex_lock(tmp->mysys_var->current_mutex);
	pthread_cond_broadcast(tmp->mysys_var->current_cond);
	pthread_mutex_unlock(tmp->mysys_var->current_mutex);
      }
      pthread_mutex_unlock(&tmp->mysys_var->mutex);
    }
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count); // For unlink from list

  if (thread_count)
    sleep(1);					// Give threads time to die

  /* Force remaining threads to die by closing the connection to the client */

  (void) my_net_init(&net, (st_vio*) 0);
  for (;;)
  {
    DBUG_PRINT("quit",("Locking LOCK_thread_count"));
    (void) pthread_mutex_lock(&LOCK_thread_count); // For unlink from list
    if (!(tmp=threads.get()))
    {
      DBUG_PRINT("quit",("Unlocking LOCK_thread_count"));
      (void) pthread_mutex_unlock(&LOCK_thread_count);
      break;
    }
#ifndef __bsdi__				// Bug in BSDI kernel
    if ((net.vio=tmp->net.vio) != 0)
    {
      sql_print_error(ER(ER_FORCING_CLOSE),my_progname,
		      tmp->thread_id,tmp->user ? tmp->user : "");
      close_connection(&net,0,0);
    }
#endif
    DBUG_PRINT("quit",("Unlocking LOCK_thread_count"));
    (void) pthread_mutex_unlock(&LOCK_thread_count);
  }
  net_end(&net);
  /* All threads has now been aborted */
  DBUG_PRINT("quit",("Waiting for threads to die (count=%u)",thread_count));
  (void) pthread_mutex_lock(&LOCK_thread_count);
  while (thread_count)
  {
    (void) pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
    DBUG_PRINT("quit",("One thread died (count=%u)",thread_count));
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  mysql_log.close(1);
  mysql_slow_log.close(1);
  mysql_update_log.close(1);
  mysql_bin_log.close(1);
  DBUG_PRINT("quit",("close_connections thread"));
  DBUG_VOID_RETURN;
}

static void close_server_sock()
{
#ifdef HAVE_CLOSE_SERVER_SOCK
  DBUG_ENTER("close_server_sock");
  my_socket tmp_sock;
  tmp_sock=ip_sock;
  if (tmp_sock != INVALID_SOCKET)
  {
    ip_sock=INVALID_SOCKET;
    DBUG_PRINT("info",("closing TCP/IP socket"));
    VOID(shutdown(tmp_sock,2));
    VOID(closesocket(tmp_sock));
  }
  tmp_sock=unix_sock;
  if (tmp_sock != INVALID_SOCKET)
  {
    unix_sock=INVALID_SOCKET;
    DBUG_PRINT("info",("closing Unix socket"));
    VOID(shutdown(tmp_sock,2));
    VOID(closesocket(tmp_sock));
    VOID(unlink(mysql_unix_port));
  }
  DBUG_VOID_RETURN;
#endif
}

void kill_mysql(void)
{
  DBUG_ENTER("kill_mysql");

#ifdef SIGNALS_DONT_BREAK_READ
  close_server_sock(); /* force accept to wake up */
#endif  

#if defined(__WIN__)
#if !defined(EMBEDDED_LIBRARY)
  {
    if (!SetEvent(hEventShutdown))
    {
      DBUG_PRINT("error",("Got error: %ld from SetEvent",GetLastError()));
    }
    /*
      or:
      HANDLE hEvent=OpenEvent(0, FALSE, "MySqlShutdown");
      SetEvent(hEventShutdown);
      CloseHandle(hEvent);
    */
  }
#endif
#elif defined(OS2)
  pthread_cond_signal( &eventShutdown);		// post semaphore
#elif defined(HAVE_PTHREAD_KILL)
  if (pthread_kill(signal_thread,SIGTERM))	/* End everything nicely */
  {
    DBUG_PRINT("error",("Got error %d from pthread_kill",errno)); /* purecov: inspected */
  }
#elif !defined(SIGNALS_DONT_BREAK_READ)
  kill(current_pid,SIGTERM);
#endif
  DBUG_PRINT("quit",("After pthread_kill"));
  shutdown_in_progress=1;			// Safety if kill didn't work
#ifdef SIGNALS_DONT_BREAK_READ
  if (!abort_loop)
  {
    pthread_t tmp;
    abort_loop=1;
    if (pthread_create(&tmp,&connection_attrib, kill_server_thread,
			   (void*) 0))
      sql_print_error("Error: Can't create thread to kill server");
  }
#endif    
  DBUG_VOID_RETURN;
}


	/* Force server down. kill all connections and threads and exit */

#if defined(OS2)
extern "C" void kill_server(int sig_ptr)
#define RETURN_FROM_KILL_SERVER return
#elif !defined(__WIN__)
static void *kill_server(void *sig_ptr)
#define RETURN_FROM_KILL_SERVER return 0
#else
static void __cdecl kill_server(int sig_ptr)
#define RETURN_FROM_KILL_SERVER return
#endif
{
  int sig=(int) (long) sig_ptr;			// This is passed a int
  DBUG_ENTER("kill_server");

  // if there is a signal during the kill in progress, ignore the other
  if (kill_in_progress)				// Safety
    RETURN_FROM_KILL_SERVER;
  kill_in_progress=TRUE;
  abort_loop=1;					// This should be set
  signal(sig,SIG_IGN);
  if (sig == MYSQL_KILL_SIGNAL || sig == 0)
    sql_print_error(ER(ER_NORMAL_SHUTDOWN),my_progname);
  else
    sql_print_error(ER(ER_GOT_SIGNAL),my_progname,sig); /* purecov: inspected */

#if defined(USE_ONE_SIGNAL_HAND) && !defined(__WIN__) && !defined(OS2)
  SHUTDOWN_THD;
  my_thread_init();				// If this is a new thread
#endif
  close_connections();
  if (sig != MYSQL_KILL_SIGNAL && sig != 0)
    unireg_abort(1);				/* purecov: inspected */
  else
    unireg_end(0);
  pthread_exit(0);				/* purecov: deadcode */
  RETURN_FROM_KILL_SERVER;
}


#ifdef USE_ONE_SIGNAL_HAND
static pthread_handler_decl(kill_server_thread,arg __attribute__((unused)))
{
  SHUTDOWN_THD;
  my_thread_init();				// Initialize new thread
  kill_server(0);
  my_thread_end();				// Normally never reached
  return 0;
}
#endif

#if defined(__amiga__)
#undef sigset
#define sigset signal
#endif

static sig_handler print_signal_warning(int sig)
{
  if (global_system_variables.log_warnings)
    sql_print_error("Warning: Got signal %d from thread %d",
		    sig,my_thread_id());
#ifdef DONT_REMEMBER_SIGNAL
  sigset(sig,print_signal_warning);		/* int. thread system calls */
#endif
#if !defined(__WIN__) && !defined(OS2)
  if (sig == SIGALRM)
    alarm(2);					/* reschedule alarm */
#endif
}


void unireg_end(int signal_number __attribute__((unused)))
{
  clean_up();
  my_thread_end();
  pthread_exit(0);				// Exit is in main thread
}


void unireg_abort(int exit_code)
{
  DBUG_ENTER("unireg_abort");
  if (exit_code)
    sql_print_error("Aborting\n");
  clean_up(); /* purecov: inspected */
  DBUG_PRINT("quit",("done with cleanup in unireg_abort"));
  my_thread_end();
  exit(exit_code); /* purecov: inspected */
}


void clean_up(bool print_message)
{
  DBUG_PRINT("exit",("clean_up"));
  if (cleanup_done++)
    return; /* purecov: inspected */
  if (use_slave_mask)
    bitmap_free(&slave_error_mask);
  acl_free(1);
  grant_free();
  query_cache_destroy();
  table_cache_free();
  hostname_cache_free();
  item_user_lock_free();
  lex_free();				/* Free some memory */
  set_var_free();
#ifdef HAVE_DLOPEN
  if (!opt_noacl)
    udf_free();
#endif
  (void) ha_panic(HA_PANIC_CLOSE);	/* close all tables and logs */
  end_key_cache();
#ifdef USE_RAID
  end_raid();
#endif
#ifdef HAVE_OPENSSL
  my_free(opt_ssl_key,MYF(MY_ALLOW_ZERO_PTR));
  my_free(opt_ssl_cert,MYF(MY_ALLOW_ZERO_PTR));
  my_free(opt_ssl_ca,MYF(MY_ALLOW_ZERO_PTR));
  my_free(opt_ssl_capath,MYF(MY_ALLOW_ZERO_PTR));
  my_free(opt_ssl_cipher,MYF(MY_ALLOW_ZERO_PTR));
  my_free((gptr) ssl_acceptor_fd, MYF(MY_ALLOW_ZERO_PTR));
  opt_ssl_key=opt_ssl_cert=opt_ssl_ca=opt_ssl_capath=0;
#endif /* HAVE_OPENSSL */

  if (defaults_argv)
    free_defaults(defaults_argv);
  my_free(charsets_list, MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql_tmpdir,MYF(MY_ALLOW_ZERO_PTR));
  my_free(slave_load_tmpdir,MYF(MY_ALLOW_ZERO_PTR));
  x_free(opt_bin_logname);
  x_free(opt_relay_logname);
  bitmap_free(&temp_pool);
  free_max_user_conn();
  end_slave_list();

#if !defined(__WIN__) && !defined(EMBEDDED_LIBRARY)
  if (!opt_bootstrap)
    (void) my_delete(pidfile_name,MYF(0));	// This may not always exist
#endif
  if (print_message && errmesg)
    sql_print_error(ER(ER_SHUTDOWN_COMPLETE),my_progname);
  x_free((gptr) my_errmsg[ERRMAPP]);	/* Free messages */
  DBUG_PRINT("quit", ("Error messages freed"));
  /* Tell main we are ready */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  DBUG_PRINT("quit", ("got thread count lock"));
  ready_to_exit=1;
  /* do the broadcast inside the lock to ensure that my_end() is not called */
  (void) pthread_cond_broadcast(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_PRINT("quit", ("done with cleanup"));
} /* clean_up */



/****************************************************************************
** Init IP and UNIX socket
****************************************************************************/

static void set_ports()
{
  char	*env;
  if (!mysql_port && !opt_disable_networking)
  {					// Get port if not from commandline
    struct  servent *serv_ptr;
    mysql_port = MYSQL_PORT;
    if ((serv_ptr = getservbyname("mysql", "tcp")))
      mysql_port = ntohs((u_short) serv_ptr->s_port); /* purecov: inspected */
    if ((env = getenv("MYSQL_TCP_PORT")))
      mysql_port = (uint) atoi(env);		/* purecov: inspected */
  }
  if (!mysql_unix_port)
  {
#ifdef __WIN__
    mysql_unix_port = (char*) MYSQL_NAMEDPIPE;
#else
    mysql_unix_port = (char*) MYSQL_UNIX_ADDR;
#endif
    if ((env = getenv("MYSQL_UNIX_PORT")))
      mysql_unix_port = env;			/* purecov: inspected */
  }
}

/* Change to run as another user if started with --user */

static void set_user(const char *user)
{
#if !defined(__WIN__) && !defined(OS2)
    struct passwd *ent;

  // don't bother if we aren't superuser
  if (geteuid())
  {
    if (user)
      fprintf(stderr,
	      "Warning: One can only use the --user switch if running as root\n");
    return;
  }
  else if (!user)
  {
    if (!opt_bootstrap)
    {
      fprintf(stderr,"Fatal error: Please read \"Security\" section of the manual to find out how to run mysqld as root!\n");
      unireg_abort(1);
    }
    return;
  }
  if (!strcmp(user,"root"))
    return;				// Avoid problem with dynamic libraries

  uid_t uid;
  if (!(ent = getpwnam(user)))
  {
    // allow a numeric uid to be used
    const char *pos;
    for (pos=user; isdigit(*pos); pos++) ;
    if (*pos)					// Not numeric id
    {
      fprintf(stderr,"Fatal error: Can't change to run as user '%s' ;  Please check that the user exists!\n",user);
      unireg_abort(1);
    }
    uid=atoi(user);				// Use numberic uid
  }
  else
  {
#ifdef HAVE_INITGROUPS
    initgroups((char*) user,ent->pw_gid);
#endif
    if (setgid(ent->pw_gid) == -1)
    {
      sql_perror("setgid");
      unireg_abort(1);
    }
    uid=ent->pw_uid;
  }

  if (setuid(uid) == -1)
  {
    sql_perror("setuid");
    unireg_abort(1);
  }
#endif
}

/* Change root user if started with  --chroot */

static void set_root(const char *path)
{
#if !defined(__WIN__) && !defined(__EMX__) && !defined(OS2)
  if (chroot(path) == -1)
  {
    sql_perror("chroot");
    unireg_abort(1);
  }
#endif
}

static void server_init(void)
{
  struct sockaddr_in	IPaddr;
#ifdef HAVE_SYS_UN_H
  struct sockaddr_un	UNIXaddr;
#endif
  int	arg=1;
  DBUG_ENTER("server_init");

#ifdef	__WIN__
  if ( !opt_disable_networking )
  {
    WSADATA WsaData;
    if (SOCKET_ERROR == WSAStartup (0x0101, &WsaData))
    {
      my_message(0,"WSAStartup Failed\n",MYF(0));
      unireg_abort(1);
    }
  }
#endif /* __WIN__ */

  set_ports();

  if (mysql_port != 0 && !opt_disable_networking && !opt_bootstrap)
  {
    DBUG_PRINT("general",("IP Socket is %d",mysql_port));
    ip_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ip_sock == INVALID_SOCKET)
    {
      DBUG_PRINT("error",("Got error: %d from socket()",socket_errno));
      sql_perror(ER(ER_IPSOCK_ERROR));		/* purecov: tested */
      unireg_abort(1);				/* purecov: tested */
    }
    bzero((char*) &IPaddr, sizeof(IPaddr));
    IPaddr.sin_family = AF_INET;
    IPaddr.sin_addr.s_addr = my_bind_addr;
    IPaddr.sin_port = (unsigned short) htons((unsigned short) mysql_port);
    (void) setsockopt(ip_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&arg,sizeof(arg));
    if (bind(ip_sock, my_reinterpret_cast(struct sockaddr *) (&IPaddr),
	     sizeof(IPaddr)) < 0)
    {
      DBUG_PRINT("error",("Got error: %d from bind",socket_errno));
      sql_perror("Can't start server: Bind on TCP/IP port");
      sql_print_error("Do you already have another mysqld server running on port: %d ?",mysql_port);
      unireg_abort(1);
    }
    if (listen(ip_sock,(int) back_log) < 0)
    {
      sql_perror("Can't start server: listen() on TCP/IP port");
      sql_print_error("Error:  listen() on TCP/IP failed with error %d",
		      socket_errno);
      unireg_abort(1);
    }
  }
  set_user(mysqld_user);		// Works also with mysqld_user==NULL

#ifdef __NT__
  /* create named pipe */
  if (Service.IsNT() && mysql_unix_port[0] && !opt_bootstrap &&
      opt_enable_named_pipe)
  {
    sprintf( szPipeName, "\\\\.\\pipe\\%s", mysql_unix_port );
    ZeroMemory( &saPipeSecurity, sizeof(saPipeSecurity) );
    ZeroMemory( &sdPipeDescriptor, sizeof(sdPipeDescriptor) );
    if ( !InitializeSecurityDescriptor(&sdPipeDescriptor,
				       SECURITY_DESCRIPTOR_REVISION) )
    {
      sql_perror("Can't start server : Initialize security descriptor");
      unireg_abort(1);
    }
    if (!SetSecurityDescriptorDacl(&sdPipeDescriptor, TRUE, NULL, FALSE))
    {
      sql_perror("Can't start server : Set security descriptor");
      unireg_abort(1);
    }
    saPipeSecurity.nLength = sizeof( SECURITY_ATTRIBUTES );
    saPipeSecurity.lpSecurityDescriptor = &sdPipeDescriptor;
    saPipeSecurity.bInheritHandle = FALSE;
    if ((hPipe = CreateNamedPipe(szPipeName,
				 PIPE_ACCESS_DUPLEX,
				 PIPE_TYPE_BYTE |
				 PIPE_READMODE_BYTE |
				 PIPE_WAIT,
				 PIPE_UNLIMITED_INSTANCES,
				 (int) global_variables.net_buffer_length,
				 (int) global_variables.net_buffer_length,
				 NMPWAIT_USE_DEFAULT_WAIT,
				 &saPipeSecurity )) == INVALID_HANDLE_VALUE)
      {
	LPVOID lpMsgBuf;
	int error=GetLastError();
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		      FORMAT_MESSAGE_FROM_SYSTEM,
		      NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		      (LPTSTR) &lpMsgBuf, 0, NULL );
	MessageBox( NULL, (LPTSTR) lpMsgBuf, "Error from CreateNamedPipe",
		    MB_OK|MB_ICONINFORMATION );
	LocalFree( lpMsgBuf );
	unireg_abort(1);
      }
  }
#endif

#if defined(HAVE_SYS_UN_H)
  /*
  ** Create the UNIX socket
  */
  if (mysql_unix_port[0] && !opt_bootstrap)
  {
    DBUG_PRINT("general",("UNIX Socket is %s",mysql_unix_port));

    if ((unix_sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
      sql_perror("Can't start server : UNIX Socket "); /* purecov: inspected */
      unireg_abort(1);				/* purecov: inspected */
    }
    bzero((char*) &UNIXaddr, sizeof(UNIXaddr));
    UNIXaddr.sun_family = AF_UNIX;
    strmov(UNIXaddr.sun_path, mysql_unix_port);
    (void) unlink(mysql_unix_port);
    (void) setsockopt(unix_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&arg,
		      sizeof(arg));
    umask(0);
    if (bind(unix_sock, my_reinterpret_cast(struct sockaddr *) (&UNIXaddr),
	     sizeof(UNIXaddr)) < 0)
    {
      sql_perror("Can't start server : Bind on unix socket"); /* purecov: tested */
      sql_print_error("Do you already have another mysqld server running on socket: %s ?",mysql_unix_port);
      unireg_abort(1);					/* purecov: tested */
    }
    umask(((~my_umask) & 0666));
#if defined(S_IFSOCK) && defined(SECURE_SOCKETS)
    (void) chmod(mysql_unix_port,S_IFSOCK);	/* Fix solaris 2.6 bug */
#endif
    if (listen(unix_sock,(int) back_log) < 0)
      sql_print_error("Warning:  listen() on Unix socket failed with error %d",
		      socket_errno);
  }
#endif
  DBUG_PRINT("info",("server started"));
  DBUG_VOID_RETURN;
}


void yyerror(const char *s)
{
  NET *net=my_pthread_getspecific_ptr(NET*,THR_NET);
  char *yytext=(char*) current_lex->tok_start;
  if (!strcmp(s,"parse error"))
    s=ER(ER_SYNTAX_ERROR);
  net_printf(net,ER_PARSE_ERROR, s, yytext ? (char*) yytext : "",
	     current_lex->yylineno);
}


void close_connection(NET *net,uint errcode,bool lock)
{
  st_vio* vio;
  DBUG_ENTER("close_connection");
  DBUG_PRINT("enter",("fd: %s  error: '%s'",
                    net->vio? vio_description(net->vio):"(not connected)",
                    errcode ? ER(errcode) : ""));
  if (lock)
    (void) pthread_mutex_lock(&LOCK_thread_count);
  if ((vio=net->vio) != 0)
  {
    if (errcode)
      send_error(net,errcode,ER(errcode));	/* purecov: inspected */
    vio_close(vio);			/* vio is freed in delete thd */
  }
  if (lock)
    (void) pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_VOID_RETURN;
}

	/* Called when a thread is aborted */
	/* ARGSUSED */

sig_handler end_thread_signal(int sig __attribute__((unused)))
{
  THD *thd=current_thd;
  DBUG_ENTER("end_thread_signal");
  if (thd)
    end_thread(thd,0);
  DBUG_VOID_RETURN;				/* purecov: deadcode */
}


void end_thread(THD *thd, bool put_in_cache)
{
  DBUG_ENTER("end_thread");
  thd->cleanup();
  (void) pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  delete thd;

  if (put_in_cache && cached_thread_count < thread_cache_size &&
      ! abort_loop && !kill_cached_threads)
  {
    /* Don't kill the thread, just put it in cache for reuse */
    DBUG_PRINT("info", ("Adding thread to cache"))
    cached_thread_count++;
    while (!abort_loop && ! wake_thread && ! kill_cached_threads)
      (void) pthread_cond_wait(&COND_thread_cache, &LOCK_thread_count);
    cached_thread_count--;
    if (kill_cached_threads)
      pthread_cond_signal(&COND_flush_thread_cache);
    if (wake_thread)
    {
      wake_thread--;
      thd=thread_cache.get();
      thd->real_id=pthread_self();
      (void) thd->store_globals();
      threads.append(thd);
      pthread_mutex_unlock(&LOCK_thread_count);
      DBUG_VOID_RETURN;
    }
  }

  DBUG_PRINT("info", ("sending a broadcast"))

  /* Tell main we are ready */
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  /* It's safe to broadcast outside a lock (COND... is not deleted here) */
  (void) pthread_cond_broadcast(&COND_thread_count);
  DBUG_PRINT("info", ("unlocked thread_count mutex"))
#ifdef ONE_THREAD
  if (!(test_flags & TEST_NO_THREADS))	// For debugging under Linux
#endif
  {
    my_thread_end();
    pthread_exit(0);
  }
  DBUG_VOID_RETURN;
}


/* Start a cached thread. LOCK_thread_count is locked on entry */

static void start_cached_thread(THD *thd)
{
  thread_cache.append(thd);
  wake_thread++;
  thread_count++;
  pthread_cond_signal(&COND_thread_cache);
}


void flush_thread_cache()
{
  (void) pthread_mutex_lock(&LOCK_thread_count);
  kill_cached_threads++;
  while (cached_thread_count)
  {
    pthread_cond_broadcast(&COND_thread_cache);
    pthread_cond_wait(&COND_flush_thread_cache,&LOCK_thread_count);
  }
  kill_cached_threads--;
  (void) pthread_mutex_unlock(&LOCK_thread_count);
}


/*
  Aborts a thread nicely. Commes here on SIGPIPE
  TODO: One should have to fix that thr_alarm know about this
  thread too.
*/

#ifdef THREAD_SPECIFIC_SIGPIPE
static sig_handler abort_thread(int sig __attribute__((unused)))
{
  THD *thd=current_thd;
  DBUG_ENTER("abort_thread");
  if (thd)
    thd->killed=1;
  DBUG_VOID_RETURN;
}
#endif

/******************************************************************************
  Setup a signal thread with handles all signals.
  Because Linux doesn't support schemas use a mutex to check that
  the signal thread is ready before continuing
******************************************************************************/

#if defined(__WIN__) || defined(OS2)
static void init_signals(void)
{
  int signals[] = {SIGINT,SIGILL,SIGFPE,SIGSEGV,SIGTERM,SIGABRT } ;
  for (uint i=0 ; i < sizeof(signals)/sizeof(int) ; i++)
    signal( signals[i], kill_server) ;
#if defined(__WIN__)
  signal(SIGBREAK,SIG_IGN);	//ignore SIGBREAK for NT
#else
  signal(SIGBREAK, kill_server);
#endif
}

static void start_signal_handler(void)
{
}

#elif defined(__EMX__)
static void sig_reload(int signo)
{
 // Flush everything
  reload_acl_and_cache((THD*) 0,REFRESH_LOG, (TABLE_LIST*) 0);
  signal(signo, SIG_ACK);
}

static void sig_kill(int signo)
{
  if (!abort_loop)
  {
    abort_loop=1;				// mark abort for threads
    kill_server((void*) signo);
  }
  signal(signo, SIG_ACK);
}

static void init_signals(void)
{
  signal(SIGQUIT, sig_kill);
  signal(SIGKILL, sig_kill);
  signal(SIGTERM, sig_kill);
  signal(SIGINT,  sig_kill);
  signal(SIGHUP,  sig_reload);	// Flush everything
  signal(SIGALRM, SIG_IGN);
  signal(SIGBREAK,SIG_IGN);
  signal_thread = pthread_self();
  SIGNAL_THD;
}

static void start_signal_handler(void)
{
}

#else /* if ! __WIN__ && ! __EMX__ */

#ifdef HAVE_LINUXTHREADS
#define UNSAFE_DEFAULT_LINUX_THREADS 200
#endif

static sig_handler handle_segfault(int sig)
{
  THD *thd=current_thd;
  /*
    Strictly speaking, one needs a mutex here
    but since we have got SIGSEGV already, things are a mess
    so not having the mutex is not as bad as possibly using a buggy
    mutex - so we keep things simple
  */
  if (segfaulted)
  {
    fprintf(stderr, "Fatal signal %d while backtracing\n", sig);
    exit(1);
  }
  
  segfaulted = 1;
  fprintf(stderr,"\
mysqld got signal %d;\n\
This could be because you hit a bug. It is also possible that this binary\n\
or one of the libraries it was linked against is corrupt, improperly built,\n\
or misconfigured. This error can also be caused by malfunctioning hardware.\n",
	  sig);
  fprintf(stderr, "\
We will try our best to scrape up some info that will hopefully help diagnose\n\
the problem, but since we have already crashed, something is definitely wrong\n\
and this may fail.\n\n");
  fprintf(stderr, "key_buffer_size=%ld\n", keybuff_size);
  fprintf(stderr, "read_buffer_size=%ld\n", global_system_variables.read_buff_size);
  fprintf(stderr, "sort_buffer_size=%ld\n", thd->variables.sortbuff_size);
  fprintf(stderr, "max_used_connections=%ld\n", max_used_connections);
  fprintf(stderr, "max_connections=%ld\n", max_connections);
  fprintf(stderr, "threads_connected=%d\n", thread_count);
  fprintf(stderr, "It is possible that mysqld could use up to \n\
key_buffer_size + (read_buffer_size + sort_buffer_size)*max_connections = %ld K\n\
bytes of memory\n", (keybuff_size + (global_system_variables.read_buff_size +
				     thd->variables.sortbuff_size) *
		     max_connections)/ 1024);
  fprintf(stderr, "Hope that's ok; if not, decrease some variables in the equation.\n\n");
  
#if defined(HAVE_LINUXTHREADS)
  if (sizeof(char*) == 4 && thread_count > UNSAFE_DEFAULT_LINUX_THREADS)
  {
    fprintf(stderr, "\
You seem to be running 32-bit Linux and have %d concurrent connections.\n\
If you have not changed STACK_SIZE in LinuxThreads and built the binary \n\
yourself, LinuxThreads is quite likely to steal a part of the global heap for\n\
the thread stack. Please read http://www.mysql.com/doc/L/i/Linux.html\n\n",
	    thread_count);
  }
#endif /* HAVE_LINUXTHREADS */

#ifdef HAVE_STACKTRACE
  if (!(test_flags & TEST_NO_STACKTRACE))
  {
    fprintf(stderr,"thd=%p\n",thd);
    print_stacktrace(thd ? (gptr) thd->thread_stack : (gptr) 0,
		     thread_stack);
  }
  if (thd)
  {
    fprintf(stderr, "Trying to get some variables.\n\
Some pointers may be invalid and cause the dump to abort...\n");
    safe_print_str("thd->query", thd->query, 1024);
    fprintf(stderr, "thd->thread_id=%ld\n", thd->thread_id);
    fprintf(stderr, "\n\
Successfully dumped variables, if you ran with --log, take a look at the\n\
details of what thread %ld did to cause the crash.  In some cases of really\n\
bad corruption, the values shown above may be invalid.\n\n",
	  thd->thread_id);
  }
  fprintf(stderr, "\
The manual page at http://www.mysql.com/doc/C/r/Crashing.html contains\n\
information that should help you find out what is causing the crash.\n");
  fflush(stderr);
#endif /* HAVE_STACKTRACE */

 if (test_flags & TEST_CORE_ON_SIGNAL)
   write_core(sig);
 exit(1);
}

#ifndef SA_RESETHAND
#define SA_RESETHAND 0
#endif
#ifndef SA_NODEFER
#define SA_NODEFER 0
#endif

static void init_signals(void)
{
  sigset_t set;
  DBUG_ENTER("init_signals");

  sigset(THR_KILL_SIGNAL,end_thread_signal);
  sigset(THR_SERVER_ALARM,print_signal_warning); // Should never be called!

  if (!(test_flags & TEST_NO_STACKTRACE) || (test_flags & TEST_CORE_ON_SIGNAL))
  {
    struct sigaction sa;
    sa.sa_flags = SA_RESETHAND | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sigprocmask(SIG_SETMASK,&sa.sa_mask,NULL);

    init_stacktrace();
#if defined(__amiga__)
    sa.sa_handler=(void(*)())handle_segfault;
#else
    sa.sa_handler=handle_segfault;
#endif
    sigaction(SIGSEGV, &sa, NULL);
#ifdef SIGBUS
    sigaction(SIGBUS, &sa, NULL);
#endif
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
  }

#ifdef HAVE_GETRLIMIT
  if (test_flags & TEST_CORE_ON_SIGNAL)
  {
    /* Change limits so that we will get a core file */
    struct rlimit rl;
    rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &rl))
      sql_print_error("Warning: setrlimit could not change the size of core files to 'infinity';  We may not be able to generate a core file on signals");
  }
#endif
  (void) sigemptyset(&set);
#ifdef THREAD_SPECIFIC_SIGPIPE
  sigset(SIGPIPE,abort_thread);
  sigaddset(&set,SIGPIPE);
#else
  (void) signal(SIGPIPE,SIG_IGN);		// Can't know which thread
  sigaddset(&set,SIGPIPE);
#endif
  sigaddset(&set,SIGINT);
  sigaddset(&set,SIGQUIT);
  sigaddset(&set,SIGTERM);
  sigaddset(&set,SIGHUP);
  signal(SIGTERM,SIG_DFL);			// If it's blocked by parent
  signal(SIGHUP,SIG_DFL);			// If it's blocked by parent
#ifdef SIGTSTP
  sigaddset(&set,SIGTSTP);
#endif
  sigaddset(&set,THR_SERVER_ALARM);
  sigdelset(&set,THR_KILL_SIGNAL);		// May be SIGINT
  sigdelset(&set,THR_CLIENT_ALARM);		// For alarms
  (void) pthread_sigmask(SIG_SETMASK,&set,NULL);
  DBUG_VOID_RETURN;
}


static void start_signal_handler(void)
{
  int error;
  pthread_attr_t thr_attr;
  DBUG_ENTER("start_signal_handler");

  (void) pthread_attr_init(&thr_attr);
#if !defined(HAVE_DEC_3_2_THREADS)
  pthread_attr_setscope(&thr_attr,PTHREAD_SCOPE_SYSTEM);
  (void) pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED);
  if (!(opt_specialflag & SPECIAL_NO_PRIOR))
    my_pthread_attr_setprio(&thr_attr,INTERRUPT_PRIOR);
  pthread_attr_setstacksize(&thr_attr,32768);
#endif

  (void) pthread_mutex_lock(&LOCK_thread_count);
  if ((error=pthread_create(&signal_thread,&thr_attr,signal_hand,0)))
  {
    sql_print_error("Can't create interrupt-thread (error %d, errno: %d)",
		    error,errno);
    exit(1);
  }
  (void) pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
  pthread_mutex_unlock(&LOCK_thread_count);

  (void) pthread_attr_destroy(&thr_attr);
  DBUG_VOID_RETURN;
}


/* This threads handles all signals and alarms */

/* ARGSUSED */
static void *signal_hand(void *arg __attribute__((unused)))
{
  sigset_t set;
  int sig;
  my_thread_init();				// Init new thread
  DBUG_ENTER("signal_hand");
  SIGNAL_THD;
  /* Setup alarm handler */
  init_thr_alarm(max_connections+max_insert_delayed_threads);
#if SIGINT != THR_KILL_SIGNAL
  (void) sigemptyset(&set);			// Setup up SIGINT for debug
  (void) sigaddset(&set,SIGINT);		// For debugging
  (void) pthread_sigmask(SIG_UNBLOCK,&set,NULL);
#endif
  (void) sigemptyset(&set);			// Setup up SIGINT for debug
#ifdef USE_ONE_SIGNAL_HAND
  (void) sigaddset(&set,THR_SERVER_ALARM);	// For alarms
#endif
  (void) sigaddset(&set,SIGQUIT);
  (void) sigaddset(&set,SIGTERM);
#if THR_CLIENT_ALARM != SIGHUP
  (void) sigaddset(&set,SIGHUP);
#endif
  (void) sigaddset(&set,SIGTSTP);

  /* Save pid to this process (or thread on Linux) */
  if (!opt_bootstrap)
  {
    File pidFile;
    if ((pidFile = my_create(pidfile_name,0664, O_WRONLY, MYF(MY_WME))) >= 0)
    {
      char buff[21];
      sprintf(buff,"%lu",(ulong) getpid());
      (void) my_write(pidFile, buff,strlen(buff),MYF(MY_WME));
      (void) my_close(pidFile,MYF(0));
    }
  }
#ifdef HAVE_STACK_TRACE_ON_SEGV
  if (opt_do_pstack)
  {
    sprintf(pstack_file_name,"mysqld-%lu-%%d-%%d.backtrace", (ulong)getpid());
    pstack_install_segv_action(pstack_file_name);
  }
#endif /* HAVE_STACK_TRACE_ON_SEGV */

  // signal to start_signal_handler that we are ready
  (void) pthread_mutex_lock(&LOCK_thread_count);
  (void) pthread_cond_signal(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  for (;;)
  {
    int error;					// Used when debugging
    if (shutdown_in_progress && !abort_loop)
    {
      sig=SIGTERM;
      error=0;
    }
    else
      while ((error=my_sigwait(&set,&sig)) == EINTR) ;
    if (cleanup_done)
    {
      my_thread_end();
      pthread_exit(0);				// Safety
    }
    switch (sig) {
    case SIGTERM:
    case SIGQUIT:
    case SIGKILL:
#ifdef EXTRA_DEBUG
      sql_print_error("Got signal %d to shutdown mysqld",sig);
#endif
      DBUG_PRINT("info",("Got signal: %d  abort_loop: %d",sig,abort_loop));
      if (!abort_loop)
      {
	abort_loop=1;				// mark abort for threads
#ifdef USE_ONE_SIGNAL_HAND
	pthread_t tmp;
	if (!(opt_specialflag & SPECIAL_NO_PRIOR))
	  my_pthread_attr_setprio(&connection_attrib,INTERRUPT_PRIOR);
	if (pthread_create(&tmp,&connection_attrib, kill_server_thread,
			   (void*) sig))
	  sql_print_error("Error: Can't create thread to kill server");
#else
	kill_server((void*) sig);	// MIT THREAD has a alarm thread
#endif
      }
      break;
    case SIGHUP:
      reload_acl_and_cache((THD*) 0,
			   (REFRESH_LOG | REFRESH_TABLES | REFRESH_FAST |
			    REFRESH_STATUS | REFRESH_GRANT | REFRESH_THREADS |
			    REFRESH_HOSTS),
			   (TABLE_LIST*) 0); // Flush logs
      mysql_print_status((THD*) 0);		// Send debug some info
      break;
#ifdef USE_ONE_SIGNAL_HAND
    case THR_SERVER_ALARM:
      process_alarm(sig);			// Trigger alarms.
      break;
#endif
    default:
#ifdef EXTRA_DEBUG
      sql_print_error("Warning: Got signal: %d, error: %d",sig,error); /* purecov: tested */
#endif
      break;					/* purecov: tested */
    }
  }
  return(0);					/* purecov: deadcode */
}

#endif	/* __WIN__*/


/*
  All global error messages are sent here where the first one is stored for
  the client
*/


/* ARGSUSED */
static int my_message_sql(uint error, const char *str,
			  myf MyFlags __attribute__((unused)))
{
  NET *net;
  DBUG_ENTER("my_message_sql");
  DBUG_PRINT("error",("Message: '%s'",str));
  if ((net=my_pthread_getspecific_ptr(NET*,THR_NET)))
  {
    if (!net->last_error[0])			// Return only first message
    {
      strmake(net->last_error,str,sizeof(net->last_error)-1);
      net->last_errno=error ? error : ER_UNKNOWN_ERROR;
    }
  }
  else
    sql_print_error("%s: %s",my_progname,str); /* purecov: inspected */
  DBUG_RETURN(0);
}

#ifdef __WIN__

struct utsname
{
  char nodename[FN_REFLEN];
};

int uname(struct utsname *a)
{
  return -1;
}
#endif


#ifdef __WIN__
pthread_handler_decl(handle_shutdown,arg)
{
  MSG msg;
  SHUTDOWN_THD;
  my_thread_init();

  /* this call should create the message queue for this thread */
  PeekMessage(&msg, NULL, 1, 65534,PM_NOREMOVE);
#if !defined(EMBEDDED_LIBRARY)
  if (WaitForSingleObject(hEventShutdown,INFINITE)==WAIT_OBJECT_0)
#endif
     kill_server(MYSQL_KILL_SIGNAL);
  return 0;
}

int __stdcall handle_kill(ulong ctrl_type)
{
  if (ctrl_type == CTRL_CLOSE_EVENT ||
      ctrl_type == CTRL_SHUTDOWN_EVENT)
  {
    kill_server(MYSQL_KILL_SIGNAL);
    return TRUE;
  }
  return FALSE;
}
#endif

#ifdef OS2
pthread_handler_decl(handle_shutdown,arg)
{
  SHUTDOWN_THD;
  my_thread_init();

  // wait semaphore
  pthread_cond_wait( &eventShutdown, NULL);

  // close semaphore and kill server
  pthread_cond_destroy( &eventShutdown);

  /*
    Exit main loop on main thread, so kill will be done from
    main thread (this is thread 2)
  */
  abort_loop = 1;

  // unblock select()
  so_cancel(ip_sock);
  so_cancel(unix_sock);

  return 0;
}
#endif


const char *load_default_groups[]= { "mysqld","server",0 };

#ifdef HAVE_LIBWRAP
char *libwrapName=NULL;
#endif

bool open_log(MYSQL_LOG *log, const char *hostname,
	      const char *opt_name, const char *extension,
	      const char *index_file_name,
	      enum_log_type type, bool read_append,
	      bool no_auto_events)
{
  char tmp[FN_REFLEN];
  if (!opt_name || !opt_name[0])
  {
    /*
      TODO: The following should be using fn_format();  We just need to
      first change fn_format() to cut the file name if it's too long.
    */
    strmake(tmp,hostname,FN_REFLEN-5);
    strmov(strcend(tmp,'.'),extension);
    opt_name=tmp;
  }
  // get rid of extention if the log is binary to avoid problems
  if (type == LOG_BIN)
  {
    char *p = fn_ext(opt_name);
    if (p)
    {
      uint length=(uint) (p-opt_name);
      strmake(tmp,opt_name,min(length,FN_REFLEN));
      opt_name=tmp;
    }
  }
  return log->open(opt_name, type, 0, index_file_name,
		   (read_append) ? SEQ_READ_APPEND : WRITE_CACHE,
		   no_auto_events);
}



#ifdef __WIN__
int win_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
  DEBUGGER_OFF;

  my_umask=0660;		// Default umask for new files
  my_umask_dir=0700;		// Default umask for new directories
  MAIN_THD;
  /*
    Initialize signal_th and shutdown_th to main_th for default value
    as we need to initialize them to something safe. They are used
    when compiled with safemalloc.
  */
  SIGNAL_THD;
  SHUTDOWN_THD;
  MY_INIT(argv[0]);		// init my_sys library & pthreads
  tzset();			// Set tzname

  start_time=time((time_t*) 0);
#ifdef OS2
  {
    // fix timezone for daylight saving
    struct tm *ts = localtime(&start_time);
    if (ts->tm_isdst > 0)
      _timezone -= 3600;
  }
#endif
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
  strmake(pidfile_name, glob_hostname, sizeof(pidfile_name)-5);
  strmov(strcend(pidfile_name,'.'),".pid");	// Add extension
#ifndef DBUG_OFF
  strxmov(strend(server_version),MYSQL_SERVER_SUFFIX,"-debug",NullS);
#else
  strmov(strend(server_version),MYSQL_SERVER_SUFFIX);
#endif
#ifdef _CUSTOMSTARTUPCONFIG_
  if (_cust_check_startup())
  {
    /* _cust_check_startup will report startup failure error */
    exit( 1 );
  }
#endif
  load_defaults("my",load_default_groups,&argc,&argv);
  defaults_argv=argv;

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

  set_options();
  get_options(argc,argv);
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
  (void) pthread_mutex_init(&LOCK_server_id, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_user_conn, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_rpl_status, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_active_mi, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_global_system_variables, MY_MUTEX_INIT_FAST);
  (void) pthread_cond_init(&COND_thread_count,NULL);
  (void) pthread_cond_init(&COND_refresh,NULL);
  (void) pthread_cond_init(&COND_thread_cache,NULL);
  (void) pthread_cond_init(&COND_flush_thread_cache,NULL);
  (void) pthread_cond_init(&COND_manager,NULL);
  (void) pthread_cond_init(&COND_rpl_status, NULL);
  init_signals();

  if (set_default_charset_by_name(sys_charset.value, MYF(MY_WME)))
    exit(1);
  charsets_list = list_charsets(MYF(MY_COMPILED_SETS|MY_CONFIG_SETS));

#ifdef HAVE_OPENSSL
  if (opt_use_ssl)
  {
    /* having ssl_acceptor_fd != 0 signals the use of SSL */
    ssl_acceptor_fd= new_VioSSLAcceptorFd(opt_ssl_key, opt_ssl_cert,
					  opt_ssl_ca, opt_ssl_capath,
					  opt_ssl_cipher);
    DBUG_PRINT("info",("ssl_acceptor_fd: %p", ssl_acceptor_fd));
    if (!ssl_acceptor_fd)
      opt_use_ssl = 0;
  }
  if (des_key_file)
    load_des_key_file(des_key_file);
#endif /* HAVE_OPENSSL */

#ifdef HAVE_LIBWRAP
  libwrapName= my_progname+dirname_length(my_progname);
  openlog(libwrapName, LOG_PID, LOG_AUTH);
#endif

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

#if defined( SET_RLIMIT_NOFILE) || defined( OS2)
  /* connections and databases needs lots of files */
  {
    uint wanted_files=10+(uint) max(max_connections*5,
				    max_connections+table_cache_size*2);
    set_if_bigger(wanted_files, open_files_limit);
    // Note that some system returns 0 if we succeed here:
    uint files=set_maximum_open_files(wanted_files);
    if (files && files < wanted_files && ! open_files_limit)
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
  set_var_init();
  mysys_uses_curses=0;
#ifdef USE_REGEX
  regex_init();
#endif
  select_thread=pthread_self();
  select_thread_in_use=1;
  if (use_temp_pool && bitmap_init(&temp_pool,1024,1))
    unireg_abort(1);

  /*
    We have enough space for fiddling with the argv, continue
  */
  umask(((~my_umask) & 0666));
  if (my_setwd(mysql_real_data_home,MYF(MY_WME)))
  {
    unireg_abort(1);				/* purecov: inspected */
  }
  mysql_data_home[0]=FN_CURLIB;		// all paths are relative from here
  mysql_data_home[1]=0;
  server_init();
  table_cache_init();
  hostname_cache_init();
  query_cache_result_size_limit(query_cache_limit);
  query_cache_resize(query_cache_size);
  randominit(&sql_rand,(ulong) start_time,(ulong) start_time/2);
  reset_floating_point_exceptions();
  init_thr_lock();
  init_slave_list();

  /* Setup log files */
  if (opt_log)
    open_log(&mysql_log, glob_hostname, opt_logname, ".log", NullS,
	     LOG_NORMAL);
  if (opt_update_log)
  {
    open_log(&mysql_update_log, glob_hostname, opt_update_logname, "",
	     NullS, LOG_NEW);
    using_update_log=1;
  }
 
  if (opt_slow_log)
    open_log(&mysql_slow_log, glob_hostname, opt_slow_logname, "-slow.log",
	     NullS, LOG_NORMAL);
#ifdef __WIN__
#define MYSQL_ERR_FILE "mysql.err"
  if (!opt_console)
  {
    freopen(MYSQL_ERR_FILE,"a+",stdout);
    freopen(MYSQL_ERR_FILE,"a+",stderr);
  }
#endif
  if (ha_init())
  {
    sql_print_error("Can't init databases");
    exit(1);
  }
  ha_key_cache();
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
  if (locked_in_memory && !geteuid())
  {
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
  ft_init_stopwords(ft_precompiled_stopwords);

#ifdef __WIN__
  if (!opt_console)
    FreeConsole();				// Remove window
#endif

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
  start_signal_handler();				// Creates pidfile
  if (acl_init(opt_noacl))
  {
    select_thread_in_use=0;
    (void) pthread_kill(signal_thread,MYSQL_KILL_SIGNAL);
#ifndef __WIN__
    if (!opt_bootstrap)
      (void) my_delete(pidfile_name,MYF(MY_WME));	// Not needed anymore
#endif
    exit(1);
  }
  if (!opt_noacl)
    (void) grant_init();
  init_max_user_conn();

#ifdef HAVE_DLOPEN
  if (!opt_noacl)
    udf_init();
#endif
  /* init_slave() must be called after the thread keys are created */
  init_slave();
  
  if (opt_bin_log && !server_id)
  {
    server_id= !master_host ? 1 : 2;
    switch (server_id) {
#ifdef EXTRA_DEBUG
    case 1:
      sql_print_error("\
Warning: You have enabled the binary log, but you haven't set server-id:\n\
Updates will be logged to the binary log, but connections to slaves will\n\
not be accepted.");
      break;
#endif
    case 2:
      sql_print_error("\
Warning: You should set server-id to a non-0 value if master_host is set.\n\
The server will not act as a slave.");
      break;
    }
  }
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
	     opt_binlog_index_name,LOG_BIN);
    using_update_log=1;
  }


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
#if defined(__WIN__) && !defined(EMBEDDED_LIBRARY)	  //IRENA
  {
    hEventShutdown=CreateEvent(0, FALSE, FALSE, event_name);
    pthread_t hThread;
    if (pthread_create(&hThread,&connection_attrib,handle_shutdown,0))
      sql_print_error("Warning: Can't create thread to handle shutdown requests");

    // On "Stop Service" we have to do regular shutdown
    Service.SetShutdownEvent(hEventShutdown);
  }
#endif
#ifdef OS2
  {
    pthread_cond_init( &eventShutdown, NULL);
    pthread_t hThread;
    if (pthread_create(&hThread,&connection_attrib,handle_shutdown,0))
      sql_print_error("Warning: Can't create thread to handle shutdown requests");
  }
#endif

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

  printf(ER(ER_READY),my_progname,server_version,"");
  fflush(stdout);

#ifdef __NT__
  if (hPipe == INVALID_HANDLE_VALUE &&
      (!have_tcpip || opt_disable_networking))
  {
    sql_print_error("TCP/IP or --enable-named-pipe should be configured on NT OS");
	unireg_abort(1);
  }
  else
  {
    pthread_mutex_lock(&LOCK_thread_count);
    (void) pthread_cond_init(&COND_handler_count,NULL);
    {
      pthread_t hThread;
      handler_count=0;
      if (hPipe != INVALID_HANDLE_VALUE && opt_enable_named_pipe)
      {
	handler_count++;
	if (pthread_create(&hThread,&connection_attrib,
			   handle_connections_namedpipes, 0))
	{
	  sql_print_error("Warning: Can't create thread to handle named pipes");
	  handler_count--;
	}
      }
      if (have_tcpip && !opt_disable_networking)
      {
	handler_count++;
	if (pthread_create(&hThread,&connection_attrib,
			   handle_connections_sockets, 0))
	{
	  sql_print_error("Warning: Can't create thread to handle named pipes");
	  handler_count--;
	}
      }
      while (handler_count > 0)
      {
	pthread_cond_wait(&COND_handler_count,&LOCK_thread_count);
      }
    }
    pthread_mutex_unlock(&LOCK_thread_count);
  }
#else
  handle_connections_sockets(0);
#ifdef EXTRA_DEBUG2
  sql_print_error("Exiting main thread");
#endif
#endif /* __NT__ */

  /* (void) pthread_attr_destroy(&connection_attrib); */

  DBUG_PRINT("quit",("Exiting main thread"));

#ifndef __WIN__
#ifdef EXTRA_DEBUG2
  sql_print_error("Before Lock_thread_count");
#endif
  (void) pthread_mutex_lock(&LOCK_thread_count);
  DBUG_PRINT("quit", ("Got thread_count mutex"));
  select_thread_in_use=0;			// For close_connections
  (void) pthread_cond_broadcast(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);
#ifdef EXTRA_DEBUG2
  sql_print_error("After lock_thread_count");
#endif
#endif /* __WIN__ */

  /* Wait until cleanup is done */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  while (!ready_to_exit)
  {
    pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);

#if defined(__WIN__) && !defined(EMBEDDED_LIBRARY)
  if (Service.IsNT() && start_mode)
    Service.Stop();
  else
  {
    Service.SetShutdownEvent(0);
    if (hEventShutdown)
      CloseHandle(hEventShutdown);
  }
#endif
  my_end(opt_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
  exit(0);
  return(0);					/* purecov: deadcode */
}


/****************************************************************************
  Main and thread entry function for Win32
  (all this is needed only to run mysqld as a service on WinNT)
****************************************************************************/

#if defined(__WIN__) && !defined(EMBEDDED_LIBRARY)
int mysql_service(void *p)
{
  win_main(Service.my_argc, Service.my_argv);
  return 0;
}

int main(int argc, char **argv)
{
  if (Service.GetOS())	/* true NT family */
  {
    char file_path[FN_REFLEN];
    my_path(file_path, argv[0], "");		      /* Find name in path */
    fn_format(file_path,argv[0],file_path,"",1+4+16); /* Force full path */

    if (argc == 2)
    {	
      if (Service.got_service_option(argv, "install"))
      {
        Service.Install(1, MYSQL_SERVICENAME, MYSQL_SERVICENAME, file_path);
        return 0;
      }
      else if (Service.got_service_option(argv, "install-manual"))
      {
        Service.Install(0, MYSQL_SERVICENAME, MYSQL_SERVICENAME, file_path);
        return 0;
      }
      else if (Service.got_service_option(argv, "remove"))
      {
        Service.Remove(MYSQL_SERVICENAME);
        return 0;
      }
      else if (Service.IsService(argv[1]))
      {
        /* start an optional service */
        load_default_groups[0]= argv[1];
        event_name= argv[1];
        start_mode= 1;
        Service.Init(event_name, mysql_service );
        return 0;
      }
    }
    else if (argc == 3) /* install or remove any optional service */
    {
      uint length=strlen(file_path);
      file_path[sizeof(file_path)-1]=0;
      strxnmov(file_path + length, sizeof(file_path)-2, " ", argv[2], NullS);
      if (Service.got_service_option(argv, "install"))
      {
        Service.Install(1, argv[2], argv[2], file_path);
        return 0;
      }
      else if (Service.got_service_option(argv, "install-manual"))
      {
        Service.Install(0, argv[2], argv[2], file_path);
        return 0;
      }
      else if (Service.got_service_option(argv, "remove"))
      {
        Service.Remove(argv[2]);
        return 0;
      }
    }
    else if (argc == 1 && Service.IsService(MYSQL_SERVICENAME))
    {
      /* start the default service */
      start_mode= 1;
      event_name= "MySqlShutdown";
      Service.Init(MYSQL_SERVICENAME, mysql_service);
      return 0;
    }
  }
  /* Start as standalone server */
  Service.my_argc=argc;
  Service.my_argv=argv;
  mysql_service(NULL);
  return 0;
}
#endif


/*
  Execute all commands from a file. Used by the mysql_install_db script to
  create MySQL privilege tables without having to start a full MySQL server.
*/

static int bootstrap(FILE *file)
{
  THD *thd= new THD;
  int error;

  thd->bootstrap=1;
  thd->client_capabilities=0;
  my_net_init(&thd->net,(st_vio*) 0);
  thd->max_client_packet_length= thd->net.max_packet;
  thd->master_access= ~0;
  thd->thread_id=thread_id++;
  thread_count++;

  bootstrap_file=file;
  if (pthread_create(&thd->real_id,&connection_attrib,handle_bootstrap,
		     (void*) thd))
  {
    sql_print_error("Warning: Can't create thread to handle bootstrap");    
    return -1;
  }
  /* Wait for thread to die */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  while (thread_count)
  {
    (void) pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
    DBUG_PRINT("quit",("One thread died (count=%u)",thread_count));
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  error= thd->fatal_error;
  net_end(&thd->net);
  thd->cleanup();
  delete thd;
  return error;
}

static bool read_init_file(char *file_name)
{
  FILE *file;
  DBUG_ENTER("read_init_file");
  DBUG_PRINT("enter",("name: %s",file_name));
  if (!(file=my_fopen(file_name,O_RDONLY,MYF(MY_WME))))
    return(1);
  bootstrap(file);				/* Ignore errors from this */
  (void) my_fclose(file,MYF(MY_WME));
  return 0;
}


static void create_new_thread(THD *thd)
{
  DBUG_ENTER("create_new_thread");

  NET *net=&thd->net;				// For easy ref
  net->read_timeout = (uint) connect_timeout;
  if (protocol_version > 9)
    net->return_errno=1;

  /* don't allow too many connections */
  if (thread_count - delayed_insert_threads >= max_connections+1 || abort_loop)
  {
    DBUG_PRINT("error",("Too many connections"));
    close_connection(net,ER_CON_COUNT_ERROR);
    delete thd;
    DBUG_VOID_RETURN;
  }
  pthread_mutex_lock(&LOCK_thread_count);
  if (thread_count-delayed_insert_threads > max_used_connections)
    max_used_connections=thread_count-delayed_insert_threads;
  thd->thread_id=thread_id++;
  for (uint i=0; i < 8 ; i++)			// Generate password teststring
    thd->scramble[i]= (char) (rnd(&sql_rand)*94+33);
  thd->scramble[8]=0;
  thd->rand=sql_rand;
  thd->real_id=pthread_self();			// Keep purify happy

  /* Start a new thread to handle connection */
#ifdef ONE_THREAD
  if (test_flags & TEST_NO_THREADS)		// For debugging under Linux
  {
    thread_cache_size=0;			// Safety
    thread_count++;
    threads.append(thd);
    thd->real_id=pthread_self();
    (void) pthread_mutex_unlock(&LOCK_thread_count);
    handle_one_connection((void*) thd);
  }
  else
#endif
  {
    if (cached_thread_count > wake_thread)
    {
      start_cached_thread(thd);
    }
    else
    {
      int error;
      thread_count++;
      thread_created++;
      threads.append(thd);
      DBUG_PRINT("info",(("creating thread %d"), thd->thread_id));
      thd->connect_time = time(NULL);
      if ((error=pthread_create(&thd->real_id,&connection_attrib,
				handle_one_connection,
				(void*) thd)))
      {
	DBUG_PRINT("error",
		   ("Can't create thread to handle request (error %d)",
		    error));
	thread_count--;
	thd->killed=1;				// Safety
	(void) pthread_mutex_unlock(&LOCK_thread_count);
	net_printf(net,ER_CANT_CREATE_THREAD,error);
	(void) pthread_mutex_lock(&LOCK_thread_count);
	close_connection(net,0,0);
	delete thd;
	(void) pthread_mutex_unlock(&LOCK_thread_count);
	DBUG_VOID_RETURN;
      }
    }
    (void) pthread_mutex_unlock(&LOCK_thread_count);

  }
  DBUG_PRINT("info",("Thread created"));
  DBUG_VOID_RETURN;
}

#ifdef SIGNALS_DONT_BREAK_READ
inline void kill_broken_server()
{
  /* hack to get around signals ignored in syscalls for problem OS's */
  if (unix_sock == INVALID_SOCKET ||
      (!opt_disable_networking && ip_sock == INVALID_SOCKET))
  {
    select_thread_in_use = 0;
    kill_server((void*)MYSQL_KILL_SIGNAL); /* never returns */
  }
}
#define MAYBE_BROKEN_SYSCALL kill_broken_server();
#else
#define MAYBE_BROKEN_SYSCALL
#endif

	/* Handle new connections and spawn new process to handle them */

pthread_handler_decl(handle_connections_sockets,arg __attribute__((unused)))
{
  my_socket sock,new_sock;
  uint error_count=0;
  uint max_used_connection= (uint) (max(ip_sock,unix_sock)+1);
  fd_set readFDs,clientFDs;
  THD *thd;
  struct sockaddr_in cAddr;
  int ip_flags=0,socket_flags=0,flags;
  st_vio *vio_tmp;
  DBUG_ENTER("handle_connections_sockets");

  LINT_INIT(new_sock);

  (void) my_pthread_getprio(pthread_self());		// For debugging

  FD_ZERO(&clientFDs);
  if (ip_sock != INVALID_SOCKET)
  {
    FD_SET(ip_sock,&clientFDs);
#ifdef HAVE_FCNTL
    ip_flags = fcntl(ip_sock, F_GETFL, 0);
#endif
  }
#ifdef HAVE_SYS_UN_H
  FD_SET(unix_sock,&clientFDs);
#ifdef HAVE_FCNTL
  socket_flags=fcntl(unix_sock, F_GETFL, 0);
#endif
#endif

  DBUG_PRINT("general",("Waiting for connections."));
  MAYBE_BROKEN_SYSCALL;
  while (!abort_loop)
  {
    readFDs=clientFDs;
#ifdef HPUX
    if (select(max_used_connection,(int*) &readFDs,0,0,0) < 0)
      continue;
#else
    if (select((int) max_used_connection,&readFDs,0,0,0) < 0)
    {
      if (socket_errno != SOCKET_EINTR)
      {
	if (!select_errors++ && !abort_loop)	/* purecov: inspected */
	  sql_print_error("mysqld: Got error %d from select",socket_errno); /* purecov: inspected */
      }
      MAYBE_BROKEN_SYSCALL
      continue;
    }
#endif	/* HPUX */
    if (abort_loop)
    {
      MAYBE_BROKEN_SYSCALL;
      break;
    }

    /* Is this a new connection request ? */
#ifdef HAVE_SYS_UN_H
    if (FD_ISSET(unix_sock,&readFDs))
    {
      sock = unix_sock;
      flags= socket_flags;
    }
    else
#endif
    {
      sock = ip_sock;
      flags= ip_flags;
    }

#if !defined(NO_FCNTL_NONBLOCK)
    if (!(test_flags & TEST_BLOCKING))
    {
#if defined(O_NONBLOCK)
      fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#elif defined(O_NDELAY)
      fcntl(sock, F_SETFL, flags | O_NDELAY);
#endif
    }
#endif /* NO_FCNTL_NONBLOCK */
    for (uint retry=0; retry < MAX_ACCEPT_RETRY; retry++)
    {
      size_socket length=sizeof(struct sockaddr_in);
      new_sock = accept(sock, my_reinterpret_cast(struct sockaddr *) (&cAddr),
			&length);
      if (new_sock != INVALID_SOCKET ||
	  (socket_errno != SOCKET_EINTR && socket_errno != SOCKET_EAGAIN))
	break;
      MAYBE_BROKEN_SYSCALL;
#if !defined(NO_FCNTL_NONBLOCK)
      if (!(test_flags & TEST_BLOCKING))
      {
	if (retry == MAX_ACCEPT_RETRY - 1)
	  fcntl(sock, F_SETFL, flags);		// Try without O_NONBLOCK
      }
#endif
    }
#if !defined(NO_FCNTL_NONBLOCK)
    if (!(test_flags & TEST_BLOCKING))
      fcntl(sock, F_SETFL, flags);
#endif
    if (new_sock == INVALID_SOCKET)
    {
      if ((error_count++ & 255) == 0)		// This can happen often
	sql_perror("Error in accept");
      MAYBE_BROKEN_SYSCALL;
      if (socket_errno == SOCKET_ENFILE || socket_errno == SOCKET_EMFILE)
	sleep(1);				// Give other threads some time
      continue;
    }

#ifdef HAVE_LIBWRAP
    {
      if (sock == ip_sock)
      {
	struct request_info req;
	signal(SIGCHLD, SIG_DFL);
	request_init(&req, RQ_DAEMON, libwrapName, RQ_FILE, new_sock, NULL);
	my_fromhost(&req);
	if (!my_hosts_access(&req))
	{
	  /*
	    This may be stupid but refuse() includes an exit(0)
	    which we surely don't want...
	    clean_exit() - same stupid thing ...
	  */
	  syslog(deny_severity, "refused connect from %s",
		 my_eval_client(&req));

	  /*
	    C++ sucks (the gibberish in front just translates the supplied
	    sink function pointer in the req structure from a void (*sink)();
	    to a void(*sink)(int) if you omit the cast, the C++ compiler
	    will cry...
	  */
	  if (req.sink)
	    ((void (*)(int))req.sink)(req.fd);

	  (void) shutdown(new_sock,2);
	  (void) closesocket(new_sock);
	  continue;
	}
      }
    }
#endif /* HAVE_LIBWRAP */

    {
      size_socket dummyLen;
      struct sockaddr dummy;
      dummyLen = sizeof(struct sockaddr);
      if (getsockname(new_sock,&dummy, &dummyLen) < 0)
      {
	sql_perror("Error on new connection socket");
	(void) shutdown(new_sock,2);
	(void) closesocket(new_sock);
	continue;
      }
    }

    /*
    ** Don't allow too many connections
    */

    if (!(thd= new THD))
    {
      (void) shutdown(new_sock,2);
      VOID(closesocket(new_sock));
      continue;
    }
    if (!(vio_tmp=vio_new(new_sock,
			  sock == unix_sock ? VIO_TYPE_SOCKET :
			  VIO_TYPE_TCPIP,
			  sock == unix_sock)) ||
	my_net_init(&thd->net,vio_tmp))
    {
      if (vio_tmp)
	vio_delete(vio_tmp);
      else
      {
	(void) shutdown(new_sock,2);
	(void) closesocket(new_sock);
      }
      delete thd;
      continue;
    }
    if (sock == unix_sock)
      thd->host=(char*) localhost;
    create_new_thread(thd);
  }

#ifdef OS2
  // kill server must be invoked from thread 1!
  kill_server(MYSQL_KILL_SIGNAL);
#endif

#ifdef __NT__
  pthread_mutex_lock(&LOCK_thread_count);
  handler_count--;
  pthread_mutex_unlock(&LOCK_thread_count);
  pthread_cond_signal(&COND_handler_count);
#endif
  DBUG_RETURN(0);
}


#ifdef __NT__
pthread_handler_decl(handle_connections_namedpipes,arg)
{
  HANDLE hConnectedPipe;
  BOOL fConnected;
  THD *thd;
  my_thread_init();
  DBUG_ENTER("handle_connections_namedpipes");
  (void) my_pthread_getprio(pthread_self());		// For debugging

  DBUG_PRINT("general",("Waiting for named pipe connections."));
  while (!abort_loop)
  {
    /* wait for named pipe connection */
    fConnected = ConnectNamedPipe( hPipe, NULL );
    if (abort_loop)
      break;
    if (!fConnected)
      fConnected = GetLastError() == ERROR_PIPE_CONNECTED;
    if (!fConnected)
    {
      CloseHandle( hPipe );
      if ((hPipe = CreateNamedPipe(szPipeName,
				   PIPE_ACCESS_DUPLEX,
				   PIPE_TYPE_BYTE |
				   PIPE_READMODE_BYTE |
				   PIPE_WAIT,
				   PIPE_UNLIMITED_INSTANCES,
				   (int) global_variables.net_buffer_length,
				   (int) global_variables.net_buffer_length,
				   NMPWAIT_USE_DEFAULT_WAIT,
				   &saPipeSecurity )) ==
	  INVALID_HANDLE_VALUE )
      {
	sql_perror("Can't create new named pipe!");
	break;					// Abort
      }
    }
    hConnectedPipe = hPipe;
    /* create new pipe for new connection */
    if ((hPipe = CreateNamedPipe(szPipeName,
				 PIPE_ACCESS_DUPLEX,
				 PIPE_TYPE_BYTE |
				 PIPE_READMODE_BYTE |
				 PIPE_WAIT,
				 PIPE_UNLIMITED_INSTANCES,
				 (int) global_variables.net_buffer_length,
				 (int) global_variables.net_buffer_length,
				 NMPWAIT_USE_DEFAULT_WAIT,
				 &saPipeSecurity)) ==
	INVALID_HANDLE_VALUE)
    {
      sql_perror("Can't create new named pipe!");
      hPipe=hConnectedPipe;
      continue;					// We have to try again
    }

    if (!(thd = new THD))
    {
      DisconnectNamedPipe( hConnectedPipe );
      CloseHandle( hConnectedPipe );
      continue;
    }
    if (!(thd->net.vio = vio_new_win32pipe(hConnectedPipe)) ||
	my_net_init(&thd->net, thd->net.vio))
    {
      close_connection(&thd->net,ER_OUT_OF_RESOURCES);
      delete thd;
      continue;
    }
    /* host name is unknown */
    thd->host = my_strdup(localhost,MYF(0)); /* Host is unknown */
    create_new_thread(thd);
  }

  pthread_mutex_lock(&LOCK_thread_count);
  handler_count--;
  pthread_cond_signal(&COND_handler_count);
  pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_RETURN(0);
}
#endif /* __NT__ */


/******************************************************************************
** handle start options
******************************************************************************/

enum options {
  OPT_ISAM_LOG=256,            OPT_SKIP_NEW, 
  OPT_SKIP_GRANT,              OPT_SKIP_LOCK, 
  OPT_ENABLE_LOCK,             OPT_USE_LOCKING,
  OPT_SOCKET,                  OPT_UPDATE_LOG, 
  OPT_BIN_LOG,                 OPT_SKIP_RESOLVE, 
  OPT_SKIP_NETWORKING,         OPT_BIN_LOG_INDEX,
  OPT_BIND_ADDRESS,            OPT_PID_FILE,
  OPT_SKIP_PRIOR,              OPT_BIG_TABLES,    
  OPT_STANDALONE,              OPT_ONE_THREAD,
  OPT_CONSOLE,                 OPT_LOW_PRIORITY_UPDATES,
  OPT_SKIP_HOST_CACHE,         OPT_LONG_FORMAT,   
  OPT_FLUSH,                   OPT_SAFE, 
  OPT_BOOTSTRAP,               OPT_SKIP_SHOW_DB,
  OPT_TABLE_TYPE,              OPT_INIT_FILE,   
  OPT_DELAY_KEY_WRITE,         OPT_SLOW_QUERY_LOG, 
  OPT_USE_DELAY_KEY_WRITE,     OPT_CHARSETS_DIR,
  OPT_BDB_HOME,                OPT_BDB_LOG,  
  OPT_BDB_TMP,                 OPT_BDB_NOSYNC,
  OPT_BDB_LOCK,                OPT_BDB_SKIP, 
  OPT_BDB_NO_RECOVER,	    OPT_BDB_SHARED,
  OPT_MASTER_HOST,             OPT_MASTER_USER,
  OPT_MASTER_PASSWORD,         OPT_MASTER_PORT,
  OPT_MASTER_INFO_FILE,        OPT_MASTER_CONNECT_RETRY,
  OPT_MASTER_RETRY_COUNT,
  OPT_MASTER_SSL,             OPT_MASTER_SSL_KEY,
  OPT_MASTER_SSL_CERT,            
  OPT_SQL_BIN_UPDATE_SAME,     OPT_REPLICATE_DO_DB,      
  OPT_REPLICATE_IGNORE_DB,     OPT_LOG_SLAVE_UPDATES,
  OPT_BINLOG_DO_DB,            OPT_BINLOG_IGNORE_DB,
  OPT_WANT_CORE,               OPT_CONCURRENT_INSERT,
  OPT_MEMLOCK,                 OPT_MYISAM_RECOVER,
  OPT_REPLICATE_REWRITE_DB,    OPT_SERVER_ID, 
  OPT_SKIP_SLAVE_START,        OPT_SKIP_INNOBASE,
  OPT_SAFEMALLOC_MEM_LIMIT,    OPT_REPLICATE_DO_TABLE, 
  OPT_REPLICATE_IGNORE_TABLE,  OPT_REPLICATE_WILD_DO_TABLE, 
  OPT_REPLICATE_WILD_IGNORE_TABLE, 
  OPT_DISCONNECT_SLAVE_EVENT_COUNT, 
  OPT_ABORT_SLAVE_EVENT_COUNT,
  OPT_INNODB_DATA_HOME_DIR,
  OPT_INNODB_DATA_FILE_PATH,
  OPT_INNODB_LOG_GROUP_HOME_DIR, 
  OPT_INNODB_LOG_ARCH_DIR, 
  OPT_INNODB_LOG_ARCHIVE, 
  OPT_INNODB_FLUSH_LOG_AT_TRX_COMMIT, 
  OPT_INNODB_FLUSH_METHOD, 
  OPT_INNODB_FAST_SHUTDOWN, 
  OPT_SAFE_SHOW_DB,
  OPT_INNODB_SKIP, OPT_SKIP_SAFEMALLOC,
  OPT_TEMP_POOL, OPT_TX_ISOLATION,
  OPT_SKIP_STACK_TRACE, OPT_SKIP_SYMLINKS,
  OPT_MAX_BINLOG_DUMP_EVENTS, OPT_SPORADIC_BINLOG_DUMP_FAIL,
  OPT_SAFE_USER_CREATE, OPT_SQL_MODE,
  OPT_HAVE_NAMED_PIPE,
  OPT_DO_PSTACK, OPT_REPORT_HOST,
  OPT_REPORT_USER, OPT_REPORT_PASSWORD, OPT_REPORT_PORT,
  OPT_SHOW_SLAVE_AUTH_INFO, OPT_OLD_RPL_COMPAT,
  OPT_SLAVE_LOAD_TMPDIR, OPT_NO_MIX_TYPE,
  OPT_RPL_RECOVERY_RANK,OPT_INIT_RPL_ROLE,
  OPT_RELAY_LOG, OPT_RELAY_LOG_INDEX, OPT_RELAY_LOG_INFO_FILE,
  OPT_SLAVE_SKIP_ERRORS, OPT_DES_KEY_FILE, OPT_LOCAL_INFILE,
  OPT_RECKLESS_SLAVE,
  OPT_SSL_SSL, OPT_SSL_KEY, OPT_SSL_CERT, OPT_SSL_CA,
  OPT_SSL_CAPATH, OPT_SSL_CIPHER,
  OPT_BACK_LOG, OPT_BINLOG_CACHE_SIZE,
  OPT_CONNECT_TIMEOUT, OPT_DELAYED_INSERT_TIMEOUT,
  OPT_DELAYED_INSERT_LIMIT, OPT_DELAYED_QUEUE_SIZE,
  OPT_FLUSH_TIME, OPT_FT_MIN_WORD_LEN,
  OPT_FT_MAX_WORD_LEN, OPT_FT_MAX_WORD_LEN_FOR_SORT,
  OPT_INTERACTIVE_TIMEOUT, OPT_JOIN_BUFF_SIZE,
  OPT_KEY_BUFFER_SIZE, OPT_LONG_QUERY_TIME,
  OPT_LOWER_CASE_TABLE_NAMES, OPT_MAX_ALLOWED_PACKET,
  OPT_MAX_BINLOG_CACHE_SIZE, OPT_MAX_BINLOG_SIZE,
  OPT_MAX_CONNECTIONS, OPT_MAX_CONNECT_ERRORS,
  OPT_MAX_DELAYED_THREADS, OPT_MAX_HEP_TABLE_SIZE,
  OPT_MAX_JOIN_SIZE, OPT_MAX_SORT_LENGTH,
  OPT_MAX_TMP_TABLES, OPT_MAX_USER_CONNECTIONS,
  OPT_MAX_WRITE_LOCK_COUNT, OPT_BULK_INSERT_BUFFER_SIZE,
  OPT_MYISAM_BLOCK_SIZE, OPT_MYISAM_MAX_EXTRA_SORT_FILE_SIZE,
  OPT_MYISAM_MAX_SORT_FILE_SIZE, OPT_MYISAM_SORT_BUFFER_SIZE,
  OPT_NET_BUFFER_LENGTH, OPT_NET_RETRY_COUNT,
  OPT_NET_READ_TIMEOUT, OPT_NET_WRITE_TIMEOUT,
  OPT_OPEN_FILES_LIMIT, 
  OPT_QUERY_CACHE_LIMIT, OPT_QUERY_CACHE_SIZE,
  OPT_QUERY_CACHE_TYPE, OPT_RECORD_BUFFER,
  OPT_RECORD_RND_BUFFER, OPT_RELAY_LOG_SPACE_LIMIT,
  OPT_SLAVE_NET_TIMEOUT, OPT_SLAVE_COMPRESSED_PROTOCOL, OPT_SLOW_LAUNCH_TIME,
  OPT_SORT_BUFFER, OPT_TABLE_CACHE,
  OPT_THREAD_CONCURRENCY, OPT_THREAD_CACHE_SIZE,
  OPT_TMP_TABLE_SIZE, OPT_THREAD_STACK,
  OPT_WAIT_TIMEOUT,
  OPT_INNODB_MIRRORED_LOG_GROUPS,
  OPT_INNODB_LOG_FILES_IN_GROUP,
  OPT_INNODB_LOG_FILE_SIZE,
  OPT_INNODB_LOG_BUFFER_SIZE,
  OPT_INNODB_BUFFER_POOL_SIZE,
  OPT_INNODB_ADDITIONAL_MEM_POOL_SIZE,
  OPT_INNODB_FILE_IO_THREADS,
  OPT_INNODB_LOCK_WAIT_TIMEOUT,
  OPT_INNODB_THREAD_CONCURRENCY,
  OPT_INNODB_FORCE_RECOVERY,
  OPT_BDB_CACHE_SIZE,
  OPT_BDB_LOG_BUFFER_SIZE,
  OPT_BDB_MAX_LOCK
};


#define LONG_TIMEOUT ((ulong) 3600L*24L*365L)

struct my_option my_long_options[] =
{
  {"ansi", 'a', "Use ANSI SQL syntax instead of MySQL syntax", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"basedir", 'b',
   "Path to installation directory. All paths are usually resolved relative to this.",
   (gptr*) &mysql_home_ptr, (gptr*) &mysql_home_ptr, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
#ifdef HAVE_BERKELEY_DB
  {"bdb-home", OPT_BDB_HOME, "Berkeley home directory", (gptr*) &berkeley_home,
   (gptr*) &berkeley_home, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"bdb-lock-detect", OPT_BDB_LOCK,
   "Berkeley lock detect (DEFAULT, OLDEST, RANDOM or YOUNGEST, # sec)",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"bdb-logdir", OPT_BDB_LOG, "Berkeley DB log file directory",
   (gptr*) &berkeley_logdir, (gptr*) &berkeley_logdir, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"bdb-no-recover", OPT_BDB_NO_RECOVER,
   "Don't try to recover Berkeley DB tables on start", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"bdb-no-sync", OPT_BDB_NOSYNC, "Don't synchronously flush logs", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"bdb-shared-data", OPT_BDB_SHARED,
   "Start Berkeley DB in multi-process mode", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"bdb-tmpdir", OPT_BDB_TMP, "Berkeley DB tempfile name",
   (gptr*) &berkeley_tmpdir, (gptr*) &berkeley_tmpdir, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif /* HAVE_BERKELEY_DB */
  {"skip-bdb", OPT_BDB_SKIP, "Don't use berkeley db (will save memory)",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"big-tables", OPT_BIG_TABLES, 
   "Allow big result sets by saving all temporary sets on file (Solves most 'table full' errors)",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog-do-db", OPT_BINLOG_DO_DB,
   "Tells the master it should log updates for the specified database, and exclude all others not explicitly mentioned.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog-ignore-db", OPT_BINLOG_IGNORE_DB, 
   "Tells the master that updates to the given database should not be logged tothe binary log",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"bind-address", OPT_BIND_ADDRESS, "Ip address to bind to",
   (gptr*) &my_bind_addr, (gptr*) &my_bind_addr, 0, GET_STR, REQUIRED_ARG, 0,
   0, 0, 0, 0, 0},
  {"bootstrap", OPT_BOOTSTRAP, "Used by mysql installation scripts", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __WIN__
  {"console", OPT_CONSOLE, "Don't remove the console window",
   (gptr*) &opt_console, (gptr*) &opt_console, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"standalone", OPT_STANDALONE,
  "Dummy option to start as a standalone program (NT)", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"core-file", OPT_WANT_CORE, "Write core on errors", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"chroot", 'r', "Chroot mysqld daemon during startup.",
   (gptr*) &mysqld_chroot, (gptr*) &mysqld_chroot, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are", (gptr*) &charsets_dir,
   (gptr*) &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 'h', "Path to the database root", (gptr*) &mysql_data_home,
   (gptr*) &mysql_data_home, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
  {"debug", '#', "Debug log.", (gptr*) &default_dbug_option,
   (gptr*) &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef SAFEMALLOC
  {"skip-safemalloc", OPT_SKIP_SAFEMALLOC,
   "Don't use the memory allocation checking", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
#endif
#endif
#ifdef HAVE_OPENSSL
  {"des-key-file", OPT_DES_KEY_FILE, 
   "Load keys for des_encrypt() and des_encrypt from given file",
   (gptr*) &des_key_file, (gptr*) &des_key_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
#endif /* HAVE_OPENSSL */
  {"default-character-set", 'C', "Set the default character set",
   (gptr*) &sys_charset.value, (gptr*) &sys_charset.value, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"default-table-type", OPT_TABLE_TYPE,
   "Set the default table type for tables", 0, 0,
   0, GET_NO_ARG, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"delay-key-write-for-all-tables", OPT_DELAY_KEY_WRITE, 
   "Don't flush key buffers between writes for any MyISAM table", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"enable-locking", OPT_ENABLE_LOCK,
   "Depricated option, use --external-locking instead",
   (gptr*) &opt_external_locking, (gptr*) &opt_external_locking,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __NT__
  {"enable-named-pipe", OPT_HAVE_NAMED_PIPE, "Enable the named pipe (NT)",
   (gptr*) &opt_enable_named_pipe, (gptr*) &opt_enable_named_pipe, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"enable-pstack", OPT_DO_PSTACK, "Print a symbolic stack trace on failure",
   (gptr*) &opt_do_pstack, (gptr*) &opt_do_pstack, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"exit-info", 'T', "Used for debugging;  Use at your own risk!", 0, 0, 0,
   GET_LONG, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"flush", OPT_FLUSH, "Flush tables to disk between SQL commands", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  /* We must always support the next option to make scripts like mysqltest
     easier to do */
  {"init-rpl-role", OPT_INIT_RPL_ROLE, "Set the replication role", 0, 0, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_data_file_path", OPT_INNODB_DATA_FILE_PATH,
   "Path to individual files and their sizes",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_INNOBASE_DB
  {"innodb_data_home_dir", OPT_INNODB_DATA_HOME_DIR,
   "The common part for Innodb table spaces", (gptr*) &innobase_data_home_dir,
   (gptr*) &innobase_data_home_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0,
   0},
  {"innodb_log_group_home_dir", OPT_INNODB_LOG_GROUP_HOME_DIR,
   "Path to innodb log files.", (gptr*) &innobase_log_group_home_dir, 
   (gptr*) &innobase_log_group_home_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
   0, 0},
  {"innodb_log_arch_dir", OPT_INNODB_LOG_ARCH_DIR,
   "Where full logs should be archived", (gptr*) &innobase_log_arch_dir,
   (gptr*) &innobase_log_arch_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_log_archive", OPT_INNODB_LOG_ARCHIVE,
   "Set to 1 if you want to have logs archived", 0, 0, 0, GET_LONG, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"innodb_flush_log_at_trx_commit", OPT_INNODB_FLUSH_LOG_AT_TRX_COMMIT,
   "Set to 0 if you don't want to flush logs",
   (gptr*) &innobase_flush_log_at_trx_commit,
   (gptr*) &innobase_flush_log_at_trx_commit,
   0, GET_BOOL, OPT_ARG,  0, 0, 0, 0, 0, 0},
  {"innodb_flush_method", OPT_INNODB_FLUSH_METHOD,
   "With which method to flush data", (gptr*) &innobase_unix_file_flush_method,
   (gptr*) &innobase_unix_file_flush_method, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
  {"innodb_fast_shutdown", OPT_INNODB_FAST_SHUTDOWN,
   "Speeds up server shutdown process", (gptr*) &innobase_fast_shutdown,
   (gptr*) &innobase_fast_shutdown, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
#endif /* End HAVE_INNOBASE_DB */
  {"help", '?', "Display this help and exit", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"init-file", OPT_INIT_FILE, "Read SQL commands from this file at startup",
   (gptr*) &opt_init_file, (gptr*) &opt_init_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"log", 'l', "Log connections and queries to file", (gptr*) &opt_logname,
   (gptr*) &opt_logname, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"language", 'L',
   "Client error messages in given language. May be given as a full path",
   (gptr*) &language_ptr, (gptr*) &language_ptr, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"local-infile", OPT_LOCAL_INFILE,
   "Enable/disable LOAD DATA LOCAL INFILE (takes values 1|0)",
   (gptr*) &opt_local_infile,
   (gptr*) &opt_local_infile, 0, GET_BOOL, OPT_ARG,
   1, 0, 0, 0, 0, 0},
  {"log-bin", OPT_BIN_LOG,
   "Log queries in new binary format (for replication)",
   (gptr*) &opt_bin_logname, (gptr*) &opt_bin_logname, 0, GET_STR_ALLOC,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-bin-index", OPT_BIN_LOG_INDEX,
   "File that holds the names for last binary log files",
   (gptr*) &opt_binlog_index_name, (gptr*) &opt_binlog_index_name, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"log-isam", OPT_ISAM_LOG, "Log all MyISAM changes to file",
   (gptr*) &myisam_log_filename, (gptr*) &myisam_log_filename, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-update", OPT_UPDATE_LOG,
   "Log updates to file.# where # is a unique number if not given.",
   (gptr*) &opt_update_logname, (gptr*) &opt_update_logname, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-slow-queries", OPT_SLOW_QUERY_LOG,
   "Log slow queries to this log file. Defaults logging to hostname-slow.log",
   (gptr*) &opt_slow_logname, (gptr*) &opt_slow_logname, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"log-long-format", OPT_LONG_FORMAT,
   "Log some extra information to update log", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"log-slave-updates", OPT_LOG_SLAVE_UPDATES, 
   "Tells the slave to log the updates from the slave thread to the binary log. You will need to turn it on if you plan to daisy-chain the slaves.",
   (gptr*) &opt_log_slave_updates, (gptr*) &opt_log_slave_updates, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"low-priority-updates", OPT_LOW_PRIORITY_UPDATES,
   "INSERT/DELETE/UPDATE has lower priority than selects",
   (gptr*) &global_system_variables.low_priority_updates,
   (gptr*) &max_system_variables.low_priority_updates,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"master-host", OPT_MASTER_HOST, 
   "Master hostname or IP address for replication. If not set, the slave thread will not be started. Note that the setting of master-host will be ignored if there exists a valid master.info file.",
   (gptr*) &master_host, (gptr*) &master_host, 0, GET_STR, REQUIRED_ARG, 0, 0,
   0, 0, 0, 0},
  {"master-user", OPT_MASTER_USER,
   "The username the slave thread will use for authentication when connecting to the master. The user must have FILE privilege. If the master user is not set, user test is assumed. The value in master.info will take precedence if it can be read.",
   (gptr*) &master_user, (gptr*) &master_user, 0, GET_STR, REQUIRED_ARG, 0, 0,
   0, 0, 0, 0},
  {"master-password", OPT_MASTER_PASSWORD,
   "The password the slave thread will authenticate with when connecting to the master. If not set, an empty password is assumed.The value in master.info will take precedence if it can be read.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"master-port", OPT_MASTER_PORT,
   "The port the master is listening on. If not set, the compiled setting of MYSQL_PORT is assumed. If you have not tinkered with configure options, this should be 3306. The value in master.info will take precedence if it can be read",
   (gptr*) &master_port, (gptr*) &master_port, 0, GET_UINT, REQUIRED_ARG,
   MYSQL_PORT, 0, 0, 0, 0, 0},
  {"master-connect-retry", OPT_MASTER_CONNECT_RETRY,
   "The number of seconds the slave thread will sleep before retrying to connect to the master in case the master goes down or the connection is lost.",
   (gptr*) &master_connect_retry, (gptr*) &master_connect_retry, 0, GET_UINT,
   REQUIRED_ARG, 60, 0, 0, 0, 0, 0},
  {"master-retry-count", OPT_MASTER_RETRY_COUNT,
   "The number of tries the slave will make to connect to the master before giving up.",
   (gptr*) &master_retry_count, (gptr*) &master_retry_count, 0, GET_ULONG,
   REQUIRED_ARG, 3600*24, 0, 0, 0, 0, 0},
  {"master-info-file", OPT_MASTER_INFO_FILE,
   "The location of the file that remembers where we left off on the master during the replication process. The default is `master.info' in the data directory. You should not need to change this.",
   (gptr*) &master_info_file, (gptr*) &master_info_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"master-ssl", OPT_MASTER_SSL,
   "Turn SSL on for replication. Be warned that is this is a relatively new feature.",
   (gptr*) &master_ssl, (gptr*) &master_ssl, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"master-ssl-key", OPT_MASTER_SSL_KEY,
   "Master SSL keyfile name. Only applies if you have enabled master-ssl.",
   (gptr*) &master_ssl_key, (gptr*) &master_ssl_key, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"master-ssl-cert", OPT_MASTER_SSL_CERT,
   "Master SSL certificate file name. Only applies if you have enabled master-ssl.",
   (gptr*) &master_ssl_cert, (gptr*) &master_ssl_cert, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"myisam-recover", OPT_MYISAM_RECOVER,
   "Syntax: myisam-recover[=option[,option...]], where option can be DEFAULT, BACKUP or FORCE.",
   (gptr*) &myisam_recover_options_str, (gptr*) &myisam_recover_options_str, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  /*
    Option needs to be available for the test case to pass in non-debugging
    mode. is a no-op.
  */
  {"memlock", OPT_MEMLOCK, "Lock mysqld in memory", (gptr*) &locked_in_memory,
   (gptr*) &locked_in_memory, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"disconnect-slave-event-count", OPT_DISCONNECT_SLAVE_EVENT_COUNT,
   "Undocumented: Meant for debugging and testing of replication",
   (gptr*) &disconnect_slave_event_count,
   (gptr*) &disconnect_slave_event_count, 0, GET_INT, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
  {"abort-slave-event-count", OPT_ABORT_SLAVE_EVENT_COUNT,
   "Undocumented: Meant for debugging and testing of replication",
   (gptr*) &abort_slave_event_count,  (gptr*) &abort_slave_event_count,
   0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"max-binlog-dump-events", OPT_MAX_BINLOG_DUMP_EVENTS, "Undocumented",
   (gptr*) &max_binlog_dump_events, (gptr*) &max_binlog_dump_events, 0,
   GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"sporadic-binlog-dump-fail", OPT_SPORADIC_BINLOG_DUMP_FAIL, "Undocumented",
   (gptr*) &opt_sporadic_binlog_dump_fail,
   (gptr*) &opt_sporadic_binlog_dump_fail, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"safemalloc-mem-limit", OPT_SAFEMALLOC_MEM_LIMIT,
   "Simulate memory shortage when compiled with the --with-debug=full option",
   0, 0, 0, GET_ULL, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"new", 'n', "Use very new possible 'unsafe' functions", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef NOT_YET
  {"no-mix-table-types", OPT_NO_MIX_TYPE, "Don't allow commands with uses two different table types",
   (gptr*) &opt_no_mix_types, (gptr*) &opt_no_mix_types, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
#endif
  {"old-protocol", 'o', "Use the old (3.20) protocol",
   (gptr*) &protocol_version, (gptr*) &protocol_version, 0, GET_UINT, NO_ARG,
   PROTOCOL_VERSION, 0, 0, 0, 0, 0},
  {"old-rpl-compat", OPT_OLD_RPL_COMPAT,
   "Use old LOAD DATA format in the binary log (don't save data in file)",
   (gptr*) &opt_old_rpl_compat, (gptr*) &opt_old_rpl_compat, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef ONE_THREAD
  {"one-thread", OPT_ONE_THREAD,
   "Only use one thread (for debugging under Linux)", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"pid-file", OPT_PID_FILE, "Pid file used by safe_mysqld",
   (gptr*) &pidfile_name_ptr, (gptr*) &pidfile_name_ptr, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection.", (gptr*) &mysql_port,
   (gptr*) &mysql_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"reckless-slave", OPT_RECKLESS_SLAVE, "For debugging", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-do-db", OPT_REPLICATE_DO_DB,
   "Tells the slave thread to restrict replication to the specified database. To specify more than one database, use the directive multiple times, once for each database. Note that this will only work if you do not use cross-database queries such as UPDATE some_db.some_table SET foo='bar' while having selected a different or no database. If you need cross database updates to work, make sure you have 3.23.28 or later, and use replicate-wild-do-table=db_name.%.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-do-table", OPT_REPLICATE_DO_TABLE,
   "Tells the slave thread to restrict replication to the specified table. To specify more than one table, use the directive multiple times, once for each table. This will work for cross-database updates, in contrast to replicate-do-db.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-wild-do-table", OPT_REPLICATE_WILD_DO_TABLE,
   "Tells the slave thread to restrict replication to the tables that match the specified wildcard pattern. To specify more than one table, use the directive multiple times, once for each table. This will work for cross-database updates. Example: replicate-wild-do-table=foo%.bar% will replicate only updates to tables in all databases that start with foo and whose table names start with bar",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-ignore-db", OPT_REPLICATE_IGNORE_DB,
   "Tells the slave thread to not replicate to the specified database. To specify more than one database to ignore, use the directive multiple times, once for each database. This option will not work if you use cross database updates. If you need cross database updates to work, make sure you have 3.23.28 or later, and use replicate-wild-ignore-table=db_name.%. ",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-ignore-table", OPT_REPLICATE_IGNORE_TABLE,
   "Tells the slave thread to not replicate to the specified table. To specify more than one table to ignore, use the directive multiple times, once for each table. This will work for cross-datbase updates, in contrast to replicate-ignore-db.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-wild-ignore-table", OPT_REPLICATE_WILD_IGNORE_TABLE,
   "Tells the slave thread to not replicate to the tables that match the given wildcard pattern. To specify more than one table to ignore, use the directive multiple times, once for each table. This will work for cross-database updates. Example: replicate-wild-ignore-table=foo%.bar% will not do updates to tables in databases that start with foo and whose table names start with bar.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-rewrite-db", OPT_REPLICATE_REWRITE_DB,
   "Updates to a database with a different name than the original. Example: replicate-rewrite-db=master_db_name->slave_db_name",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  // In replication, we may need to tell the other servers how to connect
  {"report-host", OPT_REPORT_HOST,
   "Hostname or IP of the slave to be reported to to the master during slave registration. Will appear in the output of SHOW SLAVE HOSTS. Leave unset if you do not want the slave to register itself with the master. Note that it is not sufficient for the master to simply read the IP of the slave off the socket once the slave connects. Due to NAT and other routing issues, that IP may not be valid for connecting to the slave from the master or other hosts.",
   (gptr*) &report_host, (gptr*) &report_host, 0, GET_STR, REQUIRED_ARG, 0, 0,
   0, 0, 0, 0},
  {"report-user", OPT_REPORT_USER, "Undocumented", (gptr*) &report_user,
   (gptr*) &report_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"report-password", OPT_REPORT_PASSWORD, "Undocumented",
   (gptr*) &report_password, (gptr*) &report_password, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"report-port", OPT_REPORT_PORT,
   "Port for connecting to slave reported to the master during slave registration. Set it only if the slave is listening on a non-default port or if you have a special tunnel from the master or other clients to the slave. If not sure, leave this option unset.",
   (gptr*) &report_port, (gptr*) &report_port, 0, GET_UINT, REQUIRED_ARG,
   MYSQL_PORT, 0, 0, 0, 0, 0},
  {"rpl-recovery-rank", OPT_RPL_RECOVERY_RANK, "Undocumented",
   (gptr*) &rpl_recovery_rank, (gptr*) &rpl_recovery_rank, 0, GET_UINT,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"relay-log", OPT_RELAY_LOG, "Undocumented",
   (gptr*) &opt_relay_logname, (gptr*) &opt_relay_logname, 0,
   GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"relay-log-index", OPT_RELAY_LOG_INDEX, "Undocumented",
   (gptr*) &opt_relaylog_index_name, (gptr*) &opt_relaylog_index_name, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"safe-mode", OPT_SAFE, "Skip some optimize stages (for testing).",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef TO_BE_DELETED
  {"safe-show-database", OPT_SAFE_SHOW_DB,
   "Depricated option; One should use GRANT SHOW DATABASES instead...",
   (gptr*) &opt_safe_show_db, (gptr*) &opt_safe_show_db, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
#endif
  {"safe-user-create", OPT_SAFE_USER_CREATE,
   "Don't allow new user creation by the user who has no write privileges to the mysql.user table",
   (gptr*) &opt_safe_user_create, (gptr*) &opt_safe_user_create, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"server-id",	OPT_SERVER_ID,
   "Uniquely identifies the server instance in the community of replication partners",
   (gptr*) &server_id, (gptr*) &server_id, 0, GET_ULONG, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
  {"set-variable", 'O',
   "Change the value of a variable. Please note that this option is deprecated;you can set variables directly with --variable-name=value.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"show-slave-auth-info", OPT_SHOW_SLAVE_AUTH_INFO,
   "Show user and password in SHOW SLAVE STATUS",
   (gptr*) &opt_show_slave_auth_info, (gptr*) &opt_show_slave_auth_info, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"concurrent-insert", OPT_CONCURRENT_INSERT,
   "Use concurrent insert with MyISAM. Disable with prefix --skip-",
   (gptr*) &myisam_concurrent_insert, (gptr*) &myisam_concurrent_insert,
   0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"delay-key-write", OPT_USE_DELAY_KEY_WRITE,
   "Use delay_key_write option for all tables. Disable with prefix --skip-",
   (gptr*) &myisam_delay_key_write, (gptr*) &myisam_delay_key_write, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-grant-tables", OPT_SKIP_GRANT,
   "Start without grant tables. This gives all users FULL ACCESS to all tables!",
   (gptr*) &opt_noacl, (gptr*) &opt_noacl, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"skip-innodb", OPT_INNODB_SKIP, "Don't use Innodb (will save memory)",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-locking", OPT_SKIP_LOCK,
   "Depricated option, use --skip-external-locking instead",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-host-cache", OPT_SKIP_HOST_CACHE, "Don't cache host names", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-name-resolve", OPT_SKIP_RESOLVE,
   "Don't resolve hostnames. All hostnames are IP's or 'localhost'",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-networking", OPT_SKIP_NETWORKING,
   "Don't allow connection with TCP/IP.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"skip-new", OPT_SKIP_NEW, "Don't use new, possible wrong routines.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-show-database", OPT_SKIP_SHOW_DB,
   "Don't allow 'SHOW DATABASE' commands", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"skip-slave-start", OPT_SKIP_SLAVE_START,
   "If set, slave is not autostarted.", (gptr*) &opt_skip_slave_start,
   (gptr*) &opt_skip_slave_start, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-stack-trace", OPT_SKIP_STACK_TRACE,
   "Don't print a stack trace on failure", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"skip-symlink", OPT_SKIP_SYMLINKS, "Don't allow symlinking of tables",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-thread-priority", OPT_SKIP_PRIOR,
   "Don't give threads different priorities.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"relay-log-info-file", OPT_RELAY_LOG_INFO_FILE, "Undocumented",
   (gptr*) &relay_log_info_file, (gptr*) &relay_log_info_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"slave-load-tmpdir", OPT_SLAVE_LOAD_TMPDIR, "Undocumented",
   (gptr*) &slave_load_tmpdir, (gptr*) &slave_load_tmpdir, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"slave-skip-errors", OPT_SLAVE_SKIP_ERRORS,
   "Tells the slave thread to continue replication when a query returns an error from the provided list",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", OPT_SOCKET, "Socket file to use for connection",
   (gptr*) &mysql_unix_port, (gptr*) &mysql_unix_port, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"sql-bin-update-same", OPT_SQL_BIN_UPDATE_SAME,
   "If set, setting SQL_LOG_BIN to a value will automatically set SQL_LOG_UPDATE to the same value and vice versa.",
   (gptr*) &opt_sql_bin_update, (gptr*) &opt_sql_bin_update, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"sql-mode", OPT_SQL_MODE,
   "Syntax: sql-mode=option[,option[,option...]] where option can be one of: REAL_AS_FLOAT, PIPES_AS_CONCAT, ANSI_QUOTES, IGNORE_SPACE, SERIALIZE, ONLY_FULL_GROUP_BY, NO_UNSIGNED_SUBTRACTION.",
   (gptr*) &sql_mode_str, (gptr*) &sql_mode_str, 0, GET_STR, REQUIRED_ARG, 0,
   0, 0, 0, 0, 0},
#ifdef HAVE_OPENSSL
#include "sslopt-longopts.h"
#endif
  {"temp-pool", OPT_TEMP_POOL, 
   "Using this option will cause most temporary files created to use a small set of names, rather than a unique name for each new file.",
   (gptr*) &use_temp_pool, (gptr*) &use_temp_pool, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"tmpdir", 't', "Path for temporary files", (gptr*) &opt_mysql_tmpdir,
   (gptr*) &opt_mysql_tmpdir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"transaction-isolation", OPT_TX_ISOLATION,
   "Default transaction isolation level", 0, 0, 0, GET_NO_ARG, REQUIRED_ARG, 0,
   0, 0, 0,
   0, 0},
  {"external-locking", OPT_USE_LOCKING, "Use system (external) locking.  With this option enabled you can run myisamchk to test (not repair) tables while the MySQL server is running",
   (gptr*) &opt_external_locking, (gptr*) &opt_external_locking,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef USE_SYMDIR
  {"use-symbolic-links", 's', "Enable symbolic link support",
   (gptr*) &my_use_symdir, (gptr*) &my_use_symdir, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
#endif
  {"user", 'u', "Run mysqld daemon as user", (gptr*) &mysqld_user,
   (gptr*) &mysqld_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'v', "Synonym for option -v", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"log-warnings", 'W', "Log some not critical warnings to the log file",
   (gptr*) &global_system_variables.log_warnings,
   (gptr*) &max_system_variables.log_warnings, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"warnings", 'W', "Deprecated ; Use --log-warnings instead",
   (gptr*) &global_system_variables.log_warnings,
   (gptr*) &max_system_variables.log_warnings, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  { "back_log", OPT_BACK_LOG,
    "The number of outstanding connection requests MySQL can have. This comes into play when the main MySQL thread gets very many connection requests in a very short time.", (gptr*) &back_log, (gptr*) &back_log, 0, GET_ULONG,
    REQUIRED_ARG, 50, 1, 65535, 0, 1, 0 },
#ifdef HAVE_BERKELEY_DB
  { "bdb_cache_size", OPT_BDB_CACHE_SIZE,
    "The buffer that is allocated to cache index and rows for BDB tables.",
    (gptr*) &berkeley_cache_size, (gptr*) &berkeley_cache_size, 0, GET_ULONG,
    REQUIRED_ARG, KEY_CACHE_SIZE, 20*1024, (long) ~0, 0, IO_SIZE, 0},
  {"bdb_log_buffer_size", OPT_BDB_LOG_BUFFER_SIZE,
   "The buffer that is allocated to cache index and rows for BDB tables.",
   (gptr*) &berkeley_log_buffer_size, (gptr*) &berkeley_log_buffer_size, 0,
   GET_ULONG, REQUIRED_ARG, 0, 256*1024L, ~0L, 0, 1024, 0},
  {"bdb_max_lock", OPT_BDB_MAX_LOCK,
   "The maximum number of locks you can have active on a BDB table.",
   (gptr*) &berkeley_max_lock, (gptr*) &berkeley_max_lock, 0, GET_ULONG,
   REQUIRED_ARG, 10000, 0, (long) ~0, 0, 1, 0},
  /* QQ: The following should be removed soon! */
  {"bdb_lock_max", OPT_BDB_MAX_LOCK, "Synonym for bdb_max_lock",
   (gptr*) &berkeley_max_lock, (gptr*) &berkeley_max_lock, 0, GET_ULONG,
   REQUIRED_ARG, 10000, 0, (long) ~0, 0, 1, 0},
#endif /* HAVE_BERKELEY_DB */
  {"binlog_cache_size", OPT_BINLOG_CACHE_SIZE,
   "The size of the cache to hold the SQL statements for the binary log during a transaction. If you often use big, multi-statement transactions you can increase this to get more performance.",
   (gptr*) &binlog_cache_size, (gptr*) &binlog_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, 32*1024L, IO_SIZE, ~0L, 0, IO_SIZE, 0},
  {"connect_timeout", OPT_CONNECT_TIMEOUT, 
   "The number of seconds the mysqld server is waiting for a connect packet before responding with Bad handshake",
    (gptr*) &connect_timeout, (gptr*) &connect_timeout,
   0, GET_ULONG, REQUIRED_ARG, CONNECT_TIMEOUT, 2, LONG_TIMEOUT, 0, 1, 0 },
  {"delayed_insert_timeout", OPT_DELAYED_INSERT_TIMEOUT,
   "How long a INSERT DELAYED thread should wait for INSERT statements before terminating.",
   (gptr*) &delayed_insert_timeout, (gptr*) &delayed_insert_timeout, 0,
   GET_ULONG, REQUIRED_ARG, DELAYED_WAIT_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"delayed_insert_limit", OPT_DELAYED_INSERT_LIMIT,
   "After inserting delayed_insert_limit rows, the INSERT DELAYED handler will check if there are any SELECT statements pending. If so, it allows these to execute before continuing.",
    (gptr*) &delayed_insert_limit, (gptr*) &delayed_insert_limit, 0, GET_ULONG,
    REQUIRED_ARG, DELAYED_LIMIT, 1, ~0L, 0, 1, 0},
  { "delayed_queue_size", OPT_DELAYED_QUEUE_SIZE,
    "What size queue (in rows) should be allocated for handling INSERT DELAYED. If the queue becomes full, any client that does INSERT DELAYED will wait until there is room in the queue again.",
    (gptr*) &delayed_queue_size, (gptr*) &delayed_queue_size, 0, GET_ULONG,
    REQUIRED_ARG, DELAYED_QUEUE_SIZE, 1, ~0L, 0, 1, 0},
  { "flush_time", OPT_FLUSH_TIME,
    "A dedicated thread is created to flush all tables at the given interval.",
    (gptr*) &flush_time, (gptr*) &flush_time, 0, GET_ULONG, REQUIRED_ARG,
    FLUSH_TIME, 0, LONG_TIMEOUT, 0, 1, 0},
  { "ft_min_word_len", OPT_FT_MIN_WORD_LEN,
    "The minimum length of the word to be included in a FULLTEXT index. Note: FULLTEXT indexes must be rebuilt after changing this variable.",
    (gptr*) &ft_min_word_len, (gptr*) &ft_min_word_len, 0, GET_ULONG,
    REQUIRED_ARG, 4, 2, HA_FT_MAXLEN, 0, 1, 0},
  { "ft_max_word_len", OPT_FT_MAX_WORD_LEN,
    "The maximum length of the word to be included in a FULLTEXT index. Note: FULLTEXT indexes must be rebuilt after changing this variable.",
    (gptr*) &ft_max_word_len, (gptr*) &ft_max_word_len, 0, GET_ULONG,
    REQUIRED_ARG, HA_FT_MAXLEN, 10, HA_FT_MAXLEN, 0, 1, 0},
  { "ft_max_word_len_for_sort", OPT_FT_MAX_WORD_LEN_FOR_SORT,
    "Undocumented", (gptr*) &ft_max_word_len_for_sort,
    (gptr*) &ft_max_word_len_for_sort, 0, GET_ULONG, REQUIRED_ARG, 20, 4,
    HA_FT_MAXLEN, 0, 1, 0},
#ifdef HAVE_INNOBASE_DB
  {"innodb_mirrored_log_groups", OPT_INNODB_MIRRORED_LOG_GROUPS,
   "Number of identical copies of log groups we keep for the database. Currently this should be set to 1.", 
   (gptr*) &innobase_mirrored_log_groups,
   (gptr*) &innobase_mirrored_log_groups, 0, GET_LONG, REQUIRED_ARG, 1, 1, 10,
   0, 1, 0},
  {"innodb_log_files_in_group", OPT_INNODB_LOG_FILES_IN_GROUP,
   "Number of log files in the log group. InnoDB writes to the files in a circular fashion. Value 3 is recommended here.",
   (gptr*) &innobase_log_files_in_group, (gptr*) &innobase_log_files_in_group,
   0, GET_LONG, REQUIRED_ARG, 2, 2, 100, 0, 1, 0},
  {"innodb_log_file_size", OPT_INNODB_LOG_FILE_SIZE,
   "Size of each log file in a log group in megabytes.",
   (gptr*) &innobase_log_file_size, (gptr*) &innobase_log_file_size, 0,
   GET_LONG, REQUIRED_ARG, 5*1024*1024L, 1*1024*1024L, ~0L, 0, 1024*1024L, 0},
  {"innodb_log_buffer_size", OPT_INNODB_LOG_BUFFER_SIZE,
   "The size of the buffer which InnoDB uses to write log to the log files on disk.",
   (gptr*) &innobase_log_buffer_size, (gptr*) &innobase_log_buffer_size, 0,
   GET_LONG, REQUIRED_ARG, 1024*1024L, 256*1024L, ~0L, 0, 1024, 0},
  {"innodb_buffer_pool_size", OPT_INNODB_BUFFER_POOL_SIZE,
   "The size of the memory buffer InnoDB uses to cache data and indexes of its tables.",
   (gptr*) &innobase_buffer_pool_size, (gptr*) &innobase_buffer_pool_size, 0,
   GET_LONG, REQUIRED_ARG, 8*1024*1024L, 1024*1024L, ~0L, 0, 1024*1024L, 0},
  {"innodb_additional_mem_pool_size", OPT_INNODB_ADDITIONAL_MEM_POOL_SIZE,
   "Size of a memory pool InnoDB uses to store data dictionary information and other internal data structures.",
   (gptr*) &innobase_additional_mem_pool_size,
   (gptr*) &innobase_additional_mem_pool_size, 0, GET_LONG, REQUIRED_ARG,
   1*1024*1024L, 512*1024L, ~0L, 0, 1024, 0},
  {"innodb_file_io_threads", OPT_INNODB_FILE_IO_THREADS,
   "Number of file I/O threads in InnoDB.", (gptr*) &innobase_file_io_threads,
   (gptr*) &innobase_file_io_threads, 0, GET_LONG, REQUIRED_ARG, 4, 4, 64, 0,
   1, 0},
  {"innodb_lock_wait_timeout", OPT_INNODB_LOCK_WAIT_TIMEOUT,
   "Timeout in seconds an InnoDB transaction may wait for a lock before being rolled back.",
   (gptr*) &innobase_lock_wait_timeout, (gptr*) &innobase_lock_wait_timeout,
   0, GET_LONG, REQUIRED_ARG, 50, 1, 1024 * 1024 * 1024, 0, 1, 0},
  {"innodb_thread_concurrency", OPT_INNODB_THREAD_CONCURRENCY,
   "Helps in performance tuning in heavily concurrent environments.",
   (gptr*) &innobase_thread_concurrency, (gptr*) &innobase_thread_concurrency,
   0, GET_LONG, REQUIRED_ARG, 8, 1, 1000, 0, 1, 0},
  {"innodb_force_recovery", OPT_INNODB_FORCE_RECOVERY,
   "Helps to save your data in case the disk image of the database becomes corrupt.",
   (gptr*) &innobase_force_recovery, (gptr*) &innobase_force_recovery, 0,
   GET_LONG, REQUIRED_ARG, 0, 0, 6, 0, 1, 0},
#endif /* HAVE_INNOBASE_DB */
  {"interactive_timeout", OPT_INTERACTIVE_TIMEOUT,
   "The number of seconds the server waits for activity on an interactive connection before closing it.",
   (gptr*) &global_system_variables.net_interactive_timeout,
   (gptr*) &max_system_variables.net_interactive_timeout, 0,
   GET_ULONG, REQUIRED_ARG, NET_WAIT_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"join_buffer_size", OPT_JOIN_BUFF_SIZE,
   "The size of the buffer that is used for full joins.",
   (gptr*) &global_system_variables.join_buff_size,
   (gptr*) &max_system_variables.join_buff_size, 0, GET_ULONG,
   REQUIRED_ARG, 128*1024L, IO_SIZE*2+MALLOC_OVERHEAD, ~0L, MALLOC_OVERHEAD,
   IO_SIZE, 0},
  {"key_buffer_size", OPT_KEY_BUFFER_SIZE,
   "The size of the buffer used for index blocks. Increase this to get better index handling (for all reads and multiple writes) to as much as you can afford; 64M on a 256M machine that mainly runs MySQL is quite common.",
   (gptr*) &keybuff_size, (gptr*) &keybuff_size, 0, GET_ULONG, REQUIRED_ARG,
   KEY_CACHE_SIZE, MALLOC_OVERHEAD, (long) ~0, MALLOC_OVERHEAD, IO_SIZE, 0},
  {"long_query_time", OPT_LONG_QUERY_TIME,
   "Log all queries that have taken more than long_query_time seconds to execute to file.",
   (gptr*) &global_system_variables.long_query_time,
   (gptr*) &max_system_variables.long_query_time, 0, GET_ULONG,
   REQUIRED_ARG, 10, 1, LONG_TIMEOUT, 0, 1, 0},
  {"lower_case_table_names", OPT_LOWER_CASE_TABLE_NAMES,
   "If set to 1 table names are stored in lowercase on disk and table names will be case-insensitive.",
   (gptr*) &lower_case_table_names,
   (gptr*) &lower_case_table_names, 0,
   GET_BOOL, NO_ARG, IF_WIN(1,0), 0, 1, 0, 1, 0},
  {"max_allowed_packet", OPT_MAX_ALLOWED_PACKET,
   "Max packetlength to send/receive from to server.",
   (gptr*) &global_system_variables.max_allowed_packet,
   (gptr*) &max_system_variables.max_allowed_packet, 0, GET_ULONG,
   REQUIRED_ARG, 1024*1024L, 80, 64*1024*1024L, MALLOC_OVERHEAD, 1024, 0},
  {"max_binlog_cache_size", OPT_MAX_BINLOG_CACHE_SIZE,
   "Can be used to restrict the total size used to cache a multi-transaction query.",
   (gptr*) &max_binlog_cache_size, (gptr*) &max_binlog_cache_size, 0,
   GET_ULONG, REQUIRED_ARG, ~0L, IO_SIZE, ~0L, 0, IO_SIZE, 0},
  {"max_binlog_size", OPT_MAX_BINLOG_SIZE,
   "Binary log will be rotated automatically when the size crosses the limit.",
   (gptr*) &max_binlog_size, (gptr*) &max_binlog_size, 0, GET_ULONG,
   REQUIRED_ARG, 1024*1024L*1024L, 1024, 1024*1024L*1024L, 0, 1, 0},
  {"max_connections", OPT_MAX_CONNECTIONS,
   "The number of simultaneous clients allowed.", (gptr*) &max_connections,
   (gptr*) &max_connections, 0, GET_ULONG, REQUIRED_ARG, 100, 1, 16384, 0, 1,
   0},
  {"max_connect_errors", OPT_MAX_CONNECT_ERRORS,
   "If there is more than this number of interrupted connections from a host this host will be blocked from further connections.",
   (gptr*) &max_connect_errors, (gptr*) &max_connect_errors, 0, GET_ULONG,
    REQUIRED_ARG, MAX_CONNECT_ERRORS, 1, ~0L, 0, 1, 0},
  {"max_delayed_threads", OPT_MAX_DELAYED_THREADS,
   "Don't start more than this number of threads to handle INSERT DELAYED statements.",
   (gptr*) &max_insert_delayed_threads, (gptr*) &max_insert_delayed_threads,
   0, GET_ULONG, REQUIRED_ARG, 20, 1, 16384, 0, 1, 0},
  {"max_heap_table_size", OPT_MAX_HEP_TABLE_SIZE,
   "Don't allow creation of heap tables bigger than this.",
   (gptr*) &global_system_variables.max_heap_table_size,
   (gptr*) &max_system_variables.max_heap_table_size, 0, GET_ULONG,
   REQUIRED_ARG, 16*1024*1024L, 16384, ~0L, MALLOC_OVERHEAD, 1024, 0},
  {"max_join_size", OPT_MAX_JOIN_SIZE,
   "Joins that are probably going to read more than max_join_size records return an error.",
   (gptr*) &global_system_variables.max_join_size,
   (gptr*) &max_system_variables.max_join_size, 0, GET_ULONG, REQUIRED_ARG,
   ~0L, 1, ~0L, 0, 1, 0},
  {"max_sort_length", OPT_MAX_SORT_LENGTH,
   "The number of bytes to use when sorting BLOB or TEXT values (only the first max_sort_length bytes of each value are used; the rest are ignored).",
   (gptr*) &global_system_variables.max_sort_length,
   (gptr*) &max_system_variables.max_sort_length, 0, GET_ULONG,
   REQUIRED_ARG, 1024, 4, 8192*1024L, 0, 1, 0},
  {"max_tmp_tables", OPT_MAX_TMP_TABLES,
   "Maximum number of temporary tables a client can keep open at a time.",
   (gptr*) &global_system_variables.max_tmp_tables,
   (gptr*) &max_system_variables.max_tmp_tables, 0, GET_ULONG,
   REQUIRED_ARG, 32, 1, ~0L, 0, 1, 0},
  {"max_user_connections", OPT_MAX_USER_CONNECTIONS,
   "The maximum number of active connections for a single user (0 = no limit).",
   (gptr*) &max_user_connections, (gptr*) &max_user_connections, 0, GET_ULONG,
   REQUIRED_ARG, 0, 1, ~0L, 0, 1, 0},
  {"max_write_lock_count", OPT_MAX_WRITE_LOCK_COUNT,
   "After this many write locks, allow some read locks to run in between.",
   (gptr*) &max_write_lock_count, (gptr*) &max_write_lock_count, 0, GET_ULONG,
   REQUIRED_ARG, ~0L, 1, ~0L, 0, 1, 0},
  {"bulk_insert_buffer_size", OPT_BULK_INSERT_BUFFER_SIZE,
   "Size of tree cache used in bulk insert optimisation. Note that this is a limit per thread!",
   (gptr*) &global_system_variables.bulk_insert_buff_size,
   (gptr*) &max_system_variables.bulk_insert_buff_size,
   0, GET_ULONG, REQUIRED_ARG, 8192*1024, 0, ~0L, 0, 1, 0},
  {"myisam_block_size", OPT_MYISAM_BLOCK_SIZE,
   "Block size to be used for MyISAM index pages",
   (gptr*) &opt_myisam_block_size,
   (gptr*) &opt_myisam_block_size, 0, GET_ULONG, REQUIRED_ARG,
   MI_KEY_BLOCK_LENGTH, MI_MIN_KEY_BLOCK_LENGTH, MI_MAX_KEY_BLOCK_LENGTH,
   0, MI_MIN_KEY_BLOCK_LENGTH, 0},
  {"myisam_max_extra_sort_file_size", OPT_MYISAM_MAX_EXTRA_SORT_FILE_SIZE,
   "Used to help MySQL to decide when to use the slow but safe key cache index create method",
   (gptr*) &global_system_variables.myisam_max_extra_sort_file_size,
   (gptr*) &max_system_variables.myisam_max_extra_sort_file_size,
   0, GET_ULL, REQUIRED_ARG, (ulonglong) MI_MAX_TEMP_LENGTH,
   0, ~0L, 0, 1, 0},
  {"myisam_max_sort_file_size", OPT_MYISAM_MAX_SORT_FILE_SIZE,
   "Don't use the fast sort index method to created index if the temporary file would get bigger than this!",
   (gptr*) &global_system_variables.myisam_max_sort_file_size,
   (gptr*) &max_system_variables.myisam_max_sort_file_size, 0,
   GET_ULL, REQUIRED_ARG, (longlong) LONG_MAX, 0, ~0L, 0, 1024*1024, 0},
  {"myisam_sort_buffer_size", OPT_MYISAM_SORT_BUFFER_SIZE,
   "The buffer that is allocated when sorting the index when doing a REPAIR or when creating indexes with CREATE INDEX or ALTER TABLE.",
   (gptr*) &global_system_variables.myisam_sort_buff_size,
   (gptr*) &max_system_variables.myisam_sort_buff_size, 0,
   GET_ULONG, REQUIRED_ARG, 8192*1024, 4, ~0L, 0, 1, 0},
  {"net_buffer_length", OPT_NET_BUFFER_LENGTH,
   "Buffer length for TCP/IP and socket communication.", 
   (gptr*) &global_system_variables.net_buffer_length,
   (gptr*) &max_system_variables.net_buffer_length, 0, GET_ULONG,
   REQUIRED_ARG, 16384, 1024, 1024*1024L, 0, 1024, 0},
  {"net_retry_count", OPT_NET_RETRY_COUNT,
   "If a read on a communication port is interrupted, retry this many times before giving up.",
   (gptr*) &mysqld_net_retry_count, (gptr*) &mysqld_net_retry_count, 0,
   GET_ULONG, REQUIRED_ARG, MYSQLD_NET_RETRY_COUNT, 1, ~0L, 0, 1, 0},
  {"net_read_timeout", OPT_NET_READ_TIMEOUT,
   "Number of seconds to wait for more data from a connection before aborting the read.",
   (gptr*) &global_system_variables.net_read_timeout,
   (gptr*) &max_system_variables.net_read_timeout, 0, GET_ULONG,
   REQUIRED_ARG, NET_READ_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"net_write_timeout", OPT_NET_WRITE_TIMEOUT,
   "Number of seconds to wait for a block to be written to a connection  before aborting the write.",
   (gptr*) &global_system_variables.net_write_timeout,
   (gptr*) &max_system_variables.net_write_timeout, 0, GET_ULONG,
   REQUIRED_ARG, NET_WRITE_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"open_files_limit", OPT_OPEN_FILES_LIMIT,
   "If this is not 0, then mysqld will use this value to reserve file descriptors to use with setrlimit(). If this value is 0 then mysqld will reserve max_connections*5 or max_connections + table_cache*2 (whichever is larger) number of files.",
   (gptr*) &open_files_limit, (gptr*) &open_files_limit, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, 65535, 0, 1, 0},
#ifdef HAVE_QUERY_CACHE
  {"query_cache_limit", OPT_QUERY_CACHE_LIMIT,
   "Don't cache results that are bigger than this.",
   (gptr*) &query_cache_limit, (gptr*) &query_cache_limit, 0, GET_ULONG,
   REQUIRED_ARG, 1024*1024L, 0, (longlong) ULONG_MAX, 0, 1, 0},
#endif /*HAVE_QUERY_CACHE*/
  {"query_cache_size", OPT_QUERY_CACHE_SIZE,
   "The memory allocated to store results from old queries.",
   (gptr*) &query_cache_size, (gptr*) &query_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, (longlong) ULONG_MAX, 0, 1024, 0},
#ifdef HAVE_QUERY_CACHE
  {"query_cache_type", OPT_QUERY_CACHE_TYPE,
   "0 = OFF = Don't cache or retrieve results. 1 = ON = Cache all results except SELECT SQL_NO_CACHE ... queries. 2 = DEMAND = Cache only SELECT SQL_CACHE ... queries.",
   (gptr*) &global_system_variables.query_cache_type,
   (gptr*) &max_system_variables.query_cache_type,
   0, GET_ULONG, REQUIRED_ARG, 1, 0, 2, 0, 1, 0},
#endif /*HAVE_QUERY_CACHE*/
  {"read_buffer_size", OPT_RECORD_BUFFER,
   "Each thread that does a sequential scan allocates a buffer of this size for each table it scans. If you do many sequential scans, you may want to increase this value.",
   (gptr*) &global_system_variables.read_buff_size,
   (gptr*) &max_system_variables.read_buff_size,0, GET_ULONG, REQUIRED_ARG,
   128*1024L, IO_SIZE*2+MALLOC_OVERHEAD, ~0L, MALLOC_OVERHEAD, IO_SIZE, 0},
  {"read_rnd_buffer_size", OPT_RECORD_RND_BUFFER,
   "When reading rows in sorted order after a sort, the rows are read through this buffer to avoid a disk seeks. If not set, then it's set to the value of record_buffer.",
   (gptr*) &global_system_variables.read_rnd_buff_size,
   (gptr*) &max_system_variables.read_rnd_buff_size, 0,
   GET_ULONG, REQUIRED_ARG, 256*1024L, IO_SIZE*2+MALLOC_OVERHEAD,
   ~0L, MALLOC_OVERHEAD, IO_SIZE, 0},
  {"record_buffer", OPT_RECORD_BUFFER,
   "Alias for read_buffer_size",
   (gptr*) &global_system_variables.read_buff_size,
   (gptr*) &max_system_variables.read_buff_size,0, GET_ULONG, REQUIRED_ARG,
   128*1024L, IO_SIZE*2+MALLOC_OVERHEAD, ~0L, MALLOC_OVERHEAD, IO_SIZE, 0},
  {"relay_log_space_limit", OPT_RELAY_LOG_SPACE_LIMIT,
   "Undocumented", (gptr*) &relay_log_space_limit,
   (gptr*) &relay_log_space_limit, 0, GET_ULONG, REQUIRED_ARG, 0L, 0L,
   (longlong) ULONG_MAX, 0, 1, 0},
  {"slave_compressed_protocol", OPT_SLAVE_COMPRESSED_PROTOCOL,
   "Use compression on master/slave protocol",
   (gptr*) &opt_slave_compressed_protocol,
   (gptr*) &opt_slave_compressed_protocol,
   0, GET_BOOL, REQUIRED_ARG, 0, 0, 1, 0, 1, 0},
  {"slave_net_timeout", OPT_SLAVE_NET_TIMEOUT,
   "Number of seconds to wait for more data from a master/slave connection before aborting the read.",
   (gptr*) &slave_net_timeout, (gptr*) &slave_net_timeout, 0,
   GET_ULONG, REQUIRED_ARG, SLAVE_NET_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"slow_launch_time", OPT_SLOW_LAUNCH_TIME,
   "If creating the thread takes longer than this value (in seconds), the Slow_launch_threads counter will be incremented.",
   (gptr*) &slow_launch_time, (gptr*) &slow_launch_time, 0, GET_ULONG,
   REQUIRED_ARG, 2L, 0L, LONG_TIMEOUT, 0, 1, 0},
  {"sort_buffer_size", OPT_SORT_BUFFER,
   "Each thread that needs to do a sort allocates a buffer of this size.",
   (gptr*) &global_system_variables.sortbuff_size,
   (gptr*) &max_system_variables.sortbuff_size, 0, GET_ULONG, REQUIRED_ARG,
   MAX_SORT_MEMORY, MIN_SORT_MEMORY+MALLOC_OVERHEAD*2, ~0L, MALLOC_OVERHEAD,
   1, 0},
  {"table_cache", OPT_TABLE_CACHE,
   "The number of open tables for all threads.", (gptr*) &table_cache_size,
   (gptr*) &table_cache_size, 0, GET_ULONG, REQUIRED_ARG, 64, 1, 16384, 0, 1,
   0},
  {"thread_concurrency", OPT_THREAD_CONCURRENCY,
   "Permits the application to give the threads system a hint for the desired number of threads that should be run at the same time.",
   (gptr*) &concurrency, (gptr*) &concurrency, 0, GET_ULONG, REQUIRED_ARG,
   DEFAULT_CONCURRENCY, 1, 512, 0, 1, 0},
  {"thread_cache_size", OPT_THREAD_CACHE_SIZE,
   "How many threads we should keep in a cache for reuse.",
   (gptr*) &thread_cache_size, (gptr*) &thread_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, 16384, 0, 1, 0},
  {"tmp_table_size", OPT_TMP_TABLE_SIZE,
   "If an in-memory temporary table exceeds this size, MySQL will automatically convert it to an on-disk MyISAM table.",
   (gptr*) &global_system_variables.tmp_table_size,
   (gptr*) &max_system_variables.tmp_table_size, 0, GET_ULONG,
   REQUIRED_ARG, 32*1024*1024L, 1024, ~0L, 0, 1, 0},
  {"thread_stack", OPT_THREAD_STACK,
   "The stack size for each thread.", (gptr*) &thread_stack,
   (gptr*) &thread_stack, 0, GET_ULONG, REQUIRED_ARG,DEFAULT_THREAD_STACK,
   1024*32, ~0L, 0, 1024, 0},
  {"wait_timeout", OPT_WAIT_TIMEOUT,
   "The number of seconds the server waits for activity on a connection before closing it",
   (gptr*) &global_system_variables.net_wait_timeout,
   (gptr*) &max_system_variables.net_wait_timeout, 0, GET_ULONG,
   REQUIRED_ARG, NET_WAIT_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


struct show_var_st status_vars[]= {
  {"Aborted_clients",          (char*) &aborted_threads,        SHOW_LONG},
  {"Aborted_connects",         (char*) &aborted_connects,       SHOW_LONG},
  {"Bytes_received",           (char*) &bytes_received,         SHOW_LONG},
  {"Bytes_sent",               (char*) &bytes_sent,             SHOW_LONG},
  {"Com_admin_commands",       (char*) &com_other,		SHOW_LONG},
  {"Com_alter_table",	       (char*) (com_stat+(uint) SQLCOM_ALTER_TABLE),SHOW_LONG},
  {"Com_analyze",	       (char*) (com_stat+(uint) SQLCOM_ANALYZE),SHOW_LONG},
  {"Com_backup_table",	       (char*) (com_stat+(uint) SQLCOM_BACKUP_TABLE),SHOW_LONG},
  {"Com_begin",		       (char*) (com_stat+(uint) SQLCOM_BEGIN),SHOW_LONG},
  {"Com_change_db",	       (char*) (com_stat+(uint) SQLCOM_CHANGE_DB),SHOW_LONG},
  {"Com_change_master",	       (char*) (com_stat+(uint) SQLCOM_CHANGE_MASTER),SHOW_LONG},
  {"Com_check",		       (char*) (com_stat+(uint) SQLCOM_CHECK),SHOW_LONG},
  {"Com_commit",	       (char*) (com_stat+(uint) SQLCOM_COMMIT),SHOW_LONG},
  {"Com_create_db",	       (char*) (com_stat+(uint) SQLCOM_CREATE_DB),SHOW_LONG},
  {"Com_create_function",      (char*) (com_stat+(uint) SQLCOM_CREATE_FUNCTION),SHOW_LONG},
  {"Com_create_index",	       (char*) (com_stat+(uint) SQLCOM_CREATE_INDEX),SHOW_LONG},
  {"Com_create_table",	       (char*) (com_stat+(uint) SQLCOM_CREATE_TABLE),SHOW_LONG},
  {"Com_delete",	       (char*) (com_stat+(uint) SQLCOM_DELETE),SHOW_LONG},
  {"Com_delete_multi",	       (char*) (com_stat+(uint) SQLCOM_DELETE_MULTI),SHOW_LONG},
  {"Com_drop_db",	       (char*) (com_stat+(uint) SQLCOM_DROP_DB),SHOW_LONG},
  {"Com_drop_function",	       (char*) (com_stat+(uint) SQLCOM_DROP_FUNCTION),SHOW_LONG},
  {"Com_drop_index",	       (char*) (com_stat+(uint) SQLCOM_DROP_INDEX),SHOW_LONG},
  {"Com_drop_table",	       (char*) (com_stat+(uint) SQLCOM_DROP_TABLE),SHOW_LONG},
  {"Com_flush",		       (char*) (com_stat+(uint) SQLCOM_FLUSH),SHOW_LONG},
  {"Com_grant",		       (char*) (com_stat+(uint) SQLCOM_GRANT),SHOW_LONG},
  {"Com_ha_close",	       (char*) (com_stat+(uint) SQLCOM_HA_CLOSE),SHOW_LONG},
  {"Com_ha_open",	       (char*) (com_stat+(uint) SQLCOM_HA_OPEN),SHOW_LONG},
  {"Com_ha_read",	       (char*) (com_stat+(uint) SQLCOM_HA_READ),SHOW_LONG},
  {"Com_insert",	       (char*) (com_stat+(uint) SQLCOM_INSERT),SHOW_LONG},
  {"Com_insert_select",	       (char*) (com_stat+(uint) SQLCOM_INSERT_SELECT),SHOW_LONG},
  {"Com_kill",		       (char*) (com_stat+(uint) SQLCOM_KILL),SHOW_LONG},
  {"Com_load",		       (char*) (com_stat+(uint) SQLCOM_LOAD),SHOW_LONG},
  {"Com_load_master_data",    (char*) (com_stat+(uint) SQLCOM_LOAD_MASTER_DATA),SHOW_LONG},
  {"Com_load_master_table",    (char*) (com_stat+(uint) SQLCOM_LOAD_MASTER_TABLE),SHOW_LONG},
  {"Com_lock_tables",	       (char*) (com_stat+(uint) SQLCOM_LOCK_TABLES),SHOW_LONG},
  {"Com_optimize",	       (char*) (com_stat+(uint) SQLCOM_OPTIMIZE),SHOW_LONG},
  {"Com_purge",		       (char*) (com_stat+(uint) SQLCOM_PURGE),SHOW_LONG},
  {"Com_rename_table",	       (char*) (com_stat+(uint) SQLCOM_RENAME_TABLE),SHOW_LONG},
  {"Com_repair",	       (char*) (com_stat+(uint) SQLCOM_REPAIR),SHOW_LONG},
  {"Com_replace",	       (char*) (com_stat+(uint) SQLCOM_REPLACE),SHOW_LONG},
  {"Com_replace_select",       (char*) (com_stat+(uint) SQLCOM_REPLACE_SELECT),SHOW_LONG},
  {"Com_reset",		       (char*) (com_stat+(uint) SQLCOM_RESET),SHOW_LONG},
  {"Com_restore_table",	       (char*) (com_stat+(uint) SQLCOM_RESTORE_TABLE),SHOW_LONG},
  {"Com_revoke",	       (char*) (com_stat+(uint) SQLCOM_REVOKE),SHOW_LONG},
  {"Com_rollback",	       (char*) (com_stat+(uint) SQLCOM_ROLLBACK),SHOW_LONG},
  {"Com_select",	       (char*) (com_stat+(uint) SQLCOM_SELECT),SHOW_LONG},
  {"Com_set_option",	       (char*) (com_stat+(uint) SQLCOM_SET_OPTION),SHOW_LONG},
  {"Com_show_binlog_events",   (char*) (com_stat+(uint) SQLCOM_SHOW_BINLOG_EVENTS),SHOW_LONG},
  {"Com_show_binlogs",	       (char*) (com_stat+(uint) SQLCOM_SHOW_BINLOGS),SHOW_LONG},
  {"Com_show_create",	       (char*) (com_stat+(uint) SQLCOM_SHOW_CREATE),SHOW_LONG},
  {"Com_show_databases",       (char*) (com_stat+(uint) SQLCOM_SHOW_DATABASES),SHOW_LONG},
  {"Com_show_fields",	       (char*) (com_stat+(uint) SQLCOM_SHOW_FIELDS),SHOW_LONG},
  {"Com_show_grants",	       (char*) (com_stat+(uint) SQLCOM_SHOW_GRANTS),SHOW_LONG},
  {"Com_show_keys",	       (char*) (com_stat+(uint) SQLCOM_SHOW_KEYS),SHOW_LONG},
  {"Com_show_logs",	       (char*) (com_stat+(uint) SQLCOM_SHOW_LOGS),SHOW_LONG},
  {"Com_show_master_status",   (char*) (com_stat+(uint) SQLCOM_SHOW_MASTER_STAT),SHOW_LONG},
  {"Com_show_new_master",      (char*) (com_stat+(uint) SQLCOM_SHOW_NEW_MASTER),SHOW_LONG},
  {"Com_show_open_tables",     (char*) (com_stat+(uint) SQLCOM_SHOW_OPEN_TABLES),SHOW_LONG},
  {"Com_show_processlist",     (char*) (com_stat+(uint) SQLCOM_SHOW_PROCESSLIST),SHOW_LONG},
  {"Com_show_slave_hosts",     (char*) (com_stat+(uint) SQLCOM_SHOW_SLAVE_HOSTS),SHOW_LONG},
  {"Com_show_slave_status",    (char*) (com_stat+(uint) SQLCOM_SHOW_SLAVE_STAT),SHOW_LONG},
  {"Com_show_status",	       (char*) (com_stat+(uint) SQLCOM_SHOW_STATUS),SHOW_LONG},
  {"Com_show_innodb_status",   (char*) (com_stat+(uint) SQLCOM_SHOW_INNODB_STATUS),SHOW_LONG},
  {"Com_show_tables",	       (char*) (com_stat+(uint) SQLCOM_SHOW_TABLES),SHOW_LONG},
  {"Com_show_variables",       (char*) (com_stat+(uint) SQLCOM_SHOW_VARIABLES),SHOW_LONG},
  {"Com_slave_start",	       (char*) (com_stat+(uint) SQLCOM_SLAVE_START),SHOW_LONG},
  {"Com_slave_stop",	       (char*) (com_stat+(uint) SQLCOM_SLAVE_STOP),SHOW_LONG},
  {"Com_truncate",	       (char*) (com_stat+(uint) SQLCOM_TRUNCATE),SHOW_LONG},
  {"Com_unlock_tables",	       (char*) (com_stat+(uint) SQLCOM_UNLOCK_TABLES),SHOW_LONG},
  {"Com_update",	       (char*) (com_stat+(uint) SQLCOM_UPDATE),SHOW_LONG},
  {"Connections",              (char*) &thread_id,              SHOW_LONG_CONST},
  {"Created_tmp_disk_tables",  (char*) &created_tmp_disk_tables,SHOW_LONG},
  {"Created_tmp_tables",       (char*) &created_tmp_tables,     SHOW_LONG},
  {"Created_tmp_files",	       (char*) &my_tmp_file_created,	SHOW_LONG},
  {"Delayed_insert_threads",   (char*) &delayed_insert_threads, SHOW_LONG},
  {"Delayed_writes",           (char*) &delayed_insert_writes,  SHOW_LONG},
  {"Delayed_errors",           (char*) &delayed_insert_errors,  SHOW_LONG},
  {"Flush_commands",           (char*) &refresh_version,        SHOW_LONG_CONST},
  {"Handler_commit",           (char*) &ha_commit_count,        SHOW_LONG},
  {"Handler_delete",           (char*) &ha_delete_count,        SHOW_LONG},
  {"Handler_read_first",       (char*) &ha_read_first_count,    SHOW_LONG},
  {"Handler_read_key",         (char*) &ha_read_key_count,      SHOW_LONG},
  {"Handler_read_next",        (char*) &ha_read_next_count,     SHOW_LONG},
  {"Handler_read_prev",        (char*) &ha_read_prev_count,     SHOW_LONG},
  {"Handler_read_rnd",         (char*) &ha_read_rnd_count,      SHOW_LONG},
  {"Handler_read_rnd_next",    (char*) &ha_read_rnd_next_count, SHOW_LONG},
  {"Handler_rollback",         (char*) &ha_rollback_count,      SHOW_LONG},
  {"Handler_update",           (char*) &ha_update_count,        SHOW_LONG},
  {"Handler_write",            (char*) &ha_write_count,         SHOW_LONG},
  {"Key_blocks_used",          (char*) &_my_blocks_used,        SHOW_LONG_CONST},
  {"Key_read_requests",        (char*) &_my_cache_r_requests,   SHOW_LONG},
  {"Key_reads",                (char*) &_my_cache_read,         SHOW_LONG},
  {"Key_write_requests",       (char*) &_my_cache_w_requests,   SHOW_LONG},
  {"Key_writes",               (char*) &_my_cache_write,        SHOW_LONG},
  {"Max_used_connections",     (char*) &max_used_connections,   SHOW_LONG},
  {"Not_flushed_key_blocks",   (char*) &_my_blocks_changed,     SHOW_LONG_CONST},
  {"Not_flushed_delayed_rows", (char*) &delayed_rows_in_use,    SHOW_LONG_CONST},
  {"Open_tables",              (char*) 0,                       SHOW_OPENTABLES},
  {"Open_files",               (char*) &my_file_opened,         SHOW_INT_CONST},
  {"Open_streams",             (char*) &my_stream_opened,       SHOW_INT_CONST},
  {"Opened_tables",            (char*) &opened_tables,          SHOW_LONG},
  {"Questions",                (char*) 0,                       SHOW_QUESTION},
#ifdef HAVE_QUERY_CACHE
  {"Qcache_queries_in_cache",  (char*) &query_cache.queries_in_cache, SHOW_LONG_CONST},
  {"Qcache_inserts",           (char*) &query_cache.inserts,    SHOW_LONG},
  {"Qcache_hits",              (char*) &query_cache.hits,       SHOW_LONG},
  {"Qcache_not_cached",        (char*) &query_cache.refused,    SHOW_LONG},
  {"Qcache_free_memory",       (char*) &query_cache.free_memory, 
   SHOW_LONG_CONST},
  {"Qcache_free_blocks",       (char*) &query_cache.free_memory_blocks,
   SHOW_LONG_CONST},
  {"Qcache_total_blocks",      (char*) &query_cache.total_blocks,
   SHOW_LONG_CONST},
#endif /*HAVE_QUERY_CACHE*/
  {"Rpl_status",               (char*) 0,                 SHOW_RPL_STATUS},
  {"Select_full_join",         (char*) &select_full_join_count, SHOW_LONG},
  {"Select_full_range_join",   (char*) &select_full_range_join_count, SHOW_LONG},
  {"Select_range",             (char*) &select_range_count, 	SHOW_LONG},
  {"Select_range_check",       (char*) &select_range_check_count, SHOW_LONG},
  {"Select_scan",	       (char*) &select_scan_count,	SHOW_LONG},
  {"Slave_open_temp_tables",   (char*) &slave_open_temp_tables, SHOW_LONG},
  {"Slave_running",            (char*) 0, SHOW_SLAVE_RUNNING},
  {"Slow_launch_threads",      (char*) &slow_launch_threads,    SHOW_LONG},
  {"Slow_queries",             (char*) &long_query_count,       SHOW_LONG},
  {"Sort_merge_passes",	       (char*) &filesort_merge_passes,  SHOW_LONG},
  {"Sort_range",	       (char*) &filesort_range_count,   SHOW_LONG},
  {"Sort_rows",		       (char*) &filesort_rows,	        SHOW_LONG},
  {"Sort_scan",		       (char*) &filesort_scan_count,    SHOW_LONG},
#ifdef HAVE_OPENSSL
  {"Ssl_accepts",              (char*) 0,  	SHOW_SSL_CTX_SESS_ACCEPT},
  {"Ssl_finished_accepts",     (char*) 0,  	SHOW_SSL_CTX_SESS_ACCEPT_GOOD},
  {"Ssl_finished_connects",    (char*) 0,  	SHOW_SSL_CTX_SESS_CONNECT_GOOD},
  {"Ssl_accept_renegotiates",  (char*) 0, 	SHOW_SSL_CTX_SESS_ACCEPT_RENEGOTIATE},
  {"Ssl_connect_renegotiates", (char*) 0, 	SHOW_SSL_CTX_SESS_CONNECT_RENEGOTIATE},
  {"Ssl_callback_cache_hits",  (char*) 0,	SHOW_SSL_CTX_SESS_CB_HITS},
  {"Ssl_session_cache_hits",   (char*) 0,	SHOW_SSL_CTX_SESS_HITS},
  {"Ssl_session_cache_misses", (char*) 0,	SHOW_SSL_CTX_SESS_MISSES},
  {"Ssl_session_cache_timeouts", (char*) 0,	SHOW_SSL_CTX_SESS_TIMEOUTS},
  {"Ssl_used_session_cache_entries",(char*) 0,	SHOW_SSL_CTX_SESS_NUMBER},
  {"Ssl_client_connects",      (char*) 0,	SHOW_SSL_CTX_SESS_CONNECT},
  {"Ssl_session_cache_overflows", (char*) 0,	SHOW_SSL_CTX_SESS_CACHE_FULL},
  {"Ssl_session_cache_size",   (char*) 0,	SHOW_SSL_CTX_SESS_GET_CACHE_SIZE},
  {"Ssl_session_cache_mode",   (char*) 0,	SHOW_SSL_CTX_GET_SESSION_CACHE_MODE},
  {"Ssl_sessions_reused",      (char*) 0,	SHOW_SSL_SESSION_REUSED},
  {"Ssl_ctx_verify_mode",      (char*) 0,	SHOW_SSL_CTX_GET_VERIFY_MODE},
  {"Ssl_ctx_verify_depth",     (char*) 0,	SHOW_SSL_CTX_GET_VERIFY_DEPTH},
  {"Ssl_verify_mode",          (char*) 0,	SHOW_SSL_GET_VERIFY_MODE},
  {"Ssl_verify_depth",         (char*) 0,	SHOW_SSL_GET_VERIFY_DEPTH},
  {"Ssl_version",   	       (char*) 0,  	SHOW_SSL_GET_VERSION},
  {"Ssl_cipher",               (char*) 0,  	SHOW_SSL_GET_CIPHER},
  {"Ssl_cipher_list",          (char*) 0,  	SHOW_SSL_GET_CIPHER_LIST},
  {"Ssl_default_timeout",      (char*) 0,  	SHOW_SSL_GET_DEFAULT_TIMEOUT},
#endif /* HAVE_OPENSSL */
  {"Table_locks_immediate",    (char*) &locks_immediate,        SHOW_LONG},
  {"Table_locks_waited",       (char*) &locks_waited,           SHOW_LONG},
  {"Threads_cached",           (char*) &cached_thread_count,    SHOW_LONG_CONST},
  {"Threads_created",	       (char*) &thread_created,		SHOW_LONG_CONST},
  {"Threads_connected",        (char*) &thread_count,           SHOW_INT_CONST},
  {"Threads_running",          (char*) &thread_running,         SHOW_INT_CONST},
  {"Uptime",                   (char*) 0,                       SHOW_STARTTIME},
  {NullS, NullS, SHOW_LONG}
};

static void print_version(void)
{
  printf("%s  Ver %s for %s on %s\n",my_progname,
	 server_version,SYSTEM_TYPE,MACHINE_TYPE);
}

static void use_help(void)
{
  print_version();
  printf("Use '--help' or '--no-defaults --help' for a list of available options\n");
}

static void usage(void)
{
  print_version();
  puts("\
Copyright (C) 2000 MySQL AB, by Monty and others\n\
This software comes with ABSOLUTELY NO WARRANTY. This is free software,\n\
and you are welcome to modify and redistribute it under the GPL license\n\
Starts the MySQL server\n");

  printf("Usage: %s [OPTIONS]\n", my_progname);
#ifdef __WIN__
  puts("NT and Win32 specific options:\n\
  --console                     Don't remove the console window\n\
  --install                     Install the default service (NT)\n\
  --install-manual              Install the default service started manually (NT)\n\
  --install service_name        Install an optional service (NT)\n\
  --install-manual service_name Install an optional service started manually (NT)\n\
  --remove                      Remove the default service from the service list (NT)\n\
  --remove service_name         Remove the service_name from the service list (NT)\n\
  --enable-named-pipe           Only to be used for the	default server (NT)\n\
  --standalone                  Dummy option to start as a standalone server (NT)\
");
  puts("");
#endif
  print_defaults("my",load_default_groups);
  puts("");
  fix_paths();
  set_ports();

  my_print_help(my_long_options);
  my_print_variables(my_long_options);

  puts("\n\
To see what values a running MySQL server is using, type\n\
'mysqladmin variables' instead of 'mysqld --help'.");
}


static void set_options(void)
{
#if !defined( my_pthread_setprio ) && !defined( HAVE_PTHREAD_SETSCHEDPARAM )
  opt_specialflag |= SPECIAL_NO_PRIOR;
#endif

  sys_charset.value= (char*) MYSQL_CHARSET;
  (void) strmake(language, LANGUAGE, sizeof(language)-1);
  (void) strmake(mysql_real_data_home, get_relative_path(DATADIR),
		 sizeof(mysql_real_data_home-1));

  /* Set default values for some variables */
  global_system_variables.table_type=DB_TYPE_MYISAM;
  global_system_variables.tx_isolation=ISO_READ_COMMITTED;
  global_system_variables.select_limit= (ulong) HA_POS_ERROR;
  max_system_variables.select_limit= (ulong) HA_POS_ERROR;
  global_system_variables.max_join_size= (ulong) HA_POS_ERROR;
  max_system_variables.max_join_size= (ulong) HA_POS_ERROR;

#ifdef __WIN__
  /* Allow Win32 users to move MySQL anywhere */
  {
    char prg_dev[LIBLEN];
    my_path(prg_dev,my_progname,"mysql/bin");
    strcat(prg_dev,"/../");			// Remove 'bin' to get base dir
    cleanup_dirname(mysql_home,prg_dev);
  }
#else
  const char *tmpenv;
  if (!(tmpenv = getenv("MY_BASEDIR_VERSION")))
    tmpenv = DEFAULT_MYSQL_HOME;
  (void) strmake(mysql_home, tmpenv, sizeof(mysql_home)-1);
#endif

  my_disable_locking=myisam_single_user= 1;
  opt_external_locking=0;
  my_bind_addr = htonl( INADDR_ANY );
}


static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch(optid) {
  case '#':
#ifndef DBUG_OFF
    DBUG_PUSH(argument ? argument : default_dbug_option);
#endif
    opt_endinfo=1;				/* unireg: memory allocation */
    break;
  case 'a':
    opt_sql_mode = (MODE_REAL_AS_FLOAT | MODE_PIPES_AS_CONCAT |
		    MODE_ANSI_QUOTES | MODE_IGNORE_SPACE | MODE_SERIALIZABLE
		    | MODE_ONLY_FULL_GROUP_BY);
    global_system_variables.tx_isolation= ISO_SERIALIZABLE;
    break;
  case 'b':
    strmake(mysql_home,argument,sizeof(mysql_home)-1);
    break;
  case 'l':
    opt_log=1;
    break;
  case 'h':
    strmake(mysql_real_data_home,argument, sizeof(mysql_real_data_home)-1);
    break;
  case 'L':
    strmake(language, argument, sizeof(language)-1);
    break;
  case 'n':
    opt_specialflag|= SPECIAL_NEW_FUNC;
    break;
  case 'o':
    protocol_version=PROTOCOL_VERSION-1;
    break;
  case OPT_SLAVE_SKIP_ERRORS:
    init_slave_skip_errors(argument);
    break;
  case OPT_SAFEMALLOC_MEM_LIMIT:
#if !defined(DBUG_OFF) && defined(SAFEMALLOC)      
    safemalloc_mem_limit = atoi(argument);
#endif      
    break;
  case 'v':
  case 'V':
    print_version();
    exit(0);
  case 'I':
  case '?':
    usage();
    exit(0);
  case 'T':
    test_flags= argument ? (uint) atoi(argument) : 0;
    test_flags&= ~TEST_NO_THREADS;
    opt_endinfo=1;
    break;
  case (int) OPT_BIG_TABLES:
    thd_startup_options|=OPTION_BIG_TABLES;
    break;
  case (int) OPT_ISAM_LOG:
    opt_myisam_log=1;
    break;
  case (int) OPT_UPDATE_LOG:
    opt_update_log=1;
    break;
  case (int) OPT_BIN_LOG:
    opt_bin_log=1;
    break;
  case (int) OPT_INIT_RPL_ROLE:
  {
    int role;
    if ((role=find_type(argument, &rpl_role_typelib, 2)) <= 0)
    {
      fprintf(stderr, "Unknown replication role: %s\n", argument);
      exit(1);
    }
    rpl_status = (role == 1) ?  RPL_AUTH_MASTER : RPL_IDLE_SLAVE;
    break;
  }
  case (int)OPT_REPLICATE_IGNORE_DB:
  {
    i_string *db = new i_string(argument);
    replicate_ignore_db.push_back(db);
    break;
  }
  case (int)OPT_REPLICATE_DO_DB:
  {
    i_string *db = new i_string(argument);
    replicate_do_db.push_back(db);
    break;
  }
  case (int)OPT_REPLICATE_REWRITE_DB:
  {
    char* key = argument,*p, *val;
    
    if (!(p= strstr(argument, "->")))
    {
      fprintf(stderr,
	      "Bad syntax in replicate-rewrite-db - missing '->'!\n");
      exit(1);
    }
    val= p--;
    while (isspace(*p) && p > argument)
      *p-- = 0;
    if (p == argument)
    {
      fprintf(stderr,
	      "Bad syntax in replicate-rewrite-db - empty FROM db!\n");
      exit(1);
    }
    *val= 0;
    val+= 2;
    while (*val && isspace(*val))
      *val++;
    if (!*val)
    {
      fprintf(stderr,
	      "Bad syntax in replicate-rewrite-db - empty TO db!\n");
      exit(1);
    }

    i_string_pair *db_pair = new i_string_pair(key, val);
    replicate_rewrite_db.push_back(db_pair);
    break;
  }

  case (int)OPT_BINLOG_IGNORE_DB:
  {
    i_string *db = new i_string(argument);
    binlog_ignore_db.push_back(db);
    break;
  }
  case (int)OPT_BINLOG_DO_DB:
  {
    i_string *db = new i_string(argument);
    binlog_do_db.push_back(db);
    break;
  }
  case (int)OPT_REPLICATE_DO_TABLE:
  {
    if (!do_table_inited)
      init_table_rule_hash(&replicate_do_table, &do_table_inited);
    if (add_table_rule(&replicate_do_table, argument))
    {
      fprintf(stderr, "Could not add do table rule '%s'!\n", argument);
      exit(1);
    }
    table_rules_on = 1;
    break;
  }
  case (int)OPT_REPLICATE_WILD_DO_TABLE:
  {
    if (!wild_do_table_inited)
      init_table_rule_array(&replicate_wild_do_table,
			    &wild_do_table_inited);
    if (add_wild_table_rule(&replicate_wild_do_table, argument))
    {
      fprintf(stderr, "Could not add do table rule '%s'!\n", argument);
      exit(1);
    }
    table_rules_on = 1;
    break;
  }
  case (int)OPT_REPLICATE_WILD_IGNORE_TABLE:
  {
    if (!wild_ignore_table_inited)
      init_table_rule_array(&replicate_wild_ignore_table,
			    &wild_ignore_table_inited);
    if (add_wild_table_rule(&replicate_wild_ignore_table, argument))
    {
      fprintf(stderr, "Could not add ignore table rule '%s'!\n", argument);
      exit(1);
    }
    table_rules_on = 1;
    break;
  }
  case (int)OPT_REPLICATE_IGNORE_TABLE:
  {
    if (!ignore_table_inited)
      init_table_rule_hash(&replicate_ignore_table, &ignore_table_inited);
    if (add_table_rule(&replicate_ignore_table, argument))
    {
      fprintf(stderr, "Could not add ignore table rule '%s'!\n", argument);
      exit(1);
    }
    table_rules_on = 1;
    break;
  }
  case (int) OPT_SLOW_QUERY_LOG:
    opt_slow_log=1;
    break;
  case (int)OPT_RECKLESS_SLAVE:
    opt_reckless_slave = 1;
    init_slave_skip_errors("all");
    break;
  case (int) OPT_SKIP_NEW:
    opt_specialflag|= SPECIAL_NO_NEW_FUNC;
    myisam_delay_key_write=0;
    myisam_concurrent_insert=0;
    myisam_recover_options= HA_RECOVER_NONE;
    my_disable_symlinks=1;
    my_use_symdir=0;
    have_symlink=SHOW_OPTION_DISABLED;
    ha_open_options&= ~HA_OPEN_ABORT_IF_CRASHED;
#ifdef HAVE_QUERY_CACHE
    query_cache_size=0;
#endif
    break;
  case (int) OPT_SAFE:
    opt_specialflag|= SPECIAL_SAFE_MODE;
    myisam_delay_key_write=0;
    myisam_recover_options= HA_RECOVER_NONE;	// To be changed
    ha_open_options&= ~HA_OPEN_ABORT_IF_CRASHED;
    break;
  case (int) OPT_SKIP_PRIOR:
    opt_specialflag|= SPECIAL_NO_PRIOR;
    break;
  case (int) OPT_SKIP_LOCK:
    opt_external_locking=0;
    break;
  case (int) OPT_SKIP_HOST_CACHE:
    opt_specialflag|= SPECIAL_NO_HOST_CACHE;
    break;
  case (int) OPT_SKIP_RESOLVE:
    opt_specialflag|=SPECIAL_NO_RESOLVE;
    break;
  case (int) OPT_LONG_FORMAT:
    opt_specialflag|=SPECIAL_LONG_LOG_FORMAT;
    break;
  case (int) OPT_SKIP_NETWORKING:
    opt_disable_networking=1;
    mysql_port=0;
    break;
  case (int) OPT_SKIP_SHOW_DB:
    opt_skip_show_db=1;
    opt_specialflag|=SPECIAL_SKIP_SHOW_DB;
    mysql_port=0;
    break;
#ifdef ONE_THREAD
  case (int) OPT_ONE_THREAD:
    test_flags |= TEST_NO_THREADS;
#endif
    break;
  case (int) OPT_WANT_CORE:
    test_flags |= TEST_CORE_ON_SIGNAL;
    break;
  case (int) OPT_SKIP_STACK_TRACE:
    test_flags|=TEST_NO_STACKTRACE;
    break;
  case (int) OPT_SKIP_SYMLINKS:
    my_disable_symlinks=1;
    my_use_symdir=0;
    have_symlink=SHOW_OPTION_DISABLED;
    break;
  case (int) OPT_BIND_ADDRESS:
    if (argument && isdigit(argument[0]))
    {
      my_bind_addr = (ulong) inet_addr(argument);
    }
    else
    {
      struct hostent *ent;
      if (!argument || !argument[0])
	ent=gethostbyname(argument);
      else
      {
	char myhostname[255];
	if (gethostname(myhostname,sizeof(myhostname)) < 0)
	{
	  sql_perror("Can't start server: cannot get my own hostname!");
	  exit(1);
	}
	ent=gethostbyname(myhostname);
      }
      if (!ent)
      {
	sql_perror("Can't start server: cannot resolve hostname!");
	exit(1);
      }
      my_bind_addr = (ulong) ((in_addr*)ent->h_addr_list[0])->s_addr;
    }
    break;
  case (int) OPT_PID_FILE:
    strmake(pidfile_name, argument, sizeof(pidfile_name)-1);
    break;
#ifdef __WIN__
  case (int) OPT_STANDALONE:		/* Dummy option for NT */
    break;
#endif
  case (int) OPT_FLUSH:
#ifdef HAVE_ISAM
    nisam_flush=1;
#endif
    myisam_flush=1;
    flush_time=0;			// No auto flush
    break;
  case OPT_LOW_PRIORITY_UPDATES:
    thr_upgraded_concurrent_insert_lock= TL_WRITE_LOW_PRIORITY;
    global_system_variables.low_priority_updates=1;
    break;
  case OPT_BOOTSTRAP:
    opt_noacl=opt_bootstrap=1;
    break;
  case OPT_TABLE_TYPE:
  {
    int type;
    if ((type=find_type(argument, &ha_table_typelib, 2)) <= 0)
    {
      fprintf(stderr,"Unknown table type: %s\n",argument);
      exit(1);
    }
    global_system_variables.table_type= type-1;
    break;
  }
  case OPT_SERVER_ID:
    server_id_supplied = 1;
    break;
  case OPT_DELAY_KEY_WRITE:
    ha_open_options|=HA_OPEN_DELAY_KEY_WRITE;
    myisam_delay_key_write=1;
    break;
  case OPT_CHARSETS_DIR:
    strmake(mysql_charsets_dir, argument, sizeof(mysql_charsets_dir)-1);
    charsets_dir = mysql_charsets_dir;
    break;
#ifdef HAVE_OPENSSL
#include "sslopt-case.h"
#endif
  case OPT_DES_KEY_FILE:
#ifdef HAVE_OPENSSL
    des_key_file=argument;
#endif
    break;
  case OPT_TX_ISOLATION:
  {
    int type;
    if ((type=find_type(argument, &tx_isolation_typelib, 2)) <= 0)
    {
      fprintf(stderr,"Unknown transaction isolation type: %s\n",argument);
      exit(1);
    }
    global_system_variables.tx_isolation= (type-1);
    break;
  }
#ifdef HAVE_BERKELEY_DB
  case OPT_BDB_NOSYNC:
    berkeley_env_flags|=DB_TXN_NOSYNC;
    break;
  case OPT_BDB_NO_RECOVER:
    berkeley_init_flags&= ~(DB_RECOVER);
    break;
  case OPT_BDB_LOCK:
  {
    int type;
    if ((type=find_type(argument, &berkeley_lock_typelib, 2)) > 0)
      berkeley_lock_type=berkeley_lock_types[type-1];
    else
    {
      if (test_if_int(argument,(uint) strlen(argument)))
	berkeley_lock_scan_time=atoi(argument);
      else
      {
	fprintf(stderr,"Unknown lock type: %s\n",argument);
	exit(1);
      }
    }
    break;
  }
  case OPT_BDB_SHARED:
    berkeley_init_flags&= ~(DB_PRIVATE);
    berkeley_shared_data=1;
    break;
#endif /* HAVE_BERKELEY_DB */
  case OPT_BDB_SKIP:
#ifdef HAVE_BERKELEY_DB
    berkeley_skip=1;
    have_berkeley_db=SHOW_OPTION_DISABLED;
#endif
    break;
  case OPT_INNODB_SKIP:
#ifdef HAVE_INNOBASE_DB
    innodb_skip=1;
    have_innodb=SHOW_OPTION_DISABLED;
#endif
    break;
  case OPT_INNODB_DATA_FILE_PATH:
#ifdef HAVE_INNOBASE_DB
    innobase_data_file_path=argument;
#endif
    break;
#ifdef HAVE_INNOBASE_DB
  case OPT_INNODB_LOG_ARCHIVE:
    innobase_log_archive= argument ? test(atoi(argument)) : 1;
    break;
  case OPT_INNODB_FAST_SHUTDOWN:
    innobase_fast_shutdown= argument ? test(atoi(argument)) : 1;
    break;
#endif /* HAVE_INNOBASE_DB */
  case OPT_MYISAM_RECOVER:
  {
    if (!argument || !argument[0])
    {
      myisam_recover_options=    HA_RECOVER_DEFAULT;
      myisam_recover_options_str= myisam_recover_typelib.type_names[0];
    }
    else
    {
      myisam_recover_options_str=argument;
      if ((myisam_recover_options=
	   find_bit_type(argument, &myisam_recover_typelib)) == ~(ulong) 0)
      {
	fprintf(stderr, "Unknown option to myisam-recover: %s\n",argument);
	exit(1);
      }
    }
    ha_open_options|=HA_OPEN_ABORT_IF_CRASHED;
    break;
  }
  case OPT_SQL_MODE:
  {
    sql_mode_str = argument;
    if ((opt_sql_mode =
	 find_bit_type(argument, &sql_mode_typelib)) == ~(ulong) 0)
    {
      fprintf(stderr, "Unknown option to sql-mode: %s\n", argument);
      exit(1);
    }
    global_system_variables.tx_isolation= ((opt_sql_mode & MODE_SERIALIZABLE) ?
					   ISO_SERIALIZABLE :
					   ISO_READ_COMMITTED);
    break;
  }
  case OPT_MASTER_PASSWORD:
    master_password=argument;
    break;
  case OPT_SKIP_SAFEMALLOC:
#ifdef SAFEMALLOC
    sf_malloc_quick=1;
#endif
    break;
  }
  return 0;
}

	/* Initiates DEBUG - but no debugging here ! */

static void get_options(int argc,char **argv)
{
  int ho_error;
  THD *thd= current_thd;

  myisam_delay_key_write=1;			// Allow use of this
#ifndef HAVE_purify
  my_use_symdir=1;				// Use internal symbolic links
#else
  /* Symlinks gives too many warnings with purify */
  my_disable_symlinks=1;
  my_use_symdir=0;
  have_symlink=SHOW_OPTION_DISABLED;
#endif

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (mysqld_chroot)
    set_root(mysqld_chroot);
  fix_paths();

  /*
    Set some global variables from the global_system_variables
    In most cases the global variables will not be used
  */
  my_disable_locking= myisam_single_user= test(opt_external_locking == 0);
  my_default_record_cache_size=global_system_variables.read_buff_size;
  myisam_max_temp_length=
    (my_off_t) min(global_system_variables.myisam_max_sort_file_size,
		   (ulonglong) MAX_FILE_SIZE);
  myisam_max_extra_temp_length= 
    (my_off_t) min(global_system_variables.myisam_max_extra_sort_file_size,
		   (ulonglong) MAX_FILE_SIZE);

  /* Set global variables based on startup options */
  myisam_block_size=(uint) 1 << my_bit_log2(opt_myisam_block_size);
}


static char *get_relative_path(const char *path)
{
  if (test_if_hard_path(path) &&
      is_prefix(path,DEFAULT_MYSQL_HOME) &&
      strcmp(DEFAULT_MYSQL_HOME,FN_ROOTDIR))
  {
    path+=(uint) strlen(DEFAULT_MYSQL_HOME);
    while (*path == FN_LIBCHAR)
      path++;
  }
  return (char*) path;
}


/*
  Fix filename and replace extension where 'dir' is relative to
  mysql_real_data_home.
  Return 1 if len(path) > FN_REFLEN
*/

bool
fn_format_relative_to_data_home(my_string to, const char *name,
				const char *dir, const char *extension)
{
  char tmp_path[FN_REFLEN];
  if (!test_if_hard_path(dir))
  {
    strxnmov(tmp_path,sizeof(tmp_path)-1, mysql_real_data_home,
	     dir, NullS);
    dir=tmp_path;
  }
  return !fn_format(to, name, dir, extension,
		    MY_REPLACE_EXT | MY_UNPACK_FILENAME | MY_SAFE_PATH);
}


static void fix_paths(void)
{
  char buff[FN_REFLEN];
  (void) fn_format(mysql_home,mysql_home,"","",16); // Remove symlinks
  convert_dirname(mysql_home,mysql_home,NullS);
  convert_dirname(mysql_real_data_home,mysql_real_data_home,NullS);
  convert_dirname(language,language,NullS);
  (void) my_load_path(mysql_home,mysql_home,""); // Resolve current dir
  (void) my_load_path(mysql_real_data_home,mysql_real_data_home,mysql_home);
  (void) my_load_path(pidfile_name,pidfile_name,mysql_real_data_home);

  char *sharedir=get_relative_path(SHAREDIR);
  if (test_if_hard_path(sharedir))
    strmake(buff,sharedir,sizeof(buff)-1);		/* purecov: tested */
  else
    strxnmov(buff,sizeof(buff)-1,mysql_home,sharedir,NullS);
  convert_dirname(buff,buff,NullS);
  (void) my_load_path(language,language,buff);

  /* If --character-sets-dir isn't given, use shared library dir */
  if (charsets_dir != mysql_charsets_dir)
  {
    strxnmov(mysql_charsets_dir, sizeof(mysql_charsets_dir)-1, buff,
	     CHARSET_DIR, NullS);
    charsets_dir=mysql_charsets_dir;
  }

  char *end=convert_dirname(buff, opt_mysql_tmpdir, NullS);
  if (!(mysql_tmpdir= my_memdup(buff,(uint) (end-buff)+1, MYF(MY_FAE))))
    exit(1);
  if (!slave_load_tmpdir)
  {
    if (!(slave_load_tmpdir = (char*) my_strdup(mysql_tmpdir, MYF(MY_FAE))))
      exit(1);
  }
}



#ifdef SET_RLIMIT_NOFILE
static uint set_maximum_open_files(uint max_file_limit)
{
  struct rlimit rlimit;
  ulong old_cur;

  if (!getrlimit(RLIMIT_NOFILE,&rlimit))
  {
    old_cur=rlimit.rlim_cur;
    if (rlimit.rlim_cur >= max_file_limit)	// Nothing to do
      return rlimit.rlim_cur;			/* purecov: inspected */
    rlimit.rlim_cur=rlimit.rlim_max=max_file_limit;
    if (setrlimit(RLIMIT_NOFILE,&rlimit))
    {
      sql_print_error("Warning: setrlimit couldn't increase number of open files to more than %lu (request: %u)",
		      old_cur, max_file_limit);	/* purecov: inspected */
      max_file_limit=old_cur;
    }
    else
    {
      (void) getrlimit(RLIMIT_NOFILE,&rlimit);
      if ((uint) rlimit.rlim_cur != max_file_limit)
	sql_print_error("Warning: setrlimit returned ok, but didn't change limits. Max open files is %ld (request: %u)",
			(ulong) rlimit.rlim_cur,
			max_file_limit); /* purecov: inspected */
      max_file_limit=rlimit.rlim_cur;
    }
  }
  return max_file_limit;
}
#endif

#ifdef OS2
static uint set_maximum_open_files(uint max_file_limit)
{
   LONG     cbReqCount;
   ULONG    cbCurMaxFH, cbCurMaxFH0;
   APIRET   ulrc;

   // get current limit
   cbReqCount = 0;
   DosSetRelMaxFH( &cbReqCount, &cbCurMaxFH0);

   // set new limit
   cbReqCount = max_file_limit - cbCurMaxFH0;
   ulrc = DosSetRelMaxFH( &cbReqCount, &cbCurMaxFH);
   if (ulrc) {
      sql_print_error("Warning: DosSetRelMaxFH couldn't increase number of open files to more than %d",
         cbCurMaxFH0);
      cbCurMaxFH = cbCurMaxFH0;
   }

   return cbCurMaxFH;
}
#endif

/*
  Return a bitfield from a string of substrings separated by ','
  returns ~(ulong) 0 on error.
*/

static ulong find_bit_type(const char *x, TYPELIB *bit_lib)
{
  bool found_end;
  int  found_count;
  const char *end,*i,*j;
  const char **array, *pos;
  ulong found,found_int,bit;
  DBUG_ENTER("find_bit_type");
  DBUG_PRINT("enter",("x: '%s'",x));

  found=0;
  found_end= 0;
  pos=(my_string) x;
  while (*pos == ' ') pos++;
  found_end= *pos == 0;
  while (!found_end)
  {
    if (!*(end=strcend(pos,',')))		/* Let end point at fieldend */
    {
      while (end > pos && end[-1] == ' ')
	end--;					/* Skip end-space */
      found_end=1;
    }
    found_int=0; found_count=0;
    for (array=bit_lib->type_names, bit=1 ; (i= *array++) ; bit<<=1)
    {
      j=pos;
      while (j != end)
      {
	if (toupper(*i++) != toupper(*j++))
	  goto skipp;
      }
      found_int=bit;
      if (! *i)
      {
	found_count=1;
	break;
      }
      else if (j != pos)			// Half field found
      {
	found_count++;				// Could be one of two values
      }
skipp: ;
    }
    if (found_count != 1)
      DBUG_RETURN(~(ulong) 0);				// No unique value
    found|=found_int;
    pos=end+1;
  }

  DBUG_PRINT("exit",("bit-field: %ld",(ulong) found));
  DBUG_RETURN(found);
} /* find_bit_type */


/*****************************************************************************
  Instantiate templates
*****************************************************************************/

#ifdef __GNUC__
/* Used templates */
template class I_List<THD>;
template class I_List_iterator<THD>;
template class I_List<i_string>;
template class I_List<i_string_pair>;

FIX_GCC_LINKING_PROBLEM
#endif
