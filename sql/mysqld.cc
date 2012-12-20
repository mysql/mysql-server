/* Copyright (c) 2000, 2012, Oracle and/or its affiliates.
   Copyright (c) 2008, 2012, Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "sql_priv.h"
#include "unireg.h"
#include <signal.h>
#ifndef __WIN__
#include <netdb.h>        // getservbyname, servent
#endif
#include "sql_parse.h"    // test_if_data_home_dir
#include "sql_cache.h"    // query_cache, query_cache_*
#include "sql_locale.h"   // MY_LOCALES, my_locales, my_locale_by_name
#include "sql_show.h"     // free_status_vars, add_status_vars,
                          // reset_status_vars
#include "strfunc.h"      // find_set_from_flags
#include "parse_file.h"   // File_parser_dummy_hook
#include "sql_db.h"       // my_dboptions_cache_free
                          // my_dboptions_cache_init
#include "sql_table.h"    // release_ddl_log, execute_ddl_log_recovery
#include "sql_connect.h"  // free_max_user_conn, init_max_user_conn,
                          // handle_one_connection
#include "sql_time.h"     // known_date_time_formats,
                          // get_date_time_format_str,
                          // date_time_format_make
#include "tztime.h"       // my_tz_free, my_tz_init, my_tz_SYSTEM
#include "hostname.h"     // hostname_cache_free, hostname_cache_init
#include "sql_acl.h"      // acl_free, grant_free, acl_init,
                          // grant_init
#include "sql_base.h"     // table_def_free, table_def_init,
                          // cached_open_tables,
                          // cached_table_definitions
#include "sql_test.h"     // mysql_print_status
#include "item_create.h"  // item_create_cleanup, item_create_init
#include "sql_servers.h"  // servers_free, servers_init
#include "init.h"         // unireg_init
#include "derror.h"       // init_errmessage
#include "derror.h"       // init_errmessage
#include "des_key_file.h" // load_des_key_file
#include "sql_manager.h"  // stop_handle_manager, start_handle_manager
#include "sql_expression_cache.h" // subquery_cache_miss, subquery_cache_hit

#include <m_ctype.h>
#include <my_dir.h>
#include <my_bit.h>
#include "slave.h"
#include "rpl_mi.h"
#include "sql_repl.h"
#include "rpl_filter.h"
#include "client_settings.h"
#include "repl_failsafe.h"
#include <sql_common.h>
#include <my_stacktrace.h>
#include "mysqld_suffix.h"
#include "mysys_err.h"
#include "events.h"
#include "sql_audit.h"
#include "probes_mysql.h"
#include "scheduler.h"
#include <waiting_threads.h>
#include "debug_sync.h"
#include "sql_callback.h"
#include "threadpool.h"

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
#include "../storage/perfschema/pfs_server.h"
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

#include "keycaches.h"
#include "../storage/myisam/ha_myisam.h"
#include "set_var.h"

#include "rpl_injector.h"

#include "rpl_handler.h"

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include <thr_alarm.h>
#include <ft_global.h>
#include <errmsg.h>
#include "sp_rcontext.h"
#include "sp_cache.h"
#include "sql_reload.h"  // reload_acl_and_cache

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#define mysqld_charset &my_charset_latin1

/* We have HAVE_valgrind below as this speeds up the shutdown of MySQL */

#if defined(SIGNALS_DONT_BREAK_READ) || defined(HAVE_valgrind) && defined(__linux__)
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
#include <sys/resource.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_SELECT_H
#include <select.h>
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

#ifdef HAVE_SOLARIS_LARGE_PAGES
#include <sys/mman.h>
#if defined(__sun__) && defined(__GNUC__) && defined(__cplusplus) \
    && defined(_XOPEN_SOURCE)
extern int getpagesizes(size_t *, int);
extern int getpagesizes2(size_t *, int);
extern int memcntl(caddr_t, size_t, int, caddr_t, int, int);
#endif /* __sun__ ... */
#endif /* HAVE_SOLARIS_LARGE_PAGES */

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

#ifndef HAVE_FCNTL
#define fcntl(X,Y,Z) 0
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

#include <my_pthread.h>			// For thr_setconcurency()

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

static const char *tc_heuristic_recover_names[]=
{
  "COMMIT", "ROLLBACK", NullS
};
static TYPELIB tc_heuristic_recover_typelib=
{
  array_elements(tc_heuristic_recover_names)-1,"",
  tc_heuristic_recover_names, NULL
};

const char *first_keyword= "first", *binary_keyword= "BINARY";
const char *my_localhost= "localhost", *delayed_user= "DELAYED";

bool opt_large_files= sizeof(my_off_t) > 4;
static my_bool opt_autocommit; ///< for --autocommit command-line option

/*
  Used with --help for detailed option
*/
static my_bool opt_verbose= 0;

arg_cmp_func Arg_comparator::comparator_matrix[6][2] =
{{&Arg_comparator::compare_string,     &Arg_comparator::compare_e_string},
 {&Arg_comparator::compare_real,       &Arg_comparator::compare_e_real},
 {&Arg_comparator::compare_int_signed, &Arg_comparator::compare_e_int},
 {&Arg_comparator::compare_row,        &Arg_comparator::compare_e_row},
 {&Arg_comparator::compare_decimal,    &Arg_comparator::compare_e_decimal},
 {&Arg_comparator::compare_datetime,   &Arg_comparator::compare_e_datetime}};

/* static variables */

#ifdef HAVE_PSI_INTERFACE
#if (defined(_WIN32) || defined(HAVE_SMEM)) && !defined(EMBEDDED_LIBRARY)
static PSI_thread_key key_thread_handle_con_namedpipes;
static PSI_cond_key key_COND_handler_count;
#endif /* _WIN32 || HAVE_SMEM && !EMBEDDED_LIBRARY */

#if defined(HAVE_SMEM) && !defined(EMBEDDED_LIBRARY)
static PSI_thread_key key_thread_handle_con_sharedmem;
#endif /* HAVE_SMEM && !EMBEDDED_LIBRARY */

#if (defined(_WIN32) || defined(HAVE_SMEM)) && !defined(EMBEDDED_LIBRARY)
static PSI_thread_key key_thread_handle_con_sockets;
#endif /* _WIN32 || HAVE_SMEM && !EMBEDDED_LIBRARY */

#ifdef __WIN__
static PSI_thread_key key_thread_handle_shutdown;
#endif /* __WIN__ */

#if defined (HAVE_OPENSSL) && !defined(HAVE_YASSL)
static PSI_rwlock_key key_rwlock_openssl;
#endif
#endif /* HAVE_PSI_INTERFACE */

#ifdef HAVE_NPTL
volatile sig_atomic_t ld_assume_kernel_is_set= 0;
#endif

/* the default log output is log tables */
static bool lower_case_table_names_used= 0;
static bool max_long_data_size_used= false;
static bool volatile select_thread_in_use, signal_thread_in_use;
static volatile bool ready_to_exit;
static my_bool opt_debugging= 0, opt_external_locking= 0, opt_console= 0;
static my_bool opt_short_log_format= 0;
static uint kill_cached_threads, wake_thread;
ulong max_used_connections;
static volatile ulong cached_thread_count= 0;
static char *mysqld_user, *mysqld_chroot;
static char *default_character_set_name;
static char *character_set_filesystem_name;
static char *lc_messages;
static char *lc_time_names_name;
static char *my_bind_addr_str;
static char *default_collation_name;
char *default_storage_engine;
static char compiled_default_collation_name[]= MYSQL_DEFAULT_COLLATION_NAME;
static I_List<THD> thread_cache;
static bool binlog_format_used= false;
LEX_STRING opt_init_connect, opt_init_slave;
static mysql_cond_t COND_thread_cache, COND_flush_thread_cache;
static DYNAMIC_ARRAY all_options;

/* Global variables */

bool opt_bin_log, opt_bin_log_used=0, opt_ignore_builtin_innodb= 0;
my_bool opt_log, opt_slow_log, debug_assert_if_crashed_table= 0, opt_help= 0, opt_abort;
ulonglong log_output_options;
my_bool opt_userstat_running;
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
uint volatile global_disable_checkpoint;
#if defined(_WIN32) && !defined(EMBEDDED_LIBRARY)
ulong slow_start_timeout;
#endif
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
my_bool opt_safe_user_create = 0;
my_bool opt_show_slave_auth_info;
my_bool opt_log_slave_updates= 0;
my_bool opt_replicate_annotate_row_events= 0;
char *opt_slave_skip_errors;

/*
  Legacy global handlerton. These will be removed (please do not add more).
*/
handlerton *heap_hton;
handlerton *myisam_hton;
handlerton *partition_hton;

my_bool read_only= 0, opt_readonly= 0;
my_bool use_temp_pool, relay_log_purge;
my_bool relay_log_recovery;
my_bool opt_sync_frm, opt_allow_suspicious_udfs;
my_bool opt_secure_auth= 0;
char* opt_secure_file_priv;
my_bool opt_log_slow_admin_statements= 0;
my_bool opt_log_slow_slave_statements= 0;
my_bool lower_case_file_system= 0;
my_bool opt_large_pages= 0;
my_bool opt_super_large_pages= 0;
my_bool opt_myisam_use_mmap= 0;
uint   opt_large_page_size= 0;
#if defined(ENABLED_DEBUG_SYNC)
MYSQL_PLUGIN_IMPORT uint    opt_debug_sync_timeout= 0;
#endif /* defined(ENABLED_DEBUG_SYNC) */
my_bool opt_old_style_user_limits= 0, trust_function_creators= 0;
ulong opt_replicate_events_marked_for_skip;

/*
  True if there is at least one per-hour limit for some user, so we should
  check them before each query (and possibly reset counters when hour is
  changed). False otherwise.
*/
volatile bool mqh_used = 0;
my_bool opt_noacl;
my_bool sp_automatic_privileges= 1;

ulong opt_binlog_rows_event_max_size;
my_bool opt_master_verify_checksum= 0;
my_bool opt_slave_sql_verify_checksum= 1;
const char *binlog_format_names[]= {"MIXED", "STATEMENT", "ROW", NullS};
#ifdef HAVE_INITGROUPS
volatile sig_atomic_t calling_initgroups= 0; /**< Used in SIGSEGV handler. */
#endif
uint mysqld_port, test_flags, select_errors, dropping_tables, ha_open_options;
uint mysqld_extra_port;
uint mysqld_port_timeout;
ulong delay_key_write_options;
uint protocol_version;
uint lower_case_table_names;
ulong tc_heuristic_recover= 0;
uint volatile thread_count;
int32 thread_running;
ulong thread_created;
ulong back_log, connect_timeout, concurrency, server_id;
ulong table_cache_size, table_def_size;
ulong what_to_log;
ulong slow_launch_time, slave_open_temp_tables;
ulong open_files_limit, max_binlog_size, max_relay_log_size;
ulong slave_trans_retries;
uint  slave_net_timeout;
ulong slave_exec_mode_options;
ulonglong slave_type_conversions_options;
ulong thread_cache_size=0;
ulonglong binlog_cache_size=0;
ulonglong max_binlog_cache_size=0;
ulong slave_max_allowed_packet= 0;
ulonglong binlog_stmt_cache_size=0;
ulonglong  max_binlog_stmt_cache_size=0;
ulonglong query_cache_size=0;
ulong refresh_version;  /* Increments on each reload */
ulong executed_events=0;
query_id_t global_query_id;
my_atomic_rwlock_t global_query_id_lock;
my_atomic_rwlock_t thread_running_lock;
ulong aborted_threads, aborted_connects;
ulong delayed_insert_timeout, delayed_insert_limit, delayed_queue_size;
ulong delayed_insert_threads, delayed_insert_writes, delayed_rows_in_use;
ulong delayed_insert_errors,flush_time;
ulong specialflag=0;
ulong binlog_cache_use= 0, binlog_cache_disk_use= 0;
ulong binlog_stmt_cache_use= 0, binlog_stmt_cache_disk_use= 0;
ulong max_connections, max_connect_errors;
ulong extra_max_connections;
ulonglong denied_connections;
my_decimal decimal_zero;

/*
  Maximum length of parameter value which can be set through
  mysql_send_long_data() call.
*/
ulong max_long_data_size;

/* Limits for internal temporary tables (MyISAM or Aria) */
uint internal_tmp_table_max_key_length;
uint internal_tmp_table_max_key_segments;

bool max_user_connections_checking=0;
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
ulong slow_launch_threads = 0;
uint sync_binlog_period= 0, sync_relaylog_period= 0,
     sync_relayloginfo_period= 0, sync_masterinfo_period= 0;
ulong expire_logs_days = 0;
ulong rpl_recovery_rank=0;
/**
  Soft upper limit for number of sp_head objects that can be stored
  in the sp_cache for one connection.
*/
ulong stored_program_cache_size= 0;

const double log_10[] = {
  1e000, 1e001, 1e002, 1e003, 1e004, 1e005, 1e006, 1e007, 1e008, 1e009,
  1e010, 1e011, 1e012, 1e013, 1e014, 1e015, 1e016, 1e017, 1e018, 1e019,
  1e020, 1e021, 1e022, 1e023, 1e024, 1e025, 1e026, 1e027, 1e028, 1e029,
  1e030, 1e031, 1e032, 1e033, 1e034, 1e035, 1e036, 1e037, 1e038, 1e039,
  1e040, 1e041, 1e042, 1e043, 1e044, 1e045, 1e046, 1e047, 1e048, 1e049,
  1e050, 1e051, 1e052, 1e053, 1e054, 1e055, 1e056, 1e057, 1e058, 1e059,
  1e060, 1e061, 1e062, 1e063, 1e064, 1e065, 1e066, 1e067, 1e068, 1e069,
  1e070, 1e071, 1e072, 1e073, 1e074, 1e075, 1e076, 1e077, 1e078, 1e079,
  1e080, 1e081, 1e082, 1e083, 1e084, 1e085, 1e086, 1e087, 1e088, 1e089,
  1e090, 1e091, 1e092, 1e093, 1e094, 1e095, 1e096, 1e097, 1e098, 1e099,
  1e100, 1e101, 1e102, 1e103, 1e104, 1e105, 1e106, 1e107, 1e108, 1e109,
  1e110, 1e111, 1e112, 1e113, 1e114, 1e115, 1e116, 1e117, 1e118, 1e119,
  1e120, 1e121, 1e122, 1e123, 1e124, 1e125, 1e126, 1e127, 1e128, 1e129,
  1e130, 1e131, 1e132, 1e133, 1e134, 1e135, 1e136, 1e137, 1e138, 1e139,
  1e140, 1e141, 1e142, 1e143, 1e144, 1e145, 1e146, 1e147, 1e148, 1e149,
  1e150, 1e151, 1e152, 1e153, 1e154, 1e155, 1e156, 1e157, 1e158, 1e159,
  1e160, 1e161, 1e162, 1e163, 1e164, 1e165, 1e166, 1e167, 1e168, 1e169,
  1e170, 1e171, 1e172, 1e173, 1e174, 1e175, 1e176, 1e177, 1e178, 1e179,
  1e180, 1e181, 1e182, 1e183, 1e184, 1e185, 1e186, 1e187, 1e188, 1e189,
  1e190, 1e191, 1e192, 1e193, 1e194, 1e195, 1e196, 1e197, 1e198, 1e199,
  1e200, 1e201, 1e202, 1e203, 1e204, 1e205, 1e206, 1e207, 1e208, 1e209,
  1e210, 1e211, 1e212, 1e213, 1e214, 1e215, 1e216, 1e217, 1e218, 1e219,
  1e220, 1e221, 1e222, 1e223, 1e224, 1e225, 1e226, 1e227, 1e228, 1e229,
  1e230, 1e231, 1e232, 1e233, 1e234, 1e235, 1e236, 1e237, 1e238, 1e239,
  1e240, 1e241, 1e242, 1e243, 1e244, 1e245, 1e246, 1e247, 1e248, 1e249,
  1e250, 1e251, 1e252, 1e253, 1e254, 1e255, 1e256, 1e257, 1e258, 1e259,
  1e260, 1e261, 1e262, 1e263, 1e264, 1e265, 1e266, 1e267, 1e268, 1e269,
  1e270, 1e271, 1e272, 1e273, 1e274, 1e275, 1e276, 1e277, 1e278, 1e279,
  1e280, 1e281, 1e282, 1e283, 1e284, 1e285, 1e286, 1e287, 1e288, 1e289,
  1e290, 1e291, 1e292, 1e293, 1e294, 1e295, 1e296, 1e297, 1e298, 1e299,
  1e300, 1e301, 1e302, 1e303, 1e304, 1e305, 1e306, 1e307, 1e308
};

time_t server_start_time, flush_status_time;

char mysql_home[FN_REFLEN], pidfile_name[FN_REFLEN], system_time_zone[30];
char *default_tz_name;
char log_error_file[FN_REFLEN], glob_hostname[FN_REFLEN], *opt_log_basename;
char mysql_real_data_home[FN_REFLEN],
     lc_messages_dir[FN_REFLEN], reg_ext[FN_EXTLEN],
     mysql_charsets_dir[FN_REFLEN],
     *opt_init_file, *opt_tc_log_file;
char *lc_messages_dir_ptr= lc_messages_dir, *log_error_file_ptr;
char mysql_unpacked_real_data_home[FN_REFLEN];
int mysql_unpacked_real_data_home_len;
uint mysql_real_data_home_len, mysql_data_home_len= 1;
uint reg_ext_length;
const key_map key_map_empty(0);
key_map key_map_full(0);                        // Will be initialized later

DATE_TIME_FORMAT global_date_format, global_datetime_format, global_time_format;
Time_zone *default_tz;

char *mysql_data_home= const_cast<char*>(".");
const char *mysql_real_data_home_ptr= mysql_real_data_home;
char server_version[SERVER_VERSION_LENGTH];
char *mysqld_unix_port, *opt_mysql_tmpdir;
ulong thread_handling;

/** name of reference on left expression in rewritten IN subquery */
const char *in_left_expr_name= "<left expr>";
/** name of additional condition */
const char *in_additional_cond= "<IN COND>";
const char *in_having_cond= "<IN HAVING>";

/* classes for comparation parsing/processing */
Eq_creator eq_creator;
Ne_creator ne_creator;
Gt_creator gt_creator;
Lt_creator lt_creator;
Ge_creator ge_creator;
Le_creator le_creator;

MYSQL_FILE *bootstrap_file;
int bootstrap_error;

I_List<THD> threads;
Rpl_filter* rpl_filter;
Rpl_filter* binlog_filter;

THD *first_global_thread()
{
  if (threads.is_empty())
    return NULL;
  return threads.head();
}

THD *next_global_thread(THD *thd)
{
  if (threads.is_last(thd))
    return NULL;
  struct ilink *next= thd->next;
  return static_cast<THD*>(next);
}

struct system_variables global_system_variables;
struct system_variables max_system_variables;
struct system_status_var global_status_var;

MY_TMPDIR mysql_tmpdir_list;
MY_BITMAP temp_pool;

CHARSET_INFO *system_charset_info, *files_charset_info ;
CHARSET_INFO *national_charset_info, *table_alias_charset;
CHARSET_INFO *character_set_filesystem;
CHARSET_INFO *error_message_charset_info;

MY_LOCALE *my_default_lc_messages;
MY_LOCALE *my_default_lc_time_names;

SHOW_COMP_OPTION have_ssl, have_symlink, have_dlopen, have_query_cache;
SHOW_COMP_OPTION have_geometry, have_rtree_keys;
SHOW_COMP_OPTION have_crypt, have_compress;
SHOW_COMP_OPTION have_profiling;

/* Thread specific variables */

pthread_key(MEM_ROOT**,THR_MALLOC);
pthread_key(THD*, THR_THD);
mysql_mutex_t LOCK_thread_count;
mysql_mutex_t
  LOCK_status, LOCK_error_log, LOCK_short_uuid_generator,
  LOCK_delayed_insert, LOCK_delayed_status, LOCK_delayed_create,
  LOCK_crypt,
  LOCK_global_system_variables,
  LOCK_user_conn, LOCK_slave_list, LOCK_active_mi,
  LOCK_connection_count, LOCK_error_messages;

mysql_mutex_t LOCK_stats, LOCK_global_user_client_stats,
              LOCK_global_table_stats, LOCK_global_index_stats;

/**
  The below lock protects access to two global server variables:
  max_prepared_stmt_count and prepared_stmt_count. These variables
  set the limit and hold the current total number of prepared statements
  in the server, respectively. As PREPARE/DEALLOCATE rate in a loaded
  server may be fairly high, we need a dedicated lock.
*/
mysql_mutex_t LOCK_prepared_stmt_count;
#ifdef HAVE_OPENSSL
mysql_mutex_t LOCK_des_key_file;
#endif
mysql_rwlock_t LOCK_grant, LOCK_sys_init_connect, LOCK_sys_init_slave;
mysql_rwlock_t LOCK_system_variables_hash;
mysql_cond_t COND_thread_count;
pthread_t signal_thread;
pthread_attr_t connection_attrib;
mysql_mutex_t LOCK_server_started;
mysql_cond_t COND_server_started;

int mysqld_server_started= 0;

File_parser_dummy_hook file_parser_dummy_hook;

/* replication parameters, if master_host is not NULL, we are a slave */
uint report_port= 0;
ulong master_retry_count=0;
char *master_info_file;
char *relay_log_info_file, *report_user, *report_password, *report_host;
char *opt_relay_logname = 0, *opt_relaylog_index_name=0;
char *opt_logname, *opt_slow_logname, *opt_bin_logname;

/* Static variables */

static volatile sig_atomic_t kill_in_progress;
my_bool opt_stack_trace;
my_bool opt_expect_abort= 0;
static my_bool opt_bootstrap, opt_myisam_log;
static int cleanup_done;
static ulong opt_specialflag;
static char *opt_binlog_index_name;
char *mysql_home_ptr, *pidfile_name_ptr;
/** Initial command line arguments (count), after load_defaults().*/
static int defaults_argc;
/**
  Initial command line arguments (arguments), after load_defaults().
  This memory is allocated by @c load_defaults() and should be freed
  using @c free_defaults().
  Do not modify defaults_argc / defaults_argv,
  use remaining_argc / remaining_argv instead to parse the command
  line arguments in multiple steps.
*/
static char **defaults_argv;
/** Remaining command line arguments (count), filtered by handle_options().*/
static int remaining_argc;
/** Remaining command line arguments (arguments), filtered by handle_options().*/
static char **remaining_argv;

int orig_argc;
char **orig_argv;

#ifdef HAVE_PSI_INTERFACE
#ifdef HAVE_MMAP
PSI_mutex_key key_PAGE_lock, key_LOCK_sync, key_LOCK_active, key_LOCK_pool;
#endif /* HAVE_MMAP */

#ifdef HAVE_OPENSSL
PSI_mutex_key key_LOCK_des_key_file;
#endif /* HAVE_OPENSSL */

PSI_mutex_key key_BINLOG_LOCK_index, key_BINLOG_LOCK_prep_xids,
  key_delayed_insert_mutex, key_hash_filo_lock, key_LOCK_active_mi,
  key_LOCK_connection_count, key_LOCK_crypt, key_LOCK_delayed_create,
  key_LOCK_delayed_insert, key_LOCK_delayed_status, key_LOCK_error_log,
  key_LOCK_gdl, key_LOCK_global_system_variables,
  key_LOCK_manager,
  key_LOCK_prepared_stmt_count,
  key_LOCK_rpl_status, key_LOCK_server_started, key_LOCK_status,
  key_LOCK_system_variables_hash, key_LOCK_table_share, key_LOCK_thd_data,
  key_LOCK_user_conn, key_LOCK_uuid_short_generator, key_LOG_LOCK_log,
  key_master_info_data_lock, key_master_info_run_lock,
  key_master_info_sleep_lock,
  key_mutex_slave_reporting_capability_err_lock, key_relay_log_info_data_lock,
  key_relay_log_info_log_space_lock, key_relay_log_info_run_lock,
  key_relay_log_info_sleep_lock,
  key_structure_guard_mutex, key_TABLE_SHARE_LOCK_ha_data,
  key_LOCK_error_messages, key_LOG_INFO_lock, key_LOCK_thread_count,
  key_PARTITION_LOCK_auto_inc;
PSI_mutex_key key_RELAYLOG_LOCK_index;

PSI_mutex_key key_LOCK_stats,
  key_LOCK_global_user_client_stats, key_LOCK_global_table_stats,
  key_LOCK_global_index_stats,
  key_LOCK_wakeup_ready;

PSI_mutex_key key_LOCK_prepare_ordered, key_LOCK_commit_ordered;

static PSI_mutex_info all_server_mutexes[]=
{
#ifdef HAVE_MMAP
  { &key_PAGE_lock, "PAGE::lock", 0},
  { &key_LOCK_sync, "TC_LOG_MMAP::LOCK_sync", 0},
  { &key_LOCK_active, "TC_LOG_MMAP::LOCK_active", 0},
  { &key_LOCK_pool, "TC_LOG_MMAP::LOCK_pool", 0},
#endif /* HAVE_MMAP */

#ifdef HAVE_OPENSSL
  { &key_LOCK_des_key_file, "LOCK_des_key_file", PSI_FLAG_GLOBAL},
#endif /* HAVE_OPENSSL */

  { &key_BINLOG_LOCK_index, "MYSQL_BIN_LOG::LOCK_index", 0},
  { &key_BINLOG_LOCK_prep_xids, "MYSQL_BIN_LOG::LOCK_prep_xids", 0},
  { &key_RELAYLOG_LOCK_index, "MYSQL_RELAY_LOG::LOCK_index", 0},
  { &key_delayed_insert_mutex, "Delayed_insert::mutex", 0},
  { &key_hash_filo_lock, "hash_filo::lock", 0},
  { &key_LOCK_active_mi, "LOCK_active_mi", PSI_FLAG_GLOBAL},
  { &key_LOCK_connection_count, "LOCK_connection_count", PSI_FLAG_GLOBAL},
  { &key_LOCK_crypt, "LOCK_crypt", PSI_FLAG_GLOBAL},
  { &key_LOCK_delayed_create, "LOCK_delayed_create", PSI_FLAG_GLOBAL},
  { &key_LOCK_delayed_insert, "LOCK_delayed_insert", PSI_FLAG_GLOBAL},
  { &key_LOCK_delayed_status, "LOCK_delayed_status", PSI_FLAG_GLOBAL},
  { &key_LOCK_error_log, "LOCK_error_log", PSI_FLAG_GLOBAL},
  { &key_LOCK_gdl, "LOCK_gdl", PSI_FLAG_GLOBAL},
  { &key_LOCK_global_system_variables, "LOCK_global_system_variables", PSI_FLAG_GLOBAL},
  { &key_LOCK_manager, "LOCK_manager", PSI_FLAG_GLOBAL},
  { &key_LOCK_prepared_stmt_count, "LOCK_prepared_stmt_count", PSI_FLAG_GLOBAL},
  { &key_LOCK_rpl_status, "LOCK_rpl_status", PSI_FLAG_GLOBAL},
  { &key_LOCK_server_started, "LOCK_server_started", PSI_FLAG_GLOBAL},
  { &key_LOCK_status, "LOCK_status", PSI_FLAG_GLOBAL},
  { &key_LOCK_system_variables_hash, "LOCK_system_variables_hash", PSI_FLAG_GLOBAL},
  { &key_LOCK_table_share, "LOCK_table_share", PSI_FLAG_GLOBAL},
  { &key_LOCK_stats, "LOCK_stats", PSI_FLAG_GLOBAL},
  { &key_LOCK_global_user_client_stats, "LOCK_global_user_client_stats", PSI_FLAG_GLOBAL},
  { &key_LOCK_global_table_stats, "LOCK_global_table_stats", PSI_FLAG_GLOBAL},
  { &key_LOCK_global_index_stats, "LOCK_global_index_stats", PSI_FLAG_GLOBAL},
  { &key_LOCK_wakeup_ready, "THD::LOCK_wakeup_ready", 0},
  { &key_LOCK_thd_data, "THD::LOCK_thd_data", 0},
  { &key_LOCK_user_conn, "LOCK_user_conn", PSI_FLAG_GLOBAL},
  { &key_LOCK_uuid_short_generator, "LOCK_uuid_short_generator", PSI_FLAG_GLOBAL},
  { &key_LOG_LOCK_log, "LOG::LOCK_log", 0},
  { &key_master_info_data_lock, "Master_info::data_lock", 0},
  { &key_master_info_run_lock, "Master_info::run_lock", 0},
  { &key_master_info_sleep_lock, "Master_info::sleep_lock", 0},
  { &key_mutex_slave_reporting_capability_err_lock, "Slave_reporting_capability::err_lock", 0},
  { &key_relay_log_info_data_lock, "Relay_log_info::data_lock", 0},
  { &key_relay_log_info_log_space_lock, "Relay_log_info::log_space_lock", 0},
  { &key_relay_log_info_run_lock, "Relay_log_info::run_lock", 0},
  { &key_relay_log_info_sleep_lock, "Relay_log_info::sleep_lock", 0},
  { &key_structure_guard_mutex, "Query_cache::structure_guard_mutex", 0},
  { &key_TABLE_SHARE_LOCK_ha_data, "TABLE_SHARE::LOCK_ha_data", 0},
  { &key_LOCK_error_messages, "LOCK_error_messages", PSI_FLAG_GLOBAL},
  { &key_LOCK_prepare_ordered, "LOCK_prepare_ordered", PSI_FLAG_GLOBAL},
  { &key_LOCK_commit_ordered, "LOCK_commit_ordered", PSI_FLAG_GLOBAL},
  { &key_LOG_INFO_lock, "LOG_INFO::lock", 0},
  { &key_LOCK_thread_count, "LOCK_thread_count", PSI_FLAG_GLOBAL},
  { &key_PARTITION_LOCK_auto_inc, "HA_DATA_PARTITION::LOCK_auto_inc", 0}
};

PSI_rwlock_key key_rwlock_LOCK_grant, key_rwlock_LOCK_logger,
  key_rwlock_LOCK_sys_init_connect, key_rwlock_LOCK_sys_init_slave,
  key_rwlock_LOCK_system_variables_hash, key_rwlock_query_cache_query_lock;

static PSI_rwlock_info all_server_rwlocks[]=
{
#if defined (HAVE_OPENSSL) && !defined(HAVE_YASSL)
  { &key_rwlock_openssl, "CRYPTO_dynlock_value::lock", 0},
#endif
  { &key_rwlock_LOCK_grant, "LOCK_grant", PSI_FLAG_GLOBAL},
  { &key_rwlock_LOCK_logger, "LOGGER::LOCK_logger", 0},
  { &key_rwlock_LOCK_sys_init_connect, "LOCK_sys_init_connect", PSI_FLAG_GLOBAL},
  { &key_rwlock_LOCK_sys_init_slave, "LOCK_sys_init_slave", PSI_FLAG_GLOBAL},
  { &key_rwlock_LOCK_system_variables_hash, "LOCK_system_variables_hash", PSI_FLAG_GLOBAL},
  { &key_rwlock_query_cache_query_lock, "Query_cache_query::lock", 0}
};

#ifdef HAVE_MMAP
PSI_cond_key key_PAGE_cond, key_COND_active, key_COND_pool;
#endif /* HAVE_MMAP */

PSI_cond_key key_BINLOG_COND_prep_xids, key_BINLOG_update_cond,
  key_COND_cache_status_changed, key_COND_manager,
  key_COND_rpl_status, key_COND_server_started,
  key_delayed_insert_cond, key_delayed_insert_cond_client,
  key_item_func_sleep_cond, key_master_info_data_cond,
  key_master_info_start_cond, key_master_info_stop_cond,
  key_master_info_sleep_cond,
  key_relay_log_info_data_cond, key_relay_log_info_log_space_cond,
  key_relay_log_info_start_cond, key_relay_log_info_stop_cond,
  key_relay_log_info_sleep_cond,
  key_TABLE_SHARE_cond, key_user_level_lock_cond,
  key_COND_thread_count, key_COND_thread_cache, key_COND_flush_thread_cache,
  key_BINLOG_COND_queue_busy;
PSI_cond_key key_RELAYLOG_update_cond, key_COND_wakeup_ready;
PSI_cond_key key_RELAYLOG_COND_queue_busy;
PSI_cond_key key_TC_LOG_MMAP_COND_queue_busy;

static PSI_cond_info all_server_conds[]=
{
#if (defined(_WIN32) || defined(HAVE_SMEM)) && !defined(EMBEDDED_LIBRARY)
  { &key_COND_handler_count, "COND_handler_count", PSI_FLAG_GLOBAL},
#endif /* _WIN32 || HAVE_SMEM && !EMBEDDED_LIBRARY */
#ifdef HAVE_MMAP
  { &key_PAGE_cond, "PAGE::cond", 0},
  { &key_COND_active, "TC_LOG_MMAP::COND_active", 0},
  { &key_COND_pool, "TC_LOG_MMAP::COND_pool", 0},
  { &key_TC_LOG_MMAP_COND_queue_busy, "TC_LOG_MMAP::COND_queue_busy", 0},
#endif /* HAVE_MMAP */
  { &key_BINLOG_COND_prep_xids, "MYSQL_BIN_LOG::COND_prep_xids", 0},
  { &key_BINLOG_update_cond, "MYSQL_BIN_LOG::update_cond", 0},
  { &key_BINLOG_COND_queue_busy, "MYSQL_BIN_LOG::COND_queue_busy", 0},
  { &key_RELAYLOG_update_cond, "MYSQL_RELAY_LOG::update_cond", 0},
  { &key_RELAYLOG_COND_queue_busy, "MYSQL_RELAY_LOG::COND_queue_busy", 0},
  { &key_COND_wakeup_ready, "THD::COND_wakeup_ready", 0},
  { &key_COND_cache_status_changed, "Query_cache::COND_cache_status_changed", 0},
  { &key_COND_manager, "COND_manager", PSI_FLAG_GLOBAL},
  { &key_COND_rpl_status, "COND_rpl_status", PSI_FLAG_GLOBAL},
  { &key_COND_server_started, "COND_server_started", PSI_FLAG_GLOBAL},
  { &key_delayed_insert_cond, "Delayed_insert::cond", 0},
  { &key_delayed_insert_cond_client, "Delayed_insert::cond_client", 0},
  { &key_item_func_sleep_cond, "Item_func_sleep::cond", 0},
  { &key_master_info_data_cond, "Master_info::data_cond", 0},
  { &key_master_info_start_cond, "Master_info::start_cond", 0},
  { &key_master_info_stop_cond, "Master_info::stop_cond", 0},
  { &key_master_info_sleep_cond, "Master_info::sleep_cond", 0},
  { &key_relay_log_info_data_cond, "Relay_log_info::data_cond", 0},
  { &key_relay_log_info_log_space_cond, "Relay_log_info::log_space_cond", 0},
  { &key_relay_log_info_start_cond, "Relay_log_info::start_cond", 0},
  { &key_relay_log_info_stop_cond, "Relay_log_info::stop_cond", 0},
  { &key_relay_log_info_sleep_cond, "Relay_log_info::sleep_cond", 0},
  { &key_TABLE_SHARE_cond, "TABLE_SHARE::cond", 0},
  { &key_user_level_lock_cond, "User_level_lock::cond", 0},
  { &key_COND_thread_count, "COND_thread_count", PSI_FLAG_GLOBAL},
  { &key_COND_thread_cache, "COND_thread_cache", PSI_FLAG_GLOBAL},
  { &key_COND_flush_thread_cache, "COND_flush_thread_cache", PSI_FLAG_GLOBAL}
};

PSI_thread_key key_thread_bootstrap, key_thread_delayed_insert,
  key_thread_handle_manager, key_thread_main,
  key_thread_one_connection, key_thread_signal_hand;

static PSI_thread_info all_server_threads[]=
{
#if (defined(_WIN32) || defined(HAVE_SMEM)) && !defined(EMBEDDED_LIBRARY)
  { &key_thread_handle_con_namedpipes, "con_named_pipes", PSI_FLAG_GLOBAL},
#endif /* _WIN32 || HAVE_SMEM && !EMBEDDED_LIBRARY */

#if defined(HAVE_SMEM) && !defined(EMBEDDED_LIBRARY)
  { &key_thread_handle_con_sharedmem, "con_shared_mem", PSI_FLAG_GLOBAL},
#endif /* HAVE_SMEM && !EMBEDDED_LIBRARY */

#if (defined(_WIN32) || defined(HAVE_SMEM)) && !defined(EMBEDDED_LIBRARY)
  { &key_thread_handle_con_sockets, "con_sockets", PSI_FLAG_GLOBAL},
#endif /* _WIN32 || HAVE_SMEM && !EMBEDDED_LIBRARY */

#ifdef __WIN__
  { &key_thread_handle_shutdown, "shutdown", PSI_FLAG_GLOBAL},
#endif /* __WIN__ */

  { &key_thread_bootstrap, "bootstrap", PSI_FLAG_GLOBAL},
  { &key_thread_delayed_insert, "delayed_insert", 0},
  { &key_thread_handle_manager, "manager", PSI_FLAG_GLOBAL},
  { &key_thread_main, "main", PSI_FLAG_GLOBAL},
  { &key_thread_one_connection, "one_connection", 0},
  { &key_thread_signal_hand, "signal_handler", PSI_FLAG_GLOBAL}
};

PSI_file_key key_file_binlog, key_file_binlog_index, key_file_casetest,
  key_file_dbopt, key_file_des_key_file, key_file_ERRMSG, key_select_to_file,
  key_file_fileparser, key_file_frm, key_file_global_ddl_log, key_file_load,
  key_file_loadfile, key_file_log_event_data, key_file_log_event_info,
  key_file_master_info, key_file_misc, key_file_partition,
  key_file_pid, key_file_relay_log_info, key_file_send_file, key_file_tclog,
  key_file_trg, key_file_trn, key_file_init;
PSI_file_key key_file_query_log, key_file_slow_log;
PSI_file_key key_file_relaylog, key_file_relaylog_index;

static PSI_file_info all_server_files[]=
{
  { &key_file_binlog, "binlog", 0},
  { &key_file_binlog_index, "binlog_index", 0},
  { &key_file_relaylog, "relaylog", 0},
  { &key_file_relaylog_index, "relaylog_index", 0},
  { &key_file_casetest, "casetest", 0},
  { &key_file_dbopt, "dbopt", 0},
  { &key_file_des_key_file, "des_key_file", 0},
  { &key_file_ERRMSG, "ERRMSG", 0},
  { &key_select_to_file, "select_to_file", 0},
  { &key_file_fileparser, "file_parser", 0},
  { &key_file_frm, "FRM", 0},
  { &key_file_global_ddl_log, "global_ddl_log", 0},
  { &key_file_load, "load", 0},
  { &key_file_loadfile, "LOAD_FILE", 0},
  { &key_file_log_event_data, "log_event_data", 0},
  { &key_file_log_event_info, "log_event_info", 0},
  { &key_file_master_info, "master_info", 0},
  { &key_file_misc, "misc", 0},
  { &key_file_partition, "partition", 0},
  { &key_file_pid, "pid", 0},
  { &key_file_query_log, "query_log", 0},
  { &key_file_relay_log_info, "relay_log_info", 0},
  { &key_file_send_file, "send_file", 0},
  { &key_file_slow_log, "slow_log", 0},
  { &key_file_tclog, "tclog", 0},
  { &key_file_trg, "trigger_name", 0},
  { &key_file_trn, "trigger", 0},
  { &key_file_init, "init", 0}
};

/**
  Initialise all the performance schema instrumentation points
  used by the server.
*/
void init_server_psi_keys(void)
{
  const char* category= "sql";
  int count;

  if (PSI_server == NULL)
    return;

  count= array_elements(all_server_mutexes);
  PSI_server->register_mutex(category, all_server_mutexes, count);

  count= array_elements(all_server_rwlocks);
  PSI_server->register_rwlock(category, all_server_rwlocks, count);

  count= array_elements(all_server_conds);
  PSI_server->register_cond(category, all_server_conds, count);

  count= array_elements(all_server_threads);
  PSI_server->register_thread(category, all_server_threads, count);

  count= array_elements(all_server_files);
  PSI_server->register_file(category, all_server_files, count);
}

#endif /* HAVE_PSI_INTERFACE */

/*
  Since buffered_option_error_reporter is only used currently
  for parsing performance schema options, this code is not needed
  when the performance schema is not compiled in.
*/
#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
/**
  A log message for the error log, buffered in memory.
  Log messages are temporarily buffered when generated before the error log
  is initialized, and then printed once the error log is ready.
*/
class Buffered_log : public Sql_alloc
{
public:
  Buffered_log(enum loglevel level, const char *message);

  ~Buffered_log()
  {}

  void print(void);

private:
  /** Log message level. */
  enum loglevel m_level;
  /** Log message text. */
  String m_message;
};

/**
  Constructor.
  @param level          the message log level
  @param message        the message text
*/
Buffered_log::Buffered_log(enum loglevel level, const char *message)
  : m_level(level), m_message()
{
  m_message.copy(message, strlen(message), &my_charset_latin1);
}

/**
  Print a buffered log to the real log file.
*/
void Buffered_log::print()
{
  /*
    Since messages are buffered, they can be printed out
    of order with other entries in the log.
    Add "Buffered xxx" to the message text to prevent confusion.
  */
  switch(m_level)
  {
  case ERROR_LEVEL:
    sql_print_error("Buffered error: %s\n", m_message.c_ptr_safe());
    break;
  case WARNING_LEVEL:
    sql_print_warning("Buffered warning: %s\n", m_message.c_ptr_safe());
    break;
  case INFORMATION_LEVEL:
    /*
      Messages printed as "information" still end up in the mysqld *error* log,
      but with a [Note] tag instead of an [ERROR] tag.
      While this is probably fine for a human reading the log,
      it is upsetting existing automated scripts used to parse logs,
      because such scripts are likely to not already handle [Note] properly.
      INFORMATION_LEVEL messages are simply silenced, on purpose,
      to avoid un needed verbosity.
    */
    break;
  }
}

/**
  Collection of all the buffered log messages.
*/
class Buffered_logs
{
public:
  Buffered_logs()
  {}

  ~Buffered_logs()
  {}

  void init();
  void cleanup();

  void buffer(enum loglevel m_level, const char *msg);
  void print();
private:
  /**
    Memory root to use to store buffered logs.
    This memory root lifespan is between init and cleanup.
    Once the buffered logs are printed, they are not needed anymore,
    and all the memory used is reclaimed.
  */
  MEM_ROOT m_root;
  /** List of buffered log messages. */
  List<Buffered_log> m_list;
};

void Buffered_logs::init()
{
  init_alloc_root(&m_root, 1024, 0);
}

void Buffered_logs::cleanup()
{
  m_list.delete_elements();
  free_root(&m_root, MYF(0));
}

/**
  Add a log message to the buffer.
*/
void Buffered_logs::buffer(enum loglevel level, const char *msg)
{
  /*
    Do not let Sql_alloc::operator new(size_t) allocate memory,
    there is no memory root associated with the main() thread.
    Give explicitly the proper memory root to use to
    Sql_alloc::operator new(size_t, MEM_ROOT *) instead.
  */
  Buffered_log *log= new (&m_root) Buffered_log(level, msg);
  if (log)
    m_list.push_back(log, &m_root);
}

/**
  Print buffered log messages.
*/
void Buffered_logs::print()
{
  Buffered_log *log;
  List_iterator_fast<Buffered_log> it(m_list);
  while ((log= it++))
    log->print();
}

/** Logs reported before a logger is available. */
static Buffered_logs buffered_logs;

#ifndef EMBEDDED_LIBRARY
/**
  Error reporter that buffer log messages.
  @param level          log message level
  @param format         log message format string
*/
C_MODE_START
static void buffered_option_error_reporter(enum loglevel level,
                                           const char *format, ...)
{
  va_list args;
  char buffer[1024];

  va_start(args, format);
  my_vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  buffered_logs.buffer(level, buffer);
}
C_MODE_END
#endif /* !EMBEDDED_LIBRARY */
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

static my_socket unix_sock, base_ip_sock, extra_ip_sock;
struct my_rnd_struct sql_rand; ///< used by sql_class.cc:THD::THD()

#ifndef EMBEDDED_LIBRARY
struct passwd *user_info;
static pthread_t select_thread;
#endif

/* OS specific variables */

#ifdef __WIN__
#undef	 getpid
#include <process.h>

static mysql_cond_t COND_handler_count;
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

#ifdef _WIN32
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
static const char *current_dbug_option="disabled";
#ifdef HAVE_LIBWRAP
const char *libwrapName= NULL;
int allow_severity = LOG_INFO;
int deny_severity = LOG_WARNING;
#endif
#ifdef HAVE_QUERY_CACHE
ulong query_cache_min_res_unit= QUERY_CACHE_MIN_RESULT_DATA_SIZE;
Query_cache query_cache;
#endif
#ifdef HAVE_SMEM
char *shared_memory_base_name= default_shared_memory_base_name;
my_bool opt_enable_shared_memory;
HANDLE smem_event_connect_request= 0;
#endif

my_bool opt_use_ssl  = 0;
char *opt_ssl_ca= NULL, *opt_ssl_capath= NULL, *opt_ssl_cert= NULL,
     *opt_ssl_cipher= NULL, *opt_ssl_key= NULL;

static scheduler_functions thread_scheduler_struct, extra_thread_scheduler_struct;
scheduler_functions *thread_scheduler= &thread_scheduler_struct,
                    *extra_thread_scheduler= &extra_thread_scheduler_struct;

#ifdef HAVE_OPENSSL
#include <openssl/crypto.h>
#ifndef HAVE_YASSL
typedef struct CRYPTO_dynlock_value
{
  mysql_rwlock_t lock;
} openssl_lock_t;

static openssl_lock_t *openssl_stdlocks;
static openssl_lock_t *openssl_dynlock_create(const char *, int);
static void openssl_dynlock_destroy(openssl_lock_t *, const char *, int);
static void openssl_lock_function(int, int, const char *, int);
static void openssl_lock(int, openssl_lock_t *, const char *, int);
static unsigned long openssl_id_function();
#endif
char *des_key_file;
#ifndef EMBEDDED_LIBRARY
struct st_VioSSLFd *ssl_acceptor_fd;
#endif
#endif /* HAVE_OPENSSL */

/**
  Number of currently active user connections. The variable is protected by
  LOCK_connection_count.
*/
uint connection_count= 0, extra_connection_count= 0;

/* Function declarations */

pthread_handler_t signal_hand(void *arg);
static int mysql_init_variables(void);
static int get_options(int *argc_ptr, char ***argv_ptr);
static bool add_terminator(DYNAMIC_ARRAY *options);
extern "C" my_bool mysqld_get_one_option(int, const struct my_option *, char *);
static int init_thread_environment();
static char *get_relative_path(const char *path);
static int fix_paths(void);
void handle_connections_sockets();
#ifdef _WIN32
pthread_handler_t handle_connections_sockets_thread(void *arg);
#endif
pthread_handler_t kill_server_thread(void *arg);
static void bootstrap(MYSQL_FILE *file);
static bool read_init_file(char *file_name);
#ifdef _WIN32
pthread_handler_t handle_connections_namedpipes(void *arg);
#endif
#ifdef HAVE_SMEM
pthread_handler_t handle_connections_shared_memory(void *arg);
#endif
pthread_handler_t handle_slave(void *arg);
static void clean_up(bool print_message);
static int test_if_case_insensitive(const char *dir_name);

#ifndef EMBEDDED_LIBRARY
static void usage(void);
static void start_signal_handler(void);
static void close_server_sock();
static void clean_up_mutexes(void);
static void wait_for_signal_thread_to_end(void);
static void create_pid_file();
static void mysqld_exit(int exit_code) __attribute__((noreturn));
#endif
static void end_ssl();


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
#if !defined(__WIN__)
  DBUG_PRINT("quit", ("waiting for select thread: 0x%lx",
                      (ulong) select_thread));
  mysql_mutex_lock(&LOCK_thread_count);

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
      error= mysql_cond_timedwait(&COND_thread_count, &LOCK_thread_count,
                                  &abstime);
      if (error != EINTR)
	break;
    }
#ifdef EXTRA_DEBUG
    if (error != 0 && error != ETIMEDOUT && !count++)
      sql_print_error("Got error %d from mysql_cond_timedwait", error);
#endif
    close_server_sock();
  }
  mysql_mutex_unlock(&LOCK_thread_count);
#endif /* __WIN__ */


  /* Abort listening to new connections */
  DBUG_PRINT("quit",("Closing sockets"));
  if (!opt_disable_networking )
  {
    if (base_ip_sock != INVALID_SOCKET)
    {
      (void) mysql_socket_shutdown(base_ip_sock, SHUT_RDWR);
      (void) closesocket(base_ip_sock);
      base_ip_sock= INVALID_SOCKET;
    }
    if (extra_ip_sock != INVALID_SOCKET)
    {
      (void) mysql_socket_shutdown(extra_ip_sock, SHUT_RDWR);
      (void) closesocket(extra_ip_sock);
      extra_ip_sock= INVALID_SOCKET;
    }
  }
#ifdef _WIN32
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
  mysql_mutex_lock(&LOCK_thread_count); // For unlink from list

  I_List_iterator<THD> it(threads);
  while ((tmp=it++))
  {
    DBUG_PRINT("quit",("Informing thread %ld that it's time to die",
		       tmp->thread_id));
    /* We skip slave threads & scheduler on this first loop through. */
    if (tmp->slave_thread)
      continue;

    tmp->killed= KILL_SERVER_HARD;
    MYSQL_CALLBACK(thread_scheduler, post_kill_notification, (tmp));
    mysql_mutex_lock(&tmp->LOCK_thd_data);
    if (tmp->mysys_var)
    {
      tmp->mysys_var->abort=1;
      mysql_mutex_lock(&tmp->mysys_var->mutex);
      if (tmp->mysys_var->current_cond)
      {
        uint i;
        for (i=0; i < 2; i++)
        {
          int ret= mysql_mutex_trylock(tmp->mysys_var->current_mutex);
          mysql_cond_broadcast(tmp->mysys_var->current_cond);
          if (!ret)
          {
            /* Thread has surely got the signal, unlock and abort */
            mysql_mutex_unlock(tmp->mysys_var->current_mutex);
            break;
          }
          sleep(1);
        }
      }
      mysql_mutex_unlock(&tmp->mysys_var->mutex);
    }
    mysql_mutex_unlock(&tmp->LOCK_thd_data);
  }
  mysql_mutex_unlock(&LOCK_thread_count); // For unlink from list

  Events::deinit();
  end_slave();

  /* Give threads time to die. */
  for (int i= 0; thread_count && i < 100; i++)
    my_sleep(20000);

  /*
    Force remaining threads to die by closing the connection to the client
    This will ensure that threads that are waiting for a command from the
    client on a blocking read call are aborted.
  */

  for (;;)
  {
    DBUG_PRINT("quit",("Locking LOCK_thread_count"));
    mysql_mutex_lock(&LOCK_thread_count); // For unlink from list
    if (!(tmp=threads.get()))
    {
      DBUG_PRINT("quit",("Unlocking LOCK_thread_count"));
      mysql_mutex_unlock(&LOCK_thread_count);
      break;
    }
#ifndef __bsdi__				// Bug in BSDI kernel
    if (tmp->vio_ok())
    {
      if (global_system_variables.log_warnings)
        sql_print_warning(ER_DEFAULT(ER_FORCING_CLOSE),my_progname,
                          tmp->thread_id,
                          (tmp->main_security_ctx.user ?
                           tmp->main_security_ctx.user : ""));
      close_connection(tmp,ER_SERVER_SHUTDOWN);
    }
#endif
    DBUG_PRINT("quit",("Unlocking LOCK_thread_count"));
    mysql_mutex_unlock(&LOCK_thread_count);
  }
  /* All threads has now been aborted */
  DBUG_PRINT("quit",("Waiting for threads to die (count=%u)",thread_count));
  mysql_mutex_lock(&LOCK_thread_count);
  while (thread_count)
  {
    mysql_cond_wait(&COND_thread_count, &LOCK_thread_count);
    DBUG_PRINT("quit",("One thread died (count=%u)",thread_count));
  }
  mysql_mutex_unlock(&LOCK_thread_count);

  DBUG_PRINT("quit",("close_connections thread"));
  DBUG_VOID_RETURN;
}


#ifdef HAVE_CLOSE_SERVER_SOCK
static void close_socket(my_socket sock, const char *info)
{
  DBUG_ENTER("close_socket");

  if (sock != INVALID_SOCKET)
  {
    DBUG_PRINT("info", ("calling shutdown on %s socket", info));
    (void) mysql_socket_shutdown(sock, SHUT_RDWR);
#if defined(__NETWARE__)
    /*
      The following code is disabled for normal systems as it causes MySQL
      to hang on AIX 4.3 during shutdown
    */
    DBUG_PRINT("info", ("calling closesocket on %s socket", info));
    (void) closesocket(tmp_sock);
#endif
  }
  DBUG_VOID_RETURN;
}
#endif


static void close_server_sock()
{
#ifdef HAVE_CLOSE_SERVER_SOCK
  DBUG_ENTER("close_server_sock");

  close_socket(base_ip_sock, "TCP/IP");
  close_socket(extra_ip_sock, "TCP/IP");
  close_socket(unix_sock, "unix/IP");

  if (unix_sock != INVALID_SOCKET)
    (void) unlink(mysqld_unix_port);
  base_ip_sock= extra_ip_sock= unix_sock= INVALID_SOCKET;

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
    if (mysql_thread_create(0, /* Not instrumented */
                            &tmp, &connection_attrib, kill_server_thread,
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

#if !defined(__WIN__)
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
    sql_print_information(ER_DEFAULT(ER_NORMAL_SHUTDOWN),my_progname);
  else
    sql_print_error(ER_DEFAULT(ER_GOT_SIGNAL),my_progname,sig); /* purecov: inspected */

#ifdef HAVE_SMEM
  /*
    Send event to smem_event_connect_request for aborting
  */
  if (opt_enable_shared_memory)
  {
    if (!SetEvent(smem_event_connect_request))
    {
      DBUG_PRINT("error",
                 ("Got error: %ld from SetEvent of smem_event_connect_request",
                  GetLastError()));
    }
  }
#endif

  close_connections();
  if (sig != MYSQL_KILL_SIGNAL &&
      sig != 0)
    unireg_abort(1);				/* purecov: inspected */
  else
    unireg_end();

  /* purecov: begin deadcode */
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


#if defined(USE_ONE_SIGNAL_HAND)
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
#if !defined(__WIN__)
  if (sig == SIGALRM)
    alarm(2);					/* reschedule alarm */
#endif
}

#ifndef EMBEDDED_LIBRARY

static void init_error_log_mutex()
{
  mysql_mutex_init(key_LOCK_error_log, &LOCK_error_log, MY_MUTEX_INIT_FAST);
}


static void clean_up_error_log_mutex()
{
  mysql_mutex_destroy(&LOCK_error_log);
}


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
#if defined(SIGNALS_DONT_BREAK_READ)
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
  clean_up(!opt_abort && (exit_code || !opt_bootstrap)); /* purecov: inspected */
  DBUG_PRINT("quit",("done with cleanup in unireg_abort"));
  mysqld_exit(exit_code);
}

static void mysqld_exit(int exit_code)
{
  /*
    Important note: we wait for the signal thread to end,
    but if a kill -15 signal was sent, the signal thread did
    spawn the kill_server_thread thread, which is running concurrently.
  */
  wait_for_signal_thread_to_end();
  mysql_audit_finalize();
  clean_up_mutexes();
  clean_up_error_log_mutex();
  my_end((opt_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0));
  shutdown_performance_schema();        // we do it as late as possible
  exit(exit_code); /* purecov: inspected */
}

#endif /* !EMBEDDED_LIBRARY */

void clean_up(bool print_message)
{
  DBUG_PRINT("exit",("clean_up"));
  if (cleanup_done++)
    return; /* purecov: inspected */

  close_active_mi();
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
  my_dboptions_cache_free();
  ignore_db_dirs_free();
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  servers_free(1);
  acl_free(1);
  grant_free();
#endif
  query_cache_destroy();
  hostname_cache_free();
  item_user_lock_free();
  lex_free();				/* Free some memory */
  item_create_cleanup();
  free_charsets();
  if (!opt_noacl)
  {
#ifdef HAVE_DLOPEN
    udf_free();
#endif
  }
  table_def_start_shutdown();
  plugin_shutdown();
  ha_end();
  if (tc_log)
    tc_log->close();
  delegates_destroy();
  xid_cache_free();
  table_def_free();
  mdl_destroy();
  key_caches.delete_elements((void (*)(const char*, uchar*)) free_key_cache);
  wt_end();
  multi_keycache_free();
  sp_cache_end();
  free_status_vars();
  end_thr_alarm(1);			/* Free allocated memory */
  my_free_open_file_info();
  if (defaults_argv)
    free_defaults(defaults_argv);
  free_tmpdir(&mysql_tmpdir_list);
  bitmap_free(&temp_pool);
  free_max_user_conn();
  free_global_user_stats();
  free_global_client_stats();
  free_global_table_stats();
  free_global_index_stats();
  delete_dynamic(&all_options);
#ifdef HAVE_REPLICATION
  end_slave_list();
#endif
  my_uuid_end();
  delete binlog_filter;
  delete rpl_filter;
  end_ssl();
  vio_end();
  my_regex_end();
#if defined(ENABLED_DEBUG_SYNC)
  /* End the debug sync facility. See debug_sync.cc. */
  debug_sync_end();
#endif /* defined(ENABLED_DEBUG_SYNC) */

#if !defined(EMBEDDED_LIBRARY)
  if (!opt_bootstrap)
    mysql_file_delete(key_file_pid, pidfile_name, MYF(0)); // This may not always exist
#endif
  if (print_message && my_default_lc_messages && server_start_time)
    sql_print_information(ER_DEFAULT(ER_SHUTDOWN_COMPLETE),my_progname);
  cleanup_errmsgs();
  MYSQL_CALLBACK(thread_scheduler, end, ());
  mysql_library_end();
  finish_client_errs();
  (void) my_error_unregister(ER_ERROR_FIRST, ER_ERROR_LAST); // finish server errs
  DBUG_PRINT("quit", ("Error messages freed"));
  /* Tell main we are ready */
  logger.cleanup_end();
  sys_var_end();
  my_atomic_rwlock_destroy(&global_query_id_lock);
  my_atomic_rwlock_destroy(&thread_running_lock);
  mysql_mutex_lock(&LOCK_thread_count);
  DBUG_PRINT("quit", ("got thread count lock"));
  ready_to_exit=1;
  /* do the broadcast inside the lock to ensure that my_end() is not called */
  mysql_cond_broadcast(&COND_thread_count);
  mysql_mutex_unlock(&LOCK_thread_count);

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
  uint i;
  /*
    Wait up to 10 seconds for signal thread to die. We use this mainly to
    avoid getting warnings that my_thread_end has not been called
  */
  for (i= 0 ; i < 100 && signal_thread_in_use; i++)
  {
    if (pthread_kill(signal_thread, MYSQL_KILL_SIGNAL) == ESRCH)
      break;
    my_sleep(100);				// Give it time to die
  }
}
#endif /*EMBEDDED_LIBRARY*/

static void clean_up_mutexes()
{
  DBUG_ENTER("clean_up_mutexes");
  mysql_rwlock_destroy(&LOCK_grant);
  mysql_mutex_destroy(&LOCK_thread_count);
  mysql_mutex_destroy(&LOCK_status);
  mysql_mutex_destroy(&LOCK_delayed_insert);
  mysql_mutex_destroy(&LOCK_delayed_status);
  mysql_mutex_destroy(&LOCK_delayed_create);
  mysql_mutex_destroy(&LOCK_crypt);
  mysql_mutex_destroy(&LOCK_user_conn);
  mysql_mutex_destroy(&LOCK_connection_count);
  mysql_mutex_destroy(&LOCK_stats);
  mysql_mutex_destroy(&LOCK_global_user_client_stats);
  mysql_mutex_destroy(&LOCK_global_table_stats);
  mysql_mutex_destroy(&LOCK_global_index_stats);
#ifdef HAVE_OPENSSL
  mysql_mutex_destroy(&LOCK_des_key_file);
#ifndef HAVE_YASSL
  for (int i= 0; i < CRYPTO_num_locks(); ++i)
    mysql_rwlock_destroy(&openssl_stdlocks[i].lock);
  OPENSSL_free(openssl_stdlocks);
#endif /* HAVE_YASSL */
#endif /* HAVE_OPENSSL */
#ifdef HAVE_REPLICATION
  mysql_mutex_destroy(&LOCK_rpl_status);
  mysql_cond_destroy(&COND_rpl_status);
#endif /* HAVE_REPLICATION */
  mysql_mutex_destroy(&LOCK_active_mi);
  mysql_rwlock_destroy(&LOCK_sys_init_connect);
  mysql_rwlock_destroy(&LOCK_sys_init_slave);
  mysql_mutex_destroy(&LOCK_global_system_variables);
  mysql_rwlock_destroy(&LOCK_system_variables_hash);
  mysql_mutex_destroy(&LOCK_short_uuid_generator);
  mysql_mutex_destroy(&LOCK_prepared_stmt_count);
  mysql_mutex_destroy(&LOCK_error_messages);
  mysql_cond_destroy(&COND_thread_count);
  mysql_cond_destroy(&COND_thread_cache);
  mysql_cond_destroy(&COND_flush_thread_cache);
  mysql_mutex_destroy(&LOCK_server_started);
  mysql_cond_destroy(&COND_server_started);
  mysql_mutex_destroy(&LOCK_prepare_ordered);
  mysql_mutex_destroy(&LOCK_commit_ordered);
  DBUG_VOID_RETURN;
}


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
#if !defined(__WIN__)
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
#endif
  return NULL;
}

static inline void allow_coredumps()
{
#ifdef PR_SET_DUMPABLE
  if (test_flags & TEST_CORE_ON_SIGNAL)
  {
    /* inform kernel that process is dumpable */
    (void) prctl(PR_SET_DUMPABLE, 1);
  }
#endif
}


static void set_user(const char *user, struct passwd *user_info_arg)
{
  /* purecov: begin tested */
#if !defined(__WIN__)
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
  allow_coredumps();
#endif
  /* purecov: end */
}


static void set_effective_user(struct passwd *user_info_arg)
{
#if !defined(__WIN__)
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
  allow_coredumps();
#endif
}


/** Change root user if started with @c --chroot . */
static void set_root(const char *path)
{
#if !defined(__WIN__)
  if (chroot(path) == -1)
  {
    sql_perror("chroot");
    unireg_abort(1);
  }
  my_setwd("/", MYF(0));
#endif
}

/**
   Activate usage of a tcp port
*/

static my_socket activate_tcp_port(uint port)
{
  struct addrinfo *ai, *a;
  struct addrinfo hints;
  int error;
  int	arg;
  char port_buf[NI_MAXSERV];
  my_socket ip_sock= INVALID_SOCKET;
  DBUG_ENTER("activate_tcp_port");
  DBUG_PRINT("general",("IP Socket is %d",port));

  bzero(&hints, sizeof (hints));
  hints.ai_flags= AI_PASSIVE;
  hints.ai_socktype= SOCK_STREAM;
  hints.ai_family= AF_UNSPEC;

  my_snprintf(port_buf, NI_MAXSERV, "%d", port);
  error= getaddrinfo(my_bind_addr_str, port_buf, &hints, &ai);
  if (error != 0)
  {
    DBUG_PRINT("error",("Got error: %d from getaddrinfo()", error));
    sql_perror(ER_DEFAULT(ER_IPSOCK_ERROR));  /* purecov: tested */
    unireg_abort(1);				/* purecov: tested */
  }

  for (a= ai; a != NULL; a= a->ai_next)
  {
    ip_sock= socket(a->ai_family, a->ai_socktype, a->ai_protocol);
    if (ip_sock != INVALID_SOCKET)
      break;
  }

  if (ip_sock == INVALID_SOCKET)
  {
    DBUG_PRINT("error",("Got error: %d from socket()",socket_errno));
    sql_perror(ER_DEFAULT(ER_IPSOCK_ERROR));  /* purecov: tested */
    unireg_abort(1);				/* purecov: tested */
  }

#ifndef __WIN__
  /*
    We should not use SO_REUSEADDR on windows as this would enable a
    user to open two mysqld servers with the same TCP/IP port.
  */
  arg= 1;
  (void) setsockopt(ip_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&arg,sizeof(arg));
#endif /* __WIN__ */

#ifdef IPV6_V6ONLY
   /*
     For interoperability with older clients, IPv6 socket should
     listen on both IPv6 and IPv4 wildcard addresses.
     Turn off IPV6_V6ONLY option.

     NOTE: this will work starting from Windows Vista only.
     On Windows XP dual stack is not available, so it will not
     listen on the corresponding IPv4-address.
   */
  if (a->ai_family == AF_INET6)
  {
    arg= 0;
    (void) setsockopt(ip_sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&arg,
              sizeof(arg));
  }
#endif
  /*
    Sometimes the port is not released fast enough when stopping and
    restarting the server. This happens quite often with the test suite
    on busy Linux systems. Retry to bind the address at these intervals:
    Sleep intervals: 1, 2, 4,  6,  9, 13, 17, 22, ...
    Retry at second: 1, 3, 7, 13, 22, 35, 52, 74, ...
    Limit the sequence by mysqld_port_timeout (set --port-open-timeout=#).
  */
  int ret;
  uint waited, retry, this_wait;
  for (waited= 0, retry= 1; ; retry++, waited+= this_wait)
  {
    if (((ret= bind(ip_sock, a->ai_addr, a->ai_addrlen)) >= 0 ) ||
        (socket_errno != SOCKET_EADDRINUSE) ||
        (waited >= mysqld_port_timeout))
      break;
    sql_print_information("Retrying bind on TCP/IP port %u", port);
    this_wait= retry * retry / 3 + 1;
    sleep(this_wait);
  }
  freeaddrinfo(ai);
  if (ret < 0)
  {
    char buff[100];
    sprintf(buff, "Can't start server: Bind on TCP/IP port. Got error: %d",
            (int) socket_errno);
    sql_perror(buff);
    sql_print_error("Do you already have another mysqld server running on "
                    "port: %u ?", port);
    unireg_abort(1);
  }
  if (listen(ip_sock,(int) back_log) < 0)
  {
    sql_perror("Can't start server: listen() on TCP/IP port");
    sql_print_error("listen() on TCP/IP failed with error %d",
                    socket_errno);
    unireg_abort(1);
  }
  DBUG_RETURN(ip_sock);
}

static void network_init(void)
{
#ifdef HAVE_SYS_UN_H
  struct sockaddr_un	UNIXaddr;
  int	arg;
#endif
  DBUG_ENTER("network_init");

  if (MYSQL_CALLBACK_ELSE(thread_scheduler, init, (), 0))
    unireg_abort(1);			/* purecov: inspected */

  set_ports();

  if (report_port == 0)
  {
    report_port= mysqld_port;
  }
#ifndef DBUG_OFF
  if (!opt_disable_networking)
    DBUG_ASSERT(report_port != 0);
#endif
  if (!opt_disable_networking && !opt_bootstrap)
  {
    if (mysqld_port)
      base_ip_sock= activate_tcp_port(mysqld_port);
    if (mysqld_extra_port)
      extra_ip_sock= activate_tcp_port(mysqld_extra_port);
  }

#ifdef _WIN32
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
    arg= 1;
    (void) setsockopt(unix_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&arg,
		      sizeof(arg));
    umask(0);
    if (bind(unix_sock, reinterpret_cast<struct sockaddr *>(&UNIXaddr),
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


/**
  Close a connection.

  @param thd        Thread handle.
  @param sql_errno  The error code to send before disconnect.

  @note
    For the connection that is doing shutdown, this is called twice
*/
void close_connection(THD *thd, uint sql_errno)
{
  DBUG_ENTER("close_connection");

  if (sql_errno)
    net_send_error(thd, sql_errno, ER_DEFAULT(sql_errno), NULL);

  thd->print_aborted_warning(3, sql_errno ? ER_DEFAULT(sql_errno)
                                          : "CLOSE_CONNECTION");

  thd->disconnect();

  MYSQL_CONNECTION_DONE((int) sql_errno, thd->thread_id);

  if (MYSQL_CONNECTION_DONE_ENABLED())
  {
    sleep(0); /* Workaround to avoid tailcall optimisation */
  }
  MYSQL_AUDIT_NOTIFY_CONNECTION_DISCONNECT(thd, sql_errno);
  DBUG_VOID_RETURN;
}
#endif /* EMBEDDED_LIBRARY */


/** Called when mysqld is aborted with ^C */
/* ARGSUSED */
extern "C" sig_handler end_mysqld_signal(int sig __attribute__((unused)))
{
  DBUG_ENTER("end_mysqld_signal");
  /* Don't call kill_mysql() if signal thread is not running */
  if (signal_thread_in_use)
    kill_mysql();                          // Take down mysqld nicely
  DBUG_VOID_RETURN;				/* purecov: deadcode */
}


/*
  Cleanup THD object

  SYNOPSIS
    thd_cleanup()
    thd		 Thread handler
*/

void thd_cleanup(THD *thd)
{
  thd->cleanup();
}

/*
  Decrease number of connections

  SYNOPSIS
    dec_connection_count()
*/

void dec_connection_count(THD *thd)
{
  mysql_mutex_lock(&LOCK_connection_count);
  (*thd->scheduler->connection_count)--;
  mysql_mutex_unlock(&LOCK_connection_count);
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

  thd_cleanup(thd);
  dec_connection_count(thd);

  mysql_mutex_lock(&LOCK_status);
  add_to_status(&global_status_var, &thd->status_var);
  mysql_mutex_unlock(&LOCK_status);

  mysql_mutex_lock(&LOCK_thread_count);
  thread_count--;
  thd->unlink();
  /*
    Used by binlog_reset_master.  It would be cleaner to use
    DEBUG_SYNC here, but that's not possible because the THD's debug
    sync feature has been shut down at this point.
  */
  DBUG_EXECUTE_IF("sleep_after_lock_thread_count_before_delete_thd", sleep(5););
  mysql_mutex_unlock(&LOCK_thread_count);

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
  mysql_mutex_assert_owner(&LOCK_thread_count);
  if (cached_thread_count < thread_cache_size &&
      ! abort_loop && !kill_cached_threads)
  {
    /* Don't kill the thread, just put it in cache for reuse */
    DBUG_PRINT("info", ("Adding thread to cache"));
    cached_thread_count++;

#ifdef HAVE_PSI_INTERFACE
    /*
      Delete the instrumentation for the job that just completed,
      before parking this pthread in the cache (blocked on COND_thread_cache).
    */
    if (likely(PSI_server != NULL))
      PSI_server->delete_current_thread();
#endif

    while (!abort_loop && ! wake_thread && ! kill_cached_threads)
      mysql_cond_wait(&COND_thread_cache, &LOCK_thread_count);
    cached_thread_count--;
    if (kill_cached_threads)
      mysql_cond_signal(&COND_flush_thread_cache);
    if (wake_thread)
    {
      THD *thd;
      wake_thread--;
      thd= thread_cache.get();
      thd->thread_stack= (char*) &thd;          // For store_globals
      (void) thd->store_globals();

#ifdef HAVE_PSI_INTERFACE
      /*
        Create new instrumentation for the new THD job,
        and attach it to this running pthread.
      */
      if (likely(PSI_server != NULL))
      {
        PSI_thread *psi= PSI_server->new_thread(key_thread_one_connection,
                                                thd, thd->thread_id);
        if (likely(psi != NULL))
          PSI_server->set_thread(psi);
      }
#endif

      /*
        THD::mysys_var::abort is associated with physical thread rather
        than with THD object. So we need to reset this flag before using
        this thread for handling of new THD object/connection.
      */
      thd->mysys_var->abort= 0;
      thd->thr_create_utime= microsecond_interval_timer();
      thd->start_utime= thd->thr_create_utime;
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
  /* Mark that current_thd is not valid anymore */
  my_pthread_setspecific_ptr(THR_THD,  0);
  if (put_in_cache)
  {
    mysql_mutex_lock(&LOCK_thread_count);
    put_in_cache= cache_thread();
    mysql_mutex_unlock(&LOCK_thread_count);
    if (put_in_cache)
      DBUG_RETURN(0);                             // Thread is reused
  }

  /* It's safe to broadcast outside a lock (COND... is not deleted here) */
  DBUG_PRINT("signal", ("Broadcasting COND_thread_count"));
  DBUG_LEAVE;                                   // Must match DBUG_ENTER()
  my_thread_end();
  mysql_cond_broadcast(&COND_thread_count);

  pthread_exit(0);
  return 0;                                     // Avoid compiler warnings
}


void flush_thread_cache()
{
  mysql_mutex_lock(&LOCK_thread_count);
  kill_cached_threads++;
  while (cached_thread_count)
  {
    mysql_cond_broadcast(&COND_thread_cache);
    mysql_cond_wait(&COND_flush_thread_cache, &LOCK_thread_count);
  }
  kill_cached_threads--;
  mysql_mutex_unlock(&LOCK_thread_count);
}


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




#ifdef DEBUG_UNHANDLED_EXCEPTION_FILTER
#define DEBUGGER_ATTACH_TIMEOUT 120
/*
  Wait for debugger to attach and break into debugger. If debugger is
  not attached, resume after timeout.
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

#endif /* __WIN__ */


#if BACKTRACE_DEMANGLE
#include <cxxabi.h>
extern "C" char *my_demangle(const char *mangled_name, int *status)
{
  return abi::__cxa_demangle(mangled_name, NULL, NULL, status);
}
#endif


/*
  pthread_attr_setstacksize() without so much platform-dependency

  Return: The actual stack size if possible.
*/

#ifndef EMBEDDED_LIBRARY
static size_t my_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
  size_t guard_size __attribute__((unused))= 0;

#if defined(__ia64__) || defined(__ia64)
  /*
    On IA64, half of the requested stack size is used for "normal stack"
    and half for "register stack".  The space measured by check_stack_overrun
    is the "normal stack", so double the request to make sure we have the
    caller-expected amount of normal stack.

    NOTE: there is no guarantee that the register stack can't grow faster
    than normal stack, so it's very unclear that we won't dump core due to
    stack overrun despite check_stack_overrun's efforts.  Experimentation
    shows that in the execution_constants test, the register stack grows
    less than half as fast as normal stack, but perhaps other scenarios are
    less forgiving.  If it turns out that more space is needed for the
    register stack, that could be forced (rather inefficiently) by using a
    multiplier higher than 2 here.
  */
  stacksize *= 2;
#endif

  /*
    On many machines, the "guard space" is subtracted from the requested
    stack size, and that space is quite large on some platforms.  So add
    it to our request, if we can find out what it is.
  */
#ifdef HAVE_PTHREAD_ATTR_GETGUARDSIZE
  if (pthread_attr_getguardsize(attr, &guard_size))
    guard_size = 0;		/* if can't find it out, treat as 0 */
#endif

  pthread_attr_setstacksize(attr, stacksize + guard_size);

  /* Retrieve actual stack size if possible */
#ifdef HAVE_PTHREAD_ATTR_GETSTACKSIZE
  {
    size_t real_stack_size= 0;
    /* We must ignore real_stack_size = 0 as Solaris 2.9 can return 0 here */
    if (pthread_attr_getstacksize(attr, &real_stack_size) == 0 &&
	real_stack_size > guard_size)
    {
      real_stack_size -= guard_size;
      if (real_stack_size < stacksize)
      {
	if (global_system_variables.log_warnings)
          sql_print_warning("Asked for %zu thread stack, but got %zu",
                            stacksize, real_stack_size);
	stacksize= real_stack_size;
      }
    }
  }
#endif /* !EMBEDDED_LIBRARY */

#if defined(__ia64__) || defined(__ia64)
  stacksize /= 2;
#endif
  return stacksize;
}
#endif


#if !defined(__WIN__)
#ifndef SA_RESETHAND
#define SA_RESETHAND 0
#endif /* SA_RESETHAND */
#ifndef SA_NODEFER
#define SA_NODEFER 0
#endif /* SA_NODEFER */

#ifndef EMBEDDED_LIBRARY

static void init_signals(void)
{
  sigset_t set;
  struct sigaction sa;
  DBUG_ENTER("init_signals");

  my_sigset(THR_SERVER_ALARM,print_signal_warning); // Should never be called!

  if (opt_stack_trace || (test_flags & TEST_CORE_ON_SIGNAL))
  {
    sa.sa_flags = SA_RESETHAND | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sigprocmask(SIG_SETMASK,&sa.sa_mask,NULL);

    my_init_stacktrace();
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
    rl.rlim_cur = rl.rlim_max = (rlim_t) RLIM_INFINITY;
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
    /* Allow SIGINT to break mysqld. This is for debugging with --gdb */
    my_sigset(SIGINT, end_mysqld_signal);
    sigdelset(&set, SIGINT);
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
  pthread_attr_setscope(&thr_attr,PTHREAD_SCOPE_SYSTEM);
  (void) pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED);
  (void) my_setstacksize(&thr_attr,my_thread_stack_size);

  mysql_mutex_lock(&LOCK_thread_count);
  if ((error= mysql_thread_create(key_thread_signal_hand,
                                  &signal_thread, &thr_attr, signal_hand, 0)))
  {
    sql_print_error("Can't create interrupt-thread (error %d, errno: %d)",
		    error,errno);
    exit(1);
  }
  mysql_cond_wait(&COND_thread_count, &LOCK_thread_count);
  mysql_mutex_unlock(&LOCK_thread_count);

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
  init_thr_alarm(thread_scheduler->max_threads + extra_max_connections +
		 global_system_variables.max_insert_delayed_threads + 10);
  if (test_flags & TEST_SIGINT)
  {
    /* Allow SIGINT to break mysqld. This is for debugging with --gdb */
    (void) sigemptyset(&set);
    (void) sigaddset(&set,SIGINT);
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
    should not be any other mysql_cond_signal() calls.
  */
  mysql_mutex_lock(&LOCK_thread_count);
  mysql_mutex_unlock(&LOCK_thread_count);
  mysql_cond_broadcast(&COND_thread_count);

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
      DBUG_LEAVE;                               // Must match DBUG_ENTER()
      signal_thread_in_use= 0;
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
#ifdef HAVE_PSI_INTERFACE
        /* Delete the instrumentation for the signal thread */
        if (likely(PSI_server != NULL))
          PSI_server->delete_current_thread();
#endif
#ifdef USE_ONE_SIGNAL_HAND
	pthread_t tmp;
        if (mysql_thread_create(0, /* Not instrumented */
                                &tmp, &connection_attrib, kill_server_thread,
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
extern "C" void my_message_sql(uint error, const char *str, myf MyFlags);

void my_message_sql(uint error, const char *str, myf MyFlags)
{
  THD *thd= current_thd;
  MYSQL_ERROR::enum_warning_level level;
  sql_print_message_func func;

  DBUG_ENTER("my_message_sql");
  DBUG_PRINT("error", ("error: %u  message: '%s'  Flag: %d", error, str, MyFlags));

  DBUG_ASSERT(str != NULL);
  DBUG_ASSERT(error != 0);

  mysql_audit_general(thd, MYSQL_AUDIT_GENERAL_ERROR, error, str);
  if (MyFlags & ME_JUST_INFO)
  {
    level= MYSQL_ERROR::WARN_LEVEL_NOTE;
    func= sql_print_information;
  }
  else if (MyFlags & ME_JUST_WARNING)
  {
    level= MYSQL_ERROR::WARN_LEVEL_WARN;
    func= sql_print_warning;
  }
  else
  {
    level= MYSQL_ERROR::WARN_LEVEL_ERROR;
    func= sql_print_error;
  }

  if (thd)
  {
    if (MyFlags & ME_FATALERROR)
      thd->is_fatal_error= 1;
    (void) thd->raise_condition(error, NULL, level, str);
  }

  /* When simulating OOM, skip writing to error log to avoid mtr errors */
  DBUG_EXECUTE_IF("simulate_out_of_memory", DBUG_VOID_RETURN;);

  if (!thd || thd->log_all_errors || (MyFlags & ME_NOREFRESH))
    (*func)("%s: %s", my_progname_short, str); /* purecov: inspected */
  DBUG_VOID_RETURN;
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
  my_free(ptr);
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
"mysqld", "server", MYSQL_BASE_VERSION,
"mariadb", MARIADB_BASE_VERSION,
"client-server",
0, 0};

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

  @retval
    0 ok
  @retval
    1 error
*/

static bool init_global_datetime_format(timestamp_type format_type,
                                        DATE_TIME_FORMAT *format)
{
  /*
    Get command line option
    format->format.str is already set by my_getopt
  */
  format->format.length= strlen(format->format.str);

  if (parse_date_time_format(format_type, format))
  {
    fprintf(stderr, "Wrong date/time format specifier: %s\n",
            format->format.str);
    return true;
  }
  return false;
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
  {"resignal",             (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_RESIGNAL]), SHOW_LONG_STATUS},
  {"revoke",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_REVOKE]), SHOW_LONG_STATUS},
  {"revoke_all",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_REVOKE_ALL]), SHOW_LONG_STATUS},
  {"rollback",             (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ROLLBACK]), SHOW_LONG_STATUS},
  {"rollback_to_savepoint",(char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ROLLBACK_TO_SAVEPOINT]), SHOW_LONG_STATUS},
  {"savepoint",            (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SAVEPOINT]), SHOW_LONG_STATUS},
  {"select",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SELECT]), SHOW_LONG_STATUS},
  {"set_option",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SET_OPTION]), SHOW_LONG_STATUS},
  {"signal",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SIGNAL]), SHOW_LONG_STATUS},
  {"show_authors",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_AUTHORS]), SHOW_LONG_STATUS},
  {"show_binlog_events",   (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_BINLOG_EVENTS]), SHOW_LONG_STATUS},
  {"show_binlogs",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_BINLOGS]), SHOW_LONG_STATUS},
  {"show_charsets",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_CHARSETS]), SHOW_LONG_STATUS},
  {"show_client_statistics",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_CLIENT_STATS]), SHOW_LONG_STATUS},
  {"show_collations",      (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_COLLATIONS]), SHOW_LONG_STATUS},
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
  {"show_index_statistics",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_INDEX_STATS]), SHOW_LONG_STATUS},
  {"show_master_status",   (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_MASTER_STAT]), SHOW_LONG_STATUS},
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
  {"show_relaylog_events", (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_RELAYLOG_EVENTS]), SHOW_LONG_STATUS},
  {"show_slave_hosts",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_SLAVE_HOSTS]), SHOW_LONG_STATUS},
  {"show_slave_status",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_SLAVE_STAT]), SHOW_LONG_STATUS},
  {"show_status",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_STATUS]), SHOW_LONG_STATUS},
  {"show_storage_engines", (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_STORAGE_ENGINES]), SHOW_LONG_STATUS},
  {"show_table_statistics",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_TABLE_STATS]), SHOW_LONG_STATUS},
  {"show_table_status",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_TABLE_STATUS]), SHOW_LONG_STATUS},
  {"show_tables",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_TABLES]), SHOW_LONG_STATUS},
  {"show_triggers",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_TRIGGERS]), SHOW_LONG_STATUS},
  {"show_user_statistics",      (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_USER_STATS]), SHOW_LONG_STATUS},
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

static int init_common_variables()
{
  umask(((~my_umask) & 0666));
  my_decimal_set_zero(&decimal_zero); // set decimal_zero constant;

  tzset();			// Set tzname

  sf_leaking_memory= 0; // no memory leaks from now on

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

  if (ignore_db_dirs_init())
    return 1;

#ifdef HAVE_TZNAME
  struct tm tm_tmp;
  localtime_r(&server_start_time,&tm_tmp);
  const char *tz_name=  tzname[tm_tmp.tm_isdst != 0 ? 1 : 0];
#ifdef _WIN32
  /*
    Time zone name may be localized and contain non-ASCII characters,
    Convert from ANSI encoding to UTF8.
  */
  wchar_t wtz_name[sizeof(system_time_zone)];
  mbstowcs(wtz_name, tz_name, sizeof(system_time_zone)-1);
  WideCharToMultiByte(CP_UTF8,0, wtz_name, -1, system_time_zone, 
    sizeof(system_time_zone) - 1, NULL, NULL);
#else
  strmake(system_time_zone, tz_name,  sizeof(system_time_zone)-1);
#endif /* _WIN32 */
#endif /* HAVE_TZNAME */

  /*
    We set SYSTEM time zone as reasonable default and
    also for failure of my_tz_init() and bootstrap mode.
    If user explicitly set time zone with --default-time-zone
    option we will change this value in my_tz_init().
  */
  global_system_variables.time_zone= my_tz_SYSTEM;

#ifdef HAVE_PSI_INTERFACE
  /*
    Complete the mysql_bin_log initialization.
    Instrumentation keys are known only after the performance schema initialization,
    and can not be set in the MYSQL_BIN_LOG constructor (called before main()).
  */
  mysql_bin_log.set_psi_keys(key_BINLOG_LOCK_index,
                             key_BINLOG_update_cond,
                             key_file_binlog,
                             key_file_binlog_index,
                             key_BINLOG_COND_queue_busy);
#endif

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
    /*
      Get hostname of computer (used by 'show variables') and as default
      basename for the pid file if --log-basename is not given.
    */
    strmake(glob_hostname, STRING_WITH_LEN("localhost"));
    sql_print_warning("gethostname failed, using '%s' as hostname",
                        glob_hostname);
    opt_log_basename= const_cast<char *>("mysql");
  }
  else
    opt_log_basename= glob_hostname;

  if (!*pidfile_name)
  {
    strmake(pidfile_name, opt_log_basename, sizeof(pidfile_name)-5);
    strmov(fn_ext(pidfile_name),".pid");		// Add proper extension
  }

  /*
    The default-storage-engine entry in my_long_options should have a
    non-null default value. It was earlier intialized as
    (longlong)"MyISAM" in my_long_options but this triggered a
    compiler error in the Sun Studio 12 compiler. As a work-around we
    set the def_value member to 0 in my_long_options and initialize it
    to the correct value here.

    From MySQL 5.5 onwards, the default storage engine is InnoDB
    (except in the embedded server, where the default continues to
    be MyISAM)
  */
#if defined(WITH_INNOBASE_STORAGE_ENGINE) || defined(WITH_XTRADB_STORAGE_ENGINE)
  default_storage_engine= const_cast<char *>("InnoDB");
#else
  default_storage_engine= const_cast<char *>("MyISAM");
#endif

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

  if (get_options(&remaining_argc, &remaining_argv))
    return 1;
  set_server_version();

#ifndef EMBEDDED_LIBRARY
  if (opt_abort && !opt_verbose)
    unireg_abort(0);
#endif /*!EMBEDDED_LIBRARY*/

  DBUG_PRINT("info",("%s  Ver %s for %s on %s\n",my_progname,
		     server_version, SYSTEM_TYPE,MACHINE_TYPE));

#ifdef HAVE_LARGE_PAGES
  /* Initialize large page size */
  if (opt_large_pages && (opt_large_page_size= my_get_large_page_size()))
  {
      DBUG_PRINT("info", ("Large page set, large_page_size = %d",
                 opt_large_page_size));
      my_use_large_pages= 1;
      my_large_page_size= opt_large_page_size;
  }
  else
  {
    opt_large_pages= 0;
    /* 
       Either not configured to use large pages or Linux haven't
       been compiled with large page support
    */
  }
#endif /* HAVE_LARGE_PAGES */
#ifdef HAVE_SOLARIS_LARGE_PAGES
#define LARGE_PAGESIZE (4*1024*1024)  /* 4MB */
#define SUPER_LARGE_PAGESIZE (256*1024*1024)  /* 256MB */
  if (opt_large_pages)
  {
  /*
    tell the kernel that we want to use 4/256MB page for heap storage
    and also for the stack. We use 4 MByte as default and if the
    super-large-page is set we increase it to 256 MByte. 256 MByte
    is for server installations with GBytes of RAM memory where
    the MySQL Server will have page caches and other memory regions
    measured in a number of GBytes.
    We use as big pages as possible which isn't bigger than the above
    desired page sizes.
  */
   int nelem;
   size_t max_desired_page_size;
   if (opt_super_large_pages)
     max_desired_page_size= SUPER_LARGE_PAGESIZE;
   else
     max_desired_page_size= LARGE_PAGESIZE;
   nelem = getpagesizes(NULL, 0);
   if (nelem > 0)
   {
     size_t *pagesize = (size_t *) malloc(sizeof(size_t) * nelem);
     if (pagesize != NULL && getpagesizes(pagesize, nelem) > 0)
     {
       size_t max_page_size= 0;
       for (int i= 0; i < nelem; i++)
       {
         if (pagesize[i] > max_page_size &&
             pagesize[i] <= max_desired_page_size)
            max_page_size= pagesize[i];
       }
       free(pagesize);
       if (max_page_size > 0)
       {
         struct memcntl_mha mpss;

         mpss.mha_cmd= MHA_MAPSIZE_BSSBRK;
         mpss.mha_pagesize= max_page_size;
         mpss.mha_flags= 0;
         memcntl(NULL, 0, MC_HAT_ADVISE, (caddr_t)&mpss, 0, 0);
         mpss.mha_cmd= MHA_MAPSIZE_STACK;
         memcntl(NULL, 0, MC_HAT_ADVISE, (caddr_t)&mpss, 0, 0);
       }
     }
   }
  }
#endif /* HAVE_SOLARIS_LARGE_PAGES */

  /* connections and databases needs lots of files */
  {
    uint files, wanted_files, max_open_files;

    /* MyISAM requires two file handles per table. */
    wanted_files= (10 + max_connections + extra_max_connections +
                   table_cache_size*2);
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
    max_open_files= max(max(wanted_files,
                            (max_connections + extra_max_connections)*5),
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
  if (!(my_default_lc_messages=
        my_locale_by_name(lc_messages)))
  {
    sql_print_error("Unknown locale: '%s'", lc_messages);
    return 1;
  }
  global_system_variables.lc_messages= my_default_lc_messages;
  if (init_errmessage())	/* Read error messages from file */
    return 1;
  init_client_errs();
  mysql_library_init(unused,unused,unused); /* for replication */
  lex_init();
  if (item_create_init())
    return 1;
  item_init();
#ifndef EMBEDDED_LIBRARY
  my_regex_init(&my_charset_latin1, check_enough_stack_size);
#else
  my_regex_init(&my_charset_latin1, NULL);
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
      sql_print_error(ER_DEFAULT(ER_UNKNOWN_COLLATION), default_collation_name);
      return 1;
    }
    if (!my_charset_same(default_charset_info, default_collation))
    {
      sql_print_error(ER_DEFAULT(ER_COLLATION_CHARSET_MISMATCH),
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
  global_system_variables.character_set_client=  default_charset_info;

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

  /* check log options and issue warnings if needed */
  if (opt_log && opt_logname && *opt_logname &&
      !(log_output_options & (LOG_FILE | LOG_NONE)))
    sql_print_warning("Although a path was specified for the "
                      "--log option, log tables are used. "
                      "To enable logging to files use the --log-output option.");

  if (opt_slow_log && opt_slow_logname && *opt_slow_logname &&
      !(log_output_options & (LOG_FILE | LOG_NONE)))
    sql_print_warning("Although a path was specified for the "
                      "--log-slow-queries option, log tables are used. "
                      "To enable logging to files use the --log-output=file option.");

  if (!opt_logname || !*opt_logname)
    make_default_log_name(&opt_logname, ".log", false);
  if (!opt_slow_logname || !*opt_slow_logname)
    make_default_log_name(&opt_slow_logname, "-slow.log", false);

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

  if (my_dboptions_cache_init())
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
           !(lower_case_file_system= (lower_case_file_system == 1)))
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
    lower_case_file_system= (lower_case_file_system == 1);
  }

  /* Reset table_alias_charset, now that lower_case_table_names is set. */
  table_alias_charset= (lower_case_table_names ?
			files_charset_info :
			&my_charset_bin);

  if (ignore_db_dirs_process_additions())
  {
    sql_print_error("An error occurred while storing ignore_db_dirs to a hash.");
    return 1;
  }

  return 0;
}


static int init_thread_environment()
{
  mysql_mutex_init(key_LOCK_thread_count, &LOCK_thread_count, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_status, &LOCK_status, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_delayed_insert,
                   &LOCK_delayed_insert, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_delayed_status,
                   &LOCK_delayed_status, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_delayed_create,
                   &LOCK_delayed_create, MY_MUTEX_INIT_SLOW);
  mysql_mutex_init(key_LOCK_crypt, &LOCK_crypt, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_user_conn, &LOCK_user_conn, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_active_mi, &LOCK_active_mi, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_global_system_variables,
                   &LOCK_global_system_variables, MY_MUTEX_INIT_FAST);
  mysql_rwlock_init(key_rwlock_LOCK_system_variables_hash,
                    &LOCK_system_variables_hash);
  mysql_mutex_init(key_LOCK_prepared_stmt_count,
                   &LOCK_prepared_stmt_count, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_error_messages,
                   &LOCK_error_messages, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_uuid_short_generator,
                   &LOCK_short_uuid_generator, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_connection_count,
                   &LOCK_connection_count, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_stats, &LOCK_stats, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_global_user_client_stats,
                   &LOCK_global_user_client_stats, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_global_table_stats,
                   &LOCK_global_table_stats, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_global_index_stats,
                   &LOCK_global_index_stats, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_prepare_ordered, &LOCK_prepare_ordered,
                   MY_MUTEX_INIT_SLOW);
  mysql_mutex_init(key_LOCK_commit_ordered, &LOCK_commit_ordered,
                   MY_MUTEX_INIT_SLOW);

#ifdef HAVE_OPENSSL
  mysql_mutex_init(key_LOCK_des_key_file,
                   &LOCK_des_key_file, MY_MUTEX_INIT_FAST);
#ifndef HAVE_YASSL
  openssl_stdlocks= (openssl_lock_t*) OPENSSL_malloc(CRYPTO_num_locks() *
                                                     sizeof(openssl_lock_t));
  for (int i= 0; i < CRYPTO_num_locks(); ++i)
    mysql_rwlock_init(key_rwlock_openssl, &openssl_stdlocks[i].lock);
  CRYPTO_set_dynlock_create_callback(openssl_dynlock_create);
  CRYPTO_set_dynlock_destroy_callback(openssl_dynlock_destroy);
  CRYPTO_set_dynlock_lock_callback(openssl_lock);
  CRYPTO_set_locking_callback(openssl_lock_function);
  CRYPTO_set_id_callback(openssl_id_function);
#endif
#endif
  mysql_rwlock_init(key_rwlock_LOCK_sys_init_connect, &LOCK_sys_init_connect);
  mysql_rwlock_init(key_rwlock_LOCK_sys_init_slave, &LOCK_sys_init_slave);
  mysql_rwlock_init(key_rwlock_LOCK_grant, &LOCK_grant);
  mysql_cond_init(key_COND_thread_count, &COND_thread_count, NULL);
  mysql_cond_init(key_COND_thread_cache, &COND_thread_cache, NULL);
  mysql_cond_init(key_COND_flush_thread_cache, &COND_flush_thread_cache, NULL);
#ifdef HAVE_REPLICATION
  mysql_mutex_init(key_LOCK_rpl_status, &LOCK_rpl_status, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_rpl_status, &COND_rpl_status, NULL);
#endif
  mysql_mutex_init(key_LOCK_server_started,
                   &LOCK_server_started, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_server_started, &COND_server_started, NULL);
  sp_cache_init();
#ifdef HAVE_EVENT_SCHEDULER
  Events::init_mutexes();
#endif
  /* Parameter for threads created for connections */
  (void) pthread_attr_init(&connection_attrib);
  (void) pthread_attr_setdetachstate(&connection_attrib,
				     PTHREAD_CREATE_DETACHED);
  pthread_attr_setscope(&connection_attrib, PTHREAD_SCOPE_SYSTEM);

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
  mysql_rwlock_init(key_rwlock_openssl, &lock->lock);
  return lock;
}


static void openssl_dynlock_destroy(openssl_lock_t *lock, const char *file,
				    int line)
{
  mysql_rwlock_destroy(&lock->lock);
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
    err= mysql_rwlock_rdlock(&lock->lock);
    break;
  case CRYPTO_LOCK|CRYPTO_WRITE:
    what = "write lock";
    err= mysql_rwlock_wrlock(&lock->lock);
    break;
  case CRYPTO_UNLOCK|CRYPTO_READ:
  case CRYPTO_UNLOCK|CRYPTO_WRITE:
    what = "unlock";
    err= mysql_rwlock_unlock(&lock->lock);
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


static void init_ssl()
{
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
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
#endif /* HAVE_OPENSSL && ! EMBEDDED_LIBRARY */
}


static void end_ssl()
{
#ifdef HAVE_OPENSSL
#ifndef EMBEDDED_LIBRARY
  if (ssl_acceptor_fd)
  {
    free_vio_ssl_acceptor_fd(ssl_acceptor_fd);
    ssl_acceptor_fd= 0;
  }
#endif /* ! EMBEDDED_LIBRARY */
#endif /* HAVE_OPENSSL */
}

#ifdef _WIN32
/**
  Registers a file to be collected when Windows Error Reporting creates a crash 
  report.

  @note only works on Vista and later, since WerRegisterFile() is not available
  on earlier Windows.
*/
#include <werapi.h>
static void add_file_to_crash_report(char *file)
{
  /* Load WerRegisterFile function dynamically.*/
  HRESULT (WINAPI *pWerRegisterFile)(PCWSTR, WER_REGISTER_FILE_TYPE, DWORD)
    =(HRESULT (WINAPI *) (PCWSTR, WER_REGISTER_FILE_TYPE, DWORD))
    GetProcAddress(GetModuleHandle("kernel32"),"WerRegisterFile");

  if (pWerRegisterFile)
  {
    wchar_t wfile[MAX_PATH+1]= {0};
    if (mbstowcs(wfile, file, MAX_PATH) != (size_t)-1)
    {
      pWerRegisterFile(wfile, WerRegFileTypeOther, WER_FILE_ANONYMOUS_DATA);
    }
  }
}
#endif

static int init_server_components()
{
  DBUG_ENTER("init_server_components");
  /*
    We need to call each of these following functions to ensure that
    all things are initialized so that unireg_abort() doesn't fail
  */
  mdl_init();
  if (table_def_init() | hostname_cache_init())
    unireg_abort(1);

  query_cache_set_min_res_unit(query_cache_min_res_unit);
  query_cache_init();
  query_cache_resize(query_cache_size);
  my_rnd_init(&sql_rand,(ulong) server_start_time,(ulong) server_start_time/2);
  setup_fpu();
  init_thr_lock();
  my_uuid_init((ulong) (my_rnd(&sql_rand))*12345,12345);
#ifdef HAVE_REPLICATION
  init_slave_list();
#endif
  wt_init();

  /* Setup logs */

  /*
    Enable old-fashioned error log, except when the user has requested
    help information. Since the implementation of plugin server
    variables the help output is now written much later.
  */
  if (opt_error_log && !opt_abort)
  {
    if (!log_error_file_ptr[0])
      fn_format(log_error_file, pidfile_name, mysql_data_home, ".err",
                MY_REPLACE_EXT); /* replace '.<domain>' by '.err', bug#4997 */
    else
      fn_format(log_error_file, log_error_file_ptr, mysql_data_home, ".err",
                MY_UNPACK_FILENAME | MY_SAFE_PATH);
    /*
      _ptr may have been set to my_disabled_option or "" if no argument was
      passed, but we need to show the real name in SHOW VARIABLES:
    */
    log_error_file_ptr= log_error_file;
    if (!log_error_file[0])
      opt_error_log= 0;                         // Too long file name
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

#ifdef _WIN32
      /* Add error log to windows crash reporting. */
      add_file_to_crash_report(log_error_file);
#endif
    }
  }

  /* set up the hook before initializing plugins which may use it */
  error_handler_hook= my_message_sql;
  proc_info_hook= set_thd_proc_info;

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  /*
    Parsing the performance schema command line option may have reported
    warnings/information messages.
    Now that the logger is finally available, and redirected
    to the proper file when the --log--error option is used,
    print the buffered messages to the log.
  */
  buffered_logs.print();
  buffered_logs.cleanup();
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

  if (xid_cache_init())
  {
    sql_print_error("Out of memory");
    unireg_abort(1);
  }

  /*
    initialize delegates for extension observers, errors have already
    been reported in the function
  */
  if (delegates_init())
    unireg_abort(1);

  /* need to configure logging before initializing storage engines */
  if (!opt_bin_log_used)
  {
    if (opt_log_slave_updates)
      sql_print_warning("You need to use --log-bin to make "
                        "--log-slave-updates work.");
    if (binlog_format_used)
      sql_print_warning("You need to use --log-bin to make "
                        "--binlog-format work.");
  }

  /* Check that we have not let the format to unspecified at this point */
  DBUG_ASSERT((uint)global_system_variables.binlog_format <=
              array_elements(binlog_format_names)-1);

#ifdef HAVE_REPLICATION
  if (opt_log_slave_updates && replicate_same_server_id)
  {
    if (opt_bin_log)
    {
      sql_print_error("using --replicate-same-server-id in conjunction with \
--log-slave-updates is impossible, it would lead to infinite loops in this \
server.");
      unireg_abort(1);
    }
    else
      sql_print_warning("using --replicate-same-server-id in conjunction with \
--log-slave-updates would lead to infinite loops in this server. However this \
will be ignored as the --log-bin option is not defined.");
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
      sql_print_warning("No argument was provided to --log-bin and "
                        "neither --log-basename or --log-bin-index where "
                        "used;  This may cause repliction to break when this "
                        "server acts as a master and has its hostname "
                        "changed! Please use '--log-basename=%s' or "
                        "'--log-bin=%s' to avoid this problem.",
                        opt_log_basename, ln);
    }
    if (ln == buf)
    {
      opt_bin_logname= my_once_strdup(buf, MYF(MY_WME));
    }
    if (mysql_bin_log.open_index_file(opt_binlog_index_name, ln, TRUE))
    {
      unireg_abort(1);
    }
  }

  /* call ha_init_key_cache() on all key caches to init them */
  process_key_caches(&ha_init_key_cache, 0);

  init_global_table_stats();
  init_global_index_stats();

  /* Allow storage engine to give real error messages */
  if (ha_init_errors())
    DBUG_RETURN(1);

  if (plugin_init(&remaining_argc, remaining_argv,
                  (opt_noacl ? PLUGIN_INIT_SKIP_PLUGIN_TABLE : 0) |
                  (opt_abort ? PLUGIN_INIT_SKIP_INITIALIZATION : 0)))
  {
    sql_print_error("Failed to initialize plugins.");
    unireg_abort(1);
  }
  plugins_are_initialized= TRUE;  /* Don't separate from init function */

  have_csv= plugin_status(STRING_WITH_LEN("csv"),
                          MYSQL_STORAGE_ENGINE_PLUGIN);
  have_ndbcluster= plugin_status(STRING_WITH_LEN("ndbcluster"),
                                 MYSQL_STORAGE_ENGINE_PLUGIN);
  have_partitioning= plugin_status(STRING_WITH_LEN("partition"),
                                   MYSQL_STORAGE_ENGINE_PLUGIN);

  /* we do want to exit if there are any other unknown options */
  if (remaining_argc > 1)
  {
    int ho_error;
    struct my_option no_opts[]=
    {
      {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
    };
    /*
      We need to eat any 'loose' arguments first before we conclude
      that there are unprocessed options.
    */
    my_getopt_skip_unknown= 0;

    if ((ho_error= handle_options(&remaining_argc, &remaining_argv, no_opts,
                                  mysqld_get_one_option)))
      unireg_abort(ho_error);
    /* Add back the program name handle_options removes */
    remaining_argc++;
    remaining_argv--;
    my_getopt_skip_unknown= TRUE;

    if (remaining_argc > 1)
    {
      fprintf(stderr, "%s: Too many arguments (first extra is '%s').\n",
              my_progname, remaining_argv[1]);
      unireg_abort(1);
    }
  }

  if (opt_abort)
    unireg_abort(0);

  /* if the errmsg.sys is not loaded, terminate to maintain behaviour */
  if (!DEFAULT_ERRMSGS[0][0])
    unireg_abort(1);  

  /* We have to initialize the storage engines before CSV logging */
  if (ha_init())
  {
    sql_print_error("Can't init databases");
    unireg_abort(1);
  }

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

  /*
    Set the default storage engine
  */
  LEX_STRING name= { default_storage_engine, strlen(default_storage_engine) };
  plugin_ref plugin;
  handlerton *hton;
  if ((plugin= ha_resolve_by_name(0, &name)))
    hton= plugin_data(plugin, handlerton*);
  else
  {
    sql_print_error("Unknown/unsupported storage engine: %s",
                    default_storage_engine);
    unireg_abort(1);
  }
  if (!ha_storage_engine_is_enabled(hton))
  {
    if (!opt_bootstrap)
    {
      sql_print_error("Default storage engine (%s) is not available",
                      default_storage_engine);
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
    mysql_mutex_lock(&LOCK_global_system_variables);
    plugin_unlock(0, global_system_variables.table_plugin);
    global_system_variables.table_plugin= plugin;
    mysql_mutex_unlock(&LOCK_global_system_variables);
  }
#if defined(WITH_ARIA_STORAGE_ENGINE) && defined(USE_ARIA_FOR_TMP_TABLES)
  if (!ha_storage_engine_is_enabled(maria_hton) && !opt_bootstrap)
  {
    sql_print_error("Aria engine is not enabled or did not start. The Aria engine must be enabled to continue as mysqld was configured with --with-aria-tmp-tables");
    unireg_abort(1);
  }
  internal_tmp_table_max_key_length=   maria_max_key_length();
  internal_tmp_table_max_key_segments= maria_max_key_segments();
#else
  internal_tmp_table_max_key_length=   myisam_max_key_length();
  internal_tmp_table_max_key_segments= myisam_max_key_segments();
#endif

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
  init_global_user_stats();
  init_global_client_stats();
  DBUG_RETURN(0);
}


#ifndef EMBEDDED_LIBRARY

static void create_shutdown_thread()
{
#ifdef __WIN__
  hEventShutdown=CreateEvent(0, FALSE, FALSE, shutdown_event_name);
  pthread_t hThread;
  if (mysql_thread_create(key_thread_handle_shutdown,
                          &hThread, &connection_attrib, handle_shutdown, 0))
    sql_print_warning("Can't create thread to handle shutdown requests");

  // On "Stop Service" we have to do regular shutdown
  Service.SetShutdownEvent(hEventShutdown);
#endif /* __WIN__ */
}

#endif /* EMBEDDED_LIBRARY */


#if (defined(_WIN32) || defined(HAVE_SMEM)) && !defined(EMBEDDED_LIBRARY)
static void handle_connections_methods()
{
  pthread_t hThread;
  DBUG_ENTER("handle_connections_methods");
  if (hPipe == INVALID_HANDLE_VALUE &&
      (!have_tcpip || opt_disable_networking) &&
      !opt_enable_shared_memory)
  {
    sql_print_error("TCP/IP, --shared-memory, or --named-pipe should be configured on NT OS");
    unireg_abort(1);				// Will not return
  }

  mysql_mutex_lock(&LOCK_thread_count);
  mysql_cond_init(key_COND_handler_count, &COND_handler_count, NULL);
  handler_count=0;
  if (hPipe != INVALID_HANDLE_VALUE)
  {
    handler_count++;
    if (mysql_thread_create(key_thread_handle_con_namedpipes,
                            &hThread, &connection_attrib,
                            handle_connections_namedpipes, 0))
    {
      sql_print_warning("Can't create thread to handle named pipes");
      handler_count--;
    }
  }
  if (have_tcpip && !opt_disable_networking)
  {
    handler_count++;
    if (mysql_thread_create(key_thread_handle_con_sockets,
                            &hThread, &connection_attrib,
                            handle_connections_sockets_thread, 0))
    {
      sql_print_warning("Can't create thread to handle TCP/IP");
      handler_count--;
    }
  }
#ifdef HAVE_SMEM
  if (opt_enable_shared_memory)
  {
    handler_count++;
    if (mysql_thread_create(key_thread_handle_con_sharedmem,
                            &hThread, &connection_attrib,
                            handle_connections_shared_memory, 0))
    {
      sql_print_warning("Can't create thread to handle shared memory");
      handler_count--;
    }
  }
#endif

  while (handler_count > 0)
    mysql_cond_wait(&COND_handler_count, &LOCK_thread_count);
  mysql_mutex_unlock(&LOCK_thread_count);
  DBUG_VOID_RETURN;
}

void decrement_handler_count()
{
  mysql_mutex_lock(&LOCK_thread_count);
  handler_count--;
  mysql_cond_signal(&COND_handler_count);
  mysql_mutex_unlock(&LOCK_thread_count);
  my_thread_end();
}
#else
#define decrement_handler_count()
#endif /* defined(_WIN32) || defined(HAVE_SMEM) */


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
int mysqld_main(int argc, char **argv)
#endif
{
  /*
    Perform basic thread library and malloc initialization,
    to be able to read defaults files and parse options.
  */
  my_progname= argv[0];
  sf_leaking_memory= 1; // no safemalloc memory leak reports if we exit early
#ifdef HAVE_NPTL
  ld_assume_kernel_is_set= (getenv("LD_ASSUME_KERNEL") != 0);
#endif
#ifndef _WIN32
  // For windows, my_init() is called from the win specific mysqld_main
  if (my_init())                 // init my_sys library & pthreads
  {
    fprintf(stderr, "my_init() failed.");
    return 1;
  }
#endif

  orig_argc= argc;
  orig_argv= argv;
  my_getopt_use_args_separator= TRUE;
  if (load_defaults(MYSQL_CONFIG_NAME, load_default_groups, &argc, &argv))
    return 1;
  my_getopt_use_args_separator= FALSE;
  defaults_argc= argc;
  defaults_argv= argv;
  remaining_argc= argc;
  remaining_argv= argv;

  /* Must be initialized early for comparison of options name */
  system_charset_info= &my_charset_utf8_general_ci;

  sys_var_init();

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  /*
    The performance schema needs to be initialized as early as possible,
    before to-be-instrumented objects of the server are initialized.
  */
  int ho_error;
  DYNAMIC_ARRAY all_early_options;

  my_getopt_register_get_addr(NULL);
  /* Skip unknown options so that they may be processed later */
  my_getopt_skip_unknown= TRUE;

  /* prepare all_early_options array */
  my_init_dynamic_array(&all_early_options, sizeof(my_option), 100, 25);
  sys_var_add_options(&all_early_options, sys_var::PARSE_EARLY);
  add_terminator(&all_early_options);

  /*
    Logs generated while parsing the command line
    options are buffered and printed later.
  */
  buffered_logs.init();
  my_getopt_error_reporter= buffered_option_error_reporter;

  ho_error= handle_options(&remaining_argc, &remaining_argv,
                           (my_option*)(all_early_options.buffer), NULL);
  delete_dynamic(&all_early_options);
  if (ho_error == 0)
  {
    /* Add back the program name handle_options removes */
    remaining_argc++;
    remaining_argv--;
    if (pfs_param.m_enabled)
    {
      PSI_hook= initialize_performance_schema(&pfs_param);
      if (PSI_hook == NULL)
      {
        pfs_param.m_enabled= false;
        buffered_logs.buffer(WARNING_LEVEL,
                             "Performance schema disabled (reason: init failed).");
      }
    }
  }
#else
  /*
    Other provider of the instrumentation interface should
    initialize PSI_hook here:
    - HAVE_PSI_INTERFACE is for the instrumentation interface
    - WITH_PERFSCHEMA_STORAGE_ENGINE is for one implementation
      of the interface,
    but there could be alternate implementations, which is why
    these two defines are kept separate.
  */
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

#ifdef HAVE_PSI_INTERFACE
  /*
    Obtain the current performance schema instrumentation interface,
    if available.
  */
  if (PSI_hook)
    PSI_server= (PSI*) PSI_hook->get_interface(PSI_CURRENT_VERSION);

  if (PSI_server)
  {
    /*
      Now that we have parsed the command line arguments, and have initialized
      the performance schema itself, the next step is to register all the
      server instruments.
    */
    init_server_psi_keys();
    /* Instrument the main thread */
    PSI_thread *psi= PSI_server->new_thread(key_thread_main, NULL, 0);
    if (psi)
      PSI_server->set_thread(psi);

    /*
      Now that some instrumentation is in place,
      recreate objects which were initialised early,
      so that they are instrumented as well.
    */
    my_thread_global_reinit();
  }
#endif /* HAVE_PSI_INTERFACE */

  init_error_log_mutex();

  /* Initialize audit interface globals. Audit plugins are inited later. */
  mysql_audit_initialize();

  /*
    Perform basic logger initialization logger. Should be called after
    MY_INIT, as it initializes mutexes. Log tables are inited later.
  */
  logger.init_base();

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  if (ho_error)
  {
    /*
      Parsing command line option failed,
      Since we don't have a workable remaining_argc/remaining_argv
      to continue the server initialization, this is as far as this
      code can go.
      This is the best effort to log meaningful messages:
      - messages will be printed to stderr, which is not redirected yet,
      - messages will be printed in the NT event log, for windows.
    */
    buffered_logs.print();
    buffered_logs.cleanup();
    /*
      Not enough initializations for unireg_abort()
      Using exit() for windows.
    */
    exit (ho_error);
  }
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

#ifdef _CUSTOMSTARTUPCONFIG_
  if (_cust_check_startup())
  {
    / * _cust_check_startup will report startup failure error * /
    exit(1);
  }
#endif

  if (init_common_variables())
    unireg_abort(1);				// Will do exit

  init_signals();

  my_thread_stack_size= my_setstacksize(&connection_attrib,
                                        my_thread_stack_size);

  (void) thr_setconcurrency(concurrency);	// 10 by default

  select_thread=pthread_self();
  select_thread_in_use=1;

#ifdef HAVE_LIBWRAP
  libwrapName= my_progname+dirname_length(my_progname);
  openlog(libwrapName, LOG_PID, LOG_AUTH);
#endif

#ifndef DBUG_OFF
  test_lc_time_sz();
  srand((uint) time(NULL)); 
#endif

  /*
    We have enough space for fiddling with the argv, continue
  */
  check_data_home(mysql_real_data_home);
  if (my_setwd(mysql_real_data_home, opt_abort ? 0 : MYF(MY_WME)) && !opt_abort)
    unireg_abort(1);				/* purecov: inspected */

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
    server_id= 1;
#ifdef EXTRA_DEBUG
    sql_print_warning("You have enabled the binary log, but you haven't set "
                      "server-id to a non-zero value: we force server id to 1; "
                      "updates will be logged to the binary log, but "
                      "connections from slaves will not be accepted.");
#endif
  }

  /* 
   The subsequent calls may take a long time : e.g. innodb log read.
   Thus set the long running service control manager timeout
  */
#if defined(_WIN32) && !defined(EMBEDDED_LIBRARY)
  Service.SetSlowStarting(slow_start_timeout);
#endif

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
  start_signal_handler();				// Creates pidfile

  if (mysql_rm_tmp_tables() || acl_init(opt_noacl) ||
      my_tz_init((THD *)0, default_tz_name, opt_bootstrap))
  {
    abort_loop=1;
    select_thread_in_use=0;

    (void) pthread_kill(signal_thread, MYSQL_KILL_SIGNAL);


    if (!opt_bootstrap)
      mysql_file_delete(key_file_pid, pidfile_name, MYF(MY_WME)); // Not needed anymore

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

  binlog_unsafe_map_init();
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

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  initialize_performance_schema_acl(opt_bootstrap);
  /*
    Do not check the structure of the performance schema tables
    during bootstrap:
    - the tables are not supposed to exist yet, bootstrap will create them
    - a check would print spurious error messages
  */
  if (! opt_bootstrap)
    check_performance_schema();
#endif

  initialize_information_schema_acl();

  execute_ddl_log_recovery();

  if (Events::init(opt_noacl || opt_bootstrap))
    unireg_abort(1);

  if (opt_bootstrap)
  {
    select_thread_in_use= 0;                    // Allow 'kill' to work
    bootstrap(mysql_stdin);
    if (!kill_in_progress)
      unireg_abort(bootstrap_error ? 1 : 0);
    else
    {
      sleep(2);                                 // Wait for kill
      exit(0);
    }
  }
  if (opt_init_file && *opt_init_file)
  {
    if (read_init_file(opt_init_file))
      unireg_abort(1);
  }

  /*
    We must have LOCK_open before LOCK_global_system_variables because
    LOCK_open is hold while sql_plugin.c::intern_sys_var_ptr() is called.
  */
  mysql_mutex_record_order(&LOCK_open, &LOCK_global_system_variables);

  create_shutdown_thread();
  start_handle_manager();

  sql_print_information(ER_DEFAULT(ER_STARTUP),my_progname,server_version,
                        ((unix_sock == INVALID_SOCKET) ? (char*) ""
                                                       : mysqld_unix_port),
                         mysqld_port,
                         MYSQL_COMPILATION_COMMENT);
#if defined(_WIN32) && !defined(EMBEDDED_LIBRARY)
  Service.SetRunning();
#endif


  /* Signal threads waiting for server to be started */
  mysql_mutex_lock(&LOCK_server_started);
  mysqld_server_started= 1;
  mysql_cond_signal(&COND_server_started);
  mysql_mutex_unlock(&LOCK_server_started);

#if defined(_WIN32) || defined(HAVE_SMEM)
  handle_connections_methods();
#else
  handle_connections_sockets();
#endif /* _WIN32 || HAVE_SMEM */

  /* (void) pthread_attr_destroy(&connection_attrib); */

  DBUG_PRINT("quit",("Exiting main thread"));

#ifndef __WIN__
#ifdef EXTRA_DEBUG2
  sql_print_error("Before Lock_thread_count");
#endif
  mysql_mutex_lock(&LOCK_thread_count);
  DBUG_PRINT("quit", ("Got thread_count mutex"));
  select_thread_in_use=0;			// For close_connections
  mysql_mutex_unlock(&LOCK_thread_count);
  mysql_cond_broadcast(&COND_thread_count);
#ifdef EXTRA_DEBUG2
  sql_print_error("After lock_thread_count");
#endif
#endif /* __WIN__ */

#ifdef HAVE_PSI_INTERFACE
  /*
    Disable the main thread instrumentation,
    to avoid recording events during the shutdown.
  */
  if (PSI_server)
    PSI_server->delete_current_thread();
#endif

  /* Wait until cleanup is done */
  mysql_mutex_lock(&LOCK_thread_count);
  while (!ready_to_exit)
    mysql_cond_wait(&COND_thread_count, &LOCK_thread_count);
  mysql_mutex_unlock(&LOCK_thread_count);

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
  mysqld_exit(0);
  return 0;
}

#endif /* !EMBEDDED_LIBRARY */


/****************************************************************************
  Main and thread entry function for Win32
  (all this is needed only to run mysqld as a service on WinNT)
****************************************************************************/

#if defined(__WIN__) && !defined(EMBEDDED_LIBRARY)
int mysql_service(void *p)
{
  if (my_thread_init())
    return 1;
  
  if (use_opt_args)
    win_main(opt_argc, opt_argv);
  else
    win_main(Service.my_argc, Service.my_argv);

  my_thread_end();
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

  @retval 0	option handled
  @retval 1	Could not handle option
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


int mysqld_main(int argc, char **argv)
{
  my_progname= argv[0];

  /*
    When several instances are running on the same machine, we
    need to have an  unique  named  hEventShudown  through the
    application PID e.g.: MySQLShutdown1890; MySQLShutdown2342
  */
  int10_to_str((int) GetCurrentProcessId(),strmov(shutdown_event_name,
                                                  "MySQLShutdown"), 10);

  /* Must be initialized early for comparison of service name */
  system_charset_info= &my_charset_utf8_general_ci;

  if (my_init())
  {
    fprintf(stderr, "my_init() failed.");
    return 1;
  }

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

static void bootstrap(MYSQL_FILE *file)
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
  if (mysql_thread_create(key_thread_bootstrap,
                          &thd->real_id, &connection_attrib, handle_bootstrap,
                          (void*) thd))
  {
    sql_print_warning("Can't create thread to handle bootstrap");
    bootstrap_error=-1;
    DBUG_VOID_RETURN;
  }
  /* Wait for thread to die */
  mysql_mutex_lock(&LOCK_thread_count);
  while (in_bootstrap)
  {
    mysql_cond_wait(&COND_thread_count, &LOCK_thread_count);
    DBUG_PRINT("quit",("One thread died (count=%u)",thread_count));
  }
  mysql_mutex_unlock(&LOCK_thread_count);
#else
  thd->mysql= 0;
  do_handle_bootstrap(thd);
#endif

  DBUG_VOID_RETURN;
}


static bool read_init_file(char *file_name)
{
  MYSQL_FILE *file;
  DBUG_ENTER("read_init_file");
  DBUG_PRINT("enter",("name: %s",file_name));
  if (!(file= mysql_file_fopen(key_file_init, file_name,
                               O_RDONLY, MYF(MY_WME))))
    DBUG_RETURN(TRUE);
  bootstrap(file);
  mysql_file_fclose(file, MYF(MY_WME));
  DBUG_RETURN(FALSE);
}


/**
  Increment number of created threads
*/
void inc_thread_created(void)
{
  thread_created++;
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
  mysql_mutex_assert_owner(&LOCK_thread_count);
  thread_cache_size=0;			// Safety
  threads.append(thd);
  mysql_mutex_unlock(&LOCK_thread_count);
  thd->start_utime= microsecond_interval_timer();
  do_handle_one_connection(thd);
}


/*
  Scheduler that uses one thread per connection
*/

void create_thread_to_handle_connection(THD *thd)
{
  if (cached_thread_count > wake_thread)
  {
    /* Get thread from cache */
    thread_cache.append(thd);
    wake_thread++;
    mysql_cond_signal(&COND_thread_cache);
  }
  else
  {
    char error_message_buff[MYSQL_ERRMSG_SIZE];
    /* Create new thread to handle connection */
    int error;
    thread_created++;
    threads.append(thd);
    DBUG_PRINT("info",(("creating thread %lu"), thd->thread_id));
    thd->prior_thr_create_utime= microsecond_interval_timer();
    if ((error= mysql_thread_create(key_thread_one_connection,
                                    &thd->real_id, &connection_attrib,
                                    handle_one_connection,
                                    (void*) thd)))
    {
      /* purecov: begin inspected */
      DBUG_PRINT("error",
                 ("Can't create thread to handle request (error %d)",
                  error));
      thread_count--;
      thd->killed= KILL_CONNECTION;             // Safety
      mysql_mutex_unlock(&LOCK_thread_count);

      mysql_mutex_lock(&LOCK_connection_count);
      (*thd->scheduler->connection_count)--;
      mysql_mutex_unlock(&LOCK_connection_count);

      statistic_increment(aborted_connects,&LOCK_status);
      /* Can't use my_error() since store_globals has not been called. */
      my_snprintf(error_message_buff, sizeof(error_message_buff),
                  ER_THD(thd, ER_CANT_CREATE_THREAD), error);
      net_send_error(thd, ER_CANT_CREATE_THREAD, error_message_buff, NULL);
      close_connection(thd, ER_OUT_OF_RESOURCES);
      mysql_mutex_lock(&LOCK_thread_count);
      delete thd;
      mysql_mutex_unlock(&LOCK_thread_count);
      return;
      /* purecov: end */
    }
  }
  mysql_mutex_unlock(&LOCK_thread_count);
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
  DBUG_ENTER("create_new_thread");

  /*
    Don't allow too many connections. We roughly check here that we allow
    only (max_connections + 1) connections.
  */

  mysql_mutex_lock(&LOCK_connection_count);

  if (*thd->scheduler->connection_count >=
      *thd->scheduler->max_connections + 1|| abort_loop)
  {
    mysql_mutex_unlock(&LOCK_connection_count);

    DBUG_PRINT("error",("Too many connections"));
    close_connection(thd, ER_CON_COUNT_ERROR);
    statistic_increment(denied_connections, &LOCK_status);
    delete thd;
    DBUG_VOID_RETURN;
  }

  ++*thd->scheduler->connection_count;

  if (connection_count + extra_connection_count > max_used_connections)
    max_used_connections= connection_count + extra_connection_count;

  mysql_mutex_unlock(&LOCK_connection_count);

  /* Start a new thread to handle connection. */

  mysql_mutex_lock(&LOCK_thread_count);

  /*
    The initialization of thread_id is done in create_embedded_thd() for
    the embedded library.
    TODO: refactor this to avoid code duplication there
  */
  thd->thread_id= thd->variables.pseudo_thread_id= thread_id++;

  thread_count++;

  MYSQL_CALLBACK(thd->scheduler, add_connection, (thd));

  DBUG_VOID_RETURN;
}
#endif /* EMBEDDED_LIBRARY */


#ifdef SIGNALS_DONT_BREAK_READ
inline void kill_broken_server()
{
  /* hack to get around signals ignored in syscalls for problem OS's */
  if (unix_sock == INVALID_SOCKET ||
      (!opt_disable_networking && base_ip_sock == INVALID_SOCKET))
  {
    select_thread_in_use = 0;
    /* The following call will never return */
    DBUG_PRINT("general", ("killing server because socket is closed"));
    kill_server((void*) MYSQL_KILL_SIGNAL);
  }
}
#define MAYBE_BROKEN_SYSCALL kill_broken_server();
#else
#define MAYBE_BROKEN_SYSCALL
#endif

	/* Handle new connections and spawn new process to handle them */

#ifndef EMBEDDED_LIBRARY

void handle_connections_sockets()
{
  my_socket UNINIT_VAR(sock), UNINIT_VAR(new_sock);
  uint error_count=0;
  THD *thd;
  struct sockaddr_storage cAddr;
  int ip_flags __attribute__((unused))=0;
  int socket_flags __attribute__((unused))= 0;
  int extra_ip_flags __attribute__((unused))=0;
  int flags=0,retval;
  st_vio *vio_tmp;
#ifdef HAVE_POLL
  int socket_count= 0;
  struct pollfd fds[3]; // for ip_sock, unix_sock and extra_ip_sock
#define setup_fds(X)                    \
    fds[socket_count].fd= X;            \
    fds[socket_count].events= POLLIN;   \
    socket_count++
#else
  fd_set readFDs,clientFDs;
  uint max_used_connection= (uint)
    max(max(base_ip_sock, unix_sock), extra_ip_sock) + 1;
#define setup_fds(X)    FD_SET(X,&clientFDs)
  FD_ZERO(&clientFDs);
#endif

  DBUG_ENTER("handle_connections_sockets");

  if (base_ip_sock != INVALID_SOCKET)
  {
    setup_fds(base_ip_sock);
    ip_flags = fcntl(base_ip_sock, F_GETFL, 0);
  }
  if (extra_ip_sock != INVALID_SOCKET)
  {
    setup_fds(extra_ip_sock);
    extra_ip_flags = fcntl(extra_ip_sock, F_GETFL, 0);
  }
#ifdef HAVE_SYS_UN_H
  setup_fds(unix_sock);
  socket_flags=fcntl(unix_sock, F_GETFL, 0);
#endif

  DBUG_PRINT("general",("Waiting for connections."));
  MAYBE_BROKEN_SYSCALL;
  while (!abort_loop)
  {
#ifdef HAVE_POLL
    retval= poll(fds, socket_count, -1);
#else
    readFDs=clientFDs;

    retval= select((int) max_used_connection,&readFDs,0,0,0);
#endif

    if (retval < 0)
    {
      if (socket_errno != SOCKET_EINTR)
      {
	if (!select_errors++ && !abort_loop)	/* purecov: inspected */
	  sql_print_error("mysqld: Got error %d from select",socket_errno); /* purecov: inspected */
      }
      MAYBE_BROKEN_SYSCALL
      continue;
    }

    if (abort_loop)
    {
      MAYBE_BROKEN_SYSCALL;
      break;
    }

    /* Is this a new connection request ? */
#ifdef HAVE_POLL
    for (int i= 0; i < socket_count; ++i) 
    {
      if (fds[i].revents & POLLIN)
      {
        sock= fds[i].fd;
        flags= fcntl(sock, F_GETFL, 0);
        break;
      }
    }
#else  // HAVE_POLL
    if (FD_ISSET(base_ip_sock,&readFDs))
    {
      sock=  base_ip_sock;
      flags= ip_flags;
    }
    else
    if (FD_ISSET(extra_ip_sock,&readFDs))
    {
      sock=  extra_ip_sock;
      flags= extra_ip_flags;
    }
    else
    {
      sock = unix_sock;
      flags= socket_flags;
    }
#endif // HAVE_POLL

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
      size_socket length= sizeof(struct sockaddr_storage);
      new_sock= accept(sock, (struct sockaddr *)(&cAddr),
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
      if (sock == base_ip_sock || sock == extra_ip_sock)
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
      struct sockaddr_storage dummy;
      dummyLen = sizeof(dummy);
      if (  getsockname(new_sock,(struct sockaddr *)&dummy, 
                  (SOCKET_SIZE_TYPE *)&dummyLen) < 0  )
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
      (void) closesocket(new_sock);
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

    if (sock == extra_ip_sock)
    {
      thd->extra_port= 1;
      thd->scheduler= extra_thread_scheduler;
    }
    create_new_thread(thd);
  }
  DBUG_VOID_RETURN;
}


#ifdef _WIN32
pthread_handler_t handle_connections_sockets_thread(void *arg)
{
  my_thread_init();
  handle_connections_sockets();
  decrement_handler_count();
  return 0;
}

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
      close_connection(thd, ER_OUT_OF_RESOURCES);
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
#endif /* _WIN32 */


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
      close_connection(thd, ER_OUT_OF_RESOURCES);
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
    my_free(tmp);

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

/**
  System variables are automatically command-line options (few
  exceptions are documented in sys_var.h), so don't need
  to be listed here.
*/

struct my_option my_long_options[]=
{
  {"help", '?', "Display this help and exit.", 
   &opt_help, &opt_help, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"allow-suspicious-udfs", 0,
   "Allows use of UDFs consisting of only one symbol xxx() "
   "without corresponding xxx_init() or xxx_deinit(). That also means "
   "that one can load any function from any library, for example exit() "
   "from libc.so",
   &opt_allow_suspicious_udfs, &opt_allow_suspicious_udfs,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"ansi", 'a', "Use ANSI SQL syntax instead of MySQL syntax. This mode "
   "will also set transaction isolation level 'serializable'.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  /*
    Because Sys_var_bit does not support command-line options, we need to
    explicitely add one for --autocommit
  */
  {"autocommit", 0, "Set default value for autocommit (0 or 1)",
   &opt_autocommit, &opt_autocommit, 0,
   GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, NULL},
  {"bind-address", OPT_BIND_ADDRESS, "IP address to bind to.",
   &my_bind_addr_str, &my_bind_addr_str, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog-do-db", OPT_BINLOG_DO_DB,
   "Tells the master it should log updates for the specified database, "
   "and exclude all others not explicitly mentioned.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog-ignore-db", OPT_BINLOG_IGNORE_DB,
   "Tells the master that updates to the given database should not be logged to the binary log.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog-row-event-max-size", 0,
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
  {"character-set-client-handshake", 0,
   "Don't ignore client side character set value sent during handshake.",
   &opt_character_set_client_handshake,
   &opt_character_set_client_handshake,
    0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"character-set-filesystem", 0,
   "Set the filesystem character set.",
   &character_set_filesystem_name,
   &character_set_filesystem_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"character-set-server", 'C', "Set the default character set.",
   &default_character_set_name, &default_character_set_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"chroot", 'r', "Chroot mysqld daemon during startup.",
   &mysqld_chroot, &mysqld_chroot, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"collation-server", 0, "Set the default collation.",
   &default_collation_name, &default_collation_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"console", OPT_CONSOLE, "Write error output on screen; don't remove the console window on windows.",
   &opt_console, &opt_console, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"core-file", OPT_WANT_CORE, "Write core on errors.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  /* default-storage-engine should have "MyISAM" as def_value. Instead
     of initializing it here it is done in init_common_variables() due
     to a compiler bug in Sun Studio compiler. */
#ifdef DBUG_OFF
  {"debug", '#', "Built in DBUG debugger. Disabled in this build.",
   &current_dbug_option, &current_dbug_option, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
#endif
#ifdef HAVE_REPLICATION
  {"debug-abort-slave-event-count", 0,
   "Option used by mysql-test for debugging and testing of replication.",
   &abort_slave_event_count,  &abort_slave_event_count,
   0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif /* HAVE_REPLICATION */
#ifndef DBUG_OFF
  {"debug-assert-on-error", 0,
   "Do an assert in various functions if we get a fatal error",
   &my_assert_on_error, &my_assert_on_error,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-assert-if-crashed-table", 0,
   "Do an assert in handler::print_error() if we get a crashed table",
   &debug_assert_if_crashed_table, &debug_assert_if_crashed_table,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
#ifdef HAVE_REPLICATION
  {"debug-disconnect-slave-event-count", 0,
   "Option used by mysql-test for debugging and testing of replication.",
   &disconnect_slave_event_count, &disconnect_slave_event_count,
   0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif /* HAVE_REPLICATION */
  {"debug-exit-info", 'T', "Used for debugging. Use at your own risk.",
   0, 0, 0, GET_LONG, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-gdb", 0,
   "Set up signals usable for debugging.",
   &opt_debugging, &opt_debugging,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_REPLICATION
  {"debug-max-binlog-dump-events", 0,
   "Option used by mysql-test for debugging and testing of replication.",
   &max_binlog_dump_events, &max_binlog_dump_events, 0,
   GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif /* HAVE_REPLICATION */
#ifdef SAFE_MUTEX
  {"debug-mutex-deadlock-detector", 0,
   "Enable checking of wrong mutex usage.",
   &safe_mutex_deadlock_detector,
   &safe_mutex_deadlock_detector,
   0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
#endif
  {"debug-no-sync", 0,
   "Disables system sync calls. Only for running tests or debugging!",
   &my_disable_sync, &my_disable_sync, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_REPLICATION
  {"debug-sporadic-binlog-dump-fail", 0,
   "Option used by mysql-test for debugging and testing of replication.",
   &opt_sporadic_binlog_dump_fail,
   &opt_sporadic_binlog_dump_fail, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
#endif /* HAVE_REPLICATION */
  {"default-storage-engine", 0, "The default storage engine for new tables",
   &default_storage_engine, 0, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0 },
  {"default-time-zone", 0, "Set the default time zone.",
   &default_tz_name, &default_tz_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
#if defined(ENABLED_DEBUG_SYNC)
  {"debug-sync-timeout", OPT_DEBUG_SYNC_TIMEOUT,
   "Enable the debug sync facility "
   "and optionally specify a default wait timeout in seconds. "
   "A zero value keeps the facility disabled.",
   &opt_debug_sync_timeout, 0,
   0, GET_UINT, OPT_ARG, 0, 0, UINT_MAX, 0, 0, 0},
#endif /* defined(ENABLED_DEBUG_SYNC) */
#ifdef HAVE_OPENSSL
  {"des-key-file", 0,
   "Load keys for des_encrypt() and des_encrypt from given file.",
   &des_key_file, &des_key_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
#endif /* HAVE_OPENSSL */
#ifdef HAVE_STACKTRACE
  {"stack-trace", 0 , "Print a symbolic stack trace on failure",
   &opt_stack_trace, &opt_stack_trace, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
#endif /* HAVE_STACKTRACE */
  {"external-locking", 0, "Use system (external) locking (disabled by "
   "default).  With this option enabled you can run myisamchk to test "
   "(not repair) tables while the MySQL server is running. Disable with "
   "--skip-external-locking.", &opt_external_locking, &opt_external_locking,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  /* We must always support the next option to make scripts like mysqltest
     easier to do */
  {"gdb", 0,
   "Set up signals usable for debugging. Deprecated, use --debug-gdb instead.",
   &opt_debugging, &opt_debugging,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_LARGE_PAGE_OPTION
  {"super-large-pages", 0, "Enable support for super large pages.",
   &opt_super_large_pages, &opt_super_large_pages, 0,
   GET_BOOL, OPT_ARG, 0, 0, 1, 0, 1, 0},
#endif
  {"language", 'L',
   "Client error messages in given language. May be given as a full path. "
   "Deprecated. Use --lc-messages-dir instead.",
   0, 0, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"lc-messages", 0,
   "Set the language used for the error messages.",
   &lc_messages, &lc_messages, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0 },
  {"lc-time-names", 0,
   "Set the language used for the month names and the days of the week.",
   &lc_time_names_name, &lc_time_names_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"log", 'l', "Log connections and queries to file (deprecated option, use "
   "--general-log/--general-log-file instead).", &opt_logname, &opt_logname,
   0, GET_STR_ALLOC, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-basename", OPT_LOG_BASENAME,
   "Basename for all log files and the .pid file. This sets all log file "
   "names at once (in 'datadir') and is normally the only option you need "
   "for specifying log files. Sets names for --log-bin, --log-bin-index, "
   "--relay-log, --relay-log-index, --general-log-file, "
   "--log-slow-query-log-file, --log-error-file, and --pid-file",
   &opt_log_basename, &opt_log_basename, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"log-bin", OPT_BIN_LOG,
   "Log update queries in binary format. Optional argument should be name for "
   "binary log. If not given "
   "'datadir'/'log-basename'-bin or 'datadir'/mysql-bin will be used (the later if "
   "--log-basename is not specified). We strongly recommend to use either "
   "--log-basename or specify a filename to ensure that replication doesn't "
   "stop if the real hostname of the computer changes.",
   &opt_bin_logname, &opt_bin_logname, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-bin-index", 0,
   "File that holds the names for last binary log files.",
   &opt_binlog_index_name, &opt_binlog_index_name, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"log-isam", OPT_ISAM_LOG, "Log all MyISAM changes to file.",
   &myisam_log_filename, &myisam_log_filename, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-short-format", 0,
   "Don't log extra information to update and slow-query logs.",
   &opt_short_log_format, &opt_short_log_format,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-slow-admin-statements", 0,
   "Log slow OPTIMIZE, ANALYZE, ALTER and other administrative statements to "
   "the slow log if it is open.", &opt_log_slow_admin_statements,
   &opt_log_slow_admin_statements, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
 {"log-slow-slave-statements", 0,
  "Log slow statements executed by slave thread to the slow log if it is open.",
  &opt_log_slow_slave_statements, &opt_log_slow_slave_statements,
  0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-slow-queries", OPT_SLOW_QUERY_LOG,
   "Enable logging of slow queries (longer than --long-query-time) to log file "
   "or table. Optional argument is a file name for the slow log. If not given, "
   "'log-basename'-slow.log will be used. Use --log-output=TABLE if you want "
   "to have the log in the table mysql.slow_log. "
   "Deprecated option, use --slow-query-log/--slow-query-log-file instead.",
   &opt_slow_logname, &opt_slow_logname, 0, GET_STR_ALLOC, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"log-tc", 0,
   "Path to transaction coordinator log (used for transactions that affect "
   "more than one storage engine, when binary log is disabled).",
   &opt_tc_log_file, &opt_tc_log_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_MMAP
  {"log-tc-size", 0, "Size of transaction coordinator log.",
   &opt_tc_log_size, &opt_tc_log_size, 0, GET_ULONG,
   REQUIRED_ARG, TC_LOG_MIN_SIZE, TC_LOG_MIN_SIZE, (ulonglong) ULONG_MAX, 0,
   TC_LOG_PAGE_SIZE, 0},
#endif
  {"master-info-file", 0,
   "The location and name of the file that remembers the master and where "
   "the I/O replication thread is in the master's binlogs. Defaults to "
   "master.info",
   &master_info_file, &master_info_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"master-retry-count", 0,
   "The number of tries the slave will make to connect to the master before giving up.",
   &master_retry_count, &master_retry_count, 0, GET_ULONG,
   REQUIRED_ARG, 3600*24, 0, 0, 0, 0, 0},
#ifdef HAVE_REPLICATION
  {"init-rpl-role", 0, "Set the replication role.",
   &rpl_status, &rpl_status, &rpl_role_typelib,
   GET_ENUM, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif /* HAVE_REPLICATION */
  {"memlock", 0, "Lock mysqld in memory.", &locked_in_memory,
   &locked_in_memory, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"one-thread", OPT_ONE_THREAD,
   "(Deprecated): Only use one thread (for debugging under Linux). Use "
   "thread-handling=no-threads instead.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"old-style-user-limits", 0,
   "Enable old-style user limits (before 5.0.3, user resources were counted "
   "per each user+host vs. per account).",
   &opt_old_style_user_limits, &opt_old_style_user_limits,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"port-open-timeout", 0,
   "Maximum time in seconds to wait for the port to become free. "
   "(Default: No wait).", &mysqld_port_timeout, &mysqld_port_timeout, 0,
   GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
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
  {"replicate-same-server-id", 0,
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
  {"safe-mode", OPT_SAFE, "Skip some optimize stages (for testing). Deprecated.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"safe-user-create", 0,
   "Don't allow new user creation by the user who has no write privileges to the mysql.user table.",
   &opt_safe_user_create, &opt_safe_user_create, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"show-slave-auth-info", 0,
   "Show user and password in SHOW SLAVE HOSTS on this master.",
   &opt_show_slave_auth_info, &opt_show_slave_auth_info, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-bdb", OPT_DEPRECATED_OPTION,
   "Deprecated option; Exist only for compatiblity with old my.cnf files",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DISABLE_GRANT_OPTIONS
  {"skip-grant-tables", 0,
   "Start without grant tables. This gives all users FULL ACCESS to all tables.",
   &opt_noacl, &opt_noacl, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
#endif
  {"skip-host-cache", OPT_SKIP_HOST_CACHE, "Don't cache host names.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-slave-start", 0,
   "If set, slave is not autostarted.", &opt_skip_slave_start,
   &opt_skip_slave_start, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-thread-priority", OPT_SKIP_PRIOR,
   "Don't give threads different priorities. This option is deprecated "
   "because it has no effect; the implied behavior is already the default.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#if defined(_WIN32) && !defined(EMBEDDED_LIBRARY)
  {"slow-start-timeout", 0,
   "Maximum number of milliseconds that the service control manager should wait "
   "before trying to kill the windows service during startup"
   "(Default: 15000).", &slow_start_timeout, &slow_start_timeout, 0,
   GET_ULONG, REQUIRED_ARG, 15000, 0, 0, 0, 0, 0},
#endif
#ifdef HAVE_OPENSSL
  {"ssl", 0,
   "Enable SSL for connection (automatically enabled with other flags).",
   &opt_use_ssl, &opt_use_ssl, 0, GET_BOOL, OPT_ARG, 0, 0, 0,
   0, 0, 0},
#endif
#ifdef __WIN__
  {"standalone", 0,
  "Dummy option to start as a standalone program (NT).", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"symbolic-links", 's', "Enable symbolic link support.",
   &my_use_symdir, &my_use_symdir, 0, GET_BOOL, NO_ARG,
   /*
     The system call realpath() produces warnings under valgrind and
     purify. These are not suppressed: instead we disable symlinks
     option if compiled with valgrind support.
     Also disable by default on Windows, due to high overhead for checking .sym 
     files.
   */
   IF_VALGRIND(0,IF_WIN(0,1)), 0, 0, 0, 0, 0},
  {"sysdate-is-now", 0,
   "Non-default option to alias SYSDATE() to NOW() to make it safe-replicable. "
   "Since 5.0, SYSDATE() returns a `dynamic' value different for different "
   "invocations, even within the same statement.",
   &global_system_variables.sysdate_is_now,
   0, 0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 1, 0},
  {"tc-heuristic-recover", 0,
   "Decision to use in heuristic recover process. Possible values are COMMIT "
   "or ROLLBACK.", &tc_heuristic_recover, &tc_heuristic_recover,
   &tc_heuristic_recover_typelib, GET_ENUM, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"temp-pool", 0,
#if (ENABLE_TEMP_POOL)
   "Using this option will cause most temporary files created to use a small "
   "set of names, rather than a unique name for each new file.",
#else
   "This option is ignored on this OS.",
#endif
   &use_temp_pool, &use_temp_pool, 0, GET_BOOL, NO_ARG, 1,
   0, 0, 0, 0, 0},
  {"transaction-isolation", 0,
   "Default transaction isolation level.",
   &global_system_variables.tx_isolation,
   &global_system_variables.tx_isolation, &tx_isolation_typelib,
   GET_ENUM, REQUIRED_ARG, ISO_REPEATABLE_READ, 0, 0, 0, 0, 0},
  {"user", 'u', "Run mysqld daemon as user.", 0, 0, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Used with --help option for detailed help.",
   &opt_verbose, &opt_verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"plugin-load", 0,
   "Semicolon-separated list of plugins to load, where each plugin is "
   "specified as ether a plugin_name=library_file pair or only a library_file. "
   "If the latter case, all plugins from a given library_file will be loaded.",
   &opt_plugin_load, &opt_plugin_load, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"table_cache", 0, "Deprecated; use --table-open-cache instead.",
   &table_cache_size, &table_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, TABLE_OPEN_CACHE_DEFAULT, 1, 512*1024L, 0, 1, 0},
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

#ifdef ENABLED_PROFILING
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
  mysql_mutex_lock(&LOCK_active_mi);
  var->value= buff;
  *((my_bool *)buff)= (my_bool) (active_mi && 
                                 active_mi->slave_running == MYSQL_SLAVE_RUN_CONNECT &&
                                 active_mi->rli.slave_running);
  mysql_mutex_unlock(&LOCK_active_mi);
  return 0;
}

static int show_slave_retried_trans(THD *thd, SHOW_VAR *var, char *buff)
{
  /*
    TODO: with multimaster, have one such counter per line in
    SHOW SLAVE STATUS, and have the sum over all lines here.
  */
  mysql_mutex_lock(&LOCK_active_mi);
  if (active_mi)
  {
    var->type= SHOW_LONG;
    var->value= buff;
    mysql_mutex_lock(&active_mi->rli.data_lock);
    *((long *)buff)= (long)active_mi->rli.retried_trans;
    mysql_mutex_unlock(&active_mi->rli.data_lock);
  }
  else
    var->type= SHOW_UNDEF;
  mysql_mutex_unlock(&LOCK_active_mi);
  return 0;
}

static int show_slave_received_heartbeats(THD *thd, SHOW_VAR *var, char *buff)
{
  mysql_mutex_lock(&LOCK_active_mi);
  if (active_mi)
  {
    var->type= SHOW_LONGLONG;
    var->value= buff;
    mysql_mutex_lock(&active_mi->rli.data_lock);
    *((longlong *)buff)= active_mi->received_heartbeats;
    mysql_mutex_unlock(&active_mi->rli.data_lock);
  }
  else
    var->type= SHOW_UNDEF;
  mysql_mutex_unlock(&LOCK_active_mi);
  return 0;
}

static int show_heartbeat_period(THD *thd, SHOW_VAR *var, char *buff)
{
  mysql_mutex_lock(&LOCK_active_mi);
  if (active_mi)
  {
    var->type= SHOW_CHAR;
    var->value= buff;
    sprintf(buff, "%.3f", active_mi->heartbeat_period);
  }
  else
    var->type= SHOW_UNDEF;
  mysql_mutex_unlock(&LOCK_active_mi);
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
  mysql_mutex_lock(&LOCK_prepared_stmt_count);
  *((long *)buff)= (long)prepared_stmt_count;
  mysql_mutex_unlock(&LOCK_prepared_stmt_count);
  return 0;
}

static int show_table_definitions(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long)cached_table_definitions();
  return 0;
}

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
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

#endif /* HAVE_OPENSSL && !EMBEDDED_LIBRARY */

static int show_default_keycache(THD *thd, SHOW_VAR *var, char *buff)
{
  struct st_data {
    KEY_CACHE_STATISTICS stats;
    SHOW_VAR var[8];
  } *data;
  SHOW_VAR *v;

  data=(st_data *)buff;
  v= data->var;

  var->type= SHOW_ARRAY;
  var->value= (char*)v;

  get_key_cache_statistics(dflt_key_cache, 0, &data->stats);

#define set_one_keycache_var(X,Y)       \
  v->name= X;                           \
  v->type= SHOW_LONGLONG;               \
  v->value= (char*)&data->stats.Y;      \
  v++;

  set_one_keycache_var("blocks_not_flushed", blocks_changed);
  set_one_keycache_var("blocks_unused",      blocks_unused);
  set_one_keycache_var("blocks_used",        blocks_used);
  set_one_keycache_var("blocks_warm",        blocks_warm);
  set_one_keycache_var("read_requests",      read_requests);
  set_one_keycache_var("reads",              reads);
  set_one_keycache_var("write_requests",     write_requests);
  set_one_keycache_var("writes",             writes);

  v->name= 0;

  DBUG_ASSERT((char*)(v+1) <= buff + SHOW_VAR_FUNC_BUFF_SIZE);

#undef set_one_keycache_var

  return 0;
}

#ifdef HAVE_POOL_OF_THREADS
int show_threadpool_idle_threads(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_INT;
  var->value= buff;
  *(int *)buff= tp_get_idle_thread_count(); 
  return 0;
}
#endif

/*
  Variables shown by SHOW STATUS in alphabetical order
*/

SHOW_VAR status_vars[]= {
  {"Aborted_clients",          (char*) &aborted_threads,        SHOW_LONG},
  {"Aborted_connects",         (char*) &aborted_connects,       SHOW_LONG},
  {"Access_denied_errors",     (char*) offsetof(STATUS_VAR, access_denied_errors), SHOW_LONG_STATUS},
  {"Binlog_bytes_written",     (char*) offsetof(STATUS_VAR, binlog_bytes_written), SHOW_LONGLONG_STATUS},
  {"Binlog_cache_disk_use",    (char*) &binlog_cache_disk_use,  SHOW_LONG},
  {"Binlog_cache_use",         (char*) &binlog_cache_use,       SHOW_LONG},
  {"Binlog_stmt_cache_disk_use",(char*) &binlog_stmt_cache_disk_use,  SHOW_LONG},
  {"Binlog_stmt_cache_use",    (char*) &binlog_stmt_cache_use,       SHOW_LONG},
  {"Busy_time",                (char*) offsetof(STATUS_VAR, busy_time), SHOW_DOUBLE_STATUS},
  {"Bytes_received",           (char*) offsetof(STATUS_VAR, bytes_received), SHOW_LONGLONG_STATUS},
  {"Bytes_sent",               (char*) offsetof(STATUS_VAR, bytes_sent), SHOW_LONGLONG_STATUS},
  {"Com",                      (char*) com_status_vars, SHOW_ARRAY},
  {"Compression",              (char*) &show_net_compression, SHOW_FUNC},
  {"Connections",              (char*) &thread_id,              SHOW_LONG_NOFLUSH},
  {"Cpu_time",                 (char*) offsetof(STATUS_VAR, cpu_time), SHOW_DOUBLE_STATUS},
  {"Created_tmp_disk_tables",  (char*) offsetof(STATUS_VAR, created_tmp_disk_tables), SHOW_LONG_STATUS},
  {"Created_tmp_files",	       (char*) &my_tmp_file_created,	SHOW_LONG},
  {"Created_tmp_tables",       (char*) offsetof(STATUS_VAR, created_tmp_tables), SHOW_LONG_STATUS},
  {"Delayed_errors",           (char*) &delayed_insert_errors,  SHOW_LONG},
  {"Delayed_insert_threads",   (char*) &delayed_insert_threads, SHOW_LONG_NOFLUSH},
  {"Delayed_writes",           (char*) &delayed_insert_writes,  SHOW_LONG},
  {"Empty_queries",            (char*) offsetof(STATUS_VAR, empty_queries), SHOW_LONG_STATUS},
  {"Executed_events",          (char*) &executed_events, SHOW_LONG_NOFLUSH },
  {"Executed_triggers",        (char*) offsetof(STATUS_VAR, executed_triggers), SHOW_LONG_STATUS},
  {"Feature_dynamic_columns",  (char*) offsetof(STATUS_VAR, feature_dynamic_columns), SHOW_LONG_STATUS},
  {"Feature_fulltext",         (char*) offsetof(STATUS_VAR, feature_fulltext), SHOW_LONG_STATUS},
  {"Feature_gis",              (char*) offsetof(STATUS_VAR, feature_gis), SHOW_LONG_STATUS},
  {"Feature_locale",           (char*) offsetof(STATUS_VAR, feature_locale), SHOW_LONG_STATUS},
  {"Feature_subquery",         (char*) offsetof(STATUS_VAR, feature_subquery), SHOW_LONG_STATUS},
  {"Feature_timezone",         (char*) offsetof(STATUS_VAR, feature_timezone), SHOW_LONG_STATUS},
  {"Feature_trigger",         (char*) offsetof(STATUS_VAR, feature_trigger), SHOW_LONG_STATUS},
  {"Feature_xml",             (char*) offsetof(STATUS_VAR, feature_xml), SHOW_LONG_STATUS},
  {"Flush_commands",           (char*) &refresh_version,        SHOW_LONG_NOFLUSH},
  {"Handler_commit",           (char*) offsetof(STATUS_VAR, ha_commit_count), SHOW_LONG_STATUS},
  {"Handler_delete",           (char*) offsetof(STATUS_VAR, ha_delete_count), SHOW_LONG_STATUS},
  {"Handler_discover",         (char*) offsetof(STATUS_VAR, ha_discover_count), SHOW_LONG_STATUS},
  {"Handler_icp_attempts",     (char*) offsetof(STATUS_VAR, ha_icp_attempts), SHOW_LONG_STATUS},
  {"Handler_icp_match",        (char*) offsetof(STATUS_VAR, ha_icp_match), SHOW_LONG_STATUS},
  {"Handler_mrr_init",         (char*) offsetof(STATUS_VAR, ha_mrr_init_count),  SHOW_LONG_STATUS},
  {"Handler_mrr_key_refills",   (char*) offsetof(STATUS_VAR, ha_mrr_key_refills_count), SHOW_LONG_STATUS},
  {"Handler_mrr_rowid_refills", (char*) offsetof(STATUS_VAR, ha_mrr_rowid_refills_count), SHOW_LONG_STATUS},
  {"Handler_prepare",          (char*) offsetof(STATUS_VAR, ha_prepare_count),  SHOW_LONG_STATUS},
  {"Handler_read_first",       (char*) offsetof(STATUS_VAR, ha_read_first_count), SHOW_LONG_STATUS},
  {"Handler_read_key",         (char*) offsetof(STATUS_VAR, ha_read_key_count), SHOW_LONG_STATUS},
  {"Handler_read_last",        (char*) offsetof(STATUS_VAR, ha_read_last_count), SHOW_LONG_STATUS},
  {"Handler_read_next",        (char*) offsetof(STATUS_VAR, ha_read_next_count), SHOW_LONG_STATUS},
  {"Handler_read_prev",        (char*) offsetof(STATUS_VAR, ha_read_prev_count), SHOW_LONG_STATUS},
  {"Handler_read_rnd",         (char*) offsetof(STATUS_VAR, ha_read_rnd_count), SHOW_LONG_STATUS},
  {"Handler_read_rnd_deleted", (char*) offsetof(STATUS_VAR, ha_read_rnd_deleted_count), SHOW_LONG_STATUS},
  {"Handler_read_rnd_next",    (char*) offsetof(STATUS_VAR, ha_read_rnd_next_count), SHOW_LONG_STATUS},
  {"Handler_rollback",         (char*) offsetof(STATUS_VAR, ha_rollback_count), SHOW_LONG_STATUS},
  {"Handler_savepoint",        (char*) offsetof(STATUS_VAR, ha_savepoint_count), SHOW_LONG_STATUS},
  {"Handler_savepoint_rollback",(char*) offsetof(STATUS_VAR, ha_savepoint_rollback_count), SHOW_LONG_STATUS},
  {"Handler_tmp_update",       (char*) offsetof(STATUS_VAR, ha_tmp_update_count), SHOW_LONG_STATUS},
  {"Handler_tmp_write",        (char*) offsetof(STATUS_VAR, ha_tmp_write_count), SHOW_LONG_STATUS},
  {"Handler_update",           (char*) offsetof(STATUS_VAR, ha_update_count), SHOW_LONG_STATUS},
  {"Handler_write",            (char*) offsetof(STATUS_VAR, ha_write_count), SHOW_LONG_STATUS},
  {"Key",                      (char*) &show_default_keycache, SHOW_FUNC},
  {"Last_query_cost",          (char*) offsetof(STATUS_VAR, last_query_cost), SHOW_DOUBLE_STATUS},
  {"Max_used_connections",     (char*) &max_used_connections,  SHOW_LONG},
  {"Not_flushed_delayed_rows", (char*) &delayed_rows_in_use,    SHOW_LONG_NOFLUSH},
  {"Open_files",               (char*) &my_file_opened,         SHOW_LONG_NOFLUSH},
  {"Open_streams",             (char*) &my_stream_opened,       SHOW_LONG_NOFLUSH},
  {"Open_table_definitions",   (char*) &show_table_definitions, SHOW_FUNC},
  {"Open_tables",              (char*) &show_open_tables,       SHOW_FUNC},
  {"Opened_files",             (char*) &my_file_total_opened, SHOW_LONG_NOFLUSH},
  {"Opened_table_definitions", (char*) offsetof(STATUS_VAR, opened_shares), SHOW_LONG_STATUS},
  {"Opened_tables",            (char*) offsetof(STATUS_VAR, opened_tables), SHOW_LONG_STATUS},
  {"Opened_views",            (char*) offsetof(STATUS_VAR, opened_views), SHOW_LONG_STATUS},
  {"Prepared_stmt_count",      (char*) &show_prepared_stmt_count, SHOW_FUNC},
  {"Rows_read",                (char*) offsetof(STATUS_VAR, rows_read), SHOW_LONGLONG_STATUS},
  {"Rows_sent",                (char*) offsetof(STATUS_VAR, rows_sent), SHOW_LONGLONG_STATUS},
  {"Rows_tmp_read",            (char*) offsetof(STATUS_VAR, rows_tmp_read), SHOW_LONGLONG_STATUS},
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
  {"Slave_heartbeat_period",   (char*) &show_heartbeat_period, SHOW_FUNC},
  {"Slave_received_heartbeats",(char*) &show_slave_received_heartbeats, SHOW_FUNC},
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
#ifndef EMBEDDED_LIBRARY
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
#endif
#endif /* HAVE_OPENSSL */
  {"Syncs",                    (char*) &my_sync_count,          SHOW_LONG_NOFLUSH},
  /*
    Expression cache used only for caching subqueries now, so its statistic
    variables we call subquery_cache*.
  */
  {"Subquery_cache_hit",       (char*) &subquery_cache_hit,     SHOW_LONG},
  {"Subquery_cache_miss",      (char*) &subquery_cache_miss,    SHOW_LONG},
  {"Table_locks_immediate",    (char*) &locks_immediate,        SHOW_LONG},
  {"Table_locks_waited",       (char*) &locks_waited,           SHOW_LONG},
#ifdef HAVE_MMAP
  {"Tc_log_max_pages_used",    (char*) &tc_log_max_pages_used,  SHOW_LONG},
  {"Tc_log_page_size",         (char*) &tc_log_page_size,       SHOW_LONG_NOFLUSH},
  {"Tc_log_page_waits",        (char*) &tc_log_page_waits,      SHOW_LONG},
#endif
#ifdef HAVE_POOL_OF_THREADS
  {"Threadpool_idle_threads",  (char *) &show_threadpool_idle_threads, SHOW_FUNC},
  {"Threadpool_threads",       (char *) &tp_stats.num_worker_threads, SHOW_INT},
#endif
  {"Threads_cached",           (char*) &cached_thread_count,    SHOW_LONG_NOFLUSH},
  {"Threads_connected",        (char*) &connection_count,       SHOW_INT},
  {"Threads_created",	       (char*) &thread_created,		SHOW_LONG_NOFLUSH},
  {"Threads_running",          (char*) &thread_running,         SHOW_INT},
  {"Uptime",                   (char*) &show_starttime,         SHOW_FUNC},
#ifdef ENABLED_PROFILING
  {"Uptime_since_flush_status",(char*) &show_flushstatustime,   SHOW_FUNC},
#endif
  {NullS, NullS, SHOW_LONG}
};

bool add_terminator(DYNAMIC_ARRAY *options)
{
  my_option empty_element= {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0};
  return insert_dynamic(options, (uchar *)&empty_element);
}

#ifndef EMBEDDED_LIBRARY
static void print_version(void)
{
  set_server_version();

  printf("%s  Ver %s for %s on %s (%s)\n",my_progname,
	 server_version,SYSTEM_TYPE,MACHINE_TYPE, MYSQL_COMPILATION_COMMENT);
}

/** Compares two options' names, treats - and _ the same */
static int option_cmp(my_option *a, my_option *b)
{
  const char *sa= a->name;
  const char *sb= b->name;
  for (; *sa || *sb; sa++, sb++)
  {
    if (*sa < *sb)
    {
      if (*sa == '-' && *sb == '_')
        continue;
      else
        return -1;
    }
    if (*sa > *sb)
    {
      if (*sa == '_' && *sb == '-')
        continue;
      else
        return 1;
    }
  }
  DBUG_ASSERT(a->name == b->name);
  return 0;
}

static void print_help()
{
  MEM_ROOT mem_root;
  init_alloc_root(&mem_root, 4096, 4096);

  pop_dynamic(&all_options);
  sys_var_add_options(&all_options, sys_var::PARSE_EARLY);
  add_plugin_options(&all_options, &mem_root);
  sort_dynamic(&all_options, (qsort_cmp) option_cmp);
  add_terminator(&all_options);

  my_print_help((my_option*) all_options.buffer);
  my_print_variables((my_option*) all_options.buffer);

  free_root(&mem_root, MYF(0));
}

static void usage(void)
{
  DBUG_ENTER("usage");
  if (!(default_charset_info= get_charset_by_csname(default_character_set_name,
					           MY_CS_PRIMARY,
						   MYF(MY_WME))))
    exit(1);
  if (!default_collation_name)
    default_collation_name= (char*) default_charset_info->name;
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
  puts("Starts the MariaDB database server.\n");
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
  print_help();

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
  DBUG_VOID_RETURN;
}
#endif /*!EMBEDDED_LIBRARY*/

/**
  Initialize MySQL global variables to default values.

  @note
    The reason to set a lot of global variables to zero is to allow one to
    restart the embedded server with a clean environment
    It's also needed on some exotic platforms where global variables are
    not set to 0 when a program starts.

    We don't need to set variables refered to in my_long_options
    as these are initialized by my_getopt.
*/

static int mysql_init_variables(void)
{
  /* Things reset to zero */
  opt_skip_slave_start= opt_reckless_slave = 0;
  mysql_home[0]= pidfile_name[0]= log_error_file[0]= 0;
#if defined(HAVE_REALPATH) && !defined(HAVE_valgrind) && !defined(HAVE_BROKEN_REALPATH)
  /*  We can only test for sub paths if my_symlink.c is using realpath */
  myisam_test_invalid_symlink= test_if_data_home_dir;
#endif
  opt_log= opt_slow_log= 0;
  opt_bin_log= opt_bin_log_used= 0;
  opt_disable_networking= opt_skip_show_db=0;
  opt_skip_name_resolve= 0;
  opt_ignore_builtin_innodb= 0;
  opt_logname= opt_binlog_index_name= opt_slow_logname= 0;
  opt_log_basename= 0;
  opt_tc_log_file= (char *)"tc.log";      // no hostname in tc_log file name !
  opt_secure_auth= 0;
  opt_bootstrap= opt_myisam_log= 0;
  mqh_used= 0;
  kill_in_progress= 0;
  cleanup_done= 0;
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
  subquery_cache_miss= subquery_cache_hit= 0;
  delayed_insert_threads= delayed_insert_writes= delayed_rows_in_use= 0;
  delayed_insert_errors= thread_created= 0;
  specialflag= 0;
  binlog_cache_use=  binlog_cache_disk_use= 0;
  max_used_connections= slow_launch_threads = 0;
  mysqld_user= mysqld_chroot= opt_init_file= opt_bin_logname = 0;
  prepared_stmt_count= 0;
  mysqld_unix_port= opt_mysql_tmpdir= my_bind_addr_str= NullS;
  bzero((uchar*) &mysql_tmpdir_list, sizeof(mysql_tmpdir_list));
  bzero((char *) &global_status_var, sizeof(global_status_var));
  opt_large_pages= 0;
  opt_super_large_pages= 0;
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

  opt_specialflag= SPECIAL_ENGLISH;
  unix_sock= base_ip_sock= extra_ip_sock= INVALID_SOCKET;
  mysql_home_ptr= mysql_home;
  pidfile_name_ptr= pidfile_name;
  log_error_file_ptr= log_error_file;
  protocol_version= PROTOCOL_VERSION;
  what_to_log= ~ (1L << (uint) COM_TIME);
  refresh_version= 1L;	/* Increments on each reload */
  executed_events= 0;
  global_query_id= thread_id= 1L;
  my_atomic_rwlock_init(&global_query_id_lock);
  my_atomic_rwlock_init(&thread_running_lock);
  strmov(server_version, MYSQL_SERVER_VERSION);
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
  mysql_real_data_home_len=
    strmake(mysql_real_data_home, get_relative_path(MYSQL_DATADIR),
            sizeof(mysql_real_data_home)-1) - mysql_real_data_home;
  /* Replication parameters */
  master_info_file= (char*) "master.info",
    relay_log_info_file= (char*) "relay-log.info";
  report_user= report_password = report_host= 0;	/* TO BE DELETED */
  opt_relay_logname= opt_relaylog_index_name= 0;

  /* Variables in libraries */
  charsets_dir= 0;
  default_character_set_name= (char*) MYSQL_DEFAULT_CHARSET_NAME;
  default_collation_name= compiled_default_collation_name;
  character_set_filesystem_name= (char*) "binary";
  lc_messages= (char*) "en_US";
  lc_time_names_name= (char*) "en_US";
  
  /* Variables that depends on compile options */
#ifndef DBUG_OFF
  default_dbug_option=IF_WIN("d:t:i:O,\\mysqld.trace",
			     "d:t:i:o,/tmp/mysqld.trace");
  current_dbug_option= default_dbug_option;
#endif
  opt_error_log= IF_WIN(1,0);
#ifdef ENABLED_PROFILING
    have_profiling = SHOW_OPTION_YES;
#else
    have_profiling = SHOW_OPTION_NO;
#endif

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
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
#ifndef EMBEDDED_LIBRARY
  ssl_acceptor_fd= 0;
#endif /* ! EMBEDDED_LIBRARY */
#endif /* HAVE_OPENSSL */
#ifdef HAVE_SMEM
  shared_memory_base_name= default_shared_memory_base_name;
#endif

#if defined(__WIN__)
  /* Allow Win32 users to move MySQL anywhere */
  {
    char prg_dev[LIBLEN];
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
      my_path(prg_dev, my_progname, "mysql/bin");
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
  switch(optid) {
  case '#':
#ifndef DBUG_OFF
    if (!argument)
      argument= (char*) default_dbug_option;
    if (argument[0] == '0' && !argument[1])
    {
      DEBUGGER_OFF;
      break;
    }
    DEBUGGER_ON;
    if (argument[0] == '1' && !argument[1])
      break;
    DBUG_SET_INITIAL(argument);
    opt_endinfo=1;				/* unireg: memory allocation */
#else
    sql_print_warning("'%s' is disabled in this build", opt->name);
#endif
    break;
  case OPT_DEPRECATED_OPTION:
    sql_print_warning("'%s' is deprecated. It does nothing and exists only "
                      "for compatiblity with old my.cnf files.",
                      opt->name);
    break;
  case 'a':
    global_system_variables.sql_mode= MODE_ANSI;
    global_system_variables.tx_isolation= ISO_SERIALIZABLE;
    break;
  case 'b':
    strmake(mysql_home,argument,sizeof(mysql_home)-1);
    break;
  case 'C':
    if (default_collation_name == compiled_default_collation_name)
      default_collation_name= 0;
    break;
  case 'l':
    WARN_DEPRECATED(NULL, 7, 0, "--log", "'--general-log'/'--general-log-file'");
    opt_log=1;
    break;
  case 'h':
    strmake(mysql_real_data_home,argument, sizeof(mysql_real_data_home)-1);
    /* Correct pointer set by my_getopt (for embedded library) */
    mysql_real_data_home_ptr= mysql_real_data_home;
    break;
  case 'u':
    if (!mysqld_user || !strcmp(mysqld_user, argument))
      mysqld_user= argument;
    else
      sql_print_warning("Ignoring user change to '%s' because the user was set to '%s' earlier on the command line\n", argument, mysqld_user);
    break;
  case 'L':
    strmake(lc_messages_dir, argument, sizeof(lc_messages_dir)-1);
    break;
  case OPT_BINLOG_FORMAT:
    binlog_format_used= true;
    break;
#include <sslopt-case.h>
#ifndef EMBEDDED_LIBRARY
  case 'V':
    print_version();
    opt_abort= 1;                    // Abort after parsing all options
    break;
#endif /*EMBEDDED_LIBRARY*/
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
  case (int) OPT_ISAM_LOG:
    opt_myisam_log=1;
    break;
  case (int) OPT_BIN_LOG:
    opt_bin_log= test(argument != disabled_my_option);
    opt_bin_log_used= 1;
    break;
  case (int) OPT_LOG_BASENAME:
  {
    if (opt_log_basename[0] == 0 || strchr(opt_log_basename, FN_EXTCHAR) ||
        strchr(opt_log_basename,FN_LIBCHAR))
    {
      sql_print_error("Wrong argument for --log-basename. It can't be empty or contain '.' or '" FN_DIRSEP "'");
      return 1;
    }
    if (log_error_file_ptr != disabled_my_option)
      log_error_file_ptr= opt_log_basename;

    make_default_log_name(&opt_logname, ".log", false);
    make_default_log_name(&opt_slow_logname, "-slow.log", false);
    make_default_log_name(&opt_bin_logname, "-bin", true);
    make_default_log_name(&opt_binlog_index_name, "-bin.index", true);
    make_default_log_name(&opt_relay_logname, "-relay-bin", true);
    make_default_log_name(&opt_relaylog_index_name, "-relay-bin.index", true);

    pidfile_name_ptr= pidfile_name;
    strmake(pidfile_name, argument, sizeof(pidfile_name)-5);
    strmov(fn_ext(pidfile_name),".pid");

    /* check for errors */
    if (!opt_bin_logname || !opt_relaylog_index_name || ! opt_logname ||
        ! opt_slow_logname || !pidfile_name_ptr)
      return 1;                                 // out of memory error
    break;
  }
#ifdef HAVE_REPLICATION
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
    /* See also OPT_REWRITE_DB handling in client/mysqlbinlog.cc */
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
    WARN_DEPRECATED(NULL, 7, 0, "--log-slow-queries", "'--slow-query-log'/'--slow-query-log-file'");
    opt_slow_log= 1;
    break;
  case (int) OPT_SAFE:
    opt_specialflag|= SPECIAL_SAFE_MODE | SPECIAL_NO_NEW_FUNC;
    delay_key_write_options= (uint) DELAY_KEY_WRITE_NONE;
    myisam_recover_options= HA_RECOVER_DEFAULT;
    ha_open_options&= ~(HA_OPEN_DELAY_KEY_WRITE);
#ifdef HAVE_QUERY_CACHE
    query_cache_size=0;
#endif
    sql_print_warning("The syntax '--safe-mode' is deprecated and will be "
                      "removed in a future release.");
    break;
  case (int) OPT_SKIP_PRIOR:
    opt_specialflag|= SPECIAL_NO_PRIOR;
    sql_print_warning("The --skip-thread-priority startup option is deprecated "
                      "and will be removed in MySQL 7.0. This option has no effect "
                      "as the implied behavior is already the default.");
    break;
  case (int) OPT_SKIP_HOST_CACHE:
    opt_specialflag|= SPECIAL_NO_HOST_CACHE;
    break;
  case (int) OPT_SKIP_RESOLVE:
    opt_skip_name_resolve= 1;
    opt_specialflag|=SPECIAL_NO_RESOLVE;
    break;
  case (int) OPT_WANT_CORE:
    test_flags |= TEST_CORE_ON_SIGNAL;
    break;
  case (int) OPT_BIND_ADDRESS:
    {
      struct addrinfo *res_lst, hints;    

      bzero(&hints, sizeof(struct addrinfo));
      hints.ai_socktype= SOCK_STREAM;
      hints.ai_protocol= IPPROTO_TCP;

      if (getaddrinfo(argument, NULL, &hints, &res_lst) != 0) 
      {
        sql_print_error("Can't start server: cannot resolve hostname!");
        return 1;
      }

      if (res_lst->ai_next)
      {
        sql_print_error("Can't start server: bind-address refers to multiple interfaces!");
        return 1;
      }
      freeaddrinfo(res_lst);
    }
    break;
  case OPT_CONSOLE:
    if (opt_console)
      opt_error_log= 0;			// Force logs to stdout
    break;
  case OPT_BOOTSTRAP:
    opt_noacl=opt_bootstrap=1;
    break;
  case OPT_SERVER_ID:
    server_id_supplied = 1;
    break;
  case OPT_ONE_THREAD:
    thread_handling= SCHEDULER_NO_THREADS;
    break;
  case OPT_LOWER_CASE_TABLE_NAMES:
    lower_case_table_names_used= 1;
    break;
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
  case OPT_ENGINE_CONDITION_PUSHDOWN:
    /*
      The last of --engine-condition-pushdown and --optimizer_switch on
      command line wins (see get_options().
    */
    if (global_system_variables.engine_condition_pushdown)
      global_system_variables.optimizer_switch|=
        OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN;
    else
      global_system_variables.optimizer_switch&=
        ~OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN;
    break;
  case OPT_LOG_ERROR:
    /*
      "No --log-error" == "write errors to stderr",
      "--log-error without argument" == "write errors to a file".
    */
    if (argument == NULL) /* no argument */
      log_error_file_ptr= const_cast<char*>("");
    break;
  case OPT_MAX_LONG_DATA_SIZE:
    max_long_data_size_used= true;
    break;


  case OPT_IGNORE_DB_DIRECTORY:
    if (*argument == 0)
      ignore_db_dirs_reset();
    else
    {
      if (push_ignored_db_dir(argument))
      {
        sql_print_error("Can't start server: "
                        "cannot process --ignore-db-dir=%.*s", 
                        FN_REFLEN, argument);
        return 1;
      }
    }
    break;
  }
  return 0;
}


/** Handle arguments for multiple key caches. */

C_MODE_START

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
  case OPT_KEY_CACHE_PARTITIONS:
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
    case OPT_KEY_CACHE_PARTITIONS:
      return (uchar**) &key_cache->param_partitions;
    }
  }
  }
  return option->value;
}

static void option_error_reporter(enum loglevel level, const char *format, ...)
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

C_MODE_END

/**
  Get server options from the command line,
  and perform related server initializations.
  @param [in, out] argc_ptr       command line options (count)
  @param [in, out] argv_ptr       command line options (values)
  @return 0 on success

  @todo
  - FIXME add EXIT_TOO_MANY_ARGUMENTS to "mysys_err.h" and return that code?
*/
static int get_options(int *argc_ptr, char ***argv_ptr)
{
  int ho_error;

  my_getopt_register_get_addr(mysql_getopt_value);
  my_getopt_error_reporter= option_error_reporter;

  /* prepare all_options array */
  my_init_dynamic_array(&all_options, sizeof(my_option),
                        array_elements(my_long_options),
                        array_elements(my_long_options)/4);
  for (my_option *opt= my_long_options;
       opt < my_long_options + array_elements(my_long_options) - 1;
       opt++)
    insert_dynamic(&all_options, (uchar*) opt);
  sys_var_add_options(&all_options, 0);
  add_terminator(&all_options);

  /* Skip unknown options so that they may be processed later by plugins */
  my_getopt_skip_unknown= TRUE;

  if ((ho_error= handle_options(argc_ptr, argv_ptr, (my_option*)(all_options.buffer),
                                mysqld_get_one_option)))
    return ho_error;

  if (!opt_help)
    delete_dynamic(&all_options);
  else
    opt_abort= 1;

  /* Add back the program name handle_options removes */
  (*argc_ptr)++;
  (*argv_ptr)--;

  /*
    Options have been parsed. Now some of them need additional special
    handling, like custom value checking, checking of incompatibilites
    between options, setting of multiple variables, etc.
    Do them here.
  */

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

  if (log_error_file_ptr != disabled_my_option)
    opt_error_log= 1;
  else
    log_error_file_ptr= const_cast<char*>("");

  opt_init_connect.length=strlen(opt_init_connect.str);
  opt_init_slave.length=strlen(opt_init_slave.str);

  if (global_system_variables.low_priority_updates)
    thr_upgraded_concurrent_insert_lock= TL_WRITE_LOW_PRIORITY;

  if (ft_boolean_check_syntax_string((uchar*) ft_boolean_syntax))
  {
    sql_print_error("Invalid ft-boolean-syntax string: %s\n",
                    ft_boolean_syntax);
    return 1;
  }

  if (opt_disable_networking)
    mysqld_port= mysqld_extra_port= 0;

  if (opt_skip_show_db)
    opt_specialflag|= SPECIAL_SKIP_SHOW_DB;

  if (myisam_flush)
    flush_time= 0;

#ifdef HAVE_REPLICATION
  if (opt_slave_skip_errors)
    init_slave_skip_errors(opt_slave_skip_errors);
#endif

  if (global_system_variables.max_join_size == HA_POS_ERROR)
    global_system_variables.option_bits|= OPTION_BIG_SELECTS;
  else
    global_system_variables.option_bits&= ~OPTION_BIG_SELECTS;

  // Synchronize @@global.autocommit on --autocommit
  const ulonglong turn_bit_on= opt_autocommit ?
    OPTION_AUTOCOMMIT : OPTION_NOT_AUTOCOMMIT;
  global_system_variables.option_bits=
    (global_system_variables.option_bits &
     ~(OPTION_NOT_AUTOCOMMIT | OPTION_AUTOCOMMIT)) | turn_bit_on;

  global_system_variables.sql_mode=
    expand_sql_mode(global_system_variables.sql_mode);
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
    test_flags|= TEST_SIGINT;
    opt_stack_trace= 1;
    test_flags&= ~TEST_CORE_ON_SIGNAL;
  }
  /* Set global MyISAM variables from delay_key_write_options */
  fix_delay_key_write(0, 0, OPT_GLOBAL);

#ifndef EMBEDDED_LIBRARY
  if (mysqld_chroot)
    set_root(mysqld_chroot);
#else
  thread_handling = SCHEDULER_NO_THREADS;
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

  /*
    Log mysys errors when we don't have a thd or thd->log_all_errors is set
    (recovery) to the log.  This is mainly useful for debugging strange system
    errors.
  */
  if (global_system_variables.log_warnings >= 10)
    my_global_flags= MY_WME | ME_JUST_INFO;
  /* Log all errors not handled by thd->handle_error() to my_message_sql() */
  if (global_system_variables.log_warnings >= 11)
    my_global_flags|= ME_NOREFRESH;
  if (my_assert_on_error)
    debug_assert_if_crashed_table= 1;

  global_system_variables.long_query_time= (ulonglong)
    (global_system_variables.long_query_time_double * 1e6);

  if (opt_short_log_format)
    opt_specialflag|= SPECIAL_SHORT_LOG_FORMAT;

  if (init_global_datetime_format(MYSQL_TIMESTAMP_DATE,
                                  &global_date_format) ||
      init_global_datetime_format(MYSQL_TIMESTAMP_TIME,
                                  &global_time_format) ||
      init_global_datetime_format(MYSQL_TIMESTAMP_DATETIME,
                                  &global_datetime_format))
    return 1;

#ifdef EMBEDDED_LIBRARY
  one_thread_scheduler(thread_scheduler);
  one_thread_scheduler(extra_thread_scheduler);
#else

#ifdef _WIN32
  /* workaround: disable thread pool on XP */
  if (GetProcAddress(GetModuleHandle("kernel32"),"CreateThreadpool") == 0 &&
      thread_handling > SCHEDULER_NO_THREADS)
    thread_handling = SCHEDULER_ONE_THREAD_PER_CONNECTION;
#endif

  if (thread_handling <= SCHEDULER_ONE_THREAD_PER_CONNECTION)
    one_thread_per_connection_scheduler(thread_scheduler, &max_connections,
                                        &connection_count);
  else if (thread_handling == SCHEDULER_NO_THREADS)
    one_thread_scheduler(thread_scheduler);
  else
    pool_of_threads_scheduler(thread_scheduler,  &max_connections,
                                        &connection_count); 

  one_thread_per_connection_scheduler(extra_thread_scheduler,
                                      &extra_max_connections,
                                      &extra_connection_count);
#endif

  global_system_variables.engine_condition_pushdown=
    test(global_system_variables.optimizer_switch &
         OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN);

  opt_readonly= read_only;

  /*
    If max_long_data_size is not specified explicitly use
    value of max_allowed_packet.
  */
  if (!max_long_data_size_used)
    max_long_data_size= global_system_variables.max_allowed_packet;

  /* Rember if max_user_connections was 0 at startup */
  max_user_connections_checking= global_system_variables.max_user_connections != 0;
  return 0;
}


/*
  Create version name for running mysqld version
  We automaticly add suffixes -debug, -embedded and -log to the version
  name to make the version more descriptive.
  (MYSQL_SERVER_SUFFIX is set by the compilation environment)
*/

void set_server_version(void)
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
  if (opt_log || opt_slow_log || opt_bin_log)
    strmov(end, "-log");                        // This may slow down system
}


static char *get_relative_path(const char *path)
{
  if (test_if_hard_path(path) &&
      is_prefix(path,DEFAULT_MYSQL_HOME) &&
      strcmp(DEFAULT_MYSQL_HOME,FN_ROOTDIR))
  {
    path+=(uint) strlen(DEFAULT_MYSQL_HOME);
    while (*path == FN_LIBCHAR || *path == FN_LIBCHAR2)
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
    size_t length= dirname_length(path);        // Guaranteed to be < FN_REFLEN
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
  DBUG_ENTER("fix_paths");

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
  convert_dirname(lc_messages_dir, lc_messages_dir, NullS);
  convert_dirname(mysql_real_data_home,mysql_real_data_home,NullS);
  (void) my_load_path(mysql_home,mysql_home,""); // Resolve current dir
  (void) my_load_path(mysql_real_data_home,mysql_real_data_home,mysql_home);
  (void) my_load_path(pidfile_name, pidfile_name_ptr, mysql_real_data_home);

  convert_dirname(opt_plugin_dir, opt_plugin_dir_ptr ? opt_plugin_dir_ptr : 
                                  get_relative_path(PLUGINDIR), NullS);
  (void) my_load_path(opt_plugin_dir, opt_plugin_dir, mysql_home);
  opt_plugin_dir_ptr= opt_plugin_dir;
  pidfile_name_ptr= pidfile_name;

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
  (void) my_load_path(lc_messages_dir, lc_messages_dir, buff);

  /* If --character-sets-dir isn't given, use shared library dir */
  if (charsets_dir)
    strmake(mysql_charsets_dir, charsets_dir, sizeof(mysql_charsets_dir)-1);
  else
    strxnmov(mysql_charsets_dir, sizeof(mysql_charsets_dir)-1, buff,
	     CHARSET_DIR, NullS);
  (void) my_load_path(mysql_charsets_dir, mysql_charsets_dir, buff);
  convert_dirname(mysql_charsets_dir, mysql_charsets_dir, NullS);
  charsets_dir=mysql_charsets_dir;

  if (init_tmpdir(&mysql_tmpdir_list, opt_mysql_tmpdir))
    DBUG_RETURN(1);
  if (!opt_mysql_tmpdir)
    opt_mysql_tmpdir= mysql_tmpdir;
#ifdef HAVE_REPLICATION
  if (!slave_load_tmpdir)
    slave_load_tmpdir= mysql_tmpdir;
#endif /* HAVE_REPLICATION */
  /*
    Convert the secure-file-priv option to system format, allowing
    a quick strcmp to check if read or write is in an allowed dir
  */
  if (opt_secure_file_priv)
  {
    if (*opt_secure_file_priv == 0)
    {
      my_free(opt_secure_file_priv);
      opt_secure_file_priv= 0;
    }
    else
    {
      if (strlen(opt_secure_file_priv) >= FN_REFLEN)
        opt_secure_file_priv[FN_REFLEN-1]= '\0';
      if (my_realpath(buff, opt_secure_file_priv, 0))
      {
        sql_print_warning("Failed to normalize the argument for --secure-file-priv.");
        DBUG_RETURN(1);
      }
      char *secure_file_real_path= (char *)my_malloc(FN_REFLEN, MYF(MY_FAE));
      convert_dirname(secure_file_real_path, buff, NullS);
      my_free(opt_secure_file_priv);
      opt_secure_file_priv= secure_file_real_path;
    }
  }
  DBUG_RETURN(0);
}

/**
  Check if file system used for databases is case insensitive.

  @param dir_name			Directory to test

  @retval -1  Don't know (Test failed)
  @retval  0   File system is case sensitive
  @retval  1   File system is case insensitive
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
  mysql_file_delete(key_file_casetest, buff2, MYF(0));
  if ((file= mysql_file_create(key_file_casetest,
                               buff, 0666, O_RDWR, MYF(0))) < 0)
  {
    if (!opt_abort)
      sql_print_warning("Can't create test file %s", buff);
    DBUG_RETURN(-1);
  }
  mysql_file_close(file, MYF(0));
  if (mysql_file_stat(key_file_casetest, buff2, &stat_info, MYF(0)))
    result= 1;					// Can access file
  mysql_file_delete(key_file_casetest, buff, MYF(MY_WME));
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
  if ((file= mysql_file_create(key_file_pid, pidfile_name, 0664,
                               O_WRONLY | O_TRUNC, MYF(MY_WME))) >= 0)
  {
    char buff[21], *end;
    end= int10_to_str((long) getpid(), buff, 10);
    *end++= '\n';
    if (!mysql_file_write(file, (uchar*) buff, (uint) (end-buff),
                          MYF(MY_WME | MY_NABP)))
    {
      mysql_file_close(file, MYF(0));
      return;
    }
    mysql_file_close(file, MYF(0));
  }
  sql_perror("Can't start server: can't create PID file");
  exit(1);
}
#endif /* EMBEDDED_LIBRARY */

/** Clear most status variables. */
void refresh_status(THD *thd)
{
  mysql_mutex_lock(&LOCK_status);

  /* Add thread's status variabes to global status */
  add_to_status(&global_status_var, &thd->status_var);

  /* Reset thread's status variables */
  bzero((uchar*) &thd->status_var, sizeof(thd->status_var));
  bzero((uchar*) &thd->org_status_var, sizeof(thd->org_status_var)); 
  thd->start_bytes_received= 0;

  /* Reset some global variables */
  reset_status_vars();

  /* Reset the counters of all key caches (default and named). */
  process_key_caches(reset_key_cache_counters, 0);
  flush_status_time= time((time_t*) 0);
  mysql_mutex_unlock(&LOCK_status);

  /*
    Set max_used_connections to the number of currently open
    connections.  Lock LOCK_thread_count out of LOCK_status to avoid
    deadlocks.  Status reset becomes not atomic, but status data is
    not exact anyway.
  */
  mysql_mutex_lock(&LOCK_thread_count);
  max_used_connections= thread_count-delayed_insert_threads;
  mysql_mutex_unlock(&LOCK_thread_count);
}


/*****************************************************************************
  Instantiate variables for missing storage engines
  This section should go away soon
*****************************************************************************/

/*****************************************************************************
  Instantiate templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
/* Used templates */
template class I_List<THD>;
template class I_List_iterator<THD>;
template class I_List<i_string>;
template class I_List<i_string_pair>;
template class I_List<Statement>;
template class I_List_iterator<Statement>;
#endif

