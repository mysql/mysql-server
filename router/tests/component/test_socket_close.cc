/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
#include <stdexcept>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "config_builder.h"
#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"  // make_unexpected
#include "mysql/harness/stdx/monitor.h"
#include "mysqlrouter/classic_protocol.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_config.h"
#include "router_test_helpers.h"
#include "tcp_port_pool.h"

using mysqlrouter::ClusterType;
using namespace std::chrono_literals;
using namespace std::string_literals;

enum class AcceptorType { TcpSocket = 1 << 0, UnixSocket = 1 << 1 };
using Acceptors = std::underlying_type_t<AcceptorType>;

class SocketCloseTest : public RouterComponentTest {
 protected:
  auto &launch_router(const std::string &metadata_cache_section,
                      const std::string &routing_section,
                      const int expected_exitcode,
                      std::chrono::milliseconds wait_for_notify_ready = 30s) {
    auto default_section = get_DEFAULT_defaults();
    init_keyring(default_section, get_test_temp_dir_name(), router_user,
                 router_password);
    const auto state_file =
        create_state_file(get_test_temp_dir_name(),
                          create_state_file_content("uuid", "", node_ports, 0));
    default_section["dynamic_state"] = state_file;

    // launch the router
    const std::string conf_file = create_config_file(
        get_test_temp_dir_name(), metadata_cache_section + routing_section,
        &default_section);

    auto &router =
        ProcessManager::launch_router({"-c", conf_file}, expected_exitcode,
                                      true, false, wait_for_notify_ready);

    return router;
  }

  void setup_cluster(const size_t nodes_count, const std::string &tracefile,
                     const bool no_primary = false) {
    assert(nodes_count > 0);

    for (size_t i = 0; i < nodes_count; ++i) {
      // if we are "relaunching" the cluster we want to use the same port as
      // before as router has them in the configuration
      if (node_ports.size() < nodes_count) {
        node_ports.push_back(port_pool_.get_next_available());
        node_http_ports.push_back(port_pool_.get_next_available());
      }

      cluster_nodes.push_back(
          &mock_server_spawner().spawn(mock_server_cmdline(tracefile)
                                           .port(node_ports[i])
                                           .http_port(node_http_ports[i])
                                           .args()));
    }

    for (size_t i = 0; i < nodes_count; ++i) {
      ASSERT_NO_FATAL_FAILURE(
          check_port_ready(*cluster_nodes[i], node_ports[i]));
      ASSERT_TRUE(MockServerRestClient(node_http_ports[i])
                      .wait_for_rest_endpoint_ready());

      auto gr_nodes = classic_ports_to_gr_nodes(node_ports);
      auto cluster_nodes = classic_ports_to_cluster_nodes(node_ports);
      if (no_primary && node_ports.size() > 0) {
        gr_nodes[0].member_role = "SECONDARY";
        cluster_nodes[0].role = "SECONDARY";
      }
      set_mock_metadata(node_http_ports[i], "uuid", gr_nodes, i, cluster_nodes,
                        0, false, "localhost");
    }
  }

  std::string get_metadata_cache_section(
      ClusterType cluster_type = ClusterType::GR_V2) {
    const std::string cluster_type_str =
        (cluster_type == ClusterType::RS_V2) ? "rs" : "gr";

    return "[metadata_cache:test]\n"
           "cluster_type=" +
           cluster_type_str +
           "\n"
           "router_id=1\n"
           "user=" +
           router_user +
           "\n"
           "connect_timeout=1\n"
           "metadata_cluster=test\n" +
           "ttl=" + std::to_string(std::chrono::duration<double>(ttl).count()) +
           "\n\n";
  }

  std::string get_metadata_cache_routing_section(
      const std::optional<uint16_t> &router_port,
      const std::optional<std::string> &router_socket, const std::string &role,
      const std::string &strategy, const std::string &section_name = "default",
      const std::string &protocol = "classic") const {
    std::vector<std::pair<std::string, std::string>> options{
        {"destinations", "metadata-cache://test/default?role=" + role},
        {"protocol", protocol}};

    if (router_port)
      options.emplace_back("bind_port", std::to_string(*router_port));
    if (router_socket) options.emplace_back("socket", *router_socket);

    if (!strategy.empty()) options.emplace_back("routing_strategy", strategy);

    return mysql_harness::ConfigBuilder::build_section(
        "routing:" + section_name, options);
  }

  std::string get_static_routing_section(
      const std::optional<uint16_t> &router_port,
      const std::optional<std::string> &router_socket,
      const std::vector<uint16_t> &destinations,
      const std::string &strategy) const {
    std::string destinations_str;
    for (size_t i = 0; i < destinations.size(); ++i) {
      destinations_str += "localhost:" + std::to_string(destinations[i]);
      if (i != destinations.size() - 1) {
        destinations_str += ",";
      }
    }

    std::vector<std::pair<std::string, std::string>> options{
        {"protocol", "classic"},
        {"routing_strategy", strategy},
        {"destinations", destinations_str}};

    if (router_port)
      options.emplace_back("bind_port", std::to_string(*router_port));
    if (router_socket) options.emplace_back("socket", *router_socket);

    return mysql_harness::ConfigBuilder::build_section("routing:test_default",
                                                       options);
  }

  void setup_router(ClusterType cluster_type, const Acceptors acceptors,
                    const bool read_only = false) {
    const std::string metadata_cache_section =
        get_metadata_cache_section(cluster_type);

    const bool use_tcp_port_acceptors =
        acceptors & static_cast<Acceptors>(AcceptorType::TcpSocket);
    const bool use_unix_socket_acceptors =
        acceptors & static_cast<Acceptors>(AcceptorType::UnixSocket);

    init_acceptors(acceptors);
    std::string routing_rw_section{""};
    if (!read_only) {
      routing_rw_section = get_metadata_cache_routing_section(
          router_rw_port, router_rw_socket, "PRIMARY", "round-robin", "rw");
      routing_rw_section += get_metadata_cache_routing_section(
          router_rw_x_port, router_rw_x_socket, "PRIMARY", "round-robin",
          "x_rw", "x");
    }
    std::string routing_ro_section = get_metadata_cache_routing_section(
        router_ro_port, router_ro_socket, "SECONDARY", "round-robin", "ro");
    routing_ro_section += get_metadata_cache_routing_section(
        router_ro_x_port, router_ro_x_socket, "SECONDARY", "round-robin",
        "x_ro", "x");

    router =
        &launch_router(metadata_cache_section,
                       routing_rw_section + routing_ro_section, EXIT_SUCCESS,
                       /*wait_for_notify_ready=*/30s);

    if (use_tcp_port_acceptors) {
      EXPECT_TRUE(
          wait_for_port_ready(read_only ? *router_ro_port : *router_rw_port));
    }

    if (use_unix_socket_acceptors) {
      EXPECT_TRUE(
          wait_file_exists(read_only ? *router_ro_socket : *router_rw_socket));
    }
  }

  void toggle_auth_failure(const bool toggle, const uint16_t http_port,
                           const std::vector<uint16_t> &nodes) {
    auto globals =
        mock_GR_metadata_as_json("uuid", classic_ports_to_gr_nodes(nodes), 0,
                                 classic_ports_to_cluster_nodes(nodes));
    JsonAllocator allocator;

    std::string auth_user = toggle ? custom_user : router_user;
    std::string auth_password = toggle ? custom_password : router_password;

    globals.AddMember("user",
                      JsonValue(auth_user.c_str(), auth_user.size(), allocator),
                      allocator);
    globals.AddMember(
        "password",
        JsonValue(auth_password.c_str(), auth_password.size(), allocator),
        allocator);
    const auto globals_str = json_to_string(globals);
    MockServerRestClient(http_port).set_globals(globals_str);
  }

  void toggle_auth_failure_on(const uint16_t http_port,
                              const std::vector<uint16_t> &nodes) {
    toggle_auth_failure(true, http_port, nodes);
  }

  void toggle_auth_failure_off(const uint16_t http_port,
                               const std::vector<uint16_t> &nodes) {
    toggle_auth_failure(false, http_port, nodes);
  }

  void try_connection(const std::string &host, const uint16_t port,
                      const std::string &user, const std::string &password) {
    MySQLSession client;
    client.connect(host, port, user, password, "", "");
    client.query_one("select @@port");
  }

  void try_connection(const std::string &socket, const std::string &user,
                      const std::string &password) {
    MySQLSession client;
    client.connect("", 0, user, password, socket, "");
    client.query_one("select @@port");
  }

  void simulate_cluster_node_down(const std::vector<uint16_t> &node_ports,
                                  const uint16_t http_port) {
    auto globals =
        mock_GR_metadata_as_json("uuid", classic_ports_to_gr_nodes(node_ports),
                                 0, classic_ports_to_cluster_nodes(node_ports));
    JsonAllocator allocator;
    globals.AddMember("transaction_count", 0, allocator);
    // Empty, node is not taken into account
    globals.AddMember("cluster_type", "", allocator);
    auto globals_str = json_to_string(globals);
    MockServerRestClient(http_port).set_globals(globals_str);
    EXPECT_TRUE(wait_for_transaction_count_increase(http_port, 2));
  }

  void simulate_cluster_node_up(const ClusterType cluster_type,
                                const std::vector<uint16_t> &node_ports,
                                const uint16_t http_port,
                                const bool no_primary = false) {
    auto gr_nodes = classic_ports_to_gr_nodes(node_ports);
    auto cluster_nodes = classic_ports_to_cluster_nodes(node_ports);
    if (no_primary && node_ports.size() > 0) {
      gr_nodes[0].member_role = "SECONDARY";
      cluster_nodes[0].role = "SECONDARY";
    }

    auto globals = mock_GR_metadata_as_json("uuid", gr_nodes, 0, cluster_nodes);
    JsonAllocator allocator;
    globals.AddMember("transaction_count", 0, allocator);
    globals.AddMember("cluster_type",
                      (cluster_type == ClusterType::RS_V2) ? "ar" : "gr",
                      allocator);
    auto globals_str = json_to_string(globals);
    MockServerRestClient(http_port).set_globals(globals_str);
    EXPECT_TRUE(wait_for_transaction_count_increase(http_port, 2));
  }

  void init_acceptors(const Acceptors acceptors) {
    const bool use_tcp_port_acceptors =
        acceptors & static_cast<Acceptors>(AcceptorType::TcpSocket);
    const bool use_unix_socket_acceptors =
        acceptors & static_cast<Acceptors>(AcceptorType::UnixSocket);

    if (use_tcp_port_acceptors) {
      if (!router_rw_port) router_rw_port = port_pool_.get_next_available();
      if (!router_ro_port) router_ro_port = port_pool_.get_next_available();
      if (!router_rw_x_port) router_rw_x_port = port_pool_.get_next_available();
      if (!router_ro_x_port) router_ro_x_port = port_pool_.get_next_available();
    } else {
      router_rw_port = router_ro_port = router_rw_x_port = router_ro_x_port =
          std::nullopt;
    }

    if (use_unix_socket_acceptors) {
      const std::string tmp_dir = get_test_temp_dir_name();
      if (!router_rw_socket) router_rw_socket = tmp_dir + "/mysql.socket";
      if (!router_ro_socket) router_ro_socket = tmp_dir + "/mysqlro.socket";
      if (!router_rw_x_socket) router_rw_x_socket = tmp_dir + "/mysqlx.socket";
      if (!router_ro_x_socket)
        router_ro_x_socket = tmp_dir + "/mysqlxro.socket";
    } else {
      router_rw_socket = router_ro_socket = router_rw_x_socket =
          router_ro_x_socket = std::nullopt;
    }
  }

  std::chrono::milliseconds ttl{100ms};
  std::vector<uint16_t> node_ports, node_http_ports;
  std::vector<ProcessWrapper *> cluster_nodes;
  ProcessWrapper *router;
  std::optional<uint16_t> router_rw_port;
  std::optional<uint16_t> router_ro_port;
  std::optional<uint16_t> router_rw_x_port;
  std::optional<uint16_t> router_ro_x_port;
  std::optional<std::string> router_rw_socket;
  std::optional<std::string> router_ro_socket;
  std::optional<std::string> router_rw_x_socket;
  std::optional<std::string> router_ro_x_socket;
  const std::string router_user{"mysql_test_user"};
  const std::string router_password{"mysql_test_password"};
  const std::string custom_user{"username"};
  const std::string custom_password{"password"};
};

struct SocketsCloseTestParams {
  // mock_server trace file
  std::string tracefile;
  // additional info about the testcase that gets printed by the gtest in the
  // results
  std::string description;
  // the type of the cluster GR or AR
  ClusterType cluster_type;
  // the type of acceptors the Router should be configured to use
  Acceptors acceptors;

  SocketsCloseTestParams(std::string tracefile_, std::string description_,
                         ClusterType cluster_type_, Acceptors acceptors_)
      : tracefile(std::move(tracefile_)),
        description(std::move(description_)),
        cluster_type(cluster_type_),
        acceptors(acceptors_) {}
};

auto get_test_description(
    const ::testing::TestParamInfo<SocketsCloseTestParams> &info) {
  return info.param.description;
}

class SocketCloseOnMetadataAuthFail
    : public SocketCloseTest,
      public ::testing::WithParamInterface<SocketsCloseTestParams> {};

TEST_P(SocketCloseOnMetadataAuthFail, SocketCloseOnMetadataAuthFailTest) {
  auto check_ports_available = [this]() {
    for (const auto port :
         {router_rw_port, router_ro_port, router_rw_x_port, router_ro_x_port}) {
      EXPECT_TRUE(wait_for_port_unused(*port));
    }
  };
  auto check_ports_not_available = [this]() {
    for (const auto port :
         {router_rw_port, router_ro_port, router_rw_x_port, router_ro_x_port}) {
      EXPECT_TRUE(wait_for_port_used(*port));
    }
  };

  auto check_sockets_available = [this]() {
    for (const auto &socket : {router_rw_socket, router_ro_socket,
                               router_rw_x_socket, router_ro_x_socket}) {
      EXPECT_FALSE(wait_file_exists(*socket, false));
    }
  };
  auto check_sockets_not_available = [this]() {
    for (const auto &socket : {router_rw_socket, router_ro_socket,
                               router_rw_x_socket, router_ro_x_socket}) {
      EXPECT_TRUE(wait_file_exists(*socket, true));
    }
  };

  const bool use_tcp_port_acceptors =
      GetParam().acceptors & static_cast<Acceptors>(AcceptorType::TcpSocket);
  const bool use_unix_socket_acceptors =
      GetParam().acceptors & static_cast<Acceptors>(AcceptorType::UnixSocket);

#ifdef _WIN32
  if (use_unix_socket_acceptors) {
    GTEST_SKIP() << "No support for Unix sockets on Windows";
  }
#endif

  SCOPED_TRACE("// launch cluster with 3 nodes, 1 RW/2 RO");
  ASSERT_NO_FATAL_FAILURE(setup_cluster(3, GetParam().tracefile));

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  ASSERT_NO_FATAL_FAILURE(
      setup_router(GetParam().cluster_type, GetParam().acceptors));

  SCOPED_TRACE("// check if both RO and RW ports are used");
  if (use_tcp_port_acceptors) {
    check_ports_not_available();
  }
  if (use_unix_socket_acceptors) {
    check_sockets_not_available();
  }

  SCOPED_TRACE("// RO and RW queries should pass");
  if (use_tcp_port_acceptors) {
    ASSERT_NO_THROW(try_connection("127.0.0.1", *router_rw_port, router_user,
                                   router_password));
    ASSERT_NO_THROW(try_connection("127.0.0.1", *router_ro_port, router_user,
                                   router_password));
  }
  if (use_unix_socket_acceptors) {
    ASSERT_NO_THROW(
        try_connection(*router_rw_socket, router_user, router_password));
    ASSERT_NO_THROW(
        try_connection(*router_ro_socket, router_user, router_password));
  }

  SCOPED_TRACE("// Toggle authentication failure on a primary node");
  toggle_auth_failure_on(node_http_ports[0], node_ports);
  if (use_tcp_port_acceptors) {
    check_ports_not_available();
  }
  if (use_unix_socket_acceptors) {
    check_sockets_not_available();
  }

  SCOPED_TRACE("// Toggle authentication failure on a first secondary node");
  toggle_auth_failure_on(node_http_ports[1], node_ports);
  if (use_tcp_port_acceptors) {
    check_ports_not_available();
  }
  if (use_unix_socket_acceptors) {
    check_sockets_not_available();
  }

  SCOPED_TRACE("// Toggle authentication failure on a second secondary node");
  toggle_auth_failure_on(node_http_ports[2], node_ports);
  if (use_tcp_port_acceptors) {
    check_ports_available();
  }
  if (use_unix_socket_acceptors) {
    check_sockets_available();
  }

  SCOPED_TRACE("// RO and RW queries connections should fail");
  if (use_tcp_port_acceptors) {
    EXPECT_THROW(try_connection("127.0.0.1", *router_rw_port, custom_user,
                                custom_password),
                 std::runtime_error);
    EXPECT_THROW(try_connection("127.0.0.1", *router_ro_port, custom_user,
                                custom_password),
                 std::runtime_error);
  }

  if (use_unix_socket_acceptors) {
    EXPECT_THROW(
        try_connection(*router_rw_socket, router_user, router_password),
        std::runtime_error);
    EXPECT_THROW(
        try_connection(*router_ro_socket, router_user, router_password),
        std::runtime_error);
  }

  SCOPED_TRACE("// Allow successful authentication on a second secondary node");
  toggle_auth_failure_off(node_http_ports[2], node_ports);
  if (use_tcp_port_acceptors) {
    check_ports_not_available();
  }
  if (use_unix_socket_acceptors) {
    check_sockets_not_available();
  }

  SCOPED_TRACE("// Toggle authentication failure on a second secondary node");
  toggle_auth_failure_on(node_http_ports[2], node_ports);
  if (use_tcp_port_acceptors) {
    check_ports_available();
  }
  if (use_unix_socket_acceptors) {
    check_sockets_available();
  }

  SCOPED_TRACE("// Allow successful authentication on a primary node");
  toggle_auth_failure_off(node_http_ports[0], node_ports);
  if (use_tcp_port_acceptors) {
    check_ports_not_available();
  }
  if (use_unix_socket_acceptors) {
    check_sockets_not_available();
  }

  SCOPED_TRACE("// Allow successful authentication on secondary nodes");
  toggle_auth_failure_off(node_http_ports[1], node_ports);
  toggle_auth_failure_off(node_http_ports[2], node_ports);
  wait_for_transaction_count_increase(node_http_ports[0], 2);

  if (use_tcp_port_acceptors) {
    check_ports_not_available();
  }
  if (use_unix_socket_acceptors) {
    check_sockets_not_available();
  }

  SCOPED_TRACE("// RO and RW connections should work ok");
  if (use_tcp_port_acceptors) {
    ASSERT_NO_THROW(try_connection("127.0.0.1", *router_rw_port, router_user,
                                   router_password));
    ASSERT_NO_THROW(try_connection("127.0.0.1", *router_ro_port, router_user,
                                   router_password));
  }
  if (use_unix_socket_acceptors) {
    ASSERT_NO_THROW(
        try_connection(*router_rw_socket, router_user, router_password));
    ASSERT_NO_THROW(
        try_connection(*router_ro_socket, router_user, router_password));
  }
}

INSTANTIATE_TEST_SUITE_P(
    SocketCloseOnMetadataAuthFailTest, SocketCloseOnMetadataAuthFail,
    ::testing::Values(
        SocketsCloseTestParams("metadata_dynamic_nodes_v2_gr.js",
                               "close_port_on_metadata_auth_fail_gr_v2",
                               ClusterType::GR_V2,
                               Acceptors(AcceptorType::TcpSocket)),
        SocketsCloseTestParams("metadata_dynamic_nodes_v2_ar.js",
                               "close_port_on_metadata_auth_fail_ar_v2",
                               ClusterType::RS_V2,
                               Acceptors(AcceptorType::TcpSocket)),
        SocketsCloseTestParams("metadata_dynamic_nodes_v2_gr.js",
                               "close_socket_on_metadata_auth_fail_gr_v2",
                               ClusterType::GR_V2,
                               Acceptors(AcceptorType::UnixSocket)),
        SocketsCloseTestParams(
            "metadata_dynamic_nodes_v2_gr.js",
            "close_port_and_socket_on_metadata_auth_fail_gr_v2",
            ClusterType::GR_V2,
            Acceptors(AcceptorType::TcpSocket) |
                Acceptors(AcceptorType::UnixSocket))),
    get_test_description);

class SocketCloseOnMetadataUnavailable
    : public SocketCloseTest,
      public ::testing::WithParamInterface<SocketsCloseTestParams> {};

// WL#13327: TS_R1_1, TS_R3_4
TEST_P(SocketCloseOnMetadataUnavailable, 1RW2RO) {
  const bool use_tcp_port_acceptors =
      GetParam().acceptors & static_cast<Acceptors>(AcceptorType::TcpSocket);
  const bool use_unix_socket_acceptors =
      GetParam().acceptors & static_cast<Acceptors>(AcceptorType::UnixSocket);

#ifdef _WIN32
  if (use_unix_socket_acceptors) {
    GTEST_SKIP() << "No support for Unix sockets on Windows";
  }
#endif

  SCOPED_TRACE("// launch cluster with 3 nodes, 1 RW/2 RO");
  ASSERT_NO_FATAL_FAILURE(setup_cluster(3, GetParam().tracefile));

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  ASSERT_NO_FATAL_FAILURE(
      setup_router(GetParam().cluster_type, GetParam().acceptors));
  SCOPED_TRACE("// check if both RO and RW ports are used");
  if (use_tcp_port_acceptors) {
    for (const auto &port :
         {router_rw_port, router_ro_port, router_rw_x_port, router_ro_x_port}) {
      EXPECT_TRUE(wait_for_port_used(*port));
    }
  }
  if (use_unix_socket_acceptors) {
    for (const auto &socket : {router_rw_socket, router_ro_socket,
                               router_rw_x_socket, router_ro_x_socket}) {
      EXPECT_TRUE(wait_file_exists(*socket));
    }
  }

  SCOPED_TRACE("// Primary node down");
  simulate_cluster_node_down(node_ports, node_http_ports[0]);
  if (use_tcp_port_acceptors) {
    for (const auto &port :
         {router_rw_port, router_ro_port, router_rw_x_port, router_ro_x_port}) {
      EXPECT_FALSE(is_port_bindable(*port));
    }
  }
  if (use_unix_socket_acceptors) {
    for (const auto &socket : {router_rw_socket, router_ro_socket,
                               router_rw_x_socket, router_ro_x_socket}) {
      EXPECT_FALSE(is_socket_bindable(*socket));
    }
  }

  SCOPED_TRACE("// First secondary node down");
  simulate_cluster_node_down(node_ports, node_http_ports[1]);
  if (use_tcp_port_acceptors) {
    for (const auto &port :
         {router_rw_port, router_ro_port, router_rw_x_port, router_ro_x_port}) {
      EXPECT_FALSE(is_port_bindable(*port));
    }
  }
  if (use_unix_socket_acceptors) {
    for (const auto &socket : {router_rw_socket, router_ro_socket,
                               router_rw_x_socket, router_ro_x_socket}) {
      EXPECT_FALSE(is_socket_bindable(*socket));
    }
  }

  SCOPED_TRACE("// Second secondary node down");
  simulate_cluster_node_down(node_ports, node_http_ports[2]);
  if (use_tcp_port_acceptors) {
    for (const auto &port :
         {router_rw_port, router_ro_port, router_rw_x_port, router_ro_x_port}) {
      EXPECT_TRUE(wait_for_port_unused(*port));
    }
  }
  if (use_unix_socket_acceptors) {
    for (const auto &socket : {router_rw_socket, router_ro_socket,
                               router_rw_x_socket, router_ro_x_socket}) {
      EXPECT_FALSE(wait_file_exists(*socket, false));
    }
  }

  SCOPED_TRACE("// RW and RO queries fail");
  if (use_tcp_port_acceptors) {
    ASSERT_ANY_THROW(
        try_connection("127.0.0.1", *router_ro_port, "username", "password"));
    ASSERT_ANY_THROW(
        try_connection("127.0.0.1", *router_rw_port, "username", "password"));
  }
  if (use_unix_socket_acceptors) {
    ASSERT_ANY_THROW(try_connection(*router_ro_socket, "username", "password"));
    ASSERT_ANY_THROW(try_connection(*router_rw_socket, "username", "password"));
  }

  SCOPED_TRACE("// Second secondary node up");
  simulate_cluster_node_up(GetParam().cluster_type, node_ports,
                           node_http_ports[2]);
  if (use_tcp_port_acceptors) {
    for (const auto &port :
         {router_rw_port, router_ro_port, router_rw_x_port, router_ro_x_port}) {
      EXPECT_TRUE(wait_for_port_used(*port));
    }
  }
  if (use_unix_socket_acceptors) {
    for (const auto &socket : {router_rw_socket, router_ro_socket,
                               router_rw_x_socket, router_ro_x_socket}) {
      EXPECT_TRUE(wait_file_exists(*socket, true));
    }
  }

  SCOPED_TRACE("// Second secondary node down");
  simulate_cluster_node_down(node_ports, node_http_ports[2]);
  if (use_tcp_port_acceptors) {
    for (const auto &port :
         {router_rw_port, router_ro_port, router_rw_x_port, router_ro_x_port}) {
      EXPECT_TRUE(wait_for_port_unused(*port));
    }
  }
  if (use_unix_socket_acceptors) {
    for (const auto &socket : {router_rw_socket, router_ro_socket,
                               router_rw_x_socket, router_ro_x_socket}) {
      EXPECT_FALSE(wait_file_exists(*socket, false));
    }
  }

  SCOPED_TRACE("// Primary node up");
  simulate_cluster_node_up(GetParam().cluster_type, node_ports,
                           node_http_ports[0]);
  if (use_tcp_port_acceptors) {
    for (const auto &port :
         {router_rw_port, router_ro_port, router_rw_x_port, router_ro_x_port}) {
      EXPECT_TRUE(wait_for_port_used(*port));
    }
  }
  if (use_unix_socket_acceptors) {
    for (const auto &socket : {router_rw_socket, router_ro_socket,
                               router_rw_x_socket, router_ro_x_socket}) {
      EXPECT_TRUE(wait_file_exists(*socket, true));
    }
  }

  SCOPED_TRACE("RW and RO queries are working fine");
  if (use_tcp_port_acceptors) {
    ASSERT_NO_FATAL_FAILURE(
        try_connection("127.0.0.1", *router_ro_port, "username", "password"));
    ASSERT_NO_FATAL_FAILURE(
        try_connection("127.0.0.1", *router_rw_port, "username", "password"));
  }
  if (use_unix_socket_acceptors) {
    ASSERT_NO_FATAL_FAILURE(
        try_connection(*router_ro_socket, "username", "password"));
    ASSERT_NO_FATAL_FAILURE(
        try_connection(*router_rw_socket, "username", "password"));
  }
}

// WL#13327: TS_R1_4
TEST_P(SocketCloseOnMetadataUnavailable, 1RW) {
  const bool use_tcp_port_acceptors =
      GetParam().acceptors & static_cast<Acceptors>(AcceptorType::TcpSocket);
  const bool use_unix_socket_acceptors =
      GetParam().acceptors & static_cast<Acceptors>(AcceptorType::UnixSocket);

#ifdef _WIN32
  if (use_unix_socket_acceptors) {
    GTEST_SKIP() << "No support for Unix sockets on Windows";
  }
#endif

  SCOPED_TRACE("// launch cluster with only RW node");
  ASSERT_NO_FATAL_FAILURE(setup_cluster(1, GetParam().tracefile));

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  ASSERT_NO_FATAL_FAILURE(
      setup_router(GetParam().cluster_type, GetParam().acceptors));

  SCOPED_TRACE("// check if RW port is used");
  if (use_tcp_port_acceptors) {
    EXPECT_TRUE(wait_for_port_used(*router_rw_port));
    EXPECT_TRUE(wait_for_port_used(*router_rw_x_port));
    EXPECT_TRUE(wait_for_port_unused(*router_ro_port));
    EXPECT_TRUE(wait_for_port_unused(*router_ro_x_port));
  }
  if (use_unix_socket_acceptors) {
    EXPECT_TRUE(wait_file_exists(*router_rw_socket));
    EXPECT_TRUE(wait_file_exists(*router_rw_x_socket));
    EXPECT_FALSE(wait_file_exists(*router_ro_socket, false));
    EXPECT_FALSE(wait_file_exists(*router_ro_x_socket, false));
  }

  SCOPED_TRACE("// Primary node down");
  simulate_cluster_node_down(node_ports, node_http_ports[0]);
  if (use_tcp_port_acceptors) {
    EXPECT_TRUE(wait_for_port_unused(*router_rw_port));
    EXPECT_TRUE(is_port_bindable(*router_ro_port));
    EXPECT_TRUE(wait_for_port_unused(*router_rw_x_port));
    EXPECT_TRUE(is_port_bindable(*router_ro_x_port));
  }
  if (use_unix_socket_acceptors) {
    EXPECT_FALSE(wait_file_exists(*router_rw_socket, false));
    EXPECT_TRUE(is_socket_bindable(*router_ro_socket));
    EXPECT_FALSE(wait_file_exists(*router_rw_x_socket, false));
    EXPECT_TRUE(is_socket_bindable(*router_ro_x_socket));
  }

  SCOPED_TRACE("// RW and RO queries fail");
  if (use_tcp_port_acceptors) {
    ASSERT_ANY_THROW(
        try_connection("127.0.0.1", *router_ro_port, "username", "password"));
    ASSERT_ANY_THROW(
        try_connection("127.0.0.1", *router_rw_port, "username", "password"));
  }
  if (use_unix_socket_acceptors) {
    ASSERT_ANY_THROW(try_connection(*router_ro_socket, "username", "password"));
    ASSERT_ANY_THROW(try_connection(*router_rw_socket, "username", "password"));
  }

  SCOPED_TRACE("// Primary node up");
  simulate_cluster_node_up(GetParam().cluster_type, node_ports,
                           node_http_ports[0]);
  if (use_tcp_port_acceptors) {
    EXPECT_TRUE(wait_for_port_used(*router_rw_port));
    EXPECT_TRUE(is_port_bindable(*router_ro_port));
    EXPECT_TRUE(wait_for_port_used(*router_rw_x_port));
    EXPECT_TRUE(is_port_bindable(*router_ro_x_port));
  }
  if (use_unix_socket_acceptors) {
    EXPECT_TRUE(wait_file_exists(*router_rw_socket));
    EXPECT_TRUE(is_socket_bindable(*router_ro_socket));
    EXPECT_TRUE(wait_file_exists(*router_rw_x_socket));
    EXPECT_TRUE(is_socket_bindable(*router_ro_x_socket));
  }

  SCOPED_TRACE("RW queries are working fine");
  if (use_tcp_port_acceptors) {
    ASSERT_NO_FATAL_FAILURE(
        try_connection("127.0.0.1", *router_rw_port, "username", "password"));
  }
  if (use_unix_socket_acceptors) {
    ASSERT_NO_FATAL_FAILURE(
        try_connection(*router_rw_socket, "username", "password"));
  }
}

// WL#13327: TS_R1_3
TEST_P(SocketCloseOnMetadataUnavailable, 1RO) {
  const bool use_tcp_port_acceptors =
      GetParam().acceptors & static_cast<Acceptors>(AcceptorType::TcpSocket);
  const bool use_unix_socket_acceptors =
      GetParam().acceptors & static_cast<Acceptors>(AcceptorType::UnixSocket);

#ifdef _WIN32
  if (use_unix_socket_acceptors) {
    GTEST_SKIP() << "No support for Unix sockets on Windows";
  }
#endif

  SCOPED_TRACE("// launch cluster with only RO node");
  ASSERT_NO_FATAL_FAILURE(
      setup_cluster(1, GetParam().tracefile, /*no_primary*/ true));

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  ASSERT_NO_FATAL_FAILURE(setup_router(GetParam().cluster_type,
                                       GetParam().acceptors,
                                       /*read_only*/ true));

  SCOPED_TRACE("// check if RO port is used");
  if (use_tcp_port_acceptors) {
    EXPECT_TRUE(wait_for_port_used(*router_ro_port));
    EXPECT_TRUE(wait_for_port_used(*router_ro_x_port));
    EXPECT_TRUE(wait_for_port_unused(*router_rw_port));
    EXPECT_TRUE(wait_for_port_unused(*router_rw_x_port));
  }
  if (use_unix_socket_acceptors) {
    EXPECT_TRUE(wait_file_exists(*router_ro_socket));
    EXPECT_TRUE(wait_file_exists(*router_ro_x_socket));
    EXPECT_FALSE(wait_file_exists(*router_rw_socket, false));
    EXPECT_FALSE(wait_file_exists(*router_rw_x_socket, false));
  }

  SCOPED_TRACE("// Node down");
  simulate_cluster_node_down(node_ports, node_http_ports[0]);
  if (use_tcp_port_acceptors) {
    EXPECT_TRUE(is_port_bindable(*router_rw_port));
    EXPECT_TRUE(wait_for_port_unused(*router_ro_port));
    EXPECT_TRUE(is_port_bindable(*router_rw_x_port));
    EXPECT_TRUE(wait_for_port_unused(*router_ro_x_port));
  }
  if (use_unix_socket_acceptors) {
    EXPECT_TRUE(is_socket_bindable(*router_rw_socket));
    EXPECT_FALSE(wait_file_exists(*router_ro_x_socket, false));
    EXPECT_TRUE(is_socket_bindable(*router_rw_x_socket));
    EXPECT_FALSE(wait_file_exists(*router_rw_x_socket, false));
  }

  SCOPED_TRACE("// RW and RO queries fail");
  if (use_tcp_port_acceptors) {
    ASSERT_ANY_THROW(
        try_connection("127.0.0.1", *router_ro_port, "username", "password"));
    ASSERT_ANY_THROW(
        try_connection("127.0.0.1", *router_rw_port, "username", "password"));
  }
  if (use_unix_socket_acceptors) {
    ASSERT_ANY_THROW(try_connection(*router_ro_socket, "username", "password"));
    ASSERT_ANY_THROW(try_connection(*router_rw_socket, "username", "password"));
  }

  SCOPED_TRACE("// Node up");
  simulate_cluster_node_up(GetParam().cluster_type, node_ports,
                           node_http_ports[0], /*no primary*/ true);
  if (use_tcp_port_acceptors) {
    EXPECT_TRUE(is_port_bindable(*router_rw_port));
    EXPECT_TRUE(wait_for_port_used(*router_ro_port));
    EXPECT_TRUE(is_port_bindable(*router_rw_x_port));
    EXPECT_TRUE(wait_for_port_used(*router_ro_x_port));
  }
  if (use_unix_socket_acceptors) {
    EXPECT_TRUE(is_socket_bindable(*router_rw_socket));
    EXPECT_TRUE(wait_file_exists(*router_ro_socket));
    EXPECT_TRUE(is_socket_bindable(*router_rw_x_socket));
    EXPECT_TRUE(wait_file_exists(*router_ro_x_socket));
  }

  SCOPED_TRACE("RO queries are working fine");
  if (use_tcp_port_acceptors) {
    ASSERT_NO_FATAL_FAILURE(
        try_connection("127.0.0.1", *router_ro_port, "username", "password"));
  }
  if (use_unix_socket_acceptors) {
    ASSERT_NO_FATAL_FAILURE(
        try_connection(*router_ro_socket, "username", "password"));
  }
}

// WL#13327: TS_R1_2
TEST_P(SocketCloseOnMetadataUnavailable, 2RO) {
  const bool use_tcp_port_acceptors =
      GetParam().acceptors & static_cast<Acceptors>(AcceptorType::TcpSocket);
  const bool use_unix_socket_acceptors =
      GetParam().acceptors & static_cast<Acceptors>(AcceptorType::UnixSocket);

#ifdef _WIN32
  if (use_unix_socket_acceptors) {
    GTEST_SKIP() << "No support for Unix sockets on Windows";
  }
#endif

  SCOPED_TRACE("// launch cluster with 2 RO nodes");
  ASSERT_NO_FATAL_FAILURE(
      setup_cluster(2, GetParam().tracefile, /*no_primary*/ true));

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  ASSERT_NO_FATAL_FAILURE(setup_router(GetParam().cluster_type,
                                       GetParam().acceptors,
                                       /*read_only*/ true));

  SCOPED_TRACE("// check if RO port is used");
  if (use_tcp_port_acceptors) {
    EXPECT_TRUE(wait_for_port_used(*router_ro_port));
    EXPECT_TRUE(wait_for_port_used(*router_ro_x_port));
    EXPECT_TRUE(wait_for_port_unused(*router_rw_port));
    EXPECT_TRUE(wait_for_port_unused(*router_rw_x_port));
  }
  if (use_unix_socket_acceptors) {
    EXPECT_TRUE(wait_file_exists(*router_ro_socket));
    EXPECT_TRUE(wait_file_exists(*router_ro_x_socket));
    EXPECT_FALSE(wait_file_exists(*router_rw_socket, false));
    EXPECT_FALSE(wait_file_exists(*router_rw_x_socket, false));
  }

  SCOPED_TRACE("// First node down");
  simulate_cluster_node_down(node_ports, node_http_ports[0]);
  if (use_tcp_port_acceptors) {
    EXPECT_TRUE(wait_for_port_used(*router_ro_port));
    EXPECT_TRUE(wait_for_port_used(*router_ro_x_port));
    EXPECT_TRUE(is_port_bindable(*router_rw_port));
    EXPECT_TRUE(is_port_bindable(*router_rw_x_port));
  }
  if (use_unix_socket_acceptors) {
    EXPECT_TRUE(wait_file_exists(*router_ro_socket));
    EXPECT_TRUE(wait_file_exists(*router_ro_x_socket));
    EXPECT_TRUE(is_socket_bindable(*router_rw_socket));
    EXPECT_TRUE(is_socket_bindable(*router_rw_x_socket));
  }

  SCOPED_TRACE("// Second node down");
  simulate_cluster_node_down(node_ports, node_http_ports[1]);
  if (use_tcp_port_acceptors) {
    EXPECT_TRUE(wait_for_port_unused(*router_ro_port));
    EXPECT_TRUE(wait_for_port_unused(*router_ro_x_port));
    EXPECT_TRUE(is_port_bindable(*router_rw_port));
    EXPECT_TRUE(is_port_bindable(*router_rw_x_port));
  }
  if (use_unix_socket_acceptors) {
    EXPECT_FALSE(wait_file_exists(*router_ro_socket, false));
    EXPECT_FALSE(wait_file_exists(*router_ro_x_socket, false));
    EXPECT_TRUE(is_socket_bindable(*router_rw_socket));
    EXPECT_TRUE(is_socket_bindable(*router_rw_x_socket));
  }

  SCOPED_TRACE("// RW and RO queries fail");
  if (use_tcp_port_acceptors) {
    ASSERT_ANY_THROW(
        try_connection("127.0.0.1", *router_ro_port, "username", "password"));
    ASSERT_ANY_THROW(
        try_connection("127.0.0.1", *router_rw_port, "username", "password"));
  }
  if (use_unix_socket_acceptors) {
    ASSERT_ANY_THROW(try_connection(*router_ro_socket, "username", "password"));
    ASSERT_ANY_THROW(try_connection(*router_rw_socket, "username", "password"));
  }

  SCOPED_TRACE("// Second node up");
  simulate_cluster_node_up(GetParam().cluster_type, node_ports,
                           node_http_ports[1], /*no primary*/ true);
  if (use_tcp_port_acceptors) {
    EXPECT_TRUE(is_port_bindable(*router_rw_port));
    EXPECT_TRUE(wait_for_port_used(*router_ro_port));
    EXPECT_TRUE(is_port_bindable(*router_rw_x_port));
    EXPECT_TRUE(wait_for_port_used(*router_ro_x_port));
  }
  if (use_unix_socket_acceptors) {
    EXPECT_TRUE(is_socket_bindable(*router_rw_socket));
    EXPECT_TRUE(wait_file_exists(*router_ro_socket));
    EXPECT_TRUE(is_socket_bindable(*router_rw_x_socket));
    EXPECT_TRUE(wait_file_exists(*router_ro_x_socket));
  }

  SCOPED_TRACE("// Second node down");
  simulate_cluster_node_down(node_ports, node_http_ports[1]);
  if (use_tcp_port_acceptors) {
    EXPECT_TRUE(wait_for_port_unused(*router_ro_port));
    EXPECT_TRUE(wait_for_port_unused(*router_ro_x_port));
    EXPECT_TRUE(is_port_bindable(*router_rw_port));
    EXPECT_TRUE(is_port_bindable(*router_rw_x_port));
  }
  if (use_unix_socket_acceptors) {
    EXPECT_FALSE(wait_file_exists(*router_ro_socket, false));
    EXPECT_FALSE(wait_file_exists(*router_ro_x_socket, false));
    EXPECT_TRUE(is_socket_bindable(*router_rw_socket));
    EXPECT_TRUE(is_socket_bindable(*router_rw_x_socket));
  }

  SCOPED_TRACE("// First node up");
  simulate_cluster_node_up(GetParam().cluster_type, node_ports,
                           node_http_ports[0], /*no primary*/ true);
  if (use_tcp_port_acceptors) {
    EXPECT_TRUE(is_port_bindable(*router_rw_port));
    EXPECT_TRUE(wait_for_port_used(*router_ro_port));
    EXPECT_TRUE(is_port_bindable(*router_rw_x_port));
    EXPECT_TRUE(wait_for_port_used(*router_ro_x_port));
  }
  if (use_unix_socket_acceptors) {
    EXPECT_TRUE(is_socket_bindable(*router_rw_socket));
    EXPECT_TRUE(wait_file_exists(*router_ro_socket));
    EXPECT_TRUE(is_socket_bindable(*router_rw_x_socket));
    EXPECT_TRUE(wait_file_exists(*router_ro_x_socket));
  }

  SCOPED_TRACE("RO queries are working fine");
  if (use_tcp_port_acceptors) {
    ASSERT_NO_FATAL_FAILURE(
        try_connection("127.0.0.1", *router_ro_port, "username", "password"));
  }
  if (use_unix_socket_acceptors) {
    ASSERT_NO_FATAL_FAILURE(
        try_connection(*router_ro_socket, "username", "password"));
  }
}

INSTANTIATE_TEST_SUITE_P(
    SocketCloseOnMetadataUnavailableTest, SocketCloseOnMetadataUnavailable,
    ::testing::Values(
        SocketsCloseTestParams("metadata_dynamic_nodes_v2_gr.js",
                               "close_port_on_metadata_unavailable_gr_v2",
                               ClusterType::GR_V2,
                               Acceptors(AcceptorType::TcpSocket)),
        SocketsCloseTestParams("metadata_dynamic_nodes_v2_ar.js",
                               "close_port_on_metadata_unavailable_ar_v2",
                               ClusterType::RS_V2,
                               Acceptors(AcceptorType::TcpSocket)),
        SocketsCloseTestParams("metadata_dynamic_nodes_v2_gr.js",
                               "close_socket_on_metadata_unavailable_gr_v2",
                               ClusterType::GR_V2,
                               Acceptors(AcceptorType::UnixSocket)),
        SocketsCloseTestParams(
            "metadata_dynamic_nodes_v2_gr.js",
            "close_port_and_socket_on_metadata_unavailable_gr_v2",
            ClusterType::GR_V2,
            Acceptors(AcceptorType::TcpSocket) |
                Acceptors(AcceptorType::UnixSocket))),
    get_test_description);

class ErrmsgResponderBase {
 public:
  // error-code to return on connect
  static constexpr const uint16_t error_code{1130};
  // error-msg to return on connect
  static constexpr const char error_msg[] = "You shall not pass";
};

template <class Sock>
class ErrmsgResponder : public ErrmsgResponderBase {
 public:
  using socket_type = Sock;

  explicit ErrmsgResponder(socket_type sock) : sock_(std::move(sock)) {}

  stdx::expected<void, std::error_code> respond() {
    std::vector<uint8_t> err_frame;

    const auto encode_res =
        classic_protocol::encode<classic_protocol::frame::Frame<
            classic_protocol::message::server::Error>>(
            {0, {error_code, error_msg, "HY000"}}, {},
            net::dynamic_buffer(err_frame));
    if (!encode_res) return stdx::unexpected(encode_res.error());

    const auto write_res = net::write(sock_, net::buffer(err_frame));
    if (!write_res) return stdx::unexpected(write_res.error());

    // wait until the client closed the connection on us.
    //
    while (true) {
      std::vector<std::string> drainer;
      const auto read_res = net::read(sock_, net::dynamic_buffer(drainer));

      if (!read_res &&
          read_res.error() == make_error_code(net::stream_errc::eof)) {
        break;
      }

      // looks like something else happened. At least log it.
      if (read_res) {
        std::cerr << __LINE__ << ": " << read_res.value() << std::endl;
      } else {
        return stdx::unexpected(read_res.error());
      }
    }

    return {};
  }

 private:
  socket_type sock_;
};

template <class AcceptorType>
class AcceptingEndpointUser {
 public:
  class AcceptCompletor {
   public:
    AcceptCompletor(AcceptorType &acceptor, Monitor<bool> &is_stopped)
        : acceptor_(acceptor), is_stopped_(is_stopped) {}

    void operator()(std::error_code ec, auto client_sock) {
      if (ec) return;

      ErrmsgResponder responder(std::move(client_sock));

      responder.respond();

      is_stopped_([&](bool stopped) {
        if (stopped) return;

        // accept the next one.
        acceptor_.async_accept(AcceptCompletor(acceptor_, is_stopped_));
      });
    }

   private:
    AcceptorType &acceptor_;

    Monitor<bool> &is_stopped_;
  };

  virtual ~AcceptingEndpointUser() { unlock(); }

  virtual bool lock(std::chrono::milliseconds timeout = 120s) {
    // socket can end up in a TIME_WAIT state so it could take a while for it
    // to be available again.
    const std::chrono::milliseconds step = 50ms;
    do {
      if (try_lock()) return true;
      std::this_thread::sleep_for(step);
      timeout -= step;
    } while (timeout > 0ms);
    return false;
  }

  virtual void unlock() {
    is_stopped_([this](bool &stopped) {
      stopped = true;

      // abort a currently running accept(), if there is one.
      acceptor_.cancel();
    });

    if (worker_.joinable()) worker_.join();

    // exits the io_ctx_.run() is as there is no other user.
    acceptor_.close();

    if (worker_ec_) {
      FAIL() << "acceptor() failed after accept() with: " << worker_ec_ << " "
             << worker_ec_.message();
    }
  }

 protected:
  virtual bool open_and_bind() = 0;

  virtual bool try_lock() {
    if (!open_and_bind()) return false;

    const auto listen_res = acceptor_.listen(128);
    if (!listen_res) {
      return false;
    }

    // spawn off a thread to handle a connect.
    worker_ = std::thread([this]() {
      acceptor_.async_accept(AcceptCompletor(acceptor_, is_stopped_));

      io_ctx_.run();
    });

    return true;
  }

  Monitor<bool> is_stopped_{false};

  std::thread worker_;
  std::error_code worker_ec_{};

  net::io_context io_ctx_;
  AcceptorType acceptor_{io_ctx_};
};

class TCPPortUser : public AcceptingEndpointUser<net::ip::tcp::acceptor> {
 public:
  TCPPortUser(std::string hostname, const uint16_t port)
      : hostname_{std::move(hostname)}, port_{port} {}

 protected:
  bool open_and_bind() override {
    net::ip::tcp::resolver resolver{io_ctx_};
    const auto &resolve_res =
        resolver.resolve(hostname_, std::to_string(port_));
    if (!resolve_res) return false;

    const auto &open_res =
        acceptor_.open(resolve_res->begin()->endpoint().protocol());
    if (!open_res) {
      return false;
    }

#if !defined(_WIN32)
    // don't use reuse-addr on windows as it works differently as on Unix.
    acceptor_.set_option(net::socket_base::reuse_address{true});
#endif

    const auto &bind_res = acceptor_.bind(resolve_res->begin()->endpoint());
    if (!bind_res) {
      acceptor_.close();
      return false;
    }

    return true;
  }

  const std::string hostname_{"127.0.0.1"};
  const uint16_t port_;
};

class UnixSocketUser
    : public AcceptingEndpointUser<local::stream_protocol::acceptor> {
 public:
  UnixSocketUser(std::string socket) : socket_{std::move(socket)} {}

  void cleanup() { unlink(socket_.c_str()); }

 protected:
  bool open_and_bind() override {
    const auto open_res = acceptor_.open();
    if (!open_res) {
      return false;
    }

    local::stream_protocol::endpoint ep(socket_);
    const auto bind_res = acceptor_.bind(ep);
    if (!bind_res) {
      return false;
    }

    return true;
  }

  const std::string socket_;
};

TEST_F(SocketCloseTest, StaticRoundRobinTCPPort) {
  SCOPED_TRACE("// launch cluster with one node");
  ASSERT_NO_FATAL_FAILURE(setup_cluster(1, "my_port.js"));

  router_rw_port = port_pool_.get_next_available();
  const auto router_rw_port_str = std::to_string(*router_rw_port);

  const std::string routing_section = get_static_routing_section(
      router_rw_port, std::nullopt, node_ports, "round-robin");

  SCOPED_TRACE("// launch the router with static routing configuration");
  launch_router("", routing_section, EXIT_SUCCESS,
                /*wait_for_notify_ready=*/30s);

  SCOPED_TRACE("// tcp-port:" + router_rw_port_str + " is used by the router");
  // check with netstat that the port is used by router.
  EXPECT_TRUE(wait_for_port_used(*router_rw_port));

  SCOPED_TRACE(
      "// kill backend and wait until router has released the tcp-port:" +
      std::to_string(*router_rw_port));
  EXPECT_NO_THROW(cluster_nodes[0]->send_clean_shutdown_event());
  EXPECT_NO_THROW(cluster_nodes[0]->wait_for_exit());

  EXPECT_THROW(try_connection("127.0.0.1", *router_rw_port, custom_user,
                              custom_password),
               std::runtime_error);
  EXPECT_TRUE(wait_for_port_unused(*router_rw_port, 120s));

  SCOPED_TRACE("// block router from binding to tcp-port:" +
               router_rw_port_str + " by let another app bind to it");
  TCPPortUser socket_user("127.0.0.1", *router_rw_port);
  EXPECT_TRUE(socket_user.lock());

  EXPECT_TRUE(wait_for_port_used(*router_rw_port, 120s));

  SCOPED_TRACE("// Restore a cluster node on tcp-port " +
               std::to_string(node_ports[0]) +
               " to bring the destination back from "
               "quarantine.");

  cluster_nodes.push_back(
      &mock_server_spawner().spawn(mock_server_cmdline("my_port.js")
                                       .port(node_ports[0])
                                       .http_port(node_http_ports[0])
                                       .args()));

  set_mock_metadata(
      node_http_ports[0], "uuid", classic_ports_to_gr_nodes(node_ports), 0,
      classic_ports_to_cluster_nodes(node_ports), 0, false, "localhost");

  SCOPED_TRACE("// check we can connect to tcp:" + router_rw_port_str +
               ", but get the other app.");

  try {
    try_connection("127.0.0.1", *router_rw_port, custom_user, custom_password);
    FAIL() << "should have failed";
  } catch (const MySQLSession::Error &e) {
    EXPECT_EQ(e.code(), ErrmsgResponderBase::error_code);
    EXPECT_THAT(e.what(), ::testing::HasSubstr(ErrmsgResponderBase::error_msg));
  }

  // sleep for a while to test that when the quarantine wants to reopen the
  // acceptor port and it fails it will still be retried later when the port
  // become available
  std::this_thread::sleep_for(1.5s);

  SCOPED_TRACE("// Release the tcp-port:" + router_rw_port_str +
               ", and wait a bit to set router bind to the port again");
  socket_user.unlock();

  SCOPED_TRACE("// wait until the router binds to the port again.");
  EXPECT_TRUE(wait_for_port_used(*router_rw_port, 120s));

  try {
    try_connection("127.0.0.1", *router_rw_port, custom_user, custom_password);
  } catch (const MySQLSession::Error &e) {
    FAIL() << e.what();
  }
}

#ifndef _WIN32

TEST_F(SocketCloseTest, StaticRoundRobinUnixSocket) {
  SCOPED_TRACE("// launch cluster with one node");
  ASSERT_NO_FATAL_FAILURE(setup_cluster(1, "my_port.js"));

  router_rw_socket = get_test_temp_dir_name() + "/mysql.socket";

  const std::string routing_section = get_static_routing_section(
      std::nullopt, router_rw_socket, node_ports, "round-robin");

  SCOPED_TRACE("// launch the router with static routing configuration");
  launch_router("", routing_section, EXIT_SUCCESS,
                /*wait_for_notify_ready=*/30s);

  SCOPED_TRACE("// unix socket:" + *router_rw_socket +
               " is used by the router");
  EXPECT_TRUE(wait_file_exists(*router_rw_socket));

  SCOPED_TRACE(
      "// kill backend and wait until router has released the unix socket:" +
      *router_rw_socket);
  EXPECT_NO_THROW(cluster_nodes[0]->send_clean_shutdown_event());
  EXPECT_NO_THROW(cluster_nodes[0]->wait_for_exit());

  try {
    try_connection(*router_rw_socket, custom_user, custom_password);
    FAIL() << "expected to fail";
  } catch (const MySQLSession::Error &e) {
    // /tmp/router-0tsoKZ/mysql.socket: Can't connect to remote MySQL server
    // (2003)
    EXPECT_EQ(e.code(), 2003) << e.what();
  }

  EXPECT_FALSE(wait_file_exists(*router_rw_socket, false, 10s));

  SCOPED_TRACE("// block router from binding to unix socket:" +
               *router_rw_socket + " by let another app bind to it");

  UnixSocketUser socket_user(*router_rw_socket);
  EXPECT_TRUE(socket_user.lock());

  EXPECT_TRUE(wait_file_exists(*router_rw_socket, true, 10s));

  SCOPED_TRACE("// Restore a cluster node on tcp-port " +
               std::to_string(node_ports[0]) +
               " to bring the destination back from "
               "quarantine.");

  cluster_nodes.push_back(
      &mock_server_spawner().spawn(mock_server_cmdline("my_port.js")
                                       .port(node_ports[0])
                                       .http_port(node_http_ports[0])
                                       .args()));

  set_mock_metadata(
      node_http_ports[0], "uuid", classic_ports_to_gr_nodes(node_ports), 0,
      classic_ports_to_cluster_nodes(node_ports), 0, false, "localhost");

  SCOPED_TRACE("// check we can connect to unix socket:" + *router_rw_socket +
               ", but get the other app.");

  try {
    try_connection(*router_rw_socket, custom_user, custom_password);
    FAIL() << "should have failed";
  } catch (const MySQLSession::Error &e) {
    EXPECT_EQ(e.code(), ErrmsgResponderBase::error_code);
    EXPECT_THAT(e.what(), ::testing::HasSubstr(ErrmsgResponderBase::error_msg));
  }

  // sleep for a while to test that when the quarantine wants to reopen the
  // acceptor socket and it fails it will still be retried later when the socket
  // become available
  std::this_thread::sleep_for(1.5s);

  SCOPED_TRACE("// Release the unix socket:" + *router_rw_socket +
               ", and wait a bit to set router bind to the socket again");
  socket_user.unlock();
  socket_user.cleanup();

  SCOPED_TRACE("// wait until the router binds to the socket again.");
  EXPECT_TRUE(wait_file_exists(*router_rw_socket, true, 10s));

  try {
    try_connection(*router_rw_socket, custom_user, custom_password);
  } catch (const MySQLSession::Error &e) {
    FAIL() << e.what();
  }
}
#endif

enum class PortType { RW, RO, X_RW, X_RO };

struct FailToOpenSocketParams {
  // mock_server trace file
  std::string tracefile;

  // the type of the cluster GR or AR
  ClusterType cluster_type;

  // ports that are unavailable
  std::vector<PortType> unavailable_ports;

  FailToOpenSocketParams(std::string tracefile_, ClusterType cluster_type_,
                         std::vector<PortType> unavailable_ports_)
      : tracefile(std::move(tracefile_)),
        cluster_type(cluster_type_),
        unavailable_ports(std::move(unavailable_ports_)) {}
};

class FailToOpenSocket : public SocketCloseTest {
 public:
  FailToOpenSocket() {
    router_rw_port = port_pool_.get_next_available();
    router_ro_port = port_pool_.get_next_available();
    router_rw_x_port = port_pool_.get_next_available();
    router_ro_x_port = port_pool_.get_next_available();

    port_mapping.emplace(PortType::RW, *router_rw_port);
    port_mapping.emplace(PortType::RO, *router_ro_port);
    port_mapping.emplace(PortType::X_RW, *router_rw_x_port);
    port_mapping.emplace(PortType::X_RO, *router_ro_x_port);
  }

  std::map<PortType, uint16_t> port_mapping;
};

class FailToOpenROSocketAfterStartup
    : public FailToOpenSocket,
      public ::testing::WithParamInterface<FailToOpenSocketParams> {};

TEST_P(FailToOpenROSocketAfterStartup, ROportTaken) {
  SCOPED_TRACE("// launch cluster with 3 nodes, 1 RW/2 RO");
  ASSERT_NO_FATAL_FAILURE(setup_cluster(3, GetParam().tracefile));
  const auto test_port = port_mapping.at(GetParam().unavailable_ports[0]);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  ASSERT_NO_FATAL_FAILURE(setup_router(GetParam().cluster_type,
                                       Acceptors(AcceptorType::TcpSocket)));
  EXPECT_TRUE(wait_for_port_used(*router_ro_port));

  SCOPED_TRACE("// RO nodes hidden");
  auto cluster_nodes = classic_ports_to_cluster_nodes(node_ports);
  cluster_nodes[1].attributes = R"({"tags" : {"_hidden": true} })";
  cluster_nodes[2].attributes = R"({"tags" : {"_hidden": true} })";
  set_mock_metadata(node_http_ports[0], "uuid",
                    classic_ports_to_gr_nodes(node_ports), 0, cluster_nodes, 0,
                    false, "127.0.0.1");

  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 4));
  EXPECT_FALSE(is_port_bindable(*router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(*router_ro_port));
  EXPECT_FALSE(is_port_bindable(*router_rw_x_port));
  EXPECT_TRUE(wait_for_port_unused(*router_ro_x_port));

  SCOPED_TRACE("// Take RO port by other application");
  TCPPortUser socket_user("127.0.0.1", test_port);
  socket_user.lock();

  SCOPED_TRACE("// Unhide one RO node");
  cluster_nodes[2].attributes = "";
  set_mock_metadata(node_http_ports[0], "uuid",
                    classic_ports_to_gr_nodes(node_ports), 0, cluster_nodes, 0,
                    false, "127.0.0.1");
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 4));
  EXPECT_TRUE(wait_for_port_used(*router_rw_port));
  EXPECT_TRUE(wait_for_port_used(*router_rw_x_port));

  SCOPED_TRACE("// RO connections should fail");
  EXPECT_NO_THROW(try_connection("127.0.0.1", *router_rw_port, custom_user,
                                 custom_password));
  EXPECT_THROW(
      try_connection("127.0.0.1", test_port, custom_user, custom_password),
      std::runtime_error);

  SCOPED_TRACE("// Free RO socket taken by other application");
  socket_user.unlock();

  SCOPED_TRACE("// Wait until the router port is listening again");
  EXPECT_TRUE(wait_for_port_used(test_port));

  SCOPED_TRACE("// RO and RW queries should work fine");
  EXPECT_NO_THROW(try_connection("127.0.0.1", *router_rw_port, custom_user,
                                 custom_password));
  EXPECT_NO_THROW(try_connection("127.0.0.1", *router_ro_port, custom_user,
                                 custom_password));
}

INSTANTIATE_TEST_SUITE_P(
    FailToOpenROSocketAfterStartupTest, FailToOpenROSocketAfterStartup,
    ::testing::Values(
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_gr.js",
                               ClusterType::GR_V2,
                               std::vector<PortType>({PortType::RO})),
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_gr.js",
                               ClusterType::GR_V2,
                               std::vector<PortType>({PortType::X_RO})),
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_ar.js",
                               ClusterType::RS_V2,
                               std::vector<PortType>({PortType::RO})),
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_ar.js",
                               ClusterType::RS_V2,
                               std::vector<PortType>({PortType::X_RO}))));

class FailToOpenRWSocketAfterStartup
    : public FailToOpenSocket,
      public ::testing::WithParamInterface<FailToOpenSocketParams> {};

TEST_P(FailToOpenRWSocketAfterStartup, RWportTaken) {
  SCOPED_TRACE("// launch cluster with 3 nodes, 1 RW/2 RO");
  ASSERT_NO_FATAL_FAILURE(setup_cluster(3, GetParam().tracefile));
  const auto test_port = port_mapping.at(GetParam().unavailable_ports[0]);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  ASSERT_NO_FATAL_FAILURE(setup_router(GetParam().cluster_type,
                                       Acceptors(AcceptorType::TcpSocket)));
  EXPECT_TRUE(wait_for_port_used(*router_rw_port));

  SCOPED_TRACE("// RW node hidden");
  auto cluster_nodes = classic_ports_to_cluster_nodes(node_ports);
  cluster_nodes[0].attributes = R"({"tags" : {"_hidden": true} })";
  set_mock_metadata(node_http_ports[0], "uuid",
                    classic_ports_to_gr_nodes(node_ports), 0, cluster_nodes, 0,
                    false, "127.0.0.1");
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 4));
  EXPECT_TRUE(wait_for_port_unused(*router_rw_port));
  EXPECT_FALSE(is_port_bindable(*router_ro_port));
  EXPECT_TRUE(wait_for_port_unused(*router_rw_x_port));
  EXPECT_FALSE(is_port_bindable(*router_ro_x_port));

  SCOPED_TRACE("// Take RW(X) port by other application");
  TCPPortUser socket_user("127.0.0.1", test_port);
  socket_user.lock();

  SCOPED_TRACE("// Unhide RW node");
  cluster_nodes[0].attributes = "";
  set_mock_metadata(node_http_ports[0], "uuid",
                    classic_ports_to_gr_nodes(node_ports), 0, cluster_nodes, 0,
                    false, "127.0.0.1");
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 4));
  EXPECT_TRUE(wait_for_port_used(*router_ro_port));
  EXPECT_TRUE(wait_for_port_used(*router_ro_x_port));

  SCOPED_TRACE("// RW(X) connections should fail");
  EXPECT_THROW(
      try_connection("127.0.0.1", test_port, custom_user, custom_password),
      std::runtime_error);
  EXPECT_NO_THROW(try_connection("127.0.0.1", *router_ro_port, custom_user,
                                 custom_password));

  SCOPED_TRACE("// Free RW socket taken by other application");
  socket_user.unlock();

  SCOPED_TRACE("// Wait for the socket listening again");
  EXPECT_TRUE(wait_for_port_used(test_port));

  SCOPED_TRACE("// RO and RW queries should work fine");
  EXPECT_NO_THROW(try_connection("127.0.0.1", *router_rw_port, custom_user,
                                 custom_password));
  EXPECT_NO_THROW(try_connection("127.0.0.1", *router_ro_port, custom_user,
                                 custom_password));
}

INSTANTIATE_TEST_SUITE_P(
    FailToOpenRWSocketAfterStartupTest, FailToOpenRWSocketAfterStartup,
    ::testing::Values(
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_gr.js",
                               ClusterType::GR_V2,
                               std::vector<PortType>({PortType::RW})),
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_gr.js",
                               ClusterType::GR_V2,
                               std::vector<PortType>({PortType::X_RW})),
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_ar.js",
                               ClusterType::RS_V2,
                               std::vector<PortType>({PortType::RW})),
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_ar.js",
                               ClusterType::RS_V2,
                               std::vector<PortType>({PortType::X_RW}))));

class FailToOpenSocketOnStartup
    : public FailToOpenSocket,
      public ::testing::WithParamInterface<FailToOpenSocketParams> {};

TEST_P(FailToOpenSocketOnStartup, FailOnStartup) {
  SCOPED_TRACE("// launch cluster with 1RW/2RO nodes");
  ASSERT_NO_FATAL_FAILURE(setup_cluster(3, GetParam().tracefile));

  SCOPED_TRACE("// bind sockets");
  std::vector<std::unique_ptr<TCPPortUser>> socket_users;
  for (const auto &port : GetParam().unavailable_ports) {
    socket_users.push_back(
        std::make_unique<TCPPortUser>("127.0.0.1", port_mapping.at(port)));
  }

  for (const auto &socket_user : socket_users) {
    ASSERT_TRUE(socket_user->lock());
  }

  SCOPED_TRACE("// start router against sockets that are in use.");
  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type);
  std::string routing_section = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "round-robin", "rw");
  routing_section += get_metadata_cache_routing_section(
      router_rw_x_port, "PRIMARY", "round-robin", "x_rw", "x");
  routing_section += get_metadata_cache_routing_section(
      router_ro_port, "SECONDARY", "round-robin", "ro");
  routing_section += get_metadata_cache_routing_section(
      router_ro_x_port, "SECONDARY", "round-robin", "x_ro", "x");

  auto &router =
      launch_router(metadata_cache_section, routing_section, EXIT_FAILURE,
                    /*wait_for_notify_ready=*/-1s);
  EXPECT_NE(router.wait_for_exit(), 0);
}

INSTANTIATE_TEST_SUITE_P(
    FailToOpenSocketTest, FailToOpenSocketOnStartup,
    ::testing::Values(
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_gr.js",
                               ClusterType::GR_V2,
                               std::vector<PortType>({PortType::RW})),
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_ar.js",
                               ClusterType::RS_V2,
                               std::vector<PortType>({PortType::RW})),
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_gr.js",
                               ClusterType::GR_V2,
                               std::vector<PortType>({PortType::RO})),
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_ar.js",
                               ClusterType::RS_V2,
                               std::vector<PortType>({PortType::RO})),
        FailToOpenSocketParams(
            "metadata_dynamic_nodes_v2_gr.js", ClusterType::GR_V2,
            std::vector<PortType>({PortType::RW, PortType::RO})),
        FailToOpenSocketParams(
            "metadata_dynamic_nodes_v2_ar.js", ClusterType::RS_V2,
            std::vector<PortType>({PortType::RW, PortType::RO})),
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_gr.js",
                               ClusterType::GR_V2,
                               std::vector<PortType>({PortType::X_RW})),
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_ar.js",
                               ClusterType::RS_V2,
                               std::vector<PortType>({PortType::X_RW})),
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_gr.js",
                               ClusterType::GR_V2,
                               std::vector<PortType>({PortType::X_RO})),
        FailToOpenSocketParams("metadata_dynamic_nodes_v2_ar.js",
                               ClusterType::RS_V2,
                               std::vector<PortType>({PortType::X_RO})),
        FailToOpenSocketParams(
            "metadata_dynamic_nodes_v2_gr.js", ClusterType::GR_V2,
            std::vector<PortType>({PortType::X_RW, PortType::X_RO})),
        FailToOpenSocketParams(
            "metadata_dynamic_nodes_v2_ar.js", ClusterType::RS_V2,
            std::vector<PortType>({PortType::X_RW, PortType::X_RO})),
        FailToOpenSocketParams(
            "metadata_dynamic_nodes_v2_gr.js", ClusterType::GR_V2,
            std::vector<PortType>({PortType::RW, PortType::RO, PortType::X_RW,
                                   PortType::X_RO})),
        FailToOpenSocketParams(
            "metadata_dynamic_nodes_v2_ar.js", ClusterType::RS_V2,
            std::vector<PortType>({PortType::RW, PortType::RO, PortType::X_RW,
                                   PortType::X_RO}))));

class RoundRobinFallback
    : public SocketCloseTest,
      public ::testing::WithParamInterface<SocketsCloseTestParams> {};

// WL#13327: TS_R3_1
TEST_P(RoundRobinFallback, RoundRobinFallbackTest) {
  const size_t NUM_NODES = 3;
  SCOPED_TRACE("// launch cluster with 1RW/2RO nodes");
  ASSERT_NO_FATAL_FAILURE(setup_cluster(NUM_NODES, GetParam().tracefile));

  router_rw_port = port_pool_.get_next_available();
  router_ro_port = port_pool_.get_next_available();

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type);
  std::string routing_section = get_metadata_cache_routing_section(
      router_rw_port, std::nullopt, "PRIMARY", "round-robin", "rw");
  routing_section += get_metadata_cache_routing_section(
      router_ro_port, std::nullopt, "SECONDARY", "round-robin-with-fallback",
      "ro");

  launch_router(metadata_cache_section, routing_section, EXIT_SUCCESS,
                /*wait_for_notify_ready=*/30s);

  EXPECT_TRUE(wait_for_port_used(*router_rw_port));
  EXPECT_TRUE(wait_for_port_used(*router_ro_port));

  SCOPED_TRACE("// RO nodes hidden");
  auto cluster_nodes = classic_ports_to_cluster_nodes(node_ports);
  cluster_nodes[1].attributes = R"({"tags" : {"_hidden": true} })";
  cluster_nodes[2].attributes = R"({"tags" : {"_hidden": true} })";
  set_mock_metadata(node_http_ports[0], "uuid",
                    classic_ports_to_gr_nodes(node_ports), 0, cluster_nodes, 0,
                    false, "127.0.0.1");
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 4));

  SCOPED_TRACE("// RW and RO sockets are listening");
  EXPECT_FALSE(is_port_bindable(*router_rw_port));
  EXPECT_FALSE(is_port_bindable(*router_ro_port));
  ASSERT_NO_FATAL_FAILURE(try_connection("127.0.0.1", *router_ro_port,
                                         router_user, router_password));
  ASSERT_NO_FATAL_FAILURE(try_connection("127.0.0.1", *router_rw_port,
                                         router_user, router_password));

  SCOPED_TRACE("// Unhide RO nodes");
  cluster_nodes[1].attributes = "";
  cluster_nodes[2].attributes = "";
  set_mock_metadata(node_http_ports[0], "uuid",
                    classic_ports_to_gr_nodes(node_ports), 0, cluster_nodes, 0,
                    false, "127.0.0.1");
  ASSERT_NO_FATAL_FAILURE(try_connection("127.0.0.1", *router_ro_port,
                                         router_user, router_password));
  ASSERT_NO_FATAL_FAILURE(try_connection("127.0.0.1", *router_rw_port,
                                         router_user, router_password));

  SCOPED_TRACE("// Hide primary node");
  cluster_nodes[0].attributes = R"({"tags" : {"_hidden": true} })";
  set_mock_metadata(node_http_ports[0], "uuid",
                    classic_ports_to_gr_nodes(node_ports), 0, cluster_nodes, 0,
                    false, "127.0.0.1");
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 4));
  EXPECT_TRUE(wait_for_port_unused(*router_rw_port));
  EXPECT_FALSE(is_port_bindable(*router_ro_port));
}

INSTANTIATE_TEST_SUITE_P(
    RoundRobinFallbackTest, RoundRobinFallback,
    ::testing::Values(
        SocketsCloseTestParams("metadata_dynamic_nodes_v2_gr.js",
                               "round_robin_with_fallback_gr_v2",
                               ClusterType::GR_V2,
                               Acceptors(AcceptorType::TcpSocket)),
        SocketsCloseTestParams("metadata_dynamic_nodes_v2_ar.js",
                               "round_robin_with_fallback_ar_v2",
                               ClusterType::RS_V2,
                               Acceptors(AcceptorType::TcpSocket))),
    get_test_description);

class FirstAvailableDestMetadataCache
    : public SocketCloseTest,
      public ::testing::WithParamInterface<SocketsCloseTestParams> {};

TEST_P(FirstAvailableDestMetadataCache, FirstAvailableDestMetadataCacheTest) {
  const size_t NUM_NODES = 3;
  SCOPED_TRACE("// launch cluster with 1RW/2RO nodes");
  ASSERT_NO_FATAL_FAILURE(setup_cluster(NUM_NODES, GetParam().tracefile));

  router_rw_port = port_pool_.get_next_available();
  router_ro_port = port_pool_.get_next_available();

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type);
  std::string routing_section = get_metadata_cache_routing_section(
      router_rw_port, std::nullopt, "PRIMARY", "first-available", "rw");
  routing_section += get_metadata_cache_routing_section(
      router_ro_port, std::nullopt, "SECONDARY", "first-available", "ro");

  launch_router(metadata_cache_section, routing_section, EXIT_SUCCESS,
                /*wait_for_notify_ready=*/30s);

  EXPECT_TRUE(wait_for_port_used(*router_rw_port));
  EXPECT_TRUE(wait_for_port_used(*router_ro_port));

  SCOPED_TRACE("// Disable both secondary nodes");
  set_mock_metadata(
      node_http_ports[0], "uuid", classic_ports_to_gr_nodes({node_ports[0]}), 0,
      classic_ports_to_cluster_nodes({node_ports[0]}), 0, false, "localhost");
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 4));

  SCOPED_TRACE("// RO socket is not used by the router");
  EXPECT_TRUE(wait_for_port_used(*router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(*router_ro_port));
  ASSERT_NO_FATAL_FAILURE(try_connection("127.0.0.1", *router_rw_port,
                                         router_user, router_password));
  EXPECT_THROW(try_connection("127.0.0.1", *router_ro_port, custom_user,
                              custom_password),
               std::runtime_error);

  SCOPED_TRACE("// Bring back first RO node");
  set_mock_metadata(
      node_http_ports[0], "uuid",
      classic_ports_to_gr_nodes({node_ports[0], node_ports[1]}), 0,
      classic_ports_to_cluster_nodes({node_ports[0], node_ports[1]}), 0, false,
      "localhost");
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 4));
  EXPECT_TRUE(wait_for_port_used(*router_rw_port));
  EXPECT_TRUE(wait_for_port_used(*router_ro_port));

  SCOPED_TRACE("// Disable first RO node");
  set_mock_metadata(
      node_http_ports[0], "uuid", classic_ports_to_gr_nodes({node_ports[0]}), 0,
      classic_ports_to_cluster_nodes({node_ports[0]}), 0, false, "localhost");
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 4));
  EXPECT_TRUE(wait_for_port_used(*router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(*router_ro_port));

  SCOPED_TRACE("// Bring back second RO node");
  set_mock_metadata(
      node_http_ports[0], "uuid",
      classic_ports_to_gr_nodes({node_ports[0], node_ports[2]}), 0,
      classic_ports_to_cluster_nodes({node_ports[0], node_ports[2]}), 0, false,
      "localhost");
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 4));
  EXPECT_TRUE(wait_for_port_used(*router_rw_port));
  EXPECT_TRUE(wait_for_port_used(*router_ro_port));

  SCOPED_TRACE("// Disable first RO node");
  set_mock_metadata(
      node_http_ports[0], "uuid", classic_ports_to_gr_nodes({node_ports[0]}), 0,
      classic_ports_to_cluster_nodes({node_ports[0]}), 0, false, "localhost");
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 4));
  EXPECT_TRUE(wait_for_port_used(*router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(*router_ro_port));

  SCOPED_TRACE("// Disable primary node");
  simulate_cluster_node_down(node_ports, node_http_ports[0]);
  EXPECT_TRUE(wait_for_port_unused(*router_rw_port));
  EXPECT_TRUE(wait_for_port_unused(*router_ro_port));

  SCOPED_TRACE("// Bring back all nodes");
  simulate_cluster_node_up(GetParam().cluster_type, node_ports,
                           node_http_ports[0]);
  EXPECT_TRUE(wait_for_port_used(*router_rw_port));
  EXPECT_TRUE(wait_for_port_used(*router_ro_port));
  ASSERT_NO_FATAL_FAILURE(try_connection("127.0.0.1", *router_rw_port,
                                         router_user, router_password));
  ASSERT_NO_FATAL_FAILURE(try_connection("127.0.0.1", *router_ro_port,
                                         router_user, router_password));
}

INSTANTIATE_TEST_SUITE_P(
    FirstAvailableDestMetadataCacheTest, FirstAvailableDestMetadataCache,
    ::testing::Values(
        SocketsCloseTestParams("metadata_dynamic_nodes_v2_gr.js",
                               "first_available_gr_v2", ClusterType::GR_V2,
                               Acceptors(AcceptorType::TcpSocket)),
        SocketsCloseTestParams("metadata_dynamic_nodes_v2_ar.js",
                               "first_available_ar_v2", ClusterType::RS_V2,
                               Acceptors(AcceptorType::TcpSocket))),
    get_test_description);

TEST_F(SocketCloseTest, StaticRoutingToNonExistentNodesTest) {
  const auto port1 = port_pool_.get_next_available();
  const auto port2 = port_pool_.get_next_available();
  const auto port3 = port_pool_.get_next_available();
  const auto local_port = port_pool_.get_next_available();
  const std::string routing_section{
      mysql_harness::ConfigBuilder::build_section(
          "routing:R1",
          {{"bind_port", std::to_string(port1)},
           {"routing_strategy", "first-available"},
           {"destinations", "127.0.0.1:" + std::to_string(local_port)},
           {"protocol", "classic"}}) +
      mysql_harness::ConfigBuilder::build_section(
          "routing:R2",
          {{"bind_port", std::to_string(port2)},
           {"routing_strategy", "next-available"},
           {"destinations", "127.0.0.1:" + std::to_string(local_port)},
           {"protocol", "classic"}}) +
      mysql_harness::ConfigBuilder::build_section(
          "routing:R3",
          {{"bind_port", std::to_string(port3)},
           {"routing_strategy", "round-robin"},
           {"destinations", "127.0.0.1:" + std::to_string(local_port)},
           {"protocol", "classic"}})};

  SCOPED_TRACE("// launch the router with static routing configuration");
  launch_router("", routing_section, EXIT_SUCCESS,
                /*wait_for_notify_ready=*/30s);

  EXPECT_THROW(try_connection("127.0.0.1", port1, custom_user, custom_password),
               std::runtime_error);
  EXPECT_FALSE(is_port_bindable(port1));

  EXPECT_THROW(try_connection("127.0.0.1", port2, custom_user, custom_password),
               std::runtime_error);
  EXPECT_TRUE(wait_for_port_unused(port2, 120s));

  EXPECT_THROW(try_connection("127.0.0.1", port3, custom_user, custom_password),
               std::runtime_error);
  EXPECT_TRUE(wait_for_port_unused(port3, 120s));
}

struct SharedQuarantineSocketCloseParam {
  std::string strategy;
  bool is_socket_closed;
};

class SharedQuarantineSocketClose
    : public SocketCloseTest,
      public ::testing::WithParamInterface<SharedQuarantineSocketCloseParam> {};

TEST_P(SharedQuarantineSocketClose, cross_plugin_socket_shutdown) {
  ASSERT_NO_FATAL_FAILURE(setup_cluster(1, "metadata_dynamic_nodes_v2_gr.js"));
  const auto bind_port_r1 = port_pool_.get_next_available();
  const auto bind_port_r2 = port_pool_.get_next_available();
  const std::string routing_section{
      mysql_harness::ConfigBuilder::build_section(
          "routing:R1",
          {{"bind_port", std::to_string(bind_port_r1)},
           {"routing_strategy", "round-robin"},
           {"destinations", "127.0.0.1:" + std::to_string(node_ports[0])},
           {"protocol", "classic"}}) +
      mysql_harness::ConfigBuilder::build_section(
          "routing:R2",
          {{"bind_port", std::to_string(bind_port_r2)},
           {"routing_strategy", GetParam().strategy},
           {"destinations", "127.0.0.1:" + std::to_string(node_ports[0])},
           {"protocol", "classic"}})};

  SCOPED_TRACE("// launch the router with static routing configuration");
  launch_router("", routing_section, EXIT_SUCCESS,
                /*wait_for_notify_ready=*/30s);

  SCOPED_TRACE("// both routing plugins are working fine");
  ASSERT_NO_THROW(
      try_connection("127.0.0.1", bind_port_r1, router_user, router_password));
  ASSERT_NO_THROW(
      try_connection("127.0.0.1", bind_port_r2, router_user, router_password));

  SCOPED_TRACE("// kill the server");
  EXPECT_NO_THROW(cluster_nodes[0]->send_clean_shutdown_event());
  EXPECT_NO_THROW(cluster_nodes[0]->wait_for_exit());

  SCOPED_TRACE(
      "// establishing a connection to first routing plugin will add the node "
      "to a quarantine");
  ASSERT_ANY_THROW(
      try_connection("127.0.0.1", bind_port_r1, router_user, router_password));
  SCOPED_TRACE("// first routing plugin has closed the socket");
  EXPECT_TRUE(wait_for_port_unused(bind_port_r1, 120s));
  SCOPED_TRACE(
      "// second routing plugin has closed socket even though there were no "
      "incoming connections (unless it is using first-available policy)");
  EXPECT_EQ(GetParam().is_socket_closed,
            wait_for_port_unused(bind_port_r2, 1s));
}

INSTANTIATE_TEST_SUITE_P(
    SharedQuarantineSocketCloseTest, SharedQuarantineSocketClose,
    ::testing::Values(SharedQuarantineSocketCloseParam{"round-robin", true},
                      SharedQuarantineSocketCloseParam{"next-available", true},
                      SharedQuarantineSocketCloseParam{"first-available",
                                                       false}));

class SharedQuarantineSocketCloseWithFallback : public SocketCloseTest {};

TEST_F(SharedQuarantineSocketCloseWithFallback,
       cross_plugin_socket_close_with_fallback) {
  SCOPED_TRACE("// launch cluster with 2 nodes, 1 RW/1 RO");
  ASSERT_NO_FATAL_FAILURE(setup_cluster(2, "metadata_dynamic_nodes_v2_gr.js"));

  const auto bind_port_r1 = port_pool_.get_next_available();
  const auto bind_port_r2 = port_pool_.get_next_available();
  const auto bind_port_r3 = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(ClusterType::GR_V2);
  std::string routing_section = get_metadata_cache_routing_section(
      bind_port_r1, std::nullopt, "PRIMARY", "round-robin", "r1");
  routing_section += get_metadata_cache_routing_section(
      bind_port_r2, std::nullopt, "SECONDARY", "round-robin-with-fallback",
      "r2");
  routing_section += get_static_routing_section(bind_port_r3, std::nullopt,
                                                {node_ports[1]}, "round-robin");

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  launch_router(metadata_cache_section, routing_section, EXIT_SUCCESS);

  SCOPED_TRACE("// kill the RO server");
  EXPECT_NO_THROW(cluster_nodes[1]->send_clean_shutdown_event());
  EXPECT_NO_THROW(cluster_nodes[1]->wait_for_exit());

  SCOPED_TRACE(
      "// establishing a connection to static routing plugin will add the node "
      "to a quarantine");
  ASSERT_ANY_THROW(
      try_connection("127.0.0.1", bind_port_r3, router_user, router_password));
  SCOPED_TRACE("// static routing plugin has closed the socket");
  EXPECT_TRUE(wait_for_port_unused(bind_port_r3, 120s));

  SCOPED_TRACE("// fallback is possible, do not close the RO socket");
  EXPECT_FALSE(wait_for_port_unused(bind_port_r2, 1s));
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
