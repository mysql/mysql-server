/*****************************************************************************

Copyright (c) 1995, 2026, Oracle and/or its affiliates.

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
  const auto &first_file = files.front();
  if (first_file.m_id == 0) {
    if (first_file.m_header.m_format < to_int(Log_format::VERSION_8_0_30)) {
      format = static_cast<Log_format>(first_file.m_header.m_format);
      return true;
    }
    const auto file_path = log_file_path(files_ctx, 0);
    ib::error(ER_IB_MSG_LOG_FILE_FORMAT_TOO_NEW, file_path.c_str(),
              ulong{first_file.m_header.m_format});
    return false;
  }
  const auto directory = log_directory_path(files_ctx);
  ib::error(ER_IB_MSG_LOG_PRE_8_0_30_MISSING_FILE0, directory.c_str());
  return false;
}

}  // namespace log_pre_8_0_30
