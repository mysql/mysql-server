/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
#include "router_component_clusterset.h"
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

class GRStateTest : public RouterComponentMetadataTest {
 protected:
  std::string get_router_options_as_json_str(
      const std::optional<std::string> &target_cluster,
      const std::optional<std::string> &unreachable_quorum_allowed_traffic) {
    JsonValue json_val(rapidjson::kObjectType);
    JsonAllocator allocator;

    if (target_cluster) {
      json_val.AddMember("target_cluster",
                         JsonValue((*target_cluster).c_str(),
                                   (*target_cluster).length(), allocator),
                         allocator);
    }

    if (unreachable_quorum_allowed_traffic) {
      json_val.AddMember(
          "unreachable_quorum_allowed_traffic",
          JsonValue((*unreachable_quorum_allowed_traffic).c_str(),
                    (*unreachable_quorum_allowed_traffic).length(), allocator),
          allocator);
    }

    return json_to_string(json_val);
  }

  std::string get_rw_split_routing_section(uint16_t accepting_port) {
    return get_metadata_cache_routing_section(
        accepting_port, "PRIMARY_AND_SECONDARY", "round-robin", "rwsplit",
        "classic", {{"connection_sharing", "1"}, {"access_mode", "auto"}});
  }
};

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
    set_mock_metadata(http_port, "uuid",
                      classic_ports_to_gr_nodes(md_servers_classic_ports), i,
                      classic_ports_to_cluster_nodes(md_servers_classic_ports));
  }

  // launch the router with metadata-cache configuration
  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type, "0.1");
  const auto router_rw_port = port_pool_.get_next_available();
  const std::string routing_rw_section = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "first-available", "rw");
  const auto router_ro_port = port_pool_.get_next_available();
  const std::string routing_ro_section = get_metadata_cache_routing_section(
      router_ro_port, "SECONDARY", "round-robin", "ro");
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
          {md_servers_classic_ports[0], "uuid-1", "OFFLINE", "PRIMARY"}};
      set_mock_metadata(
          http_port, "uuid", gr_nodes, 0,
          classic_ports_to_cluster_nodes(md_servers_classic_ports));
    } else {
      // remaining nodes see the previous SECONDARY-1 as new primary
      // they do not see old PRIMARY (it was expelled from the group)
      const auto gr_nodes = std::vector<GRNode>{
          {{md_servers_classic_ports[1], "uuid-2", "ONLINE", "PRIMARY"},
           {md_servers_classic_ports[2], "uuid-3", "ONLINE", "SECONDARY"}}};
      set_mock_metadata(
          http_port, "uuid", gr_nodes, i - 1,
          classic_ports_to_cluster_nodes(md_servers_classic_ports));
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
                                        "GR_V2", ClusterType::GR_V2)),
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
    set_mock_metadata(http_port, "uuid",
                      classic_ports_to_gr_nodes(md_servers_classic_ports), i,
                      classic_ports_to_cluster_nodes(md_servers_classic_ports));
  }

  // launch the router with metadata-cache configuration
  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type, "0.1");
  const auto router_rw_port = port_pool_.get_next_available();
  const std::string routing_rw_section = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "first-available", "rw");
  const auto router_ro_port = port_pool_.get_next_available();
  const std::string routing_ro_section = get_metadata_cache_routing_section(
      router_ro_port, "SECONDARY", "round-robin", "ro");
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
          {md_servers_classic_ports[0], "uuid-1", "ONLINE", "PRIMARY"},
          {md_servers_classic_ports[1], "uuid-2", "UNREACHABLE", "SECONDARY"},
          {md_servers_classic_ports[2], "uuid-3", "UNREACHABLE", "SECONDARY"}};
      set_mock_metadata(
          http_port, "uuid", gr_nodes, 0,
          classic_ports_to_cluster_nodes(md_servers_classic_ports));
    } else {
      // remaining nodes see the previous SECONDARY-1 as new primary
      // they do not see old PRIMARY (it was expelled from the group)
      const auto gr_nodes = std::vector<GRNode>{
          {{md_servers_classic_ports[1], "uuid-2", "ONLINE", "PRIMARY"},
           {md_servers_classic_ports[2], "uuid-3", "ONLINE", "SECONDARY"}}};
      set_mock_metadata(
          http_port, "uuid", gr_nodes, i - 1,
          classic_ports_to_cluster_nodes(md_servers_classic_ports));
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
                                        "GR_V2", ClusterType::GR_V2)),
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

  std::vector<GRNode> gr_nodes{
      {md_servers_classic_port, "uuid-1", GetParam(), "PRIMARY"}};
  set_mock_metadata(md_servers_http_port, "uuid", gr_nodes, 0,
                    classic_ports_to_cluster_nodes({md_servers_classic_port}));

  // launch the router with metadata-cache configuration
  const std::string metadata_cache_section =
      get_metadata_cache_section(ClusterType::GR_V2, "0.1");
  const auto router_rw_port = port_pool_.get_next_available();
  const std::string routing_rw_section = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "first-available", "rw");
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

  std::string tracefile{"metadata_dynamic_nodes_v2_gr.js"};
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
  auto param = GetParam();
  const std::string json_metadata = get_data_dir().join(param.tracefile).str();
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
    set_mock_metadata(http_ports[id], "uuid", param.gr_nodes, 0,
                      param.cluster_nodes);
  }

  const auto router_ro_port = port_pool_.get_next_available();
  const auto router_rw_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(ClusterType::GR_V2, "0.2");
  const std::string routing_rw = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "first-available", "rw");
  const std::string routing_ro = get_metadata_cache_routing_section(
      router_ro_port, "SECONDARY", "round-robin-with-fallback", "ro");

  const auto sync_point = (expect_rw_ok || expect_ro_ok)
                              ? ProcessManager::Spawner::SyncPoint::READY
                              : ProcessManager::Spawner::SyncPoint::RUNNING;

  const std::string conf_file = setup_router_config(
      metadata_cache_section, routing_rw + routing_ro, cluster_classic_ports);

  router_spawner()
      .expected_exit_code(EXIT_SUCCESS)
      .wait_for_sync_point(sync_point)
      .spawn({"-c", conf_file});

  if (sync_point == ProcessManager::Spawner::SyncPoint::RUNNING) {
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
            {{0, "uuid-1", "ONLINE", "PRIMARY"},
             {1, "uuid-2", "OFFLINE", "SECONDARY"}},
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
            {{0, "uuid-1", "ONLINE", "PRIMARY"},
             {1, "uuid-2", "RECOVERING", "SECONDARY"}},
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
            {{0, "uuid-1", "ONLINE", "PRIMARY"},
             {1, "uuid-2", "RECOVERING", "SECONDARY"},
             {2, "uuid-3", "RECOVERING", "SECONDARY"}},
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
            {{0, "uuid-1", "ONLINE", "PRIMARY"},
             {1, "uuid-2", "ONLINE", "SECONDARY"}},
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
            {{0, "uuid-1", "ONLINE", "PRIMARY"},
             {1, "uuid-2", "ONLINE", "SECONDARY"}},
            /*cluster_nodes*/
            {{2, "uuid-3"}},
            /*expected_rw_endpoints*/
            {},
            /*expected_ro_endpoints*/
            {},
        },
        // Check the same thing but with server version 5.7 (expects different
        // query checking the GR status)
        QuorumTestParam{"2_online_both_missing_in_metadata_5_7",
                        /*gr_nodes*/
                        {{0, "uuid-1", "ONLINE", "PRIMARY"},
                         {1, "uuid-2", "ONLINE", "SECONDARY"}},
                        /*cluster_nodes*/
                        {{2, "uuid-3"}},
                        /*expected_rw_endpoints*/
                        {},
                        /*expected_ro_endpoints*/
                        {},
                        /*tracefile*/
                        "metadata_dynamic_nodes_v2_gr_5_7.js"}),
    [](const ::testing::TestParamInfo<QuorumTestParam> &info) {
      return info.param.test_name;
    });

class QuorumConnectionLostStandaloneClusterTest : public GRStateTest {
 public:
  void SetUp() override {
    GRStateTest::SetUp();
    for (size_t i = 0; i < kMaxNodes; ++i) {
      classic_ports.push_back(port_pool_.get_next_available());
      http_ports.push_back(port_pool_.get_next_available());
    }
  }

 protected:
  static constexpr size_t kMaxNodes{3};
  std::vector<uint16_t> classic_ports, http_ports;
};

/* @test Checks that invalid existing connections are dropped when one of the
 * destination nodes is no longer part of the Cluster. */
TEST_F(QuorumConnectionLostStandaloneClusterTest, CheckInvalidConDropped) {
  const std::string json_metadata =
      get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str();

  // launch the 3-nodes cluster, first node is PRIMARY
  for (size_t i = 0; i < classic_ports.size(); ++i) {
    launch_mysql_server_mock(json_metadata, classic_ports[i], EXIT_SUCCESS,
                             false, http_ports[i]);

    set_mock_metadata(http_ports[i], "uuid",
                      classic_ports_to_gr_nodes(classic_ports), i,
                      classic_ports_to_cluster_nodes(classic_ports));
  }

  // start the Router
  const auto router_ro_port = port_pool_.get_next_available();
  const auto router_rw_port = port_pool_.get_next_available();
  const auto router_rw_split_port = port_pool_.get_next_available();

  std::vector<uint16_t> metadata_server_ports{
      classic_ports[0], classic_ports[1], classic_ports[2]};

  auto writer = config_writer(get_test_temp_dir_name());
  writer
      .section("connection_pool",
               {
                   {"max_idle_server_connections", "16"},
               })
      .section("metadata_cache",
               {
                   {"cluster_type", "gr"},
                   {"router_id", "1"},
                   {"user", router_metadata_username},
                   {"connect_timeout", "1"},
                   {"metadata_cluster", "test"},
                   {"ttl", "0.2"},  // for faster test-runs
               })
      .section(
          "routing:rw",
          {
              {"bind_port", std::to_string(router_rw_port)},
              {"destinations", "metadata-cache://test/default?role=PRIMARY"},
              {"protocol", "classic"},
              {"routing_strategy", "first-available"},
              {"client_ssl_mode", "DISABLED"},
              {"server_ssl_mode", "PREFERRED"},
          })
      .section(
          "routing:ro",
          {
              {"bind_port", std::to_string(router_ro_port)},
              {"destinations", "metadata-cache://test/default?role=SECONDARY"},
              {"protocol", "classic"},
              {"routing_strategy", "round-robin-with-fallback"},
              {"client_ssl_mode", "DISABLED"},
              {"server_ssl_mode", "PREFERRED"},
          })
      .section("routing:rwsplit",
               {
                   {"bind_port", std::to_string(router_rw_split_port)},
                   {"destinations",
                    "metadata-cache://test/default?role=PRIMARY_AND_SECONDARY"},
                   {"protocol", "classic"},
                   {"routing_strategy", "round-robin"},
                   {"client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert.pem"},
                   {"client_ssl_key", SSL_TEST_DATA_DIR "/server-key.pem"},
                   {"client_ssl_mode", "PREFERRED"},
                   {"server_ssl_mode", "PREFERRED"},
                   {"access_mode", "auto"},
                   {"connection_sharing", "1"},
               });
  auto &default_section = writer.sections()["DEFAULT"];

  state_file_ = create_state_file(
      get_test_temp_dir_name(),
      create_state_file_content("uuid", "", metadata_server_ports, 0));
  init_keyring(default_section, get_test_temp_dir_name());
  default_section["dynamic_state"] = state_file_;

  /*auto &router = */ router_spawner().spawn({"-c", writer.write()});

  // make the classic connections to each classic port
  MySQLSession con_rw;
  ASSERT_NO_THROW(con_rw.connect("127.0.0.1", router_rw_port, "username",
                                 "password", "", ""));
  {
    auto port_res = con_rw.query_one("select @@port");
    ASSERT_TRUE(port_res);
    ASSERT_THAT(*port_res, testing::SizeIs(1));
    EXPECT_EQ((*port_res)[0], std::to_string(classic_ports[0]));
  }

  MySQLSession con_ro;
  ASSERT_NO_THROW(con_ro.connect("127.0.0.1", router_ro_port, "username",
                                 "password", "", ""));
  {
    auto port_res = con_ro.query_one("select @@port");
    ASSERT_TRUE(port_res);
    ASSERT_THAT(*port_res, testing::SizeIs(1));
    EXPECT_EQ((*port_res)[0], std::to_string(classic_ports[1]));
  }

  MySQLSession con_rw_split;
  ASSERT_NO_THROW(con_rw_split.connect("127.0.0.1", router_rw_split_port,
                                       "username", "password", "", ""));
  {
    auto port_res = con_rw_split.query_one("select @@port");
    ASSERT_TRUE(port_res);
    ASSERT_THAT(*port_res, testing::SizeIs(1));
    EXPECT_EQ((*port_res)[0], std::to_string(classic_ports[1]));
  }

  // simulate removing the PRIMARY from the cluster (c.removeInstance(primary))
  std::vector<::ClusterNode> cluster_nodes{{classic_ports[1], "uuid-2"},
                                           {classic_ports[2], "uuid-3"}};

  // removed node sees itself as OFFLINE
  std::vector<GRNode> gr_nodes_partition1{
      {classic_ports[0], "uuid-1", "OFFLINE", "PRIMARY"}};

  set_mock_metadata(http_ports[0], "uuid", gr_nodes_partition1, 0,
                    cluster_nodes);

  // the 2 remaining ones do not see the one that was removed in GR status
  std::vector<GRNode> gr_nodes_partition2{
      {classic_ports[1], "uuid-2", "ONLINE", "PRIMARY"},
      {classic_ports[2], "uuid-3", "ONLINE", "SECONDARY"}};

  for (size_t i = 0; i < cluster_nodes.size(); ++i) {
    set_mock_metadata(http_ports[i + 1], "uuid", gr_nodes_partition2, i,
                      cluster_nodes);
  }

  // wait for the Router to notice the change in the Cluster
  ASSERT_TRUE(wait_for_transaction_count_increase(http_ports[1], 2));

  // check that read-write & read-write-split connection got dropped
  verify_existing_connection_dropped(&con_rw);
  verify_existing_connection_dropped(&con_rw_split);
  // read-only should be fine
  verify_existing_connection_ok(&con_ro);

  // check that the new rw and rw-split connections don't go to the node that is
  // gone
  make_new_connection_ok(router_rw_port, classic_ports[1]);
  make_new_connection_ok(router_rw_split_port, classic_ports[2]);
}

struct AccessToPartitionWithNoQuorumTestParam {
  std::string test_name;
  std::string test_requirements;
  std::string test_description;

  std::optional<std::string> unreachable_quorum_allowed_traffic;

  bool expect_rw_connection_ok;
  bool expect_ro_connection_ok;
  bool expect_rw_split_connection_ok;
};

class AccessToPartitionWithNoQuorum
    : public QuorumConnectionLostStandaloneClusterTest,
      public ::testing::WithParamInterface<
          AccessToPartitionWithNoQuorumTestParam> {};

// unreachable_quorum_allowed_traffic
TEST_P(AccessToPartitionWithNoQuorum, Spec) {
  const std::string json_metadata =
      get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str();

  RecordProperty("Worklog", "15841");
  RecordProperty("RequirementId", GetParam().test_requirements);
  RecordProperty("Description", GetParam().test_description);

  // The GR is split into 2 partitions, the Router only has access to the one
  // with no quorum.
  // First partition is: [ONLINE, ONLINE, UNREACHABLE].
  // The second partition is: [UNREACHABLE, UNREACHABLE, ONLINE ].
  // We only create the second partition as the Router only has accsess only
  // to this one anyway

  std::vector<GRNode> gr_nodes{
      {classic_ports[0], "uuid-1", "UNREACHABLE", "SECONDARY"},
      {classic_ports[1], "uuid-2", "UNREACHABLE", "SECONDARY"},
      {classic_ports[2], "uuid-3", "ONLINE", "PRIMARY"}};
  std::vector<::ClusterNode> cluster_nodes{{classic_ports[0], "uuid-1"},
                                           {classic_ports[1], "uuid-2"},
                                           {classic_ports[2], "uuid-3"}};

  launch_mysql_server_mock(json_metadata, classic_ports[2], EXIT_SUCCESS, false,
                           http_ports[2]);
  const std::string router_options = get_router_options_as_json_str(
      std::nullopt, GetParam().unreachable_quorum_allowed_traffic);

  set_mock_metadata(http_ports[2], "uuid", gr_nodes, 2, cluster_nodes, 2, false,
                    "127.0.0.1", router_options);

  const auto router_ro_port = port_pool_.get_next_available();
  const auto router_rw_port = port_pool_.get_next_available();
  const auto router_rw_split_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(ClusterType::GR_V2, "0.2");
  const std::string routing_rw = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "first-available", "rw");
  const std::string routing_ro = get_metadata_cache_routing_section(
      router_ro_port, "SECONDARY", "round-robin-with-fallback", "ro");
  const std::string routing_rw_split =
      get_rw_split_routing_section(router_rw_split_port);

  std::vector<uint16_t> metadata_server_ports{
      classic_ports[0], classic_ports[1], classic_ports[2]};

  const auto sync_point = (GetParam().expect_rw_connection_ok ||
                           GetParam().expect_ro_connection_ok ||
                           GetParam().expect_rw_split_connection_ok)
                              ? ProcessManager::Spawner::SyncPoint::READY
                              : ProcessManager::Spawner::SyncPoint::RUNNING;

  const std::string conf_file = setup_router_config(
      metadata_cache_section, routing_rw + routing_ro + routing_rw_split,
      metadata_server_ports);

  router_spawner()
      .expected_exit_code(EXIT_SUCCESS)
      .wait_for_sync_point(sync_point)
      .spawn({"-c", conf_file});

  if (sync_point == ProcessManager::Spawner::SyncPoint::RUNNING) {
    EXPECT_TRUE(wait_for_transaction_count_increase(http_ports[2], 2));
  }

  if (GetParam().expect_rw_connection_ok) {
    make_new_connection_ok(router_rw_port, classic_ports[2]);
  } else {
    verify_new_connection_fails(router_rw_port);
  }

  if (GetParam().expect_ro_connection_ok) {
    make_new_connection_ok(router_ro_port, classic_ports[2]);

  } else {
    verify_new_connection_fails(router_ro_port);
  }

  if (GetParam().expect_rw_split_connection_ok) {
    make_new_connection_ok(router_rw_split_port, classic_ports[2]);
  } else {
    verify_new_connection_fails(router_rw_split_port);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Spec, AccessToPartitionWithNoQuorum,
    ::testing::Values(
        AccessToPartitionWithNoQuorumTestParam{
            "unreachable_quorum_allowed_traffic_default", "FR1.3,FR3",
            "by default Router shuts down accepting ports when it only has an "
            "access to node(s) with no quorum",
            /*unreachable_quorum_allowed_traffic*/ std::nullopt,
            /*expect_rw_connection_ok*/ false,
            /*expect_ro_connection_ok*/ false,
            /*expect_rw_split_connection_ok*/ false},
        AccessToPartitionWithNoQuorumTestParam{
            "unreachable_quorum_allowed_traffic_none", "FR1.3,FR3",
            "Router shuts down accepting ports when it only has an "
            "access to node(s) with no quorum when configured "
            "unreachable_quorum_allowed_traffic=none",
            /*unreachable_quorum_allowed_traffic*/ "none",
            /*expect_rw_connection_ok*/ false,
            /*expect_ro_connection_ok*/ false,
            /*expect_rw_split_connection_ok*/ false},
        AccessToPartitionWithNoQuorumTestParam{
            "unreachable_quorum_allowed_traffic_invalid", "FR1.3,FR3",
            "Router shuts down accepting ports when it only has an "
            "access to node(s) with no quorum when configured "
            "unreachable_quorum_allowed_traffic has unsupported value",
            /*unreachable_quorum_allowed_traffic*/ "invalid",
            /*expect_rw_connection_ok*/ false,
            /*expect_ro_connection_ok*/ false,
            /*expect_rw_split_connection_ok*/ false},
        AccessToPartitionWithNoQuorumTestParam{
            "unreachable_quorum_allowed_traffic_read", "FR1.1,FR3",
            "Router keeps the RO and RWsplit accepting ports open when it only "
            "has an access to node(s) with no quorum and  "
            "unreachable_quorum_allowed_traffic=read",
            /*unreachable_quorum_allowed_traffic*/ "read",
            /*expect_rw_connection_ok*/ false,
            /*expect_ro_connection_ok*/ true,
            /*expect_rw_split_connection_ok*/ true},
        AccessToPartitionWithNoQuorumTestParam{
            "unreachable_quorum_allowed_traffic_all", "FR1.2,FR3",
            "Router keeps all the accepting ports open when it only "
            "has an access to node(s) with no quorum and  "
            "unreachable_quorum_allowed_traffic=all",
            /*unreachable_quorum_allowed_traffic*/ "all",
            /*expect_rw_connection_ok*/ true,
            /*expect_ro_connection_ok*/ true,
            /*expect_rw_split_connection_ok*/ true}),
    [](const ::testing::TestParamInfo<AccessToPartitionWithNoQuorumTestParam>
           &info) { return info.param.test_name; });

struct AccessToBothPartitionsTestParam {
  std::string test_name;

  std::optional<std::string> unreachable_quorum_allowed_traffic;
};
class AccessToBothPartitions
    : public QuorumConnectionLostStandaloneClusterTest,
      public ::testing::WithParamInterface<AccessToBothPartitionsTestParam> {};

/* @test Check that unreachable_quorum_allowed_traffic does not matter when
 * there is a group with no quorum and group with a quorum and the Router has an
 * access to both. */
TEST_P(AccessToBothPartitions, Spec) {
  const std::string json_metadata =
      get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str();

  // The GR is split into 2 Groups, the second has quorum.
  // The Router has access to both.
  // Regardless of  unreachable_quorum_allowed_traffic it should always use
  // the second one (the one with quorum).

  // First partition sees:
  // [ONLINE, UNREACHABLE, UNREACHABLE ]
  // The second parition sees:
  // [UNREACHABLE, ONLINE, ONLINE ]

  std::vector<GRNode> gr_nodes_partition1{
      {classic_ports[0], "uuid-1", "ONLINE", "PRIMARY"},
      {classic_ports[1], "uuid-2", "UNREACHABLE", "SECONDARY"},
      {classic_ports[2], "uuid-3", "UNREACHABLE", "SECONDARY"}};

  std::vector<GRNode> gr_nodes_partition2{
      {classic_ports[0], "uuid-1", "UNREACHABLE", "SECONDARY"},
      {classic_ports[1], "uuid-2", "ONLINE", "PRIMARY"},
      {classic_ports[2], "uuid-3", "ONLINE", "SECONDARY"}};

  std::vector<::ClusterNode> cluster_nodes{{classic_ports[0], "uuid-1"},
                                           {classic_ports[1], "uuid-2"},
                                           {classic_ports[2], "uuid-3"}};

  const std::string router_options = get_router_options_as_json_str(
      std::nullopt, GetParam().unreachable_quorum_allowed_traffic);

  // launch first partition - 1 node
  launch_mysql_server_mock(json_metadata, classic_ports[0], EXIT_SUCCESS, false,
                           http_ports[0]);

  set_mock_metadata(http_ports[0], "uuid", gr_nodes_partition1, 0,
                    cluster_nodes, 2, false, "127.0.0.1", router_options);

  // launch second partition - 2 nodes
  for (size_t i = 1; i <= 2; ++i) {
    launch_mysql_server_mock(json_metadata, classic_ports[i], EXIT_SUCCESS,
                             false, http_ports[i]);

    set_mock_metadata(http_ports[i], "uuid", gr_nodes_partition2, i,
                      cluster_nodes, 1, false, "127.0.0.1", router_options);
  }

  const auto router_ro_port = port_pool_.get_next_available();
  const auto router_rw_port = port_pool_.get_next_available();
  const auto router_rw_split_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(ClusterType::GR_V2, "0.2");
  const std::string routing_rw = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "first-available", "rw");
  const std::string routing_ro = get_metadata_cache_routing_section(
      router_ro_port, "SECONDARY", "round-robin-with-fallback", "ro");
  const std::string routing_rw_split =
      get_rw_split_routing_section(router_rw_split_port);

  std::vector<uint16_t> metadata_server_ports{
      classic_ports[0], classic_ports[1], classic_ports[2]};

  /*auto &router = */ launch_router(metadata_cache_section,
                                    routing_rw + routing_ro + routing_rw_split,
                                    metadata_server_ports, EXIT_SUCCESS,
                                    /*wait_for_notify_ready=*/10s);

  // Regardless of the unreachable_quorum_allowed_traffic option setting, the
  // Router should always use the partition with the quorum for the traffic as
  // it has an access to it
  make_new_connection_ok(router_rw_port, classic_ports[1]);
  make_new_connection_ok(router_ro_port, classic_ports[2]);
  make_new_connection_ok(router_rw_split_port, classic_ports[1]);
}

INSTANTIATE_TEST_SUITE_P(
    Spec, AccessToBothPartitions,
    ::testing::Values(
        AccessToBothPartitionsTestParam{
            "unreachable_quorum_allowed_traffic_default",
            /*unreachable_quorum_allowed_traffic*/ std::nullopt},
        AccessToBothPartitionsTestParam{
            "unreachable_quorum_allowed_traffic_none",
            /*unreachable_quorum_allowed_traffic*/ "none"},
        AccessToBothPartitionsTestParam{
            "unreachable_quorum_allowed_traffic_read",
            /*unreachable_quorum_allowed_traffic*/ "read"},
        AccessToBothPartitionsTestParam{
            "unreachable_quorum_allowed_traffic_all",
            /*unreachable_quorum_allowed_traffic*/ "all"}),
    [](const ::testing::TestParamInfo<AccessToBothPartitionsTestParam> &info) {
      return info.param.test_name;
    });

struct BootstrapWithNoQuorumTestParam {
  std::string test_name;

  std::optional<std::string> unreachable_quorum_allowed_traffic;
};

class BootstrapWithNoQuorum
    : public QuorumConnectionLostStandaloneClusterTest,
      public ::testing::WithParamInterface<BootstrapWithNoQuorumTestParam> {};

// @test Check that the bootstrap always fails regardless of
// unreachable_quorum_allowed_traffic option value
TEST_P(BootstrapWithNoQuorum, Spec) {
  RecordProperty("Worklog", "15841");
  RecordProperty("RequirementId", "FR2");
  RecordProperty(
      "Description",
      "Checks that the Router fails to bootstrap if it only has an access to "
      "the subgroup of the Cluster members with no quorum");

  const std::string json_metadata =
      get_data_dir().join("bootstrap_gr.js").str();

  // The GR is plit into 2 partitions, the one with no quorum is used for
  // bootstrap. First partition is: [ONLINE, ONLINE, UNREACHABLE]. The second
  // partition is: [UNREACHABLE, UNREACHABLE, ONLINE ]. We only create the
  // second partition as the Router only has accsess only to this one anyway

  std::vector<GRNode> gr_nodes{
      {classic_ports[0], "uuid-1", "ONLINE", "PRIMARY"},
      {classic_ports[1], "uuid-2", "UNREACHABLE", "SECONDARY"},
      {classic_ports[2], "uuid-3", "UNREACHABLE", "SECONDARY"}};
  std::vector<::ClusterNode> cluster_nodes{{classic_ports[0], "uuid-1"},
                                           {classic_ports[1], "uuid-2"},
                                           {classic_ports[2], "uuid-3"}};

  launch_mysql_server_mock(json_metadata, classic_ports[0], EXIT_SUCCESS, false,
                           http_ports[0]);
  const std::string router_options = get_router_options_as_json_str(
      std::nullopt, GetParam().unreachable_quorum_allowed_traffic);

  set_mock_metadata(http_ports[0], "uuid", gr_nodes, 2, cluster_nodes, 2, false,
                    "127.0.0.1", router_options);

  auto &router = launch_router_for_bootstrap(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(classic_ports[0]),
          "--connect-timeout=1",
      },
      EXIT_FAILURE);

  EXPECT_NO_THROW(router.wait_for_exit());
  EXPECT_THAT(
      router.get_full_output(),
      ::testing::HasSubstr("Error: The provided server is currently not in a "
                           "InnoDB cluster group with quorum and thus may "
                           "contain inaccurate or outdated data."));
  check_exit_code(router, EXIT_FAILURE);
}

INSTANTIATE_TEST_SUITE_P(
    Spec, BootstrapWithNoQuorum,
    ::testing::Values(
        BootstrapWithNoQuorumTestParam{
            "unreachable_quorum_allowed_traffic_default",
            /*unreachable_quorum_allowed_traffic*/ std::nullopt},
        BootstrapWithNoQuorumTestParam{
            "unreachable_quorum_allowed_traffic_none",
            /*unreachable_quorum_allowed_traffic*/ "none"},
        BootstrapWithNoQuorumTestParam{
            "unreachable_quorum_allowed_traffic_read",
            /*unreachable_quorum_allowed_traffic*/ "read"},
        BootstrapWithNoQuorumTestParam{
            "unreachable_quorum_allowed_traffic_all",
            /*unreachable_quorum_allowed_traffic*/ "all"}),
    [](const ::testing::TestParamInfo<BootstrapWithNoQuorumTestParam> &info) {
      return info.param.test_name;
    });

class GRStateClusterSetTest : public GRStateTest {};

struct NoQuorumClusterSetTestParam {
  std::string test_name;
  std::string test_requirements;
  std::string test_description;

  std::optional<std::string> unreachable_quorum_allowed_traffic;

  unsigned target_cluster_id;

  bool expect_rw_connection_ok;
  bool expect_ro_connection_ok;
  bool expect_rw_split_connection_ok;
};

class ClusterSetAccessToPartitionWithNoQuorum
    : public GRStateClusterSetTest,
      public ::testing::WithParamInterface<NoQuorumClusterSetTestParam> {};

TEST_P(ClusterSetAccessToPartitionWithNoQuorum, Spec) {
  const unsigned target_cluster_id = GetParam().target_cluster_id;
  const std::string target_cluster = "00000000-0000-0000-0000-0000000000g" +
                                     std::to_string(target_cluster_id + 1);
  const std::string router_options = get_router_options_as_json_str(
      target_cluster, GetParam().unreachable_quorum_allowed_traffic);

  RecordProperty("Worklog", "15841");
  RecordProperty("RequirementId", GetParam().test_requirements);
  RecordProperty("Description", GetParam().test_description);

  const std::vector<size_t> gr_nodes_per_cluster{3, 3};

  ClusterSetOptions cs_options;
  cs_options.target_cluster_id = target_cluster_id;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = router_options;
  cs_options.gr_nodes_number = gr_nodes_per_cluster;
  create_clusterset(cs_options);

  for (size_t i = 1; i <= 2; ++i) {
    cs_options.topology.clusters[target_cluster_id].gr_nodes[i].member_status =
        "UNREACHABLE";
    cs_options.topology.clusters[target_cluster_id].nodes[i].process->kill();
  }

  for (size_t i = 1; i <= 2; ++i) {
    cs_options.topology.clusters[target_cluster_id]
        .nodes[i]
        .process->wait_for_exit();
  }

  const auto http_port =
      cs_options.topology.clusters[target_cluster_id].nodes[0].http_port;
  set_mock_clusterset_metadata(http_port,
                               /* this_cluster_id*/ target_cluster_id,
                               /*this_node_id*/ 0, cs_options);

  const auto router_ro_port = port_pool_.get_next_available();
  const auto router_rw_port = port_pool_.get_next_available();
  const auto router_rw_split_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(ClusterType::GR_V2, "0.2");
  const std::string routing_rw = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "first-available", "rw");
  const std::string routing_ro = get_metadata_cache_routing_section(
      router_ro_port, "SECONDARY", "round-robin-with-fallback", "ro");
  const std::string routing_rw_split =
      get_rw_split_routing_section(router_rw_split_port);

  const auto metadata_server_ports =
      cs_options.topology.get_md_servers_classic_ports();

  const auto sync_point = (GetParam().expect_rw_connection_ok ||
                           GetParam().expect_ro_connection_ok ||
                           GetParam().expect_rw_split_connection_ok)
                              ? ProcessManager::Spawner::SyncPoint::READY
                              : ProcessManager::Spawner::SyncPoint::RUNNING;

  const std::string conf_file = setup_router_config(
      metadata_cache_section, routing_rw + routing_ro + routing_rw_split,
      metadata_server_ports);

  router_spawner()
      .expected_exit_code(EXIT_SUCCESS)
      .wait_for_sync_point(sync_point)
      .spawn({"-c", conf_file});

  if (sync_point == ProcessManager::Spawner::SyncPoint::RUNNING) {
    EXPECT_TRUE(wait_for_transaction_count(http_port, 1));
  }

  if (GetParam().expect_rw_connection_ok) {
    make_new_connection_ok(
        router_rw_port,
        cs_options.topology.clusters[target_cluster_id].nodes[0].classic_port);
  } else {
    verify_new_connection_fails(router_rw_port);
  }

  if (GetParam().expect_ro_connection_ok) {
    make_new_connection_ok(
        router_ro_port,
        cs_options.topology.clusters[target_cluster_id].nodes[0].classic_port);

  } else {
    verify_new_connection_fails(router_ro_port);
  }

  if (GetParam().expect_rw_split_connection_ok) {
    make_new_connection_ok(
        router_rw_split_port,
        cs_options.topology.clusters[target_cluster_id].nodes[0].classic_port);

  } else {
    verify_new_connection_fails(router_rw_split_port);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Spec, ClusterSetAccessToPartitionWithNoQuorum,
    ::testing::Values(
        NoQuorumClusterSetTestParam{
            "unreachable_quorum_allowed_traffic_default", "FR1.3,FR3",
            "Checks that when the target cluster is Primary cluster of the "
            "ClusterSet by default Router shuts down accepting ports when it "
            "only has an access to node(s) with no quorum",
            /*unreachable_quorum_allowed_traffic*/ std::nullopt,
            /*target_cluster_id*/ 0,
            /*expect_rw_connection_ok*/ false,
            /*expect_ro_connection_ok*/ false,
            /*expect_rw_split_connection_ok*/ false},
        NoQuorumClusterSetTestParam{
            "unreachable_quorum_allowed_traffic_none", "FR1.3,FR3",
            "Checks that when the target cluster is Primary cluster of the "
            "ClusterSet the Router shuts down accepting ports when it "
            "only has an access to node(s) with no quorum and "
            "unreachable_quorum_allowed_traffic=none",
            /*unreachable_quorum_allowed_traffic*/ "none",
            /*target_cluster_id*/ 0,
            /*expect_rw_connection_ok*/ false,
            /*expect_ro_connection_ok*/ false,
            /*expect_rw_split_connection_ok*/ false},
        NoQuorumClusterSetTestParam{
            "unreachable_quorum_allowed_traffic_read", "FR1.1,FR3",
            "Checks that when the target cluster is Primary cluster of the "
            "ClusterSet the Router keeps RO and RWsplit ports open when it "
            "only has an access to node(s) with no quorum and "
            "unreachable_quorum_allowed_traffic=read",
            /*unreachable_quorum_allowed_traffic*/ "read",
            /*target_cluster_id*/ 0,
            /*expect_rw_connection_ok*/ false,
            /*expect_ro_connection_ok*/ true,
            /*expect_rw_split_connection_ok*/ true},
        NoQuorumClusterSetTestParam{
            "unreachable_quorum_allowed_traffic_all", "FR1.2,FR3",
            "Checks that when the target cluster is Primary cluster of the "
            "ClusterSet the Router keeps all the accepting ports open when it "
            "only has an access to node(s) with no quorum and "
            "unreachable_quorum_allowed_traffic=all",
            /*unreachable_quorum_allowed_traffic*/ "all",
            /*target_cluster_id*/ 0,
            /*expect_rw_connection_ok*/ true,
            /*expect_ro_connection_ok*/ true,
            /*expect_rw_split_connection_ok*/ true},
        NoQuorumClusterSetTestParam{
            "target_replica_unreachable_quorum_allowed_traffic_default",
            "FR1.3,FR3",
            "Checks that when the target cluster is Replica cluster of the "
            "ClusterSet by default Router shuts down accepting ports when it "
            "only has an access to node(s) with no quorum",
            /*unreachable_quorum_allowed_traffic*/ std::nullopt,
            /*target_cluster_id*/ 1,
            /*expect_rw_connection_ok*/ false,
            /*expect_ro_connection_ok*/ false,
            /*expect_rw_split_connection_ok*/ false},
        NoQuorumClusterSetTestParam{
            "target_replica_unreachable_quorum_allowed_traffic_none",
            "FR1.3,FR3",
            "Checks that when the target cluster is Replica cluster of the "
            "ClusterSet the Router shuts down accepting ports when it "
            "only has an access to node(s) with no quorum and "
            "unreachable_quorum_allowed_traffic=none",
            /*unreachable_quorum_allowed_traffic*/ "none",
            /*target_cluster_id*/ 1,
            /*expect_rw_connection_ok*/ false,
            /*expect_ro_connection_ok*/ false,
            /*expect_rw_split_connection_ok*/ false},
        NoQuorumClusterSetTestParam{
            "target_replica_unreachable_quorum_allowed_traffic_read",
            "FR1.1,FR3",
            "Checks that when the target cluster is Replica cluster of the "
            "ClusterSet the Router keeps RO and RWsplit ports open when it "
            "only has an access to node(s) with no quorum and "
            "unreachable_quorum_allowed_traffic=read",
            /*unreachable_quorum_allowed_traffic*/ "read",
            /*target_cluster_id*/ 1,
            /*expect_rw_connection_ok*/ false,
            /*expect_ro_connection_ok*/ true,
            /*expect_rw_split_connection_ok*/ true},
        // our target is replica so we never expect RW port open
        NoQuorumClusterSetTestParam{
            "target_replica_unreachable_quorum_allowed_traffic_all",
            "FR1.2,FR3",
            "Checks that when the target cluster is Replica cluster of the "
            "ClusterSet the Router keeps RO and RW split accepting ports open "
            "when it only has an access to node(s) with no quorum and "
            "unreachable_quorum_allowed_traffic=all",
            /*unreachable_quorum_allowed_traffic*/ "all",
            /*target_cluster_id*/ 1,
            /*expect_rw_connection_ok*/ false,
            /*expect_ro_connection_ok*/ true,
            /*expect_rw_split_connection_ok*/ true}),
    [](const ::testing::TestParamInfo<NoQuorumClusterSetTestParam> &info) {
      return info.param.test_name;
    });

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
