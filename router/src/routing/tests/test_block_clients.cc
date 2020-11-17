/*
  Copyright (c) 2015, 2020, Oracle and/or its affiliates.

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

#include <cstdio>
#include <fstream>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include "mysql/harness/net_ts/io_context.h"
#ifndef _WIN32
#include <netinet/in.h>
#else
#include <WinSock2.h>
#endif

#include <gmock/gmock.h>

#include "../../router/src/router_app.h"
#include "../../routing/src/mysql_routing.h"
#include "../../routing/src/utils.h"
#include "gtest_consoleoutput.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/plugin.h"
#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/routing.h"
#include "router_test_helpers.h"
#include "test/helpers.h"

using namespace std::chrono_literals;

using mysql_harness::Path;
using std::string;
using ::testing::ContainerEq;
using ::testing::HasSubstr;
using ::testing::StrEq;

string g_cwd;
Path g_origin;

class TestBlockClients : public ConsoleOutputTest {
 protected:
  void SetUp() override {
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
  }

  net::io_context io_ctx_;
};

TEST_F(TestBlockClients, BlockClientHost) {
  unsigned long long max_connect_errors = 2;
  auto client_connect_timeout = 2s;

  auto ipv6_1_res = net::ip::make_address("::1");
  ASSERT_TRUE(ipv6_1_res);
  auto ipv6_2_res = net::ip::make_address("::2");
  ASSERT_TRUE(ipv6_2_res);

  auto ipv6_1 = net::ip::tcp::endpoint(ipv6_1_res.value(), 0);
  auto ipv6_2 = net::ip::tcp::endpoint(ipv6_2_res.value(), 0);

  MySQLRouting r(
      io_ctx_, routing::RoutingStrategy::kNextAvailable, 7001,
      Protocol::Type::kClassicProtocol, routing::AccessMode::kReadWrite,
      "127.0.0.1", mysql_harness::Path(), "routing:connect_erros", 1,
      std::chrono::seconds(1), max_connect_errors, client_connect_timeout);

  ASSERT_FALSE(r.get_context().block_client_host<net::ip::tcp>(ipv6_1));
  ASSERT_THAT(get_log_stream().str(),
              HasSubstr("1 connection errors for ::1 (max 2)"));
  reset_ssout();
  ASSERT_TRUE(r.get_context().block_client_host<net::ip::tcp>(ipv6_1));
  ASSERT_THAT(get_log_stream().str(), HasSubstr("blocking client host ::1"));

  auto blocked_hosts = r.get_context().get_blocked_client_hosts();
  ASSERT_GE(blocked_hosts.size(), 1u);
  //  ASSERT_THAT(blocked_hosts[0], ContainerEq(client_ip_array1));

  ASSERT_FALSE(r.get_context().block_client_host<net::ip::tcp>(ipv6_2));
  ASSERT_TRUE(r.get_context().block_client_host<net::ip::tcp>(ipv6_2));

  blocked_hosts = r.get_context().get_blocked_client_hosts();
  //  ASSERT_THAT(blocked_hosts[0], ContainerEq(client_ip_array1));
  //  ASSERT_THAT(blocked_hosts[1], ContainerEq(client_ip_array2));
}

TEST_F(TestBlockClients, BlockClientHostWithFakeResponse) {
  unsigned long long max_connect_errors = 2;
  auto client_connect_timeout = 2s;

  auto ipv6_1_res = net::ip::make_address("::1");
  ASSERT_TRUE(ipv6_1_res);

  auto ipv6_1 = net::ip::tcp::endpoint(ipv6_1_res.value(), 0);

  MySQLRouting r(
      io_ctx_, routing::RoutingStrategy::kNextAvailable, 7001,
      Protocol::Type::kClassicProtocol, routing::AccessMode::kReadWrite,
      "127.0.0.1", mysql_harness::Path(), "routing:connect_erros", 1,
      std::chrono::seconds(1), max_connect_errors, client_connect_timeout);

  std::FILE *fd_response = std::fopen("fake_response.data", "w");
  ASSERT_NE(fd_response, nullptr);

  ASSERT_FALSE(r.get_context().block_client_host<net::ip::tcp>(
      ipv6_1, fileno(fd_response)));
  std::fclose(fd_response);
#ifndef _WIN32
  // block_client_host() will not be able to write data to the file because in
  // windows, the syscall to writing to sockets is different than for files
  fd_response = std::fopen("fake_response.data", "r");
  ASSERT_NE(fd_response, nullptr);

  const auto fake_response = mysql_protocol::HandshakeResponsePacket(
      1, {}, "ROUTER", "", "fake_router_login");

  std::vector<uint8_t> written_data;
  for (;;) {
    auto c = std::fgetc(fd_response);
    if (c == EOF) break;

    written_data.push_back(c);
  }

  EXPECT_EQ(written_data, fake_response.message());

  std::fclose(fd_response);
#endif
  std::remove("fake_response.data");
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin = Path(argv[0]).dirname();
  g_cwd = Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);

  init_test_logger();
  return RUN_ALL_TESTS();
}
