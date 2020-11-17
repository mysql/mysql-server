/*
  Copyright (c) 2016, 2020, Oracle and/or its affiliates.

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

#ifndef ROUTING_MOCKS_INCLUDED
#define ROUTING_MOCKS_INCLUDED

#include <atomic>
#include <memory>  // unique_ptr

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/routing.h"  // RoutingSockOpsInterface
#include "socket_operations.h"

class MockSocketOperations : public mysql_harness::SocketOperationsBase {
 public:
  MOCK_METHOD3(read, result<size_t>(mysql_harness::socket_t, void *, size_t));
  MOCK_METHOD3(write,
               result<size_t>(mysql_harness::socket_t, const void *, size_t));
  MOCK_METHOD1(close, result<void>(mysql_harness::socket_t));
  MOCK_METHOD1(shutdown, result<void>(mysql_harness::socket_t));
  MOCK_METHOD3(getaddrinfo,
               addrinfo_result(const char *, const char *, const addrinfo *));
  MOCK_METHOD3(connect, result<void>(mysql_harness::socket_t,
                                     const struct sockaddr *, size_t));
  MOCK_METHOD3(bind, result<void>(mysql_harness::socket_t,
                                  const struct sockaddr *, size_t));
  MOCK_METHOD3(socket, result<mysql_harness::socket_t>(int, int, int));
  MOCK_METHOD5(setsockopt, result<void>(mysql_harness::socket_t, int, int,
                                        const void *, size_t));
  MOCK_METHOD2(listen, result<void>(mysql_harness::socket_t fd, int n));
  MOCK_METHOD3(poll, result<size_t>(struct pollfd *, size_t,
                                    std::chrono::milliseconds));
  MOCK_METHOD2(connect_non_blocking_wait,
               result<void>(mysql_harness::socket_t sock,
                            std::chrono::milliseconds timeout));
  MOCK_METHOD1(connect_non_blocking_status,
               result<void>(mysql_harness::socket_t sock));
  MOCK_METHOD2(set_socket_blocking,
               result<void>(mysql_harness::socket_t, bool));
  MOCK_METHOD0(get_local_hostname, std::string());
  MOCK_METHOD4(inetntop,
               result<const char *>(int af, const void *, char *, size_t));
  MOCK_METHOD3(getpeername, result<void>(mysql_harness::socket_t,
                                         struct sockaddr *, size_t *));
  MOCK_METHOD2(has_data, result<bool>(mysql_harness::socket_t,
                                      std::chrono::milliseconds));
};

#endif  // ROUTING_MOCKS_INCLUDED
