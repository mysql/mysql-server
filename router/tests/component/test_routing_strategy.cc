/*
  Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "config_builder.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysqlrouter/mysql_session.h"
#include "rest_metadata_client.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_test_helpers.h"
#include "tcp_port_pool.h"

/**
 * assert std::error_code has no 'error'
 */
#define ASSERT_NO_ERROR(expr) \
  ASSERT_THAT(expr, ::testing::Eq(std::error_code{})) << expr.message()

using mysqlrouter::MySQLSession;
using namespace std::chrono_literals;

static const std::string kRestApiUsername("someuser");
static const std::string kRestApiPassword("somepass");

class RouterRoutingStrategyTest : public RouterComponentTest {
 protected:
  void SetUp() override {
    RouterComponentTest::SetUp();

    // Valgrind needs way more time
    if (getenv("WITH_VALGRIND")) {
      wait_for_cache_ready_timeout = 5000ms;
      wait_for_process_exit_timeout = 20000ms;
      wait_for_static_ready_timeout = 1000ms;
    }
  }

  std::string get_metadata_cache_section(const uint16_t metadata_server_port,
                                         const uint16_t ttl = 300) const {
    return mysql_harness::ConfigBuilder::build_section(
        "metadata_cache:test",
        {{"router_id", "1"},
         {"bootstrap_server_addresses",
          "mysql://localhost:" + std::to_string(metadata_server_port)},
         {"user", "mysql_router1_user"},
         {"metadata_cluster", "test"},
         {"ttl", std::to_string(ttl)}});
  }

  std::string get_static_routing_section(
      unsigned router_port, const std::vector<uint16_t> &destinations,
      const std::string &strategy, const std::string &mode = "",
      const std::string &name = "test_default") const {
    std::string dest;
    for (size_t i = 0; i < destinations.size(); ++i) {
      dest += "127.0.0.1:" + std::to_string(destinations[i]);
      if (i != destinations.size() - 1) {
        dest += ",";
      }
    }

    std::vector<std::pair<std::string, std::string>> options{
        {"bind_port", std::to_string(router_port)},
        {"destinations", dest},
        {"protocol", "classic"}};

    if (!strategy.empty()) options.emplace_back("routing_strategy", strategy);
    if (!mode.empty()) options.emplace_back("mode", mode);

    return mysql_harness::ConfigBuilder::build_section("routing:" + name,
                                                       options);
  }

  // for error scenarios allow empty values
  std::string get_static_routing_section_error(
      unsigned router_port, const std::vector<unsigned> &destinations,
      const std::string &strategy, const std::string &mode) const {
    std::string dest;
    for (size_t i = 0; i < destinations.size(); ++i) {
      dest += "localhost:" + std::to_string(destinations[i]);
      if (i != destinations.size() - 1) {
        dest += ",";
      }
    }

    return mysql_harness::ConfigBuilder::build_section(
        "routing:test_default", {{"bind_port", std::to_string(router_port)},
                                 {"destinations", dest},
                                 {"protocol", "classic"},
                                 {"routing_strategy", strategy},
                                 {"mode", mode}});
  }

  std::string get_metadata_cache_routing_section(
      unsigned router_port, const std::string &role,
      const std::string &strategy, const std::string &mode = "",
      const std::string &name = "test_default",
      const std::optional<std::chrono::seconds>
          unreachable_destination_refresh_interval = std::nullopt) const {
    std::vector<std::pair<std::string, std::string>> options{
        {"bind_port", std::to_string(router_port)},
        {"destinations", "metadata-cache://test/default?role=" + role},
        {"protocol", "classic"}};

    if (!strategy.empty()) options.emplace_back("routing_strategy", strategy);
    if (!mode.empty()) options.emplace_back("mode", mode);
    if (unreachable_destination_refresh_interval)
      options.emplace_back(
          "unreachable_destination_refresh_interval",
          std::to_string((*unreachable_destination_refresh_interval).count()));

    return mysql_harness::ConfigBuilder::build_section("routing:" + name,
                                                       options);
  }

  std::string get_monitoring_section(unsigned monitoring_port,
                                     const std::string &config_dir) {
    std::string passwd_filename =
        mysql_harness::Path(config_dir).join("users").str();

    {
      ProcessWrapper::OutputResponder responder{
          [](const std::string &line) -> std::string {
            if (line == "Please enter password: ")
              return std::string(kRestApiPassword) + "\n";

            return "";
          }};

      auto &cmd = launch_command(
          get_origin().join("mysqlrouter_passwd").str(),
          {"set", passwd_filename, kRestApiUsername}, EXIT_SUCCESS, true,
          std::vector<std::pair<std::string, std::string>>{}, responder);
      check_exit_code(cmd, EXIT_SUCCESS);
    }

    return mysql_harness::ConfigBuilder::build_section("rest_api", {}) +
           mysql_harness::ConfigBuilder::build_section(
               "rest_metadata_cache", {{"require_realm", "somerealm"}}) +
           mysql_harness::ConfigBuilder::build_section(
               "http_auth_realm:somerealm", {{"backend", "somebackend"},
                                             {"method", "basic"},
                                             {"name", "somerealm"}}) +
           mysql_harness::ConfigBuilder::build_section(
               "http_auth_backend:somebackend",
               {{"backend", "file"}, {"filename", passwd_filename}}) +
           mysql_harness::ConfigBuilder::build_section(
               "http_server", {{"port", std::to_string(monitoring_port)}});
  }

  std::string get_destination_status_section(
      std::optional<std::chrono::seconds> quarantine_interval,
      std::optional<uint32_t> quarantine_threshold) {
    std::vector<std::pair<std::string, std::string>> options{};

    if (quarantine_interval)
      options.emplace_back("error_quarantine_interval",
                           std::to_string((*quarantine_interval).count()));
    if (quarantine_threshold)
      options.emplace_back("error_quarantine_threshold",
                           std::to_string(*quarantine_threshold));

    if (options.empty()) {
      return "";
    } else {
      return mysql_harness::ConfigBuilder::build_section("destination_status",
                                                         options);
    }
  }

  // need to return void to be able to use ASSERT_ macros
  void connect_client_and_query_port(unsigned router_port,
                                     std::string &out_port,
                                     bool should_fail = false) {
    MySQLSession client;

    if (should_fail) {
      EXPECT_THROW_LIKE(client.connect("127.0.0.1", router_port, "username",
                                       "password", "", ""),
                        std::exception, "Error connecting to MySQL server");
      out_port = "";
      return;
    } else {
      ASSERT_NO_THROW(client.connect("127.0.0.1", router_port, "username",
                                     "password", "", ""));
    }

    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    ASSERT_NE(nullptr, result.get());
    ASSERT_EQ(1u, result->size());
    out_port = std::string((*result)[0]);
  }

  ProcessWrapper &launch_cluster_node(unsigned cluster_port,
                                      const std::string &data_dir) {
    const std::string js_file = Path(data_dir).join("my_port.js").str();
    auto &cluster_node = ProcessManager::launch_mysql_server_mock(
        js_file, cluster_port, EXIT_SUCCESS, false);

    return cluster_node;
  }

  ProcessWrapper &launch_standalone_server(unsigned server_port,
                                           const std::string &data_dir) {
    // it' does the same thing, just an alias  for less confusion
    return launch_cluster_node(server_port, data_dir);
  }

  ProcessWrapper &launch_router_static(const std::string &conf_dir,
                                       const std::string &routing_section,
                                       bool expect_error = false) {
    auto def_section = get_DEFAULT_defaults();

    // launch the router with the static routing configuration
    const std::string conf_file =
        create_config_file(conf_dir, routing_section, &def_section);
    const int expected_exit_code = expect_error ? EXIT_FAILURE : EXIT_SUCCESS;
    auto &router =
        ProcessManager::launch_router({"-c", conf_file}, expected_exit_code,
                                      true, false, expect_error ? -1s : 5s);

    return router;
  }

  ProcessWrapper &launch_router(const std::string &temp_test_dir,
                                const std::string &metadata_cache_section,
                                const std::string &routing_section) {
    auto default_section = get_DEFAULT_defaults();
    init_keyring(default_section, temp_test_dir);

    // launch the router with metadata-cache configuration
    const std::string conf_file = create_config_file(
        temp_test_dir, metadata_cache_section + routing_section,
        &default_section);
    auto &router = ProcessManager::launch_router({"-c", conf_file},
                                                 EXIT_SUCCESS, true, false);

    return router;
  }

  void kill_server(ProcessWrapper *server) {
    EXPECT_NO_THROW(server->kill());
    EXPECT_EQ(server->wait_for_exit(), 0);
  }

  std::chrono::milliseconds wait_for_cache_ready_timeout{1000};
  std::chrono::milliseconds wait_for_static_ready_timeout{100};
  std::chrono::milliseconds wait_for_process_exit_timeout{10000};
};

struct MetadataCacheTestParams {
  std::string tracefile;
  std::string role;
  std::string routing_strategy;
  std::string mode;

  // consecutive nodes ids that we expect to be connected to
  std::vector<unsigned> expected_node_connections;
  bool round_robin;

  MetadataCacheTestParams(const std::string &tracefile_,
                          const std::string &role_,
                          const std::string &routing_strategy_,
                          const std::string &mode_,
                          std::vector<unsigned> expected_node_connections_,
                          bool round_robin_ = false)
      : tracefile(tracefile_),
        role(role_),
        routing_strategy(routing_strategy_),
        mode(mode_),
        expected_node_connections(expected_node_connections_),
        round_robin(round_robin_) {}
};

::std::ostream &operator<<(::std::ostream &os,
                           const MetadataCacheTestParams &mcp) {
  return os << "role=" << mcp.role
            << ", routing_strtegy=" << mcp.routing_strategy
            << ", mode=" << mcp.mode;
}

class RouterRoutingStrategyMetadataCache
    : public RouterRoutingStrategyTest,
      public ::testing::WithParamInterface<MetadataCacheTestParams> {
 protected:
  void SetUp() override { RouterRoutingStrategyTest::SetUp(); }
};

////////////////////////////////////////
/// MATADATA-CACHE ROUTING TESTS
////////////////////////////////////////

TEST_P(RouterRoutingStrategyMetadataCache, MetadataCacheRoutingStrategy) {
  auto test_params = GetParam();
  const std::string tracefile = test_params.tracefile;

  TempDirectory temp_test_dir;

  const std::vector<uint16_t> cluster_nodes_ports{
      port_pool_.get_next_available(),  // first is PRIMARY
      port_pool_.get_next_available(), port_pool_.get_next_available(),
      port_pool_.get_next_available()};
  const std::vector<uint16_t> cluster_nodes_http_ports{
      port_pool_.get_next_available(),  // first is PRIMARY
      port_pool_.get_next_available(), port_pool_.get_next_available(),
      port_pool_.get_next_available()};

  std::vector<ProcessWrapper *> cluster_nodes;

  // launch the primary node working also as metadata server
  const auto json_file = get_data_dir().join(tracefile).str();
  const auto http_port = cluster_nodes_http_ports[0];
  auto &primary_node = launch_mysql_server_mock(
      json_file, cluster_nodes_ports[0], EXIT_SUCCESS, false, http_port);
  ASSERT_NO_FATAL_FAILURE(
      check_port_ready(primary_node, cluster_nodes_ports[0]));
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
  set_mock_metadata(http_port, "", cluster_nodes_ports);
  cluster_nodes.emplace_back(&primary_node);

  // launch the secondary cluster nodes
  for (unsigned port = 1; port < cluster_nodes_ports.size(); ++port) {
    auto &secondary_node =
        launch_cluster_node(cluster_nodes_ports[port], get_data_dir().str());
    cluster_nodes.emplace_back(&secondary_node);
    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(secondary_node, cluster_nodes_ports[port]));
  }

  // launch the router with metadata-cache configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(cluster_nodes_ports[0]);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, test_params.role, test_params.routing_strategy,
      test_params.mode);
  const auto monitoring_port = port_pool_.get_next_available();
  const std::string monitoring_section =
      get_monitoring_section(monitoring_port, temp_test_dir.name());

  auto &router = launch_router(temp_test_dir.name(),
                               metadata_cache_section + monitoring_section,
                               routing_section);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port));

  // give the router a chance to initialise metadata-cache module
  // there is currently now easy way to check that
  SCOPED_TRACE("// waiting " +
               std::to_string(wait_for_cache_ready_timeout.count()) +
               "ms until metadata is initialized");
  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client("127.0.0.1", monitoring_port,
                                          kRestApiUsername, kRestApiPassword);
  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_ready(
      wait_for_cache_ready_timeout, metadata_status));

  if (!test_params.round_robin) {
    // check if the server nodes are being used in the expected order
    for (auto expected_node_id : test_params.expected_node_connections) {
      make_new_connection_ok(router_port,
                             cluster_nodes_ports[expected_node_id]);
    }
  } else {
    // for round-robin we can't be sure which server will be the starting one
    // on Solaris wait_for_port_ready() causes the router to switch to the next
    // server while on other OSes it does not. We check it the round robin is
    // done on provided set of ids.
    const auto &expected_nodes = test_params.expected_node_connections;
    std::string node_port;
    size_t first_port_id{0};
    for (size_t i = 0; i < expected_nodes.size() + 1;
         ++i) {  // + 1 to check that after
                 // full round it starts from beginning
      ASSERT_NO_FATAL_FAILURE(
          connect_client_and_query_port(router_port, node_port));
      if (i == 0) {  // first-connection
        const auto &real_port_iter =
            std::find(cluster_nodes_ports.begin(), cluster_nodes_ports.end(),
                      static_cast<uint16_t>(std::atoi(node_port.c_str())));
        ASSERT_NE(real_port_iter, cluster_nodes_ports.end());
        auto port_id_ = real_port_iter - std::begin(cluster_nodes_ports);

        EXPECT_TRUE(std::find(expected_nodes.begin(), expected_nodes.end(),
                              port_id_) != expected_nodes.end());
        first_port_id =
            std::find(expected_nodes.begin(), expected_nodes.end(), port_id_) -
            expected_nodes.begin();
      } else {
        const auto current_idx = (first_port_id + i) % expected_nodes.size();
        const auto expected_node_id = expected_nodes[current_idx];
        EXPECT_EQ(std::to_string(cluster_nodes_ports[expected_node_id]),
                  node_port);
      }
    }
  }

  ASSERT_THAT(router.kill(), testing::Eq(0));
}

INSTANTIATE_TEST_SUITE_P(
    MetadataCacheRoutingStrategy, RouterRoutingStrategyMetadataCache,
    // node_id=0 is PRIARY, node_id=1..3 are SECONDARY
    ::testing::Values(
        // test round-robin on SECONDARY servers
        // we expect 1->2->3->1 for 4 consecutive connections
        MetadataCacheTestParams("metadata_3_secondaries_pass_v2_gr.js",
                                "SECONDARY", "round-robin", "", {1, 2, 3},
                                /*round-robin=*/true),

        // the same for old metadata
        MetadataCacheTestParams("metadata_3_secondaries_pass.js", "SECONDARY",
                                "round-robin", "", {1, 2, 3},
                                /*round-robin=*/true),

        // test first-available on SECONDARY servers
        // we expect 1->1->1 for 3 consecutive connections
        MetadataCacheTestParams("metadata_3_secondaries_pass_v2_gr.js",
                                "SECONDARY", "first-available", "", {1, 1, 1}),

        // the same for old metadata
        MetadataCacheTestParams("metadata_3_secondaries_pass.js", "SECONDARY",
                                "first-available", "", {1, 1, 1}),

        // *basic* test round-robin-with-fallback
        // we expect 1->2->3->1 for 4 consecutive connections
        // as there are SECONDARY servers available (PRIMARY id=0 should not be
        // used)
        MetadataCacheTestParams("metadata_3_secondaries_pass_v2_gr.js",
                                "SECONDARY", "round-robin-with-fallback", "",
                                {1, 2, 3},
                                /*round-robin=*/true),

        // the same for old metadata
        MetadataCacheTestParams("metadata_3_secondaries_pass.js", "SECONDARY",
                                "round-robin-with-fallback", "", {1, 2, 3},
                                /*round-robin=*/true),

        // test round-robin on PRIMARY_AND_SECONDARY
        // we expect the primary to participate in the round-robin from the
        // beginning we expect 0->1->2->3->0 for 5 consecutive connections
        MetadataCacheTestParams("metadata_3_secondaries_pass_v2_gr.js",
                                "PRIMARY_AND_SECONDARY", "round-robin", "",
                                {0, 1, 2, 3},
                                /*round-robin=*/true),

        // the same for old metadata
        MetadataCacheTestParams("metadata_3_secondaries_pass.js",
                                "PRIMARY_AND_SECONDARY", "round-robin", "",
                                {0, 1, 2, 3},
                                /*round-robin=*/true),

        // test round-robin with allow-primary-reads=yes
        // this should work similar to PRIMARY_AND_SECONDARY
        // we expect 0->1->2->3->0 for 5 consecutive connections
        MetadataCacheTestParams("metadata_3_secondaries_pass_v2_gr.js",
                                "SECONDARY&allow_primary_reads=yes", "",
                                "read-only", {0, 1, 2, 3},
                                /*round-robin=*/true),

        // the same for old metadata
        MetadataCacheTestParams("metadata_3_secondaries_pass.js",
                                "SECONDARY&allow_primary_reads=yes", "",
                                "read-only", {0, 1, 2, 3},
                                /*round-robin=*/true),

        // test first-available on PRIMARY
        // we expect 0->0->0 for 2 consecutive connections
        MetadataCacheTestParams("metadata_3_secondaries_pass_v2_gr.js",
                                "PRIMARY", "first-available", "", {0, 0}),

        // the same for old metadata
        MetadataCacheTestParams("metadata_3_secondaries_pass.js", "PRIMARY",
                                "first-available", "", {0, 0}),

        // test round-robin on PRIMARY
        // there is single primary so we expect 0->0->0 for 2 consecutive
        // connections
        MetadataCacheTestParams("metadata_3_secondaries_pass_v2_gr.js",
                                "PRIMARY", "round-robin", "", {0, 0}),

        // the same for old metadata
        MetadataCacheTestParams("metadata_3_secondaries_pass.js", "PRIMARY",
                                "round-robin", "", {0, 0})));

////////////////////////////////////////
/// STATIC ROUTING TESTS
////////////////////////////////////////

class RouterRoutingStrategyTestRoundRobin
    : public RouterRoutingStrategyTest,
      // r. strategy, mode
      public ::testing::WithParamInterface<
          std::pair<std::string, std::string>> {
 protected:
  void SetUp() override { RouterRoutingStrategyTest::SetUp(); }
};

// WL#13327: TS_R6_1, TS_R6_2
TEST_P(RouterRoutingStrategyTestRoundRobin, StaticRoutingStrategyRoundRobin) {
  TempDirectory temp_test_dir;
  TempDirectory conf_dir("conf");

  const std::vector<uint16_t> server_ports{port_pool_.get_next_available(),
                                           port_pool_.get_next_available(),
                                           port_pool_.get_next_available()};

  // launch the standalone servers
  std::vector<ProcessWrapper *> server_instances;
  for (auto &server_port : server_ports) {
    auto &secondary_node =
        launch_standalone_server(server_port, get_data_dir().str());
    ASSERT_NO_FATAL_FAILURE(check_port_ready(secondary_node, server_port));
    server_instances.emplace_back(&secondary_node);
  }

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();

  const auto routing_strategy = GetParam().first;
  const auto mode = GetParam().second;
  const std::string routing_section = get_static_routing_section(
      router_port, server_ports, routing_strategy, mode);
  auto &router = launch_router_static(conf_dir.name(), routing_section);
  EXPECT_TRUE(wait_for_port_used(router_port));

  // expect consecutive connections to be done in round-robin fashion
  make_new_connection_ok(router_port, server_ports[0]);
  make_new_connection_ok(router_port, server_ports[1]);
  make_new_connection_ok(router_port, server_ports[2]);
  make_new_connection_ok(router_port, server_ports[0]);

  std::string node_port;

  SCOPED_TRACE("// kill 1st and 2nd server");
  for (int i = 0; i < 2; i++) {
    kill_server(server_instances[i]);
    EXPECT_TRUE(wait_for_port_unused(server_ports[i], 200s));
    // Go through all destinations to trigger the quarantine
    for (std::size_t i = 0; i < server_ports.size(); ++i) {
      connect_client_and_query_port(router_port, node_port);
    }
    EXPECT_TRUE(wait_log_contains(router,
                                  std::string{"add destination '.*:"} +
                                      std::to_string(server_ports[i]) +
                                      "' to quarantine",
                                  2s));
    EXPECT_FALSE(is_port_bindable(router_port));
  }

  SCOPED_TRACE("// kill 3rd server");
  kill_server(server_instances[2]);
  EXPECT_TRUE(wait_for_port_unused(server_ports[2], 200s));
  connect_client_and_query_port(router_port, node_port, /*should_fail*/ true);
  SCOPED_TRACE("// third node is added to quarantine");
  EXPECT_TRUE(wait_log_contains(router,
                                std::string{"add destination '.*:"} +
                                    std::to_string(server_ports[2]) +
                                    "' to quarantine",
                                2s));

  SCOPED_TRACE("// nodes 1 and 2 are still unreachable and quarantined");
  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(
        wait_log_contains(router,
                          std::string{"skip quarantined destination '.*:"} +
                              std::to_string(server_ports[i]) + "'",
                          2s));
  }

  // socket can end up in a TIME_WAIT state so it could take a while for it
  // to be available again.
  EXPECT_TRUE(wait_for_port_unused(router_port, 200s));

  SCOPED_TRACE("// bring back 1st server");
  server_instances.emplace_back(
      &launch_standalone_server(server_ports[0], get_data_dir().str()));
  ASSERT_NO_FATAL_FAILURE(check_port_ready(
      *server_instances[server_instances.size() - 1], server_ports[0]));
  EXPECT_TRUE(wait_for_port_ready(router_port, 10s));
  SCOPED_TRACE(
      "// 1st node is reachable and should be removed from quarantine");
  EXPECT_TRUE(wait_log_contains(
      router,
      "Destination candidate '.*:" + std::to_string(server_ports[0]) +
          "' is available, remove it from quarantine",
      5s));

  SCOPED_TRACE("// we should now succesfully connect to server on port " +
               std::to_string(server_ports[0]));
  connect_client_and_query_port(router_port, node_port);
}

// We expect round robin for routing-strategy=round-robin and as default for
// read-only
INSTANTIATE_TEST_SUITE_P(
    StaticRoutingStrategyRoundRobin, RouterRoutingStrategyTestRoundRobin,
    ::testing::Values(
        std::make_pair(std::string("round-robin"), std::string("")),
        std::make_pair(std::string("round-robin"), std::string("read-only")),
        std::make_pair(std::string("round-robin"), std::string("read-write")),
        std::make_pair(std::string(""), std::string("read-only"))));

class RouterRoutingStrategyTestFirstAvailable
    : public RouterRoutingStrategyTest,
      // r. strategy, mode
      public ::testing::WithParamInterface<
          std::pair<std::string, std::string>> {
 protected:
  void SetUp() override { RouterRoutingStrategyTest::SetUp(); }
};

// WL#13327: TS_R6_3, TS_R6_4
TEST_P(RouterRoutingStrategyTestFirstAvailable,
       StaticRoutingStrategyFirstAvailable) {
  TempDirectory temp_test_dir;
  TempDirectory conf_dir("conf");

  const std::vector<uint16_t> server_ports{port_pool_.get_next_available(),
                                           port_pool_.get_next_available(),
                                           port_pool_.get_next_available()};

  // launch the standalone servers
  std::vector<ProcessWrapper *> server_instances;
  for (auto &server_port : server_ports) {
    auto &secondary_node =
        launch_standalone_server(server_port, get_data_dir().str());
    ASSERT_NO_FATAL_FAILURE(check_port_ready(secondary_node, server_port));

    server_instances.emplace_back(&secondary_node);
  }

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();

  const auto routing_strategy = GetParam().first;
  const auto mode = GetParam().second;
  const std::string routing_section = get_static_routing_section(
      router_port, server_ports, routing_strategy, mode);
  auto &router = launch_router_static(conf_dir.name(), routing_section);
  EXPECT_TRUE(wait_for_port_used(router_port));

  // expect consecutive connections to be done in first-available fashion
  make_new_connection_ok(router_port, server_ports[0]);
  make_new_connection_ok(router_port, server_ports[0]);

  SCOPED_TRACE("// 'kill' server 1 and 2, expect moving to server 3");
  kill_server(server_instances[0]);
  EXPECT_TRUE(wait_for_port_unused(server_ports[0], 200s));
  kill_server(server_instances[1]);
  EXPECT_TRUE(wait_for_port_unused(server_ports[1], 200s));
  SCOPED_TRACE("// now we should connect to 3rd server");
  make_new_connection_ok(router_port, server_ports[2]);
  SCOPED_TRACE("// nodes 1 and two should be quarantined at this point");
  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(wait_log_contains(router,
                                  std::string{"add destination '.*:"} +
                                      std::to_string(server_ports[i]) +
                                      "' to quarantine",
                                  2s));
  }

  SCOPED_TRACE("// router listening port is still open");
  EXPECT_FALSE(is_port_bindable(router_port));

  SCOPED_TRACE("// kill also 3rd server");
  kill_server(server_instances[2]);
  EXPECT_TRUE(wait_for_port_unused(server_ports[2], 200s));
  SCOPED_TRACE("// expect connection failure");
  verify_new_connection_fails(router_port);

  SCOPED_TRACE("// third node is added to quarantine");
  EXPECT_TRUE(wait_log_contains(router,
                                std::string{"add destination '.*:"} +
                                    std::to_string(server_ports[2]) +
                                    "' to quarantine",
                                2s));

  SCOPED_TRACE("// nodes 1 and 2 are still unreachable and quarantined");
  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(
        wait_log_contains(router,
                          std::string{"skip quarantined destination '.*:"} +
                              std::to_string(server_ports[i]) + "'",
                          2s));
  }

  SCOPED_TRACE(
      "// in case of first-available policy we never close the listening "
      "ports");
  EXPECT_FALSE(is_port_bindable(router_port));

  SCOPED_TRACE("// bring back 1st server on port " +
               std::to_string(server_ports[0]));
  server_instances.emplace_back(
      &launch_standalone_server(server_ports[0], get_data_dir().str()));
  ASSERT_NO_FATAL_FAILURE(check_port_ready(
      *server_instances[server_instances.size() - 1], server_ports[0]));
  EXPECT_TRUE(wait_for_port_used(router_port, 200s));

  SCOPED_TRACE(
      "// 1st node is reachable and should be removed from quarantine");
  EXPECT_TRUE(wait_log_contains(
      router,
      "Destination candidate '.*:" + std::to_string(server_ports[0]) +
          "' is available, remove it from quarantine",
      5s));

  SCOPED_TRACE("// we should now succesfully connect to server on port " +
               std::to_string(server_ports[0]));
  make_new_connection_ok(router_port, server_ports[0]);
  EXPECT_FALSE(is_port_bindable(router_port));
}

// We expect first-available for routing-strategy=first-available and as default
// for read-write
INSTANTIATE_TEST_SUITE_P(
    StaticRoutingStrategyFirstAvailable,
    RouterRoutingStrategyTestFirstAvailable,
    ::testing::Values(
        std::make_pair(std::string("first-available"), std::string("")),
        std::make_pair(std::string("first-available"),
                       std::string("read-write")),
        std::make_pair(std::string("first-available"),
                       std::string("read-only")),
        std::make_pair(std::string(""), std::string("read-write"))));

// for non-param tests
class RouterRoutingStrategyStatic : public RouterRoutingStrategyTest {};

// WL#13327: TS_R6_5, TS_R6_6
TEST_F(RouterRoutingStrategyStatic, StaticRoutingStrategyNextAvailable) {
  TempDirectory temp_test_dir;
  TempDirectory conf_dir("conf");

  const std::vector<uint16_t> server_ports{port_pool_.get_next_available(),
                                           port_pool_.get_next_available(),
                                           port_pool_.get_next_available()};

  // launch the standalone servers
  std::vector<ProcessWrapper *> server_instances;
  for (auto &server_port : server_ports) {
    auto &secondary_node =
        launch_standalone_server(server_port, get_data_dir().str());
    ASSERT_NO_FATAL_FAILURE(check_port_ready(secondary_node, server_port));
    server_instances.emplace_back(&secondary_node);
  }

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string routing_section =
      get_static_routing_section(router_port, server_ports, "next-available");
  auto &router = launch_router_static(conf_dir.name(), routing_section);
  EXPECT_TRUE(wait_for_port_used(router_port));

  // expect consecutive connections to be done in first-available fashion
  make_new_connection_ok(router_port, server_ports[0]);
  make_new_connection_ok(router_port, server_ports[0]);
  EXPECT_FALSE(is_port_bindable(router_port));

  SCOPED_TRACE(
      "// 'kill' server 1 and 2, expect connection to server 3 after that");
  kill_server(server_instances[0]);
  kill_server(server_instances[1]);
  SCOPED_TRACE("// now we should connect to 3rd server");
  make_new_connection_ok(router_port, server_ports[2]);
  SCOPED_TRACE("// check if 1st and 2nd node are quarantined");
  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(wait_log_contains(router,
                                  std::string{"add destination '.*:"} +
                                      std::to_string(server_ports[i]) +
                                      "' to quarantine",
                                  2s));
  }
  EXPECT_FALSE(is_port_bindable(router_port));

  SCOPED_TRACE("// kill also 3rd server");
  kill_server(server_instances[2]);
  SCOPED_TRACE("// expect connection failure");
  verify_new_connection_fails(router_port);
  EXPECT_TRUE(wait_log_contains(router,
                                std::string{"add destination '.*:"} +
                                    std::to_string(server_ports[2]) +
                                    "' to quarantine",
                                2s));
  // socket can end up in a TIME_WAIT state so it could take a while for it
  // to be available again.
  EXPECT_TRUE(wait_for_port_unused(router_port, 200s));

  SCOPED_TRACE("// bring back 1st server");
  server_instances.emplace_back(
      &launch_standalone_server(server_ports[0], get_data_dir().str()));
  ASSERT_NO_FATAL_FAILURE(check_port_ready(
      *server_instances[server_instances.size() - 1], server_ports[0]));
  SCOPED_TRACE(
      "// 1st node is reachable and should be removed from quarantine");
  EXPECT_TRUE(wait_log_contains(
      router,
      "Destination candidate '.*:" + std::to_string(server_ports[0]) +
          "' is available, remove it from quarantine",
      5s));
  SCOPED_TRACE(
      "// we should NOT connect to this server (in next-available we NEVER go "
      "back)");
  verify_new_connection_fails(router_port);
  // socket can end up in a TIME_WAIT state so it could take a while for it
  // to be available again.
  EXPECT_TRUE(wait_for_port_unused(router_port, 200s));
}

// configuration error scenarios

TEST_F(RouterRoutingStrategyStatic, InvalidStrategyName) {
  TempDirectory temp_test_dir;
  TempDirectory conf_dir("conf");

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string routing_section = get_static_routing_section_error(
      router_port, {1, 2}, "round-robin-with-fallback", "read-only");
  auto &router = launch_router_static(conf_dir.name(), routing_section,
                                      /*expect_error=*/true);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_TRUE(
      wait_log_contains(router,
                        "Configuration error: option routing_strategy in "
                        "\\[routing:test_default\\] is invalid; "
                        "valid are first-available, next-available, and "
                        "round-robin \\(was 'round-robin-with-fallback'",
                        500ms));
}

TEST_F(RouterRoutingStrategyStatic, InvalidMode) {
  TempDirectory conf_dir("conf");

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string routing_section = get_static_routing_section_error(
      router_port, {1, 2}, "invalid", "read-only");
  auto &router = launch_router_static(conf_dir.name(), routing_section,
                                      /*expect_error=*/true);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_TRUE(wait_log_contains(
      router,
      "option routing_strategy in \\[routing:test_default\\] is invalid; valid "
      "are "
      "first-available, next-available, and round-robin \\(was 'invalid'\\)",
      500ms));
}

TEST_F(RouterRoutingStrategyStatic, BothStrategyAndModeMissing) {
  TempDirectory conf_dir("conf");

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string routing_section =
      get_static_routing_section(router_port, {1, 2}, "");
  auto &router = launch_router_static(conf_dir.name(), routing_section,
                                      /*expect_error=*/true);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_TRUE(
      wait_log_contains(router,
                        "Configuration error: option routing_strategy in "
                        "\\[routing:test_default\\] is required",
                        500ms));
}

TEST_F(RouterRoutingStrategyStatic, RoutingStrategyEmptyValue) {
  TempDirectory conf_dir("conf");

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string routing_section =
      get_static_routing_section_error(router_port, {1, 2}, "", "read-only");
  auto &router = launch_router_static(conf_dir.name(), routing_section,
                                      /*expect_error=*/true);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_TRUE(
      wait_log_contains(router,
                        "Configuration error: option routing_strategy in "
                        "\\[routing:test_default\\] needs a value",
                        500ms));
}

TEST_F(RouterRoutingStrategyStatic, ModeEmptyValue) {
  TempDirectory conf_dir("conf");

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string routing_section = get_static_routing_section_error(
      router_port, {1, 2}, "first-available", "");
  auto &router = launch_router_static(conf_dir.name(), routing_section,
                                      /*expect_error=*/true);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_TRUE(wait_log_contains(router,
                                "Configuration error: option mode in "
                                "\\[routing:test_default\\] needs a value",
                                500ms));
}

/**
 * @test WL14663:TS_R1_1
 */
TEST_F(RouterRoutingStrategyStatic, SharedQuarantine) {
  TempDirectory conf_dir("conf");

  const std::vector<uint16_t> server_ports{
      port_pool_.get_next_available(), port_pool_.get_next_available(),
      port_pool_.get_next_available(), port_pool_.get_next_available(),
      port_pool_.get_next_available()};
  // launch the standalone servers
  std::vector<ProcessWrapper *> server_instances;
  for (auto &server_port : server_ports) {
    auto &secondary_node =
        launch_standalone_server(server_port, get_data_dir().str());
    ASSERT_NO_FATAL_FAILURE(check_port_ready(secondary_node, server_port));
    server_instances.emplace_back(&secondary_node);
  }

  const std::vector<uint16_t> router_ports{port_pool_.get_next_available(),
                                           port_pool_.get_next_available()};
  std::string routing_section =
      get_static_routing_section(
          router_ports[0],
          {server_ports[0], server_ports[1], server_ports[0], server_ports[2]},
          "first-available", "", "r1") +
      get_static_routing_section(
          router_ports[1], {server_ports[3], server_ports[1], server_ports[4]},
          "round-robin", "", "r2");

  SCOPED_TRACE("// launch the router with static routing");
  auto &router = launch_router_static(conf_dir.name(), routing_section);
  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(wait_for_port_used(router_ports[i]));
  }

  SCOPED_TRACE("// kill 1st server");
  kill_server(server_instances[0]);

  SCOPED_TRACE("// 1st server is unreachable and quarantined");
  make_new_connection_ok(router_ports[0], server_ports[1]);
  EXPECT_TRUE(wait_log_contains(router,
                                "add destination '.*" +
                                    std::to_string(server_ports[0]) +
                                    "' to quarantine",
                                500ms));

  SCOPED_TRACE(
      "// kill 2nd server so that first-available would have to switch to a "
      "next node");
  kill_server(server_instances[1]);
  make_new_connection_ok(router_ports[0], server_ports[2]);
  EXPECT_TRUE(wait_log_contains(router,
                                "add destination '.*" +
                                    std::to_string(server_ports[1]) +
                                    "' to quarantine",
                                500ms));

  SCOPED_TRACE("// kill 4th server");
  kill_server(server_instances[3]);
  SCOPED_TRACE("// use r2 routing");
  make_new_connection_ok(router_ports[1], server_ports[4]);
  EXPECT_TRUE(wait_log_contains(router,
                                "add destination '.*" +
                                    std::to_string(server_ports[3]) +
                                    "' to quarantine",
                                500ms));
  SCOPED_TRACE(
      "// information that this destination is unreachable is from routing r1");
  EXPECT_TRUE(wait_log_contains(router,
                                "skip quarantined destination '.*" +
                                    std::to_string(server_ports[1]) + "'",
                                500ms));
  SCOPED_TRACE("// bring back 2nd server to life");
  server_instances[0] =
      &launch_cluster_node(server_ports[1], get_data_dir().str());
  EXPECT_TRUE(wait_log_contains(router,
                                "Destination candidate '.*" +
                                    std::to_string(server_ports[1]) +
                                    "' is available, remove it from quarantine",
                                5s));
  SCOPED_TRACE("// 2nd server is available again");
  make_new_connection_ok(router_ports[1], server_ports[1]);
}

/**
 * @test WL14663:TS_R1_2
 */
TEST_F(RouterRoutingStrategyMetadataCache, SharedQuarantine) {
  TempDirectory temp_test_dir;

  const std::vector<uint16_t> cluster_nodes_ports{
      port_pool_.get_next_available(),  // first is PRIMARY
      port_pool_.get_next_available(), port_pool_.get_next_available(),
      port_pool_.get_next_available()};
  const auto http_port = port_pool_.get_next_available();

  std::vector<ProcessWrapper *> cluster_nodes;

  // launch the primary node working also as metadata server
  const auto json_file =
      get_data_dir().join("metadata_3_secondaries_pass_v2_gr.js").str();
  auto &primary_node = launch_mysql_server_mock(
      json_file, cluster_nodes_ports[0], EXIT_SUCCESS, false, http_port);
  ASSERT_NO_FATAL_FAILURE(
      check_port_ready(primary_node, cluster_nodes_ports[0]));
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
  set_mock_metadata(http_port, "", cluster_nodes_ports);
  cluster_nodes.emplace_back(&primary_node);

  // launch the secondary cluster nodes
  for (unsigned port = 1; port < cluster_nodes_ports.size(); ++port) {
    auto &secondary_node =
        launch_cluster_node(cluster_nodes_ports[port], get_data_dir().str());
    cluster_nodes.emplace_back(&secondary_node);
    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(secondary_node, cluster_nodes_ports[port]));
  }

  // launch the router with metadata-cache configuration
  const auto X_RW_bind_port = port_pool_.get_next_available();
  const auto X_RO_bind_port = port_pool_.get_next_available();
  const auto classic_RO_bind_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(cluster_nodes_ports[0]);
  const std::string routing_section =
      get_metadata_cache_routing_section(X_RW_bind_port, "PRIMARY",
                                         "first-available", "", "x_rw") +
      get_metadata_cache_routing_section(X_RO_bind_port, "SECONDARY",
                                         "round-robin", "", "x_ro") +
      get_metadata_cache_routing_section(classic_RO_bind_port, "SECONDARY",
                                         "round-robin", "", "c_ro");
  const auto monitoring_port = port_pool_.get_next_available();
  const std::string monitoring_section =
      get_monitoring_section(monitoring_port, temp_test_dir.name());

  auto &router = launch_router(temp_test_dir.name(),
                               metadata_cache_section + monitoring_section,
                               routing_section);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, X_RW_bind_port));

  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client("127.0.0.1", monitoring_port,
                                          kRestApiUsername, kRestApiPassword);
  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_ready(
      wait_for_cache_ready_timeout, metadata_status));

  SCOPED_TRACE("// make first RO node unavailable");
  cluster_nodes[1]->send_clean_shutdown_event();
  EXPECT_EQ(cluster_nodes[1]->wait_for_exit(), 0);
  make_new_connection_ok(X_RO_bind_port, cluster_nodes_ports[2]);
  EXPECT_TRUE(wait_log_contains(router,
                                "add destination '.*" +
                                    std::to_string(cluster_nodes_ports[1]) +
                                    "' to quarantine",
                                500ms));
  make_new_connection_ok(classic_RO_bind_port, cluster_nodes_ports[2]);
  EXPECT_TRUE(wait_log_contains(router,
                                "skip quarantined destination '.*" +
                                    std::to_string(cluster_nodes_ports[1]) +
                                    "'",
                                500ms));

  SCOPED_TRACE("// restore first RO node unavailable");
  cluster_nodes[1] =
      &launch_cluster_node(cluster_nodes_ports[1], get_data_dir().str());
  EXPECT_TRUE(wait_log_contains(router,
                                "Destination candidate '.*" +
                                    std::to_string(cluster_nodes_ports[1]) +
                                    "' is available, remove it from quarantine",
                                5s));

  // check that restored (first) RO node got back to the round-robin rotation
  std::vector<uint16_t> ports_used;
  for (size_t i = 0; i < 3; i++) {
    ports_used.push_back(make_new_connection_ok(classic_RO_bind_port));
  }

  EXPECT_THAT(ports_used,
              ::testing::Contains(::testing::Eq(cluster_nodes_ports[1])));

  ASSERT_THAT(router.kill(), testing::Eq(0));
}

class UnreachableDestinationRefreshIntervalOption
    : public RouterRoutingStrategyTest {};

struct QuarantineTestParam {
  std::optional<std::chrono::seconds> interval;
  std::optional<uint32_t> threshold;
  // old, deprecated option for interval
  std::optional<std::chrono::seconds> unreachable_destination_refresh_interval;
};

class UnreachableDestinationQuarantineOptions
    : public RouterRoutingStrategyTest,
      public ::testing::WithParamInterface<QuarantineTestParam> {};

/**
 * @test WL14663:TS_R2_2
 */
TEST_P(UnreachableDestinationQuarantineOptions, Test) {
  TempDirectory temp_test_dir;

  const std::vector<uint16_t> cluster_nodes_ports{
      port_pool_.get_next_available(),  // first is PRIMARY
      port_pool_.get_next_available(), port_pool_.get_next_available(),
      port_pool_.get_next_available()};
  const auto http_port = port_pool_.get_next_available();

  std::vector<ProcessWrapper *> cluster_nodes;

  // launch the primary node working also as metadata server
  const auto json_file =
      get_data_dir().join("metadata_3_secondaries_pass_v2_gr.js").str();
  auto &primary_node = launch_mysql_server_mock(
      json_file, cluster_nodes_ports[0], EXIT_SUCCESS, false, http_port);
  ASSERT_NO_FATAL_FAILURE(
      check_port_ready(primary_node, cluster_nodes_ports[0]));
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
  set_mock_metadata(http_port, "", cluster_nodes_ports);
  cluster_nodes.emplace_back(&primary_node);

  // launch the secondary cluster nodes
  for (unsigned port = 1; port < cluster_nodes_ports.size(); ++port) {
    auto &secondary_node =
        launch_cluster_node(cluster_nodes_ports[port], get_data_dir().str());
    cluster_nodes.emplace_back(&secondary_node);
    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(secondary_node, cluster_nodes_ports[port]));
  }

  // launch the router with metadata-cache configuration
  const auto classic_RO_bind_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(cluster_nodes_ports[0]);
  const std::string routing_section = get_metadata_cache_routing_section(
      classic_RO_bind_port, "SECONDARY", "round-robin", "", "c_ro",
      GetParam().unreachable_destination_refresh_interval);
  const auto monitoring_port = port_pool_.get_next_available();
  const std::string monitoring_section =
      get_monitoring_section(monitoring_port, temp_test_dir.name());

  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, temp_test_dir.name());
  const std::string destination_status_section =
      get_destination_status_section(GetParam().interval, GetParam().threshold);
  const std::string conf_file{
      create_config_file(temp_test_dir.name(),
                         routing_section + metadata_cache_section +
                             monitoring_section + destination_status_section,
                         &default_section, "test")};

  auto &router{ProcessManager::launch_router({"-c", conf_file}, EXIT_SUCCESS)};

  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client("127.0.0.1", monitoring_port,
                                          kRestApiUsername, kRestApiPassword);
  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_ready(
      wait_for_cache_ready_timeout, metadata_status));

  const std::string deprecate_warning =
      "Option 'unreachable_destination_refresh_interval' is deprecated and "
      "has no effect. Please configure "
      "[destination_status].error_quarantine_interval instead.";
  if (GetParam().unreachable_destination_refresh_interval) {
    EXPECT_THAT(router.get_logfile_content(),
                ::testing::HasSubstr(deprecate_warning));

  } else {
    EXPECT_THAT(router.get_logfile_content(),
                ::testing::Not(::testing::HasSubstr(deprecate_warning)));
  }

  SCOPED_TRACE("// make first RO node unavailable");
  cluster_nodes[1]->send_clean_shutdown_event();
  EXPECT_EQ(cluster_nodes[1]->wait_for_exit(), 0);

  const std::string quarantine_pattern =
      "add destination '.*" + std::to_string(cluster_nodes_ports[1]) +
      "' to quarantine";
  const auto threshold = GetParam().threshold ? *(GetParam().threshold) : 1;
  const auto interval = GetParam().interval ? *(GetParam().interval) : 1s;

  for (size_t i = 1; i <= threshold; ++i) {
    // first node is down so we expect it to be skipped and 2 consecutive
    // connections to be routed to nodes 2 and 3.

    make_new_connection_ok(classic_RO_bind_port, cluster_nodes_ports[2]);
    make_new_connection_ok(classic_RO_bind_port, cluster_nodes_ports[3]);

    // the node should be quarantined only after reaching a threshold
    const std::string log_content = router.get_logfile_content();
    if (i < threshold) {
      EXPECT_THAT(log_content,
                  testing::Not(testing::HasSubstr(quarantine_pattern)))
          << log_content;
    } else {
      EXPECT_TRUE(wait_log_contains(router, quarantine_pattern, 500ms));
    }
  }

  SCOPED_TRACE("// restore first RO node");
  cluster_nodes[1] =
      &launch_cluster_node(cluster_nodes_ports[1], get_data_dir().str());

  const auto start_point = std::chrono::steady_clock::now();
  EXPECT_TRUE(wait_log_contains(router,
                                "Destination candidate '.*" +
                                    std::to_string(cluster_nodes_ports[1]) +
                                    "' is available, remove it from quarantine",
                                5s));
  const auto end_point = std::chrono::steady_clock::now();

  const std::chrono::seconds margin{1};
  EXPECT_THAT(end_point - start_point,
              ::testing::AllOf(::testing::Ge(interval - margin),
                               ::testing::Le(interval + margin)));

  ASSERT_THAT(router.kill(), testing::Eq(0));
}

INSTANTIATE_TEST_SUITE_P(
    Test, UnreachableDestinationQuarantineOptions,
    ::testing::Values(
        QuarantineTestParam{/*interval=default*/ std::nullopt,
                            /*threshold=default*/ std::nullopt,
                            /*deprecated_interval=none*/ std::nullopt},
        QuarantineTestParam{/*interval=default*/ std::nullopt,
                            /*threshold*/ 5,
                            /*deprecated_interval=none*/ std::nullopt},
        QuarantineTestParam{/*interval*/ 2s,
                            /*threshold=default*/ std::nullopt,
                            /*deprecated_interval=none*/ std::nullopt},
        // we expect warning about 'using
        // unreachable_destination_refresh_interval', it's value should be
        // ignored and default should be used
        QuarantineTestParam{/*interval=default*/ std::nullopt,
                            /*threshold=default*/ std::nullopt,
                            /*deprecated_interval*/ 4s}));

class RefreshSharedQuarantineOnTTL : public RouterRoutingStrategyTest {};

/**
 * @test WL14663:TS_R3_1
 */
TEST_F(RefreshSharedQuarantineOnTTL, RemoveDestination) {
  TempDirectory temp_test_dir;
  const std::chrono::seconds ttl{1};

  const std::vector<uint16_t> cluster_nodes_ports{
      port_pool_.get_next_available(),  // first is PRIMARY
      port_pool_.get_next_available(), port_pool_.get_next_available(),
      port_pool_.get_next_available()};
  const auto http_port = port_pool_.get_next_available();

  std::vector<ProcessWrapper *> cluster_nodes;

  // launch the primary node working also as metadata server
  const auto json_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str();
  auto &primary_node = launch_mysql_server_mock(
      json_file, cluster_nodes_ports[0], EXIT_SUCCESS, false, http_port);
  ASSERT_NO_FATAL_FAILURE(
      check_port_ready(primary_node, cluster_nodes_ports[0]));
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
  set_mock_metadata(http_port, "", cluster_nodes_ports);
  cluster_nodes.emplace_back(&primary_node);

  // launch the secondary cluster nodes
  for (unsigned port = 1; port < cluster_nodes_ports.size(); ++port) {
    auto &secondary_node =
        launch_cluster_node(cluster_nodes_ports[port], get_data_dir().str());
    cluster_nodes.emplace_back(&secondary_node);
    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(secondary_node, cluster_nodes_ports[port]));
  }

  // launch the router with metadata-cache configuration
  const auto X_RO_bind_port = port_pool_.get_next_available();
  const auto classic_RO_bind_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(cluster_nodes_ports[0], ttl.count());
  const std::string routing_section =
      get_metadata_cache_routing_section(X_RO_bind_port, "SECONDARY",
                                         "round-robin", "", "x_ro") +
      get_metadata_cache_routing_section(classic_RO_bind_port, "SECONDARY",
                                         "round-robin", "", "c_ro");
  const auto monitoring_port = port_pool_.get_next_available();
  const std::string monitoring_section =
      get_monitoring_section(monitoring_port, temp_test_dir.name());

  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, temp_test_dir.name());
  const std::chrono::seconds unreachable_dest_refresh_value = ttl * 10;
  const std::string destination_status_section =
      get_destination_status_section(unreachable_dest_refresh_value, 1);
  const std::string conf_file{
      create_config_file(temp_test_dir.name(),
                         routing_section + metadata_cache_section +
                             monitoring_section + destination_status_section,
                         &default_section, "test")};

  auto &router{ProcessManager::launch_router({"-c", conf_file}, EXIT_SUCCESS)};
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, X_RO_bind_port));

  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client("127.0.0.1", monitoring_port,
                                          kRestApiUsername, kRestApiPassword);
  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_ready(
      wait_for_cache_ready_timeout, metadata_status));

  SCOPED_TRACE("// make first RO node unavailable");
  cluster_nodes[1]->send_clean_shutdown_event();
  EXPECT_EQ(cluster_nodes[1]->wait_for_exit(), 0);
  make_new_connection_ok(classic_RO_bind_port, cluster_nodes_ports[2]);
  EXPECT_TRUE(wait_log_contains(router,
                                "add destination '.*" +
                                    std::to_string(cluster_nodes_ports[1]) +
                                    "' to quarantine",
                                500ms));

  SCOPED_TRACE("// remove it from metadata");
  set_mock_metadata(
      http_port, "",
      {cluster_nodes_ports[0], cluster_nodes_ports[2], cluster_nodes_ports[3]});

  EXPECT_TRUE(wait_log_contains(
      router,
      "Remove '.*" + std::to_string(cluster_nodes_ports[1]) +
          "' from quarantine, no plugin is using this destination candidate",
      5s));

  SCOPED_TRACE("// restore first RO node");
  cluster_nodes[1] =
      &launch_cluster_node(cluster_nodes_ports[1], get_data_dir().str());
  set_mock_metadata(http_port, "", cluster_nodes_ports);
  wait_for_transaction_count_increase(http_port, 2);

  // check that restored RO node got back to the round-robin rotation
  std::vector<uint16_t> ports_used;
  for (size_t i = 0; i < 3; i++) {
    ports_used.push_back(make_new_connection_ok(classic_RO_bind_port));
  }

  EXPECT_THAT(ports_used,
              ::testing::Contains(::testing::Eq(cluster_nodes_ports[1])));

  ASSERT_THAT(router.kill(), testing::Eq(0));
}

TEST_F(RefreshSharedQuarantineOnTTL, KeepDestination) {
  TempDirectory temp_test_dir;
  const std::chrono::seconds ttl{1};

  const std::vector<uint16_t> cluster_nodes_ports{
      port_pool_.get_next_available(),  // first is PRIMARY
      port_pool_.get_next_available(), port_pool_.get_next_available(),
      port_pool_.get_next_available()};
  const auto http_port = port_pool_.get_next_available();

  std::vector<ProcessWrapper *> cluster_nodes;

  // launch the primary node working also as metadata server
  const auto json_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str();
  auto &primary_node = launch_mysql_server_mock(
      json_file, cluster_nodes_ports[0], EXIT_SUCCESS, false, http_port);
  ASSERT_NO_FATAL_FAILURE(
      check_port_ready(primary_node, cluster_nodes_ports[0]));
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
  set_mock_metadata(http_port, "", cluster_nodes_ports);
  cluster_nodes.emplace_back(&primary_node);

  // launch the secondary cluster nodes
  for (unsigned port = 1; port < cluster_nodes_ports.size(); ++port) {
    auto &secondary_node =
        launch_cluster_node(cluster_nodes_ports[port], get_data_dir().str());
    cluster_nodes.emplace_back(&secondary_node);
    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(secondary_node, cluster_nodes_ports[port]));
  }

  // launch the router with metadata-cache configuration
  const auto classic_RO_bind_port = port_pool_.get_next_available();
  const auto static_bind_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(cluster_nodes_ports[0], ttl.count());
  const std::string routing_section =
      get_metadata_cache_routing_section(classic_RO_bind_port, "SECONDARY",
                                         "round-robin", "", "c_ro") +
      get_static_routing_section(
          static_bind_port, {cluster_nodes_ports[1], cluster_nodes_ports[2]},
          "round-robin", "", "static_r");
  const auto monitoring_port = port_pool_.get_next_available();
  const std::string monitoring_section =
      get_monitoring_section(monitoring_port, temp_test_dir.name());

  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, temp_test_dir.name());
  const std::chrono::seconds unreachable_dest_refresh_value = ttl * 10;
  const std::string destination_status_section =
      get_destination_status_section(unreachable_dest_refresh_value, 1);
  const std::string conf_file{
      create_config_file(temp_test_dir.name(),
                         routing_section + metadata_cache_section +
                             monitoring_section + destination_status_section,
                         &default_section, "test")};

  auto &router{ProcessManager::launch_router({"-c", conf_file}, EXIT_SUCCESS)};

  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client("127.0.0.1", monitoring_port,
                                          kRestApiUsername, kRestApiPassword);
  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_ready(
      wait_for_cache_ready_timeout, metadata_status));

  SCOPED_TRACE("// make first RO node unavailable");
  cluster_nodes[1]->send_clean_shutdown_event();
  EXPECT_EQ(cluster_nodes[1]->wait_for_exit(), 0);
  make_new_connection_ok(classic_RO_bind_port, cluster_nodes_ports[2]);
  EXPECT_TRUE(wait_log_contains(router,
                                "add destination '.*" +
                                    std::to_string(cluster_nodes_ports[1]) +
                                    "' to quarantine",
                                500ms));

  SCOPED_TRACE("// remove it from metadata");
  set_mock_metadata(
      http_port, "",
      {cluster_nodes_ports[0], cluster_nodes_ports[2], cluster_nodes_ports[3]});
  wait_for_transaction_count_increase(http_port, 2);

  // even though the first RO node is no longer in the metadata it should not
  // be removed from the quarantine queue because other plugin still
  // references it
  EXPECT_THAT(router.get_logfile_content(),
              ::testing::Not(::testing::ContainsRegex(
                  "Remove '.*" + std::to_string(cluster_nodes_ports[1]) +
                  "' from quarantine, no plugin is using this destination "
                  "candidate")));

  ASSERT_THAT(router.kill(), testing::Eq(0));
}

TEST_F(RefreshSharedQuarantineOnTTL, instance_in_metadata_but_quarantined) {
  TempDirectory temp_test_dir;

  const std::vector<uint16_t> cluster_nodes_ports{
      port_pool_.get_next_available(),  // first is PRIMARY
      port_pool_.get_next_available(), port_pool_.get_next_available(),
      port_pool_.get_next_available()};
  const auto http_port = port_pool_.get_next_available();

  std::vector<ProcessWrapper *> cluster_nodes;

  // launch the primary node working also as metadata server
  const auto json_file =
      get_data_dir().join("metadata_3_secondaries_pass_v2_gr.js").str();
  auto &primary_node = launch_mysql_server_mock(
      json_file, cluster_nodes_ports[0], EXIT_SUCCESS, false, http_port);
  ASSERT_NO_FATAL_FAILURE(
      check_port_ready(primary_node, cluster_nodes_ports[0]));
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
  set_mock_metadata(http_port, "", cluster_nodes_ports);
  cluster_nodes.emplace_back(&primary_node);

  // launch the secondary cluster nodes
  for (unsigned port = 1; port < cluster_nodes_ports.size(); ++port) {
    auto &secondary_node =
        launch_cluster_node(cluster_nodes_ports[port], get_data_dir().str());
    cluster_nodes.emplace_back(&secondary_node);
    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(secondary_node, cluster_nodes_ports[port]));
  }

  // launch the router with metadata-cache configuration
  const auto classic_RO_bind_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(cluster_nodes_ports[0], 1 /*ttl*/);
  const std::string routing_section = get_metadata_cache_routing_section(
      classic_RO_bind_port, "SECONDARY", "round-robin", "", "c_ro");
  const auto monitoring_port = port_pool_.get_next_available();
  const std::string monitoring_section =
      get_monitoring_section(monitoring_port, temp_test_dir.name());

  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, temp_test_dir.name());
  const std::chrono::seconds unreachable_dest_refresh_value{3600};
  const std::string destination_status_section =
      get_destination_status_section(unreachable_dest_refresh_value, 1);
  const std::string conf_file{
      create_config_file(temp_test_dir.name(),
                         routing_section + metadata_cache_section +
                             monitoring_section + destination_status_section,
                         &default_section, "test")};

  auto &router{ProcessManager::launch_router({"-c", conf_file}, EXIT_SUCCESS)};

  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client("127.0.0.1", monitoring_port,
                                          kRestApiUsername, kRestApiPassword);
  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_ready(
      wait_for_cache_ready_timeout, metadata_status));

  SCOPED_TRACE("// make first RO node unavailable");
  cluster_nodes[1]->send_clean_shutdown_event();
  EXPECT_EQ(cluster_nodes[1]->wait_for_exit(), 0);
  make_new_connection_ok(classic_RO_bind_port, cluster_nodes_ports[2]);
  EXPECT_TRUE(wait_log_contains(router,
                                "add destination '.*" +
                                    std::to_string(cluster_nodes_ports[1]) +
                                    "' to quarantine",
                                500ms));

  SCOPED_TRACE("// restore first RO node unavailable");
  cluster_nodes[1] =
      &launch_cluster_node(cluster_nodes_ports[1], get_data_dir().str());

  // Since error_quarantine_interval is very high this will
  // be triggered by the ttl.
  SCOPED_TRACE(
      "// Instance is quarantined but according to metadata it is available");
  EXPECT_TRUE(wait_log_contains(router,
                                "Destination candidate '.*" +
                                    std::to_string(cluster_nodes_ports[1]) +
                                    "' is available, remove it from quarantine",
                                5s));

  ASSERT_THAT(router.kill(), testing::Eq(0));
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
