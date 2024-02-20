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

#ifndef _TCP_PORT_POOL_H_
#define _TCP_PORT_POOL_H_

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>  // exchange
#include <vector>

#ifdef _WIN32
#include <Windows.h>  // HANDLE
#endif

#include "mysql/harness/stdx/expected.h"

class FileHandle {
 public:
#ifndef _WIN32
  using native_handle_type = int;
  static constexpr const native_handle_type kInvalidHandle{-1};
#else
  using native_handle_type = HANDLE;
  inline static const native_handle_type kInvalidHandle{INVALID_HANDLE_VALUE};
#endif

  FileHandle() = default;
  explicit FileHandle(native_handle_type fh) : fh_(fh) {}

  FileHandle(const FileHandle &) = delete;
  FileHandle &operator=(const FileHandle &) = delete;

  FileHandle(FileHandle &&other) noexcept
      : fh_{std::exchange(other.fh_, kInvalidHandle)} {}
  FileHandle &operator=(FileHandle &&other) noexcept {
    fh_ = std::exchange(other.fh_, kInvalidHandle);

    return *this;
  }

  native_handle_type native_handle() const noexcept { return fh_; }

  ~FileHandle() { close(); }

#ifndef _WIN32
  static stdx::expected<FileHandle, std::error_code> open(
      const std::string &filename, int opts, int mode);
#else
  static stdx::expected<FileHandle, std::error_code> open(
      const std::string &file_name, DWORD desired_access, DWORD share_mode,
      SECURITY_ATTRIBUTES *security_attributes, DWORD creation_disposition,
      DWORD flags_and_attributes, HANDLE template_file);
#endif

  stdx::expected<void, std::error_code> close();

 private:
  native_handle_type fh_{kInvalidHandle};
};

/**
 * system-level unique identifier.
 *
 * Compatible with mysql-test MTR, see mysql-test/lib/mtr_unique.pm for details
 *
 **/
class UniqueId {
 public:
  using value_type = unsigned;

  // UniqueIds of this process.
  //
  // unique-ids are implemented on top of file-locking of
  // /tmp/mysql-unique-ids/{num} (on Unix).
  //
  // file-locking via solaris fcntl() is only exclusive between processes,
  // but not within the same process. Therefore, this class keeps all the active
  // unique-ids of the current process.
  //
  class ProcessUniqueIds {
   public:
    using value_type = UniqueId::value_type;

    void insert(value_type id) { id_.push_back(id); }
    bool contains(value_type id) {
      return std::find(std::begin(id_), std::end(id_), id) != std::end(id_);
    }
    size_t erase(value_type id) { return std::erase(id_, id); }

   private:
    std::vector<value_type> id_;
  };

  UniqueId(value_type start_from, value_type range);

  UniqueId(const UniqueId &) = delete;
  UniqueId &operator=(const UniqueId &) = delete;
  UniqueId(UniqueId &&other) noexcept
      : proc_ids_(std::move(other.proc_ids_)),
        id_(std::exchange(other.id_, {})),
        lock_file_fd_(std::exchange(other.lock_file_fd_, {})) {}

  UniqueId &operator=(UniqueId &&other) noexcept {
    proc_ids_ = std::move(other.proc_ids_);
    id_ = std::exchange(other.id_, {});
    lock_file_fd_ = std::exchange(other.lock_file_fd_, {});

    return *this;
  }

  ~UniqueId();

  std::optional<value_type> id() const { return *id_; }

 private:
  stdx::expected<void, std::error_code> lock_file(const std::string &file_name);

  static std::shared_ptr<UniqueId::ProcessUniqueIds> process_unique_ids();

  std::shared_ptr<UniqueId::ProcessUniqueIds> proc_ids_;

  static std::string get_lock_file_dir();

  std::optional<value_type> id_{};

  FileHandle lock_file_fd_;
};

/** @class TcpPortPool
 *
 * Helper class allowing mechanism to retrieve pool of the system-level unique
 *TCP port numbers. Compatible with mysql-test MTR, see
 *mysql-test/lib/mtr_unique.pm for details.
 *
 **/
class TcpPortPool {
 public:
  uint16_t get_next_available();

 private:
  std::vector<UniqueId> unique_ids_;
  unsigned number_of_ids_used_{0};
  static const constexpr unsigned kPortsPerFile{10};
  static const constexpr unsigned kPortsStartFrom{100};
  static const constexpr unsigned kPortsRange{500};
};

#endif  // _TCP_PORT_POOL_H_
