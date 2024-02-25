/*****************************************************************************

Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
 @file include/log0pre_8_0_30.h

 Redo log functions and constants related to redo formats
 before 8.0.30.

 *******************************************************/

#ifndef log0pre_8_0_30_h
#define log0pre_8_0_30_h

/* lsn_t */
#include "log0types.h"

namespace log_pre_8_0_30 {

/** Prefix of log file name in the old redo format (before 8.0.30).
For more details @see Log_files_ruleset */
constexpr const char *const FILE_BASE_NAME = "ib_logfile";

/** Maximum redo log file id in the old format (before 8.0.30). */
constexpr Log_file_id FILE_MAX_ID = 99;

/* Offsets inside the checkpoint pages pre 8.0.30 redo format. */

/** Checkpoint number. It was incremented by one for each next checkpoint.
During recovery, all headers were scanned, and one with the maximum checkpoint
number was used for the recovery (checkpoint_lsn from that header was used). */
constexpr os_offset_t FIELD_CHECKPOINT_NO = 0;

/** Checkpoint lsn. Recovery starts from this lsn and searches for the first
log record group that starts since then. In InnoDB < 8.0.5, it was the exact
value at which the first log record group started. Since 8.0.5, the order in
flush lists became relaxed and because of that checkpoint lsn values were not
precise anymore. */
constexpr os_offset_t FIELD_CHECKPOINT_LSN = 8;

/** Offset within the log files, which corresponds to checkpoint lsn.
Used for calibration of lsn and offset calculations. */
constexpr os_offset_t FIELD_CHECKPOINT_OFFSET = 16;

/** Size of the log buffer, when the checkpoint write was started.
It seems it was write-only field in InnoDB. Not used by recovery.

@note
Note that when the log buffer is being resized, all the log background threads
were stopped, so there was no concurrent checkpoint write (the log_checkpointer
thread was stopped). */
constexpr uint32_t FIELD_CHECKPOINT_LOG_BUF_SIZE = 24;

/** Meta data stored in one of two checkpoint headers. */
struct Checkpoint_header {
  /** Checkpoint number stored in older formats of the redo log. */
  uint64_t m_checkpoint_no;

  /** Checkpoint LSN (oldest_lsn_lwm from the moment of checkpoint). */
  lsn_t m_checkpoint_lsn;

  /** Offset from the beginning of the redo file, which contains the
  checkpoint LSN, to the checkpoint LSN. */
  os_offset_t m_checkpoint_offset;

  /** Size of the log buffer from the moment of checkpoint. */
  uint64_t m_log_buf_size;
};

/** Provides a file offset for the given lsn. For this function to work,
some existing file lsn and corresponding offset to that file lsn have to
be provided.
@param[in]  n_files           number of log files
@param[in]  file_size         size of each log file (in bytes)
@param[in]  some_file_lsn     some file_lsn for which offset is known
@param[in]  some_file_offset  file offset corresponding to the given
                              some_file_lsn
@param[in]  requested_lsn     the given lsn for which offset is computed
@return file offset corresponding to the given requested_lsn */
os_offset_t compute_real_offset_for_lsn(size_t n_files, os_offset_t file_size,
                                        lsn_t some_file_lsn,
                                        os_offset_t some_file_offset,
                                        lsn_t requested_lsn);

/** Deserializes the log checkpoint header stored in the given buffer.
@param[in]   buf       the buffer to deserialize
@param[out]  header    the deserialized header */
bool checkpoint_header_deserialize(const byte *buf, Checkpoint_header &header);

/** Provides name of the log file with the given file id, e.g. 'ib_logfile0'.
@param[in]  file_id   id of the log file
@return file name */
std::string file_name(Log_file_id file_id);

/** Validates that ib_logfile0 exists and has format older than VERSION_8_0_30.
@param[in]  files_ctx  defines context within which redo log files exist
@param[in]  files      non-empty list of file headers of existing log
                       files, ordered by file_id
@param[out] format     discovered redo format if true was returned
@return true iff ib_logfile0 exists and has format older than VERSION_8_0_30 */
bool files_validate_format(const Log_files_context &files_ctx,
                           const ut::vector<Log_file_id_and_header> &files,
                           Log_format &format);

}  // namespace log_pre_8_0_30

#endif /* log0pre_8_0_30_h */