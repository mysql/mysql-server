/*****************************************************************************

Copyright (c) 2013, 2019, Oracle and/or its affiliates. All rights reserved.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************/ /**
 @file include/log0types.h

 Redo log types

 Created 2013-03-15 Sunny Bains
 *******************************************************/

#ifndef log0types_h
#define log0types_h

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

#include "os0event.h"
#include "os0file.h"
#include "sync0sharded_rw.h"
#include "univ.i"
#include "ut0link_buf.h"
#include "ut0mutex.h"

/** Type used for all log sequence number storage and arithmetics. */
typedef uint64_t lsn_t;

/** Print format for lsn_t values, used in functions like printf. */
#define LSN_PF UINT64PF

/** Alias for atomic based on lsn_t. */
using atomic_lsn_t = std::atomic<lsn_t>;

/** Type used for sn values, which enumerate bytes of data stored in the log.
Note that these values skip bytes of headers and footers of log blocks. */
typedef uint64_t sn_t;

/** Alias for atomic based on sn_t. */
using atomic_sn_t = std::atomic<sn_t>;

/** Type used for checkpoint numbers (consecutive checkpoints receive
a number which is increased by one). */
typedef uint64_t checkpoint_no_t;

/** Type used for counters in log_t: flushes_requested and flushes_expected.
They represent number of requests to flush the redo log to disk. */
typedef std::atomic<int64_t> log_flushes_t;

/** Function used to calculate checksums of log blocks. */
typedef std::atomic<uint32_t (*)(const byte *log_block)> log_checksum_func_t;

/** Clock used to measure time spent in redo log (e.g. when flushing). */
using Log_clock = std::chrono::high_resolution_clock;

/** Time point defined by the Log_clock. */
using Log_clock_point = std::chrono::time_point<Log_clock>;

/** Supported redo log formats. Stored in LOG_HEADER_FORMAT. */
enum log_header_format_t {
  /** The MySQL 5.7.9 redo log format identifier. We can support recovery
  from this format if the redo log is clean (logically empty). */
  LOG_HEADER_FORMAT_5_7_9 = 1,

  /** Remove MLOG_FILE_NAME and MLOG_CHECKPOINT, introduce MLOG_FILE_OPEN
  redo log record. */
  LOG_HEADER_FORMAT_8_0_1 = 2,

  /** Allow checkpoint_lsn to point any data byte within redo log (before
  it had to point the beginning of a group of log records). */
  LOG_HEADER_FORMAT_8_0_3 = 3,

  /** The redo log format identifier
  corresponding to the current format version. */
  LOG_HEADER_FORMAT_CURRENT = LOG_HEADER_FORMAT_8_0_3
};

/** The state of a log group */
enum class log_state_t {
  /** No corruption detected */
  OK,
  /** Corrupted */
  CORRUPTED
};

/** The recovery implementation. */
struct redo_recover_t;

typedef size_t log_lock_no_t;

struct Log_handle {
  log_lock_no_t lock_no;

  lsn_t start_lsn;

  lsn_t end_lsn;
};

/** Redo log - single data structure with state of the redo log system.
In future, one could consider splitting this to multiple data structures. */
struct alignas(INNOBASE_CACHE_LINE_SIZE) log_t {
  /**************************************************/ /**

   @name Users writing to log buffer

   *******************************************************/

  /** @{ */

#ifndef UNIV_HOTBACKUP
  /** Sharded rw-lock which can be used to freeze redo log lsn.
  When user thread reserves space in log, s-lock is acquired.
  Log archiver (Clone plugin) acquires x-lock. */
  mutable Sharded_rw_lock sn_lock;

  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** Current sn value. Used to reserve space in the redo log,
      and used to acquire an exclusive access to the log buffer.
      Represents number of data bytes that have ever been reserved.
      Bytes of headers and footers of log blocks are not included.
      Protected by: sn_lock.
      @see @ref subsect_redo_log_sn */
      atomic_sn_t sn;

  /** Padding after the _sn to avoid false sharing issues for
  constants below (due to changes of sn). */
  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** Pointer to the log buffer, aligned up to OS_FILE_LOG_BLOCK_SIZE.
      The alignment is to ensure that buffer parts specified for file IO write
      operations will be aligned to sector size, which is required e.g. on
      Windows when doing unbuffered file access.
      Protected by: sn_lock. */
      aligned_array_pointer<byte, OS_FILE_LOG_BLOCK_SIZE> buf;

  /** Size of the log buffer expressed in number of data bytes,
  that is excluding bytes for headers and footers of log blocks. */
  atomic_sn_t buf_size_sn;

  /** Size of the log buffer expressed in number of total bytes,
  that is including bytes for headers and footers of log blocks. */
  size_t buf_size;

  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** The recent written buffer.
      Protected by: sn_lock or writer_mutex. */
      Link_buf<lsn_t> recent_written;

  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** The recent closed buffer.
      Protected by: sn_lock or closer_mutex. */
      Link_buf<lsn_t> recent_closed;

  alignas(INNOBASE_CACHE_LINE_SIZE)

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
      atomic_sn_t buf_limit_sn;

  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** Up to this lsn, data has been written to disk (fsync not required).
      Protected by: writer_mutex (writes).
      @see @ref subsect_redo_log_write_lsn */
      atomic_lsn_t write_lsn;

  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** Unaligned pointer to array with events, which are used for
      notifications sent from the log write notifier thread to user threads.
      The notifications are sent when write_lsn is advanced. User threads
      wait for write_lsn >= lsn, for some lsn. Log writer advances the
      write_lsn and notifies the log write notifier, which notifies all users
      interested in nearby lsn values (lsn belonging to the same log block).
      Note that false wake-ups are possible, in which case user threads
      simply retry waiting. */
      os_event_t *write_events;

  /** Number of entries in the array with writer_events. */
  size_t write_events_size;

  /** Approx. number of requests to write/flush redo since startup. */
  alignas(INNOBASE_CACHE_LINE_SIZE)
      std::atomic<uint64_t> write_to_file_requests_total;

  /** How often redo write/flush is requested in average.
  Measures in microseconds. Log threads do not spin when
  the write/flush requests are not frequent. */
  alignas(INNOBASE_CACHE_LINE_SIZE)
      std::atomic<uint64_t> write_to_file_requests_interval;

  /** This padding is probably not needed, left for convenience. */
  alignas(INNOBASE_CACHE_LINE_SIZE)

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
      os_event_t *flush_events;

  /** Number of entries in the array with events. */
  size_t flush_events_size;

  /** Padding before the frequently updated flushed_to_disk_lsn. */
  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** Up to this lsn data has been flushed to disk (fsynced). */
      atomic_lsn_t flushed_to_disk_lsn;

  /** Padding after the frequently updated flushed_to_disk_lsn. */
  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** @} */

      /**************************************************/ /**

       @name Log flusher thread

       *******************************************************/

      /** @{ */

      /** Last flush start time. Updated just before fsync starts. */
      Log_clock_point last_flush_start_time;

  /** Last flush end time. Updated just after fsync is finished.
  If smaller than start time, then flush operation is pending. */
  Log_clock_point last_flush_end_time;

  /** Flushing average time (in microseconds). */
  double flush_avg_time;

  /** Mutex which can be used to pause log flusher thread. */
  mutable ib_mutex_t flusher_mutex;

  alignas(INNOBASE_CACHE_LINE_SIZE)

      os_event_t flusher_event;

  /** Padding to avoid any dependency between the log flusher
  and the log writer threads. */
  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** @} */

      /**************************************************/ /**

       @name Log writer thread

       *******************************************************/

      /** @{ */

      /** Space id for pages with log blocks. */
      space_id_t files_space_id;

  /** Size of buffer used for the write-ahead (in bytes). */
  uint32_t write_ahead_buf_size;

  /** Aligned pointer to buffer used for the write-ahead. It is aligned to
  system page size (why?) and is currently limited by constant 64KB. */
  aligned_array_pointer<byte, 64 * 1024> write_ahead_buf;

  /** Up to this file offset in the log files, the write-ahead
  has been done or is not required (for any other reason). */
  uint64_t write_ahead_end_offset;

  /** Aligned buffers for file headers. */
  aligned_array_pointer<byte, OS_FILE_LOG_BLOCK_SIZE> *file_header_bufs;
#endif /* !UNIV_HOTBACKUP */

  /** Some lsn value within the current log file. */
  lsn_t current_file_lsn;

  /** File offset for the current_file_lsn. */
  uint64_t current_file_real_offset;

  /** Up to this file offset we are within the same current log file. */
  uint64_t current_file_end_offset;

  /** Number of performed IO operations (only for printing stats). */
  uint64_t n_log_ios;

  /** Size of each single log file (expressed in bytes, including
  file header). */
  uint64_t file_size;

  /** Number of log files. */
  uint32_t n_files;

  /** Total capacity of all the log files (file_size * n_files),
  including headers of the log files. */
  uint64_t files_real_capacity;

  /** Capacity of redo log files for log writer thread. The log writer
  does not to exceed this value. If space is not reclaimed after 1 sec
  wait, it writes only as much as can fit the free space or crashes if
  there is no free space at all (checkpoint did not advance for 1 sec). */
  lsn_t lsn_capacity_for_writer;

  /** When this margin is being used, the log writer decides to increase
  the concurrency_margin to stop new incoming mini transactions earlier,
  on bigger margin. This is used to provide adaptive concurrency margin
  calculation, which we need because we might have unlimited thread
  concurrency setting or we could miss some log_free_check() calls.
  It is just best effort to help getting out of the troubles. */
  lsn_t extra_margin;

  /** True if we haven't increased the concurrency_margin since we entered
  (lsn_capacity_for_margin_inc..lsn_capacity_for_writer] range. This allows
  to increase the margin only once per issue and wait until the issue becomes
  resolved, still having an option to increase margin even more, if new issue
  comes later. */
  bool concurrency_margin_ok;

  /** Maximum allowed concurrency_margin. We never set higher, even when we
  increase the concurrency_margin in the adaptive solution. */
  lsn_t max_concurrency_margin;

#ifndef UNIV_HOTBACKUP
  /** Mutex which can be used to pause log writer thread. */
  mutable ib_mutex_t writer_mutex;

  alignas(INNOBASE_CACHE_LINE_SIZE)

      os_event_t writer_event;

  /** Padding after section for the log writer thread, to avoid any
  dependency between the log writer and the log closer threads. */
  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** @} */

      /**************************************************/ /**

       @name Log closer thread

       *******************************************************/

      /** @{ */

      /** Event used by the log closer thread to wait for tasks. */
      os_event_t closer_event;

  /** Mutex which can be used to pause log closer thread. */
  mutable ib_mutex_t closer_mutex;

  /** Padding after the log closer thread and before the memory used
  for communication between the log flusher and notifier threads. */
  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** @} */

      /**************************************************/ /**

       @name Log flusher <=> flush_notifier

       *******************************************************/

      /** @{ */

      /** Event used by the log flusher thread to notify the log flush
      notifier thread, that it should proceed with notifying user threads
      waiting for the advanced flushed_to_disk_lsn (because it has been
      advanced). */
      os_event_t flush_notifier_event;

  /** Mutex which can be used to pause log flush notifier thread. */
  mutable ib_mutex_t flush_notifier_mutex;

  /** Padding. */
  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** @} */

      /**************************************************/ /**

       @name Log writer <=> write_notifier

       *******************************************************/

      /** @{ */

      /** Mutex which can be used to pause log write notifier thread. */
      mutable ib_mutex_t write_notifier_mutex;

  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** Event used by the log writer thread to notify the log write
      notifier thread, that it should proceed with notifying user threads
      waiting for the advanced write_lsn (because it has been advanced). */
      os_event_t write_notifier_event;

  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** @} */

      /**************************************************/ /**

       @name Maintenance

       *******************************************************/

      /** @{ */

      /** Used for stopping the log background threads. */
      std::atomic_bool should_stop_threads;

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

  /** Format of the redo log: e.g., LOG_HEADER_FORMAT_CURRENT. */
  uint32_t format;

  /** Corruption status. */
  log_state_t state;

  /** Used only in recovery: recovery scan succeeded up to this lsn. */
  lsn_t scanned_lsn;

#ifdef UNIV_DEBUG

  /** When this is set, writing to the redo log should be disabled.
  We check for this in functions that write to the redo log. */
  bool disable_redo_writes;

  /** DEBUG only - if we copied or initialized the first block in buffer,
  this is set to lsn for which we did that. We later ensure that we start
  the redo log at the same lsn. Else it is zero and we would crash when
  trying to start redo then. */
  lsn_t first_block_is_correct_for_lsn;

#endif /* UNIV_DEBUG */

  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** @} */

      /**************************************************/ /**

       @name Fields protected by the log_limits mutex.
             Related to free space in the redo log.

       *******************************************************/

      /** @{ */

      /** Mutex which protects fields: available_for_checkpoint_lsn,
      requested_checkpoint_lsn. It also synchronizes updates of:
      free_check_limit_sn, concurrency_margin and dict_persist_margin.
      It also protects the srv_checkpoint_disabled (together with the
      checkpointer_mutex). */
      mutable ib_mutex_t limits_mutex;

  /** A new checkpoint could be written for this lsn value.
  Up to this lsn value, all dirty pages have been added to flush
  lists and flushed. Updated in the log checkpointer thread by
  taking minimum oldest_modification out of the last dirty pages
  from each flush list. However it will not be bigger than the
  current value of log.buf_dirty_pages_added_up_to_lsn.
  Read by: user threads when requesting fuzzy checkpoint
  Read by: log_print() (printing status of redo)
  Updated by: log_checkpointer
  Protected by: limits_mutex.
  @see @ref subsect_redo_log_available_for_checkpoint_lsn */
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

  /** Maximum sn up to which there is free space in the redo log.
  Threads check this limit and compare to current log.sn, when they
  are outside mini transactions and hold no latches. The formula used
  to compute the limitation takes into account maximum size of mtr and
  thread concurrency to include proper margins and avoid issues with
  race condition (in which all threads check the limitation and then
  all proceed with their mini transactions). Also extra margin is
  there for dd table buffer cache (dict_persist_margin).
  Read by: user threads (log_free_check())
  Updated by: log_checkpointer (after update of checkpoint_lsn)
  Updated by: log_writer (after increasing concurrency_margin)
  Updated by: DD (after update of dict_persist_margin)
  Protected by (updates only): limits_mutex. */
  atomic_sn_t free_check_limit_sn;

  /** Margin used in calculation of @see free_check_limit_sn.
  Read by: page_cleaners, log_checkpointer
  Updated by: log_writer
  Protected by (updates only): limits_mutex. */
  atomic_sn_t concurrency_margin;

  /** Margin used in calculation of @see free_check_limit_sn.
  Read by: page_cleaners, log_checkpointer
  Updated by: DD
  Protected by (updates only): limits_mutex. */
  atomic_sn_t dict_persist_margin;

  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** @} */

      /**************************************************/ /**

       @name Log checkpointer thread

       *******************************************************/

      /** @{ */

      /** Event used by the log checkpointer thread to wait for requests. */
      os_event_t checkpointer_event;

  /** Mutex which can be used to pause log checkpointer thread.
  This is used by log_position_lock() together with log_buffer_x_lock(),
  to pause any changes to current_lsn or last_checkpoint_lsn. */
  mutable ib_mutex_t checkpointer_mutex;

  /** Latest checkpoint lsn.
  Read by: user threads, log_print (no protection)
  Read by: log_writer (under writer_mutex)
  Updated by: log_checkpointer (under both mutexes)
  Protected by (updates only): checkpointer_mutex + writer_mutex.
  @see @ref subsect_redo_log_last_checkpoint_lsn */
  atomic_lsn_t last_checkpoint_lsn;

  /** Next checkpoint number.
  Read by: log_get_last_block (no protection)
  Read by: log_writer (under writer_mutex)
  Updated by: log_checkpointer (under both mutexes)
  Protected by: checkpoint_mutex + writer_mutex. */
  std::atomic<checkpoint_no_t> next_checkpoint_no;

  /** Latest checkpoint wall time.
  Used by (private): log_checkpointer. */
  Log_clock_point last_checkpoint_time;

  /** Aligned buffer used for writing a checkpoint header. It is aligned
  similarly to log.buf.
  Used by (private): log_checkpointer, recovery code */
  aligned_array_pointer<byte, OS_FILE_LOG_BLOCK_SIZE> checkpoint_buf;

  /** @} */

  /**************************************************/ /**

   @name Fields considered constant, updated when log system
         is initialized (log_sys_init()) and not assigned to
         particular log thread.

   *******************************************************/

  /** @{ */

  /** Capacity of the log files available for log_free_check(). */
  lsn_t lsn_capacity_for_free_check;

  /** Capacity of log files excluding headers of the log files.
  If the checkpoint age exceeds this, it is a serious error,
  because in such case we have already overwritten redo log. */
  lsn_t lsn_real_capacity;

  /** When the oldest dirty page age exceeds this value, we start
  an asynchronous preflush of dirty pages. */
  lsn_t max_modified_age_async;

  /** When the oldest dirty page age exceeds this value, we start
  a synchronous flush of dirty pages. */
  lsn_t max_modified_age_sync;

  /** When checkpoint age exceeds this value, we write checkpoints
  if lag between oldest_lsn and checkpoint_lsn exceeds max_checkpoint_lag. */
  lsn_t max_checkpoint_age_async;

  /** @} */
#endif /* !UNIV_HOTBACKUP */
};

#endif /* !log0types_h */
