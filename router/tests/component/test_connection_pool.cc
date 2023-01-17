/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include <chrono>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "config_builder.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlxclient/xsession.h"
#include "router_component_test.h"
#include "router_test_helpers.h"
#include "socket_operations.h"  // socket_t
#include "tcp_port_pool.h"

using namespace std::chrono_literals;
using namespace std::string_literals;

namespace std {

// pretty printer for std::chrono::duration<>
template <class T, class R>
std::ostream &operator<<(std::ostream &os,
                         const std::chrono::duration<T, R> &duration) {
  return os << std::chrono::duration_cast<std::chrono::milliseconds>(duration)
                   .count()
            << "ms";
}

}  // namespace std

struct ConnectionPoolConfigParam {
  const char *test_name;

  std::vector<std::pair<std::string, std::string>> opts;

  std::function<void(const std::vector<std::string> &)> checker;
};

class ConnectionPoolConfigTest
    : public RouterComponentTest,
      public ::testing::WithParamInterface<ConnectionPoolConfigParam> {};

TEST_P(ConnectionPoolConfigTest, check) {
  mysql_harness::ConfigBuilder builder;

  const std::string section =
      builder.build_section("connection_pool", GetParam().opts);

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), section);

  // launch the router with the created configuration
  auto &router =
      launch_router({"-c", conf_file}, EXIT_FAILURE, true, false, -1ms);
  router.wait_for_exit();

  std::vector<std::string> lines;
  {
    std::istringstream ss{router.get_logfile_content()};

    std::string line;
    while (std::getline(ss, line, '\n')) {
      lines.push_back(std::move(line));
    }
  }

  GetParam().checker(lines);
}

const ConnectionPoolConfigParam connection_pool_config_param[] = {
    {"max_idle_server_connections_negative",
     {
         {"max_idle_server_connections", "-1"},

     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                              "option max_idle_server_connections in "
                              "[connection_pool] needs value "
                              "between 0 and 4294967295 inclusive, was '-1'")));
     }},
    {"max_idle_server_connections_hex",
     {
         {"max_idle_server_connections", "0x01"},

     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "option max_idle_server_connections in "
                       "[connection_pool] needs value "
                       "between 0 and 4294967295 inclusive, was '0x01'")));
     }},
    {"max_idle_server_connections_too_large",
     {
         {"max_idle_server_connections", "4294967296"},

     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(
           lines, ::testing::Contains(::testing::HasSubstr(
                      "option max_idle_server_connections in [connection_pool] "
                      "needs value "
                      "between 0 and 4294967295 inclusive, was '4294967296'")));
     }},
};

INSTANTIATE_TEST_SUITE_P(Spec, ConnectionPoolConfigTest,
                         ::testing::ValuesIn(connection_pool_config_param),
                         [](const auto &info) { return info.param.test_name; });

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
