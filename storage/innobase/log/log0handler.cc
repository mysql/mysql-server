/*****************************************************************************

Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
#include "log0handler.h"

#include <cstring>
#include <sstream>

#include "buf0flu.h"  //buf_are_flush_lists_empty_validate
#include "db0err.h"
#include "log0buf.h"
#include "log0chkp.h"       //log_files_next_checkpoint
#include "log0encryption.h" /* log_encryption_read */
#include "log0helpers.h"
#include "log0log.h"
#include "log0pre_8_0_30.h"
#include "log0recv.h"  //recv_sys
#include "log0sys_var_handler.h"
#include "log0test.h"  // log_sync_point
#include "log0types.h"
#include "log0write.h"  //log_write_up_to
#include "srv0srv.h"    //srv_read_only_mode
#include "srv0start.h"  //srv_recovery_crash
#include "ut0new.h"

namespace {
/** Describes location of a single checkpoint. */
struct Log_checkpoint_location {
  /** File containing checkpoint header and checkpoint lsn. */
  Log_file_id m_checkpoint_file_id{0};

  /** Checkpoint header number. */
  Log_checkpoint_header_no m_checkpoint_header_no{};

  /** Checkpoint LSN. */
  lsn_t m_checkpoint_lsn{0};
};
}  // namespace

/** Validates the raw REDO log block buffer.
@param[in]  log_block         REDO log block which contains REDO logs
@param[in]  block_header      REDO log block header
@param[in]  scanned_lsn       Upto which REDO has been scanned
@param[in]  total_scanned_lsn Till the lsn REDO log is scanned
@return true if log block is validated, false otherwise */
[[nodiscard]] static bool log_block_is_valid(const byte *log_block,
                                             Log_data_block_header block_header,
                                             lsn_t scanned_lsn,
                                             lsn_t total_scanned_lsn);

/** Add raw REDO blocks to parsing buffer.
@param[in]  log_block        REDO log block which contains REDO logs
@param[in]  scanned_lsn      Upto which REDO has been scanned
@param[in, out]  ignore_below_lsn Don't add anything to parsing buffer
                                  below this LSN. If a suitable parse start
                                  lsn is found then, set upto which REDO has
                                  been added to the parsing buffer so that
                                  callers could ignore below that LSN if
                                  required.
@param[out] parsing_buffer   A buffer where REDOs to be added
@param[out] parsed_length    Length of the REDOs added to parsing buffer
@return true if all REDOs from the log block are added successfully,
false otherwise */
[[nodiscard]] static bool recv_sys_add_to_parsing_buf(
    const byte *log_block, lsn_t scanned_lsn, lsn_t &ignore_below_lsn,
    ib::redo::Buffer &parsing_buffer, size_t &parsed_length);

#ifndef UNIV_HOTBACKUP
/** Reads a specified log segment to a buffer.
@param[in,out]  log             redo log
@param[in,out]  buf             buffer where to read
@param[in]      start_lsn       read area start
@param[in]      end_lsn         read area end
@return lsn up to which data was available on disk (ideally end_lsn)
or zero in case of error */
[[nodiscard]] static ib::redo::Lsn recv_read_log_seg(log_t &log, byte *buf,
                                                     ib::redo::Lsn start_lsn,
                                                     ib::redo::Lsn end_lsn);

/** Read the checkpoint block from the REDO log.
@param[in]   log    Redo log
@param[out]  block  Block containing checkpoint found
@return DB_SUCCESS if checkpoint found, error otherwise. */
static dberr_t read_checkpoint(
    log_t &log, ib::redo::Handler_interface::Metadata_value &block);

/** Read the REDO header block from the REDO log.
@param[in]   log    Redo log
@param[out]  block  Block containing header info
@return DB_SUCCESS if header read successfully, error otherwise. */
[[nodiscard]] static dberr_t read_header(
    log_t &log, ib::redo::Handler_interface::Metadata_value &block);

/** Write the REDO header block to the REDO log.
@param[in]   log    Redo log
@param[out]  block  Block containing header info
@return DB_SUCCESS if header read successfully, error otherwise. */
[[nodiscard]] static dberr_t write_header(
    log_t &log, const ib::redo::Handler_interface::Metadata_value block);

/** Find the latest checkpoint in the given log file.
@param[in]  file_handle  handle for the opened redo log file
@param[out] checkpoint   the latest checkpoint found (if any)
@return true iff any checkpoint has been found */
[[nodiscard]] static bool recv_find_max_checkpoint(
    Log_file_handle &file_handle, Log_checkpoint_location &checkpoint);

/** Find the latest checkpoint (check all existing redo log files).
@param[in]  log         redo log
@param[out] checkpoint  the latest checkpoint found (if any)
@return true iff any checkpoint has been found */
[[nodiscard]] static bool recv_find_max_checkpoint(
    log_t &log, Log_checkpoint_location &checkpoint);

/** Scans log from a buffer and stores new log data to the destination buffer.
@param[in]      buf                  buffer containing a log segment or garbage
@param[in]      buf_len              buffer length
@param[in]      start_lsn            buffer start lsn
@param[in,out]  total_scanned_lsn    till the lsn REDO log is scanned
@param[out]     parsing_destination  Parsing buffer where RAW REDOs are stored
@return true if reached the end of redo log or not able to scan anymore during
this call. Check parsing_destination.size to see how much was actually read
before reaching the end in such case. */
[[nodiscard]] static bool recv_scan_log_recs(
    const byte *buf, size_t buf_len, ib::redo::Lsn start_lsn,
    ib::redo::Lsn &total_scanned_lsn, ib::redo::Buffer &parsing_destination);

namespace ib::redo {

Handler::~Handler() { log_sys_close(); }

Status Handler::start_reading() {
  auto err = log_sys_init(false);
  switch (err) {
    case DB_CANNOT_OPEN_FILE:
      log_sys_close();
      return Status::COULD_NOT_OPEN;
    case DB_SUCCESS:
      break;
    default:
      ib::error(ER_IB_ERR_INNODB_REDO_HANDLER_FAILURE, __FUNCTION__,
                "log_sys_init()", ut_strerr(err));
      return Status::READ_ERROR;
  }
  /* Log format already verified in log_sys_init() */
  ut_a(log_sys->m_format == Log_format::CURRENT);

  if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {
    Log_checkpoint_location checkpoint;
    if (!recv_find_max_checkpoint(*log_sys, checkpoint)) {
      ib::error(ER_IB_MSG_RECOVERY_CHECKPOINT_NOT_FOUND);
      return Status::READ_ERROR;
    }
    /* The log_read_encryption_info() method, which reads metadata about
    encrypted REDOs, needs to know which file to read. Therefore, the
    checkpoint LSN must be set here. */
    log_checkpointing->set_checkpoint(checkpoint.m_checkpoint_lsn);
  }

  return Status::SUCCESS;
}

Status Handler::create(Lsn start_lsn) {
  /* The caller will assume that start_lsn will be the actual start of the
  created log. OTOH log_files_create wants the log file to start at block
  boundary and the data to start immediately after its header. Together, this
  implies that start_lsn must be at header end. */
  ut_a(start_lsn % OS_FILE_LOG_BLOCK_SIZE == LOG_BLOCK_HDR_SIZE);

  auto err = log_sys_init(true);
  if (err != DB_SUCCESS) {
    ib::error(ER_IB_ERR_INNODB_REDO_HANDLER_FAILURE, __FUNCTION__,
              "log_sys_init()", ut_strerr(err));
    return Status::COULD_NOT_CREATE;
  }
  err = log_files_create(*log_sys, start_lsn);
  if (err != DB_SUCCESS) {
    ib::error(ER_IB_ERR_INNODB_REDO_HANDLER_FAILURE, __FUNCTION__,
              "log_files_create()", ut_strerr(err));
    return Status::COULD_NOT_CREATE;
  }
  return Status::SUCCESS;
}

Status Handler::write_mtr(const Const_buffers &mtr_data, Lsn &start_lsn,
                          Lsn &end_lsn) {
  size_t total_len = 0;
  for (size_t i = 0; i < mtr_data.count; ++i) {
    total_len += mtr_data.buffers[i].size();
  }

  auto handle = log_buffer_reserve(*log_sys, total_len);
  start_lsn = handle.start_lsn;
  end_lsn = handle.end_lsn;

  DBUG_EXECUTE_IF("mtr_filling_redo_block_write", {
    if (start_lsn % OS_FILE_LOG_BLOCK_SIZE == LOG_BLOCK_HDR_SIZE) {
      ib::redo::must_succeed(ib::redo::handler->persist_smaller_than(start_lsn),
                             UT_LOCATION_HERE);
      DBUG_SUICIDE();
    }
  });

  auto lsn = start_lsn;
  for (size_t i = 0; i < mtr_data.count; ++i) {
    auto &block{mtr_data.buffers[i]};

    if (block.size() == 0) {
      continue;
    }

    auto block_end =
        log_buffer_write(*log_sys, block.data(), block.size(), lsn);

    ut_a(lsn % OS_FILE_LOG_BLOCK_SIZE <
         OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE);

    total_len -= block.size();
    const bool is_last_block = (total_len == 0);
    if (is_last_block) {
      /* This write was up to the end of record group,
      the last record in group has been written.

      Therefore next group of records starts at m_lsn.
      We need to find out, if the next group is the first group,
      that starts in this log block.

      In such case we need to set first_rec_group.

      Now, we could have two cases:
      1. This group of log records has started in previous block
         to block containing m_lsn.
      2. This group of log records has started in the same block
         as block containing m_lsn.

      Only in case 1), the next group of records is the first group
      of log records in block containing m_lsn. */
      if (start_lsn / OS_FILE_LOG_BLOCK_SIZE !=
          end_lsn / OS_FILE_LOG_BLOCK_SIZE) {
        ut_a(end_lsn == block_end);
        log_buffer_set_first_record_group(*log_sys, end_lsn);
      }
    }

    log_buffer_write_completed(*log_sys, lsn, block_end, is_last_block);
    lsn = block_end;
  }
  ut_a(total_len == 0);
  ut_a(lsn == end_lsn);
  return Status::SUCCESS;
}

Status Handler::start_writing(Lsn lsn) {
  auto err = log_start(*log_sys, lsn);
  if (err != DB_SUCCESS) {
    ib::error(ER_IB_ERR_INNODB_REDO_HANDLER_FAILURE, __FUNCTION__,
              "log_start()", ut_strerr(err));
    return Status::WRITE_ERROR;
  }
  if (!srv_read_only_mode) {
    log_start_background_threads(*log_sys);
  }
  return Status::SUCCESS;
}
void Handler::stop_writing() { log_stop_background_threads(*log_sys); }

Status Handler::persist_smaller_than(Lsn end_lsn, Durability desired_guarantee,
                                     Origin origin) {
  auto wait_stats = log_write_up_to(
      *log_sys, end_lsn, desired_guarantee == Durability::FULLY_PERSISTED);
  switch (origin) {
    case Origin::PAGE_FLUSHING:
      MONITOR_INC_WAIT_STATS_EX(MONITOR_ON_LOG_, _PAGE_WRITTEN, wait_stats);
      break;
    case Origin::TRX_COMMIT:
      MONITOR_INC_WAIT_STATS(MONITOR_TRX_ON_LOG_, wait_stats);
      break;
    default:
      break;
  }
  return Status::SUCCESS;
}

Lsn Handler::compute_end_lsn(Lsn start_lsn, size_t data_len) const {
  return log_translate_sn_to_lsn(log_translate_lsn_to_sn(start_lsn) + data_len);
}

Lsn Handler::align_down_to_known_boundary(Lsn lsn) {
  constexpr size_t n_block_read = 16;
  constexpr size_t read_size = n_block_read * OS_FILE_LOG_BLOCK_SIZE;
  const Lsn lsn_aligned = ut_uint64_align_down(lsn, OS_FILE_LOG_BLOCK_SIZE);
  Lsn scanned_lsn = lsn_aligned;
  std::array<byte, read_size> local_buf;

  while (1) {
    ut_ad(scanned_lsn ==
          ut_uint64_align_down(scanned_lsn, OS_FILE_LOG_BLOCK_SIZE));
    local_buf.fill(0);

    /* Read the chunk from disk */
    const Lsn end_lsn = recv_read_log_seg(*log_sys, local_buf.data(),
                                          scanned_lsn, scanned_lsn + read_size);

    if (end_lsn == 0) {
      /* Could not read the log block due to some error */
      break;
    }

    /* We must be able to read blocks */
    ut_a_lt(scanned_lsn, end_lsn);
    ut_a_eq(end_lsn % OS_FILE_LOG_BLOCK_SIZE, 0);

    size_t actual_block_read = (end_lsn - scanned_lsn) / OS_FILE_LOG_BLOCK_SIZE;
    ut_ad(actual_block_read > 0 && actual_block_read <= n_block_read);

    byte *log_block = local_buf.data();
    for (size_t i = 0; i < actual_block_read; i++) {
      Log_data_block_header block_header;
      log_data_block_header_deserialize(log_block, block_header);

      DBUG_EXECUTE_IF("invalid_block_for_mtr_boundary", { return 0; });

      /* Passing 0 to make sure epoch validation happens */
      if (!log_block_is_valid(log_block, block_header, scanned_lsn, 0)) {
        /* ERROR condition */
        break;
      }

      if (block_header.m_first_rec_group > 0) {
        const auto parse_start_lsn =
            scanned_lsn + block_header.m_first_rec_group;
        ib::info(ER_IB_MSG_1261, (ulonglong)parse_start_lsn, (ulonglong)lsn,
                 (ulonglong)lsn_aligned);
        return (scanned_lsn + block_header.m_first_rec_group);
      }

      scanned_lsn += block_header.m_data_len;

      /* Look into next block */
      log_block += OS_FILE_LOG_BLOCK_SIZE;
    } /* for */

    ut_ad(actual_block_read == n_block_read);
    ut_ad(scanned_lsn == end_lsn);

    if (actual_block_read < n_block_read || scanned_lsn != end_lsn) {
      /* We couldn't find a block with first_rec_group. Error condition. */
      break;
    }
  } /* while */

  /* ERROR condition */
  return 0;
}

Status Handler::read(Lsn start_lsn, Buffer &buffer) {
  ut_a(log_sys->m_format == Log_format::CURRENT);
  /* start_lsn must point to a data byte */
  ut_a(log_is_data_lsn(start_lsn));
  if (log_sys->m_files.empty() ||
      start_lsn < log_sys->m_files.front().m_start_lsn) {
    /* This case should not occur in regular recovery scenarios, as the caller
    shouldn't ever ask us to read data below the block with checkpoint and by
    the time we've got here it was already asserted align_down_to_known_boundary
    has succeeded. Still, in general the API contract requires us to report
    ALREADY_TRUNCATED, but the code is unreachable in practice, so if we ever
    add a call which covers this case, feel free to remove the ut_error after
    verifying the code hasn't rot over years. */
#ifdef UNIV_DEBUG
    ut_error;
#else
    buffer = buffer.subspan(0, 0);
    return Status::ALREADY_TRUNCATED;
#endif
  }

  /* Reading from disk needs start_lsn to be aligned to BLOCK boundary, but
  we shall store REDOs in parsing buffer only from passed start_lsn. Thus save
  the value of start_lsn before alignment. */
  Lsn total_scanned_lsn = start_lsn;

  Lsn start_lsn_aligned =
      ut_uint64_align_down(start_lsn, OS_FILE_LOG_BLOCK_SIZE);

  /* These LSNs will be skipped and won't be added to parsing buffer from the
  first block. */
  const Lsn skipped_lsn =
      total_scanned_lsn - start_lsn_aligned - LOG_BLOCK_HDR_SIZE;
  ut_a(skipped_lsn != LOG_BLOCK_DATA_SIZE);

  const size_t passed_buffer_size = buffer.size();

  const size_t passed_buffer_size_aligned =
      ut_uint64_align_up(compute_end_lsn(start_lsn, passed_buffer_size),
                         OS_FILE_LOG_BLOCK_SIZE) -
      start_lsn_aligned;
  const size_t read_size =
      std::min((size_t)RECV_SCAN_SIZE, passed_buffer_size_aligned);

  /* A local buffer which will read logs from disk. These logs will have
  header/footer. After scanning this buffer, RAW REDO logs are stored in
  'buffer'  passed. */
  ut::vector<byte> local_buf(read_size);

  size_t total_parsed_length = 0;
  while (1) {
    ut_ad(start_lsn_aligned ==
          ut_uint64_align_down(start_lsn_aligned, OS_FILE_LOG_BLOCK_SIZE));

    std::fill(local_buf.begin(), local_buf.end(), 0);

    /* Read next chunk from disk */
    const Lsn read_end_lsn =
        recv_read_log_seg(*log_sys, local_buf.data(), start_lsn_aligned,
                          start_lsn_aligned + read_size);
    ut_a(read_end_lsn);
    const size_t read_len = read_end_lsn - start_lsn_aligned;

    if (read_len == 0) {
      /* Nothing more read, stop */
      break;
    }

    ut_a_le(total_parsed_length, buffer.size());
    Buffer parse_destination = buffer.subspan(total_parsed_length);

    /* Scan the read buffer and store the raw REDO logs into parsing buffer */
    bool res = recv_scan_log_recs(local_buf.data(), read_len, start_lsn_aligned,
                                  total_scanned_lsn, parse_destination);

    if (parse_destination.size() == 0) {
      /* Nothing more added to parsing buffer */
      break;
    }

    total_parsed_length += parse_destination.size();

    if (res) {
      /* Nothing more to scan */
      break;
    }

    /* Calculate the end_lsn based on the REDO len we read into the buffer
    because we might not have scanned everything read. */
    const Lsn scanned_end_lsn_aligned =
        ut_uint64_align_down(compute_end_lsn(start_lsn, total_parsed_length),
                             OS_FILE_LOG_BLOCK_SIZE);

    ut_a(scanned_end_lsn_aligned <= read_end_lsn);

    /* Continue reading more from disk */
    start_lsn_aligned = scanned_end_lsn_aligned;
  }

  /* In a case, when MTR is completely filling up the REDO block, we need to
  make sure that next BLOCK header is valid. This is because we always need to
  have FIRST_REC_GROUP set for the HEADER for recovery to work in case of crash.
  If next block header is not valid, we don't apply this MTR. But if next BLOCK
  is not read yet, we ignore the last byte of MTR so that this BLOCK is read
  again in next iteration. */
  const size_t data_size_covered = total_parsed_length + skipped_lsn;
  const bool is_last_block_full =
      data_size_covered > 0 && (data_size_covered % LOG_BLOCK_DATA_SIZE == 0);
  bool is_torn = false;
  if (is_last_block_full) {
    /* If you are puzzled why there's == here, instead of >=, notice, that if it
    were indeed >, then the next block would have data in it, so it would be
    "the last block". This is part of the contract with recv_scan_log_recs(),
    that when it is done copying data to the buffer from full blocks, it checks
    the next block's header and if it is valid then it includes first
    LOG_BLOCK_HDR_SIZE bytes of next block in the total_scanned_lsn reported. */
    const bool is_next_header_valid =
        ((total_scanned_lsn % OS_FILE_LOG_BLOCK_SIZE) == LOG_BLOCK_HDR_SIZE);
    is_torn = !is_next_header_valid;
  }
  if (is_torn) {
    DBUG_EXECUTE_IF("read_only_one_block", {
      ib::info() << "\t   Adjusting parsed length from : "
                 << total_parsed_length << " to : " << total_parsed_length - 1;
    });

    ut_a(total_parsed_length > 0);
    total_parsed_length--;
  }

  /* Indicate to the caller the data length added to parsing buffer */
  buffer = buffer.subspan(0, total_parsed_length);

  /* If nothing scanned, declare end of stream */
  return buffer.empty()
             ? (is_torn ? Status::TORN_STREAM_END : Status::STREAM_END)
             : Status::SUCCESS;
}
Lsn Handler::peek_first_unassigned_lsn() { return log_get_lsn(*log_sys); }
Lsn Handler::peek_first_nonpersisted_lsn() {
  return log_sys->flushed_to_disk_lsn.load();
}

Status Handler::persist_available(const Origin &origin) {
  log_sys->recent_written.advance_tail();
  return persist_smaller_than(log_buffer_ready_for_write_lsn(*log_sys),
                              Durability::FULLY_PERSISTED, origin);
}

Status Handler::store_metadata(uint16_t key, const Metadata_value &value) {
  switch ((enum Metadata_key)key) {
    case Metadata_key::HEADER:
      if (write_header(*log_sys, value) != DB_SUCCESS) {
        return Status::WRITE_METADATA_ERROR;
      }
      return Status::SUCCESS;
      break;
    case Metadata_key::CHECKPOINT: {
      Log_checkpoint_header header;
      const bool success =
          log_checkpoint_header_deserialize(value.data(), header);
      ut_a(success);
      if (log_files_next_checkpoint(*log_sys, header.m_checkpoint_lsn) !=
          DB_SUCCESS) {
        return Status::WRITE_METADATA_ERROR;
      }
      return Status::SUCCESS;
    }
    default:
      ut_error;
  }
}
/** Computes concurrency margin to be used within log_free_check calls,
for a given redo log capacity (soft_logical_capacity).
@param[in,out] max_threads
                   The maximum number of concurrently existing threads which use
                   redo-logged mtrs (call write_mtr).
@param[in]     reserved_bytes_per_thread
                   The maximum number of bytes a thread can pass to write_mtr,
                   before calling wait_for_space() again.
@param[in]     log_capacity
                   redo log capacity (soft)
@param[out]    is_safe
                   true iff the computed margin wasn't truncated because of too
                   small log_capacity
@return the computed margin */
static sn_t log_concurrency_margin(const size_t max_threads,
                                   const size_t reserved_bytes_per_thread,
                                   lsn_t log_capacity, bool &is_safe) {
  /* We have guarantee to have at most max_threads concurrent threads.
  Each of them might need the free space reservation for itself, for
  writes between checks (because in the worst case, they could all
  check together there is enough space in the same time, before any or all
  of them starts to commit any mini-transaction.)
  @note This mechanism works only if number of threads is really capped
  by the provided value. However, there is currently no semaphore which
  would ensure that the promise holds. What's more, we actually know that
  it holds only when innodb_thread_concurrency is non-zero (stands for
  limited concurrency). */
  sn_t margin = reserved_bytes_per_thread * max_threads;

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

Handler::Capacity_estimate Handler::get_capacity_estimate() {
  const lsn_t soft_logical_capacity =
      log_sys->m_capacity.soft_logical_capacity();
  bool is_safe;
  const sn_t concurrency_margin =
      log_concurrency_margin(m_max_threads, m_reserved_bytes_per_thread,
                             soft_logical_capacity, is_safe);

  Capacity_estimate estimate;
  estimate.margin_length = log_translate_sn_to_lsn(concurrency_margin);
  estimate.max_history_length = soft_logical_capacity - estimate.margin_length;
  return estimate;
}

bool Handler::update_free_check_limit() {
  ut_ad(log_limits_mutex_own());

  const lsn_t soft_logical_capacity =
      log_sys->m_capacity.soft_logical_capacity();

  bool is_safe;
  const sn_t concurrency_margin =
      log_concurrency_margin(m_max_threads, m_reserved_bytes_per_thread,
                             soft_logical_capacity, is_safe);

  MONITOR_SET(MONITOR_LOG_CONCURRENCY_MARGIN, concurrency_margin);
  lsn_t oldest_needed_lsn;
  auto consumer = log_consumer_get_oldest(*log_sys, oldest_needed_lsn);

  /* --innodb-read-only causes soft_logical_capacity == 0 as nobody is calling
  log.m_capacity.update(..). In such case we exit early to avoid underflows */
  if (soft_logical_capacity == 0) {
    return is_safe;
  }

  const lsn_t free_check_margin = log_translate_sn_to_lsn(concurrency_margin);
  ut_a_lt(free_check_margin, soft_logical_capacity);
  const auto log_free_check_capacity = ut_uint64_align_down(
      soft_logical_capacity - free_check_margin, OS_FILE_LOG_BLOCK_SIZE);

  const lsn_t limit_lsn = log_sys->m_writer_inside_extra_margin
                              ? 0
                              : oldest_needed_lsn + log_free_check_capacity;

  if (log_sys->m_free_check_limit_lsn.load() < limit_lsn) {
    log_sys->m_free_check_limit_lsn.store(limit_lsn);
  }

  /* During the server start, the only consumer here is checkpointer thread.
  We can't call consumption_requested() as it is not permitted to advance the
  checkpoint during recovery. So we skip the consumption_requested() in the
  recovery part. */
  const lsn_t current_lsn = peek_first_unassigned_lsn();
  if (!srv_is_being_started && limit_lsn < current_lsn &&
      log_free_check_capacity < current_lsn) {
    consumer->consumption_requested(current_lsn - log_free_check_capacity);
    if (log_sys->m_THREADS_WAITING_FOR_REDO_throttler.apply()) {
      ib::log_warn(ER_IB_MSG_WAITING_ON_LAGGING_REDO_LOG_CONSUMER,
                   consumer->get_name().c_str(), ulonglong{oldest_needed_lsn});
      log_sync_point("threads_waiting_on_lagging_consumer");
    }
  }
  return is_safe;
}

Status Handler::do_not_need_smaller_than(Lsn /*needed_lsn*/) {
  /* May return false if log_concurrency_margin deems the margin unsafe */
  (void)update_free_check_limit();
  return Status::SUCCESS;
}

bool Handler::reconfigure(size_t max_threads,
                          size_t reserved_bytes_per_thread) {
  m_max_threads = max_threads;
  m_reserved_bytes_per_thread = reserved_bytes_per_thread;
  /* The log_sys == nullptr happens on the first call to reconfigure,
  which happens before create() and start_reading(). In this case we should use
  the soft capacity estimated from sysvars AND also emit an ERROR with suggested
  size of the innodb_redo_log_capacity if config is unsafe */
  if (log_sys == nullptr) {
    bool is_concurrency_margin_safe;
    log_concurrency_margin(
        m_max_threads, m_reserved_bytes_per_thread,
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
            m_max_threads, m_reserved_bytes_per_thread,
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

      return false;
    }
    return true;
  }
  ut_a(log_sys != nullptr);
  ut_ad(!log_limits_mutex_own());
  IB_mutex_guard guard{&log_checkpointing->limits_mutex, UT_LOCATION_HERE};
  return update_free_check_limit();
}

void Handler::wait_for_space() {
  /** We prefer to wait for the space in log file now, because now we
  are not holding any latches of dirty pages (nor any other latches which could
  transitively cause page cleaners to wait) making it possible for the
  checkpointer to advance if space in redo log needs to be reclaimed. */

  if (has_space()) {
    return;
  }
  /* We need to wait, because the concurrency margin could be violated
  if we let all threads to go forward after making this check now. */

  DBUG_EXECUTE_IF("log_free_check_skip", return;);

  const lsn_t current_lsn = peek_first_unassigned_lsn();

  /* We are not sure here if the Log Checkpointer is the most lagging consumer,
  but checking that requires acquiring two mutexes, and is a job of
  update_free_check_limit(), which is only executed infrequently from a few
  background threads, to not cause congestion. Waking up Log Checkpointer
  spuriously is not a big problem - it will check there's nothing to do and go
  to sleep again. However, if the Log Checkpointer actually is the one blocking
  everyone, then the sooner we wake it up, the better, so we don't want to wait
  for background threads to do it. */
  os_event_set(log_checkpointing->m_event);

  auto stop_condition = [current_lsn](bool) {
    return current_lsn <= log_sys->m_free_check_limit_lsn.load();
  };

  const auto wait_stats =
      ut::wait_for(0, std::chrono::microseconds{100}, stop_condition);

  MONITOR_INC_WAIT_STATS(MONITOR_LOG_ON_FILE_SPACE_, wait_stats);
}

bool Handler::has_space() {
  if (srv_read_only_mode) {
    return true;
  }
  return peek_first_unassigned_lsn() <= log_sys->m_free_check_limit_lsn.load();
}

Sys_var_handler_interface &Handler::config_handler() {
  return m_sys_var_handler;
}

Status Handler::get_metadata(uint16_t key, Metadata_value &value) {
  Status result = Status::SUCCESS;

  switch ((enum Metadata_key)key) {
    case Metadata_key::HEADER:
      if (read_header(*log_sys, value) != DB_SUCCESS) {
        result = Status::METADATA_IS_MISSING;
      }
      break;
    case Metadata_key::CHECKPOINT:
      if (read_checkpoint(*log_sys, value) != DB_SUCCESS) {
        result = Status::METADATA_IS_MISSING;
      }
      break;
    default:
      ut_error;
  }
  return result;
}
}  // namespace ib::redo
#else  /* UNIV_HOTBACKUP */
/** Checks if a given log data block could be considered a next valid block,
with regards to the epoch_no it has stored in its header, during the recovery.
@param[in]  log_block_epoch_no  epoch_no of the log data block to check
@param[in]  last_epoch_no       epoch_no of the last data block scanned
@return true iff the provided log block has valid epoch_no */
static bool log_block_epoch_no_is_valid(uint32_t log_block_epoch_no,
                                        uint32_t last_epoch_no) {
  const auto expected_next_epoch_no = last_epoch_no + 1;

  return log_block_epoch_no == last_epoch_no ||
         log_block_epoch_no == expected_next_epoch_no;
}

static void meb_set_parse_start_lsn(Log_data_block_header block_header,
                                    ib::redo::Lsn start_lsn,
                                    ib::redo::Lsn scanned_lsn);
#endif /* UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
ib::redo::Lsn recv_read_log_seg(log_t &log, byte *buf, ib::redo::Lsn start_lsn,
                                const ib::redo::Lsn end_lsn) {
  ut_a(start_lsn < end_lsn);

  /* Find the log file which contains start_lsn */
  auto file = log.m_files.find(start_lsn);

  if (file == log.m_files.end()) {
    /* Missing valid file ! */
    return start_lsn;
  }

  auto file_handle = file->open(Log_file_access_mode::READ_ONLY);
  ut_a(file_handle.is_open());

  do {
    os_offset_t source_offset = file->offset(start_lsn);

    ut_a(end_lsn - start_lsn <= ULINT_MAX);

    os_offset_t len = end_lsn - start_lsn;
    ut_ad(len != 0);

    bool switch_to_next_file = false;

    if (source_offset + len > file->m_size_in_bytes) {
      /* If the above condition is true then len
      (which is unsigned) is > the expression below,
      so the typecast is ok */
      ut_a(file->m_size_in_bytes > source_offset);
      len = file->m_size_in_bytes - source_offset;
      switch_to_next_file = true;
    }

    ++log.n_log_ios;

    const dberr_t err =
        log_data_blocks_read(file_handle, source_offset, len, buf);

    if (err == DB_IO_DECRYPT_FAIL) {
      /* The log block may be encrypted, read and update the log_sys */
      dberr_t err = log_read_encryption_info(*log_sys);
      if (err != DB_SUCCESS) {
        return 0;
      }

      /* Try again */
      err = log_data_blocks_read(file_handle, source_offset, len, buf);
      switch (err) {
        case DB_SUCCESS:
          break;

        case DB_IO_DECRYPT_FAIL:
        case DB_UNSUPPORTED:
          ib::error(ER_IB_MSG_CANT_DECRYPT_REDO_LOG, ulonglong{source_offset},
                    file_handle.file_path().c_str());
          return 0;

        default:
          return 0;
      }
    }

    start_lsn += len;
    buf += len;

    if (switch_to_next_file) {
      auto next_id = file->next_id();

      const auto next_file = log.m_files.file(next_id);

      if (next_file == log.m_files.end() || !next_file->contains(start_lsn)) {
        return start_lsn;
      }

      file_handle.close();

      file = next_file;

      file_handle = file->open(Log_file_access_mode::READ_ONLY);
      ut_a(file_handle.is_open());
    }

  } while (start_lsn != end_lsn);

  ut_a(start_lsn == end_lsn);

  return end_lsn;
}

static dberr_t read_checkpoint(
    log_t &log, ib::redo::Handler_interface::Metadata_value &block) {
  switch (log.m_format) {
    case Log_format::CURRENT:
      break;
    case Log_format::VERSION_5_7_9:
    case Log_format::VERSION_8_0_1:
    case Log_format::VERSION_8_0_3:
    case Log_format::VERSION_8_0_19:
    case Log_format::VERSION_8_0_28: {
      ut_error;
    } break;
    default:
      /* In typical flow, format must have already been verified during
      start_reading(). But the interface spec says it's just an error to call
      get_metadata() or store_metadata() too early, so in release mode report an
      error instead of crashing .*/
      ut_d(ut_error);
      ut_o(recv_sys->found_corrupt_log = true);
      ut_o(return DB_ERROR);
  }

  IB_mutex_guard files_latch{&(log.m_files_mutex), UT_LOCATION_HERE};
  Log_checkpoint_location checkpoint;
  /* Look for the latest checkpoint */
  if (!recv_find_max_checkpoint(log, checkpoint)) {
    ib::error(ER_IB_MSG_RECOVERY_CHECKPOINT_NOT_FOUND);
    return DB_ERROR;
  }

  /* Verify that the checkpoint LSN we have found is correct */
  {
    const auto checkpoint_file = log.m_files.find(checkpoint.m_checkpoint_lsn);

    /* When reading checkpoints from redo log files, error would be reported
    if checkpoint_lsn was outside the redo log file from which it was read,
    and such file would be skipped. If no checkpoint was found because of that,
    then recv_find_max_checkpoint would return false. Therefore here we know
    that InnoDB found a valid checkpoint (for which there is a redo log file
    which contains the checkpoint_lsn). */
    if (checkpoint_file == log.m_files.end()) {
      ut_d(ut_error);
      ut_o(return DB_ERROR);
    }

    const auto file_path =
        log_file_path(log.m_files_ctx, checkpoint_file->m_id);
    ib::info(ER_IB_MSG_LOG_CHECKPOINT_FOUND,
             ulonglong{checkpoint.m_checkpoint_lsn}, file_path.c_str());

    Log_checkpoint_header checkpoint_header;

    auto checkpoint_file_handle =
        checkpoint_file->open(Log_file_access_mode::READ_ONLY);

    if (!checkpoint_file_handle.is_open()) {
      return DB_CANNOT_OPEN_FILE;
    }

    /* Read the header block into output block */
    dberr_t err = log_checkpoint_header_read(checkpoint_file_handle,
                                             checkpoint.m_checkpoint_header_no,
                                             checkpoint_header, block);
    if (err != DB_SUCCESS) {
      return err;
    }

    checkpoint_file_handle.close();

    ut_a(checkpoint.m_checkpoint_lsn == checkpoint_header.m_checkpoint_lsn);
  }

  return DB_SUCCESS;
}

static dberr_t read_header(log_t &log,
                           ib::redo::Handler_interface::Metadata_value &block) {
  switch (log.m_format) {
    case Log_format::CURRENT:
      return log_encryption_read(log, block);
    case Log_format::VERSION_5_7_9:
    case Log_format::VERSION_8_0_1:
    case Log_format::VERSION_8_0_3:
    case Log_format::VERSION_8_0_19:
    case Log_format::VERSION_8_0_28:
      ut_error;
    default:
      /* In typical flow, format must have already been verified during
      start_reading(). But the interface spec says it's just an error to call
      get_metadata() or store_metadata() too early, so in release mode mode
      report an error instead of crashing .*/
      ut_d(ut_error);
      ut_o(recv_sys->found_corrupt_log = true);
      ut_o(return DB_ERROR);
  }
}

static dberr_t write_header(
    log_t &log, const ib::redo::Handler_interface::Metadata_value block) {
  ut_ad(log.m_format == Log_format::CURRENT);

  const bool log_already_encrypted = log_can_encrypt(log);

  /* Update the log file encryption */
  Encryption_metadata encryption_metadata;
  auto log_block_buf = block.data();
  /* Decrypt the encryption information using current_master_key */
  if (!Encryption::decode_encryption_info(
          encryption_metadata,
          log_block_buf + LOG_HEADER_ENCRYPTION_INFO_OFFSET, true)) {
    return DB_ERROR;
  }
  IB_mutex_guard writer_latch{&(log.writer_mutex), UT_LOCATION_HERE};
  IB_mutex_guard files_latch{&(log.m_files_mutex), UT_LOCATION_HERE};

  if (log_already_encrypted) {
    /* If log is already encrypted, it must have come from ROTATE ENCRYPTION.
    Encryption metadata should not have changed. */
    ut_a(log.m_encryption_metadata.match(encryption_metadata));
  } else {
    log_files_update_encryption(log, encryption_metadata);
  }

  const auto err = log_encryption_update_and_write_header(log, block);
  if (err != DB_SUCCESS) {
    log_files_update_encryption(log, {});
    return err;
  }

  return DB_SUCCESS;
}

[[nodiscard]] static bool recv_find_max_checkpoint(
    Log_file_handle &file_handle, Log_checkpoint_location &checkpoint) {
  bool found = false;
  checkpoint = {};
  ib::redo::Handler_interface::Metadata_value temp_block{0};

  for (auto checkpoint_header_no : {Log_checkpoint_header_no::HEADER_1,
                                    Log_checkpoint_header_no::HEADER_2}) {
    Log_checkpoint_header checkpoint_header;

    const dberr_t err = log_checkpoint_header_read(
        file_handle, checkpoint_header_no, temp_block.data());
    if (err != DB_SUCCESS) {
      /* Crash if IO error on read */
      ut_a(err == DB_CORRUPTION);
      continue;
    }

    if (!log_checkpoint_header_deserialize(temp_block.data(),
                                           checkpoint_header)) {
      DBUG_PRINT("ib_log", ("invalid checkpoint " UINT32PF " checksum %lx",
                            uint32_t{to_int(checkpoint_header_no)},
                            ulong{log_block_get_checksum(temp_block.data())}));
      continue;
    }

    const lsn_t checkpoint_lsn = checkpoint_header.m_checkpoint_lsn;
    if (checkpoint_lsn == 0) {
      continue;
    }

    DBUG_PRINT("ib_log", ("checkpoint at " LSN_PF, checkpoint_lsn));

    if (!found || checkpoint_lsn > checkpoint.m_checkpoint_lsn) {
      ut_a(checkpoint_lsn >= LOG_START_LSN);
      found = true;
      checkpoint.m_checkpoint_file_id = file_handle.file_id();
      checkpoint.m_checkpoint_header_no = checkpoint_header_no;
      checkpoint.m_checkpoint_lsn = checkpoint_lsn;
    }
  }

  return found;
}

[[nodiscard]] static bool recv_find_max_checkpoint(
    log_t &log, Log_checkpoint_location &checkpoint) {
  bool found = false;
  checkpoint = {};

  log_files_for_each(log.m_files, [&](const Log_file &file) {
    auto file_handle = file.open(Log_file_access_mode::READ_ONLY);
    ut_a(file_handle.is_open());

    Log_checkpoint_location checkpoint_in_file;

    if (!recv_find_max_checkpoint(file_handle, checkpoint_in_file)) {
      return;
    }

    if (!file.contains(checkpoint_in_file.m_checkpoint_lsn)) {
      const auto file_path = file_handle.file_path();
      ib::error(ER_IB_MSG_RECOVERY_CHECKPOINT_OUTSIDE_LOG_FILE,
                ulonglong{checkpoint_in_file.m_checkpoint_lsn},
                file_path.c_str(), ulonglong{file.m_start_lsn},
                ulonglong{file.m_end_lsn});
      return;
    }

    if (!found ||
        checkpoint_in_file.m_checkpoint_lsn > checkpoint.m_checkpoint_lsn) {
      found = true;
      checkpoint = checkpoint_in_file;
    }
  });

  return found;
}

Log_checkpoint_header_no recv_find_checkpoint_header_no(log_t &log,
                                                        lsn_t checkpoint_lsn) {
  Log_checkpoint_location checkpoint;
  if (recv_find_max_checkpoint(log, checkpoint)) {
    /* In theory the caller may ask for a checkpoint_lsn from any of 2 headers
    of any redo log file, but in practice we know it always asks for the
    maximal one, which we assert here and exploit by reusing
    `recv_find_max_checkpoint` to make implementation shorter. */
    ut_ad(checkpoint.m_checkpoint_lsn == checkpoint_lsn);
    if (checkpoint.m_checkpoint_lsn == checkpoint_lsn) {
      return checkpoint.m_checkpoint_header_no;
    }
  }
  ut_ad(false);
  return Log_checkpoint_header_no::HEADER_1;
}

#else  /* UNIV_HOTBACKUP */

static void meb_set_parse_start_lsn(Log_data_block_header block_header,
                                    ib::redo::Lsn start_lsn,
                                    ib::redo::Lsn scanned_lsn) {
  recv_sys->parse_start_lsn = scanned_lsn + block_header.m_first_rec_group;

  ib::info(ER_IB_MSG_1261, (ulonglong)recv_sys->parse_start_lsn,
           (ulonglong)recv_sys->checkpoint_lsn, (ulonglong)start_lsn);

  if (recv_sys->parse_start_lsn < recv_sys->checkpoint_lsn) {
    ut_a(recv_sys->checkpoint_lsn % OS_FILE_LOG_BLOCK_SIZE +
             LOG_BLOCK_TRL_SIZE <
         OS_FILE_LOG_BLOCK_SIZE);

    ut_a(recv_sys->parse_start_lsn % OS_FILE_LOG_BLOCK_SIZE >=
         LOG_BLOCK_HDR_SIZE);
  }

  recv_sys->scanned_lsn = recv_sys->parse_start_lsn;
  recv_sys->recovered_lsn = recv_sys->parse_start_lsn;

  recv_track_changes_of_recovered_lsn();
}

/** Scans the log segment and n_bytes_scanned is set to the length of valid
log scanned.
@param[in]      buf                     buffer containing log data
@param[in]      buf_len                 data length in that buffer
@param[in,out]  scanned_lsn             LSN of buffer start, we return scanned
lsn
@param[in,out]  scanned_epoch_no        the highest scanned epoch number so far
@param[out]     block_no        highest block no in scanned buffer.
@param[out]     n_bytes_scanned         how much we were able to scan, smaller
than buf_len if log data ended here
@param[out]    has_encrypted_log       set true, if buffer contains encrypted
redo log, set false otherwise */
void meb_scan_log_seg(byte *buf, size_t buf_len, lsn_t *scanned_lsn,
                      uint32_t *scanned_epoch_no, uint32_t *block_no,
                      size_t *n_bytes_scanned, bool *has_encrypted_log) {
  *n_bytes_scanned = 0;
  *has_encrypted_log = false;

  for (auto log_block = buf; log_block < buf + buf_len;
       log_block += OS_FILE_LOG_BLOCK_SIZE) {
    Log_data_block_header block_header;
    log_data_block_header_deserialize(log_block, block_header);
    uint32_t no = block_header.m_hdr_no;
    bool is_encrypted = log_block_get_encrypt_bit(log_block);

    if (is_encrypted) {
      *has_encrypted_log = true;
      return;
    }

    if (no != log_block_convert_lsn_to_hdr_no(*scanned_lsn) ||
        !log_block_checksum_is_ok(log_block)) {
      ib::trace_2() << "Scanned lsn: " << *scanned_lsn << " header no: " << no
                    << " converted no: "
                    << log_block_convert_lsn_to_hdr_no(*scanned_lsn)
                    << " checksum: " << log_block_checksum_is_ok(log_block)
                    << " block epoch no: " << block_header.m_epoch_no;

      /* Garbage or an incompletely written log block */

      log_block += OS_FILE_LOG_BLOCK_SIZE;
      break;
    }

    if (*scanned_epoch_no > 0 &&
        !log_block_epoch_no_is_valid(block_header.m_epoch_no,
                                     *scanned_epoch_no)) {
      /* Garbage from a log buffer flush which was made
      before the most recent database recovery */

      ib::trace_2() << "Scanned ep no: " << *scanned_epoch_no << " block ep no "
                    << block_header.m_epoch_no;

      break;
    }

    const auto data_len = block_header.m_data_len;

    *scanned_epoch_no = block_header.m_epoch_no;
    *scanned_lsn += data_len;

    *n_bytes_scanned += data_len;

    if (data_len < OS_FILE_LOG_BLOCK_SIZE) {
      /* Log data ends here */

      break;
    }
    *block_no = no;
  }
}
#endif /* UNIV_HOTBACKUP */

static bool log_block_is_valid(const byte *log_block,
                               const Log_data_block_header block_header,
                               lsn_t scanned_lsn, lsn_t total_scanned_lsn) {
  const uint32_t expected_hdr_no = log_block_convert_lsn_to_hdr_no(scanned_lsn);
  if (block_header.m_hdr_no != expected_hdr_no) {
    /* Garbage or an incompletely written log block.

    We will not report any error, because this can
    happen when InnoDB was killed while it was
    writing redo log. We simply treat this as an
    abrupt end of the redo log. */

    return false;
  }

  if (!log_block_checksum_is_ok(log_block)) {
    uint32_t checksum1 = log_block_get_checksum(log_block);
    uint32_t checksum2 = log_block_calc_checksum(log_block);
    ib::error(ER_IB_MSG_720, ulong{block_header.m_hdr_no},
              ulonglong{scanned_lsn}, ulong{checksum1}, ulong{checksum2});

    /* Garbage or an incompletely written log block.

    This could be the result of killing the server
    while it was writing this log block. We treat
    this as an abrupt end of the redo log. */

    return false;
  }

  const auto data_len = block_header.m_data_len;
  if (scanned_lsn + data_len > total_scanned_lsn) {
    const uint32_t expected_epoch_no =
        log_block_convert_lsn_to_epoch_no(scanned_lsn);
    if (block_header.m_epoch_no != expected_epoch_no) {
      /* Garbage from a log buffer flush which was made
      before the most recent database recovery */

      return false;
    }
  }

  return true;
}

static bool recv_sys_add_to_parsing_buf(const byte *log_block,
                                        lsn_t scanned_lsn,
                                        lsn_t &ignore_below_lsn,
                                        ib::redo::Buffer &parsing_buffer,
                                        size_t &parsed_length) {
  ut_ad(scanned_lsn >= ignore_below_lsn);
#ifdef UNIV_HOTBACKUP
  if (!recv_sys->parse_start_lsn) {
    /* Cannot start parsing yet because no start point for it found */

    return false;
  }
#else
  ut_ad(recv_sys->parse_start_lsn > 0);
#endif

  size_t data_len = log_block_get_data_len(log_block);

  const lsn_t first_interesting_lsn =
      std::max(recv_sys->parse_start_lsn, ignore_below_lsn);
  if (first_interesting_lsn >= scanned_lsn) {
    return false;
  }

  const size_t more_len = scanned_lsn - first_interesting_lsn;

  ut_ad(data_len >= more_len);

  const auto start_offset =
      std::max((size_t)LOG_BLOCK_HDR_SIZE, data_len - more_len);
  const auto end_offset =
      std::min((size_t)OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE, data_len);

  ut_ad(start_offset <= end_offset);

  if (start_offset == end_offset) {
    return false;
  }

  const size_t new_data_len = end_offset - start_offset;

  /* If the parsing buffer size is smaller than the size of parsed log
  records then fill as much the parsing buffer size is, and adjust parsed
  LSN accordingly. */
  const size_t copy_len =
      std::min(parsing_buffer.size() - parsed_length, new_data_len);

  memcpy(parsing_buffer.data() + parsed_length, log_block + start_offset,
         copy_len);

  parsed_length += copy_len;

#ifdef UNIV_HOTBACKUP
  ut_a_eq(copy_len, new_data_len);
#endif /* UNIV_HOTBACKUP */

  if (copy_len == new_data_len) {
    ignore_below_lsn = scanned_lsn;
    return true;
  }
  ignore_below_lsn = scanned_lsn - data_len + start_offset + copy_len;
  return false;
}

#ifndef UNIV_HOTBACKUP
static bool recv_scan_log_recs(const byte *buf, size_t buf_len,
                               ib::redo::Lsn start_lsn,
                               ib::redo::Lsn &total_scanned_lsn,
                               ib::redo::Buffer &parsing_destination) {
#else  /* !UNIV_HOTBACKUP */
bool meb_scan_log_recs(const byte *buf, size_t buf_len, lsn_t start_lsn,
                       lsn_t *read_upto_lsn) {
#endif /* !UNIV_HOTBACKUP */

  ut_ad(start_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_ad(buf_len % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_ad(buf_len >= OS_FILE_LOG_BLOCK_SIZE);

  const byte *log_block = buf;
  ib::redo::Lsn scanned_lsn = start_lsn;

#ifdef UNIV_HOTBACKUP
  ib::redo::Lsn &total_scanned_lsn = recv_sys->scanned_lsn;
  ib::redo::Buffer buffer{recv_sys->buf + recv_sys->len,
                          recv_sys->buf_len - recv_sys->len};
  ib::redo::Buffer &parsing_destination = buffer;
#endif /* UNIV_HOTBACKUP */

  bool more_data = false;
  size_t parsed_length = 0;
  bool finished = false;
  do {
    ut_ad(!finished);

    /* Read the block header */
    Log_data_block_header block_header;
    log_data_block_header_deserialize(log_block, block_header);

    bool is_valid = true;

    if (!log_block_is_valid(log_block, block_header, scanned_lsn,
                            total_scanned_lsn)) {
      is_valid = false;
    }

    const auto data_len = block_header.m_data_len;

    DBUG_EXECUTE_IF("mtr_filling_redo_block_recovery", {
      /* If MTR filling previous REDO block, this block will have header only.
       */
      if (data_len == LOG_BLOCK_HDR_SIZE) {
        ib::info() << "\t\t     Mimicking corrupted next block header behavior."
                   << " scanned_lsn : " << scanned_lsn;
        is_valid = false;
      }
    });

    if (!is_valid) {
      finished = true;
      break;
    }

#ifdef UNIV_HOTBACKUP
    if (!recv_sys->parse_start_lsn && block_header.m_first_rec_group > 0) {
      /* We found a point from which to start the parsing of log records */
      meb_set_parse_start_lsn(block_header, start_lsn, scanned_lsn);
    }
#endif /* UNIV_HOTBACKUP */
    /* We've successfully validated the block, so at least the header can be
    considered scanned, even if we won't find any data, or won't be able to push
    it into the parsing_destination. This is important, because this might be
    the end of the log, and then it matters if it ends at the block or header
    boundary, as the former means the shutdown wasn't clean and the last byte of
    body can not be considered part of a complete mtr.*/
    total_scanned_lsn =
        std::max(total_scanned_lsn, scanned_lsn + LOG_BLOCK_HDR_SIZE);

    scanned_lsn += data_len;

    if (scanned_lsn > total_scanned_lsn) {
      /* We were able to find more log data: add it to the
      parsing buffer if parse_start_lsn is already non-zero */

#ifdef UNIV_HOTBACKUP
      /* Increase the size of parsing buffer if needed */
      if ((recv_sys->len + parsed_length) + 4 * OS_FILE_LOG_BLOCK_SIZE >=
          recv_sys->buf_len) {
        IF_DEBUG(const size_t recv_sys_len_saved = recv_sys->len);
        if (!recv_sys_resize_buf()) {
          recv_sys->found_corrupt_log = true;

          ib::fatal(UT_LOCATION_HERE,
                    ER_IB_ERR_NOT_ENOUGH_MEMORY_FOR_PARSE_BUFFER)
              << "Insufficient memory for InnoDB parse buffer; want "
              << recv_sys->buf_len;
        } else {
          ut_ad(recv_sys->len == recv_sys_len_saved);
          parsing_destination = {recv_sys->buf + recv_sys->len,
                                 recv_sys->buf_len - recv_sys->len};
        }
      }
#endif /* !UNIV_HOTBACKUP */

      if (!recv_sys->found_corrupt_log) {
        more_data = recv_sys_add_to_parsing_buf(
            log_block, scanned_lsn, total_scanned_lsn, parsing_destination,
            parsed_length);
      }

#ifndef UNIV_HOTBACKUP
      if (!more_data) {
        /* Couldn't add any more into parsing buffer */
        break;
      }

      recv_sys->scanned_epoch_no = block_header.m_epoch_no;
#endif /* UNIV_HOTBACKUP */
    }

    if (data_len < OS_FILE_LOG_BLOCK_SIZE) {
      /* Log data for this group ends here */
      finished = true;
    }

    DBUG_EXECUTE_IF("read_only_one_block", {
      static bool done_reading_one_block = false;
      if (!done_reading_one_block) {
        ib::info() << "\t    Handler::read : Read one block.";
        done_reading_one_block = true;
        finished = true;
      }
    });

    if (finished) {
      break;
    }

    /* Scan the next block */
    log_block += OS_FILE_LOG_BLOCK_SIZE;
  } while (log_block < buf + buf_len);

#ifdef UNIV_HOTBACKUP
  recv_sys->len += parsed_length;
  *read_upto_lsn = scanned_lsn;

  if (more_data && !recv_sys->found_corrupt_log) {
    /* Try to parse more log records but do not apply them. Pass the
    max() as memory threshold so that hash table does not overflow to
    avoid applying the log records */
    const auto err =
        recv_parse_and_apply_log_recs(std::numeric_limits<size_t>::max());
    ut_a(err == DB_SUCCESS);

    /* If 3/4th buffer has been consumed, move remaining data at start */
    if (recv_sys->recovered_offset > recv_sys->buf_len / 4) {
      /* Move parsing buffer data to the buffer start */

      recv_reset_buffer();
    }
  }
#endif /* UNIV_HOTBACKUP */

  parsing_destination = parsing_destination.subspan(0, parsed_length);
  return finished;
}

/** Tracks changes of recovered_lsn and tracks proper values for what
first_rec_group should be for consecutive blocks. Must be called when
recv_sys->recovered_lsn is changed to next lsn pointing at boundary
between consecutive parsed mini-transactions. */
void log_track_changes_of_recovered_lsn(
    ib::redo::Lsn old_recovered_lsn, ib::redo::Lsn new_recovered_lsn,
    ib::redo::Lsn &last_block_first_mtr_boundary) {
  /* If we have already found the first block with mtr beginning there,
  we started to track boundaries between blocks. Since then we track
  all proper values of first_rec_group for consecutive blocks.
  The reason for that is to ensure that the first_rec_group of the last
  block is correct. Even though we do not depend during this recovery
  on that value, it would become important if we crashed later, because
  the last recovered block would become the first used block in redo and
  since then we would depend on a proper value of first_rec_group there.
  The checksums of log blocks should detect if it was incorrect, but the
  checksums might be disabled in the configuration. */
  const auto old_block = old_recovered_lsn / OS_FILE_LOG_BLOCK_SIZE;
  const auto new_block = new_recovered_lsn / OS_FILE_LOG_BLOCK_SIZE;

  if (old_block != new_block) {
    ut_a(new_block > old_block);
    last_block_first_mtr_boundary = new_recovered_lsn;
  }
}

/** Check the 4-byte checksum to the trailer checksum field of a log
block.
@param[in]      block   pointer to a log block
@return whether the checksum matches */
bool log_block_checksum_is_ok(const byte *block) {
  return !srv_log_checksums ||
         log_block_get_checksum(block) == log_block_calc_checksum(block);
}
