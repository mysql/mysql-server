/*
Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
#include "mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "router_component_system_layout.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <thread>

Path g_origin_path;
using ::testing::PrintToString;
using mysqlrouter::MySQLSession;

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
  virtual void SetUp() {
    set_origin(g_origin_path);
    RouterComponentTest::init();
  }

  std::string get_metadata_cache_section(
      uint16_t metadata_server_port = 0,
      const std::chrono::milliseconds ttl = kTTL) {
    auto ttl_str = std::to_string(std::chrono::duration<double>(ttl).count());
    return "[metadata_cache:test]\n"
           "router_id=1\n" +
           ((metadata_server_port == 0)
                ? ""
                : "bootstrap_server_addresses=mysql://localhost:" +
                      std::to_string(metadata_server_port) + "\n") +
           "user=mysql_router1_user\n"
           "metadata_cluster=test\n"
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

  // returns full path to the file
  std::string create_state_file(const std::string &dir_name,
                                const std::string &content) {
    Path file_path = Path(dir_name).join("state.json");
    std::ofstream ofs_config(file_path.str());

    if (!ofs_config.good()) {
      throw(
          std::runtime_error("Could not create state file " + file_path.str()));
    }

    ofs_config << content;
    ofs_config.close();

    return file_path.str();
  }

  RouterComponentTest::CommandHandle launch_router(
      const std::string &temp_test_dir,
      const std::string &metadata_cache_section,
      const std::string &routing_section, const std::string &state_file_path) {
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
    auto router = RouterComponentTest::launch_router(
        "-c " + conf_file, /*catch_stderr=*/true, /*with_sudo=*/false);
    return router;
  }

  void check_state_file(const std::string &state_file,
                        const std::string &expected_gr_name,
                        const std::vector<std::string> expected_gr_nodes) {
    const std::string state_file_content = get_file_output(state_file);
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
    ASSERT_EQ(expected_gr_nodes.size(), cluster_nodes.Size())
        << state_file_content << get_router_log_output();
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

class StateFileDynamicChangesTest : public StateFileTest,
                                    public ::testing::Test {
 protected:
  virtual void SetUp() { StateFileTest::SetUp(); }

  std::string to_string(const JsonValue &json_doc) {
    JsonStringBuffer out_buffer;

    rapidjson::Writer<JsonStringBuffer> out_writer{out_buffer};
    json_doc.Accept(out_writer);
    return out_buffer.GetString();
  }

  void set_mock_metadata(uint16_t http_port, const std::string &gr_id,
                         const std::vector<uint16_t> &gr_node_ports) {
    JsonValue json_doc(rapidjson::kObjectType);
    JsonAllocator allocator;
    json_doc.AddMember("gr_id", JsonValue(gr_id.c_str(), gr_id.length()),
                       allocator);

    JsonValue gr_nodes_json(rapidjson::kArrayType);
    for (auto &gr_node : gr_node_ports) {
      JsonValue node(rapidjson::kArrayType);
      node.PushBack(JsonValue((int)gr_node), allocator);
      node.PushBack(JsonValue("ONLINE", strlen("ONLINE")), allocator);
      gr_nodes_json.PushBack(node, allocator);
    }
    json_doc.AddMember("gr_nodes", gr_nodes_json, allocator);

    const auto json_str = to_string(json_doc);

    EXPECT_NO_THROW(MockServerRestClient(http_port).set_globals(json_str));
  }

  void kill_server(RouterComponentTest::CommandHandle &server) {
    EXPECT_NO_THROW(server.kill()) << server.get_full_output();
  }
};

/**
 * @test
 *      Verify that changes in the cluster topology are reflected in the state
 * file in the runtime.
 */
TEST_F(StateFileDynamicChangesTest, MetadataServersChangedInRuntime) {
  const std::string kGroupId = "3a0be5af-0022-11e8-9655-0800279e6a88";

  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  const unsigned CLUSTER_NODES = 3;
  std::vector<RouterComponentTest::CommandHandle> cluster_nodes;
  std::vector<uint16_t> cluster_nodes_ports;
  std::vector<uint16_t> cluster_http_ports;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE(
      "// Launch 2 server mocks that will act as our metadata servers");
  // we do not launch the third one as it will never be queried in this test
  // scenario
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes.js").str();
  for (unsigned i = 0; i < 2; ++i) {
    cluster_nodes.push_back(RouterComponentTest::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], false, cluster_http_ports[i]));
    ASSERT_TRUE(wait_for_port_ready(cluster_nodes_ports[i], 1000))
        << cluster_nodes[i].get_full_output();
    ASSERT_TRUE(MockServerRestClient(cluster_http_ports[i])
                    .wait_for_rest_endpoint_ready())
        << cluster_nodes[i].get_full_output();
  }

  SCOPED_TRACE(
      "// Make our metadata server to return single node as a replicaset "
      "member (meaning single metadata server)");

  set_mock_metadata(cluster_http_ports[0], kGroupId,
                    std::vector<uint16_t>{cluster_nodes_ports[0]});

  SCOPED_TRACE("// Create a router state file with a single metadata server");
  // clang-format off
  const std::string state_file =
      create_state_file(temp_test_dir,
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
      get_metadata_cache_section(0, kTTL);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router with the initial state file");
  auto router = launch_router(temp_test_dir, metadata_cache_section,
                              routing_section, state_file);

  SCOPED_TRACE(
      "// Wait a few ttl periods to make sure the metadata_cache has the "
      "current metadata from our metadata server");
  std::this_thread::sleep_for(std::chrono::milliseconds(3 * kTTL));

  SCOPED_TRACE(
      "// Check our state file content, it should not change yet, there is "
      "single metadata server reported as initially");

  check_state_file(
      state_file, kGroupId,
      {"mysql://127.0.0.1:" + std::to_string(cluster_nodes_ports[0])});

  SCOPED_TRACE(
      "// Now change the response from the metadata server to return 3 gr "
      "nodes (metadata servers)");
  set_mock_metadata(cluster_http_ports[0], kGroupId, cluster_nodes_ports);

  SCOPED_TRACE(
      "// Wait a few ttl periods to make sure the metadata_cache has the "
      "current metadata from our metadata server");
  std::this_thread::sleep_for(std::chrono::milliseconds(3 * kTTL));

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
      "// Instrument the second metadata server to return 2 servers: second "
      "and third");
  set_mock_metadata(cluster_http_ports[1], kGroupId,
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
  std::this_thread::sleep_for(std::chrono::milliseconds(5 * kTTL));
#endif

  SCOPED_TRACE(
      "// Check our state file content, it should now contain 2 metadata "
      "servers reported by the second metadata server");
  check_state_file(
      state_file, kGroupId,
      {"mysql://127.0.0.1:" + std::to_string(cluster_nodes_ports[1]),
       "mysql://127.0.0.1:" + std::to_string(cluster_nodes_ports[2])});
}

/* @test
 *      Verify that if not metadata server can't be accessed the list of the
 * server does not get cleared.
 */
TEST_F(StateFileDynamicChangesTest, MetadataServersInaccessible) {
  const std::string kGroupId = "3a0be5af-0022-11e8-9655-0800279e6a88";

  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  uint16_t cluster_node_port = port_pool_.get_next_available();
  uint16_t cluster_http_port = port_pool_.get_next_available();

  SCOPED_TRACE(
      "// Launch single server mock that will act as our metadata server");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes.js").str();
  RouterComponentTest::CommandHandle cluster_node(
      RouterComponentTest::launch_mysql_server_mock(
          trace_file, cluster_node_port, false, cluster_http_port));
  ASSERT_TRUE(wait_for_port_ready(cluster_node_port, 1000))
      << cluster_node.get_full_output();
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
     create_state_file(temp_test_dir,
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
      get_metadata_cache_section(0, kTTL);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router with the initial state file");
  auto router = launch_router(temp_test_dir, metadata_cache_section,
                              routing_section, state_file);
  ASSERT_TRUE(wait_for_port_ready(router_port, 1000))
      << router.get_full_output();

  SCOPED_TRACE(
      "// Wait a few ttl periods to make sure the metadata_cache has the "
      "current metadata from our metadata server");
  std::this_thread::sleep_for(std::chrono::milliseconds(3 * kTTL));

  // kill our single instance server
  kill_server(cluster_node);

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

/**
 * @test
 *      Verify that if the metadata servers do not know about the replication
 * group id that was bootstrapped against, Router does not use metadata for
 * routing, logs an error but does not change the metadata servers list in the
 * dynamic state file.
 */
TEST_F(StateFileDynamicChangesTest, GroupReplicationIdDiffers) {
  constexpr const char kStateFileGroupId[] =
      "3a0be5af-0022-11e8-0000-0800279e6a88";
  constexpr const char kClusterFileGroupId[] =
      "3a0be5af-0022-11e8-0000-0800279e6a88";

  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  auto cluster_node_port = port_pool_.get_next_available();
  auto cluster_http_port = port_pool_.get_next_available();

  SCOPED_TRACE("// Launch  server mock that will act as our metadata server");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes.js").str();
  auto cluster_node = RouterComponentTest::launch_mysql_server_mock(
      trace_file, cluster_node_port, false, cluster_http_port);
  ASSERT_TRUE(wait_for_port_ready(cluster_node_port, 1000))
      << cluster_node.get_full_output();
  ASSERT_TRUE(
      MockServerRestClient(cluster_http_port).wait_for_rest_endpoint_ready())
      << cluster_node.get_full_output();

  SCOPED_TRACE(
      "// Make our metadata server to return single node as a replicaset "
      "member (meaning single metadata server)");

  set_mock_metadata(cluster_http_port, kMockServerGlobalsRestUri,
                    std::vector<uint16_t>{cluster_node_port});

  SCOPED_TRACE(
      "// Create a router state file with a single metadata server and "
      "group-replication-id different than the one reported by the "
      "mock-server");

  // clang-format off
  const std::string state_file =
      create_state_file(temp_test_dir,
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
      get_metadata_cache_section(0, kTTL);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router with the initial state file");
  auto router = launch_router(temp_test_dir, metadata_cache_section,
                              routing_section, state_file);

  SCOPED_TRACE(
      "// Wait a few ttl periods to make sure the metadata_cache has the "
      "current metadata from our metadata server");
  std::this_thread::sleep_for(std::chrono::milliseconds(3 * kTTL));

  SCOPED_TRACE(
      "// Check our state file content, it should not change. "
      "We did not found the data for our replication group on any of the "
      "servers so we do not update the metadata srever list.");

  check_state_file(state_file, kClusterFileGroupId,
                   {"mysql://127.0.0.1:" + std::to_string(cluster_node_port)});

  SCOPED_TRACE("// We expect an error in the logfile");
  auto log_content = get_router_log_output();
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

/**
 * @test
 *      Verify that if the split brain scenario the list of the metadata servers
 * gets updated properly in the state file.
 */
TEST_F(StateFileDynamicChangesTest, SplitBrainScenario) {
  const std::string kClusterGroupId = "3a0be5af-0022-11e8-0000-0800279e6a88";
  const unsigned kNodesNum = 3;  // number of nodes in the cluster
                                 //  TcpPortPool
  //      cluster_ports_pool;  // currently TcpPortPool supports max 10 ports so
  //                           // we create dedicated one for our cluster

  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  std::vector<RouterComponentTest::CommandHandle> cluster_nodes;
  std::vector<std::pair<uint16_t, uint16_t>>
      cluster_node_ports;  // pair of connection and http port

  for (unsigned i = 0; i < kNodesNum; i++) {
    cluster_node_ports.push_back(
        {port_pool_.get_next_available(), port_pool_.get_next_available()});
  }

  SCOPED_TRACE("// Launch  server mocks that play as our split brain cluster");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes.js").str();
  for (unsigned i = 0; i < kNodesNum; i++) {
    const auto port_connect = cluster_node_ports[i].first;
    const auto port_http = cluster_node_ports[i].second;
    cluster_nodes.push_back(RouterComponentTest::launch_mysql_server_mock(
        trace_file, port_connect, false, port_http));
    ASSERT_TRUE(wait_for_port_ready(port_connect, 1000))
        << cluster_nodes[i].get_full_output();
    ASSERT_TRUE(MockServerRestClient(port_http).wait_for_rest_endpoint_ready())
        << cluster_nodes[i].get_full_output();
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
      create_state_file(temp_test_dir,
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
      get_metadata_cache_section(0, kTTL);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router with the initial state file");
  auto router = launch_router(temp_test_dir, metadata_cache_section,
                              routing_section, state_file);

  SCOPED_TRACE(
      "// Wait a few ttl periods to make sure the metadata_cache has the "
      "current metadata from our metadata server");
  std::this_thread::sleep_for(std::chrono::milliseconds(3 * kTTL));

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

/**
 * @test
 *      Verify that in case of empty metada-server-addess list in the state file
 * the Router logs proper error and accepts (and errors out) the client
 * connections.
 */
TEST_F(StateFileDynamicChangesTest, EmptyMetadataServersList) {
  constexpr const char kGroupId[] = "3a0be5af-0022-11e8-9655-0800279e6a88";

  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  SCOPED_TRACE("// Create a router state file with empty server list");
  // clang-format off
  const std::string state_file =
      create_state_file(temp_test_dir,
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
  auto router = launch_router(temp_test_dir, metadata_cache_section,
                              routing_section, state_file);

  wait_for_port_ready(router_port, 3000);

  SCOPED_TRACE(
      "// Wait a few ttl periods to make sure the metadata_cache tried "
      "to refresh the metadata");
  std::this_thread::sleep_for(std::chrono::milliseconds(3 * kTTL));

  // proper error should get logged
  const bool found = find_in_file(
      get_logging_dir().str() + "/mysqlrouter.log",
      [&](const std::string &line) -> bool {
        return pattern_found(
            line,
            "Failed fetching metadata from any of the 0 metadata servers");
      },
      std::chrono::milliseconds(0));
  EXPECT_TRUE(found) << get_router_log_output();

  // now try to connect to the router port, we expect error 2003
  std::string out_port_unused;
  EXPECT_NO_THROW(connect_client_and_query_port(router_port, out_port_unused,
                                                /*should_fail=*/true));
}

//////////////////////////////////////////////////////////////////////////

struct StateFileSchemaTestParams {
  std::string state_file_content;
  std::vector<std::string> expected_errors_in_log;
  bool create_state_file_from_content;
  std::string state_file_path;
  bool use_static_server_list;

  StateFileSchemaTestParams(
      const std::string &state_file_content_,
      const std::vector<std::string> &expected_errors_in_log_,
      bool create_state_file_from_content_ =
          true, /* otherwise use state_file_path */
      const std::string &state_file_path_ = "",
      bool use_static_server_list_ = false)
      : state_file_content(state_file_content_),
        expected_errors_in_log(expected_errors_in_log_),
        create_state_file_from_content(create_state_file_from_content_),
        state_file_path(state_file_path_),
        use_static_server_list(use_static_server_list_) {}
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
      public ::testing::TestWithParam<StateFileSchemaTestParams> {
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

  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  const uint16_t md_server_port =
      test_params.use_static_server_list ? port_pool_.get_next_available() : 0;
  const uint16_t router_port = port_pool_.get_next_available();

  // launch the router with static metadata-cache configuration and
  // dynamic state file configured via test parameter
  const std::string metadata_cache_section =
      get_metadata_cache_section(md_server_port);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  const std::string state_file =
      test_params.create_state_file_from_content
          ? create_state_file(temp_test_dir, test_params.state_file_content)
          : test_params.state_file_path;

  auto router = launch_router(temp_test_dir, metadata_cache_section,
                              routing_section, state_file);

  // the router should close with non-0 return value
  EXPECT_EQ(router.wait_for_exit(), 1);
  EXPECT_THAT(router.exit_code(), testing::Ne(0));

  // proper log should get logged
  auto log_content = get_router_log_output();
  for (const auto &expeted_in_log : test_params.expected_errors_in_log) {
    EXPECT_TRUE(log_content.find(expeted_in_log) != std::string::npos)
        << log_content << "\n";
  }
}

INSTANTIATE_TEST_CASE_P(
    StateFileTests, StateFileSchemaTest,
    ::testing::Values(
        // state file does not exits
        StateFileSchemaTestParams(
            "",
            {"Could not open dynamic state file 'non-existing.json' "
             "for "
             "reading"},
            false, /* = don't create state file, use the path given */
            "non-existing.json"),

        // state file path empty
        StateFileSchemaTestParams(
            "", {"Could not open dynamic state file '' for reading"},
            false, /* = don't create state file, use the empty path given */
            ""),

        // state file containing invalid non-json data
        StateFileSchemaTestParams("some invalid, non-json content",
                                  {"Error parsing file dynamic state file",
                                   "Parsing JSON failed at offset 0"}),

        // state file content is not an object
        StateFileSchemaTestParams("[]",
                                  {"Invalid json structure: not an object"}),

        // version field missing
        StateFileSchemaTestParams(
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
            {"Invalid json structure: missing field: version"}),

        // version field is not a string
        StateFileSchemaTestParams(
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
             "should be a string type"}),

        // version field is non numeric string
        StateFileSchemaTestParams(
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
             "found: str"}),

        // version field has wrong number of numeric values
        StateFileSchemaTestParams(
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
             "found: 1.0"}),

        // major version does not match
        StateFileSchemaTestParams(
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
             "expected: 1.0.0, found: 2.0.0"}),

        // minor version does not match
        StateFileSchemaTestParams(
            // clang-format off
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
             "expected: 1.0.0, found: 1.1.0"}),

        // both bootstrap_server_addresses and dynamic_state configured
        StateFileSchemaTestParams(
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
            true /*use static bootstrap_server_addresses in static conf. file*/),

        // group-replication-id filed missing
        StateFileSchemaTestParams(
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
             "Failure location in validated document: #/metadata-cache"}),

        // cluster-metadata-servers filed missing
        StateFileSchemaTestParams(
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
             "Failure location in validated document: #/metadata-cache"}),

        // both bootstrap_server_addresses and dynamic_state configured
        // dynamic_state file not existing
        StateFileSchemaTestParams(
            "",
            {"bootstrap_server_addresses is not allowed when dynamic "
             "state file is used"},
            false, /* = don't create state file, use the path given */
            "non-existing.json",
            true /*use static bootstrap_server_addresses in static conf. file*/)));

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
      public ::testing::TestWithParam<StateFileAccessRightsTestParams> {
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

  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  const uint16_t router_port = port_pool_.get_next_available();

  // launch the router with static metadata-cache configuration and
  // dynamic state file configured via test parameter
  const std::string metadata_cache_section = get_metadata_cache_section();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  // clang-format off
  const std::string state_file = create_state_file(
      temp_test_dir,
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

  auto router = launch_router(temp_test_dir, metadata_cache_section,
                              routing_section, state_file);

  // the router should close with non-0 return value
  EXPECT_EQ(router.wait_for_exit(), 1);
  EXPECT_THAT(router.exit_code(), testing::Ne(0));

  // proper error should get logged
  const bool found =
      find_in_file(get_logging_dir().str() + "/mysqlrouter.log",
                   [&](const std::string &line) -> bool {
                     return pattern_found(line, test_params.expected_error);
                   },
                   std::chrono::milliseconds(1));

  EXPECT_TRUE(found) << get_router_log_output();
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

class StateFileDirectoryBootstrapTest : public StateFileTest,
                                        public ::testing::Test {
  virtual void SetUp() { StateFileTest::SetUp(); }
};

/**
 * @test
 *      Verify that state file gets correctly created with proper access rights
 * in case of directory bootstrap.
 */
TEST_F(StateFileDirectoryBootstrapTest, DirectoryBootstrapTest) {
  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  SCOPED_TRACE("// Launch our metadata server we bootsrtap against");

  const auto trace_file = get_data_dir().join("bootstrap.js").str();
  const auto metadata_server_port = port_pool_.get_next_available();
  auto md_server = RouterComponentTest::launch_mysql_server_mock(
      trace_file, metadata_server_port, false);
  ASSERT_TRUE(wait_for_port_ready(metadata_server_port, 1000))
      << md_server.get_full_output();

  SCOPED_TRACE("// Bootstrap against our metadata server");
  std::string router_cmdline =
      "--bootstrap=localhost:" + std::to_string(metadata_server_port) + " -d " +
      temp_test_dir;
  auto router = RouterComponentTest::launch_router(router_cmdline);
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // wait_for_exit() throws at timeout.
  EXPECT_NO_THROW(EXPECT_EQ(router.wait_for_exit(1000),
                            /*expected_exitcode*/ 0))
      << router.get_full_output();

  // check the state file that was produced, if it constains
  // what the bootstrap server has reported
  const std::string state_file = temp_test_dir + "/data/state.json";
  check_state_file(state_file, "replication-1",
                   {"mysql://localhost:5500", "mysql://localhost:5510",
                    "mysql://localhost:5520"});

  // check that static file has a proper reference to the dynamic file
  const std::string static_conf = temp_test_dir + "/mysqlrouter.conf";
  const std::string expected_entry =
      std::string("dynamic_state=") + Path(state_file).real_path().str();
  const bool found = find_in_file(static_conf,
                                  [&](const std::string &line) -> bool {
                                    return pattern_found(line, expected_entry);
                                  },
                                  std::chrono::milliseconds(1));

  EXPECT_TRUE(found) << "Did not found: " << expected_entry << "\n"
                     << get_file_output("mysqlrouter.conf", temp_test_dir);
}

/*
 * These tests are executed only for STANDALONE layout and are not executed for
 * Windows. Bootstrap for layouts different than STANDALONE use directories to
 * which tests don't have access (see install_layout.cmake).
 */
#ifndef SKIP_BOOTSTRAP_SYSTEM_DEPLOYMENT_TESTS

class StateFileSystemBootstrapTest : public StateFileTest,
                                     public RouterSystemLayout,
                                     public ::testing::Test {
  virtual void SetUp() {
    StateFileTest::SetUp();
    RouterSystemLayout::init_system_layout_dir(get_mysqlrouter_exec(),
                                               g_origin_path);
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

  const auto trace_file = get_data_dir().join("bootstrap.js").str();
  const auto metadata_server_port = port_pool_.get_next_available();
  auto md_server = RouterComponentTest::launch_mysql_server_mock(
      trace_file, metadata_server_port, false);
  ASSERT_TRUE(wait_for_port_ready(metadata_server_port, 1000))
      << md_server.get_full_output();

  SCOPED_TRACE("// Bootstrap against our metadata server");
  std::string router_cmdline =
      "--bootstrap=localhost:" + std::to_string(metadata_server_port);
  auto router = RouterComponentTest::launch_router(router_cmdline);
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // wait_for_exit() throws at timeout.
  EXPECT_NO_THROW(EXPECT_EQ(router.wait_for_exit(1000),
                            /*expected_exitcode*/ 0))
      << router.get_full_output();

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
