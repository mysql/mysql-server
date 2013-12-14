/* Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/pfs.cc
  The performance schema implementation of all instruments.
*/
#include "my_global.h"
#include "thr_lock.h"
#include "mysql/psi/psi.h"
#include "mysql/psi/mysql_thread.h"
#include "my_pthread.h"
#include "sql_const.h"
#include "pfs.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_host.h"
#include "pfs_user.h"
#include "pfs_account.h"
#include "pfs_global.h"
#include "pfs_column_values.h"
#include "pfs_timer.h"
#include "pfs_events_waits.h"
#include "pfs_events_stages.h"
#include "pfs_events_statements.h"
#include "pfs_setup_actor.h"
#include "pfs_setup_object.h"
#include "sql_error.h"
#include "sp_head.h"
#include "pfs_digest.h"

/**
  @page PAGE_PERFORMANCE_SCHEMA The Performance Schema main page
  MySQL PERFORMANCE_SCHEMA implementation.

  @section INTRO Introduction
  The PERFORMANCE_SCHEMA is a way to introspect the internal execution of
  the server at runtime.
  The performance schema focuses primarily on performance data,
  as opposed to the INFORMATION_SCHEMA whose purpose is to inspect metadata.

  From a user point of view, the performance schema consists of:
  - a dedicated database schema, named PERFORMANCE_SCHEMA,
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

  For details, see the @ref PAGE_INSTRUMENTATION_INTERFACE
  "instrumentation interface page".

  @subsection INT_COMPILING Compiling interface

  The implementation of the performance schema can be enabled or disabled at
  build time, when building MySQL from the source code.

  When building with the performance schema code, some compilation flags
  are available to change the default values used in the code, if required.

  For more details, see:
  @verbatim ./configure --help @endverbatim

  To compile with the performance schema:
  @verbatim ./configure --with-perfschema @endverbatim

  The implementation of all the compiling options is located in
  @verbatim ./storage/perfschema/plug.in @endverbatim

  @subsection INT_STARTUP Server startup interface

  The server startup interface consists of the "./mysqld ..."
  command line used to start the server.
  When the performance schema is compiled in the server binary,
  extra command line options are available.

  These extra start options allow the DBA to:
  - enable or disable the performance schema
  - specify some sizing parameters.

  To see help for the performance schema startup options, see:
  @verbatim ./sql/mysqld --verbose --help  @endverbatim

  The implementation of all the startup options is located in
  @verbatim ./sql/mysqld.cc, my_long_options[] @endverbatim

  @subsection INT_BOOTSTRAP Server bootstrap interface

  The bootstrap interface is a private interface exposed by
  the performance schema, and used by the SQL layer.
  Its role is to advertise all the SQL tables natively
  supported by the performance schema to the SQL server.
  The code consists of creating MySQL tables for the
  performance schema itself, and is used in './mysql --bootstrap'
  mode when a server is installed.

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

  The SHOW STATUS command is implemented in
  @verbatim ./storage/perfschema/pfs_engine_table.cc @endverbatim

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
  with the following very severe design constraints:

  The parser is unchanged. There are no new keywords, no new statements.
  This guarantees that existing applications will run the same way with or
  without the performance schema.

  All the instrumentation points return "void", there are no error codes.
  Even if the performance schema internally fails, execution of the server
  code will proceed.

  None of the instrumentation points allocate memory.
  All the memory used by the performance schema is pre-allocated at startup,
  and is considered "static" during the server life time.

  None of the instrumentation points use any pthread_mutex, pthread_rwlock,
  or pthread_cond (or platform equivalents).
  Executing the instrumentation point should not cause thread scheduling to
  change in the server.

  In other words, the implementation of the instrumentation points,
  including all the code called by the instrumentation points, is:
  - malloc free
  - mutex free
  - rwlock free

  TODO: All the code located in storage/perfschema is malloc free,
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
  - a server supporting the performance schema + a storage engine
  that is not instrumented
  - a server not supporting the performance schema + a storage engine
  that is instrumented
*/

/**
  @page PAGE_INSTRUMENTATION_INTERFACE Performance schema: instrumentation interface page.
  MySQL performance schema instrumentation interface.

  @section INTRO Introduction

  The instrumentation interface consist of two layers:
  - a raw ABI (Application Binary Interface) layer, that exposes the primitive
  instrumentation functions exported by the performance schema instrumentation
  - an API (Application Programing Interface) layer,
  that provides many helpers for a developer instrumenting some code,
  to make the instrumentation as easy as possible.

  The ABI layer consists of:
@code
#include "mysql/psi/psi.h"
@endcode

  The API layer consists of:
@code
#include "mysql/psi/mutex_mutex.h"
#include "mysql/psi/mutex_file.h"
@endcode

  The first helper is for mutexes, rwlocks and conditions,
  the second for file io.

  The API layer exposes C macros and typedefs which will expand:
  - either to non-instrumented code, when compiled without the performance
  schema instrumentation
  - or to instrumented code, that will issue the raw calls to the ABI layer
  so that the implementation can collect data.

  Note that all the names introduced (for example, @c mysql_mutex_lock) do not
  collide with any other namespace.
  In particular, the macro @c mysql_mutex_lock is on purpose not named
  @c pthread_mutex_lock.
  This is to:
  - avoid overloading @c pthread_mutex_lock with yet another macro,
  which is dangerous as it can affect user code and pollute
  the end-user namespace.
  - allow the developer instrumenting code to selectively instrument
  some code but not all.

  @section PRINCIPLES Design principles

  The ABI part is designed as a facade, that exposes basic primitives.
  The expectation is that each primitive will be very stable over time,
  but the list will constantly grow when more instruments are supported.
  To support binary compatibility with plugins compiled with a different
  version of the instrumentation, the ABI itself is versioned
  (see @c PSI_v1, @c PSI_v2).

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
  in implemented, when the instrumentation is compiled in:

@verbatim
static inline int mysql_mutex_lock(
  mysql_mutex_t *that, myf flags, const char *src_file, uint src_line)
{
  int result;
  struct PSI_mutex_locker_state state;
  struct PSI_mutex_locker *locker= NULL;

  ............... (a)
  locker= PSI_server->start_mutex_wait(&state, that->p_psi,
                                       PSI_MUTEX_LOCK, locker, src_file, src_line);

  ............... (b)
  result= pthread_mutex_lock(&that->m_mutex);

  ............... (c)
  PSI_server->end_mutex_wait(locker, result);

  return result;
}
@endverbatim

  When the performance schema instrumentation is not compiled in,
  the code becomes simply a wrapper, expanded in line by the compiler:

@verbatim
static inline int mysql_mutex_lock(...)
{
  int result;

  ............... (b)
  result= pthread_mutex_lock(&that->m_mutex);

  return result;
}
@endverbatim
*/

/**
  @page PAGE_AGGREGATES Performance schema: the aggregates page.
  Performance schema aggregates.

  @section INTRO Introduction

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

@verbatim
  PFS_wait_locker (T-A, M-1) ----------
                                      |
                                      v
                                 PFS_mutex (M-1)
                                 - m_wait_stat    ------------
                                      ^                      |
                                      |                      |
  PFS_wait_locker (T-B, M-1) ----------                      |
                                                             v
                                                        PFS_mutex_class (M)
                                                        - m_wait_stat
  PFS_wait_locker (T-C, M-2) ----------                      ^
                                      |                      |
                                      v                      |
                                 PFS_mutex (M-2)             |
                                 - m_wait_stat    ------------
                                      ^
                                      |
  PFS_wait_locker (T-D, M-2) ----------

            ||                        ||                     ||
            ||                        ||                     ||
            vv                        vv                     vv

  EVENTS_WAITS_CURRENT ..._SUMMARY_BY_INSTANCE ..._SUMMARY_BY_EVENT_NAME
@endverbatim

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
  - file io (MYSQL_FILE)
  - socket io (MYSQL_SOCKET)
  - table io
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
  - events that occur during the instrumented code execution are noted with numbers,
  as in [1]. Code executed by these events has an impact on overhead.
  - events that occur during TRUNCATE TABLE operations are noted with numbers,
  followed by "-RESET", as in [4-RESET].
  Code executed by these events has no impact on overhead,
  since they are executed by independent monitoring sessions.
  - events that occur when a reader extracts data from a performance schema table
  are noted with letters, as in [A]. The name of the table involved,
  and the method that builds a row are documented. Code executed by these events
  has no impact on the instrumentation overhead. Note that the table
  implementation may pull data from different buffers.
  - nominal code paths are in plain lines. A "nominal" code path corresponds to
  cases where the performance schema buffers are sized so that no records are lost.
  - degenerated code paths are in dotted lines. A "degenerated" code path corresponds
  to edge cases where parent buffers are full, which forces the code to aggregate to
  grand parents directly.

  Implemented as:
  - [1] @c start_mutex_wait_v1(), @c end_mutex_wait_v1()
  - [2] @c destroy_mutex_v1()
  - [3] @c aggregate_thread_waits()
  - [4] @c PFS_account::aggregate_waits()
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
  - [1] @c start_rwlock_rdwait_v1(), @c end_rwlock_rdwait_v1(), ...
  - [2] @c destroy_rwlock_v1()
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
  - [1] @c start_cond_wait_v1(), @c end_cond_wait_v1()
  - [2] @c destroy_cond_v1()
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
  - [1] @c get_thread_file_name_locker_v1(), @c start_file_wait_v1(),
        @c end_file_wait_v1(), ...
  - [2] @c close_file_v1()
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
  - [1] @c start_socket_wait_v1(), @c end_socket_wait_v1().
  - [2] @c close_socket_v1()
  - [3] @c aggregate_thread_waits()
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
  table_locker(Thread Th, Table Tb, Event = io or lock)
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
  - [1] @c start_table_io_wait_v1(), @c end_table_io_wait_v1()
  - [2] @c close_table_v1()
  - [3] @c drop_table_share_v1()
  - [4] @c TRUNCATE TABLE EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
  - [5] @c TRUNCATE TABLE EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME
  - [A] EVENTS_WAITS_SUMMARY_BY_INSTANCE,
        @c table_events_waits_summary_by_instance::make_table_row()
  - [B] OBJECTS_SUMMARY_GLOBAL_BY_TYPE,
        @c table_os_global_by_type::make_row()
  - [C] EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME,
        @c table_ews_global_by_event_name::make_table_io_row(),
        @c table_ews_global_by_event_name::make_table_lock_row()
  - [D] EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME,
        @c table_ews_by_thread_by_event_name::make_row()
  - [E] EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME,
        @c table_ews_by_user_by_account_name::make_row()
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
  - [1] @c start_stage_v1()
  - [2] @c delete_thread_v1(), @c aggregate_thread_stages()
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
  - [1] @c start_statement_v1(), end_statement_v1()
       (1a, 1b) is an aggregation by EVENT_NAME,
        (1c, 1d, 1e) is an aggregation by TIME,
        (1f) is an aggregation by DIGEST
        all of these are orthogonal,
        and implemented in end_statement_v1().
  - [2] @c delete_thread_v1(), @c aggregate_thread_statements()
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
        @c table_events_statements_current::rnd_next(),
        @c table_events_statements_common::make_row()
  - [G] EVENTS_STATEMENTS_HISTORY,
        @c table_events_statements_history::rnd_next(),
        @c table_events_statements_common::make_row()
  - [H] EVENTS_STATEMENTS_HISTORY_LONG,
        @c table_events_statements_history_long::rnd_next(),
        @c table_events_statements_common::make_row()
  - [I] EVENTS_STATEMENTS_SUMMARY_BY_DIGEST
        @c table_esms_by_digest::make_row()
*/

/**
  @defgroup Performance_schema Performance Schema
  The performance schema component.
  For details, see the
  @ref PAGE_PERFORMANCE_SCHEMA "performance schema main page".

  @defgroup Performance_schema_implementation Performance Schema Implementation
  @ingroup Performance_schema

  @defgroup Performance_schema_tables Performance Schema Tables
  @ingroup Performance_schema_implementation
*/

pthread_key(PFS_thread*, THR_PFS);
bool THR_PFS_initialized= false;

/**
  Conversion map from PSI_mutex_operation to enum_operation_type.
  Indexed by enum PSI_mutex_operation.
*/
static enum_operation_type mutex_operation_map[]=
{
  OPERATION_TYPE_LOCK,
  OPERATION_TYPE_TRYLOCK
};

/**
  Conversion map from PSI_rwlock_operation to enum_operation_type.
  Indexed by enum PSI_rwlock_operation.
*/
static enum_operation_type rwlock_operation_map[]=
{
  OPERATION_TYPE_READLOCK,
  OPERATION_TYPE_WRITELOCK,
  OPERATION_TYPE_TRYREADLOCK,
  OPERATION_TYPE_TRYWRITELOCK
};

/**
  Conversion map from PSI_cond_operation to enum_operation_type.
  Indexed by enum PSI_cond_operation.
*/
static enum_operation_type cond_operation_map[]=
{
  OPERATION_TYPE_WAIT,
  OPERATION_TYPE_TIMEDWAIT
};

/**
  Conversion map from PSI_file_operation to enum_operation_type.
  Indexed by enum PSI_file_operation.
*/
static enum_operation_type file_operation_map[]=
{
  OPERATION_TYPE_FILECREATE,
  OPERATION_TYPE_FILECREATETMP,
  OPERATION_TYPE_FILEOPEN,
  OPERATION_TYPE_FILESTREAMOPEN,
  OPERATION_TYPE_FILECLOSE,
  OPERATION_TYPE_FILESTREAMCLOSE,
  OPERATION_TYPE_FILEREAD,
  OPERATION_TYPE_FILEWRITE,
  OPERATION_TYPE_FILESEEK,
  OPERATION_TYPE_FILETELL,
  OPERATION_TYPE_FILEFLUSH,
  OPERATION_TYPE_FILESTAT,
  OPERATION_TYPE_FILEFSTAT,
  OPERATION_TYPE_FILECHSIZE,
  OPERATION_TYPE_FILEDELETE,
  OPERATION_TYPE_FILERENAME,
  OPERATION_TYPE_FILESYNC
};

/**
  Conversion map from PSI_table_operation to enum_operation_type.
  Indexed by enum PSI_table_io_operation.
*/
static enum_operation_type table_io_operation_map[]=
{
  OPERATION_TYPE_TABLE_FETCH,
  OPERATION_TYPE_TABLE_WRITE_ROW,
  OPERATION_TYPE_TABLE_UPDATE_ROW,
  OPERATION_TYPE_TABLE_DELETE_ROW
};

/**
  Conversion map from enum PFS_TL_LOCK_TYPE to enum_operation_type.
  Indexed by enum PFS_TL_LOCK_TYPE.
*/
static enum_operation_type table_lock_operation_map[]=
{
  OPERATION_TYPE_TL_READ_NORMAL, /* PFS_TL_READ */
  OPERATION_TYPE_TL_READ_WITH_SHARED_LOCKS, /* PFS_TL_READ_WITH_SHARED_LOCKS */
  OPERATION_TYPE_TL_READ_HIGH_PRIORITY, /* PFS_TL_READ_HIGH_PRIORITY */
  OPERATION_TYPE_TL_READ_NO_INSERTS, /* PFS_TL_READ_NO_INSERT */
  OPERATION_TYPE_TL_WRITE_ALLOW_WRITE, /* PFS_TL_WRITE_ALLOW_WRITE */
  OPERATION_TYPE_TL_WRITE_CONCURRENT_INSERT, /* PFS_TL_WRITE_CONCURRENT_INSERT */
  OPERATION_TYPE_TL_WRITE_DELAYED, /* PFS_TL_WRITE_DELAYED */
  OPERATION_TYPE_TL_WRITE_LOW_PRIORITY, /* PFS_TL_WRITE_LOW_PRIORITY */
  OPERATION_TYPE_TL_WRITE_NORMAL, /* PFS_TL_WRITE */
  OPERATION_TYPE_TL_READ_EXTERNAL, /* PFS_TL_READ_EXTERNAL */
  OPERATION_TYPE_TL_WRITE_EXTERNAL /* PFS_TL_WRITE_EXTERNAL */
};

/**
  Conversion map from PSI_socket_operation to enum_operation_type.
  Indexed by enum PSI_socket_operation.
*/
static enum_operation_type socket_operation_map[]=
{
  OPERATION_TYPE_SOCKETCREATE,
  OPERATION_TYPE_SOCKETCONNECT,
  OPERATION_TYPE_SOCKETBIND,
  OPERATION_TYPE_SOCKETCLOSE,
  OPERATION_TYPE_SOCKETSEND,
  OPERATION_TYPE_SOCKETRECV,
  OPERATION_TYPE_SOCKETSENDTO,
  OPERATION_TYPE_SOCKETRECVFROM,
  OPERATION_TYPE_SOCKETSENDMSG,
  OPERATION_TYPE_SOCKETRECVMSG,
  OPERATION_TYPE_SOCKETSEEK,
  OPERATION_TYPE_SOCKETOPT,
  OPERATION_TYPE_SOCKETSTAT,
  OPERATION_TYPE_SOCKETSHUTDOWN,
  OPERATION_TYPE_SOCKETSELECT
};

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
                        char *output, int *output_length)
{
  int len= strlen(category);
  char *out_ptr= output;
  int prefix_length= prefix->length;

  if (unlikely((prefix_length + len + 1) >=
               PFS_MAX_FULL_PREFIX_NAME_LENGTH))
  {
    pfs_print_error("build_prefix: prefix+category is too long <%s> <%s>\n",
                    prefix->str, category);
    return 1;
  }

  if (unlikely(strchr(category, '/') != NULL))
  {
    pfs_print_error("build_prefix: invalid category <%s>\n",
                    category);
    return 1;
  }

  /* output = prefix + category + '/' */
  memcpy(out_ptr, prefix->str, prefix_length);
  out_ptr+= prefix_length;
  memcpy(out_ptr, category, len);
  out_ptr+= len;
  *out_ptr= '/';
  out_ptr++;
  *output_length= out_ptr - output;

  return 0;
}

#define REGISTER_BODY_V1(KEY_T, PREFIX, REGISTER_FUNC)                \
  KEY_T key;                                                          \
  char formatted_name[PFS_MAX_INFO_NAME_LENGTH];                      \
  int prefix_length;                                                  \
  int len;                                                            \
  int full_length;                                                    \
                                                                      \
  DBUG_ASSERT(category != NULL);                                      \
  DBUG_ASSERT(info != NULL);                                          \
  if (unlikely(build_prefix(&PREFIX, category,                        \
                   formatted_name, &prefix_length)))                  \
  {                                                                   \
    for (; count>0; count--, info++)                                  \
      *(info->m_key)= 0;                                              \
    return ;                                                          \
  }                                                                   \
                                                                      \
  for (; count>0; count--, info++)                                    \
  {                                                                   \
    DBUG_ASSERT(info->m_key != NULL);                                 \
    DBUG_ASSERT(info->m_name != NULL);                                \
    len= strlen(info->m_name);                                        \
    full_length= prefix_length + len;                                 \
    if (likely(full_length <= PFS_MAX_INFO_NAME_LENGTH))              \
    {                                                                 \
      memcpy(formatted_name + prefix_length, info->m_name, len);      \
      key= REGISTER_FUNC(formatted_name, full_length, info->m_flags); \
    }                                                                 \
    else                                                              \
    {                                                                 \
      pfs_print_error("REGISTER_BODY_V1: name too long <%s> <%s>\n",  \
                      category, info->m_name);                        \
      key= 0;                                                         \
    }                                                                 \
                                                                      \
    *(info->m_key)= key;                                              \
  }                                                                   \
  return;

/* Use C linkage for the interface functions. */

C_MODE_START

/**
  Implementation of the mutex instrumentation interface.
  @sa PSI_v1::register_mutex.
*/
static void register_mutex_v1(const char *category,
                              PSI_mutex_info_v1 *info,
                              int count)
{
  REGISTER_BODY_V1(PSI_mutex_key,
                   mutex_instrument_prefix,
                   register_mutex_class)
}

/**
  Implementation of the rwlock instrumentation interface.
  @sa PSI_v1::register_rwlock.
*/
static void register_rwlock_v1(const char *category,
                               PSI_rwlock_info_v1 *info,
                               int count)
{
  REGISTER_BODY_V1(PSI_rwlock_key,
                   rwlock_instrument_prefix,
                   register_rwlock_class)
}

/**
  Implementation of the cond instrumentation interface.
  @sa PSI_v1::register_cond.
*/
static void register_cond_v1(const char *category,
                             PSI_cond_info_v1 *info,
                             int count)
{
  REGISTER_BODY_V1(PSI_cond_key,
                   cond_instrument_prefix,
                   register_cond_class)
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::register_thread.
*/
static void register_thread_v1(const char *category,
                               PSI_thread_info_v1 *info,
                               int count)
{
  REGISTER_BODY_V1(PSI_thread_key,
                   thread_instrument_prefix,
                   register_thread_class)
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::register_file.
*/
static void register_file_v1(const char *category,
                             PSI_file_info_v1 *info,
                             int count)
{
  REGISTER_BODY_V1(PSI_file_key,
                   file_instrument_prefix,
                   register_file_class)
}

static void register_stage_v1(const char *category,
                              PSI_stage_info_v1 **info_array,
                              int count)
{
  char formatted_name[PFS_MAX_INFO_NAME_LENGTH];
  int prefix_length;
  int len;
  int full_length;
  PSI_stage_info_v1 *info;

  DBUG_ASSERT(category != NULL);
  DBUG_ASSERT(info_array != NULL);
  if (unlikely(build_prefix(&stage_instrument_prefix, category,
               formatted_name, &prefix_length)))
  {
    for (; count>0; count--, info_array++)
      (*info_array)->m_key= 0;
    return ;
  }

  for (; count>0; count--, info_array++)
  {
    info= *info_array;
    DBUG_ASSERT(info != NULL);
    DBUG_ASSERT(info->m_name != NULL);
    len= strlen(info->m_name);
    full_length= prefix_length + len;
    if (likely(full_length <= PFS_MAX_INFO_NAME_LENGTH))
    {
      memcpy(formatted_name + prefix_length, info->m_name, len);
      info->m_key= register_stage_class(formatted_name,
                                        prefix_length,
                                        full_length,
                                        info->m_flags);
    }
    else
    {
      pfs_print_error("register_stage_v1: name too long <%s> <%s>\n",
                      category, info->m_name);
      info->m_key= 0;
    }
  }
  return;
}

static void register_statement_v1(const char *category,
                                  PSI_statement_info_v1 *info,
                                  int count)
{
  char formatted_name[PFS_MAX_INFO_NAME_LENGTH];
  int prefix_length;
  int len;
  int full_length;

  DBUG_ASSERT(category != NULL);
  DBUG_ASSERT(info != NULL);
  if (unlikely(build_prefix(&statement_instrument_prefix,
                            category, formatted_name, &prefix_length)))
  {
    for (; count>0; count--, info++)
      info->m_key= 0;
    return ;
  }

  for (; count>0; count--, info++)
  {
    DBUG_ASSERT(info->m_name != NULL);
    len= strlen(info->m_name);
    full_length= prefix_length + len;
    if (likely(full_length <= PFS_MAX_INFO_NAME_LENGTH))
    {
      memcpy(formatted_name + prefix_length, info->m_name, len);
      info->m_key= register_statement_class(formatted_name, full_length, info->m_flags);
    }
    else
    {
      pfs_print_error("register_statement_v1: name too long <%s>\n",
                      info->m_name);
      info->m_key= 0;
    }
  }
  return;
}

static void register_socket_v1(const char *category,
                             PSI_socket_info_v1 *info,
                             int count)
{
  REGISTER_BODY_V1(PSI_socket_key,
                   socket_instrument_prefix,
                   register_socket_class)
}

#define INIT_BODY_V1(T, KEY, ID)                                            \
  PFS_##T##_class *klass;                                                   \
  PFS_##T *pfs;                                                             \
  klass= find_##T##_class(KEY);                                             \
  if (unlikely(klass == NULL))                                              \
    return NULL;                                                            \
  if (! klass->m_enabled)                                                   \
    return NULL;                                                            \
  pfs= create_##T(klass, ID);                                               \
  return reinterpret_cast<PSI_##T *> (pfs)

/**
  Implementation of the mutex instrumentation interface.
  @sa PSI_v1::init_mutex.
*/
static PSI_mutex*
init_mutex_v1(PSI_mutex_key key, const void *identity)
{
  INIT_BODY_V1(mutex, key, identity);
}

/**
  Implementation of the mutex instrumentation interface.
  @sa PSI_v1::destroy_mutex.
*/
static void destroy_mutex_v1(PSI_mutex* mutex)
{
  PFS_mutex *pfs= reinterpret_cast<PFS_mutex*> (mutex);

  DBUG_ASSERT(pfs != NULL);

  destroy_mutex(pfs);
}

/**
  Implementation of the rwlock instrumentation interface.
  @sa PSI_v1::init_rwlock.
*/
static PSI_rwlock*
init_rwlock_v1(PSI_rwlock_key key, const void *identity)
{
  INIT_BODY_V1(rwlock, key, identity);
}

/**
  Implementation of the rwlock instrumentation interface.
  @sa PSI_v1::destroy_rwlock.
*/
static void destroy_rwlock_v1(PSI_rwlock* rwlock)
{
  PFS_rwlock *pfs= reinterpret_cast<PFS_rwlock*> (rwlock);

  DBUG_ASSERT(pfs != NULL);

  destroy_rwlock(pfs);
}

/**
  Implementation of the cond instrumentation interface.
  @sa PSI_v1::init_cond.
*/
static PSI_cond*
init_cond_v1(PSI_cond_key key, const void *identity)
{
  INIT_BODY_V1(cond, key, identity);
}

/**
  Implementation of the cond instrumentation interface.
  @sa PSI_v1::destroy_cond.
*/
static void destroy_cond_v1(PSI_cond* cond)
{
  PFS_cond *pfs= reinterpret_cast<PFS_cond*> (cond);

  DBUG_ASSERT(pfs != NULL);

  destroy_cond(pfs);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::get_table_share.
*/
static PSI_table_share*
get_table_share_v1(my_bool temporary, TABLE_SHARE *share)
{
  /* Ignore temporary tables and views. */
  if (temporary || share->is_view)
    return NULL;
  /* An instrumented thread is required, for LF_PINS. */
  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(pfs_thread == NULL))
    return NULL;
  PFS_table_share* pfs_share;
  pfs_share= find_or_create_table_share(pfs_thread, temporary, share);
  return reinterpret_cast<PSI_table_share*> (pfs_share);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::release_table_share.
*/
static void release_table_share_v1(PSI_table_share* share)
{
  PFS_table_share* pfs= reinterpret_cast<PFS_table_share*> (share);

  if (unlikely(pfs == NULL))
    return;

  release_table_share(pfs);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::drop_table_share.
*/
static void
drop_table_share_v1(my_bool temporary,
                    const char *schema_name, int schema_name_length,
                    const char *table_name, int table_name_length)
{
  /* Ignore temporary tables. */
  if (temporary)
    return;
  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(pfs_thread == NULL))
    return;
  /* TODO: temporary tables */
  drop_table_share(pfs_thread, temporary, schema_name, schema_name_length,
                   table_name, table_name_length);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::open_table.
*/
static PSI_table*
open_table_v1(PSI_table_share *share, const void *identity)
{
  PFS_table_share *pfs_table_share= reinterpret_cast<PFS_table_share*> (share);

  if (unlikely(pfs_table_share == NULL))
    return NULL;

  /* This object is not to be instrumented. */
  if (! pfs_table_share->m_enabled)
    return NULL;

  /* This object is instrumented, but all table instruments are disabled. */
  if (! global_table_io_class.m_enabled && ! global_table_lock_class.m_enabled)
    return NULL;

  /*
    When the performance schema is off, do not instrument anything.
    Table handles have short life cycle, instrumentation will happen
    again if needed during the next open().
  */
  if (! flag_global_instrumentation)
    return NULL;

  PFS_thread *thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(thread == NULL))
    return NULL;

  PFS_table *pfs_table= create_table(pfs_table_share, thread, identity);
  return reinterpret_cast<PSI_table *> (pfs_table);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::unbind_table.
*/
static void unbind_table_v1(PSI_table *table)
{
  PFS_table *pfs= reinterpret_cast<PFS_table*> (table);
  if (likely(pfs != NULL))
  {
    pfs->m_thread_owner= NULL;
  }
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::rebind_table.
*/
static PSI_table *
rebind_table_v1(PSI_table_share *share, const void *identity, PSI_table *table)
{
  PFS_table *pfs= reinterpret_cast<PFS_table*> (table);
  if (likely(pfs != NULL))
  {
    PFS_thread *thread;
    DBUG_ASSERT(pfs->m_thread_owner == NULL);

    /* The table handle was already instrumented, reuse it for this thread. */
    thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);

    if (unlikely(! pfs->m_share->m_enabled))
    {
      destroy_table(pfs);
      return NULL;
    }

    if (unlikely(! global_table_io_class.m_enabled && ! global_table_lock_class.m_enabled))
    {
      destroy_table(pfs);
      return NULL;
    }

    if (unlikely(! flag_global_instrumentation))
    {
      destroy_table(pfs);
      return NULL;
    }

    pfs->m_thread_owner= thread;
    return table;
  }

  /* See open_table_v1() */

  PFS_table_share *pfs_table_share= reinterpret_cast<PFS_table_share*> (share);

  if (unlikely(pfs_table_share == NULL))
    return NULL;

  if (! pfs_table_share->m_enabled)
    return NULL;

  if (! global_table_io_class.m_enabled && ! global_table_lock_class.m_enabled)
    return NULL;

  if (! flag_global_instrumentation)
    return NULL;

  PFS_thread *thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);

  PFS_table *pfs_table= create_table(pfs_table_share, thread, identity);
  return reinterpret_cast<PSI_table *> (pfs_table);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::close_table.
*/
static void close_table_v1(PSI_table *table)
{
  PFS_table *pfs= reinterpret_cast<PFS_table*> (table);
  if (unlikely(pfs == NULL))
    return;
  pfs->aggregate();
  destroy_table(pfs);
}

static PSI_socket*
init_socket_v1(PSI_socket_key key, const my_socket *fd,
               const struct sockaddr *addr, socklen_t addr_len)
{
  PFS_socket_class *klass;
  PFS_socket *pfs;
  klass= find_socket_class(key);
  if (unlikely(klass == NULL))
    return NULL;
  if (! klass->m_enabled)
    return NULL;
  pfs= create_socket(klass, fd, addr, addr_len);
  return reinterpret_cast<PSI_socket *> (pfs);
}

static void destroy_socket_v1(PSI_socket *socket)
{
  PFS_socket *pfs= reinterpret_cast<PFS_socket*> (socket);

  DBUG_ASSERT(pfs != NULL);

  destroy_socket(pfs);
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::create_file.
*/
static void create_file_v1(PSI_file_key key, const char *name, File file)
{
  if (! flag_global_instrumentation)
    return;
  int index= (int) file;
  if (unlikely(index < 0))
    return;
  PFS_file_class *klass= find_file_class(key);
  if (unlikely(klass == NULL))
    return;
  if (! klass->m_enabled)
    return;

  /* A thread is needed for LF_PINS */
  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(pfs_thread == NULL))
    return;

  if (flag_thread_instrumentation && ! pfs_thread->m_enabled)
    return;

  /*
    We want this check after pfs_thread->m_enabled,
    to avoid reporting false loss.
  */
  if (unlikely(index >= file_handle_max))
  {
    file_handle_lost++;
    return;
  }

  uint len= strlen(name);
  PFS_file *pfs_file= find_or_create_file(pfs_thread, klass, name, len, true);

  file_handle_array[index]= pfs_file;
}

/**
  Arguments given from a parent to a child thread, packaged in one structure.
  This data is used when spawning a new instrumented thread.
  @sa pfs_spawn_thread.
*/
struct PFS_spawn_thread_arg
{
  ulonglong m_thread_internal_id;
  char m_username[USERNAME_LENGTH];
  uint m_username_length;
  char m_hostname[HOSTNAME_LENGTH];
  uint m_hostname_length;

  PSI_thread_key m_child_key;
  const void *m_child_identity;
  void *(*m_user_start_routine)(void*);
  void *m_user_arg;
};

void* pfs_spawn_thread(void *arg)
{
  PFS_spawn_thread_arg *typed_arg= (PFS_spawn_thread_arg*) arg;
  void *user_arg;
  void *(*user_start_routine)(void*);

  PFS_thread *pfs;

  /* First, attach instrumentation to this newly created pthread. */
  PFS_thread_class *klass= find_thread_class(typed_arg->m_child_key);
  if (likely(klass != NULL))
  {
    pfs= create_thread(klass, typed_arg->m_child_identity, 0);
    if (likely(pfs != NULL))
    {
      clear_thread_account(pfs);

      pfs->m_parent_thread_internal_id= typed_arg->m_thread_internal_id;

      memcpy(pfs->m_username, typed_arg->m_username, sizeof(pfs->m_username));
      pfs->m_username_length= typed_arg->m_username_length;

      memcpy(pfs->m_hostname, typed_arg->m_hostname, sizeof(pfs->m_hostname));
      pfs->m_hostname_length= typed_arg->m_hostname_length;

      set_thread_account(pfs);
    }
  }
  else
  {
    pfs= NULL;
  }
  my_pthread_setspecific_ptr(THR_PFS, pfs);

  /*
    Secondly, free the memory allocated in spawn_thread_v1().
    It is preferable to do this before invoking the user
    routine, to avoid memory leaks at shutdown, in case
    the server exits without waiting for this thread.
  */
  user_start_routine= typed_arg->m_user_start_routine;
  user_arg= typed_arg->m_user_arg;
  my_free(typed_arg);

  /* Then, execute the user code for this thread. */
  (*user_start_routine)(user_arg);

  return NULL;
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::spawn_thread.
*/
static int spawn_thread_v1(PSI_thread_key key,
                           pthread_t *thread, const pthread_attr_t *attr,
                           void *(*start_routine)(void*), void *arg)
{
  PFS_spawn_thread_arg *psi_arg;
  PFS_thread *parent;

  /* psi_arg can not be global, and can not be a local variable. */
  psi_arg= (PFS_spawn_thread_arg*) my_malloc(sizeof(PFS_spawn_thread_arg),
                                             MYF(MY_WME));
  if (unlikely(psi_arg == NULL))
    return EAGAIN;

  psi_arg->m_child_key= key;
  psi_arg->m_child_identity= (arg ? arg : thread);
  psi_arg->m_user_start_routine= start_routine;
  psi_arg->m_user_arg= arg;

  parent= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (parent != NULL)
  {
    /*
      Make a copy of the parent attributes.
      This is required, because instrumentation for this thread (the parent)
      may be destroyed before the child thread instrumentation is created.
    */
    psi_arg->m_thread_internal_id= parent->m_thread_internal_id;

    memcpy(psi_arg->m_username, parent->m_username, sizeof(psi_arg->m_username));
    psi_arg->m_username_length= parent->m_username_length;

    memcpy(psi_arg->m_hostname, parent->m_hostname, sizeof(psi_arg->m_hostname));
    psi_arg->m_hostname_length= parent->m_hostname_length;
  }
  else
  {
    psi_arg->m_thread_internal_id= 0;
    psi_arg->m_username_length= 0;
    psi_arg->m_hostname_length= 0;
  }

  int result= pthread_create(thread, attr, pfs_spawn_thread, psi_arg);
  if (unlikely(result != 0))
    my_free(psi_arg);
  return result;
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::new_thread.
*/
static PSI_thread*
new_thread_v1(PSI_thread_key key, const void *identity, ulonglong processlist_id)
{
  PFS_thread *pfs;

  PFS_thread_class *klass= find_thread_class(key);
  if (likely(klass != NULL))
    pfs= create_thread(klass, identity, processlist_id);
  else
    pfs= NULL;

  return reinterpret_cast<PSI_thread*> (pfs);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_id.
*/
static void set_thread_id_v1(PSI_thread *thread, ulonglong processlist_id)
{
  PFS_thread *pfs= reinterpret_cast<PFS_thread*> (thread);
  if (unlikely(pfs == NULL))
    return;
  pfs->m_processlist_id= processlist_id;
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::get_thread_id.
*/
static PSI_thread*
get_thread_v1(void)
{
  PFS_thread *pfs= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  return reinterpret_cast<PSI_thread*> (pfs);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_user.
*/
static void set_thread_user_v1(const char *user, int user_len)
{
  PFS_thread *pfs= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);

  DBUG_ASSERT((user != NULL) || (user_len == 0));
  DBUG_ASSERT(user_len >= 0);
  DBUG_ASSERT((uint) user_len <= sizeof(pfs->m_username));

  if (unlikely(pfs == NULL))
    return;

  aggregate_thread(pfs, pfs->m_account, pfs->m_user, pfs->m_host);

  pfs->m_session_lock.allocated_to_dirty();

  clear_thread_account(pfs);

  if (user_len > 0)
    memcpy(pfs->m_username, user, user_len);
  pfs->m_username_length= user_len;

  set_thread_account(pfs);

  bool enabled= true;
  if (flag_thread_instrumentation)
  {
    if ((pfs->m_username_length > 0) && (pfs->m_hostname_length > 0))
    {
      /*
        TODO: performance improvement.
        Once performance_schema.USERS is exposed,
        we can use PFS_user::m_enabled instead of looking up
        SETUP_ACTORS every time.
      */
      lookup_setup_actor(pfs,
                         pfs->m_username, pfs->m_username_length,
                         pfs->m_hostname, pfs->m_hostname_length,
                         &enabled);
    }
  }

  pfs->m_enabled= enabled;

  pfs->m_session_lock.dirty_to_allocated();
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_account.
*/
static void set_thread_account_v1(const char *user, int user_len,
                                    const char *host, int host_len)
{
  PFS_thread *pfs= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);

  DBUG_ASSERT((user != NULL) || (user_len == 0));
  DBUG_ASSERT(user_len >= 0);
  DBUG_ASSERT((uint) user_len <= sizeof(pfs->m_username));
  DBUG_ASSERT((host != NULL) || (host_len == 0));
  DBUG_ASSERT(host_len >= 0);
  DBUG_ASSERT((uint) host_len <= sizeof(pfs->m_hostname));

  if (unlikely(pfs == NULL))
    return;

  pfs->m_session_lock.allocated_to_dirty();

  clear_thread_account(pfs);

  if (host_len > 0)
    memcpy(pfs->m_hostname, host, host_len);
  pfs->m_hostname_length= host_len;

  if (user_len > 0)
    memcpy(pfs->m_username, user, user_len);
  pfs->m_username_length= user_len;

  set_thread_account(pfs);

  bool enabled= true;
  if (flag_thread_instrumentation)
  {
    if ((pfs->m_username_length > 0) && (pfs->m_hostname_length > 0))
    {
      /*
        TODO: performance improvement.
        Once performance_schema.USERS is exposed,
        we can use PFS_user::m_enabled instead of looking up
        SETUP_ACTORS every time.
      */
      lookup_setup_actor(pfs,
                         pfs->m_username, pfs->m_username_length,
                         pfs->m_hostname, pfs->m_hostname_length,
                         &enabled);
    }
  }
  pfs->m_enabled= enabled;

  pfs->m_session_lock.dirty_to_allocated();
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_db.
*/
static void set_thread_db_v1(const char* db, int db_len)
{
  PFS_thread *pfs= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);

  DBUG_ASSERT((db != NULL) || (db_len == 0));
  DBUG_ASSERT(db_len >= 0);
  DBUG_ASSERT((uint) db_len <= sizeof(pfs->m_dbname));

  if (likely(pfs != NULL))
  {
    pfs->m_stmt_lock.allocated_to_dirty();
    if (db_len > 0)
      memcpy(pfs->m_dbname, db, db_len);
    pfs->m_dbname_length= db_len;
    pfs->m_stmt_lock.dirty_to_allocated();
  }
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_command.
*/
static void set_thread_command_v1(int command)
{
  PFS_thread *pfs= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);

  DBUG_ASSERT(command >= 0);
  DBUG_ASSERT(command <= (int) COM_END);

  if (likely(pfs != NULL))
  {
    pfs->m_command= command;
  }
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_start_time.
*/
static void set_thread_start_time_v1(time_t start_time)
{
  PFS_thread *pfs= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);

  if (likely(pfs != NULL))
  {
    pfs->m_start_time= start_time;
  }
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_state.
*/
static void set_thread_state_v1(const char* state)
{
  /* DEPRECATED. */
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread_info.
*/
static void set_thread_info_v1(const char* info, uint info_len)
{
  PFS_thread *pfs= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);

  DBUG_ASSERT((info != NULL) || (info_len == 0));

  if (likely(pfs != NULL))
  {
    if ((info != NULL) && (info_len > 0))
    {
      if (info_len > sizeof(pfs->m_processlist_info))
        info_len= sizeof(pfs->m_processlist_info);

      pfs->m_stmt_lock.allocated_to_dirty();
      memcpy(pfs->m_processlist_info, info, info_len);
      pfs->m_processlist_info_length= info_len;
      pfs->m_stmt_lock.dirty_to_allocated();
    }
    else
    {
      pfs->m_stmt_lock.allocated_to_dirty();
      pfs->m_processlist_info_length= 0;
      pfs->m_stmt_lock.dirty_to_allocated();
    }
  }
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::set_thread.
*/
static void set_thread_v1(PSI_thread* thread)
{
  PFS_thread *pfs= reinterpret_cast<PFS_thread*> (thread);
  my_pthread_setspecific_ptr(THR_PFS, pfs);
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::delete_current_thread.
*/
static void delete_current_thread_v1(void)
{
  PFS_thread *thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (thread != NULL)
  {
    aggregate_thread(thread, thread->m_account, thread->m_user, thread->m_host);
    my_pthread_setspecific_ptr(THR_PFS, NULL);
    destroy_thread(thread);
  }
}

/**
  Implementation of the thread instrumentation interface.
  @sa PSI_v1::delete_thread.
*/
static void delete_thread_v1(PSI_thread *thread)
{
  PFS_thread *pfs= reinterpret_cast<PFS_thread*> (thread);

  if (pfs != NULL)
  {
    aggregate_thread(pfs, pfs->m_account, pfs->m_user, pfs->m_host);
    destroy_thread(pfs);
  }
}

/**
  Implementation of the mutex instrumentation interface.
  @sa PSI_v1::start_mutex_wait.
*/
static PSI_mutex_locker*
start_mutex_wait_v1(PSI_mutex_locker_state *state,
                    PSI_mutex *mutex, PSI_mutex_operation op,
                    const char *src_file, uint src_line)
{
  PFS_mutex *pfs_mutex= reinterpret_cast<PFS_mutex*> (mutex);
  DBUG_ASSERT((int) op >= 0);
  DBUG_ASSERT((uint) op < array_elements(mutex_operation_map));
  DBUG_ASSERT(state != NULL);

  DBUG_ASSERT(pfs_mutex != NULL);
  DBUG_ASSERT(pfs_mutex->m_class != NULL);

  if (! pfs_mutex->m_enabled)
    return NULL;

  register uint flags;
  ulonglong timer_start= 0;

  if (flag_thread_instrumentation)
  {
    PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
    if (unlikely(pfs_thread == NULL))
      return NULL;
    if (! pfs_thread->m_enabled)
      return NULL;
    state->m_thread= reinterpret_cast<PSI_thread *> (pfs_thread);
    flags= STATE_FLAG_THREAD;

    if (pfs_mutex->m_timed)
    {
      timer_start= get_timer_raw_value_and_function(wait_timer, & state->m_timer);
      state->m_timer_start= timer_start;
      flags|= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current)
    {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   & pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE]))
      {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait= pfs_thread->m_events_waits_current;
      state->m_wait= wait;
      flags|= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event= wait - 1;
      wait->m_event_type= EVENT_TYPE_WAIT;
      wait->m_nesting_event_id= parent_event->m_event_id;
      wait->m_nesting_event_type= parent_event->m_event_type;

      wait->m_thread= pfs_thread;
      wait->m_class= pfs_mutex->m_class;
      wait->m_timer_start= timer_start;
      wait->m_timer_end= 0;
      wait->m_object_instance_addr= pfs_mutex->m_identity;
      wait->m_event_id= pfs_thread->m_event_id++;
      wait->m_end_event_id= 0;
      wait->m_operation= mutex_operation_map[(int) op];
      wait->m_source_file= src_file;
      wait->m_source_line= src_line;
      wait->m_wait_class= WAIT_CLASS_MUTEX;

      pfs_thread->m_events_waits_current++;
    }
  }
  else
  {
    if (pfs_mutex->m_timed)
    {
      timer_start= get_timer_raw_value_and_function(wait_timer, & state->m_timer);
      state->m_timer_start= timer_start;
      flags= STATE_FLAG_TIMED;
      state->m_thread= NULL;
    }
    else
    {
      /*
        Complete shortcut.
      */
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
      pfs_mutex->m_mutex_stat.m_wait_stat.aggregate_counted();
      return NULL;
    }
  }

  state->m_flags= flags;
  state->m_mutex= mutex;
  return reinterpret_cast<PSI_mutex_locker*> (state);
}

/**
  Implementation of the rwlock instrumentation interface.
  @sa PSI_v1::start_rwlock_rdwait
  @sa PSI_v1::start_rwlock_wrwait
*/
static PSI_rwlock_locker*
start_rwlock_wait_v1(PSI_rwlock_locker_state *state,
                     PSI_rwlock *rwlock,
                     PSI_rwlock_operation op,
                     const char *src_file, uint src_line)
{
  PFS_rwlock *pfs_rwlock= reinterpret_cast<PFS_rwlock*> (rwlock);
  DBUG_ASSERT(static_cast<int> (op) >= 0);
  DBUG_ASSERT(static_cast<uint> (op) < array_elements(rwlock_operation_map));
  DBUG_ASSERT(state != NULL);
  DBUG_ASSERT(pfs_rwlock != NULL);
  DBUG_ASSERT(pfs_rwlock->m_class != NULL);

  if (! pfs_rwlock->m_enabled)
    return NULL;

  register uint flags;
  ulonglong timer_start= 0;

  if (flag_thread_instrumentation)
  {
    PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
    if (unlikely(pfs_thread == NULL))
      return NULL;
    if (! pfs_thread->m_enabled)
      return NULL;
    state->m_thread= reinterpret_cast<PSI_thread *> (pfs_thread);
    flags= STATE_FLAG_THREAD;

    if (pfs_rwlock->m_timed)
    {
      timer_start= get_timer_raw_value_and_function(wait_timer, & state->m_timer);
      state->m_timer_start= timer_start;
      flags|= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current)
    {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   & pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE]))
      {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait= pfs_thread->m_events_waits_current;
      state->m_wait= wait;
      flags|= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event= wait - 1;
      wait->m_event_type= EVENT_TYPE_WAIT;
      wait->m_nesting_event_id= parent_event->m_event_id;
      wait->m_nesting_event_type= parent_event->m_event_type;

      wait->m_thread= pfs_thread;
      wait->m_class= pfs_rwlock->m_class;
      wait->m_timer_start= timer_start;
      wait->m_timer_end= 0;
      wait->m_object_instance_addr= pfs_rwlock->m_identity;
      wait->m_event_id= pfs_thread->m_event_id++;
      wait->m_end_event_id= 0;
      wait->m_operation= rwlock_operation_map[static_cast<int> (op)];
      wait->m_source_file= src_file;
      wait->m_source_line= src_line;
      wait->m_wait_class= WAIT_CLASS_RWLOCK;

      pfs_thread->m_events_waits_current++;
    }
  }
  else
  {
    if (pfs_rwlock->m_timed)
    {
      timer_start= get_timer_raw_value_and_function(wait_timer, & state->m_timer);
      state->m_timer_start= timer_start;
      flags= STATE_FLAG_TIMED;
      state->m_thread= NULL;
    }
    else
    {
      /*
        Complete shortcut.
      */
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
      pfs_rwlock->m_rwlock_stat.m_wait_stat.aggregate_counted();
      return NULL;
    }
  }

  state->m_flags= flags;
  state->m_rwlock= rwlock;
  return reinterpret_cast<PSI_rwlock_locker*> (state);
}

/**
  Implementation of the cond instrumentation interface.
  @sa PSI_v1::start_cond_wait.
*/
static PSI_cond_locker*
start_cond_wait_v1(PSI_cond_locker_state *state,
                   PSI_cond *cond, PSI_mutex *mutex,
                   PSI_cond_operation op,
                   const char *src_file, uint src_line)
{
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
  PFS_cond *pfs_cond= reinterpret_cast<PFS_cond*> (cond);
  DBUG_ASSERT(static_cast<int> (op) >= 0);
  DBUG_ASSERT(static_cast<uint> (op) < array_elements(cond_operation_map));
  DBUG_ASSERT(state != NULL);
  DBUG_ASSERT(pfs_cond != NULL);
  DBUG_ASSERT(pfs_cond->m_class != NULL);

  if (! pfs_cond->m_enabled)
    return NULL;

  register uint flags;
  ulonglong timer_start= 0;

  if (flag_thread_instrumentation)
  {
    PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
    if (unlikely(pfs_thread == NULL))
      return NULL;
    if (! pfs_thread->m_enabled)
      return NULL;
    state->m_thread= reinterpret_cast<PSI_thread *> (pfs_thread);
    flags= STATE_FLAG_THREAD;

    if (pfs_cond->m_timed)
    {
      timer_start= get_timer_raw_value_and_function(wait_timer, & state->m_timer);
      state->m_timer_start= timer_start;
      flags|= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current)
    {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   & pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE]))
      {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait= pfs_thread->m_events_waits_current;
      state->m_wait= wait;
      flags|= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event= wait - 1;
      wait->m_event_type= EVENT_TYPE_WAIT;
      wait->m_nesting_event_id= parent_event->m_event_id;
      wait->m_nesting_event_type= parent_event->m_event_type;

      wait->m_thread= pfs_thread;
      wait->m_class= pfs_cond->m_class;
      wait->m_timer_start= timer_start;
      wait->m_timer_end= 0;
      wait->m_object_instance_addr= pfs_cond->m_identity;
      wait->m_event_id= pfs_thread->m_event_id++;
      wait->m_end_event_id= 0;
      wait->m_operation= cond_operation_map[static_cast<int> (op)];
      wait->m_source_file= src_file;
      wait->m_source_line= src_line;
      wait->m_wait_class= WAIT_CLASS_COND;

      pfs_thread->m_events_waits_current++;
    }
  }
  else
  {
    if (pfs_cond->m_timed)
    {
      timer_start= get_timer_raw_value_and_function(wait_timer, & state->m_timer);
      state->m_timer_start= timer_start;
      flags= STATE_FLAG_TIMED;
    }
    else
    {
      /*
        Complete shortcut.
      */
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
      pfs_cond->m_cond_stat.m_wait_stat.aggregate_counted();
      return NULL;
    }
  }

  state->m_flags= flags;
  state->m_cond= cond;
  state->m_mutex= mutex;
  return reinterpret_cast<PSI_cond_locker*> (state);
}

static inline PFS_TL_LOCK_TYPE lock_flags_to_lock_type(uint flags)
{
  enum thr_lock_type value= static_cast<enum thr_lock_type> (flags);

  switch (value)
  {
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
    case TL_WRITE_DELAYED:
      return PFS_TL_WRITE_DELAYED;
    case TL_WRITE_LOW_PRIORITY:
      return PFS_TL_WRITE_LOW_PRIORITY;
    case TL_WRITE:
      return PFS_TL_WRITE;

    case TL_WRITE_ONLY:
    case TL_IGNORE:
    case TL_UNLOCK:
    case TL_READ_DEFAULT:
    case TL_WRITE_DEFAULT:
    default:
      DBUG_ASSERT(false);
  }

  /* Dead code */
  return PFS_TL_READ;
}

static inline PFS_TL_LOCK_TYPE external_lock_flags_to_lock_type(uint flags)
{
  DBUG_ASSERT(flags == F_RDLCK || flags == F_WRLCK);
  return (flags == F_RDLCK ? PFS_TL_READ_EXTERNAL : PFS_TL_WRITE_EXTERNAL);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::start_table_io_wait_v1
*/
static PSI_table_locker*
start_table_io_wait_v1(PSI_table_locker_state *state,
                       PSI_table *table,
                       PSI_table_io_operation op,
                       uint index,
                       const char *src_file, uint src_line)
{
  DBUG_ASSERT(static_cast<int> (op) >= 0);
  DBUG_ASSERT(static_cast<uint> (op) < array_elements(table_io_operation_map));
  DBUG_ASSERT(state != NULL);
  PFS_table *pfs_table= reinterpret_cast<PFS_table*> (table);
  DBUG_ASSERT(pfs_table != NULL);
  DBUG_ASSERT(pfs_table->m_share != NULL);

  if (! pfs_table->m_io_enabled)
    return NULL;

  PFS_thread *pfs_thread= pfs_table->m_thread_owner;

  DBUG_ASSERT(pfs_thread ==
              my_pthread_getspecific_ptr(PFS_thread*, THR_PFS));

  register uint flags;
  ulonglong timer_start= 0;

  if (flag_thread_instrumentation)
  {
    if (pfs_thread == NULL)
      return NULL;
    if (! pfs_thread->m_enabled)
      return NULL;
    state->m_thread= reinterpret_cast<PSI_thread *> (pfs_thread);
    flags= STATE_FLAG_THREAD;

    if (pfs_table->m_io_timed)
    {
      timer_start= get_timer_raw_value_and_function(wait_timer, & state->m_timer);
      state->m_timer_start= timer_start;
      flags|= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current)
    {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   & pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE]))
      {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait= pfs_thread->m_events_waits_current;
      state->m_wait= wait;
      flags|= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event= wait - 1;
      wait->m_event_type= EVENT_TYPE_WAIT;
      wait->m_nesting_event_id= parent_event->m_event_id;
      wait->m_nesting_event_type= parent_event->m_event_type;

      PFS_table_share *share= pfs_table->m_share;
      wait->m_thread= pfs_thread;
      wait->m_class= &global_table_io_class;
      wait->m_timer_start= timer_start;
      wait->m_timer_end= 0;
      wait->m_object_instance_addr= pfs_table->m_identity;
      wait->m_event_id= pfs_thread->m_event_id++;
      wait->m_end_event_id= 0;
      wait->m_operation= table_io_operation_map[static_cast<int> (op)];
      wait->m_flags= 0;
      wait->m_object_type= share->get_object_type();
      wait->m_weak_table_share= share;
      wait->m_weak_version= share->get_version();
      wait->m_index= index;
      wait->m_source_file= src_file;
      wait->m_source_line= src_line;
      wait->m_wait_class= WAIT_CLASS_TABLE;

      pfs_thread->m_events_waits_current++;
    }
  }
  else
  {
    if (pfs_table->m_io_timed)
    {
      timer_start= get_timer_raw_value_and_function(wait_timer, & state->m_timer);
      state->m_timer_start= timer_start;
      flags= STATE_FLAG_TIMED;
    }
    else
    {
      /* TODO: consider a shortcut here */
      flags= 0;
    }
  }

  state->m_flags= flags;
  state->m_table= table;
  state->m_io_operation= op;
  state->m_index= index;
  return reinterpret_cast<PSI_table_locker*> (state);
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::start_table_lock_wait.
*/
static PSI_table_locker*
start_table_lock_wait_v1(PSI_table_locker_state *state,
                         PSI_table *table,
                         PSI_table_lock_operation op,
                         ulong op_flags,
                         const char *src_file, uint src_line)
{
  DBUG_ASSERT(state != NULL);
  DBUG_ASSERT((op == PSI_TABLE_LOCK) || (op == PSI_TABLE_EXTERNAL_LOCK));

  PFS_table *pfs_table= reinterpret_cast<PFS_table*> (table);

  DBUG_ASSERT(pfs_table != NULL);
  DBUG_ASSERT(pfs_table->m_share != NULL);

  if (! pfs_table->m_lock_enabled)
    return NULL;

  PFS_thread *pfs_thread= pfs_table->m_thread_owner;

  PFS_TL_LOCK_TYPE lock_type;

  switch (op)
  {
    case PSI_TABLE_LOCK:
      lock_type= lock_flags_to_lock_type(op_flags);
      break;
    case PSI_TABLE_EXTERNAL_LOCK:
      /*
        See the handler::external_lock() API design,
        there is no handler::external_unlock().
      */
      if (op_flags == F_UNLCK)
        return NULL;
      lock_type= external_lock_flags_to_lock_type(op_flags);
      break;
    default:
      lock_type= PFS_TL_READ;
      DBUG_ASSERT(false);
  }

  DBUG_ASSERT((uint) lock_type < array_elements(table_lock_operation_map));

  register uint flags;
  ulonglong timer_start= 0;

  if (flag_thread_instrumentation)
  {
    if (pfs_thread == NULL)
      return NULL;
    if (! pfs_thread->m_enabled)
      return NULL;
    state->m_thread= reinterpret_cast<PSI_thread *> (pfs_thread);
    flags= STATE_FLAG_THREAD;

    if (pfs_table->m_lock_timed)
    {
      timer_start= get_timer_raw_value_and_function(wait_timer, & state->m_timer);
      state->m_timer_start= timer_start;
      flags|= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current)
    {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   & pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE]))
      {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait= pfs_thread->m_events_waits_current;
      state->m_wait= wait;
      flags|= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event= wait - 1;
      wait->m_event_type= EVENT_TYPE_WAIT;
      wait->m_nesting_event_id= parent_event->m_event_id;
      wait->m_nesting_event_type= parent_event->m_event_type;

      PFS_table_share *share= pfs_table->m_share;
      wait->m_thread= pfs_thread;
      wait->m_class= &global_table_lock_class;
      wait->m_timer_start= timer_start;
      wait->m_timer_end= 0;
      wait->m_object_instance_addr= pfs_table->m_identity;
      wait->m_event_id= pfs_thread->m_event_id++;
      wait->m_end_event_id= 0;
      wait->m_operation= table_lock_operation_map[lock_type];
      wait->m_flags= 0;
      wait->m_object_type= share->get_object_type();
      wait->m_weak_table_share= share;
      wait->m_weak_version= share->get_version();
      wait->m_index= 0;
      wait->m_source_file= src_file;
      wait->m_source_line= src_line;
      wait->m_wait_class= WAIT_CLASS_TABLE;

      pfs_thread->m_events_waits_current++;
    }
  }
  else
  {
    if (pfs_table->m_lock_timed)
    {
      timer_start= get_timer_raw_value_and_function(wait_timer, & state->m_timer);
      state->m_timer_start= timer_start;
      flags= STATE_FLAG_TIMED;
    }
    else
    {
      /* TODO: consider a shortcut here */
      flags= 0;
    }
  }

  state->m_flags= flags;
  state->m_table= table;
  state->m_index= lock_type;
  return reinterpret_cast<PSI_table_locker*> (state);
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::get_thread_file_name_locker.
*/
static PSI_file_locker*
get_thread_file_name_locker_v1(PSI_file_locker_state *state,
                               PSI_file_key key,
                               PSI_file_operation op,
                               const char *name, const void *identity)
{
  DBUG_ASSERT(static_cast<int> (op) >= 0);
  DBUG_ASSERT(static_cast<uint> (op) < array_elements(file_operation_map));
  DBUG_ASSERT(state != NULL);

  if (! flag_global_instrumentation)
    return NULL;
  PFS_file_class *klass= find_file_class(key);
  if (unlikely(klass == NULL))
    return NULL;
  if (! klass->m_enabled)
    return NULL;

  /* Needed for the LF_HASH */
  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(pfs_thread == NULL))
    return NULL;

  if (flag_thread_instrumentation && ! pfs_thread->m_enabled)
    return NULL;

  register uint flags;

  state->m_thread= reinterpret_cast<PSI_thread *> (pfs_thread);
  flags= STATE_FLAG_THREAD;

  if (klass->m_timed)
    flags|= STATE_FLAG_TIMED;

  if (flag_events_waits_current)
  {
    if (unlikely(pfs_thread->m_events_waits_current >=
                 & pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE]))
    {
      locker_lost++;
      return NULL;
    }
    PFS_events_waits *wait= pfs_thread->m_events_waits_current;
    state->m_wait= wait;
    flags|= STATE_FLAG_EVENT;

    PFS_events_waits *parent_event= wait - 1;
    wait->m_event_type= EVENT_TYPE_WAIT;
    wait->m_nesting_event_id= parent_event->m_event_id;
    wait->m_nesting_event_type= parent_event->m_event_type;

    wait->m_thread= pfs_thread;
    wait->m_class= klass;
    wait->m_timer_start= 0;
    wait->m_timer_end= 0;
    wait->m_object_instance_addr= NULL;
    wait->m_weak_file= NULL;
    wait->m_weak_version= 0;
    wait->m_event_id= pfs_thread->m_event_id++;
    wait->m_end_event_id= 0;
    wait->m_operation= file_operation_map[static_cast<int> (op)];
    wait->m_wait_class= WAIT_CLASS_FILE;

    pfs_thread->m_events_waits_current++;
  }

  state->m_flags= flags;
  state->m_file= NULL;
  state->m_name= name;
  state->m_class= klass;
  state->m_operation= op;
  return reinterpret_cast<PSI_file_locker*> (state);
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::get_thread_file_stream_locker.
*/
static PSI_file_locker*
get_thread_file_stream_locker_v1(PSI_file_locker_state *state,
                                 PSI_file *file, PSI_file_operation op)
{
  PFS_file *pfs_file= reinterpret_cast<PFS_file*> (file);
  DBUG_ASSERT(static_cast<int> (op) >= 0);
  DBUG_ASSERT(static_cast<uint> (op) < array_elements(file_operation_map));
  DBUG_ASSERT(state != NULL);

  if (unlikely(pfs_file == NULL))
    return NULL;
  DBUG_ASSERT(pfs_file->m_class != NULL);
  PFS_file_class *klass= pfs_file->m_class;

  if (! pfs_file->m_enabled)
    return NULL;

  register uint flags;

  if (flag_thread_instrumentation)
  {
    PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
    if (unlikely(pfs_thread == NULL))
      return NULL;
    if (! pfs_thread->m_enabled)
      return NULL;
    state->m_thread= reinterpret_cast<PSI_thread *> (pfs_thread);
    flags= STATE_FLAG_THREAD;

    if (pfs_file->m_timed)
      flags|= STATE_FLAG_TIMED;

    if (flag_events_waits_current)
    {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   & pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE]))
      {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait= pfs_thread->m_events_waits_current;
      state->m_wait= wait;
      flags|= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event= wait - 1;
      wait->m_event_type= EVENT_TYPE_WAIT;
      wait->m_nesting_event_id= parent_event->m_event_id;
      wait->m_nesting_event_type= parent_event->m_event_type;

      wait->m_thread= pfs_thread;
      wait->m_class= klass;
      wait->m_timer_start= 0;
      wait->m_timer_end= 0;
      wait->m_object_instance_addr= pfs_file;
      wait->m_weak_file= pfs_file;
      wait->m_weak_version= pfs_file->get_version();
      wait->m_event_id= pfs_thread->m_event_id++;
      wait->m_end_event_id= 0;
      wait->m_operation= file_operation_map[static_cast<int> (op)];
      wait->m_wait_class= WAIT_CLASS_FILE;

      pfs_thread->m_events_waits_current++;
    }
  }
  else
  {
    state->m_thread= NULL;
    if (pfs_file->m_timed)
    {
      flags= STATE_FLAG_TIMED;
    }
    else
    {
      /* TODO: consider a shortcut. */
      flags= 0;
    }
  }

  state->m_flags= flags;
  state->m_file= reinterpret_cast<PSI_file*> (pfs_file);
  state->m_operation= op;
  state->m_name= NULL;
  state->m_class= klass;
  return reinterpret_cast<PSI_file_locker*> (state);
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::get_thread_file_descriptor_locker.
*/
static PSI_file_locker*
get_thread_file_descriptor_locker_v1(PSI_file_locker_state *state,
                                     File file, PSI_file_operation op)
{
  int index= static_cast<int> (file);
  DBUG_ASSERT(static_cast<int> (op) >= 0);
  DBUG_ASSERT(static_cast<uint> (op) < array_elements(file_operation_map));
  DBUG_ASSERT(state != NULL);

  if (unlikely((index < 0) || (index >= file_handle_max)))
    return NULL;

  PFS_file *pfs_file= file_handle_array[index];
  if (unlikely(pfs_file == NULL))
    return NULL;

  /*
    We are about to close a file by descriptor number,
    and the calling code still holds the descriptor.
    Cleanup the file descriptor <--> file instrument association.
    Remove the instrumentation *before* the close to avoid race
    conditions with another thread opening a file
    (that could be given the same descriptor).
  */
  if (op == PSI_FILE_CLOSE)
    file_handle_array[index]= NULL;

  if (! pfs_file->m_enabled)
    return NULL;

  DBUG_ASSERT(pfs_file->m_class != NULL);
  PFS_file_class *klass= pfs_file->m_class;

  register uint flags;

  if (flag_thread_instrumentation)
  {
    PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
    if (unlikely(pfs_thread == NULL))
      return NULL;
    if (! pfs_thread->m_enabled)
      return NULL;
    state->m_thread= reinterpret_cast<PSI_thread *> (pfs_thread);
    flags= STATE_FLAG_THREAD;

    if (pfs_file->m_timed)
      flags|= STATE_FLAG_TIMED;

    if (flag_events_waits_current)
    {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   & pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE]))
      {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait= pfs_thread->m_events_waits_current;
      state->m_wait= wait;
      flags|= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event= wait - 1;
      wait->m_event_type= EVENT_TYPE_WAIT;
      wait->m_nesting_event_id= parent_event->m_event_id;
      wait->m_nesting_event_type= parent_event->m_event_type;

      wait->m_thread= pfs_thread;
      wait->m_class= klass;
      wait->m_timer_start= 0;
      wait->m_timer_end= 0;
      wait->m_object_instance_addr= pfs_file;
      wait->m_weak_file= pfs_file;
      wait->m_weak_version= pfs_file->get_version();
      wait->m_event_id= pfs_thread->m_event_id++;
      wait->m_end_event_id= 0;
      wait->m_operation= file_operation_map[static_cast<int> (op)];
      wait->m_wait_class= WAIT_CLASS_FILE;

      pfs_thread->m_events_waits_current++;
    }
  }
  else
  {
    state->m_thread= NULL;
    if (pfs_file->m_timed)
    {
      flags= STATE_FLAG_TIMED;
    }
    else
    {
      /* TODO: consider a shortcut. */
      flags= 0;
    }
  }

  state->m_flags= flags;
  state->m_file= reinterpret_cast<PSI_file*> (pfs_file);
  state->m_operation= op;
  state->m_name= NULL;
  state->m_class= klass;
  return reinterpret_cast<PSI_file_locker*> (state);
}

/** Socket locker */

static PSI_socket_locker*
start_socket_wait_v1(PSI_socket_locker_state *state,
                     PSI_socket *socket,
                     PSI_socket_operation op,
                     size_t count,
                     const char *src_file, uint src_line)
{
  DBUG_ASSERT(static_cast<int> (op) >= 0);
  DBUG_ASSERT(static_cast<uint> (op) < array_elements(socket_operation_map));
  DBUG_ASSERT(state != NULL);
  PFS_socket *pfs_socket= reinterpret_cast<PFS_socket*> (socket);

  DBUG_ASSERT(pfs_socket != NULL);
  DBUG_ASSERT(pfs_socket->m_class != NULL);

  if (!pfs_socket->m_enabled || pfs_socket->m_idle)
    return NULL;

  register uint flags= 0;
  ulonglong timer_start= 0;

  if (flag_thread_instrumentation)
  {
    PFS_thread *pfs_thread= pfs_socket->m_thread_owner;

    if (unlikely(pfs_thread == NULL))
      return NULL;

    if (!pfs_thread->m_enabled)
      return NULL;

    state->m_thread= reinterpret_cast<PSI_thread *> (pfs_thread);
    flags= STATE_FLAG_THREAD;

    if (pfs_socket->m_timed)
    {
      timer_start= get_timer_raw_value_and_function(wait_timer, & state->m_timer);
      state->m_timer_start= timer_start;
      flags|= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current)
    {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   & pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE]))
      {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait= pfs_thread->m_events_waits_current;
      state->m_wait= wait;
      flags|= STATE_FLAG_EVENT;

      PFS_events_waits *parent_event= wait - 1;
      wait->m_event_type= EVENT_TYPE_WAIT;
      wait->m_nesting_event_id=   parent_event->m_event_id;
      wait->m_nesting_event_type= parent_event->m_event_type;
      wait->m_thread=       pfs_thread;
      wait->m_class=        pfs_socket->m_class;
      wait->m_timer_start=  timer_start;
      wait->m_timer_end=    0;
      wait->m_object_instance_addr= pfs_socket->m_identity;
      wait->m_weak_socket=  pfs_socket;
      wait->m_weak_version= pfs_socket->get_version();
      wait->m_event_id=     pfs_thread->m_event_id++;
      wait->m_end_event_id= 0;
      wait->m_operation=    socket_operation_map[static_cast<int>(op)];
      wait->m_source_file= src_file;
      wait->m_source_line= src_line;
      wait->m_number_of_bytes= count;
      wait->m_wait_class=   WAIT_CLASS_SOCKET;

      pfs_thread->m_events_waits_current++;
    }
  }
  else
  {
    if (pfs_socket->m_timed)
    {
      timer_start= get_timer_raw_value_and_function(wait_timer, & state->m_timer);
      state->m_timer_start= timer_start;
      flags= STATE_FLAG_TIMED;
    }
    else
    {
      /*
        Even if timing is disabled, end_socket_wait() still needs a locker to
        capture the number of bytes sent or received by the socket operation.
        For operations that do not have a byte count, then just increment the
        event counter and return a NULL locker.
      */
      switch (op)
      {
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

  state->m_flags= flags;
  state->m_socket= socket;
  state->m_operation= op;
  return reinterpret_cast<PSI_socket_locker*> (state);
}

/**
  Implementation of the mutex instrumentation interface.
  @sa PSI_v1::unlock_mutex.
*/
static void unlock_mutex_v1(PSI_mutex *mutex)
{
  PFS_mutex *pfs_mutex= reinterpret_cast<PFS_mutex*> (mutex);

  DBUG_ASSERT(pfs_mutex != NULL);

  /*
    Note that this code is still protected by the instrumented mutex,
    and therefore is thread safe. See inline_mysql_mutex_unlock().
  */

  /* Always update the instrumented state */
  pfs_mutex->m_owner= NULL;
  pfs_mutex->m_last_locked= 0;

#ifdef LATER_WL2333
  /*
    See WL#2333: SHOW ENGINE ... LOCK STATUS.
    PFS_mutex::m_lock_stat is not exposed in user visible tables
    currently, so there is no point spending time computing it.
  */
  if (! pfs_mutex->m_enabled)
    return;

  if (! pfs_mutex->m_timed)
    return;

  ulonglong locked_time;
  locked_time= get_timer_pico_value(wait_timer) - pfs_mutex->m_last_locked;
  pfs_mutex->m_mutex_stat.m_lock_stat.aggregate_value(locked_time);
#endif
}

/**
  Implementation of the rwlock instrumentation interface.
  @sa PSI_v1::unlock_rwlock.
*/
static void unlock_rwlock_v1(PSI_rwlock *rwlock)
{
  PFS_rwlock *pfs_rwlock= reinterpret_cast<PFS_rwlock*> (rwlock);
  DBUG_ASSERT(pfs_rwlock != NULL);
  DBUG_ASSERT(pfs_rwlock == sanitize_rwlock(pfs_rwlock));
  DBUG_ASSERT(pfs_rwlock->m_class != NULL);
  DBUG_ASSERT(pfs_rwlock->m_lock.is_populated());

  bool last_writer= false;
  bool last_reader= false;

  /*
    Note that this code is still protected by the instrumented rwlock,
    and therefore is:
    - thread safe for write locks
    - almost thread safe for read locks (pfs_rwlock->m_readers is unsafe).
    See inline_mysql_rwlock_unlock()
  */

  /* Always update the instrumented state */
  if (pfs_rwlock->m_writer != NULL)
  {
    /* Nominal case, a writer is unlocking. */
    last_writer= true;
    pfs_rwlock->m_writer= NULL;
    /* Reset the readers stats, they could be off */
    pfs_rwlock->m_readers= 0;
  }
  else if (likely(pfs_rwlock->m_readers > 0))
  {
    /* Nominal case, a reader is unlocking. */
    if (--(pfs_rwlock->m_readers) == 0)
      last_reader= true;
  }
  else
  {
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

  if (! pfs_rwlock->m_enabled)
    return;

  if (! pfs_rwlock->m_timed)
    return;

  ulonglong locked_time;
  if (last_writer)
  {
    locked_time= get_timer_pico_value(wait_timer) - pfs_rwlock->m_last_written;
    pfs_rwlock->m_rwlock_stat.m_write_lock_stat.aggregate_value(locked_time);
  }
  else if (last_reader)
  {
    locked_time= get_timer_pico_value(wait_timer) - pfs_rwlock->m_last_read;
    pfs_rwlock->m_rwlock_stat.m_read_lock_stat.aggregate_value(locked_time);
  }
#else
  (void) last_reader;
  (void) last_writer;
#endif
}

/**
  Implementation of the cond instrumentation interface.
  @sa PSI_v1::signal_cond.
*/
static void signal_cond_v1(PSI_cond* cond)
{
  PFS_cond *pfs_cond= reinterpret_cast<PFS_cond*> (cond);

  DBUG_ASSERT(pfs_cond != NULL);

  pfs_cond->m_cond_stat.m_signal_count++;
}

/**
  Implementation of the cond instrumentation interface.
  @sa PSI_v1::broadcast_cond.
*/
static void broadcast_cond_v1(PSI_cond* cond)
{
  PFS_cond *pfs_cond= reinterpret_cast<PFS_cond*> (cond);

  DBUG_ASSERT(pfs_cond != NULL);

  pfs_cond->m_cond_stat.m_broadcast_count++;
}

/**
  Implementation of the idle instrumentation interface.
  @sa PSI_v1::start_idle_wait.
*/
static PSI_idle_locker*
start_idle_wait_v1(PSI_idle_locker_state* state, const char *src_file, uint src_line)
{
  DBUG_ASSERT(state != NULL);

  if (!flag_global_instrumentation)
    return NULL;

  if (!global_idle_class.m_enabled)
    return NULL;

  register uint flags= 0;
  ulonglong timer_start= 0;

  if (flag_thread_instrumentation)
  {
    PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
    if (unlikely(pfs_thread == NULL))
      return NULL;
    if (!pfs_thread->m_enabled)
      return NULL;
    state->m_thread= reinterpret_cast<PSI_thread *> (pfs_thread);
    flags= STATE_FLAG_THREAD;

    DBUG_ASSERT(pfs_thread->m_events_statements_count == 0);

    if (global_idle_class.m_timed)
    {
      timer_start= get_timer_raw_value_and_function(idle_timer, &state->m_timer);
      state->m_timer_start= timer_start;
      flags|= STATE_FLAG_TIMED;
    }

    if (flag_events_waits_current)
    {
      if (unlikely(pfs_thread->m_events_waits_current >=
                   & pfs_thread->m_events_waits_stack[WAIT_STACK_SIZE]))
      {
        locker_lost++;
        return NULL;
      }
      PFS_events_waits *wait= pfs_thread->m_events_waits_current;
      state->m_wait= wait;
      flags|= STATE_FLAG_EVENT;

      wait->m_event_type= EVENT_TYPE_WAIT;
      /*
        IDLE events are waits, but by definition we know that
        such waits happen outside of any STAGE and STATEMENT,
        so they have no parents.
      */
      wait->m_nesting_event_id= 0;
      /* no need to set wait->m_nesting_event_type */

      wait->m_thread= pfs_thread;
      wait->m_class= &global_idle_class;
      wait->m_timer_start= timer_start;
      wait->m_timer_end= 0;
      wait->m_event_id= pfs_thread->m_event_id++;
      wait->m_end_event_id= 0;
      wait->m_operation= OPERATION_TYPE_IDLE;
      wait->m_source_file= src_file;
      wait->m_source_line= src_line;
      wait->m_wait_class= WAIT_CLASS_IDLE;

      pfs_thread->m_events_waits_current++;
    }
  }
  else
  {
    if (global_idle_class.m_timed)
    {
      timer_start= get_timer_raw_value_and_function(idle_timer, &state->m_timer);
      state->m_timer_start= timer_start;
      flags= STATE_FLAG_TIMED;
    }
  }

  state->m_flags= flags;
  return reinterpret_cast<PSI_idle_locker*> (state);
}

/**
  Implementation of the mutex instrumentation interface.
  @sa PSI_v1::end_idle_wait.
*/
static void end_idle_wait_v1(PSI_idle_locker* locker)
{
  PSI_idle_locker_state *state= reinterpret_cast<PSI_idle_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);
  ulonglong timer_end= 0;
  ulonglong wait_time= 0;

  register uint flags= state->m_flags;

  if (flags & STATE_FLAG_TIMED)
  {
    timer_end= state->m_timer();
    wait_time= timer_end - state->m_timer_start;
  }

  if (flags & STATE_FLAG_THREAD)
  {
    PFS_thread *thread= reinterpret_cast<PFS_thread *> (state->m_thread);
    PFS_single_stat *event_name_array;
    event_name_array= thread->m_instr_class_waits_stats;

    if (flags & STATE_FLAG_TIMED)
    {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      event_name_array[GLOBAL_IDLE_EVENT_INDEX].aggregate_value(wait_time);
    }
    else
    {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[GLOBAL_IDLE_EVENT_INDEX].aggregate_counted();
    }

    if (flags & STATE_FLAG_EVENT)
    {
      PFS_events_waits *wait= reinterpret_cast<PFS_events_waits*> (state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end= timer_end;
      wait->m_end_event_id= thread->m_event_id;
      if (flag_events_waits_history)
        insert_events_waits_history(thread, wait);
      if (flag_events_waits_history_long)
        insert_events_waits_history_long(wait);
      thread->m_events_waits_current--;
    }
  }

  if (flags & STATE_FLAG_TIMED)
  {
    /* Aggregate to EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME (timed) */
    global_idle_stat.aggregate_value(wait_time);
  }
  else
  {
    /* Aggregate to EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME (counted) */
    global_idle_stat.aggregate_counted();
  }
}

/**
  Implementation of the mutex instrumentation interface.
  @sa PSI_v1::end_mutex_wait.
*/
static void end_mutex_wait_v1(PSI_mutex_locker* locker, int rc)
{
  PSI_mutex_locker_state *state= reinterpret_cast<PSI_mutex_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end= 0;
  ulonglong wait_time= 0;

  PFS_mutex *mutex= reinterpret_cast<PFS_mutex *> (state->m_mutex);
  DBUG_ASSERT(mutex != NULL);
  PFS_thread *thread= reinterpret_cast<PFS_thread *> (state->m_thread);

  register uint flags= state->m_flags;

  if (flags & STATE_FLAG_TIMED)
  {
    timer_end= state->m_timer();
    wait_time= timer_end - state->m_timer_start;
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (timed) */
    mutex->m_mutex_stat.m_wait_stat.aggregate_value(wait_time);
  }
  else
  {
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
    mutex->m_mutex_stat.m_wait_stat.aggregate_counted();
  }

  if (likely(rc == 0))
  {
    mutex->m_owner= thread;
    mutex->m_last_locked= timer_end;
  }

  if (flags & STATE_FLAG_THREAD)
  {
    PFS_single_stat *event_name_array;
    event_name_array= thread->m_instr_class_waits_stats;
    uint index= mutex->m_class->m_event_name_index;

    if (flags & STATE_FLAG_TIMED)
    {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      event_name_array[index].aggregate_value(wait_time);
    }
    else
    {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[index].aggregate_counted();
    }

    if (flags & STATE_FLAG_EVENT)
    {
      PFS_events_waits *wait= reinterpret_cast<PFS_events_waits*> (state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end= timer_end;
      wait->m_end_event_id= thread->m_event_id;
      if (flag_events_waits_history)
        insert_events_waits_history(thread, wait);
      if (flag_events_waits_history_long)
        insert_events_waits_history_long(wait);
      thread->m_events_waits_current--;
    }
  }
}

/**
  Implementation of the rwlock instrumentation interface.
  @sa PSI_v1::end_rwlock_rdwait.
*/
static void end_rwlock_rdwait_v1(PSI_rwlock_locker* locker, int rc)
{
  PSI_rwlock_locker_state *state= reinterpret_cast<PSI_rwlock_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end= 0;
  ulonglong wait_time= 0;

  PFS_rwlock *rwlock= reinterpret_cast<PFS_rwlock *> (state->m_rwlock);
  DBUG_ASSERT(rwlock != NULL);

  if (state->m_flags & STATE_FLAG_TIMED)
  {
    timer_end= state->m_timer();
    wait_time= timer_end - state->m_timer_start;
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (timed) */
    rwlock->m_rwlock_stat.m_wait_stat.aggregate_value(wait_time);
  }
  else
  {
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
    rwlock->m_rwlock_stat.m_wait_stat.aggregate_counted();
  }

  if (rc == 0)
  {
    /*
      Warning:
      Multiple threads can execute this section concurrently
      (since multiple readers can execute in parallel).
      The statistics generated are not safe, which is why they are
      just statistics, not facts.
    */
    if (rwlock->m_readers == 0)
      rwlock->m_last_read= timer_end;
    rwlock->m_writer= NULL;
    rwlock->m_readers++;
  }

  if (state->m_flags & STATE_FLAG_THREAD)
  {
    PFS_thread *thread= reinterpret_cast<PFS_thread *> (state->m_thread);
    DBUG_ASSERT(thread != NULL);

    PFS_single_stat *event_name_array;
    event_name_array= thread->m_instr_class_waits_stats;
    uint index= rwlock->m_class->m_event_name_index;

    if (state->m_flags & STATE_FLAG_TIMED)
    {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      event_name_array[index].aggregate_value(wait_time);
    }
    else
    {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[index].aggregate_counted();
    }

    if (state->m_flags & STATE_FLAG_EVENT)
    {
      PFS_events_waits *wait= reinterpret_cast<PFS_events_waits*> (state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end= timer_end;
      wait->m_end_event_id= thread->m_event_id;
      if (flag_events_waits_history)
        insert_events_waits_history(thread, wait);
      if (flag_events_waits_history_long)
        insert_events_waits_history_long(wait);
      thread->m_events_waits_current--;
    }
  }
}

/**
  Implementation of the rwlock instrumentation interface.
  @sa PSI_v1::end_rwlock_wrwait.
*/
static void end_rwlock_wrwait_v1(PSI_rwlock_locker* locker, int rc)
{
  PSI_rwlock_locker_state *state= reinterpret_cast<PSI_rwlock_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end= 0;
  ulonglong wait_time= 0;

  PFS_rwlock *rwlock= reinterpret_cast<PFS_rwlock *> (state->m_rwlock);
  DBUG_ASSERT(rwlock != NULL);
  PFS_thread *thread= reinterpret_cast<PFS_thread *> (state->m_thread);

  if (state->m_flags & STATE_FLAG_TIMED)
  {
    timer_end= state->m_timer();
    wait_time= timer_end - state->m_timer_start;
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (timed) */
    rwlock->m_rwlock_stat.m_wait_stat.aggregate_value(wait_time);
  }
  else
  {
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
    rwlock->m_rwlock_stat.m_wait_stat.aggregate_counted();
  }

  if (likely(rc == 0))
  {
    /* Thread safe : we are protected by the instrumented rwlock */
    rwlock->m_writer= thread;
    rwlock->m_last_written= timer_end;
    /* Reset the readers stats, they could be off */
    rwlock->m_readers= 0;
    rwlock->m_last_read= 0;
  }

  if (state->m_flags & STATE_FLAG_THREAD)
  {
    PFS_single_stat *event_name_array;
    event_name_array= thread->m_instr_class_waits_stats;
    uint index= rwlock->m_class->m_event_name_index;

    if (state->m_flags & STATE_FLAG_TIMED)
    {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      event_name_array[index].aggregate_value(wait_time);
    }
    else
    {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[index].aggregate_counted();
    }

    if (state->m_flags & STATE_FLAG_EVENT)
    {
      PFS_events_waits *wait= reinterpret_cast<PFS_events_waits*> (state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end= timer_end;
      wait->m_end_event_id= thread->m_event_id;
      if (flag_events_waits_history)
        insert_events_waits_history(thread, wait);
      if (flag_events_waits_history_long)
        insert_events_waits_history_long(wait);
      thread->m_events_waits_current--;
    }
  }
}

/**
  Implementation of the cond instrumentation interface.
  @sa PSI_v1::end_cond_wait.
*/
static void end_cond_wait_v1(PSI_cond_locker* locker, int rc)
{
  PSI_cond_locker_state *state= reinterpret_cast<PSI_cond_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end= 0;
  ulonglong wait_time= 0;

  PFS_cond *cond= reinterpret_cast<PFS_cond *> (state->m_cond);
  /* PFS_mutex *mutex= reinterpret_cast<PFS_mutex *> (state->m_mutex); */

  if (state->m_flags & STATE_FLAG_TIMED)
  {
    timer_end= state->m_timer();
    wait_time= timer_end - state->m_timer_start;
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (timed) */
    cond->m_cond_stat.m_wait_stat.aggregate_value(wait_time);
  }
  else
  {
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
    cond->m_cond_stat.m_wait_stat.aggregate_counted();
  }

  if (state->m_flags & STATE_FLAG_THREAD)
  {
    PFS_thread *thread= reinterpret_cast<PFS_thread *> (state->m_thread);
    DBUG_ASSERT(thread != NULL);

    PFS_single_stat *event_name_array;
    event_name_array= thread->m_instr_class_waits_stats;
    uint index= cond->m_class->m_event_name_index;

    if (state->m_flags & STATE_FLAG_TIMED)
    {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      event_name_array[index].aggregate_value(wait_time);
    }
    else
    {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[index].aggregate_counted();
    }

    if (state->m_flags & STATE_FLAG_EVENT)
    {
      PFS_events_waits *wait= reinterpret_cast<PFS_events_waits*> (state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end= timer_end;
      wait->m_end_event_id= thread->m_event_id;
      if (flag_events_waits_history)
        insert_events_waits_history(thread, wait);
      if (flag_events_waits_history_long)
        insert_events_waits_history_long(wait);
      thread->m_events_waits_current--;
    }
  }
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::end_table_io_wait.
*/
static void end_table_io_wait_v1(PSI_table_locker* locker)
{
  PSI_table_locker_state *state= reinterpret_cast<PSI_table_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end= 0;
  ulonglong wait_time= 0;

  PFS_table *table= reinterpret_cast<PFS_table *> (state->m_table);
  DBUG_ASSERT(table != NULL);

  PFS_single_stat *stat;
  PFS_table_io_stat *table_io_stat;

  DBUG_ASSERT((state->m_index < table->m_share->m_key_count) ||
              (state->m_index == MAX_INDEXES));

  table_io_stat= & table->m_table_stat.m_index_stat[state->m_index];
  table_io_stat->m_has_data= true;

  switch (state->m_io_operation)
  {
  case PSI_TABLE_FETCH_ROW:
    stat= & table_io_stat->m_fetch;
    break;
  case PSI_TABLE_WRITE_ROW:
    stat= & table_io_stat->m_insert;
    break;
  case PSI_TABLE_UPDATE_ROW:
    stat= & table_io_stat->m_update;
    break;
  case PSI_TABLE_DELETE_ROW:
    stat= & table_io_stat->m_delete;
    break;
  default:
    DBUG_ASSERT(false);
    stat= NULL;
    break;
  }

  register uint flags= state->m_flags;

  if (flags & STATE_FLAG_TIMED)
  {
    timer_end= state->m_timer();
    wait_time= timer_end - state->m_timer_start;
    stat->aggregate_value(wait_time);
  }
  else
  {
    stat->aggregate_counted();
  }

  if (flags & STATE_FLAG_THREAD)
  {
    PFS_thread *thread= reinterpret_cast<PFS_thread *> (state->m_thread);
    DBUG_ASSERT(thread != NULL);

    PFS_single_stat *event_name_array;
    event_name_array= thread->m_instr_class_waits_stats;

    /*
      Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      (for wait/io/table/sql/handler)
    */
    if (flags & STATE_FLAG_TIMED)
    {
      event_name_array[GLOBAL_TABLE_IO_EVENT_INDEX].aggregate_value(wait_time);
    }
    else
    {
      event_name_array[GLOBAL_TABLE_IO_EVENT_INDEX].aggregate_counted();
    }

    if (flags & STATE_FLAG_EVENT)
    {
      PFS_events_waits *wait= reinterpret_cast<PFS_events_waits*> (state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end= timer_end;
      wait->m_end_event_id= thread->m_event_id;
      if (flag_events_waits_history)
        insert_events_waits_history(thread, wait);
      if (flag_events_waits_history_long)
        insert_events_waits_history_long(wait);
      thread->m_events_waits_current--;
    }
  }

  table->m_has_io_stats= true;
}

/**
  Implementation of the table instrumentation interface.
  @sa PSI_v1::end_table_lock_wait.
*/
static void end_table_lock_wait_v1(PSI_table_locker* locker)
{
  PSI_table_locker_state *state= reinterpret_cast<PSI_table_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  ulonglong timer_end= 0;
  ulonglong wait_time= 0;

  PFS_table *table= reinterpret_cast<PFS_table *> (state->m_table);
  DBUG_ASSERT(table != NULL);

  PFS_single_stat *stat= & table->m_table_stat.m_lock_stat.m_stat[state->m_index];

  register uint flags= state->m_flags;

  if (flags & STATE_FLAG_TIMED)
  {
    timer_end= state->m_timer();
    wait_time= timer_end - state->m_timer_start;
    stat->aggregate_value(wait_time);
  }
  else
  {
    stat->aggregate_counted();
  }

  if (flags & STATE_FLAG_THREAD)
  {
    PFS_thread *thread= reinterpret_cast<PFS_thread *> (state->m_thread);
    DBUG_ASSERT(thread != NULL);

    PFS_single_stat *event_name_array;
    event_name_array= thread->m_instr_class_waits_stats;

    /*
      Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      (for wait/lock/table/sql/handler)
    */
    if (flags & STATE_FLAG_TIMED)
    {
      event_name_array[GLOBAL_TABLE_LOCK_EVENT_INDEX].aggregate_value(wait_time);
    }
    else
    {
      event_name_array[GLOBAL_TABLE_LOCK_EVENT_INDEX].aggregate_counted();
    }

    if (flags & STATE_FLAG_EVENT)
    {
      PFS_events_waits *wait= reinterpret_cast<PFS_events_waits*> (state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end= timer_end;
      wait->m_end_event_id= thread->m_event_id;
      if (flag_events_waits_history)
        insert_events_waits_history(thread, wait);
      if (flag_events_waits_history_long)
        insert_events_waits_history_long(wait);
      thread->m_events_waits_current--;
    }
  }

  table->m_has_lock_stats= true;
}

static void start_file_wait_v1(PSI_file_locker *locker,
                               size_t count,
                               const char *src_file,
                               uint src_line);

static void end_file_wait_v1(PSI_file_locker *locker,
                             size_t count);

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::start_file_open_wait.
*/
static void start_file_open_wait_v1(PSI_file_locker *locker,
                                    const char *src_file,
                                    uint src_line)
{
  start_file_wait_v1(locker, 0, src_file, src_line);

  return;
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::end_file_open_wait.
*/
static PSI_file* end_file_open_wait_v1(PSI_file_locker *locker,
                                       void *result)
{
  PSI_file_locker_state *state= reinterpret_cast<PSI_file_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  switch (state->m_operation)
  {
  case PSI_FILE_STAT:
    break;
  case PSI_FILE_STREAM_OPEN:
  case PSI_FILE_CREATE:
    if (result != NULL)
    {
      PFS_file_class *klass= reinterpret_cast<PFS_file_class*> (state->m_class);
      PFS_thread *thread= reinterpret_cast<PFS_thread*> (state->m_thread);
      const char *name= state->m_name;
      uint len= strlen(name);
      PFS_file *pfs_file= find_or_create_file(thread, klass, name, len, true);
      state->m_file= reinterpret_cast<PSI_file*> (pfs_file);
    }
    break;
  case PSI_FILE_OPEN:
  default:
    DBUG_ASSERT(false);
    break;
  }

  end_file_wait_v1(locker, 0);

  return state->m_file;
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::end_file_open_wait_and_bind_to_descriptor.
*/
static void end_file_open_wait_and_bind_to_descriptor_v1
  (PSI_file_locker *locker, File file)
{
  PFS_file *pfs_file= NULL;
  int index= (int) file;
  PSI_file_locker_state *state= reinterpret_cast<PSI_file_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  if (index >= 0)
  {
    PFS_file_class *klass= reinterpret_cast<PFS_file_class*> (state->m_class);
    PFS_thread *thread= reinterpret_cast<PFS_thread*> (state->m_thread);
    const char *name= state->m_name;
    uint len= strlen(name);
    pfs_file= find_or_create_file(thread, klass, name, len, true);
    state->m_file= reinterpret_cast<PSI_file*> (pfs_file);
  }

  end_file_wait_v1(locker, 0);

  if (likely(index >= 0))
  {
    if (likely(index < file_handle_max))
      file_handle_array[index]= pfs_file;
    else
    {
      if (pfs_file != NULL)
        release_file(pfs_file);
      file_handle_lost++;
    }
  }
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::start_file_wait.
*/
static void start_file_wait_v1(PSI_file_locker *locker,
                               size_t count,
                               const char *src_file,
                               uint src_line)
{
  ulonglong timer_start= 0;
  PSI_file_locker_state *state= reinterpret_cast<PSI_file_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  register uint flags= state->m_flags;

  if (flags & STATE_FLAG_TIMED)
  {
    timer_start= get_timer_raw_value_and_function(wait_timer, & state->m_timer);
    state->m_timer_start= timer_start;
  }

  if (flags & STATE_FLAG_EVENT)
  {
    PFS_events_waits *wait= reinterpret_cast<PFS_events_waits*> (state->m_wait);
    DBUG_ASSERT(wait != NULL);

    wait->m_timer_start= timer_start;
    wait->m_source_file= src_file;
    wait->m_source_line= src_line;
    wait->m_number_of_bytes= count;
  }
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::end_file_wait.
*/
static void end_file_wait_v1(PSI_file_locker *locker,
                             size_t byte_count)
{
  PSI_file_locker_state *state= reinterpret_cast<PSI_file_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);
  PFS_file *file= reinterpret_cast<PFS_file *> (state->m_file);
  PFS_file_class *klass= reinterpret_cast<PFS_file_class *> (state->m_class);
  PFS_thread *thread= reinterpret_cast<PFS_thread *> (state->m_thread);

  ulonglong timer_end= 0;
  ulonglong wait_time= 0;
  PFS_byte_stat *byte_stat;
  register uint flags= state->m_flags;
  size_t bytes= ((int)byte_count > -1 ? byte_count : 0);

  PFS_file_stat *file_stat;

  if (file != NULL)
  {
    file_stat= & file->m_file_stat;
  }
  else
  {
    file_stat= & klass->m_file_stat;
  }

  switch (state->m_operation)
  {
    /* Group read operations */
    case PSI_FILE_READ:
      byte_stat= &file_stat->m_io_stat.m_read;
      break;
    /* Group write operations */
    case PSI_FILE_WRITE:
      byte_stat= &file_stat->m_io_stat.m_write;
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
      byte_stat= &file_stat->m_io_stat.m_misc;
      break;
    default:
      DBUG_ASSERT(false);
      byte_stat= NULL;
      break;
  }

  /* Aggregation for EVENTS_WAITS_SUMMARY_BY_INSTANCE */
  if (flags & STATE_FLAG_TIMED)
  {
    timer_end= state->m_timer();
    wait_time= timer_end - state->m_timer_start;
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (timed) */
    byte_stat->aggregate(wait_time, bytes);
  }
  else
  {
    /* Aggregate to EVENTS_WAITS_SUMMARY_BY_INSTANCE (counted) */
    byte_stat->aggregate_counted(bytes);
  }

  if (flags & STATE_FLAG_THREAD)
  {
    DBUG_ASSERT(thread != NULL);

    PFS_single_stat *event_name_array;
    event_name_array= thread->m_instr_class_waits_stats;
    uint index= klass->m_event_name_index;

    if (flags & STATE_FLAG_TIMED)
    {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      event_name_array[index].aggregate_value(wait_time);
    }
    else
    {
      /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[index].aggregate_counted();
    }

    if (state->m_flags & STATE_FLAG_EVENT)
    {
      PFS_events_waits *wait= reinterpret_cast<PFS_events_waits*> (state->m_wait);
      DBUG_ASSERT(wait != NULL);

      wait->m_timer_end= timer_end;
      wait->m_number_of_bytes= bytes;
      wait->m_end_event_id= thread->m_event_id;
      wait->m_object_instance_addr= file;
      wait->m_weak_file= file;
      wait->m_weak_version= (file ? file->get_version() : 0);

      if (flag_events_waits_history)
        insert_events_waits_history(thread, wait);
      if (flag_events_waits_history_long)
        insert_events_waits_history_long(wait);
      thread->m_events_waits_current--;
    }
  }
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::start_file_close_wait.
*/
static void start_file_close_wait_v1(PSI_file_locker *locker,
                                     const char *src_file,
                                     uint src_line)
{
  PFS_thread *thread;
  const char *name;
  uint len;
  PFS_file *pfs_file;
  PSI_file_locker_state *state= reinterpret_cast<PSI_file_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  switch (state->m_operation)
  {
  case PSI_FILE_DELETE:
    thread= reinterpret_cast<PFS_thread*> (state->m_thread);
    name= state->m_name;
    len= strlen(name);
    pfs_file= find_or_create_file(thread, NULL, name, len, false);
    state->m_file= reinterpret_cast<PSI_file*> (pfs_file);
    break;
  case PSI_FILE_STREAM_CLOSE:
  case PSI_FILE_CLOSE:
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  start_file_wait_v1(locker, 0, src_file, src_line);

  return;
}

/**
  Implementation of the file instrumentation interface.
  @sa PSI_v1::end_file_close_wait.
*/
static void end_file_close_wait_v1(PSI_file_locker *locker, int rc)
{
  PSI_file_locker_state *state= reinterpret_cast<PSI_file_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  end_file_wait_v1(locker, 0);

  if (rc == 0)
  {
    PFS_thread *thread= reinterpret_cast<PFS_thread*> (state->m_thread);
    PFS_file *file= reinterpret_cast<PFS_file*> (state->m_file);

    /* Release or destroy the file if necessary */
    switch(state->m_operation)
    {
    case PSI_FILE_CLOSE:
    case PSI_FILE_STREAM_CLOSE:
      if (file != NULL)
        release_file(file);
      break;
    case PSI_FILE_DELETE:
      if (file != NULL)
        destroy_file(thread, file);
      break;
    default:
      DBUG_ASSERT(false);
      break;
    }
  }
  return;
}

static void start_stage_v1(PSI_stage_key key, const char *src_file, int src_line)
{
  ulonglong timer_value= 0;

  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(pfs_thread == NULL))
    return;

  /* Always update column threads.processlist_state. */
  pfs_thread->m_stage= key;

  if (! flag_global_instrumentation)
    return;

  if (flag_thread_instrumentation && ! pfs_thread->m_enabled)
    return;

  PFS_events_stages *pfs= & pfs_thread->m_stage_current;
  PFS_events_waits *child_wait= & pfs_thread->m_events_waits_stack[0];
  PFS_events_statements *parent_statement= & pfs_thread->m_statement_stack[0];

  PFS_instr_class *old_class= pfs->m_class;
  if (old_class != NULL)
  {
    PFS_stage_stat *event_name_array;
    event_name_array= pfs_thread->m_instr_class_stages_stats;
    uint index= old_class->m_event_name_index;

    /* Finish old event */
    if (old_class->m_timed)
    {
      timer_value= get_timer_raw_value(stage_timer);;
      pfs->m_timer_end= timer_value;

      /* Aggregate to EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      ulonglong stage_time= timer_value - pfs->m_timer_start;
      event_name_array[index].aggregate_value(stage_time);
    }
    else
    {
      /* Aggregate to EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[index].aggregate_counted();
    }

    if (flag_events_stages_current)
    {
      pfs->m_end_event_id= pfs_thread->m_event_id;
      if (flag_events_stages_history)
        insert_events_stages_history(pfs_thread, pfs);
      if (flag_events_stages_history_long)
        insert_events_stages_history_long(pfs);
    }

    /* This stage event is now complete. */
    pfs->m_class= NULL;

    /* New waits will now be attached directly to the parent statement. */
    child_wait->m_event_id= parent_statement->m_event_id;
    child_wait->m_event_type= parent_statement->m_event_type;
    /* See below for new stages, that may overwrite this. */
  }

  /* Start new event */

  PFS_stage_class *new_klass= find_stage_class(key);
  if (unlikely(new_klass == NULL))
    return;

  if (! new_klass->m_enabled)
    return;

  pfs->m_class= new_klass;
  if (new_klass->m_timed)
  {
    /*
      Do not call the timer again if we have a
      TIMER_END for the previous stage already.
    */
    if (timer_value == 0)
      timer_value= get_timer_raw_value(stage_timer);
    pfs->m_timer_start= timer_value;
  }
  else
    pfs->m_timer_start= 0;
  pfs->m_timer_end= 0;

  if (flag_events_stages_current)
  {
    /* m_thread_internal_id is immutable and already set */
    DBUG_ASSERT(pfs->m_thread_internal_id == pfs_thread->m_thread_internal_id);
    pfs->m_event_id= pfs_thread->m_event_id++;
    pfs->m_end_event_id= 0;
    pfs->m_source_file= src_file;
    pfs->m_source_line= src_line;

    /* New wait events will have this new stage as parent. */
    child_wait->m_event_id= pfs->m_event_id;
    child_wait->m_event_type= EVENT_TYPE_STAGE;
  }
}

static void end_stage_v1()
{
  ulonglong timer_value= 0;

  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(pfs_thread == NULL))
    return;

  pfs_thread->m_stage= 0;

  if (! flag_global_instrumentation)
    return;

  if (flag_thread_instrumentation && ! pfs_thread->m_enabled)
    return;

  PFS_events_stages *pfs= & pfs_thread->m_stage_current;

  PFS_instr_class *old_class= pfs->m_class;
  if (old_class != NULL)
  {
    PFS_stage_stat *event_name_array;
    event_name_array= pfs_thread->m_instr_class_stages_stats;
    uint index= old_class->m_event_name_index;

    /* Finish old event */
    if (old_class->m_timed)
    {
      timer_value= get_timer_raw_value(stage_timer);;
      pfs->m_timer_end= timer_value;

      /* Aggregate to EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME (timed) */
      ulonglong stage_time= timer_value - pfs->m_timer_start;
      event_name_array[index].aggregate_value(stage_time);
    }
    else
    {
      /* Aggregate to EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME (counted) */
      event_name_array[index].aggregate_counted();
    }

    if (flag_events_stages_current)
    {
      pfs->m_end_event_id= pfs_thread->m_event_id;
      if (flag_events_stages_history)
        insert_events_stages_history(pfs_thread, pfs);
      if (flag_events_stages_history_long)
        insert_events_stages_history_long(pfs);
    }

    /* New waits will now be attached directly to the parent statement. */
    PFS_events_waits *child_wait= & pfs_thread->m_events_waits_stack[0];
    PFS_events_statements *parent_statement= & pfs_thread->m_statement_stack[0];
    child_wait->m_event_id= parent_statement->m_event_id;
    child_wait->m_event_type= parent_statement->m_event_type;

    /* This stage is completed */
    pfs->m_class= NULL;
  }
}

static PSI_statement_locker*
get_thread_statement_locker_v1(PSI_statement_locker_state *state,
                               PSI_statement_key key,
                               const void *charset)
{
  DBUG_ASSERT(state != NULL);
  if (! flag_global_instrumentation)
    return NULL;
  PFS_statement_class *klass= find_statement_class(key);
  if (unlikely(klass == NULL))
    return NULL;
  if (! klass->m_enabled)
    return NULL;

  register uint flags;

  if (flag_thread_instrumentation)
  {
    PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
    if (unlikely(pfs_thread == NULL))
      return NULL;
    if (! pfs_thread->m_enabled)
      return NULL;
    state->m_thread= reinterpret_cast<PSI_thread *> (pfs_thread);
    flags= STATE_FLAG_THREAD;

    if (klass->m_timed)
      flags|= STATE_FLAG_TIMED;

    if (flag_events_statements_current)
    {
      ulonglong event_id= pfs_thread->m_event_id++;

      if (pfs_thread->m_events_statements_count >= statement_stack_max)
      {
        return NULL;
      }

      PFS_events_statements *pfs= & pfs_thread->m_statement_stack[pfs_thread->m_events_statements_count];
      /* m_thread_internal_id is immutable and already set */
      DBUG_ASSERT(pfs->m_thread_internal_id == pfs_thread->m_thread_internal_id);
      pfs->m_event_id= event_id;
      pfs->m_end_event_id= 0;
      pfs->m_class= klass;
      pfs->m_timer_start= 0;
      pfs->m_timer_end= 0;
      pfs->m_lock_time= 0;
      pfs->m_current_schema_name_length= 0;
      pfs->m_sqltext_length= 0;

      pfs->m_message_text[0]= '\0';
      pfs->m_sql_errno= 0;
      pfs->m_sqlstate[0]= '\0';
      pfs->m_error_count= 0;
      pfs->m_warning_count= 0;
      pfs->m_rows_affected= 0;

      pfs->m_rows_sent= 0;
      pfs->m_rows_examined= 0;
      pfs->m_created_tmp_disk_tables= 0;
      pfs->m_created_tmp_tables= 0;
      pfs->m_select_full_join= 0;
      pfs->m_select_full_range_join= 0;
      pfs->m_select_range= 0;
      pfs->m_select_range_check= 0;
      pfs->m_select_scan= 0;
      pfs->m_sort_merge_passes= 0;
      pfs->m_sort_range= 0;
      pfs->m_sort_rows= 0;
      pfs->m_sort_scan= 0;
      pfs->m_no_index_used= 0;
      pfs->m_no_good_index_used= 0;
      digest_reset(& pfs->m_digest_storage);

      /* New stages will have this statement as parent */
      PFS_events_stages *child_stage= & pfs_thread->m_stage_current;
      child_stage->m_nesting_event_id= event_id;
      child_stage->m_nesting_event_type= EVENT_TYPE_STATEMENT;

      /* New waits will have this statement as parent, if no stage is instrumented */
      PFS_events_waits *child_wait= & pfs_thread->m_events_waits_stack[0];
      child_wait->m_nesting_event_id= event_id;
      child_wait->m_nesting_event_type= EVENT_TYPE_STATEMENT;

      state->m_statement= pfs;
      flags|= STATE_FLAG_EVENT;

      pfs_thread->m_events_statements_count++;
    }
  }
  else
  {
    if (klass->m_timed)
      flags= STATE_FLAG_TIMED;
    else
      flags= 0;
  }

  if (flag_statements_digest)
  {
    const CHARSET_INFO *cs= static_cast <const CHARSET_INFO*> (charset);
    flags|= STATE_FLAG_DIGEST;
    state->m_digest_state.m_last_id_index= 0;
    digest_reset(& state->m_digest_state.m_digest_storage);
    state->m_digest_state.m_digest_storage.m_charset_number= cs->number;
  }

  state->m_discarded= false;
  state->m_class= klass;
  state->m_flags= flags;

  state->m_lock_time= 0;
  state->m_rows_sent= 0;
  state->m_rows_examined= 0;
  state->m_created_tmp_disk_tables= 0;
  state->m_created_tmp_tables= 0;
  state->m_select_full_join= 0;
  state->m_select_full_range_join= 0;
  state->m_select_range= 0;
  state->m_select_range_check= 0;
  state->m_select_scan= 0;
  state->m_sort_merge_passes= 0;
  state->m_sort_range= 0;
  state->m_sort_rows= 0;
  state->m_sort_scan= 0;
  state->m_no_index_used= 0;
  state->m_no_good_index_used= 0;

  state->m_schema_name_length= 0;

  return reinterpret_cast<PSI_statement_locker*> (state);
}

static PSI_statement_locker*
refine_statement_v1(PSI_statement_locker *locker,
                    PSI_statement_key key)
{
  PSI_statement_locker_state *state= reinterpret_cast<PSI_statement_locker_state*> (locker);
  if (state == NULL)
    return NULL;
  DBUG_ASSERT(state->m_class != NULL);
  PFS_statement_class *klass;
  /* Only refine statements for mutable instrumentation */
  klass= reinterpret_cast<PFS_statement_class*> (state->m_class);
  DBUG_ASSERT(klass->is_mutable());
  klass= find_statement_class(key);

  uint flags= state->m_flags;

  if (unlikely(klass == NULL) || !klass->m_enabled)
  {
    /* pop statement stack */
    if (flags & STATE_FLAG_THREAD)
    {
      PFS_thread *pfs_thread= reinterpret_cast<PFS_thread *> (state->m_thread);
      DBUG_ASSERT(pfs_thread != NULL);
      if (pfs_thread->m_events_statements_count > 0)
        pfs_thread->m_events_statements_count--;
    }

    state->m_discarded= true;
    return NULL;
  }

  if ((flags & STATE_FLAG_TIMED) && ! klass->m_timed)
    flags= flags & ~STATE_FLAG_TIMED;

  if (flags & STATE_FLAG_EVENT)
  {
    PFS_events_statements *pfs= reinterpret_cast<PFS_events_statements*> (state->m_statement);
    DBUG_ASSERT(pfs != NULL);

    /* mutate EVENTS_STATEMENTS_CURRENT.EVENT_NAME */
    pfs->m_class= klass;
  }

  state->m_class= klass;
  state->m_flags= flags;
  return reinterpret_cast<PSI_statement_locker*> (state);
}

static void start_statement_v1(PSI_statement_locker *locker,
                               const char *db, uint db_len,
                               const char *src_file, uint src_line)
{
  PSI_statement_locker_state *state= reinterpret_cast<PSI_statement_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  register uint flags= state->m_flags;
  ulonglong timer_start= 0;

  if (flags & STATE_FLAG_TIMED)
  {
    timer_start= get_timer_raw_value_and_function(statement_timer, & state->m_timer);
    state->m_timer_start= timer_start;
  }

  compile_time_assert(PSI_SCHEMA_NAME_LEN == NAME_LEN);
  DBUG_ASSERT(db_len <= sizeof(state->m_schema_name));

  if (db_len > 0)
    memcpy(state->m_schema_name, db, db_len);
  state->m_schema_name_length= db_len;

  if (flags & STATE_FLAG_EVENT)
  {
    PFS_events_statements *pfs= reinterpret_cast<PFS_events_statements*> (state->m_statement);
    DBUG_ASSERT(pfs != NULL);

    pfs->m_timer_start= timer_start;
    pfs->m_source_file= src_file;
    pfs->m_source_line= src_line;

    DBUG_ASSERT(db_len <= sizeof(pfs->m_current_schema_name));
    if (db_len > 0)
      memcpy(pfs->m_current_schema_name, db, db_len);
    pfs->m_current_schema_name_length= db_len;
  }
}

static void set_statement_text_v1(PSI_statement_locker *locker,
                                  const char *text, uint text_len)
{
  PSI_statement_locker_state *state= reinterpret_cast<PSI_statement_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  if (state->m_discarded)
    return;

  if (state->m_flags & STATE_FLAG_EVENT)
  {
    PFS_events_statements *pfs= reinterpret_cast<PFS_events_statements*> (state->m_statement);
    DBUG_ASSERT(pfs != NULL);
    if (text_len > sizeof (pfs->m_sqltext))
      text_len= sizeof(pfs->m_sqltext);
    if (text_len)
      memcpy(pfs->m_sqltext, text, text_len);
    pfs->m_sqltext_length= text_len;
  }

  return;
}

#define SET_STATEMENT_ATTR_BODY(LOCKER, ATTR, VALUE)                    \
  PSI_statement_locker_state *state;                                    \
  state= reinterpret_cast<PSI_statement_locker_state*> (LOCKER);        \
  if (unlikely(state == NULL))                                          \
    return;                                                             \
  if (state->m_discarded)                                               \
    return;                                                             \
  state->ATTR= VALUE;                                                   \
  if (state->m_flags & STATE_FLAG_EVENT)                                \
  {                                                                     \
    PFS_events_statements *pfs;                                         \
    pfs= reinterpret_cast<PFS_events_statements*> (state->m_statement); \
    DBUG_ASSERT(pfs != NULL);                                           \
    pfs->ATTR= VALUE;                                                   \
  }                                                                     \
  return;

#define INC_STATEMENT_ATTR_BODY(LOCKER, ATTR, VALUE)                    \
  PSI_statement_locker_state *state;                                    \
  state= reinterpret_cast<PSI_statement_locker_state*> (LOCKER);        \
  if (unlikely(state == NULL))                                          \
    return;                                                             \
  if (state->m_discarded)                                               \
    return;                                                             \
  state->ATTR+= VALUE;                                                  \
  if (state->m_flags & STATE_FLAG_EVENT)                                \
  {                                                                     \
    PFS_events_statements *pfs;                                         \
    pfs= reinterpret_cast<PFS_events_statements*> (state->m_statement); \
    DBUG_ASSERT(pfs != NULL);                                           \
    pfs->ATTR+= VALUE;                                                  \
  }                                                                     \
  return;

static void set_statement_lock_time_v1(PSI_statement_locker *locker,
                                       ulonglong count)
{
  SET_STATEMENT_ATTR_BODY(locker, m_lock_time, count);
}

static void set_statement_rows_sent_v1(PSI_statement_locker *locker,
                                       ulonglong count)
{
  SET_STATEMENT_ATTR_BODY(locker, m_rows_sent, count);
}

static void set_statement_rows_examined_v1(PSI_statement_locker *locker,
                                           ulonglong count)
{
  SET_STATEMENT_ATTR_BODY(locker, m_rows_examined, count);
}

static void inc_statement_created_tmp_disk_tables_v1(PSI_statement_locker *locker,
                                                    ulong count)
{
  INC_STATEMENT_ATTR_BODY(locker, m_created_tmp_disk_tables, count);
}

static void inc_statement_created_tmp_tables_v1(PSI_statement_locker *locker,
                                                ulong count)
{
  INC_STATEMENT_ATTR_BODY(locker, m_created_tmp_tables, count);
}

static void inc_statement_select_full_join_v1(PSI_statement_locker *locker,
                                              ulong count)
{
  INC_STATEMENT_ATTR_BODY(locker, m_select_full_join, count);
}

static void inc_statement_select_full_range_join_v1(PSI_statement_locker *locker,
                                                    ulong count)
{
  INC_STATEMENT_ATTR_BODY(locker, m_select_full_range_join, count);
}

static void inc_statement_select_range_v1(PSI_statement_locker *locker,
                                          ulong count)
{
  INC_STATEMENT_ATTR_BODY(locker, m_select_range, count);
}

static void inc_statement_select_range_check_v1(PSI_statement_locker *locker,
                                                ulong count)
{
  INC_STATEMENT_ATTR_BODY(locker, m_select_range_check, count);
}

static void inc_statement_select_scan_v1(PSI_statement_locker *locker,
                                         ulong count)
{
  INC_STATEMENT_ATTR_BODY(locker, m_select_scan, count);
}

static void inc_statement_sort_merge_passes_v1(PSI_statement_locker *locker,
                                               ulong count)
{
  INC_STATEMENT_ATTR_BODY(locker, m_sort_merge_passes, count);
}

static void inc_statement_sort_range_v1(PSI_statement_locker *locker,
                                        ulong count)
{
  INC_STATEMENT_ATTR_BODY(locker, m_sort_range, count);
}

static void inc_statement_sort_rows_v1(PSI_statement_locker *locker,
                                       ulong count)
{
  INC_STATEMENT_ATTR_BODY(locker, m_sort_rows, count);
}

static void inc_statement_sort_scan_v1(PSI_statement_locker *locker,
                                       ulong count)
{
  INC_STATEMENT_ATTR_BODY(locker, m_sort_scan, count);
}

static void set_statement_no_index_used_v1(PSI_statement_locker *locker)
{
  SET_STATEMENT_ATTR_BODY(locker, m_no_index_used, 1);
}

static void set_statement_no_good_index_used_v1(PSI_statement_locker *locker)
{
  SET_STATEMENT_ATTR_BODY(locker, m_no_good_index_used, 1);
}

static void end_statement_v1(PSI_statement_locker *locker, void *stmt_da)
{
  PSI_statement_locker_state *state= reinterpret_cast<PSI_statement_locker_state*> (locker);
  Diagnostics_area *da= reinterpret_cast<Diagnostics_area*> (stmt_da);
  DBUG_ASSERT(state != NULL);
  DBUG_ASSERT(da != NULL);

  if (state->m_discarded)
    return;

  PFS_statement_class *klass= reinterpret_cast<PFS_statement_class *> (state->m_class);
  DBUG_ASSERT(klass != NULL);

  ulonglong timer_end= 0;
  ulonglong wait_time= 0;
  register uint flags= state->m_flags;

  if (flags & STATE_FLAG_TIMED)
  {
    timer_end= state->m_timer();
    wait_time= timer_end - state->m_timer_start;
  }

  PFS_statement_stat *event_name_array;
  uint index= klass->m_event_name_index;
  PFS_statement_stat *stat;
  
  /*
   Capture statement stats by digest.
  */
  PSI_digest_storage *digest_storage= NULL;
  PFS_statement_stat *digest_stat= NULL;

  if (flags & STATE_FLAG_THREAD)
  {
    PFS_thread *thread= reinterpret_cast<PFS_thread *> (state->m_thread);
    DBUG_ASSERT(thread != NULL);
    event_name_array= thread->m_instr_class_statements_stats;
    /* Aggregate to EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME */
    stat= & event_name_array[index];

    if (flags & STATE_FLAG_DIGEST)
    {
      digest_storage= &state->m_digest_state.m_digest_storage;
      /* Populate PFS_statements_digest_stat with computed digest information.*/
      digest_stat= find_or_create_digest(thread, digest_storage,
                                         state->m_schema_name,
                                         state->m_schema_name_length);
    }

    if (flags & STATE_FLAG_EVENT)
    {
      PFS_events_statements *pfs= reinterpret_cast<PFS_events_statements*> (state->m_statement);
      DBUG_ASSERT(pfs != NULL);

      switch(da->status())
      {
        case Diagnostics_area::DA_EMPTY:
          break;
        case Diagnostics_area::DA_OK:
          memcpy(pfs->m_message_text, da->message(), MYSQL_ERRMSG_SIZE);
          pfs->m_message_text[MYSQL_ERRMSG_SIZE]= 0;
          pfs->m_rows_affected= da->affected_rows();
          pfs->m_warning_count= da->statement_warn_count();
          memcpy(pfs->m_sqlstate, "00000", SQLSTATE_LENGTH);
          break;
        case Diagnostics_area::DA_EOF:
          pfs->m_warning_count= da->statement_warn_count();
          break;
        case Diagnostics_area::DA_ERROR:
          memcpy(pfs->m_message_text, da->message(), MYSQL_ERRMSG_SIZE);
          pfs->m_message_text[MYSQL_ERRMSG_SIZE]= 0;
          pfs->m_sql_errno= da->sql_errno();
          memcpy(pfs->m_sqlstate, da->get_sqlstate(), SQLSTATE_LENGTH);
          break;
        case Diagnostics_area::DA_DISABLED:
          break;
      }

      pfs->m_timer_end= timer_end;
      pfs->m_end_event_id= thread->m_event_id;

      if (flags & STATE_FLAG_DIGEST)
      {
        /*
          The following columns in events_statement_current:
          - DIGEST,
          - DIGEST_TEXT
          are computed from the digest storage.
        */
        digest_copy(& pfs->m_digest_storage, digest_storage);
      }

      if (flag_events_statements_history)
        insert_events_statements_history(thread, pfs);
      if (flag_events_statements_history_long)
        insert_events_statements_history_long(pfs);

      DBUG_ASSERT(thread->m_events_statements_count > 0);
      thread->m_events_statements_count--;
    }
  }
  else
  {
    if (flags & STATE_FLAG_DIGEST)
    {
      PFS_thread *thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);

      /* An instrumented thread is required, for LF_PINS. */
      if (thread != NULL)
      {
        /* Set digest stat. */
        digest_storage= &state->m_digest_state.m_digest_storage;
        /* Populate statements_digest_stat with computed digest information. */
        digest_stat= find_or_create_digest(thread, digest_storage,
                                           state->m_schema_name,
                                           state->m_schema_name_length);
      }
    }

    event_name_array= global_instr_class_statements_array;
    /* Aggregate to EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME */
    stat= & event_name_array[index];
  }

  if (flags & STATE_FLAG_TIMED)
  {
    /* Aggregate to EVENTS_STATEMENTS_SUMMARY_..._BY_EVENT_NAME (timed) */
    stat->aggregate_value(wait_time);
  }
  else
  {
    /* Aggregate to EVENTS_STATEMENTS_SUMMARY_..._BY_EVENT_NAME (counted) */
    stat->aggregate_counted();
  }

  stat->m_lock_time+= state->m_lock_time;
  stat->m_rows_sent+= state->m_rows_sent;
  stat->m_rows_examined+= state->m_rows_examined;
  stat->m_created_tmp_disk_tables+= state->m_created_tmp_disk_tables;
  stat->m_created_tmp_tables+= state->m_created_tmp_tables;
  stat->m_select_full_join+= state->m_select_full_join;
  stat->m_select_full_range_join+= state->m_select_full_range_join;
  stat->m_select_range+= state->m_select_range;
  stat->m_select_range_check+= state->m_select_range_check;
  stat->m_select_scan+= state->m_select_scan;
  stat->m_sort_merge_passes+= state->m_sort_merge_passes;
  stat->m_sort_range+= state->m_sort_range;
  stat->m_sort_rows+= state->m_sort_rows;
  stat->m_sort_scan+= state->m_sort_scan;
  stat->m_no_index_used+= state->m_no_index_used;
  stat->m_no_good_index_used+= state->m_no_good_index_used;

  if (digest_stat != NULL)
  {
    if (flags & STATE_FLAG_TIMED)
    {
      digest_stat->aggregate_value(wait_time);
    }
    else
    {
      digest_stat->aggregate_counted();
    }
  
    digest_stat->m_lock_time+= state->m_lock_time;
    digest_stat->m_rows_sent+= state->m_rows_sent;
    digest_stat->m_rows_examined+= state->m_rows_examined;
    digest_stat->m_created_tmp_disk_tables+= state->m_created_tmp_disk_tables;
    digest_stat->m_created_tmp_tables+= state->m_created_tmp_tables;
    digest_stat->m_select_full_join+= state->m_select_full_join;
    digest_stat->m_select_full_range_join+= state->m_select_full_range_join;
    digest_stat->m_select_range+= state->m_select_range;
    digest_stat->m_select_range_check+= state->m_select_range_check;
    digest_stat->m_select_scan+= state->m_select_scan;
    digest_stat->m_sort_merge_passes+= state->m_sort_merge_passes;
    digest_stat->m_sort_range+= state->m_sort_range;
    digest_stat->m_sort_rows+= state->m_sort_rows;
    digest_stat->m_sort_scan+= state->m_sort_scan;
    digest_stat->m_no_index_used+= state->m_no_index_used;
    digest_stat->m_no_good_index_used+= state->m_no_good_index_used;
  }

  switch (da->status())
  {
    case Diagnostics_area::DA_EMPTY:
      break;
    case Diagnostics_area::DA_OK:
      stat->m_rows_affected+= da->affected_rows();
      stat->m_warning_count+= da->statement_warn_count();
      if (digest_stat != NULL)
      {
        digest_stat->m_rows_affected+= da->affected_rows();
        digest_stat->m_warning_count+= da->statement_warn_count();
      }
      break;
    case Diagnostics_area::DA_EOF:
      stat->m_warning_count+= da->statement_warn_count();
      if (digest_stat != NULL)
      {
        digest_stat->m_warning_count+= da->statement_warn_count();
      }
      break;
    case Diagnostics_area::DA_ERROR:
      stat->m_error_count++;
      if (digest_stat != NULL)
      {
        digest_stat->m_error_count++;
      }
      break;
    case Diagnostics_area::DA_DISABLED:
      break;
  }
}

/**
  Implementation of the socket instrumentation interface.
  @sa PSI_v1::end_socket_wait.
*/
static void end_socket_wait_v1(PSI_socket_locker *locker, size_t byte_count)
{
  PSI_socket_locker_state *state= reinterpret_cast<PSI_socket_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  PFS_socket *socket= reinterpret_cast<PFS_socket *>(state->m_socket);
  DBUG_ASSERT(socket != NULL);

  ulonglong timer_end= 0;
  ulonglong wait_time= 0;
  PFS_byte_stat *byte_stat;
  register uint flags= state->m_flags;
  size_t bytes= ((int)byte_count > -1 ? byte_count : 0);

  switch (state->m_operation)
  {
    /* Group read operations */
    case PSI_SOCKET_RECV:
    case PSI_SOCKET_RECVFROM:
    case PSI_SOCKET_RECVMSG:
      byte_stat= &socket->m_socket_stat.m_io_stat.m_read;
      break;
    /* Group write operations */
    case PSI_SOCKET_SEND:
    case PSI_SOCKET_SENDTO:
    case PSI_SOCKET_SENDMSG:
      byte_stat= &socket->m_socket_stat.m_io_stat.m_write;
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
      byte_stat= &socket->m_socket_stat.m_io_stat.m_misc;
      break;
    default:
      DBUG_ASSERT(false);
      byte_stat= NULL;
      break;
  }

  /* Aggregation for EVENTS_WAITS_SUMMARY_BY_INSTANCE */
  if (flags & STATE_FLAG_TIMED)
  {
    timer_end= state->m_timer();
    wait_time= timer_end - state->m_timer_start;

    /* Aggregate to the socket instrument for now (timed) */
    byte_stat->aggregate(wait_time, bytes);
  }
  else
  {
    /* Aggregate to the socket instrument (event count and byte count) */
    byte_stat->aggregate_counted(bytes);
  }

  /* Aggregate to EVENTS_WAITS_HISTORY and EVENTS_WAITS_HISTORY_LONG */
  if (flags & STATE_FLAG_EVENT)
  {
    PFS_thread *thread= reinterpret_cast<PFS_thread *>(state->m_thread);
    DBUG_ASSERT(thread != NULL);
    PFS_events_waits *wait= reinterpret_cast<PFS_events_waits*> (state->m_wait);
    DBUG_ASSERT(wait != NULL);

    wait->m_timer_end= timer_end;
    wait->m_end_event_id= thread->m_event_id;
    wait->m_number_of_bytes= bytes;

    if (flag_events_waits_history)
      insert_events_waits_history(thread, wait);
    if (flag_events_waits_history_long)
      insert_events_waits_history_long(wait);
    thread->m_events_waits_current--;
  }
}

static void set_socket_state_v1(PSI_socket *socket, PSI_socket_state state)
{
  DBUG_ASSERT((state == PSI_SOCKET_STATE_IDLE) || (state == PSI_SOCKET_STATE_ACTIVE));
  PFS_socket *pfs= reinterpret_cast<PFS_socket*>(socket);
  DBUG_ASSERT(pfs != NULL);
  DBUG_ASSERT(pfs->m_idle || (state == PSI_SOCKET_STATE_IDLE));
  DBUG_ASSERT(!pfs->m_idle || (state == PSI_SOCKET_STATE_ACTIVE));
  pfs->m_idle= (state == PSI_SOCKET_STATE_IDLE);
}

/**
  Set socket descriptor and address info.
*/
static void set_socket_info_v1(PSI_socket *socket,
                               const my_socket *fd,
                               const struct sockaddr *addr,
                               socklen_t addr_len)
{
  PFS_socket *pfs= reinterpret_cast<PFS_socket*>(socket);
  DBUG_ASSERT(pfs != NULL);

  /** Set socket descriptor */
  if (fd != NULL)
    pfs->m_fd= *fd;

  /** Set raw socket address and length */
  if (likely(addr != NULL && addr_len > 0))
  {
    pfs->m_addr_len= addr_len;

    /** Restrict address length to size of struct */
    if (unlikely(pfs->m_addr_len > sizeof(sockaddr_storage)))
      pfs->m_addr_len= sizeof(struct sockaddr_storage);

    memcpy(&pfs->m_sock_addr, addr, pfs->m_addr_len);
  }
}

/**
  Implementation of the socket instrumentation interface.
  @sa PSI_v1::set_socket_info.
*/
static void set_socket_thread_owner_v1(PSI_socket *socket)
{
  PFS_socket *pfs_socket= reinterpret_cast<PFS_socket*>(socket);
  DBUG_ASSERT(pfs_socket != NULL);
  pfs_socket->m_thread_owner= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
}


/**
  Implementation of the thread attribute connection interface
  @sa PSI_v1::set_thread_connect_attr.
*/
static int set_thread_connect_attrs_v1(const char *buffer, uint length,
                                       const void *from_cs)
{

  PFS_thread *thd= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);

  DBUG_ASSERT(buffer != NULL);

  if (likely(thd != NULL) && session_connect_attrs_size_per_thread > 0)
  {
    const CHARSET_INFO *cs = static_cast<const CHARSET_INFO *> (from_cs);

    /* copy from the input buffer as much as we can fit */
    uint copy_size= (uint)(length < session_connect_attrs_size_per_thread ?
                           length : session_connect_attrs_size_per_thread);
    thd->m_session_lock.allocated_to_dirty();
    memcpy(thd->m_session_connect_attrs, buffer, copy_size);
    thd->m_session_connect_attrs_length= copy_size;
    thd->m_session_connect_attrs_cs_number= cs->number;
    thd->m_session_lock.dirty_to_allocated();

    if (copy_size == length)
      return 0;

    session_connect_attrs_lost++;
    return 1;
  }
  return 0;
}


/**
  Implementation of the instrumentation interface.
  @sa PSI_v1.
*/
PSI_v1 PFS_v1=
{
  register_mutex_v1,
  register_rwlock_v1,
  register_cond_v1,
  register_thread_v1,
  register_file_v1,
  register_stage_v1,
  register_statement_v1,
  register_socket_v1,
  init_mutex_v1,
  destroy_mutex_v1,
  init_rwlock_v1,
  destroy_rwlock_v1,
  init_cond_v1,
  destroy_cond_v1,
  init_socket_v1,
  destroy_socket_v1,
  get_table_share_v1,
  release_table_share_v1,
  drop_table_share_v1,
  open_table_v1,
  unbind_table_v1,
  rebind_table_v1,
  close_table_v1,
  create_file_v1,
  spawn_thread_v1,
  new_thread_v1,
  set_thread_id_v1,
  get_thread_v1,
  set_thread_user_v1,
  set_thread_account_v1,
  set_thread_db_v1,
  set_thread_command_v1,
  set_thread_start_time_v1,
  set_thread_state_v1,
  set_thread_info_v1,
  set_thread_v1,
  delete_current_thread_v1,
  delete_thread_v1,
  get_thread_file_name_locker_v1,
  get_thread_file_stream_locker_v1,
  get_thread_file_descriptor_locker_v1,
  unlock_mutex_v1,
  unlock_rwlock_v1,
  signal_cond_v1,
  broadcast_cond_v1,
  start_idle_wait_v1,
  end_idle_wait_v1,
  start_mutex_wait_v1,
  end_mutex_wait_v1,
  start_rwlock_wait_v1, /* read */
  end_rwlock_rdwait_v1,
  start_rwlock_wait_v1, /* write */
  end_rwlock_wrwait_v1,
  start_cond_wait_v1,
  end_cond_wait_v1,
  start_table_io_wait_v1,
  end_table_io_wait_v1,
  start_table_lock_wait_v1,
  end_table_lock_wait_v1,
  start_file_open_wait_v1,
  end_file_open_wait_v1,
  end_file_open_wait_and_bind_to_descriptor_v1,
  start_file_wait_v1,
  end_file_wait_v1,
  start_file_close_wait_v1,
  end_file_close_wait_v1,
  start_stage_v1,
  end_stage_v1,
  get_thread_statement_locker_v1,
  refine_statement_v1,
  start_statement_v1,
  set_statement_text_v1,
  set_statement_lock_time_v1,
  set_statement_rows_sent_v1,
  set_statement_rows_examined_v1,
  inc_statement_created_tmp_disk_tables_v1,
  inc_statement_created_tmp_tables_v1,
  inc_statement_select_full_join_v1,
  inc_statement_select_full_range_join_v1,
  inc_statement_select_range_v1,
  inc_statement_select_range_check_v1,
  inc_statement_select_scan_v1,
  inc_statement_sort_merge_passes_v1,
  inc_statement_sort_range_v1,
  inc_statement_sort_rows_v1,
  inc_statement_sort_scan_v1,
  set_statement_no_index_used_v1,
  set_statement_no_good_index_used_v1,
  end_statement_v1,
  start_socket_wait_v1,
  end_socket_wait_v1,
  set_socket_state_v1,
  set_socket_info_v1,
  set_socket_thread_owner_v1,
  pfs_digest_start_v1,
  pfs_digest_add_token_v1,
  set_thread_connect_attrs_v1,
};

static void* get_interface(int version)
{
  switch (version)
  {
  case PSI_VERSION_1:
    return &PFS_v1;
  default:
    return NULL;
  }
}

C_MODE_END

struct PSI_bootstrap PFS_bootstrap=
{
  get_interface
};
