/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#ifndef _WIN32
#include <pwd.h>  // getpwuid
#include <sys/stat.h>
#endif

#include <fstream>
#include <string>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "common.h"  // truncate_string
#include "dim.h"
#include "harness_assert.h"
#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/string_utils.h"  // split_string
#include "mysqld_error.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/utils.h"  // getpwuid
#include "random_generator.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_test_helpers.h"  // get_file_output
#include "script_generator.h"
#include "socket_operations.h"
#include "tcp_port_pool.h"

/**
 * @file
 * @brief Component Tests for the bootstrap operation
 */

using namespace std::chrono_literals;
using namespace std::string_literals;
using mysqlrouter::ClusterType;

// for the test with no param
class RouterBootstrapTest : public RouterComponentBootstrapTest {};

struct BootstrapTestParam {
  ClusterType cluster_type;
  std::string description;
  std::string trace_file;
  std::string trace_file2;
  std::string trace_file3;
};

auto get_test_description(
    const ::testing::TestParamInfo<BootstrapTestParam> &info) {
  return info.param.description;
}

class RouterBootstrapOkTest
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTestParam> {};

/**
 * @test
 *       verify that the router's \c --bootstrap can bootstrap
 *       from metadata-servers's PRIMARY over TCP/IP
 * @test
 *       Group Replication roles:
 *       - PRIMARY
 */
TEST_P(RouterBootstrapOkTest, BootstrapOk) {
  const auto param = GetParam();

  std::vector<Config> config{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(param.trace_file).str()},
  };

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(config, param.cluster_type));

  // let's check if the actual config file output is what we expect:

  const char *expected_config_default_part = "unknown_config_option=error";

  const char *expected_config_gr_part1 =
      R"([metadata_cache:bootstrap]
cluster_type=gr
router_id=1)";
  // we skip user as it is random and would require regex matching which would
  // require tons of escaping
  // user=mysql_router1_daxi69tk9btt
  const char *expected_config_gr_part2 =
      R"(metadata_cluster=mycluster
ttl=0.5
auth_cache_ttl=-1
auth_cache_refresh_interval=2
use_gr_notifications=0

[routing:bootstrap_rw]
bind_address=0.0.0.0
bind_port=6446
destinations=metadata-cache://mycluster/?role=PRIMARY
routing_strategy=first-available
protocol=classic

[routing:bootstrap_ro]
bind_address=0.0.0.0
bind_port=6447
destinations=metadata-cache://mycluster/?role=SECONDARY
routing_strategy=round-robin-with-fallback
protocol=classic

[routing:bootstrap_x_rw]
bind_address=0.0.0.0
bind_port=6448
destinations=metadata-cache://mycluster/?role=PRIMARY
routing_strategy=first-available
protocol=x

[routing:bootstrap_x_ro]
bind_address=0.0.0.0
bind_port=6449
destinations=metadata-cache://mycluster/?role=SECONDARY
routing_strategy=round-robin-with-fallback
protocol=x)";

  const char *expected_config_ar_part1 =
      R"([metadata_cache:bootstrap]
cluster_type=rs
router_id=1)";
  // we skip user as it is random and would require regex matching which would
  // require tons of escaping
  // user=mysql_router1_ritc56yrjz42
  const char *expected_config_ar_part2 =
      R"(metadata_cluster=mycluster
ttl=0.5
auth_cache_ttl=-1
auth_cache_refresh_interval=2

[routing:bootstrap_rw]
bind_address=0.0.0.0
bind_port=6446
destinations=metadata-cache://mycluster/?role=PRIMARY
routing_strategy=first-available
protocol=classic

[routing:bootstrap_ro]
bind_address=0.0.0.0
bind_port=6447
destinations=metadata-cache://mycluster/?role=SECONDARY
routing_strategy=round-robin-with-fallback
protocol=classic

[routing:bootstrap_x_rw]
bind_address=0.0.0.0
bind_port=6448
destinations=metadata-cache://mycluster/?role=PRIMARY
routing_strategy=first-available
protocol=x

[routing:bootstrap_x_ro]
bind_address=0.0.0.0
bind_port=6449
destinations=metadata-cache://mycluster/?role=SECONDARY
routing_strategy=round-robin-with-fallback
protocol=x)";

  const std::string config_file_expected1 =
      GetParam().cluster_type == ClusterType::RS_V2 ? expected_config_ar_part1
                                                    : expected_config_gr_part1;

  const std::string config_file_expected2 =
      GetParam().cluster_type == ClusterType::RS_V2 ? expected_config_ar_part2
                                                    : expected_config_gr_part2;

  // 'config_file' is set as side-effect of bootstrap_failover()
  ASSERT_THAT(config_file, ::testing::Not(::testing::IsEmpty()));

  const std::string config_file_str = get_file_output(config_file);

  EXPECT_THAT(
      config_file_str,
      ::testing::AllOf(::testing::HasSubstr(expected_config_default_part),
                       ::testing::HasSubstr(config_file_expected1),
                       ::testing::HasSubstr(config_file_expected2)));
}

INSTANTIATE_TEST_SUITE_P(
    BootstrapOkTest, RouterBootstrapOkTest,
    ::testing::Values(
        BootstrapTestParam{ClusterType::GR_V2, "gr", "bootstrap_gr.js", "", ""},
        BootstrapTestParam{ClusterType::RS_V2, "ar", "bootstrap_ar.js", "", ""},
        BootstrapTestParam{ClusterType::GR_V1, "gr_v1", "bootstrap_gr_v1.js",
                           "", ""}),
    get_test_description);

struct BootstrapOkBasePortTestParam {
  const char *test_name;

  std::vector<std::string> bs_params;

  uint16_t expected_port_classic_rw;
  uint16_t expected_port_classic_ro;
  uint16_t expected_port_x_rw;
  uint16_t expected_port_x_ro;
};

class RouterBootstrapOkBasePortTest
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapOkBasePortTestParam> {};

namespace {

void check_bind_port(const std::string &conf_file_content,
                     const std::string &route_name,
                     const std::string &protocol_name,
                     const std::string &server_role,
                     uint16_t expected_bind_port) {
  const std::string routing_strategy = server_role == "PRIMARY"
                                           ? "first-available"
                                           : "round-robin-with-fallback";
  // clang-format off
  const std::string routing_section =
      "[routing:"s + route_name + "]\n"
      "bind_address=0.0.0.0\n" +
      "bind_port=" + std::to_string(expected_bind_port) + "\n" +
      "destinations=metadata-cache://mycluster/?role=" +  server_role + "\n" +
      "routing_strategy=" + routing_strategy + "\n" +
      "protocol=" + protocol_name + "\n";
  // clang-format on

  EXPECT_TRUE(conf_file_content.find(routing_section) != std::string::npos)
      << conf_file_content << "EXPECTED: \n"
      << routing_section;
}

bool config_file_contains(const std::string &conf_file_content,
                          const std::string &line,
                          const size_t occurences = 1) {
  return occurences == count_str_occurences(conf_file_content, line);
}

}  // namespace

/**
 * @test
 *       verify that the --conf-base-port bootstrap parameter is handled
 * properly
 */
TEST_P(RouterBootstrapOkBasePortTest, RouterBootstrapOkBasePort) {
  const auto param = GetParam();
  const std::string tracefile = "bootstrap_gr.js";

  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), get_data_dir().join(tracefile).str()},
  };

  std::vector<std::string> cmdline = {
      "--bootstrap=" + mock_servers.at(0).ip + ":" +
          std::to_string(mock_servers.at(0).port),
      "-d", bootstrap_dir.name()};

  cmdline.insert(cmdline.begin(), param.bs_params.begin(),
                 param.bs_params.end());

  ASSERT_NO_FATAL_FAILURE(
      bootstrap_failover(mock_servers, ClusterType::GR_V2, cmdline));

  // 'config_file' is set as side-effect of bootstrap_failover()
  ASSERT_THAT(config_file, ::testing::Not(::testing::IsEmpty()));

  // let's check if the actual config file contains what we expect:
  const std::string config_file_str = get_file_output(config_file);

  // classic RW
  check_bind_port(config_file_str, "bootstrap_rw", "classic", "PRIMARY",
                  param.expected_port_classic_rw);

  // classic RO
  check_bind_port(config_file_str, "bootstrap_ro", "classic", "SECONDARY",
                  param.expected_port_classic_ro);

  // x RW
  check_bind_port(config_file_str, "bootstrap_x_rw", "x", "PRIMARY",
                  param.expected_port_x_rw);

  // x RO
  check_bind_port(config_file_str, "bootstrap_x_ro", "x", "SECONDARY",
                  param.expected_port_x_ro);
}

const BootstrapOkBasePortTestParam bootstrap_ok_base_port_test_param[] = {
    {"default_ports",
     /* bs_params */ {},
     /* expected_port_classic_rw */ 6446,
     /* expected_port_classic_ro */ 6447,
     /* expected_port_x_rw */ 6448,
     /* expected_port_x_ro */ 6449},
    {"legacy_default_ports",
     /* bs_params */ {"--conf-base-port=0"},
     /* expected_port_classic_rw */ 6446,
     /* expected_port_classic_ro */ 6447,
     /* expected_port_x_rw */ 64460,
     /* expected_port_x_ro */ 64470},
    {"consecutive_ports",
     /* bs_params */ {"--conf-base-port=1234"},
     /* expected_port_classic_rw */ 1234,
     /* expected_port_classic_ro */ 1235,
     /* expected_port_x_rw */ 1236,
     /* expected_port_x_ro */ 1237}};

INSTANTIATE_TEST_SUITE_P(RouterBootstrapOkBasePort,
                         RouterBootstrapOkBasePortTest,
                         ::testing::ValuesIn(bootstrap_ok_base_port_test_param),
                         [](const auto &info) { return info.param.test_name; });

struct BootstrapErrorBasePortTestParam {
  const char *test_name;

  std::vector<std::string> bs_params;
  std::string expected_error;
};

class RouterBootstrapErrorBasePortTest
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapErrorBasePortTestParam> {};

/**
 * @test
 *       verify that the --conf-base-port bootstrap parameter errors are handled
 * properly
 */
TEST_P(RouterBootstrapErrorBasePortTest, RouterBootstrapErrorBasePort) {
  const auto param = GetParam();
  const std::string tracefile = "bootstrap_gr.js";

  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), get_data_dir().join(tracefile).str()},
  };

  const uint16_t server_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join(tracefile).str();
  launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);

  // launch the router in bootstrap mode
  std::vector<std::string> cmdline = {"--bootstrap=root:"s + kRootPassword +
                                          "@localhost:"s +
                                          std::to_string(server_port),
                                      "-d", bootstrap_dir.name()};
  cmdline.insert(cmdline.begin(), param.bs_params.begin(),
                 param.bs_params.end());
  auto &router = launch_router_for_bootstrap(cmdline, EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  // let's check if the expected error was reported:
  EXPECT_THAT(router.get_full_output(),
              ::testing::ContainsRegex(param.expected_error));
}

const BootstrapErrorBasePortTestParam bootstrap_error_base_port_test_param[] = {
    {"negative",
     {"--conf-base-port=-1"},
     "--conf-base-port needs value between 0 and 65532 inclusive, was '-1'"},
    {"too_big",
     {"--conf-base-port=65533"},
     "--conf-base-port needs value between 0 and 65532 inclusive, was '65533'"},
    {"nan",
     {"--conf-base-port=abc"},
     "--conf-base-port needs value between 0 and 65532 inclusive, was 'abc'"},
    {"empty",
     {"--conf-base-port="},
     "--conf-base-port needs value between 0 and 65532 inclusive, was ''"},
};

INSTANTIATE_TEST_SUITE_P(
    RouterBootstrapErrorBasePort, RouterBootstrapErrorBasePortTest,
    ::testing::ValuesIn(bootstrap_error_base_port_test_param),
    [](const auto &info) { return info.param.test_name; });

struct ReBootstrapOkBasePortTestParam {
  const char *test_name;
  std::vector<std::string> first_bs_params;
  std::vector<std::string> second_bs_params;

  uint16_t expected_port_classic_rw;
  uint16_t expected_port_classic_ro;
  uint16_t expected_port_x_rw;
  uint16_t expected_port_x_ro;
};

class RouterReBootstrapOkBasePortTest
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<ReBootstrapOkBasePortTestParam> {};

/**
 * @test
 *       verify that the --conf-base-port bootstrap parameter is handled
 * properly when we overwrite an existing Router configuration
 */
TEST_P(RouterReBootstrapOkBasePortTest, RouterReBootstrapOkBasePort) {
  const auto param = GetParam();
  const std::string tracefile = "bootstrap_gr.js";

  const uint16_t server_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join(tracefile).str();
  launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);

  // do the first bootstrap
  std::vector<std::string> cmdline_first_bs = {
      "--bootstrap=root:"s + kRootPassword + "@localhost:"s +
          std::to_string(server_port),
      "-d", bootstrap_dir.name()};
  cmdline_first_bs.insert(cmdline_first_bs.begin(),
                          param.first_bs_params.begin(),
                          param.first_bs_params.end());

  auto &router_bs1 = launch_router_for_bootstrap(cmdline_first_bs);
  check_exit_code(router_bs1, EXIT_SUCCESS);

  const std::string conf_file2 =
      mysql_harness::Path(bootstrap_dir.name()).join("mysqlrouter.conf").str();

  // do the second bootstrap using the same directory
  std::vector<std::string> cmdline_second_bs = {
      "--bootstrap=root:"s + kRootPassword + "@localhost:"s +
          std::to_string(server_port),
      "-d", bootstrap_dir.name()};
  cmdline_second_bs.insert(cmdline_second_bs.begin(),
                           param.second_bs_params.begin(),
                           param.second_bs_params.end());
  auto &router_bs2 = launch_router_for_bootstrap(cmdline_second_bs);
  check_exit_code(router_bs2, EXIT_SUCCESS);

  const std::string conf_file =
      mysql_harness::Path(bootstrap_dir.name()).join("mysqlrouter.conf").str();

  // let's check if the actual config file contains what we expect:
  const std::string config_file_str = get_file_output(conf_file);

  // classic RW
  check_bind_port(config_file_str, "bootstrap_rw", "classic", "PRIMARY",
                  param.expected_port_classic_rw);

  // classic RO
  check_bind_port(config_file_str, "bootstrap_ro", "classic", "SECONDARY",
                  param.expected_port_classic_ro);

  // x RW
  check_bind_port(config_file_str, "bootstrap_x_rw", "x", "PRIMARY",
                  param.expected_port_x_rw);

  // x RO
  check_bind_port(config_file_str, "bootstrap_x_ro", "x", "SECONDARY",
                  param.expected_port_x_ro);
}

const ReBootstrapOkBasePortTestParam rebootstrap_ok_base_port_test_param[] = {
    // create a config with legacy defaults [6446, 6447, 64460, 64470]
    // bootstrap again on top of that config with no conf-base-port parameter
    // since the existing conf uses legacy default we should stick to them
    {"overwrite_over_legacy_defaults_keep_them",
     /* first_bs_params */ {"--conf-base-port=0"},
     /* second_bs_params */ {},
     /* expected_port_classic_rw */ 6446,
     /* expected_port_classic_ro */ 6447,
     /* expected_port_x_rw */ 64460,
     /* expected_port_x_ro */ 64470},

    // create a config with custom ports [5000, 5001, 5002, 5003]
    // bootstrap again on top of that config with no conf-base-port parameter
    // we expect new default ports to be used
    {"overwrite_custom_ports",
     /* first_bs_params */ {"--conf-base-port=5000"},
     /* second_bs_params */ {},
     /* expected_port_classic_rw */ 6446,
     /* expected_port_classic_ro */ 6447,
     /* expected_port_x_rw */ 6448,
     /* expected_port_x_ro */ 6449},

    // create a config with legacy ports [6446, 6447, 64460, 64470]
    // bootstrap again on top of that config with --conf-base-port=1 parameter
    // we expect 1, 2, 3, 4 ports to overwrite the legacy ports
    {"overwrite_legacy_with_custom_ports",
     /* first_bs_params */ {"--conf-base-port=0"},
     /* second_bs_params */ {"--conf-base-port=1"},
     /* expected_port_classic_rw */ 1,
     /* expected_port_classic_ro */ 2,
     /* expected_port_x_rw */ 3,
     /* expected_port_x_ro */ 4},

    // create a config with legacy defaults [6446, 6447, 64460, 64470]
    // bootstrap again on top of that config with specifying conf-base-port
    // parameter even though the existing conf uses legacy default we change
    // them because the user used conf-base-port, so we should not be using
    // defaults
    {"overwrite_over_legacy_defaults_using_param_change_them",
     /* first_bs_params */ {"--conf-base-port=0"},
     /* second_bs_params */ {"--conf-base-port=6446"},
     /* expected_port_classic_rw */ 6446,
     /* expected_port_classic_ro */ 6447,
     /* expected_port_x_rw */ 6448,
     /* expected_port_x_ro */ 6449},

    // create a config with custom ports [6666, 6667, 6668, 6669]
    // bootstrap again on top of that config with conf-base-port=0 parameter
    // since the user requested legacy defaults the ports in the config should
    // be [6446, 6447, 64460, 64470]
    {"overwrite_custom_ports_with_legacy",
     /* first_bs_params */ {"--conf-base-port=6666"},
     /* second_bs_params */ {"--conf-base-port=0"},
     /* expected_port_classic_rw */ 6446,
     /* expected_port_classic_ro */ 6447,
     /* expected_port_x_rw */ 64460,
     /* expected_port_x_ro */ 64470},

#ifndef _WIN32
    // create a config with no tcp endpoints
    // bootstrap again on top of that config with no conf-base-port parameter
    // the new defaults should be used
    {"overwrite_over_no_tcp_config_new_defaults",
     /* first_bs_params */ {"--conf-skip-tcp"},
     /* second_bs_params */ {},
     /* expected_port_classic_rw */ 6446,
     /* expected_port_classic_ro */ 6447,
     /* expected_port_x_rw */ 6448,
     /* expected_port_x_ro */ 6449},

    // create a config with no tcp endpoints
    // bootstrap again on top of that config with conf-base-port=0 parameter
    // since the user requested legacy defaults the ports in the config should
    // be [6446, 6447, 64460, 64470]
    {"overwrite_over_no_tcp_config_legacy_defaults",
     /* first_bs_params */ {"--conf-skip-tcp"},
     /* second_bs_params */ {"--conf-base-port=0"},
     /* expected_port_classic_rw */ 6446,
     /* expected_port_classic_ro */ 6447,
     /* expected_port_x_rw */ 64460,
     /* expected_port_x_ro */ 64470}
#endif
};

INSTANTIATE_TEST_SUITE_P(
    RouterReBootstrapOkBasePort, RouterReBootstrapOkBasePortTest,
    ::testing::ValuesIn(rebootstrap_ok_base_port_test_param),
    [](const auto &info) { return info.param.test_name; });

#ifndef _WIN32

/**
 * verify that the router's \c --user is ignored if it matches the current
 * username.
 *
 * skipped on win32 as \c --user isn't supported on windows
 *
 * @test
 *       test if Bug#27698052 is fixed
 * @test
 *       Group Replication roles:
 *       - PRIMARY
 */
class RouterBootstrapUserIsCurrentUser
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTestParam> {};

TEST_P(RouterBootstrapUserIsCurrentUser, BootstrapUserIsCurrentUser) {
  const auto param = GetParam();

  auto current_userid = geteuid();
  auto current_userpw = getpwuid(current_userid);
  if (current_userpw != nullptr) {
    const char *current_username = current_userpw->pw_name;

    std::vector<Config> mock_servers{
        {"127.0.0.1", port_pool_.get_next_available(),
         port_pool_.get_next_available(),
         get_data_dir().join(param.trace_file).str()},
    };

    std::vector<std::string> router_options = {
        "--bootstrap=" + mock_servers.at(0).ip + ":" +
            std::to_string(mock_servers.at(0).port),
        "-d",
        bootstrap_dir.name(),
        "--report-host",
        my_hostname,
        "--user",
        current_username};

    ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
        mock_servers, GetParam().cluster_type, router_options));
  }
}

INSTANTIATE_TEST_SUITE_P(
    BootstrapUserIsCurrentUser, RouterBootstrapUserIsCurrentUser,
    ::testing::Values(
        BootstrapTestParam{ClusterType::GR_V2, "gr", "bootstrap_gr.js", "", ""},
        BootstrapTestParam{ClusterType::RS_V2, "ar", "bootstrap_ar.js", "", ""},
        BootstrapTestParam{ClusterType::GR_V1, "gr_v1", "bootstrap_gr_v1.js",
                           "", ""}),
    get_test_description);
#endif

class RouterBootstrapailoverClusterIdDiffers
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTestParam> {};

/**
 * @test
 *       verify that the router's \c --bootstrap fails when it fails over to the
 * node with a different cluster-id/replication-group-id
 */
TEST_P(RouterBootstrapailoverClusterIdDiffers,
       BootstrapFailoverClusterIdDiffers) {
  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(GetParam().trace_file).str(), false, "cluster-id-1"},
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(GetParam().trace_file2).str(), false,
       "cluster-id-2"},
  };

  // check that it failed as expected
  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      mock_servers, ClusterType::RS_V2, {}, EXIT_FAILURE,
      {"Node on '.*' that the bootstrap failed over to, seems to belong to "
       "different cluster\\(cluster-id-1 != cluster-id-2\\), skipping"}));
}

INSTANTIATE_TEST_SUITE_P(
    BootstrapFailoverClusterIdDiffers, RouterBootstrapailoverClusterIdDiffers,
    ::testing::Values(
        BootstrapTestParam{ClusterType::GR_V2, "gr",
                           "bootstrap_failover_super_read_only_1_gr.js",
                           "bootstrap_failover_super_read_only_1_gr.js", ""},
        BootstrapTestParam{ClusterType::RS_V2, "ar",
                           "bootstrap_failover_super_read_only_1_ar.js",
                           "bootstrap_failover_super_read_only_1_ar.js", ""},
        BootstrapTestParam{ClusterType::GR_V1, "gr_v1",
                           "bootstrap_failover_super_read_only_1_gr_v1.js",
                           "bootstrap_failover_super_read_only_1_gr_v1.js",
                           ""}),
    get_test_description);

/**
 * @test
 *       verify that the router's \c --bootstrap can bootstrap
 *       from metadata-server's PRIMARY over TCP/IP and generate
 *       a configuration with unix-sockets only
 * @test
 *       Group Replication roles:
 *       - PRIMARY
 */
class RouterBootstrapOnlySockets
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTestParam> {};

TEST_P(RouterBootstrapOnlySockets, BootstrapOnlySockets) {
  const auto param = GetParam();

  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(param.trace_file).str()},
  };

  std::vector<std::string> router_options = {
      "--bootstrap=" + mock_servers.at(0).ip + ":" +
          std::to_string(mock_servers.at(0).port),
      "-d",
      bootstrap_dir.name(),
      "--report-host",
      my_hostname,
      "--conf-skip-tcp",
      "--conf-use-sockets"};

#ifndef _WIN32
  const std::vector<std::string> expected_output{
      "- Read/Write Connections: .*/mysqlx.sock",
      "- Read/Only Connections: .*/mysqlxro.sock"};
  const auto expected_result = EXIT_SUCCESS;
#else
  const std::vector<std::string> expected_output{
      "Error: unknown option '--conf-skip-tcp'"};
  const auto expected_result = EXIT_FAILURE;
#endif

  ASSERT_NO_FATAL_FAILURE(
      bootstrap_failover(mock_servers, GetParam().cluster_type, router_options,
                         expected_result, expected_output));
}

INSTANTIATE_TEST_SUITE_P(
    BootstrapOnlySockets, RouterBootstrapOnlySockets,
    ::testing::Values(
        BootstrapTestParam{ClusterType::GR_V2, "gr", "bootstrap_gr.js", "", ""},
        BootstrapTestParam{ClusterType::RS_V2, "ar", "bootstrap_ar.js", "", ""},
        BootstrapTestParam{ClusterType::GR_V1, "gr_v1", "bootstrap_gr_v1.js",
                           "", ""}),
    get_test_description);

class BootstrapUnsupportedSchemaVersionTest
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<mysqlrouter::MetadataSchemaVersion> {
};

/**
 * @test
 *       verify that the router's \c --bootstrap detects an unsupported
 *       metadata schema version
 */
TEST_P(BootstrapUnsupportedSchemaVersionTest,
       BootstrapUnsupportedSchemaVersion) {
  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_unsupported_schema_version.js").str()},
  };

  const auto version = GetParam();

  // check that it failed as expected
  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      mock_servers, ClusterType::GR_V2, {}, EXIT_FAILURE,
      {"^Error: This version of MySQL Router is not compatible "
       "with the provided MySQL InnoDB cluster metadata. "
       "Expected metadata version 1.0.0, 2.0.0, got " +
       to_string(version)},
      10s, GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    BootstrapUnsupportedSchemaVersion, BootstrapUnsupportedSchemaVersionTest,
    ::testing::Values(mysqlrouter::MetadataSchemaVersion{3, 0, 0},
                      mysqlrouter::MetadataSchemaVersion{0, 0, 1},
                      mysqlrouter::MetadataSchemaVersion{3, 1, 0}));

/**
 * @test
 *       verify that the router errors out cleanly when received some unexpected
 *       error from the metadata server
 */
TEST_F(RouterComponentBootstrapTest, BootstrapErrorOnFirstQuery) {
  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_error_on_first_query.js").str()},
  };

  // check that it failed as expected
  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      mock_servers, ClusterType::RS_V2, {}, EXIT_FAILURE,
      {"Error executing MySQL query", "Some unexpected error occured"}, 10s));
}

/**
 * @test
 *       verify that the router's \c --bootstrap detects an upgrade
 *       metadata schema version and gives a proper message
 */
TEST_F(RouterComponentBootstrapTest, BootstrapWhileMetadataUpgradeInProgress) {
  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_unsupported_schema_version.js").str()},
  };

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      mock_servers, ClusterType::GR_V2, {}, EXIT_FAILURE,
      {"^Error: Currently the cluster metadata update is in progress. Please "
       "rerun the bootstrap when it is finished."},
      10s, {0, 0, 0}));
}

/**
 * @test
 *       verify that the router's \c --bootstrap handles --pid-file option on
 *       command line correctly
 *       TS_FR12_01
 */
TEST_F(RouterComponentBootstrapTest, BootstrapPidfileOpt) {
  std::string pidfile =
      mysql_harness::Path(get_test_temp_dir_name()).join("test.pid").str();

  std::vector<Config> config{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_gr.js").str()},
  };

  std::vector<std::string> router_options = {
      "--pid-file",
      pidfile,
      "--bootstrap=" + config.at(0).ip + ":" +
          std::to_string(config.at(0).port),
      "-d",
      bootstrap_dir.name(),
      "--report-host",
      my_hostname};

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      config, ClusterType::GR_V2, router_options, EXIT_FAILURE,
      {"^Error: Option --pid-file cannot be used together "
       "with -B/--bootstrap"},
      10s));
}

/**
 * @test
 *       verify that the router's \c --bootstrap handles pid_file option in
 *       config file correctly
 *       TS_FR13_01
 */
TEST_F(RouterComponentBootstrapTest, BootstrapPidfileCfg) {
  std::string pidfile = mysql_harness::Path(get_test_temp_dir_name())
                            .real_path()
                            .join("test.pid")
                            .str();

  auto params = get_DEFAULT_defaults();
  params["pid_file"] = pidfile;
  std::string conf_file =
      create_config_file(get_test_temp_dir_name(), "", &params);

  {
    std::vector<Config> config{
        {"127.0.0.1", port_pool_.get_next_available(),
         port_pool_.get_next_available(),
         get_data_dir().join("bootstrap_gr.js").str()},
    };

    std::vector<std::string> router_options = {
        "-c",
        conf_file,
        "--bootstrap=" + config.at(0).ip + ":" +
            std::to_string(config.at(0).port),
        "-d",
        bootstrap_dir.name(),
        "--report-host",
        my_hostname};

    ASSERT_NO_FATAL_FAILURE(
        bootstrap_failover(config, ClusterType::GR_V2, router_options));

    ASSERT_FALSE(mysql_harness::Path(pidfile.c_str()).exists());
  }

  // Post check that pid_file is not included in config
  const std::string config_file_str = get_file_output(config_file);

  EXPECT_TRUE(config_file_str.find("pid_file") == std::string::npos)
      << "config file includes pid_file setting :" << std::endl
      << config_file_str << std::endl;
}

/**
 * @test
 *       verify that the router's \c --bootstrap does not create a pidfile when
 *       ROUTER_PID is specified
 *       TS_FR13_02
 */
TEST_F(RouterComponentBootstrapTest, BootstrapPidfileEnv) {
  // Set ROUTER_PID
  std::string pidfile = mysql_harness::Path(get_test_temp_dir_name())
                            .real_path()
                            .join("test.pid")
                            .str();
#ifdef _WIN32
  int err_code = _putenv_s("ROUTER_PID", pidfile.c_str());
#else
  int err_code = ::setenv("ROUTER_PID", pidfile.c_str(), 1);
#endif
  if (err_code) throw std::runtime_error("Failed to add ROUTER_PID");

  {
    std::vector<Config> config{
        {"127.0.0.1", port_pool_.get_next_available(),
         port_pool_.get_next_available(),
         get_data_dir().join("bootstrap_gr.js").str()},
    };

    std::vector<std::string> router_options = {
        "--bootstrap=" + config.at(0).ip + ":" +
            std::to_string(config.at(0).port),
        "-d", bootstrap_dir.name(), "--report-host", my_hostname};

    ASSERT_NO_FATAL_FAILURE(
        bootstrap_failover(config, ClusterType::GR_V2, router_options));

    ASSERT_FALSE(mysql_harness::Path(pidfile.c_str()).exists());
  }

  // reset ROUTER_PID
#ifdef _WIN32
  err_code = _putenv_s("ROUTER_PID", "");
#else
  err_code = ::unsetenv("ROUTER_PID");
#endif
  if (err_code) throw std::runtime_error("Failed to remove ROUTER_PID");

  // Post check that pid_file is not included in config
  const std::string config_file_str = get_file_output(config_file);

  EXPECT_TRUE(config_file_str.find("pid_file") == std::string::npos)
      << "config file includes pid_file setting :" << std::endl
      << config_file_str << std::endl;
}

class RouterBootstrapFailoverSuperReadonly
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTestParam> {};

/**
 * @test
 *       verify that bootstrap will fail-over to another node if the initial
 *       node is not writable
 * @test
 *       Group Replication roles:
 *       - SECONDARY
 *       - PRIMARY
 *       - SECONDARY (not used)
 */
TEST_P(RouterBootstrapFailoverSuperReadonly, BootstrapFailoverSuperReadonly) {
  const auto param = GetParam();

  std::vector<Config> config{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(param.trace_file).str()},
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(param.trace_file2).str()},
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), ""},
  };

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(config, param.cluster_type));
}

INSTANTIATE_TEST_SUITE_P(
    BootstrapFailoverSuperReadonly, RouterBootstrapFailoverSuperReadonly,
    ::testing::Values(
        BootstrapTestParam{ClusterType::GR_V2, "gr",
                           "bootstrap_failover_super_read_only_1_gr.js",
                           "bootstrap_gr.js", ""},
        BootstrapTestParam{ClusterType::RS_V2, "ar",
                           "bootstrap_failover_super_read_only_1_ar.js",
                           "bootstrap_ar.js", ""},
        BootstrapTestParam{ClusterType::GR_V1, "gr_v1",
                           "bootstrap_failover_super_read_only_1_gr_v1.js",
                           "bootstrap_gr_v1.js", ""}),
    get_test_description);

class RouterBootstrapFailoverSuperReadonly2ndNodeDead
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTestParam> {};

/**
 * @test
 *       verify that bootstrap will fail-over to another node if the initial
 *       node is not writable and 2nd candidate has connection problems
 * @test
 *       Group Replication roles:
 *       - SECONDARY
 *       - <connect-failure>
 *       - PRIMARY
 * @test
 *       connection problems could be anything from 'auth-failure' to
 * 'network-errors'. This test uses a \c port==0 to create a failure which is
 * reserved and unassigned.
 *
 * @note The implementation uses \c port=65536 to circumvents libmysqlclients
 * \code{.py} if port == 0: port = 3306 \endcode default port assignment. As the
 * port will later be narrowed to an 16bit unsigned integer \code port & 0xffff
 * \endcode the code will connect to port 0 in the end.
 *
 * @todo As soon as the mysql-server-mock supports authentication failures
 *       the code can take that into account too.
 */
TEST_P(RouterBootstrapFailoverSuperReadonly2ndNodeDead,
       BootstrapFailoverSuperReadonly2ndNodeDead) {
  const auto param = GetParam();

  const auto dead_port = port_pool_.get_next_available();
  std::vector<Config> config{
      // member-1, PRIMARY, fails at first write
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(param.trace_file).str()},
      // member-2, unreachable
      {"127.0.0.1", dead_port, port_pool_.get_next_available(), "",
       /*unaccessible=*/true},
      // member-3, succeeds
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(param.trace_file2).str()},
  };

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      config, param.cluster_type, {}, EXIT_SUCCESS,
      {
          "^Fetching Cluster Members",
          "^Failed connecting to 127\\.0\\.0\\.1:"s +
              std::to_string(dead_port) + ": .*, trying next$",
      }));
}

INSTANTIATE_TEST_SUITE_P(
    BootstrapFailoverSuperReadonly2ndNodeDead,
    RouterBootstrapFailoverSuperReadonly2ndNodeDead,
    ::testing::Values(
        BootstrapTestParam{ClusterType::GR_V2, "gr",
                           "bootstrap_failover_super_read_only_1_gr.js",
                           "bootstrap_gr.js", ""},
        BootstrapTestParam{ClusterType::RS_V2, "ar",
                           "bootstrap_failover_super_read_only_1_ar.js",
                           "bootstrap_ar.js", ""},
        BootstrapTestParam{ClusterType::GR_V1, "gr_v1",
                           "bootstrap_failover_super_read_only_1_gr_v1.js",
                           "bootstrap_gr_v1.js", ""}),
    get_test_description);

class RouterBootstrapFailoverPrimaryUnreachable
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTestParam> {};

/**
 * @test
 *       verify that bootstrap will fail-over to another node if the initial
 *       nodes are not writable and the 3rd one is unreachable
 * @test
 *       Group Replication roles:
 *       - SECONDARY
 *       - SECONDARY
 *       - PRIMARY (unreachable)
 */
TEST_P(RouterBootstrapFailoverPrimaryUnreachable,
       BootstrapFailoverPrimaryUnreachable) {
  const auto param = GetParam();

  const auto dead_port = port_pool_.get_next_available();
  std::vector<Config> config{
      // member-1, fails at first write (SEONDARY)
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(param.trace_file).str()},
      // member-2, fails at first write (SEONDARY)
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(param.trace_file).str()},
      // member-3, unreachable (potential PRIMARY)
      {"127.0.0.1", dead_port, port_pool_.get_next_available(), "",
       /*unaccessible=*/true},
  };

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      config, param.cluster_type, {}, EXIT_FAILURE,
      {"^Fetching Cluster Members",
       "^Failed connecting to 127\\.0\\.0\\.1:"s + std::to_string(dead_port) +
           ": .*, trying next$",
       "Error: no more nodes to fail-over too, giving up."}));
}

INSTANTIATE_TEST_SUITE_P(
    BootstrapFailoverPrimaryUnreachable,
    RouterBootstrapFailoverPrimaryUnreachable,
    ::testing::Values(
        BootstrapTestParam{ClusterType::GR_V2, "gr",
                           "bootstrap_failover_super_read_only_1_gr.js", "",
                           ""},
        BootstrapTestParam{ClusterType::RS_V2, "ar",
                           "bootstrap_failover_super_read_only_1_ar.js", "",
                           ""},
        BootstrapTestParam{ClusterType::GR_V1, "gr_v1",
                           "bootstrap_failover_super_read_only_1_gr_v1.js", "",
                           ""}),
    get_test_description);

class RouterBootstrapFailoverSuperReadonlyCreateAccountFails
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTestParam> {};

/**
 * @test
 *       verify that bootstrap fails over and continues if create-account
 fails
 *       due to 1st node not being writable
 * @test
 *       Group Replication roles:
 *       - SECONDARY
 *       - PRIMARY
 *       - SECONDARY (not used)
 */
TEST_P(RouterBootstrapFailoverSuperReadonlyCreateAccountFails,
       BootstrapFailoverSuperReadonlyCreateAccountFails) {
  const auto param = GetParam();

  std::vector<Config> config{
      // member-1: SECONDARY, fails at DROP USER due to RW request on RO node
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(param.trace_file).str()},

      // member-2: PRIMARY, succeeds
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(param.trace_file2).str()},

      // member-3: defined, but unused
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), ""},
  };

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(config, param.cluster_type));
}

INSTANTIATE_TEST_SUITE_P(
    BootstrapFailoverSuperReadonlyCreateAccountFails,
    RouterBootstrapFailoverSuperReadonlyCreateAccountFails,
    ::testing::Values(
        BootstrapTestParam{
            ClusterType::GR_V2, "gr",
            "bootstrap_failover_super_read_only_dead_2nd_1_gr.js",
            "bootstrap_failover_reconfigure_ok.js", ""},
        BootstrapTestParam{
            ClusterType::RS_V2, "ar",
            "bootstrap_failover_super_read_only_dead_2nd_1_ar.js",
            "bootstrap_failover_reconfigure_ok.js", ""},
        BootstrapTestParam{
            ClusterType::GR_V1, "gr_v1",
            "bootstrap_failover_super_read_only_dead_2nd_1_gr_v1.js",
            "bootstrap_failover_reconfigure_ok_v1.js", ""}),
    get_test_description);

class RouterBootstrapFailoverSuperReadonlyCreateAccountGrantFails
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTestParam> {};

/**
 * @test
 *       verify that bootstrap DOES NOT fail over if create-account GRANT fails
 * @test
 *       Group Replication roles:
 *       - SECONDARY
 *       - (not used)
 *       - (not used)
 *
 */
TEST_P(RouterBootstrapFailoverSuperReadonlyCreateAccountGrantFails,
       BootstrapFailoverSuperReadonlyCreateAccountGrantFails) {
  std::vector<Config> config{
      // member-1: SECONDARY fails and exits after GRANT
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(GetParam().trace_file).str()},

      // member-2: defined, but unused
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), ""},

      // member-3: defined, but unused
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), ""},
  };

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      config, GetParam().cluster_type, {}, EXIT_FAILURE,
      {"Error: Error creating MySQL account for router \\(GRANTs stage\\): "
       "Error executing MySQL query \"GRANT SELECT, EXECUTE ON "
       "mysql_innodb_cluster_metadata.*\": The MySQL server is running with "
       "the --super-read-only option so it cannot execute this statement"}));
}

INSTANTIATE_TEST_SUITE_P(
    BootstrapFailoverSuperReadonlyCreateAccountGrantFails,
    RouterBootstrapFailoverSuperReadonlyCreateAccountGrantFails,
    ::testing::Values(
        BootstrapTestParam{ClusterType::GR_V2, "gr",
                           "bootstrap_failover_at_grant_gr.js", "", ""},
        BootstrapTestParam{ClusterType::RS_V2, "ar",
                           "bootstrap_failover_at_grant_ar.js", "", ""},
        BootstrapTestParam{ClusterType::GR_V1, "gr_v1",
                           "bootstrap_failover_at_grant_gr_v1.js", "", ""}),
    get_test_description);

/**
 * @test
 *       verify that bootstrapping via a unix-socket fails over to the
 * IP-addresses of the members
 * @test
 *       Group Replication roles:
 *       - SECONDARY
 *       - PRIMARY
 *       - SECONDARY (not used)
 * @test
 *       Initial connect via unix-socket to the 1st node, all further connects
 * via TCP/IP
 *
 * @todo needs unix-socket support in the mock-server
 */
TEST_F(RouterBootstrapTest, DISABLED_BootstrapFailoverSuperReadonlyFromSocket) {
  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_failover_super_read_only_1.js").str()},
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_gr.js").str()},
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), ""},
  };

  std::vector<std::string> router_options = {
      "--bootstrap=localhost", "--bootstrap-socket=" + mock_servers.at(0).ip,
      "-d", bootstrap_dir.name()};

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      mock_servers, ClusterType::GR_V2, router_options, EXIT_FAILURE,
      {"Error: Error executing MySQL query: Lost connection to "
       "MySQL server during query \\(2013\\)"}));
}

class RouterBootstrapFailoverSuperReadonlyNewPrimaryCrash
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTestParam> {};

/**
 * @test
 *       verify that bootstrap fails over if PRIMARY crashes while bootstrapping
 *
 * @test
 *       Group Replication roles:
 *       - SECONDARY
 *       - PRIMARY (crashing)
 *       - PRIMARY
 */
TEST_P(RouterBootstrapFailoverSuperReadonlyNewPrimaryCrash,
       BootstrapFailoverSuperReadonlyNewPrimaryCrash) {
  std::vector<Config> mock_servers{
      // member-1: PRIMARY, fails at DROP USER
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(GetParam().trace_file).str()},

      // member-2: PRIMARY, but crashing
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(GetParam().trace_file2).str()},

      // member-3: newly elected PRIMARY, succeeds
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join(GetParam().trace_file3).str()},
  };

  ASSERT_NO_FATAL_FAILURE(
      bootstrap_failover(mock_servers, GetParam().cluster_type));
}

INSTANTIATE_TEST_SUITE_P(
    BootstrapFailoverSuperReadonlyNewPrimaryCrash,
    RouterBootstrapFailoverSuperReadonlyNewPrimaryCrash,
    ::testing::Values(
        BootstrapTestParam{
            ClusterType::GR_V2, "gr",
            "bootstrap_failover_super_read_only_dead_2nd_1_gr.js",
            "bootstrap_failover_at_crash.js",
            "bootstrap_failover_reconfigure_ok.js"},
        BootstrapTestParam{
            ClusterType::RS_V2, "ar",
            "bootstrap_failover_super_read_only_dead_2nd_1_ar.js",
            "bootstrap_failover_at_crash.js",
            "bootstrap_failover_reconfigure_ok.js"},
        BootstrapTestParam{
            ClusterType::GR_V1, "gr_v1",
            "bootstrap_failover_super_read_only_dead_2nd_1_gr_v1.js",
            "bootstrap_failover_at_crash_v1.js",
            "bootstrap_failover_reconfigure_ok_v1.js"}),
    get_test_description);

/**
 * @test
 * This test proves that bootstrap will not print out the success message
 * ("MySQL Router configured for the InnoDB cluster 'mycluster'" and many lines
 *  that follow it) until entire bootstrap succeeds.
 *
 * At the time of writing, the last operation that bootstrap performs is
 * writing a config file and backing up the old one.  Therefore we use that
 * as the basis of assessing the above expectation is met.
 */
TEST_F(RouterBootstrapTest,
       bootstrap_report_not_shown_until_bootstrap_succeeds) {
  TempDirectory bootstrap_directory;

  // create config files
  const Path bs_dir(bootstrap_directory.name());
  const std::string config_file = bs_dir.join("mysqlrouter.conf").str();
  const std::string config_bak_file = bs_dir.join("mysqlrouter.conf.bak").str();
  {
    std::ofstream f1(config_file);
    std::ofstream f2(config_bak_file);

    // contents must be different, otherwise a backup will not be attempted
    f1 << "[DEFAULT]\nkey1=val1\n";
    f2 << "[DEFAULT]\nkey2=val2\n";
  }

  // make config backup file RO to trigger the error
#ifdef _WIN32
  EXPECT_EQ(_chmod(config_bak_file.c_str(), S_IREAD), 0);
#else
  EXPECT_EQ(chmod(config_bak_file.c_str(), S_IRUSR), 0);
#endif

  // launch mock server that is our metadata server for the bootstrap
  const uint16_t server_port = port_pool_.get_next_available();
  const std::string json_stmts =
      get_data_dir()
          .join("bootstrap_report_host.js")
          .str();  // we piggy back on existing .js to avoid creating a new one
  auto &server_mock =
      launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);

  // launch the router in bootstrap mode
  const std::vector<std::string> cmdline = {
      "--bootstrap=127.0.0.1:" + std::to_string(server_port), "-d",
      bootstrap_directory.name(), "--report-host", "host.foo.bar"};
  auto &router = launch_router_for_bootstrap(cmdline, EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);
  // expect config write error
  EXPECT_THAT(router.get_full_output(),
              ::testing::ContainsRegex("Error: Could not create file "
                                       "'.*/mysqlrouter.conf.bak'"));

  // expect that the bootstrap success message (bootstrap report) is not
  // displayed
  EXPECT_THAT(router.get_full_output(), ::testing::Not(::testing::HasSubstr(
                                            "MySQL Router configured for the "
                                            "InnoDB cluster 'mycluster'")));

  server_mock.kill();
}

/**
 * @test
 *        verify connection times at bootstrap can be configured
 */
TEST_F(RouterBootstrapTest,
       BootstrapSucceedWhenServerResponseLessThanReadTimeout) {
  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_exec_time_2_seconds.js").str()},
  };

  std::vector<std::string> router_options = {
      "--bootstrap=" + mock_servers.at(0).ip + ":" +
          std::to_string(mock_servers.at(0).port),
      "--report-host",
      my_hostname,
      "-d",
      bootstrap_dir.name(),
      "--connect-timeout=3",
      "--read-timeout=3"};

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(mock_servers, ClusterType::GR_V2,
                                             router_options, EXIT_SUCCESS, {}));
}

TEST_F(RouterBootstrapTest, BootstrapAccessErrorAtGrantStatement) {
  std::vector<Config> config{
      // member-1: PRIMARY, fails after GRANT
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_access_error_at_grant.js").str()},

      // member-2: defined, but unused
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), ""},

      // member-3: defined, but unused
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), ""},
  };

  ASSERT_NO_FATAL_FAILURE(
      bootstrap_failover(config, ClusterType::GR_V2, {}, EXIT_FAILURE,
                         {"Access denied for user 'native'@'%' to database "
                          "'mysql_innodb_cluster_metadata"}));
}

class RouterBootstrapBootstrapNoGroupReplicationSetup
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTestParam> {};

/**
 * @test
 *       ensure a reasonable error message if schema exists, but no
 * group-replication is setup.
 */
TEST_P(RouterBootstrapBootstrapNoGroupReplicationSetup,
       BootstrapNoGroupReplicationSetup) {
  const auto param = GetParam();

  std::vector<Config> config{
      // member-1: schema exists, but no group replication configured
      {
          "127.0.0.1",
          port_pool_.get_next_available(),
          port_pool_.get_next_available(),
          get_data_dir().join(param.trace_file).str(),
      },
  };

  ASSERT_NO_FATAL_FAILURE(
      bootstrap_failover(config, param.cluster_type, {}, EXIT_FAILURE,
                         {"to have Group Replication running"}));
}

INSTANTIATE_TEST_SUITE_P(
    BootstrapNoGroupReplicationSetup,
    RouterBootstrapBootstrapNoGroupReplicationSetup,
    ::testing::Values(BootstrapTestParam{ClusterType::GR_V2, "gr",
                                         "bootstrap_no_gr.js", "", ""},
                      BootstrapTestParam{ClusterType::GR_V1, "gr_v1",
                                         "bootstrap_no_gr_v1.js", "", ""}),
    get_test_description);

/**
 * @test
 *       ensure a reasonable error message if metadata schema does not exist.
 */
TEST_F(RouterBootstrapTest, BootstrapNoMetadataSchema) {
  std::vector<Config> config{
      // member-1: no metadata schema
      {
          "127.0.0.1",
          port_pool_.get_next_available(),
          port_pool_.get_next_available(),
          get_data_dir().join("bootstrap_no_schema.js").str(),
      },
  };

  ASSERT_NO_FATAL_FAILURE(
      bootstrap_failover(config, ClusterType::GR_V2, {}, EXIT_FAILURE,
                         {"to contain the metadata of MySQL InnoDB Cluster"}));
}

/**
 * @test
 *        verify connection times at bootstrap can be configured
 */
TEST_F(RouterBootstrapTest, BootstrapFailWhenServerResponseExceedsReadTimeout) {
  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_exec_time_2_seconds.js").str()},
  };

  std::vector<std::string> router_options = {
      "--bootstrap=" + mock_servers.at(0).ip + ":" +
          std::to_string(mock_servers.at(0).port),
      "-d", bootstrap_dir.name(), "--connect-timeout=1", "--read-timeout=1"};

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(
      mock_servers, ClusterType::GR_V2, router_options, EXIT_FAILURE,
      {"Error: Error executing MySQL query \".*\": Lost connection to "
       "MySQL server during query \\(2013\\)"}));
}

/**
 * @test
 *       verify that bootstrap succeeds when master key writer is used
 *
 */
TEST_F(RouterBootstrapTest,
       NoMasterKeyFileWhenBootstrapPassWithMasterKeyReader) {
  std::vector<Config> config{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_gr.js").str()},
  };

  ScriptGenerator script_generator(ProcessManager::get_origin(),
                                   get_test_temp_dir_name());

  std::vector<std::string> router_options = {
      "--bootstrap=" + config.at(0).ip + ":" +
          std::to_string(config.at(0).port),
      "--report-host",
      my_hostname,
      "-d",
      bootstrap_dir.name(),
      "--master-key-reader=" + script_generator.get_reader_script(),
      "--master-key-writer=" + script_generator.get_writer_script()};

  ASSERT_NO_FATAL_FAILURE(
      bootstrap_failover(config, ClusterType::GR_V2, router_options));

  Path tmp(bootstrap_dir.name());
  Path master_key_file(tmp.join("mysqlrouter.key").str());
  ASSERT_FALSE(master_key_file.exists());

  Path keyring_file(tmp.join("data").join("keyring").str());
  ASSERT_TRUE(keyring_file.exists());

  Path dir(get_test_temp_dir_name());
  Path data_file(dir.join("master_key").str());
  ASSERT_TRUE(data_file.exists());
}

/**
 * @test
 *       verify that master key file is not overridden by subsequent bootstrap.
 */
TEST_F(RouterBootstrapTest, MasterKeyFileNotChangedAfterSecondBootstrap) {
  std::string master_key_path =
      Path(bootstrap_dir.name()).join("master_key").str();
  std::string keyring_path =
      Path(bootstrap_dir.name()).join("data").join("keyring").str();

  mysql_harness::mkdir(Path(bootstrap_dir.name()).str(), 0777);
  mysql_harness::mkdir(Path(bootstrap_dir.name()).join("data").str(), 0777);

  auto &proc = launch_command(get_origin().join("mysqlrouter_keyring").str(),
                              {
                                  "init",
                                  keyring_path,
                                  "--master-key-file",
                                  master_key_path,
                              });
  ASSERT_NO_THROW(proc.wait_for_exit());

  std::string master_key;
  {
    std::ifstream file(master_key_path);
    std::stringstream iss;
    iss << file.rdbuf();
    master_key = iss.str();
  }

  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_gr.js").str()},
  };

  std::vector<std::string> router_options = {
      "--bootstrap=" + mock_servers.at(0).ip + ":" +
          std::to_string(mock_servers.at(0).port),
      "--report-host",
      my_hostname,
      "-d",
      bootstrap_dir.name(),
      "--force"};

  ASSERT_NO_FATAL_FAILURE(bootstrap_failover(mock_servers, ClusterType::GR_V2,
                                             router_options, EXIT_SUCCESS, {}));
  {
    std::ifstream file(master_key_path);
    std::stringstream iss;
    iss << file.rdbuf();
    ASSERT_THAT(master_key, testing::Eq(iss.str()));
  }
}

struct UseGrNotificationTestParams {
  std::vector<std::string> bootstrap_params;
  std::vector<std::string> expected_config_lines;
  mysqlrouter::MetadataSchemaVersion metadata_schema_version;
};

class ConfUseGrNotificationParamTest
    : public RouterBootstrapTest,
      public ::testing::WithParamInterface<UseGrNotificationTestParams> {};

/**
 * @test
 *       verify that using --conf-use-gr-notifications creates proper config
 * file entry.
 */
TEST_P(ConfUseGrNotificationParamTest, ConfUseGrNotificationParam) {
  const auto server_port = port_pool_.get_next_available();
  const auto server_x_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  // launch mock server that is our metadata server for the bootstrap
  auto &server_mock = launch_mysql_server_mock(json_stmts, server_port,
                                               EXIT_SUCCESS, false, http_port);

  set_mock_bootstrap_data(http_port, "test", {{"localhost", server_port}},
                          GetParam().metadata_schema_version,
                          "cluster-specific-id");

  const auto router_port_rw = port_pool_.get_next_available();
  const auto router_port_ro = port_pool_.get_next_available();
  const auto router_port_x_rw = port_pool_.get_next_available();
  const auto router_port_x_ro = port_pool_.get_next_available();
  std::vector<std::string> bootsrtap_params{
      "--bootstrap=127.0.0.1:" + std::to_string(server_port),
      "-d",
      bootstrap_dir.name(),
      "--conf-set-option=routing:bootstrap_rw.bind_port=" +
          std::to_string(router_port_rw),
      "--conf-set-option=routing:bootstrap_ro.bind_port=" +
          std::to_string(router_port_ro),
      "--conf-set-option=routing:bootstrap_x_rw.bind_port=" +
          std::to_string(router_port_x_rw),
      "--conf-set-option=routing:bootstrap_x_ro.bind_port=" +
          std::to_string(router_port_x_ro)};

  bootsrtap_params.insert(bootsrtap_params.end(),
                          GetParam().bootstrap_params.begin(),
                          GetParam().bootstrap_params.end());

  // launch the router in bootstrap mode
  auto &router = launch_router_for_bootstrap(bootsrtap_params);

  check_exit_code(router, EXIT_SUCCESS);

  const std::string conf_file = bootstrap_dir.name() + "/mysqlrouter.conf";

  // check if valid config option was added to the file
  auto conf_file_content = get_file_output(conf_file);
  auto conf_lines = mysql_harness::split_string(conf_file_content, '\n');
  EXPECT_THAT(conf_lines,
              ::testing::IsSupersetOf(GetParam().expected_config_lines));
  server_mock.send_clean_shutdown_event();
  EXPECT_NO_THROW(server_mock.wait_for_exit());

  auto plugin_dir = mysql_harness::get_plugin_dir(get_origin().str());
  ASSERT_TRUE(add_line_to_config_file(conf_file, "DEFAULT", "plugin_folder",
                                      plugin_dir));

  const std::string runtime_json_stmts =
      get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str();

  // launch mock server that is our metadata server
  launch_mysql_server_mock(runtime_json_stmts, server_port, EXIT_SUCCESS, false,
                           http_port);
  set_mock_metadata(http_port, "cluster-specific-id", {GRNode{server_port}}, 0,
                    {ClusterNode{server_port, server_x_port}});

  // check that the Router accepts the config file
  auto &router2 = launch_router({"-c", conf_file});
  router2.set_logging_path(bootstrap_dir.name() + "/log", "mysqlrouter.log");
}

INSTANTIATE_TEST_SUITE_P(
    ConfUseGrNotificationParam, ConfUseGrNotificationParamTest,
    ::testing::Values(
        // 0, 1) --conf-use-gr-notifications with no param
        UseGrNotificationTestParams{{"--conf-use-gr-notifications"},
                                    {"use_gr_notifications=1", "ttl=60",
                                     "auth_cache_refresh_interval=60"},
                                    {2, 0, 3}},
        UseGrNotificationTestParams{{"--conf-use-gr-notifications"},
                                    {"use_gr_notifications=1", "ttl=60",
                                     "auth_cache_refresh_interval=60"},
                                    {2, 1, 0}},
        // 2, 3) --conf-use-gr-notifications=1
        // [@FR5.2.2]
        UseGrNotificationTestParams{{"--conf-use-gr-notifications=1"},
                                    {"use_gr_notifications=1", "ttl=60",
                                     "auth_cache_refresh_interval=60"},
                                    {2, 0, 3}},
        UseGrNotificationTestParams{{"--conf-use-gr-notifications=1"},
                                    {"use_gr_notifications=1", "ttl=60",
                                     "auth_cache_refresh_interval=60"},
                                    {2, 1, 0}},
        // 4, 5) no --conf-use-gr-notifications param
        UseGrNotificationTestParams{{},
                                    {"use_gr_notifications=0", "ttl=0.5",
                                     "auth_cache_refresh_interval=2"},
                                    {2, 0, 3}},
        UseGrNotificationTestParams{{},
                                    {"use_gr_notifications=0", "ttl=0.5",
                                     "auth_cache_refresh_interval=2"},
                                    {2, 1, 0}},
        // 6, 7) --conf-use-gr-notification=0
        // [@FR5.2.1]
        UseGrNotificationTestParams{{"--conf-use-gr-notifications=0"},
                                    {"use_gr_notifications=0", "ttl=0.5",
                                     "auth_cache_refresh_interval=2"},
                                    {2, 0, 3}},
        UseGrNotificationTestParams{{"--conf-use-gr-notifications=0"},
                                    {"use_gr_notifications=0", "ttl=0.5",
                                     "auth_cache_refresh_interval=2"},
                                    {2, 1, 0}}));

class ErrorReportTest : public RouterComponentBootstrapTest {};

/**
 * @test
 *        verify that --conf-use-gr-notifications used with no bootstrap
 *        causes proper error report
 */
TEST_F(ErrorReportTest, ConfUseGrNotificationsNoBootstrap) {
  auto &router = launch_router_for_bootstrap({"--conf-use-gr-notifications"},
                                             EXIT_FAILURE);

  EXPECT_NO_THROW(router.wait_for_exit());
  EXPECT_THAT(
      router.get_full_output(),
      ::testing::HasSubstr("Error: Option --conf-use-gr-notifications can only "
                           "be used together with -B/--bootstrap"));
  check_exit_code(router, EXIT_FAILURE);
}

class ConfUseGrNotificationWrongValueParamTest
    : public RouterBootstrapTest,
      public ::testing::WithParamInterface<std::string> {};

/**
 * @test
 *        verify that --conf-use-gr-notifications used with value other than 0
 * and 1 causes proper error report
 * [@FR5.2.4]
 */
TEST_P(ConfUseGrNotificationWrongValueParamTest,
       ConfUseGrNotificationWrongValueParam) {
  auto &router = launch_router_for_bootstrap(
      {"-B", "somehost:12345", "--conf-use-gr-notifications=" + GetParam()},
      EXIT_FAILURE);

  EXPECT_NO_THROW(router.wait_for_exit());
  EXPECT_THAT(router.get_full_output(),
              ::testing::HasSubstr(
                  "Error: Value for parameter '--conf-use-gr-notifications' "
                  "needs to be one of: ['0', '1']"));
  check_exit_code(router, EXIT_FAILURE);
}

INSTANTIATE_TEST_SUITE_P(ConfUseGrNotificationWrongValueParam,
                         ConfUseGrNotificationWrongValueParamTest,
                         ::testing::Values("2", "true", "false", "N/A", "yes",
                                           "no"));

/**
 * @test
 *       verify that running bootstrap with -d with dir that already exists and
 *       is not empty gives an appropriate error to the user; particularly it
 *       should mention:
 *         - directory name
 *         - error type (it's not empty)
 */
TEST_F(ErrorReportTest, bootstrap_dir_exists_and_is_not_empty) {
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();
  const uint16_t server_port = port_pool_.get_next_available();

  TempDirectory bootstrap_directory;

  // populate bootstrap dir with a file, so it's not empty
  EXPECT_NO_THROW({
    mysql_harness::Path path =
        mysql_harness::Path(bootstrap_directory.name()).join("some_file");
    std::ofstream of(path.str());
    of << "blablabla";
  });

  // launch the router in bootstrap mode
  auto &router = launch_router_for_bootstrap(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--connect-timeout=1",
          "--report-host",
          my_hostname,
          "-d",
          bootstrap_directory.name(),
      },
      EXIT_FAILURE);

  // verify that appropriate message was logged (first line) and error message
  // printed (last line)
  std::string err_msg = "Directory '" + bootstrap_directory.name() +
                        "' already contains files\n"
                        "Error: Directory already exits";

  check_exit_code(router, EXIT_FAILURE);
}

TEST_F(ErrorReportTest, bootstrap_conf_base_port_hex) {
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();
  const uint16_t server_port = port_pool_.get_next_available();

  TempDirectory bootstrap_directory;

  // launch the router in bootstrap mode
  auto &router = launch_router_for_bootstrap(
      {
          "--bootstrap", "127.0.0.1:" + std::to_string(server_port),  //
          "--connect-timeout", "1",                                   //
          "--conf-base-port", "0x0",                                  //
          "--report-host", my_hostname,                               //
          "-d", bootstrap_directory.name(),                           //
      },
      EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_THAT(router.get_full_output(),
              ::testing::HasSubstr("--conf-base-port needs value between 0 and "
                                   "65532 inclusive, was '0x0'"));
}

// unfortunately it's not (reasonably) possible to make folders read-only on
// Windows, therefore we can run the following tests only on Unix
//
// https://support.microsoft.com/en-us/help/326549/you-cannot-view-or-change-the-read-only-or-the-system-attributes-of-fo
#ifndef _WIN32
/**
 * @test
 *       verify that running bootstrap with -d with dir that already exists but
 *       is inaccessible gives an appropriate error to the user; particularly it
 *       should mention:
 *         - directory name
 *         - error type (permission denied)
 *         - suggests AppArmor config might be at fault
 */
TEST_F(ErrorReportTest, bootstrap_dir_exists_but_is_inaccessible) {
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();
  const uint16_t server_port = port_pool_.get_next_available();

  TempDirectory bootstrap_directory;
  std::shared_ptr<void> exit_guard(nullptr, [&](void *) {
    chmod(bootstrap_directory.name().c_str(),
          S_IRUSR | S_IWUSR | S_IXUSR);  // restore RWX for owner
  });

  // make bootstrap directory inaccessible to trigger the error
  EXPECT_EQ(chmod(bootstrap_directory.name().c_str(), 0), 0);

  // launch the router in bootstrap mode: -d set to existing but inaccessible
  // dir
  auto &router = launch_router_for_bootstrap(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--connect-timeout=1",
          "--report-host",
          my_hostname,
          "-d",
          bootstrap_directory.name(),
      },
      EXIT_FAILURE);

  // verify that appropriate message was logged (all but last) and error message
  // printed (last line)
  std::string err_msg =
      "Failed to open directory '.*" + bootstrap_directory.name() +
      "': Permission denied\n"
      "This may be caused by insufficient rights or AppArmor settings.\n.*"
      "Error: Could not check contents of existing deployment directory";

  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 *       verify that running bootstrap with -d with dir that doesn't exists and
 *       cannot be created gives an appropriate error to the user; particularly
 *       it should mention:
 *         - directory name
 *         - error type (permission denied)
 *         - suggests AppArmor config might be at fault
 */
TEST_F(ErrorReportTest,
       bootstrap_dir_does_not_exist_and_is_impossible_to_create) {
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();
  const uint16_t server_port = port_pool_.get_next_available();

  TempDirectory bootstrap_superdir;
  std::shared_ptr<void> exit_guard(nullptr, [&](void *) {
    chmod(bootstrap_superdir.name().c_str(),
          S_IRUSR | S_IWUSR | S_IXUSR);  // restore RWX for owner
  });

  // make bootstrap directory inaccessible to trigger the error
  EXPECT_EQ(chmod(bootstrap_superdir.name().c_str(), 0), 0);

  // launch the router in bootstrap mode: -d set to non-existent dir and
  // impossible to create
  std::string bootstrap_directory =
      mysql_harness::Path(bootstrap_superdir.name()).join("subdir").str();
  auto &router = launch_router_for_bootstrap(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--connect-timeout=1",
          "--report-host",
          my_hostname,
          "-d",
          bootstrap_directory,
      },
      EXIT_FAILURE);

  // verify that appropriate message was logged (all but last) and error message
  // printed (last line)
  std::string err_msg =
      "Cannot create directory '" + bootstrap_directory +
      "': Permission denied\n"
      "This may be caused by insufficient rights or AppArmor settings.\n.*"
      "Error: Could not create deployment directory";

  check_exit_code(router, EXIT_FAILURE);
}
#endif

/**
 * @test
 *       verify that using --conf-use-gr-notifications creates proper error when
 * the cluster type is ReplicaSet.
 */
TEST_F(ErrorReportTest, ConfUseGrNotificationsAsyncReplicaset) {
  TempDirectory bootstrap_directory;
  const auto server_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("bootstrap_ar.js").str();

  // launch mock server that is our metadata server for the bootstrap
  launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);

  // launch the router in bootstrap mode
  auto &router = launch_router_for_bootstrap(
      {"--bootstrap=127.0.0.1:" + std::to_string(server_port), "-d",
       bootstrap_directory.name(), "--conf-use-gr-notifications"},
      EXIT_FAILURE);

  EXPECT_NO_THROW(router.wait_for_exit());
  EXPECT_THAT(
      router.get_full_output(),
      ::testing::HasSubstr("Error: The parameter 'use-gr-notifications' is "
                           "valid only for GR cluster type"));
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 *       verify that trying to register that is not unique in the metadata
 * gives expected results.
 */
TEST_F(RouterBootstrapTest, BootstrapRouterDuplicateEntry) {
  TempDirectory bootstrap_directory;
  const auto server_port = port_pool_.get_next_available();
  const auto bootstrap_server_port = port_pool_.get_next_available();
  // const auto server_http_port = port_pool_.get_next_available();
  const auto bootstrap_server_http_port = port_pool_.get_next_available();
  const std::string json_stmts =
      get_data_dir().join("bootstrap_gr_dup_router.js").str();

  // launch mock server that is our metadata server for the bootstrap
  // auto &server_mock =
  launch_mysql_server_mock(json_stmts, bootstrap_server_port, EXIT_SUCCESS,
                           false, bootstrap_server_http_port);
  set_mock_bootstrap_data(bootstrap_server_http_port, "test",
                          {{"127.0.0.1", server_port}}, {2, 0, 3},
                          "cluster-specific-id");

  // launch the router in bootstrap mode
  auto &router = launch_router_for_bootstrap(
      {"--bootstrap=127.0.0.1:" + std::to_string(bootstrap_server_port), "-d",
       bootstrap_directory.name()},
      EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  // there should be an errors about duplicate router entry
  EXPECT_TRUE(router.expect_output(
      "Error: It appears that a router instance named '' has been previously "
      "configured in this host. If that instance no longer exists, use the "
      "--force option to overwrite it.",
      false, 0ms));

  // there should be no errors about not being able to remove dirs nor files
  EXPECT_FALSE(
      router.expect_output("Could not delete directory .*", true, 0ms));

  EXPECT_FALSE(router.expect_output("Could not delete file .*", true, 0ms));
}

TEST_F(RouterBootstrapTest, CheckAuthBackendWhenOldMetadata) {
  TempDirectory bootstrap_directory;
  const auto server_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  const std::string json_stmts =
      get_data_dir().join("bootstrap_gr_v1.js").str();

  // launch mock server that is our metadata server for the bootstrap
  launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false,
                           http_port);

  set_mock_bootstrap_data(http_port, "test", {{"localhost", server_port}},
                          {1, 0, 0}, "cluster-specific-id");

  const auto base_listening_port = port_pool_.get_next_available();
  std::vector<std::string> bootsrtap_params{
      "--bootstrap=127.0.0.1:" + std::to_string(server_port), "-d",
      bootstrap_directory.name(),
      "--conf-base-port=" + std::to_string(base_listening_port)};

  // launch the router in bootstrap mode
  auto &router = launch_router_for_bootstrap(bootsrtap_params, EXIT_SUCCESS,
                                             /*disable rest*/ false);

  check_exit_code(router, EXIT_SUCCESS);

  const std::string conf_file =
      bootstrap_directory.name() + "/mysqlrouter.conf";

  // check if valid authentication backend option was added to the config file
  auto conf_file_content = get_file_output(conf_file);
  auto conf_lines = mysql_harness::split_string(conf_file_content, '\n');
  const auto passwd_file = mysql_harness::Path{
      bootstrap_directory.name() + "/data/auth_backend_passwd_file"};
  EXPECT_THAT(conf_lines,
              ::testing::IsSupersetOf(
                  {::testing::ContainsRegex("backend=file"),
                   ::testing::ContainsRegex(std::string{"filename=.*"} +
                                            passwd_file.str())}));
  ASSERT_TRUE(passwd_file.exists());
}

class ConfSetOptionTest : public RouterBootstrapTest {};

/**
 * @test
 *       verify that using --conf-set-option for not bootstrap gives a proper
 * error
 */
TEST_F(ConfSetOptionTest, ErrorIfNotBootstrap) {
  const std::string tracefile = "bootstrap_gr.js";

  std::vector<std::string> cmdline = {
      "--conf-set-option=DEFAULT.max_total_connections=1024"};

  auto &router = launch_router_for_bootstrap(cmdline, EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  // let's check if the expected error was reported:
  EXPECT_THAT(
      router.get_full_output(),
      ::testing::ContainsRegex("Error: Option --conf-set-option can only be "
                               "used together with -B/--bootstrap"));
}

/**
 * @test
 *       verify that the --conf-set-option bootstrap parameter is handled
 * properly when used to set bind port of each route along with other config
 * options
 */
TEST_F(ConfSetOptionTest, MultipleConfOptionsSet) {
  const std::string tracefile = "bootstrap_gr.js";

  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), get_data_dir().join(tracefile).str()},
  };

  // mysqlrouter -B ...
  // --conf-set-option=routing:bootstrap_rw.bind_port=A -
  // --conf-set-option=routing:bootstrap_ro.bind_port=B
  // --conf-set-option=routing:bootstrap_x_rw.bind_port=C
  // --conf-set-option=routing:bootstrap_x_ro.bind_port=D
  // --conf-set-option=logger.level=DEBUG
  // --conf-set-option=DEFAULT.read_timeout=50
  // --conf-set-option=DEFAULT.connect_timeout=38
  // --conf-set-option=DEFAULT.unknown_config_option=warning

  const uint16_t classic_rw_port = 1234;
  const uint16_t classic_ro_port = 2345;
  const uint16_t x_rw_port = 2222;
  const uint16_t x_ro_port = 3333;
  const std::string log_level = "DEBUG";
  const int read_tout = 50;
  const int connect_tout = 38;

  std::vector<std::string> cmdline = {
      "--bootstrap=" + mock_servers.at(0).ip + ":" +
          std::to_string(mock_servers.at(0).port),
      "-d",
      bootstrap_dir.name(),
      "--conf-set-option=routing:bootstrap_rw.bind_port=" +
          std::to_string(classic_rw_port),
      "--conf-set-option=routing:bootstrap_ro.bind_port=" +
          std::to_string(classic_ro_port),
      "--conf-set-option=routing:bootstrap_x_rw.bind_port=" +
          std::to_string(x_rw_port),
      "--conf-set-option=routing:bootstrap_x_ro.bind_port=" +
          std::to_string(x_ro_port),
      "--conf-set-option=logger.level=" + log_level,
      "--conf-set-option=DEFAULT.read_timeout=" + std::to_string(read_tout),
      "--conf-set-option=DEFAULT.connect_timeout=" +
          std::to_string(connect_tout),
      "--conf-set-option=DEFAULT.unknown_config_option=warning"};

  ASSERT_NO_FATAL_FAILURE(
      bootstrap_failover(mock_servers, ClusterType::GR_V2, cmdline));

  // 'config_file' is set as side-effect of bootstrap_failover()
  ASSERT_THAT(config_file, ::testing::Not(::testing::IsEmpty()));

  // let's check if the actual config file contains what we expect:
  const std::string config_file_str = get_file_output(config_file);

  // classic RW
  check_bind_port(config_file_str, "bootstrap_rw", "classic", "PRIMARY",
                  classic_rw_port);

  // classic RO
  check_bind_port(config_file_str, "bootstrap_ro", "classic", "SECONDARY",
                  classic_ro_port);

  // x RW
  check_bind_port(config_file_str, "bootstrap_x_rw", "x", "PRIMARY", x_rw_port);

  // x RO
  check_bind_port(config_file_str, "bootstrap_x_ro", "x", "SECONDARY",
                  x_ro_port);

  EXPECT_TRUE(config_file_contains(config_file_str, "level=" + log_level))
      << config_file_str;
  EXPECT_TRUE(config_file_contains(config_file_str,
                                   "read_timeout=" + std::to_string(read_tout)))
      << config_file_str;
  EXPECT_TRUE(config_file_contains(
      config_file_str, "connect_timeout=" + std::to_string(connect_tout)))
      << config_file_str;

  EXPECT_TRUE(
      config_file_contains(config_file_str, "unknown_config_option=warning"))
      << config_file_str;
  EXPECT_FALSE(
      config_file_contains(config_file_str, "unknown_config_option=error"))
      << config_file_str;
}

struct ConfSetOptionErrorTestParam {
  std::vector<std::string> con_set_option_params;
  std::string expected_error;
};

class ConfSetOptionErrorTest
    : public ConfSetOptionTest,
      public ::testing::WithParamInterface<ConfSetOptionErrorTestParam> {};

TEST_P(ConfSetOptionErrorTest, ErrorTest) {
  const std::string tracefile = get_data_dir().join("bootstrap_gr.js").str();
  const auto mock_server_port = port_pool_.get_next_available();

  launch_mysql_server_mock(tracefile, mock_server_port, EXIT_SUCCESS, false);

  std::vector<std::string> cmdline = {
      "--bootstrap=127.0.0.1:" + std::to_string(mock_server_port), "-d",
      bootstrap_dir.name()};

  for (const auto &param : GetParam().con_set_option_params) {
    cmdline.push_back(param);
  }

  auto &router = launch_router_for_bootstrap(cmdline, EXIT_FAILURE);
  check_exit_code(router, EXIT_FAILURE);

  // let's check if the expected error was reported:
  EXPECT_THAT(router.get_full_output(),
              ::testing::ContainsRegex(GetParam().expected_error));
}

INSTANTIATE_TEST_SUITE_P(
    ErrorTest, ConfSetOptionErrorTest,
    ::testing::Values(
        ConfSetOptionErrorTestParam{
            {"--conf-set-option=:test_rw.bind_port=6666"},
            "Error: conf-set-option: invalid section name ':test_rw'"},

        ConfSetOptionErrorTestParam{
            {"--conf-set-option=routing:=6666"},
            "Error: conf-set-option: invalid option 'routing:=6666', should be "
            "section.option_name=value"},

        ConfSetOptionErrorTestParam{
            {"--conf-set-option=.para=value"},
            "Error: conf-set-option: invalid section name ''"},

        ConfSetOptionErrorTestParam{
            {"--conf-set-option=.:="},
            "Error: conf-set-option: invalid section name ''"},

        ConfSetOptionErrorTestParam{
            {"--conf-set-option=:.="},
            "Error: conf-set-option: invalid section name ':'"},

        ConfSetOptionErrorTestParam{
            {"--conf-set-option=DEFAULT.read_timeout=1",
             "--conf-set-option=DEFAULT.read_timeout=1"},
            "Error: conf-set-option: duplicate value for option "
            "'default.read_timeout'"},

        ConfSetOptionErrorTestParam{
            {"--conf-set-option=DEFAULT.read_timeout=1",
             "--conf-set-option=DEFAULT.read_timeout=2"},
            "Error: conf-set-option: duplicate value for option "
            "'default.read_timeout'"},

        ConfSetOptionErrorTestParam{
            {"--conf-set-option=DEFAULT.connect_timeout=1",
             "--connect-timeout=20",
             "--conf-set-option=DEFAULT.connect_timeout=3"},
            "Error: conf-set-option: duplicate value for option "
            "'default.connect_timeout'"},

        ConfSetOptionErrorTestParam{
            {"--conf-set-option=MySection:AB.read_timeout=1",
             "--conf-set-option=mysection:ab.read_TimeOut=2"},
            "Error: conf-set-option: duplicate value for option "
            "'mysection:ab.read_timeout'"},

        ConfSetOptionErrorTestParam{
            {"--conf-set-option=DEFAULT.read_timeout=1",
             "--conf-set-option=DEFAULT.read_timeout=2",
             "--conf-set-option=DEFAULT.read_timeout=3"},
            "Error: conf-set-option: duplicate value for option "
            "'default.read_timeout'"},

        ConfSetOptionErrorTestParam{
            {"--conf-set-option=DEFAULT.=xx"},
            "Error: conf-set-option: invalid option name ''"},

        ConfSetOptionErrorTestParam{
            {"--conf-set-option=DEFAULT.:=xx"},
            "Error: conf-set-option: invalid option name ':'"},

        ConfSetOptionErrorTestParam{{"--conf-set-option=DEFAULT:.option=xx"},
                                    "Error: conf-set-option: DEFAULT section "
                                    "is not allowed to have a key: 'DEFAULT:"},

        ConfSetOptionErrorTestParam{
            {"--conf-set-option=DEFAULT:aa.option=xx"},
            "Error: conf-set-option: DEFAULT section is not allowed to have a "
            "key: 'DEFAULT:aa'"},

        ConfSetOptionErrorTestParam{
            {"--conf-set-option=abc"},
            "Error: conf-set-option: invalid option 'abc', should be "
            "section.option_name=value"}));

struct ConfSetOptionTestParam {
  std::vector<std::string> bootstrap_params;
  std::vector<std::string> expected_conf_entries;
  std::vector<std::string> unexpected_conf_entries;
};

class ConfSetOptionParamTest
    : public ConfSetOptionTest,
      public ::testing::WithParamInterface<ConfSetOptionTestParam> {};

TEST_P(ConfSetOptionParamTest, Spec) {
  const std::string tracefile = get_data_dir().join("bootstrap_gr.js").str();
  const auto mock_server_port = port_pool_.get_next_available();
  launch_mysql_server_mock(tracefile, mock_server_port, EXIT_SUCCESS, false);

  std::vector<std::string> cmdline = {
      "--bootstrap=127.0.0.1:" + std::to_string(mock_server_port), "-d",
      bootstrap_dir.name()};
  // add parameters passed by the testcase
  cmdline.insert(cmdline.end(), GetParam().bootstrap_params.begin(),
                 GetParam().bootstrap_params.end());

  auto &router = launch_router_for_bootstrap(cmdline, EXIT_SUCCESS, false);

  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_SUCCESS));

  config_file = bootstrap_dir.name() + "/mysqlrouter.conf";
  const std::string config_file_str = get_file_output(config_file);

  // check that expected entries are in the config file
  for (const auto &entry : GetParam().expected_conf_entries) {
    EXPECT_TRUE(config_file_contains(config_file_str, entry))
        << entry << "\n"
        << config_file_str;
  }

  // check that unexpected entries are NOT in the config file
  for (const auto &entry : GetParam().unexpected_conf_entries) {
    EXPECT_FALSE(config_file_contains(config_file_str, entry))
        << entry << "\n"
        << config_file_str;
  }
}

/**
 * @test
 *       verify that the --conf-set-option bootstrap parameter has precedence
 * over other existing bootstrap options setting configuration values
 */
INSTANTIATE_TEST_SUITE_P(
    OverwriteTest, ConfSetOptionParamTest,
    ::testing::Values(
        ConfSetOptionTestParam{
            {"--connect-timeout=20",
             "--conf-set-option=DEFAULT.connect_timeout=1"},
            /*expected_conf_entries=*/{"connect_timeout=1"},
            /*unexpected_conf_entries=*/{"connect_timeout=20"}},
        ConfSetOptionTestParam{
            {"--connect-timeout=1",
             "--conf-set-option=DEFAULT.connect_timeout=20"},
            /*expected_conf_entries=*/{"connect_timeout=20"},
            /*unexpected_conf_entries=*/{"connect_timeout=1"}},
        ConfSetOptionTestParam{
            {"--read-timeout=20", "--conf-set-option=DEFAULT.read_timeout=1"},
            /*expected_conf_entry=*/{"read_timeout=1"},
            /*unexpected_conf_entry=*/{"read_timeout=20"}},
        ConfSetOptionTestParam{
            {"--conf-base-port=1000",
             "--conf-set-option=routing:bootstrap_rw.bind_port=2000"},
            /*expected_conf_entries=*/{"bind_port=2000"},
            /*unexpected_conf_entries=*/{"bind_port=1000"}},
        // ConfSetOptionTestParam{
        //     {"--ssl-mode=REQUIRED",
        //      "--conf-set-option=metadata_cache:bootstrap.ssl_mode=DISABLED"},
        //     /*expected_conf_entries=*/{"ssl_mode=DISABLED"},
        //     /*unexpected_conf_entries=*/{"ssl-mode=REQUIRED"}}
        ConfSetOptionTestParam{
            {"--https-port=101", "--conf-set-option=http_server.port=202"},
            /*expected_conf_entries=*/{"port=202"},
            /*unexpected_conf_entries=*/{"port=101"}},
        ConfSetOptionTestParam{
            {"--name=Router01", "--conf-set-option=DEFAULT.name=Router02"},
            /*expected_conf_entries=*/{"name=Router02"},
            /*unexpected_conf_entries=*/{"name=Router01"}}));

/**
 * @test
 *       verify that the --conf-set-option section name and option name are case
 * insensitive
 */
INSTANTIATE_TEST_SUITE_P(
    CaseSensitivity, ConfSetOptionParamTest,
    ::testing::Values(
        ConfSetOptionTestParam{
            {"--conf-set-option=DEFAULt.read_timeout=1"},
            /*expected_conf_entries=*/{"[DEFAULT]", "read_timeout=1"},
            /*unexpected_conf_entries=*/{"[DEFAULt]", "[default]"}},

        ConfSetOptionTestParam{
            {"--conf-set-option=default.connect_timeout=15"},
            /*expected_conf_entries=*/{"[DEFAULT]", "connect_timeout=15"},
            /*unexpected_conf_entries=*/{"[default]"}},

        ConfSetOptionTestParam{
            {"--conf-set-option=LOGGER.level=DEBUG"},
            /*expected_conf_entries=*/{"[logger]", "level=DEBUG"},
            /*unexpected_conf_entries=*/{"[LOGGER]", "level=debug"}},

        ConfSetOptionTestParam{
            {"--conf-set-option=METADATA_cache:BOOTSTRAP.router_id=1"},
            /*expected_conf_entries=*/
            {"[metadata_cache:bootstrap]", "router_id=1"},
            /*unexpected_conf_entries=*/
            {"[METADATA_cache:BOOTSTRAP]", "[metadata_cache:BOOTSTRAP]"}},

        ConfSetOptionTestParam{
            {"--conf-set-option=test_section.para1=10",
             "--conf-set-option=test_Section.para2=20",
             "--conf-set-option=TEST_SECTION.para3=30"},
            /*expected_conf_entries=*/
            {"[test_section]", "para1=10", "para2=20", "para3=30"},
            /*unexpected_conf_entries=*/
            {"[test_Section]", "[TEST_SECTION]"}},

        ConfSetOptionTestParam{
            {"--conf-set-option=test_section:SUB.para1=10",
             "--conf-set-option=test_Section:Sub.para2=20",
             "--conf-set-option=TEST_SECTION:sub.para3=30"},
            /*expected_conf_entries=*/
            {"[test_section:sub]", "para1=10", "para2=20", "para3=30"},
            /*unexpected_conf_entries=*/
            {"[test_section:SUB]", "[TEST_SECTION:sub]", "[TEST_SECTION:sub]"}},

        ConfSetOptionTestParam{{"--conf-set-option=DEFAULT.READ_TIMEOUT=1"},
                               /*expected_conf_entries=*/
                               {"read_timeout=1"},
                               /*unexpected_conf_entries=*/
                               {"READ_TIMEOUT=1"}},

        ConfSetOptionTestParam{{"--conf-set-option=DEFAULT.READ_Timeout=1"},
                               /*expected_conf_entries=*/
                               {"read_timeout=1"},
                               /*unexpected_conf_entries=*/
                               {"READ_Timeout=1"}},

        ConfSetOptionTestParam{{"--conf-set-option=test_section.para1=10",
                                "--conf-set-option=test_section.Para2=20",
                                "--conf-set-option=test_section.PARA3=30"},
                               /*expected_conf_entries=*/
                               {"para1=10", "para2=20", "para3=30"},
                               /*unexpected_conf_entries=*/
                               {"Para2=10", "PARA3=20"}},

        ConfSetOptionTestParam{{"--conf-set-option=DEFAULT.name=\"My Router\""},
                               /*expected_conf_entries=*/
                               {"name=\"My Router\""},
                               /*unexpected_conf_entries=*/
                               {}},

        ConfSetOptionTestParam{
            {"--name=\"My Router\"",
             "--conf-set-option=DEFAULT.name=\"other router\""},
            /*expected_conf_entries=*/
            {"name=\"other router\""},
            /*unexpected_conf_entries=*/
            {"name=\"My Router\""}},

        ConfSetOptionTestParam{{"--name=\"My Router\"",
                                "--conf-set-option=DEFAULT.name=\"MY router\""},
                               /*expected_conf_entries=*/
                               {"name=\"MY router\""},
                               /*unexpected_conf_entries=*/
                               {"name=\"My Router\""}}

        ));

/**
 * @test
 *       verify that using ssl options during the bootstrap creates the
 * configuration file that is usable by the Router
 */
TEST_F(RouterBootstrapTest, SSLOptions) {
  TempDirectory bootstrap_directory;
  const auto server_port = port_pool_.get_next_available();
  const auto server_port2 = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  // launch mock server that is our metadata server for the bootstrap
  auto &server_mock = launch_mysql_server_mock(json_stmts, server_port,
                                               EXIT_SUCCESS, false, http_port);

  set_mock_bootstrap_data(
      http_port, "test",
      {{"localhost", server_port}, {"localhost", server_port2}}, {2, 1, 0},
      "00000000-0000-0000-0000-0000000000g1");

  const auto router_port_rw = port_pool_.get_next_available();
  const auto router_port_ro = port_pool_.get_next_available();
  const auto router_port_x_rw = port_pool_.get_next_available();
  const auto router_port_x_ro = port_pool_.get_next_available();
  std::vector<std::string> bootsrtap_params{
      "--bootstrap=127.0.0.1:" + std::to_string(server_port),
      "-d",
      bootstrap_directory.name(),
      "--conf-set-option=routing:bootstrap_rw.bind_port=" +
          std::to_string(router_port_rw),
      "--conf-set-option=routing:bootstrap_ro.bind_port=" +
          std::to_string(router_port_ro),
      "--conf-set-option=routing:bootstrap_x_rw.bind_port=" +
          std::to_string(router_port_x_rw),
      "--conf-set-option=routing:bootstrap_x_ro.bind_port=" +
          std::to_string(router_port_x_ro),
      "--ssl-mode=disabled",
      "--ssl-cipher=some",
      "--tls-version=TLSv1.2",
      "--ssl-ca=some",
      "--ssl-capath=some",
      "--ssl-crl=some",
      "--ssl-crlpath=some"};

  // launch the router in bootstrap mode
  auto &router = launch_router_for_bootstrap(bootsrtap_params);

  check_exit_code(router, EXIT_SUCCESS);

  const std::string conf_file =
      bootstrap_directory.name() + "/mysqlrouter.conf";

  std::vector<std::string> expected_config_lines{
      "ssl_mode=disabled", "ssl_cipher=some", "tls_version=TLSv1.2",
      "ssl_ca=some",       "ssl_capath=some", "ssl_crl=some",
      "ssl_crlpath=some"};

  // check if valid config options were added to the file
  auto conf_file_content = get_file_output(conf_file);
  auto conf_lines = mysql_harness::split_string(conf_file_content, '\n');
  EXPECT_THAT(conf_lines, ::testing::IsSupersetOf(expected_config_lines));
  server_mock.send_clean_shutdown_event();
  EXPECT_NO_THROW(server_mock.wait_for_exit());

  auto plugin_dir = mysql_harness::get_plugin_dir(get_origin().str());
  ASSERT_TRUE(add_line_to_config_file(conf_file, "DEFAULT", "plugin_folder",
                                      plugin_dir));

  const std::string runtime_json_stmts =
      get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str();

  // launch mock server that is our metadata server
  launch_mysql_server_mock(runtime_json_stmts, server_port, EXIT_SUCCESS, false,
                           http_port);
  set_mock_metadata(http_port, "00000000-0000-0000-0000-0000000000g1",
                    {server_port}, 0, {server_port});

  // check that the Router is running fine with this configuration file
  ASSERT_NO_FATAL_FAILURE(launch_router({"-c", conf_file}));
}

/**
 * @test
 *       verify that Router can be re-bootstrapped using the same directory if
 * the cluster name has changed in the meantime
 */
TEST_F(RouterComponentBootstrapTest, RouterReBootstrapClusetNameChange) {
  const std::string tracefile = "bootstrap_gr.js";

  const std::string kInitialClusterName = "initial_cluster_name";
  const std::string kChangedClusterName = "changed_cluster_name";

  const auto classic_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join(tracefile).str();
  launch_mysql_server_mock(json_stmts, classic_port, EXIT_SUCCESS, false,
                           http_port);

  set_mock_bootstrap_data(http_port, kInitialClusterName,
                          {{"localhost", classic_port}}, {2, 1, 0}, "gr-uuid");

  // do the first bootstrap
  std::vector<std::string> cmdline_bs = {"--bootstrap=root:"s + kRootPassword +
                                             "@localhost:"s +
                                             std::to_string(classic_port),
                                         "-d", bootstrap_dir.name()};

  auto &router_bs1 = launch_router_for_bootstrap(cmdline_bs);
  check_exit_code(router_bs1, EXIT_SUCCESS);

  // change the cluster name
  set_mock_bootstrap_data(http_port, kChangedClusterName,
                          {{"localhost", classic_port}}, {2, 1, 0}, "gr-uuid");

  // do the second bootstrap using the same directory
  auto &router_bs2 = launch_router_for_bootstrap(cmdline_bs);
  check_exit_code(router_bs2, EXIT_SUCCESS);
}

/**
 * @test
 *       verify that using --force-password-validation when bootstrapping works
 * ok
 */
TEST_F(RouterComponentBootstrapTest, ForcePasswordValidation) {
  const std::string tracefile = "bootstrap_gr_unhashed_passwd.js";

  const auto classic_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join(tracefile).str();
  launch_mysql_server_mock(json_stmts, classic_port, EXIT_SUCCESS, false,
                           http_port);

  set_mock_bootstrap_data(http_port, "cluster-name",
                          {{"localhost", classic_port}}, {2, 1, 0}, "gr-uuid");

  // do the first bootstrap
  std::vector<std::string> cmdline_bs = {
      "--bootstrap=root:"s + kRootPassword + "@localhost:"s +
          std::to_string(classic_port),
      "--force-password-validation", "-d", bootstrap_dir.name()};

  auto &router_bs = launch_router_for_bootstrap(cmdline_bs);
  check_exit_code(router_bs, EXIT_SUCCESS);
}

TEST_F(RouterComponentBootstrapTest, ShowCipherInvalidResult) {
  const std::string tracefile =
      get_data_dir()
          .join("bootstrap_show_cipher_status_invalid_result.js")
          .str();
  const auto mock_server_port = port_pool_.get_next_available();
  const auto mock_http_port = port_pool_.get_next_available();

  launch_mysql_server_mock(tracefile, mock_server_port, EXIT_SUCCESS, false,
                           mock_http_port);
  set_mock_bootstrap_data(mock_http_port, "cluster-name",
                          {{"localhost", mock_server_port}}, {2, 1, 0},
                          "gr-uuid");

  std::vector<std::string> cmdline = {
      "--bootstrap=127.0.0.1:" + std::to_string(mock_server_port), "-d",
      bootstrap_dir.name()};

  auto &router = launch_router_for_bootstrap(cmdline, EXIT_FAILURE);
  check_exit_code(router, EXIT_FAILURE);

  // let's check if the expected error was reported:
  EXPECT_THAT(router.get_full_output(),
              ::testing::HasSubstr(
                  "Failed determining if metadata connection uses SSL: Error "
                  "reading 'ssl_cipher' status variable"));
}

struct BootstrapErrorTestParam {
  std::vector<std::string> bs_params;
  std::string expected_error;
};

class BootstrapErrorTest
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapErrorTestParam> {};

TEST_P(BootstrapErrorTest, Spec) {
  std::vector<std::string> cmdline = {"-d", bootstrap_dir.name()};

  for (const auto &param : GetParam().bs_params) {
    cmdline.push_back(param);
  }

  auto &router = launch_router_for_bootstrap(cmdline, EXIT_FAILURE);
  check_exit_code(router, EXIT_FAILURE);

  // let's check if the expected error was reported:
  EXPECT_THAT(router.get_full_output(),
              ::testing::HasSubstr(GetParam().expected_error));
}

INSTANTIATE_TEST_SUITE_P(
    Spec, BootstrapErrorTest,
    ::testing::Values(
        BootstrapErrorTestParam{
            {"-B=["},
            "Error: invalid URI: expected to find IPv6 address, but failed at "
            "position 9 for: mysql://[\n"},

        BootstrapErrorTestParam{
            {"-B=abc.nodomain.com#fragment"},
            "Error: the bootstrap URI contains a #fragement, but shouldn't"},

        BootstrapErrorTestParam{
            {"-B=abc.nodomain.com?query=q"},
            "Error: the bootstrap URI contains a ?query, but shouldn't"},

        BootstrapErrorTestParam{
            {"-B=abc.nodomain.com/path"},
            "Error: the bootstrap URI contains a /path, but shouldn't"},

        BootstrapErrorTestParam{
            {"--bootstrap-socket=/mysock", "-B=abc.nodomain.com"},
            "Error: --bootstrap-socket given, but --bootstrap option contains "
            "a non-'localhost' hostname: abc.nodomain.com"}));

class BootstrapErrorTestWithMock
    : public RouterComponentBootstrapTest,
      public ::testing::WithParamInterface<BootstrapErrorTestParam> {};

TEST_P(BootstrapErrorTestWithMock, Spec) {
  const std::string tracefile = get_data_dir().join("bootstrap_gr.js").str();
  const auto mock_server_port = port_pool_.get_next_available();

  launch_mysql_server_mock(tracefile, mock_server_port, EXIT_SUCCESS, false);

  std::vector<std::string> cmdline = {"--bootstrap=root:"s + kRootPassword +
                                          "@localhost:"s +
                                          std::to_string(mock_server_port),
                                      "-d", bootstrap_dir.name()};

  for (const auto &param : GetParam().bs_params) {
    cmdline.push_back(param);
  }

  auto &router = launch_router_for_bootstrap(cmdline, EXIT_FAILURE);
  check_exit_code(router, EXIT_FAILURE);

  // let's check if the expected error was reported:
  EXPECT_THAT(router.get_full_output(),
              ::testing::HasSubstr(GetParam().expected_error));
}

INSTANTIATE_TEST_SUITE_P(
    Spec, BootstrapErrorTestWithMock,
    ::testing::Values(
        BootstrapErrorTestParam{
            {"--conf-target-cluster=primary"},
            "The parameter 'target-cluster' is valid only for Cluster that "
            "is part of the ClusterSet."},

        BootstrapErrorTestParam{{"--conf-target-cluster-by-name=name"},
                                "The parameter 'target-cluster-by-name' is "
                                "valid only for Cluster that "
                                "is part of the ClusterSet."},

        BootstrapErrorTestParam{{"--conf-bind-address=.foo"},
                                "Invalid --conf-bind-address value '.foo'"},

        BootstrapErrorTestParam{
            {"--name=name\n"},
            "Router name 'name\n' contains invalid characters."},

        BootstrapErrorTestParam{{"--name=system"},
                                "Router name 'system' is reserved"},

        BootstrapErrorTestParam{
            {"--name=" + std::string(256, 'a')},
            "Router name '" +
                mysql_harness::truncate_string(std::string(256, 'a')) +
                "' too long (max 255)."},

        BootstrapErrorTestParam{
            {"--password-retries=abc"},
            "Configuration error: --password-retries needs value between 1 and "
            "10000 inclusive, was 'abc'"},

        BootstrapErrorTestParam{
            {"--password-retries="},
            "Configuration error: --password-retries needs value between 1 and "
            "10000 inclusive, was ''"}));

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
