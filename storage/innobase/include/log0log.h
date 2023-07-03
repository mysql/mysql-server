/*****************************************************************************

<<<<<<< HEAD
Copyright (c) 1995, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
Copyright (c) 1995, 2018, Oracle and/or its affiliates. All rights reserved.
=======
Copyright (c) 1995, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
Copyright (c) 2009, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.
>>>>>>> pr/231

<<<<<<< HEAD
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
=======
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
>>>>>>> upstream/cluster-7.6

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

<<<<<<< HEAD
/** @{ */
=======
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

<<<<<<< HEAD
/* Offsets used in a log block header. */
=======
static const char ib_logfile_basename[] = "ib_logfile";

/*******************************************************************//**
Calculates where in log files we find a specified lsn.
@return log file number */
ulint
log_calc_where_lsn_is(
/*==================*/
	int64_t*	log_file_offset,	/*!< out: offset in that file
						(including the header) */
	ib_uint64_t	first_header_lsn,	/*!< in: first log file start
						lsn */
	ib_uint64_t	lsn,			/*!< in: lsn whose position to
						determine */
	ulint		n_log_files,		/*!< in: total number of log
						files */
	int64_t		log_file_size);		/*!< in: log file size
						(including the header) */
#ifndef UNIV_HOTBACKUP
/** Append a string to the log.
@param[in]	str		string
@param[in]	len		string length
@param[out]	start_lsn	start LSN of the log record
@return end lsn of the log record, zero if did not succeed */
UNIV_INLINE
lsn_t
log_reserve_and_write_fast(
	const void*	str,
	ulint		len,
	lsn_t*		start_lsn);
/***********************************************************************//**
Checks if there is need for a log buffer flush or a new checkpoint, and does
this if yes. Any database operation should call this when it has modified
more than about 4 pages. NOTE that this function may only be called when the
OS thread owns no synchronization objects except the dictionary mutex. */
UNIV_INLINE
void
log_free_check(void);
/*================*/
>>>>>>> upstream/cluster-7.6

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
>>>>>>> pr/231

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
@param[in]      first_block        data block (with start_lsn)
                                   to copy into the log buffer;
                                   nullptr if no reason to copy
@param[in]      allow_checkpoints  true iff allows writing newer checkpoints
@return DB_SUCCESS or error */
dberr_t log_start(log_t &log, lsn_t checkpoint_lsn, lsn_t start_lsn,
                  byte first_block[OS_FILE_LOG_BLOCK_SIZE],
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
