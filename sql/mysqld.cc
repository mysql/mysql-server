/* Copyright (C) 2000-2003 MySQL AB

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
#include "mysqld_suffix.h"
#ifdef HAVE_BERKELEY_DB
#include "ha_berkeley.h"
#endif
#ifdef HAVE_INNOBASE_DB
#include "ha_innodb.h"
#endif
#include "ha_myisam.h"
#ifdef HAVE_ISAM
#include "ha_isam.h"
#endif
#ifdef HAVE_NDBCLUSTER_DB
#include "ha_ndbcluster.h"
#endif
#include <nisam.h>
#include <thr_alarm.h>
#include <ft_global.h>
#include <errmsg.h>

#define mysqld_charset &my_charset_latin1

#ifndef DBUG_OFF
#define ONE_THREAD
#endif

#ifdef HAVE_purify
#define IF_PURIFY(A,B) (A)
#else
#define IF_PURIFY(A,B) (B)
#endif

#ifndef INADDR_NONE
#define INADDR_NONE -1				// Error value from inet_addr
#endif

/* stack traces are only supported on linux intel */
#if defined(__linux__)  && defined(__i386__) && defined(USE_PSTACK)
#define	HAVE_STACK_TRACE_ON_SEGV
#include "../pstack/pstack.h"
char pstack_file_name[80];
#endif /* __linux__ */

/* We have HAVE_purify below as this speeds up the shutdown of MySQL */

#if defined(HAVE_DEC_3_2_THREADS) || defined(SIGNALS_DONT_BREAK_READ) || defined(HAVE_purify) && defined(__linux__)
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
#  ifndef __NETWARE__
#include <sys/resource.h>
#  endif /* __NETWARE__ */
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
#endif /* __STDC__ */
#endif /* HAVE_LIBWRAP */

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifdef __NETWARE__
#include <nks/vm.h>
#include <library.h>
#include <monitor.h>
#include <zOmni.h>                              //For NEB
#include <neb.h>                                //For NEB
#include <nebpub.h>                             //For NEB
#include <zEvent.h>                             //For NSS event structures
#include <zPublics.h>

static void *neb_consumer_id= NULL;             //For storing NEB consumer id
static char datavolname[256]= {0};
static VolumeID_t datavolid;
static event_handle_t eh;
static Report_t ref;
static void *refneb= NULL;
my_bool event_flag= FALSE;
static int volumeid= -1;

  /* NEB event callback */
unsigned long neb_event_callback(struct EventBlock *eblock);
static void registerwithneb();
static void getvolumename();
static void getvolumeID(BYTE *volumeName);
#endif /* __NETWARE__ */
  

#ifdef _AIX41
int initgroups(const char *,unsigned int);
#endif

#if defined(__FreeBSD__) && defined(HAVE_IEEEFP_H)
#include <ieeefp.h>
#ifdef HAVE_FP_EXCEPT				// Fix type conflict
typedef fp_except fp_except_t;
#endif

  /* We can't handle floating point exceptions with threads, so disable
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
#define MYSQL_KILL_SIGNAL SIGTERM

#ifdef HAVE_GLIBC2_STYLE_GETHOSTBYNAME_R
#include <sys/types.h>
#else
#include <my_pthread.h>			// For thr_setconcurency()
#endif

#ifdef SOLARIS
extern "C" int gethostname(char *name, int namelen);
#endif


/* Constants */

const char *show_comp_option_name[]= {"YES", "NO", "DISABLED"};
const char *sql_mode_names[] =
{
  "REAL_AS_FLOAT", "PIPES_AS_CONCAT", "ANSI_QUOTES", "IGNORE_SPACE",
  "?", "ONLY_FULL_GROUP_BY", "NO_UNSIGNED_SUBTRACTION",
  "NO_DIR_IN_CREATE",
  "POSTGRESQL", "ORACLE", "MSSQL", "DB2", "MAXDB", "NO_KEY_OPTIONS",
  "NO_TABLE_OPTIONS", "NO_FIELD_OPTIONS", "MYSQL323", "MYSQL40", "ANSI",
  "NO_AUTO_VALUE_ON_ZERO", NullS
};
TYPELIB sql_mode_typelib= { array_elements(sql_mode_names)-1,"",
			    sql_mode_names, NULL };
const char *first_keyword= "first", *binary_keyword= "BINARY";
const char *my_localhost= "localhost", *delayed_user= "DELAYED";
#if SIZEOF_OFF_T > 4 && defined(BIG_TABLES)
#define GET_HA_ROWS GET_ULL
#else
#define GET_HA_ROWS GET_ULONG
#endif

bool opt_large_files= sizeof(my_off_t) > 4;

/*
  Used with --help for detailed option
*/
bool opt_help= 0;
bool opt_verbose= 0;

arg_cmp_func Arg_comparator::comparator_matrix[4][2] =
{{&Arg_comparator::compare_string,     &Arg_comparator::compare_e_string},
 {&Arg_comparator::compare_real,       &Arg_comparator::compare_e_real},
 {&Arg_comparator::compare_int_signed, &Arg_comparator::compare_e_int},
 {&Arg_comparator::compare_row,        &Arg_comparator::compare_e_row}};


/* Global variables */

bool opt_log, opt_update_log, opt_bin_log, opt_slow_log;
bool opt_error_log= IF_WIN(1,0);
bool opt_disable_networking=0, opt_skip_show_db=0;
bool lower_case_table_names_used= 0;
bool server_id_supplied = 0;
bool opt_endinfo,using_udf_functions, locked_in_memory;
bool opt_using_transactions, using_update_log;
bool volatile abort_loop, select_thread_in_use, signal_thread_in_use;
bool volatile ready_to_exit, shutdown_in_progress, grant_option;

my_bool opt_skip_slave_start = 0; // If set, slave is not autostarted
my_bool opt_reckless_slave = 0;
my_bool opt_enable_named_pipe= 0, opt_debugging= 0;
my_bool opt_local_infile, opt_external_locking, opt_slave_compressed_protocol;
my_bool opt_safe_user_create = 0, opt_no_mix_types = 0;
my_bool opt_show_slave_auth_info, opt_sql_bin_update = 0;
my_bool opt_log_slave_updates= 0;
my_bool	opt_console= 0, opt_bdb, opt_innodb, opt_isam, opt_ndbcluster;
my_bool opt_readonly, use_temp_pool, relay_log_purge;
my_bool opt_sync_bdb_logs, opt_sync_frm;
my_bool opt_secure_auth= 0;
my_bool opt_short_log_format= 0;
my_bool opt_log_queries_not_using_indexes= 0;
my_bool lower_case_file_system= 0;
my_bool opt_innodb_safe_binlog= 0;
volatile bool mqh_used = 0;

uint mysqld_port, test_flags, select_errors, dropping_tables, ha_open_options;
uint delay_key_write_options, protocol_version;
uint lower_case_table_names;
uint opt_crash_binlog_innodb;
uint volatile thread_count, thread_running, kill_cached_threads, wake_thread;

ulong back_log, connect_timeout, concurrency;
ulong server_id, thd_startup_options;
ulong table_cache_size, thread_stack, thread_stack_min, what_to_log;
ulong query_buff_size, slow_launch_time, slave_open_temp_tables;
ulong open_files_limit, max_binlog_size, max_relay_log_size;
ulong slave_net_timeout;
ulong thread_cache_size=0, binlog_cache_size=0, max_binlog_cache_size=0;
ulong query_cache_size=0;
ulong com_stat[(uint) SQLCOM_END], com_other;
ulong bytes_sent, bytes_received, net_big_packet_count;
ulong refresh_version, flush_version;	/* Increments on each reload */
ulong query_id, long_query_count;
ulong aborted_threads, killed_threads, aborted_connects;
ulong delayed_insert_timeout, delayed_insert_limit, delayed_queue_size;
ulong delayed_insert_threads, delayed_insert_writes, delayed_rows_in_use;
ulong delayed_insert_errors,flush_time, thread_created;
ulong filesort_rows, filesort_range_count, filesort_scan_count;
ulong filesort_merge_passes;
ulong select_range_check_count, select_range_count, select_scan_count;
ulong select_full_range_join_count,select_full_join_count;
ulong specialflag=0,opened_tables=0,created_tmp_tables=0,
      created_tmp_disk_tables=0;
ulong binlog_cache_use= 0, binlog_cache_disk_use= 0;
ulong max_connections,max_used_connections,
      max_connect_errors, max_user_connections = 0;
ulong thread_id=1L,current_pid;
ulong slow_launch_threads = 0, sync_binlog_period;
ulong expire_logs_days = 0;
ulong rpl_recovery_rank=0;
ulong my_bind_addr;			/* the address we bind to */
volatile ulong cached_thread_count= 0;

double log_10[32];			/* 10 potences */
time_t start_time;

char mysql_home[FN_REFLEN], pidfile_name[FN_REFLEN], system_time_zone[30];
char *default_tz_name;
char log_error_file[FN_REFLEN], glob_hostname[FN_REFLEN];
char* log_error_file_ptr= log_error_file;
char mysql_real_data_home[FN_REFLEN],
     language[LIBLEN],reg_ext[FN_EXTLEN], mysql_charsets_dir[FN_REFLEN],
     *mysqld_user,*mysqld_chroot, *opt_init_file,
     *opt_init_connect, *opt_init_slave,
     def_ft_boolean_syntax[sizeof(ft_boolean_syntax)];

const char *opt_date_time_formats[3];

char *language_ptr, *default_collation_name, *default_character_set_name;
char mysql_data_home_buff[2], *mysql_data_home=mysql_real_data_home;
struct passwd *user_info;
char server_version[SERVER_VERSION_LENGTH];
char *mysqld_unix_port, *opt_mysql_tmpdir;
char *my_bind_addr_str;
const char **errmesg;			/* Error messages */
const char *myisam_recover_options_str="OFF";
const char *sql_mode_str="OFF";
/* name of reference on left espression in rewritten IN subquery */
const char *in_left_expr_name= "<left expr>";
/* name of additional condition */
const char *in_additional_cond= "<IN COND>";
/* classes for comparation parsing/processing */
Eq_creator eq_creator;
Ne_creator ne_creator;
Gt_creator gt_creator;
Lt_creator lt_creator;
Ge_creator ge_creator;
Le_creator le_creator;


FILE *bootstrap_file;

I_List<i_string_pair> replicate_rewrite_db;
I_List<i_string> replicate_do_db, replicate_ignore_db;
// allow the user to tell us which db to replicate and which to ignore
I_List<i_string> binlog_do_db, binlog_ignore_db;
I_List<THD> threads,thread_cache;
I_List<NAMED_LIST> key_caches;

struct system_variables global_system_variables;
struct system_variables max_system_variables;

MY_TMPDIR mysql_tmpdir_list;
MY_BITMAP temp_pool;
KEY_CACHE *sql_key_cache;

CHARSET_INFO *system_charset_info, *files_charset_info ;
CHARSET_INFO *national_charset_info, *table_alias_charset;

SHOW_COMP_OPTION have_berkeley_db, have_innodb, have_isam, have_ndbcluster, 
  have_example_db, have_archive_db, have_csv_db;
SHOW_COMP_OPTION have_raid, have_openssl, have_symlink, have_query_cache;
SHOW_COMP_OPTION have_geometry, have_rtree_keys;
SHOW_COMP_OPTION have_crypt, have_compress;

/* Thread specific variables */

pthread_key(MEM_ROOT**,THR_MALLOC);
pthread_key(THD*, THR_THD);
pthread_mutex_t LOCK_mysql_create_db, LOCK_Acl, LOCK_open, LOCK_thread_count,
		LOCK_mapped_file, LOCK_status,
		LOCK_error_log, LOCK_uuid_generator,
		LOCK_delayed_insert, LOCK_delayed_status, LOCK_delayed_create,
		LOCK_crypt, LOCK_bytes_sent, LOCK_bytes_received,
	        LOCK_global_system_variables,
		LOCK_user_conn, LOCK_slave_list, LOCK_active_mi;
rw_lock_t	LOCK_grant, LOCK_sys_init_connect, LOCK_sys_init_slave;
pthread_cond_t COND_refresh,COND_thread_count, COND_slave_stopped,
	       COND_slave_start;
pthread_cond_t COND_thread_cache,COND_flush_thread_cache;
pthread_t signal_thread;
pthread_attr_t connection_attrib;

/* replication parameters, if master_host is not NULL, we are a slave */
uint master_port= MYSQL_PORT, master_connect_retry = 60;
uint report_port= MYSQL_PORT;
ulong master_retry_count=0;
char *master_user, *master_password, *master_host, *master_info_file;
char *relay_log_info_file, *report_user, *report_password, *report_host;
char *opt_relay_logname = 0, *opt_relaylog_index_name=0;
my_bool master_ssl;
char *master_ssl_key, *master_ssl_cert;
char *master_ssl_ca, *master_ssl_capath, *master_ssl_cipher;

/* Static variables */

static bool kill_in_progress, segfaulted;
static my_bool opt_do_pstack, opt_noacl, opt_bootstrap, opt_myisam_log;
static int cleanup_done;
static ulong opt_specialflag, opt_myisam_block_size;
static char *opt_logname, *opt_update_logname, *opt_binlog_index_name;
static char *opt_slow_logname;
static char *mysql_home_ptr, *pidfile_name_ptr;
static char **defaults_argv;
static char *opt_bin_logname;

static my_socket unix_sock,ip_sock;
static pthread_t select_thread;
struct rand_struct sql_rand; // used by sql_class.cc:THD::THD()

/* OS specific variables */

#ifdef __WIN__
#undef	 getpid
#include <process.h>

static pthread_cond_t COND_handler_count;
static uint handler_count;
static bool start_mode=0, use_opt_args;
static int opt_argc;
static char **opt_argv;

#if !defined(EMBEDDED_LIBRARY)
static HANDLE hEventShutdown;
static char shutdown_event_name[40];
#include "nt_servc.h"
static	 NTService  Service;	      // Service object for WinNT
#endif /* EMBEDDED_LIBRARY */
#endif /* __WIN__ */

#ifdef __NT__
static char pipe_name[512];
static SECURITY_ATTRIBUTES saPipeSecurity;
static SECURITY_DESCRIPTOR sdPipeDescriptor;
static HANDLE hPipe = INVALID_HANDLE_VALUE;
#endif

#ifdef OS2
pthread_cond_t eventShutdown;
#endif

#ifndef EMBEDDED_LIBRARY
bool mysqld_embedded=0;
#else
bool mysqld_embedded=1;
#endif

#ifndef DBUG_OFF
static const char* default_dbug_option;
#endif
#ifdef HAVE_LIBWRAP
char *libwrapName= NULL;
#endif
#ifdef HAVE_QUERY_CACHE
ulong query_cache_limit= 0;
ulong query_cache_min_res_unit= QUERY_CACHE_MIN_RESULT_DATA_SIZE;
Query_cache query_cache;
#endif
#ifdef HAVE_SMEM
char *shared_memory_base_name= default_shared_memory_base_name;
bool opt_enable_shared_memory;
HANDLE smem_event_connect_request= 0;
#endif

#include "sslopt-vars.h"
#ifdef HAVE_OPENSSL
char *des_key_file;
struct st_VioSSLAcceptorFd *ssl_acceptor_fd;
#endif /* HAVE_OPENSSL */


/* Function declarations */

static void start_signal_handler(void);
extern "C" pthread_handler_decl(signal_hand, arg);
static void mysql_init_variables(void);
static void get_options(int argc,char **argv);
static void set_server_version(void);
static int init_thread_environment();
static char *get_relative_path(const char *path);
static void fix_paths(void);
extern "C" pthread_handler_decl(handle_connections_sockets,arg);
extern "C" pthread_handler_decl(kill_server_thread,arg);
static int bootstrap(FILE *file);
static void close_server_sock();
static bool read_init_file(char *file_name);
#ifdef __NT__
extern "C" pthread_handler_decl(handle_connections_namedpipes,arg);
#endif
#ifdef HAVE_SMEM
static pthread_handler_decl(handle_connections_shared_memory,arg);
#endif
extern "C" pthread_handler_decl(handle_slave,arg);
static ulong find_bit_type(const char *x, TYPELIB *bit_lib);
static void clean_up(bool print_message);
static void clean_up_mutexes(void);
static int test_if_case_insensitive(const char *dir_name);
static void create_pid_file();

#ifndef EMBEDDED_LIBRARY
/****************************************************************************
** Code to end mysqld
****************************************************************************/

static void close_connections(void)
{
#ifdef EXTRA_DEBUG
  int count=0;
#endif
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
#if !defined(__WIN__) && !defined(__EMX__) && !defined(OS2) && !defined(__NETWARE__)
  DBUG_PRINT("quit",("waiting for select thread: %lx",select_thread));
  (void) pthread_mutex_lock(&LOCK_thread_count);

  while (select_thread_in_use)
  {
    struct timespec abstime;
    int error;
    LINT_INIT(error);
    DBUG_PRINT("info",("Waiting for select thread"));

#ifndef DONT_USE_THR_ALARM
    if (pthread_kill(select_thread,THR_CLIENT_ALARM))
      break;					// allready dead
#endif
    set_timespec(abstime, 2);
    for (uint tmp=0 ; tmp < 10 && select_thread_in_use; tmp++)
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
    if ((temp = CreateFile(pipe_name,
			   GENERIC_READ | GENERIC_WRITE,
			   0,
			   NULL,
			   OPEN_EXISTING,
			   0,
			   NULL )) != INVALID_HANDLE_VALUE)
    {
      WaitNamedPipe(pipe_name, 1000);
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
    (void) unlink(mysqld_unix_port);
    unix_sock= INVALID_SOCKET;
  }
#endif
  end_thr_alarm(0);			 // Abort old alarms.
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
    if (tmp->vio_ok())
    {
      sql_print_error(ER(ER_FORCING_CLOSE),my_progname,
		      tmp->thread_id,tmp->user ? tmp->user : "");
      close_connection(tmp,0,0);
    }
#endif
    DBUG_PRINT("quit",("Unlocking LOCK_thread_count"));
    (void) pthread_mutex_unlock(&LOCK_thread_count);
  }
  /* All threads has now been aborted */
  DBUG_PRINT("quit",("Waiting for threads to die (count=%u)",thread_count));
  (void) pthread_mutex_lock(&LOCK_thread_count);
  while (thread_count)
  {
    (void) pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
    DBUG_PRINT("quit",("One thread died (count=%u)",thread_count));
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  DBUG_PRINT("quit",("close_connections thread"));
  DBUG_VOID_RETURN;
}
#endif /*EMBEDDED_LIBRARY*/


static void close_server_sock()
{
#ifdef HAVE_CLOSE_SERVER_SOCK
  DBUG_ENTER("close_server_sock");
  my_socket tmp_sock;
  tmp_sock=ip_sock;
  if (tmp_sock != INVALID_SOCKET)
  {
    ip_sock=INVALID_SOCKET;
    DBUG_PRINT("info",("calling shutdown on TCP/IP socket"));
    VOID(shutdown(tmp_sock,2));
#if defined(__NETWARE__)
    /*
      The following code is disabled for normal systems as it causes MySQL
      to hang on AIX 4.3 during shutdown
    */
    DBUG_PRINT("info",("calling closesocket on TCP/IP socket"));
    VOID(closesocket(tmp_sock));
#endif
  }
  tmp_sock=unix_sock;
  if (tmp_sock != INVALID_SOCKET)
  {
    unix_sock=INVALID_SOCKET;
    DBUG_PRINT("info",("calling shutdown on unix socket"));
    VOID(shutdown(tmp_sock,2));
#if defined(__NETWARE__)
    /*
      The following code is disabled for normal systems as it may cause MySQL
      to hang on AIX 4.3 during shutdown
    */
    DBUG_PRINT("info",("calling closesocket on unix/IP socket"));
    VOID(closesocket(tmp_sock));
#endif
    VOID(unlink(mysqld_unix_port));
  }
  DBUG_VOID_RETURN;
#endif
}


void kill_mysql(void)
{
  DBUG_ENTER("kill_mysql");

#ifdef SIGNALS_DONT_BREAK_READ
  abort_loop=1;					// Break connection loops
  close_server_sock();				// Force accept to wake up
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
#ifdef HAVE_SMEM
    /*
     Send event to smem_event_connect_request for aborting
    */
    if (!SetEvent(smem_event_connect_request))
    {
      DBUG_PRINT("error",("Got error: %ld from SetEvent of smem_event_connect_request",GetLastError()));
    }
#endif  
#endif
#elif defined(OS2)
  pthread_cond_signal( &eventShutdown);		// post semaphore
#elif defined(HAVE_PTHREAD_KILL)
  if (pthread_kill(signal_thread, MYSQL_KILL_SIGNAL))
  {
    DBUG_PRINT("error",("Got error %d from pthread_kill",errno)); /* purecov: inspected */
  }
#elif !defined(SIGNALS_DONT_BREAK_READ)
  kill(current_pid, MYSQL_KILL_SIGNAL);
#endif
  DBUG_PRINT("quit",("After pthread_kill"));
  shutdown_in_progress=1;			// Safety if kill didn't work
#ifdef SIGNALS_DONT_BREAK_READ
  if (!kill_in_progress)
  {
    pthread_t tmp;
    abort_loop=1;
    if (pthread_create(&tmp,&connection_attrib, kill_server_thread,
			   (void*) 0))
      sql_print_error("Can't create thread to kill server");
  }
#endif
  DBUG_VOID_RETURN;
}

	/* Force server down. kill all connections and threads and exit */

#if defined(OS2) || defined(__NETWARE__)
extern "C" void kill_server(int sig_ptr)
#define RETURN_FROM_KILL_SERVER DBUG_VOID_RETURN
#elif !defined(__WIN__)
static void *kill_server(void *sig_ptr)
#define RETURN_FROM_KILL_SERVER DBUG_RETURN(0)
#else
static void __cdecl kill_server(int sig_ptr)
#define RETURN_FROM_KILL_SERVER DBUG_VOID_RETURN
#endif
{
  int sig=(int) (long) sig_ptr;			// This is passed a int
  DBUG_ENTER("kill_server");
#ifndef EMBEDDED_LIBRARY
  // if there is a signal during the kill in progress, ignore the other
  if (kill_in_progress)				// Safety
    RETURN_FROM_KILL_SERVER;
  kill_in_progress=TRUE;
  abort_loop=1;					// This should be set
  signal(sig,SIG_IGN);
  if (sig == MYSQL_KILL_SIGNAL || sig == 0)
    sql_print_information(ER(ER_NORMAL_SHUTDOWN),my_progname);
  else
    sql_print_error(ER(ER_GOT_SIGNAL),my_progname,sig); /* purecov: inspected */

#if defined(__NETWARE__) || (defined(USE_ONE_SIGNAL_HAND) && !defined(__WIN__) && !defined(OS2))
  my_thread_init();				// If this is a new thread
#endif
  close_connections();
  if (sig != MYSQL_KILL_SIGNAL && sig != 0)
    unireg_abort(1);				/* purecov: inspected */
  else
    unireg_end();
#ifdef __NETWARE__
  if (!event_flag)
      pthread_join(select_thread, NULL);	// wait for main thread
#endif /* __NETWARE__ */

  pthread_exit(0);				/* purecov: deadcode */

#endif /* EMBEDDED_LIBRARY */
  RETURN_FROM_KILL_SERVER;
}


#if defined(USE_ONE_SIGNAL_HAND) || (defined(__NETWARE__) && defined(SIGNALS_DONT_BREAK_READ))
extern "C" pthread_handler_decl(kill_server_thread,arg __attribute__((unused)))
{
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

extern "C" sig_handler print_signal_warning(int sig)
{
  if (!DBUG_IN_USE)
  {
    if (global_system_variables.log_warnings)
      sql_print_warning("Got signal %d from thread %d",
		      sig,my_thread_id());
  }
#ifdef DONT_REMEMBER_SIGNAL
  sigset(sig,print_signal_warning);		/* int. thread system calls */
#endif
#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
  if (sig == SIGALRM)
    alarm(2);					/* reschedule alarm */
#endif
}

/*
  cleanup all memory and end program nicely

  SYNOPSIS
    unireg_end()

  NOTES
    This function never returns.

    If SIGNALS_DONT_BREAK_READ is defined, this function is called
    by the main thread. To get MySQL to shut down nicely in this case
    (Mac OS X) we have to call exit() instead if pthread_exit().
*/

#ifndef EMBEDDED_LIBRARY
void unireg_end(void)
{
  clean_up(1);
  my_thread_end();
#if defined(SIGNALS_DONT_BREAK_READ) && !defined(__NETWARE__)
  exit(0);
#else
  pthread_exit(0);				// Exit is in main thread
#endif
}

extern "C" void unireg_abort(int exit_code)
{
  DBUG_ENTER("unireg_abort");
  if (exit_code)
    sql_print_error("Aborting\n");
  clean_up(exit_code || !opt_bootstrap); /* purecov: inspected */
  DBUG_PRINT("quit",("done with cleanup in unireg_abort"));
  clean_up_mutexes();
  my_end(opt_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
  exit(exit_code); /* purecov: inspected */
}
#endif


void clean_up(bool print_message)
{
  DBUG_PRINT("exit",("clean_up"));
  if (cleanup_done++)
    return; /* purecov: inspected */

  mysql_log.cleanup();
  mysql_slow_log.cleanup();
  mysql_update_log.cleanup();
  mysql_bin_log.cleanup();

#ifdef HAVE_REPLICATION
  if (use_slave_mask)
    bitmap_free(&slave_error_mask);
#endif
  my_tz_free();
  my_dbopt_free();
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  acl_free(1);
  grant_free();
#endif
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
  delete_elements(&key_caches, (void (*)(const char*, gptr)) free_key_cache);
  multi_keycache_free();
  end_thr_alarm(1);			/* Free allocated memory */
#ifdef USE_RAID
  end_raid();
#endif
  my_free_open_file_info();
  my_free((char*) global_system_variables.date_format,
	  MYF(MY_ALLOW_ZERO_PTR));
  my_free((char*) global_system_variables.time_format,
	  MYF(MY_ALLOW_ZERO_PTR));
  my_free((char*) global_system_variables.datetime_format,
	  MYF(MY_ALLOW_ZERO_PTR));
  if (defaults_argv)
    free_defaults(defaults_argv);
  my_free(sys_init_connect.value, MYF(MY_ALLOW_ZERO_PTR));
  my_free(sys_init_slave.value, MYF(MY_ALLOW_ZERO_PTR));
  free_tmpdir(&mysql_tmpdir_list);
#ifdef HAVE_REPLICATION
  my_free(slave_load_tmpdir,MYF(MY_ALLOW_ZERO_PTR));
#endif
  x_free(opt_bin_logname);
  x_free(opt_relay_logname);
  bitmap_free(&temp_pool);
  free_max_user_conn();
#ifdef HAVE_REPLICATION
  end_slave_list();
  free_list(&replicate_do_db);
  free_list(&replicate_ignore_db);
  free_list(&binlog_do_db);
  free_list(&binlog_ignore_db);
  free_list(&replicate_rewrite_db);
#endif
#ifdef HAVE_OPENSSL
  if (ssl_acceptor_fd)
    my_free((gptr) ssl_acceptor_fd, MYF(MY_ALLOW_ZERO_PTR));
  free_des_key_file();
#endif /* HAVE_OPENSSL */
#ifdef USE_REGEX
  regex_end();
#endif

  if (print_message && errmesg)
    sql_print_information(ER(ER_SHUTDOWN_COMPLETE),my_progname);
#if !defined(EMBEDDED_LIBRARY)
  if (!opt_bootstrap)
    (void) my_delete(pidfile_name,MYF(0));	// This may not always exist
#endif
  x_free((gptr) my_errmsg[ERRMAPP]);	/* Free messages */
  DBUG_PRINT("quit", ("Error messages freed"));
  /* Tell main we are ready */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  DBUG_PRINT("quit", ("got thread count lock"));
  ready_to_exit=1;
  /* do the broadcast inside the lock to ensure that my_end() is not called */
  (void) pthread_cond_broadcast(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  /*
    The following lines may never be executed as the main thread may have
    killed us
  */
  DBUG_PRINT("quit", ("done with cleanup"));
} /* clean_up */


static void clean_up_mutexes()
{
  (void) pthread_mutex_destroy(&LOCK_mysql_create_db);
  (void) pthread_mutex_destroy(&LOCK_Acl);
  (void) rwlock_destroy(&LOCK_grant);
  (void) pthread_mutex_destroy(&LOCK_open);
  (void) pthread_mutex_destroy(&LOCK_thread_count);
  (void) pthread_mutex_destroy(&LOCK_mapped_file);
  (void) pthread_mutex_destroy(&LOCK_status);
  (void) pthread_mutex_destroy(&LOCK_error_log);
  (void) pthread_mutex_destroy(&LOCK_delayed_insert);
  (void) pthread_mutex_destroy(&LOCK_delayed_status);
  (void) pthread_mutex_destroy(&LOCK_delayed_create);
  (void) pthread_mutex_destroy(&LOCK_manager);
  (void) pthread_mutex_destroy(&LOCK_crypt);
  (void) pthread_mutex_destroy(&LOCK_bytes_sent);
  (void) pthread_mutex_destroy(&LOCK_bytes_received);
  (void) pthread_mutex_destroy(&LOCK_user_conn);
#ifdef HAVE_REPLICATION
  (void) pthread_mutex_destroy(&LOCK_rpl_status);
  (void) pthread_cond_destroy(&COND_rpl_status);
#endif
  (void) pthread_mutex_destroy(&LOCK_active_mi);
  (void) rwlock_destroy(&LOCK_sys_init_connect);
  (void) rwlock_destroy(&LOCK_sys_init_slave);
  (void) pthread_mutex_destroy(&LOCK_global_system_variables);
  (void) pthread_cond_destroy(&COND_thread_count);
  (void) pthread_cond_destroy(&COND_refresh);
  (void) pthread_cond_destroy(&COND_thread_cache);
  (void) pthread_cond_destroy(&COND_flush_thread_cache);
  (void) pthread_cond_destroy(&COND_manager);
}

/****************************************************************************
** Init IP and UNIX socket
****************************************************************************/

static void set_ports()
{
  char	*env;
  if (!mysqld_port && !opt_disable_networking)
  {					// Get port if not from commandline
    struct  servent *serv_ptr;
    mysqld_port= MYSQL_PORT;
    if ((serv_ptr= getservbyname("mysql", "tcp")))
      mysqld_port= ntohs((u_short) serv_ptr->s_port); /* purecov: inspected */
    if ((env = getenv("MYSQL_TCP_PORT")))
      mysqld_port= (uint) atoi(env);		/* purecov: inspected */
  }
  if (!mysqld_unix_port)
  {
#ifdef __WIN__
    mysqld_unix_port= (char*) MYSQL_NAMEDPIPE;
#else
    mysqld_unix_port= (char*) MYSQL_UNIX_ADDR;
#endif
    if ((env = getenv("MYSQL_UNIX_PORT")))
      mysqld_unix_port= env;			/* purecov: inspected */
  }
}

#ifndef EMBEDDED_LIBRARY
/* Change to run as another user if started with --user */

static struct passwd *check_user(const char *user)
{
#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
  struct passwd *user_info;
  uid_t user_id= geteuid();

  // Don't bother if we aren't superuser
  if (user_id)
  {
    if (user)
    {
      // Don't give a warning, if real user is same as given with --user
      user_info= getpwnam(user);
      if ((!user_info || user_id != user_info->pw_uid) &&
	  global_system_variables.log_warnings)
        sql_print_warning(
                    "One can only use the --user switch if running as root\n");
    }
    return NULL;
  }
  if (!user)
  {
    if (!opt_bootstrap)
    {
      sql_print_error("Fatal error: Please read \"Security\" section of the manual to find out how to run mysqld as root!\n");
      unireg_abort(1);
    }
    return NULL;
  }
  if (!strcmp(user,"root"))
    return NULL;                        // Avoid problem with dynamic libraries

  if (!(user_info= getpwnam(user)))
  {
    // Allow a numeric uid to be used
    const char *pos;
    for (pos= user; my_isdigit(mysqld_charset,*pos); pos++) ;
    if (*pos)                                   // Not numeric id
      goto err;
    if (!(user_info= getpwuid(atoi(user))))
      goto err;
    else
      return user_info;
  }
  else
    return user_info;

err:
  sql_print_error("Fatal error: Can't change to run as user '%s' ;  Please check that the user exists!\n",user);
#endif
  return NULL;
}

static void set_user(const char *user, struct passwd *user_info)
{
#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
  DBUG_ASSERT(user_info);
#ifdef HAVE_INITGROUPS
  initgroups((char*) user,user_info->pw_gid);
#endif
  if (setgid(user_info->pw_gid) == -1)
  {
    sql_perror("setgid");
    unireg_abort(1);
  }
  if (setuid(user_info->pw_uid) == -1)
  {
    sql_perror("setuid");
    unireg_abort(1);
  }
#endif
}


static void set_effective_user(struct passwd *user_info)
{
#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
  DBUG_ASSERT(user_info);
  if (setregid((gid_t)-1, user_info->pw_gid) == -1)
  {
    sql_perror("setregid");
    unireg_abort(1);
  }
  if (setreuid((uid_t)-1, user_info->pw_uid) == -1)
  {
    sql_perror("setreuid");
    unireg_abort(1);
  }
#endif
}


/* Change root user if started with  --chroot */

static void set_root(const char *path)
{
#if !defined(__WIN__) && !defined(__EMX__) && !defined(OS2) && !defined(__NETWARE__)
  if (chroot(path) == -1)
  {
    sql_perror("chroot");
    unireg_abort(1);
  }
  my_setwd("/", MYF(0));
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

  if (mysqld_port != 0 && !opt_disable_networking && !opt_bootstrap)
  {
    DBUG_PRINT("general",("IP Socket is %d",mysqld_port));
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
    IPaddr.sin_port = (unsigned short) htons((unsigned short) mysqld_port);

#ifndef __WIN__
    /*
      We should not use SO_REUSEADDR on windows as this would enable a
      user to open two mysqld servers with the same TCP/IP port.
    */
    (void) setsockopt(ip_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&arg,sizeof(arg));
#endif /* __WIN__ */
    if (bind(ip_sock, my_reinterpret_cast(struct sockaddr *) (&IPaddr),
	     sizeof(IPaddr)) < 0)
    {
      DBUG_PRINT("error",("Got error: %d from bind",socket_errno));
      sql_perror("Can't start server: Bind on TCP/IP port");
      sql_print_error("Do you already have another mysqld server running on port: %d ?",mysqld_port);
      unireg_abort(1);
    }
    if (listen(ip_sock,(int) back_log) < 0)
    {
      sql_perror("Can't start server: listen() on TCP/IP port");
      sql_print_error("listen() on TCP/IP failed with error %d",
		      socket_errno);
      unireg_abort(1);
    }
  }

  if ((user_info= check_user(mysqld_user)))
  {
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
    if (locked_in_memory) // getuid() == 0 here
      set_effective_user(user_info);
    else
#endif
      set_user(mysqld_user, user_info);
  }

#ifdef __NT__
  /* create named pipe */
  if (Service.IsNT() && mysqld_unix_port[0] && !opt_bootstrap &&
      opt_enable_named_pipe)
  {
    
    pipe_name[sizeof(pipe_name)-1]= 0;		/* Safety if too long string */
    strxnmov(pipe_name, sizeof(pipe_name)-1, "\\\\.\\pipe\\",
	     mysqld_unix_port, NullS);
    bzero((char*) &saPipeSecurity, sizeof(saPipeSecurity));
    bzero((char*) &sdPipeDescriptor, sizeof(sdPipeDescriptor));
    if (!InitializeSecurityDescriptor(&sdPipeDescriptor,
				      SECURITY_DESCRIPTOR_REVISION))
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
    if ((hPipe= CreateNamedPipe(pipe_name,
				PIPE_ACCESS_DUPLEX,
				PIPE_TYPE_BYTE |
				PIPE_READMODE_BYTE |
				PIPE_WAIT,
				PIPE_UNLIMITED_INSTANCES,
				(int) global_system_variables.net_buffer_length,
				(int) global_system_variables.net_buffer_length,
				NMPWAIT_USE_DEFAULT_WAIT,
				&saPipeSecurity)) == INVALID_HANDLE_VALUE)
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
  if (mysqld_unix_port[0] && !opt_bootstrap)
  {
    DBUG_PRINT("general",("UNIX Socket is %s",mysqld_unix_port));

    if (strlen(mysqld_unix_port) > (sizeof(UNIXaddr.sun_path) - 1))
    {
      sql_print_error("The socket file path is too long (> %d): %s",
                    sizeof(UNIXaddr.sun_path) - 1, mysqld_unix_port);
      unireg_abort(1);
    }
    if ((unix_sock= socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
      sql_perror("Can't start server : UNIX Socket "); /* purecov: inspected */
      unireg_abort(1);				/* purecov: inspected */
    }
    bzero((char*) &UNIXaddr, sizeof(UNIXaddr));
    UNIXaddr.sun_family = AF_UNIX;
    strmov(UNIXaddr.sun_path, mysqld_unix_port);
    (void) unlink(mysqld_unix_port);
    (void) setsockopt(unix_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&arg,
		      sizeof(arg));
    umask(0);
    if (bind(unix_sock, my_reinterpret_cast(struct sockaddr *) (&UNIXaddr),
	     sizeof(UNIXaddr)) < 0)
    {
      sql_perror("Can't start server : Bind on unix socket"); /* purecov: tested */
      sql_print_error("Do you already have another mysqld server running on socket: %s ?",mysqld_unix_port);
      unireg_abort(1);					/* purecov: tested */
    }
    umask(((~my_umask) & 0666));
#if defined(S_IFSOCK) && defined(SECURE_SOCKETS)
    (void) chmod(mysqld_unix_port,S_IFSOCK);	/* Fix solaris 2.6 bug */
#endif
    if (listen(unix_sock,(int) back_log) < 0)
      sql_print_warning("listen() on Unix socket failed with error %d",
		      socket_errno);
  }
#endif
  DBUG_PRINT("info",("server started"));
  DBUG_VOID_RETURN;
}

#endif /*!EMBEDDED_LIBRARY*/

void yyerror(const char *s)
{
  THD *thd=current_thd;
  char *yytext= (char*) thd->lex->tok_start;
  /* "parse error" changed into "syntax error" between bison 1.75 and 1.875 */
  if (strcmp(s,"parse error") == 0 || strcmp(s,"syntax error") == 0)
    s=ER(ER_SYNTAX_ERROR);
  net_printf(thd,ER_PARSE_ERROR, s, yytext ? (char*) yytext : "",
	     thd->lex->yylineno);
}


#ifndef EMBEDDED_LIBRARY
/*
  Close a connection

  SYNOPSIS
    close_connection()
    thd		Thread handle
    errcode	Error code to print to console
    lock	1 if we have have to lock LOCK_thread_count

  NOTES
    For the connection that is doing shutdown, this is called twice
*/

void close_connection(THD *thd, uint errcode, bool lock)
{
  st_vio *vio;
  DBUG_ENTER("close_connection");
  DBUG_PRINT("enter",("fd: %s  error: '%s'",
		      thd->net.vio ? vio_description(thd->net.vio) :
		      "(not connected)",
		      errcode ? ER(errcode) : ""));
  if (lock)
    (void) pthread_mutex_lock(&LOCK_thread_count);
  if ((vio=thd->net.vio) != 0)
  {
    if (errcode)
      send_error(thd, errcode, ER(errcode));	/* purecov: inspected */
    vio_close(vio);			/* vio is freed in delete thd */
  }
  if (lock)
    (void) pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_VOID_RETURN;
}
#endif /* EMBEDDED_LIBRARY */


	/* Called when a thread is aborted */
	/* ARGSUSED */

extern "C" sig_handler end_thread_signal(int sig __attribute__((unused)))
{
  THD *thd=current_thd;
  DBUG_ENTER("end_thread_signal");
  if (thd && ! thd->bootstrap)
  {
    statistic_increment(killed_threads, &LOCK_status);
    end_thread(thd,0);
  }
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
extern "C" sig_handler abort_thread(int sig __attribute__((unused)))
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
  // Save vm id of this process
  if (!opt_bootstrap)
    create_pid_file();
}

static void check_data_home(const char *path)
{}


#elif defined(__NETWARE__)

// down server event callback
void mysql_down_server_cb(void *, void *)
{
  event_flag = TRUE;  
  kill_server(0);
}


// destroy callback resources
void mysql_cb_destroy(void *)
{  
  UnRegisterEventNotification(eh);  // cleanup down event notification    	  
  NX_UNWRAP_INTERFACE(ref);
  /* Deregister NSS volume deactivation event */  
  NX_UNWRAP_INTERFACE(refneb);  	
  if (neb_consumer_id)
    UnRegisterConsumer(neb_consumer_id, NULL);
}


// initialize callbacks
void mysql_cb_init()
{
  // register for down server event
  void *handle = getnlmhandle();
  rtag_t rt= AllocateResourceTag(handle, "MySQL Down Server Callback",
                                 EventSignature);
  NX_WRAP_INTERFACE((void *)mysql_down_server_cb, 2, (void **)&ref);
  eh= RegisterForEventNotification(rt, EVENT_PRE_DOWN_SERVER,
                                   EVENT_PRIORITY_APPLICATION,
                                   NULL, ref, NULL);

  /*
    Register for volume deactivation event
    Wrap the callback function, as it is called by non-LibC thread
  */
  (void *) NX_WRAP_INTERFACE(neb_event_callback, 1, &refneb);
  registerwithneb();

  NXVmRegisterExitHandler(mysql_cb_destroy, NULL);  // clean-up
}


/* To get the name of the NetWare volume having MySQL data folder */

static void getvolumename()
{
  char *p;
  /*
    We assume that data path is already set.
    If not it won't come here. Terminate after volume name
  */
  if ((p= strchr(mysql_real_data_home, ':')))
    strmake(datavolname, mysql_real_data_home,
            (uint) (p - mysql_real_data_home));
}


/*
  Registering with NEB for NSS Volume Deactivation event
*/

static void registerwithneb()
{

  ConsumerRegistrationInfo reg_info;
    
  /* Clear NEB registration structure */
  bzero((char*) &reg_info, sizeof(struct ConsumerRegistrationInfo));

  /* Fill the NEB consumer information structure */
  reg_info.CRIVersion= 1;  	            // NEB version
  /* NEB Consumer name */
  reg_info.CRIConsumerName= (BYTE *) "MySQL Database Server";
  /* Event of interest */
  reg_info.CRIEventName= (BYTE *) "NSS.ChangeVolState.Enter";
  reg_info.CRIUserParameter= NULL;	    // Consumer Info
  reg_info.CRIEventFlags= 0;	            // Event flags
  /* Consumer NLM handle */
  reg_info.CRIOwnerID= (LoadDefinitionStructure *)getnlmhandle();
  reg_info.CRIConsumerESR= NULL;	    // No consumer ESR required
  reg_info.CRISecurityToken= 0;	            // No security token for the event
  reg_info.CRIConsumerFlags= 0;             // SMP_ENABLED_BIT;	
  reg_info.CRIFilterName= 0;	            // No event filtering
  reg_info.CRIFilterDataLength= 0;          // No filtering data
  reg_info.CRIFilterData= 0;	            // No filtering data
  /* Callback function for the event */
  (void *)reg_info.CRIConsumerCallback= (void *) refneb;
  reg_info.CRIOrder= 0;	                    // Event callback order
  reg_info.CRIConsumerType= CHECK_CONSUMER; // Consumer type

  /* Register for the event with NEB */
  if (RegisterConsumer(&reg_info))
  {
    consoleprintf("Failed to register for NSS Volume Deactivation event \n");
    return;
  }
  /* This ID is required for deregistration */
  neb_consumer_id= reg_info.CRIConsumerID;

  /* Get MySQL data volume name, stored in global variable datavolname */
  getvolumename();

  /*
    Get the NSS volume ID of the MySQL Data volume.
    Volume ID is stored in a global variable
  */
  getvolumeID((BYTE*) datavolname);	
}


/*
  Callback for NSS Volume Deactivation event
*/

ulong neb_event_callback(struct EventBlock *eblock)
{
  EventChangeVolStateEnter_s *voldata;
  extern bool nw_panic;

  voldata= (EventChangeVolStateEnter_s *)eblock->EBEventData;

  /* Deactivation of a volume */
  if ((voldata->oldState == 6 && voldata->newState == 2))
  {
    /*
      Ensure that we bring down MySQL server only for MySQL data
      volume deactivation
    */
    if (!memcmp(&voldata->volID, &datavolid, sizeof(VolumeID_t)))
    {
      consoleprintf("MySQL data volume is deactivated, shutting down MySQL Server \n");
      nw_panic = TRUE;
      event_flag= TRUE;
      kill_server(0);
 
    }
  }
  return 0;
}


/*
  Function to get NSS volume ID of the MySQL data
*/

#define ADMIN_VOL_PATH					"_ADMIN:/Volumes/"

static void getvolumeID(BYTE *volumeName)
{
  char path[zMAX_FULL_NAME];
  Key_t rootKey= 0, fileKey= 0;
  QUAD getInfoMask;
  zInfo_s info;
  STATUS status;

  /* Get the root key */
  if ((status= zRootKey(0, &rootKey)) != zOK)
  {
    consoleprintf("\nGetNSSVolumeProperties - Failed to get root key, status: %d\n.", (int) status);
    goto exit;
  }

  /*
    Get the file key. This is the key to the volume object in the
    NSS admin volumes directory.
  */

  strxmov(path, (const char *) ADMIN_VOL_PATH, (const char *) volumeName,
          NullS);
  if ((status= zOpen(rootKey, zNSS_TASK, zNSPACE_LONG|zMODE_UTF8, 
                     (BYTE *) path, zRR_READ_ACCESS, &fileKey)) != zOK)
  {
    consoleprintf("\nGetNSSVolumeProperties - Failed to get file, status: %d\n.", (int) status);
    goto exit;
  }

  getInfoMask= zGET_IDS | zGET_VOLUME_INFO ;
  if ((status= zGetInfo(fileKey, getInfoMask, sizeof(info), 
                        zINFO_VERSION_A, &info)) != zOK)
  {
    consoleprintf("\nGetNSSVolumeProperties - Failed in zGetInfo, status: %d\n.", (int) status);
    goto exit;
  }

  /* Copy the data to global variable */
  datavolid.timeLow= info.vol.volumeID.timeLow;
  datavolid.timeMid= info.vol.volumeID.timeMid;
  datavolid.timeHighAndVersion= info.vol.volumeID.timeHighAndVersion;
  datavolid.clockSeqHighAndReserved= info.vol.volumeID.clockSeqHighAndReserved;
  datavolid.clockSeqLow= info.vol.volumeID.clockSeqLow;
  /* This is guranteed to be 6-byte length (but sizeof() would be better) */
  memcpy(datavolid.node, info.vol.volumeID.node, (unsigned int) 6);
		
exit:
  if (rootKey)
    zClose(rootKey);
  if (fileKey)
    zClose(fileKey);
}


static void init_signals(void)
{
  int signals[] = {SIGINT,SIGILL,SIGFPE,SIGSEGV,SIGTERM,SIGABRT};

  for (uint i=0 ; i < sizeof(signals)/sizeof(int) ; i++)
    signal(signals[i], kill_server);
  mysql_cb_init();  // initialize callbacks

}

static void start_signal_handler(void)
{
  // Save vm id of this process
  if (!opt_bootstrap)
    create_pid_file();
  // no signal handler
}


/*
  Warn if the data is on a Traditional volume

  NOTE
    Already done by mysqld_safe
*/

static void check_data_home(const char *path)
{
}

#elif defined(__EMX__)
static void sig_reload(int signo)
{
 // Flush everything
  reload_acl_and_cache((THD*) 0,REFRESH_LOG, (TABLE_LIST*) 0, NULL);
  signal(signo, SIG_ACK);
}

static void sig_kill(int signo)
{
  if (!kill_in_progress)
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
}

static void start_signal_handler(void)
{}

static void check_data_home(const char *path)
{}

#else /* if ! __WIN__ && ! __EMX__ */

#ifdef HAVE_LINUXTHREADS
#define UNSAFE_DEFAULT_LINUX_THREADS 200
#endif

extern "C" sig_handler handle_segfault(int sig)
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
  fprintf(stderr, "key_buffer_size=%lu\n", 
          (ulong) sql_key_cache->key_cache_mem_size);
  fprintf(stderr, "read_buffer_size=%ld\n", global_system_variables.read_buff_size);
  fprintf(stderr, "max_used_connections=%ld\n", max_used_connections);
  fprintf(stderr, "max_connections=%ld\n", max_connections);
  fprintf(stderr, "threads_connected=%d\n", thread_count);
  fprintf(stderr, "It is possible that mysqld could use up to \n\
key_buffer_size + (read_buffer_size + sort_buffer_size)*max_connections = %ld K\n\
bytes of memory\n", ((ulong) sql_key_cache->key_cache_mem_size +
		     (global_system_variables.read_buff_size +
		      global_system_variables.sortbuff_size) *
		     max_connections)/ 1024);
  fprintf(stderr, "Hope that's ok; if not, decrease some variables in the equation.\n\n");

#if defined(HAVE_LINUXTHREADS)
  if (sizeof(char*) == 4 && thread_count > UNSAFE_DEFAULT_LINUX_THREADS)
  {
    fprintf(stderr, "\
You seem to be running 32-bit Linux and have %d concurrent connections.\n\
If you have not changed STACK_SIZE in LinuxThreads and built the binary \n\
yourself, LinuxThreads is quite likely to steal a part of the global heap for\n\
the thread stack. Please read http://www.mysql.com/doc/en/Linux.html\n\n",
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
  }
  fprintf(stderr, "\
The manual page at http://www.mysql.com/doc/en/Crashing.html contains\n\
information that should help you find out what is causing the crash.\n");
  fflush(stderr);
#endif /* HAVE_STACKTRACE */

 if (test_flags & TEST_CORE_ON_SIGNAL)
 {
   fprintf(stderr, "Writing a core file\n");
   fflush(stderr);
   write_core(sig);
 }
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
  struct sigaction sa;
  DBUG_ENTER("init_signals");

  if (test_flags & TEST_SIGINT)
    sigset(THR_KILL_SIGNAL,end_thread_signal);
  sigset(THR_SERVER_ALARM,print_signal_warning); // Should never be called!

  if (!(test_flags & TEST_NO_STACKTRACE) || (test_flags & TEST_CORE_ON_SIGNAL))
  {
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
    sigaction(SIGABRT, &sa, NULL);
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
    if (setrlimit(RLIMIT_CORE, &rl) && global_system_variables.log_warnings)
      sql_print_warning("setrlimit could not change the size of core files to 'infinity';  We may not be able to generate a core file on signals");
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
#ifndef IGNORE_SIGHUP_SIGQUIT
  sigaddset(&set,SIGQUIT);
  sigaddset(&set,SIGHUP);
#endif
  sigaddset(&set,SIGTERM);

  /* Fix signals if blocked by parents (can happen on Mac OS X) */
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = print_signal_warning;
  sigaction(SIGTERM, &sa, (struct sigaction*) 0);
  sa.sa_flags = 0;
  sa.sa_handler = print_signal_warning;
  sigaction(SIGHUP, &sa, (struct sigaction*) 0);
#ifdef SIGTSTP
  sigaddset(&set,SIGTSTP);
#endif
  sigaddset(&set,THR_SERVER_ALARM);
  if (test_flags & TEST_SIGINT)
    sigdelset(&set,THR_KILL_SIGNAL);		// May be SIGINT
  sigdelset(&set,THR_CLIENT_ALARM);		// For alarms
  sigprocmask(SIG_SETMASK,&set,NULL);
  pthread_sigmask(SIG_SETMASK,&set,NULL);
  DBUG_VOID_RETURN;
}


#ifndef EMBEDDED_LIBRARY
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
  pthread_attr_setstacksize(&thr_attr,thread_stack);
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
extern "C" void *signal_hand(void *arg __attribute__((unused)))
{
  sigset_t set;
  int sig;
  my_thread_init();				// Init new thread
  DBUG_ENTER("signal_hand");
  signal_thread_in_use= 1;

  /*
    Setup alarm handler
    This should actually be '+ max_number_of_slaves' instead of +10,
    but the +10 should be quite safe.
  */
  init_thr_alarm(max_connections +
		 global_system_variables.max_insert_delayed_threads + 10);
#if SIGINT != THR_KILL_SIGNAL
  if (test_flags & TEST_SIGINT)
  {
    (void) sigemptyset(&set);			// Setup up SIGINT for debug
    (void) sigaddset(&set,SIGINT);		// For debugging
    (void) pthread_sigmask(SIG_UNBLOCK,&set,NULL);
  }
#endif
  (void) sigemptyset(&set);			// Setup up SIGINT for debug
#ifdef USE_ONE_SIGNAL_HAND
  (void) sigaddset(&set,THR_SERVER_ALARM);	// For alarms
#endif
#ifndef IGNORE_SIGHUP_SIGQUIT
  (void) sigaddset(&set,SIGQUIT);
#if THR_CLIENT_ALARM != SIGHUP
  (void) sigaddset(&set,SIGHUP);
#endif
#endif
  (void) sigaddset(&set,SIGTERM);
  (void) sigaddset(&set,SIGTSTP);

  /* Save pid to this process (or thread on Linux) */
  if (!opt_bootstrap)
    create_pid_file();

#ifdef HAVE_STACK_TRACE_ON_SEGV
  if (opt_do_pstack)
  {
    sprintf(pstack_file_name,"mysqld-%lu-%%d-%%d.backtrace", (ulong)getpid());
    pstack_install_segv_action(pstack_file_name);
  }
#endif /* HAVE_STACK_TRACE_ON_SEGV */

  /*
    signal to start_signal_handler that we are ready
    This works by waiting for start_signal_handler to free mutex,
    after which we signal it that we are ready.
    At this pointer there is no other threads running, so there
    should not be any other pthread_cond_signal() calls.
  */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  (void) pthread_cond_broadcast(&COND_thread_count);

  (void) pthread_sigmask(SIG_BLOCK,&set,NULL);
  for (;;)
  {
    int error;					// Used when debugging
    if (shutdown_in_progress && !abort_loop)
    {
      sig= SIGTERM;
      error=0;
    }
    else
      while ((error=my_sigwait(&set,&sig)) == EINTR) ;
    if (cleanup_done)
    {
      my_thread_end();
      signal_thread_in_use= 0;
      pthread_exit(0);				// Safety
    }
    switch (sig) {
    case SIGTERM:
    case SIGQUIT:
    case SIGKILL:
#ifdef EXTRA_DEBUG
      sql_print_information("Got signal %d to shutdown mysqld",sig);
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
	  sql_print_error("Can't create thread to kill server");
#else
	kill_server((void*) sig);	// MIT THREAD has a alarm thread
#endif
      }
      break;
    case SIGHUP:
      if (!abort_loop)
      {
	reload_acl_and_cache((THD*) 0,
			     (REFRESH_LOG | REFRESH_TABLES | REFRESH_FAST |
			      REFRESH_STATUS | REFRESH_GRANT |
			      REFRESH_THREADS | REFRESH_HOSTS),
			     (TABLE_LIST*) 0, NULL); // Flush logs
	mysql_print_status((THD*) 0);		// Send debug some info
      }
      break;
#ifdef USE_ONE_SIGNAL_HAND
    case THR_SERVER_ALARM:
      process_alarm(sig);			// Trigger alarms.
      break;
#endif
    default:
#ifdef EXTRA_DEBUG
      sql_print_warning("Got signal: %d  error: %d",sig,error); /* purecov: tested */
#endif
      break;					/* purecov: tested */
    }
  }
  return(0);					/* purecov: deadcode */
}
#endif /*!EMBEDDED_LIBRARY*/

static void check_data_home(const char *path)
{}

#endif	/* __WIN__*/


/*
  All global error messages are sent here where the first one is stored for
  the client
*/


/* ARGSUSED */
extern "C" int my_message_sql(uint error, const char *str, myf MyFlags)
{
  THD *thd;
  DBUG_ENTER("my_message_sql");
  DBUG_PRINT("error", ("Message: '%s'", str));
  if ((thd= current_thd))
  {
    /*
      thd->lex->current_select == 0 if lex structure is not inited
      (not query command (COM_QUERY))
    */
    if (thd->lex->current_select &&
	thd->lex->current_select->no_error && !thd->is_fatal_error)
    {
      DBUG_PRINT("error", ("Error converted to warning: current_select: no_error %d  fatal_error: %d",
                           (thd->lex->current_select ?
                            thd->lex->current_select->no_error : 0),
                           (int) thd->is_fatal_error));
                           
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_ERROR, error, str);
    }
    else
    {
      NET *net= &thd->net;
      net->report_error= 1;
      if (!net->last_error[0])			// Return only first message
      {
	strmake(net->last_error, str, sizeof(net->last_error)-1);
	net->last_errno= error ? error : ER_UNKNOWN_ERROR;
      }
    }
  }
  if (!thd || MyFlags & ME_NOREFRESH)
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


extern "C" pthread_handler_decl(handle_shutdown,arg)
{
  MSG msg;
  my_thread_init();

  /* this call should create the message queue for this thread */
  PeekMessage(&msg, NULL, 1, 65534,PM_NOREMOVE);
#if !defined(EMBEDDED_LIBRARY)
  if (WaitForSingleObject(hEventShutdown,INFINITE)==WAIT_OBJECT_0)
#endif /* EMBEDDED_LIBRARY */
     kill_server(MYSQL_KILL_SIGNAL);
  return 0;
}


int STDCALL handle_kill(ulong ctrl_type)
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
extern "C" pthread_handler_decl(handle_shutdown,arg)
{
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


const char *load_default_groups[]= { 
#ifdef HAVE_NDBCLUSTER_DB
"mysql_cluster",
#endif
"mysqld","server",MYSQL_BASE_VERSION,0,0};
static const int load_default_groups_sz=
sizeof(load_default_groups)/sizeof(load_default_groups[0]);

bool open_log(MYSQL_LOG *log, const char *hostname,
	      const char *opt_name, const char *extension,
	      const char *index_file_name,
	      enum_log_type type, bool read_append,
	      bool no_auto_events, ulong max_size)
{
  char tmp[FN_REFLEN];
  if (!opt_name || !opt_name[0])
  {
    /*
      TODO: The following should be using fn_format();  We just need to
      first change fn_format() to cut the file name if it's too long.
    */
    strmake(tmp,hostname,FN_REFLEN-5);
    strmov(fn_ext(tmp),extension);
    opt_name=tmp;
  }
  // get rid of extension if the log is binary to avoid problems
  if (type == LOG_BIN)
  {
    char *p = fn_ext(opt_name);
    uint length=(uint) (p-opt_name);
    strmake(tmp,opt_name,min(length,FN_REFLEN));
    opt_name=tmp;
  }
  return log->open(opt_name, type, 0, index_file_name,
		   (read_append) ? SEQ_READ_APPEND : WRITE_CACHE,
		   no_auto_events, max_size);
}


/*
  Initialize one of the global date/time format variables

  SYNOPSIS
    init_global_datetime_format()
    format_type		What kind of format should be supported
    var_ptr		Pointer to variable that should be updated
  
  NOTES
    The default value is taken from either opt_date_time_formats[] or
    the ISO format (ANSI SQL)

  RETURN
    0 ok
    1 error
*/

bool init_global_datetime_format(timestamp_type format_type,
				 DATE_TIME_FORMAT **var_ptr)
{
  /* Get command line option */
  const char *str= opt_date_time_formats[format_type];

  if (!str)					// No specified format
  {
    str= get_date_time_format_str(&known_date_time_formats[ISO_FORMAT],
				  format_type);
    /*
      Set the "command line" option to point to the generated string so
      that we can set global formats back to default
    */
    opt_date_time_formats[format_type]= str;
  }
  if (!(*var_ptr= date_time_format_make(format_type, str, strlen(str))))
  {
    fprintf(stderr, "Wrong date/time format specifier: %s\n", str);
    return 1;
  }
  return 0;
}


static int init_common_variables(const char *conf_file_name, int argc,
				 char **argv, const char **groups)
{
  umask(((~my_umask) & 0666));
  tzset();			// Set tzname

  max_system_variables.pseudo_thread_id= (ulong)~0;
  start_time=time((time_t*) 0);
  if (init_thread_environment())
    return 1;
  mysql_init_variables();

#ifdef OS2
  {
    // fix timezone for daylight saving
    struct tm *ts = localtime(&start_time);
    if (ts->tm_isdst > 0)
      _timezone -= 3600;
  }
#endif
#ifdef HAVE_TZNAME
  {
    struct tm tm_tmp;
    localtime_r(&start_time,&tm_tmp);
    strmov(system_time_zone, tzname[tm_tmp.tm_isdst != 0 ? 1 : 0]);
  }
#endif
  /*
    We set SYSTEM time zone as reasonable default and 
    also for failure of my_tz_init() and bootstrap mode.
    If user explicitly set time zone with --default-time-zone
    option we will change this value in my_tz_init().
  */
  global_system_variables.time_zone= my_tz_SYSTEM;
  
  /*
    Init mutexes for the global MYSQL_LOG objects.
    As safe_mutex depends on what MY_INIT() does, we can't init the mutexes of
    global MYSQL_LOGs in their constructors, because then they would be inited
    before MY_INIT(). So we do it here.
  */
  mysql_log.init_pthread_objects();
  mysql_update_log.init_pthread_objects();
  mysql_slow_log.init_pthread_objects();
  mysql_bin_log.init_pthread_objects();
  
  if (gethostname(glob_hostname,sizeof(glob_hostname)-4) < 0)
    strmov(glob_hostname,"mysql");
  strmake(pidfile_name, glob_hostname, sizeof(pidfile_name)-5);
  strmov(fn_ext(pidfile_name),".pid");		// Add proper extension

  load_defaults(conf_file_name, groups, &argc, &argv);
  defaults_argv=argv;
  get_options(argc,argv);
  set_server_version();

  DBUG_PRINT("info",("%s  Ver %s for %s on %s\n",my_progname,
		     server_version, SYSTEM_TYPE,MACHINE_TYPE));

  /* connections and databases needs lots of files */
  {
    uint files, wanted_files;

    wanted_files= 10+(uint) max(max_connections*5,
				 max_connections+table_cache_size*2);
    set_if_bigger(wanted_files, open_files_limit);
    files= my_set_max_open_files(wanted_files);

    if (files < wanted_files)
    {
      if (!open_files_limit)
      {
	max_connections=	(ulong) min((files-10),max_connections);
	table_cache_size= (ulong) max((files-10-max_connections)/2,64);
	DBUG_PRINT("warning",
		   ("Changed limits: max_open_files: %u  max_connections: %ld  table_cache: %ld",
		    files, max_connections, table_cache_size));
	if (global_system_variables.log_warnings)
	  sql_print_warning("Changed limits: max_open_files: %u  max_connections: %ld  table_cache: %ld",
			files, max_connections, table_cache_size);
      }
      else if (global_system_variables.log_warnings)
	sql_print_warning("Could not increase number of max_open_files to more than %u (request: %u)", files, wanted_files);
    }
    open_files_limit= files;
  }
  unireg_init(opt_specialflag); /* Set up extern variabels */
  if (init_errmessage())	/* Read error messages from file */
    return 1;
  init_client_errs();
  lex_init();
  item_init();
  set_var_init();
  mysys_uses_curses=0;
#ifdef USE_REGEX
  regex_init(&my_charset_latin1);
#endif
  if (!(default_charset_info= get_charset_by_csname(default_character_set_name,
						    MY_CS_PRIMARY,
						    MYF(MY_WME))))
    return 1;
  if (default_collation_name)
  {
    CHARSET_INFO *default_collation;
    default_collation= get_charset_by_name(default_collation_name, MYF(0));
    if (!default_collation)
    {
      sql_print_error(ER(ER_UNKNOWN_COLLATION), default_collation_name);
      return 1;
    }
    if (!my_charset_same(default_charset_info, default_collation))
    {
      sql_print_error(ER(ER_COLLATION_CHARSET_MISMATCH),
		      default_collation_name,
		      default_charset_info->csname);
      return 1;
    }
    default_charset_info= default_collation;
  }
  /* Set collactions that depends on the default collation */
  global_system_variables.collation_server=	 default_charset_info;
  global_system_variables.collation_database=	 default_charset_info;
  global_system_variables.collation_connection=  default_charset_info;
  global_system_variables.character_set_results= default_charset_info;
  global_system_variables.character_set_client= default_charset_info;
  global_system_variables.collation_connection= default_charset_info;

  sys_init_connect.value_length= 0;
  if ((sys_init_connect.value= opt_init_connect))
    sys_init_connect.value_length= strlen(opt_init_connect);
  else
    sys_init_connect.value=my_strdup("",MYF(0));

  sys_init_slave.value_length= 0;
  if ((sys_init_slave.value= opt_init_slave))
    sys_init_slave.value_length= strlen(opt_init_slave);
  else
    sys_init_slave.value=my_strdup("",MYF(0));

  if (use_temp_pool && bitmap_init(&temp_pool,0,1024,1))
    return 1;
  if (my_dbopt_init())
    return 1;

  return 0;
}


static int init_thread_environment()
{
  (void) pthread_mutex_init(&LOCK_mysql_create_db,MY_MUTEX_INIT_SLOW);
  (void) pthread_mutex_init(&LOCK_Acl,MY_MUTEX_INIT_SLOW);
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
  (void) pthread_mutex_init(&LOCK_user_conn, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_active_mi, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_global_system_variables, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_uuid_generator, MY_MUTEX_INIT_FAST);
  (void) my_rwlock_init(&LOCK_sys_init_connect, NULL);
  (void) my_rwlock_init(&LOCK_sys_init_slave, NULL);
  (void) my_rwlock_init(&LOCK_grant, NULL);
  (void) pthread_cond_init(&COND_thread_count,NULL);
  (void) pthread_cond_init(&COND_refresh,NULL);
  (void) pthread_cond_init(&COND_thread_cache,NULL);
  (void) pthread_cond_init(&COND_flush_thread_cache,NULL);
  (void) pthread_cond_init(&COND_manager,NULL);
#ifdef HAVE_REPLICATION
  (void) pthread_mutex_init(&LOCK_rpl_status, MY_MUTEX_INIT_FAST);
  (void) pthread_cond_init(&COND_rpl_status, NULL);
#endif
  /* Parameter for threads created for connections */
  (void) pthread_attr_init(&connection_attrib);
  (void) pthread_attr_setdetachstate(&connection_attrib,
				     PTHREAD_CREATE_DETACHED);
  pthread_attr_setscope(&connection_attrib, PTHREAD_SCOPE_SYSTEM);
  if (!(opt_specialflag & SPECIAL_NO_PRIOR))
    my_pthread_attr_setprio(&connection_attrib,WAIT_PRIOR);

  if (pthread_key_create(&THR_THD,NULL) ||
      pthread_key_create(&THR_MALLOC,NULL))
  {
    sql_print_error("Can't create thread-keys");
    return 1;
  }
  return 0;
}


static void init_ssl()
{
#ifdef HAVE_OPENSSL
  if (opt_use_ssl)
  {
    /* having ssl_acceptor_fd != 0 signals the use of SSL */
    ssl_acceptor_fd= new_VioSSLAcceptorFd(opt_ssl_key, opt_ssl_cert,
					  opt_ssl_ca, opt_ssl_capath,
					  opt_ssl_cipher);
    DBUG_PRINT("info",("ssl_acceptor_fd: %lx", (long) ssl_acceptor_fd));
    if (!ssl_acceptor_fd)
      opt_use_ssl = 0;
  }
  if (des_key_file)
    load_des_key_file(des_key_file);
#endif /* HAVE_OPENSSL */
}


static int init_server_components()
{
  DBUG_ENTER("init_server_components");
  if (table_cache_init() || hostname_cache_init())
    unireg_abort(1);

  query_cache_result_size_limit(query_cache_limit);
  query_cache_set_min_res_unit(query_cache_min_res_unit);
  query_cache_resize(query_cache_size);
  randominit(&sql_rand,(ulong) start_time,(ulong) start_time/2);
  reset_floating_point_exceptions();
  init_thr_lock();
#ifdef HAVE_REPLICATION
  init_slave_list();
#endif
  /* Setup log files */
  if (opt_log)
    open_log(&mysql_log, glob_hostname, opt_logname, ".log", NullS,
	     LOG_NORMAL, 0, 0, 0);
  if (opt_update_log)
  {
    open_log(&mysql_update_log, glob_hostname, opt_update_logname, "",
	     NullS, LOG_NEW, 0, 0, 0);
    using_update_log=1;
  }
  if (opt_slow_log)
    open_log(&mysql_slow_log, glob_hostname, opt_slow_logname, "-slow.log",
 	     NullS, LOG_NORMAL, 0, 0, 0);

  if (opt_bin_log)
  {
    open_log(&mysql_bin_log, glob_hostname, opt_bin_logname, "-bin",
	     opt_binlog_index_name, LOG_BIN, 0, 0, max_binlog_size);
    using_update_log=1;
#ifdef HAVE_REPLICATION
    if (expire_logs_days)
    {
      long purge_time= time(0) - expire_logs_days*24*60*60;
      if (purge_time >= 0)
	mysql_bin_log.purge_logs_before_date(purge_time);
    }
#endif
  }
  else if (opt_log_slave_updates)
  {
      sql_print_warning("\
you need to use --log-bin to make --log-slave-updates work. \
Now disabling --log-slave-updates.");
  }

#ifdef HAVE_REPLICATION
  if (opt_log_slave_updates && replicate_same_server_id)
  {
    sql_print_error("\
using --replicate-same-server-id in conjunction with \
--log-slave-updates is impossible, it would lead to infinite loops in this \
server.");
    unireg_abort(1);
  }
#endif

  if (opt_error_log)
  {
    if (!log_error_file_ptr[0])
      fn_format(log_error_file, glob_hostname, mysql_data_home, ".err",
                MY_REPLACE_EXT); /* replace '.<domain>' by '.err', bug#4997 */
    else
      fn_format(log_error_file, log_error_file_ptr, mysql_data_home, ".err",
		MY_UNPACK_FILENAME | MY_SAFE_PATH);
    if (!log_error_file[0])
      opt_error_log= 1;				// Too long file name
    else
    {
#ifndef EMBEDDED_LIBRARY
      if (freopen(log_error_file, "a+", stdout))
#endif
	freopen(log_error_file, "a+", stderr);
    }
  }

  if (opt_innodb_safe_binlog)
  {
    if (have_innodb != SHOW_OPTION_YES)
      sql_print_warning("--innodb-safe-binlog is meaningful only if "
                      "the InnoDB storage engine is enabled in the server.");
#ifdef HAVE_INNOBASE_DB
    if (innobase_flush_log_at_trx_commit != 1)
    {
      sql_print_warning("--innodb-safe-binlog is meaningful only if "
                      "innodb_flush_log_at_trx_commit is 1; now setting it "
                      "to 1.");
      innobase_flush_log_at_trx_commit= 1;
    }
    if (innobase_unix_file_flush_method)
    {
      /*
        This option has so many values that it's hard to know which value is
        good (especially "littlesync", and on Windows... see
        srv/srv0start.c).
      */
      sql_print_warning("--innodb-safe-binlog requires that "
                      "the innodb_flush_method actually synchronizes the "
                      "InnoDB log to disk; it is your responsibility "
                      "to verify that the method you chose does it.");
    }
    if (sync_binlog_period != 1)
    {
      sql_print_warning("--innodb-safe-binlog is meaningful only if "
                      "the global sync_binlog variable is 1; now setting it "
                      "to 1.");
      sync_binlog_period= 1;
    }
#endif
  }

  if (ha_init())
  {
    sql_print_error("Can't init databases");
    unireg_abort(1);
  }
  if (opt_myisam_log)
    (void) mi_log(1);

  /*
    Now that InnoDB is initialized, we can know the last good binlog position
    and cut the binlog if needed. This function does nothing if there was no
    crash recovery by InnoDB.
  */
  if (opt_innodb_safe_binlog)
  {
    /* not fatal if fails (but print errors) */
    mysql_bin_log.cut_spurious_tail();
  }
  mysql_bin_log.report_pos_in_innodb();

  /* call ha_init_key_cache() on all key caches to init them */
  process_key_caches(&ha_init_key_cache);
  /* We must set dflt_key_cache in case we are using ISAM tables */
  dflt_key_cache= sql_key_cache;

#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT) && !defined(EMBEDDED_LIBRARY)
  if (locked_in_memory && !getuid())
  {
    if (setreuid((uid_t)-1, 0) == -1)
    {                        // this should never happen
      sql_perror("setreuid");
      unireg_abort(1);
    }
    if (mlockall(MCL_CURRENT))
    {
      if (global_system_variables.log_warnings)
	sql_print_warning("Failed to lock memory. Errno: %d\n",errno);
      locked_in_memory= 0;
    }
    if (user_info)
      set_user(mysqld_user, user_info);
  }
  else
#endif
    locked_in_memory=0;

  ft_init_stopwords();

  init_max_user_conn();
  init_update_queries();
  DBUG_RETURN(0);
}


static void create_maintenance_thread()
{
  if (
#ifdef HAVE_BERKELEY_DB
      (have_berkeley_db == SHOW_OPTION_YES) ||
#endif
      (flush_time && flush_time != ~(ulong) 0L))
  {
    pthread_t hThread;
    if (pthread_create(&hThread,&connection_attrib,handle_manager,0))
      sql_print_warning("Can't create thread to manage maintenance");
  }
}


static void create_shutdown_thread()
{
#if !defined(EMBEDDED_LIBRARY)
#ifdef __WIN__
  hEventShutdown=CreateEvent(0, FALSE, FALSE, shutdown_event_name);
  pthread_t hThread;
  if (pthread_create(&hThread,&connection_attrib,handle_shutdown,0))
    sql_print_warning("Can't create thread to handle shutdown requests");

  // On "Stop Service" we have to do regular shutdown
  Service.SetShutdownEvent(hEventShutdown);
#endif
#ifdef OS2
  pthread_cond_init(&eventShutdown, NULL);
  pthread_t hThread;
  if (pthread_create(&hThread,&connection_attrib,handle_shutdown,0))
    sql_print_warning("Can't create thread to handle shutdown requests");
#endif
#endif // EMBEDDED_LIBRARY 
}


#if defined(__NT__) || defined(HAVE_SMEM)
static void handle_connections_methods()
{
  pthread_t hThread;
  DBUG_ENTER("handle_connections_methods");
#ifdef __NT__
  if (hPipe == INVALID_HANDLE_VALUE &&
      (!have_tcpip || opt_disable_networking) &&
      !opt_enable_shared_memory)
  {
    sql_print_error("TCP/IP, --shared-memory, or --named-pipe should be configured on NT OS");
    unireg_abort(1);				// Will not return
  }
#endif

  pthread_mutex_lock(&LOCK_thread_count);
  (void) pthread_cond_init(&COND_handler_count,NULL);
  handler_count=0;
#ifdef __NT__
  if (hPipe != INVALID_HANDLE_VALUE)
  {
    handler_count++;
    if (pthread_create(&hThread,&connection_attrib,
		       handle_connections_namedpipes, 0))
    {
      sql_print_warning("Can't create thread to handle named pipes");
      handler_count--;
    }
  }
#endif /* __NT__ */
  if (have_tcpip && !opt_disable_networking)
  {
    handler_count++;
    if (pthread_create(&hThread,&connection_attrib,
		       handle_connections_sockets, 0))
    {
      sql_print_warning("Can't create thread to handle TCP/IP");
      handler_count--;
    }
  }
#ifdef HAVE_SMEM
  if (opt_enable_shared_memory)
  {
    handler_count++;
    if (pthread_create(&hThread,&connection_attrib,
		       handle_connections_shared_memory, 0))
    {
      sql_print_warning("Can't create thread to handle shared memory");
      handler_count--;
    }
  }
#endif 

  while (handler_count > 0)
    pthread_cond_wait(&COND_handler_count,&LOCK_thread_count);
  pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_VOID_RETURN;
}

void decrement_handler_count()
{
  pthread_mutex_lock(&LOCK_thread_count);
  handler_count--;
  pthread_mutex_unlock(&LOCK_thread_count);
  pthread_cond_signal(&COND_handler_count);
}
#else
#define decrement_handler_count()
#endif /* defined(__NT__) || defined(HAVE_SMEM) */


#ifndef EMBEDDED_LIBRARY
#ifdef __WIN__
int win_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{

  DEBUGGER_OFF;

  MY_INIT(argv[0]);		// init my_sys library & pthreads

#ifdef _CUSTOMSTARTUPCONFIG_
  if (_cust_check_startup())
  {
    / * _cust_check_startup will report startup failure error * /
    exit( 1 );
  }
#endif

  if (init_common_variables(MYSQL_CONFIG_NAME,
			    argc, argv, load_default_groups))
    unireg_abort(1);				// Will do exit

  init_signals();
  if (!(opt_specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),CONNECT_PRIOR);
  pthread_attr_setstacksize(&connection_attrib,thread_stack);
#ifdef HAVE_PTHREAD_ATTR_GETSTACKSIZE
  {
    /* Retrieve used stack size;  Needed for checking stack overflows */
    size_t stack_size= 0;
    pthread_attr_getstacksize(&connection_attrib, &stack_size);
    /* We must check if stack_size = 0 as Solaris 2.9 can return 0 here */
    if (stack_size && stack_size < thread_stack)
    {
      if (global_system_variables.log_warnings)
	sql_print_warning("Asked for %ld thread stack, but got %ld",
			thread_stack, stack_size);
      thread_stack= stack_size;
    }
  }
#endif
  thread_stack_min=thread_stack - STACK_MIN_SIZE;

  (void) thr_setconcurrency(concurrency);	// 10 by default

  /*
    Ensure that lower_case_table_names is set on system where we have case
    insensitive names.  If this is not done the users MyISAM tables will
    get corrupted if accesses with names of different case.
  */
  DBUG_PRINT("info", ("lower_case_table_names: %d", lower_case_table_names));
  if (!lower_case_table_names &&
      (lower_case_file_system=
       (test_if_case_insensitive(mysql_real_data_home) == 1)))
  {
    if (lower_case_table_names_used)
    {
      if (global_system_variables.log_warnings)
	sql_print_warning("\
You have forced lower_case_table_names to 0 through a command-line \
option, even though your file system '%s' is case insensitive.  This means \
that you can corrupt a MyISAM table by accessing it with different cases. \
You should consider changing lower_case_table_names to 1 or 2",
			mysql_real_data_home);
    }
    else
    {
      if (global_system_variables.log_warnings)
	sql_print_warning("Setting lower_case_table_names=2 because file system for %s is case insensitive", mysql_real_data_home);
      lower_case_table_names= 2;
    }
  }

  select_thread=pthread_self();
  select_thread_in_use=1;
  init_ssl();

#ifdef HAVE_LIBWRAP
  libwrapName= my_progname+dirname_length(my_progname);
  openlog(libwrapName, LOG_PID, LOG_AUTH);
#endif

  /*
    We have enough space for fiddling with the argv, continue
  */
  check_data_home(mysql_real_data_home);
  if (my_setwd(mysql_real_data_home,MYF(MY_WME)))
  {
    unireg_abort(1);				/* purecov: inspected */
  }
  mysql_data_home= mysql_data_home_buff;
  mysql_data_home[0]=FN_CURLIB;		// all paths are relative from here
  mysql_data_home[1]=0;
  server_init();

  if (opt_bin_log && !server_id)
  {
    server_id= !master_host ? 1 : 2;
#ifdef EXTRA_DEBUG
    switch (server_id) {
    case 1:
      sql_print_warning("\
You have enabled the binary log, but you haven't set server-id to \
a non-zero value: we force server id to 1; updates will be logged to the \
binary log, but connections from slaves will not be accepted.");
      break;
    case 2:
      sql_print_warning("\
You should set server-id to a non-0 value if master_host is set; \
we force server id to 2, but this MySQL server will not act as a slave.");
      break;
    }
#endif
  }

  if (init_server_components())
    exit(1);

#ifdef __WIN__
  if (!opt_console)
  {
    freopen(log_error_file,"a+",stdout);
    freopen(log_error_file,"a+",stderr);
    FreeConsole();				// Remove window
  }
#endif

  /*
    init signals & alarm
    After this we can't quit by a simple unireg_abort
  */
  error_handler_hook = my_message_sql;
  start_signal_handler();				// Creates pidfile
  if (acl_init((THD *)0, opt_noacl) || 
      my_tz_init((THD *)0, default_tz_name, opt_bootstrap))
  {
    abort_loop=1;
    select_thread_in_use=0;
#ifndef __NETWARE__
    (void) pthread_kill(signal_thread, MYSQL_KILL_SIGNAL);
#endif /* __NETWARE__ */
    
    if (!opt_bootstrap)
      (void) my_delete(pidfile_name,MYF(MY_WME));	// Not needed anymore

    if (unix_sock != INVALID_SOCKET)
      unlink(mysqld_unix_port);
    exit(1);
  }
  if (!opt_noacl)
    (void) grant_init((THD *)0);

#ifdef HAVE_DLOPEN
  if (!opt_noacl)
    udf_init();
#endif
  if (opt_bootstrap) /* If running with bootstrap, do not start replication. */
    opt_skip_slave_start= 1;
  /* init_slave() must be called after the thread keys are created */
  init_slave();

  if (opt_bootstrap)
  {
    int error=bootstrap(stdin);
    end_thr_alarm(1);				// Don't allow alarms
    unireg_abort(error ? 1 : 0);
  }
  if (opt_init_file)
  {
    if (read_init_file(opt_init_file))
    {
      end_thr_alarm(1);				// Don't allow alarms
      unireg_abort(1);
    }
  }

  create_shutdown_thread();
  create_maintenance_thread();

  printf(ER(ER_READY),my_progname,server_version,
	 ((unix_sock == INVALID_SOCKET) ? (char*) "" : mysqld_unix_port),
	 mysqld_port);
  if (MYSQL_COMPILATION_COMMENT[0] != '\0')
    fputs("  " MYSQL_COMPILATION_COMMENT, stdout);
  putchar('\n');
  fflush(stdout);

#if defined(__NT__) || defined(HAVE_SMEM)
  handle_connections_methods();
#else
#ifdef __WIN__
  if ( !have_tcpip || opt_disable_networking)
  {
    sql_print_error("TCP/IP unavailable or disabled with --skip-networking; no available interfaces");
    unireg_abort(1);
  }
#endif
  handle_connections_sockets(0);
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
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  (void) pthread_cond_broadcast(&COND_thread_count);
#ifdef EXTRA_DEBUG2
  sql_print_error("After lock_thread_count");
#endif
#endif /* __WIN__ */

  /* Wait until cleanup is done */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  while (!ready_to_exit)
    pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
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
#ifndef __NETWARE__
  {
    uint i;
    /*
      Wait up to 10 seconds for signal thread to die. We use this mainly to
      avoid getting warnings that my_thread_end has not been called
    */
    for (i= 0 ; i < 100 && signal_thread_in_use; i++)
    {
      if (pthread_kill(signal_thread, MYSQL_KILL_SIGNAL))
	break;
      my_sleep(100);				// Give it time to die
    }
  }
#endif
  clean_up_mutexes();
  my_end(opt_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
 
  exit(0);
  return(0);					/* purecov: deadcode */
}

#endif /* EMBEDDED_LIBRARY */


/****************************************************************************
  Main and thread entry function for Win32
  (all this is needed only to run mysqld as a service on WinNT)
****************************************************************************/

#if defined(__WIN__) && !defined(EMBEDDED_LIBRARY)
int mysql_service(void *p)
{
  if (use_opt_args)
    win_main(opt_argc, opt_argv);
  else
    win_main(Service.my_argc, Service.my_argv);
  return 0;
}


/* Quote string if it contains space, else copy */

static char *add_quoted_string(char *to, const char *from, char *to_end)
{
  uint length= (uint) (to_end-to);

  if (!strchr(from, ' '))
    return strnmov(to, from, length);
  return strxnmov(to, length, "\"", from, "\"", NullS);
}


/*
  Handle basic handling of services, like installation and removal

  SYNOPSIS
    default_service_handling()
    argv		Pointer to argument list
    servicename		Internal name of service
    displayname		Display name of service (in taskbar ?)
    file_path		Path to this program
    startup_option	Startup option to mysqld

  RETURN VALUES
    0		option handled
    1		Could not handle option
 */

static bool
default_service_handling(char **argv,
			 const char *servicename,
			 const char *displayname,
			 const char *file_path,
			 const char *extra_opt)
{
  char path_and_service[FN_REFLEN+FN_REFLEN+32], *pos, *end;
  end= path_and_service + sizeof(path_and_service)-3;

  /* We have to quote filename if it contains spaces */
  pos= add_quoted_string(path_and_service, file_path, end);
  if (*extra_opt)
  {
    /* Add (possible quoted) option after file_path */
    *pos++= ' ';
    pos= add_quoted_string(pos, extra_opt, end);
  }
  /* We must have servicename last */
  *pos++= ' ';
  (void) add_quoted_string(pos, servicename, end);

  if (Service.got_service_option(argv, "install"))
  {
    Service.Install(1, servicename, displayname, path_and_service);
    return 0;
  }
  if (Service.got_service_option(argv, "install-manual"))
  {
    Service.Install(0, servicename, displayname, path_and_service);
    return 0;
  }
  if (Service.got_service_option(argv, "remove"))
  {
    Service.Remove(servicename);
    return 0;
  }
  return 1;
}


int main(int argc, char **argv)
{

  /* When several instances are running on the same machine, we
     need to have an  unique  named  hEventShudown  through the
     application PID e.g.: MySQLShutdown1890; MySQLShutdown2342
  */ 
  int10_to_str((int) GetCurrentProcessId(),strmov(shutdown_event_name,
          "MySQLShutdown"), 10);
  
  /* Must be initialized early for comparison of service name */
  system_charset_info= &my_charset_utf8_general_ci;

  if (Service.GetOS())	/* true NT family */
  {
    char file_path[FN_REFLEN];
    my_path(file_path, argv[0], "");		      /* Find name in path */
    fn_format(file_path,argv[0],file_path,"",
	      MY_REPLACE_DIR | MY_UNPACK_FILENAME | MY_RESOLVE_SYMLINKS);

    if (argc == 2)
    {
      if (!default_service_handling(argv, MYSQL_SERVICENAME, MYSQL_SERVICENAME,
				   file_path, ""))
	return 0;
      if (Service.IsService(argv[1]))        /* Start an optional service */
      {
	/*
	  Only add the service name to the groups read from the config file
	  if it's not "MySQL". (The default service name should be 'mysqld'
	  but we started a bad tradition by calling it MySQL from the start
	  and we are now stuck with it.
	*/
	if (my_strcasecmp(system_charset_info, argv[1],"mysql"))
	  load_default_groups[load_default_groups_sz-2]= argv[1];
        start_mode= 1;
        Service.Init(argv[1], mysql_service);
        return 0;
      }
    }
    else if (argc == 3) /* install or remove any optional service */
    {
      if (!default_service_handling(argv, argv[2], argv[2], file_path, ""))
	return 0;
      if (Service.IsService(argv[2]))
      {
	/*
	  mysqld was started as
	  mysqld --defaults-file=my_path\my.ini service-name
	*/
	use_opt_args=1;
	opt_argc= 2;				// Skip service-name
	opt_argv=argv;
	start_mode= 1;
	if (my_strcasecmp(system_charset_info, argv[2],"mysql"))
	  load_default_groups[load_default_groups_sz-2]= argv[2];
	Service.Init(argv[2], mysql_service);
	return 0;
      }
    }
    else if (argc == 4)
    {
      /*
	Install an optional service with optional config file
	mysqld --install-manual mysqldopt --defaults-file=c:\miguel\my.ini
      */
      if (!default_service_handling(argv, argv[2], argv[2], file_path,
				    argv[3]))
	return 0;
    }
    else if (argc == 1 && Service.IsService(MYSQL_SERVICENAME))
    {
      /* start the default service */
      start_mode= 1;
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
  int error= 0;
  DBUG_ENTER("bootstrap");

  THD *thd= new THD;
  thd->bootstrap=1;
  thd->client_capabilities=0;
  my_net_init(&thd->net,(st_vio*) 0);
  thd->max_client_packet_length= thd->net.max_packet;
  thd->master_access= ~0;
  thd->thread_id=thread_id++;
  thread_count++;

  bootstrap_file=file;
#ifndef EMBEDDED_LIBRARY			// TODO:  Enable this
  if (pthread_create(&thd->real_id,&connection_attrib,handle_bootstrap,
		     (void*) thd))
  {
    sql_print_warning("Can't create thread to handle bootstrap");
    DBUG_RETURN(-1);
  }
  /* Wait for thread to die */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  while (thread_count)
  {
    (void) pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
    DBUG_PRINT("quit",("One thread died (count=%u)",thread_count));
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);
#else
  thd->mysql= 0;
  handle_bootstrap((void *)thd);
#endif

  error= thd->is_fatal_error;
#ifndef EMBEDDED_LIBRARY
  net_end(&thd->net);
#endif
  thd->cleanup();
  delete thd;
  DBUG_RETURN(error);
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


#ifndef EMBEDDED_LIBRARY
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
    close_connection(thd, ER_CON_COUNT_ERROR, 1);
    delete thd;
    DBUG_VOID_RETURN;
  }
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id=thread_id++;

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
      if (thread_count-delayed_insert_threads > max_used_connections)
        max_used_connections=thread_count-delayed_insert_threads;
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
	statistic_increment(aborted_connects,&LOCK_status);
	net_printf(thd,ER_CANT_CREATE_THREAD,error);
	(void) pthread_mutex_lock(&LOCK_thread_count);
	close_connection(thd,0,0);
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
#endif /* EMBEDDED_LIBRARY */


#ifdef SIGNALS_DONT_BREAK_READ
inline void kill_broken_server()
{
  /* hack to get around signals ignored in syscalls for problem OS's */
  if (
#if !defined(__NETWARE__)
      unix_sock == INVALID_SOCKET ||
#endif
      (!opt_disable_networking && ip_sock == INVALID_SOCKET))
  {
    select_thread_in_use = 0;
#ifdef __NETWARE__
    kill_server(MYSQL_KILL_SIGNAL); /* never returns */
#else
    kill_server((void*)MYSQL_KILL_SIGNAL); /* never returns */
#endif /* __NETWARE__ */
  }
}
#define MAYBE_BROKEN_SYSCALL kill_broken_server();
#else
#define MAYBE_BROKEN_SYSCALL
#endif

	/* Handle new connections and spawn new process to handle them */

#ifndef EMBEDDED_LIBRARY
extern "C" pthread_handler_decl(handle_connections_sockets,
				arg __attribute__((unused)))
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
#ifdef HPUX10
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
#endif	/* HPUX10 */
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
#ifdef __NETWARE__ 
      // TODO: temporary fix, waiting for TCP/IP fix - DEFECT000303149
      if ((new_sock == INVALID_SOCKET) && (socket_errno == EINVAL))
      {
        kill_server(SIGTERM);
      }
#endif
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
      thd->host=(char*) my_localhost;
#ifdef __WIN__
    /* Set default wait_timeout */
    ulong wait_timeout= global_system_variables.net_wait_timeout * 1000;
    (void) setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&wait_timeout,
                    sizeof(wait_timeout));
#endif
    create_new_thread(thd);
  }

#ifdef OS2
  // kill server must be invoked from thread 1!
  kill_server(MYSQL_KILL_SIGNAL);
#endif
  decrement_handler_count();
  DBUG_RETURN(0);
}


#ifdef __NT__
extern "C" pthread_handler_decl(handle_connections_namedpipes,arg)
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
      if ((hPipe = CreateNamedPipe(pipe_name,
				   PIPE_ACCESS_DUPLEX,
				   PIPE_TYPE_BYTE |
				   PIPE_READMODE_BYTE |
				   PIPE_WAIT,
				   PIPE_UNLIMITED_INSTANCES,
				   (int) global_system_variables.net_buffer_length,
				   (int) global_system_variables.net_buffer_length,
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
    if ((hPipe = CreateNamedPipe(pipe_name,
				 PIPE_ACCESS_DUPLEX,
				 PIPE_TYPE_BYTE |
				 PIPE_READMODE_BYTE |
				 PIPE_WAIT,
				 PIPE_UNLIMITED_INSTANCES,
				 (int) global_system_variables.net_buffer_length,
				 (int) global_system_variables.net_buffer_length,
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
      close_connection(thd, ER_OUT_OF_RESOURCES, 1);
      delete thd;
      continue;
    }
    /* host name is unknown */
    thd->host = my_strdup(my_localhost,MYF(0)); /* Host is unknown */
    create_new_thread(thd);
  }

  decrement_handler_count();
  DBUG_RETURN(0);
}
#endif /* __NT__ */


/*
  Thread of shared memory's service

  SYNOPSIS
    pthread_handler_decl()
    handle_connections_shared_memory Thread handle
    arg                              Arguments of thread
*/

#ifdef HAVE_SMEM
pthread_handler_decl(handle_connections_shared_memory,arg)
{
  /* file-mapping object, use for create shared memory */
  HANDLE handle_connect_file_map= 0;
  char  *handle_connect_map= 0;  		// pointer on shared memory
  HANDLE event_connect_answer= 0;
  ulong smem_buffer_length= shared_memory_buffer_length + 4;
  ulong connect_number= 1;
  char tmp[63];
  char *suffix_pos;
  char connect_number_char[22], *p;
  const char *errmsg= 0;
  my_thread_init();
  DBUG_ENTER("handle_connections_shared_memorys");
  DBUG_PRINT("general",("Waiting for allocated shared memory."));

  /*
    The name of event and file-mapping events create agree next rule:
      shared_memory_base_name+unique_part
    Where:
      shared_memory_base_name is unique value for each server
      unique_part is unique value for each object (events and file-mapping)
  */
  suffix_pos= strxmov(tmp,shared_memory_base_name,"_",NullS);
  strmov(suffix_pos, "CONNECT_REQUEST");
  if ((smem_event_connect_request= CreateEvent(0,FALSE,FALSE,tmp)) == 0)
  {
    errmsg= "Could not create request event";
    goto error;
  }
  strmov(suffix_pos, "CONNECT_ANSWER");
  if ((event_connect_answer= CreateEvent(0,FALSE,FALSE,tmp)) == 0)
  {
    errmsg="Could not create answer event";
    goto error;
  }
  strmov(suffix_pos, "CONNECT_DATA");
  if ((handle_connect_file_map= CreateFileMapping(INVALID_HANDLE_VALUE,0,
						   PAGE_READWRITE,
						   0,sizeof(connect_number),
						   tmp)) == 0)
  {
    errmsg= "Could not create file mapping";
    goto error;
  }
  if ((handle_connect_map= (char *)MapViewOfFile(handle_connect_file_map,
						  FILE_MAP_WRITE,0,0,
						  sizeof(DWORD))) == 0)
  {
    errmsg= "Could not create shared memory service";
    goto error;
  }

  while (!abort_loop)
  {
    /* Wait a request from client */
    WaitForSingleObject(smem_event_connect_request,INFINITE);

    /*
       it can be after shutdown command
    */
    if (abort_loop) 
      goto error;

    HANDLE handle_client_file_map= 0;
    char  *handle_client_map= 0;
    HANDLE event_client_wrote= 0;
    HANDLE event_client_read= 0;    // for transfer data server <-> client
    HANDLE event_server_wrote= 0;
    HANDLE event_server_read= 0;
    THD *thd= 0;

    p= int10_to_str(connect_number, connect_number_char, 10);
    /*
      The name of event and file-mapping events create agree next rule:
        shared_memory_base_name+unique_part+number_of_connection
        Where:
	  shared_memory_base_name is uniquel value for each server
	  unique_part is unique value for each object (events and file-mapping)
	  number_of_connection is connection-number between server and client
    */
    suffix_pos= strxmov(tmp,shared_memory_base_name,"_",connect_number_char,
			 "_",NullS);
    strmov(suffix_pos, "DATA");
    if ((handle_client_file_map= CreateFileMapping(INVALID_HANDLE_VALUE,0,
						    PAGE_READWRITE,0,
						    smem_buffer_length,
						    tmp)) == 0)
    {
      errmsg= "Could not create file mapping";
      goto errorconn;
    }
    if ((handle_client_map= (char*)MapViewOfFile(handle_client_file_map,
						  FILE_MAP_WRITE,0,0,
						  smem_buffer_length)) == 0)
    {
      errmsg= "Could not create memory map";
      goto errorconn;
    }
    strmov(suffix_pos, "CLIENT_WROTE");
    if ((event_client_wrote= CreateEvent(0,FALSE,FALSE,tmp)) == 0)
    {
      errmsg= "Could not create client write event";
      goto errorconn;
    }
    strmov(suffix_pos, "CLIENT_READ");
    if ((event_client_read= CreateEvent(0,FALSE,FALSE,tmp)) == 0)
    {
      errmsg= "Could not create client read event";
      goto errorconn;
    }
    strmov(suffix_pos, "SERVER_READ");
    if ((event_server_read= CreateEvent(0,FALSE,FALSE,tmp)) == 0)
    {
      errmsg= "Could not create server read event";
      goto errorconn;
    }
    strmov(suffix_pos, "SERVER_WROTE");
    if ((event_server_wrote= CreateEvent(0,FALSE,FALSE,tmp)) == 0)
    {
      errmsg= "Could not create server write event";
      goto errorconn;
    }
    if (abort_loop)
      goto errorconn;
    if (!(thd= new THD))
      goto errorconn;
    /* Send number of connection to client */
    int4store(handle_connect_map, connect_number);
    if (!SetEvent(event_connect_answer))
    {
      errmsg= "Could not send answer event";
      goto errorconn;
    }
    /* Set event that client should receive data */
    if (!SetEvent(event_client_read))
    {
      errmsg= "Could not set client to read mode";
      goto errorconn;
    }
    if (!(thd->net.vio= vio_new_win32shared_memory(&thd->net,
						   handle_client_file_map,
						   handle_client_map,
						   event_client_wrote,
						   event_client_read,
						   event_server_wrote,
						   event_server_read)) ||
	my_net_init(&thd->net, thd->net.vio))
    {
      close_connection(thd, ER_OUT_OF_RESOURCES, 1);
      errmsg= 0;
      goto errorconn;
    }
    thd->host= my_strdup(my_localhost,MYF(0)); /* Host is unknown */
    create_new_thread(thd);
    connect_number++;
    continue;

errorconn:
    /* Could not form connection;  Free used handlers/memort and retry */
    if (errmsg)
    {
      char buff[180];
      strxmov(buff, "Can't create shared memory connection: ", errmsg, ".",
	      NullS);
      sql_perror(buff);
    }
    if (handle_client_file_map) CloseHandle(handle_client_file_map);
    if (handle_client_map)	UnmapViewOfFile(handle_client_map);
    if (event_server_wrote)	CloseHandle(event_server_wrote);
    if (event_server_read)	CloseHandle(event_server_read);
    if (event_client_wrote)	CloseHandle(event_client_wrote);
    if (event_client_read)	CloseHandle(event_client_read);
    delete thd;
  }

  /* End shared memory handling */
error:
  if (errmsg)
  {
    char buff[180];
    strxmov(buff, "Can't create shared memory service: ", errmsg, ".", NullS);
    sql_perror(buff);
  }
  if (handle_connect_map)	UnmapViewOfFile(handle_connect_map);
  if (handle_connect_file_map)	CloseHandle(handle_connect_file_map);
  if (event_connect_answer)	CloseHandle(event_connect_answer);
  if (smem_event_connect_request) CloseHandle(smem_event_connect_request);

  decrement_handler_count();
  DBUG_RETURN(0);
}
#endif /* HAVE_SMEM */
#endif /* EMBEDDED_LIBRARY */


/****************************************************************************
  Handle start options
******************************************************************************/

enum options_mysqld
{
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
  OPT_SKIP_HOST_CACHE,         OPT_SHORT_LOG_FORMAT,
  OPT_FLUSH,                   OPT_SAFE,
  OPT_BOOTSTRAP,               OPT_SKIP_SHOW_DB,
  OPT_STORAGE_ENGINE,          OPT_INIT_FILE,
  OPT_DELAY_KEY_WRITE_ALL,     OPT_SLOW_QUERY_LOG,
  OPT_DELAY_KEY_WRITE,	       OPT_CHARSETS_DIR,
  OPT_BDB_HOME,                OPT_BDB_LOG,
  OPT_BDB_TMP,                 OPT_BDB_SYNC,
  OPT_BDB_LOCK,                OPT_BDB,
  OPT_BDB_NO_RECOVER,	    OPT_BDB_SHARED,
  OPT_MASTER_HOST,             OPT_MASTER_USER,
  OPT_MASTER_PASSWORD,         OPT_MASTER_PORT,
  OPT_MASTER_INFO_FILE,        OPT_MASTER_CONNECT_RETRY,
  OPT_MASTER_RETRY_COUNT,
  OPT_MASTER_SSL,              OPT_MASTER_SSL_KEY,
  OPT_MASTER_SSL_CERT,         OPT_MASTER_SSL_CAPATH,
  OPT_MASTER_SSL_CIPHER,       OPT_MASTER_SSL_CA,
  OPT_SQL_BIN_UPDATE_SAME,     OPT_REPLICATE_DO_DB,
  OPT_REPLICATE_IGNORE_DB,     OPT_LOG_SLAVE_UPDATES,
  OPT_BINLOG_DO_DB,            OPT_BINLOG_IGNORE_DB,
  OPT_WANT_CORE,               OPT_CONCURRENT_INSERT,
  OPT_MEMLOCK,                 OPT_MYISAM_RECOVER,
  OPT_REPLICATE_REWRITE_DB,    OPT_SERVER_ID,
  OPT_SKIP_SLAVE_START,        OPT_SKIP_INNOBASE,
  OPT_SAFEMALLOC_MEM_LIMIT,    OPT_REPLICATE_DO_TABLE,
  OPT_REPLICATE_IGNORE_TABLE,  OPT_REPLICATE_WILD_DO_TABLE,
  OPT_REPLICATE_WILD_IGNORE_TABLE, OPT_REPLICATE_SAME_SERVER_ID,
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
  OPT_INNODB_FILE_PER_TABLE, OPT_CRASH_BINLOG_INNODB,
  OPT_INNODB_LOCKS_UNSAFE_FOR_BINLOG,
  OPT_SAFE_SHOW_DB, OPT_INNODB_SAFE_BINLOG,
  OPT_INNODB, OPT_ISAM,
  OPT_NDBCLUSTER, OPT_NDB_CONNECTSTRING, OPT_NDB_USE_EXACT_COUNT,
  OPT_NDB_FORCE_SEND, OPT_NDB_AUTOINCREMENT_PREFETCH_SZ,
  OPT_SKIP_SAFEMALLOC,
  OPT_TEMP_POOL, OPT_TX_ISOLATION,
  OPT_SKIP_STACK_TRACE, OPT_SKIP_SYMLINKS,
  OPT_MAX_BINLOG_DUMP_EVENTS, OPT_SPORADIC_BINLOG_DUMP_FAIL,
  OPT_SAFE_USER_CREATE, OPT_SQL_MODE,
  OPT_HAVE_NAMED_PIPE,
  OPT_DO_PSTACK, OPT_REPORT_HOST,
  OPT_REPORT_USER, OPT_REPORT_PASSWORD, OPT_REPORT_PORT,
  OPT_SHOW_SLAVE_AUTH_INFO,
  OPT_SLAVE_LOAD_TMPDIR, OPT_NO_MIX_TYPE,
  OPT_RPL_RECOVERY_RANK,OPT_INIT_RPL_ROLE,
  OPT_RELAY_LOG, OPT_RELAY_LOG_INDEX, OPT_RELAY_LOG_INFO_FILE,
  OPT_SLAVE_SKIP_ERRORS, OPT_DES_KEY_FILE, OPT_LOCAL_INFILE,
  OPT_SSL_SSL, OPT_SSL_KEY, OPT_SSL_CERT, OPT_SSL_CA,
  OPT_SSL_CAPATH, OPT_SSL_CIPHER,
  OPT_BACK_LOG, OPT_BINLOG_CACHE_SIZE,
  OPT_CONNECT_TIMEOUT, OPT_DELAYED_INSERT_TIMEOUT,
  OPT_DELAYED_INSERT_LIMIT, OPT_DELAYED_QUEUE_SIZE,
  OPT_FLUSH_TIME, OPT_FT_MIN_WORD_LEN, OPT_FT_BOOLEAN_SYNTAX,
  OPT_FT_MAX_WORD_LEN, OPT_FT_QUERY_EXPANSION_LIMIT, OPT_FT_STOPWORD_FILE,
  OPT_INTERACTIVE_TIMEOUT, OPT_JOIN_BUFF_SIZE,
  OPT_KEY_BUFFER_SIZE, OPT_KEY_CACHE_BLOCK_SIZE,
  OPT_KEY_CACHE_DIVISION_LIMIT, OPT_KEY_CACHE_AGE_THRESHOLD,
  OPT_LONG_QUERY_TIME,
  OPT_LOWER_CASE_TABLE_NAMES, OPT_MAX_ALLOWED_PACKET,
  OPT_MAX_BINLOG_CACHE_SIZE, OPT_MAX_BINLOG_SIZE,
  OPT_MAX_CONNECTIONS, OPT_MAX_CONNECT_ERRORS,
  OPT_MAX_DELAYED_THREADS, OPT_MAX_HEP_TABLE_SIZE,
  OPT_MAX_JOIN_SIZE, OPT_MAX_RELAY_LOG_SIZE, OPT_MAX_SORT_LENGTH, 
  OPT_MAX_SEEKS_FOR_KEY, OPT_MAX_TMP_TABLES, OPT_MAX_USER_CONNECTIONS,
  OPT_MAX_LENGTH_FOR_SORT_DATA,
  OPT_MAX_WRITE_LOCK_COUNT, OPT_BULK_INSERT_BUFFER_SIZE,
  OPT_MAX_ERROR_COUNT, OPT_MYISAM_DATA_POINTER_SIZE,
  OPT_MYISAM_BLOCK_SIZE, OPT_MYISAM_MAX_EXTRA_SORT_FILE_SIZE,
  OPT_MYISAM_MAX_SORT_FILE_SIZE, OPT_MYISAM_SORT_BUFFER_SIZE,
  OPT_NET_BUFFER_LENGTH, OPT_NET_RETRY_COUNT,
  OPT_NET_READ_TIMEOUT, OPT_NET_WRITE_TIMEOUT,
  OPT_OPEN_FILES_LIMIT,
  OPT_PRELOAD_BUFFER_SIZE,
  OPT_QUERY_CACHE_LIMIT, OPT_QUERY_CACHE_MIN_RES_UNIT, OPT_QUERY_CACHE_SIZE,
  OPT_QUERY_CACHE_TYPE, OPT_QUERY_CACHE_WLOCK_INVALIDATE, OPT_RECORD_BUFFER,
  OPT_RECORD_RND_BUFFER, OPT_RELAY_LOG_SPACE_LIMIT, OPT_RELAY_LOG_PURGE,
  OPT_SLAVE_NET_TIMEOUT, OPT_SLAVE_COMPRESSED_PROTOCOL, OPT_SLOW_LAUNCH_TIME,
  OPT_READONLY, OPT_DEBUGGING,
  OPT_SORT_BUFFER, OPT_TABLE_CACHE,
  OPT_THREAD_CONCURRENCY, OPT_THREAD_CACHE_SIZE,
  OPT_TMP_TABLE_SIZE, OPT_THREAD_STACK,
  OPT_WAIT_TIMEOUT, OPT_MYISAM_REPAIR_THREADS,
  OPT_INNODB_MIRRORED_LOG_GROUPS,
  OPT_INNODB_LOG_FILES_IN_GROUP,
  OPT_INNODB_LOG_FILE_SIZE,
  OPT_INNODB_LOG_BUFFER_SIZE,
  OPT_INNODB_BUFFER_POOL_SIZE,
  OPT_INNODB_BUFFER_POOL_AWE_MEM_MB,
  OPT_INNODB_ADDITIONAL_MEM_POOL_SIZE,
  OPT_INNODB_MAX_PURGE_LAG,
  OPT_INNODB_FILE_IO_THREADS,
  OPT_INNODB_LOCK_WAIT_TIMEOUT,
  OPT_INNODB_THREAD_CONCURRENCY,
  OPT_INNODB_FORCE_RECOVERY,
  OPT_INNODB_STATUS_FILE,
  OPT_INNODB_MAX_DIRTY_PAGES_PCT,
  OPT_INNODB_TABLE_LOCKS,
  OPT_INNODB_OPEN_FILES,
  OPT_INNODB_AUTOEXTEND_INCREMENT,
  OPT_BDB_CACHE_SIZE,
  OPT_BDB_LOG_BUFFER_SIZE,
  OPT_BDB_MAX_LOCK,
  OPT_ERROR_LOG_FILE,
  OPT_DEFAULT_WEEK_FORMAT,
  OPT_RANGE_ALLOC_BLOCK_SIZE,
  OPT_QUERY_ALLOC_BLOCK_SIZE, OPT_QUERY_PREALLOC_SIZE,
  OPT_TRANS_ALLOC_BLOCK_SIZE, OPT_TRANS_PREALLOC_SIZE,
  OPT_SYNC_FRM, OPT_SYNC_BINLOG, OPT_BDB_NOSYNC,
  OPT_ENABLE_SHARED_MEMORY,
  OPT_SHARED_MEMORY_BASE_NAME,
  OPT_OLD_PASSWORDS,
  OPT_EXPIRE_LOGS_DAYS,
  OPT_GROUP_CONCAT_MAX_LEN,
  OPT_DEFAULT_COLLATION,
  OPT_INIT_CONNECT,
  OPT_INIT_SLAVE,
  OPT_SECURE_AUTH,
  OPT_DATE_FORMAT,
  OPT_TIME_FORMAT,
  OPT_DATETIME_FORMAT,
  OPT_LOG_QUERIES_NOT_USING_INDEXES,
  OPT_DEFAULT_TIME_ZONE
};


#define LONG_TIMEOUT ((ulong) 3600L*24L*365L)

struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit.", 
   (gptr*) &opt_help, (gptr*) &opt_help, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
#ifdef HAVE_REPLICATION
  {"abort-slave-event-count", OPT_ABORT_SLAVE_EVENT_COUNT,
   "Option used by mysql-test for debugging and testing of replication.",
   (gptr*) &abort_slave_event_count,  (gptr*) &abort_slave_event_count,
   0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif /* HAVE_REPLICATION */
  {"ansi", 'a', "Use ANSI SQL syntax instead of MySQL syntax.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"basedir", 'b',
   "Path to installation directory. All paths are usually resolved relative to this.",
   (gptr*) &mysql_home_ptr, (gptr*) &mysql_home_ptr, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"bdb", OPT_BDB, "Enable Berkeley DB (if this version of MySQL supports it). \
Disable with --skip-bdb (will save memory).",
   (gptr*) &opt_bdb, (gptr*) &opt_bdb, 0, GET_BOOL, NO_ARG, 1, 0, 0,
   0, 0, 0},
#ifdef HAVE_BERKELEY_DB
  {"bdb-home", OPT_BDB_HOME, "Berkeley home directory.", (gptr*) &berkeley_home,
   (gptr*) &berkeley_home, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"bdb-lock-detect", OPT_BDB_LOCK,
   "Berkeley lock detect (DEFAULT, OLDEST, RANDOM or YOUNGEST, # sec).",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"bdb-logdir", OPT_BDB_LOG, "Berkeley DB log file directory.",
   (gptr*) &berkeley_logdir, (gptr*) &berkeley_logdir, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"bdb-no-recover", OPT_BDB_NO_RECOVER,
   "Don't try to recover Berkeley DB tables on start.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"bdb-no-sync", OPT_BDB_NOSYNC,
   "Disable synchronously flushing logs. This option is deprecated, use --skip-sync-bdb-logs or sync-bdb-logs=0 instead",
   //   (gptr*) &opt_sync_bdb_logs, (gptr*) &opt_sync_bdb_logs, 0, GET_BOOL,
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"bdb-shared-data", OPT_BDB_SHARED,
   "Start Berkeley DB in multi-process mode.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"bdb-tmpdir", OPT_BDB_TMP, "Berkeley DB tempfile name.",
   (gptr*) &berkeley_tmpdir, (gptr*) &berkeley_tmpdir, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif /* HAVE_BERKELEY_DB */
  {"big-tables", OPT_BIG_TABLES,
   "Allow big result sets by saving all temporary sets on file (Solves most 'table full' errors).",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"bind-address", OPT_BIND_ADDRESS, "IP address to bind to.",
   (gptr*) &my_bind_addr_str, (gptr*) &my_bind_addr_str, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog-do-db", OPT_BINLOG_DO_DB,
   "Tells the master it should log updates for the specified database, and exclude all others not explicitly mentioned.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog-ignore-db", OPT_BINLOG_IGNORE_DB,
   "Tells the master that updates to the given database should not be logged tothe binary log.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"bootstrap", OPT_BOOTSTRAP, "Used by mysql installation scripts.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"character-set-server", 'C', "Set the default character set.",
   (gptr*) &default_character_set_name, (gptr*) &default_character_set_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are.", (gptr*) &charsets_dir,
   (gptr*) &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"chroot", 'r', "Chroot mysqld daemon during startup.",
   (gptr*) &mysqld_chroot, (gptr*) &mysqld_chroot, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"collation-server", OPT_DEFAULT_COLLATION, "Set the default collation.",
   (gptr*) &default_collation_name, (gptr*) &default_collation_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"concurrent-insert", OPT_CONCURRENT_INSERT,
   "Use concurrent insert with MyISAM. Disable with --skip-concurrent-insert.",
   (gptr*) &myisam_concurrent_insert, (gptr*) &myisam_concurrent_insert,
   0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"console", OPT_CONSOLE, "Write error output on screen; Don't remove the console window on windows.",
   (gptr*) &opt_console, (gptr*) &opt_console, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"core-file", OPT_WANT_CORE, "Write core on errors.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 'h', "Path to the database root.", (gptr*) &mysql_data_home,
   (gptr*) &mysql_data_home, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
  {"debug", '#', "Debug log.", (gptr*) &default_dbug_option,
   (gptr*) &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"default-character-set", 'C', "Set the default character set (deprecated option, use --character-set-server instead).",
   (gptr*) &default_character_set_name, (gptr*) &default_character_set_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"default-collation", OPT_DEFAULT_COLLATION, "Set the default collation (deprecated option, use --collation-server instead).",
   (gptr*) &default_collation_name, (gptr*) &default_collation_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"default-storage-engine", OPT_STORAGE_ENGINE,
   "Set the default storage engine (table tyoe) for tables.", 0, 0,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-table-type", OPT_STORAGE_ENGINE,
   "(deprecated) Use --default-storage-engine.", 0, 0,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-time-zone", OPT_DEFAULT_TIME_ZONE, "Set the default time zone.",
   (gptr*) &default_tz_name, (gptr*) &default_tz_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"delay-key-write", OPT_DELAY_KEY_WRITE, "Type of DELAY_KEY_WRITE.",
   0,0,0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"delay-key-write-for-all-tables", OPT_DELAY_KEY_WRITE_ALL,
   "Don't flush key buffers between writes for any MyISAM table (Deprecated option, use --delay-key-write=all instead).",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_OPENSSL
  {"des-key-file", OPT_DES_KEY_FILE,
   "Load keys for des_encrypt() and des_encrypt from given file.",
   (gptr*) &des_key_file, (gptr*) &des_key_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
#endif /* HAVE_OPENSSL */
#ifdef HAVE_REPLICATION
  {"disconnect-slave-event-count", OPT_DISCONNECT_SLAVE_EVENT_COUNT,
   "Option used by mysql-test for debugging and testing of replication.",
   (gptr*) &disconnect_slave_event_count,
   (gptr*) &disconnect_slave_event_count, 0, GET_INT, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
#endif /* HAVE_REPLICATION */
  {"enable-locking", OPT_ENABLE_LOCK,
   "Deprecated option, use --external-locking instead.",
   (gptr*) &opt_external_locking, (gptr*) &opt_external_locking,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __NT__
  {"enable-named-pipe", OPT_HAVE_NAMED_PIPE, "Enable the named pipe (NT).",
   (gptr*) &opt_enable_named_pipe, (gptr*) &opt_enable_named_pipe, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"enable-pstack", OPT_DO_PSTACK, "Print a symbolic stack trace on failure.",
   (gptr*) &opt_do_pstack, (gptr*) &opt_do_pstack, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"exit-info", 'T', "Used for debugging;  Use at your own risk!", 0, 0, 0,
   GET_LONG, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"external-locking", OPT_USE_LOCKING, "Use system (external) locking.  With this option enabled you can run myisamchk to test (not repair) tables while the MySQL server is running.",
   (gptr*) &opt_external_locking, (gptr*) &opt_external_locking,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"flush", OPT_FLUSH, "Flush tables to disk between SQL commands.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  /* We must always support the next option to make scripts like mysqltest
     easier to do */
  {"gdb", OPT_DEBUGGING,
   "Set up signals usable for debugging",
   (gptr*) &opt_debugging, (gptr*) &opt_debugging,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"init-connect", OPT_INIT_CONNECT, "Command(s) that are executed for each new connection",
   (gptr*) &opt_init_connect, (gptr*) &opt_init_connect, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"init-file", OPT_INIT_FILE, "Read SQL commands from this file at startup.",
   (gptr*) &opt_init_file, (gptr*) &opt_init_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"init-rpl-role", OPT_INIT_RPL_ROLE, "Set the replication role.", 0, 0, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"init-slave", OPT_INIT_SLAVE, "Command(s) that are executed when a slave connects to this master",
   (gptr*) &opt_init_slave, (gptr*) &opt_init_slave, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb", OPT_INNODB, "Enable InnoDB (if this version of MySQL supports it). \
Disable with --skip-innodb (will save memory).",
   (gptr*) &opt_innodb, (gptr*) &opt_innodb, 0, GET_BOOL, NO_ARG, 1, 0, 0,
   0, 0, 0},
  {"innodb_data_file_path", OPT_INNODB_DATA_FILE_PATH,
   "Path to individual files and their sizes.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_INNOBASE_DB
  {"innodb_data_home_dir", OPT_INNODB_DATA_HOME_DIR,
   "The common part for InnoDB table spaces.", (gptr*) &innobase_data_home_dir,
   (gptr*) &innobase_data_home_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0,
   0},
  {"innodb_fast_shutdown", OPT_INNODB_FAST_SHUTDOWN,
   "Speeds up server shutdown process.", (gptr*) &innobase_fast_shutdown,
   (gptr*) &innobase_fast_shutdown, 0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"innodb_file_per_table", OPT_INNODB_FILE_PER_TABLE,
   "Stores each InnoDB table to an .ibd file in the database dir.",
   (gptr*) &innobase_file_per_table,
   (gptr*) &innobase_file_per_table, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_flush_log_at_trx_commit", OPT_INNODB_FLUSH_LOG_AT_TRX_COMMIT,
   "Set to 0 (write and flush once per second), 1 (write and flush at each commit) or 2 (write at commit, flush once per second).",
   (gptr*) &innobase_flush_log_at_trx_commit,
   (gptr*) &innobase_flush_log_at_trx_commit,
   0, GET_UINT, OPT_ARG,  1, 0, 2, 0, 0, 0},
  {"innodb_flush_method", OPT_INNODB_FLUSH_METHOD,
   "With which method to flush data.", (gptr*) &innobase_unix_file_flush_method,
   (gptr*) &innobase_unix_file_flush_method, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
  {"innodb_locks_unsafe_for_binlog", OPT_INNODB_LOCKS_UNSAFE_FOR_BINLOG,
   "Force InnoDB not to use next-key locking. Instead use only row-level locking",
   (gptr*) &innobase_locks_unsafe_for_binlog,
   (gptr*) &innobase_locks_unsafe_for_binlog, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_log_arch_dir", OPT_INNODB_LOG_ARCH_DIR,
   "Where full logs should be archived.", (gptr*) &innobase_log_arch_dir,
   (gptr*) &innobase_log_arch_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_log_archive", OPT_INNODB_LOG_ARCHIVE,
   "Set to 1 if you want to have logs archived.", 0, 0, 0, GET_LONG, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"innodb_log_group_home_dir", OPT_INNODB_LOG_GROUP_HOME_DIR,
   "Path to InnoDB log files.", (gptr*) &innobase_log_group_home_dir,
   (gptr*) &innobase_log_group_home_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
   0, 0},
  {"innodb_max_dirty_pages_pct", OPT_INNODB_MAX_DIRTY_PAGES_PCT,
   "Percentage of dirty pages allowed in bufferpool.", (gptr*) &srv_max_buf_pool_modified_pct,
   (gptr*) &srv_max_buf_pool_modified_pct, 0, GET_ULONG, REQUIRED_ARG, 90, 0, 100, 0, 0, 0},
  {"innodb_max_purge_lag", OPT_INNODB_MAX_PURGE_LAG,
   "Desired maximum length of the purge queue (0 = no limit)",
   (gptr*) &srv_max_purge_lag,
   (gptr*) &srv_max_purge_lag, 0, GET_LONG, REQUIRED_ARG, 0, 0, ~0L,
   0, 1L, 0},
  {"innodb_status_file", OPT_INNODB_STATUS_FILE,
   "Enable SHOW INNODB STATUS output in the innodb_status.<pid> file",
   (gptr*) &innobase_create_status_file, (gptr*) &innobase_create_status_file,
   0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"innodb_table_locks", OPT_INNODB_TABLE_LOCKS,
   "Enable InnoDB locking in LOCK TABLES",
   (gptr*) &global_system_variables.innodb_table_locks,
   (gptr*) &global_system_variables.innodb_table_locks,
   0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
#endif /* End HAVE_INNOBASE_DB */
  {"isam", OPT_ISAM, "Enable ISAM (if this version of MySQL supports it). \
Disable with --skip-isam.",
   (gptr*) &opt_isam, (gptr*) &opt_isam, 0, GET_BOOL, NO_ARG, 1, 0, 0,
   0, 0, 0},
  {"language", 'L',
   "Client error messages in given language. May be given as a full path.",
   (gptr*) &language_ptr, (gptr*) &language_ptr, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"local-infile", OPT_LOCAL_INFILE,
   "Enable/disable LOAD DATA LOCAL INFILE (takes values 1|0).",
   (gptr*) &opt_local_infile,
   (gptr*) &opt_local_infile, 0, GET_BOOL, OPT_ARG,
   1, 0, 0, 0, 0, 0},
  {"log", 'l', "Log connections and queries to file.", (gptr*) &opt_logname,
   (gptr*) &opt_logname, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-bin", OPT_BIN_LOG,
   "Log update queries in binary format.",
   (gptr*) &opt_bin_logname, (gptr*) &opt_bin_logname, 0, GET_STR_ALLOC,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-bin-index", OPT_BIN_LOG_INDEX,
   "File that holds the names for last binary log files.",
   (gptr*) &opt_binlog_index_name, (gptr*) &opt_binlog_index_name, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"log-error", OPT_ERROR_LOG_FILE, "Log error file.",
   (gptr*) &log_error_file_ptr, (gptr*) &log_error_file_ptr, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-isam", OPT_ISAM_LOG, "Log all MyISAM changes to file.",
   (gptr*) &myisam_log_filename, (gptr*) &myisam_log_filename, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-long-format", '0',
   "Log some extra information to update log. Please note that this option is deprecated; see --log-short-format option.", 
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-queries-not-using-indexes", OPT_LOG_QUERIES_NOT_USING_INDEXES,
   "Log queries that are executed without benefit of any index.",
   (gptr*) &opt_log_queries_not_using_indexes, (gptr*) &opt_log_queries_not_using_indexes,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-short-format", OPT_SHORT_LOG_FORMAT,
   "Don't log extra information to update and slow-query logs.",
   (gptr*) &opt_short_log_format, (gptr*) &opt_short_log_format,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-slave-updates", OPT_LOG_SLAVE_UPDATES,
   "Tells the slave to log the updates from the slave thread to the binary log. You will need to turn it on if you plan to daisy-chain the slaves.",
   (gptr*) &opt_log_slave_updates, (gptr*) &opt_log_slave_updates, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-slow-queries", OPT_SLOW_QUERY_LOG,
   "Log slow queries to this log file. Defaults logging to hostname-slow.log file.",
   (gptr*) &opt_slow_logname, (gptr*) &opt_slow_logname, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"log-update", OPT_UPDATE_LOG,
   "Log updates to file.# where # is a unique number if not given.",
   (gptr*) &opt_update_logname, (gptr*) &opt_update_logname, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-warnings", 'W', "Log some non-critical warnings to the error log file. Use this option twice or --log-warnings=2 if you also want 'Aborted connections' warnings.",
   (gptr*) &global_system_variables.log_warnings,
   (gptr*) &max_system_variables.log_warnings, 0, GET_ULONG, OPT_ARG, 1, 0, ~0L,
   0, 0, 0},
  {"low-priority-updates", OPT_LOW_PRIORITY_UPDATES,
   "INSERT/DELETE/UPDATE has lower priority than selects.",
   (gptr*) &global_system_variables.low_priority_updates,
   (gptr*) &max_system_variables.low_priority_updates,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"master-connect-retry", OPT_MASTER_CONNECT_RETRY,
   "The number of seconds the slave thread will sleep before retrying to connect to the master in case the master goes down or the connection is lost.",
   (gptr*) &master_connect_retry, (gptr*) &master_connect_retry, 0, GET_UINT,
   REQUIRED_ARG, 60, 0, 0, 0, 0, 0},
  {"master-host", OPT_MASTER_HOST,
   "Master hostname or IP address for replication. If not set, the slave thread will not be started. Note that the setting of master-host will be ignored if there exists a valid master.info file.",
   (gptr*) &master_host, (gptr*) &master_host, 0, GET_STR, REQUIRED_ARG, 0, 0,
   0, 0, 0, 0},
  {"master-info-file", OPT_MASTER_INFO_FILE,
   "The location and name of the file that remembers the master and where the I/O replication \
thread is in the master's binlogs.",
   (gptr*) &master_info_file, (gptr*) &master_info_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"master-password", OPT_MASTER_PASSWORD,
   "The password the slave thread will authenticate with when connecting to the master. If not set, an empty password is assumed.The value in master.info will take precedence if it can be read.",
   (gptr*)&master_password, (gptr*)&master_password, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"master-port", OPT_MASTER_PORT,
   "The port the master is listening on. If not set, the compiled setting of MYSQL_PORT is assumed. If you have not tinkered with configure options, this should be 3306. The value in master.info will take precedence if it can be read.",
   (gptr*) &master_port, (gptr*) &master_port, 0, GET_UINT, REQUIRED_ARG,
   MYSQL_PORT, 0, 0, 0, 0, 0},
  {"master-retry-count", OPT_MASTER_RETRY_COUNT,
   "The number of tries the slave will make to connect to the master before giving up.",
   (gptr*) &master_retry_count, (gptr*) &master_retry_count, 0, GET_ULONG,
   REQUIRED_ARG, 3600*24, 0, 0, 0, 0, 0},
  {"master-ssl", OPT_MASTER_SSL,
   "Enable the slave to connect to the master using SSL.",
   (gptr*) &master_ssl, (gptr*) &master_ssl, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"master-ssl-ca", OPT_MASTER_SSL_CA,
   "Master SSL CA file. Only applies if you have enabled master-ssl.",
   (gptr*) &master_ssl_ca, (gptr*) &master_ssl_ca, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"master-ssl-capath", OPT_MASTER_SSL_CAPATH,
   "Master SSL CA path. Only applies if you have enabled master-ssl.",
   (gptr*) &master_ssl_capath, (gptr*) &master_ssl_capath, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"master-ssl-cert", OPT_MASTER_SSL_CERT,
   "Master SSL certificate file name. Only applies if you have enabled \
master-ssl",
   (gptr*) &master_ssl_cert, (gptr*) &master_ssl_cert, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"master-ssl-cipher", OPT_MASTER_SSL_CIPHER,
   "Master SSL cipher. Only applies if you have enabled master-ssl.",
   (gptr*) &master_ssl_cipher, (gptr*) &master_ssl_capath, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"master-ssl-key", OPT_MASTER_SSL_KEY,
   "Master SSL keyfile name. Only applies if you have enabled master-ssl.",
   (gptr*) &master_ssl_key, (gptr*) &master_ssl_key, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"master-user", OPT_MASTER_USER,
   "The username the slave thread will use for authentication when connecting to the master. The user must have FILE privilege. If the master user is not set, user test is assumed. The value in master.info will take precedence if it can be read.",
   (gptr*) &master_user, (gptr*) &master_user, 0, GET_STR, REQUIRED_ARG, 0, 0,
   0, 0, 0, 0},
#ifdef HAVE_REPLICATION
  {"max-binlog-dump-events", OPT_MAX_BINLOG_DUMP_EVENTS,
   "Option used by mysql-test for debugging and testing of replication.",
   (gptr*) &max_binlog_dump_events, (gptr*) &max_binlog_dump_events, 0,
   GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif /* HAVE_REPLICATION */
  {"memlock", OPT_MEMLOCK, "Lock mysqld in memory.", (gptr*) &locked_in_memory,
   (gptr*) &locked_in_memory, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"myisam-recover", OPT_MYISAM_RECOVER,
   "Syntax: myisam-recover[=option[,option...]], where option can be DEFAULT, BACKUP, FORCE or QUICK.",
   (gptr*) &myisam_recover_options_str, (gptr*) &myisam_recover_options_str, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"ndbcluster", OPT_NDBCLUSTER, "Enable NDB Cluster (if this version of MySQL supports it). \
Disable with --skip-ndbcluster (will save memory).",
   (gptr*) &opt_ndbcluster, (gptr*) &opt_ndbcluster, 0, GET_BOOL, NO_ARG, 1, 0, 0,
   0, 0, 0},
#ifdef HAVE_NDBCLUSTER_DB
  {"ndb-connectstring", OPT_NDB_CONNECTSTRING,
   "Connect string for ndbcluster.",
   (gptr*) &ndbcluster_connectstring, (gptr*) &ndbcluster_connectstring,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"ndb_autoincrement_prefetch_sz", OPT_NDB_AUTOINCREMENT_PREFETCH_SZ,
   "Specify number of autoincrement values that are prefetched",
   (gptr*) &global_system_variables.ndb_autoincrement_prefetch_sz,
   (gptr*) &global_system_variables.ndb_autoincrement_prefetch_sz,
   0, GET_INT, REQUIRED_ARG, 32, 1, 256, 0, 0, 0},
  {"ndb_force_send", OPT_NDB_FORCE_SEND,
   "Force send of buffers to ndb immediately without waiting for other threads",
   (gptr*) &global_system_variables.ndb_force_send,
   (gptr*) &global_system_variables.ndb_force_send,
   0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"ndb_use_exact_count", OPT_NDB_USE_EXACT_COUNT,
   "Use exact records count during query planning and for "
   "fast select count(*)",
   (gptr*) &global_system_variables.ndb_use_exact_count,
   (gptr*) &global_system_variables.ndb_use_exact_count,
   0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
#endif
  {"new", 'n', "Use very new possible 'unsafe' functions.",
   (gptr*) &global_system_variables.new_mode,
   (gptr*) &max_system_variables.new_mode,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef NOT_YET
  {"no-mix-table-types", OPT_NO_MIX_TYPE, "Don't allow commands with uses two different table types.",
   (gptr*) &opt_no_mix_types, (gptr*) &opt_no_mix_types, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
#endif
  {"old-passwords", OPT_OLD_PASSWORDS, "Use old password encryption method (needed for 4.0 and older clients).",
   (gptr*) &global_system_variables.old_passwords,
   (gptr*) &max_system_variables.old_passwords, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
#ifdef ONE_THREAD
  {"one-thread", OPT_ONE_THREAD,
   "Only use one thread (for debugging under Linux).", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"pid-file", OPT_PID_FILE, "Pid file used by safe_mysqld.",
   (gptr*) &pidfile_name_ptr, (gptr*) &pidfile_name_ptr, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection.", (gptr*) &mysqld_port,
   (gptr*) &mysqld_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"relay-log", OPT_RELAY_LOG,
   "The location and name to use for relay logs.",
   (gptr*) &opt_relay_logname, (gptr*) &opt_relay_logname, 0,
   GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"relay-log-index", OPT_RELAY_LOG_INDEX,
   "The location and name to use for the file that keeps a list of the last \
relay logs.",
   (gptr*) &opt_relaylog_index_name, (gptr*) &opt_relaylog_index_name, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"relay-log-info-file", OPT_RELAY_LOG_INFO_FILE,
   "The location and name of the file that remembers where the SQL replication \
thread is in the relay logs.",
   (gptr*) &relay_log_info_file, (gptr*) &relay_log_info_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-do-db", OPT_REPLICATE_DO_DB,
   "Tells the slave thread to restrict replication to the specified database. To specify more than one database, use the directive multiple times, once for each database. Note that this will only work if you do not use cross-database queries such as UPDATE some_db.some_table SET foo='bar' while having selected a different or no database. If you need cross database updates to work, make sure you have 3.23.28 or later, and use replicate-wild-do-table=db_name.%.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-do-table", OPT_REPLICATE_DO_TABLE,
   "Tells the slave thread to restrict replication to the specified table. To specify more than one table, use the directive multiple times, once for each table. This will work for cross-database updates, in contrast to replicate-do-db.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-ignore-db", OPT_REPLICATE_IGNORE_DB,
   "Tells the slave thread to not replicate to the specified database. To specify more than one database to ignore, use the directive multiple times, once for each database. This option will not work if you use cross database updates. If you need cross database updates to work, make sure you have 3.23.28 or later, and use replicate-wild-ignore-table=db_name.%. ",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-ignore-table", OPT_REPLICATE_IGNORE_TABLE,
   "Tells the slave thread to not replicate to the specified table. To specify more than one table to ignore, use the directive multiple times, once for each table. This will work for cross-datbase updates, in contrast to replicate-ignore-db.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-rewrite-db", OPT_REPLICATE_REWRITE_DB,
   "Updates to a database with a different name than the original. Example: replicate-rewrite-db=master_db_name->slave_db_name.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-wild-do-table", OPT_REPLICATE_WILD_DO_TABLE,
   "Tells the slave thread to restrict replication to the tables that match the specified wildcard pattern. To specify more than one table, use the directive multiple times, once for each table. This will work for cross-database updates. Example: replicate-wild-do-table=foo%.bar% will replicate only updates to tables in all databases that start with foo and whose table names start with bar.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-wild-ignore-table", OPT_REPLICATE_WILD_IGNORE_TABLE,
   "Tells the slave thread to not replicate to the tables that match the given wildcard pattern. To specify more than one table to ignore, use the directive multiple times, once for each table. This will work for cross-database updates. Example: replicate-wild-ignore-table=foo%.bar% will not do updates to tables in databases that start with foo and whose table names start with bar.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_REPLICATION
  {"replicate-same-server-id", OPT_REPLICATE_SAME_SERVER_ID,
   "In replication, if set to 1, do not skip events having our server id. \
Default value is 0 (to break infinite loops in circular replication). \
Can't be set to 1 if --log-slave-updates is used.",
   (gptr*) &replicate_same_server_id,
   (gptr*) &replicate_same_server_id,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  // In replication, we may need to tell the other servers how to connect
  {"report-host", OPT_REPORT_HOST,
   "Hostname or IP of the slave to be reported to to the master during slave registration. Will appear in the output of SHOW SLAVE HOSTS. Leave unset if you do not want the slave to register itself with the master. Note that it is not sufficient for the master to simply read the IP of the slave off the socket once the slave connects. Due to NAT and other routing issues, that IP may not be valid for connecting to the slave from the master or other hosts.",
   (gptr*) &report_host, (gptr*) &report_host, 0, GET_STR, REQUIRED_ARG, 0, 0,
   0, 0, 0, 0},
  {"report-password", OPT_REPORT_PASSWORD, "Undocumented.",
   (gptr*) &report_password, (gptr*) &report_password, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"report-port", OPT_REPORT_PORT,
   "Port for connecting to slave reported to the master during slave registration. Set it only if the slave is listening on a non-default port or if you have a special tunnel from the master or other clients to the slave. If not sure, leave this option unset.",
   (gptr*) &report_port, (gptr*) &report_port, 0, GET_UINT, REQUIRED_ARG,
   MYSQL_PORT, 0, 0, 0, 0, 0},
  {"report-user", OPT_REPORT_USER, "Undocumented.", (gptr*) &report_user,
   (gptr*) &report_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"rpl-recovery-rank", OPT_RPL_RECOVERY_RANK, "Undocumented.",
   (gptr*) &rpl_recovery_rank, (gptr*) &rpl_recovery_rank, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"safe-mode", OPT_SAFE, "Skip some optimize stages (for testing).",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef TO_BE_DELETED
  {"safe-show-database", OPT_SAFE_SHOW_DB,
   "Deprecated option; use GRANT SHOW DATABASES instead...",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"safe-user-create", OPT_SAFE_USER_CREATE,
   "Don't allow new user creation by the user who has no write privileges to the mysql.user table.",
   (gptr*) &opt_safe_user_create, (gptr*) &opt_safe_user_create, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"safemalloc-mem-limit", OPT_SAFEMALLOC_MEM_LIMIT,
   "Simulate memory shortage when compiled with the --with-debug=full option.",
   0, 0, 0, GET_ULL, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"secure-auth", OPT_SECURE_AUTH, "Disallow authentication for accounts that have old (pre-4.1) passwords.",
   (gptr*) &opt_secure_auth, (gptr*) &opt_secure_auth, 0, GET_BOOL, NO_ARG,
   my_bool(0), 0, 0, 0, 0, 0},
  {"server-id",	OPT_SERVER_ID,
   "Uniquely identifies the server instance in the community of replication partners.",
   (gptr*) &server_id, (gptr*) &server_id, 0, GET_ULONG, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
  {"set-variable", 'O',
   "Change the value of a variable. Please note that this option is deprecated;you can set variables directly with --variable-name=value.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_SMEM
  {"shared-memory", OPT_ENABLE_SHARED_MEMORY,
   "Enable the shared memory.",(gptr*) &opt_enable_shared_memory, (gptr*) &opt_enable_shared_memory,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
#ifdef HAVE_SMEM
  {"shared-memory-base-name",OPT_SHARED_MEMORY_BASE_NAME,
   "Base name of shared memory.", (gptr*) &shared_memory_base_name, (gptr*) &shared_memory_base_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"show-slave-auth-info", OPT_SHOW_SLAVE_AUTH_INFO,
   "Show user and password in SHOW SLAVE HOSTS on this master",
   (gptr*) &opt_show_slave_auth_info, (gptr*) &opt_show_slave_auth_info, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-grant-tables", OPT_SKIP_GRANT,
   "Start without grant tables. This gives all users FULL ACCESS to all tables!",
   (gptr*) &opt_noacl, (gptr*) &opt_noacl, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"skip-host-cache", OPT_SKIP_HOST_CACHE, "Don't cache host names.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-locking", OPT_SKIP_LOCK,
   "Deprecated option, use --skip-external-locking instead.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-name-resolve", OPT_SKIP_RESOLVE,
   "Don't resolve hostnames. All hostnames are IP's or 'localhost'.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-networking", OPT_SKIP_NETWORKING,
   "Don't allow connection with TCP/IP.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"skip-new", OPT_SKIP_NEW, "Don't use new, possible wrong routines.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
#ifdef SAFEMALLOC
  {"skip-safemalloc", OPT_SKIP_SAFEMALLOC,
   "Don't use the memory allocation checking.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
#endif
#endif
  {"skip-show-database", OPT_SKIP_SHOW_DB,
   "Don't allow 'SHOW DATABASE' commands.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"skip-slave-start", OPT_SKIP_SLAVE_START,
   "If set, slave is not autostarted.", (gptr*) &opt_skip_slave_start,
   (gptr*) &opt_skip_slave_start, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-stack-trace", OPT_SKIP_STACK_TRACE,
   "Don't print a stack trace on failure.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"skip-symlink", OPT_SKIP_SYMLINKS, "Don't allow symlinking of tables. Deprecated option.  Use --skip-symbolic-links instead.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-thread-priority", OPT_SKIP_PRIOR,
   "Don't give threads different priorities.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
#ifdef HAVE_REPLICATION
  {"slave-load-tmpdir", OPT_SLAVE_LOAD_TMPDIR,
   "The location where the slave should put its temporary files when \
replicating a LOAD DATA INFILE command.",
   (gptr*) &slave_load_tmpdir, (gptr*) &slave_load_tmpdir, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"slave-skip-errors", OPT_SLAVE_SKIP_ERRORS,
   "Tells the slave thread to continue replication when a query returns an error from the provided list.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"socket", OPT_SOCKET, "Socket file to use for connection.",
   (gptr*) &mysqld_unix_port, (gptr*) &mysqld_unix_port, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_REPLICATION
  {"sporadic-binlog-dump-fail", OPT_SPORADIC_BINLOG_DUMP_FAIL,
   "Option used by mysql-test for debugging and testing of replication.",
   (gptr*) &opt_sporadic_binlog_dump_fail,
   (gptr*) &opt_sporadic_binlog_dump_fail, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
#endif /* HAVE_REPLICATION */
  {"sql-bin-update-same", OPT_SQL_BIN_UPDATE_SAME,
   "If set, setting SQL_LOG_BIN to a value will automatically set SQL_LOG_UPDATE to the same value and vice versa.",
   (gptr*) &opt_sql_bin_update, (gptr*) &opt_sql_bin_update, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"sql-mode", OPT_SQL_MODE,
   "Syntax: sql-mode=option[,option[,option...]] where option can be one of: REAL_AS_FLOAT, PIPES_AS_CONCAT, ANSI_QUOTES, IGNORE_SPACE, ONLY_FULL_GROUP_BY, NO_UNSIGNED_SUBTRACTION.",
   (gptr*) &sql_mode_str, (gptr*) &sql_mode_str, 0, GET_STR, REQUIRED_ARG, 0,
   0, 0, 0, 0, 0},
#ifdef HAVE_OPENSSL
#include "sslopt-longopts.h"
#endif
#ifdef __WIN__
  {"standalone", OPT_STANDALONE,
  "Dummy option to start as a standalone program (NT).", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"symbolic-links", 's', "Enable symbolic link support.",
   (gptr*) &my_use_symdir, (gptr*) &my_use_symdir, 0, GET_BOOL, NO_ARG,
   IF_PURIFY(0,1), 0, 0, 0, 0, 0},
  {"temp-pool", OPT_TEMP_POOL,
   "Using this option will cause most temporary files created to use a small set of names, rather than a unique name for each new file.",
   (gptr*) &use_temp_pool, (gptr*) &use_temp_pool, 0, GET_BOOL, NO_ARG, 1,
   0, 0, 0, 0, 0},
  {"tmpdir", 't',
   "Path for temporary files. Several paths may be specified, separated by a "
#if defined( __WIN__) || defined(OS2) || defined(__NETWARE__)
   "semicolon (;)"
#else
   "colon (:)"
#endif
   ", in this case they are used in a round-robin fashion.",
   (gptr*) &opt_mysql_tmpdir,
   (gptr*) &opt_mysql_tmpdir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"transaction-isolation", OPT_TX_ISOLATION,
   "Default transaction isolation level.", 0, 0, 0, GET_STR, REQUIRED_ARG, 0,
   0, 0, 0, 0, 0},
  {"use-symbolic-links", 's', "Enable symbolic link support. Deprecated option; use --symbolic-links instead.",
   (gptr*) &my_use_symdir, (gptr*) &my_use_symdir, 0, GET_BOOL, NO_ARG,
   IF_PURIFY(0,1), 0, 0, 0, 0, 0},
  {"user", 'u', "Run mysqld daemon as user.", 0, 0, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Used with --help option for detailed help",
   (gptr*) &opt_verbose, (gptr*) &opt_verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"warnings", 'W', "Deprecated; use --log-warnings instead.",
   (gptr*) &global_system_variables.log_warnings,
   (gptr*) &max_system_variables.log_warnings, 0, GET_ULONG, OPT_ARG, 1, 0, ~0L,
   0, 0, 0},
  { "back_log", OPT_BACK_LOG,
    "The number of outstanding connection requests MySQL can have. This comes into play when the main MySQL thread gets very many connection requests in a very short time.",
    (gptr*) &back_log, (gptr*) &back_log, 0, GET_ULONG,
    REQUIRED_ARG, 50, 1, 65535, 0, 1, 0 },
#ifdef HAVE_BERKELEY_DB
  { "bdb_cache_size", OPT_BDB_CACHE_SIZE,
    "The buffer that is allocated to cache index and rows for BDB tables.",
    (gptr*) &berkeley_cache_size, (gptr*) &berkeley_cache_size, 0, GET_ULONG,
    REQUIRED_ARG, KEY_CACHE_SIZE, 20*1024, (long) ~0, 0, IO_SIZE, 0},
  /* QQ: The following should be removed soon! (bdb_max_lock preferred) */
  {"bdb_lock_max", OPT_BDB_MAX_LOCK, "Synonym for bdb_max_lock.",
   (gptr*) &berkeley_max_lock, (gptr*) &berkeley_max_lock, 0, GET_ULONG,
   REQUIRED_ARG, 10000, 0, (long) ~0, 0, 1, 0},
  {"bdb_log_buffer_size", OPT_BDB_LOG_BUFFER_SIZE,
   "The buffer that is allocated to cache index and rows for BDB tables.",
   (gptr*) &berkeley_log_buffer_size, (gptr*) &berkeley_log_buffer_size, 0,
   GET_ULONG, REQUIRED_ARG, 0, 256*1024L, ~0L, 0, 1024, 0},
  {"bdb_max_lock", OPT_BDB_MAX_LOCK,
   "The maximum number of locks you can have active on a BDB table.",
   (gptr*) &berkeley_max_lock, (gptr*) &berkeley_max_lock, 0, GET_ULONG,
   REQUIRED_ARG, 10000, 0, (long) ~0, 0, 1, 0},
#endif /* HAVE_BERKELEY_DB */
  {"binlog_cache_size", OPT_BINLOG_CACHE_SIZE,
   "The size of the cache to hold the SQL statements for the binary log during a transaction. If you often use big, multi-statement transactions you can increase this to get more performance.",
   (gptr*) &binlog_cache_size, (gptr*) &binlog_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, 32*1024L, IO_SIZE, ~0L, 0, IO_SIZE, 0},
  {"bulk_insert_buffer_size", OPT_BULK_INSERT_BUFFER_SIZE,
   "Size of tree cache used in bulk insert optimisation. Note that this is a limit per thread!",
   (gptr*) &global_system_variables.bulk_insert_buff_size,
   (gptr*) &max_system_variables.bulk_insert_buff_size,
   0, GET_ULONG, REQUIRED_ARG, 8192*1024, 0, ~0L, 0, 1, 0},
  {"connect_timeout", OPT_CONNECT_TIMEOUT,
   "The number of seconds the mysqld server is waiting for a connect packet before responding with 'Bad handshake'.",
    (gptr*) &connect_timeout, (gptr*) &connect_timeout,
   0, GET_ULONG, REQUIRED_ARG, CONNECT_TIMEOUT, 2, LONG_TIMEOUT, 0, 1, 0 },
#ifdef HAVE_REPLICATION
  {"crash_binlog_innodb", OPT_CRASH_BINLOG_INNODB,
   "Used only for testing, to crash when writing Nth event to binlog.",
   (gptr*) &opt_crash_binlog_innodb, (gptr*) &opt_crash_binlog_innodb,
   0, GET_UINT, REQUIRED_ARG, 0, 0, ~(uint)0, 0, 1, 0},
#endif
  { "date_format", OPT_DATE_FORMAT,
    "The DATE format (For future).",
    (gptr*) &opt_date_time_formats[MYSQL_TIMESTAMP_DATE],
    (gptr*) &opt_date_time_formats[MYSQL_TIMESTAMP_DATE],
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "datetime_format", OPT_DATETIME_FORMAT,
    "The DATETIME/TIMESTAMP format (for future).",
    (gptr*) &opt_date_time_formats[MYSQL_TIMESTAMP_DATETIME],
    (gptr*) &opt_date_time_formats[MYSQL_TIMESTAMP_DATETIME],
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "default_week_format", OPT_DEFAULT_WEEK_FORMAT,
    "The default week format used by WEEK() functions.",
    (gptr*) &global_system_variables.default_week_format,
    (gptr*) &max_system_variables.default_week_format,
    0, GET_ULONG, REQUIRED_ARG, 0, 0, 7L, 0, 1, 0},
  {"delayed_insert_limit", OPT_DELAYED_INSERT_LIMIT,
   "After inserting delayed_insert_limit rows, the INSERT DELAYED handler will check if there are any SELECT statements pending. If so, it allows these to execute before continuing.",
    (gptr*) &delayed_insert_limit, (gptr*) &delayed_insert_limit, 0, GET_ULONG,
    REQUIRED_ARG, DELAYED_LIMIT, 1, ~0L, 0, 1, 0},
  {"delayed_insert_timeout", OPT_DELAYED_INSERT_TIMEOUT,
   "How long a INSERT DELAYED thread should wait for INSERT statements before terminating.",
   (gptr*) &delayed_insert_timeout, (gptr*) &delayed_insert_timeout, 0,
   GET_ULONG, REQUIRED_ARG, DELAYED_WAIT_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  { "delayed_queue_size", OPT_DELAYED_QUEUE_SIZE,
    "What size queue (in rows) should be allocated for handling INSERT DELAYED. If the queue becomes full, any client that does INSERT DELAYED will wait until there is room in the queue again.",
    (gptr*) &delayed_queue_size, (gptr*) &delayed_queue_size, 0, GET_ULONG,
    REQUIRED_ARG, DELAYED_QUEUE_SIZE, 1, ~0L, 0, 1, 0},
  {"expire_logs_days", OPT_EXPIRE_LOGS_DAYS,
   "Binary logs will be rotated after expire-log-days days ",
   (gptr*) &expire_logs_days,
   (gptr*) &expire_logs_days, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, 99, 0, 1, 0},
  { "flush_time", OPT_FLUSH_TIME,
    "A dedicated thread is created to flush all tables at the given interval.",
    (gptr*) &flush_time, (gptr*) &flush_time, 0, GET_ULONG, REQUIRED_ARG,
    FLUSH_TIME, 0, LONG_TIMEOUT, 0, 1, 0},
  { "ft_boolean_syntax", OPT_FT_BOOLEAN_SYNTAX,
    "List of operators for MATCH ... AGAINST ( ... IN BOOLEAN MODE)",
    0, 0, 0, GET_STR,
    REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "ft_max_word_len", OPT_FT_MAX_WORD_LEN,
    "The maximum length of the word to be included in a FULLTEXT index. Note: FULLTEXT indexes must be rebuilt after changing this variable.",
    (gptr*) &ft_max_word_len, (gptr*) &ft_max_word_len, 0, GET_ULONG,
    REQUIRED_ARG, HA_FT_MAXCHARLEN, 10, HA_FT_MAXCHARLEN, 0, 1, 0},
  { "ft_min_word_len", OPT_FT_MIN_WORD_LEN,
    "The minimum length of the word to be included in a FULLTEXT index. Note: FULLTEXT indexes must be rebuilt after changing this variable.",
    (gptr*) &ft_min_word_len, (gptr*) &ft_min_word_len, 0, GET_ULONG,
    REQUIRED_ARG, 4, 1, HA_FT_MAXCHARLEN, 0, 1, 0},
  { "ft_query_expansion_limit", OPT_FT_QUERY_EXPANSION_LIMIT,
    "Number of best matches to use for query expansion",
    (gptr*) &ft_query_expansion_limit, (gptr*) &ft_query_expansion_limit, 0, GET_ULONG,
    REQUIRED_ARG, 20, 0, 1000, 0, 1, 0},
  { "ft_stopword_file", OPT_FT_STOPWORD_FILE,
    "Use stopwords from this file instead of built-in list.",
    (gptr*) &ft_stopword_file, (gptr*) &ft_stopword_file, 0, GET_STR,
    REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "group_concat_max_len", OPT_GROUP_CONCAT_MAX_LEN,
    "The maximum length of the result of function  group_concat.",
    (gptr*) &global_system_variables.group_concat_max_len,
    (gptr*) &max_system_variables.group_concat_max_len, 0, GET_ULONG,
    REQUIRED_ARG, 1024, 4, (long) ~0, 0, 1, 0},
#ifdef HAVE_INNOBASE_DB
  {"innodb_additional_mem_pool_size", OPT_INNODB_ADDITIONAL_MEM_POOL_SIZE,
   "Size of a memory pool InnoDB uses to store data dictionary information and other internal data structures.",
   (gptr*) &innobase_additional_mem_pool_size,
   (gptr*) &innobase_additional_mem_pool_size, 0, GET_LONG, REQUIRED_ARG,
   1*1024*1024L, 512*1024L, ~0L, 0, 1024, 0},
  {"innodb_autoextend_increment", OPT_INNODB_AUTOEXTEND_INCREMENT,
   "Data file autoextend increment in megabytes",
   (gptr*) &srv_auto_extend_increment,
   (gptr*) &srv_auto_extend_increment,
   0, GET_LONG, REQUIRED_ARG, 8L, 1L, ~0L, 0, 1L, 0},
  {"innodb_buffer_pool_awe_mem_mb", OPT_INNODB_BUFFER_POOL_AWE_MEM_MB,
   "If Windows AWE is used, the size of InnoDB buffer pool allocated from the AWE memory.",
   (gptr*) &innobase_buffer_pool_awe_mem_mb, (gptr*) &innobase_buffer_pool_awe_mem_mb, 0,
   GET_LONG, REQUIRED_ARG, 0, 0, 63000, 0, 1, 0},
  {"innodb_buffer_pool_size", OPT_INNODB_BUFFER_POOL_SIZE,
   "The size of the memory buffer InnoDB uses to cache data and indexes of its tables.",
   (gptr*) &innobase_buffer_pool_size, (gptr*) &innobase_buffer_pool_size, 0,
   GET_LONG, REQUIRED_ARG, 8*1024*1024L, 1024*1024L, ~0L, 0, 1024*1024L, 0},
  {"innodb_file_io_threads", OPT_INNODB_FILE_IO_THREADS,
   "Number of file I/O threads in InnoDB.", (gptr*) &innobase_file_io_threads,
   (gptr*) &innobase_file_io_threads, 0, GET_LONG, REQUIRED_ARG, 4, 4, 64, 0,
   1, 0},
  {"innodb_force_recovery", OPT_INNODB_FORCE_RECOVERY,
   "Helps to save your data in case the disk image of the database becomes corrupt.",
   (gptr*) &innobase_force_recovery, (gptr*) &innobase_force_recovery, 0,
   GET_LONG, REQUIRED_ARG, 0, 0, 6, 0, 1, 0},
  {"innodb_lock_wait_timeout", OPT_INNODB_LOCK_WAIT_TIMEOUT,
   "Timeout in seconds an InnoDB transaction may wait for a lock before being rolled back.",
   (gptr*) &innobase_lock_wait_timeout, (gptr*) &innobase_lock_wait_timeout,
   0, GET_LONG, REQUIRED_ARG, 50, 1, 1024 * 1024 * 1024, 0, 1, 0},
  {"innodb_log_buffer_size", OPT_INNODB_LOG_BUFFER_SIZE,
   "The size of the buffer which InnoDB uses to write log to the log files on disk.",
   (gptr*) &innobase_log_buffer_size, (gptr*) &innobase_log_buffer_size, 0,
   GET_LONG, REQUIRED_ARG, 1024*1024L, 256*1024L, ~0L, 0, 1024, 0},
  {"innodb_log_file_size", OPT_INNODB_LOG_FILE_SIZE,
   "Size of each log file in a log group in megabytes.",
   (gptr*) &innobase_log_file_size, (gptr*) &innobase_log_file_size, 0,
   GET_LONG, REQUIRED_ARG, 5*1024*1024L, 1*1024*1024L, ~0L, 0, 1024*1024L, 0},
  {"innodb_log_files_in_group", OPT_INNODB_LOG_FILES_IN_GROUP,
   "Number of log files in the log group. InnoDB writes to the files in a circular fashion. Value 3 is recommended here.",
   (gptr*) &innobase_log_files_in_group, (gptr*) &innobase_log_files_in_group,
   0, GET_LONG, REQUIRED_ARG, 2, 2, 100, 0, 1, 0},
  {"innodb_mirrored_log_groups", OPT_INNODB_MIRRORED_LOG_GROUPS,
   "Number of identical copies of log groups we keep for the database. Currently this should be set to 1.",
   (gptr*) &innobase_mirrored_log_groups,
   (gptr*) &innobase_mirrored_log_groups, 0, GET_LONG, REQUIRED_ARG, 1, 1, 10,
   0, 1, 0},
  {"innodb_open_files", OPT_INNODB_OPEN_FILES,
   "How many files at the maximum InnoDB keeps open at the same time.",
   (gptr*) &innobase_open_files, (gptr*) &innobase_open_files, 0,
   GET_LONG, REQUIRED_ARG, 300L, 10L, ~0L, 0, 1L, 0},
#ifdef HAVE_REPLICATION
  /*
    Disabled for the 4.1.3 release. Disabling just this paragraph of code is
    enough, as then user can't set it to 1 so it will always be ignored in the
    rest of code.
  */
#if MYSQL_VERSION_ID >= 40103
  /*
    innodb_safe_binlog is not a variable, just an option. Does not make
    sense to make it a variable, as it is only used at startup (and so the
    value would be lost at next startup, so setting it on the fly would have no
    effect).
  */
  {"innodb_safe_binlog", OPT_INNODB_SAFE_BINLOG,
   "After a crash recovery by InnoDB, truncate the binary log after the last "
   "not-rolled-back statement/transaction.",
   (gptr*) &opt_innodb_safe_binlog, (gptr*) &opt_innodb_safe_binlog,
   0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 1, 0},
#endif
#endif
  {"innodb_thread_concurrency", OPT_INNODB_THREAD_CONCURRENCY,
   "Helps in performance tuning in heavily concurrent environments.",
   (gptr*) &innobase_thread_concurrency, (gptr*) &innobase_thread_concurrency,
   0, GET_LONG, REQUIRED_ARG, 8, 1, 1000, 0, 1, 0},
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
   "The size of the buffer used for index blocks for MyISAM tables. Increase this to get better index handling (for all reads and multiple writes) to as much as you can afford; 64M on a 256M machine that mainly runs MySQL is quite common.",
   (gptr*) &dflt_key_cache_var.param_buff_size,
   (gptr*) 0,
   0, (GET_ULL | GET_ASK_ADDR),
   REQUIRED_ARG, KEY_CACHE_SIZE, MALLOC_OVERHEAD, UINT_MAX32, MALLOC_OVERHEAD,
   IO_SIZE, 0},
  {"key_cache_age_threshold", OPT_KEY_CACHE_AGE_THRESHOLD,
   "This characterizes the number of hits a hot block has to be untouched until it is considered aged enough to be downgraded to a warm block. This specifies the percentage ratio of that number of hits to the total number of blocks in key cache",
   (gptr*) &dflt_key_cache_var.param_age_threshold,
   (gptr*) 0,
   0, (GET_ULONG | GET_ASK_ADDR), REQUIRED_ARG, 
   300, 100, ~0L, 0, 100, 0},
  {"key_cache_block_size", OPT_KEY_CACHE_BLOCK_SIZE,
   "The default size of key cache blocks",
   (gptr*) &dflt_key_cache_var.param_block_size,
   (gptr*) 0,
   0, (GET_ULONG | GET_ASK_ADDR), REQUIRED_ARG,
   KEY_CACHE_BLOCK_SIZE , 512, 1024*16, MALLOC_OVERHEAD, 512, 0},
  {"key_cache_division_limit", OPT_KEY_CACHE_DIVISION_LIMIT,
   "The minimum percentage of warm blocks in key cache",
   (gptr*) &dflt_key_cache_var.param_division_limit,
   (gptr*) 0,
   0, (GET_ULONG | GET_ASK_ADDR) , REQUIRED_ARG, 100,
   1, 100, 0, 1, 0},
  {"long_query_time", OPT_LONG_QUERY_TIME,
   "Log all queries that have taken more than long_query_time seconds to execute to file.",
   (gptr*) &global_system_variables.long_query_time,
   (gptr*) &max_system_variables.long_query_time, 0, GET_ULONG,
   REQUIRED_ARG, 10, 1, LONG_TIMEOUT, 0, 1, 0},
  {"lower_case_table_names", OPT_LOWER_CASE_TABLE_NAMES,
   "If set to 1 table names are stored in lowercase on disk and table names will be case-insensitive.  Should be set to 2 if you are using a case insensitive file system",
   (gptr*) &lower_case_table_names,
   (gptr*) &lower_case_table_names, 0, GET_UINT, OPT_ARG,
#ifdef FN_NO_CASE_SENCE
    1
#else
    0
#endif
   , 0, 2, 0, 1, 0},
  {"max_allowed_packet", OPT_MAX_ALLOWED_PACKET,
   "Max packetlength to send/receive from to server.",
   (gptr*) &global_system_variables.max_allowed_packet,
   (gptr*) &max_system_variables.max_allowed_packet, 0, GET_ULONG,
   REQUIRED_ARG, 1024*1024L, 1024, 1024L*1024L*1024L, MALLOC_OVERHEAD, 1024, 0},
  {"max_binlog_cache_size", OPT_MAX_BINLOG_CACHE_SIZE,
   "Can be used to restrict the total size used to cache a multi-transaction query.",
   (gptr*) &max_binlog_cache_size, (gptr*) &max_binlog_cache_size, 0,
   GET_ULONG, REQUIRED_ARG, ~0L, IO_SIZE, ~0L, 0, IO_SIZE, 0},
  {"max_binlog_size", OPT_MAX_BINLOG_SIZE,
   "Binary log will be rotated automatically when the size exceeds this \
value. Will also apply to relay logs if max_relay_log_size is 0. \
The minimum value for this variable is 4096.",
   (gptr*) &max_binlog_size, (gptr*) &max_binlog_size, 0, GET_ULONG,
   REQUIRED_ARG, 1024*1024L*1024L, IO_SIZE, 1024*1024L*1024L, 0, IO_SIZE, 0},
  {"max_connect_errors", OPT_MAX_CONNECT_ERRORS,
   "If there is more than this number of interrupted connections from a host this host will be blocked from further connections.",
   (gptr*) &max_connect_errors, (gptr*) &max_connect_errors, 0, GET_ULONG,
    REQUIRED_ARG, MAX_CONNECT_ERRORS, 1, ~0L, 0, 1, 0},
  {"max_connections", OPT_MAX_CONNECTIONS,
   "The number of simultaneous clients allowed.", (gptr*) &max_connections,
   (gptr*) &max_connections, 0, GET_ULONG, REQUIRED_ARG, 100, 1, 16384, 0, 1,
   0},
  {"max_delayed_threads", OPT_MAX_DELAYED_THREADS,
   "Don't start more than this number of threads to handle INSERT DELAYED statements. If set to zero, which means INSERT DELAYED is not used.",
   (gptr*) &global_system_variables.max_insert_delayed_threads,
   (gptr*) &max_system_variables.max_insert_delayed_threads,
   0, GET_ULONG, REQUIRED_ARG, 20, 0, 16384, 0, 1, 0},
  {"max_error_count", OPT_MAX_ERROR_COUNT,
   "Max number of errors/warnings to store for a statement.",
   (gptr*) &global_system_variables.max_error_count,
   (gptr*) &max_system_variables.max_error_count,
   0, GET_ULONG, REQUIRED_ARG, DEFAULT_ERROR_COUNT, 1, 65535, 0, 1, 0},
  {"max_heap_table_size", OPT_MAX_HEP_TABLE_SIZE,
   "Don't allow creation of heap tables bigger than this.",
   (gptr*) &global_system_variables.max_heap_table_size,
   (gptr*) &max_system_variables.max_heap_table_size, 0, GET_ULONG,
   REQUIRED_ARG, 16*1024*1024L, 16384, ~0L, MALLOC_OVERHEAD, 1024, 0},
  {"max_join_size", OPT_MAX_JOIN_SIZE,
   "Joins that are probably going to read more than max_join_size records return an error.",
   (gptr*) &global_system_variables.max_join_size,
   (gptr*) &max_system_variables.max_join_size, 0, GET_HA_ROWS, REQUIRED_ARG,
   ~0L, 1, ~0L, 0, 1, 0},
   {"max_length_for_sort_data", OPT_MAX_LENGTH_FOR_SORT_DATA,
    "Max number of bytes in sorted records.",
    (gptr*) &global_system_variables.max_length_for_sort_data,
    (gptr*) &max_system_variables.max_length_for_sort_data, 0, GET_ULONG,
    REQUIRED_ARG, 1024, 4, 8192*1024L, 0, 1, 0},
  {"max_relay_log_size", OPT_MAX_RELAY_LOG_SIZE,
   "If non-zero: relay log will be rotated automatically when the size exceeds this value; if zero (the default): when the size exceeds max_binlog_size. 0 expected, the minimum value for this variable is 4096.",
   (gptr*) &max_relay_log_size, (gptr*) &max_relay_log_size, 0, GET_ULONG,
   REQUIRED_ARG, 0L, 0L, 1024*1024L*1024L, 0, IO_SIZE, 0},
  { "max_seeks_for_key", OPT_MAX_SEEKS_FOR_KEY,
    "Limit assumed max number of seeks when looking up rows based on a key",
    (gptr*) &global_system_variables.max_seeks_for_key,
    (gptr*) &max_system_variables.max_seeks_for_key, 0, GET_ULONG,
    REQUIRED_ARG, ~0L, 1, ~0L, 0, 1, 0 },
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
  {"myisam_block_size", OPT_MYISAM_BLOCK_SIZE,
   "Block size to be used for MyISAM index pages.",
   (gptr*) &opt_myisam_block_size,
   (gptr*) &opt_myisam_block_size, 0, GET_ULONG, REQUIRED_ARG,
   MI_KEY_BLOCK_LENGTH, MI_MIN_KEY_BLOCK_LENGTH, MI_MAX_KEY_BLOCK_LENGTH,
   0, MI_MIN_KEY_BLOCK_LENGTH, 0},
  {"myisam_data_pointer_size", OPT_MYISAM_DATA_POINTER_SIZE,
   "Default pointer size to be used for MyISAM tables.",
   (gptr*) &myisam_data_pointer_size,
   (gptr*) &myisam_data_pointer_size, 0, GET_ULONG, REQUIRED_ARG,
   4, 2, 7, 0, 1, 0},
  {"myisam_max_extra_sort_file_size", OPT_MYISAM_MAX_EXTRA_SORT_FILE_SIZE,
   "Used to help MySQL to decide when to use the slow but safe key cache index create method.",
   (gptr*) &global_system_variables.myisam_max_extra_sort_file_size,
   (gptr*) &max_system_variables.myisam_max_extra_sort_file_size,
   0, GET_ULL, REQUIRED_ARG, (ulonglong) MI_MAX_TEMP_LENGTH,
   0, (ulonglong) MAX_FILE_SIZE, 0, 1, 0},
  {"myisam_max_sort_file_size", OPT_MYISAM_MAX_SORT_FILE_SIZE,
   "Don't use the fast sort index method to created index if the temporary file would get bigger than this.",
   (gptr*) &global_system_variables.myisam_max_sort_file_size,
   (gptr*) &max_system_variables.myisam_max_sort_file_size, 0,
   GET_ULL, REQUIRED_ARG, (longlong) LONG_MAX, 0, (ulonglong) MAX_FILE_SIZE,
   0, 1024*1024, 0},
  {"myisam_repair_threads", OPT_MYISAM_REPAIR_THREADS,
   "Number of threads to use when repairing MyISAM tables. The value of 1 disables parallel repair.",
   (gptr*) &global_system_variables.myisam_repair_threads,
   (gptr*) &max_system_variables.myisam_repair_threads, 0,
   GET_ULONG, REQUIRED_ARG, 1, 1, ~0L, 0, 1, 0},
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
  {"net_read_timeout", OPT_NET_READ_TIMEOUT,
   "Number of seconds to wait for more data from a connection before aborting the read.",
   (gptr*) &global_system_variables.net_read_timeout,
   (gptr*) &max_system_variables.net_read_timeout, 0, GET_ULONG,
   REQUIRED_ARG, NET_READ_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"net_retry_count", OPT_NET_RETRY_COUNT,
   "If a read on a communication port is interrupted, retry this many times before giving up.",
   (gptr*) &global_system_variables.net_retry_count,
   (gptr*) &max_system_variables.net_retry_count,0,
   GET_ULONG, REQUIRED_ARG, MYSQLD_NET_RETRY_COUNT, 1, ~0L, 0, 1, 0},
  {"net_write_timeout", OPT_NET_WRITE_TIMEOUT,
   "Number of seconds to wait for a block to be written to a connection  before aborting the write.",
   (gptr*) &global_system_variables.net_write_timeout,
   (gptr*) &max_system_variables.net_write_timeout, 0, GET_ULONG,
   REQUIRED_ARG, NET_WRITE_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"open_files_limit", OPT_OPEN_FILES_LIMIT,
   "If this is not 0, then mysqld will use this value to reserve file descriptors to use with setrlimit(). If this value is 0 then mysqld will reserve max_connections*5 or max_connections + table_cache*2 (whichever is larger) number of files.",
   (gptr*) &open_files_limit, (gptr*) &open_files_limit, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, OS_FILE_LIMIT, 0, 1, 0},
   {"preload_buffer_size", OPT_PRELOAD_BUFFER_SIZE,
    "The size of the buffer that is allocated when preloading indexes",
    (gptr*) &global_system_variables.preload_buff_size,
    (gptr*) &max_system_variables.preload_buff_size, 0, GET_ULONG,
    REQUIRED_ARG, 32*1024L, 1024, 1024*1024*1024L, 0, 1, 0},
  {"query_alloc_block_size", OPT_QUERY_ALLOC_BLOCK_SIZE,
   "Allocation block size for query parsing and execution",
   (gptr*) &global_system_variables.query_alloc_block_size,
   (gptr*) &max_system_variables.query_alloc_block_size, 0, GET_ULONG,
   REQUIRED_ARG, QUERY_ALLOC_BLOCK_SIZE, 1024, ~0L, 0, 1024, 0},
#ifdef HAVE_QUERY_CACHE
  {"query_cache_limit", OPT_QUERY_CACHE_LIMIT,
   "Don't cache results that are bigger than this.",
   (gptr*) &query_cache_limit, (gptr*) &query_cache_limit, 0, GET_ULONG,
   REQUIRED_ARG, 1024*1024L, 0, (longlong) ULONG_MAX, 0, 1, 0},
  {"query_cache_min_res_unit", OPT_QUERY_CACHE_MIN_RES_UNIT,
   "minimal size of unit in wich space for results is allocated (last unit will be trimed after writing all result data.",
   (gptr*) &query_cache_min_res_unit, (gptr*) &query_cache_min_res_unit,
   0, GET_ULONG, REQUIRED_ARG, QUERY_CACHE_MIN_RESULT_DATA_SIZE,
   0, (longlong) ULONG_MAX, 0, 1, 0},
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
  {"query_cache_wlock_invalidate", OPT_QUERY_CACHE_WLOCK_INVALIDATE,
   "Invalidate queries in query cache on LOCK for write",
   (gptr*) &global_system_variables.query_cache_wlock_invalidate,
   (gptr*) &max_system_variables.query_cache_wlock_invalidate,
   0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 1, 0},
#endif /*HAVE_QUERY_CACHE*/
  {"query_prealloc_size", OPT_QUERY_PREALLOC_SIZE,
   "Persistent buffer for query parsing and execution",
   (gptr*) &global_system_variables.query_prealloc_size,
   (gptr*) &max_system_variables.query_prealloc_size, 0, GET_ULONG,
   REQUIRED_ARG, QUERY_ALLOC_PREALLOC_SIZE, 16384, ~0L, 0, 1024, 0},
  {"range_alloc_block_size", OPT_RANGE_ALLOC_BLOCK_SIZE,
   "Allocation block size for storing ranges during optimization",
   (gptr*) &global_system_variables.range_alloc_block_size,
   (gptr*) &max_system_variables.range_alloc_block_size, 0, GET_ULONG,
   REQUIRED_ARG, RANGE_ALLOC_BLOCK_SIZE, 4096, ~0L, 0, 1024, 0},
  {"read_buffer_size", OPT_RECORD_BUFFER,
   "Each thread that does a sequential scan allocates a buffer of this size for each table it scans. If you do many sequential scans, you may want to increase this value.",
   (gptr*) &global_system_variables.read_buff_size,
   (gptr*) &max_system_variables.read_buff_size,0, GET_ULONG, REQUIRED_ARG,
   128*1024L, IO_SIZE*2+MALLOC_OVERHEAD, ~0L, MALLOC_OVERHEAD, IO_SIZE, 0},
  {"read_only", OPT_READONLY,
   "Make all tables readonly, with the exception for replication (slave) threads and users with the SUPER privilege",
   (gptr*) &opt_readonly,
   (gptr*) &opt_readonly,
   0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 1, 0},
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
#ifdef HAVE_REPLICATION
  {"relay_log_purge", OPT_RELAY_LOG_PURGE,
   "0 = do not purge relay logs. 1 = purge them as soon as they are no more needed.",
   (gptr*) &relay_log_purge,
   (gptr*) &relay_log_purge, 0, GET_BOOL, NO_ARG,
   1, 0, 1, 0, 1, 0},
  {"relay_log_space_limit", OPT_RELAY_LOG_SPACE_LIMIT,
   "Maximum space to use for all relay logs.",
   (gptr*) &relay_log_space_limit,
   (gptr*) &relay_log_space_limit, 0, GET_ULL, REQUIRED_ARG, 0L, 0L,
   (longlong) ULONG_MAX, 0, 1, 0},
  {"slave_compressed_protocol", OPT_SLAVE_COMPRESSED_PROTOCOL,
   "Use compression on master/slave protocol.",
   (gptr*) &opt_slave_compressed_protocol,
   (gptr*) &opt_slave_compressed_protocol,
   0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 1, 0},
  {"slave_net_timeout", OPT_SLAVE_NET_TIMEOUT,
   "Number of seconds to wait for more data from a master/slave connection before aborting the read.",
   (gptr*) &slave_net_timeout, (gptr*) &slave_net_timeout, 0,
   GET_ULONG, REQUIRED_ARG, SLAVE_NET_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
#endif /* HAVE_REPLICATION */
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
#ifdef HAVE_BERKELEY_DB
  {"sync-bdb-logs", OPT_BDB_SYNC,
   "Synchronously flush logs. Enabled by default",
   (gptr*) &opt_sync_bdb_logs, (gptr*) &opt_sync_bdb_logs, 0, GET_BOOL,
   NO_ARG, 1, 0, 0, 0, 0, 0},
#endif /* HAVE_BERKELEY_DB */
  {"sync-binlog", OPT_SYNC_BINLOG,
   "Sync the binlog to disk after every #th event. \
#=0 (the default) does no sync. Syncing slows MySQL down",
   (gptr*) &sync_binlog_period,
   (gptr*) &sync_binlog_period, 0, GET_ULONG, REQUIRED_ARG, 0, 0, ~0L, 0, 1,
   0},
  {"sync-frm", OPT_SYNC_FRM, "Sync .frm to disk on create. Enabled by default",
   (gptr*) &opt_sync_frm, (gptr*) &opt_sync_frm, 0, GET_BOOL, NO_ARG, 1, 0,
   0, 0, 0, 0},
  {"table_cache", OPT_TABLE_CACHE,
   "The number of open tables for all threads.", (gptr*) &table_cache_size,
   (gptr*) &table_cache_size, 0, GET_ULONG, REQUIRED_ARG, 64, 1, 512*1024L,
   0, 1, 0},
  {"thread_cache_size", OPT_THREAD_CACHE_SIZE,
   "How many threads we should keep in a cache for reuse.",
   (gptr*) &thread_cache_size, (gptr*) &thread_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, 16384, 0, 1, 0},
  {"thread_concurrency", OPT_THREAD_CONCURRENCY,
   "Permits the application to give the threads system a hint for the desired number of threads that should be run at the same time.",
   (gptr*) &concurrency, (gptr*) &concurrency, 0, GET_ULONG, REQUIRED_ARG,
   DEFAULT_CONCURRENCY, 1, 512, 0, 1, 0},
  {"thread_stack", OPT_THREAD_STACK,
   "The stack size for each thread.", (gptr*) &thread_stack,
   (gptr*) &thread_stack, 0, GET_ULONG, REQUIRED_ARG,DEFAULT_THREAD_STACK,
   1024L*128L, ~0L, 0, 1024, 0},
  { "time_format", OPT_TIME_FORMAT,
    "The TIME format (for future).",
    (gptr*) &opt_date_time_formats[MYSQL_TIMESTAMP_TIME],
    (gptr*) &opt_date_time_formats[MYSQL_TIMESTAMP_TIME],
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tmp_table_size", OPT_TMP_TABLE_SIZE,
   "If an in-memory temporary table exceeds this size, MySQL will automatically convert it to an on-disk MyISAM table.",
   (gptr*) &global_system_variables.tmp_table_size,
   (gptr*) &max_system_variables.tmp_table_size, 0, GET_ULONG,
   REQUIRED_ARG, 32*1024*1024L, 1024, ~0L, 0, 1, 0},
  {"transaction_alloc_block_size", OPT_TRANS_ALLOC_BLOCK_SIZE,
   "Allocation block size for transactions to be stored in binary log",
   (gptr*) &global_system_variables.trans_alloc_block_size,
   (gptr*) &max_system_variables.trans_alloc_block_size, 0, GET_ULONG,
   REQUIRED_ARG, QUERY_ALLOC_BLOCK_SIZE, 1024, ~0L, 0, 1024, 0},
  {"transaction_prealloc_size", OPT_TRANS_PREALLOC_SIZE,
   "Persistent buffer for transactions to be stored in binary log",
   (gptr*) &global_system_variables.trans_prealloc_size,
   (gptr*) &max_system_variables.trans_prealloc_size, 0, GET_ULONG,
   REQUIRED_ARG, TRANS_ALLOC_PREALLOC_SIZE, 1024, ~0L, 0, 1024, 0},
  {"wait_timeout", OPT_WAIT_TIMEOUT,
   "The number of seconds the server waits for activity on a connection before closing it.",
   (gptr*) &global_system_variables.net_wait_timeout,
   (gptr*) &max_system_variables.net_wait_timeout, 0, GET_ULONG,
   REQUIRED_ARG, NET_WAIT_TIMEOUT, 1, IF_WIN(INT_MAX32/1000, LONG_TIMEOUT),
   0, 1, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


struct show_var_st status_vars[]= {
  {"Aborted_clients",          (char*) &aborted_threads,        SHOW_LONG},
  {"Aborted_connects",         (char*) &aborted_connects,       SHOW_LONG},
  {"Binlog_cache_disk_use",    (char*) &binlog_cache_disk_use,  SHOW_LONG},
  {"Binlog_cache_use",         (char*) &binlog_cache_use,       SHOW_LONG},
  {"Bytes_received",           (char*) &bytes_received,         SHOW_LONG},
  {"Bytes_sent",               (char*) &bytes_sent,             SHOW_LONG},
  {"Com_admin_commands",       (char*) &com_other,		SHOW_LONG},
  {"Com_alter_db",	       (char*) (com_stat+(uint) SQLCOM_ALTER_DB),SHOW_LONG},
  {"Com_alter_table",	       (char*) (com_stat+(uint) SQLCOM_ALTER_TABLE),SHOW_LONG},
  {"Com_analyze",	       (char*) (com_stat+(uint) SQLCOM_ANALYZE),SHOW_LONG},
  {"Com_backup_table",	       (char*) (com_stat+(uint) SQLCOM_BACKUP_TABLE),SHOW_LONG},
  {"Com_begin",		       (char*) (com_stat+(uint) SQLCOM_BEGIN),SHOW_LONG},
  {"Com_change_db",	       (char*) (com_stat+(uint) SQLCOM_CHANGE_DB),SHOW_LONG},
  {"Com_change_master",	       (char*) (com_stat+(uint) SQLCOM_CHANGE_MASTER),SHOW_LONG},
  {"Com_check",		       (char*) (com_stat+(uint) SQLCOM_CHECK),SHOW_LONG},
  {"Com_checksum",	       (char*) (com_stat+(uint) SQLCOM_CHECKSUM),SHOW_LONG},
  {"Com_commit",	       (char*) (com_stat+(uint) SQLCOM_COMMIT),SHOW_LONG},
  {"Com_create_db",	       (char*) (com_stat+(uint) SQLCOM_CREATE_DB),SHOW_LONG},
  {"Com_create_function",      (char*) (com_stat+(uint) SQLCOM_CREATE_FUNCTION),SHOW_LONG},
  {"Com_create_index",	       (char*) (com_stat+(uint) SQLCOM_CREATE_INDEX),SHOW_LONG},
  {"Com_create_table",	       (char*) (com_stat+(uint) SQLCOM_CREATE_TABLE),SHOW_LONG},
  {"Com_dealloc_sql",          (char*) (com_stat+(uint) 
                                        SQLCOM_DEALLOCATE_PREPARE), SHOW_LONG},
  {"Com_delete",	       (char*) (com_stat+(uint) SQLCOM_DELETE),SHOW_LONG},
  {"Com_delete_multi",	       (char*) (com_stat+(uint) SQLCOM_DELETE_MULTI),SHOW_LONG},
  {"Com_do",                   (char*) (com_stat+(uint) SQLCOM_DO),SHOW_LONG},
  {"Com_drop_db",	       (char*) (com_stat+(uint) SQLCOM_DROP_DB),SHOW_LONG},
  {"Com_drop_function",	       (char*) (com_stat+(uint) SQLCOM_DROP_FUNCTION),SHOW_LONG},
  {"Com_drop_index",	       (char*) (com_stat+(uint) SQLCOM_DROP_INDEX),SHOW_LONG},
  {"Com_drop_table",	       (char*) (com_stat+(uint) SQLCOM_DROP_TABLE),SHOW_LONG},
  {"Com_drop_user",	       (char*) (com_stat+(uint) SQLCOM_DROP_USER),SHOW_LONG},
  {"Com_execute_sql",          (char*) (com_stat+(uint) SQLCOM_EXECUTE), 
   SHOW_LONG},
  {"Com_flush",		       (char*) (com_stat+(uint) SQLCOM_FLUSH),SHOW_LONG},
  {"Com_grant",		       (char*) (com_stat+(uint) SQLCOM_GRANT),SHOW_LONG},
  {"Com_ha_close",	       (char*) (com_stat+(uint) SQLCOM_HA_CLOSE),SHOW_LONG},
  {"Com_ha_open",	       (char*) (com_stat+(uint) SQLCOM_HA_OPEN),SHOW_LONG},
  {"Com_ha_read",	       (char*) (com_stat+(uint) SQLCOM_HA_READ),SHOW_LONG},
  {"Com_help",                 (char*) (com_stat+(uint) SQLCOM_HELP),SHOW_LONG},
  {"Com_insert",	       (char*) (com_stat+(uint) SQLCOM_INSERT),SHOW_LONG},
  {"Com_insert_select",	       (char*) (com_stat+(uint) SQLCOM_INSERT_SELECT),SHOW_LONG},
  {"Com_kill",		       (char*) (com_stat+(uint) SQLCOM_KILL),SHOW_LONG},
  {"Com_load",		       (char*) (com_stat+(uint) SQLCOM_LOAD),SHOW_LONG},
  {"Com_load_master_data",     (char*) (com_stat+(uint) SQLCOM_LOAD_MASTER_DATA),SHOW_LONG},
  {"Com_load_master_table",    (char*) (com_stat+(uint) SQLCOM_LOAD_MASTER_TABLE),SHOW_LONG},
  {"Com_lock_tables",	       (char*) (com_stat+(uint) SQLCOM_LOCK_TABLES),SHOW_LONG},
  {"Com_optimize",	       (char*) (com_stat+(uint) SQLCOM_OPTIMIZE),SHOW_LONG},
  {"Com_preload_keys",	       (char*) (com_stat+(uint) SQLCOM_PRELOAD_KEYS),SHOW_LONG},
  {"Com_prepare_sql",          (char*) (com_stat+(uint) SQLCOM_PREPARE),
   SHOW_LONG}, 
  {"Com_purge",		       (char*) (com_stat+(uint) SQLCOM_PURGE),SHOW_LONG},
  {"Com_purge_before_date",    (char*) (com_stat+(uint) SQLCOM_PURGE_BEFORE),SHOW_LONG},
  {"Com_rename_table",	       (char*) (com_stat+(uint) SQLCOM_RENAME_TABLE),SHOW_LONG},
  {"Com_repair",	       (char*) (com_stat+(uint) SQLCOM_REPAIR),SHOW_LONG},
  {"Com_replace",	       (char*) (com_stat+(uint) SQLCOM_REPLACE),SHOW_LONG},
  {"Com_replace_select",       (char*) (com_stat+(uint) SQLCOM_REPLACE_SELECT),SHOW_LONG},
  {"Com_reset",		       (char*) (com_stat+(uint) SQLCOM_RESET),SHOW_LONG},
  {"Com_restore_table",	       (char*) (com_stat+(uint) SQLCOM_RESTORE_TABLE),SHOW_LONG},
  {"Com_revoke",	       (char*) (com_stat+(uint) SQLCOM_REVOKE),SHOW_LONG},
  {"Com_revoke_all",	       (char*) (com_stat+(uint) SQLCOM_REVOKE_ALL),SHOW_LONG},
  {"Com_rollback",	       (char*) (com_stat+(uint) SQLCOM_ROLLBACK),SHOW_LONG},
  {"Com_savepoint",	       (char*) (com_stat+(uint) SQLCOM_SAVEPOINT),SHOW_LONG},
  {"Com_select",	       (char*) (com_stat+(uint) SQLCOM_SELECT),SHOW_LONG},
  {"Com_set_option",	       (char*) (com_stat+(uint) SQLCOM_SET_OPTION),SHOW_LONG},
  {"Com_show_binlog_events",   (char*) (com_stat+(uint) SQLCOM_SHOW_BINLOG_EVENTS),SHOW_LONG},
  {"Com_show_binlogs",	       (char*) (com_stat+(uint) SQLCOM_SHOW_BINLOGS),SHOW_LONG},
  {"Com_show_charsets",	       (char*) (com_stat+(uint) SQLCOM_SHOW_CHARSETS),SHOW_LONG},
  {"Com_show_collations",      (char*) (com_stat+(uint) SQLCOM_SHOW_COLLATIONS),SHOW_LONG},
  {"Com_show_column_types",    (char*) (com_stat+(uint) SQLCOM_SHOW_COLUMN_TYPES),SHOW_LONG},
  {"Com_show_create_db",       (char*) (com_stat+(uint) SQLCOM_SHOW_CREATE_DB),SHOW_LONG},
  {"Com_show_create_table",    (char*) (com_stat+(uint) SQLCOM_SHOW_CREATE),SHOW_LONG},
  {"Com_show_databases",       (char*) (com_stat+(uint) SQLCOM_SHOW_DATABASES),SHOW_LONG},
  {"Com_show_errors",	       (char*) (com_stat+(uint) SQLCOM_SHOW_ERRORS),SHOW_LONG},
  {"Com_show_fields",	       (char*) (com_stat+(uint) SQLCOM_SHOW_FIELDS),SHOW_LONG},
  {"Com_show_grants",	       (char*) (com_stat+(uint) SQLCOM_SHOW_GRANTS),SHOW_LONG},
  {"Com_show_innodb_status",   (char*) (com_stat+(uint) SQLCOM_SHOW_INNODB_STATUS),SHOW_LONG},
  {"Com_show_keys",	       (char*) (com_stat+(uint) SQLCOM_SHOW_KEYS),SHOW_LONG},
  {"Com_show_logs",	       (char*) (com_stat+(uint) SQLCOM_SHOW_LOGS),SHOW_LONG},
  {"Com_show_master_status",   (char*) (com_stat+(uint) SQLCOM_SHOW_MASTER_STAT),SHOW_LONG},
  {"Com_show_new_master",      (char*) (com_stat+(uint) SQLCOM_SHOW_NEW_MASTER),SHOW_LONG},
  {"Com_show_open_tables",     (char*) (com_stat+(uint) SQLCOM_SHOW_OPEN_TABLES),SHOW_LONG},
  {"Com_show_privileges",      (char*) (com_stat+(uint) SQLCOM_SHOW_PRIVILEGES),SHOW_LONG},
  {"Com_show_processlist",     (char*) (com_stat+(uint) SQLCOM_SHOW_PROCESSLIST),SHOW_LONG},
  {"Com_show_slave_hosts",     (char*) (com_stat+(uint) SQLCOM_SHOW_SLAVE_HOSTS),SHOW_LONG},
  {"Com_show_slave_status",    (char*) (com_stat+(uint) SQLCOM_SHOW_SLAVE_STAT),SHOW_LONG},
  {"Com_show_status",	       (char*) (com_stat+(uint) SQLCOM_SHOW_STATUS),SHOW_LONG},
  {"Com_show_storage_engines", (char*) (com_stat+(uint) SQLCOM_SHOW_STORAGE_ENGINES),SHOW_LONG},
  {"Com_show_tables",	       (char*) (com_stat+(uint) SQLCOM_SHOW_TABLES),SHOW_LONG},
  {"Com_show_variables",       (char*) (com_stat+(uint) SQLCOM_SHOW_VARIABLES),SHOW_LONG},
  {"Com_show_warnings",        (char*) (com_stat+(uint) SQLCOM_SHOW_WARNS),SHOW_LONG},
  {"Com_slave_start",	       (char*) (com_stat+(uint) SQLCOM_SLAVE_START),SHOW_LONG},
  {"Com_slave_stop",	       (char*) (com_stat+(uint) SQLCOM_SLAVE_STOP),SHOW_LONG},
  {"Com_truncate",	       (char*) (com_stat+(uint) SQLCOM_TRUNCATE),SHOW_LONG},
  {"Com_unlock_tables",	       (char*) (com_stat+(uint) SQLCOM_UNLOCK_TABLES),SHOW_LONG},
  {"Com_update",	       (char*) (com_stat+(uint) SQLCOM_UPDATE),SHOW_LONG},
  {"Com_update_multi",	       (char*) (com_stat+(uint) SQLCOM_UPDATE_MULTI),SHOW_LONG},
  {"Connections",              (char*) &thread_id,              SHOW_LONG_CONST},
  {"Created_tmp_disk_tables",  (char*) &created_tmp_disk_tables,SHOW_LONG},
  {"Created_tmp_files",	       (char*) &my_tmp_file_created,	SHOW_LONG},
  {"Created_tmp_tables",       (char*) &created_tmp_tables,     SHOW_LONG},
  {"Delayed_errors",           (char*) &delayed_insert_errors,  SHOW_LONG},
  {"Delayed_insert_threads",   (char*) &delayed_insert_threads, SHOW_LONG_CONST},
  {"Delayed_writes",           (char*) &delayed_insert_writes,  SHOW_LONG},
  {"Flush_commands",           (char*) &refresh_version,        SHOW_LONG_CONST},
  {"Handler_commit",           (char*) &ha_commit_count,        SHOW_LONG},
  {"Handler_delete",           (char*) &ha_delete_count,        SHOW_LONG},
  {"Handler_discover",         (char*) &ha_discover_count,      SHOW_LONG},
  {"Handler_read_first",       (char*) &ha_read_first_count,    SHOW_LONG},
  {"Handler_read_key",         (char*) &ha_read_key_count,      SHOW_LONG},
  {"Handler_read_next",        (char*) &ha_read_next_count,     SHOW_LONG},
  {"Handler_read_prev",        (char*) &ha_read_prev_count,     SHOW_LONG},
  {"Handler_read_rnd",         (char*) &ha_read_rnd_count,      SHOW_LONG},
  {"Handler_read_rnd_next",    (char*) &ha_read_rnd_next_count, SHOW_LONG},
  {"Handler_rollback",         (char*) &ha_rollback_count,      SHOW_LONG},
  {"Handler_update",           (char*) &ha_update_count,        SHOW_LONG},
  {"Handler_write",            (char*) &ha_write_count,         SHOW_LONG},
  {"Key_blocks_not_flushed",   (char*) &dflt_key_cache_var.global_blocks_changed,
   SHOW_KEY_CACHE_LONG},
  {"Key_blocks_unused",        (char*) &dflt_key_cache_var.blocks_unused,
   SHOW_KEY_CACHE_CONST_LONG},
  {"Key_blocks_used",          (char*) &dflt_key_cache_var.blocks_used,
   SHOW_KEY_CACHE_CONST_LONG},
  {"Key_read_requests",        (char*) &dflt_key_cache_var.global_cache_r_requests,
   SHOW_KEY_CACHE_LONG},
  {"Key_reads",                (char*) &dflt_key_cache_var.global_cache_read,
   SHOW_KEY_CACHE_LONG},
  {"Key_write_requests",       (char*) &dflt_key_cache_var.global_cache_w_requests,
   SHOW_KEY_CACHE_LONG},
  {"Key_writes",               (char*) &dflt_key_cache_var.global_cache_write,
   SHOW_KEY_CACHE_LONG},
  {"Max_used_connections",     (char*) &max_used_connections,  SHOW_LONG},
  {"Not_flushed_delayed_rows", (char*) &delayed_rows_in_use,    SHOW_LONG_CONST},
  {"Open_files",               (char*) &my_file_opened,         SHOW_LONG_CONST},
  {"Open_streams",             (char*) &my_stream_opened,       SHOW_LONG_CONST},
  {"Open_tables",              (char*) 0,                       SHOW_OPENTABLES},
  {"Opened_tables",            (char*) &opened_tables,          SHOW_LONG},
#ifdef HAVE_QUERY_CACHE
  {"Qcache_free_blocks",       (char*) &query_cache.free_memory_blocks,
   SHOW_LONG_CONST},
  {"Qcache_free_memory",       (char*) &query_cache.free_memory,
   SHOW_LONG_CONST},
  {"Qcache_hits",              (char*) &query_cache.hits,       SHOW_LONG},
  {"Qcache_inserts",           (char*) &query_cache.inserts,    SHOW_LONG},
  {"Qcache_lowmem_prunes",     (char*) &query_cache.lowmem_prunes, SHOW_LONG},
  {"Qcache_not_cached",        (char*) &query_cache.refused,    SHOW_LONG},
  {"Qcache_queries_in_cache",  (char*) &query_cache.queries_in_cache, SHOW_LONG_CONST},
  {"Qcache_total_blocks",      (char*) &query_cache.total_blocks,
   SHOW_LONG_CONST},
#endif /*HAVE_QUERY_CACHE*/
  {"Questions",                (char*) 0,                       SHOW_QUESTION},
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
  {"Ssl_accept_renegotiates",  (char*) 0, 	SHOW_SSL_CTX_SESS_ACCEPT_RENEGOTIATE},
  {"Ssl_accepts",              (char*) 0,  	SHOW_SSL_CTX_SESS_ACCEPT},
  {"Ssl_callback_cache_hits",  (char*) 0,	SHOW_SSL_CTX_SESS_CB_HITS},
  {"Ssl_cipher",               (char*) 0,  	SHOW_SSL_GET_CIPHER},
  {"Ssl_cipher_list",          (char*) 0,  	SHOW_SSL_GET_CIPHER_LIST},
  {"Ssl_client_connects",      (char*) 0,	SHOW_SSL_CTX_SESS_CONNECT},
  {"Ssl_connect_renegotiates", (char*) 0, 	SHOW_SSL_CTX_SESS_CONNECT_RENEGOTIATE},
  {"Ssl_ctx_verify_depth",     (char*) 0,	SHOW_SSL_CTX_GET_VERIFY_DEPTH},
  {"Ssl_ctx_verify_mode",      (char*) 0,	SHOW_SSL_CTX_GET_VERIFY_MODE},
  {"Ssl_default_timeout",      (char*) 0,  	SHOW_SSL_GET_DEFAULT_TIMEOUT},
  {"Ssl_finished_accepts",     (char*) 0,  	SHOW_SSL_CTX_SESS_ACCEPT_GOOD},
  {"Ssl_finished_connects",    (char*) 0,  	SHOW_SSL_CTX_SESS_CONNECT_GOOD},
  {"Ssl_session_cache_hits",   (char*) 0,	SHOW_SSL_CTX_SESS_HITS},
  {"Ssl_session_cache_misses", (char*) 0,	SHOW_SSL_CTX_SESS_MISSES},
  {"Ssl_session_cache_mode",   (char*) 0,	SHOW_SSL_CTX_GET_SESSION_CACHE_MODE},
  {"Ssl_session_cache_overflows", (char*) 0,	SHOW_SSL_CTX_SESS_CACHE_FULL},
  {"Ssl_session_cache_size",   (char*) 0,	SHOW_SSL_CTX_SESS_GET_CACHE_SIZE},
  {"Ssl_session_cache_timeouts", (char*) 0,	SHOW_SSL_CTX_SESS_TIMEOUTS},
  {"Ssl_sessions_reused",      (char*) 0,	SHOW_SSL_SESSION_REUSED},
  {"Ssl_used_session_cache_entries",(char*) 0,	SHOW_SSL_CTX_SESS_NUMBER},
  {"Ssl_verify_depth",         (char*) 0,	SHOW_SSL_GET_VERIFY_DEPTH},
  {"Ssl_verify_mode",          (char*) 0,	SHOW_SSL_GET_VERIFY_MODE},
  {"Ssl_version",   	       (char*) 0,  	SHOW_SSL_GET_VERSION},
#endif /* HAVE_OPENSSL */
  {"Table_locks_immediate",    (char*) &locks_immediate,        SHOW_LONG},
  {"Table_locks_waited",       (char*) &locks_waited,           SHOW_LONG},
  {"Threads_cached",           (char*) &cached_thread_count,    SHOW_LONG_CONST},
  {"Threads_connected",        (char*) &thread_count,           SHOW_INT_CONST},
  {"Threads_created",	       (char*) &thread_created,		SHOW_LONG_CONST},
  {"Threads_running",          (char*) &thread_running,         SHOW_INT_CONST},
  {"Uptime",                   (char*) 0,                       SHOW_STARTTIME},
  {NullS, NullS, SHOW_LONG}
};

static void print_version(void)
{
  set_server_version();
  printf("%s  Ver %s for %s on %s (%s)\n",my_progname,
	 server_version,SYSTEM_TYPE,MACHINE_TYPE, MYSQL_COMPILATION_COMMENT);
}

static void use_help(void)
{
  print_version();
  printf("Use '--help' or '--no-defaults --help' for a list of available options\n");
}

static void usage(void)
{
  if (!(default_charset_info= get_charset_by_csname(default_character_set_name,
					           MY_CS_PRIMARY,
						   MYF(MY_WME))))
    exit(1);
  if (!default_collation_name)
    default_collation_name= (char*) default_charset_info->name;
  print_version();
  puts("\
Copyright (C) 2000 MySQL AB, by Monty and others\n\
This software comes with ABSOLUTELY NO WARRANTY. This is free software,\n\
and you are welcome to modify and redistribute it under the GPL license\n\n\
Starts the MySQL database server\n");

  printf("Usage: %s [OPTIONS]\n", my_progname);
  if (!opt_verbose)
    puts("\nFor more help options (several pages), use mysqld --verbose --help\n");
  else
  {
#ifdef __WIN__
  puts("NT and Win32 specific options:\n\
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
  print_defaults(MYSQL_CONFIG_NAME,load_default_groups);
  puts("");
  fix_paths();
  set_ports();

  my_print_help(my_long_options);
  my_print_variables(my_long_options);

  puts("\n\
To see what values a running MySQL server is using, type\n\
'mysqladmin variables' instead of 'mysqld --verbose --help'.\n");
  }
}


/*
  Initialize all MySQL global variables to default values

  SYNOPSIS
    mysql_init_variables()

  NOTES
    The reason to set a lot of global variables to zero is to allow one to
    restart the embedded server with a clean environment
    It's also needed on some exotic platforms where global variables are
    not set to 0 when a program starts.

    We don't need to set numeric variables refered to in my_long_options
    as these are initialized by my_getopt.
*/

static void mysql_init_variables(void)
{
  /* Things reset to zero */
  opt_skip_slave_start= opt_reckless_slave = 0;
  mysql_home[0]= pidfile_name[0]= log_error_file[0]= 0;
  opt_log= opt_update_log= opt_bin_log= opt_slow_log= 0;
  opt_disable_networking= opt_skip_show_db=0;
  opt_logname= opt_update_logname= opt_binlog_index_name= opt_slow_logname=0;
  opt_secure_auth= 0;
  opt_bootstrap= opt_myisam_log= 0;
  mqh_used= 0;
  segfaulted= kill_in_progress= 0;
  cleanup_done= 0;
  defaults_argv= 0;
  server_id_supplied= 0;
  test_flags= select_errors= dropping_tables= ha_open_options=0;
  thread_count= thread_running= kill_cached_threads= wake_thread=0;
  slave_open_temp_tables= 0;
  com_other= 0;
  cached_thread_count= 0;
  bytes_sent= bytes_received= 0;
  opt_endinfo= using_udf_functions= 0;
  opt_using_transactions= using_update_log= 0;
  abort_loop= select_thread_in_use= signal_thread_in_use= 0;
  ready_to_exit= shutdown_in_progress= grant_option= 0;
  long_query_count= aborted_threads= aborted_connects= 0;
  delayed_insert_threads= delayed_insert_writes= delayed_rows_in_use= 0;
  delayed_insert_errors= thread_created= 0;
  filesort_rows= filesort_range_count= filesort_scan_count= 0;
  filesort_merge_passes= select_range_check_count= select_range_count= 0;
  select_scan_count= select_full_range_join_count= select_full_join_count= 0;
  specialflag= opened_tables= created_tmp_tables= created_tmp_disk_tables= 0;
  binlog_cache_use=  binlog_cache_disk_use= 0;
  max_used_connections= slow_launch_threads = 0;
  mysqld_user= mysqld_chroot= opt_init_file= opt_bin_logname = 0;
  errmesg= 0;
  mysqld_unix_port= opt_mysql_tmpdir= my_bind_addr_str= NullS;
  bzero((gptr) &mysql_tmpdir_list, sizeof(mysql_tmpdir_list));
  bzero((gptr) &com_stat, sizeof(com_stat));

  /* Character sets */
  system_charset_info= &my_charset_utf8_general_ci;
  files_charset_info= &my_charset_utf8_general_ci;
  national_charset_info= &my_charset_utf8_general_ci;
  table_alias_charset= &my_charset_bin;

  opt_date_time_formats[0]= opt_date_time_formats[1]= opt_date_time_formats[2]= 0;

  /* Things with default values that are not zero */
  delay_key_write_options= (uint) DELAY_KEY_WRITE_ON;
  opt_specialflag= SPECIAL_ENGLISH;
  unix_sock= ip_sock= INVALID_SOCKET;
  mysql_home_ptr= mysql_home;
  pidfile_name_ptr= pidfile_name;
  log_error_file_ptr= log_error_file;
  language_ptr= language;
  mysql_data_home= mysql_real_data_home;
  thd_startup_options= (OPTION_UPDATE_LOG | OPTION_AUTO_IS_NULL |
			OPTION_BIN_LOG | OPTION_QUOTE_SHOW_CREATE);
  protocol_version= PROTOCOL_VERSION;
  what_to_log= ~ (1L << (uint) COM_TIME);
  refresh_version= flush_version= 1L;	/* Increments on each reload */
  query_id= thread_id= 1L;
  strmov(server_version, MYSQL_SERVER_VERSION);
  myisam_recover_options_str= sql_mode_str= "OFF";
  my_bind_addr = htonl(INADDR_ANY);
  threads.empty();
  thread_cache.empty();
  key_caches.empty();
  multi_keycache_init();
  if (!(sql_key_cache= get_or_create_key_cache(default_key_cache_base.str,
					       default_key_cache_base.length)))
    exit(1);

  /* Initialize structures that is used when processing options */
  replicate_rewrite_db.empty();
  replicate_do_db.empty();
  replicate_ignore_db.empty();
  binlog_do_db.empty();
  binlog_ignore_db.empty();

  /* Set directory paths */
  strmake(language, LANGUAGE, sizeof(language)-1);
  strmake(mysql_real_data_home, get_relative_path(DATADIR),
	  sizeof(mysql_real_data_home)-1);
  mysql_data_home_buff[0]=FN_CURLIB;	// all paths are relative from here
  mysql_data_home_buff[1]=0;

  /* Replication parameters */
  master_user= (char*) "test";
  master_password= master_host= 0;
  master_info_file= (char*) "master.info",
    relay_log_info_file= (char*) "relay-log.info";
  master_ssl_key= master_ssl_cert= master_ssl_ca= 
    master_ssl_capath= master_ssl_cipher= 0;
  report_user= report_password = report_host= 0;	/* TO BE DELETED */
  opt_relay_logname= opt_relaylog_index_name= 0;

  /* Variables in libraries */
  charsets_dir= 0;
  default_character_set_name= (char*) MYSQL_DEFAULT_CHARSET_NAME;
  default_collation_name= (char*) MYSQL_DEFAULT_COLLATION_NAME;
  sys_charset_system.value= (char*) system_charset_info->csname;


  /* Set default values for some option variables */
  global_system_variables.table_type=   DB_TYPE_MYISAM;
  global_system_variables.tx_isolation= ISO_REPEATABLE_READ;
  global_system_variables.select_limit= (ulonglong) HA_POS_ERROR;
  max_system_variables.select_limit=    (ulonglong) HA_POS_ERROR;
  global_system_variables.max_join_size= (ulonglong) HA_POS_ERROR;
  max_system_variables.max_join_size=   (ulonglong) HA_POS_ERROR;
  global_system_variables.old_passwords= 0;

  /* Variables that depends on compile options */
#ifndef DBUG_OFF
  default_dbug_option=IF_WIN("d:t:i:O,\\mysqld.trace",
			     "d:t:i:o,/tmp/mysqld.trace");
#endif
  opt_error_log= IF_WIN(1,0);
#ifdef HAVE_BERKELEY_DB
  have_berkeley_db= SHOW_OPTION_YES;
#else
  have_berkeley_db= SHOW_OPTION_NO;
#endif
#ifdef HAVE_INNOBASE_DB
  have_innodb=SHOW_OPTION_YES;
#else
  have_innodb=SHOW_OPTION_NO;
#endif
#ifdef HAVE_ISAM
  have_isam=SHOW_OPTION_YES;
#else
  have_isam=SHOW_OPTION_NO;
#endif
#ifdef HAVE_EXAMPLE_DB
  have_example_db= SHOW_OPTION_YES;
#else
  have_example_db= SHOW_OPTION_NO;
#endif
#ifdef HAVE_ARCHIVE_DB
  have_archive_db= SHOW_OPTION_YES;
#else
  have_archive_db= SHOW_OPTION_NO;
#endif
#ifdef HAVE_CSV_DB
  have_csv_db= SHOW_OPTION_YES;
#else
  have_csv_db= SHOW_OPTION_NO;
#endif
#ifdef HAVE_NDBCLUSTER_DB
  have_ndbcluster=SHOW_OPTION_DISABLED;
#else
  have_ndbcluster=SHOW_OPTION_NO;
#endif
#ifdef USE_RAID
  have_raid=SHOW_OPTION_YES;
#else
  have_raid=SHOW_OPTION_NO;
#endif
#ifdef HAVE_OPENSSL
  have_openssl=SHOW_OPTION_YES;
#else
  have_openssl=SHOW_OPTION_NO;
#endif
#ifdef HAVE_BROKEN_REALPATH
  have_symlink=SHOW_OPTION_NO;
#else
  have_symlink=SHOW_OPTION_YES;
#endif
#ifdef HAVE_QUERY_CACHE
  have_query_cache=SHOW_OPTION_YES;
#else
  have_query_cache=SHOW_OPTION_NO;
#endif
#ifdef HAVE_SPATIAL
  have_geometry=SHOW_OPTION_YES;
#else
  have_geometry=SHOW_OPTION_NO;
#endif
#ifdef HAVE_RTREE_KEYS
  have_rtree_keys=SHOW_OPTION_YES;
#else
  have_rtree_keys=SHOW_OPTION_NO;
#endif
#ifdef HAVE_CRYPT
  have_crypt=SHOW_OPTION_YES;
#else
  have_crypt=SHOW_OPTION_NO;
#endif
#ifdef HAVE_COMPRESS
  have_compress= SHOW_OPTION_YES;
#else
  have_compress= SHOW_OPTION_NO;
#endif
#ifdef HAVE_LIBWRAP
  libwrapName= NullS;
#endif
#ifdef HAVE_OPENSSL
  des_key_file = 0;
  ssl_acceptor_fd= 0;
#endif
#ifdef HAVE_SMEM
  shared_memory_base_name= default_shared_memory_base_name;
#endif
#if !defined(my_pthread_setprio) && !defined(HAVE_PTHREAD_SETSCHEDPARAM)
  opt_specialflag |= SPECIAL_NO_PRIOR;
#endif

#if defined(__WIN__) || defined(__NETWARE__)
  /* Allow Win32 and NetWare users to move MySQL anywhere */
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
}


extern "C" my_bool
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
    global_system_variables.sql_mode= fix_sql_mode(MODE_ANSI);
    global_system_variables.tx_isolation= ISO_SERIALIZABLE;
    break;
  case 'b':
    strmake(mysql_home,argument,sizeof(mysql_home)-1);
    break;
  case 'C':
    default_collation_name= 0;
    break;
  case 'l':
    opt_log=1;
    break;
  case 'h':
    strmake(mysql_real_data_home,argument, sizeof(mysql_real_data_home)-1);
    /* Correct pointer set by my_getopt (for embedded library) */
    mysql_data_home= mysql_real_data_home;
    break;
  case 'u':
    if (!mysqld_user || !strcmp(mysqld_user, argument))
      mysqld_user= argument;
    else
      sql_print_warning("Ignoring user change to '%s' because the user was set to '%s' earlier on the command line\n", argument, mysqld_user);
    break;
  case 'L':
    strmake(language, argument, sizeof(language)-1);
    break;
#ifdef HAVE_REPLICATION
  case OPT_SLAVE_SKIP_ERRORS:
    init_slave_skip_errors(argument);
    break;
#endif
  case OPT_SAFEMALLOC_MEM_LIMIT:
#if !defined(DBUG_OFF) && defined(SAFEMALLOC)
    sf_malloc_mem_limit = atoi(argument);
#endif
    break;
#ifdef EMBEDDED_LIBRARY
  case OPT_MAX_ALLOWED_PACKET:
    max_allowed_packet= atoi(argument);
    global_system_variables.max_allowed_packet= max_allowed_packet;
    break;
  case OPT_NET_BUFFER_LENGTH:
    net_buffer_length=  atoi(argument);
    global_system_variables.net_buffer_length= net_buffer_length;
    break;
#endif
#include <sslopt-case.h>
  case 'V':
    print_version();
    exit(0);
  case 'W':
    if (!argument)
      global_system_variables.log_warnings++;
    else if (argument == disabled_my_option)
      global_system_variables.log_warnings= 0L;
    else
      global_system_variables.log_warnings= atoi(argument);
    break;
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
  case (int) OPT_ERROR_LOG_FILE:
    opt_error_log= 1;
    break;
#ifdef HAVE_REPLICATION
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
    while (my_isspace(mysqld_charset, *p) && p > argument)
      *p-- = 0;
    if (p == argument)
    {
      fprintf(stderr,
	      "Bad syntax in replicate-rewrite-db - empty FROM db!\n");
      exit(1);
    }
    *val= 0;
    val+= 2;
    while (*val && my_isspace(mysqld_charset, *val))
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
#endif /* HAVE_REPLICATION */
  case (int) OPT_SLOW_QUERY_LOG:
    opt_slow_log=1;
    break;
  case (int) OPT_SKIP_NEW:
    opt_specialflag|= SPECIAL_NO_NEW_FUNC;
    delay_key_write_options= (uint) DELAY_KEY_WRITE_NONE;
    myisam_concurrent_insert=0;
    myisam_recover_options= HA_RECOVER_NONE;
    my_use_symdir=0;
    ha_open_options&= ~(HA_OPEN_ABORT_IF_CRASHED | HA_OPEN_DELAY_KEY_WRITE);
#ifdef HAVE_QUERY_CACHE
    query_cache_size=0;
#endif
    break;
  case (int) OPT_SAFE:
    opt_specialflag|= SPECIAL_SAFE_MODE;
    delay_key_write_options= (uint) DELAY_KEY_WRITE_NONE;
    myisam_recover_options= HA_RECOVER_DEFAULT;
    ha_open_options&= ~(HA_OPEN_DELAY_KEY_WRITE);
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
  case (int) OPT_SKIP_NETWORKING:
#if defined(__NETWARE__)
    sql_perror("Can't start server: skip-networking option is currently not supported on NetWare");
    exit(1);
#endif
    opt_disable_networking=1;
    mysqld_port=0;
    break;
  case (int) OPT_SKIP_SHOW_DB:
    opt_skip_show_db=1;
    opt_specialflag|=SPECIAL_SKIP_SHOW_DB;
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
    my_use_symdir=0;
    break;
  case (int) OPT_BIND_ADDRESS:
    if ((my_bind_addr= (ulong) inet_addr(argument)) == INADDR_NONE)
    {
      struct hostent *ent;
      if (argument[0])
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
  case OPT_CONSOLE:
    if (opt_console)
      opt_error_log= 0;			// Force logs to stdout
    break;
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
  case OPT_STORAGE_ENGINE:
  {
    if ((enum db_type)((global_system_variables.table_type=
	  ha_resolve_by_name(argument, strlen(argument)))) == DB_TYPE_UNKNOWN)
    {
      fprintf(stderr,"Unknown table type: %s\n",argument);
      exit(1);
    }
    break;
  }
  case OPT_SERVER_ID:
    server_id_supplied = 1;
    break;
  case OPT_DELAY_KEY_WRITE_ALL:
    if (argument != disabled_my_option)
      argument= (char*) "ALL";
    /* Fall through */
  case OPT_DELAY_KEY_WRITE:
    if (argument == disabled_my_option)
      delay_key_write_options= (uint) DELAY_KEY_WRITE_NONE;
    else if (! argument)
      delay_key_write_options= (uint) DELAY_KEY_WRITE_ON;
    else
    {
      int type;
      if ((type=find_type(argument, &delay_key_write_typelib, 2)) <= 0)
      {
	fprintf(stderr,"Unknown delay_key_write type: %s\n",argument);
	exit(1);
      }
      delay_key_write_options= (uint) type-1;
    }
    break;
  case OPT_CHARSETS_DIR:
    strmake(mysql_charsets_dir, argument, sizeof(mysql_charsets_dir)-1);
    charsets_dir = mysql_charsets_dir;
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
    /* Deprecated option */
    opt_sync_bdb_logs= 0;
    /* Fall through */
  case OPT_BDB_SYNC:
    if (!opt_sync_bdb_logs)
      berkeley_env_flags|= DB_TXN_NOSYNC;
    else
      berkeley_env_flags&= ~DB_TXN_NOSYNC;
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
      int err;
      char *end;
      uint length= strlen(argument);
      long value= my_strntol(&my_charset_latin1, argument, length, 10, &end, &err);
      if (test_if_int(argument,(uint) length, end, &my_charset_latin1))
	berkeley_lock_scan_time= value;
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
    berkeley_shared_data= 1;
    break;
#endif /* HAVE_BERKELEY_DB */
  case OPT_BDB:
#ifdef HAVE_BERKELEY_DB
    if (opt_bdb)
      have_berkeley_db= SHOW_OPTION_YES;
    else
      have_berkeley_db= SHOW_OPTION_DISABLED;
#else
    if (opt_bdb)
      push_warning(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
		   0,"this binary does not contain BDB storage engine");
#endif
    break;
  case OPT_ISAM:
#ifdef HAVE_ISAM
    if (opt_isam)
      have_isam= SHOW_OPTION_YES;
    else
      have_isam= SHOW_OPTION_DISABLED;
#else
    if (opt_isam)
      push_warning(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
		   0,"this binary does not contain ISAM storage engine");
#endif
    break;
  case OPT_NDBCLUSTER:
#ifdef HAVE_NDBCLUSTER_DB
    if (opt_ndbcluster)
      have_ndbcluster= SHOW_OPTION_YES;
    else
      have_ndbcluster= SHOW_OPTION_DISABLED;
#else
    if (opt_ndbcluster)
      push_warning(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
		   0,"this binary does not contain NDBCLUSTER storage engine");
#endif
    break;
  case OPT_INNODB:
#ifdef HAVE_INNOBASE_DB
    if (opt_innodb)
      have_innodb= SHOW_OPTION_YES;
    else
      have_innodb= SHOW_OPTION_DISABLED;
#else
    if (opt_innodb)
      push_warning(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
		   0,"this binary does not contain INNODB storage engine");
#endif
    break;
  case OPT_INNODB_DATA_FILE_PATH:
#ifdef HAVE_INNOBASE_DB
    innobase_data_file_path= argument;
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
    sql_mode_str= argument;
    if ((global_system_variables.sql_mode=
         find_bit_type(argument, &sql_mode_typelib)) == ~(ulong) 0)
    {
      fprintf(stderr, "Unknown option to sql-mode: %s\n", argument);
      exit(1);
    }
    global_system_variables.sql_mode= fix_sql_mode(global_system_variables.
						   sql_mode);
    break;
  }
  case OPT_FT_BOOLEAN_SYNTAX:
    if (ft_boolean_check_syntax_string((byte*) argument))
    {
      fprintf(stderr, "Invalid ft-boolean-syntax string: %s\n", argument);
      exit(1);
    }
    strmake(ft_boolean_syntax, argument, sizeof(ft_boolean_syntax)-1);
    break;
  case OPT_SKIP_SAFEMALLOC:
#ifdef SAFEMALLOC
    sf_malloc_quick=1;
#endif
    break;
  case OPT_LOWER_CASE_TABLE_NAMES:
    lower_case_table_names= argument ? atoi(argument) : 1;
    lower_case_table_names_used= 1;
    break;
  }
  return 0;
}
	/* Initiates DEBUG - but no debugging here ! */

extern "C" gptr *
mysql_getopt_value(const char *keyname, uint key_length,
		   const struct my_option *option)
{
  switch (option->id) {
  case OPT_KEY_BUFFER_SIZE:
  case OPT_KEY_CACHE_BLOCK_SIZE:
  case OPT_KEY_CACHE_DIVISION_LIMIT:
  case OPT_KEY_CACHE_AGE_THRESHOLD:
  {
    KEY_CACHE *key_cache;
    if (!(key_cache= get_or_create_key_cache(keyname, key_length)))
      exit(1);
    switch (option->id) {
    case OPT_KEY_BUFFER_SIZE:
      return (gptr*) &key_cache->param_buff_size;
    case OPT_KEY_CACHE_BLOCK_SIZE:
      return (gptr*) &key_cache->param_block_size;
    case OPT_KEY_CACHE_DIVISION_LIMIT:
      return (gptr*) &key_cache->param_division_limit;
    case OPT_KEY_CACHE_AGE_THRESHOLD:
      return (gptr*) &key_cache->param_age_threshold;
    }
  }
  }
 return option->value;
}


void option_error_reporter(enum loglevel level, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vprint_msg_to_log(level, format, args);
  va_end(args);
}


static void get_options(int argc,char **argv)
{
  int ho_error;

  my_getopt_register_get_addr(mysql_getopt_value);
  strmake(def_ft_boolean_syntax, ft_boolean_syntax,
	  sizeof(ft_boolean_syntax)-1);
  my_getopt_error_reporter= option_error_reporter;
  if ((ho_error= handle_options(&argc, &argv, my_long_options,
                                get_one_option)))
    exit(ho_error);
  if (argc > 0)
  {
    fprintf(stderr, "%s: Too many arguments (first extra is '%s').\nUse --help to get a list of available options\n", my_progname, *argv);
    /* FIXME add EXIT_TOO_MANY_ARGUMENTS to "mysys_err.h" and return that code? */
    exit(1);
  }

  if (opt_help)
  {
    usage();
    exit(0);
  }
#if defined(HAVE_BROKEN_REALPATH)
  my_use_symdir=0;
  my_disable_symlinks=1;
  have_symlink=SHOW_OPTION_NO;
#else
  if (!my_use_symdir)
  {
    my_disable_symlinks=1;
    have_symlink=SHOW_OPTION_DISABLED;
  }
#endif
  if (opt_debugging)
  {
    /* Allow break with SIGINT, no core or stack trace */
    test_flags|= TEST_SIGINT | TEST_NO_STACKTRACE;
    test_flags&= ~TEST_CORE_ON_SIGNAL;
  }
  /* Set global MyISAM variables from delay_key_write_options */
  fix_delay_key_write((THD*) 0, OPT_GLOBAL);

#ifndef EMBEDDED_LIBRARY
  if (mysqld_chroot)
    set_root(mysqld_chroot);
#endif
  fix_paths();

  /*
    Set some global variables from the global_system_variables
    In most cases the global variables will not be used
  */
  my_disable_locking= myisam_single_user= test(opt_external_locking == 0);
  my_default_record_cache_size=global_system_variables.read_buff_size;
  myisam_max_temp_length=
    (my_off_t) global_system_variables.myisam_max_sort_file_size;
  myisam_max_extra_temp_length=
    (my_off_t) global_system_variables.myisam_max_extra_sort_file_size;

  /* Set global variables based on startup options */
  myisam_block_size=(uint) 1 << my_bit_log2(opt_myisam_block_size);
  table_alias_charset= (lower_case_table_names ?
			files_charset_info :
			&my_charset_bin);

  if (opt_short_log_format)
    opt_specialflag|= SPECIAL_SHORT_LOG_FORMAT;
  if (opt_log_queries_not_using_indexes)
    opt_specialflag|= SPECIAL_LOG_QUERIES_NOT_USING_INDEXES;

  if (init_global_datetime_format(MYSQL_TIMESTAMP_DATE,
				  &global_system_variables.date_format) ||
      init_global_datetime_format(MYSQL_TIMESTAMP_TIME,
				  &global_system_variables.time_format) ||
      init_global_datetime_format(MYSQL_TIMESTAMP_DATETIME,
				  &global_system_variables.datetime_format))
    exit(1);
}


/*
  Create version name for running mysqld version
  We automaticly add suffixes -debug, -embedded and -log to the version
  name to make the version more descriptive.
  (MYSQL_SERVER_SUFFIX is set by the compilation environment)
*/

static void set_server_version(void)
{
  char *end= strxmov(server_version, MYSQL_SERVER_VERSION,
                     MYSQL_SERVER_SUFFIX_STR, NullS);
#ifdef EMBEDDED_LIBRARY
  end= strmov(end, "-embedded");
#endif
#ifndef DBUG_OFF
  if (!strstr(MYSQL_SERVER_SUFFIX_STR, "-debug"))
    end= strmov(end, "-debug");
#endif
  if (opt_log || opt_update_log || opt_slow_log || opt_bin_log)
    strmov(end, "-log");                        // This may slow down system
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
  char buff[FN_REFLEN],*pos;
  convert_dirname(mysql_home,mysql_home,NullS);
  /* Resolve symlinks to allow 'mysql_home' to be a relative symlink */
  my_realpath(mysql_home,mysql_home,MYF(0));
  /* Ensure that mysql_home ends in FN_LIBCHAR */
  pos=strend(mysql_home);
  if (pos[-1] != FN_LIBCHAR)
  {
    pos[0]= FN_LIBCHAR;
    pos[1]= 0;
  }
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
  }
  (void) my_load_path(mysql_charsets_dir, mysql_charsets_dir, buff);
  charsets_dir=mysql_charsets_dir;

  if (init_tmpdir(&mysql_tmpdir_list, opt_mysql_tmpdir))
    exit(1);
#ifdef HAVE_REPLICATION
  if (!slave_load_tmpdir)
  {
    if (!(slave_load_tmpdir = (char*) my_strdup(mysql_tmpdir, MYF(MY_FAE))))
      exit(1);
  }
#endif /* HAVE_REPLICATION */
}


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
	if (my_toupper(mysqld_charset,*i++) !=
            my_toupper(mysqld_charset,*j++))
	  goto skip;
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
skip: ;
    }
    if (found_count != 1)
      DBUG_RETURN(~(ulong) 0);				// No unique value
    found|=found_int;
    pos=end+1;
  }

  DBUG_PRINT("exit",("bit-field: %ld",(ulong) found));
  DBUG_RETURN(found);
} /* find_bit_type */


/*
  Check if file system used for databases is case insensitive

  SYNOPSIS
    test_if_case_sensitive()
    dir_name			Directory to test

  RETURN
    -1  Don't know (Test failed)
    0   File system is case sensitive
    1   File system is case insensitive
*/

static int test_if_case_insensitive(const char *dir_name)
{
  int result= 0;
  File file;
  char buff[FN_REFLEN], buff2[FN_REFLEN];
  MY_STAT stat_info;
  DBUG_ENTER("test_if_case_insensitive");

  fn_format(buff, glob_hostname, dir_name, ".lower-test",
	    MY_UNPACK_FILENAME | MY_REPLACE_EXT | MY_REPLACE_DIR);
  fn_format(buff2, glob_hostname, dir_name, ".LOWER-TEST",
	    MY_UNPACK_FILENAME | MY_REPLACE_EXT | MY_REPLACE_DIR);
  (void) my_delete(buff2, MYF(0));
  if ((file= my_create(buff, 0666, O_RDWR, MYF(0))) < 0)
  {
    sql_print_warning("Can't create test file %s", buff);
    DBUG_RETURN(-1);
  }
  my_close(file, MYF(0));
  if (my_stat(buff2, &stat_info, MYF(0)))
    result= 1;					// Can access file
  (void) my_delete(buff, MYF(MY_WME));
  DBUG_PRINT("exit", ("result: %d", result));
  DBUG_RETURN(result);
}


/* Create file to store pid number */

static void create_pid_file()
{
  File file;
  if ((file = my_create(pidfile_name,0664,
			O_WRONLY | O_TRUNC, MYF(MY_WME))) >= 0)
  {
    char buff[21], *end;
    end= int10_to_str((long) getpid(), buff, 10);
    *end++= '\n';
    (void) my_write(file, (byte*) buff, (uint) (end-buff),MYF(MY_WME));
    (void) my_close(file, MYF(0));
  }
}


/*****************************************************************************
  Instantiate templates
*****************************************************************************/

#ifdef __GNUC__
/* Used templates */
template class I_List<THD>;
template class I_List_iterator<THD>;
template class I_List<i_string>;
template class I_List<i_string_pair>;
template class I_List<NAMED_LIST>;
FIX_GCC_LINKING_PROBLEM
#endif
