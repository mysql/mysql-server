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

#include <gtest/gtest.h>
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

#include "config_builder.h"
#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql/harness/stdx/ranges.h"     // enumerate
#include "mysql/harness/utility/string.h"  // join
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "mysqlrouter/utils.h"
#include "rest_api_testutils.h"
#include "router_component_clusterset.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_test_helpers.h"
#include "stdx_expected_no_error.h"
#include "tcp_port_pool.h"

using mysqlrouter::ClusterType;
using mysqlrouter::MySQLSession;
using ::testing::PrintToString;
using namespace std::chrono_literals;
using namespace std::string_literals;

Path g_origin_path;

namespace mysqlrouter {
std::ostream &operator<<(std::ostream &os, const MysqlError &e) {
  return os << e.sql_state() << " code: " << e.value() << ": " << e.message();
}
}  // namespace mysqlrouter

class ClusterSetTest : public RouterComponentClusterSetTest {
 protected:
  std::string metadata_cache_section(const std::chrono::milliseconds ttl = kTTL,
                                     bool use_gr_notifications = false) {
    auto ttl_str = std::to_string(std::chrono::duration<double>(ttl).count());

    return mysql_harness::ConfigBuilder::build_section(
        "metadata_cache:test",
        {{"cluster_type", "gr"},
         {"router_id", "1"},
         {"user", "mysql_router1_user"},
         {"metadata_cluster", "test"},
         {"connect_timeout", "1"},
         {"ttl", ttl_str},
         {"use_gr_notifications", use_gr_notifications ? "1" : "0"}});
  }

  std::string routing_section(uint16_t router_port, const std::string &role,
                              const std::string &strategy) {
    std::map<std::string, std::string> options{
        {"bind_address", "127.0.0.1"},
        {"bind_port", std::to_string(router_port)},
        {"destinations", "metadata-cache://test/default?role=" + role},
        {"protocol", "classic"}};

    if (!strategy.empty()) {
      options["routing_strategy"] = strategy;
    }

    return mysql_harness::ConfigBuilder::build_section(
        "routing:test_default" + std::to_string(router_port), options);
  }

  std::string create_config_and_keyring(
      const ClusterSetTopology &cs_topology,
      const std::chrono::milliseconds metadata_ttl, bool use_gr_notifications) {
    SCOPED_TRACE("// Prepare the dynamic state file for the Router");
    const auto clusterset_all_nodes_ports =
        cs_topology.get_md_servers_classic_ports();
    router_state_file =
        create_state_file(temp_test_dir.name(),
                          create_state_file_content("", cs_topology.uuid,
                                                    clusterset_all_nodes_ports,
                                                    /*view_id*/ 1));

    SCOPED_TRACE("// Prepare the config file for the Router");
    router_port_rw = port_pool_.get_next_available();
    router_port_ro = port_pool_.get_next_available();

    auto default_section = get_DEFAULT_defaults();

    init_keyring(default_section, temp_test_dir.name());

    default_section["dynamic_state"] = router_state_file;

    const std::string userfile = create_password_file();
    const std::string rest_sections = mysql_harness::join(
        get_restapi_config("rest_metadata_cache", userfile, true), "\n");

    return create_config_file(
        temp_test_dir.name(),
        metadata_cache_section(metadata_ttl, use_gr_notifications) +
            routing_section(router_port_rw, "PRIMARY", "first-available") +
            routing_section(router_port_ro, "SECONDARY", "round-robin") +
            rest_sections,
        &default_section);
  }

  auto &launch_router(const ClusterSetTopology &cs_topology,
                      const int expected_errorcode = EXIT_SUCCESS,
                      const std::chrono::milliseconds wait_for_notify_ready =
                          kReadyNotifyTimeout,
                      const std::chrono::milliseconds metadata_ttl = kTTL,
                      bool use_gr_notifications = false) {
    router_conf_file = create_config_and_keyring(cs_topology, metadata_ttl,
                                                 use_gr_notifications);

    auto &router = ProcessManager::launch_router(
        {"-c", router_conf_file}, expected_errorcode, /*catch_stderr=*/true,
        /*with_sudo=*/false, wait_for_notify_ready);

    return router;
  }

  auto &relaunch_router(const std::string &conf_file,
                        int expected_errorcode = EXIT_SUCCESS,
                        std::chrono::milliseconds wait_for_notify_ready = 30s) {
    auto &router = ProcessManager::launch_router(
        {"-c", conf_file}, expected_errorcode, /*catch_stderr=*/true,
        /*with_sudo=*/false, wait_for_notify_ready);
    return router;
  }

  int get_int_global_value(const uint16_t http_port, const std::string &name) {
    const auto server_globals =
        MockServerRestClient(http_port).get_globals_as_json_string();

    return get_int_field_value(server_globals, name);
  }

  void set_fetch_whole_topology(bool value) {
    const std::string metadata_cache_section_name = "test";
    const std::string path = std::string(rest_api_basepath) + "/metadata/" +
                             metadata_cache_section_name + "/config";
    ASSERT_TRUE(wait_for_rest_endpoint_ready(path, http_port_, kRestApiUsername,
                                             kRestApiPassword));

    const std::string parameter = "fetchWholeTopology="s + (value ? "1" : "0");

    IOContext io_ctx;
    RestClient rest_client(io_ctx, "127.0.0.1", http_port_, kRestApiUsername,
                           kRestApiPassword);

    auto req =
        rest_client.request_sync(HttpMethod::Get, path + "?" + parameter);

    ASSERT_TRUE(req) << "HTTP Request failed (early): " << req.error_msg()
                     << std::endl;
    ASSERT_GT(req.get_response_code(), 0)
        << "HTTP Request failed: " << req.error_msg() << std::endl;
  }

  // @brief wait until global read from the mock server is greater or equal
  // expected threashold
  // @retval true selected global is greater or equal to expected threshold
  // @retval false timed out waiting for selected global to become greater or
  // equal to expected threshold
  bool wait_global_ge(const uint16_t http_port, const std::string &name,
                      int threashold, std::chrono::milliseconds timeout = 15s) {
    const auto kStep = 100ms;
    do {
      const auto value = get_int_global_value(http_port, name);
      if (value >= threashold) return true;
      std::this_thread::sleep_for(kStep);
      timeout -= kStep;
    } while (timeout >= 0ms);

    return false;
  }

  void verify_only_primary_gets_updates(const ClusterSetTopology &cs_topology,
                                        const unsigned primary_cluster_id,
                                        const unsigned primary_node_id = 0) {
    // <cluster_id, node_id>
    using NodeId = std::pair<unsigned, unsigned>;
    std::map<NodeId, size_t> count;

    // in the first run pick up how many times the last_check_in update was
    // performed on each node so far
    for (const auto &cluster : cs_topology.clusters) {
      unsigned node_id = 0;
      for (const auto &node : cluster.nodes) {
        count[NodeId(cluster.id, node_id)] =
            get_int_global_value(node.http_port, "update_last_check_in_count");
        ++node_id;
      }
    }

    // in the next step wait for the counter to be incremented on the primary
    // node
    const auto http_port = cs_topology.clusters[primary_cluster_id]
                               .nodes[primary_node_id]
                               .http_port;
    EXPECT_TRUE(
        wait_global_ge(http_port, "update_last_check_in_count",
                       count[NodeId(primary_cluster_id, primary_node_id)] + 1));

    // the counter for all other nodes should not change
    for (const auto &cluster : cs_topology.clusters) {
      unsigned node_id = 0;
      for (const auto &node : cluster.nodes) {
        // only primary node of the primary cluster is expected do the
        // metadata version update and last_check_in updates
        if (cluster.id != primary_cluster_id || node_id != primary_node_id) {
          EXPECT_EQ(get_int_global_value(node.http_port,
                                         "update_last_check_in_count"),
                    count[NodeId(cluster.id, node_id)]);
        }
        ++node_id;
      }
    }
  }

  void verify_no_last_check_in_updates(const ClusterSetTopology &cs_topology,
                                       const std::chrono::milliseconds period) {
    // <cluster_id, node_id>
    using NodeId = std::pair<unsigned, unsigned>;
    std::map<NodeId, size_t> count;

    // in the first run pick up how many times the last_check_in update was
    // performed on each node so far
    for (const auto &cluster : cs_topology.clusters) {
      for (const auto [node_id, node] : stdx::views::enumerate(cluster.nodes)) {
        count[NodeId(cluster.id, node_id)] =
            get_int_global_value(node.http_port, "update_last_check_in_count");
      }
    }

    std::this_thread::sleep_for(period);

    // make sure the last_check_in update counter was not incremented on any of
    // the nodes
    for (const auto &cluster : cs_topology.clusters) {
      for (const auto [node_id, node] : stdx::views::enumerate(cluster.nodes)) {
        EXPECT_EQ(
            get_int_global_value(node.http_port, "update_last_check_in_count"),
            count[NodeId(cluster.id, node_id)]);
      }
    }
  }

  std::string router_conf_file;

  TempDirectory temp_test_dir;
  uint64_t view_id = 1;

  std::string router_state_file;
  uint16_t router_port_rw;
  uint16_t router_port_ro;

  static const std::chrono::milliseconds kTTL;
  static const std::chrono::seconds kReadyNotifyTimeout;
  static const unsigned kRWNodeId = 0;
  static const unsigned kRONodeId = 1;

  const unsigned kPrimaryClusterId{0};
  const unsigned kFirstReplicaClusterId{1};
  const unsigned kSecondReplicaClusterId{2};
};

const std::chrono::milliseconds ClusterSetTest::kTTL = 50ms;
const std::chrono::seconds ClusterSetTest::kReadyNotifyTimeout = 30s;

//////////////////////////////////////////////////////////////////////////

struct TargetClusterTestParams {
  // target_cluster= for the config file
  std::string target_cluster;
  // id of the target Cluster within ClusterSet
  unsigned target_cluster_id;

  // which cluster we expect to handle the connections (same for RW and RO)
  unsigned expected_connection_cluster_id{99};

  std::string expected_error{""};
};

class ClusterSetTargetClusterTest
    : public ClusterSetTest,
      public ::testing::WithParamInterface<TargetClusterTestParams> {};

/**
 * @test Checks that the target cluster from the metadata is respected
 * and the Router is using expected cluster for client RW and RO connections.
 * [@FR3.6]
 */
TEST_P(ClusterSetTargetClusterTest, ClusterSetTargetCluster) {
  const auto target_cluster = GetParam().target_cluster;
  const auto target_cluster_id = GetParam().target_cluster_id;
  const auto expected_connection_cluster_id =
      GetParam().expected_connection_cluster_id;

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options =
      R"({"target_cluster" : ")" + target_cluster + "\" }";
  create_clusterset(cs_options);

  SCOPED_TRACE("// Launch the Router");
  /*auto &router =*/launch_router(cs_options.topology);

  SCOPED_TRACE(
      "// Make the connections to both RW and RO ports and check if they are "
      "directed to expected Cluster from the ClusterSet");

  if (target_cluster_id == 0 /*primary_cluster_id*/) {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(
        verify_port(conn_res->get(),
                    cs_options.topology.clusters[expected_connection_cluster_id]
                        .nodes[kRWNodeId]
                        .classic_port));

  } else {
    /* replica cluster*/
    verify_new_connection_fails(router_port_rw);
  }

  // in case of replica cluster first RO node is primary node of the Cluster
  const auto first_ro_node = (target_cluster_id == 0) ? kRONodeId : kRWNodeId;

  {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(
        verify_port(conn_res->get(),
                    cs_options.topology.clusters[expected_connection_cluster_id]
                        .nodes[first_ro_node]
                        .classic_port));
  }
}

INSTANTIATE_TEST_SUITE_P(
    ClusterSetTargetCluster, ClusterSetTargetClusterTest,
    ::testing::Values(
        // 0) we use "primary" as a target_cluster so the connections should go
        // the the first Cluster as it's the Primary Cluster
        TargetClusterTestParams{/*target_cluster*/ "primary",
                                /*target_cluster_id*/ 0,
                                /*expected_connection_cluster_id*/ 0},
        // 1) we use first Cluster's GR UUID as a target_cluster so the
        // connections should go the the first Cluster
        TargetClusterTestParams{
            /*target_cluster*/ "00000000-0000-0000-0000-0000000000g1",
            /*target_cluster_id*/ 0,
            /*expected_connection_cluster_id*/ 0},
        // 2) we use second Cluster's GR UUID as a target_cluster so the
        // connections should go the the second Cluster
        TargetClusterTestParams{
            /*target_cluster*/ "00000000-0000-0000-0000-0000000000g2",
            /*target_cluster_id*/ 1,
            /*expected_connection_cluster_id*/ 1}));

struct TargetClusterChangeInMetataTestParams {
  // info about the target_cluster we start with (in config file)
  // and the expected connections destinations for that cluster
  TargetClusterTestParams initial_target_cluster;

  // info about the target_cluster we change to (in the metadata)
  // and the expected connections destinations for that cluster
  TargetClusterTestParams changed_target_cluster;

  // whether the initial connections (the ones for first target_cluster before
  // the change) are expected to be dropped or expected to stay
  bool initial_connections_should_drop;
};

/**
 * @test Checks that if the target cluster does not change in the metadata,
 * Router does not keep reporting it has changed (bug#33261274)
 */
TEST_F(ClusterSetTest, TargetClusterNoChange) {
  const std::string target_cluster = "primary";
  const auto target_cluster_id = 0;

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options =
      R"({"target_cluster" : ")" + target_cluster + "\" }";
  create_clusterset(cs_options);

  SCOPED_TRACE("// Launch the Router");
  auto &router = launch_router(cs_options.topology);

  // keep the Router running for several md refresh rounds
  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 3));

  // check the new target_cluster was repoted only once
  const std::string needle = "New target cluster assigned in the metadata";
  const std::string log_content = router.get_logfile_content();

  // 1 is expected, that comes from the initial reading of the metadata
  EXPECT_EQ(1, count_str_occurences(log_content, needle));
}

class ClusterChangeTargetClusterInTheMetadataTest
    : public ClusterSetTest,
      public ::testing::WithParamInterface<
          TargetClusterChangeInMetataTestParams> {};

/**
 * @test Checks that the target cluster changes in the metadata are correctly
 * followed by the Router.
 * [@FR3.7]
 * [@FR3.7.1]
 */
TEST_P(ClusterChangeTargetClusterInTheMetadataTest,
       ClusterChangeTargetClusterInTheMetadata) {
  const auto initial_target_cluster =
      GetParam().initial_target_cluster.target_cluster;
  const auto initial_target_cluster_id =
      GetParam().initial_target_cluster.target_cluster_id;
  const auto expected_initial_connection_cluster_id =
      GetParam().initial_target_cluster.expected_connection_cluster_id;

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = initial_target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options =
      R"({"target_cluster" : ")" + initial_target_cluster + "\" }";
  create_clusterset(cs_options);

  auto &router = launch_router(cs_options.topology);

  {
    const auto target_cluster_name =
        cs_options.topology.clusters[initial_target_cluster_id].name;
    const std::string cluster_role =
        initial_target_cluster_id == 0 ? "primary" : "replica";
    const std::string accepting_rw = initial_target_cluster_id == 0
                                         ? "accepting RW connections"
                                         : "not accepting RW connections";

    const std::string pattern1 =
        "INFO .* Target cluster\\(s\\) are part of a ClusterSet: " +
        accepting_rw;
    const std::string pattern2 =
        "INFO .* Cluster '" + target_cluster_name +
        "': role of a cluster within a ClusterSet is '" + cluster_role + "';";

    EXPECT_TRUE(wait_log_contains(router, pattern1, 5s)) << pattern1;
    EXPECT_TRUE(wait_log_contains(router, pattern2, 5s)) << pattern2;
  }

  SCOPED_TRACE(
      "// Make the connections to both RW and RO ports and check if they are "
      "directed to expected Cluster from the ClusterSet");
  std::unique_ptr<MySQLSession> rw_con1;
  if (expected_initial_connection_cluster_id == 0 /*primary_cluster_id*/) {
    auto rw_con1_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(rw_con1_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        rw_con1_res->get(),
        cs_options.topology.clusters[expected_initial_connection_cluster_id]
            .nodes[kRWNodeId]
            .classic_port));

    rw_con1 = std::move(*rw_con1_res);
  } else {
    /* replica cluster, the RW connection should fail */
    verify_new_connection_fails(router_port_rw);
  }

  const auto first_ro_node1 =
      (expected_initial_connection_cluster_id == 0 /*Primary*/) ? kRONodeId
                                                                : kRWNodeId;
  auto ro_con1_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con1_res->get(),
      cs_options.topology.clusters[expected_initial_connection_cluster_id]
          .nodes[first_ro_node1]
          .classic_port));

  auto ro_con1 = std::move(*ro_con1_res);

  SCOPED_TRACE(
      "// Change the target_cluster in the metadata of the first Cluster and "
      "bump its view id");

  const auto changed_target_cluster =
      GetParam().changed_target_cluster.target_cluster;
  const auto changed_target_cluster_id =
      GetParam().changed_target_cluster.target_cluster_id;

  cs_options.target_cluster_id = changed_target_cluster_id;
  cs_options.router_options =
      R"({"target_cluster" : ")" + changed_target_cluster + "\" }";
  set_mock_clusterset_metadata(
      cs_options.topology.clusters[0].nodes[0].http_port,
      /*this_cluster_id*/ 0,
      /*this_node_id*/ 0, cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 3));

  SCOPED_TRACE("// Check if the change of a target cluster has been logged");
  {
    const auto changed_target_cluster_name =
        cs_options.topology.clusters[changed_target_cluster_id].name;
    const std::string cluster_role =
        changed_target_cluster_id == 0 ? "primary" : "replica";
    const std::string accepting_rw = changed_target_cluster_id == 0
                                         ? "accepting RW connections"
                                         : "not accepting RW connections";
    const std::string pattern1 =
        "INFO .* New target cluster assigned in the metadata: '" +
        changed_target_cluster_name + "'";

    const std::string pattern2 =
        "INFO .* Target cluster\\(s\\) are part of a ClusterSet: " +
        accepting_rw;
    const std::string pattern3 =
        "INFO .* Cluster '" + changed_target_cluster_name +
        "': role of a cluster within a ClusterSet is '" + cluster_role + "';";

    EXPECT_TRUE(wait_log_contains(router, pattern1, 5s)) << pattern1;
    EXPECT_TRUE(wait_log_contains(router, pattern2, 5s)) << pattern2;

    const std::string pattern4 =
        "INFO .* New router options read from the metadata "
        "'\\{\"target_cluster\" : \"" +
        changed_target_cluster + "\" \\}', was '\\{\"target_cluster\" : \"" +
        initial_target_cluster + "\" \\}'";

    EXPECT_TRUE(wait_log_contains(router, pattern1, 5s)) << pattern1;
    EXPECT_TRUE(wait_log_contains(router, pattern2, 100ms)) << pattern2;
    EXPECT_TRUE(wait_log_contains(router, pattern3, 100ms)) << pattern3;
    EXPECT_TRUE(wait_log_contains(router, pattern4, 100ms)) << pattern4;
  }

  if (GetParam().initial_connections_should_drop) {
    SCOPED_TRACE(
        "// Since the target_cluster has changed the existing connection "
        "should get dropped");
    if (rw_con1) {
      ASSERT_NO_FATAL_FAILURE(
          verify_existing_connection_dropped(rw_con1.get()));
    }
    ASSERT_NO_FATAL_FAILURE(verify_existing_connection_dropped(ro_con1.get()));
  } else {
    if (rw_con1) {
      ASSERT_NO_FATAL_FAILURE(verify_existing_connection_ok(rw_con1.get()));
    }
    ASSERT_NO_FATAL_FAILURE(verify_existing_connection_ok(ro_con1.get()));
  }

  const auto expected_new_connection_cluster_id =
      GetParam().changed_target_cluster.expected_connection_cluster_id;

  SCOPED_TRACE(
      "// The new connections should get routed to the new target Cluster");

  if (expected_new_connection_cluster_id == 0 /*primary_cluster_id*/) {
    auto rw_con2_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(rw_con2_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        rw_con2_res->get(),
        cs_options.topology.clusters[expected_new_connection_cluster_id]
            .nodes[kRWNodeId]
            .classic_port));
  } else {
    /* replica cluster, the RW connection should fail */
    ASSERT_NO_FATAL_FAILURE(verify_new_connection_fails(router_port_rw));
  }

  const auto first_ro_node =
      (expected_new_connection_cluster_id == 0 /*Primary*/) ? kRONodeId
                                                            : kRWNodeId;
  // +1 because it's round-robin and this is the second RO connection
  {
    auto ro_con2_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(ro_con2_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        ro_con2_res->get(),
        cs_options.topology.clusters[expected_new_connection_cluster_id]
            .nodes[first_ro_node + 1]
            .classic_port));
  }

  SCOPED_TRACE(
      "// Check that only primary nodes from each Cluster were checked for the "
      "metadata");
  for (const auto &cluster : cs_options.topology.clusters) {
    unsigned node_id = 0;
    for (const auto &node : cluster.nodes) {
      const auto transactions_count = get_transaction_count(node.http_port);
      if (node_id == 0) {
        wait_for_transaction_count(node.http_port, 2);
      } else {
        // we expect the secondary node of each Cluster being queried only once,
        // when the first metadata refresh is run, as at that point we only have
        // a set of the metadata servers (all cluster nodes) from the state file
        // and we do not know which of then belongs to which of the Clusters
        // (we do not know the topology)
        EXPECT_EQ(transactions_count, 1);
      }
      ++node_id;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    ClusterChangeTargetClusterInTheMetadata,
    ClusterChangeTargetClusterInTheMetadataTest,
    ::testing::Values(
        // 0) "primary" (which is "gr-id-1") overwritten in metadata with
        // "gr-id-2" - existing connections are expected to drop
        TargetClusterChangeInMetataTestParams{
            TargetClusterTestParams{/*target_cluster*/ "primary",
                                    /*target_cluster_id*/ 0,
                                    /*expected_connection_cluster_id*/ 0},
            TargetClusterTestParams{
                /*target_cluster*/ "00000000-0000-0000-0000-0000000000g2",
                /*target_cluster_id*/ 1,
                /*expected_connection_cluster_id*/ 1},
            true},
        // 1) "gr-id-2" overwritten in metadata with "primary" - existing
        // connections are expected to drop
        TargetClusterChangeInMetataTestParams{
            TargetClusterTestParams{
                /*target_cluster*/ "00000000-0000-0000-0000-0000000000g2",
                /*target_cluster_id*/ 1,
                /*expected_connection_cluster_id*/ 1},
            TargetClusterTestParams{/*target_cluster*/ "primary",
                                    /*target_cluster_id*/ 0,
                                    /*expected_connection_cluster_id*/ 0},
            true},
        // 2) "gr-id-1" overwritten in metadata with "primary" - existing
        // connections are NOT expected to drop as this is the same Cluster
        TargetClusterChangeInMetataTestParams{
            TargetClusterTestParams{
                /*target_cluster*/ "00000000-0000-0000-0000-0000000000g1",
                /*target_cluster_id*/ 0,
                /*expected_connection_cluster_id*/ 0},
            TargetClusterTestParams{/*target_cluster*/ "primary",
                                    /*target_cluster_id*/ 0,
                                    /*expected_connection_cluster_id*/ 0},
            false}));

/**
 * @test Check that the Router correctly handles clustersetid not matching the
 * one in the state file.
 * [@FR13]
 * [@FR13.1]
 * [@TS_R14_1]
 */
TEST_F(ClusterSetTest, ClusterChangeClustersetIDInTheMetadata) {
  const int kTargetClusterId = 0;
  const std::string router_options = R"({"target_cluster" : "primary"})";

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = kTargetClusterId;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = router_options;
  create_clusterset(cs_options);

  /*auto &router =*/launch_router(cs_options.topology);

  SCOPED_TRACE(
      "// Make the connections to both RW and RO ports and check if they are"
      " directed to expected Cluster from the ClusterSet");
  auto rw_con1_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con1_res->get(), cs_options.topology.clusters[kTargetClusterId]
                              .nodes[kRWNodeId]
                              .classic_port));
  auto rw_con1 = std::move(*rw_con1_res);

  auto ro_con1_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con1_res->get(), cs_options.topology.clusters[kTargetClusterId]
                              .nodes[kRONodeId]
                              .classic_port));
  auto ro_con1 = std::move(*ro_con1_res);

  SCOPED_TRACE("// Change the clusterset_id in the metadata");
  cs_options.topology.uuid = "changed-clusterset-uuid";

  cs_options.view_id = view_id + 1;
  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  SCOPED_TRACE(
      "// Check that the old connections got dropped and new are not "
      "possible");
  verify_existing_connection_dropped(rw_con1.get());
  verify_existing_connection_dropped(ro_con1.get());
  verify_new_connection_fails(router_port_rw);
  verify_new_connection_fails(router_port_ro);

  SCOPED_TRACE(
      "// Restore the original ClusterSet ID, matching the one stored in the "
      "state file");
  cs_options.view_id = view_id + 2;
  cs_options.topology.uuid = "clusterset-uuid";
  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  SCOPED_TRACE("// Check that the connections are possible again");
  auto rw_con2_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con2_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con2_res->get(), cs_options.topology.clusters[kTargetClusterId]
                              .nodes[kRWNodeId]
                              .classic_port));
  auto rw_con2 = std::move(*rw_con2_res);

  auto ro_con2_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con2_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con2_res->get(), cs_options.topology.clusters[kTargetClusterId]
                              .nodes[kRONodeId + 1]
                              .classic_port));
  auto ro_con2 = std::move(*ro_con2_res);

  SCOPED_TRACE(
      "// Simulate the primary cluster can't be found in the ClusterSet");
  cs_options.view_id = view_id + 3;
  cs_options.simulate_cluster_not_found = true;
  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[1].nodes[0].http_port, 2));

  SCOPED_TRACE(
      "// Check that the old connections got dropped and new are not "
      "possible");
  verify_existing_connection_dropped(rw_con2.get());
  verify_existing_connection_dropped(ro_con2.get());
  verify_new_connection_fails(router_port_rw);
  verify_new_connection_fails(router_port_ro);
}

class UnknownClusterSetTargetClusterTest
    : public ClusterSetTest,
      public ::testing::WithParamInterface<TargetClusterTestParams> {};

/**
 * @test Checks that if the `target_cluster` for the Router can't be find in the
 * metadata the error should be logged and the Router should not accept any
 * connections.
 */
TEST_P(UnknownClusterSetTargetClusterTest, UnknownClusterSetTargetCluster) {
  const auto target_cluster = GetParam().target_cluster;
  const auto target_cluster_id = GetParam().target_cluster_id;

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options =
      R"({"target_cluster" : ")" + target_cluster + "\" }";
  create_clusterset(cs_options);

  auto config_file =
      create_config_and_keyring(cs_options.topology, kTTL,
                                /* use_gr_notifications= */ false);

  auto &router = router_spawner()
                     .wait_for_sync_point(Spawner::SyncPoint::RUNNING)
                     .spawn({"-c", config_file});

  EXPECT_TRUE(wait_log_contains(router, GetParam().expected_error, 20s));

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[1].nodes[0].http_port, 2));

  SCOPED_TRACE(
      "// Make the connections to both RW and RO ports, both should fail");

  verify_new_connection_fails(router_port_rw);
  verify_new_connection_fails(router_port_ro);
}

INSTANTIATE_TEST_SUITE_P(
    UnknownClusterSetTargetCluster, UnknownClusterSetTargetClusterTest,
    ::testing::Values(
        // [@TS_R9_1/1]
        TargetClusterTestParams{
            "000000000000000000000000000000g1", 0, 0,
            "ERROR.* Could not find target_cluster "
            "'000000000000000000000000000000g1' in the metadata"},
        // [@TS_R9_1/2]
        TargetClusterTestParams{
            "00000000-0000-0000-0000-0000000000g11", 0, 0,
            "ERROR.* Could not find target_cluster "
            "'00000000-0000-0000-0000-0000000000g11' in the metadata"},
        // [@TS_R9_1/3]
        TargetClusterTestParams{
            "00000000-0000-0000-0000-0000000000g", 0, 0,
            "ERROR.* Could not find target_cluster "
            "'00000000-0000-0000-0000-0000000000g' in the metadata"},
        // [@TS_R9_1/4]
        TargetClusterTestParams{
            "00000000-0000-0000-Z000-0000000000g1", 0, 0,
            "ERROR.* Could not find target_cluster "
            "'00000000-0000-0000-Z000-0000000000g1' in the metadata"},
        // [@TS_R9_1/5]
        TargetClusterTestParams{
            "00000000-0000-0000-0000-0000000000G1", 0, 0,
            "ERROR.* Could not find target_cluster "
            "'00000000-0000-0000-0000-0000000000G1' in the metadata"},

        // [@TS_R9_1/8]
        TargetClusterTestParams{"0", 0, 0,
                                "ERROR.* Could not find target_cluster "
                                "'0' in the metadata"},
        // [@TS_R9_1/9]
        TargetClusterTestParams{
            "'00000000-0000-0000-0000-0000000000g1'", 0, 0,
            "ERROR.* Could not find target_cluster "
            "''00000000-0000-0000-0000-0000000000g1'' in the metadata"}));

/**
 * @test Checks that if the `target_cluster` for the Router is empty in the
 * metadata the warning is logged and the Router accepts the connections
 * using primary cluster as a default.
 * [@TS_R9_1/7]
 */
TEST_F(ClusterSetTest, TargetClusterEmptyInMetadata) {
  ClusterSetOptions cs_options;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = R"({"target_cluster" : "" })";
  create_clusterset(cs_options);

  auto config_file =
      create_config_and_keyring(cs_options.topology, kTTL,
                                /* use_gr_notifications= */ false);

  auto &router = router_spawner()
                     .wait_for_sync_point(Spawner::SyncPoint::READY)
                     .spawn({"-c", config_file});

  EXPECT_TRUE(wait_log_contains(router,
                                "Target cluster for router_id=1 not set, using "
                                "'primary' as a target cluster",
                                20s));

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[1].nodes[0].http_port, 2));

  SCOPED_TRACE(
      "// Make the connections to both RW and RO ports, both should be ok");

  {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(),
        cs_options.topology.clusters[0].nodes[kRWNodeId].classic_port));
  }

  {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(),
        cs_options.topology.clusters[0].nodes[kRONodeId].classic_port));
  }
}

/**
 * @test Check that the Router correctly follows primary Cluster when it is its
 * target_cluster.
 */
TEST_F(ClusterSetTest, ClusterRolesChangeInTheRuntime) {
  // first cluster is a primary on start
  unsigned primary_cluster_id = 0;
  const std::string router_options =
      R"({"target_cluster" : "primary", "stats_updates_frequency": 1})";

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = primary_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = router_options;
  create_clusterset(cs_options);

  /*auto &router =*/launch_router(cs_options.topology);

  SCOPED_TRACE(
      "// Make the connections to both RW and RO ports and check if they are"
      " directed to expected Cluster from the ClusterSet");
  auto rw_con1_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con1_res->get(), cs_options.topology.clusters[primary_cluster_id]
                              .nodes[kRWNodeId]
                              .classic_port));
  auto rw_con1 = std::move(*rw_con1_res);

  auto ro_con1_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con1_res->get(), cs_options.topology.clusters[primary_cluster_id]
                              .nodes[kRONodeId]
                              .classic_port));
  auto ro_con1 = std::move(*ro_con1_res);

  verify_only_primary_gets_updates(cs_options.topology, primary_cluster_id);

  ////////////////////////////////////
  SCOPED_TRACE(
      "// Change the primary cluster in the metadata, now the first Replica "
      "Cluster becomes the PRIMARY");
  ////////////////////////////////////

  primary_cluster_id = 1;
  change_clusterset_primary(cs_options.topology, primary_cluster_id);
  cs_options.view_id = ++view_id;
  cs_options.target_cluster_id = primary_cluster_id;
  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  SCOPED_TRACE("// Check that the existing connections got dropped");
  verify_existing_connection_dropped(rw_con1.get());
  verify_existing_connection_dropped(ro_con1.get());

  SCOPED_TRACE(
      "// Check that new connections are directed to the new PRIMARY cluster "
      "nodes");
  auto rw_con2_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con2_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con2_res->get(), cs_options.topology.clusters[primary_cluster_id]
                              .nodes[kRWNodeId]
                              .classic_port));
  auto rw_con2 = std::move(*rw_con2_res);

  // +1%2 is for round-robin
  auto ro_con2_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con2_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con2_res->get(), cs_options.topology.clusters[primary_cluster_id]
                              .nodes[kRONodeId + 1 % 2]
                              .classic_port));
  auto ro_con2 = std::move(*ro_con2_res);

  // check the new primary gets updates
  verify_only_primary_gets_updates(cs_options.topology, primary_cluster_id);

  ////////////////////////////////////
  SCOPED_TRACE(
      "// Change the primary cluster in the metadata, now the second Replica "
      "Cluster becomes the PRIMARY");
  ////////////////////////////////////
  primary_cluster_id = 2;
  change_clusterset_primary(cs_options.topology, primary_cluster_id);
  cs_options.view_id = ++view_id;
  cs_options.target_cluster_id = primary_cluster_id;
  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  SCOPED_TRACE("// Check that the existing connections got dropped");
  verify_existing_connection_dropped(rw_con2.get());
  verify_existing_connection_dropped(ro_con2.get());

  SCOPED_TRACE(
      "// Check that new connections are directed to the new PRIMARY cluster "
      "nodes");
  auto rw_con3_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con3_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con3_res->get(), cs_options.topology.clusters[primary_cluster_id]
                              .nodes[kRWNodeId]
                              .classic_port));
  auto rw_con3 = std::move(*rw_con3_res);

  // +2 %2 is for round-robin
  auto ro_con3_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con3_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con3_res->get(), cs_options.topology.clusters[primary_cluster_id]
                              .nodes[kRONodeId + 2 % 2]
                              .classic_port));
  auto ro_con3 = std::move(*ro_con3_res);

  ////////////////////////////////////
  SCOPED_TRACE(
      "// Change the primary cluster in the metadata, let the original PRIMARY "
      "be the primary again");
  ////////////////////////////////////
  primary_cluster_id = 0;
  change_clusterset_primary(cs_options.topology, primary_cluster_id);
  cs_options.view_id = ++view_id;
  cs_options.target_cluster_id = primary_cluster_id;
  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  SCOPED_TRACE("// Check that the existing connections got dropped");
  verify_existing_connection_dropped(rw_con3.get());
  verify_existing_connection_dropped(ro_con3.get());

  SCOPED_TRACE(
      "// Check that new connections are directed to the new PRIMARY cluster "
      "nodes");
  auto rw_con4_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con4_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con4_res->get(), cs_options.topology.clusters[primary_cluster_id]
                              .nodes[kRWNodeId]
                              .classic_port));
  // +3%2 is for round-robin
  auto ro_con4_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con4_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con4_res->get(), cs_options.topology.clusters[primary_cluster_id]
                              .nodes[kRONodeId + 3 % 2]
                              .classic_port));
}

/**
 * @test Check that the Router sticks to the target_cluster given by UUID when
 * its role changes starting from PRIMARY.
 * [@TS_R6_2]
 */
TEST_F(ClusterSetTest, TargetClusterStickToPrimaryUUID) {
  // first cluster is a primary on start
  unsigned primary_cluster_id = 0;
  const unsigned target_cluster_id = 0;
  const std::string router_options =
      R"({"target_cluster" : "00000000-0000-0000-0000-0000000000g1",
         "stats_updates_frequency": 1})";

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.primary_cluster_id = primary_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = router_options;
  create_clusterset(cs_options);

  /*auto &router =*/launch_router(cs_options.topology);

  SCOPED_TRACE(
      "// Make the connections to both RW and RO ports and check if they are"
      " directed to expected Cluster from the ClusterSet");
  auto rw_con1_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con1_res->get(), cs_options.topology.clusters[target_cluster_id]
                              .nodes[kRWNodeId]
                              .classic_port));
  auto rw_con1 = std::move(*rw_con1_res);

  auto ro_con1_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con1_res->get(), cs_options.topology.clusters[target_cluster_id]
                              .nodes[kRONodeId]
                              .classic_port));
  auto ro_con1 = std::move(*ro_con1_res);

  // check that the primary cluster is getting the periodic metadata updates
  verify_only_primary_gets_updates(cs_options.topology, primary_cluster_id);

  ////////////////////////////////////
  SCOPED_TRACE(
      "// Change the primary cluster in the metadata, now the first Replica "
      "Cluster becomes the PRIMARY");
  ////////////////////////////////////

  primary_cluster_id = 1;
  change_clusterset_primary(cs_options.topology, primary_cluster_id);
  cs_options.view_id = ++view_id;
  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  SCOPED_TRACE(
      "// RW connection should get dropped as our target_cluster is no longer "
      "PRIMARY");
  verify_existing_connection_dropped(rw_con1.get());
  SCOPED_TRACE("// RO connection should stay valid");
  verify_existing_connection_ok(ro_con1.get());

  SCOPED_TRACE(
      "// Check that new RO connection is directed to the same Cluster and no "
      "new RW connection is possible");
  // +1%3 because we round-robin and we now have 3 RO nodes
  auto ro_con2_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con2_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con2_res->get(), cs_options.topology.clusters[target_cluster_id]
                              .nodes[(kRWNodeId + 1) % 3]
                              .classic_port));
  auto ro_con2 = std::move(*ro_con2_res);

  verify_new_connection_fails(router_port_rw);

  // check that the primary cluster is getting the periodic metadata updates
  verify_only_primary_gets_updates(cs_options.topology, primary_cluster_id);

  ////////////////////////////////////
  SCOPED_TRACE(
      "// Change the primary cluster in the metadata, now the second Replica "
      "Cluster becomes the PRIMARY");
  ////////////////////////////////////
  primary_cluster_id = 2;
  change_clusterset_primary(cs_options.topology, primary_cluster_id);
  cs_options.view_id = ++view_id;
  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  SCOPED_TRACE("// Both existing RO connections should be fine");
  verify_existing_connection_ok(ro_con1.get());
  verify_existing_connection_ok(ro_con2.get());

  SCOPED_TRACE(
      "// Check that new RO connection is directed to the same Cluster and no "
      "new RW connection is possible");
  verify_new_connection_fails(router_port_rw);
  // +2%3 because we round-robin and we now have 3 RO nodes
  auto ro_con3_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con3_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con3_res->get(), cs_options.topology.clusters[target_cluster_id]
                              .nodes[(kRWNodeId + 2) % 3]
                              .classic_port));
  auto ro_con3 = std::move(*ro_con3_res);

  ////////////////////////////////////
  SCOPED_TRACE(
      "// Change the primary cluster in the metadata, let the original PRIMARY "
      "be the primary again");
  ////////////////////////////////////
  primary_cluster_id = 0;
  change_clusterset_primary(cs_options.topology, primary_cluster_id);
  cs_options.view_id = ++view_id;
  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  SCOPED_TRACE("// Check that all the existing RO connections are OK");
  verify_existing_connection_ok(ro_con1.get());
  verify_existing_connection_ok(ro_con2.get());
  verify_existing_connection_ok(ro_con3.get());

  SCOPED_TRACE("// Check that both RW and RO connections are possible again");
  auto rw_con4_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con4_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con4_res->get(), cs_options.topology.clusters[target_cluster_id]
                              .nodes[kRWNodeId]
                              .classic_port));

  auto ro_con4_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con4_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con4_res->get(), cs_options.topology.clusters[target_cluster_id]
                              .nodes[kRONodeId]
                              .classic_port));
}

/**
 * @test Check that the Router sticks to the target_cluster given by UUID when
 * its role changes starting from REPLICA.
 */
TEST_F(ClusterSetTest, TargetClusterStickToReaplicaUUID) {
  // first cluster is a primary on start
  unsigned primary_cluster_id = 0;
  // our target_cluster is first Replica
  const unsigned target_cluster_id = 1;
  const std::string router_options =
      R"({"target_cluster" : "00000000-0000-0000-0000-0000000000g2"})";

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.primary_cluster_id = primary_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = router_options;
  create_clusterset(cs_options);

  /*auto &router =*/launch_router(cs_options.topology);

  SCOPED_TRACE(
      "// Make the connections to both RW and RO ports, RW should not be "
      "possible as our target_cluster is REPLICA cluster, RO should be routed "
      "to our target_cluster");
  verify_new_connection_fails(router_port_rw);
  auto ro_con1_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con1_res->get(), cs_options.topology.clusters[target_cluster_id]
                              .nodes[kRWNodeId]
                              .classic_port));
  auto ro_con1 = std::move(*ro_con1_res);

  ////////////////////////////////////
  SCOPED_TRACE(
      "// Change the primary cluster in the metadata, now the SECOND REPLICA "
      "Cluster becomes the PRIMARY");
  ////////////////////////////////////

  primary_cluster_id = 2;
  change_clusterset_primary(cs_options.topology, primary_cluster_id);
  cs_options.view_id = ++view_id;
  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  SCOPED_TRACE("// Our existing RO connection should still be fine");
  verify_existing_connection_ok(ro_con1.get());

  SCOPED_TRACE(
      "// Check that new RO connection is directed to the same Cluster and no "
      "new RW connection is possible");
  // +1%3 because we round-robin and we now have 3 RO nodes
  auto ro_con2_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con2_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con2_res->get(), cs_options.topology.clusters[target_cluster_id]
                              .nodes[(kRWNodeId + 1) % 3]
                              .classic_port));
  auto ro_con2 = std::move(*ro_con2_res);

  verify_new_connection_fails(router_port_rw);

  ////////////////////////////////////
  SCOPED_TRACE(
      "// Change the primary cluster in the metadata, now the FIRST REPLICA "
      "which happens to be our target cluster becomes PRIMARY");
  ////////////////////////////////////
  primary_cluster_id = 1;
  change_clusterset_primary(cs_options.topology, primary_cluster_id);
  cs_options.view_id = ++view_id;
  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  SCOPED_TRACE("// Both existing RO connections should be fine");
  verify_existing_connection_ok(ro_con1.get());
  verify_existing_connection_ok(ro_con2.get());

  SCOPED_TRACE(
      "// Check that new RO connection is directed to the same Cluster and now "
      "RW connection is possible");
  auto rw_con_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con_res->get(), cs_options.topology.clusters[target_cluster_id]
                             .nodes[kRWNodeId]
                             .classic_port));
  auto rw_con = std::move(*rw_con_res);

  // +2%2 because we round-robin and we now have 2 RO nodes
  auto ro_con3_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con3_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con3_res->get(), cs_options.topology.clusters[target_cluster_id]
                              .nodes[kRONodeId + 2 % 2]
                              .classic_port));
  auto ro_con3 = std::move(*ro_con3_res);

  ////////////////////////////////////
  SCOPED_TRACE(
      "// Change the primary cluster in the metadata, let the original PRIMARY "
      "be the primary again");
  ////////////////////////////////////
  primary_cluster_id = 0;
  change_clusterset_primary(cs_options.topology, primary_cluster_id);
  cs_options.view_id = ++view_id;
  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  SCOPED_TRACE("// Check that all the existing RO connections are OK");
  verify_existing_connection_ok(ro_con1.get());
  verify_existing_connection_ok(ro_con2.get());
  verify_existing_connection_ok(ro_con3.get());
  SCOPED_TRACE(
      "// Check that RW connection got dropped as our target_cluster is not "
      "PRIMARY anymore");
  verify_existing_connection_dropped(rw_con.get());

  SCOPED_TRACE(
      "// Check that new RO connection is possible, new RW connection is not "
      "possible");
  verify_new_connection_fails(router_port_rw);
  auto ro_con4_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con4_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con4_res->get(), cs_options.topology.clusters[target_cluster_id]
                              .nodes[kRONodeId]
                              .classic_port));
}

class ViewIdChangesTest
    : public ClusterSetTest,
      public ::testing::WithParamInterface<TargetClusterTestParams> {};

/**
 * @test Check that the Router correctly notices the view_id changes and
 * applies the new metadata according to them.
 * [@FR8]
 * [@FR8.1]
 */
TEST_P(ViewIdChangesTest, ViewIdChanges) {
  const int target_cluster_id = GetParam().target_cluster_id;
  const std::string target_cluster = GetParam().target_cluster;
  const std::string router_options =
      R"({"target_cluster" : ")" + target_cluster + "\" }";

  SCOPED_TRACE(
      "// We start wtih view_id=1, all the clusterset nodes are metadata "
      "servers");

  ClusterSetOptions cs_options;
  cs_options.view_id = 1;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = router_options;
  create_clusterset(cs_options);

  auto &router = launch_router(cs_options.topology);
  EXPECT_EQ(9u, cs_options.topology.get_md_servers_classic_ports().size());

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  check_state_file(router_state_file, mysqlrouter::ClusterType::GR_CS,
                   cs_options.topology.uuid,
                   cs_options.topology.get_md_servers_classic_ports(), view_id);

  SCOPED_TRACE(
      "// Now let's make some change in the metadata (remove second node in "
      "the second replicaset) and let know only first REPLICA cluster about "
      "that");

  cs_options.topology.remove_node("00000000-0000-0000-0000-000000000033");
  EXPECT_EQ(8u, cs_options.topology.get_md_servers_classic_ports().size());

  cs_options.view_id = ++view_id;
  set_mock_clusterset_metadata(
      cs_options.topology.clusters[1].nodes[0].http_port,
      /*this_cluster_id*/ 1,
      /*this_node_id*/ 0, cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  SCOPED_TRACE(
      "// Check that the Router has seen the change and that it is reflected "
      "in the state file");

  check_state_file(router_state_file, mysqlrouter::ClusterType::GR_CS,
                   cs_options.topology.uuid,
                   cs_options.topology.get_md_servers_classic_ports(), view_id);

  SCOPED_TRACE("// Check that information about outdated view id is logged");
  const std::string pattern =
      "INFO .* Metadata server 127.0.0.1:" +
      std::to_string(cs_options.topology.clusters[0].nodes[0].classic_port) +
      " has outdated metadata view_id = " + std::to_string(view_id - 1) +
      ", current view_id = " + std::to_string(view_id) + ", ignoring";

  EXPECT_TRUE(wait_log_contains(router, pattern, 5s)) << pattern;

  SCOPED_TRACE(
      "// Let's make another change in the metadata (remove second node in "
      "the first replicaset) and let know only second REPLICA cluster about "
      "that");

  cs_options.topology.remove_node("00000000-0000-0000-0000-000000000023");
  EXPECT_EQ(7u, cs_options.topology.get_md_servers_classic_ports().size());

  cs_options.view_id = ++view_id;
  set_mock_clusterset_metadata(
      cs_options.topology.clusters[2].nodes[0].http_port,
      /*this_cluster_id*/ 2,
      /*this_node_id*/ 0, cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  SCOPED_TRACE(
      "// Check that the Router has seen the change and that it is reflected "
      "in the state file");

  check_state_file(router_state_file, mysqlrouter::ClusterType::GR_CS,
                   cs_options.topology.uuid,
                   cs_options.topology.get_md_servers_classic_ports(), view_id);

  SCOPED_TRACE(
      "// Let's propagate the last change to all nodes in the ClusterSet");
  set_mock_metadata_on_all_cs_nodes(cs_options);

  // state file should not change
  SCOPED_TRACE(
      "// Check that the Router has seen the change and that it is reflected "
      "in the state file");

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  check_state_file(router_state_file, mysqlrouter::ClusterType::GR_CS,
                   cs_options.topology.uuid,
                   cs_options.topology.get_md_servers_classic_ports(), view_id);
}

INSTANTIATE_TEST_SUITE_P(ViewIdChanges, ViewIdChangesTest,
                         ::testing::Values(
                             // [@TS_R11_1]
                             TargetClusterTestParams{"primary", 0},
                             // [@TS_R11_2]
                             TargetClusterTestParams{
                                 "00000000-0000-0000-0000-0000000000g2", 1}));

/**
 * @test Check that when 2 clusters claim they are both PRIMARY, Router follows
 * the one that has a highier view_id
 * [@FR9]
 * [@TS_R11_3]
 */
TEST_F(ClusterSetTest, TwoPrimaryClustersHighierViewId) {
  const std::string router_options = R"({"target_cluster" : "primary"})";
  SCOPED_TRACE(
      "// We configure Router to follow PRIMARY cluster, first cluster starts "
      "as a PRIMARY");

  ClusterSetOptions cs_options;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = router_options;
  create_clusterset(cs_options);

  /*auto &router =*/launch_router(cs_options.topology);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[kPrimaryClusterId].nodes[0].http_port, 2));

  auto rw_con1_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con1_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                              .nodes[kRWNodeId]
                              .classic_port));
  auto rw_con1 = std::move(*rw_con1_res);

  auto ro_con1_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con1_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                              .nodes[kRONodeId]
                              .classic_port));

  auto ro_con1 = std::move(*ro_con1_res);

  SCOPED_TRACE(
      "// Now let's make first REPLICA to claim that it's also a primary. But "
      "it has a highier view so the Router should believe the REPLICA");

  change_clusterset_primary(cs_options.topology, kFirstReplicaClusterId);
  cs_options.view_id = ++view_id;
  cs_options.target_cluster_id = kFirstReplicaClusterId;
  for (unsigned node_id = 0;
       node_id <
       cs_options.topology.clusters[kFirstReplicaClusterId].nodes.size();
       ++node_id) {
    set_mock_clusterset_metadata(
        cs_options.topology.clusters[kFirstReplicaClusterId]
            .nodes[node_id]
            .http_port,
        /*this_cluster_id*/ kFirstReplicaClusterId,
        /*this_node_id*/ node_id, cs_options);
  }

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[kFirstReplicaClusterId].nodes[0].http_port,
      2));

  SCOPED_TRACE(
      "// Check that the Router has seen the change and that it is reflected "
      "in the state file");

  check_state_file(router_state_file, mysqlrouter::ClusterType::GR_CS,
                   cs_options.topology.uuid,
                   cs_options.topology.get_md_servers_classic_ports(), view_id);

  SCOPED_TRACE(
      "// Check that the Router now uses new PRIMARY as a target cluster - "
      "existing connections dropped, new one directed to second Cluster");

  verify_existing_connection_dropped(rw_con1.get());
  verify_existing_connection_dropped(ro_con1.get());

  auto rw_con2_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con2_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con2_res->get(), cs_options.topology.clusters[kFirstReplicaClusterId]
                              .nodes[kRWNodeId]
                              .classic_port));
  auto rw_con2 = std::move(*rw_con2_res);

  // +1 as we round-dobin and this is already a second connection
  auto ro_con2_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con2_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con2_res->get(), cs_options.topology.clusters[kFirstReplicaClusterId]
                              .nodes[kRONodeId + 1]
                              .classic_port));
  auto ro_con2 = std::move(*ro_con2_res);

  SCOPED_TRACE(
      "// Now let's bump the old PRIMARY's view_id up, it should become again "
      "our target_cluster");

  change_clusterset_primary(cs_options.topology, kPrimaryClusterId);
  cs_options.view_id = ++view_id;
  cs_options.target_cluster_id = kPrimaryClusterId;
  for (unsigned node_id = 0;
       node_id < cs_options.topology.clusters[kPrimaryClusterId].nodes.size();
       ++node_id) {
    set_mock_clusterset_metadata(cs_options.topology.clusters[kPrimaryClusterId]
                                     .nodes[node_id]
                                     .http_port,
                                 /*this_cluster_id*/ kPrimaryClusterId,
                                 /*this_node_id*/ node_id, cs_options);
  }

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[kPrimaryClusterId].nodes[0].http_port, 2));

  SCOPED_TRACE(
      "// Check that the Router has seen the change and that it is reflected "
      "in the state file");

  check_state_file(router_state_file, mysqlrouter::ClusterType::GR_CS,
                   cs_options.topology.uuid,
                   cs_options.topology.get_md_servers_classic_ports(), view_id);

  SCOPED_TRACE(
      "// Check that the Router now uses original PRIMARY as a target cluster "
      "- "
      "existing connections dropped, new one directed to first Cluster");

  verify_existing_connection_dropped(rw_con2.get());
  verify_existing_connection_dropped(ro_con2.get());

  auto rw_con3_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con3_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con3_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                              .nodes[kRWNodeId]
                              .classic_port));

  // +1 as we round-dobin and this is already a second connection
  auto ro_con3_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con3_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con3_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                              .nodes[kRONodeId]
                              .classic_port));
}

/**
 * @test Check that when 2 clusters claim they are both PRIMARY, Router follows
 * the one that has a highier view_id
 * [@FR9]
 * [@TS_R11_4]
 */
TEST_F(ClusterSetTest, TwoPrimaryClustersLowerViewId) {
  view_id = 1;

  SCOPED_TRACE(
      "// We configure Router to follow PRIMARY cluster, first cluster starts "
      "as a PRIMARY");

  ClusterSetOptions cs_options;
  cs_options.view_id = view_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = R"({"target_cluster" : "primary"})";
  create_clusterset(cs_options);

  /*auto &router =*/launch_router(cs_options.topology);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[kPrimaryClusterId].nodes[0].http_port, 2));

  auto rw_con1_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con1_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                              .nodes[kRWNodeId]
                              .classic_port));
  auto rw_con1 = std::move(*rw_con1_res);

  auto ro_con1_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con1_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                              .nodes[kRONodeId]
                              .classic_port));
  auto ro_con1 = std::move(*ro_con1_res);

  SCOPED_TRACE(
      "// Now let's make first REPLICA to claim that it's also a primary. But "
      "it has a lower view so the Router should not take that into account");

  change_clusterset_primary(cs_options.topology, kFirstReplicaClusterId);
  cs_options.view_id = view_id - 1;
  cs_options.target_cluster_id = kFirstReplicaClusterId;
  for (unsigned node_id = 0;
       node_id <
       cs_options.topology.clusters[kFirstReplicaClusterId].nodes.size();
       ++node_id) {
    set_mock_clusterset_metadata(
        cs_options.topology.clusters[kFirstReplicaClusterId]
            .nodes[node_id]
            .http_port,
        /*this_cluster_id*/ kFirstReplicaClusterId,
        /*this_node_id*/ node_id, cs_options);
  }

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[kFirstReplicaClusterId].nodes[0].http_port,
      2));

  SCOPED_TRACE("// Check that the state file did not change");

  change_clusterset_primary(cs_options.topology, kPrimaryClusterId);
  check_state_file(router_state_file, mysqlrouter::ClusterType::GR_CS,
                   cs_options.topology.uuid,
                   cs_options.topology.get_md_servers_classic_ports(), view_id);

  SCOPED_TRACE(
      "// Check that existing connections are still open and the original "
      "PRIMARY is used for new ones");

  verify_existing_connection_ok(rw_con1.get());
  verify_existing_connection_ok(ro_con1.get());

  auto rw_con2_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con2_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con2_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                              .nodes[kRWNodeId]
                              .classic_port));

  // +1 as we round-robin and this is already a second connection
  auto ro_con2_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con2_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con2_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                              .nodes[kRONodeId + 1]
                              .classic_port));
}

struct InvalidatedClusterTestParams {
  std::string invalidated_cluster_routing_policy;
  bool expected_ro_connections_allowed;
};

class PrimaryTargetClusterMarkedInvalidInTheMetadataTest
    : public ClusterSetTest,
      public ::testing::WithParamInterface<InvalidatedClusterTestParams> {};

/**
 * @test Check that when target_cluster is marked as invalidated in the metadata
 * the Router either handles only RO connections or no connections at all
 * depending on the invalidatedClusterRoutingPolicy
 * Also checks that the Router does not do internal UPDATE (last_check_in)
 * queries on the invalidated cluster.
 * [@FR11]
 * [@TS_R15_1-3]
 */
TEST_P(PrimaryTargetClusterMarkedInvalidInTheMetadataTest,
       TargetClusterIsPrimary) {
  view_id = 1;
  const std::string policy = GetParam().invalidated_cluster_routing_policy;
  const bool ro_allowed = GetParam().expected_ro_connections_allowed;

  SCOPED_TRACE("// We configure Router to follow the PRIMARY cluster");

  ClusterSetOptions cs_options;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = R"({"target_cluster" : "primary",
        "stats_updates_frequency": 1})";
  create_clusterset(cs_options);

  /* auto &router = */ launch_router(cs_options.topology);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[kPrimaryClusterId].nodes[0].http_port, 2));

  auto rw_con1_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con1_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                              .nodes[kRWNodeId]
                              .classic_port));
  auto rw_con1 = std::move(*rw_con1_res);

  auto ro_con1_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con1_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                              .nodes[kRONodeId]
                              .classic_port));
  auto ro_con1 = std::move(*ro_con1_res);

  SCOPED_TRACE(
      "// Mark our PRIMARY cluster as invalidated in the metadata, also set "
      "the selected invalidatedClusterRoutingPolicy");
  cs_options.topology.clusters[kPrimaryClusterId].invalid = true;

  cs_options.view_id = ++view_id;
  cs_options.target_cluster_id = kPrimaryClusterId;
  cs_options.router_options =
      R"({"target_cluster" : "primary", "stats_updates_frequency": 1,
      "invalidated_cluster_policy" : ")" +
      policy + "\" }";

  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[kPrimaryClusterId].nodes[0].http_port, 2));

  SCOPED_TRACE(
      "// Check that existing RW connections are down and no new are possible");

  verify_existing_connection_dropped(rw_con1.get());
  verify_new_connection_fails(router_port_rw);

  SCOPED_TRACE(
      "// Check that RO connections are possible or not depending on the "
      "configured policy");
  if (!ro_allowed) {
    verify_existing_connection_dropped(ro_con1.get());
    verify_new_connection_fails(router_port_ro);
  } else {
    verify_existing_connection_ok(ro_con1.get());

    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                             .nodes[kRONodeId]
                             .classic_port));
  }

  // Primary cluster is invalidated - Router should not do any UPDATE operations
  // on it
  verify_no_last_check_in_updates(cs_options.topology, 1500ms);
}

INSTANTIATE_TEST_SUITE_P(
    TargetClusterIsPrimary, PrimaryTargetClusterMarkedInvalidInTheMetadataTest,
    ::testing::Values(
        // policy empty, default should be dropAll so RO connections are not
        // allowed
        InvalidatedClusterTestParams{"", false},
        // unsupported policy name, again expect the default behavior
        InvalidatedClusterTestParams{"unsupported", false},
        // explicitly set dropAll, no RO connections allowed again
        InvalidatedClusterTestParams{"drop_all", false},
        // accept_ro policy in  the metadata, RO connections are allowed
        InvalidatedClusterTestParams{"accept_ro", true}));

class ReplicaTargetClusterMarkedInvalidInTheMetadataTest
    : public ClusterSetTest,
      public ::testing::WithParamInterface<InvalidatedClusterTestParams> {};

/**
 * @test Check that when target_cluster is Replica and it is marked as invalid
 * in the metadata along with the current Primary, the invalidate policy is
 * honored. Also check that the periodic updates are performed on the new
 * Primary.
 */
TEST_P(ReplicaTargetClusterMarkedInvalidInTheMetadataTest,
       TargetClusterIsReplica) {
  view_id = 1;
  const std::string policy = GetParam().invalidated_cluster_routing_policy;
  const bool ro_allowed = GetParam().expected_ro_connections_allowed;

  SCOPED_TRACE("// We configure Router to follow the first REPLICA cluster");

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = kFirstReplicaClusterId;
  cs_options.primary_cluster_id = kPrimaryClusterId;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options =
      R"({"target_cluster" : "00000000-0000-0000-0000-0000000000g2",
          "stats_updates_frequency": 1})";
  create_clusterset(cs_options);

  /* auto &router = */ launch_router(cs_options.topology);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[kFirstReplicaClusterId].nodes[0].http_port,
      2));

  verify_new_connection_fails(router_port_rw);

  auto ro_con1_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con1_res->get(), cs_options.topology.clusters[kFirstReplicaClusterId]
                              .nodes[0]
                              .classic_port));

  auto ro_con1 = std::move(*ro_con1_res);

  verify_only_primary_gets_updates(cs_options.topology, kPrimaryClusterId);

  SCOPED_TRACE(
      "// Simulate the invalidating scenario: clusters PRIMARY and REPLICA1 "
      "become invalid, REPLICA2 is a new PRIMARY");
  cs_options.topology.clusters[kPrimaryClusterId].invalid = true;
  cs_options.topology.clusters[kFirstReplicaClusterId].invalid = true;
  change_clusterset_primary(cs_options.topology, kSecondReplicaClusterId);
  const auto &second_replica =
      cs_options.topology.clusters[kSecondReplicaClusterId];
  size_t node_id = 0;
  cs_options.view_id = ++view_id;
  cs_options.target_cluster_id = kFirstReplicaClusterId;
  cs_options.router_options =
      R"({"target_cluster" : "00000000-0000-0000-0000-0000000000g2",
          "stats_updates_frequency": 1,
          "invalidated_cluster_policy" : ")" +
      policy + "\" }";
  for (const auto &node : second_replica.nodes) {
    const auto http_port = node.http_port;
    set_mock_clusterset_metadata(http_port,
                                 /*this_cluster_id*/ second_replica.id,
                                 /*this_node_id*/ node_id, cs_options);
    node_id++;
  }

  EXPECT_TRUE(wait_for_transaction_count_increase(
      second_replica.nodes[0].http_port, 2));

  SCOPED_TRACE(
      "// Check that making a new RW connection is still not possible");
  verify_new_connection_fails(router_port_rw);

  SCOPED_TRACE(
      "// Check that RO connections are possible or not depending on the "
      "configured policy");
  if (!ro_allowed) {
    verify_existing_connection_dropped(ro_con1.get());
    verify_new_connection_fails(router_port_ro);
  } else {
    verify_existing_connection_ok(ro_con1.get());

    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(), cs_options.topology.clusters[kFirstReplicaClusterId]
                             .nodes[1]
                             .classic_port));
  }

  // make sure only new PRIMARY (former REPLICA2) gets the periodic updates now
  verify_only_primary_gets_updates(cs_options.topology,
                                   kSecondReplicaClusterId);
}

INSTANTIATE_TEST_SUITE_P(
    TargetClusterIsReplica, ReplicaTargetClusterMarkedInvalidInTheMetadataTest,
    ::testing::Values(
        // explicitly set dropAll, no RO connections allowed again
        InvalidatedClusterTestParams{"drop_all", false},
        // accept_ro policy in  the metadata, RO connections are allowed
        InvalidatedClusterTestParams{"accept_ro", true}));

/**
 * @test Check that the changes to the ClusterSet topology are reflected in the
 * state file in the runtime.
 * [@FR12]
 * [@TS_R13_1]
 */
TEST_F(ClusterSetTest, StateFileMetadataServersChange) {
  // also check if we handle view_id grater than 2^32 correctly
  uint64_t view_id = std::numeric_limits<uint32_t>::max() + 1;
  const std::string router_options = R"({"target_cluster" : "primary"})";

  ClusterSetOptions cs_options;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = router_options;
  create_clusterset(cs_options);

  const auto original_cs_topology = cs_options.topology;

  SCOPED_TRACE("// Launch Router with target_cluster=primary");
  /*auto &router =*/launch_router(cs_options.topology);

  check_state_file(router_state_file, mysqlrouter::ClusterType::GR_CS,
                   cs_options.topology.uuid,
                   cs_options.topology.get_md_servers_classic_ports(), view_id);

  SCOPED_TRACE(
      "// Remove second Replica Cluster nodes one by one and check that it is "
      "reflected in the state file");

  for (unsigned node_id = 1; node_id <= 3; ++node_id) {
    // remove node from the metadata
    cs_options.topology.remove_node("00000000-0000-0000-0000-00000000003" +
                                    std::to_string(node_id));
    cs_options.view_id = ++view_id;
    // update each remaining node with that metadata
    set_mock_metadata_on_all_cs_nodes(cs_options);

    // wait for the Router to refresh the metadata
    EXPECT_TRUE(wait_for_transaction_count_increase(
        cs_options.topology.clusters[kPrimaryClusterId].nodes[0].http_port, 2));

    // check that the list of the nodes is reflected in the state file
    EXPECT_EQ(9 - node_id,
              cs_options.topology.get_md_servers_classic_ports().size());
    check_state_file(router_state_file, mysqlrouter::ClusterType::GR_CS,
                     cs_options.topology.uuid,
                     cs_options.topology.get_md_servers_classic_ports(),
                     view_id);
  }

  SCOPED_TRACE("// Check that we can still connect to the Primary");
  {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                             .nodes[kRWNodeId]
                             .classic_port));
  }
  {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                             .nodes[kRONodeId]
                             .classic_port));
  }
  SCOPED_TRACE(
      "// Remove Primary Cluster nodes one by one and check that it is "
      "reflected in the state file");

  for (unsigned node_id = 1; node_id <= 3; ++node_id) {
    // remove node from the metadata
    cs_options.topology.remove_node("00000000-0000-0000-0000-00000000001" +
                                    std::to_string(node_id));
    cs_options.view_id = ++view_id;
    // update each remaining node with that metadata
    set_mock_metadata_on_all_cs_nodes(cs_options);

    // wait for the Router to refresh the metadata
    EXPECT_TRUE(wait_for_transaction_count_increase(
        cs_options.topology.clusters[kFirstReplicaClusterId].nodes[0].http_port,
        2));

    // check that the list of the nodes is reflected in the state file
    EXPECT_EQ(9 - 3 - node_id,
              cs_options.topology.get_md_servers_classic_ports().size());
    check_state_file(router_state_file, mysqlrouter::ClusterType::GR_CS,
                     cs_options.topology.uuid,
                     cs_options.topology.get_md_servers_classic_ports(),
                     view_id);
  }

  verify_new_connection_fails(router_port_rw);
  verify_new_connection_fails(router_port_ro);

  SCOPED_TRACE(
      "// Remove First Replica Cluster nodes one by one and check that it is "
      "reflected in the state file");

  for (unsigned node_id = 2; node_id <= 3; ++node_id) {
    // remove node from the metadata
    cs_options.topology.remove_node("00000000-0000-0000-0000-00000000002" +
                                    std::to_string(node_id));
    cs_options.view_id = ++view_id;
    // update each remaining node with that metadata
    set_mock_metadata_on_all_cs_nodes(cs_options);

    // wait for the Router to refresh the metadata
    EXPECT_TRUE(wait_for_transaction_count_increase(
        cs_options.topology.clusters[kFirstReplicaClusterId].nodes[0].http_port,
        2));

    // check that the list of the nodes is reflected in the state file
    EXPECT_EQ(4 - node_id,
              cs_options.topology.get_md_servers_classic_ports().size());

    check_state_file(router_state_file, mysqlrouter::ClusterType::GR_CS,
                     cs_options.topology.uuid,
                     cs_options.topology.get_md_servers_classic_ports(),
                     view_id);
  }

  SCOPED_TRACE(
      "// Remove the last node, that should not be reflected in the state file "
      "as Router never writes empty list to the state file");
  cs_options.topology.remove_node("00000000-0000-0000-0000-000000000021");

  cs_options.view_id = ++view_id;
  set_mock_clusterset_metadata(
      original_cs_topology.clusters[kFirstReplicaClusterId].nodes[0].http_port,
      /*this_cluster_id*/ 1, /*this_node_id*/ 0, cs_options);
  // wait for the Router to refresh the metadata
  EXPECT_TRUE(wait_for_transaction_count_increase(
      original_cs_topology.clusters.at(kFirstReplicaClusterId)
          .nodes.at(0)
          .http_port,
      2));

  // check that the list of the nodes is NOT reflected in the state file
  EXPECT_EQ(0, cs_options.topology.get_md_servers_classic_ports().size());
  const std::vector<uint16_t> expected_port{
      original_cs_topology.clusters.at(kFirstReplicaClusterId)
          .nodes.at(0)
          .classic_port};
  check_state_file(router_state_file, mysqlrouter::ClusterType::GR_CS,
                   cs_options.topology.uuid, expected_port, view_id - 1);

  verify_new_connection_fails(router_port_rw);
  verify_new_connection_fails(router_port_ro);

  SCOPED_TRACE("// Restore Primary Cluster nodes one by one");

  for (unsigned node_id = 1; node_id <= 3; ++node_id) {
    cs_options.topology.add_node(
        kPrimaryClusterId,
        original_cs_topology.clusters[kPrimaryClusterId].nodes[node_id - 1]);
    cs_options.view_id = ++view_id;
    // update each node with that metadata
    set_mock_metadata_on_all_cs_nodes(cs_options);

    // if this is the first node that we are adding back we also need to set it
    // in our last standing metadata server which is no longer part of the
    // clusterset
    if (node_id == 1) {
      const auto http_port =
          original_cs_topology.clusters[kFirstReplicaClusterId]
              .nodes[0]
              .http_port;
      cs_options.target_cluster_id = kPrimaryClusterId;
      set_mock_clusterset_metadata(http_port, /*this_cluster_id*/ 1,
                                   /*this_node_id*/ 0, cs_options);
    }

    // wait for the Router to refresh the metadata
    EXPECT_TRUE(wait_for_transaction_count_increase(
        cs_options.topology.clusters[kPrimaryClusterId].nodes[0].http_port, 2));

    // check that the list of the nodes is reflected in the state file
    EXPECT_EQ(node_id,
              cs_options.topology.get_md_servers_classic_ports().size());
    check_state_file(router_state_file, mysqlrouter::ClusterType::GR_CS,
                     cs_options.topology.uuid,
                     cs_options.topology.get_md_servers_classic_ports(),
                     view_id);
  }

  SCOPED_TRACE("// The connections via the Router should be possible again");
  {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                             .nodes[kRWNodeId]
                             .classic_port));
  }

  {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                             .nodes[kRONodeId + 1]
                             .classic_port));
  }
}

/**
 * @test Check that the Router works correctly when can't access some metadata
 * servers.
 * [@FR10]
 * [@TS_R11_5]
 */
TEST_F(ClusterSetTest, SomeMetadataServerUnaccessible) {
  uint64_t view_id = 1;
  const std::string router_options = R"({"target_cluster" : "primary"})";

  ClusterSetOptions cs_options;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = router_options;
  create_clusterset(cs_options);

  SCOPED_TRACE("// Launch Router with target_cluster=primary");
  /*auto &router =*/launch_router(cs_options.topology);

  auto rw_con1_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con1_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                              .nodes[kRWNodeId]
                              .classic_port));
  auto rw_con1 = std::move(*rw_con1_res);

  auto ro_con1_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con1_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      ro_con1_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                              .nodes[kRONodeId]
                              .classic_port));
  auto ro_con1 = std::move(*ro_con1_res);

  SCOPED_TRACE("// Make the first Replica Cluster nodes unaccessible");
  for (unsigned node_id = 0; node_id < 3; ++node_id) {
    cs_options.topology.clusters[kFirstReplicaClusterId]
        .nodes[node_id]
        .process->kill();
  }

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[kPrimaryClusterId].nodes[0].http_port, 2));

  SCOPED_TRACE("// Bump up the view_id on the second Replica (remove First)");
  size_t node_id = 0;
  cs_options.view_id = ++view_id;
  cs_options.target_cluster_id = kPrimaryClusterId;
  for (const auto &node :
       cs_options.topology.clusters[kSecondReplicaClusterId].nodes) {
    const auto http_port = node.http_port;
    set_mock_clusterset_metadata(http_port,
                                 /*this_cluster_id*/ kSecondReplicaClusterId,
                                 /*this_node_id */ node_id, cs_options);
    node_id++;
  }

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[kSecondReplicaClusterId].nodes[0].http_port,
      2));

  SCOPED_TRACE(
      "// The existing connections should still be alive, new ones should be "
      "possible");
  verify_existing_connection_ok(rw_con1.get());
  verify_existing_connection_ok(ro_con1.get());

  {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                             .nodes[kRWNodeId]
                             .classic_port));
  }

  {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(), cs_options.topology.clusters[kPrimaryClusterId]
                             .nodes[kRONodeId + 1]
                             .classic_port));
  }
}

/**
 * @test Checks that "use_replica_primary_as_rw" router options from the
 * metadata is handled properly when the target cluster is Replica
 */
TEST_F(ClusterSetTest, UseReplicaPrimaryAsRwNode) {
  const int target_cluster_id = 1;

  std::string router_options =
      R"({"target_cluster" : "00000000-0000-0000-0000-0000000000g2",
          "use_replica_primary_as_rw": false})";

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = router_options;
  create_clusterset(cs_options);

  const auto primary_node_http_port =
      cs_options.topology.clusters[0].nodes[0].http_port;

  SCOPED_TRACE("// Launch the Router");
  /*auto &router =*/launch_router(cs_options.topology);

  SCOPED_TRACE(
      "// Make the connections to both RW and RO ports and check if they are "
      "directed to expected nodes of the Replica Cluster");

  // 'use_replica_primary_as_rw' is false and our target cluster is Replica so
  // no RW connections should be possible
  verify_new_connection_fails(router_port_rw);

  // the Replica's primary should be used in rotation as a destination of the RO
  // connections
  for (size_t i = 0;
       i < cs_options.topology.clusters[target_cluster_id].nodes.size(); ++i) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(),
        cs_options.topology.clusters[target_cluster_id].nodes[i].classic_port));
  }

  // ==================================================================
  // now we set 'use_replica_primary_as_rw' to 'true' in the metadata
  cs_options.router_options =
      R"({"target_cluster" : "00000000-0000-0000-0000-0000000000g2",
          "use_replica_primary_as_rw": true})";

  set_mock_clusterset_metadata(primary_node_http_port, target_cluster_id, 0,
                               cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(primary_node_http_port, 2));

  std::vector<std::unique_ptr<MySQLSession>> rw_connections;
  std::vector<std::unique_ptr<MySQLSession>> ro_connections;
  // Now the RW connection should be ok and directed to the Replicas Primary
  for (size_t i = 0; i < 2; ++i) {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(),
        cs_options.topology.clusters[target_cluster_id].nodes[0].classic_port));

    rw_connections.push_back(std::move(*conn_res));
  }

  // The Replicas Primary should not be used as a destination for RO connections
  // now
  for (size_t i = 0; i < 4; ++i) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(), cs_options.topology.clusters[target_cluster_id]
                             .nodes[i % 2 + 1]
                             .classic_port));

    ro_connections.push_back(std::move(*conn_res));
  }

  // ==================================================================
  // set 'use_replica_primary_as_rw' to 'false'
  cs_options.router_options =
      R"({"target_cluster" : "00000000-0000-0000-0000-0000000000g2",
          "use_replica_primary_as_rw": false})";

  set_mock_clusterset_metadata(primary_node_http_port, target_cluster_id, 0,
                               cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(primary_node_http_port, 2));

  // check that the RW connections were dropped
  for (auto &con : rw_connections) {
    EXPECT_TRUE(wait_connection_dropped(*con));
  }

  // check that the RO connections are fine
  for (auto &con : ro_connections) {
    verify_existing_connection_ok(con.get());
  }

  // connections to the RW port should not be possible again
  verify_new_connection_fails(router_port_rw);

  // the Replica's primary should be used in rotation as a destination of the RO
  // connections
  const auto target_cluster_nodes =
      cs_options.topology.clusters[target_cluster_id].nodes.size();
  for (size_t i = 0; i < target_cluster_nodes; ++i) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(), cs_options.topology.clusters[target_cluster_id]
                             .nodes[i % target_cluster_nodes]
                             .classic_port));
  }
}

/**
 * @test Checks that "use_replica_primary_as_rw" router option from the
 * metadata is ignored when the target cluster is Primary
 */
TEST_F(ClusterSetTest, UseReplicaPrimaryAsRwNodeIgnoredIfTargetPrimary) {
  const int target_cluster_id = 0;  // our target is primary cluster
  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = R"({"target_cluster" : "primary",
          "use_replica_primary_as_rw": false})";
  create_clusterset(cs_options);

  SCOPED_TRACE("// Launch the Router");
  /*auto &router =*/launch_router(cs_options.topology);

  // 'use_replica_primary_as_rw' is 'false' but our target cluster is Primary so
  //  RW connections should be possible
  {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(),
        cs_options.topology.clusters[target_cluster_id].nodes[0].classic_port));
  }

  // the RO connections should be routed to the Secondary nodes of the Primary
  // Cluster
  for (size_t i = 0;
       i < cs_options.topology.clusters[target_cluster_id].nodes.size(); ++i) {
    auto conn_res = make_new_connection(router_port_ro);

    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(), cs_options.topology.clusters[target_cluster_id]
                             .nodes[1 + i % 2]
                             .classic_port));
  }

  // ==================================================================
  // set 'use_replica_primary_as_rw' to 'true'
  cs_options.router_options =
      R"({"target_cluster" : "primary",
          "use_replica_primary_as_rw": true})";

  const auto primary_node_http_port =
      cs_options.topology.clusters[0].nodes[0].http_port;
  set_mock_clusterset_metadata(primary_node_http_port, target_cluster_id, 0,
                               cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(primary_node_http_port, 2));

  // check that the behavior did not change
  {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(),
        cs_options.topology.clusters[target_cluster_id].nodes[0].classic_port));
  }
  // the RO connections should be routed to the Secondary nodes of the Primary
  // Cluster
  for (size_t i = 0;
       i < cs_options.topology.clusters[target_cluster_id].nodes.size(); ++i) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(), cs_options.topology.clusters[target_cluster_id]
                             .nodes[1 + (i + 1) % 2]
                             .classic_port));
  }
}

class ClusterSetUseReplicaPrimaryAsRwNodeInvalidTest
    : public ClusterSetTest,
      public ::testing::WithParamInterface<std::string> {};

/**
 * @test Checks that invalid values of "use_replica_primary_as_rw" in the
 * metadata are handled properly (default = false used) when the target
 * cluster is Replica
 */
TEST_P(ClusterSetUseReplicaPrimaryAsRwNodeInvalidTest,
       UseReplicaPrimaryAsRwNodeInvalid) {
  const int target_cluster_id = 1;

  std::string inv = "\"\"";
  std::string router_options =
      R"({"target_cluster" : "00000000-0000-0000-0000-0000000000g2",
          "use_replica_primary_as_rw": )" +
      GetParam() + "}";

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = router_options;
  create_clusterset(cs_options);

  SCOPED_TRACE("// Launch the Router");
  auto &router = launch_router(cs_options.topology);

  SCOPED_TRACE(
      "// Make the connections to both RW and RO ports and check if they are "
      "directed to expected nodes of the Replica Cluster");

  // 'use_replica_primary_as_rw' is false and our target cluster is Replica so
  // no RW connections should be possible
  verify_new_connection_fails(router_port_rw);

  // the Replica's primary should be used in rotation as a destination of the RO
  // connections
  for (size_t i = 0;
       i < cs_options.topology.clusters[target_cluster_id].nodes.size(); ++i) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(),
        cs_options.topology.clusters[target_cluster_id].nodes[i].classic_port));
  }

  const std::string warning =
      "WARNING .* Error parsing use_replica_primary_as_rw from the "
      "router.options: options.use_replica_primary_as_rw='" +
      GetParam() + "'; not a boolean. Using default value 'false'";

  EXPECT_TRUE(wait_log_contains(router, warning, 1s)) << warning;
}

INSTANTIATE_TEST_SUITE_P(UseReplicaPrimaryAsRwNodeInvalid,
                         ClusterSetUseReplicaPrimaryAsRwNodeInvalidTest,
                         ::testing::Values("\"\"", "0", "1", "\"foo\"",
                                           "\"false\""));
/**
 * @test Checks that switching between fetch_whole_topology on and off works as
 * expected when it comes to routing new connections and keeping/closing
 * existing ones
 */
TEST_F(ClusterSetTest, FetchWholeTopologyConnections) {
  const std::string target_cluster = "00000000-0000-0000-0000-0000000000g2";
  const auto target_cluster_id = 1;

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options =
      R"({"target_cluster" : ")" + target_cluster + "\" }";
  create_clusterset(cs_options);

  SCOPED_TRACE("// Launch the Router");
  /*auto &router = */ launch_router(cs_options.topology);

  // since our target cluster is replica we should not be able to make RW
  // connection
  verify_new_connection_fails(router_port_rw);

  // RO connections should be routed to the first replica
  std::vector<std::unique_ptr<MySQLSession>> ro_cons_to_target_cluster;
  for (const auto &node :
       cs_options.topology.clusters[kFirstReplicaClusterId].nodes) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(conn_res->get(), node.classic_port));

    ro_cons_to_target_cluster.emplace_back(std::move(*conn_res));
  }

  EXPECT_EQ(3, ro_cons_to_target_cluster.size());

  // switch the mode to fetch_whole_topology
  set_fetch_whole_topology(true);
  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 3));

  // since now the nodes pool is the superset of the previous pool the existing
  // RO connections should still be alive
  for (const auto &con : ro_cons_to_target_cluster) {
    verify_existing_connection_ok(con.get());
  }

  // there is RW node now in the available nodes pool (from Primary Cluster) so
  // the RW connection should be possible now
  auto rw_con_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      rw_con_res->get(),
      cs_options.topology.clusters[kPrimaryClusterId].nodes[0].classic_port));
  auto rw_con = std::move(*rw_con_res);

  // Let's make a bunch of new RO connections, they should go to the RO nodes of
  // all the Clusters of the ClusterSet since we are in the fetch_whole_topology
  // mode now
  std::vector<std::unique_ptr<MySQLSession>> ro_cons_to_primary;
  for (size_t i = 1;
       i < cs_options.topology.clusters[kPrimaryClusterId].nodes.size(); ++i) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(),
        cs_options.topology.clusters[kPrimaryClusterId].nodes[i].classic_port));

    ro_cons_to_primary.emplace_back(std::move(*conn_res));
  }
  EXPECT_EQ(2, ro_cons_to_primary.size());

  std::vector<std::unique_ptr<MySQLSession>> ro_cons_to_first_replica;
  for (size_t i = 1;
       i < cs_options.topology.clusters[kFirstReplicaClusterId].nodes.size();
       ++i) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(), cs_options.topology.clusters[kFirstReplicaClusterId]
                             .nodes[i]
                             .classic_port));
    ro_cons_to_first_replica.emplace_back(std::move(*conn_res));
  }
  EXPECT_EQ(2, ro_cons_to_first_replica.size());

  std::vector<std::unique_ptr<MySQLSession>> ro_cons_to_second_replica;
  for (size_t i = 1;
       i < cs_options.topology.clusters[kSecondReplicaClusterId].nodes.size();
       ++i) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        conn_res->get(), cs_options.topology.clusters[kSecondReplicaClusterId]
                             .nodes[i]
                             .classic_port));

    ro_cons_to_second_replica.emplace_back(std::move(*conn_res));
  }
  EXPECT_EQ(2, ro_cons_to_second_replica.size());

  // switch off the mode fetch_whole_topology
  set_fetch_whole_topology(false);
  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 3));

  // we are back in the "use only the target cluster" mode
  // the RW connection should be shut down
  verify_existing_connection_dropped(rw_con.get());

  // the RO connections to the Clusters other than our target_cluster should be
  // dropped too
  for (const auto &con : ro_cons_to_primary) {
    verify_existing_connection_dropped(con.get());
  }
  for (const auto &con : ro_cons_to_second_replica) {
    verify_existing_connection_dropped(con.get());
  }

  // the RO connections to our target_cluster should still be fine tho
  for (const auto &con : ro_cons_to_target_cluster) {
    verify_existing_connection_ok(con.get());
  }
  for (const auto &con : ro_cons_to_first_replica) {
    verify_existing_connection_ok(con.get());
  }

  // again no new RW connection should be possible
  verify_new_connection_fails(router_port_rw);
  // new RO connections should be directed to our target_cluster
  for (const auto &node :
       cs_options.topology.clusters[kFirstReplicaClusterId].nodes) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(conn_res->get(), node.classic_port));

    ro_cons_to_target_cluster.emplace_back(std::move(*conn_res));
  }
}

/**
 * @test Checks that switching between fetch_whole_topology on and off works as
 * expected when when GR notifications are in use
 */
TEST_F(ClusterSetTest, UseMultipleClustersGrNotifications) {
  const std::string target_cluster = "00000000-0000-0000-0000-0000000000g2";
  const auto target_cluster_id = 1;

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options =
      R"({"target_cluster" : ")" + target_cluster + "\" }";
  cs_options.use_gr_notifications = true;
  create_clusterset(cs_options);

  SCOPED_TRACE("// Launch the Router");
  auto &router =
      launch_router(cs_options.topology, EXIT_SUCCESS, 10s, kTTL, true);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  // we do not use multiple clusters yet, let's check that we opened GR
  // notification connections only to our target_cluster
  std::string log_content = router.get_logfile_content();
  for (auto &cluster : cs_options.topology.clusters) {
    for (auto &node : cluster.nodes) {
      const std::string log_entry =
          "Enabling GR notices for cluster '" + cluster.name +
          "' changes on node 127.0.0.1:" + std::to_string(node.x_port);

      const size_t expected_log_occurences =
          cluster.gr_uuid == target_cluster ? 1 : 0;
      EXPECT_EQ(expected_log_occurences,
                count_str_occurences(log_content, log_entry));
    }
  }

  // switch to use multiple clusters now
  set_fetch_whole_topology(true);
  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  // now we expect the GR notification listener to be opened once on each
  // ClusterSet node
  log_content = router.get_logfile_content();
  for (auto &cluster : cs_options.topology.clusters) {
    for (auto &node : cluster.nodes) {
      const std::string log_entry =
          "Enabling GR notices for cluster '" + cluster.name +
          "' changes on node 127.0.0.1:" + std::to_string(node.x_port);

      const size_t expected_log_occurences = 1;
      EXPECT_EQ(expected_log_occurences,
                count_str_occurences(log_content, log_entry));
    }
  }
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ProcessManager::set_origin(g_origin_path);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
