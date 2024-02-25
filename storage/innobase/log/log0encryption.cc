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
 @file log/log0encryption.cc

 *******************************************************/

#ifndef UNIV_HOTBACKUP

/* std::memcpy, std::memcmp, std::memset */
#include <cstring>

/* log_get_checkpoint_lsn */
#include "log0chkp.h"

#include "log0encryption.h"

/* log_files_mutex_own */
#include "log0files_governor.h"

/* log_encryption_header_{read,write} */
#include "log0files_io.h"

/* log_t::m_encryption_metadata */
#include "log0sys.h"

/* LOG_HEADER_ENCRYPTION_INFO_OFFSET */
#include "log0types.h"

/* log_writer_mutex_own */
#include "log0write.h"

/* Encryption::X */
#include "os0enc.h"

/* srv_force_recovery */
#include "srv0srv.h"

/* IB_mutex_guard */
#include "ut0mutex.h"

/**************************************************/ /**

 @name Log - encryption.

 *******************************************************/

/** @{ */

/** Finds redo log file which has the current log encryption header.
Asserts that the file has been found.
@param[in]  log   redo log
@return iterator to the file containing current log encryption header */
static Log_files_dict::Const_iterator log_encryption_file(const log_t &log) {
  auto file = log.m_files.find(log_get_checkpoint_lsn(log));
  ut_a(file != log.m_files.end());
  return file;
}

dberr_t log_encryption_read(log_t &log) {
  return log_encryption_read(log, *log_encryption_file(log));
}

dberr_t log_encryption_read(log_t &log, const Log_file &file) {
  ut_a(srv_force_recovery < SRV_FORCE_NO_LOG_REDO);
  ut_a(log_sys != nullptr);

  IB_mutex_guard writer_latch{&(log.writer_mutex), UT_LOCATION_HERE};
  IB_mutex_guard files_latch{&(log.m_files_mutex), UT_LOCATION_HERE};
  std::memset(log.m_encryption_buf, 0x00, OS_FILE_LOG_BLOCK_SIZE);

  auto file_handle = file.open(Log_file_access_mode::READ_ONLY);
  ut_a(file_handle.is_open());

  byte log_block_buf[OS_FILE_LOG_BLOCK_SIZE] = {};

  const dberr_t err = log_encryption_header_read(file_handle, log_block_buf);
  if (err != DB_SUCCESS) {
    return DB_ERROR;
  }

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

/** Writes the encryption information into the log encryption header in
the log file containing current checkpoint LSN (log.last_checkpoint_lsn).
Updates: log.m_encryption_buf.
@param[in,out]  log      redo log
@return DB_SUCCESS or DB_ERROR */
static dberr_t log_encryption_write_low(log_t &log) {
  ut_ad(log_files_mutex_own(log));
  ut_ad(log_writer_mutex_own(log));

  byte log_block_buf[OS_FILE_LOG_BLOCK_SIZE];

  if (log_can_encrypt(log)) {
    if (!log_file_header_fill_encryption(log.m_encryption_metadata, true,
                                         log_block_buf)) {
      return DB_ERROR;
    }
  } else {
    std::memset(log_block_buf, 0x00, OS_FILE_LOG_BLOCK_SIZE);
  }

  std::memcpy(log.m_encryption_buf, log_block_buf, OS_FILE_LOG_BLOCK_SIZE);

  auto file_handle =
      log_encryption_file(log)->open(Log_file_access_mode::WRITE_ONLY);
  ut_a(file_handle.is_open());

  return log_encryption_header_write(file_handle, log.m_encryption_buf);
}

bool log_can_encrypt(const log_t &log) {
  return log.m_encryption_metadata.can_encrypt();
}

dberr_t log_encryption_on_master_key_changed(log_t &log) {
  IB_mutex_guard writer_latch{&(log.writer_mutex), UT_LOCATION_HERE};
  IB_mutex_guard files_latch{&(log.m_files_mutex), UT_LOCATION_HERE};

  if (!log_can_encrypt(log)) {
    return DB_SUCCESS;
  }

  /* Re-encrypt log's encryption metadata and write them to disk. */
  return log_encryption_write_low(log);
}

dberr_t log_encryption_generate_metadata(log_t &log) {
  IB_mutex_guard writer_latch{&(log.writer_mutex), UT_LOCATION_HERE};
  IB_mutex_guard files_latch{&(log.m_files_mutex), UT_LOCATION_HERE};

  Encryption_metadata encryption_metadata;

  Encryption::set_or_generate(Encryption::AES, nullptr, nullptr,
                              encryption_metadata);

  log_files_update_encryption(log, encryption_metadata);

  const auto err = log_encryption_write_low(log);
  if (err != DB_SUCCESS) {
    log_files_update_encryption(log, {});
    return err;
  }

  return DB_SUCCESS;
}

/** @} */

#endif /* !UNIV_HOTBACKUP */
