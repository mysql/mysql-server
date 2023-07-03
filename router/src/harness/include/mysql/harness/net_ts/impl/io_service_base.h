/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_NET_TS_IMPL_IO_SERVICE_BASE_H_
#define MYSQL_HARNESS_NET_TS_IMPL_IO_SERVICE_BASE_H_

#include <chrono>
#include <system_error>

#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/stdx/expected.h"

namespace net {
enum class io_service_errc {
  no_fds = 1,
};
}

namespace std {
template <>
struct is_error_code_enum<net::io_service_errc> : public std::true_type {};
}  // namespace std

namespace net {
inline const std::error_category &io_service_category() noexcept {
  class io_service_category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "io_service"; }
    std::string message(int ev) const override {
      switch (static_cast<io_service_errc>(ev)) {
        case io_service_errc::no_fds:
          return "no file-descriptors";
        default:
          return "unknown";
      }
    }
  };

  static io_service_category_impl instance;
  return instance;
}

inline std::error_code make_error_code(net::io_service_errc e) noexcept {
  return {static_cast<int>(e), net::io_service_category()};
}

struct fd_event {
  using native_handle_type = impl::socket::native_handle_type;

  fd_event() = default;
  fd_event(native_handle_type _fd, short _event) : fd{_fd}, event{_event} {}

  native_handle_type fd{net::impl::socket::kInvalidSocket};
  short event{};
};

inline constexpr bool operator==(const fd_event &a, const fd_event &b) {
  return a.event == b.event && a.fd == b.fd;
}

inline constexpr bool operator!=(const fd_event &a, const fd_event &b) {
  return !(a == b);
}

class IoServiceBase {
 public:
  using native_handle_type = impl::socket::native_handle_type;

  virtual ~IoServiceBase() = default;

  /**
   * open the io-service.
   *
   * MUST be called before any of the other functions is called.
   *
   * may fail if out of file-descriptors.
   *
   * @returns an std::error_code on error
   */
  virtual stdx::expected<void, std::error_code> open() = 0;

  virtual void notify() = 0;

  virtual stdx::expected<void, std::error_code> add_fd_interest(
      native_handle_type fd, impl::socket::wait_type event) = 0;
  virtual stdx::expected<fd_event, std::error_code> poll_one(
      std::chrono::milliseconds timeout) = 0;

  virtual stdx::expected<void, std::error_code> remove_fd(
      native_handle_type fd) = 0;
};
}  // namespace net

#endif
