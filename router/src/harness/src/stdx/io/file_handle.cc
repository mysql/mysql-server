/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/stdx/io/file_handle.h"

#include <fcntl.h>     // O_RDONLY and _O_RDONLY
#include <sys/stat.h>  // stat, _stat64 on windows
#include <sys/types.h>
#include <cstdio>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <Windows.h>  // GetLastError
#include <io.h>       // close
#else
#include <unistd.h>  // close
#include <cstdlib>
#endif

#include <array>
#include <random>

#if defined(__FreeBSD__)
#include <sys/sysctl.h>  // CTL_KERN
#include <sys/user.h>

#include <vector>
#elif defined(__APPLE__)
#include <sys/param.h>  // MAXPATHLEN
#endif

#include "mysql/harness/stdx/expected.h"

#ifdef _WIN32
using mode_t = int;
#endif

using namespace std::string_literals;

namespace stdx {
namespace io {

static std::error_code last_posix_error_code() {
  return {errno, std::generic_category()};
}

static std::error_code last_error_code() {
#ifdef _WIN32
  return {static_cast<int>(GetLastError()), std::system_category()};
#else
  return last_posix_error_code();
#endif
}

namespace impl {
stdx::expected<int, std::error_code> open(const char *fname, int flags,
                                          mode_t mode) noexcept {
#ifdef _WIN32
  int fd = ::_open(fname, flags, mode);
#else
  int fd = ::open(fname, flags, mode);
#endif

  if (-1 == fd) {
    // _open() sets errno
    return stdx::unexpected(last_posix_error_code());
  }

  return {fd};
}

stdx::expected<void, std::error_code> close(
    file_handle::native_handle_type handle) noexcept {
#ifdef _WIN32
  if (-1 == ::_close(handle)) {
    // _close() sets errno
    return stdx::unexpected(last_posix_error_code());
  }
#else
  if (-1 == ::close(handle)) {
    return stdx::unexpected(last_error_code());
  }
#endif

  return {};
}

stdx::expected<void, std::error_code> unlink(const char *fn) noexcept {
#ifdef _WIN32
  if (-1 == ::_unlink(fn)) {
    // _unlink() sets errno
    return stdx::unexpected(last_posix_error_code());
  }
#else
  if (-1 == ::unlink(fn)) {
    return stdx::unexpected(last_error_code());
  }
#endif

  return {};
}

#ifdef _WIN32
using stat_type = struct ::_stat64;
#else
using stat_type = struct stat;
#endif

stdx::expected<stat_type, std::error_code> fstat(int handle) noexcept {
  stat_type st;
#ifdef _WIN32
  if (-1 == ::_fstat64(handle, &st)) {
    // _fstat64() sets errno
    return stdx::unexpected(last_posix_error_code());
  }
#else
  if (-1 == ::fstat(handle, &st)) {
    return stdx::unexpected(last_error_code());
  }
#endif

  return {st};
}

stdx::expected<std::size_t, std::error_code> write(
    file_handle::native_handle_type handle, const char *data,
    const std::size_t len) {
#ifdef _WIN32
  const auto bytes_transferred = ::_write(handle, data, len);
#else
  const auto bytes_transferred = ::write(handle, data, len);
#endif
  if (bytes_transferred == -1) return stdx::unexpected(last_error_code());

  return bytes_transferred;
}

}  // namespace impl

stdx::expected<file_handle::path_type, std::error_code>
file_handle::current_path() const noexcept {
  if (handle_ == invalid_handle) {
    return stdx::unexpected(make_error_code(std::errc::bad_file_descriptor));
  }

#if defined(__linux__) || defined(__sun)
  const std::string in =
#if defined(__linux__)
      // /proc/self/fd/<id> is a symlink to the actual file
      "/proc/self/fd/"s
#else
      // /proc/<pid>/path/<id> is a symlink to the actual file
      "/proc/"s + std::to_string(getpid()) + "/path/"s
#endif
      + std::to_string(handle_);

  // the size of the symbolic link is the size of filename it points to
  struct stat st;
  if (0 != lstat(in.c_str(), &st)) {
    return stdx::unexpected(last_error_code());
  }

  std::string path_name;

  // allocate one more byte then actually needed to detect if the size of the
  // symlink increased between lstat() and readlink()
  //
  // 1. sz == st.st_size: same size
  // 2. sz < st.st_size: it shrank in between
  // 3. sz > st.st_size: it grew
  //
  // without the + 1 the "same size" and "it grew" case couldn't be
  // distinguished.
  path_name.resize(st.st_size + 1);

  const ssize_t sz = readlink(in.data(), &path_name.front(), path_name.size());
  if (-1 == sz) {
    return stdx::unexpected(last_error_code());
  }

  if (sz > st.st_size) {
    // between lstat() and readlink() the size of the filename increased
    // all we have is quite likely a truncated filename.
    //
    // Signal interrupted to trigger a retry by the caller.
    return stdx::unexpected(make_error_code(std::errc::interrupted));
  }

  path_name.resize(sz);

  return {path_name};
#elif defined(_WIN32)
  HANDLE win_handle = reinterpret_cast<HANDLE>(_get_osfhandle(handle_));
  if (win_handle == INVALID_HANDLE_VALUE) {
    return stdx::unexpected(make_error_code(std::errc::bad_file_descriptor));
  }
  std::array<char, MAX_PATH + 1> path;
  const auto sz =
      GetFinalPathNameByHandle(win_handle, path.data(), path.size(), 0);
  if (sz == 0) {
    // GetLastError
    return stdx::unexpected(last_error_code());
  } else if (sz > path.size()) {
    return stdx::unexpected(
        std::error_code(ERROR_NOT_ENOUGH_MEMORY, std::system_category()));
  }

  return {std::string{path.data(), std::next(path.data(), sz)}};
#elif defined(__APPLE__)
  std::string path_name;
  path_name.resize(MAXPATHLEN + 1);

  if (-1 == fcntl(handle_, F_GETPATH, &path_name.front())) {
    return stdx::unexpected(last_error_code());
  }

  // as we don't have a length information, check for the \0 char
  const auto term_pos = path_name.find('\0');
  if (term_pos != std::string::npos) path_name.resize(term_pos);

  return {path_name};
#elif defined(__FreeBSD__)
  // see https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=197695
  const std::array<int, 4> mib = {CTL_KERN, KERN_PROC, KERN_PROC_FILEDESC,
                                  getpid()};

  size_t len;
  if (-1 == ::sysctl(mib.data(), mib.size(), nullptr, &len, nullptr, 0)) {
    return stdx::unexpected(last_error_code());
  }

  std::vector<char> buffer(len * 2);
  if (-1 == ::sysctl(mib.data(), mib.size(), buffer.data(), &len, nullptr, 0)) {
    return stdx::unexpected(last_error_code());
  }

  for (const char *p = buffer.data(); p < buffer.data() + len;) {
    const auto *kif = reinterpret_cast<const kinfo_file *>(p);
    if (kif->kf_type == KF_TYPE_VNODE && kif->kf_fd == handle_) {
      return {std::string(kif->kf_path)};
    }
    p += kif->kf_structsize;
  }

  // fd wasn't found
  return stdx::unexpected(make_error_code(std::errc::bad_file_descriptor));
#elif defined(__sun)
  std::string path_name;

  std::string in =
      "/proc/" + std::to_string(getpid()) + "/path/" + std::to_string(handle_);

  path_name.resize(32768 + 1);

  ssize_t sz = ::readlink(in.data(), &path_name.front(), path_name.size());
  if (-1 == sz) {
    return stdx::unexpected(last_error_code());
  }

  path_name.resize(sz);

  return {path_name};
#else
#error unsupported OS
#endif
}

stdx::expected<void, std::error_code> file_handle::unlink() {
  // unlink works on the filenames, but we have a filehandle
  auto res = current_path();
  if (!res) return stdx::unexpected(res.error());

  return impl::unlink(res.value().c_str());
}

stdx::expected<void, std::error_code> file_handle::close() noexcept {
  if (handle_ != invalid_handle) {
    if (flags_ & flag::unlink_on_first_close) {
      unlink();
    }
    auto close_res = impl::close(handle_);

    if (!close_res) {
      return close_res;
    }
    handle_ = invalid_handle;
  }

  return {};
}

stdx::expected<std::size_t, std::error_code> file_handle::write(
    const char *data, const std::size_t len) {
  return impl::write(handle_, data, len);
}

stdx::expected<file_handle, std::error_code> file_handle::file(
    const path_handle &, file_handle::path_view_type path, mode _mode,
    creation _creation, caching _caching, flag flags) noexcept {
  int f{};
  mode_t m{0600};

  switch (_mode) {
    case mode::read:
#ifdef _WIN32
      f = _O_RDONLY;
#else
      f = O_RDONLY;
#endif
      break;
    case mode::write:
#ifdef _WIN32
      f = _O_RDWR;
#else
      f = O_RDWR;
#endif
      break;
    case mode::append:
#ifdef _WIN32
      f = _O_APPEND;
#else
      f = O_APPEND;
#endif
      break;
    case mode::unchanged:
      break;
  }

  switch (_creation) {
    case creation::open_existing:
      break;
    case creation::only_if_not_exist:
      f |= O_CREAT | O_EXCL;
      break;
    case creation::if_needed:
      f |= O_CREAT;
      break;
    case creation::truncate_existing:
      f |= O_TRUNC;
      break;
  }

  switch (_caching) {
    case caching::unchanged:
    case caching::all:
      break;
    case caching::none:
#ifdef O_DIRECT
      f |= O_DIRECT;
#endif
      break;
    case caching::temporary:
      break;
    default:
      return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  // as path_handle is currently an empty class, we can use the path here
  //
  // the APIs intent here is to use openat()/NtCreateFile()'s
  // ObjectAttribute's RootDirectory with a directory handle

  auto open_res = impl::open(path.c_str(), f, m);
  if (!open_res) {
    return stdx::unexpected(open_res.error());
  }

  auto handle = open_res.value();

  auto stat_res = impl::fstat(handle);
  if (!stat_res) {
    return stdx::unexpected(stat_res.error());
  }

  auto st = stat_res.value();

  using ret_type = stdx::expected<file_handle, std::error_code>;

  return ret_type{std::in_place, handle, st.st_dev, st.st_ino, _caching, flags};
}

/**
 * get random hex string.
 *
 * characters of the random string are from the range 0-9 and a-f.
 *
 * @param sz size of the hex-string
 * @returns hex-string of size sz
 */
static std::string random_string(size_t sz) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);

  std::string name;
  name.resize(sz);
  for (size_t ndx{}; ndx < name.size(); ++ndx) {
    auto v = dis(gen);
    name[ndx] = (v > 9) ? v - 10 + 'a' : v + '0';
  }

  return name;
}

stdx::expected<file_handle, std::error_code> file_handle::uniquely_named_file(
    const path_handle &base, mode _mode, caching _caching,
    flag flags) noexcept {
  for (;;) {
    auto name = random_string(32);
    name.append(".random");
    auto res =
        file(base, name, _mode, creation::only_if_not_exist, _caching, flags);
    // if file exists, continue. Otherwise return
    if (res || (res.error() != make_error_code(std::errc::file_exists))) {
      return res;
    }
  }
}

}  // namespace io
}  // namespace stdx
