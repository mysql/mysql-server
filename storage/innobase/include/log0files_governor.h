/*****************************************************************************

Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
 @file include/log0files_governor.h

Redo log management of log files.

 *******************************************************/

#ifndef log0files_governor_h
#define log0files_governor_h

/* Log_files_dict */
#include "log0files_dict.h"

/* Log_files_find_result */
#include "log0files_finder.h"

/* log_t& */
#include "log0types.h"

/* os_offset_t */
#include "os0file.h"

/* srv_threads */
#include "srv0srv.h"

#ifndef UNIV_HOTBACKUP

/** Checks if log file governor thread is active.
@return true if and only if the log checkpointer thread is active */
inline bool log_files_governor_is_active() {
  return srv_thread_is_active(srv_threads.m_log_files_governor);
}

#define log_files_mutex_enter(log) mutex_enter(&((log).m_files_mutex))

#define log_files_mutex_exit(log) mutex_exit(&((log).m_files_mutex))

#define log_files_mutex_own(log) mutex_own(&((log).m_files_mutex))

#endif /* !UNIV_HOTBACKUP */

/** Creates a new set of redo log files.
@remarks
Before creating the new log files, function asserts that there are no existing
log files in the directory specified for the redo log.
The flushed_lsn should be exactly at the beginning of its block body.
The new set of log files starts at the beginning of this block.
Checkpoint is written LOG_BLOCK_HDR_SIZE bytes after the beginning of that
block, that is at flushed_lsn.
The set is marked as initialized if this call succeeded and information about
the initial LSN is emitted to the error log. In such case, a new value is
generated and assigned to the log.m_log_uuid (identifying the new log files).
@param[in,out]  log             redo log
@param[in]      flushed_lsn     the new set of log files should start with log
                                block in which flushed_lsn is located, and
                                flushed_lsn should point right after this block
                                header.

@return DB_SUCCESS or error code */
dberr_t log_files_create(log_t &log, lsn_t flushed_lsn);

/** Removes all log files.
@param[in,out] log  redo log */
void log_files_remove(log_t &log);

/** Creates a next log file, ready for writes. Updates log.m_current_file.
@param[in,out] log              redo log
@retval DB_SUCCESS              if created successfully
@retval DB_OUT_OF_DISK_SPACE    if there was no free space to create next file,
                                according to limitations we have for redo files,
                                or according to space physically available on
                                the disk
@retval other errors are possible */
dberr_t log_files_produce_file(log_t &log);

/** Persists log flags to the newest log file. Flushes header of the
log file and updates log.m_log_flags if succeeded.
@param[in,out]  log         redo log
@param[in]      log_flags   log_flags to persist
@return DB_SUCCESS or error */
dberr_t log_files_persist_flags(log_t &log, Log_flags log_flags);

/** Resets creator name to the current creator and marks all files as full in
their headers by setting LOG_HEADER_FLAG_FILE_FULL bit in the log_flags field.
Flushes headers of all log files and updates log.m_creator_name and log.m_files
accordingly if succeeded (if fails, then some files might remain updated and
some not; metadata stored in log.m_files should reflect that).
@param[in,out]  log     redo log
@return DB_SUCCESS or error */
dberr_t log_files_reset_creator_and_set_full(log_t &log);

/** Waits until a next log file is available and can be produced.
@param[in]  log   redo log */
void log_files_wait_for_next_file_available(log_t &log);

/** The log files governor thread routine.
@param[in,out]	log_ptr		pointer to redo log */
void log_files_governor(log_t *log_ptr);

/** Starts the log file management.
@param[in]  log   redo log
@return DB_SUCCESS or error */
dberr_t log_files_start(log_t &log);

/** Computes initial capacity limits and size suggested for the next log file.
Called when logically empty redo log is being initialized.
@param[in,out]  log  redo log */
void log_files_initialize_on_empty_redo(log_t &log);

/** Computes initial capacity limits and size suggested for the next log file.
Called after existing redo log files have been discovered (log.m_files).
@param[in,out]  log  redo log */
void log_files_initialize_on_existing_redo(log_t &log);

/** Updates capacity limitations after srv_redo_log_capacity_used has been
changed. It is called when user requests to change innodb_redo_log_capacity
in runtime.
@param[in,out]  log  redo log */
void log_files_resize_requested(log_t &log);

/** Updates capacity limitations after srv_thread_concurrency has been changed.
It is called when user requests to change innodb_thread_concurrency in runtime.
@param[in,out]  log  redo log */
void log_files_thread_concurrency_updated(log_t &log);

/** Disallows to generate dummy redo records and waits until
the log_files_governor thread promised not to generate them.
@param[in,out]  log   redo log */
void log_files_dummy_records_disable(log_t &log);

/** Disallows to generate dummy redo records but does not wait until
the log_files_governor promised not to generate them anymore.
@param[in,out]  log   redo log */
void log_files_dummy_records_request_disable(log_t &log);

/** Updates the encryption metadata stored in-memory for all redo log files.
Caller needs to have the log.m_files_mutex acquired before calling this.
@param[in,out]  log                  redo log
@param[in]      encryption_metadata  encryption metadata */
void log_files_update_encryption(
    log_t &log, const Encryption_metadata &encryption_metadata);

#endif /* !log0files_governor_h */
