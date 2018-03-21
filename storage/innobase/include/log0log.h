/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All rights reserved.
Copyright (c) 2009, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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
 @file include/log0log.h

 Redo log constants and functions.

 Types are defined inside log0types.h.

 Created 12/9/1995 Heikki Tuuri
 *******************************************************/

#ifndef log0log_h
#define log0log_h

#include "dyn0buf.h"
#include "univ.i"
#ifndef UNIV_HOTBACKUP
#include "sync0rw.h"
#endif /* !UNIV_HOTBACKUP */

#include "log0test.h"
#include "log0types.h"

/** Prefix for name of log file, e.g. "ib_logfile" */
constexpr const char *const ib_logfile_basename = "ib_logfile";

/* base name length(10) + length for decimal digits(22) */
constexpr uint32_t MAX_LOG_FILE_NAME = 32;

/** Magic value to use instead of log checksums when they are disabled. */
constexpr uint32_t LOG_NO_CHECKSUM_MAGIC = 0xDEADBEEFUL;

/** Absolute margin for the free space in the log, before a new query step
which modifies the database, is started. Expressed in number of pages. */
constexpr uint32_t LOG_CHECKPOINT_EXTRA_FREE = 8;

/** Per thread margin for the free space in the log, before a new query step
which modifies the database, is started. It's multiplied by maximum number
of threads, that can concurrently enter mini transactions. Expressed in
number of pages. */
constexpr uint32_t LOG_CHECKPOINT_FREE_PER_THREAD = 4;

/** Controls asynchronous making of a new checkpoint.
Should be bigger than LOG_POOL_PREFLUSH_RATIO_SYNC. */
constexpr uint32_t LOG_POOL_CHECKPOINT_RATIO_ASYNC = 32;

/** Controls synchronous preflushing of modified buffer pages. */
constexpr uint32_t LOG_POOL_PREFLUSH_RATIO_SYNC = 16;

/** Controls asynchronous preflushing of modified buffer pages.
Should be less than the LOG_POOL_PREFLUSH_RATIO_SYNC. */
constexpr uint32_t LOG_POOL_PREFLUSH_RATIO_ASYNC = 8;

/** The counting of lsn's starts from this value: this must be non-zero. */
constexpr lsn_t LOG_START_LSN = 16 * OS_FILE_LOG_BLOCK_SIZE;

/* Offsets used in a log block header. */

/** Block number which must be > 0 and is allowed to wrap around at 1G.
The highest bit is set to 1, if this is the first block in a call to
fil_io (for possibly many consecutive blocks). */
constexpr uint32_t LOG_BLOCK_HDR_NO = 0;

/** Mask used to get the highest bit in the hdr_no field. */
constexpr uint32_t LOG_BLOCK_FLUSH_BIT_MASK = 0x80000000UL;

/** Maximum allowed block's number (stored in hdr_no). */
constexpr uint32_t LOG_BLOCK_MAX_NO = 0x3FFFFFFFUL + 1;

/** Number of bytes written to this block (also header bytes). */
constexpr uint32_t LOG_BLOCK_HDR_DATA_LEN = 4;

/** Mask used to get the highest bit in the data len field,
this bit is to indicate if this block is encrypted or not. */
constexpr uint32_t LOG_BLOCK_ENCRYPT_BIT_MASK = 0x8000UL;

/** Offset of the first start of mtr log record group in this log block.
0 if none. If the value is the same as LOG_BLOCK_HDR_DATA_LEN, it means
that the first rec group has not yet been concatenated to this log block,
but if it will, it will start at this offset.

An archive recovery can start parsing the log records starting from this
offset in this log block, if value is not 0. */
constexpr uint32_t LOG_BLOCK_FIRST_REC_GROUP = 6;

/** 4 lower bytes of the value of log_sys->next_checkpoint_no when the log
block was last written to: if the block has not yet been written full,
this value is only updated before a log buffer flush. */
constexpr uint32_t LOG_BLOCK_CHECKPOINT_NO = 8;

/** Size of the log block's header in bytes. */
constexpr uint32_t LOG_BLOCK_HDR_SIZE = 12;

/* Offsets used in a log block's footer (refer to the end of the block). */

/** 4 byte checksum of the log block contents. In InnoDB versions < 3.23.52
this did not contain the checksum, but the same value as .._HDR_NO. */
constexpr uint32_t LOG_BLOCK_CHECKSUM = 4;

/** Size of the log block footer (trailer) in bytes. */
constexpr uint32_t LOG_BLOCK_TRL_SIZE = 4;

static_assert(LOG_BLOCK_HDR_SIZE + LOG_BLOCK_TRL_SIZE < OS_FILE_LOG_BLOCK_SIZE,
              "Header + footer cannot be larger than the whole log block.");

/** Size of log block's data fragment (where actual data is stored). */
constexpr uint32_t LOG_BLOCK_DATA_SIZE =
    OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_HDR_SIZE - LOG_BLOCK_TRL_SIZE;

/** Ensure, that 64 bits are enough to represent lsn values, when 63 bits
are used to represent sn values. It is enough to ensure that lsn < 2*sn,
and that is guaranteed if the overhead enumerated in lsn sequence is not
bigger than number of actual data bytes. */
static_assert(LOG_BLOCK_HDR_SIZE + LOG_BLOCK_TRL_SIZE < LOG_BLOCK_DATA_SIZE,
              "Overhead in LSN sequence cannot be bigger than actual data.");

/** Maximum possible sn value. */
constexpr sn_t SN_MAX = (1ULL << 62) - 1;

/** Maximum possible lsn value is slightly higher than the maximum sn value,
because lsn sequence enumerates also bytes used for headers and footers of
all log blocks. However, still 64-bits are enough to represent the maximum
lsn value, because only 63 bits are used to represent sn value. */
constexpr lsn_t LSN_MAX = (1ULL << 63) - 1;

/* Offsets inside the checkpoint pages (redo log format version 1). */

/** Checkpoint number. It's incremented by one for each consecutive checkpoint.
During recovery, all headers are scanned, and one with the maximum checkpoint
number is used for the recovery (checkpoint_lsn from the header is used). */
constexpr uint32_t LOG_CHECKPOINT_NO = 0;

/** Checkpoint lsn. Recovery starts from this lsn and searches for the first
log record group that starts since then. In InnoDB < 8.0, it was exact value
at which the first log record group started. Because of the relaxed order in
flush lists, checkpoint lsn values are not precise anymore (the maximum delay
related to the relaxed order in flush lists, is subtracted from oldest_lsn,
when writing a checkpoint). */
constexpr uint32_t LOG_CHECKPOINT_LSN = 8;

/** Offset within the log files, which corresponds to checkpoint lsn.
Used for calibration of lsn and offset calculations. */
constexpr uint32_t LOG_CHECKPOINT_OFFSET = 16;

/** Size of the log buffer, when the checkpoint write was started.
It seems to be write-only field in InnoDB. Not used by recovery.

@note
Note that when the log buffer is being resized, all the log background threads
are stopped, so there no is concurrent checkpoint write (the log_checkpointer
thread is stopped). */
constexpr uint32_t LOG_CHECKPOINT_LOG_BUF_SIZE = 24;

/** Offsets used in a log file header */

/** Log file header format identifier (32-bit unsigned big-endian integer).
This used to be called LOG_GROUP_ID and always written as 0,
because InnoDB never supported more than one copy of the redo log. */
constexpr uint32_t LOG_HEADER_FORMAT = 0;

/** 4 unused (zero-initialized) bytes. */
constexpr uint32_t LOG_HEADER_PAD1 = 4;

/** LSN of the start of data in this log file (with format version 1 and 2). */
constexpr uint32_t LOG_HEADER_START_LSN = 8;

/** A null-terminated string which will contain either the string 'MEB'
and the MySQL version if the log file was created by mysqlbackup,
or 'MySQL' and the MySQL version that created the redo log file. */
constexpr uint32_t LOG_HEADER_CREATOR = 16;

/** End of the log file creator field. */
constexpr uint32_t LOG_HEADER_CREATOR_END = LOG_HEADER_CREATOR + 32;

/** Contents of the LOG_HEADER_CREATOR field */
#define LOG_HEADER_CREATOR_CURRENT "MySQL " INNODB_VERSION_STR

/** Header is created during DB clone */
#define LOG_HEADER_CREATOR_CLONE "MySQL Clone"

/** First checkpoint field in the log header. We write alternately to
the checkpoint fields when we make new checkpoints. This field is only
defined in the first log file. */
constexpr uint32_t LOG_CHECKPOINT_1 = OS_FILE_LOG_BLOCK_SIZE;

/** Second checkpoint field in the header of the first log file. */
constexpr uint32_t LOG_CHECKPOINT_2 = 3 * OS_FILE_LOG_BLOCK_SIZE;

/** Size of log file's header. */
constexpr uint32_t LOG_FILE_HDR_SIZE = 4 * OS_FILE_LOG_BLOCK_SIZE;

/** Constants related to server variables (default, min and max values). */

/** Default value of innodb_log_write_max_size (in bytes). */
constexpr ulint INNODB_LOG_WRITE_MAX_SIZE_DEFAULT = 4096;

/** Default value of innodb_log_checkpointer_every (in milliseconds). */
constexpr ulong INNODB_LOG_CHECKPOINT_EVERY_DEFAULT = 1000;  // 1000ms = 1s

/** Default value of innodb_log_writer_spin_delay (in spin rounds). */
constexpr ulong INNODB_LOG_WRITER_SPIN_DELAY_DEFAULT = 25000;

/** Default value of innodb_log_writer_timeout (in microseconds). */
constexpr ulong INNODB_LOG_WRITER_TIMEOUT_DEFAULT = 10;

/** Default value of innodb_log_spin_cpu_abs_lwm.
Expressed in percent (80 stands for 80%) of a single CPU core. */
constexpr ulong INNODB_LOG_SPIN_CPU_ABS_LWM_DEFAULT = 80;

/** Default value of innodb_log_spin_cpu_pct_hwm.
Expressed in percent (50 stands for 50%) of all CPU cores. */
constexpr uint INNODB_LOG_SPIN_CPU_PCT_HWM_DEFAULT = 50;

/** Default value of innodb_log_wait_for_write_spin_delay (in spin rounds). */
constexpr ulong INNODB_LOG_WAIT_FOR_WRITE_SPIN_DELAY_DEFAULT = 25000;

/** Default value of innodb_log_wait_for_write_timeout (in microseconds). */
constexpr ulong INNODB_LOG_WAIT_FOR_WRITE_TIMEOUT_DEFAULT = 1000;

/** Default value of innodb_log_wait_for_flush_spin_delay (in spin rounds). */
constexpr ulong INNODB_LOG_WAIT_FOR_FLUSH_SPIN_DELAY_DEFAULT = 25000;

/** Default value of innodb_log_wait_for_flush_spin_hwm (in microseconds). */
constexpr ulong INNODB_LOG_WAIT_FOR_FLUSH_SPIN_HWM_DEFAULT = 400;

/** Default value of innodb_log_wait_for_flush_timeout (in microseconds). */
constexpr ulong INNODB_LOG_WAIT_FOR_FLUSH_TIMEOUT_DEFAULT = 1000;

/** Default value of innodb_log_flusher_spin_delay (in spin rounds). */
constexpr ulong INNODB_LOG_FLUSHER_SPIN_DELAY_DEFAULT = 25000;

/** Default value of innodb_log_flusher_timeout (in microseconds). */
constexpr ulong INNODB_LOG_FLUSHER_TIMEOUT_DEFAULT = 10;

/** Default value of innodb_log_write_notifier_spin_delay (in spin rounds). */
constexpr ulong INNODB_LOG_WRITE_NOTIFIER_SPIN_DELAY_DEFAULT = 0;

/** Default value of innodb_log_write_notifier_timeout (in microseconds). */
constexpr ulong INNODB_LOG_WRITE_NOTIFIER_TIMEOUT_DEFAULT = 10;

/** Default value of innodb_log_flush_notifier_spin_delay (in spin rounds). */
constexpr ulong INNODB_LOG_FLUSH_NOTIFIER_SPIN_DELAY_DEFAULT = 0;

/** Default value of innodb_log_flush_notifier_timeout (in microseconds). */
constexpr ulong INNODB_LOG_FLUSH_NOTIFIER_TIMEOUT_DEFAULT = 10;

/** Default value of innodb_log_closer_spin_delay (in spin rounds). */
constexpr ulong INNODB_LOG_CLOSER_SPIN_DELAY_DEFAULT = 0;

/** Default value of innodb_log_closer_timeout (in microseconds). */
constexpr ulong INNODB_LOG_CLOSER_TIMEOUT_DEFAULT = 1000;

/** Default value of innodb_log_buffer_size (in bytes). */
constexpr ulong INNODB_LOG_BUFFER_SIZE_DEFAULT = 16 * 1024 * 1024UL;

/** Minimum allowed value of innodb_log_buffer_size. */
constexpr ulong INNODB_LOG_BUFFER_SIZE_MIN = 256 * 1024UL;

/** Maximum allowed value of innodb_log_buffer_size. */
constexpr ulong INNODB_LOG_BUFFER_SIZE_MAX = ULONG_MAX;

/** Default value of innodb_log_recent_written_size (in bytes). */
constexpr ulong INNODB_LOG_RECENT_WRITTEN_SIZE_DEFAULT = 1024 * 1024;

/** Minimum allowed value of innodb_log_recent_written_size. */
constexpr ulong INNODB_LOG_RECENT_WRITTEN_SIZE_MIN = OS_FILE_LOG_BLOCK_SIZE;

/** Maximum allowed value of innodb_log_recent_written_size. */
constexpr ulong INNODB_LOG_RECENT_WRITTEN_SIZE_MAX = 1024 * 1024 * 1024UL;

/** Default value of innodb_log_recent_closed_size (in bytes). */
constexpr ulong INNODB_LOG_RECENT_CLOSED_SIZE_DEFAULT = 2 * 1024 * 1024;

/** Minimum allowed value of innodb_log_recent_closed_size. */
constexpr ulong INNODB_LOG_RECENT_CLOSED_SIZE_MIN = OS_FILE_LOG_BLOCK_SIZE;

/** Maximum allowed value of innodb_log_recent_closed_size. */
constexpr ulong INNODB_LOG_RECENT_CLOSED_SIZE_MAX = 1024 * 1024 * 1024UL;

/** Default value of innodb_log_events (number of events). */
constexpr ulong INNODB_LOG_EVENTS_DEFAULT = 2048;

/** Minimum allowed value of innodb_log_events. */
constexpr ulong INNODB_LOG_EVENTS_MIN = 1;

/** Maximum allowed value of innodb_log_events. */
constexpr ulong INNODB_LOG_EVENTS_MAX = 1024 * 1024 * 1024UL;

/** Default value of innodb_log_write_ahead_size (in bytes). */
constexpr ulong INNODB_LOG_WRITE_AHEAD_SIZE_DEFAULT = 8192;

/** Minimum allowed value of innodb_log_write_ahead_size. */
constexpr ulong INNODB_LOG_WRITE_AHEAD_SIZE_MIN = OS_FILE_LOG_BLOCK_SIZE;

/** Maximum allowed value of innodb_log_write_ahead_size. */
constexpr ulint INNODB_LOG_WRITE_AHEAD_SIZE_MAX =
    UNIV_PAGE_SIZE_DEF;  // 16kB...

/** Value to which MLOG_TEST records should sum up within a group. */
constexpr int64_t MLOG_TEST_VALUE = 10000;

/** Maximum size of single MLOG_TEST record (in bytes). */
constexpr uint32_t MLOG_TEST_MAX_REC_LEN = 100;

/** Maximum number of MLOG_TEST records in single group of log records. */
constexpr uint32_t MLOG_TEST_GROUP_MAX_REC_N = 100;

/** Redo log system (singleton). */
extern log_t *log_sys;

/** Pointer to the log checksum calculation function. Changes are protected
by log_mutex_enter_all, which also stops the log background threads. */
extern log_checksum_func_t log_checksum_algorithm_ptr;

#ifndef UNIV_HOTBACKUP
/** Represents currently running test of redo log, nullptr otherwise. */
extern std::unique_ptr<Log_test> log_test;
#endif /* !UNIV_HOTBACKUP */

/* Declaration of inline functions (definition is available in log0log.ic). */

/** Gets a log block flush bit. The flush bit is set, if and only if,
the block was the first block written in a call to fil_io().

During recovery, when encountered the flush bit, recovery code can be
pretty sure, that all previous blocks belong to a completed fil_io(),
because the block with flush bit belongs to the next call to fil_io(),
which could only be started after the previous one has been finished.

@param[in]	log_block	log block
@return true if this block was the first to be written in fil_io(). */
inline bool log_block_get_flush_bit(const byte *log_block);

/** Sets the log block flush bit.
@param[in,out]	log_block	log block (must have hdr_no != 0)
@param[in]	value		value to set */
inline void log_block_set_flush_bit(byte *log_block, bool value);

/** Gets a log block number stored in the header. The number corresponds
to lsn range for data stored in the block.

During recovery, when a next block is being parsed, a next range of lsn
values is expected to be read. This corresponds to a log block number
increased by one. However, if a smaller number is read from the header,
it is then considered the end of the redo log and recovery is finished.
In such case, the next block is most likely an empty block or a block
from the past, because the redo log is written in circular manner.

@param[in]	log_block	log block (may be invalid or empty block)
@return log block number stored in the block header */
inline uint32_t log_block_get_hdr_no(const byte *log_block);

/** Sets the log block number stored in the header.
NOTE that this must be set before the flush bit!

@param[in,out]	log_block	log block
@param[in]	n		log block number: must be in (0, 1G] */
inline void log_block_set_hdr_no(byte *log_block, uint32_t n);

/** Gets a log block data length.
@param[in]	log_block	log block
@return log block data length measured as a byte offset from the block start */
inline uint32_t log_block_get_data_len(const byte *log_block);

/** Sets the log block data length.
@param[in,out]	log_block	log block
@param[in]	len		data length (@see log_block_get_data_len) */
inline void log_block_set_data_len(byte *log_block, uint32_t len);

/** Gets an offset to the beginning of the first group of log records
in a given log block.
@param[in]	log_block	log block
@return first mtr log record group byte offset from the block start,
0 if none. */
inline uint32_t log_block_get_first_rec_group(const byte *log_block);

/** Sets an offset to the beginning of the first group of log records
in a given log block.
@param[in,out]	log_block	log block
@param[in]	offset		offset, 0 if none */
inline void log_block_set_first_rec_group(byte *log_block, uint32_t offset);

/** Gets a log block checkpoint number field (4 lowest bytes).
@param[in]	log_block	log block
@return checkpoint no (4 lowest bytes) */
inline uint32_t log_block_get_checkpoint_no(const byte *log_block);

/** Sets a log block checkpoint number field (4 lowest bytes).
@param[in,out]	log_block	log block
@param[in]	no		checkpoint no */
inline void log_block_set_checkpoint_no(byte *log_block, uint64_t no);

/** Converts a lsn to a log block number. Consecutive log blocks have
consecutive numbers (unless the sequence wraps). It is guaranteed that
the calculated number is greater than zero.

@param[in]	lsn	lsn of a byte within the block
@return log block number, it is > 0 and <= 1G */
inline uint32_t log_block_convert_lsn_to_no(lsn_t lsn);

/** Calculates the checksum for a log block.
@param[in]	log_block	log block
@return checksum */
inline uint32_t log_block_calc_checksum(const byte *log_block);

/** Calculates the checksum for a log block using the MySQL 5.7 algorithm.
@param[in]	log_block	log block
@return checksum */
inline uint32_t log_block_calc_checksum_crc32(const byte *log_block);

/** Calculates the checksum for a log block using the "no-op" algorithm.
@param[in]     log_block   log block
@return        checksum */
inline uint32_t log_block_calc_checksum_none(const byte *log_block);

/** Gets value of a log block checksum field.
@param[in]	log_block	log block
@return checksum */
inline uint32_t log_block_get_checksum(const byte *log_block);

/** Sets value of a log block checksum field.
@param[in,out]	log_block	log block
@param[in]	checksum	checksum */
inline void log_block_set_checksum(byte *log_block, uint32_t checksum);

/** Stores a 4-byte checksum to the trailer checksum field of a log block.
This is used before writing the log block to disk. The checksum in a log
block is used in recovery to check the consistency of the log block.
@param[in]	log_block	 log block (completely filled in!) */
inline void log_block_store_checksum(byte *log_block);

/** Gets the current lsn value. This value points to the first non
reserved data byte in the redo log. When next user thread reserves
space in the redo log, it starts at this lsn.

If the last reservation finished exactly before footer of log block,
this value points to the first byte after header of the next block.

NOTE that it is possible that the current lsn value does not fit
free space in the log files or in the log buffer. In such case,
user threads need to wait until the space becomes available.

@return current lsn */
inline lsn_t log_get_lsn(const log_t &log);

/** Gets the last checkpoint lsn stored and flushed to disk.
@return last checkpoint lsn */
inline lsn_t log_get_checkpoint_lsn(const log_t &log);

#ifndef UNIV_HOTBACKUP

/** Gets capacity of log files excluding headers of the log files.
@return capacity for bytes addressed by lsn (including headers and footers
of log blocks, excluding headers of log files) */
inline lsn_t log_get_capacity();

/** When the oldest dirty page age exceeds this value, we start
an asynchronous preflush of dirty pages. This function does not
have side-effects, it only reads and returns the limit value.
@return age of dirty page at which async. preflush is started */
inline lsn_t log_get_max_modified_age_async();

/** @return true iff log_free_check should be executed. */
inline bool log_needs_free_check();

/** Any database operation should call this when it has modified more than
about 4 pages. NOTE that this function may only be called when the thread
owns no synchronization objects except the dictionary mutex.

Checks if current log.sn exceeds log.sn_limit_for_start, in which case waits.
This is supposed to guarantee that we would not run out of space in the log
files when holding latches of some dirty pages (which could end up in
a deadlock, because flush of the latched dirty pages could be required
to reclaim the space and it is impossible to flush latched pages). */
inline void log_free_check();

/** Calculates lsn value for given sn value. Sequence of sn values
enumerate all data bytes in the redo log. Sequence of lsn values
enumerate all data bytes and bytes used for headers and footers
of all log blocks in the redo log. For every LOG_BLOCK_DATA_SIZE
bytes of data we have OS_FILE_LOG_BLOCK_SIZE bytes in the redo log.
NOTE that LOG_BLOCK_DATA_SIZE + LOG_BLOCK_HDR_SIZE + LOG_BLOCK_TRL_SIZE
== OS_FILE_LOG_BLOCK_SIZE. The calculated lsn value will always point
to some data byte (will be % OS_FILE_LOG_BLOCK_SIZE >= LOG_BLOCK_HDR_SIZE,
and < OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE).

@param[in]	sn	sn value
@return lsn value for the provided sn value */
constexpr inline lsn_t log_translate_sn_to_lsn(lsn_t sn);

/** Calculates sn value for given lsn value.
@see log_translate_sn_to_lsn
@param[in]	lsn	lsn value
@return sn value for the provided lsn value */
inline lsn_t log_translate_lsn_to_sn(lsn_t lsn);

#endif /* !UNIV_HOTBACKUP */

/** Validates a given lsn value. Checks if the lsn value points to data
bytes inside log block (not to some bytes in header/footer). It is used
by assertions.
@return true if lsn points to data bytes within log block */
inline bool log_lsn_validate(lsn_t lsn);

#ifndef UNIV_HOTBACKUP

/** Calculates age of current checkpoint as number of bytes since
last checkpoint. This includes bytes for headers and footers of
all log blocks. The calculation is based on the latest written
checkpoint lsn, and the current lsn, which points to the first
non reserved data byte. Note that the current lsn could not fit
the free space in the log files. This means that the checkpoint
age could potentially be larger than capacity of the log files.
However we do the best effort to avoid such situations, and if
they happen, user threads wait until the space is reclaimed.
@param[in]	log	redo log
@return checkpoint age as number of bytes */
inline lsn_t log_get_checkpoint_age(const log_t &log);

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

@see @ref sect_redo_log_buf_reserve
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

@see @ref sect_redo_log_buf_write
@param[in,out]	log		redo log
@param[in]	handle		handle for the reservation of space
@param[in]	str		memory to write data from
@param[in]	str_len		number of bytes to write
@param[in]	start_lsn	lsn to start writing at (the reserved space)

@return end_lsn after writing the data (in the reserved space), could be
used to start the next write operation if there is still free space in
the reserved space */
lsn_t log_buffer_write(log_t &log, const Log_handle &handle, const byte *str,
                       size_t str_len, lsn_t start_lsn);

/** Adds a link start_lsn -> end_lsn to the log recent written buffer.

This function must be called after the data has been written to the
fragment of log buffer represented by range [start_lsn, end_lsn).
After the link is added, the log writer may write the data to disk.

NOTE that still dirty pages for the [start_lsn, end_lsn) are not added
to flush lists when this function is called.

@see @ref sect_redo_log_buf_add_links_to_recent_written
@param[in,out]	log		redo log
@param[in]	handle		handle for the reservation of space
@param[in]	start_lsn	start_lsn of the link to add
@param[in]	end_lsn		end_lsn of the link to add */
void log_buffer_write_completed(log_t &log, const Log_handle &handle,
                                lsn_t start_lsn, lsn_t end_lsn);

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
@param[in]	handle			handle for the reservation of space
@param[in]	rec_group_end_lsn	lsn at which the first log record
group starts within the block containing this lsn value */
void log_buffer_set_first_record_group(log_t &log, const Log_handle &handle,
                                       lsn_t rec_group_end_lsn);

/** Waits until there is free space in the log recent closed buffer
for a given link start_lsn -> end_lsn. It does not add the link.

This is called just before dirty pages for [start_lsn, end_lsn)
are added to flush lists. That's because we need to guarantee,
that the delay until dirty page is added to flush list is limited.
For detailed explanation - @see log0write.cc.

@see @ref sect_redo_log_add_dirty_pages
@param[in,out]	log		redo log
@param[in]	handle		handle for the reservation of space */
void log_buffer_write_completed_before_dirty_pages_added(
    log_t &log, const Log_handle &handle);

/** Adds a link start_lsn -> end_lsn to the log recent closed buffer.

This is called after all dirty pages related to [start_lsn, end_lsn)
have been added to corresponding flush lists.
For detailed explanation - @see log0write.cc.

@see @ref sect_redo_log_add_link_to_recent_closed
@param[in,out]	log		redo log
@param[in]	handle		handle for the reservation of space */
void log_buffer_write_completed_and_dirty_pages_added(log_t &log,
                                                      const Log_handle &handle);

/** Write to the log file up to the last log entry.
@param[in,out]	log	redo log
@param[in]	sync	whether we want the written log
also to be flushed to disk. */
void log_buffer_flush_to_disk(log_t &log, bool sync = true);

/** Requests flush of the log buffer.
@param[in]	sync	true: wait until the flush is done */
inline void log_buffer_flush_to_disk(bool sync = true);

/** @return lsn up to which all writes to log buffer have been finished */
inline lsn_t log_buffer_ready_for_write_lsn(const log_t &log);

/** @return lsn up to which all dirty pages have been added to flush list */
inline lsn_t log_buffer_dirty_pages_added_up_to_lsn(const log_t &log);

/** @return capacity of the recent_closed, or 0 if !log_use_threads() */
inline lsn_t log_buffer_flush_order_lag(const log_t &log);

/** Get last redo block from redo buffer and end LSN. Note that it takes
x-lock on the log buffer for a short period. Out values are always set,
even when provided last_block is nullptr.
@param[in,out]	log		redo log
@param[out]	last_lsn	end lsn of last mtr
@param[out]	last_block	last redo block
@param[in,out]	block_len	length in bytes */
void log_buffer_get_last_block(log_t &log, lsn_t &last_lsn, byte *last_block,
                               uint32_t &block_len);

/** Advances log.buf_ready_for_write_lsn using links in the recent written
buffer. It's used by the log writer thread only.
@param[in]	log	redo log
@return true if and only if the lsn has been advanced */
bool log_advance_ready_for_write_lsn(log_t &log);

/** Advances log.buf_dirty_pages_added_up_to_lsn using links in the recent
closed buffer. It's used by the log closer thread only.
@param[in]	log	redo log
@return true if and only if the lsn has been advanced */
bool log_advance_dirty_pages_added_up_to_lsn(log_t &log);

/** Validates that all slots in log recent written buffer for lsn values
in range between begin and end, are empty. Used during tests, crashes the
program if validation does not pass.
@param[in]	log		redo log which buffer is validated
@param[in]	begin		validation start (inclusive)
@param[in]	end		validation end (exclusive) */
void log_recent_written_empty_validate(const log_t &log, lsn_t begin,
                                       lsn_t end);

/** Validates that all slots in log recent closed buffer for lsn values
in range between begin and end, are empty. Used during tests, crashes the
program if validation does not pass.
@param[in]	log		redo log which buffer is validated
@param[in]	begin		validation start (inclusive)
@param[in]	end		validation end (exclusive) */
void log_recent_closed_empty_validate(const log_t &log, lsn_t begin, lsn_t end);

/** Declaration of remaining functions. */

/** Waits until there is free space in the log buffer. The free space has to be
available for range of sn values ending at the provided sn.
@see @ref sect_redo_log_waiting_for_writer
@param[in]     log     redo log
@param[in]     end_sn  inclusive end of the range of sn values */
void log_wait_for_space_in_log_buf(log_t &log, sn_t end_sn);

/** Waits until there is free space in the log files. The free space has to be
available for range of sn values ending at the provided sn.
@see @ref sect_redo_log_reclaim_space
@param[in]	log	redo log
@param[in]	end_sn	inclusive end of the range of sn values */
void log_wait_for_space_in_log_file(log_t &log, sn_t end_sn);

/** Waits until there is free space for range of sn values ending
at the provided sn, in both the log buffer and in the log files.
@param[in]	log	redo log
@param[in]	end_sn	inclusive end of the range of sn values */
void log_wait_for_space(log_t &log, sn_t end_sn);

/** Updates sn limit values up to which user threads may consider the
reserved space as available both in the log buffer and in the log files.
Both limits - for the start and for the end of reservation, are updated.
Limit for the end is the only one, which truly guarantees that there is
space for the whole reservation. Limit for the start is used to check
free space when being outside mtr (without latches), in which case it
is unknown how much we will need to reserve and write, so current sn
is then compared to the limit. This is called whenever these limits
may change - when write_lsn or last_checkpoint_lsn are advanced,
when the log buffer is resized or margins are changed (e.g. because
of changed concurrency limit).
@param[in,out]	log	redo log */
void log_update_limits(log_t &log);

/** Waits until the redo log is written up to a provided lsn.
@param[in]	log		redo log
@param[in]	lsn		lsn to wait for
@param[in]	flush_to_disk	true: wait until it is flushed
@return statistics about waiting inside */
Wait_stats log_write_up_to(log_t &log, lsn_t lsn, bool flush_to_disk);

/* Read the first log file header to get the encryption
information if it exist.
@return true if success */
bool log_read_encryption();

/** Write the encryption info into the log file header(the 3rd block).
It just need to flush the file header block with current master key.
@param[in]	key	encryption key
@param[in]	iv	encryption iv
@param[in]	is_boot	if it is for bootstrap
@return true if success. */
bool log_write_encryption(byte *key, byte *iv, bool is_boot);

/** Rotate the redo log encryption
It will re-encrypt the redo log encryption metadata and write it to
redo log file header.
@return true if success. */
bool log_rotate_encryption();

/** Try to enable the redo log encryption if it's set.
It will try to enable the redo log encryption and write the metadata to
redo log file header if the innodb_undo_log_encrypt is ON. */
void log_enable_encryption_if_set();

/** Requests a new checkpoint write for lsn which is currently available
for checkpointing (the lsn is updated in log checkpointer thread).
@param[in,out]	log	redo log
@param[in]	sync	true -> wait until the write is finished */
void log_request_checkpoint(log_t &log, bool sync);

/** Make a checkpoint at the current lsn. Reads current lsn and waits
until all dirty pages have been flushed up to that lsn. Afterwards
requests a checkpoint write and waits until it is finished.
@param[in,out]	log	redo log
@return true iff current lsn was greater than last checkpoint lsn */
bool log_make_latest_checkpoint(log_t &log);

/** Make a checkpoint at the current lsn. Reads current lsn and waits
until all dirty pages have been flushed up to that lsn. Afterwards
requests a checkpoint write and waits until it is finished.
@return true iff current lsn was greater than last checkpoint lsn */
bool log_make_latest_checkpoint();

/** Reads a log file header page to log.checkpoint_buf.
@param[in,out]	log	redo log
@param[in]	header	0 or LOG_CHECKPOINT_1 or LOG_CHECKPOINT2 */
void log_files_header_read(log_t &log, uint32_t header);

/** Fill redo log header.
@param[out]	buf		filled buffer
@param[in]	start_lsn	log start LSN
@param[in]	creator		creator of the header */
void log_files_header_fill(byte *buf, lsn_t start_lsn, const char *creator);

/** Writes a log file header to the log file space.
@param[in]	log		redo log
@param[in]	nth_file	header for the nth file in the log files
@param[in]	start_lsn	log file data starts at this lsn */
void log_files_header_flush(log_t &log, uint32_t nth_file, lsn_t start_lsn);

/** Changes format of redo files to previous format version.

@note Note this will work between the two formats 5_7_9 & current because
the only change is the version number */
void log_files_downgrade(log_t &log);

/** Writes the next checkpoint info to header of the first log file.
Note that two pages of the header are used alternately for consecutive
checkpoints. If we crashed during the write, we would still have the
previous checkpoint info and recovery would work.
@param[in,out]	log			redo log
@param[in]	next_checkpoint_lsn	writes checkpoint at this lsn */
void log_files_write_checkpoint(log_t &log, lsn_t next_checkpoint_lsn);

/** Updates current_file_lsn and current_file_real_offset to correspond
to a given lsn. For this function to work, the values must already be
initialized to correspond to some lsn, for instance, a checkpoint lsn.
@param[in,out]	log	redo log
@param[in]	lsn	log sequence number to set files_start_lsn at */
void log_files_update_offsets(log_t &log, lsn_t lsn);

/** Acquires the log buffer s-lock.
@param[in,out]	log	redo log
@return lock no, must be passed to s_lock_exit() */
size_t log_buffer_s_lock_enter(log_t &log);

/** Releases the log buffer s-lock.
@param[in,out]	log	redo log
@param[in]	lock_no	lock no received from s_lock_enter() */
void log_buffer_s_lock_exit(log_t &log, size_t lock_no);

/** Acquires the log buffer x-lock.
@param[in,out]	log	redo log */
void log_buffer_x_lock_enter(log_t &log);

/** Releases the log buffer x-lock.
@param[in,out]	log	redo log */
void log_buffer_x_lock_exit(log_t &log);
#endif /* !UNIV_HOTBACKUP */

/** Calculates offset within log files, excluding headers of log files.
@param[in]	log		redo log
@param[in]	offset		real offset (including log file headers)
@return	size offset excluding log file headers (<= offset) */
uint64_t log_files_size_offset(const log_t &log, uint64_t offset);

/** Calculates offset within log files, including headers of log files.
@param[in]	log		redo log
@param[in]	offset		size offset (excluding log file headers)
@return real offset including log file headers (>= offset) */
uint64_t log_files_real_offset(const log_t &log, uint64_t offset);

/** Calculates offset within log files, including headers of log files,
for the provided lsn value.
@param[in]	log	redo log
@param[in]	lsn	log sequence number
@return real offset within the log files */
uint64_t log_files_real_offset_for_lsn(const log_t &log, lsn_t lsn);
#ifndef UNIV_HOTBACKUP

/** Changes size of the log buffer. This is a thread-safe version.
It is used by SET GLOBAL innodb_log_buffer_size = X.
@param[in,out]	log		redo log
@param[in]	new_size	requested new size
@return true iff succeeded in resize */
bool log_buffer_resize(log_t &log, size_t new_size);

/** Changes size of the log buffer. This is a non-thread-safe version
which might be invoked only when there are no concurrent possible writes
to the log buffer. It is used in log_buffer_reserve() when a requested
size to reserve is larger than size of the log buffer.
@param[in,out]	log		redo log
@param[in]	new_size	requested new size
@param[in]	end_lsn		maximum lsn written to log buffer
@return true iff succeeded in resize */
bool log_buffer_resize_low(log_t &log, size_t new_size, lsn_t end_lsn);

/** Resizes the write ahead buffer in the redo log.
@param[in,out]	log		redo log
@param[in]	new_size	new size (in bytes) */
void log_write_ahead_resize(log_t &log, size_t new_size);

/** Calculates required size of margin in the log files, based on
thread concurrency limitations. Constant extra safety margin, not
related to concurrency, is also added.
@param[in]	log			redo log
@param[in]	thread_concurrency	thread concurrency
@return the required size of margin */
uint64_t log_calc_safe_concurrency_margin(const log_t &log,
                                          int thread_concurrency);

/** Calculates required size of margin in the log files, based on
thread concurrency limitations. Constant extra safety margin, not
related to concurrency, is also added. The calculated margin is
truncated to at most half of the available space in log files.
@param[in]	log			redo log
@param[in]	thread_concurrency	thread concurrency
@param[out]	concurrency_margin	calculated and truncated margin
@retval true	margin was NOT truncated (there was space in log files)
@retval false	margin was truncated (log files had not enough space) */
bool log_calc_concurrency_margin(const log_t &log, int thread_concurrency,
                                 uint64_t &concurrency_margin);

/** Prints information about important lsn values used in the redo log,
and some statistics about speed of writing and flushing of data.
@param[in]	log	redo log for which print information
@param[out]	file	file where to print */
void log_print(const log_t &log, FILE *file);

/** Refreshes the statistics used to print per-second averages in log_print().
@param[in,out]	log	redo log */
void log_refresh_stats(log_t &log);

/** Creates the first checkpoint ever in the log files. Used during
initialization of new log files. Flushes:
  - header of the first log file (including checkpoint headers),
  - log block with data addressed by the checkpoint lsn.
@param[in,out]	log	redo log
@param[in]	lsn	the first checkpoint lsn */
void log_create_first_checkpoint(log_t &log, lsn_t lsn);

/** Calculates limits for maximum age of checkpoint and maximum age of
the oldest page. Uses current value of srv_thread_concurrency.
@param[in,out]	log	redo log
@retval true if success
@retval false if the redo log is too small to accommodate the number of
OS threads in the database server */
bool log_calc_max_ages(log_t &log);

/** Initializes the log system. Note that the log system is not ready
for user writes after this call is finished. It should be followed by
a call to log_start. Also, log background threads need to be started
manually using log_start_background_threads afterwards.

Hence the proper order of calls looks like this:
        - log_sys_init(),
        - log_start(),
        - log_start_background_threads().

@param[in]	n_files		number of log files
@param[in]	file_size	size of each log file in bytes
@param[in]	space_id	space id of the file space with log files */
bool log_sys_init(uint32_t n_files, uint64_t file_size, space_id_t space_id);

/** Starts the initialized redo log system using a provided
checkpoint_lsn and current lsn.
@param[in,out]	log		redo log
@param[in]	checkpoint_no	checkpoint no (sequential number)
@param[in]	checkpoint_lsn	checkpoint lsn
@param[in]	start_lsn	current lsn to start at */
void log_start(log_t &log, checkpoint_no_t checkpoint_no, lsn_t checkpoint_lsn,
               lsn_t start_lsn);

/** Validates that the log writer thread is active.
Used only to assert, that the state is correct.
@param[in]	log	redo log */
void log_writer_thread_active_validate(const log_t &log);

/** Validates that the log closer thread is active.
Used only to assert, that the state is correct.
@param[in]	log	redo log */
void log_closer_thread_active_validate(const log_t &log);

/** Validates that the log writer, flusher threads are active.
Used only to assert, that the state is correct.
@param[in]	log	redo log */
void log_background_write_threads_active_validate(const log_t &log);

/** Validates that all the log background threads are active.
Used only to assert, that the state is correct.
@param[in]	log	redo log */
void log_background_threads_active_validate(const log_t &log);

/** Validates that all the log background threads are inactive.
Used only to assert, that the state is correct.
@param[in]	log	redo log */
void log_background_threads_inactive_validate(const log_t &log);

/** Starts all the log background threads. This can be called only,
when the threads are inactive. This should never be called concurrently.
This may not be called during read-only mode.
@param[in,out]	log	redo log */
void log_start_background_threads(log_t &log);

/** Stops all the log background threads. This can be called only,
when the threads are active. This should never be called concurrently.
This may not be called in read-only mode. Note that is is impossible
to start log background threads in such case.
@param[in,out]	log	redo log */
void log_stop_background_threads(log_t &log);

/** @return true iff log threads are started */
bool log_threads_active(const log_t &log);

/** Free the log system data structures. Deallocate all the related memory. */
void log_sys_close();

/** The log writer thread co-routine.
@see @ref sect_redo_log_writer
@param[in,out]	log_ptr		pointer to redo log */
void log_writer(log_t *log_ptr);

/** The log flusher thread co-routine.
@see @ref sect_redo_log_flusher
@param[in,out]	log_ptr		pointer to redo log */
void log_flusher(log_t *log_ptr);

/** The log flush notifier thread co-routine.
@see @ref sect_redo_log_flush_notifier
@param[in,out]	log_ptr		pointer to redo log */
void log_flush_notifier(log_t *log_ptr);

/** The log write notifier thread co-routine.
@see @ref sect_redo_log_write_notifier
@param[in,out]	log_ptr		pointer to redo log */
void log_write_notifier(log_t *log_ptr);

/** The log closer thread co-routine.
@see @ref sect_redo_log_closer
@param[in,out]	log_ptr		pointer to redo log */
void log_closer(log_t *log_ptr);

/** The log checkpointer thread co-routine.
@see @ref sect_redo_log_checkpointer
@param[in,out]	log_ptr		pointer to redo log */
void log_checkpointer(log_t *log_ptr);

#define log_buffer_x_lock_own(log) log.sn_lock.x_own()

#define log_checkpointer_mutex_enter(log) \
  mutex_enter(&((log).checkpointer_mutex))

#define log_checkpointer_mutex_exit(log) mutex_exit(&((log).checkpointer_mutex))

#define log_checkpointer_mutex_own(log)      \
  (mutex_own(&((log).checkpointer_mutex)) || \
   !(log).checkpointer_thread_alive.load())

#define log_closer_mutex_enter(log) mutex_enter(&((log).closer_mutex))

#define log_closer_mutex_exit(log) mutex_exit(&((log).closer_mutex))

#define log_closer_mutex_own(log) \
  (mutex_own(&((log).closer_mutex)) || !(log).closer_thread_alive.load())

#define log_flusher_mutex_enter(log) mutex_enter(&((log).flusher_mutex))

#define log_flusher_mutex_enter_nowait(log) \
  mutex_enter_nowait(&((log).flusher_mutex))

#define log_flusher_mutex_exit(log) mutex_exit(&((log).flusher_mutex))

#define log_flusher_mutex_own(log) \
  (mutex_own(&((log).flusher_mutex)) || !(log).flusher_thread_alive.load())

#define log_flush_notifier_mutex_enter(log) \
  mutex_enter(&((log).flush_notifier_mutex))

#define log_flush_notifier_mutex_exit(log) \
  mutex_exit(&((log).flush_notifier_mutex))

#define log_flush_notifier_mutex_own(log)      \
  (mutex_own(&((log).flush_notifier_mutex)) || \
   !(log).flush_notifier_thread_alive.load())

#define log_writer_mutex_enter(log) mutex_enter(&((log).writer_mutex))

#define log_writer_mutex_enter_nowait(log) \
  mutex_enter_nowait(&((log).writer_mutex))

#define log_writer_mutex_exit(log) mutex_exit(&((log).writer_mutex))

#define log_writer_mutex_own(log) \
  (mutex_own(&((log).writer_mutex)) || !(log).writer_thread_alive.load())

#define log_write_notifier_mutex_enter(log) \
  mutex_enter(&((log).write_notifier_mutex))

#define log_write_notifier_mutex_exit(log) \
  mutex_exit(&((log).write_notifier_mutex))

#define log_write_notifier_mutex_own(log)      \
  (mutex_own(&((log).write_notifier_mutex)) || \
   !(log).write_notifier_thread_alive.load())

#define LOG_SYNC_POINT(a)                \
  do {                                   \
    DEBUG_SYNC_C(a);                     \
    DBUG_EXECUTE_IF(a, DBUG_SUICIDE();); \
    if (log_test != nullptr) {           \
      log_test->sync_point(a);           \
    }                                    \
  } while (0)

/** Lock redo log. Both current lsn and checkpoint lsn will not change
until the redo log is unlocked.
@param[in,out]	log	redo log to lock */
void log_position_lock(log_t &log);

/** Unlock the locked redo log.
@param[in,out]	log	redo log to unlock */
void log_position_unlock(log_t &log);

/** Collect coordinates in the locked redo log.
@param[in]	log		locked redo log
@param[out]	current_lsn	stores current lsn there
@param[out]	checkpoint_lsn	stores checkpoint lsn there */
void log_position_collect_lsn_info(const log_t &log, lsn_t *current_lsn,
                                   lsn_t *checkpoint_lsn);

#else /* !UNIV_HOTBACKUP */

#ifdef UNIV_DEBUG

/** Print a log file header.
@param[in]	block	pointer to the log buffer */
void meb_log_print_file_hdr(byte *block);

#endif /* UNIV_DEBUG */

#endif /* !UNIV_HOTBACKUP */

#include "log0log.ic"

#endif /* !log0log_h */
