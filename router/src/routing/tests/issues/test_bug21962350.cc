/*
  Copyright (c) 2015, 2021, Oracle and/or its affiliates.

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

/**
 * BUG21962350 Issue with destination server removal from quarantine
 *
 */

#include <array>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>  // inet_pton
#include <netdb.h>
#include <netinet/in.h>
#else
#include <windows.h>
#include <winsock2.h>
#endif

#include <gmock/gmock-actions.h>
#include <gmock/gmock.h>

#include "dest_round_robin.h"
#include "destination.h"
#include "mock_io_service.h"
#include "mock_socket_service.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/type_traits.h"
#include "mysqlrouter/routing.h"
#include "protocol/protocol.h"
#include "tcp_address.h"
#include "test/helpers.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::HasSubstr;
using ::testing::Return;

class MockRouteDestination : public DestRoundRobin {
 public:
  using DestRoundRobin::DestRoundRobin;

  // make protected function public
  void add_to_quarantine(const size_t index) noexcept override {
    DestRoundRobin::add_to_quarantine(index);
  }

  // make protected function public
  void cleanup_quarantine() noexcept override {
    DestRoundRobin::cleanup_quarantine();
  }
};

class Bug21962350 : public ::testing::Test {
 protected:
  void SetUp() override {
    std::ostream *log_stream =
        mysql_harness::logging::get_default_logger_stream();

    orig_log_stream_ = log_stream->rdbuf();
    log_stream->rdbuf(sslog.rdbuf());
  }

  void TearDown() override {
    if (orig_log_stream_) {
      std::ostream *log_stream =
          mysql_harness::logging::get_default_logger_stream();
      log_stream->rdbuf(orig_log_stream_);
    }
  }

  std::array<std::pair<std::string, uint16_t>, 3> servers = {{
      {"127.0.0.1", 3306},
      {"127.0.0.2", 3306},
      {"127.0.0.3", 3306},
  }};

  std::stringstream sslog;

 private:
  std::streambuf *orig_log_stream_;
};

TEST_F(Bug21962350, AddToQuarantine) {
  net::io_context io_ctx;
  MockRouteDestination d(io_ctx);
  for (auto const &p : servers) {
    d.add(p.first, p.second);
  }

  EXPECT_EQ(0, d.size_quarantine());

  d.add_to_quarantine(0);
  EXPECT_EQ(1, d.size_quarantine());

  d.add_to_quarantine(1);
  EXPECT_EQ(2, d.size_quarantine());

  d.add_to_quarantine(2);
  EXPECT_EQ(3, d.size_quarantine());
}

TEST_F(Bug21962350, CleanupQuarantine) {
  auto io_service = std::make_unique<MockIoService>();

  // succeed the open
  EXPECT_CALL(*io_service, open);
  EXPECT_CALL(*io_service, notify).Times(4);
  EXPECT_CALL(*io_service, remove_fd).Times(4);

  net::io_context io_ctx{std::make_unique<MockSocketService>(),
                         std::move(io_service)};

  ASSERT_TRUE(io_ctx.open_res());

  ::testing::NiceMock<MockRouteDestination> d(io_ctx, Protocol::get_default());

  // add 3 servers to the Route
  for (auto const &p : servers) {
    d.add(p.first, p.second);
  }

  // add all 3 ndxes to the quarantine
  d.add_to_quarantine(0);
  d.add_to_quarantine(1);
  d.add_to_quarantine(2);
  ASSERT_EQ(3, d.size_quarantine());

  auto sock_service =
      reinterpret_cast<MockSocketService *>(io_ctx.socket_service());

  auto mock_addrinfo = [](const std::string &host, uint16_t port)
      -> stdx::expected<
          std::unique_ptr<struct addrinfo, void (*)(struct addrinfo *)>,
          std::error_code> {
    auto mock_free_addrinfo = [](addrinfo *ai) {
      while (ai) {
        delete ai->ai_addr;

        auto *next = ai->ai_next;
        delete ai;

        ai = next;
      }

      return;
    };
    auto *ai = new addrinfo{};
    auto *addr_in = new sockaddr_in;
    addr_in->sin_family = AF_INET;
    addr_in->sin_port = htons(port);
    ::inet_pton(addr_in->sin_family, host.c_str(), &addr_in->sin_addr);

    ai->ai_socktype = SOCK_STREAM;
    ai->ai_family = addr_in->sin_family;
    ai->ai_next = nullptr;
    ai->ai_addr = reinterpret_cast<sockaddr *>(addr_in);
    ai->ai_addrlen = sizeof(sockaddr_in);

    return {stdx::in_place, ai, mock_free_addrinfo};
  };

  EXPECT_CALL(*sock_service, getaddrinfo(_, _, _))
      .Times(4)
      .WillOnce(
          Return(ByMove(mock_addrinfo(servers[0].first, servers[0].second))))
      .WillOnce(
          Return(ByMove(mock_addrinfo(servers[1].first, servers[1].second))))
      .WillOnce(
          Return(ByMove(mock_addrinfo(servers[2].first, servers[2].second))))
      .WillOnce(
          Return(ByMove(mock_addrinfo(servers[1].first, servers[1].second))));
  EXPECT_CALL(*sock_service, socket(_, _, _)).Times(4);
  EXPECT_CALL(*sock_service, native_non_blocking(_, _)).Times(4);
  // try to connect() 4 times, but at one of them fail
  EXPECT_CALL(*sock_service, connect(_, _, _))
      .Times(4)
      .WillOnce(Return(stdx::expected<void, std::error_code>()))
      .WillOnce(Return(stdx::make_unexpected(
          make_error_code(std::errc::connection_refused))))
      .WillOnce(Return(stdx::expected<void, std::error_code>()))
      .WillOnce(Return(stdx::expected<void, std::error_code>()));
  EXPECT_CALL(*sock_service, close(_)).Times(4);

  // 1st round: 3 connect().
  //
  // - success
  // - fail
  // - success
  d.cleanup_quarantine();
  EXPECT_EQ(1, d.size_quarantine());

  // 2nd round
  // - success
  d.cleanup_quarantine();
  EXPECT_EQ(0, d.size_quarantine());
}

TEST_F(Bug21962350, QuarantineServerMultipleTimes) {
  net::io_context io_ctx;
  MockRouteDestination d(io_ctx);

  for (auto const &p : servers) {
    d.add(p.first, p.second);
  }

  d.add_to_quarantine(static_cast<size_t>(0));
  d.add_to_quarantine(static_cast<size_t>(0));
  d.add_to_quarantine(static_cast<size_t>(2));
  d.add_to_quarantine(static_cast<size_t>(1));

  ASSERT_EQ(3, d.size_quarantine());
}

TEST_F(Bug21962350, AlreadyQuarantinedServer) {
  net::io_context io_ctx;
  MockRouteDestination d(io_ctx);

  for (auto const &p : servers) {
    d.add(p.first, p.second);
  }

  d.add_to_quarantine(static_cast<size_t>(1));
  d.add_to_quarantine(static_cast<size_t>(1));
  ASSERT_EQ(1, d.size_quarantine());
}

int main(int argc, char **argv) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
