/*****************************************************************************

Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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
 @file include/log0files_finder.h

 *******************************************************/

#ifndef log0files_finder_h
#define log0files_finder_h

#include <string>

/* Log_files_dict */
#include "log0files_dict.h"

/* Log_format, Log_flags */
#include "log0types.h"

/** Result returned by the @see log_files_find_and_analyze(). */
enum class Log_files_find_result {
  /** We have found valid log files. */
  FOUND_VALID_FILES,

  /** There were log files which have not been fully initialized. */
  FOUND_UNINITIALIZED_FILES,

  /** There were no log files at all. */
  FOUND_NO_FILES,

  /** Found valid log files, but discovered that some of the newest are missing.
  When this is reported, it means that the only reason why the FOUND_VALID_FILES
  was not returned, is that the newest found log file has been already marked as
  full. */
  FOUND_VALID_FILES_BUT_MISSING_NEWEST,

  /** Found log files, but marked as "crash unsafe" (disabled redo) */
  FOUND_DISABLED_FILES,

  /** Found log files, but structure is corrupted. */
  FOUND_CORRUPTED_FILES,

  /** System error occurred when scanning files on disk. */
  SYSTEM_ERROR
};

/** Scans for existing log files on disk. Performs basic validation for the
files that have been found. Clears and builds the Log_files_dict. Determines
global properties of the redo log files: format, creator, flags and uuid.
@param[in]    read_only            T: check file permissions only for reading,
                                   F: check for both reading and writing
@param[in]    encryption_metadata  pointer to encryption metadata to be used by
                                   all redo log file IO operations except those
                                   related to the first LOG_FILE_HDR_SIZE bytes
                                   of each log file
                                   @see Log_file_handle::m_encryption_metadata
@param[out]   files                dictionary of files that have been found
                                   when FOUND_VALID_FILES is returned;
                                   otherwise stays untouched
@param[out]   format               discovered format of the redo log files
@param[out]   creator_name         name of creator of the files
@param[out]   log_flags            discovered flags of the redo log files
@param[out]   log_uuid             discovered uuid of the redo log files
@return result of the scan and validation - @see Log_files_find_result */
Log_files_find_result log_files_find_and_analyze(
    bool read_only, Encryption_metadata &encryption_metadata,
    Log_files_dict &files, Log_format &format, std::string &creator_name,
    Log_flags &log_flags, Log_uuid &log_uuid);

#endif /* !log0files_finder_h */
