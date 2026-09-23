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
  @file

  @brief Poll methods - shared with poll() and epoll() versions.
*/

#include "include/mysql/psi/psi_memory.h"
#include "mysql/service_thd_wait.h"
#include "plugin/thread_pool/src/thread_pool.h"

/* Header-only intrusive parameterized list */
#include "sql/sql_plist.h"

#ifdef HAVE_EPOLL
#include <sys/epoll.h>
#endif

#include <atomic>

#ifdef HAVE_EPOLL
extern PSI_memory_key key_memory_tp_group_low_level;
extern PSI_memory_key key_memory_tp_client_low_level;
#endif

/* Structures */

/* thread pool group low level */
struct tp_group_low_level_t;

/* thread pool client low level */
struct tp_client_low_level_t;

/* client low level open connection list adapter */
struct tp_client_low_level_open_conn_adapter_t;

/* client low level query list adapter */
struct tp_client_low_level_query_adapter_t;

// Role of worker thread.
enum class Worker_thread_type {
  CONNECTION_HANDLER_WORKER_THREAD,
  LISTENER_WORKER_THREAD,
  QUERY_WORKER_THREAD,
  TIMER_WORKER_THREAD
};

inline const char *worker_thread_type_str(Worker_thread_type t) {
  switch (t) {
    case Worker_thread_type::CONNECTION_HANDLER_WORKER_THREAD:
      return "CONNECTION_HANDLER_WORKER_THREAD";
    case Worker_thread_type::LISTENER_WORKER_THREAD:
      return "LISTENER_WORKER_THREAD";
    case Worker_thread_type::QUERY_WORKER_THREAD:
      return "QUERY_WORKER_THREAD";
    case Worker_thread_type::TIMER_WORKER_THREAD:
      return "TIMER_WORKER_THREAD";
  }
  assert(false);
  return "<Unknown enumeration value>";
}

/** States for threads represented by tp_thread_t. This includes
   the stall checker thread. */
enum class Thread_state {
  // Query worker thread states

  // In main loop, not hanging in epoll wait or executing query
  // (difficult to observe)
  MANAGING,

  // Hanging in epoll_wait() waiting for file descriptors to become
  // readable
  POLLING,

  // Processing a single connection with a readable fd without
  // queueing it
  PROCESSING_DIRECT,

  // Processing connection from queue. Either as the waiting
  // thread because multiple file descriptors became readable
  // in the same epoll_wait(), or as an extra worker woken up
  // to process more queries when using
  // high_concurrency_algorithm
  PROCESSING_QUEUED,

  // Thread is waiting on COND_consumer as "hot" thread (first choice if
  // addtional threads are needed).
  SLEEPING_CONSUMER,

  // Thread is waiting on COND_reserve as "cold" thread.
  SLEEPING_RESERVE,

  // Connection handler states

  // Connection handler is processing a connection request
  CH_PROCESSING,

  // Connection handler is in a timed sleep. This is used by
  // additional connection handler threads created if the
  // existing ones are busy when the connection request
  // arrives. If the timed sleep expires without another
  // connection request arriving, the extra connection
  // handler thread terminates.
  CH_SLEEPING_TIMED,

  // Connection handler is in an indefinite sleep. This is
  // used by the last connection handler remaining when there
  // are no more connection requests to process.
  CH_SLEEPING_INDEFINITE,

  // Stall checker states

  // Stall checker is running and doing its various checks.
  SC_CHECKING,

  // Stall checker is in a short (1 ms) sleep. This happens
  // when its last run triggered an action, e.g. a thread wakeup.
  SC_SLEEPING_SHORT,

  // Stall checker is in a long (10 ms) sleep. This happens
  // when its last run did not trigger any actions.
  SC_SLEEPING_LONG
};

/** Returns string representation of Thread_state enum. */
inline const char *thread_state_str(Thread_state ts) {
  switch (ts) {
    case Thread_state::MANAGING:
      return "MANAGING";
    case Thread_state::POLLING:
      return "POLLING";
    case Thread_state::PROCESSING_DIRECT:
      return "PROCESSING_DIRECT";
    case Thread_state::PROCESSING_QUEUED:
      return "PROCESSING_QUEUED";
    case Thread_state::SLEEPING_CONSUMER:
      return "SLEEPING_CONSUMER";
    case Thread_state::SLEEPING_RESERVE:
      return "SLEEPING_RESERVE";

    case Thread_state::CH_PROCESSING:
      return "CH_PROCESSING";
    case Thread_state::CH_SLEEPING_TIMED:
      return "CH_SLEEPING_TIMED";
    case Thread_state::CH_SLEEPING_INDEFINITE:
      return "CH_SLEEPING_INDEFINITE";

    case Thread_state::SC_CHECKING:
      return "SC_CHECKING";
    case Thread_state::SC_SLEEPING_SHORT:
      return "SC_SLEEPING_SHORT";
    case Thread_state::SC_SLEEPING_LONG:
      return "SC_SLEEPING_LONG";
  }
  assert(false);
  return "<Unknown Thread_state value>";
}

struct tp_thread_t {
  my_thread_handle thread_handle;
  /*
    numeric_id is a strictly increasing number assigned to each new thread.
    It's only used for the information schema to provide a user understandable
    identity of the thread.
  */
  uint numeric_id{0};
  /* Reference to the thread group */
  uint thread_tp_group{0};

  // Set when thread is attached to a connection for processing a query.  Set
  // back to BEGINNING_OF_EPOCH when thread is detached from connection/THD.
  //
  // The thread is deemed as stalled when thread_pool_stall_limit (=6 => 60ms
  // by default) has passed since the thread was attached.
  //
  // When a thread is deemed as stalled it is no longer active (but
  // tp_client_low_level_t::active_flag will still be true) and the
  // max_active_threads can be incremented to give space for another active
  // thread. This means that only short queries will stop other queries from
  // starting, a long-running query will quickly be deemed as stalled and
  // thereafter the thread pool will more or less treat it as a separate thread
  // until the query completes.
  Time_pt time_of_attach;

  /**
    not_stalled is used when thd_wait_begin and thd_wait_end are called.
    If the thread is already stalled it doesn't need to be removed from
    count of active threads since max_active_threads is already added for
    stalled threads. This variables indicates whether we should call
    add_active_threads to increment count when thd_wait_end is called.
  */
  bool not_stalled{false};
  /**
    stalled keeps track of if we already calculated query as stalled to
    avoid counting it twice in the event of configuration changes while
    the query is executing.
  */
  bool stalled = false;

  /** Start of stack for thread, used to set thd->thread_stack */
  char *stack_start{nullptr};
  /** Reference to thread pool low level context representing the connection */
  tp_client_low_level_t *client_low_level_cntx{nullptr};
  /**
     True if the worker thread that this contexts does the task of connection
     initialization and authentication.
  */
  bool is_connection_handler_thread{false};
  // Role this worker thread handles - either listener or connection handler
  // thread or a query thread.
  Worker_thread_type worker_type;
  // Performance schema thread id.
  ulonglong pfs_thread_id{0};

  Thread_state state;
  bool contributes_to_threads_active = false;

  std::atomic_uint64_t event_count = 0;
  std::atomic<Musec> acc_event_time;
  std::atomic_uint64_t command_count = 0;
  std::atomic<Musec> acc_command_time;
};

/** Updates the state for the thread with a new value. */
inline void set_state(tp_thread_t *my_thread_data, Thread_state s) {
  my_thread_data->state = s;
}

/** Returns the legacy process_count_time reported in P_S/I_S,
    but computes it from time_of_attach. Unit is still centiseconds
    (milliseconds/10). */
inline std::optional<std::uint64_t> get_process_count_time_unit(
    const tp_thread_t *my_thread_data, Time_pt now = tp_now()) {
  return my_thread_data->time_of_attach == BEGINNING_OF_EPOCH
             ? std::nullopt
             : std::optional<std::uint64_t>(
                   std::chrono::duration_cast<Cesec>(
                       now - my_thread_data->time_of_attach)
                       .count());
}

struct tp_group_statistics_t {
  /** Number of current connections, protected by LOCK_group */
  uint connection_count;
  /** Connection set-ups, protected by LOCK_group */
  longlong connections_started;
  /** Connections closed, protected by LOCK_group */
  longlong connections_closed;
  /** Number of queries executed */
  std::atomic<longlong> queries_executed;
  /** Number of queries queued in total */
  longlong queries_queued;
  /** Number of threads created in this group */
  longlong threads_created;
  /** Number of queries that got its priority enhanced after timeout */
  longlong prio_kickups;
  /** Number of queries executed that received stalled status */
  longlong stalled_queries_executed;
  /** Counter of how many times a thread became consumer thread */
  longlong become_consumer_thread;
  /** Counter of how many times a thread became reserve thread */
  longlong become_reserve_thread;
  /** Counter of how many times a thread became waiting thread */
  longlong become_listen_thread;
  /** Counter of how many times we woke a thread from stall checker */
  longlong wake_thread_stall_checker;
  /** Counter of number of waits */
  longlong wait_counts[THD_WAIT_LAST];
};

/** Type of the list of open connections */
typedef I_P_List<tp_client_low_level_t, tp_client_low_level_open_conn_adapter_t>
    Open_connections_list;

/** Type of the list of active queries */
typedef I_P_List<tp_client_low_level_t, tp_client_low_level_query_adapter_t,
                 I_P_List_counter,
                 I_P_List_fast_push_back<tp_client_low_level_t>>
    Query_list;

/**
   Connection context representing an incoming connection.
   It has data member next_in_queue and prev_in_queue to
   provide for an intrusive queue implementation.
*/
struct connection_context_t {
  Channel_info *m_channel_info = nullptr;
  connection_context_t *m_next_in_queue = nullptr;
  connection_context_t **m_prev_in_queue = nullptr;
  Time_pt time_of_add = tp_now();
  connection_context_t(Channel_info *cip) : m_channel_info(cip) {}
  ~connection_context_t() { destroy_channel_info(m_channel_info); }
};

struct connection_context_adapter_t
    : public I_P_List_adapter<connection_context_t,
                              &connection_context_t::m_next_in_queue,
                              &connection_context_t::m_prev_in_queue> {};

// Queue of incoming connection requests from clients.
typedef I_P_List<connection_context_t, connection_context_adapter_t,
                 I_P_List_counter,
                 I_P_List_fast_push_back<connection_context_t>>
    connection_context_queue_t;

enum class Connection_type { User, Admin_interface, Admin_privilege };
inline const char *connection_type_str(Connection_type ct) {
  switch (ct) {
    case Connection_type::User:
      return "User";
    case Connection_type::Admin_interface:
      return "Admin_interface";
    case Connection_type::Admin_privilege:
      return "Admin_privilege";
  }
  assert(false);
  return "<Unknown Connection_type>";
}

struct tp_group_t {
  /** Low level context for this group */
  tp_group_low_level_t *group_low_level_cntx{nullptr};
  /**
    This variable is used to indicate which thread is currently waiting
    for new queries to arrive. If it is NULL, no thread is waiting for
    queries to arrive, this is often the case, but after the thread has
    been declared as stalled, kept track of by process_count_time_unit in
    stall check thread) we will wake a thread to take up the waiting.
    */
  tp_thread_t *waiting_thread{nullptr};
  /** List of open connections */
  Open_connections_list open_connections;

  std::atomic<uint> num_query_threads{0};
  ulonglong last_thread_creation_time{0ULL};
  /**
    Queue of incoming connections to be processed for
    connection initialization and authentication.
    */
  connection_context_queue_t connection_context_queue;
  /** List of low prio queries */
  Query_list queued_queries;
  /** List of high prio queries */
  Query_list queued_trans;
  /** Mutex protecting this group */
  mysql_mutex_t LOCK_group;
  // Lock protecting connection context queue.
  mysql_mutex_t LOCK_connect;
  // Lock protecting query worker threads count during write.
  mysql_mutex_t LOCK_query_threads_count;
  /** Condition variable for start/stop of the threads */
  mysql_cond_t COND_group;
  /** Condition variable for consumer thread */
  mysql_cond_t COND_consumer;
  /** Condition variable for reserve thread */
  mysql_cond_t COND_reserve;
  // Condition variable for connect handler thread.
  mysql_cond_t COND_connect;
  /** Statistical variables */
  tp_group_statistics_t stats;
  /** Thread data for the threads in this group */
  tp_thread_t group_threads[MAX_THREADS_PER_GROUP];
  /** Count of number of threads waiting for consumer condition variable */
  std::uint32_t threads_for_consumer;
  /** Count of number of threads waiting for reserve condition variable */
  uint threads_for_reserve{0};
  /**
     Count of number of threads waiting waiting to handle
     connection initialization & authentication.
  */
  uint num_connect_handler_thread_in_sleep{0};

  /** Id of this thread group */
  uint group_idx{0};
  /** Number of threads initialized. tp_thread_group_state.THREAD_COUNT */
  uint threads_initialized{0};
  /** Max thread id  + 1 in the group */
  uint max_thread_ids_in_group{0};

  /*
    These are heavily used together in the main loop, so good if they can
    fit in the same cache line. Align this to 64 byte cache line if
    possible.
  */
  /**
    Statements arriving are put into high priority queue if either
    1) thd->transaction.is_active() reports TRUE OR
    2) high_priority_connection session variable is set on connection
    This means that non-transactional queries will always be put to the
    low priority queue.
  */
  /**
    max_active_threads is the maximum number of active threads we are
    allowing to execute in parallel in this thread group. Whenever a
    thread is deemed as stalled this number will be incremented and
    similarly it is decremented when a stalled thread has completed
    query execution.
  */
  std::uint32_t max_active_threads{0};

  /** Number of threads identified as stalled by the last run of the
      stall checker thread. */
  std::uint32_t stalled_threads = 0;

  /**
    active_threads keeps track of the current number of active threads.
    When thd_wait_begin is called this number will be decremented for
    non-stalled threads. This variable isn't protected by the mutex,
    it is updated using atomic instructions.
  */
  std::atomic<std::uint32_t> threads_active{0};

  // Count of threads executing transactions.
  std::atomic<std::uint32_t> trxn_threads{0};
  /**
     Total number of threads that handle
     connection authentication & initialization.
  */
  std::atomic<int> connect_threads{0};
  /** Max number of non-stalled threads in group active */
  uint threads_user_request{0};

  // Count of query worker threads.
  std::atomic<uint32_t> query_threads_count{0};

  Time_pt earliest_con_expire_timpt = Time_pt::max();

  bool is_kill_request_pending = false;

#ifndef NDEBUG
  std::atomic_bool stop_in_waiter_flush = false;
#endif /* NDEBUG */

  /**
   Time point when the stall checker last observed so many longrunning
   transactions that MTL must be suspended. This variable both records the
   time elapsed since last check, and serves a flag for the suspension of
   MTL. Values > BEGINNING_OF_EPOCH means that MTL is suspended. Only used if
   MTL is enabled.
  */
  Time_pt time_of_last_longrunning_trxs_check;
};

enum cleanup_level {
  TP_CLEANUP_IDLE = 0,
  TP_CLEANUP_STARTED = 1,
  TP_CLEANUP_COMPLETED = 2
};

inline const char *cleanup_level_str(cleanup_level cl) {
  switch (cl) {
    case TP_CLEANUP_IDLE:
      return "TP_CLEANUP_IDLE";
    case TP_CLEANUP_STARTED:
      return "TP_CLEANUP_STARTED";
    case TP_CLEANUP_COMPLETED:
      return "TP_CLEANUP_COMPLETED";
    default:
      assert(false);
      return "<Unknown cleanup level>";
  }
}

enum killed_state {
  TP_CONNECTION_NOT_KILLED = 0,
  TP_CONNECTION_KILLED_WAITING_FLAG = 1,
  TP_CONNECTION_KILLED_RECEIVED_FLAG = 2
};
inline const char *killed_state_str(killed_state ks) {
  switch (ks) {
    case TP_CONNECTION_NOT_KILLED:
      return "TP_CONNECTION_NOT_KILLED";
    case TP_CONNECTION_KILLED_WAITING_FLAG:
      return "TP_CONNECTION_KILLED_WAITING_FLAG";
    case TP_CONNECTION_KILLED_RECEIVED_FLAG:
      return "TP_CONNECTION_KILLED_RECEIVED_FLAG";
    default:
      assert(false);
      return "<Unknown killed state>";
  }
}

/** States for connections in the thread pool, epresented by
    tp_client_low_level_t */
enum class Connection_state {
  // Connection and its THD has been created, but its fd is
  // not being monitored for incoming queries.
  ESTABLISHED,

  // The connection's file descriptor has been added to the
  // set which will be monitored by the next epoll_wait() call.
  ARMED,

  // Event (query) was detected on connection and it queued
  // for later execution
  QUEUED,

  // A worker considered the query for execution but found that
  // configured limits prevented it (see function is_credit_available)
  WAITING_FOR_CREDIT,

  // Connection is attached to a worker thread which will process
  // its event(query)
  ATTACHED,

  // A kill request was recieved for the connection.
  KILLED,

  // The connection had been inactive longer than the
  // configured limit.
  EXPIRED
};

#define CONNECTION_STATE_CASE__(ev) \
  case Connection_state::ev:        \
    return #ev;
inline const char *connection_state_str(Connection_state cs) {
  switch (cs) {
    CONNECTION_STATE_CASE__(ESTABLISHED);
    CONNECTION_STATE_CASE__(ARMED);
    CONNECTION_STATE_CASE__(QUEUED);
    CONNECTION_STATE_CASE__(WAITING_FOR_CREDIT);
    CONNECTION_STATE_CASE__(ATTACHED);
    CONNECTION_STATE_CASE__(KILLED);
    CONNECTION_STATE_CASE__(EXPIRED);
    default:
      return "<Unknown Connection_state>";
  }
}

struct tp_client_low_level_t {
#ifdef HAVE_EPOLL
  struct epoll_event ev;
  my_socket epoll_fd;
#else
  /*
    The low level group object is heavily used in poll implementation
    and thus placed here in "hot" region of data structure.
  */
  tp_group_low_level_t *group;
  uint index;
  uint fill_to_16_bytes;
#endif
  /* Low level info is 16 bytes */
  my_socket fd;
  uint active_flag;
  uint ready_to_execute_flag;
  killed_state connection_killed_state;
  /* Next 16 bytes, total 32 bytes */
  tp_thread_t *processing_thread;
  THD *thd;
  // Time when connection added to queue, waiting to be executed
  Time_pt time_of_enqueueing;

  tp_client_low_level_t *next_client_query;
  tp_client_low_level_t **prev_client_query;
  /* Next 32 bytes, total 64 bytes */

  /* Uncommonly used variables */
  THD_wait_type wait_type = THD_WAIT_NONE;
#ifdef ENABLED_DEBUG_SYNC
  uint debug_sync_unavailable;
#endif
  cleanup_level cleanup_state;
  tp_client_low_level_t *next_open_connection;
  tp_client_low_level_t **prev_open_connection;
#ifdef HAVE_EPOLL
  /*
    Low level group object very rarely used in epoll implementation and
    thus placed in "cold" region.
  */
  tp_group_low_level_t *group;
#endif
  /* Uncommon part is 32 bytes for epoll and 24 bytes for poll */
#ifndef NDEBUG
  bool set_explain;
  char dbug_explain[512];
#endif
  Time_pt expiry_timpt;
  // Same as thd_get_thread_id(), but we only set it when establishing the
  // connection so should be safe to access also from threads not attached to
  // this THD.
  // Note that we assume that the following variables are protected by
  // tp_group_t::LOCK_group, and allow other threads such as stall_checker
  // and PFS connection threads to access it. Meaning that the worker threads
  // need to acquire the lock prior to accessing this variable, even after
  // setting active_flag to true.
  unsigned long connection_id = 0;
  Time_pt time_of_last_event_completion;

  Connection_state conn_state = Connection_state::ESTABLISHED;

  std::atomic_uint64_t direct_events;
  std::atomic_uint64_t queued_events;

  // Point in time when poll()/epoll returned with an event for this connection
  std::atomic<Time_pt> time_of_event_arrival;

  // Duration from when the event arrived to the start of execution.
  std::atomic<Musec> management_time;

  // When added to TP by connection handler
  Time_pt time_of_add;

  // When removed from connection_context_queue by
  // handler thread
  Time_pt time_of_pop;

  // When first armed and ready to receive queries
  Time_pt time_of_arm;

  // index of connection handler thread which
  // processed connection
  std::uint32_t conn_handler_index = 0;
  Connection_type type = Connection_type::User;
  bool is_queued = false;
};

/**
  Helper class which specifies which members of tp_client_low_level_t are
  used for participation in the list of open connections for the group.
*/
struct tp_client_low_level_open_conn_adapter_t
    : public I_P_List_adapter<tp_client_low_level_t,
                              &tp_client_low_level_t::next_open_connection,
                              &tp_client_low_level_t::prev_open_connection> {};

/**
  Helper class which specifies which members of tp_client_low_level_t are
  used for participation in the list of queued queries for the group.
*/
struct tp_client_low_level_query_adapter_t
    : public I_P_List_adapter<tp_client_low_level_t,
                              &tp_client_low_level_t::next_client_query,
                              &tp_client_low_level_t::prev_client_query> {};

/* Low level group interface */

/**
  Get group data given low level context.

  @param group_cntx       Low level group context

  @retval                 The thread group data
*/
tp_group_t *tp_group_low_level_get_tp_group(tp_group_low_level_t *group_cntx);

/**
  Initialisation of low level group data

  @param my_tp_group            Thread group data

  @retval                       Low level group context
  @retval                       NULL, failed to allocate group context
*/
tp_group_low_level_t *tp_group_low_level_init(tp_group_t *my_tp_group);

/**
  End of low level group data

  @param group_cntx             Low level group context
*/
void tp_group_low_level_end(tp_group_low_level_t *group_cntx);

/**
  Flush wait context.

  @param group_cntx       Low level group context
*/
void tp_group_low_level_waiter_flush(tp_group_low_level_t *group_cntx);

/**
  This method is used to poll for events, in some cases the execution
  of the event also happens in this method.

  @param group_cntx              Low level group context
  @param my_tp_group             Thread group data
  @param my_thread_data          Thread data
  @param next_events             Events reported back
  @param wait                    Should we poll with wait

  @returns                       Number of events received
*/
int tp_group_low_level_wait_for_events(tp_group_low_level_t *group_cntx,
                                       tp_group_t *my_tp_group,
                                       tp_thread_t *my_thread_data,
                                       tp_client_low_level_t **next_events,
                                       bool wait);

/* Low level client interface */

/**
  Initialise low level client

  @param thd                    THD object
  @param group_cntx             Low level group context

  @retval                       Low level client context created
  @retval                       NULL if failure to allocate
*/
tp_client_low_level_t *tp_client_low_level_init(
    THD *thd, tp_group_low_level_t *group_cntx);

/**
  Release low level client

  @param client_cntx            Low level client context
*/
void tp_client_low_level_end(tp_client_low_level_t *client_cntx);

/**
  Arm the client which means we are polling on this socket.

  @param client_cntx            Low level client context

  @retval                       true if failure, false if success
*/
bool tp_client_low_level_arm(tp_client_low_level_t *client_cntx);

/**
  Disarm the client which means we are no longer polling on this socket.

  @param client_cntx            Low level client context

  @retval                       true if failure, false if success
*/
bool tp_client_low_level_disarm(tp_client_low_level_t *client_cntx);

/**
  Rearm the client which means we are now polling on this socket again.

  @param client_cntx            Low level client context
  @param my_tp_group            Thread group data

  @retval                       true if failure, false if success
*/
bool tp_client_low_level_rearm(tp_client_low_level_t *client_cntx,
                               tp_group_t *my_tp_group);

/**
  Send wake flag to waiters

  @param group_cntx       Low level group context
  @param flag             No flag or KILL_FLAG

  @retval                 true if failure, false if success
*/
bool tp_group_low_level_waiter_wake(tp_group_low_level_t *group_cntx,
                                    char flag);
