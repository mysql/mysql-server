/*****************************************************************************

Copyright (c) 1995, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************/ /**

 Redo log functions related to checkpointing and log free check.

 *******************************************************/

#ifndef log0chkp_h
#define log0chkp_h

#ifndef UNIV_HOTBACKUP

/* pages_persistence */
#include "fil0pages_persistence_interface.h"

/* log_get_sn */
#include "log0log.h"

/* log.last_checkpoint_lsn */
#include "log0sys.h"

/* log_t&, lsn_t */
#include "log0types.h"

/* srv_threads.*, srv_read_only_mode, ... */
#include "srv0srv.h"

/* sync_check_iterate */
#include "sync0debug.h"

/* sync_allowed_latches, latch_level_t */
#include "sync0types.h"

/* ut::aligned_delete ut::aligned_new_withkey */
#include "ut0new.h"

/** @name Log - checkpointer thread */
/** @{ */

/** Checks if log checkpointer thread is active.
@return true if and only if the log checkpointer thread is active */
inline bool log_checkpointer_is_active() {
  return srv_thread_is_active(srv_threads.m_log_checkpointer);
}

/** @} */
/** @name Log - checkpointer mutex */
/** @{ */

#define log_checkpointer_mutex_enter() \
  mutex_enter(&(log_checkpointing->checkpoint_mutex))

#define log_checkpointer_mutex_exit() \
  mutex_exit(&(log_checkpointing->checkpoint_mutex))

#define log_checkpointer_mutex_own()                    \
  (mutex_own(&(log_checkpointing->checkpoint_mutex)) || \
   !log_checkpointer_is_active())

/** @} */
/** @name Log - limits mutex */
/** @{ */

#define log_limits_mutex_enter() mutex_enter(&(log_checkpointing->limits_mutex))

#define log_limits_mutex_exit() mutex_exit(&(log_checkpointing->limits_mutex))

#define log_limits_mutex_own() mutex_own(&(log_checkpointing->limits_mutex))

/** @} */
/** @name Log - basic information about checkpoints. */
/** @{ */

/** Provides opposite checkpoint header number to the given checkpoint
header number.
@param[in]  checkpoint_header_no  the given checkpoint header number
@return the opposite checkpoint header number */
Log_checkpoint_header_no log_next_checkpoint_header(
    Log_checkpoint_header_no checkpoint_header_no);

/** @} */

class Log_checkpointing {
 private:
  /** @name Coordination with buffer pool and oldest_lsn */
  /** @{ */

  /** Updates lsn available for checkpoint. (Never decreases it)
  @return the updated value of m_available_for_checkpoint */
  lsn_t update_available_for_checkpoint_lsn();

 public:
  [[nodiscard]] lsn_t get_available_for_checkpoint_lsn() const;
  /** @} */
  /** @name Requests to make checkpoint */
  /** @{ */

  /** Requests a checkpoint written for lsn greater or equal to provided one.
  Caller must hold log_limits_mutex
  @param[in]      requested_lsn   checkpoint should be not older than this
  @return true iff checkpoints are enabled and request was made */
  bool request_checkpoint(lsn_t requested_lsn);

  /** Requests a fuzzy checkpoint write (for currently available lsn).
  @param[in]       sync  whether request is sync (function should wait) */
  void request_fuzzy_checkpoint(bool sync);

  /** Make a checkpoint at the current lsn. Reads current lsn and waits
  until all dirty pages have been flushed up to that lsn. Afterwards
  requests a checkpoint write and waits until it is finished. */
  void request_sharp_checkpoint();

 private:
  /** Returns highest requested checkpoint lsn */
  [[nodiscard]] lsn_t get_requested_checkpoint_lsn() {
    return m_requested_checkpoint_lsn.load();
  }

  /** @} */
  /** @name Periodical updates */
  /** @{ */

  [[nodiscard]] bool are_periodical_checkpoints_enabled() const;

  /** Calculates time that elapsed since last checkpoint.
  @return Time duration elapsed since the last checkpoint */
  [[nodiscard]] std::chrono::steady_clock::duration time_since_checkpoint();

 public:
  void enable_periodical_checkpoints();

  /** @} */

  /** Allocates and constructs the global log_checkpointing object */
  static void init();

  /** Destructs and deallocates the global log_checkpointing object. */
  static void deinit();

 private:
  Log_checkpointing();
  ~Log_checkpointing();
  template <typename T>
  friend void ut::aligned_delete(T *ptr) noexcept;

  template <typename T, typename... Args>
  friend T *ut::aligned_new_withkey(ut::PSI_memory_key_t key,
                                    std::size_t alignment, Args &&...args);

 public:
  /** @name Last known checkpoint lsn value */
  /** @{ */

  /** Updates the cached checkpoint_lsn */
  void set_checkpoint(lsn_t checkpoint_lsn) {
    m_checkpoint_lsn.store(checkpoint_lsn);
  }

  /** @return Cached checkpoint_lsn */
  [[nodiscard]] lsn_t get_checkpoint() { return m_checkpoint_lsn.load(); }

  /** Save the checkpoint lsn to the persistent store
  @param[in]  checkpoint_lsn  checkpoint lsn to be persisted
  @return true iff the lsn is persisted, false otherwise. */
  [[nodiscard]] bool save_checkpoint_value(lsn_t checkpoint_lsn);

  /** Fetch the checkpoint lsn from the persistent store
  @return DB_SUCCESS or error */
  [[nodiscard]] dberr_t load_checkpoint_value();

  /** @} */
  /** @name Making checkpoints */
  /** @{ */
 private:
  /** Figures out m_available_for_checkpoint_lsn*/
  [[nodiscard]] lsn_t determine_checkpoint_lsn();

  /** Considers requesting page cleaners to execute sync flush. */
  void consider_sync_flush();

  /** Checks if checkpoint should be written. Checks time elapsed since the last
  checkpoint, age of the last checkpoint and if there was any extra request to
  write the checkpoint (e.g. coming from
  log_checkpointing->request_sharp_checkpoint()).
  @return true if checkpoint should be written */
  bool should_checkpoint();

  /** Considers writing next checkpoint. Checks if checkpoint should be written
  (using should_checkpoint()) and writes the checkpoint if that's the case. */
  void consider_checkpoint();

  /** Makes a checkpoint. Note that this function does not flush dirty blocks
  from the buffer pool. It only checks what is lsn of the oldest modification
  in the buffer pool, and writes information about the lsn in log files.
  */
  void create_checkpoint();

  /** @} */
  /** @name The log_free_check() mechanism */
  /** @{ */

 public:
  /** Calls ib::redo::handler->do_not_need_smaller_than(get_checkpoint_lsn()).
  We prefer to have Log_checkpointing be the only place which calls
  do_not_need_smaller_than() so that it is easier to reason about, and to
  ensure that the argument passed is the checkpoint.
  We know that ib::redo::Handler::do_not_need_smaller_than() will recompute
  log_sys->m_free_check_limit_lsn used in wait_for_space(), based on things
  like checkpoint_lsn, log.m_capacity etc, so we call update_limits() whenever
  any of them changes. */
  void update_limits();

  /** @} */
  /** @name Dirty page flushing speed control */
  /** @{ */

  /** Retrieves limitations determined by the current state of log.m_capacity.
  These values are retrieved atomically (are consistent with each other).

  @param[out]  limit_for_free_check      soft capacity of the redo decreased by
                                         the current free check margin; this is
                                         limit for size of redo until which the
                                         log_free_check calls do not force waits
  @param[out]  limit_for_dirty_page_age  limit for the oldest dirty page until
                                         which the async (adaptive) flushing is
                                         not forced to be started (it might be
                                         started if turned on explicitly by the
                                         innodb_adaptive_flushing); note that
                                         computation of this value include doing
                                         the subtraction of the current log free
                                         check margin */
  void get_limits(lsn_t &limit_for_free_check, lsn_t &limit_for_dirty_page_age);

  /** Once checkpoint age exceeds this value, the flushing of pages starts to
  be adaptive. The adaptive page flushing is becoming more and more aggressive
  in the following range: adaptive_flush_min_age()..adaptive_flush_max_age().
  @note This value changes only during calls to @see update or @see initialize.
  @note Note that it must hold:
  adaptive_flush_min_age() < adaptive_flush_max_age() <=
  soft_logical_capacity().
  @remarks
  The diagram below shows how flushing / checkpointing becomes more aggressive
  when the age of the oldest modified page gets increased:

  adaptive_flush_min_age  adaptive_flush_max_age  aggressive_checkpoint_min_age
         |                        |                     |
  -------!------------------------!---------------------!----------------->age
  regular     adaptive flushing     aggressive flushing   aggr. checkpoints
  @return limitation to start adaptive flushing */
  [[nodiscard]] static lsn_t adaptive_flush_min_age();

  /** Once checkpoint age exceeds that value, the flushing of pages is the most
  aggressive possible since then. For more details @see adaptive_flush_min_age.
  @note This value changes only during calls to @see update or @see initialize.
  @return limitation to start furious flushing */
  [[nodiscard]] static lsn_t adaptive_flush_max_age();

  /** Once checkpoint age exceeds that value, the log checkpointer thread keeps
  writing checkpoints aggressively (whatever the progress of last_checkpoint_lsn
  would it make). Before that happens, checkpoints could be written periodically
  (for more details @see adaptive_flush_min_age).
  @note This value changes only during calls to @see update or @see initialize.
  @note It holds: adaptive_flush_max_age() < aggressive_checkpoint_min_age().
  @return limitation to start aggressive checkpointing */
  [[nodiscard]] static lsn_t aggressive_checkpoint_min_age();
  /** Computes lsn up to which sync flush of pages should be done or returns 0
  if there is no need to execute sync flush of dirty pages now.
  @return lsn for which we want to have oldest_lsn >= lsn in each BP,
          or 0 if there is no need for sync flush */
  [[nodiscard]] lsn_t get_sync_flush_lsn();

 private:
  /** @} */
  /** @name The checkpointer thread */
  /** @{ */

  /** The actual non-static body of the static log_checkpointer() */
  void do_work();

 public:
  /** Informs the log checkpointer that it should stop, but does not wait for it
  to happen. */
  void stop_thread_no_wait();

  /** Informs the log checkpointer that it should stop and waits for it to
  happen before returning to the caller */
  void stop_thread_and_wait();

  /** Starts the log checkpointer thread. */
  void start_thread();

  /** The log checkpointer thread routine. */
  static void log_checkpointer();
  /** @} */

 private:
  std::atomic_bool m_thread_should_stop{};

  /** Latest checkpoint lsn.
  Read by: user threads, log_print (no protection)
  Read by: log_writer (under writer_mutex)
  Updated by: log_checkpointer (under both mutexes)
  Protected by (updates only): checkpoint_mutex + writer_mutex. */
  atomic_lsn_t m_checkpoint_lsn{};

  /** Latest checkpoint wall time.
  Used by (private): log_checkpointer.
  Protected by: checkpoint_mutex (although, its used only from one thread) */
  Log_clock_point m_last_checkpoint_time{};

  /**

   @name Fields protected by the log_limits_mutex.
         Related to free space in the redo log.

   @{ */

  /** A new checkpoint could be written for this lsn value.
  Up to this lsn value, all dirty pages have been added to flush
  lists and flushed. Updated in the log checkpointer thread by
  taking minimum oldest_modification out of the last dirty pages
  from each flush list minus Buf_flush_list_added_lsns::order_lag().
  However, it will not be bigger than the current value of
  Buf_flush_list_added_lsns::smallest_not_added_lsn().
  Read by: user threads when requesting fuzzy checkpoint
  Read by: log_print() (printing status of redo)
  Updated by: log_checkpointer
  Protected by: limits_mutex. */
  lsn_t m_available_for_checkpoint_lsn{};

  /** When this is larger than the latest checkpoint, the log checkpointer
  thread will be forced to write a new checkpoint (unless the new latest
  checkpoint lsn would still be smaller than this value).
  Read by: log_checkpointer without and with limit_mutex
  Read by: buf_flush_page_coordinator_thread under limits_mutex
  Updated by: user threads (log_free_check() or for sharp checkpoint)
  Protected by: limits_mutex. */
  atomic_lsn_t m_requested_checkpoint_lsn{};

  /** @} */

  /** If should perform checkpoints every innodb_log_checkpoint_every ms, or
  when doing so would help reclaim oldest redo log file.
  Disabled during startup. Enabled in srv_start_threads.
  Updated by: starting thread (srv_start_threads)
  Read by: log_checkpointer */
  bool m_periodical_checkpoints_enabled{};

  /** THD used by the log_checkpointer thread. */
  THD *m_checkpointer_thd;

 public:
  /** Event used by the log checkpointer thread to wait for requests. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) os_event_t m_event;

  /** Event signaled by log checkpointer when it advances m_checkpoint_lsn. */
  os_event_t m_next_checkpoint_event;

  /** Mutex which can be used to pause log checkpointer thread.
  This is used by log_position_lock() together with log_buffer_x_lock(),
  to pause any changes to current_lsn or last_checkpoint_lsn. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) mutable ib_mutex_t checkpoint_mutex;

  /** Mutex which protects fields: m_available_for_checkpoint_lsn,
  m_requested_checkpoint_lsn. It also synchronizes updates of
  log_sys_t::m_free_check_limit_lsn. It protects reads and writes of
  log_sys_t::m_writer_inside_extra_margin. It also protects the
  srv_checkpoint_disabled (together with the checkpoint_mutex). */
  alignas(ut::INNODB_CACHE_LINE_SIZE) mutable ib_mutex_t limits_mutex;
};

extern Log_checkpointing *log_checkpointing;

/** Calculates age of current checkpoint as number of bytes since
last checkpoint. This includes bytes for headers and footers of
all log blocks. The calculation is based on the latest written
checkpoint lsn, and the current lsn, which points to the first
non reserved data byte. Note that the current lsn could not fit
the free space in the log files. This means that the checkpoint
age could potentially be larger than capacity of the log files.
However we do the best effort to avoid such situations, and if
they happen, user threads wait until the space is reclaimed.
@return checkpoint age as number of bytes */
[[nodiscard]] inline lsn_t log_get_checkpoint_age() {
  const lsn_t last_checkpoint_lsn = pages_persistence->get_checkpoint_lsn();

  const lsn_t current_lsn = ib::redo::handler->peek_first_unassigned_lsn();

  if (current_lsn <= last_checkpoint_lsn) {
    /* Writes or reads have been somehow reordered.
    Note that this function does not provide any lock,
    and does not assume any lock existing. Therefore
    the calculated result is already outdated when the
    function is finished. Hence, we might assume that
    this time we calculated age = 0, because checkpoint
    lsn is close to current lsn if such race happened. */
    return 0;
  }

  return current_lsn - last_checkpoint_lsn;
}

[[nodiscard]] inline lsn_t Log_checkpointing::get_available_for_checkpoint_lsn()
    const {
  ut_ad(log_limits_mutex_own());
  return m_available_for_checkpoint_lsn;
}

[[nodiscard]] inline bool
Log_checkpointing::are_periodical_checkpoints_enabled() const {
  ut_ad(log_limits_mutex_own());
  return m_periodical_checkpoints_enabled;
}

inline void Log_checkpointing::enable_periodical_checkpoints() {
  log_limits_mutex_enter();
  m_periodical_checkpoints_enabled = true;
  log_limits_mutex_exit();
}

/** @name Log - other functions related to checkpoints. */
/** @{ */

/** Writes checkpoint to the file containing the written checkpoint_lsn.
The checkpoint is written to the given checkpoint header. Unless InnoDB
is starting: checkpointer, writer and files mutexes must be acquired
before calling this function.
@param[in,out]  log                     redo log
@param[in]      checkpoint_file_handle  handle to opened file
@param[in]      checkpoint_header_no    checkpoint header to be written
@param[in]      next_checkpoint_lsn     the checkpoint lsn to write
@return DB_SUCCESS or error */
dberr_t log_files_write_checkpoint_low(
    log_t &log, Log_file_handle &checkpoint_file_handle,
    Log_checkpoint_header_no checkpoint_header_no, lsn_t next_checkpoint_lsn);

/** Writes the first data block to the log file using the provided handle
to the opened log file. The block is addressed by the given checkpoint_lsn,
filled with 0x00 and its data length points to checkpoint_lsn inside, making
the block logically empty.
@remarks This is used only during creation of new log files.
@param[in,out] log             redo log
@param[in]     file_handle     handle to the opened log file
@param[in]     checkpoint_lsn  the checkpoint lsn
@param[in]     file_start_lsn  start_lsn of the file
@return DB_SUCCESS or error */
dberr_t log_files_write_first_data_block_low(log_t &log,
                                             Log_file_handle &file_handle,
                                             lsn_t checkpoint_lsn,
                                             lsn_t file_start_lsn);

/** Writes the next checkpoint to the log file, by writing a single
checkpoint header with the checkpoint lsn. Flushes the file after the
write and updates the log.last_checkpoint_lsn.

@remarks Note that two checkpoint headers are used alternately for
consecutive checkpoints. If InnoDB crashed during the write, it would
still have the previous checkpoint info and recovery would work.
@param[in,out]  log    redo log
@param[in]      lsn    writes checkpoint at this lsn
@return DB_SUCCESS or error */
dberr_t log_files_next_checkpoint(log_t &log, lsn_t lsn);

/** @} */

/** Updates:
  - Innodb_redo_log_checkpoint_lsn,
  - Innodb_redo_log_current_lsn,
  - Innodb_redo_log_flushed_to_disk_lsn.
They should be updated "together", to present a consistent state to the user,
that is one in which checkpoint_lsn <= flushed_to_disk_lsn <= current_lsn.
Calls are protected by log_checkpointer->limits_mutex, which is to avoid torn
writes to these exposed fields, but otherwise does not enforce above
inequalities. Instead they follow from loading atomics in the order from the
conceptually smallest to the largest - a load synchronizes with the store, which
in turn should happen after the conceptually larger atomic already was at least
equal to the stored value, so can be only larger now.
*/
void log_update_exported_lsns();
#endif /* !UNIV_HOTBACKUP */

#endif /* !log0chkp_h */
