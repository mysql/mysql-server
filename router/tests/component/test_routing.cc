/*
  Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.

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
#include <cstring>
#include <stdexcept>
#include <thread>
#include <typeinfo>

#include <gmock/gmock.h>

#include "router_config.h"  // defines HAVE_PRLIMIT
#ifdef HAVE_PRLIMIT
#include <sys/resource.h>  // prlimit()
#endif
#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#else
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "mysql_session.h"
#include "router_component_test.h"
#include "router_test_helpers.h"
#include "socket_operations.h"
#include "tcp_port_pool.h"

using namespace std::chrono_literals;

using mysql_harness::SocketOperations;

using mysqlrouter::MySQLSession;

class RouterRoutingTest : public RouterComponentTest {
 protected:
  TcpPortPool port_pool_;
};

TEST_F(RouterRoutingTest, RoutingOk) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // use the json file that adds additional rows to the metadata to increase the
  // packet size to +10MB to verify routing of the big packets
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();
  TempDirectory bootstrap_dir;

  // launch the server mock for bootstrapping
  auto &server_mock = launch_mysql_server_mock(
      json_stmts, server_port,
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
  auto &router_static = launch_router({"-c", conf_file});

  // wait for both to begin accepting the connections
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router_static, router_port));

  // launch another router to do the bootstrap connecting to the mock server
  // via first router instance
  auto &router_bootstrapping = launch_router({
      "--bootstrap=localhost:" + std::to_string(router_port),
      "--report-host",
      "dont.query.dns",
      "-d",
      bootstrap_dir.name(),
  });

  router_bootstrapping.register_response(
      "Please enter MySQL password for root: ", "fake-pass\n");

  ASSERT_NO_FATAL_FAILURE(check_exit_code(router_bootstrapping, EXIT_SUCCESS));

  ASSERT_TRUE(router_bootstrapping.expect_output(
      "MySQL Router configured for the InnoDB cluster 'mycluster'"))
      << "bootstrap output: " << router_bootstrapping.get_full_output()
      << std::endl
      << "routing log: " << router_bootstrapping.get_full_logfile() << std::endl
      << "server output: " << server_mock.get_full_output() << std::endl;
}

TEST_F(RouterRoutingTest, RoutingTooManyConnections) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // doesn't really matter which file we use here, we are not going to do any
  // queries
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  // launch the server mock
  auto &server_mock = launch_mysql_server_mock(json_stmts, server_port, false);

  // create a config with routing that has max_connections == 2
  const std::string routing_section =
      "[routing:basic]\n"
      "bind_port = " +
      std::to_string(router_port) +
      "\n"
      "mode = read-write\n"
      "max_connections = 2\n"
      "destinations = 127.0.0.1:" +
      std::to_string(server_port) + "\n";

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), routing_section);

  // launch the router with the created configuration
  auto &router = launch_router({"-c", conf_file});

  // wait for server and router to begin accepting the connections
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port));

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
    // as T may be std::exception we can't use it as default case and need to do
    // this extra round
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

// this test uses OS-specific methods to restrict thread creation
#ifdef HAVE_PRLIMIT
TEST_F(RouterRoutingTest, RoutingPluginCantSpawnMoreThreads) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // doesn't really matter which file we use here, we are not going to do any
  // queries
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  // launch the server mock
  auto &server_mock = launch_mysql_server_mock(json_stmts, server_port, false);

  // create a basic config
  //
  // DEBUG is needed to synchronize with 'Running.' from the Loader::main_loop()
  // to get a stable test.
  const std::string routing_section =
      "[logger]\n"
      "level = DEBUG\n"
      "[routing:basic]\n"
      "bind_port = " +
      std::to_string(router_port) +
      "\n"
      "mode = read-write\n"
      "destinations = 127.0.0.1:" +
      std::to_string(server_port) + "\n";

  TempDirectory conf_dir("conf");
  std::string conf_file = create_config_file(conf_dir.name(), routing_section);

  SCOPED_TRACE("// launch the router with the created configuration");
  auto &router_static = launch_router({"-c", conf_file});

  SCOPED_TRACE("// capture current NPROC");
  pid_t pid = router_static.get_pid();

  struct rlimit old_limit;
  EXPECT_EQ(0, prlimit(pid, RLIMIT_NPROC, nullptr, &old_limit));

  SCOPED_TRACE(
      "// wait for server and router to begin accepting the connections");
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router_static, router_port));

  // without waiting we may otherwise hit a race between the
  // signal-handler-thread and the plugin-start thread and get different
  // test-scenario which leads to "exit-code 1"
  SCOPED_TRACE("// wait for Loader::main_loop() to start");
  EXPECT_TRUE(find_in_file(
      get_logging_dir().join("mysqlrouter.log").str(),
      [](const auto &line) { return pattern_found(line, " Running."); }, 1s))
      << router_static.get_full_logfile();

  SCOPED_TRACE("// reducing NPROC to 0");
  {
    // how many threads Router process is allowed to have. If this number is
    // lower than current count, nothing will happen, but new ones will not be
    // allowed to be created until count comes down below this limit. Thus 0 is
    // a nice number to ensure nothing gets spawned anymore.
    rlim_t max_threads = 0;

    struct rlimit new_limit {
      .rlim_cur = max_threads, .rlim_max = old_limit.rlim_max
    };
    EXPECT_EQ(0, prlimit(pid, RLIMIT_NPROC, &new_limit, nullptr));
  }

  // try to create a new connection which should fail as:
  //
  // - std::thread() in routing plugin will fail to spawn a new thread for this
  //   new connection
  //   ... which returns the client "Router couldn't spawn new threads"
  //
  SCOPED_TRACE(
      "// opening connection which creates a new thread which should fail.");
  mysqlrouter::MySQLSession client1;
  EXPECT_TRUE(ThrowsExceptionWith<std::runtime_error>(
      [&client1, router_port]() {
        client1.connect("127.0.0.1", router_port, "root", "fake-pass", "", "");
      },
      "Router couldn't spawn a new thread to service new client connection "
      "(1040)"))
      << "mock: " << server_mock.get_full_logfile() << "\n"
      << "router: " << router_static.get_full_logfile();

  SCOPED_TRACE("// restoring old NPROC.");
  // we need to restore the old limit, otherwise ASAN can't spawn the thread
  // that it needs on shutdown and crashes
  EXPECT_EQ(0, prlimit(pid, RLIMIT_NPROC, &old_limit, nullptr));

  SCOPED_TRACE("// stopping router and wait for shutdown.");
  EXPECT_EQ(router_static.send_clean_shutdown_event(), std::error_code{});
  EXPECT_NO_THROW(EXPECT_EQ(router_static.wait_for_exit(), 0)
                  << router_static.get_full_logfile())
      << router_static.get_full_logfile();

  SCOPED_TRACE("// check for expected content.");
  EXPECT_THAT(router_static.get_full_logfile(),
              ::testing::ContainsRegex("Couldn't spawn a new thread to "
                                       "service new client connection"));
}
#endif  // #ifndef HAVE_PRLIMIT

#ifndef _WIN32  // named sockets are not supported on Windows;
                // on Unix, they're implemented using Unix sockets
TEST_F(RouterRoutingTest, named_socket_has_right_permissions) {
  /**
   * @test Verify that unix socket has the required file permissions so that it
   *       can be connected to by all users. According to man 7 unix, only r+w
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
  launch_router({"-c", conf_file});

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
}
#endif

TEST_F(RouterRoutingTest, RoutingMaxConnectErrors) {
  const auto server_port = port_pool_.get_next_available();
  const auto router_port = port_pool_.get_next_available();

  // json file does not actually matter in this test as we are not going to
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();
  TempDirectory bootstrap_dir;

  // launch the server mock for bootstrapping
  auto &server_mock = launch_mysql_server_mock(
      json_stmts, server_port,
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

  // wait for mock server to begin accepting the connections
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  // wait for router to begin accepting the connections
  // NOTE: this should cause connection/disconnection which
  //       should be treated as connection error and increment
  //       connection errors counter.  This test relies on that.
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port));

  // wait until blocking client host info appears in the log
  bool res =
      find_in_file(get_logging_dir().str() + "/mysqlrouter.log",
                   [](const std::string &line) -> bool {
                     return line.find("blocking client host") != line.npos;
                   });

  ASSERT_TRUE(res) << "Did not found expected entry in log file";

  // for the next connection attempt we should get an error as the
  // max_connect_errors was exceeded
  MySQLSession client;
  EXPECT_THROW_LIKE(
      client.connect("127.0.0.1", router_port, "root", "fake-pass", "", ""),
      std::exception, "Too many connection errors");
}

// in following functions we use SocketOperations for convenience: it provides
// Win and Unix implemenatations where needed, thus saving us some #ifdefs

static bool connect_to_host(int &sock, uint16_t port) {
  SocketOperations *so = SocketOperations::instance();

  struct addrinfo hints, *ainfo;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status =
      getaddrinfo("127.0.0.1", std::to_string(port).c_str(), &hints, &ainfo);
  if (status != 0)
    throw std::runtime_error(std::string("getaddrinfo() failed: ") +
                             gai_strerror(status));

  std::shared_ptr<void> exit_freeaddrinfo(nullptr,
                                          [&](void *) { freeaddrinfo(ainfo); });

  sock = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
  if (sock < 0)
    throw std::runtime_error("socket() failed: " +
                             std::to_string(so->get_errno()));

  // return connection success
  return (connect(sock, ainfo->ai_addr, ainfo->ai_addrlen) == 0);
}

static void read_until_error(int sock) {
  SocketOperations *so = SocketOperations::instance();

  char buf[1024];
  while (true) {
    int bytes_read = so->read(sock, buf, sizeof(buf));
    if (bytes_read <= 0) return;
  }
}

static void make_bad_connection(uint16_t port) {
  SocketOperations *so = SocketOperations::instance();

  // TCP-level connection phase
  int sock;
  if (!connect_to_host(sock, port)) return;

  // MySQL protocol handshake phase
  // To simplify code, instead of alternating between reading and writing
  // protocol packets, we write a lot of garbage upfront, and then read whatever
  // Router sends back. Router will read what we wrote in chunks, inbetween its
  // writes, thinking they're replies to its handshake packets. Eventually it
  // will finish the handshake with error and disconnect.
  std::vector<char> bogus_data(1024, 0);  // '=' is arbitrary bad value
  if (so->write(sock, bogus_data.data(), bogus_data.size()) == -1)
    throw std::runtime_error("write() failed: " + std::to_string(errno));
  read_until_error(sock);  // error triggered by Router disconnecting
}

/**
 * @test
 * This test Verifies that:
 *   1. Router will block a misbehaving client after consecutive
 *      <max_connect_errors> connection errors
 *   2. Router will reset its connection error counter if client establishes a
 *      successful connection before <max_connect_errors> threshold is hit
 */
TEST_F(RouterRoutingTest, test1) {
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t router_port = port_pool_.get_next_available();

  // doesn't really matter which file we use here, we are not going to do any
  // queries
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  // launch the server mock
  auto &server_mock = launch_mysql_server_mock(json_stmts, server_port, false);

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
  auto &router = launch_router({"-c", conf_file});

  // wait for server and router to begin accepting the connections
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port));

  // we loop just for good measure, to additionally test that this behaviour is
  // repeatable
  for (int i = 0; i < 5; i++) {
    // good connection, followed by 2 bad ones. Good one should reset the error
    // counter
    mysqlrouter::MySQLSession client;
    EXPECT_NO_THROW(
        client.connect("127.0.0.1", router_port, "root", "fake-pass", "", ""));
    make_bad_connection(router_port);
    make_bad_connection(router_port);
  }

  // make a 3rd consecutive bad connection - it should cause Router to start
  // blocking us
  make_bad_connection(router_port);

  // we loop just for good measure, to additionally test that this behaviour is
  // repeatable
  for (int i = 0; i < 5; i++) {
    // now trying to make a good connection should fail due to blockage
    mysqlrouter::MySQLSession client;
    EXPECT_THROW_LIKE(
        client.connect("127.0.0.1", router_port, "root", "fake-pass", "", ""),
        std::exception, "Too many connection errors");
  }
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
