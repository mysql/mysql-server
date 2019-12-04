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

#include <chrono>
#include <thread>

#include <gmock/gmock.h>

#include "cluster_metadata.h"
#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"
#include "router_config.h"
#include "tcp_port_pool.h"

using mysqlrouter::ClusterType;
using mysqlrouter::MySQLSession;
using ::testing::PrintToString;
using namespace std::chrono_literals;
using namespace std::string_literals;

class MetadataChacheTTLTest : public RouterComponentTest {
 protected:
  std::string get_metadata_cache_section(
      std::vector<uint16_t> metadata_server_ports,
      ClusterType cluster_type = ClusterType::GR_V2,
      const std::string &ttl = "0.5") {
    std::string bootstrap_server_addresses;
    bool use_comma = false;
    for (const auto &port : metadata_server_ports) {
      if (use_comma) {
        bootstrap_server_addresses += ",";
      } else {
        use_comma = true;
      }
      bootstrap_server_addresses += "mysql://localhost:" + std::to_string(port);
    }
    const std::string cluster_type_str =
        (cluster_type == ClusterType::RS_V2) ? "rs" : "gr";

    return "[metadata_cache:test]\n"
           "cluster_type=" +
           cluster_type_str +
           "\n"
           "router_id=1\n"
           "bootstrap_server_addresses=" +
           bootstrap_server_addresses + "\n" +
           "user=mysql_router1_user\n"
           "connect_timeout=1\n"
           "metadata_cluster=test\n" +
           (ttl.empty() ? "" : std::string("ttl=" + ttl + "\n")) + "\n";
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

  int get_int_field_value(const std::string &json_string,
                          const std::string &field_name) {
    rapidjson::Document json_doc;
    json_doc.Parse(json_string.c_str());
    EXPECT_TRUE(json_doc.HasMember(field_name.c_str()));
    EXPECT_TRUE(json_doc[field_name.c_str()].IsInt());

    return json_doc[field_name.c_str()].GetInt();
  }

  std::string get_string_field_value(const std::string &json_string,
                                     const std::string &field_name) {
    rapidjson::Document json_doc;
    json_doc.Parse(json_string.c_str());
    EXPECT_TRUE(json_doc.HasMember(field_name.c_str()));
    EXPECT_TRUE(json_doc[field_name.c_str()].IsString());

    return json_doc[field_name.c_str()].GetString();
  }

  int get_ttl_queries_count(const std::string &json_string) {
    return get_int_field_value(json_string, "md_query_count");
  }

  int get_update_version_count(const std::string &json_string) {
    return get_int_field_value(json_string, "update_version_count");
  }

  int get_update_last_check_in_count(const std::string &json_string) {
    return get_int_field_value(json_string, "update_last_check_in_count");
  }

  bool wait_log_contains(const ProcessWrapper &router,
                         const std::string &needle,
                         std::chrono::milliseconds timeout) {
    if (getenv("WITH_VALGRIND")) {
      timeout *= 10;
    }

    const auto MSEC_STEP = 50ms;
    bool found = false;
    const auto started = std::chrono::steady_clock::now();
    do {
      const std::string log_content = router.get_full_logfile();
      found = (log_content.find(needle) != log_content.npos);
      if (!found) {
        auto step = std::min(timeout, MSEC_STEP);
        std::this_thread::sleep_for(std::chrono::milliseconds(step));
        timeout -= step;
      }
    } while (!found && timeout > std::chrono::steady_clock::now() - started);

    return found;
  }

  bool wait_for_refresh_thread_started(
      const ProcessWrapper &router, const std::chrono::milliseconds timeout) {
    const std::string needle = "Starting metadata cache refresh thread";

    return wait_log_contains(router, needle, timeout);
  }

  bool wait_metadata_read(const ProcessWrapper &router,
                          const std::chrono::milliseconds timeout) {
    const std::string needle = "Potential changes detected in cluster";

    return wait_log_contains(router, needle, timeout);
  }

  auto &launch_router(const std::string &temp_test_dir,
                      const std::string &conf_dir,
                      const std::string &metadata_cache_section,
                      const std::string &routing_section,
                      const int expected_exitcode,
                      bool wait_for_md_refresh_started = false) {
    auto default_section = get_DEFAULT_defaults();
    init_keyring(default_section, temp_test_dir);

    // enable debug logs for better diagnostics in case of failure
    std::string logger_section = "[logger]\nlevel = DEBUG\n";

    if (!wait_for_md_refresh_started) {
      default_section["logging_folder"] = "";
    }

    // launch the router
    const std::string conf_file = create_config_file(
        conf_dir, logger_section + metadata_cache_section + routing_section,
        &default_section);
    auto &router = ProcessManager::launch_router(
        {"-c", conf_file}, expected_exitcode, true, false);
    if (wait_for_md_refresh_started) {
      bool ready = wait_for_refresh_thread_started(router, 5000ms);
      EXPECT_TRUE(ready) << router.get_full_logfile();
    }

    return router;
  }

  TcpPortPool port_pool_;
};

struct MetadataTTLTestParams {
  // trace file
  std::string tracefile;
  // ttl value we want to set (floating point decimal in seconds)
  // additional info about the testcase that gets printed by the gtest in the
  // results
  std::string description;
  ClusterType cluster_type;
  std::string ttl;
  // how long do we run the router and count the metadata queries
  std::chrono::milliseconds router_uptime;
  // how many metadata queries we expect over this period
  int expected_md_queries_count;
  // if true expected_md_queries_count is only a minimal expected
  // value, we should not check for maximum
  bool at_least;

  MetadataTTLTestParams(std::string tracefile_, std::string description_,
                        ClusterType cluster_type_, std::string ttl_,
                        std::chrono::milliseconds router_uptime_ = 0ms,
                        int expected_md_queries_count_ = 0,
                        bool at_least_ = false)
      : tracefile(tracefile_),
        description(description_),
        cluster_type(cluster_type_),
        ttl(ttl_),
        router_uptime(router_uptime_),
        expected_md_queries_count(expected_md_queries_count_),
        at_least(at_least_) {}
};

auto get_test_description(
    const ::testing::TestParamInfo<MetadataTTLTestParams> &info) {
  return info.param.description;
}

std::ostream &operator<<(std::ostream &os, const MetadataTTLTestParams &param) {
  return os << "(" << param.ttl << ", " << param.router_uptime.count() << "ms, "
            << param.expected_md_queries_count << ", " << param.at_least << ")";
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

  // create and RAII-remove tmp dirs
  TempDirectory temp_test_dir;
  TempDirectory conf_dir("conf");

  SCOPED_TRACE(
      "// launch the server mock (it's our metadata server and single cluster "
      "node)");
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir().join(test_params.tracefile).str();

  auto &metadata_server = launch_mysql_server_mock(
      json_metadata, md_server_port, EXIT_SUCCESS, false, md_server_http_port);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(metadata_server, md_server_port));

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section = get_metadata_cache_section(
      {md_server_port}, test_params.cluster_type, test_params.ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router =
      launch_router(temp_test_dir.name(), conf_dir.name(),
                    metadata_cache_section, routing_section, EXIT_SUCCESS,
                    /*wait_for_md_refresh_started=*/true);

  // keep the router running to see how many times it queries for metadata
  std::this_thread::sleep_for(test_params.router_uptime);

  // let's ask the mock how many metadata queries it got after
  std::string server_globals =
      MockServerRestClient(md_server_http_port).get_globals_as_json_string();
  int ttl_count = get_ttl_queries_count(server_globals);

  if (!test_params.at_least) {
    // it is timing based test so to decrease random failures chances let's
    // take some error marigin, we kverify that number of metadata queries
    // falls into <expected_count-1, expected_count+1>
    EXPECT_THAT(ttl_count, IsBetween(test_params.expected_md_queries_count - 1,
                                     test_params.expected_md_queries_count + 1))
        << router.get_full_logfile();
  } else {
    // we only check that the TTL was queried at least N times
    EXPECT_GE(ttl_count, test_params.expected_md_queries_count);
  }

  ASSERT_THAT(router.kill(), testing::Eq(0));
}

INSTANTIATE_TEST_CASE_P(
    CheckTTLIsUsedCorrectly, MetadataChacheTTLTestParam,
    ::testing::Values(
        MetadataTTLTestParams("metadata_1_node_repeat_v2_gr.js", "0_gr_v2",
                              ClusterType::GR_V2, "0.4",
                              std::chrono::milliseconds(600), 2),
        MetadataTTLTestParams("metadata_1_node_repeat.js", "0_gr",
                              ClusterType::GR_V1, "0.4",
                              std::chrono::milliseconds(600), 2),
        MetadataTTLTestParams("metadata_1_node_repeat_v2_ar.js", "0_ar_v2",
                              ClusterType::RS_V2, "0.4",
                              std::chrono::milliseconds(600), 2),

        MetadataTTLTestParams("metadata_1_node_repeat_v2_gr.js", "1_gr_v2",
                              ClusterType::GR_V2, "1",
                              std::chrono::milliseconds(2500), 3),
        MetadataTTLTestParams("metadata_1_node_repeat.js", "1_gr",
                              ClusterType::GR_V1, "1",
                              std::chrono::milliseconds(2500), 3),
        MetadataTTLTestParams("metadata_1_node_repeat_v2_ar.js", "1_ar_v2",
                              ClusterType::RS_V2, "1",
                              std::chrono::milliseconds(2500), 3),

        // check that default is 0.5 if not provided:
        MetadataTTLTestParams("metadata_1_node_repeat_v2_gr.js", "2_gr_v2",
                              ClusterType::GR_V2, "",
                              std::chrono::milliseconds(1750), 4),
        MetadataTTLTestParams("metadata_1_node_repeat.js", "2_gr",
                              ClusterType::GR_V1, "",
                              std::chrono::milliseconds(1750), 4),
        MetadataTTLTestParams("metadata_1_node_repeat_v2_ar.js", "2_ar_v2",
                              ClusterType::RS_V2, "",
                              std::chrono::milliseconds(1750), 4),

        // check that for 0 there are multiple ttl queries (we can't really
        // guess how many there will be, but we should be able to safely assume
        // that in 1 second it shold be at least 5 queries)
        MetadataTTLTestParams("metadata_1_node_repeat_v2_gr.js", "3_gr_v2",
                              ClusterType::GR_V2, "0",
                              std::chrono::milliseconds(1000), 5,
                              /*at_least=*/true),
        MetadataTTLTestParams("metadata_1_node_repeat.js", "3_gr",
                              ClusterType::GR_V1, "0",
                              std::chrono::milliseconds(1000), 5,
                              /*at_least=*/true),
        MetadataTTLTestParams("metadata_1_node_repeat_v2_ar.js", "3_ar_v2",
                              ClusterType::RS_V2, "0",
                              std::chrono::milliseconds(1000), 5,
                              /*at_least=*/true)),
    get_test_description);

class MetadataChacheTTLTestParamInvalid
    : public MetadataChacheTTLTest,
      public ::testing::WithParamInterface<MetadataTTLTestParams> {};

TEST_P(MetadataChacheTTLTestParamInvalid, CheckTTLInvalid) {
  auto test_params = GetParam();

  // create and RAII-remove tmp dirs
  TempDirectory temp_test_dir;
  TempDirectory conf_dir("conf");

  // launch the server mock (it's our metadata server and single cluster node)
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir().join(GetParam().tracefile).str();

  auto &metadata_server = launch_mysql_server_mock(
      json_metadata, md_server_port, false, md_server_http_port);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(metadata_server, md_server_port));

  // launch the router with metadata-cache configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section = get_metadata_cache_section(
      {md_server_port}, test_params.cluster_type, test_params.ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router =
      launch_router(temp_test_dir.name(), conf_dir.name(),
                    metadata_cache_section, routing_section, EXIT_FAILURE,
                    /*wait_for_md_refresh_started=*/false);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_THAT(router.exit_code(), testing::Ne(0));
  EXPECT_TRUE(router.expect_output(
      "Configuration error: option ttl in [metadata_cache:test] needs value "
      "between 0 and 3600 inclusive"));
}

INSTANTIATE_TEST_CASE_P(
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

static size_t count_str_occurences(const std::string &s,
                                   const std::string &needle) {
  if (needle.length() == 0) return 0;
  size_t result = 0;
  for (size_t pos = s.find(needle); pos != std::string::npos;) {
    ++result;
    pos = s.find(needle, pos + needle.length());
  }
  return result;
}

class MetadataChacheTTLTestInstanceListUnordered
    : public MetadataChacheTTLTest,
      public ::testing::WithParamInterface<MetadataTTLTestParams> {};

/**
 * @test Checks that when for some reason the metadata server starts
 *       returning the information about the cluster nodes in different order we
 *       will not treat this as a change (Bug#29264764).
 */
TEST_P(MetadataChacheTTLTestInstanceListUnordered, InstancesListUnordered) {
  // create and RAII-remove tmp dirs
  TempDirectory temp_test_dir;
  TempDirectory conf_dir("conf");

  const std::string kGroupID = "";

  SCOPED_TRACE("// launch 2 server mocks");
  std::vector<ProcessWrapper *> nodes;
  std::vector<uint16_t> node_classic_ports;
  std::vector<uint16_t> node_http_ports;
  const std::string json_metadata =
      get_data_dir().join(GetParam().tracefile).str();
  for (size_t i = 0; i < 2; ++i) {
    node_classic_ports.push_back(port_pool_.get_next_available());
    node_http_ports.push_back(port_pool_.get_next_available());

    nodes.push_back(
        &launch_mysql_server_mock(json_metadata, node_classic_ports[i],
                                  EXIT_SUCCESS, false, node_http_ports[i]));
    ASSERT_NO_FATAL_FAILURE(check_port_ready(*nodes[i], node_classic_ports[i]));

    ASSERT_TRUE(
        MockServerRestClient(node_http_ports[i]).wait_for_rest_endpoint_ready())
        << nodes[i]->get_full_output();
  }

  for (size_t i = 0; i < 2; ++i) {
    set_mock_metadata(node_http_ports[i], kGroupID, node_classic_ports);
  }

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section = get_metadata_cache_section(
      node_classic_ports, GetParam().cluster_type, GetParam().ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router = launch_router(temp_test_dir.name(), conf_dir.name(),
                               metadata_cache_section, routing_section,
                               EXIT_SUCCESS, true);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  SCOPED_TRACE("// instruct the mocks to return nodes in reverse order");
  std::vector<uint16_t> node_classic_ports_reverse(node_classic_ports.rbegin(),
                                                   node_classic_ports.rend());
  for (size_t i = 0; i < 2; ++i) {
    set_mock_metadata(node_http_ports[i], kGroupID, node_classic_ports_reverse,
                      1);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  SCOPED_TRACE("// check it is not treated as a change");
  const std::string needle = "Potential changes detected in cluster";
  const std::string log_content = router.get_full_logfile();

  // 1 is expected, that comes from the inital reading of the metadata
  EXPECT_EQ(1, count_str_occurences(log_content, needle)) << log_content;
}

INSTANTIATE_TEST_CASE_P(
    InstancesListUnordered, MetadataChacheTTLTestInstanceListUnordered,
    ::testing::Values(
        MetadataTTLTestParams("metadata_dynamic_nodes_v2_gr.js",
                              "unordered_gr_v2", ClusterType::GR_V1, "0.1"),
        MetadataTTLTestParams("metadata_dynamic_nodes.js", "unordered_gr",
                              ClusterType::GR_V2, "0.1"),
        MetadataTTLTestParams("metadata_dynamic_nodes_v2_ar.js",
                              "unordered_ar_v2", ClusterType::RS_V2, "0.1")),
    get_test_description);

class MetadataChacheTTLTestInvalidMysqlXPort
    : public MetadataChacheTTLTest,
      public ::testing::WithParamInterface<MetadataTTLTestParams> {};

/**
 * @test Check that invalid mysqlx port in the metadata does not cause the node
 * to be discarded for the classic protocol connections (Bug#30617645)
 */
TEST_P(MetadataChacheTTLTestInvalidMysqlXPort, InvalidMysqlXPort) {
  TempDirectory temp_test_dir;
  TempDirectory conf_dir("conf");

  const std::string json_metadata =
      get_data_dir().join(GetParam().tracefile).str();

  SCOPED_TRACE("// single node cluster is fine for this test");
  const uint16_t node_classic_port{port_pool_.get_next_available()};
  const uint16_t node_http_port{port_pool_.get_next_available()};
  const uint32_t kInvalidPort{76000};

  auto &cluster_node = launch_mysql_server_mock(
      json_metadata, node_classic_port, EXIT_SUCCESS, false, node_http_port);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(cluster_node, node_classic_port));

  ASSERT_TRUE(
      MockServerRestClient(node_http_port).wait_for_rest_endpoint_ready())
      << cluster_node.get_full_output();

  SCOPED_TRACE(
      "// let the metadata for our single node report invalid mysqlx port");
  set_mock_metadata(node_http_port, "", {node_classic_port}, 0, 0, false,
                    "127.0.0.1", {kInvalidPort});

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section = get_metadata_cache_section(
      {node_classic_port}, GetParam().cluster_type, GetParam().ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router = launch_router(temp_test_dir.name(), conf_dir.name(),
                               metadata_cache_section, routing_section,
                               EXIT_SUCCESS, true);

  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port));
  ASSERT_TRUE(wait_metadata_read(router, 5000ms)) << router.get_full_output();

  SCOPED_TRACE(
      "// Even though the metadata contains invalid mysqlx port we still "
      "should be able to connect on the classic port");
  MySQLSession client;
  try {
    client.connect("127.0.0.1", router_port, "username", "password", "", "");
  } catch (...) {
    FAIL() << router.get_full_logfile();
  }
}

INSTANTIATE_TEST_CASE_P(
    InvalidMysqlXPort, MetadataChacheTTLTestInvalidMysqlXPort,
    ::testing::Values(MetadataTTLTestParams("metadata_dynamic_nodes_v2_gr.js",
                                            "gr_v2", ClusterType::GR_V1, "5"),
                      MetadataTTLTestParams("metadata_dynamic_nodes.js", "gr",
                                            ClusterType::GR_V2, "5"),
                      MetadataTTLTestParams("metadata_dynamic_nodes_v2_ar.js",
                                            "ar_v2", ClusterType::RS_V2, "5")),
    get_test_description);

/**
 * @test Checks that the router operates smoothly when the metadata version has
 * changed between the metadata refreshes.
 */
TEST_F(MetadataChacheTTLTest, CheckMetadataUpgradeBetweenTTLs) {
  TempDirectory temp_test_dir;
  TempDirectory conf_dir("conf");

  SCOPED_TRACE(
      "// launch the server mock (it's our metadata server and single cluster "
      "node)");
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir().join("metadata_1_node_repeat_metadatada_upgrade.js").str();

  auto &metadata_server = launch_mysql_server_mock(
      json_metadata, md_server_port, EXIT_SUCCESS, false, md_server_http_port);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(metadata_server, md_server_port));

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const auto router_port = port_pool_.get_next_available();

  const std::string metadata_cache_section =
      get_metadata_cache_section({md_server_port}, ClusterType::GR_V1, "0.5");
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router =
      launch_router(temp_test_dir.name(), conf_dir.name(),
                    metadata_cache_section, routing_section, EXIT_SUCCESS,
                    /*wait_for_md_refresh_started=*/true);

  // keep the router running for a while and change the metadata version
  std::this_thread::sleep_for(1s);

  MockServerRestClient(md_server_http_port)
      .set_globals("{\"new_metadata\" : 1}");

  // let the router run a bit more
  std::this_thread::sleep_for(1s);

  const std::string log_content = router.get_full_logfile();

  SCOPED_TRACE(
      "// check that the router really saw the version upgrade at some point");
  std::string needle =
      "Metadata version change was discovered. New metadata version is 2.0.0";
  EXPECT_GE(1, count_str_occurences(log_content, needle)) << log_content;

  SCOPED_TRACE(
      "// there should no be any cluster change reported caused by the version "
      "upgrade");
  needle = "Potential changes detected in cluster";
  // 1 is expected, that comes from the inital reading of the metadata
  EXPECT_EQ(1, count_str_occurences(log_content, needle)) << log_content;

  // router should exit noramlly
  ASSERT_THAT(router.kill(), testing::Eq(0));
}

class CheckRouterVersionUpdateOnceTest
    : public MetadataChacheTTLTest,
      public ::testing::WithParamInterface<MetadataTTLTestParams> {};

TEST_P(CheckRouterVersionUpdateOnceTest, CheckRouterVersionUpdateOnce) {
  TempDirectory temp_test_dir;
  TempDirectory conf_dir("conf");

  SCOPED_TRACE(
      "// launch the server mock (it's our metadata server and single cluster "
      "node)");
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir().join(GetParam().tracefile).str();

  auto &metadata_server = launch_mysql_server_mock(
      json_metadata, md_server_port, EXIT_SUCCESS, false, md_server_http_port);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(metadata_server, md_server_port));
  ASSERT_TRUE(
      MockServerRestClient(md_server_http_port).wait_for_rest_endpoint_ready())
      << metadata_server.get_full_output();

  SCOPED_TRACE(
      "// let's tell the mock which version it should expect so that it does "
      "the strict sql matching for us");
  auto globals = mock_GR_metadata_as_json("", {md_server_port});
  JsonAllocator allocator;
  globals.AddMember("router_version", MYSQL_ROUTER_VERSION, allocator);
  const auto globals_str = json_to_string(globals);
  MockServerRestClient(md_server_http_port).set_globals(globals_str);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const auto router_port = port_pool_.get_next_available();

  const std::string metadata_cache_section = get_metadata_cache_section(
      {md_server_port}, GetParam().cluster_type, GetParam().ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router =
      launch_router(temp_test_dir.name(), conf_dir.name(),
                    metadata_cache_section, routing_section, EXIT_SUCCESS,
                    /*wait_for_md_refresh_started=*/true);

  SCOPED_TRACE("// let the router run for about 10 ttl periods");
  std::this_thread::sleep_for(1s);

  SCOPED_TRACE("// we still expect the version to be only set once");
  std::string server_globals =
      MockServerRestClient(md_server_http_port).get_globals_as_json_string();
  const int version_upd_count = get_update_version_count(server_globals);
  EXPECT_EQ(1, version_upd_count) << router.get_full_logfile();

  SCOPED_TRACE(
      "// Let's check if the first query is starting a trasaction and the "
      "second checking the version");
  const std::string &first_sql =
      get_string_field_value(server_globals, "first_query");
  const std::string &second_sql =
      get_string_field_value(server_globals, "second_query");
  EXPECT_STREQ("START TRANSACTION", first_sql.c_str());
  EXPECT_STREQ("SELECT * FROM mysql_innodb_cluster_metadata.schema_version",
               second_sql.c_str());

  if (GetParam().cluster_type != ClusterType::GR_V1) {
    SCOPED_TRACE("// last_check_in should be attempted at least once");
    std::string server_globals =
        MockServerRestClient(md_server_http_port).get_globals_as_json_string();
    const int last_check_in_upd_count =
        get_update_last_check_in_count(server_globals);
    EXPECT_GE(1, last_check_in_upd_count) << router.get_full_logfile();
  }
}

INSTANTIATE_TEST_CASE_P(
    CheckRouterVersionUpdateOnce, CheckRouterVersionUpdateOnceTest,
    ::testing::Values(
        MetadataTTLTestParams("metadata_dynamic_nodes_version_update.js",
                              "router_version_update_once_gr_v1",
                              ClusterType::GR_V1, "0.1"),
        MetadataTTLTestParams("metadata_dynamic_nodes_version_update_v2_gr.js",
                              "router_version_update_once_gr_v2",
                              ClusterType::GR_V2, "0.1"),
        MetadataTTLTestParams("metadata_dynamic_nodes_version_update_v2_ar.js",
                              "router_version_update_once_ar_v2",
                              ClusterType::RS_V2, "0.1")),
    get_test_description);

class PermissionErrorOnVersionUpdateTest
    : public MetadataChacheTTLTest,
      public ::testing::WithParamInterface<MetadataTTLTestParams> {};

TEST_P(PermissionErrorOnVersionUpdateTest, PermissionErrorOnVersionUpdate) {
  TempDirectory temp_test_dir;
  TempDirectory conf_dir("conf");

  SCOPED_TRACE(
      "// launch the server mock (it's our metadata server and single cluster "
      "node)");
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir().join(GetParam().tracefile).str();

  auto &metadata_server = launch_mysql_server_mock(
      json_metadata, md_server_port, EXIT_SUCCESS, false, md_server_http_port);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(metadata_server, md_server_port));
  ASSERT_TRUE(
      MockServerRestClient(md_server_http_port).wait_for_rest_endpoint_ready())
      << metadata_server.get_full_output();

  SCOPED_TRACE(
      "// let's tell the mock which version it should expect so that it does "
      "the strict sql matching for us, also tell it to issue the permission "
      "error on the update attempt");
  auto globals = mock_GR_metadata_as_json("", {md_server_port});
  JsonAllocator allocator;
  globals.AddMember("router_version", MYSQL_ROUTER_VERSION, allocator);
  globals.AddMember("perm_error_on_version_update", 1, allocator);
  const auto globals_str = json_to_string(globals);
  MockServerRestClient(md_server_http_port).set_globals(globals_str);

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const auto router_port = port_pool_.get_next_available();

  const std::string metadata_cache_section = get_metadata_cache_section(
      {md_server_port}, GetParam().cluster_type, GetParam().ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router =
      launch_router(temp_test_dir.name(), conf_dir.name(),
                    metadata_cache_section, routing_section, EXIT_SUCCESS,
                    /*wait_for_md_refresh_started=*/true);

  SCOPED_TRACE("// let the router run for about 10 ttl periods");
  std::this_thread::sleep_for(1s);

  SCOPED_TRACE(
      "// we expect the error trying to update the version in the log");
  const std::string log_content = router.get_full_logfile();
  const std::string pattern =
      "Updating the router version in metadata failed:.*\n"
      "Make sure to follow the correct steps to upgrade your metadata.\n"
      "Run the dba.upgradeMetadata\\(\\) then launch the new Router version "
      "when prompted";
  ASSERT_TRUE(pattern_found(log_content, pattern)) << log_content;

  SCOPED_TRACE(
      "// we expect that the router attempted to update the version only once, "
      "even tho it failed");
  std::string server_globals =
      MockServerRestClient(md_server_http_port).get_globals_as_json_string();
  const int version_upd_count = get_update_version_count(server_globals);
  EXPECT_EQ(1, version_upd_count) << router.get_full_logfile();

  SCOPED_TRACE(
      "// It should still not be fatal, the router should accept the "
      "connections to the cluster");
  MySQLSession client;
  ASSERT_NO_FATAL_FAILURE(
      client.connect("127.0.0.1", router_port, "username", "password", "", ""));
}

INSTANTIATE_TEST_CASE_P(
    PermissionErrorOnVersionUpdate, PermissionErrorOnVersionUpdateTest,
    ::testing::Values(
        MetadataTTLTestParams("metadata_dynamic_nodes_version_update.js",
                              "router_version_update_fail_on_perm_gr_v1",
                              ClusterType::GR_V1, "0.1"),
        MetadataTTLTestParams("metadata_dynamic_nodes_version_update_v2_gr.js",
                              "router_version_update_fail_on_perm_gr_v2",
                              ClusterType::GR_V2, "0.1"),
        MetadataTTLTestParams("metadata_dynamic_nodes_version_update_v2_ar.js",
                              "router_version_update_fail_on_perm_ar_v2",
                              ClusterType::RS_V2, "0.1")),
    get_test_description);

class UpgradeInProgressTest
    : public MetadataChacheTTLTest,
      public ::testing::WithParamInterface<MetadataTTLTestParams> {};

TEST_P(UpgradeInProgressTest, UpgradeInProgress) {
  TempDirectory temp_test_dir;
  TempDirectory conf_dir("conf");

  SCOPED_TRACE(
      "// launch the server mock (it's our metadata server and single cluster "
      "node)");
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir().join(GetParam().tracefile).str();

  auto &metadata_server = launch_mysql_server_mock(
      json_metadata, md_server_port, EXIT_SUCCESS, false, md_server_http_port);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(metadata_server, md_server_port));
  ASSERT_TRUE(
      MockServerRestClient(md_server_http_port).wait_for_rest_endpoint_ready())
      << metadata_server.get_full_output();
  set_mock_metadata(md_server_http_port, "", {md_server_port});

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const auto router_port = port_pool_.get_next_available();

  const std::string metadata_cache_section = get_metadata_cache_section(
      {md_server_port}, GetParam().cluster_type, GetParam().ttl);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");
  auto &router =
      launch_router(temp_test_dir.name(), conf_dir.name(),
                    metadata_cache_section, routing_section, EXIT_SUCCESS,
                    /*wait_for_md_refresh_started=*/true);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port));

  SCOPED_TRACE("// let us make some user connection via the router port");
  MySQLSession client;
  std::this_thread::sleep_for(500ms);
  ASSERT_NO_FATAL_FAILURE(
      client.connect("127.0.0.1", router_port, "username", "password", "", ""))
      << router.get_full_logfile();

  SCOPED_TRACE("// let's mimmic start of the metadata update now");
  auto globals = mock_GR_metadata_as_json("", {md_server_port});
  JsonAllocator allocator;
  globals.AddMember("upgrade_in_progress", 1, allocator);
  globals.AddMember("md_query_count", 0, allocator);
  const auto globals_str = json_to_string(globals);
  MockServerRestClient(md_server_http_port).set_globals(globals_str);

  SCOPED_TRACE(
      "// Wait some more and read the metadata update count once more to avoid "
      "race condition.");
  std::this_thread::sleep_for(500ms);
  MockServerRestClient(md_server_http_port).get_globals_as_json_string();
  std::string server_globals =
      MockServerRestClient(md_server_http_port).get_globals_as_json_string();
  int metadata_upd_count = get_ttl_queries_count(server_globals);

  SCOPED_TRACE(
      "// Now wait another 5 ttl periods, since the metadata update is in "
      "progress we do not expect the increased number of metadata queries "
      "after that period");
  std::this_thread::sleep_for(500ms);
  server_globals =
      MockServerRestClient(md_server_http_port).get_globals_as_json_string();
  const int metadata_upd_count2 = get_ttl_queries_count(server_globals);
  EXPECT_EQ(metadata_upd_count, metadata_upd_count2)
      << router.get_full_logfile();

  SCOPED_TRACE(
      "// Even though the upgrade is in progress the existing connection "
      "should still be active.");
  auto result{client.query_one("select @@port")};
  EXPECT_EQ(static_cast<uint16_t>(std::stoul(std::string((*result)[0]))),
            md_server_port);

  SCOPED_TRACE("// Also we should be able to create a new conenction.");
  MySQLSession client2;
  ASSERT_NO_FATAL_FAILURE(client2.connect("127.0.0.1", router_port, "username",
                                          "password", "", ""));

  SCOPED_TRACE("// Info about the update should be logged.");
  const std::string log_content = router.get_full_logfile();
  ASSERT_TRUE(log_content.find("Cluster metadata upgrade in progress, aborting "
                               "the metada refresh") != std::string::npos);
}

INSTANTIATE_TEST_CASE_P(
    UpgradeInProgress, UpgradeInProgressTest,
    ::testing::Values(
        MetadataTTLTestParams("metadata_dynamic_nodes_version_update.js",
                              "metadata_upgrade_in_progress_gr_v1",
                              ClusterType::GR_V1, "0.1"),
        MetadataTTLTestParams("metadata_dynamic_nodes_version_update_v2_gr.js",
                              "metadata_upgrade_in_progress_gr_v2",
                              ClusterType::GR_V2, "0.1"),
        MetadataTTLTestParams("metadata_dynamic_nodes_version_update_v2_ar.js",
                              "metadata_upgrade_in_progress_ar_v2",
                              ClusterType::RS_V2, "0.1")),
    get_test_description);

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
