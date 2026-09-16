/*****************************************************************************

Copyright (c) 1995, 2026, Oracle and/or its affiliates.
Copyright (c) 2009, Google Inc.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************/ /**

 Redo log checkpointing.

 *******************************************************/
#include "log0chkp.h"

/* std::chrono::X */
#include <chrono>

/* std::memcpy */
#include <cstring>

/* arch_page_sys */
#include "arch0arch.h"

/* buf_pool_get_oldest_modification_lwm, page_id_t */
#include "buf0buf.h"

/* buf_flush_fsync */
#include "buf0flu.h"

/* pages_persistence */
#include "fil0pages_persistence_interface.h"

/* log_buffer_ready_for_write_lsn */
#include "log0buf.h"

/* log_can_encrypt */
#include "log0encryption.h"

/* log_files_header_flush, ... */
#include "log0files_io.h"

/* log_limits_mutex, ... */
#include "log0log.h"

/* recv_recovery_is_on() */
#include "log0recv.h"

/* ib::redo::handler */
#include "log0handler_interface.h"

/* log_t::X */
#include "log0sys.h"

/* log_sync_point, log_test */
#include "log0test.h"

/* OS_FILE_LOG_BLOCK_SIZE, ... */
#include "log0types.h"

/* log_writer_mutex */
#include "log0write.h"

/* mach_write_to_4, ... */
#include "mach0data.h"

/* DBUG_PRINT, ... */
#include "my_dbug.h"

/* os_event_wait_time_low */
#include "os0event.h"

/* os_thread_create */
#include "os0thread-create.h"

/* create_internal_thd */
#include "sql_thd_internal_api.h"

/* MONITOR_INC, ... */
#include "srv0mon.h"

/* srv_read_only_mode */
#include "srv0srv.h"

/* srv_is_being_started */
#include "srv0start.h"

/* ut_uint64_align_down */
#include "ut0byte.h"

#ifndef UNIV_HOTBACKUP

Log_checkpointing *log_checkpointing;

/** Waits for checkpoint advanced to at least that lsn.
@param[in]      lsn     lsn up to which we are waiting */
static void log_wait_for_checkpoint(lsn_t lsn);

/** Requests for urgent flush of dirty pages, to advance oldest_lsn
in flush lists. This should force page cleaners
to perform the sync-flush in which case the innodb_max_io_capacity
is not respected. This should be called when we are close to running
out of space in redo log (close to m_free_check_limit_lsn).
*/
static void log_request_sync_flush();

/** Calculates lsn at which we might write a next checkpoint. It does the
best effort, but possibly the maximum allowed lsn, could be even bigger.
That's because the order of dirty pages in flush lists has been relaxed,
and we don't want to spend time on traversing the whole flush lists here.

Note that some flush lists could be empty, and some additions of dirty pages
could be pending (threads have written data to the log buffer and became
scheduled out just before adding the dirty pages). That's why the calculated
value cannot be larger than the buf_flush_list_added->smallest_not_added_lsn()
(only up to this lsn value we are sure, that all the dirty pages have been
added).

It is guaranteed, that the returned value will not be smaller than
the log_checkpointer.m_checkpoint_lsn.

@return lsn for which we might write the checkpoint */
[[nodiscard]] static lsn_t log_compute_available_for_checkpoint_lsn() {
  /* The buf_flush_list_added->assume_added_up_to() can only increase,
  and that happens only after all related dirty pages have been added
  to the flush lists.

  Hence, to avoid issues related to race conditions, we follow order:

          1. Note lsn up to which all dirty pages have already been
             added to flush lists.

          2. Check buffer pool to get LWM lsn for unflushed dirty pages
             added to flush lists.

          3. Flush lists were empty (no LWM) => use [1] as LWM.

          4. Checkpoint LSN could be min(LWM, flushed_to_disk_lsn). */

  log_sync_point("log_get_available_for_chkp_lsn_before_dpa");

  const lsn_t dpa_lsn = buf_flush_list_added->smallest_not_added_lsn();

  ut_ad(dpa_lsn >= log_checkpointing->get_checkpoint() ||
        !log_checkpointer_mutex_own());

  ut_ad(log_is_data_lsn(dpa_lsn));

  log_sync_point("log_get_available_for_chkp_lsn_before_buf_pool");

  lsn_t lwm_lsn = buf_pool_get_oldest_modification_lwm();

  /* We cannot return lsn larger than dpa_lsn,
  because some mtr's commit could be in the middle, after
  its log records have been written to log buffer, but before
  its dirty pages have been added to flush lists. */

  if (lwm_lsn == 0) {
    /* Empty flush list. */
    lwm_lsn = dpa_lsn;
  } else {
    ut_ad(log_is_data_lsn(lwm_lsn));
    lwm_lsn = std::min(lwm_lsn, dpa_lsn);
  }

  /* Cannot go beyond flushed lsn.

  We cannot write checkpoint at higher lsn than lsn up to which
  redo is flushed to disk. We must not wait for log writer/flusher
  in log_checkpoint(). Therefore we need to limit lsn for checkpoint.
  That's because we would risk a deadlock otherwise - because writer
  waits for advanced checkpoint, when it detected that there is no
  free space in log files.

  However, note that the deadlock would happen only if we created
  log records without dirty pages (during page flush we anyway wait
  for redo flushed up to page's newest_modification). */

  const lsn_t flushed_lsn = ib::redo::handler->peek_first_nonpersisted_lsn();
  ut_ad(log_is_data_lsn(lwm_lsn));
  lsn_t lsn = std::min(lwm_lsn, flushed_lsn);
  /* We expect in recovery that checkpoint_lsn is within data area
  of log block. In future we could get rid of this assumption, but
  we would need to ensure that recovery handles that properly.

  For that, we would better refactor log0recv.cc and separate two
  phases:
          1. Looking for the proper mtr boundary to start at (only parse).
          2. Actual parsing and applying changes. */
  if (flushed_lsn < lwm_lsn) {
    if (!log_is_data_lsn(lsn)) {
      /* The only possible reason lsn is not inside a block, is that we've used
      flushed_lsn as the value for it, and that it is at block boundary */
      ut_a(lsn == flushed_lsn);
      ut_a(lsn % OS_FILE_LOG_BLOCK_SIZE == 0);
      /* As flushed_lsn marks end of the log, and during recovery InnoDB scans
      from the block containing checkpoint_lsn *forward* searching for a block
      which has a known mtr boundary, we have to ensure that there will be at
      least one such block with non-zero FIRST_REC_GROUP header. As the scenario
      where flushed_lsn is smaller than lwm_lsn implies that the current end of
      redo log contains mtrs which do not dirty any pages, and such mtrs contain
      paths to tablespaces etc. they can be quite long, but not much longer than
      a few FN_REFLEN. Therefore we back off by 10*FN_REFLEN to be sure we move
      back past the last of these mtrs, and thus have at least one mtr boundary
      ahead of us.
      Note: Recovery can not scan backwards, as the old files could be removed.
      Note: Checkpointer can't wait for log writer to write more, as log writer
            can already be waiting for checkpointer to free up old logs. */

      lsn = lsn - ut_uint64_align_up(FN_REFLEN * 10, OS_FILE_LOG_BLOCK_SIZE) +
            LOG_BLOCK_HDR_SIZE;
    }
  }
  ut_ad(log_is_data_lsn(lsn));
  lsn = std::max(lsn, log_checkpointing->get_checkpoint());

  ut_ad(lsn >= log_checkpointing->get_checkpoint() ||
        !log_checkpointer_mutex_own());

  ut_a(lsn <= ib::redo::handler->peek_first_nonpersisted_lsn());

  return lsn;
}

lsn_t Log_checkpointing::update_available_for_checkpoint_lsn() {
  ut_ad(log_checkpointer_is_active());

  /* Update lsn available for checkpoint. */
  lsn_t oldest_lsn = log_compute_available_for_checkpoint_lsn();

  log_limits_mutex_enter();

  /* 1. The oldest_lsn can decrease in case previously buffer pool flush
        lists were empty and now a new dirty page appeared, which causes
        a maximum delay of Buf_flush_list_added_lsns::order_lag() being
        suddenly subtracted.

     2. Race between concurrent calls to
        Log_checkpointing::update_available_for_checkpoint_lsn is also possible.
  */

  if (oldest_lsn > m_available_for_checkpoint_lsn) {
    m_available_for_checkpoint_lsn = oldest_lsn;
  } else {
    oldest_lsn = m_available_for_checkpoint_lsn;
  }

  log_limits_mutex_exit();
  return oldest_lsn;
}

lsn_t Log_checkpointing::determine_checkpoint_lsn() {
  ut_ad(log_checkpointer_mutex_own());

  log_limits_mutex_enter();

  const lsn_t oldest_lsn = m_available_for_checkpoint_lsn;

  log_limits_mutex_exit();

  return oldest_lsn;
}

dberr_t log_files_next_checkpoint(log_t &log, lsn_t next_checkpoint_lsn) {
  ut_ad(log_checkpointer_mutex_own());
  ut_a(!srv_read_only_mode);

  IB_mutex_guard writer_latch{&(log.writer_mutex), UT_LOCATION_HERE};
  IB_mutex_guard files_latch{&(log.m_files_mutex), UT_LOCATION_HERE};

  const auto next_file = log.m_files.find(next_checkpoint_lsn);
  ut_a(next_file != log.m_files.end());

  auto next_file_handle = next_file->open(Log_file_access_mode::WRITE_ONLY);
  if (!next_file_handle.is_open()) {
    return DB_CANNOT_OPEN_FILE;
  }

  log_sync_point("log_before_checkpoint_write");

  const lsn_t prev_checkpoint_lsn = log_checkpointing->get_checkpoint();
  if (prev_checkpoint_lsn != 0) {
    const auto prev_file = log.m_files.find(prev_checkpoint_lsn);
    ut_a(prev_file != log.m_files.end());

    if (prev_file->m_id != next_file->m_id) {
      /* Checkpoint is moved to the next log file. */
      if (log_can_encrypt(*log_sys)) {
        /* Write the encryption header to the new checkpoint file. */
        const dberr_t err =
            log_encryption_header_write(next_file_handle, log.m_encryption_buf);
        if (err != DB_SUCCESS) {
          return err;
        }
      }
      /* Wake up log_files_governor because it potentially might consume
      the previous log file (once we release the files_mutex). */
      os_event_set(log.m_files_governor_event);
    }
  }

  const dberr_t err = log_files_write_checkpoint_low(
      log, next_file_handle, log.next_checkpoint_header_no,
      next_checkpoint_lsn);

  if (err != DB_SUCCESS) {
    return err;
  }

  log_sync_point("log_before_checkpoint_flush");

  next_file_handle.fsync();

  DBUG_PRINT("ib_log", ("checkpoint info written"));

  log.next_checkpoint_header_no =
      log_next_checkpoint_header(log.next_checkpoint_header_no);

  log_sync_point("log_before_checkpoint_lsn_update");
  /* We do it here under log.writer_mutex protection, and then the caller of
  store_metadata(Metadata_key::CHECKPOINT, ..) will do it again. This is because
  in ib::redo::Handler we need the changes of
  Log_checkpointing::m_checkpoint_lsn to be protected by log.writer_mutex (and
  they are, as we *change* it here, and then we just do an idempotent store),
  while in general a Redo Log Handler does not even know what a
  log.writer_mutex is, and only cares about log_checkpointing->checkpoint_mutex
  which we do hold in both places.
  We also hold log.m_files_mutex, which is relevant to synchronize with
  log_files_logical_size_and_checkpoint_age() which also holds it. */
  log_checkpointing->set_checkpoint(next_checkpoint_lsn);

  ut_a(!next_file->m_consumed);

  log_sync_point("log_before_checkpoint_limits_update");

  if (log.m_writer_inside_extra_margin) {
    log_writer_check_if_exited_extra_margin(log);
  }

  return DB_SUCCESS;
}

Log_checkpointing::Log_checkpointing() {
  mutex_create(LATCH_ID_LOG_CHECKPOINTER, &checkpoint_mutex);
  mutex_create(LATCH_ID_LOG_LIMITS, &limits_mutex);
  m_event = os_event_create();
  m_next_checkpoint_event = os_event_create();
}

Log_checkpointing::~Log_checkpointing() {
  mutex_free(&checkpoint_mutex);
  mutex_free(&limits_mutex);
  os_event_destroy(m_event);
  os_event_destroy(m_next_checkpoint_event);
}

void Log_checkpointing::init() {
  log_checkpointing =
      ut::aligned_new<Log_checkpointing>(alignof(Log_checkpointing));
}

void Log_checkpointing::deinit() {
  ut::aligned_delete(log_checkpointing);
  log_checkpointing = nullptr;
}

Log_checkpoint_header_no log_next_checkpoint_header(
    Log_checkpoint_header_no checkpoint_header_no) {
  switch (checkpoint_header_no) {
    case Log_checkpoint_header_no::HEADER_1:
      return Log_checkpoint_header_no::HEADER_2;
    case Log_checkpoint_header_no::HEADER_2:
      return Log_checkpoint_header_no::HEADER_1;
    default:
      ut_error;
  }
}

dberr_t log_files_write_checkpoint_low(
    log_t &log, Log_file_handle &checkpoint_file_handle,
    Log_checkpoint_header_no checkpoint_header_no, lsn_t checkpoint_lsn) {
  ut_ad(checkpoint_lsn == 0 || log_checkpointer_mutex_own());
  ut_ad(log_writer_mutex_own(log));
  ut_ad(srv_is_being_started || log_files_mutex_own(log));
  ut_a(!srv_read_only_mode);

  DBUG_PRINT("ib_log", ("checkpoint at " LSN_PF " written", checkpoint_lsn));

  Log_checkpoint_header checkpoint_header;
  checkpoint_header.m_checkpoint_lsn = checkpoint_lsn;

  return log_checkpoint_header_write(checkpoint_file_handle,
                                     checkpoint_header_no, checkpoint_header);
}

void Log_checkpointing::create_checkpoint() {
  ut_ad(log_checkpointer_mutex_own());
  ut_a(!srv_read_only_mode);
  ut_ad(!srv_checkpoint_disabled);

  /* Read the comment from should_checkpoint() from just before
  acquiring the limits mutex. It is ok if m_available_for_checkpoint_lsn
  is advanced just after we released limits_mutex here. It can only be
  increased. Also, if the value for which we will write checkpoint is
  higher than the value for which we decided that it is worth to write
  checkpoint (in should_checkpoint) - it is even better for us. */

  const lsn_t checkpoint_lsn = determine_checkpoint_lsn();

  if (arch_page_sys != nullptr) {
    arch_page_sys->flush_at_checkpoint(checkpoint_lsn);
  }

  log_sync_point("log_before_checkpoint_data_flush");

  buf_flush_fsync();

  if (log_test != nullptr) {
    log_test->fsync_written_pages();
  }

  ut_a(checkpoint_lsn >= get_checkpoint());

  ut_a(checkpoint_lsn <= buf_flush_list_added->smallest_not_added_lsn());

#ifdef UNIV_DEBUG
  if (checkpoint_lsn > ib::redo::handler->peek_first_nonpersisted_lsn()) {
    /* We need log_flusher, because we need redo flushed up
    to the oldest_lsn, and it's not been flushed yet. */
    ut_a(log_sys == nullptr || log_flusher_is_active());
  }
#endif

  ut_a(ib::redo::handler->peek_first_nonpersisted_lsn() >= checkpoint_lsn);

  m_last_checkpoint_time = std::chrono::high_resolution_clock::now();

  DBUG_PRINT("ib_log", ("Starting checkpoint at " LSN_PF, checkpoint_lsn));

  if (!save_checkpoint_value(checkpoint_lsn)) {
    return;
  }

  log_limits_mutex_enter();
  /* Inform the handler, so that it may remove the lsn(s) lower than the
  checkpoint */
  update_limits();
  /* This updates the Innodb_redo_log_checkpoint_lsn status variable, which can
  be used by an external log consumer to figure out the read position.
  It is therefore crucial that log_checkpointing->set_checkpoint(x) which
  changes the value and the subsequent call to log_update_exported_lsns()
  happens in a same critical section protected by the same
  log_checkpointing->checkpoint_mutex which is used by
  meb::redo_log_consumer_register. Otherwise a race condition would be possible
  in which a consumer was registered at "new checkpoint lsn", but got the "old"
  when querying Innodb_redo_log_checkpoint_lsn and thus wrongly believed it is
  at an older position than it really is (in the worst case: leading to crash,
  when it will try to read a file already reclaimed as no longer needed by
  consumers)
  */
  log_update_exported_lsns();
  log_limits_mutex_exit();

  os_event_set(m_next_checkpoint_event);

  DBUG_PRINT(
      "ib_log",
      ("checkpoint ended at " LSN_PF ", log flushed to " LSN_PF,
       get_checkpoint(), ib::redo::handler->peek_first_nonpersisted_lsn()));

  MONITOR_INC(MONITOR_LOG_CHECKPOINTS);

  DBUG_EXECUTE_IF("crash_after_checkpoint", DBUG_SUICIDE(););
}

dberr_t log_files_write_first_data_block_low(log_t &log,
                                             Log_file_handle &file_handle,
                                             lsn_t checkpoint_lsn,
                                             lsn_t file_start_lsn) {
  ut_a(!srv_read_only_mode);
  ut_a(file_handle.is_open());

  /* Create the first, empty log block. */
  const lsn_t block_lsn =
      ut_uint64_align_down(checkpoint_lsn, OS_FILE_LOG_BLOCK_SIZE);

  const uint16_t data_end = checkpoint_lsn % OS_FILE_LOG_BLOCK_SIZE;

  /* Write the first empty log block to the log buffer. */
  Log_data_block_header block_header;
  block_header.set_lsn(block_lsn);
  block_header.m_first_rec_group = block_header.m_data_len = data_end;

  byte block[OS_FILE_LOG_BLOCK_SIZE] = {};
  log_data_block_header_serialize(block_header, block);

  std::memcpy(log.buf + block_lsn % log.buf_size, block,
              OS_FILE_LOG_BLOCK_SIZE);

  /* Write the first empty log block to the file. */
  const os_offset_t block_offset = Log_file::offset(block_lsn, file_start_lsn);
  return log_data_blocks_write(file_handle, block_offset,
                               OS_FILE_LOG_BLOCK_SIZE, block);
}

/** Check if the checkpointing is enabled
@retval true  checkpointing is enabled
@retval false checkpointing is disabled
*/
[[nodiscard]] static bool log_request_checkpoint_validate() {
  ut_ad(log_limits_mutex_own());

#ifdef UNIV_DEBUG
  if (srv_checkpoint_disabled) {
    /* Checkpoints are disabled. Pretend it succeeded. */
    ib::info(ER_IB_MSG_1233) << "Checkpoint explicitly disabled!";

    return false;
  }
#endif /* UNIV_DEBUG */

  return true;
}

bool Log_checkpointing::request_checkpoint(lsn_t requested_lsn) {
  ut_a(requested_lsn <= ib::redo::handler->peek_first_unassigned_lsn());
  ut_ad(log_limits_mutex_own());

  ut_a(log_is_data_lsn(requested_lsn));

  const bool enabled = log_request_checkpoint_validate();
  if (enabled) {
    /* Update m_requested_checkpoint_lsn only to greater value. */
    if (requested_lsn > m_requested_checkpoint_lsn) {
      m_requested_checkpoint_lsn = requested_lsn;

      if (requested_lsn > get_checkpoint()) {
        os_event_set(m_event);
      }
    }
  }
  return enabled;
}

static void log_wait_for_checkpoint(lsn_t requested_lsn) {
  ut_a(log_checkpointer_is_active());

  auto stop_condition = [requested_lsn](bool) {
    return log_checkpointing->get_checkpoint() >= requested_lsn;
  };

  ut::wait_for(0, std::chrono::microseconds{100}, stop_condition);
}

void Log_checkpointing::request_fuzzy_checkpoint(bool sync) {
  const auto lsn = update_available_for_checkpoint_lsn();
  {
    IB_mutex_guard guard{&log_checkpointing->limits_mutex, UT_LOCATION_HERE};
    if (!request_checkpoint(lsn)) {
      ut_a(!sync);
      return;
    }
  }
  if (sync) {
    log_wait_for_checkpoint(lsn);
  }
}

void Log_checkpointing::request_sharp_checkpoint() {
  const lsn_t lsn = ib::redo::handler->peek_first_unassigned_lsn();

  {
    IB_mutex_guard guard{&log_checkpointing->limits_mutex, UT_LOCATION_HERE};
    if (!request_checkpoint(lsn)) {
      ut_error;
    }
  }

  log_wait_for_checkpoint(lsn);
}

static void log_request_sync_flush() {
  if (log_test != nullptr) {
    return;
  }

  /* A flush is urgent: we have to do a synchronous flush,
  because the oldest dirty page is too old.

  Note, that this could fire even if we did not run out
  of space in log files (users still may write to redo). */

  if (
      /* Forced flush request is processed by page_cleaner, if
      it's not active, then we must do flush ourselves. */
      !buf_flush_page_cleaner_is_active()
      /* Reason unknown. */
      || srv_is_being_started) {
    buf_flush_sync_all_buf_pools();
    return;
  }
  if (srv_flush_sync) {
    /* Wake up page cleaner asking to perform sync flush
    (unless user explicitly disabled sync-flushes). */

    int64_t sig_count = os_event_reset(buf_flush_tick_event);

    os_event_set(buf_flush_event);

    /* Wait until flush is finished or timeout happens. This is to delay
    furious checkpoint writing when sync flush is active. However, if the
    log_writer entered its extra_margin, it's better to be more aggressive
    with checkpoint writing, because the problem very likely is related to
    missing log_free_check() calls and oldest dirt page being also the newest
    page that was modified and can't be flushed due to missing space in redo.
    In such case, it is very desired to move checkpoint forward even a little
    bit. If there is a sequence of such pages, then it becomes problematic and
    we would better not delay the checkpointing that much.
    */

    const auto soft_logical_capacity =
        ib::redo::handler->get_capacity_estimate().soft_logical_capacity();
    const auto checkpoint_lsn = log_checkpointing->get_checkpoint();
    const auto current_lsn = ib::redo::handler->peek_first_unassigned_lsn();
    const auto time_to_wait_ms =
        checkpoint_lsn + soft_logical_capacity < current_lsn ? 1 : 1000;

    os_event_wait_time_low(buf_flush_tick_event,
                           std::chrono::milliseconds{time_to_wait_ms},
                           sig_count);
  }
}

static lsn_t get_soft_logical_capacity() {
  return ib::redo::handler->get_capacity_estimate().soft_logical_capacity();
}

[[nodiscard]] static lsn_t adaptive_flush_max_age(lsn_t soft_logical_capacity) {
  return Log_files_capacity::sync_flush_logical_capacity_for_soft(
      soft_logical_capacity);
}

lsn_t Log_checkpointing::adaptive_flush_max_age() {
  return ::adaptive_flush_max_age(get_soft_logical_capacity());
}

static lsn_t adaptive_flush_min_age(lsn_t soft_logical_capacity) {
  return ut_uint64_align_down(
      soft_logical_capacity -
          soft_logical_capacity / LOG_FORCING_ADAPTIVE_FLUSH_RATIO_MIN,
      OS_FILE_LOG_BLOCK_SIZE);
}

lsn_t Log_checkpointing::adaptive_flush_min_age() {
  return ::adaptive_flush_min_age(get_soft_logical_capacity());
}

static lsn_t aggressive_checkpoint_min_age(lsn_t soft_logical_capacity) {
  return ut_uint64_align_down(
      soft_logical_capacity -
          soft_logical_capacity / LOG_AGGRESSIVE_CHECKPOINT_RATIO_MIN,
      OS_FILE_LOG_BLOCK_SIZE);
}

lsn_t Log_checkpointing::aggressive_checkpoint_min_age() {
  return ::aggressive_checkpoint_min_age(get_soft_logical_capacity());
}

lsn_t Log_checkpointing::get_sync_flush_lsn() {
  /* Note: log checkpointer thread is started after recovery is finished,
  and changes gathered in recv_sys->metadata_recover are applied to dict_table_t
  objects. Until that happens checkpoints are disallowed, so sync flush
  decisions (based on checkpoint age) should be postponed. */
  if (!log_checkpointer_is_active()) {
    return 0;
  }
  update_available_for_checkpoint_lsn();

  /* We acquire limits mutex only for a short period. Afterwards these
  values might be changed:
  1. requested checkpoint lsn can increase
  2. capacity estimate could change due to reconfiguration
  3. the value of peek_first_unassigned_lsn() can march forward at any moment,
  and in particular might be "fresher" than the estimate for capacity. As we
  might request sync flush for current_lsn + margin - max_age, this means we
  might flush more or less than actually needed, in case the estimation of
  margin_length or max_history_length changed since then. None of this is a
  problem, because writing a few dirty pages more, based on slightly stale
  estimates is not a correctness issue. Writing a few too little, will be
  automatically fixed in the next iteration of log checkpointer's loop. */

  log_limits_mutex_enter();
  const lsn_t oldest_lsn = get_available_for_checkpoint_lsn();
  const lsn_t requested_checkpoint_lsn = get_requested_checkpoint_lsn();
  const auto estimate = ib::redo::handler->get_capacity_estimate();
  const lsn_t margin = estimate.margin_length;
  log_limits_mutex_exit();

  lsn_t flush_up_to = oldest_lsn;

  const lsn_t current_lsn = ib::redo::handler->peek_first_unassigned_lsn();

  ut_a(flush_up_to <= current_lsn);

  if (current_lsn == flush_up_to) {
    return 0;
  }

  const lsn_t max_age =
      ::adaptive_flush_max_age(estimate.soft_logical_capacity());

  if (current_lsn + margin - oldest_lsn > max_age) {
    ut_a(current_lsn + margin > max_age);

    flush_up_to = current_lsn + margin - max_age;
  }

  if (requested_checkpoint_lsn > flush_up_to) {
    flush_up_to = requested_checkpoint_lsn;
  }

  if (flush_up_to > current_lsn) {
    flush_up_to = current_lsn;
  }

  if (flush_up_to > oldest_lsn) {
    flush_up_to += buf_flush_list_added->order_lag();

    return flush_up_to;
  }

  return 0;
}

void Log_checkpointing::consider_sync_flush() {
  ut_ad(log_checkpointer_mutex_own());

  requested_sync_flush_lsn.store(get_sync_flush_lsn());

  if (requested_sync_flush_lsn.load() != 0) {
    log_checkpointer_mutex_exit();

    log_request_sync_flush();

    log_checkpointer_mutex_enter();

    /* It's very probable that forced flush will increase the maximum
    lsn available for creating a new checkpoint. Just try to update
    it to not wait for next checkpointer loop. */
    update_available_for_checkpoint_lsn();
  }
}

std::chrono::steady_clock::duration Log_checkpointing::time_since_checkpoint() {
  ut_ad(log_checkpointer_mutex_own());
  return std::chrono::high_resolution_clock::now() - m_last_checkpoint_time;
}

bool Log_checkpointing::should_checkpoint() {
  ut_ad(log_checkpointer_mutex_own());

#ifdef UNIV_DEBUG
  if (srv_checkpoint_disabled) {
    return false;
  }
#endif /* UNIV_DEBUG */

  const lsn_t last_checkpoint_lsn = get_checkpoint();

  /* We read the values under log_limits_mutex and release the mutex.
  The values might be changed just afterwards and that's fine. Note,
  they can only become increased. Either we decided to write chkp on
  too small value or we did not decide and we could decide in next
  iteration of the thread's loop. The only risk is that checkpointer
  could go waiting on event and miss the signaled requirement to write
  checkpoint at higher lsn, which was requested just after we released
  the mutex. This is impossible, because we read sig_count of the event
  when we reset the event which happens before this point and then pass
  the sig_count to the function responsible for waiting. If sig_count
  is changed it means new notifications are there and we instantly start
  next iteration. The event is signaled under the limits_mutex in the
  same critical section in which requirements are updated. */

  log_limits_mutex_enter();
  const lsn_t oldest_lsn = get_available_for_checkpoint_lsn();
  const lsn_t requested_checkpoint_lsn = get_requested_checkpoint_lsn();
  bool periodical_checkpoints_enabled = are_periodical_checkpoints_enabled();
  const auto estimate = ib::redo::handler->get_capacity_estimate();
  const lsn_t margin = estimate.margin_length;
  log_limits_mutex_exit();

  if (oldest_lsn <= last_checkpoint_lsn) {
    return false;
  }

  const lsn_t current_lsn = ib::redo::handler->peek_first_unassigned_lsn();

  ut_a(last_checkpoint_lsn <= oldest_lsn);
  ut_a(oldest_lsn <= current_lsn);

  const lsn_t checkpoint_age = current_lsn + margin - last_checkpoint_lsn;
  /* Update checkpoint_lsn stored in header of log files if:
          a) periodical checkpoints are enabled and more than 1s
             elapsed since the last checkpoint,
          b) or checkpoint age is greater than aggressive_checkpoint_min_age,
          c) or it was requested to have greater checkpoint_lsn,
             and oldest_lsn allows to satisfy the request. */

  if ((last_checkpoint_lsn < requested_checkpoint_lsn &&
       requested_checkpoint_lsn <= oldest_lsn) ||
      ::aggressive_checkpoint_min_age(estimate.soft_logical_capacity()) <=
          checkpoint_age) {
    return true;
  }

  DBUG_EXECUTE_IF("periodical_checkpoint_disabled",
                  periodical_checkpoints_enabled = false;);

  return periodical_checkpoints_enabled &&
         get_srv_log_checkpoint_every() <= time_since_checkpoint();
}

void Log_checkpointing::consider_checkpoint() {
  ut_ad(log_checkpointer_mutex_own());

  if (should_checkpoint()) {
    create_checkpoint();
  }
}

void Log_checkpointing::log_checkpointer() { log_checkpointing->do_work(); }

void Log_checkpointing::do_work() {
  /* As a side effect this sets current_thd and lets us use debug syncpoints. */
  m_checkpointer_thd = create_internal_thd();

  static const uint64_t log_busy_checkpoint_interval =
      7; /*SRV_MASTER_CHECKPOINT_INTERVAL*/
  auto old_activity_count = srv_get_activity_count();
  ulint error = OS_SYNC_TIME_EXCEEDED;

  for (;;) {
    log_limits_mutex_enter();
    update_limits();
    log_limits_mutex_exit();

    log_checkpointer_mutex_enter();

    const auto sig_count = os_event_reset(m_event);
    const lsn_t requested_checkpoint_lsn = get_requested_checkpoint_lsn();

    bool system_is_busy = false;
    if (error == OS_SYNC_TIME_EXCEEDED &&
        srv_check_activity(old_activity_count)) {
      old_activity_count = srv_get_activity_count();
      /* system is busy. takes longer interval. */
      system_is_busy = true;
    }

    if (error != OS_SYNC_TIME_EXCEEDED || !system_is_busy ||
        requested_checkpoint_lsn > get_checkpoint() ||
        time_since_checkpoint() >=
            log_busy_checkpoint_interval * get_srv_log_checkpoint_every()) {
      /* Consider flushing some dirty pages. */
      consider_sync_flush();

      log_sync_point("log_checkpointer_before_consider_checkpoint");

      /* Consider writing checkpoint. */
      consider_checkpoint();
    }

    log_checkpointer_mutex_exit();

    if (requested_checkpoint_lsn > get_checkpoint()) {
      /* not satisfied. retry. */
      error = 0;
    } else {
      error = os_event_wait_time_low(m_event, get_srv_log_checkpoint_every(),
                                     sig_count);
    }

    /* Check if we should close the thread. */
    if (m_thread_should_stop.load()) {
      /* The ib::redo::handler->persist_available() should have been called
      before setting m_thread_should_stop to true. We can't call it from here
      directly, as this could block on log writer waiting for checkpoint which
      can only be created by us - deadlock. */

      ut_a_eq(ib::redo::handler->peek_first_unassigned_lsn(),
              ib::redo::handler->peek_first_nonpersisted_lsn());
      ut_a_eq(buf_flush_list_added->smallest_not_added_lsn(),
              ib::redo::handler->peek_first_nonpersisted_lsn());

      if (log_sys != nullptr) {
        log_t &log = *log_sys;
        const lsn_t end_lsn = log.write_lsn.load();

        ut_a(log_is_data_lsn(end_lsn));

        ut_a(end_lsn == log_buffer_ready_for_write_lsn(log));
      }
      break;
    }
  }

  destroy_internal_thd(m_checkpointer_thd);
}

void Log_checkpointing::start_thread() {
  ut_a(!srv_read_only_mode);
  m_thread_should_stop.store(false);
  srv_threads.m_log_checkpointer = os_thread_create(
      log_checkpointer_thread_key, 0, Log_checkpointing::log_checkpointer);
  srv_threads.m_log_checkpointer.start();
}

void Log_checkpointing::stop_thread_no_wait() {
  m_thread_should_stop.store(true);
  os_event_set(m_event);
}

void Log_checkpointing::stop_thread_and_wait() {
  stop_thread_no_wait();
  while (log_checkpointer_is_active()) {
    os_event_set(m_event);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }
}

void Log_checkpointing::update_limits() {
  ut_ad(log_limits_mutex_own());
  const auto checkpoint_lsn = m_checkpoint_lsn.load();
  auto status = ib::redo::handler->do_not_need_smaller_than(checkpoint_lsn);
  if (status != ib::redo::Status::SUCCESS) {
    ib::error(ER_IB_REDO_HANDLER_COULD_NOT_ACK_TO_TRUNCATE_LSN, checkpoint_lsn);
  }
}

bool Log_checkpointing::save_checkpoint_value(lsn_t checkpoint_lsn) {
  using Metadata_value = ib::redo::Handler_interface::Metadata_value;
  using Metadata_key = ib::redo::Metadata_key;
  using Status = ib::redo::Status;

  Metadata_value value;
  log_checkpoint_header_serialize({checkpoint_lsn}, value.data());
  if (ib::redo::handler->store_metadata(Metadata_key::CHECKPOINT, value) ==
      Status::SUCCESS) {
    /* The log_sys requires all changes to checkpoint lsn to happen under the
    log_sys.writer_mutex. Therefore we assert that the call to set_checkpoint()
    in following line will not change the checkpoint lsn value, as it was
    already changed during ib::redo::Handler::store_metadata() in previous
    line where it was properly guarded by the log_sys.writer_mutex." */
    ut_a(log_sys == nullptr || checkpoint_lsn == get_checkpoint());
    set_checkpoint(checkpoint_lsn);
    return true;
  }
  return false;
}

dberr_t Log_checkpointing::load_checkpoint_value() {
  using Metadata_value = ib::redo::Handler_interface::Metadata_value;
  using Metadata_key = ib::redo::Metadata_key;
  using Status = ib::redo::Status;

  /* Call the redo log handler API to get the block having checkpoint LSN */
  Metadata_value cp_block{0};
  if (ib::redo::handler->get_metadata(Metadata_key::CHECKPOINT, cp_block) !=
      Status::SUCCESS) {
    return DB_ERROR;
  }

  /* Read checkpoint LSN from the read block */
  Log_checkpoint_header header;
  if (!log_checkpoint_header_deserialize(cp_block.data(), header)) {
    DBUG_PRINT("ib_log",
               ("invalid checkpoint " UINT32PF " checksum %lx", uint32_t{(1)},
                ulong{log_block_get_checksum(cp_block.data())}));
    return DB_CORRUPTION;
  }
  const lsn_t checkpoint_lsn = header.m_checkpoint_lsn;

  /* ib::redo::Handler::start_reading() should've already found the same lsn */
  ut_a(log_sys == nullptr || checkpoint_lsn == get_checkpoint());

  set_checkpoint(checkpoint_lsn);

  return DB_SUCCESS;
}

void Log_checkpointing::get_limits(lsn_t &limit_for_free_check,
                                   lsn_t &limit_for_dirty_page_age) {
  IB_mutex_guard limits_latch{&limits_mutex, UT_LOCATION_HERE};
  const auto estimate = ib::redo::handler->get_capacity_estimate();
  const lsn_t min_age =
      ::adaptive_flush_min_age(estimate.soft_logical_capacity());
  ut_a(min_age != 0);

  limit_for_free_check =
      ut_uint64_align_down(estimate.max_history_length, OS_FILE_LOG_BLOCK_SIZE);

  limit_for_dirty_page_age = ut_uint64_align_down(
      min_age - estimate.margin_length, OS_FILE_LOG_BLOCK_SIZE);
}

void log_update_exported_lsns() {
  ut_ad(log_limits_mutex_own());
  /* The order of this loads is important, as the underlying atomics can change
  even though we hold the limits mutex. The goal is to provide inequalities:
  checkpoint_lsn <= flushed_to_disk_lsn <= current_lsn.
  The mutex only ensures that the writes to export_vars have defined behaviour
  even though they are not atomics. */
  export_vars.innodb_redo_log_checkpoint_lsn =
      pages_persistence->get_checkpoint_lsn();

  export_vars.innodb_redo_log_flushed_to_disk_lsn =
      ib::redo::handler->peek_first_nonpersisted_lsn();

  export_vars.innodb_redo_log_current_lsn =
      ib::redo::handler->peek_first_unassigned_lsn();
}

#endif /* !UNIV_HOTBACKUP */
