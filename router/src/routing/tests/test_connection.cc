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

#include "connection.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "context.h"
#include "mock_io_service.h"
#include "mock_socket_service.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqlrouter/base_protocol.h"
#include "protocol/classic_protocol.h"
#include "routing_mocks.h"
#include "socket_operations.h"
#include "ssl_mode.h"
#include "tcp_address.h"
#include "tcp_port_pool.h"
#include "test/helpers.h"

using namespace std::chrono_literals;

using ::testing::_;
using ::testing::Return;

class MockProtocol : public BaseProtocol {
 public:
  MockProtocol() : BaseProtocol(nullptr) {}

  MOCK_METHOD(bool, on_block_client_host, (int, const std::string &));
  MOCK_METHOD(bool, send_error,
              (int, unsigned short, const std::string &, const std::string &,
               const std::string &));
  MOCK_METHOD(BaseProtocol::Type, get_type, ());

  /* Mocking copy_packet triggers compilation error in VS so let's just stub it,
     it is good enough for our needs here. */
  /* MOCK_METHOD8(copy_packets, int(int, int, bool,
     RoutingProtocolBuffer&, int* , bool&, size_t*, bool)); */
  stdx::expected<size_t, std::error_code> copy_packets(int, int, bool,
                                                       std::vector<uint8_t> &,
                                                       int *, bool &, bool) {
    return stdx::unexpected(make_error_code(std::errc::connection_reset));
  }
};

class TestRoutingConnection : public testing::Test {
 public:
  // context
  ::testing::StrictMock<MockSocketOperations> socket_operations_;
  const std::string name_{"routing_name"};
  const unsigned int net_buffer_length_{routing::kDefaultNetBufferLength};
  const std::chrono::milliseconds destination_connect_timeout_{10ms};
  const std::chrono::milliseconds client_connect_timeout_{10ms};
  mysql_harness::TCPAddress bind_address_;
  mysql_harness::Path bind_named_socket_;
  const unsigned long long max_connect_errors_{100};
  const size_t thread_stack_size_{1000};
};

/**
 * @test
 *       Verify if callback is called when run() function completes.
 */
TEST_F(TestRoutingConnection, IsCallbackCalledAtRunExit) {
  auto io_service = std::make_unique<MockIoService>();

  // succeed the open
  EXPECT_CALL(*io_service, open);

  net::io_context io_ctx{std::make_unique<MockSocketService>(),
                         std::move(io_service)};

  ASSERT_TRUE(io_ctx.open_res());

  MySQLRoutingContext context(
      new ::testing::StrictMock<MockProtocol>, &socket_operations_, name_,
      net_buffer_length_, destination_connect_timeout_, client_connect_timeout_,
      bind_address_, bind_named_socket_, max_connect_errors_,
      thread_stack_size_, SslMode::kPassthrough, nullptr, SslMode::kAsClient,
      nullptr);

  auto &sock_ops = *dynamic_cast<MockSocketService *>(io_ctx.socket_service());
  auto &io_ops = *dynamic_cast<MockIoService *>(io_ctx.io_service());

  constexpr const net::impl::socket::native_handle_type client_socket_handle{
      25};
  constexpr const net::impl::socket::native_handle_type server_socket_handle{
      32};

  EXPECT_CALL(sock_ops, socket(_, _, _))
      .WillOnce(Return(client_socket_handle))
      .WillOnce(Return(server_socket_handle));

  // pretend the server side is readable.
  EXPECT_CALL(io_ops, poll_one(_))
      .WillRepeatedly(
          Return(stdx::unexpected(make_error_code(std::errc::timed_out))));

  EXPECT_CALL(*dynamic_cast<MockProtocol *>(&context.get_protocol()),
              get_type())
      .WillOnce(Return(BaseProtocol::Type::kClassicProtocol));

  EXPECT_CALL(*dynamic_cast<MockProtocol *>(&context.get_protocol()),
              on_block_client_host(server_socket_handle, _));

  EXPECT_CALL(io_ops, notify()).Times(3);

  // pretend the server closed the socket on the first recvmsg().
  EXPECT_CALL(sock_ops, recvmsg(server_socket_handle, _, _))
      .WillOnce(
          Return(stdx::unexpected(make_error_code(net::stream_errc::eof))));

  // each FD is removed once
  EXPECT_CALL(io_ops, remove_fd(client_socket_handle)).Times(1);
  EXPECT_CALL(io_ops, remove_fd(server_socket_handle)).Times(1);

  EXPECT_CALL(sock_ops, shutdown(client_socket_handle, _));
  EXPECT_CALL(sock_ops, shutdown(server_socket_handle, _));
  EXPECT_CALL(sock_ops, close(client_socket_handle));
  EXPECT_CALL(sock_ops, close(server_socket_handle));

  net::ip::tcp::socket client_socket(io_ctx);
  net::ip::tcp::endpoint client_endpoint;  // ipv4, 0.0.0.0:0
  net::ip::tcp::socket server_socket(io_ctx);
  net::ip::tcp::endpoint server_endpoint;  // ipv4, 0.0.0.0:0

  // open the socket to trigger the socket() call
  client_socket.open(net::ip::tcp::v4());
  server_socket.open(net::ip::tcp::v4());

  // test target: if the remove-callback is called, it will set is_called.
  bool is_called{false};

  MySQLRoutingConnection<net::ip::tcp, net::ip::tcp> connection(
      context, "some-destination-name", std::move(client_socket),
      client_endpoint, std::move(server_socket), server_endpoint,
      [&is_called](MySQLRoutingConnectionBase * /* connection */) {
        is_called = true;
      });

  // execution the connection until it would block.
  connection.async_run();

  // nothing should be waited for.
  EXPECT_EQ(io_ctx.run(), 0);

  //
  EXPECT_EQ(context.get_active_routes(), 0);

  EXPECT_TRUE(is_called);
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
