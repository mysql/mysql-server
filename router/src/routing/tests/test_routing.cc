/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <gtest/gtest_prod.h>  // must be the first header

#include "common.h"
#include "mysql/harness/loader.h"
#include "mysql_routing.h"
#include "mysql_routing_common.h"
#include "mysqlrouter/routing.h"
#include "protocol/classic_protocol.h"
#include "routing_mocks.h"
#include "tcp_port_pool.h"
#include "test/helpers.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif

using mysql_harness::TCPAddress;
using routing::AccessMode;
using routing::set_socket_blocking;

using ::testing::ContainerEq;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::InSequence;
using ::testing::Ne;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;

class RoutingTests : public ::testing::Test {
 protected:
  RoutingTests() {}
  virtual void SetUp() {}

  virtual void TearDown() {}

  MockRoutingSockOps routing_sock_ops;
  MockSocketOperations &socket_op = *routing_sock_ops.so();
};

TEST_F(RoutingTests, AccessModes) {
  ASSERT_EQ(static_cast<int>(AccessMode::kReadWrite), 1);
  ASSERT_EQ(static_cast<int>(AccessMode::kReadOnly), 2);
}

TEST_F(RoutingTests, AccessModeLiteralNames) {
  using routing::get_access_mode;
  ASSERT_THAT(get_access_mode("read-write"), Eq(AccessMode::kReadWrite));
  ASSERT_THAT(get_access_mode("read-only"), Eq(AccessMode::kReadOnly));
}

TEST_F(RoutingTests, GetAccessLiteralName) {
  using routing::get_access_mode_name;
  ASSERT_THAT(get_access_mode_name(AccessMode::kReadWrite),
              StrEq("read-write"));
  ASSERT_THAT(get_access_mode_name(AccessMode::kReadOnly), StrEq("read-only"));
}

TEST_F(RoutingTests, Defaults) {
  ASSERT_EQ(routing::kDefaultWaitTimeout, 0);
  ASSERT_EQ(routing::kDefaultMaxConnections, 512);
  ASSERT_EQ(routing::kDefaultDestinationConnectionTimeout,
            std::chrono::seconds(1));
  ASSERT_EQ(routing::kDefaultBindAddress, "127.0.0.1");
  ASSERT_EQ(routing::kDefaultNetBufferLength, 16384U);
  ASSERT_EQ(routing::kDefaultMaxConnectErrors, 100ULL);
  ASSERT_EQ(routing::kDefaultClientConnectTimeout, std::chrono::seconds(9));
}

#ifndef _WIN32
// No way to read nonblocking status in Windows
TEST_F(RoutingTests, SetSocketBlocking) {
  int s = socket(PF_INET, SOCK_STREAM, 6);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_NONBLOCK, 0);
  set_socket_blocking(s, false);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_NONBLOCK, O_NONBLOCK);
  set_socket_blocking(s, true);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_NONBLOCK, 0) << std::endl;

  fcntl(s, F_SETFL, O_RDONLY);
  set_socket_blocking(s, false);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_NONBLOCK, O_NONBLOCK);
  ASSERT_EQ(fcntl(s, F_GETFL, nullptr) & O_RDONLY, O_RDONLY);
}
#endif

TEST_F(RoutingTests, CopyPacketsSingleWrite) {
  int sender_socket = 1, receiver_socket = 2;
  RoutingProtocolBuffer buffer(500);
  int curr_pktnr = 100;
  bool handshake_done = true;
  size_t report_bytes_read = 0u;

  EXPECT_CALL(socket_op, read(sender_socket, &buffer[0], buffer.size()))
      .WillOnce(Return(200));
  EXPECT_CALL(socket_op, write(receiver_socket, &buffer[0], 200))
      .WillOnce(Return(200));

  ClassicProtocol cp(&routing_sock_ops);
  int res = cp.copy_packets(sender_socket, receiver_socket,
                            true /* sender is writable */, buffer, &curr_pktnr,
                            handshake_done, &report_bytes_read, false);

  ASSERT_EQ(0, res);
  ASSERT_EQ(200u, report_bytes_read);
}

TEST_F(RoutingTests, CopyPacketsMultipleWrites) {
  int sender_socket = 1, receiver_socket = 2;
  RoutingProtocolBuffer buffer(500);
  int curr_pktnr = 100;
  bool handshake_done = true;
  size_t report_bytes_read = 0u;

  InSequence seq;

  EXPECT_CALL(socket_op, read(sender_socket, &buffer[0], buffer.size()))
      .WillOnce(Return(200));

  // first write does not write everything
  EXPECT_CALL(socket_op, write(receiver_socket, &buffer[0], 200))
      .WillOnce(Return(100));
  // second does not do anything (which is not treated as an error
  EXPECT_CALL(socket_op, write(receiver_socket, &buffer[100], 100))
      .WillOnce(Return(0));
  // third writes the remaining chunk
  EXPECT_CALL(socket_op, write(receiver_socket, &buffer[100], 100))
      .WillOnce(Return(100));

  ClassicProtocol cp(&routing_sock_ops);
  int res =
      cp.copy_packets(sender_socket, receiver_socket, true, buffer, &curr_pktnr,
                      handshake_done, &report_bytes_read, false);

  ASSERT_EQ(0, res);
  ASSERT_EQ(200u, report_bytes_read);
}

TEST_F(RoutingTests, CopyPacketsWriteError) {
  int sender_socket = 1, receiver_socket = 2;
  RoutingProtocolBuffer buffer(500);
  int curr_pktnr = 100;
  bool handshake_done = true;
  size_t report_bytes_read = 0u;

  EXPECT_CALL(socket_op, read(sender_socket, &buffer[0], buffer.size()))
      .WillOnce(Return(200));
  EXPECT_CALL(socket_op, write(receiver_socket, &buffer[0], 200))
      .WillOnce(Return(-1));

  ClassicProtocol cp(&routing_sock_ops);
  // will log "Write error: ..." as we don't mock an errno
  int res =
      cp.copy_packets(sender_socket, receiver_socket, true, buffer, &curr_pktnr,
                      handshake_done, &report_bytes_read, false);

  ASSERT_EQ(-1, res);
}

#ifndef _WIN32  // [_HERE_]

// a valid Connection::Close xprotocol message
#define kByeMessage "\x01\x00\x00\x00\x03"

class MockServer {
 public:
  MockServer(uint16_t port) {
    int option_value;

    socket_operations_ = mysql_harness::SocketOperations::instance();

    if ((service_tcp_ = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      throw std::runtime_error(mysql_harness::get_strerror(errno));
    }

#ifndef _WIN32
    option_value = 1;
    if (setsockopt(service_tcp_, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char *>(&option_value),
                   static_cast<socklen_t>(sizeof(int))) == -1) {
      throw std::runtime_error(get_message_error(errno));
    }
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (::bind(service_tcp_, (const struct sockaddr *)&addr,
               static_cast<socklen_t>(sizeof(addr))) == -1) {
      socket_operations_->close(service_tcp_);
      int errcode = socket_operations_->get_errno();
      throw std::runtime_error(get_message_error(errcode));
    }
    if (listen(service_tcp_, 20) < 0) {
      throw std::runtime_error(
          "Failed to start listening for connections using TCP");
    }
  }

  ~MockServer() { stop(); }

  void start() {
    stop_ = false;
    thread_ = std::thread(&MockServer::runloop, this);
  }

  void stop() {
    if (!stop_) {
      stop_ = true;
      socket_operations_->shutdown(service_tcp_);
      socket_operations_->close(service_tcp_);
      thread_.join();
    }
  }

  void stop_after_n_accepts(int c) { max_expected_accepts_ = c; }

  void runloop() {
    mysql_harness::rename_thread("runloop()");
    std::vector<std::thread> client_threads;

    while (!stop_ && (max_expected_accepts_ == 0 ||
                      num_accepts_ < max_expected_accepts_)) {
      int sock_client;
      struct sockaddr_in6 client_addr;
      socklen_t sin_size = static_cast<socklen_t>(sizeof client_addr);
      if ((sock_client = accept(service_tcp_, (struct sockaddr *)&client_addr,
                                &sin_size)) < 0) {
        std::cout << mysql_harness::get_strerror(errno) << " ERROR\n";
        continue;
      }
      num_accepts_++;
      client_threads.emplace_back(
          std::thread([this, sock_client]() { new_client(sock_client); }));
    }

    // wait for all threads to shut down again
    for (auto &thr : client_threads) {
      thr.join();
    }
  }

  void new_client(int sock) {
    mysql_harness::rename_thread("new_client()");
    num_connections_++;
    char buf[sizeof(kByeMessage)];
    // block until we receive the bye msg
    if (read(sock, buf, sizeof(buf)) < 0) {
      FAIL() << "Unexpected results from read(): "
             << mysql_harness::get_strerror(errno);
    }
    socket_operations_->close(sock);
    num_connections_--;
  }

 public:
  std::atomic_int num_connections_{0};
  std::atomic_int num_accepts_{0};
  std::atomic_int max_expected_accepts_{0};

 private:
  mysql_harness::SocketOperationsBase *socket_operations_;
  std::thread thread_;
  int service_tcp_;
  std::atomic_bool stop_;
};

static int connect_local(uint16_t port) {
  return routing::RoutingSockOps::instance(
             mysql_harness::SocketOperations::instance())
      ->get_mysql_socket(TCPAddress("127.0.0.1", port),
                         std::chrono::milliseconds(100), true);
}

static void disconnect(int sock) {
  if (write(sock, kByeMessage, sizeof(kByeMessage)) < 0)
    std::cout << "write(xproto-connection-close) returned error\n";

  mysql_harness::SocketOperations::instance()->close(sock);
}

#ifndef _WIN32
static int connect_socket(const char *path) {
  struct sockaddr_un addr;
  int fd;

  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    throw std::runtime_error(mysql_harness::get_strerror(errno));
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

  if (connect(fd, (struct sockaddr *)&addr,
              static_cast<socklen_t>(sizeof(addr))) == -1) {
    throw std::runtime_error(mysql_harness::get_strerror(errno));
  }

  return fd;
}
#endif

static bool call_until(std::function<bool()> f, int timeout = 2) {
  time_t start = time(NULL);
  while (time(NULL) - start < timeout) {
    if (f()) return true;

    // wait a bit and let other threads run
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

// Bug#24841281 NOT ABLE TO CONNECT ANY CLIENTS WHEN ROUTER IS CONFIGURED WITH
// SOCKETS OPTION
TEST_F(RoutingTests, bug_24841281) {
  mysql_harness::rename_thread("TEST_F()");

  TcpPortPool port_pool_;

  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t router_port = port_pool_.get_next_available();

  MockServer server(server_port);
  server.start();

  TmpDir tmp_dir;  // create a tmp dir (it will be destroyed via RAII later)
  std::string sock_path = tmp_dir() + "/sock";

  // check that connecting to a TCP socket or a UNIX socket works
  MySQLRouting routing(
      routing::RoutingStrategy::kNextAvailable, router_port,
      Protocol::Type::kXProtocol, routing::AccessMode::kReadWrite, "0.0.0.0",
      mysql_harness::Path(sock_path), "routing:testroute",
      routing::kDefaultMaxConnections,
      routing::kDefaultDestinationConnectionTimeout,
      routing::kDefaultMaxConnectErrors, routing::kDefaultClientConnectTimeout,
      routing::kDefaultNetBufferLength);
  routing.set_destinations_from_csv("127.0.0.1:" + std::to_string(server_port));
  mysql_harness::PluginFuncEnv env(nullptr, nullptr, true);
  std::thread thd(&MySQLRouting::start, &routing, &env);

  // set the number of accepts that the server should expect for before stopping
#ifdef _WIN32
  server.stop_after_n_accepts(4);
#else
  server.stop_after_n_accepts(6);
#endif

  EXPECT_EQ(routing.get_context().info_active_routes_.load(), 0);

  // open connections to the socket and see if we get a matching outgoing
  // socket connection attempt to our mock server

  int sock1;
  // router is running in a thread, so we need to sync it
  EXPECT_TRUE(call_until([&]() -> bool {
    sock1 = connect_local(router_port);
    return sock1 > 0;
  })) << "timed out connecting to router_port";
  int sock2 = connect_local(router_port);

  EXPECT_THAT(sock1, ::testing::Gt(0));
  EXPECT_THAT(sock2, ::testing::Gt(0));

  EXPECT_TRUE(call_until([&server]() -> bool {
    return server.num_connections_.load() == 2;
  })) << "timed out, got "
      << server.num_connections_.load() << " connections";
  EXPECT_TRUE(call_until([&routing]() -> bool {
    return routing.get_context().info_active_routes_.load() == 2;
  })) << "timed out, got "
      << routing.get_context().info_active_routes_.load() << " active routes";

  disconnect(sock1);

  EXPECT_TRUE(call_until([&routing]() -> bool {
    return routing.get_context().info_active_routes_.load() == 1;
  })) << "timed out, got "
      << routing.get_context().info_active_routes_.load() << " active routes";

  {
    int sock11 = connect_local(router_port);
    int sock12 = connect_local(router_port);

    EXPECT_THAT(sock11, Gt(0));
    EXPECT_THAT(sock12, Gt(0));

    EXPECT_TRUE(call_until([&server]() -> bool {
      return server.num_connections_.load() == 3;
    })) << "timed out: "
        << server.num_connections_.load();

    call_until([&routing]() -> bool {
      return routing.get_context().info_active_routes_.load() == 3;
    });
    EXPECT_EQ(3, routing.get_context().info_active_routes_.load());

    disconnect(sock11);
    call_until([&routing]() -> bool {
      return routing.get_context().info_active_routes_.load() == 2;
    });
    EXPECT_EQ(2, routing.get_context().info_active_routes_.load());

    disconnect(sock12);
    call_until([&routing]() -> bool {
      return routing.get_context().info_active_routes_.load() == 1;
    });
    EXPECT_EQ(1, routing.get_context().info_active_routes_.load());

    call_until(
        [&server]() -> bool { return server.num_connections_.load() == 1; });
    EXPECT_EQ(1, server.num_connections_.load());
  }

  disconnect(sock2);
  call_until([&routing]() -> bool {
    return routing.get_context().info_active_routes_.load() == 0;
  });
  EXPECT_EQ(0, routing.get_context().info_active_routes_.load());

#ifndef _WIN32
  // now try the same with socket ops
  int sock3 = connect_socket(sock_path.c_str());
  int sock4 = connect_socket(sock_path.c_str());

  EXPECT_THAT(sock3, Ne(-1));
  EXPECT_THAT(sock4, Ne(-1));

  call_until(
      [&server]() -> bool { return server.num_connections_.load() == 2; });
  EXPECT_EQ(2, server.num_connections_.load());

  call_until([&routing]() -> bool {
    return routing.get_context().info_active_routes_.load() == 2;
  });
  EXPECT_EQ(2, routing.get_context().info_active_routes_.load());

  disconnect(sock3);
  call_until([&routing]() -> bool {
    return routing.get_context().info_active_routes_.load() == 1;
  });
  EXPECT_EQ(1, routing.get_context().info_active_routes_.load());

  disconnect(sock4);
  call_until([&routing]() -> bool {
    return routing.get_context().info_active_routes_.load() == 0;
  });
  EXPECT_EQ(0, routing.get_context().info_active_routes_.load());
#endif
  env.clear_running();  // shut down MySQLRouting
  server.stop();
  thd.join();
}

TEST_F(RoutingTests, set_destinations_from_uri) {
  MySQLRouting routing(routing::RoutingStrategy::kFirstAvailable, 7001,
                       Protocol::Type::kXProtocol);

  // valid metadata-cache uri
  {
    URI uri("metadata-cache://test/default?role=PRIMARY");
    EXPECT_NO_THROW(routing.set_destinations_from_uri(uri));
  }

  // metadata-cache uri, role missing
  {
    URI uri("metadata-cache://test/default");
    try {
      routing.set_destinations_from_uri(uri);
      FAIL() << "Expected std::runtime_error exception";
    } catch (const std::runtime_error &err) {
      EXPECT_EQ(
          err.what(),
          std::string("Missing 'role' in routing destination specification"));
    } catch (...) {
      FAIL() << "Expected std::runtime_error exception";
    }
  }

  // invalid scheme
  {
    URI uri("invalid-scheme://test/default?role=SECONDARY");
    try {
      routing.set_destinations_from_uri(uri);
      FAIL() << "Expected std::runtime_error exception";
    } catch (const std::runtime_error &err) {
      EXPECT_EQ(err.what(),
                std::string("Invalid URI scheme; expecting: 'metadata-cache' "
                            "is: 'invalid-scheme'"));
    } catch (...) {
      FAIL() << "Expected std::runtime_error exception";
    }
  }
}

TEST_F(RoutingTests, set_destinations_from_cvs) {
  MySQLRouting routing(routing::RoutingStrategy::kNextAvailable, 7001,
                       Protocol::Type::kXProtocol);

  // valid address list
  {
    const std::string cvs = "127.0.0.1:2002,127.0.0.1:2004";
    EXPECT_NO_THROW(routing.set_destinations_from_csv(cvs));
  }

  // no routing strategy, should go with default
  {
    MySQLRouting routing_inv(routing::RoutingStrategy::kUndefined, 7001,
                             Protocol::Type::kXProtocol);
    const std::string csv = "127.0.0.1:2002,127.0.0.1:2004";
    EXPECT_NO_THROW(routing_inv.set_destinations_from_csv(csv));
  }

  // no address
  {
    const std::string csv = "";
    EXPECT_THROW(routing.set_destinations_from_csv(csv), std::runtime_error);
  }

  // invalid address
  {
    const std::string csv = "127.0.0.1.2:2222";
    EXPECT_THROW(routing.set_destinations_from_csv(csv), std::runtime_error);
  }

  // let's check if the correct defualt port gets chosen for
  // the respective protocol
  // we use the trick here setting the expected address also as
  // the binding address for the routing which should make the method throw
  // an exception if these are the same
  {
    const std::string address = "127.0.0.1";
    MySQLRouting routing_classic(routing::RoutingStrategy::kNextAvailable, 3306,
                                 Protocol::Type::kClassicProtocol,
                                 routing::AccessMode::kReadWrite, address);
    EXPECT_THROW(routing_classic.set_destinations_from_csv("127.0.0.1"),
                 std::runtime_error);
    EXPECT_THROW(routing_classic.set_destinations_from_csv("127.0.0.1:3306"),
                 std::runtime_error);
    EXPECT_NO_THROW(
        routing_classic.set_destinations_from_csv("127.0.0.1:33060"));

    MySQLRouting routing_x(routing::RoutingStrategy::kNextAvailable, 33060,
                           Protocol::Type::kXProtocol,
                           routing::AccessMode::kReadWrite, address);
    EXPECT_THROW(routing_x.set_destinations_from_csv("127.0.0.1"),
                 std::runtime_error);
    EXPECT_THROW(routing_x.set_destinations_from_csv("127.0.0.1:33060"),
                 std::runtime_error);
    EXPECT_NO_THROW(routing_x.set_destinations_from_csv("127.0.0.1:3306"));
  }
}

#endif  // #ifndef _WIN32 [_HERE_]

TEST_F(RoutingTests, get_routing_thread_name) {
  // config name must begin with "routing" (name of the plugin passed from
  // configuration file)
  EXPECT_STREQ(":parse err", get_routing_thread_name("", "").c_str());
  EXPECT_STREQ(":parse err", get_routing_thread_name("routin", "").c_str());
  EXPECT_STREQ(":parse err", get_routing_thread_name(" routing", "").c_str());
  EXPECT_STREQ("pre:parse err", get_routing_thread_name("", "pre").c_str());
  EXPECT_STREQ("pre:parse err",
               get_routing_thread_name("routin", "pre").c_str());
  EXPECT_STREQ("pre:parse err",
               get_routing_thread_name(" routing", "pre").c_str());

  // normally prefix would never be empty, so the behavior below is not be very
  // meaningful; it should not crash however
  EXPECT_STREQ(":", get_routing_thread_name("routing", "").c_str());
  EXPECT_STREQ(":", get_routing_thread_name("routing:", "").c_str());

  // realistic (but unanticipated) cases - removing everything up to _default_
  // will fail, in which case we fall back of <prefix>:<everything after
  // "routing:">, trimmed to 15 chars
  EXPECT_STREQ(
      "RtS:test_def_ul",
      get_routing_thread_name("routing:test_def_ult_x_ro", "RtS").c_str());
  EXPECT_STREQ(
      "RtS:test_def_ul",
      get_routing_thread_name("routing:test_def_ult_ro", "RtS").c_str());
  EXPECT_STREQ("RtS:", get_routing_thread_name("routing", "RtS").c_str());
  EXPECT_STREQ("RtS:test_x_ro",
               get_routing_thread_name("routing:test_x_ro", "RtS").c_str());
  EXPECT_STREQ("RtS:test_ro",
               get_routing_thread_name("routing:test_ro", "RtS").c_str());

  // real cases
  EXPECT_STREQ(
      "RtS:x_ro",
      get_routing_thread_name("routing:test_default_x_ro", "RtS").c_str());
  EXPECT_STREQ(
      "RtS:ro",
      get_routing_thread_name("routing:test_default_ro", "RtS").c_str());
  EXPECT_STREQ("RtS:", get_routing_thread_name("routing", "RtS").c_str());
}

/*
 * @test This test verifies fix for Bug 23857183 and checks if trying to connect
 * to wrong port fails immediately not via timeout
 *
 * @todo (jan) disabled the test as the result is unpredictable as port may be
 * in use, IP may be or not be bound, ... The test needs to be rewritten and
 * have predictable output, or be removed.
 */
TEST_F(RoutingTests, DISABLED_ConnectToServerWrongPort) {
  const std::chrono::seconds TIMEOUT{4};

  // wrong port number
  {
    TCPAddress address("127.0.0.1", 10888);
    int server = routing::RoutingSockOps::instance(
                     mysql_harness::SocketOperations::instance())
                     ->get_mysql_socket(address, TIMEOUT);
    // should return -1, -2 is timeout expired which is not what we expect when
    // connecting with the wrong port
    ASSERT_EQ(server, -1);
  }

// in darwin and solaris, attempting connection to 127.0.0.11 will fail by
// timeout
#if !defined(__APPLE__) && !defined(__sun)
  // wrong port number and IP
  {
    TCPAddress address("127.0.0.11", 10888);
    int server = routing::RoutingSockOps::instance(
                     mysql_harness::SocketOperations::instance())
                     ->get_mysql_socket(address, TIMEOUT);
    // should return -1, -2 is timeout expired which is not what we expect when
    // connecting with the wrong port
    ASSERT_EQ(server, -1);
  }
#endif
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
