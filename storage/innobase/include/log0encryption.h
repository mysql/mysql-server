/*****************************************************************************

Copyright (c) 1995, 2026, Oracle and/or its affiliates.

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

/**************************************************/ /**
 @file include/log0encryption.h

 Redo log - encryption.

 *******************************************************/

#ifndef log0encryption_h
#define log0encryption_h

#include "log0handler_interface.h" /* ib::redo::Handler_interface::Metadata_value */
#include "log0sys.h"               /* log_t */
#include "os0enc.h"                /* Encryption_metadata */
#ifndef UNIV_HOTBACKUP

#include "log0log.h" /* log_t */
#include "os0enc.h"  /* Encryption_metadata */
#include "univ.i"    /* byte */

/**************************************************/ /**

 @name Log - encryption management.

 *******************************************************/

/** @{ */

/** Writes encryption information to log header.
@param[in]      encryption_metadata   encryption metadata (algorithm, key, iv)
@param[in]      encrypt_key           encrypt with master key
@param[in,out]  buf                   log file encryption header */
[[nodiscard]] bool log_file_header_fill_encryption(
    const Encryption_metadata &encryption_metadata, bool encrypt_key,
    byte *buf);

/** Reads the log encryption header to get the redo log encryption information.
Read is done using the file which contains the current checkpoint_lsn.
@param[in]  log          redo log
@param[out] block        block to read encryption information into
@return DB_SUCCESS or DB_ERROR */
[[nodiscard]] dberr_t log_encryption_read(
    log_t &log, ib::redo::Handler_interface::Metadata_value &block);

/** Read the log encryption information from Redo Log Handler and update
log.m_encryption_metadata and log.m_encryption_buf.
@param[out] log          redo log
@return DB_SUCCESS or DB_ERROR */
[[nodiscard]] dberr_t log_read_encryption_info(log_t &log);

/** Update in-mem encryption information with the new information and also
write this new information to encryption header.
@param[in,out] log   redo log
@param[in]     block block containing new encryption information
@return DB_SUCCESS or DB_ERROR */
[[nodiscard]] dberr_t log_encryption_update_and_write_header(
    log_t &log, const ib::redo::Handler_interface::Metadata_value block);

/** Generates enough dummy redo to cross a physical redo block boundary and
waits until the barrier is written using the current value of
srv_redo_log_encrypt. */
void log_encryption_write_dummy_barrier();

/** Generate new encryption information for REDO log.
@return DB_SUCCESS or DB_ERROR */
[[nodiscard]] dberr_t log_encryption_generate_metadata();

/** @return true iff redo log is encrypted (checks in-memory metadata in log_t).
 */
[[nodiscard]] bool log_can_encrypt(const log_t &log);

/** @} */

#endif /* !UNIV_HOTBACKUP */

#endif /* !log0encryption_h */
