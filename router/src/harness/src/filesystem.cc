/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <cstring>
#include <fstream>
#include <ostream>

using std::string;

namespace mysql_harness {

////////////////////////////////////////////////////////////////
// class Path members and free functions

Path::Path() noexcept : type_(FileType::EMPTY_PATH) {}

// throws std::invalid_argument
Path::Path(const std::string &path)
    : path_(path), type_(FileType::TYPE_UNKNOWN) {
#ifdef _WIN32
  // in Windows, we normalize directory separator from \ to /, to not
  // confuse the rest of the code, which assume \ to be an escape char
  std::string::size_type p = path_.find('\\');
  while (p != std::string::npos) {
    path_[p] = '/';
    p = path_.find('\\');
  }
#endif
  string::size_type pos = path_.find_last_not_of(directory_separator);
  if (pos != string::npos)
    path_.erase(pos + 1);
  else if (path_.size() > 0)
    path_.erase(1);
  else
    throw std::invalid_argument("Empty path");
}

// throws std::invalid_argument
Path::Path(const char *path) : Path(string(path)) {}

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
  string::size_type pos = path_.find_last_of(directory_separator);
  if (pos == string::npos)
    return *this;
  else if (pos > 1)
    return string(path_, pos + 1);
  else
    return Path(root_directory);
}

Path Path::dirname() const {
  validate_non_empty_path();  // throws std::invalid_argument
  string::size_type pos = path_.find_last_of(directory_separator);
  if (pos == string::npos)
    return Path(".");
  else if (pos > 0)
    return string(path_, 0, pos);
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
  return type() != FileType::FILE_NOT_FOUND && type() != FileType::STATUS_ERROR;
}

bool Path::is_readable() const {
  validate_non_empty_path();
  return exists() && std::ifstream(real_path().str()).good();
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

std::ostream &operator<<(std::ostream &out, Path::FileType type) {
  static const char *type_names[]{
      "ERROR",        "not found",        "regular", "directory", "symlink",
      "block device", "character device", "FIFO",    "socket",    "UNKNOWN",
  };
  out << type_names[static_cast<int>(type)];
  return out;
}

///////////////////////////////////////////////////////////
// Directory::Iterator members

Directory::DirectoryIterator Directory::begin() {
  return DirectoryIterator(*this);
}

Directory::DirectoryIterator Directory::glob(const string &pattern) {
  return DirectoryIterator(*this, pattern);
}

Directory::DirectoryIterator Directory::end() { return DirectoryIterator(); }

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

int delete_dir_recursive(const std::string &dir) noexcept {
  mysql_harness::Directory d(dir);
  try {
    for (auto const &f : d) {
      if (f.is_directory()) {
        if (delete_dir_recursive(f.str()) < 0) return -1;
      } else {
        if (delete_file(f.str()) < 0) return -1;
      }
    }
  } catch (...) {
    return -1;
  }
  return delete_dir(dir);
}

std::string get_plugin_dir(const std::string &runtime_dir) {
  std::string cur_dir = Path(runtime_dir.c_str()).basename().str();
  if (cur_dir == "runtime_output_directory") {
    // single configuration build
    auto result = Path(runtime_dir.c_str()).dirname();
    return result.join("plugin_output_directory").str();
  } else {
    // multiple configuration build
    // in that case cur_dir has to be configuration name (Debug, Release etc.)
    // we need to go 2 levels up
    auto result = Path(runtime_dir.c_str()).dirname().dirname();
    return result.join("plugin_output_directory").join(cur_dir).str();
  }
}

HARNESS_EXPORT
std::string get_tests_data_dir(const std::string &runtime_dir) {
  std::string cur_dir = Path(runtime_dir.c_str()).basename().str();
  if (cur_dir == "runtime_output_directory") {
    // single configuration build
    auto result = Path(runtime_dir.c_str()).dirname();
    return result.join("router").join("tests").join("data").str();
  } else {
    // multiple configuration build
    // in that case cur_dir has to be configuration name (Debug, Release etc.)
    // we need to go 2 levels up
    auto result = Path(runtime_dir.c_str()).dirname().dirname();
    return result.join("router").join("tests").join("data").join(cur_dir).str();

    return result.str();
  }
}

}  // namespace mysql_harness
