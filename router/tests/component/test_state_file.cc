/*
Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>
#include "gmock/gmock.h"
#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql_session.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/rest_client.h"
#include "router_component_system_layout.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <thread>

using mysqlrouter::ClusterType;
using mysqlrouter::MySQLSession;
using ::testing::PrintToString;
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

constexpr auto kTTL = std::chrono::milliseconds(100);
}  // namespace

class StateFileTest : public RouterComponentTest {
 protected:
  void SetUp() override {
    RouterComponentTest::SetUp();
    // this test modifies the origin path so we need to restore it
    ProcessManager::set_origin(g_origin_path);
  }

  std::string get_metadata_cache_section(
      uint16_t metadata_server_port = 0,
      const std::chrono::milliseconds ttl = kTTL,
      ClusterType cluster_type = ClusterType::GR_V2) {
    auto ttl_str = std::to_string(std::chrono::duration<double>(ttl).count());
    const std::string cluster_type_str =
        (cluster_type == ClusterType::RS_V2) ? "rs" : "gr";

    return "[metadata_cache:test]\n"
           "cluster_type=" +
           cluster_type_str +
           "\n"
           "router_id=1\n" +
           ((metadata_server_port == 0)
                ? ""
                : "bootstrap_server_addresses=mysql://localhost:" +
                      std::to_string(metadata_server_port) + "\n") +
           "user=mysql_router1_user\n"
           "metadata_cluster=test\n"
           "connect_timeout=1\n"
           "ttl=" +
           ttl_str + "\n\n";
  }

  std::string get_metadata_cache_routing_section(uint16_t router_port,
                                                 const std::string &role,
                                                 const std::string &strategy,
                                                 const std::string &mode = "") {
    std::string result =
        "[routing:test_default]\n"
        "bind_port=" +
        std::to_string(router_port) + "\n" +
        "destinations=metadata-cache://test/default?role=" + role + "\n" +
        "protocol=classic\n";

    if (!strategy.empty())
      result += std::string("routing_strategy=" + strategy + "\n");
    if (!mode.empty()) result += std::string("mode=" + mode + "\n");

    return result;
  }

  auto &launch_router(const std::string &temp_test_dir,
                      const std::string &metadata_cache_section,
                      const std::string &routing_section,
                      const std::string &state_file_path,
                      const int expected_errorcode = EXIT_SUCCESS) {
    const std::string masterkey_file =
        Path(temp_test_dir).join("master.key").str();
    const std::string keyring_file = Path(temp_test_dir).join("keyring").str();
    mysql_harness::init_keyring(keyring_file, masterkey_file, true);
    mysql_harness::Keyring *keyring = mysql_harness::get_keyring();
    keyring->store("mysql_router1_user", "password", "root");
    mysql_harness::flush_keyring();
    mysql_harness::reset_keyring();

    // enable debug logs for better diagnostics in case of failure
    std::string logger_section = "[logger]\nlevel = DEBUG\n";

    // launch the router with metadata-cache configuration
    auto default_section = get_DEFAULT_defaults();
    default_section["keyring_path"] = keyring_file;
    default_section["master_key_path"] = masterkey_file;
    default_section["dynamic_state"] = state_file_path;
    const std::string conf_file = create_config_file(
        temp_test_dir,
        logger_section + metadata_cache_section + routing_section,
        &default_section);
    auto &router = ProcessManager::launch_router(
        {"-c", conf_file}, expected_errorcode, /*catch_stderr=*/true,
        /*with_sudo=*/false);
    return router;
  }

  void check_state_file(const std::string &state_file,
                        const std::string &expected_gr_name,
                        const std::vector<std::string> expected_gr_nodes) {
    const std::string state_file_content =
        get_file_output(state_file, true /*throw on error*/);
    JsonDocument json_doc;
    json_doc.Parse(state_file_content.c_str());
    constexpr const char *kExpectedVersion = "1.0.0";

    EXPECT_TRUE(json_doc.HasMember("version")) << state_file_content;
    EXPECT_TRUE(json_doc["version"].IsString()) << state_file_content;
    EXPECT_STREQ(kExpectedVersion, json_doc["version"].GetString())
        << state_file_content;

    EXPECT_TRUE(json_doc.HasMember("metadata-cache")) << state_file_content;
    EXPECT_TRUE(json_doc["metadata-cache"].IsObject()) << state_file_content;

    auto metadata_cache_section = json_doc["metadata-cache"].GetObject();

    EXPECT_TRUE(metadata_cache_section.HasMember("group-replication-id"))
        << state_file_content;
    EXPECT_TRUE(metadata_cache_section["group-replication-id"].IsString())
        << state_file_content;
    EXPECT_STREQ(expected_gr_name.c_str(),
                 metadata_cache_section["group-replication-id"].GetString())
        << state_file_content;

    EXPECT_TRUE(metadata_cache_section.HasMember("cluster-metadata-servers"))
        << state_file_content;
    EXPECT_TRUE(metadata_cache_section["cluster-metadata-servers"].IsArray())
        << state_file_content;
    auto cluster_nodes =
        metadata_cache_section["cluster-metadata-servers"].GetArray();
    EXPECT_EQ(expected_gr_nodes.size(), cluster_nodes.Size())
        << state_file_content;
    for (unsigned i = 0; i < cluster_nodes.Size(); ++i) {
      EXPECT_TRUE(cluster_nodes[i].IsString()) << state_file_content;
      EXPECT_STREQ(expected_gr_nodes[i].c_str(), cluster_nodes[i].GetString())
          << state_file_content;
    }

    // check that we have write access to the file
    // just append it with an empty line, that will not break it
    EXPECT_NO_THROW({
      std::ofstream ofs(state_file, std::ios::app);
      ofs << "\n";
    });
  }

  TcpPortPool port_pool_;
};

//////////////////////////////////////////////////////////////////////////

class StateFileDynamicChangesTest : public StateFileTest {
 protected:
  void SetUp() override { StateFileTest::SetUp(); }

  void kill_server(ProcessWrapper *server) {
    EXPECT_NO_THROW(server->kill()) << server->get_full_output();
  }
};

struct StateFileTestParam {
 public:
  std::string description;
  std::string trace_file;
  ClusterType cluster_type;
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

  SCOPED_TRACE(
      "// Launch 3 server mocks that will act as our metadata servers");
  const auto trace_file = get_data_dir().join(param.trace_file).str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));
    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(*cluster_nodes[i], cluster_nodes_ports[i]));
    ASSERT_TRUE(MockServerRestClient(cluster_http_ports[i])
                    .wait_for_rest_endpoint_ready())
        << cluster_nodes[i]->get_full_output();

    SCOPED_TRACE(
        "// Make our metadata server to return single node as a replicaset "
        "member (meaning single metadata server)");

    set_mock_metadata(cluster_http_ports[i], kGroupId,
                      std::vector<uint16_t>{cluster_nodes_ports[i]});
  }

  SCOPED_TRACE("// Create a router state file with a single metadata server");
  // clang-format off
  const std::string state_file =
      create_state_file(temp_test_dir.name(),
                        "{"
                          "\"version\": \"1.0.0\","
                          "\"metadata-cache\": {"
                            "\"group-replication-id\": " "\"" + kGroupId + "\","
                            "\"cluster-metadata-servers\": ["
                              "\"mysql://127.0.0.1:" +
                                 std::to_string(cluster_nodes_ports[0]) + "\""
                            "]"
                          "}"
                        "}");
  // clang-format on

  SCOPED_TRACE(
      "// Create a configuration file sections with low ttl so that any "
      "changes we make in the mock server via http port were refreshed "
      "quickly");
  const std::string metadata_cache_section =
      get_metadata_cache_section(0, kTTL, param.cluster_type);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE(
      "// Wait a few ttl periods to make sure the metadata_cache has the "
      "current metadata from our metadata server");
  std::this_thread::sleep_for(std::chrono::milliseconds(10 * kTTL));

  SCOPED_TRACE(
      "// Check our state file content, it should not change yet, there is "
      "single metadata server reported as initially");

  check_state_file(
      state_file, kGroupId,
      {"mysql://127.0.0.1:" + std::to_string(cluster_nodes_ports[0])});

  SCOPED_TRACE(
      "// Now change the response from the metadata server to return 3 gr "
      "nodes (metadata servers)");
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    set_mock_metadata(cluster_http_ports[i], kGroupId, cluster_nodes_ports);
  }

  SCOPED_TRACE(
      "// Wait a few ttl periods to make sure the metadata_cache has the "
      "current metadata from our metadata server");
  std::this_thread::sleep_for(std::chrono::milliseconds(10 * kTTL));

  SCOPED_TRACE(
      "// Check our state file content, it should now contain 3 metadata "
      "servers");

  check_state_file(
      state_file, kGroupId,
      {"mysql://127.0.0.1:" + std::to_string(cluster_nodes_ports[0]),
       "mysql://127.0.0.1:" + std::to_string(cluster_nodes_ports[1]),
       "mysql://127.0.0.1:" + std::to_string(cluster_nodes_ports[2])});

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
                    {cluster_nodes_ports[1], cluster_nodes_ports[2]});
  set_mock_metadata(cluster_http_ports[2], kGroupId,
                    {cluster_nodes_ports[1], cluster_nodes_ports[2]});

  SCOPED_TRACE("// Kill first metada server");
  kill_server(cluster_nodes[0]);

  SCOPED_TRACE(
      "// Wait a few ttl periods to make sure the metadata_cache has the "
      "current metadata from the second metadata server");
#ifdef _WIN32
  // On windows the mysql_real_connect that we use, will take about 2 seconds to
  // figure out it needs to try another metadata server
  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
#elif defined(__sparc__)
  // It also takes quite a long time on Sparc Solaris
  std::this_thread::sleep_for(std::chrono::milliseconds(10000));
#else
  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
#endif

  SCOPED_TRACE(
      "// Check our state file content, it should now contain 2 metadata "
      "servers reported by the second metadata server");
  check_state_file(
      state_file, kGroupId,
      {"mysql://127.0.0.1:" + std::to_string(cluster_nodes_ports[1]),
       "mysql://127.0.0.1:" + std::to_string(cluster_nodes_ports[2])});
}

INSTANTIATE_TEST_CASE_P(
    MetadataServersChangedInRuntime,
    StateFileMetadataServersChangedInRuntimeTest,
    ::testing::Values(StateFileTestParam{"gr", "metadata_dynamic_nodes.js",
                                         ClusterType::GR_V1},
                      StateFileTestParam{"gr_v2",
                                         "metadata_dynamic_nodes_v2_gr.js",
                                         ClusterType::GR_V2},
                      StateFileTestParam{"ar_v2",
                                         "metadata_dynamic_nodes_v2_ar.js",
                                         ClusterType::RS_V2}),
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
  const auto trace_file = get_data_dir().join(param.trace_file).str();
  auto &cluster_node(ProcessManager::launch_mysql_server_mock(
      trace_file, cluster_node_port, EXIT_SUCCESS, false, cluster_http_port));
  ASSERT_NO_FATAL_FAILURE(check_port_ready(cluster_node, cluster_node_port));
  ASSERT_TRUE(
      MockServerRestClient(cluster_http_port).wait_for_rest_endpoint_ready())
      << cluster_node.get_full_output();

  SCOPED_TRACE(
      "// Make our metadata server return single node as a replicaset "
      "member (meaning single metadata server)");

  set_mock_metadata(cluster_http_port, kGroupId,
                    std::vector<uint16_t>{cluster_node_port});

  SCOPED_TRACE("// Create a router state file with a single metadata server");
  // clang-format off
 const std::string state_file =
     create_state_file(temp_test_dir.name(),
                       "{"
                         "\"version\": \"1.0.0\","
                         "\"metadata-cache\": {"
                           "\"group-replication-id\": " "\"" + kGroupId + "\","
                           "\"cluster-metadata-servers\": ["
                             "\"mysql://127.0.0.1:" +
                                std::to_string(cluster_node_port) + "\""
                           "]"
                         "}"
                       "}");
  // clang-format on

  SCOPED_TRACE("// Create a configuration file with low ttl");
  const std::string metadata_cache_section =
      get_metadata_cache_section(0, kTTL, param.cluster_type);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router with the initial state file");
  auto &router = launch_router(temp_test_dir.name(), metadata_cache_section,
                               routing_section, state_file);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port));

  SCOPED_TRACE(
      "// Wait a few ttl periods to make sure the metadata_cache has the "
      "current metadata from our metadata server");
  std::this_thread::sleep_for(std::chrono::milliseconds(3 * kTTL));

  // kill our single instance server
  kill_server(&cluster_node);

  SCOPED_TRACE(
      "// Wait a few ttl periods to make sure the refresh has been called at "
      "least once");
#ifdef _WIN32
  // On windows the mysql_real_connect that we use, will take about 2 seconds to
  // figure out it needs to try another metadata server
  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
#elif defined(__sparc__)
  // It also takes quite a long time on Sparc Solaris
  std::this_thread::sleep_for(std::chrono::milliseconds(10000));
#else
  std::this_thread::sleep_for(std::chrono::milliseconds(5 * kTTL));
#endif

  SCOPED_TRACE(
      "// Check our state file content, it should still contain out metadata "
      "server");
  check_state_file(state_file, kGroupId,
                   {"mysql://127.0.0.1:" + std::to_string(cluster_node_port)});
}

INSTANTIATE_TEST_CASE_P(
    MetadataServersInaccessible, StateFileMetadataServersInaccessibleTest,
    ::testing::Values(StateFileTestParam{"gr", "metadata_dynamic_nodes.js",
                                         ClusterType::GR_V1},
                      StateFileTestParam{"gr_v2",
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

  SCOPED_TRACE("// Launch  server mock that will act as our metadata server");
  const auto trace_file = get_data_dir().join(param.trace_file).str();
  auto &cluster_node = ProcessManager::launch_mysql_server_mock(
      trace_file, cluster_node_port, EXIT_SUCCESS, false, cluster_http_port);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(cluster_node, cluster_node_port));
  ASSERT_TRUE(
      MockServerRestClient(cluster_http_port).wait_for_rest_endpoint_ready())
      << cluster_node.get_full_output();

  SCOPED_TRACE(
      "// Make our metadata server to return single node as a replicaset "
      "member (meaning single metadata server)");

  set_mock_metadata(cluster_http_port, kClusterFileGroupId,
                    std::vector<uint16_t>{cluster_node_port});

  SCOPED_TRACE(
      "// Create a router state file with a single metadata server and "
      "group-replication-id different than the one reported by the "
      "mock-server");

  // clang-format off
  const std::string state_file =
      create_state_file(temp_test_dir.name(),
                        "{"
                          "\"version\": \"1.0.0\","
                          "\"metadata-cache\": {"
                            "\"group-replication-id\": " "\"" + std::string(kStateFileGroupId) + "\","
                            "\"cluster-metadata-servers\": ["
                              "\"mysql://127.0.0.1:" +
                                 std::to_string(cluster_node_port) + "\""
                            "]"
                          "}"
                        "}");
  // clang-format on

  SCOPED_TRACE(
      "// Create a configuration file sections with low ttl so that any "
      "changes we make in the mock server via http port were refreshed "
      "quickly");
  const std::string metadata_cache_section =
      get_metadata_cache_section(0, kTTL, param.cluster_type);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router with the initial state file");
  auto &router = launch_router(temp_test_dir.name(), metadata_cache_section,
                               routing_section, state_file);

  SCOPED_TRACE(
      "// Wait a few ttl periods to make sure the metadata_cache has the "
      "current metadata from our metadata server");
  std::this_thread::sleep_for(std::chrono::milliseconds(10 * kTTL));

  SCOPED_TRACE(
      "// Check our state file content, it should not change. "
      "We did not found the data for our replication group on any of the "
      "servers so we do not update the metadata srever list.");

  check_state_file(state_file, kStateFileGroupId,
                   {"mysql://127.0.0.1:" + std::to_string(cluster_node_port)});

  SCOPED_TRACE("// We expect an error in the logfile");
  auto log_content = router.get_full_logfile();
  EXPECT_TRUE(
      log_content.find(
          "Failed fetching metadata from any of the 1 metadata servers") !=
      std::string::npos)
      << log_content << "\n";

  // now try to connect to the router port, we expect error 2003
  std::string out_port_unused;
  EXPECT_NO_THROW(connect_client_and_query_port(router_port, out_port_unused,
                                                /*should_fail=*/true));
}

INSTANTIATE_TEST_CASE_P(
    GroupReplicationIdDiffers, StateFileGroupReplicationIdDiffersTest,
    ::testing::Values(StateFileTestParam{"gr", "metadata_dynamic_nodes.js",
                                         ClusterType::GR_V1},
                      StateFileTestParam{"gr_v2",
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
  const auto trace_file = get_data_dir().join(param.trace_file).str();
  for (unsigned i = 0; i < kNodesNum; i++) {
    const auto port_connect = cluster_node_ports[i].first;
    const auto port_http = cluster_node_ports[i].second;
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, port_connect, EXIT_SUCCESS, false, port_http));
    ASSERT_NO_FATAL_FAILURE(check_port_ready(*cluster_nodes[i], port_connect));
    ASSERT_TRUE(MockServerRestClient(port_http).wait_for_rest_endpoint_ready())
        << cluster_nodes[i]->get_full_output();
  }

  SCOPED_TRACE(
      "// let's configure the metadata so that there are 2 groups that do not "
      "know about each other (split brain)");

  std::vector<uint16_t> fist_group{cluster_node_ports[0].first,
                                   cluster_node_ports[1].first};
  for (unsigned i = 0; i < 1; i++) {
    const auto port_http = cluster_node_ports[i].second;
    set_mock_metadata(port_http, kClusterGroupId, fist_group);
  }

  std::vector<uint16_t> second_group{cluster_node_ports[2].first};
  for (unsigned i = 2; i < kNodesNum; i++) {
    const auto port_http = cluster_node_ports[i].second;
    set_mock_metadata(port_http, kMockServerGlobalsRestUri, second_group);
  }

  SCOPED_TRACE(
      "// Create a router state file with all the nodes as a "
      "cluster-metadata-servers ");
  std::string all_nodes_list;
  for (unsigned i = 0; i < kNodesNum; ++i) {
    const auto port_connect = cluster_node_ports[i].first;
    all_nodes_list +=
        "\"mysql://127.0.0.1:" + std::to_string(port_connect) + "\"";
    if (i != kNodesNum - 1) all_nodes_list += ", ";
  }

  // clang-format off
  const std::string state_file =
      create_state_file(temp_test_dir.name(),
                        "{"
                          "\"version\": \"1.0.0\","
                          "\"metadata-cache\": {"
                            "\"group-replication-id\": \"" + kClusterGroupId + "\","
                            "\"cluster-metadata-servers\": ["
                                 + all_nodes_list  +
                            "]"
                          "}"
                        "}");
  // clang-format on

  SCOPED_TRACE(
      "// Create a configuration file sections with low ttl so that any "
      "changes we make in the mock server via http port were refreshed "
      "quickly");
  const std::string metadata_cache_section =
      get_metadata_cache_section(0, kTTL, param.cluster_type);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router with the initial state file");
  launch_router(temp_test_dir.name(), metadata_cache_section, routing_section,
                state_file);

  SCOPED_TRACE(
      "// Wait a few ttl periods to make sure the metadata_cache has the "
      "current metadata from our metadata server");
  std::this_thread::sleep_for(std::chrono::milliseconds(10 * kTTL));

  SCOPED_TRACE(
      "// Check our state file content, it should now contain only the nodes "
      "from the first group.");

  std::vector<std::string> expected_gr_nodes;
  for (unsigned i = 0; i < 2; ++i) {
    const auto port_connect = cluster_node_ports[i].first;
    expected_gr_nodes.push_back("mysql://127.0.0.1:" +
                                std::to_string(port_connect));
  }
  check_state_file(state_file, kClusterGroupId, expected_gr_nodes);

  SCOPED_TRACE(
      "// Try to connect to the router port, we expect first port from the "
      "first group.");
  std::string port_connected;
  EXPECT_NO_THROW(connect_client_and_query_port(router_port, port_connected,
                                                /*should_fail=*/false));
  EXPECT_STREQ(std::to_string(cluster_node_ports[0].first).c_str(),
               port_connected.c_str());
}

INSTANTIATE_TEST_CASE_P(
    SplitBrainScenario, StateFileSplitBrainScenarioTest,
    ::testing::Values(StateFileTestParam{"gr", "metadata_dynamic_nodes.js",
                                         ClusterType::GR_V1},
                      StateFileTestParam{"gr_v2",
                                         "metadata_dynamic_nodes_v2_gr.js",
                                         ClusterType::GR_V2}),
    get_test_description);

/**
 * @test
 *      Verify that in case of empty metada-server-addess list in the state file
 * the Router logs proper error and accepts (and errors out) the client
 * connections.
 */
TEST_F(StateFileDynamicChangesTest, EmptyMetadataServersList) {
  constexpr const char kGroupId[] = "3a0be5af-0022-11e8-9655-0800279e6a88";

  TempDirectory temp_test_dir;

  SCOPED_TRACE("// Create a router state file with empty server list");
  // clang-format off
  const std::string state_file =
      create_state_file(temp_test_dir.name(),
                        "{"
                          "\"version\": \"1.0.0\","
                          "\"metadata-cache\": {"
                            "\"group-replication-id\": \"" + std::string(kGroupId) + "\","
                            "\"cluster-metadata-servers\": []"
                          "}"
                        "}");
  // clang-format on

  SCOPED_TRACE(
      "// Create a configuration file sections with low ttl so that any "
      "changes we make in the mock server via http port were refreshed "
      "quickly");
  const std::string metadata_cache_section =
      get_metadata_cache_section(0, kTTL);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router with the initial state file");
  auto &router = launch_router(temp_test_dir.name(), metadata_cache_section,
                               routing_section, state_file, EXIT_FAILURE);

  wait_for_port_ready(router_port);

  SCOPED_TRACE(
      "// Wait a few ttl periods to make sure the metadata_cache tried "
      "to refresh the metadata");
  std::this_thread::sleep_for(3 * kTTL);

  // proper error should get logged
  const bool found = find_in_file(
      get_logging_dir().str() + "/mysqlrouter.log",
      [&](const std::string &line) -> bool {
        return pattern_found(
            line,
            "'bootstrap_server_addresses' is the configuration file is empty "
            "or not set and list of 'cluster-metadata-servers' in "
            "'dynamic_config'-file is empty, too.");
      },
      std::chrono::milliseconds(0));
  EXPECT_TRUE(found) << router.get_full_logfile();

  // now try to connect to the router port, we expect error 2003
  std::string out_port_unused;
  EXPECT_NO_THROW(connect_client_and_query_port(router_port, out_port_unused,
                                                /*should_fail=*/true));
}

//////////////////////////////////////////////////////////////////////////

struct StateFileSchemaTestParams {
  std::string state_file_content;
  std::vector<std::string> expected_errors_in_log;
  bool create_state_file_from_content{true};
  std::string state_file_path{""};
  bool use_static_server_list{false};
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
  virtual void SetUp() { StateFileTest::SetUp(); }
};

/**
 * @test
 *      Verify that the proper error gets logged and the Router shuts down in
 * case of various configuration mimatches.
 */
TEST_P(StateFileSchemaTest, ParametrizedStateFileSchemaTest) {
  auto test_params = GetParam();

  TempDirectory temp_test_dir;

  const uint16_t md_server_port =
      test_params.use_static_server_list ? port_pool_.get_next_available() : 0;
  const uint16_t router_port = port_pool_.get_next_available();

  // launch the router with static metadata-cache configuration and
  // dynamic state file configured via test parameter
  const std::string metadata_cache_section = get_metadata_cache_section(
      md_server_port, kTTL, test_params.cluster_type);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  const std::string state_file =
      test_params.create_state_file_from_content
          ? create_state_file(temp_test_dir.name(),
                              test_params.state_file_content)
          : test_params.state_file_path;

  auto &router = launch_router(temp_test_dir.name(), metadata_cache_section,
                               routing_section, state_file, EXIT_FAILURE);

  // the router should close with non-0 return value
  check_exit_code(router, EXIT_FAILURE);
  EXPECT_THAT(router.exit_code(), testing::Ne(0));

  // proper log should get logged
  auto log_content = router.get_full_logfile();
  for (const auto &expeted_in_log : test_params.expected_errors_in_log) {
    EXPECT_TRUE(log_content.find(expeted_in_log) != std::string::npos)
        << log_content << "\n";
  }
}

INSTANTIATE_TEST_CASE_P(
    StateFileTests, StateFileSchemaTest,
    ::testing::Values(
        // state file does not exits
        StateFileSchemaTestParams{
            "",
            {"Could not open dynamic state file 'non-existing.json' "
             "for "
             "reading"},
            false, /* = don't create state file, use the path given */
            "non-existing.json"},

        // state file path empty
        StateFileSchemaTestParams{
            "", {"Could not open dynamic state file '' for reading"},
            false, /* = don't create state file, use the empty path given */
            ""},

        // state file containing invalid non-json data
        StateFileSchemaTestParams{"some invalid, non-json content",
                                  {"Error parsing file dynamic state file",
                                   "Parsing JSON failed at offset 0"}},

        // state file content is not an object
        StateFileSchemaTestParams{"[]",
                                  {"Invalid json structure: not an object"}},

        // version field missing
        StateFileSchemaTestParams{
            // clang-format off
            "{"
              "\"metadata-cache\": {"
                "\"group-replication-id\": \"3a0be5af-994c-11e8-9655-0800279e6a88\","
                "\"cluster-metadata-servers\": ["
                  "\"mysql://localhost:5000\","
                  "\"mysql://127.0.0.1:5001\""
                "]"
              "}"
            "}",
            // clang-format on
            {"Invalid json structure: missing field: version"}},

        // version field is not a string
        StateFileSchemaTestParams{
            // clang-format off
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
            // clang-format on
            {"Invalid json structure: field version "
             "should be a string type"}},

        // version field is non numeric string
        StateFileSchemaTestParams{
            // clang-format off
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
            // clang-format on
            {"Invalid version field format, expected MAJOR.MINOR.PATCH, "
             "found: str"}},

        // version field has wrong number of numeric values
        StateFileSchemaTestParams{
            // clang-format off
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
            // clang-format on
            {"Invalid version field format, expected MAJOR.MINOR.PATCH, "
             "found: 1.0"}},

        // major version does not match (GR cluster)
        StateFileSchemaTestParams{
            // clang-format off
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
            // clang-format on
            {"Unsupported state file version, "
             "expected: 1.0.0, found: 2.0.0"}},

        // major version does not match (AR cluster)
        StateFileSchemaTestParams{
            // clang-format off
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
            // clang-format on
            {"Unsupported state file version, "
             "expected: 1.0.0, found: 2.0.0"}, true, "", false,
             ClusterType::RS_V2},

        // minor version does not match
        StateFileSchemaTestParams{            // clang-format off
        "{"
          "\"version\": \"1.1.0\","
          "\"metadata-cache\": {"
            "\"group-replication-id\": \"3a0be5af-994c-11e8-9655-0800279e6a88\","
            "\"cluster-metadata-servers\": ["
              "\"mysql://localhost:5000\","
              "\"mysql://127.0.0.1:5001\""
            "]"
          "}"
        "}",
        // clang-format on
        {"Unsupported state file version, "
         "expected: 1.0.0, found: 1.1.0"}},

        // both bootstrap_server_addresses and dynamic_state configured
        StateFileSchemaTestParams{
        // clang-format off
        "{"
          "\"version\": \"1.0.0\","
          "\"metadata-cache\": {"
            "\"group-replication-id\": \"3a0be5af-994c-11e8-9655-0800279e6a88\","
            "\"cluster-metadata-servers\": ["
              "\"mysql://localhost:5000\","
              "\"mysql://127.0.0.1:5001\""
            "]"
          "}"
        "}",
        // clang-format on
        {"bootstrap_server_addresses is not allowed when dynamic "
         "state file is used"},
        true, "",
        true /*use static bootstrap_server_addresses in static conf. file*/},

        // group-replication-id filed missing
        StateFileSchemaTestParams{
        // clang-format off
        "{"
          "\"version\": \"1.0.0\","
          "\"metadata-cache\": {"
            "\"cluster-metadata-servers\": ["
              "\"mysql://localhost:5000\","
              "\"mysql://127.0.0.1:5001\""
            "]"
          "}"
        "}",
        // clang-format on
        {"JSON file failed validation against JSON schema: Failed schema "
         "directive: #/properties/metadata-cache",
         "Failed schema keyword:   required",
         "Failure location in validated document: #/metadata-cache"}},

        // cluster-metadata-servers filed missing (GR cluster)
        StateFileSchemaTestParams{
        // clang-format off
        "{"
          "\"version\": \"1.0.0\","
          "\"metadata-cache\": {"
            "\"group-replication-id\": \"3a0be5af-994c-11e8-9655-0800279e6a88\""
          "}"
        "}",
        // clang-format on
        {"JSON file failed validation against JSON schema: Failed schema "
         "directive: #/properties/metadata-cache",
         "Failed schema keyword:   required",
         "Failure location in validated document: #/metadata-cache"}},

         // cluster-metadata-servers filed missing (AR cluster)
         StateFileSchemaTestParams{
            // clang-format off
            "{"
              "\"version\": \"1.0.0\","
              "\"metadata-cache\": {"
                "\"group-replication-id\": \"3a0be5af-994c-11e8-9655-0800279e6a88\""
              "}"
            "}",
            // clang-format on
            {"JSON file failed validation against JSON schema: Failed schema "
             "directive: #/properties/metadata-cache",
             "Failed schema keyword:   required",
             "Failure location in validated document: #/metadata-cache"}},

        // both bootstrap_server_addresses and dynamic_state configured
        // dynamic_state file not existing
            StateFileSchemaTestParams{
            "",
            {"bootstrap_server_addresses is not allowed when dynamic "
             "state file is used"},
            false, /* = don't create state file, use the path given */
            "non-existing.json",
            true /*use static bootstrap_server_addresses in static conf. file*/}));

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
  virtual void SetUp() { StateFileTest::SetUp(); }
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
  const std::string metadata_cache_section = get_metadata_cache_section();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  // clang-format off
  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      "{"
        "\"version\": \"1.0.0\","
        "\"metadata-cache\": {"
          "\"group-replication-id\": \"000-000\","
          "\"cluster-metadata-servers\": [\"mysql://127.0.0.1:10000\"]"
       "}}");
  // clang-format on
  mode_t file_mode = 0;
  if (test_params.read_access) file_mode |= S_IRUSR;
  if (test_params.write_access) file_mode |= S_IWUSR;
  chmod(state_file.c_str(), file_mode);

  auto &router = launch_router(temp_test_dir.name(), metadata_cache_section,
                               routing_section, state_file, EXIT_FAILURE);

  // the router should close with non-0 return value
  check_exit_code(router, EXIT_FAILURE);
  EXPECT_THAT(router.exit_code(), testing::Ne(0));

  // proper error should get logged
  const bool found = find_in_file(
      get_logging_dir().str() + "/mysqlrouter.log",
      [&](const std::string &line) -> bool {
        return pattern_found(line, test_params.expected_error);
      },
      std::chrono::milliseconds(1));

  EXPECT_TRUE(found) << router.get_full_logfile();
}

INSTANTIATE_TEST_CASE_P(
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
  virtual void SetUp() { StateFileTest::SetUp(); }
};

/**
 * @test
 *      Verify that state file gets correctly created with proper access rights
 * in case of directory bootstrap.
 */
TEST_F(StateFileDirectoryBootstrapTest, DirectoryBootstrapTest) {
  TempDirectory temp_test_dir;

  SCOPED_TRACE("// Launch our metadata server we bootsrtap against");

  const auto trace_file = get_data_dir().join("bootstrap_gr.js").str();
  const auto metadata_server_port = port_pool_.get_next_available();
  auto &md_server = ProcessManager::launch_mysql_server_mock(
      trace_file, metadata_server_port, EXIT_SUCCESS, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(md_server, metadata_server_port));

  SCOPED_TRACE("// Bootstrap against our metadata server");
  std::vector<std::string> router_cmdline{
      "--bootstrap=localhost:" + std::to_string(metadata_server_port), "-d",
      temp_test_dir.name()};
  auto &router = ProcessManager::launch_router(router_cmdline);
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  check_exit_code(router, EXIT_SUCCESS, 1s);

  // check the state file that was produced, if it constains
  // what the bootstrap server has reported
  const std::string state_file = temp_test_dir.name() + "/data/state.json";
  check_state_file(state_file, "replication-1",
                   {"mysql://localhost:5500", "mysql://localhost:5510",
                    "mysql://localhost:5520"});

  // check that static file has a proper reference to the dynamic file
  const std::string static_conf = temp_test_dir.name() + "/mysqlrouter.conf";
  const std::string expected_entry =
      std::string("dynamic_state=") + Path(state_file).real_path().str();
  const bool found = find_in_file(
      static_conf,
      [&](const std::string &line) -> bool {
        return pattern_found(line, expected_entry);
      },
      std::chrono::milliseconds(1));

  EXPECT_TRUE(found) << "Did not found: " << expected_entry << "\n"
                     << get_file_output("mysqlrouter.conf",
                                        temp_test_dir.name());
}

/*
 * These tests are executed only for STANDALONE layout and are not executed for
 * Windows. Bootstrap for layouts different than STANDALONE use directories to
 * which tests don't have access (see install_layout.cmake).
 */
#ifndef SKIP_BOOTSTRAP_SYSTEM_DEPLOYMENT_TESTS

class StateFileSystemBootstrapTest : public StateFileTest,
                                     public RouterSystemLayout {
  virtual void SetUp() {
    StateFileTest::SetUp();
    RouterSystemLayout::init_system_layout_dir(get_mysqlrouter_exec(),
                                               ProcessManager::get_origin());
    set_mysqlrouter_exec(Path(exec_file_));
  }

  virtual void TearDown() { RouterSystemLayout::cleanup_system_layout(); }
};

/**
 * @test
 *      Verify that state file gets correctly created with proper access rights
 * in case of system (non-directory) bootstrap.
 */
TEST_F(StateFileSystemBootstrapTest, SystemBootstrapTest) {
  SCOPED_TRACE("// Launch our metadata server we bootsrtap against");

  const auto trace_file = get_data_dir().join("bootstrap_gr.js").str();
  const auto metadata_server_port = port_pool_.get_next_available();
  auto &md_server = ProcessManager::launch_mysql_server_mock(
      trace_file, metadata_server_port, EXIT_SUCCESS, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(md_server, metadata_server_port));

  SCOPED_TRACE("// Bootstrap against our metadata server");
  std::vector<std::string> router_cmdline{"--bootstrap=localhost:" +
                                          std::to_string(metadata_server_port)};
  auto &router = ProcessManager::launch_router(router_cmdline);
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  check_exit_code(router, EXIT_SUCCESS, 1s);

  // check the state file that was produced, if it constains
  // what the bootstrap server has reported
  const std::string state_file =
      RouterSystemLayout::tmp_dir_ + "/stage/var/lib/mysqlrouter/state.json";
  check_state_file(state_file, "replication-1",
                   {"mysql://localhost:5500", "mysql://localhost:5510",
                    "mysql://localhost:5520"});
}

#endif  // SKIP_BOOTSTRAP_SYSTEM_DEPLOYMENT_TESTS

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
