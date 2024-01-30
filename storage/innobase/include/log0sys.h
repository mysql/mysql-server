/*****************************************************************************

Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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
 @file include/log0sys.h

 Redo log - the log_sys.

 *******************************************************/

#ifndef log0sys_h
#define log0sys_h

#include "univ.i"

/* Log_consumer */
#include "log0consumer.h"

/* Log_files_capacity */
#include "log0files_capacity.h"

/* Log_files_dict */
#include "log0files_dict.h"

/* lsn_t, Log_clock, ... */
#include "log0types.h"

/* os_event_t */
#include "os0event.h"

/* OS_FILE_LOG_BLOCK_SIZE, os_offset_t */
#include "os0file.h"

/* rw_lock_t */
#include "sync0rw.h"

/* ut::INNODB_CACHE_LINE_SIZE */
#include "ut0cpu_cache.h"

/* Link_buf */
#include "ut0link_buf.h"

/* ib_mutex_t */
#include "ut0mutex.h"

/* aligned_array_pointer, ut::vector */
#include "ut0new.h"

class THD;

/** Redo log - single data structure with state of the redo log system.
In future, one could consider splitting this to multiple data structures. */
struct alignas(ut::INNODB_CACHE_LINE_SIZE) log_t {
#ifndef UNIV_HOTBACKUP

  /**************************************************/ /**

   @name Users writing to log buffer

   *******************************************************/

  /** @{ */

  /** Event used for locking sn */
  os_event_t sn_lock_event;

#ifdef UNIV_DEBUG
  /** The rw_lock instance only for the debug info list */
  /* NOTE: Just "rw_lock_t sn_lock_inst;" and direct minimum initialization
  seem to hit the bug of Sun Studio of Solaris. */
  rw_lock_t *sn_lock_inst;
#endif /* UNIV_DEBUG */

  /** Current sn value. Used to reserve space in the redo log,
  and used to acquire an exclusive access to the log buffer.
  Represents number of data bytes that have ever been reserved.
  Bytes of headers and footers of log blocks are not included.
  Its highest bit is used for locking the access to the log buffer. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) atomic_sn_t sn;

  /** Intended sn value while x-locked. */
  atomic_sn_t sn_locked;

  /** Mutex which can be used for x-lock sn value */
  mutable ib_mutex_t sn_x_lock_mutex;

  /** Aligned log buffer. Committing mini-transactions write there
  redo records, and the log_writer thread writes the log buffer to
  disk in background.
  Protected by: locking sn not to add. */
  alignas(ut::INNODB_CACHE_LINE_SIZE)
      ut::aligned_array_pointer<byte, LOG_BUFFER_ALIGNMENT> buf;

  /** Size of the log buffer expressed in number of data bytes,
  that is excluding bytes for headers and footers of log blocks. */
  atomic_sn_t buf_size_sn;

  /** Size of the log buffer expressed in number of total bytes,
  that is including bytes for headers and footers of log blocks. */
  size_t buf_size;

#ifdef UNIV_PFS_RWLOCK
  /** The instrumentation hook.
  @remarks This field is rarely modified, so can not be the cause of
  frequent cache line invalidations. However, user threads read it only during
  mtr.commit(), which in some scenarios happens rarely enough, that the cache
  line containing pfs_psi is evicted between mtr.commit()s causing a cache miss,
  a stall and in consequence MACHINE_CLEARS during mtr.commit(). As this miss
  seems inevitable, we at least want to make it really worth it. So, we put the
  pfs_psi in the same cache line which contains buf, buf_size_sn and buf_size,
  which are also needed during mtr.commit(). This way instead of two separate
  cache misses, we have just one.
  TBD: We could additionally use `lfence` to limit MACHINE_CLEARS.*/
  struct PSI_rwlock *pfs_psi;
#endif /* UNIV_PFS_RWLOCK */

  /** The recent written buffer.
  Protected by: locking sn not to add. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) Link_buf<lsn_t> recent_written;

  /** Used for pausing the log writer threads.
  When paused, each user thread should write log as in the former version. */
  std::atomic_bool writer_threads_paused;

  /** @} */

  /**************************************************/ /**

   @name Users <=> writer

   *******************************************************/

  /** @{ */

  /** Maximum sn up to which there is free space in both the log buffer
  and the log files. This is limitation for the end of any write to the
  log buffer. Threads, which are limited need to wait, and possibly they
  hold latches of dirty pages making a deadlock possible.
  Protected by: writer_mutex (writes). */
  alignas(ut::INNODB_CACHE_LINE_SIZE) atomic_sn_t buf_limit_sn;

  /** Up to this lsn, data has been written to disk (fsync not required).
  Protected by: writer_mutex (writes). */
  alignas(ut::INNODB_CACHE_LINE_SIZE) atomic_lsn_t write_lsn;

  /** Unaligned pointer to array with events, which are used for
  notifications sent from the log write notifier thread to user threads.
  The notifications are sent when write_lsn is advanced. User threads
  wait for write_lsn >= lsn, for some lsn. Log writer advances the
  write_lsn and notifies the log write notifier, which notifies all users
  interested in nearby lsn values (lsn belonging to the same log block).
  Note that false wake-ups are possible, in which case user threads
  simply retry waiting. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) os_event_t *write_events;

  /** Number of entries in the array with writer_events. */
  size_t write_events_size;

  /** Approx. number of requests to write/flush redo since startup. */
  alignas(ut::INNODB_CACHE_LINE_SIZE)
      std::atomic<uint64_t> write_to_file_requests_total;

  /** How often redo write/flush is requested in average.
  Measures in microseconds. Log threads do not spin when
  the write/flush requests are not frequent. */
  alignas(ut::INNODB_CACHE_LINE_SIZE)
      std::atomic<std::chrono::microseconds> write_to_file_requests_interval;
  static_assert(decltype(write_to_file_requests_interval)::is_always_lock_free);

  /** @} */

  /**************************************************/ /**

   @name Users <=> flusher

   *******************************************************/

  /** @{ */

  /** Unaligned pointer to array with events, which are used for
  notifications sent from the log flush notifier thread to user threads.
  The notifications are sent when flushed_to_disk_lsn is advanced.
  User threads wait for flushed_to_disk_lsn >= lsn, for some lsn.
  Log flusher advances the flushed_to_disk_lsn and notifies the
  log flush notifier, which notifies all users interested in nearby lsn
  values (lsn belonging to the same log block). Note that false
  wake-ups are possible, in which case user threads simply retry
  waiting. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) os_event_t *flush_events;

  /** Number of entries in the array with events. */
  size_t flush_events_size;

  /** This event is in the reset state when a flush is running;
  a thread should wait for this without owning any of redo mutexes,
  but NOTE that to reset this event, the thread MUST own the writer_mutex */
  os_event_t old_flush_event;

  /** Up to this lsn data has been flushed to disk (fsynced). */
  alignas(ut::INNODB_CACHE_LINE_SIZE) atomic_lsn_t flushed_to_disk_lsn;

  /** @} */

  /**************************************************/ /**

   @name Log flusher thread

   *******************************************************/

  /** @{ */

  /** Last flush start time. Updated just before fsync starts. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) Log_clock_point last_flush_start_time;

  /** Last flush end time. Updated just after fsync is finished.
  If smaller than start time, then flush operation is pending. */
  Log_clock_point last_flush_end_time;

  /** Flushing average time (in microseconds). */
  double flush_avg_time;

  /** Mutex which can be used to pause log flusher thread. */
  mutable ib_mutex_t flusher_mutex;

  alignas(ut::INNODB_CACHE_LINE_SIZE) os_event_t flusher_event;

  /** @} */

  /**************************************************/ /**

   @name Log writer thread

   *******************************************************/

  /** @{ */

  /** Size of buffer used for the write-ahead (in bytes). */
  alignas(ut::INNODB_CACHE_LINE_SIZE) uint32_t write_ahead_buf_size;

  /** Aligned buffer used for some of redo log writes. Data is copied
  there from the log buffer and written to disk, in following cases:
  - when writing ahead full kernel page to avoid read-on-write issue,
  - to copy, prepare and write the incomplete block of the log buffer
    (because mini-transactions might be writing new redo records to
    the block in parallel, when the block is being written to disk) */
  ut::aligned_array_pointer<byte, LOG_WRITE_AHEAD_BUFFER_ALIGNMENT>
      write_ahead_buf;

  /** Up to this file offset in the log files, the write-ahead
  has been done or is not required (for any other reason). */
  os_offset_t write_ahead_end_offset;

  /** File within which write_lsn is located, so the newest file in m_files
  in the same time - updates are protected by the m_files_mutex. This field
  exists, because the log_writer thread needs to locate offsets each time
  it writes data blocks to disk, but we do not want to acquire and release
  the m_files_mutex for each such write, because that would slow down the
  log_writer thread a lot. Instead of that, the log_writer uses this object
  to locate the offsets.

  Updates of this field require two mutexes: writer_mutex and m_files_mutex.
  Its m_id is updated only when the write_lsn moves to the next log file. */
  Log_file m_current_file{m_files_ctx, m_encryption_metadata};

  /** Handle for the opened m_current_file. The log_writer uses this handle
  to do writes (protected by writer_mutex). The log_flusher uses this handle
  to do fsyncs (protected by flusher_mutex). Both these threads might use
  this handle in parallel. The required synchronization between writes and
  fsyncs will happen on the OS side. When m_current_file is re-pointed to
  other file, this field is also updated, in the same critical section.
  Updates of this field are protected by: writer_mutex, m_files_mutex
  and flusher_mutex acquired all together. The reason for flusher_mutex
  is to avoid a need to acquire / release m_files_mutex in the log_flusher
  thread for each fsync. Instead of that, the log_flusher thread keeps the
  log_flusher_mutex, which is released less often, but still prevents from
  updates of this field. */
  Log_file_handle m_current_file_handle{m_encryption_metadata};

  /** True iff the log writer has entered extra writer margin and still
  hasn't exited since then. Each time the log_writer enters that margin,
  it pauses all user threads at log_free_check() calls and emits warning
  to the log. When the writer exits the extra margin, notice is emitted.
  Protected by: log_limits_mutex and writer_mutex. */
  bool m_writer_inside_extra_margin;

#endif /* !UNIV_HOTBACKUP */

  /** Number of performed IO operations (only for printing stats). */
  uint64_t n_log_ios;

#ifndef UNIV_HOTBACKUP

  /** Mutex which can be used to pause log writer thread. */
  mutable ib_mutex_t writer_mutex;

#ifdef UNIV_DEBUG
  /** THD used by the log_writer thread. */
  THD *m_writer_thd;
#endif /* UNIV_DEBUG */

  alignas(ut::INNODB_CACHE_LINE_SIZE) os_event_t writer_event;

  /** A recently seen value of log_consumer_get_oldest()->get_consumed_lsn().
  It serves as a lower bound for future values of this expression, because it is
  guaranteed to be monotonic in time: each individual consumer can only go
  forward, and new consumers must start at least from checkpoint lsn, and the
  checkpointer is always one of the consumers.
  Protected by: writer_mutex. */
  lsn_t m_oldest_need_lsn_lowerbound;

  /** @} */

  /**************************************************/ /**

   @name Log closing

   *******************************************************/

  /** @{ */

  /** Event used by threads to wait for recent_written.tail() to advance.
  Protected by: closer_mutex. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) os_event_t closer_event;

  /** Mutex protecting closer_event, current_ready_waiting_lsn, and
  current_ready_waiting_sig_count. */
  mutable ib_mutex_t closer_mutex;

  /** Some threads waiting for the ready for write lsn by closer_event.
  Protected by: closer_mutex. */
  lsn_t current_ready_waiting_lsn;

  /** current_ready_waiting_lsn is waited using this sig_count.
  Protected by: closer_mutex. */
  int64_t current_ready_waiting_sig_count;

  /** @} */

  /**************************************************/ /**

   @name Log flusher <=> flush_notifier

   *******************************************************/

  /** @{ */

  /** Event used by the log flusher thread to notify the log flush
  notifier thread, that it should proceed with notifying user threads
  waiting for the advanced flushed_to_disk_lsn (because it has been
  advanced). */
  alignas(ut::INNODB_CACHE_LINE_SIZE) os_event_t flush_notifier_event;

  /** The next flushed_to_disk_lsn can be waited using this sig_count. */
  int64_t current_flush_sig_count;

  /** Mutex which can be used to pause log flush notifier thread. */
  mutable ib_mutex_t flush_notifier_mutex;

  /** @} */

  /**************************************************/ /**

   @name Log writer <=> write_notifier

   *******************************************************/

  /** @{ */

  /** Mutex which can be used to pause log write notifier thread. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) mutable ib_mutex_t write_notifier_mutex;

  /** Event used by the log writer thread to notify the log write
  notifier thread, that it should proceed with notifying user threads
  waiting for the advanced write_lsn (because it has been advanced). */
  alignas(ut::INNODB_CACHE_LINE_SIZE) os_event_t write_notifier_event;

  /** @} */

  /**************************************************/ /**

   @name Log files management

   *******************************************************/

  /** @{ */

  /** Mutex protecting set of existing log files and their meta data. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) mutable ib_mutex_t m_files_mutex;

  /** Context for all operations on redo log files from log0files_io.h. */
  Log_files_context m_files_ctx;

  /** The in-memory dictionary of log files.
  Protected by: m_files_mutex. */
  Log_files_dict m_files{m_files_ctx};

  /** Number of existing unused files (those with _tmp suffix).
  Protected by: m_files_mutex. */
  size_t m_unused_files_count;

  /** Size of each unused redo log file, to which recently all unused
  redo log files became resized. Expressed in bytes. */
  os_offset_t m_unused_file_size;

  /** Capacity limits for the redo log. Responsible for resize.
  Mutex protection is decided per each Log_files_capacity method. */
  Log_files_capacity m_capacity;

  /** True iff log_writer is waiting for a next log file available.
  Protected by: m_files_mutex. */
  bool m_requested_files_consumption;

  /** Statistics related to redo log files consumption and creation.
  Protected by: m_files_mutex. */
  Log_files_stats m_files_stats;

  /** Event used by log files governor thread to wait. */
  os_event_t m_files_governor_event;

  /** Mutex which can be used to pause log governor thread. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) mutable ib_mutex_t
      governor_iteration_mutex;

  /** Event used by other threads to wait until log files governor finished
  its next iteration. This is useful when some sys_var gets changed to wait
  until log files governor re-computed everything and then check if the
  concurrency_margin is safe to emit warning if needed (the warning would
  still belong to the sys_var's SET GLOBAL statement then). */
  os_event_t m_files_governor_iteration_event;

  /** False if log files governor thread is allowed to add new redo records.
  This is set as intention, to tell the log files governor about what it is
  allowed to do. To ensure that the log_files_governor is aware of what has
  been told, user needs to wait on @see m_no_more_dummy_records_promised. */
  std::atomic_bool m_no_more_dummy_records_requested;

  /** False if the log files governor thread is allowed to add new dummy redo
  records. This is set to true only by the log_files_governor thread, and
  after it observed @see m_no_more_dummy_records_requested being true.
  It can be used to wait until the log files governor thread promises not to
  generate any more dummy redo records. */
  std::atomic_bool m_no_more_dummy_records_promised;

#ifdef UNIV_DEBUG
  /** THD used by the log_files_governor thread. */
  THD *m_files_governor_thd;
#endif /* UNIV_DEBUG */

  /** Event used for waiting on next file available. Used by log writer
  thread to wait when it needs to produce a next log file but there are
  no free (consumed) log files available. */
  os_event_t m_file_removed_event;

  /** Buffer that contains encryption meta data encrypted with master key.
  Protected by: m_files_mutex */
  byte m_encryption_buf[OS_FILE_LOG_BLOCK_SIZE];

#endif /* !UNIV_HOTBACKUP */

  /** Encryption metadata. This member is passed to Log_file_handle objects
  created for redo log files. In particular, the m_current_file_handle has
  a reference to this field. When encryption metadata is updated, it needs
  to be written to the redo log file's header. Also, each write performed
  by the log_writer thread needs to use m_encryption_metadata (it's passed
  by reference to the m_current_file_handle) and the log_writer does not
  acquire m_files_mutex for its writes (it is a hot path and it's better to
  keep it shorter). Therefore it's been decided that updates of this field
  require both m_files_mutex and writer_mutex.
  Protected by: m_files_mutex, writer_mutex */
  Encryption_metadata m_encryption_metadata;

#ifndef UNIV_HOTBACKUP

  /** @} */

  /**************************************************/ /**

   @name Consumers

   *******************************************************/

  /** @{ */

  /** Set of registered redo log consumers. Note, that this object
  is not responsible for freeing them (does not claim to be owner).
  If you wanted to register or unregister a redo log consumer, then
  please use following functions: @see log_consumer_register() and
  @see log_consumer_unregister(). The details of implementation
  related to redo log consumers can be found in log0consumer.cc.
  Protected by: m_files_mutex (unless it is the startup phase or
  the shutdown phase). */
  ut::unordered_set<Log_consumer *> m_consumers;

  /** @} */

  /**************************************************/ /**

   @name Maintenance

   *******************************************************/

  /** @{ */

  /** Used for stopping the log background threads. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) std::atomic_bool should_stop_threads;

  /** Event used for pausing the log writer threads. */
  os_event_t writer_threads_resume_event;

  /** Used for resuming write notifier thread */
  atomic_lsn_t write_notifier_resume_lsn;

  /** Used for resuming flush notifier thread */
  atomic_lsn_t flush_notifier_resume_lsn;

  /** Number of total I/O operations performed when we printed
  the statistics last time. */
  mutable uint64_t n_log_ios_old;

  /** Wall time when we printed the statistics last time. */
  mutable time_t last_printout_time;

  /** @} */

  /**************************************************/ /**

   @name Recovery

   *******************************************************/

  /** @{ */

  /** Lsn from which recovery has been started. */
  lsn_t recovered_lsn;

  /** Format of the redo log: e.g., Log_format::CURRENT. */
  Log_format m_format;

  /** Log creator name */
  std::string m_creator_name;

  /** Log flags */
  Log_flags m_log_flags;

  /** Log UUID */
  Log_uuid m_log_uuid;

  /** Used only in recovery: recovery scan succeeded up to this lsn. */
  lsn_t m_scanned_lsn;

#ifdef UNIV_DEBUG

  /** When this is set, writing to the redo log should be disabled.
  We check for this in functions that write to the redo log. */
  bool disable_redo_writes;

#endif /* UNIV_DEBUG */

  /** @} */

  /**************************************************/ /**

   @name Fields protected by the log_limits_mutex.
         Related to free space in the redo log.

   *******************************************************/

  /** @{ */

  /** Mutex which protects fields: available_for_checkpoint_lsn,
  requested_checkpoint_lsn. It also synchronizes updates of:
  free_check_limit_lsn, concurrency_margin, dict_persist_margin.
  It protects reads and writes of m_writer_inside_extra_margin.
  It also protects the srv_checkpoint_disabled (together with the
  checkpointer_mutex). */
  alignas(ut::INNODB_CACHE_LINE_SIZE) mutable ib_mutex_t limits_mutex;

  /** A new checkpoint could be written for this lsn value.
  Up to this lsn value, all dirty pages have been added to flush
  lists and flushed. Updated in the log checkpointer thread by
  taking minimum oldest_modification out of the last dirty pages
  from each flush list minus buf_flush_list_added->order_lag(). However
  it will not be bigger than the current value of
  buf_flush_list_added->smallest_not_added_lsn().
  Read by: user threads when requesting fuzzy checkpoint
  Read by: log_print() (printing status of redo)
  Updated by: log_checkpointer
  Protected by: limits_mutex. */
  lsn_t available_for_checkpoint_lsn;

  /** When this is larger than the latest checkpoint, the log checkpointer
  thread will be forced to write a new checkpoint (unless the new latest
  checkpoint lsn would still be smaller than this value).
  Read by: log_checkpointer
  Updated by: user threads (log_free_check() or for sharp checkpoint)
  Protected by: limits_mutex. */
  lsn_t requested_checkpoint_lsn;

  /** Maximum lsn allowed for checkpoint by dict_persist or zero.
  This will be set by dict_persist_to_dd_table_buffer(), which should
  be always called before really making a checkpoint.
  If non-zero, up to this lsn value, dynamic metadata changes have been
  written back to mysql.innodb_dynamic_metadata under dict_persist->mutex
  protection. All dynamic metadata changes after this lsn have to
  be kept in redo logs, but not discarded. If zero, just ignore it.
  Updated by: DD (when persisting dynamic meta data)
  Updated by: log_checkpointer (reset when checkpoint is written)
  Protected by: limits_mutex. */
  lsn_t dict_max_allowed_checkpoint_lsn;

  /** If should perform checkpoints every innodb_log_checkpoint_every ms.
  Disabled during startup / shutdown. Enabled in srv_start_threads.
  Updated by: starting thread (srv_start_threads)
  Read by: log_checkpointer */
  bool periodical_checkpoints_enabled;

  /** If checkpoints are allowed. When this is set to false, neither new
  checkpoints might be written nor lsn available for checkpoint might be
  updated. This is useful in recovery period, when neither flush lists can
  be trusted nor DD dynamic metadata redo records might be reclaimed.
  This is never set from true to false after log_start(). */
  std::atomic_bool m_allow_checkpoints;

  /** Maximum lsn up to which there is free space in the redo log.
  Threads check this limit and compare to current lsn, when they
  are outside mini-transactions and hold no latches. The formula used
  to compute the limitation takes into account maximum size of mtr and
  thread concurrency to include proper margins and avoid issues with
  race condition (in which all threads check the limitation and then
  all proceed with their mini-transactions). Also extra margin is
  there for dd table buffer cache (dict_persist_margin).
  Read by: user threads (log_free_check())
  Updated by: log_checkpointer (after update of checkpoint_lsn)
  Updated by: log_writer (after pausing/resuming user threads)
  Updated by: DD (after update of dict_persist_margin)
  Protected by (updates only): limits_mutex. */
  atomic_lsn_t free_check_limit_lsn;

  /** Margin used in calculation of @see free_check_limit_lsn.
  Protected by (updates only): limits_mutex. */
  atomic_sn_t concurrency_margin;

  /** True iff current concurrency_margin isn't truncated because of too small
  redo log capacity.
  Protected by (updates only): limits_mutex. */
  std::atomic<bool> concurrency_margin_is_safe;

  /** Margin used in calculation of @see free_check_limit_lsn.
  Read by: page_cleaners, log_checkpointer
  Updated by: DD
  Protected by (updates only): limits_mutex. */
  atomic_sn_t dict_persist_margin;

  /** @} */

  /**************************************************/ /**

   @name Log checkpointer thread

   *******************************************************/

  /** @{ */

  /** Event used by the log checkpointer thread to wait for requests. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) os_event_t checkpointer_event;

  /** Mutex which can be used to pause log checkpointer thread.
  This is used by log_position_lock() together with log_buffer_x_lock(),
  to pause any changes to current_lsn or last_checkpoint_lsn. */
  mutable ib_mutex_t checkpointer_mutex;

  /** Latest checkpoint lsn.
  Read by: user threads, log_print (no protection)
  Read by: log_writer (under writer_mutex)
  Updated by: log_checkpointer (under both mutexes)
  Protected by (updates only): checkpointer_mutex + writer_mutex. */
  atomic_lsn_t last_checkpoint_lsn;

  /** Next checkpoint header to use.
  Updated by: log_checkpointer
  Protected by: checkpointer_mutex */
  Log_checkpoint_header_no next_checkpoint_header_no;

  /** Event signaled when last_checkpoint_lsn is advanced by
  the log_checkpointer thread. */
  os_event_t next_checkpoint_event;

  /** Latest checkpoint wall time.
  Used by (private): log_checkpointer. */
  Log_clock_point last_checkpoint_time;

  /** Redo log consumer which is always registered and which is responsible
  for protecting redo log records at lsn >= last_checkpoint_lsn. */
  Log_checkpoint_consumer m_checkpoint_consumer{*this};

#ifdef UNIV_DEBUG
  /** THD used by the log_checkpointer thread. */
  THD *m_checkpointer_thd;
#endif /* UNIV_DEBUG */

  /** @} */

#endif /* !UNIV_HOTBACKUP */
};

/** Redo log system (singleton). */
extern log_t *log_sys;

#ifdef UNIV_PFS_MEMORY
/* PFS key for the redo log buffer's memory */
extern PSI_memory_key log_buffer_memory_key;
#endif /* UNIV_PFS_MEMORY */

#endif /* !log0sys_h */
