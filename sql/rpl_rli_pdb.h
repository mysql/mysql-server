/* Copyright (c) 2011, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RPL_RLI_PDB_H
#define RPL_RLI_PDB_H

#include <stdarg.h>
#include <sys/types.h>
#include <time.h>
#include <atomic>
#include <tuple>

#include "my_bitmap.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_psi_config.h"
#include "mysql/binlog/event/binlog_event.h"
#include "mysql/components/services/bits/mysql_cond_bits.h"
#include "mysql/components/services/bits/mysql_mutex_bits.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/bits/psi_mutex_bits.h"
#include "mysql/my_loglevel.h"
#include "mysql/service_mysql_alloc.h"
#include "prealloced_array.h"  // Prealloced_array
#include "sql/log_event.h"     // Format_description_log_event
#include "sql/rpl_gtid.h"
#include "sql/rpl_mta_submode.h"  // enum_mts_parallel_type
#include "sql/rpl_replica.h"      // MTS_WORKER_UNDEF
#include "sql/rpl_rli.h"          // Relay_log_info
#include "sql/sql_class.h"
#include "sql/system_variables.h"

class Rpl_info_handler;
class Slave_worker;
struct TABLE;

#ifndef NDEBUG
extern ulong w_rr;
#endif
/**
  Legends running throughout the module:

  C  - Coordinator
  CP - checkpoint
  W  - Worker

  B-event event that Begins a group (a transaction)
  T-event event that Terminates a group (a transaction)
*/

/* Assigned Partition Hash (APH) entry */
struct db_worker_hash_entry {
  uint db_len;
  const char *db;
  Slave_worker *worker;
  /*
    The number of transaction pending on this database.
    This should only be modified under the lock slave_worker_hash_lock.
   */
  long usage;
  /*
    The list of temp tables belonging to @ db database is
    attached to an assigned @c worker to become its thd->temporary_tables.
    The list is updated with every ddl incl CREATE, DROP.
    It is removed from the entry and merged to the coordinator's
    thd->temporary_tables in case of events: slave stops, APH oversize.
  */
  TABLE *volatile temporary_tables;

  /* todo: relax concurrency to mimic record-level locking.
     That is to augmenting the entry with mutex/cond pair
     pthread_mutex_t
     pthread_cond_t
     timestamp updated_at; */
};

bool init_hash_workers(Relay_log_info *rli);
void destroy_hash_workers(Relay_log_info *);
Slave_worker *map_db_to_worker(const char *dbname, Relay_log_info *rli,
                               db_worker_hash_entry **ptr_entry,
                               bool need_temp_tables, Slave_worker *w);
Slave_worker *get_least_occupied_worker(Relay_log_info *rli,
                                        Slave_worker_array *workers,
                                        Log_event *ev);

#define SLAVE_INIT_DBS_IN_GROUP 4  // initial allocation for CGEP dynarray

struct Slave_job_group {
  Slave_job_group() = default;

  /*
    We need a custom copy constructor and assign operator because std::atomic<T>
    is not copy-constructible.
  */
  Slave_job_group(const Slave_job_group &other)
      : group_master_log_name(other.group_master_log_name),
        group_master_log_pos(other.group_master_log_pos),
        group_relay_log_name(other.group_relay_log_name),
        group_relay_log_pos(other.group_relay_log_pos),
        worker_id(other.worker_id),
        worker(other.worker),
        total_seqno(other.total_seqno),
        master_log_pos(other.master_log_pos),
        checkpoint_seqno(other.checkpoint_seqno),
        checkpoint_log_pos(other.checkpoint_log_pos),
        checkpoint_log_name(other.checkpoint_log_name),
        checkpoint_relay_log_pos(other.checkpoint_relay_log_pos),
        checkpoint_relay_log_name(other.checkpoint_relay_log_name),
        done(other.done.load()),
        shifted(other.shifted),
        ts(other.ts),
#ifndef NDEBUG
        notified(other.notified),
#endif
        last_committed(other.last_committed),
        sequence_number(other.sequence_number),
        new_fd_event(other.new_fd_event) {
  }

  Slave_job_group &operator=(const Slave_job_group &other) {
    group_master_log_name = other.group_master_log_name;
    group_master_log_pos = other.group_master_log_pos;
    group_relay_log_name = other.group_relay_log_name;
    group_relay_log_pos = other.group_relay_log_pos;
    worker_id = other.worker_id;
    worker = other.worker;
    total_seqno = other.total_seqno;
    master_log_pos = other.master_log_pos;
    checkpoint_seqno = other.checkpoint_seqno;
    checkpoint_log_pos = other.checkpoint_log_pos;
    checkpoint_log_name = other.checkpoint_log_name;
    checkpoint_relay_log_pos = other.checkpoint_relay_log_pos;
    checkpoint_relay_log_name = other.checkpoint_relay_log_name;
    done.store(other.done.load());
    shifted = other.shifted;
    ts = other.ts;
#ifndef NDEBUG
    notified = other.notified;
#endif
    last_committed = other.last_committed;
    sequence_number = other.sequence_number;
    new_fd_event = other.new_fd_event;
    return *this;
  }

  char *group_master_log_name;  // (actually redundant)
  /*
    T-event lop_pos filled by Worker for CheckPoint (CP)
  */
  my_off_t group_master_log_pos;

  /*
     When relay-log name changes  allocates and fill in a new name of relay-log,
     otherwise it fills in NULL.
     Coordinator keeps track of each Worker has been notified on the updating
     to make sure the routine runs once per change.

     W checks the value at commit and memorizes a not-NULL.
     Freeing unless NULL is left to Coordinator at CP.
  */
  char *group_relay_log_name;    // The value is last seen relay-log
  my_off_t group_relay_log_pos;  // filled by W
  ulong worker_id;
  Slave_worker *worker;
  ulonglong total_seqno;

  my_off_t master_log_pos;  // B-event log_pos
  /* checkpoint coord are reset by periodical and special (Rotate event) CP:s */
  uint checkpoint_seqno;
  my_off_t checkpoint_log_pos;  // T-event lop_pos filled by W for CheckPoint
  char *checkpoint_log_name;
  my_off_t
      checkpoint_relay_log_pos;  // T-event lop_pos filled by W for CheckPoint
  char *checkpoint_relay_log_name;
  std::atomic<int32> done;  // Flag raised by W,  read and reset by Coordinator
  ulong shifted;            // shift the last CP bitmap at receiving a new CP
  time_t ts;                // Group's timestamp to update Seconds_behind_source
#ifndef NDEBUG
  bool notified{false};  // to debug group_master_log_name change notification
#endif
  /* Clock-based scheduler requirement: */
  longlong last_committed;   // commit parent timestamp
  longlong sequence_number;  // transaction's logical timestamp
  /*
    After Coordinator has seen a new FD event, it sets this member to
    point to the new event, once per worker. Coordinator does so
    when it schedules a first group following the FD event to a worker.
    It checks Slave_worker::fd_change_notified flag to decide whether
    to do this or not.
    When the worker executes the group, it replaces its currently
    active FD by the new FD once it takes on the group first event. It
    checks this member and resets it after the FD replacement is done.

    The member is kind of lock-free. It's updated by Coordinator and
    read by Worker without holding any mutex. That's still safe thanks
    to Slave_worker::jobs_lock that works as synchronizer, Worker
    can't read any stale info.
    The member is updated by Coordinator when it decides which Worker
    an event following a new FD is to be scheduled.
    After Coordinator has chosen a Worker, it queues the event to it
    with necessarily taking Slave_worker::jobs_lock. The Worker grabs
    the mutex lock later at pulling the event from the queue and
    releases the lock before to read from this member.

    This sequence of actions shows the write operation always precedes
    the read one, and ensures no stale FD info is passed to the
    Worker.
  */
  Format_description_log_event *new_fd_event;
  /*
    Coordinator fills the struct with defaults and options at starting of
    a group distribution.
  */
  void reset(my_off_t master_pos, ulonglong seqno) {
    master_log_pos = master_pos;
    group_master_log_pos = group_relay_log_pos = 0;
    group_master_log_name = nullptr;  // todo: remove
    group_relay_log_name = nullptr;
    worker_id = MTS_WORKER_UNDEF;
    total_seqno = seqno;
    checkpoint_log_name = nullptr;
    checkpoint_log_pos = 0;
    checkpoint_relay_log_name = nullptr;
    checkpoint_relay_log_pos = 0;
    checkpoint_seqno = (uint)-1;
    done = 0;
    ts = 0;
#ifndef NDEBUG
    notified = false;
#endif
    last_committed = SEQ_UNINIT;
    sequence_number = SEQ_UNINIT;
    new_fd_event = nullptr;
  }
};

/**
   The class defines a type of queue with a predefined max capacity that is
   implemented using the circular memory buffer.
   That is items of the queue are accessed as indexed elements of
   the array buffer in a way that when the index value reaches
   a max value it wraps around to point to the first buffer element.
*/
template <typename Element_type>
class circular_buffer_queue {
 public:
  Prealloced_array<Element_type, 1> m_Q;
  /**
     The capacity and maximum length of the queue in terms of element.
  */
  size_t capacity;
  /**
     Its value modulo `capacity` is index of the element where the next element
     will be enqueued. It's entry+length. It may be bigger than capacity, but
     will be smaller than 2*capacity.
  */
  size_t avail;
  /**
     The head index of the queue. It is an index of next element that will be
     dequeued. It is less than capacity, so it is an actual index (in contrast
     to `avail`), don't need to be calculated modulo `capacity`.
  */
  size_t entry;
  /**
     Actual length. It can be read while not protected by any mutex.
  */
  std::atomic<size_t> len;
  bool inited_queue;

  circular_buffer_queue(size_t max)
      : m_Q(PSI_INSTRUMENT_ME),
        capacity(max),
        avail(0),
        entry(0),
        len(0),
        inited_queue(false) {
    if (!m_Q.reserve(capacity)) inited_queue = true;
    m_Q.resize(capacity);
  }
  circular_buffer_queue() : m_Q(PSI_INSTRUMENT_ME), inited_queue(false) {}
  ~circular_buffer_queue() = default;

  /**
     Content of the being dequeued item is copied to the arg-pointer
     location.

     @param [out] item A pointer to the being dequeued item.
     @return true if an element was returned, false if the queue was empty.
  */
  bool de_queue(Element_type *item);
  /**
    Similar to de_queue but extracting happens from the tail side.

    @param [out] item A pointer to the being dequeued item.
    @return true if an element was returned, false if the queue was empty.
  */
  bool de_tail(Element_type *item);

  /**
    return the index where the arg item locates
           or an error encoded as a value `circular_buffer_queue::error_result`.
  */
  size_t en_queue(Element_type *item);
  /**
     return the value of @c data member of the head of the queue.
  */
  Element_type *head_queue() {
    if (empty()) return nullptr;
    return &m_Q[entry];
  }

  /* index is within the valid range */
  bool in(size_t i) {
    return (avail >= capacity) ? (entry <= i || i < avail - capacity)
                               : (entry <= i && i < avail);
  }
  size_t get_length() const { return len.load(std::memory_order_relaxed); }
  bool empty() const { return get_length() == 0; }
  bool full() const { return get_length() == capacity; }

  static constexpr size_t error_result = std::numeric_limits<size_t>::max();
};

/**
  Group Assigned Queue whose first element identifies first gap
  in committed sequence. The head of the queue is therefore next to
  the low-water-mark.
*/
class Slave_committed_queue : public circular_buffer_queue<Slave_job_group> {
 public:
  bool inited;

  /* master's Rot-ev exec */
  void update_current_binlog(const char *post_rotate);

  /*
     The last checkpoint time Low-Water-Mark
  */
  Slave_job_group lwm;

  /* last time processed indexes for each worker */
  Prealloced_array<ulonglong, 1> last_done;

  /* the being assigned group index in GAQ */
  ulong assigned_group_index;

  Slave_committed_queue(size_t max, uint n);

  ~Slave_committed_queue() {
    if (inited) {
      my_free(lwm.group_relay_log_name);
      free_dynamic_items();  // free possibly left allocated strings in GAQ list
    }
  }

#ifndef NDEBUG
  bool count_done(Relay_log_info *rli);
#endif

  /* Checkpoint routine refreshes the queue */
  size_t move_queue_head(Slave_worker_array *ws);
  /* Method is for slave shutdown time cleanup */
  void free_dynamic_items();
  /*
     returns a pointer to Slave_job_group struct instance as indexed by arg
     in the circular buffer dyn-array
  */
  Slave_job_group *get_job_group(size_t ind) {
    assert(ind < capacity);
    return &m_Q[ind];
  }

  /**
     Assigns @c assigned_group_index to an index of enqueued item
     and returns it.
  */
  size_t en_queue(Slave_job_group *item) {
    return assigned_group_index =
               circular_buffer_queue<Slave_job_group>::en_queue(item);
  }

  /**
    Dequeue from head.

    @param [out] item A pointer to the being dequeued item.
    @return true if an element was returned, false if the queue was empty.
  */
  bool de_queue(Slave_job_group *item) {
    return circular_buffer_queue<Slave_job_group>::de_queue(item);
  }

  /**
    Similar to de_queue() but removing an item from the tail side.

    @param [out] item A pointer to the being dequeued item.
  @return true if an element was returned, false if the queue was empty.
  */
  bool de_tail(Slave_job_group *item) {
    return circular_buffer_queue<Slave_job_group>::de_tail(item);
  }

  size_t find_lwm(Slave_job_group **, size_t);
};

/**
    @return  the index where the arg item has been located
             or an error encoded as a value
             `circular_buffer_queue::error_result`.
*/
template <typename Element_type>
size_t circular_buffer_queue<Element_type>::en_queue(Element_type *item) {
  if (full()) {
    return error_result;
  }

  const auto ret = (avail++) % capacity;
  m_Q[ret] = *item;
  len++;
  assert(len == avail - entry);
  assert(entry < avail);

  return ret;
}

/**
  Dequeue from head.

  @param [out] item A pointer to the being dequeued item.
  @return true if an element was returned, false if the queue was empty.
*/
template <typename Element_type>
bool circular_buffer_queue<Element_type>::de_queue(Element_type *item) {
  if (empty()) {
    return false;
  }
  *item = m_Q[entry++];
  len--;
  assert(len == avail - entry);
  assert(entry <= avail);

  // The start of the queue just have returned to the first index. Normalize
  // indexes so they are small again.
  if (entry == capacity) {
    entry = 0;
    avail -= capacity;
    assert(avail < capacity);
    assert(avail == len);
  }
  return true;
}

template <typename Element_type>
bool circular_buffer_queue<Element_type>::de_tail(Element_type *item) {
  if (empty()) {
    return false;
  }

  assert(avail > entry);
  *item = m_Q[(--avail) % capacity];
  len--;
  assert(len == avail - entry);
  return true;
}

class Slave_jobs_queue : public circular_buffer_queue<Slave_job_item> {
 public:
  Slave_jobs_queue() : circular_buffer_queue<Slave_job_item>() {}
  /*
     Coordinator marks with true, Worker signals back at queue back to
     available
  */
  bool overfill;
  ulonglong waited_overfill;
};

class Slave_worker : public Relay_log_info {
 public:
  Slave_worker(Relay_log_info *rli,
#ifdef HAVE_PSI_INTERFACE
               PSI_mutex_key *param_key_info_run_lock,
               PSI_mutex_key *param_key_info_data_lock,
               PSI_mutex_key *param_key_info_sleep_lock,
               PSI_mutex_key *param_key_info_thd_lock,
               PSI_mutex_key *param_key_info_data_cond,
               PSI_mutex_key *param_key_info_start_cond,
               PSI_mutex_key *param_key_info_stop_cond,
               PSI_mutex_key *param_key_info_sleep_cond,
#endif
               uint param_id, const char *param_channel);

  ~Slave_worker() override;

  Slave_jobs_queue jobs;    // assignment queue containing events to execute
  mysql_mutex_t jobs_lock;  // mutex for the jobs queue
  mysql_cond_t jobs_cond;   // condition variable for the jobs queue
  Relay_log_info *c_rli;    // pointer to Coordinator's rli

  Prealloced_array<db_worker_hash_entry *, SLAVE_INIT_DBS_IN_GROUP>
      curr_group_exec_parts;  // Current Group Executed Partitions

#ifndef NDEBUG
  bool curr_group_seen_sequence_number;  // is set to true about starts_group()
#endif
  ulong id;  // numeric identifier of the Worker

  /*
    Worker runtime statistics
  */
  // the index in GAQ of the last processed group by this Worker
  volatile ulong last_group_done_index;
  ulonglong
      last_groups_assigned_index;  // index of previous group assigned to worker
  ulong wq_empty_waits;            // how many times got idle
  ulong events_done;               // how many events (statements) processed
  ulong groups_done;               // how many groups (transactions) processed
  std::atomic<int> curr_jobs;      // number of active  assignments
  // number of partitions allocated to the worker at point in time
  long usage_partition;
  // symmetric to rli->mts_end_group_sets_max_dbs
  bool end_group_sets_max_dbs;

  volatile bool relay_log_change_notified;  // Coord sets and resets, W can read
  volatile bool checkpoint_notified;        // Coord sets and resets, W can read
  volatile bool
      master_log_change_notified;  // Coord sets and resets, W can read
  /*
    The variable serves to Coordinator as a memo to itself
    to notify a Worker about the fact that a new FD has been read.
    Normally, the value is true, to mean the Worker is notified.
    When Coordinator reads a new FD it changes the value to false.
    When Coordinator schedules to a Worker the first event following the new FD,
    it propagates the new FD to the Worker through
    Slave_job_group::new_fd_event. Afterwards Coordinator returns the value back
    to the regular true, to denote things done. Worker will adapt to the new FD
    once it takes on a first event of the marked group.
  */
  bool fd_change_notified;
  ulong bitmap_shifted;  // shift the last bitmap at receiving new CP
  // WQ current excess above the overrun level
  long wq_overrun_cnt;
  /*
    number of events starting from which Worker queue is regarded as
    close to full. The number of the excessive events yields a weight factor
    to compute Coordinator's nap.
  */
  ulong overrun_level;
  /*
     reverse to overrun: the number of events below which Worker is
     considered under-running
  */
  ulong underrun_level;
  /*
    Total of increments done to rli->mts_wq_excess_cnt on behalf of this worker.
    When WQ length is dropped below overrun the counter is reset.
  */
  ulong excess_cnt;
  /*
    Coordinates of the last CheckPoint (CP) this Worker has
    acknowledged; part of is persistent data
  */
  char checkpoint_relay_log_name[FN_REFLEN];
  ulonglong checkpoint_relay_log_pos;
  char checkpoint_master_log_name[FN_REFLEN];
  ulonglong checkpoint_master_log_pos;
  MY_BITMAP group_executed;  // bitmap describes groups executed after last CP
  MY_BITMAP group_shifted;   // temporary bitmap to compute group_executed
  ulong
      worker_checkpoint_seqno;  // the most significant ON bit in group_executed
  /* Initial value of FD-for-execution version until it's gets known. */
  ulong server_version;
  enum en_running_state {
    NOT_RUNNING = 0,
    RUNNING = 1,
    ERROR_LEAVING = 2,  // is set by Worker
    STOP = 3,           // is set by Coordinator upon receiving STOP
    STOP_ACCEPTED =
        4  // is set by worker upon completing job when STOP REPLICA is issued
  };

  /*
    This function is used to make a copy of the worker object before we
    destroy it on STOP REPLICA. This new object is then used to report the
    worker status until next START REPLICA following which the new worker
    objects will be used.
  */
  void copy_values_for_PFS(ulong worker_id, en_running_state running_status,
                           THD *worker_thd, const Error &last_error,
                           Gtid_monitoring_info *monitoring_info_arg);

  /*
    The running status is guarded by jobs_lock mutex that a writer
    Coordinator or Worker itself needs to hold when write a new value.
  */
  en_running_state volatile running_status;
  /*
    exit_incremented indicates whether worker has contributed to max updated
    index. By default it is set to false. When the worker contributes for the
    first time this variable is set to true.
  */
  bool exit_incremented;

  int init_worker(Relay_log_info *, ulong);
  int rli_init_info(bool);
  int flush_info(bool force = false);
  static size_t get_number_worker_fields();
  /**
     Sets bits for columns that are allowed to be `NULL`.

     @param nullable_fields the bitmap to hold the nullable fields.
  */
  static void set_nullable_fields(MY_BITMAP *nullable_fields);
  void slave_worker_ends_group(Log_event *, int);
  const char *get_master_log_name();
  ulonglong get_master_log_pos() { return master_log_pos; }
  ulonglong set_master_log_pos(ulong val) { return master_log_pos = val; }
  bool commit_positions(Log_event *evt, Slave_job_group *ptr_g, bool force);
  /**
    The method is a wrapper to provide uniform interface with STS and is
    to be called from Relay_log_info and Slave_worker pre_commit() methods.
  */
  bool commit_positions() override {
    assert(current_event);

    return commit_positions(
        current_event, c_rli->gaq->get_job_group(current_event->mts_group_idx),
        is_transactional());
  }
  /**
    See the comments for STS version of this method.
  */
  void post_commit(bool on_rollback) override {
    if (on_rollback) {
      if (is_transactional())
        rollback_positions(
            c_rli->gaq->get_job_group(current_event->mts_group_idx));
    } else if (!is_transactional())
      commit_positions(current_event,
                       c_rli->gaq->get_job_group(current_event->mts_group_idx),
                       true);
  }
  /*
    When commit fails clear bitmap for executed worker group. Revert back the
    positions to the old positions that existed before commit using the
    checkpoint.

    @param Slave_job_group a pointer to Slave_job_group struct instance which
    holds group master log pos, group relay log pos and checkpoint positions.
  */
  void rollback_positions(Slave_job_group *ptr_g);
  bool reset_recovery_info();
  /**
    The method runs at Worker initialization, at runtime when
    Coordinator supplied a new FD event for execution context, and at
    the Worker pool shutdown.
    Similarly to the Coordinator's
    Relay_log_info::set_rli_description_event() the possibly existing
    old FD is destroyed, carefully; each worker decrements
    Format_description_log_event::atomic_usage_counter and when it is made
    zero the destructor runs.
    Unlike to Coordinator's role, the usage counter of the new FD is *not*
    incremented, see @c Log_event::get_slave_worker() where and why it's done
    there.

    Notice, the method is run as well by Coordinator per each Worker at MTS
    shutdown time.

    Todo: consider to merge logics of the method with that of
    Relay_log_info class.

    @param fdle   pointer to a new Format_description_log_event

    @return 1 if an error was encountered, 0 otherwise.
  */
  int set_rli_description_event(Format_description_log_event *fdle) override {
    DBUG_TRACE;

    if (fdle) {
      /*
        When the master rotates its binary log, set gtid_next to
        NOT_YET_DETERMINED.  This tells the slave thread that:

        - If a Gtid_log_event is read subsequently, gtid_next will be set to the
          given GTID (this is done in gtid_pre_statement_checks()).

        - If a statement is executed before any Gtid_log_event, then gtid_next
          is set to anonymous (this is done in Gtid_log_event::do_apply_event().

        It is important to not set GTID_NEXT=NOT_YET_DETERMINED in the middle of
        a transaction.  If that would happen when GTID_MODE=ON, the next
        statement would fail because it implicitly sets GTID_NEXT=ANONYMOUS,
        which is disallowed when GTID_MODE=ON.  So then there would be no way to
        end the transaction; any attempt to do so would result in this error.

        There are three possible states when reaching this execution flow point
        (see further below for a more detailed explanation on each):

        - **No active transaction, and not in a group**: set `gtid_next` to
          `NOT_YET_DETERMINED`.

        - **No active transaction, and in a group**: do nothing regarding
          `gtid_next`.

        - **An active transaction exists**: impossible to set `gtid_next` and no
          reason to process the `Format_description` event so, trigger an error.

        For the sake of correctness, let's defined the meaning of having a
        transaction "active" or "in a group".

        A transaction is "active" if either BEGIN was executed or autocommit=0
        and a DML statement was executed (@see
        THD::in_active_multi_stmt_transaction).

        A transaction is "in a group" if it is applied by the replication
        applier, and the relay log position is between Gtid_log_event and the
        committing event (@see Relay_log_info::is_in_group).

        The three different states explained further:

        **No active transaction, and not in a group**: It is normal to have
        gtid_next=automatic/undefined and have a Format_description_log_event in
        this condition. We are outside transaction context and should set
        gtid_next to not_yet_determined.

        **No active transaction, and in a group**: Having
        gtid_next=automatic/undefined in a group is impossible if master is 5.7
        or later, because the group always starts with a Gtid_log_event or an
        Anonymous_gtid_log_event, which will set gtid_next to anonymous or
        gtid. But it is possible to have gtid_next=undefined when replicating
        from a 5.6 master with gtid_mode=off, because it does not generate any
        such event. And then, it is possible to have no active transaction in a
        group if the master has logged a DDL as a User_var_log_event followed by
        a Query_log_event. The User_var_log_event will start a group, but not
        start an active transaction or change gtid_next. In this case, it is
        possible that a Format_description_log_event occurs, if the group
        (transaction) is broken on two relay logs, so that User_var_log_event
        appears at the end of one relay log and Query_log_event at the beginning
        of the next one. In such cases, we should not set gtid_next.

        **An active transaction exists**: It is possible to have
        gtid_next=automatic/undefined in an active transaction, only if
        gtid_next=automatic, which is only possible in a client connection using
        gtid_next=automatic. In this scenario, there is no reason to execute a
        Format_description_log_event. So we generate an error.
      */
      if (info_thd->variables.gtid_next.is_automatic() ||
          info_thd->variables.gtid_next.is_undefined()) {
        bool in_active_multi_stmt =
            info_thd->in_active_multi_stmt_transaction();

        if (!is_in_group() && !in_active_multi_stmt) {
          DBUG_PRINT("info",
                     ("Setting gtid_next.type to NOT_YET_DETERMINED_GTID"));
          info_thd->variables.gtid_next.set_not_yet_determined();
        } else if (in_active_multi_stmt) {
          my_error(ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION, MYF(0),
                   "gtid_next");
          return 1;
        }
      }
      adapt_to_master_version_updown(fdle->get_product_version(),
                                     get_master_server_version());
    }
    if (rli_description_event) {
      assert(rli_description_event->atomic_usage_counter > 0);

      if (--rli_description_event->atomic_usage_counter == 0) {
        /* The being deleted by Worker FD can't be the latest one */
        assert(rli_description_event != c_rli->get_rli_description_event());

        delete rli_description_event;
      }
    }
    rli_description_event = fdle;

    return 0;
  }

  inline void reset_gaq_index() { gaq_index = c_rli->gaq->capacity; }
  inline void set_gaq_index(ulong val) {
    if (gaq_index == c_rli->gaq->capacity) gaq_index = val;
  }

  int slave_worker_exec_event(Log_event *ev);

  /**
    Make the necessary changes to both the `Slave_worker` and current
    `Log_event` objects, before retrying to apply the transaction.

    Since the event is going to be re-read from the relay-log file, there
    may be actions needed to be taken to reset the state both of `this`
    instance, as well as of the current `Log_event` being processed.

    @param event The `Log_event` object currently being processed.
   */
  void prepare_for_retry(Log_event &event);

  /**
    Checks if the transaction can be retried, and if not, reports an error.

    @param[in] thd          The THD object of current thread.

    @returns std::tuple<bool, bool, uint> where each element has
              following meaning:

              first element of tuple is function return value and determines:
                false  if the transaction should be retried
                true   if the transaction should not be retried

              second element of tuple determines:
                the function will set the value to true, in case the retry
                should be "silent". Silent means that the caller should not
                report it in performance_schema tables, write to the error log,
                or sleep. Currently, silent is used by NDB only.

              third element of tuple determines:
                If the caller should report any other error than that stored in
                thd->get_stmt_da()->mysql_errno(), then this function will store
                that error in this third element of the tuple.

  */
  std::tuple<bool, bool, uint> check_and_report_end_of_retries(THD *thd);

  /**
    It is called after an error happens. It checks if that is an temporary
    error and if the transaction should be retried. Then it will retry the
    transaction if it is allowed. Retry policy and logic is similar to
    single-threaded slave.

    @param[in] start_relay_number The extension number of the relay log which
                 includes the first event of the transaction.
    @param[in] start_relay_pos The offset of the transaction's first event.

    @param[in] end_relay_number The extension number of the relay log which
               includes the last event it should retry.
    @param[in] end_relay_pos The offset of the last event it should retry.

    @retval false if transaction succeeds (possibly after a number of retries)
    @retval true  if transaction fails
  */
  bool retry_transaction(uint start_relay_number, my_off_t start_relay_pos,
                         uint end_relay_number, my_off_t end_relay_pos);

  bool set_info_search_keys(Rpl_info_handler *to) override;

  /**
    Get coordinator's RLI. Especially used get the rli from
    a slave thread, like this: thd->rli_slave->get_c_rli();
    thd could be a SQL thread or a worker thread.
  */
  Relay_log_info *get_c_rli() override { return c_rli; }

  /**
     return an extension "for channel channel_name"
     for error messages per channel
  */
  const char *get_for_channel_str(bool upper_case = false) const override;

  longlong sequence_number() {
    Slave_job_group *ptr_g = c_rli->gaq->get_job_group(gaq_index);
    return ptr_g->sequence_number;
  }

  /**
     Return true if replica-preserve-commit-order is enabled and an
     earlier transaction is waiting for a row-level lock held by this
     transaction.
  */
  bool found_commit_order_deadlock();

  /**
     Called when replica-preserve-commit-order is enabled, by the worker
     processing an earlier transaction that waits on a row-level lock
     held by this worker's transaction.
  */
  void report_commit_order_deadlock();

  /**
    @return either the master server version as extracted from the last
            installed Format_description_log_event, or when it was not
            installed then the slave own server version.
  */
  ulong get_master_server_version() {
    return !get_rli_description_event()
               ? server_version
               : get_rli_description_event()->get_product_version();
  }

 protected:
  void do_report(loglevel level, int err_code, const char *msg,
                 va_list v_args) const override
      MY_ATTRIBUTE((format(printf, 4, 0)));

  void do_report(loglevel level, int err_code,
                 const Gtid_specification *gtid_next, const char *msg,
                 va_list v_args) const override
      MY_ATTRIBUTE((format(printf, 5, 0)));

 private:
  ulong gaq_index;           // GAQ index of the current assignment
  ulonglong master_log_pos;  // event's cached log_pos for possible error report
  void end_info();
  bool read_info(Rpl_info_handler *from) override;
  bool write_info(Rpl_info_handler *to) override;
  std::atomic<bool> m_commit_order_deadlock;

  /// This flag indicates whether positions were already modified during the
  /// event processing, if yes, positions are not updated in the
  /// slave_worker_ends_group function
  bool m_flag_positions_committed = false;

  Slave_worker &operator=(const Slave_worker &info);
  Slave_worker(const Slave_worker &info);
  bool worker_sleep(ulong seconds);
  bool read_and_apply_events(uint start_relay_number, my_off_t start_relay_pos,
                             uint end_relay_number, my_off_t end_relay_pos);
  void assign_partition_db(Log_event *ev);

 public:
  /**
    Set the flag the signals a deadlock to false
  */
  void reset_commit_order_deadlock();

  /**
     Returns an array with the expected column numbers of the primary key
     fields of the table repository.
  */
  static const uint *get_table_pk_field_indexes();
  /**
     Returns the index of the Channel_name field of the table repository.
  */
  static uint get_channel_field_index();
};

bool handle_slave_worker_stop(Slave_worker *worker, Slave_job_item *job_item);
bool set_max_updated_index_on_stop(Slave_worker *worker,
                                   Slave_job_item *job_item);

TABLE *mts_move_temp_table_to_entry(TABLE *, THD *, db_worker_hash_entry *);
TABLE *mts_move_temp_tables_to_thd(THD *, TABLE *);
// Auxiliary function
TABLE *mts_move_temp_tables_to_thd(THD *, TABLE *, enum_mts_parallel_type);

bool append_item_to_jobs(slave_job_item *job_item, Slave_worker *w,
                         Relay_log_info *rli);

inline Slave_worker *get_thd_worker(const THD *thd) {
  return static_cast<Slave_worker *>(thd->rli_slave);
}

int slave_worker_exec_job_group(Slave_worker *w, Relay_log_info *rli);

#endif
