/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2009, Google Inc.

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
 @file log/log0log.cc

 Redo log system - provides durability for unflushed modifications
 to contents of data pages.

 This file covers general maintenance, including:
 -# Allocation and deallocation of the redo log data structures.
 -# Initialization and shutdown of the redo log.
 -# Start / stop for the log background threads.
 -# Runtime updates of server variables.
 -# Extending size of the redo log buffers.
 -# Locking redo position (for replication).

 Code responsible for writing to redo log could be found in log0buf.cc,
 log0write.cc, and log0log.ic. The log writer, flusher, write notifier,
 flush notifier, and closer threads are implemented in log0write.cc.

 Code responsible for checkpoints could be found in log0chkp.cc.
 The log checkpointer thread is implemented there.

 Created 12/9/1995 Heikki Tuuri
 *******************************************************/

#include "log0types.h"

/** Pointer to the log checksum calculation function. */
log_checksum_func_t log_checksum_algorithm_ptr;

#ifndef UNIV_HOTBACKUP

#include <debug_sync.h>
#include <sys/types.h>
#include <time.h>
#include "dict0boot.h"
#include "ha_prototypes.h"
#include "os0thread-create.h"
#include "trx0sys.h"

/**
@page PAGE_INNODB_REDO_LOG Innodb redo log

@section sect_redo_log_general General idea of redo log

The redo log is a write ahead log of changes applied to contents of data pages.
It provides durability for all changes applied to the pages. In case of crash,
it is used to recover modifications to pages that were modified but have not
been flushed to disk.

@note In case of clean shutdown, the redo log should be logically empty.
This means that after the checkpoint lsn there should be no records to apply.
However the log files still could contain some old data (which is not used
during the recovery process).

Every change to content of a data page must be done through a mini transaction
(so called mtr - mtr_t), which in mtr_commit() writes all its log records
to the redo log.

@remarks Normally these changes are performed using the mlog_write_ulint()
or similar function. In some page-level operations, only a code number of
a c-function and its parameters are written to the redo log, to reduce the
size of the redo log. You should not add parameters to such functions
(e.g. trx_undo_header_create(), trx_undo_insert_header_reuse()).
You should not add functionality which can either change when compared to older
versions, or which is dependent on data outside of the page being modified.
Therefore all functions must implement self-contained page transformation
and it should be unchanged if you don't have very essential reasons to change
the log semantics or format.

Single mtr can cover changes to multiple pages. In case of crash, either the
whole set of changes from a given mtr is recovered or none of the changes.

During life time of a mtr, a log of changes is collected inside an internal
buffer of the mtr. It contains multiple log records, which describe changes
applied to possibly different modified pages. When the mtr is committed, all
the log records are written to the log buffer within a single group of the log
records. Procedure:

-# Total number of data bytes of log records is calculated.
-# Space for the log records is reserved. Range of lsn values is assigned for
   a group of log records.
-# %Log records are written to the reserved space in the log buffer.
-# Modified pages are marked as dirty and moved to flush lists.
   All the dirty pages are marked with the same range of lsn values.
-# Reserved space is closed.

Background threads are responsible for writing of new changes in the log buffer
to the log files. User threads that require durability for the logged records,
have to wait until the log gets flushed up to the required point.

During recovery only complete groups of log records are recovered and applied.
Example given, if we had rotation in a tree, which resulted in changes to three
nodes (pages), we have a guarantee, that either the whole rotation is recovered
or nothing, so we will not end up with a tree that has incorrect structure.

Consecutive bytes written to the redo log are enumerated by the lsn values.
Every single byte written to the log buffer corresponds to current lsn
increased by one.

Data in the redo log is structured in consecutive blocks of 512 bytes
(_OS_FILE_LOG_BLOCK_SIZE_). Each block contains a header of 12 bytes
(_LOG_BLOCK_HDR_SIZE_) and a footer of 4 bytes (_LOG_BLOCK_TRL_SIZE_).
These extra bytes are also enumerated by lsn values. Whenever we refer to
data bytes, we mean actual bytes of log records - not bytes of headers and
footers of log blocks. The sequence of enumerated data bytes, is called the
sn values. All headers and footers of log blocks are added within the log
buffer, where data is actually stored in proper redo format.

When a user transaction commits, extra mtr is committed (related to undo log),
and then user thread waits until the redo log is flushed up to the point,
where log records of that mtr end.

When a dirty page is being flushed, a thread doing the flush, first needs to
wait until the redo log gets flushed up to the newest modification of the page.
Afterwards the page might be flushed. In case of crash, we might end up with
the newest version of the page and without any earlier versions of the page.
Then other pages, which potentially have not been flushed before the crash,
need to be recovered to that version. This applies to:
        * pages modified within the same group of log records,
        * and pages modified within any earlier group of log records.

@section sect_redo_log_architecture Architecture of redo log

@subsection subsect_redo_log_data_layers Data layers

Redo log consists of following data layers:

-# %Log files (typically 4 - 32 GB) - physical redo files that reside on
   the disk.

-# %Log buffer (64 MB by default) - groups data to write to log files,
   formats data in proper way: include headers/footers of log blocks,
   calculates checksums, maintains boundaries of record groups.

-# %Log recent written buffer (e.g. 4MB) - tracks recent writes to the
   log buffer. Allows to have concurrent writes to the log buffer and tracks
   up to which lsn all such writes have been already finished.

-# %Log recent closed buffer (e.g. 4MB) - tracks for which recent writes,
   corresponding dirty pages have been already added to the flush lists.
   Allows to relax order in which dirty pages have to be added to the flush
   lists and tracks up to which lsn, all dirty pages have been added.
   This is required to not make checkpoint at lsn which is larger than
   oldest_modification of some dirty page, which still has not been added
   to the flush list (because user thread was scheduled out).

-# %Log write ahead buffer (e.g. 4kB) - used to write ahead more bytes
   to the redo files, to avoid read-on-write problem. This buffer is also
   used when we need to write an incomplete log block, which might
   concurrently be receiving even more data from next user threads. In such
   case we first copy the incomplete block to the write ahead buffer.

@subsection subsect_redo_log_general_rules General rules

-# User threads write their redo data only to the log buffer.

-# User threads write concurrently to the log buffer, without synchronization
   between each other.

-# The log recent written buffer is maintained to track concurrent writes.

-# Background log threads write and flush the log buffer to disk.

-# User threads do not touch log files. Background log threads are the only
   allowed to touch the log files.

-# User threads wait for the background threads when they need flushed redo.

-# %Events per log block are exposed by redo log for users interested in waiting
   for the flushed redo.

-# Users can see up to which point log has been written / flushed.

-# User threads need to wait if there is no space in the log buffer.

   @diafile storage/innobase/log/arch_writing.dia "Writing to the redo log"

-# User threads add dirty pages to flush lists in the relaxed order.

-# Order in which user threads reserve ranges of lsn values, order in which
   they write to the log buffer, and order in which they add dirty pages to
   flush lists, could all be three completely different orders.

-# User threads do not write checkpoints (are not allowed to touch log files).

-# Checkpoint is automatically written from time to time by a background thread.

-# User threads can request a forced write of checkpoint and wait.

-# User threads need to wait if there is no space in the log files.

   @diafile storage/innobase/log/arch_deleting.dia "Reclaiming space in the redo
log"

-# Well thought out and tested set of _MONITOR_ counters is maintained and
   documented.

-# All settings are configurable through server variables, but the new server
   variables are hidden unless a special _EXPERIMENTAL_ mode has been defined
   when running cmake.

-# All the new buffers could be resized dynamically during runtime. In practice,
   only size of the log buffer is accessible without the _EXPERIMENTAL_ mode.

   @note
   This is a functional change - the log buffer could be resized dynamically
   by users (also decreased).

@section sect_redo_log_lsn_values Glossary of lsn values

Different fragments of head of the redo log are tracked by different values:
  - @ref subsect_redo_log_write_lsn,
  - @ref subsect_redo_log_buf_ready_for_write_lsn,
  - @ref subsect_redo_log_sn.

Different fragments of the redo log's tail are tracked by different values:
  - @ref subsect_redo_log_buf_dirty_pages_added_up_to_lsn,
  - @ref subsect_redo_log_available_for_checkpoint_lsn,
  - @ref subsect_redo_log_last_checkpoint_lsn.

@subsection subsect_redo_log_write_lsn log.write_lsn

Up to this lsn we have written all data to log files. It's the beginning of
the unwritten log buffer. Older bytes in the buffer are not required and might
be overwritten in cyclic manner for lsn values larger by _log.buf_size_.

Value is updated by: [log writer thread](@ref sect_redo_log_writer).

@subsection subsect_redo_log_buf_ready_for_write_lsn log.buf_ready_for_write_lsn

Up to this lsn, all concurrent writes to log buffer have been finished.
We don't need older part of the log recent-written buffer.

It obviously holds:

        log.buf_ready_for_write_lsn >= log.write_lsn

Value is updated by: [log writer thread](@ref sect_redo_log_writer).

@subsection subsect_redo_log_flushed_to_disk_lsn log.flushed_to_disk_lsn

Up to this lsn, we have written and flushed data to log files.

It obviously holds:

        log.flushed_to_disk_lsn <= log.write_lsn

Value is updated by: [log flusher thread](@ref sect_redo_log_flusher).

@subsection subsect_redo_log_sn log.sn

Corresponds to current lsn. Maximum assigned sn value (enumerates only
data bytes).

It obviously holds:

        log.sn >= log_translate_lsn_to_sn(log.buf_ready_for_write_lsn)

Value is updated by: user threads during reservation of space.

@subsection subsect_redo_log_buf_dirty_pages_added_up_to_lsn
log.buf_dirty_pages_added_up_to_lsn

Up to this lsn user threads have added all dirty pages to flush lists.

The redo log records are allowed to be deleted not further than up to this lsn.
That's because there could be a page with _oldest_modification_ smaller than
the minimum _oldest_modification_ available in flush lists. Note that such page
is just about to be added to flush list by a user thread, but there is no mutex
protecting access to the minimum _oldest_modification_, which would be acquired
by the user thread before writing to redo log. Hence for any lsn greater than
_buf_dirty_pages_added_up_to_lsn_ we cannot trust that flush lists are complete
and minimum calculated value (or its approximation) is valid.

@note
Note that we do not delete redo log records physically, but we still can delete
them logically by doing checkpoint at given lsn.

It holds (unless the log writer thread misses an update of the
@ref subsect_redo_log_buf_ready_for_write_lsn):

        log.buf_dirty_pages_added_up_to_lsn <= log.buf_ready_for_write_lsn.

Value is updated by: [log closer thread](@ref sect_redo_log_closer).

@subsection subsect_redo_log_available_for_checkpoint_lsn
log.available_for_checkpoint_lsn

Up to this lsn all dirty pages have been flushed to disk. However, this value
is not guaranteed to be the maximum such value. As insertion order to flush
lists is relaxed, the buf_pool_get_oldest_modification_approx() returns
modification time of some page that was inserted the earliest, it doesn't
have to be the oldest modification though. However, the maximum difference
between the first page in flush list, and one with the oldest modification
lsn is limited by the number of entries in the log recent closed buffer.

That's why from result of buf_pool_get_oldest_modification_approx() size of
the log recent closed buffer is subtracted. The result is used to update the
lsn available for a next checkpoint.

This has impact on the redo format, because the checkpoint_lsn can now point
to the middle of some group of log records (even to the middle of a single
log record). Log files with such checkpoint are not recoverable by older
versions of InnoDB by default.

Value is updated by:
[log checkpointer thread](@ref sect_redo_log_checkpointer).

@see @ref sect_redo_log_add_dirty_pages

@subsection subsect_redo_log_last_checkpoint_lsn log.last_checkpoint_lsn

Up to this lsn all dirty pages have been flushed to disk and the lsn value
has been flushed to header of the first log file (_ib_logfile0_).

The lsn value points to place where recovery is supposed to start. Data bytes
for smaller lsn values are not required and might be overwritten (log files
are circular). One could consider them logically deleted.

Value is updated by:
[log checkpointer thread](@ref sect_redo_log_checkpointer).

It holds:

        log.last_checkpoint_lsn
        <= log.available_for_checkpoint_lsn
        <= log.buf_dirty_pages_added_up_to_lsn.


Read more about redo log details:
- @subpage PAGE_INNODB_REDO_LOG_BUF
- @subpage PAGE_INNODB_REDO_LOG_THREADS
- @subpage PAGE_INNODB_REDO_LOG_FORMAT

*******************************************************/

/** Redo log system. Singleton used to populate global pointer. */
aligned_pointer<log_t> *log_sys_object;

/** Redo log system (singleton). */
log_t *log_sys;

#ifdef UNIV_PFS_THREAD

/** PFS key for the log writer thread. */
mysql_pfs_key_t log_writer_thread_key;

/** PFS key for the log closer thread. */
mysql_pfs_key_t log_closer_thread_key;

/** PFS key for the log checkpointer thread. */
mysql_pfs_key_t log_checkpointer_thread_key;

/** PFS key for the log flusher thread. */
mysql_pfs_key_t log_flusher_thread_key;

/** PFS key for the log flush notifier thread. */
mysql_pfs_key_t log_flush_notifier_thread_key;

/** PFS key for the log write notifier thread. */
mysql_pfs_key_t log_write_notifier_thread_key;

#endif /* UNIV_PFS_THREAD */

/** Calculates proper size for the log buffer and allocates the log buffer.
@param[out]	log	redo log */
static void log_allocate_buffer(log_t &log);

/** Deallocates the log buffer.
@param[out]	log	redo log */
static void log_deallocate_buffer(log_t &log);

/** Allocates the log write-ahead buffer (aligned to system page for
easier migrations between NUMA nodes).
@param[out]	log	redo log */
static void log_allocate_write_ahead_buffer(log_t &log);

/** Deallocates the log write-ahead buffer.
@param[out]	log	redo log */
static void log_deallocate_write_ahead_buffer(log_t &log);

/** Allocates the log checkpoint buffer (used to write checkpoint headers).
@param[out]	log	redo log */
static void log_allocate_checkpoint_buffer(log_t &log);

/** Deallocates the log checkpoint buffer.
@param[out]	log	redo log */
static void log_deallocate_checkpoint_buffer(log_t &log);

/** Allocates the array with flush events.
@param[out]	log	redo log */
static void log_allocate_flush_events(log_t &log);

/** Deallocates the array with flush events.
@param[out]	log	redo log */
static void log_deallocate_flush_events(log_t &log);

/** Deallocates the array with write events.
@param[out]	log	redo log */
static void log_deallocate_write_events(log_t &log);

/** Allocates the array with write events.
@param[out]	log	redo log */
static void log_allocate_write_events(log_t &log);

/** Allocates the log recent written buffer.
@param[out]	log	redo log */
static void log_allocate_recent_written(log_t &log);

/** Deallocates the log recent written buffer.
@param[out]	log	redo log */
static void log_deallocate_recent_written(log_t &log);

/** Allocates the log recent closed buffer.
@param[out]	log	redo log */
static void log_allocate_recent_closed(log_t &log);

/** Deallocates the log recent closed buffer.
@param[out]	log	redo log */
static void log_deallocate_recent_closed(log_t &log);

/** Allocates buffers for headers of the log files.
@param[out]	log	redo log */
static void log_allocate_file_header_buffers(log_t &log);

/** Deallocates buffers for headers of the log files.
@param[out]	log	redo log */
static void log_deallocate_file_header_buffers(log_t &log);

/** Calculates proper size of the log buffer and updates related fields.
Calculations are based on current value of srv_log_buffer_size. Note,
that the proper size of the log buffer should be a power of two.
@param[out]	log		redo log */
static void log_calc_buf_size(log_t &log);

/**************************************************/ /**

 @name	Initialization and finalization of log_sys

 *******************************************************/

/* @{ */

bool log_sys_init(uint32_t n_files, uint64_t file_size, space_id_t space_id) {
  ut_a(log_sys == nullptr);

  /* The log_sys_object is pointer to aligned_pointer. That's
  temporary solution until we refactor redo log more.

  That's required for now, because the aligned_pointer, has dtor
  which tries to free the memory and as long as this is global
  variable it will have the dtor called. However because we can
  exit without proper cleanup for redo log in some cases, we
  need to forbid dtor calls then. */

  log_sys_object = UT_NEW_NOKEY(aligned_pointer<log_t>{});

  log_sys_object->create();
  log_sys = *log_sys_object;

  log_t &log = *log_sys;

  /* Initialize simple value fields. */
  log.dict_persist_margin.store(0);
  log.periodical_checkpoints_enabled = false;
  log.format = LOG_HEADER_FORMAT_CURRENT;
  log.files_space_id = space_id;
  log.state = log_state_t::OK;
  log.n_log_ios_old = log.n_log_ios;
  log.last_printout_time = time(nullptr);

  ut_a(file_size <= std::numeric_limits<uint64_t>::max() / n_files);
  log.file_size = file_size;
  log.n_files = n_files;
  log.files_real_capacity = file_size * n_files;

  log.current_file_lsn = LOG_START_LSN;
  log.current_file_real_offset = LOG_FILE_HDR_SIZE;
  log_files_update_offsets(log, log.current_file_lsn);

  log.checkpointer_event = os_event_create("log_checkpointer_event");
  log.write_notifier_event = os_event_create("log_write_notifier_event");
  log.flush_notifier_event = os_event_create("log_flush_notifier_event");
  log.writer_event = os_event_create("log_writer_event");
  log.flusher_event = os_event_create("log_flusher_event");

  mutex_create(LATCH_ID_LOG_CHECKPOINTER, &log.checkpointer_mutex);
  mutex_create(LATCH_ID_LOG_CLOSER, &log.closer_mutex);
  mutex_create(LATCH_ID_LOG_WRITER, &log.writer_mutex);
  mutex_create(LATCH_ID_LOG_FLUSHER, &log.flusher_mutex);
  mutex_create(LATCH_ID_LOG_WRITE_NOTIFIER, &log.write_notifier_mutex);
  mutex_create(LATCH_ID_LOG_FLUSH_NOTIFIER, &log.flush_notifier_mutex);

  log.sn_lock.create(log_sn_lock_key, SYNC_LOG_SN, 64);

  /* Allocate buffers. */
  log_allocate_buffer(log);
  log_allocate_write_ahead_buffer(log);
  log_allocate_checkpoint_buffer(log);
  log_allocate_recent_written(log);
  log_allocate_recent_closed(log);
  log_allocate_flush_events(log);
  log_allocate_write_events(log);
  log_allocate_file_header_buffers(log);

  log_calc_buf_size(log);

  if (!log_calc_max_ages(log)) {
    ib::error(ER_IB_MSG_1267)
        << "Cannot continue operation. ib_logfiles are too"
        << " small for innodb_thread_concurrency " << srv_thread_concurrency
        << ". The combined size of"
        << " ib_logfiles should be bigger than"
        << " 200 kB * innodb_thread_concurrency. To get mysqld"
        << " to start up, set innodb_thread_concurrency in"
        << " my.cnf to a lower value, for example, to 8. After"
        << " an ERROR-FREE shutdown of mysqld you can adjust"
        << " the size of ib_logfiles. " << INNODB_PARAMETERS_MSG;

    return (false);
  }

  return (true);
}

void log_start(log_t &log, checkpoint_no_t checkpoint_no, lsn_t checkpoint_lsn,
               lsn_t start_lsn) {
  ut_a(log_sys != nullptr);
  ut_a(checkpoint_lsn >= OS_FILE_LOG_BLOCK_SIZE);
  ut_a(checkpoint_lsn >= LOG_START_LSN);
  ut_a(start_lsn >= checkpoint_lsn);

  log.recovered_lsn = start_lsn;
  log.last_checkpoint_lsn = checkpoint_lsn;
  log.next_checkpoint_no = checkpoint_no;
  log.available_for_checkpoint_lsn = checkpoint_lsn;

  log_update_limits(log);

  log.sn = log_translate_lsn_to_sn(log.recovered_lsn);

  if ((start_lsn + LOG_BLOCK_TRL_SIZE) % OS_FILE_LOG_BLOCK_SIZE == 0) {
    start_lsn += LOG_BLOCK_TRL_SIZE + LOG_BLOCK_HDR_SIZE;
  } else if (start_lsn % OS_FILE_LOG_BLOCK_SIZE == 0) {
    start_lsn += LOG_BLOCK_HDR_SIZE;
  }
  ut_a(start_lsn > LOG_START_LSN);

  log.recent_written.add_link(0, start_lsn);
  log.recent_written.advance_tail();
  ut_a(log_buffer_ready_for_write_lsn(log) == start_lsn);

  log.recent_closed.add_link(0, start_lsn);
  log.recent_closed.advance_tail();
  ut_a(log_buffer_dirty_pages_added_up_to_lsn(log) == start_lsn);

  log.write_lsn = start_lsn;
  log.flushed_to_disk_lsn = start_lsn;

  log_files_update_offsets(log, start_lsn);

  log.write_ahead_end_offset =
      ut_uint64_align_up(log.current_file_end_offset, srv_log_write_ahead_size);

  lsn_t block_lsn;
  byte *block;

  block_lsn = ut_uint64_align_down(start_lsn, OS_FILE_LOG_BLOCK_SIZE);

  ut_a(block_lsn % log.buf_size + OS_FILE_LOG_BLOCK_SIZE <= log.buf_size);

  block = static_cast<byte *>(log.buf) + block_lsn % log.buf_size;

  log_block_set_hdr_no(block, log_block_convert_lsn_to_no(block_lsn));

  log_block_set_flush_bit(block, true);

  log_block_set_data_len(block, start_lsn - block_lsn);

  log_block_set_first_rec_group(block, start_lsn % OS_FILE_LOG_BLOCK_SIZE);

  /* Do not reorder writes above, below this line. For x86 this
  protects only from unlikely compile-time reordering. */
  std::atomic_thread_fence(std::memory_order_release);
}

void log_sys_close() {
  ut_a(log_sys != nullptr);

  log_t &log = *log_sys;

  log_deallocate_file_header_buffers(log);
  log_deallocate_write_events(log);
  log_deallocate_flush_events(log);
  log_deallocate_recent_closed(log);
  log_deallocate_recent_written(log);
  log_deallocate_checkpoint_buffer(log);
  log_deallocate_write_ahead_buffer(log);
  log_deallocate_buffer(log);

  log.sn_lock.free();

  mutex_free(&log.write_notifier_mutex);
  mutex_free(&log.flush_notifier_mutex);
  mutex_free(&log.flusher_mutex);
  mutex_free(&log.writer_mutex);
  mutex_free(&log.closer_mutex);
  mutex_free(&log.checkpointer_mutex);

  os_event_destroy(log.write_notifier_event);
  os_event_destroy(log.flush_notifier_event);
  os_event_destroy(log.checkpointer_event);
  os_event_destroy(log.writer_event);
  os_event_destroy(log.flusher_event);

  log_sys_object->destroy();

  ut_free(log_sys_object);
  log_sys_object = nullptr;

  log_sys = nullptr;
}

/* @} */

/**************************************************/ /**

 @name	Start / stop of background threads

 *******************************************************/

/* @{ */

void log_writer_thread_active_validate(const log_t &log) {
  ut_a(log.writer_thread_alive.load());
}

void log_closer_thread_active_validate(const log_t &log) {
  ut_a(log.closer_thread_alive.load());
}

void log_background_write_threads_active_validate(const log_t &log) {
  ut_ad(!log.disable_redo_writes);

  ut_a(log.writer_thread_alive.load());

  ut_a(log.flusher_thread_alive.load());
}

void log_background_threads_active_validate(const log_t &log) {
  log_background_write_threads_active_validate(log);

  ut_a(log.write_notifier_thread_alive.load());
  ut_a(log.flush_notifier_thread_alive.load());

  ut_a(log.closer_thread_alive.load());

  ut_a(log.checkpointer_thread_alive.load());
}

void log_background_threads_inactive_validate(const log_t &log) {
  ut_a(!log.checkpointer_thread_alive.load());
  ut_a(!log.closer_thread_alive.load());
  ut_a(!log.write_notifier_thread_alive.load());
  ut_a(!log.flush_notifier_thread_alive.load());
  ut_a(!log.writer_thread_alive.load());
  ut_a(!log.flusher_thread_alive.load());
}

void log_start_background_threads(log_t &log) {
  ib::info(ER_IB_MSG_1258) << "Log background threads are being started...";

  std::atomic_thread_fence(std::memory_order_seq_cst);

  log_background_threads_inactive_validate(log);

  ut_ad(!log.disable_redo_writes);
  ut_a(!srv_read_only_mode);
  ut_a(log.sn.load() > 0);

  log.closer_thread_alive = true;
  log.checkpointer_thread_alive = true;
  log.writer_thread_alive = true;
  log.flusher_thread_alive = true;
  log.write_notifier_thread_alive = true;
  log.flush_notifier_thread_alive = true;

  log.should_stop_threads = false;

  std::atomic_thread_fence(std::memory_order_seq_cst);

  os_thread_create(log_checkpointer_thread_key, log_checkpointer, &log);

  os_thread_create(log_closer_thread_key, log_closer, &log);

  os_thread_create(log_writer_thread_key, log_writer, &log);

  os_thread_create(log_flusher_thread_key, log_flusher, &log);

  os_thread_create(log_write_notifier_thread_key, log_write_notifier, &log);

  os_thread_create(log_flush_notifier_thread_key, log_flush_notifier, &log);

  log_background_threads_active_validate(log);
}

void log_stop_background_threads(log_t &log) {
  /* We cannot stop threads when x-lock is acquired, because of scenario:
          * log_checkpointer starts log_checkpoint()
          * log_checkpoint() asks to persist dd dynamic metadata
          * dict_persist_dd_table_buffer() tries to write to redo
          * but cannot acquire shared lock on log.sn_lock
          * so log_checkpointer thread waits for this thread
            until the x-lock is released
          * but this thread waits until log background threads
            have been stopped - log_checkpointer is not stopped. */
  ut_ad(!log.sn_lock.x_own());

  ib::info(ER_IB_MSG_1259) << "Log background threads are being closed...";

  std::atomic_thread_fence(std::memory_order_seq_cst);

  log_background_threads_active_validate(log);

  ut_a(!srv_read_only_mode);

  log.should_stop_threads = true;

  /* Wait until threads are closed. */
  while (log.closer_thread_alive.load() ||
         log.checkpointer_thread_alive.load() ||
         log.writer_thread_alive.load() || log.flusher_thread_alive.load() ||
         log.write_notifier_thread_alive.load() ||
         log.flush_notifier_thread_alive.load()) {
    os_thread_sleep(100 * 1000);
  }

  std::atomic_thread_fence(std::memory_order_seq_cst);

  log_background_threads_inactive_validate(log);
}

bool log_threads_active(const log_t &log) {
  return (log.closer_thread_alive.load() ||
          log.checkpointer_thread_alive.load() ||
          log.writer_thread_alive.load() || log.flusher_thread_alive.load() ||
          log.write_notifier_thread_alive.load() ||
          log.flush_notifier_thread_alive.load());
}

/* @} */

/**************************************************/ /**

 @name	Status printing

 *******************************************************/

/* @{ */

void log_print(const log_t &log, FILE *file) {
  lsn_t last_checkpoint_lsn;
  lsn_t dirty_pages_added_up_to_lsn;
  lsn_t ready_for_write_lsn;
  lsn_t write_lsn;
  lsn_t flush_lsn;
  lsn_t oldest_lsn;
  lsn_t max_assigned_lsn;
  lsn_t current_lsn;

  last_checkpoint_lsn = log.last_checkpoint_lsn;
  dirty_pages_added_up_to_lsn = log_buffer_dirty_pages_added_up_to_lsn(log);
  ready_for_write_lsn = log_buffer_ready_for_write_lsn(log);
  write_lsn = log.write_lsn;
  flush_lsn = log.flushed_to_disk_lsn;
  oldest_lsn = log.available_for_checkpoint_lsn;
  max_assigned_lsn = log_get_lsn(log);
  current_lsn = log_get_lsn(log);

  fprintf(file,
          "Log sequence number          " LSN_PF
          "\n"
          "Log buffer assigned up to    " LSN_PF
          "\n"
          "Log buffer completed up to   " LSN_PF
          "\n"
          "Log written up to            " LSN_PF
          "\n"
          "Log flushed up to            " LSN_PF
          "\n"
          "Added dirty pages up to      " LSN_PF
          "\n"
          "Pages flushed up to          " LSN_PF
          "\n"
          "Last checkpoint at           " LSN_PF "\n",
          current_lsn, max_assigned_lsn, ready_for_write_lsn, write_lsn,
          flush_lsn, dirty_pages_added_up_to_lsn, oldest_lsn,
          last_checkpoint_lsn);

  time_t current_time = time(nullptr);

  double time_elapsed = difftime(current_time, log.last_printout_time);

  if (time_elapsed <= 0) {
    time_elapsed = 1;
  }

  fprintf(
      file, ULINTPF " log i/o's done, %.2f log i/o's/second\n",
      ulint(log.n_log_ios),
      static_cast<double>(log.n_log_ios - log.n_log_ios_old) / time_elapsed);

  log.n_log_ios_old = log.n_log_ios;
  log.last_printout_time = current_time;
}

void log_refresh_stats(log_t &log) {
  log.n_log_ios_old = log.n_log_ios;
  log.last_printout_time = time(nullptr);
}

/* @} */

/**************************************************/ /**

 @name	Resizing of buffers

 *******************************************************/

/* @{ */

bool log_buffer_resize_low(log_t &log, size_t new_size, lsn_t end_lsn) {
  ut_ad(log_checkpointer_mutex_own(log));
  ut_ad(log_writer_mutex_own(log));

  const lsn_t start_lsn =
      ut_uint64_align_down(log.write_lsn.load(), OS_FILE_LOG_BLOCK_SIZE);

  end_lsn = ut_uint64_align_up(end_lsn, OS_FILE_LOG_BLOCK_SIZE);

  if (end_lsn == start_lsn) {
    end_lsn += OS_FILE_LOG_BLOCK_SIZE;
  }

  ut_ad(end_lsn - start_lsn <= log.buf_size);

  if (end_lsn - start_lsn > new_size) {
    return (false);
  }

  /* Save the contents. */
  byte *tmp_buf = UT_NEW_ARRAY_NOKEY(byte, end_lsn - start_lsn);
  for (auto i = start_lsn; i < end_lsn; i += OS_FILE_LOG_BLOCK_SIZE) {
    std::memcpy(&tmp_buf[i - start_lsn], &log.buf[i % log.buf_size],
                OS_FILE_LOG_BLOCK_SIZE);
  }

  /* Re-allocate log buffer. */
  srv_log_buffer_size = static_cast<ulong>(new_size);
  log_deallocate_buffer(log);
  log_allocate_buffer(log);

  /* Restore the contents. */
  for (auto i = start_lsn; i < end_lsn; i += OS_FILE_LOG_BLOCK_SIZE) {
    std::memcpy(&log.buf[i % new_size], &tmp_buf[i - start_lsn],
                OS_FILE_LOG_BLOCK_SIZE);
  }
  UT_DELETE_ARRAY(tmp_buf);

  log_calc_buf_size(log);

  ut_a(srv_log_buffer_size == log.buf_size);

  ib::info(ER_IB_MSG_1260) << "srv_log_buffer_size was extended to "
                           << log.buf_size << ".";

  return (true);
}

bool log_buffer_resize(log_t &log, size_t new_size) {
  log_buffer_x_lock_enter(log);

  const lsn_t end_lsn = log_get_lsn(log);

  log_checkpointer_mutex_enter(log);
  log_writer_mutex_enter(log);

  const bool ret = log_buffer_resize_low(log, new_size, end_lsn);

  log_writer_mutex_exit(log);
  log_checkpointer_mutex_exit(log);
  log_buffer_x_lock_exit(log);

  return (ret);
}

void log_write_ahead_resize(log_t &log, size_t new_size) {
  ut_a(new_size >= INNODB_LOG_WRITE_AHEAD_SIZE_MIN);
  ut_a(new_size <= INNODB_LOG_WRITE_AHEAD_SIZE_MAX);

  log_writer_mutex_enter(log);

  log_deallocate_write_ahead_buffer(log);
  srv_log_write_ahead_size = static_cast<ulong>(new_size);

  log.write_ahead_end_offset =
      ut_uint64_align_down(log.write_ahead_end_offset, new_size);

  log_allocate_write_ahead_buffer(log);

  log_writer_mutex_exit(log);
}

static void log_calc_buf_size(log_t &log) {
  ut_a(srv_log_buffer_size >= INNODB_LOG_BUFFER_SIZE_MIN);
  ut_a(srv_log_buffer_size <= INNODB_LOG_BUFFER_SIZE_MAX);

  log.buf_size = srv_log_buffer_size;

  /* The following update has to be the last operation during resize
  procedure of log buffer. That's because since this moment, possibly
  new concurrent writes for higher sn will start (which were waiting
  for free space in the log buffer). */

  log.buf_size_sn = log_translate_lsn_to_sn(log.buf_size);

  log_update_limits(log);
}

/* @} */

/**************************************************/ /**

 @name	Allocation / deallocation of buffers

 *******************************************************/

/* @{ */

static void log_allocate_buffer(log_t &log) {
  ut_a(srv_log_buffer_size >= INNODB_LOG_BUFFER_SIZE_MIN);
  ut_a(srv_log_buffer_size <= INNODB_LOG_BUFFER_SIZE_MAX);
  ut_a(srv_log_buffer_size >= 4 * UNIV_PAGE_SIZE);

  log.buf.create(srv_log_buffer_size);
}

static void log_deallocate_buffer(log_t &log) { log.buf.destroy(); }

static void log_allocate_write_ahead_buffer(log_t &log) {
  ut_a(srv_log_write_ahead_size >= INNODB_LOG_WRITE_AHEAD_SIZE_MIN);
  ut_a(srv_log_write_ahead_size <= INNODB_LOG_WRITE_AHEAD_SIZE_MAX);

  log.write_ahead_buf_size = srv_log_write_ahead_size;
  log.write_ahead_buf.create(log.write_ahead_buf_size);
}

static void log_deallocate_write_ahead_buffer(log_t &log) {
  log.write_ahead_buf.destroy();
}

static void log_allocate_checkpoint_buffer(log_t &log) {
  log.checkpoint_buf.create(OS_FILE_LOG_BLOCK_SIZE);
}

static void log_deallocate_checkpoint_buffer(log_t &log) {
  log.checkpoint_buf.destroy();
}

static void log_allocate_flush_events(log_t &log) {
  const size_t n = srv_log_flush_events;

  ut_a(log.flush_events == nullptr);
  ut_a(n >= 1);
  ut_a((n & (n - 1)) == 0);

  log.flush_events_size = n;
  log.flush_events = UT_NEW_ARRAY_NOKEY(os_event_t, n);

  for (size_t i = 0; i < log.flush_events_size; ++i) {
    log.flush_events[i] = os_event_create("log_flush_event");
  }
}

static void log_deallocate_flush_events(log_t &log) {
  ut_a(log.flush_events != nullptr);

  for (size_t i = 0; i < log.flush_events_size; ++i) {
    os_event_destroy(log.flush_events[i]);
  }

  UT_DELETE_ARRAY(log.flush_events);
  log.flush_events = nullptr;
}

static void log_allocate_write_events(log_t &log) {
  const size_t n = srv_log_write_events;

  ut_a(log.write_events == nullptr);
  ut_a(n >= 1);
  ut_a((n & (n - 1)) == 0);

  log.write_events_size = n;
  log.write_events = UT_NEW_ARRAY_NOKEY(os_event_t, n);

  for (size_t i = 0; i < log.write_events_size; ++i) {
    log.write_events[i] = os_event_create("log_write_event");
  }
}

static void log_deallocate_write_events(log_t &log) {
  ut_a(log.write_events != nullptr);

  for (size_t i = 0; i < log.write_events_size; ++i) {
    os_event_destroy(log.write_events[i]);
  }

  UT_DELETE_ARRAY(log.write_events);
  log.write_events = nullptr;
}

static void log_allocate_recent_written(log_t &log) {
  log.recent_written = Link_buf<lsn_t>{srv_log_recent_written_size};
}
static void log_deallocate_recent_written(log_t &log) {
  log.recent_written.validate_no_links();
  log.recent_written = {};
}

static void log_allocate_recent_closed(log_t &log) {
  log.recent_closed = Link_buf<lsn_t>{srv_log_recent_closed_size};
}

static void log_deallocate_recent_closed(log_t &log) {
  log.recent_closed.validate_no_links();
  log.recent_closed = {};
}

static void log_allocate_file_header_buffers(log_t &log) {
  const uint32_t n_files = log.n_files;

  using Buf_ptr = aligned_array_pointer<byte, OS_FILE_LOG_BLOCK_SIZE>;

  log.file_header_bufs = UT_NEW_ARRAY_NOKEY(Buf_ptr, n_files);

  for (uint32_t i = 0; i < n_files; i++) {
    log.file_header_bufs[i].create(LOG_FILE_HDR_SIZE);
  }
}

static void log_deallocate_file_header_buffers(log_t &log) {
  ut_a(log.n_files > 0);
  ut_a(log.file_header_bufs != nullptr);

  UT_DELETE_ARRAY(log.file_header_bufs);
  log.file_header_bufs = nullptr;
}

/* @} */

/**************************************************/ /**

 @name	Log position locking (for replication)

 *******************************************************/

/* @{ */

void log_position_lock(log_t &log) {
  log_buffer_x_lock_enter(log);

  log_checkpointer_mutex_enter(log);
}

void log_position_unlock(log_t &log) {
  log_checkpointer_mutex_exit(log);

  log_buffer_x_lock_exit(log);
}

void log_position_collect_lsn_info(const log_t &log, lsn_t *current_lsn,
                                   lsn_t *checkpoint_lsn) {
  ut_ad(log_buffer_x_lock_own(log));
  ut_ad(log_checkpointer_mutex_own(log));

  *checkpoint_lsn = log.last_checkpoint_lsn.load();

  *current_lsn = log_get_lsn(log);

  /* Ensure we have redo log started. */
  ut_a(*current_lsn >= LOG_START_LSN);
  ut_a(*checkpoint_lsn >= LOG_START_LSN);

  /* Obviously current lsn cannot point to before checkpoint. */
  ut_a(*current_lsn >= *checkpoint_lsn);
}

  /* @} */

#endif /* !UNIV_HOTBACKUP */
