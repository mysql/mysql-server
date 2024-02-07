/*
Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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
#include <fstream>
#include <stdexcept>
#include <thread>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <gmock/gmock.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql/harness/stdx/ranges.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "tcp_port_pool.h"

using mysqlrouter::ClusterType;
using mysqlrouter::MySQLSession;
using namespace std::chrono_literals;

using testing::ElementsAre;

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
}  // namespace

class AsyncReplicasetTest : public RouterComponentTest {
 protected:
  void SetUp() override {
    RouterComponentTest::SetUp();
    // this test modifies the origin path so we need to restore it
    ProcessManager::set_origin(g_origin_path);
  }

  std::string get_metadata_cache_section(
      const std::chrono::milliseconds ttl = kTTL,
      const std::string &cluster_type_str = "rs") {
    auto ttl_str = std::to_string(std::chrono::duration<double>(ttl).count());

    return "[metadata_cache:test]\n"
           "cluster_type=" +
           cluster_type_str +
           "\n"
           "router_id=1\n"
           "user=mysql_router1_user\n"
           "metadata_cluster=test\n"
           "connect_timeout=1\n"
           "ttl=" +
           ttl_str + "\n\n";
  }

  std::string get_metadata_cache_routing_section(
      uint16_t router_port, const std::string &role,
      const std::string &strategy,
      bool disconnect_on_metadata_unavailable = false,
      bool disconnect_on_promoted_to_primary = false) {
    std::string disconnect_rules;
    if (disconnect_on_metadata_unavailable) {
      disconnect_rules += "&disconnect_on_metadata_unavailable=yes";
    }
    if (disconnect_on_promoted_to_primary) {
      disconnect_rules += "&disconnect_on_promoted_to_primary=yes";
    }
    std::string result =
        "[routing:test_default" + std::to_string(router_port) +
        "]\n"
        "bind_port=" +
        std::to_string(router_port) + "\n" +
        "destinations=metadata-cache://test/default?role=" + role +
        disconnect_rules + "\n" + "protocol=classic\n";

    if (!strategy.empty())
      result += std::string("routing_strategy=" + strategy + "\n");

    return result;
  }

  auto &launch_router(const std::string &temp_test_dir,
                      const std::string &metadata_cache_section,
                      const std::string &routing_section,
                      const std::string &state_file_path,
                      const int expected_errorcode = EXIT_SUCCESS,
                      std::chrono::milliseconds wait_for_notify_ready = 30s) {
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
        /*with_sudo=*/false, wait_for_notify_ready);
    return router;
  }

  void set_mock_metadata(uint16_t http_port, const std::string &cluster_id,
                         const std::vector<uint16_t> &cluster_node_ports,
                         unsigned primary_id = 0, uint64_t view_id = 0,
                         bool error_on_md_query = false,
                         bool empty_result_from_cluster_type_query = false,
                         bool is_gr_cluster = false, unsigned gr_pos = 0) {
    const auto gr_nodes = is_gr_cluster
                              ? classic_ports_to_gr_nodes(cluster_nodes_ports)
                              : std::vector<GRNode>{};
    auto cluster_nodes = classic_ports_to_cluster_nodes(cluster_node_ports);
    for (auto [i, node] : stdx::views::enumerate(cluster_nodes)) {
      if (i == primary_id) {
        node.role = "PRIMARY";
      } else {
        node.role = "SECONDARY";
      }
    }

    auto json_doc =
        mock_GR_metadata_as_json(cluster_id, gr_nodes, gr_pos, cluster_nodes,
                                 view_id, error_on_md_query);

    // we can't allow this counter become undefined as that breaks the
    // wait_for_transaction_count_increase logic
    JsonAllocator allocator;
    json_doc.AddMember("md_query_count", 0, allocator);

    if (empty_result_from_cluster_type_query) {
      json_doc.AddMember("cluster_type", "", allocator);
    }

    const auto json_str = json_to_string(json_doc);

    EXPECT_NO_THROW(MockServerRestClient(http_port).set_globals(json_str));
  }

  static const std::chrono::milliseconds kTTL;
  static const std::string cluster_id;
  TempDirectory temp_test_dir;
  unsigned view_id = 1;

  std::vector<ProcessWrapper *> cluster_nodes;
  std::vector<uint16_t> cluster_nodes_ports;
  std::vector<uint16_t> cluster_http_ports;
};

const std::chrono::milliseconds AsyncReplicasetTest::kTTL = 50ms;
const std::string AsyncReplicasetTest::cluster_id =
    "3a0be5af-0022-11e8-9655-0800279e6a88";

//////////////////////////////////////////////////////////////////////////

/**
 * @test TS_R-FR2_1
 *
 */
TEST_F(AsyncReplicasetTest, NoChange) {
  const unsigned CLUSTER_NODES = 3;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// Launch 3 server mocks that will act as our cluster members");
  const auto trace_file_primary =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  const auto trace_file_secondary =
      get_data_dir().join("metadata_only_view_id_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    const auto trace_file = i == 0 ? trace_file_primary : trace_file_secondary;
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE(
        "// Make our metadata server to return all 3 nodes as a cluster "
        "members");

    // each member should report the same view_id (=1)
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports, 0,
                      view_id);
  }

  SCOPED_TRACE("// Create a router state file with all of the members");
  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE(
      "// Create a configuration file sections with low ttl so that any "
      "changes we make in the mock server via http port were refreshed "
      "quickly");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should not change, there is "
      "single metadata server reported as initially");

  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);
}

/**
 * @test TS_R-FR2.1.1_1, TS_FR4.3_1, TS_R-EX_1
 */
TEST_F(AsyncReplicasetTest, SecondaryAdded) {
  const unsigned CLUSTER_NODES = 3;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// Launch 3 server mocks that will act as our cluster members");
  const auto trace_file_primary =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  const auto trace_file_secondary =
      get_data_dir().join("metadata_only_view_id_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    const auto trace_file = i == 0 ? trace_file_primary : trace_file_secondary;
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE(
        "// Make our metadata server to return all 3 nodes a s a cluster "
        "members");

    // the primary only knows about the first secondaruy first
    set_mock_metadata(cluster_http_ports[i], cluster_id,
                      {cluster_nodes_ports[0], cluster_nodes_ports[1]}, 0,
                      view_id);
  }

  SCOPED_TRACE("// Create a router state file the 2 members");
  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(
          cluster_id, "", {cluster_nodes_ports[0], cluster_nodes_ports[1]},
          view_id));

  SCOPED_TRACE(
      "// Create a configuration file sections with low ttl so that any "
      "changes we make in the mock server via http port were refreshed "
      "quickly");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", "round-robin");

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should first contain only 2 "
      "members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   {cluster_nodes_ports[0], cluster_nodes_ports[1]}, view_id);

  SCOPED_TRACE("// Make a connection to the secondary");
  auto client1 = make_new_connection_ok(router_port_ro, cluster_nodes_ports[1]);

  SCOPED_TRACE(
      "// Now let's change the md on the PRIMARY adding 2nd SECONDARY, also "
      "bumping view_id");
  set_mock_metadata(cluster_http_ports[0], cluster_id, cluster_nodes_ports, 0,
                    view_id + 1);

  SCOPED_TRACE("// Wait untill the router sees this change");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should now contain all 3 members "
      "and increased view_id");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id + 1);

  SCOPED_TRACE("// Check that the existing connection is still alive");
  verify_existing_connection_ok(client1.get());

  SCOPED_TRACE("// Check that newly added node is used for ro connections ");
  /*auto client2 =*/make_new_connection_ok(router_port_ro,
                                           cluster_nodes_ports[1]);
  /*auto client3 =*/make_new_connection_ok(router_port_ro,
                                           cluster_nodes_ports[2]);
}

/**
 * @test TS_R-FR2.1.1_2
 */
TEST_F(AsyncReplicasetTest, SecondaryRemovedStillReachable) {
  const unsigned CLUSTER_NODES = 3;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// Launch 3 server mocks that will act as our cluster members");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE(
        "// Make our metadata server to return all 3 nodes as a cluster "
        "members");

    // all 3 are cluster members at the beginning
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports, 0,
                      view_id);
  }

  SCOPED_TRACE("// Create a router state file the 3 members");
  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE(
      "// Create a configuration file sections with low ttl so that any "
      "changes we make in the mock server via http port were refreshed "
      "quickly");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available");
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", "round-robin");

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should first contain all 3 members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);

  SCOPED_TRACE(
      "// Let's make a connection to the both secondaries, both should be "
      "successful");
  auto client1 = make_new_connection_ok(router_port_ro, cluster_nodes_ports[1]);
  auto client2 = make_new_connection_ok(router_port_ro, cluster_nodes_ports[2]);

  SCOPED_TRACE(
      "// Now let's change the md on the first SECONDARY removing 2nd "
      "SECONDARY, also bumping it's view_id");
  set_mock_metadata(cluster_http_ports[1], cluster_id,
                    {cluster_nodes_ports[0], cluster_nodes_ports[1]}, 0,
                    view_id + 1);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[1], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should now contain only 2 members "
      "and increased view_id");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   {cluster_nodes_ports[0], cluster_nodes_ports[1]},
                   view_id + 1);

  SCOPED_TRACE(
      "// The connection to the first secondary should still be alive, the "
      "connection to the second secondary should be dropped");
  verify_existing_connection_ok(client1.get());
  EXPECT_TRUE(wait_connection_dropped(*client2.get()));
}

/**
 * @test TS_R-FR2.2_1
 */
TEST_F(AsyncReplicasetTest, ClusterIdChanged) {
  const std::string changed_cluster_id = "4b0be5af-0022-11e8-9655-0800279e6a99";
  const unsigned CLUSTER_NODES = 3;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// Launch 3 server mocks that will act as our cluster members");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE(
        "// Make our metadata server to return all 3 nodes as a cluster "
        "members");

    // all 3 are cluster members at the beginning
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports, 0,
                      view_id);
  }

  SCOPED_TRACE("// Create a router state file with 3 members");
  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE(
      "// Create a configuration file sections with low ttl so that any "
      "changes we make in the mock server via http port were refreshed "
      "quickly");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available");
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", "round-robin");

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should first contain all 3 members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);

  SCOPED_TRACE("// Now let's change the md on the PRIMARY: " + cluster_id +
               ", " + std::to_string(view_id) + " (cluster_id, view_id) to " +
               changed_cluster_id + ", " + std::to_string(view_id + 1));
  set_mock_metadata(cluster_http_ports[0], changed_cluster_id,
                    {cluster_nodes_ports[0], cluster_nodes_ports[1]}, 0,
                    view_id + 1);

  SCOPED_TRACE("// Wait until the router sees this change");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, not change, the PRIMARYs view of the "
      "world should not get into account as it contains different cluster_id");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);
}

/**
 * @test TS_R-FR2.2_1
 */
TEST_F(AsyncReplicasetTest, ClusterSecondaryQueryErrors) {
  const unsigned CLUSTER_NODES = 3;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// Launch 3 server mocks that will act as our cluster members");
  // the secondaries fail on metadata query
  const auto trace_file_ok =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  const auto trace_file_err =
      get_data_dir().join("metadata_error_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    const std::string trace_file = i == 0 ? trace_file_ok : trace_file_err;
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE(
        "// Make our metadata server to return all 3 nodes as a cluster "
        "members");
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports, 0,
                      view_id);
  }

  SCOPED_TRACE("// Create a router state file the 3 members");
  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE(
      "// Create a configuration file sections with low ttl so that any "
      "changes we make in the mock server via http port were refreshed "
      "quickly");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available");
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", "round-robin");

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  auto &router = launch_router(temp_test_dir.name(), metadata_cache_section,
                               routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[2], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should contain all 3 members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);

  SCOPED_TRACE(
      "// Check that there are warnings reported for not being able to fetch "
      "the metadata from both secondaries");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);
  const std::string log_content = router.get_logfile_content();

  for (size_t i = 1; i <= 2; i++) {
    const std::string pattern =
        "metadata_cache WARNING .* Failed fetching metadata from metadata "
        "server on 127.0.0.1:" +
        std::to_string(cluster_nodes_ports[i]);
    ASSERT_TRUE(pattern_found(log_content, pattern));
  }
}

/**
 * @test TS_R-FR2.2_2, TS_R-FR3_1
 */
TEST_F(AsyncReplicasetTest, MetadataUnavailableDisconnectFromSecondary) {
  const unsigned CLUSTER_NODES = 2;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// Launch 2 server mocks that will act as our cluster members");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE(
        "// Make our metadata server to return both nodes as a cluster "
        "members");
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports, 0,
                      view_id);
  }

  SCOPED_TRACE("// Create a router state file the 3 members");
  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE(
      "// Create a configuration file. disconnect_on_metadata_unavailable for "
      "R/W routing is false, for RO routing is true");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available",
      /*disconnect_on_metadata_unavailable=*/false);
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", "round-robin",
      /*disconnect_on_metadata_unavailable=*/true);

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should contain both members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);

  SCOPED_TRACE("// Let's make a connection to the both servers RW and RO");
  auto client1 = make_new_connection_ok(router_port_rw, cluster_nodes_ports[0]);
  auto client2 = make_new_connection_ok(router_port_ro, cluster_nodes_ports[1]);

  SCOPED_TRACE(
      "// Make both members to start returning errors on metadata query now");

  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports, 0,
                      view_id, /*error_on_md_query=*/true);
  }

  SCOPED_TRACE("// Wait untill the router sees this change");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[1], 2));

  SCOPED_TRACE(
      "// RW connection should have survived, RO one should have been closed");
  verify_existing_connection_ok(client1.get());
  EXPECT_TRUE(wait_connection_dropped(*client2.get()));

  SCOPED_TRACE(
      "// Make sure the state file did not change, it should still contain "
      "the 2 members.");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);
}

/**
 * @test TS_R-FR2.2_3, TS_R-FR2.2_4
 */
TEST_F(AsyncReplicasetTest, MetadataUnavailableDisconnectFromPrimary) {
  const unsigned CLUSTER_NODES = 2;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// Launch 2 server mocks that will act as our cluster members");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE(
        "// Make our metadata server to return both nodes as a cluster "
        "members");
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports, 0,
                      view_id);
  }

  SCOPED_TRACE("// Create a router state file the 3 members");
  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE(
      "// Create a configuration file. disconnect_on_metadata_unavailable for "
      "R/W routing is true, for RO routing is false");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available",
      /*disconnect_on_metadata_unavailable=*/true);
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", "round-robin",
      /*disconnect_on_metadata_unavailable=*/false);

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should contain both members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);

  SCOPED_TRACE("// Let's make a connection to the both servers RW and RO");
  auto client1 = make_new_connection_ok(router_port_rw, cluster_nodes_ports[0]);
  auto client2 = make_new_connection_ok(router_port_ro, cluster_nodes_ports[1]);

  SCOPED_TRACE(
      "// Make both members to start returning errors on metadata query now");

  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports, 0,
                      view_id, /*error_on_md_query=*/true);
  }

  SCOPED_TRACE("// Wait untill the router sees this change");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// RO connection should have survived, RW one should have been closed");
  EXPECT_TRUE(wait_connection_dropped(*client1.get()));
  verify_existing_connection_ok(client2.get());

  /////////////////////////////////////////
  // here comes the TS_R-FR2.2_4 part
  /////////////////////////////////////////
  SCOPED_TRACE(
      "// Make both members to STOP returning errors on metadata query now");
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports, 0,
                      view_id, /*error_on_md_query=*/false);
  }

  SCOPED_TRACE("// Wait untill the router sees this change");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE("// We should be able to connect to the PRIMARY again ");
  /*auto client3 =*/make_new_connection_ok(router_port_rw,
                                           cluster_nodes_ports[0]);
}

/**
 * @test TS_FR4.2_1
 */
TEST_F(AsyncReplicasetTest, MultipleChangesInTheCluster) {
  const unsigned CLUSTER_NODES = 4;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  std::vector<uint16_t> initial_cluster_members{
      cluster_nodes_ports[0], cluster_nodes_ports[1], cluster_nodes_ports[2]};

  SCOPED_TRACE(
      "// Launch 4 server mocks that will act as our (current and future) "
      "cluster members");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE(
        "// Make our metadata server to return first 3 nodes as a cluster "
        "members");
    set_mock_metadata(cluster_http_ports[i], cluster_id,
                      initial_cluster_members, 0, view_id);
  }

  SCOPED_TRACE(
      "// Let us start with 3 members (one PRIMARY and 2 SECONDARIES)");
  const std::string state_file =
      create_state_file(temp_test_dir.name(),
                        create_state_file_content(
                            cluster_id, "", initial_cluster_members, view_id));

  SCOPED_TRACE("// Create a configuration file.");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available");
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", "round-robin");

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should contain the initial members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   initial_cluster_members, view_id);

  SCOPED_TRACE("// Now let's mess a little bit with the metadata");
  // let's remove one of the nodes and add another one
  std::vector<uint16_t> new_cluster_members{
      cluster_nodes_ports[0], cluster_nodes_ports[2], cluster_nodes_ports[3]};

  // Let's let it know to the member2 also making it a new PRIMARY and bumping
  // up its view_id
  set_mock_metadata(cluster_http_ports[2], cluster_id, new_cluster_members,
                    /*primary_id=*/1, view_id + 1);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[2], 2));

  SCOPED_TRACE("// Check that the state file caught up with all those changes");
  // since the node 2 became a new PRIMARY it should be first metadata server on
  // the list now
  std::vector<uint16_t> md_servers_expected{
      cluster_nodes_ports[2], cluster_nodes_ports[0], cluster_nodes_ports[3]};
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   md_servers_expected, view_id + 1);
}

/**
 * @test TS_FR4.4_1
 */
TEST_F(AsyncReplicasetTest, SecondaryRemoved) {
  const unsigned CLUSTER_NODES = 3;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// Launch 3 server mocks that will act as our cluster members");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE(
        "// Make our metadata server initially return all 3 nodes as a cluster "
        "members");
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports, 0,
                      view_id);
  }

  SCOPED_TRACE(
      "// Let us start with 3 members (one PRIMARY and 2 SECONDARIES)");
  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE("// Create a configuration file.");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available");
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", "round-robin");

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should contain the initial members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);

  SCOPED_TRACE("// Make 2 RO connections, one for each SECONDARY");
  auto client1 = make_new_connection_ok(router_port_ro, cluster_nodes_ports[1]);
  auto client2 = make_new_connection_ok(router_port_ro, cluster_nodes_ports[2]);

  SCOPED_TRACE("// Now let's remove the second SECONDARY from the metadata");
  std::vector<uint16_t> new_cluster_members{cluster_nodes_ports[0],
                                            cluster_nodes_ports[1]};
  set_mock_metadata(cluster_http_ports[0], cluster_id, new_cluster_members,
                    /*primary_id=*/0, view_id + 1);

  SCOPED_TRACE("// Wait untill the router sees this change");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check that the state file does not contain the second SECONDARY "
      "anymore");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   new_cluster_members, view_id + 1);

  SCOPED_TRACE(
      "// Check that the existing connection to the second SECONDARY got "
      "dropped");
  verify_existing_connection_ok(client1.get());
  EXPECT_TRUE(wait_connection_dropped(*client2.get()));

  SCOPED_TRACE(
      "// Check that new RO connections are made to the first secondary");
  for (int i = 0; i < 2; i++) {
    /*auto client =*/make_new_connection_ok(router_port_ro,
                                            cluster_nodes_ports[1]);
  }
}

/**
 * @test TS_FR4.4_2
 */
TEST_F(AsyncReplicasetTest, NewPrimaryOldGone) {
  const unsigned CLUSTER_NODES = 3;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  std::vector<uint16_t> initial_cluster_members{cluster_nodes_ports[0],
                                                cluster_nodes_ports[1]};

  SCOPED_TRACE("// Launch 3 server mocks that will act as our cluster members");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE("// Let us start with 2 members (PRIMARY and SECONDARY)");
    set_mock_metadata(cluster_http_ports[i], cluster_id,
                      initial_cluster_members, 0, view_id);
  }

  const std::string state_file =
      create_state_file(temp_test_dir.name(),
                        create_state_file_content(
                            cluster_id, "", initial_cluster_members, view_id));

  SCOPED_TRACE("// Create a configuration file.");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available");
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", "round-robin");

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should contain the initial members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   initial_cluster_members, view_id);

  SCOPED_TRACE("// Make one RW and one RO connection");
  auto client_rw =
      make_new_connection_ok(router_port_rw, cluster_nodes_ports[0]);
  auto client_ro =
      make_new_connection_ok(router_port_ro, cluster_nodes_ports[1]);

  SCOPED_TRACE(
      "// Now let's remove old primary and promote a first secondary to become "
      "one");
  std::vector<uint16_t> new_cluster_members{cluster_nodes_ports[1],
                                            cluster_nodes_ports[2]};
  for (size_t i = 1; i <= 2; i++) {
    set_mock_metadata(cluster_http_ports[i], cluster_id, new_cluster_members,
                      /*primary_id=*/0, view_id + 1);
  }

  SCOPED_TRACE("// Wait untill the router sees this change");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[1], 2));

  SCOPED_TRACE("// Check that the state file is as expected");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   new_cluster_members, view_id + 1);

  SCOPED_TRACE(
      "// Check that the existing connection to the old PRIMARY got dropped");
  EXPECT_TRUE(wait_connection_dropped(*client_rw.get()));
  verify_existing_connection_ok(client_ro.get());

  SCOPED_TRACE("// Check that new RW connections is made to the new PRIMARY");
  /*auto client_rw2 =*/make_new_connection_ok(router_port_rw,
                                              cluster_nodes_ports[1]);
}

/**
 * @test TS_FR4.5_1, TS_R-EX_6
 */
TEST_F(AsyncReplicasetTest, NewPrimaryOldBecomesSecondary) {
  const unsigned CLUSTER_NODES = 3;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// Launch 3 server mocks that will act as our cluster members");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE("// Let us start with all 3 members (PRIMARY and SECONDARY)");
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports,
                      /*primary_id=*/0, view_id);
  }

  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE("// Create a configuration file.");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available");
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", "round-robin");

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should contain the initial members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);

  SCOPED_TRACE("// Make one RW and one RO connection");
  auto client_rw =
      make_new_connection_ok(router_port_rw, cluster_nodes_ports[0]);
  auto client_ro =
      make_new_connection_ok(router_port_ro, cluster_nodes_ports[1]);

  SCOPED_TRACE(
      "// Now let's change the primary from node[0] to node[1] and let "
      "announce it via the new PRIMARY");
  set_mock_metadata(cluster_http_ports[1], cluster_id, cluster_nodes_ports,
                    /*primary_id=*/1, view_id + 1);

  SCOPED_TRACE("// Wait untill the router sees this change");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[1], 2));

  SCOPED_TRACE(
      "// Check that the existing connection to the old PRIMARY got dropped "
      "and the ro connection to the new PRIMARY is still up");
  EXPECT_TRUE(wait_connection_dropped(*client_rw.get()));
  verify_existing_connection_ok(client_ro.get());

  SCOPED_TRACE("// Check that new RW connections is made to the new PRIMARY");
  /*auto client_rw2 =*/make_new_connection_ok(router_port_rw,
                                              cluster_nodes_ports[1]);
}

/**
 * @test TS_FR4.5_2, TS_R-EX_6,
 */
TEST_F(AsyncReplicasetTest, NewPrimaryOldBecomesSecondaryDisconnectOnPromoted) {
  const unsigned CLUSTER_NODES = 3;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// Launch 3 server mocks that will act as our cluster members");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE("// Let us start with all 3 members");
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports,
                      /*primary_id=*/0, view_id);
  }

  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE("// Create a configuration file.");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available");
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", "round-robin",
      /*disconnect_on_metadata_unavailable*/ false,
      /*disconnect_on_promoted_to_primary*/ true);

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should contain the initial members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);

  SCOPED_TRACE("// Make one RW and one RO connection");
  auto client_rw =
      make_new_connection_ok(router_port_rw, cluster_nodes_ports[0]);
  auto client_ro =
      make_new_connection_ok(router_port_ro, cluster_nodes_ports[1]);

  SCOPED_TRACE(
      "// Now let's change the primary from node[0] to node[1] and let "
      "announce it via the new PRIMARY");
  set_mock_metadata(cluster_http_ports[1], cluster_id, cluster_nodes_ports,
                    /*primary_id=*/1, view_id + 1);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[1], 2));

  SCOPED_TRACE("// Check that the state file is as expected");
  // since the primary has changed we expect that change reflected in the
  // metadata-servers order too
  check_state_file(
      state_file, ClusterType::RS_V2, cluster_id,
      {cluster_nodes_ports[1], cluster_nodes_ports[0], cluster_nodes_ports[2]},
      view_id + 1);

  SCOPED_TRACE("// Check that both RW and RO connections are down");
  EXPECT_TRUE(wait_connection_dropped(*client_rw.get()));
  EXPECT_TRUE(wait_connection_dropped(*client_ro.get()));

  SCOPED_TRACE("// Check that new RW connections is made to the new PRIMARY");
  /*auto client_rw2 =*/make_new_connection_ok(router_port_rw,
                                              cluster_nodes_ports[1]);
}

/**
 * @test TS_FR4.5_3
 */
TEST_F(AsyncReplicasetTest, OnlyPrimaryLeftAcceptsRWAndRO) {
  const unsigned CLUSTER_NODES = 2;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// Launch 2 server mocks that will act as our cluster members");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE("// Let us start with 2 members (PRIMARY and SECONDARY)");
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports,
                      /*primary_id=*/0, view_id);
  }

  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE("// Create a configuration file.");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available");
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "PRIMARY_AND_SECONDARY", "round-robin");

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should contain the initial members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);

  SCOPED_TRACE("// Make one RW and one RO connection");
  auto client_rw =
      make_new_connection_ok(router_port_rw, cluster_nodes_ports[0]);
  // the ro port is configured for PRIMARY_AND_SECONDARY so the first connection
  // will be directed to the PRIMARY
  auto client_ro =
      make_new_connection_ok(router_port_ro, cluster_nodes_ports[0]);

  SCOPED_TRACE(
      "// Now let's change the primary from node[0] to node[1] and let "
      "announce it via the new PRIMARY, also the old PRIMARY is gone now");
  set_mock_metadata(cluster_http_ports[1], cluster_id, {cluster_nodes_ports[1]},
                    /*primary_id=*/0, view_id + 1);

  SCOPED_TRACE("// Wait untill the router sees this change");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[1], 2));

  SCOPED_TRACE("// Check that the state file is as expected");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   {cluster_nodes_ports[1]}, view_id + 1);

  SCOPED_TRACE("// Check that both RW and RO connections are down");
  EXPECT_TRUE(wait_connection_dropped(*client_rw.get()));
  EXPECT_TRUE(wait_connection_dropped(*client_ro.get()));

  SCOPED_TRACE(
      "// Check that new RO connection is now made to the new PRIMARY");
  /*auto client_ro2 =*/make_new_connection_ok(router_port_ro,
                                              cluster_nodes_ports[1]);
}

/**
 * @test TS_R_EX_1
 */
TEST_F(AsyncReplicasetTest, OnlyPrimaryLeftAcceptsRW) {
  const unsigned CLUSTER_NODES = 2;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// Launch 2 server mocks that will act as our cluster members");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE("// Let us start with 2 members (PRIMARY and SECONDARY)");
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports,
                      /*primary_id=*/0, view_id);
  }

  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE("// Create a configuration file.");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available");
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", "round-robin");

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should contain the initial members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);

  SCOPED_TRACE("// Make one RO connection");
  auto client_ro =
      make_new_connection_ok(router_port_ro, cluster_nodes_ports[1]);

  SCOPED_TRACE("// Now let's bring the only SECONDARY down");
  set_mock_metadata(cluster_http_ports[0], cluster_id, {cluster_nodes_ports[0]},
                    /*primary_id=*/0, view_id + 1);

  SCOPED_TRACE("// Wait untill the router sees this change");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE("// Check that the state file is as expected");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   {cluster_nodes_ports[0]}, view_id + 1);

  SCOPED_TRACE("// Check that RO connection is down and no new is accepted");
  EXPECT_TRUE(wait_connection_dropped(*client_ro.get()));
  verify_new_connection_fails(router_port_ro);
}

class NodeUnavailableTest : public AsyncReplicasetTest,
                            public ::testing::WithParamInterface<std::string> {
};

/**
 * @test Veriify that when one of the nodes is not available despite being
 * present in the metadata, the Router redirects the connection to another
 * node(s), when they are available for the given routing.
 */
TEST_P(NodeUnavailableTest, NodeUnavailable) {
  const std::string routing_strategy = GetParam();

  const unsigned CLUSTER_NODES = 4;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// The cluster has 4 nodes but the first SECONDARY is down");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    if (i == 1) continue;
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE("// All 4 nodes are in the metadata");
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports,
                      /*primary_id=*/0, view_id);
  }

  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE("// Create a configuration file.");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available");
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", routing_strategy);

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router =*/launch_router(temp_test_dir.name(), metadata_cache_section,
                                  routing_section, state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should contain the initial members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);

  SCOPED_TRACE(
      "// Make 3 RO connections, even though one of the secondaries is down "
      "each of them should be successful");

  std::vector<std::string> connected_ports;

  // make 3 connections.
  for (int ndx = 0; ndx < 4; ++ndx) {
    MySQLSession session;

    ASSERT_NO_THROW(session.connect("127.0.0.1", router_port_ro, "username",
                                    "password", "", ""));

    auto result = session.query_one("select @@port");
    ASSERT_TRUE(result);
    ASSERT_NE((*result)[0], nullptr);

    connected_ports.push_back((*result)[0]);
  }

  if (routing_strategy == "first-available") {
    auto node_2 = std::to_string(cluster_nodes_ports[2]);

    EXPECT_THAT(connected_ports,
                ElementsAre(node_2,  //
                            node_2,  //
                            node_2,  //
                            node_2)  //
    );
  } else if (routing_strategy == "round-robin" ||
             routing_strategy == "round-robin-with-fallback") {
    auto node_2 = std::to_string(cluster_nodes_ports[2]);
    auto node_3 = std::to_string(cluster_nodes_ports[3]);

    EXPECT_THAT(connected_ports,
                ElementsAre(node_2,  // try [1], fallover to [2]
                            node_2,  // use [2]
                            node_3,  // use [3]
                            node_2   // use [2]
                            ));
  }
}

INSTANTIATE_TEST_SUITE_P(NodeUnavailable, NodeUnavailableTest,
                         ::testing::Values("first-available", "round-robin",
                                           "round-robin-with-fallback"));

class NodeUnavailableAllNodesDownTest
    : public AsyncReplicasetTest,
      public ::testing::WithParamInterface<std::string> {};

/**
 * @test Veriify that when all of the nodes for given routing are not available
 * the client connection fails or in case of round-robin-with-fallback we
 * fallback to the primary node.
 */
TEST_P(NodeUnavailableAllNodesDownTest, NodeUnavailableAllNodesDown) {
  const std::string routing_strategy = GetParam();

  const unsigned CLUSTER_NODES = 3;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// The cluster has 3 nodes all SECONDARIES are down");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    if (i > 0) continue;
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE("// All 3 nodes are in the metadata");
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports,
                      /*primary_id=*/0, view_id);
  }

  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE("// Create a configuration file.");
  const std::string metadata_cache_section = get_metadata_cache_section(kTTL);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available");
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", routing_strategy);

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  /*auto &router = */ launch_router(temp_test_dir.name(),
                                    metadata_cache_section, routing_section,
                                    state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE(
      "// Check our state file content, it should contain the initial members");
  check_state_file(state_file, ClusterType::RS_V2, cluster_id,
                   cluster_nodes_ports, view_id);

  SCOPED_TRACE(
      "// Attempt 2 RO connections, each should fail unless we fallback to the "
      "PRIMARY");
  for (size_t i = 0; i < 2; ++i) {
    if (routing_strategy != "round-robin-with-fallback") {
      verify_new_connection_fails(router_port_ro);
    } else {
      /*auto client_ro =*/make_new_connection_ok(router_port_ro,
                                                 cluster_nodes_ports[0]);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(NodeUnavailableAllNodesDown,
                         NodeUnavailableAllNodesDownTest,
                         ::testing::Values("first-available", "round-robin",
                                           "round-robin-with-fallback"));

struct ClusterTypeMismatchTestParams {
  std::string cluster_type_str;
  std::string tracefile;
  std::string expected_error;
};

class ClusterTypeMismatchTest
    : public AsyncReplicasetTest,
      public ::testing::WithParamInterface<ClusterTypeMismatchTestParams> {};

/**
 * @test TS_R_EX_1
 */
TEST_P(ClusterTypeMismatchTest, ClusterTypeMismatch) {
  const unsigned CLUSTER_NODES = 2;
  const bool is_gr_cluster =
      GetParam().tracefile == "metadata_dynamic_nodes_v2_gr.js";
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE(
      "// Launch 2 server mocks that will act as our cluster members.");
  const auto trace_file = get_data_dir().join(GetParam().tracefile).str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE("// Let us start with 2 members (PRIMARY and SECONDARY)");
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports,
                      /*primary_id=*/0, view_id, false, false, is_gr_cluster);
  }

  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE("// Create a configuration file.");
  const std::string metadata_cache_section =
      get_metadata_cache_section(kTTL, GetParam().cluster_type_str);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available");
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", "round-robin");

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  auto &router = launch_router(temp_test_dir.name(), metadata_cache_section,
                               routing_section, state_file, EXIT_SUCCESS,
                               /*wait_for_notify_ready=*/-1s);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE("// No connection should be possible");
  MySQLSession client_rw;
  ASSERT_ANY_THROW(client_rw.connect("127.0.0.1", router_port_ro, "username",
                                     "password", "", ""));

  SCOPED_TRACE("// Logfile should contain proper message");
  const std::string log_content = router.get_logfile_content();
  ASSERT_TRUE(pattern_found(log_content, GetParam().expected_error));
}

INSTANTIATE_TEST_SUITE_P(
    ClusterTypeMismatch, ClusterTypeMismatchTest,
    ::testing::Values(
        ClusterTypeMismatchTestParams{
            "rs", "metadata_dynamic_nodes_v2_gr.js",
            "Invalid cluster type 'gr'. Configured 'rs'"},
        ClusterTypeMismatchTestParams{
            "gr", "metadata_dynamic_nodes_v2_ar.js",
            "Invalid cluster type 'rs'. Configured 'gr'"}));

class UnexpectedResultFromMDRefreshTest
    : public AsyncReplicasetTest,
      public ::testing::WithParamInterface<ClusterTypeMismatchTestParams> {};

/**
 * @test Check that unexpected result returned from the metadata query does not
 * cause a router crash (BUG#30407266)
 */
TEST_P(UnexpectedResultFromMDRefreshTest, UnexpectedResultFromMDRefreshQuery) {
  const unsigned CLUSTER_NODES = 2;
  const bool is_gr_cluster = GetParam().cluster_type_str == "gr";
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE("// Launch 2 server mocks that will act as our cluster members");
  const auto trace_file = get_data_dir().join(GetParam().tracefile).str();
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));

    SCOPED_TRACE(
        "// Make our metadata server to return both nodes as a cluster "
        "members");
    set_mock_metadata(cluster_http_ports[i], cluster_id, cluster_nodes_ports, 0,
                      view_id, false, false, is_gr_cluster, i);
  }

  SCOPED_TRACE("// Create a router state file containing both members");
  const std::string state_file = create_state_file(
      temp_test_dir.name(),
      create_state_file_content(cluster_id, "", cluster_nodes_ports, view_id));

  SCOPED_TRACE(
      "// Create a configuration file. disconnect_on_metadata_unavailable for "
      "R/W  and R/O routing is true");
  const std::string metadata_cache_section =
      get_metadata_cache_section(kTTL, GetParam().cluster_type_str);
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const std::string routing_section_rw = get_metadata_cache_routing_section(
      router_port_rw, "PRIMARY", "first-available",
      /*disconnect_on_metadata_unavailable=*/true);
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const std::string routing_section_ro = get_metadata_cache_routing_section(
      router_port_ro, "SECONDARY", "round-robin",
      /*disconnect_on_metadata_unavailable=*/true);

  const std::string routing_section =
      routing_section_rw + "\n" + routing_section_ro;

  SCOPED_TRACE("// Launch the router with the initial state file");
  launch_router(temp_test_dir.name(), metadata_cache_section, routing_section,
                state_file);

  SCOPED_TRACE("// Wait until the router at least once queried the metadata");
  ASSERT_TRUE(wait_for_transaction_count_increase(cluster_http_ports[0], 2));

  SCOPED_TRACE("// Let's make a connection to the both servers RW and RO");
  auto client1 = make_new_connection_ok(router_port_rw, cluster_nodes_ports[0]);
  auto client2 = make_new_connection_ok(router_port_ro, cluster_nodes_ports[1]);

  SCOPED_TRACE(
      "// Make all members to start returning invalid data when queried for "
      "cluster type (empty resultset)");

  for (auto [i, http_port] : stdx::views::enumerate(cluster_http_ports)) {
    set_mock_metadata(http_port, cluster_id, cluster_nodes_ports, 0, view_id,
                      /*error_on_md_query=*/false,
                      /*empty_result_from_cluster_type_query=*/true,
                      is_gr_cluster, i);
  }

  SCOPED_TRACE("// Both connections should get dropped");
  EXPECT_TRUE(wait_connection_dropped(*client1.get()));
  EXPECT_TRUE(wait_connection_dropped(*client2.get()));
  // check that the router did not crash (happens automatically)
}

INSTANTIATE_TEST_SUITE_P(UnexpectedResultFromMDRefreshQuery,
                         UnexpectedResultFromMDRefreshTest,
                         ::testing::Values(
                             ClusterTypeMismatchTestParams{
                                 "gr", "metadata_dynamic_nodes_v2_gr.js", ""},
                             ClusterTypeMismatchTestParams{
                                 "rs", "metadata_dynamic_nodes_v2_ar.js", ""}));

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
