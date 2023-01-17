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
#ifndef MYSQL_HARNESS_STDX_FILE_HANDLE_INCLUDED
#define MYSQL_HARNESS_STDX_FILE_HANDLE_INCLUDED

#include <string>
#include <system_error>
#include <utility>  // exchange

#ifndef _WIN32
#include <sys/types.h>  // ino_t
#endif

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/filesystem.h"
#include "mysql/harness/stdx_export.h"

// (Partial) Implementation of P1883r1
//
// see: http://wg21.link/p1883
//
// barely enough implementation to replace all uses of
//
// - open()
// - close()

namespace stdx {
namespace io {

enum class creation {
  open_existing = 0,
  only_if_not_exist,
  if_needed,
  truncate_existing,
  // always_new
};

enum class mode {
  unchanged = 0,
  read = 6,
  write = 7,
  append = 9,
};

enum class caching {
  unchanged = 0,
  none = 1,
  all = 6,
  temporary = 8,
};

class flag {
 public:
  using value_type = uint64_t;

  static constexpr value_type none{0};
  static constexpr value_type unlink_on_first_close{1 << 0};

  constexpr flag(value_type v) : value_{v} {}

  constexpr value_type value() const noexcept { return value_; }

  constexpr value_type operator&(const value_type &other) {
    return value() & other;
  }

 private:
  value_type value_;
};

class path_handle {
 public:
  path_handle() = default;
};

class HARNESS_STDX_EXPORT file_handle {
 public:
  using path_type = filesystem::path;
  using path_view_type = filesystem::path;

  using native_handle_type = int;
#ifdef _WIN32
  using dev_t = ::_dev_t;
  using ino_t = ::_ino_t;
#else
  using dev_t = ::dev_t;
  using ino_t = ::ino_t;
#endif

  static constexpr const native_handle_type invalid_handle{-1};

  file_handle(native_handle_type h, dev_t devid, ino_t inode,
              caching caching = caching::none, flag flags = flag::none) noexcept
      : handle_{h},
        devid_{devid},
        inode_{inode},
        caching_{caching},
        flags_{flags} {}

  file_handle(const file_handle &) = delete;
  file_handle &operator=(const file_handle &) = delete;

  file_handle(file_handle &&rhs)
      : handle_{std::exchange(rhs.handle_, invalid_handle)},
        devid_{std::move(rhs.devid_)},
        inode_{std::move(rhs.inode_)},
        caching_{std::move(rhs.caching_)},
        flags_{std::move(rhs.flags_)} {}

  ~file_handle() {
    if (handle_ != invalid_handle) {
      close();
    }
  }

  static stdx::expected<file_handle, std::error_code> file(
      const path_handle &base, file_handle::path_view_type path,
      mode _mode = mode::read, creation _creation = creation::open_existing,
      caching _caching = caching::all, flag flags = flag::none) noexcept;

  static stdx::expected<file_handle, std::error_code> uniquely_named_file(
      const path_handle &base, mode _mode = mode::write,
      caching _caching = caching::temporary, flag flags = flag::none) noexcept;

  stdx::expected<void, std::error_code> unlink();

  stdx::expected<void, std::error_code> close() noexcept;

  stdx::expected<size_t, std::error_code> write(const char *data,
                                                const size_t len);

  native_handle_type release() noexcept {
    return std::exchange(handle_, invalid_handle);
  }

  caching kernel_caching() const noexcept { return caching_; }
  flag flags() const noexcept { return flags_; }

  native_handle_type native_handle() const noexcept { return handle_; }

  dev_t st_dev() const noexcept { return devid_; }
  ino_t st_ino() const noexcept { return inode_; }

  // get path of the current file_handle
  stdx::expected<path_type, std::error_code> current_path() const noexcept;

 private:
  native_handle_type handle_{invalid_handle};

  dev_t devid_;
  ino_t inode_;
  caching caching_;
  flag flags_;
};

}  // namespace io
}  // namespace stdx

#endif
