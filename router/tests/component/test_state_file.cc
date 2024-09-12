/*
Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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
#include <fstream>
#include <stdexcept>
#include <thread>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "config_builder.h"
#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql/harness/string_utils.h"  // split_string
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "process_manager.h"
#include "router_component_system_layout.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_test_helpers.h"
#include "tcp_port_pool.h"

using mysqlrouter::ClusterType;
using mysqlrouter::MySQLSession;
using namespace std::chrono_literals;

Path g_origin_path;

namespace {
// default allocator for rapidJson (MemoryPoolAllocator) is broken for
// SparcSolaris
using JsonAllocator = rapidjson::CrtAllocator;
using JsonValue = rapidjson::GenericValue<rapidjson::UTF8<>, JsonAllocator>;
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, JsonAllocator>;
using JsonStringBuffer =
    rapidjson::GenericStringBuffer<rapidjson::UTF8<>, rapidjson::CrtAllocator>;

constexpr auto kTTL = 100ms;
}  // namespace

class StateFileTest : public RouterComponentBootstrapTest {
 protected:
  void SetUp() override {
    RouterComponentTest::SetUp();
    // this test modifies the origin path so we need to restore it
    ProcessManager::set_origin(g_origin_path);
  }

  std::pair<std::string, std::map<std::string, std::string>>
  metadata_cache_section(const std::chrono::milliseconds ttl = kTTL,
                         ClusterType cluster_type = ClusterType::GR_V2) {
    std::map<std::string, std::string> options{
        {"cluster_type", (cluster_type == ClusterType::RS_V2) ? "rs" : "gr"},
        {"router_id", "1"},
        {"user", "mysql_router1_user"},
        {"metadata_cluster", "mycluster"},
        {"connect_timeout", "1"},
        {"ttl", std::to_string(std::chrono::duration<double>(ttl).count())},
    };

    return {"metadata_cache:test", options};
  }

  std::string get_metadata_cache_section(
      const std::chrono::milliseconds ttl = kTTL,
      ClusterType cluster_type = ClusterType::GR_V2) {
    auto section = metadata_cache_section(ttl, cluster_type);
    return mysql_harness::ConfigBuilder::build_section(section.first,
                                                       section.second) +
           "\n";
  }

  std::pair<std::string, std::map<std::string, std::string>>
  metadata_cache_routing_section(uint16_t router_port, const std::string &role,
                                 const std::string &strategy) {
    std::map<std::string, std::string> options{
        {"bind_port", std::to_string(router_port)},
        {"destinations", "metadata-cache://test/default?role=" + role},
        {"protocol", "classic"},
    };

    if (!strategy.empty()) options["routing_strategy"] = strategy;

    return {"routing:test_default", options};
  }

  std::string get_metadata_cache_routing_section(uint16_t router_port,
                                                 const std::string &role,
                                                 const std::string &strategy) {
    const auto section =
        metadata_cache_routing_section(router_port, role, strategy);
    return mysql_harness::ConfigBuilder::build_section(section.first,
                                                       section.second);
  }

  auto &launch_router(const std::string &temp_test_dir,
                      const std::string &metadata_cache_section,
                      const std::string &routing_section,
                      const std::string &state_file_path,
                      const int expected_errorcode = EXIT_SUCCESS,
                      std::chrono::milliseconds wait_for_notify = 5s) {
    const std::string masterkey_file =
        Path(temp_test_dir).join("master.key").str();
    const std::string keyring_file = Path(temp_test_dir).join("keyring").str();
    mysql_harness::init_keyring(keyring_file, masterkey_file, true);
    mysql_harness::Keyring *keyring = mysql_harness::get_keyring();
    keyring->store("mysql_router1_user", "password", "root");
    mysql_harness::flush_keyring();
    mysql_harness::reset_keyring();

    // launch the router with metadata-cache configuration
    auto default_section = get_DEFAULT_defaults();
    default_section["keyring_path"] = keyring_file;
    default_section["master_key_path"] = masterkey_file;
    default_section["dynamic_state"] = state_file_path;
    const std::string conf_file = create_config_file(
        temp_test_dir, metadata_cache_section + routing_section,
        &default_section);
    auto &router = ProcessManager::launch_router(
        {"-c", conf_file}, expected_errorcode, /*catch_stderr=*/true,
        /*with_sudo=*/false,
        /*wait_for_notify_ready=*/wait_for_notify);
    return router;
  }

  bool wait_log_file_contains(ProcessWrapper &router,
                              const std::string &expected_entry,
                              std::chrono::milliseconds max_wait_time) {
    auto kRetrySleep = 100ms;
    if (getenv("WITH_VALGRIND")) {
      max_wait_time *= 50;
      kRetrySleep *= 10;
    }
    do {
      const auto log_content = router.get_logfile_content();
      if (log_content.find(expected_entry) != std::string::npos) return true;

      std::this_thread::sleep_for(kRetrySleep);
      if (max_wait_time <= kRetrySleep) return false;
      max_wait_time -= kRetrySleep;
    } while (true);
  }

  std::string create_state_file_content(
      const std::string &cluster_id,
      const std::vector<uint16_t> &metadata_servers_ports,
      const std::string &hostname = "127.0.0.1") {
    std::string metadata_servers;
    for (std::size_t i = 0; i < metadata_servers_ports.size(); i++) {
      metadata_servers += R"("mysql://)" + hostname + ":" +
                          std::to_string(metadata_servers_ports[i]) + "\"";
      if (i < metadata_servers_ports.size() - 1) metadata_servers += ",";
    }
    // clang-format off
    const std::string result =
      "{"
         R"("version": "1.0.0",)"
         R"("metadata-cache": {)"
           R"("group-replication-id": ")" + cluster_id + R"(",)"
           R"("cluster-metadata-servers": [)" + metadata_servers + "]"
          "}"
        "}";
    // clang-format on

    return result;
  }
};

//////////////////////////////////////////////////////////////////////////

class StateFileDynamicChangesTest : public StateFileTest {
 protected:
  void kill_server(ProcessWrapper *server) { EXPECT_NO_THROW(server->kill()); }
};

struct StateFileTestParam {
 public:
  std::string description;
  std::string trace_file;
  ClusterType cluster_type;
  bool ipv6{false};
};

auto get_test_description(
    const ::testing::TestParamInfo<StateFileTestParam> &info) {
  return info.param.description;
}

class StateFileMetadataServersChangedInRuntimeTest
    : public StateFileDynamicChangesTest,
      public ::testing::WithParamInterface<StateFileTestParam> {};

/**
 * @test
 *      Verify that changes in the cluster topology are reflected in the state
 * file in the runtime.
 */
TEST_P(StateFileMetadataServersChangedInRuntimeTest,
       MetadataServersChangedInRuntime) {
  const auto param = GetParam();
  const std::string kGroupId = "3a0be5af-0022-11e8-9655-0800279e6a88";

  TempDirectory temp_test_dir;

  const unsigned CLUSTER_NODES = 3;
  std::vector<ProcessWrapper *> cluster_nodes;
  std::vector<uint16_t> cluster_nodes_ports;
  std::vector<uint16_t> cluster_http_ports;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  const std::string node_host = param.ipv6 ? "[::1]" : "127.0.0.1";
  const std::string bind_address = param.ipv6 ? "::" : "127.0.0.1";

  SCOPED_TRACE(
      "// Launch 3 server mocks that will act as our metadata servers");
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(
        &mock_server_spawner().spawn(mock_server_cmdline(param.trace_file)
                                         .port(cluster_nodes_ports[i])
                                         .http_port(cluster_http_ports[i])
                                         .bind_address(bind_address)
                                         .args()));
    try {
      ASSERT_NO_FATAL_FAILURE(check_port_ready(
          *cluster_nodes[i], cluster_nodes_ports[i], kDefaultPortReadyTimeout,
          param.ipv6 ? "::1" : "127.0.0.1"));
    } catch (const std::system_error &e) {
      // the only expected system-error is "address-no-available" in case of
      // trying to bind to ipv6 when ipv6 is disabled on the host
      ASSERT_EQ(e.code(),
                make_error_condition(std::errc::address_not_available));

      // there is no good synchronization point for waiting for the mock's
      // signal handler to be setup
      //
      // - nothing is written to the log
      std::this_thread::sleep_for(100ms);
      return;
    }
    ASSERT_TRUE(MockServerRestClient(cluster_http_ports[i])
                    .wait_for_rest_endpoint_ready());

    SCOPED_TRACE(
        "// Make our metadata server to return single node as a cluster "
        "member (meaning single metadata server)");
    set_mock_metadata(cluster_http_ports[i], kGroupId,
                      classic_ports_to_gr_nodes({cluster_nodes_ports[i]}), i,
                      {cluster_nodes_ports[i]}, 0, false, node_host);
  }

  SCOPED_TRACE("// Create a router state file with a single metadata server");
  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(kGroupId, {cluster_nodes_ports[0]}, node_host));

  SCOPED_TRACE(
      "// Create a configuration file sections with low ttl so that any "
      "changes we make in the mock server via http port were refreshed "
      "quickly");
  const std::string metadata_cache_section =
      get_metadata_cache_section(kTTL, param.cluster_type);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router with the initial state file");
  launch_router(temp_test_dir.name(), metadata_cache_section, routing_section,
                state_file);

  SCOPED_TRACE(
      "// Check our state file content, it should not change yet, there is "
      "single metadata server reported as initially");

  check_state_file(state_file, param.cluster_type, kGroupId,
                   {cluster_nodes_ports[0]}, 0, node_host);

  SCOPED_TRACE(
      "// Now change the response from the metadata server to return 3 gr "
      "nodes (metadata servers)");
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    set_mock_metadata(cluster_http_ports[i], kGroupId,
                      classic_ports_to_gr_nodes(cluster_nodes_ports), i,
                      classic_ports_to_cluster_nodes(cluster_nodes_ports), 0,
                      false, node_host);
  }

  SCOPED_TRACE(
      "// Check our state file content, it should now contain 3 metadata "
      "servers");

  check_state_file(
      state_file, param.cluster_type, kGroupId,
      {cluster_nodes_ports[0], cluster_nodes_ports[1], cluster_nodes_ports[2]},
      0, node_host);

  ///////////////////////////////////////////////////

  SCOPED_TRACE(
      "// We have 3 nodes now, let's make a few connections, we have single "
      "primary configured so they should be directed to the same first server");
  std::string out_port;
  for (unsigned i = 0; i < 3; ++i) {
    EXPECT_NO_THROW(connect_client_and_query_port(router_port, out_port,
                                                  /*should_fail=*/false));
    EXPECT_EQ(out_port, std::to_string(cluster_nodes_ports[0]));
  }

  ///////////////////////////////////////////////////

  SCOPED_TRACE(
      "// Instrument the second and third metadata servers to return 2 "
      "servers: second and third");
  set_mock_metadata(cluster_http_ports[1], kGroupId,
                    classic_ports_to_gr_nodes(
                        {cluster_nodes_ports[1], cluster_nodes_ports[2]}),
                    1, {cluster_nodes_ports[1], cluster_nodes_ports[2]}, 0,
                    false, node_host);
  set_mock_metadata(cluster_http_ports[2], kGroupId,
                    classic_ports_to_gr_nodes(
                        {cluster_nodes_ports[1], cluster_nodes_ports[2]}),
                    2, {cluster_nodes_ports[1], cluster_nodes_ports[2]}, 0,
                    false, node_host);

  SCOPED_TRACE("// Kill first metada server");
  kill_server(cluster_nodes[0]);

  SCOPED_TRACE(
      "// Check our state file content, it should now contain 2 metadata "
      "servers reported by the second metadata server");
  check_state_file(state_file, param.cluster_type, kGroupId,
                   {cluster_nodes_ports[1], cluster_nodes_ports[2]}, 0,
                   node_host, 10000ms);
}

INSTANTIATE_TEST_SUITE_P(
    MetadataServersChangedInRuntime,
    StateFileMetadataServersChangedInRuntimeTest,
    ::testing::Values(
        StateFileTestParam{"gr_v2", "metadata_dynamic_nodes_v2_gr.js",
                           ClusterType::GR_V2},
        StateFileTestParam{"ar_v2", "metadata_dynamic_nodes_v2_ar.js",
                           ClusterType::RS_V2},
        StateFileTestParam{"gr_v2_ipv6", "metadata_dynamic_nodes_v2_gr.js",
                           ClusterType::GR_V2, /*ipv6=*/true},
        StateFileTestParam{"ar_v2_ipv6", "metadata_dynamic_nodes_v2_ar.js",
                           ClusterType::RS_V2, /*ipv6=*/true}),
    get_test_description);

class StateFileMetadataServersInaccessibleTest
    : public StateFileDynamicChangesTest,
      public ::testing::WithParamInterface<StateFileTestParam> {};

/* @test
 *      Verify that if not metadata server can't be accessed the list of the
 * server does not get cleared.
 */
TEST_P(StateFileMetadataServersInaccessibleTest, MetadataServersInaccessible) {
  const auto param = GetParam();
  const std::string kGroupId = "3a0be5af-0022-11e8-9655-0800279e6a88";

  TempDirectory temp_test_dir;

  uint16_t cluster_node_port = port_pool_.get_next_available();
  uint16_t cluster_http_port = port_pool_.get_next_available();

  SCOPED_TRACE(
      "// Launch single server mock that will act as our metadata server");
  auto &cluster_node =
      mock_server_spawner().spawn(mock_server_cmdline(param.trace_file)
                                      .port(cluster_node_port)
                                      .http_port(cluster_http_port)
                                      .args());

  ASSERT_NO_FATAL_FAILURE(check_port_ready(cluster_node, cluster_node_port));
  ASSERT_TRUE(
      MockServerRestClient(cluster_http_port).wait_for_rest_endpoint_ready());

  SCOPED_TRACE(
      "// Make our metadata server return single node as a cluster "
      "member (meaning single metadata server)");

  set_mock_metadata(cluster_http_port, kGroupId,
                    classic_ports_to_gr_nodes({cluster_node_port}), 0,
                    classic_ports_to_cluster_nodes({cluster_node_port}));

  SCOPED_TRACE("// Create a router state file with a single metadata server");
  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(kGroupId, {cluster_node_port}));

  SCOPED_TRACE("// Create a configuration file with low ttl");
  const std::string metadata_cache_section =
      get_metadata_cache_section(kTTL, param.cluster_type);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router with the initial state file");
  auto &router = launch_router(temp_test_dir.name(), metadata_cache_section,
                               routing_section, state_file);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port));

  // kill our single instance server
  kill_server(&cluster_node);

  SCOPED_TRACE(
      "// Check our state file content, it should still contain out metadata "
      "server");

  check_state_file(state_file, param.cluster_type, kGroupId,
                   {cluster_node_port}, 0, "127.0.0.1", 10s);

  router.send_shutdown_event();

  EXPECT_EQ(0, router.wait_for_exit());
}

INSTANTIATE_TEST_SUITE_P(
    MetadataServersInaccessible, StateFileMetadataServersInaccessibleTest,
    ::testing::Values(StateFileTestParam{"gr_v2",
                                         "metadata_dynamic_nodes_v2_gr.js",
                                         ClusterType::GR_V2},
                      StateFileTestParam{"ar_v2",
                                         "metadata_dynamic_nodes_v2_ar.js",
                                         ClusterType::RS_V2}),
    get_test_description);

class StateFileGroupReplicationIdDiffersTest
    : public StateFileDynamicChangesTest,
      public ::testing::WithParamInterface<StateFileTestParam> {};

/**
 * @test
 *      Verify that if the metadata servers do not know about the replication
 * group id that was bootstrapped against, Router does not use metadata for
 * routing, logs an error but does not change the metadata servers list in the
 * dynamic state file.
 */
TEST_P(StateFileGroupReplicationIdDiffersTest, GroupReplicationIdDiffers) {
  const auto param = GetParam();
  constexpr const char kStateFileGroupId[] =
      "3a0be5af-0022-11e8-0000-0800279e6a88";
  constexpr const char kClusterFileGroupId[] =
      "3a0be5af-0022-11e8-0000-0800279e6a89";

  TempDirectory temp_test_dir;

  auto cluster_node_port = port_pool_.get_next_available();
  auto cluster_http_port = port_pool_.get_next_available();

  SCOPED_TRACE("// Launch server mock that will act as our metadata server");

  /*auto &cluster_node =*/mock_server_spawner().spawn(
      mock_server_cmdline(param.trace_file)
          .port(cluster_node_port)
          .http_port(cluster_http_port)
          .args());

  SCOPED_TRACE(
      "// Make our metadata server to return single node as a cluster "
      "member (meaning single metadata server)");

  set_mock_metadata(cluster_http_port, kClusterFileGroupId,
                    classic_ports_to_gr_nodes({cluster_node_port}), 0,
                    {cluster_node_port});

  SCOPED_TRACE(
      "// Create a router state file with a single metadata server and "
      "group-replication-id different than the one reported by the "
      "mock-server");
  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(kStateFileGroupId, {cluster_node_port}));

  SCOPED_TRACE(
      "// Create a configuration file sections with low ttl so that any "
      "changes we make in the mock server via http port were refreshed "
      "quickly");
  const std::string metadata_cache_section =
      get_metadata_cache_section(kTTL, param.cluster_type);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch the router with the initial state file");
  auto &router = launch_router(temp_test_dir.name(), metadata_cache_section,
                               routing_section, state_file, EXIT_SUCCESS, -1s);

  SCOPED_TRACE(
      "// Check our state file content, it should not change. "
      "We did not found the data for our replication group on any of the "
      "servers so we do not update the metadata srever list.");

  check_state_file(state_file, param.cluster_type, kStateFileGroupId,
                   {cluster_node_port});

  SCOPED_TRACE("// We expect an error in the logfile");
  EXPECT_TRUE(wait_log_file_contains(
      router, "Failed fetching metadata from any of the 1 metadata servers",
      5s));

  // now try to connect to the router port, we expect error 2003
  std::string out_port_unused;
  EXPECT_NO_THROW(connect_client_and_query_port(router_port, out_port_unused,
                                                /*should_fail=*/true));
}

INSTANTIATE_TEST_SUITE_P(
    GroupReplicationIdDiffers, StateFileGroupReplicationIdDiffersTest,
    ::testing::Values(StateFileTestParam{"gr_v2",
                                         "metadata_dynamic_nodes_v2_gr.js",
                                         ClusterType::GR_V2},
                      StateFileTestParam{"ar_v2",
                                         "metadata_dynamic_nodes_v2_ar.js",
                                         ClusterType::RS_V2}),
    get_test_description);

class StateFileSplitBrainScenarioTest
    : public StateFileDynamicChangesTest,
      public ::testing::WithParamInterface<StateFileTestParam> {};
/**
 * @test
 *      Verify that if the split brain scenario the list of the metadata servers
 * gets updated properly in the state file.
 */
TEST_P(StateFileSplitBrainScenarioTest, SplitBrainScenario) {
  const auto param = GetParam();
  const std::string kClusterGroupId = "3a0be5af-0022-11e8-0000-0800279e6a88";
  const unsigned kNodesNum = 3;  // number of nodes in the cluster
                                 //  TcpPortPool
  //      cluster_ports_pool;  // currently TcpPortPool supports max 10 ports so
  //                           // we create dedicated one for our cluster

  TempDirectory temp_test_dir;

  std::vector<ProcessWrapper *> cluster_nodes;
  std::vector<std::pair<uint16_t, uint16_t>>
      cluster_node_ports;  // pair of connection and http port

  for (unsigned i = 0; i < kNodesNum; i++) {
    cluster_node_ports.push_back(
        {port_pool_.get_next_available(), port_pool_.get_next_available()});
  }

  SCOPED_TRACE("// Launch  server mocks that play as our split brain cluster");
  for (unsigned i = 0; i < kNodesNum; i++) {
    const auto [port_connect, port_http] = cluster_node_ports[i];

    cluster_nodes.push_back(
        &mock_server_spawner().spawn(mock_server_cmdline(param.trace_file)
                                         .port(port_connect)
                                         .http_port(port_http)
                                         .args()));

    ASSERT_NO_FATAL_FAILURE(check_port_ready(*cluster_nodes[i], port_connect));
    ASSERT_TRUE(MockServerRestClient(port_http).wait_for_rest_endpoint_ready());
  }

  SCOPED_TRACE(
      "// let's configure the metadata so that there are 2 groups that do not "
      "know about each other (split brain)");

  std::vector<uint16_t> fist_group{cluster_node_ports[0].first,
                                   cluster_node_ports[1].first};
  for (unsigned i = 0; i < 1; i++) {
    const auto port_http = cluster_node_ports[i].second;
    set_mock_metadata(port_http, kClusterGroupId,
                      classic_ports_to_gr_nodes(fist_group), i,
                      classic_ports_to_cluster_nodes(fist_group));
  }

  std::vector<uint16_t> second_group{cluster_node_ports[2].first};
  for (unsigned i = 2; i < kNodesNum; i++) {
    const auto port_http = cluster_node_ports[i].second;
    set_mock_metadata(port_http, kMockServerGlobalsRestUri,
                      classic_ports_to_gr_nodes(second_group), i - 2,
                      classic_ports_to_cluster_nodes(second_group));
  }

  SCOPED_TRACE(
      "// Create a router state file with all the nodes as a "
      "cluster-metadata-servers ");
  std::vector<uint16_t> cluster_ports;
  for (const auto &port : cluster_node_ports)
    cluster_ports.push_back(port.first);
  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(kClusterGroupId, cluster_ports));

  SCOPED_TRACE(
      "// Create a configuration file sections with low ttl so that any "
      "changes we make in the mock server via http port were refreshed "
      "quickly");
  const std::string metadata_cache_section =
      get_metadata_cache_section(kTTL, param.cluster_type);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router with the initial state file");
  launch_router(temp_test_dir.name(), metadata_cache_section, routing_section,
                state_file);
  SCOPED_TRACE(
      "// Check our state file content, it should now contain only the nodes "
      "from the first group.");

  std::vector<uint16_t> node_ports;
  for (unsigned i = 0; i < 2; ++i) {
    node_ports.push_back(cluster_node_ports[i].first);
  }
  check_state_file(state_file, param.cluster_type, kClusterGroupId, node_ports);

  SCOPED_TRACE(
      "// Try to connect to the router port, we expect first port from the "
      "first group.");
  std::string port_connected;
  EXPECT_NO_THROW(connect_client_and_query_port(router_port, port_connected,
                                                /*should_fail=*/false));
  EXPECT_STREQ(std::to_string(cluster_node_ports[0].first).c_str(),
               port_connected.c_str());
}

INSTANTIATE_TEST_SUITE_P(SplitBrainScenario, StateFileSplitBrainScenarioTest,
                         ::testing::Values(StateFileTestParam{
                             "gr_v2", "metadata_dynamic_nodes_v2_gr.js",
                             ClusterType::GR_V2}),
                         get_test_description);

/**
 * @test
 *      Verify that in case of empty metada-server-address list in the state
 * file the Router logs proper error and exits.
 */
TEST_F(StateFileDynamicChangesTest, EmptyMetadataServersList) {
  constexpr const char kGroupId[] = "3a0be5af-0022-11e8-9655-0800279e6a88";

  TempDirectory temp_test_dir;

  SCOPED_TRACE("// Create a router state file with empty server list");
  const std::string state_file = create_state_file(
      temp_test_dir.name(), create_state_file_content(kGroupId, {}));

  SCOPED_TRACE(
      "// Create a configuration file sections with low ttl so that any "
      "changes we make in the mock server via http port were refreshed "
      "quickly");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router with the initial state file");
  auto &router = launch_router(temp_test_dir.name(), metadata_cache_section,
                               routing_section, state_file, EXIT_FAILURE, -1s);

  // wait for shutdown before checking the logfile.
  EXPECT_EQ(router.wait_for_exit(), EXIT_FAILURE);

  // proper error should get logged
  EXPECT_TRUE(wait_log_file_contains(
      router,
      "list of 'cluster-metadata-servers' in 'dynamic_config'-file is empty.",
      3 * kTTL));
}

//////////////////////////////////////////////////////////////////////////

struct StateFileSchemaTestParams {
  std::string state_file_content;
  std::vector<std::string> expected_errors_in_log;
  bool create_state_file_from_content{true};
  std::string state_file_path{""};
  ClusterType cluster_type{ClusterType::GR_V2};
};

::std::ostream &operator<<(::std::ostream &os,
                           const StateFileSchemaTestParams &sfp) {
  os << "state_file_content = " << sfp.state_file_content
     << "\n, expected_errors = [";

  for (const auto &err : sfp.expected_errors_in_log) {
    os << err << "\n";
  }
  os << "]\n";

  return os;
}

class StateFileSchemaTest
    : public StateFileTest,
      public ::testing::WithParamInterface<StateFileSchemaTestParams> {
 protected:
  void SetUp() override { StateFileTest::SetUp(); }
};

/**
 * @test
 *      Verify that the proper error gets logged and the Router shuts down in
 * case of various configuration mismatches.
 */
TEST_P(StateFileSchemaTest, ParametrizedStateFileSchemaTest) {
  auto test_params = GetParam();

  TempDirectory temp_test_dir;

  const uint16_t router_port = port_pool_.get_next_available();

  const std::string state_file =
      test_params.create_state_file_from_content
          ? create_state_file(temp_test_dir.name(),
                              test_params.state_file_content)
          : test_params.state_file_path;

  auto writer =
      config_writer(temp_test_dir.name())
          .section(metadata_cache_section(kTTL, test_params.cluster_type))
          .section(metadata_cache_routing_section(router_port, "PRIMARY",
                                                  "first-available"));

  auto &default_section = writer.sections()["DEFAULT"];

  init_keyring(default_section, temp_test_dir.name());

  default_section["dynamic_state"] = state_file;

  auto &router =
      router_spawner()
          .expected_exit_code(EXIT_FAILURE)
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::RUNNING)
          .spawn({"-c", writer.write()});

  // the router should close with non-0 return value
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));

  // proper log should get logged
  auto log_content = router.get_logfile_content();
  for (const auto &expeted_in_log : test_params.expected_errors_in_log) {
    EXPECT_TRUE(log_content.find(expeted_in_log) != std::string::npos);
  }
}

INSTANTIATE_TEST_SUITE_P(
    StateFileTests, StateFileSchemaTest,
    ::testing::Values(
        // state file does not exits
        StateFileSchemaTestParams{
            "",
            {"Could not open dynamic state file 'non-existing.json' for "
             "reading"},
            false, /* = don't create state file, use the path given */
            "non-existing.json"},

        // state file path empty
        StateFileSchemaTestParams{
            "",
            {"Could not open dynamic state file '' for reading"},
            false, /* = don't create state file, use the empty path given */
            ""},

        // state file containing invalid non-json data
        StateFileSchemaTestParams{"some invalid, non-json content",
                                  {"Error parsing file dynamic state file",
                                   "Parsing JSON failed at offset 0"}},

        // state file content is not an object
        StateFileSchemaTestParams{"[]",
                                  {"Invalid json structure: not an object"}},

        // clang-format off
        // version field missing
        StateFileSchemaTestParams{
            "{"
              "\"metadata-cache\": {"
                "\"group-replication-id\": \"3a0be5af-994c-11e8-9655-0800279e6a88\","
                "\"cluster-metadata-servers\": ["
                  "\"mysql://localhost:5000\","
                  "\"mysql://127.0.0.1:5001\""
                "]"
              "}"
            "}",
            {"Invalid json structure: missing field: version"}},

        // version field is not a string
        StateFileSchemaTestParams{
            "{"
              "\"version\": 1,"
              "\"metadata-cache\": {"
                "\"group-replication-id\": \"3a0be5af-994c-11e8-9655-0800279e6a88\","
                "\"cluster-metadata-servers\": ["
                  "\"mysql://localhost:5000\","
                  "\"mysql://127.0.0.1:5001\""
                "]"
              "}"
            "}",
            {"Invalid json structure: field version "
             "should be a string type"}},

        // version field is non numeric string
        StateFileSchemaTestParams{
            "{"
              "\"version\": \"str\","
              "\"metadata-cache\": {"
                "\"group-replication-id\": \"3a0be5af-994c-11e8-9655-0800279e6a88\","
                "\"cluster-metadata-servers\": ["
                  "\"mysql://localhost:5000\","
                  "\"mysql://127.0.0.1:5001\""
                "]"
              "}"
            "}",
            {"Invalid version field format, expected MAJOR.MINOR.PATCH, "
             "found: str"}},

        // version field has wrong number of numeric values
        StateFileSchemaTestParams{
            "{"
              "\"version\": \"1.0\","
              "\"metadata-cache\": {"
                "\"group-replication-id\": \"3a0be5af-994c-11e8-9655-0800279e6a88\","
                "\"cluster-metadata-servers\": ["
                  "\"mysql://localhost:5000\","
                  "\"mysql://127.0.0.1:5001\""
                "]"
              "}"
            "}",
            {"Invalid version field format, expected MAJOR.MINOR.PATCH, "
             "found: 1.0"}},

        // major version does not match (GR cluster)
        StateFileSchemaTestParams{
            "{"
              "\"version\": \"2.0.0\","
              "\"metadata-cache\": {"
                "\"group-replication-id\": \"3a0be5af-994c-11e8-9655-0800279e6a88\","
                "\"cluster-metadata-servers\": ["
                  "\"mysql://localhost:5000\","
                  "\"mysql://127.0.0.1:5001\""
                "]"
              "}"
            "}",
            {"Unsupported state file version, "
             "expected: 1.1.0, found: 2.0.0"}},
        // major version does not match (AR cluster)
        StateFileSchemaTestParams{
            "{"
              "\"version\": \"2.0.0\","
              "\"metadata-cache\": {"
                "\"group-replication-id\": \"3a0be5af-994c-11e8-9655-0800279e6a88\","
                "\"cluster-metadata-servers\": ["
                  "\"mysql://localhost:5000\","
                  "\"mysql://127.0.0.1:5001\""
                "]"
              "}"
            "}",
            {"Unsupported state file version, expected: 1.1.0, found: 2.0.0"},
              true, "", ClusterType::RS_V2},
        // minor version does not match
        StateFileSchemaTestParams{
        "{"
          "\"version\": \"1.2.0\","
          "\"metadata-cache\": {"
            "\"group-replication-id\": \"3a0be5af-994c-11e8-9655-0800279e6a88\","
            "\"cluster-metadata-servers\": ["
              "\"mysql://localhost:5000\","
              "\"mysql://127.0.0.1:5001\""
            "]"
          "}"
        "}",
        {"Unsupported state file version, "
         "expected: 1.1.0, found: 1.2.0"}},
        // group-replication-id filed missing
        // no longer required
//        StateFileSchemaTestParams{
//        "{"
//          "\"version\": \"1.0.0\","
//          "\"metadata-cache\": {"
//            "\"cluster-metadata-servers\": ["
//              "\"mysql://localhost:5000\","
//              "\"mysql://127.0.0.1:5001\""
//            "]"
//          "}"
//        "}",
//        {"JSON file failed validation against JSON schema: Failed schema "
//         "directive: #/properties/metadata-cache",
//         "Failed schema keyword:   required",
//         "Failure location in validated document: #/metadata-cache"}},
        // cluster-metadata-servers filed missing (GR cluster)
        StateFileSchemaTestParams{
        "{"
          "\"version\": \"1.0.0\","
          "\"metadata-cache\": {"
            "\"group-replication-id\": \"3a0be5af-994c-11e8-9655-0800279e6a88\""
          "}"
        "}",
        {"JSON file failed validation against JSON schema: Failed schema "
         "directive: #/properties/metadata-cache",
         "Failed schema keyword:   required",
         "Failure location in validated document: #/metadata-cache"}},
         // cluster-metadata-servers filed missing (AR cluster)
         StateFileSchemaTestParams{
            "{"
              "\"version\": \"1.0.0\","
              "\"metadata-cache\": {"
                "\"group-replication-id\": \"3a0be5af-994c-11e8-9655-0800279e6a88\""
              "}"
            "}",
            {"JSON file failed validation against JSON schema: Failed schema "
             "directive: #/properties/metadata-cache",
             "Failed schema keyword:   required",
             "Failure location in validated document: #/metadata-cache"
                // clang-format on
            }}));

////////////////////////////////////////////
// Test for state file right access
////////////////////////////////////////////

#ifndef _WIN32

struct StateFileAccessRightsTestParams {
  bool read_access;
  bool write_access;
  std::string expected_error;

  StateFileAccessRightsTestParams(bool read_access_, bool write_access_,
                                  const std::string &expected_error_)
      : read_access(read_access_),
        write_access(write_access_),
        expected_error(expected_error_) {}
};

class StateFileAccessRightsTest
    : public StateFileTest,
      public ::testing::WithParamInterface<StateFileAccessRightsTestParams> {
 protected:
  void SetUp() override { StateFileTest::SetUp(); }
};

/**
 * @test
 *      Verify that the proper error gets logged and the Router shuts down in
 * case of state file access rights problems.
 */
TEST_P(StateFileAccessRightsTest, ParametrizedStateFileSchemaTest) {
  auto test_params = GetParam();

  TempDirectory temp_test_dir;

  const uint16_t router_port = port_pool_.get_next_available();

  // launch the router with static metadata-cache configuration and
  // dynamic state file configured via test parameter

  const std::string state_file = create_state_file(
      temp_test_dir.name(), create_state_file_content("000-000", {10000}));
  mode_t file_mode = 0;
  if (test_params.read_access) file_mode |= S_IRUSR;
  if (test_params.write_access) file_mode |= S_IWUSR;
  chmod(state_file.c_str(), file_mode);

  auto writer = config_writer(temp_test_dir.name())
                    .section(metadata_cache_section())
                    .section(metadata_cache_routing_section(
                        router_port, "PRIMARY", "first-available"));

  auto &default_section = writer.sections()["DEFAULT"];

  init_keyring(default_section, temp_test_dir.name());

  default_section["dynamic_state"] = state_file;

  auto &router =
      router_spawner()
          .expected_exit_code(EXIT_FAILURE)
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
          .spawn({"-c", writer.write()});

  // the router should close with non-0 return value
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));

  // proper error should get logged
  EXPECT_TRUE(wait_log_file_contains(router, test_params.expected_error, 1ms));
}

INSTANTIATE_TEST_SUITE_P(
    StateFileTests, StateFileAccessRightsTest,
    ::testing::Values(
        // no read, nor write access
        StateFileAccessRightsTestParams(false, false,
                                        "Could not open dynamic state file"),
        // read access, no write access
        StateFileAccessRightsTestParams(true, false,
                                        "Could not open dynamic state file")));

#endif

////////////////////////////////////////////
// Bootstrap tests
////////////////////////////////////////////

class StateFileDirectoryBootstrapTest : public StateFileTest {
  void SetUp() override { StateFileTest::SetUp(); }
};

/**
 * @test
 *      Verify that state file gets correctly created with proper access rights
 * in case of directory bootstrap.
 */
TEST_F(StateFileDirectoryBootstrapTest, DirectoryBootstrapTest) {
  TempDirectory temp_test_dir;

  prepare_config_dir_with_default_certs(temp_test_dir.name());

  SCOPED_TRACE("// Launch our metadata server we bootstrap against");

  const auto metadata_server_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();

  mock_server_spawner().spawn(mock_server_cmdline("bootstrap_gr.js")
                                  .port(metadata_server_port)
                                  .http_port(http_port)
                                  .args());
  set_mock_metadata(http_port, "00000000-0000-0000-0000-0000000000g1",
                    classic_ports_to_gr_nodes({metadata_server_port}), 0,
                    {metadata_server_port});

  SCOPED_TRACE("// Bootstrap against our metadata server");
  std::vector<std::string> router_cmdline{
      "--bootstrap=localhost:" + std::to_string(metadata_server_port), "-d",
      temp_test_dir.name()};
  auto &router = launch_router_for_bootstrap(router_cmdline, EXIT_SUCCESS);

  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_SUCCESS));

  // check the state file that was produced, if it contains
  // what the bootstrap server has reported
  const std::string state_file = temp_test_dir.name() + "/data/state.json";
  check_state_file(state_file, ClusterType::GR_V2,
                   "00000000-0000-0000-0000-0000000000g1",
                   {metadata_server_port});

  // check that static file has a proper reference to the dynamic file
  const std::string conf_content =
      get_file_output("mysqlrouter.conf", temp_test_dir.name());
  const std::vector<std::string> lines =
      mysql_harness::split_string(conf_content, '\n');

  ASSERT_THAT(lines,
              ::testing::Contains(::testing::HasSubstr(
                  "dynamic_state=" + Path(state_file).real_path().str())));
}

/*
 * These tests are executed only for STANDALONE layout and are not executed for
 * Windows. Bootstrap for layouts different than STANDALONE use directories to
 * which tests don't have access (see install_layout.cmake).
 */
#ifndef SKIP_BOOTSTRAP_SYSTEM_DEPLOYMENT_TESTS

class StateFileSystemBootstrapTest : public StateFileTest,
                                     public RouterSystemLayout {
  void SetUp() override {
    StateFileTest::SetUp();
    RouterSystemLayout::init_system_layout_dir(get_mysqlrouter_exec(),
                                               ProcessManager::get_origin());
    set_mysqlrouter_exec(Path(exec_file_));
  }

  void TearDown() override { RouterSystemLayout::cleanup_system_layout(); }
};

/**
 * @test
 *      Verify that state file gets correctly created with proper access rights
 * in case of system (non-directory) bootstrap.
 */
TEST_F(StateFileSystemBootstrapTest, SystemBootstrapTest) {
  SCOPED_TRACE("// Launch our metadata server we bootstrap against");

  const auto metadata_server_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();

  mock_server_spawner().spawn(mock_server_cmdline("bootstrap_gr.js")
                                  .port(metadata_server_port)
                                  .http_port(http_port)
                                  .args());

  set_mock_metadata(http_port, "00000000-0000-0000-0000-0000000000g1",
                    classic_ports_to_gr_nodes({metadata_server_port}), 0,
                    {metadata_server_port});

  SCOPED_TRACE("// Bootstrap against our metadata server");
  std::vector<std::string> router_cmdline{"--bootstrap=localhost:" +
                                          std::to_string(metadata_server_port)};
  auto &router = launch_router_for_bootstrap(router_cmdline, EXIT_SUCCESS);

  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_SUCCESS));

  // check the state file that was produced, if it contains
  // what the bootstrap server has reported
  const std::string state_file =
      RouterSystemLayout::tmp_dir_ + "/stage/var/lib/mysqlrouter/state.json";

  check_state_file(state_file, ClusterType::GR_V2,
                   "00000000-0000-0000-0000-0000000000g1",
                   {metadata_server_port});
}

#endif  // SKIP_BOOTSTRAP_SYSTEM_DEPLOYMENT_TESTS

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
