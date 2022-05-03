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
 @file include/log0constants.h

 Redo log constant values. This file should not be included
 except the include in log0types.h.

 Include log0types.h if you needed to use constants.

 *******************************************************/

#ifndef log0constants_h
#define log0constants_h

/* lsn_t, sn_t, Log_file_id */
#include "log0types.h"

/* os_offset_t, OS_FILE_LOG_BLOCK_SIZE */
#include "os0file.h"

/** Align the log buffer (log_t::buf) to this size. This is to preserve the
compatibility with older MySQL versions which also aligned the log buffer
to OS_FILE_LOG_BLOCK_SIZE. Note, that each write from the log buffer starts
at the beginning of one of the blocks in this buffer i.e. from address being
a multiple of OS_FILE_LOG_BLOCK_SIZE. Therefore any bigger value for alignment
here does not make sense. Please do not use this constant for other buffers. */
constexpr size_t LOG_BUFFER_ALIGNMENT = OS_FILE_LOG_BLOCK_SIZE;

/** Align the log write-ahead buffer (log_t::write_ahead_buf) to this size.
This increases chance that the write-ahead buffer is spanned over smaller
number of memory pages. Please do not use this constant for other buffers. */
constexpr size_t LOG_WRITE_AHEAD_BUFFER_ALIGNMENT =
    ut::INNODB_KERNEL_PAGE_SIZE_DEFAULT;

/**************************************************/ /**

 @name Log constants related to the log file i-nodes.

 *******************************************************/

/** @{ */

/** Name of subdirectory which contains redo log files. */
constexpr const char *const LOG_DIRECTORY_NAME = "#innodb_redo";

/** Prefix of log file name in the current redo format. */
constexpr const char *const LOG_FILE_BASE_NAME = "#ib_redo";

/** Maximum length of log file name, computed as: base name length(8)
+ length for decimal digits(22). */
constexpr uint32_t LOG_FILE_NAME_MAX_LENGTH = 8 + 22;

/** Targeted number of log files. */
constexpr size_t LOG_N_FILES = 32;

/** Determines maximum downsize for maximum redo file size during resize.
If maximum file size is 8G, then 1.0/8 means, that InnoDB needs to first
achieve maximum file size equal to 1G before targeting even lower values. */
constexpr double LOG_N_FILES_MAX_DOWNSIZE_RATIO = 1.0 / 8;

/** Minimum size of single log file, expressed in bytes. */
constexpr os_offset_t LOG_FILE_MIN_SIZE = 64 * 1024;

/** Maximum size of single log file, expressed in bytes. */
constexpr os_offset_t LOG_FILE_MAX_SIZE = 4ULL * 1024 * 1024 * 1024; /* 4G */

/** Minimum allowed value for innodb_redo_log_capacity. */
constexpr os_offset_t LOG_CAPACITY_MIN = 8 * 1024 * 1024; /* 8M */

/** Maximum allowed value for innodb_redo_log_capacity. */
constexpr os_offset_t LOG_CAPACITY_MAX = LOG_N_FILES * LOG_FILE_MAX_SIZE;

/** Id of the first redo log file (assigned to the first log file
when new data directory is being initialized). */
constexpr Log_file_id LOG_FIRST_FILE_ID = 0;

/** Maximum number of handles for opened redo log files (in parallel).
The following handles for opened files have been identified during runtime:
    - protected by the log_writer_mutex and the log_flusher_mutex:
        - log_writer() and log_flusher() use log.m_current_file_handle
          and this handle represents one file and can only be switched
          to the next file if both mutexes are acquired,
        - if redo log file is being rewritten, the read_handle for the old
          file acts on behalf of m_current_file_handle which is closed before
          the read_handle is opened.
    - protected by the log_files_mutex:
        - log_files_next_checkpoint() uses handle on stack,
        - log_files_prepare_unused_file() uses handle on stack,
        - log_encryption_write_low() uses handle on stack,
        - if redo log file is being rewritten, the write_handle for the new
          file uses this slot (protected by the files_mutex); it is opened
          after log_files_prepare_unused_file() closed its handle.
During startup - in main thread (recv functions):
    - log_files_find_and_analyze() uses handle on stack,
    - recv_log_recover_pre_8_0_30() uses handle on stack,
    - recv_find_max_checkpoint() uses handle on stack,
    - recv_read_log_seg() uses handle on stack,
    - recv_recovery_from_checkpoint_start() uses handle on stack but
      after the recv_find_max_checkpoint() is finished and before
      the recv_read_log_seg() is started.
Redo threads are started after the recv_recovery_from_checkpoint_start() is
finished, so they don't use handle in parallel with these recv functions. */
constexpr size_t LOG_MAX_OPEN_FILES = 2;

/** @} */

/**************************************************/ /**

 @name Log constants related to the log file format.

 *******************************************************/

/** @{ */

/* General constants describing the log file format. */

/** Magic value to use instead of log checksums when they are disabled. */
constexpr uint32_t LOG_NO_CHECKSUM_MAGIC = 0xDEADBEEFUL;

/** The counting of lsn's starts from this value: this must be non-zero. */
constexpr lsn_t LOG_START_LSN = 16 * OS_FILE_LOG_BLOCK_SIZE;

/** Maximum possible lsn value is slightly higher than the maximum sn value,
because lsn sequence enumerates also bytes used for headers and footers of
all log blocks. However, still 64-bits are enough to represent the maximum
lsn value, because only 63 bits are used to represent sn value. */
constexpr lsn_t LSN_MAX = (1ULL << 63) - 1;

/** The sn bit to express locked state. */
constexpr sn_t SN_LOCKED = 1ULL << 63;

/** First checkpoint field in the log header. We write alternately to
the checkpoint fields when we make new checkpoints. This field is only
defined in the first log file. */
constexpr os_offset_t LOG_CHECKPOINT_1 = OS_FILE_LOG_BLOCK_SIZE;

/** Log Encryption information in redo log header. */
constexpr os_offset_t LOG_ENCRYPTION = 2 * OS_FILE_LOG_BLOCK_SIZE;

/** Second checkpoint field in the header of the first log file. */
constexpr os_offset_t LOG_CHECKPOINT_2 = 3 * OS_FILE_LOG_BLOCK_SIZE;

/** Size of log file's header. */
constexpr os_offset_t LOG_FILE_HDR_SIZE = 4 * OS_FILE_LOG_BLOCK_SIZE;

/* Offsets used in a log file header */

/** Log file header format identifier (32-bit unsigned big-endian integer).
This used to be called LOG_GROUP_ID and always written as 0,
because InnoDB never supported more than one copy of the redo log. */
constexpr os_offset_t LOG_HEADER_FORMAT = 0;

/** Offset within the log file header, to the field which stores the log_uuid.
The log_uuid is chosen after a new data directory is initialized, and allows
to detect situation, in which some of log files came from other data directory
(detection is performed on startup, before starting recovery). */
constexpr uint32_t LOG_HEADER_LOG_UUID = 4;

/** LSN of the start of data in this log file (with format version 1 and 2). */
constexpr os_offset_t LOG_HEADER_START_LSN = 8;

/** A null-terminated string which will contain either the string 'MEB'
and the MySQL version if the log file was created by mysqlbackup,
or 'MySQL' and the MySQL version that created the redo log file. */
constexpr os_offset_t LOG_HEADER_CREATOR = 16;

/** Maximum length of string with creator name (excludes \0). */
constexpr size_t LOG_HEADER_CREATOR_MAX_LENGTH = 31;

/** End of the log file creator field (we add 1 for \0). */
constexpr os_offset_t LOG_HEADER_CREATOR_END =
    LOG_HEADER_CREATOR + LOG_HEADER_CREATOR_MAX_LENGTH + 1;

/** Offset to encryption information in the log encryption header. */
constexpr os_offset_t LOG_HEADER_ENCRYPTION_INFO_OFFSET =
    LOG_HEADER_CREATOR_END;

/** Contents of the LOG_HEADER_CREATOR field */
#define LOG_HEADER_CREATOR_CURRENT "MySQL " INNODB_VERSION_STR

/** Header is created during DB clone */
#define LOG_HEADER_CREATOR_CLONE "MySQL Clone"

/** 32 BITs flag */
constexpr os_offset_t LOG_HEADER_FLAGS = LOG_HEADER_CREATOR_END;

/** Flag at BIT-1 to indicate if redo logging is disabled or not. */
constexpr uint32_t LOG_HEADER_FLAG_NO_LOGGING = 1;

/** Flag at BIT-2 to indicate if server is not recoverable on crash. This
is set only when redo logging is disabled and unset on slow shutdown after
all pages are flushed to disk. */
constexpr uint32_t LOG_HEADER_FLAG_CRASH_UNSAFE = 2;

/** Flag at BIT-3 to indicate if server is not recoverable on crash because
data directory still has not been fully initialized. */
constexpr uint32_t LOG_HEADER_FLAG_NOT_INITIALIZED = 3;

/** Flag at BIT-4 to mark the redo log file as completely full and closed
for any future writes. */
constexpr uint32_t LOG_HEADER_FLAG_FILE_FULL = 4;

/** Maximum BIT position number. Should be set to the latest added. */
constexpr uint32_t LOG_HEADER_FLAG_MAX = LOG_HEADER_FLAG_FILE_FULL;

/** Current total size of LOG header. */
constexpr os_offset_t LOG_HEADER_SIZE = LOG_HEADER_FLAGS + 4;

/* Offsets inside the checkpoint pages since 8.0.30 redo format. */

/** Checkpoint lsn. Recovery starts from this lsn and searches for the first
log record group that starts since then. */
constexpr os_offset_t LOG_CHECKPOINT_LSN = 8;

/* Offsets used in a log block header. */

/** Offset to hdr_no, which is a log block number and must be > 0.
It is allowed to wrap around at LOG_BLOCK_MAX_NO.
In older versions of MySQL the highest bit (LOG_BLOCK_FLUSH_BIT_MASK) of hdr_no
is set to 1, if this is the first block in a call to write. */
constexpr uint32_t LOG_BLOCK_HDR_NO = 0;

/** Mask used to get the highest bit in the hdr_no field.
In the older MySQL versions this bit was used to mark first block in a write.*/
constexpr uint32_t LOG_BLOCK_FLUSH_BIT_MASK = 0x80000000UL;

/** Maximum allowed block's number (stored in hdr_no) increased by 1. */
constexpr uint32_t LOG_BLOCK_MAX_NO = 0x3FFFFFFFUL + 1;

/** Offset to number of bytes written to this block (also header bytes). */
constexpr uint32_t LOG_BLOCK_HDR_DATA_LEN = 4;

/** Mask used to get the highest bit in the data len field,
this bit is to indicate if this block is encrypted or not. */
constexpr uint32_t LOG_BLOCK_ENCRYPT_BIT_MASK = 0x8000UL;

/** Offset to "first_rec_group offset" stored in the log block header.

The first_rec_group offset is an offset of the first start of mtr log
record group in this log block (0 if no mtr starts in that log block).

If the value is the same as LOG_BLOCK_HDR_DATA_LEN, it means that the
first rec group has not yet been concatenated to this log block, but if
it was supposed to be appended, it would start at this offset.

An archive recovery can start parsing the log records starting from this
offset in this log block, if value is not 0. */
constexpr uint32_t LOG_BLOCK_FIRST_REC_GROUP = 6;

/** Offset to epoch_no stored in this log block. The epoch_no is computed
as the number of epochs passed by the value of start_lsn of the log block.
Single epoch is defined as range of lsn values containing LOG_BLOCK_MAX_NO
log blocks, each of OS_FILE_LOG_BLOCK_SIZE bytes. Note, that hdr_no stored
in header of log block at offset=LOG_BLOCK_HDR_NO, can address the block
within a given epoch, whereas epoch_no stored at offset=LOG_BLOCK_EPOCH_NO
is the number of full epochs that were before. The pair <epoch_no, hdr_no>
would be the absolute block number, so the epoch_no helps in discovery of
unexpected end of the log during recovery in similar way as hdr_no does.
@remarks The epoch_no for block that starts at start_lsn is computed as
the start_lsn divided by OS_FILE_LOG_BLOCK_SIZE, and then divided by the
LOG_BLOCK_MAX_NO. */
constexpr uint32_t LOG_BLOCK_EPOCH_NO = 8;

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

/** @} */

/**************************************************/ /**

 @name Log constants related to the log margins.

 *******************************************************/

/** @{ */

/** Extra safety margin in the redo capacity, never ever used ! */
constexpr os_offset_t LOG_EXTRA_SAFETY_MARGIN = 2 * UNIV_PAGE_SIZE_MAX;

/** Margin which is used ahead of log.write_lsn to create unused files earlier
than the log.write_lsn reaches the m_end_lsn of the log.m_current_file. This
margin is expressed in percentage of the next file size. */
constexpr double LOG_NEXT_FILE_EARLIER_MARGIN = 10;

/** Extra margin, reserved in the redo capacity for the log writer thread.
When checkpoint age exceeds its maximum limits and user threads are waiting
in log_free_check() calls, the log writer thread still has "extra margin"
space reserved in the log files (it is his private fragment of the redo log,
not announced to users of the redo log). When that happens, all user threads
are paused at log_free_check.
This mechanism is supposed to help with getting out of possible deadlocks
between mini-transactions holding latched pages and page cleaners trying to
reclaim space in the redo log by flushing the oldest modified pages. It is
supposed to help if the innodb_thread_concurrency is unlimited or we missed
to do some log_free_check() calls. This margin is expressed in percentage of
the total redo capacity available for the log writer thread (hard capacity). */
constexpr double LOG_EXTRA_WRITER_MARGIN_PCT = 5; /* 5% */

/** Extra margin, reserved in the redo capacity for the concurrency margin.
Expressed in percentage of the total redo capacity available for user threads
(soft capacity). Excluded from LOG_CONCCURENCY_MARGIN_MAX_PCT. */
constexpr double LOG_EXTRA_CONC_MARGIN_PCT = 5; /* 5% */

/** The maximum limit for concurrency_margin expressed as percentage of the
redo capacity available for user threads (soft capacity).

@remarks The concurrency margin is computed as the maximum number of concurrent
threads multiplied by some fixed size. Therefore it could happen that it would
be even bigger than the redo capacity. To avoid such problem, we need to limit
the concurrency margin and warn if the limitation is hit. */
constexpr double LOG_CONCCURENCY_MARGIN_MAX_PCT = 50; /* 50% */

/** Maximum number of concurrent background threads, that could be using mini
transactions which are not read-only (producing redo log records). These are
threads, which also call log_free_check() to reserve space in the redo log,
but which are not included in the innodb_thread_concurrency limitation. That's
why this number is added to the innodb_thread_concurrency when computing the
concurrency_margin, which is used in log_free_check() calls. */
constexpr size_t LOG_BACKGROUND_THREADS_USING_RW_MTRS = 10;

/** Per thread margin for the free space in the log, before a new query step
which modifies the database, is started. It's multiplied by maximum number
of threads, that can concurrently enter mini-transactions. Expressed in
number of pages. */
constexpr uint32_t LOG_CHECKPOINT_FREE_PER_THREAD = 4;

/** Number of bytes that might be generated by log_files_governor thread
to fill up the current log file faster. Note that before generating those
bytes, the log_files_governor checks if log_free_check is required:
- no: acts as automatic reservation of space for the records to generate,
- yes: it skips the redo records generation in this round */
constexpr uint32_t LOG_FILES_DUMMY_INTAKE_SIZE = 4 * 1024;

/** Controls when the aggressive checkpointing should be started,
with regards to the free space in the redo log.
Should be bigger than LOG_FORCING_ADAPTIVE_FLUSH_RATIO_MAX. */
constexpr uint32_t LOG_AGGRESSIVE_CHECKPOINT_RATIO_MIN = 32;

/** Controls when the maximum speed of adaptive flushing of modified pages
is reached (with regards to free space in the redo log). */
constexpr uint32_t LOG_FORCING_ADAPTIVE_FLUSH_RATIO_MAX = 16;

/** Controls when the speed of adaptive flushing of modified pages starts to
increase. Should be less than the LOG_FORCING_ADAPTIVE_FLUSH_RATIO_MAX. */
constexpr uint32_t LOG_FORCING_ADAPTIVE_FLUSH_RATIO_MIN = 8;

/** @} */

/**************************************************/ /**

 @name Log constants related to the system variables.

 *******************************************************/

/** @{ */

/** Default value of innodb_log_write_max_size (in bytes). */
constexpr ulint INNODB_LOG_WRITE_MAX_SIZE_DEFAULT = 4096;

/** Default value of innodb_log_checkpointer_every (in milliseconds). */
constexpr ulong INNODB_LOG_CHECKPOINT_EVERY_DEFAULT = 1000;  // 1000ms = 1s

/** Default value of innodb_log_writer_spin_delay (in spin rounds).
We measured that 1000 spin round takes 4us. We decided to select 1ms
as the maximum time for busy waiting. Therefore it corresponds to 250k
spin rounds. Note that first wait on event takes 50us-100us (even if 10us
is passed), so it is 5%-10% of the total time that we have already spent
on busy waiting, when we fall back to wait on event. */
constexpr ulong INNODB_LOG_WRITER_SPIN_DELAY_DEFAULT = 250000;

/** Default value of innodb_log_writer_timeout (in microseconds).
Note that it will anyway take at least 50us. */
constexpr ulong INNODB_LOG_WRITER_TIMEOUT_DEFAULT = 10;

/** Default value of innodb_log_spin_cpu_abs_lwm.
Expressed in percent (80 stands for 80%) of a single CPU core. */
constexpr ulong INNODB_LOG_SPIN_CPU_ABS_LWM_DEFAULT = 80;

/** Default value of innodb_log_spin_cpu_pct_hwm.
Expressed in percent (50 stands for 50%) of all CPU cores. */
constexpr uint INNODB_LOG_SPIN_CPU_PCT_HWM_DEFAULT = 50;

/** Default value of innodb_log_wait_for_write_spin_delay (in spin rounds).
Read about INNODB_LOG_WRITER_SPIN_DELAY_DEFAULT.
Number of spin rounds is calculated according to current usage of CPU cores.
If the usage is smaller than lwm percents of single core, then max rounds = 0.
If the usage is smaller than 50% of hwm percents of all cores, then max rounds
is decreasing linearly from 10x innodb_log_writer_spin_delay to 1x (for 50%).
Then in range from 50% of hwm to 100% of hwm, the max rounds stays equal to
the innodb_log_writer_spin_delay, because it doesn't make sense to use too
short waits. Hence this is minimum value for the max rounds when non-zero
value is being used. */
constexpr ulong INNODB_LOG_WAIT_FOR_WRITE_SPIN_DELAY_DEFAULT = 25000;

/** Default value of innodb_log_wait_for_write_timeout (in microseconds). */
constexpr ulong INNODB_LOG_WAIT_FOR_WRITE_TIMEOUT_DEFAULT = 1000;

/** Default value of innodb_log_wait_for_flush_spin_delay (in spin rounds).
Read about INNODB_LOG_WAIT_FOR_WRITE_SPIN_DELAY_DEFAULT. The same mechanism
applies here (to compute max rounds). */
constexpr ulong INNODB_LOG_WAIT_FOR_FLUSH_SPIN_DELAY_DEFAULT = 25000;

/** Default value of innodb_log_wait_for_flush_spin_hwm (in microseconds). */
constexpr ulong INNODB_LOG_WAIT_FOR_FLUSH_SPIN_HWM_DEFAULT = 400;

/** Default value of innodb_log_wait_for_flush_timeout (in microseconds). */
constexpr ulong INNODB_LOG_WAIT_FOR_FLUSH_TIMEOUT_DEFAULT = 1000;

/** Default value of innodb_log_flusher_spin_delay (in spin rounds).
Read about INNODB_LOG_WRITER_SPIN_DELAY_DEFAULT. */
constexpr ulong INNODB_LOG_FLUSHER_SPIN_DELAY_DEFAULT = 250000;

/** Default value of innodb_log_flusher_timeout (in microseconds).
Note that it will anyway take at least 50us. */
constexpr ulong INNODB_LOG_FLUSHER_TIMEOUT_DEFAULT = 10;

/** Default value of innodb_log_write_notifier_spin_delay (in spin rounds). */
constexpr ulong INNODB_LOG_WRITE_NOTIFIER_SPIN_DELAY_DEFAULT = 0;

/** Default value of innodb_log_write_notifier_timeout (in microseconds). */
constexpr ulong INNODB_LOG_WRITE_NOTIFIER_TIMEOUT_DEFAULT = 10;

/** Default value of innodb_log_flush_notifier_spin_delay (in spin rounds). */
constexpr ulong INNODB_LOG_FLUSH_NOTIFIER_SPIN_DELAY_DEFAULT = 0;

/** Default value of innodb_log_flush_notifier_timeout (in microseconds). */
constexpr ulong INNODB_LOG_FLUSH_NOTIFIER_TIMEOUT_DEFAULT = 10;

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

/** @} */

/**************************************************/ /**

 @name Log constants used in the tests of the redo log.

 *******************************************************/

/** @{ */

/** Value to which MLOG_TEST records should sum up within a group. */
constexpr int64_t MLOG_TEST_VALUE = 10000;

/** Maximum size of single MLOG_TEST record (in bytes). */
constexpr uint32_t MLOG_TEST_MAX_REC_LEN = 100;

/** Maximum number of MLOG_TEST records in single group of log records. */
constexpr uint32_t MLOG_TEST_GROUP_MAX_REC_N = 100;

/** Bytes occupied by MLOG_TEST record with an empty payload. */
constexpr uint32_t MLOG_TEST_REC_OVERHEAD = 37;

/** @} */

#endif /* !log0constants_h */
