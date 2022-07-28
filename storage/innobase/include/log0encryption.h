/*****************************************************************************

Copyright (c) 1995, 2022, Oracle and/or its affiliates.

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
 @file include/log0encryption.h

 Redo log - encryption.

 *******************************************************/

#ifndef log0encryption_h
#define log0encryption_h

#ifndef UNIV_HOTBACKUP

/* byte */
#include "univ.i"

/**************************************************/ /**

 @name Log - encryption management.

 *******************************************************/

/** @{ */

/** Writes encryption information to log header.
@param[in]      encryption_metadata   encryption metadata (algorithm, key, iv)
@param[in]      encrypt_key           encrypt with master key
@param[in,out]  buf                   log file encryption header */
bool log_file_header_fill_encryption(
    const Encryption_metadata &encryption_metadata, bool encrypt_key,
    byte *buf);

/** Reads the log encryption header to get the redo log encryption information.
Read is done using the file which contains the current checkpoint_lsn.
Updates: log.m_encryption_metadata and log.m_encryption_buf.
@param[in,out]  log          redo log
@return DB_SUCCESS or DB_ERROR */
dberr_t log_encryption_read(log_t &log);

/** Reads the log encryption header to get the redo log encryption information.
Read is done using the provided file.
Updates: log.m_encryption_metadata and log.m_encryption_buf.
@param[in,out]  log          redo log
@param[in]      file         redo file to read from
@return DB_SUCCESS or DB_ERROR */
dberr_t log_encryption_read(log_t &log, const Log_file &file);

/** Enables the redo log encryption and generates new encryption metadata.
Writes the generated metadata to the log encryption header in the log file
containing the current checkpoint lsn (log.last_checkpoint_lsn).
@param[in,out]  log         redo log
@return DB_SUCCESS or DB_ERROR */
dberr_t log_encryption_generate_metadata(log_t &log);

/** Re-encrypts the redo log's encryption metadata using the current master key
and writes it encrypted to the log encryption header in the log file containing
the current checkpoint lsn (log.last_checkpoint_lsn)).
@remarks This is called after the new master key has been generated.
@return DB_SUCCESS or DB_ERROR */
dberr_t log_encryption_on_master_key_changed(log_t &log);

/** @return true iff redo log is encrypted (checks in-memory metadata in log_t).
 */
bool log_can_encrypt(const log_t &log);

/** @} */

#endif /* !UNIV_HOTBACKUP */

#endif /* !log0encryption_h */
