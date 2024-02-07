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

class MetadataChacheTTLTest : public RouterComponentMetadataTest {};

struct MetadataTTLTestParams {
  // mock_server trace file
  std::string tracefile;
  // additional info about the testcase that gets printed by the gtest in the
  // results
  std::string description;
  // the type of the cluster GR or AR
  ClusterType cluster_type;
  // ttl value we want to set (floating point decimal in seconds)
  std::string ttl;
  // what is the minimal expected period between the updates
  std::chrono::milliseconds ttl_expected_min;
  // what is the maximal expected period between the updates
  std::chrono::milliseconds ttl_expected_max;

  MetadataTTLTestParams(std::string tracefile_, std::string description_,
                        ClusterType cluster_type_, std::string ttl_ = "0.5",
                        std::chrono::milliseconds ttl_expected_min_ = 0ms,
                        std::chrono::milliseconds ttl_expected_max_ = 0ms)
      : tracefile(std::move(tracefile_)),
        description(std::move(description_)),
        cluster_type(cluster_type_),
        ttl(std::move(ttl_)),
        ttl_expected_min(ttl_expected_min_),
        ttl_expected_max(ttl_expected_max_) {}
};

auto get_test_description(
    const ::testing::TestParamInfo<MetadataTTLTestParams> &info) {
  return info.param.description;
}

std::ostream &operator<<(std::ostream &os, const MetadataTTLTestParams &param) {
  return os << "(" << param.ttl << "s not in the range ["
            << param.ttl_expected_min.count() << "ms,"
            << param.ttl_expected_max.count() << "ms])";
}

namespace std {

template <class T, class R>
std::ostream &operator<<(std::ostream &os,
                         const std::chrono::duration<T, R> &duration) {
  return os << std::chrono::duration_cast<std::chrono::milliseconds>(duration)
                   .count()
            << "ms";
}

}  // namespace std

/**
 * @test Checks that the quarantine works as expected with metadata-cache
 * updates
 */
TEST_F(MetadataChacheTTLTest, Quarantine) {
  std::vector<uint16_t> classic_ports, http_ports;
  std::vector<ProcessWrapper *> cluster_nodes;

  const size_t kClusterNodes = 2;
  for (size_t i = 0; i < kClusterNodes; ++i) {
    classic_ports.push_back(port_pool_.get_next_available());
    http_ports.push_back(port_pool_.get_next_available());
  }
  const std::string json_metadata =
      get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str();

  for (size_t i = 0; i < kClusterNodes; ++i) {
    cluster_nodes.push_back(&launch_mysql_server_mock(
        json_metadata, classic_ports[i], EXIT_SUCCESS, false, http_ports[i]));
    set_mock_metadata(http_ports[i], "uuid",
                      classic_ports_to_gr_nodes(classic_ports), 0,
                      classic_ports_to_cluster_nodes(classic_ports));
  }

  const auto router_ro_port = port_pool_.get_next_available();
  const auto router_rw_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(ClusterType::GR_V2, "0.2");
  const std::string routing_rw = get_metadata_cache_routing_section(
      router_rw_port, "PRIMARY", "first-available", "rw");
  const std::string routing_ro = get_metadata_cache_routing_section(
      router_ro_port, "SECONDARY", "round-robin", "ro");

  auto &router = launch_router(metadata_cache_section, routing_rw + routing_ro,
                               classic_ports, EXIT_SUCCESS,
                               /*wait_for_notify_ready=*/30s);
  EXPECT_TRUE(wait_for_transaction_count_increase(http_ports[0], 2));
  make_new_connection_ok(router_ro_port, classic_ports[1]);

  SCOPED_TRACE(
      "// kill the cluster RO node and wait for it to be added to quarantine");
  EXPECT_NO_THROW(cluster_nodes[1]->kill());
  check_exit_code(*cluster_nodes[1], EXIT_SUCCESS, 5s);

  SCOPED_TRACE("// connect and trigger a quarantine");
  verify_new_connection_fails(router_ro_port);
  EXPECT_TRUE(wait_log_contains(
      router,
      "add destination '127.0.0.1:" + std::to_string(classic_ports[1]) +
          "' to quarantine",
      1s));

  SCOPED_TRACE("// bring back the cluster node");
  cluster_nodes[1] = &launch_mysql_server_mock(
      json_metadata, classic_ports[1], EXIT_SUCCESS, false, http_ports[1]);
  set_mock_metadata(http_ports[1], "uuid",
                    classic_ports_to_gr_nodes(classic_ports), 0,
                    classic_ports_to_cluster_nodes(classic_ports));

  SCOPED_TRACE("// .. and wait for it to be cleared by the quarantine");
  EXPECT_TRUE(wait_log_contains(
      router,
      "Destination candidate '127.0.0.1:" + std::to_string(classic_ports[1]) +
          "' is available, remove it from quarantine",
      10s));
}

class MetadataChacheTTLTestParam
    : public MetadataChacheTTLTest,
      public ::testing::WithParamInterface<MetadataTTLTestParams> {};

MATCHER_P2(IsBetween, a, b,
           std::string(negation ? "isn't" : "is") + " between " +
               PrintToString(a) + " and " + PrintToString(b)) {
  return a <= arg && arg <= b;
}

TEST_P(MetadataChacheTTLTestParam, CheckTTLValid) {
  auto test_params = GetParam();

  SCOPED_TRACE(
      "// launch the server mock (it's our metadata server and single cluster "
      "node)");
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir().join(test_params.tracefile).str();

  /*auto &metadata_server = */ launch_mysql_server_mock(
      json_metadata, md_server_port, EXIT_SUCCESS, false, md_server_http_port);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(test_params.cluster_type, test_params.ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router = launch_router(metadata_cache_section, routing_section,
                               {md_server_port}, EXIT_SUCCESS,
                               /*wait_for_notify_ready=*/30s);

  // the remaining is too time-dependent to hope it will pass with VALGRIND
  if (getenv("WITH_VALGRIND")) {
    return;
  }

  SCOPED_TRACE("// Wait for the initial metadata refresh to end");
  const auto first_refresh_stop_timestamp =
      get_log_timestamp(router.get_logfile_path(),
                        ".*Finished refreshing the cluster metadata.*", 1, 2s);
  if (!first_refresh_stop_timestamp) {
    FAIL() << "Did not find first metadata refresh end log in the logfile.\n"
           << router.get_logfile_content();
  }

  SCOPED_TRACE("// Wait for the second metadata refresh to start");
  const auto second_refresh_start_timestamp = get_log_timestamp(
      router.get_logfile_path(), ".*Started refreshing the cluster metadata.*",
      2, test_params.ttl_expected_max + 1s);
  if (!second_refresh_start_timestamp) {
    FAIL() << "Did not find second metadata refresh start log in the logfile.\n"
           << router.get_logfile_content();
  }

  SCOPED_TRACE(
      "// Check if the time passed in between falls into expected range");
  const auto ttl = second_refresh_start_timestamp.value() -
                   first_refresh_stop_timestamp.value();

  // The upper bound can't be tested reliably in PB2 environment
  // EXPECT_THAT(ttl, IsBetween(test_params.ttl_expected_min,
  //                            test_params.ttl_expected_max));

  EXPECT_GE(ttl, test_params.ttl_expected_min);
}

INSTANTIATE_TEST_SUITE_P(
    CheckTTLIsUsedCorrectly, MetadataChacheTTLTestParam,
    ::testing::Values(
        MetadataTTLTestParams("metadata_1_node_repeat_v2_gr.js", "0_gr_v2",
                              ClusterType::GR_V2, "0.2", 150ms, 490ms),
        MetadataTTLTestParams("metadata_1_node_repeat_v2_ar.js", "0_ar_v2",
                              ClusterType::RS_V2, "0.2", 150ms, 490ms),

        MetadataTTLTestParams("metadata_1_node_repeat_v2_gr.js", "1_gr_v2",
                              ClusterType::GR_V2, "1", 700ms, 1800ms),
        MetadataTTLTestParams("metadata_1_node_repeat_v2_ar.js", "1_ar_v2",
                              ClusterType::RS_V2, "1", 700ms, 1800ms),

        // check that default is 0.5 if not provided:
        MetadataTTLTestParams("metadata_1_node_repeat_v2_gr.js", "2_gr_v2",
                              ClusterType::GR_V2, "", 450ms, 900ms),
        MetadataTTLTestParams("metadata_1_node_repeat_v2_ar.js", "2_ar_v2",
                              ClusterType::RS_V2, "", 450ms, 900ms),

        // check that for 0 the delay between the refresh is very short
        MetadataTTLTestParams("metadata_1_node_repeat_v2_gr.js", "3_gr_v2",
                              ClusterType::GR_V2, "0", 0ms, 450ms),
        MetadataTTLTestParams("metadata_1_node_repeat_v2_ar.js", "3_ar_v2",
                              ClusterType::RS_V2, "0", 0ms, 450ms)),
    get_test_description);

class MetadataChacheTTLTestParamInvalid
    : public MetadataChacheTTLTest,
      public ::testing::WithParamInterface<MetadataTTLTestParams> {};

TEST_P(MetadataChacheTTLTestParamInvalid, CheckTTLInvalid) {
  auto test_params = GetParam();

  // launch the server mock (it's our metadata server and single cluster node)
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir().join(GetParam().tracefile).str();

  /*auto &metadata_server =*/launch_mysql_server_mock(
      json_metadata, md_server_port, false, md_server_http_port);

  // launch the router with metadata-cache configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(test_params.cluster_type, test_params.ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router = launch_router(metadata_cache_section, routing_section,
                               {md_server_port}, EXIT_FAILURE,
                               /*wait_for_notify_ready=*/-1s);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_THAT(router.exit_code(), testing::Ne(0));
  EXPECT_TRUE(wait_log_contains(router,
                                "Configuration error: option ttl in "
                                "\\[metadata_cache:bootstrap\\] needs value "
                                "between 0 and 3600 inclusive",
                                500ms));
}

INSTANTIATE_TEST_SUITE_P(
    CheckInvalidTTLRefusesStart, MetadataChacheTTLTestParamInvalid,
    ::testing::Values(
        MetadataTTLTestParams("metadata_1_node_repeat_gr_v2.js", "0_all",
                              ClusterType::GR_V2, "-0.001"),
        MetadataTTLTestParams("metadata_1_node_repeat_gr_v2.js", "1_all",
                              ClusterType::GR_V2, "3600.001"),
        MetadataTTLTestParams("metadata_1_node_repeat_gr_v2.js", "2_all",
                              ClusterType::GR_V2, "INVALID"),
        MetadataTTLTestParams("metadata_1_node_repeat_gr_v2.js", "3_all",
                              ClusterType::GR_V2, "1,1")),
    get_test_description);

/**
 * @test Checks that the router operates smoothly when the metadata version has
 * changed between the metadata refreshes from usupported to the supported one.
 */
TEST_F(MetadataChacheTTLTest, CheckMetadataUpgradeBetweenTTLs) {
  RecordProperty("Worklog", "15868");
  RecordProperty("RequirementId", "FR1");
  RecordProperty("Requirement",
                 "When MySQLRouter connects to the Cluster with metadata "
                 "version 1.x, it MUST disable the routing and log an error");
  RecordProperty(
      "Description",
      "Testing that the Router will enable the Routing when the metadata is "
      "upgraded from unsupported to supported version while it is running.");

  SCOPED_TRACE(
      "// launch the server mock (it's our metadata server and single cluster "
      "node)");
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir().join("metadata_1_node_repeat_metadatada_upgrade.js").str();

  /*auto &metadata_server = */ launch_mysql_server_mock(
      json_metadata, md_server_port, EXIT_SUCCESS, false, md_server_http_port);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const auto router_port = port_pool_.get_next_available();

  const std::string metadata_cache_section =
      get_metadata_cache_section(ClusterType::GR_V2, "0.5");
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router = launch_router(metadata_cache_section, routing_section,
                               {md_server_port}, EXIT_SUCCESS,
                               /*wait_for_notify_ready=*/-1s);

  // keep the router running for a while and change the metadata version
  EXPECT_TRUE(wait_for_transaction_count_increase(md_server_http_port, 2));

  EXPECT_TRUE(wait_log_contains(
      router,
      "The target Cluster's Metadata version \\('1\\.0\\.2'\\) is not "
      "supported. Please use the latest MySQL Shell to upgrade it using "
      "'dba\\.upgradeMetadata\\(\\)'\\. Expected metadata version compatible "
      "with '2\\.0\\.0'",
      1s));

  MockServerRestClient(md_server_http_port)
      .set_globals("{\"new_metadata\" : 1}");

  // let the router run a bit more
  EXPECT_TRUE(wait_for_transaction_count_increase(md_server_http_port, 4));

  SCOPED_TRACE(
      "// there should be no cluster change reported caused by the version "
      "upgrade");
  const std::string log_content = router.get_logfile_content();
  const std::string needle = "Potential changes detected in cluster";
  // 1 is expected, that comes from the initial reading of the metadata
  EXPECT_EQ(1, count_str_occurences(log_content, needle));

  // the Router should start handling connections
  make_new_connection_ok(router_port, md_server_port);

  // router should exit noramlly
  ASSERT_THAT(router.kill(), testing::Eq(0));
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
