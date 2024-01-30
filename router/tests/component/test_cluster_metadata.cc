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

using mysqlrouter::ClusterType;
using mysqlrouter::MetadataSchemaVersion;
using mysqlrouter::MySQLSession;
using ::testing::PrintToString;
using namespace std::chrono_literals;
using namespace std::string_literals;

class RouterComponenClustertMetadataTest : public RouterComponentMetadataTest {
};

struct ClusterMetadataTestParams {
  // mock_server trace file
  std::string tracefile;
  // additional info about the testcase that gets printed by the gtest in the
  // results
  std::string description;
  // the type of the cluster GR or AR
  ClusterType cluster_type;
  // ttl value we want to set (floating point decimal in seconds)
  std::string ttl;

  ClusterMetadataTestParams(std::string tracefile_, std::string description_,
                            ClusterType cluster_type_, std::string ttl_ = "0.5")
      : tracefile(std::move(tracefile_)),
        description(std::move(description_)),
        cluster_type(cluster_type_),
        ttl(std::move(ttl_)) {}
};

auto get_test_description(
    const ::testing::TestParamInfo<ClusterMetadataTestParams> &info) {
  return info.param.description;
}

class RouterComponenClustertMetadataTestInstanceListUnordered
    : public RouterComponenClustertMetadataTest,
      public ::testing::WithParamInterface<ClusterMetadataTestParams> {};

/**
 * @test Checks that when for some reason the metadata server starts
 *       returning the information about the cluster nodes in different order we
 *       will not treat this as a change (Bug#29264764).
 */
TEST_P(RouterComponenClustertMetadataTestInstanceListUnordered,
       InstancesListUnordered) {
  const std::string kGroupID = "uuid";

  SCOPED_TRACE("// launch 2 server mocks");
  std::vector<ProcessWrapper *> nodes;
  std::vector<uint16_t> node_classic_ports;
  std::vector<uint16_t> node_http_ports;
  const std::string json_metadata =
      get_data_dir().join(GetParam().tracefile).str();

  using ClusterNode = ::ClusterNode;
  std::vector<GRNode> gr_nodes;
  std::vector<ClusterNode> cluster_nodes;
  for (size_t i = 0; i < 2; ++i) {
    node_classic_ports.push_back(port_pool_.get_next_available());
    node_http_ports.push_back(port_pool_.get_next_available());

    nodes.push_back(
        &launch_mysql_server_mock(json_metadata, node_classic_ports[i],
                                  EXIT_SUCCESS, false, node_http_ports[i]));
    const std::string role = (i == 0) ? "PRIMARY" : "SECONDARY";
    gr_nodes.emplace_back(node_classic_ports[i],
                          "uuid-" + std::to_string(i + 1), "ONLINE", role);
    cluster_nodes.emplace_back(node_classic_ports[i],
                               "uuid-" + std::to_string(i + 1));
  }

  for (auto [i, http_port] : stdx::views::enumerate(node_http_ports)) {
    set_mock_metadata(http_port, kGroupID, gr_nodes, i, cluster_nodes);
  }

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type, GetParam().ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router = launch_router(metadata_cache_section, routing_section,
                               {node_classic_ports}, EXIT_SUCCESS, 5s);

  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0]));

  SCOPED_TRACE("// instruct the mocks to return nodes in reverse order");
  const std::vector<GRNode> gr_nodes_reversed(gr_nodes.rbegin(),
                                              gr_nodes.rend());
  const std::vector<ClusterNode> cluster_nodes_reversed(cluster_nodes.rbegin(),
                                                        cluster_nodes.rend());
  for (auto [i, http_port] : stdx::views::enumerate(node_http_ports)) {
    set_mock_metadata(http_port, kGroupID, gr_nodes_reversed, i,
                      cluster_nodes_reversed);
  }

  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0]));

  SCOPED_TRACE("// check it is not treated as a change");
  const std::string needle = "Potential changes detected in cluster";
  const std::string log_content = router.get_logfile_content();

  // 1 is expected, that comes from the initial reading of the metadata
  EXPECT_EQ(1, count_str_occurences(log_content, needle)) << log_content;
}

INSTANTIATE_TEST_SUITE_P(
    InstancesListUnordered,
    RouterComponenClustertMetadataTestInstanceListUnordered,
    ::testing::Values(
        ClusterMetadataTestParams("metadata_dynamic_nodes_v2_gr.js",
                                  "unordered_gr_v2", ClusterType::GR_V2, "0.1"),
        ClusterMetadataTestParams("metadata_dynamic_nodes_v2_ar.js",
                                  "unordered_ar_v2", ClusterType::RS_V2,
                                  "0.1")),
    get_test_description);

class RouterComponenClustertMetadataTestInvalidMysqlXPort
    : public RouterComponenClustertMetadataTest,
      public ::testing::WithParamInterface<ClusterMetadataTestParams> {};

/**
 * @test Check that invalid mysqlx port in the metadata does not cause the node
 * to be discarded for the classic protocol connections (Bug#30617645)
 */
TEST_P(RouterComponenClustertMetadataTestInvalidMysqlXPort, InvalidMysqlXPort) {
  const std::string json_metadata =
      get_data_dir().join(GetParam().tracefile).str();

  SCOPED_TRACE("// single node cluster is fine for this test");
  const uint16_t node_classic_port{port_pool_.get_next_available()};
  const uint16_t node_http_port{port_pool_.get_next_available()};
  const uint32_t kInvalidPort{76000};

  /*auto &cluster_node = */ launch_mysql_server_mock(
      json_metadata, node_classic_port, EXIT_SUCCESS, false, node_http_port);

  SCOPED_TRACE(
      "// let the metadata for our single node report invalid mysqlx port");
  auto cluster_nodes = classic_ports_to_cluster_nodes({node_classic_port});
  cluster_nodes[0].x_port = kInvalidPort;
  set_mock_metadata(node_http_port, "uuid",
                    classic_ports_to_gr_nodes({node_classic_port}), 0,
                    cluster_nodes, 0, false, "127.0.0.1");

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type, GetParam().ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router = launch_router(metadata_cache_section, routing_section,
                               {node_classic_port}, EXIT_SUCCESS, 5s);

  // TODO: still needed?
  ASSERT_TRUE(wait_metadata_read(router, 5s)) << router.get_full_output();

  SCOPED_TRACE(
      "// Even though the metadata contains invalid mysqlx port we still "
      "should be able to connect on the classic port");
  EXPECT_TRUE(wait_for_port_ready(router_port));
  MySQLSession client;
  ASSERT_NO_FATAL_FAILURE(
      client.connect("127.0.0.1", router_port, "username", "password", "", ""));
}

INSTANTIATE_TEST_SUITE_P(
    InvalidMysqlXPort, RouterComponenClustertMetadataTestInvalidMysqlXPort,
    ::testing::Values(
        ClusterMetadataTestParams("metadata_dynamic_nodes_v2_gr.js", "gr_v2",
                                  ClusterType::GR_V2, "5"),
        ClusterMetadataTestParams("metadata_dynamic_nodes_v2_ar.js", "ar_v2",
                                  ClusterType::RS_V2, "5")),
    get_test_description);

class CheckRouterInfoUpdatesTest
    : public RouterComponenClustertMetadataTest,
      public ::testing::WithParamInterface<ClusterMetadataTestParams> {};

/**
 * @test Checks that the Router updates the static configuration information in
 * the metadata once when it starts and that the periodic updates are done every
 * 10th metadata refresh when working with standalone Cluster (that is not a
 * part of a ClusterSet).
 */
TEST_P(CheckRouterInfoUpdatesTest, CheckRouterInfoUpdates) {
  const auto router_port = port_pool_.get_next_available();
  SCOPED_TRACE(
      "// launch the server mock (it's our metadata server and single cluster "
      "node)");
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir().join(GetParam().tracefile).str();

  /*auto &metadata_server = */ launch_mysql_server_mock(
      json_metadata, md_server_port, EXIT_SUCCESS, false, md_server_http_port);

  auto globals = mock_GR_metadata_as_json(
      "uuid", classic_ports_to_gr_nodes({md_server_port}), 0,
      classic_ports_to_cluster_nodes({md_server_port}));

  const auto globals_str = json_to_string(globals);
  MockServerRestClient(md_server_http_port).set_globals(globals_str);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");

  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type, GetParam().ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  launch_router(metadata_cache_section, routing_section, {md_server_port},
                EXIT_SUCCESS,
                /*wait_for_notify_ready=*/30s);

  SCOPED_TRACE("// let the router run for at least 10 metadata refresh cycles");
  EXPECT_TRUE(wait_for_transaction_count_increase(md_server_http_port, 12));

  SCOPED_TRACE("// we still expect the version to be only set once");
  std::string server_globals =
      MockServerRestClient(md_server_http_port).get_globals_as_json_string();
  const int attributes_upd_count = get_update_attributes_count(server_globals);
  EXPECT_EQ(1, attributes_upd_count);

  SCOPED_TRACE(
      "// Let's check if the first query is starting a trasaction and the "
      "second checking the version");

  const auto queries = get_array_field_value(server_globals, "queries");
  EXPECT_EQ(4u, queries.size()) << server_globals;

  EXPECT_STREQ(
      "SET @@SESSION.autocommit=1, @@SESSION.character_set_client=utf8, "
      "@@SESSION.character_set_results=utf8, "
      "@@SESSION.character_set_connection=utf8, "
      "@@SESSION.sql_mode='ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_"
      "DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION', "
      "@@SESSION.optimizer_switch='derived_merge=on'",
      queries.at(0).c_str());
  EXPECT_STREQ("SET @@SESSION.group_replication_consistency='EVENTUAL'",
               queries.at(1).c_str());
  EXPECT_STREQ("START TRANSACTION", queries.at(2).c_str());
  EXPECT_STREQ("SELECT * FROM mysql_innodb_cluster_metadata.schema_version",
               queries.at(3).c_str());

  {
    SCOPED_TRACE(
        "// last_check_in should be attempted at least twice (first update is "
        "done on start)");
    std::string server_globals =
        MockServerRestClient(md_server_http_port).get_globals_as_json_string();
    const int last_check_in_upd_count =
        get_update_last_check_in_count(server_globals);
    EXPECT_GE(2, last_check_in_upd_count);
  }

  {
    std::string server_globals =
        MockServerRestClient(md_server_http_port).get_globals_as_json_string();

    const std::string router_version =
        get_str_field_value(server_globals, "upd_attr_router_version");
    EXPECT_STREQ(MYSQL_ROUTER_VERSION, router_version.c_str())
        << server_globals;

    const std::string md_username =
        get_str_field_value(server_globals, "upd_attr_md_username");
    EXPECT_STREQ(router_metadata_username.c_str(), md_username.c_str())
        << server_globals;

    const std::string rw_classic_port =
        get_str_field_value(server_globals, "upd_attr_rw_classic_port");
    EXPECT_STREQ(rw_classic_port.c_str(), std::to_string(router_port).c_str())
        << server_globals;

    SCOPED_TRACE(
        "// verify the JSON config set by the Router in the attributes against "
        "the schema");

    RecordProperty("Worklog", "15649");
    RecordProperty("RequirementId", "FR1,FR1.2,FR2");
    RecordProperty("Description",
                   "Testing if the Router correctly exposes it's full static "
                   "configuration upon start.");

    // first validate the configuration json against general "public" schema for
    // the structure corectness
    const std::string public_config_schema =
        get_file_output(Path(ROUTER_SRC_DIR)
                            .join("src")
                            .join("harness")
                            .join("src")
                            .join("configuration_schema.json")
                            .str());

    validate_config_stored_in_md(md_server_http_port, public_config_schema);

    // then validate against strict schema that also checks the values expected
    // for the current configuration
    const std::string strict_config_schema = get_file_output(
        get_data_dir().join("configuration_schema_strict.json").str());

    validate_config_stored_in_md(md_server_http_port, strict_config_schema);
  }
}

INSTANTIATE_TEST_SUITE_P(
    CheckRouterInfoUpdates, CheckRouterInfoUpdatesTest,
    ::testing::Values(ClusterMetadataTestParams(
                          "metadata_dynamic_nodes_version_update_v2_gr.js",
                          "router_version_update_once_gr_v2",
                          ClusterType::GR_V2, "0.1"),
                      ClusterMetadataTestParams(
                          "metadata_dynamic_nodes_version_update_v2_ar.js",
                          "router_version_update_once_ar_v2",
                          ClusterType::RS_V2, "0.1")),
    get_test_description);

/**
 * @test Verify that when the Router was bootstrapped against the Cluster while
 * it was a standalone Cluster and now it is part of a ClusterSet, Router checks
 * v2_cs_router_options for periodic updates frequency
 */
TEST_F(RouterComponenClustertMetadataTest,
       CheckRouterInfoUpdatesClusterPartOfCS) {
  const auto router_port = port_pool_.get_next_available();
  SCOPED_TRACE(
      "// launch the server mock (it's our metadata server and single cluster "
      "node)");
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir()
          .join("metadata_dynamic_nodes_version_update_v2_gr.js")
          .str();

  /*auto &metadata_server = */ launch_mysql_server_mock(
      json_metadata, md_server_port, EXIT_SUCCESS, false, md_server_http_port);

  SCOPED_TRACE(
      "// let's tell the mock which attributes it should expect so that it "
      "does the strict sql matching for us");
  auto globals = mock_GR_metadata_as_json(
      "uuid", classic_ports_to_gr_nodes({md_server_port}), 0,
      classic_ports_to_cluster_nodes({md_server_port}));

  JsonAllocator allocator;

  // instrument the metadata in a way that shows that we bootstrapped once the
  // Cluster was standalone but now it is part of a ClusterSet
  globals.AddMember("bootstrap_target_type", "cluster", allocator);
  globals.AddMember("clusterset_present", 1, allocator);
  const auto globals_str = json_to_string(globals);
  MockServerRestClient(md_server_http_port).set_globals(globals_str);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");

  const std::string metadata_cache_section =
      get_metadata_cache_section(ClusterType::GR_V2, "0.1");
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  launch_router(metadata_cache_section, routing_section, {md_server_port},
                EXIT_SUCCESS,
                /*wait_for_notify_ready=*/30s);

  SCOPED_TRACE("// let the router run for at least 10 metadata refresh cycles");
  EXPECT_TRUE(wait_for_transaction_count_increase(md_server_http_port, 12));

  SCOPED_TRACE("// we expect the version to be only set once");
  std::string server_globals =
      MockServerRestClient(md_server_http_port).get_globals_as_json_string();
  const int attributes_upd_count = get_update_attributes_count(server_globals);
  EXPECT_EQ(1, attributes_upd_count);

  // We were bootstrapped once the Cluster was standalone Cluster. Now it is
  // part of the ClusterSet. Even though we keep using the Cluster as a
  // standalone Cluster, we make an expection when it comes to periodic updates.
  // We don't want to do them unless the frequency is explicitly set in the
  // v2_cs_router_options.
  const int last_check_in_upd_count =
      get_update_last_check_in_count(server_globals);

  // since the frequency is not set in v2_cs_router_options we do not expect any
  // periodic updates
  EXPECT_EQ(0, last_check_in_upd_count);
}

/**
 * @test verify if appropriate warning messages are logged when the Cluster has
 * deprecated metadata version.
 *
 * Disabled as there is currently no deprecated version. Version 1.x is no
 * longer supported.
 */
TEST_F(RouterComponenClustertMetadataTest,
       DISABLED_LogWarningWhenMetadataIsDeprecated) {
  RecordProperty("Worklog", "15876");
  RecordProperty("RequirementId", "FR1");
  RecordProperty("Description",
                 "Checks that the Router logs a deprecation warning for "
                 "metadata version 1.x exactly once per each metadata server");

  // create a 2-node cluster
  const std::vector<uint16_t> cluster_nodes_ports{
      port_pool_.get_next_available(), port_pool_.get_next_available()};
  const std::vector<uint16_t> cluster_nodes_http_ports{
      port_pool_.get_next_available(), port_pool_.get_next_available()};

  for (size_t i = 0; i < cluster_nodes_ports.size(); ++i) {
    const auto classic_port = cluster_nodes_ports[i];
    const auto http_port = cluster_nodes_http_ports[i];
    launch_mysql_server_mock(
        get_data_dir().join("metadata_dynamic_nodes.js").str(), classic_port,
        EXIT_SUCCESS, false, http_port);

    EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
    set_mock_metadata(http_port, "uuid",
                      classic_ports_to_gr_nodes(cluster_nodes_ports), 1,
                      classic_ports_to_cluster_nodes(cluster_nodes_ports));
  }

  // launch the router with metadata-cache configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(ClusterType::GR_V2, "0.1");
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  auto &router = launch_router(metadata_cache_section, routing_section,
                               cluster_nodes_ports, EXIT_SUCCESS,
                               /*wait_for_notify_ready=*/30s);

  // let the Router run for a several metadata refresh cycles
  wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 6);

  // check that warning about deprecated metadata was logged once (we only
  // connected to a single metadata server as it is a part of quorum)
  check_log_contains(
      router,
      "Instance '127.0.0.1:" + std::to_string(cluster_nodes_ports[0]) +
          "': The target Cluster's Metadata version ('1.0.2') is "
          "deprecated. Please use the latest MySQL Shell to upgrade it using "
          "'dba.upgradeMetadata()'.",
      1);
}

class PermissionErrorOnVersionUpdateTest
    : public RouterComponenClustertMetadataTest,
      public ::testing::WithParamInterface<ClusterMetadataTestParams> {};

TEST_P(PermissionErrorOnVersionUpdateTest, PermissionErrorOnAttributesUpdate) {
  const auto router_port = port_pool_.get_next_available();
  SCOPED_TRACE(
      "// launch the server mock (it's our metadata server and single cluster "
      "node)");
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir().join(GetParam().tracefile).str();

  /*auto &metadata_server =*/launch_mysql_server_mock(
      json_metadata, md_server_port, EXIT_SUCCESS, false, md_server_http_port);

  SCOPED_TRACE(
      "// let's tell the mock which attributes it should expect so that it "
      "does the strict sql matching for us, also tell it to issue the "
      "permission error on the update attempt");
  auto globals = mock_GR_metadata_as_json(
      "uuid", classic_ports_to_gr_nodes({md_server_port}), 0,
      classic_ports_to_cluster_nodes({md_server_port}));
  JsonAllocator allocator;
  globals.AddMember("router_version", MYSQL_ROUTER_VERSION, allocator);
  globals.AddMember("router_rw_classic_port", router_port, allocator);
  globals.AddMember("router_metadata_user",
                    JsonValue(router_metadata_username.c_str(),
                              router_metadata_username.length(), allocator),
                    allocator);

  globals.AddMember("perm_error_on_version_update", 1, allocator);
  const auto globals_str = json_to_string(globals);
  MockServerRestClient(md_server_http_port).set_globals(globals_str);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");

  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type, GetParam().ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router = launch_router(metadata_cache_section, routing_section,
                               {md_server_port}, EXIT_SUCCESS,
                               /*wait_for_notify_ready=*/30s);

  SCOPED_TRACE(
      "// wait for several Router transactions on the metadata server");
  EXPECT_TRUE(wait_for_transaction_count_increase(md_server_http_port, 6));

  SCOPED_TRACE(
      "// we expect the error trying to update the attributes in the log "
      "exactly once");
  const std::string log_content = router.get_logfile_content();
  const std::string needle =
      "Make sure to follow the correct steps to upgrade your metadata.\n"
      "Run the dba.upgradeMetadata() then launch the new Router version "
      "when prompted";
  EXPECT_EQ(1, count_str_occurences(log_content, needle)) << log_content;

  SCOPED_TRACE(
      "// we expect that the router attempted to update the continuously "
      "because of the missing access rights error");
  std::string server_globals =
      MockServerRestClient(md_server_http_port).get_globals_as_json_string();
  const int attributes_upd_count = get_update_attributes_count(server_globals);
  EXPECT_GT(attributes_upd_count, 1);

  SCOPED_TRACE(
      "// It should still not be fatal, the router should accept the "
      "connections to the cluster");
  MySQLSession client;
  ASSERT_NO_FATAL_FAILURE(
      client.connect("127.0.0.1", router_port, "username", "password", "", ""));
}

INSTANTIATE_TEST_SUITE_P(
    PermissionErrorOnVersionUpdate, PermissionErrorOnVersionUpdateTest,
    ::testing::Values(ClusterMetadataTestParams(
                          "metadata_dynamic_nodes_version_update_v2_gr.js",
                          "router_version_update_fail_on_perm_gr_v2",
                          ClusterType::GR_V2, "0.1"),
                      ClusterMetadataTestParams(
                          "metadata_dynamic_nodes_version_update_v2_ar.js",
                          "router_version_update_fail_on_perm_ar_v2",
                          ClusterType::RS_V2, "0.1")),
    get_test_description);

class UpgradeInProgressTest
    : public RouterComponenClustertMetadataTest,
      public ::testing::WithParamInterface<ClusterMetadataTestParams> {};

TEST_P(UpgradeInProgressTest, UpgradeInProgress) {
  SCOPED_TRACE(
      "// launch the server mock (it's our metadata server and single cluster "
      "node)");
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir().join(GetParam().tracefile).str();

  /*auto &metadata_server = */ launch_mysql_server_mock(
      json_metadata, md_server_port, EXIT_SUCCESS, false, md_server_http_port);
  set_mock_metadata(md_server_http_port, "uuid",
                    classic_ports_to_gr_nodes({md_server_port}), 0,
                    classic_ports_to_cluster_nodes({md_server_port}));

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const auto router_port = port_pool_.get_next_available();

  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type, GetParam().ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router = launch_router(metadata_cache_section, routing_section,
                               {md_server_port}, EXIT_SUCCESS,
                               /*wait_for_notify_ready=*/30s);
  EXPECT_TRUE(wait_for_port_used(router_port));

  SCOPED_TRACE("// let us make some user connection via the router port");
  auto client = make_new_connection_ok(router_port, md_server_port);

  SCOPED_TRACE("// let's mimmic start of the metadata update now");
  auto globals = mock_GR_metadata_as_json(
      "uuid", classic_ports_to_gr_nodes({md_server_port}), 0,
      classic_ports_to_cluster_nodes({md_server_port}));
  JsonAllocator allocator;
  globals.AddMember("upgrade_in_progress", 1, allocator);
  globals.AddMember("md_query_count", 0, allocator);
  const auto globals_str = json_to_string(globals);
  MockServerRestClient(md_server_http_port).set_globals(globals_str);

  SCOPED_TRACE(
      "// Wait some more and read the metadata update count once more to avoid "
      "race condition.");
  EXPECT_TRUE(wait_for_transaction_count_increase(md_server_http_port, 2));
  MockServerRestClient(md_server_http_port).get_globals_as_json_string();
  std::string server_globals =
      MockServerRestClient(md_server_http_port).get_globals_as_json_string();
  int metadata_upd_count = get_ttl_queries_count(server_globals);

  SCOPED_TRACE(
      "// Now wait another 3 ttl periods, since the metadata update is in "
      "progress we do not expect the increased number of metadata queries "
      "after that period");
  EXPECT_TRUE(wait_for_transaction_count_increase(md_server_http_port, 3));
  server_globals =
      MockServerRestClient(md_server_http_port).get_globals_as_json_string();
  const int metadata_upd_count2 = get_ttl_queries_count(server_globals);
  EXPECT_EQ(metadata_upd_count, metadata_upd_count2);

  SCOPED_TRACE(
      "// Even though the upgrade is in progress the existing connection "
      "should still be active.");
  verify_existing_connection_ok(client.get());

  SCOPED_TRACE("// Also we should be able to create a new conenction.");
  MySQLSession client2;
  ASSERT_NO_FATAL_FAILURE(client2.connect("127.0.0.1", router_port, "username",
                                          "password", "", ""));

  SCOPED_TRACE("// Info about the update should be logged.");
  const std::string log_content = router.get_logfile_content();
  ASSERT_TRUE(log_content.find("Cluster metadata upgrade in progress, aborting "
                               "the metada refresh") != std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    UpgradeInProgress, UpgradeInProgressTest,
    ::testing::Values(ClusterMetadataTestParams(
                          "metadata_dynamic_nodes_version_update_v2_gr.js",
                          "metadata_upgrade_in_progress_gr_v2",
                          ClusterType::GR_V2, "0.1"),
                      ClusterMetadataTestParams(
                          "metadata_dynamic_nodes_version_update_v2_ar.js",
                          "metadata_upgrade_in_progress_ar_v2",
                          ClusterType::RS_V2, "0.1")),
    get_test_description);

/**
 * @test
 * Verify that when the cluster node returns empty dataset from the
 * v2_this_instance view, the router fails over to the other known nodes to try
 * to read the metadata (BUG#30733189)
 */
class NodeRemovedTest
    : public RouterComponenClustertMetadataTest,
      public ::testing::WithParamInterface<ClusterMetadataTestParams> {};

TEST_P(NodeRemovedTest, NodeRemoved) {
  const size_t NUM_NODES = 2;
  std::vector<uint16_t> node_ports, node_http_ports;
  std::vector<ProcessWrapper *> cluster_nodes;

  SCOPED_TRACE("// launch cluster with 2 nodes");
  const std::string json_metadata =
      get_data_dir().join(GetParam().tracefile).str();

  for (size_t i = 0; i < NUM_NODES; ++i) {
    node_ports.push_back(port_pool_.get_next_available());
    node_http_ports.push_back(port_pool_.get_next_available());
  }

  for (size_t i = 0; i < NUM_NODES; ++i) {
    cluster_nodes.push_back(&launch_mysql_server_mock(
        json_metadata, node_ports[i], EXIT_SUCCESS, false, node_http_ports[i]));
    set_mock_metadata(node_http_ports[i], "uuid",
                      classic_ports_to_gr_nodes(node_ports), i,
                      classic_ports_to_cluster_nodes(node_ports));
  }

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const auto router_port = port_pool_.get_next_available();

  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type, GetParam().ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  launch_router(metadata_cache_section, routing_section, node_ports,
                EXIT_SUCCESS,
                /*wait_for_notify_ready=*/30s);

  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[0], 2));
  SCOPED_TRACE(
      "// Make a connection to the primary, it should be the first node");
  { /*auto client =*/
    make_new_connection_ok(router_port, node_ports[0]);
  }

  SCOPED_TRACE(
      "// Mimic the removal of the first node, this_instance view on this node "
      "should return empty dataset");
  auto globals =
      mock_GR_metadata_as_json("uuid", classic_ports_to_gr_nodes(node_ports), 0,
                               classic_ports_to_cluster_nodes(node_ports));
  JsonAllocator allocator;
  globals.AddMember("cluster_type", "", allocator);
  const auto globals_str = json_to_string(globals);
  MockServerRestClient(node_http_ports[0]).set_globals(globals_str);

  SCOPED_TRACE(
      "// Tell the second node that it is a new Primary and the only member of "
      "the cluster");
  set_mock_metadata(node_http_ports[1], "uuid",
                    classic_ports_to_gr_nodes({node_ports[1]}), 0,
                    classic_ports_to_cluster_nodes({node_ports[1]}));

  SCOPED_TRACE(
      "// Connect to the router primary port, the connection should be ok and "
      "we should be connected to the new primary now");
  EXPECT_TRUE(wait_for_transaction_count_increase(node_http_ports[1], 2));

  SCOPED_TRACE("// let us make some user connection via the router port");
  /*auto client =*/make_new_connection_ok(router_port, node_ports[1]);
}

INSTANTIATE_TEST_SUITE_P(
    NodeRemoved, NodeRemovedTest,
    ::testing::Values(
        ClusterMetadataTestParams("metadata_dynamic_nodes_v2_gr.js",
                                  "node_removed_gr_v2", ClusterType::GR_V2,
                                  "0.1"),
        ClusterMetadataTestParams("metadata_dynamic_nodes_v2_ar.js",
                                  "node_removed_ar_v2", ClusterType::RS_V2,
                                  "0.1")),
    get_test_description);

class MetadataCacheMetadataServersOrder
    : public RouterComponenClustertMetadataTest,
      public ::testing::WithParamInterface<ClusterMetadataTestParams> {};

TEST_P(MetadataCacheMetadataServersOrder, MetadataServersOrder) {
  const size_t kClusterNodes{3};
  std::vector<ProcessWrapper *> cluster_nodes;
  std::vector<uint16_t> md_servers_classic_ports, md_servers_http_ports;

  // launch the mock servers
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
      router_ro_port, "PRIMARY", "round-robin", "ro");
  /*auto &router =*/
  launch_router(metadata_cache_section, routing_rw_section + routing_ro_section,
                md_servers_classic_ports, EXIT_SUCCESS,
                /*wait_for_notify_ready=*/30s);

  // check first metadata server (PRIMARY) is queried for metadata
  EXPECT_TRUE(wait_for_transaction_count_increase(md_servers_http_ports[0], 2));

  // check that 2nd and 3rd servers (SECONDARIES) are NOT queried for metadata
  // in case of ReplicaSet Cluster every node gets queried for view_id so this
  // check would fail
  if (GetParam().cluster_type != mysqlrouter::ClusterType::RS_V2) {
    for (const auto i : {1, 2}) {
      EXPECT_FALSE(wait_for_transaction_count_increase(md_servers_http_ports[i],
                                                       1, 200ms));
    }
  }

  // check that the PRIMARY is first in the state file
  check_state_file(state_file_, GetParam().cluster_type, "uuid",
                   {md_servers_classic_ports[0], md_servers_classic_ports[1],
                    md_servers_classic_ports[2]});

  // now promote first SECONDARY to become new PRIMARY
  auto gr_nodes = classic_ports_to_gr_nodes(md_servers_classic_ports);
  auto metadata_nodes =
      classic_ports_to_cluster_nodes(md_servers_classic_ports);

  if (GetParam().cluster_type != mysqlrouter::ClusterType::RS_V2) {
    // For ReplicaSet there is no GR and role is determined directly in the
    // metadata
    gr_nodes[0].member_role = "SECONDARY";
    gr_nodes[1].member_role = "PRIMARY";
  } else {
    metadata_nodes[0].role = "SECONDARY";
    metadata_nodes[1].role = "PRIMARY";
  }

  for (const auto [i, http_port] :
       stdx::views::enumerate(md_servers_http_ports)) {
    set_mock_metadata(http_port, "uuid", gr_nodes, i, metadata_nodes);
  }

  // check that the second metadata server (new PRIMARY) is queried for metadata
  EXPECT_TRUE(wait_for_transaction_count_increase(md_servers_http_ports[1], 2));

  // check that 1st and 3rd servers (new SECONDARIES) are NOT queried for
  // metadata in case of ReplicaSet Cluster every node gets queried for view_id
  // so this check would fail
  if (GetParam().cluster_type != mysqlrouter::ClusterType::RS_V2) {
    for (const auto i : {0, 2}) {
      EXPECT_FALSE(wait_for_transaction_count_increase(
          md_servers_http_ports[i], 1, std::chrono::milliseconds(500)));
    }
  }

  // check that the new PRIMARY is first in the state file
  check_state_file(state_file_, GetParam().cluster_type, "uuid",
                   {md_servers_classic_ports[1], md_servers_classic_ports[0],
                    md_servers_classic_ports[2]});
}

INSTANTIATE_TEST_SUITE_P(
    MetadataServersOrder, MetadataCacheMetadataServersOrder,
    ::testing::Values(
        ClusterMetadataTestParams("metadata_dynamic_nodes_v2_gr.js", "GR_V2",
                                  ClusterType::GR_V2),
        ClusterMetadataTestParams("metadata_dynamic_nodes_v2_ar.js", "AR",
                                  ClusterType::RS_V2)),
    get_test_description);

class MetadataCacheChangeClusterName
    : public RouterComponenClustertMetadataTest,
      public ::testing::WithParamInterface<ClusterMetadataTestParams> {};

TEST_P(MetadataCacheChangeClusterName, ChangeClusterName) {
  const size_t kClusterNodes{2};
  std::vector<ProcessWrapper *> cluster_nodes;
  std::vector<uint16_t> md_servers_classic_ports, md_servers_http_ports;

  const std::string kInitialClusterName = "initial_cluster_name";
  const std::string kChangedClusterName = "changed_cluster_name";

  // launch the mock servers
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

  auto set_metadata = [&](uint16_t http_port, unsigned int gr_pos,
                          const std::string &cluster_name) {
    auto globals = mock_GR_metadata_as_json(
        "uuid", classic_ports_to_gr_nodes(md_servers_classic_ports), gr_pos,
        classic_ports_to_cluster_nodes(md_servers_classic_ports));
    JsonAllocator allocator;
    globals.AddMember(
        "cluster_name",
        JsonValue(cluster_name.c_str(), cluster_name.length(), allocator),
        allocator);
    const auto globals_str = json_to_string(globals);
    MockServerRestClient(http_port).set_globals(globals_str);
  };

  // initially set the name of the cluster in the metadata to the same value
  // that was set in the Router configuration file
  for (const auto [i, http_port] :
       stdx::views::enumerate(md_servers_http_ports)) {
    set_metadata(http_port, i, kInitialClusterName);
  }

  // launch the router
  const std::string metadata_cache_section = get_metadata_cache_section(
      GetParam().cluster_type, "0.1", kInitialClusterName);
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

  // make sure that Router works
  make_new_connection_ok(router_rw_port, md_servers_classic_ports[0]);
  make_new_connection_ok(router_ro_port, md_servers_classic_ports[1]);

  // now change the cluster name in the metadata
  for (const auto [i, http_port] :
       stdx::views::enumerate(md_servers_http_ports)) {
    set_metadata(http_port, i, kChangedClusterName);
  }

  EXPECT_TRUE(
      wait_for_transaction_count_increase(md_servers_http_ports[0], 2, 5s));

  // the Router should still work
  make_new_connection_ok(router_rw_port, md_servers_classic_ports[0]);
  make_new_connection_ok(router_ro_port, md_servers_classic_ports[1]);

  // now stop the Router and start it again, this is to make sure that not only
  // change of the ClusterName while the Router is running works but also when
  // it is restarted and loads the configuration from scratch
  EXPECT_NO_THROW(router.kill());
  check_exit_code(router, EXIT_SUCCESS, 5s);

  /*auto &router2 = */ launch_router(metadata_cache_section,
                                     routing_rw_section + routing_ro_section,
                                     md_servers_classic_ports, EXIT_SUCCESS,
                                     /*wait_for_notify_ready=*/30s);

  make_new_connection_ok(router_rw_port, md_servers_classic_ports[0]);
  make_new_connection_ok(router_ro_port, md_servers_classic_ports[1]);
}

INSTANTIATE_TEST_SUITE_P(
    ChangeClusterName, MetadataCacheChangeClusterName,
    ::testing::Values(
        ClusterMetadataTestParams("metadata_dynamic_nodes_v2_gr.js", "GR_V2",
                                  ClusterType::GR_V2),
        ClusterMetadataTestParams("metadata_dynamic_nodes_v2_ar.js", "AR",
                                  ClusterType::RS_V2)),
    get_test_description);

struct SessionReuseTestParams {
  std::string router_ssl_mode;
  bool server_ssl_enabled;
  bool expected_session_reuse;
};

class SessionReuseTest
    : public RouterComponenClustertMetadataTest,
      public ::testing::WithParamInterface<SessionReuseTestParams> {};

/**
 * @test Checks that the SSL sessions to the server, that metadata cache is
 * creating to refresh metadata, are reused if SSL is used
 */
TEST_P(SessionReuseTest, SessionReuse) {
  std::vector<uint16_t> classic_ports, http_ports;
  std::vector<ProcessWrapper *> cluster_nodes;
  const auto test_params = GetParam();

  const size_t kClusterNodes = 2;
  for (size_t i = 0; i < kClusterNodes; ++i) {
    classic_ports.push_back(port_pool_.get_next_available());
    http_ports.push_back(port_pool_.get_next_available());
  }
  const std::string json_metadata =
      get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str();

  for (size_t i = 0; i < kClusterNodes; ++i) {
    cluster_nodes.push_back(&launch_mysql_server_mock(
        json_metadata, classic_ports[i], EXIT_SUCCESS, false, http_ports[i], 0,
        "", "127.0.0.1", 30s, /*enable_ssl*/ test_params.server_ssl_enabled));
    set_mock_metadata(http_ports[i], "uuid",
                      classic_ports_to_gr_nodes(classic_ports), 0,
                      classic_ports_to_cluster_nodes(classic_ports));
  }

  const auto router_rw_port = port_pool_.get_next_available();
  const std::string metadata_cache_section = get_metadata_cache_section(
      ClusterType::GR_V2, "0.2", "test", test_params.router_ssl_mode);
  const std::string routing_rw = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "first-available", "rw");

  launch_router(metadata_cache_section, routing_rw, classic_ports, EXIT_SUCCESS,
                /*wait_for_notify_ready=*/30s);

  // wait for several metadata cache refresh cycles
  EXPECT_TRUE(wait_for_transaction_count_increase(http_ports[0], 4));

  MySQLSession client;
  ASSERT_NO_FATAL_FAILURE(client.connect("127.0.0.1", classic_ports[0],
                                         "username", "password", "", ""));

  // check how many sessions were reused on the metadata server side
  std::unique_ptr<mysqlrouter::MySQLSession::ResultRow> result{
      client.query_one("SHOW STATUS LIKE 'Ssl_session_cache_hits'")};
  ASSERT_NE(nullptr, result.get());
  ASSERT_EQ(1u, result->size());
  const auto cache_hits = std::atoi((*result)[0]);
  if (test_params.expected_session_reuse) {
    EXPECT_GT(cache_hits, 0);
  } else {
    EXPECT_EQ(0, cache_hits);
  }
}

INSTANTIATE_TEST_SUITE_P(
    SessionReuse, SessionReuseTest,

    ::testing::Values(
        /* default ssl_mode in the Router ("PREFERRED"), ssl enabled on the
           server side so we expect session reuse */
        SessionReuseTestParams{
            /*router_ssl_mode*/ "",
            /*server_ssl_enabled*/ true,
            /*expected_session_reuse*/ true,
        },

        /* ssl_mode in the Router "REQUIRED", ssl enabled on the server side so
           we expect session reuse */
        SessionReuseTestParams{/*router_ssl_mode*/ "REQUIRED",
                               /*server_ssl_enabled*/ true,
                               /*expected_session_reuse*/ true},

        /* ssl_mode in the Router "PREFERRED", ssl disabled on the server side
         so we DON'T expect session reuse */
        SessionReuseTestParams{/*router_ssl_mode*/ "PREFERRED",
                               /*server_ssl_enabled*/ false,
                               /*expected_session_reuse*/ false},

        /* ssl_mode in the Router "DISABLED", ssl enabled on the server side
           so we DON'T expect session reuse */
        SessionReuseTestParams{/*router_ssl_mode*/ "DISABLED",
                               /*server_ssl_enabled*/ true,
                               /*expected_session_reuse*/ false}));

struct StatsUpdatesFrequencyParam {
  std::string test_name;
  std::string test_requirements;
  std::string test_description;

  std::string router_options_json;
  ClusterType cluster_type;
  MetadataSchemaVersion metadata_version;
  bool expect_updates;
  bool expect_parsing_error;
};

class StatsUpdatesFrequencyTest
    : public RouterComponenClustertMetadataTest,
      public ::testing::WithParamInterface<StatsUpdatesFrequencyParam> {
 protected:
  int get_int_global_value(const uint16_t http_port, const std::string &name) {
    const auto server_globals =
        MockServerRestClient(http_port).get_globals_as_json_string();

    return get_int_field_value(server_globals, name);
  }
};

/**
 * @test Verifies that router_options stats_updates_frequency field is
 * honoured as expected
 */
TEST_P(StatsUpdatesFrequencyTest, Verify) {
  uint16_t primary_node_http_port{};
  std::vector<uint16_t> metadata_server_ports;

  RecordProperty("Worklog", "15599");
  RecordProperty("RequirementId", GetParam().test_requirements);
  RecordProperty("Description", GetParam().test_description);

  if (GetParam().cluster_type == ClusterType::GR_CS) {
    ClusterSetOptions cs_options;
    cs_options.tracefile = "metadata_clusterset.js";
    cs_options.router_options = GetParam().router_options_json;
    create_clusterset(cs_options);

    primary_node_http_port = cs_options.topology.clusters[0].nodes[0].http_port;
    metadata_server_ports = cs_options.topology.get_md_servers_classic_ports();
  } else {
    const std::string tracefile =
        GetParam().cluster_type == ClusterType::GR_V2
            ? get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str()
            : get_data_dir().join("metadata_dynamic_nodes_v2_ar.js").str();

    const auto md_server_port = port_pool_.get_next_available();
    primary_node_http_port = port_pool_.get_next_available();
    metadata_server_ports.push_back(md_server_port);

    launch_mysql_server_mock(tracefile, md_server_port, EXIT_SUCCESS, false,
                             primary_node_http_port);

    set_mock_metadata(primary_node_http_port, "uuid",
                      classic_ports_to_gr_nodes({md_server_port}), 0,
                      classic_ports_to_cluster_nodes({md_server_port}), 0,
                      false, "127.0.0.1", GetParam().router_options_json,
                      GetParam().metadata_version);
  }

  SCOPED_TRACE("// Launch the Router");
  const auto router_rw_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(GetParam().cluster_type, "0.05");
  const std::string routing_rw = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "first-available", "rw");

  auto &router = launch_router(metadata_cache_section, routing_rw,
                               metadata_server_ports, EXIT_SUCCESS,
                               /*wait_for_notify_ready=*/30s);

  // the tests assume we run for about 2 seconds
  std::this_thread::sleep_for(2s);

  // initial update should always be done once
  const int attributes_upd_count =
      get_int_global_value(primary_node_http_port, "update_attributes_count");
  EXPECT_EQ(1, attributes_upd_count);

  const auto last_check_in_count = get_int_global_value(
      primary_node_http_port, "update_last_check_in_count");

  if (GetParam().expect_updates) {
    // last_check_in updates expected
    EXPECT_GT(last_check_in_count, 0);
  } else {
    // no last_check_in updates expected
    EXPECT_EQ(0, last_check_in_count);
  }

  const std::string log_content = router.get_logfile_content();
  const std::string error =
      "Error parsing stats_updates_frequency from the router.options";
  if (GetParam().expect_parsing_error) {
    EXPECT_TRUE(pattern_found(log_content, error));
  } else {
    EXPECT_FALSE(pattern_found(log_content, error));
  }
}

INSTANTIATE_TEST_SUITE_P(
    Spec, StatsUpdatesFrequencyTest,
    ::testing::Values(
        StatsUpdatesFrequencyParam{
            "clusterset_updates_frequency_0", "FR1.1,FR1.3",
            "router_options.stats_updates_frequency=0 - ClusterSet",
            /*router_options_json*/ R"({"stats_updates_frequency" : 0})",
            /*cluster_type*/ ClusterType::GR_CS,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ false,
            /*expect_parsing_error*/ false},
        // explicit 0 - InnoDBCluster
        StatsUpdatesFrequencyParam{
            "cluster_updates_frequency_0", "FR1.1,FR1.3",
            "router_options.stats_updates_frequency=0 - InnoDBCluster",
            /*router_options_json*/ R"({"stats_updates_frequency" : 0})",
            /*cluster_type*/ ClusterType::GR_V2,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ false,
            /*expect_parsing_error*/ false},
        // explicit 0 - ReplicaSet
        StatsUpdatesFrequencyParam{
            "replicaset_updates_frequency_0", "FR1.1,FR1.3",
            "router_options.stats_updates_frequency=0 - ReplicaSet",
            /*router_options_json*/ R"({"stats_updates_frequency" : 0})",
            /*cluster_type*/ ClusterType::RS_V2,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ false,
            /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{
            "clusterset_options_empty_json", "FR1.1,FR1.4.2",
            "stats_updates_frequency field not present in router_options JSON "
            "- ClusterSet - default is never do updates",
            /*router_options_json*/ "{}",
            /*cluster_type*/ ClusterType::GR_CS,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ false,
            /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{"cluster_options_empty_json",
                                   "FR1.1,FR1.4.1",
                                   "stats_updates_frequency field not present "
                                   "in router_options JSON - InnoDBCluster - "
                                   "default is do updates every 10th TTL",
                                   /*router_options_json*/ "{}",
                                   /*cluster_type*/ ClusterType::GR_V2,
                                   /*metadata_version*/ {2, 2, 0},
                                   /*expect_updates*/ true,
                                   /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{"replicaset_options_empty_json",
                                   "FR1.1,FR1.4.1",
                                   "stats_updates_frequency field not present "
                                   "in router_options JSON - ReplicaSet - "
                                   "default is do updates every 10th TTL",
                                   /*router_options_json*/ "{}",
                                   /*cluster_type*/ ClusterType::RS_V2,
                                   /*metadata_version*/ {2, 2, 0},
                                   /*expect_updates*/ true,
                                   /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{"clusterset_options_empty_string",
                                   "FR1.1,FR1.4.2",
                                   "router_options is empty string - "
                                   "ClusterSet - default is never do updates",
                                   /*router_options_json*/ "",
                                   /*cluster_type*/ ClusterType::GR_CS,
                                   /*metadata_version*/ {2, 2, 0},
                                   /*expect_updates*/ false,
                                   /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{
            "cluster_options_empty_string", "FR1.1,FR1.4.1",
            "router_options is empty string - InnoDBCluster - default is do "
            "updates every 10th TTL",
            /*router_options_json*/ "",
            /*cluster_type*/ ClusterType::GR_V2,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ true,
            /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{
            "replicaset_options_empty_string", "FR1.1,FR1.4.1",
            "router_options is empty string - ReplicaSet - default is do "
            "updates every 10th TTL",
            /*router_options_json*/ "",
            /*cluster_type*/ ClusterType::RS_V2,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ true,
            /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{
            "clusterset_updates_frequency_not_a_number",
            "FR1.1,FR1.4.2,FR1.4.3",
            "router_options.stats_updates_frequency is not a number - "
            "ClusterSet - default is never do updates",
            /*router_options_json*/ R"({"stats_updates_frequency" : "aaa"})",
            /*cluster_type*/ ClusterType::GR_CS,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ false,
            /*expect_parsing_error*/ true},
        StatsUpdatesFrequencyParam{
            "cluster_updates_frequency_not_a_number", "FR1.1,FR1.4.1,FR1.4.3",
            "router_options.stats_updates_frequency is not a number - "
            "InnoDBCluster - default is do updates every 10th TTL",
            /*router_options_json*/ R"({"stats_updates_frequency" : "aaa"})",
            /*cluster_type*/ ClusterType::GR_V2,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ true,
            /*expect_parsing_error*/ true},
        StatsUpdatesFrequencyParam{
            "replicaset_updates_frequency_negative_number",
            "FR1.1,FR1.4.1,FR1.4.3",
            "router_options.stats_updates_frequency is negative number - "
            "ReplicaSet - default is do updates every 10th TTL",
            /*router_options_json*/ R"({"stats_updates_frequency" : -1})",
            /*cluster_type*/ ClusterType::RS_V2,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ true,
            /*expect_parsing_error*/ true},
        StatsUpdatesFrequencyParam{
            "clusterset_updates_frequency_1s", "FR1.1,FR1.2",
            "router_options.stats_updates_frequency is 1s - we run for 2s+ so "
            "at least 1 update is expected",
            /*router_options_json*/ R"({"stats_updates_frequency" : 1})",
            /*cluster_type*/ ClusterType::GR_CS,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ true,
            /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{
            "cluster_updates_frequency_1s", "FR1.1,FR1.2",
            "router_options.stats_updates_frequency is 1s - we run for 2s+ so "
            "at least 1 update is expected",
            /*router_options_json*/ R"({"stats_updates_frequency" : 1})",
            /*cluster_type*/ ClusterType::GR_V2,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ true,
            /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{
            "replicaset_updates_frequency_1s", "FR1.1,FR1.2",
            "router_options.stats_updates_frequency is 1s - we run for 2s+ so "
            "at least 1 update is expected",
            /*router_options_json*/ R"({"stats_updates_frequency" : 1})",
            /*cluster_type*/ ClusterType::RS_V2,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ true,
            /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{
            "clusterset_updates_frequency_5s", "FR1.1,FR1.2",
            "router_options.stats_updates_frequency 5s - we run for 2s+ so no "
            "update is expected",
            /*router_options_json*/ R"({"stats_updates_frequency" : 5})",
            /*cluster_type*/ ClusterType::GR_CS,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ false,
            /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{
            "cluster_updates_frequency_5s", "FR1.1,FR1.2",
            "router_options.stats_updates_frequency 5s - we run for 2s+ so no "
            "update is expected",
            /*router_options_json*/ R"({"stats_updates_frequency" : 5})",
            /*cluster_type*/ ClusterType::GR_V2,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ false,
            /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{
            "replicaset_updates_frequency_5s", "FR1.1,FR1.2",
            "router_options.stats_updates_frequency 5s - we run for 2s+ so no "
            "update is expected",
            /*router_options_json*/ R"({"stats_updates_frequency" : 5})",
            /*cluster_type*/ ClusterType::RS_V2,
            /*metadata_version*/ {2, 2, 0},
            /*expect_updates*/ false,
            /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{"replicaset_options_invalid_json",
                                   "FR1.1,FR1.4.3",
                                   "ReplicaSet - router_options is not a valid "
                                   "JSON - default is update every 10TTL, "
                                   "parsing error should be logged",
                                   /*router_options_json*/ "aaabc",
                                   /*cluster_type*/ ClusterType::RS_V2,
                                   /*metadata_version*/ {2, 2, 0},
                                   /*expect_updates*/ true,
                                   /*expect_parsing_error*/ true},
        StatsUpdatesFrequencyParam{
            "clusterset_metadata_2_1_0_empty_options", "FR2",
            "ClusterSet, metadata vesion 2.1.0 (before v2_router_options view "
            "was added) - router_cs_options is empty - default is never update",
            /*router_options_json*/ "",
            /*cluster_type*/ ClusterType::GR_CS,
            /*metadata_version*/ {2, 1, 0},
            /*expect_updates*/ false,
            /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{
            "clusterset_metadata_2_1_0_updates_frequency_1s", "FR2",
            "ClusterSet, metadata vesion 2.1.0 (before v2_router_options view "
            "was added) - v2_router_cs_options has 1s configured so we "
            "fallback to it, updates expected",
            /*router_options_json*/ R"({"stats_updates_frequency" : 1})",
            /*cluster_type*/ ClusterType::GR_CS,
            /*metadata_version*/ {2, 1, 0},
            /*expect_updates*/ true,
            /*expect_parsing_error*/ false},
        StatsUpdatesFrequencyParam{
            "clusterset_metadata_2_1_0_updates_frequency_0s", "FR2",
            "Standalone Cluster, metadata vesion 2.1.0 (before "
            "v2_router_options view was added), even though "
            "v2_router_cs_options has '0' configured so we don't use it for "
            "standalone Cluster, we still expect updates every 10TTL",
            /*router_options_json*/ R"({"stats_updates_frequency" : 0})",
            /*cluster_type*/ ClusterType::GR_V2,
            /*metadata_version*/ {2, 1, 0},
            /*expect_updates*/ true,
            /*expect_parsing_error*/ false}),
    [](const ::testing::TestParamInfo<StatsUpdatesFrequencyParam> &info) {
      return info.param.test_name;
    });

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
