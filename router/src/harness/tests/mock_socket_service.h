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

  MOCK_CONST_METHOD3(socket,
                     stdx::expected<native_handle_type, error_type>(int, int,
                                                                    int));

  MOCK_CONST_METHOD3(
      socketpair,
      stdx::expected<std::pair<native_handle_type, native_handle_type>,
                     error_type>(int, int, int));

  MOCK_CONST_METHOD1(close,
                     stdx::expected<void, error_type>(native_handle_type));

  MOCK_CONST_METHOD3(ioctl,
                     stdx::expected<void, error_type>(native_handle_type,
                                                      unsigned long, void *));

  MOCK_CONST_METHOD1(native_non_blocking,
                     stdx::expected<bool, error_type>(native_handle_type));
  MOCK_CONST_METHOD2(native_non_blocking,
                     stdx::expected<void, error_type>(native_handle_type,
                                                      bool));

  MOCK_CONST_METHOD2(listen,
                     stdx::expected<void, error_type>(native_handle_type, int));

  MOCK_CONST_METHOD5(setsockopt,
                     stdx::expected<void, error_type>(native_handle_type, int,
                                                      int, const void *,
                                                      socklen_t));
  MOCK_CONST_METHOD5(getsockopt,
                     stdx::expected<void, error_type>(native_handle_type, int,
                                                      int, void *,
                                                      socklen_t *));

  MOCK_CONST_METHOD3(recvmsg,
                     stdx::expected<size_t, error_type>(native_handle_type,
                                                        msghdr_base &,
                                                        message_flags));
  MOCK_CONST_METHOD3(sendmsg,
                     stdx::expected<size_t, error_type>(native_handle_type,
                                                        msghdr_base &,
                                                        message_flags));
  MOCK_CONST_METHOD3(bind,
                     stdx::expected<void, error_type>(native_handle_type,
                                                      const struct sockaddr *,
                                                      size_t));
  MOCK_CONST_METHOD3(connect,
                     stdx::expected<void, error_type>(native_handle_type,
                                                      const struct sockaddr *,
                                                      size_t));
  MOCK_CONST_METHOD3(accept,
                     stdx::expected<native_handle_type, error_type>(
                         native_handle_type, struct sockaddr *, socklen_t *));

  MOCK_CONST_METHOD4(accept4, stdx::expected<native_handle_type, error_type>(
                                  native_handle_type, struct sockaddr *,
                                  socklen_t *, int));
  MOCK_CONST_METHOD3(getsockname,
                     stdx::expected<void, error_type>(native_handle_type,
                                                      struct sockaddr *,
                                                      size_t *));
  MOCK_CONST_METHOD3(getpeername,
                     stdx::expected<void, error_type>(native_handle_type,
                                                      struct sockaddr *,
                                                      size_t *));

  MOCK_CONST_METHOD4(splice,
                     stdx::expected<size_t, error_type>(native_handle_type,
                                                        native_handle_type,
                                                        size_t, int));

  MOCK_CONST_METHOD4(splice_to_pipe,
                     stdx::expected<size_t, error_type>(
                         native_handle_type, net::impl::file::file_handle_type,
                         size_t, int));

  MOCK_CONST_METHOD4(splice_from_pipe, stdx::expected<size_t, error_type>(
                                           net::impl::file::file_handle_type,
                                           native_handle_type, size_t, int));

  MOCK_CONST_METHOD2(wait, stdx::expected<void, error_type>(native_handle_type,
                                                            wait_type));

  MOCK_CONST_METHOD2(shutdown,
                     stdx::expected<void, error_type>(native_handle_type, int));

  MOCK_CONST_METHOD3(
      getaddrinfo,
      stdx::expected<std::unique_ptr<addrinfo, void (*)(addrinfo *)>,
                     error_type>(const char *, const char *, const addrinfo *));
};

#endif
