/*****************************************************************************

Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

/** @file arch/arch0log.cc
 Innodb implementation for log archive

 *******************************************************/

#include "arch0log.h"
#include "clone0clone.h"
#include "log0buf.h"
#include "log0chkp.h"
#include "log0encryption.h"
#include "log0files_governor.h"
#include "log0write.h"
#include "srv0start.h"
#include "ut0mutex.h"

/** Chunk size for archiving redo log */
const uint ARCH_LOG_CHUNK_SIZE = 1024 * 1024;

os_offset_t Log_Arch_Client_Ctx::get_archived_file_size() const {
  return m_group->get_file_size();
}

void Log_Arch_Client_Ctx::get_header_size(uint &header_sz,
                                          uint &trailer_sz) const {
  header_sz = LOG_FILE_HDR_SIZE;
  trailer_sz = OS_FILE_LOG_BLOCK_SIZE;
}

int Log_Arch_Client_Ctx::start(byte *header, uint len) {
  ut_ad(len >= LOG_FILE_HDR_SIZE);

  auto err = arch_log_sys->start(m_group, m_begin_lsn, header, false);

  if (err != 0) {
    return (err);
  }

  m_state = ARCH_CLIENT_STATE_STARTED;

  ib::info(ER_IB_MSG_15) << "Clone Start LOG ARCH : start LSN : "
                         << m_begin_lsn;

  return (0);
}

/** Stop redo log archiving. Exact trailer length is returned as out
parameter which could be less than the redo block size.
@param[out]     trailer redo trailer. Caller must allocate buffer.
@param[in,out]  len     trailer length
@param[out]     offset  trailer block offset
@return error code */
int Log_Arch_Client_Ctx::stop(byte *trailer, uint32_t &len, uint64_t &offset) {
  lsn_t start_lsn;
  lsn_t stop_lsn;

  ut_ad(m_state == ARCH_CLIENT_STATE_STARTED);
  ut_ad(trailer == nullptr || len >= OS_FILE_LOG_BLOCK_SIZE);

  auto err = arch_log_sys->stop(m_group, m_end_lsn, trailer, len);

  start_lsn = m_group->get_begin_lsn();

  start_lsn = ut_uint64_align_down(start_lsn, OS_FILE_LOG_BLOCK_SIZE);
  stop_lsn = ut_uint64_align_down(m_end_lsn, OS_FILE_LOG_BLOCK_SIZE);

  lsn_t file_capacity = m_group->get_file_size();

  file_capacity -= LOG_FILE_HDR_SIZE;

  offset = (stop_lsn - start_lsn) % file_capacity;

  offset += LOG_FILE_HDR_SIZE;

  m_state = ARCH_CLIENT_STATE_STOPPED;

  ib::info(ER_IB_MSG_16) << "Clone Stop  LOG ARCH : end LSN : " << m_end_lsn;

  return (err);
}

/** Get archived data file details
@param[in]      cbk_func        callback called for each file
@param[in]      ctx             callback function context
@return error code */
int Log_Arch_Client_Ctx::get_files(Log_Arch_Cbk *cbk_func, void *ctx) {
  ut_ad(m_state == ARCH_CLIENT_STATE_STOPPED);

  int err = 0;

  auto size = m_group->get_file_size();

  /* Check if the archived redo log is less than one block size. In this
  case we send the data in trailer buffer. */
  auto low_begin = ut_uint64_align_down(m_begin_lsn, OS_FILE_LOG_BLOCK_SIZE);

  auto low_end = ut_uint64_align_down(m_end_lsn, OS_FILE_LOG_BLOCK_SIZE);

  if (low_begin == low_end) {
    err = cbk_func(nullptr, size, 0, ctx);
    return (err);
  }

  /* Get the start lsn of the group */
  auto start_lsn = m_group->get_begin_lsn();
  start_lsn = ut_uint64_align_down(start_lsn, OS_FILE_LOG_BLOCK_SIZE);

  ut_ad(m_begin_lsn >= start_lsn);

  /* Calculate first file index and offset for this client. */
  lsn_t lsn_diff = m_begin_lsn - start_lsn;
  uint64_t capacity = size - LOG_FILE_HDR_SIZE;

  auto idx = static_cast<uint>(lsn_diff / capacity);
  uint64_t offset = lsn_diff % capacity;

  /* Set start lsn to the beginning of file. */
  start_lsn = m_begin_lsn - offset;

  offset += LOG_FILE_HDR_SIZE;
  offset = ut_uint64_align_down(offset, OS_FILE_LOG_BLOCK_SIZE);

  /* Callback with all archive file names that holds the range of log
  data for this client. */
  while (start_lsn < m_end_lsn) {
    char name[MAX_ARCH_LOG_FILE_NAME_LEN];
    m_group->get_file_name(idx, name, MAX_ARCH_LOG_FILE_NAME_LEN);

    idx++;
    start_lsn += capacity;

    /* For last file adjust the size based on end lsn. */
    if (start_lsn >= m_end_lsn) {
      lsn_diff =
          ut_uint64_align_up(start_lsn - m_end_lsn, OS_FILE_LOG_BLOCK_SIZE);
      size -= lsn_diff;
    }

    err = cbk_func(name, size, offset, ctx);

    if (err != 0) {
      break;
    }

    /* offset could be non-zero only for first file. */
    offset = 0;
  }

  return (err);
}

/** Release archived data so that system can purge it */
void Log_Arch_Client_Ctx::release() {
  if (m_state == ARCH_CLIENT_STATE_INIT) {
    return;
  }

  if (m_state == ARCH_CLIENT_STATE_STARTED) {
    uint64_t dummy_offset;
    uint32_t dummy_len = 0;

    /* This is for cleanup in error cases. */
    stop(nullptr, dummy_len, dummy_offset);
  }

  ut_ad(m_state == ARCH_CLIENT_STATE_STOPPED);

  arch_log_sys->release(m_group, false);

  m_group = nullptr;

  m_begin_lsn = LSN_MAX;
  m_end_lsn = LSN_MAX;

  m_state = ARCH_CLIENT_STATE_INIT;
}

os_offset_t Arch_Log_Sys::get_recommended_file_size() const {
  if (log_sys == nullptr) {
    ut_d(ut_error);
    /* This shouldn't be executed, but if there was a bug,
    we would prefer to return some value instead of crash,
    because the archiver must not crash the server. */
    ut_o(return srv_redo_log_capacity_used / LOG_N_FILES);
  }
  return log_sys->m_capacity.next_file_size();
}

void Arch_Log_Sys::update_header(byte *header, lsn_t file_start_lsn,
                                 lsn_t checkpoint_lsn) {
  ut_a(file_start_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);

  /* Copy Header information. */
  Log_file_header file_header;
  file_header.m_format = to_int(Log_format::CURRENT);
  file_header.m_start_lsn = file_start_lsn;
  file_header.m_creator_name = LOG_HEADER_CREATOR_CLONE;
  file_header.m_log_flags = 0;
  file_header.m_log_uuid = m_current_group->get_uuid();
  ut_a(file_header.m_log_uuid > 0);
  log_file_header_serialize(file_header, header);

  /* Update checkpoint headers. */
  Log_checkpoint_header checkpoint_header;
  checkpoint_header.m_checkpoint_lsn = checkpoint_lsn;
  log_checkpoint_header_serialize(checkpoint_header, header + LOG_CHECKPOINT_1);
  log_checkpoint_header_serialize(checkpoint_header, header + LOG_CHECKPOINT_2);

  /* Fill encryption information if needed. */
  if (log_sys == nullptr || !log_can_encrypt(*log_sys)) {
    return;
  }
  byte *dest = header + LOG_ENCRYPTION;
  log_file_header_fill_encryption(log_sys->m_encryption_metadata, false, dest);
}

/** Start redo log archiving.
If archiving is already in progress, the client
is attached to current group.
@param[out]     group           log archive group
@param[out]     start_lsn       start lsn for client
@param[out]     header          redo log header
@param[in]      is_durable      if client needs durable archiving
@return error code */
int Arch_Log_Sys::start(Arch_Group *&group, lsn_t &start_lsn, byte *header,
                        bool is_durable) {
  bool create_new_group = false;

  memset(header, 0, LOG_FILE_HDR_SIZE);

  log_request_checkpoint(*log_sys, true);

  arch_mutex_enter();

  if (m_state == ARCH_STATE_READ_ONLY) {
    arch_mutex_exit();
    return 0;
  }

  /* Wait for idle state, if preparing to idle. */
  if (!wait_idle()) {
    int err = 0;

    if (srv_shutdown_state.load() >= SRV_SHUTDOWN_CLEANUP) {
      err = ER_QUERY_INTERRUPTED;
      my_error(err, MYF(0));
    } else {
      err = ER_INTERNAL_ERROR;
      my_error(err, MYF(0), "Log Archiver wait too long");
    }

    arch_mutex_exit();
    return (err);
  }

  ut_ad(m_state != ARCH_STATE_PREPARE_IDLE);

  if (m_state == ARCH_STATE_ABORT) {
    arch_mutex_exit();
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    return (ER_QUERY_INTERRUPTED);
  }

  /* Start archiver task, if needed. */
  if (m_state == ARCH_STATE_INIT) {
    auto err = start_log_archiver_background();

    if (err != 0) {
      arch_mutex_exit();

      ib::error(ER_IB_MSG_17) << "Could not start Archiver"
                              << " background task";

      return (err);
    }
  }

  /* Start archiving from checkpoint LSN. */
  log_writer_mutex_enter(*log_sys); /* protects log_sys->last_checkpoint_lsn */
  log_files_mutex_enter(*log_sys);  /* protects log_sys->m_files */

  start_lsn = log_sys->last_checkpoint_lsn.load();

  const auto file = log_sys->m_files.find(start_lsn);

  ut_ad(file != log_sys->m_files.end());

  if (file == log_sys->m_files.end()) {
    ib::error(ER_IB_MSG_17) << "Could not start Archiver"
                            << " background task because of"
                            << " unexpected internal error";
    log_files_mutex_exit(*log_sys);
    log_writer_mutex_exit(*log_sys);
    return ER_INTERNAL_ERROR;
  }

  const auto start_index = file->m_id;

  const uint64_t start_offset =
      ut_uint64_align_down(file->offset(start_lsn), OS_FILE_LOG_BLOCK_SIZE);

  /* Need to create a new group if archiving is not in progress. */
  if (m_state == ARCH_STATE_IDLE || m_state == ARCH_STATE_INIT) {
    m_archived_lsn.store(
        ut_uint64_align_down(start_lsn, OS_FILE_LOG_BLOCK_SIZE));
    create_new_group = true;
  }

  /* Set archiver state to active. */
  if (m_state != ARCH_STATE_ACTIVE) {
    update_state_low(ARCH_STATE_ACTIVE);
    os_event_set(log_archiver_thread_event);
  }

  log_files_mutex_exit(*log_sys);
  log_writer_mutex_exit(*log_sys);

  /* Create a new group. */
  if (create_new_group) {
    m_current_group =
        ut::new_withkey<Arch_Group>(ut::make_psi_memory_key(mem_key_archive),
                                    start_lsn, LOG_FILE_HDR_SIZE, &m_mutex);

    if (m_current_group == nullptr) {
      arch_mutex_exit();

      my_error(ER_OUTOFMEMORY, MYF(0), sizeof(Arch_Group));
      return (ER_OUTOFMEMORY);
    }

    const Arch_group_uuid uuid{log_generate_uuid()};

    auto db_err =
        m_current_group->init_file_ctx(ARCH_DIR, ARCH_LOG_DIR, ARCH_LOG_FILE, 0,
                                       get_recommended_file_size(), uuid);

    if (db_err != DB_SUCCESS) {
      arch_mutex_exit();

      my_error(ER_OUTOFMEMORY, MYF(0), sizeof(Arch_File_Ctx));

      return (ER_OUTOFMEMORY);
    }

    m_start_log_index = start_index;
    m_start_log_offset = start_offset;

    m_chunk_size = ARCH_LOG_CHUNK_SIZE;

    m_group_list.push_back(m_current_group);
  }

  /* Attach to the current group. */
  m_current_group->attach(is_durable);

  group = m_current_group;

  arch_mutex_exit();

  /* Update header with checkpoint LSN. Note, that arch mutex is released
  and m_current_group should no longer be accessed. The group cannot be
  freed as we have already attached to it. */
  const lsn_t file_start_lsn =
      ut_uint64_align_down(group->get_begin_lsn(), OS_FILE_LOG_BLOCK_SIZE);

  update_header(header, file_start_lsn, start_lsn);

  return (0);
}

#ifdef UNIV_DEBUG
void Arch_Group::adjust_end_lsn(lsn_t &stop_lsn, uint32_t &blk_len) {
  stop_lsn = ut_uint64_align_down(get_begin_lsn(), OS_FILE_LOG_BLOCK_SIZE);

  stop_lsn += get_file_size() - LOG_FILE_HDR_SIZE;
  blk_len = 0;

  /* Increase Stop LSN 64 bytes ahead of file end not exceeding
  redo block size. */
  DBUG_EXECUTE_IF(
      "clone_arch_log_extra_bytes", blk_len = OS_FILE_LOG_BLOCK_SIZE;
      stop_lsn += 64; stop_lsn = std::min(stop_lsn, log_get_lsn(*log_sys)););
}

void Arch_Group::adjust_copy_length(lsn_t arch_lsn, uint32_t &copy_len) {
  lsn_t end_lsn = LSN_MAX;
  uint32_t blk_len = 0;
  adjust_end_lsn(end_lsn, blk_len);

  if (end_lsn <= arch_lsn) {
    copy_len = 0;
    return;
  }

  /* Adjust if copying beyond end LSN. */
  auto len_left = end_lsn - arch_lsn;
  len_left = ut_uint64_align_down(len_left, OS_FILE_LOG_BLOCK_SIZE);

  if (len_left < copy_len) {
    copy_len = static_cast<uint32_t>(len_left);
  }
}

#endif /* UNIV_DEBUG */

/** Stop redo log archiving.
If other clients are there, the client is detached from
the current group.
@param[out]     group           log archive group
@param[out]     stop_lsn        stop lsn for client
@param[out]     log_blk         redo log trailer block
@param[in,out]  blk_len         length in bytes
@return error code */
int Arch_Log_Sys::stop(Arch_Group *group, lsn_t &stop_lsn, byte *log_blk,
                       uint32_t &blk_len) {
  int err = 0;
  blk_len = 0;
  stop_lsn = m_archived_lsn.load();

  if (log_blk != nullptr) {
    /* Get the current LSN and trailer block. */
    log_buffer_get_last_block(*log_sys, stop_lsn, log_blk, blk_len);

    DBUG_EXECUTE_IF("clone_arch_log_stop_file_end",
                    group->adjust_end_lsn(stop_lsn, blk_len););

    /* Will throw error, if shutdown. We still continue
    with detach but return the error. */
    err = wait_archive_complete(stop_lsn);
  }

  arch_mutex_enter();

  if (m_state == ARCH_STATE_READ_ONLY) {
    arch_mutex_exit();
    return 0;
  }

  auto count_active_client = group->detach(stop_lsn, nullptr);
  ut_ad(group->is_referenced());

  if (!group->is_active() && err == 0) {
    /* Archiving for the group has already stopped. */
    my_error(ER_INTERNAL_ERROR, MYF(0), "Clone: Log Archiver failed");
    err = ER_INTERNAL_ERROR;
  }

  if (group->is_active() && count_active_client == 0) {
    /* No other active client. Prepare to get idle. */
    if (m_state == ARCH_STATE_ACTIVE) {
      /* The active group must be the current group. */
      ut_ad(group == m_current_group);
      update_state(ARCH_STATE_PREPARE_IDLE);
      os_event_set(log_archiver_thread_event);
    }
  }

  arch_mutex_exit();

  return (err);
}

void Arch_Log_Sys::force_abort() {
  lsn_t lsn_max = LSN_MAX; /* unused */
  uint to_archive = 0;     /* unused */
  check_set_state(true, &lsn_max, &to_archive);
  /* Above line changes state to ARCH_STATE_PREPARE_IDLE or ARCH_STATE_ABORT.
  Let us notify the background thread to give it chance to notice the change and
  wait for it to transition to ARCH_STATE_IDLE before returning (in case of
  ARCH_STATE_ABORT, wait_idle() does nothing).*/
  arch_mutex_enter();
  wait_idle();
  arch_mutex_exit();
}

/** Release the current group from client.
@param[in]      group           group the client is attached to
@param[in]      is_durable      if client needs durable archiving */
void Arch_Log_Sys::release(Arch_Group *group, bool is_durable) {
  arch_mutex_enter();

  group->release(is_durable);

  /* Check if there are other references or archiving is still
  in progress. */
  if (group->is_referenced() || group->is_active()) {
    arch_mutex_exit();
    return;
  }

  /* Cleanup the group. */
  ut_ad(group != m_current_group);

  m_group_list.remove(group);

  ut::delete_(group);

  arch_mutex_exit();
}

/** Check and set log archive system state and output the
amount of redo log available for archiving.
@param[in]      is_abort        need to abort
@param[in,out]  archived_lsn    LSN up to which redo log is archived
@param[out]     to_archive      amount of redo log to be archived */
Arch_State Arch_Log_Sys::check_set_state(bool is_abort, lsn_t *archived_lsn,
                                         uint *to_archive) {
  auto is_shutdown = (srv_shutdown_state.load() == SRV_SHUTDOWN_LAST_PHASE ||
                      srv_shutdown_state.load() == SRV_SHUTDOWN_EXIT_THREADS);

  auto need_to_abort = (is_abort || is_shutdown);

  *to_archive = 0;

  arch_mutex_enter();

  switch (m_state) {
    case ARCH_STATE_ACTIVE:

      if (*archived_lsn != LSN_MAX) {
        /* Update system archived LSN from input */
        ut_ad(*archived_lsn >= m_archived_lsn.load());
        m_archived_lsn.store(*archived_lsn);
      } else {
        /* If input is not initialized,
        set from system archived LSN */
        *archived_lsn = m_archived_lsn.load();
      }

      lsn_t lsn_diff;

      /* Check redo log data ready to archive. */
      ut_ad(log_sys->write_lsn.load() >= m_archived_lsn.load());

      lsn_diff = log_sys->write_lsn.load() - m_archived_lsn.load();

      lsn_diff = ut_uint64_align_down(lsn_diff, OS_FILE_LOG_BLOCK_SIZE);

      /* Adjust archive data length if bigger than chunks size. */
      if (lsn_diff < m_chunk_size) {
        *to_archive = static_cast<uint>(lsn_diff);
      } else {
        *to_archive = m_chunk_size;
      }

      if (!need_to_abort) {
        break;
      }

      if (!is_shutdown) {
        ut_ad(is_abort);
        /* If caller asked to abort, move to prepare idle state. Archiver
        thread will move to IDLE state eventually. */
        update_state(ARCH_STATE_PREPARE_IDLE);
        break;
      }
      [[fallthrough]];

    case ARCH_STATE_PREPARE_IDLE: {
      /* No active clients. Mark the group inactive and move
      to idle state. */
      m_current_group->disable(m_archived_lsn.load());

      /* If no client reference, free the group. */
      if (!m_current_group->is_referenced()) {
        m_group_list.remove(m_current_group);

        ut::delete_(m_current_group);
      }

      m_current_group = nullptr;

      update_state(ARCH_STATE_IDLE);
    }
      [[fallthrough]];

    case ARCH_STATE_IDLE:
    case ARCH_STATE_INIT:

      /* Abort archiver thread only in case of shutdown. */
      if (is_shutdown) {
        update_state(ARCH_STATE_ABORT);
      }
      break;

    case ARCH_STATE_ABORT:
      /* We could abort archiver from log_writer when
      it is already in the aborted state (shutdown). */
      break;

    default:
      ut_d(ut_error);
  }

  auto ret_state = m_state;
  arch_mutex_exit();

  return (ret_state);
}

dberr_t Arch_Log_Sys::copy_log(Arch_File_Ctx *file_ctx, lsn_t start_lsn,
                               uint length) {
  dberr_t err = DB_SUCCESS;

  if (file_ctx->is_closed()) {
    /* Open system redo log file context */
    err =
        file_ctx->open(true, LSN_MAX, m_start_log_index, m_start_log_offset, 0);
    if (err != DB_SUCCESS) {
      return err;
    }
  }

  Arch_Group *curr_group;
  uint write_size;

  curr_group = arch_log_sys->get_arch_group();

  /* Copy log data into one or more files in archiver group. */
  while (length > 0) {
    uint64_t len_copy;
    uint64_t len_left;

    len_copy = static_cast<uint64_t>(length);

    len_left = file_ctx->bytes_left();

    /* Current file is over, switch to next file. */
    if (len_left == 0) {
      err = file_ctx->open_next(LSN_MAX, LOG_FILE_HDR_SIZE, 0);
      if (err != DB_SUCCESS) {
        return (err);
      }

      len_left = file_ctx->bytes_left();

      ut_ad(len_left > 0);
    }

    if (len_left == 0) {
      return DB_ERROR;
    }

    /* Write as much as possible from current file. */
    if (len_left < len_copy) {
      write_size = static_cast<uint>(len_left);
    } else {
      write_size = length;
    }

    auto get_header_cbk = [start_lsn, this](uint64_t start_offset,
                                            byte *header) {
      ut_ad(start_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);
      ut_ad(start_offset % OS_FILE_LOG_BLOCK_SIZE == 0);

      /* Assertions  above verify that the conditions below shouldn't hold.
      However, they are only for debug binary. The release binary must not
      crash in the Archiver. */
      if (start_lsn % OS_FILE_LOG_BLOCK_SIZE != 0 ||
          start_offset % OS_FILE_LOG_BLOCK_SIZE != 0) {
        return DB_ERROR;
      }

      /* Do not store checkpoint_lsn inside archived log files, because these
      files become later copied by possibly multiple readers and each of such
      copies would possibly be started at different checkpoint lsn and after
      all data >= checkpoint_lsn is copied, the valid checkpoint header would
      anyway be written to the first file created in the copy. Therefore the
      checkpoint_lsn is irrelevant for headers of archived log files. */
      update_header(header, start_lsn + start_offset, 0);
      return DB_SUCCESS;
    };

    err = curr_group->write_to_file(file_ctx, nullptr, write_size, false, false,
                                    get_header_cbk);

    if (err != DB_SUCCESS) {
      return (err);
    }

    ut_ad(length >= write_size);
    length -= write_size;
    start_lsn += write_size;
  }

  return (DB_SUCCESS);
}

bool Arch_Log_Sys::wait_idle() {
  ut_ad(mutex_own(&m_mutex));

  if (m_state == ARCH_STATE_PREPARE_IDLE) {
    os_event_set(log_archiver_thread_event);
    bool is_timeout = false;
    int alert_count = 0;

    auto err = Clone_Sys::wait_default(
        [&](bool alert, bool &result) {
          ut_ad(mutex_own(&m_mutex));
          result = (m_state == ARCH_STATE_PREPARE_IDLE);

          if (srv_shutdown_state.load() >= SRV_SHUTDOWN_CLEANUP) {
            return (ER_QUERY_INTERRUPTED);
          }

          if (result) {
            os_event_set(log_archiver_thread_event);

            /* Print messages every 1 minute - default is 5 seconds. */
            if (alert && ++alert_count == 12) {
              alert_count = 0;
              ib::info(ER_IB_MSG_24) << "Log Archiving start: waiting for "
                                        "idle state.";
            }
          }
          return (0);
        },
        &m_mutex, is_timeout);

    if (err == 0 && is_timeout) {
      err = ER_INTERNAL_ERROR;
      ib::info(ER_IB_MSG_25) << "Log Archiving start: wait for idle state "
                                "timed out";
      ut_d(ut_error);
    }

    if (err != 0) {
      return (false);
    }
  }
  return (true);
}

/** Wait for redo log archive up to the target LSN.
We need to wait till current log sys LSN during archive stop.
@param[in]      target_lsn      target archive LSN to wait for
@return error code */
int Arch_Log_Sys::wait_archive_complete(lsn_t target_lsn) {
  target_lsn = ut_uint64_align_down(target_lsn, OS_FILE_LOG_BLOCK_SIZE);

  /* Check and wait for archiver thread if needed. */
  if (m_archived_lsn.load() < target_lsn) {
    os_event_set(log_archiver_thread_event);

    bool is_timeout = false;
    int alert_count = 0;

    auto err = Clone_Sys::wait_default(
        [&](bool alert, bool &result) {
          /* Read consistent state. */
          arch_mutex_enter();
          auto state = m_state;
          arch_mutex_exit();

          /* Check if we need to abort. */
          if (state == ARCH_STATE_ABORT ||
              srv_shutdown_state.load() >= SRV_SHUTDOWN_CLEANUP) {
            my_error(ER_QUERY_INTERRUPTED, MYF(0));
            return (ER_QUERY_INTERRUPTED);
          }

          if (state == ARCH_STATE_IDLE || state == ARCH_STATE_PREPARE_IDLE) {
            my_error(ER_INTERNAL_ERROR, MYF(0), "Clone: Log Archiver failed");
            return (ER_INTERNAL_ERROR);
          }

          ut_ad(state == ARCH_STATE_ACTIVE);

          /* Check if archived LSN is behind target. */
          auto archived_lsn = m_archived_lsn.load();
          result = (archived_lsn < target_lsn);

          /* Trigger flush if needed */
          auto flush = log_sys->write_lsn.load() < target_lsn;

          if (result) {
            /* More data needs to be archived. */
            os_event_set(log_archiver_thread_event);

            /* Write system redo log if needed. */
            if (flush) {
              log_write_up_to(*log_sys, target_lsn, false);
            }
            /* Print messages every 1 minute - default is 5 seconds. */
            if (alert && ++alert_count == 12) {
              alert_count = 0;
              ib::info(ER_IB_MSG_18)
                  << "Clone Log archive stop: waiting for archiver to "
                     "finish archiving log till LSN: "
                  << target_lsn << " Archived LSN: " << archived_lsn;
            }
          }
          return (0);
        },
        nullptr, is_timeout);

    if (err == 0 && is_timeout) {
      ib::info(ER_IB_MSG_19) << "Clone Log archive stop: "
                                "wait for Archiver timed out";

      err = ER_INTERNAL_ERROR;
      my_error(ER_INTERNAL_ERROR, MYF(0), "Clone: Log Archiver wait too long");
      ut_d(ut_error);
    }
    return (err);
  }
  return (0);
}

/** Archive accumulated redo log in current group.
This interface is for archiver background task to archive redo log
data by calling it repeatedly over time.
@param[in, out] init            true when called the first time; it will then
                                be set to false
@param[in]      curr_ctx        system redo logs to copy data from
@param[out]     arch_lsn        LSN up to which archiving is completed
@param[out]     wait            true, if no more redo to archive
@return true, if archiving is aborted */
bool Arch_Log_Sys::archive(bool init, Arch_File_Ctx *curr_ctx, lsn_t *arch_lsn,
                           bool *wait) {
  Arch_State curr_state;
  uint32_t arch_len;

  dberr_t err = DB_SUCCESS;
  bool is_abort = false;

  /* Initialize system redo log file context first time. */
  if (init) {
    /* We will use curr_ctx to read data from existing log files.
    We set the limit for number of files as the biggest value to
    avoid any such limitation in practice. */
    const auto path = log_directory_path(log_sys->m_files_ctx);

    err = curr_ctx->init(path.c_str(), nullptr, LOG_FILE_BASE_NAME,
                         std::numeric_limits<uint>::max());

    if (err != DB_SUCCESS) {
      is_abort = true;
    }
  }

  /* Find archive system state and amount of log data to archive. */
  curr_state = check_set_state(is_abort, arch_lsn, &arch_len);

  if (curr_state == ARCH_STATE_ACTIVE) {
    /* Adjust archiver length to no go beyond file end. */
    DBUG_EXECUTE_IF("clone_arch_log_stop_file_end",
                    m_current_group->adjust_copy_length(*arch_lsn, arch_len););

    /* Simulate archive error. */
    DBUG_EXECUTE_IF("clone_redo_no_archive", arch_len = 0;);

    if (arch_len == 0) {
      /* Nothing to archive. Need to wait. */
      *wait = true;
      return (false);
    }

    /* Copy data from system redo log files to archiver files */
    err = copy_log(curr_ctx, *arch_lsn, arch_len);

    /* Simulate archive error. */
    DBUG_EXECUTE_IF("clone_redo_archive_error", err = DB_ERROR;);

    if (err == DB_SUCCESS) {
      *arch_lsn += arch_len;
      *wait = false;
      return (false);
    }

    /* Force abort in case of an error archiving data. */
    curr_state = check_set_state(true, arch_lsn, &arch_len);
  }

  if (curr_state == ARCH_STATE_ABORT) {
    curr_ctx->close();
    return (true);
  }

  if (curr_state == ARCH_STATE_IDLE || curr_state == ARCH_STATE_INIT) {
    curr_ctx->close();
    *arch_lsn = LSN_MAX;
    *wait = true;
    return (false);
  }

  ut_ad(curr_state == ARCH_STATE_PREPARE_IDLE);
  *wait = false;
  return (false);
}

void Arch_Log_Sys::update_state(Arch_State state) {
  log_t &log = *log_sys;
  IB_mutex_guard writer_latch{&(log.writer_mutex), UT_LOCATION_HERE};
  IB_mutex_guard files_latch{&(log.m_files_mutex), UT_LOCATION_HERE};
  update_state_low(state);
}

void Arch_Log_Sys::update_state_low(Arch_State state) {
  log_t &log = *log_sys;
  ut_ad(log_writer_mutex_own(log));
  ut_ad(log_files_mutex_own(log));

  const bool was_active = is_active();
  m_state = state;
  const bool is_active_now = is_active();

  if (was_active && !is_active_now) {
    // De-register - transiting to active state
    log_consumer_unregister(log, &m_log_consumer);
  } else if (!was_active && is_active_now) {
    // Register - transitioning inactive state
    log_consumer_register(log, &m_log_consumer);
  }
}

const std::string &Arch_log_consumer::get_name() const {
  static std::string name{"log_archiver"};
  return name;
}

lsn_t Arch_log_consumer::get_consumed_lsn() const {
  ut_a(arch_log_sys != nullptr);
  ut_a(arch_log_sys->is_active());

  lsn_t archiver_lsn = arch_log_sys->get_archived_lsn();
  ut_a(archiver_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);

  return archiver_lsn;
}

void Arch_log_consumer::consumption_requested() { arch_wake_threads(); }
