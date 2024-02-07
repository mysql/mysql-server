/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "tcp_port_pool.h"

#ifndef _WIN32
#include <fcntl.h>     // fcntl
#include <sys/file.h>  // flock
#include <sys/stat.h>  // chmod
#include <unistd.h>    // open, close
#else
#include <windows.h>
#endif

#include <cstring>
#include <memory>  // shared_ptr
#include <system_error>
#include <utility>  // move

#include "mysql/harness/filesystem.h"  // Path
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/utils.h"
#include "router_test_helpers.h"

using mysql_harness::Path;

namespace {
#ifndef _WIN32
std::error_code posix_error_code() { return {errno, std::generic_category()}; }
#else
std::error_code win32_error_code() {
  return {static_cast<int>(GetLastError()), std::generic_category()};
}
#endif

}  // namespace

stdx::expected<void, std::error_code> FileHandle::close() {
  if (fh_ == kInvalidHandle) return {};

#ifndef _WIN32
  int res = ::close(fh_);
  if (res != 0) return stdx::unexpected(posix_error_code());
#else
  BOOL res = ::CloseHandle(fh_);
  if (res == 0) return stdx::unexpected(win32_error_code());
#endif

  fh_ = kInvalidHandle;

  return {};
}

#ifndef _WIN32
stdx::expected<FileHandle, std::error_code> FileHandle::open(
    const std::string &filename, int opts, int mode) {
  int res = ::open(filename.c_str(), opts, mode);
  if (res < 0) return stdx::unexpected(posix_error_code());

  return FileHandle(res);
}
#else
stdx::expected<FileHandle, std::error_code> FileHandle::open(
    const std::string &file_name, DWORD desired_access, DWORD share_mode,
    SECURITY_ATTRIBUTES *security_attributes, DWORD creation_disposition,
    DWORD flags_and_attributes, HANDLE template_file) {
  HANDLE res = ::CreateFile(file_name.c_str(), desired_access, share_mode,
                            security_attributes, creation_disposition,
                            flags_and_attributes, template_file);

  if (res == kInvalidHandle) return stdx::unexpected(win32_error_code());

  return FileHandle(res);
}
#endif

#ifndef _WIN32
class LockedFileHandle {
 public:
  static stdx::expected<void, std::error_code> lock(
      FileHandle::native_handle_type fd);
};

stdx::expected<void, std::error_code> LockedFileHandle::lock(
    FileHandle::native_handle_type fd) {
#ifdef __sun
  // fcntl locks aren't inherited to other processes.
  struct flock fl {};

  fl.l_start = 0;
  fl.l_len = 0;
  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;

  int lock = fcntl(fd, F_SETLK, &fl);
#else
  // don't pass the lock-fd to the child processes
  {
    int flag = ::fcntl(fd, F_GETFD);
    if ((flag & FD_CLOEXEC) != 0) {
      flag |= FD_CLOEXEC;
      ::fcntl(fd, F_SETFD, &flag);
    }
  }

  int lock = flock(fd, LOCK_EX | LOCK_NB);
#endif
  if (lock != 0) return stdx::unexpected(posix_error_code());

  return {};
}
#endif

stdx::expected<void, std::error_code> UniqueId::lock_file(
    const std::string &file_name) {
#ifndef _WIN32
  auto open_res =
      FileHandle::open(file_name, O_RDWR | O_CREAT | O_CLOEXEC, 0666);
  if (!open_res) return stdx::unexpected(open_res.error());

  lock_file_fd_ = std::move(*open_res);

  {
    // open() honours umask and we want to make sure this directory is
    // accessible for every user regardless of umask settings
    ::chmod(file_name.c_str(), 0666);
  }

  auto lock_res = LockedFileHandle::lock(lock_file_fd_.native_handle());
  if (!lock_res) {
    lock_file_fd_.close();

    return stdx::unexpected(lock_res.error());
  }
#else
  auto open_res =
      FileHandle::open(file_name,
                       GENERIC_READ,  // read is enough
                       0,  // prevent other processes from opening the file.
                       nullptr,                    // no extra security
                       OPEN_ALWAYS,                // if it exists, ok
                       FILE_FLAG_DELETE_ON_CLOSE,  // delete the file on close
                       nullptr);
  if (!open_res) return stdx::unexpected(open_res.error());

  lock_file_fd_ = std::move(*open_res);
#endif

  // obtained the lock
  return {};
}

std::string UniqueId::get_lock_file_dir() {
  // this is what MTR uses, see mysql-test/lib/mtr_unique.pm for details
#ifndef _WIN32
  return "/tmp/mysql-unique-ids";
#else
  // this are env variables that MTR uses, see mysql-test/lib/mtr_unique.pm for
  // details
  DWORD buff_size = 65535;

  std::string buffer;
  buffer.resize(buff_size);

  buff_size =
      GetEnvironmentVariableA("ALLUSERSPROFILE", buffer.data(), buffer.size());
  if (buff_size == 0) {
    buff_size = GetEnvironmentVariableA("TEMP", buffer.data(), buffer.size());
  }

  if (buff_size == 0) {
    throw std::runtime_error("Could not get directory for lock files.");
  }

  buffer.resize(buff_size);

  buffer.append("\\mysql-unique-ids");
  return buffer;
#endif
}

UniqueId::UniqueId(value_type start_from, value_type range)
    : proc_ids_(process_unique_ids()) {
  const std::string lock_file_dir = get_lock_file_dir();
  mysql_harness::mkdir(lock_file_dir, 0777);
#ifndef _WIN32
  // mkdir honours umask and we want to make sure this directory is accessible
  // for every user regardless of umask settings
  ::chmod(lock_file_dir.c_str(), 0777);
#endif

  for (auto i = start_from; i < start_from + range; i++) {
    if (proc_ids_->contains(i)) continue;

    Path lock_file_path(lock_file_dir);
    lock_file_path.append(std::to_string(i));

    auto lock_res = lock_file(lock_file_path.str());
    if (lock_res) {
      id_ = i;

      proc_ids_->insert(i);

      // obtained the lock, we are good to go
      return;
    }
  }

  throw std::runtime_error("Could not get unique id from the given range");
}

UniqueId::~UniqueId() {
  // release the process unique-id if we own one.
  if (id_) {
    proc_ids_->erase(*id_);
  }
}

// process-wide unique-ids
//
// is a "static shared_ptr<>" instead of a "static" as the TcpPort may be part
// of a "static" too.
//
// It would create:
// 1. (static) TcpPortPool
// 2. static ProcessUniqueIds
//
// ... and then at destruct in reverse order:
// * ProcessUniqueIds
// * TcpPortPool ... but the TcpPortPool would try to remove itself from the
//   ProcessUniqueIds
//
//
//
std::shared_ptr<UniqueId::ProcessUniqueIds> UniqueId::process_unique_ids() {
  static std::shared_ptr<UniqueId::ProcessUniqueIds> ids =
      std::make_shared<UniqueId::ProcessUniqueIds>();

  return ids;
}

uint16_t TcpPortPool::get_next_available() {
  net::io_context io_ctx;

  constexpr const auto bind_address = net::ip::address_v4::loopback();

  while (true) {
    if (unique_ids_.empty()) {
      unique_ids_.emplace_back(kPortsStartFrom, kPortsRange);
    } else if (number_of_ids_used_ % kPortsPerFile == 0) {
      // need another lock file
      number_of_ids_used_ = 0;

      auto last_id = unique_ids_.back().id().value();
      unique_ids_.emplace_back(last_id + 1, kPortsRange);
    }

    // this is the formula that mysql-test also uses to map lock filename to
    // actual port number, they currently start from 13000 though

    uint16_t port = 10000 + unique_ids_.back().id().value() * kPortsPerFile +
                    number_of_ids_used_++;

    if (is_port_bindable(io_ctx, {bind_address, port})) return port;
  }
}
