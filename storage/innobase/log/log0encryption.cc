/*****************************************************************************

Copyright (c) 1995, 2026, Oracle and/or its affiliates.

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
 @file log/log0encryption.cc

 *******************************************************/

#ifndef UNIV_HOTBACKUP

#include "log0encryption.h"
#include <cstring>     /* std::memcpy, std::memcmp, std::memset */
#include "dict0dict.h" /* dict_sys_t::s_invalid_space_id */
#include "fil0pages_persistence_interface.h" /* pages_persistence */
#include "log0chkp.h"                        /* log_get_checkpoint_lsn */
#include "log0files_governor.h"              /* log_files_mutex_own */
#include "log0files_io.h"          /* log_encryption_header_{read,write} */
#include "log0handler_interface.h" /* Redo Log Handler */
#include "log0sys.h"               /* log_t::m_encryption_metadata */
#include "log0types.h"             /* LOG_HEADER_ENCRYPTION_INFO_OFFSET */
#include "log0write.h"             /* log_writer_mutex_own */
#include "mtr0log.h"               /* mlog_open, mlog_close */
#include "mtr0mtr.h"               /* mtr_t */
#include "os0enc.h"                /* Encryption::* */
#include "srv0srv.h"               /* srv_force_recovery */
#include "ut0mutex.h"              /* IB_mutex_guard */

/**************************************************/ /**

 @name Log - encryption.

 *******************************************************/

/** @{ */

/** Finds redo log file which has the current log encryption header.
Asserts that the file has been found.
@param[in]  log   redo log
@return iterator to the file containing current log encryption header */
static Log_files_dict::Const_iterator log_encryption_file(const log_t &log) {
  ut_ad(mutex_own(&log.m_files_mutex));
  auto file = log.m_files.find(pages_persistence->get_checkpoint_lsn());
  ut_a(file != log.m_files.end());
  return file;
}

using Metadata_key = ib::redo::Metadata_key;
using Metadata_value = ib::redo::Handler_interface::Metadata_value;
using Status = ib::redo::Status;

dberr_t log_encryption_read(log_t &log, Metadata_value &block) {
  ut_a(srv_force_recovery < SRV_FORCE_NO_LOG_REDO);
  ut_a(log_sys != nullptr);

  IB_mutex_guard writer_latch{&(log.writer_mutex), UT_LOCATION_HERE};
  IB_mutex_guard files_latch{&(log.m_files_mutex), UT_LOCATION_HERE};
  std::memset(log.m_encryption_buf, 0x00, OS_FILE_LOG_BLOCK_SIZE);
  auto file_handle =
      log_encryption_file(log)->open(Log_file_access_mode::READ_ONLY);
  ut_a(file_handle.is_open());

  const dberr_t err = log_encryption_header_read(file_handle, block.data());
  if (err != DB_SUCCESS) {
    return DB_ERROR;
  }

  return DB_SUCCESS;
}

dberr_t log_read_encryption_info(log_t &log) {
  /* Read the encryption header to get the encryption information. */
  Metadata_value header_block{'\0'};
  if (ib::redo::handler->get_metadata(Metadata_key::HEADER, header_block) !=
      Status::SUCCESS) {
    return DB_ERROR;
  }

  auto log_block_buf = header_block.data();

  if (Encryption::is_encrypted_with_v3(log_block_buf +
                                       LOG_HEADER_ENCRYPTION_INFO_OFFSET)) {
    /* Make sure the keyring is loaded. */
    if (!Encryption::check_keyring()) {
      ib::error(ER_IB_MSG_1238) << "Redo log was encrypted,"
                                << " but keyring is not loaded.";
      return DB_ERROR;
    }

    Encryption_metadata encryption_metadata;

    if (Encryption::decode_encryption_info(
            encryption_metadata,
            log_block_buf + LOG_HEADER_ENCRYPTION_INFO_OFFSET, true)) {
      log_files_update_encryption(log, encryption_metadata);

      ib::info(ER_IB_MSG_1239) << "Read redo log encryption"
                               << " metadata successful.";

      std::memcpy(log.m_encryption_buf, log_block_buf, OS_FILE_LOG_BLOCK_SIZE);

      return DB_SUCCESS;

    } else {
      ib::error(ER_IB_MSG_1241) << "Cannot read the encryption"
                                   " information in log file header, please"
                                   " check if keyring is loaded.";
      return DB_ERROR;
    }
  }

  return DB_SUCCESS;
}

bool log_file_header_fill_encryption(
    const Encryption_metadata &encryption_metadata, bool encrypt_key,
    byte *buf) {
  byte encryption_info[Encryption::INFO_SIZE];

  if (!Encryption::fill_encryption_info(encryption_metadata, encrypt_key,
                                        encryption_info)) {
    return false;
  }

  static_assert(LOG_HEADER_ENCRYPTION_INFO_OFFSET + Encryption::INFO_SIZE <
                    OS_FILE_LOG_BLOCK_SIZE,
                "Encryption information is too big.");

  std::memset(buf, 0x00, OS_FILE_LOG_BLOCK_SIZE);

  std::memcpy(buf + LOG_HEADER_ENCRYPTION_INFO_OFFSET, encryption_info,
              Encryption::INFO_SIZE);

  return true;
}

bool log_can_encrypt(const log_t &log) {
  return log.m_encryption_metadata.can_encrypt();
}

dberr_t log_encryption_update_and_write_header(log_t &log,
                                               const Metadata_value block) {
  ut_a(log_can_encrypt(log));

  const byte *log_block_buf = block.data();
  std::memcpy(log.m_encryption_buf, log_block_buf, OS_FILE_LOG_BLOCK_SIZE);

  auto file_handle =
      log_encryption_file(log)->open(Log_file_access_mode::WRITE_ONLY);
  ut_a(file_handle.is_open());

  return log_encryption_header_write(file_handle, log.m_encryption_buf);
}

void log_encryption_write_dummy_barrier() {
  ut_ad(!srv_read_only_mode);

  mtr_t mtr;
  mtr_start(&mtr);

  /* We force MTR_LOG_ALL even if global redo logging is disabled because
  creating padding after data written with the current encryption mode is
  crucial before changing that mode. Writing to the redo log in this state is
  unusual but supported: InnoDB does not wait for already started MTRs to
  finish before acknowledging that global redo logging has been disabled.
  Such MTRs can therefore append redo after disablement. An MTR started while
  global redo logging is disabled uses MTR_LOG_NO_REDO, and a direct transition
  from MTR_LOG_NO_REDO to MTR_LOG_ALL is ignored. MTR_LOG_NONE is therefore
  used as an intermediate state. */
  mtr.set_log_mode(mtr_log_t::MTR_LOG_NONE);
  mtr.set_log_mode(mtr_log_t::MTR_LOG_ALL);

  byte *buf;
  const bool allocated = mlog_open(&mtr, OS_FILE_LOG_BLOCK_SIZE, buf);
  ut_a(allocated);

  for (size_t i = 0; i < OS_FILE_LOG_BLOCK_SIZE; ++i) {
    *buf++ = MLOG_DUMMY_RECORD;
    mtr.added_rec();
  }

  mlog_close(&mtr, buf);
  mtr_commit(&mtr);

  /* The encryption mode is selected when the block is written, not when the
  MTR commits. Waiting for the write is sufficient; an fsync is unnecessary. */
  const lsn_t barrier_lsn = mtr.commit_lsn();
  ut_a(barrier_lsn > 0);
  log_write_up_to(*log_sys, barrier_lsn, false);
}

dberr_t log_encryption_generate_metadata() {
  ut_a(ib::redo::handler->get_capabilities().supports_encryption);

  /* generate new encryption info */
  Encryption_metadata encryption_metadata;
  Encryption::set_or_generate(Encryption::AES, nullptr, nullptr,
                              encryption_metadata);

  /* encrypt the log block header with master key */
  Metadata_value header_block{0};
  auto log_block_buf = header_block.data();
  if (!log_file_header_fill_encryption(encryption_metadata, true,
                                       log_block_buf)) {
    ib::error(ER_IB_MSG_LOG_FILES_ENCRYPTION_INIT_FAILED);
    return DB_ERROR;
  }

  if (ib::redo::handler->store_metadata(Metadata_key::HEADER, header_block) !=
      Status::SUCCESS) {
    ib::error(ER_IB_MSG_LOG_FILES_ENCRYPTION_INIT_FAILED);
    return DB_ERROR;
  }

  return DB_SUCCESS;
}

/** @} */

#endif /* !UNIV_HOTBACKUP */
