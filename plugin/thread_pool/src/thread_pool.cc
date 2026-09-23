/* Copyright (c) 2011, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation. The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

/* Includes */

/*
  The following things are needed for most plugins:
  1) General data type definitions, included through my_inttypes.h
  2) DBUG support, included through my_dbug.h
  3) Performance Schema support, included through mysql/psi/psi.h and
     mysql/psi/mysql_thread.h
  4) General mutex and condition variable support, included through
     my_thread.h which is included via my_sys.h
     This includes the function set_timespec_nsec
  4) General thread handling support, included through my_thread.h which
     is included via my_sys.h
  5) MySQL configuration variable definition support, included through
     mysql/plugin.h
  6) Definition of the plugin(s), included through mysql/plugin.h
  7) Definitions of plugin services used, included through mysql/plugin.h
  8) Various things in mysys library, included through my_sys.h (in our case
     this is my_malloc and my_free)
  9) Access to compilation settings like NDEBUG, HAVE_PSI_INTERFACE,
     included through my_config.h and my_psi_config.h.

  These things are needed by almost every useful plugin and contains things
  that can be seen as part of the plugin interface.

  In addition the thread pool requires usage of MySQL Server internals, all
  these variables, functions and definitions needed here are defined by
  mysql/thread_pool_priv.h which is maintained in the community version to
  ensure that the MySQL Server developers knows what parts of the server
  that the thread pool plugin requires access to.
  This is included through mysql/thread_pool_priv.h

  Finally the thread pool has its own header file, thread_pool.h which
  needs to be included from all thread pool files.
*/

#include "my_config.h"

#include <errno.h>
#include <limits.h>
#include <mysql/components/library_mysys/my_system.h>  // my_num_vcpus
#include <mysql/components/services/log_builtins.h>
#include <mysql/plugin.h>
#include <mysql/psi/mysql_memory.h>
#include <mysql/psi/mysql_thread.h>
#include <mysql/thread_pool_priv.h>
#include <stdarg.h>
#include <stdio.h>  // Solaris header file bug.
#include <stdlib.h>
#include <time.h>
#include <algorithm>  // Depends on FILE (on Solaris).
#include <atomic>
#include <chrono>
#include <ios>
#include <numeric>  // std::accumulate
#include <thread>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_psi_config.h"
#include "my_sys.h"
#include "my_systime.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/service_thd_wait.h"
#include "mysqld_errmsg.h"
#include "scope_guard.h"
#include "sql/sql_class.h"
/* Poll methods */
#include "plugin/thread_pool/src/methods.h"
/* Thread pool includes */
#include "plugin/thread_pool/src/thread_pool.h"
/* Poll events */
#include "sql/events.h"

#include "plugin/thread_pool/src/option_usage.h"
#include "sql/debug_sync.h"
#include "sql/field.h"
#include "sql/mysqld.h"  // enum_server_operational_state
#include "sql/sql_thd_internal_api.h"
#include "sql/table.h"

/* Defines */

#define X_(x) #x << ":" << (x) << " "

/* Garbage value to have in thread_tp_group when not used */
constexpr auto GROUP_ID_GARBAGE = 0xFFFF;

#ifndef NDEBUG
static std::thread::id plugin_init_tid = std::this_thread::get_id();
#endif /* NDEBUG */

/**
  Number of threads spawned to process connection
  initialization & authentication permanently per
  thread group.
*/
static constexpr int INITIAL_CONNECT_THREADS_PER_GROUP = 1;

/**
 The lowest possible max number of connect threads for the server as a whole.
 The max number of connect threads allowed is controlled per group, but the
 limit per group multiplied by the number of groups must be greater than or
 equal to this limit. This to ensure that there is enough connect threads also
 when using few thread groups. Even with few thread groups a large number of
 connect threads may be required, e.g. when a burst of connection closures
 invokes the connection control plugin.

 Previously, a maximum of 5 connect threads was allowed per thread group. This
 resulted in an acceptable total number of connect threads for thread_pool_size
 >= 32, so it was desirable to allow a total of at least 160(=5x32) connect
 threads also for smaller thread_pool_size values.

 To achiveve this the below constant is divided by thread_pool_size and if the
 number is greater than LOWEST_MAX_CONNECT_THREADS_PER_GROUP(=5) (implies
 thread_pool_size < 32) it is rounded to the next higher integer, and this
 value is used as the max number of connect thread per group. Otherwise,
 (implies thread_pool_size >= 32) the constant value of 5 is used as before.
 */
static constexpr int LOWEST_MAX_CONNECT_THREADS = 160;  // 32 * 5

/**
 The lowest possible max number of connect threads for an individual thread
 group.
*/
static constexpr int LOWEST_MAX_CONNECT_THREADS_PER_GROUP = 5;

/**
  Maximum number of connect threads per group to handle
  connection authentication & initialization. Initialized in thd_pool_init()
  based on the value of LOWEST_TOTAL_MAX_CONNECT_THREADS and thread_pool_size.
*/
static int max_connect_threads_per_group = -1;

/**
  Time in seconds the connect handler threads (other
  than threads which are spawned initially) can be in idle
  state before it exits.
*/
static constexpr int CONNECT_THREAD_IDLE_TIMEOUT_SECS = 60;

/* Structures */
/* thread pool group low level */
struct tp_group_low_level_t;
/* thread pool client low level */
struct tp_client_low_level_t;

/*
  When we wake threads we have 3 scenarios:
  WAKE_IF_CONSUMER:
    This is used when we don't want to use reserved threads and
    also don't want to start a new thread. We only want to use
    a consumer thread.
  WAKE_IF_CREATED:
    This is used when we can also accept that a reserved thread is
    started, but we don't accept a new thread to be started. We
    still prefer consumer thread if there is one.
  WAKE_OR_CREATE_ONE:
    This is used when we can also accept to start a new thread.
    This is only used from stall check thread to avoid that we create
    new threads too fast which could lead to too many threads started.
*/

enum wake_level {
  WAKE_IF_CONSUMER = 0,
  WAKE_IF_CREATED = 1,
  WAKE_OR_CREATE_ONE = 2
};

struct tp_thread_create_control_ctx_t {
  std::atomic<uint64> total_query_threads;
  uint32 vcpu_count;
};
tp_thread_create_control_ctx_t tp_thread_create_control_ctx;

/* Function Prototypes */

/* Transaction queueing interface */

static void check_trans_queue_for_prio_kickups(tp_group_t *my_tp_group,
                                               bool new_10ms,
                                               Time_pt time_of_check);

static tp_client_low_level_t *pop_highest_priority_query(
    tp_group_t *my_tp_group);

/* Methods and definitions to handle KILL thread_id */

/* Send a KILL_FLAG on notify socket to invoke check for killed connection */
constexpr auto KILL_FLAG = 1;

static void insert_open_connection(tp_group_t *my_tp_group,
                                   tp_client_low_level_t *client_cntx);
static void remove_open_connection(tp_group_t *my_tp_group,
                                   tp_client_low_level_t *client_cntx);
static void remove_client_cntx(tp_group_t *my_tp_group,
                               tp_client_low_level_t *client_cntx);
static void remove_connection_from_queue(tp_group_t *my_tp_group,
                                         tp_client_low_level_t *client_cntx);
static inline void assign_thread_to_connection(tp_group_t *my_tp_group,
                                               tp_thread_t *my_thread_data);
static inline void unassign_thread_from_connection(tp_group_t *my_tp_group,
                                                   tp_thread_t *my_thread_data);
static inline bool set_connection_active(tp_client_low_level_t *client_cntx);
static inline bool set_connection_inactive_with_existing_lock(
    tp_group_t *my_tp_group, tp_thread_t *my_thread_data);
static bool handle_rearm_error(tp_group_t *tp_group,
                               tp_client_low_level_t *client_cntx);
static void handle_killed_connections(tp_group_t *my_tp_group, uint len,
                                      char *msg,
                                      tp_client_low_level_t **client_cntx_array,
                                      int *added_events);

/* GENERAL methods */
extern "C" void *tp_worker_thread_main(void *arg);
extern "C" void *tp_stall_check_thread_main(void *arg);
static void tp_full_cleanup(THD *thd, tp_thread_t *my_thread_data,
                            tp_client_low_level_t *client_cntx,
                            tp_group_t *my_tp_group, unsigned int errcode);
static void set_idle_timeout(tp_group_t *my_tp_group,
                             tp_client_low_level_t *client_cntx);
static bool create_thd_and_authenticate_conn(connection_context_t *,
                                             tp_group_t *, tp_thread_t *);
static void tp_create_connect_thread(tp_group_t *my_tp_group);
static void tp_wake_thread(tp_group_t *my_tp_group, wake_level level);
static inline bool is_query_ready_to_process(tp_group_t *my_tp_group);

/* Global Variables */

/*
  Atomic counters to keep track of number of connections currently being
  authenticated.
*/
std::atomic_ulong connections_entering_auth;
std::atomic_ulong connections_exiting_auth;

/*
  Read only globals (some can be changed as configuration changes). We
  ensure these variables are all in a separate cache line(s). This means
  all CPU caches can have this information and won't have to kick it out
  since it's almost never updated.
*/
alignas(128) static uint NANOSEC_PER_MILLI = 1000 * 1000;

// Number of normal thread groups (=thread_pool_size)
static uint tp_normal_groups = 0;
// Number of thread groups including admin thread group
static uint tp_groups = 0;
// Pointer to admin thread group
static tp_group_t *admin_thread_group = nullptr;

ulong thread_pool_size = DEF_POOL_SIZE;
ulong thread_pool_algorithm = HIGH_CONCURRENCY_ALGORITHM;
ulong thread_pool_stall_limit = DEF_STALL_LIMIT;
ulong thread_pool_prio_kickup_timer = DEF_PRIO_KICKUP_TIMER;
ulong thread_pool_max_unused_threads = DEF_MAX_UNUSED_THREADS;
std::uint32_t thread_pool_max_active_query_threads =
    DEF_MAX_ACTIVE_QUERY_THREADS;
std::uint32_t thread_pool_max_transactions_limit = DEF_MAX_TRANSACTIONS_LIMIT;
std::uint32_t thread_pool_max_transactions_limit_per_tg = 0;
bool thread_pool_dedicated_listeners = DEF_DEDICATED_LISTENERS;
std::uint32_t thread_pool_query_worker_threads = DEF_QUERY_WORKER_THREADS;
std::uint32_t thread_pool_query_threads_per_group = MIN_QUERY_THREADS_PER_GROUP;

static tp_thread_t tp_stall_check_thread = {
    my_thread_handle{},
    0,
    GROUP_ID_GARBAGE,
    {},
    false,
    false,
    nullptr,
    nullptr,
    false,
    Worker_thread_type::TIMER_WORKER_THREAD,
    0ULL,
    Thread_state::SC_CHECKING,
    false,
    0,
    {},
    0,
    {}};

static bool stall_check_initialized = false;
enum class Stall_checker_state {
  NOT_STARTED,
  RUNNING,
  TERMINATED
} stall_checker_state;

/* Ensure stall check mutexes have their own cache line */
alignas(128) static mysql_mutex_t tp_LOCK_stall_check;
alignas(128) static mysql_cond_t tp_COND_stall_check;
alignas(128) static tp_group_t tp_group_list[MAX_THREAD_GROUPS];

/*
  @def THR_PSI_backup
  Backup for the thread instrumentation.
  A physical thread (as in pthread) can execute either the thread pooling code
  or a logical thread (as in THD).
  Both are instrumented, but with different PSI_thread to have separate
  accounting of waits.
  THR_PSI_backup is used to save the instrumentation of the thread pooling code
  while executing the user job.
*/
thread_local PSI_thread *THR_PSI_backup = nullptr;

static PSI_mutex_key key_LOCK_group;
static PSI_mutex_key key_LOCK_connect;
static PSI_cond_key key_COND_group;
static PSI_cond_key key_COND_consumer;
static PSI_cond_key key_COND_reserve;
static PSI_cond_key key_COND_connect;

static PSI_mutex_key key_tp_LOCK_stall_check;
static PSI_cond_key key_tp_COND_stall_check;

static PSI_thread_key key_tp_worker_thread;
static PSI_thread_key key_tp_stall_check_thread;
static PSI_thread_key key_tp_one_connection;

static PSI_mutex_info all_tp_mutexes[] = {
    {&key_LOCK_group, "tp_group_t::LOCK_group", 0, 0, PSI_DOCUMENT_ME},
    {&key_tp_LOCK_stall_check, "tp_LOCK_stall_check", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&key_LOCK_connect, "tp_group_t::LOCK_connect", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME}};

static PSI_cond_info all_tp_conds[] = {
    {&key_COND_group, "tp_group_t::COND_group", 0, 0, PSI_DOCUMENT_ME},
    {&key_COND_consumer, "tp_group_t::COND_consumer", 0, 0, PSI_DOCUMENT_ME},
    {&key_COND_reserve, "tp_group_t::COND_reserve", 0, 0, PSI_DOCUMENT_ME},
    {&key_COND_connect, "tp_group_t::COND_connect", 0, 0, PSI_DOCUMENT_ME},
    {&key_tp_COND_stall_check, "tp_LOCK_stall_check", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME}};

static PSI_thread_info all_tp_threads[] = {
    {&key_tp_stall_check_thread, "tp_stall_check", "tp_stall",
     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&key_tp_one_connection, "tp_one_connection", "tp_connection",
     PSI_FLAG_USER | PSI_FLAG_NO_SEQNUM, 0, PSI_DOCUMENT_ME},
    {&key_tp_worker_thread, "tp_worker", "tp_worker", PSI_FLAG_NO_SEQNUM, 0,
     PSI_DOCUMENT_ME}};

#ifdef HAVE_EPOLL
static PSI_memory_info all_tp_memory[] = {
    {&key_memory_tp_group_low_level, "tp_group_low_level", 0, 0,
     PSI_DOCUMENT_ME},
    {&key_memory_tp_client_low_level, "tp_client_low_level", 0, 0,
     PSI_DOCUMENT_ME}};
#endif

/**
  Initialise keys for performance schema for thread pool

  @retval            true if failure, false if success
*/
static int init_tp_psi_keys() {
  const char *category = "thread_pool";
  int count;

  DBUG_TRACE;

  count = static_cast<int>(array_elements(all_tp_mutexes));
  mysql_mutex_register(category, all_tp_mutexes, count);

  count = static_cast<int>(array_elements(all_tp_conds));
  mysql_cond_register(category, all_tp_conds, count);

  count = static_cast<int>(array_elements(all_tp_threads));
  mysql_thread_register(category, all_tp_threads, count);

#ifdef HAVE_EPOLL
  count = static_cast<int>(array_elements(all_tp_memory));
  mysql_memory_register(category, all_tp_memory, count);
#endif

  return 0;
}

/**
  Set-up environment for user connection instead of thread pool
  environment

  @param thd                   THD object
*/
static void set_user_psi_env(THD *thd [[maybe_unused]]) {
#ifdef HAVE_PSI_THREAD_INTERFACE
  /*
    Until now, this thread is running under the thread pool instrumentation.
    Save this instrumentation, and bind to the user job instrumentation psi.
  */
  PSI_thread *tp_psi = PSI_THREAD_CALL(get_thread)();
  THR_PSI_backup = tp_psi;
  PSI_thread *user_psi = thd_get_psi(thd);
  PSI_THREAD_CALL(set_thread)(user_psi);
  // FIXME: PSI_THREAD_CALL(detect_telemetry)(user_psi);
#endif /* HAVE_PSI_THREAD_INTERFACE */
}

/**
  Set the environment back to using the thread pool environment again
  after completing query execution.
*/
static void set_tp_psi_env() {
#ifdef HAVE_PSI_THREAD_INTERFACE
  /*
    Until now, this thread is running under the user job instrumentation.
    Restore the thread pool instrumentation before giving up the job,
    so that from now on:
    - waits are properly counted against thread pool,
    - race conditions are not generated in the user job psi,
    which might be picked up by another thread.
  */
  PSI_THREAD_CALL(set_thread)(THR_PSI_backup);
#endif /* HAVE_PSI_THREAD_INTERFACE */
}

/**
  Destroy the performance schema instrumentation for the user job.

  @param  psi     PSI_thread instance.

  @note
  Note that the thread must run under the thread pool instrumentation
  when this function is called.
*/
static void clear_user_psi_env(PSI_thread *psi [[maybe_unused]]) {
#ifdef HAVE_PSI_THREAD_INTERFACE
  if (psi != nullptr) {
    PSI_THREAD_CALL(delete_thread)(psi);
  }
#endif /* HAVE_PSI_THREAD_INTERFACE */
}

/* Internal (private) Functions */

/**
  Returns true if MTL has been temporarily suspended due to too many
  long running transactions.
 */
static bool is_mtl_suspended(const tp_group_t *my_tp_group) {
  return my_tp_group->time_of_last_longrunning_trxs_check > BEGINNING_OF_EPOCH;
}

/**
  Returns the duration in which a thread group must have a normal number
  of long running transactions before MTL can be re-enabled.
*/
static Misec longrun_grace_period() {
  return std::clamp(15 * ATMC_longrun_trx_limit.load(std::memory_order_relaxed),
                    Misec(5 * 1000), Misec(30 * 1000));
}

/**
  Returns the minimum time which must elapse between each check for long
  running transactions.
*/
static Misec longrun_check_interval() {
  return ATMC_longrun_trx_limit.load(std::memory_order_relaxed) / 2;
}

/** Verify that LOCK_group is acquired for a given connection. */
void assert_LOCK_group_acquired(const tp_client_low_level_t *client_cntx
                                [[maybe_unused]]) {
  assert(client_cntx != nullptr);
  assert(client_cntx->group != nullptr);
  assert(tp_group_low_level_get_tp_group(client_cntx->group) != nullptr);
  mysql_mutex_assert_owner(
      &tp_group_low_level_get_tp_group(client_cntx->group)->LOCK_group);
}

/**
  Returns the configured thread_pool_max_transactions_limit_per_tg for this
  thread group. That is, 0 for the admin threadgroup and the value of
  thread_pool_max_transactions_limit_per_tg otherwise.
 */
std::uint32_t configured_max_transactions_limit_per_group(
    const tp_group_t *tg) {
  return tg == admin_thread_group ? 0
                                  : thread_pool_max_transactions_limit_per_tg;
}

/**
  Increment threads_active and mark this thread as having contributed
  to the count.
 */
static inline void inc_active_threads(tp_group_t *tp_group,
                                      tp_thread_t *my_thread_data) {
  DBUG_TRACE;
  assert(!my_thread_data->contributes_to_threads_active);
  ++tp_group->threads_active;
  my_thread_data->contributes_to_threads_active = true;
}

/**
  Decrement threads_active and mark this thread as not having contributed
  to the count.
 */
static inline void dec_active_threads(tp_group_t *tp_group,
                                      tp_thread_t *my_thread_data) {
  DBUG_TRACE;
  assert(my_thread_data->contributes_to_threads_active);
  --tp_group->threads_active;
  my_thread_data->contributes_to_threads_active = false;
}

/** Read threads active value with relaxed memory order. */
static inline std::uint32_t get_active_threads_relaxed(
    const tp_group_t *tp_group) {
  return tp_group->threads_active.load(std::memory_order_relaxed);
}

/**
  This method is used to atomically update the queries_executed variable.

  @param tp_group           Thread group data
*/
static inline void inc_queries_executed(tp_group_t *tp_group) {
  ++tp_group->stats.queries_executed;
}

/**
  Loops over all attached threads in a group and marks those that are
  talled. Counts total number of stalled threads and the number
  of threads which became stalled now, and stores this in tp_group_t.
 */
static inline void mark_and_count_stalled_threads(tp_group_t *my_tp_group,
                                                  Time_pt now) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&my_tp_group->LOCK_group);
  std::uint32_t threads_stalled = 0;
  std::uint32_t stalled_queries_detected = 0;
  auto stalled_threshold = now - get_stall_limit_us();
  for (uint i = 0; i < my_tp_group->max_thread_ids_in_group; ++i) {
    tp_thread_t *cur_thread = &(my_tp_group->group_threads[i]);

    // Ignore threads not associated with a connection
    if (cur_thread->client_low_level_cntx == nullptr ||
        cur_thread->time_of_attach == BEGINNING_OF_EPOCH)
      continue;
    /*
      This thread is currently processing a query.
      If beyond stall limit, set thread as stalled.
    */
    DBUG_LOG("tp_scv", "thread id " << cur_thread->numeric_id
                                    << " now executed "
                                    << std::chrono::duration_cast<Misec>(
                                           now - cur_thread->time_of_attach)
                                           .count()
                                    << " milliseconds");

    if (cur_thread->stalled) {
      ++threads_stalled;
      continue;
    }

    if (cur_thread->time_of_attach < stalled_threshold) {
      cur_thread->stalled = true;
      // We have detected a not-stalled -> stalled transition
      DBUG_LOG("tp", "thread id " << cur_thread->numeric_id << " now stalled");
      ++stalled_queries_detected;
      ++threads_stalled;
    }
  }
  // Update number of threads identified as stalled.
  my_tp_group->stalled_threads = threads_stalled;

  // Update statistics on stalled_queries_executed
  my_tp_group->stats.stalled_queries_executed += stalled_queries_detected;
}

/**
  Loops over all attached threads in a group and counts those that have been
  attached longer than thread_pool_longrun_trx_limit. If the count is greater
  than or equal to MTL for the group so that all available MTL for the group
  is consumed by long running transactions, the MTL is supended for
  the group. Supension ends when the number of long-running trans threads has
  remained below half of MTL for the group for longrun_grace_period() ms
  (if last_trx_check timepoint is earlier than the grace threshold
  timepoint passed in).
 */
inline void check_longrunning_transactions_and_adjust_MTL(
    tp_group_t *my_tp_group, Time_pt now, Time_pt gt) {
  mysql_mutex_assert_owner(&my_tp_group->LOCK_group);
  assert(thread_pool_max_transactions_limit > 0);

  auto longrunning_threshold =
      now - ATMC_longrun_trx_limit.load(std::memory_order_relaxed);

  // Copy on the stack which cannot be mutated by other threads.
  std::uint32_t txnlim = thread_pool_max_transactions_limit_per_tg;

  std::uint32_t longrunning = std::count_if(
      my_tp_group->group_threads,
      my_tp_group->group_threads + my_tp_group->max_thread_ids_in_group,
      [&](const tp_thread_t &t) {
        return t.time_of_attach > BEGINNING_OF_EPOCH &&
               t.time_of_attach < longrunning_threshold;
      });

  // MTL is enabled
  if (!is_mtl_suspended(my_tp_group)) {
    // Check if it should be enabled now
    if (longrunning >= txnlim) {
      LogErr(INFORMATION_LEVEL, ER_THREAD_POOL_MTL_DISABLE,
             my_tp_group->group_idx, longrunning);
      // Setting this to a value > BEGINNING_OF_EPOCH suspends MTL for the
      // group. By setting it to now we can infer how long it was since the
      // group last was observed to have too many long running transactions.
      my_tp_group->time_of_last_longrunning_trxs_check = now;
    }
    return;
  }

  // MTL is suspended and there are still too many longrunning trans
  // threads. Update the timepoint to reflect this.
  if (longrunning > (txnlim / 2)) {
    my_tp_group->time_of_last_longrunning_trxs_check = now;
    return;
  }

  // Number of longrunning trans threads is down to a normal level - check if
  // enough time has passed to re-enable MTL
  assert(longrunning <= (txnlim / 2));
  if (my_tp_group->time_of_last_longrunning_trxs_check < gt) {
    LogErr(INFORMATION_LEVEL, ER_THREAD_POOL_MTL_REENABLE,
           my_tp_group->group_idx, longrunning);
    my_tp_group->time_of_last_longrunning_trxs_check = BEGINNING_OF_EPOCH;
  }
}

/**
  Recalulates the number of max_active threads from
  tp_group_t::stalled_threads and tp_group_t::threads_user_request.
 */
static inline int update_max_active_threads(tp_group_t *my_tp_group) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&my_tp_group->LOCK_group);
  auto old_max_active_threads = my_tp_group->max_active_threads;
  auto threads_user_request = my_tp_group->threads_user_request;

  /*
    Allow user defined number of threads per group + allow stalled
    threads to also continue without being counted in group.
  */
  auto new_max_active_threads =
      threads_user_request + my_tp_group->stalled_threads;
  my_tp_group->max_active_threads = new_max_active_threads;

  DBUG_LOG("tp", X_(threads_user_request) << X_(my_tp_group->stalled_threads)
                                          << X_(new_max_active_threads)
                                          << X_(old_max_active_threads));

  if (new_max_active_threads != old_max_active_threads) {
    DBUG_PRINT("tp",
               ("New max_active_threads = %u, Old max_active_threads = %u",
                new_max_active_threads, old_max_active_threads));
  }
  return (new_max_active_threads - old_max_active_threads);
}

/**
  Clears the time_of_attch and stalled tp_thread_t member variables.
  If the thread was stalled, stalled_threads and max_active_threads
  are decremented.

  @param my_tp_group       Thread group data
  @param my_thread_data    Thread data
*/
static inline void init_process_count_time_unit(tp_group_t *my_tp_group,
                                                tp_thread_t *my_thread_data) {
  mysql_mutex_assert_owner(&my_tp_group->LOCK_group);
  if (my_thread_data->stalled) {
    --my_tp_group->stalled_threads;
    --my_tp_group->max_active_threads;
    my_thread_data->stalled = false;
  }
  my_thread_data->time_of_attach = BEGINNING_OF_EPOCH;
}

/**
  This function is called when the connection has been broken or an
  error requires us to close the connection.

  @param my_tp_group The Threadpool thread group this connection belongs to
  @param client_cntx Thread Pool connection representation (or nullptr)
                     if not created yet
  @param thd         THD of connection
  @param end         true -> call end_connection()
  @param errcode     Parameter passed to close_connection
*/
static void tp_thd_cleanup(tp_group_t *my_tp_group,
                           tp_client_low_level_t *client_cntx, THD *thd,
                           bool end, unsigned int errcode) {
  DBUG_TRACE;
  DBUG_PRINT("tp_enter",
             ("Thread group id: %d, connection id: %lu", my_tp_group->group_idx,
              thd_get_thread_id(pointer_cast<MYSQL_THD>(thd))));

  // Run THD cleanup under the user job instrumentation
  set_user_psi_env(thd);

  PSI_thread *psi [[maybe_unused]] = nullptr;

  /*
    Save PSI context of user instrumentation. User job instrumentation is
    cleared after deleting thd.
  */
  psi = thd_get_psi(thd);

  /*
    Session cleanup requires that current_thd is valid and equivalent to
    the session being killed. This is necessary because it might involve
    rolling back a transaction, closing tables and a whole lot of other
    operations that might use current_thd. Also, if not set, current_thd
    at this point could probably point to a already dead session.
  */

  thd_lock_data(thd);
  thd_store_globals(thd);
  thd_set_scheduler_data(thd, nullptr);
  thd_unlock_data(thd);
  thd_set_net_read_write(thd, 0);

#ifdef HAVE_PSI_THREAD_INTERFACE
  if (psi != nullptr) {
    /*
      Cleanly abort the telemetry session,
      before end_connection() clears any remaining THD slot.
    */
    PSI_THREAD_CALL(abort_telemetry)(psi);
  }
#endif /* HAVE_PSI_THREAD_INTERFACE */

  if (end) {
    /*
      end_connection()
      - "This mainly updates status variables"
    */
    end_connection(thd);
  }

  close_connection(thd, errcode, false, !end);

#ifdef HAVE_PSI_THREAD_INTERFACE
  if (psi != nullptr) {
    PSI_THREAD_CALL(abort_telemetry)(psi);
    PSI_THREAD_CALL(set_thread_THD(psi, nullptr));
  }
#endif /* HAVE_PSI_THREAD_INTERFACE */

  if (client_cntx) {
    client_cntx->thd = nullptr;
  }
  assert(current_thd == thd);
  destroy_thd(thd, false /* clear_pfs_instr */);
  assert(current_thd != thd);
  assert(current_thd == nullptr);
  dec_connection_count();

  // Restore the thread pool instrumentation.
  set_tp_psi_env();
  // Clear user job instrumentation.
  clear_user_psi_env(psi);

  if (client_cntx) {
    mysql_mutex_lock(&my_tp_group->LOCK_group);

    // If the connection attempt fails before we have managed
    // to create a valid client context for the connection, the
    // connection stats have not yet been incremented.
    assert(my_tp_group->stats.connection_count > 0);
    my_tp_group->stats.connection_count--;
    my_tp_group->stats.connections_closed++;

#ifdef ENABLED_DEBUG_SYNC
    client_cntx->debug_sync_unavailable = 1;
#endif /* ENABLED_DEBUG_SYNC */
    mysql_mutex_unlock(&my_tp_group->LOCK_group);
  }
}

/**
  Support method to check if query of any priority is available

  @param my_tp_group        Thread group data

  @retval                   true if query available in queue
  */
static inline bool is_query_available(tp_group_t *my_tp_group) {
  return !my_tp_group->queued_queries.is_empty() ||
         !my_tp_group->queued_trans.is_empty();
}

/** Returns a pointer to the query(connection) queue which has
    the next (highest priority) query(connection) */
static inline Query_list *next_query_queue(tp_group_t *my_tp_group) {
  if (!my_tp_group->queued_queries.is_empty())
    return &my_tp_group->queued_queries;
  return &my_tp_group->queued_trans;
}

/** Returns the next (highest priority) query(connection). */
static inline tp_client_low_level_t *next_queued_query(
    tp_group_t *my_tp_group) {
  return next_query_queue(my_tp_group)->front();
}

/**
   Check if worker thread needs to bound until transaction commit time.

   @param thd THD object

   @retval true if the thread needs to be bound else false.
*/
static inline bool tp_bind_to_current_worker_thread(THD *thd) {
  return thd_is_transaction_active(thd);
}

/**
  Attach/associate the connection with the OS thread, for command processing.

  @param thd               THD object
  @param my_thread_data    Thread data
  @param my_tp_group       Thread group data

  @retval                  true if failure, false to indicate success
*/
static inline bool thread_attach(THD *thd, tp_thread_t *my_thread_data,
                                 tp_group_t *my_tp_group [[maybe_unused]]) {
  DBUG_TRACE;
#ifndef NDEBUG
  {
    tp_client_low_level_t *client_cntx = my_thread_data->client_low_level_cntx;
    /*
      When we attach the thread for a connection for the first time,
      we know that there is no session value set yet. Thus
      the initial setting of set_explain to false is OK.

      Since this thread is reused by many connections it is necessary to
      set up the DBUG environment with the settings from this connection if
      it has been used previously.
    */
    if (client_cntx->set_explain) {
      DBUG_SET(client_cntx->dbug_explain);
    }
  }
#endif
  /*
    Set process count to zero, this will ensure that the stall check thread
    can keep track of our progress in executing the query. As support to the
    stall check thread we also bind the thread and the client low level
    context together.

    We need to know the start of the stack so that we could check for
    stack overruns. We need to setup performance schema accounting
    to user connection and we need to increase number of active
    threads to keep our credit system working properly.

    Finally we set-up the thd object properly for execution in this thread.
    This method also performs some cleanup on failures.
  */
  thd_set_thread_stack(thd, my_thread_data->stack_start);
  set_user_psi_env(thd);
  thd_store_globals(thd);
  thd_clear_errors(thd);
  return false;
}

/**
  Detach/disassociate the connection with the OS thread.

  @param thd              THD object
  @param my_thread_data   Thread data
  @param my_tp_group      Thread group data
*/
static void thread_detach(THD *thd,
                          tp_thread_t *my_thread_data [[maybe_unused]],
                          tp_group_t *my_tp_group [[maybe_unused]]) {
  DBUG_TRACE;
  DBUG_PRINT(
      "tp",
      ("Detach connection id %lu from Thread group id %d with client_cntx %p",
       my_thread_data->client_low_level_cntx->connection_id,
       my_tp_group->group_idx, my_thread_data->client_low_level_cntx));

  /*
    Set the process count to indicate we're no longer executing a query to
    avoid stall check thread tracking.

    Remove an active thread from thread group algorithm.

    Remove pointer to thread specific variable THR_KEY_mysys to ensure that
    kill logic won't use the next connection's mutexes to protect access to
    our thd object.
  */
  // Run under tp instrumentation as following statements update group status.
  set_tp_psi_env();

  // Restore user job instrumentation.
  set_user_psi_env(thd);
  // Indicate we're reading again (part of net object).
  thd_set_net_read_write(thd, 1);
  /*
    The last thing we do here on the THD and client context is an
    operation that requires a mutex, this mutex will also be a
    memory barrier for the THD and client context object to ensure
    that the next thread that operates on these object see our
    updates to those objects.
  */
  thd_set_not_killable(thd);

#ifndef NDEBUG
  {
    tp_client_low_level_t *client_cntx = my_thread_data->client_low_level_cntx;
    /*
      If during the session @@session.dbug was assigned, the
      dbug options/state has been pushed. Check if this is the
      case, to be able to restore the state when we attach this
      logical connection to a physical thread.
    */
    if (_db_is_pushed_()) {
      client_cntx->set_explain = true;
      if (DBUG_EXPLAIN(client_cntx->dbug_explain,
                       sizeof(client_cntx->dbug_explain)))
        LogErr(ERROR_LEVEL, ER_THREAD_POOL_BUFFER_TOO_SMALL, "thd_scheduler",
               "DBUG_EXPLAIN");
    }
    /* DBUG_POP is a no-op if nothing pushed */
    DBUG_POP();
    /*
      Just to ensure that a memory barrier is placed here to ensure that
      the update of the client context is seen by other threads when they
      take over this object.
    */
    thd_lock_data((MYSQL_THD)thd);
    thd_unlock_data((MYSQL_THD)thd);
  }
#endif
  set_tp_psi_env(); /* Set perfschema accounting to thread pool */
}

/**
  Executes a single command/statement.
  Can be called multiple times for a single event.

  @param thd Thread context
  @param my_tp_group Thread pool thread group

  @returns true if error, false otherwise
*/
static inline bool tp_execute_command(THD *thd, tp_group_t *my_tp_group,
                                      tp_thread_t *my_thread_data) {
  auto time_of_enter = tp_now();
  bool admingrp = (my_tp_group == admin_thread_group);

#ifndef NDEBUG
  // Allow tests to expect a particular transaction_delay mode for a statement,
  // and force an assert if the expectation is not met.
  auto tos = [](TDM mode) {
    switch (mode) {
      case TDM::ANY:
        return "TDM::ANY";
      case TDM::ON:
        return "TDM::ON";
      case TDM::ZERO:
        return "TDM::ZERO";
      case TDM::NOT_AVAILABLE:
        return "TDM::NOT_AVAILABLE";
    }
    return "<Unknown TDM value>";
  };
  TDM actual = TDM::ANY;
  auto expect = [&](TDM expected) {
    if (expected != actual) {
      std::ostringstream ss;
      ss << "Assertion failure: expected transaction delay mode: "
         << tos(expected) << " but actual TDM is " << tos(actual)
         << ", server_status:" << thd->server_status;
      LogErr(ERROR_LEVEL, ER_CONDITIONAL_DEBUG, ss.str().c_str());
      assert(false);
    }
  };
#endif /* NDEBUG */

  if (!admingrp) {
    if (!thd_in_active_multi_stmt_transaction(thd)) {
      auto txn_delay = get_transaction_delay();
      if (txn_delay != txn_delay.zero()) {
        DBUG_PRINT("tp_throttle",
                   ("tg:%u thd->thread_id():%u "
                    "thd:%p "
                    "Enter delay sleep for %lld ms",
                    my_tp_group->group_idx, thd->thread_id(), thd,
                    static_cast<long long>(txn_delay.count())));

        mysql_mutex_lock(&my_tp_group->LOCK_group);
        my_thread_data->client_low_level_cntx->wait_type = THD_WAIT_TRX_DELAY;
        mysql_mutex_unlock(&my_tp_group->LOCK_group);
        std::this_thread::sleep_for(txn_delay);
        mysql_mutex_lock(&my_tp_group->LOCK_group);
        my_thread_data->client_low_level_cntx->wait_type = THD_WAIT_NONE;
        ++my_tp_group->stats.wait_counts[THD_WAIT_TRX_DELAY];
        mysql_mutex_unlock(&my_tp_group->LOCK_group);
#ifndef NDEBUG
        actual = TDM::ON;
      } else {
        actual = TDM::ZERO;
      }
    } else {
      actual = TDM::NOT_AVAILABLE;
    }
  }  // if (!admingrp)
#else
      }  // if (txn_delay
    }    // if (!thd_in_active...
  }      // if (!admingrp)
#endif /* !NDEBUG */

  DBUG_PRINT("tp", ("Process a command/statement in thread group id %d for "
                    "connection id %lu",
                    my_tp_group->group_idx,
                    thd_get_thread_id(pointer_cast<MYSQL_THD>(thd))));

  inc_queries_executed(my_tp_group);
  // Process one request. do_command() returns true if connection shut down.

  // reading_or_writing should be zero when doing the command since
  // we aren't reading at the time we're calling do_command.
  thd_set_net_read_write(thd, 0);

  // We need to ensure the connection isn't broken both before and
  // after we start the query.
  if (unlikely(!thd_connection_alive(thd) || do_command(thd) ||
               !thd_connection_alive(thd))) {
    return true;
  }
#ifndef NDEBUG
  if (!admingrp) {
    // Used to ensure we get the expected transaction delay behavior. Not done
    // for admin connections so that an admin connection can safely manipulate
    // the debug variable without triggering the asserts.

    DBUG_PRINT("tp", ("Transaction delay mode used:%s expected:%s \n",
                      tos(actual), tos(expected_from_debug_flag)));
    if (expected_from_debug_flag != TDM::ANY) {
      expect(expected_from_debug_flag);
    }
  }
#endif /* NDEBUG */
  my_thread_data->command_count.fetch_add(1, std::memory_order_relaxed);
  unsafe_atomic_add(&my_thread_data->acc_command_time,
                    tp_now() - time_of_enter);
  return false;
}

/**
  Handles the actual execution of an event by attachin the THD to the thread
  and tp_execute_command() for all commnands/statement which can be read from
  the socket at this time.
  It handles cleanup in failure cases.
  It can be called both from low level and from high-level methods.

  @param my_thread_data    Thread data

  @retval                  true if failure, false to indicate success
*/
static inline bool tp_process_event(tp_group_t *my_tp_group,
                                    tp_thread_t *my_thread_data) {
  DBUG_TRACE;
  auto time_of_enter = tp_now();
  tp_client_low_level_t *client_cntx = my_thread_data->client_low_level_cntx;
  if (is_tp_shutdown() || client_cntx == nullptr) return 1;

  assert(my_thread_data->thread_tp_group < MAX_THREAD_GROUPS);
  THD *thd = client_cntx->thd;

  DBUG_EXECUTE_IF("stop_waiter_flush_g0", {
    DBUG_LOG("tp_kill", "Setting stop_in_waiter_flush to true");
    assert(tp_groups == 2);  // All user connections must be in group 0.
    tp_group_list[0].stop_in_waiter_flush = true;
  });
  DBUG_EXECUTE_IF("start_waiter_flush_g0", {
    DBUG_LOG("tp_kill",
             "Setting stop_in_waiter_flush to false and waking waiting "
             "threads");
    assert(tp_groups == 2);  // All user connections must be in group 0.
    tp_group_list[0].stop_in_waiter_flush = false;
  });

  std::uint32_t txnlim =
      configured_max_transactions_limit_per_group(my_tp_group);

  mysql_mutex_assert_not_owner(&my_tp_group->LOCK_group);
  DBUG_PRINT("tp_enter",
             ("Process an event in thread group %u for connection id %lu,"
              " using client_cntx %p, txnlim:%" PRIu32 " admingrp:%d",
              my_tp_group->group_idx, client_cntx->connection_id, client_cntx,
              txnlim, (my_tp_group == admin_thread_group)));

  unsigned int errcode = 0;
  auto err_grd = create_scope_guard([&]() {
    tp_full_cleanup(thd, my_thread_data, client_cntx, my_tp_group, errcode);
  });
  if (unlikely(thread_attach(thd, my_thread_data, my_tp_group))) {
    return true;
  }

  // Increment trxn threads count if configured
  // thread_pool_max_transactions_limit is set for the thread group.
  if (txnlim != 0) {
    my_tp_group->trxn_threads++;
    DBUG_PRINT("tp", ("txnlim:%u my_tp_group->trxn_threads:%u :%d",
                      thread_pool_max_transactions_limit_per_tg,
                      my_tp_group->trxn_threads.load(), __LINE__));
  }
  auto trxn_threads_decrement_guard = create_scope_guard([&]() {
    if (txnlim != 0) {
      my_tp_group->trxn_threads--;
      DBUG_PRINT("tp", ("txnlim:%u my_tp_group->trxn_threads:%u :%d", txnlim,
                        my_tp_group->trxn_threads.load(), __LINE__));
    }
  });

  unsafe_atomic_add(&client_cntx->management_time,
                    (tp_now() - client_cntx->time_of_event_arrival.load(
                                    std::memory_order_relaxed)));

  // Execute commands/statments as long as there is data to read from the
  // connection. If txnlimit > 0, stay in loop and block on read until the
  // transaction ends.
  do {
    if (tp_execute_command(thd, my_tp_group, my_thread_data)) {
      set_tp_psi_env();
      return true;
    }

    // Allow worker thread to handle next network event from the same client
    // if the current transaction has not ended.
    if (!thd_connection_has_data(thd)) {
      if (!thread_pool_max_transactions_limit ||
          !tp_bind_to_current_worker_thread(thd))
        break;
    }
    /*
      There is cached data already read off of the socket,
      so we can't wait safely for new events on socket, we
      need to process the next event immediately. This can
      happen e.g. for SSL implementations of vio.
    */
    DBUG_PRINT("tp", ("vio still has data, execute another query"));
  } while (true);

  DBUG_EXECUTE_IF("make_query_very_long", {
    if (my_tp_group->waiting_thread != my_thread_data) my_sleep(6000000);
  });
  thread_detach(thd, my_thread_data, my_tp_group);
  my_thread_data->event_count.fetch_add(1, std::memory_order_relaxed);
  unsafe_atomic_add(&my_thread_data->acc_event_time, tp_now() - time_of_enter);
#if defined(ENABLED_DEBUG_SYNC)
  /*
    To be able to test a special tricky case where a KILL command
    gets handled after deactivating the connection but before we have
    rearmed the connection. In this case if we kill the connection in
    this state we might crash when rearming or might rearm a different
    connection. So to test this we introduce a new wait state here
    which a test program can use to test this particular test case.
    The test case won't work if we own the waiting_thread since the
    stall check thread will check if client context is NULL (which it
    is at this point, so thus no query is active).
  */
  if (my_tp_group->waiting_thread != my_thread_data &&
      !client_cntx->debug_sync_unavailable) {
    DEBUG_SYNC(thd, "after_deactivate");
  }
#endif

  /*
    This is the limit at which we can no longer use the thd object.
    As soon as we rearm the socket, another thread can pick up a
    query on the socket and start executing it.

    The system call to rearm also ensures a memory barrier for the thd
    object.
  */
  if (unlikely(tp_client_low_level_rearm(client_cntx, my_tp_group) != 0)) {
    if (handle_rearm_error(my_tp_group, client_cntx)) {
      errcode = ER_ERROR_ON_READ;
      return true;
    }
    err_grd.release();
    return true;
  }
  client_cntx->conn_state = Connection_state::ARMED;
  err_grd.release();
  return false;
}

/**
  Create a new worker thread

  @param cur_group        Thread group data
  @param new_threads      Number of threads to crete
  @param worker_type      Role of worker thread

  @retval                 Number of threads actually started
*/
static int tp_create_worker_threads(tp_group_t *cur_group, uint new_threads,
                                    Worker_thread_type worker_type) {
  mysql_mutex_assert_owner(&cur_group->LOCK_group);
  uint new_count = 0;
  uint i;
  int error;
  tp_thread_t *cur_thread = nullptr;
  DBUG_TRACE;
  DBUG_PRINT("tp", ("Attempting to create %u %s in group %u", new_threads,
                    worker_thread_type_str(worker_type), cur_group->group_idx));

  std::uint32_t txnlim = configured_max_transactions_limit_per_group(cur_group);
  /* Start threads to deal with a group of sockets/clients */
  for (i = 0; (i < MAX_THREADS_PER_GROUP) && (new_count < new_threads); i++) {
    cur_thread = &(cur_group->group_threads[i]);
    if (cur_thread->thread_handle.thread == null_thread_initializer &&
        cur_thread->thread_tp_group == GROUP_ID_GARBAGE) {
      /* Found free thread entry */
      assert(cur_group->group_idx < MAX_THREAD_GROUPS);
      // Limit query threads count if thread_max_transactions_limit_per_tg for
      // the thread group is not 0 (disabled)
      if (txnlim != 0) {
        mysql_mutex_lock(&cur_group->LOCK_query_threads_count);
        if (worker_type == Worker_thread_type::QUERY_WORKER_THREAD) {
          // Allow one more for the waiting thread in case all trxn
          // threads are tied to long-running transactions. If we don't do this
          // we will not detect credit increases due to MTL changes and kill
          // requests. This must matche the // condition used in
          // is_credit_available() + one extra thread.
          if (cur_group->query_threads_count <= txnlim + 1 ||
              // Need to allow unlimited thread creation when mtl is suspended
              is_mtl_suspended(cur_group)) {
            thread_pool_query_worker_threads++;
            cur_group->query_threads_count++;
          } else {
            mysql_mutex_unlock(&cur_group->LOCK_query_threads_count);
            DBUG_PRINT("tp", ("Refusing to create another %s due to "
                              "txnlim:%u "
                              "cur_group->query_threads_count:%u :%d",
                              worker_thread_type_str(worker_type), txnlim,
                              cur_group->query_threads_count.load(), __LINE__));
            return 0;
          }
        }
        mysql_mutex_unlock(&cur_group->LOCK_query_threads_count);
      } else {
        // No need to acquire lock here as counts are maintained for stats
        if (worker_type == Worker_thread_type::QUERY_WORKER_THREAD) {
          thread_pool_query_worker_threads++;
          cur_group->query_threads_count++;
        }
      }
      cur_thread->thread_tp_group = cur_group->group_idx;
      cur_thread->client_low_level_cntx = nullptr;
      cur_group->stats.threads_created++;
      cur_thread->worker_type = worker_type;
      cur_thread->is_connection_handler_thread =
          (worker_type == Worker_thread_type::CONNECTION_HANDLER_WORKER_THREAD);
      set_state(cur_thread, (cur_thread->is_connection_handler_thread
                                 ? Thread_state::CH_PROCESSING
                                 : Thread_state::MANAGING));
      error = mysql_thread_create(
          key_tp_worker_thread, &(cur_thread->thread_handle),
          get_connection_attrib(), tp_worker_thread_main, cur_thread);
      if (!error) {
        cur_group->max_thread_ids_in_group =
            std::max(i + 1, cur_group->max_thread_ids_in_group);
        DBUG_PRINT("tp",
                   ("Creating new worker thread id %d in Thread group id %d", i,
                    cur_group->group_idx));
        new_count++;
      } else {
        LogErr(WARNING_LEVEL, ER_THREAD_POOL_CREATE_THREAD_FAILED, error,
               socket_errno);
        if (txnlim != 0) {
          mysql_mutex_lock(&cur_group->LOCK_query_threads_count);
          thread_pool_query_worker_threads--;
          cur_group->query_threads_count--;
          DBUG_PRINT("tp",
                     ("txnlim:%u "
                      "cur_group->query_threads_count:%u :%d",
                      txnlim, cur_group->query_threads_count.load(), __LINE__));
          mysql_mutex_unlock(&cur_group->LOCK_query_threads_count);
        } else {
          thread_pool_query_worker_threads--;
          cur_group->query_threads_count--;
        }
        break;
      }
    }
  }
  return (int)new_count;
}

/**
  Create a new connect handler provided the number of connect handler threads in
  doesn't exceed the upper limit of allowed connect handler threads.

  @param  my_tp_group   Thread group data.
*/
static void tp_create_connect_thread(tp_group_t *my_tp_group) {
  DBUG_TRACE;
  mysql_mutex_assert_not_owner(&my_tp_group->LOCK_connect);
  mysql_mutex_assert_not_owner(&my_tp_group->LOCK_group);
  DBUG_EXECUTE_IF("simulate_max_connect_threads_per_group", {
    my_tp_group->connect_threads = max_connect_threads_per_group;
  });

  // LOCK_group must be acquired before evaluating connect_threads
  // even if it is atomic, because there needs to be a critical
  // region from the evaluation to the increment below.
  mysql_mutex_lock(&my_tp_group->LOCK_group);
  auto lock_guard = create_scope_guard(
      [&]() { mysql_mutex_unlock(&my_tp_group->LOCK_group); });
  if (my_tp_group->connect_threads >= max_connect_threads_per_group) return;

  // Create a new thread
  DBUG_PRINT("tp", ("Create new connect handler thread"));
  if (tp_create_worker_threads(
          my_tp_group, 1,
          Worker_thread_type::CONNECTION_HANDLER_WORKER_THREAD) == 1) {
    my_tp_group->connect_threads++;
    return;
  }
  LogErr(WARNING_LEVEL, ER_THREAD_POOL_FAILED_TO_CREATE_CONNECT_HANDLER_THD);
}

static inline ulonglong tp_thread_create_ctrl_interval() {
  uint64 total_threads_in_tp = tp_thread_create_control_ctx.total_query_threads;
  if (total_threads_in_tp < (3 * tp_thread_create_control_ctx.vcpu_count))
    return 0;
  if (total_threads_in_tp < (5 * tp_thread_create_control_ctx.vcpu_count))
    return 50;
  return 100;
}

/**
  Wake one of the worker threads that is waiting to process clients commands.
  Or create a new thread to help process client commands.

  @param my_tp_group    The thread pool group context to active a thread
  @param level          Ability to wake up one thread or create a new one

  @note
    This will:
      - Wake a consumer or reserve thread
      - Create a thread if there are no "idle" threads AND level is
        WAKE_OR_CREATE_ONE

    The purpose of consumer and reserve threads is a very simple manner of
    a queue. We don't need a full purpose queue, it's enough to keep track
    of which thread was last used. The purpose of this is to ensure that we
    always use the thread with most of its stack in the CPU cache. Given
    that the CPU cache changes very rapidly it makes no sense to keep track
    of more than the latest thread used. However to keep track of this one
    thread gives a significant boost of 5-10% in performance in experiments.
*/
static void tp_wake_thread(tp_group_t *my_tp_group, wake_level level) {
  DBUG_TRACE;
  DBUG_PRINT("tp_enter", ("wake thread in Thread group id %d, level %d",
                          my_tp_group->group_idx, level));
  mysql_mutex_assert_owner(&my_tp_group->LOCK_group);
#ifndef NDEBUG
  bool skip_thread_wakeup = false;
  DBUG_EXECUTE_IF("test_throttle_thread_create", {
    skip_thread_wakeup = true;
    my_tp_group->threads_active = 1;
    level = WAKE_OR_CREATE_ONE;
    tp_thread_create_control_ctx.vcpu_count = 1;
  });
  if (!skip_thread_wakeup && my_tp_group->threads_for_consumer > 0) {
#else
  if (my_tp_group->threads_for_consumer > 0) {
#endif  // NDEBUG
    /* Get another thread from the consumer group */
    DBUG_PRINT("tp", ("Wake consumer thread"));
    mysql_cond_signal(&my_tp_group->COND_consumer);
  } else {
    if (level == WAKE_IF_CREATED || level == WAKE_OR_CREATE_ONE) {
#ifndef NDEBUG
      if (!skip_thread_wakeup && my_tp_group->threads_for_reserve > 0) {
#else
      if (my_tp_group->threads_for_reserve > 0) {
#endif  // NDEBUG
        /* Get another thread from the reserve group */
        DBUG_PRINT("tp", ("Wake reserve thread"));
        mysql_cond_signal(&my_tp_group->COND_reserve);
      } else {
        if (level == WAKE_OR_CREATE_ONE) {
          if (my_tp_group->num_query_threads >
                  1 + my_tp_group->stats.connection_count ||
              my_tp_group->num_query_threads.load() > MAX_THREADS_PER_GROUP)
            return;
          if (my_tp_group->threads_active == 0 ||
              (tp_thread_create_control_ctx.total_query_threads <
               (3 * tp_thread_create_control_ctx.vcpu_count))) {
            if (tp_create_worker_threads(
                    my_tp_group, 1, Worker_thread_type::QUERY_WORKER_THREAD) ==
                1) {
              my_tp_group->num_query_threads++;
              tp_thread_create_control_ctx.total_query_threads++;
              my_tp_group->last_thread_creation_time = my_micro_time();
            }
          } else {
            // Rate limit thread creation.
            ulonglong now = my_micro_time();
            ulonglong time_since_last_thr_created =
                (now - my_tp_group->last_thread_creation_time);
            if (time_since_last_thr_created >
                tp_thread_create_ctrl_interval()) {
              /* Create a new thread */
              DBUG_PRINT("tp", ("Create new thread"));
              if (tp_create_worker_threads(
                      my_tp_group, 1,
                      Worker_thread_type::QUERY_WORKER_THREAD) == 1) {
                my_tp_group->num_query_threads++;
                tp_thread_create_control_ctx.total_query_threads++;
                my_tp_group->last_thread_creation_time = my_micro_time();
              }
            }
          }
        }
      }
    }
  }
}

/**
  Support method to check if credit is available

  @param my_tp_group        Thread group data

  @retval                   true if credit available, otherwise false
*/
static inline bool is_credit_available(tp_group_t *my_tp_group) {
  DBUG_TRACE;
  bool res;
  std::uint32_t txnlim =
      configured_max_transactions_limit_per_group(my_tp_group);
  if (txnlim != 0)
    res =
        (my_tp_group->trxn_threads <= txnlim || is_mtl_suspended(my_tp_group));
  else if (thread_pool_max_active_query_threads)
    res = (my_tp_group->threads_active < thread_pool_max_active_query_threads);
  else
    res = (my_tp_group->threads_active <= my_tp_group->max_active_threads);

  DBUG_LOG("tp_query_ready", X_(res));
  return res;
}

/**
  Support method to check if high prio query is available

  @param my_tp_group         Thread group data

  @retval                    true if high prio query available
*/
static inline bool is_high_prio_query_available(tp_group_t *my_tp_group) {
  return !my_tp_group->queued_queries.is_empty();
}

/**
  Support method to check if there is any query available and there is
  credit available to execute it.

  @param my_tp_group        Thread group data

  @retval                   true if query available to execute
*/
static inline bool is_query_ready_to_process(tp_group_t *my_tp_group) {
  DBUG_TRACE;
  DBUG_LOG("tp_enter",
           "Thread group id "
               << my_tp_group->group_idx << ", Num queries queued"
               << my_tp_group->queued_queries.elements()
               << ", Num trans queued " << my_tp_group->queued_trans.elements()
               << ", max_active_threads " << my_tp_group->max_active_threads);

  tp_client_low_level_t *next = next_queued_query(my_tp_group);
  if (next == nullptr) {
    DBUG_LOG("tp_query_ready", "No queued query");
    return false;
  }

  if (is_credit_available(my_tp_group)) {
    DBUG_PRINT("tp", ("Credit is avalable"));
    return true;
  }
  DBUG_PRINT("tp_query_ready", ("Credit is NOT avalable"));
  next->conn_state = Connection_state::WAITING_FOR_CREDIT;
  return false;
}

/**
  Support method to store query in prio queue

  @param my_tp_group        Thread group data
  @param client_cntx        Low level client context
*/
static inline void queue_query(tp_group_t *my_tp_group,
                               tp_client_low_level_t *client_cntx) {
  DBUG_TRACE;
  assert(client_cntx);

  DBUG_PRINT("tp", ("Queueing query from connection id %lu",
                    client_cntx->connection_id));
  if (thd_is_transaction_active(client_cntx->thd) ||
      thread_pool_prio_kickup_timer == 0 ||
      is_high_priority_connection(client_cntx->thd)) {
    my_tp_group->queued_queries.push_back(client_cntx);
  } else {
    my_tp_group->queued_trans.push_back(client_cntx);
  }
  client_cntx->is_queued = true;
  client_cntx->conn_state = Connection_state::QUEUED;
}

/**
  Support method to count number of queued queries

  @param my_tp_group        Thread group data

  @retval                   Number of waiting queries
*/
static inline uint count_waiting_queries(tp_group_t *my_tp_group) {
  return (my_tp_group->queued_queries.elements() +
          my_tp_group->queued_trans.elements());
}

/**
  Become waiting thread

  @param my_tp_group        Thread group data
  @param my_thread_data     Thread data
  @param wait               Should we wait for events or return immediately
*/
static void handle_waiting_thread(tp_group_t *my_tp_group,
                                  tp_thread_t *my_thread_data, bool wait) {
  tp_client_low_level_t *next_events[MAX_EVENTS_PER_WAIT_CALL];
  uint events_ready;
  uint i;
  DBUG_TRACE;
  mysql_mutex_assert_owner(&my_tp_group->LOCK_group);

  /* This thread will be the waiting thread. */
  my_tp_group->waiting_thread = my_thread_data;

  /* Update statistics on becoming listener thread */
  my_tp_group->stats.become_listen_thread++;

  set_state(my_thread_data, Thread_state::POLLING);

  /*
    my_tp_group->waiting_thread is the "lock state" that protects
    my_tp_group->events_index_produce from having multiple threads
    accessing it. Thus we don't need to hold a pthread lock any longer.
    Unlock the group before I wait for new tp_events
  */
  mysql_mutex_unlock(&my_tp_group->LOCK_group);

  /*
    Wait for file descriptors to have something of interest
    NOTE: This call may process an event if a single fd has IO ready
    this may deadlock if we don't have correct "going to sleep"
    callbacks, this optimisation is for situations with only a single
    connection actively pursuing queries. If there is a query
    available when we arrive here it isn't likely to be this situation
    we're in.
  */
  events_ready = tp_group_low_level_wait_for_events(
      my_tp_group->group_low_level_cntx, my_tp_group, my_thread_data,
      next_events, wait);

  mysql_mutex_assert_not_owner(&my_tp_group->LOCK_group);

  /* Lock the group so I can figure out what to do next */
  mysql_mutex_lock(&my_tp_group->LOCK_group);

  set_state(my_thread_data, Thread_state::MANAGING);

  if (my_tp_group->waiting_thread == my_thread_data) {
    /* I'm no longer the waiting thread */
    my_tp_group->waiting_thread = nullptr;
  }

  /* Update statistics on queries queued */
  my_tp_group->stats.queries_queued += events_ready;

  for (i = 0; i < events_ready; i++) {
    queue_query(my_tp_group, next_events[i]);
  }
}

/** Processes a query (actually event) by attaching this thread
    to the connection (which could come directly from polling or from one of
    the query queues). */
static inline void process_query_with_existing_lock(
    tp_group_t *my_tp_group, tp_thread_t *my_thread_data) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&my_tp_group->LOCK_group);
  auto &client_cntx = my_thread_data->client_low_level_cntx;

  // Set connection active for KILL handling
  // Check if the connection was already killed
  if (unlikely(set_connection_active(client_cntx))) {
    my_thread_data->client_low_level_cntx = nullptr;
    return;
  }

  // Assign connection context to this thread.
  assign_thread_to_connection(my_tp_group, my_thread_data);

  auto unassign_grd = create_scope_guard([&] {
    mysql_mutex_assert_owner(&my_tp_group->LOCK_group);
    unassign_thread_from_connection(my_tp_group, my_thread_data);
  });

  // Process the event
  do {
    // Unlock the group before I process an event
    mysql_mutex_unlock(&my_tp_group->LOCK_group);
    auto relock_grd =
        create_scope_guard([&] { mysql_mutex_lock(&my_tp_group->LOCK_group); });

    mysql_mutex_assert_not_owner(&my_tp_group->LOCK_group);
    if (unlikely(tp_process_event(my_tp_group, my_thread_data))) {
      assert(client_cntx == nullptr);
      return;
    }
    // The query on the connection must be re-tried as long as
    // set_connection_inactive_with_existing_lock() returns true.
  } while (
      set_connection_inactive_with_existing_lock(my_tp_group, my_thread_data));
}

/**
  Process one direct query
  This happens when the waiting thread only receives one connection
  with an event and there are no queued queries and there is credit
  to execute a query available.

  @param my_thread_data     Thread data
  @param my_tp_group        Thread group data

  @note
    Single thread can handle work load...
    deliver the event for processing (to myself)

    There is no need to wake anyone up here to process available
    queries. Even though we held the necessary credit to process
    queries we also ensured that there were no queries available
    before we started processing queries. This means that two cases
    can occur here when returning from this call.
    1) We are still the waiting thread, in this case there are no
       available queries since the only one to provide them is
       ourselves and we've been busy processing queries. Thus
       we can safely continue being the waiter thread and wait for
       more queries to arrive.
    2) We are no longer the waiting thread. In this case it is
       possible that the current waiting thread has produced
       available queries. It is however not necessary to wake
       anyone up to handle them since we will enter the main
       loop from here since we're no longer the waiting thread.
*/
void process_direct_query(tp_thread_t *my_thread_data,
                          tp_group_t *my_tp_group) {
  DBUG_TRACE;

  mysql_mutex_assert_not_owner(&my_tp_group->LOCK_group);
  assert(my_thread_data->client_low_level_cntx != nullptr);
  auto &client_cntx = my_thread_data->client_low_level_cntx;

  std::ignore =
      client_cntx->direct_events.fetch_add(1, std::memory_order_relaxed);

  mysql_mutex_lock(&my_tp_group->LOCK_group);
  set_state(my_thread_data, Thread_state::PROCESSING_DIRECT);

  process_query_with_existing_lock(my_tp_group, my_thread_data);
  mysql_mutex_unlock(&my_tp_group->LOCK_group);
}

/**
  Process one queued query

  @param my_tp_group        Thread group data
  @param my_thread_data     Thread data
*/
static void process_queued_query(tp_group_t *my_tp_group,
                                 tp_thread_t *my_thread_data) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&my_tp_group->LOCK_group);

  auto &client_cntx = my_thread_data->client_low_level_cntx;
  // Get a connection context for the highest priority query around
  assert(client_cntx == nullptr);
  client_cntx = pop_highest_priority_query(my_tp_group);

  if (client_cntx == nullptr) return;

  DBUG_PRINT(
      "tp",
      ("Process queued query in Thread group id %d from connection id %lu",
       my_tp_group->group_idx, client_cntx->connection_id));

  set_state(my_thread_data, Thread_state::PROCESSING_QUEUED);
  std::ignore =
      client_cntx->queued_events.fetch_add(1, std::memory_order_relaxed);

  process_query_with_existing_lock(my_tp_group, my_thread_data);
}

/**
  Become consumer thread

  @param my_tp_group     Thread group data
  @param my_thread_data  TP thread data structure
  @return false, always.
*/
static bool become_consumer_thread(tp_group_t *my_tp_group,
                                   tp_thread_t *my_thread_data) {
  DBUG_TRACE;
  /* I'll be a consumer thread */
  my_tp_group->threads_for_consumer++;
  my_tp_group->stats.become_consumer_thread++;
  DBUG_PRINT("tp", ("Become a waiting consumer thread in Thread group id %d",
                    my_tp_group->group_idx));
  set_state(my_thread_data, Thread_state::SLEEPING_CONSUMER);
  std::ignore =
      mysql_cond_wait(&my_tp_group->COND_consumer, &my_tp_group->LOCK_group);
  set_state(my_thread_data, Thread_state::MANAGING);
  DBUG_PRINT("tp", ("Consumer thread wakes up in Thread group id %d",
                    my_tp_group->group_idx));
  my_tp_group->threads_for_consumer--;
  return false;
}

/**
  Become reserve thread

  @param my_tp_group      Thread group data
  @param my_thread_data   TP thread data structure
  @return true if the thread should terminate, false otherwise.
*/
static bool become_reserve_thread(tp_group_t *my_tp_group,
                                  tp_thread_t *my_thread_data) {
  DBUG_TRACE;
  if (thread_pool_max_unused_threads != 0 &&
      ((my_tp_group->threads_for_reserve + 1) >=
       thread_pool_max_unused_threads)) {
    /*
      We already have a sufficient number of reserve threads. We'll
      quit this thread.
    */
    DBUG_PRINT("tp", ("Quit this thread in Thread group id %d",
                      my_tp_group->group_idx));
    return true;
  }
  /* I'll become a reserve thread */

  my_tp_group->threads_for_reserve++;
  my_tp_group->stats.become_reserve_thread++;
  DBUG_PRINT("tp", ("Become a waiting reserve thread in Thread group id %d",
                    my_tp_group->group_idx));
  set_state(my_thread_data, Thread_state::SLEEPING_CONSUMER);
  std::ignore =
      mysql_cond_wait(&my_tp_group->COND_reserve, &my_tp_group->LOCK_group);
  set_state(my_thread_data, Thread_state::MANAGING);
  DBUG_PRINT("tp", ("Reserve thread wakes up in Thread group id %d",
                    my_tp_group->group_idx));
  my_tp_group->threads_for_reserve--;
  return false;
}

/**
   Set connect handler thread to sleep.

   @param my_tp_group  Thread group data.
   @param my_thread_data Query worker thread used.

   @return true if thread should terminate, false otherwise.
*/
static inline bool handle_connect_thread_sleep(tp_group_t *my_tp_group,
                                               tp_thread_t *my_thread_data) {
  mysql_mutex_assert_not_owner(&my_tp_group->LOCK_group);
  mysql_mutex_assert_owner(&my_tp_group->LOCK_connect);
  DBUG_TRACE;

  DBUG_EXECUTE_IF("terminate_connect_thread", return true;);

  assert(my_tp_group->connect_threads <= max_connect_threads_per_group);
  if (my_tp_group->connect_threads > max_connect_threads_per_group) {
    return true;
  }

  DBUG_PRINT("tp", ("Become a waiting connect thread in Thread group id %d",
                    my_tp_group->group_idx));

  my_tp_group->num_connect_handler_thread_in_sleep++;
  auto grd = create_scope_guard([&]() {
    my_tp_group->num_connect_handler_thread_in_sleep--;
    set_state(my_thread_data, Thread_state::CH_PROCESSING);
    DBUG_LOG("tp", "Connect thread wakes up in Thread group id "
                       << my_tp_group->group_idx);
  });

  int rc = 0;
  if (my_tp_group->connect_threads == INITIAL_CONNECT_THREADS_PER_GROUP) {
    set_state(my_thread_data, Thread_state::CH_SLEEPING_INDEFINITE);
    rc =
        mysql_cond_wait(&my_tp_group->COND_connect, &my_tp_group->LOCK_connect);
    assert(rc == 0);
    return false;
  }
  set_state(my_thread_data, Thread_state::CH_SLEEPING_TIMED);
  struct timespec absolute_time;
  set_timespec(&absolute_time,
               DBUG_EVALUATE_IF("ch_force_timed_wait", 2,
                                CONNECT_THREAD_IDLE_TIMEOUT_SECS));
  rc = mysql_cond_timedwait(&my_tp_group->COND_connect,
                            &my_tp_group->LOCK_connect, &absolute_time);

  if (my_tp_group->num_connect_handler_thread_in_sleep > 1 && is_timeout(rc)) {
    return true;  // Return true in case of timeout so worker thread exits.
  }
  return false;
}

/**
  Top-level function for connection handler threads. Pre-condition: Owns
  LOCK_group. Pops first Channel_info object from the connection queue, if any.
  If an event was popped it is processed, otherwise the thread sleeps on its
  condition variable. Post-condition: Owns LOCK_group.
  */
[[nodiscard]] static bool tp_connect_thread_main(tp_group_t *my_tp_group,
                                                 tp_thread_t *my_thread_data) {
  DBUG_EXECUTE_IF("sleep_before_handling_connection",
                  my_sleep(5 * 1000 * 1000););

  mysql_mutex_assert_owner(&my_tp_group->LOCK_group);
  mysql_mutex_assert_not_owner(&my_tp_group->LOCK_connect);

  {  // LOCK_group released scope
    mysql_mutex_unlock(&my_tp_group->LOCK_group);
    auto assert_guard = create_scope_guard([&]() {
      mysql_mutex_assert_not_owner(&my_tp_group->LOCK_group);
      mysql_mutex_lock(&my_tp_group->LOCK_group);
    });

    DBUG_EXECUTE_IF("sleep_before_handling_connection_nolock",
                    my_sleep(5 * 1000 * 1000););

    // Both the queue empty check and the action of sleeping should
    // happen together, atomically, under the LOCK_connect mutex. This
    // ensures that no other thread can alter the queue state between
    // the check and going to sleep.
    mysql_mutex_lock(&my_tp_group->LOCK_connect);
    connection_context_t *next_conn =
        my_tp_group->connection_context_queue.front();
    if (next_conn != nullptr) {
      my_tp_group->connection_context_queue.remove(next_conn);
    }

    if (next_conn == nullptr) {
      // Ensure that all connect handlers will do timed sleep
      DBUG_EXECUTE_IF(
          "ch_force_timed_wait", while (my_tp_group->connect_threads < 2) {
            mysql_mutex_unlock(&my_tp_group->LOCK_connect);
            std::this_thread::sleep_for(Musec(100));
            mysql_mutex_lock(&my_tp_group->LOCK_connect);
          });

      // Call handle_connect_thread_sleep() with LOCK_connect held. The
      // mutex will be released by cond_wait before the thread actually
      // goes to sleep. This ensures that the state of the queue is
      // protected while the thread is checking the queue and deciding
      // to sleep. Meanwhile, producer threads can safely add new
      // connections to the queue while this thread is asleep.
      bool ret = handle_connect_thread_sleep(my_tp_group, my_thread_data);
      mysql_mutex_unlock(&my_tp_group->LOCK_connect);
      return ret;
    }

    mysql_mutex_unlock(&my_tp_group->LOCK_connect);

    // There is a connection event for this thread to work on
    auto next_conn_owner = std::unique_ptr<connection_context_t>(next_conn);

    // If shutdown is initiated, decrement connection count and leave the
    // connection alone. The next_conn object will be deleted, and the
    // connection will be closed when the endpoint is closed.
    if (connection_events_loop_aborted()) {
      dec_connection_count();
      return false;
    }
    if (create_thd_and_authenticate_conn(next_conn, my_tp_group,
                                         my_thread_data)) {
      // We need to simulate that the connection attempt gets blocked by the
      // connection control plugin. But we need to do it after the reply has
      // been sent to the client since mtr does not allow us to create
      // connections asynchronously.
      // Return true to avoid having the extra threads for a full minute.
      DBUG_EXECUTE_IF("simulate_conn_ctrl",
                      std::this_thread::sleep_for(std::chrono::seconds{5});
                      return true;);
      return false;  // Yes false, since we do not want to terminate the main
                     // loop here. We only do that to terminate an extra
                     // connection handler thread which has timed out sleeping
                     // and is not needed.
    }
  }  // LOCK_group released scope
  mysql_mutex_assert_owner(&my_tp_group->LOCK_group);
  mysql_mutex_assert_not_owner(&my_tp_group->LOCK_connect);

  return false;
}

/**
  Set worker thread to sleep as either consumer or reserve thread

  @param my_tp_group      Thread group data
  @param my_thread_data   TP thread data structure
  @return true if thread should terminate, false otherwise.
*/
static bool handle_worker_thread_sleep(tp_group_t *my_tp_group,
                                       tp_thread_t *my_thread_data) {
  return (my_tp_group->threads_for_consumer < my_tp_group->threads_user_request)
             ? become_consumer_thread(my_tp_group, my_thread_data)
             : become_reserve_thread(my_tp_group, my_thread_data);
}

/**
  Listen for network events from client and dispatch query to
  worker thread if enough credit is available.

  @param my_tp_group Pointer to thread group.
  @param my_thread_data Thread data.
  @return false, always.
*/
static inline bool dedicated_listener_loop(tp_group_t *my_tp_group,
                                           tp_thread_t *my_thread_data) {
  while (!is_tp_shutdown()) {
    handle_waiting_thread(my_tp_group, my_thread_data, true);
    if (is_tp_shutdown()) break;
    // NOTE: We not acquire LOCK_query_threads since
    // this lock is used as synchronization between thread create and thread
    // exit to maintain exact count of query threads. Taking lock during query
    // processing path is unnecessary overhead in terms of performance.

    std::uint32_t txnlim =
        configured_max_transactions_limit_per_group(my_tp_group);

    if (txnlim == 0 || my_tp_group->query_threads_count < txnlim)
      tp_wake_thread(my_tp_group, WAKE_OR_CREATE_ONE);
    else
      tp_wake_thread(my_tp_group, WAKE_IF_CREATED);
  }
  return false;
}

/**
  Process query by dequeuing from queue.

  @param my_tp_group Pointer to thread group.
  @param my_thread_data Thread group data.

  @return true if thread should terminate, false otherwise.
*/

static inline bool query_worker_thread_loop(tp_group_t *my_tp_group,
                                            tp_thread_t *my_thread_data) {
  while (!is_tp_shutdown()) {
    if (is_query_ready_to_process(my_tp_group))
      process_queued_query(my_tp_group, my_thread_data);
    else {
      std::uint32_t txnlim =
          configured_max_transactions_limit_per_group(my_tp_group);
      if (txnlim != 0 && my_tp_group->query_threads_count > txnlim) return true;
      handle_worker_thread_sleep(my_tp_group, my_thread_data);
    }
  }
  return false;
}

/**
  Run using low concurrency algorithm

  @param my_tp_group    Thread group data
  @param my_thread_data Thread data
  @return true if the thread should terminate, false otherwise.
*/
static inline bool low_concurrency_algorithm(tp_group_t *my_tp_group,
                                             tp_thread_t *my_thread_data) {
  /*
    Entering this function always means that there are available
    queries since it is the low concurrency algorithm. In this
    mode we don't operate a separate waiting thread, we take care
    of getting events as part of the execution loop. Thus the only
    reason to become stalled here is due to lack of credit to do
    anything.

    Wait for something to do as long as the following is true:
    1) There are no queries available OR there is no credit to execute
    2) Shutdown isn't ordered

    If there is nothing to do and there is enough threads waiting to consume,
    lets get into the reserve group that will RARELY need to be scheduled.
    This is an optimization to avoid too many context switches when the
    pool group has MANY threads and FEW clients. It also improves the use of
    CPU caches since it will reuse mostly the same threads all the time.
  */
  DBUG_TRACE;
  while ((!is_query_ready_to_process(my_tp_group)) && (!is_tp_shutdown())) {
    /*
      Even in the low concurrency case it is possible to come here
      with no queries queued up after the last query being delayed by
      some IO or row lock or similar wait situation. If no one is taking
      care of waiting in this situation we need to do it here.
    */
    // Use essentially the same logic as in the high_concurrency
    // algorithm to ensure there is a waiting thread even when no queries
    // actually ready to process due to MTL. Without this we may miss credit
    // increases due to changed MTL and kill requests.
    if ((!is_query_available(my_tp_group) ||
         configured_max_transactions_limit_per_group(my_tp_group) > 0) &&
        my_tp_group->waiting_thread == nullptr) {
      handle_waiting_thread(my_tp_group, my_thread_data, true);
      return false;
    }
    if (handle_worker_thread_sleep(my_tp_group, my_thread_data)) {
      return true;
    }
  }

  /*
    Process queries until there are no more queries to process OR
    someone has grabbed all the credit available. To ensure we
    can keep the priorities right, we check for new events when
    there are no more high priority queries available to process.
    This check is done without waiting for events, we simply check
    if there are events already arrived.

    This means that in a high load situation we have one thread
    taking care of both getting events and executing queries to
    ensure we have optimal CPU cache usage. What will disturb
    this behaviour is when this thread needs to go to sleep, in
    this case another thread is woken up to process the events.
  */
  if (my_tp_group->waiting_thread == nullptr &&
      !is_high_prio_query_available(my_tp_group)) {
    handle_waiting_thread(my_tp_group, my_thread_data, false);
  }

  while (is_query_ready_to_process(my_tp_group) && (!is_tp_shutdown())) {
    process_queued_query(my_tp_group, my_thread_data);
    if (my_tp_group->waiting_thread == nullptr &&
        !is_high_prio_query_available(my_tp_group)) {
      handle_waiting_thread(my_tp_group, my_thread_data, false);
    }
  }
  return false;
}

/**
  Run using high concurrency algorithm

  @param my_tp_group    Thread group data
  @param my_thread_data Thread data
  @return true if the thread should terminate, false otherwise.
*/
static inline bool high_concurrency_algorithm(tp_group_t *my_tp_group,
                                              tp_thread_t *my_thread_data) {
  DBUG_TRACE;
  if ((my_tp_group->waiting_thread != nullptr &&
       (!is_query_ready_to_process(my_tp_group)) && !is_tp_shutdown())) {
    return handle_worker_thread_sleep(my_tp_group, my_thread_data);
  }

  while (is_query_ready_to_process(my_tp_group) && (!is_tp_shutdown())) {
    if ((count_waiting_queries(my_tp_group) > 1) ||
        my_tp_group->waiting_thread == nullptr) {
      /*
        There are either more queries to execute or there is a waiting
        thread to handle. We will wake up another thread to handle
        this. However we will not wake up any "cold" thread (reserve
        threads), we only want to use "hot" threads (consumer threads).

        This is to avoid polluting the CPU caches, in medium concurrency
        it is important to ensure that there is a sufficient number of
        threads working to ensure that some of them get some progress

        At most we can have 3 threads working in parallel per thread group
        when 1 is the pool size per thread group. One thread is actively
        executing a query, one is a waiting thread and one is waiting on
        the consumer condition variable. In most cases 1-2 threads are
        executing per thread group though.
      */
      tp_wake_thread(my_tp_group, WAKE_IF_CONSUMER);
    }
    /*
      Check for high priority queries before processing the query to
      ensure that we don't lose performance at high concurrency.
    */
    if (my_tp_group->waiting_thread == nullptr &&
        !is_high_prio_query_available(my_tp_group)) {
      handle_waiting_thread(my_tp_group, my_thread_data, false);
      if (count_waiting_queries(my_tp_group) == 0) {
        continue;
      }
    }
    process_queued_query(my_tp_group, my_thread_data);
  }

  if (my_tp_group->waiting_thread == nullptr && !is_tp_shutdown()) {
    handle_waiting_thread(my_tp_group, my_thread_data, true);
  }
  return false;
}

/**
  Update thread group variables at worker thread exit

  @param my_tp_group            Thread group data
  @param my_thread_data         Thread data
*/
static void update_thread_group_thread_exit(tp_group_t *my_tp_group,
                                            tp_thread_t *my_thread_data) {
  DBUG_TRACE;
  uint i;
  uint max_id = 0;
  tp_thread_t *cur_thread;

  my_thread_data->thread_handle.thread = null_thread_initializer;
  my_thread_data->client_low_level_cntx = nullptr;
  my_thread_data->thread_tp_group = GROUP_ID_GARBAGE;
  my_thread_data->pfs_thread_id = 0;

  for (i = 0; i < MAX_THREADS_PER_GROUP; i++) {
    cur_thread = &(my_tp_group->group_threads[i]);
    if (cur_thread->thread_handle.thread != null_thread_initializer) {
      max_id = i + 1;
    }
  }
  my_tp_group->max_thread_ids_in_group = max_id;
}

/*
  The main() or while(forever) routine for the thread pool worker threads.

  @param arg      Thread context for the thread pool

  @retval         NULL

  @note
    Threads here wait and process events from clients/sockets.
    This will:
      - Loop until global shutdown variable is set
      - Watch sockets for bytes
      - Process requests for a client
*/
extern "C" void *tp_worker_thread_main(void *arg) {
  THD *thd = nullptr; /* Used to indicate start of stack */
  auto thread_end_guard = create_scope_guard([&]() {
    my_thread_end();
    remove_ssl_err_thread_state();
  });
  tp_thread_t *my_thread_data = pointer_cast<tp_thread_t *>(arg);

  if (my_thread_init()) {
    return nullptr;
  }

  if (is_tp_shutdown()) return nullptr;

  /* Set up the thread pool */
  inc_thread_created();

  assert(my_thread_data->thread_tp_group < MAX_THREAD_GROUPS);
#ifdef HAVE_PSI_THREAD_INTERFACE
  my_thread_data->pfs_thread_id =
      PSI_THREAD_CALL(get_current_thread_internal_id)();
#endif
  tp_group_t *my_tp_group = &(tp_group_list[my_thread_data->thread_tp_group]);

  my_thread_data->stack_start = (char *)&thd;
  mysql_mutex_lock(&my_tp_group->LOCK_group);

  /* Count how many threads are initialized for this group */
  my_tp_group->threads_initialized++;
  DBUG_PRINT("tp", ("new thread initialized in Thread group id %d",
                    my_tp_group->group_idx));

  if (!is_thread_pool_plugin_initialized()) {
    // Signal that the thread is now started
    mysql_cond_broadcast(&my_tp_group->COND_group);
  }

  /* START MAIN FOREVER LOOP */
  bool ret_code = false;
  while (!is_tp_shutdown() && !ret_code) {
    if (my_thread_data->is_connection_handler_thread == true) {
      ret_code = tp_connect_thread_main(my_tp_group, my_thread_data);
      continue;
    }
    if (thread_pool_dedicated_listeners) {
      if (my_thread_data->worker_type ==
          Worker_thread_type::LISTENER_WORKER_THREAD)
        dedicated_listener_loop(my_tp_group, my_thread_data);
      else
        ret_code = query_worker_thread_loop(my_tp_group, my_thread_data);

      continue;
    }

    if (thread_pool_algorithm == LOW_CONCURRENCY_ALGORITHM) {
      ret_code = low_concurrency_algorithm(my_tp_group, my_thread_data);
    } else {
      ret_code = high_concurrency_algorithm(my_tp_group, my_thread_data);
    }
  }  // while

  if (my_thread_data->is_connection_handler_thread) {
    assert(is_tp_shutdown() ||
           DBUG_EVALUATE_IF("terminate_connect_thread", true, false) ||
           my_tp_group->connect_threads > 1);
    my_tp_group->connect_threads--;
  } else {
    my_tp_group->num_query_threads--;
    tp_thread_create_control_ctx.total_query_threads--;
  }

  my_tp_group->threads_initialized--;
  if (my_thread_data->worker_type == Worker_thread_type::QUERY_WORKER_THREAD) {
    mysql_mutex_lock(&my_tp_group->LOCK_query_threads_count);
    thread_pool_query_worker_threads--;
    my_tp_group->query_threads_count--;
    mysql_mutex_unlock(&my_tp_group->LOCK_query_threads_count);
  }

  update_thread_group_thread_exit(my_tp_group, my_thread_data);
  if (is_tp_shutdown()) {
    /*
      No need to wake anyone to announce a thread is quitting except
      shutdown.
    */
    mysql_cond_broadcast(&my_tp_group->COND_group);
    mysql_cond_broadcast(&my_tp_group->COND_consumer);
    mysql_cond_broadcast(&my_tp_group->COND_reserve);
    mysql_cond_broadcast(&my_tp_group->COND_connect);
  }
  mysql_mutex_unlock(&my_tp_group->LOCK_group);
  return nullptr;
}

/**
  The routine to start the background stall checker thread.

  @note
    Start the stall checker thread
      - A new OS thread is created
*/
static bool tp_create_stall_check_thread() {
  DBUG_TRACE;

  mysql_mutex_init(key_tp_LOCK_stall_check, &tp_LOCK_stall_check,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_tp_COND_stall_check, &tp_COND_stall_check);

  stall_check_initialized = true;
  assert(stall_checker_state == Stall_checker_state::NOT_STARTED);
  tp_thread_t *cur_thread = &tp_stall_check_thread;
  assert(cur_thread->thread_handle.thread == null_thread_initializer);

  mysql_mutex_lock(&tp_LOCK_stall_check);
  auto lock_guard =
      create_scope_guard([] { mysql_mutex_unlock(&tp_LOCK_stall_check); });
  int error = mysql_thread_create(
      key_tp_stall_check_thread, &(cur_thread->thread_handle),
      get_connection_attrib(), tp_stall_check_thread_main, cur_thread);
  if (error) {
    LogErr(ERROR_LEVEL, ER_THREAD_POOL_FAILED_TO_CREATE_POOL, error,
           socket_errno);
    return true;
  }
  // Wait for stall check thread to start
  while (stall_checker_state == Stall_checker_state::NOT_STARTED) {
    (void)mysql_cond_wait(&tp_COND_stall_check, &tp_LOCK_stall_check);
  }
  return stall_checker_state == Stall_checker_state::TERMINATED ? true : false;
}

/**
  Let stall check thread go to sleep for a while

  @param         actions_taken    Action taken will affect sleep time
*/
static inline void sleep_stall_check(int actions_taken, uint *wakeup_reason) {
  assert(stall_check_initialized);

  uint sleep_nanosecs;
  struct timespec time_out;

  if (actions_taken == 0) {
    sleep_nanosecs = 10 * NANOSEC_PER_MILLI;
    set_state(&tp_stall_check_thread, Thread_state::SC_SLEEPING_LONG);
  } else {
    sleep_nanosecs = 1 * NANOSEC_PER_MILLI;
    set_state(&tp_stall_check_thread, Thread_state::SC_SLEEPING_SHORT);
  }

  set_timespec_nsec(&time_out, sleep_nanosecs);

  mysql_mutex_lock(&tp_LOCK_stall_check);
  *wakeup_reason = mysql_cond_timedwait(&tp_COND_stall_check,
                                        &tp_LOCK_stall_check, &time_out);
  mysql_mutex_unlock(&tp_LOCK_stall_check);
  set_state(&tp_stall_check_thread, Thread_state::SC_CHECKING);
}

/**
  Signal that the stall check thread has changed its status

  @param val            New status, 0 = stopped, 1 = started
*/
static void signal_stall_check_thread_change(Stall_checker_state val) {
  assert(stall_check_initialized);

  mysql_mutex_lock(&tp_LOCK_stall_check);
  stall_checker_state = val;
  mysql_cond_broadcast(&tp_COND_stall_check);
  mysql_mutex_unlock(&tp_LOCK_stall_check);
}

/**
  Return the connection timeout in TP, which is the net_wait_timeout from the
  THD + 10 ms for the stall interval.
*/
static inline Musec tp_connection_timeout(const tp_client_low_level_t &cc) {
  return Musec{10} + std::chrono::seconds{thd_get_net_wait_timeout(cc.thd)};
}

/**
  Set (update) expiry time point for a connection, i.e. mark it as active now.
*/
static inline void set_idle_timeout(tp_group_t *my_tp_group,
                                    tp_client_low_level_t *client_cntx) {
  DBUG_TRACE;

  auto expire_timpt = tp_now() + tp_connection_timeout(*client_cntx);
  if (unlikely(expire_timpt < my_tp_group->earliest_con_expire_timpt)) {
    my_tp_group->earliest_con_expire_timpt = expire_timpt;
  }
  client_cntx->expiry_timpt = expire_timpt;
}

/**
   Check for connections in the thread group that have been idle longer than
   the idle timeout and terminate them. Print a message in the error log with
   information about terminated connections and how long they have been idle.

   @param cur_group thread group in which to terminate expired connections
 */
static void terminate_expired_connections(tp_group_t *cur_group,
                                          Time_pt stall_check_timpt) {
  if (cur_group->earliest_con_expire_timpt >= stall_check_timpt) return;

  // We have passed the earliest recorded expiry time point,
  // so there could potentially be exprired connections in the group.

  // By resetting the earliest expiry time point the loop will
  // compute the new earliest expiry time point, so that the next
  // run of the loop can be delayed.
  DBUG_PRINT(
      "tp_idle",
      ("We have passed earliest expire point by %lld microseconds in "
       "thread group %u",
       static_cast<long long int>(
           (stall_check_timpt - cur_group->earliest_con_expire_timpt).count()),
       cur_group->group_idx));

  cur_group->earliest_con_expire_timpt = Time_pt::max();
  for (tp_client_low_level_t *c = cur_group->open_connections.front();
       c != nullptr; c = c->next_open_connection) {
    DBUG_PRINT("tp_idle", ("Checking if connection {cc:%p, thd:%p, grp:%u, "
                           "connid:%lu} is idle",
                           c, c->thd, cur_group->group_idx, c->connection_id));

    if (c->is_queued || c->active_flag) continue;

    assert(thd_get_net_read_write(c->thd) == 1);

    if (c->expiry_timpt >= stall_check_timpt) {
      // Connection has not yet expired, and if the expiry point
      // of the connection is earlier than what has been seen so
      // far we update the earliest expiry point for the thread group.
      if (c->expiry_timpt < cur_group->earliest_con_expire_timpt)
        cur_group->earliest_con_expire_timpt = c->expiry_timpt;
      continue;
    }
    thd_lock_data(c->thd);
    thd_set_killed(c->thd);
    thd_unlock_data(c->thd);

    if (c->connection_killed_state == TP_CONNECTION_NOT_KILLED) {
      c->connection_killed_state = TP_CONNECTION_KILLED_WAITING_FLAG;
      c->conn_state = Connection_state::EXPIRED;
      tp_group_low_level_waiter_wake(cur_group->group_low_level_cntx,
                                     KILL_FLAG);
    }

    Musec last_active_diff =
        tp_connection_timeout(*c) + (stall_check_timpt - c->expiry_timpt);
    Time_pt last_active = c->expiry_timpt - tp_connection_timeout(*c);

    Security_context *sctx = c->thd->security_context();
    Auth_id auth_id(sctx->priv_user(), sctx->priv_host());
    auto last_active_diff_sec =
        std::chrono::duration_cast<std::chrono::seconds>(last_active_diff);
    std::ostringstream msg;
    msg << "Thread pool closed connection id " << c->connection_id << " for "
        << auth_id.auth_str() << " after " << last_active_diff_sec.count()
        << '.' << std::setfill('0') << std::setw(6)
        << (last_active_diff % last_active_diff_sec).count()
        << " seconds of inactivity. Attributes: priority:"
        << (is_high_priority_connection(c->thd) ? "high" : "normal")
        << ", type:" << (cur_group == admin_thread_group ? "admin" : "normal")
        << ", last active:" << last_active << ", expired:" << c->expiry_timpt
        << " (" << (stall_check_timpt - c->expiry_timpt).count()
        << " microseconds ago)";

    LogErr(INFORMATION_LEVEL, ER_THREAD_POOL_IDLE_CONNECTION_CLOSED,
           msg.str().c_str());

    std::string timeout{std::to_string(thd_get_net_wait_timeout(c->thd))};
    LogErr(INFORMATION_LEVEL, ER_NET_WAIT_ERROR2, timeout.c_str(),
           auth_id.auth_str().c_str());
  }  // for
}

namespace {

using Connection_id = std::int64_t;

/**
  Represent a connection which may have been closed by its id
  and the last time it was deactivated (a qury ended).
*/
struct Conn_rep {
  Connection_id id = 0;
  Time_pt time_of_deactivation;
};
bool operator<(const Conn_rep &lhs, const Conn_rep &rhs) {
  return lhs.id < rhs.id;
}
using Conn_rep_vec = std::vector<Conn_rep>;

/**
  A view of the connections found in the system at a given
  time which includes both information from thread group and
  Connection_reps for the connections currently present.
 */
using Conn_stat_array = std::array<longlong, 8>;
using Init_stat_array = std::array<longlong, 5>;
class Conn_snapshot {
  Time_pt m_time_of_collection;
  Conn_stat_array c_stats = {};
  Init_stat_array i_stats = {};
  Conn_rep_vec conn_rep_buf;
  bool m_reps_are_sorted = false;

 public:
  Time_pt get_time_of_collection() const { return m_time_of_collection; }
  void set_time_of_collection(Time_pt sc_now) { m_time_of_collection = sc_now; }

  void ensure_sorted() {
    if (m_reps_are_sorted) return;
    std::sort(conn_rep_buf.begin(), conn_rep_buf.end());
    m_reps_are_sorted = true;
  }

  const Conn_rep_vec *conn_reps() const { return &conn_rep_buf; }
  const Conn_stat_array *conn_stats() const { return &c_stats; }
  const Init_stat_array *init_stats() const { return &i_stats; }

  /** Clear the state for collecting a new snapshot. */
  void reset() {
    m_time_of_collection = Time_pt::min();
    c_stats = {};
    i_stats = {};
    conn_rep_buf.clear();
    m_reps_are_sorted = false;
  }

 private:
  template <std::int64_t SIGN_FACTOR>
  void collect_reps(const tp_group_t &tg) {
    for (const tp_client_low_level_t *c = tg.open_connections.front();
         c != nullptr; c = c->next_open_connection) {
      Connection_id cid = c->connection_id * SIGN_FACTOR;
      conn_rep_buf.push_back(
          {cid, (c->active_flag ? Time_pt::max()
                                : c->time_of_last_event_completion)});
      [[maybe_unused]] auto rel_time_since_completion =
          (c->time_of_last_event_completion - tp_now()).count();
      DBUG_LOG("tp_conrep", "Collected connection "
                                << X_(c->connection_id) << X_(c->active_flag)
                                << X_(rel_time_since_completion));
    }
  }

 public:
  /** Collection information about connections in the process of being
   * established. */
  void collect_init() {
    auto &[incoming, queued, auth, err, ok] = i_stats;
    auto &[acnt, ncnt, aopn, nopn, acls, ncls, aque, nque] = c_stats;

    incoming = get_incoming_connects();
    queued = aque + nque;

    // Sequenceing this to load atomics only once to have better consistency.
    auto exit_auth = connections_exiting_auth.load();
    auth = connections_entering_auth.load() - exit_auth;
    err = get_aborted_connects();
    ok = exit_auth - err;
  }

  /** Collection information about connections in a givent thread group. */
  void collect_admin_group(const tp_group_t &tg) {
    auto &[acnt, ncnt, aopn, nopn, acls, ncls, aque, nque] = c_stats;
    acnt = tg.stats.connection_count;
    aopn = tg.stats.connections_started;
    acls = tg.stats.connections_closed;
    aque = tg.connection_context_queue.elements();
    collect_reps<-1>(tg);
  }
  void collect_user_group(const tp_group_t &tg) {
    auto &[acnt, ncnt, aopn, nopn, acls, ncls, aque, nque] = c_stats;
    ncnt += tg.stats.connection_count;
    nopn += tg.stats.connections_started;
    ncls += tg.stats.connections_closed;
    nque += tg.connection_context_queue.elements();
    collect_reps<1>(tg);
  }
};

/** Output information for connections being established. */
std::ostream &operator<<(std::ostream &os, const Init_stat_array &i_stats) {
  auto &[incoming, queued, auth, err, ok] = i_stats;
  os << incoming << ", " << queued << ", " << auth << ", " << err << ", " << ok;
  return os;
}

/** Helper struct to simplify formatting of log message. */
struct Conn_counts {
  longlong count = 0;
  longlong count_d = 0;
  longlong opened_d = 0;
  longlong closed_d = 0;

  std::size_t ids_added = 0;
  std::size_t ids_dropped = 0;
  longlong ids_active = 0;
};

/**
  Output connection information for a class of connections (normal or admin)
  from a Conn_counts object.
 */
std::ostream &operator<<(std::ostream &os, const Conn_counts &cc) {
  os << cc.count << "(" << std::showpos << cc.count_d << std::noshowpos << "), "
     << cc.ids_active << ", " << cc.opened_d << ", " << cc.closed_d << ", "
     << cc.ids_added << ", " << cc.ids_dropped;
  return os;
}

/**
  Keeps track of connection info between each time a log message is written
  and calculates the changes which occur. */
class Conn_tracker {
  Conn_snapshot a;
  Conn_snapshot b;
  Conn_snapshot *baseline = &a;
  Conn_snapshot *current = &b;

  Conn_rep_vec resbuf;

 public:
  Time_pt get_time_of_previous_report() const {
    return baseline->get_time_of_collection();
  }

  Conn_snapshot *current_snap() const { return current; }

  /**
    Calculate the difference since last report and print a message to the
    error log.
   */
  void emit_connection_report(Time_pt sc_now) {
    current->set_time_of_collection(sc_now);
    auto base_tp = baseline->get_time_of_collection();

    /** Collect and calculate connections being established. */
    current->collect_init();
    const Init_stat_array &init_stats = *current->init_stats();
    Init_stat_array init_stat_diffs;
    std::transform(init_stats.begin(), init_stats.end(),
                   baseline->init_stats()->begin(), init_stat_diffs.begin(),
                   std::minus<>());

    // Keep queue size and auth from last snapshot, they are not accumulated.
    init_stat_diffs[1] = init_stats[1];
    init_stat_diffs[2] = init_stats[2];

    /** Calculate connections already established. */
    const Conn_stat_array &conn_stats = *current->conn_stats();
    const Conn_rep_vec &conn_reps = *current->conn_reps();
    longlong adm_count = conn_stats[0];
    longlong usr_count = conn_stats[1];
    Conn_stat_array conn_stat_diffs;
    std::transform(conn_stats.begin(), conn_stats.end(),
                   baseline->conn_stats()->begin(), conn_stat_diffs.begin(),
                   std::minus<>());
    longlong stat_diff_count =
        std::accumulate(conn_stat_diffs.begin() + 2, conn_stat_diffs.end(), 0);

    using LLP = std::pair<longlong, longlong>;
    auto [adm, usr] = [&]() {
      auto [acntd, ucntd, aopnd, uopnd, aclsd, uclsd, aque, uque] =
          conn_stat_diffs;
      Conn_counts adm_ = {adm_count, acntd, aopnd, aclsd};
      Conn_counts usr_ = {usr_count, ucntd, uopnd, uclsd};

      if (stat_diff_count == 0) {
        auto [aa, ua] =
            std::accumulate(conn_reps.begin(), conn_reps.end(), LLP(0, 0),
                            [&](auto init, const Conn_rep &cr) {
                              if (cr.time_of_deactivation > base_tp) {
                                if (cr.id < 0)
                                  ++init.first;
                                else
                                  ++init.second;
                              }
                              return init;
                            });
        adm_.ids_active = aa;
        usr_.ids_active = ua;

        return std::pair(adm_, usr_);
      }

      // If connections have been opened and closed, we need to
      // add that and find the sets of ids added and dropped

      // Find ids which have been added
      baseline->ensure_sorted();
      current->ensure_sorted();
      resbuf.clear();
      std::set_difference(
          conn_reps.begin(), conn_reps.end(), baseline->conn_reps()->begin(),
          baseline->conn_reps()->end(), std::back_inserter(resbuf));

      auto is_usr_conn = [](const Conn_rep &rep) { return rep.id >= 0; };
      auto usr_begin = std::find_if(resbuf.begin(), resbuf.end(), is_usr_conn);
      adm_.ids_added = std::distance(resbuf.begin(), usr_begin);
      usr_.ids_added = resbuf.size() - adm_.ids_added;

      // Find ids which have been dropped
      resbuf.clear();
      std::set_difference(baseline->conn_reps()->begin(),
                          baseline->conn_reps()->end(), conn_reps.begin(),
                          conn_reps.end(), std::back_inserter(resbuf));
      usr_begin = std::find_if(resbuf.begin(), resbuf.end(), is_usr_conn);
      adm_.ids_dropped = std::distance(resbuf.begin(), usr_begin);
      usr_.ids_dropped = resbuf.size() - adm_.ids_dropped;

      // Find active ids. Sorted version.
      // First examine the set of dropped ids. If any were active in the
      // baseline, we count them as active.
      auto is_currently_active = [](const Conn_rep &cr) {
        return cr.time_of_deactivation == Time_pt::max();
      };
      adm_.ids_active =
          std::count_if(resbuf.begin(), usr_begin, is_currently_active);
      usr_.ids_active =
          std::count_if(usr_begin, resbuf.end(), is_currently_active);

      // Now add those in the current snapshot which have
      // time_of_deactivation after the baseline was collected.
      auto was_active_since_baseline = [&](const Conn_rep &rep) {
        return rep.time_of_deactivation > base_tp;
      };
      resbuf.clear();
      adm_.ids_active +=
          std::count_if(conn_reps.begin(), conn_reps.begin() + adm_.count,
                        was_active_since_baseline);
      usr_.ids_active +=
          std::count_if(conn_reps.begin() + adm_.count, conn_reps.end(),
                        was_active_since_baseline);

      return std::pair(adm_, usr_);
    }();

    // Emit message here
    std::ostringstream msgstr;
    msgstr << "Init: " << init_stat_diffs;
    msgstr << ". Usr: " << usr;
    msgstr << ". Adm: " << adm << ".";

    LogErr(INFORMATION_LEVEL, ER_THREAD_POOL_CONNECTION_INIT_REPORT,
           msgstr.str().c_str());

    std::swap(current, baseline);
    current->reset();
  }
};

/**
  Atomic variable holding the currently set value of the plugin system variable,
  which can be safely accessed from multiple threads.
*/
std::atomic<std::chrono::seconds> connection_report_interval;

/**
  Duration after startup in which connection report messages are emitted more
  frequently than the configured thread_pool_connection_report_interval (unless
  it is disabled).
 */
constexpr std::chrono::minutes STARTUP_FREQUENT_LOGGING_INTERVAL(3);

/**
  When in the STARTUP_FREQUENT_LOGGING_INTERVAL, the duration between each
  connection report will be
  (thread_pool_connection_report_interval/STARTUP_FREQUENT_LOGGING_FACTOR).
*/
constexpr int STARTUP_FREQUENT_LOGGING_FACTOR = 10;

/**
  Get the target duration to when to emit the next connection report.

   @param sc_now time when current iteration of stall checker started.
   @param cri    connection report interval
   @return Target Time_pt for next connection report.
 */
Musec get_duration_to_next_connection_report(Time_pt sc_now,
                                             std::chrono::seconds cri) {
  static Time_pt end_of_startup = sc_now + STARTUP_FREQUENT_LOGGING_INTERVAL;

  if (sc_now < end_of_startup) {
    Musec dur = (std::chrono::duration_cast<Musec>(cri) /
                 STARTUP_FREQUENT_LOGGING_FACTOR);
    DBUG_LOG("tp_conrepV", "Using reduced connection report interval:"
                               << dur.count()
                               << " microseconds during startup.");
    return dur;
  }

  DBUG_LOG("tp_conrepV", "Using default connection report interval:"
                             << cri.count()
                             << " microseconds during normal opertaion.");
  return cri;
}
}  // namespace

/** Predicate to determine if a value for connection_report_interval is valid.
 */
bool is_valid_connection_report_interval(longlong cri) {
  return (cri == CONNECTION_REPORT_INTERVAL_OFF ||
          (MIN_CONNECTION_REPORT_INTERVAL <= cri &&
           cri <= MAX_CONNECTION_REPORT_INTERVAL));
}

#ifndef NDEBUG
static bool force_connection_report = false;
#endif /* NDEBUG */

/** Update the connection_report interval using memory_order_relaxed. */
void set_connection_report_interval(std::chrono::seconds v) {
  DBUG_EXECUTE_IF("tp_force_connection_report", {
    mysql_mutex_lock(&tp_LOCK_stall_check);
    // By setting this flag we force the stall checker thread to
    // emit a single connection report as soon as it wakes up.
    force_connection_report = true;
    mysql_mutex_unlock(&tp_LOCK_stall_check);
    mysql_cond_signal(&tp_COND_stall_check);
  });
  connection_report_interval.store(v, std::memory_order_relaxed);
}

/*
  The main() or while(forever) routine for the thread pool background stall
  check thread.

  @param arg         Argument to new thread

  @retval            NULL

  @note
    This threads watches for situations where the thread pool
      does not have enough credits to process the workload.  It may take
      actions such as changing the amount of credits or creating more
      thread to process the workload.
    This will:
      - Loop until global shutdown variable is set
      - Check all thread pool groups for stalled conditions
      - Adjust credits or start threads to unstall

    The stall check thread is NOT waited for on shutdown
*/

extern "C" void *tp_stall_check_thread_main(void *) {
  assert(stall_check_initialized);

  auto thread_end_guard = create_scope_guard([&]() {
    DBUG_PRINT("tp_exit", ("ending thread"));
    signal_stall_check_thread_change(Stall_checker_state::TERMINATED);
    my_thread_end();
  });

  /* Set up the thread */
  if (my_thread_init()) {
    return nullptr;
  }

  /* Signal that the thread is started properly */
  signal_stall_check_thread_change(Stall_checker_state::RUNNING);
  DBUG_PRINT("tp", ("Completed start-up of stall check thread"));

  uint wakeup_reason = 0;

  Conn_tracker conn_tracker;
  Time_pt time_of_completed_check;
  Time_pt time_of_next_longrun_check;
  /* START MAIN FOREVER LOOP */
  while (!is_tp_shutdown()) {
    uint actions_taken = 0;
    Time_pt stall_check_timpt = tp_now();
    auto time_since_completed_check =
        stall_check_timpt - time_of_completed_check;
    time_of_completed_check = stall_check_timpt;
    bool new_10ms = (time_since_completed_check >= Misec(10));

    std::chrono::seconds cri =
        connection_report_interval.load(std::memory_order_relaxed);

    bool must_emit_connections_report =
        (cri != std::chrono::seconds::zero() &&  // Reporting is not disabled
         get_server_state() >=
             SERVER_OPERATING &&  // Server is running or shutting down
         stall_check_timpt >=
             (conn_tracker.get_time_of_previous_report() +
              get_duration_to_next_connection_report(stall_check_timpt, cri)));

    DBUG_EXECUTE_IF("tp_force_connection_report", {
      // Should be ok, even if we do not hold tp_LOCK_stall_check
      // because we will see a true value written by a connection
      // thread as long as it was written before we woke up from cv-wait.
      must_emit_connections_report = force_connection_report;
      // This write may not be visible to connection threads but
      // they do not read it, only we do.
      force_connection_report = false;
    });

    DBUG_LOG(
        "tp_conrepV",
        X_(must_emit_connections_report)
            << X_(stall_check_timpt.time_since_epoch().count())
            << X_(conn_tracker.get_time_of_previous_report()
                      .time_since_epoch()
                      .count())
            << X_(conn_tracker.get_time_of_previous_report()
                      .time_since_epoch()
                      .count())
            << X_(get_duration_to_next_connection_report(stall_check_timpt, cri)
                      .count()));

    auto lrci = longrun_check_interval();
    bool do_longrun_check =
        (thread_pool_max_transactions_limit > 0 && lrci > Misec::zero() &&
         stall_check_timpt > time_of_next_longrun_check);
    Time_pt time_of_grace_threshold;
    if (do_longrun_check) {
      time_of_next_longrun_check = stall_check_timpt + lrci;
      time_of_grace_threshold = stall_check_timpt - longrun_grace_period();
    }
    /*
    We have one stall check thread for all thread groups, we loop over
    all thread groups once per time we're waken up. Including the admin thread
    group.
  */
    for (uint i = 0; (i < tp_groups) && (!is_tp_shutdown()); i++) {
      tp_group_t *cur_group = &(tp_group_list[i]);

      DBUG_LOG("tp_scv", "SC: " << X_(cur_group->group_idx)
                                << X_(cur_group->waiting_thread)
                                << X_(is_query_ready_to_process(cur_group)));
      mysql_mutex_lock(&cur_group->LOCK_group);
      mark_and_count_stalled_threads(cur_group, stall_check_timpt);
      auto change_active_threads = update_max_active_threads(cur_group);

      if (do_longrun_check && cur_group != admin_thread_group) {
        check_longrunning_transactions_and_adjust_MTL(
            cur_group, stall_check_timpt, time_of_grace_threshold);
      }
      tp_thread_t *cur_thread = cur_group->waiting_thread;
#ifndef NDEBUG
      if (cur_thread) {
        DBUG_PRINT(
            "tp",
            ("Thread group id: %u, Waiting thread active since %lld ms ",
             cur_group->group_idx,
             (cur_thread->time_of_attach == BEGINNING_OF_EPOCH
                  ? -1
                  : static_cast<long long>(
                        std::chrono::duration_cast<Misec>(
                            stall_check_timpt - cur_thread->time_of_attach)
                            .count()))));
      }
#endif
      bool wake_thread = false;
      if ((cur_thread != nullptr) &&
          (cur_thread->client_low_level_cntx != nullptr) &&
          cur_thread->stalled) {
        /*
          Deassign the waiting thread since the thread is now stalled
          and wake up a thread to ensure that someone takes up the role
          of being the waiting thread.
        */
        DBUG_PRINT(
            "tp",
            ("Wake thread to take over waiting thread in Thread group id %d",
             cur_group->group_idx));
        cur_group->waiting_thread = nullptr;
        wake_thread = true;
      }

      check_trans_queue_for_prio_kickups(cur_group, new_10ms,
                                         stall_check_timpt);
      if (change_active_threads > 0 || (cur_group->threads_for_consumer == 0 &&
                                        cur_group->threads_for_reserve == 0 &&
                                        is_query_ready_to_process(cur_group))) {
        /*
          With the change in max_active_threads it might be credit available
          to start up new jobs and we might be missing threads to execute
          them, so wake a thread and start one up if necessary.

          It's necessary to be conservative here about when to start a new
          thread. If we start a new thread to often, or even wake a thread
          up too often, we quickly lose CPU cache benefits, an earlier
          variant of this condition lost 15% in performance simply by
          waking threads up to frivolously.
        */
        DBUG_PRINT("tp", ("Wake ready query in Thread group id %d",
                          cur_group->group_idx));
        wake_thread = true;
      } else if (is_credit_available(cur_group) &&
                 cur_group->waiting_thread == nullptr &&
                 thread_pool_algorithm == HIGH_CONCURRENCY_ALGORITHM) {
        /*
          All threads in the thread group which are active are also
          stalled, also there is no one waiting for new events. Thus
          we need to wake a thread up to handle waiting for new events.
          Only needed for high_concurrency_algorithm which is a more
          aggressive algorithm.
        */
        DBUG_PRINT("tp", ("Wake up waiting thread in Thread group id %d",
                          cur_group->group_idx));
        wake_thread = true;
      } else if (configured_max_transactions_limit_per_group(cur_group) > 0 &&
                 is_query_ready_to_process(cur_group)) {
        // This will catch the case when MTL has been increased and thereby
        // making a query which was waiting for credit able to start processing.
        // It is perhaps overly aggressive, as it will wake up threads also when
        // there has been no change in MTL and this may disturb the logic for
        // waking threads in the algorithms themselves.
        // This will need to be revisited if it becomes possible to change mtl
        // from a non-zero value to 0.
        wake_thread = true;
      }
      if (wake_thread) {
        /* Update statistics on number of wake ups from stall checker */
        cur_group->stats.wake_thread_stall_checker++;
        tp_wake_thread(cur_group, WAKE_OR_CREATE_ONE);
        actions_taken++;
      }
      terminate_expired_connections(cur_group, stall_check_timpt);

      // Accumulate connection statistics
      if (must_emit_connections_report) {
        if (cur_group == admin_thread_group)
          conn_tracker.current_snap()->collect_admin_group(*cur_group);
        else
          conn_tracker.current_snap()->collect_user_group(*cur_group);
      }
      mysql_mutex_unlock(&cur_group->LOCK_group);
    }  // for all thread groups

    // Emit periodic message to the error log about the number of connections
    // managed by the thread pool
    if (must_emit_connections_report) {
      conn_tracker.emit_connection_report(stall_check_timpt);
    }  // if (must_emit_connections_report)

    sleep_stall_check(actions_taken, &wakeup_reason);
  }  // while - main forever loop
  return nullptr;
}

/**
  Assign the connection to a thread group. This is actually a preliminary
  assignement, based only on the RR algorithm. If the privileges (only
  available after creating the THD) indicate that this is an admin connection
  it will be reassigned to the admin thread group in
  create_thd_and_authenticate_conn(). So we cannot update connection
  counts for the thread group here.
  Maintains a static variable which holds the next index into the thread group
  list, and which is incremented modulo tp_group in each call. This ensures
  that connections are assigned to thread groups in round robin fashion.
  (As it happens, this function DOES get called from different threads on
  Windows, so there we need to use an atomic).

  @retval Pointer to thread group the connection is assigned to
*/
static tp_group_t *assign_connection_to_thread_group() {
  DBUG_TRACE;
#ifdef _WIN32
  static std::atomic_int next_group_idx = -1;
#else
#ifndef NDEBUG
  // Verify that this function is only ever called by a single thread (the
  // thread listening to the socket).
  static auto tid = std::this_thread::get_id();
  assert(std::this_thread::get_id() == tid);
#endif /* !NDEBUG */
  // Index which incremented on each call. Since this function is only ever
  // called by one thread (see assert above), we can use an ordinary static
  // variable without synchronization.
  static int next_group_idx = -1;
#endif /* _WIN32 */
  next_group_idx = (next_group_idx + 1) % tp_normal_groups;
  DBUG_LOG("tp", "Assigning connection to thread group: " << next_group_idx);
  return tp_group_list + next_group_idx;
}

/**
  Signal stall check thread and wait for its termination.
*/
static void shutdown_stall_check_thread() {
  assert(stall_check_initialized);

  /* Wait for stall check thread to stop */
  mysql_mutex_lock(&tp_LOCK_stall_check);
  mysql_cond_broadcast(&tp_COND_stall_check);
  while (stall_checker_state != Stall_checker_state::TERMINATED) {
    (void)mysql_cond_wait(&tp_COND_stall_check, &tp_LOCK_stall_check);
  }
  mysql_mutex_unlock(&tp_LOCK_stall_check);

  mysql_mutex_destroy(&tp_LOCK_stall_check);
  mysql_cond_destroy(&tp_COND_stall_check);
  stall_check_initialized = false;
}

/**
  Signal all threads in the thread groups to stop and wait
  until all connection threads terminate.
*/
static void shutdown_thread_groups(uint num_thread_groups) {
  assert(std::this_thread::get_id() == plugin_init_tid);

  /* Signal all threads in all groups to stop, including the admin thread group.
   */
  for (uint i = 0; i < num_thread_groups; i++) {
    tp_group_t *cur_group = &(tp_group_list[i]);
    assert(std::none_of(std::begin(cur_group->group_threads),
                        std::end(cur_group->group_threads),
                        [](const tp_thread_t &thr) {
                          return thr.thread_handle.thread == my_thread_self();
                        }));

    mysql_mutex_lock(&cur_group->LOCK_group);
    mysql_cond_broadcast(&(cur_group->COND_consumer));
    mysql_cond_broadcast(&(cur_group->COND_reserve));
    mysql_cond_broadcast(&cur_group->COND_connect);

    if (cur_group->group_low_level_cntx &&
        cur_group->waiting_thread != nullptr) {
      // All threads will terminate eventually when tp_shutdown has been set to
      // true, but this could speed up the process by terminating any active
      // fd-waits.
      tp_group_low_level_waiter_wake(cur_group->group_low_level_cntx,
                                     KILL_FLAG);
      cur_group->waiting_thread = nullptr;
    }
    mysql_mutex_unlock(&cur_group->LOCK_group);
  }

  /* Wait for connection threads in thread groups to stop. Including the admin
   * thread group. */
  for (uint i = 0; i < num_thread_groups; i++) {
    tp_group_t *cur_group = &(tp_group_list[i]);
    mysql_mutex_lock(&cur_group->LOCK_group);
    while (cur_group->threads_initialized) {
      (void)mysql_cond_wait(&cur_group->COND_group, &cur_group->LOCK_group);
    }
    mysql_mutex_unlock(&cur_group->LOCK_group);
  }
}

/**
  Deinitialize and free the allocated memory associated with
  num_thread_groups specified by an argument to this function.

  @param  num_thread_groups  number of thread groups which need to be
                             deinitialized.
*/
static void deinit_thread_groups(uint num_thread_groups) {
  uint i, j;
  tp_thread_t *cur_thread;
  tp_group_t *cur_group;

  /* Free memory attached to each thread group */
  // Iterate over all, including admin group
  for (i = 0; i < num_thread_groups; i++) {
    cur_group = &(tp_group_list[i]);

    for (j = 0; j < MAX_THREADS_PER_GROUP; j++) {
      cur_thread = &(cur_group->group_threads[j]);
      cur_thread->thread_handle.thread = null_thread_initializer;
      cur_thread->thread_tp_group = GROUP_ID_GARBAGE;
    }

    if (cur_group->group_low_level_cntx) {
      tp_group_low_level_end(cur_group->group_low_level_cntx);
    }
    mysql_mutex_destroy(&cur_group->LOCK_group);
    mysql_mutex_destroy(&cur_group->LOCK_connect);
    mysql_mutex_destroy(&cur_group->LOCK_query_threads_count);
    mysql_cond_destroy(&cur_group->COND_group);
    mysql_cond_destroy(&cur_group->COND_consumer);
    mysql_cond_destroy(&cur_group->COND_reserve);
    mysql_cond_destroy(&cur_group->COND_connect);
  }
}

/* API (exported) Functions */

/**
  An API call to the thread pool.  Call this to shutdown the thread pool.

  @note
    Called when the mysql server is shutting down.
    This will:
      - Shutdown all threads in all thread pool groups
      - Wait for all threads in all thread pool groups to exit
      - Reset memory used to track thread pool threads
      - Destroy all condition variables and mutexes used by the thread pool

  May, in fact, be called even if thd_pool_init() has not yet been called. E.g.
  by the Plugin_connection_handler if an error is detected in
  thread_pool_plugin_init().
*/
static bool thd_pool_init_completed = false;
void thd_pool_end() {
  DBUG_TRACE;
  assert(std::this_thread::get_id() == plugin_init_tid);

  // This check is necessary because thd_pool_end() is called as a cleanup
  // function also before thd_pool_init() has completed. Notably
  // my_connection_handler_reset() calls it and this function is called to
  // clean up after failures in thread_pool_plugin_init() before
  // thd_pool_init() has been called.
  if (thd_pool_init_completed == false) return;

  assert(thd_pool_init_completed);

  assert(tp_shutdown.load() == false);
  tp_shutdown.store(true);

  // Signal and wait for stall check thread to stop. Destroy its mutex and cond
  // var.
  shutdown_stall_check_thread();

  // Wait for all threads in thread groups to exit.
  shutdown_thread_groups(tp_groups);

  // Free memory attached to each thread group, including the admin thread
  // group.
  deinit_thread_groups(tp_groups);
}

/**
  An API call to the thread pool.  Call this to initialize the thread pool.

  @retval          true           return true on thread pool initialization
  failure.
  @retval          false          return false on success.

  @note
    Called during the mysql server's startup/initialization.
    This will:
      - Call the PSI initialization interface for the thread pool
      - Sanitize the global thread_pool_size
      - Determine how many groups the thread pool should use
      - Determine how many threads per group the thread pool should use
      - Initialize all condition variables and mutexes used by the thread pool
      - Initialize all groups
      - Start the processing threads
      - Wait for the processing threads to be ready
      - Start a background thread to check for stalls
*/
bool thd_pool_init() {
  DBUG_TRACE;

  uint tp_groups_initialized = 0;
  auto init_guard = create_scope_guard([&] {
    tp_shutdown.store(true);
    shutdown_thread_groups(tp_groups_initialized);
    deinit_thread_groups(tp_groups_initialized);
  });

  if (init_tp_psi_keys()) {
    return true;
  }

  assert(thread_pool_size <= MAX_NORMAL_THREAD_GROUPS && thread_pool_size > 0);
  tp_normal_groups = thread_pool_size;
  tp_groups = tp_normal_groups + MAX_ADMIN_THREAD_GROUPS;
  admin_thread_group =
      MAX_ADMIN_THREAD_GROUPS > 0 ? tp_group_list + tp_normal_groups : nullptr;

  tp_thread_create_control_ctx.total_query_threads = 0;
  tp_thread_create_control_ctx.vcpu_count = my_num_vcpus();

  const int tgs = static_cast<int>(thread_pool_size);
  max_connect_threads_per_group =
      std::max(LOWEST_MAX_CONNECT_THREADS_PER_GROUP,
               (LOWEST_MAX_CONNECT_THREADS / tgs) +
                   (LOWEST_MAX_CONNECT_THREADS % tgs == 0 ? 0 : 1));

  /* Initialize the thread tp_groups, including the admin thread group. */
  for (uint i = 0; i < tp_groups; i++) {
    tp_group_t *cur_group = &(tp_group_list[i]);

    new (cur_group) tp_group_t;

    // Initialize group low-level context
    cur_group->group_low_level_cntx = tp_group_low_level_init(cur_group);

    if (cur_group->group_low_level_cntx == nullptr) {
      LogErr(ERROR_LEVEL, ER_TRHEAD_POOL_LOW_LEVEL_INIT_FAILED);
      // Let the scope guard know how many thread groups need to be
      // deinitialized.
      tp_groups_initialized = i;
      return true;
    }

    /* Initialize certain group members */
    cur_group->group_idx = i;
    cur_group->open_connections.clear();
    cur_group->queued_queries.clear();
    cur_group->queued_trans.clear();
    cur_group->connection_context_queue.clear();
    mysql_mutex_init(key_LOCK_group, &cur_group->LOCK_group,
                     MY_MUTEX_INIT_FAST);
    mysql_mutex_init(key_LOCK_connect, &cur_group->LOCK_connect,
                     MY_MUTEX_INIT_FAST);
    mysql_mutex_init(PSI_NOT_INSTRUMENTED, &cur_group->LOCK_query_threads_count,
                     MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_COND_group, &cur_group->COND_group);
    mysql_cond_init(key_COND_consumer, &cur_group->COND_consumer);
    mysql_cond_init(key_COND_reserve, &cur_group->COND_reserve);
    mysql_cond_init(key_COND_connect, &cur_group->COND_connect);

    for (uint j = 0; j < MAX_THREADS_PER_GROUP; j++) {
      tp_thread_t *cur_thread = &(cur_group->group_threads[j]);
      cur_thread->numeric_id = j;
      cur_thread->thread_tp_group = GROUP_ID_GARBAGE;
    }
  }
  tp_groups_initialized = tp_groups;

  /* Start the threads in each group */
  const uint threads_per_group = thread_pool_query_threads_per_group;
  for (uint i = 0; i < tp_groups; i++) {
    tp_group_t *cur_group = &(tp_group_list[i]);
    cur_group->threads_user_request = threads_per_group;
    cur_group->max_active_threads = threads_per_group;

    /* Start an appropriate set of threads to start with */
    mysql_mutex_lock(&cur_group->LOCK_group);
    auto lck_grd =
        create_scope_guard([&] { mysql_mutex_unlock(&cur_group->LOCK_group); });

    uint threads_created = 0;
    if (thread_pool_dedicated_listeners) {
      threads_created = tp_create_worker_threads(
          cur_group, 1, Worker_thread_type::LISTENER_WORKER_THREAD);
      assert(threads_created == 1);
      threads_created =
          tp_create_worker_threads(cur_group, cur_group->threads_user_request,
                                   Worker_thread_type::QUERY_WORKER_THREAD);
    } else {
      threads_created =
          tp_create_worker_threads(cur_group, cur_group->threads_user_request,
                                   Worker_thread_type::QUERY_WORKER_THREAD);
    }
    assert(threads_created == cur_group->threads_user_request);

    cur_group->num_query_threads += threads_created;
    tp_thread_create_control_ctx.total_query_threads += threads_created;
    cur_group->last_thread_creation_time = my_micro_time();

    // create connect handler threads.
    uint connect_handler_threads_created = tp_create_worker_threads(
        cur_group, INITIAL_CONNECT_THREADS_PER_GROUP,
        Worker_thread_type::CONNECTION_HANDLER_WORKER_THREAD);
    cur_group->connect_threads = INITIAL_CONNECT_THREADS_PER_GROUP;

    uint total_threads_created =
        threads_created + connect_handler_threads_created;

    if (total_threads_created !=
        cur_group->threads_user_request + INITIAL_CONNECT_THREADS_PER_GROUP) {
      // Failed to create the requested number of threads.
      return true;
    }

    /* Wait for the thread to reach its "ready to run" point */
    while (cur_group->threads_initialized < total_threads_created) {
      assert(!is_tp_shutdown());
      (void)mysql_cond_wait(&cur_group->COND_group, &cur_group->LOCK_group);
    }

    DBUG_PRINT("tp", ("Threads: wanted=%d max thread id + 1=%d initialized=%d",
                      cur_group->threads_user_request,
                      cur_group->max_thread_ids_in_group,
                      cur_group->threads_initialized));
  }

  /* Start the background monitoring thread */
  if (tp_create_stall_check_thread()) {
    shutdown_stall_check_thread();
    return true;
  }
  init_guard.release();
  thd_pool_init_completed = true;
  return false;
}

/**
  An API call to the thread pool.  Call this when new client sockets have been
    accepted.

  @param channel_info      Pointer to object containing information
                           about the new connection.

  @note
    Called when the mysql server accepted a new client socket.
    This will:
      - Create the needed PSI context to track the client
      - Initialize THD timestamps: current time, create time, start time
      - Append the THD to the list of threads
      - Pick a group for this client to be a member
      - Prepare stack offset tracking for this thread

      - Call the thread connection setup globals
      - Call thd_prepare_connection()
      - Check for errors in any of the above calls and call thd_cleanup() as
          appropriate and return
      - If no errors in above calls, initialize a context for use in the
          thread pool to track this client/connection

   @retval true  failure
   @retval false success
*/
bool thd_pool_add_connection(Channel_info *channel_info) {
  DBUG_TRACE;

  // Will assign connections on the admin interface directly to the
  // admin thread group to prevent contention with other connections.
  // Connections on the normal interface may be reassigned to the admin thread
  // group when the THD has been created and it becomes possible to check if
  // the connecting user has the TP_CONNECTION_ADMIN privilege.
  tp_group_t *my_tp_group = (channel_info->is_admin_connection()
                                 ? admin_thread_group
                                 : assign_connection_to_thread_group());
  assert(my_tp_group != nullptr);
  assert(channel_info->is_admin_connection() ||
         my_tp_group != admin_thread_group);

  connection_context_t *connection_context =
      new (std::nothrow) connection_context_t(channel_info);
  if (connection_context == nullptr) {
    return true;
  }

  {
    mysql_mutex_lock(&my_tp_group->LOCK_connect);
    auto unlock_guard = create_scope_guard([&]() {
      DBUG_EXECUTE_IF("verify_no_pop_without_lock", {
        // Trap the connection handler after it wakes up from cond wait,
        // but before it can enter its main loop
        assert(my_tp_group->num_connect_handler_thread_in_sleep > 0);
        mysql_mutex_lock(&my_tp_group->LOCK_group);
      });
      mysql_mutex_unlock(&my_tp_group->LOCK_connect);
      DBUG_EXECUTE_IF("verify_no_pop_without_lock", {
        // This should ensure that the conn thread has woken up and
        // blocked on LOCK_group
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
        // So we can lock LOCK_connect again
        mysql_mutex_lock(&my_tp_group->LOCK_connect);
        // The connect thread stll has not reached the point where it
        // pops the queue, so this should still hold.
        assert(!my_tp_group->connection_context_queue.is_empty());
        // Now we unblock the conn thread
        mysql_mutex_unlock(&my_tp_group->LOCK_group);
        // and wait to ensure that it has tried to pop
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
        // It should not succeed as long as we hold LOCK_connect, so
        // the following should still hold.
        assert(my_tp_group->connection_context_queue.is_empty() == false);
        mysql_mutex_unlock(&my_tp_group->LOCK_connect);
      });
    });
    my_tp_group->connection_context_queue.push_back(connection_context);
    ++opt_option_tracker_usage_thread_pool_plugin;

    if (my_tp_group->num_connect_handler_thread_in_sleep > 0) {
      mysql_cond_signal(&my_tp_group->COND_connect);

      return false;
    }
  }  // End of scope holding LOCK_connect

  // Create worker thread to handle connection event.
  tp_create_connect_thread(my_tp_group);
  DBUG_PRINT("tp",
             ("Added connection is_admin_connection=%d to grp=%u",
              channel_info->is_admin_connection(), my_tp_group->group_idx));
  return false;
}

/**
   Create and intializat THD context. Authenticate the
   incoming connection request from client.

   @retval true  Failure.
   @retval flase Success.
*/
static bool create_thd_and_authenticate_conn(
    connection_context_t *conn_cntx, tp_group_t *my_tp_group,
    tp_thread_t *conn_handler_thread_data) {
  mysql_mutex_assert_not_owner(&my_tp_group->LOCK_group);
  DBUG_TRACE;
  auto time_of_pop = tp_now();
  tp_client_low_level_t *client_cntx;
  bool prepare_rc;

  PSI_thread *psi [[maybe_unused]] = nullptr;

#ifdef HAVE_PSI_THREAD_INTERFACE

  /*
    Thread pool uses separate PFS instrumentations to run thread pool code and
    the user session. User session runs under the 'user job instrumentation'
    in order to account all the PFS events related to user session(/THD)
    together and to reclaim memory allocated for the PFS events at the end of
    session.

    User job instrumentation is created and used in the following order,

      *) Create instrumentation for a user session.
      *) Instantiate THD for a user session.
      *) Associate user job instrumentation to the THD.
      *) Switch from thread pool instrumentation to user job instrumentation to
         run user session statements.
      *) At the end of user session, cleanup THD.
      *) Remove user job instrumentations.

    Thread pool code always runs under the 'thread pool worker instrumentation'.
  */

  // Save connection handler worker thread instrumentation.
  THR_PSI_backup = PSI_THREAD_CALL(get_thread)();

  // PSI context for the user job instrumentation.
  psi = PSI_THREAD_CALL(new_thread)(key_tp_one_connection, 0, nullptr, 0);

  /* Run under the user job instrumentation ... */
  PSI_THREAD_CALL(set_thread)(psi);
#endif /* HAVE_PSI_THREAD_INTERFACE */

  // New connection request entering authentication.
  connections_entering_auth++;

  // Connection request exiting authentication upon function scope exit.
  auto exit_auth = create_scope_guard([&]() { connections_exiting_auth++; });

  DBUG_EXECUTE_IF("sleep_before_auth", my_sleep(5 * 1000 * 1000););

  auto channel_info = conn_cntx->m_channel_info;
  THD *thd = create_thd(channel_info);
  if (thd == nullptr) {
    dec_connection_count();
    /* Restore the thread pool instrumentation. */
    set_tp_psi_env();
    clear_user_psi_env(psi);
    LogErr(WARNING_LEVEL, ER_THREAD_POOL_ALLOC_FAILED, "THD");

    /*
      If we simluate a resource failure to test log throttling, we also
      emit the rest of the throttled error codes.
    */
    DBUG_EXECUTE_IF(
        "emit_throttled_msg",
        LogErr(WARNING_LEVEL, ER_THREAD_POOL_SOCKETPAIR_FAILED, 0);
        LogErr(WARNING_LEVEL, ER_THREAD_POOL_LOW_LEVEL_INIT_FAILED);
        LogErr(WARNING_LEVEL, ER_THREAD_POOL_LOW_LEVEL_ARM_FAILED); LogErr(
            WARNING_LEVEL, ER_THREAD_POOL_LOW_LEVEL_ARM_FAILED_WITH_ERRNO, 0);
        LogErr(WARNING_LEVEL, ER_THREAD_POOL_CREATE_THREAD_FAILED, 0, 0);
        LogErr(WARNING_LEVEL, ER_THREAD_POOL_LOW_LEVEL_INIT_ALLOC_FAILED);
        LogErr(WARNING_LEVEL, ER_THREAD_POOL_CREATE_EPOLL_FAILED, 0);
        LogErr(WARNING_LEVEL, ER_THREAD_POOL_EPOLL_WAIT_ERROR, 0);
        LogErr(WARNING_LEVEL, ER_THREAD_POOL_POLL_WAIT_ERROR, 0););

    return true;
  }
  thd_init(thd, (char *)&client_cntx);

#ifdef HAVE_PSI_THREAD_INTERFACE
  // Set THD information in PSI context and vice versa.
  PSI_THREAD_CALL(set_thread_THD(psi, thd));
  PSI_THREAD_CALL(set_thread_id(psi, thd->thread_id()));
  mysql_socket_set_thread_owner(thd_get_mysql_socket(thd));
  thd->set_psi(psi);
  // FIXME: PSI_THREAD_CALL(detect_telemetry)(psi);
#endif

  /*
    ... so that thd_prepare_connection() tags the user job
    with account information (processlist_user, processlist_host),
    instead of changing account info for this thread (main).
  */
  prepare_rc = thd_prepare_connection(thd);
  /* Restore the thread pool instrumentation. */
  set_tp_psi_env();

  if (prepare_rc) {
    increment_aborted_connects();
    tp_thd_cleanup(my_tp_group, nullptr, thd, true, 0);
    return true;
  }

  // Cleanup and return if shutdown is initiated or if the connection is
  // killed.
  //
  // It can happen that new THDs are added to Global_THD_manager after
  // the signal handler thread forcefully closes existing connections.
  // In such cases, there will be no one to signal the new THDs added
  // resulting in shutdown waiting forever. Checking
  // connection_events_loop_aborted() and performing cleanup here
  // ensures that THDs are removed from the Global_THD_manager and
  // server shutdown proceeds gracefully.
  if (!thd_connection_alive(thd) || connection_events_loop_aborted()) {
    tp_thd_cleanup(my_tp_group, nullptr, thd, true, 0);
    return true;
  }

  // Diassociate thread globals associated.
  reset_thread_globals(thd);

  // We need to simulate a read of the net for SHOW PROCESSLIST
  // while we're waiting for external messages
  thd_set_net_read_write(thd, 1);

  bool has_tp_connection_admin =
      (!skip_grant_tables() &&
       thd->security_context()
           ->has_global_grant(&TP_CONNECTION_ADMIN.front(),
                              TP_CONNECTION_ADMIN.length())
           .first);
  DBUG_PRINT("tp",
             ("admin_con:%d, TPCA:%d", channel_info->is_admin_connection(),
              has_tp_connection_admin));

  assert(channel_info->is_admin_connection() == false ||
         my_tp_group == admin_thread_group);

  // has_tp_connection_admin can be true even if the TP_CONNECTON_ADMIN
  // privilege was not created by the thread_pool. see --skip-grant_tables
  // testcase in thread_pool_admin_interface.test
  if (has_tp_connection_admin) {
    my_tp_group = admin_thread_group;
    DBUG_PRINT("tp",
               ("Found TP_CONNECTION_ADMIN and using admin thread group"));
  }

  client_cntx =
      tp_client_low_level_init(thd, my_tp_group->group_low_level_cntx);

  if (client_cntx == nullptr) {
    LogErr(WARNING_LEVEL, ER_THREAD_POOL_LOW_LEVEL_INIT_FAILED);
    tp_thd_cleanup(my_tp_group, client_cntx, thd, true, 0);
    return true;
  }
  client_cntx->time_of_add = conn_cntx->time_of_add;
  client_cntx->time_of_pop = time_of_pop;
  client_cntx->conn_handler_index = conn_handler_thread_data->numeric_id;
  if (channel_info->is_admin_connection()) {
    client_cntx->type = Connection_type::Admin_interface;
  } else if (has_tp_connection_admin) {
    client_cntx->type = Connection_type::Admin_privilege;
  } else {
    assert(client_cntx->type == Connection_type::User);
  }

  /*
    We have successfully created a client context for this
    connection. We will now add this client context (which
    represents the connection to the list of open connections
    in the thread group). This makes it possible to search
    the list of open connections to find any connections which
    needs to be killed.
  */
  thd_lock_data(thd);
  mysql_mutex_lock(&my_tp_group->LOCK_group);
  auto lock_guard = create_scope_guard([&]() {
    thd_unlock_data(thd);
    mysql_mutex_unlock(&my_tp_group->LOCK_group);
  });

  // Cannot do this until we have determined actual thread group for the
  // connection, AND hold LOCK_group.
  // (Moved from assign_connection_to_thread_group()).
  my_tp_group->stats.connection_count++;
  my_tp_group->stats.connections_started++;

  thd_set_scheduler_data(thd, client_cntx);
  insert_open_connection(my_tp_group, client_cntx);
  /*
    Set connection to active to avoid that someone else starts
    handling connection before we've completed the setup of it.
  */
  client_cntx->active_flag = 1;
  DBUG_EXECUTE_IF("thd_killed_after_auth",
                  { thd->killed = THD::KILL_CONNECTION; });
  DBUG_PRINT("tp",
             ("Arming connection id %lu", thd_get_thread_id((MYSQL_THD)thd)));
  if (thd_killed(thd) || tp_client_low_level_arm(client_cntx) != 0) {
    if (!thd_killed(thd))
      LogErr(WARNING_LEVEL, ER_THREAD_POOL_LOW_LEVEL_ARM_FAILED);
    // Reset lock_guard before cleanup.
    lock_guard.reset();
    tp_full_cleanup(thd, nullptr, client_cntx, my_tp_group, 0);
    return true;
  }

  client_cntx->time_of_arm = tp_now();
  client_cntx->conn_state = Connection_state::ARMED;
  assert(client_cntx->processing_thread == nullptr);
  client_cntx->active_flag = 0;
  set_idle_timeout(my_tp_group, client_cntx);

  return false;
}

/*
  An API call to the thread pool.  Call this before a client socket is
    shutdown due to an error or a kill.

  @param thd      Connection handle

  @note
    Called when the mysql server decides to shutdown a socket due to an error
      or a kill.  The thread pool will NOT use or watch the socket for input
      any longer.  A state variable may be set that the thread pool low level
      will use later to do final cleanup.
    This will:
      - Cleanup the thread pool context for the client/socket

    thd->LOCK_thd_data is held when this function is called (thd_lock_data
    is called from this plugin to lock this).
*/

void thd_pool_post_kill_notification(THD *thd) {
  tp_client_low_level_t *client_cntx;
  tp_group_t *my_tp_group;

  DBUG_TRACE;
  DBUG_PRINT("tp_enter",
             ("close connection id %lu", thd_get_thread_id((MYSQL_THD)thd)));

  /*
    The lock on LOCK_thd_data ensure that nobody releases the
    connection object while we're operating on it from a thread
    other than the thread where it is currently executing from
    which it can at any time be closed and released.

    We need the object to find the thread group this connection
    is using. This function is called while holding the
    LOCK_thd_data mutex.
  */
  client_cntx = (tp_client_low_level_t *)thd_get_scheduler_data(thd);
  if (client_cntx != nullptr) {
    /*
      Wake up thread group waiter thread. When it receives
      this wake up call, it will check for killed connections.
    */
    my_tp_group = tp_group_low_level_get_tp_group(client_cntx->group);
    DBUG_PRINT("tp", ("Thread group id = %d", my_tp_group->group_idx));
    mysql_mutex_lock(&my_tp_group->LOCK_group);
    if (client_cntx->connection_killed_state != TP_CONNECTION_NOT_KILLED) {
      /*
        Some other connection already started the KILL handling, the code
        is designed only for one kill and that this should be successfully
        done.
      */
      DBUG_PRINT("tp", ("Connection already killed"));
      mysql_mutex_unlock(&my_tp_group->LOCK_group);
      return;
    }
    client_cntx->connection_killed_state = TP_CONNECTION_KILLED_WAITING_FLAG;
    client_cntx->conn_state = Connection_state::KILLED;
    bool send_kill_flag = (my_tp_group->is_kill_request_pending == false);
    my_tp_group->is_kill_request_pending = true;
    if (send_kill_flag) {
      DBUG_LOG("tp_kill", "Sending kill flag for connection id "
                              << thd_get_thread_id(thd) << " in thread group "
                              << my_tp_group->group_idx);
    }
    mysql_mutex_unlock(&my_tp_group->LOCK_group);

    DBUG_EXECUTE_IF("sleep_after_connection_killed", my_sleep(6000000););
    if (send_kill_flag) {
      [[maybe_unused]] int s = tp_group_low_level_waiter_wake(
          my_tp_group->group_low_level_cntx, KILL_FLAG);
      DBUG_LOG("tp_kill", "Kill flag for connection id "
                              << thd_get_thread_id(thd) << " in thread group "
                              << my_tp_group->group_idx << " sent. send status:"
                              << s << " errno:" << errno);
    }
  }
  DBUG_EXECUTE_IF("thd_pool_post_kill_notification_hang", my_sleep(3000000););
}

/*
  An API call to the thread pool.  Call this when a client/socket command
    is being processed and a stall (long held mutex or disk IO) is about to
    occur.  This notifies the thread pool that an OS thread is about to sleep.

  @param thd       Connection handle. If nullptr is passed, current_thd is used.
  @param wait_type An enum value from the enum thd_wait_type (defined
                   in include/mysql/service_thd_wait.h) but passed as int to
                   preserve compatibility with exported service api.

  @note
    Called when the mysql server or storage engine is about to stall
    This will:
      - Adjust the active thread count so other threads may start
      - Possibly wake more threads to process new requests

    This may wake another OS thread
*/
void thd_pool_wait_begin(THD *thd, int wait_type) {
  tp_client_low_level_t *client_cntx;
  tp_group_t *my_tp_group;
  tp_thread_t *my_thread_data;
  wake_level level = WAKE_IF_CREATED;
  DBUG_TRACE;

  if (thd == nullptr) {
    thd = thd_get_current_thd();
    if (thd == nullptr) {
      return;
    }
  }

  client_cntx = (tp_client_low_level_t *)thd_get_scheduler_data(thd);
  if (client_cntx != nullptr) {
    assert(client_cntx->processing_thread != nullptr);
    my_tp_group = tp_group_low_level_get_tp_group(client_cntx->group);
    my_thread_data = client_cntx->processing_thread;
    // FIXME.bug#34723119: Uncomment the DEBUG_EXECUTE_IF below and
    // corresponding lines in thread_pool.par_add_index to be able to reproduce
    // problem.
    // DBUG_EXECUTE_IF("make_sure_wait_beging_invoked_by_worker",
    //    { assert(my_thread_self() == my_thread_data->thread_handle.thread);
    //    });

    // Can only call this from the original tp_worker_thread
    // TODO.bug#34723119: Once this bug is fixed, we should be able to add
    // an assert for this
    if (my_thread_self() != my_thread_data->thread_handle.thread) return;

    // thd_wait_begin() must not be called again without an intervening
    // thd_wait_end()
    assert(client_cntx->wait_type == THD_WAIT_NONE);
    if (client_cntx->wait_type > THD_WAIT_NONE) return;

    client_cntx->wait_type = static_cast<THD_wait_type>(wait_type);

    DBUG_PRINT(
        "tp_enter",
        ("Begin wait for connection on client_cntx %p"
         " in Thread group id %d, with worker thread active for %lld"
         " microseconds",
         client_cntx, my_tp_group->group_idx,
         static_cast<long long>(std::chrono::duration_cast<Misec>(
                                    tp_now() - my_thread_data->time_of_attach)
                                    .count())));

    if (!my_thread_data->stalled) {
      my_thread_data->not_stalled = true;
      dec_active_threads(my_tp_group, my_thread_data);
    }
    /* Lock the group so I can make group level updates */
    mysql_mutex_lock(&my_tp_group->LOCK_group);

    if (my_tp_group->waiting_thread == my_thread_data) {
      /*
        Since I'm stalling, I'll no longer be the waiting thread.
        It's important to create a thread to avoid a deadlock here
        since we have no check in stall check thread for a missing
        waiter thread.
      */
      my_tp_group->waiting_thread = nullptr;
      level = WAKE_OR_CREATE_ONE;
      /*
        To support test of KILL we introduce a delay that holds
        off on starting up the new waiter thread to ensure that we
        have sufficient time to send off a number of queries on a
        number of connections, including the byte sent on the
        notify connection to start up a KILL operation.
      */
      DBUG_PRINT("tp", ("remove me from waiting_thread"));
      DBUG_EXECUTE_IF("wait_begin_waiter_thread", my_sleep(3000000););
    }

    /* Update statistics on thread going to sleep waiting for an event */
    assert(wait_type < THD_WAIT_LAST);
    my_tp_group->stats.wait_counts[wait_type]++;

    /*
      Wake another thread to see if necessary to handle waiting thread or there
      are queries ready to execute. Use a reserve thread if necessary. It is
      essential for performance to ensure a thread gets woken up here
      unconditionally to check if anything needs to be done to get things going
      again.

      The most important thing here is not really to wake the thread up. It's
      quite likely that it will go to sleep again. However even if this happens
      it will move from the reserve thread pool to the consumer pool of threads
      very likely. This is needed here to avoid that we decrease the concurrency
      level on the thread group. This is why we need to make the wake up
      unconditional here, it's more about moving the thread to the consumer
      pool more than about waking a thread up. If no thread exists it will be
      created by the stall check thread.
    */
    tp_wake_thread(my_tp_group, level);
    mysql_mutex_unlock(&my_tp_group->LOCK_group);
  }
}

/*
  An API call to the thread pool.  Call this when a client/socket command
    is continuing after a stall (long held mutex or disk IO).
    This notifies the thread pool that an OS thread is active again.

  @param thd      Connection handle

  @note
    Called when the mysql server or storage engine is about to continue
      after a stall.
    This will:
      - Adjust the active thread count
*/
void thd_pool_wait_end(THD *thd) {
  tp_client_low_level_t *client_cntx;
  tp_group_t *my_tp_group;
  tp_thread_t *my_thread_data;

  DBUG_TRACE;

  if (thd == nullptr) {
    thd = thd_get_current_thd();
    if (thd == nullptr) {
      return;
    }
  }

  client_cntx = (tp_client_low_level_t *)thd_get_scheduler_data(thd);

  if (client_cntx != nullptr) {
    assert(client_cntx->processing_thread != nullptr);
    my_tp_group = tp_group_low_level_get_tp_group(client_cntx->group);
    my_thread_data = client_cntx->processing_thread;
    // Can only call this from the original tp_worker_thread
    // TODO.bug#34723119: Once this bug is fixed, we should be able to add
    // an assert for this.
    if (my_thread_self() != my_thread_data->thread_handle.thread) return;

    // thd_wait_end() must always have been preceded by thd_wait_begin()
    assert(client_cntx->wait_type > THD_WAIT_NONE);

    DBUG_PRINT("tp", ("End wait for connection on client_cntx %p"
                      " in Thread group id %d, stalled %d",
                      client_cntx, my_tp_group->group_idx,
                      my_thread_data->not_stalled));

    if (my_thread_data->not_stalled) {
      my_thread_data->not_stalled = false;
      inc_active_threads(my_tp_group, my_thread_data);
    }
    client_cntx->wait_type = THD_WAIT_NONE;
  }
}

/**
  Read all wake flags received

  @param group_cntx        Low level group context
  @param client_cntx_array Array of low level client contexts
  @param added_events      Number of events added
*/
void waiter_flush(tp_group_low_level_t *group_cntx,
                  tp_client_low_level_t **client_cntx_array, int *added_events,
                  my_socket read_fd) {
  uint len;
  char msg[MAX_THREADS_PER_GROUP];
  tp_group_t *my_tp_group;
  DBUG_TRACE;

  my_tp_group = tp_group_low_level_get_tp_group(group_cntx);

#ifndef NDEBUG
  if (my_tp_group->stop_in_waiter_flush) {
    DBUG_LOG("tp_kill", "Waiting for stop_in_waiter_flush to become false.");
    while (my_tp_group->stop_in_waiter_flush) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    DBUG_LOG("tp_kill", "Waking up, stop_in_waiter_flush is now false.");
  }
#endif /* NDEBUG */

  /* Read many bytes from the notify descriptor */
  len = recv(read_fd, msg, MAX_THREADS_PER_GROUP, 0);

  DBUG_LOG("tp_kill", "Draining " << len
                                  << " bytes from socketpair in thread group "
                                  << my_tp_group->group_idx);

  tp_group_low_level_waiter_flush(group_cntx);

  handle_killed_connections(my_tp_group, len, msg, client_cntx_array,
                            added_events);
}

bool in_highly_concurrent_query_processing_mode() {
  return (thread_pool_algorithm == 1 &&
          thread_pool_query_threads_per_group > 1);
}

/**
  Check if we can process query direct from wait for events handling

  @param             my_tp_group          Thread group data
  @param             events_total         Total number of events received
  @param             allowed_direct       Is it allowed to handle direct
                                          processing

  @retval            true                 Process direct from events handler
  @retval            false                Don't process direct from events
  handler

  @note
    To avoid having to put it in queue and immediately remove it from the
    queue we check if it's ok to process the query directly. This is the
    normal case when the number of active threads is smaller than the number
    of thread groups.

    Since new waiter thread is immediately awakened in highly concurrent
    query processing mode, query is added to queue instead of processing it
    directly.

    We can call is_query_available and is_credit_available without mutex
    since if no query is available and credit is available there is no
    risk that these conditions will change since we are the waiting thread
    when arriving here, and only the waiting thread can add queries and
    start queries such that no credit is available any more.
*/
bool can_process_direct(tp_group_t *my_tp_group, int events_total,
                        bool allowed_direct) {
  if (events_total == 1 && !in_highly_concurrent_query_processing_mode() &&
      !thread_pool_dedicated_listeners && allowed_direct &&
      !is_query_available(my_tp_group) && is_credit_available(my_tp_group)) {
    return true;
  }
  return false;
}

/**
  Insert event into next_events array

  @param      next_events     Array of client contexts that are ready for
                              processing a query
  @param      events_copied   Counter of events copied to array, incremented
                              by the method
  @param      client_cntx     Client context for connection ready to execute
                              query
*/
void insert_next_events(tp_client_low_level_t **next_events, int *events_copied,
                        tp_client_low_level_t *client_cntx) {
  next_events[*events_copied] = client_cntx;
  (*events_copied)++;
}

/**
  Check if we should continue checking for events

  @param           my_tp_group     Thread group data
  @param           my_thread_data  Thread data
  @param           loop            Loop counter

  @retval          true            Continue waiting for events
  @retval          false           Stop waiting for events

  @note
    While not shutting down and while the thread is still a waiting thread.
    While being away to execute a query in handle_single_event we could
    have let go of this role. Also only loop once if we call without wait
    flag set.
*/
bool continue_wait_events(tp_group_t *my_tp_group, tp_thread_t *my_thread_data,
                          int loop) {
  if ((!is_tp_shutdown()) && loop &&
      (my_tp_group->waiting_thread == my_thread_data)) {
    return true;
  }
  return false;
}

#ifndef NDEBUG
/**
  Check if client context is in list of open connections

  @param my_tp_group        My thread group
  @param client_cntx        Client context of new connection

  @retval                   true if client connection is open
*/
static bool is_in_open_connection(tp_group_t *my_tp_group,
                                  tp_client_low_level_t *client_cntx) {
  tp_client_low_level_t *loop_client_cntx;
  Open_connections_list::Iterator it(my_tp_group->open_connections);

  while ((loop_client_cntx = it++)) {
    if (loop_client_cntx == client_cntx) {
      return true;
    }
  }

  return false;
}
#endif

/*
  KILL Query Module
  -----------------
  This module contains methods to handle KILL query_id when a thread pool
  is used. It's invoked from waiter_flush in the epoll/poll wait_for_events
  after thd_pool_post_kill_notification has sent a KILL flag to the notify
  socket.

  It also contains methods called to handle state changes for
  1) New connection
  2) Connection closed
  3) Connection bound to a thread for query execution
  4) Connection unbound from a thread

  These methods are needed to ensure that it's clear whether the thread pool
  should handle the KILL or whether it should be handled by the MySQL Server
  code. If the connection is bound to a thread for query execution then it's
  the responsibility of the MySQL Server to ensure it gets properly killed
  as part of query execution. The final phases of this query execution happens
  inside the thread pool in tp_process_event. If the connection isn't bound
  to a thread, then it's the responsibility of the thread pool to ensure that
  it gets killed properly.

  Event 1) and 2) are needed to be able to know about the connection being
  a part of the thread group. Event 3) and 4) are needed to ensure we know
  whether MySQL Server is responsible or not.
*/

/*
  Insert a new connection into list of open connections in the thread
  group, this list is maintained to enable a kill of the connection when
  the connection is maintained by the thread pool.

  @param my_tp_group        My thread group
  @param client_cntx        Client context of new connection
*/
static void insert_open_connection(tp_group_t *my_tp_group,
                                   tp_client_low_level_t *client_cntx) {
  DBUG_TRACE;

  assert(!is_in_open_connection(my_tp_group, client_cntx));
  my_tp_group->open_connections.push_front(client_cntx);
}

/*
  Remove a connection from the list of open connections in the thread
  group, this list is maintained to enable a kill of the connection when
  the connection is maintained by the thread pool.

  @param my_tp_group        My thread group
  @param client_cntx        Client context of new connection
*/

static void remove_open_connection(tp_group_t *my_tp_group,
                                   tp_client_low_level_t *client_cntx) {
  DBUG_TRACE;

  assert(is_in_open_connection(my_tp_group, client_cntx));
  my_tp_group->open_connections.remove(client_cntx);
}

/**
  Tie a connection (tp_client_low_level_t) to a tp worker thread (tp_thread_t).
  This is the TP eqivalent of attaching a THD to a thread, but this operation
  needs LOCK_group to be locked.
*/
static inline void assign_thread_to_connection(tp_group_t *my_tp_group,
                                               tp_thread_t *my_thread_data) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&my_tp_group->LOCK_group);
  auto &client_cntx = my_thread_data->client_low_level_cntx;
  assert(client_cntx != nullptr);
  assert(my_thread_data->time_of_attach == BEGINNING_OF_EPOCH);
  assert(!my_thread_data->stalled);

  if (client_cntx->processing_thread != nullptr) {
    DBUG_LOG("tp", "WARN: Thread " << X_(my_thread_data)
                                   << " steals connection from "
                                   << X_(client_cntx->processing_thread));
  }
  client_cntx->processing_thread = my_thread_data;
  client_cntx->conn_state = Connection_state::ATTACHED;
  my_thread_data->time_of_attach = tp_now();
  inc_active_threads(my_tp_group, my_thread_data);
}

/**
  Un-tie a connection from a worker thread.
  @note Another thread may have assigned itself to this connection in
  the intval between when the connection is re-armed, and when LOCK_group
  is re-acquired. If this happens we must not modify processing_thread.
 */
static inline void unassign_thread_from_connection(
    tp_group_t *my_tp_group, tp_thread_t *my_thread_data) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&my_tp_group->LOCK_group);
  dec_active_threads(my_tp_group, my_thread_data);
  init_process_count_time_unit(my_tp_group, my_thread_data);
  auto &client_cntx = my_thread_data->client_low_level_cntx;
  if (client_cntx != nullptr &&
      client_cntx->processing_thread == my_thread_data) {
    // Still owner of connection context at this point, so set it to nullptr
    // to mark that no thread is currently using it.
    client_cntx->processing_thread = nullptr;
  }
  client_cntx = nullptr;
}

/**
  Bind the connection from the thread group

  @param client_cntx         Client context representing connection

  @retval                    true indicates failure, false success
*/
static inline bool set_connection_active(tp_client_low_level_t *client_cntx) {
  DBUG_TRACE;
  assert_LOCK_group_acquired(client_cntx);

  if (unlikely(client_cntx->connection_killed_state !=
               TP_CONNECTION_NOT_KILLED)) {
    return true;
  }
  if (unlikely(client_cntx->active_flag == 1)) {
    /*
      Another thread has rearmed the connection but hasn't been able to
      set the connection as inactive yet. We will not execute the
      connection, we will mark the connection as ready to execute and
      allow the thread that hasn't set the connection as inactive to
      execute it instead.
    */
    client_cntx->ready_to_execute_flag = true;
    return true;
  }
  client_cntx->active_flag = 1;
  return false;
}

/**
  Unbind the connection from the thread group.
  LOCK_group must be locked prior to calling function, and will be locked
  when the function returns.

  @note LOCK_group may be released and re-acquired inside this function.

  @param my_tp_group         The thread group
  @param my_thread_data      The thread context

  @retval  true              Connection should be re-executed
  @retval  false             Connection need no re-execution
*/
static inline bool set_connection_inactive_with_existing_lock(
    tp_group_t *my_tp_group, tp_thread_t *my_thread_data) {
  DBUG_TRACE;
  mysql_mutex_assert_owner(&my_tp_group->LOCK_group);
  auto assert_is_locked_grd = create_scope_guard(
      [&] { mysql_mutex_assert_owner(&my_tp_group->LOCK_group); });

  auto &client_cntx = my_thread_data->client_low_level_cntx;
  set_idle_timeout(my_tp_group, client_cntx);

  assert(client_cntx->processing_thread != nullptr);
  assert(client_cntx->active_flag == 1);

  DBUG_LOG("tp", X_(client_cntx->connection_id) << X_(
                     killed_state_str(client_cntx->connection_killed_state)));
  switch (client_cntx->connection_killed_state) {
    case TP_CONNECTION_NOT_KILLED:
      if (unlikely(client_cntx->ready_to_execute_flag)) {
        DBUG_LOG("tp", "Re-execute connection "
                           << X_(client_cntx->connection_id) << " due to "
                           << X_(client_cntx->ready_to_execute_flag));
        client_cntx->ready_to_execute_flag = false;
        return true;
      }

      DBUG_LOG("tp", "Normal inactivation after query "
                         << X_(client_cntx->connection_id));

      if (client_cntx->processing_thread == my_thread_data) {
        client_cntx->active_flag = 0;
      }
      client_cntx->time_of_last_event_completion = tp_now();
      DBUG_LOG("tp_conrep",
               "Setting completion time for "
                   << X_(client_cntx->connection_id) << "to "
                   << X_(client_cntx->time_of_last_event_completion)
                   << X_(client_cntx->time_of_last_event_completion
                             .time_since_epoch()
                             .count()));
      break;

    case TP_CONNECTION_KILLED_WAITING_FLAG:
      DBUG_LOG("tp",
               "Connection killed, but other thread will perform cleanup");
      client_cntx->active_flag = 0;
      client_cntx->ready_to_execute_flag = false;
      break;

    case TP_CONNECTION_KILLED_RECEIVED_FLAG:
      DBUG_LOG("tp", "KILL request found when inactivating a connection");
      mysql_mutex_unlock(&my_tp_group->LOCK_group);
      tp_full_cleanup(client_cntx->thd, nullptr, client_cntx, my_tp_group, 0);
      mysql_mutex_lock(&my_tp_group->LOCK_group);
      // Ensure we clear thread client context before we remove client
      // context, otherwise we'll risk reading free'd data in
      // fill_thread_state_table.
      client_cntx = nullptr;
      break;
    default:
      DBUG_LOG("tp", "Unknown connection killed value: "
                         << client_cntx->connection_killed_state);
      assert(false);
  }
  return false;
}

/*
  Called after rearm error to ensure we don't attempt to handle
  error if it is already handled by someone else.

  @param tp_group        The thread group
  @param client_cntx     Client context representing connection

  RETURN VALUE
  @retval                 true, Means we need to handle the error
  @retval                 false, Some other thread already handling error
*/
static bool handle_rearm_error(tp_group_t *tp_group,
                               tp_client_low_level_t *client_cntx) {
  bool ret_code = true;
  DBUG_TRACE;

  mysql_mutex_lock(&tp_group->LOCK_group);
  if (client_cntx->active_flag == 2) {
    ret_code = false;
  } else {
    assert(client_cntx->active_flag == 1);
  }
  mysql_mutex_unlock(&tp_group->LOCK_group);
  return ret_code;
}

/*
  Remove connection from event array

  @param client_cntx            Client context representing connection
  @param client_cntx_array      An array of client contexts received
  @param added_events           Reference to the number of added events
*/

static void remove_connection_from_event_array(
    tp_client_low_level_t *client_cntx,
    tp_client_low_level_t **client_cntx_array, int *added_events) {
  int num_events = *added_events;
  int found = false;
  int i;
  DBUG_TRACE;

  for (i = 0; i < num_events; i++) {
    if (!found) {
      if (client_cntx_array[i] == client_cntx) {
        (*added_events)--;
        DBUG_PRINT("tp", ("Removing killed query from event array"));
        found = true;
      }
    } else {
      client_cntx_array[i - 1] = client_cntx_array[i];
    }
  }
}

/**
  Ensure that someone takes care of killing the connection.

  @param my_tp_group            Thread group of failed connection
  @param client_cntx            Client context for connction
  @param client_cntx_array      An array of client contexts received
  @param[out] added_events           Reference to the number of added events

  @retval                       true, Released thread group mutex
  @retval                       false, Still holds the thread group mutex

  @note
    When entering this method we hold the mutex on the thread group,
    when we leave we return whether we have released the mutex or not.
    To handle the killing of the connection might require releasing the mutex.

    We need to discover if we are responsible to handle the KILL action
    or whether the query is already handled by the MySQL Server and
    can thus be expected to handle it before returning the connection
    to the thread pool.
*/

static inline bool handle_failed_connection(
    tp_group_t *my_tp_group, tp_client_low_level_t *client_cntx,
    tp_client_low_level_t **client_cntx_array, int *added_events) {
  THD *thd;
  bool ret_code = false;
  DBUG_TRACE;

  thd = client_cntx->thd;
  if (thd) {
    DBUG_PRINT("tp", ("connection id = %lu, Thread group id = %d",
                      client_cntx->connection_id, my_tp_group->group_idx));
#ifdef ENABLED_DEBUG_SYNC
    if (!client_cntx->debug_sync_unavailable) {
      DEBUG_SYNC(thd, "before_handled_failed_connection");
    }
#endif
  } else {
    DBUG_PRINT("tp", ("handle_failed_connection where thd is deleted"));
  }
  if (client_cntx->connection_killed_state ==
      TP_CONNECTION_KILLED_RECEIVED_FLAG) {
    DBUG_PRINT("tp", ("KILL flag already handled"));
    return false;
  }

  /*
    Since it is possible that a long time have passed since the poll
    call completed before we get here, we need to remove the client
    context from the array even if some other thread have taken care of
    cleanup already, only here and now can we cleanup this context which
    only exists on our stack.
  */
  if (client_cntx_array)
    remove_connection_from_event_array(client_cntx, client_cntx_array,
                                       added_events);
  /*
    It is possible to have the following event chain.
    1) Owner of active_flag completes a query in the connection and
       completes by successfully rearm:ing the connection.
    2) A new query is received on the connection and put into one of
       the query queues.
    3) A KILL query is received, this will set the connection_killed_state
       and will on many platforms also close the connection.
    4) Finally we arrive here, but the thread owning the active_flag is
       still not scheduled and is still waiting to release the active_flag.

    In this case we have the active_flag set and still the connection is
    in a query queue. We could even have gotten further and started the
    cleanup code from set_connection_inactive. The cleanup code doesn't
    remove the connection from the query queue so we need to handle this
    here.

    Given that we execute this code as owner of the waiting_thread, there
    is no risk that anybody else can put it into the queue (a new thread
    can only take over as waiting_thread when the waiting_thread is
    currently actively executing a query. It cannot take over the
    waiting_thread while the waiting_thread is still taking care of the
    polled connections.
  */
  remove_connection_from_queue(my_tp_group, client_cntx);

restart_handling:
  /* Flag that we've now received the kill flag. */
  if (client_cntx->cleanup_state == TP_CLEANUP_STARTED) {
    /*
      The cleanup is already going on, but isn't yet completed.
      We need not do anything since the cleanup code will take
      care of the rest and we've already set the kill flag received
      state. We don't need to remove connection from poll/epoll
      set and close connection here since we know that the
      connection won't be rearmed after reaching this state.
    */
    DBUG_PRINT("tp", ("KILL flag received when cleanup started"));
    client_cntx->connection_killed_state = TP_CONNECTION_KILLED_RECEIVED_FLAG;
#ifdef ENABLED_DEBUG_SYNC
    if (thd && !client_cntx->debug_sync_unavailable) {
      DEBUG_SYNC(thd, "handle_failed_connection_cleanup_started");
    }
#endif
    if (ret_code) {
      mysql_mutex_unlock(&my_tp_group->LOCK_group);
    }
    return ret_code;
  } else if (client_cntx->cleanup_state == TP_CLEANUP_COMPLETED) {
    /*
      The cleanup has already been done, but since the kill flag had not
      been received yet it wasn't possible to remove the client context.
      The reason we need to keep it around is that the there is a small
      probability that the following happens.

      1) Connection is rearmed in the thread X
      2) Event arrives on connection in thread Y
      3) KILL connection arrives in thread Z
      4) The killed connection is discovered in thread X
      5) Thread X cleans up after KILL
      6) Event on KILLED connection is processed in thread Y

      To avoid this race condition we keep the client context around
      until we have received the KILL flag. When we receive the KILL
      flag it happens in the waiting thread and thus we cannot have
      2) above happen at that time. Also since we know that we will
      ensure that connection is broken and removed from poll/epoll
      set during handling of KILL flag we're ensured that we can
      safely remove the client context after receiving the KILL
      flag.
    */
    assert(client_cntx->active_flag == 1);
    DBUG_PRINT("tp", ("KILL flag received when cleanup completed"));
    client_cntx->connection_killed_state = TP_CONNECTION_KILLED_RECEIVED_FLAG;
    mysql_mutex_unlock(&my_tp_group->LOCK_group);
    remove_client_cntx(my_tp_group, client_cntx);
    return true;
  }
  /* No handling of the KILL has started yet.  */
  if (client_cntx->active_flag > 0) {
    /*
      The connection is actively being executed, we'll let the close of
      the connection be handled by the thread actively executing it.
      However it is possible that the execution has passed the detach
      stage but the connection isn't yet rearmed. We need to ensure that
      in this case it won't be possible to rearm it. We do that by closing
      the connection and disarming it. We need to lock LOCK_thd_data on THD
      object before closing it to ensure we don't interfere with the KILL
      thread here.

      We have to do some complex mutex lock/unlock handling here. The reason
      is that we cannot grab thd->LOCK_thd_data while holding the LOCK_group
      mutex since thd_pool_post_kill_notification will grab the mutexes
      in the opposite order (there is nothing preventing two calls to KILL
      a connection from different connections).

      To handle this we first release the LOCK_group mutex, then we acquire
      the mutex LOCK_thd_list and then we can verify that the THD
      object is still around and if it is we grab the LOCK_thd_data mutex
      and close the connection. Then we release these mutexes and grab the
      LOCK_group mutex again and continue processing the KILL.
    */
    if (client_cntx->connection_killed_state ==
        TP_CONNECTION_KILLED_RECEIVED_FLAG) {
      /* Already handled this, we're back here again */
      DBUG_PRINT("tp", ("Already handled KILL with active flag set"));
      mysql_mutex_unlock(&my_tp_group->LOCK_group);
      return true;
    }
    DBUG_PRINT("tp", ("KILL flag received when active_flag set"));
    assert(client_cntx->active_flag == 1);
#ifdef ENABLED_DEBUG_SYNC
    if (thd && !client_cntx->debug_sync_unavailable) {
      DEBUG_SYNC(thd, "handled_failed_connection_before_close");
    }
#endif
    mysql_mutex_unlock(&my_tp_group->LOCK_group);
    thd_lock_thread_count();
    if (client_cntx->thd) {
      thd_lock_data(thd);
      thd_close_connection(thd);
      thd_unlock_data(thd);
#ifndef HAVE_EPOLL
      /* Not required for epoll since close automatically removes it */
      tp_client_low_level_disarm(client_cntx);
#endif
    }
    thd_unlock_thread_count();
    mysql_mutex_lock(&my_tp_group->LOCK_group);
    thd = client_cntx->thd;
    client_cntx->connection_killed_state = TP_CONNECTION_KILLED_RECEIVED_FLAG;
    ret_code = true;
    goto restart_handling;
  }

  DBUG_PRINT("tp", ("KILL flag received when active_flag not set"));
  assert(!client_cntx->ready_to_execute_flag);

  /*
    We will flag that we are taking care of the close of the connection
    by setting active_flag to 2. This will be checked after rearm error.
  */
  client_cntx->active_flag = 2;
  client_cntx->connection_killed_state = TP_CONNECTION_KILLED_RECEIVED_FLAG;
  /*
    We found a connection that has been set to be killed and which
    is currently owned and operated by the thread pool. We need to
    ensure the connection is closed. We must however be very careful
    in doing so.

    At this point we are operating the waiter thread, so no one will
    start executing any new queries except if they are found in the
    query queue. Thus we must check the query queues to see if the
    connection has a query ready to execute. If this is the case
    then we'll remove the query from the queue and handle the close
    of the connection in this thread.

    At this point the connection could be in 3 places:
    1) Waiting for query execution in a queue
    2) Waiting for query execution in the event ready array
    3) Waiting in a epoll set or in a poll set.

    Handling of 1) requires removal of the query from the queue
      Has to be handled also in other cases, so is handled earlier in this
      method.
    Handling of 2) requires removal of the query from the event ready array
      We have to do this also in all other situations, so this was performed
      earlier in this method.
    Handling of 3) requires that we disarm the client connection.
  */
  DBUG_LOG("tp_kill", "waiting_thread kills idle connection id "
                          << client_cntx->connection_id);
  tp_client_low_level_disarm(client_cntx);
  mysql_mutex_unlock(&my_tp_group->LOCK_group);

  thd = client_cntx->thd;
  /*
    We have prepared fully now, we have removed any potential context
    in queues, from epoll/poll sets. Thus we're safe that no one will react
    on this connection anymore. Thus we can safely close the connection
    now in a normal manner.
  */
  tp_thd_cleanup(my_tp_group, client_cntx, thd, true, 0);
  remove_client_cntx(my_tp_group, client_cntx);
  return true;
}

/**
  Find and handle a killed connection.

  @param my_tp_group        The thread group where the killed connection
                            resides.
  @param client_cntx_array  An array of client contexts received
  @param added_events       Reference to the number of added events

  @retval   true, a killed connection was found, restart search.
  @retval   false, no killed connection found.
*/
static bool find_killed_connection(tp_group_t *my_tp_group,
                                   tp_client_low_level_t **client_cntx_array,
                                   int *added_events) {
  tp_client_low_level_t *client_cntx;
  Open_connections_list::Iterator it(my_tp_group->open_connections);

  while ((client_cntx = it++)) {
    if (client_cntx->connection_killed_state == TP_CONNECTION_NOT_KILLED) {
      continue;
    }

    if (handle_failed_connection(my_tp_group, client_cntx, client_cntx_array,
                                 added_events)) {
      /*
        Since mutex was released we need to restart list search since
        list could have changed while we released the mutex. This
        operation holds the mutex potentially for a slightly longer
        time than what would be deemed good, but the KILL operation
        is the only method of invoking this.
      */
      return true;
    }
  }

  return false;
}

/*
  This call is made from low level code when it has received a byte
  on the notify file descriptor. When this happens we check for
  killed connections and check if the connection is currently handled
  by the thread pool in which case we need to call the cleanup code
  for it. The byte sent that invokes this function was sent from
  thd_pool_post_kill_notification above.

  @param my_tp_group           The thread group where the killed connection
                               resides.
  @param len                   The number of flag bytes read
  @param msg                   The flag bytes array
  @param client_cntx_array     An array of client contexts received
  @param added_events          Reference to the number of added events
*/
static void handle_killed_connections(tp_group_t *my_tp_group, uint len,
                                      char *msg,
                                      tp_client_low_level_t **client_cntx_array,
                                      int *added_events) {
  uint i;
  bool kill_flag_found = false;
  DBUG_TRACE;

  for (i = 0; i < len; i++) {
    if (msg[i] == KILL_FLAG) {
      kill_flag_found = true;
    }
  }
  if (!kill_flag_found) {
    return;
  }
  bool krp_cleared = false;
  DBUG_LOG("tp_kill", "Found KILL_FLAG in msg");

  do {
    DBUG_PRINT(
        "tp", ("restart_search on Thread group id %d", my_tp_group->group_idx));
    mysql_mutex_lock(&my_tp_group->LOCK_group);
    if (!krp_cleared) {
      my_tp_group->is_kill_request_pending = false;
      krp_cleared = true;
    }
  } while (
      find_killed_connection(my_tp_group, client_cntx_array, added_events));

  mysql_mutex_unlock(&my_tp_group->LOCK_group);
}

/**
  Check if any query needs to have its priority increased

  @param my_tp_group        Thread group data
  @param new_10ms           Is it a new 10ms interval
*/
static void check_trans_queue_for_prio_kickups(tp_group_t *my_tp_group,
                                               bool new_10ms,
                                               Time_pt time_of_check) {
  tp_client_low_level_t *client_cntx;

  client_cntx = my_tp_group->queued_trans.front();
  /*
    At most we will move one transaction per 10ms interval. We will only
    move when a transaction has waited longer than the set maximum wait
    time. Setting this time to zero effectively means we're no queueing
    transactions at all.
  */
  if (client_cntx && new_10ms &&
      (time_of_check -
           client_cntx->time_of_event_arrival.load(std::memory_order_relaxed) >
       Misec(thread_pool_prio_kickup_timer))) {
    assert(client_cntx->is_queued);
    my_tp_group->queued_trans.remove(client_cntx);
    my_tp_group->queued_queries.push_back(client_cntx);
    /* Update statistics on prio kickups */
    my_tp_group->stats.prio_kickups++;
  }
}

/**
  Pops highest priority query(connection) from its queue,
  and returns it.

  @param my_tp_group       Thread group data

  @retval                  Non-owning pointer to popped query(connection)
  @retval                  nullptr if no queries available
*/
static inline tp_client_low_level_t *pop_highest_priority_query(
    tp_group_t *my_tp_group) {
  DBUG_TRACE;

  auto next_queue = next_query_queue(my_tp_group);
  tp_client_low_level_t *client_cntx = next_queue->front();

  if (client_cntx != nullptr) {
    next_queue->remove(client_cntx);
    assert(client_cntx->is_queued);
    client_cntx->is_queued = false;
  }

  return client_cntx;
}

/*
  Remove connection from any queue it might be in for the moment.
  This happens when a connection is killed.

  @param my_tp_group             The thread group
  @param client_cntx             Client context representing connection
*/

static void remove_connection_from_queue(tp_group_t *my_tp_group,
                                         tp_client_low_level_t *client_cntx) {
  tp_client_low_level_t *current_client_cntx;
  Query_list::Iterator it(my_tp_group->queued_queries);
  DBUG_TRACE;

  while ((current_client_cntx = it++)) {
    if (current_client_cntx == client_cntx) {
      break;
    }
  }

  if (current_client_cntx) {
    DBUG_PRINT("tp", ("Remove query from queued_queries with connection id %lu",
                      current_client_cntx->connection_id));
    assert(current_client_cntx->is_queued);
    my_tp_group->queued_queries.remove(current_client_cntx);
    current_client_cntx->is_queued = false;
    return;
  }

  it.init(my_tp_group->queued_trans);

  while ((current_client_cntx = it++)) {
    if (current_client_cntx == client_cntx) {
      break;
    }
  }

  if (current_client_cntx) {
    DBUG_PRINT("tp", ("Remove query from queued_trans with connection id %lu",
                      current_client_cntx->connection_id));
    assert(current_client_cntx->is_queued);
    my_tp_group->queued_trans.remove(current_client_cntx);
    current_client_cntx->is_queued = false;
  }
}

/*
  Final step of cleanup after a closed connection. We need to coordinate
  this with possible KILL queries in other connections.

  @param my_tp_group Thread group data
  @param client_cntx Client context representing connection
*/

static void remove_client_cntx(tp_group_t *my_tp_group,
                               tp_client_low_level_t *client_cntx) {
  DBUG_TRACE;
  mysql_mutex_lock(&my_tp_group->LOCK_group);
  client_cntx->cleanup_state = TP_CLEANUP_COMPLETED;
  if (client_cntx->connection_killed_state ==
      TP_CONNECTION_KILLED_WAITING_FLAG) {
    /*
      We're still waiting for the kill flag to arrive, we need
      to keep the client context around for this and also keep
      it in list of open connections although we already released
      the THD object.
    */
    mysql_mutex_unlock(&my_tp_group->LOCK_group);
    return;
  }
  remove_open_connection(my_tp_group, client_cntx);
  assert(!client_cntx->is_queued);
  mysql_mutex_unlock(&my_tp_group->LOCK_group);
  tp_client_low_level_end(client_cntx);
}

/**
  Perform full cleanup after failed connection

  @param thd               THD object
  @param my_thread_data    Thread data
  @param client_cntx       Client context representing connection
  @param my_tp_group       Thread group data
  @param errcode           Error code
*/

static void tp_full_cleanup(THD *thd, tp_thread_t *my_thread_data,
                            tp_client_low_level_t *client_cntx,
                            tp_group_t *my_tp_group, unsigned int errcode) {
  DBUG_TRACE;
  DBUG_PRINT("tp_enter", ("Thread group id: %d, connection id: %lu",
                          my_tp_group->group_idx, client_cntx->connection_id));

  /*
    We signal to the KILL handling code that we have started the
    cleanup of the connection so that he need not worry about it
    anymore.
  */
  mysql_mutex_lock(&my_tp_group->LOCK_group);
  assert(client_cntx);
  assert(client_cntx->active_flag == 1);
  assert(client_cntx->cleanup_state == TP_CLEANUP_IDLE);
  client_cntx->cleanup_state = TP_CLEANUP_STARTED;
  if (my_thread_data) {
    /*
      Remove pointer to object to be removed to ensure we don't read
      free'd memory in filling of TP_THREAD_STATE table
    */
    my_thread_data->client_low_level_cntx = nullptr;
  }
  mysql_mutex_unlock(&my_tp_group->LOCK_group);

#ifdef ENABLED_DEBUG_SYNC
  if (!client_cntx->debug_sync_unavailable) {
    DEBUG_SYNC(thd, "sleep_after_cleanup_started");
  }
#endif

  tp_thd_cleanup(my_tp_group, client_cntx, thd, true, errcode);

  /*
    The environment of DBUG has disappeared since the thd is now gone, so we
    need to remove the session state, we pop, if nothing is pushed it's a
    no-op.
  */
  DBUG_POP();

  remove_client_cntx(my_tp_group, client_cntx);
}

bool schema_table_store_record(THD *thd, TABLE *table);

/** Counter for accesses to deprecated Thread Pool Information Schema tables. */
static std::atomic_int64_t ATMC_i_s_usage_counter = 0;

/**
  Fill in TP_THREAD_STATE table

  @param thd            THD object
  @param tables         I_S table list object

  @return               true if failure, false if success
*/
int fill_thread_state_table(THD *thd, Table_ref *tables, Item *) {
  uint i, j;
  uint thread_id;
  int64 process_count_time_unit;
  TABLE *table = tables->table;
  tp_client_low_level_t *client_cntx;
  tp_thread_t *cur_thread;
  tp_group_t *cur_group;

  assert(is_thread_pool_plugin_initialized());

  push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WARN_DEPRECATED_SYNTAX,
                      ER_WARN_DEPRECATED_SYNTAX_MSG,
                      "information_schema.tp_thread_state",
                      "performance_schema.tp_thread_state");
  ++ATMC_i_s_usage_counter;

  auto now = tp_now();
  // Iterate over all thread groups, including admin thread group
  for (i = 0; i < tp_groups; i++) {
    cur_group = &(tp_group_list[i]);

    /* Read the variables under mutex protection */
    mysql_mutex_lock(&cur_group->LOCK_group);
    for (j = 0; j < cur_group->max_thread_ids_in_group; j++) {
      cur_thread = &(cur_group->group_threads[j]);
      if (cur_thread->thread_handle.thread == null_thread_initializer) {
        continue;
      }
      thread_id = cur_thread->numeric_id;
      process_count_time_unit =
          get_process_count_time_unit(cur_thread, now).value_or(-1);
      client_cntx = cur_thread->client_low_level_cntx;
      auto wt = client_cntx ? client_cntx->wait_type : THD_WAIT_NONE;

      mysql_mutex_unlock(&cur_group->LOCK_group);
      // Write thread row into I_S table

      // TP_GROUP_ID
      table->field[0]->store(i, true);

      // TP_THREAD_NUMBER
      table->field[1]->store(thread_id, true);

      // PROCESS_COUNT
      if (process_count_time_unit >= 0) {
        process_count_time_unit++;
      } else {
        process_count_time_unit = 0;
      }
      table->field[2]->store(process_count_time_unit, true);

      // WAIT_TYPE
      if (wt != THD_WAIT_NONE) {
        const char *srs = THD_wait_type_str(wt);
        table->field[3]->store(srs, strlen(srs), system_charset_info);
        table->field[3]->set_notnull();
      } else {
        table->field[3]->set_null();
      }

      // TP_THREAD_TYPE
      const char *worker_type_str =
          worker_thread_type_str(cur_thread->worker_type);
      table->field[4]->store(worker_type_str, strlen(worker_type_str),
                             system_charset_info);

      // THREAD_ID
      if (cur_thread->pfs_thread_id != 0) {
        table->field[5]->store(cur_thread->pfs_thread_id, true);
        table->field[5]->set_notnull();
      } else {
        table->field[5]->set_null();
      }

      if (schema_table_store_record(thd, table)) {
        return true;
      }
      mysql_mutex_lock(&cur_group->LOCK_group);
    }
    mysql_mutex_unlock(&cur_group->LOCK_group);
  }
  return false;
}

/**
  Fill in TP_THREAD_GROUP_STATE table

  @param thd            THD object
  @param tables         I_S table list object

  @return               true if failure, false if success
*/
int fill_thread_group_state_table(THD *thd, Table_ref *tables, Item *) {
  uint i;
  tp_group_t *cur_group;
  TABLE *table = tables->table;
  tp_thread_t *waiter_thread;
  tp_client_low_level_t *client_cntx;
  const char *algorithm_str;
  uint exists_waiter;
  longlong oldest_waiter = 0;
  uint waiting_thread_number = 0;
  uint consumer_threads, reserve_threads, connect_threads, connection_count;
  uint queued_queries, queued_transactions, thread_count;
  uint active_thread_count, stalled_thread_count;
  uint max_thread_ids_in_group;

  assert(is_thread_pool_plugin_initialized());

  push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WARN_DEPRECATED_SYNTAX,
                      ER_WARN_DEPRECATED_SYNTAX_MSG,
                      "information_schema.tp_thread_group_state",
                      "performance_schema.tp_thread_group_state");
  ++ATMC_i_s_usage_counter;

  // Iterate over all thread groups, including admin thread group
  for (i = 0; i < tp_groups; i++) {
    cur_group = &(tp_group_list[i]);

    /* Read the variables under mutex protection */
    mysql_mutex_lock(&cur_group->LOCK_group);
    consumer_threads = cur_group->threads_for_consumer;
    reserve_threads = cur_group->threads_for_reserve;
    connect_threads = cur_group->connect_threads;
    connection_count = cur_group->stats.connection_count;
    queued_queries = cur_group->queued_queries.elements();
    queued_transactions = cur_group->queued_trans.elements();
    thread_count = cur_group->threads_initialized;
    waiter_thread = cur_group->waiting_thread;
    stalled_thread_count =
        cur_group->max_active_threads - cur_group->threads_user_request;
    max_thread_ids_in_group = cur_group->max_thread_ids_in_group;
    /* Find oldest waiter */
    if ((client_cntx = cur_group->queued_trans.front())) {
      exists_waiter = true;
      oldest_waiter = std::chrono::duration_cast<Misec>(
                          tp_now() - client_cntx->time_of_event_arrival.load(
                                         std::memory_order_relaxed))
                          .count();
    } else {
      exists_waiter = false;
    }
    active_thread_count = get_active_threads_relaxed(cur_group);
    mysql_mutex_unlock(&cur_group->LOCK_group);
    if (waiter_thread) {
      waiting_thread_number = waiter_thread->numeric_id;
    }
    switch (thread_pool_algorithm) {
      case LOW_CONCURRENCY_ALGORITHM:
        algorithm_str = "Low Concurrency";
        break;
      case HIGH_CONCURRENCY_ALGORITHM:
        algorithm_str = "High Concurrency";
        break;
      default:
        algorithm_str = "";
        break;
    }

    /* Set fields in I_S table */
    table->field[0]->store(i, true);
    table->field[1]->store(consumer_threads, true);
    table->field[2]->store(reserve_threads, true);
    table->field[3]->store(connect_threads, true);
    table->field[4]->store(connection_count, true);
    table->field[5]->store(queued_queries, true);
    table->field[6]->store(queued_transactions, true);
    table->field[7]->store(thread_pool_stall_limit, true);
    table->field[8]->store(thread_pool_prio_kickup_timer, true);
    table->field[9]->store(algorithm_str, strlen(algorithm_str),
                           system_charset_info);
    table->field[10]->store(thread_count, true);
    table->field[11]->store(active_thread_count, true);
    table->field[12]->store(stalled_thread_count, true);
    if (waiter_thread) {
      table->field[13]->store(waiting_thread_number, true);
      table->field[13]->set_notnull();
    } else {
      table->field[13]->set_null();
    }

    if (exists_waiter) {
      table->field[14]->set_notnull();
      table->field[14]->store(oldest_waiter, true);
    } else {
      table->field[14]->set_null();
    }
    table->field[15]->store(max_thread_ids_in_group, true);
    /* Store the record in the I_S table */
    if (schema_table_store_record(thd, table)) {
      return true;
    }
  }
  return false;
}

/**
  Fill in TP_THREAD_GROUP_STATS table

  @param thd            THD object
  @param tables         I_S table list object

  @return               true if failure, false if success
*/
int fill_thread_group_stats_table(THD *thd, Table_ref *tables, Item *) {
  uint i;
  tp_group_t *cur_group;
  TABLE *table = tables->table;

  assert(is_thread_pool_plugin_initialized());

  push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WARN_DEPRECATED_SYNTAX,
                      ER_WARN_DEPRECATED_SYNTAX_MSG,
                      "information_schema.tp_thread_group_stats",
                      "performance_schema.tp_thread_group_stats");
  ++ATMC_i_s_usage_counter;

  // Iterate over all thread groups, including admin thread group
  for (i = 0; i < tp_groups; i++) {
    cur_group = &(tp_group_list[i]);
    /* Read the variables under mutex protection */
    mysql_mutex_lock(&cur_group->LOCK_group);
    tp_group_statistics_t stats{cur_group->stats.connection_count,
                                cur_group->stats.connections_started,
                                cur_group->stats.connections_closed,
                                cur_group->stats.queries_executed.load(),
                                cur_group->stats.queries_queued,
                                cur_group->stats.threads_created,
                                cur_group->stats.prio_kickups,
                                cur_group->stats.stalled_queries_executed,
                                cur_group->stats.become_consumer_thread,
                                cur_group->stats.become_reserve_thread,
                                cur_group->stats.become_listen_thread,
                                cur_group->stats.wake_thread_stall_checker,
                                {}};
    std::uninitialized_copy(std::begin(cur_group->stats.wait_counts),
                            std::end(cur_group->stats.wait_counts),
                            std::begin(stats.wait_counts));
    mysql_mutex_unlock(&cur_group->LOCK_group);

    table->field[0]->store(i, true);
    table->field[1]->store(stats.connections_started, true);
    table->field[2]->store(stats.connections_closed, true);
    table->field[3]->store(stats.queries_executed, true);
    table->field[4]->store(stats.queries_queued, true);
    table->field[5]->store(stats.threads_created, true);
    table->field[6]->store(stats.prio_kickups, true);
    table->field[7]->store(stats.stalled_queries_executed, true);
    table->field[8]->store(stats.become_consumer_thread, true);
    table->field[9]->store(stats.become_reserve_thread, true);
    table->field[10]->store(stats.become_listen_thread, true);
    table->field[11]->store(stats.wake_thread_stall_checker, true);
    table->field[12]->store(stats.wait_counts[THD_WAIT_SLEEP], true);
    table->field[13]->store(stats.wait_counts[THD_WAIT_DISKIO], true);
    table->field[14]->store(stats.wait_counts[THD_WAIT_ROW_LOCK], true);
    table->field[15]->store(stats.wait_counts[THD_WAIT_GLOBAL_LOCK], true);
    table->field[16]->store(stats.wait_counts[THD_WAIT_META_DATA_LOCK], true);
    table->field[17]->store(stats.wait_counts[THD_WAIT_TABLE_LOCK], true);
    table->field[18]->store(stats.wait_counts[THD_WAIT_USER_LOCK], true);
    table->field[19]->store(stats.wait_counts[THD_WAIT_BINLOG], true);
    table->field[20]->store(stats.wait_counts[THD_WAIT_GROUP_COMMIT], true);
    table->field[21]->store(stats.wait_counts[THD_WAIT_SYNC], true);

    if (schema_table_store_record(thd, table)) {
      return true;
    }
  }
  return false;
}

/**
  Return the recorded number of accesses for the TP I_S tables. For use by
  the Thread_pool_i_s_usage status variable.
  */
std::int64_t get_i_s_usage() { return ATMC_i_s_usage_counter.load(); }

tp_group_t *get_group(unsigned int index) {
  // Even admin thread group can be returned
  if (index < tp_groups) {
    return &tp_group_list[index];
  }

  return nullptr;
}

std::uint32_t get_active_threads(const tp_group_t *tp_group) {
  return get_active_threads_relaxed(tp_group);
}

// Return Query worker threads
ulong tp_get_query_worker_threads() { return thread_pool_query_worker_threads; }

bool adjust_query_threads_in_each_thread_group(
    std::uint32_t thread_pool_query_threads_per_group_new) {
  std::function<void(tp_group_t *)> query_worker_thread_adjuster;

  auto reduce_worker_threads =
      [thread_pool_query_threads_per_group_new](tp_group_t *cur_group) {
        /*
          If thread group has active threads (including stalled threads) and no
          consumer threads, then just updating "threads_user_request" member of
          thread group will either terminate or move thread to unused threads
          pool once they complete query execution.

          If a thread group has few active threads and few consumer threads,
          then signal consumer threads so that threads in consumer threads pool
          are either terminated or moved to the unused threads pool. In either
          case, query threads per group are *not* reduced immediately.
        */
        std::uint32_t consumer_threads = cur_group->threads_for_consumer;
        std::uint32_t no_of_threads_to_awake =
            (thread_pool_query_threads_per_group -
             thread_pool_query_threads_per_group_new);
        while (consumer_threads > 0 && no_of_threads_to_awake > 0) {
          mysql_cond_signal(&cur_group->COND_consumer);
          consumer_threads--;
          no_of_threads_to_awake--;
        }
      };

  auto add_worker_threads = [thread_pool_query_threads_per_group_new](
                                tp_group_t *cur_group) {
    /*
      If thread group has sufficient active threads (including stalled threads)
      then, just updating "threads_user_request" member of thread group is
      sufficient. Required number of threads are not moved to unused threads
      pool or terminated once they complete query execution.

      If threads group has less active threads then, try to get thread from
      the unused(reserve) threads first. If more threads are needed then, create
      new one.
    */
    std::uint32_t active_query_threads =
        (cur_group->num_query_threads - cur_group->threads_for_reserve);
    if (active_query_threads < thread_pool_query_threads_per_group_new) {
      // First try to get thread from the unused(reserve) threads.
      std::uint32_t new_threads_count =
          thread_pool_query_threads_per_group_new - active_query_threads;
      std::uint32_t reserve_threads = cur_group->threads_for_reserve;
      while (reserve_threads > 0 && new_threads_count > 0) {
        mysql_cond_signal(&cur_group->COND_reserve);
        reserve_threads--;
        new_threads_count--;
      }
      // If still more threads are needed then, create new query threads.
      if (new_threads_count > 0) {
        uint threads_created =
            tp_create_worker_threads(cur_group, new_threads_count,
                                     Worker_thread_type::QUERY_WORKER_THREAD);

        cur_group->num_query_threads += threads_created;
        tp_thread_create_control_ctx.total_query_threads += threads_created;
        cur_group->last_thread_creation_time = my_micro_time();
      }
    }
  };

  auto adjust_worker_threads_in_each_tg =
      [thread_pool_query_threads_per_group_new](auto adjuster) {
        for (std::uint32_t i = 0; i < tp_groups; i++) {
          tp_group_t *cur_group = &(tp_group_list[i]);

          mysql_mutex_lock(&cur_group->LOCK_group);
          cur_group->threads_user_request =
              thread_pool_query_threads_per_group_new;
          adjuster(cur_group);
          cur_group->max_active_threads =
              cur_group->threads_user_request + cur_group->stalled_threads;
          mysql_mutex_unlock(&cur_group->LOCK_group);
        }
      };

  if (thread_pool_query_threads_per_group_new <
      thread_pool_query_threads_per_group)
    adjust_worker_threads_in_each_tg(reduce_worker_threads);
  else
    adjust_worker_threads_in_each_tg(add_worker_threads);

  return false;
}
