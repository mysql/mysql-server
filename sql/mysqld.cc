/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file sql/mysqld.cc
  MySQL server daemon.
*/

/* clang-format off */
/**
  @mainpage Welcome

  Welcome to the MySQL source code documentation.

  The order chosen to present the content is to start with low level components,
  and build upon previous sections, so that code is presented in a logical order.

  For some sections, a full article (Doxygen 'page') presents the component in detail.

  For other sections, only links are provided, as a starting point into the component.

  For the user manual, see http://dev.mysql.com/doc/refman/8.0/en/

  For the internals manual, see https://dev.mysql.com/doc/internals/en/index.html

  Document generated on: ${DOXYGEN_GENERATION_DATE},
  branch: ${DOXYGEN_GENERATION_BRANCH},
  revision: ${DOXYGEN_GENERATION_REVISION}
*/

/**
  @page PAGE_GET_STARTED Getting Started

  - @ref start_source
  - @subpage PAGE_CODING_GUIDELINES
  - @ref start_debug

  @section start_source Build from source

  See https://dev.mysql.com/doc/refman/8.0/en/source-installation.html

  @section start_debug Debugging

  The easiest way to install a server, and attach a debugger to it,
  is to start the mysql-test-run (MTR) tool with debugging options

  @verbatim
  cd mysql-test
  ./mtr --ddd main.parser
  @endverbatim

  The following functions are good candidates for breakpoints:
  - #my_message_sql
  - #dispatch_command

  Replace 'main.parser' with another test script, or write your own, to debug a specific area.
*/

/**
  @page PAGE_CODING_GUIDELINES Coding Guidelines

  This section shows the guidelines that MySQL developers
  follow when writing new code.

  New MySQL code uses the Google C++ coding style
  (https://google.github.io/styleguide/cppguide.html), with one
  exception:

  - Member variable names: Do not use foo_. Instead, use
    m_foo (non-static) or s_foo (static).

  Old projects and modifications to old code use an older MySQL-specific
  style for the time being. Since 8.0, MySQL style uses the same formatting
  rules as Google coding style (e.g., brace placement, indentation, line
  lengths, etc.), but differs in a few important aspects:

  - Class names: Do not use MyClass. Instead, use My_class.

  - Function names: Use snake_case().

  - Comment Style: Use either the // or <em>/</em>* *<em>/</em> syntax. // is
    much more common but both syntaxes are permitted for the time being.

  - Doxygen comments: Use <em>/</em>** ... *<em>/</em> syntax and not ///.

  - Doxygen commands: Use '@' and not '\' for doxygen commands.

  - You may see structs starting with st_ and being typedef-ed to some
    UPPERCASE (e.g. typedef struct st_foo { ... } FOO). However,
    this is legacy from when the codebase contained C. Do not make such new
    typedefs nor structs with st_ prefixes, and feel free to remove those that
    already exist, except in public header files that are part of libmysql
    (which need to be parseable as C99).


  Code formatting is enforced by use of clang-format throughout the code
  base. However, note that formatting is only one part of coding style;
  you are required to take care of non-formatting issues yourself, such as
  following naming conventions, having clear ownership of code or minimizing
  the use of macros. See the Google coding style guide for the entire list.

  Consistent style is important for us, because everyone must know what to
  expect. Knowing our rules, you'll find it easier to read our code, and when
  you decide to contribute (which we hope you'll consider!) we'll find it
  easier to read and review your code.

  - @subpage GENERAL_DEVELOPMENT_GUIDELINES
  - @subpage CPP_CODING_GUIDELINES_FOR_NDB_SE
  - @subpage DBUG_TAGS

*/

/**
  @page PAGE_INFRASTRUCTURE Infrastructure

  @section infra_basic Basic classes and templates

  @subsection infra_basic_container Container

  See #DYNAMIC_ARRAY, #List, #I_P_List, #LF_HASH.

  @subsection infra_basic_syncho Synchronization

  See #native_mutex_t, #native_rw_lock_t, #native_cond_t.

  @subsection infra_basic_fileio File IO

  See #my_open, #my_dir.

  @section infra_server_blocks Server building blocs

  @subsection infra_server_blocks_vio Virtual Input Output

  See #Vio, #vio_init.

  @section deployment Deployment

  @subsection deploy_install Installation

  See #opt_initialize, #bootstrap::run_bootstrap_thread.

  @subsection deploy_startup Startup

  See #mysqld_main.

  @subsection deploy_shutdown Shutdown

  See #handle_fatal_signal, #signal_hand.

  @subsection deploy_upgrade Upgrade

  See #Mysql::Tools::Upgrade::Program.

*/

/**
  @page PAGE_PROTOCOL Client/Server Protocol

  @section protocol_overview Overview

  The MySQL protocol is used between MySQL Clients and a MySQL Server.
  It is implemented by:
    - Connectors (Connector/C, Connector/J, and so forth)
    - MySQL Proxy
    - Communication between master and slave replication servers

  The protocol supports these features:
    - Transparent encryption using SSL
    - Transparent compression
    - A @ref page_protocol_connection_phase where capabilities and
      authentication data are exchanged
    - A @ref page_protocol_command_phase which accepts commands
      from the client and executes them

  Further reading:
    - @subpage page_protocol_basics
    - @subpage page_protocol_connection_lifecycle
*/

/**
  @page PAGE_SQL_EXECUTION SQL Query Execution

  @section sql_query_exec_parsing SQL Parsing

  The parser processes SQL strings and builds a tree representation of them.

  See @ref GROUP_PARSER.

  @subpage PAGE_SQL_Optimizer

  @subpage stored_programs

  @section sql_query_exec_prepared Prepared statements

  See #mysql_stmt_prepare

  @section func_stored_proc Stored procedures

  See #sp_head, #sp_instr.

  @section sql_query_exec_sql_functions SQL Functions

  See #Item_func

  @section sql_query_exec_error_handling Error handling

  See #my_message, #my_error

  @subpage PAGE_TXN

*/

/**
  @page PAGE_STORAGE Data Storage

  @section storage_innodb Innodb

  See #ha_innobase or read details about InnoDB internals:
  - @subpage PAGE_INNODB_PFS
  - @subpage PAGE_INNODB_REDO_LOG
  - @subpage PAGE_INNODB_UTILS

  @section storage_temptable Temp table

  Before 8.0, temporary tables were handled by heap engine.
  The heap engine had no feature to store bigger tables on disk.

  Since 8.0, there is a brand new temptable engine, which
  is written from scratch using c++11. It has following advantages:
  - it is able to store bigger tables on disk (in temporary files),
  - it uses row format with variable size (can save memory for varchars),
  - it is better designed (easier to maintain).

  @subpage PAGE_TEMPTABLE

*/


/**
  @page PAGE_REPLICATION Replication

  @subpage PAGE_RPL_FIELD_METADATA

*/

/**
  @page PAGE_TXN Transactions

  See #trans_begin, #trans_commit, #trans_rollback.
*/

/**
  @page PAGE_SECURITY Security

  @subpage AUTHORIZATION_PAGE
*/


/**
  @page PAGE_MONITORING Monitoring

  @subpage PAGE_PFS
*/

/**
  @page PAGE_EXTENDING Extending MySQL

  Components
  ----------

  MySQL 8.0 introduces support for extending the server through components.
  Components can communicate with other components through service APIs.
  And can provide implementations of service APIs for other components to use.
  All components are equal and can communicate with all other components.
  Service implementations can be found by name via a registry service handle
  which is passed to the component initialization function.
  There can be multiple service API implementations for a single service API.
  One of them is the default implementation.
  Service API are stateless by definition. If they need to handle state or
  object instances they need to do so by using factory methods and instance
  handles.

  To ease up transition to the component model the current server
  functionality (server proper and plugins) is contained within
  a dedicated built in server component. The server component currently
  contains all of the functionality provided by the server and
  classical server plugins.

  More components can be installed via the "INSTALL COMPONENT" SQL command.

  The component infrastructure is designed as a replacement for the classical
  MySQL plugin system as it does not suffer from some of the limitations of it
  and provides better isolation for the component code.

  See @subpage PAGE_COMPONENTS.

  Plugins and Services
  --------------------

  As of MySQL 5.1 the server functionality can be extended through
  installing (dynamically or statically linked) extra code modules
  called plugins.

  The server defines a set of well known plugin APIs that the modules
  can implement.

  To allow plugins to reuse server code the server exposes a pre-defined
  set of functions to plugins called plugin services.

  See the following for more details:
  - @subpage page_ext_plugins
  - @subpage page_ext_plugin_services


  User Defined Functions
  ----------------------

  Native code user defined functions can be added to MySQL server using
  the CREATE FUNCTION ... SONAME syntax.

  These can co-exit with @ref page_ext_plugins or reside in their own
  separate binaries.

  To learn how to create these user defined functions see @subpage page_ext_udf
*/


/**
  @page PAGE_CLIENT_TOOLS Client tools

  See mysqldump.cc mysql.cc
*/


/**
  @page PAGE_TESTING_TOOLS Testing Tools

  - @subpage PAGE_MYSQL_TEST_RUN
*/


/**
  @page PAGE_SQL_Optimizer SQL Optimizer

  The task of query optimizer is to determine the most efficient means for
  executing queries. The query optimizer consists of the following
  sub-modules:

  - @ref Query_Resolver
  - @ref Query_Optimizer
  - @ref Query_Planner
  - @ref Query_Executor

  @subpage PAGE_OPT_TRACE

  Additional articles about the query optimizer:

  - @ref PAGE_OPT_TRACE
  - @ref AGGREGATE_CHECKS
*/
/* clang-format on */

#include "sql/mysqld.h"

#include "my_config.h"

#include "binlog_event.h"
#include "control_events.h"
#include "errmsg.h"  // init_client_errs
#include "ft_global.h"
#include "keycache.h"  // KEY_CACHE
#include "m_string.h"
#include "migrate_keyring.h"  // Migrate_keyring
#include "my_alloc.h"
#include "my_base.h"
#include "my_bitmap.h"  // MY_BITMAP
#include "my_command.h"
#include "my_dbug.h"
#include "my_default.h"  // print_defaults
#include "my_dir.h"
#include "my_loglevel.h"
#include "my_macros.h"
#include "my_shm_defaults.h"  // IWYU pragma: keep
#include "my_stacktrace.h"    // my_set_exception_pointers
#include "my_thread_local.h"
#include "my_time.h"
#include "my_timer.h"  // my_timer_initialize
#include "myisam.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/plugin.h"
#include "mysql/plugin_audit.h"
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/mysql_rwlock.h"
#include "mysql/psi/mysql_socket.h"
#include "mysql/psi/mysql_stage.h"
#include "mysql/psi/mysql_statement.h"
#include "mysql/psi/mysql_thread.h"
#include "mysql/psi/psi_base.h"
#include "mysql/psi/psi_cond.h"
#include "mysql/psi/psi_data_lock.h"
#include "mysql/psi/psi_error.h"
#include "mysql/psi/psi_file.h"
#include "mysql/psi/psi_idle.h"
#include "mysql/psi/psi_mdl.h"
#include "mysql/psi/psi_memory.h"
#include "mysql/psi/psi_mutex.h"
#include "mysql/psi/psi_rwlock.h"
#include "mysql/psi/psi_socket.h"
#include "mysql/psi/psi_stage.h"
#include "mysql/psi/psi_statement.h"
#include "mysql/psi/psi_system.h"
#include "mysql/psi/psi_table.h"
#include "mysql/psi/psi_thread.h"
#include "mysql/psi/psi_transaction.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/thread_type.h"
#include "mysql_com.h"
#include "mysql_time.h"
#include "mysql_version.h"
#include "mysqld_error.h"
#include "mysys_err.h"  // EXIT_OUT_OF_MEMORY
#include "pfs_thread_provider.h"
#include "print_version.h"
#ifdef _WIN32
#include <shellapi.h>
#endif
#include "sql/auth/auth_common.h"         // grant_init
#include "sql/auth/sql_authentication.h"  // init_rsa_keys
#include "sql/auth/sql_security_ctx.h"
#include "sql/auto_thd.h"   // Auto_THD
#include "sql/binlog.h"     // mysql_bin_log
#include "sql/bootstrap.h"  // bootstrap
#include "sql/check_stack.h"
#include "sql/conn_handler/connection_acceptor.h"  // Connection_acceptor
#include "sql/conn_handler/connection_handler_impl.h"  // Per_thread_connection_handler
#include "sql/conn_handler/connection_handler_manager.h"  // Connection_handler_manager
#include "sql/conn_handler/socket_connection.h"  // stmt_info_new_packet
#include "sql/current_thd.h"                     // current_thd
#include "sql/dd/cache/dictionary_client.h"
#include "sql/debug_sync.h"  // debug_sync_end
#include "sql/derror.h"
#include "sql/event_data_objects.h"  // init_scheduler_psi_keys
#include "sql/events.h"              // Events
#include "sql/handler.h"
#include "sql/hostname.h"  // hostname_cache_init
#include "sql/init.h"      // unireg_init
#include "sql/item.h"
#include "sql/item_cmpfunc.h"  // Arg_comparator
#include "sql/item_create.h"
#include "sql/item_func.h"
#include "sql/item_strfunc.h"  // Item_func_uuid
#include "sql/keycaches.h"     // get_or_create_key_cache
#include "sql/log.h"
#include "sql/log_event.h"  // Rows_log_event
#include "sql/log_resource.h"
#include "sql/mdl.h"
#include "sql/my_decimal.h"
#include "sql/mysqld_daemon.h"
#include "sql/mysqld_thd_manager.h"              // Global_THD_manager
#include "sql/opt_costconstantcache.h"           // delete_optimizer_cost_module
#include "sql/opt_range.h"                       // range_optimizer_init
#include "sql/options_mysqld.h"                  // OPT_THREAD_CACHE_SIZE
#include "sql/partitioning/partition_handler.h"  // partitioning_init
#include "sql/persisted_variable.h"              // Persisted_variables_cache
#include "sql/plugin_table.h"
#include "sql/protocol.h"
#include "sql/psi_memory_key.h"  // key_memory_MYSQL_RELAY_LOG_index
#include "sql/query_options.h"
#include "sql/replication.h"                        // thd_enter_cond
#include "sql/resourcegroups/resource_group_mgr.h"  // init, post_init
#ifdef _WIN32
#include "sql/restart_monitor_win.h"
#endif
#include "sql/rpl_filter.h"
#include "sql/rpl_gtid.h"
#include "sql/rpl_gtid_persist.h"  // Gtid_table_persistor
#include "sql/rpl_handler.h"       // RUN_HOOK
#include "sql/rpl_info_factory.h"
#include "sql/rpl_info_handler.h"
#include "sql/rpl_injector.h"  // injector
#include "sql/rpl_master.h"    // max_binlog_dump_events
#include "sql/rpl_mi.h"
#include "sql/rpl_msr.h"    // Multisource_info
#include "sql/rpl_rli.h"    // Relay_log_info
#include "sql/rpl_slave.h"  // slave_load_tmpdir
#include "sql/rpl_trx_tracking.h"
#include "sql/sd_notify.h"  // sd_notify_connect
#include "sql/session_tracker.h"
#include "sql/set_var.h"
#include "sql/sp_head.h"    // init_sp_psi_keys
#include "sql/sql_audit.h"  // mysql_audit_general
#include "sql/sql_base.h"
#include "sql/sql_callback.h"  // MUSQL_CALLBACK
#include "sql/sql_class.h"     // THD
#include "sql/sql_connect.h"
#include "sql/sql_error.h"
#include "sql/sql_initialize.h"  // opt_initialize_insecure
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_locale.h"   // MY_LOCALE
#include "sql/sql_manager.h"  // start_handle_manager
#include "sql/sql_parse.h"    // check_stack_overrun
#include "sql/sql_plugin.h"   // opt_plugin_dir
#include "sql/sql_plugin_ref.h"
#include "sql/sql_reload.h"          // handle_reload_request
#include "sql/sql_restart_server.h"  // is_mysqld_managed
#include "sql/sql_servers.h"
#include "sql/sql_show.h"
#include "sql/sql_table.h"  // build_table_filename
#include "sql/sql_test.h"   // mysql_print_status
#include "sql/sql_udf.h"
#include "sql/sys_vars.h"         // fixup_enforce_gtid_consistency_...
#include "sql/sys_vars_shared.h"  // intern_find_sys_var
#include "sql/table_cache.h"      // table_cache_manager
#include "sql/tc_log.h"           // tc_log
#include "sql/thd_raii.h"
#include "sql/thr_malloc.h"
#include "sql/transaction.h"
#include "sql/tztime.h"  // Time_zone
#include "sql/xa.h"
#include "sql_common.h"  // mysql_client_plugin_init
#include "sql_string.h"
#include "storage/myisam/ha_myisam.h"  // HA_RECOVER_OFF
#include "storage/perfschema/pfs_services.h"
#include "thr_lock.h"
#include "thr_mutex.h"
#include "typelib.h"
#include "violite.h"

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
#include "storage/perfschema/pfs_server.h"
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

#ifdef _WIN32
#include "sql/conn_handler/named_pipe_connection.h"
#include "sql/conn_handler/shared_memory_connection.h"
#include "sql/named_pipe.h"
#endif

#ifdef MY_MSCRT_DEBUG
#include <crtdbg.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <fenv.h>
#include <limits.h>
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifndef _WIN32
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef _WIN32
#include <crtdbg.h>
#include <process.h>
#endif
#include "unicode/uclean.h"  // u_cleanup()

#include <algorithm>
#include <atomic>
#include <functional>
#include <new>
#include <string>
#include <vector>

#include "../components/mysql_server/log_builtins_filter_imp.h"
#include "../components/mysql_server/server_component.h"
#include "sql/auth/dynamic_privileges_impl.h"
#include "sql/dd/dd.h"                       // dd::shutdown
#include "sql/dd/dd_kill_immunizer.h"        // dd::DD_kill_immunizer
#include "sql/dd/dictionary.h"               // dd::get_dictionary
#include "sql/dd/performance_schema/init.h"  // performance_schema::init
#include "sql/dd/upgrade/upgrade.h"          // dd::upgrade::in_progress
#include "sql/srv_session.h"

using std::max;
using std::min;
using std::vector;

#define mysqld_charset &my_charset_latin1
#define mysqld_default_locale_name "en_US"

#if defined(HAVE_SOLARIS_LARGE_PAGES) && defined(__GNUC__)
extern "C" int getpagesizes(size_t *, int);
extern "C" int memcntl(caddr_t, size_t, int, caddr_t, int, int);
#endif

#ifdef HAVE_FPU_CONTROL_H
#include <fpu_control.h>  // IWYU pragma: keep
#elif defined(__i386__)
#define fpu_control_t unsigned int
#define _FPU_EXTENDED 0x300
#define _FPU_DOUBLE 0x200
#if defined(__GNUC__) || defined(__SUNPRO_CC)
#define _FPU_GETCW(cw) asm volatile("fnstcw %0" : "=m"(*&cw))
#define _FPU_SETCW(cw) asm volatile("fldcw %0" : : "m"(*&cw))
#else
#define _FPU_GETCW(cw) (cw = 0)
#define _FPU_SETCW(cw)
#endif
#endif
inline void setup_fpu() {
#ifdef HAVE_FEDISABLEEXCEPT
  fedisableexcept(FE_ALL_EXCEPT);
#endif

  /* Set FPU rounding mode to "round-to-nearest" */
  fesetround(FE_TONEAREST);

  /*
    x86 (32-bit) requires FPU precision to be explicitly set to 64 bit
    (double precision) for portable results of floating point operations.
    However, there is no need to do so if compiler is using SSE2 for floating
    point, double values will be stored and processed in 64 bits anyway.
  */
#if defined(__i386__) && !defined(__SSE2_MATH__)
#if !defined(_WIN32)
  fpu_control_t cw;
  _FPU_GETCW(cw);
  cw = (cw & ~_FPU_EXTENDED) | _FPU_DOUBLE;
  _FPU_SETCW(cw);
#endif /* _WIN32 && */
#endif /* __i386__ */
}

extern "C" void handle_fatal_signal(int sig);

/* Constants */

#include "welcome_copyright_notice.h"  // ORACLE_WELCOME_COPYRIGHT_NOTICE

const char *show_comp_option_name[] = {"YES", "NO", "DISABLED"};

static const char *tc_heuristic_recover_names[] = {"OFF", "COMMIT", "ROLLBACK",
                                                   NullS};
static TYPELIB tc_heuristic_recover_typelib = {
    array_elements(tc_heuristic_recover_names) - 1, "",
    tc_heuristic_recover_names, NULL};

const char *first_keyword = "first", *binary_keyword = "BINARY";
const char *my_localhost = "localhost";

bool opt_large_files = sizeof(my_off_t) > 4;
static bool opt_autocommit;  ///< for --autocommit command-line option
static get_opt_arg_source source_autocommit;

/*
  Used with --help for detailed option
*/
bool opt_help = false, opt_verbose = false;

arg_cmp_func Arg_comparator::comparator_matrix[5][2] = {
    {&Arg_comparator::compare_string, &Arg_comparator::compare_e_string},
    {&Arg_comparator::compare_real, &Arg_comparator::compare_e_real},
    {&Arg_comparator::compare_int_signed, &Arg_comparator::compare_e_int},
    {&Arg_comparator::compare_row, &Arg_comparator::compare_e_row},
    {&Arg_comparator::compare_decimal, &Arg_comparator::compare_e_decimal}};

PSI_file_key key_file_binlog_cache;
PSI_file_key key_file_binlog_index_cache;

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_LOCK_status;
static PSI_mutex_key key_LOCK_manager;
static PSI_mutex_key key_LOCK_crypt;
static PSI_mutex_key key_LOCK_user_conn;
static PSI_mutex_key key_LOCK_global_system_variables;
static PSI_mutex_key key_LOCK_prepared_stmt_count;
static PSI_mutex_key key_LOCK_sql_slave_skip_counter;
static PSI_mutex_key key_LOCK_slave_net_timeout;
static PSI_mutex_key key_LOCK_uuid_generator;
static PSI_mutex_key key_LOCK_error_messages;
static PSI_mutex_key key_LOCK_default_password_lifetime;
static PSI_mutex_key key_LOCK_mandatory_roles;
static PSI_mutex_key key_LOCK_password_history;
static PSI_mutex_key key_LOCK_password_reuse_interval;
static PSI_mutex_key key_LOCK_sql_rand;
static PSI_mutex_key key_LOCK_log_throttle_qni;
static PSI_mutex_key key_LOCK_reset_gtid_table;
static PSI_mutex_key key_LOCK_offline_mode;
static PSI_mutex_key key_LOCK_compress_gtid_table;
static PSI_mutex_key key_LOCK_collect_instance_log;
static PSI_mutex_key key_BINLOG_LOCK_commit;
static PSI_mutex_key key_BINLOG_LOCK_commit_queue;
static PSI_mutex_key key_BINLOG_LOCK_done;
static PSI_mutex_key key_BINLOG_LOCK_flush_queue;
static PSI_mutex_key key_BINLOG_LOCK_index;
static PSI_mutex_key key_BINLOG_LOCK_log;
static PSI_mutex_key key_BINLOG_LOCK_binlog_end_pos;
static PSI_mutex_key key_BINLOG_LOCK_sync;
static PSI_mutex_key key_BINLOG_LOCK_sync_queue;
static PSI_mutex_key key_BINLOG_LOCK_xids;
static PSI_rwlock_key key_rwlock_global_sid_lock;
static PSI_rwlock_key key_rwlock_gtid_mode_lock;
static PSI_rwlock_key key_rwlock_LOCK_system_variables_hash;
static PSI_rwlock_key key_rwlock_LOCK_sys_init_connect;
static PSI_rwlock_key key_rwlock_LOCK_sys_init_slave;
static PSI_cond_key key_BINLOG_COND_done;
static PSI_cond_key key_BINLOG_update_cond;
static PSI_cond_key key_BINLOG_prep_xids_cond;
static PSI_cond_key key_COND_manager;
static PSI_cond_key key_COND_compress_gtid_table;
static PSI_thread_key key_thread_signal_hand;
static PSI_thread_key key_thread_main;
static PSI_file_key key_file_casetest;
static PSI_file_key key_file_pid;
#if defined(_WIN32)
static PSI_thread_key key_thread_handle_con_namedpipes;
static PSI_thread_key key_thread_handle_con_sharedmem;
static PSI_thread_key key_thread_handle_con_sockets;
static PSI_mutex_key key_LOCK_handler_count;
static PSI_cond_key key_COND_handler_count;
static PSI_thread_key key_thread_handle_shutdown_restart;
#else
static PSI_mutex_key key_LOCK_socket_listener_active;
static PSI_cond_key key_COND_socket_listener_active;
static PSI_mutex_key key_LOCK_start_signal_handler;
static PSI_cond_key key_COND_start_signal_handler;
#endif  // _WIN32
static PSI_mutex_key key_LOCK_server_started;
static PSI_cond_key key_COND_server_started;
static PSI_mutex_key key_LOCK_keyring_operations;
#endif /* HAVE_PSI_INTERFACE */

/**
  Statement instrumentation key for replication.
*/
#ifdef HAVE_PSI_STATEMENT_INTERFACE
PSI_statement_info stmt_info_rpl;
#endif

/* the default log output is log tables */
static bool lower_case_table_names_used = 0;
#if !defined(_WIN32)
static bool socket_listener_active = false;
static int pipe_write_fd = -1;
static bool opt_daemonize = 0;
#endif
bool opt_debugging = false;
static bool opt_external_locking = 0, opt_console = 0;
static bool opt_short_log_format = 0;
static char *mysqld_user, *mysqld_chroot;
static char *default_character_set_name;
static char *character_set_filesystem_name;
static char *lc_messages;
static char *lc_time_names_name;
char *my_bind_addr_str;
static char *default_collation_name;
static char *default_collation_name_for_utf8mb4;
char *default_storage_engine;
char *default_tmp_storage_engine;
/**
   Use to mark which engine should be chosen to create internal
   temp table
 */
ulong internal_tmp_disk_storage_engine;
ulonglong temptable_max_ram;
static char compiled_default_collation_name[] = MYSQL_DEFAULT_COLLATION_NAME;
static char compiled_default_collation_name_for_utf8mb4[] =
    "utf8mb4_0900_ai_ci";
static bool binlog_format_used = false;

LEX_STRING opt_init_connect, opt_init_slave;

/* Global variables */

LEX_STRING opt_mandatory_roles;
bool opt_mandatory_roles_cache = false;
bool opt_always_activate_granted_roles = false;
bool opt_bin_log;
bool opt_general_log, opt_slow_log, opt_general_log_raw;
ulonglong log_output_options;
bool opt_log_queries_not_using_indexes = 0;
ulong opt_log_throttle_queries_not_using_indexes = 0;
bool opt_disable_networking = 0, opt_skip_show_db = 0;
bool opt_skip_name_resolve = 0;
bool opt_character_set_client_handshake = 1;
bool server_id_supplied = false;
static bool opt_endinfo;
bool using_udf_functions;
bool locked_in_memory;
bool opt_using_transactions;
ulong opt_tc_log_size;
std::atomic<int32> connection_events_loop_aborted_flag;
static enum_server_operational_state server_operational_state = SERVER_BOOTING;
char *opt_log_error_filter_rules;
char *opt_log_error_services;
bool opt_log_syslog_enable;
char *opt_log_syslog_tag = NULL;
char *opt_keyring_migration_user = NULL;
char *opt_keyring_migration_host = NULL;
char *opt_keyring_migration_password = NULL;
char *opt_keyring_migration_socket = NULL;
char *opt_keyring_migration_source = NULL;
char *opt_keyring_migration_destination = NULL;
ulong opt_keyring_migration_port = 0;
bool migrate_connect_options = 0;
#ifndef _WIN32
bool opt_log_syslog_include_pid;
char *opt_log_syslog_facility;

#else
/*
  Thread handle of shutdown event handler thread.
  It is used as argument during thread join.
*/
my_thread_handle shutdown_restart_thr_handle;
#endif
uint host_cache_size;
ulong log_error_verbosity = 3;  // have a non-zero value during early start-up

#if defined(_WIN32)
ulong slow_start_timeout;
bool opt_no_monitor = false;
#endif

bool opt_no_dd_upgrade = false;
bool opt_initialize = 0;
bool opt_skip_slave_start = 0;  ///< If set, slave is not autostarted
bool opt_enable_named_pipe = 0;
bool opt_local_infile, opt_slave_compressed_protocol;
bool opt_safe_user_create = 0;
bool opt_show_slave_auth_info;
bool opt_log_slave_updates = 0;
char *opt_slave_skip_errors;
bool opt_slave_allow_batching = 0;

/**
  compatibility option:
    - index usage hints (USE INDEX without a FOR clause) behave as in 5.0
*/
bool old_mode;

/*
  Legacy global handlerton. These will be removed (please do not add more).
*/
handlerton *heap_hton;
handlerton *temptable_hton;
handlerton *myisam_hton;
handlerton *innodb_hton;

char *opt_disabled_storage_engines;
uint opt_server_id_bits = 0;
ulong opt_server_id_mask = 0;
bool read_only = 0, opt_readonly = 0;
bool super_read_only = 0, opt_super_readonly = 0;
bool opt_require_secure_transport = 0;
bool relay_log_purge;
bool relay_log_recovery;
bool opt_allow_suspicious_udfs;
char *opt_secure_file_priv;
bool opt_log_slow_admin_statements = 0;
bool opt_log_slow_slave_statements = 0;
bool lower_case_file_system = 0;
bool opt_large_pages = 0;
bool opt_super_large_pages = 0;
bool opt_myisam_use_mmap = 0;
bool offline_mode = 0;
uint opt_large_page_size = 0;
uint default_password_lifetime = 0;

mysql_mutex_t LOCK_default_password_lifetime;
mysql_mutex_t LOCK_mandatory_roles;
mysql_mutex_t LOCK_password_history;
mysql_mutex_t LOCK_password_reuse_interval;

#if defined(ENABLED_DEBUG_SYNC)
MYSQL_PLUGIN_IMPORT uint opt_debug_sync_timeout = 0;
#endif /* defined(ENABLED_DEBUG_SYNC) */
bool opt_old_style_user_limits = 0, trust_function_creators = 0;
bool check_proxy_users = 0, mysql_native_password_proxy_users = 0,
     sha256_password_proxy_users = 0;
/*
  True if there is at least one per-hour limit for some user, so we should
  check them before each query (and possibly reset counters when hour is
  changed). False otherwise.
*/
volatile bool mqh_used = 0;
bool opt_noacl = 0;
bool sp_automatic_privileges = 1;

int32_t opt_regexp_time_limit;
int32_t opt_regexp_stack_limit;

ulong opt_binlog_rows_event_max_size;
ulong binlog_checksum_options;
ulong binlog_row_metadata;
bool opt_master_verify_checksum = 0;
bool opt_slave_sql_verify_checksum = 1;
const char *binlog_format_names[] = {"MIXED", "STATEMENT", "ROW", NullS};
bool binlog_gtid_simple_recovery;
ulong binlog_error_action;
const char *binlog_error_action_list[] = {"IGNORE_ERROR", "ABORT_SERVER",
                                          NullS};
uint32 gtid_executed_compression_period = 0;
bool opt_log_unsafe_statements;

#ifdef HAVE_INITGROUPS
volatile sig_atomic_t calling_initgroups = 0; /**< Used in SIGSEGV handler. */
#endif
const char *timestamp_type_names[] = {"UTC", "SYSTEM", NullS};
ulong opt_log_timestamps;
uint mysqld_port, test_flags, select_errors, ha_open_options;
uint mysqld_port_timeout;
ulong delay_key_write_options;
uint protocol_version;
uint lower_case_table_names;
long tc_heuristic_recover;
ulong back_log, connect_timeout, server_id;
ulong table_cache_size;
ulong table_cache_instances;
ulong table_cache_size_per_instance;
ulong schema_def_size;
ulong stored_program_def_size;
ulong table_def_size;
ulong tablespace_def_size;
ulong what_to_log;
ulong slow_launch_time;
std::atomic<int32> atomic_slave_open_temp_tables{0};
ulong open_files_limit, max_binlog_size, max_relay_log_size;
ulong slave_trans_retries;
uint slave_net_timeout;
ulong slave_exec_mode_options;
ulonglong slave_type_conversions_options;
ulong opt_mts_slave_parallel_workers;
ulonglong opt_mts_pending_jobs_size_max;
ulonglong slave_rows_search_algorithms_options;
bool opt_slave_preserve_commit_order;
#ifndef DBUG_OFF
uint slave_rows_last_search_algorithm_used;
#endif
ulong mts_parallel_option;
ulong binlog_cache_size = 0;
ulonglong max_binlog_cache_size = 0;
ulong slave_max_allowed_packet = 0;
ulong binlog_stmt_cache_size = 0;
int32 opt_binlog_max_flush_queue_time = 0;
ulong opt_binlog_group_commit_sync_delay = 0;
ulong opt_binlog_group_commit_sync_no_delay_count = 0;
ulonglong max_binlog_stmt_cache_size = 0;
ulong refresh_version; /* Increments on each reload */
std::atomic<query_id_t> atomic_global_query_id{1};
ulong aborted_threads;
ulong delayed_insert_timeout, delayed_insert_limit, delayed_queue_size;
ulong delayed_insert_threads, delayed_insert_writes, delayed_rows_in_use;
ulong delayed_insert_errors, flush_time;
ulong specialflag = 0;
ulong binlog_cache_use = 0, binlog_cache_disk_use = 0;
ulong binlog_stmt_cache_use = 0, binlog_stmt_cache_disk_use = 0;
ulong max_connections, max_connect_errors;
ulong rpl_stop_slave_timeout = LONG_TIMEOUT;
bool log_bin_use_v1_row_events = 0;
bool thread_cache_size_specified = false;
bool host_cache_size_specified = false;
bool table_definition_cache_specified = false;
ulong locked_account_connection_count = 0;

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
ulong prepared_stmt_count = 0;
ulong current_pid;
uint sync_binlog_period = 0, sync_relaylog_period = 0,
     sync_relayloginfo_period = 0, sync_masterinfo_period = 0,
     opt_mts_checkpoint_period, opt_mts_checkpoint_group;
ulong expire_logs_days = 0;
ulong binlog_expire_logs_seconds = 0;
/**
  Soft upper limit for number of sp_head objects that can be stored
  in the sp_cache for one connection.
*/
ulong stored_program_cache_size = 0;
/**
  Compatibility option to prevent auto upgrade of old temporals
  during certain ALTER TABLE operations.
*/
bool avoid_temporal_upgrade;

bool persisted_globals_load = true;

bool opt_keyring_operations = true;

const double log_10[] = {
    1e000, 1e001, 1e002, 1e003, 1e004, 1e005, 1e006, 1e007, 1e008, 1e009, 1e010,
    1e011, 1e012, 1e013, 1e014, 1e015, 1e016, 1e017, 1e018, 1e019, 1e020, 1e021,
    1e022, 1e023, 1e024, 1e025, 1e026, 1e027, 1e028, 1e029, 1e030, 1e031, 1e032,
    1e033, 1e034, 1e035, 1e036, 1e037, 1e038, 1e039, 1e040, 1e041, 1e042, 1e043,
    1e044, 1e045, 1e046, 1e047, 1e048, 1e049, 1e050, 1e051, 1e052, 1e053, 1e054,
    1e055, 1e056, 1e057, 1e058, 1e059, 1e060, 1e061, 1e062, 1e063, 1e064, 1e065,
    1e066, 1e067, 1e068, 1e069, 1e070, 1e071, 1e072, 1e073, 1e074, 1e075, 1e076,
    1e077, 1e078, 1e079, 1e080, 1e081, 1e082, 1e083, 1e084, 1e085, 1e086, 1e087,
    1e088, 1e089, 1e090, 1e091, 1e092, 1e093, 1e094, 1e095, 1e096, 1e097, 1e098,
    1e099, 1e100, 1e101, 1e102, 1e103, 1e104, 1e105, 1e106, 1e107, 1e108, 1e109,
    1e110, 1e111, 1e112, 1e113, 1e114, 1e115, 1e116, 1e117, 1e118, 1e119, 1e120,
    1e121, 1e122, 1e123, 1e124, 1e125, 1e126, 1e127, 1e128, 1e129, 1e130, 1e131,
    1e132, 1e133, 1e134, 1e135, 1e136, 1e137, 1e138, 1e139, 1e140, 1e141, 1e142,
    1e143, 1e144, 1e145, 1e146, 1e147, 1e148, 1e149, 1e150, 1e151, 1e152, 1e153,
    1e154, 1e155, 1e156, 1e157, 1e158, 1e159, 1e160, 1e161, 1e162, 1e163, 1e164,
    1e165, 1e166, 1e167, 1e168, 1e169, 1e170, 1e171, 1e172, 1e173, 1e174, 1e175,
    1e176, 1e177, 1e178, 1e179, 1e180, 1e181, 1e182, 1e183, 1e184, 1e185, 1e186,
    1e187, 1e188, 1e189, 1e190, 1e191, 1e192, 1e193, 1e194, 1e195, 1e196, 1e197,
    1e198, 1e199, 1e200, 1e201, 1e202, 1e203, 1e204, 1e205, 1e206, 1e207, 1e208,
    1e209, 1e210, 1e211, 1e212, 1e213, 1e214, 1e215, 1e216, 1e217, 1e218, 1e219,
    1e220, 1e221, 1e222, 1e223, 1e224, 1e225, 1e226, 1e227, 1e228, 1e229, 1e230,
    1e231, 1e232, 1e233, 1e234, 1e235, 1e236, 1e237, 1e238, 1e239, 1e240, 1e241,
    1e242, 1e243, 1e244, 1e245, 1e246, 1e247, 1e248, 1e249, 1e250, 1e251, 1e252,
    1e253, 1e254, 1e255, 1e256, 1e257, 1e258, 1e259, 1e260, 1e261, 1e262, 1e263,
    1e264, 1e265, 1e266, 1e267, 1e268, 1e269, 1e270, 1e271, 1e272, 1e273, 1e274,
    1e275, 1e276, 1e277, 1e278, 1e279, 1e280, 1e281, 1e282, 1e283, 1e284, 1e285,
    1e286, 1e287, 1e288, 1e289, 1e290, 1e291, 1e292, 1e293, 1e294, 1e295, 1e296,
    1e297, 1e298, 1e299, 1e300, 1e301, 1e302, 1e303, 1e304, 1e305, 1e306, 1e307,
    1e308};

/* Index extention. */
const int index_ext_length = 6;
const char *index_ext = ".index";
const int relay_ext_length = 10;
const char *relay_ext = "-relay-bin";
/* True if --log-bin option is used. */
bool log_bin_supplied = false;

time_t server_start_time, flush_status_time;

char server_uuid[UUID_LENGTH + 1];
const char *server_uuid_ptr;
char mysql_home[FN_REFLEN], pidfile_name[FN_REFLEN], system_time_zone[30];
char default_logfile_name[FN_REFLEN];
char default_binlogfile_name[FN_REFLEN];
char default_binlog_index_name[FN_REFLEN + index_ext_length];
char default_relaylogfile_name[FN_REFLEN + relay_ext_length];
char default_relaylog_index_name[FN_REFLEN + relay_ext_length +
                                 index_ext_length];
char *default_tz_name;
static char errorlog_filename_buff[FN_REFLEN];
const char *log_error_dest;
const char *my_share_dir[FN_REFLEN];
char glob_hostname[FN_REFLEN];
char mysql_real_data_home[FN_REFLEN], lc_messages_dir[FN_REFLEN],
    reg_ext[FN_EXTLEN], mysql_charsets_dir[FN_REFLEN], *opt_init_file,
    *opt_tc_log_file;
char *lc_messages_dir_ptr;
char mysql_unpacked_real_data_home[FN_REFLEN];
size_t mysql_unpacked_real_data_home_len;
size_t mysql_data_home_len = 1;
uint reg_ext_length;
char logname_path[FN_REFLEN];
char slow_logname_path[FN_REFLEN];
char secure_file_real_path[FN_REFLEN];
Time_zone *default_tz;
char *mysql_data_home = const_cast<char *>(".");
const char *mysql_real_data_home_ptr = mysql_real_data_home;
char server_version[SERVER_VERSION_LENGTH];
char *mysqld_unix_port, *opt_mysql_tmpdir;

/** name of reference on left expression in rewritten IN subquery */
const char *in_left_expr_name = "<left expr>";

my_decimal decimal_zero;
/** Number of connection errors from internal server errors. */
ulong connection_errors_internal = 0;
/** Number of errors when reading the peer address. */
ulong connection_errors_peer_addr = 0;

/* classes for comparation parsing/processing */
Eq_creator eq_creator;
Ne_creator ne_creator;
Equal_creator equal_creator;
Gt_creator gt_creator;
Lt_creator lt_creator;
Ge_creator ge_creator;
Le_creator le_creator;

Rpl_global_filter rpl_global_filter;
Rpl_filter *binlog_filter;

struct System_variables global_system_variables;
struct System_variables max_system_variables;
struct System_status_var global_status_var;

MY_TMPDIR mysql_tmpdir_list;

CHARSET_INFO *system_charset_info, *files_charset_info;
CHARSET_INFO *national_charset_info, *table_alias_charset;
CHARSET_INFO *character_set_filesystem;
CHARSET_INFO *default_collation_for_utf8mb4;

MY_LOCALE *my_default_lc_messages;
MY_LOCALE *my_default_lc_time_names;

SHOW_COMP_OPTION have_ssl, have_symlink, have_dlopen, have_query_cache;
SHOW_COMP_OPTION have_geometry, have_rtree_keys;
SHOW_COMP_OPTION have_compress;
SHOW_COMP_OPTION have_profiling;
SHOW_COMP_OPTION have_statement_timeout = SHOW_OPTION_DISABLED;

/* Thread specific variables */

thread_local MEM_ROOT **THR_MALLOC = nullptr;

mysql_mutex_t LOCK_status, LOCK_uuid_generator, LOCK_crypt,
    LOCK_global_system_variables, LOCK_user_conn, LOCK_error_messages;
mysql_mutex_t LOCK_sql_rand;

/**
  The below lock protects access to two global server variables:
  max_prepared_stmt_count and prepared_stmt_count. These variables
  set the limit and hold the current total number of prepared statements
  in the server, respectively. As PREPARE/DEALLOCATE rate in a loaded
  server may be fairly high, we need a dedicated lock.
*/
mysql_mutex_t LOCK_prepared_stmt_count;

/*
 The below two locks are introduced as guards (second mutex) for
  the global variables sql_slave_skip_counter and slave_net_timeout
  respectively. See fix_slave_skip_counter/fix_slave_net_timeout
  for more details
*/
mysql_mutex_t LOCK_sql_slave_skip_counter;
mysql_mutex_t LOCK_slave_net_timeout;
mysql_mutex_t LOCK_log_throttle_qni;
mysql_mutex_t LOCK_offline_mode;
mysql_rwlock_t LOCK_sys_init_connect, LOCK_sys_init_slave;
mysql_rwlock_t LOCK_system_variables_hash;
my_thread_handle signal_thread_id;
sigset_t mysqld_signal_mask;
my_thread_attr_t connection_attrib;
mysql_mutex_t LOCK_server_started;
mysql_cond_t COND_server_started;
mysql_mutex_t LOCK_reset_gtid_table;
mysql_mutex_t LOCK_compress_gtid_table;
mysql_cond_t COND_compress_gtid_table;
mysql_mutex_t LOCK_collect_instance_log;
#if !defined(_WIN32)
mysql_mutex_t LOCK_socket_listener_active;
mysql_cond_t COND_socket_listener_active;
mysql_mutex_t LOCK_start_signal_handler;
mysql_cond_t COND_start_signal_handler;
#endif

/*
  The below lock protects access to global server variable
  keyring_operations.
*/
mysql_mutex_t LOCK_keyring_operations;

bool mysqld_server_started = false;

/* replication parameters, if master_host is not NULL, we are a slave */
uint report_port = 0;
ulong master_retry_count = 0;
char *master_info_file;
char *relay_log_info_file, *report_user, *report_password, *report_host;
char *opt_relay_logname = 0, *opt_relaylog_index_name = 0;
/*
  True if the --relay-log-index is set by users from
  config file or command line.
*/
bool opt_relaylog_index_name_supplied = false;
/*
  True if the --relay-log is set by users from
  config file or command line.
*/
bool opt_relay_logname_supplied = false;
/*
  True if --log-slave-updates option is set explicitly
  on command line or configuration file.
*/
bool log_slave_updates_supplied = false;

/*
  True if --slave-preserve-commit-order-supplied option is set explicitly
  on command line or configuration file.
*/
bool slave_preserve_commit_order_supplied = false;
char *opt_general_logname, *opt_slow_logname, *opt_bin_logname;

/*
  True if expire_logs_days and binlog_expire_logs_seconds is set
  explictly.
*/
bool expire_logs_days_supplied = false;
bool binlog_expire_logs_seconds_supplied = false;
/* Static variables */

static bool opt_myisam_log;
static int cleanup_done;
static ulong opt_specialflag;
char *opt_binlog_index_name;
char *mysql_home_ptr, *pidfile_name_ptr;
char *default_auth_plugin;
/**
  Memory for allocating command line arguments, after load_defaults().
*/
static MEM_ROOT argv_alloc{PSI_NOT_INSTRUMENTED, 512};
/** Remaining command line arguments (count), filtered by handle_options().*/
static int remaining_argc;
/** Remaining command line arguments (arguments), filtered by
 * handle_options().*/
static char **remaining_argv;

int orig_argc;
char **orig_argv;
namespace {
FILE *nstdout = nullptr;
char my_progpath[FN_REFLEN];
const char *my_orig_progname = nullptr;

/**
  This variable holds the exit value of the signal handler thread.
*/
std::atomic<int> signal_hand_thr_exit_code(MYSQLD_SUCCESS_EXIT);

/**
  Inspects the program name in argv[0] and substitutes the full path
  of the executable.

  @param argv argument vector (array) for executable.
 */
void substitute_progpath(char **argv) {
  if (test_if_hard_path(argv[0])) return;

#if defined(_WIN32)
  if (GetModuleFileName(NULL, my_progpath, sizeof(my_progpath))) {
    my_orig_progname = argv[0];
    argv[0] = my_progpath;
  }
#else
  /* If the path has a directory component, use my_realpath()
     (implicitly relative to cwd) */
  if (strchr(argv[0], FN_LIBCHAR) != nullptr &&
      !my_realpath(my_progpath, argv[0], MYF(0))) {
    my_orig_progname = argv[0];
    argv[0] = my_progpath;
    return;
  }

  // my_realpath() cannot resolve it, it must be a bare executable
  // name in path
  DBUG_ASSERT(strchr(argv[0], FN_LIBCHAR) == nullptr);

  const char *spbegin = getenv("PATH");
  if (spbegin == nullptr) spbegin = "";
  const char *spend = spbegin + strlen(spbegin);

  while (true) {
    const char *colonend = std::find(spbegin, spend, ':');
    if (colonend == spend) {
      DBUG_ASSERT(false);
      break;
    }

    std::string cand{spbegin, colonend};
    spbegin = colonend + 1;

    cand.append(1, '/');
    cand.append(argv[0]);

    if (my_access(cand.c_str(), X_OK) == 0) {
      if (my_realpath(my_progpath, cand.c_str(), MYF(0))) {
        // Fallback to raw cand
        DBUG_ASSERT(cand.length() < FN_REFLEN);
        std::copy(cand.begin(), cand.end(), my_progpath);
        my_progpath[cand.length()] = '\0';
      }
      my_orig_progname = argv[0];
      argv[0] = my_progpath;
      break;
    }
  }  // while (true)
#endif  // defined(_WIN32)
  if (my_orig_progname == nullptr) {
    LogErr(WARNING_LEVEL, ER_FAILED_TO_GET_ABSOLUTE_PATH, argv[0]);
  }
}
}  // namespace

static Connection_acceptor<Mysqld_socket_listener> *mysqld_socket_acceptor =
    NULL;
#ifdef _WIN32
Connection_acceptor<Named_pipe_listener> *named_pipe_acceptor = NULL;
Connection_acceptor<Shared_mem_listener> *shared_mem_acceptor = NULL;
#endif

Checkable_rwlock *global_sid_lock = NULL;
Sid_map *global_sid_map = NULL;
Gtid_state *gtid_state = NULL;
Gtid_table_persistor *gtid_table_persistor = NULL;

/* cache for persisted variables */
static Persisted_variables_cache persisted_variables_cache;

void set_remaining_args(int argc, char **argv) {
  remaining_argc = argc;
  remaining_argv = argv;
}

int *get_remaining_argc() { return &remaining_argc; }

char ***get_remaining_argv() { return &remaining_argv; }

/*
  Multiple threads of execution use the random state maintained in global
  sql_rand to generate random numbers. sql_rnd_with_mutex use mutex
  LOCK_sql_rand to protect sql_rand across multiple instantiations that use
  sql_rand to generate random numbers.
 */
ulong sql_rnd_with_mutex() {
  mysql_mutex_lock(&LOCK_sql_rand);
  ulong tmp =
      (ulong)(my_rnd(&sql_rand) * 0xffffffff); /* make all bits random */
  mysql_mutex_unlock(&LOCK_sql_rand);
  return tmp;
}

struct System_status_var *get_thd_status_var(THD *thd) {
  return &thd->status_var;
}

static void option_error_reporter(enum loglevel level, const char *format, ...)
    MY_ATTRIBUTE((format(printf, 2, 3)));

static void option_error_reporter(enum loglevel level, const char *format,
                                  ...) {
  va_list args;
  va_start(args, format);

  /*
    Don't print warnings for --loose options during initialize.
  */
  if (level == ERROR_LEVEL || !opt_initialize || (log_error_verbosity > 1)) {
    error_log_printf(level, format, args);
  }
  va_end(args);
}

/**
  Character set and collation error reporter that prints to sql error log.
  @param level          log message level
  @param format         log message format string

  This routine is used to print character set and collation
  warnings and errors inside an already running mysqld server,
  e.g. when a character set or collation is requested for the very first time
  and its initialization does not go well for some reasons.
*/
static void charset_error_reporter(enum loglevel level, const char *format, ...)
    MY_ATTRIBUTE((format(printf, 2, 3)));

static void charset_error_reporter(enum loglevel level, const char *format,
                                   ...) {
  va_list args;
  va_start(args, format);
  error_log_printf(level, format, args);
  va_end(args);
}

struct rand_struct sql_rand;  ///< used by sql_class.cc:THD::THD()

struct passwd *user_info = NULL;
#ifndef _WIN32
static my_thread_t main_thread_id;
#endif  // !_WIN32

/* OS specific variables */

#ifdef _WIN32
static bool mysqld_early_option = false;
static bool windows_service = false;
static bool use_opt_args;
static int opt_argc;
static char **opt_argv;
static char **my_global_argv = nullptr;
static int my_global_argc;

static mysql_mutex_t LOCK_handler_count;
static mysql_cond_t COND_handler_count;
static HANDLE hEventShutdown;
static HANDLE hEventRestart;
char *shared_memory_base_name = default_shared_memory_base_name;
bool opt_enable_shared_memory;
static char shutdown_event_name[40];
static char restart_event_name[40];
static NTService Service;  ///< Service object for WinNT
#endif                     /* _WIN32 */

static bool dynamic_plugins_are_initialized = false;

#ifndef DBUG_OFF
static const char *default_dbug_option;
#endif

bool opt_use_ssl = 1;
char *opt_ssl_ca = NULL, *opt_ssl_capath = NULL, *opt_ssl_cert = NULL,
     *opt_ssl_cipher = NULL, *opt_ssl_key = NULL, *opt_ssl_crl = NULL,
     *opt_ssl_crlpath = NULL, *opt_tls_version = NULL;
ulong opt_ssl_fips_mode = SSL_FIPS_MODE_OFF;

#ifdef HAVE_OPENSSL
struct st_VioSSLFd *ssl_acceptor_fd;
SSL *ssl_acceptor;
#endif /* HAVE_OPENSSL */

/* Function declarations */

static int mysql_init_variables();
static int get_options(int *argc_ptr, char ***argv_ptr);
static void add_terminator(vector<my_option> *options);
extern "C" bool mysqld_get_one_option(int, const struct my_option *, char *);
static void set_server_version(void);
static int init_thread_environment();
static char *get_relative_path(const char *path);
static int fix_paths(void);
static int test_if_case_insensitive(const char *dir_name);
static void end_ssl();
static void delete_dictionary_tablespace();

extern "C" void *signal_hand(void *arg);
static bool pid_file_created = false;
static void usage(void);
static void clean_up_mutexes(void);
static bool create_pid_file();
static void mysqld_exit(int exit_code) MY_ATTRIBUTE((noreturn));
static void delete_pid_file(myf flags);
static void clean_up(bool print_message);
static int handle_early_options();
static void adjust_related_options(ulong *requested_open_files);
static bool read_init_file(char *file_name);
#ifdef HAVE_PSI_INTERFACE
static void init_server_psi_keys();
#endif

/**
  Notify any waiters that the server components have been initialized.
  Used by the signal handler thread and by Cluster.

  @see signal_hand
*/

static void server_components_initialized() {
  mysql_mutex_lock(&LOCK_server_started);
  mysqld_server_started = true;
  mysql_cond_broadcast(&COND_server_started);
  mysql_mutex_unlock(&LOCK_server_started);
}

/**
  Initializes component infrastructure by bootstrapping core component
  subsystem.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
static bool component_infrastructure_init() {
  if (mysql_services_bootstrap(NULL)) {
    LogErr(ERROR_LEVEL, ER_COMPONENTS_INFRASTRUCTURE_BOOTSTRAP);
    return true;
  }
  if (pfs_init_services(&imp_mysql_server_registry_registration)) {
    LogErr(ERROR_LEVEL, ER_PERFSCHEMA_COMPONENTS_INFRASTRUCTURE_BOOTSTRAP);
    return true;
  }
  return false;
}

/**
  This function is used to initialize the mysql_server component services.
  Most of the init functions are dummy functions, to solve the linker issues.
*/
static void server_component_init() {
  mysql_comp_sys_var_services_init();
  /*
    Below are dummy initialization functions. Else linker, is cutting out (as
    library optimization) all the below services code. This is because of
    libsql code is not calling any functions of them.
  */
  mysql_string_services_init();
  mysql_comp_status_var_services_init();
  mysql_comp_system_variable_source_init();
  mysql_backup_lock_service_init();
  mysql_security_context_init();
}

/**
  Initializes MySQL Server component infrastructure part by initialize of
  dynamic loader persistence.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/

static bool mysql_component_infrastructure_init() {
  /* We need a temporary THD during boot */
  Auto_THD thd;
  Disable_autocommit_guard autocommit_guard(thd.thd);
  dd::cache::Dictionary_client::Auto_releaser scope_releaser(
      thd.thd->dd_client());
  if (persistent_dynamic_loader_init(thd.thd)) {
    LogErr(ERROR_LEVEL, ER_COMPONENTS_PERSIST_LOADER_BOOTSTRAP);
    trans_rollback_stmt(thd.thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd.thd);
    return true;
  }
  server_component_init();
  return trans_commit_stmt(thd.thd) || trans_commit(thd.thd);
}

/**
  De-initializes Component infrastructure by de-initialization of the MySQL
  Server services (persistent dynamic loader) followed by de-initailization of
  the core Components infrostructure.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
static bool component_infrastructure_deinit() {
  persistent_dynamic_loader_deinit();
  shutdown_dynamic_loader();

  if (pfs_deinit_services(&imp_mysql_server_registry_registration)) {
    LogErr(ERROR_LEVEL, ER_PERFSCHEMA_COMPONENTS_INFRASTRUCTURE_SHUTDOWN);
    return true;
  }
  if (mysql_services_shutdown()) {
    LogErr(ERROR_LEVEL, ER_COMPONENTS_INFRASTRUCTURE_SHUTDOWN);
    return true;
  }
  return false;
}

/**
  Block and wait until server components have been initialized.
*/

static void server_components_init_wait() {
  mysql_mutex_lock(&LOCK_server_started);
  while (!mysqld_server_started)
    mysql_cond_wait(&COND_server_started, &LOCK_server_started);
  mysql_mutex_unlock(&LOCK_server_started);
}

/****************************************************************************
** Code to end mysqld
****************************************************************************/

/**
  This class implements callback function used by close_connections()
  to set KILL_CONNECTION flag on all thds in thd list.
  If m_kill_dump_thread_flag is not set it kills all other threads
  except dump threads. If this flag is set, it kills dump threads.
*/
class Set_kill_conn : public Do_THD_Impl {
 private:
  int m_dump_thread_count;
  bool m_kill_dump_threads_flag;

 public:
  Set_kill_conn() : m_dump_thread_count(0), m_kill_dump_threads_flag(false) {}

  void set_dump_thread_flag() { m_kill_dump_threads_flag = true; }

  int get_dump_thread_count() const { return m_dump_thread_count; }

  virtual void operator()(THD *killing_thd) {
    DBUG_PRINT("quit", ("Informing thread %u that it's time to die",
                        killing_thd->thread_id()));
    if (!m_kill_dump_threads_flag) {
      // We skip slave threads & scheduler on this first loop through.
      if (killing_thd->slave_thread) return;

      if (killing_thd->get_command() == COM_BINLOG_DUMP ||
          killing_thd->get_command() == COM_BINLOG_DUMP_GTID) {
        ++m_dump_thread_count;
        return;
      }
      DBUG_EXECUTE_IF("Check_dump_thread_is_alive", {
        DBUG_ASSERT(killing_thd->get_command() != COM_BINLOG_DUMP &&
                    killing_thd->get_command() != COM_BINLOG_DUMP_GTID);
      };);
    }
    mysql_mutex_lock(&killing_thd->LOCK_thd_data);

    if (killing_thd->kill_immunizer) {
      /*
        If killing_thd is in kill immune mode (i.e. operation on new DD tables
        is in progress) then just save state_to_set with THD::kill_immunizer
        object.

        While exiting kill immune mode, awake() is called again with the killed
        state saved in THD::kill_immunizer object.
      */
      killing_thd->kill_immunizer->save_killed_state(THD::KILL_CONNECTION);
    } else {
      killing_thd->killed = THD::KILL_CONNECTION;

      MYSQL_CALLBACK(Connection_handler_manager::event_functions,
                     post_kill_notification, (killing_thd));
    }

    if (killing_thd->is_killable && killing_thd->kill_immunizer == NULL) {
      mysql_mutex_lock(&killing_thd->LOCK_current_cond);
      if (killing_thd->current_cond.load()) {
        mysql_mutex_lock(killing_thd->current_mutex);
        mysql_cond_broadcast(killing_thd->current_cond);
        mysql_mutex_unlock(killing_thd->current_mutex);
      }
      mysql_mutex_unlock(&killing_thd->LOCK_current_cond);
    }
    mysql_mutex_unlock(&killing_thd->LOCK_thd_data);
  }
};

/**
  This class implements callback function used by close_connections()
  to close vio connection for all thds in thd list
*/
class Call_close_conn : public Do_THD_Impl {
 public:
  Call_close_conn(bool server_shutdown) : is_server_shutdown(server_shutdown) {}

  virtual void operator()(THD *closing_thd) {
    if (closing_thd->get_protocol()->connection_alive()) {
      LEX_CSTRING main_sctx_user = closing_thd->m_main_security_ctx.user();
      LogErr(WARNING_LEVEL, ER_FORCE_CLOSE_THREAD, my_progname,
             (long)closing_thd->thread_id(),
             (main_sctx_user.length ? main_sctx_user.str : ""));
      /*
        Do not generate MYSQL_AUDIT_CONNECTION_DISCONNECT event, when closing
        thread close sessions. Each session will generate DISCONNECT event by
        itself.
      */
      close_connection(closing_thd, 0, is_server_shutdown, false);
    }
  }

 private:
  bool is_server_shutdown;
};

static void close_connections(void) {
  DBUG_ENTER("close_connections");
  (void)RUN_HOOK(server_state, before_server_shutdown, (NULL));

  Per_thread_connection_handler::kill_blocked_pthreads();

  uint dump_thread_count = 0;
  uint dump_thread_kill_retries = 8;

  // Close listeners.
  if (mysqld_socket_acceptor != NULL) mysqld_socket_acceptor->close_listener();
#ifdef _WIN32
  if (named_pipe_acceptor != NULL) named_pipe_acceptor->close_listener();

  if (shared_mem_acceptor != NULL) shared_mem_acceptor->close_listener();
#endif

  /*
    First signal all threads that it's time to die
    This will give the threads some time to gracefully abort their
    statements and inform their clients that the server is about to die.
  */

  Global_THD_manager *thd_manager = Global_THD_manager::get_instance();
  LogErr(INFORMATION_LEVEL, ER_DEPART_WITH_GRACE,
         static_cast<int>(thd_manager->get_thd_count()));

  Set_kill_conn set_kill_conn;
  thd_manager->do_for_all_thd(&set_kill_conn);
  LogErr(INFORMATION_LEVEL, ER_SHUTTING_DOWN_SLAVE_THREADS);
  end_slave();

  if (set_kill_conn.get_dump_thread_count()) {
    /*
      Replication dump thread should be terminated after the clients are
      terminated. Wait for few more seconds for other sessions to end.
     */
    while (thd_manager->get_thd_count() > dump_thread_count &&
           dump_thread_kill_retries) {
      sleep(1);
      dump_thread_kill_retries--;
    }
    set_kill_conn.set_dump_thread_flag();
    thd_manager->do_for_all_thd(&set_kill_conn);
  }

  // Disable the event scheduler
  Events::stop();

  if (thd_manager->get_thd_count() > 0) sleep(2);  // Give threads time to die

  /*
    Force remaining threads to die by closing the connection to the client
    This will ensure that threads that are waiting for a command from the
    client on a blocking read call are aborted.
  */

  LogErr(INFORMATION_LEVEL, ER_DISCONNECTING_REMAINING_CLIENTS,
         static_cast<int>(thd_manager->get_thd_count()));

  Call_close_conn call_close_conn(true);
  thd_manager->do_for_all_thd(&call_close_conn);

  (void)RUN_HOOK(server_state, after_server_shutdown, (NULL));

  /*
    All threads have now been aborted. Stop event scheduler thread
    after aborting all client connections, otherwise user may
    start/stop event scheduler after Events::deinit() deallocates
    scheduler object(static member in Events class)
  */
  Events::deinit();
  DBUG_PRINT("quit", ("Waiting for threads to die (count=%u)",
                      thd_manager->get_thd_count()));
  thd_manager->wait_till_no_thd();
  /*
    Connection threads might take a little while to go down after removing from
    global thread list. Give it some time.
  */
  Connection_handler_manager::wait_till_no_connection();

  delete_slave_info_objects();
  DBUG_PRINT("quit", ("close_connections thread"));

  DBUG_VOID_RETURN;
}

bool signal_restart_server() {
  if (!is_mysqld_managed()) {
    my_error(ER_RESTART_SERVER_FAILED, MYF(0),
             "mysqld is not managed by supervisor process");
    return true;
  }

#ifdef _WIN32
  if (!SetEvent(hEventRestart)) {
    sql_print_error("Got error: %ld from SetEvent", GetLastError());
    my_error(ER_RESTART_SERVER_FAILED, MYF(0), "Internal operation failure");
    return true;
  }
#else

  if (pthread_kill(signal_thread_id.thread, SIGUSR2)) {
    DBUG_PRINT("error", ("Got error %d from pthread_kill", errno));
    my_error(ER_RESTART_SERVER_FAILED, MYF(0), "Internal operation failure");
    return true;
  }
#endif
  return false;
}

void kill_mysql(void) {
  DBUG_ENTER("kill_mysql");

#if defined(_WIN32)
  {
    if (!SetEvent(hEventShutdown)) {
      DBUG_PRINT("error", ("Got error: %ld from SetEvent", GetLastError()));
    }
    /*
      or:
      HANDLE hEvent=OpenEvent(0, false, "MySqlShutdown");
      SetEvent(hEventShutdown);
      CloseHandle(hEvent);
    */
  }
#else
  if (pthread_kill(signal_thread_id.thread, SIGTERM)) {
    DBUG_PRINT("error", ("Got error %d from pthread_kill",
                         errno)); /* purecov: inspected */
  }
#endif
  DBUG_PRINT("quit", ("After pthread_kill"));
  DBUG_VOID_RETURN;
}

static void unireg_abort(int exit_code) {
  DBUG_ENTER("unireg_abort");

  if (errno) {
    sysd::notify("ERRNO=", errno, "\n");
  }

  // At this point it does not make sense to buffer more messages.
  // Just flush what we have and write directly to stderr.
  flush_error_log_messages();

  if (opt_help) usage();

  bool daemon_launcher_quiet = (IF_WIN(false, opt_daemonize) &&
                                !mysqld::runtime::is_daemon() && !opt_help);

  if (!daemon_launcher_quiet && exit_code) LogErr(ERROR_LEVEL, ER_ABORTING);

  mysql_audit_notify(MYSQL_AUDIT_SERVER_SHUTDOWN_SHUTDOWN,
                     MYSQL_AUDIT_SERVER_SHUTDOWN_REASON_ABORT, exit_code);
#ifndef _WIN32
  if (signal_thread_id.thread != 0) {
    // Make sure the signal thread isn't blocked when we are trying to exit.
    server_components_initialized();

    pthread_kill(signal_thread_id.thread, SIGTERM);
    my_thread_join(&signal_thread_id, NULL);
  }
  signal_thread_id.thread = 0;

  if (mysqld::runtime::is_daemon()) {
    mysqld::runtime::signal_parent(pipe_write_fd, 0);
  }
#endif
  clean_up(!opt_help && !daemon_launcher_quiet &&
           (exit_code || !opt_initialize)); /* purecov: inspected */
  DBUG_PRINT("quit", ("done with cleanup in unireg_abort"));
  mysqld_exit(exit_code);
}

void clean_up_mysqld_mutexes() { clean_up_mutexes(); }

static void mysqld_exit(int exit_code) {
  DBUG_ASSERT(
      (exit_code >= MYSQLD_SUCCESS_EXIT && exit_code <= MYSQLD_ABORT_EXIT) ||
      exit_code == MYSQLD_RESTART_EXIT);
  mysql_audit_finalize();
  Srv_session::module_deinit();
  delete_optimizer_cost_module();
  clean_up_mutexes();
  my_end(opt_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
  destroy_error_log();
#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  shutdown_performance_schema();
#endif

#if defined(_WIN32)
  if (hEventShutdown) CloseHandle(hEventShutdown);
  close_service_status_pipe_in_mysqld();
#endif  // _WIN32

  exit(exit_code); /* purecov: inspected */
}

/**
   GTID cleanup destroys objects and reset their pointer.
   Function is reentrant.
*/
void gtid_server_cleanup() {
  if (gtid_state != NULL) {
    delete gtid_state;
    gtid_state = NULL;
  }
  if (global_sid_map != NULL) {
    delete global_sid_map;
    global_sid_map = NULL;
  }
  if (global_sid_lock != NULL) {
    delete global_sid_lock;
    global_sid_lock = NULL;
  }
  if (gtid_table_persistor != NULL) {
    delete gtid_table_persistor;
    gtid_table_persistor = NULL;
  }
  if (gtid_mode_lock) {
    delete gtid_mode_lock;
    gtid_mode_lock = NULL;
  }
}

/**
   GTID initialization.

   @return true if allocation does not succeed
           false if OK
*/
bool gtid_server_init() {
  bool res = (!(global_sid_lock = new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
                    key_rwlock_global_sid_lock
#endif
                    )) ||
              !(gtid_mode_lock = new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
                    key_rwlock_gtid_mode_lock
#endif
                    )) ||
              !(global_sid_map = new Sid_map(global_sid_lock)) ||
              !(gtid_state = new Gtid_state(global_sid_lock, global_sid_map)) ||
              !(gtid_table_persistor = new Gtid_table_persistor()));

  gtid_mode_counter = 1;

  if (res) {
    gtid_server_cleanup();
  }
  return res;
}

// Free connection acceptors
static void free_connection_acceptors() {
  delete mysqld_socket_acceptor;
  mysqld_socket_acceptor = NULL;

#ifdef _WIN32
  delete named_pipe_acceptor;
  named_pipe_acceptor = NULL;
  delete shared_mem_acceptor;
  shared_mem_acceptor = NULL;
#endif
}

static void clean_up(bool print_message) {
  DBUG_PRINT("exit", ("clean_up"));
  if (cleanup_done++) return; /* purecov: inspected */

  ha_pre_dd_shutdown();
  dd::shutdown();

  Events::deinit();
  stop_handle_manager();

  memcached_shutdown();

  /*
    make sure that handlers finish up
    what they have that is dependent on the binlog
  */
  if (print_message && ((!opt_help) || (opt_verbose)))
    LogErr(INFORMATION_LEVEL, ER_BINLOG_END);
  ha_binlog_end(current_thd);

  injector::free_instance();
  mysql_bin_log.cleanup();

  if (use_slave_mask) bitmap_free(&slave_error_mask);
  my_tz_free();
  servers_free(true);
  acl_free(true);
  grant_free();
  hostname_cache_free();
  range_optimizer_free();
  item_func_sleep_free();
  lex_free(); /* Free some memory */
  item_create_cleanup();
  if (!opt_noacl) udf_unload_udfs();
  table_def_start_shutdown();
  plugin_shutdown();
  gtid_server_cleanup();  // after plugin_shutdown
  delete_optimizer_cost_module();
  ha_end();
  if (tc_log) {
    tc_log->close();
    tc_log = NULL;
  }

  if (dd::upgrade_57::in_progress()) delete_dictionary_tablespace();

  delegates_destroy();
  transaction_cache_free();
  table_def_free();
  mdl_destroy();
  key_caches.delete_elements();
  multi_keycache_free();
  query_logger.cleanup();
  my_free_open_file_info();
  free_tmpdir(&mysql_tmpdir_list);
  my_free(opt_bin_logname);
  free_max_user_conn();
  end_slave_list();
  delete binlog_filter;
  rpl_channel_filters.clean_up();
  end_ssl();
  vio_end();
  u_cleanup();
#if defined(ENABLED_DEBUG_SYNC)
  /* End the debug sync facility. See debug_sync.cc. */
  debug_sync_end();
#endif /* defined(ENABLED_DEBUG_SYNC) */

  delete_pid_file(MYF(0));

  if (print_message && my_default_lc_messages && server_start_time)
    LogErr(SYSTEM_LEVEL, ER_SERVER_SHUTDOWN_COMPLETE, my_progname,
           server_version, MYSQL_COMPILATION_COMMENT);
  cleanup_errmsgs();

  free_connection_acceptors();
  Connection_handler_manager::destroy_instance();

  if (!opt_help && !opt_initialize)
    resourcegroups::Resource_group_mgr::destroy_instance();
  mysql_client_plugin_deinit();
  finish_client_errs();
  deinit_errmessage();  // finish server errs
  DBUG_PRINT("quit", ("Error messages freed"));

  Global_THD_manager::destroy_instance();

  my_free(const_cast<char *>(log_bin_basename));
  my_free(const_cast<char *>(log_bin_index));
  my_free(const_cast<char *>(relay_log_basename));
  my_free(const_cast<char *>(relay_log_index));
  free_list(opt_early_plugin_load_list_ptr);
  free_list(opt_plugin_load_list_ptr);

  /*
    Is this the best place for components deinit? It may be changed when new
    dependencies are discovered, possibly being divided into separate points
    where all dependencies are still ok.
  */
  log_builtins_error_stack("log_filter_internal; log_sink_internal", false);
#ifdef HAVE_PSI_THREAD_INTERFACE
  if (!opt_help && !opt_initialize) {
    unregister_pfs_notification_service();
    unregister_pfs_resource_group_service();
  }
#endif
  component_infrastructure_deinit();
  /*
    component unregister_variable() api depends on system_variable_hash.
    component_infrastructure_deinit() interns calls the deinit funtion
    of components which are loaded, and the deinit functions can have
    the component system unregister_ variable()  api's, hence we need
    to call the sys_var_end() after component_infrastructure_deinit()
  */
  sys_var_end();
  free_status_vars();

  if (have_statement_timeout == SHOW_OPTION_YES) my_timer_deinitialize();

  have_statement_timeout = SHOW_OPTION_DISABLED;

  persisted_variables_cache.cleanup();

  udf_deinit_globals();
  /*
    The following lines may never be executed as the main thread may have
    killed us
  */
  DBUG_PRINT("quit", ("done with cleanup"));
} /* clean_up */

static void clean_up_mutexes() {
  mysql_mutex_destroy(&LOCK_log_throttle_qni);
  mysql_mutex_destroy(&LOCK_status);
  mysql_mutex_destroy(&LOCK_manager);
  mysql_mutex_destroy(&LOCK_crypt);
  mysql_mutex_destroy(&LOCK_user_conn);
  mysql_rwlock_destroy(&LOCK_sys_init_connect);
  mysql_rwlock_destroy(&LOCK_sys_init_slave);
  mysql_mutex_destroy(&LOCK_global_system_variables);
  mysql_rwlock_destroy(&LOCK_system_variables_hash);
  mysql_mutex_destroy(&LOCK_uuid_generator);
  mysql_mutex_destroy(&LOCK_sql_rand);
  mysql_mutex_destroy(&LOCK_prepared_stmt_count);
  mysql_mutex_destroy(&LOCK_sql_slave_skip_counter);
  mysql_mutex_destroy(&LOCK_slave_net_timeout);
  mysql_mutex_destroy(&LOCK_error_messages);
  mysql_mutex_destroy(&LOCK_offline_mode);
  mysql_mutex_destroy(&LOCK_default_password_lifetime);
  mysql_mutex_destroy(&LOCK_mandatory_roles);
  mysql_mutex_destroy(&LOCK_server_started);
  mysql_cond_destroy(&COND_server_started);
  mysql_mutex_destroy(&LOCK_reset_gtid_table);
  mysql_mutex_destroy(&LOCK_compress_gtid_table);
  mysql_cond_destroy(&COND_compress_gtid_table);
  mysql_mutex_destroy(&LOCK_collect_instance_log);
  mysql_mutex_destroy(&LOCK_password_history);
  mysql_mutex_destroy(&LOCK_password_reuse_interval);
  mysql_cond_destroy(&COND_manager);
#ifdef _WIN32
  mysql_cond_destroy(&COND_handler_count);
  mysql_mutex_destroy(&LOCK_handler_count);
#endif
#ifndef _WIN32
  mysql_cond_destroy(&COND_socket_listener_active);
  mysql_mutex_destroy(&LOCK_socket_listener_active);
  mysql_cond_destroy(&COND_start_signal_handler);
  mysql_mutex_destroy(&LOCK_start_signal_handler);
#endif
  mysql_mutex_destroy(&LOCK_keyring_operations);
}

/****************************************************************************
** Init IP and UNIX socket
****************************************************************************/

static void set_ports() {
  char *env;
  if (!mysqld_port &&
      !opt_disable_networking) {  // Get port if not from commandline
    mysqld_port = MYSQL_PORT;

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
    struct servent *serv_ptr;
    if ((serv_ptr = getservbyname("mysql", "tcp")))
      mysqld_port = ntohs((u_short)serv_ptr->s_port); /* purecov: inspected */
#endif
    if ((env = getenv("MYSQL_TCP_PORT")))
      mysqld_port = (uint)atoi(env); /* purecov: inspected */
  }
  if (!mysqld_unix_port) {
#ifdef _WIN32
    mysqld_unix_port = (char *)MYSQL_NAMEDPIPE;
#else
    mysqld_unix_port = (char *)MYSQL_UNIX_ADDR;
#endif
    if ((env = getenv("MYSQL_UNIX_PORT")))
      mysqld_unix_port = env; /* purecov: inspected */
  }
}

#if !defined(_WIN32)
/* Change to run as another user if started with --user */

static struct passwd *check_user(const char *user) {
  struct passwd *tmp_user_info;
  uid_t user_id = geteuid();

  // Don't bother if we aren't superuser
  if (user_id) {
    if (user) {
      /* Don't give a warning, if real user is same as given with --user */
      tmp_user_info = getpwnam(user);
      if ((!tmp_user_info || user_id != tmp_user_info->pw_uid))
        LogErr(WARNING_LEVEL, ER_USER_REQUIRES_ROOT);
    }
    return NULL;
  }
  if (!user) {
    if (!opt_initialize && !opt_help) {
      LogErr(ERROR_LEVEL, ER_REALLY_RUN_AS_ROOT);
      unireg_abort(MYSQLD_ABORT_EXIT);
    }
    return NULL;
  }
  /* purecov: begin tested */
  if (!strcmp(user, "root"))
    return NULL;  // Avoid problem with dynamic libraries

  if (!(tmp_user_info = getpwnam(user))) {
    // Allow a numeric uid to be used
    const char *pos;
    for (pos = user; my_isdigit(mysqld_charset, *pos); pos++)
      ;
    if (*pos)  // Not numeric id
      goto err;
    if (!(tmp_user_info = getpwuid(atoi(user)))) goto err;
  }
  return tmp_user_info;
  /* purecov: end */

err:
  LogErr(ERROR_LEVEL, ER_USER_WHAT_USER, user);
  unireg_abort(MYSQLD_ABORT_EXIT);

  return NULL;
}

static void set_user(const char *user, struct passwd *user_info_arg) {
  /* purecov: begin tested */
  DBUG_ASSERT(user_info_arg != 0);
#ifdef HAVE_INITGROUPS
  /*
    We can get a SIGSEGV when calling initgroups() on some systems when NSS
    is configured to use LDAP and the server is statically linked.  We set
    calling_initgroups as a flag to the SIGSEGV handler that is then used to
    output a specific message to help the user resolve this problem.
  */
  calling_initgroups = 1;
  initgroups((char *)user, user_info_arg->pw_gid);
  calling_initgroups = 0;
#endif
  if (setgid(user_info_arg->pw_gid) == -1) {
    LogErr(ERROR_LEVEL, ER_FAIL_SETGID, strerror(errno));
    unireg_abort(MYSQLD_ABORT_EXIT);
  }
  if (setuid(user_info_arg->pw_uid) == -1) {
    LogErr(ERROR_LEVEL, ER_FAIL_SETUID, strerror(errno));
    unireg_abort(MYSQLD_ABORT_EXIT);
  }

#ifdef HAVE_SYS_PRCTL_H
  if (test_flags & TEST_CORE_ON_SIGNAL) {
    /* inform kernel that process is dumpable */
    (void)prctl(PR_SET_DUMPABLE, 1);
  }
#endif

  /* purecov: end */
}

static void set_effective_user(struct passwd *user_info_arg) {
  DBUG_ASSERT(user_info_arg != 0);
  if (setregid((gid_t)-1, user_info_arg->pw_gid) == -1) {
    LogErr(ERROR_LEVEL, ER_FAIL_SETREGID, strerror(errno));
    unireg_abort(MYSQLD_ABORT_EXIT);
  }
  if (setreuid((uid_t)-1, user_info_arg->pw_uid) == -1) {
    LogErr(ERROR_LEVEL, ER_FAIL_SETREUID, strerror(errno));
    unireg_abort(MYSQLD_ABORT_EXIT);
  }
}

/** Change root user if started with @c --chroot . */
static void set_root(const char *path) {
  if (chroot(path) == -1) {
    LogErr(ERROR_LEVEL, ER_FAIL_CHROOT, strerror(errno));
    unireg_abort(MYSQLD_ABORT_EXIT);
  }
  my_setwd("/", MYF(0));
}
#endif  // !_WIN32

static bool network_init(void) {
  if (opt_initialize) return false;

  set_ports();

#ifdef HAVE_SYS_UN_H
  std::string const unix_sock_name(mysqld_unix_port ? mysqld_unix_port : "");
#else
  std::string const unix_sock_name("");
#endif

  if (!opt_disable_networking || unix_sock_name != "") {
    std::string const bind_addr_str(my_bind_addr_str ? my_bind_addr_str : "");

    Mysqld_socket_listener *mysqld_socket_listener = new (std::nothrow)
        Mysqld_socket_listener(bind_addr_str, mysqld_port, back_log,
                               mysqld_port_timeout, unix_sock_name);
    if (mysqld_socket_listener == NULL) return true;

    mysqld_socket_acceptor = new (std::nothrow)
        Connection_acceptor<Mysqld_socket_listener>(mysqld_socket_listener);
    if (mysqld_socket_acceptor == NULL) {
      delete mysqld_socket_listener;
      mysqld_socket_listener = NULL;
      return true;
    }

    if (mysqld_socket_acceptor->init_connection_acceptor())
      return true;  // mysqld_socket_acceptor would be freed in unireg_abort.

    if (report_port == 0) report_port = mysqld_port;

    if (!opt_disable_networking) DBUG_ASSERT(report_port != 0);
  }
#ifdef _WIN32
  // Create named pipe
  if (opt_enable_named_pipe) {
    std::string pipe_name = mysqld_unix_port ? mysqld_unix_port : "";

    Named_pipe_listener *named_pipe_listener =
        new (std::nothrow) Named_pipe_listener(&pipe_name);
    if (named_pipe_listener == NULL) return true;

    named_pipe_acceptor = new (std::nothrow)
        Connection_acceptor<Named_pipe_listener>(named_pipe_listener);
    if (named_pipe_acceptor == NULL) {
      delete named_pipe_listener;
      named_pipe_listener = NULL;
      return true;
    }

    if (named_pipe_acceptor->init_connection_acceptor())
      return true;  // named_pipe_acceptor would be freed in unireg_abort.
  }

  // Setup shared_memory acceptor
  if (opt_enable_shared_memory) {
    std::string shared_mem_base_name =
        shared_memory_base_name ? shared_memory_base_name : "";

    Shared_mem_listener *shared_mem_listener =
        new (std::nothrow) Shared_mem_listener(&shared_mem_base_name);
    if (shared_mem_listener == NULL) return true;

    shared_mem_acceptor = new (std::nothrow)
        Connection_acceptor<Shared_mem_listener>(shared_mem_listener);
    if (shared_mem_acceptor == NULL) {
      delete shared_mem_listener;
      shared_mem_listener = NULL;
      return true;
    }

    if (shared_mem_acceptor->init_connection_acceptor())
      return true;  // shared_mem_acceptor would be freed in unireg_abort.
  }
#endif  // _WIN32
  return false;
}

#ifdef _WIN32
static uint handler_count = 0;

static inline void decrement_handler_count() {
  mysql_mutex_lock(&LOCK_handler_count);
  handler_count--;
  mysql_cond_signal(&COND_handler_count);
  mysql_mutex_unlock(&LOCK_handler_count);
}

extern "C" void *socket_conn_event_handler(void *arg) {
  my_thread_init();

  Connection_acceptor<Mysqld_socket_listener> *conn_acceptor =
      static_cast<Connection_acceptor<Mysqld_socket_listener> *>(arg);
  conn_acceptor->connection_event_loop();

  decrement_handler_count();
  my_thread_end();
  return 0;
}

extern "C" void *named_pipe_conn_event_handler(void *arg) {
  my_thread_init();

  Connection_acceptor<Named_pipe_listener> *conn_acceptor =
      static_cast<Connection_acceptor<Named_pipe_listener> *>(arg);
  conn_acceptor->connection_event_loop();

  decrement_handler_count();
  my_thread_end();
  return 0;
}

extern "C" void *shared_mem_conn_event_handler(void *arg) {
  my_thread_init();

  Connection_acceptor<Shared_mem_listener> *conn_acceptor =
      static_cast<Connection_acceptor<Shared_mem_listener> *>(arg);
  conn_acceptor->connection_event_loop();

  decrement_handler_count();
  my_thread_end();
  return 0;
}

void setup_conn_event_handler_threads() {
  my_thread_handle hThread;

  DBUG_ENTER("handle_connections_methods");

  if ((!have_tcpip || opt_disable_networking) && !opt_enable_shared_memory &&
      !opt_enable_named_pipe) {
    LogErr(ERROR_LEVEL, ER_WIN_LISTEN_BUT_HOW);
    unireg_abort(MYSQLD_ABORT_EXIT);  // Will not return
  }

  mysql_mutex_lock(&LOCK_handler_count);
  handler_count = 0;

  if (opt_enable_named_pipe) {
    int error = mysql_thread_create(
        key_thread_handle_con_namedpipes, &hThread, &connection_attrib,
        named_pipe_conn_event_handler, named_pipe_acceptor);
    if (!error)
      handler_count++;
    else
      LogErr(WARNING_LEVEL, ER_CANT_CREATE_NAMED_PIPES_THREAD, error);
  }

  if (have_tcpip && !opt_disable_networking) {
    int error = mysql_thread_create(
        key_thread_handle_con_sockets, &hThread, &connection_attrib,
        socket_conn_event_handler, mysqld_socket_acceptor);
    if (!error)
      handler_count++;
    else
      LogErr(WARNING_LEVEL, ER_CANT_CREATE_TCPIP_THREAD, error);
  }

  if (opt_enable_shared_memory) {
    int error = mysql_thread_create(
        key_thread_handle_con_sharedmem, &hThread, &connection_attrib,
        shared_mem_conn_event_handler, shared_mem_acceptor);
    if (!error)
      handler_count++;
    else
      LogErr(WARNING_LEVEL, ER_CANT_CREATE_SHM_THREAD, error);
  }

  // Block until all connection listener threads have exited.
  while (handler_count > 0)
    mysql_cond_wait(&COND_handler_count, &LOCK_handler_count);
  mysql_mutex_unlock(&LOCK_handler_count);
  DBUG_VOID_RETURN;
}

/*
  On Windows, we use native SetConsoleCtrlHandler for handle events like Ctrl-C
  with graceful shutdown.
  Also, we do not use signal(), but SetUnhandledExceptionFilter instead - as it
  provides possibility to pass the exception to just-in-time debugger, collect
  dumps and potentially also the exception and thread context used to output
  callstack.
*/

static BOOL WINAPI console_event_handler(DWORD type) {
  DBUG_ENTER("console_event_handler");
  if (type == CTRL_C_EVENT) {
    /*
      Do not shutdown before startup is finished and shutdown
      thread is initialized. Otherwise there is a race condition
      between main thread doing initialization and CTRL-C thread doing
      cleanup, which can result into crash.
    */
    if (hEventShutdown)
      kill_mysql();
    else
      LogErr(WARNING_LEVEL, ER_NOT_RIGHT_NOW);
    DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}

#ifdef DEBUG_UNHANDLED_EXCEPTION_FILTER
#define DEBUGGER_ATTACH_TIMEOUT 120
/*
  Wait for debugger to attach and break into debugger. If debugger is not
  attached, resume after timeout.
*/
static void wait_for_debugger(int timeout_sec) {
  if (!IsDebuggerPresent()) {
    int i;
    printf("Waiting for debugger to attach, pid=%u\n", GetCurrentProcessId());
    fflush(stdout);
    for (i = 0; i < timeout_sec; i++) {
      Sleep(1000);
      if (IsDebuggerPresent()) {
        /* Break into debugger */
        __debugbreak();
        return;
      }
    }
    printf("pid=%u, debugger not attached after %d seconds, resuming\n",
           GetCurrentProcessId(), timeout_sec);
    fflush(stdout);
  }
}
#endif /* DEBUG_UNHANDLED_EXCEPTION_FILTER */

LONG WINAPI my_unhandler_exception_filter(EXCEPTION_POINTERS *ex_pointers) {
  static BOOL first_time = true;
  if (!first_time) {
    /*
      This routine can be called twice, typically
      when detaching in JIT debugger.
      Return EXCEPTION_EXECUTE_HANDLER to terminate process.
    */
    return EXCEPTION_EXECUTE_HANDLER;
  }
  first_time = false;
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
  __try {
    my_set_exception_pointers(ex_pointers);
    handle_fatal_signal(ex_pointers->ExceptionRecord->ExceptionCode);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    DWORD written;
    const char msg[] = "Got exception in exception handler!\n";
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, sizeof(msg) - 1, &written,
              NULL);
  }
  /*
    Return EXCEPTION_CONTINUE_SEARCH to give JIT debugger
    (drwtsn32 or vsjitdebugger) possibility to attach,
    if JIT debugger is configured.
    Windows Error reporting might generate a dump here.
  */
  return EXCEPTION_CONTINUE_SEARCH;
}

void my_init_signals() {
  if (opt_console) SetConsoleCtrlHandler(console_event_handler, true);

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
  SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS |
               SEM_NOOPENFILEERRORBOX);
  SetUnhandledExceptionFilter(my_unhandler_exception_filter);
}

#else  // !_WIN32

extern "C" {
static void empty_signal_handler(int sig MY_ATTRIBUTE((unused))) {}
}

void my_init_signals() {
  DBUG_ENTER("my_init_signals");
  struct sigaction sa;
  (void)sigemptyset(&sa.sa_mask);

  if (!(test_flags & TEST_NO_STACKTRACE) ||
      (test_flags & TEST_CORE_ON_SIGNAL)) {
#ifdef HAVE_STACKTRACE
    my_init_stacktrace();
#endif

    if (test_flags & TEST_CORE_ON_SIGNAL) {
      // Change limits so that we will get a core file.
      struct rlimit rl;
      rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
      if (setrlimit(RLIMIT_CORE, &rl)) LogErr(WARNING_LEVEL, ER_CORE_VALUES);
    }

    /*
      SA_RESETHAND resets handler action to default when entering handler.
      SA_NODEFER allows receiving the same signal during handler.
      E.g. SIGABRT during our signal handler will dump core (default action).
    */
    sa.sa_flags = SA_RESETHAND | SA_NODEFER;
    sa.sa_handler = handle_fatal_signal;
    // Treat all these as fatal and handle them.
    (void)sigaction(SIGSEGV, &sa, NULL);
    (void)sigaction(SIGABRT, &sa, NULL);
    (void)sigaction(SIGBUS, &sa, NULL);
    (void)sigaction(SIGILL, &sa, NULL);
    (void)sigaction(SIGFPE, &sa, NULL);
  }

  // Ignore SIGPIPE and SIGALRM
  sa.sa_flags = 0;
  sa.sa_handler = SIG_IGN;
  (void)sigaction(SIGPIPE, &sa, NULL);
  (void)sigaction(SIGALRM, &sa, NULL);

  // SIGUSR1 is used to interrupt the socket listener.
  sa.sa_handler = empty_signal_handler;
  (void)sigaction(SIGUSR1, &sa, NULL);

  // Fix signals if ignored by parents (can happen on Mac OS X).
  sa.sa_handler = SIG_DFL;
  (void)sigaction(SIGTERM, &sa, NULL);
  (void)sigaction(SIGHUP, &sa, NULL);

  (void)sigemptyset(&mysqld_signal_mask);
  /*
    Block SIGQUIT, SIGHUP, SIGTERM and SIGUSR2.
    The signal handler thread does sigwait() on these.
  */
  (void)sigaddset(&mysqld_signal_mask, SIGQUIT);
  (void)sigaddset(&mysqld_signal_mask, SIGHUP);
  (void)sigaddset(&mysqld_signal_mask, SIGTERM);
  (void)sigaddset(&mysqld_signal_mask, SIGTSTP);
  (void)sigaddset(&mysqld_signal_mask, SIGUSR2);
  /*
    Block SIGINT unless debugging to prevent Ctrl+C from causing
    unclean shutdown of the server.
  */
  if (!(test_flags & TEST_SIGINT)) (void)sigaddset(&mysqld_signal_mask, SIGINT);
  pthread_sigmask(SIG_SETMASK, &mysqld_signal_mask, NULL);
  DBUG_VOID_RETURN;
}

static void start_signal_handler() {
  int error;
  my_thread_attr_t thr_attr;
  DBUG_ENTER("start_signal_handler");

  (void)my_thread_attr_init(&thr_attr);
  (void)pthread_attr_setscope(&thr_attr, PTHREAD_SCOPE_SYSTEM);
  (void)my_thread_attr_setdetachstate(&thr_attr, MY_THREAD_CREATE_JOINABLE);

  size_t guardize = 0;
  (void)pthread_attr_getguardsize(&thr_attr, &guardize);
#if defined(__ia64__) || defined(__ia64)
  /*
    Peculiar things with ia64 platforms - it seems we only have half the
    stack size in reality, so we have to double it here
  */
  guardize = my_thread_stack_size;
#endif
  if (0 !=
      my_thread_attr_setstacksize(&thr_attr, my_thread_stack_size + guardize)) {
    DBUG_ASSERT(false);
  }

  /*
    Set main_thread_id so that SIGTERM/SIGQUIT/SIGKILL/SIGUSR2 can interrupt
    the socket listener successfully.
  */
  main_thread_id = my_thread_self();

  mysql_mutex_lock(&LOCK_start_signal_handler);
  if ((error = mysql_thread_create(key_thread_signal_hand, &signal_thread_id,
                                   &thr_attr, signal_hand, 0))) {
    LogErr(ERROR_LEVEL, ER_CANT_CREATE_INTERRUPT_THREAD, error, errno);
    flush_error_log_messages();
    exit(MYSQLD_ABORT_EXIT);
  }
  mysql_cond_wait(&COND_start_signal_handler, &LOCK_start_signal_handler);
  mysql_mutex_unlock(&LOCK_start_signal_handler);

  (void)my_thread_attr_destroy(&thr_attr);
  DBUG_VOID_RETURN;
}

/** This thread handles SIGTERM, SIGQUIT and SIGHUP signals. */
/* ARGSUSED */
extern "C" void *signal_hand(void *arg MY_ATTRIBUTE((unused))) {
  my_thread_init();

  sigset_t set;
  (void)sigemptyset(&set);
  (void)sigaddset(&set, SIGTERM);
  (void)sigaddset(&set, SIGQUIT);
  (void)sigaddset(&set, SIGHUP);
  (void)sigaddset(&set, SIGUSR2);

  /*
    Signal to start_signal_handler that we are ready.
    This works by waiting for start_signal_handler to free mutex,
    after which we signal it that we are ready.
  */
  mysql_mutex_lock(&LOCK_start_signal_handler);
  mysql_cond_broadcast(&COND_start_signal_handler);
  mysql_mutex_unlock(&LOCK_start_signal_handler);

  /*
    Wait until that all server components have been successfully initialized.
    This step is mandatory since signal processing can be done safely only when
    all server components have been initialized.
  */
  server_components_init_wait();
  for (;;) {
    int sig;
#ifdef __APPLE__
    while (sigwait(&set, &sig) == EINTR) {
    }
#else
    siginfo_t sig_info;
    while (sigwaitinfo(&set, &sig_info) == EINTR) {
    }
    sig = sig_info.si_signo;
#endif             // __APPLE__
    if (cleanup_done) {
      my_thread_end();
      my_thread_exit(0);  // Safety
      return NULL;        // Avoid compiler warnings
    }
    switch (sig) {
      case SIGUSR2:
        signal_hand_thr_exit_code = MYSQLD_RESTART_EXIT;
#ifndef __APPLE__  // Mac OS doesn't have sigwaitinfo.
        //  Log a note if mysqld is restarted via kill command.
        if (sig_info.si_pid != getpid()) {
          sql_print_information(
              "Received signal SIGUSR2."
              " Restarting mysqld (Version %s)",
              server_version);
        }
#endif             // __APPLE__
        // fall through
      case SIGTERM:
      case SIGQUIT:
        // Switch to the file log message processing.
        query_logger.set_handlers((log_output_options != LOG_NONE) ? LOG_FILE
                                                                   : LOG_NONE);
        DBUG_PRINT("info",
                   ("Got signal: %d  connection_events_loop_aborted: %d", sig,
                    connection_events_loop_aborted()));
        if (!connection_events_loop_aborted()) {
          // Mark abort for threads.
          set_connection_events_loop_aborted(true);
#ifdef HAVE_PSI_THREAD_INTERFACE
          // Delete the instrumentation for the signal thread.
          PSI_THREAD_CALL(delete_current_thread)();
#endif /* HAVE_PSI_THREAD_INTERFACE */
          /*
            Kill the socket listener.
            The main thread will then set socket_listener_active= false,
            and wait for us to finish all the cleanup below.
          */
          mysql_mutex_lock(&LOCK_socket_listener_active);
          while (socket_listener_active) {
            DBUG_PRINT("info", ("Killing socket listener"));
            if (pthread_kill(main_thread_id, SIGUSR1)) {
              DBUG_ASSERT(false);
              break;
            }
            mysql_cond_wait(&COND_socket_listener_active,
                            &LOCK_socket_listener_active);
          }
          mysql_mutex_unlock(&LOCK_socket_listener_active);

          close_connections();
        }
        my_thread_end();
        my_thread_exit(nullptr);
        return NULL;  // Avoid compiler warnings
        break;
      case SIGHUP:
        if (!connection_events_loop_aborted()) {
          int not_used;
          mysql_print_status();  // Print some debug info
          handle_reload_request(
              NULL,
              (REFRESH_LOG | REFRESH_TABLES | REFRESH_FAST | REFRESH_GRANT |
               REFRESH_THREADS | REFRESH_HOSTS),
              NULL, &not_used);  // Flush logs
          // Reenable query logs after the options were reloaded.
          query_logger.set_handlers(log_output_options);
        }
        break;
      default:
        break; /* purecov: tested */
    }
  }
  return NULL; /* purecov: deadcode */
}

#endif  // !_WIN32

/**
  All global error messages are sent here where the first one is stored
  for the client.
*/
/* ARGSUSED */
extern "C" void my_message_sql(uint error, const char *str, myf MyFlags);

void my_message_sql(uint error, const char *str, myf MyFlags) {
  THD *thd = current_thd;
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

  if (error == 0) {
    /* At least, prevent new abuse ... */
    DBUG_ASSERT(strncmp(str, "MyISAM table", 12) == 0);
    error = ER_UNKNOWN_ERROR;
  }

  if (thd) {
    Sql_condition::enum_severity_level level = Sql_condition::SL_ERROR;

    /**
      Reporting an error invokes audit API call that notifies the error
      to the plugin. Audit API that generate the error adds a protection
      (condition handler) that prevents entering infinite recursion, when
      a plugin signals error, when already handling the error.

      handle_condition is normally invoked from within raise_condition,
      but we need to prevent recursion befere notifying error to the plugin.

      Additionaly, handle_condition must be called once during reporting
      an error, so the raise_condition is called depending on the result of
      the handle_condition call.
    */
    bool handle = thd->handle_condition(error, mysql_errno_to_sqlstate(error),
                                        &level, str ? str : ER_DEFAULT(error));
    if (!handle)
      mysql_audit_notify(thd, AUDIT_EVENT(MYSQL_AUDIT_GENERAL_ERROR), error,
                         str, strlen(str));

    if (MyFlags & ME_FATALERROR) thd->is_fatal_error = 1;

    if (!handle) (void)thd->raise_condition(error, NULL, level, str, false);

    /*
      Only error-codes from the client range should be seen here.
      We'll assert this here (rather than in raise_condition) as
      SQL's SIGNAL command calls that as well, and is currently
      allowed to set any error-code. Those values will be handled
      in a uniform way, that is to say, SIGNALing an error-code
      from the error-log range will not result in writing to that
      log to prevent abuse.
      We're bailing after rather than before printing to make the
      culprit easier to track down.)
    */
    DBUG_ASSERT(errno < ER_SERVER_RANGE_START);
  }

  /* When simulating OOM, skip writing to error log to avoid mtr errors */
  DBUG_EXECUTE_IF("simulate_out_of_memory", DBUG_VOID_RETURN;);

  /*
    Caller wishes to send to both the client and the error-log.
    This is legacy behaviour that is no longer legal as errors flagged
    to a client and those sent to the error-log are in different
    numeric ranges now. If you own code that does this, see about
    updating it by splitting it into two calls, one sending status
    to the client, the other sending it to the error-log using
    LogErr() and friends.
  */
  if (MyFlags & ME_ERRORLOG) {
    /*
      We've removed most uses of ME_ERRORLOG in the server.
      This leaves three possible cases:

      - EE_OUTOFMEMORY: Correct to ER_SERVER_OUT_OF_RESOURCES so
                        mysys can remain logger-agnostic.
      - HA_* range:     Correct to catch-all ER_SERVER_HANDLER_ERROR.
      - otherwise:      Flag as using info from the diagnostics area
                        (ER_ERROR_INFO_FROM_DA). This is a failsafe;
                        if your code triggers it, your code is probably
                        wrong.
    */
    if ((error == EE_OUTOFMEMORY) || (error == HA_ERR_OUT_OF_MEM))
      error = ER_SERVER_OUT_OF_RESOURCES;
    else if (error <= HA_ERR_LAST)
      error = ER_SERVER_HANDLER_ERROR;

    if (error < ER_SERVER_RANGE_START)
      LogEvent()
          .type(LOG_TYPE_ERROR)
          .prio(ERROR_LEVEL)
          .errcode(ER_ERROR_INFO_FROM_DA)
          .lookup(ER_ERROR_INFO_FROM_DA, error, str);
    else
      LogEvent()
          .type(LOG_TYPE_ERROR)
          .prio(ERROR_LEVEL)
          .errcode(error)
          .verbatim(str);

    /*
      This is no longer supported behaviour except for the cases
      outlined above, so flag anything else in debug builds!
      (We're bailing after rather than before printing to make the
      culprit easier to track down.)
    */
    DBUG_ASSERT((error == ER_FEATURE_NOT_AVAILABLE) ||
                (error >= ER_SERVER_RANGE_START));
  }

  /*
    Caller wishes to send to client, but none is attached, so we send
    to error-log instead.
  */
  else if (!thd) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .subsys(LOG_SUBSYSTEM_TAG)
        .prio(ERROR_LEVEL)
        .errcode((error < ER_SERVER_RANGE_START)
                     ? ER_SERVER_NO_SESSION_TO_SEND_TO
                     : error)
        .lookup(ER_SERVER_NO_SESSION_TO_SEND_TO, error, str);
  }

  DBUG_VOID_RETURN;
}

extern "C" void *my_str_malloc_mysqld(size_t size);
extern "C" void my_str_free_mysqld(void *ptr);
extern "C" void *my_str_realloc_mysqld(void *ptr, size_t size);

void *my_str_malloc_mysqld(size_t size) {
  return my_malloc(key_memory_my_str_malloc, size, MYF(MY_FAE));
}

void my_str_free_mysqld(void *ptr) { my_free(ptr); }

void *my_str_realloc_mysqld(void *ptr, size_t size) {
  return my_realloc(key_memory_my_str_malloc, ptr, size, MYF(MY_FAE));
}

const char *load_default_groups[] = {
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
    "mysql_cluster",
#endif
    "mysqld",        "server", MYSQL_BASE_VERSION, 0, 0};

#if defined(_WIN32)
static const int load_default_groups_sz =
    sizeof(load_default_groups) / sizeof(load_default_groups[0]);
#endif

/**
  This function is used to check for stack overrun for pathological
  cases of regular expressions and 'like' expressions.
  The call to current_thd is quite expensive, so we try to avoid it
  for the normal cases.
  The size of each stack frame for the wildcmp() routines is ~128 bytes,
  so checking *every* recursive call is not necessary.
 */
extern "C" {
static int check_enough_stack_size(int recurse_level) {
  uchar stack_top;
  if (recurse_level % 16 != 0) return 0;

  THD *my_thd = current_thd;
  if (my_thd != NULL)
    return check_stack_overrun(my_thd, STACK_MIN_SIZE * 2, &stack_top);
  return 0;
}
}  // extern "C"

/**
  Initialize one of the global date/time format variables.

  @param format_type    What kind of format should be supported
  @param [in,out] format Format variable to initialize

  @retval
    0 ok
  @retval
    1 error
*/

SHOW_VAR com_status_vars[] = {
    {"admin_commands", (char *)offsetof(System_status_var, com_other),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"assign_to_keycache",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_ASSIGN_TO_KEYCACHE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"alter_db",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_ALTER_DB]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"alter_event",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_ALTER_EVENT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"alter_function",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_ALTER_FUNCTION]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"alter_instance",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_ALTER_INSTANCE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"alter_procedure",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_ALTER_PROCEDURE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"alter_resource_group",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_ALTER_RESOURCE_GROUP]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"alter_server",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_ALTER_SERVER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"alter_table",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_ALTER_TABLE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"alter_tablespace",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_ALTER_TABLESPACE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"alter_user",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_ALTER_USER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"alter_user_default_role",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_ALTER_USER_DEFAULT_ROLE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"analyze",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_ANALYZE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"begin", (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_BEGIN]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"binlog",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_BINLOG_BASE64_EVENT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"call_procedure",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CALL]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"change_db",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CHANGE_DB]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"change_master",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CHANGE_MASTER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"change_repl_filter",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_CHANGE_REPLICATION_FILTER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"check", (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CHECK]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"checksum",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CHECKSUM]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"clone", (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CLONE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"commit",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_COMMIT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"create_db",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CREATE_DB]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"create_event",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CREATE_EVENT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"create_function",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_CREATE_SPFUNCTION]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"create_index",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CREATE_INDEX]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"create_procedure",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_CREATE_PROCEDURE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"create_role",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CREATE_ROLE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"create_server",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CREATE_SERVER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"create_table",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CREATE_TABLE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"create_resource_group",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_CREATE_RESOURCE_GROUP]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"create_trigger",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CREATE_TRIGGER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"create_udf",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_CREATE_FUNCTION]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"create_user",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CREATE_USER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"create_view",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CREATE_VIEW]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"create_spatial_reference_system",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_CREATE_SRS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"dealloc_sql",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_DEALLOCATE_PREPARE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"delete",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DELETE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"delete_multi",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DELETE_MULTI]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"do", (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DO]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"drop_db",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DROP_DB]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"drop_event",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DROP_EVENT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"drop_function",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DROP_FUNCTION]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"drop_index",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DROP_INDEX]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"drop_procedure",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DROP_PROCEDURE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"drop_resource_group",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_DROP_RESOURCE_GROUP]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"drop_role",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DROP_ROLE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"drop_server",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DROP_SERVER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"drop_spatial_reference_system",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DROP_SRS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"drop_table",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DROP_TABLE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"drop_trigger",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DROP_TRIGGER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"drop_user",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DROP_USER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"drop_view",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_DROP_VIEW]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"empty_query",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_EMPTY_QUERY]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"execute_sql",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_EXECUTE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"explain_other",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_EXPLAIN_OTHER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"flush", (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_FLUSH]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"get_diagnostics",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_GET_DIAGNOSTICS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"grant", (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_GRANT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"grant_roles",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_GRANT_ROLE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"ha_close",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_HA_CLOSE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"ha_open",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_HA_OPEN]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"ha_read",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_HA_READ]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"help", (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_HELP]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"import",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_IMPORT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"insert",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_INSERT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"insert_select",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_INSERT_SELECT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"install_component",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_INSTALL_COMPONENT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"install_plugin",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_INSTALL_PLUGIN]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"kill", (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_KILL]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"load", (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_LOAD]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"lock_instance",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_LOCK_INSTANCE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"lock_tables",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_LOCK_TABLES]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"optimize",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_OPTIMIZE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"preload_keys",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_PRELOAD_KEYS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"prepare_sql",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_PREPARE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"purge", (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_PURGE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"purge_before_date",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_PURGE_BEFORE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"release_savepoint",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_RELEASE_SAVEPOINT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"rename_table",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_RENAME_TABLE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"rename_user",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_RENAME_USER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"repair",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_REPAIR]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"replace",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_REPLACE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"replace_select",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_REPLACE_SELECT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"reset", (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_RESET]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"resignal",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_RESIGNAL]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"restart",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_RESTART_SERVER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"revoke",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_REVOKE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"revoke_all",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_REVOKE_ALL]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"revoke_roles",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_REVOKE_ROLE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"rollback",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_ROLLBACK]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"rollback_to_savepoint",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_ROLLBACK_TO_SAVEPOINT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"savepoint",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SAVEPOINT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"select",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SELECT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"set_option",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SET_OPTION]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"set_password",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SET_PASSWORD]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"set_resource_group",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SET_RESOURCE_GROUP]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"set_role",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SET_ROLE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"signal",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SIGNAL]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_binlog_events",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_BINLOG_EVENTS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_binlogs",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_BINLOGS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_charsets",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_CHARSETS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_collations",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_COLLATIONS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_create_db",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_CREATE_DB]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_create_event",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_CREATE_EVENT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_create_func",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_CREATE_FUNC]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_create_proc",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_CREATE_PROC]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_create_table",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_CREATE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_create_trigger",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_CREATE_TRIGGER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_databases",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_DATABASES]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_engine_logs",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_ENGINE_LOGS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_engine_mutex",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_ENGINE_MUTEX]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_engine_status",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_ENGINE_STATUS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_events",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_EVENTS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_errors",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_ERRORS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_fields",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_FIELDS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_function_code",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_FUNC_CODE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_function_status",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_STATUS_FUNC]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_grants",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_GRANTS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_keys",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_KEYS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_master_status",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_MASTER_STAT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_open_tables",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_OPEN_TABLES]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_plugins",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_PLUGINS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_privileges",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_PRIVILEGES]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_procedure_code",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_PROC_CODE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_procedure_status",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_STATUS_PROC]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_processlist",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_PROCESSLIST]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_profile",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_PROFILE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_profiles",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_PROFILES]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_relaylog_events",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_RELAYLOG_EVENTS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_slave_hosts",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_SLAVE_HOSTS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_slave_status",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_SLAVE_STAT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_status",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_STATUS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_storage_engines",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_STORAGE_ENGINES]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_table_status",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_TABLE_STATUS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_tables",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_TABLES]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_triggers",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_TRIGGERS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_variables",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_VARIABLES]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_warnings",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHOW_WARNS]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"show_create_user",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_SHOW_CREATE_USER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"shutdown",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SHUTDOWN]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"slave_start",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SLAVE_START]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"slave_stop",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_SLAVE_STOP]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"group_replication_start",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_START_GROUP_REPLICATION]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"group_replication_stop",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_STOP_GROUP_REPLICATION]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"stmt_execute", (char *)offsetof(System_status_var, com_stmt_execute),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"stmt_close", (char *)offsetof(System_status_var, com_stmt_close),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"stmt_fetch", (char *)offsetof(System_status_var, com_stmt_fetch),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"stmt_prepare", (char *)offsetof(System_status_var, com_stmt_prepare),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"stmt_reset", (char *)offsetof(System_status_var, com_stmt_reset),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"stmt_send_long_data",
     (char *)offsetof(System_status_var, com_stmt_send_long_data),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"truncate",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_TRUNCATE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"uninstall_component",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_UNINSTALL_COMPONENT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"uninstall_plugin",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_UNINSTALL_PLUGIN]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"unlock_instance",
     (char *)offsetof(System_status_var,
                      com_stat[(uint)SQLCOM_UNLOCK_INSTANCE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"unlock_tables",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_UNLOCK_TABLES]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"update",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_UPDATE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"update_multi",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_UPDATE_MULTI]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"xa_commit",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_XA_COMMIT]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"xa_end",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_XA_END]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"xa_prepare",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_XA_PREPARE]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"xa_recover",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_XA_RECOVER]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"xa_rollback",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_XA_ROLLBACK]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {"xa_start",
     (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_XA_START]),
     SHOW_LONG_STATUS, SHOW_SCOPE_ALL},
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_ALL}};

LEX_CSTRING sql_statement_names[(uint)SQLCOM_END + 1];

static void init_sql_statement_names() {
  static LEX_CSTRING empty = {C_STRING_WITH_LEN("")};

  char *first_com = (char *)offsetof(System_status_var, com_stat[0]);
  char *last_com =
      (char *)offsetof(System_status_var, com_stat[(uint)SQLCOM_END]);
  int record_size = (char *)offsetof(System_status_var, com_stat[1]) -
                    (char *)offsetof(System_status_var, com_stat[0]);
  char *ptr;
  uint i;
  uint com_index;

  for (i = 0; i < ((uint)SQLCOM_END + 1); i++) sql_statement_names[i] = empty;

  SHOW_VAR *var = &com_status_vars[0];
  while (var->name != NULL) {
    ptr = var->value;
    if ((first_com <= ptr) && (ptr <= last_com)) {
      com_index = ((int)(ptr - first_com)) / record_size;
      DBUG_ASSERT(com_index < (uint)SQLCOM_END);
      sql_statement_names[com_index].str = var->name;
      /* TODO: Change SHOW_VAR::name to a LEX_STRING, to avoid strlen() */
      sql_statement_names[com_index].length = strlen(var->name);
    }
    var++;
  }

  DBUG_ASSERT(strcmp(sql_statement_names[(uint)SQLCOM_SELECT].str, "select") ==
              0);
  DBUG_ASSERT(strcmp(sql_statement_names[(uint)SQLCOM_SIGNAL].str, "signal") ==
              0);

  sql_statement_names[(uint)SQLCOM_END].str = "error";
}

#ifdef HAVE_PSI_STATEMENT_INTERFACE
PSI_statement_info sql_statement_info[(uint)SQLCOM_END + 1];
PSI_statement_info com_statement_info[(uint)COM_END + 1];

/**
  Initialize the command names array.
  Since we do not want to maintain a separate array,
  this is populated from data mined in com_status_vars,
  which already has one name for each command.
*/
static void init_sql_statement_info() {
  uint i;

  for (i = 0; i < ((uint)SQLCOM_END + 1); i++) {
    sql_statement_info[i].m_name = sql_statement_names[i].str;
    sql_statement_info[i].m_flags = 0;
    sql_statement_info[i].m_documentation = PSI_DOCUMENT_ME;
  }

  /* "statement/sql/error" represents broken queries (syntax error). */
  sql_statement_info[(uint)SQLCOM_END].m_name = "error";
  sql_statement_info[(uint)SQLCOM_END].m_flags = 0;
  sql_statement_info[(uint)SQLCOM_END].m_documentation =
      "Invalid SQL queries (syntax error).";
}

static void init_com_statement_info() {
  uint index;

  for (index = 0; index < (uint)COM_END + 1; index++) {
    com_statement_info[index].m_name = command_name[index].str;
    com_statement_info[index].m_flags = 0;
    com_statement_info[index].m_documentation = PSI_DOCUMENT_ME;
  }

  /* "statement/abstract/query" can mutate into "statement/sql/..." */
  com_statement_info[(uint)COM_QUERY].m_flags = PSI_FLAG_MUTABLE;
  com_statement_info[(uint)COM_QUERY].m_documentation =
      "SQL query just received from the network. "
      "At this point, the real statement type is unknown, "
      "the type will be refined after SQL parsing.";
}
#endif

/**
  Create a replication file name or base for file names.

  @param     key Instrumentation key used to track allocations
  @param[in] opt Value of option, or NULL
  @param[in] def Default value if option value is not set.
  @param[in] ext Extension to use for the path

  @returns Pointer to string containing the full file path, or NULL if
  it was not possible to create the path.
 */
static inline const char *rpl_make_log_name(PSI_memory_key key, const char *opt,
                                            const char *def, const char *ext) {
  DBUG_ENTER("rpl_make_log_name");
  DBUG_PRINT("enter", ("opt: %s, def: %s, ext: %s", (opt && opt[0]) ? opt : "",
                       def, ext));
  char buff[FN_REFLEN];
  /*
    opt[0] needs to be checked to make sure opt name is not an empty
    string, incase it is an empty string default name will be considered
  */
  const char *base = (opt && opt[0]) ? opt : def;
  unsigned int options = MY_REPLACE_EXT | MY_UNPACK_FILENAME | MY_SAFE_PATH;

  /* mysql_real_data_home_ptr may be null if no value of datadir has been
     specified through command-line or througha cnf file. If that is the
     case we make mysql_real_data_home_ptr point to mysql_real_data_home
     which, in that case holds the default path for data-dir.
  */

  DBUG_EXECUTE_IF("emulate_empty_datadir_param",
                  { mysql_real_data_home_ptr = NULL; };);

  if (mysql_real_data_home_ptr == NULL)
    mysql_real_data_home_ptr = mysql_real_data_home;

  if (fn_format(buff, base, mysql_real_data_home_ptr, ext, options))
    DBUG_RETURN(my_strdup(key, buff, MYF(0)));
  else
    DBUG_RETURN(NULL);
}

int init_common_variables() {
  umask(((~my_umask) & 0666));
  my_decimal_set_zero(&decimal_zero);  // set decimal_zero constant;
  tzset();                             // Set tzname

  max_system_variables.pseudo_thread_id = (my_thread_id)~0;
  server_start_time = flush_status_time = my_time(0);

  binlog_filter = new Rpl_filter;
  if (!binlog_filter) {
    LogErr(ERROR_LEVEL, ER_RPL_BINLOG_FILTERS_OOM, strerror(errno));
    return 1;
  }

  if (init_thread_environment() || mysql_init_variables()) return 1;

  {
    struct tm tm_tmp;
    localtime_r(&server_start_time, &tm_tmp);
#ifdef _WIN32
    strmake(system_time_zone, _tzname[tm_tmp.tm_isdst != 0 ? 1 : 0],
            sizeof(system_time_zone) - 1);
#else
    strmake(system_time_zone, tzname[tm_tmp.tm_isdst != 0 ? 1 : 0],
            sizeof(system_time_zone) - 1);
#endif
  }
  /*
    We set SYSTEM time zone as reasonable default and
    also for failure of my_tz_init() and bootstrap mode.
    If user explicitly set time zone with --default-time-zone
    option we will change this value in my_tz_init().
  */
  global_system_variables.time_zone = my_tz_SYSTEM;

#ifdef HAVE_PSI_INTERFACE
  /*
    Complete the mysql_bin_log initialization.
    Instrumentation keys are known only after the performance schema
    initialization, and can not be set in the MYSQL_BIN_LOG constructor (called
    before main()).
  */
  mysql_bin_log.set_psi_keys(
      key_BINLOG_LOCK_index, key_BINLOG_LOCK_commit,
      key_BINLOG_LOCK_commit_queue, key_BINLOG_LOCK_done,
      key_BINLOG_LOCK_flush_queue, key_BINLOG_LOCK_log,
      key_BINLOG_LOCK_binlog_end_pos, key_BINLOG_LOCK_sync,
      key_BINLOG_LOCK_sync_queue, key_BINLOG_LOCK_xids, key_BINLOG_COND_done,
      key_BINLOG_update_cond, key_BINLOG_prep_xids_cond, key_file_binlog,
      key_file_binlog_index, key_file_binlog_cache,
      key_file_binlog_index_cache);
#endif

  /*
    Init mutexes for the global MYSQL_BIN_LOG objects.
    As safe_mutex depends on what MY_INIT() does, we can't init the mutexes of
    global MYSQL_BIN_LOGs in their constructors, because then they would be
    inited before MY_INIT(). So we do it here.
  */
  mysql_bin_log.init_pthread_objects();

  /* TODO: remove this when my_time_t is 64 bit compatible */
  if (!IS_TIME_T_VALID_FOR_TIMESTAMP(server_start_time)) {
    LogErr(ERROR_LEVEL, ER_UNSUPPORTED_DATE);
    return 1;
  }

  if (gethostname(glob_hostname, sizeof(glob_hostname)) < 0) {
    strmake(glob_hostname, STRING_WITH_LEN("localhost"));
    LogErr(WARNING_LEVEL, ER_CALL_ME_LOCALHOST, glob_hostname);
    strmake(default_logfile_name, STRING_WITH_LEN("mysql"));
  } else
    strmake(default_logfile_name, glob_hostname,
            sizeof(default_logfile_name) - 5);

  strmake(default_binlogfile_name, STRING_WITH_LEN("binlog"));
  if (opt_initialize || opt_initialize_insecure) {
    /*
      System tables initialization are not binary logged (regardless
      --log-bin option).

      Disable binary log while executing any user script sourced while
      initializing system except if explicitly requested.
    */
    opt_bin_log = false;
  }

  strmake(pidfile_name, default_logfile_name, sizeof(pidfile_name) - 5);
  my_stpcpy(fn_ext(pidfile_name), ".pid");  // Add proper extension

  /*
    The default-storage-engine entry in my_long_options should have a
    non-null default value. It was earlier intialized as
    (longlong)"MyISAM" in my_long_options but this triggered a
    compiler error in the Sun Studio 12 compiler. As a work-around we
    set the def_value member to 0 in my_long_options and initialize it
    to the correct value here.

    From MySQL 5.5 onwards, the default storage engine is InnoDB.
  */
  default_storage_engine = const_cast<char *>("InnoDB");
  default_tmp_storage_engine = default_storage_engine;

  /*
    Add server status variables to the dynamic list of
    status variables that is shown by SHOW STATUS.
    Later, in plugin_register_builtin_and_init_core_se(),
    plugin_register_dynamic_and_init_all() and
    mysql_install_plugin(), new entries could be added
    to that list.
  */
  if (add_status_vars(status_vars)) return 1;  // an error was already reported

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
  static_assert(sizeof(com_status_vars) / sizeof(com_status_vars[0]) - 1 ==
                    SQLCOM_END + 7,
                "");
#endif

  if (get_options(&remaining_argc, &remaining_argv)) return 1;

  /*
    The opt_bin_log can be false (binary log is disabled) only if
    --skip-log-bin/--disable-log-bin is configured or while the
    system is initializing.
  */
  if (!opt_bin_log) {
    /*
      The log-slave-updates should be disabled if binary log is disabled
      and --log-slave-updates option is not set explicitly on command
      line or configuration file.
    */
    if (!log_slave_updates_supplied) opt_log_slave_updates = false;
    /*
      The slave-preserve-commit-order should be disabled if binary log is
      disabled and --slave-preserve-commit-order option is not set
      explicitly on command line or configuration file.
    */
    if (!slave_preserve_commit_order_supplied)
      opt_slave_preserve_commit_order = false;
  }

  update_parser_max_mem_size();

  if (set_default_auth_plugin(default_auth_plugin,
                              strlen(default_auth_plugin))) {
    LogErr(ERROR_LEVEL, ER_AUTH_CANT_SET_DEFAULT_PLUGIN);
    return 1;
  }
  set_server_version();

  if (!opt_help) {
    LogErr(INFORMATION_LEVEL, ER_BASEDIR_SET_TO, mysql_home);
  }

  if (opt_initialize || opt_initialize_insecure) {
    LogErr(SYSTEM_LEVEL, ER_STARTING_INIT, my_progname, server_version,
           (ulong)getpid());
  } else if (!opt_help) {
    LogErr(SYSTEM_LEVEL, ER_STARTING_AS, my_progname, server_version,
           (ulong)getpid());
  }
  if (opt_help && !opt_verbose) unireg_abort(MYSQLD_SUCCESS_EXIT);

  DBUG_PRINT("info", ("%s  Ver %s for %s on %s\n", my_progname, server_version,
                      SYSTEM_TYPE, MACHINE_TYPE));

#ifdef HAVE_LINUX_LARGE_PAGES
  /* Initialize large page size */
  if (opt_large_pages && (opt_large_page_size = my_get_large_page_size())) {
    DBUG_PRINT("info",
               ("Large page set, large_page_size = %d", opt_large_page_size));
  } else {
    opt_large_pages = 0;
    /*
       Either not configured to use large pages or Linux haven't
       been compiled with large page support
    */
  }
#endif /* HAVE_LINUX_LARGE_PAGES */
#ifdef HAVE_SOLARIS_LARGE_PAGES
#define LARGE_PAGESIZE (4 * 1024 * 1024)         /* 4MB */
#define SUPER_LARGE_PAGESIZE (256 * 1024 * 1024) /* 256MB */
  if (opt_large_pages) {
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
      max_desired_page_size = SUPER_LARGE_PAGESIZE;
    else
      max_desired_page_size = LARGE_PAGESIZE;
    nelem = getpagesizes(NULL, 0);
    if (nelem > 0) {
      size_t *pagesize = (size_t *)malloc(sizeof(size_t) * nelem);
      if (pagesize != NULL && getpagesizes(pagesize, nelem) > 0) {
        size_t max_page_size = 0;
        for (int i = 0; i < nelem; i++) {
          if (pagesize[i] > max_page_size &&
              pagesize[i] <= max_desired_page_size)
            max_page_size = pagesize[i];
        }
        free(pagesize);
        if (max_page_size > 0) {
          struct memcntl_mha mpss;

          mpss.mha_cmd = MHA_MAPSIZE_BSSBRK;
          mpss.mha_pagesize = max_page_size;
          mpss.mha_flags = 0;
          memcntl(NULL, 0, MC_HAT_ADVISE, (caddr_t)&mpss, 0, 0);
          mpss.mha_cmd = MHA_MAPSIZE_STACK;
          memcntl(NULL, 0, MC_HAT_ADVISE, (caddr_t)&mpss, 0, 0);
        }
      }
    }
  }
#endif /* HAVE_SOLARIS_LARGE_PAGES */

  longlong default_value;
  sys_var *var;
  /* Calculate and update default value for thread_cache_size. */
  if ((default_value = 8 + max_connections / 100) > 100) default_value = 100;
  var = intern_find_sys_var(STRING_WITH_LEN("thread_cache_size"));
  var->update_default(default_value);

  /* Calculate and update default value for host_cache_size. */
  if ((default_value = 128 + max_connections) > 628 &&
      (default_value = 628 + ((max_connections - 500) / 20)) > 2000)
    default_value = 2000;
  var = intern_find_sys_var(STRING_WITH_LEN("host_cache_size"));
  var->update_default(default_value);

  /* Fix thread_cache_size. */
  if (!thread_cache_size_specified &&
      (Per_thread_connection_handler::max_blocked_pthreads =
           8 + max_connections / 100) > 100)
    Per_thread_connection_handler::max_blocked_pthreads = 100;

  /* Fix host_cache_size. */
  if (!host_cache_size_specified &&
      (host_cache_size = 128 + max_connections) > 628 &&
      (host_cache_size = 628 + ((max_connections - 500) / 20)) > 2000)
    host_cache_size = 2000;

  /* Fix back_log */
  if (back_log == 0 && (back_log = max_connections) > 65535) back_log = 65535;

  unireg_init(opt_specialflag); /* Set up extern variables */
  while (!(my_default_lc_messages = my_locale_by_name(NULL, lc_messages))) {
    LogErr(ERROR_LEVEL, ER_FAILED_TO_FIND_LOCALE_NAME, lc_messages);
    if (!my_strcasecmp(&my_charset_latin1, lc_messages,
                       mysqld_default_locale_name))
      return 1;
    lc_messages = (char *)mysqld_default_locale_name;
  }
  global_system_variables.lc_messages = my_default_lc_messages;
  if (init_errmessage()) /* Read error messages from file */
    return 1;
  init_client_errs();

  mysql_client_plugin_init();
  if (item_create_init()) return 1;
  item_init();
  range_optimizer_init();
  my_string_stack_guard = check_enough_stack_size;
  /*
    Process a comma-separated character set list and choose
    the first available character set. This is mostly for
    test purposes, to be able to start "mysqld" even if
    the requested character set is not available (see bug#18743).
  */
  for (;;) {
    char *next_character_set_name = strchr(default_character_set_name, ',');
    if (next_character_set_name) *next_character_set_name++ = '\0';
    if (!(default_charset_info = get_charset_by_csname(
              default_character_set_name, MY_CS_PRIMARY, MYF(MY_WME)))) {
      if (next_character_set_name) {
        default_character_set_name = next_character_set_name;
        default_collation_name = 0;  // Ignore collation
      } else
        return 1;  // Eof of the list
    } else
      break;
  }

  if (default_collation_name) {
    CHARSET_INFO *default_collation;
    default_collation = get_charset_by_name(default_collation_name, MYF(0));
    if (!default_collation) {
      LogErr(ERROR_LEVEL, ER_FAILED_TO_FIND_COLLATION_NAME,
             default_collation_name);
      return 1;
    }
    if (!my_charset_same(default_charset_info, default_collation)) {
      LogErr(ERROR_LEVEL, ER_INVALID_COLLATION_FOR_CHARSET,
             default_collation_name, default_charset_info->csname);
      return 1;
    }
    default_charset_info = default_collation;
  }
  /* Set collactions that depends on the default collation */
  global_system_variables.collation_server = default_charset_info;
  global_system_variables.collation_database = default_charset_info;

  if (is_supported_parser_charset(default_charset_info)) {
    global_system_variables.collation_connection = default_charset_info;
    global_system_variables.character_set_results = default_charset_info;
    global_system_variables.character_set_client = default_charset_info;
  } else {
    LogErr(INFORMATION_LEVEL, ER_FIXING_CLIENT_CHARSET,
           default_charset_info->csname, my_charset_latin1.csname);
    global_system_variables.collation_connection = &my_charset_latin1;
    global_system_variables.character_set_results = &my_charset_latin1;
    global_system_variables.character_set_client = &my_charset_latin1;
  }

  if (!(character_set_filesystem = get_charset_by_csname(
            character_set_filesystem_name, MY_CS_PRIMARY, MYF(MY_WME))))
    return 1;
  global_system_variables.character_set_filesystem = character_set_filesystem;

  default_collation_for_utf8mb4 =
      get_charset_by_name(default_collation_name_for_utf8mb4, MYF(0));
  if (default_collation_for_utf8mb4 == nullptr ||
      validate_default_collation_for_utf8mb4(default_collation_for_utf8mb4)) {
    LogErr(ERROR_LEVEL, ER_INVALID_DEFAULT_UTF8MB4_COLLATION_OPTION,
           default_collation_name_for_utf8mb4);
    return 1;
  }
  global_system_variables.default_collation_for_utf8mb4 =
      default_collation_for_utf8mb4;

  if (lex_init()) {
    LogErr(ERROR_LEVEL, ER_OOM);
    return 1;
  }

  while (!(my_default_lc_time_names =
               my_locale_by_name(NULL, lc_time_names_name))) {
    LogErr(ERROR_LEVEL, ER_FAILED_TO_FIND_LOCALE_NAME, lc_time_names_name);
    if (!my_strcasecmp(&my_charset_latin1, lc_time_names_name,
                       mysqld_default_locale_name))
      return 1;
    lc_time_names_name = (char *)mysqld_default_locale_name;
  }
  global_system_variables.lc_time_names = my_default_lc_time_names;

  /* check log options and issue warnings if needed */
  if (opt_general_log && opt_general_logname &&
      !(log_output_options & LOG_FILE) && !(log_output_options & LOG_NONE))
    LogErr(WARNING_LEVEL, ER_LOG_FILES_GIVEN_LOG_OUTPUT_IS_TABLE,
           "--general-log-file option");

  if (opt_slow_log && opt_slow_logname && !(log_output_options & LOG_FILE) &&
      !(log_output_options & LOG_NONE))
    LogErr(WARNING_LEVEL, ER_LOG_FILES_GIVEN_LOG_OUTPUT_IS_TABLE,
           "--slow-query-log-file option");

  if (opt_general_logname &&
      !is_valid_log_name(opt_general_logname, strlen(opt_general_logname))) {
    LogErr(ERROR_LEVEL, ER_LOG_FILE_INVALID, "--general_log_file",
           opt_general_logname);
    return 1;
  }

  if (opt_slow_logname &&
      !is_valid_log_name(opt_slow_logname, strlen(opt_slow_logname))) {
    LogErr(ERROR_LEVEL, ER_LOG_FILE_INVALID, "--slow_query_log_file",
           opt_slow_logname);
    return 1;
  }

  if (global_system_variables.transaction_write_set_extraction ==
          HASH_ALGORITHM_OFF &&
      mysql_bin_log.m_dependency_tracker.m_opt_tracking_mode !=
          DEPENDENCY_TRACKING_COMMIT_ORDER) {
    LogErr(ERROR_LEVEL,
           ER_TX_EXTRACTION_ALGORITHM_FOR_BINLOG_TX_DEPEDENCY_TRACKING,
           "XXHASH64 or MURMUR32", "WRITESET or WRITESET_SESSION");
    return 1;
  } else
    mysql_bin_log.m_dependency_tracker.tracking_mode_changed();

#define FIX_LOG_VAR(VAR, ALT) \
  if (!VAR || !*VAR) VAR = ALT;

  FIX_LOG_VAR(opt_general_logname,
              make_query_log_name(logname_path, QUERY_LOG_GENERAL));
  FIX_LOG_VAR(opt_slow_logname,
              make_query_log_name(slow_logname_path, QUERY_LOG_SLOW));

#if defined(ENABLED_DEBUG_SYNC)
  /* Initialize the debug sync facility. See debug_sync.cc. */
  if (debug_sync_init()) return 1; /* purecov: tested */
#endif                             /* defined(ENABLED_DEBUG_SYNC) */

  /* create the data directory if requested */
  if (unlikely(opt_initialize) &&
      initialize_create_data_directory(mysql_real_data_home))
    return 1;

  /*
    Ensure that lower_case_table_names is set on system where we have case
    insensitive names.  If this is not done the users MyISAM tables will
    get corrupted if accesses with names of different case.
  */
  DBUG_PRINT("info", ("lower_case_table_names: %d", lower_case_table_names));
  lower_case_file_system = test_if_case_insensitive(mysql_real_data_home);
  if (!lower_case_table_names && lower_case_file_system == 1) {
    if (lower_case_table_names_used) {
      LogErr(ERROR_LEVEL, ER_LOWER_CASE_TABLE_NAMES_CS_DD_ON_CI_FS_UNSUPPORTED);
      return 1;
    } else {
      LogErr(WARNING_LEVEL, ER_LOWER_CASE_TABLE_NAMES_USING_2,
             mysql_real_data_home);
      lower_case_table_names = 2;
    }
  } else if (lower_case_table_names == 2 &&
             !(lower_case_file_system =
                   (test_if_case_insensitive(mysql_real_data_home) == 1))) {
    LogErr(WARNING_LEVEL, ER_LOWER_CASE_TABLE_NAMES_USING_0,
           mysql_real_data_home);
    lower_case_table_names = 0;
  } else {
    lower_case_file_system =
        (test_if_case_insensitive(mysql_real_data_home) == 1);
  }

  /* Reset table_alias_charset, now that lower_case_table_names is set. */
  table_alias_charset =
      (lower_case_table_names ? &my_charset_utf8_tolower_ci : &my_charset_bin);

  /*
    Build do_table and ignore_table rules to hashes
    after the resetting of table_alias_charset.
  */
  if (rpl_global_filter.build_do_table_hash() ||
      rpl_global_filter.build_ignore_table_hash()) {
    LogErr(ERROR_LEVEL, ER_CANT_HASH_DO_AND_IGNORE_RULES);
    return 1;
  }

    /*
      Reset the P_S view for global replication filter at
      the end of server startup.
    */
#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  rpl_global_filter.wrlock();
  rpl_global_filter.reset_pfs_view();
  rpl_global_filter.unlock();
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

  if (rpl_channel_filters.build_do_and_ignore_table_hashes()) return 1;

  return 0;
}

static int init_thread_environment() {
  mysql_mutex_init(key_LOCK_status, &LOCK_status, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_manager, &LOCK_manager, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_crypt, &LOCK_crypt, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_user_conn, &LOCK_user_conn, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_global_system_variables,
                   &LOCK_global_system_variables, MY_MUTEX_INIT_FAST);
  mysql_rwlock_init(key_rwlock_LOCK_system_variables_hash,
                    &LOCK_system_variables_hash);
  mysql_mutex_init(key_LOCK_prepared_stmt_count, &LOCK_prepared_stmt_count,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_sql_slave_skip_counter,
                   &LOCK_sql_slave_skip_counter, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_slave_net_timeout, &LOCK_slave_net_timeout,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_error_messages, &LOCK_error_messages,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_uuid_generator, &LOCK_uuid_generator,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_sql_rand, &LOCK_sql_rand, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_log_throttle_qni, &LOCK_log_throttle_qni,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_offline_mode, &LOCK_offline_mode,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_default_password_lifetime,
                   &LOCK_default_password_lifetime, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_mandatory_roles, &LOCK_mandatory_roles,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_password_history, &LOCK_password_history,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_password_reuse_interval,
                   &LOCK_password_reuse_interval, MY_MUTEX_INIT_FAST);
  mysql_rwlock_init(key_rwlock_LOCK_sys_init_connect, &LOCK_sys_init_connect);
  mysql_rwlock_init(key_rwlock_LOCK_sys_init_slave, &LOCK_sys_init_slave);
  mysql_cond_init(key_COND_manager, &COND_manager);
  mysql_mutex_init(key_LOCK_server_started, &LOCK_server_started,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_server_started, &COND_server_started);
  mysql_mutex_init(key_LOCK_reset_gtid_table, &LOCK_reset_gtid_table,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_compress_gtid_table, &LOCK_compress_gtid_table,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_collect_instance_log, &LOCK_collect_instance_log,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_compress_gtid_table, &COND_compress_gtid_table);
  Events::init_mutexes();
#if defined(_WIN32)
  mysql_mutex_init(key_LOCK_handler_count, &LOCK_handler_count,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_handler_count, &COND_handler_count);
#else
  mysql_mutex_init(key_LOCK_socket_listener_active,
                   &LOCK_socket_listener_active, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_socket_listener_active,
                  &COND_socket_listener_active);
  mysql_mutex_init(key_LOCK_start_signal_handler, &LOCK_start_signal_handler,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_start_signal_handler, &COND_start_signal_handler);
#endif  // _WIN32
  /* Parameter for threads created for connections */
  (void)my_thread_attr_init(&connection_attrib);
  my_thread_attr_setdetachstate(&connection_attrib, MY_THREAD_CREATE_DETACHED);
#ifndef _WIN32
  pthread_attr_setscope(&connection_attrib, PTHREAD_SCOPE_SYSTEM);
#endif

  mysql_mutex_init(key_LOCK_keyring_operations, &LOCK_keyring_operations,
                   MY_MUTEX_INIT_FAST);
  return 0;
}

static ssl_artifacts_status auto_detect_ssl() {
  MY_STAT cert_stat, cert_key, ca_stat;
  uint result = 1;
  ssl_artifacts_status ret_status = SSL_ARTIFACTS_VIA_OPTIONS;

  if ((!opt_ssl_cert || !opt_ssl_cert[0]) &&
      (!opt_ssl_key || !opt_ssl_key[0]) && (!opt_ssl_ca || !opt_ssl_ca[0]) &&
      (!opt_ssl_capath || !opt_ssl_capath[0]) &&
      (!opt_ssl_crl || !opt_ssl_crl[0]) &&
      (!opt_ssl_crlpath || !opt_ssl_crlpath[0])) {
    result =
        result << (my_stat(DEFAULT_SSL_SERVER_CERT, &cert_stat, MYF(0)) ? 1 : 0)
               << (my_stat(DEFAULT_SSL_SERVER_KEY, &cert_key, MYF(0)) ? 1 : 0)
               << (my_stat(DEFAULT_SSL_CA_CERT, &ca_stat, MYF(0)) ? 1 : 0);

    switch (result) {
      case 8:
        opt_ssl_ca = (char *)DEFAULT_SSL_CA_CERT;
        opt_ssl_cert = (char *)DEFAULT_SSL_SERVER_CERT;
        opt_ssl_key = (char *)DEFAULT_SSL_SERVER_KEY;
        ret_status = SSL_ARTIFACTS_AUTO_DETECTED;
        break;
      case 4:
      case 2:
        ret_status = SSL_ARTIFACT_TRACES_FOUND;
        break;
      default:
        ret_status = SSL_ARTIFACTS_NOT_FOUND;
        break;
    };
  }

  return ret_status;
}

static int warn_one(const char *file_name) {
  char *issuer = NULL;
  char *subject = NULL;
  X509 *ca_cert;
  BIO *bio;
  FILE *fp;

  if (!(fp = my_fopen(file_name, O_RDONLY | MY_FOPEN_BINARY, MYF(MY_WME)))) {
    LogErr(ERROR_LEVEL, ER_CANT_OPEN_CA);
    return 1;
  }

  bio = BIO_new(BIO_s_file());
  if (!bio) {
    sql_print_error("Error allocating SSL BIO");
    my_fclose(fp, MYF(0));
    return 1;
  }
  BIO_set_fp(bio, fp, BIO_NOCLOSE);
  ca_cert = PEM_read_bio_X509(bio, 0, 0, 0);
  BIO_free(bio);

  if (!ca_cert) {
    /* We are not interested in anything other than X509 certificates */
    my_fclose(fp, MYF(MY_WME));
    return 0;
  }

  issuer = X509_NAME_oneline(X509_get_issuer_name(ca_cert), 0, 0);
  subject = X509_NAME_oneline(X509_get_subject_name(ca_cert), 0, 0);

  /* Suppressing warning which is not relevant during initialization */
  if (!strcmp(issuer, subject) &&
      !(opt_initialize || opt_initialize_insecure)) {
    LogErr(WARNING_LEVEL, ER_CA_SELF_SIGNED, file_name);
  }

  OPENSSL_free(issuer);
  OPENSSL_free(subject);
  X509_free(ca_cert);
  my_fclose(fp, MYF(MY_WME));
  return 0;
}

static int warn_self_signed_ca() {
  int ret_val = 0;
  if (opt_ssl_ca && opt_ssl_ca[0]) {
    if (warn_one(opt_ssl_ca)) return 1;
  }
  if (opt_ssl_capath && opt_ssl_capath[0]) {
    /* We have ssl-capath. So search all files in the dir */
    MY_DIR *ca_dir;
    uint file_count;
    DYNAMIC_STRING file_path;
    char dir_separator[FN_REFLEN];
    size_t dir_path_length;

    init_dynamic_string(&file_path, opt_ssl_capath, FN_REFLEN, FN_REFLEN);
    dir_separator[0] = FN_LIBCHAR;
    dir_separator[1] = 0;
    dynstr_append(&file_path, dir_separator);
    dir_path_length = file_path.length;

    if (!(ca_dir =
              my_dir(opt_ssl_capath, MY_WANT_STAT | MY_DONT_SORT | MY_WME))) {
      LogErr(ERROR_LEVEL, ER_CANT_ACCESS_CAPATH);
      return 1;
    }

    for (file_count = 0; file_count < ca_dir->number_off_files; file_count++) {
      if (!MY_S_ISDIR(ca_dir->dir_entry[file_count].mystat->st_mode)) {
        file_path.length = dir_path_length;
        dynstr_append(&file_path, ca_dir->dir_entry[file_count].name);
        if ((ret_val = warn_one(file_path.str))) break;
      }
    }
    my_dirend(ca_dir);
    dynstr_free(&file_path);

    ca_dir = 0;
    memset(&file_path, 0, sizeof(file_path));
  }
  return ret_val;
}

static void init_ssl() {
#ifdef HAVE_OPENSSL
#ifndef HAVE_WOLFSSL
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  CRYPTO_malloc_init();
#else  /* OPENSSL_VERSION_NUMBER < 0x10100000L */
  OPENSSL_malloc_init();
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
#endif
  ssl_start();
#endif
}

static int init_ssl_communication() {
#ifdef HAVE_OPENSSL
#ifndef HAVE_WOLFSSL
  char ssl_err_string[OPENSSL_ERROR_LENGTH] = {'\0'};
  int ret_fips_mode = set_fips_mode(opt_ssl_fips_mode, ssl_err_string);
  if (ret_fips_mode != 1) {
    LogErr(ERROR_LEVEL, ER_SSL_FIPS_MODE_ERROR, ssl_err_string);
    return 1;
  }
#endif
  if (opt_use_ssl) {
    ssl_artifacts_status auto_detection_status = auto_detect_ssl();
    if (auto_detection_status == SSL_ARTIFACTS_AUTO_DETECTED)
      LogErr(INFORMATION_LEVEL, ER_SSL_TRYING_DATADIR_DEFAULTS,
             DEFAULT_SSL_CA_CERT, DEFAULT_SSL_SERVER_CERT,
             DEFAULT_SSL_SERVER_KEY);
#ifndef HAVE_WOLFSSL
    if (do_auto_cert_generation(auto_detection_status) == false) return 1;
#endif

    enum enum_ssl_init_error error = SSL_INITERR_NOERROR;
    long ssl_ctx_flags = process_tls_version(opt_tls_version);
    /* having ssl_acceptor_fd != 0 signals the use of SSL */
    ssl_acceptor_fd = new_VioSSLAcceptorFd(
        opt_ssl_key, opt_ssl_cert, opt_ssl_ca, opt_ssl_capath, opt_ssl_cipher,
        &error, opt_ssl_crl, opt_ssl_crlpath, ssl_ctx_flags);
    DBUG_PRINT("info", ("ssl_acceptor_fd: %p", ssl_acceptor_fd));
#ifndef HAVE_WOLFSSL
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    ERR_remove_thread_state(0);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
#endif
    if (!ssl_acceptor_fd) {
    /*
      No real need for opt_use_ssl to be enabled in bootstrap mode,
      but we want the SSL materal generation and/or validation (if supplied).
      So we keep it on.

      For wolfSSL (since it can't auto-generate the certs from inside the
      server) we need to hush the warning if in bootstrap mode, as in
      that mode the server won't be listening for connections and thus
      the lack of SSL material makes no real difference.
      However if the user specified any of the --ssl options we keep the
      warning as it's showing problems with the values supplied.

      For openssl, we don't hush the option since it would indicate a failure
      in auto-generation, bad key material explicitly specified or
      auto-generation disabled explcitly while SSL is still on.
    */
#ifdef HAVE_WOLFSSL
      if (!opt_initialize || SSL_ARTIFACTS_NOT_FOUND != auto_detection_status)
#endif
      {
        LogErr(WARNING_LEVEL, ER_SSL_LIBRARY_ERROR, sslGetErrString(error));
      }
      opt_use_ssl = 0;
      have_ssl = SHOW_OPTION_DISABLED;
    } else {
      /* Check if CA certificate is self signed */
      if (warn_self_signed_ca()) return 1;
      /* create one SSL that we can use to read information from */
      if (!(ssl_acceptor = SSL_new(ssl_acceptor_fd->ssl_context))) return 1;
    }
  } else {
    have_ssl = SHOW_OPTION_DISABLED;
  }
  if (init_rsa_keys()) return 1;
#endif /* HAVE_OPENSSL */
  return 0;
}

static void end_ssl() {
#ifdef HAVE_OPENSSL
  if (ssl_acceptor_fd) {
    if (ssl_acceptor) SSL_free(ssl_acceptor);
    free_vio_ssl_acceptor_fd(ssl_acceptor_fd);
    ssl_acceptor_fd = 0;
  }
  deinit_rsa_keys();
#endif /* HAVE_OPENSSL */
}

/**
  Generate a UUID and save it into server_uuid variable.

  @return Retur 0 or 1 if an error occurred.
 */
static int generate_server_uuid() {
  THD *thd;
  Item_func_uuid *func_uuid;
  String uuid;

  /*
    To be able to run this from boot, we allocate a temporary THD
   */
  if (!(thd = new THD)) {
    LogErr(ERROR_LEVEL, ER_NO_THD_NO_UUID);
    return 1;
  }
  thd->thread_stack = (char *)&thd;
  thd->store_globals();

  /*
    Initialize the variables which are used during "uuid generator
    initialization" with values that should normally differ between
    mysqlds on the same host. This avoids that another mysqld started
    at the same time on the same host get the same "server_uuid".
  */

  const time_t save_server_start_time = server_start_time;
  server_start_time += ((ulonglong)current_pid << 48) + current_pid;
  thd->status_var.bytes_sent = (ulonglong)thd;

  lex_start(thd);
  func_uuid = new (thd->mem_root) Item_func_uuid();
  func_uuid->fixed = 1;
  func_uuid->val_str(&uuid);

  // Restore global variables used for salting
  server_start_time = save_server_start_time;

  delete thd;

  strncpy(server_uuid, uuid.c_ptr(), UUID_LENGTH);
  DBUG_EXECUTE_IF("server_uuid_deterministic",
                  memcpy(server_uuid, "00000000-1111-0000-1111-000000000000",
                         UUID_LENGTH););
  server_uuid[UUID_LENGTH] = '\0';
  return 0;
}

/**
  Save all options which was auto-generated by server-self into the given file.

  @param fname The name of the file in which the auto-generated options will b
  e saved.

  @return Return 0 or 1 if an error occurred.
 */
static int flush_auto_options(const char *fname) {
  File fd;
  IO_CACHE io_cache;
  int result = 0;

  if ((fd = my_open(fname, O_CREAT | O_RDWR, MYF(MY_WME))) < 0) {
    LogErr(ERROR_LEVEL, ER_AUTO_OPTIONS_FAILED, "file", fname, my_errno());
    return 1;
  }

  if (init_io_cache(&io_cache, fd, IO_SIZE * 2, WRITE_CACHE, 0L, 0,
                    MYF(MY_WME))) {
    LogErr(ERROR_LEVEL, ER_AUTO_OPTIONS_FAILED, "a cache on ", fname,
           my_errno());
    my_close(fd, MYF(MY_WME));
    return 1;
  }

  my_b_seek(&io_cache, 0L);
  my_b_printf(&io_cache, "%s\n", "[auto]");
  my_b_printf(&io_cache, "server-uuid=%s\n", server_uuid);

  if (flush_io_cache(&io_cache) || my_sync(fd, MYF(MY_WME))) result = 1;

  my_close(fd, MYF(MY_WME));
  end_io_cache(&io_cache);
  return result;
}

/**
  File 'auto.cnf' resides in the data directory to hold values of options that
  server evaluates itself and that needs to be durable to sustain the server
  restart. There is only a section ['auto'] in the file. All these options are
  in the section. Only one option exists now, it is server_uuid.
  Note, the user may not supply any literal value to these auto-options, and
  only allowed to trigger (re)evaluation.
  For instance, 'server_uuid' value will be evaluated and stored if there is
  no corresponding line in the file.
  Because of the specifics of the auto-options, they need a seperate storage.
  Meanwhile, it is the 'auto.cnf' that has the same structure as 'my.cnf'.

  @todo consider to implement sql-query-able persistent storage by WL#5279.
  @return Return 0 or 1 if an error occurred.
 */
static int init_server_auto_options() {
  bool flush = false;
  char fname[FN_REFLEN];
  char *name = (char *)"auto";
  const char *groups[] = {"auto", NULL};
  char *uuid = 0;
  my_option auto_options[] = {
      {"server-uuid", 0, "", &uuid, &uuid, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
       0, 0},
      {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

  DBUG_ENTER("init_server_auto_options");

  if (NULL == fn_format(fname, "auto.cnf", mysql_data_home, "",
                        MY_UNPACK_FILENAME | MY_SAFE_PATH))
    DBUG_RETURN(1);

  /* load_defaults require argv[0] is not null */
  char **argv = &name;
  int argc = 1;
  if (!check_file_permissions(fname, false)) {
    /*
      Found a world writable file hence removing it as it is dangerous to write
      a new UUID into the same file.
     */
    my_delete(fname, MYF(MY_WME));
    LogErr(WARNING_LEVEL, ER_WRITABLE_CONFIG_REMOVED, fname);
  }

  /* load all options in 'auto.cnf'. */
  MEM_ROOT alloc{PSI_NOT_INSTRUMENTED, 512};
  if (my_load_defaults(fname, groups, &argc, &argv, &alloc, NULL))
    DBUG_RETURN(1);

  if (handle_options(&argc, &argv, auto_options, mysqld_get_one_option))
    DBUG_RETURN(1);

  DBUG_PRINT("info", ("uuid=%p=%s server_uuid=%s", uuid, uuid, server_uuid));
  if (uuid) {
    if (!binary_log::Uuid::is_valid(uuid, binary_log::Uuid::TEXT_LENGTH)) {
      LogErr(ERROR_LEVEL, ER_UUID_INVALID);
      goto err;
    }
    /*
      Uuid::is_valid() cannot do strict check on the length as it will be
      called by GTID::is_valid() as well (GTID = UUID:seq_no). We should
      explicitly add the *length check* here in this function.

      If UUID length is less than '36' (UUID_LENGTH), that error case would have
      got caught in above is_valid check. The below check is to make sure that
      length is not greater than UUID_LENGTH i.e., there are no extra characters
      (Garbage) at the end of the valid UUID.
    */
    if (strlen(uuid) > UUID_LENGTH) {
      LogErr(ERROR_LEVEL, ER_UUID_SCRUB, UUID_LENGTH);
      goto err;
    }
    strcpy(server_uuid, uuid);
  } else {
    DBUG_PRINT("info", ("generating server_uuid"));
    flush = true;
    /* server_uuid will be set in the function */
    if (generate_server_uuid()) goto err;
    DBUG_PRINT("info", ("generated server_uuid=%s", server_uuid));
    if (opt_initialize || opt_initialize_insecure) {
      LogErr(INFORMATION_LEVEL, ER_CREATING_NEW_UUID_FIRST_START, server_uuid);

    } else {
      LogErr(WARNING_LEVEL, ER_CREATING_NEW_UUID, server_uuid);
    }
  }

  if (flush) DBUG_RETURN(flush_auto_options(fname));
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}

static bool initialize_storage_engine(char *se_name, const char *se_kind,
                                      plugin_ref *dest_plugin) {
  LEX_STRING name = {se_name, strlen(se_name)};
  plugin_ref plugin;
  handlerton *hton;
  if ((plugin = ha_resolve_by_name(0, &name, false)))
    hton = plugin_data<handlerton *>(plugin);
  else {
    LogErr(ERROR_LEVEL, ER_UNKNOWN_UNSUPPORTED_STORAGE_ENGINE, se_name);
    return true;
  }
  if (!ha_storage_engine_is_enabled(hton)) {
    if (!opt_initialize) {
      LogErr(ERROR_LEVEL, ER_DEFAULT_SE_UNAVAILABLE, se_kind, se_name);
      return true;
    }
    DBUG_ASSERT(*dest_plugin);
  } else {
    /*
      Need to unlock as global_system_variables.table_plugin
      was acquired during plugin_register_builtin_and_init_core_se()
    */
    plugin_unlock(0, *dest_plugin);
    *dest_plugin = plugin;
  }
  return false;
}

static int init_server_components() {
  DBUG_ENTER("init_server_components");
  /*
    We need to call each of these following functions to ensure that
    all things are initialized so that unireg_abort() doesn't fail
  */
  mdl_init();
  partitioning_init();
  if (table_def_init() | hostname_cache_init(host_cache_size))
    unireg_abort(MYSQLD_ABORT_EXIT);

  /*
    Timers not needed if only starting with --help.
  */
  if (!opt_help) {
    if (my_timer_initialize())
      LogErr(ERROR_LEVEL, ER_CANT_INIT_TIMER, errno);
    else
      have_statement_timeout = SHOW_OPTION_YES;
  }

  randominit(&sql_rand, (ulong)server_start_time, (ulong)server_start_time / 2);
  setup_fpu();
  init_slave_list();

  /* Setup logs */

  /*
    Enable old-fashioned error log, except when the user has requested
    help information. Since the implementation of plugin server
    variables the help output is now written much later.

    log_error_dest can be:
    disabled_my_option     --log-error was not used or --log-error=
    ""                     --log-error without arguments (no '=')
    filename               --log-error=filename
  */
#ifdef _WIN32
  /*
    Enable the error log file only if console option is not specified
    and --help is not used.
  */
  bool log_errors_to_file = !opt_help && !opt_console;
#else
  /*
    Enable the error log file only if --log-error=filename or --log-error
    was used. Logging to file is disabled by default unlike on Windows.
  */
  bool log_errors_to_file = !opt_help && (log_error_dest != disabled_my_option);
#endif

  if (log_errors_to_file) {
    // Construct filename if no filename was given by the user.
    if (!log_error_dest[0] || log_error_dest == disabled_my_option) {
#ifdef _WIN32
      const char *filename = pidfile_name;
#else
      const char *filename = default_logfile_name;
#endif
      fn_format(errorlog_filename_buff, filename, mysql_real_data_home, ".err",
                MY_REPLACE_EXT | /* replace '.<domain>' by '.err', bug#4997 */
                    MY_REPLACE_DIR);
    } else
      fn_format(errorlog_filename_buff, log_error_dest, mysql_data_home, ".err",
                MY_UNPACK_FILENAME);
    /*
      log_error_dest may have been set to disabled_my_option or "" if no
      argument was passed, but we need to show the real name in SHOW VARIABLES.
    */
    log_error_dest = errorlog_filename_buff;

#ifndef _WIN32
    // Create backup stream to stdout if deamonizing and connected to tty
    if (opt_daemonize && isatty(STDOUT_FILENO)) {
      nstdout = fdopen(dup(STDOUT_FILENO), "a");
      if (nstdout == nullptr) {
        LogErr(ERROR_LEVEL, ER_DUP_FD_OPEN_FAILED, "stdout", strerror(errno));
        unireg_abort(MYSQLD_ABORT_EXIT);
      }
      // Display location of error log file on stdout if connected to tty
      fprintf(nstdout, "mysqld will log errors to %s\n",
              errorlog_filename_buff);
    }
#endif /* ndef _WIN32 */

    if (open_error_log(errorlog_filename_buff)) {
      LogErr(ERROR_LEVEL, ER_CANT_OPEN_ERROR_LOG, log_error_dest,
             strerror(errno));
      unireg_abort(MYSQLD_ABORT_EXIT);
    }
#ifdef _WIN32
      // FreeConsole();        // Remove window
#endif /* _WIN32 */
  } else {
    // We are logging to stderr and SHOW VARIABLES should reflect that.
    log_error_dest = "stderr";
    // Flush messages buffered so far.
    flush_error_log_messages();
  }

  enter_cond_hook = thd_enter_cond;
  exit_cond_hook = thd_exit_cond;
  enter_stage_hook = thd_enter_stage;
  set_waiting_for_disk_space_hook = thd_set_waiting_for_disk_space;
  is_killed_hook = (int (*)(const void *))thd_killed;

  if (transaction_cache_init()) {
    LogErr(ERROR_LEVEL, ER_OOM);
    unireg_abort(MYSQLD_ABORT_EXIT);
  }

  /*
    initialize delegates for extension observers, errors have already
    been reported in the function
  */
  if (delegates_init()) unireg_abort(MYSQLD_ABORT_EXIT);

  /* need to configure logging before initializing storage engines */
  if (opt_log_slave_updates && !opt_bin_log) {
    LogErr(WARNING_LEVEL, ER_NEED_LOG_BIN, "--log-slave-updates");
  }
  if (binlog_format_used && !opt_bin_log)
    LogErr(WARNING_LEVEL, ER_NEED_LOG_BIN, "--binlog-format");

  /* Check that we have not let the format to unspecified at this point */
  DBUG_ASSERT((uint)global_system_variables.binlog_format <=
              array_elements(binlog_format_names) - 1);

  if (opt_log_slave_updates && replicate_same_server_id) {
    if (opt_bin_log) {
      LogErr(ERROR_LEVEL, ER_RPL_INFINITY_DENIED);
      unireg_abort(MYSQLD_ABORT_EXIT);
    } else
      LogErr(WARNING_LEVEL, ER_RPL_INFINITY_IGNORED);
  }

  opt_server_id_mask = ~ulong(0);
  opt_server_id_mask =
      (opt_server_id_bits == 32) ? ~ulong(0) : (1 << opt_server_id_bits) - 1;
  if (server_id != (server_id & opt_server_id_mask)) {
    LogErr(ERROR_LEVEL, ER_SERVERID_TOO_LARGE);
    unireg_abort(MYSQLD_ABORT_EXIT);
  }

  if (opt_bin_log) {
    /* Reports an error and aborts, if the --log-bin's path
       is a directory.*/
    if (opt_bin_logname &&
        opt_bin_logname[strlen(opt_bin_logname) - 1] == FN_LIBCHAR) {
      LogErr(ERROR_LEVEL, ER_NEED_FILE_INSTEAD_OF_DIR, "--log-bin",
             opt_bin_logname);
      unireg_abort(MYSQLD_ABORT_EXIT);
    }

    /* Reports an error and aborts, if the --log-bin-index's path
       is a directory.*/
    if (opt_binlog_index_name &&
        opt_binlog_index_name[strlen(opt_binlog_index_name) - 1] ==
            FN_LIBCHAR) {
      LogErr(ERROR_LEVEL, ER_NEED_FILE_INSTEAD_OF_DIR, "--log-bin-index",
             opt_binlog_index_name);
      unireg_abort(MYSQLD_ABORT_EXIT);
    }

    char buf[FN_REFLEN];
    const char *ln;
    if (log_bin_supplied) {
      /*
        Binary log basename defaults to "`hostname`-bin" name prefix
        if --log-bin is used without argument.
      */
      ln = mysql_bin_log.generate_name(opt_bin_logname, "-bin", buf);
    } else {
      /*
        Binary log basename defaults to "binlog" name prefix
        if --log-bin is not used.
      */
      ln = mysql_bin_log.generate_name(opt_bin_logname, "", buf);
    }

    if (!opt_bin_logname && !opt_binlog_index_name && log_bin_supplied) {
      /*
        User didn't give us info to name the binlog index file.
        Picking `hostname`-bin.index like did in 4.x, causes replication to
        fail if the hostname is changed later. So, we would like to instead
        require a name. But as we don't want to break many existing setups, we
        only give warning, not error.
      */
      LogErr(INFORMATION_LEVEL, ER_LOG_BIN_BETTER_WITH_NAME, ln);
    }
    if (ln == buf) {
      my_free(opt_bin_logname);
      opt_bin_logname = my_strdup(key_memory_opt_bin_logname, buf, MYF(0));
    }

    /*
      Skip opening the index file if we start with --help. This is necessary
      to avoid creating the file in an otherwise empty datadir, which will
      cause a succeeding 'mysqld --initialize' to fail.
    */
    if (!opt_help &&
        mysql_bin_log.open_index_file(opt_binlog_index_name, ln, true)) {
      unireg_abort(MYSQLD_ABORT_EXIT);
    }
  }

  if (opt_bin_log) {
    /*
      opt_bin_logname[0] needs to be checked to make sure opt binlog name is
      not an empty string, incase it is an empty string default file
      extension will be passed
     */
    if (log_bin_supplied) {
      log_bin_basename = rpl_make_log_name(
          key_memory_MYSQL_BIN_LOG_basename, opt_bin_logname,
          default_logfile_name,
          (opt_bin_logname && opt_bin_logname[0]) ? "" : "-bin");
    } else {
      log_bin_basename =
          rpl_make_log_name(key_memory_MYSQL_BIN_LOG_basename, opt_bin_logname,
                            default_binlogfile_name, "");
    }

    log_bin_index =
        rpl_make_log_name(key_memory_MYSQL_BIN_LOG_index, opt_binlog_index_name,
                          log_bin_basename, ".index");

    if ((!opt_binlog_index_name || !opt_binlog_index_name[0]) &&
        log_bin_index) {
      strmake(default_binlog_index_name,
              log_bin_index + dirname_length(log_bin_index),
              FN_REFLEN + index_ext_length - 1);
      opt_binlog_index_name = default_binlog_index_name;
    }

    if (log_bin_basename == NULL || log_bin_index == NULL) {
      LogErr(ERROR_LEVEL, ER_RPL_CANT_MAKE_PATHS, (int)FN_REFLEN, (int)FN_LEN);
      unireg_abort(MYSQLD_ABORT_EXIT);
    }
  }

  DBUG_PRINT("debug",
             ("opt_bin_logname: %s, opt_relay_logname: %s, pidfile_name: %s",
              opt_bin_logname, opt_relay_logname, pidfile_name));

  /*
    opt_relay_logname[0] needs to be checked to make sure opt relaylog name is
    not an empty string, incase it is an empty string default file
    extension will be passed
   */
  relay_log_basename = rpl_make_log_name(
      key_memory_MYSQL_RELAY_LOG_basename, opt_relay_logname,
      default_logfile_name,
      (opt_relay_logname && opt_relay_logname[0]) ? "" : relay_ext);

  if (!opt_relay_logname || !opt_relay_logname[0]) {
    if (relay_log_basename) {
      strmake(default_relaylogfile_name,
              relay_log_basename + dirname_length(relay_log_basename),
              FN_REFLEN + relay_ext_length - 1);
      opt_relay_logname = default_relaylogfile_name;
    }
  } else
    opt_relay_logname_supplied = true;

  if (relay_log_basename != NULL)
    relay_log_index = rpl_make_log_name(key_memory_MYSQL_RELAY_LOG_index,
                                        opt_relaylog_index_name,
                                        relay_log_basename, ".index");

  if (!opt_relaylog_index_name || !opt_relaylog_index_name[0]) {
    if (relay_log_index) {
      strmake(default_relaylog_index_name,
              relay_log_index + dirname_length(relay_log_index),
              FN_REFLEN + relay_ext_length + index_ext_length - 1);
      opt_relaylog_index_name = default_relaylog_index_name;
    }
  } else
    opt_relaylog_index_name_supplied = true;

  if (relay_log_basename == NULL || relay_log_index == NULL) {
    LogErr(ERROR_LEVEL, ER_RPL_CANT_MAKE_PATHS, (int)FN_REFLEN, (int)FN_LEN);
    unireg_abort(MYSQLD_ABORT_EXIT);
  }

  if (log_bin_basename != NULL &&
      !strcmp(log_bin_basename, relay_log_basename)) {
    const int bin_ext_length = 4;
    char default_binlogfile_name_from_hostname[FN_REFLEN + bin_ext_length];
    /* Generate default bin log file name. */
    strmake(default_binlogfile_name_from_hostname, default_logfile_name,
            FN_REFLEN - 1);
    strcat(default_binlogfile_name_from_hostname, "-bin");

    if (!default_relaylogfile_name[0]) {
      /* Generate default relay log file name. */
      strmake(default_relaylogfile_name, default_logfile_name, FN_REFLEN - 1);
      strcat(default_relaylogfile_name, relay_ext);
    }
    /*
      Reports an error and aborts, if the same base name is specified
      for both binary and relay logs.
    */
    LogErr(ERROR_LEVEL, ER_RPL_CANT_HAVE_SAME_BASENAME, log_bin_basename,
           "--log-bin", default_binlogfile_name,
           default_binlogfile_name_from_hostname, "--relay-log",
           default_relaylogfile_name);
    unireg_abort(MYSQLD_ABORT_EXIT);
  }

  if (global_system_variables.binlog_row_value_options != 0) {
    const char *msg = NULL;
    longlong err = ER_BINLOG_ROW_VALUE_OPTION_IGNORED;
    if (!opt_bin_log)
      msg = "the binary log is disabled";
    else if (global_system_variables.binlog_format == BINLOG_FORMAT_STMT)
      msg = "binlog_format=STATEMENT";
    else if (log_bin_use_v1_row_events) {
      msg = "binlog_row_value_options=PARTIAL_JSON";
      err = ER_BINLOG_USE_V1_ROW_EVENTS_IGNORED;
    } else if (global_system_variables.binlog_row_image ==
               BINLOG_ROW_IMAGE_FULL) {
      msg = "binlog_row_image=FULL";
      err = ER_BINLOG_ROW_VALUE_OPTION_USED_ONLY_FOR_AFTER_IMAGES;
    }
    if (msg) {
      switch (err) {
        case ER_BINLOG_ROW_VALUE_OPTION_IGNORED:
        case ER_BINLOG_ROW_VALUE_OPTION_USED_ONLY_FOR_AFTER_IMAGES:
          LogErr(WARNING_LEVEL, err, msg, "PARTIAL_JSON");
          break;
        case ER_BINLOG_USE_V1_ROW_EVENTS_IGNORED:
          LogErr(WARNING_LEVEL, err, msg);
          break;
        default:
          DBUG_ASSERT(0); /* purecov: deadcode */
      }
    }
  }

  /* call ha_init_key_cache() on all key caches to init them */
  process_key_caches(&ha_init_key_cache);

  /* Allow storage engine to give real error messages */
  if (ha_init_errors()) DBUG_RETURN(1);

  if (gtid_server_init()) {
    LogErr(ERROR_LEVEL, ER_CANT_INITIALIZE_GTID);
    unireg_abort(MYSQLD_ABORT_EXIT);
  }

  {
  /*
    We have to call a function in log_resource.cc, or its references
    won't be visible to plugins.
  */
#ifndef DBUG_OFF
    int dummy =
#endif
        Log_resource::dummy_function_to_ensure_we_are_linked_into_the_server();
    DBUG_ASSERT(dummy == 1);
  }

  /*
    We need to initialize the UDF globals early before reading the proc table
    and before the server component initialization to allow other components
    to register their UDFs at init time and de-register them at deinit time.
  */
  udf_init_globals();

  /*
    Set tc_log to point to TC_LOG_DUMMY early in order to allow plugin_init()
    to commit attachable transaction after reading from mysql.plugin table.
    If necessary tc_log will be adjusted to point to correct TC_LOG instance
    later.
  */
  tc_log = &tc_log_dummy;

  /*Load early plugins */
  if (plugin_register_early_plugins(
          &remaining_argc, remaining_argv,
          opt_help ? PLUGIN_INIT_SKIP_INITIALIZATION : 0)) {
    LogErr(ERROR_LEVEL, ER_CANT_INITIALIZE_EARLY_PLUGINS);
    unireg_abort(1);
  }

  /* This limits ability to configure SSL library through config options */
  init_ssl();
  /* Load builtin plugins, initialize MyISAM, CSV and InnoDB */
  if (plugin_register_builtin_and_init_core_se(&remaining_argc,
                                               remaining_argv)) {
    LogErr(ERROR_LEVEL, ER_CANT_INITIALIZE_BUILTIN_PLUGINS);
    unireg_abort(1);
  }

  /*
    Needs to be done before dd::init() which runs DDL commands (for real)
    during instance initialization.
  */
  init_sql_command_flags();

  /*
    plugin_register_dynamic_and_init_all() needs DD initialized.
    Initialize DD to create data directory using current server.
  */
  if (opt_initialize) {
    if (!opt_help) {
      if (dd::init(dd::enum_dd_init_type::DD_INITIALIZE)) {
        LogErr(ERROR_LEVEL, ER_DD_INIT_FAILED);
        unireg_abort(1);
      }

      if (dd::init(dd::enum_dd_init_type::DD_INITIALIZE_SYSTEM_VIEWS)) {
        LogErr(ERROR_LEVEL, ER_SYSTEM_VIEW_INIT_FAILED);
        unireg_abort(1);
      }
    }
  } else {
    /*
      Initialize DD in case of upgrade and normal normal server restart.
      It is detected if we are starting on old data directory or current
      data directory. If it is old data directory, DD tables are created.
      If server is starting on data directory with DD tables, DD is initialized.
    */
    if (!opt_help && dd::init(dd::enum_dd_init_type::DD_RESTART_OR_UPGRADE)) {
      LogErr(ERROR_LEVEL, ER_DD_INIT_FAILED);
      unireg_abort(1);
    }
  }

  /*
    Skip reading the plugin table when starting with --help in order
    to also skip initializing InnoDB. This provides a simpler and more
    uniform handling of various startup use cases, e.g. when the data
    directory does not exist, exists but is empty, exists with InnoDB
    system tablespaces present etc.
  */
  if (plugin_register_dynamic_and_init_all(
          &remaining_argc, remaining_argv,
          (opt_noacl ? PLUGIN_INIT_SKIP_PLUGIN_TABLE : 0) |
              (opt_help ? (PLUGIN_INIT_SKIP_INITIALIZATION |
                           PLUGIN_INIT_SKIP_PLUGIN_TABLE)
                        : 0))) {
    // Delete all DD tables in case of error in initializing plugins.
    if (dd::upgrade_57::in_progress())
      (void)dd::init(dd::enum_dd_init_type::DD_DELETE);

    LogErr(ERROR_LEVEL, ER_CANT_INITIALIZE_DYNAMIC_PLUGINS);
    unireg_abort(MYSQLD_ABORT_EXIT);
  }
  dynamic_plugins_are_initialized =
      true; /* Don't separate from init function */

  LEX_CSTRING plugin_name = {C_STRING_WITH_LEN("thread_pool")};
  if (Connection_handler_manager::thread_handling !=
          Connection_handler_manager::SCHEDULER_ONE_THREAD_PER_CONNECTION ||
      plugin_is_ready(plugin_name, MYSQL_DAEMON_PLUGIN)) {
    auto res_grp_mgr = resourcegroups::Resource_group_mgr::instance();
    res_grp_mgr->disable_resource_group();
    res_grp_mgr->set_unsupport_reason("Thread pool plugin enabled");
  }

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  /*
    A value of the variable dd_upgrade_flag is reset after
    dd::init(dd::enum_dd_init_type::DD_POPULATE_UPGRADE) returned.
    So make its copy to call init_pfs_tables() with right argument value later.
  */
  bool dd_upgrade_was_initiated = dd::upgrade_57::in_progress();
#endif
  // Populate DD tables with meta data from 5.7 in case of upgrade
  if (!opt_help && dd::upgrade_57::in_progress() &&
      dd::init(dd::enum_dd_init_type::DD_POPULATE_UPGRADE)) {
    LogErr(ERROR_LEVEL, ER_DD_POPULATING_TABLES_FAILED);
    unireg_abort(1);
  }

  /*
    Store server and plugin IS tables metadata into new DD.
    This is done after all the plugins are registered.
  */
  if (!opt_help && !opt_initialize && !dd::upgrade_57::in_progress() &&
      dd::init(dd::enum_dd_init_type::DD_UPDATE_I_S_METADATA)) {
    LogErr(ERROR_LEVEL, ER_DD_UPDATING_PLUGIN_MD_FAILED);
    unireg_abort(MYSQLD_ABORT_EXIT);
  }

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  if (!opt_help) {
    bool st;
    if (opt_initialize || dd_upgrade_was_initiated)
      st = dd::performance_schema::init_pfs_tables(
          dd::enum_dd_init_type::DD_INITIALIZE);
    else
      st = dd::performance_schema::init_pfs_tables(
          dd::enum_dd_init_type::DD_RESTART_OR_UPGRADE);

    if (st) {
      LogErr(ERROR_LEVEL, ER_PERFSCHEMA_TABLES_INIT_FAILED);
      unireg_abort(1);
    }
  }
#endif

  auto res_grp_mgr = resourcegroups::Resource_group_mgr::instance();
  // Initialize the Resource group subsystem.
  if (!opt_help && !opt_initialize) {
    if (res_grp_mgr->post_init()) {
      LogErr(ERROR_LEVEL, ER_RESOURCE_GROUP_POST_INIT_FAILED);
      unireg_abort(MYSQLD_ABORT_EXIT);
    }
  }

  Session_tracker session_track_system_variables_check;
  LEX_STRING var_list;
  char *tmp_str;
  size_t len = strlen(global_system_variables.track_sysvars_ptr);
  tmp_str = (char *)my_malloc(PSI_NOT_INSTRUMENTED, len * sizeof(char) + 2,
                              MYF(MY_WME));
  strcpy(tmp_str, global_system_variables.track_sysvars_ptr);
  var_list.length = len;
  var_list.str = tmp_str;
  if (session_track_system_variables_check.server_boot_verify(
          system_charset_info, var_list)) {
    LogErr(ERROR_LEVEL, ER_TRACK_VARIABLES_BOGUS);
    if (tmp_str) my_free(tmp_str);
    unireg_abort(MYSQLD_ABORT_EXIT);
  }
  if (tmp_str) my_free(tmp_str);

  if (opt_help) unireg_abort(MYSQLD_SUCCESS_EXIT);

  /* if the errmsg.sys is not loaded, terminate to maintain behaviour */
  if (!my_default_lc_messages->errmsgs->is_loaded()) {
    LogErr(ERROR_LEVEL, ER_CANT_READ_ERRMSGS);
    unireg_abort(MYSQLD_ABORT_EXIT);
  }

  /* We have to initialize the storage engines before CSV logging */
  if (ha_init()) {
    LogErr(ERROR_LEVEL, ER_CANT_INIT_DBS);
    unireg_abort(MYSQLD_ABORT_EXIT);
  }

  if (opt_initialize) log_output_options = LOG_FILE;

  /*
    Issue a warning if there were specified additional options to the
    log-output along with NONE. Probably this wasn't what user wanted.
  */
  if ((log_output_options & LOG_NONE) && (log_output_options & ~LOG_NONE))
    LogErr(WARNING_LEVEL, ER_LOG_OUTPUT_CONTRADICTORY);

  if (log_output_options & LOG_TABLE) {
    /* Fall back to log files if the csv engine is not loaded. */
    LEX_CSTRING csv_name = {C_STRING_WITH_LEN("csv")};
    if (!plugin_is_ready(csv_name, MYSQL_STORAGE_ENGINE_PLUGIN)) {
      LogErr(ERROR_LEVEL, ER_NO_CSV_NO_LOG_TABLES);
      log_output_options = (log_output_options & ~LOG_TABLE) | LOG_FILE;
    }
  }

  query_logger.set_handlers(log_output_options);

  // Open slow log file if enabled.
  query_logger.set_log_file(QUERY_LOG_SLOW);
  if (opt_slow_log && query_logger.reopen_log_file(QUERY_LOG_SLOW))
    opt_slow_log = false;

  // Open general log file if enabled.
  query_logger.set_log_file(QUERY_LOG_GENERAL);
  if (opt_general_log && query_logger.reopen_log_file(QUERY_LOG_GENERAL))
    opt_general_log = false;

  /*
    Set the default storage engines
  */
  if (initialize_storage_engine(default_storage_engine, "",
                                &global_system_variables.table_plugin))
    unireg_abort(MYSQLD_ABORT_EXIT);
  if (initialize_storage_engine(default_tmp_storage_engine, " temp",
                                &global_system_variables.temp_table_plugin))
    unireg_abort(MYSQLD_ABORT_EXIT);

  if (!opt_initialize && !opt_noacl) {
    std::string disabled_se_str(opt_disabled_storage_engines);
    ha_set_normalized_disabled_se_str(disabled_se_str);

    // Log warning if default_storage_engine is a disabled storage engine.
    handlerton *default_se_handle =
        plugin_data<handlerton *>(global_system_variables.table_plugin);
    if (ha_is_storage_engine_disabled(default_se_handle))
      LogErr(WARNING_LEVEL, ER_DISABLED_STORAGE_ENGINE_AS_DEFAULT,
             "default_storage_engine", default_storage_engine);

    // Log warning if default_tmp_storage_engine is a disabled storage engine.
    handlerton *default_tmp_se_handle =
        plugin_data<handlerton *>(global_system_variables.temp_table_plugin);
    if (ha_is_storage_engine_disabled(default_tmp_se_handle))
      LogErr(WARNING_LEVEL, ER_DISABLED_STORAGE_ENGINE_AS_DEFAULT,
             "default_tmp_storage_engine", default_tmp_storage_engine);
  }

  if (total_ha_2pc > 1 || (1 == total_ha_2pc && opt_bin_log)) {
    if (opt_bin_log)
      tc_log = &mysql_bin_log;
    else
      tc_log = &tc_log_mmap;
  }

  if (tc_log->open(opt_bin_log ? opt_bin_logname : opt_tc_log_file)) {
    LogErr(ERROR_LEVEL, ER_CANT_INIT_TC_LOG);
    unireg_abort(MYSQLD_ABORT_EXIT);
  }
  (void)RUN_HOOK(server_state, before_recovery, (NULL));

  if (ha_recover(0)) {
    unireg_abort(MYSQLD_ABORT_EXIT);
  }

  if (dd::reset_tables_and_tablespaces()) {
    unireg_abort(MYSQLD_ABORT_EXIT);
  }
  ha_post_recover();

  /// @todo: this looks suspicious, revisit this /sven
  enum_gtid_mode gtid_mode = get_gtid_mode(GTID_MODE_LOCK_NONE);

  if (gtid_mode == GTID_MODE_ON &&
      _gtid_consistency_mode != GTID_CONSISTENCY_MODE_ON) {
    LogErr(ERROR_LEVEL, ER_RPL_GTID_MODE_REQUIRES_ENFORCE_GTID_CONSISTENCY_ON);
    unireg_abort(MYSQLD_ABORT_EXIT);
  }

  if (opt_bin_log) {
    /*
      Configures what object is used by the current log to store processed
      gtid(s). This is necessary in the MYSQL_BIN_LOG::MYSQL_BIN_LOG to
      corretly compute the set of previous gtids.
    */
    DBUG_ASSERT(!mysql_bin_log.is_relay_log);
    mysql_mutex_t *log_lock = mysql_bin_log.get_log_lock();
    mysql_mutex_lock(log_lock);

    if (mysql_bin_log.open_binlog(opt_bin_logname, 0, max_binlog_size, false,
                                  true /*need_lock_index=true*/,
                                  true /*need_sid_lock=true*/, NULL)) {
      mysql_mutex_unlock(log_lock);
      unireg_abort(MYSQLD_ABORT_EXIT);
    }
    mysql_mutex_unlock(log_lock);
  }

  /*
    When we pass non-zero values for both expire_logs_days and
    binlog_expire_logs_seconds at the server start-up, the value of
    expire_logs_days will be ignored and only binlog_expire_logs_seconds
    will be used.
  */
  if (binlog_expire_logs_seconds_supplied && expire_logs_days_supplied) {
    if (binlog_expire_logs_seconds != 0 && expire_logs_days != 0) {
      LogErr(WARNING_LEVEL, ER_EXPIRE_LOGS_DAYS_IGNORED);
      expire_logs_days = 0;
    }
  } else if (expire_logs_days_supplied)
    binlog_expire_logs_seconds = 0;
  DBUG_ASSERT(expire_logs_days == 0 || binlog_expire_logs_seconds == 0);

  if (opt_bin_log) {
    if (expire_logs_days > 0 || binlog_expire_logs_seconds > 0) {
      time_t purge_time = my_time(0) - binlog_expire_logs_seconds -
                          expire_logs_days * 24 * 60 * 60;
      mysql_bin_log.purge_logs_before_date(purge_time, true);
    }
  } else {
    if (binlog_expire_logs_seconds_supplied)
      LogErr(WARNING_LEVEL, ER_NEED_LOG_BIN, "--binlog-expire-logs-seconds");
    if (expire_logs_days_supplied)
      LogErr(WARNING_LEVEL, ER_NEED_LOG_BIN, "--expire_logs_days");
  }

  if (opt_myisam_log) (void)mi_log(1);

#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
  if (locked_in_memory && !getuid()) {
    if (setreuid((uid_t)-1, 0) == -1) {  // this should never happen
      LogErr(ERROR_LEVEL, ER_FAIL_SETREUID, strerror(errno));
      unireg_abort(MYSQLD_ABORT_EXIT);
    }
    if (mlockall(MCL_CURRENT)) {
      LogErr(WARNING_LEVEL, ER_FAILED_TO_LOCK_MEM,
             errno); /* purecov: inspected */
      locked_in_memory = 0;
    }
#ifndef _WIN32
    if (user_info) set_user(mysqld_user, user_info);
#endif
  } else
#endif
    locked_in_memory = 0;

  /* Initialize the optimizer cost module */
  init_optimizer_cost_module(true);
  ft_init_stopwords();

  init_max_user_conn();

  DBUG_RETURN(0);
}

#ifdef _WIN32

extern "C" void *handle_shutdown_and_restart(void *arg) {
  MSG msg;
  HANDLE event_handles[2];
  event_handles[0] = hEventShutdown;
  event_handles[1] = hEventRestart;

  my_thread_init();
  /* This call should create the message queue for this thread. */
  PeekMessage(&msg, NULL, 1, 65534, PM_NOREMOVE);
  DWORD ret_code = WaitForMultipleObjects(
      2, static_cast<HANDLE *>(event_handles), FALSE, INFINITE);

  if (ret_code == WAIT_OBJECT_0 || ret_code == WAIT_OBJECT_0 + 1) {
    if (ret_code == WAIT_OBJECT_0)
      LogErr(SYSTEM_LEVEL, ER_NORMAL_SERVER_SHUTDOWN, my_progname);
    else
      signal_hand_thr_exit_code = MYSQLD_RESTART_EXIT;

    set_connection_events_loop_aborted(true);
    close_connections();
    my_thread_end();
    my_thread_exit(0);
  }
  return 0;
}

static void create_shutdown_and_restart_thread() {
  DBUG_ENTER("create_shutdown_and_restart_thread");

  const char *errmsg;
  my_thread_attr_t thr_attr;
  SECURITY_ATTRIBUTES *shutdown_sec_attr;

  my_security_attr_create(&shutdown_sec_attr, &errmsg, GENERIC_ALL,
                          SYNCHRONIZE | EVENT_MODIFY_STATE);

  hEventShutdown =
      CreateEvent(shutdown_sec_attr, FALSE, FALSE, shutdown_event_name);
  hEventRestart = CreateEvent(0, FALSE, FALSE, restart_event_name);

  my_thread_attr_init(&thr_attr);

  if (my_thread_create(&shutdown_restart_thr_handle, &thr_attr,
                       handle_shutdown_and_restart, 0))
    LogErr(WARNING_LEVEL, ER_CANT_CREATE_SHUTDOWN_THREAD, errno);

  my_security_attr_free(shutdown_sec_attr);
  my_thread_attr_destroy(&thr_attr);
}
#endif /* _WIN32 */

#ifndef DBUG_OFF
/*
  Debugging helper function to keep the locale database
  (see sql_locale.cc) and max_month_name_length and
  max_day_name_length variable values in consistent state.
*/
static void test_lc_time_sz() {
  DBUG_ENTER("test_lc_time_sz");
  for (MY_LOCALE **loc = my_locales; *loc; loc++) {
    size_t max_month_len = 0;
    size_t max_day_len = 0;
    for (const char **month = (*loc)->month_names->type_names; *month;
         month++) {
      set_if_bigger(max_month_len,
                    my_numchars_mb(&my_charset_utf8_general_ci, *month,
                                   *month + strlen(*month)));
    }
    for (const char **day = (*loc)->day_names->type_names; *day; day++) {
      set_if_bigger(max_day_len, my_numchars_mb(&my_charset_utf8_general_ci,
                                                *day, *day + strlen(*day)));
    }
    if ((*loc)->max_month_name_length != max_month_len ||
        (*loc)->max_day_name_length != max_day_len) {
      DBUG_PRINT("Wrong max day name(or month name) length for locale:",
                 ("%s", (*loc)->name));
      DBUG_ASSERT(0);
    }
  }
  DBUG_VOID_RETURN;
}
#endif  // DBUG_OFF

/*
  @brief : Set opt_super_readonly to user supplied value before
           enabling communication channels to accept user connections
*/

static void set_super_read_only_post_init() {
  opt_super_readonly = super_read_only;
}

#ifdef _WIN32
int win_main(int argc, char **argv)
#else
int mysqld_main(int argc, char **argv)
#endif
{
  // Substitute the full path to the executable in argv[0]
  substitute_progpath(argv);
  sysd::notify_connect();
  sysd::notify("STATUS=SERVER_BOOTING\n");

  /*
    Perform basic thread library and malloc initialization,
    to be able to read defaults files and parse options.
  */
  my_progname = argv[0];

#ifndef _WIN32
#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  pre_initialize_performance_schema();
#endif /*WITH_PERFSCHEMA_STORAGE_ENGINE */
  // For windows, my_init() is called from the win specific mysqld_main
  if (my_init())  // init my_sys library & pthreads
  {
    LogErr(ERROR_LEVEL, ER_MYINIT_FAILED);
    flush_error_log_messages();
    return 1;
  }
#endif /* _WIN32 */

  orig_argc = argc;
  orig_argv = argv;
  my_getopt_use_args_separator = true;
  my_defaults_read_login_file = false;
  if (load_defaults(MYSQL_CONFIG_NAME, load_default_groups, &argc, &argv,
                    &argv_alloc)) {
    flush_error_log_messages();
    return 1;
  }

  /* Set data dir directory paths */
  strmake(mysql_real_data_home, get_relative_path(MYSQL_DATADIR),
          sizeof(mysql_real_data_home) - 1);

  /*
   Initialize variables cache for persisted variables, load persisted
   config file and append read only persisted variables to command line
   options if present.
  */
  if (persisted_variables_cache.init(&argc, &argv) ||
      persisted_variables_cache.load_persist_file() ||
      persisted_variables_cache.append_read_only_variables(&argc, &argv))
    return 1;
  my_getopt_use_args_separator = false;
  remaining_argc = argc;
  remaining_argv = argv;

  init_variable_default_paths();

  /* Must be initialized early for comparison of options name */
  system_charset_info = &my_charset_utf8_general_ci;

  /* Write mysys error messages to the error log. */
  local_message_hook = error_log_printf;

  int ho_error;

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  /*
    Initialize the array of performance schema instrument configurations.
  */
  init_pfs_instrument_array();
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

  ho_error = handle_early_options();

  init_sql_statement_names();
  sys_var_init();
  ulong requested_open_files;
  init_error_log();
  adjust_related_options(&requested_open_files);

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  if (ho_error == 0) {
    if (!opt_help && !opt_initialize) {
      int pfs_rc;
      /* Add sizing hints from the server sizing parameters. */
      pfs_param.m_hints.m_table_definition_cache = table_def_size;
      pfs_param.m_hints.m_table_open_cache = table_cache_size;
      pfs_param.m_hints.m_max_connections = max_connections;
      pfs_param.m_hints.m_open_files_limit = requested_open_files;
      pfs_param.m_hints.m_max_prepared_stmt_count = max_prepared_stmt_count;

      pfs_rc = initialize_performance_schema(
          &pfs_param, &psi_thread_hook, &psi_mutex_hook, &psi_rwlock_hook,
          &psi_cond_hook, &psi_file_hook, &psi_socket_hook, &psi_table_hook,
          &psi_mdl_hook, &psi_idle_hook, &psi_stage_hook, &psi_statement_hook,
          &psi_transaction_hook, &psi_memory_hook, &psi_error_hook,
          &psi_data_lock_hook, &psi_system_hook);
      if ((pfs_rc != 0) && pfs_param.m_enabled) {
        pfs_param.m_enabled = false;
        LogErr(WARNING_LEVEL, ER_PERFSCHEMA_INIT_FAILED);
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

  void *service;

  if (psi_thread_hook != NULL) {
    service = psi_thread_hook->get_interface(PSI_CURRENT_THREAD_VERSION);
    if (service != NULL) {
      set_psi_thread_service(service);
    }
  }

  if (psi_mutex_hook != NULL) {
    service = psi_mutex_hook->get_interface(PSI_CURRENT_MUTEX_VERSION);
    if (service != NULL) {
      set_psi_mutex_service(service);
    }
  }

  if (psi_rwlock_hook != NULL) {
    service = psi_rwlock_hook->get_interface(PSI_CURRENT_RWLOCK_VERSION);
    if (service != NULL) {
      set_psi_rwlock_service(service);
    }
  }

  if (psi_cond_hook != NULL) {
    service = psi_cond_hook->get_interface(PSI_CURRENT_COND_VERSION);
    if (service != NULL) {
      set_psi_cond_service(service);
    }
  }

  if (psi_file_hook != NULL) {
    service = psi_file_hook->get_interface(PSI_CURRENT_FILE_VERSION);
    if (service != NULL) {
      set_psi_file_service(service);
    }
  }

  if (psi_socket_hook != NULL) {
    service = psi_socket_hook->get_interface(PSI_CURRENT_SOCKET_VERSION);
    if (service != NULL) {
      set_psi_socket_service(service);
    }
  }

  if (psi_table_hook != NULL) {
    service = psi_table_hook->get_interface(PSI_CURRENT_TABLE_VERSION);
    if (service != NULL) {
      set_psi_table_service(service);
    }
  }

  if (psi_mdl_hook != NULL) {
    service = psi_mdl_hook->get_interface(PSI_CURRENT_MDL_VERSION);
    if (service != NULL) {
      set_psi_mdl_service(service);
    }
  }

  if (psi_idle_hook != NULL) {
    service = psi_idle_hook->get_interface(PSI_CURRENT_IDLE_VERSION);
    if (service != NULL) {
      set_psi_idle_service(service);
    }
  }

  if (psi_stage_hook != NULL) {
    service = psi_stage_hook->get_interface(PSI_CURRENT_STAGE_VERSION);
    if (service != NULL) {
      set_psi_stage_service(service);
    }
  }

  if (psi_statement_hook != NULL) {
    service = psi_statement_hook->get_interface(PSI_CURRENT_STATEMENT_VERSION);
    if (service != NULL) {
      set_psi_statement_service(service);
    }
  }

  if (psi_transaction_hook != NULL) {
    service =
        psi_transaction_hook->get_interface(PSI_CURRENT_TRANSACTION_VERSION);
    if (service != NULL) {
      set_psi_transaction_service(service);
    }
  }

  if (psi_memory_hook != NULL) {
    service = psi_memory_hook->get_interface(PSI_CURRENT_MEMORY_VERSION);
    if (service != NULL) {
      set_psi_memory_service(service);
    }
  }

  if (psi_error_hook != NULL) {
    service = psi_error_hook->get_interface(PSI_CURRENT_ERROR_VERSION);
    if (service != NULL) {
      set_psi_error_service(service);
    }
  }

  if (psi_data_lock_hook != NULL) {
    service = psi_data_lock_hook->get_interface(PSI_CURRENT_DATA_LOCK_VERSION);
    if (service != NULL) {
      set_psi_data_lock_service(service);
    }
  }

  if (psi_system_hook != NULL) {
    service = psi_system_hook->get_interface(PSI_CURRENT_SYSTEM_VERSION);
    if (service != NULL) {
      set_psi_system_service(service);
    }
  }

  /*
    Now that we have parsed the command line arguments, and have initialized
    the performance schema itself, the next step is to register all the
    server instruments.
  */
  init_server_psi_keys();

  /*
    Now that some instrumentation is in place,
    recreate objects which were initialised early,
    so that they are instrumented as well.
  */
  my_thread_global_reinit();
#endif /* HAVE_PSI_INTERFACE */

  /*
    Initialize Components core subsystem early on, once we have PSI, which it
    uses. This part doesn't use any more MySQL-specific functionalities but
    error logging and PFS.
  */
  if (component_infrastructure_init()) unireg_abort(MYSQLD_ABORT_EXIT);

    /*
      Initialize Performance Schema component services.
    */
#ifdef HAVE_PSI_THREAD_INTERFACE
  if (!opt_help && !opt_initialize) {
    register_pfs_notification_service();
    register_pfs_resource_group_service();
  }
#endif

  // Initialize the resource group subsystem.
  auto res_grp_mgr = resourcegroups::Resource_group_mgr::instance();
  if (!opt_help && !opt_initialize) {
    if (res_grp_mgr->init()) {
      LogErr(ERROR_LEVEL, ER_RESOURCE_GROUP_SUBSYSTEM_INIT_FAILED);
      unireg_abort(MYSQLD_ABORT_EXIT);
    }
  }

#ifdef HAVE_PSI_THREAD_INTERFACE
  /* Instrument the main thread */
  PSI_thread *psi = PSI_THREAD_CALL(new_thread)(key_thread_main, NULL, 0);
  PSI_THREAD_CALL(set_thread_os_id)(psi);
  PSI_THREAD_CALL(set_thread)(psi);
#endif /* HAVE_PSI_THREAD_INTERFACE */

  /* Initialize audit interface globals. Audit plugins are inited later. */
  mysql_audit_initialize();

  Srv_session::module_init();

  /*
    Perform basic query log initialization. Should be called after
    MY_INIT, as it initializes mutexes.
  */
  query_logger.init();

  if (ho_error) {
    /*
      Parsing command line option failed,
      Since we don't have a workable remaining_argc/remaining_argv
      to continue the server initialization, this is as far as this
      code can go.
      This is the best effort to log meaningful messages:
      - messages will be printed to stderr, which is not redirected yet,
      - messages will be printed in the NT event log, for windows.
    */
    flush_error_log_messages();
    /*
      Not enough initializations for unireg_abort()
      Using exit() for windows.
    */
    exit(MYSQLD_ABORT_EXIT);
  }

  if (init_common_variables()) unireg_abort(MYSQLD_ABORT_EXIT);  // Will do exit

  my_init_signals();

  size_t guardize = 0;
#ifndef _WIN32
  int retval = pthread_attr_getguardsize(&connection_attrib, &guardize);
  DBUG_ASSERT(retval == 0);
  if (retval != 0) guardize = my_thread_stack_size;
#endif

#if defined(__ia64__) || defined(__ia64)
  /*
    Peculiar things with ia64 platforms - it seems we only have half the
    stack size in reality, so we have to double it here
  */
  guardize = my_thread_stack_size;
#endif

  if (0 != my_thread_attr_setstacksize(&connection_attrib,
                                       my_thread_stack_size + guardize)) {
    DBUG_ASSERT(false);
  }

  {
    /* Retrieve used stack size;  Needed for checking stack overflows */
    size_t stack_size = 0;
    my_thread_attr_getstacksize(&connection_attrib, &stack_size);

    /* We must check if stack_size = 0 as Solaris 2.9 can return 0 here */
    if (stack_size && stack_size < (my_thread_stack_size + guardize)) {
      LogErr(WARNING_LEVEL, ER_STACKSIZE_UNEXPECTED,
             my_thread_stack_size + guardize, (long)stack_size);
#if defined(__ia64__) || defined(__ia64)
      my_thread_stack_size = stack_size / 2;
#else
      my_thread_stack_size = static_cast<ulong>(stack_size - guardize);
#endif
    }
  }

#ifndef DBUG_OFF
  test_lc_time_sz();
  srand(static_cast<uint>(time(NULL)));
#endif

#if !defined(_WIN32)

  if (opt_initialize && opt_daemonize) {
    fprintf(stderr, "Initialize and daemon options are incompatible.\n");
    unireg_abort(MYSQLD_ABORT_EXIT);
  }

  if (opt_daemonize && log_error_dest == disabled_my_option &&
      (isatty(STDOUT_FILENO) || isatty(STDERR_FILENO))) {
    // Just use the default in this case.
    log_error_dest = "";
  }

  if (opt_daemonize) {
    if (chdir("/") < 0) {
      LogErr(ERROR_LEVEL, ER_CANNOT_CHANGE_TO_ROOT_DIR, strerror(errno));
      unireg_abort(MYSQLD_ABORT_EXIT);
    }

    if ((pipe_write_fd = mysqld::runtime::mysqld_daemonize()) < -1) {
      LogErr(ERROR_LEVEL, ER_FAILED_START_MYSQLD_DAEMON);
      unireg_abort(MYSQLD_ABORT_EXIT);
    }

    if (pipe_write_fd < 0) {
      // This is the launching process and the daemon appears to have
      // started ok (Need to call unireg_abort with success here to
      // clean up resources in the lauching process.
      unireg_abort(MYSQLD_SUCCESS_EXIT);
    }

    // Need to update the value of current_pid so that it reflects the
    // pid of the daemon (the previous value was set by unireg_init()
    // while still in the launcher process.
    current_pid = static_cast<ulong>(getpid());
  }
#endif

#ifndef _WIN32
  if ((user_info = check_user(mysqld_user))) {
#if HAVE_CHOWN
    if (unlikely(opt_initialize)) {
      /* need to change the owner of the freshly created data directory */
      MY_STAT stat;
      char errbuf[MYSYS_STRERROR_SIZE];
      bool must_chown = true;

      /* fetch the directory's owner */
      if (!my_stat(mysql_real_data_home, &stat, MYF(0))) {
        LogErr(INFORMATION_LEVEL, ER_CANT_STAT_DATADIR, my_errno(),
               my_strerror(errbuf, sizeof(errbuf), my_errno()));
      }
      /* Don't change it if it's already the same as SElinux stops this */
      else if (stat.st_uid == user_info->pw_uid &&
               stat.st_gid == user_info->pw_gid)
        must_chown = false;

      if (must_chown &&
          chown(mysql_real_data_home, user_info->pw_uid, user_info->pw_gid)) {
        LogErr(ERROR_LEVEL, ER_CANT_CHOWN_DATADIR, mysqld_user);
        unireg_abort(1);
      }
    }
#endif

#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
    if (locked_in_memory)  // getuid() == 0 here
      set_effective_user(user_info);
    else
#endif
      set_user(mysqld_user, user_info);
  }
#endif  // !_WIN32

  /*
   initiate key migration if any one of the migration specific
   options are provided.
  */
  if (opt_keyring_migration_source || opt_keyring_migration_destination ||
      migrate_connect_options) {
    Migrate_keyring mk;
    my_getopt_skip_unknown = TRUE;
    if (mk.init(remaining_argc, remaining_argv, opt_keyring_migration_source,
                opt_keyring_migration_destination, opt_keyring_migration_user,
                opt_keyring_migration_host, opt_keyring_migration_password,
                opt_keyring_migration_socket, opt_keyring_migration_port)) {
      LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATION_FAILED);
      log_error_dest = "stderr";
      flush_error_log_messages();
      unireg_abort(MYSQLD_ABORT_EXIT);
    }

    if (mk.execute()) {
      LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATION_FAILED);
      log_error_dest = "stderr";
      flush_error_log_messages();
      unireg_abort(MYSQLD_ABORT_EXIT);
    }

    my_getopt_skip_unknown = 0;
    LogErr(INFORMATION_LEVEL, ER_KEYRING_MIGRATION_SUCCESSFUL);
    log_error_dest = "stderr";
    flush_error_log_messages();
    unireg_abort(MYSQLD_SUCCESS_EXIT);
  }

  /*
   We have enough space for fiddling with the argv, continue
  */
  if (!opt_help && my_setwd(mysql_real_data_home, MYF(MY_WME))) {
    LogErr(ERROR_LEVEL, ER_CANT_SET_DATADIR, mysql_real_data_home);
    unireg_abort(MYSQLD_ABORT_EXIT); /* purecov: inspected */
  }

    /*
     The subsequent calls may take a long time : e.g. innodb log read.
     Thus set the long running service control manager timeout
    */
#if defined(_WIN32)
  if (windows_service) {
    if (setup_service_status_cmd_processed_handle())
      unireg_abort(MYSQLD_ABORT_EXIT);

    char buf[32];
    snprintf(buf, sizeof(buf), "T %lu", slow_start_timeout);
    Service_status_msg msg(buf);
    send_service_status(msg);
  }
#endif

  if (init_server_components()) unireg_abort(MYSQLD_ABORT_EXIT);

  /*
    Each server should have one UUID. We will create it automatically, if it
    does not exist.
   */
  if (init_server_auto_options()) {
    LogErr(ERROR_LEVEL, ER_CANT_CREATE_UUID);
    unireg_abort(MYSQLD_ABORT_EXIT);
  }

  if (!server_id_supplied)
    LogErr(INFORMATION_LEVEL, ER_WARN_NO_SERVERID_SPECIFIED);

  /*
    Add server_uuid to the sid_map.  This must be done after
    server_uuid has been initialized in init_server_auto_options and
    after the binary log (and sid_map file) has been initialized in
    init_server_components().

    No error message is needed: init_sid_map() prints a message.

    Strictly speaking, this is not currently needed when
    opt_bin_log==0, since the variables that gtid_state->init
    initializes are not currently used in that case.  But we call it
    regardless to avoid possible future bugs if gtid_state ever
    needs to do anything else.
  */
  global_sid_lock->wrlock();
  int gtid_ret = gtid_state->init();
  global_sid_lock->unlock();

  if (gtid_ret) unireg_abort(MYSQLD_ABORT_EXIT);

  if (!opt_initialize && !opt_initialize_insecure) {
    // Initialize executed_gtids from mysql.gtid_executed table.
    if (gtid_state->read_gtid_executed_from_table() == -1) unireg_abort(1);
  }

  if (opt_bin_log) {
    /*
      Initialize GLOBAL.GTID_EXECUTED and GLOBAL.GTID_PURGED from
      gtid_executed table and binlog files during server startup.
    */
    Gtid_set *executed_gtids =
        const_cast<Gtid_set *>(gtid_state->get_executed_gtids());
    Gtid_set *lost_gtids = const_cast<Gtid_set *>(gtid_state->get_lost_gtids());
    Gtid_set *gtids_only_in_table =
        const_cast<Gtid_set *>(gtid_state->get_gtids_only_in_table());
    Gtid_set *previous_gtids_logged =
        const_cast<Gtid_set *>(gtid_state->get_previous_gtids_logged());

    Gtid_set purged_gtids_from_binlog(global_sid_map, global_sid_lock);
    Gtid_set gtids_in_binlog(global_sid_map, global_sid_lock);
    Gtid_set gtids_in_binlog_not_in_table(global_sid_map, global_sid_lock);

    if (mysql_bin_log.init_gtid_sets(
            &gtids_in_binlog, &purged_gtids_from_binlog,
            opt_master_verify_checksum, true /*true=need lock*/,
            NULL /*trx_parser*/, NULL /*partial_trx*/,
            true /*is_server_starting*/))
      unireg_abort(MYSQLD_ABORT_EXIT);

    global_sid_lock->wrlock();

    purged_gtids_from_binlog.dbug_print("purged_gtids_from_binlog");
    gtids_in_binlog.dbug_print("gtids_in_binlog");

    if (!gtids_in_binlog.is_empty() &&
        !gtids_in_binlog.is_subset(executed_gtids)) {
      gtids_in_binlog_not_in_table.add_gtid_set(&gtids_in_binlog);
      if (!executed_gtids->is_empty())
        gtids_in_binlog_not_in_table.remove_gtid_set(executed_gtids);
      /*
        Save unsaved GTIDs into gtid_executed table, in the following
        four cases:
          1. the upgrade case.
          2. the case that a slave is provisioned from a backup of
             the master and the slave is cleaned by RESET MASTER
             and RESET SLAVE before this.
          3. the case that no binlog rotation happened from the
             last RESET MASTER on the server before it crashes.
          4. The set of GTIDs of the last binlog is not saved into the
             gtid_executed table if server crashes, so we save it into
             gtid_executed table and executed_gtids during recovery
             from the crash.
      */
      if (gtid_state->save(&gtids_in_binlog_not_in_table) == -1) {
        global_sid_lock->unlock();
        unireg_abort(MYSQLD_ABORT_EXIT);
      }
      executed_gtids->add_gtid_set(&gtids_in_binlog_not_in_table);
    }

    /* gtids_only_in_table= executed_gtids - gtids_in_binlog */
    if (gtids_only_in_table->add_gtid_set(executed_gtids) != RETURN_STATUS_OK) {
      global_sid_lock->unlock();
      unireg_abort(MYSQLD_ABORT_EXIT);
    }
    gtids_only_in_table->remove_gtid_set(&gtids_in_binlog);
    /*
      lost_gtids = executed_gtids -
                   (gtids_in_binlog - purged_gtids_from_binlog)
                 = gtids_only_in_table + purged_gtids_from_binlog;
    */
    DBUG_ASSERT(lost_gtids->is_empty());
    if (lost_gtids->add_gtid_set(gtids_only_in_table) != RETURN_STATUS_OK ||
        lost_gtids->add_gtid_set(&purged_gtids_from_binlog) !=
            RETURN_STATUS_OK) {
      global_sid_lock->unlock();
      unireg_abort(MYSQLD_ABORT_EXIT);
    }

    /* Prepare previous_gtids_logged for next binlog */
    if (previous_gtids_logged->add_gtid_set(&gtids_in_binlog) !=
        RETURN_STATUS_OK) {
      global_sid_lock->unlock();
      unireg_abort(MYSQLD_ABORT_EXIT);
    }

    /*
      Write the previous set of gtids at this point because during
      the creation of the binary log this is not done as we cannot
      move the init_gtid_sets() to a place before openning the binary
      log. This requires some investigation.

      /Alfranio
    */
    Previous_gtids_log_event prev_gtids_ev(&gtids_in_binlog);

    global_sid_lock->unlock();

    (prev_gtids_ev.common_footer)->checksum_alg =
        static_cast<enum_binlog_checksum_alg>(binlog_checksum_options);

    if (prev_gtids_ev.write(mysql_bin_log.get_log_file()))
      unireg_abort(MYSQLD_ABORT_EXIT);
    mysql_bin_log.add_bytes_written(prev_gtids_ev.common_header->data_written);

    if (flush_io_cache(mysql_bin_log.get_log_file()) ||
        mysql_file_sync(mysql_bin_log.get_log_file()->file, MYF(MY_WME)))
      unireg_abort(MYSQLD_ABORT_EXIT);
    mysql_bin_log.update_binlog_end_pos();

    (void)RUN_HOOK(server_state, after_engine_recovery, (NULL));
  }

  if (init_ssl_communication()) unireg_abort(MYSQLD_ABORT_EXIT);
  if (network_init()) unireg_abort(MYSQLD_ABORT_EXIT);

#ifdef _WIN32
  if (opt_require_secure_transport && !opt_enable_shared_memory &&
      !opt_use_ssl && !opt_initialize) {
    LogErr(ERROR_LEVEL, ER_TRANSPORTS_WHAT_TRANSPORTS);
    unireg_abort(MYSQLD_ABORT_EXIT);
  }
#endif

  /*
   Initialize my_str_malloc(), my_str_realloc() and my_str_free()
  */
  my_str_malloc = &my_str_malloc_mysqld;
  my_str_free = &my_str_free_mysqld;
  my_str_realloc = &my_str_realloc_mysqld;

  error_handler_hook = my_message_sql;

  bool abort = false;

  /* Save pid of this process in a file */
  if (!opt_initialize) {
    if (create_pid_file()) abort = true;
  }

  /* Read the optimizer cost model configuration tables */
  if (!opt_initialize) reload_optimizer_cost_constants();

  if (
      /*
        Read components table to restore previously installed components. This
        requires read access to mysql.component table.
      */
      (!opt_initialize && mysql_component_infrastructure_init()) ||
      mysql_rm_tmp_tables()) {
    abort = true;
  }

  /* we do want to exit if there are any other unknown options */
  if (remaining_argc > 1) {
    int ho_error;
    struct my_option no_opts[] = {
        {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};
    /*
      We need to eat any 'loose' arguments first before we conclude
      that there are unprocessed options.
    */
    my_getopt_skip_unknown = 0;

    if ((ho_error = handle_options(&remaining_argc, &remaining_argv, no_opts,
                                   mysqld_get_one_option)))
      abort = true;
    else {
      /* Add back the program name handle_options removes */
      remaining_argc++;
      remaining_argv--;
      my_getopt_skip_unknown = true;

      if (remaining_argc > 1) {
        LogErr(ERROR_LEVEL, ER_EXCESS_ARGUMENTS, remaining_argv[1]);
        LogErr(INFORMATION_LEVEL, ER_VERBOSE_HINT);
        abort = true;
      }
    }
  }

  if (abort || acl_init(opt_noacl)) {
    /*
      During upgrade we might be missing the mysql.global_grants table
      which is opened during acl_reload along with all the other core privilege
      tables. If this operation fails we simply disable the privilege system
      and issue a warning.
    */
    opt_noacl = true;
    LogErr(WARNING_LEVEL, ER_PRIVILEGE_SYSTEM_INIT_FAILED);
  }
  if (abort || my_tz_init((THD *)0, default_tz_name, opt_initialize) ||
      grant_init(opt_noacl)) {
    set_connection_events_loop_aborted(true);

    delete_pid_file(MYF(MY_WME));

    unireg_abort(MYSQLD_ABORT_EXIT);
  }

  /*
    Activate loadable error logging components, if any.
  */
  if (log_builtins_error_stack(opt_log_error_services, true) >= 0) {
    // Syntax is OK and services exist; let's try to initialize them:
    int rr = log_builtins_error_stack(opt_log_error_services, false);

    // Well, that didn't work. Print diagnostics and bail.
    if (rr < 0) {
      char *problem = opt_log_error_services;
      const char *var_name = "log_error_services";

      rr = -(rr + 1);

      if (((size_t)rr) < strlen(opt_log_error_services))
        problem = &((char *)opt_log_error_services)[rr];

      /*
        Try to fall back to default error logging stack.
        If that's possible, print diagnostics there, then exit.
      */
      sys_var *var = intern_find_sys_var(var_name, strlen(var_name));

      if (var != nullptr) {
        opt_log_error_services = (char *)var->get_default();
        if (log_builtins_error_stack(opt_log_error_services, false) >= 0) {
          LogErr(ERROR_LEVEL, ER_CANT_START_ERROR_LOG_SERVICE, var_name,
                 problem);
          unireg_abort(MYSQLD_ABORT_EXIT);
        }
      }

      /*
        We failed to set the default error logging stack. At this point,
        we don't know whether ANY of the requested sinks work,
        so our best bet is to write directly to the error stream.
        Then, we abort.
      */
      {
        char buff[512];
        size_t len;

        len = snprintf(buff, sizeof(buff),
                       ER_DEFAULT(ER_CANT_START_ERROR_LOG_SERVICE), var_name,
                       problem);
        len = std::min(len, sizeof(buff) - 1);

        log_write_errstream(buff, len);

        unireg_abort(MYSQLD_ABORT_EXIT);
      }
    }
  } else {
    LogErr(INFORMATION_LEVEL, ER_CANNOT_SET_LOG_ERROR_SERVICES,
           opt_log_error_services);
    /*
      We were given an illegal value at start-up, so the default was
      used instead. We have reported the problem (and the dodgy value);
      let's now point our variable back at the default (i.e. the value
      actually used) so SELECT @@GLOBAL.log_error_services will render
      correct results.
    */
    sys_var *var = intern_find_sys_var(STRING_WITH_LEN("log_error_services"));
    if (var != nullptr) opt_log_error_services = (char *)var->get_default();
  }

  /*
    Bootstrap the dynamic privilege service implementation
  */
  if (dynamic_privilege_init()) {
    LogErr(WARNING_LEVEL, ER_PERSISTENT_PRIVILEGES_BOOTSTRAP);
  }

  if (!opt_initialize) servers_init(0);

  if (!opt_noacl) {
    udf_read_functions_table();
  }

  init_status_vars();
  /* If running with --initialize, do not start replication. */
  if (opt_initialize) opt_skip_slave_start = 1;

  check_binlog_cache_size(NULL);
  check_binlog_stmt_cache_size(NULL);

  binlog_unsafe_map_init();

  /* If running with --initialize, do not start replication. */
  if (!opt_initialize) {
    // Make @@slave_skip_errors show the nice human-readable value.
    set_slave_skip_errors(&opt_slave_skip_errors);
    /*
      Group replication filters should be discarded before init_slave(),
      otherwise the pre-configured filters will be referenced by group
      replication channels.
    */
    rpl_channel_filters.discard_group_replication_filters();

    /*
      init_slave() must be called after the thread keys are created.
    */
    if (server_id != 0)
      init_slave(); /* Ignoring errors while configuring replication. */

    /*
      If the user specifies a per-channel replication filter through a
      command-line option (or in a configuration file) for a slave
      replication channel which does not exist as of now (i.e not
      present in slave info tables yet), then the per-channel
      replication filter is discarded with a warning.
      If the user specifies a per-channel replication filter through
      a command-line option (or in a configuration file) for group
      replication channels 'group_replication_recovery' and
      'group_replication_applier' which is disallowed, then the
      per-channel replication filter is discarded with a warning.
    */
    rpl_channel_filters.discard_all_unattached_filters();
  }

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  initialize_performance_schema_acl(opt_initialize);
#endif

  initialize_information_schema_acl();

  (void)RUN_HOOK(server_state, after_recovery, (NULL));

  if (Events::init(opt_noacl || opt_initialize))
    unireg_abort(MYSQLD_ABORT_EXIT);

#ifndef _WIN32
  //  Start signal handler thread.
  start_signal_handler();
#endif

  /* set all persistent options */
  if (persisted_variables_cache.set_persist_options()) {
    LogErr(ERROR_LEVEL, ER_CANT_SET_UP_PERSISTED_VALUES);
    return 1;
  }

  if (opt_initialize) {
    // Make sure we can process SIGHUP during bootstrap.
    server_components_initialized();

    int error = bootstrap::run_bootstrap_thread(
        mysql_stdin, NULL, SYSTEM_THREAD_SERVER_INITIALIZE);
    if (error == 0) {
      LogErr(SYSTEM_LEVEL, ER_ENDING_INIT, my_progname, server_version);
    }
    unireg_abort(error ? MYSQLD_ABORT_EXIT : MYSQLD_SUCCESS_EXIT);
  }

  if (opt_init_file && *opt_init_file) {
    if (read_init_file(opt_init_file)) unireg_abort(MYSQLD_ABORT_EXIT);
  }

  /*
    Event must be invoked after error_handler_hook is assigned to
    my_message_sql, otherwise my_message will not cause the event to abort.
  */
  if (mysql_audit_notify(AUDIT_EVENT(MYSQL_AUDIT_SERVER_STARTUP_STARTUP),
                         (const char **)argv, argc))
    unireg_abort(MYSQLD_ABORT_EXIT);

#ifdef _WIN32
  create_shutdown_and_restart_thread();
#endif
  start_handle_manager();

  create_compress_gtid_table_thread();

  LogEvent()
      .type(LOG_TYPE_ERROR)
      .subsys(LOG_SUBSYSTEM_TAG)
      .prio(SYSTEM_LEVEL)
      .lookup(ER_SERVER_STARTUP_MSG, my_progname, server_version,
#ifdef HAVE_SYS_UN_H
              (opt_initialize ? (char *)"" : mysqld_unix_port),
#else
              (char *)"",
#endif
              mysqld_port, MYSQL_COMPILATION_COMMENT);

#if defined(_WIN32)
  if (windows_service) {
    Service_status_msg s("R");
    send_service_status(s);
  }
#endif

  server_components_initialized();

  /*
    Set opt_super_readonly here because if opt_super_readonly is set
    in get_option, it will create problem while setting up event scheduler.
  */
  set_super_read_only_post_init();

  DBUG_PRINT("info", ("Block, listening for incoming connections"));

  (void)MYSQL_SET_STAGE(0, __FILE__, __LINE__);

  server_operational_state = SERVER_OPERATING;
  sysd::notify("READY=1\nSTATUS=SERVER_OPERATING\nMAIN_PID=", getpid(), "\n");

  (void)RUN_HOOK(server_state, before_handle_connection, (NULL));

#if defined(_WIN32)
  setup_conn_event_handler_threads();
#else
  mysql_mutex_lock(&LOCK_socket_listener_active);
  // Make it possible for the signal handler to kill the listener.
  socket_listener_active = true;
  mysql_mutex_unlock(&LOCK_socket_listener_active);

  if (opt_daemonize) {
    if (nstdout != nullptr) {
      // Show the pid on stdout if deamonizing and connected to tty
      fprintf(nstdout, "mysqld is running as pid %lu\n", current_pid);
      fclose(nstdout);
      nstdout = nullptr;
    }

    mysqld::runtime::signal_parent(pipe_write_fd, 1);
  }

  mysqld_socket_acceptor->connection_event_loop();
#endif /* _WIN32 */
  server_operational_state = SERVER_SHUTTING_DOWN;
  sysd::notify("STOPPING=1\nSTATUS=SERVER_SHUTTING_DOWN\n");

  DBUG_PRINT("info", ("No longer listening for incoming connections"));

  mysql_audit_notify(MYSQL_AUDIT_SERVER_SHUTDOWN_SHUTDOWN,
                     MYSQL_AUDIT_SERVER_SHUTDOWN_REASON_SHUTDOWN,
                     MYSQLD_SUCCESS_EXIT);

  terminate_compress_gtid_table_thread();
  /*
    Save set of GTIDs of the last binlog into gtid_executed table
    on server shutdown.
  */
  if (opt_bin_log)
    if (gtid_state->save_gtids_of_last_binlog_into_table(false))
      LogErr(WARNING_LEVEL, ER_CANT_SAVE_GTIDS);

#ifndef _WIN32
  mysql_mutex_lock(&LOCK_socket_listener_active);
  // Notify the signal handler that we have stopped listening for connections.
  socket_listener_active = false;
  mysql_cond_broadcast(&COND_socket_listener_active);
  mysql_mutex_unlock(&LOCK_socket_listener_active);
#endif  // !_WIN32

#ifdef HAVE_PSI_THREAD_INTERFACE
  /*
    Disable the main thread instrumentation,
    to avoid recording events during the shutdown.
  */
  PSI_THREAD_CALL(delete_current_thread)();
#endif /* HAVE_PSI_THREAD_INTERFACE */

  DBUG_PRINT("info", ("Waiting for shutdown proceed"));
  int ret = 0;
#ifdef _WIN32
  if (shutdown_restart_thr_handle.handle)
    ret = my_thread_join(&shutdown_restart_thr_handle, NULL);
  shutdown_restart_thr_handle.handle = NULL;
  if (0 != ret)
    LogErr(WARNING_LEVEL, ER_CANT_JOIN_SHUTDOWN_THREAD, "shutdown ", ret);
#else
  if (signal_thread_id.thread != 0)
    ret = my_thread_join(&signal_thread_id, nullptr);
  signal_thread_id.thread = 0;
  if (0 != ret)
    LogErr(WARNING_LEVEL, ER_CANT_JOIN_SHUTDOWN_THREAD, "signal_", ret);
#endif  // _WIN32

  clean_up(1);
  mysqld_exit(signal_hand_thr_exit_code);
}

  /****************************************************************************
    Main and thread entry function for Win32
    (all this is needed only to run mysqld as a service on WinNT)
  ****************************************************************************/

#if defined(_WIN32)

bool is_windows_service() { return windows_service; }

NTService *get_win_service_ptr() { return &Service; }

int mysql_service(void *p) {
  int my_argc;
  char **my_argv;

  if (use_opt_args) {
    my_argc = opt_argc;
    my_argv = opt_argv;
  } else if (is_mysqld_monitor()) {
    my_argc = Service.my_argc;
    my_argv = Service.my_argv;
  } else {
    my_argc = my_global_argc;
    my_argv = my_global_argv;
  }

  if (!mysqld_early_option) {
    int res = start_monitor();
    if (res != -1) {
      deinitialize_mysqld_monitor();
      return res;
    }
  }

  if (my_thread_init()) {
    flush_error_log_messages();
    return 1;
  }

  win_main(my_argc, my_argv);

  my_thread_end();
  return 0;
}

/* Quote string if it contains space, else copy */

static char *add_quoted_string(char *to, const char *from, char *to_end) {
  uint length = (uint)(to_end - to);

  if (!strchr(from, ' ')) return strmake(to, from, length - 1);
  return strxnmov(to, length - 1, "\"", from, "\"", NullS);
}

/**
  Handle basic handling of services, like installation and removal.

  @param argv             Pointer to argument list
  @param servicename    Internal name of service
  @param displayname    Display name of service (in taskbar ?)
  @param file_path    Path to this program
  @param startup_option Startup option to mysqld

  @retval
    0   option handled
  @retval
    1   Could not handle option
*/

static bool default_service_handling(char **argv, const char *servicename,
                                     const char *displayname,
                                     const char *file_path,
                                     const char *extra_opt,
                                     const char *account_name) {
  char path_and_service[FN_REFLEN + FN_REFLEN + 32], *pos, *end;
  const char *opt_delim;
  end = path_and_service + sizeof(path_and_service) - 3;

  /* We have to quote filename if it contains spaces */
  pos = add_quoted_string(path_and_service, file_path, end);
  if (extra_opt && *extra_opt) {
    /*
     Add option after file_path. There will be zero or one extra option.  It's
     assumed to be --defaults-file=file but isn't checked.  The variable (not
     the option name) should be quoted if it contains a string.
    */
    *pos++ = ' ';
    if (opt_delim = strchr(extra_opt, '=')) {
      size_t length = ++opt_delim - extra_opt;
      pos = my_stpnmov(pos, extra_opt, length);
    } else
      opt_delim = extra_opt;

    pos = add_quoted_string(pos, opt_delim, end);
  }
  /* We must have servicename last */
  *pos++ = ' ';
  (void)add_quoted_string(pos, servicename, end);

  if (Service.got_service_option(argv, "install")) {
    Service.Install(1, servicename, displayname, path_and_service,
                    account_name);
    return 0;
  }
  if (Service.got_service_option(argv, "install-manual")) {
    Service.Install(0, servicename, displayname, path_and_service,
                    account_name);
    return 0;
  }
  if (Service.got_service_option(argv, "remove")) {
    Service.Remove(servicename);
    return 0;
  }
  return 1;
}

int mysqld_main(int argc, char **argv) {
  bool mysqld_monitor = false;
  mysqld_early_option = is_early_option(argc, argv);

  if (!mysqld_early_option) {
    initialize_mysqld_monitor();
    mysqld_monitor = is_mysqld_monitor();
  }

  if (mysqld_early_option || !mysqld_monitor) {
    /*
      When several instances are running on the same machine, we
      need to have an  unique  named  hEventShudown  through the
      application PID e.g.: MySQLShutdown1890; MySQLShutdown2342
    */

    snprintf(shutdown_event_name, sizeof(shutdown_event_name),
             "mysqld%s_shutdown", get_monitor_pid());
    int10_to_str((int)GetCurrentProcessId(),
                 my_stpcpy(restart_event_name, "MYSQLRestart"), 10);
  }

  /* Must be initialized early for comparison of service name */
  system_charset_info = &my_charset_utf8_general_ci;

  if (mysqld_early_option || !mysqld_monitor) {
#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
    pre_initialize_performance_schema();
#endif /*WITH_PERFSCHEMA_STORAGE_ENGINE */

    if (my_init()) {
      LogErr(ERROR_LEVEL, ER_MYINIT_FAILED);
      flush_error_log_messages();
      return 1;
    }
  }

  if (Service.GetOS() && mysqld_monitor) /* true NT family */
  {
    char file_path[FN_REFLEN];
    my_path(file_path, argv[0], ""); /* Find name in path */
    fn_format(file_path, argv[0], file_path, "",
              MY_REPLACE_DIR | MY_UNPACK_FILENAME | MY_RESOLVE_SYMLINKS);

    if (argc == 2) {
      if (!default_service_handling(argv, MYSQL_SERVICENAME, MYSQL_SERVICENAME,
                                    file_path, "", NULL))
        return 0;
      if (Service.IsService(argv[1])) /* Start an optional service */
      {
        /*
          Only add the service name to the groups read from the config file
          if it's not "MySQL". (The default service name should be 'mysqld'
          but we started a bad tradition by calling it MySQL from the start
          and we are now stuck with it.
        */
        if (my_strcasecmp(system_charset_info, argv[1], "mysql"))
          load_default_groups[load_default_groups_sz - 2] = argv[1];
        windows_service = true;

        Service.Init(argv[1], mysql_service);
        return 0;
      }
    } else if (argc == 3) /* install or remove any optional service */
    {
      if (!default_service_handling(argv, argv[2], argv[2], file_path, "",
                                    NULL))
        return 0;
      if (Service.IsService(argv[2])) {
        /*
          mysqld was started as
          mysqld --defaults-file=my_path\my.ini service-name
        */
        use_opt_args = 1;
        opt_argc = 2;  // Skip service-name
        opt_argv = argv;
        windows_service = true;
        if (my_strcasecmp(system_charset_info, argv[2], "mysql"))
          load_default_groups[load_default_groups_sz - 2] = argv[2];
        Service.Init(argv[2], mysql_service);
        return 0;
      }
    } else if (argc == 4 || argc == 5) {
      /*
        This may seem strange, because we handle --local-service while
        preserving 4.1's behavior of allowing any one other argument that is
        passed to the service on startup. (The assumption is that this is
        --defaults-file=file, but that was not enforced in 4.1, so we don't
        enforce it here.)
      */
      const char *extra_opt = NullS;
      const char *account_name = NullS;
      int index;
      for (index = 3; index < argc; index++) {
        if (!strcmp(argv[index], "--local-service"))
          account_name = "NT AUTHORITY\\LocalService";
        else
          extra_opt = argv[index];
      }

      if (argc == 4 || account_name)
        if (!default_service_handling(argv, argv[2], argv[2], file_path,
                                      extra_opt, account_name))
          return 0;
    } else if (argc == 1 && Service.IsService(MYSQL_SERVICENAME)) {
      /* start the default service */
      windows_service = true;
      Service.Init(MYSQL_SERVICENAME, mysql_service);
      return 0;
    }
  }

  // Set windows_service value in mysqld
  if (!mysqld_monitor) {
    windows_service = is_monitor_win_service();
    my_global_argc = argc;
    my_global_argv = argv;
  } else {
    Service.my_argc = argc;
    Service.my_argv = argv;
  }

  return mysql_service(NULL);
}
#endif  // _WIN32

static bool read_init_file(char *file_name) {
  MYSQL_FILE *file;
  DBUG_ENTER("read_init_file");
  DBUG_PRINT("enter", ("name: %s", file_name));

  LogErr(INFORMATION_LEVEL, ER_BEG_INITFILE, file_name);

  if (!(file =
            mysql_file_fopen(key_file_init, file_name, O_RDONLY, MYF(MY_WME))))
    DBUG_RETURN(true);
  (void)bootstrap::run_bootstrap_thread(file, NULL, SYSTEM_THREAD_INIT_FILE);
  mysql_file_fclose(file, MYF(MY_WME));

  LogErr(INFORMATION_LEVEL, ER_END_INITFILE, file_name);

  DBUG_RETURN(false);
}

/****************************************************************************
  Handle start options
******************************************************************************/

/**
  Process command line options flagged as 'early'.
  Some components needs to be initialized as early as possible,
  because the rest of the server initialization depends on them.
  Options that needs to be parsed early includes:
  - the performance schema, when compiled in,
  - options related to the help,
  - options related to the bootstrap
  The performance schema needs to be initialized as early as possible,
  before to-be-instrumented objects of the server are initialized.
*/
static int handle_early_options() {
  int ho_error;
  vector<my_option> all_early_options;
  all_early_options.reserve(100);

  my_getopt_register_get_addr(NULL);
  /* Skip unknown options so that they may be processed later */
  my_getopt_skip_unknown = true;

  /* Add the system variables parsed early */
  sys_var_add_options(&all_early_options, sys_var::PARSE_EARLY);

  /* Add the command line options parsed early */
  for (my_option *opt = my_long_early_options; opt->name != NULL; opt++)
    all_early_options.push_back(*opt);

  add_terminator(&all_early_options);

  my_getopt_error_reporter = option_error_reporter;
  my_charset_error_reporter = charset_error_reporter;

  ho_error = handle_options(&remaining_argc, &remaining_argv,
                            &all_early_options[0], mysqld_get_one_option);
  if (ho_error == 0) {
    /* Add back the program name handle_options removes */
    remaining_argc++;
    remaining_argv--;

    if (opt_initialize_insecure) opt_initialize = true;
  }

  // Swap with an empty vector, i.e. delete elements and free allocated space.
  vector<my_option>().swap(all_early_options);

  return ho_error;
}

/**
  Adjust @c open_files_limit.
  Computation is  based on:
  - @c max_connections,
  - @c table_cache_size,
  - the platform max open file limit.
*/
static void adjust_open_files_limit(ulong *requested_open_files) {
  ulong limit_1;
  ulong limit_2;
  ulong limit_3;
  ulong request_open_files;
  ulong effective_open_files;

  /* MyISAM requires two file handles per table. */
  limit_1 = 10 + max_connections + table_cache_size * 2;

  /*
    We are trying to allocate no less than max_connections*5 file
    handles (i.e. we are trying to set the limit so that they will
    be available).
  */
  limit_2 = max_connections * 5;

  /* Try to allocate no less than 5000 by default. */
  limit_3 = open_files_limit ? open_files_limit : 5000;

  request_open_files = max<ulong>(max<ulong>(limit_1, limit_2), limit_3);

  /* Notice: my_set_max_open_files() may return more than requested. */
  effective_open_files = my_set_max_open_files(request_open_files);

  if (effective_open_files < request_open_files) {
    if (open_files_limit == 0) {
      LogErr(WARNING_LEVEL, ER_CHANGED_MAX_OPEN_FILES, effective_open_files,
             request_open_files);
    } else {
      LogErr(WARNING_LEVEL, ER_CANT_INCREASE_MAX_OPEN_FILES,
             effective_open_files, request_open_files);
    }
  }

  open_files_limit = effective_open_files;
  if (requested_open_files)
    *requested_open_files =
        min<ulong>(effective_open_files, request_open_files);
}

static void adjust_max_connections(ulong requested_open_files) {
  ulong limit;

  limit = requested_open_files - 10 - TABLE_OPEN_CACHE_MIN * 2;

  if (limit < max_connections) {
    LogErr(WARNING_LEVEL, ER_CHANGED_MAX_CONNECTIONS, limit, max_connections);

    // This can be done unprotected since it is only called on startup.
    max_connections = limit;
  }
}

static void adjust_table_cache_size(ulong requested_open_files) {
  ulong limit;

  limit = max<ulong>((requested_open_files - 10 - max_connections) / 2,
                     TABLE_OPEN_CACHE_MIN);

  if (limit < table_cache_size) {
    LogErr(WARNING_LEVEL, ER_CHANGED_TABLE_OPEN_CACHE, limit, table_cache_size);

    table_cache_size = limit;
  }

  table_cache_size_per_instance = table_cache_size / table_cache_instances;
}

static void adjust_table_def_size() {
  ulong default_value;
  sys_var *var;

  default_value = min<ulong>(400 + table_cache_size / 2, 2000);
  var = intern_find_sys_var(STRING_WITH_LEN("table_definition_cache"));
  DBUG_ASSERT(var != NULL);
  var->update_default(default_value);

  if (!table_definition_cache_specified) table_def_size = default_value;
}

static void adjust_related_options(ulong *requested_open_files) {
  /*
    In bootstrap, disable grant tables (about to be created)
  */
  if (opt_initialize) opt_noacl = 1;

  /* The order is critical here, because of dependencies. */
  adjust_open_files_limit(requested_open_files);
  adjust_max_connections(*requested_open_files);
  adjust_table_cache_size(*requested_open_files);
  adjust_table_def_size();
}

vector<my_option> all_options;

struct my_option my_long_early_options[] = {
#if !defined(_WIN32)
    {"daemonize", 'D', "Run mysqld as sysv daemon", &opt_daemonize,
     &opt_daemonize, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
    {"skip-grant-tables", 0,
     "Start without grant tables. This gives all users FULL ACCESS to all "
     "tables.",
     &opt_noacl, &opt_noacl, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"help", '?', "Display this help and exit.", &opt_help, &opt_help, 0,
     GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"verbose", 'v', "Used with --help option for detailed help.", &opt_verbose,
     &opt_verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"version", 'V', "Output version information and exit.", 0, 0, 0,
     GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"initialize", 'I',
     "Create the default database and exit."
     " Create a super user with a random expired password and store it into "
     "the log.",
     &opt_initialize, &opt_initialize, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"initialize-insecure", 0,
     "Create the default database and exit."
     " Create a super user with empty password.",
     &opt_initialize_insecure, &opt_initialize_insecure, 0, GET_BOOL, NO_ARG, 0,
     0, 0, 0, 0, 0},
    {"keyring-migration-source", OPT_KEYRING_MIGRATION_SOURCE,
     "Keyring plugin from where the keys needs to "
     "be migrated to. This option must be specified along with "
     "--keyring-migration-destination.",
     &opt_keyring_migration_source, &opt_keyring_migration_source, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"keyring-migration-destination", OPT_KEYRING_MIGRATION_DESTINATION,
     "Keyring plugin to which the keys are "
     "migrated to. This option must be specified along with "
     "--keyring-migration-source.",
     &opt_keyring_migration_destination, &opt_keyring_migration_destination, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"keyring-migration-user", OPT_KEYRING_MIGRATION_USER,
     "User to login to server.", &opt_keyring_migration_user,
     &opt_keyring_migration_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"keyring-migration-host", OPT_KEYRING_MIGRATION_HOST, "Connect to host.",
     &opt_keyring_migration_host, &opt_keyring_migration_host, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"keyring-migration-password", 'p',
     "Password to use when connecting to server during keyring migration. "
     "If password value is not specified then it will be asked from the tty.",
     0, 0, 0, GET_PASSWORD, OPT_ARG, 0, 0, 0, 0, 0, 0},
    {"keyring-migration-socket", OPT_KEYRING_MIGRATION_SOCKET,
     "The socket file to use for connection.", &opt_keyring_migration_socket,
     &opt_keyring_migration_socket, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"keyring-migration-port", OPT_KEYRING_MIGRATION_PORT,
     "Port number to use for connection.", &opt_keyring_migration_port,
     &opt_keyring_migration_port, 0, GET_ULONG, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"no-dd-upgrade", 0,
     "Abort restart if automatic upgrade or downgrade of the data dictionary "
     "is needed.",
     &opt_no_dd_upgrade, &opt_no_dd_upgrade, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
     0},
    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

/**
  System variables are automatically command-line options (few
  exceptions are documented in sys_var.h), so don't need
  to be listed here.
*/

struct my_option my_long_options[] = {
    {"abort-slave-event-count", 0,
     "Option used by mysql-test for debugging and testing of replication.",
     &abort_slave_event_count, &abort_slave_event_count, 0, GET_INT,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"allow-suspicious-udfs", 0,
     "Allows use of UDFs consisting of only one symbol xxx() "
     "without corresponding xxx_init() or xxx_deinit(). That also means "
     "that one can load any function from any library, for example exit() "
     "from libc.so",
     &opt_allow_suspicious_udfs, &opt_allow_suspicious_udfs, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    {"ansi", 'a',
     "Use ANSI SQL syntax instead of MySQL syntax. This mode "
     "will also set transaction isolation level 'serializable'.",
     0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
    /*
      Because Sys_var_bit does not support command-line options, we need to
      explicitly add one for --autocommit
    */
    {"autocommit", 0, "Set default value for autocommit (0 or 1)",
     &opt_autocommit, &opt_autocommit, 0, GET_BOOL, OPT_ARG, 1, 0, 0,
     &source_autocommit, /* arg_source, to be copied to Sys_var */
     0, NULL},
    {"binlog-do-db", OPT_BINLOG_DO_DB,
     "Tells the master it should log updates for the specified database, "
     "and exclude all others not explicitly mentioned.",
     0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"binlog-ignore-db", OPT_BINLOG_IGNORE_DB,
     "Tells the master that updates to the given database should not be logged "
     "to the binary log.",
     0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"binlog-row-event-max-size", 0,
     "The maximum size of a row-based binary log event in bytes. Rows will be "
     "grouped into events smaller than this size if possible. "
     "The value has to be a multiple of 256.",
     &opt_binlog_rows_event_max_size, &opt_binlog_rows_event_max_size, 0,
     GET_ULONG, REQUIRED_ARG,
     /* def_value */ 8192, /* min_value */ 256, /* max_value */ ULONG_MAX,
     /* sub_size */ 0, /* block_size */ 256,
     /* app_type */ 0},
    {"character-set-client-handshake", 0,
     "Don't ignore client side character set value sent during handshake.",
     &opt_character_set_client_handshake, &opt_character_set_client_handshake,
     0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
    {"character-set-filesystem", 0, "Set the filesystem character set.",
     &character_set_filesystem_name, &character_set_filesystem_name, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"character-set-server", 'C', "Set the default character set.",
     &default_character_set_name, &default_character_set_name, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"chroot", 'r', "Chroot mysqld daemon during startup.", &mysqld_chroot,
     &mysqld_chroot, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"collation-server", 0, "Set the default collation.",
     &default_collation_name, &default_collation_name, 0, GET_STR, REQUIRED_ARG,
     0, 0, 0, 0, 0, 0},
    {"console", OPT_CONSOLE,
     "Write error output on screen; don't remove the console window on "
     "windows.",
     &opt_console, &opt_console, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"core-file", OPT_WANT_CORE, "Write core on errors.", 0, 0, 0, GET_NO_ARG,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    {"default-collation-for-utf8mb4", 0,
     "Set the default collation for utf8mb4 while replicating implicit utf8mb4 "
     "collations.",
     &default_collation_name_for_utf8mb4, &default_collation_name_for_utf8mb4,
     0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    /* default-storage-engine should have "MyISAM" as def_value. Instead
       of initializing it here it is done in init_common_variables() due
       to a compiler bug in Sun Studio compiler. */
    {"default-storage-engine", 0, "The default storage engine for new tables",
     &default_storage_engine, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"default-tmp-storage-engine", 0,
     "The default storage engine for new explict temporary tables",
     &default_tmp_storage_engine, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0,
     0},
    {"default-time-zone", 0, "Set the default time zone.", &default_tz_name,
     &default_tz_name, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"disconnect-slave-event-count", 0,
     "Option used by mysql-test for debugging and testing of replication.",
     &disconnect_slave_event_count, &disconnect_slave_event_count, 0, GET_INT,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"exit-info", 'T', "Used for debugging. Use at your own risk.", 0, 0, 0,
     GET_LONG, OPT_ARG, 0, 0, 0, 0, 0, 0},

    {"external-locking", 0,
     "Use system (external) locking (disabled by "
     "default).  With this option enabled you can run myisamchk to test "
     "(not repair) tables while the MySQL server is running. Disable with "
     "--skip-external-locking.",
     &opt_external_locking, &opt_external_locking, 0, GET_BOOL, NO_ARG, 0, 0, 0,
     0, 0, 0},
    /* We must always support the next option to make scripts like mysqltest
       easier to do */
    {"gdb", 0, "Set up signals usable for debugging.", &opt_debugging,
     &opt_debugging, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#if defined(HAVE_LINUX_LARGE_PAGES) || defined(HAVE_SOLARIS_LARGE_PAGES)
    {"super-large-pages", 0, "Enable support for super large pages.",
     &opt_super_large_pages, &opt_super_large_pages, 0, GET_BOOL, OPT_ARG, 0, 0,
     1, 0, 1, 0},
#endif
    {"language", 'L',
     "Client error messages in given language. May be given as a full path. "
     "Deprecated. Use --lc-messages-dir instead.",
     &lc_messages_dir_ptr, &lc_messages_dir_ptr, 0, GET_STR, REQUIRED_ARG, 0, 0,
     0, 0, 0, 0},
    {"lc-messages", 0, "Set the language used for the error messages.",
     &lc_messages, &lc_messages, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"lc-time-names", 0,
     "Set the language used for the month names and the days of the week.",
     &lc_time_names_name, &lc_time_names_name, 0, GET_STR, REQUIRED_ARG, 0, 0,
     0, 0, 0, 0},
    {"log-bin", OPT_BIN_LOG,
     "Configures the name prefix to use for binary log files. If the --log-bin "
     "option is not supplied, the name prefix defaults to \"binlog\". If the "
     "--log-bin option is supplied without argument, the name prefix defaults "
     "to \"HOSTNAME-bin\", where HOSTNAME is the machine's hostname. To set a "
     "different name prefix for binary log files, use --log-bin=name. To "
     "disable "
     "binary logging, use the --skip-log-bin or --disable-log-bin option.",
     &opt_bin_logname, &opt_bin_logname, 0, GET_STR_ALLOC, OPT_ARG, 0, 0, 0, 0,
     0, 0},
    {"log-bin-index", 0, "File that holds the names for binary log files.",
     &opt_binlog_index_name, &opt_binlog_index_name, 0, GET_STR, REQUIRED_ARG,
     0, 0, 0, 0, 0, 0},
    {"relay-log-index", 0, "File that holds the names for relay log files.",
     &opt_relaylog_index_name, &opt_relaylog_index_name, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"log-isam", OPT_ISAM_LOG, "Log all MyISAM changes to file.",
     &myisam_log_filename, &myisam_log_filename, 0, GET_STR, OPT_ARG, 0, 0, 0,
     0, 0, 0},
    {"log-raw", 0,
     "Log to general log before any rewriting of the query. For use in "
     "debugging, not production as "
     "sensitive information may be logged.",
     &opt_general_log_raw, &opt_general_log_raw, 0, GET_BOOL, NO_ARG, 0, 0, 1,
     0, 1, 0},
    {"log-short-format", 0,
     "Don't log extra information to update and slow-query logs.",
     &opt_short_log_format, &opt_short_log_format, 0, GET_BOOL, NO_ARG, 0, 0, 0,
     0, 0, 0},
    {"log-tc", 0,
     "Path to transaction coordinator log (used for transactions that affect "
     "more than one storage engine, when binary log is disabled).",
     &opt_tc_log_file, &opt_tc_log_file, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
     0, 0},
    {"log-tc-size", 0, "Size of transaction coordinator log.", &opt_tc_log_size,
     &opt_tc_log_size, 0, GET_ULONG, REQUIRED_ARG,
     TC_LOG_MIN_PAGES *my_getpagesize(), TC_LOG_MIN_PAGES *my_getpagesize(),
     ULONG_MAX, 0, my_getpagesize(), 0},
    {"master-info-file", 0,
     "The location and name of the file that remembers the master and where "
     "the I/O replication thread is in the master's binlogs.",
     &master_info_file, &master_info_file, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
     0, 0},
    {"master-retry-count", OPT_MASTER_RETRY_COUNT,
     "The number of tries the slave will make to connect to the master before "
     "giving up. "
     "Deprecated option, use 'CHANGE MASTER TO master_retry_count = <num>' "
     "instead.",
     &master_retry_count, &master_retry_count, 0, GET_ULONG, REQUIRED_ARG,
     3600 * 24, 0, 0, 0, 0, 0},
    {"max-binlog-dump-events", 0,
     "Option used by mysql-test for debugging and testing of replication.",
     &max_binlog_dump_events, &max_binlog_dump_events, 0, GET_INT, REQUIRED_ARG,
     0, 0, 0, 0, 0, 0},
    {"memlock", 0, "Lock mysqld in memory.", &locked_in_memory,
     &locked_in_memory, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"old-style-user-limits", 0,
     "Enable old-style user limits (before 5.0.3, user resources were counted "
     "per each user+host vs. per account).",
     &opt_old_style_user_limits, &opt_old_style_user_limits, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    {"port-open-timeout", 0,
     "Maximum time in seconds to wait for the port to become free. "
     "(Default: No wait).",
     &mysqld_port_timeout, &mysqld_port_timeout, 0, GET_UINT, REQUIRED_ARG, 0,
     0, 0, 0, 0, 0},
    {"replicate-do-db", OPT_REPLICATE_DO_DB,
     "Tells the slave thread to restrict replication to the specified "
     "database. "
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
     "to replicate-do-db.",
     0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"replicate-ignore-db", OPT_REPLICATE_IGNORE_DB,
     "Tells the slave thread to not replicate to the specified database. To "
     "specify more than one database to ignore, use the directive multiple "
     "times, once for each database. This option will not work if you use "
     "cross database updates. If you need cross database updates to work, "
     "make sure you have 3.23.28 or later, and use replicate-wild-ignore-"
     "table=db_name.%. ",
     0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"replicate-ignore-table", OPT_REPLICATE_IGNORE_TABLE,
     "Tells the slave thread to not replicate to the specified table. To "
     "specify "
     "more than one table to ignore, use the directive multiple times, once "
     "for "
     "each table. This will work for cross-database updates, in contrast to "
     "replicate-ignore-db.",
     0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"replicate-rewrite-db", OPT_REPLICATE_REWRITE_DB,
     "Updates to a database with a different name than the original. Example: "
     "replicate-rewrite-db=master_db_name->slave_db_name.",
     0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"replicate-same-server-id", 0,
     "In replication, if set to 1, do not skip events having our server id. "
     "Default value is 0 (to break infinite loops in circular replication). "
     "Can't be set to 1 if --log-slave-updates is used.",
     &replicate_same_server_id, &replicate_same_server_id, 0, GET_BOOL, NO_ARG,
     0, 0, 0, 0, 0, 0},
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
    {"safe-user-create", 0,
     "Don't allow new user creation by the user who has no write privileges to "
     "the mysql.user table.",
     &opt_safe_user_create, &opt_safe_user_create, 0, GET_BOOL, NO_ARG, 0, 0, 0,
     0, 0, 0},
    {"show-slave-auth-info", 0,
     "Show user and password in SHOW SLAVE HOSTS on this master.",
     &opt_show_slave_auth_info, &opt_show_slave_auth_info, 0, GET_BOOL, NO_ARG,
     0, 0, 0, 0, 0, 0},
    {"skip-host-cache", OPT_SKIP_HOST_CACHE, "Don't cache host names.", 0, 0, 0,
     GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"skip-new", OPT_SKIP_NEW, "Don't use new, possibly wrong routines.", 0, 0,
     0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"skip-slave-start", 0, "If set, slave is not autostarted.",
     &opt_skip_slave_start, &opt_skip_slave_start, 0, GET_BOOL, NO_ARG, 0, 0, 0,
     0, 0, 0},
    {"skip-stack-trace", OPT_SKIP_STACK_TRACE,
     "Don't print a stack trace on failure.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
     0, 0, 0, 0},
#if defined(_WIN32)
    {"slow-start-timeout", 0,
     "Maximum number of milliseconds that the service control manager should "
     "wait "
     "before trying to kill the windows service during startup"
     "(Default: 15000).",
     &slow_start_timeout, &slow_start_timeout, 0, GET_ULONG, REQUIRED_ARG,
     15000, 0, 0, 0, 0, 0},
#endif
    {"sporadic-binlog-dump-fail", 0,
     "Option used by mysql-test for debugging and testing of replication.",
     &opt_sporadic_binlog_dump_fail, &opt_sporadic_binlog_dump_fail, 0,
     GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_OPENSSL
    {"ssl", 0,
     "Enable SSL for connection (automatically enabled with other flags).",
     &opt_use_ssl, &opt_use_ssl, 0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
#endif
#ifdef _WIN32
    {"standalone", 0, "Dummy option to start as a standalone program (NT).", 0,
     0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"no-monitor", 0, "Disable monitor process.", &opt_no_monitor,
     &opt_no_monitor, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
    {"symbolic-links", 's',
     "Enable symbolic link support (deprecated and will be  removed in a future"
     " release).",
     &my_enable_symlinks, &my_enable_symlinks, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
     0, 0},
    {"sysdate-is-now", 0,
     "Non-default option to alias SYSDATE() to NOW() to make it "
     "safe-replicable. "
     "Since 5.0, SYSDATE() returns a `dynamic' value different for different "
     "invocations, even within the same statement.",
     &global_system_variables.sysdate_is_now, 0, 0, GET_BOOL, NO_ARG, 0, 0, 1,
     0, 1, 0},
    {"tc-heuristic-recover", 0,
     "Decision to use in heuristic recover process. Possible values are OFF, "
     "COMMIT or ROLLBACK.",
     &tc_heuristic_recover, &tc_heuristic_recover,
     &tc_heuristic_recover_typelib, GET_ENUM, REQUIRED_ARG,
     TC_HEURISTIC_NOT_USED, 0, 0, 0, 0, 0},
#if defined(ENABLED_DEBUG_SYNC)
    {"debug-sync-timeout", OPT_DEBUG_SYNC_TIMEOUT,
     "Enable the debug sync facility "
     "and optionally specify a default wait timeout in seconds. "
     "A zero value keeps the facility disabled.",
     &opt_debug_sync_timeout, 0, 0, GET_UINT, OPT_ARG, 0, 0, UINT_MAX, 0, 0, 0},
#endif /* defined(ENABLED_DEBUG_SYNC) */
    {"transaction-isolation", 0, "Default transaction isolation level.",
     &global_system_variables.transaction_isolation,
     &global_system_variables.transaction_isolation, &tx_isolation_typelib,
     GET_ENUM, REQUIRED_ARG, ISO_REPEATABLE_READ, 0, 0, 0, 0, 0},
    {"transaction-read-only", 0,
     "Default transaction access mode. "
     "True if transactions are read-only.",
     &global_system_variables.transaction_read_only,
     &global_system_variables.transaction_read_only, 0, GET_BOOL, OPT_ARG, 0, 0,
     0, 0, 0, 0},
    {"user", 'u', "Run mysqld daemon as user.", 0, 0, 0, GET_STR, REQUIRED_ARG,
     0, 0, 0, 0, 0, 0},
    {"early-plugin-load", OPT_EARLY_PLUGIN_LOAD,
     "Optional semicolon-separated list of plugins to load before storage "
     "engine "
     "initialization, where each plugin is identified as name=library, where "
     "name is the plugin name and library is the plugin library in plugin_dir.",
     0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"plugin-load", OPT_PLUGIN_LOAD,
     "Optional semicolon-separated list of plugins to load, where each plugin "
     "is "
     "identified as name=library, where name is the plugin name and library "
     "is the plugin library in plugin_dir.",
     0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"plugin-load-add", OPT_PLUGIN_LOAD_ADD,
     "Optional semicolon-separated list of plugins to load, where each plugin "
     "is "
     "identified as name=library, where name is the plugin name and library "
     "is the plugin library in plugin_dir. This option adds to the list "
     "specified by --plugin-load in an incremental way. "
     "Multiple --plugin-load-add are supported.",
     0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

    {"innodb", OPT_SKIP_INNODB,
     "Deprecated option. Provided for backward compatibility only. "
     "The option has no effect on the server behaviour. InnoDB is always "
     "enabled. "
     "The option will be removed in a future release.",
     0, 0, 0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},

    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

static int show_queries(THD *thd, SHOW_VAR *var, char *) {
  var->type = SHOW_LONGLONG;
  var->value = (char *)&thd->query_id;
  return 0;
}

static int show_net_compression(THD *thd, SHOW_VAR *var, char *buff) {
  var->type = SHOW_MY_BOOL;
  var->value = buff;
  *((bool *)buff) = thd->get_protocol()->get_compression();
  return 0;
}

static int show_starttime(THD *thd, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONGLONG;
  var->value = buff;
  *((longlong *)buff) =
      (longlong)(thd->query_start_in_secs() - server_start_time);
  return 0;
}

static int show_max_used_connections_time(THD *thd, SHOW_VAR *var, char *buff) {
  MYSQL_TIME max_used_connections_time;
  var->type = SHOW_CHAR;
  var->value = buff;
  thd->variables.time_zone->gmt_sec_to_TIME(
      &max_used_connections_time,
      Connection_handler_manager::max_used_connections_time);
  my_datetime_to_str(&max_used_connections_time, buff, 0);
  return 0;
}

static int show_num_thread_running(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONGLONG;
  var->value = buff;
  long long *value = reinterpret_cast<long long *>(buff);
  *value = static_cast<long long>(
      Global_THD_manager::get_instance()->get_num_thread_running());
  return 0;
}

static int show_num_thread_created(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  long *value = reinterpret_cast<long *>(buff);
  *value = static_cast<long>(
      Global_THD_manager::get_instance()->get_num_thread_created());
  return 0;
}

static int show_thread_id_count(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  long *value = reinterpret_cast<long *>(buff);
  *value = static_cast<long>(
      Global_THD_manager::get_instance()->get_thread_id() - 1);
  return 0;
}

static int show_aborted_connects(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  long *value = reinterpret_cast<long *>(buff);
  *value = static_cast<long>(
      Connection_handler_manager::get_instance()->aborted_connects());
  return 0;
}

static int show_acl_cache_items_count(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  long *value = reinterpret_cast<long *>(buff);
  *value = static_cast<long>(get_global_acl_cache_size());
  return 0;
}

static int show_connection_errors_max_connection(THD *, SHOW_VAR *var,
                                                 char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  long *value = reinterpret_cast<long *>(buff);
  *value = static_cast<long>(Connection_handler_manager::get_instance()
                                 ->connection_errors_max_connection());
  return 0;
}

static int show_connection_errors_select(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  long *value = reinterpret_cast<long *>(buff);
  *value =
      static_cast<long>(Mysqld_socket_listener::get_connection_errors_select());
  return 0;
}

static int show_connection_errors_accept(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  long *value = reinterpret_cast<long *>(buff);
  *value =
      static_cast<long>(Mysqld_socket_listener::get_connection_errors_accept());
  return 0;
}

static int show_connection_errors_tcpwrap(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  long *value = reinterpret_cast<long *>(buff);
  *value = static_cast<long>(
      Mysqld_socket_listener::get_connection_errors_tcpwrap());
  return 0;
}

#ifdef ENABLED_PROFILING
static int show_flushstatustime(THD *thd, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONGLONG;
  var->value = buff;
  *((longlong *)buff) =
      (longlong)(thd->query_start_in_secs() - flush_status_time);
  return 0;
}
#endif

/**
  After Multisource replication, this function only shows the value
  of default channel.

  To know the status of other channels, performance schema replication
  tables comes to the rescue.

  @todo  Any warning needed if multiple channels exist to request
         the users to start using replication performance schema
         tables.
*/
static int show_slave_running(THD *, SHOW_VAR *var, char *buff) {
  channel_map.rdlock();
  Master_info *mi = channel_map.get_default_channel_mi();

  if (mi) {
    var->type = SHOW_MY_BOOL;
    var->value = buff;
    *((bool *)buff) =
        (bool)(mi && mi->slave_running == MYSQL_SLAVE_RUN_CONNECT &&
               mi->rli->slave_running);
  } else
    var->type = SHOW_UNDEF;

  channel_map.unlock();
  return 0;
}

/**
  This status variable is also exclusively (look comments on
  show_slave_running()) for default channel.
*/
static int show_slave_retried_trans(THD *, SHOW_VAR *var, char *buff) {
  channel_map.rdlock();
  Master_info *mi = channel_map.get_default_channel_mi();

  if (mi) {
    var->type = SHOW_LONG;
    var->value = buff;
    *((long *)buff) = (long)mi->rli->retried_trans;
  } else
    var->type = SHOW_UNDEF;

  channel_map.unlock();
  return 0;
}

/**
  Only for default channel. Refer to comments on show_slave_running()
*/
static int show_slave_received_heartbeats(THD *, SHOW_VAR *var, char *buff) {
  channel_map.rdlock();
  Master_info *mi = channel_map.get_default_channel_mi();

  if (mi) {
    var->type = SHOW_LONGLONG;
    var->value = buff;
    *((longlong *)buff) = mi->received_heartbeats;
  } else
    var->type = SHOW_UNDEF;

  channel_map.unlock();
  return 0;
}

/**
  Only for default channel. Refer to comments on show_slave_running()
*/
static int show_slave_last_heartbeat(THD *thd, SHOW_VAR *var, char *buff) {
  MYSQL_TIME received_heartbeat_time;

  channel_map.rdlock();
  Master_info *mi = channel_map.get_default_channel_mi();

  if (mi) {
    var->type = SHOW_CHAR;
    var->value = buff;
    if (mi->last_heartbeat == 0)
      buff[0] = '\0';
    else {
      thd->variables.time_zone->gmt_sec_to_TIME(
          &received_heartbeat_time,
          static_cast<my_time_t>(mi->last_heartbeat / 1000000));
      my_datetime_to_str(&received_heartbeat_time, buff, 0);
    }
  } else
    var->type = SHOW_UNDEF;

  channel_map.unlock();
  return 0;
}

/**
  Only for default channel. For details, refer to show_slave_running()
*/
static int show_heartbeat_period(THD *, SHOW_VAR *var, char *buff) {
  channel_map.rdlock();
  Master_info *mi = channel_map.get_default_channel_mi();

  if (mi) {
    var->type = SHOW_CHAR;
    var->value = buff;
    sprintf(buff, "%.3f", mi->heartbeat_period);
  } else
    var->type = SHOW_UNDEF;

  channel_map.unlock();
  return 0;
}

#ifndef DBUG_OFF
static int show_slave_rows_last_search_algorithm_used(THD *, SHOW_VAR *var,
                                                      char *buff) {
  uint res = slave_rows_last_search_algorithm_used;
  const char *s =
      ((res == Rows_log_event::ROW_LOOKUP_TABLE_SCAN)
           ? "TABLE_SCAN"
           : ((res == Rows_log_event::ROW_LOOKUP_HASH_SCAN) ? "HASH_SCAN"
                                                            : "INDEX_SCAN"));

  var->type = SHOW_CHAR;
  var->value = buff;
  sprintf(buff, "%s", s);

  return 0;
}

static int show_ongoing_automatic_gtid_violating_transaction_count(
    THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_CHAR;
  var->value = buf;
  sprintf(buf, "%d",
          gtid_state->get_automatic_gtid_violating_transaction_count());
  return 0;
}

static int show_ongoing_anonymous_gtid_violating_transaction_count(
    THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_CHAR;
  var->value = buf;
  sprintf(buf, "%d",
          gtid_state->get_anonymous_gtid_violating_transaction_count());
  return 0;
}

#endif

static int show_ongoing_anonymous_transaction_count(THD *, SHOW_VAR *var,
                                                    char *buf) {
  var->type = SHOW_CHAR;
  var->value = buf;
  sprintf(buf, "%d", gtid_state->get_anonymous_ownership_count());
  return 0;
}

static int show_open_tables(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) = (long)table_cache_manager.cached_tables();
  return 0;
}

static int show_prepared_stmt_count(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  mysql_mutex_lock(&LOCK_prepared_stmt_count);
  *((long *)buff) = (long)prepared_stmt_count;
  mysql_mutex_unlock(&LOCK_prepared_stmt_count);
  return 0;
}

static int show_table_definitions(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) = (long)cached_table_definitions();
  return 0;
}

#if defined(HAVE_OPENSSL)
/* Functions relying on CTX */
static int show_ssl_ctx_sess_accept(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd ? 0
                        : SSL_CTX_sess_accept(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_accept_good(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd
           ? 0
           : SSL_CTX_sess_accept_good(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_connect_good(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd
           ? 0
           : SSL_CTX_sess_connect_good(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_accept_renegotiate(THD *, SHOW_VAR *var,
                                                char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd
           ? 0
           : SSL_CTX_sess_accept_renegotiate(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_connect_renegotiate(THD *, SHOW_VAR *var,
                                                 char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd
           ? 0
           : SSL_CTX_sess_connect_renegotiate(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_cb_hits(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd ? 0
                        : SSL_CTX_sess_cb_hits(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_hits(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd ? 0 : SSL_CTX_sess_hits(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_cache_full(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd
           ? 0
           : SSL_CTX_sess_cache_full(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_misses(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd ? 0
                        : SSL_CTX_sess_misses(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_timeouts(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd ? 0
                        : SSL_CTX_sess_timeouts(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_number(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd ? 0
                        : SSL_CTX_sess_number(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_connect(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd ? 0
                        : SSL_CTX_sess_connect(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_sess_get_cache_size(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd
           ? 0
           : SSL_CTX_sess_get_cache_size(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_get_verify_mode(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd
           ? 0
           : SSL_CTX_get_verify_mode(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_get_verify_depth(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONG;
  var->value = buff;
  *((long *)buff) =
      (!ssl_acceptor_fd
           ? 0
           : SSL_CTX_get_verify_depth(ssl_acceptor_fd->ssl_context));
  return 0;
}

static int show_ssl_ctx_get_session_cache_mode(THD *, SHOW_VAR *var, char *) {
  var->type = SHOW_CHAR;
  if (!ssl_acceptor_fd)
    var->value = const_cast<char *>("NONE");
  else
    switch (SSL_CTX_get_session_cache_mode(ssl_acceptor_fd->ssl_context)) {
      case SSL_SESS_CACHE_OFF:
        var->value = const_cast<char *>("OFF");
        break;
      case SSL_SESS_CACHE_CLIENT:
        var->value = const_cast<char *>("CLIENT");
        break;
      case SSL_SESS_CACHE_SERVER:
        var->value = const_cast<char *>("SERVER");
        break;
      case SSL_SESS_CACHE_BOTH:
        var->value = const_cast<char *>("BOTH");
        break;
      case SSL_SESS_CACHE_NO_AUTO_CLEAR:
        var->value = const_cast<char *>("NO_AUTO_CLEAR");
        break;
      case SSL_SESS_CACHE_NO_INTERNAL_LOOKUP:
        var->value = const_cast<char *>("NO_INTERNAL_LOOKUP");
        break;
      default:
        var->value = const_cast<char *>("Unknown");
        break;
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
static int show_ssl_get_version(THD *thd, SHOW_VAR *var, char *) {
  SSL_handle ssl = thd->get_ssl();
  var->type = SHOW_CHAR;
  if (ssl)
    var->value = const_cast<char *>(SSL_get_version(ssl));
  else
    var->value = (char *)"";
  return 0;
}

static int show_ssl_session_reused(THD *thd, SHOW_VAR *var, char *buff) {
  SSL_handle ssl = thd->get_ssl();
  var->type = SHOW_LONG;
  var->value = buff;
  if (ssl)
    *((long *)buff) = (long)SSL_session_reused(ssl);
  else
    *((long *)buff) = 0;
  return 0;
}

static int show_ssl_get_default_timeout(THD *thd, SHOW_VAR *var, char *buff) {
  SSL_handle ssl = thd->get_ssl();
  var->type = SHOW_LONG;
  var->value = buff;
  if (ssl)
    *((long *)buff) = (long)SSL_get_default_timeout(ssl);
  else
    *((long *)buff) = 0;
  return 0;
}

static int show_ssl_get_verify_mode(THD *thd, SHOW_VAR *var, char *buff) {
  SSL_handle ssl = thd->get_ssl();
  var->type = SHOW_LONG;
  var->value = buff;
  if (ssl)
    *((long *)buff) = (long)SSL_get_verify_mode(ssl);
  else
    *((long *)buff) = 0;
  return 0;
}

static int show_ssl_get_verify_depth(THD *thd, SHOW_VAR *var, char *buff) {
  SSL_handle ssl = thd->get_ssl();
  var->type = SHOW_LONG;
  var->value = buff;
  if (ssl)
    *((long *)buff) = (long)SSL_get_verify_depth(ssl);
  else
    *((long *)buff) = 0;
  return 0;
}

static int show_ssl_get_cipher(THD *thd, SHOW_VAR *var, char *) {
  SSL_handle ssl = thd->get_ssl();
  var->type = SHOW_CHAR;
  if (ssl)
    var->value = const_cast<char *>(SSL_get_cipher(ssl));
  else
    var->value = (char *)"";
  return 0;
}

static int show_ssl_get_cipher_list(THD *thd, SHOW_VAR *var, char *buff) {
  SSL_handle ssl = thd->get_ssl();
  var->type = SHOW_CHAR;
  var->value = buff;
  if (ssl) {
    int i;
    const char *p;
    char *end = buff + SHOW_VAR_FUNC_BUFF_SIZE;
    for (i = 0; (p = SSL_get_cipher_list(ssl, i)) && buff < end; i++) {
      buff = my_stpnmov(buff, p, end - buff - 1);
      *buff++ = ':';
    }
    if (i) buff--;
  }
  *buff = 0;
  return 0;
}

static char *my_asn1_time_to_string(ASN1_TIME *time, char *buf, int len) {
  int n_read;
  char *res = NULL;
  BIO *bio = BIO_new(BIO_s_mem());

  if (bio == NULL) return NULL;

  if (!ASN1_TIME_print(bio, time)) goto end;

  n_read = BIO_read(bio, buf, len - 1);

  if (n_read > 0) {
    buf[n_read] = 0;
    res = buf;
  }

end:
  BIO_free(bio);
  return res;
}

/**
  Handler function for the 'ssl_get_server_not_before' variable

  @param      var  the data for the variable
  @param[out] buf  the string to put the value of the variable into

  @return          status
  @retval     0    success
*/

static int show_ssl_get_server_not_before(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_CHAR;
  if (ssl_acceptor_fd) {
    X509 *cert = SSL_get_certificate(ssl_acceptor);
    ASN1_TIME *not_before = X509_get_notBefore(cert);

    if (not_before == NULL) {
      var->value = empty_c_string;
      return 0;
    }

    var->value =
        my_asn1_time_to_string(not_before, buff, SHOW_VAR_FUNC_BUFF_SIZE);
    if (var->value == NULL) {
      var->value = empty_c_string;
      return 1;
    }
  } else
    var->value = empty_c_string;
  return 0;
}

/**
  Handler function for the 'ssl_get_server_not_after' variable

  @param      var  the data for the variable
  @param[out] buf  the string to put the value of the variable into

  @return          status
  @retval     0    success
*/

static int show_ssl_get_server_not_after(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_CHAR;
  if (ssl_acceptor_fd) {
    X509 *cert = SSL_get_certificate(ssl_acceptor);
    ASN1_TIME *not_after = X509_get_notAfter(cert);

    if (not_after == NULL) {
      var->value = empty_c_string;
      return 0;
    }

    var->value =
        my_asn1_time_to_string(not_after, buff, SHOW_VAR_FUNC_BUFF_SIZE);
    if (var->value == NULL) {
      var->value = empty_c_string;
      return 1;
    }
  } else
    var->value = empty_c_string;
  return 0;
}

#endif /* HAVE_OPENSSL */

static int show_slave_open_temp_tables(THD *, SHOW_VAR *var, char *buf) {
  var->type = SHOW_INT;
  var->value = buf;
  *((int *)buf) = atomic_slave_open_temp_tables;
  return 0;
}

/*
  Variables shown by SHOW STATUS in alphabetical order
*/

SHOW_VAR status_vars[] = {
    {"Aborted_clients", (char *)&aborted_threads, SHOW_LONG, SHOW_SCOPE_GLOBAL},
    {"Aborted_connects", (char *)&show_aborted_connects, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Acl_cache_items_count", (char *)&show_acl_cache_items_count, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
#ifndef DBUG_OFF
    {"Ongoing_anonymous_gtid_violating_transaction_count",
     (char *)&show_ongoing_anonymous_gtid_violating_transaction_count,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
#endif  //! DBUG_OFF
    {"Ongoing_anonymous_transaction_count",
     (char *)&show_ongoing_anonymous_transaction_count, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
#ifndef DBUG_OFF
    {"Ongoing_automatic_gtid_violating_transaction_count",
     (char *)&show_ongoing_automatic_gtid_violating_transaction_count,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
#endif  //! DBUG_OFF
    {"Binlog_cache_disk_use", (char *)&binlog_cache_disk_use, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"Binlog_cache_use", (char *)&binlog_cache_use, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"Binlog_stmt_cache_disk_use", (char *)&binlog_stmt_cache_disk_use,
     SHOW_LONG, SHOW_SCOPE_GLOBAL},
    {"Binlog_stmt_cache_use", (char *)&binlog_stmt_cache_use, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"Bytes_received", (char *)offsetof(System_status_var, bytes_received),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Bytes_sent", (char *)offsetof(System_status_var, bytes_sent),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Com", (char *)com_status_vars, SHOW_ARRAY, SHOW_SCOPE_ALL},
    {"Com_stmt_reprepare",
     (char *)offsetof(System_status_var, com_stmt_reprepare), SHOW_LONG_STATUS,
     SHOW_SCOPE_ALL},
    {"Compression", (char *)&show_net_compression, SHOW_FUNC,
     SHOW_SCOPE_SESSION},
    {"Connections", (char *)&show_thread_id_count, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Connection_errors_accept", (char *)&show_connection_errors_accept,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Connection_errors_internal", (char *)&connection_errors_internal,
     SHOW_LONG, SHOW_SCOPE_GLOBAL},
    {"Connection_errors_max_connections",
     (char *)&show_connection_errors_max_connection, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Connection_errors_peer_address", (char *)&connection_errors_peer_addr,
     SHOW_LONG, SHOW_SCOPE_GLOBAL},
    {"Connection_errors_select", (char *)&show_connection_errors_select,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Connection_errors_tcpwrap", (char *)&show_connection_errors_tcpwrap,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Created_tmp_disk_tables",
     (char *)offsetof(System_status_var, created_tmp_disk_tables),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Created_tmp_files", (char *)&my_tmp_file_created, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"Created_tmp_tables",
     (char *)offsetof(System_status_var, created_tmp_tables),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Delayed_errors", (char *)&delayed_insert_errors, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"Delayed_insert_threads", (char *)&delayed_insert_threads,
     SHOW_LONG_NOFLUSH, SHOW_SCOPE_GLOBAL},
    {"Delayed_writes", (char *)&delayed_insert_writes, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"Flush_commands", (char *)&refresh_version, SHOW_LONG_NOFLUSH,
     SHOW_SCOPE_GLOBAL},
    {"Handler_commit", (char *)offsetof(System_status_var, ha_commit_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_delete", (char *)offsetof(System_status_var, ha_delete_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_discover", (char *)offsetof(System_status_var, ha_discover_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_external_lock",
     (char *)offsetof(System_status_var, ha_external_lock_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_mrr_init",
     (char *)offsetof(System_status_var, ha_multi_range_read_init_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_prepare", (char *)offsetof(System_status_var, ha_prepare_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_read_first",
     (char *)offsetof(System_status_var, ha_read_first_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_read_key", (char *)offsetof(System_status_var, ha_read_key_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_read_last",
     (char *)offsetof(System_status_var, ha_read_last_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_read_next",
     (char *)offsetof(System_status_var, ha_read_next_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_read_prev",
     (char *)offsetof(System_status_var, ha_read_prev_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_read_rnd", (char *)offsetof(System_status_var, ha_read_rnd_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_read_rnd_next",
     (char *)offsetof(System_status_var, ha_read_rnd_next_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_rollback", (char *)offsetof(System_status_var, ha_rollback_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_savepoint",
     (char *)offsetof(System_status_var, ha_savepoint_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_savepoint_rollback",
     (char *)offsetof(System_status_var, ha_savepoint_rollback_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_update", (char *)offsetof(System_status_var, ha_update_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Handler_write", (char *)offsetof(System_status_var, ha_write_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Key_blocks_not_flushed",
     (char *)offsetof(KEY_CACHE, global_blocks_changed), SHOW_KEY_CACHE_LONG,
     SHOW_SCOPE_GLOBAL},
    {"Key_blocks_unused", (char *)offsetof(KEY_CACHE, blocks_unused),
     SHOW_KEY_CACHE_LONG, SHOW_SCOPE_GLOBAL},
    {"Key_blocks_used", (char *)offsetof(KEY_CACHE, blocks_used),
     SHOW_KEY_CACHE_LONG, SHOW_SCOPE_GLOBAL},
    {"Key_read_requests", (char *)offsetof(KEY_CACHE, global_cache_r_requests),
     SHOW_KEY_CACHE_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"Key_reads", (char *)offsetof(KEY_CACHE, global_cache_read),
     SHOW_KEY_CACHE_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"Key_write_requests", (char *)offsetof(KEY_CACHE, global_cache_w_requests),
     SHOW_KEY_CACHE_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"Key_writes", (char *)offsetof(KEY_CACHE, global_cache_write),
     SHOW_KEY_CACHE_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"Last_query_cost", (char *)offsetof(System_status_var, last_query_cost),
     SHOW_DOUBLE_STATUS, SHOW_SCOPE_SESSION},
    {"Last_query_partial_plans",
     (char *)offsetof(System_status_var, last_query_partial_plans),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_SESSION},
    {"Locked_connects", (char *)&locked_account_connection_count, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"Max_execution_time_exceeded",
     (char *)offsetof(System_status_var, max_execution_time_exceeded),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Max_execution_time_set",
     (char *)offsetof(System_status_var, max_execution_time_set),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Max_execution_time_set_failed",
     (char *)offsetof(System_status_var, max_execution_time_set_failed),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Max_used_connections",
     (char *)&Connection_handler_manager::max_used_connections, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"Max_used_connections_time", (char *)&show_max_used_connections_time,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Not_flushed_delayed_rows", (char *)&delayed_rows_in_use,
     SHOW_LONG_NOFLUSH, SHOW_SCOPE_GLOBAL},
    {"Open_files", (char *)&my_file_opened, SHOW_LONG_NOFLUSH,
     SHOW_SCOPE_GLOBAL},
    {"Open_streams", (char *)&my_stream_opened, SHOW_LONG_NOFLUSH,
     SHOW_SCOPE_GLOBAL},
    {"Open_table_definitions", (char *)&show_table_definitions, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Open_tables", (char *)&show_open_tables, SHOW_FUNC, SHOW_SCOPE_ALL},
    {"Opened_files", (char *)&my_file_total_opened, SHOW_LONG_NOFLUSH,
     SHOW_SCOPE_GLOBAL},
    {"Opened_tables", (char *)offsetof(System_status_var, opened_tables),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Opened_table_definitions",
     (char *)offsetof(System_status_var, opened_shares), SHOW_LONGLONG_STATUS,
     SHOW_SCOPE_ALL},
    {"Prepared_stmt_count", (char *)&show_prepared_stmt_count, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Queries", (char *)&show_queries, SHOW_FUNC, SHOW_SCOPE_ALL},
    {"Questions", (char *)offsetof(System_status_var, questions),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Select_full_join",
     (char *)offsetof(System_status_var, select_full_join_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Select_full_range_join",
     (char *)offsetof(System_status_var, select_full_range_join_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Select_range", (char *)offsetof(System_status_var, select_range_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Select_range_check",
     (char *)offsetof(System_status_var, select_range_check_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Select_scan", (char *)offsetof(System_status_var, select_scan_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Slave_open_temp_tables", (char *)&show_slave_open_temp_tables, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Slave_retried_transactions", (char *)&show_slave_retried_trans, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Slave_heartbeat_period", (char *)&show_heartbeat_period, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Slave_received_heartbeats", (char *)&show_slave_received_heartbeats,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Slave_last_heartbeat", (char *)&show_slave_last_heartbeat, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
#ifndef DBUG_OFF
    {"Slave_rows_last_search_algorithm_used",
     (char *)&show_slave_rows_last_search_algorithm_used, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
#endif
    {"Slave_running", (char *)&show_slave_running, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Slow_launch_threads",
     (char *)&Per_thread_connection_handler::slow_launch_threads, SHOW_LONG,
     SHOW_SCOPE_ALL},
    {"Slow_queries", (char *)offsetof(System_status_var, long_query_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Sort_merge_passes",
     (char *)offsetof(System_status_var, filesort_merge_passes),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Sort_range", (char *)offsetof(System_status_var, filesort_range_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Sort_rows", (char *)offsetof(System_status_var, filesort_rows),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Sort_scan", (char *)offsetof(System_status_var, filesort_scan_count),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
#ifdef HAVE_OPENSSL
    {"Ssl_accept_renegotiates", (char *)&show_ssl_ctx_sess_accept_renegotiate,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Ssl_accepts", (char *)&show_ssl_ctx_sess_accept, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Ssl_callback_cache_hits", (char *)&show_ssl_ctx_sess_cb_hits, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Ssl_cipher", (char *)&show_ssl_get_cipher, SHOW_FUNC, SHOW_SCOPE_ALL},
    {"Ssl_cipher_list", (char *)&show_ssl_get_cipher_list, SHOW_FUNC,
     SHOW_SCOPE_ALL},
    {"Ssl_client_connects", (char *)&show_ssl_ctx_sess_connect, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Ssl_connect_renegotiates", (char *)&show_ssl_ctx_sess_connect_renegotiate,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Ssl_ctx_verify_depth", (char *)&show_ssl_ctx_get_verify_depth, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Ssl_ctx_verify_mode", (char *)&show_ssl_ctx_get_verify_mode, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Ssl_default_timeout", (char *)&show_ssl_get_default_timeout, SHOW_FUNC,
     SHOW_SCOPE_ALL},
    {"Ssl_finished_accepts", (char *)&show_ssl_ctx_sess_accept_good, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Ssl_finished_connects", (char *)&show_ssl_ctx_sess_connect_good,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Ssl_session_cache_hits", (char *)&show_ssl_ctx_sess_hits, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Ssl_session_cache_misses", (char *)&show_ssl_ctx_sess_misses, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Ssl_session_cache_mode", (char *)&show_ssl_ctx_get_session_cache_mode,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Ssl_session_cache_overflows", (char *)&show_ssl_ctx_sess_cache_full,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Ssl_session_cache_size", (char *)&show_ssl_ctx_sess_get_cache_size,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Ssl_session_cache_timeouts", (char *)&show_ssl_ctx_sess_timeouts,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Ssl_sessions_reused", (char *)&show_ssl_session_reused, SHOW_FUNC,
     SHOW_SCOPE_ALL},
    {"Ssl_used_session_cache_entries", (char *)&show_ssl_ctx_sess_number,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Ssl_verify_depth", (char *)&show_ssl_get_verify_depth, SHOW_FUNC,
     SHOW_SCOPE_ALL},
    {"Ssl_verify_mode", (char *)&show_ssl_get_verify_mode, SHOW_FUNC,
     SHOW_SCOPE_ALL},
    {"Ssl_version", (char *)&show_ssl_get_version, SHOW_FUNC, SHOW_SCOPE_ALL},
    {"Ssl_server_not_before", (char *)&show_ssl_get_server_not_before,
     SHOW_FUNC, SHOW_SCOPE_ALL},
    {"Ssl_server_not_after", (char *)&show_ssl_get_server_not_after, SHOW_FUNC,
     SHOW_SCOPE_ALL},
#ifndef HAVE_WOLFSSL
    {"Rsa_public_key", (char *)&show_rsa_public_key, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
#endif
#endif /* HAVE_OPENSSL */
    {"Table_locks_immediate", (char *)&locks_immediate, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"Table_locks_waited", (char *)&locks_waited, SHOW_LONG, SHOW_SCOPE_GLOBAL},
    {"Table_open_cache_hits",
     (char *)offsetof(System_status_var, table_open_cache_hits),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Table_open_cache_misses",
     (char *)offsetof(System_status_var, table_open_cache_misses),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Table_open_cache_overflows",
     (char *)offsetof(System_status_var, table_open_cache_overflows),
     SHOW_LONGLONG_STATUS, SHOW_SCOPE_ALL},
    {"Tc_log_max_pages_used", (char *)&tc_log_max_pages_used, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"Tc_log_page_size", (char *)&tc_log_page_size, SHOW_LONG_NOFLUSH,
     SHOW_SCOPE_GLOBAL},
    {"Tc_log_page_waits", (char *)&tc_log_page_waits, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"Threads_cached",
     (char *)&Per_thread_connection_handler::blocked_pthread_count,
     SHOW_LONG_NOFLUSH, SHOW_SCOPE_GLOBAL},
    {"Threads_connected", (char *)&Connection_handler_manager::connection_count,
     SHOW_INT, SHOW_SCOPE_GLOBAL},
    {"Threads_created", (char *)&show_num_thread_created, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Threads_running", (char *)&show_num_thread_running, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"Uptime", (char *)&show_starttime, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
#ifdef ENABLED_PROFILING
    {"Uptime_since_flush_status", (char *)&show_flushstatustime, SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
#endif
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_ALL}};

void add_terminator(vector<my_option> *options) {
  my_option empty_element = {0,      0, 0, 0, 0, 0, GET_NO_ARG,
                             NO_ARG, 0, 0, 0, 0, 0, 0};
  options->push_back(empty_element);
}

static void print_server_version(void) {
  set_server_version();

  print_explicit_version(server_version);
}

/** Compares two options' names, treats - and _ the same */
static bool operator<(const my_option &a, const my_option &b) {
  const char *sa = a.name;
  const char *sb = b.name;
  for (; *sa || *sb; sa++, sb++) {
    if (*sa < *sb) {
      if (*sa == '-' && *sb == '_')
        continue;
      else
        return true;
    }
    if (*sa > *sb) {
      if (*sa == '_' && *sb == '-')
        continue;
      else
        return false;
    }
  }
  DBUG_ASSERT(a.name == b.name);
  return false;
}

static void print_help() {
  MEM_ROOT mem_root;
  init_alloc_root(key_memory_help, &mem_root, 4096, 4096);

  all_options.pop_back();
  sys_var_add_options(&all_options, sys_var::PARSE_EARLY);
  for (my_option *opt = my_long_early_options; opt->name != NULL; opt++) {
    all_options.push_back(*opt);
  }
  add_plugin_options(&all_options, &mem_root);
  std::sort(all_options.begin(), all_options.end(), std::less<my_option>());
  add_terminator(&all_options);

  my_print_help(&all_options[0]);
  my_print_variables(&all_options[0]);

  free_root(&mem_root, MYF(0));
  vector<my_option>().swap(all_options);  // Deletes the vector contents.
}

static void usage(void) {
  DBUG_ENTER("usage");
  if (!(default_charset_info = get_charset_by_csname(
            default_character_set_name, MY_CS_PRIMARY, MYF(MY_WME))))
    exit(MYSQLD_ABORT_EXIT);
  if (!default_collation_name)
    default_collation_name = (char *)default_charset_info->name;
  if (opt_help || opt_verbose) {
    my_progname = my_progname + dirname_length(my_progname);
  }
  print_server_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
  puts("Starts the MySQL database server.\n");
  printf("Usage: %s [OPTIONS]\n", my_progname);
  if (!opt_verbose)
    puts(
        "\nFor more help options (several pages), use mysqld --verbose "
        "--help.");
  else {
#ifdef _WIN32
    puts(
        "NT and Win32 specific options:\n\
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
    print_defaults(MYSQL_CONFIG_NAME, load_default_groups);
    puts("");
    set_ports();

    /* Print out all the options including plugin supplied options */
    print_help();

    if (!dynamic_plugins_are_initialized) {
      puts(
          "\n\
Plugins have parameters that are not reflected in this list\n\
because execution stopped before plugins were initialized.");
    }

    puts(
        "\n\
To see what values a running MySQL server is using, type\n\
'mysqladmin variables' instead of 'mysqld --verbose --help'.");
  }
  DBUG_VOID_RETURN;
}

/**
  Initialize MySQL global variables to default values.

  @note
    The reason to set a lot of global variables to zero is that
    on some exotic platforms global variables are
    not set to 0 when a program starts.

    We don't need to set variables refered to in my_long_options
    as these are initialized by my_getopt.
*/

static int mysql_init_variables() {
  /* Things reset to zero */
  opt_skip_slave_start = 0;
  mysql_home[0] = pidfile_name[0] = 0;
  myisam_test_invalid_symlink = test_if_data_home_dir;
  opt_general_log = opt_slow_log = false;
  opt_disable_networking = opt_skip_show_db = 0;
  opt_skip_name_resolve = 0;
  opt_general_logname = opt_binlog_index_name = opt_slow_logname = NULL;
  opt_tc_log_file = (char *)"tc.log";  // no hostname in tc_log file name !
  opt_myisam_log = 0;
  mqh_used = 0;
  cleanup_done = 0;
  server_id_supplied = false;
  test_flags = select_errors = ha_open_options = 0;
  atomic_slave_open_temp_tables = 0;
  opt_endinfo = using_udf_functions = 0;
  opt_using_transactions = 0;
  set_connection_events_loop_aborted(false);
  server_operational_state = SERVER_BOOTING;
  aborted_threads = 0;
  delayed_insert_threads = delayed_insert_writes = delayed_rows_in_use = 0;
  delayed_insert_errors = 0;
  specialflag = 0;
  binlog_cache_use = binlog_cache_disk_use = 0;
  mysqld_user = mysqld_chroot = opt_init_file = opt_bin_logname = 0;
  prepared_stmt_count = 0;
  mysqld_unix_port = opt_mysql_tmpdir = my_bind_addr_str = NullS;
  new (&mysql_tmpdir_list) MY_TMPDIR;
  memset(&global_status_var, 0, sizeof(global_status_var));
  opt_large_pages = 0;
  opt_super_large_pages = 0;
#if defined(ENABLED_DEBUG_SYNC)
  opt_debug_sync_timeout = 0;
#endif /* defined(ENABLED_DEBUG_SYNC) */
  server_uuid[0] = 0;

  /* Character sets */
  system_charset_info = &my_charset_utf8_general_ci;
  files_charset_info = &my_charset_utf8_general_ci;
  national_charset_info = &my_charset_utf8_general_ci;
  table_alias_charset = &my_charset_bin;
  character_set_filesystem = &my_charset_bin;
  default_collation_for_utf8mb4 = &my_charset_utf8mb4_0900_ai_ci;

  opt_specialflag = 0;
  mysql_home_ptr = mysql_home;
  pidfile_name_ptr = pidfile_name;
  lc_messages_dir_ptr = lc_messages_dir;
  protocol_version = PROTOCOL_VERSION;
  what_to_log = ~(1L << (uint)COM_TIME);
  refresh_version = 1L; /* Increments on each reload */
  my_stpcpy(server_version, MYSQL_SERVER_VERSION);
  key_caches.empty();
  if (!(dflt_key_cache = get_or_create_key_cache(
            default_key_cache_base.str, default_key_cache_base.length))) {
    LogErr(ERROR_LEVEL, ER_KEYCACHE_OOM);
    return 1;
  }
  /* set key_cache_hash.default_value = dflt_key_cache */
  multi_keycache_init();

  /* Replication parameters */
  master_info_file = (char *)"master.info",
  relay_log_info_file = (char *)"relay-log.info";
  report_user = report_password = report_host = 0; /* TO BE DELETED */
  opt_relay_logname = opt_relaylog_index_name = 0;
  opt_relaylog_index_name_supplied = false;
  opt_relay_logname_supplied = false;
  log_bin_basename = NULL;
  log_bin_index = NULL;

  /* Handler variables */
  total_ha_2pc = 0;
  /* Variables in libraries */
  charsets_dir = 0;
  default_character_set_name = (char *)MYSQL_DEFAULT_CHARSET_NAME;
  default_collation_name = compiled_default_collation_name;
  default_collation_name_for_utf8mb4 =
      compiled_default_collation_name_for_utf8mb4;
  character_set_filesystem_name = (char *)"binary";
  lc_messages = (char *)mysqld_default_locale_name;
  lc_time_names_name = (char *)mysqld_default_locale_name;

  /* Variables that depends on compile options */
#ifndef DBUG_OFF
  default_dbug_option =
      IF_WIN("d:t:i:O,\\mysqld.trace", "d:t:i:o,/tmp/mysqld.trace");
#endif
#ifdef ENABLED_PROFILING
  have_profiling = SHOW_OPTION_YES;
#else
  have_profiling = SHOW_OPTION_NO;
#endif

#ifdef HAVE_OPENSSL
  have_ssl = SHOW_OPTION_YES;
#else
  have_ssl = SHOW_OPTION_NO;
#endif

  have_symlink = SHOW_OPTION_YES;

  have_dlopen = SHOW_OPTION_YES;

  have_query_cache = SHOW_OPTION_NO;

  have_geometry = SHOW_OPTION_YES;

  have_rtree_keys = SHOW_OPTION_YES;

  /* Always true */
  have_compress = SHOW_OPTION_YES;
#ifdef HAVE_OPENSSL
  ssl_acceptor_fd = 0;
#endif /* HAVE_OPENSSL */
#if defined(_WIN32)
  shared_memory_base_name = default_shared_memory_base_name;
#endif

#if defined(_WIN32) || defined(APPLE_XCODE)
  /* Allow Win32 users to move MySQL anywhere */
  char prg_dev[LIBLEN];
  my_path(prg_dev, my_progname, nullptr);

  // On windows or Xcode the basedir will always be one level up from where
  // the executable is located. E.g. <basedir>/bin/mysqld.exe in a
  // package, or <basedir>/runtime_output_directory/<buildconfig>/mysqld.exe
  // for a sandbox build.
  strcat(prg_dev, "/../");  // Remove containing directory to get base dir
  cleanup_dirname(mysql_home, prg_dev);

  // New layout: <cmake_binary_dir>/runtime_output_directory/<buildconfig>/
  char cmake_binary_dir[FN_REFLEN];
  size_t dlen = 0;
  dirname_part(cmake_binary_dir, mysql_home, &dlen);
  if (dlen > 26U &&
      (!strcmp(cmake_binary_dir + (dlen - 26), "/runtime_output_directory/") ||
       !strcmp(cmake_binary_dir + (dlen - 26),
               "\\runtime_output_directory\\"))) {
    mysql_home[strlen(mysql_home) - 1] = '\0';  // remove trailing
    dirname_part(cmake_binary_dir, mysql_home, &dlen);
    strmake(mysql_home, cmake_binary_dir, sizeof(mysql_home) - 1);
  }
    // The sql_print_information below outputs nothing ??
    // fprintf(stderr, "mysql_home %s\n", mysql_home);
    // fflush(stderr);
#else
  const char *tmpenv = getenv("MY_BASEDIR_VERSION");
  if (tmpenv != nullptr) {
    strmake(mysql_home, tmpenv, sizeof(mysql_home) - 1);
  } else {
    char progdir[FN_REFLEN];
    size_t dlen = 0;
    dirname_part(progdir, my_progname, &dlen);
    if (dlen > 26U &&
        !strcmp(progdir + (dlen - 26), "/runtime_output_directory/")) {
      char cmake_binary_dir[FN_REFLEN];
      progdir[strlen(progdir) - 1] = '\0';  // remove trailing "/"
      dirname_part(cmake_binary_dir, progdir, &dlen);
      strmake(mysql_home, cmake_binary_dir, sizeof(mysql_home) - 1);
    } else {
      strcat(progdir, "/../");
      cleanup_dirname(mysql_home, progdir);
    }
  }
#endif

  return 0;
}

/**
  Check if it is a global replication filter setting.

  @param argument The setting of startup option --replicate-*.

  @retval
    0    OK
  @retval
    1    Error
*/
static bool is_rpl_global_filter_setting(char *argument) {
  DBUG_ENTER("is_rpl_global_filter_setting");

  bool res = false;
  char *p = strchr(argument, ':');
  if (p == NULL) res = true;

  DBUG_RETURN(res);
}

/**
  Extract channel name and filter value from argument.

  @param [out] channel_name The name of the channel.
  @param [out] filter_val The value of filter.
  @param argument The setting of startup option --replicate-*.
*/
void parse_filter_arg(char **channel_name, char **filter_val, char *argument) {
  DBUG_ENTER("parse_filter_arg");

  char *p = strchr(argument, ':');

  DBUG_ASSERT(p != NULL);

  /*
    If argument='channel_1:db1', then channel_name='channel_1'
    and filter_val='db1'; If argument=':db1', then channel_name=''
    and filter_val='db1'.
  */
  *channel_name = argument;
  *filter_val = p + 1;
  *p = 0;

  DBUG_VOID_RETURN;
}

/**
  Extract channel name and filter value from argument.

  @param [out] key The db is rewritten from.
  @param [out] val The db is rewritten to.
  @param argument The value of filter.

  @retval
    0    OK
  @retval
    1    Error
*/
static int parse_replicate_rewrite_db(char **key, char **val, char *argument) {
  DBUG_ENTER("parse_replicate_rewrite_db");
  char *p;
  *key = argument;

  if (!(p = strstr(argument, "->"))) {
    LogErr(ERROR_LEVEL, ER_RPL_REWRITEDB_MISSING_ARROW);
    DBUG_RETURN(1);
  }
  *val = p + 2;

  while (p > argument && my_isspace(mysqld_charset, p[-1])) p--;
  *p = 0;

  if (!**key) {
    LogErr(ERROR_LEVEL, ER_RPL_REWRITEDB_EMPTY_FROM);
    DBUG_RETURN(1);
  }
  while (**val && my_isspace(mysqld_charset, **val)) (*val)++;
  if (!**val) {
    LogErr(ERROR_LEVEL, ER_RPL_REWRITEDB_EMPTY_TO);
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}

bool mysqld_get_one_option(int optid,
                           const struct my_option *opt MY_ATTRIBUTE((unused)),
                           char *argument) {
  Rpl_filter *rpl_filter = NULL;
  char *filter_val;
  char *channel_name;

  switch (optid) {
    case '#':
#ifndef DBUG_OFF
      DBUG_SET_INITIAL(argument ? argument : default_dbug_option);
#endif
      opt_endinfo = 1; /* unireg: memory allocation */
      break;
    case 'a':
      global_system_variables.sql_mode = MODE_ANSI;
      global_system_variables.transaction_isolation = ISO_SERIALIZABLE;
      break;
    case 'b':
      strmake(mysql_home, argument, sizeof(mysql_home) - 1);
      mysql_home_ptr = mysql_home;
      break;
    case 'C':
      if (default_collation_name == compiled_default_collation_name)
        default_collation_name = 0;
      break;
    case 'h':
      strmake(mysql_real_data_home, argument, sizeof(mysql_real_data_home) - 1);
      /* Correct pointer set by my_getopt */
      mysql_real_data_home_ptr = mysql_real_data_home;
      break;
    case 'u':
      if (!mysqld_user || !strcmp(mysqld_user, argument))
        mysqld_user = argument;
      else
        LogErr(WARNING_LEVEL, ER_THE_USER_ABIDES, argument, mysqld_user);
      break;
    case 's':
      if (argument[0] == '0') {
        LogErr(WARNING_LEVEL, ER_DEPRECATE_MSG_NO_REPLACEMENT,
               "Disabling symbolic links using --skip-symbolic-links"
               " (or equivalent) is the default. Consider not using"
               " this option as it");
      } else {
        LogErr(WARNING_LEVEL, ER_DEPRECATE_MSG_NO_REPLACEMENT,
               "Enabling symbolic using --symbolic-links/-s (or equivalent)");
      }
      break;
    case 'L':
      push_deprecated_warn(NULL, "--language/-l", "'--lc-messages-dir'");
      /* Note:  fall-through */
    case OPT_LC_MESSAGES_DIRECTORY:
      strmake(lc_messages_dir, argument, sizeof(lc_messages_dir) - 1);
      lc_messages_dir_ptr = lc_messages_dir;
      break;
    case OPT_BINLOG_FORMAT:
      binlog_format_used = true;
      break;
    case OPT_BINLOG_MAX_FLUSH_QUEUE_TIME:
      push_deprecated_warn_no_replacement(NULL,
                                          "--binlog_max_flush_queue_time");
      break;
    case OPT_EXPIRE_LOGS_DAYS:
      push_deprecated_warn(NULL, "expire-logs-days",
                           "binlog_expire_logs_seconds");
      expire_logs_days_supplied = true;
      break;
    case OPT_BINLOG_EXPIRE_LOGS_SECONDS:
      binlog_expire_logs_seconds_supplied = true;
      break;
#if defined(HAVE_OPENSSL)
    case OPT_SSL_KEY:
    case OPT_SSL_CERT:
    case OPT_SSL_CA:
    case OPT_SSL_CAPATH:
    case OPT_SSL_CIPHER:
    case OPT_SSL_CRL:
    case OPT_SSL_CRLPATH:
    case OPT_TLS_VERSION:
      /*
        Enable use of SSL if we are using any ssl option.
        One can disable SSL later by using --skip-ssl or --ssl=0.
      */
      opt_use_ssl = true;
#ifdef HAVE_WOLFSSL
      /* crl has no effect in wolfSSL. */
      opt_ssl_crl = NULL;
      opt_ssl_crlpath = NULL;
      opt_ssl_fips_mode = SSL_FIPS_MODE_OFF;
#endif /* HAVE_WOLFSSL */
      break;
#endif /* HAVE_OPENSSL */
    case 'V':
      print_server_version();
      exit(MYSQLD_SUCCESS_EXIT);
    case 'T':
      test_flags = argument ? (uint)atoi(argument) : 0;
      opt_endinfo = 1;
      break;
    case (int)OPT_ISAM_LOG:
      opt_myisam_log = 1;
      break;
    case (int)OPT_BIN_LOG:
      opt_bin_log = (argument != disabled_my_option);
      if (!opt_bin_log) {
        // Clear the binlog basename used by any previous --log-bin
        if (opt_bin_logname) {
          my_free(opt_bin_logname);
          opt_bin_logname = NULL;
        }
      }
      log_bin_supplied = true;
      break;
    case (int)OPT_REPLICATE_IGNORE_DB: {
      if (is_rpl_global_filter_setting(argument)) {
        rpl_global_filter.add_ignore_db(argument);
        rpl_global_filter.ignore_db_statistics.set_all(
            CONFIGURED_BY_STARTUP_OPTIONS);
      } else {
        parse_filter_arg(&channel_name, &filter_val, argument);
        rpl_filter = rpl_channel_filters.get_channel_filter(channel_name);
        rpl_filter->add_ignore_db(filter_val);
        rpl_filter->ignore_db_statistics.set_all(
            CONFIGURED_BY_STARTUP_OPTIONS_FOR_CHANNEL);
      }
      break;
    }
    case (int)OPT_REPLICATE_DO_DB: {
      if (is_rpl_global_filter_setting(argument)) {
        rpl_global_filter.add_do_db(argument);
        rpl_global_filter.do_db_statistics.set_all(
            CONFIGURED_BY_STARTUP_OPTIONS);
      } else {
        parse_filter_arg(&channel_name, &filter_val, argument);
        rpl_filter = rpl_channel_filters.get_channel_filter(channel_name);
        rpl_filter->add_do_db(filter_val);
        rpl_filter->do_db_statistics.set_all(
            CONFIGURED_BY_STARTUP_OPTIONS_FOR_CHANNEL);
      }
      break;
    }
    case (int)OPT_REPLICATE_REWRITE_DB: {
      char *key, *val;
      if (is_rpl_global_filter_setting(argument)) {
        if (parse_replicate_rewrite_db(&key, &val, argument)) return 1;
        rpl_global_filter.add_db_rewrite(key, val);
        rpl_global_filter.rewrite_db_statistics.set_all(
            CONFIGURED_BY_STARTUP_OPTIONS);
      } else {
        parse_filter_arg(&channel_name, &filter_val, argument);
        rpl_filter = rpl_channel_filters.get_channel_filter(channel_name);
        if (parse_replicate_rewrite_db(&key, &val, filter_val)) return 1;
        rpl_filter->add_db_rewrite(key, val);
        rpl_filter->rewrite_db_statistics.set_all(
            CONFIGURED_BY_STARTUP_OPTIONS_FOR_CHANNEL);
      }
      break;
    }

    case (int)OPT_BINLOG_IGNORE_DB: {
      binlog_filter->add_ignore_db(argument);
      break;
    }
    case (int)OPT_BINLOG_DO_DB: {
      binlog_filter->add_do_db(argument);
      break;
    }
    case (int)OPT_REPLICATE_DO_TABLE: {
      if (is_rpl_global_filter_setting(argument)) {
        if (rpl_global_filter.add_do_table_array(argument)) {
          LogErr(ERROR_LEVEL, ER_RPL_CANT_ADD_DO_TABLE, argument);
          return 1;
        }
        rpl_global_filter.do_table_statistics.set_all(
            CONFIGURED_BY_STARTUP_OPTIONS);
      } else {
        parse_filter_arg(&channel_name, &filter_val, argument);
        rpl_filter = rpl_channel_filters.get_channel_filter(channel_name);
        if (rpl_filter->add_do_table_array(filter_val)) {
          LogErr(ERROR_LEVEL, ER_RPL_CANT_ADD_DO_TABLE, argument);
          return 1;
        }
        rpl_filter->do_table_statistics.set_all(
            CONFIGURED_BY_STARTUP_OPTIONS_FOR_CHANNEL);
      }
      break;
    }
    case (int)OPT_REPLICATE_WILD_DO_TABLE: {
      if (is_rpl_global_filter_setting(argument)) {
        if (rpl_global_filter.add_wild_do_table(argument)) {
          LogErr(ERROR_LEVEL, ER_RPL_FILTER_ADD_WILD_DO_TABLE_FAILED, argument);
          return 1;
        }
        rpl_global_filter.wild_do_table_statistics.set_all(
            CONFIGURED_BY_STARTUP_OPTIONS);
      } else {
        parse_filter_arg(&channel_name, &filter_val, argument);
        rpl_filter = rpl_channel_filters.get_channel_filter(channel_name);
        if (rpl_filter->add_wild_do_table(filter_val)) {
          LogErr(ERROR_LEVEL, ER_RPL_FILTER_ADD_WILD_DO_TABLE_FAILED, argument);
          return 1;
        }
        rpl_filter->wild_do_table_statistics.set_all(
            CONFIGURED_BY_STARTUP_OPTIONS_FOR_CHANNEL);
      }
      break;
    }
    case (int)OPT_REPLICATE_WILD_IGNORE_TABLE: {
      if (is_rpl_global_filter_setting(argument)) {
        if (rpl_global_filter.add_wild_ignore_table(argument)) {
          LogErr(ERROR_LEVEL, ER_RPL_FILTER_ADD_WILD_IGNORE_TABLE_FAILED,
                 argument);
          return 1;
        }
        rpl_global_filter.wild_ignore_table_statistics.set_all(
            CONFIGURED_BY_STARTUP_OPTIONS);
      } else {
        parse_filter_arg(&channel_name, &filter_val, argument);
        rpl_filter = rpl_channel_filters.get_channel_filter(channel_name);
        if (rpl_filter->add_wild_ignore_table(filter_val)) {
          LogErr(ERROR_LEVEL, ER_RPL_FILTER_ADD_WILD_IGNORE_TABLE_FAILED,
                 argument);
          return 1;
        }
        rpl_filter->wild_ignore_table_statistics.set_all(
            CONFIGURED_BY_STARTUP_OPTIONS_FOR_CHANNEL);
      }
      break;
    }
    case (int)OPT_REPLICATE_IGNORE_TABLE: {
      if (is_rpl_global_filter_setting(argument)) {
        if (rpl_global_filter.add_ignore_table_array(argument)) {
          LogErr(ERROR_LEVEL, ER_RPL_CANT_ADD_IGNORE_TABLE, argument);
          return 1;
        }
        rpl_global_filter.ignore_table_statistics.set_all(
            CONFIGURED_BY_STARTUP_OPTIONS);
      } else {
        parse_filter_arg(&channel_name, &filter_val, argument);
        rpl_filter = rpl_channel_filters.get_channel_filter(channel_name);
        if (rpl_filter->add_ignore_table_array(filter_val)) {
          LogErr(ERROR_LEVEL, ER_RPL_CANT_ADD_IGNORE_TABLE, argument);
          return 1;
        }
        rpl_filter->ignore_table_statistics.set_all(
            CONFIGURED_BY_STARTUP_OPTIONS_FOR_CHANNEL);
      }
      break;
    }
    case (int)OPT_MASTER_RETRY_COUNT:
      push_deprecated_warn(NULL, "--master-retry-count",
                           "'CHANGE MASTER TO master_retry_count = <num>'");
      break;
    case (int)OPT_SKIP_NEW:
      opt_specialflag |= SPECIAL_NO_NEW_FUNC;
      delay_key_write_options = DELAY_KEY_WRITE_NONE;
      myisam_concurrent_insert = 0;
      myisam_recover_options = HA_RECOVER_OFF;
      sp_automatic_privileges = 0;
      my_enable_symlinks = 0;
      ha_open_options &= ~(HA_OPEN_ABORT_IF_CRASHED | HA_OPEN_DELAY_KEY_WRITE);
      break;
    case (int)OPT_SKIP_HOST_CACHE:
      opt_specialflag |= SPECIAL_NO_HOST_CACHE;
      break;
    case (int)OPT_SKIP_RESOLVE:
      opt_skip_name_resolve = 1;
      opt_specialflag |= SPECIAL_NO_RESOLVE;
      break;
    case (int)OPT_WANT_CORE:
      test_flags |= TEST_CORE_ON_SIGNAL;
      break;
    case (int)OPT_SKIP_STACK_TRACE:
      test_flags |= TEST_NO_STACKTRACE;
      break;
    case OPT_SERVER_ID:
      /*
       Consider that one received a Server Id when 2 conditions are present:
       1) The argument is on the list
       2) There is a value present
      */
      server_id_supplied = (*argument != 0);
      break;
    case OPT_LOWER_CASE_TABLE_NAMES:
      lower_case_table_names_used = 1;
      break;
#if defined(ENABLED_DEBUG_SYNC)
    case OPT_DEBUG_SYNC_TIMEOUT:
      /*
        Debug Sync Facility. See debug_sync.cc.
        Default timeout for WAIT_FOR action.
        Default value is zero (facility disabled).
        If option is given without an argument, supply a non-zero value.
      */
      if (!argument) {
        /* purecov: begin tested */
        opt_debug_sync_timeout = DEBUG_SYNC_DEFAULT_WAIT_TIMEOUT;
        /* purecov: end */
      }
      break;
#endif /* defined(ENABLED_DEBUG_SYNC) */
    case OPT_LOG_ERROR:
      /*
        "No --log-error" == "write errors to stderr",
        "--log-error without argument" == "write errors to a file".
      */
      if (argument == NULL) /* no argument */
        log_error_dest = "";
      break;

    case OPT_EARLY_PLUGIN_LOAD:
      free_list(opt_early_plugin_load_list_ptr);
      opt_early_plugin_load_list_ptr->push_back(new i_string(argument));
      break;
    case OPT_PLUGIN_LOAD:
      free_list(opt_plugin_load_list_ptr);
      /* fall through */
    case OPT_PLUGIN_LOAD_ADD:
      opt_plugin_load_list_ptr->push_back(new i_string(argument));
      break;
    case OPT_PFS_INSTRUMENT: {
#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
      /*
        Parse instrument name and value from argument string. Handle leading
        and trailing spaces. Also handle single quotes.

        Acceptable:
          performance_schema_instrument = ' foo/%/bar/  =  ON  '
          performance_schema_instrument = '%=OFF'
        Not acceptable:
          performance_schema_instrument = '' foo/%/bar = ON ''
          performance_schema_instrument = '%='OFF''
      */
      char *name = argument, *p = NULL, *val = NULL;
      bool quote = false; /* true if quote detected */
      bool error = true;  /* false if no errors detected */
      const int PFS_BUFFER_SIZE = 128;
      char orig_argument[PFS_BUFFER_SIZE + 1];
      orig_argument[0] = 0;

      if (!argument) goto pfs_error;

      /* Save original argument string for error reporting */
      strncpy(orig_argument, argument, PFS_BUFFER_SIZE);

      /* Split instrument name and value at the equal sign */
      if (!(p = strchr(argument, '='))) goto pfs_error;

      /* Get option value */
      val = p + 1;
      if (!*val) goto pfs_error;

      /* Trim leading spaces and quote from the instrument name */
      while (*name && (my_isspace(mysqld_charset, *name) || (*name == '\''))) {
        /* One quote allowed */
        if (*name == '\'') {
          if (!quote)
            quote = true;
          else
            goto pfs_error;
        }
        name++;
      }

      /* Trim trailing spaces from instrument name */
      while ((p > name) && my_isspace(mysqld_charset, p[-1])) p--;
      *p = 0;

      /* Remove trailing slash from instrument name */
      if (p > name && (p[-1] == '/')) p[-1] = 0;

      if (!*name) goto pfs_error;

      /* Trim leading spaces from option value */
      while (*val && my_isspace(mysqld_charset, *val)) val++;

      /* Trim trailing spaces and matching quote from value */
      p = val + strlen(val);
      while (p > val && (my_isspace(mysqld_charset, p[-1]) || p[-1] == '\'')) {
        /* One matching quote allowed */
        if (p[-1] == '\'') {
          if (quote)
            quote = false;
          else
            goto pfs_error;
        }
        p--;
      }

      *p = 0;

      if (!*val) goto pfs_error;

      /* Add instrument name and value to array of configuration options */
      if (add_pfs_instr_to_array(name, val)) goto pfs_error;

      error = false;

    pfs_error:
      if (error) {
        LogErr(WARNING_LEVEL, ER_INVALID_INSTRUMENT, orig_argument);
        return 0;
      }
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */
      break;
    }
    case OPT_THREAD_CACHE_SIZE:
      thread_cache_size_specified = true;
      break;
    case OPT_HOST_CACHE_SIZE:
      host_cache_size_specified = true;
      break;
    case OPT_TABLE_DEFINITION_CACHE:
      table_definition_cache_specified = true;
      break;
    case OPT_MDL_CACHE_SIZE:
      push_deprecated_warn_no_replacement(NULL, "--metadata_locks_cache_size");
      break;
    case OPT_MDL_HASH_INSTANCES:
      push_deprecated_warn_no_replacement(NULL,
                                          "--metadata_locks_hash_instances");
      break;
    case OPT_SKIP_INNODB:
      LogErr(WARNING_LEVEL, ER_INNODB_MANDATORY);
      break;
    case OPT_AVOID_TEMPORAL_UPGRADE:
      push_deprecated_warn_no_replacement(NULL, "avoid_temporal_upgrade");
      break;
    case OPT_SHOW_OLD_TEMPORALS:
      push_deprecated_warn_no_replacement(NULL, "show_old_temporals");
      break;
    case 'p':
      if (argument) {
        char *start = argument;
        my_free(opt_keyring_migration_password);
        opt_keyring_migration_password =
            my_strdup(PSI_NOT_INSTRUMENTED, argument, MYF(MY_FAE));
        while (*argument) *argument++ = 'x';
        if (*start) start[1] = 0;
      } else
        opt_keyring_migration_password = get_tty_password(NullS);
      migrate_connect_options = 1;
      break;
    case OPT_KEYRING_MIGRATION_USER:
    case OPT_KEYRING_MIGRATION_HOST:
    case OPT_KEYRING_MIGRATION_SOCKET:
    case OPT_KEYRING_MIGRATION_PORT:
      migrate_connect_options = 1;
      break;
    case OPT_LOG_SLAVE_UPDATES:
      log_slave_updates_supplied = true;
      break;
    case OPT_SLAVE_PRESERVE_COMMIT_ORDER:
      slave_preserve_commit_order_supplied = true;
      break;
    case OPT_ENFORCE_GTID_CONSISTENCY: {
      const char *wrong_value =
          fixup_enforce_gtid_consistency_command_line(argument);
      if (wrong_value != NULL)
        LogErr(WARNING_LEVEL, ER_INVALID_VALUE_FOR_ENFORCE_GTID_CONSISTENCY,
               wrong_value);
    }
  }
  return 0;
}

/** Handle arguments for multiple key caches. */

C_MODE_START

static void *mysql_getopt_value(const char *keyname, size_t key_length,
                                const struct my_option *option, int *error) {
  if (error) *error = 0;
  switch (option->id) {
    case OPT_KEY_BUFFER_SIZE:
    case OPT_KEY_CACHE_BLOCK_SIZE:
    case OPT_KEY_CACHE_DIVISION_LIMIT:
    case OPT_KEY_CACHE_AGE_THRESHOLD: {
      KEY_CACHE *key_cache;
      if (!(key_cache = get_or_create_key_cache(keyname, key_length))) {
        if (error) *error = EXIT_OUT_OF_MEMORY;
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
static int get_options(int *argc_ptr, char ***argv_ptr) {
  int ho_error;

  my_getopt_register_get_addr(mysql_getopt_value);

  /* prepare all_options array */
  all_options.reserve(array_elements(my_long_options));
  for (my_option *opt = my_long_options;
       opt < my_long_options + array_elements(my_long_options) - 1; opt++) {
    all_options.push_back(*opt);
  }
  sys_var_add_options(&all_options, sys_var::PARSE_NORMAL);
  add_terminator(&all_options);

  if (opt_help || opt_initialize) {
    /*
      Show errors during --help, but mute everything else so the info the
      user actually wants isn't lost in the spam.  (For --help --verbose,
      we need to set up far enough to be able to print variables provided
      by plugins, so a good number of warnings/notes might get printed.)
      Likewise for --initialize.
    */
    struct my_option *opt = &all_options[0];
    for (; opt->name; opt++)
      if (!strcmp("log_error_verbosity", opt->name))
        opt->def_value = opt_initialize ? 2 : 1;
  }

  /* Skip unknown options so that they may be processed later by plugins */
  my_getopt_skip_unknown = true;

  if ((ho_error = handle_options(argc_ptr, argv_ptr, &all_options[0],
                                 mysqld_get_one_option)))
    return ho_error;

  // update verbosity in filter engine, if needed
  log_builtins_filter_update_verbosity(log_error_verbosity);

  if (!opt_help)
    vector<my_option>().swap(all_options);  // Deletes the vector contents.

  /* Add back the program name handle_options removes */
  (*argc_ptr)++;
  (*argv_ptr)--;

  /*
    Options have been parsed. Now some of them need additional special
    handling, like custom value checking, checking of incompatibilites
    between options, setting of multiple variables, etc.
    Do them here.
  */

  if (!opt_help && opt_verbose) LogErr(ERROR_LEVEL, ER_VERBOSE_REQUIRES_HELP);

  if ((opt_log_slow_admin_statements || opt_log_queries_not_using_indexes ||
       opt_log_slow_slave_statements) &&
      !opt_slow_log)
    LogErr(WARNING_LEVEL, ER_POINTLESS_WITHOUT_SLOWLOG);

  if (global_system_variables.net_buffer_length >
      global_system_variables.max_allowed_packet) {
    LogErr(WARNING_LEVEL, ER_WASTEFUL_NET_BUFFER_SIZE,
           global_system_variables.net_buffer_length,
           global_system_variables.max_allowed_packet);
  }

  /*
    TIMESTAMP columns get implicit DEFAULT values when
    --explicit_defaults_for_timestamp is not set.
    This behavior is deprecated now.
  */
  if (!opt_help && !global_system_variables.explicit_defaults_for_timestamp)
    LogErr(WARNING_LEVEL, ER_DEPRECATED_TIMESTAMP_IMPLICIT_DEFAULTS);

  if (!opt_help && opt_mi_repository_id == INFO_REPOSITORY_FILE)
    push_deprecated_warn(NULL, "--master-info-repository=FILE",
                         "'--master-info-repository=TABLE'");

  if (!opt_help && opt_rli_repository_id == INFO_REPOSITORY_FILE)
    push_deprecated_warn(NULL, "--relay-log-info-repository=FILE",
                         "'--relay-log-info-repository=TABLE'");

  opt_init_connect.length = strlen(opt_init_connect.str);
  opt_init_slave.length = strlen(opt_init_slave.str);
  opt_mandatory_roles.length = strlen(opt_mandatory_roles.str);

  if (global_system_variables.low_priority_updates)
    thr_upgraded_concurrent_insert_lock = TL_WRITE_LOW_PRIORITY;

  if (ft_boolean_check_syntax_string((uchar *)ft_boolean_syntax)) {
    LogErr(ERROR_LEVEL, ER_FT_BOOL_SYNTAX_INVALID, ft_boolean_syntax);
    return 1;
  }

  if (opt_noacl && !opt_help) opt_disable_networking = true;

  if (opt_disable_networking) mysqld_port = 0;

  if (opt_skip_show_db) opt_specialflag |= SPECIAL_SKIP_SHOW_DB;

  if (myisam_flush) flush_time = 0;

  if (opt_slave_skip_errors) add_slave_skip_errors(opt_slave_skip_errors);

  if (global_system_variables.max_join_size == HA_POS_ERROR)
    global_system_variables.option_bits |= OPTION_BIG_SELECTS;
  else
    global_system_variables.option_bits &= ~OPTION_BIG_SELECTS;

  // Synchronize @@global.autocommit value on --autocommit
  const ulonglong turn_bit_on =
      opt_autocommit ? OPTION_AUTOCOMMIT : OPTION_NOT_AUTOCOMMIT;
  global_system_variables.option_bits =
      (global_system_variables.option_bits &
       ~(OPTION_NOT_AUTOCOMMIT | OPTION_AUTOCOMMIT)) |
      turn_bit_on;

  // Synchronize @@global.autocommit metadata on --autocommit
  my_option *opt = &my_long_options[3];
  DBUG_ASSERT(strcmp(opt->name, "autocommit") == 0);
  DBUG_ASSERT(opt->arg_source != NULL);
  Sys_autocommit_ptr->set_source_name(opt->arg_source->m_path_name);
  Sys_autocommit_ptr->set_source(opt->arg_source->m_source);

  global_system_variables.sql_mode =
      expand_sql_mode(global_system_variables.sql_mode, NULL);

  if (!my_enable_symlinks) have_symlink = SHOW_OPTION_DISABLED;

  if (opt_debugging) {
    /* Allow break with SIGINT, no core or stack trace */
    test_flags |= TEST_SIGINT | TEST_NO_STACKTRACE;
    test_flags &= ~TEST_CORE_ON_SIGNAL;
  }
  /* Set global MyISAM variables from delay_key_write_options */
  fix_delay_key_write(0, 0, OPT_GLOBAL);

#ifndef _WIN32
  if (mysqld_chroot) set_root(mysqld_chroot);
#endif
  if (fix_paths()) return 1;

  /*
    Set some global variables from the global_system_variables
    In most cases the global variables will not be used
  */
  my_disable_locking = myisam_single_user = (opt_external_locking == 0);
  my_default_record_cache_size = global_system_variables.read_buff_size;

  global_system_variables.long_query_time =
      (ulonglong)(global_system_variables.long_query_time_double * 1e6);

  if (opt_short_log_format) opt_specialflag |= SPECIAL_SHORT_LOG_FORMAT;

  if (Connection_handler_manager::init()) {
    LogErr(ERROR_LEVEL, ER_CONNECTION_HANDLING_OOM);
    return 1;
  }
  if (Global_THD_manager::create_instance()) {
    LogErr(ERROR_LEVEL, ER_THREAD_HANDLING_OOM);
    return 1;
  }

  /* If --super-read-only was specified, set read_only to 1 */
  read_only = super_read_only ? super_read_only : read_only;
  opt_readonly = read_only;

  return 0;
}

/*
  Create version name for running mysqld version
  We automaticly add suffixes -debug, -valgrind, -asan, -ubsan
  to the version name to make the version more descriptive.
  (MYSQL_SERVER_SUFFIX is set by the compilation environment)
*/

/*
  The following code is quite ugly as there is no portable way to easily set a
  string to the value of a macro
*/
#ifdef MYSQL_SERVER_SUFFIX
#define MYSQL_SERVER_SUFFIX_STR STRINGIFY_ARG(MYSQL_SERVER_SUFFIX)
#else
#define MYSQL_SERVER_SUFFIX_STR MYSQL_SERVER_SUFFIX_DEF
#endif

static void set_server_version(void) {
  char *end MY_ATTRIBUTE((unused)) = strxmov(
      server_version, MYSQL_SERVER_VERSION, MYSQL_SERVER_SUFFIX_STR, NullS);
#ifndef DBUG_OFF
  if (!strstr(MYSQL_SERVER_SUFFIX_STR, "-debug"))
    end = my_stpcpy(end, "-debug");
#endif
#ifdef HAVE_VALGRIND
  if (SERVER_VERSION_LENGTH - (end - server_version) >
      static_cast<int>(sizeof("-valgrind")))
    end = my_stpcpy(end, "-valgrind");
#endif
#ifdef HAVE_ASAN
  if (SERVER_VERSION_LENGTH - (end - server_version) >
      static_cast<int>(sizeof("-asan")))
    end = my_stpcpy(end, "-asan");
#endif
#ifdef HAVE_UBSAN
  if (SERVER_VERSION_LENGTH - (end - server_version) >
      static_cast<int>(sizeof("-ubsan")))
    end = my_stpcpy(end, "-ubsan");
#endif
}

static char *get_relative_path(const char *path) {
  if (test_if_hard_path(path) && is_prefix(path, DEFAULT_MYSQL_HOME) &&
      strcmp(DEFAULT_MYSQL_HOME, FN_ROOTDIR)) {
    path += strlen(DEFAULT_MYSQL_HOME);
    while (is_directory_separator(*path)) path++;
  }
  return (char *)path;
}

/**
  Test a file path to determine if the path is compatible with the secure file
  path restriction.

  @param path null terminated character string

  @return
    @retval true The path is secure
    @retval false The path isn't secure
*/

bool is_secure_file_path(const char *path) {
  char buff1[FN_REFLEN], buff2[FN_REFLEN];
  size_t opt_secure_file_priv_len;
  /*
    All paths are secure if opt_secure_file_priv is 0
  */
  if (!opt_secure_file_priv[0]) return true;

  opt_secure_file_priv_len = strlen(opt_secure_file_priv);

  if (strlen(path) >= FN_REFLEN) return false;

  if (!my_strcasecmp(system_charset_info, opt_secure_file_priv, "NULL"))
    return false;

  if (my_realpath(buff1, path, 0)) {
    /*
      The supplied file path might have been a file and not a directory.
    */
    int length = (int)dirname_length(path);
    if (length >= FN_REFLEN) return false;
    memcpy(buff2, path, length);
    buff2[length] = '\0';
    if (length == 0 || my_realpath(buff1, buff2, 0)) return false;
  }
  convert_dirname(buff2, buff1, NullS);
  if (!lower_case_file_system) {
    if (strncmp(opt_secure_file_priv, buff2, opt_secure_file_priv_len))
      return false;
  } else {
    if (files_charset_info->coll->strnncoll(
            files_charset_info, (uchar *)buff2, strlen(buff2),
            (uchar *)opt_secure_file_priv, opt_secure_file_priv_len, true))
      return false;
  }
  return true;
}

/**
  check_secure_file_priv_path : Checks path specified through
  --secure-file-priv and raises warning in following cases:
  1. If path is empty string or NULL and mysqld is not running
     with --initialize (bootstrap mode).
  2. If path can access data directory
  3. If path points to a directory which is accessible by
     all OS users (non-Windows build only)

  It throws error in following cases:

  1. If path normalization fails
  2. If it can not get stats of the directory

  Assumptions :
  1. Data directory path has been normalized
  2. opt_secure_file_priv has been normalized unless it is set
     to "NULL".

  @returns Status of validation
    @retval true : Validation is successful with/without warnings
    @retval false : Validation failed. Error is raised.
*/

static bool check_secure_file_priv_path() {
  char datadir_buffer[FN_REFLEN + 1] = {0};
  char plugindir_buffer[FN_REFLEN + 1] = {0};
  char whichdir[20] = {0};
  size_t opt_plugindir_len = 0;
  size_t opt_datadir_len = 0;
  size_t opt_secure_file_priv_len = 0;
  bool warn = false;
  bool case_insensitive_fs;
#ifndef _WIN32
  MY_STAT dir_stat;
#endif

  if (!opt_secure_file_priv[0]) {
    if (opt_initialize) {
      /*
        Do not impose --secure-file-priv restriction
        in bootstrap mode
      */
      LogErr(INFORMATION_LEVEL, ER_SEC_FILE_PRIV_IGNORED);
    } else {
      LogErr(WARNING_LEVEL, ER_SEC_FILE_PRIV_EMPTY);
    }
    return true;
  }

  /*
    Setting --secure-file-priv to NULL would disable
    reading/writing from/to file
  */
  if (!my_strcasecmp(system_charset_info, opt_secure_file_priv, "NULL")) {
    LogErr(INFORMATION_LEVEL, ER_SEC_FILE_PRIV_NULL);
    return true;
  }

  /*
    Check if --secure-file-priv can access data directory
  */
  opt_secure_file_priv_len = strlen(opt_secure_file_priv);

  /*
    Adds dir seperator at the end.
    This is required in subsequent comparison
  */
  convert_dirname(datadir_buffer, mysql_unpacked_real_data_home, NullS);
  opt_datadir_len = strlen(datadir_buffer);

  case_insensitive_fs = (test_if_case_insensitive(datadir_buffer) == 1);

  if (!case_insensitive_fs) {
    if (!strncmp(datadir_buffer, opt_secure_file_priv,
                 opt_datadir_len < opt_secure_file_priv_len
                     ? opt_datadir_len
                     : opt_secure_file_priv_len)) {
      warn = true;
      strcpy(whichdir, "Data directory");
    }
  } else {
    if (!files_charset_info->coll->strnncoll(
            files_charset_info, (uchar *)datadir_buffer, opt_datadir_len,
            (uchar *)opt_secure_file_priv, opt_secure_file_priv_len, true)) {
      warn = true;
      strcpy(whichdir, "Data directory");
    }
  }

  /*
    Don't bother comparing --secure-file-priv with --plugin-dir
    if we already have a match against --datdir or
    --plugin-dir is not pointing to a valid directory.
  */
  if (!warn && !my_realpath(plugindir_buffer, opt_plugin_dir, 0)) {
    convert_dirname(plugindir_buffer, plugindir_buffer, NullS);
    opt_plugindir_len = strlen(plugindir_buffer);

    if (!case_insensitive_fs) {
      if (!strncmp(plugindir_buffer, opt_secure_file_priv,
                   opt_plugindir_len < opt_secure_file_priv_len
                       ? opt_plugindir_len
                       : opt_secure_file_priv_len)) {
        warn = true;
        strcpy(whichdir, "Plugin directory");
      }
    } else {
      if (!files_charset_info->coll->strnncoll(
              files_charset_info, (uchar *)plugindir_buffer, opt_plugindir_len,
              (uchar *)opt_secure_file_priv, opt_secure_file_priv_len, true)) {
        warn = true;
        strcpy(whichdir, "Plugin directory");
      }
    }
  }

  if (warn)
    LogErr(WARNING_LEVEL, ER_SEC_FILE_PRIV_DIRECTORY_INSECURE, whichdir);

#ifndef _WIN32
  /*
     Check for --secure-file-priv directory's permission
  */
  if (!(my_stat(opt_secure_file_priv, &dir_stat, MYF(0)))) {
    LogErr(ERROR_LEVEL, ER_SEC_FILE_PRIV_CANT_STAT);
    return false;
  }

  if (dir_stat.st_mode & S_IRWXO)
    LogErr(WARNING_LEVEL, ER_SEC_FILE_PRIV_DIRECTORY_PERMISSIONS);
#endif
  return true;
}

static int fix_paths(void) {
  char buff[FN_REFLEN];
  bool secure_file_priv_nonempty = false;
  convert_dirname(mysql_home, mysql_home, NullS);
  /* Resolve symlinks to allow 'mysql_home' to be a relative symlink */
  my_realpath(mysql_home, mysql_home, MYF(0));
  /* Ensure that mysql_home ends in FN_LIBCHAR */
  char *pos = strend(mysql_home);
  if (pos == mysql_home || pos[-1] != FN_LIBCHAR) {
    pos[0] = FN_LIBCHAR;
    pos[1] = 0;
  }
  convert_dirname(lc_messages_dir, lc_messages_dir, NullS);
  convert_dirname(mysql_real_data_home, mysql_real_data_home, NullS);
  (void)my_load_path(mysql_home, mysql_home, "");  // Resolve current dir
  (void)my_load_path(mysql_real_data_home, mysql_real_data_home, mysql_home);
  (void)my_load_path(pidfile_name, pidfile_name_ptr, mysql_real_data_home);

  convert_dirname(
      opt_plugin_dir,
      opt_plugin_dir_ptr ? opt_plugin_dir_ptr : get_relative_path(PLUGINDIR),
      NullS);
  (void)my_load_path(opt_plugin_dir, opt_plugin_dir, mysql_home);
  opt_plugin_dir_ptr = opt_plugin_dir;

  my_realpath(mysql_unpacked_real_data_home, mysql_real_data_home, MYF(0));
  mysql_unpacked_real_data_home_len = strlen(mysql_unpacked_real_data_home);
  if (mysql_unpacked_real_data_home[mysql_unpacked_real_data_home_len - 1] ==
      FN_LIBCHAR)
    --mysql_unpacked_real_data_home_len;

  char *sharedir = get_relative_path(SHAREDIR);
  if (test_if_hard_path(sharedir))
    strmake(buff, sharedir, sizeof(buff) - 1); /* purecov: tested */
  else
    strxnmov(buff, sizeof(buff) - 1, mysql_home, sharedir, NullS);
  convert_dirname(buff, buff, NullS);
  (void)my_load_path(lc_messages_dir, lc_messages_dir, buff);

  /* If --character-sets-dir isn't given, use shared library dir */
  if (charsets_dir)
    strmake(mysql_charsets_dir, charsets_dir, sizeof(mysql_charsets_dir) - 1);
  else
    strxnmov(mysql_charsets_dir, sizeof(mysql_charsets_dir) - 1, buff,
             CHARSET_DIR, NullS);
  (void)my_load_path(mysql_charsets_dir, mysql_charsets_dir, buff);
  convert_dirname(mysql_charsets_dir, mysql_charsets_dir, NullS);
  charsets_dir = mysql_charsets_dir;

  if (init_tmpdir(&mysql_tmpdir_list, opt_mysql_tmpdir)) return 1;
  if (!opt_mysql_tmpdir) opt_mysql_tmpdir = mysql_tmpdir;
  if (!slave_load_tmpdir) slave_load_tmpdir = mysql_tmpdir;
  /*
    Convert the secure-file-priv option to system format, allowing
    a quick strcmp to check if read or write is in an allowed dir
  */
  if (opt_initialize) opt_secure_file_priv = EMPTY_STR.str;
  secure_file_priv_nonempty = opt_secure_file_priv[0] ? true : false;

  if (secure_file_priv_nonempty && strlen(opt_secure_file_priv) > FN_REFLEN) {
    LogErr(WARNING_LEVEL, ER_SEC_FILE_PRIV_ARGUMENT_TOO_LONG, FN_REFLEN - 1);
    return 1;
  }

  memset(buff, 0, sizeof(buff));
  if (secure_file_priv_nonempty &&
      my_strcasecmp(system_charset_info, opt_secure_file_priv, "NULL")) {
    int retval = my_realpath(buff, opt_secure_file_priv, MYF(MY_WME));
    if (!retval) {
      convert_dirname(secure_file_real_path, buff, NullS);
#ifdef WIN32
      MY_DIR *dir = my_dir(secure_file_real_path, MYF(MY_DONT_SORT + MY_WME));
      if (!dir) {
        retval = 1;
      } else {
        my_dirend(dir);
      }
#endif
    }

    if (retval) {
      LogErr(ERROR_LEVEL, ER_SEC_FILE_PRIV_CANT_ACCESS_DIR,
             opt_secure_file_priv);
      return 1;
    }
    opt_secure_file_priv = secure_file_real_path;
  }

  if (!check_secure_file_priv_path()) return 1;

  return 0;
}

/**
  Check if file system used for databases is case insensitive.

  @param dir_name     Directory to test

  @retval
    -1  Don't know (Test failed)
  @retval
    0   File system is case sensitive
  @retval
    1   File system is case insensitive
*/

static int test_if_case_insensitive(const char *dir_name) {
  int result = 0;
  File file;
  char buff[FN_REFLEN], buff2[FN_REFLEN];
  MY_STAT stat_info;
  DBUG_ENTER("test_if_case_insensitive");

  fn_format(buff, glob_hostname, dir_name, ".lower-test",
            MY_UNPACK_FILENAME | MY_REPLACE_EXT | MY_REPLACE_DIR);
  fn_format(buff2, glob_hostname, dir_name, ".LOWER-TEST",
            MY_UNPACK_FILENAME | MY_REPLACE_EXT | MY_REPLACE_DIR);
  mysql_file_delete(key_file_casetest, buff2, MYF(0));
  if ((file = mysql_file_create(key_file_casetest, buff, 0666, O_RDWR,
                                MYF(0))) < 0) {
    LogErr(WARNING_LEVEL, ER_CANT_CREATE_TEST_FILE, buff);
    DBUG_RETURN(-1);
  }
  mysql_file_close(file, MYF(0));
  if (mysql_file_stat(key_file_casetest, buff2, &stat_info, MYF(0)))
    result = 1;  // Can access file
  mysql_file_delete(key_file_casetest, buff, MYF(MY_WME));
  DBUG_PRINT("exit", ("result: %d", result));
  DBUG_RETURN(result);
}

/**
  Create file to store pid number.
*/
static bool create_pid_file() {
  File file;
  bool check_parent_path = 1, is_path_accessible = 1;
  char pid_filepath[FN_REFLEN], *pos = NULL;
  /* Copy pid file name to get pid file path */
  strcpy(pid_filepath, pidfile_name);

  /* Iterate through the entire path to check if even one of the sub-dirs
     is world-writable */
  while (check_parent_path && (pos = strrchr(pid_filepath, FN_LIBCHAR)) &&
         (pos != pid_filepath)) /* shouldn't check root */
  {
    *pos = '\0'; /* Trim the inner-most dir */
    switch (is_file_or_dir_world_writable(pid_filepath)) {
      case -2:
        is_path_accessible = 0;
        break;
      case -1:
        LogErr(ERROR_LEVEL, ER_CANT_CHECK_PID_PATH, strerror(errno));
        exit(MYSQLD_ABORT_EXIT);
      case 1:
        LogErr(WARNING_LEVEL, ER_PID_FILE_PRIV_DIRECTORY_INSECURE,
               pid_filepath);
        check_parent_path = 0;
        break;
      case 0:
        continue; /* Keep checking the parent dir */
    }
  }
  if (!is_path_accessible) {
    sql_print_warning(
        "Few location(s) are inaccessible while checking PID filepath.");
  }
  if ((file = mysql_file_create(key_file_pid, pidfile_name, 0664,
                                O_WRONLY | O_TRUNC, MYF(MY_WME))) >= 0) {
    char buff[MAX_BIGINT_WIDTH + 1], *end;
    end = int10_to_str((long)getpid(), buff, 10);
    *end++ = '\n';
    if (!mysql_file_write(file, (uchar *)buff, (uint)(end - buff),
                          MYF(MY_WME | MY_NABP))) {
      mysql_file_close(file, MYF(0));
      pid_file_created = true;
      return false;
    }
    mysql_file_close(file, MYF(0));
  }
  LogErr(ERROR_LEVEL, ER_CANT_CREATE_PID_FILE, strerror(errno));
  return true;
}

/**
  Remove the process' pid file.

  @param  flags  file operation flags
*/

static void delete_pid_file(myf flags) {
  File file;
  if (opt_initialize || !pid_file_created ||
      !(file = mysql_file_open(key_file_pid, pidfile_name, O_RDONLY, flags)))
    return;

  if (file == -1) {
    LogErr(INFORMATION_LEVEL, ER_CANT_REMOVE_PID_FILE, strerror(errno));
    return;
  }

  uchar buff[MAX_BIGINT_WIDTH + 1];
  /* Make sure that the pid file was created by the same process. */
  size_t error = mysql_file_read(file, buff, sizeof(buff), flags);
  mysql_file_close(file, flags);
  buff[sizeof(buff) - 1] = '\0';
  if (error != MY_FILE_ERROR && atol((char *)buff) == (long)getpid()) {
    mysql_file_delete(key_file_pid, pidfile_name, flags);
    pid_file_created = false;
  }
  return;
}

/**
  Delete mysql.ibd after aborting upgrade.
*/
static void delete_dictionary_tablespace() {
  char path[FN_REFLEN + 1];
  bool not_used;

  build_table_filename(path, sizeof(path) - 1, "", "mysql", ".ibd", 0,
                       &not_used);
  (void)mysql_file_delete(key_file_misc, path, MYF(MY_WME));

  // Drop file which tracks progress of upgrade.
  dd::upgrade_57::Upgrade_status().remove();
}

/**
  Returns the current state of the server : booting, operational or shutting
  down.

  @return
    SERVER_BOOTING        Server is not operational. It is starting.
    SERVER_OPERATING      Server is fully initialized and operating.
    SERVER_SHUTTING_DOWN  Server is shutting down.
*/
enum_server_operational_state get_server_state() {
  return server_operational_state;
}

/**
  Reset status for all threads.
*/
class Reset_thd_status : public Do_THD_Impl {
 public:
  Reset_thd_status() {}
  virtual void operator()(THD *thd) {
    /*
      Add thread's status variabes to global status
      and reset thread's status variables.
    */
    add_to_status(&global_status_var, &thd->status_var, true);
  }
};

/**
  Reset global and session status variables.
*/
void refresh_status() {
  mysql_mutex_lock(&LOCK_status);

  /* For all threads, add status to global status and then reset. */
  Reset_thd_status reset_thd_status;
  Global_THD_manager::get_instance()->do_for_all_thd_copy(&reset_thd_status);
#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  /* Reset aggregated status counters. */
  reset_pfs_status_stats();
#endif

  /* Reset some global variables. */
  reset_status_vars();

  /* Reset the counters of all key caches (default and named). */
  process_key_caches(reset_key_cache_counters);
  flush_status_time = time((time_t *)0);
  mysql_mutex_unlock(&LOCK_status);

  /*
    Set max_used_connections to the number of currently open
    connections.  Do this out of LOCK_status to avoid deadlocks.
    Status reset becomes not atomic, but status data is not exact anyway.
  */
  Connection_handler_manager::reset_max_used_connections();
}

  /*****************************************************************************
    Instantiate variables for missing storage engines
    This section should go away soon
  *****************************************************************************/

#ifdef HAVE_PSI_INTERFACE
PSI_mutex_key key_LOCK_tc;
PSI_mutex_key key_hash_filo_lock;
PSI_mutex_key key_LOCK_error_log;
PSI_mutex_key key_LOCK_thd_data;
PSI_mutex_key key_LOCK_thd_sysvar;
PSI_mutex_key key_LOCK_thd_protocol;
PSI_mutex_key key_LOG_LOCK_log;
PSI_mutex_key key_master_info_data_lock;
PSI_mutex_key key_master_info_run_lock;
PSI_mutex_key key_master_info_sleep_lock;
PSI_mutex_key key_master_info_thd_lock;
PSI_mutex_key key_mutex_slave_reporting_capability_err_lock;
PSI_mutex_key key_relay_log_info_data_lock;
PSI_mutex_key key_relay_log_info_sleep_lock;
PSI_mutex_key key_relay_log_info_thd_lock;
PSI_mutex_key key_relay_log_info_log_space_lock;
PSI_mutex_key key_relay_log_info_run_lock;
PSI_mutex_key key_mutex_slave_parallel_pend_jobs;
PSI_mutex_key key_mutex_slave_parallel_worker_count;
PSI_mutex_key key_mutex_slave_parallel_worker;
PSI_mutex_key key_structure_guard_mutex;
PSI_mutex_key key_TABLE_SHARE_LOCK_ha_data;
PSI_mutex_key key_LOCK_query_plan;
PSI_mutex_key key_LOCK_thd_query;
PSI_mutex_key key_LOCK_cost_const;
PSI_mutex_key key_LOCK_current_cond;
PSI_mutex_key key_RELAYLOG_LOCK_commit;
PSI_mutex_key key_RELAYLOG_LOCK_commit_queue;
PSI_mutex_key key_RELAYLOG_LOCK_done;
PSI_mutex_key key_RELAYLOG_LOCK_flush_queue;
PSI_mutex_key key_RELAYLOG_LOCK_index;
PSI_mutex_key key_RELAYLOG_LOCK_log;
PSI_mutex_key key_RELAYLOG_LOCK_log_end_pos;
PSI_mutex_key key_RELAYLOG_LOCK_sync;
PSI_mutex_key key_RELAYLOG_LOCK_sync_queue;
PSI_mutex_key key_RELAYLOG_LOCK_xids;
PSI_mutex_key key_gtid_ensure_index_mutex;
PSI_mutex_key key_object_cache_mutex;  // TODO need to initialize
PSI_cond_key key_object_loading_cond;  // TODO need to initialize
PSI_mutex_key key_mts_temp_table_LOCK;
PSI_mutex_key key_mts_gaq_LOCK;
PSI_mutex_key key_thd_timer_mutex;
PSI_mutex_key key_commit_order_manager_mutex;
PSI_mutex_key key_mutex_slave_worker_hash;

/* clang-format off */
static PSI_mutex_info all_server_mutexes[]=
{
  { &key_LOCK_tc, "TC_LOG_MMAP::LOCK_tc", 0, 0, PSI_DOCUMENT_ME},
  { &key_BINLOG_LOCK_commit, "MYSQL_BIN_LOG::LOCK_commit", 0, 0, PSI_DOCUMENT_ME},
  { &key_BINLOG_LOCK_commit_queue, "MYSQL_BIN_LOG::LOCK_commit_queue", 0, 0, PSI_DOCUMENT_ME},
  { &key_BINLOG_LOCK_done, "MYSQL_BIN_LOG::LOCK_done", 0, 0, PSI_DOCUMENT_ME},
  { &key_BINLOG_LOCK_flush_queue, "MYSQL_BIN_LOG::LOCK_flush_queue", 0, 0, PSI_DOCUMENT_ME},
  { &key_BINLOG_LOCK_index, "MYSQL_BIN_LOG::LOCK_index", 0, 0, PSI_DOCUMENT_ME},
  { &key_BINLOG_LOCK_log, "MYSQL_BIN_LOG::LOCK_log", 0, 0, PSI_DOCUMENT_ME},
  { &key_BINLOG_LOCK_binlog_end_pos, "MYSQL_BIN_LOG::LOCK_binlog_end_pos", 0, 0, PSI_DOCUMENT_ME},
  { &key_BINLOG_LOCK_sync, "MYSQL_BIN_LOG::LOCK_sync", 0, 0, PSI_DOCUMENT_ME},
  { &key_BINLOG_LOCK_sync_queue, "MYSQL_BIN_LOG::LOCK_sync_queue", 0, 0, PSI_DOCUMENT_ME},
  { &key_BINLOG_LOCK_xids, "MYSQL_BIN_LOG::LOCK_xids", 0, 0, PSI_DOCUMENT_ME},
  { &key_RELAYLOG_LOCK_commit, "MYSQL_RELAY_LOG::LOCK_commit", 0, 0, PSI_DOCUMENT_ME},
  { &key_RELAYLOG_LOCK_commit_queue, "MYSQL_RELAY_LOG::LOCK_commit_queue", 0, 0, PSI_DOCUMENT_ME},
  { &key_RELAYLOG_LOCK_done, "MYSQL_RELAY_LOG::LOCK_done", 0, 0, PSI_DOCUMENT_ME},
  { &key_RELAYLOG_LOCK_flush_queue, "MYSQL_RELAY_LOG::LOCK_flush_queue", 0, 0, PSI_DOCUMENT_ME},
  { &key_RELAYLOG_LOCK_index, "MYSQL_RELAY_LOG::LOCK_index", 0, 0, PSI_DOCUMENT_ME},
  { &key_RELAYLOG_LOCK_log, "MYSQL_RELAY_LOG::LOCK_log", 0, 0, PSI_DOCUMENT_ME},
  { &key_RELAYLOG_LOCK_log_end_pos, "MYSQL_RELAY_LOG::LOCK_log_end_pos", 0, 0, PSI_DOCUMENT_ME},
  { &key_RELAYLOG_LOCK_sync, "MYSQL_RELAY_LOG::LOCK_sync", 0, 0, PSI_DOCUMENT_ME},
  { &key_RELAYLOG_LOCK_sync_queue, "MYSQL_RELAY_LOG::LOCK_sync_queue", 0, 0, PSI_DOCUMENT_ME},
  { &key_RELAYLOG_LOCK_xids, "MYSQL_RELAY_LOG::LOCK_xids", 0, 0, PSI_DOCUMENT_ME},
  { &key_hash_filo_lock, "hash_filo::lock", 0, 0, PSI_DOCUMENT_ME},
  { &Gtid_set::key_gtid_executed_free_intervals_mutex, "Gtid_set::gtid_executed::free_intervals_mutex", 0, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_crypt, "LOCK_crypt", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_error_log, "LOCK_error_log", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_global_system_variables, "LOCK_global_system_variables", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
#if defined(_WIN32)
  { &key_LOCK_handler_count, "LOCK_handler_count", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
#endif
  { &key_LOCK_manager, "LOCK_manager", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_prepared_stmt_count, "LOCK_prepared_stmt_count", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_sql_slave_skip_counter, "LOCK_sql_slave_skip_counter", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_slave_net_timeout, "LOCK_slave_net_timeout", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_server_started, "LOCK_server_started", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
#if !defined(_WIN32)
  { &key_LOCK_socket_listener_active, "LOCK_socket_listener_active", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_start_signal_handler, "LOCK_start_signal_handler", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
#endif
  { &key_LOCK_status, "LOCK_status", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_thd_data, "THD::LOCK_thd_data", 0, PSI_VOLATILITY_SESSION, PSI_DOCUMENT_ME},
  { &key_LOCK_thd_query, "THD::LOCK_thd_query", 0, PSI_VOLATILITY_SESSION, PSI_DOCUMENT_ME},
  { &key_LOCK_thd_sysvar, "THD::LOCK_thd_sysvar", 0, PSI_VOLATILITY_SESSION, PSI_DOCUMENT_ME},
  { &key_LOCK_thd_protocol, "THD::LOCK_thd_protocol", 0, PSI_VOLATILITY_SESSION, PSI_DOCUMENT_ME},
  { &key_LOCK_user_conn, "LOCK_user_conn", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_uuid_generator, "LOCK_uuid_generator", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_sql_rand, "LOCK_sql_rand", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOG_LOCK_log, "LOG::LOCK_log", 0, 0, PSI_DOCUMENT_ME},
  { &key_master_info_data_lock, "Master_info::data_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_master_info_run_lock, "Master_info::run_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_master_info_sleep_lock, "Master_info::sleep_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_master_info_thd_lock, "Master_info::info_thd_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_mutex_slave_reporting_capability_err_lock, "Slave_reporting_capability::err_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_relay_log_info_data_lock, "Relay_log_info::data_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_relay_log_info_sleep_lock, "Relay_log_info::sleep_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_relay_log_info_thd_lock, "Relay_log_info::info_thd_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_relay_log_info_log_space_lock, "Relay_log_info::log_space_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_relay_log_info_run_lock, "Relay_log_info::run_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_mutex_slave_parallel_pend_jobs, "Relay_log_info::pending_jobs_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_mutex_slave_parallel_worker_count, "Relay_log_info::exit_count_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_mutex_slave_parallel_worker, "Worker_info::jobs_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_TABLE_SHARE_LOCK_ha_data, "TABLE_SHARE::LOCK_ha_data", 0, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_error_messages, "LOCK_error_messages", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_log_throttle_qni, "LOCK_log_throttle_qni", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_gtid_ensure_index_mutex, "Gtid_state", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_query_plan, "THD::LOCK_query_plan", 0, PSI_VOLATILITY_SESSION, PSI_DOCUMENT_ME},
  { &key_LOCK_cost_const, "Cost_constant_cache::LOCK_cost_const", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_current_cond, "THD::LOCK_current_cond", 0, PSI_VOLATILITY_SESSION, PSI_DOCUMENT_ME},
  { &key_mts_temp_table_LOCK, "key_mts_temp_table_LOCK", 0, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_reset_gtid_table, "LOCK_reset_gtid_table", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_compress_gtid_table, "LOCK_compress_gtid_table", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_collect_instance_log, "LOCK_collect_instance_log", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_mts_gaq_LOCK, "key_mts_gaq_LOCK", 0, 0, PSI_DOCUMENT_ME},
  { &key_thd_timer_mutex, "thd_timer_mutex", 0, 0, PSI_DOCUMENT_ME},
  { &key_commit_order_manager_mutex, "Commit_order_manager::m_mutex", 0, 0, PSI_DOCUMENT_ME},
  { &key_mutex_slave_worker_hash, "Relay_log_info::slave_worker_hash_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_offline_mode, "LOCK_offline_mode", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_default_password_lifetime, "LOCK_default_password_lifetime", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_mandatory_roles, "LOCK_mandatory_roles", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_password_history, "LOCK_password_history", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_password_reuse_interval, "LOCK_password_reuse_interval", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_LOCK_keyring_operations, "LOCK_keyring_operations", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}
};
/* clang-format on */

PSI_rwlock_key key_rwlock_LOCK_logger;
PSI_rwlock_key key_rwlock_channel_map_lock;
PSI_rwlock_key key_rwlock_channel_lock;
PSI_rwlock_key key_rwlock_receiver_sid_lock;
PSI_rwlock_key key_rwlock_rpl_filter_lock;
PSI_rwlock_key key_rwlock_channel_to_filter_lock;

PSI_rwlock_key key_rwlock_Trans_delegate_lock;
PSI_rwlock_key key_rwlock_Server_state_delegate_lock;
PSI_rwlock_key key_rwlock_Binlog_storage_delegate_lock;
PSI_rwlock_key key_rwlock_Binlog_transmit_delegate_lock;
PSI_rwlock_key key_rwlock_Binlog_relay_IO_delegate_lock;
PSI_rwlock_key key_rwlock_resource_group_mgr_map_lock;

/* clang-format off */
static PSI_rwlock_info all_server_rwlocks[]=
{
  { &key_rwlock_Binlog_transmit_delegate_lock, "Binlog_transmit_delegate::lock", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_Binlog_relay_IO_delegate_lock, "Binlog_relay_IO_delegate::lock", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_LOCK_logger, "LOGGER::LOCK_logger", 0, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_LOCK_sys_init_connect, "LOCK_sys_init_connect", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_LOCK_sys_init_slave, "LOCK_sys_init_slave", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_LOCK_system_variables_hash, "LOCK_system_variables_hash", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_global_sid_lock, "gtid_commit_rollback", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_gtid_mode_lock, "gtid_mode_lock", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_channel_map_lock, "channel_map_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_channel_lock, "channel_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_Trans_delegate_lock, "Trans_delegate::lock", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_Server_state_delegate_lock, "Server_state_delegate::lock", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_Binlog_storage_delegate_lock, "Binlog_storage_delegate::lock", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_receiver_sid_lock, "gtid_retrieved", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_rpl_filter_lock, "rpl_filter_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_channel_to_filter_lock, "channel_to_filter_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_rwlock_resource_group_mgr_map_lock, "Resource_group_mgr::m_map_rwlock", 0, 0, PSI_DOCUMENT_ME}
};
/* clang-format on */

PSI_cond_key key_PAGE_cond;
PSI_cond_key key_COND_active;
PSI_cond_key key_COND_pool;
PSI_cond_key key_COND_cache_status_changed;
PSI_cond_key key_item_func_sleep_cond;
PSI_cond_key key_master_info_data_cond;
PSI_cond_key key_master_info_start_cond;
PSI_cond_key key_master_info_stop_cond;
PSI_cond_key key_master_info_sleep_cond;
PSI_cond_key key_relay_log_info_data_cond;
PSI_cond_key key_relay_log_info_log_space_cond;
PSI_cond_key key_relay_log_info_start_cond;
PSI_cond_key key_relay_log_info_stop_cond;
PSI_cond_key key_relay_log_info_sleep_cond;
PSI_cond_key key_cond_slave_parallel_pend_jobs;
PSI_cond_key key_cond_slave_parallel_worker;
PSI_cond_key key_cond_mts_gaq;
PSI_cond_key key_RELAYLOG_update_cond;
PSI_cond_key key_RELAYLOG_COND_done;
PSI_cond_key key_RELAYLOG_prep_xids_cond;
PSI_cond_key key_gtid_ensure_index_cond;
PSI_cond_key key_COND_thr_lock;
PSI_cond_key key_commit_order_manager_cond;
PSI_cond_key key_cond_slave_worker_hash;

/* clang-format off */
static PSI_cond_info all_server_conds[]=
{
  { &key_PAGE_cond, "PAGE::cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_COND_active, "TC_LOG_MMAP::COND_active", 0, 0, PSI_DOCUMENT_ME},
  { &key_COND_pool, "TC_LOG_MMAP::COND_pool", 0, 0, PSI_DOCUMENT_ME},
  { &key_BINLOG_COND_done, "MYSQL_BIN_LOG::COND_done", 0, 0, PSI_DOCUMENT_ME},
  { &key_BINLOG_update_cond, "MYSQL_BIN_LOG::update_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_BINLOG_prep_xids_cond, "MYSQL_BIN_LOG::prep_xids_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_RELAYLOG_COND_done, "MYSQL_RELAY_LOG::COND_done", 0, 0, PSI_DOCUMENT_ME},
  { &key_RELAYLOG_update_cond, "MYSQL_RELAY_LOG::update_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_RELAYLOG_prep_xids_cond, "MYSQL_RELAY_LOG::prep_xids_cond", 0, 0, PSI_DOCUMENT_ME},
#if defined(_WIN32)
  { &key_COND_handler_count, "COND_handler_count", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
#endif
  { &key_COND_manager, "COND_manager", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_COND_server_started, "COND_server_started", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
#if !defined(_WIN32)
  { &key_COND_socket_listener_active, "COND_socket_listener_active", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_COND_start_signal_handler, "COND_start_signal_handler", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
#endif
  { &key_COND_thr_lock, "COND_thr_lock", 0, 0, PSI_DOCUMENT_ME},
  { &key_item_func_sleep_cond, "Item_func_sleep::cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_master_info_data_cond, "Master_info::data_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_master_info_start_cond, "Master_info::start_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_master_info_stop_cond, "Master_info::stop_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_master_info_sleep_cond, "Master_info::sleep_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_relay_log_info_data_cond, "Relay_log_info::data_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_relay_log_info_log_space_cond, "Relay_log_info::log_space_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_relay_log_info_start_cond, "Relay_log_info::start_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_relay_log_info_stop_cond, "Relay_log_info::stop_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_relay_log_info_sleep_cond, "Relay_log_info::sleep_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_cond_slave_parallel_pend_jobs, "Relay_log_info::pending_jobs_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_cond_slave_parallel_worker, "Worker_info::jobs_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_cond_mts_gaq, "Relay_log_info::mts_gaq_cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_gtid_ensure_index_cond, "Gtid_state", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_COND_compress_gtid_table, "COND_compress_gtid_table", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_commit_order_manager_cond, "Commit_order_manager::m_workers.cond", 0, 0, PSI_DOCUMENT_ME},
  { &key_cond_slave_worker_hash, "Relay_log_info::slave_worker_hash_lock", 0, 0, PSI_DOCUMENT_ME}
};
/* clang-format on */

PSI_thread_key key_thread_bootstrap;
PSI_thread_key key_thread_handle_manager;
PSI_thread_key key_thread_one_connection;
PSI_thread_key key_thread_compress_gtid_table;
PSI_thread_key key_thread_parser_service;

/* clang-format off */
static PSI_thread_info all_server_threads[]=
{
#if defined (_WIN32)
  { &key_thread_handle_con_namedpipes, "con_named_pipes", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_thread_handle_con_sharedmem, "con_shared_mem", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_thread_handle_con_sockets, "con_sockets", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_thread_handle_shutdown_restart, "shutdown_restart", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
#endif /* _WIN32 */
  { &key_thread_bootstrap, "bootstrap", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_thread_handle_manager, "manager", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_thread_main, "main", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_thread_one_connection, "one_connection", PSI_FLAG_USER, 0, PSI_DOCUMENT_ME},
  { &key_thread_signal_hand, "signal_handler", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_thread_compress_gtid_table, "compress_gtid_table", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_thread_parser_service, "parser_service", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
};
/* clang-format on */

PSI_file_key key_file_binlog;
PSI_file_key key_file_binlog_index;
PSI_file_key key_file_dbopt;
PSI_file_key key_file_ERRMSG;
PSI_file_key key_select_to_file;
PSI_file_key key_file_fileparser;
PSI_file_key key_file_frm;
PSI_file_key key_file_load;
PSI_file_key key_file_loadfile;
PSI_file_key key_file_log_event_data;
PSI_file_key key_file_log_event_info;
PSI_file_key key_file_misc;
PSI_file_key key_file_tclog;
PSI_file_key key_file_trg;
PSI_file_key key_file_trn;
PSI_file_key key_file_init;
PSI_file_key key_file_general_log;
PSI_file_key key_file_slow_log;
PSI_file_key key_file_relaylog;
PSI_file_key key_file_relaylog_cache;
PSI_file_key key_file_relaylog_index;
PSI_file_key key_file_relaylog_index_cache;
PSI_file_key key_file_sdi;

/* clang-format off */
static PSI_file_info all_server_files[]=
{
  { &key_file_binlog, "binlog", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_binlog_cache, "binlog_cache", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_binlog_index, "binlog_index", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_binlog_index_cache, "binlog_index_cache", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_relaylog, "relaylog", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_relaylog_cache, "relaylog_cache", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_relaylog_index, "relaylog_index", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_relaylog_index_cache, "relaylog_index_cache", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_io_cache, "io_cache", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_casetest, "casetest", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_dbopt, "dbopt", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_ERRMSG, "ERRMSG", 0, 0, PSI_DOCUMENT_ME},
  { &key_select_to_file, "select_to_file", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_fileparser, "file_parser", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_frm, "FRM", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_load, "load", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_loadfile, "LOAD_FILE", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_log_event_data, "log_event_data", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_log_event_info, "log_event_info", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_misc, "misc", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_pid, "pid", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_general_log, "query_log", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_slow_log, "slow_log", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_tclog, "tclog", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_trg, "trigger_name", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_trn, "trigger", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_init, "init", 0, 0, PSI_DOCUMENT_ME},
  { &key_file_sdi, "SDI", 0, 0, PSI_DOCUMENT_ME}
};
/* clang-format on */
#endif /* HAVE_PSI_INTERFACE */

/* clang-format off */
PSI_stage_info stage_after_create= { 0, "After create", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_allocating_local_table= { 0, "allocating local table", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_alter_inplace_prepare= { 0, "preparing for alter table", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_alter_inplace= { 0, "altering table", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_alter_inplace_commit= { 0, "committing alter table to storage engine", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_changing_master= { 0, "Changing master", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_checking_master_version= { 0, "Checking master version", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_checking_permissions= { 0, "checking permissions", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_checking_privileges_on_cached_query= { 0, "checking privileges on cached query", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_cleaning_up= { 0, "cleaning up", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_closing_tables= { 0, "closing tables", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_compressing_gtid_table= { 0, "Compressing gtid_executed table", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_connecting_to_master= { 0, "Connecting to master", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_converting_heap_to_ondisk= { 0, "converting HEAP to ondisk", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_copying_to_group_table= { 0, "Copying to group table", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_copying_to_tmp_table= { 0, "Copying to tmp table", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_copy_to_tmp_table= { 0, "copy to tmp table", PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info stage_creating_sort_index= { 0, "Creating sort index", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_creating_table= { 0, "creating table", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_creating_tmp_table= { 0, "Creating tmp table", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_deleting_from_main_table= { 0, "deleting from main table", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_deleting_from_reference_tables= { 0, "deleting from reference tables", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_discard_or_import_tablespace= { 0, "discard_or_import_tablespace", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_end= { 0, "end", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_executing= { 0, "executing", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_execution_of_init_command= { 0, "Execution of init_command", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_explaining= { 0, "explaining", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_finished_reading_one_binlog_switching_to_next_binlog= { 0, "Finished reading one binlog; switching to next binlog", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_flushing_relay_log_and_master_info_repository= { 0, "Flushing relay log and master info repository.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_flushing_relay_log_info_file= { 0, "Flushing relay-log info file.", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_freeing_items= { 0, "freeing items", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_fulltext_initialization= { 0, "FULLTEXT initialization", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_got_handler_lock= { 0, "got handler lock", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_got_old_table= { 0, "got old table", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_init= { 0, "init", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_insert= { 0, "insert", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_killing_slave= { 0, "Killing slave", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_logging_slow_query= { 0, "logging slow query", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_making_temp_file_append_before_load_data= { 0, "Making temporary file (append) before replaying LOAD DATA INFILE", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_making_temp_file_create_before_load_data= { 0, "Making temporary file (create) before replaying LOAD DATA INFILE", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_manage_keys= { 0, "manage keys", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_master_has_sent_all_binlog_to_slave= { 0, "Master has sent all binlog to slave; waiting for more updates", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_opening_tables= { 0, "Opening tables", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_optimizing= { 0, "optimizing", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_preparing= { 0, "preparing", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_purging_old_relay_logs= { 0, "Purging old relay logs", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_query_end= { 0, "query end", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_queueing_master_event_to_the_relay_log= { 0, "Queueing master event to the relay log", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_reading_event_from_the_relay_log= { 0, "Reading event from the relay log", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_registering_slave_on_master= { 0, "Registering slave on master", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_removing_duplicates= { 0, "Removing duplicates", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_removing_tmp_table= { 0, "removing tmp table", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_rename= { 0, "rename", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_rename_result_table= { 0, "rename result table", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_requesting_binlog_dump= { 0, "Requesting binlog dump", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_reschedule= { 0, "reschedule", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_searching_rows_for_update= { 0, "Searching rows for update", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_sending_binlog_event_to_slave= { 0, "Sending binlog event to slave", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_sending_cached_result_to_client= { 0, "sending cached result to client", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_sending_data= { 0, "Sending data", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_setup= { 0, "setup", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_slave_has_read_all_relay_log= { 0, "Slave has read all relay log; waiting for more updates", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_slave_waiting_event_from_coordinator= { 0, "Waiting for an event from Coordinator", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_slave_waiting_for_workers_to_process_queue= { 0, "Waiting for slave workers to process their queues", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_slave_waiting_worker_queue= { 0, "Waiting for Slave Worker queue", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_slave_waiting_worker_to_free_events= { 0, "Waiting for Slave Workers to free pending events", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_slave_waiting_worker_to_release_partition= { 0, "Waiting for Slave Worker to release partition", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_slave_waiting_workers_to_exit= { 0, "Waiting for workers to exit", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_rpl_apply_row_evt_write= { 0, "Applying batch of row changes (write)", PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info stage_rpl_apply_row_evt_update= { 0, "Applying batch of row changes (update)", PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info stage_rpl_apply_row_evt_delete= { 0, "Applying batch of row changes (delete)", PSI_FLAG_STAGE_PROGRESS, PSI_DOCUMENT_ME};
PSI_stage_info stage_sorting_for_group= { 0, "Sorting for group", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_sorting_for_order= { 0, "Sorting for order", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_sorting_result= { 0, "Sorting result", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_statistics= { 0, "statistics", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_sql_thd_waiting_until_delay= { 0, "Waiting until MASTER_DELAY seconds after master executed event", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_storing_row_into_queue= { 0, "storing row into queue", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_system_lock= { 0, "System lock", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_update= { 0, "update", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_updating= { 0, "updating", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_updating_main_table= { 0, "updating main table", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_updating_reference_tables= { 0, "updating reference tables", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_upgrading_lock= { 0, "upgrading lock", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_user_sleep= { 0, "User sleep", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_verifying_table= { 0, "verifying table", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_for_gtid_to_be_committed= { 0, "Waiting for GTID to be committed", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_for_handler_insert= { 0, "waiting for handler insert", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_for_handler_lock= { 0, "waiting for handler lock", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_for_handler_open= { 0, "waiting for handler open", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_for_insert= { 0, "Waiting for INSERT", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_for_master_to_send_event= { 0, "Waiting for master to send event", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_for_master_update= { 0, "Waiting for master update", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_for_relay_log_space= { 0, "Waiting for the slave SQL thread to free enough relay log space", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_for_slave_mutex_on_exit= { 0, "Waiting for slave mutex on exit", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_for_slave_thread_to_start= { 0, "Waiting for slave thread to start", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_for_table_flush= { 0, "Waiting for table flush", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_for_the_next_event_in_relay_log= { 0, "Waiting for the next event in relay log", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_for_the_slave_thread_to_advance_position= { 0, "Waiting for the slave SQL thread to advance position", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_to_finalize_termination= { 0, "Waiting to finalize termination", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_worker_waiting_for_its_turn_to_commit= { 0, "Waiting for preceding transaction to commit", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_worker_waiting_for_commit_parent= { 0, "Waiting for dependent transaction to commit", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_suspending= { 0, "Suspending", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_starting= { 0, "starting", 0, PSI_DOCUMENT_ME};
PSI_stage_info stage_waiting_for_no_channel_reference= { 0, "Waiting for no channel reference.", 0, PSI_DOCUMENT_ME};
/* clang-format on */

extern PSI_stage_info stage_waiting_for_disk_space;

#ifdef HAVE_PSI_INTERFACE

PSI_stage_info *all_server_stages[] = {
    &stage_after_create,
    &stage_allocating_local_table,
    &stage_alter_inplace_prepare,
    &stage_alter_inplace,
    &stage_alter_inplace_commit,
    &stage_changing_master,
    &stage_checking_master_version,
    &stage_checking_permissions,
    &stage_checking_privileges_on_cached_query,
    &stage_cleaning_up,
    &stage_closing_tables,
    &stage_compressing_gtid_table,
    &stage_connecting_to_master,
    &stage_converting_heap_to_ondisk,
    &stage_copying_to_group_table,
    &stage_copying_to_tmp_table,
    &stage_copy_to_tmp_table,
    &stage_creating_sort_index,
    &stage_creating_table,
    &stage_creating_tmp_table,
    &stage_deleting_from_main_table,
    &stage_deleting_from_reference_tables,
    &stage_discard_or_import_tablespace,
    &stage_end,
    &stage_executing,
    &stage_execution_of_init_command,
    &stage_explaining,
    &stage_finished_reading_one_binlog_switching_to_next_binlog,
    &stage_flushing_relay_log_and_master_info_repository,
    &stage_flushing_relay_log_info_file,
    &stage_freeing_items,
    &stage_fulltext_initialization,
    &stage_got_handler_lock,
    &stage_got_old_table,
    &stage_init,
    &stage_insert,
    &stage_killing_slave,
    &stage_logging_slow_query,
    &stage_making_temp_file_append_before_load_data,
    &stage_making_temp_file_create_before_load_data,
    &stage_manage_keys,
    &stage_master_has_sent_all_binlog_to_slave,
    &stage_opening_tables,
    &stage_optimizing,
    &stage_preparing,
    &stage_purging_old_relay_logs,
    &stage_query_end,
    &stage_queueing_master_event_to_the_relay_log,
    &stage_reading_event_from_the_relay_log,
    &stage_registering_slave_on_master,
    &stage_removing_duplicates,
    &stage_removing_tmp_table,
    &stage_rename,
    &stage_rename_result_table,
    &stage_requesting_binlog_dump,
    &stage_reschedule,
    &stage_searching_rows_for_update,
    &stage_sending_binlog_event_to_slave,
    &stage_sending_cached_result_to_client,
    &stage_sending_data,
    &stage_setup,
    &stage_slave_has_read_all_relay_log,
    &stage_slave_waiting_event_from_coordinator,
    &stage_slave_waiting_for_workers_to_process_queue,
    &stage_slave_waiting_worker_queue,
    &stage_slave_waiting_worker_to_free_events,
    &stage_slave_waiting_worker_to_release_partition,
    &stage_slave_waiting_workers_to_exit,
    &stage_rpl_apply_row_evt_write,
    &stage_rpl_apply_row_evt_update,
    &stage_rpl_apply_row_evt_delete,
    &stage_sorting_for_group,
    &stage_sorting_for_order,
    &stage_sorting_result,
    &stage_sql_thd_waiting_until_delay,
    &stage_statistics,
    &stage_storing_row_into_queue,
    &stage_system_lock,
    &stage_update,
    &stage_updating,
    &stage_updating_main_table,
    &stage_updating_reference_tables,
    &stage_upgrading_lock,
    &stage_user_sleep,
    &stage_verifying_table,
    &stage_waiting_for_gtid_to_be_committed,
    &stage_waiting_for_handler_insert,
    &stage_waiting_for_handler_lock,
    &stage_waiting_for_handler_open,
    &stage_waiting_for_insert,
    &stage_waiting_for_master_to_send_event,
    &stage_waiting_for_master_update,
    &stage_waiting_for_relay_log_space,
    &stage_waiting_for_slave_mutex_on_exit,
    &stage_waiting_for_slave_thread_to_start,
    &stage_waiting_for_table_flush,
    &stage_waiting_for_the_next_event_in_relay_log,
    &stage_waiting_for_the_slave_thread_to_advance_position,
    &stage_waiting_to_finalize_termination,
    &stage_worker_waiting_for_its_turn_to_commit,
    &stage_worker_waiting_for_commit_parent,
    &stage_suspending,
    &stage_starting,
    &stage_waiting_for_no_channel_reference,
    &stage_waiting_for_disk_space};

PSI_socket_key key_socket_tcpip;
PSI_socket_key key_socket_unix;
PSI_socket_key key_socket_client_connection;

/* clang-format off */
static PSI_socket_info all_server_sockets[]=
{
  { &key_socket_tcpip, "server_tcpip_socket", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_socket_unix, "server_unix_socket", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  { &key_socket_client_connection, "client_connection", PSI_FLAG_USER, 0, PSI_DOCUMENT_ME}
};
/* clang-format on */

/* TODO: find a good header */
void init_client_psi_keys(void);

/**
  Initialise all the performance schema instrumentation points
  used by the server.
*/
static void init_server_psi_keys(void) {
  const char *category = "sql";
  int count;

  count = static_cast<int>(array_elements(all_server_mutexes));
  mysql_mutex_register(category, all_server_mutexes, count);

  count = static_cast<int>(array_elements(all_server_rwlocks));
  mysql_rwlock_register(category, all_server_rwlocks, count);

  count = static_cast<int>(array_elements(all_server_conds));
  mysql_cond_register(category, all_server_conds, count);

  count = static_cast<int>(array_elements(all_server_threads));
  mysql_thread_register(category, all_server_threads, count);

  count = static_cast<int>(array_elements(all_server_files));
  mysql_file_register(category, all_server_files, count);

  count = static_cast<int>(array_elements(all_server_stages));
  mysql_stage_register(category, all_server_stages, count);

  count = static_cast<int>(array_elements(all_server_sockets));
  mysql_socket_register(category, all_server_sockets, count);

  register_server_memory_keys();

#ifdef HAVE_PSI_STATEMENT_INTERFACE
  init_sql_statement_info();
  count = static_cast<int>(array_elements(sql_statement_info));
  mysql_statement_register(category, sql_statement_info, count);

  init_sp_psi_keys();

  init_scheduler_psi_keys();

  category = "com";
  init_com_statement_info();

  /*
    Register [0 .. COM_QUERY - 1] as "statement/com/..."
  */
  count = (int)COM_QUERY;
  mysql_statement_register(category, com_statement_info, count);

  /*
    Register [COM_QUERY + 1 .. COM_END] as "statement/com/..."
  */
  count = (int)COM_END - (int)COM_QUERY;
  mysql_statement_register(category, &com_statement_info[(int)COM_QUERY + 1],
                           count);

  category = "abstract";
  /*
    Register [COM_QUERY] as "statement/abstract/com_query"
  */
  mysql_statement_register(category, &com_statement_info[(int)COM_QUERY], 1);

  /*
    When a new packet is received,
    it is instrumented as "statement/abstract/new_packet".
    Based on the packet type found, it later mutates to the
    proper narrow type, for example
    "statement/abstract/query" or "statement/com/ping".
    In cases of "statement/abstract/query", SQL queries are given to
    the parser, which mutates the statement type to an even more
    narrow classification, for example "statement/sql/select".
  */
  stmt_info_new_packet.m_key = 0;
  stmt_info_new_packet.m_name = "new_packet";
  stmt_info_new_packet.m_flags = PSI_FLAG_MUTABLE;
  stmt_info_new_packet.m_documentation =
      "New packet just received from the network. "
      "At this point, the real command type is unknown, "
      "the type will be refined after reading the packet header.";
  mysql_statement_register(category, &stmt_info_new_packet, 1);

  /*
    Statements processed from the relay log are initially instrumented as
    "statement/abstract/relay_log". The parser will mutate the statement type to
    a more specific classification, for example "statement/sql/insert".
  */
  stmt_info_rpl.m_key = 0;
  stmt_info_rpl.m_name = "relay_log";
  stmt_info_rpl.m_flags = PSI_FLAG_MUTABLE;
  stmt_info_rpl.m_documentation =
      "New event just read from the relay log. "
      "At this point, the real statement type is unknown, "
      "the type will be refined after parsing the event.";
  mysql_statement_register(category, &stmt_info_rpl, 1);
#endif

  /* Common client and server code. */
  init_client_psi_keys();
  /* Vio */
  init_vio_psi_keys();
}
#endif /* HAVE_PSI_INTERFACE */

bool do_create_native_table_for_pfs(THD *thd, const Plugin_table *t) {
  const char *schema_name = t->get_schema_name();
  const char *table_name = t->get_name();
  MDL_request table_request;
  MDL_REQUEST_INIT(&table_request, MDL_key::TABLE, schema_name, table_name,
                   MDL_EXCLUSIVE, MDL_TRANSACTION);

  if (thd->mdl_context.acquire_lock(&table_request,
                                    thd->variables.lock_wait_timeout)) {
    /* Error, failed to get MDL lock. */
    return true;
  }

  tdc_remove_table(thd, TDC_RT_REMOVE_ALL, schema_name, table_name, false);

  if (dd::create_native_table(thd, t)) {
    /* Error, failed to create DD table. */
    return true;
  }

  return false;
}

bool create_native_table_for_pfs(const Plugin_table *t) {
  /* If InnoDB is not initialized yet, return error */
  if (!is_builtin_and_core_se_initialized()) return true;

  THD *thd = current_thd;
  DBUG_ASSERT(thd);
  return do_create_native_table_for_pfs(thd, t);
}

static bool do_drop_native_table_for_pfs(THD *thd, const char *schema_name,
                                         const char *table_name) {
  MDL_request table_request;
  MDL_REQUEST_INIT(&table_request, MDL_key::TABLE, schema_name, table_name,
                   MDL_EXCLUSIVE, MDL_TRANSACTION);

  if (thd->mdl_context.acquire_lock(&table_request,
                                    thd->variables.lock_wait_timeout)) {
    /* Error, failed to get MDL lock. */
    return true;
  }

  tdc_remove_table(thd, TDC_RT_REMOVE_ALL, schema_name, table_name, false);

  if (dd::drop_native_table(thd, schema_name, table_name)) {
    /* Error, failed to destroy DD table. */
    return true;
  }

  return false;
}

bool drop_native_table_for_pfs(const char *schema_name,
                               const char *table_name) {
  /* If server is shutting down, by the time control reaches here, DD would have
   * already been shut down. Therefore return success and tables won't be
   * deleted and would be available at next server start.
   */
  if (get_server_state() == SERVER_SHUTTING_DOWN) {
    return false;
  }

  THD *thd = current_thd;
  DBUG_ASSERT(thd);
  return do_drop_native_table_for_pfs(thd, schema_name, table_name);
}
