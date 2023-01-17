/*
  Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#include "mysql/harness/filesystem.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <ostream>
#include <string_view>

namespace mysql_harness {

////////////////////////////////////////////////////////////////
// class Path members and free functions

Path::Path() noexcept : type_(FileType::EMPTY_PATH) {}

// throws std::invalid_argument
Path::Path(std::string path)
    : path_(std::move(path)), type_(FileType::TYPE_UNKNOWN) {
#ifdef _WIN32
  // in Windows, we normalize directory separator from \ to /, to not
  // confuse the rest of the code, which assume \ to be an escape char
  std::string::size_type p = path_.find('\\');
  while (p != std::string::npos) {
    path_[p] = '/';
    p = path_.find('\\');
  }
#endif
  std::string::size_type pos = path_.find_last_not_of(directory_separator);
  if (pos != std::string::npos)
    path_.erase(pos + 1);
  else if (path_.size() > 0)
    path_.erase(1);
  else
    throw std::invalid_argument("Empty path");
}

// throws std::invalid_argument
void Path::validate_non_empty_path() const {
  if (!is_set()) {
    throw std::invalid_argument("Empty path");
  }
}

bool Path::operator==(const Path &rhs) const {
  return real_path().str() == rhs.real_path().str();
}

bool Path::operator<(const Path &rhs) const { return path_ < rhs.path_; }

Path Path::basename() const {
  validate_non_empty_path();  // throws std::invalid_argument
  std::string::size_type pos = path_.find_last_of(directory_separator);
  if (pos == std::string::npos)
    return *this;
  else if (pos > 1)
    return std::string(path_, pos + 1);
  else
    return Path(root_directory);
}

Path Path::dirname() const {
  validate_non_empty_path();  // throws std::invalid_argument
  std::string::size_type pos = path_.find_last_of(directory_separator);
  if (pos == std::string::npos)
    return Path(".");
  else if (pos > 0)
    return std::string(path_, 0, pos);
  else
    return Path(root_directory);
}

bool Path::is_directory() const {
  validate_non_empty_path();  // throws std::invalid_argument
  return type() == FileType::DIRECTORY_FILE;
}

bool Path::is_regular() const {
  validate_non_empty_path();  // throws std::invalid_argument
  return type() == FileType::REGULAR_FILE;
}

bool Path::exists() const {
  validate_non_empty_path();  // throws std::invalid_argument
  // First type() needs to be force refreshed as the type of the file could have
  // been changed in the meantime (e.g. file was created)
  return type(true) != FileType::FILE_NOT_FOUND &&
         type() != FileType::STATUS_ERROR;
}

void Path::append(const Path &other) {
  validate_non_empty_path();        // throws std::invalid_argument
  other.validate_non_empty_path();  // throws std::invalid_argument
  path_.append(directory_separator + other.path_);
  type_ = FileType::TYPE_UNKNOWN;
}

Path Path::join(const Path &other) const {
  validate_non_empty_path();        // throws std::invalid_argument
  other.validate_non_empty_path();  // throws std::invalid_argument
  Path result(*this);
  result.append(other);
  return result;
}

static const char *file_type_name(Path::FileType type) {
  switch (type) {
    case Path::FileType::DIRECTORY_FILE:
      return "a directory";
    case Path::FileType::CHARACTER_FILE:
      return "a character device";
    case Path::FileType::BLOCK_FILE:
      return "a block device";
    case Path::FileType::EMPTY_PATH:
      return "an empty path";
    case Path::FileType::FIFO_FILE:
      return "a FIFO";
    case Path::FileType::FILE_NOT_FOUND:
      return "not found";
    case Path::FileType::REGULAR_FILE:
      return "a regular file";
    case Path::FileType::TYPE_UNKNOWN:
      return "unknown";
    case Path::FileType::STATUS_ERROR:
      return "error";
    case Path::FileType::SOCKET_FILE:
      return "a socket";
    case Path::FileType::SYMLINK_FILE:
      return "a symlink";
  }

  // in case a non-enum value is passed in, return 'undefined'
  // [should never happen]
  //
  // note: don't use 'default:' in the switch to get a warning for
  // 'unhandled enunaration' when new values are added.
  return "undefined";
}

std::ostream &operator<<(std::ostream &out, Path::FileType type) {
  out << file_type_name(type);
  return out;
}

///////////////////////////////////////////////////////////
// Directory::Iterator members

Directory::DirectoryIterator Directory::begin() {
  return DirectoryIterator(*this);
}

Directory::DirectoryIterator Directory::glob(const std::string &pattern) {
  return DirectoryIterator(*this, pattern);
}

Directory::DirectoryIterator Directory::end() { return DirectoryIterator(); }

Directory::DirectoryIterator Directory::cbegin() const {
  return DirectoryIterator(*this);
}

Directory::DirectoryIterator Directory::cend() const {
  return DirectoryIterator();
}

bool Directory::is_empty() const {
  return std::none_of(cbegin(), cend(), [](const Directory &dir) {
    std::string name = dir.basename().str();
    return name != "." && name != "..";
  });
}

std::vector<Path> Directory::list_recursive() const {
  auto merge_subpaths = [](mysql_harness::Directory dir,
                           std::vector<mysql_harness::Path> subpaths) {
    std::transform(
        std::begin(subpaths), std::end(subpaths), std::begin(subpaths),
        [&dir](mysql_harness::Path &subpath) { return dir.join(subpath); });
    return subpaths;
  };

  // Recursively visit all subdirectories (for files just return their name),
  // call upper on the stack merges the parent directory name to the returned
  // path.
  std::function<std::vector<mysql_harness::Path>(mysql_harness::Directory)>
      recurse = [&recurse, &merge_subpaths](mysql_harness::Directory dir) {
        std::vector<mysql_harness::Path> result;
        for (const auto &file : dir) {
          if (file.is_directory() &&
              !mysql_harness::Directory{file}.is_empty()) {
            auto partial_results = merge_subpaths(
                file.basename(), recurse(mysql_harness::Directory{file}));
            std::move(std::begin(partial_results), std::end(partial_results),
                      std::back_inserter(result));
          } else {
            result.push_back(file.basename());
          }
        }
        return result;
      };

  return recurse(*this);
}

///////////////////////////////////////////////////////////
// Directory members

Directory::~Directory() = default;

// throws std::invalid_argument
Directory::Directory(const Path &path) : Path(path) {}

////////////////////////////////////////////////////////////////////////////////
//
// Utility free functions
//
////////////////////////////////////////////////////////////////////////////////

stdx::expected<void, std::error_code> delete_dir_recursive(
    const std::string &dir) noexcept {
  mysql_harness::Directory d(dir);
  try {
    for (auto const &f : d) {
      if (f.is_directory()) {
        const auto res = delete_dir_recursive(f.str());
        if (!res) return res.get_unexpected();
      } else {
        const auto res = delete_file(f.str());
        if (!res) return res.get_unexpected();
      }
    }
  } catch (...) {
    return stdx::make_unexpected(
        std::error_code(errno, std::system_category()));
  }

  return delete_dir(dir);
}

std::string get_plugin_dir(const std::string &runtime_dir) {
  std::string cur_dir = Path(runtime_dir).basename().str();
  if (cur_dir == "runtime_output_directory") {
    // single configuration build
    return Path(runtime_dir).dirname().join("plugin_output_directory").str();
  } else {
    // multiple configuration build
    // in that case cur_dir has to be configuration name (Debug, Release etc.)
    // we need to go 2 levels up
    return Path(runtime_dir)
        .dirname()
        .dirname()
        .join("plugin_output_directory")
        .join(cur_dir)
        .str();
  }
}

std::string get_tests_data_dir(const std::string &runtime_dir) {
  std::string cur_dir = Path(runtime_dir).basename().str();
  if (cur_dir == "runtime_output_directory") {
    // single configuration build
    return Path(runtime_dir)
        .dirname()
        .join("router")
        .join("tests")
        .join("data")
        .str();
  } else {
    // multiple configuration build
    // in that case cur_dir has to be configuration name (Debug, Release etc.)
    // we need to go 2 levels up
    return Path(runtime_dir)
        .dirname()
        .dirname()
        .join("router")
        .join("tests")
        .join("data")
        .join(cur_dir)
        .str();
  }
}

int mkdir_wrapper(const std::string &dir, perm_mode mode);

int mkdir_recursive(const Path &path, perm_mode mode) {
  if (path.str().empty() || path.c_str() == Path::root_directory) return -1;

  // "mkdir -p" on Unix succeeds even if the directory one tries to create
  // exists, we want to mimic that
  if (path.exists()) {
    return path.is_directory() ? 0 : -1;
  }

  const auto parent = path.dirname();
  if (!parent.exists()) {
    auto res = mkdir_recursive(parent, mode);
    if (res != 0) return res;
  }

  return mkdir_wrapper(path.str(), mode);
}

int mkdir(const std::string &dir, perm_mode mode, bool recursive) {
  if (!recursive) {
    return mkdir_wrapper(dir, mode);
  }

  return mkdir_recursive(mysql_harness::Path(dir), mode);
}

void check_file_access_rights(const std::string &file_name) {
  auto rights_res = access_rights_get(file_name);
  if (!rights_res) {
    auto ec = rights_res.error();

    if (ec == std::errc::no_such_file_or_directory) return;

    throw std::system_error(
        ec, "getting access rights for '" + file_name + "' failed");
  }

  auto verify_res =
      access_rights_verify(rights_res.value(), DenyOtherReadWritableVerifier());
  if (!verify_res) {
    const auto ec = verify_res.error();

    throw std::system_error(ec, "'" + file_name + "' has insecure permissions");
  }
}

}  // namespace mysql_harness
