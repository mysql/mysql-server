/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_NET_TS_IMPL_SOCKET_ERROR_H_
#define MYSQL_HARNESS_NET_TS_IMPL_SOCKET_ERROR_H_

#include <system_error>

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#else
#include <cerrno>
#endif

namespace net {
enum class socket_errc {
  already_open = 1,
  not_found = 2,
};
}  // namespace net

namespace std {
template <>
struct is_error_code_enum<net::socket_errc> : public true_type {};
}  // namespace std

namespace net {
inline const std::error_category &socket_category() noexcept {
  class category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "socket"; }
    std::string message(int ev) const override {
      switch (static_cast<socket_errc>(ev)) {
        case socket_errc::already_open:
          return "already_open";
        case socket_errc::not_found:
          return "not found";
      }

      // don't use switch-default to trigger a warning for unhandled enum-value
      return "unknown";
    }
  };

  static category_impl instance;
  return instance;
}

inline std::error_code make_error_code(net::socket_errc e) noexcept {
  return {static_cast<int>(e), net::socket_category()};
}

namespace impl {
namespace socket {

/**
 * get last socket error.
 */
inline int last_error() {
#ifdef _WIN32
  return WSAGetLastError();
#else
  return errno;
#endif
}

/**
 * make proper std::error_code for socket errno's
 *
 * On windows, WSAGetLastError() returns a code from the system-category
 * On POSIX systems, errno returns a code from the generic-category
 */
inline std::error_code make_error_code(int errcode) {
#ifdef _WIN32
  return {errcode, std::system_category()};
#else
  return {errcode, std::generic_category()};
#endif
}

/**
 * get last std::error_code for socket-errors.
 */
inline std::error_code last_error_code() {
  return make_error_code(last_error());
}

}  // namespace socket
}  // namespace impl
}  // namespace net

#endif
