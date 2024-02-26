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

// On OSX, this causes __DARWIN_C_LEVEL to be upgraded to __DARWIN_C_FULL in
// sys/cdefs.h, which in turn enables non-POSIX extensions such as mkdtemp().
// Needs to be set before sys/cdefs.h gets #included (from any other headers),
// thus best left here before any #includes.
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif

#include "mysql/harness/filesystem.h"

#include <cassert>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include <dirent.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "mysql/harness/access_rights.h"
#include "mysql/harness/stdx/expected.h"

namespace {
const std::string dirsep("/");
const std::string extsep(".");
}  // namespace

namespace mysql_harness {

const perm_mode kStrictDirectoryPerm = S_IRWXU;

////////////////////////////////////////////////////////////////
// class Path members and free functions

const char *const Path::directory_separator = "/";
const char *const Path::root_directory = "/";

Path::FileType Path::type(bool refresh) const {
  validate_non_empty_path();
  if (type_ == FileType::TYPE_UNKNOWN || refresh) {
    struct stat stat_buf;
    if (stat(c_str(), &stat_buf) == -1) {
      if (errno == ENOENT || errno == ENOTDIR)
        type_ = FileType::FILE_NOT_FOUND;
      else
        type_ = FileType::STATUS_ERROR;
    } else {
      switch (stat_buf.st_mode & S_IFMT) {
        case S_IFDIR:
          type_ = FileType::DIRECTORY_FILE;
          break;
        case S_IFBLK:
          type_ = FileType::BLOCK_FILE;
          break;
        case S_IFCHR:
          type_ = FileType::CHARACTER_FILE;
          break;
        case S_IFIFO:
          type_ = FileType::FIFO_FILE;
          break;
        case S_IFLNK:
          type_ = FileType::SYMLINK_FILE;
          break;
        case S_IFREG:
          type_ = FileType::REGULAR_FILE;
          break;
        case S_IFSOCK:
          type_ = FileType::SOCKET_FILE;
          break;
        default:
          type_ = FileType::TYPE_UNKNOWN;
          break;
      }
    }
  }
  return type_;
}

bool Path::is_absolute() const {
  validate_non_empty_path();  // throws std::invalid_argument
  if (path_[0] == '/') return true;
  return false;
}

bool Path::is_readable() const {
  validate_non_empty_path();
  return exists() && std::ifstream(real_path().str()).good();
}

////////////////////////////////////////////////////////////////
// Directory::DirectoryIterator

class Directory::DirectoryIterator::State {
 public:
  State();
  State(const Path &path,
        const std::string &pattern);  // throws std::system_error
  ~State();

  bool eof() const { return result_ == nullptr; }

  void fill_result();  // throws std::system_error

  template <typename IteratorType>
  static bool equal(const IteratorType &lhs, const IteratorType &rhs) {
    assert(lhs != nullptr && rhs != nullptr);

    // If either one is null (end iterators), they are equal if both
    // are end iterators.
    if (lhs->result_ == nullptr || rhs->result_ == nullptr)
      return lhs->result_ == rhs->result_;

    // Otherwise they are not equal (this is an input iterator and we
    // should not compare entries received through different
    // iterations.
    return false;
  }

  DIR *dirp_;

  struct free_dealloc {
    void operator()(void *p) {
      if (p) free(p);
    }
  };

  std::unique_ptr<dirent, free_dealloc> entry_;
  const std::string pattern_;
  struct dirent *result_;
};

Directory::DirectoryIterator::State::State()
    : dirp_(nullptr), pattern_(""), result_(nullptr) {}

// throws std::system_error
Directory::DirectoryIterator::State::State(const Path &path,
                                           const std::string &pattern)
    : dirp_(opendir(path.c_str())), pattern_(pattern) {
  // dirent can be NOT large enough to hold a directory name, so we need to
  // ensure there's extra space for it. From the "man readdir_r":
  // "Since POSIX.1 does not specify the size of the d_name field, and other
  // nonstandard fields may
  //  precede that field within the dirent structure, portable  applications
  //  that use readdir_r() should allocate the buffer whose address is passed in
  //  entry as follows:
  //    name_max = pathconf(dirpath, _PC_NAME_MAX);
  //    if (name_max == -1)         /* Limit not defined, or error */
  //        name_max = 255;         /* Take a guess */
  //    len = offsetof(struct dirent, d_name) + name_max + 1;
  //    entryp = malloc(len);
  //  (POSIX.1 requires that d_name is the last field in a struct dirent.)"
  size_t alloc_size = sizeof(struct dirent) +
                      (size_t)pathconf(path.str().c_str(), _PC_NAME_MAX) + 1;

  // We need RAII here as we throw an exception in the constructor which means
  // we can't rely on the destructor always being called
  entry_.reset((struct dirent *)malloc(alloc_size));
  result_ = entry_.get();

  if (dirp_ == nullptr) {
    std::ostringstream msg;
    msg << "Failed to open directory '" << path << "'";
    throw std::system_error(errno, std::system_category(), msg.str());
  }

  fill_result();  // throws std::system_error
}

Directory::DirectoryIterator::State::~State() {
  // There is no guarantee that calling closedir() with NULL will
  // work. For example, BSD systems do not always support this.
  if (dirp_ != nullptr) closedir(dirp_);
}

// throws std::system_error
void Directory::DirectoryIterator::State::fill_result() {
  // This is similar to scandir(2), but we do not use scandir(2) since
  // we want to be thread-safe.

  // If we have reached the end, filling do not have any effect.
  if (result_ == nullptr) return;

  while (true) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    // new glibc 2.24-and-later don't like readdir_r(), and deprecate it in
    // favor of readdir(). However, readdir() is not thread-safe according to
    // POSIX.1:2008 yet.
    int error = readdir_r(dirp_, entry_.get(), &result_);
#pragma GCC diagnostic pop

    if (error) {
      throw std::system_error(errno, std::system_category(),
                              "Failed to read directory entry");
    }

    // If there are no more entries, we're done.
    if (result_ == nullptr) break;

    // Skip current directory and parent directory.
    if (strcmp(result_->d_name, ".") == 0 || strcmp(result_->d_name, "..") == 0)
      continue;

    // If no pattern is given, we're done.
    if (pattern_.size() == 0) break;

    // Skip any entries that do not match the pattern
    error = fnmatch(pattern_.c_str(), result_->d_name, FNM_PATHNAME);
    if (error == FNM_NOMATCH) {
      continue;
    } else if (error == 0) {
      break;
    } else {
      std::ostringstream msg;
      msg << "Matching name pattern '" << pattern_.c_str()
          << "' against directory entry '" << result_->d_name << "' failed";
      throw std::system_error(errno, std::system_category(), msg.str());
    }
  }
}

////////////////////////////////////////////////////////////////
// Directory::DirectoryIterator

// These definition of the default constructor and destructor need to
// be here since the automatically generated default
// constructor/destructor uses the definition of the class 'State',
// which is not available when the header file is read.
Directory::DirectoryIterator::~DirectoryIterator() = default;
Directory::DirectoryIterator::DirectoryIterator(DirectoryIterator &&) = default;
Directory::DirectoryIterator::DirectoryIterator(const DirectoryIterator &) =
    default;

Directory::DirectoryIterator::DirectoryIterator()
    : path_("*END*"), state_(std::make_shared<State>()) {}

Directory::DirectoryIterator::DirectoryIterator(const Path &path,
                                                const std::string &pattern)
    : path_(path), state_(std::make_shared<State>(path, pattern)) {}

Directory::DirectoryIterator &Directory::DirectoryIterator::operator++() {
  assert(state_ != nullptr);
  state_->fill_result();  // throws std::system_error
  return *this;
}

Path Directory::DirectoryIterator::operator*() const {
  assert(state_ != nullptr && state_->result_ != nullptr);
  return path_.join(state_->result_->d_name);
}

bool Directory::DirectoryIterator::operator!=(
    const Directory::DirectoryIterator &rhs) const {
  return !State::equal(state_, rhs.state_);
}

Path Path::make_path(const Path &dir, const std::string &base,
                     const std::string &ext) {
  return dir.join(base + extsep + ext);
}

Path Path::real_path() const {
  validate_non_empty_path();
  char buf[PATH_MAX];
  if (realpath(c_str(), buf))
    return Path(buf);
  else
    return Path();
}

////////////////////////////////////////////////////////////////////////////////
//
// Utility free functions
//
////////////////////////////////////////////////////////////////////////////////

stdx::expected<void, std::error_code> delete_dir(
    const std::string &dir) noexcept {
  if (::rmdir(dir.c_str()) != 0) {
    return stdx::make_unexpected(
        std::error_code(errno, std::generic_category()));
  }

  return {};
}

stdx::expected<void, std::error_code> delete_file(
    const std::string &path) noexcept {
  if (::unlink(path.c_str()) != 0) {
    return stdx::make_unexpected(
        std::error_code(errno, std::generic_category()));
  }

  return {};
}

std::string get_tmp_dir(const std::string &name) {
  const size_t MAX_LEN = 256;
  const std::string pattern_str = std::string("/tmp/" + name + "-XXXXXX");
  const char *pattern = pattern_str.c_str();
  if (strlen(pattern) >= MAX_LEN) {
    throw std::runtime_error(
        "Could not create temporary directory, name too long");
  }
  char buf[MAX_LEN];
  strncpy(buf, pattern, sizeof(buf) - 1);
  const char *res = mkdtemp(buf);
  if (res == nullptr) {
    throw std::system_error(errno, std::generic_category(),
                            "mkdtemp(" + pattern_str + ") failed");
  }

  return std::string(res);
}

int mkdir_wrapper(const std::string &dir, perm_mode mode) {
  auto res = ::mkdir(dir.c_str(), mode);
  if (res != 0) return errno;
  return 0;
}

void make_file_public(const std::string &file_name) {
  const auto set_res =
      access_rights_set(file_name, S_IRWXU | S_IRWXG | S_IRWXO);
  if (!set_res) {
    const auto ec = set_res.error();
    throw std::system_error(ec, "chmod() failed: " + file_name);
  }
}

void make_file_private(const std::string &file_name,
                       const bool read_only_for_local_service
                       [[maybe_unused]]) {
  const auto set_res = access_rights_set(file_name, S_IRUSR | S_IWUSR);
  if (!set_res) {
    const auto ec = set_res.error();
    throw std::system_error(
        ec, "Could not set permissions for file '" + file_name + "'");
  }
}

void make_file_readonly(const std::string &file_name) {
  const auto set_res =
      access_rights_set(file_name, (S_IRUSR | S_IXUSR) | (S_IRGRP | S_IXGRP) |
                                       (S_IROTH | S_IXOTH));
  if (!set_res) {
    const auto ec = set_res.error();
    throw std::system_error(ec, "chmod() failed: " + file_name);
  }
}

}  // namespace mysql_harness
