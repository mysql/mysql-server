/*****************************************************************************

Copyright (c) 1995, 2024, Oracle and/or its affiliates.
Copyright (c) 2009, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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
 @file include/log0buf.h

 Redo log functions related to the log buffer.

 *******************************************************/

#ifndef log0buf_h
#define log0buf_h

/* MEB should neither write to the log buffer nor maintain
the log buffer or recent_written / recent_closed buffers. */
#ifndef UNIV_HOTBACKUP

/* log.recent_written, log.recent_closed */
#include "log0sys.h"

/* log_t&, lsn_t, constants */
#include "log0types.h"

/**************************************************/ /**

 @name Log - writing to the log buffer.

 @remarks
 These functions are designed for mtr_commit(),
 and used only there (except log0log-t.cc).

 *******************************************************/

/** @{ */

/** Handle which is used for writes to the log buffer. */
struct Log_handle {
  /** LSN of the first data byte. */
  lsn_t start_lsn;

  /** LSN after the last data byte. */
  lsn_t end_lsn;
};

/** Acquires the log buffer x-lock.
@param[in,out]	log	redo log */
void log_buffer_x_lock_enter(log_t &log);

/** Releases the log buffer x-lock.
@param[in,out]	log	redo log */
void log_buffer_x_lock_exit(log_t &log);

/** Reserves space in the redo log for following write operations.
Space is reserved for a given number of data bytes. Additionally
bytes for required headers and footers of log blocks are reserved.

After the space is reserved, range of lsn values from a start_lsn
to an end_lsn is assigned. The log writer thread cannot proceed
further than to the start_lsn, until a link start_lsn -> end_lsn
has been added to the log recent written buffer.

NOTE that the link is added after data is written to the reserved
space in the log buffer. It is very critical to do all these steps
as fast as possible, because very likely the log writer thread is
waiting for the link.

@param[in,out]	log	redo log
@param[in]	len	number of data bytes to reserve for write
@return handle that represents the reservation */
Log_handle log_buffer_reserve(log_t &log, size_t len);

/** Writes data to the log buffer. The space in the redo log has to be
reserved before calling to this function and lsn pointing to inside the
reserved range of lsn values has to be provided.

The write does not have to cover the whole reserved space, but may not
overflow it. If it does not cover, then returned value should be used
to start the next write operation. Note that finally we must use exactly
all the reserved space.

@param[in,out]	log		redo log
@param[in]	str		memory to write data from
@param[in]	str_len		number of bytes to write
@param[in]	start_lsn	lsn to start writing at (the reserved space)

@return end_lsn after writing the data (in the reserved space), could be
used to start the next write operation if there is still free space in
the reserved space */
lsn_t log_buffer_write(log_t &log, const byte *str, size_t str_len,
                       lsn_t start_lsn);

/** Adds a link start_lsn -> end_lsn to the log recent written buffer.

This function must be called after the data has been written to the
fragment of log buffer represented by range [start_lsn, end_lsn).
After the link is added, the log writer may write the data to disk.

NOTE that still dirty pages for the [start_lsn, end_lsn) are not added
to flush lists when this function is called.

@param[in,out]	log		redo log
@param[in]	start_lsn	start_lsn of the link to add
@param[in]	end_lsn		end_lsn of the link to add */
void log_buffer_write_completed(log_t &log, lsn_t start_lsn, lsn_t end_lsn);

/** Modifies header of log block in the log buffer, which contains
a given lsn value, and sets offset to the first group of log records
within the block.

This is used by mtr after writing a log record group which ends at
lsn belonging to different log block than lsn at which the group was
started. When write was finished at the last data byte of log block,
it is considered ended in the next log block, because the next data
byte belongs to that block.

During recovery, when recovery is started in the middle of some group
of log records, it first looks for the beginning of the next group.

@param[in,out]	log			redo log
@param[in]	rec_group_end_lsn	lsn at which the first log record
group starts within the block containing this lsn value */
void log_buffer_set_first_record_group(log_t &log, lsn_t rec_group_end_lsn);

/** Adds a link start_lsn -> end_lsn to the log recent closed buffer.

This is called after all dirty pages related to [start_lsn, end_lsn)
have been added to corresponding flush lists.
For detailed explanation - @see log0write.cc.

@param[in,out]	log		redo log
@param[in]	handle		handle for the reservation of space */
void log_buffer_close(log_t &log, const Log_handle &handle);

/** @} */

/**************************************************/ /**

 @name Log - management of the log buffer.

 *******************************************************/

/** @{ */

/** Updates limit used when writing to log buffer. Note that the
log buffer may have space for log records for which we still do
not have space in log files (for larger lsn values).
@param[in,out]   log        redo log */
void log_update_buf_limit(log_t &log);

/** Updates limit used when writing to log buffer, according to provided
write_lsn. It must be <= log.write_lsn.load() to protect from log buffer
overwrites.
@param[in,out]   log        redo log
@param[in]       write_lsn  value <= log.write_lsn.load() */
void log_update_buf_limit(log_t &log, lsn_t write_lsn);

/** Write to the log file up to the last log entry.
@param[in,out]	log	redo log
@param[in]	sync	whether we want the written log
also to be flushed to disk. */
void log_buffer_flush_to_disk(log_t &log, bool sync = true);

/** Requests flush of the log buffer.
@param[in]	sync	true: wait until the flush is done */
void log_buffer_flush_to_disk(bool sync = true);

/** Writes the log buffer to the log file. It is intended to be called from
background master thread periodically. If the log writer threads are active,
this function writes nothing. */
void log_buffer_sync_in_background();

/** Get last redo block from redo buffer and end LSN. Note that it takes
x-lock on the log buffer for a short period. Out values are always set,
even when provided last_block is nullptr.
@param[in,out]	log		redo log
@param[out]	last_lsn	end lsn of last mtr
@param[out]	last_block	last redo block
@param[in,out]	block_len	length in bytes */
void log_buffer_get_last_block(log_t &log, lsn_t &last_lsn, byte *last_block,
                               uint32_t &block_len);

/** Changes size of the log buffer. This is a thread-safe version.
It is used by SET GLOBAL innodb_log_buffer_size = X.
@param[in,out]  log       redo log
@param[in]      new_size  requested new size
@return true iff succeeded in resize */
bool log_buffer_resize(log_t &log, size_t new_size);

/** Changes size of the log buffer. This is a non-thread-safe version
which might be invoked only when there are no concurrent possible writes
to the log buffer. It is used in log_buffer_reserve() when a requested
size to reserve is larger than size of the log buffer.
@param[in,out]  log       redo log
@param[in]      new_size  requested new size
@param[in]      end_lsn   maximum lsn written to log buffer
@return true iff succeeded in resize */
bool log_buffer_resize_low(log_t &log, size_t new_size, lsn_t end_lsn);

/** @} */

/**************************************************/ /**

 @name Log - the recent written, the recent closed buffers.

 *******************************************************/

/** @{ */

#define log_closer_mutex_enter(log) mutex_enter(&((log).closer_mutex))

#define log_closer_mutex_enter_nowait(log) \
  mutex_enter_nowait(&((log).closer_mutex))

#define log_closer_mutex_exit(log) mutex_exit(&((log).closer_mutex))

/** @return lsn up to which all writes to log buffer have been finished */
inline lsn_t log_buffer_ready_for_write_lsn(const log_t &log) {
  return log.recent_written.tail();
}

/** @return lsn up to which all dirty pages have been added to flush list */
inline lsn_t log_buffer_dirty_pages_added_up_to_lsn(const log_t &log) {
  return log.recent_closed.tail();
}

/** @return capacity of the recent_closed, or 0 if !log_use_threads() */
inline lsn_t log_buffer_flush_order_lag(const log_t &log) {
  return log.recent_closed.capacity();
}

/** Advances log.buf_ready_for_write_lsn using links in the recent written
buffer. It's used by the log writer thread only.
@param[in]	log	redo log */
void log_advance_ready_for_write_lsn(log_t &log);

/** Validates that all slots in log recent written buffer for lsn values
in range between begin and end, are empty. Used during tests, crashes the
program if validation does not pass.
@param[in]	log     redo log which buffer is validated
@param[in]	begin   validation start (inclusive)
@param[in]	end     validation end (exclusive) */
void log_recent_written_empty_validate(const log_t &log, lsn_t begin,
                                       lsn_t end);

/** Validates that all slots in log recent closed buffer for lsn values
in range between begin and end, are empty. Used during tests, crashes the
program if validation does not pass.
@param[in]	log		redo log which buffer is validated
@param[in]	begin		validation start (inclusive)
@param[in]	end		validation end (exclusive) */
void log_recent_closed_empty_validate(const log_t &log, lsn_t begin, lsn_t end);

/** Waits until there is free space in the log recent closed buffer
for any links start_lsn -> end_lsn, which start at provided start_lsn.
It does not add any link.

This is called just before dirty pages for [start_lsn, end_lsn)
are added to flush lists. That's because we need to guarantee,
that the delay until dirty page is added to flush list is limited.
For detailed explanation - @see log0write.cc.

@param[in,out]	log   redo log
@param[in]      lsn   lsn on which we wait (for any link: lsn -> x) */
void log_wait_for_space_in_log_recent_closed(log_t &log, lsn_t lsn);

/** @} */

#endif /* !UNIV_HOTBACKUP */

#endif /* !log0buf_h */
