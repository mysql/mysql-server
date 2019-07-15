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

#include "gmock/gmock.h"
#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"

#include <chrono>
#include <thread>

using mysqlrouter::MySQLSession;
using ::testing::PrintToString;
using namespace std::chrono_literals;

class MetadataChacheTTLTest : public RouterComponentTest {
 protected:
  std::string get_metadata_cache_section(
      std::vector<uint16_t> metadata_server_ports,
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
    return "[metadata_cache:test]\n"
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

  int get_ttl_queries_count(const std::string &json_string) {
    rapidjson::Document json_doc;
    json_doc.Parse(json_string.c_str());
    EXPECT_TRUE(json_doc.HasMember("md_query_count"));
    EXPECT_TRUE(json_doc["md_query_count"].IsInt());

    return json_doc["md_query_count"].GetInt();
  }

  bool wait_for_refresh_thread_started(const ProcessWrapper &router,
                                       std::chrono::milliseconds timeout) {
    if (getenv("WITH_VALGRIND")) {
      timeout *= 10;
    }

    const auto MSEC_STEP = 10ms;
    bool thread_started = false;
    const auto started = std::chrono::steady_clock::now();
    do {
      const std::string log_content = router.get_full_logfile();
      const std::string needle = "Starting metadata cache refresh thread";
      thread_started = (log_content.find(needle) != log_content.npos);
      if (!thread_started) {
        auto step = std::min(timeout, MSEC_STEP);
        std::this_thread::sleep_for(std::chrono::milliseconds(step));
        timeout -= step;
      }
    } while (!thread_started &&
             timeout > std::chrono::steady_clock::now() - started);

    return thread_started;
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
  //  ttl value we want to set (floating point decimal in seconds)
  std::string ttl;
  // how long do we run the router and count the metadata queries
  std::chrono::milliseconds router_uptime;
  // how many metadata queries we expect over this period
  int expected_md_queries_count;
  // if true expected_md_queries_count is only a minimal expected
  // value, we should not check for maximum
  bool at_least;

  MetadataTTLTestParams(std::string ttl_,
                        std::chrono::milliseconds router_uptime_ = 0ms,
                        int expected_md_queries_count_ = 0,
                        bool at_least_ = false)
      : ttl(ttl_),
        router_uptime(router_uptime_),
        expected_md_queries_count(expected_md_queries_count_),
        at_least(at_least_) {}
};

std::ostream &operator<<(std::ostream &os, const MetadataTTLTestParams &param) {
  return os << "(" << param.ttl << ", " << param.router_uptime.count() << "ms, "
            << param.expected_md_queries_count << ", " << param.at_least << ")";
}

class MetadataChacheTTLTestParam
    : public MetadataChacheTTLTest,
      public ::testing::WithParamInterface<MetadataTTLTestParams> {
 protected:
  virtual void SetUp() { MetadataChacheTTLTest::SetUp(); }
};

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
      get_data_dir().join("metadata_1_node_repeat.js").str();

  auto &metadata_server = launch_mysql_server_mock(
      json_metadata, md_server_port, EXIT_SUCCESS, false, md_server_http_port);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(metadata_server, md_server_port));

  SCOPED_TRACE("// launch the router with metadata-cache configuration");
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section({md_server_port}, test_params.ttl);
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
        << router.get_full_output();
  } else {
    // we only check that the TTL was queried at least N times
    EXPECT_GE(ttl_count, test_params.expected_md_queries_count);
  }

  ASSERT_THAT(router.kill(), testing::Eq(0));
}

INSTANTIATE_TEST_CASE_P(
    CheckTTLIsUsedCorrectly, MetadataChacheTTLTestParam,
    ::testing::Values(
        MetadataTTLTestParams("0.4", std::chrono::milliseconds(600), 2),
        MetadataTTLTestParams("1", std::chrono::milliseconds(2500), 3),
        // check that default is 0.5 if not provided:
        MetadataTTLTestParams("", std::chrono::milliseconds(1750), 4),
        // check that for 0 there are multiple ttl queries (we can't really
        // guess how many there will be, but we should be able to safely assume
        // that in 1 second it shold be at least 5 queries)
        MetadataTTLTestParams("0", std::chrono::milliseconds(1000), 5,
                              /*at_least=*/true)));

class MetadataChacheTTLTestParamInvalid
    : public MetadataChacheTTLTest,
      public ::testing::WithParamInterface<MetadataTTLTestParams> {
 protected:
  virtual void SetUp() { MetadataChacheTTLTest::SetUp(); }
};

TEST_P(MetadataChacheTTLTestParamInvalid, CheckTTLInvalid) {
  auto test_params = GetParam();

  // create and RAII-remove tmp dirs
  TempDirectory temp_test_dir;
  TempDirectory conf_dir("conf");

  // launch the server mock (it's our metadata server and single cluster node)
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir().join("metadata_1_node_repeat.js").str();

  auto &metadata_server = launch_mysql_server_mock(
      json_metadata, md_server_port, false, md_server_http_port);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(metadata_server, md_server_port));

  // launch the router with metadata-cache configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section({md_server_port}, test_params.ttl);
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

INSTANTIATE_TEST_CASE_P(CheckInvalidTTLRefusesStart,
                        MetadataChacheTTLTestParamInvalid,
                        ::testing::Values(MetadataTTLTestParams("-0.001"),
                                          MetadataTTLTestParams("3600.001"),
                                          MetadataTTLTestParams("INVALID"),
                                          MetadataTTLTestParams("1,1")));

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

/**
 * @test Checks that when for some reason the metadata server starts
 *       returning the information about the cluster nodes in different order we
 *       will not treat this as a change (Bug#29264764).
 */
TEST_F(MetadataChacheTTLTest, InstancesListUnordered) {
  // create and RAII-remove tmp dirs
  TempDirectory temp_test_dir;
  TempDirectory conf_dir("conf");

  const std::string kGroupID = "";

  SCOPED_TRACE("// launch 2 server mocks");
  std::vector<ProcessWrapper *> nodes;
  std::vector<uint16_t> node_classic_ports;
  std::vector<uint16_t> node_http_ports;
  const std::string json_metadata =
      get_data_dir().join("metadata_dynamic_nodes.js").str();
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
  const std::string metadata_cache_section =
      get_metadata_cache_section(node_classic_ports, "0.1");
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

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
