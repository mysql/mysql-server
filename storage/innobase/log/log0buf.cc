/*****************************************************************************

Copyright (c) 1995, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************/ /**
 @file log/log0buf.cc

 Redo log buffer implementation, including functions to:

 -# Reserve space in the redo log buffer,

 -# Write to the reserved space in the log buffer,

 -# Add link to the log recent written buffer,

 -# Add link to the log recent closed buffer.

 *******************************************************/

#ifndef UNIV_HOTBACKUP

/* std::atomic_thread_fence */
#include <atomic>

/* std::memcpy */
#include <cstring>

/* Log_handle, ... */
#include "log0buf.h"

/* log_checkpointer_mutex */
#include "log0chkp.h"

/* log_translate_sn_to_lsn, ... */
#include "log0log.h"

/* recv_recovery_is_on(), ... */
#include "log0recv.h"

/* log_t::X */
#include "log0sys.h"

/* log_sync_point, log_test */
#include "log0test.h"

/* OS_FILE_LOG_BLOCK_SIZE, ... */
#include "log0types.h"

/* log_writer_mutex */
#include "log0write.h"

/* DBUG_EXECUTE_IF, ... */
#include "my_dbug.h"

/* srv_read_only_mode */
#include "srv0srv.h"

/* srv_shutdown_state */
#include "srv0start.h"

/* ut_uint64_align_down */
#include "ut0byte.h"

// clang-format off
/**************************************************/ /**
 @page PAGE_INNODB_REDO_LOG_BUF Redo log buffer

 When mtr commits, data has to be moved from internal buffer of the mtr
 to the redo log buffer. For a better concurrency, procedure for writing
 to the log buffer consists of following steps:
 -# @ref sect_redo_log_buf_reserve
 -# @ref sect_redo_log_buf_write
 -# @ref sect_redo_log_buf_add_links_to_recent_written

 Afterwards pages modified during the mtr, need to be added to flush lists.
 Because there is no longer a mutex protecting order in which dirty pages
 are added to flush lists, additional mechanism is required to ensure that
 lsn available for checkpoint is determined properly. Hence the procedure
 consists of following steps:
 -# @ref sect_redo_log_mark_dirty_pages
 -# @ref sect_redo_log_add_dirty_pages
 -# @ref sect_redo_log_add_link_to_recent_closed

 @section sect_redo_log_buf_reserve Reservation of space in the redo

 Range of lsn values is reserved for a provided number of data bytes.
 The reserved range will directly address space for the data in both
 the log buffer and the log files.

 Procedure used to reserve the range of lsn values:

 -# Acquiring shared access to the redo log (through sharded rw_lock)

 -# Increase the global number of reserved data bytes (@ref subsect_redo_log_sn)
    by number of data bytes we need to write.

    This is performed by an atomic fetch_add operation:

         start_sn = log.sn.fetch_add(len)
         end_sn = start_sn + len

    where _len_ is number of data bytes we need to write.

    Then range of sn values is translated to range of lsn values:

         start_lsn = log_translate_sn_to_lsn(start_sn)
         end_lsn = log_translate_sn_to_lsn(end_sn)

    The required translations are performed by simple calculations, because:

         lsn = sn / LOG_BLOCK_DATA_SIZE * OS_FILE_LOG_BLOCK_SIZE
                 + sn % LOG_BLOCK_DATA_SIZE
                 + LOG_BLOCK_HDR_SIZE

 -# Wait until the reserved range corresponds to free space in the log buffer.

   In this step we could be forced to wait for the
   [log writer thread](@ref sect_redo_log_writer),
   which reclaims space in the log buffer by writing data to system buffers.

   The user thread waits until the reserved range of lsn values maps to free
   space in the log buffer, which is true when:

         end_lsn - log.write_lsn <= log.buf_size

   @remarks
   The waiting is performed by a call to log_write_up_to(end_lsn -
 log.buf_size), which has a loop with short sleeps. We assume that it is
 unlikely that the waiting is actually needed. The
 _MONITOR_LOG_ON_BUFFER_SPACE_* counters track number of iterations spent in the
 waiting loop. If they are not nearby 0, DBA should try to increase the size of
 the log buffer.

   @note The log writer thread could be waiting on the write syscall, but it
   also could be waiting for other user threads, which need to complete writing
   their data to the log buffer for smaller sn values! Hopefully these user
   threads have not been scheduled out. If we controlled scheduling (e.g. if
   we have used fiber-based approach), we could avoid such problem.

 -# Wait until the reserved range corresponds to free space in the log files.

   In this step we could be forced to wait for page cleaner threads or the
   [log checkpointer thread](@ref sect_redo_log_checkpointer) until it made
   a next checkpoint.

   The user thread waits until the reserved range of lsn values maps to free
   space in the log files, which is true when:

         end_lsn - log.last_checkpoint_lsn <= redo lsn capacity

   @remarks
   The waiting is performed by a loop with progressive sleeps.
   The _MONITOR_LOG_ON_FILE_SPACE_* counters track number of iterations
   spent in the waiting loop. If they are not nearby 0, DBA should try to
   use more page cleaner threads, increase size of the log files or ask for
   better storage device.

   This mechanism could lead to a __deadlock__, because the user thread waiting
   during commit of mtr, keeps the dirty pages locked, which makes it impossible
   to flush them. Now, if these pages have very old modifications, it could be
   impossible to move checkpoint further without flushing them. In such case the
   log checkpointer thread will be unable to
   [reclaim space in the log files](@ref sect_redo_log_reclaim_space).

   To avoid such problems, user threads call log_free_check() from time to time,
   when they don't keep any latches. They try to do it at least every 4 modified
   pages and if they detected that there is not much free space in the log
 files, they wait until the free space is reclaimed (but without holding
 latches!).

   @note Note that multiple user threads could check the free space without
   holding latches and then proceed with writes. Therefore this mechanism only
   works because the minimum required free space is based on assumptions:
     - maximum number of such concurrent user threads is limited,
     - maximum size of write between two checks within a thread is limited.

   This mechanism does not provide safety when concurrency is not limited!
   In such case we only do the best effort but the deadlock is still possible
   in theory.

 @see log_buffer_reserve()


 @section sect_redo_log_buf_write Copying data to the reserved space

 After a range of lsn values has been reserved, the data is copied to the log
 buffer's fragment related to the range of lsn values.

 The log buffer is a ring buffer, directly addressed by lsn values, which means
 that there is no need for shifting of data in the log buffer. Byte for a given
 lsn is stored at lsn modulo size of the buffer. It is then easier to reach
 higher concurrency with such the log buffer, because shifting would require
 an exclusive access.

 @note However, when writing the wrapped fragment of the log buffer to disk,
 extra IO operation could happen (because we need to copy two disjoint areas
 of memory). First of all, it's a rare case so it shouldn't matter at all.
 Also note that the wrapped fragment results only in additional write to
 system buffer, so still number of real IO operations could stay the same.

 Writes to different ranges of lsn values happen concurrently without any
 synchronization. Each user thread writes its own sequence of log records
 to the log buffer, copying them from the internal buffer of the mtr, leaving
 holes for headers and footers of consecutive log blocks.

 @note There is some hidden synchronization when multiple user threads write to
 the same memory cache line. That happens when they write to the same 64 bytes,
 because they have reserved small consecutive ranges of lsn values. Fortunately
 each mtr takes in average more than few bytes, which limits number of such user
 threads that meet within a cache line.

 When mtr_commit() finishes writing the group of log records, it is responsible
 for updating the _first_rec_group_ field in the header of the block to which
 _end_lsn_ belongs, unless it is the same block to which _start_lsn_ belongs
 (in which case user ending at _start_lsn_ is responsible for the update).

 @see log_buffer_write()


 @section sect_redo_log_buf_add_links_to_recent_written Adding links to the
 recent written buffer

 Fragment of the log buffer, which is close to current lsn, is very likely being
 written concurrently by multiple user threads. There is no restriction on order
 in which such concurrent writes might be finished. Each user thread which has
 finished writing, proceeds further without waiting for any other user threads.

 @diafile storage/innobase/log/user_thread_writes_to_buffer.dia "One of many concurrent writes"

 @note Note that when a user thread has finished writing, still some other user
 threads could be writing their data for smaller lsn values. It is still fine,
 because it is the [log writer thread](@ref sect_redo_log_writer) that needs to
 ensure, that it writes only complete fragments of the log buffer. For that we
 need information about the finished writes.

 The log recent written buffer is responsible for tracking which of concurrent
 writes to the log buffer, have been finished. It allows the log writer thread
 to update @ref subsect_redo_log_buf_ready_for_write_lsn, which allows to find
 the next complete fragment of the log buffer to write. It is enough to track
 only recent writes, because we know that up to _log.buf_ready_for_write_lsn_,
 all writes have been finished. Hence this lsn value defines the beginning of
 lsn range represented by the recent written buffer in a given time. The recent
 written buffer is a ring buffer, directly addressed by lsn value. When there
 is no space in the buffer, user thread needs to wait.

 @note Size of the log recent written buffer is limited, so concurrency might
 be limited if the recent written buffer is too small and user threads start
 to wait for each other then (indirectly by waiting for the space reclaimed
 in the recent written buffer by the log writer thread).

 Let us describe the procedure used for adding the links.

 Suppose, user thread has just written some of mtr's log records to a range
 of lsn values _tmp_start_lsn_ .. _tmp_end_lsn_, then:

 -# User thread waits for free space in the recent written buffer, until:

         tmp_end_lsn - log.buf_ready_for_write_lsn <= S

    where _S_ is a number of slots in the log recent_written buffer.

 -# User thread adds the link by setting value of slot for _tmp_start_lsn_:

         log.recent_written[tmp_start_lsn % S] = tmp_end_lsn

    The value gives information about how much to advance lsn when traversing
    the link.

    @note Note that possibly _tmp_end_lsn_ < _end_lsn_. In such case, next write
    of log records in the mtr will start at _tmp_end_lsn_. After all the log
    records are written, the _tmp_end_lsn_ should become equal to the _end_lsn_
    of the reservation (we must not reserve more bytes than we write).

 The log writer thread follows path created by the added links, updates
 @ref subsect_redo_log_buf_ready_for_write_lsn and clears the links, allowing
 to reuse them (for lsn larger by _S_).

 Before the link is added, release barrier is required, to avoid compile time
 or memory reordering of writes to the log buffer and the recent written buffer.
 It is extremely important to ensure, that write to the log buffer will precede
 write to the recent written buffer.

 The same will apply to reads in the log writer thread, so then the log writer
 thread will be sure, that after reading the link from the recent written buffer
 it will read the proper data from the log buffer's fragment related to the
 link.

 Copying data and adding links is performed in loop for consecutive log records
 within the group of log records in the mtr.

 @note Note that until some user thread finished writing all the log records,
 any log records which have been written to the log buffer for larger lsn
 (by other user threads), cannot be written to disk. The log writer thread
 will stop at the missing links in the log recent written buffer and wait.
 It follows connected links only.

 @see log_buffer_write_completed()


 @section sect_redo_log_mark_dirty_pages Marking pages as dirty

 Range of lsn values _start_lsn_ .. _end_lsn_, acquired during the reservation
 of space, represents the whole group of log records. It is used to mark all
 the pages in the mtr as dirty.

 @note During recovery the whole mtr needs to be recovered or skipped at all.
 Hence we don't need more detailed ranges of lsn values when marking pages.

 Each page modified in the mtr is locked and its _oldest_modification_ is
 checked to see if this is the first modification or the page had already been
 modified when its modification in this mtr started.

 Page, which was modified the first time, will have updated:
   - _oldest_modification_ = _start_lsn_,
   - _newest_modification_ = _end_lsn_,

 and will be added to the flush list for corresponding buffer pool (buffer pools
 are sharded by page_id).

 For other pages, only _newest_modification_ field is updated (with _end_lsn_).

 @note Note that some pages could already be modified earlier (in a previous
 mtr) and still unflushed. Such pages would have _oldest_modification_ != 0
 during this phase and they would belong already to flush lists. Hence it is
 enough to update their _newest_modification_.


 @section sect_redo_log_add_dirty_pages Adding dirty pages to flush lists

 After writes of all log records in a mtr_commit() have been finished, dirty
 pages have to be moved to flush lists. Hopefully, after some time the pages
 will become flushed and space in the log files could be reclaimed.

 The procedure for adding pages to flush lists:

 -# Wait for the recent closed buffer covering _end_lsn_.

    Before moving the pages, user thread waits until there is free space for
    a link pointing from _start_lsn_ to _end_lsn_ in the recent closed buffer.
    The free space is available when:

         end_lsn - log.buf_dirty_pages_added_up_to_lsn < L

    where _L_ is a number of slots in the log recent closed buffer.

    This way we have guarantee, that the maximum delay in flush lists is limited
    by _L_. That's because we disallow adding dirty page with too high lsn value
    until pages with smaller lsn values (smaller by more than _L_), have been
    added!

 -# Add the dirty pages to corresponding flush lists.

    During this step pages are locked and marked as dirty as described in
    @ref sect_redo_log_mark_dirty_pages.

    Multiple user threads could perform this step in any order of them.
    Hence order of dirty pages in a flush list, is not the same as order by
    their oldest modification lsn.

    @diafile storage/innobase/log/relaxed_order_of_dirty_pages.dia "Relaxed order of dirty pages"

 @note Note that still the @ref subsect_redo_log_buf_dirty_pages_added_up_to_lsn
 cannot be advanced further than to _start_lsn_. That's because the link from
 _start_lsn_ to _end_lsn_, has still not been added at this stage.

 @see log_buffer_write_completed_before_dirty_pages_added()


 @section sect_redo_log_add_link_to_recent_closed Adding link to the log recent
 closed buffer

 After all the dirty pages have been added to flush lists, a link pointing from
 _start_lsn_ to _end_lsn_ is added to the log recent closed buffer.

 This is performed by user thread, by setting value of slot for start_lsn:

         log.recent_closed[start_lsn % L] = end_lsn

 where _L_ is size of the log recent closed buffer. The value gives information
 about how much to advance lsn when traversing the link.

 @see log_buffer_write_completed_and_dirty_pages_added()

 After the link is added, the shared-access for log buffer is released.
 This possibly allows any thread waiting for an exclussive access to proceed.


 @section sect_redo_log_reclaim_space Reclaiming space in redo log

 Recall that recovery always starts at the last written checkpoint lsn.
 Therefore @ref subsect_redo_log_last_checkpoint_lsn defines the beginning of
 the log files. Because size of the log files is fixed, it is easy to determine
 if a given range of lsn values corresponds to free space in the log files or
 not (in which case it would overwrite tail of the redo log for smaller lsns).

 Space in the log files is reclaimed by writing a checkpoint for a higher lsn.
 This could be possible when more dirty pages have been flushed. The checkpoint
 cannot be written for higher lsn than the _oldest_modification_ of any of the
 dirty pages (otherwise we would have lost modifications for that page in case
 of crash). It is [log checkpointer thread](@ref sect_redo_log_checkpointer),
 which calculates safe lsn for a next checkpoint
 (@ref subsect_redo_log_available_for_checkpoint_lsn) and writes the checkpoint.
 User threads doing writes to the log buffer, no longer hold mutex, which would
 disallow to determine such lsn and write checkpoint meanwhile.

 Suppose user thread has just finished writing to the log buffer, and it is just
 before adding the corresponding dirty pages to flush lists, but suddenly became
 scheduled out. Now, the log checkpointer thread comes in and tries to determine
 lsn available for a next checkpoint. If we allowed the thread to take minimum
 _oldest_modification_ of dirty pages in flush lists and write checkpoint at
 that lsn value, we would logically erase all log records for smaller lsn
 values. However the dirty pages, which the user thread was trying to add to
 flush lists, could have smaller value of _oldest_modification_. Then log
 records protecting the modifications would be logically erased and in case of
 crash we would not be able to recover the pages.

 That's why we need to protect from doing checkpoint at such lsn value, which
 would logically erase the just written data to the redo log, until the related
 dirty pages have been added to flush lists.

 When user thread has added all the dirty pages related to _start_lsn_ ..
 _end_lsn_, it creates link in the log recent closed buffer, pointing from
 _start_lsn_ to _end_lsn_. The log closer thread tracks the links in the recent
 closed buffer, clears the slots (so they could be safely reused) and updates
 the @ref subsect_redo_log_buf_dirty_pages_added_up_to_lsn, reclaiming space
 in the recent closed buffer and potentially allowing to advance checkpoint
 further.

 Order of pages added to flush lists became relaxed so we also cannot rely
 directly on the lsn of the earliest added page to a given flush list.
 It is not guaranteed that it has the minimum _oldest_modification_ anymore.
 However it is guaranteed that it has _oldest_modification_ not higher than
 the minimum by more than _L_. Hence we subtract _L_ from its value and use
 that as lsn available for checkpoint according to the given flush list.
 For more details
 [read about adding dirty pages](@ref sect_redo_log_add_dirty_pages).

 @note Note there are two reasons for which lsn available for checkpoint could
 be updated:
   - because @ref subsect_redo_log_buf_dirty_pages_added_up_to_lsn was updated,
   - because the earliest added dirty page in one of flush lists became flushed.

 *******************************************************/
// clang-format on

/** Waits until there is free space in log buffer up to reserved handle.end_sn.
If there was no space, it basically waits for log writer thread which copies
data from log buffer to log files and advances log.write_lsn, reclaiming space
in the log buffer (it's a ring buffer).

There is a special case - if it turned out, that log buffer is too small for
the reserved range of lsn values, it resizes the log buffer.

It's used during reservation of lsn values, when the reserved handle.end_sn is
greater than log.buf_limit_sn.

@param[in,out]  log             redo log
@param[in]      handle          handle for the reservation */
static void log_wait_for_space_after_reserving(log_t &log,
                                               const Log_handle &handle);

/** Waits until there is free space in the log buffer. The free space has
to be available for range of sn values ending at the provided sn.
@param[in]     log     redo log
@param[in]     end_sn  end of the range of sn values */
static void log_wait_for_space_in_log_buf(log_t &log, sn_t end_sn);

/**************************************************/ /**

 @name Locking for the redo log

 *******************************************************/

/** @{ */

/** Waits for the start_sn unlocked and allowed to write to the buffer.
@param[in,out] log       redo log
@param[in]     start_sn  target sn value to start to write */
static inline void log_buffer_s_lock_wait(log_t &log, const sn_t start_sn) {
  int64_t signal_count = 0;
  uint32_t i = 0;

  if (log.sn_locked.load(std::memory_order_acquire) <= start_sn) {
    do {
      if (srv_spin_wait_delay) {
        ut_delay(ut::random_from_interval(0, srv_spin_wait_delay));
      }
      if (i < srv_n_spin_wait_rounds) {
        i++;
      } else {
        signal_count = os_event_reset(log.sn_lock_event);
        if ((log.sn.load(std::memory_order_acquire) & SN_LOCKED) == 0 ||
            log.sn_locked.load(std::memory_order_acquire) > start_sn) {
          break;
        }
        os_event_wait_time_low(log.sn_lock_event, std::chrono::seconds{1},
                               signal_count);
      }
    } while ((log.sn.load(std::memory_order_acquire) & SN_LOCKED) != 0 &&
             log.sn_locked.load(std::memory_order_acquire) <= start_sn);
  }
}

/** Acquires the log buffer s-lock.
And reserve space in the log buffer.
The corresponding unlock operation is adding link to log.recent_closed.
@param[in,out] log     redo log
@param[in]     len     number of data bytes to reserve for write
@return start sn of reserved */
static inline sn_t log_buffer_s_lock_enter_reserve(log_t &log, size_t len) {
#ifdef UNIV_PFS_RWLOCK
  PSI_rwlock_locker *locker = nullptr;
  PSI_rwlock_locker_state state;
  if (log.pfs_psi != nullptr) {
    if (log.pfs_psi->m_enabled) {
      /* Instrumented to inform we are acquiring a shared rwlock */
      locker = PSI_RWLOCK_CALL(start_rwlock_rdwait)(
          &state, log.pfs_psi, PSI_RWLOCK_SHAREDLOCK, __FILE__,
          static_cast<uint>(__LINE__));
    }
  }
#endif /* UNIV_PFS_RWLOCK */

  /* Reserve space in sequence of data bytes: */
  sn_t start_sn = log.sn.fetch_add(len);
  if (UNIV_UNLIKELY((start_sn & SN_LOCKED) != 0)) {
    start_sn &= ~SN_LOCKED;
    /* log.sn is locked. Should wait for unlocked. */
    log_buffer_s_lock_wait(log, start_sn);
  }

  ut_d(
      rw_lock_add_debug_info(log.sn_lock_inst, 0, RW_LOCK_S, UT_LOCATION_HERE));
#ifdef UNIV_PFS_RWLOCK
  if (locker != nullptr) {
    PSI_RWLOCK_CALL(end_rwlock_rdwait)(locker, 0);
  }
#endif /* UNIV_PFS_RWLOCK */

  return start_sn;
}

/** Releases the log buffer s-lock.
@param[in,out] log       redo log
@param[in]     start_lsn start lsn of the reservation
@param[in]     end_lsn   end lsn of the reservation */
static inline void log_buffer_s_lock_exit_close(log_t &log, lsn_t start_lsn,
                                                lsn_t end_lsn) {
#ifdef UNIV_PFS_RWLOCK
  if (log.pfs_psi != nullptr) {
    if (log.pfs_psi->m_enabled) {
      /* Inform performance schema we are unlocking the lock */
      PSI_RWLOCK_CALL(unlock_rwlock)
      (log.pfs_psi, PSI_RWLOCK_SHAREDUNLOCK);
    }
  }
#endif /* UNIV_PFS_RWLOCK */
  ut_d(rw_lock_remove_debug_info(log.sn_lock_inst, 0, RW_LOCK_S));

  log.recent_closed.add_link_advance_tail(start_lsn, end_lsn);
}

void log_buffer_x_lock_enter(log_t &log) {
  log_sync_point("log_buffer_x_lock_enter_before_lock");

#ifdef UNIV_PFS_RWLOCK
  PSI_rwlock_locker *locker = nullptr;
  PSI_rwlock_locker_state state;
  if (log.pfs_psi != nullptr) {
    if (log.pfs_psi->m_enabled) {
      /* Record the acquisition of a read-write lock in exclusive
      mode in performance schema */
      locker = PSI_RWLOCK_CALL(start_rwlock_wrwait)(
          &state, log.pfs_psi, PSI_RWLOCK_EXCLUSIVELOCK, __FILE__,
          static_cast<uint>(__LINE__));
    }
  }
#endif /* UNIV_PFS_RWLOCK */

  /* locks log.sn_locked value */
  mutex_enter(&(log.sn_x_lock_mutex));

  /* locks log.sn value */
  sn_t sn = log.sn.load(std::memory_order_acquire);
  sn_t sn_locked;
  do {
    ut_ad((sn & SN_LOCKED) == 0);
    sn_locked = sn | SN_LOCKED;
    /* needs to update log.sn_locked before log.sn */
    /* Indicates x-locked sn value */
    log.sn_locked.store(sn, std::memory_order_relaxed);
  } while (
      !log.sn.compare_exchange_weak(sn, sn_locked, std::memory_order_acq_rel));

  /* Some s-lockers might wait for the new log.sn_locked value. */
  os_event_set(log.sn_lock_event);

  if (sn > 0) {
    /* redo log system has been started */
    const lsn_t current_lsn = log_translate_sn_to_lsn(sn);
    lsn_t closed_lsn = log_buffer_dirty_pages_added_up_to_lsn(log);
    uint32_t i = 0;
    /* must wait for closed_lsn == current_lsn */
    while (i < srv_n_spin_wait_rounds && closed_lsn < current_lsn) {
      if (srv_spin_wait_delay) {
        ut_delay(ut::random_from_interval(0, srv_spin_wait_delay));
      }
      i++;
      closed_lsn = log_buffer_dirty_pages_added_up_to_lsn(log);
    }
    if (closed_lsn < current_lsn) {
      log.recent_closed.advance_tail();
      closed_lsn = log_buffer_dirty_pages_added_up_to_lsn(log);
    }
    if (closed_lsn < current_lsn) {
      std::this_thread::yield();
      closed_lsn = log_buffer_dirty_pages_added_up_to_lsn(log);
    }
    while (closed_lsn < current_lsn) {
      std::this_thread::sleep_for(std::chrono::microseconds(20));
      log.recent_closed.advance_tail();
      closed_lsn = log_buffer_dirty_pages_added_up_to_lsn(log);
    }
  }

  ut_d(
      rw_lock_add_debug_info(log.sn_lock_inst, 0, RW_LOCK_X, UT_LOCATION_HERE));
#ifdef UNIV_PFS_RWLOCK
  if (locker != nullptr) {
    PSI_RWLOCK_CALL(end_rwlock_wrwait)(locker, 0);
  }
#endif /* UNIV_PFS_RWLOCK */

  log_sync_point("log_buffer_x_lock_enter_after_lock");
}

void log_buffer_x_lock_exit(log_t &log) {
  log_sync_point("log_buffer_x_lock_exit_before_unlock");

#ifdef UNIV_PFS_RWLOCK
  if (log.pfs_psi != nullptr) {
    if (log.pfs_psi->m_enabled) {
      /* Inform performance schema we are unlocking the lock */
      PSI_RWLOCK_CALL(unlock_rwlock)
      (log.pfs_psi, PSI_RWLOCK_EXCLUSIVEUNLOCK);
    }
  }
#endif /* UNIV_PFS_RWLOCK */
  ut_d(rw_lock_remove_debug_info(log.sn_lock_inst, 0, RW_LOCK_X));

  /* unlocks log.sn */
  sn_t sn = log.sn.load(std::memory_order_acquire);
  ut_a((sn & SN_LOCKED) != 0);
  sn_t sn_unlocked;
  do {
    sn_unlocked = sn & ~SN_LOCKED;
    log.sn_locked.store(sn_unlocked, std::memory_order_relaxed);
  } while (!log.sn.compare_exchange_weak(sn, sn_unlocked,
                                         std::memory_order_acq_rel));
  os_event_set(log.sn_lock_event);

  /* unlocks log.sn_locked */
  mutex_exit(&(log.sn_x_lock_mutex));

  log_sync_point("log_buffer_x_lock_exit_after_unlock");
}

/** @} */

/**************************************************/ /**

 @name Reservation of space in the redo log

 *******************************************************/

/** @{ */

static void log_wait_for_space_after_reserving(log_t &log,
                                               const Log_handle &handle) {
  ut_ad(rw_lock_own(log.sn_lock_inst, RW_LOCK_S));

  const sn_t start_sn = log_translate_lsn_to_sn(handle.start_lsn);

  const sn_t end_sn = log_translate_lsn_to_sn(handle.end_lsn);

  const sn_t len = end_sn - start_sn;

  /* If we had not allowed to resize log buffer, it would have
  been sufficient here to simply call:
          - log_wait_for_space_in_log_buf(log, end_sn).

  However we do allow, and we need to handle the possible race
  condition, when user tries to set very small log buffer size
  and other threads try to write large groups of log records.

  Note that since this point, log.buf_size_sn may only be
  increased from our point of view. That's because:

          1. Other threads doing mtr_commit will only try to
             increase the size (if needed).

          2. If user wanted to manually resize the log buffer,
             he needs to obtain x-lock for the redo log, but
             we keep s-lock. */

  log_wait_for_space_in_log_buf(log, start_sn);

  /* Now start_sn fits the log buffer or is at the boundary.
  Therefore all previous reservations (for smaller LSN), fit
  the log buffer [1].

  We check if len > log.buf_size_sn. If that's the case, our
  range start_sn..end_sn will cover more than size of the log
  buffer and we need to extend the size. Note that users that
  reserved smaller LSN will not try to extend because of [1].
  Users that reserved larger LSN, will not have their start_sn
  in the log buffer, because our end_sn already does not fit.
  Such users will first wait to reach invariant [1]. */

  log_sync_point("log_wfs_after_reserving_before_buf_size_1");

  if (len > log.buf_size_sn.load()) {
    DBUG_EXECUTE_IF("ib_log_buffer_is_short_crash", DBUG_SUICIDE(););

    log_write_up_to(log, log_translate_sn_to_lsn(start_sn), false);

    /* Now the whole log has been written to disk up to start_sn,
    so there are no pending writes to log buffer for smaller sn. */
    log_sync_point("log_wfs_after_reserving_before_buf_size_2");

    /* Reservations for larger LSN could not increase size of log
    buffer as they could not have reached [1], because end_sn did
    not fit the log buffer (end_sn - start_sn > buf_size_sn), and
    next reservations would have their start_sn even greater. */
    ut_a(len > log.buf_size_sn.load());

    /* Note that the log.write_lsn could not be changed since it
    reached start_sn, until current thread continues and finishes
    writing its data to the log buffer.

    Note that any other thread will not attempt to write
    concurrently to the log buffer, because the log buffer
    represents range of sn:
            [start_sn, start_sn + log.buf_size_sn)
    and it holds:
            end_sn > start_sn + log_buf_size_sn.
    This will not change until we finished resizing log
    buffer and updated log.buf_size_sn, which therefore
    must happen at the very end of the resize procedure. */
    ut_a(log_translate_lsn_to_sn(log.write_lsn.load()) == start_sn);

    ib::info(ER_IB_MSG_1231)
        << "The transaction log size is too large"
        << " for srv_log_buffer_size (" << len << " > "
        << log.buf_size_sn.load() << "). Trying to extend it.";

    /* Resize without extra locking required.

    We cannot call log_buffer_resize() because it would try
    to acquire x-lock for the redo log and we keep s-lock.

    We already have ensured, that there are no possible
    concurrent writes to the log buffer. Note, we have also
    ensured that log writer finished writing up to start_sn.

    However, for extra safety, we prefer to acquire writer_mutex,
    and checkpointer_mutex. We consider this rare event. */

    log_checkpointer_mutex_enter(log);
    log_writer_mutex_enter(log);

    /* We multiply size at least by 1.382 to avoid case
    in which we keep resizing by few bytes only. */

    lsn_t new_lsn_size = log_translate_sn_to_lsn(
        static_cast<sn_t>(1.382 * len + OS_FILE_LOG_BLOCK_SIZE));

    new_lsn_size = ut_uint64_align_up(new_lsn_size, OS_FILE_LOG_BLOCK_SIZE);

    log_buffer_resize_low(log, new_lsn_size, handle.start_lsn);

    log_writer_mutex_exit(log);
    log_checkpointer_mutex_exit(log);

  } else {
    /* Note that the size cannot get decreased.
    We are safe to continue. */
  }

  ut_a(len <= log.buf_size_sn.load());

  log_wait_for_space_in_log_buf(log, end_sn);
}

void log_update_buf_limit(log_t &log) {
  log_update_buf_limit(log, log.write_lsn.load());
}

void log_update_buf_limit(log_t &log, lsn_t write_lsn) {
  ut_ad(write_lsn <= log.write_lsn.load());

  const sn_t limit_for_end = log_translate_lsn_to_sn(write_lsn) +
                             log.buf_size_sn.load() -
                             2 * OS_FILE_LOG_BLOCK_SIZE;

  log.buf_limit_sn.store(limit_for_end);
}

static void log_wait_for_space_in_log_buf(log_t &log, sn_t end_sn) {
  lsn_t lsn;
  Wait_stats wait_stats;

  const sn_t write_sn = log_translate_lsn_to_sn(log.write_lsn.load());

  log_sync_point("log_wait_for_space_in_buf_middle");

  const sn_t buf_size_sn = log.buf_size_sn.load();

  if (end_sn + OS_FILE_LOG_BLOCK_SIZE <= write_sn + buf_size_sn) {
    return;
  }

  /* We preserve this counter for backward compatibility with 5.7. */
  srv_stats.log_waits.inc();

  lsn = log_translate_sn_to_lsn(end_sn + OS_FILE_LOG_BLOCK_SIZE - buf_size_sn);

  wait_stats = log_write_up_to(log, lsn, false);

  MONITOR_INC_WAIT_STATS(MONITOR_LOG_ON_BUFFER_SPACE_, wait_stats);

  ut_a(end_sn + OS_FILE_LOG_BLOCK_SIZE <=
       log_translate_lsn_to_sn(log.write_lsn.load()) + buf_size_sn);
}

Log_handle log_buffer_reserve(log_t &log, size_t len) {
  Log_handle handle;

  /* In 5.7, we incremented log_write_requests for each single
  write to log buffer in commit of mini-transaction.

  However, writes which were solved by log_reserve_and_write_fast
  missed to increment the counter. Therefore it wasn't reliable.

  Dimitri and I have decided to change meaning of the counter
  to reflect mtr commit rate. */
  srv_stats.log_write_requests.inc();

  ut_ad(srv_shutdown_state_matches([](auto state) {
    return state <= SRV_SHUTDOWN_FLUSH_PHASE ||
           state == SRV_SHUTDOWN_EXIT_THREADS;
  }));

  ut_a(len > 0);

  /* Reserve space in sequence of data bytes: */
  const sn_t start_sn = log_buffer_s_lock_enter_reserve(log, len);

  /* Ensure that redo log has been initialized properly. */
  ut_a(start_sn > 0);

#ifdef UNIV_DEBUG
  if (!recv_recovery_is_on()) {
    log_background_threads_active_validate(log);
  } else {
    ut_a(!recv_no_ibuf_operations);
  }
#endif

  /* Headers in redo blocks are not calculated to sn values: */
  const sn_t end_sn = start_sn + len;

  log_sync_point("log_buffer_reserve_before_buf_limit_sn");

  /* Translate sn to lsn (which includes also headers in redo blocks): */
  handle.start_lsn = log_translate_sn_to_lsn(start_sn);
  handle.end_lsn = log_translate_sn_to_lsn(end_sn);

  if (unlikely(end_sn > log.buf_limit_sn.load())) {
    log_wait_for_space_after_reserving(log, handle);
  }

  ut_a(log_is_data_lsn(handle.start_lsn));
  ut_a(log_is_data_lsn(handle.end_lsn));

  return handle;
}

/** @} */

/**************************************************/ /**

 @name Writing to the redo log buffer

 *******************************************************/

/** @{ */

lsn_t log_buffer_write(log_t &log, const byte *str, size_t str_len,
                       lsn_t start_lsn) {
  ut_ad(rw_lock_own(log.sn_lock_inst, RW_LOCK_S));

  ut_a(log.buf != nullptr);
  ut_a(log.buf_size > 0);
  ut_a(log.buf_size % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_a(str != nullptr);
  ut_a(str_len > 0);

  /* We should first resize the log buffer, if str_len is that big. */
  ut_a(str_len < log.buf_size_sn.load());

  /* The start_lsn points a data byte (not a header of log block). */
  ut_a(log_is_data_lsn(start_lsn));

  /* We neither write with holes, nor overwrite any fragments of data. */
  ut_ad(log.write_lsn.load() <= start_lsn);
  ut_ad(log_buffer_ready_for_write_lsn(log) <= start_lsn);

  /* That's only used in the assertion at the very end. */
  const sn_t end_sn = log_translate_lsn_to_sn(start_lsn) + sn_t{str_len};

  /* A guard used to detect when we should wrap (to avoid overflowing
  outside the log buffer). */
  byte *buf_end = log.buf + log.buf_size;

  /* Pointer to next data byte to set within the log buffer. */
  byte *ptr = log.buf + (start_lsn % log.buf_size);

  /* Lsn value for the next byte to copy. */
  lsn_t lsn = start_lsn;

  /* Copy log records to the reserved space in the log buffer.
  Decrease number of bytes to copy (str_len) after some are
  copied. Proceed until number of bytes to copy reaches zero. */
  while (true) {
    /* Calculate offset from the beginning of log block. */
    const auto offset = lsn % OS_FILE_LOG_BLOCK_SIZE;

    ut_a(offset >= LOG_BLOCK_HDR_SIZE);
    ut_a(offset < OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE);

    /* Calculate how many free data bytes are available
    within current log block. */
    const auto left = OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE - offset;

    ut_a(left > 0);
    ut_a(left < OS_FILE_LOG_BLOCK_SIZE);

    size_t len, lsn_diff;

    if (left > str_len) {
      /* There are enough free bytes to finish copying
      the remaining part, leaving at least single free
      data byte in the log block. */

      len = str_len;

      lsn_diff = str_len;

    } else {
      /* We have more to copy than the current log block
      has remaining data bytes, or exactly the same.

      In both cases, next lsn value will belong to the
      next log block. Copy data up to the end of the
      current log block and start a next iteration if
      there is more to copy. */

      len = left;

      lsn_diff = left + LOG_BLOCK_TRL_SIZE + LOG_BLOCK_HDR_SIZE;
    }

    ut_a(len > 0);
    ut_a(ptr + len <= buf_end);

    log_sync_point("log_buffer_write_before_memcpy");

    /* This is the critical memcpy operation, which copies data
    from internal mtr's buffer to the shared log buffer. */
    std::memcpy(ptr, str, len);

    ut_a(len <= str_len);

    str_len -= len;
    str += len;
    lsn += lsn_diff;
    ptr += lsn_diff;

    ut_a(log_is_data_lsn(lsn));

    if (ptr >= buf_end) {
      /* Wrap - next copy operation will write at the
      beginning of the log buffer. */

      ptr -= log.buf_size;
    }

    if (lsn_diff > left) {
      /* We have crossed boundaries between consecutive log
      blocks. Either we finish in next block, in which case
      user will set the proper first_rec_group field after
      this function is finished, or we finish even further,
      in which case next block should have 0. In both cases,
      we reset next block's value to 0 now, and in the first
      case, user will simply overwrite it afterwards. */

      ut_a((uintptr_t(ptr) % OS_FILE_LOG_BLOCK_SIZE) == LOG_BLOCK_HDR_SIZE);

      ut_a((uintptr_t(ptr) & ~uintptr_t(LOG_BLOCK_HDR_SIZE)) %
               OS_FILE_LOG_BLOCK_SIZE ==
           0);

      log_block_set_first_rec_group(
          reinterpret_cast<byte *>(uintptr_t(ptr) &
                                   ~uintptr_t(LOG_BLOCK_HDR_SIZE)),
          0);

      if (str_len == 0) {
        /* We have finished at the boundary. */
        break;
      }

    } else {
      /* Nothing more to copy - we have finished! */
      break;
    }
  }

  ut_a(ptr >= log.buf);
  ut_a(ptr <= buf_end);
  ut_a(buf_end == log.buf + log.buf_size);
  ut_a(log_translate_lsn_to_sn(lsn) == end_sn);

  return lsn;
}

void log_buffer_write_completed(log_t &log, lsn_t start_lsn, lsn_t end_lsn) {
  ut_ad(rw_lock_own(log.sn_lock_inst, RW_LOCK_S));

  ut_a(log_is_data_lsn(start_lsn));
  ut_a(log_is_data_lsn(end_lsn));
  ut_a(end_lsn > start_lsn);

  /* Let M = log.recent_written_size (number of slots).
  For any integer k, all lsn values equal to: start_lsn + k*M
  correspond to the same slot, and only the smallest of them
  may use the slot. At most one of them can fit the range
  [log.buf_ready_for_write_lsn..log.buf_ready_ready_write_lsn+M).
  Any smaller values have already used the slot. Hence, we just
  need to wait until start_lsn will fit the mentioned range. */

  uint64_t wait_loops = 0;

  while (!log.recent_written.has_space(start_lsn)) {
    os_event_set(log.writer_event);
    ++wait_loops;
    std::this_thread::sleep_for(std::chrono::microseconds(20));
  }

  if (unlikely(wait_loops != 0)) {
    MONITOR_INC_VALUE(MONITOR_LOG_ON_RECENT_WRITTEN_WAIT_LOOPS, wait_loops);
  }

  /* Disallow reordering of writes to log buffer after this point.
  This is actually redundant, because we use seq_cst inside the
  log.recent_written.add_link(). However, we've decided to leave
  the separate acq-rel synchronization between user threads and
  log writer. Reasons:
          1. Not to rely on internals of Link_buf::add_link.
          2. Stress that this synchronization is required in
             case someone decided to weaken memory ordering
             inside Link_buf. */
  std::atomic_thread_fence(std::memory_order_release);

  log_sync_point("log_buffer_write_completed_before_store");

  ut_ad(log.write_lsn.load() <= start_lsn);
  ut_ad(log_buffer_ready_for_write_lsn(log) <= start_lsn);

  /* Note that end_lsn will not point to just before footer,
  because we have already validated that end_lsn is valid. */
  log.recent_written.add_link_advance_tail(start_lsn, end_lsn);

  /* if someone is waiting for, set the event. (if possible) */
  lsn_t ready_lsn = log_buffer_ready_for_write_lsn(log);

  if (log.current_ready_waiting_lsn > 0 &&
      log.current_ready_waiting_lsn <= ready_lsn &&
      !os_event_is_set(log.closer_event) &&
      log_closer_mutex_enter_nowait(log) == 0) {
    if (log.current_ready_waiting_lsn > 0 &&
        log.current_ready_waiting_lsn <= ready_lsn &&
        !os_event_is_set(log.closer_event)) {
      log.current_ready_waiting_lsn = 0;
      os_event_set(log.closer_event);
    }
    log_closer_mutex_exit(log);
  }
}

void log_wait_for_space_in_log_recent_closed(log_t &log, lsn_t lsn) {
  ut_a(log_is_data_lsn(lsn));

  ut_ad(lsn >= log_buffer_dirty_pages_added_up_to_lsn(log));

  uint64_t wait_loops = 0;

  while (!log.recent_closed.has_space(lsn)) {
    ++wait_loops;
    std::this_thread::sleep_for(std::chrono::microseconds(20));
  }

  if (unlikely(wait_loops != 0)) {
    MONITOR_INC_VALUE(MONITOR_LOG_ON_RECENT_CLOSED_WAIT_LOOPS, wait_loops);
  }
}

void log_buffer_close(log_t &log, const Log_handle &handle) {
  const lsn_t start_lsn = handle.start_lsn;
  const lsn_t end_lsn = handle.end_lsn;

  ut_a(log_is_data_lsn(start_lsn));
  ut_a(log_is_data_lsn(end_lsn));
  ut_a(end_lsn > start_lsn);

  ut_ad(start_lsn >= log_buffer_dirty_pages_added_up_to_lsn(log));

  ut_ad(rw_lock_own(log.sn_lock_inst, RW_LOCK_S));

  std::atomic_thread_fence(std::memory_order_release);

  log_sync_point("log_buffer_write_completed_dpa_before_store");

  log_buffer_s_lock_exit_close(log, start_lsn, end_lsn);
}

void log_buffer_set_first_record_group(log_t &log, lsn_t rec_group_end_lsn) {
  ut_ad(rw_lock_own(log.sn_lock_inst, RW_LOCK_S));

  ut_a(log_is_data_lsn(rec_group_end_lsn));

  const lsn_t last_block_lsn =
      ut_uint64_align_down(rec_group_end_lsn, OS_FILE_LOG_BLOCK_SIZE);

  byte *buf = log.buf;

  ut_a(buf != nullptr);

  byte *last_block_ptr = buf + (last_block_lsn % log.buf_size);

  log_sync_point("log_buffer_set_first_record_group_before_update");

  /* User thread needs to set proper first_rec_group value before
  link is added to recent written buffer. */
  ut_ad(log_buffer_ready_for_write_lsn(log) < rec_group_end_lsn);

  /* This also guarantees, that log buffer could not become resized
  meanwhile. */
  ut_a(buf + (last_block_lsn % log.buf_size) == last_block_ptr);

  /* This field is not overwritten. It is set to 0, when user thread
  crosses boundaries of consecutive log blocks. */
  ut_a(log_block_get_first_rec_group(last_block_ptr) == 0);

  log_block_set_first_rec_group(last_block_ptr,
                                rec_group_end_lsn % OS_FILE_LOG_BLOCK_SIZE);
}

void log_buffer_flush_to_disk(log_t &log, bool sync) {
  ut_a(!srv_read_only_mode);
  ut_a(!recv_recovery_is_on());

  const lsn_t lsn = log_get_lsn(log);

  /* Google's patch introduced log_buffer_sync_in_background which was calling
  log_write_up_to, and this is the left-over from that. */
  log_write_up_to(log, lsn, sync);
}

void log_buffer_flush_to_disk(bool sync) {
  log_buffer_flush_to_disk(*log_sys, sync);
}

void log_buffer_sync_in_background() {
  log_t &log = *log_sys;

  /* Just to be sure not to miss advance */
  log.recent_closed.advance_tail();

  /* If the log flusher thread is working, no need to call. */
  if (log.writer_threads_paused.load(std::memory_order_acquire)) {
    log.recent_written.advance_tail();
    log_buffer_flush_to_disk(log, true);
  }
}

void log_buffer_get_last_block(log_t &log, lsn_t &last_lsn, byte *last_block,
                               uint32_t &block_len) {
  ut_ad(last_block != nullptr);

  /* We acquire x-lock for the log buffer to prevent:
          a) resize of the log buffer
          b) overwrite of the fragment which we are copying */

  log_buffer_x_lock_enter(log);

  /* Because we have acquired x-lock for the log buffer, current
  lsn will not advance and all users that reserved smaller lsn
  have finished writing to the log buffer. */

  last_lsn = log_get_lsn(log);

  byte *buf = log.buf;

  ut_a(buf != nullptr);

  /* Copy last block from current buffer. */

  const lsn_t block_lsn =
      ut_uint64_align_down(last_lsn, OS_FILE_LOG_BLOCK_SIZE);

  byte *src_block = buf + block_lsn % log.buf_size;

  const auto data_len = last_lsn % OS_FILE_LOG_BLOCK_SIZE;

  ut_ad(data_len >= LOG_BLOCK_HDR_SIZE);

  std::memcpy(last_block, src_block, data_len);

  /* We have copied data from the log buffer. We can release
  the x-lock and let new writes to the buffer go. Since now,
  we work only with our local copy of the data. */

  log_buffer_x_lock_exit(log);

  std::memset(last_block + data_len, 0x00, OS_FILE_LOG_BLOCK_SIZE - data_len);

  Log_data_block_header block_header;
  block_header.set_lsn(block_lsn);
  block_header.m_data_len = data_len;
  block_header.m_first_rec_group = log_block_get_first_rec_group(last_block);
  ut_ad(block_header.m_first_rec_group <= data_len);

  log_data_block_header_serialize(block_header, last_block);

  block_len = OS_FILE_LOG_BLOCK_SIZE;
}

/** @} */

/**************************************************/ /**

 @name Traversing links in the redo log recent buffers

 @todo Consider refactoring to extract common logic of
 two recent buffers to a common class (Links_buffer ?).

 *******************************************************/

/** @{ */

void log_advance_ready_for_write_lsn(log_t &log) {
  ut_ad(log_writer_mutex_own(log));
  ut_d(log_writer_thread_active_validate());

  const lsn_t write_lsn = log.write_lsn.load();

  const auto write_max_size = srv_log_write_max_size;

  ut_a(write_max_size > 0);

  auto stop_condition = [&](lsn_t prev_lsn, lsn_t next_lsn) {
    ut_a(log_is_data_lsn(prev_lsn));
    ut_a(log_is_data_lsn(next_lsn));

    ut_a(next_lsn > prev_lsn);
    ut_a(prev_lsn >= write_lsn);

    log_sync_point("log_advance_ready_for_write_before_reclaim");

    return prev_lsn - write_lsn >= write_max_size;
  };

  const lsn_t previous_lsn = log_buffer_ready_for_write_lsn(log);

  ut_a(previous_lsn >= write_lsn);

  if (log.recent_written.advance_tail_until(stop_condition)) {
    log_sync_point("log_advance_ready_for_write_before_update");

    /* Validation of recent_written is optional because
    it takes significant time (delaying the log writer). */
    if (log_test != nullptr &&
        log_test->enabled(Log_test::Options::VALIDATE_RECENT_WRITTEN)) {
      /* All links between ready_lsn and lsn have
      been traversed. The slots can't be re-used
      before we updated the tail. */
      log.recent_written.validate_no_links(previous_lsn,
                                           log_buffer_ready_for_write_lsn(log));
    }

    ut_a(log_buffer_ready_for_write_lsn(log) > previous_lsn);

    std::atomic_thread_fence(std::memory_order_acquire);
  }
}

/** @} */

#endif /* !UNIV_HOTBACKUP */
