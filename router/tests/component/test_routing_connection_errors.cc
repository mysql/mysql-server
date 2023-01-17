/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <thread>

#include "mysql/harness/stdx/expected_ostream.h"
#include "mysqlrouter/http_client.h"
#include "mysqlrouter/http_request.h"
#include "mysqlrouter/rest_client.h"
#include "rest_api_testutils.h"
#include "router/src/routing/tests/mysql_client.h"
#include "router_component_test.h"
#include "router_test_helpers.h"  // init_windows_sockets
#include "test/temp_directory.h"

class RoutingConnectionErrorTest : public RestApiComponentTest {
 protected:
  static std::string router_host() { return "127.0.0.1"; }
};

TEST_F(RoutingConnectionErrorTest, connect_successful) {
  auto router_port = port_pool_.get_next_available();
  auto rest_port = port_pool_.get_next_available();
  auto server_port = port_pool_.get_next_available();

  launch_mysql_server_mock(get_data_dir().join("my_port.js").str(),
                           server_port);

  std::string route_name = "under_test";

  auto pwfile = create_password_file();

  auto writer = config_writer(conf_dir_.name());
  writer
      .section("routing:" + route_name,
               {
                   {"bind_port", std::to_string(router_port)},
                   {"destinations", "127.0.0.1:" + std::to_string(server_port)},
                   {"routing_strategy", "round-robin"},
                   {"protocol", "classic"},
                   {"max_connect_errors", "1"},
               })
      .section("rest_routing",
               {
                   {"require_realm", "somerealm"},
               })
      .section("http_auth_realm:somerealm",
               {
                   {"backend", "somebackend"},
                   {"method", "basic"},
                   {"name", "Some Realm"},
               })
      .section("http_auth_backend:somebackend",
               {
                   {"backend", "file"},
                   {"filename", pwfile},
               })
      .section("http_server", {
                                  {"port", std::to_string(rest_port)},
                              });
  auto &r = router_spawner().spawn({"-c", writer.write()});

  {
    MysqlClient cli;

    auto connect_res = cli.connect(router_host(), router_port);
    EXPECT_TRUE(connect_res) << connect_res.error().message();
  }

  // check for connect errors via REST API
  IOContext io_ctx;
  RestClient rest_cli(io_ctx, router_host(), rest_port, kRestApiUsername,
                      kRestApiPassword);

  auto resp = rest_cli.request_sync(
      HttpMethod::Get, rest_api_basepath + "/routes/" + route_name + "/status");

  EXPECT_EQ(resp.get_response_code(), 200) << resp.get_response_code_line();
  if (resp.get_response_code() == 200) {
    auto http_buf = resp.get_input_buffer();
    auto buf = http_buf.pop_front(http_buf.length());

    rapidjson::Document json_doc;

    std::string_view sv(reinterpret_cast<char *>(buf.data()), buf.size());

    json_doc.Parse(sv.data(), sv.size());

    auto *blocked_hosts = rapidjson::Pointer("/blockedHosts").Get(json_doc);
    ASSERT_NE(blocked_hosts, nullptr);

    EXPECT_EQ(blocked_hosts->GetInt(), 0);
  }

  SCOPED_TRACE("// shutdown router");
  r.send_shutdown_event();
  r.wait_for_exit();

  // the log should not contain "closed connection before ..."
  EXPECT_THAT(r.get_logfile_content(),
              ::testing::Not(::testing::AnyOf(
                  ::testing::HasSubstr("closed connection before"),
                  ::testing::HasSubstr("blocking client host for"))));
}

TEST_F(RoutingConnectionErrorTest, connect_backend_not_reachable) {
  auto server_port = port_pool_.get_next_available();
  auto router_port = port_pool_.get_next_available();
  auto rest_port = port_pool_.get_next_available();

  std::string route_name = "under_test";

  auto pwfile = create_password_file();

  SCOPED_TRACE("// start router");
  auto writer = config_writer(conf_dir_.name());
  writer
      .section("routing:" + route_name,
               {
                   {"bind_port", std::to_string(router_port)},
                   {"destinations", "127.0.0.1:" + std::to_string(server_port)},
                   {"routing_strategy", "round-robin"},
                   {"max_connect_errors", "1"},
               })
      .section("rest_routing",
               {
                   {"require_realm", "somerealm"},
               })
      .section("http_auth_realm:somerealm",
               {
                   {"backend", "somebackend"},
                   {"method", "basic"},
                   {"name", "Some Realm"},
               })
      .section("http_auth_backend:somebackend",
               {
                   {"backend", "file"},
                   {"filename", pwfile},
               })
      .section("http_server", {
                                  {"port", std::to_string(rest_port)},
                              });
  auto &r = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect should fail as we have no backend.");
  MysqlClient cli;

  auto connect_res = cli.connect(router_host(), router_port);
  EXPECT_FALSE(connect_res);

  SCOPED_TRACE("// check for connect errors via REST API");
  IOContext io_ctx;
  RestClient rest_cli(io_ctx, router_host(), rest_port, kRestApiUsername,
                      kRestApiPassword);

  auto resp = rest_cli.request_sync(
      HttpMethod::Get, rest_api_basepath + "/routes/" + route_name + "/status");

  EXPECT_EQ(resp.get_response_code(), 200) << resp.get_response_code_line();

  if (resp.get_response_code() == 200) {
    auto http_buf = resp.get_input_buffer();
    auto buf = http_buf.pop_front(http_buf.length());

    rapidjson::Document json_doc;

    std::string_view sv(reinterpret_cast<char *>(buf.data()), buf.size());

    json_doc.Parse(sv.data(), sv.size());

    auto *blocked_hosts = rapidjson::Pointer("/blockedHosts").Get(json_doc);
    ASSERT_NE(blocked_hosts, nullptr) << sv;

    EXPECT_EQ(blocked_hosts->GetInt(), 0);
  }

  SCOPED_TRACE("// shutdown router");
  r.send_shutdown_event();
  r.wait_for_exit();

  // the log should not contain "closed connection before ..."
  EXPECT_THAT(
      r.get_logfile_content(),
      ::testing::AllOf(::testing::HasSubstr("connecting to backend failed"),
                       ::testing::Not(::testing::AnyOf(
                           ::testing::HasSubstr("closed connection before"),
                           ::testing::HasSubstr("blocking client host for")))));
}

TEST_F(RoutingConnectionErrorTest, connect_from_connection_pool) {
  auto router_port = port_pool_.get_next_available();
  auto rest_port = port_pool_.get_next_available();
  auto server_port = port_pool_.get_next_available();

  launch_mysql_server_mock(get_data_dir().join("my_port.js").str(),
                           server_port);

  std::string route_name = "under_test";

  auto pwfile = create_password_file();

  auto writer = config_writer(conf_dir_.name());
  writer
      .section("routing:" + route_name,
               {
                   {"bind_port", std::to_string(router_port)},
                   {"destinations", "127.0.0.1:" + std::to_string(server_port)},
                   {"routing_strategy", "round-robin"},
                   {"protocol", "classic"},
                   {"max_connect_errors", "1"},
                   {"client_ssl_mode", "DISABLED"},
                   {"server_ssl_mode", "DISABLED"},
               })
      .section("connection_pool",
               {
                   {"max_idle_server_connections", "1"},
               })
      .section("rest_routing",
               {
                   {"require_realm", "somerealm"},
               })
      .section("http_auth_realm:somerealm",
               {
                   {"backend", "somebackend"},
                   {"method", "basic"},
                   {"name", "Some Realm"},
               })
      .section("http_auth_backend:somebackend",
               {
                   {"backend", "file"},
                   {"filename", pwfile},
               })
      .section("http_server", {
                                  {"port", std::to_string(rest_port)},
                              });
  auto &r = router_spawner().spawn({"-c", writer.write()});

  {
    MysqlClient cli;  // first connection.

    auto connect_res = cli.connect(router_host(), router_port);
    EXPECT_TRUE(connect_res) << connect_res.error().message();
  }

  using namespace std::chrono_literals;

  std::this_thread::sleep_for(100ms);

  {
    MysqlClient cli;  // from connection pool

    auto connect_res = cli.connect(router_host(), router_port);
    EXPECT_FALSE(connect_res);
    if (connect_res) {
      // change-user is issued by the router when it takes the connection from
      // the pool.
      //
      // the mock server doesn't support change-user (command 17) yet.
      EXPECT_EQ(connect_res.error().message(), "Unsupported command: 17");
    }
  }

  // check for connect errors via REST API
  IOContext io_ctx;
  RestClient rest_cli(io_ctx, router_host(), rest_port, kRestApiUsername,
                      kRestApiPassword);

  auto resp = rest_cli.request_sync(
      HttpMethod::Get, rest_api_basepath + "/routes/" + route_name + "/status");

  EXPECT_EQ(resp.get_response_code(), 200) << resp.get_response_code_line();

  if (resp.get_response_code() == 200) {
    auto http_buf = resp.get_input_buffer();
    auto buf = http_buf.pop_front(http_buf.length());

    rapidjson::Document json_doc;

    std::string_view sv(reinterpret_cast<char *>(buf.data()), buf.size());

    json_doc.Parse(sv.data(), sv.size());

    auto *blocked_hosts = rapidjson::Pointer("/blockedHosts").Get(json_doc);
    ASSERT_NE(blocked_hosts, nullptr);

    EXPECT_EQ(blocked_hosts->GetInt(), 0);
  }

  SCOPED_TRACE("// shutdown router");
  r.send_shutdown_event();
  r.wait_for_exit();

  // the log should not contain "closed connection before ..."
  EXPECT_THAT(r.get_logfile_content(),
              ::testing::Not(::testing::AnyOf(
                  ::testing::HasSubstr("closed connection before"),
                  ::testing::HasSubstr("blocking client host for"))));
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
