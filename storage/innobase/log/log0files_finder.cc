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
 @file log/log0files_finder.cc

 Redo log - finding log files and inspecting their meta data.

 *******************************************************/

#include <string>

/* Log_files_find_result */
#include "log0files_finder.h"

/* log_collect_existing_files, ... */
#include "log0files_io.h"

/* log_pre_8_0_30::files_validate_format */
#include "log0pre_8_0_30.h"

/* Log_file_id, constants */
#include "log0types.h"

/* os_offset_t */
#include "os0file.h"

/**************************************************/ /**

 @name Log - multiple files analysis

 *******************************************************/

/** @{ */

/** Validates that all redo files have the same format and the format
is in range [Log_format::VERSION_8_0_30, Log_format::CURRENT].
@param[in]  files_ctx  context within which files exist
@param[in]  files      non-empty list of file headers of existing log
                       files, ordered by file_id
@param[out] format     discovered format if true was returned
@return true iff all files have the same format and the format is neither
older than Log_format::VERSION_8_0_30 nor newer than Log_format::CURRENT */
static bool log_files_validate_format(
    const Log_files_context &files_ctx,
    const ut::vector<Log_file_id_and_header> &files, Log_format &format) {
  ut_a(!files.empty());
  const uint32_t first_file_format_int = files.front().m_header.m_format;
  for (const auto &file : files) {
    const uint32_t curr_format_int = file.m_header.m_format;
    if (curr_format_int > to_int(Log_format::CURRENT)) {
      const auto file_path = log_file_path(files_ctx, file.m_id);
      ib::error(ER_IB_MSG_LOG_FILE_FORMAT_UNKNOWN, ulong{curr_format_int},
                file_path.c_str(), REFMAN "upgrading-downgrading.html");
      return false;
    }
    if (curr_format_int < to_int(Log_format::VERSION_8_0_30)) {
      /* Format of redo file is too old for the configured ruleset. */
      const auto file_path = log_file_path(files_ctx, file.m_id);
      ib::error(ER_IB_MSG_LOG_FILE_FORMAT_TOO_OLD, file_path.c_str(),
                ulong{curr_format_int});
      return false;
    }
    if (curr_format_int != first_file_format_int) {
      /* Two existing redo files have different format. */
      const auto first_file_path = log_file_path(files_ctx, files.front().m_id);
      const auto other_file_path = log_file_path(files_ctx, file.m_id);
      ib::error(ER_IB_MSG_LOG_FILE_DIFFERENT_FORMATS, first_file_path.c_str(),
                ulong{first_file_format_int}, other_file_path.c_str(),
                ulong{curr_format_int});
      return false;
    }
  }
  format = static_cast<Log_format>(first_file_format_int);
  return true;
}

/** Validates that all log files have start_lsn % OS_FILE_LOG_BLOCK_SIZE == 0,
@param[in]  files_ctx     context within which files exist
@param[in]  file_headers  non-empty list of file headers of existing log files,
                          ordered by file_id
@return true if all files have possibly proper value of start_lsn */
static bool log_files_validate_start_lsn(
    const Log_files_context &files_ctx,
    const ut::vector<Log_file_id_and_header> &file_headers) {
  for (const auto &file : file_headers) {
    if (file.m_header.m_start_lsn % OS_FILE_LOG_BLOCK_SIZE != 0) {
      const auto file_path = log_file_path(files_ctx, file.m_id);
      ib::error(ER_IB_MSG_LOG_FILE_INVALID_START_LSN, file_path.c_str(),
                ulonglong{file.m_header.m_start_lsn});
      return false;
    }
  }
  return true;
}

/** Validates that all log files create a chain of consecutive lsn ranges.
@param[in]  files_ctx     context within which files exist
@param[in]  file_sizes    non-empty list of file sizes of existing log files,
                          ordered by file_id
@param[in]  file_headers  non-empty list of file headers of existing log files,
                          ordered by file_id
@return true iff lsn ranges seem to be consistent and form a single chain */
static bool log_files_validate_lsn_chain(
    const Log_files_context &files_ctx,
    const ut::vector<Log_file_id_and_size> &file_sizes,
    const ut::vector<Log_file_id_and_header> &file_headers) {
  ut_a(!file_sizes.empty());
  ut_a(file_sizes.size() == file_headers.size());

  /* Start at min_start_lsn and traverse all redo files.
  Check that file's end_lsn is equal to start_lsn of the next file. */
  lsn_t expected_start_lsn = file_headers.front().m_header.m_start_lsn;
  Log_file_id expected_file_id = file_headers.front().m_id;
  for (size_t i = 0; i < file_headers.size(); ++i) {
    const auto &file = file_headers[i];
    ut_ad(file_sizes[i].m_id == file.m_id);

    if (file.m_id != expected_file_id) {
      /* We are missing file with id = expected_file_id. */
      const auto file_path = log_file_path(files_ctx, expected_file_id);
      ib::error(ER_IB_MSG_LOG_FILE_MISSING_FOR_ID, file_path.c_str(),
                ulonglong{expected_start_lsn});
      return false;
    }

    if (file.m_header.m_start_lsn != expected_start_lsn) {
      const auto file_path = log_file_path(files_ctx, expected_file_id);
      ib::error(ER_IB_MSG_LOG_FILE_INVALID_LSN_RANGES, file_path.c_str(),
                ulonglong{file.m_header.m_start_lsn},
                ulonglong{expected_start_lsn});
      return false;
    }

    const bool ret = log_file_compute_end_lsn(file.m_header.m_start_lsn,
                                              file_sizes[i].m_size_in_bytes,
                                              expected_start_lsn);
    ut_a(ret);

    expected_file_id = Log_file::next_id(expected_file_id);
  }
  return true;
}

/** Validates if all existing redo log files have the same log_uuid.
@param[in]  files_ctx  context within which files exist
@param[in]  files      non-empty list of file headers of existing log files,
                       ordered by file_id
@param[out] log_uuid   the log_uuid that has been discovered
@return true iff all existing log files had the same log_uuid value */
static bool log_files_validate_log_uuid(
    const Log_files_context &files_ctx,
    const ut::vector<Log_file_id_and_header> &files, Log_uuid &log_uuid) {
  ut_a(!files.empty());
  log_uuid = files.front().m_header.m_log_uuid;
  for (const auto &file : files) {
    if (log_uuid != file.m_header.m_log_uuid) {
      const std::string file_path = log_file_path(files_ctx, file.m_id);
      const std::string first_file_path =
          log_file_path(files_ctx, files.front().m_id);
      ib::error(ER_IB_MSG_LOG_FILE_FOREIGN_UUID, file_path.c_str(),
                first_file_path.c_str());
      return false;
    }
  }
  return true;
}

/** Validates if a set of redo log files consists of files of equal size.
This is used for files with older redo format.
@param[in]  files_ctx  context within which files exist
@param[in]  files      non-empty list of file headers of existing log files,
                       ordered by file_id
@return true iff all redo log files have the same size */
static bool log_files_validate_file_sizes_equal(
    const Log_files_context &files_ctx,
    const ut::vector<Log_file_id_and_size> &files) {
  ut_a(!files.empty());
  const os_offset_t first_file_size = files.front().m_size_in_bytes;
  for (const auto &file : files) {
    if (file.m_size_in_bytes != first_file_size) {
      const auto file_path = log_file_path(files_ctx, file.m_id);
      ib::error(ER_IB_MSG_LOG_FILES_DIFFERENT_SIZES, file_path.c_str(),
                ulonglong{file.m_size_in_bytes}, ulonglong{first_file_size});
      return false;
    }
  }
  return true;
}

/** @} */

/**************************************************/ /**

 @name Log - files finder

 *******************************************************/

/** @{ */

Log_files_find_result log_files_find_and_analyze(
    bool read_only, Encryption_metadata &encryption_metadata,
    Log_files_dict &files, Log_format &format, std::string &creator_name,
    Log_flags &log_flags, Log_uuid &log_uuid) {
  ut::vector<Log_file_id_and_size> file_sizes;

  switch (log_collect_existing_files(files.ctx(), read_only, file_sizes)) {
    case DB_NOT_FOUND:
      return Log_files_find_result::FOUND_NO_FILES;
    case DB_SUCCESS:
      ut_a(!file_sizes.empty());
      break;
    case DB_ERROR:
      /* Error message emitted in log_collect_existing_files. */
      return Log_files_find_result::SYSTEM_ERROR;
    default:
      ut_error;
  }

  /* Read headers of all log files. */
  ut::vector<Log_file_id_and_header> file_headers;
  for (const auto &file : file_sizes) {
    Log_file_header file_header;

    /* Redo log file header is never encrypted. */
    Encryption_metadata unused_encryption_metadata;

    auto file_handle =
        Log_file::open(files.ctx(), file.m_id, Log_file_access_mode::READ_ONLY,
                       unused_encryption_metadata, Log_file_type::NORMAL);

    /* The file can be opened - checked by
    the log_collect_existing_files() */
    if (!file_handle.is_open()) {
      return Log_files_find_result::SYSTEM_ERROR;
    }

    if (log_file_header_read(file_handle, file_header) != DB_SUCCESS) {
      ib::error(ER_IB_MSG_LOG_FILE_HEADER_READ_FAILED,
                file_handle.file_path().c_str());
      return Log_files_find_result::SYSTEM_ERROR;
    }
    file_headers.emplace_back(file.m_id, file_header);
  }

  /* Read properties global to the whole set of redo log files:
    - format,
    - creator_name,
    - log_flags,
    - log_uuid. */

  if (files.ctx().m_files_ruleset > Log_files_ruleset::PRE_8_0_30) {
    const auto &newest_file_header = file_headers.back().m_header;
    log_flags = newest_file_header.m_log_flags;
    creator_name = newest_file_header.m_creator_name;
    format = Log_format::LEGACY;

    if (!log_files_validate_format(files.ctx(), file_headers, format)) {
      /* Error message emitted in log_files_validate_format. */
      return Log_files_find_result::FOUND_CORRUPTED_FILES;
    }
    ut_a(format >= Log_format::VERSION_8_0_30);

    if (!log_files_validate_log_uuid(files.ctx(), file_headers, log_uuid)) {
      /* Error message emitted in log_files_validate_log_uuid. */
      return Log_files_find_result::FOUND_CORRUPTED_FILES;
    }
    if (!log_files_validate_start_lsn(files.ctx(), file_headers)) {
      /* Error message emitted in log_files_validate_start_lsn. */
      return Log_files_find_result::FOUND_CORRUPTED_FILES;
    }
    if (!log_files_validate_lsn_chain(files.ctx(), file_sizes, file_headers)) {
      /* Error message emitted in log_files_validate_lsn_chain. */
      return Log_files_find_result::FOUND_CORRUPTED_FILES;
    }
  } else {
    const auto &file0_header = file_headers.front().m_header;
    creator_name = file0_header.m_creator_name;
    log_flags = file0_header.m_log_flags;
    log_uuid = Log_uuid{};
    format = Log_format::LEGACY;

    if (!log_pre_8_0_30::files_validate_format(files.ctx(), file_headers,
                                               format)) {
      /* Error message emitted in log_pre_8_0_30::files_validate_format. */
      return Log_files_find_result::FOUND_CORRUPTED_FILES;
    }
    ut_a(format < Log_format::VERSION_8_0_30);
    ut_a(file_sizes.front().m_id == 0);

    if (file_sizes.size() < 2) {
      /* Must have at least 2 log files. */
      ib::error(ER_IB_MSG_LOG_FILES_INVALID_SET);
      return Log_files_find_result::FOUND_CORRUPTED_FILES;
    }
    if (!log_files_validate_file_sizes_equal(files.ctx(), file_sizes)) {
      /* Error message emitted in log_files_validate_file_sizes_equal. */
      return Log_files_find_result::FOUND_CORRUPTED_FILES;
    }
  }

  if (Log_format::VERSION_8_0_19 <= format) {
    /* Check if log files have been initialized. */
    if (log_file_header_check_flag(log_flags,
                                   LOG_HEADER_FLAG_NOT_INITIALIZED)) {
      if (format == Log_format::CURRENT) {
        return Log_files_find_result::FOUND_UNINITIALIZED_FILES;
      } else {
        ib::error(ER_IB_MSG_LOG_UPGRADE_UNINITIALIZED_FILES,
                  ulong{to_int(format)});
        return Log_files_find_result::FOUND_CORRUPTED_FILES;
      }
    }

    /* Exit if server is crashed while running without redo logging. */
    if (log_file_header_check_flag(log_flags, LOG_HEADER_FLAG_CRASH_UNSAFE)) {
      /* As of today, the only scenario which leads us here is that
      log_persist_disable() was called and then we crashed. If we
      ever introduce more possibilities, then we need to update
      the error message. */
      ut_ad(log_file_header_check_flag(log_flags, LOG_HEADER_FLAG_NO_LOGGING));
      ib::error(ER_IB_ERR_RECOVERY_REDO_DISABLED);
      return Log_files_find_result::FOUND_DISABLED_FILES;
    }
  }

  /* The newest log file must not be marked as full. If the existing newest
  file is marked as such, it means that the real newest log file was lost. */
  if (log_file_header_check_flag(log_flags, LOG_HEADER_FLAG_FILE_FULL)) {
    ib::error(ER_IB_MSG_LOG_FILES_FOUND_MISSING);
    return Log_files_find_result::FOUND_VALID_FILES_BUT_MISSING_NEWEST;
  }

  lsn_t size_capacity = 0;
  for (const auto &file : file_sizes) {
    ut_a(file.m_size_in_bytes >= LOG_FILE_HDR_SIZE);
    lsn_t file_lsn_capacity;
    const bool ret = log_file_compute_logical_capacity(file.m_size_in_bytes,
                                                       file_lsn_capacity);
    ut_a(ret);
    size_capacity += file_lsn_capacity;
  }

  /* Filling dictionary. */

  files.clear();
  for (size_t i = 0; i < file_sizes.size(); ++i) {
    ut_a(i == 0 ||
         Log_file::next_id(file_sizes[i - 1].m_id) == file_sizes[i].m_id);
    ut_a(file_sizes[i].m_id == file_headers[i].m_id);

    if (format >= Log_format::VERSION_8_0_30) {
      ut_a(i == 0 || file_headers[i - 1].m_header.m_start_lsn <
                         file_headers[i].m_header.m_start_lsn);

      files.add(file_sizes[i].m_id, file_sizes[i].m_size_in_bytes,
                file_headers[i].m_header.m_start_lsn,
                log_file_header_check_flag(file_headers[i].m_header.m_log_flags,
                                           LOG_HEADER_FLAG_FILE_FULL),
                encryption_metadata);
    } else {
      files.add(file_sizes[i].m_id, file_sizes[i].m_size_in_bytes, 0, true,
                encryption_metadata);
    }
  }

  /** The size_capacity was computed by iterating files_list, in which we
  potentially could have two files with the same m_id if some bug was
  introduced. This should be caught easily by this check. */
  ut_a(size_capacity == log_files_capacity_of_existing_files(files));

  return Log_files_find_result::FOUND_VALID_FILES;
}

/** @} */
