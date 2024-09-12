/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "config_builder.h"
#include "mock_server_testutils.h"
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
#include "test/temp_directory.h"

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

namespace mysqlrouter {
std::ostream &operator<<(std::ostream &os, const MysqlError &e) {
  return os << e.sql_state() << " code: " << e.value() << ": " << e.message();
}
}  // namespace mysqlrouter

using mysql_harness::ConfigBuilder;
using mysqlrouter::MySQLSession;

class RouterRoutingTest : public RouterComponentBootstrapTest {
 public:
  std::string get_static_routing_section(
      const std::string &name, uint16_t bind_port, const std::string &socket,
      std::vector<uint16_t> dest_ports, const std::string &protocol,
      const std::vector<ConfigBuilder::kv_type> &custom_settings = {}) {
    std::vector<std::string> destinations;
    for (const auto port : dest_ports) {
      destinations.push_back("127.0.0.1:" + std::to_string(port));
    }
    std::vector<ConfigBuilder::kv_type> options{
        {"destinations", mysql_harness::join(destinations, ",")},
        {"routing_strategy", "round-robin"},
        {"protocol", protocol}};

    if (socket.empty()) {
      options.push_back({"bind_port", std::to_string(bind_port)});
    } else {
      options.push_back({"socket", socket});
    }

    for (const auto &s : custom_settings) {
      options.push_back(s);
    }

    return mysql_harness::ConfigBuilder::build_section("routing:"s + name,
                                                       options);
  }
};

using XProtocolSession = std::shared_ptr<xcl::XSession>;

static xcl::XError setup_x_session(XProtocolSession &session,
                                   int64_t connect_timeout = 10000 /*10s*/,
                                   const std::string &ssl_mode = "PREFERRED") {
  xcl::XError err;

  err = session->set_mysql_option(
      xcl::XSession::Mysqlx_option::Authentication_method, "FROM_CAPABILITIES");
  if (err) return err;

  err = session->set_mysql_option(xcl::XSession::Mysqlx_option::Ssl_mode,
                                  ssl_mode);
  if (err) return err;

  err = session->set_mysql_option(
      xcl::XSession::Mysqlx_option::Session_connect_timeout, connect_timeout);
  if (err) return err;

  err = session->set_mysql_option(xcl::XSession::Mysqlx_option::Connect_timeout,
                                  connect_timeout);
  if (err) return err;

  return {};
}

static xcl::XError make_x_connection(
    XProtocolSession &session, const std::string &host, const uint16_t port,
    const std::string &username, const std::string &password,
    int64_t connect_timeout = 10000 /*10s*/,
    const std::string &ssl_mode = "PREFERRED") {
  session = xcl::create_session();
  xcl::XError err = setup_x_session(session, connect_timeout, ssl_mode);
  if (err) return err;

  return session->connect(host.c_str(), port, username.c_str(),
                          password.c_str(), "");
}

#ifndef _WIN32
static xcl::XError make_x_connection(XProtocolSession &session,
                                     const std::string &socket,
                                     const std::string &username,
                                     const std::string &password,
                                     int64_t connect_timeout = 10000 /*10s*/) {
  session = xcl::create_session();
  xcl::XError err = setup_x_session(session, connect_timeout);
  if (err) return err;

  return session->connect(socket.c_str(), username.c_str(), password.c_str(),
                          "");
}
#endif  // _WIN32

TEST_F(RouterRoutingTest, RoutingOk) {
  const auto server_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // use the json file that adds additional rows to the metadata to increase the
  // packet size to +10MB to verify routing of the big packets
  TempDirectory bootstrap_dir;

  // launch the server mock for bootstrapping
  mock_server_spawner().spawn(mock_server_cmdline("bootstrap_gr.js")
                                  .port(server_port)
                                  .http_port(http_port)
                                  .args());

  set_mock_metadata(http_port, "00000000-0000-0000-0000-0000000000g1",
                    classic_ports_to_gr_nodes({server_port}), 0, {server_port});

  const std::string routing_section = get_static_routing_section(
      "basic", router_port, "", {server_port}, "classic");

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), routing_section);

  // launch the router with simple static routing configuration
  /*auto &router_static =*/launch_router({"-c", conf_file});

  // launch another router to do the bootstrap connecting to the mock server
  // via first router instance
  auto &router_bootstrapping = launch_router_for_bootstrap(
      {
          "--bootstrap=localhost:" + std::to_string(router_port),
          "-d",
          bootstrap_dir.name(),
      },
      EXIT_SUCCESS);

  ASSERT_NO_FATAL_FAILURE(check_exit_code(router_bootstrapping, EXIT_SUCCESS));

  ASSERT_TRUE(router_bootstrapping.expect_output(
      "MySQL Router configured for the InnoDB Cluster 'test'"));
}

TEST_F(RouterRoutingTest, ResolveFails) {
  RecordProperty("Description",
                 "If resolve fails due to timeout or not resolvable, move the "
                 "destination to the quarantine.");
  const auto router_port = port_pool_.get_next_available();

  TempDirectory conf_dir("conf");

  auto writer = config_writer(conf_dir.name());
  writer.section("routing:does_not_resolve",
                 {
                     // the test needs a hostname that always fails to resolve.
                     //
                     // RFC2606 declares .invalid as reserved TLD.
                     {"destinations", "does-not-resolve.invalid"},
                     {"routing_strategy", "round-robin"},
                     {"protocol", "classic"},
                     {"bind_port", std::to_string(router_port)},
                 });

  auto &rtr = router_spawner().spawn({"-c", writer.write()});

  mysqlrouter::MySQLSession sess;

  SCOPED_TRACE(
      "// make a connection that should fail as the host isn't resolvable");
  try {
    sess.connect("127.0.0.1", router_port, "user", "pass", "", "");
    FAIL() << "expected connect fail.";
  } catch (const MySQLSession::Error &e) {
    EXPECT_EQ(e.code(), 2003) << e.what();
    EXPECT_THAT(e.what(),
                ::testing::HasSubstr("Can't connect to remote MySQL server"))
        << e.what();
  } catch (...) {
    FAIL() << "expected connect fail with a mysql-error";
  }

  SCOPED_TRACE("// port should be closed now.");
  try {
    sess.connect("127.0.0.1", router_port, "user", "pass", "", "");
    FAIL() << "expected connect fail.";
  } catch (const MySQLSession::Error &e) {
    EXPECT_EQ(e.code(), 2003) << e.what();
    EXPECT_THAT(e.what(), ::testing::HasSubstr("Can't connect to MySQL server"))
        << e.what();
  } catch (...) {
    FAIL() << "expected connect fail with a mysql-error";
  }

  rtr.send_clean_shutdown_event();
  ASSERT_NO_THROW(rtr.wait_for_exit());

  // connecting to backend(s) for client from 127.0.0.1:57804 failed:
  // resolve(does-not-resolve-to-anything.foo) failed after 160ms: Name or
  // service not known, end of destinations: no more destinations
  auto logcontent = rtr.get_logfile_content();
  EXPECT_THAT(
      logcontent,
      testing::HasSubstr("resolve(does-not-resolve.invalid) failed after"));

  // check that it was actually added to the quarantine.
  EXPECT_THAT(
      logcontent,
      testing::HasSubstr(
          "add destination 'does-not-resolve.invalid:3306' to quarantine"));
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
      {"routing_strategy", "round-robin"},
      // we use example.org's IP here to avoid DNS resolution which on PB2
      // often takes too long and causes the test timeout assumption to fail
      {"destinations", "93.184.216.34:81"}};

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
       {"routing_strategy", "round-robin"},
       {"connect_timeout", std::to_string(connect_timeout.count())},
       {"destinations", "93.184.216.34:81"}});

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
  mock_server_spawner().spawn(
      mock_server_cmdline("my_port.js").port(server_port).args());

  SCOPED_TRACE("// build router config with connect_timeout=" +
               std::to_string(connect_timeout.count()));
  const auto routing_section = mysql_harness::ConfigBuilder::build_section(
      "routing:timeout",
      {{"bind_port", std::to_string(router_port)},
       {"routing_strategy", "round-robin"},
       {"connect_timeout", std::to_string(connect_timeout.count())},
       {"destinations", "127.0.0.1:" + std::to_string(server_port)}});

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), routing_section);

  // launch the router with simple static routing configuration
  launch_router({"-c", conf_file}, EXIT_SUCCESS);

  // make the connection and close it right away
  {
    auto conn_res = make_new_connection(router_port);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, server_port);
  }

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
       {"routing_strategy", "round-robin"},
       {"connect_timeout", std::to_string(connect_timeout.count())},
       {"protocol", "x"},
       // we use example.org's IP here to avoid DNS resolution which on PB2
       // often takes too long and causes the test timeout assumption to fail
       {"destinations", "93.184.216.34:81"}});

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
  std::this_thread::sleep_for(500ms);
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

  mock_server_spawner().spawn(mock_server_cmdline("bootstrap_gr.js")
                                  .port(server_classic_port)
                                  .x_port(server_x_port)
                                  .args());

  TempDirectory conf_dir("conf-ecc-certificate");
  auto writer = config_writer(conf_dir.name());
  writer.section(
      "routing:classic_ecdh_rsa",
      {
          {"bind_port", std::to_string(router_classic_ecdh_rsa_port)},
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

  // launch the server mock
  mock_server_spawner().spawn(
      // doesn't really matter which file we use here, we are not going to do
      // any queries
      mock_server_cmdline("handshake_too_many_con_error.js")
          .port(server_classic_port)
          .x_port(server_x_port)
          .args());

  const auto routing_section = mysql_harness::ConfigBuilder::build_section(
      "routing:xproto",
      {{"bind_port", std::to_string(router_port)},
       {"routing_strategy", "round-robin"},
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
                              stdx::unexpected(net::stream_errc::eof))));
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

  // launch the server mock
  mock_server_spawner().spawn(
      // doesn't really matter which file we use here, we are not going to do
      // any queries
      mock_server_cmdline("bootstrap_gr.js").port(server_port).args());

  // create a config with routing that has max_connections == 2
  const std::string routing_section =
      get_static_routing_section("A", router_port, "", {server_port}, "classic",
                                 {{"max_connections", "2"}});

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

  // launch the server mock
  mock_server_spawner().spawn(
      // doesn't really matter which file we use here, we are not going to do
      // any queries
      mock_server_cmdline("handshake_too_many_con_error.js")
          .port(server_port)
          .args());

  const std::string routing_section = get_static_routing_section(
      "basic", router_port, "", {server_port}, "classic",
      {
          {"connect_retry_timeout", "0"},
      });

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

  // launch the server mock
  mock_server_spawner().spawn(
      mock_server_cmdline("bootstrap_gr.js").port(server_port).args());

  // create a config with 2 routing sections and max_total_connections = 2
  const std::string routing_section1 = get_static_routing_section(
      "A", router_portA, "", {server_port}, "classic");
  const std::string routing_section2 = get_static_routing_section(
      "B", router_portB, "", {server_port}, "classic");

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

  // launch the server mock that will terminate all our classic and x
  // connections

  mock_server_spawner().spawn(mock_server_cmdline("bootstrap_gr.js")
                                  .port(server_classic_port)
                                  .x_port(server_x_port)
                                  .args());

  // create a configuration with 4 routes (classic rw, ro, x rw, ro)
  // each has "local" limit of 5 max_connections
  // the total_max_connections is 10
  const std::string routing_section_classic_rw = get_static_routing_section(
      "classic_rw", router_classic_rw_port, "", {server_classic_port},
      "classic", {{"max_connections", "5"}});
  const std::string routing_section_classic_ro = get_static_routing_section(
      "classic_ro", router_classic_ro_port, "", {server_classic_port},
      "classic", {{"max_connections", "5"}});

  const std::string routing_section_x_rw =
      get_static_routing_section("x_rw", router_x_rw_port, "", {server_x_port},
                                 "x", {{"max_connections", "2"}});
  const std::string routing_section_x_ro =
      get_static_routing_section("x_ro", router_x_ro_port, "", {server_x_port},
                                 "x", {{"max_connections", "2"}});

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

  // launch the server mock that will terminate all our classic and x
  // connections
  mock_server_spawner().spawn(mock_server_cmdline("bootstrap_gr.js")
                                  .port(server_classic_port)
                                  .x_port(server_x_port)
                                  .args());

  // create a configuration with 4 routes (classic rw, ro, x rw, ro)
  // each has "local" limit of 5 max_connections
  // the total_max_connections is 25
  const std::string routing_section_classic_rw = get_static_routing_section(
      "classic_rw", router_classic_rw_port, "", {server_classic_port},
      "classic", {{"max_connections", "5"}});
  const std::string routing_section_classic_ro = get_static_routing_section(
      "classic_ro", router_classic_ro_port, "", {server_classic_port},
      "classic", {{"max_connections", "5"}});

  const std::string routing_section_x_rw =
      get_static_routing_section("x_rw", router_x_rw_port, "", {server_x_port},
                                 "x", {{"max_connections", "5"}});
  const std::string routing_section_x_ro =
      get_static_routing_section("x_ro", router_x_ro_port, "", {server_x_port},
                                 "x", {{"max_connections", "5"}});

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

  // launch the server mock that will terminate all our classic and x
  // connections

  mock_server_spawner().spawn(
      mock_server_cmdline("bootstrap_gr.js").port(server_classic_port).args());

  // create a configuration with 1 route (classic rw) that has  "local" limit of
  // 600 max_connections the total_max_connections is default 512
  const std::string routing_section_classic_rw = get_static_routing_section(
      "classic_rw", router_classic_rw_port, "", {server_classic_port},
      "classic", {{"max_connections", "600"}});
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
      "routing_strategy = round-robin\n"
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
                                "routing:basic listening on '" +
                                    socket_file + "'",
                                5s));
}

TEST_F(RouterRoutingTest, named_socket_fails_with_socket_is_not_readable) {
  TempDirectory bootstrap_dir;

  // launch Router with unix socket
  const std::string socket_file = bootstrap_dir.name() + "/sockfile";

  // create the file that's not readable to trigger a permission denied check.
  {
    int fd = open(socket_file.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0000);
    ASSERT_NE(fd, -1) << errno;
    close(fd);
  }

  auto writer = config_writer(bootstrap_dir.name());

  writer.section("routing:basic", {
                                      {"socket", socket_file},
                                      {"routing_strategy", "first-available"},
                                      {"destinations", "127.0.0.1:1234"},
                                  });
  auto &router = router_spawner()
                     .wait_for_sync_point(Spawner::SyncPoint::NONE)
                     .expected_exit_code(EXIT_FAILURE)
                     .spawn({"-c", writer.write()});

  ASSERT_NO_THROW(router.wait_for_exit());

  EXPECT_THAT(router.get_logfile_content(),
              ::testing::AnyOf(
                  ::testing::HasSubstr(
                      "is bound by another process failed: Permission denied"),
                  ::testing::HasSubstr("is bound by another process failed: "
                                       "Socket operation on non-socket")));

  // check if the file still exists and hasn't been deleted
  EXPECT_EQ(access(socket_file.c_str(), F_OK), 0);
}
#endif

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

  mock_server_spawner().spawn(mock_server_cmdline("bootstrap_gr.js")
                                  .port(server_classic_port)
                                  .x_port(server_x_port)
                                  .args());

  const std::string routing_x_section = get_static_routing_section(
      "x", router_x_rw_port, "", {server_x_port}, "x");

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

  mock_server_spawner().spawn(mock_server_cmdline("bootstrap_gr.js")
                                  .port(server_classic_port)
                                  .x_port(server_x_port)
                                  .args());

  const std::string routing_x_section = get_static_routing_section(
      "x", router_x_rw_port, "", {server_x_port}, "x");

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

/**
 * @test Check that the Router logs expected debug lines when connection is
 * established and closed.
 */
TEST_F(RouterRoutingTest, ConnectionDebugLogsTcp) {
  const auto server_classic_port = port_pool_.get_next_available();
  const auto server_x_port = port_pool_.get_next_available();
  const auto router_classic_rw_port = port_pool_.get_next_available();
  const auto router_x_rw_port = port_pool_.get_next_available();

  mock_server_spawner().spawn(mock_server_cmdline("my_port.js")
                                  .port(server_classic_port)
                                  .x_port(server_x_port)
                                  .args());

  const std::string routing_classic_section = get_static_routing_section(
      "classic", router_classic_rw_port, "", {server_classic_port}, "classic");

  const std::string routing_x_section = get_static_routing_section(
      "x", router_x_rw_port, "", {server_x_port}, "x");

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(
      conf_dir.name(), routing_classic_section + routing_x_section, nullptr,
      "mysqlrouter.conf");

  // launch the router with the created configuration
  auto &router = launch_router({"-c", conf_file});

  auto check_conn_debug_logs = [&](const uint16_t accepting_port,
                                   const uint16_t dest_port) {
  // clang-format off
  // 2023-06-12 11:37:08 routing DEBUG [0x16f423000] [routing:x] fd=24 connection accepted at 127.0.0.1:11013
  // 2023-06-12 10:37:08 routing DEBUG [0x16de8f000] [routing:x] fd=26 connected 127.0.0.1:56065 -> 127.0.0.1:6001
  // 2023-06-12 10:37:12 routing DEBUG [0x16de8f000] [routing:x] fd=26 127.0.0.1:56065 -> 127.0.0.1:6001: connection closed (up: 2288b; down: 955b)
  // clang-format on
#ifdef GTEST_USES_SIMPLE_RE
    const std::string line_con_accepted_regex =
        ".* routing DEBUG .* \\[routing:.*\\] fd=\\d+ connection accepted "
        "at 127\\.0\\.0\\.1:" +
        std::to_string(accepting_port);

    const std::string line_con_connected_regex =
        ".* routing DEBUG .* \\[routing:.*\\] fd=\\d+ connected "
        "127.0.0.1:\\d+ -> 127.0.0.1:" +
        std::to_string(dest_port);

    const std::string line_con_closed_regex =
        ".* routing DEBUG .* \\[routing:.*\\] fd=\\d+ 127.0.0.1:\\d+ -> "
        "127.0.0.1:" +
        std::to_string(dest_port) +
        ": connection closed \\(up: \\d+b; down: \\d+b\\)";
#else
    const std::string line_con_accepted_regex =
        ".* routing DEBUG .* \\[routing:.*\\] fd=[0-9]+ connection accepted "
        "at 127\\.0\\.0\\.1:" +
        std::to_string(accepting_port);

    const std::string line_con_connected_regex =
        ".* routing DEBUG .* \\[routing:.*\\] fd=[0-9]+ connected "
        "127.0.0.1:[0-9]+ -> 127.0.0.1:" +
        std::to_string(dest_port);

    const std::string line_con_closed_regex =
        ".* routing DEBUG .* \\[routing:.*\\] fd=[0-9]+ "
        "127.0.0.1:[0-9]+ -> 127.0.0.1:" +
        std::to_string(dest_port) +
        ": connection closed \\(up: [0-9]+b; down: [0-9]+b\\)";
#endif

    EXPECT_TRUE(wait_log_contains(router, line_con_accepted_regex, 1s))
        << line_con_accepted_regex;
    EXPECT_TRUE(wait_log_contains(router, line_con_connected_regex, 1s))
        << line_con_connected_regex;
    EXPECT_TRUE(wait_log_contains(router, line_con_closed_regex, 1s))
        << line_con_closed_regex;
  };

  {
    // open and close classic connection
    make_new_connection(router_classic_rw_port);

    // open and close x connection
    XProtocolSession x_session;

    const auto res = make_x_connection(x_session, "127.0.0.1", router_x_rw_port,
                                       "user", "pass");
  }

  // check that there are expected debug logs for both
  check_conn_debug_logs(router_classic_rw_port, server_classic_port);
  check_conn_debug_logs(router_x_rw_port, server_x_port);
}

#ifndef _WIN32
/**
 * @test Check that the Router logs expected debug lines when connection is
 * established and closed via the Unix socket.
 */
TEST_F(RouterRoutingTest, ConnectionDebugLogsSocket) {
  const auto server_classic_port = port_pool_.get_next_available();
  const auto server_x_port = port_pool_.get_next_available();

  mock_server_spawner().spawn(mock_server_cmdline("my_port.js")
                                  .port(server_classic_port)
                                  .x_port(server_x_port)
                                  .args());

  TempDirectory conf_dir("conf");

  const std::string classic_socket =
      Path(conf_dir.name()).join("mysql.sock").str();
  const std::string x_socket = Path(conf_dir.name()).join("mysqlx.sock").str();

  const std::string routing_classic_section = get_static_routing_section(
      "classic", 0, classic_socket, {server_classic_port}, "classic");
  const std::string routing_x_section =
      get_static_routing_section("x", 0, x_socket, {server_x_port}, "x");

  std::string conf_file = create_config_file(
      conf_dir.name(), routing_classic_section + routing_x_section, nullptr,
      "mysqlrouter.conf");

  // launch the router with the created configuration
  auto &router = launch_router({"-c", conf_file});

  auto check_conn_debug_logs = [&](const std::string &socket,
                                   const uint16_t dest_port) {
  // clang-format off
  // 2023-06-12 09:19:37 routing DEBUG [0x170353000] [routing:bootstrap_rw] fd=37 connection accepted at /tmp/mysql.sock
  // 2023-06-12 09:19:37 routing DEBUG [0x16fddb000] [routing:bootstrap_rw] connected /tmp/mysql.sock -> 127.0.0.1:6000
  // 2023-06-12 09:19:51 routing DEBUG [0x16fddb000] [routing:bootstrap_rw] /tmp/mysql.sock -> 127.0.0.1:6000: connection closed (up: 344b; down: 875b)
  // clang-format on
#ifdef GTEST_USES_SIMPLE_RE
    const std::string line_con_accepted_regex =
        ".* routing DEBUG .* \\[routing:.*\\] fd=\\d+ connection accepted "
        "at " +
        socket;

    const std::string line_con_connected_regex =
        ".* routing DEBUG .* \\[routing:.*\\] fd=\\d+ connected " + socket +
        " -> 127.0.0.1:" + std::to_string(dest_port);

    const std::string line_con_closed_regex =
        ".* routing DEBUG .* \\[routing:.*\\] " + socket +
        " -> 127.0.0.1:" + std::to_string(dest_port) +
        ": connection closed \\(up: \\d+b; down: \\d+b\\)";
#else
    const std::string line_con_accepted_regex =
        ".* routing DEBUG .* \\[routing:.*\\] fd=[0-9]+ connection accepted "
        "at " +
        socket;

    const std::string line_con_connected_regex =
        ".* routing DEBUG .* \\[routing:.*\\] fd=[0-9]+ connected " + socket +
        " -> 127.0.0.1:" + std::to_string(dest_port);

    const std::string line_con_closed_regex =
        ".* routing DEBUG .* \\[routing:.*\\] fd=[0-9]+ " + socket +
        " -> 127.0.0.1:" + std::to_string(dest_port) +
        ": connection closed \\(up: [0-9]+b; down: [0-9]+b\\)";
#endif
    EXPECT_TRUE(wait_log_contains(router, line_con_accepted_regex, 1s))
        << line_con_accepted_regex;
    EXPECT_TRUE(wait_log_contains(router, line_con_connected_regex, 1s))
        << line_con_connected_regex;
    EXPECT_TRUE(wait_log_contains(router, line_con_closed_regex, 1s))
        << line_con_closed_regex;
  };

  {
    // open and close classic connection
    make_new_connection(classic_socket);

    // open and close x connection
    XProtocolSession x_session;

    const auto res = make_x_connection(x_session, x_socket, "user", "pass");
  }

  // check that there are expected debug logs for both
  check_conn_debug_logs(classic_socket, server_classic_port);
  check_conn_debug_logs(x_socket, server_x_port);
}
#endif

using OptionalStr = std::optional<std::string>;

struct SslSessionCacheConfig {
  OptionalStr client_ssl_session_cache_mode;
  OptionalStr client_ssl_session_cache_size;
  OptionalStr client_ssl_session_cache_timeout;
  OptionalStr server_ssl_session_cache_mode;
  OptionalStr server_ssl_session_cache_size;
  OptionalStr server_ssl_session_cache_timeout;
};

class RoutingSessionReuseTest : public RouterRoutingTest {
 protected:
  std::vector<ConfigBuilder::kv_type> to_config_options(
      const SslSessionCacheConfig &c) {
    std::vector<ConfigBuilder::kv_type> result;

    if (c.client_ssl_session_cache_mode)
      result.emplace_back("client_ssl_session_cache_mode",
                          *c.client_ssl_session_cache_mode);
    if (c.client_ssl_session_cache_size)
      result.emplace_back("client_ssl_session_cache_size",
                          *c.client_ssl_session_cache_size);
    if (c.client_ssl_session_cache_timeout)
      result.emplace_back("client_ssl_session_cache_timeout",
                          *c.client_ssl_session_cache_timeout);

    if (c.server_ssl_session_cache_mode)
      result.emplace_back("server_ssl_session_cache_mode",
                          *c.server_ssl_session_cache_mode);
    if (c.server_ssl_session_cache_size)
      result.emplace_back("server_ssl_session_cache_size",
                          *c.server_ssl_session_cache_size);
    if (c.server_ssl_session_cache_timeout)
      result.emplace_back("server_ssl_session_cache_timeout",
                          *c.server_ssl_session_cache_timeout);

    return result;
  }

  void get_cache_hits_classic(uint16_t dest_port, size_t &result) {
    // connect with no SSL to not affect the SSL related counters and check the
    // cache hits number

    MySQLSession session_no_ssl;
    session_no_ssl.set_ssl_options(mysql_ssl_mode::SSL_MODE_DISABLED, "", "",
                                   "", "", "", "");
    ASSERT_NO_FATAL_FAILURE(session_no_ssl.connect(
        "127.0.0.1", dest_port, "username", "password", "", ""));
    std::unique_ptr<mysqlrouter::MySQLSession::ResultRow> resultset{
        session_no_ssl.query_one("SHOW STATUS LIKE 'Ssl_session_cache_hits'")};
    ASSERT_NE(nullptr, resultset.get());
    ASSERT_EQ(1u, resultset->size());
    result = std::atoi((*resultset)[0]);
  }

  void get_cache_hits_x(uint16_t dest_port, size_t &result) {
    // connect with no SSL to not affect the SSL related counters and check the
    // cache hits number
    XProtocolSession x_session_no_ssl;
    const auto res =
        make_x_connection(x_session_no_ssl, "127.0.0.1", dest_port, "username",
                          "password", 2000, "DISABLED");
    ASSERT_EQ(res.error(), 0);

    xcl::XError xerr;
    const auto resultset = x_session_no_ssl->execute_sql(
        "SHOW STATUS LIKE 'Ssl_session_cache_hits'", &xerr);
    ASSERT_TRUE(resultset) << xerr;

    const auto row = resultset->get_next_row();
    ASSERT_NE(row, nullptr);
    int64_t cache_hits;
    ASSERT_TRUE(row->get_int64(0, &cache_hits));

    result = static_cast<size_t>(cache_hits);
  }

  void check_session_reuse_classic(const uint16_t port,
                                   const bool expected_reuse_client,
                                   const bool expected_reuse_server,
                                   const size_t expected_server_reuse_counter,
                                   std::string &out_performance) {
    uint16_t dest_port{};
    {
      MySQLSession session;
      session.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "", "", "", "",
                              "", "");
      const auto start = std::chrono::steady_clock::now();
      ASSERT_NO_FATAL_FAILURE(
          session.connect("127.0.0.1", port, "username", "password", "", ""));
      const auto stop = std::chrono::steady_clock::now();

      std::stringstream oss;
      oss << "[Classic] client: "
          << (expected_reuse_client ? "reused"s : "not reused"s)
          << "; server: " << (expected_reuse_server ? "reused"s : "not reused"s)
          << "; conn_time="
          << std::chrono::duration_cast<std::chrono::microseconds>(stop - start)
          << "us\n";
      out_performance += oss.str();

      const bool is_reused = session.is_ssl_session_reused();
      EXPECT_EQ(expected_reuse_client, is_reused);

      std::unique_ptr<MySQLSession::ResultRow> result{
          session.query_one("select @@port")};
      dest_port = static_cast<uint16_t>(std::stoul(std::string((*result)[0])));
    }

    size_t cache_hits;
    get_cache_hits_classic(dest_port, cache_hits);

    const size_t expected_hits =
        expected_reuse_server ? expected_server_reuse_counter : 0;
    EXPECT_EQ(expected_hits, cache_hits);
  }

  void check_session_reuse_x(const uint16_t port,
                             const bool expected_reuse_client,
                             const bool expected_reuse_server,
                             const size_t expected_server_reuse_counter,
                             std::string &out_performance) {
    uint16_t dest_port{};
    {
      XProtocolSession x_session;
      const auto start = std::chrono::steady_clock::now();
      const auto res =
          make_x_connection(x_session, "127.0.0.1", port, "username",
                            "password", 2000, "REQUIRED");
      const auto stop = std::chrono::steady_clock::now();

      std::stringstream oss;
      oss << "[X] client: "
          << (expected_reuse_client ? "reused"s : "not reused"s)
          << "; server: " << (expected_reuse_server ? "reused"s : "not reused"s)
          << "; conn_time="
          << std::chrono::duration_cast<std::chrono::microseconds>(stop - start)
          << "us\n";
      out_performance += oss.str();
      ASSERT_EQ(res.error(), 0);

      xcl::XError xerr;
      const auto result = x_session->execute_sql("select @@port", &xerr);
      ASSERT_TRUE(result) << xerr;

      const auto row = result->get_next_row();
      ASSERT_NE(row, nullptr);
      int64_t dest_port_int64;
      ASSERT_TRUE(row->get_int64(0, &dest_port_int64));
      dest_port = static_cast<uint16_t>(dest_port_int64);
    }

    size_t cache_hits;
    get_cache_hits_x(dest_port, cache_hits);

    const size_t expected_hits =
        expected_reuse_server ? expected_server_reuse_counter : 0;
    EXPECT_EQ(expected_hits, cache_hits);
  }

  void launch_destinations(const size_t num) {
    for (size_t i = 0; i < num; i++) {
      dest_classic_ports_.emplace_back(port_pool_.get_next_available());
      dest_x_ports_.emplace_back(port_pool_.get_next_available());
      dest_http_ports_.emplace_back(port_pool_.get_next_available());
    }

    for (size_t i = 0; i < num; i++) {
      mock_server_spawner().spawn(mock_server_cmdline("my_port.js")
                                      .port(dest_classic_ports_[i])
                                      .http_port(dest_http_ports_[i])
                                      .x_port(dest_x_ports_[i])
                                      .enable_ssl(true)
                                      .args());
    }
  }

  ProcessWrapper &launch_router(const SslSessionCacheConfig &conf,
                                const int expected_exit_code) {
    router_classic_port_ = port_pool_.get_next_available();
    router_x_port_ = port_pool_.get_next_available();

    if (dest_classic_ports_.empty()) {
      dest_classic_ports_.push_back(port_pool_.get_next_available());
    }
    if (dest_x_ports_.empty()) {
      dest_x_ports_.push_back(port_pool_.get_next_available());
    }

    const std::string routing_classic_section = get_static_routing_section(
        "classic", router_classic_port_, "", dest_classic_ports_, "classic",
        to_config_options(conf));

    const std::string routing_x_section = get_static_routing_section(
        "x", router_x_port_, "", dest_x_ports_, "x", to_config_options(conf));

    const std::string server_ssl_mode = "REQUIRED";
    const std::string client_ssl_mode = "REQUIRED";
    std::vector<std::string> ssl_conf{
        "server_ssl_mode="s + server_ssl_mode,
        "client_ssl_mode="s + client_ssl_mode,
        "client_ssl_key=" SSL_TEST_DATA_DIR "/server-key-sha512.pem",
        "client_ssl_cert=" SSL_TEST_DATA_DIR "/server-cert-sha512.pem"};

    std::string conf_file = create_config_file(
        conf_dir_.name(), routing_classic_section + routing_x_section, nullptr,
        "mysqlrouter.conf", mysql_harness::join(ssl_conf, "\n"));

    // launch the router with the created configuration
    const auto wait_notify_ready =
        expected_exit_code == EXIT_SUCCESS ? 30s : -1s;

    return RouterComponentTest::launch_router(
        {"-c", conf_file}, expected_exit_code, true, false, wait_notify_ready);
  }

  std::vector<uint16_t> dest_classic_ports_;
  std::vector<uint16_t> dest_x_ports_;
  std::vector<uint16_t> dest_http_ports_;
  uint16_t router_classic_port_;
  uint16_t router_x_port_;

  TempDirectory conf_dir_{"conf"};
};

struct SessionReuseTestParam {
  std::string test_name;
  std::string test_requirements;
  std::string test_description;

  SslSessionCacheConfig config;

  bool expect_client_session_reuse;
  bool expect_server_session_reuse;
};

class RoutingSessionReuseTestWithParams
    : public RoutingSessionReuseTest,
      public ::testing::WithParamInterface<SessionReuseTestParam> {};

TEST_P(RoutingSessionReuseTestWithParams, Spec) {
  const size_t kDestinations = 1;
  const auto test_param = GetParam();
  const bool client_reuse = test_param.expect_client_session_reuse;
  const bool server_reuse = test_param.expect_server_session_reuse;
  std::string performance;

  RecordProperty("Worklog", "15573");
  RecordProperty("RequirementId", test_param.test_requirements);
  RecordProperty("Description", test_param.test_description);

  launch_destinations(kDestinations);

  launch_router(test_param.config, EXIT_SUCCESS);

  SCOPED_TRACE(
      "// check if server-side and client-side sessions are reused as "
      "expected");
  check_session_reuse_classic(router_classic_port_, false, false, 0,
                              performance);
  check_session_reuse_classic(router_classic_port_, client_reuse, server_reuse,
                              1, performance);
  check_session_reuse_classic(router_classic_port_, client_reuse, server_reuse,
                              2, performance);

  check_session_reuse_x(router_x_port_, false, false, 0, performance);
  check_session_reuse_x(router_x_port_, false, server_reuse, 1, performance);
  check_session_reuse_x(router_x_port_, false, server_reuse, 2, performance);

  RecordProperty("AdditionalInfo", performance);
}

INSTANTIATE_TEST_SUITE_P(
    Spec, RoutingSessionReuseTestWithParams,

    ::testing::Values(
        SessionReuseTestParam{
            "all_options_default",
            "FR01,FR05,FR09,FR10,FR11,FR13,FR14",
            "all session cache params are default so we expect session reuse",
            {/* client_ssl_session_cache_mode */ std::nullopt,
             /* client_ssl_session_cache_size */ std::nullopt,
             /* client_ssl_session_cache_timeout */ std::nullopt,
             /* server_ssl_session_cache_mode */ std::nullopt,
             /* server_ssl_session_cache_size */ std::nullopt,
             /* server_ssl_session_cache_timeout */ std::nullopt},
            /*expect_client_session_reuse*/ true,
            /*expect_server_session_reuse*/ true},
        SessionReuseTestParam{
            "server_cache_disabled_client_default",
            "FR01,FR09,FR13",
            "`server_ssl_session_cache_mode` is 0 so no server side reusing "
            "expected, client side is default so should be reused",
            {/* client_ssl_session_cache_mode */ std::nullopt,
             /* client_ssl_session_cache_size */ std::nullopt,
             /* client_ssl_session_cache_timeout */ std::nullopt,
             /* server_ssl_session_cache_mode */ "0",
             /* server_ssl_session_cache_size */ std::nullopt,
             /* server_ssl_session_cache_timeout */ std::nullopt},
            /*expect_client_session_reuse*/ true,
            /*expect_server_session_reuse*/ false},
        SessionReuseTestParam{
            "client_cache_disabled_server_default",
            "FR05,FR09,FR14",
            "`client_ssl_session_cache_mode` is 0 so no client side reusing "
            "expected, server side is default so should be reused",
            {/* client_ssl_session_cache_mode */ "0",
             /* client_ssl_session_cache_size */ std::nullopt,
             /* client_ssl_session_cache_timeout */ std::nullopt,
             /* server_ssl_session_cache_mode */ std::nullopt,
             /* server_ssl_session_cache_size */ std::nullopt,
             /* server_ssl_session_cache_timeout */ std::nullopt},
            /*expect_client_session_reuse*/ false,
            /*expect_server_session_reuse*/ true},
        SessionReuseTestParam{
            "client_cache_disabled_server_cache_disabled",
            "FR12",
            "both `client_ssl_session_cache_mode` and "
            "`server_ssl_session_cache_mode` are 0, no "
            "resumption expected on both client and server",
            {/* client_ssl_session_cache_mode */ "0",
             /* client_ssl_session_cache_size */ std::nullopt,
             /* client_ssl_session_cache_timeout */ std::nullopt,
             /* server_ssl_session_cache_mode */ "0",
             /* server_ssl_session_cache_size */ std::nullopt,
             /* server_ssl_session_cache_timeout */ std::nullopt},
            /*expect_client_session_reuse*/ false,
            /*expect_server_session_reuse*/ false},
        SessionReuseTestParam{
            "client_cache_enabled_server_cache_enabled",
            "FR01,FR02,FR05,FR06,FR09,FR10,FR11",
            "both `client_ssl_session_cache_mode` and "
            "`server_ssl_session_cache_mode` are explicitly 1",
            {/* client_ssl_session_cache_mode */ "1",
             /* client_ssl_session_cache_size */ "2",
             /* client_ssl_session_cache_timeout */ std::nullopt,
             /* server_ssl_session_cache_mode */ "1",
             /* server_ssl_session_cache_size */ "2",
             /* server_ssl_session_cache_timeout */ std::nullopt},
            /*expect_client_session_reuse*/ true,
            /*expect_server_session_reuse*/ true}),
    [](const ::testing::TestParamInfo<SessionReuseTestParam> &info) {
      return info.param.test_name;
    });

class RoutingClientSessionReuseCacheTimeoutTest
    : public RoutingSessionReuseTest,
      public ::testing::WithParamInterface<SessionReuseTestParam> {};

TEST_P(RoutingClientSessionReuseCacheTimeoutTest, Spec) {
  const size_t kDestinations = 1;
  const auto test_param = GetParam();
  std::string performance;
  RecordProperty("Worklog", "15573");
  RecordProperty("RequirementId", test_param.test_requirements);
  RecordProperty("Description", test_param.test_description);

  launch_destinations(kDestinations);

  launch_router(test_param.config, EXIT_SUCCESS);

  SCOPED_TRACE("// check if server-side sessions are reused as expected");
  check_session_reuse_classic(router_classic_port_, false, false, 0,
                              performance);
  // we wait for 2 seconds to verify if the cache timeout is handled properly
  // (the session expired/reused or not depending on the test params)
  std::this_thread::sleep_for(2s);
  check_session_reuse_classic(router_classic_port_,
                              test_param.expect_client_session_reuse, false, 0,
                              performance);

  RecordProperty("AdditionalInfo", performance);
}

INSTANTIATE_TEST_SUITE_P(
    Spec, RoutingClientSessionReuseCacheTimeoutTest,

    ::testing::Values(
        SessionReuseTestParam{
            "client_session_expired",
            "FR03,FR04",
            "`client_ssl_session_cache_timeout` is 1s so after 2 seconds the "
            "session should not be reused",
            {/* client_ssl_session_cache_mode */ std::nullopt,
             /* client_ssl_session_cache_size */ std::nullopt,
             /* client_ssl_session_cache_timeout */ "1",
             /* server_ssl_session_cache_mode */ "0",
             /* server_ssl_session_cache_size */ std::nullopt,
             /* server_ssl_session_cache_timeout */ std::nullopt},
            /*expect_client_session_reuse*/ false,
            /*expect_server_session_reuse*/ true},
        SessionReuseTestParam{
            "client_session_not_expired",
            "FR03",
            "`client_ssl_session_cache_timeout` is 5s so after 2 seconds the "
            "session should be reused",
            {/* client_ssl_session_cache_mode */ std::nullopt,
             /* client_ssl_session_cache_size */ std::nullopt,
             /* client_ssl_session_cache_timeout */ "5",
             /* server_ssl_session_cache_mode */ "0",
             /* server_ssl_session_cache_size */ std::nullopt,
             /* server_ssl_session_cache_timeout */ std::nullopt},
            /*expect_client_session_reuse*/ true,
            /*expect_server_session_reuse*/ true}),
    [](const ::testing::TestParamInfo<SessionReuseTestParam> &info) {
      return info.param.test_name;
    });

class RoutingServerSessionReuseCacheTimeoutTest
    : public RoutingSessionReuseTest,
      public ::testing::WithParamInterface<SessionReuseTestParam> {};

TEST_P(RoutingServerSessionReuseCacheTimeoutTest, Spec) {
  const size_t kDestinations = 1;
  const auto test_param = GetParam();
  std::string performance;
  RecordProperty("Worklog", "15573");
  RecordProperty("RequirementId", test_param.test_requirements);
  RecordProperty("Description", test_param.test_description);

  launch_destinations(kDestinations);

  launch_router(test_param.config, EXIT_SUCCESS);

  SCOPED_TRACE("// check if server-side sessions are reused as expected");
  check_session_reuse_classic(router_classic_port_, false, false, 0,
                              performance);
  // we wait for 2 seconds to verify if the cache timeout is handled properly
  // (the session expired/reused or not depending on the test params)
  std::this_thread::sleep_for(2s);
  check_session_reuse_classic(
      router_classic_port_, false, test_param.expect_server_session_reuse,
      test_param.expect_server_session_reuse ? 1 : 0, performance);

  RecordProperty("AdditionalInfo", performance);
}

INSTANTIATE_TEST_SUITE_P(
    Spec, RoutingServerSessionReuseCacheTimeoutTest,
    ::testing::Values(
        SessionReuseTestParam{
            "server_session_expired",
            "FR07,FR08",
            "`server_ssl_session_cache_timeout` is 1s so after 2 seconds the "
            "session should not be reused",
            {/* client_ssl_session_cache_mode */ "0",
             /* client_ssl_session_cache_size */ std::nullopt,
             /* client_ssl_session_cache_timeout */ std::nullopt,
             /* server_ssl_session_cache_mode */ std::nullopt,
             /* server_ssl_session_cache_size */ std::nullopt,
             /* server_ssl_session_cache_timeout */ "1"},
            /*expect_client_session_reuse*/ true,
            /*expect_server_session_reuse*/ false},
        SessionReuseTestParam{
            "server_session_not_expired",
            "FR07",
            "`server_ssl_session_cache_timeout` is 5s so after 2 seconds the "
            "session should be reused",
            {/* client_ssl_session_cache_mode */ "0",
             /* client_ssl_session_cache_size */ std::nullopt,
             /* client_ssl_session_cache_timeout */ std::nullopt,
             /* server_ssl_session_cache_mode */ std::nullopt,
             /* server_ssl_session_cache_size */ std::nullopt,
             /* server_ssl_session_cache_timeout */ "5"},
            /*expect_client_session_reuse*/ true,
            /*expect_server_session_reuse*/ true}),
    [](const ::testing::TestParamInfo<SessionReuseTestParam> &info) {
      return info.param.test_name;
    });

struct SessionReuseInvalidOptionValueParam {
  std::string test_name;
  SslSessionCacheConfig config;

  std::string expected_error;
};

class RoutingSessionReuseInvalidOptionValueTest
    : public RoutingSessionReuseTest,
      public ::testing::WithParamInterface<
          SessionReuseInvalidOptionValueParam> {};

TEST_P(RoutingSessionReuseInvalidOptionValueTest, Spec) {
  const auto test_param = GetParam();

  auto &router = launch_router(test_param.config, EXIT_FAILURE);
  EXPECT_NO_THROW(router.wait_for_exit());

  check_log_contains(router, test_param.expected_error);
}

INSTANTIATE_TEST_SUITE_P(
    Spec, RoutingSessionReuseInvalidOptionValueTest,

    ::testing::Values(
        SessionReuseInvalidOptionValueParam{
            "client_ssl_session_cache_mode_negative",
            {/* client_ssl_session_cache_mode */ "-1", std::nullopt,
             std::nullopt, std::nullopt, std::nullopt, std::nullopt},
            "Configuration error: option client_ssl_session_cache_mode in "
            "[routing:classic] needs a value of either 0, 1, false or true, "
            "was '-1'"},
        SessionReuseInvalidOptionValueParam{
            "client_ssl_session_cache_mode_out_of_range",
            {/* client_ssl_session_cache_mode */ "2", std::nullopt,
             std::nullopt, std::nullopt, std::nullopt, std::nullopt},
            "Configuration error: option client_ssl_session_cache_mode in "
            "[routing:classic] needs a value of either 0, 1, false or true, "
            "was '2"},
        SessionReuseInvalidOptionValueParam{
            "client_ssl_session_cache_mode_not_integer",
            {/* client_ssl_session_cache_mode */ "a", std::nullopt,
             std::nullopt, std::nullopt, std::nullopt, std::nullopt},
            "Configuration error: option client_ssl_session_cache_mode in "
            "[routing:classic] needs a value of either 0, 1, false or true, "
            "was 'a"},
        SessionReuseInvalidOptionValueParam{
            "client_ssl_session_cache_mode_special_character",
            {/* client_ssl_session_cache_mode */ "$", std::nullopt,
             std::nullopt, std::nullopt, std::nullopt, std::nullopt},
            "Configuration error: option client_ssl_session_cache_mode in "
            "[routing:classic] needs a value of either 0, 1, false or true, "
            "was "
            "'$"},

        SessionReuseInvalidOptionValueParam{
            "client_ssl_session_cache_size_zero",
            {std::nullopt, /* client_ssl_session_cache_size */ "0",
             std::nullopt, std::nullopt, std::nullopt, std::nullopt},
            "Configuration error: option client_ssl_session_cache_size in "
            "[routing:classic] needs value between 1 and 2147483647 inclusive, "
            "was '0'"},
        SessionReuseInvalidOptionValueParam{
            "client_ssl_session_cache_size_out_of_range",
            {std::nullopt, /* client_ssl_session_cache_size */ "2147483648",
             std::nullopt, std::nullopt, std::nullopt, std::nullopt},
            "Configuration error: option client_ssl_session_cache_size in "
            "[routing:classic] needs value between 1 and 2147483647 inclusive, "
            "was '2147483648'"},
        SessionReuseInvalidOptionValueParam{
            "client_ssl_session_cache_size_not_integer",
            {std::nullopt, /* client_ssl_session_cache_size */ "a",
             std::nullopt, std::nullopt, std::nullopt, std::nullopt},
            "Configuration error: option client_ssl_session_cache_size in "
            "[routing:classic] needs value between 1 and 2147483647 inclusive, "
            "was 'a'"},
        SessionReuseInvalidOptionValueParam{
            "client_ssl_session_cache_size_special_character",
            {std::nullopt, /* client_ssl_session_cache_size */ "$",
             std::nullopt, std::nullopt, std::nullopt, std::nullopt},
            "Configuration error: option client_ssl_session_cache_size in "
            "[routing:classic] needs value between 1 and 2147483647 inclusive, "
            "was '$'"},

        SessionReuseInvalidOptionValueParam{
            "client_ssl_session_cache_timeout_negative",
            {std::nullopt, std::nullopt,
             /* client_ssl_session_cache_timeout */ "-1", std::nullopt,
             std::nullopt, std::nullopt},
            "Configuration error: option client_ssl_session_cache_timeout in "
            "[routing:classic] needs value between 0 and 84600 inclusive, "
            "was '-1'"},
        SessionReuseInvalidOptionValueParam{
            "client_ssl_session_cache_timeout_out_of_range",
            {std::nullopt, std::nullopt,
             /* client_ssl_session_cache_timeout */ "84601", std::nullopt,
             std::nullopt, std::nullopt},
            "Configuration error: option client_ssl_session_cache_timeout in "
            "[routing:classic] needs value between 0 and 84600 inclusive, "
            "was '84601'"},
        SessionReuseInvalidOptionValueParam{
            "client_ssl_session_cache_timeout_not_integer",
            {std::nullopt, std::nullopt,
             /* client_ssl_session_cache_timeout */ "a", std::nullopt,
             std::nullopt, std::nullopt},
            "Configuration error: option client_ssl_session_cache_timeout in "
            "[routing:classic] needs value between 0 and 84600 inclusive, "
            "was 'a'"},
        SessionReuseInvalidOptionValueParam{
            "client_ssl_session_cache_timeout_special_character",
            {std::nullopt, std::nullopt,
             /* client_ssl_session_cache_timeout */ "$", std::nullopt,
             std::nullopt, std::nullopt},
            "Configuration error: option client_ssl_session_cache_timeout in "
            "[routing:classic] needs value between 0 and 84600 inclusive, "
            "was '$'"},

        // server
        SessionReuseInvalidOptionValueParam{
            "server_ssl_session_cache_mode_negative",
            {std::nullopt, std::nullopt, std::nullopt,
             /* server_ssl_session_cache_mode */ "-1", std::nullopt,
             std::nullopt},
            "Configuration error: option server_ssl_session_cache_mode in "
            "[routing:classic] needs a value of either 0, 1, false or true, "
            "was '-1'"},
        SessionReuseInvalidOptionValueParam{
            "server_ssl_session_cache_out_of_range",
            {std::nullopt, std::nullopt, std::nullopt,
             /* server_ssl_session_cache_mode */ "2", std::nullopt,
             std::nullopt},
            "Configuration error: option server_ssl_session_cache_mode in "
            "[routing:classic] needs a value of either 0, 1, false or true, "
            "was '2'"},
        SessionReuseInvalidOptionValueParam{
            "server_ssl_session_cache_mode_not_integer",
            {std::nullopt, std::nullopt, std::nullopt,
             /* server_ssl_session_cache_mode */ "a", std::nullopt,
             std::nullopt},
            "Configuration error: option server_ssl_session_cache_mode in "
            "[routing:classic] needs a value of either 0, 1, false or true, "
            "was 'a'"},
        SessionReuseInvalidOptionValueParam{
            "server_ssl_session_cache_special_character",
            {std::nullopt, std::nullopt, std::nullopt,
             /* server_ssl_session_cache_mode */ "$", std::nullopt,
             std::nullopt},
            "Configuration error: option server_ssl_session_cache_mode in "
            "[routing:classic] needs a value of either 0, 1, false or true, "
            "was '$'"},

        SessionReuseInvalidOptionValueParam{
            "server_ssl_session_cache_size_zero",
            {std::nullopt, std::nullopt, std::nullopt, std::nullopt,
             /* server_ssl_session_cache_size */ "0", std::nullopt},
            "Configuration error: option server_ssl_session_cache_size in "
            "[routing:classic] needs value between 1 and 2147483647 inclusive, "
            "was '0'"},
        SessionReuseInvalidOptionValueParam{
            "server_ssl_session_cache_size_out_of_range",
            {std::nullopt, std::nullopt, std::nullopt, std::nullopt,
             /* server_ssl_session_cache_size */ "2147483648", std::nullopt},
            "Configuration error: option server_ssl_session_cache_size in "
            "[routing:classic] needs value between 1 and 2147483647 inclusive, "
            "was '2147483648'"},
        SessionReuseInvalidOptionValueParam{
            "server_ssl_session_cache_size_not_integer",
            {std::nullopt, std::nullopt, std::nullopt, std::nullopt,
             /* server_ssl_session_cache_size */ "a", std::nullopt},
            "Configuration error: option server_ssl_session_cache_size in "
            "[routing:classic] needs value between 1 and 2147483647 inclusive, "
            "was 'a'"},
        SessionReuseInvalidOptionValueParam{
            "server_ssl_session_cache_size_special_character",
            {std::nullopt, std::nullopt, std::nullopt, std::nullopt,
             /* server_ssl_session_cache_size */ "$", std::nullopt},
            "Configuration error: option server_ssl_session_cache_size in "
            "[routing:classic] needs value between 1 and 2147483647 inclusive, "
            "was '$'"},

        SessionReuseInvalidOptionValueParam{
            "server_ssl_session_cache_timeout_negative",
            {std::nullopt, std::nullopt, std::nullopt, std::nullopt,
             std::nullopt, /* server_ssl_session_cache_timeout */ "-1"},
            "Configuration error: option server_ssl_session_cache_timeout in "
            "[routing:classic] needs value between 0 and 84600 inclusive, "
            "was '-1'"},
        SessionReuseInvalidOptionValueParam{
            "server_ssl_session_cache_timeout_out_of_range",
            {std::nullopt, std::nullopt, std::nullopt, std::nullopt,
             std::nullopt, /* server_ssl_session_cache_timeout */ "84601"},
            "Configuration error: option server_ssl_session_cache_timeout in "
            "[routing:classic] needs value between 0 and 84600 inclusive, "
            "was '84601"},
        SessionReuseInvalidOptionValueParam{
            "server_ssl_session_cache_timeout_not_integer",
            {std::nullopt, std::nullopt, std::nullopt, std::nullopt,
             std::nullopt, /* server_ssl_session_cache_timeout */ "a"},
            "Configuration error: option server_ssl_session_cache_timeout in "
            "[routing:classic] needs value between 0 and 84600 inclusive, "
            "was 'a'"},
        SessionReuseInvalidOptionValueParam{
            "server_ssl_session_cache_timeout_special_character",
            {std::nullopt, std::nullopt, std::nullopt, std::nullopt,
             std::nullopt, /* server_ssl_session_cache_timeout */ "$"},
            "Configuration error: option server_ssl_session_cache_timeout in "
            "[routing:classic] needs value between 0 and 84600 inclusive, "
            "was '$"}),
    [](const ::testing::TestParamInfo<SessionReuseInvalidOptionValueParam>
           &info) { return info.param.test_name; });

TEST_F(RoutingSessionReuseTest, ReuseAfterInvalidAuth) {
  const size_t kDestinations = 1;
  std::string performance;

  launch_destinations(kDestinations);

  launch_router({/* client_ssl_session_cache_mode */ "1",
                 /* client_ssl_session_cache_size */ std::nullopt,
                 /* client_ssl_session_cache_timeout */ std::nullopt,
                 /* server_ssl_session_cache_mode */ "1",
                 /* server_ssl_session_cache_size */ std::nullopt,
                 /* server_ssl_session_cache_timeout */ std::nullopt},
                EXIT_SUCCESS);

  SCOPED_TRACE(
      "// check if server-side and client-side sessions are reused as "
      "expected");

  auto check_server_session_reuse_invalid_auth_classic =
      [&](const size_t expected_reuse_counter) {
        MySQLSession session;
        session.set_ssl_options(mysql_ssl_mode::SSL_MODE_REQUIRED, "", "", "",
                                "", "", "");

        EXPECT_THROW_LIKE(
            session.connect("127.0.0.1", router_classic_port_, "username",
                            "invalid-password", "", ""),
            std::exception, "Access Denied for user");
        size_t cache_hits;
        get_cache_hits_classic(dest_classic_ports_[0], cache_hits);
        EXPECT_EQ(expected_reuse_counter, cache_hits);
      };

  // make a few consecutive connections with invalid auth data, check that the
  // server sessions are reused
  check_server_session_reuse_invalid_auth_classic(/*expected_reuse_counter*/ 0);
  check_server_session_reuse_invalid_auth_classic(/*expected_reuse_counter*/ 1);
  check_server_session_reuse_invalid_auth_classic(/*expected_reuse_counter*/ 2);

  auto check_server_session_reuse_invalid_auth_x =
      [&](const size_t expected_reuse_counter) {
        XProtocolSession x_session;
        const auto res =
            make_x_connection(x_session, "127.0.0.1", router_x_port_,
                              "username", "password", 2000, "REQUIRED");
        size_t cache_hits;
        get_cache_hits_x(dest_x_ports_[0], cache_hits);
        EXPECT_EQ(expected_reuse_counter, cache_hits);
      };

  // make a few consecutive connections with invalid auth data, check that the
  // server sessions are reused
  check_server_session_reuse_invalid_auth_x(/*expected_reuse_counter*/ 0);
  check_server_session_reuse_invalid_auth_x(/*expected_reuse_counter*/ 1);
  check_server_session_reuse_invalid_auth_x(/*expected_reuse_counter*/ 2);
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
