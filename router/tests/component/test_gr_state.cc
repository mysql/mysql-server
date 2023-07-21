/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "my_config.h"

#include "config_builder.h"
#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql/harness/stdx/ranges.h"  // enumerate
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "rest_api_testutils.h"
#include "router_component_metadata.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_config.h"
#include "router_test_helpers.h"
#include "socket_operations.h"
#include "tcp_port_pool.h"

using mysqlrouter::ClusterType;
using mysqlrouter::MetadataSchemaVersion;
using mysqlrouter::MySQLSession;
using ::testing::PrintToString;
using namespace std::chrono_literals;
using namespace std::string_literals;

class GRStateTest : public RouterComponentMetadataTest {};

struct GRStateTestParams {
  // mock_server trace file
  std::string tracefile;
  // additional info about the testcase that gets printed by the gtest in the
  // results
  std::string description;
  // the type of the cluster GR V1 or GR V2
  ClusterType cluster_type;

  GRStateTestParams(std::string tracefile_, std::string description_,
                    ClusterType cluster_type_)
      : tracefile(std::move(tracefile_)),
        description(std::move(description_)),
        cluster_type(cluster_type_) {}
};

auto get_test_description(
    const ::testing::TestParamInfo<GRStateTestParams> &info) {
  return info.param.description;
}

class MetadataServerInvalidGRState
    : public GRStateTest,
      public ::testing::WithParamInterface<GRStateTestParams> {};

TEST_P(MetadataServerInvalidGRState, InvalidGRState) {
  const size_t kClusterNodes{3};
  std::vector<ProcessWrapper *> cluster_nodes;
  std::vector<uint16_t> md_servers_classic_ports, md_servers_http_ports;

  // launch the server mocks
  for (size_t i = 0; i < kClusterNodes; ++i) {
    const auto classic_port = port_pool_.get_next_available();
    const auto http_port = port_pool_.get_next_available();
    const std::string tracefile =
        get_data_dir().join(GetParam().tracefile).str();
    cluster_nodes.push_back(&launch_mysql_server_mock(
        tracefile, classic_port, EXIT_SUCCESS, false, http_port));

    md_servers_classic_ports.push_back(classic_port);
    md_servers_http_ports.push_back(http_port);
  }

  for (const auto [i, http_port] :
       stdx::views::enumerate(md_servers_http_ports)) {
    ::set_mock_metadata(
        http_port, "uuid", classic_ports_to_gr_nodes(md_servers_classic_ports),
        i, classic_ports_to_cluster_nodes(md_servers_classic_ports),
        /*primary_id=*/0);
  }

  // launch the router with metadata-cache configuration
  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type, "0.1");
  const auto router_rw_port = port_pool_.get_next_available();
  const std::string routing_rw_section = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "first-available", "", "rw");
  const auto router_ro_port = port_pool_.get_next_available();
  const std::string routing_ro_section = get_metadata_cache_routing_section(
      router_ro_port, "SECONDARY", "round-robin", "", "ro");
  auto &router = launch_router(metadata_cache_section,
                               routing_rw_section + routing_ro_section,
                               md_servers_classic_ports, EXIT_SUCCESS,
                               /*wait_for_notify_ready=*/30s);

  // check first metadata server (PRIMARY) is queried for metadata
  EXPECT_TRUE(
      wait_for_transaction_count_increase(md_servers_http_ports[0], 2, 5s));

  // check that 2nd and 3rd servers (SECONDARIES) are NOT queried for metadata
  for (const auto i : {1, 2}) {
    EXPECT_FALSE(wait_for_transaction_count_increase(md_servers_http_ports[i],
                                                     1, 200ms));
  }

  // now promote first SECONDARY to become new PRIMARY
  // make the old PRIMARY offline (static metadata does not change)
  for (const auto [i, http_port] :
       stdx::views::enumerate(md_servers_http_ports)) {
    if (i == 0) {
      // old PRIMARY sees itself as OFFLINE, does not see other nodes
      const auto gr_nodes = std::vector<GRNode>{
          {md_servers_classic_ports[0], "uuid-1", "OFFLINE"}};
      ::set_mock_metadata(
          http_port, "uuid", gr_nodes, 0,
          classic_ports_to_cluster_nodes(md_servers_classic_ports),
          /*primary_id=*/0);
    } else {
      // remaining nodes see the previous SECONDARY-1 as new primary
      // they do not see old PRIMARY (it was expelled from the group)
      const auto gr_nodes = std::vector<GRNode>{
          {{md_servers_classic_ports[1], "uuid-2", "ONLINE"},
           {md_servers_classic_ports[2], "uuid-3", "ONLINE"}}};
      ::set_mock_metadata(
          http_port, "uuid", gr_nodes, i - 1,
          classic_ports_to_cluster_nodes(md_servers_classic_ports),
          /*primary_id=*/0);
    }
  }

  // check that the second metadata server (new PRIMARY) is queried for metadata
  EXPECT_TRUE(
      wait_for_transaction_count_increase(md_servers_http_ports[1], 2, 5s));

  // check that Router refused to use metadata from former PRIMARY (only once,
  // then should stop using it)
  check_log_contains(router,
                     "Metadata server 127.0.0.1:" +
                         std::to_string(md_servers_classic_ports[0]) +
                         " is not an online GR member - skipping.",
                     1);

  // new connections are now handled by new primary and the secon secondary
  make_new_connection_ok(router_rw_port, md_servers_classic_ports[1]);
  make_new_connection_ok(router_ro_port, md_servers_classic_ports[2]);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidGRState, MetadataServerInvalidGRState,
    ::testing::Values(GRStateTestParams("metadata_dynamic_nodes_v2_gr.js",
                                        "GR_V2", ClusterType::GR_V2),
                      GRStateTestParams("metadata_dynamic_nodes.js", "GR_V1",
                                        ClusterType::GR_V1)),
    get_test_description);

class MetadataServerNoQuorum
    : public GRStateTest,
      public ::testing::WithParamInterface<GRStateTestParams> {};

TEST_P(MetadataServerNoQuorum, NoQuorum) {
  const size_t kClusterNodes{3};
  std::vector<ProcessWrapper *> cluster_nodes;
  std::vector<uint16_t> md_servers_classic_ports, md_servers_http_ports;

  // launch the server mocks
  for (size_t i = 0; i < kClusterNodes; ++i) {
    const auto classic_port = port_pool_.get_next_available();
    const auto http_port = port_pool_.get_next_available();
    const std::string tracefile =
        get_data_dir().join(GetParam().tracefile).str();
    cluster_nodes.push_back(&launch_mysql_server_mock(
        tracefile, classic_port, EXIT_SUCCESS, false, http_port));

    md_servers_classic_ports.push_back(classic_port);
    md_servers_http_ports.push_back(http_port);
  }

  for (const auto [i, http_port] :
       stdx::views::enumerate(md_servers_http_ports)) {
    ::set_mock_metadata(
        http_port, "uuid", classic_ports_to_gr_nodes(md_servers_classic_ports),
        i, classic_ports_to_cluster_nodes(md_servers_classic_ports),
        /*primary_id=*/0);
  }

  // launch the router with metadata-cache configuration
  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type, "0.1");
  const auto router_rw_port = port_pool_.get_next_available();
  const std::string routing_rw_section = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "first-available", "", "rw");
  const auto router_ro_port = port_pool_.get_next_available();
  const std::string routing_ro_section = get_metadata_cache_routing_section(
      router_ro_port, "SECONDARY", "round-robin", "", "ro");
  auto &router = launch_router(metadata_cache_section,
                               routing_rw_section + routing_ro_section,
                               md_servers_classic_ports, EXIT_SUCCESS,
                               /*wait_for_notify_ready=*/30s);

  // check first metadata server (PRIMARY) is queried for metadata
  EXPECT_TRUE(
      wait_for_transaction_count_increase(md_servers_http_ports[0], 2, 5s));

  // check that 2nd and 3rd servers (SECONDARIES) are NOT queried for metadata
  for (const auto i : {1, 2}) {
    EXPECT_FALSE(wait_for_transaction_count_increase(md_servers_http_ports[i],
                                                     1, 200ms));
  }

  // now promote first SECONDARY to become new PRIMARY
  // make the old PRIMARY see other as OFFLINE and claim it is ONLINE
  // (static metadata does not change)
  for (const auto [i, http_port] :
       stdx::views::enumerate(md_servers_http_ports)) {
    if (i == 0) {
      // old PRIMARY still sees itself as ONLINE, but it lost quorum, do not
      // see other GR members
      const auto gr_nodes = std::vector<GRNode>{
          {md_servers_classic_ports[0], "uuid-1", "ONLINE"},
          {md_servers_classic_ports[1], "uuid-2", "OFFLINE"},
          {md_servers_classic_ports[2], "uuid-3", "OFFLINE"}};
      ::set_mock_metadata(
          http_port, "uuid", gr_nodes, 0,
          classic_ports_to_cluster_nodes(md_servers_classic_ports),
          /*primary_id=*/0);
    } else {
      // remaining nodes see the previous SECONDARY-1 as new primary
      // they do not see old PRIMARY (it was expelled from the group)
      const auto gr_nodes = std::vector<GRNode>{
          {{md_servers_classic_ports[1], "uuid-2", "ONLINE"},
           {md_servers_classic_ports[2], "uuid-3", "ONLINE"}}};
      ::set_mock_metadata(
          http_port, "uuid", gr_nodes, i - 1,
          classic_ports_to_cluster_nodes(md_servers_classic_ports),
          /*primary_id=*/0);
    }
  }

  // check that the second metadata server (new PRIMARY) is queried for metadata
  EXPECT_TRUE(
      wait_for_transaction_count_increase(md_servers_http_ports[1], 2, 5s));

  // check that Router refused to use metadata from former PRIMARY (only once,
  // then should stop using it)
  check_log_contains(router,
                     "Metadata server 127.0.0.1:" +
                         std::to_string(md_servers_classic_ports[0]) +
                         " is not a member of quorum group - skipping.",
                     1);

  // new connections are now handled by new primary and the secon secondary
  make_new_connection_ok(router_rw_port, md_servers_classic_ports[1]);
  make_new_connection_ok(router_ro_port, md_servers_classic_ports[2]);
}

INSTANTIATE_TEST_SUITE_P(
    NoQuorum, MetadataServerNoQuorum,
    ::testing::Values(GRStateTestParams("metadata_dynamic_nodes_v2_gr.js",
                                        "GR_V2", ClusterType::GR_V2),
                      GRStateTestParams("metadata_dynamic_nodes.js", "GR_V1",
                                        ClusterType::GR_V1)),
    get_test_description);

class MetadataServerGRErrorStates
    : public GRStateTest,
      public ::testing::WithParamInterface<std::string> {};

/**
 * @test Checks that the Router correctly handles non-ONLINE GR nodes
 */
TEST_P(MetadataServerGRErrorStates, GRErrorStates) {
  const std::string tracefile =
      get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str();

  // launch the server mock
  const auto md_servers_classic_port = port_pool_.get_next_available();
  const auto md_servers_http_port = port_pool_.get_next_available();
  launch_mysql_server_mock(tracefile, md_servers_classic_port, EXIT_SUCCESS,
                           false, md_servers_http_port);

  std::vector<GRNode> gr_nodes{{md_servers_classic_port, "uuid-1", GetParam()}};
  ::set_mock_metadata(md_servers_http_port, "uuid", gr_nodes, 0,
                      classic_ports_to_cluster_nodes({md_servers_classic_port}),
                      /*primary_id=*/0);

  // launch the router with metadata-cache configuration
  const std::string metadata_cache_section =
      get_metadata_cache_section(ClusterType::GR_V2, "0.1");
  const auto router_rw_port = port_pool_.get_next_available();
  const std::string routing_rw_section = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "first-available", "", "rw");
  auto &router = launch_router(metadata_cache_section, routing_rw_section,
                               {md_servers_classic_port}, EXIT_SUCCESS,
                               /*wait_for_notify_ready=*/-1s);

  EXPECT_TRUE(wait_for_transaction_count_increase(md_servers_http_port, 2, 5s));

  const std::string expected_string =
      "Metadata server 127.0.0.1:" + std::to_string(md_servers_classic_port) +
      " is not an online GR member - skipping.";

  const std::string log_content = router.get_logfile_content();
  EXPECT_GE(count_str_occurences(log_content, expected_string), 1)
      << log_content;
}

INSTANTIATE_TEST_SUITE_P(GRErrorStates, MetadataServerGRErrorStates,
                         ::testing::Values("OFFLINE", "UNREACHABLE",
                                           "RECOVERING", "ERROR", "UNKNOWN", "",
                                           ".."));

class MetadataCacheChangeClusterName
    : public GRStateTest,
      public ::testing::WithParamInterface<GRStateTestParams> {};

struct QuorumTestParam {
  std::string test_name;

  std::vector<GRNode> gr_nodes;
  std::vector<ClusterNode> cluster_nodes;

  std::vector<uint16_t> expected_rw_endpoints;
  std::vector<uint16_t> expected_ro_endpoints;
};

class QuorumTest : public GRStateTest,
                   public ::testing::WithParamInterface<QuorumTestParam> {
 public:
  void SetUp() override {
    GRStateTest::SetUp();
    for (size_t i = 0; i < 3; ++i) {
      classic_ports.push_back(port_pool_.get_next_available());
      http_ports.push_back(port_pool_.get_next_available());
    }
  }

 protected:
  std::vector<uint16_t> classic_ports, http_ports;
};

/**
 * @test Testing various quorum scenarios.
 */
TEST_P(QuorumTest, Verify) {
  const std::string json_metadata =
      get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str();

  auto param = GetParam();
  std::vector<uint16_t> cluster_classic_ports;
  // The ports set via INSTANTIATE_TEST_SUITE_P are only ids
  // (INSTANTIATE_TEST_SUITE_P does not have access to classic_ports vector). We
  // need to fill them up here.
  const auto primary_http_port =
      http_ports[param.cluster_nodes[0].classic_port];
  for (auto &node : param.gr_nodes) {
    node.classic_port = classic_ports[node.classic_port];
  }
  for (auto &node : param.cluster_nodes) {
    node.classic_port = classic_ports[node.classic_port];
    cluster_classic_ports.push_back(node.classic_port);
  }
  for (auto &port : param.expected_rw_endpoints) {
    port = classic_ports[port];
  }
  for (auto &port : param.expected_ro_endpoints) {
    port = classic_ports[port];
  }

  const bool expect_rw_ok = !param.expected_rw_endpoints.empty();
  const bool expect_ro_ok = !param.expected_rw_endpoints.empty();

  for (const auto [id, port] : stdx::views::enumerate(classic_ports)) {
    launch_mysql_server_mock(json_metadata, port, EXIT_SUCCESS, false,
                             http_ports[id]);
    ::set_mock_metadata(http_ports[id], "uuid", param.gr_nodes, 0,
                        param.cluster_nodes);
  }

  const auto router_ro_port = port_pool_.get_next_available();
  const auto router_rw_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(ClusterType::GR_V2, "0.2");
  const std::string routing_rw = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "first-available", "", "rw");
  const std::string routing_ro = get_metadata_cache_routing_section(
      router_ro_port, "SECONDARY", "round-robin-with-fallback", "", "ro");

  const auto wait_ready = (expect_rw_ok || expect_ro_ok) ? 10s : -1s;

  /*auto &router = */ launch_router(
      metadata_cache_section, routing_rw + routing_ro, cluster_classic_ports,
      EXIT_SUCCESS, wait_ready);

  if (wait_ready == -1s) {
    EXPECT_TRUE(wait_for_transaction_count_increase(primary_http_port, 2));
  }

  for (int i = 0; i < 2; i++) {
    if (expect_rw_ok) {
      make_new_connection_ok(router_rw_port, param.expected_rw_endpoints);
    } else {
      verify_new_connection_fails(router_rw_port);
    }

    if (expect_ro_ok) {
      make_new_connection_ok(router_ro_port, param.expected_ro_endpoints);
    } else {
      verify_new_connection_fails(router_ro_port);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    Spec, QuorumTest,
    ::testing::Values(
        // 2 nodes: 1 ONLINE, 1 OFFLINE = no quorum, no connections handled
        QuorumTestParam{
            "1_online_1_offline",
            /*gr_nodes*/
            {{0, "uuid-1", "ONLINE"}, {1, "uuid-2", "OFFLINE"}},
            /*cluster_nodes*/
            {{0, "uuid-1"}, {1, "uuid-2"}},
            /*expected_rw_endpoints*/
            {},
            /*expected_ro_endpoints*/
            {},
        },
        // 2 nodes: 1 ONLINE, 1 RECOVERING = quorum, connections handled
        QuorumTestParam{
            "1_online_1_recovering",
            /*gr_nodes*/
            {{0, "uuid-1", "ONLINE"}, {1, "uuid-2", "RECOVERING"}},
            /*cluster_nodes*/
            {{0, "uuid-1"}, {1, "uuid-2"}},
            /*expected_rw_endpoints*/
            {0},
            /*expected_ro_endpoints*/
            {0},
        },
        // 3 nodes: 1 ONLINE, 2 RECOVERING = quorum, connections handled
        QuorumTestParam{
            "1_online_2_recovering",
            /*gr_nodes*/
            {{0, "uuid-1", "ONLINE"},
             {1, "uuid-2", "RECOVERING"},
             {2, "uuid-3", "RECOVERING"}},
            /*cluster_nodes*/
            {{0, "uuid-1"}, {1, "uuid-2"}, {2, "uuid-3"}},
            /*expected_rw_endpoints*/
            {0},
            /*expected_ro_endpoints*/
            {0},
        },
        // There are 2 nodes in GR, only one of them is defined in the metadata.
        // The RW and RO connections should still be possible and should be only
        // reaching the node that is present in both GR and cluster metadata.
        QuorumTestParam{
            "2_online_1_missing_in_metadata",
            /*gr_nodes*/
            {{0, "uuid-1", "ONLINE"}, {1, "uuid-2", "ONLINE"}},
            /*cluster_nodes*/
            {{0, "uuid-1"}},
            /*expected_rw_endpoints*/
            {0},
            /*expected_ro_endpoints*/
            {0},
        },
        // There are 2 nodes in GR, one node in the cluster metadata.
        // The one in the cluster metadata is not present in the GR, no
        // connections should be possible.
        QuorumTestParam{
            "2_online_both_missing_in_metadata",
            /*gr_nodes*/
            {{0, "uuid-1", "ONLINE"}, {1, "uuid-2", "ONLINE"}},
            /*cluster_nodes*/
            {{2, "uuid-3"}},
            /*expected_rw_endpoints*/
            {},
            /*expected_ro_endpoints*/
            {},
        }),
    [](const ::testing::TestParamInfo<QuorumTestParam> &info) {
      return info.param.test_name;
    });

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
