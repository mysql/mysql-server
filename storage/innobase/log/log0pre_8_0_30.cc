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
 @file log/log0pre_8_0_30.cc

 Redo log - functions related to redo formats before 8.0.30.

 *******************************************************/

/* std::ostringstream */
#include <sstream>

#include "log0files_io.h"
#include "log0pre_8_0_30.h"
#include "log0types.h"
#include "mach0data.h"

namespace log_pre_8_0_30 {

static os_offset_t compute_size_offset(os_offset_t file_size,
                                       os_offset_t real_offset) {
  const auto files_before = real_offset / file_size;
  return real_offset - LOG_FILE_HDR_SIZE * (1 + files_before);
}

static os_offset_t compute_real_offset(os_offset_t file_size,
                                       os_offset_t size_offset) {
  const auto files_before = size_offset / (file_size - LOG_FILE_HDR_SIZE);
  return size_offset + LOG_FILE_HDR_SIZE * (1 + files_before);
}

os_offset_t compute_real_offset_for_lsn(size_t n_files, os_offset_t file_size,
                                        lsn_t some_file_lsn,
                                        os_offset_t some_file_offset,
                                        lsn_t requested_lsn) {
  os_offset_t size_offset;
  os_offset_t size_capacity;
  os_offset_t delta;

  size_capacity = n_files * (file_size - LOG_FILE_HDR_SIZE);

  if (requested_lsn >= some_file_lsn) {
    delta = requested_lsn - some_file_lsn;
    delta = delta % size_capacity;
  } else {
    /* Special case because lsn and offset are unsigned. */
    delta = some_file_lsn - requested_lsn;
    delta = size_capacity - delta % size_capacity;
  }

  size_offset = compute_size_offset(file_size, some_file_offset);
  size_offset = (size_offset + delta) % size_capacity;

  return compute_real_offset(file_size, size_offset);
}

bool checkpoint_header_deserialize(const byte *buf, Checkpoint_header &header) {
  header.m_checkpoint_no = mach_read_from_8(buf + FIELD_CHECKPOINT_NO);

  header.m_checkpoint_lsn = mach_read_from_8(buf + FIELD_CHECKPOINT_LSN);

  header.m_checkpoint_offset = mach_read_from_8(buf + FIELD_CHECKPOINT_OFFSET);

  header.m_log_buf_size = mach_read_from_8(buf + FIELD_CHECKPOINT_LOG_BUF_SIZE);

  return log_header_checksum_is_ok(buf);
}

std::string file_name(Log_file_id file_id) {
  ut_a(file_id <= FILE_MAX_ID);
  std::ostringstream str;
  str << FILE_BASE_NAME << file_id;
  return str.str();
}

bool files_validate_format(const Log_files_context &files_ctx,
                           const ut::vector<Log_file_id_and_header> &files,
                           Log_format &format) {
  ut_a(!files.empty());
  const auto first_file = files.front();
  if (first_file.m_id == 0) {
    if (first_file.m_header.m_format < to_int(Log_format::VERSION_8_0_30)) {
      format = static_cast<Log_format>(first_file.m_header.m_format);
      return true;
    }
    const auto file_path = log_file_path(files_ctx, 0);
    ib::error(ER_IB_MSG_LOG_FILE_FORMAT_TOO_NEW, file_path.c_str(),
              ulong{first_file.m_header.m_format});
    return false;
  } else {
    const auto directory = log_directory_path(files_ctx);
    ib::error(ER_IB_MSG_LOG_PRE_8_0_30_MISSING_FILE0, directory.c_str());
    return false;
  }
}

}  // namespace log_pre_8_0_30
