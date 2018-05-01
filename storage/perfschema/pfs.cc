/* Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.

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

#define HAVE_PSI_COND_INTERFACE
#define HAVE_PSI_DATA_LOCK_INTERFACE
#define HAVE_PSI_ERROR_INTERFACE
#define HAVE_PSI_FILE_INTERFACE
#define HAVE_PSI_IDLE_INTERFACE
#define HAVE_PSI_MEMORY_INTERFACE
#define HAVE_PSI_METADATA_INTERFACE
#define HAVE_PSI_MUTEX_INTERFACE
#define HAVE_PSI_PS_INTERFACE
#define HAVE_PSI_RWLOCK_INTERFACE
#define HAVE_PSI_SOCKET_INTERFACE
#define HAVE_PSI_SP_INTERFACE
#define HAVE_PSI_STAGE_INTERFACE
#define HAVE_PSI_STATEMENT_DIGEST_INTERFACE
#define HAVE_PSI_STATEMENT_INTERFACE
#define HAVE_PSI_SYSTEM_INTERFACE
#define HAVE_PSI_TABLE_INTERFACE
#define HAVE_PSI_THREAD_INTERFACE
#define HAVE_PSI_TRANSACTION_INTERFACE

#include "storage/perfschema/pfs.h"

#include "my_config.h"

#include <errno.h>
#include <fcntl.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/psi_cond_service.h>
#include <mysql/components/services/psi_error_service.h>
#include <mysql/components/services/psi_file_service.h>
#include <mysql/components/services/psi_idle_service.h>
#include <mysql/components/services/psi_mdl_service.h>
#include <mysql/components/services/psi_memory_service.h>
#include <mysql/components/services/psi_mutex_service.h>
#include <mysql/components/services/psi_rwlock_service.h>
#include <mysql/components/services/psi_socket_service.h>
#include <mysql/components/services/psi_stage_service.h>
#include <mysql/components/services/psi_statement_service.h>
#include <mysql/components/services/psi_system_service.h>
#include <mysql/components/services/psi_table_service.h>
#include <mysql/components/services/psi_thread_service.h>
#include <mysql/components/services/psi_transaction_service.h>
#include <sys/types.h>
#include <time.h>

/**
  @file storage/perfschema/pfs.cc
  The performance schema implementation of all instruments.
*/
#include "lex_string.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_thread.h"
#include "mysql/psi/mysql_thread.h"
#include "pfs_error_provider.h"
/* Make sure exported prototypes match the implementation. */
#include "pfs_file_provider.h"
#include "pfs_idle_provider.h"
#include "pfs_memory_provider.h"
#include "pfs_metadata_provider.h"
#include "pfs_socket_provider.h"
#include "pfs_stage_provider.h"
#include "pfs_statement_provider.h"
#include "pfs_system_provider.h"
#include "pfs_table_provider.h"
#include "pfs_thread_provider.h"
#include "pfs_transaction_provider.h"
#include "sql/mdl.h" /* mdl_key_init */
#include "sql/sp_head.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_data_lock.h"
#include "storage/perfschema/pfs_digest.h"
#include "storage/perfschema/pfs_error.h"
#include "storage/perfschema/pfs_events_stages.h"
#include "storage/perfschema/pfs_events_statements.h"
#include "storage/perfschema/pfs_events_transactions.h"
#include "storage/perfschema/pfs_events_waits.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_plugin_table.h"
#include "storage/perfschema/pfs_prepared_stmt.h"
#include "storage/perfschema/pfs_program.h"
#include "storage/perfschema/pfs_services.h"
#include "storage/perfschema/pfs_setup_actor.h"
#include "storage/perfschema/pfs_setup_object.h"
#include "storage/perfschema/pfs_timer.h"
#include "storage/perfschema/pfs_user.h"
#include "storage/perfschema/service_pfs_notification.h"
#include "thr_lock.h"

using std::min;

/*
  Exporting cmake compilation flags to doxygen,
  so they get documented.
*/
#ifdef IN_DOXYGEN
#define HAVE_PSI_2
#define DISABLE_PSI_COND
#define DISABLE_PSI_DATA_LOCK
#define DISABLE_PSI_ERROR
#define DISABLE_PSI_FILE
#define DISABLE_PSI_IDLE
#define DISABLE_PSI_MEMORY
#define DISABLE_PSI_METADATA
#define DISABLE_PSI_MUTEX
#define DISABLE_PSI_PS
#define DISABLE_PSI_RWLOCK
#define DISABLE_PSI_SOCKET
#define DISABLE_PSI_SP
#define DISABLE_PSI_STAGE
#define DISABLE_PSI_STATEMENT
#define DISABLE_PSI_STATEMENT_DIGEST
#define DISABLE_PSI_SYSTEM
#define DISABLE_PSI_TABLE
#define DISABLE_PSI_THREAD
#define DISABLE_PSI_TRANSACTION
#endif /* IN_DOXYGEN */

/*
  This is a development tool to investigate memory statistics,
  do not use in production.
*/
#undef PFS_PARANOID

#ifdef PFS_PARANOID
static void report_memory_accounting_error(const char *api_name,
                                           PFS_thread *new_thread, size_t size,
                                           PFS_memory_class *klass,
                                           PFS_thread *old_thread) {
  pfs_print_error(
      "%s "
      "thread <%d> of class <%s> "
      "not owner of <%d> bytes in class <%s> "
      "allocated by thread <%d> of class <%s>\n",
      api_name, new_thread->m_thread_internal_id, new_thread->m_class->m_name,
      size, klass->m_name, old_thread->m_thread_internal_id,
      old_thread->m_class->m_name);

  DBUG_ASSERT(strcmp(new_thread->m_class->m_name, "thread/sql/event_worker") !=
              0);
  DBUG_ASSERT(
      strcmp(new_thread->m_class->m_name, "thread/sql/event_scheduler") != 0);
  DBUG_ASSERT(
      strcmp(new_thread->m_class->m_name, "thread/sql/one_connection") != 0);
}
#endif /* PFS_PARANOID */

/* clang-format off */
/**
  @page PAGE_PFS Performance Schema
  MySQL PERFORMANCE_SCHEMA implementation.

  @section PFS_MAIN_INTRO Introduction
  The PERFORMANCE_SCHEMA is a way to introspect the internal execution of
  the server at runtime.
  The performance schema focuses primarily on performance data,
  as opposed to the INFORMATION_SCHEMA whose purpose is to inspect metadata.

  From a user point of view, the performance schema consists of:
  - a dedicated database schema, named @c performance_schema,
  - SQL tables, used to query the server internal state or change
  configuration settings.

  From an implementation point of view, the performance schema is a dedicated
  Storage Engine which exposes data collected by 'Instrumentation Points'
  placed in the server code.

  @section INTERFACES Multiple interfaces

  The performance schema exposes many different interfaces,
  for different components, and for different purposes.

  @subsection INT_INSTRUMENTING Instrumenting interface

  All the data representing the server internal state exposed
  in the performance schema must be first collected:
  this is the role of the instrumenting interface.
  The instrumenting interface is a coding interface provided
  by implementors (of the performance schema) to implementors
  (of the server or server components).

  This interface is available to:
  - C implementations
  - C++ implementations
  - the core SQL layer (/sql)
  - the mysys library (/mysys)
  - MySQL plugins, including storage engines,
  - third party plugins, including third party storage engines.

  For details, see @ref PAGE_PFS_PSI

  @subsection INT_COMPILING Compiling interface

  The performance schema storage engine, the code that expose SQL tables,
  is always compiled.

  The instrumentation points, that collects data to the storage engine,
  can be enabled or disabled at build time,
  when building MySQL from the source code.
  Each kind of instrumentation can be enabled or disabled independently.

  For more details, see:
  @verbatim ccmake @endverbatim

  Press [t] to toggle advanced mode (Currently Off)
  and search for options named like @c DISABLE_PSI_MUTEX.

  To compile with the performance schema:
  @verbatim cmake @endverbatim

  To compile without some performance schema instrumentation:
  @verbatim cmake -DDISABLE_PSI_MUTEX @endverbatim

  The implementation of all the compiling options is located in
  @verbatim ./storage/perfschema/CMakeLists.txt @endverbatim

  @subsection INT_STARTUP Server startup interface

  The server startup interface consists of the @code ./mysqld ... @endcode
  command line used to start the server.

  These start options allow the DBA to:
  - enable or disable the performance schema
  - specify some sizing parameters.

  To see help for the performance schema startup options, see:
  @verbatim ./sql/mysqld --verbose --help  @endverbatim

  The implementation of all the startup options is located in
  file @code ./sql/mysqld.cc @endcode, see for example @c Sys_pfs_enabled.

  @subsection INT_BOOTSTRAP Server bootstrap interface

  The bootstrap interface is a private interface exposed by
  the performance schema, and used by the SQL layer.
  Its role is to advertise all the SQL tables natively
  supported by the performance schema to the SQL server.
  The code consists of creating MySQL tables for the
  performance schema itself, and is used in './mysqld --initialize'
  mode when a server is initialized.

  The implementation of the database creation script is located in
  @verbatim ./scripts/mysql_system_tables.sql @endverbatim

  @subsection INT_CONFIG Runtime configuration interface

  When the performance schema is used at runtime, various configuration
  parameters can be used to specify what kind of data is collected,
  what kind of aggregations are computed, what kind of timers are used,
  what events are timed, etc.

  For all these capabilities, not a single statement or special syntax
  was introduced in the parser.
  Instead of new SQL statements, the interface consists of DML
  (SELECT, INSERT, UPDATE, DELETE) against special "SETUP" tables.

  For example:
  @verbatim mysql> update performance_schema.SETUP_INSTRUMENTS
    set ENABLED='YES', TIMED='YES';
  Query OK, 234 rows affected (0.00 sec)
  Rows matched: 234  Changed: 234  Warnings: 0 @endverbatim

  @subsection INT_STATUS Internal audit interface

  The internal audit interface is provided to the DBA to inspect if the
  performance schema code itself is functioning properly.
  This interface is necessary because a failure caused while
  instrumenting code in the server should not cause failures in the
  MySQL server itself, so that the performance schema implementation
  never raises errors during runtime execution.

  This auditing interface consists of:
  @verbatim SHOW ENGINE PERFORMANCE_SCHEMA STATUS; @endverbatim
  It displays data related to the memory usage of the performance schema,
  as well as statistics about lost events, if any.

  The SHOW STATUS command is implemented in @c pfs_show_status.

  @subsection INT_QUERY Query interface

  The query interface is used to query the internal state of a running server.
  It is provided as SQL tables.

  For example:
  @verbatim mysql> select * from performance_schema.EVENTS_WAITS_CURRENT;
  @endverbatim

  @section DESIGN_PRINCIPLES Design principles

  @subsection PRINCIPLE_BEHAVIOR No behavior changes

  The primary goal of the performance schema is to measure (instrument) the
  execution of the server. A good measure should not cause any change
  in behavior.

  To achieve this, the overall design of the performance schema complies
  (for the most part, there are some exceptions)
  with the following very severe design constraints.

  The parser is unchanged. There are no new keywords, no new statements.
  This guarantees that existing applications will run the same way with or
  without the performance schema.

  All the instrumentation points return "void", there are no error codes.
  Even if the performance schema internally fails, execution of the server
  code will proceed.

  None of the instrumentation points allocate memory.
  In general, the memory used by the performance schema is pre-allocated at
  startup.
  For some instrumentations, memory is pre-allocated incrementally, by chunks,
  at runtime.
  In both cases, memory is considered "static" during the server life time.
  Performance schema memory can be reused, but is never returned.

  For nominal code paths,
  none of the instrumentation points use any pthread_mutex, pthread_rwlock,
  or pthread_cond (or platform equivalents).
  Executing the instrumentation point should not cause thread scheduling to
  change in the server.

  In other words, the implementation of the instrumentation points,
  including all the code called by the instrumentation points, is:
  - malloc free
  - mutex free
  - rwlock free

  Currently, most of the code located in storage/perfschema is malloc free,
  but unfortunately the usage of LF_HASH introduces some memory allocation.
  This should be revised if possible, to use a lock-free,
  malloc-free hash code table.

  @subsection PRINCIPLE_PERFORMANCE No performance hit

  The instrumentation of the server should be as fast as possible.
  In cases when there are choices between:
  - doing some processing when recording the performance data
  in the instrumentation,
  - doing some processing when retrieving the performance data,

  priority is given in the design to make the instrumentation faster,
  pushing some complexity to data retrieval.

  As a result, some parts of the design, related to:
  - the setup code path,
  - the query code path,

  might appear to be sub-optimal.

  The criterion used here is to optimize primarily the critical path (data
  collection), possibly at the expense of non-critical code paths.

  @subsection PRINCIPLE_NOT_INTRUSIVE Unintrusive instrumentation

  For the performance schema in general to be successful, the barrier
  of entry for a developer should be low, so it's easy to instrument code.

  In particular, the instrumentation interface:
  - is available for C and C++ code (so it's a C interface),
  - does not require parameters that the calling code can't easily provide,
  - supports partial instrumentation (for example, instrumenting mutexes does
  not require that every mutex is instrumented)

  @subsection PRINCIPLE_EXTENDABLE Extendable instrumentation

  As the content of the performance schema improves,
  with more tables exposed and more data collected,
  the instrumentation interface will also be augmented
  to support instrumenting new concepts.
  Existing instrumentations should not be affected when additional
  instrumentation is made available, and making a new instrumentation
  available should not require existing instrumented code to support it.

  @subsection PRINCIPLE_VERSIONED Versioned instrumentation

  Given that the instrumentation offered by the performance schema will
  be augmented with time, when more features are implemented,
  the interface itself should be versioned, to keep compatibility
  with previous instrumented code.

  For example, after both plugin-A and plugin-B have been instrumented for
  mutexes, read write locks and conditions, using the instrumentation
  interface, we can anticipate that the instrumentation interface
  is expanded to support file based operations.

  Plugin-A, a file based storage engine, will most likely use the expanded
  interface and instrument its file usage, using the version 2
  interface, while Plugin-B, a network based storage engine, will not change
  its code and not release a new binary.

  When later the instrumentation interface is expanded to support network
  based operations (which will define interface version 3), the Plugin-B code
  can then be changed to make use of it.

  Note, this is just an example to illustrate the design concept here.
  Both mutexes and file instrumentation are already available
  since version 1 of the instrumentation interface.

  @subsection PRINCIPLE_DEPLOYMENT Easy deployment

  Internally, we might want every plugin implementation to upgrade the
  instrumented code to the latest available, but this will cause additional
  work and this is not practical if the code change is monolithic.

  Externally, for third party plugin implementors, asking implementors to
  always stay aligned to the latest instrumentation and make new releases,
  even when the change does not provide new functionality for them,
  is a bad idea.

  For example, requiring a network based engine to re-release because the
  instrumentation interface changed for file based operations, will create
  too many deployment issues.

  So, the performance schema implementation must support concurrently,
  in the same deployment, multiple versions of the instrumentation
  interface, and ensure binary compatibility with each version.

  In addition to this, the performance schema can be included or excluded
  from the server binary, using build time configuration options.

  Regardless, the following types of deployment are valid:
  - a server supporting the performance schema instrumentation X + a storage
  engine
  that is not instrumented for X
  - a server not supporting the performance schema Y + a storage engine
  that is instrumented for Y

  @subpage PAGE_PFS_PSI

  @subpage PAGE_PFS_AGGREGATES

  @subpage PAGE_PFS_DATA_LOCKS

  @subpage PAGE_PFS_NEW_TABLE

  @subpage PAGE_PFS_NOTIFICATION_SERVICE

  @subpage PAGE_PFS_RESOURCE_GROUP_SERVICE

  @subpage PAGE_PFS_TABLE_PLUGIN_SERVICE
*/

/**
  @page PAGE_PFS_PSI Instrumentation interface
  MySQL performance schema instrumentation interface.

  @section PFS_PSI_INTRO Introduction

  Any code can be instrumented with the performance schema,
  regardless of the kind of code used:
  - code within the server implementation itself
  - code within various plugins, including storage engines,
    compiled and delivered with the server (aka, static plugins)
  - code within various plugins, including storage engines,
    compiled and delivered separately from the server (aka, dynamic plugins)
  - code from components, either provided by MySQL or by third parties,
    compiled and delivered separately (and loaded dynamically).

  The instrumentation interface consist of two layers:
  - a raw ABI (Application Binary Interface) layer, that exposes the primitive
  instrumentation functions exported by the performance schema instrumentation
  - an API (Application Programing Interface) layer,
  that provides many helpers for a developer instrumenting some code,
  to make the instrumentation as easy as possible.

  In each case, the API may expand into different code paths,
  and use different ABI infrastructure, depending on how the code is built.
  In each case, the API exposed is consistent:
  instrumented source code is the same, regardless of where it is built.

  The following sections details what happens technically for each type of code.
  The same instrumentation (a mutex) is used, to illustrate.

  @section PFS_PSI_NONE Non instrumented code

  Assume some existing code using a mutex, for example for linux

  @code
  #include <pthread.h>

  pthread_mutex_t foo;

  void do_something()
  {
    pthread_mutex_lock(&foo);
    ...
    pthread_mutex_unlock(&foo);
  }
  @endcode

  MySQL already provides some wrappers,
  to make the code platform independent, as in

  @code
  #include "thr_mutex.h"

  my_mutex_t foo;

  void do_something()
  {
    my_mutex_lock(&foo);
    ...
    my_mutex_unlock(&foo);
  }
  @endcode

  Code using @c my_mutex_t is not instrumented for performance measurement.

  @section PFS_PSI_SERVER Instrumentation for server code

  To instrument the code, use a @c mysql_mutex_t instead.
  All the APIs provided are meant to be used as a simple replacement.

  @code
  #include "mysql/psi/mysql_mutex.h"

  mysql_mutex_t foo;

  void do_something()
  {
    mysql_mutex_lock(&foo);
    ...
    mysql_mutex_unlock(&foo);
  }
  @endcode

  The server code can be built with or without each kind of instrumentation.

  If cmake is configured with @c -DDISABLE_PSI_MUTEX,
  the mutex instrumentation is not compiled.
  In this case, @c HAVE_PSI_MUTEX_INTERFACE is not defined,
  so that @c mysql_mutex_lock() expands into

  @code
static inline int
inline_mysql_mutex_lock(mysql_mutex_t *that,
                        const char *src_file MY_ATTRIBUTE((unused)),
                        uint src_line MY_ATTRIBUTE((unused))
                        )
{
  int result;

  ... Non instrumented code ...
  result = my_mutex_lock(&that->m_mutex);

  return result;
}
  @endcode

  In other words, when not building with instrumentation, @c mysql_mutex_lock()
  is simply replaced with the actual call to @c my_mutex_lock(),

  When building with the mutex instrumentation enabled (the default),
  @c mysql_mutex_lock() expands into this sequence.

  @code
static inline int
inline_mysql_mutex_lock(mysql_mutex_t *that,
                        const char *src_file,
                        uint src_line)
{
  int result;

#ifdef HAVE_PSI_MUTEX_INTERFACE
  if (that->m_psi != NULL)
  {
    ... (a) Instrumentation start ...
    PSI_mutex_locker *locker;
    PSI_mutex_locker_state state;
    locker = PSI_MUTEX_CALL(start_mutex_wait)(
      &state, that->m_psi, PSI_MUTEX_LOCK, src_file, src_line);

    ... (b) Instrumented code ...
    result = my_mutex_lock(&that->m_mutex);

    ... (c) Instrumentation end ...
    if (locker != NULL)
    {
      PSI_MUTEX_CALL(end_mutex_wait)(locker, result);
    }

    return result;
  }
#endif

  ... Non instrumented code ...
  result = my_mutex_lock(&that->m_mutex);

  return result;
}
  @endcode

  For a given instrumentation point in the API, the basic coding pattern
  used is:
  - (a) notify the performance schema of the operation
  about to be performed.
  - (b) execute the instrumented code.
  - (c) notify the performance schema that the operation
  is completed.

  An opaque "locker" pointer is returned by (a), that is given to (c).
  This pointer helps the implementation to keep context, for performances.

  The following code fragment is annotated to show how in detail this pattern
  in implemented, when the instrumentation is compiled in statically.
  In this case,
  the @c PSI_MUTEX_CALL macro expands to a function call,
  which results in

  @code
static inline int
inline_mysql_mutex_lock(mysql_mutex_t *that,
                        const char *src_file,
                        uint src_line)
{
  int result;

#ifdef HAVE_PSI_MUTEX_INTERFACE
  if (that->m_psi != NULL)
  {
    ... (a) Instrumentation start ...
    PSI_mutex_locker *locker;
    PSI_mutex_locker_state state;
    locker = pfs_start_mutex_wait_v1(
      &state, that->m_psi, PSI_MUTEX_LOCK, src_file, src_line);

    ... (b) Instrumented code ...
    result = my_mutex_lock(&that->m_mutex);

    ... (c) Instrumentation end ...
    if (locker != NULL)
    {
      pfs_end_mutex_wait_v1(locker, result);
    }

    return result;
  }
#endif

  ... Non instrumented code ...
  result = my_mutex_lock(&that->m_mutex);

  return result;
}
  @endcode

  When the performance schema is disabled at runtime, the @c m_psi pointer
  is @c NULL, so the extra overhead is one if statement.

  When the performance schema is enabled at runtime, the @c m_psi pointer
  points to the actual instrumentation added to the mutex,
  and two extra calls are present on the code path:
  - a call to @c pfs_start_mutex_wait_v1() before the operation
  - a call to @c pfs_end_mutex_wait_v1() after the operation

  Both these calls are static, and point directly to the
  performance_schema implementation.

  @section PFS_PSI_PLUGIN Instrumentation for a plugin

  Inside a plugin, @c PSI_MUTEX_CALL expands to a call using a function pointer.
  This is to decouple the plugin binary from the implementation,
  to avoid having hard coded references to symbols like
  pfs_start_mutex_wait_v1()
  from the plugin library (either shared or static).

  The plugin library, however, still depends on a symbol
  to be provided by the server,
  in this case @c psi_mutex_service.

  @code
static inline int
inline_mysql_mutex_lock(mysql_mutex_t *that,
                        const char *src_file,
                        uint src_line)
{
  int result;

#ifdef HAVE_PSI_MUTEX_INTERFACE
  if (that->m_psi != NULL)
  {
    ... (a) Instrumentation start ...
    PSI_mutex_locker *locker;
    PSI_mutex_locker_state state;
    locker = psi_mutex_service->start_mutex_wait(
      &state, that->m_psi, PSI_MUTEX_LOCK, src_file, src_line);

    ... (b) Instrumented code ...
    result = my_mutex_lock(&that->m_mutex);

    ... (c) Instrumentation end ...
    if (locker != NULL)
    {
      psi_mutex_service->end_mutex_wait(locker, result);
    }

    return result;
  }
#endif

  ... Non instrumented code ...
  result = my_mutex_lock(&that->m_mutex);

  return result;
}
  @endcode

  This solution works overall, but has some limitations.

  @section PFS_PSI_COMPONENT Instrumentation for component

  The goal when compiling a component is to have zero linker dependencies
  on the server code, and have technical dependencies described as
  references to services instead.

  Another goal is to compile code the same way, independently of the
  technical choices affecting the implementation (pthread mutexes
  or native windows critical sections, SAFE_MUTEX debug helpers or not, etc)

  For the mutex instrumentation in particular, the pattern used
  is to have a service that delegates all decisions to the server code.
  For this reason, all the parameters that might or might not be needed
  depending on the ultimate implementation
  (such as @c __FILE__ and @c __LINE__ for @c SAFE_MUTEX)
  are provided in all cases.

  In a component, the code is instrumented using a different header file,
  everything else is the same.

  @code
  #include "mysql/component/services/mysql_mutex.h"

  mysql_mutex_t foo;

  void do_something()
  {
    mysql_mutex_lock(&foo);
    ...
    mysql_mutex_unlock(&foo);
  }
  @endcode

  The call to @c mysql_mutex_lock() expands into

  @code
  REQUIRES_SERVICE_PLACEHOLDER(mysql_mutex_v1);

  mysql_service_mysql_mutex_v1->lock(M, __FILE__, __LINE__);
  @endcode

  In the component metadata, the dependency on the mutex service needs
  to be declared explicitly, as in:

  @code
  BEGIN_COMPONENT_REQUIRES(pfs_example)
    REQUIRES_SERVICE(mysql_mutex_v1)
  END_COMPONENT_REQUIRES();
  @endcode

  When the component is loaded:
  - the loader inspects the COMPONENT_REQUIRES metadata,
    and find the service dependencies
  - service dependencies are resolved,
    which assigns a proper value to the @c mysql_service_mysql_mutex_v1 pointer.

  From this point, code compiled in the component can be executed normally,
  and calls to @c mysql_mutex_lock() land into the service implementation,
  which is a simple instrumented mutex lock compiled in the server.

  @code
  int
  impl_mysql_mutex_lock(
    mysql_mutex_t *that,
    const char *src_file,
    unsigned int src_line)
  {
    return mysql_mutex_lock_with_src(that, src_file, src_line);
  }
  @endcode

  @section PFS_PSI_LIST List of available instrumentation

  The current list of instrumentation provided by the performance schema
  is as follows:

  <table>
  <caption id="list_pfs_instr">%List of available instrumentation</caption>
  <tr>
    <th rowspan="2">Scope</th>
    <th colspan="2">User code</th>
    <th colspan="2">Implementation entry point</th>
  </tr>
  <tr>
    <th>Server or Plugin</th>
    <th>Component</th>
    <th>Server or Plugin</th>
    <th>Component</th>
  </tr>

  <tr>
    <td>Mutex (high level)</td>
    <td>
@verbatim
#include "mysql/psi/mysql_mutex.h"
mysql_mutex_lock()
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/mysql_mutex.h"
mysql_mutex_lock()
@endverbatim
    </td>
    <td>@ref PSI_mutex_bootstrap</td>
    <td>@ref REQUIRES_MYSQL_MUTEX_SERVICE</td>
  </tr>

  <tr>
    <td>Rwlock (high level)</td>
    <td>
@verbatim
#include "mysql/psi/mysql_rwlock.h"
mysql_rwlock_rdlock()
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/mysql_rwlock.h"
mysql_rwlock_rdlock()
@endverbatim
    </td>
    <td>@ref PSI_rwlock_bootstrap</td>
    <td>@ref REQUIRES_MYSQL_RWLOCK_SERVICE</td>
  </tr>

  <tr>
    <td>Conditions (high level)</td>
    <td>
@verbatim
#include "mysql/psi/mysql_cond.h"
mysql_cond_wait()
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/mysql_cond.h"
mysql_cond_wait()
@endverbatim
    </td>
    <td>@ref PSI_cond_bootstrap</td>
    <td>@ref REQUIRES_MYSQL_COND_SERVICE</td>
  </tr>

  <tr>
    <td>Mutex (low level)</td>
    <td>
@verbatim
#include "mysql/psi/psi_mutex.h"
PSI_MUTEX_CALL(start_mutex_wait)(...)
PSI_MUTEX_CALL(end_mutex_wait)(...)
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_mutex.h"
PSI_MUTEX_CALL(start_mutex_wait)(...)
PSI_MUTEX_CALL(end_mutex_wait)(...)
@endverbatim
    </td>
    <td>@ref PSI_mutex_bootstrap</td>
    <td>@ref REQUIRES_PSI_MUTEX_SERVICE</td>
  </tr>

  <tr>
    <td>Rwlock (low level)</td>
    <td>
@verbatim
#include "mysql/psi/psi_rwlock.h"
PSI_RWLOCK_CALL(start_rwlock_rdwait)(...)
PSI_RWLOCK_CALL(end_rwlock_rdwait)(...)
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_rwlock.h"
PSI_RWLOCK_CALL(start_rwlock_rdwait)(...)
PSI_RWLOCK_CALL(end_rwlock_rdwait)(...)
@endverbatim
    </td>
    <td>@ref PSI_rwlock_bootstrap</td>
    <td>@ref REQUIRES_PSI_RWLOCK_SERVICE</td>
  </tr>

  <tr>
    <td>Conditions (low level)</td>
    <td>
@verbatim
#include "mysql/psi/mysql_cond.h"
PSI_COND_CALL(start_cond_wait)(...)
PSI_COND_CALL(end_cond_wait)(...)
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_cond.h"
PSI_COND_CALL(start_cond_wait)(...)
PSI_COND_CALL(end_cond_wait)(...)
@endverbatim
    </td>
    <td>@ref PSI_cond_bootstrap</td>
    <td>@ref REQUIRES_PSI_COND_SERVICE</td>
  </tr>

  <tr>
    <td>Errors</td>
    <td>
@verbatim
#include "mysql/psi/psi_error.h"
PSI_ERROR_CALL(log_error)(...)
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_error.h"
PSI_ERROR_CALL(log_error)(...)
@endverbatim
    </td>
    <td>@ref PSI_error_bootstrap</td>
    <td>@ref REQUIRES_PSI_ERROR_SERVICE</td>
  </tr>

  <tr>
    <td>File</td>
    <td>
@verbatim
#include "mysql/psi/psi_file.h"
PSI_FILE_CALL(start_file_wait)(...)
PSI_FILE_CALL(end_file_wait)(...)
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_file.h"
PSI_FILE_CALL(start_file_wait)(...)
PSI_FILE_CALL(end_file_wait)(...)
@endverbatim
    </td>
    <td>@ref PSI_file_bootstrap</td>
    <td>@ref REQUIRES_PSI_FILE_SERVICE</td>
  </tr>

  <tr>
    <td>Idle</td>
    <td>
@verbatim
#include "mysql/psi/psi_idle.h"
PSI_IDLE_CALL(start_idle_wait)(...)
PSI_IDLE_CALL(end_idle_wait)(...)
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_idle.h"
PSI_IDLE_CALL(start_idle_wait)(...)
PSI_IDLE_CALL(end_idle_wait)(...)
@endverbatim
    </td>
    <td>@ref PSI_idle_bootstrap</td>
    <td>@ref REQUIRES_PSI_IDLE_SERVICE</td>
  </tr>

  <tr>
    <td>Metadata locks</td>
    <td>
@verbatim
#include "mysql/psi/psi_mdl.h"
PSI_METADATA_CALL(start_metadata_wait)(...)
PSI_METADATA_CALL(end_metadata_wait)(...)
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_mdl.h"
PSI_METADATA_CALL(start_metadata_wait)(...)
PSI_METADATA_CALL(end_metadata_wait)(...)
@endverbatim
    </td>
    <td>@ref PSI_mdl_bootstrap</td>
    <td>@ref REQUIRES_PSI_MDL_SERVICE</td>
  </tr>

  <tr>
    <td>Memory</td>
    <td>
@verbatim
#include "mysql/psi/psi_memory.h"
PSI_MEMORY_CALL(memory_alloc)(...)
PSI_MEMORY_CALL(memory_free)(...)
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_memory.h"
PSI_MEMORY_CALL(memory_alloc)(...)
PSI_MEMORY_CALL(memory_free)(...)
@endverbatim
    </td>
    <td>@ref PSI_memory_bootstrap</td>
    <td>@ref REQUIRES_PSI_MEMORY_SERVICE</td>
  </tr>

  <tr>
    <td>Socket</td>
    <td>
@verbatim
#include "mysql/psi/psi_socket.h"
PSI_SOCKET_CALL(start_socket_wait)(...)
PSI_SOCKET_CALL(end_socket_wait)(...)
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_socket.h"
PSI_SOCKET_CALL(start_socket_wait)(...)
PSI_SOCKET_CALL(end_socket_wait)(...)
@endverbatim
    </td>
    <td>@ref PSI_socket_bootstrap</td>
    <td>@ref REQUIRES_PSI_SOCKET_SERVICE</td>
  </tr>

  <tr>
    <td>Stage</td>
    <td>
@verbatim
#include "mysql/psi/psi_stage.h"
PSI_STAGE_CALL(start_stage)(...)
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_stage.h"
PSI_STAGE_CALL(start_stage)(...)
@endverbatim
    </td>
    <td>@ref PSI_stage_bootstrap</td>
    <td>@ref REQUIRES_PSI_STAGE_SERVICE</td>
  </tr>

  <tr>
    <td>Statement</td>
    <td>
@verbatim
#include "mysql/psi/psi_statement.h"
PSI_STATEMENT_CALL(start_statement)(...)
PSI_STATEMENT_CALL(end_statement)(...)

Extra helpers available:
MYSQL_START_STATEMENT()
MYSQL_END_STATEMENT()
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_statement.h"
PSI_STATEMENT_CALL(start_statement)(...)
PSI_STATEMENT_CALL(end_statement)(...)
@endverbatim
    </td>
    <td>@ref PSI_statement_bootstrap</td>
    <td>@ref REQUIRES_PSI_STATEMENT_SERVICE</td>
  </tr>

  <tr>
    <td>%Table</td>
    <td>
@verbatim
#include "mysql/psi/psi_table.h"
PSI_TABLE_CALL(start_table_io_wait)(...)
PSI_TABLE_CALL(end_table_io_wait)(...)
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_table.h"
PSI_TABLE_CALL(start_table_io_wait)(...)
PSI_TABLE_CALL(end_table_io_wait)(...)
@endverbatim
    </td>
    <td>@ref PSI_table_bootstrap</td>
    <td>@ref REQUIRES_PSI_TABLE_SERVICE</td>
  </tr>

  <tr>
    <td>System</td>
    <td>
@verbatim
#include "mysql/psi/psi_system.h"
PSI_SYSTEM_CALL(plugin_unload)(...)
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_system.h"
PSI_SYSTEM_CALL(plugin_unload)(...)
@endverbatim
    </td>
    <td>@ref PSI_system_bootstrap</td>
    <td>@ref REQUIRES_PSI_SYSTEM_SERVICE</td>
  </tr>

  <tr>
    <td>Thread</td>
    <td>
@verbatim
#include "mysql/psi/psi_thread.h"
PSI_THREAD_CALL(spawn_thread)(...)
PSI_THREAD_CALL(delete_thread)(...)
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_thread.h"
PSI_THREAD_CALL(spawn_thread)(...)
PSI_THREAD_CALL(delete_thread)(...)
@endverbatim
    </td>
    <td>@ref PSI_thread_bootstrap</td>
    <td>@ref REQUIRES_PSI_THREAD_SERVICE</td>
  </tr>

  <tr>
    <td>Transaction</td>
    <td>
@verbatim
#include "mysql/psi/psi_transaction.h"
PSI_TRANSACTION_CALL(start_transaction)(...)
PSI_TRANSACTION_CALL(end_transaction)(...)
@endverbatim
    </td>
    <td>
@verbatim
#include "mysql/components/services/psi_transaction.h"
PSI_TRANSACTION_CALL(start_transaction)(...)
PSI_TRANSACTION_CALL(end_transaction)(...)
@endverbatim
    </td>
    <td>@ref PSI_transaction_bootstrap</td>
    <td>@ref REQUIRES_PSI_TRANSACTION_SERVICE</td>
  </tr>

  <tr>
    <td>Data locks</td>
    <td>
@verbatim
#include "mysql/psi/psi_data_lock.h"
PSI_DATA_LOCK_CALL(register_data_lock)(...)
PSI_DATA_LOCK_CALL(unregister_data_lock)(...)
@endverbatim
    </td>
    <td> Not available as a service.</td>
    <td>@ref PSI_data_lock_bootstrap</td>
    <td>Not available as a service.</td>
  </tr>

  </table>
*/

/**
  @page PAGE_PFS_AGGREGATES Aggregates
  Performance schema aggregates.

  @section PFS_AGG_INTRO Introduction

  Aggregates tables are tables that can be formally defined as
  SELECT ... from EVENTS_WAITS_HISTORY_INFINITE ... group by 'group clause'.

  Each group clause defines a different kind of aggregate, and corresponds to
  a different table exposed by the performance schema.

  Aggregates can be either:
  - computed on the fly,
  - computed on demand, based on other available data.

  'EVENTS_WAITS_HISTORY_INFINITE' is a table that does not exist,
  the best approximation is EVENTS_WAITS_HISTORY_LONG.
  Aggregates computed on the fly in fact are based on EVENTS_WAITS_CURRENT,
  while aggregates computed on demand are based on other
  EVENTS_WAITS_SUMMARY_BY_xxx tables.

  To better understand the implementation itself, a bit of math is
  required first, to understand the model behind the code:
  the code is deceptively simple, the real complexity resides
  in the flyweight of pointers between various performance schema buffers.

  @section DIMENSION Concept of dimension

  An event measured by the instrumentation has many attributes.
  An event is represented as a data point P(x1, x2, ..., xN),
  where each x_i coordinate represents a given attribute value.

  Examples of attributes are:
  - the time waited
  - the object waited on
  - the instrument waited on
  - the thread that waited
  - the operation performed
  - per object or per operation additional attributes, such as spins,
  number of bytes, etc.

  Computing an aggregate per thread is fundamentally different from
  computing an aggregate by instrument, so the "_BY_THREAD" and
  "_BY_EVENT_NAME" aggregates are different dimensions,
  operating on different x_i and x_j coordinates.
  These aggregates are "orthogonal".

  @section PROJECTION Concept of projection

  A given x_i attribute value can convey either just one basic information,
  such as a number of bytes, or can convey implied information,
  such as an object fully qualified name.

  For example, from the value "test.t1", the name of the object schema
  "test" can be separated from the object name "t1", so that now aggregates
  by object schema can be implemented.

  In math terms, that corresponds to defining a function:
  F_i (x): x --> y
  Applying this function to our point P gives another point P':

  F_i (P):
  P(x1, x2, ..., x{i-1}, x_i, x{i+1}, ..., x_N)
  --> P' (x1, x2, ..., x{i-1}, f_i(x_i), x{i+1}, ..., x_N)

  That function defines in fact an aggregate !
  In SQL terms, this aggregate would look like the following table:

@verbatim
  CREATE VIEW EVENTS_WAITS_SUMMARY_BY_Func_i AS
  SELECT col_1, col_2, ..., col_{i-1},
         Func_i(col_i),
         COUNT(col_i),
         MIN(col_i), AVG(col_i), MAX(col_i), -- if col_i is a numeric value
         col_{i+1}, ..., col_N
         FROM EVENTS_WAITS_HISTORY_INFINITE
         group by col_1, col_2, ..., col_{i-1}, col{i+1}, ..., col_N.
@endverbatim

  Note that not all columns have to be included,
  in particular some columns that are dependent on the x_i column should
  be removed, so that in practice, MySQL's aggregation method tends to
  remove many attributes at each aggregation steps.

  For example, when aggregating wait events by object instances,
  - the wait_time and number_of_bytes can be summed,
  and sum(wait_time) now becomes an object instance attribute.
  - the source, timer_start, timer_end columns are not in the
  _BY_INSTANCE table, because these attributes are only
  meaningful for a wait.

  @section COMPOSITION Concept of composition

  Now, the "test.t1" --> "test" example was purely theory,
  just to explain the concept, and does not lead very far.
  Let's look at a more interesting example of data that can be derived
  from the row event.

  An event creates a transient object, PFS_wait_locker, per operation.
  This object's life cycle is extremely short: it's created just
  before the start_wait() instrumentation call, and is destroyed in
  the end_wait() call.

  The wait locker itself contains a pointer to the object instance
  waited on.
  That allows to implement a wait_locker --> object instance projection,
  with m_target.
  The object instance life cycle depends on _init and _destroy calls
  from the code, such as mysql_mutex_init()
  and mysql_mutex_destroy() for a mutex.

  The object instance waited on contains a pointer to the object class,
  which is represented by the instrument name.
  That allows to implement an object instance --> object class projection.
  The object class life cycle is permanent, as instruments are loaded in
  the server and never removed.

  The object class is named in such a way
  (for example, "wait/sync/mutex/sql/LOCK_open",
  "wait/io/file/maria/data_file) that the component ("sql", "maria")
  that it belongs to can be inferred.
  That allows to implement an object class --> server component projection.

  Back to math again, we have, for example for mutexes:

  F1 (l) : PFS_wait_locker l --> PFS_mutex m = l->m_target.m_mutex

  F1_to_2 (m) : PFS_mutex m --> PFS_mutex_class i = m->m_class

  F2_to_3 (i) : PFS_mutex_class i --> const char *component =
                                        substring(i->m_name, ...)

  Per components aggregates are not implemented, this is just an illustration.

  F1 alone defines this aggregate:

  EVENTS_WAITS_HISTORY_INFINITE --> EVENTS_WAITS_SUMMARY_BY_INSTANCE
  (or MUTEX_INSTANCE)

  F1_to_2 alone could define this aggregate:

  EVENTS_WAITS_SUMMARY_BY_INSTANCE --> EVENTS_WAITS_SUMMARY_BY_EVENT_NAME

  Alternatively, using function composition, with
  F2 = F1_to_2 o F1, F2 defines:

  EVENTS_WAITS_HISTORY_INFINITE --> EVENTS_WAITS_SUMMARY_BY_EVENT_NAME

  Likewise, F_2_to_3 defines:

  EVENTS_WAITS_SUMMARY_BY_EVENT_NAME --> EVENTS_WAITS_SUMMARY_BY_COMPONENT

  and F3 = F_2_to_3 o F_1_to_2 o F1 defines:

  EVENTS_WAITS_HISTORY_INFINITE --> EVENTS_WAITS_SUMMARY_BY_COMPONENT

  What has all this to do with the code ?

  Functions (or aggregates) such as F_3 are not implemented as is.
  Instead, they are decomposed into F_2_to_3 o F_1_to_2 o F1,
  and each intermediate aggregate is stored into an internal buffer.
  This allows to support every F1, F2, F3 aggregates from shared
  internal buffers, where computation already performed to compute F2
  is reused when computing F3.

  @section OBJECT_GRAPH Object graph

  In terms of object instances, or records, pointers between
  different buffers define an object instance graph.

  For example, assuming the following scenario:
  - A mutex class "M" is instrumented, the instrument name
  is "wait/sync/mutex/sql/M"
  - This mutex instrument has been instantiated twice,
  mutex instances are noted M-1 and M-2
  - Threads T-A and T-B are locking mutex instance M-1
  - Threads T-C and T-D are locking mutex instance M-2

  The performance schema will record the following data:
  - EVENTS_WAITS_CURRENT has 4 rows, one for each mutex locker
  - EVENTS_WAITS_SUMMARY_BY_INSTANCE shows 2 rows, for M-1 and M-2
  - EVENTS_WAITS_SUMMARY_BY_EVENT_NAME shows 1 row, for M

  The graph of structures will look like:

  @startuml

  object "PFS_wait_locker (T-A, M-1)" as TAM1
  object "PFS_wait_locker (T-B, M-1)" as TBM1
  object "PFS_wait_locker (T-C, M-2)" as TCM2
  object "PFS_wait_locker (T-D, M-2)" as TDM2

  object "PFS_mutex (M-1)" as M1
  M1 : m_wait_stat

  object "PFS_mutex (M-2)" as M2
  M2 : m_wait_stat

  object "PFS_mutex_class (M)" as M
  M : m_wait_stat

  M <-- M1
  M1 <-- TAM1
  M1 <-- TBM1
  M <-- M2
  M2 <-- TCM2
  M2 <-- TDM2

  M1 .right. M2
  TAM1 .right. TBM1
  TBM1 .right. TCM2
  TCM2 .right. TDM2

  note right of M
    PFS_mutex_class displayed in TABLE EVENTS_WAITS_SUMMARY_BY_EVENT_NAME
  end note

  note right of M2
    PFS_mutex displayed in TABLE EVENTS_WAITS_SUMMARY_BY_INSTANCE
  end note

  note right of TDM2
    PFS_wait_lockers displayed in TABLE EVENTS_WAITS_CURRENT
  end note

  @enduml

  @section ON_THE_FLY On the fly aggregates

  'On the fly' aggregates are computed during the code execution.
  This is necessary because the data the aggregate is based on is volatile,
  and can not be kept indefinitely.

  With on the fly aggregates:
  - the writer thread does all the computation
  - the reader thread accesses the result directly

  This model is to be avoided if possible, due to the overhead
  caused when instrumenting code.

  @section HIGHER_LEVEL Higher level aggregates

  'Higher level' aggregates are implemented on demand only.
  The code executing a SELECT from the aggregate table is
  collecting data from multiple internal buffers to produce the result.

  With higher level aggregates:
  - the reader thread does all the computation
  - the writer thread has no overhead.

  @section MIXED Mixed level aggregates

  The 'Mixed' model is a compromise between 'On the fly' and 'Higher level'
  aggregates, for internal buffers that are not permanent.

  While an object is present in a buffer, the higher level model is used.
  When an object is about to be destroyed, statistics are saved into
  a 'parent' buffer with a longer life cycle, to follow the on the fly model.

  With mixed aggregates:
  - the reader thread does a lot of complex computation,
  - the writer thread has minimal overhead, on destroy events.

  @section IMPL_WAIT Implementation for waits aggregates

  For waits, the tables that contains aggregated wait data are:
  - EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME
  - EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME
  - EVENTS_WAITS_SUMMARY_BY_INSTANCE
  - EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
  - EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME
  - EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME
  - FILE_SUMMARY_BY_EVENT_NAME
  - FILE_SUMMARY_BY_INSTANCE
  - SOCKET_SUMMARY_BY_INSTANCE
  - SOCKET_SUMMARY_BY_EVENT_NAME
  - OBJECTS_SUMMARY_GLOBAL_BY_TYPE

  The instrumented code that generates waits events consist of:
  - mutexes (mysql_mutex_t)
  - rwlocks (mysql_rwlock_t)
  - conditions (mysql_cond_t)
  - file I/O (MYSQL_FILE)
  - socket I/O (MYSQL_SOCKET)
  - table I/O
  - table lock
  - idle

  The flow of data between aggregates tables varies for each instrumentation.

  @subsection IMPL_WAIT_MUTEX Mutex waits

@verbatim
  mutex_locker(T, M)
   |
   | [1]
   |
   |-> pfs_mutex(M)                           =====>> [B], [C]
   |    |
   |    | [2]
   |    |
   |    |-> pfs_mutex_class(M.class)          =====>> [C]
   |
   |-> pfs_thread(T).event_name(M)            =====>> [A], [D], [E], [F]
        |
        | [3]
        |
     3a |-> pfs_account(U, H).event_name(M)   =====>> [D], [E], [F]
        .    |
        .    | [4-RESET]
        .    |
     3b .....+-> pfs_user(U).event_name(M)    =====>> [E]
        .    |
     3c .....+-> pfs_host(H).event_name(M)    =====>> [F]
@endverbatim

  How to read this diagram:
  - events that occur during the instrumented code execution
  are noted with numbers, as in [1].
  Code executed by these events has an impact on overhead.
  - events that occur during TRUNCATE TABLE operations are noted with numbers,
  followed by "-RESET", as in [4-RESET].
  Code executed by these events has no impact on overhead,
  since they are executed by independent monitoring sessions.
  - events that occur when a reader extracts data
  from a performance schema table are noted with letters, as in [A].
  The name of the table involved,
  and the method that builds a row are documented.
  Code executed by these events
  has no impact on the instrumentation overhead. Note that the table
  implementation may pull data from different buffers.
  - nominal code paths are in plain lines. A "nominal" code path corresponds to
  cases where the performance schema buffers are sized
  so that no records are lost.
  - degenerated code paths are in dotted lines.
  A "degenerated" code path corresponds
  to edge cases where parent buffers are full,
  which forces the code to aggregate to
  grand parents directly.

  Implemented as:
  - [1] #pfs_start_mutex_wait_v1(), #pfs_end_mutex_wait_v1()
  - [2] #pfs_destroy_mutex_v1()
  - [3] #aggregate_thread_waits()
  - [4] PFS_account::aggregate_waits()
  - [A] EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME,
        @c table_ews_by_thread_by_event_name::make_row()
  - [B] EVENTS_WAITS_SUMMARY_BY_INSTANCE,
        @c table_events_waits_summary_by_instance::make_mutex_row()
  - [C] EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME,
        @c table_ews_global_by_event_name::make_mutex_row()
  - [D] EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME,
        @c table_ews_by_account_by_event_name::make_row()
  - [E] EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME,
        @c table_ews_by_user_by_event_name::make_row()
  - [F] EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME,
        @c table_ews_by_host_by_event_name::make_row()

  Table EVENTS_WAITS_SUMMARY_BY_INSTANCE is a 'on the fly' aggregate,
  because the data is collected on the fly by (1) and stored into a buffer,
  pfs_mutex. The table implementation [B] simply reads the results directly
  from this buffer.

  Table EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME is a 'mixed' aggregate,
  because some data is collected on the fly (1),
  some data is preserved with (2) at a later time in the life cycle,
  and two different buffers pfs_mutex and pfs_mutex_class are used to store the
  statistics collected. The table implementation [C] is more complex, since
  it reads from two buffers pfs_mutex and pfs_mutex_class.

  @subsection IMPL_WAIT_RWLOCK Rwlock waits

@verbatim
  rwlock_locker(T, R)
   |
   | [1]
   |
   |-> pfs_rwlock(R)                          =====>> [B], [C]
   |    |
   |    | [2]
   |    |
   |    |-> pfs_rwlock_class(R.class)         =====>> [C]
   |
   |-> pfs_thread(T).event_name(R)            =====>> [A]
        |
       ...
@endverbatim

  Implemented as:
  - [1] #pfs_start_rwlock_rdwait_v1(), #pfs_end_rwlock_rdwait_v1(), ...
  - [2] #pfs_destroy_rwlock_v1()
  - [A] EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME,
        @c table_ews_by_thread_by_event_name::make_row()
  - [B] EVENTS_WAITS_SUMMARY_BY_INSTANCE,
        @c table_events_waits_summary_by_instance::make_rwlock_row()
  - [C] EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME,
        @c table_ews_global_by_event_name::make_rwlock_row()

  @subsection IMPL_WAIT_COND Cond waits

@verbatim
  cond_locker(T, C)
   |
   | [1]
   |
   |-> pfs_cond(C)                            =====>> [B], [C]
   |    |
   |    | [2]
   |    |
   |    |-> pfs_cond_class(C.class)           =====>> [C]
   |
   |-> pfs_thread(T).event_name(C)            =====>> [A]
        |
       ...
@endverbatim

  Implemented as:
  - [1] #pfs_start_cond_wait_v1(), #pfs_end_cond_wait_v1()
  - [2] #pfs_destroy_cond_v1()
  - [A] EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME,
        @c table_ews_by_thread_by_event_name::make_row()
  - [B] EVENTS_WAITS_SUMMARY_BY_INSTANCE,
        @c table_events_waits_summary_by_instance::make_cond_row()
  - [C] EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME,
        @c table_ews_global_by_event_name::make_cond_row()

  @subsection IMPL_WAIT_FILE File waits

@verbatim
  file_locker(T, F)
   |
   | [1]
   |
   |-> pfs_file(F)                            =====>> [B], [C], [D], [E]
   |    |
   |    | [2]
   |    |
   |    |-> pfs_file_class(F.class)           =====>> [C], [D]
   |
   |-> pfs_thread(T).event_name(F)            =====>> [A]
        |
       ...
@endverbatim

  Implemented as:
  - [1] #pfs_get_thread_file_name_locker_v1(), #pfs_start_file_wait_v1(),
        #pfs_end_file_wait_v1(), ...
  - [2] #pfs_start_file_close_wait_v1(), #pfs_end_file_close_wait_v1()
  - [A] EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME,
        @c table_ews_by_thread_by_event_name::make_row()
  - [B] EVENTS_WAITS_SUMMARY_BY_INSTANCE,
        @c table_events_waits_summary_by_instance::make_file_row()
  - [C] EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME,
        @c table_ews_global_by_event_name::make_file_row()
  - [D] FILE_SUMMARY_BY_EVENT_NAME,
        @c table_file_summary_by_event_name::make_row()
  - [E] FILE_SUMMARY_BY_INSTANCE,
        @c table_file_summary_by_instance::make_row()

  @subsection IMPL_WAIT_SOCKET Socket waits

@verbatim
  socket_locker(T, S)
   |
   | [1]
   |
   |-> pfs_socket(S)                            =====>> [A], [B], [C], [D], [E]
        |
        | [2]
        |
        |-> pfs_socket_class(S.class)           =====>> [C], [D]
        |
        |-> pfs_thread(T).event_name(S)         =====>> [A]
        |
        | [3]
        |
     3a |-> pfs_account(U, H).event_name(S)     =====>> [F], [G], [H]
        .    |
        .    | [4-RESET]
        .    |
     3b .....+-> pfs_user(U).event_name(S)      =====>> [G]
        .    |
     3c .....+-> pfs_host(H).event_name(S)      =====>> [H]
@endverbatim

  Implemented as:
  - [1] #pfs_start_socket_wait_v1(), #pfs_end_socket_wait_v1().
  - [2] #pfs_destroy_socket_v1()
  - [3] #aggregate_thread_waits()
  - [4] @c PFS_account::aggregate_waits()
  - [5] @c PFS_host::aggregate_waits()
  - [A] EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME,
        @c table_ews_by_thread_by_event_name::make_row()
  - [B] EVENTS_WAITS_SUMMARY_BY_INSTANCE,
        @c table_events_waits_summary_by_instance::make_socket_row()
  - [C] EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME,
        @c table_ews_global_by_event_name::make_socket_row()
  - [D] SOCKET_SUMMARY_BY_EVENT_NAME,
        @c table_socket_summary_by_event_name::make_row()
  - [E] SOCKET_SUMMARY_BY_INSTANCE,
        @c table_socket_summary_by_instance::make_row()
  - [F] EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME,
        @c table_ews_by_account_by_event_name::make_row()
  - [G] EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME,
        @c table_ews_by_user_by_event_name::make_row()
  - [H] EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME,
        @c table_ews_by_host_by_event_name::make_row()

  @subsection IMPL_WAIT_TABLE Table waits

@verbatim
  table_locker(Thread Th, Table Tb, Event = I/O or lock)
   |
   | [1]
   |
1a |-> pfs_table(Tb)                          =====>> [A], [B], [C]
   |    |
   |    | [2]
   |    |
   |    |-> pfs_table_share(Tb.share)         =====>> [B], [C]
   |         |
   |         | [3]
   |         |
   |         |-> global_table_io_stat         =====>> [C]
   |         |
   |         |-> global_table_lock_stat       =====>> [C]
   |
1b |-> pfs_thread(Th).event_name(E)           =====>> [D], [E], [F], [G]
   |    |
   |    | [ 4-RESET]
   |    |
   |    |-> pfs_account(U, H).event_name(E)   =====>> [E], [F], [G]
   |    .    |
   |    .    | [5-RESET]
   |    .    |
   |    .....+-> pfs_user(U).event_name(E)    =====>> [F]
   |    .    |
   |    .....+-> pfs_host(H).event_name(E)    =====>> [G]
   |
1c |-> pfs_thread(Th).waits_current(W)        =====>> [H]
   |
1d |-> pfs_thread(Th).waits_history(W)        =====>> [I]
   |
1e |-> waits_history_long(W)                  =====>> [J]
@endverbatim

  Implemented as:
  - [1] #pfs_start_table_io_wait_v1(), #pfs_end_table_io_wait_v1()
  - [2] #pfs_close_table_v1()
  - [3] #pfs_drop_table_share_v1()
  - [4] TRUNCATE TABLE EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
  - [5] TRUNCATE TABLE EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME
  - [A] TABLE_IO_WAITS_SUMMARY_BY_TABLE,
        @c table_tiws_by_table::make_row()
  - [B] OBJECTS_SUMMARY_GLOBAL_BY_TYPE,
        @c table_os_global_by_type::make_table_row()
  - [C] EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME,
        @c table_ews_global_by_event_name::make_table_io_row(),
        @c table_ews_global_by_event_name::make_table_lock_row()
  - [D] EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME,
        @c table_ews_by_thread_by_event_name::make_row()
  - [E] EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME,
        @c table_ews_by_account_by_event_name::make_row()
  - [F] EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME,
        @c table_ews_by_user_by_event_name::make_row()
  - [G] EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME,
        @c table_ews_by_host_by_event_name::make_row()
  - [H] EVENTS_WAITS_CURRENT,
        @c table_events_waits_common::make_row()
  - [I] EVENTS_WAITS_HISTORY,
        @c table_events_waits_common::make_row()
  - [J] EVENTS_WAITS_HISTORY_LONG,
        @c table_events_waits_common::make_row()

  @section IMPL_STAGE Implementation for stages aggregates

  For stages, the tables that contains aggregated data are:
  - EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME
  - EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME
  - EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME
  - EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME
  - EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME

@verbatim
  start_stage(T, S)
   |
   | [1]
   |
1a |-> pfs_thread(T).event_name(S)            =====>> [A], [B], [C], [D], [E]
   |    |
   |    | [2]
   |    |
   | 2a |-> pfs_account(U, H).event_name(S)   =====>> [B], [C], [D], [E]
   |    .    |
   |    .    | [3-RESET]
   |    .    |
   | 2b .....+-> pfs_user(U).event_name(S)    =====>> [C]
   |    .    |
   | 2c .....+-> pfs_host(H).event_name(S)    =====>> [D], [E]
   |    .    .    |
   |    .    .    | [4-RESET]
   | 2d .    .    |
1b |----+----+----+-> pfs_stage_class(S)      =====>> [E]

@endverbatim

  Implemented as:
  - [1] #pfs_start_stage_v1()
  - [2] #pfs_delete_thread_v1(), #aggregate_thread_stages()
  - [3] @c PFS_account::aggregate_stages()
  - [4] @c PFS_host::aggregate_stages()
  - [A] EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME,
        @c table_esgs_by_thread_by_event_name::make_row()
  - [B] EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME,
        @c table_esgs_by_account_by_event_name::make_row()
  - [C] EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME,
        @c table_esgs_by_user_by_event_name::make_row()
  - [D] EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME,
        @c table_esgs_by_host_by_event_name::make_row()
  - [E] EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME,
        @c table_esgs_global_by_event_name::make_row()

@section IMPL_STATEMENT Implementation for statements consumers

  For statements, the tables that contains individual event data are:
  - EVENTS_STATEMENTS_CURRENT
  - EVENTS_STATEMENTS_HISTORY
  - EVENTS_STATEMENTS_HISTORY_LONG

  For statements, the tables that contains aggregated data are:
  - EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME
  - EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME
  - EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME
  - EVENTS_STATEMENTS_SUMMARY_BY_USER_BY_EVENT_NAME
  - EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME
  - EVENTS_STATEMENTS_SUMMARY_BY_DIGEST

@verbatim
  statement_locker(T, S)
   |
   | [1]
   |
1a |-> pfs_thread(T).event_name(S)            =====>> [A], [B], [C], [D], [E]
   |    |
   |    | [2]
   |    |
   | 2a |-> pfs_account(U, H).event_name(S)   =====>> [B], [C], [D], [E]
   |    .    |
   |    .    | [3-RESET]
   |    .    |
   | 2b .....+-> pfs_user(U).event_name(S)    =====>> [C]
   |    .    |
   | 2c .....+-> pfs_host(H).event_name(S)    =====>> [D], [E]
   |    .    .    |
   |    .    .    | [4-RESET]
   | 2d .    .    |
1b |----+----+----+-> pfs_statement_class(S)  =====>> [E]
   |
1c |-> pfs_thread(T).statement_current(S)     =====>> [F]
   |
1d |-> pfs_thread(T).statement_history(S)     =====>> [G]
   |
1e |-> statement_history_long(S)              =====>> [H]
   |
1f |-> statement_digest(S)                    =====>> [I]

@endverbatim

  Implemented as:
  - [1] #pfs_start_statement_v1(), #pfs_end_statement_v1()
       (1a, 1b) is an aggregation by EVENT_NAME,
        (1c, 1d, 1e) is an aggregation by TIME,
        (1f) is an aggregation by DIGEST
        all of these are orthogonal,
        and implemented in #pfs_end_statement_v1().
  - [2] #pfs_delete_thread_v1(), #aggregate_thread_statements()
  - [3] @c PFS_account::aggregate_statements()
  - [4] @c PFS_host::aggregate_statements()
  - [A] EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME,
        @c table_esms_by_thread_by_event_name::make_row()
  - [B] EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME,
        @c table_esms_by_account_by_event_name::make_row()
  - [C] EVENTS_STATEMENTS_SUMMARY_BY_USER_BY_EVENT_NAME,
        @c table_esms_by_user_by_event_name::make_row()
  - [D] EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME,
        @c table_esms_by_host_by_event_name::make_row()
  - [E] EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME,
        @c table_esms_global_by_event_name::make_row()
  - [F] EVENTS_STATEMENTS_CURRENT,
        @c table_events_statements_current::make_row()
  - [G] EVENTS_STATEMENTS_HISTORY,
        @c table_events_statements_history::make_row()
  - [H] EVENTS_STATEMENTS_HISTORY_LONG,
        @c table_events_statements_history_long::make_row()
  - [I] EVENTS_STATEMENTS_SUMMARY_BY_DIGEST
        @c table_esms_by_digest::make_row()

@section IMPL_TRANSACTION Implementation for transactions consumers

  For transactions, the tables that contains individual event data are:
  - EVENTS_TRANSACTIONS_CURRENT
  - EVENTS_TRANSACTIONS_HISTORY
  - EVENTS_TRANSACTIONS_HISTORY_LONG

  For transactions, the tables that contains aggregated data are:
  - EVENTS_TRANSACTIONS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME
  - EVENTS_TRANSACTIONS_SUMMARY_BY_HOST_BY_EVENT_NAME
  - EVENTS_TRANSACTIONS_SUMMARY_BY_THREAD_BY_EVENT_NAME
  - EVENTS_TRANSACTIONS_SUMMARY_BY_USER_BY_EVENT_NAME
  - EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME

@verbatim
  transaction_locker(T, TX)
   |
   | [1]
   |
1a |-> pfs_thread(T).event_name(TX)              =====>> [A], [B], [C], [D], [E]
   |    |
   |    | [2]
   |    |
   | 2a |-> pfs_account(U, H).event_name(TX)     =====>> [B], [C], [D], [E]
   |    .    |
   |    .    | [3-RESET]
   |    .    |
   | 2b .....+-> pfs_user(U).event_name(TX)      =====>> [C]
   |    .    |
   | 2c .....+-> pfs_host(H).event_name(TX)      =====>> [D], [E]
   |    .    .    |
   |    .    .    | [4-RESET]
   | 2d .    .    |
1b |----+----+----+-> pfs_transaction_class(TX)  =====>> [E]
   |
1c |-> pfs_thread(T).transaction_current(TX)     =====>> [F]
   |
1d |-> pfs_thread(T).transaction_history(TX)     =====>> [G]
   |
1e |-> transaction_history_long(TX)              =====>> [H]

@endverbatim

  Implemented as:
  - [1] #pfs_start_transaction_v1(), #pfs_end_transaction_v1()
       (1a, 1b) is an aggregation by EVENT_NAME,
        (1c, 1d, 1e) is an aggregation by TIME,
        all of these are orthogonal,
        and implemented in #pfs_end_transaction_v1().
  - [2] #pfs_delete_thread_v1(), #aggregate_thread_transactions()
  - [3] @c PFS_account::aggregate_transactions()
  - [4] @c PFS_host::aggregate_transactions()

  - [A] EVENTS_TRANSACTIONS_SUMMARY_BY_THREAD_BY_EVENT_NAME,
        @c table_ets_by_thread_by_event_name::make_row()
  - [B] EVENTS_TRANSACTIONS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME,
        @c table_ets_by_account_by_event_name::make_row()
  - [C] EVENTS_TRANSACTIONS_SUMMARY_BY_USER_BY_EVENT_NAME,
        @c table_ets_by_user_by_event_name::make_row()
  - [D] EVENTS_TRANSACTIONS_SUMMARY_BY_HOST_BY_EVENT_NAME,
        @c table_ets_by_host_by_event_name::make_row()
  - [E] EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME,
        @c table_ets_global_by_event_name::make_row()
  - [F] EVENTS_TRANSACTIONS_CURRENT,
        @c table_events_transactions_current::rnd_next(),
        @c table_events_transactions_common::make_row()
  - [G] EVENTS_TRANSACTIONS_HISTORY,
        @c table_events_transactions_history::rnd_next(),
        @c table_events_transactions_common::make_row()
  - [H] EVENTS_TRANSACTIONS_HISTORY_LONG,
        @c table_events_transactions_history_long::rnd_next(),
        @c table_events_transactions_common::make_row()

@section IMPL_MEMORY Implementation for memory instruments

  For memory, there are no tables that contains individual event data.

  For memory, the tables that contains aggregated data are:
  - MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME
  - MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME
  - MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME
  - MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME
  - MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME

@verbatim
  memory_event(T, S)
   |
   | [1]
   |
1a |-> pfs_thread(T).event_name(S)            =====>> [A], [B], [C], [D], [E]
   |    |
   |    | [2]
   |    |
1+ | 2a |-> pfs_account(U, H).event_name(S)   =====>> [B], [C], [D], [E]
   |    .    |
   |    .    | [3-RESET]
   |    .    |
1+ | 2b .....+-> pfs_user(U).event_name(S)    =====>> [C]
   |    .    |
1+ | 2c .....+-> pfs_host(H).event_name(S)    =====>> [D], [E]
   |    .    .    |
   |    .    .    | [4-RESET]
   | 2d .    .    |
1b |----+----+----+-> global.event_name(S)    =====>> [E]

@endverbatim

  Implemented as:
  - [1] #pfs_memory_alloc_v1(),
        #pfs_memory_realloc_v1(),
        #pfs_memory_free_v1().
  - [1+] are overflows that can happen during [1a],
        implemented with @c carry_memory_stat_delta()
  - [2] #pfs_delete_thread_v1(), #aggregate_thread_memory()
  - [3] @c PFS_account::aggregate_memory()
  - [4] @c PFS_host::aggregate_memory()
  - [A] MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME,
        @c table_mems_by_thread_by_event_name::make_row()
  - [B] MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME,
        @c table_mems_by_account_by_event_name::make_row()
  - [C] MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME,
        @c table_mems_by_user_by_event_name::make_row()
  - [D] MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME,
        @c table_mems_by_host_by_event_name::make_row()
  - [E] MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME,
        @c table_mems_global_by_event_name::make_row()

@section IMPL_ERROR Implementation for error instruments

  For errors, there are no tables that contains individual event data.

  For errors, the tables that contains aggregated data are:
  - EVENTS_ERRORS_SUMMARY_BY_ACCOUNT_BY_ERROR
  - EVENTS_ERRORS_SUMMARY_BY_HOST_BY_ERROR
  - EVENTS_ERRORS_SUMMARY_BY_THREAD_BY_ERROR
  - EVENTS_ERRORS_SUMMARY_BY_USER_BY_ERROR
  - EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR

@verbatim
  error_event(T, S)
   |
   | [1]
   |
1a |-> pfs_thread(T).event_name(S)            =====>> [A], [B], [C], [D], [E]
   |    |
   |    | [2]
   |    |
   | 2a |-> pfs_account(U, H).event_name(S)   =====>> [B], [C], [D], [E]
   |    .    |
   |    .    | [3-RESET]
   |    .    |
   | 2b .....+-> pfs_user(U).event_name(S)    =====>> [C]
   |    .    |
   | 2c .....+-> pfs_host(H).event_name(S)    =====>> [D], [E]
   |    .    .    |
   |    .    .    | [4-RESET]
   | 2d .    .    |
1b |----+----+----+-> global.event_name(S)    =====>> [E]

@endverbatim

  Implemented as:
  - [1] #pfs_log_error_v1()
  - [2] #pfs_delete_thread_v1(), #aggregate_thread_errors()
  - [3] @c PFS_account::aggregate_errors()
  - [4] @c PFS_host::aggregate_errors()
  - [A] EVENTS_ERRORS_SUMMARY_BY_THREAD_BY_ERROR,
        @c table_ees_by_thread_by_error::make_row()
  - [B] EVENTS_ERRORS_SUMMARY_BY_ACCOUNT_BY_ERROR,
        @c table_ees_by_account_by_error::make_row()
  - [C] EVENTS_ERRORS_SUMMARY_BY_USER_BY_ERROR,
        @c table_ees_by_user_by_error::make_row()
  - [D] EVENTS_ERRORS_SUMMARY_BY_HOST_BY_ERROR,
        @c table_ees_by_host_by_error::make_row()
  - [E] EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR,
        @c table_ees_global_by_error::make_row()
*/
/* clang-format on */

/**
  @defgroup performance_schema Performance Schema
  The performance schema component.
  For details, see @ref PAGE_PFS

  @defgroup performance_schema_implementation Performance Schema Implementation
  @ingroup performance_schema

  @defgroup performance_schema_tables Performance Schema Tables
  @ingroup performance_schema_implementation
*/

thread_local PFS_thread *THR_PFS = nullptr;
thread_local PFS_table_context *THR_PFS_contexts[THR_PFS_NUM_KEYS];

static inline PFS_thread *my_thread_get_THR_PFS() { return THR_PFS; }

static inline void my_thread_set_THR_PFS(PFS_thread *pfs) { THR_PFS = pfs; }

/**
  Conversion map from PSI_mutex_operation to enum_operation_type.
  Indexed by enum PSI_mutex_operation.
*/
static enum_operation_type mutex_operation_map[] = {OPERATION_TYPE_LOCK,
                                                    OPERATION_TYPE_TRYLOCK};

/**
  Conversion map from PSI_rwlock_operation to enum_operation_type.
  Indexed by enum PSI_rwlock_operation.
*/
static enum_operation_type rwlock_operation_map[] = {
    OPERATION_TYPE_READLOCK,
    OPERATION_TYPE_WRITELOCK,
    OPERATION_TYPE_TRYREADLOCK,
    OPERATION_TYPE_TRYWRITELOCK,

    OPERATION_TYPE_SHAREDLOCK,
    OPERATION_TYPE_SHAREDEXCLUSIVELOCK,
    OPERATION_TYPE_EXCLUSIVELOCK,
    OPERATION_TYPE_TRYSHAREDLOCK,
    OPERATION_TYPE_TRYSHAREDEXCLUSIVELOCK,
    OPERATION_TYPE_TRYEXCLUSIVELOCK,
};

/**
  Conversion map from PSI_cond_operation to enum_operation_type.
  Indexed by enum PSI_cond_operation.
*/
static enum_operation_type cond_operation_map[] = {OPERATION_TYPE_WAIT,
                                                   OPERATION_TYPE_TIMEDWAIT};

/**
  Conversion map from PSI_file_operation to enum_operation_type.
  Indexed by enum PSI_file_operation.
*/
static enum_operation_type file_operation_map[] = {
    OPERATION_TYPE_FILECREATE, OPERATION_TYPE_FILECREATETMP,
    OPERATION_TYPE_FILEOPEN,   OPERATION_TYPE_FILESTREAMOPEN,
    OPERATION_TYPE_FILECLOSE,  OPERATION_TYPE_FILESTREAMCLOSE,
    OPERATION_TYPE_FILEREAD,   OPERATION_TYPE_FILEWRITE,
    OPERATION_TYPE_FILESEEK,   OPERATION_TYPE_FILETELL,
    OPERATION_TYPE_FILEFLUSH,  OPERATION_TYPE_FILESTAT,
    OPERATION_TYPE_FILEFSTAT,  OPERATION_TYPE_FILECHSIZE,
    OPERATION_TYPE_FILEDELETE, OPERATION_TYPE_FILERENAME,
    OPERATION_TYPE_FILESYNC};

/**
  Conversion map from PSI_table_operation to enum_operation_type.
  Indexed by enum PSI_table_io_operation.
*/
static enum_operation_type table_io_operation_map[] = {
    OPERATION_TYPE_TABLE_FETCH, OPERATION_TYPE_TABLE_WRITE_ROW,
    OPERATION_TYPE_TABLE_UPDATE_ROW, OPERATION_TYPE_TABLE_DELETE_ROW};

/**
  Conversion map from enum PFS_TL_LOCK_TYPE to enum_operation_type.
  Indexed by enum PFS_TL_LOCK_TYPE.
*/
static enum_operation_type table_lock_operation_map[] = {
    OPERATION_TYPE_TL_READ_NORMAL,             /* PFS_TL_READ */
    OPERATION_TYPE_TL_READ_WITH_SHARED_LOCKS,  /* PFS_TL_READ_WITH_SHARED_LOCKS
                                                */
    OPERATION_TYPE_TL_READ_HIGH_PRIORITY,      /* PFS_TL_READ_HIGH_PRIORITY */
    OPERATION_TYPE_TL_READ_NO_INSERTS,         /* PFS_TL_READ_NO_INSERT */
    OPERATION_TYPE_TL_WRITE_ALLOW_WRITE,       /* PFS_TL_WRITE_ALLOW_WRITE */
    OPERATION_TYPE_TL_WRITE_CONCURRENT_INSERT, /* PFS_TL_WRITE_CONCURRENT_INSERT
                                                */
    OPERATION_TYPE_TL_WRITE_LOW_PRIORITY,      /* PFS_TL_WRITE_LOW_PRIORITY */
    OPERATION_TYPE_TL_WRITE_NORMAL,            /* PFS_TL_WRITE */
    OPERATION_TYPE_TL_READ_EXTERNAL,           /* PFS_TL_READ_EXTERNAL */
    OPERATION_TYPE_TL_WRITE_EXTERNAL           /* PFS_TL_WRITE_EXTERNAL */
};

/**
  Conversion map from PSI_socket_operation to enum_operation_type.
  Indexed by enum PSI_socket_operation.
*/
static enum_operation_type socket_operation_map[] = {
    OPERATION_TYPE_SOCKETCREATE,  OPERATION_TYPE_SOCKETCONNECT,
    OPERATION_TYPE_SOCKETBIND,    OPERATION_TYPE_SOCKETCLOSE,
    OPERATION_TYPE_SOCKETSEND,    OPERATION_TYPE_SOCKETRECV,
    OPERATION_TYPE_SOCKETSENDTO,  OPERATION_TYPE_SOCKETRECVFROM,
    OPERATION_TYPE_SOCKETSENDMSG, OPERATION_TYPE_SOCKETRECVMSG,
    OPERATION_TYPE_SOCKETSEEK,    OPERATION_TYPE_SOCKETOPT,
    OPERATION_TYPE_SOCKETSTAT,    OPERATION_TYPE_SOCKETSHUTDOWN,
    OPERATION_TYPE_SOCKETSELECT};

/**
  Build the prefix name of a class of instruments in a category.
  For example, this function builds the string 'wait/sync/mutex/sql/' from
  a prefix 'wait/sync/mutex' and a category 'sql'.
  This prefix is used later to build each instrument name, such as
  'wait/sync/mutex/sql/LOCK_open'.
  @param prefix               Prefix for this class of instruments
  @param category             Category name
  @param [out] output         Buffer of length PFS_MAX_INFO_NAME_LENGTH.
  @param [out] output_length  Length of the resulting output string.
  @return 0 for success, non zero for errors
*/
static int build_prefix(const LEX_STRING *prefix, const char *category,
                        char *output, size_t *output_length) {
  size_t len = strlen(category);
  char *out_ptr = output;
  size_t prefix_length = prefix->length;

  if (unlikely((prefix_length + len + 2) >= PFS_MAX_FULL_PREFIX_NAME_LENGTH)) {
    pfs_print_error("build_prefix: prefix+category is too long <%s> <%s>\n",
                    prefix->str, category);
    return 1;
  }

  if (unlikely(strchr(category, '/') != NULL)) {
    pfs_print_error("build_prefix: invalid category <%s>\n", category);
    return 1;
  }

  /* output = prefix + '/' + category + '/' */
  memcpy(out_ptr, prefix->str, prefix_length);
  out_ptr += prefix_length;
  if (len > 0) {
    *out_ptr = '/';
    out_ptr++;
    memcpy(out_ptr, category, len);
    out_ptr += len;
    *out_ptr = '/';
    out_ptr++;
  }
  *output_length = int(out_ptr - output);

  return 0;
}

#define REGISTER_BODY_V1(KEY_T, PREFIX, REGISTER_FUNC)                         \
  KEY_T key;                                                                   \
  char formatted_name[PFS_MAX_INFO_NAME_LENGTH];                               \
  size_t prefix_length;                                                        \
  size_t len;                                                                  \
  size_t full_length;                                                          \
                                                                               \
  DBUG_ASSERT(category != NULL);                                               \
  DBUG_ASSERT(info != NULL);                                                   \
  if (unlikely(                                                                \
          build_prefix(&PREFIX, category, formatted_name, &prefix_length)) ||  \
      !pfs_initialized) {                                                      \
    for (; count > 0; count--, info++) *(info->m_key) = 0;                     \
    return;                                                                    \
  }                                                                            \
                                                                               \
  for (; count > 0; count--, info++) {                                         \
    DBUG_ASSERT(info->m_key != NULL);                                          \
    DBUG_ASSERT(info->m_name != NULL);                                         \
    len = strlen(info->m_name);                                                \
    full_length = prefix_length + len;                                         \
    if (likely(full_length <= PFS_MAX_INFO_NAME_LENGTH)) {                     \
      memcpy(formatted_name + prefix_length, info->m_name, len);               \
      key = REGISTER_FUNC(formatted_name, (uint)full_length, info);            \
    } else {                                                                   \
      pfs_print_error("REGISTER_BODY_V1: name too long <%s> <%s>\n", category, \
                      info->m_name);                                           \
      key = 0;                                                                 \
    }                                                                          \
                                                                               \
    *(info->m_key) = key;                                                      \
  }                                                                            \
  return

/* Use C linkage for the interface functions. */

/**
  Implementation of the mutex instrumentation interface.
  @sa PSI_v1::register_mutex.
*/
void pfs_register_mutex_v1(const char *category, PSI_mutex_info_v1 *info,
                           int count) {
  REGISTER_BODY_V1(PSI_mutex_key, mutex_instrument_prefix,
                   register_mutex_class);
}

/**
  Implementation of the rwlock instrumentation interface.
  @sa PSI_v1::register_rwlock.
*/
void pfs_register_rwlock_v1(const char *category, PSI_rwlock_info_v1 *info,
                            int count) {
  PSI_rwlock_key key;
  char rw_formatted_name[PFS_MAX_INFO_NAME_LENGTH];
  char sx_formatted_name[PFS_MAX_INFO_NAME_LENGTH];
  size_t rw_prefix_length;
  size_t sx_prefix_length;
  size_t len;
  size_t full_length;

  DBUG_ASSERT(category != NULL);
  DBUG_ASSERT(info != NULL);
  if (build_prefix(&rwlock_instrument_prefix, category, rw_formatted_name,
                   &rw_prefix_length) ||
      build_prefix(&sxlock_instrument_prefix, category, sx_formatted_name,
                   &sx_prefix_length) ||
      !pfs_initialized) {
    for (; count > 0; count--, info++) {
      *(info->m_key) = 0;
    }
    return;
  }

  for (; count > 0; count--, info++) {
    DBUG_ASSERT(info->m_key != NULL);
    DBUG_ASSERT(info->m_name != NULL);
    len = strlen(info->m_name);

    if (info->m_flags & PSI_FLAG_RWLOCK_SX) {
      full_length = sx_prefix_length + len;
      if (likely(full_length <= PFS_MAX_INFO_NAME_LENGTH)) {
        memcpy(sx_formatted_name + sx_prefix_length, info->m_name, len);
        key = register_rwlock_class(sx_formatted_name, (uint)full_length, info);
      } else {
        pfs_print_error(
            "pfs_register_rwlock_v1: (sx) name too long <%s> <%s>\n", category,
            info->m_name);
        key = 0;
      }
    } else {
      full_length = rw_prefix_length + len;
      if (likely(full_length <= PFS_MAX_INFO_NAME_LENGTH)) {
        memcpy(rw_formatted_name + rw_prefix_length, info->m_name, len);
        key = register_rwlock_class(rw_formatted_name, (uint)full_length, info);
      } else {
        pfs_print_error(
            "pfs_register_rwlock_v1: (rw) name too long <%s> <%s>\n", category,
            info->m_name);
        key = 0;
      }
    }

    *(info->m_key) = key;
  }
  return;
}

/**
  Implementation of the cond instrumentation interface.
  @sa PSI_v1::register_cond.
*/
void pfs_register_cond_v1(const char *category, PSI_cond_info_v1 *info,
                          int count) {
  REGISTER_BODY_V1(PSI_cond_key, cond_instrument_prefix, register_cond_class);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::register_thread.
*/
void pfs_register_thread_v1(const char *category, PSI_thread_info_v1 *info,
                            int count) {
  REGISTER_BODY_V1(PSI_thread_key, thread_instrument_prefix,
                   register_thread_class);
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::register_file.
*/
void pfs_register_file_v1(const char *category, PSI_file_info_v1 *info,
                          int count) {
  REGISTER_BODY_V1(PSI_file_key, file_instrument_prefix, register_file_class);
}

void pfs_register_stage_v1(const char *category, PSI_stage_info_v1 **info_array,
                           int count) {
  char formatted_name[PFS_MAX_INFO_NAME_LENGTH];
  size_t prefix_length;
  size_t len;
  size_t full_length;
  PSI_stage_info_v1 *info;

  DBUG_ASSERT(category != NULL);
  DBUG_ASSERT(info_array != NULL);
  if (unlikely(build_prefix(&stage_instrument_prefix, category, formatted_name,
                            &prefix_length)) ||
      !pfs_initialized) {
    for (; count > 0; count--, info_array++) {
      (*info_array)->m_key = 0;
    }
    return;
  }

  for (; count > 0; count--, info_array++) {
    info = *info_array;
    DBUG_ASSERT(info != NULL);
    DBUG_ASSERT(info->m_name != NULL);
    len = strlen(info->m_name);
    full_length = prefix_length + len;
    if (likely(full_length <= PFS_MAX_INFO_NAME_LENGTH)) {
      memcpy(formatted_name + prefix_length, info->m_name, len);
      info->m_key = register_stage_class(formatted_name, (uint)prefix_length,
                                         (uint)full_length, info);
    } else {
      pfs_print_error("register_stage_v1: name too long <%s> <%s>\n", category,
                      info->m_name);
      info->m_key = 0;
    }
  }
  return;
}

void pfs_register_statement_v1(const char *category,
                               PSI_statement_info_v1 *info, int count) {
  char formatted_name[PFS_MAX_INFO_NAME_LENGTH];
  size_t prefix_length;
  size_t len;
  size_t full_length;

  DBUG_ASSERT(category != NULL);
  DBUG_ASSERT(info != NULL);
  if (unlikely(build_prefix(&statement_instrument_prefix, category,
                            formatted_name, &prefix_length)) ||
      !pfs_initialized) {
    for (; count > 0; count--, info++) {
      info->m_key = 0;
    }
    return;
  }

  for (; count > 0; count--, info++) {
    DBUG_ASSERT(info->m_name != NULL);
    len = strlen(info->m_name);
    full_length = prefix_length + len;
    if (likely(full_length <= PFS_MAX_INFO_NAME_LENGTH)) {
      memcpy(formatted_name + prefix_length, info->m_name, len);
      info->m_key =
          register_statement_class(formatted_name, (uint)full_length, info);
    } else {
      pfs_print_error("register_statement_v1: name too long <%s>\n",
                      info->m_name);
      info->m_key = 0;
    }
  }
  return;
}

void pfs_register_socket_v1(const char *category, PSI_socket_info_v1 *info,
                            int count) {
  REGISTER_BODY_V1(PSI_socket_key, socket_instrument_prefix,
                   register_socket_class);
}

/**
  Implementation of the mutex instrumentation interface.
  @sa PSI_v1::init_mutex.
*/
PSI_mutex *pfs_init_mutex_v1(PSI_mutex_key key, const void *identity) {
  PFS_mutex_class *klass;
  PFS_mutex *pfs;
  klass = find_mutex_class(key);
  if (unlikely(klass == NULL)) {
    return NULL;
  }
  pfs = create_mutex(klass, identity);
  return reinterpret_cast<PSI_mutex *>(pfs);
}

/**
  Implementation of the mutex instrumentation interface.
  @sa PSI_v1::destroy_mutex.
*/
void pfs_destroy_mutex_v1(PSI_mutex *mutex) {
  PFS_mutex *pfs = reinterpret_cast<PFS_mutex *>(mutex);

  DBUG_ASSERT(pfs != NULL);

  destroy_mutex(pfs);
}

/**
  Implementation of the rwlock instrumentation interface.
  @sa PSI_v1::init_rwlock.
*/
PSI_rwlock *pfs_init_rwlock_v1(PSI_rwlock_key key, const void *identity) {
  PFS_rwlock_class *klass;
  PFS_rwlock *pfs;
  klass = find_rwlock_class(key);
  if (unlikely(klass == NULL)) {
    return NULL;
  }
  pfs = create_rwlock(klass, identity);
  return reinterpret_cast<PSI_rwlock *>(pfs);
}

/**
  Implementation of the rwlock instrumentation interface.
  @sa PSI_v1::destroy_rwlock.
*/
void pfs_destroy_rwlock_v1(PSI_rwlock *rwlock) {
  PFS_rwlock *pfs = reinterpret_cast<PFS_rwlock *>(rwlock);

  DBUG_ASSERT(pfs != NULL);

  destroy_rwlock(pfs);
}

/**
  Implementation of the cond instrumentation interface.
  @sa PSI_v1::init_cond.
*/
PSI_cond *pfs_init_cond_v1(PSI_cond_key key, const void *identity) {
  PFS_cond_class *klass;
  PFS_cond *pfs;
  klass = find_cond_class(key);
  if (unlikely(klass == NULL)) {
    return NULL;
  }
  pfs = create_cond(klass, identity);
  return reinterpret_cast<PSI_cond *>(pfs);
}

/**
  Implementation of the cond instrumentation interface.
  @sa PSI_v1::destroy_cond.
*/
void pfs_destroy_cond_v1(PSI_cond *cond) {
  PFS_cond *pfs = reinterpret_cast<PFS_cond *>(cond);

  DBUG_ASSERT(pfs != NULL);

  destroy_cond(pfs);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::get_table_share.
*/
PSI_table_share *pfs_get_table_share_v1(bool temporary, TABLE_SHARE *share) {
  /* Ignore temporary tables and views. */
  if (temporary || share->is_view) {
    return NULL;
  }
  /* An instrumented thread is required, for LF_PINS. */
  PFS_thread *pfs_thread = my_thread_get_THR_PFS();
  if (unlikely(pfs_thread == NULL)) {
    return NULL;
  }
  PFS_table_share *pfs_share;
  pfs_share = find_or_create_table_share(pfs_thread, temporary, share);
  return reinterpret_cast<PSI_table_share *>(pfs_share);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::release_table_share.
*/
void pfs_release_table_share_v1(PSI_table_share *share) {
  PFS_table_share *pfs = reinterpret_cast<PFS_table_share *>(share);

  if (unlikely(pfs == NULL)) {
    return;
  }

  release_table_share(pfs);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::drop_table_share.
*/
void pfs_drop_table_share_v1(bool temporary, const char *schema_name,
                             int schema_name_length, const char *table_name,
                             int table_name_length) {
  /* Ignore temporary tables. */
  if (temporary) {
    return;
  }
  PFS_thread *pfs_thread = my_thread_get_THR_PFS();
  if (unlikely(pfs_thread == NULL)) {
    return;
  }
  /* TODO: temporary tables */
  drop_table_share(pfs_thread, temporary, schema_name, schema_name_length,
                   table_name, table_name_length);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::open_table.
*/
PSI_table *pfs_open_table_v1(PSI_table_share *share, const void *identity) {
  PFS_table_share *pfs_table_share = reinterpret_cast<PFS_table_share *>(share);

  if (unlikely(pfs_table_share == NULL)) {
    return NULL;
  }

  /* This object is not to be instrumented. */
  if (!pfs_table_share->m_enabled) {
    return NULL;
  }

  /* This object is instrumented, but all table instruments are disabled. */
  if (!global_table_io_class.m_enabled && !global_table_lock_class.m_enabled) {
    return NULL;
  }

  /*
    When the performance schema is off, do not instrument anything.
    Table handles have short life cycle, instrumentation will happen
    again if needed during the next open().
  */
  if (!flag_global_instrumentation) {
    return NULL;
  }

  PFS_thread *thread = my_thread_get_THR_PFS();
  if (unlikely(thread == NULL)) {
    return NULL;
  }

  PFS_table *pfs_table = create_table(pfs_table_share, thread, identity);
  return reinterpret_cast<PSI_table *>(pfs_table);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::unbind_table.
*/
void pfs_unbind_table_v1(PSI_table *table) {
  PFS_table *pfs = reinterpret_cast<PFS_table *>(table);
  if (likely(pfs != NULL)) {
    pfs->m_thread_owner = NULL;
    pfs->m_owner_event_id = 0;
  }
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::rebind_table.
*/
PSI_table *pfs_rebind_table_v1(PSI_table_share *share, const void *identity,
                               PSI_table *table) {
  PFS_table *pfs = reinterpret_cast<PFS_table *>(table);
  if (likely(pfs != NULL)) {
    DBUG_ASSERT(pfs->m_thread_owner == NULL);

    if (unlikely(!pfs->m_share->m_enabled)) {
      destroy_table(pfs);
      return NULL;
    }

    if (unlikely(!global_table_io_class.m_enabled &&
                 !global_table_lock_class.m_enabled)) {
      destroy_table(pfs);
      return NULL;
    }

    if (unlikely(!flag_global_instrumentation)) {
      destroy_table(pfs);
      return NULL;
    }

    /* The table handle was already instrumented, reuse it for this thread. */
    PFS_thread *thread = my_thread_get_THR_PFS();
    pfs->m_thread_owner = thread;
    if (thread != NULL) {
      pfs->m_owner_event_id = thread->m_event_id;
    } else {
      pfs->m_owner_event_id = 0;
    }
    return table;
  }

  /* See open_table_v1() */

  PFS_table_share *pfs_table_share = reinterpret_cast<PFS_table_share *>(share);

  if (unlikely(pfs_table_share == NULL)) {
    return NULL;
  }

  if (!pfs_table_share->m_enabled) {
    return NULL;
  }

  if (!global_table_io_class.m_enabled && !global_table_lock_class.m_enabled) {
    return NULL;
  }

  if (!flag_global_instrumentation) {
    return NULL;
  }

  PFS_thread *thread = my_thread_get_THR_PFS();
  if (unlikely(thread == NULL)) {
    return NULL;
  }

  PFS_table *pfs_table = create_table(pfs_table_share, thread, identity);
  return reinterpret_cast<PSI_table *>(pfs_table);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::close_table.
*/
void pfs_close_table_v1(TABLE_SHARE *server_share, PSI_table *table) {
  PFS_table *pfs = reinterpret_cast<PFS_table *>(table);
  if (unlikely(pfs == NULL)) {
    return;
  }
  pfs->aggregate(server_share);
  destroy_table(pfs);
}

PSI_socket *pfs_init_socket_v1(PSI_socket_key key, const my_socket *fd,
                               const struct sockaddr *addr,
                               socklen_t addr_len) {
  PFS_socket_class *klass;
  PFS_socket *pfs;
  klass = find_socket_class(key);
  if (unlikely(klass == NULL)) {
    return NULL;
  }
  pfs = create_socket(klass, fd, addr, addr_len);
  return reinterpret_cast<PSI_socket *>(pfs);
}

void pfs_destroy_socket_v1(PSI_socket *socket) {
  PFS_socket *pfs = reinterpret_cast<PFS_socket *>(socket);

  DBUG_ASSERT(pfs != NULL);

  destroy_socket(pfs);
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::create_file.
*/
void pfs_create_file_v1(PSI_file_key key, const char *name, File file) {
  if (!flag_global_instrumentation) {
    return;
  }
  int index = (int)file;
  if (unlikely(index < 0)) {
    return;
  }
  PFS_file_class *klass = find_file_class(key);
  if (unlikely(klass == NULL)) {
    return;
  }
  if (!klass->m_enabled) {
    return;
  }

  /* A thread is needed for LF_PINS */
  PFS_thread *pfs_thread = my_thread_get_THR_PFS();
  if (unlikely(pfs_thread == NULL)) {
    return;
  }

  if (flag_thread_instrumentation && !pfs_thread->m_enabled) {
    return;
  }

  /*
    We want this check after pfs_thread->m_enabled,
    to avoid reporting false loss.
  */
  if (unlikely(index >= file_handle_max)) {
    file_handle_lost++;
    return;
  }

  uint len = (uint)strlen(name);
  PFS_file *pfs_file = find_or_create_file(pfs_thread, klass, name, len, true);

  file_handle_array[index] = pfs_file;
}

/**
  Arguments given from a parent to a child thread, packaged in one structure.
  This data is used when spawning a new instrumented thread.
  @sa pfs_spawn_thread.
*/
struct PFS_spawn_thread_arg {
  ulonglong m_thread_internal_id;
  char m_username[USERNAME_LENGTH];
  uint m_username_length;
  char m_hostname[HOSTNAME_LENGTH];
  uint m_hostname_length;

  PSI_thread_key m_child_key;
  const void *m_child_identity;
  void *(*m_user_start_routine)(void *);
  void *m_user_arg;
};

extern "C" {
static void *pfs_spawn_thread(void *arg) {
  PFS_spawn_thread_arg *typed_arg = (PFS_spawn_thread_arg *)arg;
  void *user_arg;
  void *(*user_start_routine)(void *);

  PFS_thread *pfs;

  /* First, attach instrumentation to this newly created pthread. */
  PFS_thread_class *klass = find_thread_class(typed_arg->m_child_key);
  if (likely(klass != NULL)) {
    pfs = create_thread(klass, typed_arg->m_child_identity, 0);
    if (likely(pfs != NULL)) {
      pfs->m_thread_os_id = my_thread_os_id();
      clear_thread_account(pfs);

      pfs->m_parent_thread_internal_id = typed_arg->m_thread_internal_id;

      memcpy(pfs->m_username, typed_arg->m_username, sizeof(pfs->m_username));
      pfs->m_username_length = typed_arg->m_username_length;

      memcpy(pfs->m_hostname, typed_arg->m_hostname, sizeof(pfs->m_hostname));
      pfs->m_hostname_length = typed_arg->m_hostname_length;

      set_thread_account(pfs);
    }
  } else {
    pfs = NULL;
  }
  my_thread_set_THR_PFS(pfs);

  pfs_notify_thread_create((PSI_thread *)pfs);
  /*
    Secondly, free the memory allocated in spawn_thread_v1().
    It is preferable to do this before invoking the user
    routine, to avoid memory leaks at shutdown, in case
    the server exits without waiting for this thread.
  */
  user_start_routine = typed_arg->m_user_start_routine;
  user_arg = typed_arg->m_user_arg;
  my_free(typed_arg);

  /* Then, execute the user code for this thread. */
  (*user_start_routine)(user_arg);

  return NULL;
}
}  // extern "C"

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::spawn_thread.
*/
int pfs_spawn_thread_v1(PSI_thread_key key, my_thread_handle *thread,
                        const my_thread_attr_t *attr,
                        void *(*start_routine)(void *), void *arg) {
  PFS_spawn_thread_arg *psi_arg;
  PFS_thread *parent;

  /* psi_arg can not be global, and can not be a local variable. */
  psi_arg = (PFS_spawn_thread_arg *)my_malloc(
      PSI_NOT_INSTRUMENTED, sizeof(PFS_spawn_thread_arg), MYF(MY_WME));
  if (unlikely(psi_arg == NULL)) {
    return EAGAIN;
  }

  psi_arg->m_child_key = key;
  psi_arg->m_child_identity = (arg ? arg : thread);
  psi_arg->m_user_start_routine = start_routine;
  psi_arg->m_user_arg = arg;

  parent = my_thread_get_THR_PFS();
  if (parent != NULL) {
    /*
      Make a copy of the parent attributes.
      This is required, because instrumentation for this thread (the parent)
      may be destroyed before the child thread instrumentation is created.
    */
    psi_arg->m_thread_internal_id = parent->m_thread_internal_id;

    memcpy(psi_arg->m_username, parent->m_username,
           sizeof(psi_arg->m_username));
    psi_arg->m_username_length = parent->m_username_length;

    memcpy(psi_arg->m_hostname, parent->m_hostname,
           sizeof(psi_arg->m_hostname));
    psi_arg->m_hostname_length = parent->m_hostname_length;
  } else {
    psi_arg->m_thread_internal_id = 0;
    psi_arg->m_username_length = 0;
    psi_arg->m_hostname_length = 0;
  }

  int result = my_thread_create(thread, attr, pfs_spawn_thread, psi_arg);
  if (unlikely(result != 0)) {
    my_free(psi_arg);
  }
  return result;
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::new_thread.
*/
PSI_thread *pfs_new_thread_v1(PSI_thread_key key, const void *identity,
                              ulonglong processlist_id) {
  PFS_thread *pfs;

  PFS_thread_class *klass = find_thread_class(key);
  if (likely(klass != NULL)) {
    pfs = create_thread(klass, identity, processlist_id);
  } else {
    pfs = NULL;
  }

  if (pfs) {
    pfs_notify_thread_create((PSI_thread *)pfs);
  }

  return reinterpret_cast<PSI_thread *>(pfs);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_id.
*/
void pfs_set_thread_id_v1(PSI_thread *thread, ulonglong processlist_id) {
  PFS_thread *pfs = reinterpret_cast<PFS_thread *>(thread);
  if (unlikely(pfs == NULL)) {
    return;
  }
  pfs->m_processlist_id = (ulong)processlist_id;
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_THD.
*/
void pfs_set_thread_THD_v1(PSI_thread *thread, THD *thd) {
  PFS_thread *pfs = reinterpret_cast<PFS_thread *>(thread);
  if (unlikely(pfs == NULL)) {
    return;
  }
  pfs->m_thd = thd;
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_os_thread_id.
*/
void pfs_set_thread_os_id_v1(PSI_thread *thread) {
  PFS_thread *pfs = reinterpret_cast<PFS_thread *>(thread);
  if (unlikely(pfs == NULL)) {
    return;
  }
  pfs->m_thread_os_id = my_thread_os_id();
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::get_thread_id.
*/
PSI_thread *pfs_get_thread_v1(void) {
  PFS_thread *pfs = my_thread_get_THR_PFS();
  return reinterpret_cast<PSI_thread *>(pfs);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_user.
*/
void pfs_set_thread_user_v1(const char *user, int user_len) {
  pfs_dirty_state dirty_state;
  PFS_thread *pfs = my_thread_get_THR_PFS();

  DBUG_ASSERT((user != NULL) || (user_len == 0));
  DBUG_ASSERT(user_len >= 0);
  DBUG_ASSERT((uint)user_len <= sizeof(pfs->m_username));

  if (unlikely(pfs == NULL)) {
    return;
  }

  aggregate_thread(pfs, pfs->m_account, pfs->m_user, pfs->m_host);

  pfs->m_session_lock.allocated_to_dirty(&dirty_state);

  clear_thread_account(pfs);

  if (user_len > 0) {
    memcpy(pfs->m_username, user, user_len);
  }
  pfs->m_username_length = user_len;

  set_thread_account(pfs);

  bool enabled;
  bool history;
  if (pfs->m_account != NULL) {
    enabled = pfs->m_account->m_enabled;
    history = pfs->m_account->m_history;
  } else {
    if ((pfs->m_username_length > 0) && (pfs->m_hostname_length > 0)) {
      lookup_setup_actor(pfs, pfs->m_username, pfs->m_username_length,
                         pfs->m_hostname, pfs->m_hostname_length, &enabled,
                         &history);
    } else {
      /* There is no setting for background threads */
      enabled = true;
      history = true;
    }
  }
  pfs->set_enabled(enabled);
  pfs->set_history(history);

  pfs->m_session_lock.dirty_to_allocated(&dirty_state);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_account.
*/
void pfs_set_thread_account_v1(const char *user, int user_len, const char *host,
                               int host_len) {
  pfs_dirty_state dirty_state;
  PFS_thread *pfs = my_thread_get_THR_PFS();

  DBUG_ASSERT((user != NULL) || (user_len == 0));
  DBUG_ASSERT(user_len >= 0);
  DBUG_ASSERT((uint)user_len <= sizeof(pfs->m_username));
  DBUG_ASSERT((host != NULL) || (host_len == 0));
  DBUG_ASSERT(host_len >= 0);

  host_len = min<size_t>(host_len, sizeof(pfs->m_hostname));

  if (unlikely(pfs == NULL)) {
    return;
  }

  pfs->m_session_lock.allocated_to_dirty(&dirty_state);

  clear_thread_account(pfs);

  if (host_len > 0) {
    memcpy(pfs->m_hostname, host, host_len);
  }
  pfs->m_hostname_length = host_len;

  if (user_len > 0) {
    memcpy(pfs->m_username, user, user_len);
  }
  pfs->m_username_length = user_len;

  set_thread_account(pfs);

  bool enabled;
  bool history;
  if (pfs->m_account != NULL) {
    enabled = pfs->m_account->m_enabled;
    history = pfs->m_account->m_history;
  } else {
    if ((pfs->m_username_length > 0) && (pfs->m_hostname_length > 0)) {
      lookup_setup_actor(pfs, pfs->m_username, pfs->m_username_length,
                         pfs->m_hostname, pfs->m_hostname_length, &enabled,
                         &history);
    } else {
      /* There is no setting for background threads */
      enabled = true;
      history = true;
    }
  }
  pfs->set_enabled(enabled);
  pfs->set_history(history);

  pfs->m_session_lock.dirty_to_allocated(&dirty_state);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_db.
*/
void pfs_set_thread_db_v1(const char *db, int db_len) {
  PFS_thread *pfs = my_thread_get_THR_PFS();

  DBUG_ASSERT((db != NULL) || (db_len == 0));
  DBUG_ASSERT(db_len >= 0);
  DBUG_ASSERT((uint)db_len <= sizeof(pfs->m_dbname));

  if (likely(pfs != NULL)) {
    pfs_dirty_state dirty_state;
    pfs->m_stmt_lock.allocated_to_dirty(&dirty_state);
    if (db_len > 0) {
      memcpy(pfs->m_dbname, db, db_len);
    }
    pfs->m_dbname_length = db_len;
    pfs->m_stmt_lock.dirty_to_allocated(&dirty_state);
  }
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_command.
*/
void pfs_set_thread_command_v1(int command) {
  PFS_thread *pfs = my_thread_get_THR_PFS();

  DBUG_ASSERT(command >= 0);
  DBUG_ASSERT(command <= (int)COM_END);

  if (likely(pfs != NULL)) {
    pfs->m_command = command;
  }
}

/**
Implementation of the thread instrumentation interface.
@sa PSI_v1::set_thread_connection_type.
*/
void pfs_set_connection_type_v1(opaque_vio_type conn_type) {
  PFS_thread *pfs = my_thread_get_THR_PFS();

  DBUG_ASSERT(conn_type >= FIRST_VIO_TYPE);
  DBUG_ASSERT(conn_type <= LAST_VIO_TYPE);

  if (likely(pfs != NULL)) {
    pfs->m_connection_type = static_cast<enum_vio_type>(conn_type);
  }
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_start_time.
*/
void pfs_set_thread_start_time_v1(time_t start_time) {
  PFS_thread *pfs = my_thread_get_THR_PFS();

  if (likely(pfs != NULL)) {
    pfs->m_start_time = start_time;
  }
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_state.
*/
void pfs_set_thread_state_v1(const char *) { /* DEPRECATED. */
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_info.
*/
void pfs_set_thread_info_v1(const char *info, uint info_len) {
  pfs_dirty_state dirty_state;
  PFS_thread *pfs = my_thread_get_THR_PFS();

  DBUG_ASSERT((info != NULL) || (info_len == 0));

  if (likely(pfs != NULL)) {
    if ((info != NULL) && (info_len > 0)) {
      if (info_len > sizeof(pfs->m_processlist_info)) {
        info_len = sizeof(pfs->m_processlist_info);
      }

      pfs->m_stmt_lock.allocated_to_dirty(&dirty_state);
      memcpy(pfs->m_processlist_info, info, info_len);
      pfs->m_processlist_info_length = info_len;
      pfs->m_stmt_lock.dirty_to_allocated(&dirty_state);
    } else {
      pfs->m_stmt_lock.allocated_to_dirty(&dirty_state);
      pfs->m_processlist_info_length = 0;
      pfs->m_stmt_lock.dirty_to_allocated(&dirty_state);
    }
  }
}

/**
  Set the resource group name for a given thread.
  @param pfs Thread instrumentation
  @param group_name Group name
  @param group_name_len Length of group_name
  @param user_data  Optional pointer to user-defined data
  @return 0 if successful, 1 otherwise
*/
int set_thread_resource_group(PFS_thread *pfs, const char *group_name,
                              int group_name_len, void *user_data) {
  int result = 0;
  pfs_dirty_state dirty_state;

  if (unlikely(pfs == NULL || group_name_len <= 0)) {
    return 1;
  }

  if ((size_t)group_name_len > sizeof(pfs->m_groupname)) {
    return 1;
  }

  pfs->m_session_lock.allocated_to_dirty(&dirty_state);

  memcpy(pfs->m_groupname, group_name, group_name_len);

  pfs->m_groupname_length = group_name_len;
  pfs->m_user_data = user_data;

  pfs->m_session_lock.dirty_to_allocated(&dirty_state);
  return result;
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_resource_group
*/
int pfs_set_thread_resource_group_v1(const char *group_name, int group_name_len,
                                     void *user_data) {
  PFS_thread *pfs = my_thread_get_THR_PFS();
  return set_thread_resource_group(pfs, group_name, group_name_len, user_data);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_resource_group_by_id
*/
int pfs_set_thread_resource_group_by_id_v1(PSI_thread *thread,
                                           ulonglong thread_id,
                                           const char *group_name,
                                           int group_name_len,
                                           void *user_data) {
  PFS_thread *pfs = reinterpret_cast<PFS_thread *>(thread);
  if (pfs == NULL) {
    pfs = find_thread(thread_id);
  }
  return set_thread_resource_group(pfs, group_name, group_name_len, user_data);
}

/**
  Get the system and session attributes for a given PFS_thread.
  @param pfs thread instrumentation
  @param current_thread true if pfs refers to the current thread
  @param thread_attrs thread attribute structure
  @return 0 if successful, non-zero otherwise
*/
int get_thread_attributes(PFS_thread *pfs, bool current_thread,
                          PSI_thread_attrs *thread_attrs)

{
  int result = 0;
  pfs_optimistic_state lock;
  pfs_optimistic_state session_lock;

  DBUG_ASSERT(thread_attrs != NULL);

  static_assert(PSI_NAME_LEN == NAME_LEN, "");
  static_assert(PSI_USERNAME_LENGTH == USERNAME_LENGTH, "");
  static_assert(PSI_HOSTNAME_LENGTH == HOSTNAME_LENGTH, "");

  if (unlikely(pfs == NULL)) {
    return 1;
  }

  if (!current_thread) {
    /* Protect this reader against a thread delete. */
    pfs->m_lock.begin_optimistic_lock(&lock);
    /* Protect this reader against writing on session attributes */
    pfs->m_session_lock.begin_optimistic_lock(&session_lock);
  }

  thread_attrs->m_thread_internal_id = pfs->m_thread_internal_id;
  thread_attrs->m_processlist_id = pfs->m_processlist_id;
  thread_attrs->m_thread_os_id = pfs->m_thread_os_id;
  thread_attrs->m_user_data = pfs->m_user_data;
  thread_attrs->m_system_thread = pfs->m_system_thread;

  DBUG_ASSERT(pfs->m_sock_addr_len <= sizeof(PSI_thread_attrs::m_sock_addr));
  thread_attrs->m_sock_addr_length = pfs->m_sock_addr_len;
  if (thread_attrs->m_sock_addr_length > 0) {
    memcpy(&thread_attrs->m_sock_addr, &pfs->m_sock_addr, pfs->m_sock_addr_len);
  }

  DBUG_ASSERT(pfs->m_username_length <= sizeof(PSI_thread_attrs::m_username));
  thread_attrs->m_username_length = pfs->m_username_length;
  if (pfs->m_username_length > 0) {
    memcpy(thread_attrs->m_username, pfs->m_username, pfs->m_username_length);
  }

  DBUG_ASSERT(pfs->m_hostname_length <= sizeof(PSI_thread_attrs::m_hostname));
  thread_attrs->m_hostname_length = pfs->m_hostname_length;
  if (pfs->m_hostname_length > 0) {
    memcpy(thread_attrs->m_hostname, pfs->m_hostname, pfs->m_hostname_length);
  }

  DBUG_ASSERT(pfs->m_groupname_length <= sizeof(PSI_thread_attrs::m_groupname));
  thread_attrs->m_groupname_length = pfs->m_groupname_length;
  if (pfs->m_groupname_length > 0) {
    memcpy(thread_attrs->m_groupname, pfs->m_groupname,
           pfs->m_groupname_length);
  }

  if (!current_thread) {
    if (!pfs->m_session_lock.end_optimistic_lock(&session_lock)) {
      result = 1;
    }

    if (!pfs->m_lock.end_optimistic_lock(&lock)) {
      result = 1;
    }
  }

  return result;
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::get_thread_system_attrs.
*/
int pfs_get_thread_system_attrs_v1(PSI_thread_attrs *thread_attrs) {
  PFS_thread *pfs = my_thread_get_THR_PFS();
  return get_thread_attributes(pfs, true, thread_attrs);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::get_thread_system_attrs_by_id.
*/
int pfs_get_thread_system_attrs_by_id_v1(PSI_thread *thread,
                                         ulonglong thread_id,
                                         PSI_thread_attrs *thread_attrs) {
  PFS_thread *pfs = reinterpret_cast<PFS_thread *>(thread);
  if (pfs == NULL) {
    pfs = find_thread(thread_id);
  }
  return get_thread_attributes(pfs, false, thread_attrs);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::register_notification.
*/
int pfs_register_notification_v1(const PSI_notification *callbacks,
                                 bool with_ref_count) {
  return pfs_register_notification(callbacks, with_ref_count);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::unregister_notification.
*/
int pfs_unregister_notification_v1(int handle) {
  return pfs_unregister_notification(handle);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::notify_session_connect.
*/
void pfs_notify_session_connect_v1(PSI_thread *thread MY_ATTRIBUTE((unused))) {
  pfs_notify_session_connect(thread);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::notify_session_disconnect.
*/
void pfs_notify_session_disconnect_v1(
    PSI_thread *thread MY_ATTRIBUTE((unused))) {
  pfs_notify_session_disconnect(thread);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::notify_session_change_user.
*/
void pfs_notify_session_change_user_v1(
    PSI_thread *thread MY_ATTRIBUTE((unused))) {
  pfs_notify_session_change_user(thread);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread.
*/
void pfs_set_thread_v1(PSI_thread *thread) {
  PFS_thread *pfs = reinterpret_cast<PFS_thread *>(thread);
  my_thread_set_THR_PFS(pfs);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::delete_current_thread.
*/
void pfs_delete_current_thread_v1(void) {
  PFS_thread *thread = my_thread_get_THR_PFS();
  if (thread != NULL) {
    aggregate_thread(thread, thread->m_account, thread->m_user, thread->m_host);
    my_thread_set_THR_PFS(NULL);
    pfs_notify_thread_destroy((PSI_thread *)thread);
    destroy_thread(thread);
  }
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::delete_thread.
*/
void pfs_delete_thread_v1(PSI_thread *thread) {
  PFS_thread *pfs = reinterpret_cast<PFS_thread *>(thread);

  if (pfs != NULL) {
    aggregate_thread(pfs, pfs->m_account, pfs->m_user, pfs->m_host);
    pfs_notify_thread_destroy(thread);
    destroy_thread(pfs);
  }
}

/**
  Implementation of the mutex instrumentation interface.
  @sa PSI_v1::start_mutex_wait.
*/
PSI_mutex_locker *pfs_start_mutex_wait_v1(PSI_mutex_locker_state *state,
                                          PSI_mutex *mutex,
                                          PSI_mutex_operation op,
                                          const char *src_file, uint src_line) {
  PFS_mutex *pfs_mutex = reinterpret_cast<PFS_mutex *>(mutex);
  DBUG_ASSERT((int)op >= 0);
  DBUG_ASSERT((uint)op < array_elements(mutex_operation_map));
  DBUG_ASSERT(state != NULL);

  DBUG_ASSERT(pfs_mutex != NULL);
  DBUG_ASSERT(pfs_mutex->m_class != NULL);

  if (!pfs_mutex->m_enabled) {
    return NULL;
  }

  uint flags;
  ulonglong timer_start = 0;

  if (flag_thread_instrumentation) {
    PFS_thread *pfs_thread = my_thread_get_THR_PFS();
    if (unlikely(pfs_thread == NULL)) {
      return NULL;
    }
    if (!pfs_thread->m_enabled) {
      return NULL;
    }
    state->m_thread = reinterpret_cast<PSI_thread *>(pfs_thread);
    flags = STATE_FLAG_THREAD;

    if (pfs_mutex->m_timed) {
      timer_start = get_wait_timer();
      state->m_timer_start = timer_start;
      flags |= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current) {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   &pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE])) {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait = pfs_thread->m_events_waits_current;
      state->m_wait = wait;
      flags |= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event = wait - 1;
      wait->m_event_type = EVENT_TYPE_WAIT;
      wait->m_nesting_event_id = parent_event->m_event_id;
      wait->m_nesting_event_type = parent_event->m_event_type;

      wait->m_thread_internal_id = pfs_thread->m_thread_internal_id;
      wait->m_class = pfs_mutex->m_class;
      wait->m_timer_start = timer_start;
      wait->m_timer_end = 0;
      wait->m_object_instance_addr = pfs_mutex->m_identity;
      wait->m_event_id = pfs_thread->m_event_id++;
      wait->m_end_event_id = 0;
      wait->m_operation = mutex_operation_map[(int)op];
      wait->m_source_file = src_file;
      wait->m_source_line = src_line;
      wait->m_wait_class = WAIT_CLASS_MUTEX;

      pfs_thread->m_events_waits_current++;
    }
  } else {
    if (pfs_mutex->m_timed) {
      timer_start = get_wait_timer();
      state->m_timer_start = timer_start;
      flags = STATE_FLAG_TIMED;
      state->m_thread = NULL;
    } else {
      /*
        Complete shortcut.
      */
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
      pfs_mutex->m_mutex_stat.m_wait_stat.aggregate_counted();
      return NULL;
    }
  }

  state->m_flags = flags;
  state->m_mutex = mutex;
  return reinterpret_cast<PSI_mutex_locker *>(state);
}

/**
  Implementation of the rwlock instrumentation interface.
  @sa PSI_v1::start_rwlock_rdwait
  @sa PSI_v1::start_rwlock_wrwait
*/
static PSI_rwlock_locker *pfs_start_rwlock_wait_v1(
    PSI_rwlock_locker_state *state, PSI_rwlock *rwlock, PSI_rwlock_operation op,
    const char *src_file, uint src_line) {
  PFS_rwlock *pfs_rwlock = reinterpret_cast<PFS_rwlock *>(rwlock);
  DBUG_ASSERT(static_cast<int>(op) >= 0);
  DBUG_ASSERT(static_cast<uint>(op) < array_elements(rwlock_operation_map));
  DBUG_ASSERT(state != NULL);
  DBUG_ASSERT(pfs_rwlock != NULL);
  DBUG_ASSERT(pfs_rwlock->m_class != NULL);

  /* Operations supported for READ WRITE LOCK */

  DBUG_ASSERT(pfs_rwlock->m_class->is_shared_exclusive() ||
              (op == PSI_RWLOCK_READLOCK) || (op == PSI_RWLOCK_WRITELOCK) ||
              (op == PSI_RWLOCK_TRYREADLOCK) ||
              (op == PSI_RWLOCK_TRYWRITELOCK));

  /* Operations supported for SHARED EXCLUSIVE LOCK */

  DBUG_ASSERT(
      !pfs_rwlock->m_class->is_shared_exclusive() ||
      (op == PSI_RWLOCK_SHAREDLOCK) || (op == PSI_RWLOCK_SHAREDEXCLUSIVELOCK) ||
      (op == PSI_RWLOCK_EXCLUSIVELOCK) || (op == PSI_RWLOCK_TRYSHAREDLOCK) ||
      (op == PSI_RWLOCK_TRYSHAREDEXCLUSIVELOCK) ||
      (op == PSI_RWLOCK_TRYEXCLUSIVELOCK));

  if (!pfs_rwlock->m_enabled) {
    return NULL;
  }

  uint flags;
  ulonglong timer_start = 0;

  if (flag_thread_instrumentation) {
    PFS_thread *pfs_thread = my_thread_get_THR_PFS();
    if (unlikely(pfs_thread == NULL)) {
      return NULL;
    }
    if (!pfs_thread->m_enabled) {
      return NULL;
    }
    state->m_thread = reinterpret_cast<PSI_thread *>(pfs_thread);
    flags = STATE_FLAG_THREAD;

    if (pfs_rwlock->m_timed) {
      timer_start = get_wait_timer();
      state->m_timer_start = timer_start;
      flags |= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current) {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   &pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE])) {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait = pfs_thread->m_events_waits_current;
      state->m_wait = wait;
      flags |= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event = wait - 1;
      wait->m_event_type = EVENT_TYPE_WAIT;
      wait->m_nesting_event_id = parent_event->m_event_id;
      wait->m_nesting_event_type = parent_event->m_event_type;

      wait->m_thread_internal_id = pfs_thread->m_thread_internal_id;
      wait->m_class = pfs_rwlock->m_class;
      wait->m_timer_start = timer_start;
      wait->m_timer_end = 0;
      wait->m_object_instance_addr = pfs_rwlock->m_identity;
      wait->m_event_id = pfs_thread->m_event_id++;
      wait->m_end_event_id = 0;
      wait->m_operation = rwlock_operation_map[static_cast<int>(op)];
      wait->m_source_file = src_file;
      wait->m_source_line = src_line;
      wait->m_wait_class = WAIT_CLASS_RWLOCK;

      pfs_thread->m_events_waits_current++;
    }
  } else {
    if (pfs_rwlock->m_timed) {
      timer_start = get_wait_timer();
      state->m_timer_start = timer_start;
      flags = STATE_FLAG_TIMED;
      state->m_thread = NULL;
    } else {
      /*
        Complete shortcut.
      */
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
      pfs_rwlock->m_rwlock_stat.m_wait_stat.aggregate_counted();
      return NULL;
    }
  }

  state->m_flags = flags;
  state->m_rwlock = rwlock;
  state->m_operation = op;
  return reinterpret_cast<PSI_rwlock_locker *>(state);
}

PSI_rwlock_locker *pfs_start_rwlock_rdwait_v1(PSI_rwlock_locker_state *state,
                                              PSI_rwlock *rwlock,
                                              PSI_rwlock_operation op,
                                              const char *src_file,
                                              uint src_line) {
  DBUG_ASSERT((op == PSI_RWLOCK_READLOCK) || (op == PSI_RWLOCK_TRYREADLOCK) ||
              (op == PSI_RWLOCK_SHAREDLOCK) ||
              (op == PSI_RWLOCK_TRYSHAREDLOCK));

  return pfs_start_rwlock_wait_v1(state, rwlock, op, src_file, src_line);
}

PSI_rwlock_locker *pfs_start_rwlock_wrwait_v1(PSI_rwlock_locker_state *state,
                                              PSI_rwlock *rwlock,
                                              PSI_rwlock_operation op,
                                              const char *src_file,
                                              uint src_line) {
  DBUG_ASSERT((op == PSI_RWLOCK_WRITELOCK) || (op == PSI_RWLOCK_TRYWRITELOCK) ||
              (op == PSI_RWLOCK_SHAREDEXCLUSIVELOCK) ||
              (op == PSI_RWLOCK_TRYSHAREDEXCLUSIVELOCK) ||
              (op == PSI_RWLOCK_EXCLUSIVELOCK) ||
              (op == PSI_RWLOCK_TRYEXCLUSIVELOCK));

  return pfs_start_rwlock_wait_v1(state, rwlock, op, src_file, src_line);
}

/**
  Implementation of the cond instrumentation interface.
  @sa PSI_v1::start_cond_wait.
*/
PSI_cond_locker *pfs_start_cond_wait_v1(PSI_cond_locker_state *state,
                                        PSI_cond *cond, PSI_mutex *mutex,
                                        PSI_cond_operation op,
                                        const char *src_file, uint src_line) {
  /*
    Note about the unused PSI_mutex *mutex parameter:
    In the pthread library, a call to pthread_cond_wait()
    causes an unlock() + lock() on the mutex associated with the condition.
    This mutex operation is not instrumented, so the mutex will still
    appear as locked when a thread is waiting on a condition.
    This has no impact now, as unlock_mutex() is not recording events.
    When unlock_mutex() is implemented by later work logs,
    this parameter here will be used to adjust the mutex state,
    in start_cond_wait_v1() and end_cond_wait_v1().
  */
  PFS_cond *pfs_cond = reinterpret_cast<PFS_cond *>(cond);
  DBUG_ASSERT(static_cast<int>(op) >= 0);
  DBUG_ASSERT(static_cast<uint>(op) < array_elements(cond_operation_map));
  DBUG_ASSERT(state != NULL);
  DBUG_ASSERT(pfs_cond != NULL);
  DBUG_ASSERT(pfs_cond->m_class != NULL);

  if (!pfs_cond->m_enabled) {
    return NULL;
  }

  uint flags;
  ulonglong timer_start = 0;

  if (flag_thread_instrumentation) {
    PFS_thread *pfs_thread = my_thread_get_THR_PFS();
    if (unlikely(pfs_thread == NULL)) {
      return NULL;
    }
    if (!pfs_thread->m_enabled) {
      return NULL;
    }
    state->m_thread = reinterpret_cast<PSI_thread *>(pfs_thread);
    flags = STATE_FLAG_THREAD;

    if (pfs_cond->m_timed) {
      timer_start = get_wait_timer();
      state->m_timer_start = timer_start;
      flags |= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current) {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   &pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE])) {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait = pfs_thread->m_events_waits_current;
      state->m_wait = wait;
      flags |= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event = wait - 1;
      wait->m_event_type = EVENT_TYPE_WAIT;
      wait->m_nesting_event_id = parent_event->m_event_id;
      wait->m_nesting_event_type = parent_event->m_event_type;

      wait->m_thread_internal_id = pfs_thread->m_thread_internal_id;
      wait->m_class = pfs_cond->m_class;
      wait->m_timer_start = timer_start;
      wait->m_timer_end = 0;
      wait->m_object_instance_addr = pfs_cond->m_identity;
      wait->m_event_id = pfs_thread->m_event_id++;
      wait->m_end_event_id = 0;
      wait->m_operation = cond_operation_map[static_cast<int>(op)];
      wait->m_source_file = src_file;
      wait->m_source_line = src_line;
      wait->m_wait_class = WAIT_CLASS_COND;

      pfs_thread->m_events_waits_current++;
    }
  } else {
    if (pfs_cond->m_timed) {
      timer_start = get_wait_timer();
      state->m_timer_start = timer_start;
      flags = STATE_FLAG_TIMED;
    } else {
      /*
        Complete shortcut.
      */
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
      pfs_cond->m_cond_stat.m_wait_stat.aggregate_counted();
      return NULL;
    }
  }

  state->m_flags = flags;
  state->m_cond = cond;
  state->m_mutex = mutex;
  return reinterpret_cast<PSI_cond_locker *>(state);
}

static inline PFS_TL_LOCK_TYPE lock_flags_to_lock_type(uint flags) {
  enum thr_lock_type value = static_cast<enum thr_lock_type>(flags);

  switch (value) {
    case TL_READ:
      return PFS_TL_READ;
    case TL_READ_WITH_SHARED_LOCKS:
      return PFS_TL_READ_WITH_SHARED_LOCKS;
    case TL_READ_HIGH_PRIORITY:
      return PFS_TL_READ_HIGH_PRIORITY;
    case TL_READ_NO_INSERT:
      return PFS_TL_READ_NO_INSERT;
    case TL_WRITE_ALLOW_WRITE:
      return PFS_TL_WRITE_ALLOW_WRITE;
    case TL_WRITE_CONCURRENT_INSERT:
      return PFS_TL_WRITE_CONCURRENT_INSERT;
    case TL_WRITE_LOW_PRIORITY:
      return PFS_TL_WRITE_LOW_PRIORITY;
    case TL_WRITE:
      return PFS_TL_WRITE;

    case TL_WRITE_ONLY:
    case TL_IGNORE:
    case TL_UNLOCK:
    case TL_READ_DEFAULT:
    case TL_WRITE_DEFAULT:
    case TL_WRITE_CONCURRENT_DEFAULT:
    default:
      DBUG_ASSERT(false);
  }

  /* Dead code */
  return PFS_TL_READ;
}

static inline PFS_TL_LOCK_TYPE external_lock_flags_to_lock_type(uint flags) {
  DBUG_ASSERT(flags == F_RDLCK || flags == F_WRLCK);
  return (flags == F_RDLCK ? PFS_TL_READ_EXTERNAL : PFS_TL_WRITE_EXTERNAL);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::start_table_io_wait_v1
*/
PSI_table_locker *pfs_start_table_io_wait_v1(PSI_table_locker_state *state,
                                             PSI_table *table,
                                             PSI_table_io_operation op,
                                             uint index, const char *src_file,
                                             uint src_line) {
  DBUG_ASSERT(static_cast<int>(op) >= 0);
  DBUG_ASSERT(static_cast<uint>(op) < array_elements(table_io_operation_map));
  DBUG_ASSERT(state != NULL);
  PFS_table *pfs_table = reinterpret_cast<PFS_table *>(table);
  DBUG_ASSERT(pfs_table != NULL);
  DBUG_ASSERT(pfs_table->m_share != NULL);

  if (!pfs_table->m_io_enabled) {
    return NULL;
  }

  PFS_thread *pfs_thread = my_thread_get_THR_PFS();

  uint flags;
  ulonglong timer_start = 0;

  if (flag_thread_instrumentation) {
    if (pfs_thread == NULL) {
      return NULL;
    }
    if (!pfs_thread->m_enabled) {
      return NULL;
    }
    state->m_thread = reinterpret_cast<PSI_thread *>(pfs_thread);
    flags = STATE_FLAG_THREAD;

    if (pfs_table->m_io_timed) {
      timer_start = get_wait_timer();
      state->m_timer_start = timer_start;
      flags |= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current) {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   &pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE])) {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait = pfs_thread->m_events_waits_current;
      state->m_wait = wait;
      flags |= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event = wait - 1;
      wait->m_event_type = EVENT_TYPE_WAIT;
      wait->m_nesting_event_id = parent_event->m_event_id;
      wait->m_nesting_event_type = parent_event->m_event_type;

      PFS_table_share *share = pfs_table->m_share;
      wait->m_thread_internal_id = pfs_thread->m_thread_internal_id;
      wait->m_class = &global_table_io_class;
      wait->m_timer_start = timer_start;
      wait->m_timer_end = 0;
      wait->m_object_instance_addr = pfs_table->m_identity;
      wait->m_event_id = pfs_thread->m_event_id++;
      wait->m_end_event_id = 0;
      wait->m_operation = table_io_operation_map[static_cast<int>(op)];
      wait->m_flags = 0;
      wait->m_object_type = share->get_object_type();
      wait->m_weak_table_share = share;
      wait->m_weak_version = share->get_version();
      wait->m_index = index;
      wait->m_source_file = src_file;
      wait->m_source_line = src_line;
      wait->m_wait_class = WAIT_CLASS_TABLE;

      pfs_thread->m_events_waits_current++;
    }
  } else {
    if (pfs_table->m_io_timed) {
      timer_start = get_wait_timer();
      state->m_timer_start = timer_start;
      flags = STATE_FLAG_TIMED;
    } else {
      /* TODO: consider a shortcut here */
      flags = 0;
    }
  }

  state->m_flags = flags;
  state->m_table = table;
  state->m_io_operation = op;
  state->m_index = index;
  return reinterpret_cast<PSI_table_locker *>(state);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::start_table_lock_wait.
*/
PSI_table_locker *pfs_start_table_lock_wait_v1(PSI_table_locker_state *state,
                                               PSI_table *table,
                                               PSI_table_lock_operation op,
                                               ulong op_flags,
                                               const char *src_file,
                                               uint src_line) {
  DBUG_ASSERT(state != NULL);
  DBUG_ASSERT((op == PSI_TABLE_LOCK) || (op == PSI_TABLE_EXTERNAL_LOCK));

  PFS_table *pfs_table = reinterpret_cast<PFS_table *>(table);

  DBUG_ASSERT(pfs_table != NULL);
  DBUG_ASSERT(pfs_table->m_share != NULL);

  if (!pfs_table->m_lock_enabled) {
    return NULL;
  }

  PFS_thread *pfs_thread = my_thread_get_THR_PFS();

  PFS_TL_LOCK_TYPE lock_type;

  switch (op) {
    case PSI_TABLE_LOCK:
      lock_type = lock_flags_to_lock_type(op_flags);
      pfs_table->m_internal_lock = lock_type;
      break;
    case PSI_TABLE_EXTERNAL_LOCK:
      /*
        See the handler::external_lock() API design,
        there is no handler::external_unlock().
      */
      if (op_flags == F_UNLCK) {
        pfs_table->m_external_lock = PFS_TL_NONE;
        return NULL;
      }
      lock_type = external_lock_flags_to_lock_type(op_flags);
      pfs_table->m_external_lock = lock_type;
      break;
    default:
      lock_type = PFS_TL_READ;
      DBUG_ASSERT(false);
  }

  DBUG_ASSERT((uint)lock_type < array_elements(table_lock_operation_map));

  uint flags;
  ulonglong timer_start = 0;

  if (flag_thread_instrumentation) {
    if (pfs_thread == NULL) {
      return NULL;
    }
    if (!pfs_thread->m_enabled) {
      return NULL;
    }
    state->m_thread = reinterpret_cast<PSI_thread *>(pfs_thread);
    flags = STATE_FLAG_THREAD;

    if (pfs_table->m_lock_timed) {
      timer_start = get_wait_timer();
      state->m_timer_start = timer_start;
      flags |= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current) {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   &pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE])) {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait = pfs_thread->m_events_waits_current;
      state->m_wait = wait;
      flags |= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event = wait - 1;
      wait->m_event_type = EVENT_TYPE_WAIT;
      wait->m_nesting_event_id = parent_event->m_event_id;
      wait->m_nesting_event_type = parent_event->m_event_type;

      PFS_table_share *share = pfs_table->m_share;
      wait->m_thread_internal_id = pfs_thread->m_thread_internal_id;
      wait->m_class = &global_table_lock_class;
      wait->m_timer_start = timer_start;
      wait->m_timer_end = 0;
      wait->m_object_instance_addr = pfs_table->m_identity;
      wait->m_event_id = pfs_thread->m_event_id++;
      wait->m_end_event_id = 0;
      wait->m_operation = table_lock_operation_map[lock_type];
      wait->m_flags = 0;
      wait->m_object_type = share->get_object_type();
      wait->m_weak_table_share = share;
      wait->m_weak_version = share->get_version();
      wait->m_index = 0;
      wait->m_source_file = src_file;
      wait->m_source_line = src_line;
      wait->m_wait_class = WAIT_CLASS_TABLE;

      pfs_thread->m_events_waits_current++;
    }
  } else {
    if (pfs_table->m_lock_timed) {
      timer_start = get_wait_timer();
      state->m_timer_start = timer_start;
      flags = STATE_FLAG_TIMED;
    } else {
      /* TODO: consider a shortcut here */
      flags = 0;
    }
  }

  state->m_flags = flags;
  state->m_table = table;
  state->m_index = lock_type;
  return reinterpret_cast<PSI_table_locker *>(state);
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::get_thread_file_name_locker.
*/
PSI_file_locker *pfs_get_thread_file_name_locker_v1(
    PSI_file_locker_state *state, PSI_file_key key, PSI_file_operation op,
    const char *name, const void *) {
  DBUG_ASSERT(static_cast<int>(op) >= 0);
  DBUG_ASSERT(static_cast<uint>(op) < array_elements(file_operation_map));
  DBUG_ASSERT(state != NULL);

  if (!flag_global_instrumentation) {
    return NULL;
  }
  PFS_file_class *klass = find_file_class(key);
  if (unlikely(klass == NULL)) {
    return NULL;
  }
  if (!klass->m_enabled) {
    return NULL;
  }

  /* Needed for the LF_HASH */
  PFS_thread *pfs_thread = my_thread_get_THR_PFS();
  if (unlikely(pfs_thread == NULL)) {
    return NULL;
  }

  if (flag_thread_instrumentation && !pfs_thread->m_enabled) {
    return NULL;
  }

  uint flags;

  state->m_thread = reinterpret_cast<PSI_thread *>(pfs_thread);
  flags = STATE_FLAG_THREAD;

  if (klass->m_timed) {
    flags |= STATE_FLAG_TIMED;
  }

  if (flag_events_waits_current) {
    if (unlikely(pfs_thread->m_events_waits_current >=
                 &pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE])) {
      locker_lost++;
      return NULL;
    }
    PFS_events_waits *wait = pfs_thread->m_events_waits_current;
    state->m_wait = wait;
    flags |= STATE_FLAG_EVENT;

    PFS_events_waits *parent_event = wait - 1;
    wait->m_event_type = EVENT_TYPE_WAIT;
    wait->m_nesting_event_id = parent_event->m_event_id;
    wait->m_nesting_event_type = parent_event->m_event_type;

    wait->m_thread_internal_id = pfs_thread->m_thread_internal_id;
    wait->m_class = klass;
    wait->m_timer_start = 0;
    wait->m_timer_end = 0;
    wait->m_object_instance_addr = NULL;
    wait->m_weak_file = NULL;
    wait->m_weak_version = 0;
    wait->m_event_id = pfs_thread->m_event_id++;
    wait->m_end_event_id = 0;
    wait->m_operation = file_operation_map[static_cast<int>(op)];
    wait->m_wait_class = WAIT_CLASS_FILE;

    pfs_thread->m_events_waits_current++;
  }

  state->m_flags = flags;
  state->m_file = NULL;
  state->m_name = name;
  state->m_class = klass;
  state->m_operation = op;
  return reinterpret_cast<PSI_file_locker *>(state);
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::get_thread_file_stream_locker.
*/
PSI_file_locker *pfs_get_thread_file_stream_locker_v1(
    PSI_file_locker_state *state, PSI_file *file, PSI_file_operation op) {
  PFS_file *pfs_file = reinterpret_cast<PFS_file *>(file);
  DBUG_ASSERT(static_cast<int>(op) >= 0);
  DBUG_ASSERT(static_cast<uint>(op) < array_elements(file_operation_map));
  DBUG_ASSERT(state != NULL);

  if (unlikely(pfs_file == NULL)) {
    return NULL;
  }
  DBUG_ASSERT(pfs_file->m_class != NULL);
  PFS_file_class *klass = pfs_file->m_class;

  if (!pfs_file->m_enabled) {
    return NULL;
  }

  /* Needed for the LF_HASH */
  PFS_thread *pfs_thread = my_thread_get_THR_PFS();
  if (unlikely(pfs_thread == NULL)) {
    return NULL;
  }

  uint flags;

  /* Always populated */
  state->m_thread = reinterpret_cast<PSI_thread *>(pfs_thread);

  if (flag_thread_instrumentation) {
    if (!pfs_thread->m_enabled) {
      return NULL;
    }
    flags = STATE_FLAG_THREAD;

    if (pfs_file->m_timed) {
      flags |= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current) {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   &pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE])) {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait = pfs_thread->m_events_waits_current;
      state->m_wait = wait;
      flags |= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event = wait - 1;
      wait->m_event_type = EVENT_TYPE_WAIT;
      wait->m_nesting_event_id = parent_event->m_event_id;
      wait->m_nesting_event_type = parent_event->m_event_type;

      wait->m_thread_internal_id = pfs_thread->m_thread_internal_id;
      wait->m_class = klass;
      wait->m_timer_start = 0;
      wait->m_timer_end = 0;
      wait->m_object_instance_addr = pfs_file;
      wait->m_weak_file = pfs_file;
      wait->m_weak_version = pfs_file->get_version();
      wait->m_event_id = pfs_thread->m_event_id++;
      wait->m_end_event_id = 0;
      wait->m_operation = file_operation_map[static_cast<int>(op)];
      wait->m_wait_class = WAIT_CLASS_FILE;

      pfs_thread->m_events_waits_current++;
    }
  } else {
    if (pfs_file->m_timed) {
      flags = STATE_FLAG_TIMED;
    } else {
      /* TODO: consider a shortcut. */
      flags = 0;
    }
  }

  state->m_flags = flags;
  state->m_file = reinterpret_cast<PSI_file *>(pfs_file);
  state->m_operation = op;
  state->m_name = NULL;
  state->m_class = klass;
  return reinterpret_cast<PSI_file_locker *>(state);
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::get_thread_file_descriptor_locker.
*/
PSI_file_locker *pfs_get_thread_file_descriptor_locker_v1(
    PSI_file_locker_state *state, File file, PSI_file_operation op) {
  int index = static_cast<int>(file);
  DBUG_ASSERT(static_cast<int>(op) >= 0);
  DBUG_ASSERT(static_cast<uint>(op) < array_elements(file_operation_map));
  DBUG_ASSERT(state != NULL);

  if (unlikely((index < 0) || (index >= file_handle_max))) {
    return NULL;
  }

  PFS_file *pfs_file = file_handle_array[index];
  if (unlikely(pfs_file == NULL)) {
    return NULL;
  }

  /*
    We are about to close a file by descriptor number,
    and the calling code still holds the descriptor.
    Cleanup the file descriptor <--> file instrument association.
    Remove the instrumentation *before* the close to avoid race
    conditions with another thread opening a file
    (that could be given the same descriptor).
  */
  if (op == PSI_FILE_CLOSE) {
    file_handle_array[index] = NULL;
  }

  if (!pfs_file->m_enabled) {
    return NULL;
  }

  DBUG_ASSERT(pfs_file->m_class != NULL);
  PFS_file_class *klass = pfs_file->m_class;

  /* Needed for the LF_HASH */
  PFS_thread *pfs_thread = my_thread_get_THR_PFS();
  if (unlikely(pfs_thread == NULL)) {
    return NULL;
  }

  uint flags;

  /* Always populated */
  state->m_thread = reinterpret_cast<PSI_thread *>(pfs_thread);

  if (flag_thread_instrumentation) {
    if (!pfs_thread->m_enabled) {
      return NULL;
    }
    flags = STATE_FLAG_THREAD;

    if (pfs_file->m_timed) {
      flags |= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current) {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   &pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE])) {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait = pfs_thread->m_events_waits_current;
      state->m_wait = wait;
      flags |= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event = wait - 1;
      wait->m_event_type = EVENT_TYPE_WAIT;
      wait->m_nesting_event_id = parent_event->m_event_id;
      wait->m_nesting_event_type = parent_event->m_event_type;

      wait->m_thread_internal_id = pfs_thread->m_thread_internal_id;
      wait->m_class = klass;
      wait->m_timer_start = 0;
      wait->m_timer_end = 0;
      wait->m_object_instance_addr = pfs_file;
      wait->m_weak_file = pfs_file;
      wait->m_weak_version = pfs_file->get_version();
      wait->m_event_id = pfs_thread->m_event_id++;
      wait->m_end_event_id = 0;
      wait->m_operation = file_operation_map[static_cast<int>(op)];
      wait->m_wait_class = WAIT_CLASS_FILE;

      pfs_thread->m_events_waits_current++;
    }
  } else {
    if (pfs_file->m_timed) {
      flags = STATE_FLAG_TIMED;
    } else {
      /* TODO: consider a shortcut. */
      flags = 0;
    }
  }

  state->m_flags = flags;
  state->m_file = reinterpret_cast<PSI_file *>(pfs_file);
  state->m_operation = op;
  state->m_name = NULL;
  state->m_class = klass;
  return reinterpret_cast<PSI_file_locker *>(state);
}

/** Socket locker */

PSI_socket_locker *pfs_start_socket_wait_v1(PSI_socket_locker_state *state,
                                            PSI_socket *socket,
                                            PSI_socket_operation op,
                                            size_t count, const char *src_file,
                                            uint src_line) {
  DBUG_ASSERT(static_cast<int>(op) >= 0);
  DBUG_ASSERT(static_cast<uint>(op) < array_elements(socket_operation_map));
  DBUG_ASSERT(state != NULL);
  PFS_socket *pfs_socket = reinterpret_cast<PFS_socket *>(socket);

  DBUG_ASSERT(pfs_socket != NULL);
  DBUG_ASSERT(pfs_socket->m_class != NULL);

  if (!pfs_socket->m_enabled || pfs_socket->m_idle) {
    return NULL;
  }

  uint flags = 0;
  ulonglong timer_start = 0;

  if (flag_thread_instrumentation) {
    /*
       Do not use pfs_socket->m_thread_owner here,
       as different threads may use concurrently the same socket,
       for example during a KILL.
    */
    PFS_thread *pfs_thread = my_thread_get_THR_PFS();

    if (unlikely(pfs_thread == NULL)) {
      return NULL;
    }

    if (!pfs_thread->m_enabled) {
      return NULL;
    }

    state->m_thread = reinterpret_cast<PSI_thread *>(pfs_thread);
    flags = STATE_FLAG_THREAD;

    if (pfs_socket->m_timed) {
      timer_start = get_wait_timer();
      state->m_timer_start = timer_start;
      flags |= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current) {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   &pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE])) {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait = pfs_thread->m_events_waits_current;
      state->m_wait = wait;
      flags |= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event = wait - 1;
      wait->m_event_type = EVENT_TYPE_WAIT;
      wait->m_nesting_event_id = parent_event->m_event_id;
      wait->m_nesting_event_type = parent_event->m_event_type;
      wait->m_thread_internal_id = pfs_thread->m_thread_internal_id;
      wait->m_class = pfs_socket->m_class;
      wait->m_timer_start = timer_start;
      wait->m_timer_end = 0;
      wait->m_object_instance_addr = pfs_socket->m_identity;
      wait->m_weak_socket = pfs_socket;
      wait->m_weak_version = pfs_socket->get_version();
      wait->m_event_id = pfs_thread->m_event_id++;
      wait->m_end_event_id = 0;
      wait->m_operation = socket_operation_map[static_cast<int>(op)];
      wait->m_source_file = src_file;
      wait->m_source_line = src_line;
      wait->m_number_of_bytes = count;
      wait->m_wait_class = WAIT_CLASS_SOCKET;

      pfs_thread->m_events_waits_current++;
    }
  } else {
    if (pfs_socket->m_timed) {
      timer_start = get_wait_timer();
      state->m_timer_start = timer_start;
      flags = STATE_FLAG_TIMED;
    } else {
      /*
        Even if timing is disabled, end_socket_wait() still needs a locker to
        capture the number of bytes sent or received by the socket operation.
        For operations that do not have a byte count, then just increment the
        event counter and return a NULL locker.
      */
      switch (op) {
        case PSI_SOCKET_CONNECT:
        case PSI_SOCKET_CREATE:
        case PSI_SOCKET_BIND:
        case PSI_SOCKET_SEEK:
        case PSI_SOCKET_OPT:
        case PSI_SOCKET_STAT:
        case PSI_SOCKET_SHUTDOWN:
        case PSI_SOCKET_CLOSE:
        case PSI_SOCKET_SELECT:
          pfs_socket->m_socket_stat.m_io_stat.m_misc.aggregate_counted();
          return NULL;
        default:
          break;
      }
    }
  }

  state->m_flags = flags;
  state->m_socket = socket;
  state->m_operation = op;
  return reinterpret_cast<PSI_socket_locker *>(state);
}

/**
  Implementation of the mutex instrumentation interface.
  @sa PSI_v1::unlock_mutex.
*/
void pfs_unlock_mutex_v1(PSI_mutex *mutex) {
  PFS_mutex *pfs_mutex = reinterpret_cast<PFS_mutex *>(mutex);

  DBUG_ASSERT(pfs_mutex != NULL);

  /*
    Note that this code is still protected by the instrumented mutex,
    and therefore is thread safe. See inline_mysql_mutex_unlock().
  */

  /* Always update the instrumented state */
  pfs_mutex->m_owner = NULL;
  pfs_mutex->m_last_locked = 0;

#ifdef LATER_WL2333
  /*
    See WL#2333: SHOW ENGINE ... LOCK STATUS.
    PFS_mutex::m_lock_stat is not exposed in user visible tables
    currently, so there is no point spending time computing it.
  */
  if (!pfs_mutex->m_enabled) {
    return;
  }

  if (!pfs_mutex->m_timed) {
    return;
  }

  ulonglong locked_time;
  locked_time = get_wait_timer() - pfs_mutex->m_last_locked;
  pfs_mutex->m_mutex_stat.m_lock_stat.aggregate_value(locked_time);
#endif
}

/**
  Implementation of the rwlock instrumentation interface.
  @sa PSI_v1::unlock_rwlock.
*/
void pfs_unlock_rwlock_v1(PSI_rwlock *rwlock) {
  PFS_rwlock *pfs_rwlock = reinterpret_cast<PFS_rwlock *>(rwlock);
  DBUG_ASSERT(pfs_rwlock != NULL);
  DBUG_ASSERT(pfs_rwlock == sanitize_rwlock(pfs_rwlock));
  DBUG_ASSERT(pfs_rwlock->m_class != NULL);
  DBUG_ASSERT(pfs_rwlock->m_lock.is_populated());

  bool last_writer = false;
  bool last_reader = false;

  /*
    Note that this code is still protected by the instrumented rwlock,
    and therefore is:
    - thread safe for write locks
    - almost thread safe for read locks (pfs_rwlock->m_readers is unsafe).
    See inline_mysql_rwlock_unlock()
  */

  /* Always update the instrumented state */
  if (pfs_rwlock->m_writer != NULL) {
    /* Nominal case, a writer is unlocking. */
    last_writer = true;
    pfs_rwlock->m_writer = NULL;
    /* Reset the readers stats, they could be off */
    pfs_rwlock->m_readers = 0;
  } else if (likely(pfs_rwlock->m_readers > 0)) {
    /* Nominal case, a reader is unlocking. */
    if (--(pfs_rwlock->m_readers) == 0) {
      last_reader = true;
    }
  } else {
    /*
      Edge case, we have no writer and no readers,
      on an unlock event.
      This is possible for:
      - partial instrumentation
      - instrumentation disabled at runtime,
        see when get_thread_rwlock_locker_v1() returns NULL
      No further action is taken here, the next
      write lock will put the statistics is a valid state.
    */
  }

#ifdef LATER_WL2333
  /* See WL#2333: SHOW ENGINE ... LOCK STATUS. */

  if (!pfs_rwlock->m_enabled) {
    return;
  }

  if (!pfs_rwlock->m_timed) {
    return;
  }

  ulonglong locked_time;
  if (last_writer) {
    locked_time = get_wait_timer() - pfs_rwlock->m_last_written;
    pfs_rwlock->m_rwlock_stat.m_write_lock_stat.aggregate_value(locked_time);
  } else if (last_reader) {
    locked_time = get_wait_timer() - pfs_rwlock->m_last_read;
    pfs_rwlock->m_rwlock_stat.m_read_lock_stat.aggregate_value(locked_time);
  }
#else
  (void)last_reader;
  (void)last_writer;
#endif
}

/**
  Implementation of the cond instrumentation interface.
  @sa PSI_v1::signal_cond.
*/
void pfs_signal_cond_v1(PSI_cond *cond MY_ATTRIBUTE((unused))) {
#ifdef PFS_LATER
  PFS_cond *pfs_cond = reinterpret_cast<PFS_cond *>(cond);

  DBUG_ASSERT(pfs_cond != NULL);

  pfs_cond->m_cond_stat.m_signal_count++;
#endif
}

/**
  Implementation of the cond instrumentation interface.
  @sa PSI_v1::broadcast_cond.
*/
void pfs_broadcast_cond_v1(PSI_cond *cond MY_ATTRIBUTE((unused))) {
#ifdef PFS_LATER
  PFS_cond *pfs_cond = reinterpret_cast<PFS_cond *>(cond);

  DBUG_ASSERT(pfs_cond != NULL);

  pfs_cond->m_cond_stat.m_broadcast_count++;
#endif
}

/**
  Implementation of the idle instrumentation interface.
  @sa PSI_v1::start_idle_wait.
*/
PSI_idle_locker *pfs_start_idle_wait_v1(PSI_idle_locker_state *state,
                                        const char *src_file, uint src_line) {
  DBUG_ASSERT(state != NULL);

  if (!flag_global_instrumentation) {
    return NULL;
  }

  if (!global_idle_class.m_enabled) {
    return NULL;
  }

  uint flags = 0;
  ulonglong timer_start = 0;

  if (flag_thread_instrumentation) {
    PFS_thread *pfs_thread = my_thread_get_THR_PFS();
    if (unlikely(pfs_thread == NULL)) {
      return NULL;
    }
    if (!pfs_thread->m_enabled) {
      return NULL;
    }
    state->m_thread = reinterpret_cast<PSI_thread *>(pfs_thread);
    flags = STATE_FLAG_THREAD;

    DBUG_ASSERT(pfs_thread->m_events_statements_count == 0);

    if (global_idle_class.m_timed) {
      timer_start = get_idle_timer();
      state->m_timer_start = timer_start;
      flags |= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current) {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   &pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE])) {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait = pfs_thread->m_events_waits_current;
      state->m_wait = wait;
      flags |= STATE_FLAG_EVENT;

      wait->m_event_type = EVENT_TYPE_WAIT;
      /*
        IDLE events are waits, but by definition we know that
        such waits happen outside of any STAGE and STATEMENT,
        so they have no parents.
      */
      wait->m_nesting_event_id = 0;
      /* no need to set wait->m_nesting_event_type */

      wait->m_thread_internal_id = pfs_thread->m_thread_internal_id;
      wait->m_class = &global_idle_class;
      wait->m_timer_start = timer_start;
      wait->m_timer_end = 0;
      wait->m_event_id = pfs_thread->m_event_id++;
      wait->m_end_event_id = 0;
      wait->m_operation = OPERATION_TYPE_IDLE;
      wait->m_source_file = src_file;
      wait->m_source_line = src_line;
      wait->m_wait_class = WAIT_CLASS_IDLE;

      pfs_thread->m_events_waits_current++;
    }
  } else {
    if (global_idle_class.m_timed) {
      timer_start = get_idle_timer();
      state->m_timer_start = timer_start;
      flags = STATE_FLAG_TIMED;
    }
  }

  state->m_flags = flags;
  return reinterpret_cast<PSI_idle_locker *>(state);
}

/**
  Implementation of the mutex instrumentation interface.
  @sa PSI_v1::end_idle_wait.
*/
void pfs_end_idle_wait_v1(PSI_idle_locker *locker) {
  PSI_idle_locker_state *state =
      reinterpret_cast<PSI_idle_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);
  ulonglong timer_end = 0;
  ulonglong wait_time = 0;

  uint flags = state->m_flags;

  if (flags & STATE_FLAG_TIMED) {
    timer_end = get_idle_timer();
    wait_time = timer_end - state->m_timer_start;
  }

  if (flags & STATE_FLAG_THREAD) {
    PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);
    PFS_single_stat *event_name_array;
    event_name_array = thread->write_instr_class_waits_stats();

    if (flags & STATE_FLAG_TIMED) {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      event_name_array[GLOBAL_IDLE_EVENT_INDEX].aggregate_value(wait_time);
    } else {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[GLOBAL_IDLE_EVENT_INDEX].aggregate_counted();
    }

    if (flags & STATE_FLAG_EVENT) {
      PFS_events_waits *wait =
          reinterpret_cast<PFS_events_waits *>(state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end = timer_end;
      wait->m_end_event_id = thread->m_event_id;
      if (thread->m_flag_events_waits_history) {
        insert_events_waits_history(thread, wait);
      }
      if (thread->m_flag_events_waits_history_long) {
        insert_events_waits_history_long(wait);
      }
      thread->m_events_waits_current--;

      DBUG_ASSERT(wait == thread->m_events_waits_current);
    }
  }

  if (flags & STATE_FLAG_TIMED) {
    /* Aggregate to EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME (timed) */
    global_idle_stat.aggregate_value(wait_time);
  } else {
    /* Aggregate to EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME (counted) */
    global_idle_stat.aggregate_counted();
  }
}

/**
  Implementation of the mutex instrumentation interface.
  @sa PSI_v1::end_mutex_wait.
*/
void pfs_end_mutex_wait_v1(PSI_mutex_locker *locker, int rc) {
  PSI_mutex_locker_state *state =
      reinterpret_cast<PSI_mutex_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end = 0;
  ulonglong wait_time = 0;

  PFS_mutex *mutex = reinterpret_cast<PFS_mutex *>(state->m_mutex);
  DBUG_ASSERT(mutex != NULL);
  PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);

  uint flags = state->m_flags;

  if (flags & STATE_FLAG_TIMED) {
    timer_end = get_wait_timer();
    wait_time = timer_end - state->m_timer_start;
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (timed) */
    mutex->m_mutex_stat.m_wait_stat.aggregate_value(wait_time);
  } else {
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
    mutex->m_mutex_stat.m_wait_stat.aggregate_counted();
  }

  if (likely(rc == 0)) {
    mutex->m_owner = thread;
    mutex->m_last_locked = timer_end;
  }

  if (flags & STATE_FLAG_THREAD) {
    PFS_single_stat *event_name_array;
    event_name_array = thread->write_instr_class_waits_stats();
    uint index = mutex->m_class->m_event_name_index;

    DBUG_ASSERT(index <= wait_class_max);
    DBUG_ASSERT(sanitize_thread(thread) != NULL);

    if (flags & STATE_FLAG_TIMED) {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      event_name_array[index].aggregate_value(wait_time);
    } else {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[index].aggregate_counted();
    }

    if (flags & STATE_FLAG_EVENT) {
      PFS_events_waits *wait =
          reinterpret_cast<PFS_events_waits *>(state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end = timer_end;
      wait->m_end_event_id = thread->m_event_id;
      if (thread->m_flag_events_waits_history) {
        insert_events_waits_history(thread, wait);
      }
      if (thread->m_flag_events_waits_history_long) {
        insert_events_waits_history_long(wait);
      }
      thread->m_events_waits_current--;

      DBUG_ASSERT(wait == thread->m_events_waits_current);
    }
  }
}

/**
  Implementation of the rwlock instrumentation interface.
  @sa PSI_v1::end_rwlock_rdwait.
*/
void pfs_end_rwlock_rdwait_v1(PSI_rwlock_locker *locker, int rc) {
  PSI_rwlock_locker_state *state =
      reinterpret_cast<PSI_rwlock_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end = 0;
  ulonglong wait_time = 0;

  PFS_rwlock *rwlock = reinterpret_cast<PFS_rwlock *>(state->m_rwlock);
  DBUG_ASSERT(rwlock != NULL);

  if (state->m_flags & STATE_FLAG_TIMED) {
    timer_end = get_wait_timer();
    wait_time = timer_end - state->m_timer_start;
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (timed) */
    rwlock->m_rwlock_stat.m_wait_stat.aggregate_value(wait_time);
  } else {
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
    rwlock->m_rwlock_stat.m_wait_stat.aggregate_counted();
  }

  if (rc == 0) {
    /*
      Warning:
      Multiple threads can execute this section concurrently
      (since multiple readers can execute in parallel).
      The statistics generated are not safe, which is why they are
      just statistics, not facts.
    */
    if (rwlock->m_readers == 0) {
      rwlock->m_last_read = timer_end;
    }
    rwlock->m_writer = NULL;
    rwlock->m_readers++;
  }

  if (state->m_flags & STATE_FLAG_THREAD) {
    PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);
    DBUG_ASSERT(thread != NULL);

    PFS_single_stat *event_name_array;
    event_name_array = thread->write_instr_class_waits_stats();
    uint index = rwlock->m_class->m_event_name_index;

    if (state->m_flags & STATE_FLAG_TIMED) {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      event_name_array[index].aggregate_value(wait_time);
    } else {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[index].aggregate_counted();
    }

    if (state->m_flags & STATE_FLAG_EVENT) {
      PFS_events_waits *wait =
          reinterpret_cast<PFS_events_waits *>(state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end = timer_end;
      wait->m_end_event_id = thread->m_event_id;
      if (thread->m_flag_events_waits_history) {
        insert_events_waits_history(thread, wait);
      }
      if (thread->m_flag_events_waits_history_long) {
        insert_events_waits_history_long(wait);
      }
      thread->m_events_waits_current--;

      DBUG_ASSERT(wait == thread->m_events_waits_current);
    }
  }
}

/**
  Implementation of the rwlock instrumentation interface.
  @sa PSI_v1::end_rwlock_wrwait.
*/
void pfs_end_rwlock_wrwait_v1(PSI_rwlock_locker *locker, int rc) {
  PSI_rwlock_locker_state *state =
      reinterpret_cast<PSI_rwlock_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end = 0;
  ulonglong wait_time = 0;

  PFS_rwlock *rwlock = reinterpret_cast<PFS_rwlock *>(state->m_rwlock);
  DBUG_ASSERT(rwlock != NULL);
  PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);

  if (state->m_flags & STATE_FLAG_TIMED) {
    timer_end = get_wait_timer();
    wait_time = timer_end - state->m_timer_start;
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (timed) */
    rwlock->m_rwlock_stat.m_wait_stat.aggregate_value(wait_time);
  } else {
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
    rwlock->m_rwlock_stat.m_wait_stat.aggregate_counted();
  }

  if (likely(rc == 0)) {
    /* Thread safe : we are protected by the instrumented rwlock */
    rwlock->m_writer = thread;
    rwlock->m_last_written = timer_end;

    if ((state->m_operation != PSI_RWLOCK_SHAREDEXCLUSIVELOCK) &&
        (state->m_operation != PSI_RWLOCK_TRYSHAREDEXCLUSIVELOCK)) {
      /* Reset the readers stats, they could be off */
      rwlock->m_readers = 0;
      rwlock->m_last_read = 0;
    }
  }

  if (state->m_flags & STATE_FLAG_THREAD) {
    PFS_single_stat *event_name_array;
    event_name_array = thread->write_instr_class_waits_stats();
    uint index = rwlock->m_class->m_event_name_index;

    if (state->m_flags & STATE_FLAG_TIMED) {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      event_name_array[index].aggregate_value(wait_time);
    } else {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[index].aggregate_counted();
    }

    if (state->m_flags & STATE_FLAG_EVENT) {
      PFS_events_waits *wait =
          reinterpret_cast<PFS_events_waits *>(state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end = timer_end;
      wait->m_end_event_id = thread->m_event_id;
      if (thread->m_flag_events_waits_history) {
        insert_events_waits_history(thread, wait);
      }
      if (thread->m_flag_events_waits_history_long) {
        insert_events_waits_history_long(wait);
      }
      thread->m_events_waits_current--;

      DBUG_ASSERT(wait == thread->m_events_waits_current);
    }
  }
}

/**
  Implementation of the cond instrumentation interface.
  @sa PSI_v1::end_cond_wait.
*/
void pfs_end_cond_wait_v1(PSI_cond_locker *locker, int) {
  PSI_cond_locker_state *state =
      reinterpret_cast<PSI_cond_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end = 0;
  ulonglong wait_time = 0;

  PFS_cond *cond = reinterpret_cast<PFS_cond *>(state->m_cond);
  /* PFS_mutex *mutex= reinterpret_cast<PFS_mutex *> (state->m_mutex); */

  if (state->m_flags & STATE_FLAG_TIMED) {
    timer_end = get_wait_timer();
    wait_time = timer_end - state->m_timer_start;
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (timed) */
    cond->m_cond_stat.m_wait_stat.aggregate_value(wait_time);
  } else {
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
    cond->m_cond_stat.m_wait_stat.aggregate_counted();
  }

  if (state->m_flags & STATE_FLAG_THREAD) {
    PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);
    DBUG_ASSERT(thread != NULL);

    PFS_single_stat *event_name_array;
    event_name_array = thread->write_instr_class_waits_stats();
    uint index = cond->m_class->m_event_name_index;

    if (state->m_flags & STATE_FLAG_TIMED) {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      event_name_array[index].aggregate_value(wait_time);
    } else {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[index].aggregate_counted();
    }

    if (state->m_flags & STATE_FLAG_EVENT) {
      PFS_events_waits *wait =
          reinterpret_cast<PFS_events_waits *>(state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end = timer_end;
      wait->m_end_event_id = thread->m_event_id;
      if (thread->m_flag_events_waits_history) {
        insert_events_waits_history(thread, wait);
      }
      if (thread->m_flag_events_waits_history_long) {
        insert_events_waits_history_long(wait);
      }
      thread->m_events_waits_current--;

      DBUG_ASSERT(wait == thread->m_events_waits_current);
    }
  }
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::end_table_io_wait.
*/
void pfs_end_table_io_wait_v1(PSI_table_locker *locker, ulonglong numrows) {
  PSI_table_locker_state *state =
      reinterpret_cast<PSI_table_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end = 0;
  ulonglong wait_time = 0;

  PFS_table *table = reinterpret_cast<PFS_table *>(state->m_table);
  DBUG_ASSERT(table != NULL);

  PFS_single_stat *stat;
  PFS_table_io_stat *table_io_stat;

  DBUG_ASSERT((state->m_index < table->m_share->m_key_count) ||
              (state->m_index == MAX_INDEXES));

  table_io_stat = &table->m_table_stat.m_index_stat[state->m_index];
  table_io_stat->m_has_data = true;

  switch (state->m_io_operation) {
    case PSI_TABLE_FETCH_ROW:
      stat = &table_io_stat->m_fetch;
      break;
    case PSI_TABLE_WRITE_ROW:
      stat = &table_io_stat->m_insert;
      break;
    case PSI_TABLE_UPDATE_ROW:
      stat = &table_io_stat->m_update;
      break;
    case PSI_TABLE_DELETE_ROW:
      stat = &table_io_stat->m_delete;
      break;
    default:
      DBUG_ASSERT(false);
      stat = NULL;
      break;
  }

  uint flags = state->m_flags;

  if (flags & STATE_FLAG_TIMED) {
    timer_end = get_wait_timer();
    wait_time = timer_end - state->m_timer_start;
    stat->aggregate_many_value(wait_time, numrows);
  } else {
    stat->aggregate_counted(numrows);
  }

  if (flags & STATE_FLAG_THREAD) {
    PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);
    DBUG_ASSERT(thread != NULL);

    PFS_single_stat *event_name_array;
    event_name_array = thread->write_instr_class_waits_stats();

    /*
      Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      (for wait/io/table/sql/handler)
    */
    if (flags & STATE_FLAG_TIMED) {
      event_name_array[GLOBAL_TABLE_IO_EVENT_INDEX].aggregate_many_value(
          wait_time, numrows);
    } else {
      event_name_array[GLOBAL_TABLE_IO_EVENT_INDEX].aggregate_counted(numrows);
    }

    if (flags & STATE_FLAG_EVENT) {
      PFS_events_waits *wait =
          reinterpret_cast<PFS_events_waits *>(state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end = timer_end;
      wait->m_end_event_id = thread->m_event_id;
      wait->m_number_of_bytes = static_cast<size_t>(numrows);
      if (thread->m_flag_events_waits_history) {
        insert_events_waits_history(thread, wait);
      }
      if (thread->m_flag_events_waits_history_long) {
        insert_events_waits_history_long(wait);
      }
      thread->m_events_waits_current--;

      DBUG_ASSERT(wait == thread->m_events_waits_current);
    }
  }

  table->m_has_io_stats = true;
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::end_table_lock_wait.
*/
void pfs_end_table_lock_wait_v1(PSI_table_locker *locker) {
  PSI_table_locker_state *state =
      reinterpret_cast<PSI_table_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end = 0;
  ulonglong wait_time = 0;

  PFS_table *table = reinterpret_cast<PFS_table *>(state->m_table);
  DBUG_ASSERT(table != NULL);

  PFS_single_stat *stat =
      &table->m_table_stat.m_lock_stat.m_stat[state->m_index];

  uint flags = state->m_flags;

  if (flags & STATE_FLAG_TIMED) {
    timer_end = get_wait_timer();
    wait_time = timer_end - state->m_timer_start;
    stat->aggregate_value(wait_time);
  } else {
    stat->aggregate_counted();
  }

  if (flags & STATE_FLAG_THREAD) {
    PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);
    DBUG_ASSERT(thread != NULL);

    PFS_single_stat *event_name_array;
    event_name_array = thread->write_instr_class_waits_stats();

    /*
      Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      (for wait/lock/table/sql/handler)
    */
    if (flags & STATE_FLAG_TIMED) {
      event_name_array[GLOBAL_TABLE_LOCK_EVENT_INDEX].aggregate_value(
          wait_time);
    } else {
      event_name_array[GLOBAL_TABLE_LOCK_EVENT_INDEX].aggregate_counted();
    }

    if (flags & STATE_FLAG_EVENT) {
      PFS_events_waits *wait =
          reinterpret_cast<PFS_events_waits *>(state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end = timer_end;
      wait->m_end_event_id = thread->m_event_id;
      if (thread->m_flag_events_waits_history) {
        insert_events_waits_history(thread, wait);
      }
      if (thread->m_flag_events_waits_history_long) {
        insert_events_waits_history_long(wait);
      }
      thread->m_events_waits_current--;

      DBUG_ASSERT(wait == thread->m_events_waits_current);
    }
  }

  table->m_has_lock_stats = true;
}

void pfs_start_file_wait_v1(PSI_file_locker *locker, size_t count,
                            const char *src_file, uint src_line);

void pfs_end_file_wait_v1(PSI_file_locker *locker, size_t count);

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::start_file_open_wait.
*/
void pfs_start_file_open_wait_v1(PSI_file_locker *locker, const char *src_file,
                                 uint src_line) {
  pfs_start_file_wait_v1(locker, 0, src_file, src_line);

  return;
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::end_file_open_wait.
*/
PSI_file *pfs_end_file_open_wait_v1(PSI_file_locker *locker, void *result) {
  PSI_file_locker_state *state =
      reinterpret_cast<PSI_file_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  switch (state->m_operation) {
    case PSI_FILE_STAT:
    case PSI_FILE_RENAME:
      break;
    case PSI_FILE_STREAM_OPEN:
    case PSI_FILE_CREATE:
    case PSI_FILE_OPEN:
      if (result != NULL) {
        PFS_file_class *klass =
            reinterpret_cast<PFS_file_class *>(state->m_class);
        PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);
        const char *name = state->m_name;
        uint len = (uint)strlen(name);
        PFS_file *pfs_file =
            find_or_create_file(thread, klass, name, len, true);
        state->m_file = reinterpret_cast<PSI_file *>(pfs_file);
      }
      break;
    default:
      DBUG_ASSERT(false);
      break;
  }

  pfs_end_file_wait_v1(locker, 0);

  return state->m_file;
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::end_file_open_wait_and_bind_to_descriptor.
*/
void pfs_end_file_open_wait_and_bind_to_descriptor_v1(PSI_file_locker *locker,
                                                      File file) {
  PFS_file *pfs_file = NULL;
  int index = (int)file;
  PSI_file_locker_state *state =
      reinterpret_cast<PSI_file_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  if (index >= 0) {
    PFS_file_class *klass = reinterpret_cast<PFS_file_class *>(state->m_class);
    PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);
    const char *name = state->m_name;
    uint len = (uint)strlen(name);
    pfs_file = find_or_create_file(thread, klass, name, len, true);
    state->m_file = reinterpret_cast<PSI_file *>(pfs_file);
  }

  pfs_end_file_wait_v1(locker, 0);

  if (likely(index >= 0)) {
    if (likely(index < file_handle_max)) {
      file_handle_array[index] = pfs_file;
    } else {
      if (pfs_file != NULL) {
        release_file(pfs_file);
      }
      file_handle_lost++;
    }
  }
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::end_temp_file_open_wait_and_bind_to_descriptor.
*/
void pfs_end_temp_file_open_wait_and_bind_to_descriptor_v1(
    PSI_file_locker *locker, File file, const char *filename) {
  DBUG_ASSERT(filename != NULL);
  PSI_file_locker_state *state =
      reinterpret_cast<PSI_file_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  /* Set filename that was generated during creation of temporary file. */
  state->m_name = filename;
  pfs_end_file_open_wait_and_bind_to_descriptor_v1(locker, file);

  PFS_file *pfs_file = reinterpret_cast<PFS_file *>(state->m_file);
  if (pfs_file != NULL) {
    pfs_file->m_temporary = true;
  }
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::start_file_wait.
*/
void pfs_start_file_wait_v1(PSI_file_locker *locker, size_t count,
                            const char *src_file, uint src_line) {
  ulonglong timer_start = 0;
  PSI_file_locker_state *state =
      reinterpret_cast<PSI_file_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  uint flags = state->m_flags;

  if (flags & STATE_FLAG_TIMED) {
    timer_start = get_wait_timer();
    state->m_timer_start = timer_start;
  }

  if (flags & STATE_FLAG_EVENT) {
    PFS_events_waits *wait =
        reinterpret_cast<PFS_events_waits *>(state->m_wait);
    DBUG_ASSERT(wait != NULL);

    wait->m_timer_start = timer_start;
    wait->m_source_file = src_file;
    wait->m_source_line = src_line;
    wait->m_number_of_bytes = count;
  }
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::end_file_wait.
*/
void pfs_end_file_wait_v1(PSI_file_locker *locker, size_t byte_count) {
  PSI_file_locker_state *state =
      reinterpret_cast<PSI_file_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);
  PFS_file *file = reinterpret_cast<PFS_file *>(state->m_file);
  PFS_file_class *klass = reinterpret_cast<PFS_file_class *>(state->m_class);
  PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);

  ulonglong timer_end = 0;
  ulonglong wait_time = 0;
  PFS_byte_stat *byte_stat;
  uint flags = state->m_flags;
  size_t bytes = ((int)byte_count > -1 ? byte_count : 0);

  PFS_file_stat *file_stat;

  if (file != NULL) {
    file_stat = &file->m_file_stat;
  } else {
    file_stat = &klass->m_file_stat;
  }

  switch (state->m_operation) {
    /* Group read operations */
    case PSI_FILE_READ:
      byte_stat = &file_stat->m_io_stat.m_read;
      break;
    /* Group write operations */
    case PSI_FILE_WRITE:
      byte_stat = &file_stat->m_io_stat.m_write;
      break;
    /* Group remaining operations as miscellaneous */
    case PSI_FILE_CREATE:
    case PSI_FILE_CREATE_TMP:
    case PSI_FILE_OPEN:
    case PSI_FILE_STREAM_OPEN:
    case PSI_FILE_STREAM_CLOSE:
    case PSI_FILE_SEEK:
    case PSI_FILE_TELL:
    case PSI_FILE_FLUSH:
    case PSI_FILE_FSTAT:
    case PSI_FILE_CHSIZE:
    case PSI_FILE_DELETE:
    case PSI_FILE_RENAME:
    case PSI_FILE_SYNC:
    case PSI_FILE_STAT:
    case PSI_FILE_CLOSE:
      byte_stat = &file_stat->m_io_stat.m_misc;
      break;
    default:
      DBUG_ASSERT(false);
      byte_stat = NULL;
      break;
  }

  /* Aggregation for EVENTS_WAITS_SUMMARY_BY_INSTANCE */
  if (flags & STATE_FLAG_TIMED) {
    timer_end = get_wait_timer();
    wait_time = timer_end - state->m_timer_start;
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (timed) */
    byte_stat->aggregate(wait_time, bytes);
  } else {
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
    byte_stat->aggregate_counted(bytes);
  }

  if (flags & STATE_FLAG_THREAD) {
    DBUG_ASSERT(thread != NULL);

    PFS_single_stat *event_name_array;
    event_name_array = thread->write_instr_class_waits_stats();
    uint index = klass->m_event_name_index;

    if (flags & STATE_FLAG_TIMED) {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      event_name_array[index].aggregate_value(wait_time);
    } else {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[index].aggregate_counted();
    }

    if (state->m_flags & STATE_FLAG_EVENT) {
      PFS_events_waits *wait =
          reinterpret_cast<PFS_events_waits *>(state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end = timer_end;
      wait->m_number_of_bytes = bytes;
      wait->m_end_event_id = thread->m_event_id;
      wait->m_object_instance_addr = file;
      wait->m_weak_file = file;
      wait->m_weak_version = (file ? file->get_version() : 0);

      if (thread->m_flag_events_waits_history) {
        insert_events_waits_history(thread, wait);
      }
      if (thread->m_flag_events_waits_history_long) {
        insert_events_waits_history_long(wait);
      }
      thread->m_events_waits_current--;

      DBUG_ASSERT(wait == thread->m_events_waits_current);
    }
  }
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::start_file_close_wait.
*/
void pfs_start_file_close_wait_v1(PSI_file_locker *locker, const char *src_file,
                                  uint src_line) {
  PFS_thread *thread;
  const char *name;
  uint len;
  PFS_file *pfs_file;
  PSI_file_locker_state *state =
      reinterpret_cast<PSI_file_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  switch (state->m_operation) {
    case PSI_FILE_DELETE:
      thread = reinterpret_cast<PFS_thread *>(state->m_thread);
      name = state->m_name;
      len = (uint)strlen(name);
      pfs_file = find_or_create_file(thread, NULL, name, len, false);
      state->m_file = reinterpret_cast<PSI_file *>(pfs_file);
      break;
    case PSI_FILE_STREAM_CLOSE:
    case PSI_FILE_CLOSE:
      break;
    default:
      DBUG_ASSERT(false);
      break;
  }

  pfs_start_file_wait_v1(locker, 0, src_file, src_line);

  return;
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::end_file_close_wait.
*/
void pfs_end_file_close_wait_v1(PSI_file_locker *locker, int rc) {
  PSI_file_locker_state *state =
      reinterpret_cast<PSI_file_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  pfs_end_file_wait_v1(locker, 0);

  if (rc == 0) {
    PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);
    PFS_file *file = reinterpret_cast<PFS_file *>(state->m_file);

    /* Release or destroy the file if necessary */
    switch (state->m_operation) {
      case PSI_FILE_CLOSE:
        if (file != NULL) {
          if (file->m_temporary) {
            DBUG_ASSERT(file->m_file_stat.m_open_count <= 1);
            destroy_file(thread, file);
          } else {
            release_file(file);
          }
        }
        break;
      case PSI_FILE_STREAM_CLOSE:
        if (file != NULL) {
          release_file(file);
        }
        break;
      case PSI_FILE_DELETE:
        if (file != NULL) {
          destroy_file(thread, file);
        }
        break;
      default:
        DBUG_ASSERT(false);
        break;
    }
  }
  return;
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::end_file_rename_wait.
*/
void pfs_end_file_rename_wait_v1(PSI_file_locker *locker, const char *old_name,
                                 const char *new_name, int rc) {
  PSI_file_locker_state *state =
      reinterpret_cast<PSI_file_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);
  DBUG_ASSERT(state->m_operation == PSI_FILE_RENAME);

  if (rc == 0) {
    PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);

    uint old_len = (uint)strlen(old_name);
    uint new_len = (uint)strlen(new_name);

    find_and_rename_file(thread, old_name, old_len, new_name, new_len);
  }

  pfs_end_file_wait_v1(locker, 0);
  return;
}

PSI_stage_progress *pfs_start_stage_v1(PSI_stage_key key, const char *src_file,
                                       int src_line) {
  ulonglong timer_value = 0;

  PFS_thread *pfs_thread = my_thread_get_THR_PFS();
  if (unlikely(pfs_thread == NULL)) {
    return NULL;
  }

  /* Always update column threads.processlist_state. */
  pfs_thread->m_stage = key;
  /* Default value when the stage is not instrumented for progress */
  pfs_thread->m_stage_progress = NULL;

  if (!flag_global_instrumentation) {
    return NULL;
  }

  if (flag_thread_instrumentation && !pfs_thread->m_enabled) {
    return NULL;
  }

  PFS_events_stages *pfs = &pfs_thread->m_stage_current;
  PFS_events_waits *child_wait = &pfs_thread->m_events_waits_stack[0];
  PFS_events_statements *parent_statement = &pfs_thread->m_statement_stack[0];

  PFS_instr_class *old_class = pfs->m_class;
  if (old_class != NULL) {
    PFS_stage_stat *event_name_array;
    event_name_array = pfs_thread->write_instr_class_stages_stats();
    uint index = old_class->m_event_name_index;

    /* Finish old event */
    if (old_class->m_timed) {
      timer_value = get_stage_timer();
      pfs->m_timer_end = timer_value;

      /* Aggregate to EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      ulonglong stage_time = timer_value - pfs->m_timer_start;
      event_name_array[index].aggregate_value(stage_time);
    } else {
      /* Aggregate to EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[index].aggregate_counted();
    }

    if (flag_events_stages_current) {
      pfs->m_end_event_id = pfs_thread->m_event_id;
      if (pfs_thread->m_flag_events_stages_history) {
        insert_events_stages_history(pfs_thread, pfs);
      }
      if (pfs_thread->m_flag_events_stages_history_long) {
        insert_events_stages_history_long(pfs);
      }
    }

    /* This stage event is now complete. */
    pfs->m_class = NULL;

    /* New waits will now be attached directly to the parent statement. */
    child_wait->m_event_id = parent_statement->m_event_id;
    child_wait->m_event_type = parent_statement->m_event_type;
    /* See below for new stages, that may overwrite this. */
  }

  /* Start new event */

  PFS_stage_class *new_klass = find_stage_class(key);
  if (unlikely(new_klass == NULL)) {
    return NULL;
  }

  if (!new_klass->m_enabled) {
    return NULL;
  }

  pfs->m_class = new_klass;
  if (new_klass->m_timed) {
    /*
      Do not call the timer again if we have a
      TIMER_END for the previous stage already.
    */
    if (timer_value == 0) {
      timer_value = get_stage_timer();
    }
    pfs->m_timer_start = timer_value;
  } else {
    pfs->m_timer_start = 0;
  }
  pfs->m_timer_end = 0;

  if (flag_events_stages_current) {
    pfs->m_thread_internal_id = pfs_thread->m_thread_internal_id;
    pfs->m_event_id = pfs_thread->m_event_id++;
    pfs->m_end_event_id = 0;
    pfs->m_source_file = src_file;
    pfs->m_source_line = src_line;

    /* New wait events will have this new stage as parent. */
    child_wait->m_event_id = pfs->m_event_id;
    child_wait->m_event_type = EVENT_TYPE_STAGE;
  }

  if (new_klass->is_progress()) {
    pfs_thread->m_stage_progress = &pfs->m_progress;
    pfs->m_progress.m_work_completed = 0;
    pfs->m_progress.m_work_estimated = 0;
  }

  return pfs_thread->m_stage_progress;
}

PSI_stage_progress *pfs_get_current_stage_progress_v1() {
  PFS_thread *pfs_thread = my_thread_get_THR_PFS();
  if (unlikely(pfs_thread == NULL)) {
    return NULL;
  }

  return pfs_thread->m_stage_progress;
}

void pfs_end_stage_v1() {
  ulonglong timer_value = 0;

  PFS_thread *pfs_thread = my_thread_get_THR_PFS();
  if (unlikely(pfs_thread == NULL)) {
    return;
  }

  pfs_thread->m_stage = 0;
  pfs_thread->m_stage_progress = NULL;

  if (!flag_global_instrumentation) {
    return;
  }

  if (flag_thread_instrumentation && !pfs_thread->m_enabled) {
    return;
  }

  PFS_events_stages *pfs = &pfs_thread->m_stage_current;

  PFS_instr_class *old_class = pfs->m_class;
  if (old_class != NULL) {
    PFS_stage_stat *event_name_array;
    event_name_array = pfs_thread->write_instr_class_stages_stats();
    uint index = old_class->m_event_name_index;

    /* Finish old event */
    if (old_class->m_timed) {
      timer_value = get_stage_timer();
      pfs->m_timer_end = timer_value;

      /* Aggregate to EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      ulonglong stage_time = timer_value - pfs->m_timer_start;
      event_name_array[index].aggregate_value(stage_time);
    } else {
      /* Aggregate to EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[index].aggregate_counted();
    }

    if (flag_events_stages_current) {
      pfs->m_end_event_id = pfs_thread->m_event_id;
      if (pfs_thread->m_flag_events_stages_history) {
        insert_events_stages_history(pfs_thread, pfs);
      }
      if (pfs_thread->m_flag_events_stages_history_long) {
        insert_events_stages_history_long(pfs);
      }
    }

    /* New waits will now be attached directly to the parent statement. */
    PFS_events_waits *child_wait = &pfs_thread->m_events_waits_stack[0];
    PFS_events_statements *parent_statement = &pfs_thread->m_statement_stack[0];
    child_wait->m_event_id = parent_statement->m_event_id;
    child_wait->m_event_type = parent_statement->m_event_type;

    /* This stage is completed */
    pfs->m_class = NULL;
  }
}

PSI_statement_locker *pfs_get_thread_statement_locker_v1(
    PSI_statement_locker_state *state, PSI_statement_key key,
    const void *charset, PSI_sp_share *sp_share) {
  DBUG_ASSERT(state != NULL);
  DBUG_ASSERT(charset != NULL);
  if (!flag_global_instrumentation) {
    return NULL;
  }
  PFS_statement_class *klass = find_statement_class(key);
  if (unlikely(klass == NULL)) {
    return NULL;
  }
  if (!klass->m_enabled) {
    return NULL;
  }

  uint flags;

  if (flag_thread_instrumentation) {
    PFS_thread *pfs_thread = my_thread_get_THR_PFS();
    if (unlikely(pfs_thread == NULL)) {
      return NULL;
    }
    if (!pfs_thread->m_enabled) {
      return NULL;
    }
    state->m_thread = reinterpret_cast<PSI_thread *>(pfs_thread);
    flags = STATE_FLAG_THREAD;

    if (klass->m_timed) {
      flags |= STATE_FLAG_TIMED;
    }

    if (flag_events_statements_current) {
      ulonglong event_id = pfs_thread->m_event_id++;

      if (pfs_thread->m_events_statements_count >= statement_stack_max) {
        nested_statement_lost++;
        return NULL;
      }

      pfs_dirty_state dirty_state;
      pfs_thread->m_stmt_lock.allocated_to_dirty(&dirty_state);
      PFS_events_statements *pfs =
          &pfs_thread->m_statement_stack[pfs_thread->m_events_statements_count];
      pfs->m_thread_internal_id = pfs_thread->m_thread_internal_id;
      pfs->m_event_id = event_id;
      pfs->m_event_type = EVENT_TYPE_STATEMENT;
      pfs->m_end_event_id = 0;
      pfs->m_class = klass;
      pfs->m_timer_start = 0;
      pfs->m_timer_end = 0;
      pfs->m_lock_time = 0;
      pfs->m_current_schema_name_length = 0;
      pfs->m_sqltext_length = 0;
      pfs->m_sqltext_truncated = false;
      pfs->m_sqltext_cs_number = system_charset_info->number; /* default */

      pfs->m_message_text[0] = '\0';
      pfs->m_sql_errno = 0;
      pfs->m_sqlstate[0] = '\0';
      pfs->m_error_count = 0;
      pfs->m_warning_count = 0;
      pfs->m_rows_affected = 0;

      pfs->m_rows_sent = 0;
      pfs->m_rows_examined = 0;
      pfs->m_created_tmp_disk_tables = 0;
      pfs->m_created_tmp_tables = 0;
      pfs->m_select_full_join = 0;
      pfs->m_select_full_range_join = 0;
      pfs->m_select_range = 0;
      pfs->m_select_range_check = 0;
      pfs->m_select_scan = 0;
      pfs->m_sort_merge_passes = 0;
      pfs->m_sort_range = 0;
      pfs->m_sort_rows = 0;
      pfs->m_sort_scan = 0;
      pfs->m_no_index_used = 0;
      pfs->m_no_good_index_used = 0;
      pfs->m_digest_storage.reset();

      /* New stages will have this statement as parent */
      PFS_events_stages *child_stage = &pfs_thread->m_stage_current;
      child_stage->m_nesting_event_id = event_id;
      child_stage->m_nesting_event_type = EVENT_TYPE_STATEMENT;

      /* New waits will have this statement as parent, if no stage is
       * instrumented */
      PFS_events_waits *child_wait = &pfs_thread->m_events_waits_stack[0];
      child_wait->m_event_id = event_id;
      child_wait->m_event_type = EVENT_TYPE_STATEMENT;

      PFS_events_statements *parent_statement = NULL;
      PFS_events_transactions *parent_transaction =
          &pfs_thread->m_transaction_current;
      ulonglong parent_event = 0;
      enum_event_type parent_type = EVENT_TYPE_STATEMENT;
      uint parent_level = 0;

      if (pfs_thread->m_events_statements_count > 0) {
        parent_statement = pfs - 1;
        parent_event = parent_statement->m_event_id;
        parent_type = parent_statement->m_event_type;
        parent_level = parent_statement->m_nesting_event_level + 1;
      }

      if (parent_transaction->m_state == TRANS_STATE_ACTIVE &&
          parent_transaction->m_event_id > parent_event) {
        parent_event = parent_transaction->m_event_id;
        parent_type = parent_transaction->m_event_type;
      }

      pfs->m_nesting_event_id = parent_event;
      pfs->m_nesting_event_type = parent_type;
      pfs->m_nesting_event_level = parent_level;

      /* Set parent Stored Procedure information for this statement. */
      if (sp_share) {
        PFS_program *parent_sp = reinterpret_cast<PFS_program *>(sp_share);
        pfs->m_sp_type = parent_sp->m_type;
        memcpy(pfs->m_schema_name, parent_sp->m_schema_name,
               parent_sp->m_schema_name_length);
        pfs->m_schema_name_length = parent_sp->m_schema_name_length;
        memcpy(pfs->m_object_name, parent_sp->m_object_name,
               parent_sp->m_object_name_length);
        pfs->m_object_name_length = parent_sp->m_object_name_length;
      } else {
        pfs->m_sp_type = NO_OBJECT_TYPE;
        pfs->m_schema_name_length = 0;
        pfs->m_object_name_length = 0;
      }

      state->m_statement = pfs;
      flags |= STATE_FLAG_EVENT;

      pfs_thread->m_events_statements_count++;
      pfs_thread->m_stmt_lock.dirty_to_allocated(&dirty_state);
    } else {
      state->m_statement = NULL;
    }
  } else {
    state->m_statement = NULL;

    if (klass->m_timed) {
      flags = STATE_FLAG_TIMED;
    } else {
      flags = 0;
    }
  }

  if (flag_statements_digest) {
    flags |= STATE_FLAG_DIGEST;
  }

  state->m_discarded = false;
  state->m_class = klass;
  state->m_flags = flags;

  state->m_lock_time = 0;
  state->m_rows_sent = 0;
  state->m_rows_examined = 0;
  state->m_created_tmp_disk_tables = 0;
  state->m_created_tmp_tables = 0;
  state->m_select_full_join = 0;
  state->m_select_full_range_join = 0;
  state->m_select_range = 0;
  state->m_select_range_check = 0;
  state->m_select_scan = 0;
  state->m_sort_merge_passes = 0;
  state->m_sort_range = 0;
  state->m_sort_rows = 0;
  state->m_sort_scan = 0;
  state->m_no_index_used = 0;
  state->m_no_good_index_used = 0;

  state->m_digest = NULL;
  state->m_cs_number = ((CHARSET_INFO *)charset)->number;

  state->m_schema_name_length = 0;
  state->m_parent_sp_share = sp_share;
  state->m_parent_prepared_stmt = NULL;

  state->m_query_sample = nullptr;
  state->m_query_sample_length = 0;
  state->m_query_sample_truncated = false;

  return reinterpret_cast<PSI_statement_locker *>(state);
}

PSI_statement_locker *pfs_refine_statement_v1(PSI_statement_locker *locker,
                                              PSI_statement_key key) {
  PSI_statement_locker_state *state =
      reinterpret_cast<PSI_statement_locker_state *>(locker);
  if (state == NULL) {
    return NULL;
  }
  DBUG_ASSERT(state->m_class != NULL);
  PFS_statement_class *klass;
  /* Only refine statements for mutable instrumentation */
  klass = reinterpret_cast<PFS_statement_class *>(state->m_class);
  DBUG_ASSERT(klass->is_mutable());
  klass = find_statement_class(key);

  uint flags = state->m_flags;

  if (unlikely(klass == NULL) || !klass->m_enabled) {
    /* pop statement stack */
    if (flags & STATE_FLAG_THREAD) {
      PFS_thread *pfs_thread = reinterpret_cast<PFS_thread *>(state->m_thread);
      DBUG_ASSERT(pfs_thread != NULL);
      if (pfs_thread->m_events_statements_count > 0) {
        pfs_thread->m_events_statements_count--;
      }
    }

    state->m_discarded = true;
    return NULL;
  }

  if ((flags & STATE_FLAG_TIMED) && !klass->m_timed) {
    flags = flags & ~STATE_FLAG_TIMED;
  }

  if (flags & STATE_FLAG_EVENT) {
    PFS_events_statements *pfs =
        reinterpret_cast<PFS_events_statements *>(state->m_statement);
    DBUG_ASSERT(pfs != NULL);

    /* mutate EVENTS_STATEMENTS_CURRENT.EVENT_NAME */
    pfs->m_class = klass;
  }

  state->m_class = klass;
  state->m_flags = flags;
  return reinterpret_cast<PSI_statement_locker *>(state);
}

void pfs_start_statement_v1(PSI_statement_locker *locker, const char *db,
                            uint db_len, const char *src_file, uint src_line) {
  PSI_statement_locker_state *state =
      reinterpret_cast<PSI_statement_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  uint flags = state->m_flags;
  ulonglong timer_start = 0;

  if (flags & STATE_FLAG_TIMED) {
    timer_start = get_statement_timer();
    state->m_timer_start = timer_start;
  }

  static_assert(PSI_SCHEMA_NAME_LEN == NAME_LEN, "");
  DBUG_ASSERT(db_len <= sizeof(state->m_schema_name));

  if (db_len > 0) {
    memcpy(state->m_schema_name, db, db_len);
  }
  state->m_schema_name_length = db_len;

  if (flags & STATE_FLAG_EVENT) {
    PFS_events_statements *pfs =
        reinterpret_cast<PFS_events_statements *>(state->m_statement);
    DBUG_ASSERT(pfs != NULL);

    pfs->m_timer_start = timer_start;
    pfs->m_source_file = src_file;
    pfs->m_source_line = src_line;

    DBUG_ASSERT(db_len <= sizeof(pfs->m_current_schema_name));
    if (db_len > 0) {
      memcpy(pfs->m_current_schema_name, db, db_len);
    }
    pfs->m_current_schema_name_length = db_len;
  }

  state->m_query_sample = nullptr;
  state->m_query_sample_length = 0;
  state->m_query_sample_truncated = false;
}

void pfs_set_statement_text_v1(PSI_statement_locker *locker, const char *text,
                               uint text_len) {
  PSI_statement_locker_state *state =
      reinterpret_cast<PSI_statement_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  if (state->m_discarded) {
    return;
  }

  if (text_len > pfs_max_sqltext) {
    text_len = (uint)pfs_max_sqltext;
    state->m_query_sample_truncated = true;
  }
  state->m_query_sample = text;
  state->m_query_sample_length = text_len;

  if (state->m_flags & STATE_FLAG_EVENT) {
    PFS_events_statements *pfs =
        reinterpret_cast<PFS_events_statements *>(state->m_statement);
    DBUG_ASSERT(pfs != NULL);

    pfs->m_sqltext_length = text_len;
    pfs->m_sqltext_truncated = state->m_query_sample_truncated;
    pfs->m_sqltext_cs_number = state->m_cs_number;
    if (text_len) {
      DBUG_ASSERT(pfs->m_sqltext != NULL);
      memcpy(pfs->m_sqltext, text, text_len);
    }
  }

  return;
}

#define SET_STATEMENT_ATTR_BODY(LOCKER, ATTR, VALUE)                     \
  PSI_statement_locker_state *state;                                     \
  state = reinterpret_cast<PSI_statement_locker_state *>(LOCKER);        \
  if (unlikely(state == NULL)) {                                         \
    return;                                                              \
  }                                                                      \
  if (state->m_discarded) {                                              \
    return;                                                              \
  }                                                                      \
  state->ATTR = VALUE;                                                   \
  if (state->m_flags & STATE_FLAG_EVENT) {                               \
    PFS_events_statements *pfs;                                          \
    pfs = reinterpret_cast<PFS_events_statements *>(state->m_statement); \
    DBUG_ASSERT(pfs != NULL);                                            \
    pfs->ATTR = VALUE;                                                   \
  }                                                                      \
  return;

#define INC_STATEMENT_ATTR_BODY(LOCKER, ATTR, VALUE)                     \
  PSI_statement_locker_state *state;                                     \
  state = reinterpret_cast<PSI_statement_locker_state *>(LOCKER);        \
  if (unlikely(state == NULL)) {                                         \
    return;                                                              \
  }                                                                      \
  if (state->m_discarded) {                                              \
    return;                                                              \
  }                                                                      \
  state->ATTR += VALUE;                                                  \
  if (state->m_flags & STATE_FLAG_EVENT) {                               \
    PFS_events_statements *pfs;                                          \
    pfs = reinterpret_cast<PFS_events_statements *>(state->m_statement); \
    DBUG_ASSERT(pfs != NULL);                                            \
    pfs->ATTR += VALUE;                                                  \
  }                                                                      \
  return;

void pfs_set_statement_lock_time_v1(PSI_statement_locker *locker,
                                    ulonglong count) {
  SET_STATEMENT_ATTR_BODY(locker, m_lock_time, count);
}

void pfs_set_statement_rows_sent_v1(PSI_statement_locker *locker,
                                    ulonglong count) {
  SET_STATEMENT_ATTR_BODY(locker, m_rows_sent, count);
}

void pfs_set_statement_rows_examined_v1(PSI_statement_locker *locker,
                                        ulonglong count) {
  SET_STATEMENT_ATTR_BODY(locker, m_rows_examined, count);
}

void pfs_inc_statement_created_tmp_disk_tables_v1(PSI_statement_locker *locker,
                                                  ulong count) {
  INC_STATEMENT_ATTR_BODY(locker, m_created_tmp_disk_tables, count);
}

void pfs_inc_statement_created_tmp_tables_v1(PSI_statement_locker *locker,
                                             ulong count) {
  INC_STATEMENT_ATTR_BODY(locker, m_created_tmp_tables, count);
}

void pfs_inc_statement_select_full_join_v1(PSI_statement_locker *locker,
                                           ulong count) {
  INC_STATEMENT_ATTR_BODY(locker, m_select_full_join, count);
}

void pfs_inc_statement_select_full_range_join_v1(PSI_statement_locker *locker,
                                                 ulong count) {
  INC_STATEMENT_ATTR_BODY(locker, m_select_full_range_join, count);
}

void pfs_inc_statement_select_range_v1(PSI_statement_locker *locker,
                                       ulong count) {
  INC_STATEMENT_ATTR_BODY(locker, m_select_range, count);
}

void pfs_inc_statement_select_range_check_v1(PSI_statement_locker *locker,
                                             ulong count) {
  INC_STATEMENT_ATTR_BODY(locker, m_select_range_check, count);
}

void pfs_inc_statement_select_scan_v1(PSI_statement_locker *locker,
                                      ulong count) {
  INC_STATEMENT_ATTR_BODY(locker, m_select_scan, count);
}

void pfs_inc_statement_sort_merge_passes_v1(PSI_statement_locker *locker,
                                            ulong count) {
  INC_STATEMENT_ATTR_BODY(locker, m_sort_merge_passes, count);
}

void pfs_inc_statement_sort_range_v1(PSI_statement_locker *locker,
                                     ulong count) {
  INC_STATEMENT_ATTR_BODY(locker, m_sort_range, count);
}

void pfs_inc_statement_sort_rows_v1(PSI_statement_locker *locker, ulong count) {
  INC_STATEMENT_ATTR_BODY(locker, m_sort_rows, count);
}

void pfs_inc_statement_sort_scan_v1(PSI_statement_locker *locker, ulong count) {
  INC_STATEMENT_ATTR_BODY(locker, m_sort_scan, count);
}

void pfs_set_statement_no_index_used_v1(PSI_statement_locker *locker) {
  SET_STATEMENT_ATTR_BODY(locker, m_no_index_used, 1);
}

void pfs_set_statement_no_good_index_used_v1(PSI_statement_locker *locker) {
  SET_STATEMENT_ATTR_BODY(locker, m_no_good_index_used, 1);
}

void pfs_end_statement_v1(PSI_statement_locker *locker, void *stmt_da) {
  PSI_statement_locker_state *state =
      reinterpret_cast<PSI_statement_locker_state *>(locker);
  Diagnostics_area *da = reinterpret_cast<Diagnostics_area *>(stmt_da);
  DBUG_ASSERT(state != NULL);
  DBUG_ASSERT(da != NULL);

  if (state->m_discarded) {
    return;
  }

  PFS_statement_class *klass =
      reinterpret_cast<PFS_statement_class *>(state->m_class);
  DBUG_ASSERT(klass != NULL);

  ulonglong timer_end = 0;
  ulonglong wait_time = 0;
  uint flags = state->m_flags;

  if (flags & STATE_FLAG_TIMED) {
    timer_end = get_statement_timer();
    wait_time = timer_end - state->m_timer_start;
  }

  PFS_statement_stat *event_name_array;
  uint index = klass->m_event_name_index;
  PFS_statement_stat *stat;

  /*
   Capture statement stats by digest.
  */
  const sql_digest_storage *digest_storage = NULL;
  PFS_statements_digest_stat *digest_stat = NULL;
  PFS_program *pfs_program = NULL;
  PFS_prepared_stmt *pfs_prepared_stmt = NULL;

  if (flags & STATE_FLAG_THREAD) {
    PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);
    DBUG_ASSERT(thread != NULL);
    event_name_array = thread->write_instr_class_statements_stats();
    /* Aggregate to EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME */
    stat = &event_name_array[index];

    if (flags & STATE_FLAG_DIGEST) {
      digest_storage = state->m_digest;

      if (digest_storage != NULL) {
        /* Populate PFS_statements_digest_stat with computed digest
         * information.*/
        digest_stat =
            find_or_create_digest(thread, digest_storage, state->m_schema_name,
                                  state->m_schema_name_length);
      }
    }

    if (flags & STATE_FLAG_EVENT) {
      PFS_events_statements *pfs =
          reinterpret_cast<PFS_events_statements *>(state->m_statement);
      DBUG_ASSERT(pfs != NULL);

      pfs_dirty_state dirty_state;
      thread->m_stmt_lock.allocated_to_dirty(&dirty_state);

      switch (da->status()) {
        case Diagnostics_area::DA_EMPTY:
          break;
        case Diagnostics_area::DA_OK:
          memcpy(pfs->m_message_text, da->message_text(), MYSQL_ERRMSG_SIZE);
          pfs->m_message_text[MYSQL_ERRMSG_SIZE] = 0;
          pfs->m_rows_affected = da->affected_rows();
          pfs->m_warning_count = da->last_statement_cond_count();
          memcpy(pfs->m_sqlstate, "00000", SQLSTATE_LENGTH);
          break;
        case Diagnostics_area::DA_EOF:
          pfs->m_warning_count = da->last_statement_cond_count();
          break;
        case Diagnostics_area::DA_ERROR:
          memcpy(pfs->m_message_text, da->message_text(), MYSQL_ERRMSG_SIZE);
          pfs->m_message_text[MYSQL_ERRMSG_SIZE] = 0;
          pfs->m_sql_errno = da->mysql_errno();
          memcpy(pfs->m_sqlstate, da->returned_sqlstate(), SQLSTATE_LENGTH);
          pfs->m_error_count++;
          break;
        case Diagnostics_area::DA_DISABLED:
          break;
      }

      pfs->m_timer_end = timer_end;
      pfs->m_end_event_id = thread->m_event_id;

      pfs_program = reinterpret_cast<PFS_program *>(state->m_parent_sp_share);
      pfs_prepared_stmt =
          reinterpret_cast<PFS_prepared_stmt *>(state->m_parent_prepared_stmt);

      if (thread->m_flag_events_statements_history) {
        insert_events_statements_history(thread, pfs);
      }
      if (thread->m_flag_events_statements_history_long) {
        insert_events_statements_history_long(pfs);
      }

      DBUG_ASSERT(thread->m_events_statements_count > 0);
      thread->m_events_statements_count--;
      thread->m_stmt_lock.dirty_to_allocated(&dirty_state);
    }
  } else {
    if (flags & STATE_FLAG_DIGEST) {
      PFS_thread *thread = my_thread_get_THR_PFS();

      /* An instrumented thread is required, for LF_PINS. */
      if (thread != NULL) {
        /* Set digest stat. */
        digest_storage = state->m_digest;

        if (digest_storage != NULL) {
          /* Populate statements_digest_stat with computed digest information.
           */
          digest_stat = find_or_create_digest(thread, digest_storage,
                                              state->m_schema_name,
                                              state->m_schema_name_length);
        }
      }
    }

    event_name_array = global_instr_class_statements_array;
    /* Aggregate to EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME */
    stat = &event_name_array[index];
  }

  stat->mark_used();

  if (flags & STATE_FLAG_TIMED) {
    /* Aggregate to EVENTS_STATEMENTS_SUMMARY_..._BY_EVENT_NAME (timed) */
    stat->aggregate_value(wait_time);
  } else {
    /* Aggregate to EVENTS_STATEMENTS_SUMMARY_..._BY_EVENT_NAME (counted) */
    stat->aggregate_counted();
  }

  stat->m_lock_time += state->m_lock_time;
  stat->m_rows_sent += state->m_rows_sent;
  stat->m_rows_examined += state->m_rows_examined;
  stat->m_created_tmp_disk_tables += state->m_created_tmp_disk_tables;
  stat->m_created_tmp_tables += state->m_created_tmp_tables;
  stat->m_select_full_join += state->m_select_full_join;
  stat->m_select_full_range_join += state->m_select_full_range_join;
  stat->m_select_range += state->m_select_range;
  stat->m_select_range_check += state->m_select_range_check;
  stat->m_select_scan += state->m_select_scan;
  stat->m_sort_merge_passes += state->m_sort_merge_passes;
  stat->m_sort_range += state->m_sort_range;
  stat->m_sort_rows += state->m_sort_rows;
  stat->m_sort_scan += state->m_sort_scan;
  stat->m_no_index_used += state->m_no_index_used;
  stat->m_no_good_index_used += state->m_no_good_index_used;

  if (digest_stat != NULL) {
    bool new_max_wait = false;

    digest_stat->m_stat.mark_used();

    if (flags & STATE_FLAG_TIMED) {
      digest_stat->m_stat.aggregate_value(wait_time);

      /* Update the digest sample if it's a new maximum. */
      if (wait_time > digest_stat->get_sample_timer_wait()) {
        new_max_wait = true;
      }
      time_normalizer *normalizer = time_normalizer::get_statement();
      ulong bucket_index = normalizer->bucket_index(wait_time);

      /* Update digest histogram. */
      digest_stat->m_histogram.increment_bucket(bucket_index);

      /* Update global histogram. */
      global_statements_histogram.increment_bucket(bucket_index);
    } else {
      digest_stat->m_stat.aggregate_counted();
    }

    if (state->m_query_sample_length != 0) {
      /* Get a new query sample if:
         - This is the first query sample, or
         - The wait time is a new maximum, or
         - The last query sample age exceeds the maximum age.
      */
      bool get_sample_query = (digest_stat->m_query_sample_length == 0);

      if (!get_sample_query) {
        get_sample_query = new_max_wait;

        if (!get_sample_query) {
          /* Check the query sample age. */
          if (pfs_param.m_max_digest_sample_age > 0) {
            /* Comparison in micro seconds. */
            get_sample_query = (digest_stat->get_sample_age() >
                                pfs_param.m_max_digest_sample_age * 1000000);
          }
        }
      }

      /* Update the query sample. */
      if (get_sample_query) {
        /* Get exclusive access otherwise abort. */
        if (digest_stat->inc_sample_ref() == 0) {
          digest_stat->set_sample_timer_wait(wait_time);
          DBUG_ASSERT(digest_stat->m_query_sample != NULL);
          memcpy(digest_stat->m_query_sample, state->m_query_sample,
                 state->m_query_sample_length);
          digest_stat->m_query_sample_length = state->m_query_sample_length;
          digest_stat->m_query_sample_cs_number = state->m_cs_number;
          digest_stat->m_query_sample_truncated =
              state->m_query_sample_truncated;
          digest_stat->m_query_sample_seen = digest_stat->m_last_seen;
        }
        digest_stat->dec_sample_ref();
      }
    }

    digest_stat->m_stat.m_lock_time += state->m_lock_time;
    digest_stat->m_stat.m_rows_sent += state->m_rows_sent;
    digest_stat->m_stat.m_rows_examined += state->m_rows_examined;
    digest_stat->m_stat.m_created_tmp_disk_tables +=
        state->m_created_tmp_disk_tables;
    digest_stat->m_stat.m_created_tmp_tables += state->m_created_tmp_tables;
    digest_stat->m_stat.m_select_full_join += state->m_select_full_join;
    digest_stat->m_stat.m_select_full_range_join +=
        state->m_select_full_range_join;
    digest_stat->m_stat.m_select_range += state->m_select_range;
    digest_stat->m_stat.m_select_range_check += state->m_select_range_check;
    digest_stat->m_stat.m_select_scan += state->m_select_scan;
    digest_stat->m_stat.m_sort_merge_passes += state->m_sort_merge_passes;
    digest_stat->m_stat.m_sort_range += state->m_sort_range;
    digest_stat->m_stat.m_sort_rows += state->m_sort_rows;
    digest_stat->m_stat.m_sort_scan += state->m_sort_scan;
    digest_stat->m_stat.m_no_index_used += state->m_no_index_used;
    digest_stat->m_stat.m_no_good_index_used += state->m_no_good_index_used;
  } else {
    if (flags & STATE_FLAG_TIMED) {
      time_normalizer *normalizer = time_normalizer::get_statement();
      ulong bucket_index = normalizer->bucket_index(wait_time);

      /* Update global histogram. */
      global_statements_histogram.increment_bucket(bucket_index);
    }
  }

  if (pfs_program != NULL) {
    PFS_statement_stat *sub_stmt_stat = NULL;
    sub_stmt_stat = &pfs_program->m_stmt_stat;
    if (sub_stmt_stat != NULL) {
      sub_stmt_stat->mark_used();

      if (flags & STATE_FLAG_TIMED) {
        sub_stmt_stat->aggregate_value(wait_time);
      } else {
        sub_stmt_stat->aggregate_counted();
      }

      sub_stmt_stat->m_lock_time += state->m_lock_time;
      sub_stmt_stat->m_rows_sent += state->m_rows_sent;
      sub_stmt_stat->m_rows_examined += state->m_rows_examined;
      sub_stmt_stat->m_created_tmp_disk_tables +=
          state->m_created_tmp_disk_tables;
      sub_stmt_stat->m_created_tmp_tables += state->m_created_tmp_tables;
      sub_stmt_stat->m_select_full_join += state->m_select_full_join;
      sub_stmt_stat->m_select_full_range_join +=
          state->m_select_full_range_join;
      sub_stmt_stat->m_select_range += state->m_select_range;
      sub_stmt_stat->m_select_range_check += state->m_select_range_check;
      sub_stmt_stat->m_select_scan += state->m_select_scan;
      sub_stmt_stat->m_sort_merge_passes += state->m_sort_merge_passes;
      sub_stmt_stat->m_sort_range += state->m_sort_range;
      sub_stmt_stat->m_sort_rows += state->m_sort_rows;
      sub_stmt_stat->m_sort_scan += state->m_sort_scan;
      sub_stmt_stat->m_no_index_used += state->m_no_index_used;
      sub_stmt_stat->m_no_good_index_used += state->m_no_good_index_used;
    }
  }

  if (pfs_prepared_stmt != NULL) {
    if (state->m_in_prepare) {
      PFS_single_stat *prepared_stmt_stat = NULL;
      prepared_stmt_stat = &pfs_prepared_stmt->m_prepare_stat;
      if (prepared_stmt_stat != NULL) {
        if (flags & STATE_FLAG_TIMED) {
          prepared_stmt_stat->aggregate_value(wait_time);
        } else {
          prepared_stmt_stat->aggregate_counted();
        }
      }
    } else {
      PFS_statement_stat *prepared_stmt_stat = NULL;
      prepared_stmt_stat = &pfs_prepared_stmt->m_execute_stat;
      if (prepared_stmt_stat != NULL) {
        if (flags & STATE_FLAG_TIMED) {
          prepared_stmt_stat->aggregate_value(wait_time);
        } else {
          prepared_stmt_stat->aggregate_counted();
        }

        prepared_stmt_stat->m_lock_time += state->m_lock_time;
        prepared_stmt_stat->m_rows_sent += state->m_rows_sent;
        prepared_stmt_stat->m_rows_examined += state->m_rows_examined;
        prepared_stmt_stat->m_created_tmp_disk_tables +=
            state->m_created_tmp_disk_tables;
        prepared_stmt_stat->m_created_tmp_tables += state->m_created_tmp_tables;
        prepared_stmt_stat->m_select_full_join += state->m_select_full_join;
        prepared_stmt_stat->m_select_full_range_join +=
            state->m_select_full_range_join;
        prepared_stmt_stat->m_select_range += state->m_select_range;
        prepared_stmt_stat->m_select_range_check += state->m_select_range_check;
        prepared_stmt_stat->m_select_scan += state->m_select_scan;
        prepared_stmt_stat->m_sort_merge_passes += state->m_sort_merge_passes;
        prepared_stmt_stat->m_sort_range += state->m_sort_range;
        prepared_stmt_stat->m_sort_rows += state->m_sort_rows;
        prepared_stmt_stat->m_sort_scan += state->m_sort_scan;
        prepared_stmt_stat->m_no_index_used += state->m_no_index_used;
        prepared_stmt_stat->m_no_good_index_used += state->m_no_good_index_used;
      }
    }
  }

  state->m_query_sample_length = 0;
  state->m_query_sample = nullptr;

  PFS_statement_stat *sub_stmt_stat = NULL;
  if (pfs_program != NULL) {
    sub_stmt_stat = &pfs_program->m_stmt_stat;
  }

  PFS_statement_stat *prepared_stmt_stat = NULL;
  if (pfs_prepared_stmt != NULL && !state->m_in_prepare) {
    prepared_stmt_stat = &pfs_prepared_stmt->m_execute_stat;
  }

  switch (da->status()) {
    case Diagnostics_area::DA_EMPTY:
      break;
    case Diagnostics_area::DA_OK:
      stat->m_rows_affected += da->affected_rows();
      stat->m_warning_count += da->last_statement_cond_count();
      if (digest_stat != NULL) {
        digest_stat->m_stat.m_rows_affected += da->affected_rows();
        digest_stat->m_stat.m_warning_count += da->last_statement_cond_count();
      }
      if (sub_stmt_stat != NULL) {
        sub_stmt_stat->m_rows_affected += da->affected_rows();
        sub_stmt_stat->m_warning_count += da->last_statement_cond_count();
      }
      if (prepared_stmt_stat != NULL) {
        prepared_stmt_stat->m_rows_affected += da->affected_rows();
        prepared_stmt_stat->m_warning_count += da->last_statement_cond_count();
      }
      break;
    case Diagnostics_area::DA_EOF:
      stat->m_warning_count += da->last_statement_cond_count();
      if (digest_stat != NULL) {
        digest_stat->m_stat.m_warning_count += da->last_statement_cond_count();
      }
      if (sub_stmt_stat != NULL) {
        sub_stmt_stat->m_warning_count += da->last_statement_cond_count();
      }
      if (prepared_stmt_stat != NULL) {
        prepared_stmt_stat->m_warning_count += da->last_statement_cond_count();
      }
      break;
    case Diagnostics_area::DA_ERROR:
      stat->m_error_count++;
      if (digest_stat != NULL) {
        digest_stat->m_stat.m_error_count++;
      }
      if (sub_stmt_stat != NULL) {
        sub_stmt_stat->m_error_count++;
      }
      if (prepared_stmt_stat != NULL) {
        prepared_stmt_stat->m_error_count++;
      }
      break;
    case Diagnostics_area::DA_DISABLED:
      break;
  }
}

static inline enum_object_type sp_type_to_object_type(uint sp_type) {
  enum_sp_type value = to_sp_type(sp_type);

  switch (value) {
    case enum_sp_type::FUNCTION:
      return OBJECT_TYPE_FUNCTION;
    case enum_sp_type::PROCEDURE:
      return OBJECT_TYPE_PROCEDURE;
    case enum_sp_type::TRIGGER:
      return OBJECT_TYPE_TRIGGER;
    case enum_sp_type::EVENT:
      return OBJECT_TYPE_EVENT;
    default:
      DBUG_ASSERT(false);
      /* Dead code */
      return NO_OBJECT_TYPE;
  }
}

/**
  Implementation of the stored program instrumentation interface.
  @sa PSI_v1::get_sp_share.
*/
static PSI_sp_share *pfs_get_sp_share_v1(uint sp_type, const char *schema_name,
                                         uint schema_name_length,
                                         const char *object_name,
                                         uint object_name_length) {
  PFS_thread *pfs_thread = my_thread_get_THR_PFS();
  if (unlikely(pfs_thread == NULL)) {
    return NULL;
  }

  if (object_name_length > COL_OBJECT_NAME_SIZE) {
    object_name_length = COL_OBJECT_NAME_SIZE;
  }
  if (schema_name_length > COL_OBJECT_SCHEMA_SIZE) {
    schema_name_length = COL_OBJECT_SCHEMA_SIZE;
  }

  PFS_program *pfs_program;
  pfs_program = find_or_create_program(
      pfs_thread, sp_type_to_object_type(sp_type), object_name,
      object_name_length, schema_name, schema_name_length);

  return reinterpret_cast<PSI_sp_share *>(pfs_program);
}

static void pfs_release_sp_share_v1(PSI_sp_share *) {
  /* Unused */
  return;
}

static PSI_sp_locker *pfs_start_sp_v1(PSI_sp_locker_state *state,
                                      PSI_sp_share *sp_share) {
  DBUG_ASSERT(state != NULL);
  if (!flag_global_instrumentation) {
    return NULL;
  }

  if (flag_thread_instrumentation) {
    PFS_thread *pfs_thread = my_thread_get_THR_PFS();
    if (unlikely(pfs_thread == NULL)) {
      return NULL;
    }
    if (!pfs_thread->m_enabled) {
      return NULL;
    }
  }

  /*
    sp share might be null in case when stat array is full and no new
    stored program stats are being inserted into it.
  */
  PFS_program *pfs_program = reinterpret_cast<PFS_program *>(sp_share);
  if (pfs_program == NULL || !pfs_program->m_enabled) {
    return NULL;
  }

  state->m_flags = 0;

  if (pfs_program->m_timed) {
    state->m_timer_start = get_statement_timer();
    state->m_flags |= STATE_FLAG_TIMED;
  }

  state->m_sp_share = sp_share;

  return reinterpret_cast<PSI_sp_locker *>(state);
}

static void pfs_end_sp_v1(PSI_sp_locker *locker) {
  PSI_sp_locker_state *state = reinterpret_cast<PSI_sp_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end;
  ulonglong wait_time;

  PFS_program *pfs_program = reinterpret_cast<PFS_program *>(state->m_sp_share);
  PFS_sp_stat *stat = &pfs_program->m_sp_stat;

  if (state->m_flags & STATE_FLAG_TIMED) {
    timer_end = get_statement_timer();
    wait_time = timer_end - state->m_timer_start;

    /* Now use this timer_end and wait_time for timing information. */
    stat->aggregate_value(wait_time);
  } else {
    stat->aggregate_counted();
  }
}

static void pfs_drop_sp_v1(uint sp_type, const char *schema_name,
                           uint schema_name_length, const char *object_name,
                           uint object_name_length) {
  PFS_thread *pfs_thread = my_thread_get_THR_PFS();
  if (unlikely(pfs_thread == NULL)) {
    return;
  }

  if (object_name_length > COL_OBJECT_NAME_SIZE) {
    object_name_length = COL_OBJECT_NAME_SIZE;
  }
  if (schema_name_length > COL_OBJECT_SCHEMA_SIZE) {
    schema_name_length = COL_OBJECT_SCHEMA_SIZE;
  }

  drop_program(pfs_thread, sp_type_to_object_type(sp_type), object_name,
               object_name_length, schema_name, schema_name_length);
}

PSI_transaction_locker *pfs_get_thread_transaction_locker_v1(
    PSI_transaction_locker_state *state, const void *xid,
    const ulonglong *trxid, int isolation_level, bool read_only,
    bool autocommit) {
  DBUG_ASSERT(state != NULL);

  if (!flag_global_instrumentation) {
    return NULL;
  }

  if (!global_transaction_class.m_enabled) {
    return NULL;
  }

  uint flags;

  if (flag_thread_instrumentation) {
    PFS_thread *pfs_thread = my_thread_get_THR_PFS();
    if (unlikely(pfs_thread == NULL)) {
      return NULL;
    }
    if (!pfs_thread->m_enabled) {
      return NULL;
    }
    state->m_thread = reinterpret_cast<PSI_thread *>(pfs_thread);
    flags = STATE_FLAG_THREAD;

    if (global_transaction_class.m_timed) {
      flags |= STATE_FLAG_TIMED;
    }

    if (flag_events_transactions_current) {
      ulonglong event_id = pfs_thread->m_event_id++;

      PFS_events_transactions *pfs = &pfs_thread->m_transaction_current;
      pfs->m_thread_internal_id = pfs_thread->m_thread_internal_id;
      pfs->m_event_id = event_id;
      pfs->m_event_type = EVENT_TYPE_TRANSACTION;
      pfs->m_end_event_id = 0;
      pfs->m_class = &global_transaction_class;
      pfs->m_timer_start = 0;
      pfs->m_timer_end = 0;
      if (xid != NULL) {
        pfs->m_xid = *(PSI_xid *)xid;
      }
      pfs->m_xa = false;
      pfs->m_xa_state = TRANS_STATE_XA_NOTR;
      pfs->m_trxid = (trxid == NULL) ? 0 : *trxid;
      pfs->m_isolation_level = (enum_isolation_level)isolation_level;
      pfs->m_read_only = read_only;
      pfs->m_autocommit = autocommit;
      pfs->m_savepoint_count = 0;
      pfs->m_rollback_to_savepoint_count = 0;
      pfs->m_release_savepoint_count = 0;

      uint statements_count = pfs_thread->m_events_statements_count;
      if (statements_count > 0) {
        PFS_events_statements *pfs_statement =
            &pfs_thread->m_statement_stack[statements_count - 1];
        pfs->m_nesting_event_id = pfs_statement->m_event_id;
        pfs->m_nesting_event_type = pfs_statement->m_event_type;
      } else {
        pfs->m_nesting_event_id = 0;
        /* pfs->m_nesting_event_type not used when m_nesting_event_id is 0 */
      }

      state->m_transaction = pfs;
      flags |= STATE_FLAG_EVENT;
    }
  } else {
    if (global_transaction_class.m_timed) {
      flags = STATE_FLAG_TIMED;
    } else {
      flags = 0;
    }
  }

  state->m_class = &global_transaction_class;
  state->m_flags = flags;
  state->m_autocommit = autocommit;
  state->m_read_only = read_only;
  state->m_savepoint_count = 0;
  state->m_rollback_to_savepoint_count = 0;
  state->m_release_savepoint_count = 0;

  return reinterpret_cast<PSI_transaction_locker *>(state);
}

void pfs_start_transaction_v1(PSI_transaction_locker *locker,
                              const char *src_file, uint src_line) {
  PSI_transaction_locker_state *state =
      reinterpret_cast<PSI_transaction_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  uint flags = state->m_flags;
  ulonglong timer_start = 0;

  if (flags & STATE_FLAG_TIMED) {
    timer_start = get_transaction_timer();
    state->m_timer_start = timer_start;
  }

  if (flags & STATE_FLAG_EVENT) {
    PFS_events_transactions *pfs =
        reinterpret_cast<PFS_events_transactions *>(state->m_transaction);
    DBUG_ASSERT(pfs != NULL);

    pfs->m_timer_start = timer_start;
    pfs->m_source_file = src_file;
    pfs->m_source_line = src_line;
    pfs->m_state = TRANS_STATE_ACTIVE;
    pfs->m_sid.clear();
    pfs->m_gtid_spec.set_automatic();
  }
}

void pfs_set_transaction_gtid_v1(PSI_transaction_locker *locker,
                                 const void *sid, const void *gtid_spec) {
  PSI_transaction_locker_state *state =
      reinterpret_cast<PSI_transaction_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);
  DBUG_ASSERT(sid != NULL);
  DBUG_ASSERT(gtid_spec != NULL);

  if (state->m_flags & STATE_FLAG_EVENT) {
    PFS_events_transactions *pfs =
        reinterpret_cast<PFS_events_transactions *>(state->m_transaction);
    DBUG_ASSERT(pfs != NULL);
    pfs->m_sid = *(rpl_sid *)sid;
    pfs->m_gtid_spec = *(Gtid_specification *)gtid_spec;
  }
}

void pfs_set_transaction_xid_v1(PSI_transaction_locker *locker, const void *xid,
                                int xa_state) {
  PSI_transaction_locker_state *state =
      reinterpret_cast<PSI_transaction_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  if (state->m_flags & STATE_FLAG_EVENT) {
    PFS_events_transactions *pfs =
        reinterpret_cast<PFS_events_transactions *>(state->m_transaction);
    DBUG_ASSERT(pfs != NULL);
    DBUG_ASSERT(xid != NULL);

    pfs->m_xid = *(PSI_xid *)xid;
    pfs->m_xa_state = (enum_xa_transaction_state)xa_state;
    pfs->m_xa = true;
  }
  return;
}

void pfs_set_transaction_xa_state_v1(PSI_transaction_locker *locker,
                                     int xa_state) {
  PSI_transaction_locker_state *state =
      reinterpret_cast<PSI_transaction_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  if (state->m_flags & STATE_FLAG_EVENT) {
    PFS_events_transactions *pfs =
        reinterpret_cast<PFS_events_transactions *>(state->m_transaction);
    DBUG_ASSERT(pfs != NULL);

    pfs->m_xa_state = (enum_xa_transaction_state)xa_state;
    pfs->m_xa = true;
  }
  return;
}

void pfs_set_transaction_trxid_v1(PSI_transaction_locker *locker,
                                  const ulonglong *trxid) {
  DBUG_ASSERT(trxid != NULL);

  PSI_transaction_locker_state *state =
      reinterpret_cast<PSI_transaction_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  if (state->m_flags & STATE_FLAG_EVENT) {
    PFS_events_transactions *pfs =
        reinterpret_cast<PFS_events_transactions *>(state->m_transaction);
    DBUG_ASSERT(pfs != NULL);

    if (pfs->m_trxid == 0) {
      pfs->m_trxid = *trxid;
    }
  }
}

#define INC_TRANSACTION_ATTR_BODY(LOCKER, ATTR, VALUE)                       \
  PSI_transaction_locker_state *state;                                       \
  state = reinterpret_cast<PSI_transaction_locker_state *>(LOCKER);          \
  if (unlikely(state == NULL)) {                                             \
    return;                                                                  \
  }                                                                          \
  state->ATTR += VALUE;                                                      \
  if (state->m_flags & STATE_FLAG_EVENT) {                                   \
    PFS_events_transactions *pfs;                                            \
    pfs = reinterpret_cast<PFS_events_transactions *>(state->m_transaction); \
    DBUG_ASSERT(pfs != NULL);                                                \
    pfs->ATTR += VALUE;                                                      \
  }                                                                          \
  return;

void pfs_inc_transaction_savepoints_v1(PSI_transaction_locker *locker,
                                       ulong count) {
  INC_TRANSACTION_ATTR_BODY(locker, m_savepoint_count, count);
}

void pfs_inc_transaction_rollback_to_savepoint_v1(
    PSI_transaction_locker *locker, ulong count) {
  INC_TRANSACTION_ATTR_BODY(locker, m_rollback_to_savepoint_count, count);
}

void pfs_inc_transaction_release_savepoint_v1(PSI_transaction_locker *locker,
                                              ulong count) {
  INC_TRANSACTION_ATTR_BODY(locker, m_release_savepoint_count, count);
}

void pfs_end_transaction_v1(PSI_transaction_locker *locker, bool commit) {
  PSI_transaction_locker_state *state =
      reinterpret_cast<PSI_transaction_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end = 0;
  ulonglong wait_time = 0;
  uint flags = state->m_flags;

  if (flags & STATE_FLAG_TIMED) {
    timer_end = get_transaction_timer();
    wait_time = timer_end - state->m_timer_start;
  }

  PFS_transaction_stat *stat;

  if (flags & STATE_FLAG_THREAD) {
    PFS_thread *pfs_thread = reinterpret_cast<PFS_thread *>(state->m_thread);
    DBUG_ASSERT(pfs_thread != NULL);

    /* Aggregate to EVENTS_TRANSACTIONS_SUMMARY_BY_THREAD_BY_EVENT_NAME */
    stat =
        &pfs_thread
             ->write_instr_class_transactions_stats()[GLOBAL_TRANSACTION_INDEX];

    if (flags & STATE_FLAG_EVENT) {
      PFS_events_transactions *pfs =
          reinterpret_cast<PFS_events_transactions *>(state->m_transaction);
      DBUG_ASSERT(pfs != NULL);

      /* events_transactions_current may have been cleared while the transaction
       * was active */
      if (unlikely(pfs->m_class == NULL)) {
        return;
      }

      pfs->m_timer_end = timer_end;
      pfs->m_end_event_id = pfs_thread->m_event_id;

      pfs->m_state = (commit ? TRANS_STATE_COMMITTED : TRANS_STATE_ROLLED_BACK);

      if (pfs->m_xa) {
        pfs->m_xa_state =
            (commit ? TRANS_STATE_XA_COMMITTED : TRANS_STATE_XA_ROLLBACK_ONLY);
      }

      if (pfs_thread->m_flag_events_transactions_history) {
        insert_events_transactions_history(pfs_thread, pfs);
      }
      if (pfs_thread->m_flag_events_transactions_history_long) {
        insert_events_transactions_history_long(pfs);
      }
    }
  } else {
    /* Aggregate to EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME */
    stat = &global_transaction_stat;
  }

  if (flags & STATE_FLAG_TIMED) {
    /* Aggregate to EVENTS_TRANSACTIONS_SUMMARY_..._BY_EVENT_NAME (timed) */
    if (state->m_read_only) {
      stat->m_read_only_stat.aggregate_value(wait_time);
    } else {
      stat->m_read_write_stat.aggregate_value(wait_time);
    }
  } else {
    /* Aggregate to EVENTS_TRANSACTIONS_SUMMARY_..._BY_EVENT_NAME (counted) */
    if (state->m_read_only) {
      stat->m_read_only_stat.aggregate_counted();
    } else {
      stat->m_read_write_stat.aggregate_counted();
    }
  }

  stat->m_savepoint_count += state->m_savepoint_count;
  stat->m_rollback_to_savepoint_count += state->m_rollback_to_savepoint_count;
  stat->m_release_savepoint_count += state->m_release_savepoint_count;
}

/**
  Implementation of the socket instrumentation interface.
  @sa PSI_v1::end_socket_wait.
*/
void pfs_end_socket_wait_v1(PSI_socket_locker *locker, size_t byte_count) {
  PSI_socket_locker_state *state =
      reinterpret_cast<PSI_socket_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  PFS_socket *socket = reinterpret_cast<PFS_socket *>(state->m_socket);
  DBUG_ASSERT(socket != NULL);

  ulonglong timer_end = 0;
  ulonglong wait_time = 0;
  PFS_byte_stat *byte_stat;
  uint flags = state->m_flags;
  size_t bytes = ((int)byte_count > -1 ? byte_count : 0);

  switch (state->m_operation) {
    /* Group read operations */
    case PSI_SOCKET_RECV:
    case PSI_SOCKET_RECVFROM:
    case PSI_SOCKET_RECVMSG:
      byte_stat = &socket->m_socket_stat.m_io_stat.m_read;
      break;
    /* Group write operations */
    case PSI_SOCKET_SEND:
    case PSI_SOCKET_SENDTO:
    case PSI_SOCKET_SENDMSG:
      byte_stat = &socket->m_socket_stat.m_io_stat.m_write;
      break;
    /* Group remaining operations as miscellaneous */
    case PSI_SOCKET_CONNECT:
    case PSI_SOCKET_CREATE:
    case PSI_SOCKET_BIND:
    case PSI_SOCKET_SEEK:
    case PSI_SOCKET_OPT:
    case PSI_SOCKET_STAT:
    case PSI_SOCKET_SHUTDOWN:
    case PSI_SOCKET_SELECT:
    case PSI_SOCKET_CLOSE:
      byte_stat = &socket->m_socket_stat.m_io_stat.m_misc;
      break;
    default:
      DBUG_ASSERT(false);
      byte_stat = NULL;
      break;
  }

  /* Aggregation for EVENTS_WAITS_SUMMARY_BY_INSTANCE */
  if (flags & STATE_FLAG_TIMED) {
    timer_end = get_wait_timer();
    wait_time = timer_end - state->m_timer_start;

    /* Aggregate to the socket instrument for now (timed) */
    byte_stat->aggregate(wait_time, bytes);
  } else {
    /* Aggregate to the socket instrument (event count and byte count) */
    byte_stat->aggregate_counted(bytes);
  }

  /* Aggregate to EVENTS_WAITS_HISTORY and EVENTS_WAITS_HISTORY_LONG */
  if (flags & STATE_FLAG_EVENT) {
    PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);
    DBUG_ASSERT(thread != NULL);
    PFS_events_waits *wait =
        reinterpret_cast<PFS_events_waits *>(state->m_wait);
    DBUG_ASSERT(wait != NULL);

    wait->m_timer_end = timer_end;
    wait->m_end_event_id = thread->m_event_id;
    wait->m_number_of_bytes = bytes;

    if (thread->m_flag_events_waits_history) {
      insert_events_waits_history(thread, wait);
    }
    if (thread->m_flag_events_waits_history_long) {
      insert_events_waits_history_long(wait);
    }
    thread->m_events_waits_current--;

    DBUG_ASSERT(wait == thread->m_events_waits_current);
  }
}

void pfs_set_socket_state_v1(PSI_socket *socket, PSI_socket_state state) {
  DBUG_ASSERT((state == PSI_SOCKET_STATE_IDLE) ||
              (state == PSI_SOCKET_STATE_ACTIVE));
  PFS_socket *pfs = reinterpret_cast<PFS_socket *>(socket);
  DBUG_ASSERT(pfs != NULL);
  DBUG_ASSERT(pfs->m_idle || (state == PSI_SOCKET_STATE_IDLE));
  DBUG_ASSERT(!pfs->m_idle || (state == PSI_SOCKET_STATE_ACTIVE));
  pfs->m_idle = (state == PSI_SOCKET_STATE_IDLE);
}

/**
  Set socket descriptor and address info.
*/
void pfs_set_socket_info_v1(PSI_socket *socket, const my_socket *fd,
                            const struct sockaddr *addr, socklen_t addr_len) {
  PFS_socket *pfs = reinterpret_cast<PFS_socket *>(socket);
  DBUG_ASSERT(pfs != NULL);

  /** Set socket descriptor */
  if (fd != NULL) {
    pfs->m_fd = (uint)*fd;
  }

  /** Set raw socket address and length */
  if (likely(addr != NULL && addr_len > 0)) {
    pfs->m_addr_len = addr_len;

    /** Restrict address length to size of struct */
    if (unlikely(pfs->m_addr_len > sizeof(sockaddr_storage))) {
      pfs->m_addr_len = sizeof(struct sockaddr_storage);
    }

    memcpy(&pfs->m_sock_addr, addr, pfs->m_addr_len);
  }
}

/**
  Implementation of the socket instrumentation interface.
  @sa PSI_v1::set_socket_info.
*/
void pfs_set_socket_thread_owner_v1(PSI_socket *socket) {
  PFS_socket *pfs_socket = reinterpret_cast<PFS_socket *>(socket);
  DBUG_ASSERT(pfs_socket != NULL);
  PFS_thread *pfs_thread = my_thread_get_THR_PFS();
  pfs_socket->m_thread_owner = pfs_thread;

  if (pfs_thread)  // TODO use set_thread_ip_addr()
  {
    pfs_thread->m_sock_addr_len = pfs_socket->m_addr_len;
    if (pfs_thread->m_sock_addr_len > 0) {
      memcpy(&pfs_thread->m_sock_addr, &pfs_socket->m_sock_addr,
             pfs_socket->m_addr_len);
    }
  }
}

struct PSI_digest_locker *pfs_digest_start_v1(PSI_statement_locker *locker) {
  PSI_statement_locker_state *statement_state;
  statement_state = reinterpret_cast<PSI_statement_locker_state *>(locker);
  DBUG_ASSERT(statement_state != NULL);

  if (statement_state->m_discarded) {
    return NULL;
  }

  if (statement_state->m_flags & STATE_FLAG_DIGEST) {
    return reinterpret_cast<PSI_digest_locker *>(locker);
  }

  return NULL;
}

void pfs_digest_end_v1(PSI_digest_locker *locker,
                       const sql_digest_storage *digest) {
  PSI_statement_locker_state *state;
  state = reinterpret_cast<PSI_statement_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);
  DBUG_ASSERT(digest != NULL);

  if (state->m_discarded) {
    return;
  }

  if (state->m_flags & STATE_FLAG_DIGEST) {
    /* TODO: pfs_digest_end_v1() has side effects here, to document better */
    sql_digest_storage *update_digest =
        const_cast<sql_digest_storage *>(digest);

    /* Compute digest hash of the tokens received. */
    compute_digest_hash(digest, update_digest->m_hash);

    state->m_digest = digest;

    uint req_flags = STATE_FLAG_THREAD | STATE_FLAG_EVENT;

    if ((state->m_flags & req_flags) == req_flags) {
      PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);
      DBUG_ASSERT(thread != NULL);
      PFS_events_statements *pfs =
          reinterpret_cast<PFS_events_statements *>(state->m_statement);
      DBUG_ASSERT(pfs != NULL);

      pfs_dirty_state dirty_state;
      thread->m_stmt_lock.allocated_to_dirty(&dirty_state);

      /*
        The following columns in events_statement_current:
        - DIGEST,
        - DIGEST_TEXT
        are computed from the digest storage.
      */
      pfs->m_digest_storage.copy(digest);

      thread->m_stmt_lock.dirty_to_allocated(&dirty_state);
    }
  }
}

static PSI_prepared_stmt *pfs_create_prepared_stmt_v1(
    void *identity, uint stmt_id, PSI_statement_locker *locker,
    const char *stmt_name, size_t stmt_name_length, const char *sql_text,
    size_t sql_text_length) {
  PSI_statement_locker_state *state =
      reinterpret_cast<PSI_statement_locker_state *>(locker);
  PFS_events_statements *pfs_stmt =
      reinterpret_cast<PFS_events_statements *>(state->m_statement);
  PFS_program *pfs_program =
      reinterpret_cast<PFS_program *>(state->m_parent_sp_share);

  PFS_thread *pfs_thread = my_thread_get_THR_PFS();
  if (unlikely(pfs_thread == NULL)) {
    return NULL;
  }

  if (sql_text_length > COL_INFO_SIZE) {
    sql_text_length = COL_INFO_SIZE;
  }

  PFS_prepared_stmt *pfs = create_prepared_stmt(
      identity, pfs_thread, pfs_program, pfs_stmt, stmt_id, stmt_name,
      stmt_name_length, sql_text, sql_text_length);

  state->m_parent_prepared_stmt = reinterpret_cast<PSI_prepared_stmt *>(pfs);
  state->m_in_prepare = true;

  return reinterpret_cast<PSI_prepared_stmt *>(pfs);
}

static void pfs_execute_prepared_stmt_v1(PSI_statement_locker *locker,
                                         PSI_prepared_stmt *ps) {
  PSI_statement_locker_state *state =
      reinterpret_cast<PSI_statement_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  state->m_parent_prepared_stmt = ps;
  state->m_in_prepare = false;
}

static void pfs_destroy_prepared_stmt_v1(PSI_prepared_stmt *prepared_stmt) {
  PFS_prepared_stmt *pfs_prepared_stmt =
      reinterpret_cast<PFS_prepared_stmt *>(prepared_stmt);
  delete_prepared_stmt(pfs_prepared_stmt);
  return;
}

static void pfs_reprepare_prepared_stmt_v1(PSI_prepared_stmt *prepared_stmt) {
  PFS_prepared_stmt *pfs_prepared_stmt =
      reinterpret_cast<PFS_prepared_stmt *>(prepared_stmt);
  PFS_single_stat *prepared_stmt_stat = &pfs_prepared_stmt->m_reprepare_stat;

  if (prepared_stmt_stat != NULL) {
    prepared_stmt_stat->aggregate_counted();
  }
  return;
}

void pfs_set_prepared_stmt_text_v1(PSI_prepared_stmt *prepared_stmt,
                                   const char *text, uint text_len) {
  PFS_prepared_stmt *pfs_prepared_stmt =
      reinterpret_cast<PFS_prepared_stmt *>(prepared_stmt);
  DBUG_ASSERT(pfs_prepared_stmt != NULL);

  uint max_len = COL_INFO_SIZE;
  if (text_len > max_len) {
    text_len = max_len;
  }

  memcpy(pfs_prepared_stmt->m_sqltext, text, text_len);
  pfs_prepared_stmt->m_sqltext_length = text_len;

  return;
}

/**
  Implementation of the thread attribute connection interface
  @sa PSI_v1::set_thread_connect_attr.
*/
int pfs_set_thread_connect_attrs_v1(const char *buffer, uint length,
                                    const void *from_cs) {
  PFS_thread *thd = my_thread_get_THR_PFS();

  DBUG_ASSERT(buffer != NULL);

  if (likely(thd != NULL) && session_connect_attrs_size_per_thread > 0) {
    pfs_dirty_state dirty_state;
    const CHARSET_INFO *cs = static_cast<const CHARSET_INFO *>(from_cs);

    thd->m_session_lock.allocated_to_dirty(&dirty_state);

    /* copy from the input buffer as much as we can fit */
    uint copy_size, lost;

    if (length > session_connect_attrs_longest_seen) {
      session_connect_attrs_longest_seen = length;
    }

    if (length <= session_connect_attrs_size_per_thread) {
      copy_size = length;
      lost = 0;

      memcpy(thd->m_session_connect_attrs, buffer, copy_size);
    } else {
      copy_size = session_connect_attrs_size_per_thread;
      lost = length - copy_size;

      /* create warning */
      const char *key = "_truncated";
      char val[7], warn_buf[64];
      size_t key_len = strlen(key), warning_size, val_len;

      /* lost characters, factoring in warning attribute key/value */
      lost += key_len + 1 + sizeof(val);

      /* we want UTF-8, so my_convert() is not necessary here. */
      val_len = snprintf(val, sizeof(val) - 1, "%d", lost);

      warning_size = snprintf(warn_buf, sizeof(warn_buf), "%c%s%c%s",
                              int(key_len), key, int(val_len), val);

      if (warning_size <= copy_size) {
        size_t left = copy_size - warning_size;
        memcpy(thd->m_session_connect_attrs, warn_buf, warning_size);
        if (left > 0) {
          memcpy(thd->m_session_connect_attrs + warning_size, buffer, left);
        }
      }
    }

    thd->m_session_connect_attrs_length = copy_size;
    thd->m_session_connect_attrs_cs_number = cs->number;
    thd->m_session_lock.dirty_to_allocated(&dirty_state);

    if (copy_size == length) {
      return 0;
    }

    session_connect_attrs_lost++;
    return lost;
  }
  return 0;
}

/**
  Implementation of the get event id interface
  @sa PSI_v1::get_thread_event_id.
*/
void pfs_get_thread_event_id_v1(ulonglong *internal_thread_id,
                                ulonglong *event_id) {
  PFS_thread *pfs = my_thread_get_THR_PFS();

  if (pfs != NULL) {
    *internal_thread_id = pfs->m_thread_internal_id;
    *event_id = pfs->m_event_id;
  } else {
    *internal_thread_id = 0;
    *event_id = 0;
  }
}

void pfs_register_memory_v1(const char *category, PSI_memory_info_v1 *info,
                            int count) {
  REGISTER_BODY_V1(PSI_memory_key, memory_instrument_prefix,
                   register_memory_class);
}

PSI_memory_key pfs_memory_alloc_v1(PSI_memory_key key, size_t size,
                                   PSI_thread **owner) {
  PFS_thread **owner_thread = reinterpret_cast<PFS_thread **>(owner);
  DBUG_ASSERT(owner_thread != NULL);

  if (!flag_global_instrumentation) {
    *owner_thread = NULL;
    return PSI_NOT_INSTRUMENTED;
  }

  PFS_memory_class *klass = find_memory_class(key);
  if (klass == NULL) {
    *owner_thread = NULL;
    return PSI_NOT_INSTRUMENTED;
  }

  if (!klass->m_enabled) {
    *owner_thread = NULL;
    return PSI_NOT_INSTRUMENTED;
  }

  uint index = klass->m_event_name_index;

  if (flag_thread_instrumentation && !klass->is_global()) {
    PFS_thread *pfs_thread = my_thread_get_THR_PFS();
    if (unlikely(pfs_thread == NULL)) {
      *owner_thread = NULL;
      return PSI_NOT_INSTRUMENTED;
    }
    if (!pfs_thread->m_enabled) {
      *owner_thread = NULL;
      return PSI_NOT_INSTRUMENTED;
    }

    PFS_memory_safe_stat *event_name_array;
    PFS_memory_safe_stat *stat;
    PFS_memory_stat_delta delta_buffer;
    PFS_memory_stat_delta *delta;

    /* Aggregate to MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME */
    event_name_array = pfs_thread->write_instr_class_memory_stats();
    stat = &event_name_array[index];
    delta = stat->count_alloc(size, &delta_buffer);

    if (delta != NULL) {
      pfs_thread->carry_memory_stat_delta(delta, index);
    }

    /* Flag this memory as owned by the current thread. */
    *owner_thread = pfs_thread;
  } else {
    PFS_memory_shared_stat *event_name_array;
    PFS_memory_shared_stat *stat;

    /* Aggregate to MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME */
    event_name_array = global_instr_class_memory_array;
    stat = &event_name_array[index];
    stat->count_global_alloc(size);

    *owner_thread = NULL;
  }

  return key;
}

PSI_memory_key pfs_memory_realloc_v1(PSI_memory_key key, size_t old_size,
                                     size_t new_size, PSI_thread **owner) {
  PFS_thread **owner_thread_hdl = reinterpret_cast<PFS_thread **>(owner);
  DBUG_ASSERT(owner != NULL);

  PFS_memory_class *klass = find_memory_class(key);
  if (klass == NULL) {
    *owner_thread_hdl = NULL;
    return PSI_NOT_INSTRUMENTED;
  }

  uint index = klass->m_event_name_index;
  PFS_memory_stat_delta delta_buffer;
  PFS_memory_stat_delta *delta;

  if (flag_thread_instrumentation && !klass->is_global()) {
    PFS_thread *pfs_thread = my_thread_get_THR_PFS();
    if (likely(pfs_thread != NULL)) {
#ifdef PFS_PARANOID
      PFS_thread *owner_thread = *owner_thread_hdl;
      if (owner_thread != pfs_thread) {
        owner_thread = sanitize_thread(owner_thread);
        if (owner_thread != NULL) {
          report_memory_accounting_error("pfs_memory_realloc_v1", pfs_thread,
                                         old_size, klass, owner_thread);
        }
      }
#endif /* PFS_PARANOID */

      PFS_memory_safe_stat *event_name_array;
      PFS_memory_safe_stat *stat;

      /* Aggregate to MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME */
      event_name_array = pfs_thread->write_instr_class_memory_stats();
      stat = &event_name_array[index];

      if (flag_global_instrumentation && klass->m_enabled) {
        delta = stat->count_realloc(old_size, new_size, &delta_buffer);
        *owner_thread_hdl = pfs_thread;
      } else {
        delta = stat->count_free(old_size, &delta_buffer);
        *owner_thread_hdl = NULL;
        key = PSI_NOT_INSTRUMENTED;
      }

      if (delta != NULL) {
        pfs_thread->carry_memory_stat_delta(delta, index);
      }
      return key;
    }
  }

  PFS_memory_shared_stat *event_name_array;
  PFS_memory_shared_stat *stat;

  /* Aggregate to MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME */
  event_name_array = global_instr_class_memory_array;
  stat = &event_name_array[index];

  if (flag_global_instrumentation && klass->m_enabled) {
    stat->count_global_realloc(old_size, new_size);
  } else {
    stat->count_global_free(old_size);
    key = PSI_NOT_INSTRUMENTED;
  }

  *owner_thread_hdl = NULL;
  return key;
}

static PSI_memory_key pfs_memory_claim_v1(PSI_memory_key key, size_t size,
                                          PSI_thread **owner) {
  PFS_thread **owner_thread = reinterpret_cast<PFS_thread **>(owner);
  DBUG_ASSERT(owner_thread != NULL);

  PFS_memory_class *klass = find_memory_class(key);
  if (klass == NULL) {
    *owner_thread = NULL;
    return PSI_NOT_INSTRUMENTED;
  }

  /*
    Do not check klass->m_enabled.
    Do not check flag_global_instrumentation.
    If a memory alloc was instrumented,
    the corresponding free must be instrumented.
  */

  uint index = klass->m_event_name_index;
  PFS_memory_stat_delta delta_buffer;
  PFS_memory_stat_delta *delta;

  if (flag_thread_instrumentation && !klass->is_global()) {
    PFS_thread *old_thread = sanitize_thread(*owner_thread);
    PFS_thread *new_thread = my_thread_get_THR_PFS();
    PFS_memory_safe_stat *event_name_array;
    PFS_memory_safe_stat *stat;

    if (old_thread != new_thread) {
      if (old_thread != NULL) {
        event_name_array = old_thread->write_instr_class_memory_stats();
        stat = &event_name_array[index];
        delta = stat->count_free(size, &delta_buffer);

        if (delta != NULL) {
          old_thread->carry_memory_stat_delta(delta, index);
        }
      }

      if (new_thread != NULL) {
        event_name_array = new_thread->write_instr_class_memory_stats();
        stat = &event_name_array[index];
        delta = stat->count_alloc(size, &delta_buffer);

        if (delta != NULL) {
          new_thread->carry_memory_stat_delta(delta, index);
        }
      }

      *owner_thread = new_thread;
    }

    return key;
  }

  *owner_thread = NULL;
  return key;
}

void pfs_memory_free_v1(PSI_memory_key key, size_t size,
                        PSI_thread *owner MY_ATTRIBUTE((unused))) {
  PFS_memory_class *klass = find_memory_class(key);
  if (klass == NULL) {
    return;
  }

  /*
    Do not check klass->m_enabled.
    Do not check flag_global_instrumentation.
    If a memory alloc was instrumented,
    the corresponding free must be instrumented.
  */

  uint index = klass->m_event_name_index;
  PFS_memory_stat_delta delta_buffer;
  PFS_memory_stat_delta *delta;

  if (flag_thread_instrumentation && !klass->is_global()) {
    PFS_thread *pfs_thread = my_thread_get_THR_PFS();
    if (likely(pfs_thread != NULL)) {
#ifdef PFS_PARANOID
      PFS_thread *owner_thread = reinterpret_cast<PFS_thread *>(owner);

      if (owner_thread != pfs_thread) {
        owner_thread = sanitize_thread(owner_thread);
        if (owner_thread != NULL) {
          report_memory_accounting_error("pfs_memory_free_v1", pfs_thread, size,
                                         klass, owner_thread);
        }
      }
#endif /* PFS_PARANOID */

      /*
        Do not check pfs_thread->m_enabled.
        If a memory alloc was instrumented,
        the corresponding free must be instrumented.
      */
      /* Aggregate to MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME */
      PFS_memory_safe_stat *event_name_array;
      PFS_memory_safe_stat *stat;
      event_name_array = pfs_thread->write_instr_class_memory_stats();
      stat = &event_name_array[index];
      delta = stat->count_free(size, &delta_buffer);

      if (delta != NULL) {
        pfs_thread->carry_memory_stat_delta(delta, index);
      }
      return;
    }
  }

  PFS_memory_shared_stat *event_name_array;
  PFS_memory_shared_stat *stat;
  /* Aggregate to MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME */
  event_name_array = global_instr_class_memory_array;
  if (event_name_array) {
    stat = &event_name_array[index];
    (void)stat->count_free(size, &delta_buffer);
  }
  return;
}

void pfs_unlock_table_v1(PSI_table *table) {
  PFS_table *pfs_table = reinterpret_cast<PFS_table *>(table);

  DBUG_ASSERT(pfs_table != NULL);

  pfs_table->m_internal_lock = PFS_TL_NONE;
  return;
}

PSI_metadata_lock *pfs_create_metadata_lock_v1(
    void *identity, const MDL_key *mdl_key, opaque_mdl_type mdl_type,
    opaque_mdl_duration mdl_duration, opaque_mdl_status mdl_status,
    const char *src_file, uint src_line) {
  if (!flag_global_instrumentation) {
    return NULL;
  }

  if (!global_metadata_class.m_enabled) {
    return NULL;
  }

  PFS_thread *pfs_thread = my_thread_get_THR_PFS();
  if (pfs_thread == NULL) {
    return NULL;
  }

  PFS_metadata_lock *pfs;
  pfs = create_metadata_lock(identity, mdl_key, mdl_type, mdl_duration,
                             mdl_status, src_file, src_line);

  if (pfs != NULL) {
    pfs->m_owner_thread_id = pfs_thread->m_thread_internal_id;
    pfs->m_owner_event_id = pfs_thread->m_event_id;
  }

  return reinterpret_cast<PSI_metadata_lock *>(pfs);
}

void pfs_set_metadata_lock_status_v1(PSI_metadata_lock *lock,
                                     opaque_mdl_status mdl_status) {
  PFS_metadata_lock *pfs = reinterpret_cast<PFS_metadata_lock *>(lock);
  DBUG_ASSERT(pfs != NULL);
  pfs->m_mdl_status = mdl_status;
}

void pfs_destroy_metadata_lock_v1(PSI_metadata_lock *lock) {
  PFS_metadata_lock *pfs = reinterpret_cast<PFS_metadata_lock *>(lock);
  DBUG_ASSERT(pfs != NULL);
  destroy_metadata_lock(pfs);
}

PSI_metadata_locker *pfs_start_metadata_wait_v1(
    PSI_metadata_locker_state *state, PSI_metadata_lock *lock,
    const char *src_file, uint src_line) {
  PFS_metadata_lock *pfs_lock = reinterpret_cast<PFS_metadata_lock *>(lock);
  DBUG_ASSERT(state != NULL);
  DBUG_ASSERT(pfs_lock != NULL);

  if (!pfs_lock->m_enabled) {
    return NULL;
  }

  uint flags;
  ulonglong timer_start = 0;

  if (flag_thread_instrumentation) {
    PFS_thread *pfs_thread = my_thread_get_THR_PFS();
    if (unlikely(pfs_thread == NULL)) {
      return NULL;
    }
    if (!pfs_thread->m_enabled) {
      return NULL;
    }
    state->m_thread = reinterpret_cast<PSI_thread *>(pfs_thread);
    flags = STATE_FLAG_THREAD;

    if (pfs_lock->m_timed) {
      timer_start = get_wait_timer();
      state->m_timer_start = timer_start;
      flags |= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current) {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   &pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE])) {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait = pfs_thread->m_events_waits_current;
      state->m_wait = wait;
      flags |= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event = wait - 1;
      wait->m_event_type = EVENT_TYPE_WAIT;
      wait->m_nesting_event_id = parent_event->m_event_id;
      wait->m_nesting_event_type = parent_event->m_event_type;

      wait->m_thread_internal_id = pfs_thread->m_thread_internal_id;
      wait->m_class = &global_metadata_class;
      wait->m_timer_start = timer_start;
      wait->m_timer_end = 0;
      wait->m_object_instance_addr = pfs_lock->m_identity;
      wait->m_event_id = pfs_thread->m_event_id++;
      wait->m_end_event_id = 0;
      wait->m_weak_metadata_lock = pfs_lock;
      wait->m_weak_version = pfs_lock->get_version();
      wait->m_operation = OPERATION_TYPE_METADATA;
      wait->m_source_file = src_file;
      wait->m_source_line = src_line;
      wait->m_wait_class = WAIT_CLASS_METADATA;

      pfs_thread->m_events_waits_current++;
    }
  } else {
    if (pfs_lock->m_timed) {
      timer_start = get_wait_timer();
      state->m_timer_start = timer_start;
      flags = STATE_FLAG_TIMED;
      state->m_thread = NULL;
    } else {
      /*
        Complete shortcut.
      */
      /* Aggregate to EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME (counted) */
      global_metadata_stat.aggregate_counted();
      return NULL;
    }
  }

  state->m_flags = flags;
  state->m_metadata_lock = lock;
  return reinterpret_cast<PSI_metadata_locker *>(state);
}

void pfs_end_metadata_wait_v1(PSI_metadata_locker *locker, int) {
  PSI_metadata_locker_state *state =
      reinterpret_cast<PSI_metadata_locker_state *>(locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end = 0;
  ulonglong wait_time = 0;

  PFS_thread *thread = reinterpret_cast<PFS_thread *>(state->m_thread);

  uint flags = state->m_flags;

  if (flags & STATE_FLAG_TIMED) {
    timer_end = get_wait_timer();
    wait_time = timer_end - state->m_timer_start;
  }

  if (flags & STATE_FLAG_THREAD) {
    PFS_single_stat *event_name_array;
    event_name_array = thread->write_instr_class_waits_stats();

    if (flags & STATE_FLAG_TIMED) {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      event_name_array[GLOBAL_METADATA_EVENT_INDEX].aggregate_value(wait_time);
    } else {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[GLOBAL_METADATA_EVENT_INDEX].aggregate_counted();
    }

    if (flags & STATE_FLAG_EVENT) {
      PFS_events_waits *wait =
          reinterpret_cast<PFS_events_waits *>(state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end = timer_end;
      wait->m_end_event_id = thread->m_event_id;
      if (thread->m_flag_events_waits_history) {
        insert_events_waits_history(thread, wait);
      }
      if (thread->m_flag_events_waits_history_long) {
        insert_events_waits_history_long(wait);
      }
      thread->m_events_waits_current--;

      DBUG_ASSERT(wait == thread->m_events_waits_current);
    }
  } else {
    if (flags & STATE_FLAG_TIMED) {
      /* Aggregate to EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME (timed) */
      global_metadata_stat.aggregate_value(wait_time);
    } else {
      /* Aggregate to EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME (counted) */
      global_metadata_stat.aggregate_counted();
    }
  }
}

void pfs_log_error_v1(uint error_num, PSI_error_operation error_operation) {
  PFS_error_stat *stat;
  uint error_stat_index;

  DBUG_ASSERT(error_num != 0);

  if (!flag_global_instrumentation) {
    return;
  }

  if (!global_error_class.m_enabled) {
    return;
  }

  if (!max_server_errors) {
    return;
  }

  if (flag_thread_instrumentation) {
    PFS_thread *pfs_thread = my_thread_get_THR_PFS();
    if (unlikely(pfs_thread == NULL)) {
      return;
    }
    if (!pfs_thread->m_enabled) {
      return;
    }

    /* Aggregate to EVENTS_ERRORS_SUMMARY_BY_THREAD_BY_ERROR */
    stat = &pfs_thread->write_instr_class_errors_stats()[GLOBAL_ERROR_INDEX];
  } else {
    /* Aggregate to EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR */
    stat = &global_error_stat;
  }

  /* Find the index of this particular error in array of error stats. */
  error_stat_index = lookup_error_stat_index(error_num);

  /*
     If this error goes beyond max_server_errors, OR
     If it's (RE)SIGNALED error with custom error number
     collect its stats at NULL row.
   */
  if (error_stat_index >= max_server_errors) {
    error_stat_index = 0;
  }

  /* Aggregate to EVENTS_ERRORS_SUMMARY_..._BY_ERROR (counted) */
  stat->aggregate_count(error_stat_index, error_operation);
}

void pfs_register_data_lock_v1(PSI_engine_data_lock_inspector *inspector) {
  DBUG_ASSERT(g_data_lock_inspector_count < COUNT_DATA_LOCK_ENGINES);

  g_data_lock_inspector[g_data_lock_inspector_count] = inspector;
  g_data_lock_inspector_count++;
}

void pfs_unregister_data_lock_v1(
    PSI_engine_data_lock_inspector * /* inspector */) {
  /*
    This code is not used yet, because:
    - there is only one engine exposing data locks (innodb)
    - the innodb engine is never unloaded.
  */
  DBUG_ASSERT(false);

#ifdef LATER
  for (unsigned int i = 0; i < COUNT_DATA_LOCK_ENGINES; i++) {
    if (g_data_lock_inspector[i] == inspector) {
      g_data_lock_inspector[i] = NULL;
    }
  }
#endif
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::unload_plugin.
*/
void pfs_unload_plugin_v1(const char *plugin_name MY_ATTRIBUTE((unused))) {
  /*
    A plugin or component is being unloaded. Events that originated from the
    plugin contain string pointers set by the __FILE__ macro. These source file
    strings are used in the SOURCE column in some tables, and will become
    invalid when the plugin memory is freed.
    For safety, all file string pointers in all events will be set to NULL.
  */

  /* Temporarily disable the SOURCE column in all tables. */
  pfs_unload_plugin_ref_count++;

  /* Find the relevant events, set source file strings to NULL. */
  reset_source_file_pointers();

  /* Re-enable the SOURCE column in all tables. */
  pfs_unload_plugin_ref_count--;
}

/**
  Implementation of the instrumentation interface.
  @sa PSI_system_service_v1
*/
PSI_system_service_v1 pfs_system_service_v1 = {
    /* Old interface, for plugins. */
    pfs_unload_plugin_v1};

SERVICE_TYPE(psi_system_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_system_v1) = {
    /* New interface, for components. */
    pfs_unload_plugin_v1};

/**
  Implementation of the instrumentation interface.
  @sa PSI_thread_service_v1
*/
PSI_thread_service_v1 pfs_thread_service_v1 = {
    /* Old interface, for plugins. */
    pfs_register_thread_v1,
    pfs_spawn_thread_v1,
    pfs_new_thread_v1,
    pfs_set_thread_id_v1,
    pfs_set_thread_THD_v1,
    pfs_set_thread_os_id_v1,
    pfs_get_thread_v1,
    pfs_set_thread_user_v1,
    pfs_set_thread_account_v1,
    pfs_set_thread_db_v1,
    pfs_set_thread_command_v1,
    pfs_set_connection_type_v1,
    pfs_set_thread_start_time_v1,
    pfs_set_thread_state_v1,
    pfs_set_thread_info_v1,
    pfs_set_thread_resource_group_v1,
    pfs_set_thread_resource_group_by_id_v1,
    pfs_set_thread_v1,
    pfs_delete_current_thread_v1,
    pfs_delete_thread_v1,
    pfs_set_thread_connect_attrs_v1,
    pfs_get_thread_event_id_v1,
    pfs_get_thread_system_attrs_v1,
    pfs_get_thread_system_attrs_by_id_v1,
    pfs_register_notification_v1,
    pfs_unregister_notification_v1,
    pfs_notify_session_connect_v1,
    pfs_notify_session_disconnect_v1,
    pfs_notify_session_change_user_v1};

SERVICE_TYPE(psi_thread_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_thread_v1) = {
    /* New interface, for components. */
    pfs_register_thread_v1,
    pfs_spawn_thread_v1,
    pfs_new_thread_v1,
    pfs_set_thread_id_v1,
    pfs_set_thread_THD_v1,
    pfs_set_thread_os_id_v1,
    pfs_get_thread_v1,
    pfs_set_thread_user_v1,
    pfs_set_thread_account_v1,
    pfs_set_thread_db_v1,
    pfs_set_thread_command_v1,
    pfs_set_connection_type_v1,
    pfs_set_thread_start_time_v1,
    pfs_set_thread_state_v1,
    pfs_set_thread_info_v1,
    pfs_set_thread_v1,
    pfs_delete_current_thread_v1,
    pfs_delete_thread_v1,
    pfs_set_thread_connect_attrs_v1,
    pfs_get_thread_event_id_v1,
    pfs_get_thread_system_attrs_v1,
    pfs_get_thread_system_attrs_by_id_v1,
    pfs_register_notification_v1,
    pfs_unregister_notification_v1,
    pfs_notify_session_connect_v1,
    pfs_notify_session_disconnect_v1,
    pfs_notify_session_change_user_v1};

PSI_mutex_service_v1 pfs_mutex_service_v1 = {
    /* Old interface, for plugins. */
    pfs_register_mutex_v1,   pfs_init_mutex_v1,     pfs_destroy_mutex_v1,
    pfs_start_mutex_wait_v1, pfs_end_mutex_wait_v1, pfs_unlock_mutex_v1};

SERVICE_TYPE(psi_mutex_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_mutex_v1) = {
    /* New interface, for components. */
    pfs_register_mutex_v1,   pfs_init_mutex_v1,     pfs_destroy_mutex_v1,
    pfs_start_mutex_wait_v1, pfs_end_mutex_wait_v1, pfs_unlock_mutex_v1};

PSI_rwlock_service_v1 pfs_rwlock_service_v1 = {
    /* Old interface, for plugins. */
    pfs_register_rwlock_v1,   pfs_init_rwlock_v1,
    pfs_destroy_rwlock_v1,    pfs_start_rwlock_rdwait_v1,
    pfs_end_rwlock_rdwait_v1, pfs_start_rwlock_wrwait_v1,
    pfs_end_rwlock_wrwait_v1, pfs_unlock_rwlock_v1};

SERVICE_TYPE(psi_rwlock_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_rwlock_v1) = {
    /* New interface, for components. */
    pfs_register_rwlock_v1,   pfs_init_rwlock_v1,
    pfs_destroy_rwlock_v1,    pfs_start_rwlock_rdwait_v1,
    pfs_end_rwlock_rdwait_v1, pfs_start_rwlock_wrwait_v1,
    pfs_end_rwlock_wrwait_v1, pfs_unlock_rwlock_v1};

PSI_cond_service_v1 pfs_cond_service_v1 = {
    /* Old interface, for plugins. */
    pfs_register_cond_v1, pfs_init_cond_v1,      pfs_destroy_cond_v1,
    pfs_signal_cond_v1,   pfs_broadcast_cond_v1, pfs_start_cond_wait_v1,
    pfs_end_cond_wait_v1};

SERVICE_TYPE(psi_cond_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_cond_v1) = {
    /* New interface, for components. */
    pfs_register_cond_v1, pfs_init_cond_v1,      pfs_destroy_cond_v1,
    pfs_signal_cond_v1,   pfs_broadcast_cond_v1, pfs_start_cond_wait_v1,
    pfs_end_cond_wait_v1};

PSI_file_service_v1 pfs_file_service_v1 = {
    /* Old interface, for plugins. */
    pfs_register_file_v1,
    pfs_create_file_v1,
    pfs_get_thread_file_name_locker_v1,
    pfs_get_thread_file_stream_locker_v1,
    pfs_get_thread_file_descriptor_locker_v1,
    pfs_start_file_open_wait_v1,
    pfs_end_file_open_wait_v1,
    pfs_end_file_open_wait_and_bind_to_descriptor_v1,
    pfs_end_temp_file_open_wait_and_bind_to_descriptor_v1,
    pfs_start_file_wait_v1,
    pfs_end_file_wait_v1,
    pfs_start_file_close_wait_v1,
    pfs_end_file_close_wait_v1,
    pfs_end_file_rename_wait_v1};

SERVICE_TYPE(psi_file_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_file_v1) = {
    /* New interface, for components. */
    pfs_register_file_v1,
    pfs_create_file_v1,
    pfs_get_thread_file_name_locker_v1,
    pfs_get_thread_file_stream_locker_v1,
    pfs_get_thread_file_descriptor_locker_v1,
    pfs_start_file_open_wait_v1,
    pfs_end_file_open_wait_v1,
    pfs_end_file_open_wait_and_bind_to_descriptor_v1,
    pfs_end_temp_file_open_wait_and_bind_to_descriptor_v1,
    pfs_start_file_wait_v1,
    pfs_end_file_wait_v1,
    pfs_start_file_close_wait_v1,
    pfs_end_file_close_wait_v1,
    pfs_end_file_rename_wait_v1};

PSI_socket_service_v1 pfs_socket_service_v1 = {
    /* Old interface, for plugins. */
    pfs_register_socket_v1, pfs_init_socket_v1,
    pfs_destroy_socket_v1,  pfs_start_socket_wait_v1,
    pfs_end_socket_wait_v1, pfs_set_socket_state_v1,
    pfs_set_socket_info_v1, pfs_set_socket_thread_owner_v1};

SERVICE_TYPE(psi_socket_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_socket_v1) = {
    /* New interface, for components. */
    pfs_register_socket_v1, pfs_init_socket_v1,
    pfs_destroy_socket_v1,  pfs_start_socket_wait_v1,
    pfs_end_socket_wait_v1, pfs_set_socket_state_v1,
    pfs_set_socket_info_v1, pfs_set_socket_thread_owner_v1};

PSI_table_service_v1 pfs_table_service_v1 = {
    /* Old interface, for plugins. */
    pfs_get_table_share_v1,     pfs_release_table_share_v1,
    pfs_drop_table_share_v1,    pfs_open_table_v1,
    pfs_unbind_table_v1,        pfs_rebind_table_v1,
    pfs_close_table_v1,         pfs_start_table_io_wait_v1,
    pfs_end_table_io_wait_v1,   pfs_start_table_lock_wait_v1,
    pfs_end_table_lock_wait_v1, pfs_unlock_table_v1};

SERVICE_TYPE(psi_table_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_table_v1) = {
    /* New interface, for components. */
    pfs_get_table_share_v1,     pfs_release_table_share_v1,
    pfs_drop_table_share_v1,    pfs_open_table_v1,
    pfs_unbind_table_v1,        pfs_rebind_table_v1,
    pfs_close_table_v1,         pfs_start_table_io_wait_v1,
    pfs_end_table_io_wait_v1,   pfs_start_table_lock_wait_v1,
    pfs_end_table_lock_wait_v1, pfs_unlock_table_v1};

PSI_mdl_service_v1 pfs_mdl_service_v1 = {
    /* Old interface, for plugins. */
    pfs_create_metadata_lock_v1, pfs_set_metadata_lock_status_v1,
    pfs_destroy_metadata_lock_v1, pfs_start_metadata_wait_v1,
    pfs_end_metadata_wait_v1};

SERVICE_TYPE(psi_mdl_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_mdl_v1) = {
    /* New interface, for components. */
    pfs_create_metadata_lock_v1, pfs_set_metadata_lock_status_v1,
    pfs_destroy_metadata_lock_v1, pfs_start_metadata_wait_v1,
    pfs_end_metadata_wait_v1};

PSI_idle_service_v1 pfs_idle_service_v1 = {
    /* Old interface, for plugins. */
    pfs_start_idle_wait_v1, pfs_end_idle_wait_v1};

SERVICE_TYPE(psi_idle_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_idle_v1) = {
    /* New interface, for components. */
    pfs_start_idle_wait_v1, pfs_end_idle_wait_v1};

PSI_stage_service_v1 pfs_stage_service_v1 = {
    /* Old interface, for plugins. */
    pfs_register_stage_v1, pfs_start_stage_v1,
    pfs_get_current_stage_progress_v1, pfs_end_stage_v1};

SERVICE_TYPE(psi_stage_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_stage_v1) = {
    /* New interface, for components. */
    pfs_register_stage_v1, pfs_start_stage_v1,
    pfs_get_current_stage_progress_v1, pfs_end_stage_v1};

PSI_statement_service_v1 pfs_statement_service_v1 = {
    /* Old interface, for plugins. */
    pfs_register_statement_v1,
    pfs_get_thread_statement_locker_v1,
    pfs_refine_statement_v1,
    pfs_start_statement_v1,
    pfs_set_statement_text_v1,
    pfs_set_statement_lock_time_v1,
    pfs_set_statement_rows_sent_v1,
    pfs_set_statement_rows_examined_v1,
    pfs_inc_statement_created_tmp_disk_tables_v1,
    pfs_inc_statement_created_tmp_tables_v1,
    pfs_inc_statement_select_full_join_v1,
    pfs_inc_statement_select_full_range_join_v1,
    pfs_inc_statement_select_range_v1,
    pfs_inc_statement_select_range_check_v1,
    pfs_inc_statement_select_scan_v1,
    pfs_inc_statement_sort_merge_passes_v1,
    pfs_inc_statement_sort_range_v1,
    pfs_inc_statement_sort_rows_v1,
    pfs_inc_statement_sort_scan_v1,
    pfs_set_statement_no_index_used_v1,
    pfs_set_statement_no_good_index_used_v1,
    pfs_end_statement_v1,
    pfs_create_prepared_stmt_v1,
    pfs_destroy_prepared_stmt_v1,
    pfs_reprepare_prepared_stmt_v1,
    pfs_execute_prepared_stmt_v1,
    pfs_set_prepared_stmt_text_v1,
    pfs_digest_start_v1,
    pfs_digest_end_v1,
    pfs_get_sp_share_v1,
    pfs_release_sp_share_v1,
    pfs_start_sp_v1,
    pfs_end_sp_v1,
    pfs_drop_sp_v1};

SERVICE_TYPE(psi_statement_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_statement_v1) = {
    /* New interface, for components. */
    pfs_register_statement_v1,
    pfs_get_thread_statement_locker_v1,
    pfs_refine_statement_v1,
    pfs_start_statement_v1,
    pfs_set_statement_text_v1,
    pfs_set_statement_lock_time_v1,
    pfs_set_statement_rows_sent_v1,
    pfs_set_statement_rows_examined_v1,
    pfs_inc_statement_created_tmp_disk_tables_v1,
    pfs_inc_statement_created_tmp_tables_v1,
    pfs_inc_statement_select_full_join_v1,
    pfs_inc_statement_select_full_range_join_v1,
    pfs_inc_statement_select_range_v1,
    pfs_inc_statement_select_range_check_v1,
    pfs_inc_statement_select_scan_v1,
    pfs_inc_statement_sort_merge_passes_v1,
    pfs_inc_statement_sort_range_v1,
    pfs_inc_statement_sort_rows_v1,
    pfs_inc_statement_sort_scan_v1,
    pfs_set_statement_no_index_used_v1,
    pfs_set_statement_no_good_index_used_v1,
    pfs_end_statement_v1,
    pfs_create_prepared_stmt_v1,
    pfs_destroy_prepared_stmt_v1,
    pfs_reprepare_prepared_stmt_v1,
    pfs_execute_prepared_stmt_v1,
    pfs_set_prepared_stmt_text_v1,
    pfs_digest_start_v1,
    pfs_digest_end_v1,
    pfs_get_sp_share_v1,
    pfs_release_sp_share_v1,
    pfs_start_sp_v1,
    pfs_end_sp_v1,
    pfs_drop_sp_v1};

PSI_transaction_service_v1 pfs_transaction_service_v1 = {
    /* Old interface, for plugins. */
    pfs_get_thread_transaction_locker_v1,
    pfs_start_transaction_v1,
    pfs_set_transaction_xid_v1,
    pfs_set_transaction_xa_state_v1,
    pfs_set_transaction_gtid_v1,
    pfs_set_transaction_trxid_v1,
    pfs_inc_transaction_savepoints_v1,
    pfs_inc_transaction_rollback_to_savepoint_v1,
    pfs_inc_transaction_release_savepoint_v1,
    pfs_end_transaction_v1};

SERVICE_TYPE(psi_transaction_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_transaction_v1) = {
    /* New interface, for components. */
    pfs_get_thread_transaction_locker_v1,
    pfs_start_transaction_v1,
    pfs_set_transaction_xid_v1,
    pfs_set_transaction_xa_state_v1,
    pfs_set_transaction_gtid_v1,
    pfs_set_transaction_trxid_v1,
    pfs_inc_transaction_savepoints_v1,
    pfs_inc_transaction_rollback_to_savepoint_v1,
    pfs_inc_transaction_release_savepoint_v1,
    pfs_end_transaction_v1};

PSI_memory_service_v1 pfs_memory_service_v1 = {
    /* Old interface, for plugins. */
    pfs_register_memory_v1, pfs_memory_alloc_v1, pfs_memory_realloc_v1,
    pfs_memory_claim_v1, pfs_memory_free_v1};

SERVICE_TYPE(psi_memory_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_memory_v1) = {
    /* New interface, for components. */
    pfs_register_memory_v1, pfs_memory_alloc_v1, pfs_memory_realloc_v1,
    pfs_memory_claim_v1, pfs_memory_free_v1};

PSI_error_service_v1 pfs_error_service_v1 = {
    /* Old interface, for plugins. */
    pfs_log_error_v1};

SERVICE_TYPE(psi_error_v1)
SERVICE_IMPLEMENTATION(performance_schema, psi_error_v1) = {
    /* New interface, for components. */
    pfs_log_error_v1};

PSI_data_lock_service_v1 pfs_data_lock_service_v1 = {
    /* Old interface, for plugins. */
    pfs_register_data_lock_v1, pfs_unregister_data_lock_v1};

static void *get_system_interface(int version) {
  switch (version) {
    case PSI_SYSTEM_VERSION_1:
      return &pfs_system_service_v1;
    default:
      return NULL;
  }
}

static void *get_thread_interface(int version) {
  switch (version) {
    case PSI_THREAD_VERSION_1:
      return &pfs_thread_service_v1;
    default:
      return NULL;
  }
}

static void *get_mutex_interface(int version) {
  switch (version) {
    case PSI_MUTEX_VERSION_1:
      return &pfs_mutex_service_v1;
    default:
      return NULL;
  }
}

static void *get_rwlock_interface(int version) {
  switch (version) {
    case PSI_RWLOCK_VERSION_1:
      return &pfs_rwlock_service_v1;
    default:
      return NULL;
  }
}

static void *get_cond_interface(int version) {
  switch (version) {
    case PSI_COND_VERSION_1:
      return &pfs_cond_service_v1;
    default:
      return NULL;
  }
}

static void *get_file_interface(int version) {
  switch (version) {
    case PSI_FILE_VERSION_1:
      return &pfs_file_service_v1;
    default:
      return NULL;
  }
}

static void *get_socket_interface(int version) {
  switch (version) {
    case PSI_SOCKET_VERSION_1:
      return &pfs_socket_service_v1;
    default:
      return NULL;
  }
}

static void *get_table_interface(int version) {
  switch (version) {
    case PSI_TABLE_VERSION_1:
      return &pfs_table_service_v1;
    default:
      return NULL;
  }
}

static void *get_mdl_interface(int version) {
  switch (version) {
    case PSI_MDL_VERSION_1:
      return &pfs_mdl_service_v1;
    default:
      return NULL;
  }
}

static void *get_idle_interface(int version) {
  switch (version) {
    case PSI_IDLE_VERSION_1:
      return &pfs_idle_service_v1;
    default:
      return NULL;
  }
}

static void *get_stage_interface(int version) {
  switch (version) {
    case PSI_STAGE_VERSION_1:
      return &pfs_stage_service_v1;
    default:
      return NULL;
  }
}

static void *get_statement_interface(int version) {
  switch (version) {
    case PSI_STATEMENT_VERSION_1:
      return &pfs_statement_service_v1;
    default:
      return NULL;
  }
}

static void *get_transaction_interface(int version) {
  switch (version) {
    case PSI_TRANSACTION_VERSION_1:
      return &pfs_transaction_service_v1;
    default:
      return NULL;
  }
}

static void *get_memory_interface(int version) {
  switch (version) {
    case PSI_MEMORY_VERSION_1:
      return &pfs_memory_service_v1;
    default:
      return NULL;
  }
}

static void *get_error_interface(int version) {
  switch (version) {
    case PSI_ERROR_VERSION_1:
      return &pfs_error_service_v1;
    default:
      return NULL;
  }
}

static void *get_data_lock_interface(int version) {
  switch (version) {
    case PSI_DATA_LOCK_VERSION_1:
      return &pfs_data_lock_service_v1;
    default:
      return NULL;
  }
}

struct PSI_cond_bootstrap pfs_cond_bootstrap = {get_cond_interface};

struct PSI_data_lock_bootstrap pfs_data_lock_bootstrap = {
    get_data_lock_interface};

PSI_engine_data_lock_inspector *g_data_lock_inspector[COUNT_DATA_LOCK_ENGINES] =
    {NULL};

unsigned int g_data_lock_inspector_count = 0;

struct PSI_error_bootstrap pfs_error_bootstrap = {get_error_interface};

struct PSI_file_bootstrap pfs_file_bootstrap = {get_file_interface};

struct PSI_idle_bootstrap pfs_idle_bootstrap = {get_idle_interface};

struct PSI_mdl_bootstrap pfs_mdl_bootstrap = {get_mdl_interface};

struct PSI_memory_bootstrap pfs_memory_bootstrap = {get_memory_interface};

struct PSI_mutex_bootstrap pfs_mutex_bootstrap = {get_mutex_interface};

struct PSI_rwlock_bootstrap pfs_rwlock_bootstrap = {get_rwlock_interface};

struct PSI_socket_bootstrap pfs_socket_bootstrap = {get_socket_interface};

struct PSI_stage_bootstrap pfs_stage_bootstrap = {get_stage_interface};

struct PSI_statement_bootstrap pfs_statement_bootstrap = {
    get_statement_interface};

struct PSI_system_bootstrap pfs_system_bootstrap = {get_system_interface};

struct PSI_table_bootstrap pfs_table_bootstrap = {get_table_interface};

struct PSI_thread_bootstrap pfs_thread_bootstrap = {get_thread_interface};

struct PSI_transaction_bootstrap pfs_transaction_bootstrap = {
    get_transaction_interface};

BEGIN_COMPONENT_PROVIDES(performance_schema)
PROVIDES_SERVICE(performance_schema, psi_cond_v1),
    PROVIDES_SERVICE(performance_schema, psi_error_v1),
    PROVIDES_SERVICE(performance_schema, psi_file_v1),
    PROVIDES_SERVICE(performance_schema, psi_idle_v1),
    PROVIDES_SERVICE(performance_schema, psi_mdl_v1),
    PROVIDES_SERVICE(performance_schema, psi_memory_v1),
    PROVIDES_SERVICE(performance_schema, psi_mutex_v1),
    PROVIDES_SERVICE(performance_schema, psi_rwlock_v1),
    PROVIDES_SERVICE(performance_schema, psi_socket_v1),
    PROVIDES_SERVICE(performance_schema, psi_stage_v1),
    PROVIDES_SERVICE(performance_schema, psi_statement_v1),
    PROVIDES_SERVICE(performance_schema, psi_system_v1),
    PROVIDES_SERVICE(performance_schema, psi_table_v1),
    PROVIDES_SERVICE(performance_schema, psi_thread_v1),
    PROVIDES_SERVICE(performance_schema, psi_transaction_v1),
    PROVIDES_SERVICE(performance_schema, pfs_plugin_table),
    END_COMPONENT_PROVIDES();

static BEGIN_COMPONENT_REQUIRES(performance_schema) END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(performance_schema)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), END_COMPONENT_METADATA();

DECLARE_COMPONENT(performance_schema, "mysql:pfs")
/* There are no initialization/deinitialization functions, they will not be
   called as this component is not a regular one. */
NULL, NULL END_DECLARE_COMPONENT();

bool pfs_init_services(SERVICE_TYPE(registry_registration) * reg) {
  int inx = 0;

  for (;;) {
    my_h_service pfs_service;
    pfs_service = reinterpret_cast<my_h_service>(
        mysql_component_performance_schema.provides[inx].implementation);

    if (pfs_service == NULL) {
      break;
    }

    if (reg->register_service(
            mysql_component_performance_schema.provides[inx].name,
            pfs_service)) {
      return true;
    }

    inx++;
  }

  return false;
}

bool pfs_deinit_services(SERVICE_TYPE(registry_registration) * reg) {
  for (int inx = 0;
       mysql_component_performance_schema.provides[inx].name != NULL; inx++) {
    if (reg->unregister(
            mysql_component_performance_schema.provides[inx].name)) {
      return true;
    }
  }

  return false;
}
