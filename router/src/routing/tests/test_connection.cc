/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#include "connection.h"
#include "context.h"
#include "protocol/base_protocol.h"
#include "protocol/classic_protocol.h"
#include "routing_mocks.h"
#include "socket_operations.h"
#include "tcp_port_pool.h"
#include "test/helpers.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::_;

class MockProtocol : public BaseProtocol {
 public:
  MockProtocol() : BaseProtocol(nullptr) {}

  MOCK_METHOD2(on_block_client_host, bool(int, const std::string &));
  MOCK_METHOD5(send_error, bool(int, unsigned short, const std::string &,
                                const std::string &, const std::string &));
  MOCK_METHOD0(get_type, BaseProtocol::Type());

  /* Mocking copy_packet triggers compilation error in VS so let's just stub it,
     it is good enough for our needs here. */
  /* MOCK_METHOD8(copy_packets, int(int, int, bool,
     RoutingProtocolBuffer&, int* , bool&, size_t*, bool)); */
  virtual int copy_packets(int, int, bool, RoutingProtocolBuffer &, int *,
                           bool &, size_t *, bool) {
    return 1;
  }
};

#ifdef _WIN32
using socket_t = SOCKET;
#else
using socket_t = int;
#endif

class TestRoutingConnection : public testing::Test {
 public:
  void SetUp() override {
    protocol_.reset(new MockProtocol);
    name_ = "routing_name";
    net_buffer_length_ = routing::kDefaultNetBufferLength;
    destination_connect_timeout_ = std::chrono::milliseconds(10);
    client_connect_timeout_ = std::chrono::milliseconds(10);
    max_connect_errors_ = 100;
    thread_stack_size_ = 1000;
    client_socket_ = 3243;  // any number greater than 0
    server_socket_ = 5345;  // any number greater than 0
  }

  // context
  std::unique_ptr<MockProtocol> protocol_;
  MockSocketOperations socket_operations_;
  std::string name_;
  unsigned int net_buffer_length_;
  std::chrono::milliseconds destination_connect_timeout_;
  std::chrono::milliseconds client_connect_timeout_;
  mysql_harness::TCPAddress bind_address_;
  mysql_harness::Path bind_named_socket_;
  unsigned long long max_connect_errors_;
  size_t thread_stack_size_ = 1000;

  // connection
  socket_t client_socket_;
  sockaddr_storage client_addr_;
  socket_t server_socket_;
  mysql_harness::TCPAddress server_address_;

  mysql_harness::Path bind_named_socket;
};

/**
 * @test
 *       Verify if callback is called when run() function completes.
 */
TEST_F(TestRoutingConnection, IsCallbackCalledAtRunExit) {
  EXPECT_CALL(socket_operations_, shutdown(testing::_)).Times(2);
  EXPECT_CALL(socket_operations_, close(testing::_)).Times(2);

  union {
    sockaddr_storage client_addr_storage;
    sockaddr_in6 client_addr;
  };
  client_addr.sin6_family = AF_INET6;
  memset(&client_addr.sin6_addr, 0x0, sizeof(client_addr.sin6_addr));
  EXPECT_CALL(socket_operations_, getpeername(_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(*((sockaddr *)(&client_addr_storage))),
                      Return(0)));

  EXPECT_CALL(socket_operations_, inetntop(_, _, _, _))
      .WillOnce(Return("127.0.0.1"));

  EXPECT_CALL(*protocol_, on_block_client_host(testing::_, testing::_))
      .Times(testing::AtLeast(0))
      .WillRepeatedly(testing::Return(false));

  MySQLRoutingContext context(
      protocol_.release(), &socket_operations_, name_, net_buffer_length_,
      destination_connect_timeout_, client_connect_timeout_, bind_address_,
      bind_named_socket_, max_connect_errors_, thread_stack_size_);

  bool is_called = false;

  MySQLRoutingConnection connection(
      context, client_socket_, client_addr_, server_socket_, server_address_,
      [&is_called](MySQLRoutingConnection * /* connection */) {
        is_called = true;
      });

  // disconnect the connection
  connection.disconnect();

  // run connection in current thread
  connection.run();

  ASSERT_TRUE(is_called);
}

/**
 * @test
 *       Verify if callback is called when connection is closed.
 */
TEST_F(TestRoutingConnection, IsCallbackCalledAtThreadExit) {
  union {
    sockaddr_storage client_addr_storage;
    sockaddr_in6 client_addr;
  };
  client_addr.sin6_family = AF_INET6;
  memset(&client_addr.sin6_addr, 0x0, sizeof(client_addr.sin6_addr));

  EXPECT_CALL(socket_operations_, shutdown(testing::_)).Times(2);
  EXPECT_CALL(socket_operations_, close(testing::_)).Times(2);
  EXPECT_CALL(socket_operations_, getpeername(_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(*((sockaddr *)(&client_addr_storage))),
                      Return(0)));

  EXPECT_CALL(socket_operations_, inetntop(_, _, _, _))
      .WillOnce(Return("127.0.0.1"));

  EXPECT_CALL(*protocol_, on_block_client_host(testing::_, testing::_))
      .Times(testing::AtLeast(0))
      .WillRepeatedly(testing::Return(false));

  MySQLRoutingContext context(
      protocol_.release(), &socket_operations_, name_, net_buffer_length_,
      destination_connect_timeout_, client_connect_timeout_, bind_address_,
      bind_named_socket_, max_connect_errors_, thread_stack_size_);

  std::atomic_bool is_called{false};

  MySQLRoutingConnection connection(
      context, client_socket_, client_addr_, server_socket_, server_address_,
      [&is_called](MySQLRoutingConnection * /* connection */) {
        is_called = true;
      });

  // disconnect the thread
  connection.disconnect();

  // start new connection thread and wait for completion
  connection.start(false);
  ASSERT_TRUE(is_called);
}

/**
 * @test
 *       Verify if callback is called and thread of execution stops
 *       when connection is closed.
 */
TEST_F(TestRoutingConnection, IsConnectionThreadStopOnDisconnect) {
  union {
    sockaddr_storage client_addr_storage;
    sockaddr_in6 client_addr;
  };
  client_addr.sin6_family = AF_INET6;
  memset(&client_addr.sin6_addr, 0x0, sizeof(client_addr.sin6_addr));

  struct pollfd fds[] = {
      {client_socket_, POLLIN, 1},
      {server_socket_, POLLIN, 1},
  };

  using ::testing::InSequence;
  EXPECT_CALL(socket_operations_, poll(testing::_, testing::_, testing::_))
      .WillRepeatedly(testing::DoAll(testing::SetArgPointee<0>(fds[0]),
                                     testing::Return(1)));

  EXPECT_CALL(socket_operations_, shutdown(testing::_)).Times(2);
  EXPECT_CALL(socket_operations_, close(testing::_)).Times(2);
  EXPECT_CALL(socket_operations_, getpeername(_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(*((sockaddr *)(&client_addr_storage))),
                      Return(0)));

  EXPECT_CALL(socket_operations_, inetntop(_, _, _, _))
      .WillOnce(Return("127.0.0.1"));

  EXPECT_CALL(*protocol_, on_block_client_host(testing::_, testing::_))
      .Times(testing::AtLeast(0))
      .WillRepeatedly(testing::Return(false));

  MySQLRoutingContext context(
      protocol_.release(), &socket_operations_, name_, net_buffer_length_,
      destination_connect_timeout_, client_connect_timeout_, bind_address_,
      bind_named_socket_, max_connect_errors_, thread_stack_size_);

  std::atomic_bool is_called{false};

  MySQLRoutingConnection connection(
      context, client_socket_, client_addr_, server_socket_, server_address_,
      [&is_called](MySQLRoutingConnection * /* connection */) {
        is_called = true;
      });

  // start new connection thread
  connection.start();

  // wait for a while to let connection thread run
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // disconnect the connection
  connection.disconnect();

  // wait for connection to close
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  ASSERT_TRUE(is_called);
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
