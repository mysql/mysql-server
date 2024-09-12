/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <chrono>
#include <cstdint>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysql/harness/string_utils.h"  // split_string
#include "mysqlrouter/http_client.h"
#include "mysqlrouter/rest_client.h"
#include "rest_api_testutils.h"
#include "router/src/routing/tests/mysql_client.h"
#include "router_component_test.h"
#include "router_component_testutils.h"  // make_bad_connection
#include "stdx_expected_no_error.h"      // EXPECT_NO_ERROR

using namespace std::chrono_literals;

using testing::AllOf;
using testing::AnyOf;
using testing::HasSubstr;
using testing::Not;

std::ostream &operator<<(std::ostream &os, MysqlError e) {
  os << e.sql_state() << " (" << e.value() << ") " << e.message();
  return os;
}

struct Route {
  std::string_view client_ssl_mode;
  std::string_view server_ssl_mode;
  uint16_t bind_port;

  std::string route_name() const {
    return std::string(client_ssl_mode) + "__" + std::string(server_ssl_mode);
  }
};

class RoutingConnectionErrorTest : public RestApiComponentTest {
 protected:
  static std::string router_host() { return "127.0.0.1"; }
};

TEST_F(RoutingConnectionErrorTest, connect_successful) {
  auto rest_port = port_pool_.get_next_available();
  auto server_port = port_pool_.get_next_available();

  mock_server_spawner().spawn(
      mock_server_cmdline("my_port.js").port(server_port).args());

  std::array routes{
      Route{"PASSTHROUGH", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "PREFERRED", port_pool_.get_next_available()},
  };

  auto pwfile = create_password_file();

  auto writer = config_writer(conf_dir_.name());
  for (auto route : routes) {
    writer.section(
        "routing:" + route.route_name(),
        {
            {"bind_port", std::to_string(route.bind_port)},
            {"destinations", "127.0.0.1:" + std::to_string(server_port)},
            {"routing_strategy", "round-robin"},
            {"protocol", "classic"},
            {"max_connect_errors", "1"},
            {"client_ssl_mode", std::string(route.client_ssl_mode)},
            {"server_ssl_mode", std::string(route.server_ssl_mode)},
            {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
            {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
        });
  }

  writer
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
                                  {"bind_address", "127.0.0.1"},
                                  {"port", std::to_string(rest_port)},
                              });
  auto &r = router_spawner().spawn({"-c", writer.write()});

  IOContext io_ctx;
  RestClient rest_cli(io_ctx, router_host(), rest_port, kRestApiUsername,
                      kRestApiPassword);

  for (auto route : routes) {
    {
      MysqlClient cli;
      cli.username("username");
      cli.password("password");

      auto connect_res = cli.connect(router_host(), route.bind_port);
      EXPECT_NO_ERROR(connect_res);
    }

    // check for connect errors via REST API
    auto resp = rest_cli.request_sync(
        HttpMethod::Get,
        rest_api_basepath + "/routes/" + route.route_name() + "/status");

    EXPECT_EQ(resp.get_response_code(), 200) << resp.get_response_code_line();
    if (resp.get_response_code() == 200) {
      auto &http_buf = resp.get_input_buffer();
      auto buf = http_buf.pop_front(http_buf.length());

      rapidjson::Document json_doc;

      std::string_view sv(reinterpret_cast<char *>(buf.data()), buf.size());

      json_doc.Parse(sv.data(), sv.size());

      auto *blocked_hosts = rapidjson::Pointer("/blockedHosts").Get(json_doc);
      ASSERT_NE(blocked_hosts, nullptr);

      EXPECT_EQ(blocked_hosts->GetInt(), 0);
    }
  }

  SCOPED_TRACE("// shutdown router");
  r.send_shutdown_event();
  r.wait_for_exit();

  // the log should not contain "closed connection before ..."
  EXPECT_THAT(r.get_logfile_content(),
              Not(AnyOf(HasSubstr("closed connection before"),
                        HasSubstr("blocking client host for"),
                        HasSubstr("incrementing"))));
}

TEST_F(RoutingConnectionErrorTest, connect_backend_not_reachable) {
  auto server_port = port_pool_.get_next_available();
  auto rest_port = port_pool_.get_next_available();

  std::array routes{
      Route{"PASSTHROUGH", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "PREFERRED", port_pool_.get_next_available()},
  };

  auto pwfile = create_password_file();

  SCOPED_TRACE("// start router");
  auto writer = config_writer(conf_dir_.name());
  for (auto route : routes) {
    writer.section(
        "routing:" + route.route_name(),
        {
            {"bind_port", std::to_string(route.bind_port)},
            {"destinations", "127.0.0.1:" + std::to_string(server_port)},
            {"routing_strategy", "round-robin"},
            {"max_connect_errors", "1"},
            {"client_ssl_mode", std::string(route.client_ssl_mode)},
            {"server_ssl_mode", std::string(route.server_ssl_mode)},
            {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
            {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
        });
  }
  writer
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
                                  {"bind_address", "127.0.0.1"},
                                  {"port", std::to_string(rest_port)},
                              });
  auto &r = router_spawner().spawn({"-c", writer.write()});

  IOContext io_ctx;
  RestClient rest_cli(io_ctx, router_host(), rest_port, kRestApiUsername,
                      kRestApiPassword);

  SCOPED_TRACE("// connect should fail as we have no backend.");
  for (auto route : routes) {
    {
      MysqlClient cli;

      auto connect_res = cli.connect(router_host(), route.bind_port);
      EXPECT_ERROR(connect_res);
    }

    SCOPED_TRACE("// check for connect errors via REST API");

    auto resp = rest_cli.request_sync(
        HttpMethod::Get,
        rest_api_basepath + "/routes/" + route.route_name() + "/status");

    EXPECT_EQ(resp.get_response_code(), 200) << resp.get_response_code_line();

    if (resp.get_response_code() == 200) {
      auto &http_buf = resp.get_input_buffer();
      auto buf = http_buf.pop_front(http_buf.length());

      rapidjson::Document json_doc;

      std::string_view sv(reinterpret_cast<char *>(buf.data()), buf.size());

      json_doc.Parse(sv.data(), sv.size());

      auto *blocked_hosts = rapidjson::Pointer("/blockedHosts").Get(json_doc);
      ASSERT_NE(blocked_hosts, nullptr) << sv;

      EXPECT_EQ(blocked_hosts->GetInt(), 0);
    }
  }
  SCOPED_TRACE("// shutdown router");
  r.send_shutdown_event();
  r.wait_for_exit();

  // the log should not contain "closed connection before ..."
  EXPECT_THAT(r.get_logfile_content(),
              AllOf(HasSubstr("connecting to backend(s) for client"),
                    Not(AnyOf(HasSubstr("closed connection before"),
                              HasSubstr("blocking client host for"),
                              HasSubstr("incrementing")))));
}

TEST_F(RoutingConnectionErrorTest, connect_from_connection_pool) {
  auto router_port = port_pool_.get_next_available();
  auto rest_port = port_pool_.get_next_available();
  auto server_port = port_pool_.get_next_available();

  mock_server_spawner().spawn(
      mock_server_cmdline("my_port.js").port(server_port).args());

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
                                  {"bind_address", "127.0.0.1"},
                                  {"port", std::to_string(rest_port)},
                              });
  auto &r = router_spawner().spawn({"-c", writer.write()});

  {
    MysqlClient cli;  // first connection.
    cli.username("username");
    cli.password("password");

    auto connect_res = cli.connect(router_host(), router_port);
    EXPECT_NO_ERROR(connect_res);
  }

  using namespace std::chrono_literals;

  std::this_thread::sleep_for(100ms);

  {
    MysqlClient cli;  // from connection pool
    cli.username("username");
    cli.password("password");

    auto connect_res = cli.connect(router_host(), router_port);
    ASSERT_NO_ERROR(connect_res);
  }

  // check for connect errors via REST API
  IOContext io_ctx;
  RestClient rest_cli(io_ctx, router_host(), rest_port, kRestApiUsername,
                      kRestApiPassword);

  auto resp = rest_cli.request_sync(
      HttpMethod::Get, rest_api_basepath + "/routes/" + route_name + "/status");

  EXPECT_EQ(resp.get_response_code(), 200) << resp.get_response_code_line();

  if (resp.get_response_code() == 200) {
    auto &http_buf = resp.get_input_buffer();
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
              Not(AnyOf(HasSubstr("closed connection before"),
                        HasSubstr("blocking client host for"),
                        HasSubstr("incrementing"))));
}

TEST_F(RoutingConnectionErrorTest, connect_close_is_not_an_error) {
  auto server_port = port_pool_.get_next_available();

  mock_server_spawner().spawn(
      mock_server_cmdline("my_port.js").port(server_port).args());

  std::array routes{
      Route{"PASSTHROUGH", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "PREFERRED", port_pool_.get_next_available()},
  };

  SCOPED_TRACE("// start router");
  auto writer = config_writer(conf_dir_.name());
  for (auto route : routes) {
    writer.section(
        "routing:" + route.route_name(),
        {
            {"bind_address", "127.0.0.1"},
            {"bind_port", std::to_string(route.bind_port)},
            {"destinations", "127.0.0.1:" + std::to_string(server_port)},
            {"routing_strategy", "round-robin"},
            {"client_ssl_mode", std::string(route.client_ssl_mode)},
            {"server_ssl_mode", std::string(route.server_ssl_mode)},
            {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
            {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
        });
  }
  auto &r = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect+close should not cause a connect-error.");
  net::io_context io_ctx;
  for (auto route : routes) {
    net::ip::tcp::socket sock(io_ctx);

    net::ip::tcp::endpoint ep(net::ip::address_v4::loopback(), route.bind_port);
    EXPECT_NO_ERROR(sock.connect(ep));
  }

  SCOPED_TRACE("// shutdown router");
  r.send_shutdown_event();
  r.wait_for_exit();

  // the log should not contain "closed connection before ..."
  EXPECT_THAT(r.get_logfile_content(),
              Not(AnyOf(HasSubstr("closed connection before"),
                        HasSubstr("blocking client host for"),
                        HasSubstr("incrementing"))));
}

TEST_F(RoutingConnectionErrorTest, connect_recv_close_is_not_an_error) {
  auto server_port = port_pool_.get_next_available();

  mock_server_spawner().spawn(
      mock_server_cmdline("my_port.js").port(server_port).args());

  std::array routes{
      Route{"PASSTHROUGH", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "PREFERRED", port_pool_.get_next_available()},
  };

  SCOPED_TRACE("// start router");
  auto writer = config_writer(conf_dir_.name());

  for (auto route : routes) {
    writer.section(
        "routing:" + route.route_name(),
        {
            {"bind_address", "127.0.0.1"},
            {"bind_port", std::to_string(route.bind_port)},
            {"destinations", "127.0.0.1:" + std::to_string(server_port)},
            {"routing_strategy", "round-robin"},
            {"client_ssl_mode", std::string(route.client_ssl_mode)},
            {"server_ssl_mode", std::string(route.server_ssl_mode)},
            {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
            {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
        });
  }

  auto &r = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect+wait+close should not cause a connect-error.");
  net::io_context io_ctx;
  for (auto route : routes) {
    net::ip::tcp::socket sock(io_ctx);

    net::ip::tcp::endpoint ep(net::ip::address_v4::loopback(), route.bind_port);
    EXPECT_NO_ERROR(sock.connect(ep));

    // recv the server-greeting
    std::array<char, 1024> buf;
    EXPECT_NO_ERROR(
        net::read(sock, net::buffer(buf), net::transfer_at_least(1)));

    // and drop the connection.
  }

  SCOPED_TRACE("// shutdown router");
  r.send_shutdown_event();
  r.wait_for_exit();

  // the log should not contain "closed connection before ..."
  EXPECT_THAT(r.get_logfile_content(),
              Not(AnyOf(HasSubstr("closed connection before"),
                        HasSubstr("blocking client host for"),
                        HasSubstr("incrementing"))));
}

TEST_F(RoutingConnectionErrorTest, broken_client_greeting_is_an_error) {
  auto server_port = port_pool_.get_next_available();

  mock_server_spawner().spawn(
      mock_server_cmdline("my_port.js").port(server_port).args());

  std::array routes{
      Route{"PASSTHROUGH", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "PREFERRED", port_pool_.get_next_available()},
  };

  SCOPED_TRACE("// start router");
  auto writer = config_writer(conf_dir_.name());

  for (auto route : routes) {
    writer.section(
        "routing:" + route.route_name(),
        {
            {"bind_address", "127.0.0.1"},
            {"bind_port", std::to_string(route.bind_port)},
            {"destinations", "127.0.0.1:" + std::to_string(server_port)},
            {"routing_strategy", "round-robin"},
            {"client_ssl_mode", std::string(route.client_ssl_mode)},
            {"server_ssl_mode", std::string(route.server_ssl_mode)},
            {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
            {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
        });
  }

  auto &r = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect+wait+close should not cause a connect-error.");
  for (auto route : routes) {
    EXPECT_NO_FATAL_FAILURE(make_bad_connection(route.bind_port));
  }

  SCOPED_TRACE("// shutdown router");
  r.send_shutdown_event();
  r.wait_for_exit();

  std::string expected_substr("incrementing error counter for host");
  auto lines = mysql_harness::split_string(r.get_logfile_content(), '\n');

  size_t matches{};
  for (const auto &line : lines) {
    if (line.find(expected_substr) != std::string::npos) {
      ++matches;
    }
  }

  SCOPED_TRACE(
      "// the log should contain the 'incrementing error-count' once per "
      "route");
  EXPECT_EQ(matches, routes.size()) << expected_substr;
}

TEST_F(RoutingConnectionErrorTest, broken_client_greeting_seq_id_is_an_error) {
  auto server_port = port_pool_.get_next_available();

  mock_server_spawner().spawn(
      mock_server_cmdline("my_port.js").port(server_port).args());

  std::array routes{
      Route{"PASSTHROUGH", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "PREFERRED", port_pool_.get_next_available()},
  };

  SCOPED_TRACE("// start router");
  auto writer = config_writer(conf_dir_.name());

  for (auto route : routes) {
    writer.section(
        "routing:" + route.route_name(),
        {
            {"bind_address", "127.0.0.1"},
            {"bind_port", std::to_string(route.bind_port)},
            {"destinations", "127.0.0.1:" + std::to_string(server_port)},
            {"routing_strategy", "round-robin"},
            {"client_ssl_mode", std::string(route.client_ssl_mode)},
            {"server_ssl_mode", std::string(route.server_ssl_mode)},
            {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
            {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
        });
  }

  auto &r = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect+wait+close should not cause a connect-error.");
  net::io_context io_ctx;
  for (auto route : routes) {
    net::ip::tcp::socket sock(io_ctx);

    net::ip::tcp::endpoint ep(net::ip::address_v4::loopback(), route.bind_port);
    EXPECT_NO_ERROR(sock.connect(ep));

    // recv the server-greeting
    {
      std::array<char, 1024> buf{};
      EXPECT_NO_ERROR(
          net::read(sock, net::buffer(buf), net::transfer_at_least(1)));
    }

    {
      // send a valid client-greeting, with the wrong sequence-id
      std::array<char, 23> buf{23 - 4, 0x00, 0x00, 0x00,     // frame header
                               0x0d,   0x24,                 // caps
                               0,      0,    0,              // max-packet-size
                               'r',    'o',  'o',  't',  0,  // username
                               'H',    ']',  '^',  'C',  'S',
                               'V',    'Y',  '[',  '\0'};
      EXPECT_NO_ERROR(net::write(sock, net::buffer(buf), net::transfer_all()));
    }

    // recv the error.
    {
      std::array<char, 1024> buf{};
      auto read_res =
          net::read(sock, net::buffer(buf), net::transfer_at_least(1));
      ASSERT_NO_ERROR(read_res);

      std::string msg(buf.data(), buf.data() + read_res.value());
      EXPECT_THAT(msg, testing::HasSubstr("Got packets out of order"));
    }

    // and drop the connection.
  }

  SCOPED_TRACE("// shutdown router");
  r.send_shutdown_event();
  r.wait_for_exit();

  std::string expected_substr("incrementing error counter for host");
  auto lines = mysql_harness::split_string(r.get_logfile_content(), '\n');

  size_t matches{};
  for (const auto &line : lines) {
    if (line.find(expected_substr) != std::string::npos) {
      ++matches;
    }
  }

  SCOPED_TRACE(
      "// the log should contain the 'incrementing error-count' once per "
      "route");
  EXPECT_EQ(matches, routes.size()) << expected_substr;
}

TEST_F(RoutingConnectionErrorTest, auth_fail_is_not_an_connection_error) {
  auto server_port = port_pool_.get_next_available();

  mock_server_spawner().spawn(
      mock_server_cmdline("my_port.js").port(server_port).args());

  std::array routes{
      Route{"PASSTHROUGH", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "PREFERRED", port_pool_.get_next_available()},
  };

  SCOPED_TRACE("// start router");
  auto writer = config_writer(conf_dir_.name());

  for (auto route : routes) {
    writer.section(
        "routing:" + route.route_name(),
        {
            {"bind_address", "127.0.0.1"},
            {"bind_port", std::to_string(route.bind_port)},
            {"destinations", "127.0.0.1:" + std::to_string(server_port)},
            {"routing_strategy", "round-robin"},
            {"client_ssl_mode", std::string(route.client_ssl_mode)},
            {"server_ssl_mode", std::string(route.server_ssl_mode)},
            {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
            {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
        });
  }

  auto &r = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect+wait+close should not cause a connect-error.");
  net::io_context io_ctx;
  for (auto route : routes) {
    net::ip::tcp::socket sock(io_ctx);

    MysqlClient cli;  // first connection.
    cli.username("username");
    cli.password("wrong_password");

    auto connect_res = cli.connect(router_host(), route.bind_port);
    ASSERT_ERROR(connect_res);
    EXPECT_EQ(connect_res.error().value(), 1045) << connect_res.error();
  }

  SCOPED_TRACE("// shutdown router");
  r.send_shutdown_event();
  r.wait_for_exit();

  EXPECT_THAT(r.get_logfile_content(),
              Not(AnyOf(HasSubstr("closed connection before"),
                        HasSubstr("blocking client host for"),
                        HasSubstr("incrementing"))));
}

TEST_F(RoutingConnectionErrorTest, ssl_fail_is_not_an_connection_error) {
  auto server_port = port_pool_.get_next_available();

  mock_server_spawner().spawn(
      mock_server_cmdline("my_port.js").port(server_port).args());

  std::array routes{
      Route{"REQUIRED", "PREFERRED", port_pool_.get_next_available()},
  };

  SCOPED_TRACE("// start router");
  auto writer = config_writer(conf_dir_.name());

  for (auto route : routes) {
    writer.section(
        "routing:" + route.route_name(),
        {
            {"bind_address", "127.0.0.1"},
            {"bind_port", std::to_string(route.bind_port)},
            {"destinations", "127.0.0.1:" + std::to_string(server_port)},
            {"routing_strategy", "round-robin"},
            {"client_ssl_mode", std::string(route.client_ssl_mode)},
            {"server_ssl_mode", std::string(route.server_ssl_mode)},
            {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
            {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
        });
  }

  auto &r = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// connect+wait+close should not cause a connect-error.");
  for (auto route : routes) {
    MysqlClient cli;  // first connection.
    cli.username("username");
    cli.password("password");
    cli.set_option(MysqlClient::SslMode(SSL_MODE_DISABLED));

    auto connect_res = cli.connect(router_host(), route.bind_port);
    ASSERT_ERROR(connect_res);
    EXPECT_EQ(connect_res.error().value(), 2026) << connect_res.error();
  }

  SCOPED_TRACE("// shutdown router");
  r.send_shutdown_event();
  r.wait_for_exit();

  EXPECT_THAT(r.get_logfile_content(),
              Not(AnyOf(HasSubstr("closed connection before"),
                        HasSubstr("blocking client host for"),
                        HasSubstr("incrementing"))));
}

TEST_F(RoutingConnectionErrorTest, max_connect_errors) {
  const auto server_port = port_pool_.get_next_available();

  mock_server_spawner().spawn(
      mock_server_cmdline("my_port.js").port(server_port).args());

  std::array routes{
      Route{"PASSTHROUGH", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "PREFERRED", port_pool_.get_next_available()},
  };

  auto writer = config_writer(conf_dir_.name());
  for (auto route : routes) {
    writer.section(
        "routing:" + route.route_name(),
        {
            {"bind_port", std::to_string(route.bind_port)},
            {"destinations", "127.0.0.1:" + std::to_string(server_port)},
            {"routing_strategy", "round-robin"},
            {"protocol", "classic"},
            {"max_connect_errors", "1"},
            {"client_ssl_mode", std::string(route.client_ssl_mode)},
            {"server_ssl_mode", std::string(route.server_ssl_mode)},
            {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
            {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
        });
  }

  // launch the router
  auto &router = router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE("// trigger a connection-error");
  net::io_context io_ctx;
  for (auto route : routes) {
    net::ip::tcp::socket sock(io_ctx);

    net::ip::tcp::endpoint ep(net::ip::address_v4::loopback(), route.bind_port);
    EXPECT_NO_ERROR(sock.connect(ep));

    // recv the server-greeting
    {
      std::array<char, 1024> buf{};
      EXPECT_NO_ERROR(
          net::read(sock, net::buffer(buf), net::transfer_at_least(1)));
    }

    {
      // send a broken client-greeting.
      std::array buf{0x01, 0x00, 0x00, 0x01, 0xff};

      EXPECT_NO_ERROR(net::write(sock, net::buffer(buf), net::transfer_all()));
    }

    // recv the error.
    {
      std::array<char, 1024> buf{};
      auto read_res =
          net::read(sock, net::buffer(buf), net::transfer_at_least(1));
      ASSERT_NO_ERROR(read_res);

      std::string msg(buf.data(), buf.data() + read_res.value());
      EXPECT_THAT(msg, testing::HasSubstr("Bad handshake"));
    }

    // and drop the connection.
  }

  SCOPED_TRACE("// wait until 'blocking client host' appears in the log");
  ASSERT_TRUE(wait_log_contains(router, "blocking client host", 5000ms));

  for (auto route : routes) {
    // for the next connection attempt we should get an error as the
    // max_connect_errors was exceeded
    MysqlClient cli;
    cli.username("root");
    cli.password("fake-pass");

    auto connect_res = cli.connect("127.0.0.1", route.bind_port);
    ASSERT_ERROR(connect_res);
    EXPECT_EQ(connect_res.error().value(), 1129) << connect_res.error();
    EXPECT_THAT(connect_res.error().message(),
                ::testing::HasSubstr("Too many connection errors"));
  }
}

/**
 * @test
 * This test verifies that:
 *   1. Router will block a misbehaving client after consecutive
 *      <max_connect_errors> connection errors
 *   2. Router will reset its connection error counter if client establishes a
 *      successful connection before <max_connect_errors> threshold is hit
 */
TEST_F(RoutingConnectionErrorTest, error_counters) {
  const uint16_t server_port = port_pool_.get_next_available();

  // launch the server mock
  mock_server_spawner().spawn(
      mock_server_cmdline("my_port.js").port(server_port).args());

  std::array routes{
      // Route{"PASSTHROUGH", "AS_CLIENT", port_pool_.get_next_available()},
      // Route{"PREFERRED", "AS_CLIENT", port_pool_.get_next_available()},
      Route{"PREFERRED", "PREFERRED", port_pool_.get_next_available()},
  };

  auto writer = config_writer(conf_dir_.name());
  for (auto route : routes) {
    writer.section(
        "routing:" + route.route_name(),
        {
            {"bind_port", std::to_string(route.bind_port)},
            {"destinations", "127.0.0.1:" + std::to_string(server_port)},
            {"routing_strategy", "round-robin"},
            {"protocol", "classic"},
            {"max_connect_errors", "3"},
            {"client_ssl_mode", std::string(route.client_ssl_mode)},
            {"server_ssl_mode", std::string(route.server_ssl_mode)},
            {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"},
            {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"},
        });
  }

  // launch the router
  router_spawner().spawn({"-c", writer.write()});

  SCOPED_TRACE(
      "// make good and bad connections to check blocked client gets reset");
  for (auto route : routes) {
    // we loop just for good measure, to additionally test that this behaviour
    // is repeatable
    for (int i = 0; i < 5; i++) {
      // good connection, followed by 2 bad ones. Good one should reset the
      // error counter
      MysqlClient cli;
      cli.username("username");
      cli.password("password");

      auto connect_res = cli.connect("127.0.0.1", route.bind_port);
      ASSERT_NO_ERROR(connect_res);

      EXPECT_NO_FATAL_FAILURE(make_bad_connection(route.bind_port));
      EXPECT_NO_FATAL_FAILURE(make_bad_connection(route.bind_port));
    }

    SCOPED_TRACE("// make bad connection to trigger blocked client");
    // make a 3rd consecutive bad connection - it should cause Router to start
    // blocking us
    EXPECT_NO_FATAL_FAILURE(make_bad_connection(route.bind_port));

    // we loop just for good measure, to additionally test that this behaviour
    // is repeatable
    for (int i = 0; i < 5; i++) {
      // now trying to make a good connection should fail due to blockage
      //
      MysqlClient cli;
      cli.username("username");
      cli.password("password");

      SCOPED_TRACE("// make connection to check if we are really blocked");

      auto connect_res = cli.connect("127.0.0.1", route.bind_port);
      ASSERT_ERROR(connect_res);
      EXPECT_EQ(connect_res.error().value(), 1129) << connect_res.error();

      EXPECT_THAT(connect_res.error().message(),
                  ::testing::HasSubstr("Too many connection errors"));
    }
  }
}

int main(int argc, char *argv[]) {
  net::impl::socket::init();

  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
