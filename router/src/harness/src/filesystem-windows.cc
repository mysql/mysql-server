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

#include <direct.h>
#include <cassert>
#include <cerrno>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

#include <shlwapi.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>

using std::ostringstream;
using std::string;

namespace {
const std::string extsep(".");
}  // namespace

namespace mysql_harness {

////////////////////////////////////////////////////////////////
// class Path members and free functions

// We normalize the Path class to use / internally, to avoid problems
// with code that assume \ to be an escape character
const char *const Path::directory_separator = "/";
const char *const Path::root_directory = "/";

Path::FileType Path::type(bool refresh) const {
  validate_non_empty_path();
  if (type_ == FileType::TYPE_UNKNOWN || refresh) {
    struct _stat stat_buf;
    if (_stat(c_str(), &stat_buf) == -1) {
      if (errno == ENOENT) {
        // Special case, a drive name like "C:"
        if (path_[path_.size() - 1] == ':') {
          DWORD flags = GetFileAttributesA(path_.c_str());
          // API reports it as directory if it exist
          if (flags & FILE_ATTRIBUTE_DIRECTORY) {
            type_ = FileType::DIRECTORY_FILE;
            return type_;
          }
        }
        type_ = FileType::FILE_NOT_FOUND;
      } else if (errno == EINVAL)
        type_ = FileType::STATUS_ERROR;
    } else {
      switch (stat_buf.st_mode & S_IFMT) {
        case S_IFDIR:
          type_ = FileType::DIRECTORY_FILE;
          break;
        case S_IFCHR:
          type_ = FileType::CHARACTER_FILE;
          break;
        case S_IFREG:
          type_ = FileType::REGULAR_FILE;
          break;
        default:
          type_ = FileType::TYPE_UNKNOWN;
          break;
      }
    }
  }
  return type_;
}

////////////////////////////////////////////////////////////////
// Directory::Iterator::State

class Directory::DirectoryIterator::State {
 public:
  State();
  State(const Path &path, const string &pattern);  // throws std::system_error
  ~State();

  void fill_result(bool first_entry_set = false);  // throws std::system_error

  template <typename IteratorType>
  static bool equal(const IteratorType &lhs, const IteratorType &rhs) {
    assert(lhs != nullptr && rhs != nullptr);

    // If either interator is an end iterator, they are equal if both
    // are end iterators.
    if (!lhs->more_ || !rhs->more_) return lhs->more_ == rhs->more_;

    // Otherwise, they are not equal (since we are using input
    // iterators, they do not compare equal in any other cases).
    return false;
  }

  WIN32_FIND_DATA data_;
  HANDLE handle_;
  bool more_;
  const string pattern_;

 private:
  static const char *dot;
  static const char *dotdot;
};

const char *Directory::DirectoryIterator::State::dot = ".";
const char *Directory::DirectoryIterator::State::dotdot = "..";

Directory::DirectoryIterator::State::State()
    : handle_(INVALID_HANDLE_VALUE), more_(false), pattern_("") {}

// throws std::system_error
Directory::DirectoryIterator::State::State(const Path &path,
                                           const string &pattern)
    : handle_(INVALID_HANDLE_VALUE), more_(true), pattern_(pattern) {
  const Path r_path = path.real_path();
  const string pat = r_path.join(pattern.size() > 0 ? pattern : "*").str();

  if (pat.size() > MAX_PATH) {
    ostringstream msg;
    msg << "Failed to open path '" << path << "'";
    throw std::system_error(std::make_error_code(std::errc::filename_too_long),
                            msg.str());
  }

  handle_ = FindFirstFile(pat.c_str(), &data_);
  if (handle_ == INVALID_HANDLE_VALUE)
    throw std::system_error(GetLastError(), std::system_category(),
                            "Failed to read first directory entry");

  fill_result(true);  // throws std::system_error
}

Directory::DirectoryIterator::State::~State() {
  if (handle_ != INVALID_HANDLE_VALUE) FindClose(handle_);
}

// throws std::system_error
void Directory::DirectoryIterator::State::fill_result(
    bool first_entry_set /*= false*/) {
  assert(handle_ != INVALID_HANDLE_VALUE);
  while (true) {
    if (first_entry_set) {
      more_ = true;
      first_entry_set = false;
    } else {
      more_ = (FindNextFile(handle_, &data_) != 0);
    }

    if (!more_) {
      int error = GetLastError();
      if (error != ERROR_NO_MORE_FILES) {
        throw std::system_error(GetLastError(), std::system_category(),
                                "Failed to read directory entry");
      } else {
        break;
      }
    } else {
      // Skip current directory and parent directory.
      if (!strcmp(data_.cFileName, dot) || !strcmp(data_.cFileName, dotdot))
        continue;

      // If no pattern is given, we're done.
      if (pattern_.size() == 0) break;

      // Skip any entries that do not match the pattern
      BOOL result = PathMatchSpecA(data_.cFileName, pattern_.c_str());
      if (!result)
        continue;
      else
        break;
    }
  }
}

////////////////////////////////////////////////////////////////
// Directory::Iterator

// These definition of the default constructor and destructor need to
// be here since the automatically generated default
// constructor/destructor uses the definition of the class 'State',
// which is not available when the header file is read.
#if !defined(_MSC_VER) || (_MSC_VER >= 1900)
Directory::DirectoryIterator::~DirectoryIterator() = default;
Directory::DirectoryIterator::DirectoryIterator(DirectoryIterator &&) = default;
Directory::DirectoryIterator::DirectoryIterator(const DirectoryIterator &) =
    default;
#elif defined(_MSC_VER)
Directory::DirectoryIterator::~DirectoryIterator() { state_.reset(); }
#endif

Directory::DirectoryIterator::DirectoryIterator()
    : path_("*END*"), state_(std::make_shared<State>()) {}

Directory::DirectoryIterator::DirectoryIterator(const Path &path,
                                                const std::string &pattern)
    : path_(path.real_path()), state_(std::make_shared<State>(path, pattern)) {}

Directory::DirectoryIterator &Directory::DirectoryIterator::operator++() {
  assert(state_ != nullptr);
  state_->fill_result();  // throws std::system_error
  return *this;
}

Path Directory::DirectoryIterator::operator*() const {
  assert(state_ != nullptr && state_->handle_ != INVALID_HANDLE_VALUE);
  return path_.join(state_->data_.cFileName);
}

bool Directory::DirectoryIterator::operator!=(
    const DirectoryIterator &rhs) const {
  return !State::equal(state_, rhs.state_);
}

Path Path::make_path(const Path &dir, const std::string &base,
                     const std::string &ext) {
  return dir.join(base + extsep + ext);
}

Path Path::real_path() const {
  validate_non_empty_path();

  // store a copy of str() in native_path
  assert(0 < str().size() && str().size() < MAX_PATH);
  char native_path[MAX_PATH];
  std::memcpy(native_path, c_str(),
              str().size() + 1);  // +1 for null terminator

  // replace all '/' with '\'
  char *p = native_path;
  while (*p) {
    if (*p == '/') {
      *p = '\\';
    }
    p++;
  }

  // resolve absolute path
  char path[MAX_PATH];
  if (GetFullPathNameA(native_path, sizeof(path), path, nullptr) == 0) {
    return Path();
  }

  // check if the path exists, to match posix behaviour
  WIN32_FIND_DATA find_data;
  HANDLE h = FindFirstFile(path, &find_data);
  if (h == INVALID_HANDLE_VALUE) {
    auto error = GetLastError();
    // If we got ERROR_ACCESS_DENIED here that does not necessarily mean
    // that the path does not exist. We still can have the access to the
    // file itself but we can't call the Find on the directory that contains
    // the file. (This is true for example when the config file is placed in
    // the User's directory and it is accesseed by the router that is run
    // as a Windows service.)
    // In that case we do not treat that as an error.
    if (error != ERROR_ACCESS_DENIED) {
      return Path();
    }
  } else {
    FindClose(h);
  }

  return Path(path);
}

////////////////////////////////////////////////////////////////////////////////
//
// Utility free functions
//
////////////////////////////////////////////////////////////////////////////////

int delete_dir(const std::string &dir) noexcept { return _rmdir(dir.c_str()); }

int delete_file(const std::string &path) noexcept {
  // In Windows a file recently closed may fail to be deleted because its
  // still be locked (or have a 3rd party reading it, like an Indexer service
  // or AntiVirus). So the recommended is to retry the delete operation.
  BOOL flag = TRUE;
  int max_attempts = 10;
  while (max_attempts--) {
    flag = DeleteFile(path.c_str());
    DWORD err = GetLastError();
    if (flag)
      break;
    else if (err == ERROR_FILE_NOT_FOUND) {
      flag = 1;
      break;
    } else if (err == ERROR_ACCESS_DENIED) {
      Sleep(100);
      continue;
    } else {
      return -1;
    }
  }

  return flag ? 0 : -1;
}

std::string get_tmp_dir(const std::string &name) {
  char buf[MAX_PATH];
  auto res = GetTempPath(MAX_PATH, buf);
  if (res == 0 || res > MAX_PATH) {
    throw std::runtime_error("Could not get temporary directory");
  }

  auto generate_random_sequence = [](size_t len) -> std::string {
    std::random_device rd;
    std::string result;
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<unsigned long> dist(0, sizeof(alphabet) - 2);

    for (size_t i = 0; i < len; ++i) {
      result += alphabet[dist(rd)];
    }

    return result;
  };

  std::string dir_name = name + "-" + generate_random_sequence(10);
  std::string result = Path(buf).join(dir_name).str();
  int err = _mkdir(result.c_str());
  if (err != 0) {
    throw std::runtime_error("Error creating temporary directory " + result);
  }
  return result;
}

SecurityDescriptorPtr get_security_descriptor(const std::string &file_name) {
  static constexpr SECURITY_INFORMATION kReqInfo = DACL_SECURITY_INFORMATION;

  // Get the size of the descriptor.
  DWORD sec_desc_size;

  if (GetFileSecurityA(file_name.c_str(), kReqInfo, nullptr, 0,
                       &sec_desc_size) == FALSE) {
    // calling code checks for errno
    // also multiple calls to GetLastError() erase error value
    errno = GetLastError();

    // We expect to receive `ERROR_INSUFFICIENT_BUFFER`.
    if (errno != ERROR_INSUFFICIENT_BUFFER) {
      throw std::system_error(errno, std::system_category(),
                              "GetFileSecurity() failed (" + file_name +
                                  "): " + std::to_string(errno));
    }
  }

  SecurityDescriptorPtr sec_desc(
      static_cast<SECURITY_DESCRIPTOR *>(std::malloc(sec_desc_size)));

  if (GetFileSecurityA(file_name.c_str(), kReqInfo, sec_desc.get(),
                       sec_desc_size, &sec_desc_size) == FALSE) {
    errno = GetLastError();
    throw std::system_error(errno, std::system_category(),
                            "GetFileSecurity() failed (" + file_name +
                                "): " + std::to_string(GetLastError()));
  }

  return sec_desc;
}

int mkdir_wrapper(const std::string &dir, perm_mode mode) {
  auto res = _mkdir(dir.c_str());
  if (res != 0) return errno;
  return 0;
}

}  // namespace mysql_harness
