/*****************************************************************************

Copyright (c) 1995, 2022, Oracle and/or its affiliates.
Copyright (c) 2009, Google Inc.

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
 @file log/log0chkp.cc

 Redo log checkpointing.

 File consists of four groups:
   1. Coordination between log and buffer pool (oldest_lsn).
   2. Making checkpoints (including the log_checkpointer thread).
   3. Free check.

 *******************************************************/

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

/* dict_persist_to_dd_table_buffer */
#include "dict0dict.h"

/* log_buffer_dirty_pages_added_up_to_lsn */
#include "log0buf.h"

#include "log0chkp.h"

/* log_can_encrypt */
#include "log0encryption.h"

/* log_files_header_flush, ... */
#include "log0files_io.h"

/* log_limits_mutex, ... */
#include "log0log.h"

/* recv_recovery_is_on() */
#include "log0recv.h"

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

/* MONITOR_INC, ... */
#include "srv0mon.h"

/* srv_read_only_mode */
#include "srv0srv.h"

/* srv_is_being_started */
#include "srv0start.h"

/* ut_uint64_align_down */
#include "ut0byte.h"

#ifndef UNIV_HOTBACKUP

/** Updates lsn available for checkpoint.
@param[in,out]  log redo log */
static void log_update_available_for_checkpoint_lsn(log_t &log);

/** Checks if checkpoint should be written. Checks time elapsed since the last
checkpoint, age of the last checkpoint and if there was any extra request to
write the checkpoint (e.g. coming from log_make_latest_checkpoint()).
@return true if checkpoint should be written */
static bool log_should_checkpoint(log_t &log);

/** Considers writing next checkpoint. Checks if checkpoint should be written
(using log_should_checkpoint()) and writes the checkpoint if that's the case. */
static void log_consider_checkpoint(log_t &log);

/** Considers requesting page cleaners to execute sync flush. */
static void log_consider_sync_flush(log_t &log);

/** Makes a checkpoint. Note that this function does not flush dirty blocks
from the buffer pool. It only checks what is lsn of the oldest modification
in the buffer pool, and writes information about the lsn in log files.
@param[in,out]  log  redo log */
static void log_checkpoint(log_t &log);

/** Calculates time that elapsed since last checkpoint.
@return Time duration elapsed since the last checkpoint */
static std::chrono::steady_clock::duration log_checkpoint_time_elapsed(
    const log_t &log);

/** Requests a checkpoint written for lsn greater or equal to provided one.
The log.checkpointer_mutex has to be acquired before it is called, and it
is not released within this function.
@param[in,out]  log             redo log
@param[in]      requested_lsn   provided lsn (checkpoint should be not older) */
static void log_request_checkpoint_low(log_t &log, lsn_t requested_lsn);

/** Requests a checkpoint written in the next log file (not in the one,
to which current log.last_checkpoint_lsn belongs to). Prior to calling
this function, caller must acquire the log.limits_mutex !
@param[in,out]   log   redo log */
static void log_request_checkpoint_in_next_file_low(log_t &log);

/** Waits for checkpoint advanced to at least that lsn.
@param[in]      log     redo log
@param[in]      lsn     lsn up to which we are waiting */
static void log_wait_for_checkpoint(const log_t &log, lsn_t lsn);

/** Requests for urgent flush of dirty pages, to advance oldest_lsn
in flush lists to provided value. This should force page cleaners
to perform the sync-flush in which case the innodb_max_io_capacity
is not respected. This should be called when we are close to running
out of space in redo log (close to free_check_limit_sn).
@param[in]  log         redo log
@param[in]  new_oldest  oldest_lsn to stop flush at (or greater)
@retval  true   requested page flushing
@retval  false  did not request page flushing (either because it is
                unit test for redo log or sync flushing is disabled
                by sys_var: innodb_flush_sync) */
static bool log_request_sync_flush(const log_t &log, lsn_t new_oldest);

/**************************************************/ /**

 @name Log - coordination with buffer pool and oldest_lsn

 *******************************************************/

/** @{ */

/** Calculates lsn at which we might write a next checkpoint. It does the
best effort, but possibly the maximum allowed lsn, could be even bigger.
That's because the order of dirty pages in flush lists has been relaxed,
and we don't want to spend time on traversing the whole flush lists here.

Note that some flush lists could be empty, and some additions of dirty pages
could be pending (threads have written data to the log buffer and became
scheduled out just before adding the dirty pages). That's why the calculated
value cannot be larger than the log.buf_dirty_pages_added_up_to_lsn (only up
to this lsn value we are sure, that all the dirty pages have been added).

It is guaranteed, that the returned value will not be smaller than
the log.last_checkpoint_lsn.

@return lsn for which we might write the checkpoint */
static lsn_t log_compute_available_for_checkpoint_lsn(const log_t &log) {
  /* The log_buffer_dirty_pages_added_up_to_lsn() can only increase,
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

  const lsn_t dpa_lsn = log_buffer_dirty_pages_added_up_to_lsn(log);

  ut_ad(dpa_lsn >= log.last_checkpoint_lsn.load() ||
        !log_checkpointer_mutex_own(log));

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

  const lsn_t flushed_lsn = log.flushed_to_disk_lsn.load();

  lsn_t lsn = std::min(lwm_lsn, flushed_lsn);

  /* We expect in recovery that checkpoint_lsn is within data area
  of log block. In future we could get rid of this assumption, but
  we would need to ensure that recovery handles that properly.

  For that, we would better refactor log0recv.cc and separate two
  phases:
          1. Looking for the proper mtr boundary to start at (only parse).
          2. Actual parsing and applying changes. */

  if (lsn % OS_FILE_LOG_BLOCK_SIZE == 0) {
    /* Do not make checkpoints at block boundary.

    We hopefully will get rid of this exception and allow
    recovery to start at arbitrary checkpoint value. */
    lsn = lsn - OS_FILE_LOG_BLOCK_SIZE + LOG_BLOCK_HDR_SIZE;
  }

  ut_a(lsn % OS_FILE_LOG_BLOCK_SIZE >= LOG_BLOCK_HDR_SIZE);

  ut_a(lsn % OS_FILE_LOG_BLOCK_SIZE <
       OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE);

  lsn = std::max(lsn, log.last_checkpoint_lsn.load());

  ut_ad(lsn >= log.last_checkpoint_lsn.load() ||
        !log_checkpointer_mutex_own(log));

  ut_a(lsn <= log.flushed_to_disk_lsn.load());

  return lsn;
}

static void log_update_available_for_checkpoint_lsn(log_t &log) {
  /* Note: log.m_allow_checkpoints is set to true after recovery is finished,
  and changes gathered in srv_dict_metadata are applied to dict_table_t
  objects; or in log_start() if recovery was not needed. We can't trust
  flush lists until recovery is finished, so we must not update lsn available
  for checkpoint (as update would be based on what we can see inside them). */
  if (!log.m_allow_checkpoints.load(std::memory_order_acquire)) {
    return;
  }

  /* Update lsn available for checkpoint. */
  log.recent_closed.advance_tail();
  const lsn_t oldest_lsn = log_compute_available_for_checkpoint_lsn(log);

  log_limits_mutex_enter(log);

  /* 1. The oldest_lsn can decrease in case previously buffer pool flush
        lists were empty and now a new dirty page appeared, which causes
        a maximum delay of log.recent_closed_size being suddenly subtracted.

     2. Race between concurrent log_update_available_for_checkpoint_lsn is
        also possible. */

  if (oldest_lsn > log.available_for_checkpoint_lsn) {
    log.available_for_checkpoint_lsn = oldest_lsn;
  }

  log_limits_mutex_exit(log);
}

/** @} */

/**************************************************/ /**

 @name Log - making checkpoints

 *******************************************************/

/** @{ */

void log_set_dict_max_allowed_checkpoint_lsn(log_t &log, lsn_t max_lsn) {
  log_limits_mutex_enter(log);
  log.dict_max_allowed_checkpoint_lsn = max_lsn;
  log_limits_mutex_exit(log);
}

static lsn_t log_determine_checkpoint_lsn(log_t &log) {
  ut_ad(log_checkpointer_mutex_own(log));
  ut_ad(log.m_allow_checkpoints.load());

  log_limits_mutex_enter(log);

  const lsn_t oldest_lsn = log.available_for_checkpoint_lsn;

  const lsn_t dict_lsn = log.dict_max_allowed_checkpoint_lsn;

  log_limits_mutex_exit(log);

  ut_a(dict_lsn == 0 || dict_lsn >= log.last_checkpoint_lsn.load());

  if (dict_lsn == 0) {
    return oldest_lsn;
  } else {
    return std::min(oldest_lsn, dict_lsn);
  }
}

dberr_t log_files_next_checkpoint(log_t &log, lsn_t next_checkpoint_lsn) {
  ut_ad(log_checkpointer_mutex_own(log));
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

  const lsn_t prev_checkpoint_lsn = log.last_checkpoint_lsn.load();
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

  log.last_checkpoint_lsn.store(next_checkpoint_lsn);

  ut_a(!next_file->m_consumed);

  log_sync_point("log_before_checkpoint_limits_update");

  log_limits_mutex_enter(log);
  log_update_limits_low(log);
  log_update_exported_variables(log);
  log.dict_max_allowed_checkpoint_lsn = 0;
  log_limits_mutex_exit(log);

  if (log.m_writer_inside_extra_margin) {
    log_writer_check_if_exited_extra_margin(log);
  }

  os_event_set(log.next_checkpoint_event);

  return DB_SUCCESS;
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
  ut_ad(checkpoint_lsn == 0 || log_checkpointer_mutex_own(log));
  ut_ad(log_writer_mutex_own(log));
  ut_ad(srv_is_being_started || log_files_mutex_own(log));
  ut_a(!srv_read_only_mode);

  DBUG_PRINT("ib_log", ("checkpoint at " LSN_PF " written", checkpoint_lsn));

  Log_checkpoint_header checkpoint_header;
  checkpoint_header.m_checkpoint_lsn = checkpoint_lsn;

  return log_checkpoint_header_write(checkpoint_file_handle,
                                     checkpoint_header_no, checkpoint_header);
}

static void log_checkpoint(log_t &log) {
  ut_ad(log_checkpointer_mutex_own(log));
  ut_a(!srv_read_only_mode);
  ut_ad(!srv_checkpoint_disabled);
  ut_ad(log.m_allow_checkpoints.load());

  /* Read the comment from log_should_checkpoint() from just before
  acquiring the limits mutex. It is ok if available_for_checkpoint_lsn
  is advanced just after we released limits_mutex here. It can only be
  increaed. Also, if the value for which we will write checkpoint is
  higher than the value for which we decided that it is worth to write
  checkpoint (in log_should_checkpoint) - it is even better for us. */

  const lsn_t checkpoint_lsn = log_determine_checkpoint_lsn(log);

  if (arch_page_sys != nullptr) {
    arch_page_sys->flush_at_checkpoint(checkpoint_lsn);
  }

  log_sync_point("log_before_checkpoint_data_flush");

  buf_flush_fsync();

  if (log_test != nullptr) {
    log_test->fsync_written_pages();
  }

  ut_a(checkpoint_lsn >= log.last_checkpoint_lsn.load());

  ut_a(checkpoint_lsn <= log_buffer_dirty_pages_added_up_to_lsn(log));

#ifdef UNIV_DEBUG
  if (checkpoint_lsn > log.flushed_to_disk_lsn.load()) {
    /* We need log_flusher, because we need redo flushed up
    to the oldest_lsn, and it's not been flushed yet. */

    log_background_threads_active_validate(log);
  }
#endif

  ut_a(log.flushed_to_disk_lsn.load() >= checkpoint_lsn);

  const auto current_time = std::chrono::high_resolution_clock::now();

  log.last_checkpoint_time = current_time;

  DBUG_PRINT("ib_log", ("Starting checkpoint at " LSN_PF, checkpoint_lsn));

  const dberr_t err = log_files_next_checkpoint(log, checkpoint_lsn);
  if (err != DB_SUCCESS) {
    return;
  }

  DBUG_PRINT("ib_log",
             ("checkpoint ended at " LSN_PF ", log flushed to " LSN_PF,
              log.last_checkpoint_lsn.load(), log.flushed_to_disk_lsn.load()));

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
  ut_d(log.first_block_is_correct_for_lsn = checkpoint_lsn);

  /* Write the first empty log block to the file. */
  const os_offset_t block_offset = Log_file::offset(block_lsn, file_start_lsn);
  return log_data_blocks_write(file_handle, block_offset,
                               OS_FILE_LOG_BLOCK_SIZE, block);
}

static void log_request_checkpoint_low(log_t &log, lsn_t requested_lsn) {
  ut_a(requested_lsn <= log_get_lsn(log));
  ut_ad(log_limits_mutex_own(log));

  ut_a(requested_lsn % OS_FILE_LOG_BLOCK_SIZE >= LOG_BLOCK_HDR_SIZE);

  ut_a(requested_lsn % OS_FILE_LOG_BLOCK_SIZE <
       OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE);

  /* Update log.requested_checkpoint_lsn only to greater value. */

  if (requested_lsn > log.requested_checkpoint_lsn) {
    log.requested_checkpoint_lsn = requested_lsn;

    if (requested_lsn > log.last_checkpoint_lsn.load()) {
      os_event_set(log.checkpointer_event);
    }
  }
}

static void log_wait_for_checkpoint(const log_t &log, lsn_t requested_lsn) {
  ut_d(log_background_threads_active_validate(log));

  auto stop_condition = [&log, requested_lsn](bool) {
    return log.last_checkpoint_lsn.load() >= requested_lsn;
  };

  ut::wait_for(0, std::chrono::microseconds{100}, stop_condition);
}

static bool log_request_checkpoint_validate(const log_t &log) {
  ut_ad(log_limits_mutex_own(log));

#ifdef UNIV_DEBUG
  if (srv_checkpoint_disabled) {
    /* Checkpoints are disabled. Pretend it succeeded. */
    ib::info(ER_IB_MSG_1233) << "Checkpoint explicitly disabled!";

    return false;
  }
#endif /* UNIV_DEBUG */

  return true;
}

void log_request_checkpoint(log_t &log, bool sync) {
  log_update_available_for_checkpoint_lsn(log);

  log_limits_mutex_enter(log);

  if (!log_request_checkpoint_validate(log)) {
    log_limits_mutex_exit(log);
    if (sync) {
      ut_error;
    }
    return;
  }

  const lsn_t lsn = log.available_for_checkpoint_lsn;

  log_request_checkpoint_low(log, lsn);

  log_limits_mutex_exit(log);

  if (sync) {
    log_wait_for_checkpoint(log, lsn);
  }
}

void log_request_checkpoint_in_next_file_low(log_t &log) {
  ut_ad(log_limits_mutex_own(log));
  ut_ad(log_files_mutex_own(log));

  if (!log_request_checkpoint_validate(log)) {
    return;
  }

  const auto oldest_file = log.m_files.begin();
  if (oldest_file == log.m_files.end()) {
    return;
  }

  oldest_file->lsn_validate();

  const lsn_t checkpoint_lsn = log.last_checkpoint_lsn.load();
  ut_a(log_is_data_lsn(checkpoint_lsn));

  const lsn_t current_lsn = log_get_lsn(log);
  ut_a(log_is_data_lsn(current_lsn));

  if (oldest_file->m_end_lsn > checkpoint_lsn &&
      current_lsn >= oldest_file->m_end_lsn) {
    /* LOG_FILE_HDR_SIZE bytes of next file are not counted in the lsn
    sequence, but the LOG_BLOCK_HDR_SIZE bytes of the first log data block
    are counted. Because oldest_file->m_end_lsn % OS_FILE_LOG_BLOCK_SIZE == 0,
    we need to add LOG_BLOCK_HDR_SIZE to build a proper lsn (pointing on data
    byte). */
    const lsn_t request_lsn = oldest_file->m_end_lsn + LOG_BLOCK_HDR_SIZE;
    ut_a(log_is_data_lsn(request_lsn));

    ut_a(current_lsn >= request_lsn);

    DBUG_PRINT("ib_log",
               ("Requesting checkpoint in the next file at LSN " LSN_PF
                " because the oldest file ends at LSN " LSN_PF,
                request_lsn, oldest_file->m_end_lsn));

    log_request_checkpoint_low(log, request_lsn);
  }
}

void log_request_checkpoint_in_next_file(log_t &log) {
  log_limits_mutex_enter(log);
  log_request_checkpoint_in_next_file_low(log);
  log_limits_mutex_exit(log);
}

bool log_request_latest_checkpoint(log_t &log, lsn_t &requested_lsn) {
  const lsn_t lsn = log_get_lsn(log);

  if (lsn <= log.last_checkpoint_lsn.load()) {
    return false;
  }

  log_limits_mutex_enter(log);

  if (!log_request_checkpoint_validate(log)) {
    log_limits_mutex_exit(log);
    ut_error;
  }

  requested_lsn = lsn;

  log_request_checkpoint_low(log, requested_lsn);

  log_limits_mutex_exit(log);

  return true;
}

bool log_make_latest_checkpoint(log_t &log) {
  lsn_t lsn;
  if (!log_request_latest_checkpoint(log, lsn)) {
    return false;
  }

  log_wait_for_checkpoint(log, lsn);

  return true;
}

bool log_make_latest_checkpoint() {
  return log_make_latest_checkpoint(*log_sys);
}

static bool log_request_sync_flush(const log_t &log, lsn_t new_oldest) {
  if (log_test != nullptr) {
    return false;
  }

  /* A flush is urgent: we have to do a synchronous flush,
  because the oldest dirty page is too old.

  Note, that this could fire even if we did not run out
  of space in log files (users still may write to redo). */

  if (new_oldest == LSN_MAX
      /* Forced flush request is processed by page_cleaner, if
      it's not active, then we must do flush ourselves. */
      || !buf_flush_page_cleaner_is_active()
      /* Reason unknown. */
      || srv_is_being_started) {
    buf_flush_sync_all_buf_pools();

    return true;

  } else if (srv_flush_sync) {
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

    The log.m_writer_inside_extra_margin is read without mutex protection for
    performance reasons (not to keep the mutex acquired when waiting below).
    In case of torn read or race, in the worst case we would use different
    timeout than the desired one. It doesn't affect correctness. */

    const auto time_to_wait_ms = log.m_writer_inside_extra_margin ? 1 : 1000;

    os_event_wait_time_low(buf_flush_tick_event,
                           std::chrono::milliseconds{time_to_wait_ms},
                           sig_count);

    return true;

  } else {
    return false;
  }
}

lsn_t log_sync_flush_lsn(log_t &log) {
  /* Note: log.m_allow_checkpoints is set to true after recovery is finished,
  and changes gathered in srv_dict_metadata are applied to dict_table_t
  objects; or in log_start() if recovery was not needed. Until that happens
  checkpoints are disallowed, so sync flush decisions (based on checkpoint age)
  should be postponed. */
  if (!log.m_allow_checkpoints.load(std::memory_order_acquire)) {
    return 0;
  }

  log_update_available_for_checkpoint_lsn(log);

  /* We acquire limits mutex only for a short period. Afterwards these
  values might be changed (advanced to higher values). However, in the
  worst case we would request sync flush for too small value, and the
  function which requests the sync flush is safe to be used with any
  lsn value. It ensures itself that maximum of all requested lsn values
  is taken. In next iteration of log_checkpointer we would notice the
  higher values and re-request the sync flush if needed (or user threads
  waiting in log_free_check() would request it themselves meanwhile). */

  log_limits_mutex_enter(log);
  const lsn_t oldest_lsn = log.available_for_checkpoint_lsn;
  const lsn_t requested_checkpoint_lsn = log.requested_checkpoint_lsn;
  log_limits_mutex_exit(log);

  lsn_t flush_up_to = oldest_lsn;

  lsn_t current_lsn = log_get_lsn(log);

  ut_a(flush_up_to <= current_lsn);

  if (current_lsn == flush_up_to) {
    return 0;
  }

  const lsn_t margin = log_free_check_margin(log);

  const lsn_t adaptive_flush_max_age = log.m_capacity.adaptive_flush_max_age();

  if (current_lsn + margin - oldest_lsn > adaptive_flush_max_age) {
    ut_a(current_lsn + margin > adaptive_flush_max_age);

    flush_up_to = current_lsn + margin - adaptive_flush_max_age;
  }

  if (requested_checkpoint_lsn > flush_up_to) {
    flush_up_to = requested_checkpoint_lsn;
  }

  if (flush_up_to > current_lsn) {
    flush_up_to = current_lsn;
  }

  if (flush_up_to > oldest_lsn) {
    flush_up_to += log_buffer_flush_order_lag(log);

    return flush_up_to;
  }

  return 0;
}

static void log_consider_sync_flush(log_t &log) {
  ut_ad(log_checkpointer_mutex_own(log));

  const auto flush_up_to = log_sync_flush_lsn(log);

  if (flush_up_to != 0) {
    log_checkpointer_mutex_exit(log);

    log_request_sync_flush(log, flush_up_to);

    log_checkpointer_mutex_enter(log);

    /* It's very probable that forced flush will result in maximum
    lsn available for creating a new checkpoint, just try to update
    it to not wait for next checkpointer loop. */
    log_update_available_for_checkpoint_lsn(log);
  }
}

static std::chrono::steady_clock::duration log_checkpoint_time_elapsed(
    const log_t &log) {
  ut_ad(log_checkpointer_mutex_own(log));

  return std::chrono::high_resolution_clock::now() - log.last_checkpoint_time;
}

static bool log_should_checkpoint(log_t &log) {
  lsn_t last_checkpoint_lsn;
  lsn_t oldest_lsn;
  lsn_t current_lsn;
  lsn_t requested_checkpoint_lsn;
  lsn_t checkpoint_age;
  bool periodical_checkpoints_enabled;

  ut_ad(log_checkpointer_mutex_own(log));

#ifdef UNIV_DEBUG
  if (srv_checkpoint_disabled) {
    return false;
  }
#endif /* UNIV_DEBUG */

  /* Note: log.allow_checkpoints is set to true after recovery is finished,
  and changes gathered in srv_dict_metadata are applied to dict_table_t
  objects; or in log_start() if recovery was not needed. We can't reclaim
  free space in redo log until DD dynamic metadata records are safe. */
  if (!log.m_allow_checkpoints.load(std::memory_order_acquire)) {
    return false;
  }

  last_checkpoint_lsn = log.last_checkpoint_lsn.load();

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

  log_limits_mutex_enter(log);
  oldest_lsn = log.available_for_checkpoint_lsn;
  requested_checkpoint_lsn = log.requested_checkpoint_lsn;
  periodical_checkpoints_enabled = log.periodical_checkpoints_enabled;
  log_limits_mutex_exit(log);

  if (oldest_lsn <= last_checkpoint_lsn) {
    return false;
  }

  current_lsn = log_get_lsn(log);

  ut_a(last_checkpoint_lsn <= oldest_lsn);
  ut_a(oldest_lsn <= current_lsn);

  const lsn_t margin = log_free_check_margin(log);

  checkpoint_age = current_lsn + margin - last_checkpoint_lsn;

  /* Update checkpoint_lsn stored in header of log files if:
          a) periodical checkpoints are enabled and either more than 1s
             elapsed since the last checkpoint or checkpoint could be
             written in the next redo log file,
          b) or checkpoint age is greater than aggressive_checkpoint_min_age,
          c) or it was requested to have greater checkpoint_lsn,
             and oldest_lsn allows to satisfy the request. */

  if ((last_checkpoint_lsn < requested_checkpoint_lsn &&
       requested_checkpoint_lsn <= oldest_lsn) ||
      checkpoint_age >= log.m_capacity.aggressive_checkpoint_min_age()) {
    return true;
  }

  DBUG_EXECUTE_IF("periodical_checkpoint_disabled",
                  periodical_checkpoints_enabled = false;);

  if (!periodical_checkpoints_enabled) {
    return false;
  }

  /* Below is the check if a periodical checkpoint should be written. */
  IB_mutex_guard files_lock{&log.m_files_mutex, UT_LOCATION_HERE};

  const auto checkpoint_file = log.m_files.find(last_checkpoint_lsn);
  ut_a(checkpoint_file != log.m_files.end());
  ut_a(!checkpoint_file->m_consumed);

  const auto checkpoint_time_elapsed = log_checkpoint_time_elapsed(log);

  ut_a(last_checkpoint_lsn < checkpoint_file->m_end_lsn);

  return checkpoint_time_elapsed >= get_srv_log_checkpoint_every() ||
         checkpoint_file->m_end_lsn < oldest_lsn;
}

static void log_consider_checkpoint(log_t &log) {
  ut_ad(log_checkpointer_mutex_own(log));

  if (!log_should_checkpoint(log)) {
    return;
  }

  /* It's clear that a new checkpoint should be written.
  So do write back the dynamic metadata. Since the checkpointer
  mutex is low-level one, it has to be released first. */
  log_checkpointer_mutex_exit(log);

  if (log_test == nullptr) {
    dict_persist_to_dd_table_buffer();
  }

  log_checkpointer_mutex_enter(log);

  /* We need to re-check if checkpoint should really be
  written, because we re-acquired the checkpointer_mutex.
  Some conditions could have changed - e.g. user could
  acquire the mutex and specify srv_checkpoint_disabled=T.
  Instead of trying to figure out which conditions could
  have changed, we follow a simple way and perform a full
  re-check of all conditions. */
  if (!log_should_checkpoint(log)) {
    return;
  }

  log_checkpoint(log);
}

void log_checkpointer(log_t *log_ptr) {
  ut_a(log_ptr != nullptr);

  log_t &log = *log_ptr;

  ut_d(log.m_checkpointer_thd = create_internal_thd());

  static const uint64_t log_busy_checkpoint_interval =
      7; /*SRV_MASTER_CHECKPOINT_INTERVAL*/
  auto old_activity_count = srv_get_activity_count();
  ulint error = OS_SYNC_TIME_EXCEEDED;

  for (;;) {
    log_checkpointer_mutex_enter(log);

    const auto sig_count = os_event_reset(log.checkpointer_event);
    const lsn_t requested_checkpoint_lsn = log.requested_checkpoint_lsn;

    bool system_is_busy = false;
    if (error == OS_SYNC_TIME_EXCEEDED &&
        srv_check_activity(old_activity_count)) {
      old_activity_count = srv_get_activity_count();
      /* system is busy. takes longer interval. */
      system_is_busy = true;
    }

    if (error != OS_SYNC_TIME_EXCEEDED || !system_is_busy ||
        requested_checkpoint_lsn >
            log.last_checkpoint_lsn.load(std::memory_order_acquire) ||
        log_checkpoint_time_elapsed(log) >=
            log_busy_checkpoint_interval * get_srv_log_checkpoint_every()) {
      /* Consider flushing some dirty pages. */
      log_consider_sync_flush(log);

      log_sync_point("log_checkpointer_before_consider_checkpoint");

      /* Consider writing checkpoint. */
      log_consider_checkpoint(log);
    }

    log_checkpointer_mutex_exit(log);

    if (requested_checkpoint_lsn >
        log.last_checkpoint_lsn.load(std::memory_order_relaxed)) {
      /* not satisfied. retry. */
      error = 0;
    } else {
      error = os_event_wait_time_low(log.checkpointer_event,
                                     get_srv_log_checkpoint_every(), sig_count);
    }

    /* Check if we should close the thread. */
    if (log.should_stop_threads.load()) {
      ut_ad(!log.writer_threads_paused.load());
      if (!log_flusher_is_active() && !log_writer_is_active()) {
        lsn_t end_lsn = log.write_lsn.load();

        ut_a(log_is_data_lsn(end_lsn));
        ut_a(end_lsn == log.flushed_to_disk_lsn.load());
        ut_a(end_lsn == log_buffer_ready_for_write_lsn(log));

        ut_a(end_lsn >= log_buffer_dirty_pages_added_up_to_lsn(log));

        if (log_buffer_dirty_pages_added_up_to_lsn(log) == end_lsn) {
          /* All confirmed reservations have been written
          to redo and all dirty pages related to those
          writes have been added to flush lists.

          However, there could be user threads, which are
          in the middle of log_buffer_reserve(), reserved
          range of sn values, but could not confirm.

          Note that because log_writer is already not alive,
          the only possible reason guaranteed by its death,
          is that there is x-lock at end_lsn, in which case
          end_lsn separates two regions in log buffer:
          completely full and completely empty. */
          const lsn_t ready_lsn = log_buffer_ready_for_write_lsn(log);

          const lsn_t current_lsn = log_get_lsn(log);

          if (current_lsn > ready_lsn) {
            log.recent_written.validate_no_links(ready_lsn, current_lsn);
            log.recent_closed.validate_no_links(ready_lsn, current_lsn);
          }

          break;
        }
        /* We need to wait until remaining dirty pages
        have been added. */
      }
      /* We prefer to wait until all writing is done. */
    }
  }

  ut_d(destroy_internal_thd(log.m_checkpointer_thd));
}

lsn_t log_get_checkpoint_age(const log_t &log) {
  const lsn_t last_checkpoint_lsn = log.last_checkpoint_lsn.load();

  const lsn_t current_lsn = log_get_lsn(log);

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

/** @} */

/**************************************************/ /**

 @name Log - free check

 *******************************************************/

/** @{ */

sn_t log_concurrency_margin(lsn_t log_capacity, bool &is_safe) {
  /* Add number of background threads that might use mini-transactions
  and modify pages (generating new redo records). */

  /* NOTE: When srv_thread_concurrency = 0 (stands for unlimited thread
  concurrency), we compute the concurrency margin only for the background
  threads. There is no guarantee provided by the log_free_check calls then. */

  const size_t max_total_threads =
      srv_thread_concurrency + LOG_BACKGROUND_THREADS_USING_RW_MTRS;

  /* A thread, which keeps latches of the oldest dirty pages, might
  need to finish its mini-transaction to unlock those pages and allow
  to flush them and advance checkpoint (to reclaim free space in redo).
  Therefore check of free space must be performed when thread is not
  holding latches of pages (or other latches which prevent other threads,
  waiting for such latches, from finishing their mini-transactions).
  Ideally each thread should check for free space, when not holding any
  latches, before it starts next mini-transaction. However, to mitigate
  performance drawbacks, we decided that few (still, limited number)
  mini-transactions could be executed between consecutive such checks.
  Also, each mini-transaction needs to have limited space it might take
  in the redo log. Thanks to that, capacity of redo reserved by single
  thread between its consecutive checks of free space, is limited.
  It is guaranteed not to exceed:
      LOG_CHECKPOINT_FREE_PER_THREAD * UNIV_PAGE_SIZE.
  @note The aforementioned checks of free space are handled by calls to
  log_free_check(). */
  const auto margin_per_thread =
      LOG_CHECKPOINT_FREE_PER_THREAD * UNIV_PAGE_SIZE;

  /* We have guarantee to have at most max_threads concurrent threads.
  Each of them might need the free space reservation for itself, for
  writes between checks (because in the worst case, they could all
  check together there is enough space in the same time, before any
  of them starts to commit any mini-transaction.
  @note This mechanism works only if number of threads is really capped
  by the provided value. However, there is currently no semaphore which
  would ensure that the promise holds. What's more, we actually know that
  it holds only when innodb_thread_concurrency is non-zero (stands for
  limited concurrency). */
  sn_t margin = margin_per_thread * max_total_threads;

  /* Add margin for the log_files_governor, so it could safely use dummy
  log records to fill up the current redo log file if needed (during resize).
  @see LOG_FILES_DUMMY_INTAKE_SIZE */
  margin += LOG_FILES_DUMMY_INTAKE_SIZE;

  /* Add extra safety calculated from redo-size. This is yet another
  "just in case", but being proportional to the total redo capacity. */
  margin += ut_uint64_align_down(
      static_cast<lsn_t>(LOG_EXTRA_CONC_MARGIN_PCT / 100.0 * log_capacity),
      OS_FILE_LOG_BLOCK_SIZE);

  /* If maximum number of concurrent threads is relatively big in comparison
  to the total capacity of redo log, it might happen, that the concurrency
  margin required to avoid deadlocks, is too big. In such case, we use smaller
  margin and report that the margin is unsafe for current concurrency and redo
  capacity. It's up to user to take required steps to protect from deadlock. */

  const auto max_margin = log_translate_lsn_to_sn(ut_uint64_align_down(
      log_capacity *
          (LOG_CONCCURENCY_MARGIN_MAX_PCT + LOG_EXTRA_CONC_MARGIN_PCT) / 100.0,
      OS_FILE_LOG_BLOCK_SIZE));

  if (margin > max_margin) {
    margin = max_margin;
    is_safe = false;
  } else {
    is_safe = true;
  }

  return margin;
}

void log_update_concurrency_margin(log_t &log) {
  ut_ad(srv_is_being_started || log_limits_mutex_own(log));

  const lsn_t log_capacity = log.m_capacity.soft_logical_capacity();

  bool is_safe;
  const sn_t margin = log_concurrency_margin(log_capacity, is_safe);

  log.concurrency_margin.store(margin);
  log.concurrency_margin_is_safe.store(is_safe);

  MONITOR_SET(MONITOR_LOG_CONCURRENCY_MARGIN, margin);
}

void log_update_limits_low(log_t &log) {
  ut_ad(srv_is_being_started || log_limits_mutex_own(log));

  log_update_concurrency_margin(log);

  if (log.m_writer_inside_extra_margin) {
    /* Stop all new incoming user threads at safe place. */
    log.free_check_limit_sn.store(0);
    return;
  }

  const lsn_t log_capacity = log_free_check_capacity(log);

  const lsn_t limit_lsn = log.last_checkpoint_lsn.load() + log_capacity;

  const sn_t limit_sn = log_translate_lsn_to_sn(limit_lsn);

  if (log.free_check_limit_sn.load() < limit_sn) {
    log.free_check_limit_sn.store(limit_sn);
  }
}

void log_set_dict_persist_margin(log_t &log, sn_t margin) {
  log_limits_mutex_enter(log);
  log.dict_persist_margin.store(margin);
  log_update_limits_low(*log_sys);
  log_limits_mutex_exit(log);
}

lsn_t log_free_check_margin(const log_t &log) {
  sn_t margins = log.concurrency_margin.load();

  margins += log.dict_persist_margin.load();

  return log_translate_sn_to_lsn(margins);
}

lsn_t log_free_check_capacity(const log_t &log, lsn_t free_check_margin) {
  ut_ad(srv_is_being_started || log_limits_mutex_own(log));
  const lsn_t soft_logical_capacity = log.m_capacity.soft_logical_capacity();
  ut_a(free_check_margin < soft_logical_capacity);
  return ut_uint64_align_down(soft_logical_capacity - free_check_margin,
                              OS_FILE_LOG_BLOCK_SIZE);
}

lsn_t log_free_check_capacity(const log_t &log) {
  return log_free_check_capacity(log, log_free_check_margin(log));
}

void log_free_check_wait(log_t &log) {
  DBUG_EXECUTE_IF("log_free_check_skip", return;);

  const lsn_t current_lsn = log_get_lsn(log);

  bool request_chkp = true;
#ifdef UNIV_DEBUG
  request_chkp = !srv_checkpoint_disabled;
#endif

  if (request_chkp) {
    log_limits_mutex_enter(log);

    const lsn_t log_capacity = log_free_check_capacity(log);

    if (current_lsn > LOG_START_LSN + log_capacity) {
      log_request_checkpoint_low(log, current_lsn - log_capacity);
    }

    log_limits_mutex_exit(log);
  }

  const sn_t current_sn = log_translate_lsn_to_sn(current_lsn);

  auto stop_condition = [&log, current_sn](bool) {
    return current_sn <= log.free_check_limit_sn.load();
  };

  const auto wait_stats =
      ut::wait_for(0, std::chrono::microseconds{100}, stop_condition);

  MONITOR_INC_WAIT_STATS(MONITOR_LOG_ON_FILE_SPACE_, wait_stats);
}

#ifdef UNIV_DEBUG
void log_free_check_validate() {
  /* This function may be called while holding some latches. This is OK,
  as long as we are not holding any latches on buffer blocks or file spaces.
  The following latches are not held by any thread that frees up redo log
  space. */
  static const latch_level_t latches[] = {
      SYNC_NO_ORDER_CHECK, /* used for non-labeled latches */
      SYNC_RSEGS,          /* rsegs->x_lock in trx_rseg_create() */
      SYNC_UNDO_DDL,       /* undo::ddl_mutex */
      SYNC_UNDO_SPACES,    /* undo::spaces::m_latch */
      SYNC_FTS_CACHE,      /* fts_cache_t::lock */
      SYNC_DICT,           /* dict_sys->mutex in commit_try_rebuild() */
      SYNC_DICT_OPERATION, /* X-latch in commit_try_rebuild() */
      SYNC_INDEX_TREE      /* index->lock */
  };

  sync_allowed_latches check(latches,
                             latches + sizeof(latches) / sizeof(*latches));

  if (sync_check_iterate(check)) {
#ifndef UNIV_NO_ERR_MSGS
    ib::error(ER_IB_MSG_1381)
#else
    ib::error()
#endif
        << "log_free_check() was called while holding an un-listed latch.";
    ut_error;
  }
  mtr_t::check_my_thread_mtrs_are_not_latching();
}
#endif /* !UNIV_DEBUG */

/** @} */

#endif /* !UNIV_HOTBACKUP */
