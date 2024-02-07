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

#ifndef MYSQLROUTER_MOCK_SOCKET_SERVICE_H
#define MYSQLROUTER_MOCK_SOCKET_SERVICE_H

#include <gmock/gmock.h>

#include "mysql/harness/net_ts/impl/socket_service_base.h"

class MockSocketService : public net::impl::socket::SocketServiceBase {
 public:
  using native_handle_type = net::impl::socket::native_handle_type;
  using error_type = net::impl::socket::error_type;
  using message_flags = net::impl::socket::message_flags;
  using msghdr_base = net::impl::socket::msghdr_base;
  using wait_type = net::impl::socket::wait_type;

  MOCK_METHOD((stdx::expected<native_handle_type, error_type>), socket,
              (int, int, int), (const, override));

  MOCK_METHOD((stdx::expected<std::pair<native_handle_type, native_handle_type>,
                              error_type>),
              socketpair, (int, int, int), (const, override));

  MOCK_METHOD((stdx::expected<void, error_type>), close, (native_handle_type),
              (const, override));

  MOCK_METHOD((stdx::expected<void, error_type>), ioctl,
              (native_handle_type, unsigned long, void *), (const, override));

  MOCK_METHOD((stdx::expected<bool, error_type>), native_non_blocking,
              (native_handle_type), (const, override));
  MOCK_METHOD((stdx::expected<void, error_type>), native_non_blocking,
              (native_handle_type, bool), (const, override));

  MOCK_METHOD((stdx::expected<void, error_type>), listen,
              (native_handle_type, int), (const, override));

  MOCK_METHOD((stdx::expected<void, error_type>), setsockopt,
              (native_handle_type, int, int, const void *, socklen_t),
              (const, override));

  MOCK_METHOD((stdx::expected<void, error_type>), getsockopt,
              (native_handle_type, int, int, void *, socklen_t *),
              (const, override));

  MOCK_METHOD((stdx::expected<size_t, error_type>), recvmsg,
              (native_handle_type, msghdr_base &, message_flags),
              (const, override));
  MOCK_METHOD((stdx::expected<size_t, error_type>), sendmsg,
              (native_handle_type, msghdr_base &, message_flags),
              (const, override));
  MOCK_METHOD((stdx::expected<void, error_type>), bind,
              (native_handle_type, const struct sockaddr *, size_t),
              (const, override));
  MOCK_METHOD((stdx::expected<void, error_type>), connect,
              (native_handle_type, const struct sockaddr *, size_t),
              (const, override));
  MOCK_METHOD((stdx::expected<native_handle_type, error_type>), accept,
              (native_handle_type, struct sockaddr *, socklen_t *),
              (const, override));

  MOCK_METHOD((stdx::expected<native_handle_type, error_type>), accept4,
              (native_handle_type, struct sockaddr *, socklen_t *, int),
              (const, override));
  MOCK_METHOD((stdx::expected<void, error_type>), getsockname,
              (native_handle_type, struct sockaddr *, size_t *),
              (const, override));
  MOCK_METHOD((stdx::expected<void, error_type>), getpeername,
              (native_handle_type, struct sockaddr *, size_t *),
              (const, override));

#ifdef __linux__
  MOCK_METHOD((stdx::expected<size_t, error_type>), splice,
              (native_handle_type, native_handle_type, size_t, int),
              (const, override));
#endif

  MOCK_METHOD((stdx::expected<size_t, error_type>), splice_to_pipe,
              (native_handle_type, net::impl::file::file_handle_type, size_t,
               int),
              (const, override));

  MOCK_METHOD((stdx::expected<size_t, error_type>), splice_from_pipe,
              (net::impl::file::file_handle_type, native_handle_type, size_t,
               int),
              (const, override));

  MOCK_METHOD((stdx::expected<void, error_type>), wait,
              (native_handle_type, wait_type), (const, override));

  MOCK_METHOD((stdx::expected<void, error_type>), shutdown,
              (native_handle_type, int), (const, override));

  MOCK_METHOD((stdx::expected<std::unique_ptr<addrinfo, void (*)(addrinfo *)>,
                              error_type>),
              getaddrinfo, (const char *, const char *, const addrinfo *),
              (const, override));
};

#endif
