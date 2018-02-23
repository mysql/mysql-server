/*****************************************************************************

Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

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
  Sharded_rw_lock sn_lock;

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
      atomic_sn_t sn_limit_for_end;

  /** Maximum sn up to which there is free space in both the log buffer
  and the log files for any possible mtr. This is limitation for the
  beginning of any write to the log buffer. Threads check this limitation
  when they are outside mini transactions and hold no latches. The formula
  used to calculate the limitation takes into account maximum size of mtr
  and thread concurrency to include proper margins and avoid issue with
  race condition (in which all threads check the limitation and then all
  proceed with their mini transactions).
  Protected by: writer_mutex (writes). */
  atomic_sn_t sn_limit_for_start;

  /** Margin used in calculation of @see sn_limit_for_start.
  Protected by: writer_mutex. */
  sn_t concurrency_safe_free_margin;

  atomic_sn_t dict_persist_margin;

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
  ib_mutex_t flusher_mutex;

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

  /** Total capacity of all the log files (file_size * n_files),
  including headers of the log files. */
  uint64_t files_real_capacity;

  /** Mutex which can be used to pause log writer thread. */
  ib_mutex_t writer_mutex;

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

      /** Mutex which can be used to pause log closer thread. */
      ib_mutex_t closer_mutex;

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
  ib_mutex_t flush_notifier_mutex;

  /** Padding. */
  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** @} */

      /**************************************************/ /**

       @name Log writer <=> write_notifier

       *******************************************************/

      /** @{ */

      /** Mutex which can be used to pause log write notifier thread. */
      ib_mutex_t write_notifier_mutex;

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

  /** True iff the log closer thread is alive. */
  std::atomic_bool closer_thread_alive;

  /** True iff the log checkpointer thread is alive. */
  std::atomic_bool checkpointer_thread_alive;

  /** True iff the log writer thread is alive. */
  std::atomic_bool writer_thread_alive;

  /** True iff the log flusher thread is alive. */
  std::atomic_bool flusher_thread_alive;

  /** True iff the log write notifier thread is alive. */
  std::atomic_bool write_notifier_thread_alive;

  /** True iff the log flush notifier thread is alive. */
  std::atomic_bool flush_notifier_thread_alive;

  /** Size of the log buffer expressed in number of data bytes,
  that is excluding bytes for headers and footers of log blocks. */
  atomic_sn_t buf_size_sn;

  /** Size of the log buffer expressed in number of total bytes,
  that is including bytes for headers and footers of log blocks. */
  size_t buf_size;

  /** Capacity of the log files in total, expressed in number of
  data bytes, that is excluding bytes for headers and footers of
  log blocks. */
  lsn_t sn_capacity;

  /** Lsn from which recovery has been started. */
  lsn_t recovered_lsn;

  /** Number of log files. */
  uint32_t n_files;

  /** Format of the redo log: e.g., LOG_HEADER_FORMAT_CURRENT. */
  uint32_t format;

  /** Corruption status. */
  log_state_t state;

  /** Aligned buffers for file headers. */
  aligned_array_pointer<byte, OS_FILE_LOG_BLOCK_SIZE> *file_header_bufs;

  /** Used only in recovery: recovery scan succeeded up to this lsn. */
  lsn_t scanned_lsn;

  /** Number of total I/O operations performed when we printed
  the statistics last time. */
  mutable uint64_t n_log_ios_old;

  /** Wall time when we printed the statistics last time. */
  mutable time_t last_printout_time;

  //#ifdef UNIV_DEBUG
  /** When this is set, writing to the redo log should be disabled.
  We check for this in functions that write to the redo log. */
  bool disable_redo_writes;
  //#endif /* UNIV_DEBUG */

  /** Padding before memory used for checkpoints logic. */
  alignas(INNOBASE_CACHE_LINE_SIZE)

      /** @} */

      /**************************************************/ /**

       @name Fields involved in checkpoints

       *******************************************************/

      /** @{ */

      /** Event used by the log checkpointer thread to wait for requests. */
      os_event_t checkpointer_event;

  /** Mutex which can be used to pause log checkpointer thread. */
  ib_mutex_t checkpointer_mutex;

  /** Capacity of log files excluding headers of the log files.
  If the checkpoint age exceeds this, it is a serious error,
  because it is possible we will then overwrite log and spoil
  crash recovery. */
  lsn_t lsn_capacity;

  /** When the oldest dirty page age exceeds this value, we start
  an asynchronous preflush of dirty pages. */
  lsn_t max_modified_age_async;

  /** When the oldest dirty page age exceeds this value, we start
  a synchronous flush of dirty pages. */
  lsn_t max_modified_age_sync;

  /** When checkpoint age exceeds this value, we force writing next
  checkpoint (requesting the log checkpointer thread to do it). */
  lsn_t max_checkpoint_age_async;

  /** When checkpoint age exceeds this value, user thread needs
  to wait. The check is performed when a new query step is started. */
  lsn_t max_checkpoint_age;

  /** If should perform checkpoints every innodb_log_checkpoint_every ms.
  Disabled during startup / shutdown. */
  bool periodical_checkpoints_enabled;

  /** A new checkpoint could be written for this lsn value.
  Up to this lsn value, all dirty pages have been added to flush
  lists and flushed. Updated in the log checkpointer thread by
  taking minimum oldest_modification out of the last dirty pages
  from each flush list. However it will not be bigger than the
  current value of log.buf_dirty_pages_added_up_to_lsn.
  Protected by: checkpointer_mutex.
  @see @ref subsect_redo_log_available_for_checkpoint_lsn */
  lsn_t available_for_checkpoint_lsn;

  /** A new checkpoint lsn suggested by dict_persist.
  This will be set by dict_persist_to_dd_table_buffer(), which should
  be always called before really making a checkpoint.
  If non-zero, up to this lsn value, dynamic metadata changes have been
  written back to mysql.innodb_dynamic_metadata under dict_persist->mutex
  protection. All dynamic metadata changes after this lsn have to
  be kept in redo logs, but not discarded. If zero, just ignore it. */
  lsn_t dict_suggest_checkpoint_lsn;

  /** When this is larger than the latest checkpoint, the log checkpointer
  thread will be forced to write a new checkpoint (unless the new latest
  checkpoint lsn would still be smaller than this value).
  Protected by: checkpointer_mutex. */
  lsn_t requested_checkpoint_lsn;

  /** Latest checkpoint wall time.
  Protected by: checkpointer_mutex. */
  Log_clock_point last_checkpoint_time;

  /** Latest checkpoint lsn.
  Protected by: checkpointer_mutex (writes).
  @see @ref subsect_redo_log_last_checkpoint_lsn */
  atomic_lsn_t last_checkpoint_lsn;

  /** Next checkpoint number.
  Protected by: checkpoint_mutex. */
  std::atomic<checkpoint_no_t> next_checkpoint_no;

  /** Aligned buffer used for writing a checkpoint header. It is aligned
  similarly to log.buf.
  Protected by: checkpointer_mutex. */
  aligned_array_pointer<byte, OS_FILE_LOG_BLOCK_SIZE> checkpoint_buf;

#endif /* !UNIV_HOTBACKUP */
       /** @} */
};

#endif /* !log0types_h */
