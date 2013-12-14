#ifndef BINLOG_H_INCLUDED
/* Copyright (c) 2010, 2011, 2012, 2013 Oracle and/or its affiliates. All rights reserved.

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

#include "mysqld.h"                             /* opt_relay_logname */
#include "log_event.h"
#include "log.h"

class Relay_log_info;
class Master_info;

class Format_description_log_event;

/**
  Class for maintaining the commit stages for binary log group commit.
 */
class Stage_manager {
public:
  class Mutex_queue {
    friend class Stage_manager;
  public:
    Mutex_queue()
      : m_first(NULL), m_last(&m_first)
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

    /** Append a linked list of threads to the queue */
    bool append(THD *first);

    /**
       Fetch the entire queue for a stage.

       This will fetch the entire queue in one go.
    */
    THD *fetch_and_empty();

    std::pair<bool,THD*> pop_front();

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

    /** Lock for protecting the queue. */
    mysql_mutex_t m_lock;
  } __attribute__((aligned(CPU_LEVEL1_DCACHE_LINESIZE)));

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
    mysql_cond_init(key_COND_done, &m_cond_done, NULL);
#ifndef DBUG_OFF
    /* reuse key_COND_done 'cos a new PSI object would be wasteful in DBUG_ON */
    mysql_cond_init(key_COND_done, &m_cond_preempt, NULL);
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

  void signal_done(THD *queue) {
    mysql_mutex_lock(&m_lock_done);
    for (THD *thd= queue ; thd ; thd = thd->next_to_commit)
      thd->transaction.flags.pending= false;
    mysql_mutex_unlock(&m_lock_done);
    mysql_cond_broadcast(&m_cond_done);
  }

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


class MYSQL_BIN_LOG: public TC_LOG, private MYSQL_LOG
{
 private:
#ifdef HAVE_PSI_INTERFACE
  /** The instrumentation key to use for @ LOCK_index. */
  PSI_mutex_key m_key_LOCK_index;

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
#endif
  /* POSIX thread objects are inited by init_pthread_objects() */
  mysql_mutex_t LOCK_index;
  mysql_mutex_t LOCK_commit;
  mysql_mutex_t LOCK_sync;
  mysql_mutex_t LOCK_xids;
  mysql_cond_t update_cond;
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

  my_atomic_rwlock_t m_prep_xids_lock;
  mysql_cond_t m_prep_xids_cond;
  volatile int32 m_prep_xids;

  /**
    Increment the prepared XID counter.
   */
  void inc_prep_xids(THD *thd) {
    DBUG_ENTER("MYSQL_BIN_LOG::inc_prep_xids");
    my_atomic_rwlock_wrlock(&m_prep_xids_lock);
#ifndef DBUG_OFF
    int result= my_atomic_add32(&m_prep_xids, 1);
#else
    (void) my_atomic_add32(&m_prep_xids, 1);
#endif
    DBUG_PRINT("debug", ("m_prep_xids: %d", result + 1));
    my_atomic_rwlock_wrunlock(&m_prep_xids_lock);
    thd->transaction.flags.xid_written= true;
    DBUG_VOID_RETURN;
  }

  /**
    Decrement the prepared XID counter.

    Signal m_prep_xids_cond if the counter reaches zero.
   */
  void dec_prep_xids(THD *thd) {
    DBUG_ENTER("MYSQL_BIN_LOG::dec_prep_xids");
    my_atomic_rwlock_wrlock(&m_prep_xids_lock);
    int32 result= my_atomic_add32(&m_prep_xids, -1);
    DBUG_PRINT("debug", ("m_prep_xids: %d", result - 1));
    my_atomic_rwlock_wrunlock(&m_prep_xids_lock);
    thd->transaction.flags.xid_written= false;
    /* If the old value was 1, it is zero now. */
    if (result == 1)
    {
      mysql_mutex_lock(&LOCK_xids);
      mysql_cond_signal(&m_prep_xids_cond);
      mysql_mutex_unlock(&LOCK_xids);
    }
    DBUG_VOID_RETURN;
  }

  int32 get_prep_xids() {
    my_atomic_rwlock_rdlock(&m_prep_xids_lock);
    int32 result= my_atomic_load32(&m_prep_xids);
    my_atomic_rwlock_rdunlock(&m_prep_xids_lock);
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

public:
  using MYSQL_LOG::generate_name;
  using MYSQL_LOG::is_open;

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
  uint8 relay_log_checksum_alg;

  MYSQL_BIN_LOG(uint *sync_period);
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
                    PSI_mutex_key key_LOCK_sync,
                    PSI_mutex_key key_LOCK_sync_queue,
                    PSI_mutex_key key_LOCK_xids,
                    PSI_cond_key key_COND_done,
                    PSI_cond_key key_update_cond,
                    PSI_cond_key key_prep_xids_cond,
                    PSI_file_key key_file_log,
                    PSI_file_key key_file_log_index)
  {
    m_key_COND_done= key_COND_done;

    m_key_LOCK_commit_queue= key_LOCK_commit_queue;
    m_key_LOCK_done= key_LOCK_done;
    m_key_LOCK_flush_queue= key_LOCK_flush_queue;
    m_key_LOCK_sync_queue= key_LOCK_sync_queue;

    m_key_LOCK_index= key_LOCK_index;
    m_key_LOCK_log= key_LOCK_log;
    m_key_LOCK_commit= key_LOCK_commit;
    m_key_LOCK_sync= key_LOCK_sync;
    m_key_LOCK_xids= key_LOCK_xids;
    m_key_update_cond= key_update_cond;
    m_key_prep_xids_cond= key_prep_xids_cond;
    m_key_file_log= key_file_log;
    m_key_file_log_index= key_file_log_index;
  }
#endif
  /**
    Find the oldest binary log that contains any GTID that
    is not in the given gtid set.

    @param[out] binlog_file_name, the file name of oldest binary log found
    @param[in]  gtid_set, the given gtid set
    @param[out] errmsg, the error message outputted, which is left untouched
                if the function returns false
    @return false on success, true on error.
  */
  bool find_first_log_not_in_gtid_set(char *binlog_file_name,
                                      const Gtid_set *gtid_set,
                                      const char **errmsg);

  /**
    Reads the set of all GTIDs in the binary log, and the set of all
    lost GTIDs in the binary log, and stores each set in respective
    argument.

    @param gtid_set Will be filled with all GTIDs in this binary log.
    @param lost_groups Will be filled with all GTIDs in the
    Previous_gtids_log_event of the first binary log that has a
    Previous_gtids_log_event.
    @param last_gtid Will be filled with the last availble GTID information
    in the binary/relay log files.
    @param verify_checksum If true, checksums will be checked.
    @param need_lock If true, LOCK_log, LOCK_index, and
    global_sid_lock->wrlock are acquired; otherwise they are asserted
    to be taken already.
    @return false on success, true on error.
  */
  bool init_gtid_sets(Gtid_set *gtid_set, Gtid_set *lost_groups,
                      Gtid *last_gtid, bool verify_checksum,
                      bool need_lock);

  void set_previous_gtid_set(Gtid_set *previous_gtid_set_param)
  {
    previous_gtid_set= previous_gtid_set_param;
  }
private:
  Gtid_set* previous_gtid_set;

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
  void signal_update();
  int wait_for_update_relay_log(THD* thd, const struct timespec * timeout);
  int  wait_for_update_bin_log(THD* thd, const struct timespec * timeout);
public:
  void init_pthread_objects();
  void cleanup();
  /**
    Create a new binary log.
    @param log_name Name of binlog
    @param new_name Name of binlog, too. todo: what's the difference
    between new_name and log_name?
    @param io_cache_type_arg Specifies how the IO cache is opened:
    read-only or read-write.
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
                   enum cache_type io_cache_type_arg,
                   ulong max_size,
                   bool null_created,
                   bool need_lock_index, bool need_sid_lock,
                   Format_description_log_event *extra_description_event);
  bool open_index_file(const char *index_file_name_arg,
                       const char *log_name, bool need_lock_index);
  /* Use this to start writing a new log file */
  int new_file(Format_description_log_event *extra_description_event);

  bool write_event(Log_event* event_info);
  bool write_cache(THD *thd, class binlog_cache_data *binlog_cache_data);
  int  do_write_cache(IO_CACHE *cache);

  void set_write_error(THD *thd, bool is_transactional);
  bool check_write_error(THD *thd);
  bool write_incident(THD *thd, bool need_lock_log,
                      bool do_flush_and_sync= true);
  bool write_incident(Incident_log_event *ev, bool need_lock_log,
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
  int rotate_and_purge(bool force_rotate);
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
  int add_log_to_index(uchar* log_file_name, int name_len,
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
  bool reset_logs(THD* thd);
  void close(uint exiting);

  // iterating through the log index file
  int find_log_pos(LOG_INFO* linfo, const char* log_name,
                   bool need_lock_index);
  int find_next_log(LOG_INFO* linfo, bool need_lock_index);
  int get_current_log(LOG_INFO* linfo);
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
};

typedef struct st_load_file_info
{
  THD* thd;
  my_off_t last_pos_in_file;
  bool wrote_create_file, log_delayed;
} LOAD_FILE_INFO;

extern MYSQL_PLUGIN_IMPORT MYSQL_BIN_LOG mysql_bin_log;

bool trans_has_updated_trans_table(const THD* thd);
bool stmt_has_updated_trans_table(const THD *thd);
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
int gtid_empty_group_log_and_cleanup(THD *thd);

extern const char *log_bin_index;
extern const char *log_bin_basename;
extern bool opt_binlog_order_commits;

/**
  Turns a relative log binary log path into a full path, based on the
  opt_bin_logname or opt_relay_logname.

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
    strmake(to, ptr, strlen(ptr));

end:
  DBUG_RETURN(error);
}
#endif /* BINLOG_H_INCLUDED */
