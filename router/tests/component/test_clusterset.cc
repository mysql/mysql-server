/*
Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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
#include "mysql/harness/utility/string.h"  // string_format
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "mysqlrouter/utils.h"
#include "rest_api_testutils.h"
#include "router_component_clusterset.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_config.h"  // MYSQL_ROUTER_VERSION
#include "router_test_helpers.h"
#include "routing_guidelines_builder.h"
#include "stdx_expected_no_error.h"
#include "tcp_port_pool.h"

using mysqlrouter::ClusterType;
using mysqlrouter::MySQLSession;
using ::testing::Contains;
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
        {"destinations",
         "metadata-cache://cluster-name-1/default?role=" + role},
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

  std::vector<uint16_t> get_cluster_classic_ro_nodes(
      const RouterComponentClusterSetTest::ClusterData &cluster) const {
    std::vector<std::uint16_t> res{cluster.nodes[kRONode1Id].classic_port,
                                   cluster.nodes[kRONode2Id].classic_port};

    if (cluster.role == "SECONDARY" || cluster.invalid)
      res.push_back(cluster.nodes[kRWNodeId].classic_port);
    return res;
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
  static const unsigned kRONode1Id = 1;
  static const unsigned kRONode2Id = 2;

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

TEST_F(ClusterSetTest, CloseConnectionAfterRefreshIsDefaultOff) {
  RecordProperty("Worklog", "16652");
  RecordProperty("RequirementId", "FR3");
  RecordProperty("Description",
                 "close-connection-after-refresh MUST default to 0. "
                 "But with cluster-set, it has no impact as the connection "
                 "switches between the servers.");
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

  // check the new target_cluster was reported only once
  const std::string needle = "New target cluster assigned in the metadata";
  const std::string log_content = router.get_logfile_content();

  // 1 is expected, that comes from the initial reading of the metadata
  EXPECT_EQ(1, count_str_occurences(log_content, needle));

  // should be more than 1 connect-attempt
  std::string server_globals =
      MockServerRestClient(cs_options.topology.clusters[0].nodes[0].http_port)
          .get_globals_as_json_string();

  EXPECT_GE(get_int_field_value(server_globals, "connects"), 2);
}

TEST_F(ClusterSetTest, CloseConnectionAfterRefreshOnWorks) {
  RecordProperty("Worklog", "16652");
  RecordProperty("RequirementId", "FR1");
  RecordProperty("Description",
                 "if close-connection-after-refresh is 1, the connection after "
                 "refresh MUST be closed."
                 "With cluster-set, it has no impact as the connection is "
                 "closed anyway as the cache switches between the servers.");
  const std::string target_cluster = "primary";
  const auto target_cluster_id = 0;

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options =
      R"({"target_cluster" : ")" + target_cluster + "\" }";
  create_clusterset(cs_options);

  auto cs_topology = cs_options.topology;
  bool use_gr_notifications = false;
  auto metadata_ttl = kTTL;

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

  router_conf_file = create_config_file(
      temp_test_dir.name(),
      metadata_cache_section(metadata_ttl, use_gr_notifications) +
          "close_connection_after_refresh=1\n" +
          routing_section(router_port_rw, "PRIMARY", "first-available") +
          routing_section(router_port_ro, "SECONDARY", "round-robin") +
          rest_sections,
      &default_section);

  SCOPED_TRACE("// Launch the Router");
  auto &router = router_spawner().spawn({"-c", router_conf_file});

  // keep the Router running for several md refresh rounds
  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 3));

  // check the new target_cluster was repoted only once
  const std::string needle = "New target cluster assigned in the metadata";
  const std::string log_content = router.get_logfile_content();

  // 1 is expected, that comes from the initial reading of the metadata
  EXPECT_EQ(1, count_str_occurences(log_content, needle));

  // should be more than 1 connect-attempt
  std::string server_globals =
      MockServerRestClient(cs_options.topology.clusters[0].nodes[0].http_port)
          .get_globals_as_json_string();

  EXPECT_GE(get_int_field_value(server_globals, "connects"), 2);
}

TEST_F(ClusterSetTest, CloseConnectionAfterRefreshOffWorks) {
  RecordProperty("Worklog", "16652");
  RecordProperty("RequirementId", "FR2.1");
  RecordProperty(
      "Description",
      "If close-connection-after-refresh is 0, the connection after refresh "
      "can stay open if it is same-server, but with cluster-set, it has "
      "no impact as the connection switches between the servers.");

  const std::string target_cluster = "primary";
  const auto target_cluster_id = 0;

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options =
      R"({"target_cluster" : ")" + target_cluster + "\" }";
  create_clusterset(cs_options);

  auto cs_topology = cs_options.topology;
  bool use_gr_notifications = false;
  auto metadata_ttl = kTTL;

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

  router_conf_file = create_config_file(
      temp_test_dir.name(),
      metadata_cache_section(metadata_ttl, use_gr_notifications) +
          "close_connection_after_refresh=0\n" +
          routing_section(router_port_rw, "PRIMARY", "first-available") +
          routing_section(router_port_ro, "SECONDARY", "round-robin") +
          rest_sections,
      &default_section);

  SCOPED_TRACE("// Launch the Router");
  auto &router = router_spawner().spawn({"-c", router_conf_file});

  // keep the Router running for several md refresh rounds
  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 3));

  // check the new target_cluster was repoted only once
  const std::string needle = "New target cluster assigned in the metadata";
  const std::string log_content = router.get_logfile_content();

  // 1 is expected, that comes from the initial reading of the metadata
  EXPECT_EQ(1, count_str_occurences(log_content, needle));

  // should be more than 1 connect-attempt as the clusterset implementation will
  // close the connection and go to another node.
  std::string server_globals =
      MockServerRestClient(cs_options.topology.clusters[0].nodes[0].http_port)
          .get_globals_as_json_string();

  EXPECT_GE(get_int_field_value(server_globals, "connects"), 2);
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

  auto ro_con1_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con1_res);
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con1_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology
                          .clusters[expected_initial_connection_cluster_id])));

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

  {
    auto ro_con2_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(ro_con2_res);
    ASSERT_NO_FATAL_FAILURE(verify_port(
        ro_con2_res->get(),
        get_cluster_classic_ro_nodes(
            cs_options.topology.clusters[expected_new_connection_cluster_id])));
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
      ro_con1_res->get(), get_cluster_classic_ro_nodes(
                              cs_options.topology.clusters[kTargetClusterId])));
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
      ro_con2_res->get(), get_cluster_classic_ro_nodes(
                              cs_options.topology.clusters[kTargetClusterId])));
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
      ro_con1_res->get(),
      get_cluster_classic_ro_nodes(cs_options.topology.clusters[0])));
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

  auto ro_con2_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con2_res);
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con2_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[primary_cluster_id])));
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

  auto ro_con3_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con3_res);
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con3_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[primary_cluster_id])));
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

  auto ro_con4_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con4_res);
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con4_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[primary_cluster_id])));
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
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con1_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[target_cluster_id])));
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
  auto ro_con2_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con2_res);
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con2_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[target_cluster_id])));
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
  auto ro_con3_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con3_res);
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con3_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[target_cluster_id])));
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
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con4_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[target_cluster_id])));
}

/**
 * @test Check that the Router sticks to the target_cluster given by UUID when
 * its role changes starting from REPLICA.
 */
TEST_F(ClusterSetTest, TargetClusterStickToReplicaUUID) {
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
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con1_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[target_cluster_id])));
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
  auto ro_con2_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con2_res);
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con2_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[target_cluster_id])));
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

  auto ro_con3_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con3_res);
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con3_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[target_cluster_id])));
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
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con4_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[target_cluster_id])));
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
 * the one that has a higher view_id
 * [@FR9]
 * [@TS_R11_3]
 */
TEST_F(ClusterSetTest, TwoPrimaryClustersHigherViewId) {
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
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con1_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[kPrimaryClusterId])));

  auto ro_con1 = std::move(*ro_con1_res);

  SCOPED_TRACE(
      "// Now let's make first REPLICA to claim that it's also a primary. But "
      "it has a higher view so the Router should believe the REPLICA");

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

  auto ro_con2_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con2_res);
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con2_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[kFirstReplicaClusterId])));
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

  auto ro_con3_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con3_res);
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con3_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[kPrimaryClusterId])));
}

/**
 * @test Check that when 2 clusters claim they are both PRIMARY, Router follows
 * the one that has a higher view_id
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
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con1_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[kPrimaryClusterId])));
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

  auto ro_con2_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con2_res);
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con2_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[kPrimaryClusterId])));
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
  cs_options.router_options =
      R"({"target_cluster" : "primary", "stats_updates_frequency": 1,
      "invalidated_cluster_policy" : ")" +
      policy + "\" }";
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
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con1_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[kPrimaryClusterId])));
  auto ro_con1 = std::move(*ro_con1_res);

  SCOPED_TRACE(
      "// Mark our PRIMARY cluster as invalidated in the metadata, also set "
      "the selected invalidatedClusterRoutingPolicy");
  cs_options.topology.clusters[kPrimaryClusterId].invalid = true;

  cs_options.view_id = ++view_id;
  cs_options.target_cluster_id = kPrimaryClusterId;

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
        conn_res->get(), get_cluster_classic_ro_nodes(
                             cs_options.topology.clusters[kPrimaryClusterId])));
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
          "stats_updates_frequency": 1,
          "invalidated_cluster_policy" : ")" +
      policy + "\" }";
  create_clusterset(cs_options);

  /* auto &router = */ launch_router(cs_options.topology);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[kFirstReplicaClusterId].nodes[0].http_port,
      2));

  verify_new_connection_fails(router_port_rw);

  auto ro_con1_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con1_res);
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con1_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[kFirstReplicaClusterId])));

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
    ASSERT_NO_FATAL_FAILURE(
        verify_port(conn_res->get(),
                    get_cluster_classic_ro_nodes(
                        cs_options.topology.clusters[kFirstReplicaClusterId])));
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
        conn_res->get(), get_cluster_classic_ro_nodes(
                             cs_options.topology.clusters[kPrimaryClusterId])));
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
        conn_res->get(), get_cluster_classic_ro_nodes(
                             cs_options.topology.clusters[kPrimaryClusterId])));
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
  ASSERT_NO_FATAL_FAILURE(
      verify_port(ro_con1_res->get(),
                  get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[kPrimaryClusterId])));
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
        conn_res->get(), get_cluster_classic_ro_nodes(
                             cs_options.topology.clusters[kPrimaryClusterId])));
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
        conn_res->get(), get_cluster_classic_ro_nodes(
                             cs_options.topology.clusters[target_cluster_id])));
  }

  // ==================================================================
  // now we set 'use_replica_primary_as_rw' to 'true' in the metadata
  cs_options.router_options =
      R"({"target_cluster" : "00000000-0000-0000-0000-0000000000g2",
          "use_replica_primary_as_rw": true})";

  set_mock_clusterset_metadata(primary_node_http_port, target_cluster_id, 0,
                               cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(primary_node_http_port, 3));

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
        conn_res->get(), get_cluster_classic_ro_nodes(
                             cs_options.topology.clusters[target_cluster_id])));

    ro_connections.push_back(std::move(*conn_res));
  }

  // ==================================================================
  // set 'use_replica_primary_as_rw' to 'false'
  cs_options.router_options =
      R"({"target_cluster" : "00000000-0000-0000-0000-0000000000g2",
          "use_replica_primary_as_rw": false})";

  set_mock_clusterset_metadata(primary_node_http_port, target_cluster_id, 0,
                               cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(primary_node_http_port, 3));

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
        conn_res->get(), get_cluster_classic_ro_nodes(
                             cs_options.topology.clusters[target_cluster_id])));
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
        conn_res->get(), get_cluster_classic_ro_nodes(
                             cs_options.topology.clusters[target_cluster_id])));
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
        conn_res->get(), get_cluster_classic_ro_nodes(
                             cs_options.topology.clusters[target_cluster_id])));
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

  // wait for a few md refresh rounds
  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 3));

  const std::string warning =
      "Error parsing use_replica_primary_as_rw from the "
      "router_options: options.use_replica_primary_as_rw='" +
      GetParam() + "'; not a boolean. Using default value 'false'";

  // the warning should be logged only once
  check_log_contains(router, warning, 1);
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
  for ([[maybe_unused]] const auto &node :
       cs_options.topology.clusters[kFirstReplicaClusterId].nodes) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(
        verify_port(conn_res->get(),
                    get_cluster_classic_ro_nodes(
                        cs_options.topology.clusters[kFirstReplicaClusterId])));

    ro_cons_to_target_cluster.emplace_back(std::move(*conn_res));
  }

  EXPECT_EQ(3, ro_cons_to_target_cluster.size());

  const std::string custom_routing_guidelines = guidelines_builder::create(
      {{"d1", "$.server.memberRole = PRIMARY"},
       {"d2",
        "$.server.memberRole = SECONDARY OR $.server.memberRole = REPLICA"}},
      {{"r1",
        "$.session.targetPort =" + std::to_string(router_port_rw),
        {{"round-robin", {"d1"}}}},
       {"r2",
        "$.session.targetPort =" + std::to_string(router_port_ro),
        {{"round-robin", {"d2", "d1"}}}}});

  SCOPED_TRACE("Set routing guidelines");
  // switch on the mode fetch_whole_topology by setting the routing guidelines
  for (unsigned cluster_id = 0;
       cluster_id < cs_options.topology.clusters.size(); ++cluster_id) {
    auto &cluster = cs_options.topology.clusters[cluster_id];
    for (unsigned node_id = 0; node_id < cluster.nodes.size(); ++node_id) {
      auto &node = cluster.nodes[node_id];

      set_mock_clusterset_metadata(node.http_port, cluster_id, node_id,
                                   cs_options, custom_routing_guidelines);
    }
  }

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
        conn_res->get(), get_cluster_classic_ro_nodes(
                             cs_options.topology.clusters[kPrimaryClusterId])));

    ro_cons_to_primary.emplace_back(std::move(*conn_res));
  }
  EXPECT_EQ(2, ro_cons_to_primary.size());

  std::vector<std::unique_ptr<MySQLSession>> ro_cons_to_first_replica;
  for (size_t i = 1;
       i < cs_options.topology.clusters[kFirstReplicaClusterId].nodes.size();
       ++i) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(
        verify_port(conn_res->get(),
                    get_cluster_classic_ro_nodes(
                        cs_options.topology.clusters[kFirstReplicaClusterId])));
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
        conn_res->get(),
        get_cluster_classic_ro_nodes(
            cs_options.topology.clusters[kSecondReplicaClusterId])));

    ro_cons_to_second_replica.emplace_back(std::move(*conn_res));
  }
  EXPECT_EQ(2, ro_cons_to_second_replica.size());

  // switch off the mode fetch_whole_topology by reseting routing guidelines
  for (unsigned cluster_id = 0;
       cluster_id < cs_options.topology.clusters.size(); ++cluster_id) {
    auto &cluster = cs_options.topology.clusters[cluster_id];
    for (unsigned node_id = 0; node_id < cluster.nodes.size(); ++node_id) {
      auto &node = cluster.nodes[node_id];

      set_mock_clusterset_metadata(node.http_port, cluster_id, node_id,
                                   cs_options, "");
    }
  }

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
  for ([[maybe_unused]] const auto &node :
       cs_options.topology.clusters[kFirstReplicaClusterId].nodes) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(
        verify_port(conn_res->get(),
                    get_cluster_classic_ro_nodes(
                        cs_options.topology.clusters[kFirstReplicaClusterId])));

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
  const std::string custom_routing_guidelines = guidelines_builder::create(
      {{"d1", "$.server.memberRole = PRIMARY"},
       {"d2",
        "$.server.memberRole = SECONDARY OR $.server.memberRole = REPLICA"}},
      {{"r1",
        "$.session.targetPort =" + std::to_string(router_port_rw),
        {{"round-robin", {"d1"}}}},
       {"r2",
        "$.session.targetPort =" + std::to_string(router_port_ro),
        {{"round-robin", {"d2", "d1"}}}}});

  // switch on the mode fetch_whole_topology by setting the routing guidelines
  for (unsigned cluster_id = 0;
       cluster_id < cs_options.topology.clusters.size(); ++cluster_id) {
    auto &cluster = cs_options.topology.clusters[cluster_id];
    for (unsigned node_id = 0; node_id < cluster.nodes.size(); ++node_id) {
      auto &node = cluster.nodes[node_id];

      set_mock_clusterset_metadata(node.http_port, cluster_id, node_id,
                                   cs_options, custom_routing_guidelines);
    }
  }
  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 3));

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

struct ServerCompatTestParam {
  std::string description;
  std::string server_version;
  bool expect_failure;
  std::string expected_error_msg;
};

class CheckServerCompatibilityTest
    : public ClusterSetTest,
      public ::testing::WithParamInterface<ServerCompatTestParam> {};

/**
 * @test
 *       Verifies that the server version is checked for compatibility when the
 * Router is running with the ClusterSet
 */
TEST_P(CheckServerCompatibilityTest, Spec) {
  RecordProperty("Description", GetParam().description);

  const auto target_cluster_id = 0;

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  create_clusterset(cs_options);

  SCOPED_TRACE("// Launch the Router");
  auto &router =
      launch_router(cs_options.topology, EXIT_SUCCESS, 10s, kTTL, true);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  auto conn_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(conn_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      conn_res->get(), cs_options.topology.clusters[0].nodes[0].classic_port));

  conn_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(conn_res);
  ASSERT_NO_FATAL_FAILURE(verify_port(
      conn_res->get(), cs_options.topology.clusters[0].nodes[1].classic_port));

  for (const auto &cluster : cs_options.topology.clusters) {
    for (const auto &node : cluster.nodes) {
      set_mock_server_version(node.http_port, GetParam().server_version);
    }
  }

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  if (GetParam().expect_failure) {
    verify_new_connection_fails(router_port_rw);
    verify_new_connection_fails(router_port_ro);

    EXPECT_TRUE(wait_log_contains(router, GetParam().expected_error_msg, 5s));
  } else {
    conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(
        verify_port(conn_res->get(),
                    cs_options.topology.clusters[0].nodes[0].classic_port));

    conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    ASSERT_NO_FATAL_FAILURE(
        verify_port(conn_res->get(),
                    cs_options.topology.clusters[0].nodes[2].classic_port));
  }
}

INSTANTIATE_TEST_SUITE_P(
    Spec, CheckServerCompatibilityTest,
    ::testing::Values(
        ServerCompatTestParam{"Server is the same version as Router - OK",
                              std::to_string(MYSQL_ROUTER_VERSION_MAJOR) + "." +
                                  std::to_string(MYSQL_ROUTER_VERSION_MINOR) +
                                  "." +
                                  std::to_string(MYSQL_ROUTER_VERSION_PATCH),
                              false, ""},
        ServerCompatTestParam{
            "Server major version is higher than Router - "
            "we should log a warning",
            std::to_string(MYSQL_ROUTER_VERSION_MAJOR + 1) + "." +
                std::to_string(MYSQL_ROUTER_VERSION_MINOR) + "." +
                std::to_string(MYSQL_ROUTER_VERSION_PATCH),
            false,
            "WARNING .* MySQL Server version .* is higher than the Router "
            "version. You should upgrade the Router to match the MySQL Server "
            "version."},
        ServerCompatTestParam{
            "Server minor version is higher than Router - "
            "we should log a warning",
            std::to_string(MYSQL_ROUTER_VERSION_MAJOR) + "." +
                std::to_string(MYSQL_ROUTER_VERSION_MINOR + 1) + "." +
                std::to_string(MYSQL_ROUTER_VERSION_PATCH),
            false,
            "WARNING .* MySQL Server version .* is higher than the Router "
            "version. You should upgrade the Router to match the MySQL Server "
            "version."},
        ServerCompatTestParam{
            "Server patch version is higher than Router - OK",
            std::to_string(MYSQL_ROUTER_VERSION_MAJOR) + "." +
                std::to_string(MYSQL_ROUTER_VERSION_MINOR) + "." +
                std::to_string(MYSQL_ROUTER_VERSION_PATCH + 1),
            false, ""}));

TEST_F(ClusterSetTest, MatchReplicaNodes) {
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

  const std::string custom_routing_guidelines = guidelines_builder::create(
      {{"d", "$.server.clusterRole = REPLICA"}},
      {{"r",
        "$.session.targetPort = " + std::to_string(router_port_ro),
        {{"round-robin", {"d"}}}}});

  SCOPED_TRACE("Set routing guidelines");
  set_mock_metadata_on_all_cs_nodes(cs_options, custom_routing_guidelines);
  ASSERT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 3));

  SCOPED_TRACE("Set up RO connections to replica nodes");
  // Let's make a bunch of new RO connections, they should go to the RO nodes of
  // replica Clusters of the ClusterSet
  std::vector<std::unique_ptr<MySQLSession>> ro_cons_to_replica;

  const auto replica_nodes1 = get_cluster_classic_ro_nodes(
      cs_options.topology.clusters[kFirstReplicaClusterId]);
  const auto replica_nodes2 = get_cluster_classic_ro_nodes(
      cs_options.topology.clusters[kSecondReplicaClusterId]);
  auto all_replica_nodes = replica_nodes1;
  all_replica_nodes.insert(std::end(all_replica_nodes),
                           std::begin(replica_nodes2),
                           std::end(replica_nodes2));

  for (size_t i = 0; i < all_replica_nodes.size() + 1; ++i) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(all_replica_nodes, ::Contains(*port_res));
    ro_cons_to_replica.push_back(std::move(client_res.value()));
  }
  EXPECT_EQ(ro_cons_to_replica.size(), all_replica_nodes.size() + 1);
}

TEST_F(ClusterSetTest, MatchClusterName) {
  const std::string target_cluster = "primary";
  const auto target_cluster_id = 0;

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options =
      R"({"target_cluster" : ")" + target_cluster + "\" }";
  create_clusterset(cs_options);

  SCOPED_TRACE("// Launch the Router");
  /*auto &router = */ launch_router(cs_options.topology);

  // Policy matching nodes from kSecondReplicaClusterId
  const std::string custom_routing_guidelines = guidelines_builder::create(
      {{"d", "$.server.clusterName = 'cluster-name-3'"}},
      {{"r",
        "$.session.targetPort = " + std::to_string(router_port_ro),
        {{"round-robin", {"d"}}}}});

  SCOPED_TRACE("Set routing guidelines");
  set_mock_metadata_on_all_cs_nodes(cs_options, custom_routing_guidelines);
  ASSERT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 3));

  SCOPED_TRACE("Set up connections to cluster-name-3 cluster");
  // Let's make a bunch of new connections, they should go to the nodes of
  // cluster-name-3 Cluster
  std::vector<std::unique_ptr<MySQLSession>> connections;

  const auto cnt =
      cs_options.topology.clusters[kSecondReplicaClusterId].nodes.size();
  for (size_t i = 0; i < cnt; ++i) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_cluster_classic_ro_nodes(
                    cs_options.topology.clusters[kSecondReplicaClusterId]),
                ::Contains(*port_res));
    connections.push_back(std::move(client_res.value()));
  }

  EXPECT_EQ(cnt, connections.size());
}

TEST_F(ClusterSetTest, MatchClusterSetName) {
  const std::string target_cluster = "00000000-0000-0000-0000-0000000000g2";
  const auto target_cluster_id = 1;

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options =
      R"({"target_cluster" : ")" + target_cluster + "\" }";
  create_clusterset(cs_options);

  SCOPED_TRACE("// Launch the Router");
  auto &router = launch_router(cs_options.topology);

  // Policy matching nodes from kSecondReplicaClusterId
  const std::string custom_routing_guidelines = guidelines_builder::create(
      {{"d", "$.server.clusterSetName = 'clusterset-name'"}},
      {{"r",
        "$.session.targetPort = " + std::to_string(router_port_ro),
        {{"round-robin", {"d"}}}}});

  SCOPED_TRACE("Set routing guidelines");
  set_mock_metadata_on_all_cs_nodes(cs_options, custom_routing_guidelines);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Set up connections matching the whole ClusterSet");
  // Let's make a bunch of new connections, they should go to all of the
  // ClusterSet nodes
  std::vector<std::unique_ptr<MySQLSession>> connections;

  std::vector<std::uint16_t> nodes_ports;
  const auto get_nodes_ports = [&nodes_ports](const auto &nodes) {
    for (const auto &node : nodes) nodes_ports.push_back(node.classic_port);
  };
  get_nodes_ports(cs_options.topology.clusters[kPrimaryClusterId].nodes);
  get_nodes_ports(cs_options.topology.clusters[kFirstReplicaClusterId].nodes);
  get_nodes_ports(cs_options.topology.clusters[kSecondReplicaClusterId].nodes);

  const auto cnt = nodes_ports.size();
  for (size_t i = 0; i < cnt; ++i) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(nodes_ports, ::Contains(*port_res));
    connections.push_back(std::move(client_res.value()));
  }

  EXPECT_EQ(cnt, connections.size());
}

struct InvalidClusterSetTest
    : public ClusterSetTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(InvalidClusterSetTest, MatchInvalidatedCluster) {
  const std::string target_cluster = "00000000-0000-0000-0000-0000000000g2";
  const auto target_cluster_id = 1;

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  const std::string policy = GetParam();
  cs_options.router_options = R"({"target_cluster" : ")" + target_cluster +
                              R"(", "invalidated_cluster_policy" : ")" +
                              policy + "\" }";
  create_clusterset(cs_options);

  SCOPED_TRACE("// Launch the Router");
  /*auto &router = */ launch_router(cs_options.topology);

  SCOPED_TRACE(
      "// Mark our PRIMARY cluster as invalidated in the metadata, also set "
      "the selected invalidatedClusterRoutingPolicy");
  cs_options.topology.clusters[kPrimaryClusterId].invalid = true;

  cs_options.view_id = ++view_id;
  cs_options.target_cluster_id = kPrimaryClusterId;
  cs_options.router_options =
      R"({"target_cluster" : "primary", "invalidated_cluster_policy" : ")" +
      policy + "\" }";

  // Policy matching nodes from kSecondReplicaClusterId
  const std::string custom_routing_guidelines = guidelines_builder::create(
      {{"d", "$.server.isClusterInvalidated = TRUE"}},
      {{"r",
        "$.session.targetPort = " + std::to_string(router_port_ro),
        {{"round-robin", {"d"}}}}});

  SCOPED_TRACE("Set routing guidelines");
  set_mock_metadata_on_all_cs_nodes(cs_options, custom_routing_guidelines);
  ASSERT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 3));

  if (policy == "drop_all") {
    verify_new_connection_fails(router_port_ro);
  } else {
    for (std::size_t i = 0;
         i < cs_options.topology.clusters[kPrimaryClusterId].nodes.size();
         i++) {
      auto client_res = make_new_connection(router_port_ro);
      ASSERT_NO_ERROR(client_res);
      auto port_res = select_port(client_res->get());
      ASSERT_NO_ERROR(port_res);
      EXPECT_THAT(get_cluster_classic_ro_nodes(
                      cs_options.topology.clusters[kPrimaryClusterId]),
                  ::Contains(*port_res));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(MatchInvalidClusterSet, InvalidClusterSetTest,
                         ::testing::Values("drop_all", "accept_ro"),
                         [](const auto &info) { return info.param; });

TEST_F(ClusterSetTest, MatchNodesFromDifferentClusters) {
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

  // Policy matching nodes from kSecondReplicaClusterId
  const std::string custom_routing_guidelines = guidelines_builder::create(
      {{"d", "$.server.clusterName = 'clusterset-1' OR $.server.port = " +
                 std::to_string(
                     cs_options.topology.clusters[0].nodes[0].classic_port)}},
      {{"r",
        "$.session.targetPort = " + std::to_string(router_port_ro),
        {{"round-robin", {"d"}}}}});

  SCOPED_TRACE("Set routing guidelines");
  set_mock_metadata_on_all_cs_nodes(cs_options, custom_routing_guidelines);
  ASSERT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 3));

  SCOPED_TRACE("Set up connections matching the whole ClusterSet");
  // Let's make a bunch of new connections, they should go to all of the
  // ClusterSet nodes
  std::vector<std::unique_ptr<MySQLSession>> connections;

  std::vector<std::uint16_t> nodes_ports = get_cluster_classic_ro_nodes(
      cs_options.topology.clusters[kFirstReplicaClusterId]);
  nodes_ports.push_back(cs_options.topology.clusters[0].nodes[0].classic_port);

  const auto cnt = nodes_ports.size();
  for (size_t i = 0; i < cnt; ++i) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(nodes_ports, ::Contains(*port_res));
    connections.push_back(std::move(client_res.value()));
  }

  EXPECT_EQ(cnt, connections.size());
}

TEST_F(ClusterSetTest, HiddenNodes) {
  const std::string target_cluster = "primary";
  const auto target_cluster_id = 0;

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options =
      R"({"target_cluster" : ")" + target_cluster + "\" }";
  create_clusterset(cs_options);

  SCOPED_TRACE("// Launch the Router");
  /*auto &router = */ launch_router(cs_options.topology);

  // Policy matching nodes from kSecondReplicaClusterId
  const std::string custom_routing_guidelines = guidelines_builder::create(
      {{"d", "$.server.clusterName = 'cluster-name-3'"}},
      {{"r",
        "$.session.targetPort = " + std::to_string(router_port_ro),
        {{"round-robin", {"d"}}}}});

  SCOPED_TRACE("Set routing guidelines");
  set_mock_metadata_on_all_cs_nodes(cs_options, custom_routing_guidelines);
  ASSERT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 3));

  SCOPED_TRACE("Set up connections to cluster-name-3 cluster");
  // Let's make a bunch of new connections, they should go to the nodes of
  // cluster-name-3 Cluster
  std::vector<std::unique_ptr<MySQLSession>> connections;
  std::vector<uint16_t> connection_ports;

  const auto node_cnt =
      cs_options.topology.clusters[kSecondReplicaClusterId].nodes.size();
  for (size_t i = 0; i < node_cnt; ++i) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_cluster_classic_ro_nodes(
                    cs_options.topology.clusters[kSecondReplicaClusterId]),
                ::Contains(*port_res));
    connection_ports.push_back(*port_res);
    connections.push_back(std::move(client_res.value()));
  }

  SCOPED_TRACE("Hide two nodes from cluster-name-3 cluster");
  auto &node1 = cs_options.topology.clusters[kSecondReplicaClusterId].nodes[1];
  auto &node2 = cs_options.topology.clusters[kSecondReplicaClusterId].nodes[2];

  node1.is_hidden = true;
  node1.disconnect_when_hidden = false;
  node2.is_hidden = true;
  node2.disconnect_when_hidden = true;

  set_mock_metadata_on_all_cs_nodes(cs_options, custom_routing_guidelines);
  ASSERT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  SCOPED_TRACE("Verify existing connections");
  for (std::size_t i = 0; i < connections.size(); i++) {
    if (connection_ports[i] == node2.classic_port) {
      verify_existing_connection_dropped(connections[i].get());
    } else {
      verify_existing_connection_ok(connections[i].get());
    }
  }

  SCOPED_TRACE("New connections should go to the visible nodes only");
  std::vector<std::uint16_t> visible_nodes_ports;
  for (const auto &node :
       cs_options.topology.clusters[kSecondReplicaClusterId].nodes) {
    if (!node.is_hidden.has_value() || !node.is_hidden.value()) {
      visible_nodes_ports.push_back(node.classic_port);
    }
  }

  for (size_t i = 0; i < visible_nodes_ports.size(); ++i) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(visible_nodes_ports, ::Contains(*port_res));
  }

  SCOPED_TRACE("Unhide nodes");
  node1.is_hidden = false;
  node2.is_hidden = false;
  set_mock_metadata_on_all_cs_nodes(cs_options, custom_routing_guidelines);
  ASSERT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  for (size_t i = 0; i < node_cnt; ++i) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_cluster_classic_ro_nodes(
                    cs_options.topology.clusters[kSecondReplicaClusterId]),
                ::Contains(*port_res));
  }
}

/**
 * @test Checks that warning about invalid value of "stats_updates_frequency"
 * in the metadata is only logged once by the Router
 */
TEST_F(ClusterSetTest, InvalidStatsUpdateFrequency) {
  const int target_cluster_id = 0;

  std::string inv = "\"\"";
  std::string router_options =
      R"({"target_cluster" : "00000000-0000-0000-0000-0000000000g1",
          "stats_updates_frequency": null} )";

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = router_options;
  create_clusterset(cs_options);

  SCOPED_TRACE("// Launch the Router");
  auto &router = launch_router(cs_options.topology);

  // wait for a few md refresh rounds
  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 3));

  const std::string warning =
      "Error parsing stats_updates_frequency from the "
      "router_options: options.stats_updates_frequency='null'; not an unsigned "
      "int. Using default value";

  // the warning should be logged only once
  check_log_contains(router, warning, 1);
}

/**
 * @test Checks that the Router still is operating as expected if the first node
 * on the metadata servers list is lagging to replicate the metadata from the
 * primary after the boostrap. (BUG#38132603)
 */
TEST_F(ClusterSetTest, FirstMdServerLagsReplicatingMetadata) {
  const std::string target_cluster = "primary";
  const auto target_cluster_id = 0;

  /* create a clusterset where the first node (also a first metadata server) is
   * a SECONDARY node */
  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options =
      R"({"target_cluster" : ")" + target_cluster + "\" }";
  cs_options.primary_node_id = 1;
  create_clusterset(cs_options);

  /* instrument the first metadata server to return no rows when queried for
   * options from the `routers` table. This emulates a scenario where this
   * node is lagging in replicating the metadata from the primary node where the
   * Router was bootstrapped. */
  cs_options.simulate_router_options_no_rows = true;
  set_mock_clusterset_metadata(
      cs_options.topology.clusters[0].nodes[0].http_port,
      /*this_cluster_id*/ 0,
      /*this_node_id*/ 0, cs_options);

  SCOPED_TRACE("// Launch the Router");
  auto &router = launch_router(cs_options.topology);

  const std::string warning =
      "WARNING .* Failed determining the type of the cluster: No row in "
      "v2_routers table "
      "for router with id 1";

  EXPECT_TRUE(wait_log_contains(router, warning, 5s));

  auto conn_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(conn_res);

  conn_res = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(conn_res);
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ProcessManager::set_origin(g_origin_path);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
