/*****************************************************************************

Copyright (c) 1995, 2023, Oracle and/or its affiliates.
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

 Created 12/9/1995 Heikki Tuuri
 *******************************************************/

#ifndef UNIV_HOTBACKUP

/* time, difftime */
#include <time.h>

/* fprintf */
#include <cstdio>

/* std::memcpy */
#include <cstring>

/* arch_log_sys, arch_wake_threads */
#include "arch0arch.h"

/* dblwr::set(), ... */
#include "buf0dblwr.h"

/* fil_space_t */
#include "fil0fil.h"

/* log_update_buf_limit, ... */
#include "log0buf.h"

/* log_checkpointer_mutex */
#include "log0chkp.h"

/* log_calc_max_ages, ... */
#include "log0files_capacity.h"

/* log_create_new_files */
#include "log0files_governor.h"

/* log_limits_mutex, ... */
#include "log0log.h"

/* redo_log_archive_init, ... */
#include "log0meb.h"

/* log_pre_8_0_30::FILE_BASE_NAME */
#include "log0pre_8_0_30.h"

/* recv_sys->dblwr_state, ... */
#include "log0recv.h"

/* log_t::X */
#include "log0sys.h"

/* log_writer_mutex */
#include "log0write.h"

/* mtr_t::s_logging */
#include "mtr0mtr.h"

/* os_event_create, ... */
#include "os0event.h"

/* os_thread_create, ... */
#include "os0thread-create.h"

/* os_event_sleep */
#include "os0thread.h"

/* srv_log_buffer_size, ... */
#include "srv0srv.h"

/* srv_is_being_started */
#include "srv0start.h"

/* mysql_pfs_key_t */
#include "sync0sync.h"

/* LATCH_ID_LOG_WRITER, ... */
#include "sync0types.h"

/* ut_uint64_align_down */
#include "ut0byte.h"

/* Link_buf */
#include "ut0link_buf.h"

/* UT_DELETE_ARRAY */
#include "ut0new.h"

// clang-format off
/**
@page PAGE_INNODB_REDO_LOG Innodb redo log

@section sect_redo_log_general General idea of redo log

The redo log is a write ahead log of changes applied to contents of data pages.
It provides durability for all changes applied to the pages. In case of crash,
it is used to recover modifications to pages that were modified but have not
been flushed to disk.

@note In case of a shutdown (unless innodb-fast-shutdown = 2), the redo log
should be logically empty. This means that after the checkpoint lsn there
should be no records to apply. However the log files still could contain
some old data (which is not used during the recovery process).

Every change to content of a data page must be done through a mini-transaction
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

   @diafile storage/innobase/log/arch_deleting.dia "Reclaiming space in the redo log"

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
has been flushed to the header of the redo log file containing that lsn.

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
// clang-format on

/** Redo log system. Singleton used to populate global pointer. */
ut::aligned_pointer<log_t, ut::INNODB_CACHE_LINE_SIZE> *log_sys_object;

/** Redo log system (singleton). */
log_t *log_sys;

#ifdef UNIV_PFS_MEMORY
PSI_memory_key log_buffer_memory_key;
#endif /* UNIV_PFS_MEMORY */

#ifdef UNIV_PFS_THREAD

/** PFS key for the log files governor thread. */
mysql_pfs_key_t log_files_governor_thread_key;

/** PFS key for the log writer thread. */
mysql_pfs_key_t log_writer_thread_key;

/** PFS key for the log checkpointer thread. */
mysql_pfs_key_t log_checkpointer_thread_key;

/** PFS key for the log flusher thread. */
mysql_pfs_key_t log_flusher_thread_key;

/** PFS key for the log flush notifier thread. */
mysql_pfs_key_t log_flush_notifier_thread_key;

/** PFS key for the log write notifier thread. */
mysql_pfs_key_t log_write_notifier_thread_key;

#endif /* UNIV_PFS_THREAD */

/** Allocates the log system and initializes all log mutexes and log events. */
static void log_sys_create();

/** Handles the log creator name stored on the disk (in the redo log files).
Does whatever is required to handle the creator name and decides if further
initialization of InnoDB might be allowed.
@remarks Recognizes log creator by its name and marks that within recv_sys
by setting recv_sys->is_meb_db or recv_sys->is_cloned_db. Disables the dblwr
if log files were created by MEB. Emits message to the error log when the
recognized creator name is not an usual MySQL's creator name.
@param[in]  log  redo log with log.m_creator_name read from files
@retval DB_SUCCESS if further initialization might be continued (note that
potentially the creator name could be recognized as an unknown one)
@retval DB_ERROR if further initialization must not be continued (for example
because a foreign creator name was detected but read-only mode is turned on) */
static dberr_t log_sys_handle_creator(log_t &log);

/** Recognizes log format and emits corresponding message to the error log.
Returns DB_ERROR if format is too old and no longer supported or format is
not the current one and innodb_force_recovery != 0 is passed.
@param[in]  log    redo log
@return DB_SUCCESS or error */
static dberr_t log_sys_check_format(const log_t &log);

/** Checks if the redo log directory exists, can be listed and contains
at least one redo log file.
@remarks Redo log file is recognized only by looking at the file name,
accordingly to the configured ruleset (@see log_configure()).
@param[in]      ctx           redo log files context
@param[in,out]  path          path to redo log directory checked during this
call
@param[out]  found_files  true iff found at least one redo log file
@retval DB_SUCCESS    if succeeded to list the log directory
@retval DB_ERROR      if failed to list the existing log directory
@retval DB_NOT_FOUND  if the log directory does not exist */
static dberr_t log_sys_check_directory(const Log_files_context &ctx,
                                       std::string &path, bool &found_files);

/** Free the log system data structures. Deallocate all the related memory. */
static void log_sys_free();

/** Calculates proper size for the log buffer and allocates the log buffer.
@param[out]     log     redo log */
static void log_allocate_buffer(log_t &log);

/** Deallocates the log buffer.
@param[out]     log     redo log */
static void log_deallocate_buffer(log_t &log);

/** Allocates the log write-ahead buffer (aligned to system page for
easier migrations between NUMA nodes).
@param[out]     log     redo log */
static void log_allocate_write_ahead_buffer(log_t &log);

/** Deallocates the log write-ahead buffer.
@param[out]     log     redo log */
static void log_deallocate_write_ahead_buffer(log_t &log);

/** Allocates the array with flush events.
@param[out]     log     redo log */
static void log_allocate_flush_events(log_t &log);

/** Deallocates the array with flush events.
@param[out]     log     redo log */
static void log_deallocate_flush_events(log_t &log);

/** Deallocates the array with write events.
@param[out]     log     redo log */
static void log_deallocate_write_events(log_t &log);

/** Allocates the array with write events.
@param[out]     log     redo log */
static void log_allocate_write_events(log_t &log);

/** Allocates the log recent written buffer.
@param[out]     log     redo log */
static void log_allocate_recent_written(log_t &log);

/** Deallocates the log recent written buffer.
@param[out]     log     redo log */
static void log_deallocate_recent_written(log_t &log);

/** Allocates the log recent closed buffer.
@param[out]     log     redo log */
static void log_allocate_recent_closed(log_t &log);

/** Deallocates the log recent closed buffer.
@param[out]     log     redo log */
static void log_deallocate_recent_closed(log_t &log);

/** Resets the log encryption buffer (used to write encryption headers).
@param[out]     log     redo log */
static void log_reset_encryption_buffer(log_t &log);

/** Calculates proper size of the log buffer and updates related fields.
Calculations are based on current value of srv_log_buffer_size. Note,
that the proper size of the log buffer should be a power of two.
@param[out]     log             redo log */
static void log_calc_buf_size(log_t &log);

/** Pauses writer, flusher and notifiers and switches user threads
to write log as former version.
NOTE: These pause/resume functions should be protected by mutex while serving.
The caller innodb_log_writer_threads_update() is protected
by LOCK_global_system_variables in mysqld.
@param[out]     log     redo log */
static void log_pause_writer_threads(log_t &log);

/** Resumes writer, flusher and notifiers and switches user threads
not to write log.
@param[out]     log     redo log */
static void log_resume_writer_threads(log_t &log);

/**************************************************/ /**

 @name  Allocation and deallocation of log_sys

 *******************************************************/

/** @{ */

static void log_sys_create() {
  ut_a(log_sys == nullptr);

  /* The log_sys_object is pointer to aligned_pointer. That's
  temporary solution until we refactor redo log more.

  That's required for now, because the aligned_pointer, has dtor
  which tries to free the memory and as long as this is global
  variable it will have the dtor called. However because we can
  exit without proper cleanup for redo log in some cases, we
  need to forbid dtor calls then. */

  using log_t_aligned_pointer = std::decay_t<decltype(*log_sys_object)>;
  log_sys_object =
      ut::new_withkey<log_t_aligned_pointer>(UT_NEW_THIS_FILE_PSI_KEY);
  log_sys_object->alloc_withkey(UT_NEW_THIS_FILE_PSI_KEY);

  log_sys = *log_sys_object;

  log_t &log = *log_sys;

  /* Initialize simple value fields. */
  log.dict_persist_margin.store(0);
  log.periodical_checkpoints_enabled = false;
  log.m_format = Log_format::CURRENT;
  log.m_creator_name = LOG_HEADER_CREATOR_CURRENT;
  log.n_log_ios_old = log.n_log_ios;
  log.last_printout_time = time(nullptr);
  log.m_requested_files_consumption = false;
  log.m_writer_inside_extra_margin = false;
  log.m_oldest_need_lsn_lowerbound = 0;
  log.m_unused_files_count = 0;
  log.m_encryption_metadata = {};

  log.checkpointer_event = os_event_create();
  log.closer_event = os_event_create();
  log.write_notifier_event = os_event_create();
  log.flush_notifier_event = os_event_create();
  log.writer_event = os_event_create();
  log.flusher_event = os_event_create();
  log.old_flush_event = os_event_create();
  os_event_set(log.old_flush_event);
  log.writer_threads_resume_event = os_event_create();
  os_event_set(log.writer_threads_resume_event);
  log.sn_lock_event = os_event_create();
  os_event_set(log.sn_lock_event);
  log.m_files_governor_event = os_event_create();
  log.m_files_governor_iteration_event = os_event_create();
  log.m_file_removed_event = os_event_create();
  log.next_checkpoint_event = os_event_create();

  mutex_create(LATCH_ID_LOG_CHECKPOINTER, &log.checkpointer_mutex);
  mutex_create(LATCH_ID_LOG_CLOSER, &log.closer_mutex);
  mutex_create(LATCH_ID_LOG_WRITER, &log.writer_mutex);
  mutex_create(LATCH_ID_LOG_FLUSHER, &log.flusher_mutex);
  mutex_create(LATCH_ID_LOG_WRITE_NOTIFIER, &log.write_notifier_mutex);
  mutex_create(LATCH_ID_LOG_FLUSH_NOTIFIER, &log.flush_notifier_mutex);
  mutex_create(LATCH_ID_LOG_LIMITS, &log.limits_mutex);
  mutex_create(LATCH_ID_LOG_FILES, &log.m_files_mutex);
  mutex_create(LATCH_ID_LOG_SN_MUTEX, &log.sn_x_lock_mutex);

#ifdef UNIV_PFS_RWLOCK
  /* pfs_psi is separated from sn_lock_inst,
  because not needed for non debug build. */
  log.pfs_psi =
      PSI_RWLOCK_CALL(init_rwlock)(log_sn_lock_key.m_value, &log.pfs_psi);
#endif /* UNIV_PFS_RWLOCK */
#ifdef UNIV_DEBUG
  /* initialize rw_lock without pfs_psi */
  log.sn_lock_inst = static_cast<rw_lock_t *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(*log.sn_lock_inst)));
  new (log.sn_lock_inst) rw_lock_t;
  rw_lock_create_func(log.sn_lock_inst, LATCH_ID_LOG_SN, UT_LOCATION_HERE);
#endif /* UNIV_DEBUG */

  /* Allocate buffers. */
  log_allocate_buffer(log);
  log_allocate_write_ahead_buffer(log);
  log_allocate_recent_written(log);
  log_allocate_recent_closed(log);
  log_allocate_flush_events(log);
  log_allocate_write_events(log);

  log_reset_encryption_buffer(log);

  log_calc_buf_size(log);

  log_consumer_register(log, &(log.m_checkpoint_consumer));
}

static void log_fix_first_rec_group(lsn_t block_lsn,
                                    Log_data_block_header &block_header,
                                    lsn_t start_lsn) {
  const uint16_t on_disc_value = block_header.m_first_rec_group;
  uint16_t change_to_value = 0;  // no change
  if (recv_sys != nullptr && recv_sys->last_block_first_mtr_boundary != 0 &&
      block_lsn < recv_sys->last_block_first_mtr_boundary &&
      recv_sys->last_block_first_mtr_boundary <
          block_lsn + OS_FILE_LOG_BLOCK_SIZE &&
      on_disc_value !=
          recv_sys->last_block_first_mtr_boundary % OS_FILE_LOG_BLOCK_SIZE) {
    ut_a(log_is_data_lsn(recv_sys->last_block_first_mtr_boundary));
    change_to_value =
        recv_sys->last_block_first_mtr_boundary % OS_FILE_LOG_BLOCK_SIZE;
    /* We must not start with invalid first_rec_group in the first block,
    because if we crashed, we could be unable to recover. We do NOT have
    guarantee that the first_rec_group was correct because recovery did
    not report error. The first_rec_group was used only to locate the
    beginning of the log for recovery. For later blocks it was not used.
    It might be corrupted on disk and stay unnoticed if checksums for
    log blocks are disabled. In such case it would be better to repair
    it now instead of relying on the broken value and risking data loss.
    We emit warning to notice user about the situation. We repair that
    only in the log buffer. */
  } else if (on_disc_value == 0) {
    /* Again, if it was zero, for any reason, we prefer to fix it
    before starting (we emit warning). */
    change_to_value = start_lsn % OS_FILE_LOG_BLOCK_SIZE;
  }
  if (change_to_value != 0) {
    ib::warn(ER_IB_RECV_FIRST_REC_GROUP_INVALID, uint(on_disc_value),
             uint(change_to_value));
    block_header.m_first_rec_group = change_to_value;
  }
}

dberr_t log_start(log_t &log, lsn_t checkpoint_lsn, lsn_t start_lsn,
                  bool allow_checkpoints) {
  ut_a(log_sys != nullptr);
  ut_a(checkpoint_lsn >= OS_FILE_LOG_BLOCK_SIZE);
  ut_a(checkpoint_lsn >= LOG_START_LSN);
  ut_a(start_lsn >= checkpoint_lsn);
  ut_a(arch_log_sys == nullptr || !arch_log_sys->is_active());

  log.write_to_file_requests_total.store(0);
  log.write_to_file_requests_interval.store(std::chrono::seconds::zero());

  log.recovered_lsn = start_lsn;
  log.last_checkpoint_lsn = checkpoint_lsn;
  log.available_for_checkpoint_lsn = checkpoint_lsn;
  log.m_allow_checkpoints.store(allow_checkpoints);

  ut_a((log.sn.load(std::memory_order_acquire) & SN_LOCKED) == 0);
  log.sn = log_translate_lsn_to_sn(log.recovered_lsn);
  log.sn_locked = log_translate_lsn_to_sn(log.recovered_lsn);

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

  log.write_ahead_end_offset = 0;

  lsn_t block_lsn;
  byte *block;

  block_lsn = ut_uint64_align_down(start_lsn, OS_FILE_LOG_BLOCK_SIZE);

  ut_a(block_lsn % log.buf_size + OS_FILE_LOG_BLOCK_SIZE <= log.buf_size);

  block = static_cast<byte *>(log.buf) + block_lsn % log.buf_size;

  Log_data_block_header block_header;

  if (log.m_format == Log_format::CURRENT) {
    {
      auto file = log.m_files.find(block_lsn);
      ut_a(file != log.m_files.end());
      auto file_handle = file->open(Log_file_access_mode::READ_ONLY);
      ut_a(file_handle.is_open());

      const auto err = log_data_blocks_read(
          file_handle, file->offset(block_lsn), OS_FILE_LOG_BLOCK_SIZE, block);
      if (err != DB_SUCCESS) {
        return err;
      }
      log_data_block_header_deserialize(block, block_header);
    }

    /* FOLLOWING IS NOT NEEDED IF WE DON'T ALLOW DISABLING crc32 checksum */
    log_fix_first_rec_group(block_lsn, block_header, start_lsn);
  } else {
    ut_a(start_lsn % OS_FILE_LOG_BLOCK_SIZE == LOG_BLOCK_HDR_SIZE);
    std::memset(block, 0x00, OS_FILE_LOG_BLOCK_SIZE);
    block_header.m_first_rec_group = LOG_BLOCK_HDR_SIZE;
  }

  block_header.set_lsn(block_lsn);
  /* The last mtr in the block might have been incomplete, thus we trim the
  block to the start_lsn which is the end of the last mtr found in recovery */
  block_header.m_data_len = start_lsn - block_lsn;
  ut_ad(LOG_BLOCK_HDR_SIZE <= block_header.m_first_rec_group);
  ut_ad(block_header.m_first_rec_group <= block_header.m_data_len);

  log_data_block_header_serialize(block_header, block);

  log_update_buf_limit(log, start_lsn);

  const dberr_t err = log_files_start(log);

  /* Do not reorder writes above, below this line. For x86 this
  protects only from unlikely compile-time reordering. */
  std::atomic_thread_fence(std::memory_order_release);

  return err;
}

static void log_sys_free() {
  ut_a(log_sys != nullptr);

  log_t &log = *log_sys;

  log_consumer_unregister(log, &(log.m_checkpoint_consumer));

  log_deallocate_write_events(log);
  log_deallocate_flush_events(log);
  log_deallocate_recent_closed(log);
  log_deallocate_recent_written(log);
  log_deallocate_write_ahead_buffer(log);
  log_deallocate_buffer(log);

#ifdef UNIV_DEBUG
  rw_lock_free_func(log.sn_lock_inst);
  ut::free(log.sn_lock_inst);
  log.sn_lock_inst = nullptr;
#endif /* UNIV_DEBUG */
#ifdef UNIV_PFS_RWLOCK
  if (log.pfs_psi != nullptr) {
    PSI_RWLOCK_CALL(destroy_rwlock)(log.pfs_psi);
    log.pfs_psi = nullptr;
  }
#endif /* UNIV_PFS_RWLOCK */

  mutex_free(&log.m_files_mutex);
  mutex_free(&log.sn_x_lock_mutex);
  mutex_free(&log.limits_mutex);
  mutex_free(&log.write_notifier_mutex);
  mutex_free(&log.flush_notifier_mutex);
  mutex_free(&log.flusher_mutex);
  mutex_free(&log.writer_mutex);
  mutex_free(&log.closer_mutex);
  mutex_free(&log.checkpointer_mutex);

  os_event_destroy(log.next_checkpoint_event);
  os_event_destroy(log.write_notifier_event);
  os_event_destroy(log.flush_notifier_event);
  os_event_destroy(log.closer_event);
  os_event_destroy(log.checkpointer_event);
  os_event_destroy(log.writer_event);
  os_event_destroy(log.flusher_event);
  os_event_destroy(log.old_flush_event);
  os_event_destroy(log.writer_threads_resume_event);
  os_event_destroy(log.m_files_governor_event);
  os_event_destroy(log.m_files_governor_iteration_event);
  os_event_destroy(log.m_file_removed_event);
  os_event_destroy(log.sn_lock_event);

  log_sys_object->dealloc();
  ut::delete_(log_sys_object);
  log_sys_object = nullptr;

  log_sys = nullptr;
}

/** @} */

/**************************************************/ /**

 @name  Start / stop of background threads

 *******************************************************/

/** @{ */

void log_writer_thread_active_validate() { ut_a(log_writer_is_active()); }

void log_background_write_threads_active_validate(const log_t &log) {
  ut_ad(!log.disable_redo_writes);

  ut_a(log_writer_is_active());
  ut_a(log_flusher_is_active());
}

void log_background_threads_active_validate(const log_t &log) {
  log_background_write_threads_active_validate(log);

  ut_a(log_write_notifier_is_active());
  ut_a(log_flush_notifier_is_active());
  ut_a(log_checkpointer_is_active());
  ut_a(log_files_governor_is_active());
}

void log_background_threads_inactive_validate() {
  ut_a(!log_files_governor_is_active());
  ut_a(!log_checkpointer_is_active());
  ut_a(!log_write_notifier_is_active());
  ut_a(!log_flush_notifier_is_active());
  ut_a(!log_writer_is_active());
  ut_a(!log_flusher_is_active());
}

void log_start_background_threads(log_t &log) {
  ib::info(ER_IB_MSG_1258) << "Log background threads are being started...";

  log_background_threads_inactive_validate();

  ut_ad(!log.disable_redo_writes);
  ut_a(!srv_read_only_mode);
  ut_a(log.sn.load() > 0);

  log.should_stop_threads.store(false);
  log.writer_threads_paused.store(false);

  srv_threads.m_log_checkpointer =
      os_thread_create(log_checkpointer_thread_key, 0, log_checkpointer, &log);

  srv_threads.m_log_flush_notifier = os_thread_create(
      log_flush_notifier_thread_key, 0, log_flush_notifier, &log);

  srv_threads.m_log_flusher =
      os_thread_create(log_flusher_thread_key, 0, log_flusher, &log);

  srv_threads.m_log_write_notifier = os_thread_create(
      log_write_notifier_thread_key, 0, log_write_notifier, &log);

  srv_threads.m_log_writer =
      os_thread_create(log_writer_thread_key, 0, log_writer, &log);

  srv_threads.m_log_files_governor = os_thread_create(
      log_files_governor_thread_key, 0, log_files_governor, &log);

  log.m_no_more_dummy_records_requested.store(false);
  log.m_no_more_dummy_records_promised.store(false);

  srv_threads.m_log_checkpointer.start();
  srv_threads.m_log_flush_notifier.start();
  srv_threads.m_log_flusher.start();
  srv_threads.m_log_write_notifier.start();
  srv_threads.m_log_writer.start();
  srv_threads.m_log_files_governor.start();

  log_background_threads_active_validate(log);

  log_control_writer_threads(log);

  meb::redo_log_archive_init();
}

void log_stop_background_threads(log_t &log) {
  /* We cannot stop threads when x-lock is acquired, because of scenario:
          * log_checkpointer starts log_checkpoint()
          * log_checkpoint() asks to persist dd dynamic metadata
          * dict_persist_to_dd_table_buffer() tries to write to redo
          * but cannot lock on log.sn
          * so log_checkpointer thread waits for this thread
            until the x-lock is released
          * but this thread waits until log background threads
            have been stopped - log_checkpointer is not stopped. */
  ut_ad(!rw_lock_own(log.sn_lock_inst, RW_LOCK_X));

  ib::info(ER_IB_MSG_1259) << "Log background threads are being closed...";

  meb::redo_log_archive_deinit();

  log_background_threads_active_validate(log);

  ut_a(!srv_read_only_mode);

  log_resume_writer_threads(log);

  log_files_dummy_records_disable(log);

  log.should_stop_threads.store(true);

  /* Wait until threads are closed. */
  while (log_writer_is_active()) {
    os_event_set(log.writer_event);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }
  while (log_write_notifier_is_active()) {
    os_event_set(log.write_notifier_event);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }
  while (log_flusher_is_active()) {
    os_event_set(log.flusher_event);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }
  while (log_flush_notifier_is_active()) {
    os_event_set(log.flush_notifier_event);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }
  while (log_checkpointer_is_active()) {
    os_event_set(log.checkpointer_event);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }
  while (log_files_governor_is_active()) {
    os_event_set(log.m_files_governor_event);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }

  log_background_threads_inactive_validate();
}

void log_stop_background_threads_nowait(log_t &log) {
  log_resume_writer_threads(log);
  log_files_dummy_records_request_disable(log);
  log.should_stop_threads.store(true);
  log_wake_threads(log);
}

void log_make_empty_and_stop_background_threads(log_t &log) {
  log_files_dummy_records_disable(log);

  while (log_make_latest_checkpoint(log)) {
    /* It could happen, that when writing a new checkpoint,
    DD dynamic metadata was persisted, making some pages
    dirty (with the persisted data) and writing new redo
    records to protect those modifications. In such case,
    current lsn would be higher than lsn and we would need
    another iteration to ensure, that checkpoint lsn points
    to the newest lsn. */
  }

  log_stop_background_threads(log);
}

void log_wake_threads(log_t &log) {
  if (log_files_governor_is_active()) {
    os_event_set(log.m_files_governor_event);
  }
  if (log_checkpointer_is_active()) {
    os_event_set(log.checkpointer_event);
  }
  if (log_writer_is_active()) {
    os_event_set(log.writer_event);
  }
  if (log_flusher_is_active()) {
    os_event_set(log.flusher_event);
  }
  if (log_write_notifier_is_active()) {
    os_event_set(log.write_notifier_event);
  }
}

static void log_pause_writer_threads(log_t &log) {
  /* protected by LOCK_global_system_variables */
  if (!log.writer_threads_paused.load()) {
    os_event_reset(log.writer_threads_resume_event);
    log.writer_threads_paused.store(true);
    if (log_writer_is_active()) {
      os_event_set(log.writer_event);
    }
    if (log_flusher_is_active()) {
      os_event_set(log.flusher_event);
    }
    if (log_write_notifier_is_active()) {
      os_event_set(log.write_notifier_event);
    }
    if (log_flush_notifier_is_active()) {
      os_event_set(log.flush_notifier_event);
    }

    /* wakeup waiters to use the log writer threads */
    for (size_t i = 0; i < log.write_events_size; ++i) {
      os_event_set(log.write_events[i]);
    }
    for (size_t i = 0; i < log.flush_events_size; ++i) {
      os_event_set(log.flush_events[i]);
    }

    /* confirms *_notifier_thread accepted to pause */
    while (log.write_notifier_resume_lsn.load(std::memory_order_acquire) == 0 ||
           log.flush_notifier_resume_lsn.load(std::memory_order_acquire) == 0) {
      ut_a(log_write_notifier_is_active());
      ut_a(log_flush_notifier_is_active());
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

static void log_resume_writer_threads(log_t &log) {
  /* protected by LOCK_global_system_variables */
  if (log.writer_threads_paused.load()) {
    log.writer_threads_paused.store(false);

    /* wakeup waiters not to use the log writer threads */
    os_event_set(log.old_flush_event);

    /* gives resume lsn for each notifiers */
    log.write_notifier_resume_lsn.store(log.write_lsn.load());
    log.flush_notifier_resume_lsn.store(log.flushed_to_disk_lsn.load());
    os_event_set(log.writer_threads_resume_event);

    /* confirms *_notifier_resume_lsn have been accepted */
    while (log.write_notifier_resume_lsn.load(std::memory_order_acquire) != 0 ||
           log.flush_notifier_resume_lsn.load(std::memory_order_acquire) != 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      ut_a(log_write_notifier_is_active());
      ut_a(log_flush_notifier_is_active());
      os_event_set(log.writer_threads_resume_event);
    }
  }
}

void log_control_writer_threads(log_t &log) {
  /* pause/resume the log writer threads based on innodb_log_writer_threads
  value. NOTE: This function is protected by LOCK_global_system_variables
  in mysqld by called from innodb_log_writer_threads_update() */
  if (srv_log_writer_threads) {
    log_resume_writer_threads(log);
  } else {
    log_pause_writer_threads(log);
  }
}

/** @} */

/**************************************************/ /**

 @name  Status printing

 *******************************************************/

/** @{ */

void log_print(const log_t &log, FILE *file) {
  lsn_t last_checkpoint_lsn;
  lsn_t dirty_pages_added_up_to_lsn;
  lsn_t ready_for_write_lsn;
  lsn_t write_lsn;
  lsn_t flush_lsn;
  lsn_t max_assigned_lsn;
  lsn_t current_lsn;
  lsn_t oldest_lsn;
  uint64_t file_min_id;
  uint64_t file_max_id;

  log_files_mutex_enter(log);

  last_checkpoint_lsn = log.last_checkpoint_lsn.load();
  dirty_pages_added_up_to_lsn = log_buffer_dirty_pages_added_up_to_lsn(log);
  ready_for_write_lsn = log_buffer_ready_for_write_lsn(log);
  write_lsn = log.write_lsn.load();
  flush_lsn = log.flushed_to_disk_lsn.load();
  max_assigned_lsn = log_get_lsn(log);
  current_lsn = log_get_lsn(log);
  file_min_id = log.m_files.begin()->m_id;
  file_max_id = log.m_current_file.m_id;

  log_limits_mutex_enter(log);
  oldest_lsn = log.available_for_checkpoint_lsn;
  log_limits_mutex_exit(log);

  log_files_mutex_exit(log);

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
          "Last checkpoint at           " LSN_PF
          "\n"
          "Log minimum file id is       " UINT64PF
          "\n"
          "Log maximum file id is       " UINT64PF "\n",
          current_lsn, max_assigned_lsn, ready_for_write_lsn, write_lsn,
          flush_lsn, dirty_pages_added_up_to_lsn, oldest_lsn,
          last_checkpoint_lsn, file_min_id, file_max_id);

  time_t current_time = time(nullptr);

  double time_elapsed = difftime(current_time, log.last_printout_time);

  if (time_elapsed <= 0) {
    time_elapsed = 1;
  }

  fprintf(
      file, UINT64PF " log i/o's done, %.2f log i/o's/second\n", log.n_log_ios,
      static_cast<double>(log.n_log_ios - log.n_log_ios_old) / time_elapsed);

  log.n_log_ios_old = log.n_log_ios;
  log.last_printout_time = current_time;
}

void log_refresh_stats(log_t &log) {
  log.n_log_ios_old = log.n_log_ios;
  log.last_printout_time = time(nullptr);
}

void log_update_exported_variables(const log_t &log) {
  export_vars.innodb_redo_log_read_only = srv_read_only_mode;

  export_vars.innodb_redo_log_uuid = log.m_log_uuid;

  export_vars.innodb_redo_log_checkpoint_lsn = log_get_checkpoint_lsn(log);

  export_vars.innodb_redo_log_flushed_to_disk_lsn =
      log.flushed_to_disk_lsn.load();

  export_vars.innodb_redo_log_current_lsn = log_get_lsn(log);
}

/** @} */

/**************************************************/ /**

 @name  Resizing of buffers

 *******************************************************/

/** @{ */

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
    return false;
  }

  /* Save the contents. */
  byte *tmp_buf = ut::new_arr_withkey<byte>(UT_NEW_THIS_FILE_PSI_KEY,
                                            ut::Count(end_lsn - start_lsn));
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
  ut::delete_arr(tmp_buf);

  log_calc_buf_size(log);

  log_update_buf_limit(log);

  ut_a(srv_log_buffer_size == log.buf_size);

  ib::info(ER_IB_MSG_1260) << "srv_log_buffer_size was extended to "
                           << log.buf_size << ".";

  return true;
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

  return ret;
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
}

/** @} */

/**************************************************/ /**

 @name  Allocation / deallocation of buffers

 *******************************************************/

/** @{ */

static void log_allocate_buffer(log_t &log) {
  ut_a(srv_log_buffer_size >= INNODB_LOG_BUFFER_SIZE_MIN);
  ut_a(srv_log_buffer_size <= INNODB_LOG_BUFFER_SIZE_MAX);
  ut_a(srv_log_buffer_size >= 4 * UNIV_PAGE_SIZE);

#ifdef UNIV_PFS_MEMORY
  log.buf.alloc_withkey(ut::make_psi_memory_key(log_buffer_memory_key),
                        ut::Count{srv_log_buffer_size});
#else
  log.buf.alloc_withkey(ut::make_psi_memory_key(PSI_NOT_INSTRUMENTED),
                        ut::Count{srv_log_buffer_size});
#endif
}

static void log_deallocate_buffer(log_t &log) { log.buf.dealloc(); }

static void log_allocate_write_ahead_buffer(log_t &log) {
  ut_a(srv_log_write_ahead_size >= INNODB_LOG_WRITE_AHEAD_SIZE_MIN);
  ut_a(srv_log_write_ahead_size <= INNODB_LOG_WRITE_AHEAD_SIZE_MAX);

  log.write_ahead_buf_size = srv_log_write_ahead_size;
  log.write_ahead_buf.alloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                                    ut::Count{log.write_ahead_buf_size});
}

static void log_deallocate_write_ahead_buffer(log_t &log) {
  log.write_ahead_buf.dealloc();
}

static void log_allocate_flush_events(log_t &log) {
  const size_t n = srv_log_flush_events;

  ut_a(log.flush_events == nullptr);
  ut_a(n >= 1);
  ut_a((n & (n - 1)) == 0);

  log.flush_events_size = n;
  log.flush_events =
      ut::new_arr_withkey<os_event_t>(UT_NEW_THIS_FILE_PSI_KEY, ut::Count{n});

  for (size_t i = 0; i < log.flush_events_size; ++i) {
    log.flush_events[i] = os_event_create();
  }
}

static void log_deallocate_flush_events(log_t &log) {
  ut_a(log.flush_events != nullptr);

  for (size_t i = 0; i < log.flush_events_size; ++i) {
    os_event_destroy(log.flush_events[i]);
  }

  ut::delete_arr(log.flush_events);
  log.flush_events = nullptr;
}

static void log_allocate_write_events(log_t &log) {
  const size_t n = srv_log_write_events;

  ut_a(log.write_events == nullptr);
  ut_a(n >= 1);
  ut_a((n & (n - 1)) == 0);

  log.write_events_size = n;
  log.write_events =
      ut::new_arr_withkey<os_event_t>(UT_NEW_THIS_FILE_PSI_KEY, ut::Count{n});

  for (size_t i = 0; i < log.write_events_size; ++i) {
    log.write_events[i] = os_event_create();
  }
}

static void log_deallocate_write_events(log_t &log) {
  ut_a(log.write_events != nullptr);

  for (size_t i = 0; i < log.write_events_size; ++i) {
    os_event_destroy(log.write_events[i]);
  }

  ut::delete_arr(log.write_events);
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

static void log_reset_encryption_buffer(log_t &log) {
  std::memset(log.m_encryption_buf, 0x00, OS_FILE_LOG_BLOCK_SIZE);
}

/** @} */

/**************************************************/ /**

 @name  Log position locking (for replication)

 *******************************************************/

/** @{ */

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
  ut_ad(rw_lock_own(log.sn_lock_inst, RW_LOCK_X));
  ut_ad(log_checkpointer_mutex_own(log));

  *checkpoint_lsn = log.last_checkpoint_lsn.load();

  *current_lsn = log_get_lsn(log);

  /* Ensure we have redo log started. */
  ut_a(*current_lsn >= LOG_START_LSN);
  ut_a(*checkpoint_lsn >= LOG_START_LSN);

  /* Obviously current lsn cannot point to before checkpoint. */
  ut_a(*current_lsn >= *checkpoint_lsn);
}

/** @} */

/**************************************************/ /**

 @name Log - persisting flags

 *******************************************************/

/** @{ */

/** Updates and persists the updated log flags to disk.
@param[in,out]  log               redo log
@param[in]      update_function   functor called on existing flags,
                                  supposed to make changes */
template <typename T>
static void log_update_flags(log_t &log, T update_function) {
  ut_a(!srv_read_only_mode);

  log_writer_mutex_enter(log); /* writing to log files */
  log_files_mutex_enter(log);  /* accessing log.m_files */

  Log_flags log_flags = log.m_log_flags;
  update_function(log_flags);

  const dberr_t err = log_files_persist_flags(log, log_flags);
  ut_a(err == DB_SUCCESS);

  log_files_mutex_exit(log);
  log_writer_mutex_exit(log);
}

void log_persist_enable(log_t &log) {
  log_update_flags(log, [](Log_flags &log_flags) {
    log_file_header_reset_flag(log_flags, LOG_HEADER_FLAG_NO_LOGGING);
    log_file_header_reset_flag(log_flags, LOG_HEADER_FLAG_CRASH_UNSAFE);
  });
}

void log_persist_disable(log_t &log) {
  log_update_flags(log, [](Log_flags &log_flags) {
    log_file_header_set_flag(log_flags, LOG_HEADER_FLAG_NO_LOGGING);
    log_file_header_set_flag(log_flags, LOG_HEADER_FLAG_CRASH_UNSAFE);
  });
}

void log_persist_crash_safe(log_t &log) {
  log_update_flags(log, [](Log_flags &log_flags) {
    ut_a(log_file_header_check_flag(log_flags, LOG_HEADER_FLAG_NO_LOGGING));
    log_file_header_reset_flag(log_flags, LOG_HEADER_FLAG_CRASH_UNSAFE);
  });
}

void log_persist_initialized(log_t &log) {
  log_update_flags(log, [](Log_flags &log_flags) {
    constexpr auto flag_to_reset = LOG_HEADER_FLAG_NOT_INITIALIZED;
    ut_a(log_file_header_check_flag(log_flags, flag_to_reset));
    log_file_header_reset_flag(log_flags, flag_to_reset);
  });
}

void log_crash_safe_validate(log_t &log) {
  log_files_mutex_enter(log); /* accessing log.m_files */
  ut_a(!log_file_header_check_flag(log.m_log_flags,
                                   LOG_HEADER_FLAG_CRASH_UNSAFE));
  log_files_mutex_exit(log);
}

/** @} */

/**************************************************/ /**

 @name Log - log system initialization.

 *******************************************************/

/** @{ */

static dberr_t log_sys_handle_creator(log_t &log) {
  auto str_starts_with = [](const std::string &a, const std::string &b) {
    return a.size() >= b.size() && a.substr(0, b.size()) == b;
  };

  const auto &creator_name = log.m_creator_name;

  if (str_starts_with(creator_name, "MEB")) {
    /* Disable the double write buffer. MEB ensures that the data pages
    are consistent. Therefore the dblwr is superfluous. Secondly, the dblwr
    file is not redo logged and we can have pages in there that were written
    after the redo log was copied by MEB. */

    /* Restore state after recovery completes. */
    recv_sys->dblwr_state = dblwr::g_mode;
    dblwr::g_mode = dblwr::Mode::OFF;
    dblwr::set();

    recv_sys->is_meb_db = true;

    if (srv_read_only_mode) {
      ib::error(ER_IB_MSG_LOG_FILES_CREATED_BY_MEB_AND_READ_ONLY_MODE);
      return DB_ERROR;
    }

    /* This log file was created by mysqlbackup --restore: print
    a note to the user about it */
    ib::info(ER_IB_MSG_LOG_FILES_CREATED_BY_MEB, creator_name.c_str());

  } else if (str_starts_with(creator_name, LOG_HEADER_CREATOR_CLONE)) {
    recv_sys->is_cloned_db = true;
    /* Refuse clone database recovery in read only mode. */
    if (srv_read_only_mode) {
      ib::error(ER_IB_MSG_LOG_FILES_CREATED_BY_CLONE_AND_READ_ONLY_MODE);
      return DB_ERROR;
    }
    if (log.m_format < Log_format::CURRENT) {
      ib::error(ER_IB_MSG_LOG_UPGRADE_CLONED_DB, ulong{to_int(log.m_format)});
      return DB_ERROR;
    }
    ib::info(ER_IB_MSG_LOG_FILES_CREATED_BY_CLONE);

  } else if (!str_starts_with(creator_name, "MySQL ")) {
    ib::warn(ER_IB_MSG_LOG_FILES_CREATED_BY_UNKNOWN_CREATOR,
             creator_name.c_str());
  }
  return DB_SUCCESS;
}

static dberr_t log_sys_check_format(const log_t &log) {
  switch (log.m_format) {
    case Log_format::LEGACY:
      ib::error(ER_IB_MSG_LOG_FORMAT_BEFORE_5_7_9,
                ulong{to_int(Log_format::LEGACY)});
      return DB_ERROR;

    case Log_format::VERSION_5_7_9:
    case Log_format::VERSION_8_0_1:
    case Log_format::VERSION_8_0_3:
    case Log_format::VERSION_8_0_19:
    case Log_format::VERSION_8_0_28:
      ib::info(ER_IB_MSG_LOG_FORMAT_BEFORE_8_0_30, ulong{to_int(log.m_format)});
      break;

    case Log_format::CURRENT:
      break;

    default:
      /* The log_files_find_and_analyze() would return error if format
      was invalid, and InnoDB would quit in log_sys_init() before
      calling this function. */
      ut_error;
  }

  if (log.m_format < Log_format::CURRENT && srv_force_recovery != 0) {
    /* We say no to running with forced recovery and old format.
    User should rather use previous version of MySQL and recover
    properly before he switches to newer version. */
    const auto directory = log_directory_path(log.m_files_ctx);
    ib::error(ER_IB_MSG_LOG_UPGRADE_FORCED_RECV, ulong{to_int(log.m_format)});
    return DB_ERROR;
  }

  return DB_SUCCESS;
}

static dberr_t log_sys_check_directory(const Log_files_context &ctx,
                                       std::string &path, bool &found_files) {
  path = log_directory_path(ctx);
  if (!os_file_exists(path.c_str())) {
    return DB_NOT_FOUND;
  }
  ut::vector<Log_file_id> listed_files;
  const dberr_t err = log_list_existing_files(ctx, listed_files);
  if (err != DB_SUCCESS) {
    return DB_ERROR;
  }
  found_files = !listed_files.empty();
  return DB_SUCCESS;
}

dberr_t log_sys_init(bool expect_no_files, lsn_t flushed_lsn,
                     lsn_t &new_files_lsn) {
  ut_a(log_is_data_lsn(flushed_lsn));
  ut_a(log_sys == nullptr);

  new_files_lsn = 0;

  Log_files_context log_files_ctx{srv_log_group_home_dir,
                                  Log_files_ruleset::PRE_8_0_30};

  std::string root_path;
  bool found_files_in_root{false};
  dberr_t err =
      log_sys_check_directory(log_files_ctx, root_path, found_files_in_root);

  /* Report error if innodb_log_group_home_dir / datadir has not been found or
  could not be listed. It's a proper decision for all redo format versions:
    - older formats store there ib_logfile* files directly,
    - newer formats store there #innodb_redo subdirectory. */
  if (err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_LOG_INIT_DIR_LIST_FAILED, root_path.c_str());
    return err;
  }

  Log_file_handle::s_on_before_read = [](Log_file_id, Log_file_type file_type,
                                         os_offset_t, os_offset_t read_size) {
    ut_a(file_type == Log_file_type::NORMAL);
    ut_a(srv_is_being_started);
#ifndef UNIV_HOTBACKUP
    srv_stats.data_read.add(read_size);
#endif /* !UNIV_HOTBACKUP */
  };

  Log_file_handle::s_on_before_write =
      [](Log_file_id file_id, Log_file_type file_type, os_offset_t write_offset,
         os_offset_t write_size) {
        ut_a(!srv_read_only_mode);
        if (!srv_is_being_started) {
          ut_a(log_sys != nullptr);
          auto file = log_sys->m_files.file(file_id);
          if (file_type == Log_file_type::NORMAL) {
            ut_a(file != log_sys->m_files.end());
            ut_a((file_id == log_sys->m_current_file.m_id &&
                  write_offset + write_size <= file->m_size_in_bytes) ||
                 write_offset + write_size <= LOG_FILE_HDR_SIZE);
          } else {
            ut_a(file == log_sys->m_files.end());
            ut_a(file_id == log_sys->m_current_file.next_id());
          }
        }
#ifndef UNIV_HOTBACKUP
        srv_stats.data_written.add(write_size);
#endif
      };

#ifdef _WIN32
  Log_file_handle::s_skip_fsyncs =
      (srv_win_file_flush_method == SRV_WIN_IO_UNBUFFERED);
#else
  Log_file_handle::s_skip_fsyncs =
      (srv_unix_file_flush_method == SRV_UNIX_O_DSYNC ||
       srv_unix_file_flush_method == SRV_UNIX_NOSYNC);
#endif /* _WIN32 */

  if (!found_files_in_root) {
    log_files_ctx =
        Log_files_context{srv_log_group_home_dir, Log_files_ruleset::CURRENT};

    std::string subdir_path;
    bool found_files_in_subdir{false};
    err = log_sys_check_directory(log_files_ctx, subdir_path,
                                  found_files_in_subdir);

    switch (err) {
      case DB_SUCCESS:
        if (expect_no_files && found_files_in_subdir) {
          ib::error(ER_IB_MSG_LOG_INIT_DIR_NOT_EMPTY_WONT_INITIALIZE,
                    subdir_path.c_str());
          return DB_ERROR;
        }
        if (!srv_read_only_mode) {
          /* The problem is that a lot of people is not aware
          that sending SHUTDOWN command does not end when the
          server is no longer running, but earlier (obvious!).
          Starting MySQL without waiting on previous instance
          stopped, seems a bad idea and it often led to
          quick failures here if we did not retry. */
          for (size_t retries = 0;; ++retries) {
            const auto remove_unused_files_ret =
                log_remove_unused_files(log_files_ctx);
            if (remove_unused_files_ret.first == DB_SUCCESS) {
              break;
            }
            ut_a(retries < 300);
            std::this_thread::sleep_for(std::chrono::seconds(1));
          }
        }
        break;
      case DB_NOT_FOUND:
        /* The #innodb_redo directory has not been found. */
        if (expect_no_files) {
          /* InnoDB needs to create new directory #innodb_redo. */
          if (!os_file_create_directory(subdir_path.c_str(), false)) {
            return DB_ERROR;
          }
        } else {
          /* InnoDB does not start if neither ib_logfile* files were found,
          nor the #innodb_redo directory was found. User should be informed
          about the problem and decide to either:
            - use older version of MySQL (<= 8.0.29) and do a non-fast shutdown,
            - or create the missing #innodb_redo */
          ib::error(ER_IB_MSG_LOG_INIT_DIR_MISSING_SUBDIR, LOG_DIRECTORY_NAME,
                    log_pre_8_0_30::FILE_BASE_NAME, root_path.c_str());
          return DB_ERROR;
        }
        break;
      default:
        ib::error(ER_IB_MSG_LOG_INIT_DIR_LIST_FAILED, subdir_path.c_str());
        return err;
    }

  } else {
    /* Found existing files in old location for redo files (PRE_8_0_30).
    If expected to see no files (and create new), return error emitting
    the error message. */
    if (expect_no_files) {
      ib::error(ER_IB_MSG_LOG_INIT_DIR_NOT_EMPTY_WONT_INITIALIZE,
                root_path.c_str());
      return DB_ERROR;
    }
  }

  log_sys_create();
  ut_a(log_sys != nullptr);
  log_t &log = *log_sys;

  bool is_concurrency_margin_safe;
  log_concurrency_margin(
      Log_files_capacity::soft_logical_capacity_for_hard(
          Log_files_capacity::hard_logical_capacity_for_physical(
              srv_redo_log_capacity_used)),
      is_concurrency_margin_safe);

  if (!is_concurrency_margin_safe) {
    os_offset_t min_redo_log_capacity = srv_redo_log_capacity_used;
    os_offset_t max_redo_log_capacity = LOG_CAPACITY_MAX;
    while (min_redo_log_capacity < max_redo_log_capacity) {
      const os_offset_t capacity_to_check =
          (min_redo_log_capacity + max_redo_log_capacity) / 2;

      log_concurrency_margin(
          Log_files_capacity::soft_logical_capacity_for_hard(
              Log_files_capacity::hard_logical_capacity_for_physical(
                  capacity_to_check)),
          is_concurrency_margin_safe);

      if (is_concurrency_margin_safe) {
        max_redo_log_capacity = capacity_to_check;
      } else {
        min_redo_log_capacity = capacity_to_check + 1;
      }
    }

    /* The innodb_redo_log_capacity is always rounded to 1M */
    min_redo_log_capacity =
        ut_uint64_align_up(min_redo_log_capacity, 1024UL * 1024);

    ib::error(ER_IB_MSG_LOG_PARAMS_CONCURRENCY_MARGIN_UNSAFE,
              ulonglong{srv_redo_log_capacity_used / 1024 / 1024},
              ulong{srv_thread_concurrency},
              ulonglong{min_redo_log_capacity / 1024 / 1024},
              INNODB_PARAMETERS_MSG);

    return DB_ERROR;
  }

  log.m_files_ctx = std::move(log_files_ctx);

  if (expect_no_files) {
    ut_a(srv_force_recovery < SRV_FORCE_NO_LOG_REDO);
    ut_a(!srv_read_only_mode);

    ut_a(log.m_files_ctx.m_files_ruleset == Log_files_ruleset::CURRENT);

    new_files_lsn = flushed_lsn;
    return log_files_create(log, flushed_lsn);
  }

  if (srv_force_recovery >= SRV_FORCE_NO_LOG_REDO) {
    return DB_SUCCESS;
  }

  Log_files_dict files{log.m_files_ctx};
  Log_format format;
  std::string creator_name;
  Log_flags log_flags;
  Log_uuid log_uuid;

  ut_a(srv_force_recovery < SRV_FORCE_NO_LOG_REDO);

  auto res = log_files_find_and_analyze(
      srv_read_only_mode, log.m_encryption_metadata, files, format,
      creator_name, log_flags, log_uuid);
  switch (res) {
    case Log_files_find_result::FOUND_VALID_FILES:
      log.m_format = format;
      log.m_creator_name = creator_name;
      log.m_log_flags = log_flags;
      log.m_log_uuid = log_uuid;
      log.m_files = std::move(files);
      break;

    case Log_files_find_result::FOUND_UNINITIALIZED_FILES:
      ut_a(format == Log_format::CURRENT);
      [[fallthrough]];
    case Log_files_find_result::FOUND_NO_FILES:
      ut_a(log.m_files_ctx.m_files_ruleset == Log_files_ruleset::CURRENT);
      ut_a(files.empty());

      if (srv_read_only_mode) {
        ut_a(srv_force_recovery < SRV_FORCE_NO_LOG_REDO);
        ib::error(ER_IB_MSG_LOG_FILES_CREATE_AND_READ_ONLY_MODE);
        return DB_ERROR;
      }

      {
        const auto ret = log_remove_files(log.m_files_ctx);
        ut_a(ret.first == DB_SUCCESS);
      }
      new_files_lsn =
          flushed_lsn % OS_FILE_LOG_BLOCK_SIZE == LOG_BLOCK_HDR_SIZE
              ? flushed_lsn
              : ut_uint64_align_up(flushed_lsn, OS_FILE_LOG_BLOCK_SIZE) +
                    LOG_BLOCK_HDR_SIZE;
      return log_files_create(log, new_files_lsn);

    case Log_files_find_result::SYSTEM_ERROR:
    case Log_files_find_result::FOUND_CORRUPTED_FILES:
    case Log_files_find_result::FOUND_DISABLED_FILES:
    case Log_files_find_result::FOUND_VALID_FILES_BUT_MISSING_NEWEST:
      return DB_ERROR;
  }

  /* Check format of the redo log and emit information to the error log,
  if the format was not the newest one. */
  err = log_sys_check_format(log);
  if (err != DB_SUCCESS) {
    return err;
  }

  /* Check creator of log files and mark fields of recv_sys: is_cloned_db,
  is_meb_db if needed. */
  err = log_sys_handle_creator(log);
  if (err != DB_SUCCESS) {
    return err;
  }

  if (log_file_header_check_flag(log_flags, LOG_HEADER_FLAG_NO_LOGGING)) {
    auto result = mtr_t::s_logging.disable(nullptr);
    /* Currently never fails. */
    ut_a(result == 0);
    srv_redo_log = false;
  }

  return DB_SUCCESS;
}

void log_sys_close() {
  log_sys_free();
  ut_a(log_sys == nullptr);
}

/** @} */

#endif /* !UNIV_HOTBACKUP */
