/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "mysql_priv.h"
#include <m_ctype.h>
#include <my_dir.h>
#include <my_bit.h>
#include "slave.h"
#include "rpl_mi.h"
#include "sql_repl.h"
#include "rpl_filter.h"
#include "repl_failsafe.h"
#include <my_stacktrace.h>
#include "mysqld_suffix.h"
#include "mysys_err.h"
#include "events.h"
#include "debug_sync.h"
#include "log_event.h"

#include "../storage/myisam/ha_myisam.h"

#include "rpl_injector.h"

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
#if defined(NOT_ENOUGH_TESTED) \
  && defined(NDB_SHM_TRANSPORTER) && MYSQL_VERSION_ID >= 50000
#define OPT_NDB_SHM_DEFAULT 1
#else
#define OPT_NDB_SHM_DEFAULT 0
#endif
#endif

#ifndef DEFAULT_SKIP_THREAD_PRIORITY
#define DEFAULT_SKIP_THREAD_PRIORITY 0
#endif

#include <thr_alarm.h>
#include <ft_global.h>
#include <errmsg.h>
#include "sp_rcontext.h"
#include "sp_cache.h"

#define mysqld_charset &my_charset_latin1

#ifdef HAVE_purify
#define IF_PURIFY(A,B) (A)
#else
#define IF_PURIFY(A,B) (B)
#endif

#if SIZEOF_CHARP == 4
#define MAX_MEM_TABLE_SIZE ~(ulong) 0
#else
#define MAX_MEM_TABLE_SIZE ~(ulonglong) 0
#endif

/* stack traces are only supported on linux intel */
#if defined(__linux__)  && defined(__i386__)
#define	HAVE_STACK_TRACE_ON_SEGV
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
#include <my_net.h>

#if !defined(__WIN__)
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

#include <my_libwrap.h>

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifdef __WIN__ 
#include <crtdbg.h>
#endif

#ifdef __NETWARE__
#define zVOLSTATE_ACTIVE 6
#define zVOLSTATE_DEACTIVE 2
#define zVOLSTATE_MAINTENANCE 3

#undef __event_h__
#include <../include/event.h>
/*
  This #undef exists here because both libc of NetWare and MySQL have
  files named event.h which causes compilation errors.
*/

#include <nks/netware.h>
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

#if defined(__FreeBSD__) && defined(HAVE_IEEEFP_H) && !defined(HAVE_FEDISABLEEXCEPT)
#include <ieeefp.h>
#ifdef HAVE_FP_EXCEPT				// Fix type conflict
typedef fp_except fp_except_t;
#endif
#endif /* __FreeBSD__ && HAVE_IEEEFP_H && !HAVE_FEDISABLEEXCEPT */
#ifdef HAVE_SYS_FPU_H
/* for IRIX to use set_fpc_csr() */
#include <sys/fpu.h>
#endif
#ifdef HAVE_FPU_CONTROL_H
#include <fpu_control.h>
#endif
#if defined(__i386__) && !defined(HAVE_FPU_CONTROL_H)
# define fpu_control_t unsigned int
# define _FPU_EXTENDED 0x300
# define _FPU_DOUBLE 0x200
# if defined(__GNUC__) || (defined(__SUNPRO_CC) && __SUNPRO_CC >= 0x590)
#  define _FPU_GETCW(cw) asm volatile ("fnstcw %0" : "=m" (*&cw))
#  define _FPU_SETCW(cw) asm volatile ("fldcw %0" : : "m" (*&cw))
# else
#  define _FPU_GETCW(cw) (cw= 0)
#  define _FPU_SETCW(cw)
# endif
#endif

extern "C" my_bool reopen_fstreams(const char *filename,
                                   FILE *outstream, FILE *errstream);

inline void setup_fpu()
{
#if defined(__FreeBSD__) && defined(HAVE_IEEEFP_H) && !defined(HAVE_FEDISABLEEXCEPT)
  /* We can't handle floating point exceptions with threads, so disable
     this on freebsd
     Don't fall for overflow, underflow,divide-by-zero or loss of precision.
     fpsetmask() is deprecated in favor of fedisableexcept() in C99.
  */
#if defined(FP_X_DNML)
  fpsetmask(~(FP_X_INV | FP_X_DNML | FP_X_OFL | FP_X_UFL | FP_X_DZ |
	      FP_X_IMP));
#else
  fpsetmask(~(FP_X_INV |             FP_X_OFL | FP_X_UFL | FP_X_DZ |
              FP_X_IMP));
#endif /* FP_X_DNML */
#endif /* __FreeBSD__ && HAVE_IEEEFP_H && !HAVE_FEDISABLEEXCEPT */

#ifdef HAVE_FEDISABLEEXCEPT
  fedisableexcept(FE_ALL_EXCEPT);
#endif

#ifdef HAVE_FESETROUND
    /* Set FPU rounding mode to "round-to-nearest" */
  fesetround(FE_TONEAREST);
#endif /* HAVE_FESETROUND */

  /*
    x86 (32-bit) requires FPU precision to be explicitly set to 64 bit
    (double precision) for portable results of floating point operations.
    However, there is no need to do so if compiler is using SSE2 for floating
    point, double values will be stored and processed in 64 bits anyway.
  */
#if defined(__i386__) && !defined(__SSE2_MATH__)
#if defined(_WIN32)
#if !defined(_WIN64)
  _control87(_PC_53, MCW_PC);
#endif /* !_WIN64 */
#else /* !_WIN32 */
  fpu_control_t cw;
  _FPU_GETCW(cw);
  cw= (cw & ~_FPU_EXTENDED) | _FPU_DOUBLE;
  _FPU_SETCW(cw);
#endif /* _WIN32 && */
#endif /* __i386__ */

#if defined(__sgi) && defined(HAVE_SYS_FPU_H)
  /* Enable denormalized DOUBLE values support for IRIX */
  union fpc_csr n;
  n.fc_word = get_fpc_csr();
  n.fc_struct.flush = 0;
  set_fpc_csr(n.fc_word);
#endif
}

} /* cplusplus */

#define MYSQL_KILL_SIGNAL SIGTERM

#ifdef HAVE_GLIBC2_STYLE_GETHOSTBYNAME_R
#include <sys/types.h>
#else
#include <my_pthread.h>			// For thr_setconcurency()
#endif

#ifdef SOLARIS
extern "C" int gethostname(char *name, int namelen);
#endif

extern "C" sig_handler handle_fatal_signal(int sig);

#if defined(__linux__)
#define ENABLE_TEMP_POOL 1
#else
#define ENABLE_TEMP_POOL 0
#endif

/* Constants */

#include <welcome_copyright_notice.h> // ORACLE_WELCOME_COPYRIGHT_NOTICE

const char *show_comp_option_name[]= {"YES", "NO", "DISABLED"};
/*
  WARNING: When adding new SQL modes don't forget to update the
           tables definitions that stores it's value.
           (ie: mysql.event, mysql.proc)
*/
static const char *sql_mode_names[]=
{
  "REAL_AS_FLOAT", "PIPES_AS_CONCAT", "ANSI_QUOTES", "IGNORE_SPACE",
  "?", "ONLY_FULL_GROUP_BY", "NO_UNSIGNED_SUBTRACTION",
  "NO_DIR_IN_CREATE",
  "POSTGRESQL", "ORACLE", "MSSQL", "DB2", "MAXDB", "NO_KEY_OPTIONS",
  "NO_TABLE_OPTIONS", "NO_FIELD_OPTIONS", "MYSQL323", "MYSQL40", "ANSI",
  "NO_AUTO_VALUE_ON_ZERO", "NO_BACKSLASH_ESCAPES", "STRICT_TRANS_TABLES",
  "STRICT_ALL_TABLES",
  "NO_ZERO_IN_DATE", "NO_ZERO_DATE", "ALLOW_INVALID_DATES",
  "ERROR_FOR_DIVISION_BY_ZERO",
  "TRADITIONAL", "NO_AUTO_CREATE_USER", "HIGH_NOT_PRECEDENCE",
  "NO_ENGINE_SUBSTITUTION",
  "PAD_CHAR_TO_FULL_LENGTH",
  NullS
};

static const unsigned int sql_mode_names_len[]=
{
  /*REAL_AS_FLOAT*/               13,
  /*PIPES_AS_CONCAT*/             15,
  /*ANSI_QUOTES*/                 11,
  /*IGNORE_SPACE*/                12,
  /*?*/                           1,
  /*ONLY_FULL_GROUP_BY*/          18,
  /*NO_UNSIGNED_SUBTRACTION*/     23,
  /*NO_DIR_IN_CREATE*/            16,
  /*POSTGRESQL*/                  10,
  /*ORACLE*/                      6,
  /*MSSQL*/                       5,
  /*DB2*/                         3,
  /*MAXDB*/                       5,
  /*NO_KEY_OPTIONS*/              14,
  /*NO_TABLE_OPTIONS*/            16,
  /*NO_FIELD_OPTIONS*/            16,
  /*MYSQL323*/                    8,
  /*MYSQL40*/                     7,
  /*ANSI*/                        4,
  /*NO_AUTO_VALUE_ON_ZERO*/       21,
  /*NO_BACKSLASH_ESCAPES*/        20,
  /*STRICT_TRANS_TABLES*/         19,
  /*STRICT_ALL_TABLES*/           17,
  /*NO_ZERO_IN_DATE*/             15,
  /*NO_ZERO_DATE*/                12,
  /*ALLOW_INVALID_DATES*/         19,
  /*ERROR_FOR_DIVISION_BY_ZERO*/  26,
  /*TRADITIONAL*/                 11,
  /*NO_AUTO_CREATE_USER*/         19,
  /*HIGH_NOT_PRECEDENCE*/         19,
  /*NO_ENGINE_SUBSTITUTION*/      22,
  /*PAD_CHAR_TO_FULL_LENGTH*/     23
};

TYPELIB sql_mode_typelib= { array_elements(sql_mode_names)-1,"",
			    sql_mode_names,
                            (unsigned int *)sql_mode_names_len };

static const char *optimizer_switch_names[]=
{
  "index_merge","index_merge_union","index_merge_sort_union", 
  "index_merge_intersection", "default", NullS
};
/* Corresponding defines are named OPTIMIZER_SWITCH_XXX */
static const unsigned int optimizer_switch_names_len[]=
{
  sizeof("index_merge") - 1,
  sizeof("index_merge_union") - 1,
  sizeof("index_merge_sort_union") - 1,
  sizeof("index_merge_intersection") - 1,
  sizeof("default") - 1
};
TYPELIB optimizer_switch_typelib= { array_elements(optimizer_switch_names)-1,"",
                                    optimizer_switch_names,
                                    (unsigned int *)optimizer_switch_names_len };

static const char *tc_heuristic_recover_names[]=
{
  "COMMIT", "ROLLBACK", NullS
};
static TYPELIB tc_heuristic_recover_typelib=
{
  array_elements(tc_heuristic_recover_names)-1,"",
  tc_heuristic_recover_names, NULL
};

static const char *thread_handling_names[]=
{ "one-thread-per-connection", "no-threads",
#if HAVE_POOL_OF_THREADS == 1
  "pool-of-threads",
#endif
  NullS};

TYPELIB thread_handling_typelib=
{
  array_elements(thread_handling_names) - 1, "",
  thread_handling_names, NULL
};

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
static my_bool opt_help= 0, opt_verbose= 0;

arg_cmp_func Arg_comparator::comparator_matrix[5][2] =
{{&Arg_comparator::compare_string,     &Arg_comparator::compare_e_string},
 {&Arg_comparator::compare_real,       &Arg_comparator::compare_e_real},
 {&Arg_comparator::compare_int_signed, &Arg_comparator::compare_e_int},
 {&Arg_comparator::compare_row,        &Arg_comparator::compare_e_row},
 {&Arg_comparator::compare_decimal,    &Arg_comparator::compare_e_decimal}};

const char *log_output_names[] = { "NONE", "FILE", "TABLE", NullS};
static const unsigned int log_output_names_len[]= { 4, 4, 5, 0 };
TYPELIB log_output_typelib= {array_elements(log_output_names)-1,"",
                             log_output_names, 
                             (unsigned int *) log_output_names_len};

/* static variables */

#ifdef HAVE_NPTL
volatile sig_atomic_t ld_assume_kernel_is_set= 0;
#endif

/* the default log output is log tables */
static bool lower_case_table_names_used= 0;
static bool max_long_data_size_used= false;
static bool volatile select_thread_in_use, signal_thread_in_use;
static bool volatile ready_to_exit;
static my_bool opt_debugging= 0, opt_external_locking= 0, opt_console= 0;
static my_bool opt_short_log_format= 0;
static uint kill_cached_threads, wake_thread;
static ulong killed_threads, thread_created;
       ulong max_used_connections;
static ulong my_bind_addr;			/**< the address we bind to */
static volatile ulong cached_thread_count= 0;
static const char *sql_mode_str= "OFF";
/* Text representation for OPTIMIZER_SWITCH_DEFAULT */
static const char *optimizer_switch_str="index_merge=on,index_merge_union=on,"
                                        "index_merge_sort_union=on,"
                                        "index_merge_intersection=on";
static char *mysqld_user, *mysqld_chroot, *log_error_file_ptr;
static char *opt_init_slave, *language_ptr, *opt_init_connect;
static char *default_character_set_name;
static char *character_set_filesystem_name;
static char *lc_time_names_name;
static char *my_bind_addr_str;
static char *default_collation_name; 
static char *default_storage_engine_str;
static char compiled_default_collation_name[]= MYSQL_DEFAULT_COLLATION_NAME;
static I_List<THD> thread_cache;
static double long_query_time;

static pthread_cond_t COND_thread_cache, COND_flush_thread_cache;

/* Global variables */

bool opt_update_log, opt_bin_log, opt_ignore_builtin_innodb= 0;
my_bool opt_log, opt_slow_log;
ulong log_output_options;
my_bool opt_log_queries_not_using_indexes= 0;
bool opt_error_log= IF_WIN(1,0);
bool opt_disable_networking=0, opt_skip_show_db=0;
bool opt_skip_name_resolve=0;
my_bool opt_character_set_client_handshake= 1;
bool server_id_supplied = 0;
bool opt_endinfo, using_udf_functions;
my_bool locked_in_memory;
bool opt_using_transactions;
bool volatile abort_loop;
bool volatile shutdown_in_progress;
/*
  True if the bootstrap thread is running. Protected by LOCK_thread_count,
  just like thread_count.
  Used in bootstrap() function to determine if the bootstrap thread
  has completed. Note, that we can't use 'thread_count' instead,
  since in 5.1, in presence of the Event Scheduler, there may be
  event threads running in parallel, so it's impossible to know
  what value of 'thread_count' is a sign of completion of the
  bootstrap thread.

  At the same time, we can't start the event scheduler after
  bootstrap either, since we want to be able to process event-related
  SQL commands in the init file and in --bootstrap mode.
*/
bool in_bootstrap= FALSE;
/**
   @brief 'grant_option' is used to indicate if privileges needs
   to be checked, in which case the lock, LOCK_grant, is used
   to protect access to the grant table.
   @note This flag is dropped in 5.1 
   @see grant_init()
 */
bool volatile grant_option;

my_bool opt_skip_slave_start = 0; ///< If set, slave is not autostarted
my_bool opt_reckless_slave = 0;
my_bool opt_enable_named_pipe= 0;
my_bool opt_local_infile, opt_slave_compressed_protocol;
my_bool opt_safe_user_create = 0, opt_no_mix_types = 0;
my_bool opt_show_slave_auth_info, opt_sql_bin_update = 0;
my_bool opt_log_slave_updates= 0;
bool slave_warning_issued = false; 

/*
  Legacy global handlerton. These will be removed (please do not add more).
*/
handlerton *heap_hton;
handlerton *myisam_hton;
handlerton *partition_hton;

#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
const char *opt_ndbcluster_connectstring= 0;
const char *opt_ndb_connectstring= 0;
char opt_ndb_constrbuf[1024]= {0};
unsigned opt_ndb_constrbuf_len= 0;
my_bool	opt_ndb_shm, opt_ndb_optimized_node_selection;
ulong opt_ndb_cache_check_time;
const char *opt_ndb_mgmd;
ulong opt_ndb_nodeid;
ulong ndb_extra_logging;
#ifdef HAVE_NDB_BINLOG
ulong ndb_report_thresh_binlog_epoch_slip;
ulong ndb_report_thresh_binlog_mem_usage;
#endif

extern const char *ndb_distribution_names[];
extern TYPELIB ndb_distribution_typelib;
extern const char *opt_ndb_distribution;
extern enum ndb_distribution opt_ndb_distribution_id;
#endif
my_bool opt_readonly, use_temp_pool, relay_log_purge;
my_bool opt_sync_frm, opt_allow_suspicious_udfs;
my_bool opt_secure_auth= 0;
char* opt_secure_file_priv= 0;
my_bool opt_log_slow_admin_statements= 0;
my_bool opt_log_slow_slave_statements= 0;
my_bool lower_case_file_system= 0;
my_bool opt_large_pages= 0;
my_bool opt_myisam_use_mmap= 0;
uint    opt_large_page_size= 0;
#if defined(ENABLED_DEBUG_SYNC)
uint    opt_debug_sync_timeout= 0;
#endif /* defined(ENABLED_DEBUG_SYNC) */
my_bool opt_old_style_user_limits= 0, trust_function_creators= 0;
/*
  True if there is at least one per-hour limit for some user, so we should
  check them before each query (and possibly reset counters when hour is
  changed). False otherwise.
*/
volatile bool mqh_used = 0;
my_bool opt_noacl;
my_bool sp_automatic_privileges= 1;

ulong opt_binlog_rows_event_max_size;
const char *binlog_format_names[]= {"MIXED", "STATEMENT", "ROW", NullS};
TYPELIB binlog_format_typelib=
  { array_elements(binlog_format_names) - 1, "",
    binlog_format_names, NULL };
ulong opt_binlog_format_id= (ulong) BINLOG_FORMAT_UNSPEC;
const char *opt_binlog_format= binlog_format_names[opt_binlog_format_id];
#ifdef HAVE_INITGROUPS
volatile sig_atomic_t calling_initgroups= 0; /**< Used in SIGSEGV handler. */
#endif
uint mysqld_port, test_flags, select_errors, dropping_tables, ha_open_options;
uint mysqld_port_timeout;
uint delay_key_write_options, protocol_version;
uint lower_case_table_names;
uint tc_heuristic_recover= 0;
uint volatile thread_count, thread_running;
ulonglong thd_startup_options;
ulong back_log, connect_timeout, concurrency, server_id;
ulong table_cache_size, table_def_size;
ulong what_to_log;
ulong query_buff_size, slow_launch_time, slave_open_temp_tables;
ulong open_files_limit, max_binlog_size, max_relay_log_size;
ulong slave_net_timeout, slave_trans_retries;
ulong slave_exec_mode_options;
static const char *slave_exec_mode_str= "STRICT";
ulong thread_cache_size=0, thread_pool_size= 0;
ulong binlog_cache_size=0;
ulonglong  max_binlog_cache_size=0;
ulong slave_max_allowed_packet= 0;
ulong query_cache_size=0;
ulong refresh_version;  /* Increments on each reload */
query_id_t global_query_id;
ulong aborted_threads, aborted_connects;
ulong delayed_insert_timeout, delayed_insert_limit, delayed_queue_size;
ulong delayed_insert_threads, delayed_insert_writes, delayed_rows_in_use;
ulong delayed_insert_errors,flush_time;
ulong specialflag=0;
ulong binlog_cache_use= 0, binlog_cache_disk_use= 0;
ulong max_connections, max_connect_errors;
/*
  Maximum length of parameter value which can be set through
  mysql_send_long_data() call.
*/
ulong max_long_data_size;
uint  max_user_connections= 0;
/**
  Limit of the total number of prepared statements in the server.
  Is necessary to protect the server against out-of-memory attacks.
*/
ulong max_prepared_stmt_count;
/**
  Current total number of prepared statements in the server. This number
  is exact, and therefore may not be equal to the difference between
  `com_stmt_prepare' and `com_stmt_close' (global status variables), as
  the latter ones account for all registered attempts to prepare
  a statement (including unsuccessful ones).  Prepared statements are
  currently connection-local: if the same SQL query text is prepared in
  two different connections, this counts as two distinct prepared
  statements.
*/
ulong prepared_stmt_count=0;
ulong thread_id=1L,current_pid;
ulong slow_launch_threads = 0, sync_binlog_period;
ulong expire_logs_days = 0;
ulong rpl_recovery_rank=0;
const char *log_output_str= "FILE";

time_t server_start_time, flush_status_time;

char mysql_home[FN_REFLEN], pidfile_name[FN_REFLEN], system_time_zone[30];
char *default_tz_name;
char log_error_file[FN_REFLEN], glob_hostname[FN_REFLEN];
char mysql_real_data_home[FN_REFLEN],
     language[FN_REFLEN], reg_ext[FN_EXTLEN], mysql_charsets_dir[FN_REFLEN],
     *opt_init_file, *opt_tc_log_file,
     def_ft_boolean_syntax[sizeof(ft_boolean_syntax)];
char mysql_unpacked_real_data_home[FN_REFLEN];
int mysql_unpacked_real_data_home_len;
uint reg_ext_length;
const key_map key_map_empty(0);
key_map key_map_full(0);                        // Will be initialized later

const char *opt_date_time_formats[3];

uint mysql_data_home_len;
char mysql_data_home_buff[2], *mysql_data_home=mysql_real_data_home;
char server_version[SERVER_VERSION_LENGTH];
char *mysqld_unix_port, *opt_mysql_tmpdir;
const char **errmesg;			/**< Error messages */
const char *myisam_recover_options_str="OFF";
const char *myisam_stats_method_str="nulls_unequal";

/** name of reference on left espression in rewritten IN subquery */
const char *in_left_expr_name= "<left expr>";
/** name of additional condition */
const char *in_additional_cond= "<IN COND>";
const char *in_having_cond= "<IN HAVING>";

my_decimal decimal_zero;
/* classes for comparation parsing/processing */
Eq_creator eq_creator;
Ne_creator ne_creator;
Gt_creator gt_creator;
Lt_creator lt_creator;
Ge_creator ge_creator;
Le_creator le_creator;

FILE *bootstrap_file;
int bootstrap_error;
FILE *stderror_file=0;

I_List<THD> threads;
I_List<NAMED_LIST> key_caches;
Rpl_filter* rpl_filter;
Rpl_filter* binlog_filter;

struct system_variables global_system_variables;
struct system_variables max_system_variables;
struct system_status_var global_status_var;

MY_TMPDIR mysql_tmpdir_list;
MY_BITMAP temp_pool;

CHARSET_INFO *system_charset_info, *files_charset_info ;
CHARSET_INFO *national_charset_info, *table_alias_charset;
CHARSET_INFO *character_set_filesystem;

MY_LOCALE *my_default_lc_time_names;

SHOW_COMP_OPTION have_ssl, have_symlink, have_dlopen, have_query_cache;
SHOW_COMP_OPTION have_geometry, have_rtree_keys;
SHOW_COMP_OPTION have_crypt, have_compress;
SHOW_COMP_OPTION have_community_features;

/* Thread specific variables */

pthread_key(MEM_ROOT**,THR_MALLOC);
pthread_key(THD*, THR_THD);
pthread_mutex_t LOCK_mysql_create_db, LOCK_Acl, LOCK_open, LOCK_thread_count,
		LOCK_mapped_file, LOCK_status, LOCK_global_read_lock,
		LOCK_error_log, LOCK_uuid_generator,
		LOCK_delayed_insert, LOCK_delayed_status, LOCK_delayed_create,
		LOCK_crypt, LOCK_bytes_sent, LOCK_bytes_received,
	        LOCK_global_system_variables,
		LOCK_user_conn, LOCK_slave_list, LOCK_active_mi,
                LOCK_connection_count;
/**
  The below lock protects access to two global server variables:
  max_prepared_stmt_count and prepared_stmt_count. These variables
  set the limit and hold the current total number of prepared statements
  in the server, respectively. As PREPARE/DEALLOCATE rate in a loaded
  server may be fairly high, we need a dedicated lock.
*/
pthread_mutex_t LOCK_prepared_stmt_count;
#ifdef HAVE_OPENSSL
pthread_mutex_t LOCK_des_key_file;
#endif
rw_lock_t	LOCK_grant, LOCK_sys_init_connect, LOCK_sys_init_slave;
rw_lock_t	LOCK_system_variables_hash;
pthread_cond_t COND_refresh, COND_thread_count, COND_global_read_lock;
pthread_t signal_thread;
pthread_attr_t connection_attrib;
pthread_mutex_t  LOCK_server_started;
pthread_cond_t  COND_server_started;

int mysqld_server_started= 0;

File_parser_dummy_hook file_parser_dummy_hook;

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
char *opt_logname, *opt_slow_logname;

/* Static variables */

static volatile sig_atomic_t kill_in_progress;
#ifdef HAVE_STACK_TRACE_ON_SEGV
static my_bool opt_do_pstack;
#endif /* HAVE_STACK_TRACE_ON_SEGV */
static my_bool opt_bootstrap, opt_myisam_log;
static int cleanup_done;
static ulong opt_specialflag, opt_myisam_block_size;
static char *opt_update_logname, *opt_binlog_index_name;
static char *opt_tc_heuristic_recover;
static char *mysql_home_ptr, *pidfile_name_ptr;
static int defaults_argc;
static char **defaults_argv;
static char *opt_bin_logname;

int orig_argc;
char **orig_argv;

static my_socket unix_sock,ip_sock;
struct rand_struct sql_rand; ///< used by sql_class.cc:THD::THD()

#ifndef EMBEDDED_LIBRARY
struct passwd *user_info;
static pthread_t select_thread;
static uint thr_kill_signal;
#endif

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
static	 NTService  Service;	      ///< Service object for WinNT
#endif /* EMBEDDED_LIBRARY */
#endif /* __WIN__ */

#ifdef __NT__
static char pipe_name[512];
static SECURITY_ATTRIBUTES saPipeSecurity;
static SECURITY_DESCRIPTOR sdPipeDescriptor;
static HANDLE hPipe = INVALID_HANDLE_VALUE;
#endif

#ifndef EMBEDDED_LIBRARY
bool mysqld_embedded=0;
#else
bool mysqld_embedded=1;
#endif

static my_bool plugins_are_initialized= FALSE;

#ifndef DBUG_OFF
static const char* default_dbug_option;
#endif
#ifdef HAVE_LIBWRAP
const char *libwrapName= NULL;
int allow_severity = LOG_INFO;
int deny_severity = LOG_WARNING;
#endif
#ifdef HAVE_QUERY_CACHE
static ulong query_cache_limit= 0;
ulong query_cache_min_res_unit= QUERY_CACHE_MIN_RESULT_DATA_SIZE;
Query_cache query_cache;
#endif
#ifdef HAVE_SMEM
char *shared_memory_base_name= default_shared_memory_base_name;
my_bool opt_enable_shared_memory;
HANDLE smem_event_connect_request= 0;
#endif

scheduler_functions thread_scheduler;

#define SSL_VARS_NOT_STATIC
#include "sslopt-vars.h"
#ifdef HAVE_OPENSSL
#include <openssl/crypto.h>
#ifndef HAVE_YASSL
typedef struct CRYPTO_dynlock_value
{
  rw_lock_t lock;
} openssl_lock_t;

static openssl_lock_t *openssl_stdlocks;
static openssl_lock_t *openssl_dynlock_create(const char *, int);
static void openssl_dynlock_destroy(openssl_lock_t *, const char *, int);
static void openssl_lock_function(int, int, const char *, int);
static void openssl_lock(int, openssl_lock_t *, const char *, int);
static unsigned long openssl_id_function();
#endif
char *des_key_file;
struct st_VioSSLFd *ssl_acceptor_fd;
#endif /* HAVE_OPENSSL */

/**
  Number of currently active user connections. The variable is protected by
  LOCK_connection_count.
*/
uint connection_count= 0;

/* Function declarations */

pthread_handler_t signal_hand(void *arg);
static int mysql_init_variables(void);
static int get_options(int *argc,char **argv);
extern "C" my_bool mysqld_get_one_option(int, const struct my_option *, char *);
static void set_server_version(void);
static int init_thread_environment();
static char *get_relative_path(const char *path);
static int fix_paths(void);
pthread_handler_t handle_connections_sockets(void *arg);
pthread_handler_t kill_server_thread(void *arg);
static void bootstrap(FILE *file);
static bool read_init_file(char *file_name);
#ifdef __NT__
pthread_handler_t handle_connections_namedpipes(void *arg);
#endif
#ifdef HAVE_SMEM
pthread_handler_t handle_connections_shared_memory(void *arg);
#endif
pthread_handler_t handle_slave(void *arg);
static ulong find_bit_type(const char *x, TYPELIB *bit_lib);
static ulong find_bit_type_or_exit(const char *x, TYPELIB *bit_lib,
                                   const char *option, int *error);
static void clean_up(bool print_message);
static int test_if_case_insensitive(const char *dir_name);

#ifndef EMBEDDED_LIBRARY
static void usage(void);
static void start_signal_handler(void);
static void close_server_sock();
static void clean_up_mutexes(void);
static void wait_for_signal_thread_to_end(void);
static void create_pid_file();
static void end_ssl();
#endif


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

  /* kill connection thread */
#if !defined(__WIN__) && !defined(__NETWARE__)
  DBUG_PRINT("quit", ("waiting for select thread: 0x%lx",
                      (ulong) select_thread));
  (void) pthread_mutex_lock(&LOCK_thread_count);

  while (select_thread_in_use)
  {
    struct timespec abstime;
    int error;
    LINT_INIT(error);
    DBUG_PRINT("info",("Waiting for select thread"));

#ifndef DONT_USE_THR_ALARM
    if (pthread_kill(select_thread, thr_client_alarm))
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
  if (!opt_disable_networking )
  {
    if (ip_sock != INVALID_SOCKET)
    {
      (void) mysql_socket_shutdown(ip_sock, SHUT_RDWR);
      (void) closesocket(ip_sock);
      ip_sock= INVALID_SOCKET;
    }
  }
#ifdef __NT__
  if (hPipe != INVALID_HANDLE_VALUE && opt_enable_named_pipe)
  {
    HANDLE temp;
    DBUG_PRINT("quit", ("Closing named pipes") );

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
    (void) mysql_socket_shutdown(unix_sock, SHUT_RDWR);
    (void) closesocket(unix_sock);
    (void) unlink(mysqld_unix_port);
    unix_sock= INVALID_SOCKET;
  }
#endif
  end_thr_alarm(0);			 // Abort old alarms.

  /*
    First signal all threads that it's time to die
    This will give the threads some time to gracefully abort their
    statements and inform their clients that the server is about to die.
  */

  THD *tmp;
  (void) pthread_mutex_lock(&LOCK_thread_count); // For unlink from list

  I_List_iterator<THD> it(threads);
  while ((tmp=it++))
  {
    DBUG_PRINT("quit",("Informing thread %ld that it's time to die",
		       tmp->thread_id));
    /* We skip slave threads & scheduler on this first loop through. */
    if (tmp->slave_thread)
      continue;

    tmp->killed= THD::KILL_CONNECTION;
    thread_scheduler.post_kill_notification(tmp);
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

  Events::deinit();
  end_slave();

  if (thread_count)
    sleep(2);					// Give threads time to die

  /*
    Force remaining threads to die by closing the connection to the client
    This will ensure that threads that are waiting for a command from the
    client on a blocking read call are aborted.
  */

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
      if (global_system_variables.log_warnings)
        sql_print_warning(ER(ER_FORCING_CLOSE),my_progname,
                          tmp->thread_id,
                          (tmp->main_security_ctx.user ?
                           tmp->main_security_ctx.user : ""));
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

  close_active_mi();
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
    DBUG_PRINT("info",("calling shutdown on TCP/IP socket"));
    VOID(mysql_socket_shutdown(tmp_sock, SHUT_RDWR));
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
    VOID(mysql_socket_shutdown(tmp_sock, SHUT_RDWR));
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

#endif /*EMBEDDED_LIBRARY*/


void kill_mysql(void)
{
  DBUG_ENTER("kill_mysql");

#if defined(SIGNALS_DONT_BREAK_READ) && !defined(EMBEDDED_LIBRARY)
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
#endif
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

/**
  Force server down. Kill all connections and threads and exit.

  @param  sig_ptr       Signal number that caused kill_server to be called.

  @note
    A signal number of 0 mean that the function was not called
    from a signal handler and there is thus no signal to block
    or stop, we just want to kill the server.
*/

#if defined(__NETWARE__)
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
  DBUG_ENTER("kill_server");
#ifndef EMBEDDED_LIBRARY
  int sig=(int) (long) sig_ptr;			// This is passed a int
  // if there is a signal during the kill in progress, ignore the other
  if (kill_in_progress)				// Safety
  {
    DBUG_LEAVE;
    RETURN_FROM_KILL_SERVER;
  }
  kill_in_progress=TRUE;
  abort_loop=1;					// This should be set
  if (sig != 0) // 0 is not a valid signal number
    my_sigset(sig, SIG_IGN);                    /* purify inspected */
  if (sig == MYSQL_KILL_SIGNAL || sig == 0)
    sql_print_information(ER(ER_NORMAL_SHUTDOWN),my_progname);
  else
    sql_print_error(ER(ER_GOT_SIGNAL),my_progname,sig); /* purecov: inspected */

#if defined(HAVE_SMEM) && defined(__WIN__)    
  /*    
   Send event to smem_event_connect_request for aborting    
   */    
  if (!SetEvent(smem_event_connect_request))    
  {      
	  DBUG_PRINT("error",
		("Got error: %ld from SetEvent of smem_event_connect_request",
		 GetLastError()));    
  }
#endif  
  
  close_connections();
  if (sig != MYSQL_KILL_SIGNAL &&
      sig != 0)
    unireg_abort(1);				/* purecov: inspected */
  else
    unireg_end();

  /* purecov: begin deadcode */
#ifdef __NETWARE__
  if (!event_flag)
    pthread_join(select_thread, NULL);		// wait for main thread
#endif /* __NETWARE__ */

  DBUG_LEAVE;                                   // Must match DBUG_ENTER()
  my_thread_end();
  pthread_exit(0);
  /* purecov: end */

  RETURN_FROM_KILL_SERVER;                      // Avoid compiler warnings

#else /* EMBEDDED_LIBRARY*/

  DBUG_LEAVE;
  RETURN_FROM_KILL_SERVER;

#endif /* EMBEDDED_LIBRARY */
}


#if defined(USE_ONE_SIGNAL_HAND) || (defined(__NETWARE__) && defined(SIGNALS_DONT_BREAK_READ))
pthread_handler_t kill_server_thread(void *arg __attribute__((unused)))
{
  my_thread_init();				// Initialize new thread
  kill_server(0);
  /* purecov: begin deadcode */
  my_thread_end();
  pthread_exit(0);
  return 0;
  /* purecov: end */
}
#endif


extern "C" sig_handler print_signal_warning(int sig)
{
  if (global_system_variables.log_warnings)
    sql_print_warning("Got signal %d from thread %ld", sig,my_thread_id());
#ifdef SIGNAL_HANDLER_RESET_ON_DELIVERY
  my_sigset(sig,print_signal_warning);		/* int. thread system calls */
#endif
#if !defined(__WIN__) && !defined(__NETWARE__)
  if (sig == SIGALRM)
    alarm(2);					/* reschedule alarm */
#endif
}

#ifndef EMBEDDED_LIBRARY

/**
  cleanup all memory and end program nicely.

    If SIGNALS_DONT_BREAK_READ is defined, this function is called
    by the main thread. To get MySQL to shut down nicely in this case
    (Mac OS X) we have to call exit() instead if pthread_exit().

  @note
    This function never returns.
*/
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

  if (opt_help)
    usage();
  if (exit_code)
    sql_print_error("Aborting\n");
  clean_up(!opt_help && (exit_code || !opt_bootstrap)); /* purecov: inspected */
  DBUG_PRINT("quit",("done with cleanup in unireg_abort"));
  wait_for_signal_thread_to_end();
  clean_up_mutexes();
  my_end(opt_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
  exit(exit_code); /* purecov: inspected */
}

#endif /*EMBEDDED_LIBRARY*/


void clean_up(bool print_message)
{
  DBUG_PRINT("exit",("clean_up"));
  if (cleanup_done++)
    return; /* purecov: inspected */

  stop_handle_manager();
  release_ddl_log();

  /*
    make sure that handlers finish up
    what they have that is dependent on the binlog
  */
  ha_binlog_end(current_thd);

  logger.cleanup_base();

  injector::free_instance();
  mysql_bin_log.cleanup();

#ifdef HAVE_REPLICATION
  if (use_slave_mask)
    bitmap_free(&slave_error_mask);
#endif
  my_tz_free();
  my_database_names_free();
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  servers_free(1);
  acl_free(1);
  grant_free();
#endif
  query_cache_destroy();
  table_cache_free();
  table_def_free();
  hostname_cache_free();
  item_user_lock_free();
  lex_free();				/* Free some memory */
  item_create_cleanup();
  set_var_free();
  if (!opt_noacl)
  {
#ifdef HAVE_DLOPEN
    udf_free();
#endif
  }
  plugin_shutdown();
  ha_end();
  if (tc_log)
    tc_log->close();
  xid_cache_free();
  delete_elements(&key_caches, (void (*)(const char*, uchar*)) free_key_cache);
  multi_keycache_free();
  free_status_vars();
  end_thr_alarm(1);			/* Free allocated memory */
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
  my_free(sys_var_general_log_path.value, MYF(MY_ALLOW_ZERO_PTR));
  my_free(sys_var_slow_log_path.value, MYF(MY_ALLOW_ZERO_PTR));
  free_tmpdir(&mysql_tmpdir_list);
#ifdef HAVE_REPLICATION
  my_free(slave_load_tmpdir,MYF(MY_ALLOW_ZERO_PTR));
#endif
  x_free(opt_bin_logname);
  x_free(opt_relay_logname);
  x_free(opt_secure_file_priv);
  bitmap_free(&temp_pool);
  free_max_user_conn();
#ifdef HAVE_REPLICATION
  end_slave_list();
#endif
  delete binlog_filter;
  delete rpl_filter;
#ifndef EMBEDDED_LIBRARY
  end_ssl();
#endif
  vio_end();
#ifdef USE_REGEX
  my_regex_end();
#endif
  free_charsets();
#if defined(ENABLED_DEBUG_SYNC)
  /* End the debug sync facility. See debug_sync.cc. */
  debug_sync_end();
#endif /* defined(ENABLED_DEBUG_SYNC) */

#if !defined(EMBEDDED_LIBRARY)
  if (!opt_bootstrap)
    (void) my_delete(pidfile_name,MYF(0));	// This may not always exist
#endif
  if (print_message && errmesg && server_start_time)
    sql_print_information(ER(ER_SHUTDOWN_COMPLETE),my_progname);
  thread_scheduler.end();
  finish_client_errs();
  my_free((uchar*) my_error_unregister(ER_ERROR_FIRST, ER_ERROR_LAST),
          MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR));
  DBUG_PRINT("quit", ("Error messages freed"));
  /* Tell main we are ready */
  logger.cleanup_end();
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


#ifndef EMBEDDED_LIBRARY

/**
  This is mainly needed when running with purify, but it's still nice to
  know that all child threads have died when mysqld exits.
*/
static void wait_for_signal_thread_to_end()
{
#ifndef __NETWARE__
  uint i;
  /*
    Wait up to 10 seconds for signal thread to die. We use this mainly to
    avoid getting warnings that my_thread_end has not been called
  */
  for (i= 0 ; i < 100 && signal_thread_in_use; i++)
  {
    if (pthread_kill(signal_thread, MYSQL_KILL_SIGNAL) != ESRCH)
      break;
    my_sleep(100);				// Give it time to die
  }
#endif
}


static void clean_up_mutexes()
{
  (void) pthread_mutex_destroy(&LOCK_mysql_create_db);
  (void) pthread_mutex_destroy(&LOCK_lock_db);
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
  (void) pthread_mutex_destroy(&LOCK_connection_count);
  Events::destroy_mutexes();
#ifdef HAVE_OPENSSL
  (void) pthread_mutex_destroy(&LOCK_des_key_file);
#ifndef HAVE_YASSL
  for (int i= 0; i < CRYPTO_num_locks(); ++i)
    (void) rwlock_destroy(&openssl_stdlocks[i].lock);
  OPENSSL_free(openssl_stdlocks);
#endif
#endif
#ifdef HAVE_REPLICATION
  (void) pthread_mutex_destroy(&LOCK_rpl_status);
  (void) pthread_cond_destroy(&COND_rpl_status);
#endif
  (void) pthread_mutex_destroy(&LOCK_active_mi);
  (void) rwlock_destroy(&LOCK_sys_init_connect);
  (void) rwlock_destroy(&LOCK_sys_init_slave);
  (void) pthread_mutex_destroy(&LOCK_global_system_variables);
  (void) rwlock_destroy(&LOCK_system_variables_hash);
  (void) pthread_mutex_destroy(&LOCK_global_read_lock);
  (void) pthread_mutex_destroy(&LOCK_uuid_generator);
  (void) pthread_mutex_destroy(&LOCK_prepared_stmt_count);
  (void) pthread_cond_destroy(&COND_thread_count);
  (void) pthread_cond_destroy(&COND_refresh);
  (void) pthread_cond_destroy(&COND_global_read_lock);
  (void) pthread_cond_destroy(&COND_thread_cache);
  (void) pthread_cond_destroy(&COND_flush_thread_cache);
  (void) pthread_cond_destroy(&COND_manager);
}

#endif /*EMBEDDED_LIBRARY*/


/****************************************************************************
** Init IP and UNIX socket
****************************************************************************/

#ifndef EMBEDDED_LIBRARY
static void set_ports()
{
  char	*env;
  if (!mysqld_port && !opt_disable_networking)
  {					// Get port if not from commandline
    mysqld_port= MYSQL_PORT;

    /*
      if builder specifically requested a default port, use that
      (even if it coincides with our factory default).
      only if they didn't do we check /etc/services (and, failing
      on that, fall back to the factory default of 3306).
      either default can be overridden by the environment variable
      MYSQL_TCP_PORT, which in turn can be overridden with command
      line options.
    */

#if MYSQL_PORT_DEFAULT == 0
    struct  servent *serv_ptr;
    if ((serv_ptr= getservbyname("mysql", "tcp")))
      mysqld_port= ntohs((u_short) serv_ptr->s_port); /* purecov: inspected */
#endif
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

/* Change to run as another user if started with --user */

static struct passwd *check_user(const char *user)
{
#if !defined(__WIN__) && !defined(__NETWARE__)
  struct passwd *tmp_user_info;
  uid_t user_id= geteuid();

  // Don't bother if we aren't superuser
  if (user_id)
  {
    if (user)
    {
      /* Don't give a warning, if real user is same as given with --user */
      /* purecov: begin tested */
      tmp_user_info= getpwnam(user);
      if ((!tmp_user_info || user_id != tmp_user_info->pw_uid) &&
	  global_system_variables.log_warnings)
        sql_print_warning(
                    "One can only use the --user switch if running as root\n");
      /* purecov: end */
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
  /* purecov: begin tested */
  if (!strcmp(user,"root"))
    return NULL;                        // Avoid problem with dynamic libraries

  if (!(tmp_user_info= getpwnam(user)))
  {
    // Allow a numeric uid to be used
    const char *pos;
    for (pos= user; my_isdigit(mysqld_charset,*pos); pos++) ;
    if (*pos)                                   // Not numeric id
      goto err;
    if (!(tmp_user_info= getpwuid(atoi(user))))
      goto err;
  }
  return tmp_user_info;
  /* purecov: end */

err:
  sql_print_error("Fatal error: Can't change to run as user '%s' ;  Please check that the user exists!\n",user);
  unireg_abort(1);

#ifdef PR_SET_DUMPABLE
  if (test_flags & TEST_CORE_ON_SIGNAL)
  {
    /* inform kernel that process is dumpable */
    (void) prctl(PR_SET_DUMPABLE, 1);
  }
#endif

#endif
  return NULL;
}

static void set_user(const char *user, struct passwd *user_info_arg)
{
  /* purecov: begin tested */
#if !defined(__WIN__) && !defined(__NETWARE__)
  DBUG_ASSERT(user_info_arg != 0);
#ifdef HAVE_INITGROUPS
  /*
    We can get a SIGSEGV when calling initgroups() on some systems when NSS
    is configured to use LDAP and the server is statically linked.  We set
    calling_initgroups as a flag to the SIGSEGV handler that is then used to
    output a specific message to help the user resolve this problem.
  */
  calling_initgroups= 1;
  initgroups((char*) user, user_info_arg->pw_gid);
  calling_initgroups= 0;
#endif
  if (setgid(user_info_arg->pw_gid) == -1)
  {
    sql_perror("setgid");
    unireg_abort(1);
  }
  if (setuid(user_info_arg->pw_uid) == -1)
  {
    sql_perror("setuid");
    unireg_abort(1);
  }
#endif
  /* purecov: end */
}


static void set_effective_user(struct passwd *user_info_arg)
{
#if !defined(__WIN__) && !defined(__NETWARE__)
  DBUG_ASSERT(user_info_arg != 0);
  if (setregid((gid_t)-1, user_info_arg->pw_gid) == -1)
  {
    sql_perror("setregid");
    unireg_abort(1);
  }
  if (setreuid((uid_t)-1, user_info_arg->pw_uid) == -1)
  {
    sql_perror("setreuid");
    unireg_abort(1);
  }
#endif
}


/** Change root user if started with @c --chroot . */
static void set_root(const char *path)
{
#if !defined(__WIN__) && !defined(__NETWARE__)
  if (chroot(path) == -1)
  {
    sql_perror("chroot");
    unireg_abort(1);
  }
  my_setwd("/", MYF(0));
#endif
}

static void network_init(void)
{
  struct sockaddr_in	IPaddr;
#ifdef HAVE_SYS_UN_H
  struct sockaddr_un	UNIXaddr;
#endif
  int	arg=1;
  int   ret;
  uint  waited;
  uint  this_wait;
  uint  retry;
  DBUG_ENTER("network_init");
  LINT_INIT(ret);

  if (thread_scheduler.init())
    unireg_abort(1);			/* purecov: inspected */

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
    /*
      Sometimes the port is not released fast enough when stopping and
      restarting the server. This happens quite often with the test suite
      on busy Linux systems. Retry to bind the address at these intervals:
      Sleep intervals: 1, 2, 4,  6,  9, 13, 17, 22, ...
      Retry at second: 1, 3, 7, 13, 22, 35, 52, 74, ...
      Limit the sequence by mysqld_port_timeout (set --port-open-timeout=#).
    */
    for (waited= 0, retry= 1; ; retry++, waited+= this_wait)
    {
      if (((ret= bind(ip_sock, my_reinterpret_cast(struct sockaddr *) (&IPaddr),
                      sizeof(IPaddr))) >= 0) ||
          (socket_errno != SOCKET_EADDRINUSE) ||
          (waited >= mysqld_port_timeout))
        break;
      sql_print_information("Retrying bind on TCP/IP port %u", mysqld_port);
      this_wait= retry * retry / 3 + 1;
      sleep(this_wait);
    }
    if (ret < 0)
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

#ifdef __NT__
  /* create named pipe */
  if (Service.IsNT() && mysqld_unix_port[0] && !opt_bootstrap &&
      opt_enable_named_pipe)
  {
    
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
    saPipeSecurity.nLength = sizeof(SECURITY_ATTRIBUTES);
    saPipeSecurity.lpSecurityDescriptor = &sdPipeDescriptor;
    saPipeSecurity.bInheritHandle = FALSE;
    if ((hPipe= CreateNamedPipe(pipe_name,
				PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,
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
	sql_perror((char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
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
      sql_print_error("The socket file path is too long (> %u): %s",
                      (uint) sizeof(UNIXaddr.sun_path) - 1, mysqld_unix_port);
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


#ifndef EMBEDDED_LIBRARY
/**
  Close a connection.

  @param thd		Thread handle
  @param errcode	Error code to print to console
  @param lock	        1 if we have have to lock LOCK_thread_count

  @note
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
  thd->killed= THD::KILL_CONNECTION;
  if ((vio= thd->net.vio) != 0)
  {
    if (errcode)
      net_send_error(thd, errcode, ER(errcode)); /* purecov: inspected */
    vio_close(vio);			/* vio is freed in delete thd */
  }
  if (lock)
    (void) pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_VOID_RETURN;
}
#endif /* EMBEDDED_LIBRARY */


/** Called when a thread is aborted. */
/* ARGSUSED */
extern "C" sig_handler end_thread_signal(int sig __attribute__((unused)))
{
  THD *thd=current_thd;
  DBUG_ENTER("end_thread_signal");
  if (thd && ! thd->bootstrap)
  {
    statistic_increment(killed_threads, &LOCK_status);
    thread_scheduler.end_thread(thd,0);		/* purecov: inspected */
  }
  DBUG_VOID_RETURN;				/* purecov: deadcode */
}


/*
  Unlink thd from global list of available connections and free thd

  SYNOPSIS
    unlink_thd()
    thd		 Thread handler

  NOTES
    LOCK_thread_count is locked and left locked
*/

void unlink_thd(THD *thd)
{
  DBUG_ENTER("unlink_thd");
  DBUG_PRINT("enter", ("thd: 0x%lx", (long) thd));
  thd->cleanup();

  pthread_mutex_lock(&LOCK_connection_count);
  --connection_count;
  pthread_mutex_unlock(&LOCK_connection_count);

  (void) pthread_mutex_lock(&LOCK_thread_count);
  /*
    Used by binlog_reset_master.  It would be cleaner to use
    DEBUG_SYNC here, but that's not possible because the THD's debug
    sync feature has been shut down at this point.
  */
  DBUG_EXECUTE_IF("sleep_after_lock_thread_count_before_delete_thd", sleep(5););
  thread_count--;
  delete thd;
  DBUG_VOID_RETURN;
}


/*
  Store thread in cache for reuse by new connections

  SYNOPSIS
    cache_thread()

  NOTES
    LOCK_thread_count has to be locked

  RETURN
    0  Thread was not put in cache
    1  Thread is to be reused by new connection.
       (ie, caller should return, not abort with pthread_exit())
*/


static bool cache_thread()
{
  safe_mutex_assert_owner(&LOCK_thread_count);
  if (cached_thread_count < thread_cache_size &&
      ! abort_loop && !kill_cached_threads)
  {
    /* Don't kill the thread, just put it in cache for reuse */
    DBUG_PRINT("info", ("Adding thread to cache"));
    cached_thread_count++;
    while (!abort_loop && ! wake_thread && ! kill_cached_threads)
      (void) pthread_cond_wait(&COND_thread_cache, &LOCK_thread_count);
    cached_thread_count--;
    if (kill_cached_threads)
      pthread_cond_signal(&COND_flush_thread_cache);
    if (wake_thread)
    {
      THD *thd;
      wake_thread--;
      thd= thread_cache.get();
      thd->thread_stack= (char*) &thd;          // For store_globals
      (void) thd->store_globals();
      /*
        THD::mysys_var::abort is associated with physical thread rather
        than with THD object. So we need to reset this flag before using
        this thread for handling of new THD object/connection.
      */
      thd->mysys_var->abort= 0;
      thd->thr_create_utime= my_micro_time();
      threads.append(thd);
      return(1);
    }
  }
  return(0);
}


/*
  End thread for the current connection

  SYNOPSIS
    one_thread_per_connection_end()
    thd		  Thread handler
    put_in_cache  Store thread in cache, if there is room in it
                  Normally this is true in all cases except when we got
                  out of resources initializing the current thread

  NOTES
    If thread is cached, we will wait until thread is scheduled to be
    reused and then we will return.
    If thread is not cached, we end the thread.

  RETURN
    0    Signal to handle_one_connection to reuse connection
*/

bool one_thread_per_connection_end(THD *thd, bool put_in_cache)
{
  DBUG_ENTER("one_thread_per_connection_end");
  unlink_thd(thd);
  if (put_in_cache)
    put_in_cache= cache_thread();
  pthread_mutex_unlock(&LOCK_thread_count);
  if (put_in_cache)
    DBUG_RETURN(0);                             // Thread is reused

  /* It's safe to broadcast outside a lock (COND... is not deleted here) */
  DBUG_PRINT("signal", ("Broadcasting COND_thread_count"));
  DBUG_LEAVE;                                   // Must match DBUG_ENTER()
  my_thread_end();
  (void) pthread_cond_broadcast(&COND_thread_count);

  pthread_exit(0);
  return 0;                                     // Avoid compiler warnings
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


#ifdef THREAD_SPECIFIC_SIGPIPE
/**
  Aborts a thread nicely. Comes here on SIGPIPE.

  @todo
    One should have to fix that thr_alarm know about this thread too.
*/
extern "C" sig_handler abort_thread(int sig __attribute__((unused)))
{
  THD *thd=current_thd;
  DBUG_ENTER("abort_thread");
  if (thd)
    thd->killed= THD::KILL_CONNECTION;
  DBUG_VOID_RETURN;
}
#endif


/******************************************************************************
  Setup a signal thread with handles all signals.
  Because Linux doesn't support schemas use a mutex to check that
  the signal thread is ready before continuing
******************************************************************************/

#if defined(__WIN__)


/*
  On Windows, we use native SetConsoleCtrlHandler for handle events like Ctrl-C
  with graceful shutdown.
  Also, we do not use signal(), but SetUnhandledExceptionFilter instead - as it
  provides possibility to pass the exception to just-in-time debugger, collect
  dumps and potentially also the exception and thread context used to output
  callstack.
*/

static BOOL WINAPI console_event_handler( DWORD type ) 
{
  DBUG_ENTER("console_event_handler");
#ifndef EMBEDDED_LIBRARY
  if(type == CTRL_C_EVENT)
  {
     /*
       Do not shutdown before startup is finished and shutdown
       thread is initialized. Otherwise there is a race condition 
       between main thread doing initialization and CTRL-C thread doing
       cleanup, which can result into crash.
     */
#ifndef EMBEDDED_LIBRARY
     if(hEventShutdown)
       kill_mysql();
     else
#endif
       sql_print_warning("CTRL-C ignored during startup");
     DBUG_RETURN(TRUE);
  }
#endif
  DBUG_RETURN(FALSE);
}


/*
  In Visual Studio 2005 and later, default SIGABRT handler will overwrite
  any unhandled exception filter set by the application  and will try to
  call JIT debugger. This is not what we want, this we calling __debugbreak
  to stop in debugger, if process is being debugged or to generate 
  EXCEPTION_BREAKPOINT and then handle_segfault will do its magic.
*/

#if (_MSC_VER >= 1400)
static void my_sigabrt_handler(int sig)
{
  __debugbreak();
}
#endif /*_MSC_VER >=1400 */

void win_install_sigabrt_handler(void)
{
#if (_MSC_VER >=1400)
  /*abort() should not override our exception filter*/
  _set_abort_behavior(0,_CALL_REPORTFAULT);
  signal(SIGABRT,my_sigabrt_handler);
#endif /* _MSC_VER >=1400 */
}

#ifdef DEBUG_UNHANDLED_EXCEPTION_FILTER
#define DEBUGGER_ATTACH_TIMEOUT 120
/*
  Wait for debugger to attach and break into debugger. If debugger is not attached,
  resume after timeout.
*/
static void wait_for_debugger(int timeout_sec)
{
   if(!IsDebuggerPresent())
   {
     int i;
     printf("Waiting for debugger to attach, pid=%u\n",GetCurrentProcessId());
     fflush(stdout);
     for(i= 0; i < timeout_sec; i++)
     {
       Sleep(1000);
       if(IsDebuggerPresent())
       {
         /* Break into debugger */
         __debugbreak();
         return;
       }
     }
     printf("pid=%u, debugger not attached after %d seconds, resuming\n",GetCurrentProcessId(),
       timeout_sec);
     fflush(stdout);
   }
}
#endif /* DEBUG_UNHANDLED_EXCEPTION_FILTER */

LONG WINAPI my_unhandler_exception_filter(EXCEPTION_POINTERS *ex_pointers)
{
   static BOOL first_time= TRUE;
   if(!first_time)
   {
     /*
       This routine can be called twice, typically
       when detaching in JIT debugger.
       Return EXCEPTION_EXECUTE_HANDLER to terminate process.
     */
     return EXCEPTION_EXECUTE_HANDLER;
   }
   first_time= FALSE;
#ifdef DEBUG_UNHANDLED_EXCEPTION_FILTER
   /*
    Unfortunately there is no clean way to debug unhandled exception filters,
    as debugger does not stop there(also documented in MSDN) 
    To overcome, one could put a MessageBox, but this will not work in service.
    Better solution is to print error message and sleep some minutes 
    until debugger is attached
  */
  wait_for_debugger(DEBUGGER_ATTACH_TIMEOUT);
#endif /* DEBUG_UNHANDLED_EXCEPTION_FILTER */
  __try
  {
    my_set_exception_pointers(ex_pointers);
    handle_fatal_signal(ex_pointers->ExceptionRecord->ExceptionCode);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    DWORD written;
    const char msg[] = "Got exception in exception handler!\n";
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),msg, sizeof(msg)-1, 
      &written,NULL);
  }
  /*
    Return EXCEPTION_CONTINUE_SEARCH to give JIT debugger
    (drwtsn32 or vsjitdebugger) possibility to attach,
    if JIT debugger is configured.
    Windows Error reporting might generate a dump here.
  */
  return EXCEPTION_CONTINUE_SEARCH;
}


static void init_signals(void)
{
  win_install_sigabrt_handler();
  if(opt_console)
    SetConsoleCtrlHandler(console_event_handler,TRUE);

    /* Avoid MessageBox()es*/
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);

   /*
     Do not use SEM_NOGPFAULTERRORBOX in the following SetErrorMode (),
     because it would prevent JIT debugger and Windows error reporting
     from working. We need WER or JIT-debugging, since our own unhandled
     exception filter is not guaranteed to work in all situation
     (like heap corruption or stack overflow)
   */
  SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS
                               | SEM_NOOPENFILEERRORBOX);
  SetUnhandledExceptionFilter(my_unhandler_exception_filter);
}


static void start_signal_handler(void)
{
#ifndef EMBEDDED_LIBRARY
  // Save vm id of this process
  if (!opt_bootstrap)
    create_pid_file();
#endif /* EMBEDDED_LIBRARY */
}


static void check_data_home(const char *path)
{}


#elif defined(__NETWARE__)

/// down server event callback.
void mysql_down_server_cb(void *, void *)
{
  event_flag= TRUE;
  kill_server(0);
}


/// destroy callback resources.
void mysql_cb_destroy(void *)
{
  UnRegisterEventNotification(eh);  // cleanup down event notification
  NX_UNWRAP_INTERFACE(ref);
  /* Deregister NSS volume deactivation event */
  NX_UNWRAP_INTERFACE(refneb);
  if (neb_consumer_id)
    UnRegisterConsumer(neb_consumer_id, NULL);
}


/// initialize callbacks.
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


/** To get the name of the NetWare volume having MySQL data folder. */
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


/**
  Registering with NEB for NSS Volume Deactivation event.
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


/**
  Callback for NSS Volume Deactivation event.
*/

ulong neb_event_callback(struct EventBlock *eblock)
{
  EventChangeVolStateEnter_s *voldata;
  extern bool nw_panic;

  voldata= (EventChangeVolStateEnter_s *)eblock->EBEventData;

  /* Deactivation of a volume */
  if ((voldata->oldState == zVOLSTATE_ACTIVE &&
       voldata->newState == zVOLSTATE_DEACTIVE ||
       voldata->newState == zVOLSTATE_MAINTENANCE))
  {
    /*
      Ensure that we bring down MySQL server only for MySQL data
      volume deactivation
    */
    if (!memcmp(&voldata->volID, &datavolid, sizeof(VolumeID_t)))
    {
      consoleprintf("MySQL data volume is deactivated, shutting down MySQL Server \n");
      event_flag= TRUE;
      nw_panic = TRUE;
      event_flag= TRUE;
      kill_server(0);
    }
  }
  return 0;
}


#define ADMIN_VOL_PATH					"_ADMIN:/Volumes/"

/**
  Function to get NSS volume ID of the MySQL data.
*/
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


/**
  Warn if the data is on a Traditional volume.

  @note
    Already done by mysqld_safe
*/

static void check_data_home(const char *path)
{
}

#endif /*__WIN__ || __NETWARE */


#if BACKTRACE_DEMANGLE
#include <cxxabi.h>
extern "C" char *my_demangle(const char *mangled_name, int *status)
{
  return abi::__cxa_demangle(mangled_name, NULL, NULL, status);
}
#endif


#if !defined(__WIN__) && !defined(__NETWARE__)
#ifndef SA_RESETHAND
#define SA_RESETHAND 0
#endif
#ifndef SA_NODEFER
#define SA_NODEFER 0
#endif

#ifndef EMBEDDED_LIBRARY

static void init_signals(void)
{
  sigset_t set;
  struct sigaction sa;
  DBUG_ENTER("init_signals");

  my_sigset(THR_SERVER_ALARM,print_signal_warning); // Should never be called!

  if (!(test_flags & TEST_NO_STACKTRACE) || (test_flags & TEST_CORE_ON_SIGNAL))
  {
    sa.sa_flags = SA_RESETHAND | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sigprocmask(SIG_SETMASK,&sa.sa_mask,NULL);

#ifdef HAVE_STACKTRACE
    my_init_stacktrace();
#endif
#if defined(__amiga__)
    sa.sa_handler=(void(*)())handle_fatal_signal;
#else
    sa.sa_handler=handle_fatal_signal;
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
    STRUCT_RLIMIT rl;
    rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &rl) && global_system_variables.log_warnings)
      sql_print_warning("setrlimit could not change the size of core files to 'infinity';  We may not be able to generate a core file on signals");
  }
#endif
  (void) sigemptyset(&set);
  my_sigset(SIGPIPE,SIG_IGN);
  sigaddset(&set,SIGPIPE);
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
  if (thd_lib_detected != THD_LIB_LT)
    sigaddset(&set,THR_SERVER_ALARM);
  if (test_flags & TEST_SIGINT)
  {
    my_sigset(thr_kill_signal, end_thread_signal);
    // May be SIGINT
    sigdelset(&set, thr_kill_signal);
  }
  else
    sigaddset(&set,SIGINT);
  sigprocmask(SIG_SETMASK,&set,NULL);
  pthread_sigmask(SIG_SETMASK,&set,NULL);
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
#if defined(__ia64__) || defined(__ia64)
  /*
    Peculiar things with ia64 platforms - it seems we only have half the
    stack size in reality, so we have to double it here
  */
  pthread_attr_setstacksize(&thr_attr,my_thread_stack_size*2);
#else
  pthread_attr_setstacksize(&thr_attr,my_thread_stack_size);
#endif
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


/** This threads handles all signals and alarms. */
/* ARGSUSED */
pthread_handler_t signal_hand(void *arg __attribute__((unused)))
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
  init_thr_alarm(thread_scheduler.max_threads +
		 global_system_variables.max_insert_delayed_threads + 10);
  if (thd_lib_detected != THD_LIB_LT && (test_flags & TEST_SIGINT))
  {
    (void) sigemptyset(&set);			// Setup up SIGINT for debug
    (void) sigaddset(&set,SIGINT);		// For debugging
    (void) pthread_sigmask(SIG_UNBLOCK,&set,NULL);
  }
  (void) sigemptyset(&set);			// Setup up SIGINT for debug
#ifdef USE_ONE_SIGNAL_HAND
  (void) sigaddset(&set,THR_SERVER_ALARM);	// For alarms
#endif
#ifndef IGNORE_SIGHUP_SIGQUIT
  (void) sigaddset(&set,SIGQUIT);
  (void) sigaddset(&set,SIGHUP);
#endif
  (void) sigaddset(&set,SIGTERM);
  (void) sigaddset(&set,SIGTSTP);

  /* Save pid to this process (or thread on Linux) */
  if (!opt_bootstrap)
    create_pid_file();

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
      DBUG_PRINT("quit",("signal_handler: calling my_thread_end()"));
      my_thread_end();
      signal_thread_in_use= 0;
      DBUG_LEAVE;                               // Must match DBUG_ENTER()
      pthread_exit(0);				// Safety
      return 0;                                 // Avoid compiler warnings
    }
    switch (sig) {
    case SIGTERM:
    case SIGQUIT:
    case SIGKILL:
#ifdef EXTRA_DEBUG
      sql_print_information("Got signal %d to shutdown mysqld",sig);
#endif
      /* switch to the old log message processing */
      logger.set_handlers(LOG_FILE, opt_slow_log ? LOG_FILE:LOG_NONE,
                          opt_log ? LOG_FILE:LOG_NONE);
      DBUG_PRINT("info",("Got signal: %d  abort_loop: %d",sig,abort_loop));
      if (!abort_loop)
      {
	abort_loop=1;				// mark abort for threads
#ifdef USE_ONE_SIGNAL_HAND
	pthread_t tmp;
	if (!(opt_specialflag & SPECIAL_NO_PRIOR))
	  my_pthread_attr_setprio(&connection_attrib,INTERRUPT_PRIOR);
	if (pthread_create(&tmp,&connection_attrib, kill_server_thread,
			   (void*) &sig))
	  sql_print_error("Can't create thread to kill server");
#else
	kill_server((void*) sig);	// MIT THREAD has a alarm thread
#endif
      }
      break;
    case SIGHUP:
      if (!abort_loop)
      {
        int not_used;
	mysql_print_status();		// Print some debug info
	reload_acl_and_cache((THD*) 0,
			     (REFRESH_LOG | REFRESH_TABLES | REFRESH_FAST |
			      REFRESH_GRANT |
			      REFRESH_THREADS | REFRESH_HOSTS),
			     (TABLE_LIST*) 0, &not_used); // Flush logs
      }
      /* reenable logs after the options were reloaded */
      if (log_output_options & LOG_NONE)
      {
        logger.set_handlers(LOG_FILE,
                            opt_slow_log ? LOG_TABLE : LOG_NONE,
                            opt_log ? LOG_TABLE : LOG_NONE);
      }
      else
      {
        logger.set_handlers(LOG_FILE,
                            opt_slow_log ? log_output_options : LOG_NONE,
                            opt_log ? log_output_options : LOG_NONE);
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

static void check_data_home(const char *path)
{}

#endif /*!EMBEDDED_LIBRARY*/
#endif	/* __WIN__*/


/**
  All global error messages are sent here where the first one is stored
  for the client.
*/
/* ARGSUSED */
extern "C" int my_message_sql(uint error, const char *str, myf MyFlags);

int my_message_sql(uint error, const char *str, myf MyFlags)
{
  THD *thd;
  DBUG_ENTER("my_message_sql");
  DBUG_PRINT("error", ("error: %u  message: '%s'", error, str));

  DBUG_ASSERT(str != NULL);
  /*
    An error should have a valid error number (!= 0), so it can be caught
    in stored procedures by SQL exception handlers.
    Calling my_error() with error == 0 is a bug.
    Remaining known places to fix:
    - storage/myisam/mi_create.c, my_printf_error()
    TODO:
    DBUG_ASSERT(error != 0);
  */

  if (error == 0)
  {
    /* At least, prevent new abuse ... */
    DBUG_ASSERT(strncmp(str, "MyISAM table", 12) == 0);
    error= ER_UNKNOWN_ERROR;
  }

  if ((thd= current_thd))
  {
    /*
      TODO: There are two exceptions mechanism (THD and sp_rcontext),
      this could be improved by having a common stack of handlers.
    */
    if (thd->handle_error(error, str,
                          MYSQL_ERROR::WARN_LEVEL_ERROR))
      DBUG_RETURN(0);

    thd->is_slave_error=  1; // needed to catch query errors during replication

    /*
      thd->lex->current_select == 0 if lex structure is not inited
      (not query command (COM_QUERY))
    */
    if (thd->lex->current_select &&
	thd->lex->current_select->no_error && !thd->is_fatal_error)
    {
      DBUG_PRINT("error",
                 ("Error converted to warning: current_select: no_error %d  "
                  "fatal_error: %d",
                  (thd->lex->current_select ?
                   thd->lex->current_select->no_error : 0),
                  (int) thd->is_fatal_error));
    }
    else
    {
      if (! thd->main_da.is_error())            // Return only first message
      {
        thd->main_da.set_error_status(thd, error, str);
      }
      query_cache_abort(&thd->net);
    }
    /*
      If a continue handler is found, the error message will be cleared
      by the stored procedures code.
    */
    if (thd->spcont &&
        ! (MyFlags & ME_NO_SP_HANDLER) &&
        thd->spcont->handle_error(error, MYSQL_ERROR::WARN_LEVEL_ERROR, thd))
    {
      /*
        Do not push any warnings, a handled error must be completely
        silenced.
      */
      DBUG_RETURN(0);
    }

    /* When simulating OOM, skip writing to error log to avoid mtr errors */
    DBUG_EXECUTE_IF("simulate_out_of_memory", DBUG_RETURN(0););

    if (!thd->no_warnings_for_error &&
        !(MyFlags & ME_NO_WARNING_FOR_ERROR))
    {
      /*
        Suppress infinite recursion if there a memory allocation error
        inside push_warning.
      */
      thd->no_warnings_for_error= TRUE;
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_ERROR, error, str);
      thd->no_warnings_for_error= FALSE;
    }
  }

  /* When simulating OOM, skip writing to error log to avoid mtr errors */
  DBUG_EXECUTE_IF("simulate_out_of_memory", DBUG_RETURN(0););

  if (!thd || MyFlags & ME_NOREFRESH)
    sql_print_error("%s: %s",my_progname,str); /* purecov: inspected */
  DBUG_RETURN(0);
}


#ifndef EMBEDDED_LIBRARY
extern "C" void *my_str_malloc_mysqld(size_t size);
extern "C" void my_str_free_mysqld(void *ptr);

void *my_str_malloc_mysqld(size_t size)
{
  return my_malloc(size, MYF(MY_FAE));
}


void my_str_free_mysqld(void *ptr)
{
  my_free((uchar*)ptr, MYF(MY_FAE));
}
#endif /* EMBEDDED_LIBRARY */


#ifdef __WIN__

pthread_handler_t handle_shutdown(void *arg)
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
#endif

const char *load_default_groups[]= {
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
"mysql_cluster",
#endif
"mysqld","server", MYSQL_BASE_VERSION, 0, 0};

#if defined(__WIN__) && !defined(EMBEDDED_LIBRARY)
static const int load_default_groups_sz=
sizeof(load_default_groups)/sizeof(load_default_groups[0]);
#endif


#ifndef EMBEDDED_LIBRARY
static
int
check_enough_stack_size()
{
  uchar stack_top;

  return check_stack_overrun(current_thd, STACK_MIN_SIZE,
                             &stack_top);
}
#endif


/**
  Initialize one of the global date/time format variables.

  @param format_type		What kind of format should be supported
  @param var_ptr		Pointer to variable that should be updated

  @note
    The default value is taken from either opt_date_time_formats[] or
    the ISO format (ANSI SQL)

  @retval
    0 ok
  @retval
    1 error
*/

static bool init_global_datetime_format(timestamp_type format_type,
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

SHOW_VAR com_status_vars[]= {
  {"admin_commands",       (char*) offsetof(STATUS_VAR, com_other), SHOW_LONG_STATUS},
  {"assign_to_keycache",   (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ASSIGN_TO_KEYCACHE]), SHOW_LONG_STATUS},
  {"alter_db",             (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ALTER_DB]), SHOW_LONG_STATUS},
  {"alter_db_upgrade",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ALTER_DB_UPGRADE]), SHOW_LONG_STATUS},
  {"alter_event",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ALTER_EVENT]), SHOW_LONG_STATUS},
  {"alter_function",       (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ALTER_FUNCTION]), SHOW_LONG_STATUS},
  {"alter_procedure",      (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ALTER_PROCEDURE]), SHOW_LONG_STATUS},
  {"alter_server",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ALTER_SERVER]), SHOW_LONG_STATUS},
  {"alter_table",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ALTER_TABLE]), SHOW_LONG_STATUS},
  {"alter_tablespace",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ALTER_TABLESPACE]), SHOW_LONG_STATUS},
  {"analyze",              (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ANALYZE]), SHOW_LONG_STATUS},
  {"backup_table",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_BACKUP_TABLE]), SHOW_LONG_STATUS},
  {"begin",                (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_BEGIN]), SHOW_LONG_STATUS},
  {"binlog",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_BINLOG_BASE64_EVENT]), SHOW_LONG_STATUS},
  {"call_procedure",       (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CALL]), SHOW_LONG_STATUS},
  {"change_db",            (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CHANGE_DB]), SHOW_LONG_STATUS},
  {"change_master",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CHANGE_MASTER]), SHOW_LONG_STATUS},
  {"check",                (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CHECK]), SHOW_LONG_STATUS},
  {"checksum",             (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CHECKSUM]), SHOW_LONG_STATUS},
  {"commit",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_COMMIT]), SHOW_LONG_STATUS},
  {"create_db",            (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CREATE_DB]), SHOW_LONG_STATUS},
  {"create_event",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CREATE_EVENT]), SHOW_LONG_STATUS},
  {"create_function",      (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CREATE_SPFUNCTION]), SHOW_LONG_STATUS},
  {"create_index",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CREATE_INDEX]), SHOW_LONG_STATUS},
  {"create_procedure",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CREATE_PROCEDURE]), SHOW_LONG_STATUS},
  {"create_server",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CREATE_SERVER]), SHOW_LONG_STATUS},
  {"create_table",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CREATE_TABLE]), SHOW_LONG_STATUS},
  {"create_trigger",       (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CREATE_TRIGGER]), SHOW_LONG_STATUS},
  {"create_udf",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CREATE_FUNCTION]), SHOW_LONG_STATUS},
  {"create_user",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CREATE_USER]), SHOW_LONG_STATUS},
  {"create_view",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CREATE_VIEW]), SHOW_LONG_STATUS},
  {"dealloc_sql",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DEALLOCATE_PREPARE]), SHOW_LONG_STATUS},
  {"delete",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DELETE]), SHOW_LONG_STATUS},
  {"delete_multi",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DELETE_MULTI]), SHOW_LONG_STATUS},
  {"do",                   (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DO]), SHOW_LONG_STATUS},
  {"drop_db",              (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DROP_DB]), SHOW_LONG_STATUS},
  {"drop_event",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DROP_EVENT]), SHOW_LONG_STATUS},
  {"drop_function",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DROP_FUNCTION]), SHOW_LONG_STATUS},
  {"drop_index",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DROP_INDEX]), SHOW_LONG_STATUS},
  {"drop_procedure",       (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DROP_PROCEDURE]), SHOW_LONG_STATUS},
  {"drop_server",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DROP_SERVER]), SHOW_LONG_STATUS},
  {"drop_table",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DROP_TABLE]), SHOW_LONG_STATUS},
  {"drop_trigger",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DROP_TRIGGER]), SHOW_LONG_STATUS},
  {"drop_user",            (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DROP_USER]), SHOW_LONG_STATUS},
  {"drop_view",            (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DROP_VIEW]), SHOW_LONG_STATUS},
  {"empty_query",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_EMPTY_QUERY]), SHOW_LONG_STATUS},
  {"execute_sql",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_EXECUTE]), SHOW_LONG_STATUS},
  {"flush",                (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_FLUSH]), SHOW_LONG_STATUS},
  {"grant",                (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_GRANT]), SHOW_LONG_STATUS},
  {"ha_close",             (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_HA_CLOSE]), SHOW_LONG_STATUS},
  {"ha_open",              (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_HA_OPEN]), SHOW_LONG_STATUS},
  {"ha_read",              (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_HA_READ]), SHOW_LONG_STATUS},
  {"help",                 (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_HELP]), SHOW_LONG_STATUS},
  {"insert",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_INSERT]), SHOW_LONG_STATUS},
  {"insert_select",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_INSERT_SELECT]), SHOW_LONG_STATUS},
  {"install_plugin",       (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_INSTALL_PLUGIN]), SHOW_LONG_STATUS},
  {"kill",                 (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_KILL]), SHOW_LONG_STATUS},
  {"load",                 (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_LOAD]), SHOW_LONG_STATUS},
  {"load_master_data",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_LOAD_MASTER_DATA]), SHOW_LONG_STATUS},
  {"load_master_table",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_LOAD_MASTER_TABLE]), SHOW_LONG_STATUS},
  {"lock_tables",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_LOCK_TABLES]), SHOW_LONG_STATUS},
  {"optimize",             (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_OPTIMIZE]), SHOW_LONG_STATUS},
  {"preload_keys",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_PRELOAD_KEYS]), SHOW_LONG_STATUS},
  {"prepare_sql",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_PREPARE]), SHOW_LONG_STATUS},
  {"purge",                (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_PURGE]), SHOW_LONG_STATUS},
  {"purge_before_date",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_PURGE_BEFORE]), SHOW_LONG_STATUS},
  {"release_savepoint",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_RELEASE_SAVEPOINT]), SHOW_LONG_STATUS},
  {"rename_table",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_RENAME_TABLE]), SHOW_LONG_STATUS},
  {"rename_user",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_RENAME_USER]), SHOW_LONG_STATUS},
  {"repair",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_REPAIR]), SHOW_LONG_STATUS},
  {"replace",              (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_REPLACE]), SHOW_LONG_STATUS},
  {"replace_select",       (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_REPLACE_SELECT]), SHOW_LONG_STATUS},
  {"reset",                (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_RESET]), SHOW_LONG_STATUS},
  {"restore_table",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_RESTORE_TABLE]), SHOW_LONG_STATUS},
  {"revoke",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_REVOKE]), SHOW_LONG_STATUS},
  {"revoke_all",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_REVOKE_ALL]), SHOW_LONG_STATUS},
  {"rollback",             (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ROLLBACK]), SHOW_LONG_STATUS},
  {"rollback_to_savepoint",(char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ROLLBACK_TO_SAVEPOINT]), SHOW_LONG_STATUS},
  {"savepoint",            (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SAVEPOINT]), SHOW_LONG_STATUS},
  {"select",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SELECT]), SHOW_LONG_STATUS},
  {"set_option",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SET_OPTION]), SHOW_LONG_STATUS},
  {"show_authors",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_AUTHORS]), SHOW_LONG_STATUS},
  {"show_binlog_events",   (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_BINLOG_EVENTS]), SHOW_LONG_STATUS},
  {"show_binlogs",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_BINLOGS]), SHOW_LONG_STATUS},
  {"show_charsets",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_CHARSETS]), SHOW_LONG_STATUS},
  {"show_collations",      (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_COLLATIONS]), SHOW_LONG_STATUS},
  {"show_column_types",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_COLUMN_TYPES]), SHOW_LONG_STATUS},
  {"show_contributors",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_CONTRIBUTORS]), SHOW_LONG_STATUS},
  {"show_create_db",       (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_CREATE_DB]), SHOW_LONG_STATUS},
  {"show_create_event",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_CREATE_EVENT]), SHOW_LONG_STATUS},
  {"show_create_func",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_CREATE_FUNC]), SHOW_LONG_STATUS},
  {"show_create_proc",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_CREATE_PROC]), SHOW_LONG_STATUS},
  {"show_create_table",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_CREATE]), SHOW_LONG_STATUS},
  {"show_create_trigger",  (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_CREATE_TRIGGER]), SHOW_LONG_STATUS},
  {"show_databases",       (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_DATABASES]), SHOW_LONG_STATUS},
  {"show_engine_logs",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_ENGINE_LOGS]), SHOW_LONG_STATUS},
  {"show_engine_mutex",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_ENGINE_MUTEX]), SHOW_LONG_STATUS},
  {"show_engine_status",   (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_ENGINE_STATUS]), SHOW_LONG_STATUS},
  {"show_events",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_EVENTS]), SHOW_LONG_STATUS},
  {"show_errors",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_ERRORS]), SHOW_LONG_STATUS},
  {"show_fields",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_FIELDS]), SHOW_LONG_STATUS},
#ifndef DBUG_OFF
  {"show_function_code",   (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_FUNC_CODE]), SHOW_LONG_STATUS},
#endif
  {"show_function_status", (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_STATUS_FUNC]), SHOW_LONG_STATUS},
  {"show_grants",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_GRANTS]), SHOW_LONG_STATUS},
  {"show_keys",            (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_KEYS]), SHOW_LONG_STATUS},
  {"show_master_status",   (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_MASTER_STAT]), SHOW_LONG_STATUS},
  {"show_new_master",      (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_NEW_MASTER]), SHOW_LONG_STATUS},
  {"show_open_tables",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_OPEN_TABLES]), SHOW_LONG_STATUS},
  {"show_plugins",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_PLUGINS]), SHOW_LONG_STATUS},
  {"show_privileges",      (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_PRIVILEGES]), SHOW_LONG_STATUS},
#ifndef DBUG_OFF
  {"show_procedure_code",  (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_PROC_CODE]), SHOW_LONG_STATUS},
#endif
  {"show_procedure_status",(char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_STATUS_PROC]), SHOW_LONG_STATUS},
  {"show_processlist",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_PROCESSLIST]), SHOW_LONG_STATUS},
  {"show_profile",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_PROFILE]), SHOW_LONG_STATUS},
  {"show_profiles",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_PROFILES]), SHOW_LONG_STATUS},
  {"show_slave_hosts",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_SLAVE_HOSTS]), SHOW_LONG_STATUS},
  {"show_slave_status",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_SLAVE_STAT]), SHOW_LONG_STATUS},
  {"show_status",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_STATUS]), SHOW_LONG_STATUS},
  {"show_storage_engines", (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_STORAGE_ENGINES]), SHOW_LONG_STATUS},
  {"show_table_status",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_TABLE_STATUS]), SHOW_LONG_STATUS},
  {"show_tables",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_TABLES]), SHOW_LONG_STATUS},
  {"show_triggers",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_TRIGGERS]), SHOW_LONG_STATUS},
  {"show_variables",       (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_VARIABLES]), SHOW_LONG_STATUS},
  {"show_warnings",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_WARNS]), SHOW_LONG_STATUS},
  {"slave_start",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SLAVE_START]), SHOW_LONG_STATUS},
  {"slave_stop",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SLAVE_STOP]), SHOW_LONG_STATUS},
  {"stmt_close",           (char*) offsetof(STATUS_VAR, com_stmt_close), SHOW_LONG_STATUS},
  {"stmt_execute",         (char*) offsetof(STATUS_VAR, com_stmt_execute), SHOW_LONG_STATUS},
  {"stmt_fetch",           (char*) offsetof(STATUS_VAR, com_stmt_fetch), SHOW_LONG_STATUS},
  {"stmt_prepare",         (char*) offsetof(STATUS_VAR, com_stmt_prepare), SHOW_LONG_STATUS},
  {"stmt_reprepare",       (char*) offsetof(STATUS_VAR, com_stmt_reprepare), SHOW_LONG_STATUS},
  {"stmt_reset",           (char*) offsetof(STATUS_VAR, com_stmt_reset), SHOW_LONG_STATUS},
  {"stmt_send_long_data",  (char*) offsetof(STATUS_VAR, com_stmt_send_long_data), SHOW_LONG_STATUS},
  {"truncate",             (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_TRUNCATE]), SHOW_LONG_STATUS},
  {"uninstall_plugin",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_UNINSTALL_PLUGIN]), SHOW_LONG_STATUS},
  {"unlock_tables",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_UNLOCK_TABLES]), SHOW_LONG_STATUS},
  {"update",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_UPDATE]), SHOW_LONG_STATUS},
  {"update_multi",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_UPDATE_MULTI]), SHOW_LONG_STATUS},
  {"xa_commit",            (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_XA_COMMIT]),SHOW_LONG_STATUS},
  {"xa_end",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_XA_END]),SHOW_LONG_STATUS},
  {"xa_prepare",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_XA_PREPARE]),SHOW_LONG_STATUS},
  {"xa_recover",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_XA_RECOVER]),SHOW_LONG_STATUS},
  {"xa_rollback",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_XA_ROLLBACK]),SHOW_LONG_STATUS},
  {"xa_start",             (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_XA_START]),SHOW_LONG_STATUS},
  {NullS, NullS, SHOW_LONG}
};

static int init_common_variables(const char *conf_file_name, int argc,
				 char **argv, const char **groups)
{
  char buff[FN_REFLEN], *s;
  umask(((~my_umask) & 0666));
  my_decimal_set_zero(&decimal_zero); // set decimal_zero constant;
  tzset();			// Set tzname

  max_system_variables.pseudo_thread_id= (ulong)~0;
  server_start_time= flush_status_time= my_time(0);

  rpl_filter= new Rpl_filter;
  binlog_filter= new Rpl_filter;
  if (!rpl_filter || !binlog_filter)
  {
    sql_perror("Could not allocate replication and binlog filters");
    return 1;
  }

  if (init_thread_environment() ||
      mysql_init_variables())
    return 1;

#ifdef HAVE_TZNAME
  {
    struct tm tm_tmp;
    localtime_r(&server_start_time,&tm_tmp);
    strmake(system_time_zone, tzname[tm_tmp.tm_isdst != 0 ? 1 : 0],
            sizeof(system_time_zone)-1);

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
    Init mutexes for the global MYSQL_BIN_LOG objects.
    As safe_mutex depends on what MY_INIT() does, we can't init the mutexes of
    global MYSQL_BIN_LOGs in their constructors, because then they would be
    inited before MY_INIT(). So we do it here.
  */
  mysql_bin_log.init_pthread_objects();

  /* TODO: remove this when my_time_t is 64 bit compatible */
  if (!IS_TIME_T_VALID_FOR_TIMESTAMP(server_start_time))
  {
    sql_print_error("This MySQL server doesn't support dates later then 2038");
    return 1;
  }

  if (gethostname(glob_hostname,sizeof(glob_hostname)) < 0)
  {
    strmake(glob_hostname, STRING_WITH_LEN("localhost"));
    sql_print_warning("gethostname failed, using '%s' as hostname",
                      glob_hostname);
    strmake(pidfile_name, STRING_WITH_LEN("mysql"));
  }
  else
  strmake(pidfile_name, glob_hostname, sizeof(pidfile_name)-5);
  strmov(fn_ext(pidfile_name),".pid");		// Add proper extension

  /*
    Add server status variables to the dynamic list of
    status variables that is shown by SHOW STATUS.
    Later, in plugin_init, and mysql_install_plugin
    new entries could be added to that list.
  */
  if (add_status_vars(status_vars))
    return 1; // an error was already reported

#ifndef DBUG_OFF
  /*
    We have few debug-only commands in com_status_vars, only visible in debug
    builds. for simplicity we enable the assert only in debug builds

    There are 8 Com_ variables which don't have corresponding SQLCOM_ values:
    (TODO strictly speaking they shouldn't be here, should not have Com_ prefix
    that is. Perhaps Stmt_ ? Comstmt_ ? Prepstmt_ ?)

      Com_admin_commands       => com_other
      Com_stmt_close           => com_stmt_close
      Com_stmt_execute         => com_stmt_execute
      Com_stmt_fetch           => com_stmt_fetch
      Com_stmt_prepare         => com_stmt_prepare
      Com_stmt_reprepare       => com_stmt_reprepare
      Com_stmt_reset           => com_stmt_reset
      Com_stmt_send_long_data  => com_stmt_send_long_data

    With this correction the number of Com_ variables (number of elements in
    the array, excluding the last element - terminator) must match the number
    of SQLCOM_ constants.
  */
  compile_time_assert(sizeof(com_status_vars)/sizeof(com_status_vars[0]) - 1 ==
                     SQLCOM_END + 8);
#endif

  orig_argc=argc;
  orig_argv=argv;
  load_defaults(conf_file_name, groups, &argc, &argv);
  defaults_argv=argv;
  defaults_argc=argc;
  if (get_options(&defaults_argc, defaults_argv))
    return 1;
  set_server_version();

  DBUG_PRINT("info",("%s  Ver %s for %s on %s\n",my_progname,
		     server_version, SYSTEM_TYPE,MACHINE_TYPE));

#ifdef HAVE_LARGE_PAGES
  /* Initialize large page size */
  if (opt_large_pages && (opt_large_page_size= my_get_large_page_size()))
  {
      my_use_large_pages= 1;
      my_large_page_size= opt_large_page_size;
  }
#endif /* HAVE_LARGE_PAGES */

  /* connections and databases needs lots of files */
  {
    uint files, wanted_files, max_open_files;

    /* MyISAM requires two file handles per table. */
    wanted_files= 10+max_connections+table_cache_size*2;
    /*
      We are trying to allocate no less than max_connections*5 file
      handles (i.e. we are trying to set the limit so that they will
      be available).  In addition, we allocate no less than how much
      was already allocated.  However below we report a warning and
      recompute values only if we got less file handles than were
      explicitly requested.  No warning and re-computation occur if we
      can't get max_connections*5 but still got no less than was
      requested (value of wanted_files).
    */
    max_open_files= max(max(wanted_files, max_connections*5),
                        open_files_limit);
    files= my_set_max_open_files(max_open_files);

    if (files < wanted_files)
    {
      if (!open_files_limit)
      {
        /*
          If we have requested too much file handles than we bring
          max_connections in supported bounds.
        */
        max_connections= (ulong) min(files-10-TABLE_OPEN_CACHE_MIN*2,
                                     max_connections);
        /*
          Decrease table_cache_size according to max_connections, but
          not below TABLE_OPEN_CACHE_MIN.  Outer min() ensures that we
          never increase table_cache_size automatically (that could
          happen if max_connections is decreased above).
        */
        table_cache_size= (ulong) min(max((files-10-max_connections)/2,
                                          TABLE_OPEN_CACHE_MIN),
                                      table_cache_size);
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
  if (item_create_init())
    return 1;
  item_init();
  if (set_var_init())
    return 1;
#ifdef HAVE_REPLICATION
  if (init_replication_sys_vars())
    return 1;
#endif
  mysys_uses_curses=0;
#ifdef USE_REGEX
#ifndef EMBEDDED_LIBRARY
  my_regex_init(&my_charset_latin1, check_enough_stack_size);
#else
  my_regex_init(&my_charset_latin1, NULL);
#endif
#endif
  /*
    Process a comma-separated character set list and choose
    the first available character set. This is mostly for
    test purposes, to be able to start "mysqld" even if
    the requested character set is not available (see bug#18743).
  */
  for (;;)
  {
    char *next_character_set_name= strchr(default_character_set_name, ',');
    if (next_character_set_name)
      *next_character_set_name++= '\0';
    if (!(default_charset_info=
          get_charset_by_csname(default_character_set_name,
                                MY_CS_PRIMARY, MYF(MY_WME))))
    {
      if (next_character_set_name)
      {
        default_character_set_name= next_character_set_name;
        default_collation_name= 0;          // Ignore collation
      }
      else
        return 1;                           // Eof of the list
    }
    else
      break;
  }

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

  if (!(character_set_filesystem= 
        get_charset_by_csname(character_set_filesystem_name,
                              MY_CS_PRIMARY, MYF(MY_WME))))
    return 1;
  global_system_variables.character_set_filesystem= character_set_filesystem;

  if (!(my_default_lc_time_names=
        my_locale_by_name(lc_time_names_name)))
  {
    sql_print_error("Unknown locale: '%s'", lc_time_names_name);
    return 1;
  }
  global_system_variables.lc_time_names= my_default_lc_time_names;
  
  sys_init_connect.value_length= 0;
  if ((sys_init_connect.value= opt_init_connect))
    sys_init_connect.value_length= strlen(opt_init_connect);
  else
    sys_init_connect.value=my_strdup("",MYF(0));
  sys_init_connect.is_os_charset= TRUE;

  sys_init_slave.value_length= 0;
  if ((sys_init_slave.value= opt_init_slave))
    sys_init_slave.value_length= strlen(opt_init_slave);
  else
    sys_init_slave.value=my_strdup("",MYF(0));
  sys_init_slave.is_os_charset= TRUE;

  /* check log options and issue warnings if needed */
  if (opt_log && opt_logname && !(log_output_options & LOG_FILE) &&
      !(log_output_options & LOG_NONE))
    sql_print_warning("Although a path was specified for the "
                      "--log option, log tables are used. "
                      "To enable logging to files use the --log-output option.");

  if (opt_slow_log && opt_slow_logname && !(log_output_options & LOG_FILE)
      && !(log_output_options & LOG_NONE))
    sql_print_warning("Although a path was specified for the "
                      "--log_slow_queries option, log tables are used. "
                      "To enable logging to files use the --log-output=file option.");

  s= opt_logname ? opt_logname : make_default_log_name(buff, ".log");
  sys_var_general_log_path.value= my_strdup(s, MYF(0));
  sys_var_general_log_path.value_length= strlen(s);

  s= opt_slow_logname ? opt_slow_logname : make_default_log_name(buff, "-slow.log");
  sys_var_slow_log_path.value= my_strdup(s, MYF(0));
  sys_var_slow_log_path.value_length= strlen(s);

#if defined(ENABLED_DEBUG_SYNC)
  /* Initialize the debug sync facility. See debug_sync.cc. */
  if (debug_sync_init())
    return 1; /* purecov: tested */
#endif /* defined(ENABLED_DEBUG_SYNC) */

#if (ENABLE_TEMP_POOL)
  if (use_temp_pool && bitmap_init(&temp_pool,0,1024,1))
    return 1;
#else
  use_temp_pool= 0;
#endif

  if (my_database_names_init())
    return 1;

  /*
    Ensure that lower_case_table_names is set on system where we have case
    insensitive names.  If this is not done the users MyISAM tables will
    get corrupted if accesses with names of different case.
  */
  DBUG_PRINT("info", ("lower_case_table_names: %d", lower_case_table_names));
  lower_case_file_system= test_if_case_insensitive(mysql_real_data_home);
  if (!lower_case_table_names && lower_case_file_system == 1)
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
  else if (lower_case_table_names == 2 &&
           !(lower_case_file_system=
             (test_if_case_insensitive(mysql_real_data_home) == 1)))
  {
    if (global_system_variables.log_warnings)
      sql_print_warning("lower_case_table_names was set to 2, even though your "
                        "the file system '%s' is case sensitive.  Now setting "
                        "lower_case_table_names to 0 to avoid future problems.",
			mysql_real_data_home);
    lower_case_table_names= 0;
  }
  else
  {
    lower_case_file_system=
      (test_if_case_insensitive(mysql_real_data_home) == 1);
  }

  /* Reset table_alias_charset, now that lower_case_table_names is set. */
  table_alias_charset= (lower_case_table_names ?
			files_charset_info :
			&my_charset_bin);

  return 0;
}


static int init_thread_environment()
{
  (void) pthread_mutex_init(&LOCK_mysql_create_db,MY_MUTEX_INIT_SLOW);
  (void) pthread_mutex_init(&LOCK_lock_db,MY_MUTEX_INIT_SLOW);
  (void) pthread_mutex_init(&LOCK_Acl,MY_MUTEX_INIT_SLOW);
  (void) pthread_mutex_init(&LOCK_open, MY_MUTEX_INIT_FAST);
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
  (void) my_rwlock_init(&LOCK_system_variables_hash, NULL);
  (void) pthread_mutex_init(&LOCK_global_read_lock, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_prepared_stmt_count, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_uuid_generator, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_connection_count, MY_MUTEX_INIT_FAST);
#ifdef HAVE_OPENSSL
  (void) pthread_mutex_init(&LOCK_des_key_file,MY_MUTEX_INIT_FAST);
#ifndef HAVE_YASSL
  openssl_stdlocks= (openssl_lock_t*) OPENSSL_malloc(CRYPTO_num_locks() *
                                                     sizeof(openssl_lock_t));
  for (int i= 0; i < CRYPTO_num_locks(); ++i)
    (void) my_rwlock_init(&openssl_stdlocks[i].lock, NULL); 
  CRYPTO_set_dynlock_create_callback(openssl_dynlock_create);
  CRYPTO_set_dynlock_destroy_callback(openssl_dynlock_destroy);
  CRYPTO_set_dynlock_lock_callback(openssl_lock);
  CRYPTO_set_locking_callback(openssl_lock_function);
  CRYPTO_set_id_callback(openssl_id_function);
#endif
#endif
  (void) my_rwlock_init(&LOCK_sys_init_connect, NULL);
  (void) my_rwlock_init(&LOCK_sys_init_slave, NULL);
  (void) my_rwlock_init(&LOCK_grant, NULL);
  (void) pthread_cond_init(&COND_thread_count,NULL);
  (void) pthread_cond_init(&COND_refresh,NULL);
  (void) pthread_cond_init(&COND_global_read_lock,NULL);
  (void) pthread_cond_init(&COND_thread_cache,NULL);
  (void) pthread_cond_init(&COND_flush_thread_cache,NULL);
  (void) pthread_cond_init(&COND_manager,NULL);
#ifdef HAVE_REPLICATION
  (void) pthread_mutex_init(&LOCK_rpl_status, MY_MUTEX_INIT_FAST);
  (void) pthread_cond_init(&COND_rpl_status, NULL);
#endif
  (void) pthread_mutex_init(&LOCK_server_started, MY_MUTEX_INIT_FAST);
  (void) pthread_cond_init(&COND_server_started,NULL);
  sp_cache_init();
#ifdef HAVE_EVENT_SCHEDULER
  Events::init_mutexes();
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


#if defined(HAVE_OPENSSL) && !defined(HAVE_YASSL)
static unsigned long openssl_id_function()
{ 
  return (unsigned long) pthread_self();
} 


static openssl_lock_t *openssl_dynlock_create(const char *file, int line)
{ 
  openssl_lock_t *lock= new openssl_lock_t;
  my_rwlock_init(&lock->lock, NULL);
  return lock;
}


static void openssl_dynlock_destroy(openssl_lock_t *lock, const char *file, 
				    int line)
{
  rwlock_destroy(&lock->lock);
  delete lock;
}


static void openssl_lock_function(int mode, int n, const char *file, int line)
{
  if (n < 0 || n > CRYPTO_num_locks())
  {
    /* Lock number out of bounds. */
    sql_print_error("Fatal: OpenSSL interface problem (n = %d)", n);
    abort();
  }
  openssl_lock(mode, &openssl_stdlocks[n], file, line);
}


static void openssl_lock(int mode, openssl_lock_t *lock, const char *file, 
			 int line)
{
  int err;
  char const *what;

  switch (mode) {
  case CRYPTO_LOCK|CRYPTO_READ:
    what = "read lock";
    err = rw_rdlock(&lock->lock);
    break;
  case CRYPTO_LOCK|CRYPTO_WRITE:
    what = "write lock";
    err = rw_wrlock(&lock->lock);
    break;
  case CRYPTO_UNLOCK|CRYPTO_READ:
  case CRYPTO_UNLOCK|CRYPTO_WRITE:
    what = "unlock";
    err = rw_unlock(&lock->lock);
    break;
  default:
    /* Unknown locking mode. */
    sql_print_error("Fatal: OpenSSL interface problem (mode=0x%x)", mode);
    abort();
  }
  if (err) 
  {
    sql_print_error("Fatal: can't %s OpenSSL lock", what);
    abort();
  }
}
#endif /* HAVE_OPENSSL */


#ifndef EMBEDDED_LIBRARY

static void init_ssl()
{
#ifdef HAVE_OPENSSL
  if (opt_use_ssl)
  {
    enum enum_ssl_init_error error= SSL_INITERR_NOERROR;

    /* having ssl_acceptor_fd != 0 signals the use of SSL */
    ssl_acceptor_fd= new_VioSSLAcceptorFd(opt_ssl_key, opt_ssl_cert,
					  opt_ssl_ca, opt_ssl_capath,
					  opt_ssl_cipher, &error);
    DBUG_PRINT("info",("ssl_acceptor_fd: 0x%lx", (long) ssl_acceptor_fd));
    if (!ssl_acceptor_fd)
    {
      sql_print_warning("Failed to setup SSL");
      sql_print_warning("SSL error: %s", sslGetErrString(error));
      opt_use_ssl = 0;
      have_ssl= SHOW_OPTION_DISABLED;
    }
  }
  else
  {
    have_ssl= SHOW_OPTION_DISABLED;
  }
  if (des_key_file)
    load_des_key_file(des_key_file);
#endif /* HAVE_OPENSSL */
}


static void end_ssl()
{
#ifdef HAVE_OPENSSL
  if (ssl_acceptor_fd)
  {
    free_vio_ssl_acceptor_fd(ssl_acceptor_fd);
    ssl_acceptor_fd= 0;
  }
#endif /* HAVE_OPENSSL */
}

#endif /* EMBEDDED_LIBRARY */


static int init_server_components()
{
  DBUG_ENTER("init_server_components");
  /*
    We need to call each of these following functions to ensure that
    all things are initialized so that unireg_abort() doesn't fail
  */
  if (table_cache_init() | table_def_init() | hostname_cache_init())
    unireg_abort(1);

  query_cache_result_size_limit(query_cache_limit);
  query_cache_set_min_res_unit(query_cache_min_res_unit);
  query_cache_init();
  query_cache_resize(query_cache_size);
  randominit(&sql_rand,(ulong) server_start_time,(ulong) server_start_time/2);
  setup_fpu();
  init_thr_lock();
#ifdef HAVE_REPLICATION
  init_slave_list();
#endif

  /* Setup logs */

  /*
    Enable old-fashioned error log, except when the user has requested
    help information. Since the implementation of plugin server
    variables the help output is now written much later.
  */
  if (opt_error_log && !opt_help)
  {
    if (!log_error_file_ptr[0])
      fn_format(log_error_file, pidfile_name, mysql_data_home, ".err",
                MY_REPLACE_EXT); /* replace '.<domain>' by '.err', bug#4997 */
    else
      fn_format(log_error_file, log_error_file_ptr, mysql_data_home, ".err",
                MY_UNPACK_FILENAME | MY_SAFE_PATH);
    if (!log_error_file[0])
      opt_error_log= 1;				// Too long file name
    else
    {
      my_bool res;
#ifndef EMBEDDED_LIBRARY
      res= reopen_fstreams(log_error_file, stdout, stderr);
#else
      res= reopen_fstreams(log_error_file, NULL, stderr);
#endif

      if (!res)
        setbuf(stderr, NULL);
    }
  }

  if (xid_cache_init())
  {
    sql_print_error("Out of memory");
    unireg_abort(1);
  }

  /* need to configure logging before initializing storage engines */
  if (opt_update_log)
  {
    /*
      Update log is removed since 5.0. But we still accept the option.
      The idea is if the user already uses the binlog and the update log,
      we completely ignore any option/variable related to the update log, like
      if the update log did not exist. But if the user uses only the update
      log, then we translate everything into binlog for him (with warnings).
      Implementation of the above :
      - If mysqld is started with --log-update and --log-bin,
      ignore --log-update (print a warning), push a warning when SQL_LOG_UPDATE
      is used, and turn off --sql-bin-update-same.
      This will completely ignore SQL_LOG_UPDATE
      - If mysqld is started with --log-update only,
      change it to --log-bin (with the filename passed to log-update,
      plus '-bin') (print a warning), push a warning when SQL_LOG_UPDATE is
      used, and turn on --sql-bin-update-same.
      This will translate SQL_LOG_UPDATE to SQL_LOG_BIN.

      Note that we tell the user that --sql-bin-update-same is deprecated and
      does nothing, and we don't take into account if he used this option or
      not; but internally we give this variable a value to have the behaviour
      we want (i.e. have SQL_LOG_UPDATE influence SQL_LOG_BIN or not).
      As sql-bin-update-same, log-update and log-bin cannot be changed by the
      user after starting the server (they are not variables), the user will
      not later interfere with the settings we do here.
    */
    if (opt_bin_log)
    {
      opt_sql_bin_update= 0;
      sql_print_error("The update log is no longer supported by MySQL in \
version 5.0 and above. It is replaced by the binary log.");
    }
    else
    {
      opt_sql_bin_update= 1;
      opt_bin_log= 1;
      if (opt_update_logname)
      {
        /* as opt_bin_log==0, no need to free opt_bin_logname */
        if (!(opt_bin_logname= my_strdup(opt_update_logname, MYF(MY_WME))))
        {
          sql_print_error("Out of memory");
          return EXIT_OUT_OF_MEMORY;
        }
        sql_print_error("The update log is no longer supported by MySQL in \
version 5.0 and above. It is replaced by the binary log. Now starting MySQL \
with --log-bin='%s' instead.",opt_bin_logname);
      }
      else
        sql_print_error("The update log is no longer supported by MySQL in \
version 5.0 and above. It is replaced by the binary log. Now starting MySQL \
with --log-bin instead.");
    }
  }
  if (opt_log_slave_updates && !opt_bin_log)
  {
    sql_print_error("You need to use --log-bin to make "
                    "--log-slave-updates work.");
    unireg_abort(1);
  }
  if (!opt_bin_log)
  {
    if (opt_binlog_format_id != BINLOG_FORMAT_UNSPEC)
    {
      sql_print_error("You need to use --log-bin to make "
                      "--binlog-format work.");
      unireg_abort(1);
    }
    else
    {
      global_system_variables.binlog_format= BINLOG_FORMAT_STMT;
    }
  }
  else
    if (opt_binlog_format_id == BINLOG_FORMAT_UNSPEC)
      global_system_variables.binlog_format= BINLOG_FORMAT_STMT;
    else
    { 
      DBUG_ASSERT(global_system_variables.binlog_format != BINLOG_FORMAT_UNSPEC);
    }

  /* Check that we have not let the format to unspecified at this point */
  DBUG_ASSERT((uint)global_system_variables.binlog_format <=
              array_elements(binlog_format_names)-1);

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

  if (opt_bin_log)
  {
    /* Reports an error and aborts, if the --log-bin's path 
       is a directory.*/
    if (opt_bin_logname && 
        opt_bin_logname[strlen(opt_bin_logname) - 1] == FN_LIBCHAR)
    {
      sql_print_error("Path '%s' is a directory name, please specify \
a file name for --log-bin option", opt_bin_logname);
      unireg_abort(1);
    }

    /* Reports an error and aborts, if the --log-bin-index's path 
       is a directory.*/
    if (opt_binlog_index_name && 
        opt_binlog_index_name[strlen(opt_binlog_index_name) - 1] 
        == FN_LIBCHAR)
    {
      sql_print_error("Path '%s' is a directory name, please specify \
a file name for --log-bin-index option", opt_binlog_index_name);
      unireg_abort(1);
    }

    char buf[FN_REFLEN];
    const char *ln;
    ln= mysql_bin_log.generate_name(opt_bin_logname, "-bin", 1, buf);
    if (!opt_bin_logname && !opt_binlog_index_name)
    {
      /*
        User didn't give us info to name the binlog index file.
        Picking `hostname`-bin.index like did in 4.x, causes replication to
        fail if the hostname is changed later. So, we would like to instead
        require a name. But as we don't want to break many existing setups, we
        only give warning, not error.
      */
      sql_print_warning("No argument was provided to --log-bin, and "
                        "--log-bin-index was not used; so replication "
                        "may break when this MySQL server acts as a "
                        "master and has his hostname changed!! Please "
                        "use '--log-bin=%s' to avoid this problem.", ln);
    }
    if (ln == buf)
    {
      my_free(opt_bin_logname, MYF(MY_ALLOW_ZERO_PTR));
      opt_bin_logname=my_strdup(buf, MYF(0));
    }
    if (mysql_bin_log.open_index_file(opt_binlog_index_name, ln, TRUE))
    {
      unireg_abort(1);
    }
  }

  /* call ha_init_key_cache() on all key caches to init them */
  process_key_caches(&ha_init_key_cache);

  /* Allow storage engine to give real error messages */
  if (ha_init_errors())
    DBUG_RETURN(1);

  { 
    if (plugin_init(&defaults_argc, defaults_argv,
		    (opt_noacl ? PLUGIN_INIT_SKIP_PLUGIN_TABLE : 0) |
		    (opt_help ? PLUGIN_INIT_SKIP_INITIALIZATION : 0)))
    {
      sql_print_error("Failed to initialize plugins.");
      unireg_abort(1);
    }
    plugins_are_initialized= TRUE;  /* Don't separate from init function */
  }

  if (opt_help)
    unireg_abort(0);

  /* we do want to exit if there are any other unknown options */
  if (defaults_argc > 1)
  {
    int ho_error;
    char **tmp_argv= defaults_argv;
    struct my_option no_opts[]=
    {
      {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
    };
    /*
      We need to eat any 'loose' arguments first before we conclude
      that there are unprocessed options.
      But we need to preserve defaults_argv pointer intact for
      free_defaults() to work. Thus we use a copy here.
    */
    my_getopt_skip_unknown= 0;

    if ((ho_error= handle_options(&defaults_argc, &tmp_argv, no_opts,
                                  mysqld_get_one_option)))
      unireg_abort(ho_error);
    my_getopt_skip_unknown= TRUE;

    if (defaults_argc)
    {
      fprintf(stderr, "%s: Too many arguments (first extra is '%s').\n"
              "Use --verbose --help to get a list of available options\n",
              my_progname, *tmp_argv);
      unireg_abort(1);
    }
  }

  /* if the errmsg.sys is not loaded, terminate to maintain behaviour */
  if (!errmesg[0][0])
    unireg_abort(1);

  /* We have to initialize the storage engines before CSV logging */
  if (ha_init())
  {
    sql_print_error("Can't init databases");
    unireg_abort(1);
  }

#ifdef WITH_CSV_STORAGE_ENGINE
  if (opt_bootstrap)
    log_output_options= LOG_FILE;
  else
    logger.init_log_tables();

  if (log_output_options & LOG_NONE)
  {
    /*
      Issue a warining if there were specified additional options to the
      log-output along with NONE. Probably this wasn't what user wanted.
    */
    if ((log_output_options & LOG_NONE) && (log_output_options & ~LOG_NONE))
      sql_print_warning("There were other values specified to "
                        "log-output besides NONE. Disabling slow "
                        "and general logs anyway.");
    logger.set_handlers(LOG_FILE, LOG_NONE, LOG_NONE);
  }
  else
  {
    /* fall back to the log files if tables are not present */
    LEX_STRING csv_name={C_STRING_WITH_LEN("csv")};
    if (!plugin_is_ready(&csv_name, MYSQL_STORAGE_ENGINE_PLUGIN))
    {
      /* purecov: begin inspected */
      sql_print_error("CSV engine is not present, falling back to the "
                      "log files");
      log_output_options= (log_output_options & ~LOG_TABLE) | LOG_FILE;
      /* purecov: end */
    }

    logger.set_handlers(LOG_FILE, opt_slow_log ? log_output_options:LOG_NONE,
                        opt_log ? log_output_options:LOG_NONE);
  }
#else
  logger.set_handlers(LOG_FILE, opt_slow_log ? LOG_FILE:LOG_NONE,
                      opt_log ? LOG_FILE:LOG_NONE);
#endif

  /*
    Check that the default storage engine is actually available.
  */
  if (default_storage_engine_str)
  {
    LEX_STRING name= { default_storage_engine_str,
                       strlen(default_storage_engine_str) };
    plugin_ref plugin;
    handlerton *hton;
    
    if ((plugin= ha_resolve_by_name(0, &name)))
      hton= plugin_data(plugin, handlerton*);
    else
    {
      sql_print_error("Unknown/unsupported table type: %s",
                      default_storage_engine_str);
      unireg_abort(1);
    }
    if (!ha_storage_engine_is_enabled(hton))
    {
      if (!opt_bootstrap)
      {
        sql_print_error("Default storage engine (%s) is not available",
                        default_storage_engine_str);
        unireg_abort(1);
      }
      DBUG_ASSERT(global_system_variables.table_plugin);
    }
    else
    {
      /*
        Need to unlock as global_system_variables.table_plugin 
        was acquired during plugin_init()
      */
      plugin_unlock(0, global_system_variables.table_plugin);
      global_system_variables.table_plugin= plugin;
    }
  }

  tc_log= (total_ha_2pc > 1 ? (opt_bin_log  ?
                               (TC_LOG *) &mysql_bin_log :
                               (TC_LOG *) &tc_log_mmap) :
           (TC_LOG *) &tc_log_dummy);

  if (tc_log->open(opt_bin_log ? opt_bin_logname : opt_tc_log_file))
  {
    sql_print_error("Can't init tc log");
    unireg_abort(1);
  }

  if (ha_recover(0))
  {
    unireg_abort(1);
  }

  if (opt_bin_log && mysql_bin_log.open(opt_bin_logname, LOG_BIN, 0,
                                        WRITE_CACHE, 0, max_binlog_size, 0, TRUE))
    unireg_abort(1);

#ifdef HAVE_REPLICATION
  if (opt_bin_log && expire_logs_days)
  {
    time_t purge_time= server_start_time - expire_logs_days*24*60*60;
    if (purge_time >= 0)
      mysql_bin_log.purge_logs_before_date(purge_time);
  }
#endif
#ifdef __NETWARE__
  /* Increasing stacksize of threads on NetWare */
  pthread_attr_setstacksize(&connection_attrib, NW_THD_STACKSIZE);
#endif

  if (opt_myisam_log)
    (void) mi_log(1);

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


#ifndef EMBEDDED_LIBRARY

static void create_shutdown_thread()
{
#ifdef __WIN__
  hEventShutdown=CreateEvent(0, FALSE, FALSE, shutdown_event_name);
  pthread_t hThread;
  if (pthread_create(&hThread,&connection_attrib,handle_shutdown,0))
    sql_print_warning("Can't create thread to handle shutdown requests");

  // On "Stop Service" we have to do regular shutdown
  Service.SetShutdownEvent(hEventShutdown);
#endif /* __WIN__ */
}

#endif /* EMBEDDED_LIBRARY */


#if (defined(__NT__) || defined(HAVE_SMEM)) && !defined(EMBEDDED_LIBRARY)
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
  pthread_cond_signal(&COND_handler_count);
  pthread_mutex_unlock(&LOCK_thread_count);  
  my_thread_end();
}
#else
#define decrement_handler_count()
#endif /* defined(__NT__) || defined(HAVE_SMEM) */


#ifndef EMBEDDED_LIBRARY
#ifndef DBUG_OFF
/*
  Debugging helper function to keep the locale database
  (see sql_locale.cc) and max_month_name_length and
  max_day_name_length variable values in consistent state.
*/
static void test_lc_time_sz()
{
  DBUG_ENTER("test_lc_time_sz");
  for (MY_LOCALE **loc= my_locales; *loc; loc++)
  {
    uint max_month_len= 0;
    uint max_day_len = 0;
    for (const char **month= (*loc)->month_names->type_names; *month; month++)
    {
      set_if_bigger(max_month_len,
                    my_numchars_mb(&my_charset_utf8_general_ci,
                                   *month, *month + strlen(*month)));
    }
    for (const char **day= (*loc)->day_names->type_names; *day; day++)
    {
      set_if_bigger(max_day_len,
                    my_numchars_mb(&my_charset_utf8_general_ci,
                                   *day, *day + strlen(*day)));
    }
    if ((*loc)->max_month_name_length != max_month_len ||
        (*loc)->max_day_name_length != max_day_len)
    {
      DBUG_PRINT("Wrong max day name(or month name) length for locale:",
                 ("%s", (*loc)->name));
      DBUG_ASSERT(0);
    }
  }
  DBUG_VOID_RETURN;
}
#endif//DBUG_OFF


#ifdef __WIN__
int win_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
#ifdef HAVE_NPTL
  ld_assume_kernel_is_set= (getenv("LD_ASSUME_KERNEL") != 0);
#endif

  MY_INIT(argv[0]);		// init my_sys library & pthreads
  /* nothing should come before this line ^^^ */

  /* Set signal used to kill MySQL */
#if defined(SIGUSR2)
  thr_kill_signal= thd_lib_detected == THD_LIB_LT ? SIGINT : SIGUSR2;
#else
  thr_kill_signal= SIGINT;
#endif

  /*
    Perform basic logger initialization logger. Should be called after
    MY_INIT, as it initializes mutexes. Log tables are inited later.
  */
  logger.init_base();

#ifdef _CUSTOMSTARTUPCONFIG_
  if (_cust_check_startup())
  {
    / * _cust_check_startup will report startup failure error * /
    exit(1);
  }
#endif

#ifdef	__WIN__
  /*
    Before performing any socket operation (like retrieving hostname
    in init_common_variables we have to call WSAStartup
  */
  {
    WSADATA WsaData;
    if (SOCKET_ERROR == WSAStartup (0x0101, &WsaData))
    {
      /* errors are not read yet, so we use english text here */
      my_message(ER_WSAS_FAILED, "WSAStartup Failed", MYF(0));
      unireg_abort(1);
    }
  }
#endif /* __WIN__ */

  if (init_common_variables(MYSQL_CONFIG_NAME,
			    argc, argv, load_default_groups))
    unireg_abort(1);				// Will do exit

  init_signals();
  if (!(opt_specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),CONNECT_PRIOR);
#if defined(__ia64__) || defined(__ia64)
  /*
    Peculiar things with ia64 platforms - it seems we only have half the
    stack size in reality, so we have to double it here
  */
  pthread_attr_setstacksize(&connection_attrib,my_thread_stack_size*2);
#else
  pthread_attr_setstacksize(&connection_attrib,my_thread_stack_size);
#endif
#ifdef HAVE_PTHREAD_ATTR_GETSTACKSIZE
  {
    /* Retrieve used stack size;  Needed for checking stack overflows */
    size_t stack_size= 0;
    pthread_attr_getstacksize(&connection_attrib, &stack_size);
#if defined(__ia64__) || defined(__ia64)
    stack_size/= 2;
#endif
    /* We must check if stack_size = 0 as Solaris 2.9 can return 0 here */
    if (stack_size && stack_size < my_thread_stack_size)
    {
      if (global_system_variables.log_warnings)
	sql_print_warning("Asked for %lu thread stack, but got %ld",
			  my_thread_stack_size, (long) stack_size);
#if defined(__ia64__) || defined(__ia64)
      my_thread_stack_size= stack_size*2;
#else
      my_thread_stack_size= stack_size;
#endif
    }
  }
#endif
#ifdef __NETWARE__
  /* Increasing stacksize of threads on NetWare */
  pthread_attr_setstacksize(&connection_attrib, NW_THD_STACKSIZE);
#endif

  (void) thr_setconcurrency(concurrency);	// 10 by default

  select_thread=pthread_self();
  select_thread_in_use=1;

#ifdef HAVE_LIBWRAP
  libwrapName= my_progname+dirname_length(my_progname);
  openlog(libwrapName, LOG_PID, LOG_AUTH);
#endif

#ifndef DBUG_OFF
  test_lc_time_sz();
#endif

  /*
    We have enough space for fiddling with the argv, continue
  */
  check_data_home(mysql_real_data_home);
  if (my_setwd(mysql_real_data_home,MYF(MY_WME)) && !opt_help)
    unireg_abort(1);				/* purecov: inspected */
  mysql_data_home= mysql_data_home_buff;
  mysql_data_home[0]=FN_CURLIB;		// all paths are relative from here
  mysql_data_home[1]=0;
  mysql_data_home_len= 2;

  if ((user_info= check_user(mysqld_user)))
  {
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
    if (locked_in_memory) // getuid() == 0 here
      set_effective_user(user_info);
    else
#endif
      set_user(mysqld_user, user_info);
  }

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
    unireg_abort(1);

  init_ssl();
  network_init();

#ifdef __WIN__
  if (!opt_console)
  {
    if (reopen_fstreams(log_error_file, stdout, stderr))
      unireg_abort(1);
    setbuf(stderr, NULL);
    FreeConsole();				// Remove window
  }
#endif

  /*
   Initialize my_str_malloc() and my_str_free()
  */
  my_str_malloc= &my_str_malloc_mysqld;
  my_str_free= &my_str_free_mysqld;

  /*
    init signals & alarm
    After this we can't quit by a simple unireg_abort
  */
  error_handler_hook= my_message_sql;
  start_signal_handler();				// Creates pidfile

  if (mysql_rm_tmp_tables() || acl_init(opt_noacl) ||
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
    (void) grant_init();

  if (!opt_bootstrap)
    servers_init(0);

  if (!opt_noacl)
  {
#ifdef HAVE_DLOPEN
    udf_init();
#endif
  }

  init_status_vars();
  if (opt_bootstrap) /* If running with bootstrap, do not start replication. */
    opt_skip_slave_start= 1;
  /*
    init_slave() must be called after the thread keys are created.
    Some parts of the code (e.g. SHOW STATUS LIKE 'slave_running' and other
    places) assume that active_mi != 0, so let's fail if it's 0 (out of
    memory); a message has already been printed.
  */
  if (init_slave() && !active_mi)
  {
    unireg_abort(1);
  }

  execute_ddl_log_recovery();

  if (Events::init(opt_noacl || opt_bootstrap))
    unireg_abort(1);

  if (opt_bootstrap)
  {
    select_thread_in_use= 0;                    // Allow 'kill' to work
    bootstrap(stdin);
    unireg_abort(bootstrap_error ? 1 : 0);
  }
  if (opt_init_file)
  {
    if (read_init_file(opt_init_file))
      unireg_abort(1);
  }

  create_shutdown_thread();
  start_handle_manager();

  sql_print_information(ER(ER_STARTUP),my_progname,server_version,
                        ((unix_sock == INVALID_SOCKET) ? (char*) ""
                                                       : mysqld_unix_port),
                         mysqld_port,
                         MYSQL_COMPILATION_COMMENT);
#if defined(_WIN32) && !defined(EMBEDDED_LIBRARY)
  Service.SetRunning();
#endif


  /* Signal threads waiting for server to be started */
  pthread_mutex_lock(&LOCK_server_started);
  mysqld_server_started= 1;
  pthread_cond_signal(&COND_server_started);
  pthread_mutex_unlock(&LOCK_server_started);

#if defined(__NT__) || defined(HAVE_SMEM)
  handle_connections_methods();
#else
#ifdef __WIN__
  if (!have_tcpip || opt_disable_networking)
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
  clean_up(1);
  wait_for_signal_thread_to_end();
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
    return strmake(to, from, length-1);
  return strxnmov(to, length-1, "\"", from, "\"", NullS);
}


/**
  Handle basic handling of services, like installation and removal.

  @param argv	   	        Pointer to argument list
  @param servicename		Internal name of service
  @param displayname		Display name of service (in taskbar ?)
  @param file_path		Path to this program
  @param startup_option	Startup option to mysqld

  @retval
    0		option handled
  @retval
    1		Could not handle option
*/

static bool
default_service_handling(char **argv,
			 const char *servicename,
			 const char *displayname,
			 const char *file_path,
			 const char *extra_opt,
			 const char *account_name)
{
  char path_and_service[FN_REFLEN+FN_REFLEN+32], *pos, *end;
  const char *opt_delim;
  end= path_and_service + sizeof(path_and_service)-3;

  /* We have to quote filename if it contains spaces */
  pos= add_quoted_string(path_and_service, file_path, end);
  if (*extra_opt)
  {
    /* 
     Add option after file_path. There will be zero or one extra option.  It's 
     assumed to be --defaults-file=file but isn't checked.  The variable (not
     the option name) should be quoted if it contains a string.  
    */
    *pos++= ' ';
    if (opt_delim= strchr(extra_opt, '='))
    {
      size_t length= ++opt_delim - extra_opt;
      pos= strnmov(pos, extra_opt, length);
    }
    else
      opt_delim= extra_opt;
    
    pos= add_quoted_string(pos, opt_delim, end);
  }
  /* We must have servicename last */
  *pos++= ' ';
  (void) add_quoted_string(pos, servicename, end);

  if (Service.got_service_option(argv, "install"))
  {
    Service.Install(1, servicename, displayname, path_and_service,
                    account_name);
    return 0;
  }
  if (Service.got_service_option(argv, "install-manual"))
  {
    Service.Install(0, servicename, displayname, path_and_service,
                    account_name);
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
  /*
    When several instances are running on the same machine, we
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
				   file_path, "", NULL))
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
      if (!default_service_handling(argv, argv[2], argv[2], file_path, "",
                                    NULL))
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
    else if (argc == 4 || argc == 5)
    {
      /*
        This may seem strange, because we handle --local-service while
        preserving 4.1's behavior of allowing any one other argument that is
        passed to the service on startup. (The assumption is that this is
        --defaults-file=file, but that was not enforced in 4.1, so we don't
        enforce it here.)
      */
      const char *extra_opt= NullS;
      const char *account_name = NullS;
      int index;
      for (index = 3; index < argc; index++)
      {
        if (!strcmp(argv[index], "--local-service"))
          account_name= "NT AUTHORITY\\LocalService";
        else
          extra_opt= argv[index];
      }

      if (argc == 4 || account_name)
        if (!default_service_handling(argv, argv[2], argv[2], file_path,
                                      extra_opt, account_name))
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


/**
  Execute all commands from a file. Used by the mysql_install_db script to
  create MySQL privilege tables without having to start a full MySQL server.
*/

static void bootstrap(FILE *file)
{
  DBUG_ENTER("bootstrap");

  THD *thd= new THD;
  thd->bootstrap=1;
  my_net_init(&thd->net,(st_vio*) 0);
  thd->max_client_packet_length= thd->net.max_packet;
  thd->security_ctx->master_access= ~(ulong)0;
  thd->thread_id= thd->variables.pseudo_thread_id= thread_id++;
  thread_count++;
  in_bootstrap= TRUE;

  bootstrap_file=file;
#ifndef EMBEDDED_LIBRARY			// TODO:  Enable this
  if (pthread_create(&thd->real_id,&connection_attrib,handle_bootstrap,
		     (void*) thd))
  {
    sql_print_warning("Can't create thread to handle bootstrap");
    bootstrap_error=-1;
    DBUG_VOID_RETURN;
  }
  /* Wait for thread to die */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  while (in_bootstrap)
  {
    (void) pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
    DBUG_PRINT("quit",("One thread died (count=%u)",thread_count));
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);
#else
  thd->mysql= 0;
  handle_bootstrap((void *)thd);
#endif

  DBUG_VOID_RETURN;
}


static bool read_init_file(char *file_name)
{
  FILE *file;
  DBUG_ENTER("read_init_file");
  DBUG_PRINT("enter",("name: %s",file_name));
  if (!(file=my_fopen(file_name,O_RDONLY,MYF(MY_WME))))
    DBUG_RETURN(TRUE);
  bootstrap(file);
  (void) my_fclose(file,MYF(MY_WME));
  DBUG_RETURN(FALSE);
}


#ifndef EMBEDDED_LIBRARY

/*
   Simple scheduler that use the main thread to handle the request

   NOTES
     This is only used for debugging, when starting mysqld with
     --thread-handling=no-threads or --one-thread

     When we enter this function, LOCK_thread_count is hold!
*/
   
void handle_connection_in_main_thread(THD *thd)
{
  safe_mutex_assert_owner(&LOCK_thread_count);
  thread_cache_size=0;			// Safety
  threads.append(thd);
  pthread_mutex_unlock(&LOCK_thread_count);
  thd->start_utime= my_micro_time();
  handle_one_connection(thd);
}


/*
  Scheduler that uses one thread per connection
*/

void create_thread_to_handle_connection(THD *thd)
{
  if (cached_thread_count > wake_thread)
  {
    /* Get thread from cache */
    thread_cache.push_back(thd);
    wake_thread++;
    pthread_cond_signal(&COND_thread_cache);
  }
  else
  {
    char error_message_buff[MYSQL_ERRMSG_SIZE];
    /* Create new thread to handle connection */
    int error;
    thread_created++;
    threads.append(thd);
    DBUG_PRINT("info",(("creating thread %lu"), thd->thread_id));
    thd->prior_thr_create_utime= thd->start_utime= my_micro_time();
    if ((error=pthread_create(&thd->real_id,&connection_attrib,
                              handle_one_connection,
                              (void*) thd)))
    {
      /* purecov: begin inspected */
      DBUG_PRINT("error",
                 ("Can't create thread to handle request (error %d)",
                  error));
      thread_count--;
      thd->killed= THD::KILL_CONNECTION;			// Safety
      (void) pthread_mutex_unlock(&LOCK_thread_count);

      pthread_mutex_lock(&LOCK_connection_count);
      --connection_count;
      pthread_mutex_unlock(&LOCK_connection_count);

      statistic_increment(aborted_connects,&LOCK_status);
      /* Can't use my_error() since store_globals has not been called. */
      my_snprintf(error_message_buff, sizeof(error_message_buff),
                  ER(ER_CANT_CREATE_THREAD), error);
      net_send_error(thd, ER_CANT_CREATE_THREAD, error_message_buff);
      (void) pthread_mutex_lock(&LOCK_thread_count);
      close_connection(thd,0,0);
      delete thd;
      (void) pthread_mutex_unlock(&LOCK_thread_count);
      return;
      /* purecov: end */
    }
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_PRINT("info",("Thread created"));
}


/**
  Create new thread to handle incoming connection.

    This function will create new thread to handle the incoming
    connection.  If there are idle cached threads one will be used.
    'thd' will be pushed into 'threads'.

    In single-threaded mode (\#define ONE_THREAD) connection will be
    handled inside this function.

  @param[in,out] thd    Thread handle of future thread.
*/

static void create_new_thread(THD *thd)
{
  NET *net=&thd->net;
  DBUG_ENTER("create_new_thread");

  if (protocol_version > 9)
    net->return_errno=1;

  /*
    Don't allow too many connections. We roughly check here that we allow
    only (max_connections + 1) connections.
  */

  pthread_mutex_lock(&LOCK_connection_count);

  if (connection_count >= max_connections + 1 || abort_loop)
  {
    pthread_mutex_unlock(&LOCK_connection_count);

    DBUG_PRINT("error",("Too many connections"));
    close_connection(thd, ER_CON_COUNT_ERROR, 1);
    delete thd;
    DBUG_VOID_RETURN;
  }

  ++connection_count;

  if (connection_count > max_used_connections)
    max_used_connections= connection_count;

  pthread_mutex_unlock(&LOCK_connection_count);

  /* Start a new thread to handle connection. */

  pthread_mutex_lock(&LOCK_thread_count);

  /*
    The initialization of thread_id is done in create_embedded_thd() for
    the embedded library.
    TODO: refactor this to avoid code duplication there
  */
  thd->thread_id= thd->variables.pseudo_thread_id= thread_id++;

  thread_count++;

  thread_scheduler.add_connection(thd);

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
    /* The following call will never return */
    kill_server(IF_NETWARE(MYSQL_KILL_SIGNAL, (void*) MYSQL_KILL_SIGNAL));
  }
}
#define MAYBE_BROKEN_SYSCALL kill_broken_server();
#else
#define MAYBE_BROKEN_SYSCALL
#endif

	/* Handle new connections and spawn new process to handle them */

#ifndef EMBEDDED_LIBRARY
pthread_handler_t handle_connections_sockets(void *arg __attribute__((unused)))
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

	  (void) mysql_socket_shutdown(new_sock, SHUT_RDWR);
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
	(void) mysql_socket_shutdown(new_sock, SHUT_RDWR);
	(void) closesocket(new_sock);
	continue;
      }
    }

    /*
    ** Don't allow too many connections
    */

    if (!(thd= new THD))
    {
      (void) mysql_socket_shutdown(new_sock, SHUT_RDWR);
      VOID(closesocket(new_sock));
      continue;
    }
    if (!(vio_tmp=vio_new(new_sock,
			  sock == unix_sock ? VIO_TYPE_SOCKET :
			  VIO_TYPE_TCPIP,
			  sock == unix_sock ? VIO_LOCALHOST: 0)) ||
	my_net_init(&thd->net,vio_tmp))
    {
      /*
        Only delete the temporary vio if we didn't already attach it to the
        NET object. The destructor in THD will delete any initialized net
        structure.
      */
      if (vio_tmp && thd->net.vio != vio_tmp)
        vio_delete(vio_tmp);
      else
      {
	(void) mysql_socket_shutdown(new_sock, SHUT_RDWR);
	(void) closesocket(new_sock);
      }
      delete thd;
      continue;
    }
    if (sock == unix_sock)
      thd->security_ctx->host=(char*) my_localhost;

    create_new_thread(thd);
  }
  DBUG_LEAVE;
  decrement_handler_count();
  return 0;
}


#ifdef __NT__
pthread_handler_t handle_connections_namedpipes(void *arg)
{
  HANDLE hConnectedPipe;
  OVERLAPPED connectOverlapped= {0};
  THD *thd;
  my_thread_init();
  DBUG_ENTER("handle_connections_namedpipes");
  connectOverlapped.hEvent= CreateEvent(NULL, TRUE, FALSE, NULL);
  if (!connectOverlapped.hEvent)
  {
    sql_print_error("Can't create event, last error=%u", GetLastError());
    unireg_abort(1);
  }
  DBUG_PRINT("general",("Waiting for named pipe connections."));
  while (!abort_loop)
  {
    /* wait for named pipe connection */
    BOOL fConnected= ConnectNamedPipe(hPipe, &connectOverlapped);
    if (!fConnected && (GetLastError() == ERROR_IO_PENDING))
    {
        /*
          ERROR_IO_PENDING says async IO has started but not yet finished.
          GetOverlappedResult will wait for completion.
        */
        DWORD bytes;
        fConnected= GetOverlappedResult(hPipe, &connectOverlapped,&bytes, TRUE);
    }
    if (abort_loop)
      break;
    if (!fConnected)
      fConnected = GetLastError() == ERROR_PIPE_CONNECTED;
    if (!fConnected)
    {
      CloseHandle(hPipe);
      if ((hPipe= CreateNamedPipe(pipe_name,
                                  PIPE_ACCESS_DUPLEX |
                                  FILE_FLAG_OVERLAPPED,
                                  PIPE_TYPE_BYTE |
                                  PIPE_READMODE_BYTE |
                                  PIPE_WAIT,
                                  PIPE_UNLIMITED_INSTANCES,
                                  (int) global_system_variables.
                                  net_buffer_length,
                                  (int) global_system_variables.
                                  net_buffer_length,
                                  NMPWAIT_USE_DEFAULT_WAIT,
                                  &saPipeSecurity)) ==
	  INVALID_HANDLE_VALUE)
      {
	sql_perror("Can't create new named pipe!");
	break;					// Abort
      }
    }
    hConnectedPipe = hPipe;
    /* create new pipe for new connection */
    if ((hPipe = CreateNamedPipe(pipe_name,
                 PIPE_ACCESS_DUPLEX |
                 FILE_FLAG_OVERLAPPED,
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
      DisconnectNamedPipe(hConnectedPipe);
      CloseHandle(hConnectedPipe);
      continue;
    }
    if (!(thd->net.vio= vio_new_win32pipe(hConnectedPipe)) ||
	my_net_init(&thd->net, thd->net.vio))
    {
      close_connection(thd, ER_OUT_OF_RESOURCES, 1);
      delete thd;
      continue;
    }
    /* Host is unknown */
    thd->security_ctx->host= my_strdup(my_localhost, MYF(0));
    create_new_thread(thd);
  }
  CloseHandle(connectOverlapped.hEvent);
  DBUG_LEAVE;
  decrement_handler_count();
  return 0;
}
#endif /* __NT__ */


#ifdef HAVE_SMEM

/**
  Thread of shared memory's service.

  @param arg                              Arguments of thread
*/
pthread_handler_t handle_connections_shared_memory(void *arg)
{
  /* file-mapping object, use for create shared memory */
  HANDLE handle_connect_file_map= 0;
  char  *handle_connect_map= 0;                 // pointer on shared memory
  HANDLE event_connect_answer= 0;
  ulong smem_buffer_length= shared_memory_buffer_length + 4;
  ulong connect_number= 1;
  char *tmp= NULL;
  char *suffix_pos;
  char connect_number_char[22], *p;
  const char *errmsg= 0;
  SECURITY_ATTRIBUTES *sa_event= 0, *sa_mapping= 0;
  my_thread_init();
  DBUG_ENTER("handle_connections_shared_memorys");
  DBUG_PRINT("general",("Waiting for allocated shared memory."));

  /*
     get enough space base-name + '_' + longest suffix we might ever send
   */
  if (!(tmp= (char *)my_malloc(strlen(shared_memory_base_name) + 32L, MYF(MY_FAE))))
    goto error;

  if (my_security_attr_create(&sa_event, &errmsg,
                              GENERIC_ALL, SYNCHRONIZE | EVENT_MODIFY_STATE))
    goto error;

  if (my_security_attr_create(&sa_mapping, &errmsg,
                             GENERIC_ALL, FILE_MAP_READ | FILE_MAP_WRITE))
    goto error;

  /*
    The name of event and file-mapping events create agree next rule:
      shared_memory_base_name+unique_part
    Where:
      shared_memory_base_name is unique value for each server
      unique_part is unique value for each object (events and file-mapping)
  */
  suffix_pos= strxmov(tmp,shared_memory_base_name,"_",NullS);
  strmov(suffix_pos, "CONNECT_REQUEST");
  if ((smem_event_connect_request= CreateEvent(sa_event,
                                               FALSE, FALSE, tmp)) == 0)
  {
    errmsg= "Could not create request event";
    goto error;
  }
  strmov(suffix_pos, "CONNECT_ANSWER");
  if ((event_connect_answer= CreateEvent(sa_event, FALSE, FALSE, tmp)) == 0)
  {
    errmsg="Could not create answer event";
    goto error;
  }
  strmov(suffix_pos, "CONNECT_DATA");
  if ((handle_connect_file_map=
       CreateFileMapping(INVALID_HANDLE_VALUE, sa_mapping,
                         PAGE_READWRITE, 0, sizeof(connect_number), tmp)) == 0)
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
    HANDLE event_conn_closed= 0;
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
    if ((handle_client_file_map=
         CreateFileMapping(INVALID_HANDLE_VALUE, sa_mapping,
                           PAGE_READWRITE, 0, smem_buffer_length, tmp)) == 0)
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
    if ((event_client_wrote= CreateEvent(sa_event, FALSE, FALSE, tmp)) == 0)
    {
      errmsg= "Could not create client write event";
      goto errorconn;
    }
    strmov(suffix_pos, "CLIENT_READ");
    if ((event_client_read= CreateEvent(sa_event, FALSE, FALSE, tmp)) == 0)
    {
      errmsg= "Could not create client read event";
      goto errorconn;
    }
    strmov(suffix_pos, "SERVER_READ");
    if ((event_server_read= CreateEvent(sa_event, FALSE, FALSE, tmp)) == 0)
    {
      errmsg= "Could not create server read event";
      goto errorconn;
    }
    strmov(suffix_pos, "SERVER_WROTE");
    if ((event_server_wrote= CreateEvent(sa_event,
                                         FALSE, FALSE, tmp)) == 0)
    {
      errmsg= "Could not create server write event";
      goto errorconn;
    }
    strmov(suffix_pos, "CONNECTION_CLOSED");
    if ((event_conn_closed= CreateEvent(sa_event,
                                        TRUE, FALSE, tmp)) == 0)
    {
      errmsg= "Could not create closed connection event";
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
    if (!(thd->net.vio= vio_new_win32shared_memory(handle_client_file_map,
                                                   handle_client_map,
                                                   event_client_wrote,
                                                   event_client_read,
                                                   event_server_wrote,
                                                   event_server_read,
                                                   event_conn_closed)) ||
                        my_net_init(&thd->net, thd->net.vio))
    {
      close_connection(thd, ER_OUT_OF_RESOURCES, 1);
      errmsg= 0;
      goto errorconn;
    }
    thd->security_ctx->host= my_strdup(my_localhost, MYF(0)); /* Host is unknown */
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
    if (handle_client_file_map) 
      CloseHandle(handle_client_file_map);
    if (handle_client_map)
      UnmapViewOfFile(handle_client_map);
    if (event_server_wrote)
      CloseHandle(event_server_wrote);
    if (event_server_read)
      CloseHandle(event_server_read);
    if (event_client_wrote)
      CloseHandle(event_client_wrote);
    if (event_client_read)
      CloseHandle(event_client_read);
    if (event_conn_closed)
      CloseHandle(event_conn_closed);
    delete thd;
  }

  /* End shared memory handling */
error:
  if (tmp)
    my_free(tmp, MYF(0));

  if (errmsg)
  {
    char buff[180];
    strxmov(buff, "Can't create shared memory service: ", errmsg, ".", NullS);
    sql_perror(buff);
  }
  my_security_attr_free(sa_event);
  my_security_attr_free(sa_mapping);
  if (handle_connect_map)	UnmapViewOfFile(handle_connect_map);
  if (handle_connect_file_map)	CloseHandle(handle_connect_file_map);
  if (event_connect_answer)	CloseHandle(event_connect_answer);
  if (smem_event_connect_request) CloseHandle(smem_event_connect_request);
  DBUG_LEAVE;
  decrement_handler_count();
  return 0;
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
  OPT_MASTER_HOST,             OPT_MASTER_USER,
  OPT_MASTER_PASSWORD,         OPT_MASTER_PORT,
  OPT_MASTER_INFO_FILE,        OPT_MASTER_CONNECT_RETRY,
  OPT_MASTER_RETRY_COUNT,      OPT_LOG_TC, OPT_LOG_TC_SIZE,
  OPT_MASTER_SSL,              OPT_MASTER_SSL_KEY,
  OPT_MASTER_SSL_CERT,         OPT_MASTER_SSL_CAPATH,
  OPT_MASTER_SSL_CIPHER,       OPT_MASTER_SSL_CA,
  OPT_SQL_BIN_UPDATE_SAME,     OPT_REPLICATE_DO_DB,
  OPT_REPLICATE_IGNORE_DB,     OPT_LOG_SLAVE_UPDATES,
  OPT_BINLOG_DO_DB,            OPT_BINLOG_IGNORE_DB,
  OPT_BINLOG_FORMAT,
#ifndef DBUG_OFF
  OPT_BINLOG_SHOW_XID,
#endif
  OPT_BINLOG_ROWS_EVENT_MAX_SIZE, 
  OPT_WANT_CORE,               OPT_CONCURRENT_INSERT,
  OPT_MEMLOCK,                 OPT_MYISAM_RECOVER,
  OPT_REPLICATE_REWRITE_DB,    OPT_SERVER_ID,
  OPT_SKIP_SLAVE_START,        OPT_SAFE_SHOW_DB, 
  OPT_SAFEMALLOC_MEM_LIMIT,    OPT_REPLICATE_DO_TABLE,
  OPT_REPLICATE_IGNORE_TABLE,  OPT_REPLICATE_WILD_DO_TABLE,
  OPT_REPLICATE_WILD_IGNORE_TABLE, OPT_REPLICATE_SAME_SERVER_ID,
  OPT_DISCONNECT_SLAVE_EVENT_COUNT, OPT_TC_HEURISTIC_RECOVER,
  OPT_ABORT_SLAVE_EVENT_COUNT,
  OPT_LOG_BIN_TRUST_FUNCTION_CREATORS,
  OPT_LOG_BIN_TRUST_FUNCTION_CREATORS_OLD,
  OPT_ENGINE_CONDITION_PUSHDOWN, OPT_NDB_CONNECTSTRING, 
  OPT_NDB_USE_EXACT_COUNT, OPT_NDB_USE_TRANSACTIONS,
  OPT_NDB_FORCE_SEND, OPT_NDB_AUTOINCREMENT_PREFETCH_SZ,
  OPT_NDB_SHM, OPT_NDB_OPTIMIZED_NODE_SELECTION, OPT_NDB_CACHE_CHECK_TIME,
  OPT_NDB_MGMD, OPT_NDB_NODEID,
  OPT_NDB_DISTRIBUTION,
  OPT_NDB_INDEX_STAT_ENABLE,
  OPT_NDB_EXTRA_LOGGING,
  OPT_NDB_REPORT_THRESH_BINLOG_EPOCH_SLIP,
  OPT_NDB_REPORT_THRESH_BINLOG_MEM_USAGE,
  OPT_NDB_USE_COPYING_ALTER_TABLE,
  OPT_SKIP_SAFEMALLOC,
  OPT_TEMP_POOL, OPT_TX_ISOLATION, OPT_COMPLETION_TYPE,
  OPT_SKIP_STACK_TRACE, OPT_SKIP_SYMLINKS,
  OPT_MAX_BINLOG_DUMP_EVENTS, OPT_SPORADIC_BINLOG_DUMP_FAIL,
  OPT_SAFE_USER_CREATE, OPT_SQL_MODE,
  OPT_HAVE_NAMED_PIPE,
  OPT_DO_PSTACK, OPT_EVENT_SCHEDULER, OPT_REPORT_HOST,
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
  OPT_SLAVE_MAX_ALLOWED_PACKET,
  OPT_MAX_BINLOG_CACHE_SIZE, OPT_MAX_BINLOG_SIZE,
  OPT_MAX_CONNECTIONS, OPT_MAX_CONNECT_ERRORS,
  OPT_MAX_DELAYED_THREADS, OPT_MAX_HEP_TABLE_SIZE,
  OPT_MAX_JOIN_SIZE, OPT_MAX_PREPARED_STMT_COUNT,
  OPT_MAX_RELAY_LOG_SIZE, OPT_MAX_SORT_LENGTH,
  OPT_MAX_SEEKS_FOR_KEY, OPT_MAX_TMP_TABLES, OPT_MAX_USER_CONNECTIONS,
  OPT_MAX_LENGTH_FOR_SORT_DATA,
  OPT_MAX_WRITE_LOCK_COUNT, OPT_BULK_INSERT_BUFFER_SIZE,
  OPT_MAX_ERROR_COUNT, OPT_MULTI_RANGE_COUNT, OPT_MYISAM_DATA_POINTER_SIZE,
  OPT_MYISAM_BLOCK_SIZE, OPT_MYISAM_MAX_EXTRA_SORT_FILE_SIZE,
  OPT_MYISAM_MAX_SORT_FILE_SIZE, OPT_MYISAM_SORT_BUFFER_SIZE,
  OPT_MYISAM_USE_MMAP, OPT_MYISAM_REPAIR_THREADS,
  OPT_MYISAM_MMAP_SIZE,
  OPT_MYISAM_STATS_METHOD,
  OPT_NET_BUFFER_LENGTH, OPT_NET_RETRY_COUNT,
  OPT_NET_READ_TIMEOUT, OPT_NET_WRITE_TIMEOUT,
  OPT_OPEN_FILES_LIMIT,
  OPT_PRELOAD_BUFFER_SIZE,
  OPT_QUERY_CACHE_LIMIT, OPT_QUERY_CACHE_MIN_RES_UNIT, OPT_QUERY_CACHE_SIZE,
  OPT_QUERY_CACHE_TYPE, OPT_QUERY_CACHE_WLOCK_INVALIDATE, OPT_RECORD_BUFFER,
  OPT_RECORD_RND_BUFFER, OPT_DIV_PRECINCREMENT, OPT_RELAY_LOG_SPACE_LIMIT,
  OPT_RELAY_LOG_PURGE,
  OPT_SLAVE_NET_TIMEOUT, OPT_SLAVE_COMPRESSED_PROTOCOL, OPT_SLOW_LAUNCH_TIME,
  OPT_SLAVE_TRANS_RETRIES, OPT_READONLY, OPT_DEBUGGING,
  OPT_SORT_BUFFER, OPT_TABLE_OPEN_CACHE, OPT_TABLE_DEF_CACHE,
  OPT_THREAD_CONCURRENCY, OPT_THREAD_CACHE_SIZE,
  OPT_TMP_TABLE_SIZE, OPT_THREAD_STACK,
  OPT_WAIT_TIMEOUT,
  OPT_ERROR_LOG_FILE,
  OPT_DEFAULT_WEEK_FORMAT,
  OPT_RANGE_ALLOC_BLOCK_SIZE, OPT_ALLOW_SUSPICIOUS_UDFS,
  OPT_QUERY_ALLOC_BLOCK_SIZE, OPT_QUERY_PREALLOC_SIZE,
  OPT_TRANS_ALLOC_BLOCK_SIZE, OPT_TRANS_PREALLOC_SIZE,
  OPT_SYNC_FRM, OPT_SYNC_BINLOG,
  OPT_SYNC_REPLICATION,
  OPT_SYNC_REPLICATION_SLAVE_ID,
  OPT_SYNC_REPLICATION_TIMEOUT,
  OPT_ENABLE_SHARED_MEMORY,
  OPT_SHARED_MEMORY_BASE_NAME,
  OPT_OLD_PASSWORDS,
  OPT_OLD_ALTER_TABLE,
  OPT_EXPIRE_LOGS_DAYS,
  OPT_GROUP_CONCAT_MAX_LEN,
  OPT_DEFAULT_COLLATION,
  OPT_DEFAULT_COLLATION_OLD,
  OPT_CHARACTER_SET_CLIENT_HANDSHAKE,
  OPT_CHARACTER_SET_FILESYSTEM,
  OPT_LC_TIME_NAMES,
  OPT_INIT_CONNECT,
  OPT_INIT_SLAVE,
  OPT_SECURE_AUTH,
  OPT_DATE_FORMAT,
  OPT_TIME_FORMAT,
  OPT_DATETIME_FORMAT,
  OPT_LOG_QUERIES_NOT_USING_INDEXES,
  OPT_DEFAULT_TIME_ZONE,
  OPT_SYSDATE_IS_NOW,
  OPT_OPTIMIZER_SEARCH_DEPTH,
  OPT_OPTIMIZER_PRUNE_LEVEL,
  OPT_OPTIMIZER_SWITCH,
  OPT_UPDATABLE_VIEWS_WITH_LIMIT,
  OPT_SP_AUTOMATIC_PRIVILEGES,
  OPT_MAX_SP_RECURSION_DEPTH,
  OPT_AUTO_INCREMENT, OPT_AUTO_INCREMENT_OFFSET,
  OPT_ENABLE_LARGE_PAGES,
  OPT_TIMED_MUTEXES,
  OPT_OLD_STYLE_USER_LIMITS,
  OPT_LOG_SLOW_ADMIN_STATEMENTS,
  OPT_TABLE_LOCK_WAIT_TIMEOUT,
  OPT_PLUGIN_LOAD,
  OPT_PLUGIN_DIR,
  OPT_SYMBOLIC_LINKS,
  OPT_WARNINGS,
  OPT_RECORD_BUFFER_OLD,
  OPT_LOG_OUTPUT,
  OPT_PORT_OPEN_TIMEOUT,
  OPT_PROFILING,
  OPT_KEEP_FILES_ON_CREATE,
  OPT_GENERAL_LOG,
  OPT_SLOW_LOG,
  OPT_THREAD_HANDLING,
  OPT_INNODB_ROLLBACK_ON_TIMEOUT,
  OPT_SECURE_FILE_PRIV,
  OPT_MIN_EXAMINED_ROW_LIMIT,
  OPT_LOG_SLOW_SLAVE_STATEMENTS,
#if defined(ENABLED_DEBUG_SYNC)
  OPT_DEBUG_SYNC_TIMEOUT,
#endif /* defined(ENABLED_DEBUG_SYNC) */
  OPT_OLD_MODE,
  OPT_SLAVE_EXEC_MODE,
  OPT_GENERAL_LOG_FILE,
  OPT_SLOW_QUERY_LOG_FILE,
  OPT_IGNORE_BUILTIN_INNODB,
  OPT_BINLOG_DIRECT_NON_TRANS_UPDATE,
  OPT_DEFAULT_CHARACTER_SET_OLD,
  OPT_MAX_LONG_DATA_SIZE
};


#define LONG_TIMEOUT ((ulong) 3600L*24L*365L)

struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit.", 
   &opt_help, &opt_help, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
#ifdef HAVE_REPLICATION
  {"abort-slave-event-count", OPT_ABORT_SLAVE_EVENT_COUNT,
   "Option used by mysql-test for debugging and testing of replication.",
   &abort_slave_event_count,  &abort_slave_event_count,
   0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif /* HAVE_REPLICATION */
  {"allow-suspicious-udfs", OPT_ALLOW_SUSPICIOUS_UDFS,
   "Allows use of UDFs consisting of only one symbol xxx() "
   "without corresponding xxx_init() or xxx_deinit(). That also means "
   "that one can load any function from any library, for example exit() "
   "from libc.so",
   &opt_allow_suspicious_udfs, &opt_allow_suspicious_udfs,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"ansi", 'a', "Use ANSI SQL syntax instead of MySQL syntax. This mode "
   "will also set transaction isolation level 'serializable'.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"auto-increment-increment", OPT_AUTO_INCREMENT,
   "Auto-increment columns are incremented by this.",
   &global_system_variables.auto_increment_increment,
   &max_system_variables.auto_increment_increment, 0, GET_ULONG,
   OPT_ARG, 1, 1, 65535, 0, 1, 0 },
  {"auto-increment-offset", OPT_AUTO_INCREMENT_OFFSET,
   "Offset added to Auto-increment columns. Used when auto-increment-increment != 1.",
   &global_system_variables.auto_increment_offset,
   &max_system_variables.auto_increment_offset, 0, GET_ULONG, OPT_ARG,
   1, 1, 65535, 0, 1, 0 },
  {"automatic-sp-privileges", OPT_SP_AUTOMATIC_PRIVILEGES,
   "Creating and dropping stored procedures alters ACLs. Disable with --skip-automatic-sp-privileges.",
   &sp_automatic_privileges, &sp_automatic_privileges,
   0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"basedir", 'b',
   "Path to installation directory. All paths are usually resolved relative to this.",
   &mysql_home_ptr, &mysql_home_ptr, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"big-tables", OPT_BIG_TABLES,
   "Allow big result sets by saving all temporary sets on file (solves most 'table full' errors).",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"bind-address", OPT_BIND_ADDRESS, "IP address to bind to.",
   &my_bind_addr_str, &my_bind_addr_str, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog_format", OPT_BINLOG_FORMAT,
   "Does not have any effect without '--log-bin'. "
   "Tell the master the form of binary logging to use: either 'row' for "
   "row-based binary logging, 'statement' for statement-based binary "
   "logging, or 'mixed'. 'mixed' is statement-based binary logging except "
   "for statements where only row-based is correct: Statements that involve "
   "user-defined functions (i.e., UDFs) or the UUID() function."
#ifdef HAVE_NDB_BINLOG
   "If ndbcluster is enabled and binlog_format is `mixed', the format switches"
   " to 'row' and back implicitly per each query accessing a NDB table."
#endif
   , &opt_binlog_format, &opt_binlog_format,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog-do-db", OPT_BINLOG_DO_DB,
   "Tells the master it should log updates for the specified database, "
   "and exclude all others not explicitly mentioned.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog-ignore-db", OPT_BINLOG_IGNORE_DB,
   "Tells the master that updates to the given database should not be logged to the binary log.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog-row-event-max-size", OPT_BINLOG_ROWS_EVENT_MAX_SIZE,
   "The maximum size of a row-based binary log event in bytes. Rows will be "
   "grouped into events smaller than this size if possible. "
   "The value has to be a multiple of 256.",
   &opt_binlog_rows_event_max_size, &opt_binlog_rows_event_max_size,
   0, GET_ULONG, REQUIRED_ARG,
   /* def_value */ 1024, /* min_value */  256, /* max_value */ ULONG_MAX, 
   /* sub_size */     0, /* block_size */ 256, 
   /* app_type */ 0
  },
#ifndef DISABLE_GRANT_OPTIONS
  {"bootstrap", OPT_BOOTSTRAP, "Used by mysql installation scripts.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"character-set-client-handshake", OPT_CHARACTER_SET_CLIENT_HANDSHAKE,
   "Don't ignore client side character set value sent during handshake.",
   &opt_character_set_client_handshake,
   &opt_character_set_client_handshake,
    0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"character-set-filesystem", OPT_CHARACTER_SET_FILESYSTEM,
   "Set the filesystem character set.",
   &character_set_filesystem_name,
   &character_set_filesystem_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"character-set-server", 'C', "Set the default character set.",
   &default_character_set_name, &default_character_set_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are.", &charsets_dir,
   &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"chroot", 'r', "Chroot mysqld daemon during startup.",
   &mysqld_chroot, &mysqld_chroot, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"collation-server", OPT_DEFAULT_COLLATION, "Set the default collation.",
   &default_collation_name, &default_collation_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"completion-type", OPT_COMPLETION_TYPE, "Default completion type.",
   &global_system_variables.completion_type,
   &max_system_variables.completion_type, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, 2, 0, 1, 0},
  {"concurrent-insert", OPT_CONCURRENT_INSERT,
   "Use concurrent insert with MyISAM. Disable with --concurrent-insert=0.",
   &myisam_concurrent_insert, &myisam_concurrent_insert,
   0, GET_ULONG, OPT_ARG, 1, 0, 2, 0, 0, 0},
  {"console", OPT_CONSOLE, "Write error output on screen; don't remove the console window on windows.",
   &opt_console, &opt_console, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"core-file", OPT_WANT_CORE, "Write core on errors.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 'h', "Path to the database root.", &mysql_data_home,
   &mysql_data_home, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
  {"debug", '#', "Debug log.", &default_dbug_option,
   &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"default-character-set", OPT_DEFAULT_CHARACTER_SET_OLD, 
   "Set the default character set (deprecated option, use --character-set-server instead).",
   &default_character_set_name, &default_character_set_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"default-collation", OPT_DEFAULT_COLLATION_OLD, "Set the default collation "
   "(deprecated option, use --collation-server instead).",
   &default_collation_name, &default_collation_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"default-storage-engine", OPT_STORAGE_ENGINE,
   "Set the default storage engine (table type) for tables.",
   &default_storage_engine_str, &default_storage_engine_str,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-table-type", OPT_STORAGE_ENGINE,
   "(deprecated) Use --default-storage-engine.",
   &default_storage_engine_str, &default_storage_engine_str,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-time-zone", OPT_DEFAULT_TIME_ZONE, "Set the default time zone.",
   &default_tz_name, &default_tz_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"delay-key-write", OPT_DELAY_KEY_WRITE, "Type of DELAY_KEY_WRITE.",
   0,0,0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"delay-key-write-for-all-tables", OPT_DELAY_KEY_WRITE_ALL,
   "Don't flush key buffers between writes for any MyISAM table. "
   "(Deprecated option, use --delay-key-write=all instead.)",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_OPENSSL
  {"des-key-file", OPT_DES_KEY_FILE,
   "Load keys for des_encrypt() and des_encrypt from given file.",
   &des_key_file, &des_key_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
#endif /* HAVE_OPENSSL */
#ifdef HAVE_REPLICATION
  {"disconnect-slave-event-count", OPT_DISCONNECT_SLAVE_EVENT_COUNT,
   "Option used by mysql-test for debugging and testing of replication.",
   &disconnect_slave_event_count, &disconnect_slave_event_count,
   0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif /* HAVE_REPLICATION */
  {"enable-locking", OPT_ENABLE_LOCK,
   "Deprecated option, use --external-locking instead.",
   &opt_external_locking, &opt_external_locking,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __NT__
  {"enable-named-pipe", OPT_HAVE_NAMED_PIPE, "Enable the named pipe (NT).",
   &opt_enable_named_pipe, &opt_enable_named_pipe, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
#ifdef HAVE_STACK_TRACE_ON_SEGV
  {"enable-pstack", OPT_DO_PSTACK, "Print a symbolic stack trace on failure. "
   "This option is deprecated and has no effect; a symbolic stack trace will "
   "be printed after a crash whenever possible.", &opt_do_pstack, &opt_do_pstack,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif /* HAVE_STACK_TRACE_ON_SEGV */
  {"engine-condition-pushdown",
   OPT_ENGINE_CONDITION_PUSHDOWN,
   "Push supported query conditions to the storage engine.",
   &global_system_variables.engine_condition_pushdown,
   &global_system_variables.engine_condition_pushdown,
   0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  /* See how it's handled in get_one_option() */
  {"event-scheduler", OPT_EVENT_SCHEDULER, "Enable/disable the event scheduler.",
   NULL,  NULL, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"exit-info", 'T', "Used for debugging. Use at your own risk.", 0, 0, 0,
   GET_LONG, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"external-locking", OPT_USE_LOCKING, "Use system (external) locking "
   "(disabled by default).  With this option enabled you can run myisamchk "
   "to test (not repair) tables while the MySQL server is running. "
   "Disable with --skip-external-locking.",
   &opt_external_locking, &opt_external_locking,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"flush", OPT_FLUSH, "Flush tables to disk between SQL commands.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  /* We must always support the next option to make scripts like mysqltest
     easier to do */
  {"gdb", OPT_DEBUGGING,
   "Set up signals usable for debugging.",
   &opt_debugging, &opt_debugging,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"general_log", OPT_GENERAL_LOG,
   "Enable/disable general log.", &opt_log,
   &opt_log, 0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_LARGE_PAGES
  {"large-pages", OPT_ENABLE_LARGE_PAGES, "Enable support for large pages. "
   "Disable with --skip-large-pages.", &opt_large_pages, &opt_large_pages,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"ignore-builtin-innodb", OPT_IGNORE_BUILTIN_INNODB ,
   "Disable initialization of builtin InnoDB plugin.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"init-connect", OPT_INIT_CONNECT, 
   "Command(s) that are executed for each new connection.",
   &opt_init_connect, &opt_init_connect, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DISABLE_GRANT_OPTIONS
  {"init-file", OPT_INIT_FILE, "Read SQL commands from this file at startup.",
   &opt_init_file, &opt_init_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
#endif
  {"init-rpl-role", OPT_INIT_RPL_ROLE, "Set the replication role.", 0, 0, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"init-slave", OPT_INIT_SLAVE, "Command(s) that are executed by a slave server \
each time the SQL thread starts.",
   &opt_init_slave, &opt_init_slave, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"language", 'L',
   "Client error messages in given language. May be given as a full path.",
   &language_ptr, &language_ptr, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"lc-time-names", OPT_LC_TIME_NAMES,
   "Set the language used for the month names and the days of the week.",
   &lc_time_names_name, &lc_time_names_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"local-infile", OPT_LOCAL_INFILE,
   "Enable/disable LOAD DATA LOCAL INFILE (takes values 1 or 0).",
   &opt_local_infile, &opt_local_infile, 0, GET_BOOL, OPT_ARG,
   1, 0, 0, 0, 0, 0},
  {"log", 'l', "Log connections and queries to file (deprecated option, use "
   "--general_log/--general_log_file instead).", &opt_logname,
   &opt_logname, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"general_log_file", OPT_GENERAL_LOG_FILE,
   "Log connections and queries to given file.", &opt_logname,
   &opt_logname, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"log-bin", OPT_BIN_LOG,
   "Log update queries in binary format. Optional (but strongly recommended "
   "to avoid replication problems if server's hostname changes) argument "
   "should be the chosen location for the binary log files.",
   &opt_bin_logname, &opt_bin_logname, 0, GET_STR_ALLOC,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-bin-index", OPT_BIN_LOG_INDEX,
   "File that holds the names for last binary log files.",
   &opt_binlog_index_name, &opt_binlog_index_name, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifndef TO_BE_REMOVED_IN_5_1_OR_6_0
  /*
    In 5.0.6 we introduced the below option, then in 5.0.16 we renamed it to
    log-bin-trust-function-creators but kept also the old name for
    compatibility; the behaviour was also changed to apply only to functions
    (and triggers). In a future release this old name could be removed.
  */
  {"log-bin-trust-routine-creators", OPT_LOG_BIN_TRUST_FUNCTION_CREATORS_OLD,
   "(deprecated) Use log-bin-trust-function-creators.",
   &trust_function_creators, &trust_function_creators, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  /*
    This option starts with "log-bin" to emphasize that it is specific of
    binary logging.
  */
  {"log-bin-trust-function-creators", OPT_LOG_BIN_TRUST_FUNCTION_CREATORS,
   "If equal to 0 (the default), then when --log-bin is used, creation of "
   "a stored function (or trigger) is allowed only to users having the SUPER "
   "privilege, and only if this stored function (trigger) may not break "
   "binary logging."
   "Note that if ALL connections to this server ALWAYS use row-based binary "
   "logging, the security issues do not exist and the binary logging cannot "
   "break, so you can safely set this to 1."
   ,&trust_function_creators, &trust_function_creators, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-error", OPT_ERROR_LOG_FILE, "Error log file.",
   &log_error_file_ptr, &log_error_file_ptr, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-isam", OPT_ISAM_LOG, "Log all MyISAM changes to file.",
   &myisam_log_filename, &myisam_log_filename, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-long-format", '0',
   "Log some extra information to update log. Please note that this option "
   "is deprecated; see --log-short-format option.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef WITH_CSV_STORAGE_ENGINE
  {"log-output", OPT_LOG_OUTPUT,
   "Syntax: log-output[=value[,value...]], where \"value\" could be TABLE, "
   "FILE or NONE.",
   &log_output_str, &log_output_str, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"log-queries-not-using-indexes", OPT_LOG_QUERIES_NOT_USING_INDEXES,
   "Log queries that are executed without benefit of any index to the slow log if it is open.",
   &opt_log_queries_not_using_indexes, &opt_log_queries_not_using_indexes,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-short-format", OPT_SHORT_LOG_FORMAT,
   "Don't log extra information to update and slow-query logs.",
   &opt_short_log_format, &opt_short_log_format,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-slave-updates", OPT_LOG_SLAVE_UPDATES,
   "Tells the slave to log the updates from the slave thread to the binary log. "
   "You will need to turn it on if you plan to daisy-chain the slaves.",
   &opt_log_slave_updates, &opt_log_slave_updates, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-slow-admin-statements", OPT_LOG_SLOW_ADMIN_STATEMENTS,
   "Log slow OPTIMIZE, ANALYZE, ALTER and other administrative statements "
   "to the slow log if it is open.", &opt_log_slow_admin_statements,
   &opt_log_slow_admin_statements, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
 {"log-slow-slave-statements", OPT_LOG_SLOW_SLAVE_STATEMENTS,
  "Log slow statements executed by slave thread to the slow log if it is open.",
  &opt_log_slow_slave_statements,
  &opt_log_slow_slave_statements,
  0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log_slow_queries", OPT_SLOW_QUERY_LOG,
    "Log slow queries to a table or log file. Defaults logging to table "
    "mysql.slow_log or hostname-slow.log if --log-output=file is used. "
    "Must be enabled to activate other slow log options. "
    "(deprecated option, use --slow_query_log/--slow_query_log_file instead)",
   &opt_slow_logname, &opt_slow_logname, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"slow_query_log_file", OPT_SLOW_QUERY_LOG_FILE,
    "Log slow queries to given log file. Defaults logging to hostname-slow.log. "
    "Must be enabled to activate other slow log options.",
   &opt_slow_logname, &opt_slow_logname, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"log-tc", OPT_LOG_TC,
   "Path to transaction coordinator log (used for transactions that affect "
   "more than one storage engine, when binary log is disabled).",
   &opt_tc_log_file, &opt_tc_log_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_MMAP
  {"log-tc-size", OPT_LOG_TC_SIZE, "Size of transaction coordinator log.",
   &opt_tc_log_size, &opt_tc_log_size, 0, GET_ULONG,
   REQUIRED_ARG, TC_LOG_MIN_SIZE, TC_LOG_MIN_SIZE, ULONG_MAX, 0,
   TC_LOG_PAGE_SIZE, 0},
#endif
  {"log-update", OPT_UPDATE_LOG,
   "The update log is deprecated since version 5.0, is replaced by the binary "
   "log and this option just turns on --log-bin instead.",
   &opt_update_logname, &opt_update_logname, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-warnings", 'W', "Log some not critical warnings to the log file.",
   &global_system_variables.log_warnings,
   &max_system_variables.log_warnings, 0, GET_ULONG, OPT_ARG, 1, 0, 0,
   0, 0, 0},
  {"low-priority-updates", OPT_LOW_PRIORITY_UPDATES,
   "INSERT/DELETE/UPDATE has lower priority than selects.",
   &global_system_variables.low_priority_updates,
   &max_system_variables.low_priority_updates,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"master-connect-retry", OPT_MASTER_CONNECT_RETRY,
   "The number of seconds the slave thread will sleep before retrying to "
   "connect to the master, in case the master goes down or the connection "
   "is lost.",
   &master_connect_retry, &master_connect_retry, 0, GET_UINT,
   REQUIRED_ARG, 60, 0, 0, 0, 0, 0},
  {"master-host", OPT_MASTER_HOST,
   "Master hostname or IP address for replication. If not set, the slave "
   "thread will not be started. Note that the setting of master-host will "
   "be ignored if there exists a valid master.info file.",
   &master_host, &master_host, 0, GET_STR, REQUIRED_ARG, 0, 0,
   0, 0, 0, 0},
  {"master-info-file", OPT_MASTER_INFO_FILE,
   "The location and name of the file that remembers the master and where "
   "the I/O replication thread is in the master's binlogs.",
   &master_info_file, &master_info_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"master-password", OPT_MASTER_PASSWORD,
   "The password the slave thread will authenticate with when connecting to "
   "the master. If not set, an empty password is assumed. The value in "
   "master.info will take precedence if it can be read.",
   &master_password, &master_password, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"master-port", OPT_MASTER_PORT,
   "The port the master is listening on. If not set, the compiled setting of "
   "MYSQL_PORT is assumed. If you have not tinkered with configure options, "
   "this should be 3306. The value in master.info will take precedence if it "
   "can be read.", &master_port, &master_port, 0, GET_UINT, REQUIRED_ARG,
   MYSQL_PORT, 0, 0, 0, 0, 0},
  {"master-retry-count", OPT_MASTER_RETRY_COUNT,
   "The number of tries the slave will make to connect to the master before giving up.",
   &master_retry_count, &master_retry_count, 0, GET_ULONG,
   REQUIRED_ARG, 3600*24, 0, 0, 0, 0, 0},
  {"master-ssl", OPT_MASTER_SSL,
   "Enable the slave to connect to the master using SSL.",
   &master_ssl, &master_ssl, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"master-ssl-ca", OPT_MASTER_SSL_CA,
   "Master SSL CA file. Only applies if you have enabled master-ssl.",
   &master_ssl_ca, &master_ssl_ca, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"master-ssl-capath", OPT_MASTER_SSL_CAPATH,
   "Master SSL CA path. Only applies if you have enabled master-ssl.",
   &master_ssl_capath, &master_ssl_capath, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"master-ssl-cert", OPT_MASTER_SSL_CERT,
   "Master SSL certificate file name. Only applies if you have enabled "
   "master-ssl.",
   &master_ssl_cert, &master_ssl_cert, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"master-ssl-cipher", OPT_MASTER_SSL_CIPHER,
   "Master SSL cipher. Only applies if you have enabled master-ssl.",
   &master_ssl_cipher, &master_ssl_capath, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"master-ssl-key", OPT_MASTER_SSL_KEY,
   "Master SSL keyfile name. Only applies if you have enabled master-ssl.",
   &master_ssl_key, &master_ssl_key, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"master-user", OPT_MASTER_USER,
   "The username the slave thread will use for authentication when "
   "connecting to the master. The user must have FILE privilege. "
   "If the master user is not set, user test is assumed. The value "
   "in master.info will take precedence if it can be read.",
   &master_user, &master_user, 0, GET_STR, REQUIRED_ARG, 0, 0,
   0, 0, 0, 0},
#ifdef HAVE_REPLICATION
  {"max-binlog-dump-events", OPT_MAX_BINLOG_DUMP_EVENTS,
   "Option used by mysql-test for debugging and testing of replication.",
   &max_binlog_dump_events, &max_binlog_dump_events, 0,
   GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif /* HAVE_REPLICATION */
  {"memlock", OPT_MEMLOCK, "Lock mysqld in memory.", &locked_in_memory,
   &locked_in_memory, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"myisam-recover", OPT_MYISAM_RECOVER,
   "Syntax: myisam-recover[=option[,option...]], where option can be DEFAULT, BACKUP, FORCE or QUICK.",
   &myisam_recover_options_str, &myisam_recover_options_str, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
  {"ndb-connectstring", OPT_NDB_CONNECTSTRING,
   "Connect string for ndbcluster.",
   &opt_ndb_connectstring, &opt_ndb_connectstring,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"ndb-mgmd-host", OPT_NDB_MGMD,
   "Set host and port for ndb_mgmd. Syntax: hostname[:port]",
   &opt_ndb_mgmd, &opt_ndb_mgmd,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"ndb-nodeid", OPT_NDB_NODEID,
   "Nodeid for this mysqlserver in the cluster.",
   &opt_ndb_nodeid,
   &opt_ndb_nodeid,
   0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"ndb-autoincrement-prefetch-sz", OPT_NDB_AUTOINCREMENT_PREFETCH_SZ,
   "Specify number of autoincrement values that are prefetched.",
   &global_system_variables.ndb_autoincrement_prefetch_sz,
   &max_system_variables.ndb_autoincrement_prefetch_sz,
   0, GET_ULONG, REQUIRED_ARG, 1, 1, 256, 0, 0, 0},
  {"ndb-force-send", OPT_NDB_FORCE_SEND,
   "Force send of buffers to ndb immediately without waiting for "
   "other threads.",
   &global_system_variables.ndb_force_send,
   &global_system_variables.ndb_force_send,
   0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"ndb_force_send", OPT_NDB_FORCE_SEND,
   "same as --ndb-force-send.",
   &global_system_variables.ndb_force_send,
   &global_system_variables.ndb_force_send,
   0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"ndb-extra-logging", OPT_NDB_EXTRA_LOGGING,
   "Turn on more logging in the error log.",
   &ndb_extra_logging,
   &ndb_extra_logging,
   0, GET_INT, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_NDB_BINLOG
  {"ndb-report-thresh-binlog-epoch-slip", OPT_NDB_REPORT_THRESH_BINLOG_EPOCH_SLIP,
   "Threshold on number of epochs to be behind before reporting binlog status. "
   "E.g., 3 means that if the difference between what epoch has been received "
   "from the storage nodes and what has been applied to the binlog is 3 or more, "
   "a status message will be sent to the cluster log.",
   &ndb_report_thresh_binlog_epoch_slip,
   &ndb_report_thresh_binlog_epoch_slip,
   0, GET_ULONG, REQUIRED_ARG, 3, 0, 256, 0, 0, 0},
  {"ndb-report-thresh-binlog-mem-usage", OPT_NDB_REPORT_THRESH_BINLOG_MEM_USAGE,
   "Threshold on percentage of free memory before reporting binlog status. E.g., "
   "10 means that if amount of available memory for receiving binlog data from "
   "the storage nodes goes below 10%, "
   "a status message will be sent to the cluster log.",
   &ndb_report_thresh_binlog_mem_usage,
   &ndb_report_thresh_binlog_mem_usage,
   0, GET_ULONG, REQUIRED_ARG, 10, 0, 100, 0, 0, 0},
#endif
  {"ndb-use-exact-count", OPT_NDB_USE_EXACT_COUNT,
   "Use exact records count during query planning and for fast "
   "select count(*), disable for faster queries.",
   &global_system_variables.ndb_use_exact_count,
   &global_system_variables.ndb_use_exact_count,
   0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"ndb_use_exact_count", OPT_NDB_USE_EXACT_COUNT,
   "Same as --ndb-use-exact-count.",
   &global_system_variables.ndb_use_exact_count,
   &global_system_variables.ndb_use_exact_count,
   0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"ndb-use-transactions", OPT_NDB_USE_TRANSACTIONS,
   "Use transactions for large inserts, if enabled then large "
   "inserts will be split into several smaller transactions",
   &global_system_variables.ndb_use_transactions,
   &global_system_variables.ndb_use_transactions,
   0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"ndb_use_transactions", OPT_NDB_USE_TRANSACTIONS,
   "Same as --ndb-use-transactions.",
   &global_system_variables.ndb_use_transactions,
   &global_system_variables.ndb_use_transactions,
   0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"ndb-shm", OPT_NDB_SHM,
   "Use shared memory connections when available.",
   &opt_ndb_shm, &opt_ndb_shm,
   0, GET_BOOL, OPT_ARG, OPT_NDB_SHM_DEFAULT, 0, 0, 0, 0, 0},
  {"ndb-optimized-node-selection", OPT_NDB_OPTIMIZED_NODE_SELECTION,
   "Select nodes for transactions in a more optimal way.",
   &opt_ndb_optimized_node_selection,
   &opt_ndb_optimized_node_selection,
   0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
  { "ndb-cache-check-time", OPT_NDB_CACHE_CHECK_TIME,
    "A dedicated thread is created to, at the given milliseconds interval, "
    "invalidate the query cache if another MySQL server in the cluster has "
    "changed the data in the database.",
    &opt_ndb_cache_check_time, &opt_ndb_cache_check_time, 0, GET_ULONG, REQUIRED_ARG,
    0, 0, LONG_TIMEOUT, 0, 1, 0},
  {"ndb-index-stat-enable", OPT_NDB_INDEX_STAT_ENABLE,
   "Use ndb index statistics in query optimization.",
   &global_system_variables.ndb_index_stat_enable,
   &max_system_variables.ndb_index_stat_enable,
   0, GET_BOOL, OPT_ARG, 0, 0, 1, 0, 0, 0},
#endif
  {"ndb-use-copying-alter-table",
   OPT_NDB_USE_COPYING_ALTER_TABLE,
   "Force ndbcluster to always copy tables at alter table "
   "(should only be used if on-line alter table fails).",
   &global_system_variables.ndb_use_copying_alter_table,
   &global_system_variables.ndb_use_copying_alter_table,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},  
  {"new", 'n', "Use very new, possibly 'unsafe', functions.",
   &global_system_variables.new_mode,
   &max_system_variables.new_mode,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef NOT_YET
  {"no-mix-table-types", OPT_NO_MIX_TYPE, 
   "Don't allow commands that use two different table types.",
   &opt_no_mix_types, &opt_no_mix_types, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
#endif
  {"old-alter-table", OPT_OLD_ALTER_TABLE,
   "Use old, non-optimized alter table.",
   &global_system_variables.old_alter_table,
   &max_system_variables.old_alter_table, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"old-passwords", OPT_OLD_PASSWORDS, "Use old password "
   "encryption method (needed for 4.0 and older clients).",
   &global_system_variables.old_passwords,
   &max_system_variables.old_passwords, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"one-thread", OPT_ONE_THREAD,
   "(Deprecated): Only use one thread (for debugging under Linux). Use "
   "thread-handling=no-threads instead.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"old-style-user-limits", OPT_OLD_STYLE_USER_LIMITS,
   "Enable old-style user limits (before 5.0.3, user resources were counted "
   "per each user+host vs. per account).",
   &opt_old_style_user_limits, &opt_old_style_user_limits,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"pid-file", OPT_PID_FILE, "Pid file used by safe_mysqld.",
   &pidfile_name_ptr, &pidfile_name_ptr, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
   &mysqld_port,
   &mysqld_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port-open-timeout", OPT_PORT_OPEN_TIMEOUT,
   "Maximum time in seconds to wait for the port to become free. "
   "(Default: No wait).", &mysqld_port_timeout,
   &mysqld_port_timeout, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
  {"profiling_history_size", OPT_PROFILING, "Limit of query profiling memory.",
   &global_system_variables.profiling_history_size,
   &max_system_variables.profiling_history_size,
   0, GET_ULONG, REQUIRED_ARG, 15, 0, 100, 0, 0, 0},
#endif
  {"relay-log", OPT_RELAY_LOG,
   "The location and name to use for relay logs.",
   &opt_relay_logname, &opt_relay_logname, 0,
   GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"relay-log-index", OPT_RELAY_LOG_INDEX,
   "The location and name to use for the file that keeps a list of the last \
relay logs.",
   &opt_relaylog_index_name, &opt_relaylog_index_name, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"relay-log-info-file", OPT_RELAY_LOG_INFO_FILE,
   "The location and name of the file that remembers where the SQL replication \
thread is in the relay logs.",
   &relay_log_info_file, &relay_log_info_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-do-db", OPT_REPLICATE_DO_DB,
   "Tells the slave thread to restrict replication to the specified database. "
   "To specify more than one database, use the directive multiple times, "
   "once for each database. Note that this will only work if you do not use "
   "cross-database queries such as UPDATE some_db.some_table SET foo='bar' "
   "while having selected a different or no database. If you need cross "
   "database updates to work, make sure you have 3.23.28 or later, and use "
   "replicate-wild-do-table=db_name.%.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-do-table", OPT_REPLICATE_DO_TABLE,
   "Tells the slave thread to restrict replication to the specified table. "
   "To specify more than one table, use the directive multiple times, once "
   "for each table. This will work for cross-database updates, in contrast "
   "to replicate-do-db.", 0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-ignore-db", OPT_REPLICATE_IGNORE_DB,
   "Tells the slave thread to not replicate to the specified database. To "
   "specify more than one database to ignore, use the directive multiple "
   "times, once for each database. This option will not work if you use "
   "cross database updates. If you need cross database updates to work, "
   "make sure you have 3.23.28 or later, and use replicate-wild-ignore-"
   "table=db_name.%. ", 0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-ignore-table", OPT_REPLICATE_IGNORE_TABLE,
   "Tells the slave thread to not replicate to the specified table. To specify "
   "more than one table to ignore, use the directive multiple times, once for "
   "each table. This will work for cross-database updates, in contrast to "
   "replicate-ignore-db.", 0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-rewrite-db", OPT_REPLICATE_REWRITE_DB,
   "Updates to a database with a different name than the original. Example: "
   "replicate-rewrite-db=master_db_name->slave_db_name.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_REPLICATION
  {"replicate-same-server-id", OPT_REPLICATE_SAME_SERVER_ID,
   "In replication, if set to 1, do not skip events having our server id. "
   "Default value is 0 (to break infinite loops in circular replication). "
   "Can't be set to 1 if --log-slave-updates is used.",
   &replicate_same_server_id, &replicate_same_server_id,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"replicate-wild-do-table", OPT_REPLICATE_WILD_DO_TABLE,
   "Tells the slave thread to restrict replication to the tables that match "
   "the specified wildcard pattern. To specify more than one table, use the "
   "directive multiple times, once for each table. This will work for cross-"
   "database updates. Example: replicate-wild-do-table=foo%.bar% will "
   "replicate only updates to tables in all databases that start with foo "
   "and whose table names start with bar.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-wild-ignore-table", OPT_REPLICATE_WILD_IGNORE_TABLE,
   "Tells the slave thread to not replicate to the tables that match the "
   "given wildcard pattern. To specify more than one table to ignore, use "
   "the directive multiple times, once for each table. This will work for "
   "cross-database updates. Example: replicate-wild-ignore-table=foo%.bar% "
   "will not do updates to tables in databases that start with foo and whose "
   "table names start with bar.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  // In replication, we may need to tell the other servers how to connect
  {"report-host", OPT_REPORT_HOST,
   "Hostname or IP of the slave to be reported to the master during slave "
   "registration. Will appear in the output of SHOW SLAVE HOSTS. Leave unset "
   "if you do not want the slave to register itself with the master. Note that "
   "it is not sufficient for the master to simply read the IP of the slave "
   "from the socket once the slave connects. Due to NAT and other routing "
   "issues, that IP may not be valid for connecting to the slave from the "
   "master or other hosts.",
   &report_host, &report_host, 0, GET_STR, REQUIRED_ARG, 0, 0,
   0, 0, 0, 0},
  {"report-password", OPT_REPORT_PASSWORD, "Undocumented.",
   &report_password, &report_password, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"report-port", OPT_REPORT_PORT,
   "Port for connecting to slave reported to the master during slave "
   "registration. Set it only if the slave is listening on a non-default "
   "port or if you have a special tunnel from the master or other clients "
   "to the slave. If not sure, leave this option unset.",
   &report_port, &report_port, 0, GET_UINT, REQUIRED_ARG,
   MYSQL_PORT, 0, 0, 0, 0, 0},
  {"report-user", OPT_REPORT_USER, "Undocumented.", &report_user,
   &report_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"rpl-recovery-rank", OPT_RPL_RECOVERY_RANK, "Undocumented.",
   &rpl_recovery_rank, &rpl_recovery_rank, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"safe-mode", OPT_SAFE, "Skip some optimize stages (for testing).",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef TO_BE_DELETED
  {"safe-show-database", OPT_SAFE_SHOW_DB,
   "Deprecated option; use GRANT SHOW DATABASES instead.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"safe-user-create", OPT_SAFE_USER_CREATE,
   "Don't allow new user creation by the user who has no write privileges to the mysql.user table.",
   &opt_safe_user_create, &opt_safe_user_create, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"safemalloc-mem-limit", OPT_SAFEMALLOC_MEM_LIMIT,
   "Simulate memory shortage when compiled with the --with-debug=full option.",
   0, 0, 0, GET_ULL, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"secure-auth", OPT_SECURE_AUTH, "Disallow authentication for accounts that have old (pre-4.1) passwords.",
   &opt_secure_auth, &opt_secure_auth, 0, GET_BOOL, NO_ARG,
   my_bool(0), 0, 0, 0, 0, 0},
  {"secure-file-priv", OPT_SECURE_FILE_PRIV,
   "Limit LOAD DATA, SELECT ... OUTFILE, and LOAD_FILE() to files within specified directory.",
   &opt_secure_file_priv, &opt_secure_file_priv, 0,
   GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"server-id",	OPT_SERVER_ID,
   "Uniquely identifies the server instance in the community of replication partners.",
   &server_id, &server_id, 0, GET_ULONG, REQUIRED_ARG, 0, 0, UINT_MAX32,
   0, 0, 0},
  {"set-variable", 'O',
   "Change the value of a variable. Please note that this option is deprecated; "
   "you can set variables directly with --variable-name=value.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_SMEM
  {"shared-memory", OPT_ENABLE_SHARED_MEMORY,
   "Enable the shared memory.",&opt_enable_shared_memory, &opt_enable_shared_memory,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
#ifdef HAVE_SMEM
  {"shared-memory-base-name",OPT_SHARED_MEMORY_BASE_NAME,
   "Base name of shared memory.", &shared_memory_base_name, &shared_memory_base_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"show-slave-auth-info", OPT_SHOW_SLAVE_AUTH_INFO,
   "Show user and password in SHOW SLAVE HOSTS on this master.",
   &opt_show_slave_auth_info, &opt_show_slave_auth_info, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DISABLE_GRANT_OPTIONS
  {"skip-grant-tables", OPT_SKIP_GRANT,
   "Start without grant tables. This gives all users FULL ACCESS to all tables.",
   &opt_noacl, &opt_noacl, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
#endif
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
  {"skip-new", OPT_SKIP_NEW, "Don't use new, possibly wrong routines.",
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
   "If set, slave is not autostarted.", &opt_skip_slave_start,
   &opt_skip_slave_start, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-stack-trace", OPT_SKIP_STACK_TRACE,
   "Don't print a stack trace on failure.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"skip-symlink", OPT_SKIP_SYMLINKS, "Don't allow symlinking of tables. "
  "Deprecated option. Use --skip-symbolic-links instead.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-thread-priority", OPT_SKIP_PRIOR,
   "Don't give threads different priorities. Deprecated option.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   DEFAULT_SKIP_THREAD_PRIORITY, 0, 0, 0, 0, 0},
#ifdef HAVE_REPLICATION
  {"slave-load-tmpdir", OPT_SLAVE_LOAD_TMPDIR,
   "The location where the slave should put its temporary files when "
   "replicating a LOAD DATA INFILE command.",
   &slave_load_tmpdir, &slave_load_tmpdir, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"slave-skip-errors", OPT_SLAVE_SKIP_ERRORS,
   "Tells the slave thread to continue replication when a query event returns an error from the provided list.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"slave-exec-mode", OPT_SLAVE_EXEC_MODE,
   "Modes for how replication events should be executed. Legal values are "
   "STRICT (default) and IDEMPOTENT. In IDEMPOTENT mode, replication will "
   "not stop for operations that are idempotent. In STRICT mode, replication "
   "will stop on any unexpected difference between the master and the slave.",
   &slave_exec_mode_str, &slave_exec_mode_str, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"slow-query-log", OPT_SLOW_LOG,
   "Enable/disable slow query log.", &opt_slow_log,
   &opt_slow_log, 0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", OPT_SOCKET, "Socket file to use for connection.",
   &mysqld_unix_port, &mysqld_unix_port, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_REPLICATION
  {"sporadic-binlog-dump-fail", OPT_SPORADIC_BINLOG_DUMP_FAIL,
   "Option used by mysql-test for debugging and testing of replication.",
   &opt_sporadic_binlog_dump_fail,
   &opt_sporadic_binlog_dump_fail, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
#endif /* HAVE_REPLICATION */
  {"sql-bin-update-same", OPT_SQL_BIN_UPDATE_SAME,
   "The update log is deprecated since version 5.0, is replaced by the "
   "binary log and this option does nothing anymore.",
   0, 0, 0, GET_DISABLED, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"sql-mode", OPT_SQL_MODE,
   "Syntax: sql-mode=option[,option[,option...]] where option can be one "
   "of: REAL_AS_FLOAT, PIPES_AS_CONCAT, ANSI_QUOTES, IGNORE_SPACE, "
   "ONLY_FULL_GROUP_BY, NO_UNSIGNED_SUBTRACTION.",
   &sql_mode_str, &sql_mode_str, 0, GET_STR, REQUIRED_ARG, 0,
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
   &my_use_symdir, &my_use_symdir, 0, GET_BOOL, NO_ARG,
   /*
     The system call realpath() produces warnings under valgrind and
     purify. These are not suppressed: instead we disable symlinks
     option if compiled with valgrind support.
   */
   IF_PURIFY(0,1), 0, 0, 0, 0, 0},
  {"sysdate-is-now", OPT_SYSDATE_IS_NOW,
   "Non-default option to alias SYSDATE() to NOW() to make it safe-replicable. "
   "Since 5.0, SYSDATE() returns a `dynamic' value different for different "
   "invocations, even within the same statement.",
   &global_system_variables.sysdate_is_now,
   0, 0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 1, 0},
  {"tc-heuristic-recover", OPT_TC_HEURISTIC_RECOVER,
   "Decision to use in heuristic recover process. Possible values are COMMIT "
   "or ROLLBACK.", &opt_tc_heuristic_recover, &opt_tc_heuristic_recover,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#if defined(ENABLED_DEBUG_SYNC)
  {"debug-sync-timeout", OPT_DEBUG_SYNC_TIMEOUT,
   "Enable the debug sync facility "
   "and optionally specify a default wait timeout in seconds. "
   "A zero value keeps the facility disabled.",
   &opt_debug_sync_timeout, 0,
   0, GET_UINT, OPT_ARG, 0, 0, UINT_MAX, 0, 0, 0},
#endif /* defined(ENABLED_DEBUG_SYNC) */
  {"temp-pool", OPT_TEMP_POOL,
#if (ENABLE_TEMP_POOL)
   "Using this option will cause most temporary files created to use a small "
   "set of names, rather than a unique name for each new file.",
#else
   "This option is ignored on this OS.",
#endif
   &use_temp_pool, &use_temp_pool, 0, GET_BOOL, NO_ARG, 1,
   0, 0, 0, 0, 0},
  {"timed_mutexes", OPT_TIMED_MUTEXES,
   "Specify whether to time mutexes (only InnoDB mutexes are currently supported).",
   &timed_mutexes, &timed_mutexes, 0, GET_BOOL, NO_ARG, 0,
    0, 0, 0, 0, 0},
  {"tmpdir", 't',
   "Path for temporary files. Several paths may be specified, separated by a "
#if defined(__WIN__) || defined(__NETWARE__)
   "semicolon (;)"
#else
   "colon (:)"
#endif
   ", in this case they are used in a round-robin fashion.",
   &opt_mysql_tmpdir, &opt_mysql_tmpdir, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"transaction-isolation", OPT_TX_ISOLATION,
   "Default transaction isolation level.", 0, 0, 0, GET_STR, REQUIRED_ARG, 0,
   0, 0, 0, 0, 0},
  {"use-symbolic-links", OPT_SYMBOLIC_LINKS, "Enable symbolic link support. "
   "Deprecated option; use --symbolic-links instead.",
   &my_use_symdir, &my_use_symdir, 0, GET_BOOL, NO_ARG,
   IF_PURIFY(0,1), 0, 0, 0, 0, 0},
  {"user", 'u', "Run mysqld daemon as user.", 0, 0, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Used with --help option for detailed help.",
   &opt_verbose, &opt_verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"warnings", OPT_WARNINGS, "Deprecated; use --log-warnings instead.",
   &global_system_variables.log_warnings,
   &max_system_variables.log_warnings, 0, GET_ULONG, OPT_ARG,
   1, 0, ULONG_MAX, 0, 0, 0},
  {"back_log", OPT_BACK_LOG,
   "The number of outstanding connection requests MySQL can have. This "
   "comes into play when the main MySQL thread gets very many connection "
   "requests in a very short time.", &back_log, &back_log, 0, GET_ULONG,
   REQUIRED_ARG, 50, 1, 65535, 0, 1, 0 },
  {"binlog_cache_size", OPT_BINLOG_CACHE_SIZE,
   "The size of the cache to hold the SQL statements for the binary log "
   "during a transaction. If you often use big, multi-statement "
   "transactions you can increase this to get more performance.",
   &binlog_cache_size, &binlog_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, 32*1024L, IO_SIZE, ULONG_MAX, 0, IO_SIZE, 0},
  {"bulk_insert_buffer_size", OPT_BULK_INSERT_BUFFER_SIZE,
   "Size of tree cache used in bulk insert optimization. Note that this "
   "is a limit per thread.", &global_system_variables.bulk_insert_buff_size,
   &max_system_variables.bulk_insert_buff_size,
   0, GET_ULONG, REQUIRED_ARG, 8192*1024, 0, ULONG_MAX, 0, 1, 0},
  {"connect_timeout", OPT_CONNECT_TIMEOUT,
   "The number of seconds the mysqld server is waiting for a connect packet "
   "before responding with 'Bad handshake'.", &connect_timeout, &connect_timeout,
   0, GET_ULONG, REQUIRED_ARG, CONNECT_TIMEOUT, 2, LONG_TIMEOUT, 0, 1, 0 },
  { "date_format", OPT_DATE_FORMAT,
    "The DATE format (for future).",
    &opt_date_time_formats[MYSQL_TIMESTAMP_DATE],
    &opt_date_time_formats[MYSQL_TIMESTAMP_DATE],
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "datetime_format", OPT_DATETIME_FORMAT,
    "The DATETIME/TIMESTAMP format (for future).",
    &opt_date_time_formats[MYSQL_TIMESTAMP_DATETIME],
    &opt_date_time_formats[MYSQL_TIMESTAMP_DATETIME],
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "default_week_format", OPT_DEFAULT_WEEK_FORMAT,
    "The default week format used by WEEK() functions.",
    &global_system_variables.default_week_format,
    &max_system_variables.default_week_format,
    0, GET_ULONG, REQUIRED_ARG, 0, 0, 7L, 0, 1, 0},
  {"delayed_insert_limit", OPT_DELAYED_INSERT_LIMIT,
   "After inserting delayed_insert_limit rows, the INSERT DELAYED handler "
   "will check if there are any SELECT statements pending. If so, it allows "
   "these to execute before continuing.",
    &delayed_insert_limit, &delayed_insert_limit, 0, GET_ULONG,
    REQUIRED_ARG, DELAYED_LIMIT, 1, ULONG_MAX, 0, 1, 0},
  {"delayed_insert_timeout", OPT_DELAYED_INSERT_TIMEOUT,
   "How long a INSERT DELAYED thread should wait for INSERT statements before terminating.",
   &delayed_insert_timeout, &delayed_insert_timeout, 0,
   GET_ULONG, REQUIRED_ARG, DELAYED_WAIT_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  { "delayed_queue_size", OPT_DELAYED_QUEUE_SIZE,
    "What size queue (in rows) should be allocated for handling INSERT DELAYED. "
    "If the queue becomes full, any client that does INSERT DELAYED will wait "
    "until there is room in the queue again.",
    &delayed_queue_size, &delayed_queue_size, 0, GET_ULONG,
    REQUIRED_ARG, DELAYED_QUEUE_SIZE, 1, ULONG_MAX, 0, 1, 0},
  {"div_precision_increment", OPT_DIV_PRECINCREMENT,
   "Precision of the result of '/' operator will be increased on that value.",
   &global_system_variables.div_precincrement,
   &max_system_variables.div_precincrement, 0, GET_ULONG,
   REQUIRED_ARG, 4, 0, DECIMAL_MAX_SCALE, 0, 0, 0},
  {"expire_logs_days", OPT_EXPIRE_LOGS_DAYS,
   "If non-zero, binary logs will be purged after expire_logs_days "
   "days; possible purges happen at startup and at binary log rotation.",
   &expire_logs_days, &expire_logs_days, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, 99, 0, 1, 0},
  { "flush_time", OPT_FLUSH_TIME,
    "A dedicated thread is created to flush all tables at the given interval.",
    &flush_time, &flush_time, 0, GET_ULONG, REQUIRED_ARG,
    FLUSH_TIME, 0, LONG_TIMEOUT, 0, 1, 0},
  { "ft_boolean_syntax", OPT_FT_BOOLEAN_SYNTAX,
    "List of operators for MATCH ... AGAINST ( ... IN BOOLEAN MODE).",
    0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "ft_max_word_len", OPT_FT_MAX_WORD_LEN,
    "The maximum length of the word to be included in a FULLTEXT index. "
    "Note: FULLTEXT indexes must be rebuilt after changing this variable.",
    &ft_max_word_len, &ft_max_word_len, 0, GET_ULONG,
    REQUIRED_ARG, HA_FT_MAXCHARLEN, 10, HA_FT_MAXCHARLEN, 0, 1, 0},
  { "ft_min_word_len", OPT_FT_MIN_WORD_LEN,
    "The minimum length of the word to be included in a FULLTEXT index. "
    "Note: FULLTEXT indexes must be rebuilt after changing this variable.",
    &ft_min_word_len, &ft_min_word_len, 0, GET_ULONG,
    REQUIRED_ARG, 4, 1, HA_FT_MAXCHARLEN, 0, 1, 0},
  { "ft_query_expansion_limit", OPT_FT_QUERY_EXPANSION_LIMIT,
    "Number of best matches to use for query expansion.",
    &ft_query_expansion_limit, &ft_query_expansion_limit, 0, GET_ULONG,
    REQUIRED_ARG, 20, 0, 1000, 0, 1, 0},
  { "ft_stopword_file", OPT_FT_STOPWORD_FILE,
    "Use stopwords from this file instead of built-in list.",
    &ft_stopword_file, &ft_stopword_file, 0, GET_STR,
    REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "group_concat_max_len", OPT_GROUP_CONCAT_MAX_LEN,
    "The maximum length of the result of function group_concat.",
    &global_system_variables.group_concat_max_len,
    &max_system_variables.group_concat_max_len, 0, GET_ULONG,
    REQUIRED_ARG, 1024, 4, ULONG_MAX, 0, 1, 0},
  {"interactive_timeout", OPT_INTERACTIVE_TIMEOUT,
   "The number of seconds the server waits for activity on an interactive "
   "connection before closing it.",
   &global_system_variables.net_interactive_timeout,
   &max_system_variables.net_interactive_timeout, 0,
   GET_ULONG, REQUIRED_ARG, NET_WAIT_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"join_buffer_size", OPT_JOIN_BUFF_SIZE,
   "The size of the buffer that is used for full joins.",
   &global_system_variables.join_buff_size,
   &max_system_variables.join_buff_size, 0, GET_ULONG,
   REQUIRED_ARG, 128*1024L, IO_SIZE*2+MALLOC_OVERHEAD, ULONG_MAX,
   MALLOC_OVERHEAD, IO_SIZE, 0},
  {"keep_files_on_create", OPT_KEEP_FILES_ON_CREATE,
   "Don't overwrite stale .MYD and .MYI even if no directory is specified.",
   &global_system_variables.keep_files_on_create,
   &max_system_variables.keep_files_on_create,
   0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"key_buffer_size", OPT_KEY_BUFFER_SIZE,
   "The size of the buffer used for index blocks for MyISAM tables. Increase "
   "this to get better index handling (for all reads and multiple writes) to "
   "as much as you can afford; 1GB on a 4GB machine that mainly runs MySQL is "
   "quite common.",
   &dflt_key_cache_var.param_buff_size, NULL, NULL, (GET_ULL | GET_ASK_ADDR),
   REQUIRED_ARG, KEY_CACHE_SIZE, 0, SIZE_T_MAX, MALLOC_OVERHEAD,
   IO_SIZE, 0},
  {"key_cache_age_threshold", OPT_KEY_CACHE_AGE_THRESHOLD,
   "This characterizes the number of hits a hot block has to be untouched "
   "until it is considered aged enough to be downgraded to a warm block. "
   "This specifies the percentage ratio of that number of hits to the total "
   "number of blocks in key cache.",
   &dflt_key_cache_var.param_age_threshold, NULL, NULL,
   (GET_ULONG | GET_ASK_ADDR), REQUIRED_ARG, 300, 100, ULONG_MAX, 0, 100, 0},
  {"key_cache_block_size", OPT_KEY_CACHE_BLOCK_SIZE,
   "The default size of key cache blocks.",
   &dflt_key_cache_var.param_block_size, NULL, NULL, (GET_ULONG | GET_ASK_ADDR),
   REQUIRED_ARG, KEY_CACHE_BLOCK_SIZE, 512, 1024 * 16, 0, 512, 0},
  {"key_cache_division_limit", OPT_KEY_CACHE_DIVISION_LIMIT,
   "The minimum percentage of warm blocks in key cache.",
   &dflt_key_cache_var.param_division_limit, NULL, NULL,
   (GET_ULONG | GET_ASK_ADDR) , REQUIRED_ARG, 100, 1, 100, 0, 1, 0},
  {"long_query_time", OPT_LONG_QUERY_TIME,
   "Log all queries that have taken more than long_query_time seconds to "
   "execute. The argument will be treated as a decimal value with "
   "microsecond precision.",
   &long_query_time, &long_query_time, 0, GET_DOUBLE,
   REQUIRED_ARG, 10, 0, LONG_TIMEOUT, 0, 0, 0},
  {"lower_case_table_names", OPT_LOWER_CASE_TABLE_NAMES,
   "If set to 1, table names are stored in lowercase on disk and table names "
   "will be case-insensitive.  Should be set to 2 if you are using a case-"
   "insensitive file system.",
   &lower_case_table_names, &lower_case_table_names, 0, GET_UINT, OPT_ARG,
#ifdef FN_NO_CASE_SENCE
    1
#else
    0
#endif
   , 0, 2, 0, 1, 0},
  {"max_allowed_packet", OPT_MAX_ALLOWED_PACKET,
   "The maximum packet length to send to or receive from server.",
   &global_system_variables.max_allowed_packet,
   &max_system_variables.max_allowed_packet, 0, GET_ULONG,
   REQUIRED_ARG, 1024*1024L, 1024, 1024L*1024L*1024L, MALLOC_OVERHEAD, 1024, 0},
  {"slave_max_allowed_packet", OPT_SLAVE_MAX_ALLOWED_PACKET,
   "The maximum packet length to sent successfully from the master to slave.",
   &slave_max_allowed_packet, &slave_max_allowed_packet, 0, GET_ULONG,
   REQUIRED_ARG, MAX_MAX_ALLOWED_PACKET, 1024, MAX_MAX_ALLOWED_PACKET, MALLOC_OVERHEAD, 1024, 0},
  {"max_binlog_cache_size", OPT_MAX_BINLOG_CACHE_SIZE,
   "Can be used to restrict the total size used to cache a multi-transaction query.",
   &max_binlog_cache_size, &max_binlog_cache_size, 0,
   GET_ULL, REQUIRED_ARG, (longlong) ULONG_MAX, IO_SIZE, ULONGLONG_MAX, 0, IO_SIZE, 0},
  {"max_binlog_size", OPT_MAX_BINLOG_SIZE,
   "Binary log will be rotated automatically when the size exceeds this "
   "value. Will also apply to relay logs if max_relay_log_size is 0. "
   "The minimum value for this variable is 4096.",
   &max_binlog_size, &max_binlog_size, 0, GET_ULONG,
   REQUIRED_ARG, 1024*1024L*1024L, IO_SIZE, 1024*1024L*1024L, 0, IO_SIZE, 0},
  {"max_connect_errors", OPT_MAX_CONNECT_ERRORS,
   "If there is more than this number of interrupted connections from a host "
   "this host will be blocked from further connections.",
   &max_connect_errors, &max_connect_errors, 0, GET_ULONG,
   REQUIRED_ARG, MAX_CONNECT_ERRORS, 1, ULONG_MAX, 0, 1, 0},
  // Default max_connections of 151 is larger than Apache's default max
  // children, to avoid "too many connections" error in a common setup
  {"max_connections", OPT_MAX_CONNECTIONS,
   "The number of simultaneous clients allowed.", &max_connections,
   &max_connections, 0, GET_ULONG, REQUIRED_ARG, 151, 1, 100000, 0, 1, 0},
  {"max_delayed_threads", OPT_MAX_DELAYED_THREADS,
   "Don't start more than this number of threads to handle INSERT DELAYED "
   "statements. If set to zero, which means INSERT DELAYED is not used.",
   &global_system_variables.max_insert_delayed_threads,
   &max_system_variables.max_insert_delayed_threads,
   0, GET_ULONG, REQUIRED_ARG, 20, 0, 16384, 0, 1, 0},
  {"max_error_count", OPT_MAX_ERROR_COUNT,
   "Max number of errors/warnings to store for a statement.",
   &global_system_variables.max_error_count,
   &max_system_variables.max_error_count,
   0, GET_ULONG, REQUIRED_ARG, DEFAULT_ERROR_COUNT, 0, 65535, 0, 1, 0},
  {"max_heap_table_size", OPT_MAX_HEP_TABLE_SIZE,
   "Don't allow creation of heap tables bigger than this.",
   &global_system_variables.max_heap_table_size,
   &max_system_variables.max_heap_table_size, 0, GET_ULL,
   REQUIRED_ARG, 16*1024*1024L, 16384, MAX_MEM_TABLE_SIZE,
   MALLOC_OVERHEAD, 1024, 0},
  {"max_join_size", OPT_MAX_JOIN_SIZE,
   "Joins that are probably going to read more than max_join_size records return an error.",
   &global_system_variables.max_join_size,
   &max_system_variables.max_join_size, 0, GET_HA_ROWS, REQUIRED_ARG,
   (longlong) HA_POS_ERROR, 1, HA_POS_ERROR, 0, 1, 0},
   {"max_length_for_sort_data", OPT_MAX_LENGTH_FOR_SORT_DATA,
    "Max number of bytes in sorted records.",
    &global_system_variables.max_length_for_sort_data,
    &max_system_variables.max_length_for_sort_data, 0, GET_ULONG,
    REQUIRED_ARG, 1024, 4, 8192*1024L, 0, 1, 0},
  {"max_long_data_size", OPT_MAX_LONG_DATA_SIZE,
   "The maximum size of prepared statement parameter which can be provided "
   "through mysql_send_long_data() API call. "
   "Deprecated option; use max_allowed_packet instead.",
   &max_long_data_size,
   &max_long_data_size, 0, GET_ULONG,
   REQUIRED_ARG, 1024*1024L, 1024, UINT_MAX32, MALLOC_OVERHEAD, 1, 0},
  {"max_prepared_stmt_count", OPT_MAX_PREPARED_STMT_COUNT,
   "Maximum number of prepared statements in the server.",
   &max_prepared_stmt_count, &max_prepared_stmt_count,
   0, GET_ULONG, REQUIRED_ARG, 16382, 0, 1*1024*1024, 0, 1, 0},
  {"max_relay_log_size", OPT_MAX_RELAY_LOG_SIZE,
   "If non-zero: relay log will be rotated automatically when the size "
   "exceeds this value; if zero (the default): when the size exceeds "
   "max_binlog_size. 0 excepted, the minimum value for this variable is 4096.",
   &max_relay_log_size, &max_relay_log_size, 0, GET_ULONG,
   REQUIRED_ARG, 0L, 0L, 1024*1024L*1024L, 0, IO_SIZE, 0},
  { "max_seeks_for_key", OPT_MAX_SEEKS_FOR_KEY,
    "Limit assumed max number of seeks when looking up rows based on a key.",
    &global_system_variables.max_seeks_for_key,
    &max_system_variables.max_seeks_for_key, 0, GET_ULONG,
    REQUIRED_ARG, (longlong) ULONG_MAX, 1, ULONG_MAX, 0, 1, 0 },
  {"max_sort_length", OPT_MAX_SORT_LENGTH,
   "The number of bytes to use when sorting BLOB or TEXT values (only the "
   "first max_sort_length bytes of each value are used; the rest are ignored).",
   &global_system_variables.max_sort_length,
   &max_system_variables.max_sort_length, 0, GET_ULONG,
   REQUIRED_ARG, 1024, 4, 8192*1024L, 0, 1, 0},
  {"max_sp_recursion_depth", OPT_MAX_SP_RECURSION_DEPTH,
   "Maximum stored procedure recursion depth. (discussed with docs).",
   &global_system_variables.max_sp_recursion_depth,
   &max_system_variables.max_sp_recursion_depth, 0, GET_ULONG,
   OPT_ARG, 0, 0, 255, 0, 1, 0 },
  {"max_tmp_tables", OPT_MAX_TMP_TABLES,
   "Maximum number of temporary tables a client can keep open at a time.",
   &global_system_variables.max_tmp_tables,
   &max_system_variables.max_tmp_tables, 0, GET_ULONG,
   REQUIRED_ARG, 32, 1, ULONG_MAX, 0, 1, 0},
  {"max_user_connections", OPT_MAX_USER_CONNECTIONS,
   "The maximum number of active connections for a single user (0 = no limit).",
   &max_user_connections, &max_user_connections, 0, GET_UINT,
   REQUIRED_ARG, 0, 0, UINT_MAX, 0, 1, 0},
  {"max_write_lock_count", OPT_MAX_WRITE_LOCK_COUNT,
   "After this many write locks, allow some read locks to run in between.",
   &max_write_lock_count, &max_write_lock_count, 0, GET_ULONG,
   REQUIRED_ARG, (longlong) ULONG_MAX, 1, ULONG_MAX, 0, 1, 0},
  {"min_examined_row_limit", OPT_MIN_EXAMINED_ROW_LIMIT,
   "Don't log queries which examine less than min_examined_row_limit rows to file.",
   &global_system_variables.min_examined_row_limit,
   &max_system_variables.min_examined_row_limit, 0, GET_ULONG,
  REQUIRED_ARG, 0, 0, ULONG_MAX, 0, 1L, 0},
  {"multi_range_count", OPT_MULTI_RANGE_COUNT,
   "Number of key ranges to request at once.",
   &global_system_variables.multi_range_count,
   &max_system_variables.multi_range_count, 0,
   GET_ULONG, REQUIRED_ARG, 256, 1, ULONG_MAX, 0, 1, 0},
  {"myisam_block_size", OPT_MYISAM_BLOCK_SIZE,
   "Block size to be used for MyISAM index pages.",
   &opt_myisam_block_size, &opt_myisam_block_size, 0, GET_ULONG, REQUIRED_ARG,
   MI_KEY_BLOCK_LENGTH, MI_MIN_KEY_BLOCK_LENGTH, MI_MAX_KEY_BLOCK_LENGTH,
   0, MI_MIN_KEY_BLOCK_LENGTH, 0},
  {"myisam_data_pointer_size", OPT_MYISAM_DATA_POINTER_SIZE,
   "Default pointer size to be used for MyISAM tables.",
   &myisam_data_pointer_size,
   &myisam_data_pointer_size, 0, GET_ULONG, REQUIRED_ARG,
   6, 2, 7, 0, 1, 0},
  {"myisam_max_extra_sort_file_size", OPT_MYISAM_MAX_EXTRA_SORT_FILE_SIZE,
   "This is a deprecated option that does nothing anymore. "
   "It will be removed in MySQL " VER_CELOSIA,
   &global_system_variables.myisam_max_extra_sort_file_size,
   &max_system_variables.myisam_max_extra_sort_file_size,
   0, GET_ULL, REQUIRED_ARG, (ulonglong) INT_MAX32,
   0, MAX_FILE_SIZE, 0, 1, 0},
  {"myisam_max_sort_file_size", OPT_MYISAM_MAX_SORT_FILE_SIZE,
   "Don't use the fast sort index method to created index if the temporary "
   "file would get bigger than this.",
   &global_system_variables.myisam_max_sort_file_size,
   &max_system_variables.myisam_max_sort_file_size, 0,
   GET_ULL, REQUIRED_ARG, (longlong) LONG_MAX, 0, MAX_FILE_SIZE,
   0, 1024*1024, 0},
  {"myisam_mmap_size", OPT_MYISAM_MMAP_SIZE,
   "Can be used to restrict the total memory used for memory mmaping of myisam files",
   &myisam_mmap_size, &myisam_mmap_size, 0,
   GET_ULL, REQUIRED_ARG, (longlong) SIZE_T_MAX, MEMMAP_EXTRA_MARGIN, SIZE_T_MAX,
   0, 1, 0},
  {"myisam_repair_threads", OPT_MYISAM_REPAIR_THREADS,
   "Specifies whether several threads should be used when repairing MyISAM "
   "tables. For values > 1, one thread is used per index. The value of 1 "
   "disables parallel repair.",
   &global_system_variables.myisam_repair_threads,
   &max_system_variables.myisam_repair_threads, 0,
   GET_ULONG, REQUIRED_ARG, 1, 1, ULONG_MAX, 0, 1, 0},
  {"myisam_sort_buffer_size", OPT_MYISAM_SORT_BUFFER_SIZE,
   "The buffer that is allocated when sorting the index when doing a REPAIR "
   "or when creating indexes with CREATE INDEX or ALTER TABLE.",
   &global_system_variables.myisam_sort_buff_size,
   &max_system_variables.myisam_sort_buff_size, 0,
   GET_ULONG, REQUIRED_ARG, 8192 * 1024, 4096, ~0ULL, 0, 1, 0},
  {"myisam_use_mmap", OPT_MYISAM_USE_MMAP,
   "Use memory mapping for reading and writing MyISAM tables.",
   &opt_myisam_use_mmap, &opt_myisam_use_mmap, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"myisam_stats_method", OPT_MYISAM_STATS_METHOD,
   "Specifies how MyISAM index statistics collection code should threat NULLs. "
   "Possible values of name are \"nulls_unequal\" (default behavior for 4.1/5.0), "
   "\"nulls_equal\" (emulate 4.0 behavior), and \"nulls_ignored\".",
   &myisam_stats_method_str, &myisam_stats_method_str, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"net_buffer_length", OPT_NET_BUFFER_LENGTH,
   "Buffer length for TCP/IP and socket communication.",
   &global_system_variables.net_buffer_length,
   &max_system_variables.net_buffer_length, 0, GET_ULONG,
   REQUIRED_ARG, 16384, 1024, 1024*1024L, 0, 1024, 0},
  {"net_read_timeout", OPT_NET_READ_TIMEOUT,
   "Number of seconds to wait for more data from a connection before aborting the read.",
   &global_system_variables.net_read_timeout,
   &max_system_variables.net_read_timeout, 0, GET_ULONG,
   REQUIRED_ARG, NET_READ_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"net_retry_count", OPT_NET_RETRY_COUNT,
   "If a read on a communication port is interrupted, retry this many times before giving up.",
   &global_system_variables.net_retry_count,
   &max_system_variables.net_retry_count,0,
   GET_ULONG, REQUIRED_ARG, MYSQLD_NET_RETRY_COUNT, 1, ULONG_MAX, 0, 1, 0},
  {"net_write_timeout", OPT_NET_WRITE_TIMEOUT,
   "Number of seconds to wait for a block to be written to a connection before "
   "aborting the write.",
   &global_system_variables.net_write_timeout,
   &max_system_variables.net_write_timeout, 0, GET_ULONG,
   REQUIRED_ARG, NET_WRITE_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  { "old", OPT_OLD_MODE, "Use compatible behavior.", 
    &global_system_variables.old_mode,
    &max_system_variables.old_mode, 0, GET_BOOL, NO_ARG, 
    0, 0, 0, 0, 0, 0},
  {"open_files_limit", OPT_OPEN_FILES_LIMIT,
   "If this is not 0, then mysqld will use this value to reserve file "
   "descriptors to use with setrlimit(). If this value is 0 then mysqld "
   "will reserve max_connections*5 or max_connections + table_cache*2 "
   "(whichever is larger) number of files.",
   &open_files_limit, &open_files_limit, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, OS_FILE_LIMIT, 0, 1, 0},
  {"optimizer_prune_level", OPT_OPTIMIZER_PRUNE_LEVEL,
   "Controls the heuristic(s) applied during query optimization to prune "
   "less-promising partial plans from the optimizer search space. Meaning: "
   "0 - do not apply any heuristic, thus perform exhaustive search; 1 - "
   "prune plans based on number of retrieved rows.",
   &global_system_variables.optimizer_prune_level,
   &max_system_variables.optimizer_prune_level,
   0, GET_ULONG, OPT_ARG, 1, 0, 1, 0, 1, 0},
  {"optimizer_search_depth", OPT_OPTIMIZER_SEARCH_DEPTH,
   "Maximum depth of search performed by the query optimizer. Values larger "
   "than the number of relations in a query result in better query plans, "
   "but take longer to compile a query. Smaller values than the number of "
   "tables in a relation result in faster optimization, but may produce "
   "very bad query plans. If set to 0, the system will automatically pick "
   "a reasonable value; if set to MAX_TABLES+2, the optimizer will switch "
   "to the original find_best (used for testing/comparison).",
   &global_system_variables.optimizer_search_depth,
   &max_system_variables.optimizer_search_depth,
   0, GET_ULONG, OPT_ARG, MAX_TABLES+1, 0, MAX_TABLES+2, 0, 1, 0},
  {"optimizer_switch", OPT_OPTIMIZER_SWITCH,
   "optimizer_switch=option=val[,option=val...], where option={index_merge, "
   "index_merge_union, index_merge_sort_union, index_merge_intersection} and "
   "val={on, off, default}.",
   &optimizer_switch_str, &optimizer_switch_str, 0, GET_STR, REQUIRED_ARG,
   /*OPTIMIZER_SWITCH_DEFAULT*/0, 0, 0, 0, 0, 0},
  {"plugin_dir", OPT_PLUGIN_DIR,
   "Directory for plugins.",
   &opt_plugin_dir_ptr, &opt_plugin_dir_ptr, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"plugin-load", OPT_PLUGIN_LOAD,
   "Optional semicolon-separated list of plugins to load, where each plugin is "
   "identified as name=library, where name is the plugin name and library "
   "is the plugin library in plugin_dir.",
   &opt_plugin_load, &opt_plugin_load, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"preload_buffer_size", OPT_PRELOAD_BUFFER_SIZE,
   "The size of the buffer that is allocated when preloading indexes.",
   &global_system_variables.preload_buff_size,
   &max_system_variables.preload_buff_size, 0, GET_ULONG,
   REQUIRED_ARG, 32*1024L, 1024, 1024*1024*1024L, 0, 1, 0},
  {"query_alloc_block_size", OPT_QUERY_ALLOC_BLOCK_SIZE,
   "Allocation block size for query parsing and execution.",
   &global_system_variables.query_alloc_block_size,
   &max_system_variables.query_alloc_block_size, 0, GET_ULONG,
   REQUIRED_ARG, QUERY_ALLOC_BLOCK_SIZE, 1024, ULONG_MAX, 0, 1024, 0},
#ifdef HAVE_QUERY_CACHE
  {"query_cache_limit", OPT_QUERY_CACHE_LIMIT,
   "Don't cache results that are bigger than this.",
   &query_cache_limit, &query_cache_limit, 0, GET_ULONG,
   REQUIRED_ARG, 1024*1024L, 0, ULONG_MAX, 0, 1, 0},
  {"query_cache_min_res_unit", OPT_QUERY_CACHE_MIN_RES_UNIT,
   "Minimal size of unit in which space for results is allocated (last unit "
   "will be trimmed after writing all result data).",
   &query_cache_min_res_unit, &query_cache_min_res_unit,
   0, GET_ULONG, REQUIRED_ARG, QUERY_CACHE_MIN_RESULT_DATA_SIZE,
   0, ULONG_MAX, 0, 1, 0},
#endif /*HAVE_QUERY_CACHE*/
  {"query_cache_size", OPT_QUERY_CACHE_SIZE,
   "The memory allocated to store results from old queries.",
   &query_cache_size, &query_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, ULONG_MAX, 0, 1024, 0},
#ifdef HAVE_QUERY_CACHE
  {"query_cache_type", OPT_QUERY_CACHE_TYPE,
   "0 = OFF = Don't cache or retrieve results. 1 = ON = Cache all results "
   "except SELECT SQL_NO_CACHE ... queries. 2 = DEMAND = Cache only SELECT "
   "SQL_CACHE ... queries.", &global_system_variables.query_cache_type,
   &max_system_variables.query_cache_type,
   0, GET_ULONG, REQUIRED_ARG, 1, 0, 2, 0, 1, 0},
  {"query_cache_wlock_invalidate", OPT_QUERY_CACHE_WLOCK_INVALIDATE,
   "Invalidate queries in query cache on LOCK for write.",
   &global_system_variables.query_cache_wlock_invalidate,
   &max_system_variables.query_cache_wlock_invalidate,
   0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 1, 0},
#endif /*HAVE_QUERY_CACHE*/
  {"query_prealloc_size", OPT_QUERY_PREALLOC_SIZE,
   "Persistent buffer for query parsing and execution.",
   &global_system_variables.query_prealloc_size,
   &max_system_variables.query_prealloc_size, 0, GET_ULONG,
   REQUIRED_ARG, QUERY_ALLOC_PREALLOC_SIZE, QUERY_ALLOC_PREALLOC_SIZE,
   ULONG_MAX, 0, 1024, 0},
  {"range_alloc_block_size", OPT_RANGE_ALLOC_BLOCK_SIZE,
   "Allocation block size for storing ranges during optimization.",
   &global_system_variables.range_alloc_block_size,
   &max_system_variables.range_alloc_block_size, 0, GET_ULONG,
   REQUIRED_ARG, RANGE_ALLOC_BLOCK_SIZE, RANGE_ALLOC_BLOCK_SIZE, ULONG_MAX,
   0, 1024, 0},
  {"read_buffer_size", OPT_RECORD_BUFFER,
   "Each thread that does a sequential scan allocates a buffer of this size "
   "for each table it scans. If you do many sequential scans, you may want "
   "to increase this value.", &global_system_variables.read_buff_size,
   &max_system_variables.read_buff_size,0, GET_ULONG, REQUIRED_ARG,
   128*1024L, IO_SIZE*2+MALLOC_OVERHEAD, INT_MAX32, MALLOC_OVERHEAD, IO_SIZE,
   0},
  {"read_only", OPT_READONLY,
   "Make all non-temporary tables read-only, with the exception of replication "
   "(slave) threads and users with the SUPER privilege.",
   &opt_readonly,
   &opt_readonly,
   0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 1, 0},
  {"read_rnd_buffer_size", OPT_RECORD_RND_BUFFER,
   "When reading rows in sorted order after a sort, the rows are read through "
   "this buffer to avoid disk seeks. If not set, then it's set to the value of "
   "record_buffer.",
   &global_system_variables.read_rnd_buff_size,
   &max_system_variables.read_rnd_buff_size, 0,
   GET_ULONG, REQUIRED_ARG, 256*1024L, IO_SIZE*2+MALLOC_OVERHEAD,
   INT_MAX32, MALLOC_OVERHEAD, IO_SIZE, 0},
  {"record_buffer", OPT_RECORD_BUFFER_OLD,
   "Alias for read_buffer_size. This variable is deprecated and will be removed in a future release.",
   &global_system_variables.read_buff_size,
   &max_system_variables.read_buff_size,0, GET_ULONG, REQUIRED_ARG,
   128*1024L, IO_SIZE*2+MALLOC_OVERHEAD, INT_MAX32, MALLOC_OVERHEAD, IO_SIZE, 0},
#ifdef HAVE_REPLICATION
  {"relay_log_purge", OPT_RELAY_LOG_PURGE,
   "0 = do not purge relay logs. 1 = purge them as soon as they are no more needed.",
   &relay_log_purge,
   &relay_log_purge, 0, GET_BOOL, NO_ARG,
   1, 0, 1, 0, 1, 0},
  {"relay_log_space_limit", OPT_RELAY_LOG_SPACE_LIMIT,
   "Maximum space to use for all relay logs.",
   &relay_log_space_limit,
   &relay_log_space_limit, 0, GET_ULL, REQUIRED_ARG, 0L, 0L,
   ULONG_MAX, 0, 1, 0},
  {"slave_compressed_protocol", OPT_SLAVE_COMPRESSED_PROTOCOL,
   "Use compression on master/slave protocol.",
   &opt_slave_compressed_protocol,
   &opt_slave_compressed_protocol,
   0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 1, 0},
  {"slave_net_timeout", OPT_SLAVE_NET_TIMEOUT,
   "Number of seconds to wait for more data from a master/slave connection before aborting the read.",
   &slave_net_timeout, &slave_net_timeout, 0,
   GET_ULONG, REQUIRED_ARG, SLAVE_NET_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"slave_transaction_retries", OPT_SLAVE_TRANS_RETRIES,
   "Number of times the slave SQL thread will retry a transaction in case "
   "it failed with a deadlock or elapsed lock wait timeout, "
   "before giving up and stopping.",
   &slave_trans_retries, &slave_trans_retries, 0,
   GET_ULONG, REQUIRED_ARG, 10L, 0L, ULONG_MAX, 0, 1, 0},
#endif /* HAVE_REPLICATION */
  {"slow_launch_time", OPT_SLOW_LAUNCH_TIME,
   "If creating the thread takes longer than this value (in seconds), "
   "the Slow_launch_threads counter will be incremented.",
   &slow_launch_time, &slow_launch_time, 0, GET_ULONG,
   REQUIRED_ARG, 2L, 0L, LONG_TIMEOUT, 0, 1, 0},
  {"sort_buffer_size", OPT_SORT_BUFFER,
   "Each thread that needs to do a sort allocates a buffer of this size.",
   &global_system_variables.sortbuff_size,
   &max_system_variables.sortbuff_size, 0, GET_ULONG, REQUIRED_ARG,
   MAX_SORT_MEMORY, MIN_SORT_MEMORY+MALLOC_OVERHEAD*2, ~0ULL, MALLOC_OVERHEAD,
   1, 0},
  {"sync-binlog", OPT_SYNC_BINLOG,
   "Synchronously flush binary log to disk after every #th event. "
   "Use 0 (default) to disable synchronous flushing.",
   &sync_binlog_period, &sync_binlog_period, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, ULONG_MAX, 0, 1, 0},
  {"sync-frm", OPT_SYNC_FRM, "Sync .frm to disk on create. Enabled by default.",
   &opt_sync_frm, &opt_sync_frm, 0, GET_BOOL, NO_ARG, 1, 0,
   0, 0, 0, 0},
  {"table_cache", OPT_TABLE_OPEN_CACHE,
   "Deprecated; use --table_open_cache instead.",
   &table_cache_size, &table_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, TABLE_OPEN_CACHE_DEFAULT, 1, 512*1024L, 0, 1, 0},
  {"table_definition_cache", OPT_TABLE_DEF_CACHE,
   "The number of cached table definitions.",
   &table_def_size, &table_def_size,
   0, GET_ULONG, REQUIRED_ARG, TABLE_DEF_CACHE_DEFAULT, TABLE_DEF_CACHE_MIN,
   512*1024L, 0, 1, 0},
  {"table_open_cache", OPT_TABLE_OPEN_CACHE,
   "The number of cached open tables.",
   &table_cache_size, &table_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, TABLE_OPEN_CACHE_DEFAULT, 1, 512*1024L, 0, 1, 0},
  {"table_lock_wait_timeout", OPT_TABLE_LOCK_WAIT_TIMEOUT,
   "Timeout in seconds to wait for a table level lock before returning an "
   "error. Used only if the connection has active cursors.",
   &table_lock_wait_timeout, &table_lock_wait_timeout,
   0, GET_ULONG, REQUIRED_ARG, 50, 1, 1024 * 1024 * 1024, 0, 1, 0},
  {"thread_cache_size", OPT_THREAD_CACHE_SIZE,
   "How many threads we should keep in a cache for reuse.",
   &thread_cache_size, &thread_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, 16384, 0, 1, 0},
  {"thread_concurrency", OPT_THREAD_CONCURRENCY,
   "Permits the application to give the threads system a hint for the "
   "desired number of threads that should be run at the same time.",
   &concurrency, &concurrency, 0, GET_ULONG, REQUIRED_ARG,
   DEFAULT_CONCURRENCY, 1, 512, 0, 1, 0},
#if HAVE_POOL_OF_THREADS == 1
  {"thread_pool_size", OPT_THREAD_CACHE_SIZE,
   "How many threads we should create to handle query requests in case of "
   "'thread_handling=pool-of-threads'.",
   &thread_pool_size, &thread_pool_size, 0, GET_ULONG,
   REQUIRED_ARG, 20, 1, 16384, 0, 1, 0},
#endif
  {"thread_stack", OPT_THREAD_STACK,
   "The stack size for each thread.", &my_thread_stack_size,
   &my_thread_stack_size, 0, GET_ULONG, REQUIRED_ARG,DEFAULT_THREAD_STACK,
   1024L*128L, ULONG_MAX, 0, 1024, 0},
  { "time_format", OPT_TIME_FORMAT,
    "The TIME format (for future).",
    &opt_date_time_formats[MYSQL_TIMESTAMP_TIME],
    &opt_date_time_formats[MYSQL_TIMESTAMP_TIME],
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tmp_table_size", OPT_TMP_TABLE_SIZE,
   "If an internal in-memory temporary table exceeds this size, MySQL will"
   " automatically convert it to an on-disk MyISAM table.",
   &global_system_variables.tmp_table_size,
   &max_system_variables.tmp_table_size, 0, GET_ULL,
   REQUIRED_ARG, 16*1024*1024L, 1024, MAX_MEM_TABLE_SIZE, 0, 1, 0},
  {"transaction_alloc_block_size", OPT_TRANS_ALLOC_BLOCK_SIZE,
   "Allocation block size for transactions to be stored in binary log.",
   &global_system_variables.trans_alloc_block_size,
   &max_system_variables.trans_alloc_block_size, 0, GET_ULONG,
   REQUIRED_ARG, QUERY_ALLOC_BLOCK_SIZE, 1024, ULONG_MAX, 0, 1024, 0},
  {"transaction_prealloc_size", OPT_TRANS_PREALLOC_SIZE,
   "Persistent buffer for transactions to be stored in binary log.",
   &global_system_variables.trans_prealloc_size,
   &max_system_variables.trans_prealloc_size, 0, GET_ULONG,
   REQUIRED_ARG, TRANS_ALLOC_PREALLOC_SIZE, 1024, ULONG_MAX, 0, 1024, 0},
  {"thread_handling", OPT_THREAD_HANDLING,
   "Define threads usage for handling queries: "
   "one-thread-per-connection or no-threads.", 0, 0,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"updatable_views_with_limit", OPT_UPDATABLE_VIEWS_WITH_LIMIT,
   "1 = YES = Don't issue an error message (warning only) if a VIEW without "
   "presence of a key of the underlying table is used in queries with a "
   "LIMIT clause for updating. 0 = NO = Prohibit update of a VIEW, which "
   "does not contain a key of the underlying table and the query uses a "
   "LIMIT clause (usually get from GUI tools).",
   &global_system_variables.updatable_views_with_limit,
   &max_system_variables.updatable_views_with_limit,
   0, GET_ULONG, REQUIRED_ARG, 1, 0, 1, 0, 1, 0},
  {"wait_timeout", OPT_WAIT_TIMEOUT,
   "The number of seconds the server waits for activity on a connection before closing it.",
   &global_system_variables.net_wait_timeout,
   &max_system_variables.net_wait_timeout, 0, GET_ULONG,
   REQUIRED_ARG, NET_WAIT_TIMEOUT, 1, IF_WIN(INT_MAX32/1000, LONG_TIMEOUT),
   0, 1, 0},
  {"binlog-direct-non-transactional-updates", OPT_BINLOG_DIRECT_NON_TRANS_UPDATE,
   "Causes updates to non-transactional engines using statement format to be "
   "written directly to binary log. Before using this option, make sure that "
   "there are no dependencies between transactional and non-transactional "
   "tables such as in the statement INSERT INTO t_myisam SELECT * FROM "
   "t_innodb; otherwise, slaves may diverge from the master.",
   &global_system_variables.binlog_direct_non_trans_update,
   &max_system_variables.binlog_direct_non_trans_update,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static int show_queries(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONGLONG;
  var->value= (char *)&thd->query_id;
  return 0;
}


static int show_net_compression(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_MY_BOOL;
  var->value= (char *)&thd->net.compress;
  return 0;
}

static int show_starttime(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long) (thd->query_start() - server_start_time);
  return 0;
}

#ifdef COMMUNITY_SERVER
static int show_flushstatustime(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long) (thd->query_start() - flush_status_time);
  return 0;
}
#endif

#ifdef HAVE_REPLICATION
static int show_rpl_status(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_CHAR;
  var->value= const_cast<char*>(rpl_status_type[(int)rpl_status]);
  return 0;
}

static int show_slave_running(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_MY_BOOL;
  pthread_mutex_lock(&LOCK_active_mi);
  var->value= buff;
  *((my_bool *)buff)= (my_bool) (active_mi && 
                                 active_mi->slave_running == MYSQL_SLAVE_RUN_CONNECT &&
                                 active_mi->rli.slave_running);
  pthread_mutex_unlock(&LOCK_active_mi);
  return 0;
}

static int show_slave_retried_trans(THD *thd, SHOW_VAR *var, char *buff)
{
  /*
    TODO: with multimaster, have one such counter per line in
    SHOW SLAVE STATUS, and have the sum over all lines here.
  */
  pthread_mutex_lock(&LOCK_active_mi);
  if (active_mi)
  {
    var->type= SHOW_LONG;
    var->value= buff;
    pthread_mutex_lock(&active_mi->rli.data_lock);
    *((long *)buff)= (long)active_mi->rli.retried_trans;
    pthread_mutex_unlock(&active_mi->rli.data_lock);
  }
  else
    var->type= SHOW_UNDEF;
  pthread_mutex_unlock(&LOCK_active_mi);
  return 0;
}
#endif /* HAVE_REPLICATION */

static int show_open_tables(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long)cached_open_tables();
  return 0;
}

static int show_prepared_stmt_count(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  pthread_mutex_lock(&LOCK_prepared_stmt_count);
  *((long *)buff)= (long)prepared_stmt_count;
  pthread_mutex_unlock(&LOCK_prepared_stmt_count);
  return 0;
}

static int show_table_definitions(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long)cached_table_definitions();
  return 0;
}

#ifdef HAVE_OPENSSL
/* Functions relying on CTX */
static int show_ssl_ctx_sess_accept(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_sess_accept(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_accept_good(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_sess_accept_good(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_connect_good(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_sess_connect_good(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_accept_renegotiate(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_sess_accept_renegotiate(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_connect_renegotiate(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_sess_connect_renegotiate(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_cb_hits(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_sess_cb_hits(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_hits(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_sess_hits(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_cache_full(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_sess_cache_full(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_misses(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_sess_misses(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_timeouts(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_sess_timeouts(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_number(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_sess_number(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_connect(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_sess_connect(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_get_cache_size(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_sess_get_cache_size(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_get_verify_mode(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_get_verify_mode(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_get_verify_depth(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (!ssl_acceptor_fd ? 0 :
                     SSL_CTX_get_verify_depth(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_get_session_cache_mode(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_CHAR;
  if (!ssl_acceptor_fd)
    var->value= const_cast<char*>("NONE");
  else
    switch (SSL_CTX_get_session_cache_mode(ssl_acceptor_fd->ssl_context))
    {
    case SSL_SESS_CACHE_OFF:
      var->value= const_cast<char*>("OFF"); break;
    case SSL_SESS_CACHE_CLIENT:
      var->value= const_cast<char*>("CLIENT"); break;
    case SSL_SESS_CACHE_SERVER:
      var->value= const_cast<char*>("SERVER"); break;
    case SSL_SESS_CACHE_BOTH:
      var->value= const_cast<char*>("BOTH"); break;
    case SSL_SESS_CACHE_NO_AUTO_CLEAR:
      var->value= const_cast<char*>("NO_AUTO_CLEAR"); break;
    case SSL_SESS_CACHE_NO_INTERNAL_LOOKUP:
      var->value= const_cast<char*>("NO_INTERNAL_LOOKUP"); break;
    default:
      var->value= const_cast<char*>("Unknown"); break;
    }
  return 0;
}

/*
   Functions relying on SSL 
   Note: In the show_ssl_* functions, we need to check if we have a
         valid vio-object since this isn't always true, specifically
         when session_status or global_status is requested from
         inside an Event.
 */
static int show_ssl_get_version(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_CHAR;
  if( thd->vio_ok() && thd->net.vio->ssl_arg )
    var->value= const_cast<char*>(SSL_get_version((SSL*) thd->net.vio->ssl_arg));
  else
    var->value= (char *)"";
  return 0;
}

static int show_ssl_session_reused(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  if( thd->vio_ok() && thd->net.vio->ssl_arg )
    *((long *)buff)= (long)SSL_session_reused((SSL*) thd->net.vio->ssl_arg);
  else
    *((long *)buff)= 0;
  return 0;
}

static int show_ssl_get_default_timeout(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  if( thd->vio_ok() && thd->net.vio->ssl_arg )
    *((long *)buff)= (long)SSL_get_default_timeout((SSL*)thd->net.vio->ssl_arg);
  else
    *((long *)buff)= 0;
  return 0;
}

static int show_ssl_get_verify_mode(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  if( thd->net.vio && thd->net.vio->ssl_arg )
    *((long *)buff)= (long)SSL_get_verify_mode((SSL*)thd->net.vio->ssl_arg);
  else
    *((long *)buff)= 0;
  return 0;
}

static int show_ssl_get_verify_depth(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  if( thd->vio_ok() && thd->net.vio->ssl_arg )
    *((long *)buff)= (long)SSL_get_verify_depth((SSL*)thd->net.vio->ssl_arg);
  else
    *((long *)buff)= 0;
  return 0;
}

static int show_ssl_get_cipher(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_CHAR;
  if( thd->vio_ok() && thd->net.vio->ssl_arg )
    var->value= const_cast<char*>(SSL_get_cipher((SSL*) thd->net.vio->ssl_arg));
  else
    var->value= (char *)"";
  return 0;
}

static int show_ssl_get_cipher_list(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_CHAR;
  var->value= buff;
  if (thd->vio_ok() && thd->net.vio->ssl_arg)
  {
    int i;
    const char *p;
    char *end= buff + SHOW_VAR_FUNC_BUFF_SIZE;
    for (i=0; (p= SSL_get_cipher_list((SSL*) thd->net.vio->ssl_arg,i)) &&
               buff < end; i++)
    {
      buff= strnmov(buff, p, end-buff-1);
      *buff++= ':';
    }
    if (i)
      buff--;
  }
  *buff=0;
  return 0;
}

#endif /* HAVE_OPENSSL */


/*
  Variables shown by SHOW STATUS in alphabetical order
*/

SHOW_VAR status_vars[]= {
  {"Aborted_clients",          (char*) &aborted_threads,        SHOW_LONG},
  {"Aborted_connects",         (char*) &aborted_connects,       SHOW_LONG},
  {"Binlog_cache_disk_use",    (char*) &binlog_cache_disk_use,  SHOW_LONG},
  {"Binlog_cache_use",         (char*) &binlog_cache_use,       SHOW_LONG},
  {"Bytes_received",           (char*) offsetof(STATUS_VAR, bytes_received), SHOW_LONGLONG_STATUS},
  {"Bytes_sent",               (char*) offsetof(STATUS_VAR, bytes_sent), SHOW_LONGLONG_STATUS},
  {"Com",                      (char*) com_status_vars, SHOW_ARRAY},
  {"Compression",              (char*) &show_net_compression, SHOW_FUNC},
  {"Connections",              (char*) &thread_id,              SHOW_LONG_NOFLUSH},
  {"Created_tmp_disk_tables",  (char*) offsetof(STATUS_VAR, created_tmp_disk_tables), SHOW_LONG_STATUS},
  {"Created_tmp_files",	       (char*) &my_tmp_file_created,	SHOW_LONG},
  {"Created_tmp_tables",       (char*) offsetof(STATUS_VAR, created_tmp_tables), SHOW_LONG_STATUS},
  {"Delayed_errors",           (char*) &delayed_insert_errors,  SHOW_LONG},
  {"Delayed_insert_threads",   (char*) &delayed_insert_threads, SHOW_LONG_NOFLUSH},
  {"Delayed_writes",           (char*) &delayed_insert_writes,  SHOW_LONG},
  {"Flush_commands",           (char*) &refresh_version,        SHOW_LONG_NOFLUSH},
  {"Handler_commit",           (char*) offsetof(STATUS_VAR, ha_commit_count), SHOW_LONG_STATUS},
  {"Handler_delete",           (char*) offsetof(STATUS_VAR, ha_delete_count), SHOW_LONG_STATUS},
  {"Handler_discover",         (char*) offsetof(STATUS_VAR, ha_discover_count), SHOW_LONG_STATUS},
  {"Handler_prepare",          (char*) offsetof(STATUS_VAR, ha_prepare_count),  SHOW_LONG_STATUS},
  {"Handler_read_first",       (char*) offsetof(STATUS_VAR, ha_read_first_count), SHOW_LONG_STATUS},
  {"Handler_read_key",         (char*) offsetof(STATUS_VAR, ha_read_key_count), SHOW_LONG_STATUS},
  {"Handler_read_next",        (char*) offsetof(STATUS_VAR, ha_read_next_count), SHOW_LONG_STATUS},
  {"Handler_read_prev",        (char*) offsetof(STATUS_VAR, ha_read_prev_count), SHOW_LONG_STATUS},
  {"Handler_read_rnd",         (char*) offsetof(STATUS_VAR, ha_read_rnd_count), SHOW_LONG_STATUS},
  {"Handler_read_rnd_next",    (char*) offsetof(STATUS_VAR, ha_read_rnd_next_count), SHOW_LONG_STATUS},
  {"Handler_rollback",         (char*) offsetof(STATUS_VAR, ha_rollback_count), SHOW_LONG_STATUS},
  {"Handler_savepoint",        (char*) offsetof(STATUS_VAR, ha_savepoint_count), SHOW_LONG_STATUS},
  {"Handler_savepoint_rollback",(char*) offsetof(STATUS_VAR, ha_savepoint_rollback_count), SHOW_LONG_STATUS},
  {"Handler_update",           (char*) offsetof(STATUS_VAR, ha_update_count), SHOW_LONG_STATUS},
  {"Handler_write",            (char*) offsetof(STATUS_VAR, ha_write_count), SHOW_LONG_STATUS},
  {"Key_blocks_not_flushed",   (char*) offsetof(KEY_CACHE, global_blocks_changed), SHOW_KEY_CACHE_LONG},
  {"Key_blocks_unused",        (char*) offsetof(KEY_CACHE, blocks_unused), SHOW_KEY_CACHE_LONG},
  {"Key_blocks_used",          (char*) offsetof(KEY_CACHE, blocks_used), SHOW_KEY_CACHE_LONG},
  {"Key_read_requests",        (char*) offsetof(KEY_CACHE, global_cache_r_requests), SHOW_KEY_CACHE_LONGLONG},
  {"Key_reads",                (char*) offsetof(KEY_CACHE, global_cache_read), SHOW_KEY_CACHE_LONGLONG},
  {"Key_write_requests",       (char*) offsetof(KEY_CACHE, global_cache_w_requests), SHOW_KEY_CACHE_LONGLONG},
  {"Key_writes",               (char*) offsetof(KEY_CACHE, global_cache_write), SHOW_KEY_CACHE_LONGLONG},
  {"Last_query_cost",          (char*) offsetof(STATUS_VAR, last_query_cost), SHOW_DOUBLE_STATUS},
  {"Max_used_connections",     (char*) &max_used_connections,  SHOW_LONG},
  {"Not_flushed_delayed_rows", (char*) &delayed_rows_in_use,    SHOW_LONG_NOFLUSH},
  {"Open_files",               (char*) &my_file_opened,         SHOW_LONG_NOFLUSH},
  {"Open_streams",             (char*) &my_stream_opened,       SHOW_LONG_NOFLUSH},
  {"Open_table_definitions",   (char*) &show_table_definitions, SHOW_FUNC},
  {"Open_tables",              (char*) &show_open_tables,       SHOW_FUNC},
  {"Opened_files",             (char*) &my_file_total_opened, SHOW_LONG_NOFLUSH},
  {"Opened_tables",            (char*) offsetof(STATUS_VAR, opened_tables), SHOW_LONG_STATUS},
  {"Opened_table_definitions", (char*) offsetof(STATUS_VAR, opened_shares), SHOW_LONG_STATUS},
  {"Prepared_stmt_count",      (char*) &show_prepared_stmt_count, SHOW_FUNC},
#ifdef HAVE_QUERY_CACHE
  {"Qcache_free_blocks",       (char*) &query_cache.free_memory_blocks, SHOW_LONG_NOFLUSH},
  {"Qcache_free_memory",       (char*) &query_cache.free_memory, SHOW_LONG_NOFLUSH},
  {"Qcache_hits",              (char*) &query_cache.hits,       SHOW_LONG},
  {"Qcache_inserts",           (char*) &query_cache.inserts,    SHOW_LONG},
  {"Qcache_lowmem_prunes",     (char*) &query_cache.lowmem_prunes, SHOW_LONG},
  {"Qcache_not_cached",        (char*) &query_cache.refused,    SHOW_LONG},
  {"Qcache_queries_in_cache",  (char*) &query_cache.queries_in_cache, SHOW_LONG_NOFLUSH},
  {"Qcache_total_blocks",      (char*) &query_cache.total_blocks, SHOW_LONG_NOFLUSH},
#endif /*HAVE_QUERY_CACHE*/
  {"Queries",                  (char*) &show_queries,            SHOW_FUNC},
  {"Questions",                (char*) offsetof(STATUS_VAR, questions), SHOW_LONG_STATUS},
#ifdef HAVE_REPLICATION
  {"Rpl_status",               (char*) &show_rpl_status,          SHOW_FUNC},
#endif
  {"Select_full_join",         (char*) offsetof(STATUS_VAR, select_full_join_count), SHOW_LONG_STATUS},
  {"Select_full_range_join",   (char*) offsetof(STATUS_VAR, select_full_range_join_count), SHOW_LONG_STATUS},
  {"Select_range",             (char*) offsetof(STATUS_VAR, select_range_count), SHOW_LONG_STATUS},
  {"Select_range_check",       (char*) offsetof(STATUS_VAR, select_range_check_count), SHOW_LONG_STATUS},
  {"Select_scan",	       (char*) offsetof(STATUS_VAR, select_scan_count), SHOW_LONG_STATUS},
  {"Slave_open_temp_tables",   (char*) &slave_open_temp_tables, SHOW_LONG},
#ifdef HAVE_REPLICATION
  {"Slave_retried_transactions",(char*) &show_slave_retried_trans, SHOW_FUNC},
  {"Slave_running",            (char*) &show_slave_running,     SHOW_FUNC},
#endif
  {"Slow_launch_threads",      (char*) &slow_launch_threads,    SHOW_LONG},
  {"Slow_queries",             (char*) offsetof(STATUS_VAR, long_query_count), SHOW_LONG_STATUS},
  {"Sort_merge_passes",	       (char*) offsetof(STATUS_VAR, filesort_merge_passes), SHOW_LONG_STATUS},
  {"Sort_range",	       (char*) offsetof(STATUS_VAR, filesort_range_count), SHOW_LONG_STATUS},
  {"Sort_rows",		       (char*) offsetof(STATUS_VAR, filesort_rows), SHOW_LONG_STATUS},
  {"Sort_scan",		       (char*) offsetof(STATUS_VAR, filesort_scan_count), SHOW_LONG_STATUS},
#ifdef HAVE_OPENSSL
  {"Ssl_accept_renegotiates",  (char*) &show_ssl_ctx_sess_accept_renegotiate, SHOW_FUNC},
  {"Ssl_accepts",              (char*) &show_ssl_ctx_sess_accept, SHOW_FUNC},
  {"Ssl_callback_cache_hits",  (char*) &show_ssl_ctx_sess_cb_hits, SHOW_FUNC},
  {"Ssl_cipher",               (char*) &show_ssl_get_cipher, SHOW_FUNC},
  {"Ssl_cipher_list",          (char*) &show_ssl_get_cipher_list, SHOW_FUNC},
  {"Ssl_client_connects",      (char*) &show_ssl_ctx_sess_connect, SHOW_FUNC},
  {"Ssl_connect_renegotiates", (char*) &show_ssl_ctx_sess_connect_renegotiate, SHOW_FUNC},
  {"Ssl_ctx_verify_depth",     (char*) &show_ssl_ctx_get_verify_depth, SHOW_FUNC},
  {"Ssl_ctx_verify_mode",      (char*) &show_ssl_ctx_get_verify_mode, SHOW_FUNC},
  {"Ssl_default_timeout",      (char*) &show_ssl_get_default_timeout, SHOW_FUNC},
  {"Ssl_finished_accepts",     (char*) &show_ssl_ctx_sess_accept_good, SHOW_FUNC},
  {"Ssl_finished_connects",    (char*) &show_ssl_ctx_sess_connect_good, SHOW_FUNC},
  {"Ssl_session_cache_hits",   (char*) &show_ssl_ctx_sess_hits, SHOW_FUNC},
  {"Ssl_session_cache_misses", (char*) &show_ssl_ctx_sess_misses, SHOW_FUNC},
  {"Ssl_session_cache_mode",   (char*) &show_ssl_ctx_get_session_cache_mode, SHOW_FUNC},
  {"Ssl_session_cache_overflows", (char*) &show_ssl_ctx_sess_cache_full, SHOW_FUNC},
  {"Ssl_session_cache_size",   (char*) &show_ssl_ctx_sess_get_cache_size, SHOW_FUNC},
  {"Ssl_session_cache_timeouts", (char*) &show_ssl_ctx_sess_timeouts, SHOW_FUNC},
  {"Ssl_sessions_reused",      (char*) &show_ssl_session_reused, SHOW_FUNC},
  {"Ssl_used_session_cache_entries",(char*) &show_ssl_ctx_sess_number, SHOW_FUNC},
  {"Ssl_verify_depth",         (char*) &show_ssl_get_verify_depth, SHOW_FUNC},
  {"Ssl_verify_mode",          (char*) &show_ssl_get_verify_mode, SHOW_FUNC},
  {"Ssl_version",              (char*) &show_ssl_get_version, SHOW_FUNC},
#endif /* HAVE_OPENSSL */
  {"Table_locks_immediate",    (char*) &locks_immediate,        SHOW_LONG},
  {"Table_locks_waited",       (char*) &locks_waited,           SHOW_LONG},
#ifdef HAVE_MMAP
  {"Tc_log_max_pages_used",    (char*) &tc_log_max_pages_used,  SHOW_LONG},
  {"Tc_log_page_size",         (char*) &tc_log_page_size,       SHOW_LONG},
  {"Tc_log_page_waits",        (char*) &tc_log_page_waits,      SHOW_LONG},
#endif
  {"Threads_cached",           (char*) &cached_thread_count,    SHOW_LONG_NOFLUSH},
  {"Threads_connected",        (char*) &thread_count,           SHOW_INT},
  {"Threads_created",	       (char*) &thread_created,		SHOW_LONG_NOFLUSH},
  {"Threads_running",          (char*) &thread_running,         SHOW_INT},
  {"Uptime",                   (char*) &show_starttime,         SHOW_FUNC},
#ifdef COMMUNITY_SERVER
  {"Uptime_since_flush_status",(char*) &show_flushstatustime,   SHOW_FUNC},
#endif
  {NullS, NullS, SHOW_LONG}
};

#ifndef EMBEDDED_LIBRARY
static void print_version(void)
{
  set_server_version();
  /*
    Note: the instance manager keys off the string 'Ver' so it can find the
    version from the output of 'mysqld --version', so don't change it!
  */
  printf("%s  Ver %s for %s on %s (%s)\n",my_progname,
	 server_version,SYSTEM_TYPE,MACHINE_TYPE, MYSQL_COMPILATION_COMMENT);
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
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
  puts("Starts the MySQL database server.\n");
  printf("Usage: %s [OPTIONS]\n", my_progname);
  if (!opt_verbose)
    puts("\nFor more help options (several pages), use mysqld --verbose --help.");
  else
  {
#ifdef __WIN__
  puts("NT and Win32 specific options:\n\
  --install                     Install the default service (NT).\n\
  --install-manual              Install the default service started manually (NT).\n\
  --install service_name        Install an optional service (NT).\n\
  --install-manual service_name Install an optional service started manually (NT).\n\
  --remove                      Remove the default service from the service list (NT).\n\
  --remove service_name         Remove the service_name from the service list (NT).\n\
  --enable-named-pipe           Only to be used for the default server (NT).\n\
  --standalone                  Dummy option to start as a standalone server (NT).\
");
  puts("");
#endif
  print_defaults(MYSQL_CONFIG_NAME,load_default_groups);
  puts("");
  set_ports();

  /* Print out all the options including plugin supplied options */
  my_print_help_inc_plugins(my_long_options, sizeof(my_long_options)/sizeof(my_option));

  if (! plugins_are_initialized)
  {
    puts("\n\
Plugins have parameters that are not reflected in this list\n\
because execution stopped before plugins were initialized.");
  }

  puts("\n\
To see what values a running MySQL server is using, type\n\
'mysqladmin variables' instead of 'mysqld --verbose --help'.");
  }
}
#endif /*!EMBEDDED_LIBRARY*/


/**
  Initialize all MySQL global variables to default values.

  We don't need to set numeric variables refered to in my_long_options
  as these are initialized by my_getopt.

  @note
    The reason to set a lot of global variables to zero is to allow one to
    restart the embedded server with a clean environment
    It's also needed on some exotic platforms where global variables are
    not set to 0 when a program starts.

    We don't need to set numeric variables refered to in my_long_options
    as these are initialized by my_getopt.
*/

static int mysql_init_variables(void)
{
  int error;
  /* Things reset to zero */
  opt_skip_slave_start= opt_reckless_slave = 0;
  mysql_home[0]= pidfile_name[0]= log_error_file[0]= 0;
  myisam_test_invalid_symlink= test_if_data_home_dir;
  opt_log= opt_slow_log= 0;
  opt_update_log= 0;
  log_output_options= find_bit_type(log_output_str, &log_output_typelib);
  opt_bin_log= 0;
  opt_disable_networking= opt_skip_show_db=0;
  opt_skip_name_resolve= 0;
  opt_ignore_builtin_innodb= 0;
  opt_logname= opt_update_logname= opt_binlog_index_name= opt_slow_logname= 0;
  opt_tc_log_file= (char *)"tc.log";      // no hostname in tc_log file name !
  opt_secure_auth= 0;
  opt_secure_file_priv= 0;
  opt_bootstrap= opt_myisam_log= 0;
  mqh_used= 0;
  kill_in_progress= 0;
  cleanup_done= 0;
  defaults_argc= 0;
  defaults_argv= 0;
  server_id_supplied= 0;
  test_flags= select_errors= dropping_tables= ha_open_options=0;
  thread_count= thread_running= kill_cached_threads= wake_thread=0;
  slave_open_temp_tables= 0;
  cached_thread_count= 0;
  opt_endinfo= using_udf_functions= 0;
  opt_using_transactions= 0;
  abort_loop= select_thread_in_use= signal_thread_in_use= 0;
  ready_to_exit= shutdown_in_progress= grant_option= 0;
  aborted_threads= aborted_connects= 0;
  delayed_insert_threads= delayed_insert_writes= delayed_rows_in_use= 0;
  delayed_insert_errors= thread_created= 0;
  specialflag= 0;
  binlog_cache_use=  binlog_cache_disk_use= 0;
  max_used_connections= slow_launch_threads = 0;
  mysqld_user= mysqld_chroot= opt_init_file= opt_bin_logname = 0;
  prepared_stmt_count= 0;
  errmesg= 0;
  mysqld_unix_port= opt_mysql_tmpdir= my_bind_addr_str= NullS;
  bzero((uchar*) &mysql_tmpdir_list, sizeof(mysql_tmpdir_list));
  bzero((char *) &global_status_var, sizeof(global_status_var));
  opt_large_pages= 0;
#if defined(ENABLED_DEBUG_SYNC)
  opt_debug_sync_timeout= 0;
#endif /* defined(ENABLED_DEBUG_SYNC) */
  key_map_full.set_all();

  /* Character sets */
  system_charset_info= &my_charset_utf8_general_ci;
  files_charset_info= &my_charset_utf8_general_ci;
  national_charset_info= &my_charset_utf8_general_ci;
  table_alias_charset= &my_charset_bin;
  character_set_filesystem= &my_charset_bin;

  opt_date_time_formats[0]= opt_date_time_formats[1]= opt_date_time_formats[2]= 0;

  /* Things with default values that are not zero */
  delay_key_write_options= (uint) DELAY_KEY_WRITE_ON;
  slave_exec_mode_options= find_bit_type_or_exit(slave_exec_mode_str,
                                                 &slave_exec_mode_typelib,
                                                 NULL, &error);
  /* Default mode string must not yield a error. */
  DBUG_ASSERT(!error);
  if (error)
    return 1;
  opt_specialflag= SPECIAL_ENGLISH;
  unix_sock= ip_sock= INVALID_SOCKET;
  mysql_home_ptr= mysql_home;
  pidfile_name_ptr= pidfile_name;
  log_error_file_ptr= log_error_file;
  language_ptr= language;
  mysql_data_home= mysql_real_data_home;
  thd_startup_options= (OPTION_AUTO_IS_NULL | OPTION_BIN_LOG |
                        OPTION_QUOTE_SHOW_CREATE | OPTION_SQL_NOTES);
  protocol_version= PROTOCOL_VERSION;
  what_to_log= ~ (1L << (uint) COM_TIME);
  refresh_version= 1L;	/* Increments on each reload */
  global_query_id= thread_id= 1L;
  strmov(server_version, MYSQL_SERVER_VERSION);
  myisam_recover_options_str= sql_mode_str= "OFF";
  myisam_stats_method_str= "nulls_unequal";
  my_bind_addr = htonl(INADDR_ANY);
  threads.empty();
  thread_cache.empty();
  key_caches.empty();
  if (!(dflt_key_cache= get_or_create_key_cache(default_key_cache_base.str,
                                                default_key_cache_base.length)))
  {
    sql_print_error("Cannot allocate the keycache");
    return 1;
  }
  /* set key_cache_hash.default_value = dflt_key_cache */
  multi_keycache_init();

  /* Set directory paths */
  strmake(language, LANGUAGE, sizeof(language)-1);
  strmake(mysql_real_data_home, get_relative_path(MYSQL_DATADIR),
	  sizeof(mysql_real_data_home)-1);
  mysql_data_home_buff[0]=FN_CURLIB;	// all paths are relative from here
  mysql_data_home_buff[1]=0;
  mysql_data_home_len= 2;

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
  default_collation_name= compiled_default_collation_name;
  sys_charset_system.value= (char*) system_charset_info->csname;
  character_set_filesystem_name= (char*) "binary";
  lc_time_names_name= (char*) "en_US";
  /* Set default values for some option variables */
  default_storage_engine_str= (char*) "MyISAM";
  global_system_variables.table_plugin= NULL;
  global_system_variables.tx_isolation= ISO_REPEATABLE_READ;
  global_system_variables.select_limit= (ulonglong) HA_POS_ERROR;
  max_system_variables.select_limit=    (ulonglong) HA_POS_ERROR;
  global_system_variables.max_join_size= (ulonglong) HA_POS_ERROR;
  max_system_variables.max_join_size=   (ulonglong) HA_POS_ERROR;
  global_system_variables.old_passwords= 0;
  global_system_variables.old_alter_table= 0;
  global_system_variables.binlog_format= BINLOG_FORMAT_UNSPEC;
  /*
    Default behavior for 4.1 and 5.0 is to treat NULL values as unequal
    when collecting index statistics for MyISAM tables.
  */
  global_system_variables.myisam_stats_method= MI_STATS_METHOD_NULLS_NOT_EQUAL;
  
  global_system_variables.optimizer_switch= OPTIMIZER_SWITCH_DEFAULT;
  /* Variables that depends on compile options */
#ifndef DBUG_OFF
  default_dbug_option=IF_WIN("d:t:i:O,\\mysqld.trace",
			     "d:t:i:o,/tmp/mysqld.trace");
#endif
  opt_error_log= IF_WIN(1,0);
#ifdef COMMUNITY_SERVER
    have_community_features = SHOW_OPTION_YES;
#else
    have_community_features = SHOW_OPTION_NO;
#endif
  global_system_variables.ndb_index_stat_enable=FALSE;
  max_system_variables.ndb_index_stat_enable=TRUE;
  global_system_variables.ndb_index_stat_cache_entries=32;
  max_system_variables.ndb_index_stat_cache_entries=~0L;
  global_system_variables.ndb_index_stat_update_freq=20;
  max_system_variables.ndb_index_stat_update_freq=~0L;
#ifdef HAVE_OPENSSL
  have_ssl=SHOW_OPTION_YES;
#else
  have_ssl=SHOW_OPTION_NO;
#endif
#ifdef HAVE_BROKEN_REALPATH
  have_symlink=SHOW_OPTION_NO;
#else
  have_symlink=SHOW_OPTION_YES;
#endif
#ifdef HAVE_DLOPEN
  have_dlopen=SHOW_OPTION_YES;
#else
  have_dlopen=SHOW_OPTION_NO;
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
#if defined __WIN__
	char executing_path_name[LIBLEN];
	if (!test_if_hard_path(my_progname))
	{
		// we don't want to use GetModuleFileName inside of my_path since
		// my_path is a generic path dereferencing function and here we care
		// only about the executing binary.
		GetModuleFileName(NULL, executing_path_name, sizeof(executing_path_name));
		my_path(prg_dev, executing_path_name, NULL);
	}
	else
#endif
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
  return 0;
}


my_bool
mysqld_get_one_option(int optid,
                      const struct my_option *opt __attribute__((unused)),
                      char *argument)
{
  int error;

  switch(optid) {
  case '#':
#ifndef DBUG_OFF
    DBUG_SET_INITIAL(argument ? argument : default_dbug_option);
#endif
    opt_endinfo=1;				/* unireg: memory allocation */
    break;
  case '0':
    WARN_DEPRECATED(NULL, VER_CELOSIA, "--log-long-format", "--log-short-format");
    break;
  case 'a':
    global_system_variables.sql_mode= fix_sql_mode(MODE_ANSI);
    global_system_variables.tx_isolation= ISO_SERIALIZABLE;
    break;
  case 'b':
    strmake(mysql_home,argument,sizeof(mysql_home)-1);
    break;
  case OPT_DEFAULT_CHARACTER_SET_OLD: // --default-character-set
    WARN_DEPRECATED(NULL, VER_CELOSIA, 
                    "--default-character-set",
                    "--character-set-server");
    /* Fall through */
  case 'C':
    if (default_collation_name == compiled_default_collation_name)
      default_collation_name= 0;
    break;
  case 'l':
    WARN_DEPRECATED(NULL, "7.0", "--log", "'--general_log'/'--general_log_file'");
    opt_log=1;
    break;
  case 'h':
    strmake(mysql_real_data_home,argument, sizeof(mysql_real_data_home)-1);
    /* Correct pointer set by my_getopt (for embedded library) */
    mysql_data_home= mysql_real_data_home;
    mysql_data_home_len= strlen(mysql_data_home);
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
  case 'O':
    WARN_DEPRECATED(NULL, VER_CELOSIA, "--set-variable", "--variable-name=value");
    break;
#ifdef HAVE_REPLICATION
  case OPT_SLAVE_SKIP_ERRORS:
    init_slave_skip_errors(argument);
    break;
  case OPT_SLAVE_EXEC_MODE:
    slave_exec_mode_options= find_bit_type_or_exit(argument,
                                                   &slave_exec_mode_typelib,
                                                   "", &error);
    if (error)
      return 1;
    break;
#endif
  case OPT_SAFEMALLOC_MEM_LIMIT:
#if !defined(DBUG_OFF) && defined(SAFEMALLOC)
    sf_malloc_mem_limit = atoi(argument);
#endif
    break;
#include <sslopt-case.h>
#ifndef EMBEDDED_LIBRARY
  case 'V':
    print_version();
    exit(0);
#endif /*EMBEDDED_LIBRARY*/
  case OPT_WARNINGS:
    WARN_DEPRECATED(NULL, VER_CELOSIA, "--warnings", "--log-warnings");
    /* Note: fall-through to 'W' */
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
    opt_endinfo=1;
    break;
  case (int) OPT_DEFAULT_COLLATION_OLD:
    WARN_DEPRECATED(NULL, VER_CELOSIA, "--default-collation", "--collation-server");
    break;
  case (int) OPT_SAFE_SHOW_DB:
    WARN_DEPRECATED(NULL, VER_CELOSIA, "--safe-show-database", "GRANT SHOW DATABASES");
    break;
  case (int) OPT_LOG_BIN_TRUST_FUNCTION_CREATORS_OLD:
    WARN_DEPRECATED(NULL, VER_CELOSIA, "--log-bin-trust-routine-creators", "--log-bin-trust-function-creators");
    break;
  case (int) OPT_ENABLE_LOCK:
    WARN_DEPRECATED(NULL, VER_CELOSIA, "--enable-locking", "--external-locking");
    break;
  case (int) OPT_BIG_TABLES:
    thd_startup_options|=OPTION_BIG_TABLES;
    break;
  case (int) OPT_IGNORE_BUILTIN_INNODB:
    opt_ignore_builtin_innodb= 1;
    break;
  case (int) OPT_ISAM_LOG:
    opt_myisam_log=1;
    break;
  case (int) OPT_UPDATE_LOG:
    WARN_DEPRECATED(NULL, VER_CELOSIA, "--log-update", "--log-bin");
    opt_update_log=1;
    break;
  case (int) OPT_BIN_LOG:
    opt_bin_log= test(argument != disabled_my_option);
    break;
  case (int) OPT_ERROR_LOG_FILE:
    opt_error_log= 1;
    break;
#ifdef HAVE_REPLICATION
  case (int) OPT_INIT_RPL_ROLE:
  {
    int role;
    role= find_type_or_exit(argument, &rpl_role_typelib, opt->name);
    rpl_status = (role == 1) ?  RPL_AUTH_MASTER : RPL_IDLE_SLAVE;
    break;
  }
  case (int)OPT_REPLICATE_IGNORE_DB:
  {
    rpl_filter->add_ignore_db(argument);
    break;
  }
  case (int)OPT_REPLICATE_DO_DB:
  {
    rpl_filter->add_do_db(argument);
    break;
  }
  case (int)OPT_REPLICATE_REWRITE_DB:
  {
    char* key = argument,*p, *val;

    if (!(p= strstr(argument, "->")))
    {
      sql_print_error("Bad syntax in replicate-rewrite-db - missing '->'!\n");
      return 1;
    }
    val= p--;
    while (my_isspace(mysqld_charset, *p) && p > argument)
      *p-- = 0;
    if (p == argument)
    {
      sql_print_error("Bad syntax in replicate-rewrite-db - empty FROM db!\n");
      return 1;
    }
    *val= 0;
    val+= 2;
    while (*val && my_isspace(mysqld_charset, *val))
      val++;
    if (!*val)
    {
      sql_print_error("Bad syntax in replicate-rewrite-db - empty TO db!\n");
      return 1;
    }

    rpl_filter->add_db_rewrite(key, val);
    break;
  }

  case (int)OPT_BINLOG_IGNORE_DB:
  {
    binlog_filter->add_ignore_db(argument);
    break;
  }
  case OPT_BINLOG_FORMAT:
  {
    int id;
    id= find_type_or_exit(argument, &binlog_format_typelib, opt->name);
    global_system_variables.binlog_format= opt_binlog_format_id= id - 1;
    break;
  }
  case (int)OPT_BINLOG_DO_DB:
  {
    binlog_filter->add_do_db(argument);
    break;
  }
  case (int)OPT_REPLICATE_DO_TABLE:
  {
    if (rpl_filter->add_do_table(argument))
    {
      sql_print_error("Could not add do table rule '%s'!\n", argument);
      return 1;
    }
    break;
  }
  case (int)OPT_REPLICATE_WILD_DO_TABLE:
  {
    if (rpl_filter->add_wild_do_table(argument))
    {
      sql_print_error("Could not add do table rule '%s'!\n", argument);
      return 1;
    }
    break;
  }
  case (int)OPT_REPLICATE_WILD_IGNORE_TABLE:
  {
    if (rpl_filter->add_wild_ignore_table(argument))
    {
      sql_print_error("Could not add ignore table rule '%s'!\n", argument);
      return 1;
    }
    break;
  }
  case (int)OPT_REPLICATE_IGNORE_TABLE:
  {
    if (rpl_filter->add_ignore_table(argument))
    {
      sql_print_error("Could not add ignore table rule '%s'!\n", argument);
      return 1;
    }
    break;
  }
#endif /* HAVE_REPLICATION */
  case (int) OPT_SLOW_QUERY_LOG:
    WARN_DEPRECATED(NULL, "7.0", "--log_slow_queries", "'--slow_query_log'/'--slow_query_log_file'");
    opt_slow_log= 1;
    break;
#ifdef WITH_CSV_STORAGE_ENGINE
  case  OPT_LOG_OUTPUT:
  {
    if (!argument || !argument[0])
    {
      log_output_options= LOG_FILE;
      log_output_str= log_output_typelib.type_names[1];
    }
    else
    {
      log_output_str= argument;
      log_output_options=
        find_bit_type_or_exit(argument, &log_output_typelib, opt->name, &error);
      if (error)
        return 1;
  }
    break;
  }
#endif
  case OPT_EVENT_SCHEDULER:
#ifndef HAVE_EVENT_SCHEDULER
    sql_perror("Event scheduler is not supported in embedded build.");
#else
    if (Events::set_opt_event_scheduler(argument))
      return 1;
#endif
    break;
  case (int) OPT_SKIP_NEW:
    opt_specialflag|= SPECIAL_NO_NEW_FUNC;
    delay_key_write_options= (uint) DELAY_KEY_WRITE_NONE;
    myisam_concurrent_insert=0;
    myisam_recover_options= HA_RECOVER_NONE;
    sp_automatic_privileges=0;
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
    sql_print_warning("The --skip-thread-priority startup option is deprecated "
                      "and will be removed in MySQL 7.0. MySQL 6.0 and up do not "
                      "give threads different priorities.");
    break;
  case (int) OPT_SKIP_LOCK:
    WARN_DEPRECATED(NULL, VER_CELOSIA, "--skip-locking", "--skip-external-locking");
    opt_external_locking=0;
    break;
  case (int) OPT_SQL_BIN_UPDATE_SAME:
    WARN_DEPRECATED(NULL, VER_CELOSIA, "--sql-bin-update-same", "the binary log");
    break;
  case (int) OPT_RECORD_BUFFER_OLD:
    WARN_DEPRECATED(NULL, VER_CELOSIA, "record_buffer", "read_buffer_size");
    break;
  case (int) OPT_SYMBOLIC_LINKS:
    WARN_DEPRECATED(NULL, VER_CELOSIA, "--use-symbolic-links", "--symbolic-links");
    break;
  case (int) OPT_SKIP_HOST_CACHE:
    opt_specialflag|= SPECIAL_NO_HOST_CACHE;
    break;
  case (int) OPT_SKIP_RESOLVE:
    opt_skip_name_resolve= 1;
    opt_specialflag|=SPECIAL_NO_RESOLVE;
    break;
  case (int) OPT_SKIP_NETWORKING:
#if defined(__NETWARE__)
    sql_perror("Can't start server: skip-networking option is currently not supported on NetWare");
    return 1;
#endif
    opt_disable_networking=1;
    mysqld_port=0;
    break;
  case (int) OPT_SKIP_SHOW_DB:
    opt_skip_show_db=1;
    opt_specialflag|=SPECIAL_SKIP_SHOW_DB;
    break;
  case (int) OPT_WANT_CORE:
    test_flags |= TEST_CORE_ON_SIGNAL;
    break;
  case (int) OPT_SKIP_STACK_TRACE:
    test_flags|=TEST_NO_STACKTRACE;
    break;
  case (int) OPT_SKIP_SYMLINKS:
    WARN_DEPRECATED(NULL, VER_CELOSIA, "--skip-symlink", "--skip-symbolic-links");
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
          return 1;
	}
	ent=gethostbyname(myhostname);
      }
      if (!ent)
      {
	sql_perror("Can't start server: cannot resolve hostname!");
        return 1;
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
  /*
    The following change issues a deprecation warning if the slave
    configuration is specified either in the my.cnf file or on
    the command-line. See BUG#21490.
  */
  case OPT_MASTER_HOST:
  case OPT_MASTER_USER:
  case OPT_MASTER_PASSWORD:
  case OPT_MASTER_PORT:
  case OPT_MASTER_CONNECT_RETRY:
  case OPT_MASTER_SSL:          
  case OPT_MASTER_SSL_KEY:
  case OPT_MASTER_SSL_CERT:       
  case OPT_MASTER_SSL_CAPATH:
  case OPT_MASTER_SSL_CIPHER:
  case OPT_MASTER_SSL_CA:
    if (!slave_warning_issued)                 //only show the warning once
    {
      slave_warning_issued = true;   
      WARN_DEPRECATED(NULL, "6.0", "for replication startup options", 
        "'CHANGE MASTER'");
    }
    break;
  case OPT_CONSOLE:
    if (opt_console)
      opt_error_log= 0;			// Force logs to stdout
    break;
  case (int) OPT_FLUSH:
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
  case OPT_SERVER_ID:
    server_id_supplied = 1;
    break;
  case OPT_DELAY_KEY_WRITE_ALL:
    WARN_DEPRECATED(NULL, VER_CELOSIA, 
                    "--delay-key-write-for-all-tables",
                    "--delay-key-write=ALL");
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
      type= find_type_or_exit(argument, &delay_key_write_typelib, opt->name);
      delay_key_write_options= (uint) type-1;
    }
    break;
  case OPT_MYISAM_MAX_EXTRA_SORT_FILE_SIZE:
    sql_print_warning("--myisam_max_extra_sort_file_size is deprecated and "
                      "does nothing in this version.  It will be removed in "
                      "a future release.");
    break;
  case OPT_CHARSETS_DIR:
    strmake(mysql_charsets_dir, argument, sizeof(mysql_charsets_dir)-1);
    charsets_dir = mysql_charsets_dir;
    break;
  case OPT_TX_ISOLATION:
  {
    int type;
    type= find_type_or_exit(argument, &tx_isolation_typelib, opt->name);
    global_system_variables.tx_isolation= (type-1);
    break;
  }
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
  case OPT_NDB_MGMD:
  case OPT_NDB_NODEID:
  {
    int len= my_snprintf(opt_ndb_constrbuf+opt_ndb_constrbuf_len,
			 sizeof(opt_ndb_constrbuf)-opt_ndb_constrbuf_len,
			 "%s%s%s",opt_ndb_constrbuf_len > 0 ? ",":"",
			 optid == OPT_NDB_NODEID ? "nodeid=" : "",
			 argument);
    opt_ndb_constrbuf_len+= len;
  }
  /* fall through to add the connectstring to the end
   * and set opt_ndbcluster_connectstring
   */
  case OPT_NDB_CONNECTSTRING:
    if (opt_ndb_connectstring && opt_ndb_connectstring[0])
      my_snprintf(opt_ndb_constrbuf+opt_ndb_constrbuf_len,
		  sizeof(opt_ndb_constrbuf)-opt_ndb_constrbuf_len,
		  "%s%s", opt_ndb_constrbuf_len > 0 ? ",":"",
		  opt_ndb_connectstring);
    else
      opt_ndb_constrbuf[opt_ndb_constrbuf_len]= 0;
    opt_ndbcluster_connectstring= opt_ndb_constrbuf;
    break;
  case OPT_NDB_DISTRIBUTION:
    int id;
    id= find_type_or_exit(argument, &ndb_distribution_typelib, opt->name);
    opt_ndb_distribution_id= (enum ndb_distribution)(id-1);
    break;
  case OPT_NDB_EXTRA_LOGGING:
    if (!argument)
      ndb_extra_logging++;
    else if (argument == disabled_my_option)
      ndb_extra_logging= 0L;
    else
      ndb_extra_logging= atoi(argument);
    break;
#endif
  case OPT_MYISAM_RECOVER:
  {
    if (!argument)
    {
      myisam_recover_options=    HA_RECOVER_DEFAULT;
      myisam_recover_options_str= myisam_recover_typelib.type_names[0];
    }
    else if (!argument[0])
    {
      myisam_recover_options= HA_RECOVER_NONE;
      myisam_recover_options_str= "OFF";
    }
    else
    {
      myisam_recover_options_str=argument;
      myisam_recover_options=
        find_bit_type_or_exit(argument, &myisam_recover_typelib, opt->name,
                              &error);
      if (error)
        return 1;
    }
    ha_open_options|=HA_OPEN_ABORT_IF_CRASHED;
    break;
  }
  case OPT_CONCURRENT_INSERT:
    /* The following code is mainly here to emulate old behavior */
    if (!argument)                      /* --concurrent-insert */
      myisam_concurrent_insert= 1;
    else if (argument == disabled_my_option)
      myisam_concurrent_insert= 0;      /* --skip-concurrent-insert */
    break;
  case OPT_TC_HEURISTIC_RECOVER:
    tc_heuristic_recover= find_type_or_exit(argument,
                                            &tc_heuristic_recover_typelib,
                                            opt->name);
    break;
  case OPT_MYISAM_STATS_METHOD:
  {
    ulong method_conv;
    int method;
    LINT_INIT(method_conv);

    myisam_stats_method_str= argument;
    method= find_type_or_exit(argument, &myisam_stats_method_typelib,
                              opt->name);
    switch (method-1) {
    case 2:
      method_conv= MI_STATS_METHOD_IGNORE_NULLS;
      break;
    case 1:
      method_conv= MI_STATS_METHOD_NULLS_EQUAL;
      break;
    case 0:
    default:
      method_conv= MI_STATS_METHOD_NULLS_NOT_EQUAL;
      break;
    }
    global_system_variables.myisam_stats_method= method_conv;
    break;
  }
  case OPT_SQL_MODE:
  {
    sql_mode_str= argument;
    global_system_variables.sql_mode=
      find_bit_type_or_exit(argument, &sql_mode_typelib, opt->name, &error);
    if (error)
      return 1;
    global_system_variables.sql_mode= fix_sql_mode(global_system_variables.
						   sql_mode);
    break;
  }
  case OPT_OPTIMIZER_SWITCH:
  {
    bool not_used;
    char *error= 0;
    uint error_len= 0;
    optimizer_switch_str= argument;
    global_system_variables.optimizer_switch=
      (ulong)find_set_from_flags(&optimizer_switch_typelib, 
                                 optimizer_switch_typelib.count, 
                                 global_system_variables.optimizer_switch,
                                 global_system_variables.optimizer_switch,
                                 argument, strlen(argument), NULL,
                                 &error, &error_len, &not_used);
     if (error)
     {
       char buf[512];
       char *cbuf= buf;
       cbuf += my_snprintf(buf, 512, "Error in parsing optimizer_switch setting near %*s\n", error_len, error);
       sql_perror(buf);
       return 1;
     }
    break;
  }
  case OPT_ONE_THREAD:
    global_system_variables.thread_handling=
      SCHEDULER_ONE_THREAD_PER_CONNECTION;
    break;
  case OPT_THREAD_HANDLING:
  {
    global_system_variables.thread_handling=
      find_type_or_exit(argument, &thread_handling_typelib, opt->name)-1;
    break;
  }
  case OPT_FT_BOOLEAN_SYNTAX:
    if (ft_boolean_check_syntax_string((uchar*) argument))
    {
      sql_print_error("Invalid ft-boolean-syntax string: %s\n", argument);
      return 1;
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
#ifdef HAVE_STACK_TRACE_ON_SEGV
  case OPT_DO_PSTACK:
    sql_print_warning("'--enable-pstack' is deprecated and will be removed "
                      "in a future release. A symbolic stack trace will be "
                      "printed after a crash whenever possible.");
    break;
#endif
#if defined(ENABLED_DEBUG_SYNC)
  case OPT_DEBUG_SYNC_TIMEOUT:
    /*
      Debug Sync Facility. See debug_sync.cc.
      Default timeout for WAIT_FOR action.
      Default value is zero (facility disabled).
      If option is given without an argument, supply a non-zero value.
    */
    if (!argument)
    {
      /* purecov: begin tested */
      opt_debug_sync_timeout= DEBUG_SYNC_DEFAULT_WAIT_TIMEOUT;
      /* purecov: end */
    }
    break;
#endif /* defined(ENABLED_DEBUG_SYNC) */
  case OPT_MAX_LONG_DATA_SIZE:
    max_long_data_size_used= true;
    WARN_DEPRECATED(NULL, VER_CELOSIA, "--max_long_data_size", "--max_allowed_packet");
    break;
  }
  return 0;
}


/** Handle arguments for multiple key caches. */
C_MODE_START
static void* mysql_getopt_value(const char *, uint,
                                const struct my_option *, int *);
C_MODE_END

static void*
mysql_getopt_value(const char *keyname, uint key_length,
		   const struct my_option *option, int *error)
{
  if (error)
    *error= 0;
  switch (option->id) {
  case OPT_KEY_BUFFER_SIZE:
  case OPT_KEY_CACHE_BLOCK_SIZE:
  case OPT_KEY_CACHE_DIVISION_LIMIT:
  case OPT_KEY_CACHE_AGE_THRESHOLD:
  {
    KEY_CACHE *key_cache;
    if (!(key_cache= get_or_create_key_cache(keyname, key_length)))
    {
      if (error)
        *error= EXIT_OUT_OF_MEMORY;
      return 0;
    }
    switch (option->id) {
    case OPT_KEY_BUFFER_SIZE:
      return &key_cache->param_buff_size;
    case OPT_KEY_CACHE_BLOCK_SIZE:
      return &key_cache->param_block_size;
    case OPT_KEY_CACHE_DIVISION_LIMIT:
      return &key_cache->param_division_limit;
    case OPT_KEY_CACHE_AGE_THRESHOLD:
      return &key_cache->param_age_threshold;
    }
  }
  }
  return option->value;
}


extern "C" void option_error_reporter(enum loglevel level, const char *format, ...);

void option_error_reporter(enum loglevel level, const char *format, ...)
{
  va_list args;
  va_start(args, format);

  /* Don't print warnings for --loose options during bootstrap */
  if (level == ERROR_LEVEL || !opt_bootstrap ||
      global_system_variables.log_warnings)
  {
    vprint_msg_to_log(level, format, args);
  }
  va_end(args);
}


/**
  @todo
  - FIXME add EXIT_TOO_MANY_ARGUMENTS to "mysys_err.h" and return that code?
*/
static int get_options(int *argc,char **argv)
{
  int ho_error;

  my_getopt_register_get_addr(mysql_getopt_value);
  strmake(def_ft_boolean_syntax, ft_boolean_syntax,
	  sizeof(ft_boolean_syntax)-1);
  my_getopt_error_reporter= option_error_reporter;

  /* Skip unknown options so that they may be processed later by plugins */
  my_getopt_skip_unknown= TRUE;

  if ((ho_error= handle_options(argc, &argv, my_long_options,
                                mysqld_get_one_option)))
    return ho_error;
  (*argc)++; /* add back one for the progname handle_options removes */
             /* no need to do this for argv as we are discarding it. */

  if ((opt_log_slow_admin_statements || opt_log_queries_not_using_indexes ||
       opt_log_slow_slave_statements) &&
      !opt_slow_log)
    sql_print_warning("options --log-slow-admin-statements, --log-queries-not-using-indexes and --log-slow-slave-statements have no effect if --log_slow_queries is not set");
  if (global_system_variables.net_buffer_length > 
      global_system_variables.max_allowed_packet)
  {
    sql_print_warning("net_buffer_length (%lu) is set to be larger "
                      "than max_allowed_packet (%lu). Please rectify.",
                      global_system_variables.net_buffer_length, 
                      global_system_variables.max_allowed_packet);
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
  /* Set global slave_exec_mode from its option */
  fix_slave_exec_mode();

#ifndef EMBEDDED_LIBRARY
  if (mysqld_chroot)
    set_root(mysqld_chroot);
#else
  global_system_variables.thread_handling = SCHEDULER_NO_THREADS;
  max_allowed_packet= global_system_variables.max_allowed_packet;
  net_buffer_length= global_system_variables.net_buffer_length;
#endif
  if (fix_paths())
    return 1;

  /*
    Set some global variables from the global_system_variables
    In most cases the global variables will not be used
  */
  my_disable_locking= myisam_single_user= test(opt_external_locking == 0);
  my_default_record_cache_size=global_system_variables.read_buff_size;
  myisam_max_temp_length=
    (my_off_t) global_system_variables.myisam_max_sort_file_size;

  /* Set global variables based on startup options */
  myisam_block_size=(uint) 1 << my_bit_log2(opt_myisam_block_size);

  /* long_query_time is in microseconds */
  global_system_variables.long_query_time= max_system_variables.long_query_time=
    (longlong) (long_query_time * 1000000.0);

  if (opt_short_log_format)
    opt_specialflag|= SPECIAL_SHORT_LOG_FORMAT;

  if (init_global_datetime_format(MYSQL_TIMESTAMP_DATE,
				  &global_system_variables.date_format) ||
      init_global_datetime_format(MYSQL_TIMESTAMP_TIME,
				  &global_system_variables.time_format) ||
      init_global_datetime_format(MYSQL_TIMESTAMP_DATETIME,
				  &global_system_variables.datetime_format))
    return 1;

#ifdef EMBEDDED_LIBRARY
  one_thread_scheduler(&thread_scheduler);
#else
  if (global_system_variables.thread_handling <=
      SCHEDULER_ONE_THREAD_PER_CONNECTION)
    one_thread_per_connection_scheduler(&thread_scheduler);
  else if (global_system_variables.thread_handling == SCHEDULER_NO_THREADS)
    one_thread_scheduler(&thread_scheduler);
  else
    pool_of_threads_scheduler(&thread_scheduler);  /* purecov: tested */
#endif

  /*
    If max_long_data_size is not specified explicitly use
    value of max_allowed_packet.
  */
  if (!max_long_data_size_used)
    max_long_data_size= global_system_variables.max_allowed_packet;

  return 0;
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


/**
  Fix filename and replace extension where 'dir' is relative to
  mysql_real_data_home.
  @return
    1 if len(path) > FN_REFLEN
*/

bool
fn_format_relative_to_data_home(char * to, const char *name,
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
		    MY_APPEND_EXT | MY_UNPACK_FILENAME | MY_SAFE_PATH);
}


/**
  Test a file path to determine if the path is compatible with the secure file
  path restriction.
 
  @param path null terminated character string

  @return
    @retval TRUE The path is secure
    @retval FALSE The path isn't secure
*/

bool is_secure_file_path(char *path)
{
  char buff1[FN_REFLEN], buff2[FN_REFLEN];
  size_t opt_secure_file_priv_len;
  /*
    All paths are secure if opt_secure_file_path is 0
  */
  if (!opt_secure_file_priv)
    return TRUE;

  opt_secure_file_priv_len= strlen(opt_secure_file_priv);

  if (strlen(path) >= FN_REFLEN)
    return FALSE;

  if (my_realpath(buff1, path, 0))
  {
    /*
      The supplied file path might have been a file and not a directory.
    */
    int length= (int)dirname_length(path);
    if (length >= FN_REFLEN)
      return FALSE;
    memcpy(buff2, path, length);
    buff2[length]= '\0';
    if (length == 0 || my_realpath(buff1, buff2, 0))
      return FALSE;
  }
  convert_dirname(buff2, buff1, NullS);
  if (!lower_case_file_system)
  {
    if (strncmp(opt_secure_file_priv, buff2, opt_secure_file_priv_len))
      return FALSE;
  }
  else
  {
    if (files_charset_info->coll->strnncoll(files_charset_info,
                                            (uchar *) buff2, strlen(buff2),
                                            (uchar *) opt_secure_file_priv,
                                            opt_secure_file_priv_len,
                                            TRUE))
      return FALSE;
  }
  return TRUE;
}


static int fix_paths(void)
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
  convert_dirname(language,language,NullS);
  convert_dirname(mysql_real_data_home,mysql_real_data_home,NullS);
  (void) my_load_path(mysql_home,mysql_home,""); // Resolve current dir
  (void) my_load_path(mysql_real_data_home,mysql_real_data_home,mysql_home);
  (void) my_load_path(pidfile_name,pidfile_name,mysql_real_data_home);
  (void) my_load_path(opt_plugin_dir, opt_plugin_dir_ptr ? opt_plugin_dir_ptr :
                                      get_relative_path(PLUGINDIR), mysql_home);
  opt_plugin_dir_ptr= opt_plugin_dir;

  my_realpath(mysql_unpacked_real_data_home, mysql_real_data_home, MYF(0));
  mysql_unpacked_real_data_home_len= 
    (int) strlen(mysql_unpacked_real_data_home);
  if (mysql_unpacked_real_data_home[mysql_unpacked_real_data_home_len-1] == FN_LIBCHAR)
    --mysql_unpacked_real_data_home_len;

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
  convert_dirname(mysql_charsets_dir, mysql_charsets_dir, NullS);
  charsets_dir=mysql_charsets_dir;

  if (init_tmpdir(&mysql_tmpdir_list, opt_mysql_tmpdir))
    return 1;
#ifdef HAVE_REPLICATION
  if (!slave_load_tmpdir)
  {
    if (!(slave_load_tmpdir = (char*) my_strdup(mysql_tmpdir, MYF(MY_FAE))))
      return 1;
  }
#endif /* HAVE_REPLICATION */
  /*
    Convert the secure-file-priv option to system format, allowing
    a quick strcmp to check if read or write is in an allowed dir
   */
  if (opt_secure_file_priv)
  {
    if (*opt_secure_file_priv == 0)
    {
      my_free(opt_secure_file_priv, MYF(0));
      opt_secure_file_priv= 0;
    }
    else
    {
      if (strlen(opt_secure_file_priv) >= FN_REFLEN)
        opt_secure_file_priv[FN_REFLEN-1]= '\0';
      if (my_realpath(buff, opt_secure_file_priv, 0))
      {
        sql_print_warning("Failed to normalize the argument for --secure-file-priv.");
        return 1;
      }
      char *secure_file_real_path= (char *)my_malloc(FN_REFLEN, MYF(MY_FAE));
      convert_dirname(secure_file_real_path, buff, NullS);
      my_free(opt_secure_file_priv, MYF(0));
      opt_secure_file_priv= secure_file_real_path;
    }
  }
  
  return 0;
}


static ulong find_bit_type_or_exit(const char *x, TYPELIB *bit_lib,
                                   const char *option, int *error)
{
  ulong result;
  const char **ptr;
  
  *error= 0;
  if ((result= find_bit_type(x, bit_lib)) == ~(ulong) 0)
  {
    char *buff= (char *) my_alloca(2048);
    char *cbuf;
    ptr= bit_lib->type_names;
    cbuf= buff + ((!*x) ?
      my_snprintf(buff, 2048, "No option given to %s\n", option) :
      my_snprintf(buff, 2048, "Wrong option to %s. Option(s) given: %s\n",
                  option, x));
    cbuf+= my_snprintf(cbuf, 2048 - (cbuf-buff), "Alternatives are: '%s'", *ptr);
    while (*++ptr)
      cbuf+= my_snprintf(cbuf, 2048 - (cbuf-buff), ",'%s'", *ptr);
    my_snprintf(cbuf, 2048 - (cbuf-buff), "\n");
    sql_perror(buff);
    *error= 1;
    my_afree(buff);
    return 0;
  }

  return result;
}


/**
  @return
    a bitfield from a string of substrings separated by ','
    or
    ~(ulong) 0 on error.
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
  pos=(char *) x;
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


/**
  Check if file system used for databases is case insensitive.

  @param dir_name			Directory to test

  @retval
    -1  Don't know (Test failed)
  @retval
    0   File system is case sensitive
  @retval
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


#ifndef EMBEDDED_LIBRARY

/**
  Create file to store pid number.
*/
static void create_pid_file()
{
  File file;
  if ((file = my_create(pidfile_name,0664,
			O_WRONLY | O_TRUNC, MYF(MY_WME))) >= 0)
  {
    char buff[21], *end;
    end= int10_to_str((long) getpid(), buff, 10);
    *end++= '\n';
    if (!my_write(file, (uchar*) buff, (uint) (end-buff), MYF(MY_WME | MY_NABP)))
    {
      (void) my_close(file, MYF(0));
      return;
    }
    (void) my_close(file, MYF(0));
  }
  sql_perror("Can't start server: can't create PID file");
  exit(1);
}
#endif /* EMBEDDED_LIBRARY */

/** Clear most status variables. */
void refresh_status(THD *thd)
{
  pthread_mutex_lock(&LOCK_status);

  /* Add thread's status variabes to global status */
  add_to_status(&global_status_var, &thd->status_var);

  /* Reset thread's status variables */
  bzero((uchar*) &thd->status_var, sizeof(thd->status_var));

  /* Reset some global variables */
  reset_status_vars();

  /* Reset the counters of all key caches (default and named). */
  process_key_caches(reset_key_cache_counters);
#ifdef COMMUNITY_SERVER
  flush_status_time= time((time_t*) 0);
#endif
  pthread_mutex_unlock(&LOCK_status);

  /*
    Set max_used_connections to the number of currently open
    connections.  Lock LOCK_thread_count out of LOCK_status to avoid
    deadlocks.  Status reset becomes not atomic, but status data is
    not exact anyway.
  */
  pthread_mutex_lock(&LOCK_thread_count);
  max_used_connections= thread_count-delayed_insert_threads;
  pthread_mutex_unlock(&LOCK_thread_count);
}


/*****************************************************************************
  Instantiate variables for missing storage engines
  This section should go away soon
*****************************************************************************/

#ifndef WITH_NDBCLUSTER_STORAGE_ENGINE
ulong ndb_cache_check_time;
ulong ndb_extra_logging;
#endif

/*****************************************************************************
  Instantiate templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
/* Used templates */
template class I_List<THD>;
template class I_List_iterator<THD>;
template class I_List<i_string>;
template class I_List<i_string_pair>;
template class I_List<NAMED_LIST>;
template class I_List<Statement>;
template class I_List_iterator<Statement>;
#endif
