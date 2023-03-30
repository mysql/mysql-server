/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTER_AUTO_CLEANER_INCLUDED
#define ROUTER_AUTO_CLEANER_INCLUDED

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "mysqlrouter/router_export.h"

namespace mysqlrouter {

/**
 * Automatic cleanup on scope exit utility class.
 *
 * Automatic cleanup takes place on AutoCleaner object destruction. It allows
 * to:
 * - cleanup files
 * - remove directories (non-recursive and recursive)
 * - revert files from auto-managed backup file
 * - revert files from a user provided backup file
 * - call user provided callbacks
 * Callbacks are called in order they were added into AutoCleaner. Files and
 * directories are being deleted in a reverse order in which they were added to
 * the AutoCleaner. If automatic cleanup fails (file or directory could not be
 * deleted, failed to revert a file) a proper error is being logged and
 * AutoCleaner continues with the next cleanup steps.
 *
 * AutoCleaner allows to clear its state so that no action will be taken on
 * scope exit (auto generated backup files will be cleaned up in such case).
 *
 * Adding an action (cleanup, revert) is done once per file. It is not possible
 * to add a second action for the same file (such add call will fail, initial
 * action will not be affected). Adding a revert file action may fail if initial
 * or backup files could not be opened.
 */
class ROUTER_LIB_EXPORT AutoCleaner {
 public:
  void add_file_delete(const std::string &file);
  void add_directory_delete(const std::string &d, bool recursive = false);
  void add_file_revert(const std::string &file);
  void add_file_revert(const std::string &file, const std::string &backup_file);
  void add_cleanup_callback(std::function<void()> callback) noexcept;
  void clear_cleanup_callbacks() noexcept;
  void remove(const std::string &file) noexcept;
  void clear();

  AutoCleaner() = default;
  ~AutoCleaner();

  AutoCleaner(AutoCleaner &&other) noexcept = default;
  AutoCleaner &operator=(AutoCleaner &&other) = default;

  AutoCleaner(const AutoCleaner &) = delete;
  AutoCleaner &operator=(const AutoCleaner &) = delete;

 private:
  enum Type { Directory, DirectoryRecursive, File, FileBackup };

  /*
   * The vector stores all the files that are scheduled to be auto-removed or
   * restored from backup if clean() wasn't called.
   * The first value of pair is a name of file to backup, and second is a pair
   * of backup's type and name of backup file (used only for FileBackup type).
   */
  std::vector<std::pair<std::string, std::pair<Type, std::string>>> files_;

  /*
   * The vector stores callbacks that are scheduled to be called if clean()
   * wasn't called. Callbacks are not allowed to throw exceptions.
   */
  std::vector<std::function<void()>> callbacks_;
};

}  // namespace mysqlrouter
#endif  // ROUTER_AUTO_CLEANER_INCLUDED
