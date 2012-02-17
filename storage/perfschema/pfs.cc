/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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
#include "pfs.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_global.h"
#include "pfs_column_values.h"
#include "pfs_timer.h"
#include "pfs_events_waits.h"

/* Pending WL#4895 PERFORMANCE_SCHEMA Instrumenting Table IO */
#undef HAVE_TABLE_WAIT

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
  @page PAGE_INSTRUMENTATION_INTERFACE
  Performance schema: instrumentation interface page.
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
  - (a) If the performance schema is not initialized, do nothing
  - (b) If the object acted upon is not instrumented, do nothing
  - (c) otherwise, notify the performance schema of the operation
  about to be performed.

  The implementation of the instrumentation interface can:
  - decide that it is not interested by the event, and return NULL.
  In this context, 'interested' means whether the instrumentation for
  this object + event is turned on in the performance schema configuration
  (the SETUP_ tables).
  - decide that this event is to be instrumented.
  In this case, the instrumentation returns an opaque pointer,
  that acts as a listener.

  If a listener is returned, the instrumentation point then:
  - (d) invokes the "start" event method
  - (e) executes the instrumented code.
  - (f) invokes the "end" event method.

  If no listener is returned, only the instrumented code (e) is invoked.

  The following code fragment is annotated to show how in detail this pattern
  in implemented, when the instrumentation is compiled in:

@verbatim
static inline int mysql_mutex_lock(
  mysql_mutex_t *that, myf flags, const char *src_file, uint src_line)
{
  int result;
  struct PSI_mutex_locker *locker= NULL;

  ...... (a) .......... (b)
  if (PSI_server && that->m_psi)

  .......................... (c)
    if ((locker= PSI_server->get_thread_mutex_locker(that->m_psi,
                                                     PSI_MUTEX_LOCK)))

  ............... (d)
      PSI_server->start_mutex_wait(locker, src_file, src_line);

  ........ (e)
  result= pthread_mutex_lock(&that->m_mutex);

  if (locker)

  ............. (f)
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

  ........ (e)
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
  P(x1, x2, ..., x{i-1}, x_i, x{i+1}, ..., x_N
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

  Function composition such as F_2_to_3 o F_1_to_2 o F1 is implemented
  as PFS_single_stat_chain, where each link in the chain represents
  an individual F_{i}_to_{i+1} aggregation step.

  A single call to aggregate_single_stat_chain() updates all the tables
  described in the statistics chain.

  @section STAT_CHAIN Statistics chains

  Statistics chains are only used for on the fly aggregates,
  and are therefore all based initially on the '_CURRENT' base table that
  contains the data recorded.
  The following table aggregates are implemented with a statistics chain:

  EVENTS_WAITS_CURRENT --> EVENTS_WAITS_SUMMARY_BY_INSTANCE
  --> EVENTS_WAITS_SUMMARY_BY_EVENT_NAME

  This relationship is between classes.

  In terms of object instances, or records, this chain is implemented
  as a flyweight.

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

  @section HIGHER_LEVEL Higher level aggregates

  Note: no higher level aggregate is implemented yet,
  this section is a place holder.
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

static enum_operation_type mutex_operation_map[]=
{
  OPERATION_TYPE_LOCK,
  OPERATION_TYPE_TRYLOCK
};

static enum_operation_type rwlock_operation_map[]=
{
  OPERATION_TYPE_READLOCK,
  OPERATION_TYPE_WRITELOCK,
  OPERATION_TYPE_TRYREADLOCK,
  OPERATION_TYPE_TRYWRITELOCK
};

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

static void register_mutex_v1(const char *category,
                              PSI_mutex_info_v1 *info,
                              int count)
{
  REGISTER_BODY_V1(PSI_mutex_key,
                   mutex_instrument_prefix,
                   register_mutex_class)
}

static void register_rwlock_v1(const char *category,
                               PSI_rwlock_info_v1 *info,
                               int count)
{
  REGISTER_BODY_V1(PSI_rwlock_key,
                   rwlock_instrument_prefix,
                   register_rwlock_class)
}

static void register_cond_v1(const char *category,
                             PSI_cond_info_v1 *info,
                             int count)
{
  REGISTER_BODY_V1(PSI_cond_key,
                   cond_instrument_prefix,
                   register_cond_class)
}

static void register_thread_v1(const char *category,
                               PSI_thread_info_v1 *info,
                               int count)
{
  REGISTER_BODY_V1(PSI_thread_key,
                   thread_instrument_prefix,
                   register_thread_class)
}

static void register_file_v1(const char *category,
                             PSI_file_info_v1 *info,
                             int count)
{
  REGISTER_BODY_V1(PSI_file_key,
                   file_instrument_prefix,
                   register_file_class)
}

#define INIT_BODY_V1(T, KEY, ID)                                            \
  PFS_##T##_class *klass;                                                   \
  PFS_##T *pfs;                                                             \
  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS); \
  if (unlikely(pfs_thread == NULL))                                         \
    return NULL;                                                            \
  if (! pfs_thread->m_enabled)                                              \
    return NULL;                                                            \
  klass= find_##T##_class(KEY);                                             \
  if (unlikely(klass == NULL))                                              \
    return NULL;                                                            \
  if (! klass->m_enabled)                                                   \
    return NULL;                                                            \
  pfs= create_##T(klass, ID);                                               \
  return reinterpret_cast<PSI_##T *> (pfs)

static PSI_mutex*
init_mutex_v1(PSI_mutex_key key, const void *identity)
{
  INIT_BODY_V1(mutex, key, identity);
}

static void destroy_mutex_v1(PSI_mutex* mutex)
{
  PFS_mutex *pfs= reinterpret_cast<PFS_mutex*> (mutex);
  destroy_mutex(pfs);
}

static PSI_rwlock*
init_rwlock_v1(PSI_rwlock_key key, const void *identity)
{
  INIT_BODY_V1(rwlock, key, identity);
}

static void destroy_rwlock_v1(PSI_rwlock* rwlock)
{
  PFS_rwlock *pfs= reinterpret_cast<PFS_rwlock*> (rwlock);
  destroy_rwlock(pfs);
}

static PSI_cond*
init_cond_v1(PSI_cond_key key, const void *identity)
{
  INIT_BODY_V1(cond, key, identity);
}

static void destroy_cond_v1(PSI_cond* cond)
{
  PFS_cond *pfs= reinterpret_cast<PFS_cond*> (cond);
  destroy_cond(pfs);
}

static PSI_table_share*
get_table_share_v1(const char *schema_name, int schema_name_length,
                   const char *table_name, int table_name_length,
                   const void *identity)
{
#ifdef HAVE_TABLE_WAIT
  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(pfs_thread == NULL))
    return NULL;
  PFS_table_share* share;
  share= find_or_create_table_share(pfs_thread,
                                    schema_name, schema_name_length,
                                    table_name, table_name_length);
  return reinterpret_cast<PSI_table_share*> (share);
#else
  return NULL;
#endif
}

static void release_table_share_v1(PSI_table_share* share)
{
  /*
    To be implemented by WL#4895 PERFORMANCE_SCHEMA Instrumenting Table IO.
  */
}

static PSI_table*
open_table_v1(PSI_table_share *share, const void *identity)
{
  PFS_table_share *pfs_table_share=
    reinterpret_cast<PFS_table_share*> (share);
  PFS_table *pfs_table;
  DBUG_ASSERT(pfs_table_share);
  pfs_table= create_table(pfs_table_share, identity);
  return reinterpret_cast<PSI_table *> (pfs_table);
}

static void close_table_v1(PSI_table *table)
{
  PFS_table *pfs= reinterpret_cast<PFS_table*> (table);
  DBUG_ASSERT(pfs);
  destroy_table(pfs);
}

static void create_file_v1(PSI_file_key key, const char *name, File file)
{
  int index= (int) file;
  if (unlikely(index < 0))
    return;
  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(pfs_thread == NULL))
    return;
  if (! pfs_thread->m_enabled)
    return;
  PFS_file_class *klass= find_file_class(key);
  if (unlikely(klass == NULL))
    return;
  if (! klass->m_enabled)
    return;
  if (likely(index < file_handle_max))
  {
    uint len= strlen(name);
    PFS_file *pfs= find_or_create_file(pfs_thread, klass, name, len);
    file_handle_array[index]= pfs;
  }
  else
    file_handle_lost++;
}

struct PFS_spawn_thread_arg
{
  PFS_thread *m_parent_thread;
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
    pfs= create_thread(klass, typed_arg->m_child_identity, 0);
  else
    pfs= NULL;
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

static int spawn_thread_v1(PSI_thread_key key,
                           pthread_t *thread, const pthread_attr_t *attr,
                           void *(*start_routine)(void*), void *arg)
{
  PFS_spawn_thread_arg *psi_arg;

  /* psi_arg can not be global, and can not be a local variable. */
  psi_arg= (PFS_spawn_thread_arg*) my_malloc(sizeof(PFS_spawn_thread_arg),
                                             MYF(MY_WME));
  if (unlikely(psi_arg == NULL))
    return EAGAIN;

  psi_arg->m_parent_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  psi_arg->m_child_key= key;
  psi_arg->m_child_identity= (arg ? arg : thread);
  psi_arg->m_user_start_routine= start_routine;
  psi_arg->m_user_arg= arg;

  int result= pthread_create(thread, attr, pfs_spawn_thread, psi_arg);
  if (unlikely(result != 0))
    my_free(psi_arg);
  return result;
}

static PSI_thread*
new_thread_v1(PSI_thread_key key, const void *identity, ulong thread_id)
{
  PFS_thread *pfs;

  PFS_thread_class *klass= find_thread_class(key);
  if (likely(klass != NULL))
    pfs= create_thread(klass, identity, thread_id);
  else
    pfs= NULL;

  return reinterpret_cast<PSI_thread*> (pfs);
}

static void set_thread_id_v1(PSI_thread *thread, unsigned long id)
{
  DBUG_ASSERT(thread);
  PFS_thread *pfs= reinterpret_cast<PFS_thread*> (thread);
  pfs->m_thread_id= id;
}

static PSI_thread*
get_thread_v1(void)
{
  PFS_thread *pfs= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  return reinterpret_cast<PSI_thread*> (pfs);
}

static void set_thread_v1(PSI_thread* thread)
{
  PFS_thread *pfs= reinterpret_cast<PFS_thread*> (thread);
  my_pthread_setspecific_ptr(THR_PFS, pfs);
}

static void delete_current_thread_v1(void)
{
  PFS_thread *thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (thread != NULL)
  {
    my_pthread_setspecific_ptr(THR_PFS, NULL);
    destroy_thread(thread);
  }
}

static void delete_thread_v1(PSI_thread *thread)
{
  PFS_thread *pfs= reinterpret_cast<PFS_thread*> (thread);
  if (pfs != NULL)
    destroy_thread(pfs);
}

static PSI_mutex_locker*
get_thread_mutex_locker_v1(PSI_mutex_locker_state *state,
                           PSI_mutex *mutex, PSI_mutex_operation op)
{
  PFS_mutex *pfs_mutex= reinterpret_cast<PFS_mutex*> (mutex);
  DBUG_ASSERT((int) op >= 0);
  DBUG_ASSERT((uint) op < array_elements(mutex_operation_map));
  DBUG_ASSERT(pfs_mutex != NULL);
  DBUG_ASSERT(pfs_mutex->m_class != NULL);
  if (! flag_events_waits_current)
    return NULL;
  if (! pfs_mutex->m_class->m_enabled)
    return NULL;
  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(pfs_thread == NULL))
    return NULL;
  if (! pfs_thread->m_enabled)
    return NULL;
  if (unlikely(pfs_thread->m_wait_locker_count >= LOCKER_STACK_SIZE))
  {
    locker_lost++;
    return NULL;
  }
  PFS_wait_locker *pfs_locker= &pfs_thread->m_wait_locker_stack
    [pfs_thread->m_wait_locker_count];

  pfs_locker->m_target.m_mutex= pfs_mutex;
  pfs_locker->m_waits_current.m_thread= pfs_thread;
  pfs_locker->m_waits_current.m_class= pfs_mutex->m_class;
  if (pfs_mutex->m_class->m_timed)
  {
    pfs_locker->m_timer_name= wait_timer;
    pfs_locker->m_waits_current.m_timer_state= TIMER_STATE_STARTING;
  }
  else
    pfs_locker->m_waits_current.m_timer_state= TIMER_STATE_UNTIMED;
  pfs_locker->m_waits_current.m_object_instance_addr= pfs_mutex->m_identity;
  pfs_locker->m_waits_current.m_event_id= pfs_thread->m_event_id++;
  pfs_locker->m_waits_current.m_operation= mutex_operation_map[(int) op];
  pfs_locker->m_waits_current.m_wait_class= WAIT_CLASS_MUTEX;

  pfs_thread->m_wait_locker_count++;
  return reinterpret_cast<PSI_mutex_locker*> (pfs_locker);
}

static PSI_rwlock_locker*
get_thread_rwlock_locker_v1(PSI_rwlock_locker_state *state,
                            PSI_rwlock *rwlock, PSI_rwlock_operation op)
{
  PFS_rwlock *pfs_rwlock= reinterpret_cast<PFS_rwlock*> (rwlock);
  DBUG_ASSERT(static_cast<int> (op) >= 0);
  DBUG_ASSERT(static_cast<uint> (op) < array_elements(rwlock_operation_map));
  DBUG_ASSERT(pfs_rwlock != NULL);
  DBUG_ASSERT(pfs_rwlock->m_class != NULL);
  if (! flag_events_waits_current)
    return NULL;
  if (! pfs_rwlock->m_class->m_enabled)
    return NULL;
  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(pfs_thread == NULL))
    return NULL;
  if (! pfs_thread->m_enabled)
    return NULL;
  if (unlikely(pfs_thread->m_wait_locker_count >= LOCKER_STACK_SIZE))
  {
    locker_lost++;
    return NULL;
  }
  PFS_wait_locker *pfs_locker= &pfs_thread->m_wait_locker_stack
    [pfs_thread->m_wait_locker_count];

  pfs_locker->m_target.m_rwlock= pfs_rwlock;
  pfs_locker->m_waits_current.m_thread= pfs_thread;
  pfs_locker->m_waits_current.m_class= pfs_rwlock->m_class;
  if (pfs_rwlock->m_class->m_timed)
  {
    pfs_locker->m_timer_name= wait_timer;
    pfs_locker->m_waits_current.m_timer_state= TIMER_STATE_STARTING;
  }
  else
    pfs_locker->m_waits_current.m_timer_state= TIMER_STATE_UNTIMED;
  pfs_locker->m_waits_current.m_object_instance_addr= pfs_rwlock->m_identity;
  pfs_locker->m_waits_current.m_event_id= pfs_thread->m_event_id++;
  pfs_locker->m_waits_current.m_operation=
    rwlock_operation_map[static_cast<int> (op)];
  pfs_locker->m_waits_current.m_wait_class= WAIT_CLASS_RWLOCK;

  pfs_thread->m_wait_locker_count++;
  return reinterpret_cast<PSI_rwlock_locker*> (pfs_locker);
}

static PSI_cond_locker*
get_thread_cond_locker_v1(PSI_cond_locker_state *state,
                          PSI_cond *cond, PSI_mutex * /* unused: mutex */,
                          PSI_cond_operation op)
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
  DBUG_ASSERT(pfs_cond != NULL);
  DBUG_ASSERT(pfs_cond->m_class != NULL);
  if (! flag_events_waits_current)
    return NULL;
  if (! pfs_cond->m_class->m_enabled)
    return NULL;
  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(pfs_thread == NULL))
    return NULL;
  if (! pfs_thread->m_enabled)
    return NULL;
  if (unlikely(pfs_thread->m_wait_locker_count >= LOCKER_STACK_SIZE))
  {
    locker_lost++;
    return NULL;
  }
  PFS_wait_locker *pfs_locker= &pfs_thread->m_wait_locker_stack
    [pfs_thread->m_wait_locker_count];

  pfs_locker->m_target.m_cond= pfs_cond;
  pfs_locker->m_waits_current.m_thread= pfs_thread;
  pfs_locker->m_waits_current.m_class= pfs_cond->m_class;
  if (pfs_cond->m_class->m_timed)
  {
    pfs_locker->m_timer_name= wait_timer;
    pfs_locker->m_waits_current.m_timer_state= TIMER_STATE_STARTING;
  }
  else
    pfs_locker->m_waits_current.m_timer_state= TIMER_STATE_UNTIMED;
  pfs_locker->m_waits_current.m_object_instance_addr= pfs_cond->m_identity;
  pfs_locker->m_waits_current.m_event_id= pfs_thread->m_event_id++;
  pfs_locker->m_waits_current.m_operation=
    cond_operation_map[static_cast<int> (op)];
  pfs_locker->m_waits_current.m_wait_class= WAIT_CLASS_COND;

  pfs_thread->m_wait_locker_count++;
  return reinterpret_cast<PSI_cond_locker*> (pfs_locker);
}

static PSI_table_locker*
get_thread_table_locker_v1(PSI_table_locker_state *state,
                           PSI_table *table)
{
  PFS_table *pfs_table= reinterpret_cast<PFS_table*> (table);
  DBUG_ASSERT(pfs_table != NULL);
  DBUG_ASSERT(pfs_table->m_share != NULL);
  if (! flag_events_waits_current)
    return NULL;
  if (! pfs_table->m_share->m_enabled)
    return NULL;
  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(pfs_thread == NULL))
    return NULL;
  if (! pfs_thread->m_enabled)
    return NULL;
  if (unlikely(pfs_thread->m_wait_locker_count >= LOCKER_STACK_SIZE))
  {
    locker_lost++;
    return NULL;
  }
  PFS_wait_locker *pfs_locker= &pfs_thread->m_wait_locker_stack
    [pfs_thread->m_wait_locker_count];

  pfs_locker->m_target.m_table= pfs_table;
  pfs_locker->m_waits_current.m_thread= pfs_thread;
  pfs_locker->m_waits_current.m_class= &global_table_class;
  if (pfs_table->m_share->m_timed)
  {
    pfs_locker->m_timer_name= wait_timer;
    pfs_locker->m_waits_current.m_timer_state= TIMER_STATE_STARTING;
  }
  else
    pfs_locker->m_waits_current.m_timer_state= TIMER_STATE_UNTIMED;
  pfs_locker->m_waits_current.m_object_instance_addr= pfs_table->m_identity;
  pfs_locker->m_waits_current.m_event_id= pfs_thread->m_event_id++;
  pfs_locker->m_waits_current.m_wait_class= WAIT_CLASS_TABLE;

  pfs_thread->m_wait_locker_count++;
  return reinterpret_cast<PSI_table_locker*> (pfs_locker);
}

static PSI_file_locker*
get_thread_file_name_locker_v1(PSI_file_locker_state *state,
                               PSI_file_key key,
                               PSI_file_operation op,
                               const char *name, const void *identity)
{
  DBUG_ASSERT(static_cast<int> (op) >= 0);
  DBUG_ASSERT(static_cast<uint> (op) < array_elements(file_operation_map));

  if (! flag_events_waits_current)
    return NULL;
  PFS_file_class *klass= find_file_class(key);
  if (unlikely(klass == NULL))
    return NULL;
  if (! klass->m_enabled)
    return NULL;
  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(pfs_thread == NULL))
    return NULL;
  if (! pfs_thread->m_enabled)
    return NULL;
  if (unlikely(pfs_thread->m_wait_locker_count >= LOCKER_STACK_SIZE))
  {
    locker_lost++;
    return NULL;
  }
  uint len= strlen(name);
  PFS_file *pfs_file= find_or_create_file(pfs_thread, klass, name, len);
  if (unlikely(pfs_file == NULL))
    return NULL;

  PFS_wait_locker *pfs_locker= &pfs_thread->m_wait_locker_stack
    [pfs_thread->m_wait_locker_count];

  pfs_locker->m_target.m_file= pfs_file;
  pfs_locker->m_waits_current.m_thread= pfs_thread;
  pfs_locker->m_waits_current.m_class= pfs_file->m_class;
  if (pfs_file->m_class->m_timed)
  {
    pfs_locker->m_timer_name= wait_timer;
    pfs_locker->m_waits_current.m_timer_state= TIMER_STATE_STARTING;
  }
  else
    pfs_locker->m_waits_current.m_timer_state= TIMER_STATE_UNTIMED;
  pfs_locker->m_waits_current.m_object_instance_addr= pfs_file;
  pfs_locker->m_waits_current.m_object_name= pfs_file->m_filename;
  pfs_locker->m_waits_current.m_object_name_length=
    pfs_file->m_filename_length;
  pfs_locker->m_waits_current.m_event_id= pfs_thread->m_event_id++;
  pfs_locker->m_waits_current.m_operation=
    file_operation_map[static_cast<int> (op)];
  pfs_locker->m_waits_current.m_wait_class= WAIT_CLASS_FILE;

  pfs_thread->m_wait_locker_count++;
  return reinterpret_cast<PSI_file_locker*> (pfs_locker);
}

static PSI_file_locker*
get_thread_file_stream_locker_v1(PSI_file_locker_state *state,
                                 PSI_file *file, PSI_file_operation op)
{
  PFS_file *pfs_file= reinterpret_cast<PFS_file*> (file);

  DBUG_ASSERT(static_cast<int> (op) >= 0);
  DBUG_ASSERT(static_cast<uint> (op) < array_elements(file_operation_map));
  DBUG_ASSERT(pfs_file != NULL);
  DBUG_ASSERT(pfs_file->m_class != NULL);

  if (! flag_events_waits_current)
    return NULL;
  if (! pfs_file->m_class->m_enabled)
    return NULL;
  PFS_thread *pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
  if (unlikely(pfs_thread == NULL))
    return NULL;
  if (! pfs_thread->m_enabled)
    return NULL;
  if (unlikely(pfs_thread->m_wait_locker_count >= LOCKER_STACK_SIZE))
  {
    locker_lost++;
    return NULL;
  }
  PFS_wait_locker *pfs_locker= &pfs_thread->m_wait_locker_stack
    [pfs_thread->m_wait_locker_count];

  pfs_locker->m_target.m_file= pfs_file;
  pfs_locker->m_waits_current.m_thread= pfs_thread;
  pfs_locker->m_waits_current.m_class= pfs_file->m_class;
  if (pfs_file->m_class->m_timed)
  {
    pfs_locker->m_timer_name= wait_timer;
    pfs_locker->m_waits_current.m_timer_state= TIMER_STATE_STARTING;
  }
  else
    pfs_locker->m_waits_current.m_timer_state= TIMER_STATE_UNTIMED;
  pfs_locker->m_waits_current.m_object_instance_addr= pfs_file;
  pfs_locker->m_waits_current.m_object_name= pfs_file->m_filename;
  pfs_locker->m_waits_current.m_object_name_length=
    pfs_file->m_filename_length;
  pfs_locker->m_waits_current.m_event_id= pfs_thread->m_event_id++;
  pfs_locker->m_waits_current.m_operation=
    file_operation_map[static_cast<int> (op)];
  pfs_locker->m_waits_current.m_wait_class= WAIT_CLASS_FILE;

  pfs_thread->m_wait_locker_count++;
  return reinterpret_cast<PSI_file_locker*> (pfs_locker);
}

static PSI_file_locker*
get_thread_file_descriptor_locker_v1(PSI_file_locker_state *state,
                                     File file, PSI_file_operation op)
{
  int index= static_cast<int> (file);

  DBUG_ASSERT(static_cast<int> (op) >= 0);
  DBUG_ASSERT(static_cast<uint> (op) < array_elements(file_operation_map));

  if (! flag_events_waits_current)
    return NULL;
  if (likely((index >= 0) && (index < file_handle_max)))
  {
    PFS_file *pfs_file= file_handle_array[index];
    if (likely(pfs_file != NULL))
    {
      PFS_thread *pfs_thread;

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

      DBUG_ASSERT(pfs_file->m_class != NULL);
      if (! pfs_file->m_class->m_enabled)
        return NULL;
      pfs_thread= my_pthread_getspecific_ptr(PFS_thread*, THR_PFS);
      if (unlikely(pfs_thread == NULL))
        return NULL;
      if (! pfs_thread->m_enabled)
        return NULL;
      if (unlikely(pfs_thread->m_wait_locker_count >= LOCKER_STACK_SIZE))
      {
        locker_lost++;
        return NULL;
      }
      PFS_wait_locker *pfs_locker= &pfs_thread->m_wait_locker_stack
        [pfs_thread->m_wait_locker_count];

      pfs_locker->m_target.m_file= pfs_file;
      pfs_locker->m_waits_current.m_thread= pfs_thread;
      pfs_locker->m_waits_current.m_class= pfs_file->m_class;
      if (pfs_file->m_class->m_timed)
      {
        pfs_locker->m_timer_name= wait_timer;
        pfs_locker->m_waits_current.m_timer_state= TIMER_STATE_STARTING;
      }
      else
        pfs_locker->m_waits_current.m_timer_state= TIMER_STATE_UNTIMED;
      pfs_locker->m_waits_current.m_object_instance_addr= pfs_file;
      pfs_locker->m_waits_current.m_object_name= pfs_file->m_filename;
      pfs_locker->m_waits_current.m_object_name_length=
        pfs_file->m_filename_length;
      pfs_locker->m_waits_current.m_event_id= pfs_thread->m_event_id++;
      pfs_locker->m_waits_current.m_operation=
        file_operation_map[static_cast<int> (op)];
      pfs_locker->m_waits_current.m_wait_class= WAIT_CLASS_FILE;

      pfs_thread->m_wait_locker_count++;
      return reinterpret_cast<PSI_file_locker*> (pfs_locker);
    }
  }
  return NULL;
}

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
  PFS_thread *pfs_thread= reinterpret_cast<PFS_thread*> (thread);
  DBUG_ASSERT(pfs_thread != NULL);

  if (unlikely(! flag_events_waits_current))
    return;
  if (! pfs_mutex->m_class->m_enabled)
    return;
  if (! pfs_thread->m_enabled)
    return;

  if (pfs_mutex->m_class->m_timed)
  {
    ulonglong locked_time;
    locked_time= get_timer_value(wait_timer) - pfs_mutex->m_last_locked;
    aggregate_single_stat_chain(&pfs_mutex->m_lock_stat, locked_time);
  }
#endif
}

static void unlock_rwlock_v1(PSI_rwlock *rwlock)
{
  PFS_rwlock *pfs_rwlock= reinterpret_cast<PFS_rwlock*> (rwlock);
  DBUG_ASSERT(pfs_rwlock != NULL);
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
  if (pfs_rwlock->m_writer)
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
  PFS_thread *pfs_thread= reinterpret_cast<PFS_thread*> (thread);
  DBUG_ASSERT(pfs_thread != NULL);

  if (unlikely(! flag_events_waits_current))
    return;
  if (! pfs_rwlock->m_class->m_enabled)
    return;
  if (! pfs_thread->m_enabled)
    return;

  ulonglong locked_time;
  if (last_writer)
  {
    if (pfs_rwlock->m_class->m_timed)
    {
      locked_time= get_timer_value(wait_timer) - pfs_rwlock->m_last_written;
      aggregate_single_stat_chain(&pfs_rwlock->m_write_lock_stat, locked_time);
    }
  }
  else if (last_reader)
  {
    if (pfs_rwlock->m_class->m_timed)
    {
      locked_time= get_timer_value(wait_timer) - pfs_rwlock->m_last_read;
      aggregate_single_stat_chain(&pfs_rwlock->m_read_lock_stat, locked_time);
    }
  }
#else
  (void) last_reader;
  (void) last_writer;
#endif
}

static void signal_cond_v1(PSI_cond* cond)
{
  PFS_cond *pfs_cond= reinterpret_cast<PFS_cond*> (cond);
  DBUG_ASSERT(pfs_cond != NULL);

  pfs_cond->m_cond_stat.m_signal_count++;
}

static void broadcast_cond_v1(PSI_cond* cond)
{
  PFS_cond *pfs_cond= reinterpret_cast<PFS_cond*> (cond);
  DBUG_ASSERT(pfs_cond != NULL);

  pfs_cond->m_cond_stat.m_broadcast_count++;
}

static void start_mutex_wait_v1(PSI_mutex_locker* locker,
                                const char *src_file, uint src_line)
{
  PFS_wait_locker *pfs_locker= reinterpret_cast<PFS_wait_locker*> (locker);
  DBUG_ASSERT(pfs_locker != NULL);

  PFS_events_waits *wait= &pfs_locker->m_waits_current;
  if (wait->m_timer_state == TIMER_STATE_STARTING)
  {
    wait->m_timer_start= get_timer_value(pfs_locker->m_timer_name);
    wait->m_timer_state= TIMER_STATE_STARTED;
  }
  wait->m_source_file= src_file;
  wait->m_source_line= src_line;
}

static void end_mutex_wait_v1(PSI_mutex_locker* locker, int rc)
{
  PFS_wait_locker *pfs_locker= reinterpret_cast<PFS_wait_locker*> (locker);
  DBUG_ASSERT(pfs_locker != NULL);
  PFS_events_waits *wait= &pfs_locker->m_waits_current;

  if (wait->m_timer_state == TIMER_STATE_STARTED)
  {
    wait->m_timer_end= get_timer_value(pfs_locker->m_timer_name);
    wait->m_timer_state= TIMER_STATE_TIMED;
  }
  if (flag_events_waits_history)
    insert_events_waits_history(wait->m_thread, wait);
  if (flag_events_waits_history_long)
    insert_events_waits_history_long(wait);

  if (rc == 0)
  {
    /* Thread safe: we are protected by the instrumented mutex */
    PFS_mutex *mutex= pfs_locker->m_target.m_mutex;
    PFS_single_stat_chain *stat= find_per_thread_mutex_class_wait_stat(wait->m_thread, mutex->m_class);
    mutex->m_owner= wait->m_thread;
    mutex->m_last_locked= wait->m_timer_end;

    /* If timed then aggregate stats, else increment the value counts only */
    if (wait->m_timer_state == TIMER_STATE_TIMED)
    {
      ulonglong wait_time= wait->m_timer_end - wait->m_timer_start;
      aggregate_single_stat_chain(&mutex->m_wait_stat, wait_time);
      aggregate_single_stat_chain(stat, wait_time);
    }
    else
    {
      increment_single_stat_chain(&mutex->m_wait_stat);
      increment_single_stat_chain(stat);
    }
  }
  wait->m_thread->m_wait_locker_count--;
}

static void start_rwlock_rdwait_v1(PSI_rwlock_locker* locker,
                                   const char *src_file, uint src_line)
{
  PFS_wait_locker *pfs_locker= reinterpret_cast<PFS_wait_locker*> (locker);
  DBUG_ASSERT(pfs_locker != NULL);

  PFS_events_waits *wait= &pfs_locker->m_waits_current;
  if (wait->m_timer_state == TIMER_STATE_STARTING)
  {
    wait->m_timer_start= get_timer_value(pfs_locker->m_timer_name);
    wait->m_timer_state= TIMER_STATE_STARTED;
  }
  wait->m_source_file= src_file;
  wait->m_source_line= src_line;
}

static void end_rwlock_rdwait_v1(PSI_rwlock_locker* locker, int rc)
{
  PFS_wait_locker *pfs_locker= reinterpret_cast<PFS_wait_locker*> (locker);
  DBUG_ASSERT(pfs_locker != NULL);
  PFS_events_waits *wait= &pfs_locker->m_waits_current;

  if (wait->m_timer_state == TIMER_STATE_STARTED)
  {
    wait->m_timer_end= get_timer_value(pfs_locker->m_timer_name);
    wait->m_timer_state= TIMER_STATE_TIMED;
  }
  if (flag_events_waits_history)
    insert_events_waits_history(wait->m_thread, wait);
  if (flag_events_waits_history_long)
    insert_events_waits_history_long(wait);

  if (rc == 0)
  {
    /*
      Warning:
      Multiple threads can execute this section concurrently
      (since multiple readers can execute in parallel).
      The statistics generated are not safe, which is why they are
      just statistics, not facts.
    */
    PFS_rwlock *rwlock= pfs_locker->m_target.m_rwlock;
    PFS_single_stat_chain *stat= find_per_thread_rwlock_class_wait_stat(wait->m_thread, rwlock->m_class);

    if (rwlock->m_readers == 0)
      rwlock->m_last_read= wait->m_timer_end;
    rwlock->m_writer= NULL;
    rwlock->m_readers++;

    /* If timed then aggregate stats, else increment the value counts only */
    if (wait->m_timer_state == TIMER_STATE_TIMED)
    {
      ulonglong wait_time= wait->m_timer_end - wait->m_timer_start;
      aggregate_single_stat_chain(&rwlock->m_wait_stat, wait_time);
      aggregate_single_stat_chain(stat, wait_time);
    }
    else
    {
      increment_single_stat_chain(&rwlock->m_wait_stat);
      increment_single_stat_chain(stat);
    }
  }
  wait->m_thread->m_wait_locker_count--;
}

static void start_rwlock_wrwait_v1(PSI_rwlock_locker* locker,
                                   const char *src_file, uint src_line)
{
  PFS_wait_locker *pfs_locker= reinterpret_cast<PFS_wait_locker*> (locker);
  DBUG_ASSERT(pfs_locker != NULL);

  PFS_events_waits *wait= &pfs_locker->m_waits_current;
  if (wait->m_timer_state == TIMER_STATE_STARTING)
  {
    wait->m_timer_start= get_timer_value(pfs_locker->m_timer_name);
    wait->m_timer_state= TIMER_STATE_STARTED;
  }
  wait->m_source_file= src_file;
  wait->m_source_line= src_line;
}

static void end_rwlock_wrwait_v1(PSI_rwlock_locker* locker, int rc)
{
  PFS_wait_locker *pfs_locker= reinterpret_cast<PFS_wait_locker*> (locker);
  DBUG_ASSERT(pfs_locker != NULL);
  PFS_events_waits *wait= &pfs_locker->m_waits_current;

  if (wait->m_timer_state == TIMER_STATE_STARTED)
  {
    wait->m_timer_end= get_timer_value(pfs_locker->m_timer_name);
    wait->m_timer_state= TIMER_STATE_TIMED;
  }
  if (flag_events_waits_history)
    insert_events_waits_history(wait->m_thread, wait);
  if (flag_events_waits_history_long)
    insert_events_waits_history_long(wait);

  if (rc == 0)
  {
    /* Thread safe : we are protected by the instrumented rwlock */
    PFS_rwlock *rwlock= pfs_locker->m_target.m_rwlock;
    PFS_single_stat_chain *stat= find_per_thread_rwlock_class_wait_stat(wait->m_thread, rwlock->m_class);
    rwlock->m_writer= wait->m_thread;
    rwlock->m_last_written= wait->m_timer_end;
    /* Reset the readers stats, they could be off */
    rwlock->m_readers= 0;
    rwlock->m_last_read= 0;

    /* If timed then aggregate stats, else increment the value counts only */
    if (wait->m_timer_state == TIMER_STATE_TIMED)
    {
      ulonglong wait_time= wait->m_timer_end - wait->m_timer_start;
      aggregate_single_stat_chain(&rwlock->m_wait_stat, wait_time);
      aggregate_single_stat_chain(stat, wait_time);
    }
    else
    {
      increment_single_stat_chain(&rwlock->m_wait_stat);
      increment_single_stat_chain(stat);
    }
  }
  wait->m_thread->m_wait_locker_count--;
}

static void start_cond_wait_v1(PSI_cond_locker* locker,
                               const char *src_file, uint src_line)
{
  PFS_wait_locker *pfs_locker= reinterpret_cast<PFS_wait_locker*> (locker);
  DBUG_ASSERT(pfs_locker != NULL);

  PFS_events_waits *wait= &pfs_locker->m_waits_current;
  if (wait->m_timer_state == TIMER_STATE_STARTING)
  {
    wait->m_timer_start= get_timer_value(pfs_locker->m_timer_name);
    wait->m_timer_state= TIMER_STATE_STARTED;
  }
  wait->m_source_file= src_file;
  wait->m_source_line= src_line;
}

static void end_cond_wait_v1(PSI_cond_locker* locker, int rc)
{
  PFS_wait_locker *pfs_locker= reinterpret_cast<PFS_wait_locker*> (locker);
  DBUG_ASSERT(pfs_locker != NULL);
  PFS_events_waits *wait= &pfs_locker->m_waits_current;

  if (wait->m_timer_state == TIMER_STATE_STARTED)
  {
    wait->m_timer_end= get_timer_value(pfs_locker->m_timer_name);
    wait->m_timer_state= TIMER_STATE_TIMED;
  }
  if (flag_events_waits_history)
    insert_events_waits_history(wait->m_thread, wait);
  if (flag_events_waits_history_long)
    insert_events_waits_history_long(wait);

  if (rc == 0)
  {
    /*
      Not thread safe, race conditions will occur.
      A first race condition is:
      - thread 1 waits on cond A
      - thread 2 waits on cond B
      threads 1 and 2 compete when updating the same cond A
      statistics, possibly missing a min / max / sum / count.
      A second race condition is:
      - thread 1 waits on cond A
      - thread 2 destroys cond A
      - thread 2 or 3 creates cond B in the same condition slot
      thread 1 will then aggregate statistics about defunct A
      in condition B.
      This is accepted, the data will be slightly inaccurate.
    */
    PFS_cond *cond= pfs_locker->m_target.m_cond;
    PFS_single_stat_chain *stat= find_per_thread_cond_class_wait_stat(wait->m_thread, cond->m_class);

    /* If timed then aggregate stats, else increment the value counts only */
    if (wait->m_timer_state == TIMER_STATE_TIMED)
    {
      ulonglong wait_time= wait->m_timer_end - wait->m_timer_start;
      aggregate_single_stat_chain(&cond->m_wait_stat, wait_time);
      aggregate_single_stat_chain(stat, wait_time);
    }
    else
    {
      increment_single_stat_chain(&cond->m_wait_stat);
      increment_single_stat_chain(stat);
    }
  }
  wait->m_thread->m_wait_locker_count--;
}

static void start_table_wait_v1(PSI_table_locker* locker,
                                const char *src_file, uint src_line)
{
  PFS_wait_locker *pfs_locker= reinterpret_cast<PFS_wait_locker*> (locker);
  DBUG_ASSERT(pfs_locker != NULL);

  PFS_events_waits *wait= &pfs_locker->m_waits_current;
  if (wait->m_timer_state == TIMER_STATE_STARTING)
  {
    wait->m_timer_start= get_timer_value(pfs_locker->m_timer_name);
    wait->m_timer_state= TIMER_STATE_STARTED;
  }
  wait->m_source_file= src_file;
  wait->m_source_line= src_line;
  wait->m_operation= OPERATION_TYPE_LOCK;
  PFS_table_share *share= pfs_locker->m_target.m_table->m_share;
  wait->m_schema_name= share->m_schema_name;
  wait->m_schema_name_length= share->m_schema_name_length;
  wait->m_object_name= share->m_table_name;
  wait->m_object_name_length= share->m_table_name_length;
}

static void end_table_wait_v1(PSI_table_locker* locker)
{
  PFS_wait_locker *pfs_locker= reinterpret_cast<PFS_wait_locker*> (locker);
  DBUG_ASSERT(pfs_locker != NULL);
  PFS_events_waits *wait= &pfs_locker->m_waits_current;

  if (wait->m_timer_state == TIMER_STATE_STARTED)
  {
    wait->m_timer_end= get_timer_value(pfs_locker->m_timer_name);
    wait->m_timer_state= TIMER_STATE_TIMED;
  }
  if (flag_events_waits_history)
    insert_events_waits_history(wait->m_thread, wait);
  if (flag_events_waits_history_long)
    insert_events_waits_history_long(wait);

  PFS_table *table= pfs_locker->m_target.m_table;

  /* If timed then aggregate stats, else increment the value counts only */
  if (wait->m_timer_state == TIMER_STATE_TIMED)
  {
    ulonglong wait_time= wait->m_timer_end - wait->m_timer_start;
    aggregate_single_stat_chain(&table->m_wait_stat, wait_time);
  }
  else
  {
    increment_single_stat_chain(&table->m_wait_stat);
  }

  /*
    There is currently no per table and per thread aggregation.
    The number of tables in the application is arbitrary, and may be high.
    The number of slots per thread to hold aggregates is fixed,
    and is constrained by memory.
    Implementing a per thread and per table aggregate has not been
    decided yet.
    If it's implemented, it's likely that the user will have to specify,
    per table name, if the aggregate per thread is to be computed or not.
    This will mean a SETUP_ table.
  */
  wait->m_thread->m_wait_locker_count--;
}

static void start_file_wait_v1(PSI_file_locker *locker,
                               size_t count,
                               const char *src_file,
                               uint src_line);

static void end_file_wait_v1(PSI_file_locker *locker,
                             size_t count);

static PSI_file* start_file_open_wait_v1(PSI_file_locker *locker,
                                         const char *src_file,
                                         uint src_line)
{
  PFS_wait_locker *pfs_locker= reinterpret_cast<PFS_wait_locker*> (locker);
  DBUG_ASSERT(pfs_locker != NULL);

  start_file_wait_v1(locker, 0, src_file, src_line);

  PFS_file *pfs_file= pfs_locker->m_target.m_file;
  return reinterpret_cast<PSI_file*> (pfs_file);
}

static void end_file_open_wait_v1(PSI_file_locker *locker)
{
  end_file_wait_v1(locker, 0);
}

static void end_file_open_wait_and_bind_to_descriptor_v1
  (PSI_file_locker *locker, File file)
{
  int index= (int) file;
  PFS_wait_locker *pfs_locker= reinterpret_cast<PFS_wait_locker*> (locker);
  DBUG_ASSERT(pfs_locker != NULL);

  end_file_wait_v1(locker, 0);

  PFS_file *pfs_file= pfs_locker->m_target.m_file;
  DBUG_ASSERT(pfs_file != NULL);

  if (likely(index >= 0))
  {
    if (likely(index < file_handle_max))
      file_handle_array[index]= pfs_file;
    else
      file_handle_lost++;
  }
  else
    release_file(pfs_file);
}

static void start_file_wait_v1(PSI_file_locker *locker,
                               size_t count,
                               const char *src_file,
                               uint src_line)
{
  PFS_wait_locker *pfs_locker= reinterpret_cast<PFS_wait_locker*> (locker);
  DBUG_ASSERT(pfs_locker != NULL);

  PFS_events_waits *wait= &pfs_locker->m_waits_current;
  if (wait->m_timer_state == TIMER_STATE_STARTING)
  {
    wait->m_timer_start= get_timer_value(pfs_locker->m_timer_name);
    wait->m_timer_state= TIMER_STATE_STARTED;
  }
  wait->m_source_file= src_file;
  wait->m_source_line= src_line;
  wait->m_number_of_bytes= count;
}

static void end_file_wait_v1(PSI_file_locker *locker,
                             size_t count)
{
  PFS_wait_locker *pfs_locker= reinterpret_cast<PFS_wait_locker*> (locker);
  DBUG_ASSERT(pfs_locker != NULL);
  PFS_events_waits *wait= &pfs_locker->m_waits_current;

  wait->m_number_of_bytes= count;
  if (wait->m_timer_state == TIMER_STATE_STARTED)
  {
    wait->m_timer_end= get_timer_value(pfs_locker->m_timer_name);
    wait->m_timer_state= TIMER_STATE_TIMED;
  }
  if (flag_events_waits_history)
    insert_events_waits_history(wait->m_thread, wait);
  if (flag_events_waits_history_long)
    insert_events_waits_history_long(wait);

  PFS_file *file= pfs_locker->m_target.m_file;
  PFS_single_stat_chain *stat= find_per_thread_file_class_wait_stat(wait->m_thread, file->m_class);

  /* If timed then aggregate stats, else increment the value counts only */
  if (wait->m_timer_state == TIMER_STATE_TIMED)
  {
    ulonglong wait_time= wait->m_timer_end - wait->m_timer_start;
    aggregate_single_stat_chain(&file->m_wait_stat, wait_time);
    aggregate_single_stat_chain(stat, wait_time);
  }
  else
  {
    increment_single_stat_chain(&file->m_wait_stat);
    increment_single_stat_chain(stat);
  }

  PFS_file_class *klass= file->m_class;

  switch(wait->m_operation)
  {
  case OPERATION_TYPE_FILEREAD:
    file->m_file_stat.m_count_read++;
    file->m_file_stat.m_read_bytes+= count;
    klass->m_file_stat.m_count_read++;
    klass->m_file_stat.m_read_bytes+= count;
    break;
  case OPERATION_TYPE_FILEWRITE:
    file->m_file_stat.m_count_write++;
    file->m_file_stat.m_write_bytes+= count;
    klass->m_file_stat.m_count_write++;
    klass->m_file_stat.m_write_bytes+= count;
    break;
  case OPERATION_TYPE_FILECLOSE:
  case OPERATION_TYPE_FILESTREAMCLOSE:
  case OPERATION_TYPE_FILESTAT:
    release_file(pfs_locker->m_target.m_file);
    break;
  case OPERATION_TYPE_FILEDELETE:
    destroy_file(wait->m_thread, pfs_locker->m_target.m_file);
    break;
  default:
    break;
  }

  wait->m_thread->m_wait_locker_count--;
}

PSI_v1 PFS_v1=
{
  register_mutex_v1,
  register_rwlock_v1,
  register_cond_v1,
  register_thread_v1,
  register_file_v1,
  init_mutex_v1,
  destroy_mutex_v1,
  init_rwlock_v1,
  destroy_rwlock_v1,
  init_cond_v1,
  destroy_cond_v1,
  get_table_share_v1,
  release_table_share_v1,
  open_table_v1,
  close_table_v1,
  create_file_v1,
  spawn_thread_v1,
  new_thread_v1,
  set_thread_id_v1,
  get_thread_v1,
  set_thread_v1,
  delete_current_thread_v1,
  delete_thread_v1,
  get_thread_mutex_locker_v1,
  get_thread_rwlock_locker_v1,
  get_thread_cond_locker_v1,
  get_thread_table_locker_v1,
  get_thread_file_name_locker_v1,
  get_thread_file_stream_locker_v1,
  get_thread_file_descriptor_locker_v1,
  unlock_mutex_v1,
  unlock_rwlock_v1,
  signal_cond_v1,
  broadcast_cond_v1,
  start_mutex_wait_v1,
  end_mutex_wait_v1,
  start_rwlock_rdwait_v1,
  end_rwlock_rdwait_v1,
  start_rwlock_wrwait_v1,
  end_rwlock_wrwait_v1,
  start_cond_wait_v1,
  end_cond_wait_v1,
  start_table_wait_v1,
  end_table_wait_v1,
  start_file_open_wait_v1,
  end_file_open_wait_v1,
  end_file_open_wait_and_bind_to_descriptor_v1,
  start_file_wait_v1,
  end_file_wait_v1
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
