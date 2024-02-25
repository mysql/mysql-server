/*
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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_NET_TS_IMPL_FILE_H_
#define MYSQL_HARNESS_NET_TS_IMPL_FILE_H_

#include <array>
#include <system_error>

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "mysql/harness/stdx/expected.h"

namespace net {
namespace impl {
namespace file {

#ifdef _WIN32
using file_handle_type = HANDLE;

// can't be constexpr as INVALID_HANDLE_VALUE is a old-style-cast from -1 to
// pointer
const file_handle_type kInvalidHandle{INVALID_HANDLE_VALUE};
#else
using file_handle_type = int;
constexpr file_handle_type kInvalidHandle{-1};
#endif

inline std::error_code last_error_code() {
#ifdef _WIN32
  return {static_cast<int>(GetLastError()), std::system_category()};
#else
  return {errno, std::generic_category()};
#endif
}

template <int Name, class Arg>
class file_control_option {
 public:
  using value_type = Arg;

  file_control_option(value_type v) : v_{v} {}

  constexpr int name() const { return Name; }

  constexpr value_type value() const { return v_; }

 private:
  value_type v_;
};

template <int Name>
class file_control_option<Name, void> {
 public:
  using arg_type = void;

  file_control_option() = default;

  constexpr int name() const { return Name; }

  constexpr int value() const { return 0; }
};

#ifndef _WIN32
// windows has "File Attribute Constants" (FILE_ATTRIBUTE_...) which
// can be accessed via GetFileAttributes() and GetFileAttributesEx()
// but there is no direct, good mapping to fcntl()

// posix
using dup_fd = file_control_option<F_DUPFD, int>;
using dup_fd_cloexec = file_control_option<F_DUPFD_CLOEXEC, int>;

// posix
//
// - FD_CLOEXEC
using get_file_flags = file_control_option<F_GETFD, void>;
using set_file_flags = file_control_option<F_SETFD, int>;

// posix
//
// - O_DIRECT
// - O_NONBLOCK
using get_file_status = file_control_option<F_GETFL, void>;
using set_file_status = file_control_option<F_SETFL, int>;

// posix
using get_own = file_control_option<F_GETOWN, void>;
using set_own = file_control_option<F_SETOWN, int>;

#ifdef F_GETPIPE_SZ
// linux
using get_pipe_size = file_control_option<F_GETPIPE_SZ, void>;
using set_pipe_size = file_control_option<F_SETPIPE_SZ, int>;
#endif

template <class FileControlOption>
inline stdx::expected<int, std::error_code> fcntl(
    file_handle_type fd, const FileControlOption &cmd) {
  int res;
  if (-1 == (res = ::fcntl(fd, cmd.name(), cmd.value()))) {
    return stdx::make_unexpected(last_error_code());
  }

  return {res};
}
#endif

/**
 * create pipe.
 *
 * @param flags flags passed to pipe2() like O_NONBLOCK or O_CLOEXEC
 * @return pair file-handles or std::error_code
 */
inline stdx::expected<std::pair<file_handle_type, file_handle_type>,
                      std::error_code>
pipe(int flags = 0) {
  std::array<file_handle_type, 2> fds{};
#ifdef _WIN32
  if (flags != 0) {
    // on windows we can't set the flags
    //
    // PIPE_WAIT only exists for named-pipes
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }
  if (0 == ::CreatePipe(&fds[0], &fds[1], nullptr, 0)) {
    return stdx::make_unexpected(last_error_code());
  }
#elif defined(__linux__) || defined(__FreeBSD__)
  // pipe2() exists
  // FreeBSD 10.0
  // Linux 2.6.27
  if (0 != ::pipe2(fds.data(), flags)) {
    return stdx::make_unexpected(last_error_code());
  }
#else
  if (0 != ::pipe(fds.data())) {
    return stdx::make_unexpected(last_error_code());
  }

  set_file_status fl(flags);
  auto fcntl_res = fcntl(fds[0], fl);
  if (!fcntl_res) {
    close(fds[0]);
    close(fds[1]);

    return stdx::make_unexpected(fcntl_res.error());
  }

  fcntl_res = fcntl(fds[1], fl);
  if (!fcntl_res) {
    close(fds[0]);
    close(fds[1]);

    return stdx::make_unexpected(fcntl_res.error());
  }
#endif

  return std::make_pair(fds[0], fds[1]);
}

/**
 * write a buffer to a file handle.
 *
 * calls write() on POSIX, WriteFile() on win32
 */
inline stdx::expected<size_t, std::error_code> write(file_handle_type handle,
                                                     const void *buf,
                                                     size_t buf_len) {
#if defined(_WIN32)
  DWORD transfered{0};
  if (0 == ::WriteFile(handle, buf, buf_len, &transfered, nullptr)) {
    return stdx::make_unexpected(last_error_code());
  }
#else
  ssize_t transfered = ::write(handle, buf, buf_len);
  if (-1 == transfered) {
    return stdx::make_unexpected(last_error_code());
  }
#endif

  return transfered;
}

/**
 * read from file handle into a buffer.
 *
 * calls read() on POSIX, ReadFile() on win32
 */
inline stdx::expected<size_t, std::error_code> read(file_handle_type handle,
                                                    void *buf, size_t buf_len) {
#if defined(_WIN32)
  DWORD transfered{0};
  if (0 == ::ReadFile(handle, buf, buf_len, &transfered, nullptr)) {
    return stdx::make_unexpected(last_error_code());
  }
#else
  ssize_t transfered = ::read(handle, buf, buf_len);
  if (-1 == transfered) {
    return stdx::make_unexpected(last_error_code());
  }
#endif

  return transfered;
}

/**
 * close file handle.
 *
 * calls close() on POSIX, CloseHandle() on win32
 */
inline stdx::expected<void, std::error_code> close(
    file_handle_type native_handle) {
#ifdef _WIN32
  if (0 == ::CloseHandle(native_handle))
#else
  if (0 != ::close(native_handle))
#endif
  {
    return stdx::make_unexpected(last_error_code());
  }
  return {};
}  // namespace file

}  // namespace file
}  // namespace impl
}  // namespace net

#endif
