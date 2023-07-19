/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "config_builder.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlxclient/xsession.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_test_helpers.h"
#include "stdx_expected_no_error.h"
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

using mysql_harness::ConfigBuilder;
using mysqlrouter::MySQLSession;

class RouterRoutingTest : public RouterComponentTest {
 public:
  std::string get_static_routing_section(
      const std::string &name, uint16_t bind_port, uint16_t server_port,
      const std::string &protocol,
      const std::vector<ConfigBuilder::kv_type> &custom_settings = {}) {
    std::vector<ConfigBuilder::kv_type> options{
        {"bind_port", std::to_string(bind_port)},
        {"mode", "read-write"},
        {"destinations", "127.0.0.1:" + std::to_string(server_port)},
        {"protocol", protocol}};

    for (const auto &s : custom_settings) {
      options.push_back(s);
    }

    return mysql_harness::ConfigBuilder::build_section("routing:"s + name,
                                                       options);
  }
};

using XProtocolSession = std::shared_ptr<xcl::XSession>;

static xcl::XError make_x_connection(XProtocolSession &session,
                                     const std::string &host,
                                     const uint16_t port,
                                     const std::string &username,
                                     const std::string &password,
                                     int64_t connect_timeout = 10000 /*10s*/) {
  session = xcl::create_session();
  xcl::XError err;

  err = session->set_mysql_option(
      xcl::XSession::Mysqlx_option::Authentication_method, "FROM_CAPABILITIES");
  if (err) return err;

  err = session->set_mysql_option(xcl::XSession::Mysqlx_option::Ssl_mode,
                                  "PREFERRED");
  if (err) return err;

  err = session->set_mysql_option(
      xcl::XSession::Mysqlx_option::Session_connect_timeout, connect_timeout);
  if (err) return err;

  err = session->set_mysql_option(xcl::XSession::Mysqlx_option::Connect_timeout,
                                  connect_timeout);
  if (err) return err;

  return session->connect(host.c_str(), port, username.c_str(),
                          password.c_str(), "");
}

TEST_F(RouterRoutingTest, RoutingOk) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // use the json file that adds additional rows to the metadata to increase the
  // packet size to +10MB to verify routing of the big packets
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();
  TempDirectory bootstrap_dir;

  // launch the server mock for bootstrapping
  launch_mysql_server_mock(
      json_stmts, server_port, EXIT_SUCCESS,
      false /*expecting huge data, can't print on the console*/);

  const std::string routing_section =
      "[routing:basic]\n"
      "bind_port = " +
      std::to_string(router_port) +
      "\n"
      "mode = read-write\n"
      "destinations = 127.0.0.1:" +
      std::to_string(server_port) + "\n";

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), routing_section);

  // launch the router with simple static routing configuration
  /*auto &router_static =*/launch_router({"-c", conf_file});

  // launch another router to do the bootstrap connecting to the mock server
  // via first router instance
  auto &router_bootstrapping = launch_router(
      {
          "--bootstrap=localhost:" + std::to_string(router_port),
          "--report-host",
          "dont.query.dns",
          "-d",
          bootstrap_dir.name(),
      },
      EXIT_SUCCESS, true, false, -1s,
      RouterComponentBootstrapTest::kBootstrapOutputResponder);

  ASSERT_NO_FATAL_FAILURE(check_exit_code(router_bootstrapping, EXIT_SUCCESS));

  ASSERT_TRUE(router_bootstrapping.expect_output(
      "MySQL Router configured for the InnoDB Cluster 'mycluster'"));
}

struct ConnectTimeoutTestParam {
  std::chrono::seconds expected_connect_timeout;
  std::string config_file_timeout;
  std::vector<std::string> command_line_params;
};

class RouterRoutingConnectTimeoutTest
    : public RouterRoutingTest,
      public ::testing::WithParamInterface<ConnectTimeoutTestParam> {};

/**
 * check connect-timeout is honored.
 */
TEST_P(RouterRoutingConnectTimeoutTest, ConnectTimeout) {
  const auto router_port = port_pool_.get_next_available();

  const auto client_connect_timeout = 10s;

  // the test requires a address:port which is not responding to SYN packets:
  //
  // - all the TEST-NET-* return "network not reachable" right away.
  // - RFC2606 defines example.org and its TCP port 81 is currently blocking
  // packets (which is what this test needs)
  //
  // if there is no DNS or no network, the test may fail.

  SCOPED_TRACE("// build router config with connect_timeout=" +
               GetParam().config_file_timeout);

  std::vector<std::pair<std::string, std::string>> routing_section_options{
      {"bind_port", std::to_string(router_port)},
      {"mode", "read-write"},
      {"destinations", "example.org:81"}};

  if (!GetParam().config_file_timeout.empty()) {
    routing_section_options.emplace_back("connect_timeout",
                                         GetParam().config_file_timeout);
  }

  const auto routing_section = mysql_harness::ConfigBuilder::build_section(
      "routing:timeout", routing_section_options);

  std::string conf_file =
      create_config_file(get_test_temp_dir_name(), routing_section);

  std::vector<std::string> cmdline = {{"-c", conf_file}};

  cmdline.insert(cmdline.end(), GetParam().command_line_params.begin(),
                 GetParam().command_line_params.end());

  // launch the router with simple static routing configuration
  /*auto &router_static =*/launch_router(cmdline);

  SCOPED_TRACE("// connect and trigger a timeout in the router");
  mysqlrouter::MySQLSession sess;

  using clock_type = std::chrono::steady_clock;

  const auto start = clock_type::now();
  try {
    sess.connect("127.0.0.1", router_port, "user", "pass", "", "",
                 client_connect_timeout.count());
    FAIL() << "expected connect fail.";
  } catch (const MySQLSession::Error &e) {
    EXPECT_EQ(e.code(), 2003) << e.what();
    EXPECT_THAT(e.what(),
                ::testing::HasSubstr("Can't connect to remote MySQL server"))
        << e.what();
  } catch (...) {
    FAIL() << "expected connect fail with a mysql-error";
  }
  const auto end = clock_type::now();

  // check the wait was long enough, but not too long.
  EXPECT_GE(end - start, GetParam().expected_connect_timeout);
  EXPECT_LT(end - start, GetParam().expected_connect_timeout + 5s);
}

INSTANTIATE_TEST_SUITE_P(
    ConnectTimeout, RouterRoutingConnectTimeoutTest,
    ::testing::Values(ConnectTimeoutTestParam{1s, "1", {}},
                      ConnectTimeoutTestParam{
                          1s, "1", {"--DEFAULT.connect_timeout=10"}},
                      ConnectTimeoutTestParam{
                          1s, "10", {"--routing:timeout.connect_timeout=1"}}));

/**
 * check connect-timeout doesn't block shutdown.
 */
TEST_F(RouterRoutingTest, ConnectTimeoutShutdownEarly) {
  const auto router_port = port_pool_.get_next_available();
  // we use the same long timeout for client and endpoint side
  const auto connect_timeout = 10s;

  // the test requires a address:port which is not responding to SYN packets:
  //
  // - all the TEST-NET-* return "network not reachable" right away.
  // - RFC2606 defines example.org and its TCP port 81 is currently blocking
  // packets (which is what this test needs)
  //
  // if there is no DNS or no network, the test may fail.

  SCOPED_TRACE("// build router config with connect_timeout=" +
               std::to_string(connect_timeout.count()));
  const auto routing_section = mysql_harness::ConfigBuilder::build_section(
      "routing:timeout",
      {{"bind_port", std::to_string(router_port)},
       {"mode", "read-write"},
       {"connect_timeout", std::to_string(connect_timeout.count())},
       {"destinations", "example.org:81"}});

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), routing_section);

  // launch the router with simple static routing configuration
  auto &router = launch_router({"-c", conf_file});
  using clock_type = std::chrono::steady_clock;

  // initiate a connection attempt in a separate thread
  std::thread connect_thread([&]() {
    try {
      mysqlrouter::MySQLSession sess;
      sess.connect("127.0.0.1", router_port, "user", "pass", "", "",
                   connect_timeout.count());
      FAIL() << "expected connect fail.";
    } catch (const MySQLSession::Error &e) {
      EXPECT_THAT(e.code(),
                  ::testing::AnyOf(::testing::Eq(2003), ::testing::Eq(2013)));

      EXPECT_THAT(e.what(),
                  ::testing::AnyOf(::testing::HasSubstr("Lost connection"),
                                   ::testing::HasSubstr(
                                       "Error connecting to MySQL server")));
    } catch (...) {
      FAIL() << "expected connect fail with a mysql-error";
    }
  });

  const auto start = clock_type::now();
  // give the connect thread chance to initiate the connection, even if it
  // sometimes does not it should be fine, we just test a different scenario
  // then
  std::this_thread::sleep_for(200ms);
  // now force shutdown the router
  const auto kill_res = router.kill();
  EXPECT_EQ(0, kill_res);

  const auto end = clock_type::now();

  // it should take much less time than connect_timeout which is 10s
  EXPECT_LT(end - start, 5s);

  connect_thread.join();
}

/**
 * check that the connection timeout Timer gets canceled after the connection
 * and does not lead to Router crash when the connection object has been
 * released
 */
TEST_F(RouterRoutingTest, ConnectTimeoutTimerCanceledCorrectly) {
  const auto router_port = port_pool_.get_next_available();
  const auto server_port = port_pool_.get_next_available();
  const auto connect_timeout = 1s;

  // launch the server mock
  const std::string json_stmts = get_data_dir().join("my_port.js").str();
  launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS);

  SCOPED_TRACE("// build router config with connect_timeout=" +
               std::to_string(connect_timeout.count()));
  const auto routing_section = mysql_harness::ConfigBuilder::build_section(
      "routing:timeout",
      {{"bind_port", std::to_string(router_port)},
       {"mode", "read-write"},
       {"connect_timeout", std::to_string(connect_timeout.count())},
       {"destinations", "127.0.0.1:" + std::to_string(server_port)}});

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), routing_section);

  // launch the router with simple static routing configuration
  launch_router({"-c", conf_file}, EXIT_SUCCESS);

  // make the connection and close it right away
  { auto con = make_new_connection_ok(router_port, server_port); }

  // wait longer than connect timeout, the process manager will check at exit
  // that the Router exits cleanly
  std::this_thread::sleep_for(2 * connect_timeout);
}

/**
 * check connect-timeout doesn't block shutdown when using x-protocol
 * connection.
 */
TEST_F(RouterRoutingTest, ConnectTimeoutShutdownEarlyXProtocol) {
  const auto router_port = port_pool_.get_next_available();
  // we use the same long timeout for client and endpoint side
  const auto connect_timeout = 10s;

  SCOPED_TRACE("// build router config with connect_timeout=" +
               std::to_string(connect_timeout.count()));
  const auto routing_section = mysql_harness::ConfigBuilder::build_section(
      "routing:timeout",
      {{"bind_port", std::to_string(router_port)},
       {"mode", "read-write"},
       {"connect_timeout", std::to_string(connect_timeout.count())},
       {"protocol", "x"},
       {"destinations", "example.org:81"}});

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), routing_section);

  // launch the router with simple static routing configuration
  auto &router = launch_router({"-c", conf_file});
  using clock_type = std::chrono::steady_clock;

  // initiate a connection attempt in a separate thread
  std::thread connect_thread([&]() {
    XProtocolSession x_session;

    const auto res =
        make_x_connection(x_session, "127.0.0.1", router_port, "user", "pass",
                          connect_timeout.count() * 1000);

    EXPECT_THAT(res.error(),
                ::testing::AnyOf(::testing::Eq(2006), ::testing::Eq(2002)));
    EXPECT_THAT(res.what(),
                ::testing::AnyOf(
                    ::testing::HasSubstr("MySQL server has gone away"),
                    ::testing::HasSubstr("Connection refused connecting to")));
  });

  const auto start = clock_type::now();
  // give the connect thread chance to initiate the connection, even if it
  // sometimes does not it should be fine, we just test a different scenario
  // then
  std::this_thread::sleep_for(200ms);
  // now force shutdown the router
  const auto kill_res = router.kill();
  EXPECT_EQ(0, kill_res);

  const auto end = clock_type::now();

  // it should take much less time than connect_timeout which is 10s
  EXPECT_LT(end - start, 5s);

  connect_thread.join();
}

TEST_F(RouterRoutingTest, EccCertificate) {
  RecordProperty("Bug", "35317484");
  RecordProperty("Description",
                 "Check if router can start with a ECC certificate");

  const auto server_classic_port = port_pool_.get_next_available();
  const auto server_x_port = port_pool_.get_next_available();
  const auto router_classic_ecdh_rsa_port = port_pool_.get_next_available();
  const auto router_classic_ecdh_dsa_port = port_pool_.get_next_available();
  const auto router_classic_ecdsa_port = port_pool_.get_next_available();

  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  launch_mysql_server_mock(json_stmts, server_classic_port, EXIT_SUCCESS, false,
                           /*http_port*/ 0, server_x_port);

  TempDirectory conf_dir("conf-ecc-certificate");
  auto writer = config_writer(conf_dir.name());
  writer.section(
      "routing:classic_ecdh_rsa",
      {
          {"bind_port", std::to_string(router_classic_ecdh_rsa_port)},
          {"mode", "read-write"},
          {"destinations", "127.0.0.1:" + std::to_string(server_classic_port)},
          {"routing_strategy", "round-robin"},
          {"protocol", "classic"},
          {"client_ssl_key",
           SSL_TEST_DATA_DIR "/ecdh_rsa_certs/server-key.pem"},
          {"client_ssl_cert",
           SSL_TEST_DATA_DIR "/ecdh_rsa_certs/server-cert.pem"},
      });
  writer.section(
      "routing:classic_ecdh_dsa",
      {
          {"bind_port", std::to_string(router_classic_ecdh_dsa_port)},
          {"mode", "read-write"},
          {"destinations", "127.0.0.1:" + std::to_string(server_classic_port)},
          {"routing_strategy", "round-robin"},
          {"protocol", "classic"},
          {"client_ssl_key",
           SSL_TEST_DATA_DIR "/ecdh_dsa_certs/server-key.pem"},
          {"client_ssl_cert",
           SSL_TEST_DATA_DIR "/ecdh_dsa_certs/server-cert.pem"},
      });
  writer.section(
      "routing:classic_ecdsa",
      {
          {"bind_port", std::to_string(router_classic_ecdsa_port)},
          {"mode", "read-write"},
          {"destinations", "127.0.0.1:" + std::to_string(server_classic_port)},
          {"routing_strategy", "round-robin"},
          {"protocol", "classic"},
          {"client_ssl_key", SSL_TEST_DATA_DIR "/ecdsa_certs/server-key.pem"},
          {"client_ssl_cert", SSL_TEST_DATA_DIR "/ecdsa_certs/server-cert.pem"},
      });
  ASSERT_NO_FATAL_FAILURE(router_spawner().spawn({"-c", writer.write()}));

  {
    mysqlrouter::MySQLSession client;
    EXPECT_NO_THROW(client.connect("127.0.0.1", router_classic_ecdh_rsa_port,
                                   "root", "fake-pass", "", ""));
  }

  {
    mysqlrouter::MySQLSession client;
    EXPECT_NO_THROW(client.connect("127.0.0.1", router_classic_ecdh_dsa_port,
                                   "root", "fake-pass", "", ""));
  }

  {
    mysqlrouter::MySQLSession client;
    EXPECT_NO_THROW(client.connect("127.0.0.1", router_classic_ecdsa_port,
                                   "root", "fake-pass", "", ""));
  }
}

/**
 * check empty packet leads to an error.
 *
 * - Bug#33240637 crash when empty packet is sent in first handshake packet
 */
TEST_F(RouterRoutingTest, XProtoHandshakeEmpty) {
  const auto server_classic_port = port_pool_.get_next_available();
  const auto server_x_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // doesn't really matter which file we use here, we are not going to do any
  // queries
  const std::string json_stmts =
      get_data_dir().join("handshake_too_many_con_error.js").str();

  // launch the server mock
  launch_mysql_server_mock(json_stmts, server_classic_port, EXIT_SUCCESS, false,
                           0, server_x_port);

  const auto routing_section = mysql_harness::ConfigBuilder::build_section(
      "routing:xproto",
      {{"bind_port", std::to_string(router_port)},
       {"mode", "read-write"},
       {"protocol", "x"},
       {"destinations", "127.0.0.1:" + std::to_string(server_x_port)}});

  const std::string conf_file =
      create_config_file(get_test_temp_dir_name(), routing_section);

  // launch the router with simple static routing configuration
  /*auto &router_static =*/launch_router({"-c", conf_file});

  SCOPED_TRACE("// connect to router");

  net::io_context io_ctx;
  net::ip::tcp::socket router_sock{io_ctx};

  net::ip::tcp::endpoint router_ep{net::ip::address_v4::loopback(),
                                   router_port};

  EXPECT_NO_ERROR(router_sock.connect(router_ep));
  EXPECT_NO_ERROR(router_sock.write_some(net::buffer("\x00\x00\x00\x00")));

  // shutdown the send side to signal a TCP-FIN.
  EXPECT_NO_ERROR(router_sock.shutdown(net::socket_base::shutdown_send));

  // wait for the server side close to ensure the it received the empty packet.
  {
    // a notify.
    std::vector<uint8_t> recv_buf;
    auto read_res = net::read(router_sock, net::dynamic_buffer(recv_buf));
    if (read_res) {
      // may return a Notice
      ASSERT_NO_ERROR(read_res);
      EXPECT_THAT(recv_buf, ::testing::SizeIs(
                                ::testing::Ge(4 + 7)));  // notify (+ error-msg)

      // read more ... which should be EOF
      read_res = net::read(router_sock, net::dynamic_buffer(recv_buf));
      // the read will either block until the socket is closed or succeed.
    }
    EXPECT_THAT(read_res, ::testing::AnyOf(::testing::Eq(
                              stdx::make_unexpected(net::stream_errc::eof))));
  }
}

class RouterMaxConnectionsTest : public RouterRoutingTest {
 public:
  bool make_new_connection(uint16_t port,
                           const std::chrono::milliseconds timeout = 5s) {
    const auto start_timestamp = std::chrono::steady_clock::now();
    const auto kStep = 50ms;
    mysqlrouter::MySQLSession client;

    do {
      try {
        client.connect("127.0.0.1", port, "root", "fake-pass", "", "");

        return true;
      } catch (...) {
      }

      if (std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - start_timestamp) >= timeout) {
        break;
      }
      std::this_thread::sleep_for(kStep);
    } while (true);

    return false;
  }
};

TEST_F(RouterMaxConnectionsTest, RoutingTooManyConnections) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // doesn't really matter which file we use here, we are not going to do any
  // queries
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  // launch the server mock
  launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);

  // create a config with routing that has max_connections == 2
  const std::string routing_section = get_static_routing_section(
      "A", router_port, server_port, "classic", {{"max_connections", "2"}});

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), routing_section);

  // launch the router with the created configuration
  launch_router({"-c", conf_file});
  EXPECT_TRUE(wait_for_port_used(router_port));

  // try to create 3 connections, the third should fail
  // because of the max_connections limit being exceeded
  mysqlrouter::MySQLSession client1, client2, client3;
  EXPECT_NO_THROW(
      client1.connect("127.0.0.1", router_port, "root", "fake-pass", "", ""));
  EXPECT_NO_THROW(
      client2.connect("127.0.0.1", router_port, "root", "fake-pass", "", ""));
  ASSERT_THROW_LIKE(
      client3.connect("127.0.0.1", router_port, "root", "fake-pass", "", ""),
      std::runtime_error, "Too many connections to MySQL Router (1040)");
}

/**
 * @test
 * This test verifies that:
 *   1. When the server returns an error when the client expects Greetings
 *      message this error is correctly forwarded to the clinet
 *   2. This scenario is not treated as connection error (connection error is
 *      not incremented)
 */
TEST_F(RouterMaxConnectionsTest, RoutingTooManyServerConnections) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // doesn't really matter which file we use here, we are not going to do any
  // queries
  const std::string json_stmts =
      get_data_dir().join("handshake_too_many_con_error.js").str();

  // launch the server mock
  launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);

  // create a config with routing that has max_connections == 2
  const std::string routing_section =
      "[routing:basic]\n"
      "bind_port = " +
      std::to_string(router_port) +
      "\n"
      "mode = read-write\n"
      "destinations = 127.0.0.1:" +
      std::to_string(server_port) + "\n";

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), routing_section);

  // launch the router with the created configuration
  auto &router = launch_router({"-c", conf_file});

  // try to make a connection, the client should get the error from server
  // forwarded
  mysqlrouter::MySQLSession client;

  // The client should get the original server error about the connections limit
  // being reached
  ASSERT_THROW_LIKE(
      client.connect("127.0.0.1", router_port, "root", "fake-pass", "", ""),
      std::runtime_error, "Too many connections");

  // The Router log should contain debug info with the error while waiting
  // for the Greeting message
  EXPECT_TRUE(wait_log_contains(
      router,
      "DEBUG .* Error from the server while waiting for greetings "
      "message: 1040, 'Too many connections'",
      5s));

  // There should be no trace of the connection errors counter incremented as a
  // result of the result from error
  const auto log_content = router.get_logfile_content();
  const std::string pattern = "1 connection errors for 127.0.0.1";
  ASSERT_FALSE(pattern_found(log_content, pattern)) << log_content;
}

/**
 * @test Verify that max_total_connections configuration options is correctly
 * honoured.
 */
TEST_F(RouterMaxConnectionsTest, RoutingTotalMaxConnectionsExceeded) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_portA = port_pool_.get_next_available();
  const auto router_portB = port_pool_.get_next_available();

  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  // launch the server mock
  launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);

  // create a config with 2 routing sections and max_total_connections = 2
  const std::string routing_section1 =
      get_static_routing_section("A", router_portA, server_port, "classic");
  const std::string routing_section2 =
      get_static_routing_section("B", router_portB, server_port, "classic");

  TempDirectory conf_dir("conf");

  std::string conf_file = create_config_file(
      conf_dir.name(), routing_section1 + routing_section2, nullptr,
      "mysqlrouter.conf", "max_total_connections=2");

  // launch the router with the created configuration
  auto &router = launch_router({"-c", conf_file});

  // try to create 3 connections, the third should fail
  // because of the max_connections limit being exceeded
  mysqlrouter::MySQLSession client1, client2, client3;

  // make 2 connections, one for each routing port
  EXPECT_NO_THROW(
      client1.connect("127.0.0.1", router_portA, "root", "fake-pass", "", ""));
  EXPECT_NO_THROW(
      client2.connect("127.0.0.1", router_portB, "root", "fake-pass", "", ""));

  // try to connect to both routing ports, it should fail both times,
  // max_total_connections has been reached
  ASSERT_THROW_LIKE(
      client3.connect("127.0.0.1", router_portA, "root", "fake-pass", "", ""),
      std::runtime_error, "Too many connections to MySQL Router (1040)");

  // The log should contain expected warning message
  EXPECT_TRUE(
      wait_log_contains(router,
                        "WARNING .* \\[routing:A\\] Total connections count=2 "
                        "exceeds \\[DEFAULT\\].max_total_connections=2",
                        5s));

  ASSERT_THROW_LIKE(
      client3.connect("127.0.0.1", router_portB, "root", "fake-pass", "", ""),
      std::runtime_error, "Too many connections to MySQL Router (1040)");

  EXPECT_TRUE(
      wait_log_contains(router,
                        "WARNING .* \\[routing:B\\] Total connections count=2 "
                        "exceeds \\[DEFAULT\\].max_total_connections=2",
                        5s));

  // disconnect the first client, now we should be able to connect again
  client1.disconnect();
  EXPECT_TRUE(make_new_connection(router_portA));
}

/**
 * @test Check if the Router behavior is correct when the configured sum of all
 * max_connections per route is higher than max_total_connections
 */
TEST_F(RouterMaxConnectionsTest,
       RoutingRouteMaxConnectionsSumOfAllMaxConsHigherThanMaxTotalConns) {
  const auto server_classic_port = port_pool_.get_next_available();
  const auto server_x_port = port_pool_.get_next_available();
  const auto router_classic_rw_port = port_pool_.get_next_available();
  const auto router_classic_ro_port = port_pool_.get_next_available();
  const auto router_x_rw_port = port_pool_.get_next_available();
  const auto router_x_ro_port = port_pool_.get_next_available();

  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  // launch the server mock that will terminate all our classic and x
  // connections
  launch_mysql_server_mock(json_stmts, server_classic_port, EXIT_SUCCESS, false,
                           /*http_port*/ 0, server_x_port);

  // create a configuration with 4 routes (classic rw, ro, x rw, ro)
  // each has "local" limit of 5 max_connections
  // the total_max_connections is 10
  const std::string routing_section_classic_rw = get_static_routing_section(
      "classic_rw", router_classic_rw_port, server_classic_port, "classic",
      {{"max_connections", "5"}});
  const std::string routing_section_classic_ro = get_static_routing_section(
      "classic_ro", router_classic_ro_port, server_classic_port, "classic",
      {{"max_connections", "5"}});

  const std::string routing_section_x_rw = get_static_routing_section(
      "x_rw", router_x_rw_port, server_x_port, "x", {{"max_connections", "2"}});
  const std::string routing_section_x_ro = get_static_routing_section(
      "x_ro", router_x_ro_port, server_x_port, "x", {{"max_connections", "2"}});

  TempDirectory conf_dir("conf");

  std::string conf_file = create_config_file(
      conf_dir.name(),
      routing_section_classic_rw + routing_section_classic_ro +
          routing_section_x_rw + routing_section_x_ro,
      nullptr, "mysqlrouter.conf", "max_total_connections=10");

  // launch the router with the created configuration
  launch_router({"-c", conf_file});

  std::list<mysqlrouter::MySQLSession> classic_sessions;
  // connect 5x to classic rw route, it should be OK
  for (size_t i = 0; i < 5; ++i) {
    classic_sessions.emplace_back();
    auto &new_session = classic_sessions.back();
    EXPECT_NO_THROW(new_session.connect("127.0.0.1", router_classic_rw_port,
                                        "root", "fake-pass", "", ""));
  }

  // the 6th connection should fail, the "local" route connections limit has
  // been reached
  mysqlrouter::MySQLSession failed_session;
  ASSERT_THROW_LIKE(failed_session.connect("127.0.0.1", router_classic_rw_port,
                                           "root", "fake-pass", "", ""),
                    std::runtime_error,
                    "Too many connections to MySQL Router (1040)");

  // connect 5x to classic ro route, it should be OK
  for (size_t i = 0; i < 5; ++i) {
    classic_sessions.emplace_back();
    auto &new_session = classic_sessions.back();
    EXPECT_NO_THROW(new_session.connect("127.0.0.1", router_classic_ro_port,
                                        "root", "fake-pass", "", ""));
  }

  // the 6th connection should fail, both "local" route connections limit and
  // max_total_connections limits has been reached
  ASSERT_THROW_LIKE(failed_session.connect("127.0.0.1", router_classic_ro_port,
                                           "root", "fake-pass", "", ""),
                    std::runtime_error,
                    "Too many connections to MySQL Router (1040)");

  // trying to connect to x routes should fail, as max_total_connections limit
  // has been reached
  for (size_t i = 0; i < 5; ++i) {
    XProtocolSession x_session;
    const auto res = make_x_connection(x_session, "127.0.0.1", router_x_rw_port,
                                       "root", "fake-pass");
    EXPECT_TRUE(res);
    EXPECT_EQ("Too many connections to MySQL Router"s, res.what());
  }

  for (size_t i = 0; i < 5; ++i) {
    XProtocolSession x_session;
    const auto res = make_x_connection(x_session, "127.0.0.1", router_x_ro_port,
                                       "root", "fake-pass");
    EXPECT_TRUE(res);
    EXPECT_EQ("Too many connections to MySQL Router"s, res.what());
  }
}

/**
 * @test Check if the Router behavior is correct when the configured sum of all
 * max_connections per route is lower than max_total_connections
 */
TEST_F(RouterMaxConnectionsTest,
       RoutingRouteMaxConnectionsSumOfAllMaxConsLowerThanMaxTotalConns) {
  const auto server_classic_port = port_pool_.get_next_available();
  const auto server_x_port = port_pool_.get_next_available();
  const auto router_classic_rw_port = port_pool_.get_next_available();
  const auto router_classic_ro_port = port_pool_.get_next_available();
  const auto router_x_rw_port = port_pool_.get_next_available();
  const auto router_x_ro_port = port_pool_.get_next_available();

  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  // launch the server mock that will terminate all our classic and x
  // connections
  launch_mysql_server_mock(json_stmts, server_classic_port, EXIT_SUCCESS, false,
                           /*http_port*/ 0, server_x_port);

  // create a configuration with 4 routes (classic rw, ro, x rw, ro)
  // each has "local" limit of 5 max_connections
  // the total_max_connections is 25
  const std::string routing_section_classic_rw = get_static_routing_section(
      "classic_rw", router_classic_rw_port, server_classic_port, "classic",
      {{"max_connections", "5"}});
  const std::string routing_section_classic_ro = get_static_routing_section(
      "classic_ro", router_classic_ro_port, server_classic_port, "classic",
      {{"max_connections", "5"}});

  const std::string routing_section_x_rw = get_static_routing_section(
      "x_rw", router_x_rw_port, server_x_port, "x", {{"max_connections", "5"}});
  const std::string routing_section_x_ro = get_static_routing_section(
      "x_ro", router_x_ro_port, server_x_port, "x", {{"max_connections", "5"}});

  TempDirectory conf_dir("conf");

  std::string conf_file = create_config_file(
      conf_dir.name(),
      routing_section_classic_rw + routing_section_classic_ro +
          routing_section_x_rw + routing_section_x_ro,
      nullptr, "mysqlrouter.conf", "max_total_connections=25");

  // launch the router with the created configuration
  launch_router({"-c", conf_file});

  std::list<mysqlrouter::MySQLSession> classic_sessions;
  // connect 5x to classic rw route, it should be OK
  for (size_t i = 0; i < 5; ++i) {
    classic_sessions.emplace_back();
    auto &new_session = classic_sessions.back();
    EXPECT_NO_THROW(new_session.connect("127.0.0.1", router_classic_rw_port,
                                        "root", "fake-pass", "", ""));
  }

  // the 6th connection should fail, the "local" route connections limit has
  // been reached
  mysqlrouter::MySQLSession failed_session;
  ASSERT_THROW_LIKE(failed_session.connect("127.0.0.1", router_classic_rw_port,
                                           "root", "fake-pass", "", ""),
                    std::runtime_error,
                    "Too many connections to MySQL Router (1040)");

  // connect 5x to classic ro route, it should be OK
  for (size_t i = 0; i < 5; ++i) {
    classic_sessions.emplace_back();
    auto &new_session = classic_sessions.back();
    EXPECT_NO_THROW(new_session.connect("127.0.0.1", router_classic_ro_port,
                                        "root", "fake-pass", "", ""));
  }

  // the 6th connection should fail, the "local" route connections limit has
  // been reached
  ASSERT_THROW_LIKE(failed_session.connect("127.0.0.1", router_classic_ro_port,
                                           "root", "fake-pass", "", ""),
                    std::runtime_error,
                    "Too many connections to MySQL Router (1040)");

  std::list<XProtocolSession> x_sessions;

  // connect 5x to X rw route, it should be OK
  for (size_t i = 0; i < 5; ++i) {
    x_sessions.emplace_back();
    auto &new_session = x_sessions.back();
    EXPECT_FALSE(make_x_connection(new_session, "127.0.0.1", router_x_rw_port,
                                   "root", "fake-pass"));
  }

  // the 6th connection should fail, the "local" route connections limit has
  // been reached
  for (size_t i = 0; i < 1; ++i) {
    XProtocolSession x_session;
    const auto res = make_x_connection(x_session, "127.0.0.1", router_x_rw_port,
                                       "root", "fake-pass");
    EXPECT_TRUE(res);
    EXPECT_EQ("Too many connections to MySQL Router"s, res.what());
  }

  // connect 5x to X ro route, it should be OK
  for (size_t i = 0; i < 5; ++i) {
    x_sessions.emplace_back();
    auto &new_session = x_sessions.back();
    EXPECT_FALSE(make_x_connection(new_session, "127.0.0.1", router_x_ro_port,
                                   "root", "fake-pass"));
  }

  // the 6th connection should fail, the "local" route connections limit has
  // been reached
  for (size_t i = 0; i < 1; ++i) {
    XProtocolSession x_session;
    const auto res = make_x_connection(x_session, "127.0.0.1", router_x_ro_port,
                                       "root", "fake-pass");
    EXPECT_TRUE(res);
    EXPECT_EQ("Too many connections to MySQL Router"s, res.what());
  }
}

template <class T>
::testing::AssertionResult ThrowsExceptionWith(std::function<void()> callable,
                                               const char *expected_text) {
  try {
    callable();
    return ::testing::AssertionFailure()
           << "Expected exception to throw, but it didn't";
  } catch (const T &e) {
    if (nullptr == ::strstr(e.what(), expected_text)) {
      return ::testing::AssertionFailure()
             << "Expected exception-text to contain: " << expected_text
             << ". Actual: " << e.what();
    }

    return ::testing::AssertionSuccess();
  } catch (...) {
    // as T may be std::exception we can't use it as default case and need to
    // do this extra round
    try {
      throw;
    } catch (const std::exception &e) {
      return ::testing::AssertionFailure()
             << "Expected exception of type " << typeid(T).name()
             << ". Actual: " << typeid(e).name();
    } catch (...) {
      return ::testing::AssertionFailure()
             << "Expected exception of type " << typeid(T).name()
             << ". Actual: non-std exception";
    }
  }
}

/**
 * @test Check if the Router logs expected warning if the
 * routing.max_connections is configured to non-default value that exceeds
 * max_total_connections
 */
TEST_F(RouterMaxConnectionsTest, WarningWhenLocalMaxConGreaterThanTotalMaxCon) {
  const auto server_classic_port = port_pool_.get_next_available();
  const auto router_classic_rw_port = port_pool_.get_next_available();

  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  // launch the server mock that will terminate all our classic and x
  // connections
  launch_mysql_server_mock(json_stmts, server_classic_port, EXIT_SUCCESS, false,
                           /*http_port*/ 0);

  // create a configuration with 1 route (classic rw) that has  "local" limit of
  // 600 max_connections the total_max_connections is default 512
  const std::string routing_section_classic_rw = get_static_routing_section(
      "classic_rw", router_classic_rw_port, server_classic_port, "classic",
      {{"max_connections", "600"}});
  TempDirectory conf_dir("conf");

  std::string conf_file = create_config_file(
      conf_dir.name(), routing_section_classic_rw, nullptr, "mysqlrouter.conf");

  // launch the router with the created configuration
  auto &router = launch_router({"-c", conf_file});

  // The log should contain expected warning message
  EXPECT_TRUE(wait_log_contains(
      router,
      "WARNING .* Value configured for max_connections > max_total_connections "
      "\\(600 > 512\\)\\. Will have no effect\\.",
      5s));
}

#ifndef _WIN32  // named sockets are not supported on Windows;
                // on Unix, they're implemented using Unix sockets
TEST_F(RouterRoutingTest, named_socket_has_right_permissions) {
  /**
   * @test Verify that unix socket has the required file permissions so that
   * it can be connected to by all users. According to man 7 unix, only r+w
   *       permissions are required, but Server sets x as well, so we do the
   * same.
   */

  // get config dir (we will also stuff our unix socket file there)
  TempDirectory bootstrap_dir;

  // launch Router with unix socket
  const std::string socket_file = bootstrap_dir.name() + "/sockfile";
  const std::string routing_section =
      "[routing:basic]\n"
      "socket = " +
      socket_file +
      "\n"
      "mode = read-write\n"
      "destinations = 127.0.0.1:1234\n";  // port can be bogus
  TempDirectory conf_dir("conf");
  const std::string conf_file =
      create_config_file(conf_dir.name(), routing_section);
  auto &router = launch_router({"-c", conf_file});

  // loop until socket file appears and has correct permissions
  auto wait_for_correct_perms = [&socket_file](int timeout_ms) {
    const mode_t expected_mode = S_IFSOCK | S_IRUSR | S_IWUSR | S_IXUSR |
                                 S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH |
                                 S_IWOTH | S_IXOTH;
    while (timeout_ms > 0) {
      struct stat info;

      memset(&info, 0, sizeof(info));
      stat(socket_file.c_str(),
           &info);  // silently ignore error when file doesn't exist yet

      if (info.st_mode == expected_mode) return true;

      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      timeout_ms -= 10;
    }

    return false;
  };

  EXPECT_THAT(wait_for_correct_perms(5000), testing::Eq(true));
  EXPECT_TRUE(wait_log_contains(router,
                                "Start accepting connections for routing "
                                "routing:basic listening on named socket",
                                5s));
}
#endif

TEST_F(RouterRoutingTest, RoutingMaxConnectErrors) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // json file does not actually matter in this test as we are not going to
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();
  TempDirectory bootstrap_dir;

  // launch the server mock for bootstrapping
  launch_mysql_server_mock(
      json_stmts, server_port, EXIT_SUCCESS,
      false /*expecting huge data, can't print on the console*/);

  const std::string routing_section =
      "[routing:basic]\n"
      "bind_port = " +
      std::to_string(router_port) +
      "\n"
      "mode = read-write\n"
      "destinations = 127.0.0.1:" +
      std::to_string(server_port) +
      "\n"
      "max_connect_errors = 1\n";

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), routing_section);

  // launch the router
  auto &router = launch_router({"-c", conf_file});

  // wait for router to begin accepting the connections
  // NOTE: this should cause connection/disconnection which
  //       should be treated as connection error and increment
  //       connection errors counter.  This test relies on that.
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port));

  SCOPED_TRACE("// wait until 'blocking client host' appears in the log");
  ASSERT_TRUE(wait_log_contains(router, "blocking client host", 5000ms));

  // for the next connection attempt we should get an error as the
  // max_connect_errors was exceeded
  MySQLSession client;
  EXPECT_THROW_LIKE(
      client.connect("127.0.0.1", router_port, "root", "fake-pass", "", ""),
      std::exception, "Too many connection errors");
}

/**
 * @test
 * This test verifies that:
 *   1. Router will block a misbehaving client after consecutive
 *      <max_connect_errors> connection errors
 *   2. Router will reset its connection error counter if client establishes a
 *      successful connection before <max_connect_errors> threshold is hit
 */
TEST_F(RouterRoutingTest, error_counters) {
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t router_port = port_pool_.get_next_available();

  // doesn't really matter which file we use here, we are not going to do any
  // queries
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  // launch the server mock
  launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);

  // create a config with max_connect_errors == 3
  const std::string routing_section =
      "[routing:basic]\n"
      "bind_port = " +
      std::to_string(router_port) +
      "\n"
      "mode = read-write\n"
      "max_connect_errors = 3\n"
      "destinations = 127.0.0.1:" +
      std::to_string(server_port) + "\n";
  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), routing_section);

  // launch the router with the created configuration
  launch_router({"-c", conf_file});

  SCOPED_TRACE(
      "// make good and bad connections (connect() + 1024 0-bytes) to check "
      "blocked client gets reset");
  // we loop just for good measure, to additionally test that this behaviour
  // is repeatable
  for (int i = 0; i < 5; i++) {
    // good connection, followed by 2 bad ones. Good one should reset the
    // error counter
    try {
      mysqlrouter::MySQLSession client;
      client.connect("127.0.0.1", router_port, "root", "fake-pass", "", "");
    } catch (const std::exception &e) {
      FAIL() << e.what();
    }
    make_bad_connection(router_port);
    make_bad_connection(router_port);
  }

  SCOPED_TRACE("// make bad connection to trigger blocked client");
  // make a 3rd consecutive bad connection - it should cause Router to start
  // blocking us
  make_bad_connection(router_port);

  // we loop just for good measure, to additionally test that this behaviour
  // is repeatable
  for (int i = 0; i < 5; i++) {
    // now trying to make a good connection should fail due to blockage
    mysqlrouter::MySQLSession client;
    SCOPED_TRACE("// make connection to check if we are really blocked");
    try {
      client.connect("127.0.0.1", router_port, "root", "fake-pass", "", "");

      FAIL() << "connect should be blocked, but isn't";
    } catch (const std::exception &e) {
      EXPECT_THAT(e.what(), ::testing::HasSubstr("Too many connection errors"));
    }
  }
}

TEST_F(RouterRoutingTest, spaces_in_destinations_list) {
  mysql_harness::ConfigBuilder builder;
  auto bind_port = port_pool_.get_next_available();

  const auto routing_section = builder.build_section(
      "routing", {
                     {"destinations",
                      " localhost:13005, localhost:13003  ,localhost:13004 "},
                     {"bind_address", "127.0.0.1"},
                     {"bind_port", std::to_string(bind_port)},
                     {"routing_strategy", "first-available"},
                 });

  TempDirectory conf_dir("conf");
  const auto conf_file = create_config_file(conf_dir.name(), routing_section);

  ASSERT_NO_FATAL_FAILURE(launch_router({"-c", conf_file}, EXIT_SUCCESS));
}

struct RoutingConfigParam {
  const char *test_name;

  std::vector<std::pair<std::string, std::string>> routing_opts;

  std::function<void(const std::vector<std::string> &)> checker;
};

class RoutingConfigTest
    : public RouterComponentTest,
      public ::testing::WithParamInterface<RoutingConfigParam> {};

TEST_P(RoutingConfigTest, check) {
  mysql_harness::ConfigBuilder builder;

  const std::string routing_section =
      builder.build_section("routing", GetParam().routing_opts);

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), routing_section);

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

const RoutingConfigParam routing_config_param[] = {
    {"no_destination",
     {
         {"destinations", "127.0.0.1:3306"},
         {"routing_strategy", "first-available"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                              "either bind_address or socket option needs to "
                              "be supplied, or both")));
     }},
    {"missing_port_in_bind_address",
     {
         {"destinations", "127.0.0.1:3306"},
         {"routing_strategy", "first-available"},
         {"bind_address", "127.0.0.1"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                              "either bind_address or socket option needs to "
                              "be supplied, or both")));
     }},
    {"invalid_port_in_bind_address",
     {
         {"destinations", "127.0.0.1:3306"},
         {"routing_strategy", "first-available"},
         {"bind_address", "127.0.0.1:999292"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(
           lines, ::testing::Contains(::testing::HasSubstr(
                      "option bind_address in [routing]: '127.0.0.1:999292' is "
                      "not a valid endpoint")));
     }},
    {"too_large_bind_port",
     {
         {"destinations", "127.0.0.1:3306"},
         {"routing_strategy", "first-available"},
         {"bind_port", "23123124123123"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(
           lines, ::testing::Contains(::testing::HasSubstr(
                      "option bind_port in [routing] needs value between 1 and "
                      "65535 inclusive, was '23123124123123'")));
     }},
    {"invalid_mode",
     {
         {"destinations", "127.0.0.1:3306"},
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"mode", "invalid"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(
           lines,
           ::testing::Contains(::testing::HasSubstr(
               "option mode in [routing] is invalid; valid are read-write "
               "and read-only (was 'invalid')")));
     }},
    {"invalid_routing_strategy",
     {
         {"destinations", "127.0.0.1:3306"},
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", "invalid"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "option routing_strategy in [routing] is invalid; valid "
                       "are first-available, "
                       "next-available, and round-robin (was 'invalid')")));
     }},
    {"empty_mode",
     {
         {"destinations", "127.0.0.1:3306"},
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"mode", ""},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                              "option mode in [routing] needs a value")));
     }},
    {"empty_routing_strategy",
     {
         {"destinations", "127.0.0.1:3306"},
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", ""},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "option routing_strategy in [routing] needs a value")));
     }},
    {"missing_routing_strategy",
     {
         {"destinations", "127.0.0.1:3306"},
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "option routing_strategy in [routing] is required")));
     }},
    {"thread_stack_size_negative",
     {
         {"destinations", "127.0.0.1:3306"},
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", "first-available"},
         {"thread_stack_size", "-1"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "option thread_stack_size in [routing] needs "
                       "value between 1 and 65535 inclusive, was '-1'")));
     }},
    {"thread_stack_size_float",
     {
         {"destinations", "127.0.0.1:3306"},
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", "first-available"},
         {"thread_stack_size", "4.5"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "option thread_stack_size in [routing] needs "
                       "value between 1 and 65535 inclusive, was '4.5'")));
     }},
    {"thread_stack_size_string",
     {
         {"destinations", "127.0.0.1:3306"},
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", "first-available"},
         {"thread_stack_size", "dfs4"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "option thread_stack_size in [routing] needs "
                       "value between 1 and 65535 inclusive, was 'dfs4'")));
     }},
    {"thread_stack_size_hex",
     {
         {"destinations", "127.0.0.1:3306"},
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", "first-available"},
         {"thread_stack_size", "0xff"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "option thread_stack_size in [routing] needs "
                       "value between 1 and 65535 inclusive, was '0xff'")));
     }},
    {"invalid_destination_host_start",
     {
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", "first-available"},
         {"destinations", "{#mysqld1}"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                              "option destinations in [routing] has an "
                              "invalid destination address '{#mysqld1}'")));
     }},
    {"invalid_destination_host_mid",
     {
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", "first-available"},
         {"destinations", "{mysqld1@1}"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                              "option destinations in [routing] has an "
                              "invalid destination address '{mysqld1@1}'")));
     }},
    {"invalid_destination_host_end",
     {
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", "first-available"},
         {"destinations", "{mysqld1`}"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                              "option destinations in [routing] has an "
                              "invalid destination address '{mysqld1`}'")));
     }},
    {"invalid_destination_host_many",
     {
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", "first-available"},
         {"destinations", "{mysql$d1%1}"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                              "option destinations in [routing] has an "
                              "invalid destination address '{mysql$d1%1}'")));
     }},
    {"invalid_destination_space_start",
     {
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", "first-available"},
         {"destinations", "{ mysql1}"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                              "option destinations in [routing] has an "
                              "invalid destination address '{ mysql1}'")));
     }},
    {"invalid_destination_space_mid",
     {
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", "first-available"},
         {"destinations", "{my sql1}"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                              "option destinations in [routing] has an "
                              "invalid destination address '{my sql1}'")));
     }},
    {"invalid_destination_space_end",
     {
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", "first-available"},
         {"destinations", "{mysql1 }"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                              "option destinations in [routing] has an "
                              "invalid destination address '{mysql1 }'")));
     }},
    {"invalid_destination_space",
     {
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", "first-available"},
         {"destinations", "{m@ysql d1}"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                              "option destinations in [routing] has an "
                              "invalid destination address '{m@ysql d1}'")));
     }},
    {"invalid_destination_multiple_space",
     {
         {"bind_address", "127.0.0.1"},
         {"bind_port", "6000"},
         {"routing_strategy", "first-available"},
         {"destinations", "{my sql d1}"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                              "option destinations in [routing] has an "
                              "invalid destination address '{my sql d1}'")));
     }},
    {"invalid_bind_port",
     {
         {"destinations", "127.0.0.1:3306"},
         {"bind_address", "127.0.0.1"},
         {"routing_strategy", "first-available"},

         {"bind_port", "{mysqld@1}"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "option bind_port in [routing] needs value "
                       "between 1 and 65535 inclusive, was '{mysqld@1}'")));
     }},
    {"destinations_trailing_comma",
     {
         {"destinations", "localhost:13005,localhost:13003,localhost:13004,"},

         {"bind_address", "127.0.0.1"},
         {"routing_strategy", "first-available"},
     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "empty address found in destination list (was "
                       "'localhost:13005,localhost:13003,localhost:13004,')")));
     }},
    {"destinations_trailing_comma_and_spaces",
     {
         {"destinations",
          "localhost:13005,localhost:13003,localhost:13004, , ,"},

         {"bind_address", "127.0.0.1"},
         {"routing_strategy", "first-available"},

     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(
           lines,
           ::testing::Contains(::testing::HasSubstr(
               "empty address found in destination list (was "
               "'localhost:13005,localhost:13003,localhost:13004, , ,')")));
     }},
    {"destinations_empty_and_spaces",
     {
         {"destinations", "localhost:13005, ,,localhost:13003,localhost:13004"},

         {"bind_address", "127.0.0.1"},
         {"routing_strategy", "first-available"},

     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(
           lines,
           ::testing::Contains(::testing::HasSubstr(
               "empty address found in destination list (was "
               "'localhost:13005, ,,localhost:13003,localhost:13004')")));
     }},
    {"destinations_leading_comma",
     {
         {"destinations", ",localhost:13005,localhost:13003,localhost:13004"},

         {"bind_address", "127.0.0.1"},
         {"routing_strategy", "first-available"},

     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "empty address found in destination list (was "
                       "',localhost:13005,localhost:13003,localhost:13004')")));
     }},
    {"destinations_only_commas",
     {
         {"destinations", ",, ,"},

         {"bind_address", "127.0.0.1"},
         {"routing_strategy", "first-available"},

     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                              "empty address found in destination list (was "
                              "',, ,')")));
     }},
    {"destinations_leading_trailing_comma",
     {
         {"destinations",
          ",localhost:13005, ,,localhost:13003,localhost:13004, ,"},

         {"bind_address", "127.0.0.1"},
         {"routing_strategy", "first-available"},

     },
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(
           lines,
           ::testing::Contains(::testing::HasSubstr(
               "empty address found in destination list (was "
               "',localhost:13005, ,,localhost:13003,localhost:13004, ,')")));
     }},
};

INSTANTIATE_TEST_SUITE_P(Spec, RoutingConfigTest,
                         ::testing::ValuesIn(routing_config_param),
                         [](const auto &info) { return info.param.test_name; });

struct RoutingDefaultConfigParam {
  const char *test_name;

  std::string extra_defaults;

  std::function<void(const std::vector<std::string> &)> checker;
};

class RoutingDefaultConfigTest
    : public RouterComponentTest,
      public ::testing::WithParamInterface<RoutingDefaultConfigParam> {};

TEST_P(RoutingDefaultConfigTest, check) {
  mysql_harness::ConfigBuilder builder;

  const std::string routing_section = builder.build_section(
      "routing", {
                     {"destinations", "127.0.0.1:3306"},
                     {"bind_address", "127.0.0.1"},
                     {"routing_strategy", "first-available"},
                 });

  TempDirectory conf_dir("conf");
  std::string conf_file =
      create_config_file(conf_dir.name(), routing_section, nullptr,
                         "mysqlrouter.conf", GetParam().extra_defaults);

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

const RoutingDefaultConfigParam routing_default_config_param[] = {
    {"max_total_connections_0", "max_total_connections=0",
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "Configuration error: "
                       "[DEFAULT].max_total_connections needs value between 1 "
                       "and 9223372036854775807 inclusive, was '0'")));
     }},
    {"max_total_connections_negative", "max_total_connections=-1",
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "Configuration error: "
                       "[DEFAULT].max_total_connections needs value between 1 "
                       "and 9223372036854775807 inclusive, was '-1'")));
     }},
    {"max_total_connections_too_big",
     "max_total_connections=9223372036854775808",
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "Configuration error: "
                       "[DEFAULT].max_total_connections needs value between 1 "
                       "and 9223372036854775807 inclusive, was "
                       "'9223372036854775808'")));
     }},
    {"max_total_connections_comma", "max_total_connections=10,000",
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "Configuration error: "
                       "[DEFAULT].max_total_connections needs value between 1 "
                       "and 9223372036854775807 inclusive, was "
                       "'10,000'")));
     }},
    {"max_total_connections_yes", "max_total_connections=yes",
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "Configuration error: "
                       "[DEFAULT].max_total_connections needs value between 1 "
                       "and 9223372036854775807 inclusive, was 'yes'")));
     }},
    {"max_total_connections_hex", "max_total_connections=0x7FFFFFFFFFFFFFFF ",
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(
           lines,
           ::testing::Contains(::testing::HasSubstr(
               "Configuration error: "
               "[DEFAULT].max_total_connections needs value between 1 "
               "and 9223372036854775807 inclusive, was '0x7FFFFFFFFFFFFFFF'")));
     }},
    {"max_total_connections_hex2", "max_total_connections=0x1",
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "Configuration error: "
                       "[DEFAULT].max_total_connections needs value between 1 "
                       "and 9223372036854775807 inclusive, was '0x1'")));
     }},
    {"max_total_connections_inv2", "max_total_connections=12a",
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "Configuration error: "
                       "[DEFAULT].max_total_connections needs value between 1 "
                       "and 9223372036854775807 inclusive, was '12a'")));
     }},
    {"max_total_connections_inv3", "max_total_connections=#^%",
     [](const std::vector<std::string> &lines) {
       EXPECT_THAT(lines,
                   ::testing::Contains(::testing::HasSubstr(
                       "Configuration error: "
                       "[DEFAULT].max_total_connections needs value between 1 "
                       "and 9223372036854775807 inclusive, was '#^%'")));
     }}};

INSTANTIATE_TEST_SUITE_P(Spec, RoutingDefaultConfigTest,
                         ::testing::ValuesIn(routing_default_config_param),
                         [](const auto &info) { return info.param.test_name; });

void shut_and_close_socket(net::impl::socket::native_handle_type sock) {
  const auto shut_both =
      static_cast<std::underlying_type_t<net::socket_base::shutdown_type>>(
          net::socket_base::shutdown_type::shutdown_both);
  net::impl::socket::shutdown(sock, shut_both);
  net::impl::socket::close(sock);
}

net::impl::socket::native_handle_type connect_to_port(
    const std::string &hostname, uint16_t port) {
  struct addrinfo hints, *ainfo;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(hostname.c_str(), std::to_string(port).c_str(),
                           &hints, &ainfo);
  if (status != 0) {
    return net::impl::socket::kInvalidSocket;
  }
  std::shared_ptr<void> exit_freeaddrinfo(nullptr,
                                          [&](void *) { freeaddrinfo(ainfo); });

  auto result =
      socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
  if (result == net::impl::socket::kInvalidSocket) {
    return result;
  }

  status = connect(result, ainfo->ai_addr, ainfo->ai_addrlen);
  if (status < 0) {
    return net::impl::socket::kInvalidSocket;
  }

  return result;
}

struct InvalidInitMessageParam {
  std::string client_ssl_mode;
  std::string server_ssl_mode;
  // binary data that client sends after connecting
  std::vector<uint8_t> client_data;
};

class RouterRoutingXProtocolInvalidInitMessageTest
    : public RouterRoutingTest,
      public ::testing::WithParamInterface<InvalidInitMessageParam> {};

/**
 * @test Check if the Router behavior is correct when the client sends
 * unexpected data right after connecting. It is pretty basic test, we check if
 * the Router does not crash and that connecting to the port is still possible
 * after that.
 */
TEST_P(RouterRoutingXProtocolInvalidInitMessageTest,
       XProtocolInvalidInitMessageTest) {
  const auto server_classic_port = port_pool_.get_next_available();
  const auto server_x_port = port_pool_.get_next_available();
  const auto router_x_rw_port = port_pool_.get_next_available();

  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  launch_mysql_server_mock(json_stmts, server_classic_port, EXIT_SUCCESS, false,
                           /*http_port*/ 0, server_x_port);

  const std::string routing_x_section =
      get_static_routing_section("x", router_x_rw_port, server_x_port, "x");

  TempDirectory conf_dir("conf");

  const std::string ssl_conf =
      "server_ssl_mode="s + GetParam().server_ssl_mode +
      "\n"
      "client_ssl_mode="s +
      GetParam().client_ssl_mode +
      "\n"
      "client_ssl_key=" SSL_TEST_DATA_DIR "/server-key-sha512.pem\n" +
      "client_ssl_cert=" SSL_TEST_DATA_DIR "/server-cert-sha512.pem";

  std::string conf_file =
      create_config_file(conf_dir.name(), routing_x_section, nullptr,
                         "mysqlrouter.conf", ssl_conf);

  // launch the router with the created configuration
  launch_router({"-c", conf_file});

  const auto x_con_sock = connect_to_port("127.0.0.1", router_x_rw_port);
  ASSERT_NE(net::impl::socket::kInvalidSocket, x_con_sock);

  std::shared_ptr<void> exit_close_socket(
      nullptr, [&](void *) { shut_and_close_socket(x_con_sock); });

  const auto write_res = net::impl::socket::write(
      x_con_sock, GetParam().client_data.data(), GetParam().client_data.size());

  ASSERT_TRUE(write_res);

  // check that after we have sent the random data, connecting is still
  // possible
  XProtocolSession x_session;
  const auto res = make_x_connection(x_session, "127.0.0.1", router_x_rw_port,
                                     "root", "fake-pass");

  EXPECT_THAT(res.error(), ::testing::AnyOf(0, 3159));
}

INSTANTIATE_TEST_SUITE_P(
    XProtocolInvalidInitMessageTest,
    RouterRoutingXProtocolInvalidInitMessageTest,

    ::testing::Values(
        // ResetSession frame
        InvalidInitMessageParam{
            "REQUIRED", "AS_CLIENT", {0x1, 0x0, 0x0, 0x0, 0x6}},
        InvalidInitMessageParam{
            "PASSTHROUGH", "AS_CLIENT", {0x1, 0x0, 0x0, 0x0, 0x6}},
        // SessionClose frame
        InvalidInitMessageParam{
            "REQUIRED", "AS_CLIENT", {0x1, 0x0, 0x0, 0x0, 0x7}},
        InvalidInitMessageParam{
            "PASSTHROUGH", "AS_CLIENT", {0x1, 0x0, 0x0, 0x0, 0x7}},
        // short frame
        InvalidInitMessageParam{"REQUIRED", "AS_CLIENT", {0x1}},
        InvalidInitMessageParam{"PASSTHROUGH", "AS_CLIENT", {0x1}},
        // random garbage
        InvalidInitMessageParam{
            "REQUIRED", "AS_CLIENT", {0x2, 0x3, 0x4, 0x5, 0x11, 0x22}},
        InvalidInitMessageParam{
            "PASSTHROUGH", "AS_CLIENT", {0x2, 0x3, 0x4, 0x5, 0x11, 0x22}}));

static size_t message_byte_size(const google::protobuf::MessageLite &msg) {
#if (defined(GOOGLE_PROTOBUF_VERSION) && GOOGLE_PROTOBUF_VERSION > 3000000)
  return msg.ByteSizeLong();
#else
  return msg.ByteSize();
#endif
}

template <class T>
static size_t xproto_frame_encode(const T &msg, uint8_t msg_type,
                                  std::vector<uint8_t> &out_buf) {
  using google::protobuf::io::ArrayOutputStream;
  using google::protobuf::io::CodedOutputStream;

  const auto out_payload_size = message_byte_size(msg);
  out_buf.resize(5 + out_payload_size);
  ArrayOutputStream outs(out_buf.data(), out_buf.size());
  CodedOutputStream codecouts(&outs);

  codecouts.WriteLittleEndian32(out_payload_size + 1);
  codecouts.WriteRaw(&msg_type, 1);
  return msg.SerializeToCodedStream(&codecouts);
}

/**
 * @test Check that if the x protocol client sends CONCLOSE message the Router
 * replies with OK{bye!} message.
 */
TEST_F(RouterRoutingTest, CloseConnection) {
  const auto server_classic_port = port_pool_.get_next_available();
  const auto server_x_port = port_pool_.get_next_available();
  const auto router_x_rw_port = port_pool_.get_next_available();

  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  launch_mysql_server_mock(json_stmts, server_classic_port, EXIT_SUCCESS, false,
                           /*http_port*/ 0, server_x_port);

  const std::string routing_x_section =
      get_static_routing_section("x", router_x_rw_port, server_x_port, "x");

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), routing_x_section,
                                             nullptr, "mysqlrouter.conf");

  // launch the router with the created configuration
  launch_router({"-c", conf_file});

  // make x connection to the Router
  const auto x_con_sock = connect_to_port("127.0.0.1", router_x_rw_port);
  ASSERT_NE(net::impl::socket::kInvalidSocket, x_con_sock);
  std::shared_ptr<void> exit_close_socket(
      nullptr, [&](void *) { shut_and_close_socket(x_con_sock); });

  // send the CON_CLOSE message
  Mysqlx::Connection::Close close_msg;
  std::vector<uint8_t> out_buf;
  xproto_frame_encode(close_msg, Mysqlx::ClientMessages::CON_CLOSE, out_buf);
  const auto write_res =
      net::impl::socket::write(x_con_sock, out_buf.data(), out_buf.size());
  ASSERT_TRUE(write_res);

  // read the reply from the Router
  std::vector<uint8_t> read_buf(128);
  const auto read_res =
      net::impl::socket::read(x_con_sock, read_buf.data(), read_buf.size());
  ASSERT_TRUE(read_res);
  read_buf.resize(read_res.value());

  // it should be OK{bye!} message
  Mysqlx::Ok ok_bye_msg;
  ok_bye_msg.set_msg("bye!");
  std::vector<uint8_t> ok_bye_msg_buf;
  xproto_frame_encode(ok_bye_msg, Mysqlx::ServerMessages::OK, ok_bye_msg_buf);

  EXPECT_THAT(read_buf, ::testing::ContainerEq(ok_bye_msg_buf));
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
