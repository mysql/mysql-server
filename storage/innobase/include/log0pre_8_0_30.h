/*****************************************************************************

Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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