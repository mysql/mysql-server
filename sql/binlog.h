#ifndef BINLOG_H_INCLUDED
/* Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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

#define BINLOG_H_INCLUDED

#include "sql_class.h"
#include "my_global.h"
#include "m_string.h"                  // llstr
#include "binlog_event.h"              // enum_binlog_checksum_alg
#include "mysqld.h"                    // opt_relay_logname
#include "tc_log.h"                    // TC_LOG
#include "atomic_class.h"
#include "rpl_gtid.h"                  // Gtid_set, Sid_map
#include "rpl_trx_tracking.h"


class Relay_log_info;
class Master_info;
class Slave_worker;
class Format_description_log_event;
class Transaction_boundary_parser;
class Rows_log_event;
class Rows_query_log_event;
class Incident_log_event;
class Log_event;
class Gtid_set;
struct Gtid;

typedef int64 query_id_t;


/**
  Class for maintaining the commit stages for binary log group commit.
 */
class Stage_manager {
public:
  class Mutex_queue {
    friend class Stage_manager;
  public:
    Mutex_queue()
      : m_first(NULL), m_last(&m_first), m_size(0)
    {
    }

    void init(
#ifdef HAVE_PSI_INTERFACE
              PSI_mutex_key key_LOCK_queue
#endif
              ) {
      mysql_mutex_init(key_LOCK_queue, &m_lock, MY_MUTEX_INIT_FAST);
    }

    void deinit() {
      mysql_mutex_destroy(&m_lock);
    }

    bool is_empty() const {
      return m_first == NULL;
    }

    /**
      Append a linked list of threads to the queue.
      @retval true The queue was empty before this operation.
      @retval false The queue was non-empty before this operation.
    */
    bool append(THD *first);

    /**
       Fetch the entire queue for a stage.

       This will fetch the entire queue in one go.
    */
    THD *fetch_and_empty();

    std::pair<bool,THD*> pop_front();

    inline int32 get_size()
    {
      return my_atomic_load32(&m_size);
    }

  private:
    void lock() { mysql_mutex_lock(&m_lock); }
    void unlock() { mysql_mutex_unlock(&m_lock); }

    /**
       Pointer to the first thread in the queue, or NULL if the queue is
       empty.
    */
    THD *m_first;

    /**
       Pointer to the location holding the end of the queue.

       This is either @c &first, or a pointer to the @c next_to_commit of
       the last thread that is enqueued.
    */
    THD **m_last;

    /** size of the queue */
    int32 m_size;

    /** Lock for protecting the queue. */
    mysql_mutex_t m_lock;
    /*
      This attribute did not have the desired effect, at least not according
      to -fsanitize=undefined with gcc 5.2.1
      Also: it fails to compile with gcc 7.2
     */
  }; // MY_ATTRIBUTE((aligned(CPU_LEVEL1_DCACHE_LINESIZE)));

public:
  Stage_manager()
  {
  }

  ~Stage_manager()
  {
  }

  /**
     Constants for queues for different stages.
   */
  enum StageID {
    FLUSH_STAGE,
    SYNC_STAGE,
    COMMIT_STAGE,
    STAGE_COUNTER
  };

  void init(
#ifdef HAVE_PSI_INTERFACE
            PSI_mutex_key key_LOCK_flush_queue,
            PSI_mutex_key key_LOCK_sync_queue,
            PSI_mutex_key key_LOCK_commit_queue,
            PSI_mutex_key key_LOCK_done,
            PSI_cond_key key_COND_done
#endif
            )
  {
    mysql_mutex_init(key_LOCK_done, &m_lock_done, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_COND_done, &m_cond_done);
#ifndef DBUG_OFF
    /* reuse key_COND_done 'cos a new PSI object would be wasteful in !DBUG_OFF */
    mysql_cond_init(key_COND_done, &m_cond_preempt);
#endif
    m_queue[FLUSH_STAGE].init(
#ifdef HAVE_PSI_INTERFACE
                              key_LOCK_flush_queue
#endif
                              );
    m_queue[SYNC_STAGE].init(
#ifdef HAVE_PSI_INTERFACE
                             key_LOCK_sync_queue
#endif
                             );
    m_queue[COMMIT_STAGE].init(
#ifdef HAVE_PSI_INTERFACE
                               key_LOCK_commit_queue
#endif
                               );
  }

  void deinit()
  {
    for (size_t i = 0 ; i < STAGE_COUNTER ; ++i)
      m_queue[i].deinit();
    mysql_cond_destroy(&m_cond_done);
    mysql_mutex_destroy(&m_lock_done);
  }

  /**
    Enroll a set of sessions for a stage.

    This will queue the session thread for writing and flushing.

    If the thread being queued is assigned as stage leader, it will
    return immediately.

    If wait_if_follower is true the thread is not the stage leader,
    the thread will be wait for the queue to be processed by the
    leader before it returns.
    In DBUG-ON version the follower marks is preempt status as ready.

    @param stage Stage identifier for the queue to append to.
    @param first Queue to append.
    @param stage_mutex
                 Pointer to the currently held stage mutex, or NULL if
                 we're not in a stage.

    @retval true  Thread is stage leader.
    @retval false Thread was not stage leader and processing has been done.
   */
  bool enroll_for(StageID stage, THD *first, mysql_mutex_t *stage_mutex);

  std::pair<bool,THD*> pop_front(StageID stage)
  {
    return m_queue[stage].pop_front();
  }

#ifndef DBUG_OFF
  /**
     The method ensures the follower's execution path can be preempted
     by the leader's thread.
     Preempt status of @c head follower is checked to engange the leader
     into waiting when set.

     @param head  THD* of a follower thread
  */
  void clear_preempt_status(THD *head);
#endif

  /**
    Fetch the entire queue and empty it.

    @return Pointer to the first session of the queue.
   */
  THD *fetch_queue_for(StageID stage) {
    DBUG_PRINT("debug", ("Fetching queue for stage %d", stage));
    return m_queue[stage].fetch_and_empty();
  }

  /**
    Introduces a wait operation on the executing thread.  The
    waiting is done until the timeout elapses or count is
    reached (whichever comes first).

    If count == 0, then the session will wait until the timeout
    elapses. If timeout == 0, then there is no waiting.

    @param usec     the number of microseconds to wait.
    @param count    wait for as many as count to join the queue the
                    session is waiting on
    @param stage    which stage queue size to compare count against.
   */
  void wait_count_or_timeout(ulong count, ulong usec, StageID stage);

  void signal_done(THD *queue);

private:
  /**
     Queues for sessions.

     We need two queues:
     - Waiting. Threads waiting to be processed
     - Committing. Threads waiting to be committed.
   */
  Mutex_queue m_queue[STAGE_COUNTER];

  /** Condition variable to indicate that the commit was processed */
  mysql_cond_t m_cond_done;

  /** Mutex used for the condition variable above */
  mysql_mutex_t m_lock_done;
#ifndef DBUG_OFF
  /** Flag is set by Leader when it starts waiting for follower's all-clear */
  bool leader_await_preempt_status;

  /** Condition variable to indicate a follower started waiting for commit */
  mysql_cond_t m_cond_preempt;
#endif
};

/* log info errors */
#define LOG_INFO_EOF -1
#define LOG_INFO_IO  -2
#define LOG_INFO_INVALID -3
#define LOG_INFO_SEEK -4
#define LOG_INFO_MEM -6
#define LOG_INFO_FATAL -7
#define LOG_INFO_IN_USE -8
#define LOG_INFO_EMFILE -9

/* bitmap to MYSQL_BIN_LOG::close() */
#define LOG_CLOSE_INDEX		1
#define LOG_CLOSE_TO_BE_OPENED	2
#define LOG_CLOSE_STOP_EVENT	4


/*
  Note that we destroy the lock mutex in the destructor here.
  This means that object instances cannot be destroyed/go out of scope
  until we have reset thd->current_linfo to NULL;
 */
typedef struct st_log_info
{
  char log_file_name[FN_REFLEN];
  my_off_t index_file_offset, index_file_start_offset;
  my_off_t pos;
  bool fatal; // if the purge happens to give us a negative offset
  int entry_index; //used in purge_logs(), calculatd in find_log_pos().
  st_log_info()
    : index_file_offset(0), index_file_start_offset(0),
      pos(0), fatal(0), entry_index(0)
    {
      memset(log_file_name, 0, FN_REFLEN);
    }
} LOG_INFO;

/*
  TODO use mmap instead of IO_CACHE for binlog
  (mmap+fsync is two times faster than write+fsync)
*/

class MYSQL_BIN_LOG: public TC_LOG
{
  enum enum_log_state { LOG_OPENED, LOG_CLOSED, LOG_TO_BE_OPENED };

  /* LOCK_log is inited by init_pthread_objects() */
  mysql_mutex_t LOCK_log;
  char *name;
  char log_file_name[FN_REFLEN];
  char db[NAME_LEN + 1];
  bool write_error, inited;
  IO_CACHE log_file;
  const enum cache_type io_cache_type;
#ifdef HAVE_PSI_INTERFACE
  /** Instrumentation key to use for file io in @c log_file */
  PSI_file_key m_log_file_key;
  /** The instrumentation key to use for @ LOCK_log. */
  PSI_mutex_key m_key_LOCK_log;
  /** The instrumentation key to use for @ LOCK_index. */
  PSI_mutex_key m_key_LOCK_index;
  /** The instrumentation key to use for @ LOCK_binlog_end_pos. */
  PSI_mutex_key m_key_LOCK_binlog_end_pos;

  PSI_mutex_key m_key_COND_done;

  PSI_mutex_key m_key_LOCK_commit_queue;
  PSI_mutex_key m_key_LOCK_done;
  PSI_mutex_key m_key_LOCK_flush_queue;
  PSI_mutex_key m_key_LOCK_sync_queue;
  /** The instrumentation key to use for @ LOCK_commit. */
  PSI_mutex_key m_key_LOCK_commit;
  /** The instrumentation key to use for @ LOCK_sync. */
  PSI_mutex_key m_key_LOCK_sync;
  /** The instrumentation key to use for @ LOCK_xids. */
  PSI_mutex_key m_key_LOCK_xids;
  /** The instrumentation key to use for @ update_cond. */
  PSI_cond_key m_key_update_cond;
  /** The instrumentation key to use for @ prep_xids_cond. */
  PSI_cond_key m_key_prep_xids_cond;
  /** The instrumentation key to use for opening the log file. */
  PSI_file_key m_key_file_log;
  /** The instrumentation key to use for opening the log index file. */
  PSI_file_key m_key_file_log_index;
  /** The instrumentation key to use for opening a log cache file. */
  PSI_file_key m_key_file_log_cache;
  /** The instrumentation key to use for opening a log index cache file. */
  PSI_file_key m_key_file_log_index_cache;
#endif
  /* POSIX thread objects are inited by init_pthread_objects() */
  mysql_mutex_t LOCK_index;
  mysql_mutex_t LOCK_commit;
  mysql_mutex_t LOCK_sync;
  mysql_mutex_t LOCK_binlog_end_pos;
  mysql_mutex_t LOCK_xids;
  mysql_cond_t update_cond;

  my_off_t binlog_end_pos;
  ulonglong bytes_written;
  IO_CACHE index_file;
  char index_file_name[FN_REFLEN];
  /*
    crash_safe_index_file is temp file used for guaranteeing
    index file crash safe when master server restarts.
  */
  IO_CACHE crash_safe_index_file;
  char crash_safe_index_file_name[FN_REFLEN];
  /*
    purge_file is a temp file used in purge_logs so that the index file
    can be updated before deleting files from disk, yielding better crash
    recovery. It is created on demand the first time purge_logs is called
    and then reused for subsequent calls. It is cleaned up in cleanup().
  */
  IO_CACHE purge_index_file;
  char purge_index_file_name[FN_REFLEN];
  /*
     The max size before rotation (usable only if log_type == LOG_BIN: binary
     logs and relay logs).
     For a binlog, max_size should be max_binlog_size.
     For a relay log, it should be max_relay_log_size if this is non-zero,
     max_binlog_size otherwise.
     max_size is set in init(), and dynamically changed (when one does SET
     GLOBAL MAX_BINLOG_SIZE|MAX_RELAY_LOG_SIZE) by fix_max_binlog_size and
     fix_max_relay_log_size).
  */
  ulong max_size;

  // current file sequence number for load data infile binary logging
  uint file_id;
  uint open_count;				// For replication
  int readers_count;

  /* pointer to the sync period variable, for binlog this will be
     sync_binlog_period, for relay log this will be
     sync_relay_log_period
  */
  uint *sync_period_ptr;
  uint sync_counter;

  mysql_cond_t m_prep_xids_cond;
  Atomic_int32 m_prep_xids;

  /**
    Increment the prepared XID counter.
   */
  void inc_prep_xids(THD *thd);

  /**
    Decrement the prepared XID counter.

    Signal m_prep_xids_cond if the counter reaches zero.
   */
  void dec_prep_xids(THD *thd);

  int32 get_prep_xids() {
    int32 result= m_prep_xids.atomic_get();
    return result;
  }

  inline uint get_sync_period()
  {
    return *sync_period_ptr;
  }

  int write_to_file(IO_CACHE *cache);
  /*
    This is used to start writing to a new log file. The difference from
    new_file() is locking. new_file_without_locking() does not acquire
    LOCK_log.
  */
  int new_file_without_locking(Format_description_log_event *extra_description_event);
  int new_file_impl(bool need_lock, Format_description_log_event *extra_description_event);

  /** Manage the stages in ordered_commit. */
  Stage_manager stage_manager;
  void do_flush(THD *thd);

  bool open(
#ifdef HAVE_PSI_INTERFACE
            PSI_file_key log_file_key,
#endif
            const char *log_name,
            const char *new_name);
  bool init_and_set_log_file_name(const char *log_name,
                                  const char *new_name);
  int generate_new_name(char *new_name, const char *log_name);

public:
  const char *generate_name(const char *log_name, const char *suffix,
                            char *buff);
  bool is_open() { return log_state.atomic_get() != LOG_CLOSED; }

  /* This is relay log */
  bool is_relay_log;
  ulong signal_cnt;  // update of the counter is checked by heartbeat
  uint8 checksum_alg_reset; // to contain a new value when binlog is rotated
  /*
    Holds the last seen in Relay-Log FD's checksum alg value.
    The initial value comes from the slave's local FD that heads
    the very first Relay-Log file. In the following the value may change
    with each received master's FD_m.
    Besides to be used in verification events that IO thread receives
    (except the 1st fake Rotate, see @c Master_info:: checksum_alg_before_fd), 
    the value specifies if/how to compute checksum for slave's local events
    and the first fake Rotate (R_f^1) coming from the master.
    R_f^1 needs logging checksum-compatibly with the RL's heading FD_s.

    Legends for the checksum related comments:

    FD     - Format-Description event,
    R      - Rotate event
    R_f    - the fake Rotate event
    E      - an arbirary event

    The underscore indexes for any event
    `_s'   indicates the event is generated by Slave
    `_m'   - by Master

    Two special underscore indexes of FD:
    FD_q   - Format Description event for queuing   (relay-logging)
    FD_e   - Format Description event for executing (relay-logging)

    Upper indexes:
    E^n    - n:th event is a sequence

    RL     - Relay Log
    (A)    - checksum algorithm descriptor value
    FD.(A) - the value of (A) in FD
  */
  binary_log::enum_binlog_checksum_alg relay_log_checksum_alg;

  MYSQL_BIN_LOG(uint *sync_period,
                enum cache_type io_cache_type_arg);
  /*
    note that there's no destructor ~MYSQL_BIN_LOG() !
    The reason is that we don't want it to be automatically called
    on exit() - but only during the correct shutdown process
  */

#ifdef HAVE_PSI_INTERFACE
  void set_psi_keys(PSI_mutex_key key_LOCK_index,
                    PSI_mutex_key key_LOCK_commit,
                    PSI_mutex_key key_LOCK_commit_queue,
                    PSI_mutex_key key_LOCK_done,
                    PSI_mutex_key key_LOCK_flush_queue,
                    PSI_mutex_key key_LOCK_log,
                    PSI_mutex_key key_LOCK_binlog_end_pos,
                    PSI_mutex_key key_LOCK_sync,
                    PSI_mutex_key key_LOCK_sync_queue,
                    PSI_mutex_key key_LOCK_xids,
                    PSI_cond_key key_COND_done,
                    PSI_cond_key key_update_cond,
                    PSI_cond_key key_prep_xids_cond,
                    PSI_file_key key_file_log,
                    PSI_file_key key_file_log_index,
                    PSI_file_key key_file_log_cache,
                    PSI_file_key key_file_log_index_cache)
  {
    m_key_COND_done= key_COND_done;

    m_key_LOCK_commit_queue= key_LOCK_commit_queue;
    m_key_LOCK_done= key_LOCK_done;
    m_key_LOCK_flush_queue= key_LOCK_flush_queue;
    m_key_LOCK_sync_queue= key_LOCK_sync_queue;

    m_key_LOCK_index= key_LOCK_index;
    m_key_LOCK_log= key_LOCK_log;
    m_key_LOCK_binlog_end_pos= key_LOCK_binlog_end_pos;
    m_key_LOCK_commit= key_LOCK_commit;
    m_key_LOCK_sync= key_LOCK_sync;
    m_key_LOCK_xids= key_LOCK_xids;
    m_key_update_cond= key_update_cond;
    m_key_prep_xids_cond= key_prep_xids_cond;
    m_key_file_log= key_file_log;
    m_key_file_log_index= key_file_log_index;
    m_key_file_log_cache= key_file_log_cache;
    m_key_file_log_index_cache= key_file_log_index_cache;
  }
#endif

public:
  /** Manage the MTS dependency tracking */
  Transaction_dependency_tracker m_dependency_tracker;

  /**
    Find the oldest binary log that contains any GTID that
    is not in the given gtid set.

    @param[out] binlog_file_name, the file name of oldest binary log found
    @param[in]  gtid_set, the given gtid set
    @param[out] first_gtid, the first GTID information from the binary log
                file returned at binlog_file_name
    @param[out] errmsg, the error message outputted, which is left untouched
                if the function returns false
    @return false on success, true on error.
  */
  bool find_first_log_not_in_gtid_set(char *binlog_file_name,
                                      const Gtid_set *gtid_set,
                                      Gtid *first_gtid,
                                      const char **errmsg);

  /**
    Reads the set of all GTIDs in the binary/relay log, and the set
    of all lost GTIDs in the binary log, and stores each set in
    respective argument.

    @param gtid_set Will be filled with all GTIDs in this binary/relay
    log.
    @param lost_groups Will be filled with all GTIDs in the
    Previous_gtids_log_event of the first binary log that has a
    Previous_gtids_log_event. This is requested to binary logs but not
    to relay logs.
    @param verify_checksum If true, checksums will be checked.
    @param need_lock If true, LOCK_log, LOCK_index, and
    global_sid_lock->wrlock are acquired; otherwise they are asserted
    to be taken already.
    @param trx_parser [out] This will be used to return the actual
    relaylog transaction parser state because of the possibility
    of partial transactions.
    @param [out] gtid_partial_trx If a transaction was left incomplete
    on the relaylog, it's GTID should be returned to be used in the
    case of the rest of the transaction be added to the relaylog.
    @param is_server_starting True if the server is starting.
    @return false on success, true on error.
  */
  bool init_gtid_sets(Gtid_set *gtid_set, Gtid_set *lost_groups,
                      bool verify_checksum,
                      bool need_lock,
                      Transaction_boundary_parser *trx_parser,
                      Gtid *gtid_partial_trx,
                      bool is_server_starting= false);

  void set_previous_gtid_set_relaylog(Gtid_set *previous_gtid_set_param)
  {
    DBUG_ASSERT(is_relay_log);
    previous_gtid_set_relaylog= previous_gtid_set_param;
  }
  /**
    If the thread owns a GTID, this function generates an empty
    transaction and releases ownership of the GTID.

    - If the binary log is disabled for this thread, the GTID is
      inserted directly into the mysql.gtid_executed table and the
      GTID is included in @@global.gtid_executed.  (This only happens
      for DDL, since DML will save the GTID into table and release
      ownership inside ha_commit_trans.)

    - If the binary log is enabled for this thread, an empty
      transaction consisting of GTID, BEGIN, COMMIT is written to the
      binary log, the GTID is included in @@global.gtid_executed, and
      the GTID is added to the mysql.gtid_executed table on the next
      binlog rotation.

    This function must be called by any committing statement (COMMIT,
    implicitly committing statements, or Xid_log_event), after the
    statement has completed execution, regardless of whether the
    statement updated the database.

    This logic ensures that an empty transaction is generated for the
    following cases:

    - Explicit empty transaction:
      SET GTID_NEXT = 'UUID:NUMBER'; BEGIN; COMMIT;

    - Transaction or DDL that gets completely filtered out in the
      slave thread.

    @param thd The committing thread

    @retval 0 Success
    @retval nonzero Error
  */
  int gtid_end_transaction(THD *thd);
private:
  Atomic_int32 log_state; /* atomic enum_log_state */

  /* The previous gtid set in relay log. */
  Gtid_set* previous_gtid_set_relaylog;

  int open(const char *opt_name) { return open_binlog(opt_name); }
  bool change_stage(THD *thd, Stage_manager::StageID stage,
                    THD* queue, mysql_mutex_t *leave,
                    mysql_mutex_t *enter);
  std::pair<int,my_off_t> flush_thread_caches(THD *thd);
  int flush_cache_to_file(my_off_t *flush_end_pos);
  int finish_commit(THD *thd);
  std::pair<bool, bool> sync_binlog_file(bool force);
  void process_commit_stage_queue(THD *thd, THD *queue);
  void process_after_commit_stage_queue(THD *thd, THD *first);
  int process_flush_stage_queue(my_off_t *total_bytes_var, bool *rotate_var,
                                THD **out_queue_var);
  int ordered_commit(THD *thd, bool all, bool skip_commit = false);
  void handle_binlog_flush_or_sync_error(THD *thd, bool need_lock_log);
public:
  int open_binlog(const char *opt_name);
  void close();
  enum_result commit(THD *thd, bool all);
  int rollback(THD *thd, bool all);
  int prepare(THD *thd, bool all);
  int recover(IO_CACHE *log, Format_description_log_event *fdle,
              my_off_t *valid_pos);
  int recover(IO_CACHE *log, Format_description_log_event *fdle);
#if !defined(MYSQL_CLIENT)

  void update_thd_next_event_pos(THD *thd);
  int flush_and_set_pending_rows_event(THD *thd, Rows_log_event* event,
                                       bool is_transactional);

#endif /* !defined(MYSQL_CLIENT) */
  void add_bytes_written(ulonglong inc)
  {
    bytes_written += inc;
  }
  void reset_bytes_written()
  {
    bytes_written = 0;
  }
  void harvest_bytes_written(ulonglong* counter)
  {
#ifndef DBUG_OFF
    char buf1[22],buf2[22];
#endif
    DBUG_ENTER("harvest_bytes_written");
    (*counter)+=bytes_written;
    DBUG_PRINT("info",("counter: %s  bytes_written: %s", llstr(*counter,buf1),
		       llstr(bytes_written,buf2)));
    bytes_written=0;
    DBUG_VOID_RETURN;
  }
  void set_max_size(ulong max_size_arg);
  void signal_update()
  {
    DBUG_ENTER("MYSQL_BIN_LOG::signal_update");
    signal_cnt++;
    mysql_cond_broadcast(&update_cond);
    DBUG_VOID_RETURN;
  }

  void update_binlog_end_pos()
  {
    /*
      binlog_end_pos is used only on master's binlog right now. It is possible
      to use it on relay log.
    */
    if (is_relay_log)
      signal_update();
    else
    {
      lock_binlog_end_pos();
      binlog_end_pos= my_b_tell(&log_file);
      signal_update();
      unlock_binlog_end_pos();
    }
  }

  void update_binlog_end_pos(my_off_t pos)
  {
    lock_binlog_end_pos();
    if (pos > binlog_end_pos)
      binlog_end_pos= pos;
    signal_update();
    unlock_binlog_end_pos();
  }

  int wait_for_update_relay_log(THD* thd, const struct timespec * timeout);
  int wait_for_update_bin_log(THD* thd, const struct timespec * timeout);
  bool do_write_cache(IO_CACHE *cache, class Binlog_event_writer *writer);
public:
  void init_pthread_objects();
  void cleanup();
  /**
    Create a new binary log.
    @param log_name Name of binlog
    @param new_name Name of binlog, too. todo: what's the difference
    between new_name and log_name?
    @param max_size The size at which this binlog will be rotated.
    @param null_created If false, and a Format_description_log_event
    is written, then the Format_description_log_event will have the
    timestamp 0. Otherwise, it the timestamp will be the time when the
    event was written to the log.
    @param need_lock_index If true, LOCK_index is acquired; otherwise
    LOCK_index must be taken by the caller.
    @param need_sid_lock If true, the read lock on global_sid_lock
    will be acquired.  Otherwise, the caller must hold the read lock
    on global_sid_lock.
  */
  bool open_binlog(const char *log_name,
                   const char *new_name,
                   ulong max_size,
                   bool null_created,
                   bool need_lock_index, bool need_sid_lock,
                   Format_description_log_event *extra_description_event);
  bool open_index_file(const char *index_file_name_arg,
                       const char *log_name, bool need_lock_index);
  /* Use this to start writing a new log file */
  int new_file(Format_description_log_event *extra_description_event);

  bool write_event(Log_event* event_info);
  bool write_cache(THD *thd, class binlog_cache_data *binlog_cache_data,
                   class Binlog_event_writer *writer);
  /**
    Assign automatic generated GTIDs for all commit group threads in the flush
    stage having gtid_next.type == AUTOMATIC_GROUP.

    @param first_seen The first thread seen entering the flush stage.
    @return Returns false if succeeds, otherwise true is returned.
  */
  bool assign_automatic_gtids_to_flush_group(THD *first_seen);
  bool write_gtid(THD *thd, binlog_cache_data *cache_data,
                  class Binlog_event_writer *writer);

  /**
     Write a dml into statement cache and then flush it into binlog. It writes
     Gtid_log_event and BEGIN, COMMIT automatically.

     It is aimed to handle cases of "background" logging where a statement is
     logged indirectly, like "DELETE FROM a_memory_table". So don't use it on any
     normal statement.

     @param[IN] thd  the THD object of current thread.
     @param[IN] stmt the DELETE statement.
     @param[IN] stmt_len the length of DELETE statement.

     @return Returns false if succeeds, otherwise true is returned.
  */
  bool write_dml_directly(THD* thd, const char *stmt, size_t stmt_len);

  void set_write_error(THD *thd, bool is_transactional);
  bool check_write_error(THD *thd);
  bool write_incident(THD *thd, bool need_lock_log,
                      const char* err_msg,
                      bool do_flush_and_sync= true);
  bool write_incident(Incident_log_event *ev, THD *thd,
                      bool need_lock_log,
                      const char* err_msg,
                      bool do_flush_and_sync= true);

  void start_union_events(THD *thd, query_id_t query_id_param);
  void stop_union_events(THD *thd);
  bool is_query_in_union(THD *thd, query_id_t query_id_param);

#ifdef HAVE_REPLICATION
  bool append_buffer(const char* buf, uint len, Master_info *mi);
  bool append_event(Log_event* ev, Master_info *mi);
private:
  bool after_append_to_relay_log(Master_info *mi);
#endif // ifdef HAVE_REPLICATION
public:

  void make_log_name(char* buf, const char* log_ident);
  bool is_active(const char* log_file_name);
  int remove_logs_from_index(LOG_INFO* linfo, bool need_update_threads);
  int rotate(bool force_rotate, bool* check_purge);
  void purge();
  int rotate_and_purge(THD* thd, bool force_rotate);
  /**
     Flush binlog cache and synchronize to disk.

     This function flushes events in binlog cache to binary log file,
     it will do synchronizing according to the setting of system
     variable 'sync_binlog'. If file is synchronized, @c synced will
     be set to 1, otherwise 0.

     @param[out] synced if not NULL, set to 1 if file is synchronized, otherwise 0
     @param[in] force if TRUE, ignores the 'sync_binlog' and synchronizes the file.

     @retval 0 Success
     @retval other Failure
  */
  bool flush_and_sync(const bool force= false);
  int purge_logs(const char *to_log, bool included,
                 bool need_lock_index, bool need_update_threads,
                 ulonglong *decrease_log_space, bool auto_purge);
  int purge_logs_before_date(time_t purge_time, bool auto_purge);
  int purge_first_log(Relay_log_info* rli, bool included);
  int set_crash_safe_index_file_name(const char *base_file_name);
  int open_crash_safe_index_file();
  int close_crash_safe_index_file();
  int add_log_to_index(uchar* log_file_name, size_t name_len,
                       bool need_lock_index);
  int move_crash_safe_index_file_to_index_file(bool need_lock_index);
  int set_purge_index_file_name(const char *base_file_name);
  int open_purge_index_file(bool destroy);
  bool is_inited_purge_index_file();
  int close_purge_index_file();
  int clean_purge_index_file();
  int sync_purge_index_file();
  int register_purge_index_entry(const char* entry);
  int register_create_index_entry(const char* entry);
  int purge_index_entry(THD *thd, ulonglong *decrease_log_space,
                        bool need_lock_index);
  bool reset_logs(THD* thd, bool delete_only= false);
  void close(uint exiting, bool need_lock_log, bool need_lock_index);

  // iterating through the log index file
  int find_log_pos(LOG_INFO* linfo, const char* log_name,
                   bool need_lock_index);
  int find_next_log(LOG_INFO* linfo, bool need_lock_index);
  int find_next_relay_log(char log_name[FN_REFLEN+1]);
  int get_current_log(LOG_INFO* linfo, bool need_lock_log= true);
  int raw_get_current_log(LOG_INFO* linfo);
  uint next_file_id();
  inline char* get_index_fname() { return index_file_name;}
  inline char* get_log_fname() { return log_file_name; }
  inline char* get_name() { return name; }
  inline mysql_mutex_t* get_log_lock() { return &LOCK_log; }
  inline mysql_cond_t* get_log_cond() { return &update_cond; }
  inline IO_CACHE* get_log_file() { return &log_file; }

  inline void lock_index() { mysql_mutex_lock(&LOCK_index);}
  inline void unlock_index() { mysql_mutex_unlock(&LOCK_index);}
  inline IO_CACHE *get_index_file() { return &index_file;}
  inline uint32 get_open_count() { return open_count; }

  /*
    It is called by the threads(e.g. dump thread) which want to read
    hot log without LOCK_log protection.
  */
  my_off_t get_binlog_end_pos() const
  {
    mysql_mutex_assert_not_owner(&LOCK_log);
    mysql_mutex_assert_owner(&LOCK_binlog_end_pos);
    return binlog_end_pos;
  }
  mysql_mutex_t* get_binlog_end_pos_lock() { return &LOCK_binlog_end_pos; }
  void lock_binlog_end_pos() { mysql_mutex_lock(&LOCK_binlog_end_pos); }
  void unlock_binlog_end_pos() { mysql_mutex_unlock(&LOCK_binlog_end_pos); }

  /**
    Deep copy global_sid_map to @param sid_map and
    gtid_state->get_executed_gtids() to @param gtid_set
    Both operations are done under LOCK_commit and global_sid_lock
    protection.

    @param[out] sid_map  The Sid_map to which global_sid_map will
                         be copied.
    @param[out] gtid_set The Gtid_set to which gtid_executed will
                         be copied.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int get_gtid_executed(Sid_map *sid_map, Gtid_set *gtid_set);

  /*
    True while rotating binlog, which is caused by logging Incident_log_event.
  */
  bool is_rotating_caused_by_incident;
};

typedef struct st_load_file_info
{
  THD* thd;
  my_off_t last_pos_in_file;
  bool wrote_create_file, log_delayed;
} LOAD_FILE_INFO;

extern MYSQL_PLUGIN_IMPORT MYSQL_BIN_LOG mysql_bin_log;

/**
  Check if at least one of transacaction and statement binlog caches contains
  an empty transaction, other one is empty or contains an empty transaction,
  which has two binlog events "BEGIN" and "COMMIT".

  @param thd The client thread that executed the current statement.

  @retval true  At least one of transacaction and statement binlog caches
                contains an empty transaction, other one is empty or
                contains an empty transaction.
  @retval false Otherwise.
*/
bool is_empty_transaction_in_binlog_cache(const THD* thd);
bool trans_has_updated_trans_table(const THD* thd);
bool stmt_has_updated_trans_table(Ha_trx_info* ha_list);
bool ending_trans(THD* thd, const bool all);
bool ending_single_stmt_trans(THD* thd, const bool all);
bool trans_cannot_safely_rollback(const THD* thd);
bool stmt_cannot_safely_rollback(const THD* thd);

int log_loaded_block(IO_CACHE* file);

/**
  Open a single binary log file for reading.
*/
File open_binlog_file(IO_CACHE *log, const char *log_file_name,
                      const char **errmsg);
int check_binlog_magic(IO_CACHE* log, const char** errmsg);
bool purge_master_logs(THD* thd, const char* to_log);
bool purge_master_logs_before_date(THD* thd, time_t purge_time);
bool show_binlog_events(THD *thd, MYSQL_BIN_LOG *binary_log);
bool mysql_show_binlog_events(THD* thd);
void check_binlog_cache_size(THD *thd);
void check_binlog_stmt_cache_size(THD *thd);
bool binlog_enabled();
void register_binlog_handler(THD *thd, bool trx);
int query_error_code(THD *thd, bool not_killed);

extern const char *log_bin_index;
extern const char *log_bin_basename;
extern bool opt_binlog_order_commits;

/**
  Turns a relative log binary log path into a full path, based on the
  opt_bin_logname or opt_relay_logname. Also trims the cr-lf at the
  end of the full_path before return to avoid any server startup
  problem on windows.

  @param from         The log name we want to make into an absolute path.
  @param to           The buffer where to put the results of the 
                      normalization.
  @param is_relay_log Switch that makes is used inside to choose which
                      option (opt_bin_logname or opt_relay_logname) to
                      use when calculating the base path.

  @returns true if a problem occurs, false otherwise.
 */

inline bool normalize_binlog_name(char *to, const char *from, bool is_relay_log)
{
  DBUG_ENTER("normalize_binlog_name");
  bool error= false;
  char buff[FN_REFLEN];
  char *ptr= (char*) from;
  char *opt_name= is_relay_log ? opt_relay_logname : opt_bin_logname;

  DBUG_ASSERT(from);

  /* opt_name is not null and not empty and from is a relative path */
  if (opt_name && opt_name[0] && from && !test_if_hard_path(from))
  {
    // take the path from opt_name
    // take the filename from from 
    char log_dirpart[FN_REFLEN], log_dirname[FN_REFLEN];
    size_t log_dirpart_len, log_dirname_len;
    dirname_part(log_dirpart, opt_name, &log_dirpart_len);
    dirname_part(log_dirname, from, &log_dirname_len);

    /* log may be empty => relay-log or log-bin did not 
        hold paths, just filename pattern */
    if (log_dirpart_len > 0)
    {
      /* create the new path name */
      if(fn_format(buff, from+log_dirname_len, log_dirpart, "",
                   MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH)) == NULL)
      {
        error= true;
        goto end;
      }

      ptr= buff;
    }
  }

  DBUG_ASSERT(ptr);
  if (ptr)
  {
    size_t length= strlen(ptr);

    // Strips the CR+LF at the end of log name and \0-terminates it.
    if (length && ptr[length-1] == '\n')
    {
      ptr[length-1]= 0;
      length--;
      if (length && ptr[length-1] == '\r')
      {
        ptr[length-1]= 0;
        length--;
      }
    }
    if (!length)
    {
      error= true;
      goto end;
    }
    strmake(to, ptr, length);
  }
end:
  DBUG_RETURN(error);
}
#endif /* BINLOG_H_INCLUDED */
