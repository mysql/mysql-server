/*****************************************************************************

Copyright (c) 1995, 2024, Oracle and/or its affiliates.

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
 @file include/log0log.h

 Redo log - the main header.

 Basic types are defined inside log0types.h.

 Constant values are defined inside log0constants.h, but that
 file should only be included by log0types.h.

 The log_sys is defined in log0sys.h.

 Functions related to the log buffer are declared in log0buf.h.

 Functions related to the checkpoints are declared in log0chkp.h.

 Functions related to the writer/flusher are declared in log0write.h.

 Functions computing capacity of redo and related margins are declared
 in log0files_capacity.h.

 Functions doing IO to log files and formatting log blocks are declared
 in log0files_io.h.

 *******************************************************/

#ifndef log0log_h
#define log0log_h

#include "log0files_capacity.h"
#include "log0files_dict.h"
#include "log0files_finder.h"
#include "log0files_governor.h"
#include "log0files_io.h"
#include "log0sys.h"
#include "log0types.h"

/**************************************************/ /**

 @name Log - LSN computations.

 *******************************************************/

/** @{ */

/** Calculates lsn value for given sn value. Sequence of sn values
enumerate all data bytes in the redo log. Sequence of lsn values
enumerate all data bytes and bytes used for headers and footers
of all log blocks in the redo log. For every LOG_BLOCK_DATA_SIZE
bytes of data we have OS_FILE_LOG_BLOCK_SIZE bytes in the redo log.
NOTE that LOG_BLOCK_DATA_SIZE + LOG_BLOCK_HDR_SIZE + LOG_BLOCK_TRL_SIZE
== OS_FILE_LOG_BLOCK_SIZE. The calculated lsn value will always point
to some data byte (will be % OS_FILE_LOG_BLOCK_SIZE >= LOG_BLOCK_HDR_SIZE,
and < OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE).

@param[in]      sn      sn value
@return lsn value for the provided sn value */
constexpr inline lsn_t log_translate_sn_to_lsn(sn_t sn) {
  return sn / LOG_BLOCK_DATA_SIZE * OS_FILE_LOG_BLOCK_SIZE +
         sn % LOG_BLOCK_DATA_SIZE + LOG_BLOCK_HDR_SIZE;
}

/** Calculates sn value for given lsn value.
@see log_translate_sn_to_lsn
@param[in]      lsn     lsn value
@return sn value for the provided lsn value */
inline sn_t log_translate_lsn_to_sn(lsn_t lsn) {
  /* Calculate sn of the beginning of log block, which contains
  the provided lsn value. */
  const sn_t sn = lsn / OS_FILE_LOG_BLOCK_SIZE * LOG_BLOCK_DATA_SIZE;

  /* Calculate offset for the provided lsn within the log block.
  The offset includes LOG_BLOCK_HDR_SIZE bytes of block's header. */
  const uint32_t diff = lsn % OS_FILE_LOG_BLOCK_SIZE;

  if (diff < LOG_BLOCK_HDR_SIZE) {
    /* The lsn points to some bytes inside the block's header.
    Return sn for the beginning of the block. Note, that sn
    values don't enumerate bytes of blocks' headers, so the
    value of diff does not matter at all. */
    return sn;
  }

  if (diff > OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE) {
    /* The lsn points to some bytes inside the block's footer.
    Return sn for the beginning of the next block. Note, that
    sn values don't enumerate bytes of blocks' footer, so the
    value of diff does not matter at all. */
    return sn + LOG_BLOCK_DATA_SIZE;
  }

  /* Add the offset but skip bytes of block's header. */
  return sn + diff - LOG_BLOCK_HDR_SIZE;
}

/** Validates a given lsn value. Checks if the lsn value points to data
bytes inside log block (not to some bytes in header/footer). It is used
by assertions.
@return true if lsn points to data bytes within log block */
inline bool log_is_data_lsn(lsn_t lsn) {
  const uint32_t offset = lsn % OS_FILE_LOG_BLOCK_SIZE;

  return lsn >= LOG_START_LSN && offset >= LOG_BLOCK_HDR_SIZE &&
         offset < OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE;
}

/** @} */

#ifndef UNIV_HOTBACKUP

/**************************************************/ /**

 @name Log - general functions.

 *******************************************************/

/** @{ */

/** @return consistent sn value for locked state */
static inline sn_t log_get_sn(const log_t &log) {
  const sn_t sn = log.sn.load();
  if ((sn & SN_LOCKED) != 0) {
    return log.sn_locked.load();
  } else {
    return sn;
  }
}

/** Gets the current lsn value. This value points to the first non
reserved data byte in the redo log. When next user thread reserves
space in the redo log, it starts at this lsn.

If the last reservation finished exactly before footer of log block,
this value points to the first byte after header of the next block.

@note It is possible that the current lsn value does not fit free
space in the log files or in the log buffer. In such case, user
threads need to wait until the space becomes available.

@return current lsn */
inline lsn_t log_get_lsn(const log_t &log) {
  return log_translate_sn_to_lsn(log_get_sn(log));
}

/** Waits until there is free space for range of sn values ending
at the provided sn, in both the log buffer and in the log files.
@param[in]      log       redo log
@param[in]      end_sn    end of the range of sn values */
void log_wait_for_space(log_t &log, sn_t end_sn);

/** Prints information about important lsn values used in the redo log,
and some statistics about speed of writing and flushing of data.
@param[in]      log     redo log for which print information
@param[out]     file    file where to print */
void log_print(const log_t &log, FILE *file);

/** Refreshes the statistics used to print per-second averages in log_print().
@param[in,out]  log     redo log */
void log_refresh_stats(log_t &log);

void log_update_exported_variables(const log_t &log);

/** @} */

/**************************************************/ /**

 @name Log - initialization of the redo log system.

 *******************************************************/

/** @{ */

/** Initializes log_sys and finds existing redo log files, or creates a new
set of redo log files.

New redo log files are created in following cases:
  - there are no existing redo log files in the log directory,
  - existing set of redo log files is not marked as fully initialized
    (flag LOG_HEADER_FLAG_NOT_INITIALIZED exists in the newest file).

After this call, the log_sys global variable is allocated and initialized.
InnoDB might start recovery then.

@remarks
The redo log files are not resized in this function, because before resizing
log files, InnoDB must run recovery and ensure log files are logically empty.
The redo resize is currently the only scenario in which the initialized log_sys
might become closed by log_sys_close() and then re-initialized by another call
to log_sys_init().

@note Note that the redo log system is NOT ready for user writes after this
call is finished. The proper order of calls looks like this:
        - log_sys_init(),
        - log_start(),
        - log_start_background_threads()
and this sequence is executed inside srv_start() in srv0start.cc (interleaved
with remaining logic of the srv_start())

@note Note this function also verifies that REDO logs are in known format.

@param[in]    expect_no_files   true means we should return DB_ERROR if log
                                files are present in the directory before
                                proceeding any further
@param[in]    flushed_lsn       lsn at which new redo log files might be
                                started if they had to be created during
                                this call; this should be lsn stored in
                                the system tablespace header at offset
                                FIL_PAGE_FILE_FLUSH_LSN if the data
                                directory has been initialized;
@param[out]   new_files_lsn     updated to the lsn of the first checkpoint
                                created in the new log files if new log files
                                are created; else: 0
@return DB_SUCCESS or error */
dberr_t log_sys_init(bool expect_no_files, lsn_t flushed_lsn,
                     lsn_t &new_files_lsn);

/** Starts the initialized redo log system using a provided
checkpoint_lsn and current lsn. Block for current_lsn must
be properly initialized in the log buffer prior to calling
this function. Therefore a proper value of first_rec_group
must be set for that block before log_start is called.
@param[in,out]  log                redo log
@param[in]      checkpoint_lsn     checkpoint lsn
@param[in]      start_lsn          current lsn to start at
@param[in]      allow_checkpoints  true iff allows writing newer checkpoints
@return DB_SUCCESS or error */
dberr_t log_start(log_t &log, lsn_t checkpoint_lsn, lsn_t start_lsn,
                  bool allow_checkpoints = true);

/** Close the log system and free all the related memory. */
void log_sys_close();

/** Resizes the write ahead buffer in the redo log.
@param[in,out]  log       redo log
@param[in]      new_size  new size (in bytes) */
void log_write_ahead_resize(log_t &log, size_t new_size);

/** @} */

/**************************************************/ /**

 @name Log - the log threads and mutexes

 *******************************************************/

/** @{ */

/** Validates that all the log background threads are active.
Used only to assert, that the state is correct.
@param[in]      log     redo log */
void log_background_threads_active_validate(const log_t &log);

/** Validates that all the log background threads are inactive.
Used only to assert, that the state is correct. */
void log_background_threads_inactive_validate();

/** Starts all the log background threads. This can be called only,
when the threads are inactive. This should never be called concurrently.
This may not be called during read-only mode.
@param[in,out]  log     redo log */
void log_start_background_threads(log_t &log);

/** Stops all the log background threads. This can be called only,
when the threads are active. This should never be called concurrently.
This may not be called in read-only mode. Note that is is impossible
to start log background threads in such case.
@param[in,out]  log     redo log */
void log_stop_background_threads(log_t &log);

/** Marks the flag which tells log threads to stop and wakes them.
Does not wait until they are stopped.
@param[in,out]  log     redo log */
void log_stop_background_threads_nowait(log_t &log);

/** Function similar to @see log_stop_background_threads() except that it
stops all the log threads in such a way, that the redo log will be logically
empty after the threads are stopped.
@note It is caller responsibility to ensure that all threads other than the
log_files_governor cannot produce new redo log records when this function
is being called. */
void log_make_empty_and_stop_background_threads(log_t &log);

/** Wakes up all log threads which are alive.
@param[in,out]  log     redo log */
void log_wake_threads(log_t &log);

#define log_limits_mutex_enter(log) mutex_enter(&((log).limits_mutex))

#define log_limits_mutex_exit(log) mutex_exit(&((log).limits_mutex))

#define log_limits_mutex_own(log) mutex_own(&(log).limits_mutex)

/** @} */

/**************************************************/ /**

 @name Log - the log position locking.

 *******************************************************/

/** @{ */

/** Lock redo log. Both current lsn and checkpoint lsn will not change
until the redo log is unlocked.
@param[in,out]  log     redo log to lock */
void log_position_lock(log_t &log);

/** Unlock the locked redo log.
@param[in,out]  log     redo log to unlock */
void log_position_unlock(log_t &log);

/** Collect coordinates in the locked redo log.
@param[in]      log             locked redo log
@param[out]     current_lsn     stores current lsn there
@param[out]     checkpoint_lsn  stores checkpoint lsn there */
void log_position_collect_lsn_info(const log_t &log, lsn_t *current_lsn,
                                   lsn_t *checkpoint_lsn);

/** @} */

/**************************************************/ /**

 @name Log - persisting the flags.

 *******************************************************/

/** @{ */

/** Disable redo logging and persist the information.
@param[in,out]  log     redo log */
void log_persist_disable(log_t &log);

/** Enable redo logging and persist the information.
@param[in,out]  log     redo log */
void log_persist_enable(log_t &log);

/** Persist the information that it is safe to restart server.
@param[in,out]  log     redo log */
void log_persist_crash_safe(log_t &log);

/** Marks the redo log files as belonging to the initialized data directory
with initialized set of redo log files. Flushes the log_flags without the
flag LOG_HEADER_FLAG_NOT_INITIALIZED to the newest redo log file.
@param[in,out]  log   redo log */
void log_persist_initialized(log_t &log);

/** Asserts that the log is not marked as crash-unsafe.
@param[in,out]  log   redo log */
void log_crash_safe_validate(log_t &log);

/** @} */

#endif /* !UNIV_HOTBACKUP */

#endif /* !log0log_h */
