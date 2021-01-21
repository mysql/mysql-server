/*
  Copyright (c) 2020, 2021, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "auto_cleaner.h"

#include <exception>
#include <utility>  // make_pair

#include "common.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/logging/logging.h"
#include "utils.h"  // copy_file

IMPORT_LOG_FUNCTIONS()

namespace mysqlrouter {

bool AutoCleaner::add_file_delete(const std::string &file) {
  return files_.insert(std::make_pair(file, std::make_pair(File, ""))).second;
}

bool AutoCleaner::add_directory_delete(const std::string &directory,
                                       bool recursive) {
  const auto type = recursive ? DirectoryRecursive : Directory;
  return files_.insert(std::make_pair(directory, std::make_pair(type, "")))
      .second;
}

bool AutoCleaner::add_file_revert(const std::string &file) {
  return add_file_revert(file, file + ".bck");
}

bool AutoCleaner::add_file_revert(const std::string &file,
                                  const std::string &backup_file) {
  bool res;
  if (mysql_harness::Path(file).is_regular()) {
    try {
      copy_file(file, backup_file);
    } catch (const std::exception &e) {
      log_warning("Failed to copy '%s' to '%s': %s", file.c_str(),
                  backup_file.c_str(), e.what());
      return false;
    }

    res = files_
              .insert(
                  std::make_pair(file, std::make_pair(FileBackup, backup_file)))
              .second;
  } else {
    if (mysql_harness::Path(backup_file).exists())
      mysql_harness::delete_file(backup_file);
    res = files_.insert(std::make_pair(file, std::make_pair(File, ""))).second;
  }
  return res;
}

void AutoCleaner::add_cleanup_callback(
    std::function<void()> callback) noexcept {
  callbacks_.emplace_back(callback);
}

void AutoCleaner::clear_cleanup_callbacks() noexcept { callbacks_.clear(); }

void AutoCleaner::remove(const std::string &file) noexcept {
  files_.erase(file);
}

void AutoCleaner::clear() {
  for (auto f = files_.rbegin(); f != files_.rend(); ++f) {
    if (f->second.first == FileBackup &&
        mysql_harness::delete_file(f->second.second) != 0) {
      log_warning("Could not delete backup file '%s': %s",
                  f->second.second.c_str(),
                  mysql_harness::get_strerror(errno).c_str());
    }
  }
  files_.clear();
  callbacks_.clear();
}

AutoCleaner::~AutoCleaner() {
  // remove in reverse order, so that files are deleted before their
  // contained directories
  for (auto f = files_.rbegin(); f != files_.rend(); ++f) {
    switch (f->second.first) {
      case File:
        if (mysql_harness::delete_file(f->first) != 0) {
          log_error("Could not delete file '%s': %s", f->first.c_str(),
                    mysql_harness::get_strerror(errno).c_str());
        }
        break;

      case Directory:
        if (mysql_harness::delete_dir(f->first) != 0) {
          log_error("Could not delete directory '%s': %s", f->first.c_str(),
                    mysql_harness::get_strerror(errno).c_str());
        }
        break;

      case DirectoryRecursive:
        if (mysql_harness::delete_dir_recursive(f->first) != 0) {
          log_error("Could not delete directory '%s': %s", f->first.c_str(),
                    mysql_harness::get_strerror(errno).c_str());
        }
        break;

      case FileBackup:
        try {
          copy_file(f->second.second, f->first);
          if (mysql_harness::delete_file(f->second.second) != 0) {
            log_warning("Could not delete file'%s': %s",
                        f->second.second.c_str(),
                        mysql_harness::get_strerror(errno).c_str());
          }
        } catch (const std::exception &e) {
          log_error("Could not revert '%s' file: %s", f->first.c_str(),
                    e.what());
        }
        break;
    }
  }

  for (const auto &callback : callbacks_) {
    try {
      callback();
    } catch (const std::exception &e) {
      log_error("Automatic cleanup callback failed: %s", e.what());
    }
  }
}

}  // namespace mysqlrouter
