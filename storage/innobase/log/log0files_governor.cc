/*****************************************************************************

Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
 @file log/log0files_governor.cc

 Redo log files - governor.

 This module contains functions, which are useful during startup, for:
  -# redo log files creation / deletion,
  -# initialization on existing set of redo log files.

 However, the major part of this module is the log_files_governor's
 implementation. This thread is fully responsible for:
  -# cooperation with the set of redo log consumers, including:
    -# finding out the oldest redo log consumer
    -# determining the oldest needed lsn (files ending at smaller lsn
       might be consumed),
    -# rushing the oldest redo log consumer when it's lagging too much,
  -# consumption of the oldest redo log files, including:
    -# finding the files that might be consumed,
    -# deciding if consumption is really needed or might be postponed,
    -# deciding if consumed redo log files should be recycled or removed,
    -# removing or renaming the redo log files,
  -# all updates of log.m_capacity object (@see Log_files_capacity),
  -# supervising pending redo resizes, helping to finish them when needed, by:
    -# truncating the newest redo log file if its end is too far away,
    -# writing dummy redo records to complete the file if intake is too slow,
  -# managing the set of unused redo log files, including:
    -# resizing them if needed,
    -# creating a spare file ahead,
    -# recycling the consumed files,
    -# deleting the consumed files if needed.

 This module is responsible for managing redo log files on disk and keeping them
 in-sync with in-memory data structures: log.m_files, log.m_encryption_metadata.

 *******************************************************/

/* std::for_each */
#include <algorithm>

/* log_buffer_flush_to_disk */
#include "log0buf.h"

/* log_files_write_first_data_block_low, log_files_write_checkpoint_low */
#include "log0chkp.h"

/* log_encryption_generate_metadata */
#include "log0encryption.h"

/* log.m_capacity, ... */
#include "log0files_capacity.h"

/* log_create_unused_file, ... */
#include "log0files_io.h"

#include "log0files_governor.h"

/* log_persist_initialized, ... */
#include "log0log.h"

/* log_t::X */
#include "log0sys.h"

/* log_sync_point, srv_is_being_started */
#include "log0test.h"

/* LOG_N_FILES, ... */
#include "log0types.h"

/* log_writer_mutex_own */
#include "log0write.h"

/* os_offset_t */
#include "os0file.h"

#ifdef UNIV_DEBUG

/* current_thd */
#include "sql/current_thd.h"

/* debug_sync_set_action */
#include "sql/debug_sync.h"

#endif /* UNIV_DEBUG */

/* create_internal_thd, destroy_internal_thd */
#include "sql/sql_thd_internal_api.h"

/* srv_read_only_mode */
#include "srv0srv.h"

/* RECOVERY_CRASH, ... */
#include "srv0start.h"

/* ut_uint64_align_down */
#include "ut0byte.h"

/* ut::set<Log_file_id>, ut::vector */
#include "ut0new.h"

#ifndef UNIV_HOTBACKUP

/**************************************************/ /**

 @name Log - files management.

 *******************************************************/

/** @{ */

/** Validates that new log files might be created. */
static void log_files_create_allowed_validate();

/** Validates that log.m_files might be accessed from the current thread.

Validates that log.m_files_mutex is acquired unless srv_is_being_stated is true.

@param[in]  log   redo log */
static void log_files_access_allowed_validate(const log_t &log);

/** Validates that log.m_files might be accessed from the current thread
and the current thread is allowed to perform write IO for redo log files,
or create / remove / rename the existing redo log files.

Validates that both log.m_files_mutex and log.writer_mutex are acquired
unless srv_is_being_stated is true.

@param[in]  log   redo log */
static void log_files_write_allowed_validate(const log_t &log);

/** Prepares a log file header according to:
   - meta data provided by parameters (file_start_lsn),
   - and fields: log.m_format, log.m_creator_name, log.m_log_flags,
     log.m_log_uuid.

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in]  log             redo log
@param[in]  file_start_lsn  start lsn of the file
@return the prepared log file header */
static Log_file_header log_files_prepare_header(const log_t &log,
                                                lsn_t file_start_lsn);

/** Prepares a log file header according to:
   - meta data currently stored for the given log file in log.m_files,
   - and fields: log.m_format, log.m_creator_name, log.m_log_flags,
     log.m_log_uuid.

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in]  log       redo log
@param[in]  file      log file for which header is prepared
@return the prepared log file header */
static Log_file_header log_files_prepare_header(const log_t &log,
                                                const Log_file &file);

/** Asserts that all log files with id greater or equal to id of the file
containing the oldest lsn, have not been consumed / marked for consumption.

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in]  log   redo log */
static void log_files_validate_not_consumed(const log_t &log);

/** Asserts that the current file exists and contains
the log_files_newest_needed_lsn(log).

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in]  log   redo log */
static void log_files_validate_current_file(const log_t &log);

/** Creates a new redo log file and resizes the file to the size returned by
log.m_capacity.next_file_size(). If checkpoint_lsn != 0, it also must hold:
checkpoint_lsn >= start_lsn. The checkpoint information is stored to both
checkpoint headers of the new file. When create_first_data_block is true,
the first data block is modified also in the log buffer and written to disk
- that can only be used when there are no concurrent writes to the log buffer.

Requirement: log.m_files_mutex and log.writer_mutex acquired before calling
this function (unless srv_is_being_started).

@param[in,out] log                      redo log
@param[in]     file_id                  id of the log file to create
@param[in]     start_lsn                start_lsn for the log file
@param[in]     checkpoint_lsn           lsn of the current checkpoint if it
                                        is located in the file, or zero
@param[in]     create_first_data_block  true iff should format the first data
                                        block in log.buf and write to the file
@return DB_SUCCESS or error code */
static dberr_t log_files_create_file(log_t &log, Log_file_id file_id,
                                     lsn_t start_lsn, lsn_t checkpoint_lsn,
                                     bool create_first_data_block);

/** Callback type for callbacks called for a provided log file header.
@param[in]      file_id        id of the log file which owns the header
@param[in,out]  file_header    initially filled header of redo file,
                               accordingly to its currently known meta data;
                               should be updated by callback if needed */
typedef std::function<void(Log_file_id file_id, Log_file_header &file_header)>
    Log_file_header_callback;

/** Rewrites header of each of log files except the header of the newest file.

Requirement: log.m_files_mutex and log.writer_mutex acquired before calling
this function (unless srv_is_being_started).

@param[in,out]  log            redo log
@param[in]      before_write   callback which can modify the prepared header,
                               before the write
@param[in]      after_write    callback called after the successful write
@return DB_SUCCESS or error (if writing to disk failed) */
static dberr_t log_files_rewrite_old_headers(
    log_t &log, Log_file_header_callback before_write,
    Log_file_header_callback after_write);

/** Rewrites header of the newest log file, preparing it accordingly to the
current metadata of the file, which is stored in memory (in log.m_files).

Requirement: log.m_files_mutex and log.writer_mutex acquired before calling
this function (unless srv_is_being_started).

@param[in,out]  log   redo log
@param[in]      cbk   callback which can modify the prepared log file header,
                      before the write to disk
@return DB_SUCCESS or error (if writing to disk failed) */
static dberr_t log_files_rewrite_newest_header(log_t &log,
                                               Log_file_header_callback cbk);

/** Rewrites header of the current log file (log.m_current_file) marking the
file as not full in log_flags stored in the header. Updates metadata stored
in log.m_files and log.m_current_file for that file.

@remarks This is required to be called just before InnoDB can remove log files
from future (which it does when recovery didn't advance up to the last file).

Requirement: log.m_files_mutex and log.writer_mutex acquired before calling
this function (unless srv_is_being_started).

@param[in,out]  log   redo log
@return DB_SUCCESS or error (if writing to disk failed) */
static dberr_t log_files_mark_current_file_as_incomplete(log_t &log);

/** Rewrites header of the current log file (log.m_current_file) marking the
file as full in log_flags stored in the header. Updates metadata stored in
log.m_files and log.m_current_file for that file.

Requirement: log.m_files_mutex and log.writer_mutex acquired before calling
this function (unless srv_is_being_started).

@param[in,out]  log       redo log
@return DB_SUCCESS or error (if writing to disk failed) */
static dberr_t log_files_mark_current_file_as_full(log_t &log);

/** Updates log.m_current_file and (re-)opens that file (therefore also the
log.m_current_file_handle becomes updated).

Requirement: log.m_files_mutex, log.writer_mutex and log.flusher_mutex acquired
before calling this function (unless srv_is_being_started).

@param[in,out]  log   redo log */
static void log_files_update_current_file_low(log_t &log);

/** Finds the oldest LSN required by some of currently registered log consumers.
This is based on log_consumer_get_oldest().

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in]  log   redo log
@return maximum lsn up to which all log consumers have consumed the redo log */
static lsn_t log_files_oldest_needed_lsn(const log_t &log);

/** Finds the newest LSN which potentially might be interesting for currently
registered log consumers.

@remarks This is currently simply returning the log.write_lsn, but to preserve
the design clean this function exists as a complementary to the
@see log_files_oldest_needed_lsn().

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in]  log   redo log
@return the newest LSN (log.write_lsn) */
static lsn_t log_files_newest_needed_lsn(const log_t &log);

/** Removes redo log files for LSN ranges (from future) with m_start_lsn larger
than log_files_newest_needed_lsn(log). The files are removed from disk and from
the log.m_files.

Requirement: log.m_files_mutex and log.writer_mutex acquired before calling
this function (unless srv_is_being_started).

@param[in,out]  log   redo log
@return DB_SUCCESS or error (if rewriting header of the current file failed) */
static dberr_t log_files_remove_from_future(log_t &log);

/** Removes the file from disk and after that from log.m_files.

Requirement: log.m_files_mutex and log.writer_mutex acquired before calling
this function (unless srv_is_being_started).

@param[in,out]  log       redo log
@param[in]      file_id   id of the log file to remove
@return DB_SUCCESS or error */
static dberr_t log_files_remove_file(log_t &log, Log_file_id file_id);

/** Computes current logical size of the redo log and the minimum suggested
soft (excluding extra_margin) logical capacity for the current redo log data.
Note that this two values could be based on different redo log consumers,
because different redo log consumers might be related to different margins
preserved between size and capacity.

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in]  log   redo log
@return pair of logical redo log size and suggesed logical soft capacity */
static std::pair<lsn_t, lsn_t> log_files_logical_size_and_checkpoint_age(
    const log_t &log);

/** Provides size for the next redo log file that will be created, unless
there is not enough free space in current physical capacity, in which case
it returns zero. If the zero was returned, then it's guaranteed that either:
  - some consumed log files might be removed or recycled,
  - or some log files might become consumed and then removed or recycled.
These actions are executed by the log_files_governor thread, and that will
certainly lead to state in which log_files_next_file_size() returns non-zero,
because that's the property guaranteed by the strategy in Log_files_capacity.
Therefore for other threads, which are waiting on this function to return non
zero value, it's enough to wait. However, while waiting for that, these threads
must not hold m_files_mutex. This function cannot wait itself, because it needs
m_files_mutex prior to be called and guarantees not to release the mutex itself.

@remarks This function should be used only in scenarios in which it is important
if there is free space to create the next redo log file. In scenarios which do
not care about that, the log.m_capacity.next_file_size() should be used instead.

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in]  log   redo log
@return size of next redo log file or 0 if it cannot be created */
static os_offset_t log_files_next_file_size(const log_t &log);

/** Updates capacity limitations (log.m_capacity.* and log.free_check_sn).
If redo resize was in progress, and conditions to consider it finished
became satisfied, this call would result in marking the resize as done.

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in,out]  log   redo log */
static void log_files_update_capacity_limits(log_t &log);

/** Generates at least a given bytes of intake to the redo log.

Requirement: none of log.m_files_mutex, log.writer_mutex, log.flusher_mutex,
log.checkpointer_mutex is acquired when the function is called.

Requirement: the log_files_governor thread still hasn't promised not to
generate dummy redo records (!log.m_no_more_dummy_records_promised).

@param[in,out]  log       redo log
@param[in]      min_size  min number of bytes to write to the log buffer */
static void log_files_generate_dummy_records(log_t &log, lsn_t min_size);

/** Checks if redo log file truncation is allowed. It is guaranteed that
the conditions checked by this function are based on properties guarded
by log.m_files_mutex. Note that this function checks only for allowance.
It does not check if truncate is recommended to be done.

@remarks This function currently checks if there is just a single redo log
consumer (and log_checkpointer is expected to be that one) in which case
(and only then) the truncation is allowed.

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in]  log   redo log
@return true iff redo log file truncation is allowed */
static bool log_files_is_truncate_allowed(const log_t &log);

/** Truncates the redo log file. It must be called when there is exactly
one redo log file.

Requirement: log.m_files_mutex and log.writer_mutex acquired before calling
this function (unless srv_is_being_started) and log_files_is_truncate_allowed()
must be allowing to do the truncation and there must exist exactly one redo
log file.

@param[in,out]  log       redo log */
static void log_files_truncate(log_t &log);

/** Marks the given file as consumed by the registered redo log consumers.
It allows to recycle or remove the file later, when
log_files_process_consumed_files() is called.

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in,out]  log       redo log
@param[in]      file_id   id of the file which should be marked as consumed */
static void log_files_mark_consumed_file(log_t &log, Log_file_id file_id);

/** Marks each redo log file which became consumed by all registered redo log
consumers, as consumed. It allows to recycle or remove those files later,
when log_files_process_consumed_files() is called.

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in,out]  log       redo log */
static void log_files_mark_consumed_files(log_t &log);

/** Removes the given consumed redo log file. This is called when the redo log
files marked as consumed are being processed (recycled or removed).

Requirement: log.m_files_mutex and log.writer_mutex acquired before calling
this function (unless srv_is_being_started) and the given file must have been
marked as consumed before calling this function.

@param[in,out]  log       redo log
@param[in]      file_id   id of the file which should be removed
@retval  true   removed successfully
@retval  false  failed to remove (disk error) */
static bool log_files_remove_consumed_file(log_t &log, Log_file_id file_id);

/** Recycles the given consumed redo log file. This is called when the redo log
files marked as consumed are being processed (recycled or removed). The file
becomes renamed and joins the set of unused (spare) redo log files.

Requirement: log.m_files_mutex and log.writer_mutex acquired before calling
this function (unless srv_is_being_started) and the given file must have been
marked as consumed before calling this function.

@param[in,out]  log               redo log
@param[in]      file_id           id of the file which should be recycled
@param[in]      unused_file_size  size to which the file should be resized
@retval  true   recycled (including rename) successfully
@retval  false  failed to rename (disk error) */
static bool log_files_recycle_file(log_t &log, Log_file_id file_id,
                                   os_offset_t unused_file_size);

/** Process the given consumed redo log file (recycle or remove the file).

Requirement: log.m_files_mutex and log.writer_mutex acquired before calling
this function (unless srv_is_being_started) and the given file must have been
marked as consumed before calling this function.

@param[in,out]  log       redo log
@param[in]      file_id   id of the file which should be processed
@retval  true   processed (recycled or removed) successfully
@retval  false  failed to process (disk error) */
static bool log_files_process_consumed_file(log_t &log, Log_file_id file_id);

/** Process all redo log files marked as consumed (recycling or removing each).

Requirement: log.m_files_mutex and log.writer_mutex acquired before calling
this function (unless srv_is_being_started).

@param[in,out]  log  redo log */
static void log_files_process_consumed_files(log_t &log);

/** Create a spare unused redo log file if there was no such file.

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in,out]  log  redo log */
static void log_files_create_next_as_unused_if_needed(log_t &log);

/** Checks if the redo log consumption, according to the measured average
speed of consumption, is too slow to consume the oldest file in reasonable
time (10 seconds).

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in]  log       redo log
@return true  iff the redo log consumption is not going to consume the
              oldest redo log file soon */
static bool log_files_consuming_oldest_file_takes_too_long(const log_t &log);

/** Checks if the redo log, according to the measured average speed,
is not being filled fast enough to fill the oldest redo log file in
reasonable time (10 seconds). Note, that if there are multiple redo
log files, then the oldest file has already been filled, in which
case this function would return false.

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in]  log       redo log
@return true  iff the redo log is not being filled with new data
              fast enough to fill the oldest redo log file soon */
static bool log_files_filling_oldest_file_takes_too_long(const log_t &log);

/** Checks if there are any reasons to rush consumption of the oldest redo
log file, that is if either:
  - rushing consumption has been requested explicitly (by the log_writer
    which is waiting for a next available file),
  - or the redo log is being resized down.

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in]  log       redo log
@return true  iff consumption of the oldest redo log files should be rushed */
static bool log_files_should_rush_oldest_file_consumption(const log_t &log);

/** Ensures that unused redo log files have log.m_capacity.next_file_size().

Requirement: log.m_files_mutex acquired before calling this function
(unless srv_is_being_started).

@param[in,out]  log   redo log */
static void log_files_adjust_unused_files_sizes(log_t &log);

/** Result of execution of log_files_governor_iteration_low(). */
enum class Log_files_governor_iteration_result {
  /** Execution needs to be repeated, but this time when having
  the log.writer_mutex acquired. */
  RETRY_WITH_WRITER_MUTEX,

  /** Execution completed successfully, the governor should produce
  extra intake to help the pending redo resize to be finished. */
  COMPLETED_BUT_NEED_MORE_INTAKE,

  /** Execution completed successfully, nothing to be done more. */
  COMPLETED
};

/** Tries to perform a single iteration of the log_files_governor thread.
However, it might turn out, that the log.writer_mutex is required to perform
required actions. In such case (happens only when has_writer_mutex is false)
this function returns earlier, without completing all the steps and returns the
Log_files_governor_iteration_result::RETRY_WITH_WRITER_MUTEX value. Then caller
is responsible for repeating the call, after acquiring the log.writer_mutex.

Requirement: log.m_files_mutex not acquired before calling this function and
log.writer_mutex acquired iff has_writer_mutex is true.

@param[in,out]  log               redo log
@param[in]      has_writer_mutex  true iff this thread has log.writer_mutex
                                  acquired
@return @see Log_files_governor_iteration_result */
static Log_files_governor_iteration_result log_files_governor_iteration_low(
    log_t &log, bool has_writer_mutex);

/** Performs a single iteration of the log_files_governor thread. It first tries
to perform the iteration without the log.writer_mutex. However, if that failed
then this function would retry the attempt, after acquiring the
log.writer_mutex.

Requirement: none of log.m_files_mutex, log.writer_mutex acquired before calling
this function.

@param[in,out]  log   redo log */
static void log_files_governor_iteration(log_t &log);

static dberr_t log_files_prepare_unused_file(log_t &log, Log_file_id file_id,
                                             lsn_t start_lsn,
                                             lsn_t checkpoint_lsn,
                                             bool create_first_data_block,
                                             os_offset_t &file_size);

static lsn_t log_files_oldest_needed_lsn(const log_t &log) {
  log_files_access_allowed_validate(log);
  lsn_t oldest_need_lsn;
  log_consumer_get_oldest(log, oldest_need_lsn);
  return oldest_need_lsn;
}

static lsn_t log_files_newest_needed_lsn(const log_t &log) {
  log_files_access_allowed_validate(log);
  return log.write_lsn.load();
}

static std::pair<lsn_t, lsn_t> log_files_logical_size_and_checkpoint_age(
    const log_t &log) {
  log_files_access_allowed_validate(log);

  const lsn_t oldest_lsn = log_files_oldest_needed_lsn(log);

  const lsn_t checkpoint_lsn = log.last_checkpoint_lsn.load();

  const lsn_t newest_lsn = log_files_newest_needed_lsn(log);

  const lsn_t size = ut_uint64_align_up(newest_lsn, OS_FILE_LOG_BLOCK_SIZE) -
                     ut_uint64_align_down(oldest_lsn, OS_FILE_LOG_BLOCK_SIZE);

  /* Note that log_files_next_checkpoint() updates log.last_checkpoint_lsn
  under m_files_mutex which we hold here, so it couldn't increase meanwhile. */
  ut_a(checkpoint_lsn <= newest_lsn);

  const lsn_t checkpoint_age = newest_lsn - checkpoint_lsn;

  return {size, checkpoint_age};
}

static os_offset_t log_files_next_file_size(const log_t &log) {
  log_files_access_allowed_validate(log);

  const os_offset_t file_size = log.m_capacity.next_file_size();

  if (log.m_capacity.current_physical_capacity() <
      log_files_size_of_existing_files(log.m_files) + file_size) {
    /* Note: it might happen if the log_files_governor hasn't yet
    consumed (or processed all consumed) log files. However, it's
    safe to wait after releasing m_files_mutex in case this function
    returned 0, because the log_files_governor will be able to consume
    and process the required files, so there is no cycle. */
#ifdef UNIV_DEBUG
    const lsn_t oldest_lsn = log_files_oldest_needed_lsn(log);
    const auto oldest_file = log.m_files.begin();
    ut_a(oldest_file != log.m_files.end());
    ut_a(!oldest_file->contains(oldest_lsn));
#endif
    return 0;
  }
  return file_size;
}

static Log_file_id log_files_next_unused_id(log_t &log) {
  log_files_validate_current_file(log);
  return Log_file::next_id(log.m_current_file.m_id,
                           log.m_unused_files_count + 1);
}

static void log_files_create_allowed_validate() {
  /* During shutdown we might also allow to do some maintenance. */
  ut_a(srv_is_being_started ||
       srv_shutdown_state.load() >= SRV_SHUTDOWN_LAST_PHASE);

  ut_a(!srv_read_only_mode);
  ut_a(!recv_recovery_is_on());

  log_background_threads_inactive_validate();
}

static void log_files_access_allowed_validate(const log_t &log) {
  ut_ad(log_files_mutex_own(log) || srv_is_being_started);
}

static void log_files_write_allowed_validate(const log_t &log) {
  log_files_access_allowed_validate(log);
  ut_a(!srv_read_only_mode);
  ut_ad(log_writer_mutex_own(log));
}

static Log_file_header log_files_prepare_header(const log_t &log,
                                                lsn_t file_start_lsn) {
  log_files_access_allowed_validate(log);

  Log_file_header file_header;
  file_header.m_format = to_int(log.m_format);
  file_header.m_start_lsn = file_start_lsn;
  file_header.m_creator_name = log.m_creator_name;
  file_header.m_log_flags = log.m_log_flags;
  file_header.m_log_uuid = log.m_log_uuid;
  return file_header;
}

static Log_file_header log_files_prepare_header(const log_t &log,
                                                const Log_file &file) {
  log_files_access_allowed_validate(log);

  ut_ad(file.m_start_lsn == log.m_files.file(file.m_id)->m_start_lsn);
  auto file_header = log_files_prepare_header(log, file.m_start_lsn);
  if (file.m_full) {
    log_file_header_set_flag(file_header.m_log_flags,
                             LOG_HEADER_FLAG_FILE_FULL);
  }
  return file_header;
}

static void log_files_validate_not_consumed(const log_t &log) {
  log_files_access_allowed_validate(log);
  log_files_for_each(log.m_files, log_files_oldest_needed_lsn(log),
                     log_files_newest_needed_lsn(log),
                     [](const Log_file &file) { ut_a(!file.m_consumed); });
}

static void log_files_validate_current_file(const log_t &log) {
  log_files_access_allowed_validate(log);
  const auto file = log.m_files.file(log.m_current_file.m_id);
  ut_a(file != log.m_files.end());
  ut_a(!file->m_consumed);
  ut_a(*file == log.m_current_file);
  const lsn_t newest_lsn = log_files_newest_needed_lsn(log);
  ut_a(file->contains(newest_lsn) || newest_lsn == file->m_end_lsn);
}

static void log_files_mark_consumed_file(log_t &log, Log_file_id file_id) {
  log_files_access_allowed_validate(log);

  const auto file = log.m_files.file(file_id);
  ut_a(file != log.m_files.end());
  ut_a(file->m_id == file_id);
  ut_a(!file->m_consumed);

  /* We are not consuming file which has LSN range containing oldest_lsn. */
  const lsn_t oldest_lsn = log_files_oldest_needed_lsn(log);
  ut_a(!file->contains(oldest_lsn));

  /* We are not consuming file which has LSN range containing newest_lsn,
  unless this is lsn pointing exactly on the beginning of the file. */
  const lsn_t newest_lsn = log_files_newest_needed_lsn(log);
  ut_a(!file->contains(newest_lsn));

  /* We are not consuming file on path from the oldest file to the newest. */
  log_files_for_each(log.m_files, oldest_lsn, newest_lsn,
                     [file_id](const Log_file &f) { ut_a(f.m_id != file_id); });

  log.m_files.set_consumed(file_id);

  DBUG_PRINT("ib_log", ("consumed log file %zu (LSN " LSN_PF ".." LSN_PF ")",
                        size_t{file_id}, file->m_start_lsn, file->m_end_lsn));

  log_files_validate_not_consumed(log);
}

static dberr_t log_files_remove_from_future(log_t &log) {
  log_files_write_allowed_validate(log);

  const lsn_t newest_lsn = log_files_newest_needed_lsn(log);
  ut_a(newest_lsn >= LOG_START_LSN);

  /* NOTE: This list has to be built, because log_files_remove_file()
  removes file from the log.m_files, so InnoDB cannot call it when
  iterating the log.m_files. */

  std::vector<Log_file_id> to_remove;

  log_files_for_each(log.m_files, [&](const Log_file &file) {
    if (!file.m_consumed && newest_lsn <= file.m_start_lsn &&
        file.m_id != log.m_current_file.m_id) {
      to_remove.push_back(file.m_id);
    }
  });

  if (!to_remove.empty()) {
    const dberr_t err = log_files_mark_current_file_as_incomplete(log);
    if (err != DB_SUCCESS) {
      return err;
    }
  }

  log_files_validate_current_file(log);

  for (Log_file_id file_id : to_remove) {
    const dberr_t err = log_files_remove_file(log, file_id);
    if (err != DB_SUCCESS) {
      return err;
    }
  }

  log_files_validate_current_file(log);

  return DB_SUCCESS;
}

static void log_files_mark_consumed_files(log_t &log) {
  log_files_access_allowed_validate(log);

  const lsn_t oldest_lsn = log_files_oldest_needed_lsn(log);

  log_files_validate_current_file(log);

  log_files_for_each(log.m_files, [&](const Log_file &file) {
    if (!file.m_consumed && file.m_end_lsn <= oldest_lsn) {
      log_files_mark_consumed_file(log, file.m_id);
    }
  });
}

namespace log_files {

static os_offset_t physical_size(const log_t &log,
                                 os_offset_t unused_file_size) {
  return log_files_size_of_existing_files(log.m_files) +
         log.m_unused_files_count * unused_file_size;
}

static bool physical_capacity_allows_to_recycle(const log_t &log,
                                                os_offset_t removed_file_size,
                                                os_offset_t unused_file_size) {
  const auto current_total_physical_size = physical_size(log, unused_file_size);

  const auto planned_total_physical_size =
      current_total_physical_size + unused_file_size - removed_file_size;

  return planned_total_physical_size <=
         log.m_capacity.current_physical_capacity();
}

static bool physical_capacity_allows_to_create(const log_t &log,
                                               os_offset_t unused_file_size) {
  return physical_capacity_allows_to_recycle(log, 0, unused_file_size);
}

static size_t number_of_files(const log_t &log) {
  return log_files_number_of_existing_files(log.m_files) +
         log.m_unused_files_count;
}

static bool is_newest_lsn_nearby_the_end(const log_t &log) {
  const auto margin = log.m_capacity.next_file_earlier_margin();
  const lsn_t newest_lsn = log_files_newest_needed_lsn(log);
  return newest_lsn + margin >= log.m_current_file.m_end_lsn;
}

static bool number_of_files_enforced_to_allow(const log_t &log) {
  return log.m_unused_files_count == 0 && is_newest_lsn_nearby_the_end(log);
}

/** Checks if current number of redo files allows to recycle the oldest one.
@remarks
The strategy is to keep the total number of log files equal to LOG_N_FILES
(including unused files), unless the newest_lsn is close to the end of file
and there are no unused files (in which case, the creation is also allowed,
because at least one unused file is allowed, as long as we won't exceed
current_physical_capacity, which is checked by the caller).
@return true iff allowed to recycle log file according to number of files */
static bool number_of_files_allows_to_recycle(const log_t &log) {
  return number_of_files(log) <= LOG_N_FILES ||
         number_of_files_enforced_to_allow(log);
}

/** Checks if current number of redo files allows to create a new unused file.
@see number_of_files_allows_to_recycle
@return true iff allowed to create unused file according to number of files */
static bool number_of_files_allows_to_create(const log_t &log) {
  return number_of_files(log) + 1 <= LOG_N_FILES ||
         number_of_files_enforced_to_allow(log);
}

static bool might_recycle_file(const log_t &log, os_offset_t removed_file_size,
                               os_offset_t unused_file_size) {
  return number_of_files_allows_to_recycle(log) &&
         physical_capacity_allows_to_recycle(log, removed_file_size,
                                             unused_file_size);
}

static bool might_create_unused_file(const log_t &log,
                                     os_offset_t unused_file_size) {
  return number_of_files_allows_to_create(log) &&
         physical_capacity_allows_to_create(log, unused_file_size);
}

/** Checks if consumption of the oldest redo log files needs to be done or
might be postponed.

@remarks
This check is being used in order to keep more log files, even if according to
all registered redo log consumers, they are not needed anymore. The motivation
for that is to support external redo consumers which are not being registered
and preserve for them comparable chances to succeed to chances they had in older
versions of MySQL. This is just to be gentle, and this is not always guaranteed.
In particular, we do not provide such properties when it is not comfortable for
the InnoDB. This function is supposed to tell when it is comfortable for InnoDB
to provide such properties. The log files consumption must not be postponed in
any of the following cases:

1. The log_files_governor has been explicitly requested to consume more files,
   which could happen e.g. in mtr test awaiting until there is just one file,
   or when there is no next file (in which case requested by log_writer).

2. There are no spare (unused) log files (at least one is needed for log_writer
   so it could switch to a next log file smoothly if it needed to do so).

3. Redo log is supposed to be resized down (current_capacity hasn't yet reached
   the target_capacity).

4. Redo log is supposed to be resized up (unused files might be resized easily
   so it is preferable to consume files).

@param[in]  log   redo log
@return true iff redo log files consumption is needed */
bool is_consumption_needed(const log_t &log) {
  DBUG_EXECUTE_IF("log_force_consumption", return true;);
  const auto current_size = physical_size(log, log.m_capacity.next_file_size());
  const auto target_capacity = log.m_capacity.target_physical_capacity();
  const auto current_capacity = log.m_capacity.current_physical_capacity();

  ut_a(current_size <= current_capacity);

  return /* case 1. */ log.m_requested_files_consumption ||
         /* case 2. */ log.m_unused_files_count == 0 ||
         /* case 3. */ target_capacity < current_capacity ||
         /* case 4. */ current_size < current_capacity;
}

}  // namespace log_files

static dberr_t log_files_remove_file(log_t &log, Log_file_id file_id) {
  log_files_write_allowed_validate(log);
  ut_a(!log.m_files.empty());
  const auto file = log.m_files.file(file_id);
  ut_a(file != log.m_files.end());
  ut_a(file->m_id == file_id);
  const dberr_t remove_err = log_remove_file(log.m_files_ctx, file_id);
  if (remove_err != DB_SUCCESS) {
    return remove_err;
  }
  log.m_files.erase(file_id);
  os_event_set(log.m_file_removed_event);
  return DB_SUCCESS;
}

static bool log_files_remove_consumed_file(log_t &log, Log_file_id file_id) {
  log_files_write_allowed_validate(log);
  ut_a(!log.m_files.empty());
  ut_a(log.m_files.begin()->m_id == file_id);
  ut_a(log.m_files.begin()->m_consumed);
  return log_files_remove_file(log, file_id) == DB_SUCCESS;
}

static bool log_files_recycle_file(log_t &log, Log_file_id file_id,
                                   os_offset_t unused_file_size) {
  log_files_write_allowed_validate(log);
  ut_a(!log.m_files.empty());

  const auto file = log.m_files.file(file_id);
  ut_a(file == log.m_files.begin());

  /* For example: #ib_redo10 -> #ib_redo10_tmp */
  dberr_t err = log_mark_file_as_unused(log.m_files_ctx, file_id);
  if (err != DB_SUCCESS) {
    return false;
  }

  const auto next_unused_id = log_files_next_unused_id(log);

  /* For example: #ib_redo10_tmp -> #ib_redo15_tmp */
  err = log_rename_unused_file(log.m_files_ctx, file_id, next_unused_id);
  ut_a(err == DB_SUCCESS);

  /* For example: resize #ib_redo15_tmp to innodb_redo_log_capacity / 32 */
  err =
      log_resize_unused_file(log.m_files_ctx, next_unused_id, unused_file_size);
  ut_a(err == DB_SUCCESS);

  log.m_unused_files_count++;
  log.m_files.erase(file_id);
  return true;
}

static bool log_files_process_consumed_file(log_t &log, Log_file_id file_id) {
  log_files_write_allowed_validate(log);
  const auto file = log.m_files.file(file_id);
  ut_a(file != log.m_files.end());
  ut_a(file->m_consumed);
  log_files_validate_current_file(log);

  const os_offset_t unused_file_size = log.m_capacity.next_file_size();

  if (log_files::might_recycle_file(log, file->m_size_in_bytes,
                                    unused_file_size)) {
    return log_files_recycle_file(log, file_id, unused_file_size);
  } else {
    return log_files_remove_consumed_file(log, file_id);
  }
}

static void log_files_process_consumed_files(log_t &log) {
  log_files_write_allowed_validate(log);

  /* NOTE: This list has to be built, because log_files_process_consumed_file()
  removes file from the log.m_files, so InnoDB cannot call it when iterating
  the log.m_files. */

  ut::vector<Log_file_id> to_process;

  log_files_for_each(log.m_files, [&](const Log_file &file) {
    if (file.m_consumed) {
      to_process.push_back(file.m_id);
    }
  });

  bool any_processed = false;
  for (Log_file_id file_id : to_process) {
    if (!log_files_process_consumed_file(log, file_id)) {
      break;
    }
    any_processed = true;
  }

  if (any_processed) {
    log_files_update_capacity_limits(log);
  }
}

static void log_files_create_next_as_unused_if_needed(log_t &log) {
  log_files_access_allowed_validate(log);
  log_files_validate_current_file(log);

  const os_offset_t unused_file_size = log.m_capacity.next_file_size();

  if (!log_files::might_create_unused_file(log, unused_file_size)) {
    return;
  }

  const Log_file_id file_id = log_files_next_unused_id(log);

  const dberr_t err =
      log_create_unused_file(log.m_files_ctx, file_id, unused_file_size);

  if (err == DB_SUCCESS) {
    log.m_unused_files_count++;
  }
}

dberr_t log_files_produce_file(log_t &log) {
  log_files_write_allowed_validate(log);
  log_files_validate_current_file(log);

  const lsn_t start_lsn = log.m_current_file.m_end_lsn;

  ut_a(start_lsn >= LOG_START_LSN);
  ut_a(start_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_a(start_lsn == log_files_newest_needed_lsn(log));

  Log_file_id file_id = log.m_current_file.next_id();
  ut_a(log.m_files.file(file_id) == log.m_files.end());

  if (log_files_next_file_size(log) == 0) {
    return DB_OUT_OF_DISK_SPACE;
  }

  log_sync_point("log_before_file_produced");

  ut_a(start_lsn >= log.last_checkpoint_lsn.load());

  {
    const dberr_t err =
        log_files_create_file(log, file_id, start_lsn, 0, false);

    if (err != DB_SUCCESS) {
      return err;
    }
  }

  log_sync_point("log_before_file_marked_as_full");

  ut_a(file_id > log.m_current_file.m_id);
  /* Because newer file is created, we can mark the previously newest file
  as full. That is useful, because if during recovery InnoDB couldn't find
  next redo log file, it could then determine the reason for that:
  - if server was crashed just before log_files_produce_file was called
    (but after the last log block to file was written),
  - or if the newest redo log file was simply lost (e.g. FS corruption). */
  {
    const dberr_t err = log_files_mark_current_file_as_full(log);

    ut_a(err == DB_SUCCESS);
  }

  log_flusher_mutex_enter(log);
  log_files_update_current_file_low(log);
  log_flusher_mutex_exit(log);

  ut_a(log.m_current_file.m_id == file_id);

  DBUG_PRINT("ib_log", ("produced log file %zu (LSN " LSN_PF ".." LSN_PF ")",
                        size_t{file_id}, log.m_current_file.m_start_lsn,
                        log.m_current_file.m_end_lsn));

  log_files_validate_not_consumed(log);

  log_sync_point("log_after_file_produced");
  os_event_set(log.m_files_governor_event);

  return DB_SUCCESS;
}

static void log_files_update_capacity_limits(log_t &log) {
  log_files_access_allowed_validate(log);

  IB_mutex_guard limits_lock{&log.limits_mutex, UT_LOCATION_HERE};

  lsn_t logical_size, checkpoint_age;
  std::tie(logical_size, checkpoint_age) =
      log_files_logical_size_and_checkpoint_age(log);

  log.m_capacity.update(log.m_files, logical_size, checkpoint_age);

  log_update_limits_low(log);
  log_update_exported_variables(log);
}

static bool log_files_consuming_oldest_file_takes_too_long(const log_t &log) {
  log_files_access_allowed_validate(log);
  ut_a(!log.m_files.empty());

  if (!(log_checkpointer_is_active() && log.m_allow_checkpoints.load())) {
    return false;
  }

  const lsn_t oldest_lsn = log_files_oldest_needed_lsn(log);

  const auto oldest_file = log.m_files.find(oldest_lsn);
  ut_a(oldest_file != log.m_files.end());

  /** Maximum time it might take to consume the oldest redo log file
  since now. If the average lsn consumption speed shows that most likely
  this time will not be enough for the oldest file to become consumed,
  then the oldest redo log consumer must be requested to proceed faster. */

  const uint32_t MAX_CONSUMPTION_TIME_IN_SEC = 10;

  const lsn_t predicted_oldest_lsn =
      oldest_lsn +
      log.m_files_stats.m_lsn_consumption_per_1s * MAX_CONSUMPTION_TIME_IN_SEC;

  const lsn_t newest_lsn = log_files_newest_needed_lsn(log);

  return predicted_oldest_lsn <= newest_lsn &&
         oldest_file->contains(predicted_oldest_lsn);
}

static bool log_files_filling_oldest_file_takes_too_long(const log_t &log) {
  log_files_access_allowed_validate(log);
  ut_a(!log.m_files.empty());

  if (!(log_checkpointer_is_active() && log.m_allow_checkpoints.load())) {
    return false;
  }

  DBUG_EXECUTE_IF("log_force_truncate", return true;);

  const lsn_t oldest_lsn = log_files_oldest_needed_lsn(log);

  const auto oldest_file = log.m_files.find(oldest_lsn);
  ut_a(oldest_file != log.m_files.end());

  const uint32_t MAX_FILL_TIME_IN_SEC = 10;

  const lsn_t predicted_newest_lsn =
      log_files_newest_needed_lsn(log) +
      log.m_files_stats.m_lsn_production_per_1s * MAX_FILL_TIME_IN_SEC;

  /* Check if next 10-seconds of current avg. intake would result in
  the newest lsn still being inside the oldest redo log file. */

  return oldest_file->contains(predicted_newest_lsn);
}

void Log_files_stats::update(const log_t &log) {
  log_files_access_allowed_validate(log);

  /* Check if stats should be updated (so called "successful call"). */
  const auto now = Log_clock::now();
  const auto duration = std::chrono::duration_cast<std::chrono::seconds>(
      now - m_last_update_time);
  if (duration.count() < 1.0) {
    /* Failed to update stats (not enough time elapsed since last update) */
    return;
  }
  /* It is a next successful call to update(). */
  m_last_update_time = now;

  /* Update m_lsn_consumption_per_1s, m_oldest_lsn_on_update. */
  const lsn_t oldest_lsn = log_files_oldest_needed_lsn(log);
  if (m_oldest_lsn_on_update != 0) {
    const lsn_t lsn_diff = oldest_lsn - m_oldest_lsn_on_update;
    m_lsn_consumption_per_1s = lsn_diff / duration.count();
  }
  m_oldest_lsn_on_update = oldest_lsn;

  /* Update m_lsn_production_per_1s, m_newest_lsn_on_update. */
  const lsn_t newest_lsn = log_files_newest_needed_lsn(log);
  if (m_newest_lsn_on_update != 0) {
    const lsn_t lsn_diff = newest_lsn - m_newest_lsn_on_update;
    m_lsn_production_per_1s = lsn_diff / duration.count();
  }
  m_newest_lsn_on_update = newest_lsn;
}

static void log_files_adjust_unused_files_sizes(log_t &log) {
  log_files_access_allowed_validate(log);
  const os_offset_t next_file_size = log.m_capacity.next_file_size();
  if (log.m_unused_file_size != next_file_size) {
    const auto ret = log_remove_unused_files(log.m_files_ctx);
    ut_a(ret.first == DB_SUCCESS);

    log.m_unused_files_count = 0;
    log.m_unused_file_size = next_file_size;
  }
}

static bool log_files_should_rush_oldest_file_consumption(const log_t &log) {
  log_files_access_allowed_validate(log);
  return log.m_capacity.is_resizing_down() || log.m_requested_files_consumption;
}

static Log_files_governor_iteration_result log_files_governor_iteration_low(
    log_t &log, bool has_writer_mutex) {
  using Iteration_result = Log_files_governor_iteration_result;

  IB_mutex_guard files_latch{&(log.m_files_mutex), UT_LOCATION_HERE};
  log_files_update_capacity_limits(log);
  log_files_adjust_unused_files_sizes(log);

  if (log_files::is_consumption_needed(log)) {
    log_files_mark_consumed_files(log);
  }
  if (log_files_number_of_consumed_files(log.m_files) != 0) {
    if (has_writer_mutex) {
      log_files_process_consumed_files(log);
    } else {
      return Iteration_result::RETRY_WITH_WRITER_MUTEX;
    }
  }

  if (log.m_requested_files_consumption && log_files_next_file_size(log) != 0) {
    /* The log_writer thread called log_files_wait_for_next_file_available(),
    which checked that log_files_next_file_size() returned zero and set
    m_requested_files_consumption to true under the the log.m_files_mutex.
    This would force rushing the consumption of the oldest redo log file.
    However, log_files_next_file_size() is no longer zero so there is no
    reason to force the consumption any longer. */

    log.m_requested_files_consumption = false;
  }

  log.m_files_stats.update(log);

  bool needs_more_intake = false;

  if (log_files_should_rush_oldest_file_consumption(log)) {
    /* Consider special actions to get rid of the oldest file sooner.
    This include following possible actions:
      - rushing the oldest redo log consumer to consume faster,
      - truncating the redo log file if the oldest file is also the newest,
      - requesting extra intake generated with usage of dummy redo records. */

    if (log_files_consuming_oldest_file_takes_too_long(log)) {
      lsn_t oldest_needed_lsn;
      /* Note, that there is a possible race because the consumer
      has possibly already consumed what we wanted to request.
      Such spurious claims / requests are not considered dangerous. */
      if (auto *consumer = log_consumer_get_oldest(log, oldest_needed_lsn)) {
        consumer->consumption_requested();
      }
    }

    if (log_files_filling_oldest_file_takes_too_long(log)) {
      /* If there is more than one file, then the oldest file is already
      filled, so filling it will never be considered taking too long. */

      if (log_files_is_truncate_allowed(log)) {
        if (has_writer_mutex) {
          log_files_truncate(log);
        } else {
          return Iteration_result::RETRY_WITH_WRITER_MUTEX;
        }
      }
    }

    /* Re-check if filling the oldest file still takes too long,
    because the oldest file might have become truncated. */

    needs_more_intake = log_files_filling_oldest_file_takes_too_long(log);
  }

  log_files_create_next_as_unused_if_needed(log);

  os_event_set(log.m_files_governor_iteration_event);

  return needs_more_intake ? Iteration_result::COMPLETED_BUT_NEED_MORE_INTAKE
                           : Iteration_result::COMPLETED;
}

static void log_files_governor_iteration(log_t &log) {
  using Iteration_result = Log_files_governor_iteration_result;

  /* We can't use log_writer_mutex_own() here, because it could return true
  when the log_writer thread was inactive (gone). Note, that even though the
  log_writer thread's activity is checked before calling this function, there
  is no protection for that condition, so the log_writer thread could become
  inactive meanwhile. Even in such case, this function might still need to
  acquire the writer_mutex, in order to remove the oldest redo log files,
  or to truncate the single file.

  Note, that the property that the log.writer_mutex hasn't been acquired yet,
  is also important for the mechanism which generates dummy redo records, to
  avoid a possible deadlock when there was no space in the log buffer.

  However, the function which generates dummy redo records could only be called
  if the log_files_governor thread still hasn't promised not to generate dummy
  redo records, and for such promise the log_writer thread is waiting before it
  decides to stop and can become inactive. */
  ut_ad(!mutex_own(&log.writer_mutex));
  ut_ad(!log_files_mutex_own(log));

  log_sync_point("log_before_file_consume");

  Iteration_result result = log_files_governor_iteration_low(log, false);

  if (result == Iteration_result::RETRY_WITH_WRITER_MUTEX) {
    IB_mutex_guard writer_latch{&(log.writer_mutex), UT_LOCATION_HERE};
    result = log_files_governor_iteration_low(log, true);
    ut_a(result != Iteration_result::RETRY_WITH_WRITER_MUTEX);
  }

  if (log.m_no_more_dummy_records_requested.load()) {
    log.m_no_more_dummy_records_promised.store(true);

  } else if (result == Iteration_result::COMPLETED_BUT_NEED_MORE_INTAKE &&
             !log_free_check_is_required(log)) {
    ut_ad(!log.m_no_more_dummy_records_promised.load());
    log_files_generate_dummy_records(log, LOG_FILES_DUMMY_INTAKE_SIZE);
  }
}

void log_files_governor(log_t *log_ptr) {
  ut_a(log_ptr != nullptr);
  log_t &log = *log_ptr;

  ut_d(log.m_files_governor_thd = create_internal_thd());

  // We need to initialize a temporary to work around a gcc12 bug.
  Log_files_stats tmp{};
  log.m_files_stats = tmp;

  while (true) {
    /* We note down value of this event's sig_count before calling the
    log_files_governor_iteration() to avoid waiting on the event after
    the call is finished if the event was signalled meanwhile.

    The log_writer's finish is announced by log_stop_background_threads()
    setting this event, so not to miss it, we note down sig_count before
    checking log_writer's status. */
    const auto sig_count = os_event_reset(log.m_files_governor_event);
    if (!log_writer_is_active()) {
      break;
    }

    log_files_governor_iteration(log);
    os_event_wait_time_low(log.m_files_governor_event,
                           std::chrono::milliseconds{10}, sig_count);
  }

  {
    IB_mutex_guard writer_latch{&(log.writer_mutex), UT_LOCATION_HERE};
    IB_mutex_guard files_latch{&(log.m_files_mutex), UT_LOCATION_HERE};
    log_files_update_capacity_limits(log);
    log_files_mark_consumed_files(log);
    log_files_process_consumed_files(log);
  }

  ut_d(destroy_internal_thd(log.m_files_governor_thd));
}

void log_files_wait_for_next_file_available(log_t &log) {
  log_files_mutex_enter(log);

  const auto sig_count = os_event_reset(log.m_file_removed_event);

  if (log_files_next_file_size(log) != 0) {
    log_files_mutex_exit(log);
    return;
  }

  log.m_requested_files_consumption = true;

  log_files_mutex_exit(log);

  os_event_set(log.m_files_governor_event);

  log_sync_point("log_before_waiting_for_next_file");

  /* Wait for 100ms or until some log file is removed. */
  os_event_wait_time_low(log.m_file_removed_event,
                         std::chrono::milliseconds{100}, sig_count);
}

static dberr_t log_files_prepare_unused_file(log_t &log, Log_file_id file_id,
                                             lsn_t start_lsn,
                                             lsn_t checkpoint_lsn,
                                             bool create_first_data_block,
                                             os_offset_t &file_size) {
  log_files_write_allowed_validate(log);

  file_size = log_files_next_file_size(log);
  ut_a(file_size != 0); /* verified in log_files_produce_file() */

  lsn_t end_lsn;
  const bool end_lsn_can_be_computed =
      log_file_compute_end_lsn(start_lsn, file_size, end_lsn);
  ut_a(end_lsn_can_be_computed);

  ut_a(checkpoint_lsn == 0 || start_lsn <= checkpoint_lsn);
  ut_a(checkpoint_lsn < end_lsn);

  dberr_t err;

  if (log.m_unused_files_count > 0) {
    err = log_resize_unused_file(log.m_files_ctx, file_id, file_size);
    if (err != DB_SUCCESS) {
      return err;
    }

  } else {
    err = log_create_unused_file(log.m_files_ctx, file_id, file_size);
    if (err != DB_SUCCESS) {
      return err;
    }
    log.m_unused_files_count++;
  }

  const Log_file_header header = log_files_prepare_header(log, start_lsn);

  auto file_handle =
      Log_file::open(log.m_files_ctx, file_id, Log_file_access_mode::READ_WRITE,
                     log.m_encryption_metadata, Log_file_type::UNUSED);
  if (file_handle.is_open()) {
    err = log_file_header_write(file_handle, header);
  } else {
    err = DB_CANNOT_OPEN_FILE;
  }
  if (err != DB_SUCCESS) {
    return err;
  }

  RECOVERY_CRASH(9);

  /* Write the first checkpoint twice to overwrite both checkpoint headers. */
  err = log_files_write_checkpoint_low(
      log, file_handle, Log_checkpoint_header_no::HEADER_1, checkpoint_lsn);
  if (err != DB_SUCCESS) {
    return err;
  }
  err = log_files_write_checkpoint_low(
      log, file_handle, Log_checkpoint_header_no::HEADER_2, checkpoint_lsn);
  if (err != DB_SUCCESS) {
    return err;
  }

  if (create_first_data_block) {
    ut_a(checkpoint_lsn >= start_lsn);
    err = log_files_write_first_data_block_low(log, file_handle, checkpoint_lsn,
                                               start_lsn);
  } else {
    /* A new log file should have 0 data blocks written. This is guaranteed,
    because the file either:
      - became created and resized, in which case it is filled with 0x00,
      - or became recycled, in which case it contains old data blocks,
        which have smaller epoch_no or hdr_no field, marking end of recovery.
    Note, that InnoDB cannot write the first data block as empty one here,
    because it would potentially have an invalid first_rec_group field. */
    err = DB_SUCCESS;
  }

  return err;
}

static dberr_t log_files_create_file(log_t &log, Log_file_id file_id,
                                     lsn_t start_lsn, lsn_t checkpoint_lsn,
                                     bool create_first_data_block) {
  log_files_write_allowed_validate(log);

  os_offset_t file_size;
  dberr_t err =
      log_files_prepare_unused_file(log, file_id, start_lsn, checkpoint_lsn,
                                    create_first_data_block, file_size);

  const auto file_path = log_file_path(log.m_files_ctx, file_id);

  if (err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_LOG_FILE_PREPARE_ON_CREATE_FAILED, file_path.c_str(),
              static_cast<int>(err), ulonglong{start_lsn});
    return err;
  }

  RECOVERY_CRASH(10);

  err = log_mark_file_as_in_use(log.m_files_ctx, file_id);

  if (err != DB_SUCCESS) {
    const auto unused_file_path =
        log_file_path_for_unused_file(log.m_files_ctx, file_id);
    ib::error(ER_IB_MSG_LOG_FILE_RENAME_ON_CREATE_FAILED,
              unused_file_path.c_str(), file_path.c_str(),
              static_cast<int>(err));
    return err;
  }

  if (!log.m_files.empty()) {
    auto it = log.m_files.end();
    it--;
    ut_a(it->next_id() == file_id);
    ut_a(it->m_end_lsn == start_lsn);
  }

  log.m_files.add(file_id, file_size, start_lsn, false,
                  log.m_encryption_metadata);
  log.m_unused_files_count--;

  RECOVERY_CRASH(11);
  return DB_SUCCESS;
}

dberr_t log_files_create(log_t &log, lsn_t flushed_lsn) {
  log_files_initialize_on_empty_redo(log);

  dberr_t err;
  ut_a(log_is_data_lsn(flushed_lsn));
  log_files_create_allowed_validate();
  RECOVERY_CRASH(8);

  /* Do not allow to create new log files if redo log directory isn't empty. */
  ut::vector<Log_file_id> listed_files;
  err = log_list_existing_files(log.m_files_ctx, listed_files);
  ut_a(err == DB_SUCCESS);
  ut_a(listed_files.empty());

  ut_a(log.m_format == Log_format::CURRENT);
  ut_a(log.m_creator_name == LOG_HEADER_CREATOR_CURRENT);

  log.m_log_flags = Log_flags{0};
  log_file_header_set_flag(log.m_log_flags, LOG_HEADER_FLAG_NOT_INITIALIZED);

  log.m_log_uuid = log_generate_uuid();

  /* Create the first checkpoint and flush headers of the first log
  file (the flushed headers store information about the checkpoint,
  format of redo log and that is neither created by mysqlbackup
  nor by clone). */
  /* Start lsn stored in header of the first log file is divisible
  by OS_FILE_LOG_BLOCK_SIZE. Also, we want the MTR data to start
  immediately after the header.
  To achieve this, flushed_lsn should point to header's end.*/

  ut_a(flushed_lsn % OS_FILE_LOG_BLOCK_SIZE == LOG_BLOCK_HDR_SIZE);
  const lsn_t file_start_lsn = flushed_lsn - LOG_BLOCK_HDR_SIZE;

  ut_a(log.last_checkpoint_lsn.load() == 0);

  err = log_files_create_file(log, LOG_FIRST_FILE_ID, file_start_lsn,
                              flushed_lsn, true);
  if (err != DB_SUCCESS) {
    return err;
  }

  log.last_checkpoint_lsn.store(flushed_lsn);

  /* If the redo log is set to be encrypted,
  initialize encryption information. */
  if (srv_redo_log_encrypt) {
    if (!Encryption::check_keyring()) {
      ib::error(ER_IB_MSG_1065);
      return DB_ERROR;
    }
    if (log_encryption_generate_metadata(log) != DB_SUCCESS) {
      return DB_ERROR;
    }
  }

  RECOVERY_CRASH(12);

  log_persist_initialized(log);

  ib::info(ER_IB_MSG_LOG_FILES_INITIALIZED, ulonglong{flushed_lsn});

  RECOVERY_CRASH(13);

  return DB_SUCCESS;
}

void log_files_remove(log_t &log) {
  /* InnoDB doesn't want to end up in situation, that it removed redo files
  and can't create new redo files. */
  log_files_create_allowed_validate();

  if (log.m_current_file_handle.is_open()) {
    log.m_current_file_handle.close();
  }

  /* Remove any old log files. */

#ifdef UNIV_DEBUG
  dberr_t first_file_remove_err;
  Log_file_id first_file_id;
  std::tie(first_file_remove_err, first_file_id) =
      log_remove_file(log.m_files_ctx);
  ut_a(first_file_remove_err == DB_SUCCESS ||
       first_file_remove_err == DB_NOT_FOUND);

  /* Crashing after deleting the first
  file should be recoverable. The buffer
  pool was clean, and we can simply create
  all log files from the scratch. */
  RECOVERY_CRASH(7);
#endif /* UNIV_DEBUG */

  auto remove_files_ret = log_remove_files(log.m_files_ctx);
  ut_a(remove_files_ret.first == DB_SUCCESS);

#ifdef UNIV_DEBUG
  if (first_file_remove_err == DB_SUCCESS) {
    remove_files_ret.second.push_back(first_file_id);
  }
#endif /* UNIV_DEBUG */

  for (Log_file_id id : remove_files_ret.second) {
    log.m_files.erase(id);
  }

  const auto ret = log_remove_unused_files(log.m_files_ctx);
  ut_a(ret.first == DB_SUCCESS);

  log.m_unused_files_count = 0;
}

dberr_t log_files_start(log_t &log) {
  ut_a(!log_writer_is_active());
  ut_a(!log_checkpointer_is_active());
  ut_a(!log_files_governor_is_active());

  /** Existing log files are marked as not consumed. */
  log_files_for_each(log.m_files,
                     [](const Log_file &file) { ut_a(!file.m_consumed); });

  if (srv_read_only_mode) {
    log_update_exported_variables(log);
    /* We are not allowed to consume in read-only mode. */
    return DB_SUCCESS;
  }

  if (log.m_format < Log_format::VERSION_8_0_30) {
    /* We are not allowed to consume log files of format
    older than 8.0.30. */
    return DB_SUCCESS;
  }

  ut_a(log_get_lsn(log) == log.write_lsn.load());
  ut_a(log_get_lsn(log) == log.flushed_to_disk_lsn.load());

  log_files_update_current_file_low(log);

  log_files_mark_consumed_files(log);

  log_files_validate_not_consumed(log);

  ut_a(!srv_read_only_mode);

  log_files_initialize_on_existing_redo(log);

  /* It could happen that write_lsn was in a new file, but flushed_to_disk_lsn
  was still in the previous file when server rebooted. In such case we might
  have recovery ended in the previous file successfully and need to remove the
  new file (containing some unflushed data). */
  const dberr_t err = log_files_remove_from_future(log);
  if (err != DB_SUCCESS) {
    return err;
  }

  log_files_process_consumed_files(log);

  return DB_SUCCESS;
}

static dberr_t log_files_mark_current_file_as_incomplete(log_t &log) {
  log_files_write_allowed_validate(log);
  auto header = log_files_prepare_header(log, log.m_current_file);
  log_file_header_reset_flag(header.m_log_flags, LOG_HEADER_FLAG_FILE_FULL);
  const dberr_t err = log_file_header_write(log.m_current_file_handle, header);
  if (err != DB_SUCCESS) {
    return err;
  }
  const std::string file_path =
      log_file_path(log.m_files_ctx, log.m_current_file.m_id);
  ib::info(ER_IB_MSG_LOG_FILE_MARK_CURRENT_AS_INCOMPLETE, file_path.c_str());
  log.m_current_file.m_full = false;
  log.m_files.set_incomplete(log.m_current_file.m_id);
  return DB_SUCCESS;
}

static dberr_t log_files_mark_current_file_as_full(log_t &log) {
  log_files_write_allowed_validate(log);

  const auto file_id = log.m_current_file.m_id;

  {
    bool found_newer = false;
    for (const auto &f : log.m_files) {
      if (f.m_id < file_id) {
        ut_a(f.m_full);
      } else {
        ut_a(!f.m_full);
        if (file_id < f.m_id) {
          found_newer = true;
        }
      }
    }
    ut_a(found_newer);
  }

  const auto file = log.m_files.file(file_id);
  ut_a(file != log.m_files.end());

  /* Prepare header with updated log_flags. */
  auto header = log_files_prepare_header(log, *file);
  ut_a(!log_file_header_check_flag(header.m_log_flags,
                                   LOG_HEADER_FLAG_FILE_FULL));
  log_file_header_set_flag(header.m_log_flags, LOG_HEADER_FLAG_FILE_FULL);
  /* Flush to disk. */
  auto file_handle = file->open(Log_file_access_mode::WRITE_ONLY);
  if (!file_handle.is_open()) {
    return DB_CANNOT_OPEN_FILE;
  }
  const dberr_t err = log_file_header_write(file_handle, header);
  if (err != DB_SUCCESS) {
    return err;
  }
  /* Update in in-memory dictionary of log files. */
  log.m_current_file.m_full = true;
  log.m_files.set_full(file_id);
  return DB_SUCCESS;
}

static dberr_t log_files_rewrite_old_headers(
    log_t &log, Log_file_header_callback before_write,
    Log_file_header_callback after_write) {
  log_files_write_allowed_validate(log);
  ut_a(!log.m_files.empty());

  auto newest_it = log.m_files.end();
  --newest_it;

  for (const auto &file : log.m_files) {
    if (file.m_id == newest_it->m_id) {
      continue;
    }
    ut_a(file.m_id < newest_it->m_id);

    auto header = log_files_prepare_header(log, file);
    before_write(file.m_id, header);

    auto file_handle = file.open(Log_file_access_mode::WRITE_ONLY);
    if (!file_handle.is_open()) {
      return DB_CANNOT_OPEN_FILE;
    }

    const dberr_t err = log_file_header_write(file_handle, header);
    if (err != DB_SUCCESS) {
      return err;
    }

    after_write(file.m_id, header);
  }
  return DB_SUCCESS;
}

static dberr_t log_files_rewrite_newest_header(
    log_t &log, Log_file_header_callback update_callback) {
  log_files_write_allowed_validate(log);
  ut_a(!log.m_files.empty());

  auto it = log.m_files.end();
  --it;

  auto header = log_files_prepare_header(log, *it);

  update_callback(it->m_id, header);

  auto file_handle = it->open(Log_file_access_mode::WRITE_ONLY);
  if (!file_handle.is_open()) {
    return DB_CANNOT_OPEN_FILE;
  }

  return log_file_header_write(file_handle, header);
}

dberr_t log_files_persist_flags(log_t &log, Log_flags log_flags) {
  const dberr_t err = log_files_rewrite_newest_header(
      log, [&](Log_file_id, Log_file_header &header) {
        ut_a(header.m_log_flags == log.m_log_flags);
        header.m_log_flags = log_flags;
      });
  if (err != DB_SUCCESS) {
    return err;
  }
  log.m_log_flags = log_flags;
  return DB_SUCCESS;
}

dberr_t log_files_reset_creator_and_set_full(log_t &log) {
  const std::string new_creator = LOG_HEADER_CREATOR_CURRENT;

  const dberr_t rewrite_old_err = log_files_rewrite_old_headers(
      log,
      [&](Log_file_id, Log_file_header &header) {
        ut_a(header.m_creator_name == log.m_creator_name);
        header.m_creator_name = new_creator;
        log_file_header_set_flag(header.m_log_flags, LOG_HEADER_FLAG_FILE_FULL);
      },
      [&](Log_file_id file_id, Log_file_header &) {
        log.m_files.set_full(file_id);
      });
  if (rewrite_old_err != DB_SUCCESS) {
    return rewrite_old_err;
  }

  const dberr_t rewrite_newest_err = log_files_rewrite_newest_header(
      log, [&](Log_file_id, Log_file_header &header) {
        ut_a(header.m_creator_name == log.m_creator_name);
        header.m_creator_name = new_creator;
      });
  if (rewrite_newest_err != DB_SUCCESS) {
    return rewrite_newest_err;
  }

  log.m_creator_name = new_creator;
  return DB_SUCCESS;
}

void log_files_update_encryption(
    log_t &log, const Encryption_metadata &encryption_metadata) {
  log_files_access_allowed_validate(log);
  log.m_encryption_metadata = encryption_metadata;
}

static void log_files_update_current_file_low(log_t &log) {
  log_files_write_allowed_validate(log);
  ut_ad(log_flusher_mutex_own(log));

  const lsn_t newest_lsn = log_files_newest_needed_lsn(log);
  ut_a(newest_lsn >= LOG_START_LSN);

  const auto it = log.m_files.find(newest_lsn);
  ut_a(it != log.m_files.end());
  ut_a(!it->m_consumed);

  if (log.m_current_file_handle.is_open()) {
    log.m_current_file_handle.close();
  }

  log.m_current_file = *it;

  log.m_current_file_handle =
      log.m_current_file.open(Log_file_access_mode::WRITE_ONLY);

  ut_a(log.m_current_file_handle.is_open());

  log_files_validate_current_file(log);
}

static void log_files_generate_dummy_records(log_t &log, lsn_t min_bytes) {
  ut_ad(log_writer_is_active());
  ut_ad(!log_writer_mutex_own(log));
  ut_ad(log_checkpointer_is_active());
  ut_ad(log.m_allow_checkpoints.load());
  ut_ad(!log_checkpointer_mutex_own(log));
  ut_ad(log_flusher_is_active());
  ut_ad(!log_flusher_mutex_own(log));
  ut_ad(!log_files_mutex_own(log));
  ut_ad(!log.m_no_more_dummy_records_promised.load());

  ut_d(const lsn_t start_lsn = log_get_lsn(log));
  byte *buf;
  mtr_t mtr;
  mtr_start(&mtr);
  lsn_t bytes_stored = 0;
  while (bytes_stored < min_bytes && mlog_open(&mtr, 1, buf)) {
    mach_write_to_1(buf, MLOG_DUMMY_RECORD);
    mlog_close(&mtr, buf + 1);
    mtr.added_rec();
    ++bytes_stored;
  }
  mtr_commit(&mtr);
  ut_ad(start_lsn + bytes_stored <= log_get_lsn(log));
  log_buffer_flush_to_disk(log, false);
}

static bool log_files_is_truncate_allowed(const log_t &log) {
  log_files_access_allowed_validate(log);

  /* It is guaranteed that checkpointer consumer is always there.
  Note, that we cannot use log.m_consumers.find() because it would
  expect Log_consumer* as argument and we can only provide the
  const Log_consumer*, because we have const ref to log_t here. */
  ut_a(std::find_if(log.m_consumers.begin(), log.m_consumers.end(),
                    [&log](Log_consumer *consumer) {
                      return consumer == &log.m_checkpoint_consumer;
                    }) != log.m_consumers.end());

  /* Allow truncation of redo files only if there are no other consumers
  than redo log checkpointer. The truncation acquires the checkpointer
  mutex. */
  return log.m_consumers.size() == 1;
}

void log_files_dummy_records_request_disable(log_t &log) {
  log.m_no_more_dummy_records_requested.store(true);
}

void log_files_dummy_records_disable(log_t &log) {
  log_files_dummy_records_request_disable(log);
  while (!log.m_no_more_dummy_records_promised.load()) {
    os_event_set(log.m_files_governor_event);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }
}

static void log_files_truncate(log_t &log) {
  log_files_write_allowed_validate(log);
  ut_a(log_files_is_truncate_allowed(log));

  const os_offset_t end_offset = ut_uint64_align_up(
      log.m_current_file.offset(log.write_lsn.load()), UNIV_PAGE_SIZE);

  const os_offset_t new_size =
      std::max(end_offset, log.m_capacity.next_file_size());

  if (log.m_current_file.m_size_in_bytes <= new_size) {
    return;
  }

  IB_mutex_guard flusher_latch{&(log.flusher_mutex), UT_LOCATION_HERE};

  if (log.m_current_file_handle.is_open()) {
    log.m_current_file_handle.close();
  }

  const auto file_path =
      log_file_path(log.m_files_ctx, log.m_current_file.m_id);
  ib::info(ER_IB_MSG_LOG_FILE_TRUNCATE, file_path.c_str());

  const dberr_t err =
      log_resize_file(log.m_files_ctx, log.m_current_file.m_id, new_size);
  ut_a(err == DB_SUCCESS);

  log.m_files.set_size(log.m_current_file.m_id, new_size);

  log_files_update_current_file_low(log);
  log.write_ahead_end_offset = 0;

  log_files_update_capacity_limits(log);
}

/** @} */

/**************************************************/ /**

 @name Log - files initialization and handling sysvar updates

 *******************************************************/

/** @{ */

/** Computes initial capacity limits and size suggested for the next log file.
Called after existing redo log files have been discovered (log.m_files), or
when logically empty redo log is being initialized.

Requirement: srv_is_being_started is true.

@param[in,out]  log                     redo log
@param[in]      current_logical_size    current logical size of the redo log
@param[in]      current_checkpoint_age  current checkpoint age */
static void log_files_initialize(log_t &log, lsn_t current_logical_size,
                                 lsn_t current_checkpoint_age) {
  ut_a(srv_is_being_started);
  ut_a(log.m_files_ctx.m_files_ruleset == Log_files_ruleset::CURRENT);
  log.m_capacity.initialize(log.m_files, current_logical_size,
                            current_checkpoint_age);
  log_update_limits_low(log);
  log_update_exported_variables(log);
}

void log_files_initialize_on_empty_redo(log_t &log) {
  log_files_initialize(log, 0, 0);
}

void log_files_initialize_on_existing_redo(log_t &log) {
  lsn_t logical_size, checkpoint_age;
  std::tie(logical_size, checkpoint_age) =
      log_files_logical_size_and_checkpoint_age(log);

  log_files_initialize(log, logical_size, checkpoint_age);
}

/** Waits until the log_files_governor performs a next iteration of its loop.
Notifies the log_files_governor thread (to ensure it is soon).
@param[in,out]  log   redo log */
static void log_files_wait_until_next_governor_iteration(log_t &log) {
  const auto sig_count = os_event_reset(log.m_files_governor_iteration_event);
  os_event_set(log.m_files_governor_event);
  os_event_wait_low(log.m_files_governor_iteration_event, sig_count);
}

void log_files_resize_requested(log_t &log) {
  log_files_wait_until_next_governor_iteration(log);
}

void log_files_thread_concurrency_updated(log_t &log) {
  log_files_wait_until_next_governor_iteration(log);
}

/** @} */

#endif /* !UNIV_HOTBACKUP */
