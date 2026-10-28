// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

// Spill directory operations. See spill_directory.h.

#include "sql/changestreams/apply/storage/in_memory/spill_directory.h"

#include <sys/stat.h>  // S_IRWXU, S_IRGRP, S_IXGRP
#include <cerrno>
#include <cstring>
#include <string>

#include "my_dbug.h"  // DBUG_EVALUATE_IF
#include "my_dir.h"   // my_dir, my_dirend, my_stat, MY_S_ISDIR, MY_DONT_SORT
#include "my_io.h"    // FN_LIBCHAR, MYSYS_STRERROR_SIZE, R_OK, W_OK
#include "my_sys.h"   // my_delete, my_errno, my_access, my_is_symlink
#include "mysql/components/services/log_builtins.h"  // LogErr
#include "mysql/my_loglevel.h"                       // ERROR_LEVEL
#include "mysqld_error.h"  // ER_LOG_PRINTF_MSG, ER_BINLOG_CANT_DELETE_FILE
#include "sql/changestreams/apply/storage/in_memory/spill_file_writer.h"

namespace mysql::csa {

namespace {

/// The mode mysqld --initialize gives the data directory (sql_initialize.cc).
constexpr int kSpillDirMode =
#ifdef _WIN32
    0;
#else
    S_IRWXU | S_IRGRP | S_IXGRP;
#endif

}  // namespace

std::string join_path(const std::string &dir, const std::string &leaf) {
  if (dir.empty()) return leaf;
  if (dir.back() == FN_LIBCHAR) return dir + leaf;
  return dir + FN_LIBCHAR + leaf;
}

std::string spill_directory_path(const std::string &relay_log_dir) {
  if (relay_log_dir.empty()) return {};
  return join_path(relay_log_dir, Spill_file_writer::kTempSubdirName);
}

bool ensure_spill_directory(const std::string &relay_log_dir) {
  const std::string dir = spill_directory_path(relay_log_dir);
  // No relay log directory is known for this channel yet, so there is no spill
  // directory to create. Not an error.
  if (dir.empty()) return false;

  // Log why the path cannot hold spill files and report it.
  auto unusable = [&dir](const std::string &cause) {
    std::string msg("[In-memory relay log] the spill file directory '");
    msg += dir;
    msg += "' cannot be used: ";
    msg += cause;
    LogErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, msg.c_str());
    return true;
  };

  // EEXIST is common (the directory survives restarts); the checks below
  // verify it. No MY_WME: the message names the directory explicitly.
  if (my_mkdir(dir.c_str(), kSpillDirMode, MYF(0)) != 0 &&
      my_errno() != EEXIST) {
    char errbuf[MYSYS_STRERROR_SIZE];
    const int err = my_errno();
    return unusable("it could not be created (errno " + std::to_string(err) +
                    " - " + my_strerror(errbuf, sizeof(errbuf), err) + ")");
  }
  if (my_is_symlink(dir.c_str(), nullptr))
    return unusable("it is a symbolic link, not a directory");
  MY_STAT stat_info;
  if (my_stat(dir.c_str(), &stat_info, MYF(0)) == nullptr ||
      !MY_S_ISDIR(stat_info.st_mode))
    return unusable("it exists but is not a directory");
#ifdef _WIN32
  constexpr int kAccessMode = R_OK | W_OK;  // Windows has no X_OK
#else
  constexpr int kAccessMode = R_OK | W_OK | X_OK;
#endif
  if (my_access(dir.c_str(), kAccessMode) != 0)
    return unusable(
        "the server has no read, write and search permission on it");
  return false;
}

bool purge_orphan_spill_files(const std::string &relay_log_dir) {
  const std::string dir = spill_directory_path(relay_log_dir);
  // No relay log directory is known for this channel yet, so it cannot own a
  // spill directory.
  if (dir.empty()) return false;
  // Never purge through a symlink; ensure_spill_directory() rejects it.
  if (my_is_symlink(dir.c_str(), nullptr)) return false;

  // No MY_WANT_STAT: it drops entries that lack the owner-read bit. No MY_WME:
  // an absent spill directory is the common case and must stay quiet.
  MY_DIR *dirp = my_dir(dir.c_str(), MYF(MY_DONT_SORT));
  if (dirp == nullptr) {
    const int err = my_errno();
    // A missing directory has nothing to purge.
    if (err == ENOENT) return false;
    // A non-directory path is left for ensure_spill_directory() to reject.
    if (err == ENOTDIR) return false;
    // Any other listing failure leaves orphans in place, so it is a failure.
    char errbuf[MYSYS_STRERROR_SIZE];
    std::string msg(
        "[In-memory relay log] could not read the spill file directory '");
    msg += dir;
    msg += "' to remove stale spill files (errno " + std::to_string(err) +
           " - " + my_strerror(errbuf, sizeof(errbuf), err) + ")";
    LogErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, msg.c_str());
    return true;
  }

  const std::size_t prefix_len =
      std::strlen(Spill_file_writer::kFileNamePrefix);
  bool delete_failed = false;

  for (uint i = 0; i < dirp->number_off_files; ++i) {
    const fileinfo &entry = dirp->dir_entry[i];
    if (entry.name == nullptr) continue;

    // Prefix match only: never touch anything this feature did not create. This
    // also excludes "." and "..".
    if (std::strncmp(entry.name, Spill_file_writer::kFileNamePrefix,
                     prefix_len) != 0) {
      continue;
    }

    const std::string path = join_path(dir, entry.name);
    // Never recurse into, and never remove, a directory -- even one that
    // happens to carry the spill file prefix.
    MY_STAT stat_info;
    if (my_stat(path.c_str(), &stat_info, MYF(0)) != nullptr &&
        MY_S_ISDIR(stat_info.st_mode))
      continue;
    // The debug point leaves the file in place, as a real failure would.
    if (DBUG_EVALUATE_IF("imr_force_spill_purge_delete_failure", true, false) ||
        my_delete(path.c_str(), MYF(0)) != 0) {
      // Report it as the binary log does, and go on with the other files.
      LogErr(ERROR_LEVEL, ER_BINLOG_CANT_DELETE_FILE, path.c_str());
      delete_failed = true;
    }
  }

  my_dirend(dirp);
  return delete_failed;
}

}  // namespace mysql::csa
