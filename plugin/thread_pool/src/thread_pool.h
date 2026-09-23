#pragma once

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

/**
   @defgroup ThreadPool Thread Pool Scheduler
 */

/**@{*/

/**
  @file

  Pool of Threads - shared with poll() and epoll() versions (definition).
*/

#include <sys/types.h>
#include <atomic>
#include <chrono>
#include <iomanip>  // std::put_time
#include <optional>
#include <sstream>  // std::stringstream

#include <mysql/thread_pool_priv.h>
#include "my_compiler.h"
#include "my_systime.h"  // Our gmtime_r(const time_t*,...)

#include <mysql/components/services/log_builtins.h>

using Musec = std::chrono::microseconds;
using Misec = std::chrono::milliseconds;
using Cesec = std::chrono::duration<std::int64_t, std::centi>;
using Time_pt = std::chrono::time_point<std::chrono::system_clock,
                                        std::chrono::microseconds>;

inline constexpr Time_pt BEGINNING_OF_EPOCH;

// Max 512 thread groups for normal query execution
inline constexpr int MAX_NORMAL_THREAD_GROUPS = 512;
/* constexpr int MAX_NORMAL_THREAD_GROUPS = 1 */ /* use this for testing */

// Max number of admin thread groups
inline constexpr int MAX_ADMIN_THREAD_GROUPS = 1;

// Normal thread groups + admin thread group to allow managenent operations
// even when all the normal thread groups are blocked
inline constexpr int MAX_THREAD_GROUPS =
    MAX_NORMAL_THREAD_GROUPS + MAX_ADMIN_THREAD_GROUPS;

/* Max 4096 threads per thread group will be created */
inline constexpr auto MAX_THREADS_PER_GROUP = 4096;
inline constexpr auto MAX_THREADS_PER_GROUP_MASK = MAX_THREADS_PER_GROUP - 1;

/* We will wait for a maximum of 32 query events per poll/epoll_wait call */
inline constexpr auto MAX_EVENTS_PER_WAIT_CALL = 32;

#ifdef _WIN32
inline constexpr auto TP_INVALID_SOCKET = INVALID_SOCKET;
#else
inline constexpr int TP_INVALID_SOCKET = static_cast<int>(-1);
#endif

inline constexpr std::string_view TP_CONNECTION_ADMIN = "TP_CONNECTION_ADMIN";

bool thd_pool_init();
bool thd_pool_add_connection(Channel_info *channel_info);
void thd_pool_post_kill_notification(THD *thd);
void thd_pool_wait_begin(THD *thd, int wait_type);
void thd_pool_wait_end(THD *thd);
void thd_pool_end();

bool is_high_priority_connection(THD *thd);

/**
  Check thread pool is in highly concurrent query processing mode.
  high_concurrency_algorithm with multiple query worker threads
  used in highly concurrent query processing mode.

  @returns true if highly concurrent query processing is enabled.
*/
bool in_highly_concurrent_query_processing_mode();

/**
  Adjust query worker threads in each thread group on system variable
  "thread_pool_query_threads_per_group" variable update.

  @param  thread_pool_query_threads_per_group_new  New value of system variable.

  @retval   true   On Success.
  @retval   false  On Failure.
*/
bool adjust_query_threads_in_each_thread_group(
    std::uint32_t thread_pool_query_threads_per_group_new);

// Thread pool size is the number of thread groups.
inline constexpr auto DEF_POOL_SIZE = 16;
inline constexpr auto TP_MIN_POOL_SIZE = 1;
inline constexpr int TP_MAX_POOL_SIZE = MAX_NORMAL_THREAD_GROUPS;
extern ulong thread_pool_size;

// Algorithm used by the thread pool scheduler.
inline constexpr auto LOW_CONCURRENCY_ALGORITHM = 0;
inline constexpr auto HIGH_CONCURRENCY_ALGORITHM = 1;
inline constexpr auto DEF_ALGORITHM = HIGH_CONCURRENCY_ALGORITHM;
extern ulong thread_pool_algorithm;

// Stall limit in centiseconds.
inline constexpr auto DEF_STALL_LIMIT = 6;
inline constexpr auto TP_MIN_STALL_LIMIT = 4;
inline constexpr auto TP_MAX_STALL_LIMIT = 600;
extern ulong thread_pool_stall_limit;

/** Returns the value of the system variable thread_pool_stall_limit (which
    is in centiseconds) converted to Musec (std::chrono::microseconds).
    @note Technically this is a race, since we do not take the sys_var mutex
    here but this is "established practice". */
inline Musec get_stall_limit_us() {
  return Musec(thread_pool_stall_limit * 10 * 1000);
}

// Priority kickup timer in milliseconds.
inline constexpr auto DEF_PRIO_KICKUP_TIMER = 1000;
inline constexpr auto TP_MIN_PRIO_KICKUP_TIMER = 0;
inline constexpr auto TP_MAX_PRIO_KICKUP_TIMER =
    std::numeric_limits<uint32>::max();
extern ulong thread_pool_prio_kickup_timer;

// Max number of unused threads in each thread group.
inline constexpr auto DEF_MAX_UNUSED_THREADS = 32;
inline constexpr auto TP_MIN_MAX_UNUSED_THREADS = 0;
inline constexpr auto TP_MAX_MAX_UNUSED_THREADS = MAX_THREADS_PER_GROUP;
extern ulong thread_pool_max_unused_threads;

// Max number of active query threads in each thread group.
inline constexpr auto DEF_MAX_ACTIVE_QUERY_THREADS = 0;
inline constexpr auto TP_MIN_MAX_ACTIVE_QUERY_THREADS = 0;
inline constexpr auto TP_MAX_MAX_ACTIVE_QUERY_THREADS = 512;
extern std::uint32_t thread_pool_max_active_query_threads;

// The maximum number of transactions permitted by the thread pool.
constexpr std::uint32_t DEF_MAX_TRANSACTIONS_LIMIT = 0;
constexpr std::uint32_t TP_MIN_MAX_TRANSACTIONS_LIMIT = 0;
constexpr std::uint32_t TP_MAX_MAX_TRANSACTIONS_LIMIT = 1000000;
extern std::uint32_t thread_pool_max_transactions_limit;
extern std::uint32_t thread_pool_max_transactions_limit_per_tg;

// Dedicates a listener thread in each thread group.
inline constexpr bool DEF_DEDICATED_LISTENERS = false;
extern bool thread_pool_dedicated_listeners;

/** Returns a string containing a textual representation of the algorithm used
    based on the values of the system variabled thread_pool_algorithm and
    thread_pool_dedicated_listeners. */
inline const char *tp_algorithm() {
  if (thread_pool_dedicated_listeners) return "Dedicated Listeners";
  if (thread_pool_algorithm == LOW_CONCURRENCY_ALGORITHM)
    return "Low Concurrency";
  if (thread_pool_algorithm == HIGH_CONCURRENCY_ALGORITHM)
    return "High Concurrency";
  assert(false);
  return "Unknown algorithm";
}

// Transaction delay in milliseconds.
inline constexpr int DEF_TRANSACTION_DELAY = 0;
inline constexpr int MIN_TRANSACTION_DELAY = 0;
inline constexpr int MAX_TRANSACTION_DELAY = 300000;  // 5 minutes

Misec get_transaction_delay();

// Query worker threads per thread group.
inline constexpr std::uint32_t DEF_QUERY_THREADS_PER_GROUP = 2;
inline constexpr std::uint32_t MIN_QUERY_THREADS_PER_GROUP = 1;
inline constexpr std::uint32_t MAX_QUERY_THREADS_PER_GROUP =
    MAX_THREADS_PER_GROUP;
extern std::uint32_t thread_pool_query_threads_per_group;

// Count of query worker thread in thread pool.
inline constexpr auto DEF_QUERY_WORKER_THREADS = 0;
extern uint32_t thread_pool_query_worker_threads;

// Connection report interval in seconds
inline constexpr longlong CONNECTION_REPORT_INTERVAL_OFF = 0;
inline constexpr longlong DEF_CONNECTION_REPORT_INTERVAL = 120;
inline constexpr longlong MIN_CONNECTION_REPORT_INTERVAL = 60;
inline constexpr longlong MAX_CONNECTION_REPORT_INTERVAL = 600;

// longrun_trx_limit in milliseconds
inline constexpr std::uint32_t DEF_LONGRUN_TRX_LIMIT = 2000;
inline constexpr std::uint32_t MIN_LONGRUN_TRX_LIMIT = 10;
inline constexpr std::uint32_t MAX_LONGRUN_TRX_LIMIT = 86400000;

inline std::atomic<Misec> ATMC_longrun_trx_limit;
/**
  Flag which indicates that the thread_pool_plugin_init() function
  has completed successfully.
*/
inline std::atomic_bool thread_pool_plugin_initialized = false;

/**
  Reads thread_pool_plugin_initialized with a weaker
  memory order, currently std::memory_order_relaxed.
 */
inline bool is_thread_pool_plugin_initialized() {
  return thread_pool_plugin_initialized.load(std::memory_order_relaxed);
}

/** Flag which indicates that a shutdown has been initiated. */
inline std::atomic_bool tp_shutdown = false;

/**
  Reads tp_shutdown with a weaker
  memory order, currently std::memory_order_relaxed.
 */
inline bool is_tp_shutdown() {
  return tp_shutdown.load(std::memory_order_relaxed);
}

inline Time_pt tp_now() { return Time_pt{Musec{my_micro_time()}}; }
inline std::ostream &operator<<(std::ostream &os, Time_pt t) {
  auto d = t.time_since_epoch();
  if (d == d.zero()) {
    os << "#beginning of epoch#";
    return os;
  }
  auto sd = std::chrono::duration_cast<std::chrono::seconds>(d);
  time_t sec = sd.count();
  std::tm gmdate;
  gmtime_r(&sec, &gmdate);
  os << std::put_time(&gmdate, "%FT%T") << '.' << std::setfill('0')
     << std::setw(6) << (d % sd).count() << 'Z';
  return os;
}

using PSI_time_point = std::optional<std::uint64_t>;

/** Returns a Time_pt as a PSI_time_point. The Time_pt
    BEGINNING_OF_EPOCH is returned as an PSI_timepoint without a value
   (std::nullopt)*/
inline PSI_time_point psi_time_point(Time_pt tp) {
  return tp == BEGINNING_OF_EPOCH
             ? std::nullopt
             : PSI_time_point(tp.time_since_epoch().count());
}

class Table_ref;
class Item;

/* I_S table fillers */
int fill_thread_state_table(THD *thd, Table_ref *tables, Item *);
int fill_thread_stats_table(THD *thd, Table_ref *tables, Item *);
int fill_thread_group_state_table(THD *thd, Table_ref *tables, Item *);
int fill_thread_group_stats_table(THD *thd, Table_ref *tables, Item *);

ulong tp_get_query_worker_threads();
std::int64_t get_i_s_usage();

/* Access for P_S tables iterators */
struct tp_group_t;
tp_group_t *get_group(unsigned int index);

std::uint32_t get_active_threads(const tp_group_t *tp_group);
std::uint32_t configured_max_transactions_limit_per_group(const tp_group_t *);

/** Adds an increment to an atomic unsafely. That is, unlike for operator++ or
   fetch_add there is no guarantee that concurrent updates are not lost.
   Intended use-case is when the atomic is ONLY updated from one thread, but may
   be read from many threads (typically worker threads executing P_S queries).
   Without using an atomic this would be a so-called "benign" race. */
template <typename T>
void unsafe_atomic_add(std::atomic<T> *atm, T inc) {
  atm->store(atm->load(std::memory_order_relaxed) + inc,
             std::memory_order_relaxed);
}

/**@}*/
