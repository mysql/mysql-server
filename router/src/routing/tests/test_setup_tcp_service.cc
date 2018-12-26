/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "gmock/gmock.h"

#include "../../routing/src/mysql_routing.h"
#include "mysqlrouter/routing.h"
#include "router_test_helpers.h"
#include "routing_mocks.h"
#include "test/helpers.h"

#include <memory>
#include <string>
#ifndef _WIN32
#include <netinet/in.h>
#else
#include <WinSock2.h>
#endif

using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::_;

class TestSetupTcpService : public ::testing::Test {
 protected:
  // create some linked list of the addresses that getaddrinfo returns
  addrinfo *get_test_addresses_list(size_t qty = 1) {
    addrinfo *prev{nullptr}, *head{nullptr};
    for (size_t i = 0; i < qty; i++) {
      auto ai = new addrinfo();
      // fields values don't matter for us in those tests
      memset(ai, 0x0, sizeof(addrinfo));

      if (prev) {
        prev->ai_next = ai;
      }
      if (!head) {
        head = ai;
      }

      prev = ai;

      addresses_to_release_.emplace_back(ai);
    }

    return head;
  }

 protected:
  MockRoutingSockOps routing_sock_ops;
  MockSocketOperations &socket_op = *routing_sock_ops.so();

 private:
  std::vector<std::unique_ptr<addrinfo>> addresses_to_release_;
};

TEST_F(TestSetupTcpService, single_addr_ok) {
  MySQLRouting r(routing::RoutingStrategy::kFirstAvailable, 7001,
                 Protocol::Type::kClassicProtocol,
                 routing::AccessMode::kReadWrite, "127.0.0.1",
                 mysql_harness::Path(), "routing-name", 1,
                 std::chrono::seconds(1), 1, std::chrono::seconds(1),
                 routing::kDefaultNetBufferLength, &routing_sock_ops);

  const auto addr_list = get_test_addresses_list(1);
  EXPECT_CALL(socket_op, getaddrinfo(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(addr_list), Return(0)));

  EXPECT_CALL(socket_op, socket(_, _, _)).WillOnce(Return(1));
#ifndef _WIN32
  EXPECT_CALL(socket_op, setsockopt(_, _, _, _, _)).WillOnce(Return(0));
#endif
  EXPECT_CALL(socket_op, bind(_, _, _)).WillOnce(Return(0));

  EXPECT_CALL(socket_op, listen(_, _)).WillOnce(Return(0));

  EXPECT_CALL(socket_op, freeaddrinfo(_));

  // those are called in the MySQLRouting destructor
  EXPECT_CALL(socket_op, close(_));
  EXPECT_CALL(socket_op, shutdown(_));

  ASSERT_NO_THROW(r.setup_tcp_service());
}

TEST_F(TestSetupTcpService, getaddrinfo_fails) {
  MySQLRouting r(routing::RoutingStrategy::kFirstAvailable, 7001,
                 Protocol::Type::kClassicProtocol,
                 routing::AccessMode::kReadWrite, "127.0.0.1",
                 mysql_harness::Path(), "routing-name", 1,
                 std::chrono::seconds(1), 1, std::chrono::seconds(1),
                 routing::kDefaultNetBufferLength, &routing_sock_ops);

  EXPECT_CALL(socket_op, getaddrinfo(_, _, _, _)).WillOnce(Return(-1));

  ASSERT_THROW_LIKE(r.setup_tcp_service(), std::runtime_error,
                    "[routing-name] Failed getting address information");
}

TEST_F(TestSetupTcpService, socket_fails_for_all_addr) {
  MySQLRouting r(routing::RoutingStrategy::kFirstAvailable, 7001,
                 Protocol::Type::kClassicProtocol,
                 routing::AccessMode::kReadWrite, "127.0.0.1",
                 mysql_harness::Path(), "routing-name", 1,
                 std::chrono::seconds(1), 1, std::chrono::seconds(1),
                 routing::kDefaultNetBufferLength, &routing_sock_ops);

  const auto addr_list = get_test_addresses_list(2);
  EXPECT_CALL(socket_op, getaddrinfo(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(addr_list), Return(0)));

  // make the first call to socket() fail
  EXPECT_CALL(socket_op, socket(_, _, _))
      .WillOnce(Return(-1))
      .WillOnce(Return(-1));

  EXPECT_CALL(socket_op, freeaddrinfo(_));

  ASSERT_THROW_LIKE(r.setup_tcp_service(), std::runtime_error,
                    "[routing-name] Failed to setup service socket");
}

TEST_F(TestSetupTcpService, socket_fails) {
  MySQLRouting r(routing::RoutingStrategy::kFirstAvailable, 7001,
                 Protocol::Type::kClassicProtocol,
                 routing::AccessMode::kReadWrite, "127.0.0.1",
                 mysql_harness::Path(), "routing-name", 1,
                 std::chrono::seconds(1), 1, std::chrono::seconds(1),
                 routing::kDefaultNetBufferLength, &routing_sock_ops);

  const auto addr_list = get_test_addresses_list(2);
  EXPECT_CALL(socket_op, getaddrinfo(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(addr_list), Return(0)));

  // make the first call to socket() fail
  EXPECT_CALL(socket_op, socket(_, _, _))
      .WillOnce(Return(-1))
      .WillOnce(Return(1));

#ifndef _WIN32
  EXPECT_CALL(socket_op, setsockopt(_, _, _, _, _)).WillOnce(Return(0));
#endif
  EXPECT_CALL(socket_op, bind(_, _, _)).WillOnce(Return(0));

  EXPECT_CALL(socket_op, listen(_, _)).WillOnce(Return(0));

  EXPECT_CALL(socket_op, freeaddrinfo(_));

  // those are called in the MySQLRouting destructor
  EXPECT_CALL(socket_op, close(_));
  EXPECT_CALL(socket_op, shutdown(_));

  ASSERT_NO_THROW(r.setup_tcp_service());
}

#ifndef _WIN32
TEST_F(TestSetupTcpService, setsockopt_fails) {
  MySQLRouting r(routing::RoutingStrategy::kFirstAvailable, 7001,
                 Protocol::Type::kClassicProtocol,
                 routing::AccessMode::kReadWrite, "127.0.0.1",
                 mysql_harness::Path(), "routing-name", 1,
                 std::chrono::seconds(1), 1, std::chrono::seconds(1),
                 routing::kDefaultNetBufferLength, &routing_sock_ops);

  const auto addr_list = get_test_addresses_list(2);
  EXPECT_CALL(socket_op, getaddrinfo(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(addr_list), Return(0)));

  EXPECT_CALL(socket_op, socket(_, _, _))
      .WillOnce(Return(1))
      .WillOnce(Return(1));

  // make the first call to setsockopt() fail
  EXPECT_CALL(socket_op, setsockopt(_, _, _, _, _))
      .WillOnce(Return(-1))
      .WillOnce(Return(0));

  EXPECT_CALL(socket_op, bind(_, _, _)).WillOnce(Return(0));

  EXPECT_CALL(socket_op, listen(_, _)).WillOnce(Return(0));

  EXPECT_CALL(socket_op, freeaddrinfo(_));

  // those are called in the MySQLRouting destructor
  EXPECT_CALL(socket_op, close(_)).Times(2);
  EXPECT_CALL(socket_op, shutdown(_));

  ASSERT_NO_THROW(r.setup_tcp_service());
}
#endif

TEST_F(TestSetupTcpService, bind_fails) {
  MySQLRouting r(routing::RoutingStrategy::kFirstAvailable, 7001,
                 Protocol::Type::kClassicProtocol,
                 routing::AccessMode::kReadWrite, "127.0.0.1",
                 mysql_harness::Path(), "routing-name", 1,
                 std::chrono::seconds(1), 1, std::chrono::seconds(1),
                 routing::kDefaultNetBufferLength, &routing_sock_ops);

  const auto addr_list = get_test_addresses_list(2);
  EXPECT_CALL(socket_op, getaddrinfo(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(addr_list), Return(0)));

  // make the first call to socket() fail
  EXPECT_CALL(socket_op, socket(_, _, _))
      .WillOnce(Return(1))
      .WillOnce(Return(1));

#ifndef _WIN32
  EXPECT_CALL(socket_op, setsockopt(_, _, _, _, _))
      .WillOnce(Return(0))
      .WillOnce(Return(0));
#endif
  EXPECT_CALL(socket_op, bind(_, _, _))
      .WillOnce(Return(-1))
      .WillOnce(Return(0));

  EXPECT_CALL(socket_op, listen(_, _)).WillOnce(Return(0));

  EXPECT_CALL(socket_op, freeaddrinfo(_));

  // those are called in the MySQLRouting destructor
  EXPECT_CALL(socket_op, close(_)).Times(2);
  EXPECT_CALL(socket_op, shutdown(_));

  ASSERT_NO_THROW(r.setup_tcp_service());
}

TEST_F(TestSetupTcpService, listen_fails) {
  MySQLRouting r(routing::RoutingStrategy::kFirstAvailable, 7001,
                 Protocol::Type::kClassicProtocol,
                 routing::AccessMode::kReadWrite, "127.0.0.1",
                 mysql_harness::Path(), "routing-name", 1,
                 std::chrono::seconds(1), 1, std::chrono::seconds(1),
                 routing::kDefaultNetBufferLength, &routing_sock_ops);

  const auto addr_list = get_test_addresses_list(2);
  EXPECT_CALL(socket_op, getaddrinfo(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(addr_list), Return(0)));

  // make the first call to socket() fail
  EXPECT_CALL(socket_op, socket(_, _, _)).WillOnce(Return(1));

#ifndef _WIN32
  EXPECT_CALL(socket_op, setsockopt(_, _, _, _, _)).WillOnce(Return(0));
#endif
  EXPECT_CALL(socket_op, bind(_, _, _)).WillOnce(Return(0));

  EXPECT_CALL(socket_op, listen(_, _)).WillOnce(Return(-1));

  EXPECT_CALL(socket_op, freeaddrinfo(_));

  // those are called in the MySQLRouting destructor
  EXPECT_CALL(socket_op, close(_));
  EXPECT_CALL(socket_op, shutdown(_));

  ASSERT_THROW_LIKE(
      r.setup_tcp_service(), std::runtime_error,
      "[routing-name] Failed to start listening for connections using TCP");
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  init_test_logger();
  return RUN_ALL_TESTS();
}
