/*
  Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.

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
#include <sys/stat.h>
#endif

#include <fstream>
#include <string>

#include <gmock/gmock.h>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "dim.h"
#include "filesystem_utils.h"
#include "harness_assert.h"
#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql/harness/networking/resolver.h"
#include "mysqld_error.h"
#include "mysqlrouter/cluster_metadata.h"
#include "random_generator.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"
#include "script_generator.h"
#include "socket_operations.h"
#include "tcp_port_pool.h"
#include "utils.h"

using namespace std::chrono_literals;
using namespace std::string_literals;

/**
 * @file
 * @brief Component Tests for the bootstrap operation
 */

using namespace std::chrono_literals;
using namespace std::string_literals;
using mysqlrouter::ClusterType;

static constexpr const char kRootPassword[] = "fake-pass";

// we create a number of classes to logically group tests together. But to avoid
// code duplication, we derive them from a class which contains the common code
// they need.
class CommonBootstrapTest : public RouterComponentTest {
 public:
  static void SetUpTestCase() { my_hostname = "dont.query.dns"; }

 protected:
  TcpPortPool port_pool_;
  TempDirectory bootstrap_dir;
  TempDirectory tmp_dir;
  static std::string my_hostname;
  std::string config_file;

  struct Config {
    std::string ip;
    unsigned int port;
    uint16_t http_port;
    std::string js_filename;
    bool unaccessible{false};
  };

  void bootstrap_failover(
      const std::vector<Config> &mock_server_configs,
      const ClusterType cluster_type,
      const std::vector<std::string> &router_options = {},
      int expected_exitcode = 0,
      const std::vector<std::string> &expected_output_regex = {},
      std::chrono::milliseconds wait_for_exit_timeout = 10s,
      const mysqlrouter::MetadataSchemaVersion &metadata_version = {2, 0, 3});

  friend std::ostream &operator<<(
      std::ostream &os,
      const std::vector<std::tuple<ProcessWrapper &, unsigned int>> &T);
};

std::string CommonBootstrapTest::my_hostname;

std::ostream &operator<<(
    std::ostream &os,
    const std::vector<std::tuple<ProcessWrapper &, unsigned int>> &T) {
  for (auto &t : T) {
    auto &proc = std::get<0>(t);

    os << "member@" << std::to_string(std::get<1>(t)) << ": "
       << proc.get_current_output() << std::endl;
  }
  return os;
}

/**
 * the tiny power function that does all the work.
 *
 * - build environment
 * - start mock servers based on Config[]
 * - pass router_options to the launched router
 * - check the router exits as expected
 * - check output of router contains the expected lines
 */
void CommonBootstrapTest::bootstrap_failover(
    const std::vector<Config> &mock_server_configs,
    const ClusterType cluster_type,
    const std::vector<std::string> &router_options, int expected_exitcode,
    const std::vector<std::string> &expected_output_regex,
    std::chrono::milliseconds wait_for_exit_timeout,

    const mysqlrouter::MetadataSchemaVersion &metadata_version) {
  std::string cluster_name("mycluster");

  std::vector<std::pair<std::string, unsigned>> gr_members;
  for (const auto &mock_server_config : mock_server_configs) {
    gr_members.emplace_back(mock_server_config.ip, mock_server_config.port);
  }

  std::vector<std::tuple<ProcessWrapper &, unsigned int>> mock_servers;

  // start the mocks
  for (const auto &mock_server_config : mock_server_configs) {
    if (mock_server_config.js_filename.empty()) continue;

    // 0x10000 & 0xffff = 0 (port 0), but we bypass
    // libmysqlclient's default-port assignment
    const auto port =
        mock_server_config.unaccessible ? 0x10000 : mock_server_config.port;
    const auto http_port = mock_server_config.http_port;
    mock_servers.emplace_back(
        launch_mysql_server_mock(mock_server_config.js_filename, port,
                                 EXIT_SUCCESS, false, http_port),
        port);

    ProcessWrapper &mock_server = std::get<0>(mock_servers.back());
    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(mock_server, static_cast<uint16_t>(port)));

    EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
    set_mock_bootstrap_data(http_port, cluster_name, gr_members,
                            metadata_version);
  }

  std::vector<std::string> router_cmdline;

  if (router_options.size()) {
    router_cmdline = router_options;
  } else {
    router_cmdline.emplace_back("--bootstrap=" + gr_members[0].first + ":" +
                                std::to_string(gr_members[0].second));

    router_cmdline.emplace_back("--report-host");
    router_cmdline.emplace_back(my_hostname);
    router_cmdline.emplace_back("--connect-timeout");
    router_cmdline.emplace_back("1");
    router_cmdline.emplace_back("-d");
    router_cmdline.emplace_back(bootstrap_dir.name());
  }

  // launch the router
  auto &router = launch_router(router_cmdline, expected_exitcode);

  // type in the password
  router.register_response("Please enter MySQL password for root: ",
                           kRootPassword + "\n"s);

  ASSERT_NO_FATAL_FAILURE(
      check_exit_code(router, expected_exitcode, wait_for_exit_timeout))
      << std::get<0>(mock_servers[0]).get_full_output();

  // split the output into lines
  std::vector<std::string> lines;
  {
    std::istringstream ss{router.get_full_output()};

    for (std::string line; std::getline(ss, line);) {
      lines.emplace_back(line);
    }
  }

  for (auto const &re_str : expected_output_regex) {
    EXPECT_THAT(lines, ::testing::Contains(::testing::ContainsRegex(re_str)))
        << "router:" << router.get_full_output() << std::endl
        << mock_servers;
  }

  if (EXIT_SUCCESS == expected_exitcode) {
    // fetch all the content for debugging
    for (auto &mock_server : mock_servers) {
      std::get<0>(mock_server).get_full_output();
    }
    const std::string cluster_type_name = cluster_type == ClusterType::RS_V2
                                              ? "InnoDB ReplicaSet"
                                              : "InnoDB Cluster";
    EXPECT_THAT(lines, ::testing::Contains(
                           "# MySQL Router configured for the " +
                           cluster_type_name + " '" + cluster_name + "'"))
        << "router:" << router.get_full_output() << std::endl
        << mock_servers;

    config_file = bootstrap_dir.name() + "/mysqlrouter.conf";

    // check that the config files (static and dynamic) have the proper
    // access rights
    ASSERT_NO_FATAL_FAILURE(
        check_config_file_access_rights(config_file, /*read_only=*/true));
    const std::string state_file = bootstrap_dir.name() + "/data/state.json";
    ASSERT_NO_FATAL_FAILURE(
        check_config_file_access_rights(state_file, /*read_only=*/false));
  }
}

// for the test with no param
class RouterBootstrapTest : public CommonBootstrapTest {};

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
    : public CommonBootstrapTest,
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

  bootstrap_failover(config, param.cluster_type);

  // let's check if the actual config file output is what we expect:

  const char *expected_config_gr_part1 =
      R"([metadata_cache:mycluster]
cluster_type=gr
router_id=1)";
  // we skip user as it is random and would require regex matching which would
  // require tons of escaping
  // user=mysql_router1_daxi69tk9btt
  const char *expected_config_gr_part2 =
      R"(metadata_cluster=mycluster
ttl=0.5
use_gr_notifications=0

[routing:mycluster_rw]
bind_address=0.0.0.0
bind_port=6446
destinations=metadata-cache://mycluster/?role=PRIMARY
routing_strategy=first-available
protocol=classic

[routing:mycluster_ro]
bind_address=0.0.0.0
bind_port=6447
destinations=metadata-cache://mycluster/?role=SECONDARY
routing_strategy=round-robin-with-fallback
protocol=classic

[routing:mycluster_x_rw]
bind_address=0.0.0.0
bind_port=64460
destinations=metadata-cache://mycluster/?role=PRIMARY
routing_strategy=first-available
protocol=x

[routing:mycluster_x_ro]
bind_address=0.0.0.0
bind_port=64470
destinations=metadata-cache://mycluster/?role=SECONDARY
routing_strategy=round-robin-with-fallback
protocol=x)";

  const char *expected_config_ar_part1 =
      R"([metadata_cache:mycluster]
cluster_type=rs
router_id=1)";
  // we skip user as it is random and would require regex matching which would
  // require tons of escaping
  // user=mysql_router1_ritc56yrjz42
  const char *expected_config_ar_part2 =
      R"(metadata_cluster=mycluster
ttl=0.5

[routing:mycluster_rw]
bind_address=0.0.0.0
bind_port=6446
destinations=metadata-cache://mycluster/?role=PRIMARY
routing_strategy=first-available
protocol=classic

[routing:mycluster_ro]
bind_address=0.0.0.0
bind_port=6447
destinations=metadata-cache://mycluster/?role=SECONDARY
routing_strategy=round-robin-with-fallback
protocol=classic

[routing:mycluster_x_rw]
bind_address=0.0.0.0
bind_port=64460
destinations=metadata-cache://mycluster/?role=PRIMARY
routing_strategy=first-available
protocol=x

[routing:mycluster_x_ro]
bind_address=0.0.0.0
bind_port=64470
destinations=metadata-cache://mycluster/?role=SECONDARY
routing_strategy=round-robin-with-fallback
protocol=x)";

  const std::string config_file_expected1 =
      GetParam().cluster_type == ClusterType::RS_V2 ? expected_config_ar_part1
                                                    : expected_config_gr_part1;

  const std::string config_file_expected2 =
      GetParam().cluster_type == ClusterType::RS_V2 ? expected_config_ar_part2
                                                    : expected_config_gr_part2;

  const std::string config_file_str = get_file_output(config_file);

  EXPECT_TRUE(config_file_str.find(config_file_expected1) !=
                  std::string::npos &&
              config_file_str.find(config_file_expected2) != std::string::npos)
      << "Unexptected config file output:" << std::endl
      << config_file_str << std::endl
      << "Expected:" << config_file_expected1 << std::endl
      << config_file_expected2;
}

INSTANTIATE_TEST_CASE_P(
    BootstrapOkTest, RouterBootstrapOkTest,
    ::testing::Values(
        BootstrapTestParam{ClusterType::GR_V2, "gr", "bootstrap_gr.js", "", ""},
        BootstrapTestParam{ClusterType::RS_V2, "ar", "bootstrap_ar.js", "", ""},
        BootstrapTestParam{ClusterType::GR_V1, "gr_v1", "bootstrap_gr_v1.js",
                           "", ""}),
    get_test_description);

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
    : public CommonBootstrapTest,
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

    bootstrap_failover(mock_servers, GetParam().cluster_type, router_options);
  }
}

INSTANTIATE_TEST_CASE_P(
    BootstrapUserIsCurrentUser, RouterBootstrapUserIsCurrentUser,
    ::testing::Values(
        BootstrapTestParam{ClusterType::GR_V2, "gr", "bootstrap_gr.js", "", ""},
        BootstrapTestParam{ClusterType::RS_V2, "ar", "bootstrap_ar.js", "", ""},
        BootstrapTestParam{ClusterType::GR_V1, "gr_v1", "bootstrap_gr_v1.js",
                           "", ""}),
    get_test_description);
#endif

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
    : public CommonBootstrapTest,
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

  bootstrap_failover(mock_servers, GetParam().cluster_type, router_options,
#ifndef _WIN32
                     EXIT_SUCCESS,
                     {
                       "- Read/Write Connections: .*/mysqlx.sock",
                           "- Read/Only Connections: .*/mysqlxro.sock"
                     }
#else
                     1, { "Error: unknown option '--conf-skip-tcp'" }
#endif
  );
}

INSTANTIATE_TEST_CASE_P(
    BootstrapOnlySockets, RouterBootstrapOnlySockets,
    ::testing::Values(
        BootstrapTestParam{ClusterType::GR_V2, "gr", "bootstrap_gr.js", "", ""},
        BootstrapTestParam{ClusterType::RS_V2, "ar", "bootstrap_ar.js", "", ""},
        BootstrapTestParam{ClusterType::GR_V1, "gr_v1", "bootstrap_gr_v1.js",
                           "", ""}),
    get_test_description);

class BootstrapUnsupportedSchemaVersionTest
    : public CommonBootstrapTest,
      public ::testing::WithParamInterface<mysqlrouter::MetadataSchemaVersion> {
};

/**
 * @test
 *       verify that the router's \c --bootstrap detects an unsupported
 *       metadata schema version
 */
TEST_P(BootstrapUnsupportedSchemaVersionTest,
       BootstrapUnsupportedSchemaVersion) {
  {
    std::vector<Config> mock_servers{
        {"127.0.0.1", port_pool_.get_next_available(),
         port_pool_.get_next_available(),
         get_data_dir().join("bootstrap_unsupported_schema_version.js").str()},
    };

    const auto version = GetParam();

    // check that it failed as expected
    bootstrap_failover(
        mock_servers, ClusterType::GR_V2, {}, EXIT_FAILURE,
        {"^Error: This version of MySQL Router is not compatible "
         "with the provided MySQL InnoDB cluster metadata. "
         "Expected metadata version 1.0.0, 2.0.0, got " +
         to_string(version)},
        10s, GetParam());
  }
}

INSTANTIATE_TEST_CASE_P(
    BootstrapUnsupportedSchemaVersion, BootstrapUnsupportedSchemaVersionTest,
    ::testing::Values(mysqlrouter::MetadataSchemaVersion{3, 0, 0},
                      mysqlrouter::MetadataSchemaVersion{0, 0, 1},
                      mysqlrouter::MetadataSchemaVersion{3, 1, 0}));

/**
 * @test
 *       verify that the router errors out cleanly when received some unexpected
 *       error from the metadata server
 */
TEST_F(CommonBootstrapTest, BootstrapErrorOnFirstQuery) {
  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_error_on_first_query.js").str()},
  };

  // check that it failed as expected
  bootstrap_failover(
      mock_servers, ClusterType::RS_V2, {}, EXIT_FAILURE,
      {"Error executing MySQL query", "Some unexpected error occured"}, 10s);
}

/**
 * @test
 *       verify that the router's \c --bootstrap detects an upgrade
 *       metadata schema version and gives a proper message
 */
TEST_F(CommonBootstrapTest, BootstrapWhileMetadataUpgradeInProgress) {
  {
    std::vector<Config> mock_servers{
        {"127.0.0.1", port_pool_.get_next_available(),
         port_pool_.get_next_available(),
         get_data_dir().join("bootstrap_unsupported_schema_version.js").str()},
    };

    bootstrap_failover(
        mock_servers, ClusterType::GR_V2, {}, EXIT_FAILURE,
        {"^Error: Currently the cluster metadata update is in progress. Please "
         "rerun the bootstrap when it is finished."},
        10s, {0, 0, 0});
  }
}

class RouterBootstrapFailoverSuperReadonly
    : public CommonBootstrapTest,
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

  bootstrap_failover(config, param.cluster_type);
}

INSTANTIATE_TEST_CASE_P(
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
    : public CommonBootstrapTest,
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

  bootstrap_failover(config, param.cluster_type, {}, EXIT_SUCCESS,
                     {
                         "^Fetching Cluster Members",
                         "^Failed connecting to 127\\.0\\.0\\.1:"s +
                             std::to_string(dead_port) + ": .*, trying next$",
                     });
}

INSTANTIATE_TEST_CASE_P(
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
    : public CommonBootstrapTest,
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

  bootstrap_failover(config, param.cluster_type, {}, EXIT_FAILURE,
                     {"^Fetching Cluster Members",
                      "^Failed connecting to 127\\.0\\.0\\.1:"s +
                          std::to_string(dead_port) + ": .*, trying next$",
                      "Error: no more nodes to fail-over too, giving up."});
}

INSTANTIATE_TEST_CASE_P(
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
    : public CommonBootstrapTest,
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

  bootstrap_failover(config, param.cluster_type);
}

INSTANTIATE_TEST_CASE_P(
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
    : public CommonBootstrapTest,
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

  bootstrap_failover(
      config, GetParam().cluster_type, {}, EXIT_FAILURE,
      {"Error: Error creating MySQL account for router \\(GRANTs stage\\): "
       "Error executing MySQL query \"GRANT SELECT, EXECUTE ON "
       "mysql_innodb_cluster_metadata.*\": The MySQL server is running with "
       "the --super-read-only option so it cannot execute this statement"});
}

INSTANTIATE_TEST_CASE_P(
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
 *       verify that bootstraping via a unix-socket fails over to the
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

  bootstrap_failover(mock_servers, ClusterType::GR_V2, router_options,
                     EXIT_FAILURE,
                     {"Error: Error executing MySQL query: Lost connection to "
                      "MySQL server during query \\(2013\\)"});
}

class RouterBootstrapFailoverSuperReadonlyNewPrimaryCrash
    : public CommonBootstrapTest,
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

  bootstrap_failover(mock_servers, GetParam().cluster_type);
}

INSTANTIATE_TEST_CASE_P(
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

  bootstrap_failover(mock_servers, ClusterType::GR_V2, router_options,
                     EXIT_SUCCESS, {});
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

  bootstrap_failover(config, ClusterType::GR_V2, {}, EXIT_FAILURE,
                     {"Access denied for user 'native'@'%' to database "
                      "'mysql_innodb_cluster_metadata"});
}

class RouterBootstrapBootstrapNoGroupReplicationSetup
    : public CommonBootstrapTest,
      public ::testing::WithParamInterface<BootstrapTestParam> {};

/**
 * @test
 *       ensure a resonable error message if schema exists, but no
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

  bootstrap_failover(config, param.cluster_type, {}, EXIT_FAILURE,
                     {"to have Group Replication running"});
}

INSTANTIATE_TEST_CASE_P(
    BootstrapNoGroupReplicationSetup,
    RouterBootstrapBootstrapNoGroupReplicationSetup,
    ::testing::Values(BootstrapTestParam{ClusterType::GR_V2, "gr",
                                         "bootstrap_no_gr.js", "", ""},
                      BootstrapTestParam{ClusterType::GR_V1, "gr_v1",
                                         "bootstrap_no_gr_v1.js", "", ""}),
    get_test_description);

/**
 * @test
 *       ensure a resonable error message if metadata schema does not exist.
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

  bootstrap_failover(config, ClusterType::GR_V2, {}, EXIT_FAILURE,
                     {"to contain the metadata of MySQL InnoDB Cluster"});
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

  bootstrap_failover(
      mock_servers, ClusterType::GR_V2, router_options, EXIT_FAILURE,
      {"Error: Error executing MySQL query \".*\": Lost connection to "
       "MySQL server during query \\(2013\\)"});
}

////////////////////////////////////////////////////////////////////////////////
//
// class AccountReuseTestBase
//
////////////////////////////////////////////////////////////////////////////////

class AccountReuseTestBase : public CommonBootstrapTest {
 public:
  template <typename Container, typename Func>
  static std::string make_list(const Container &items, Func generator) {
    if (items.empty()) return "";

    std::string res;
    bool is_first{true};
    for (const auto &i : items) {
      if (is_first) {
        is_first = false;
      } else {
        res += ",";
      }
      res += generator(i);
    }
    return res;
  }

 protected:
  ////////////////////////////////////////////////////////////////////////////////
  // SQL for .JS backend functions
  ////////////////////////////////////////////////////////////////////////////////

  static std::string res_ok() { return R"("ok": {})"; }
  static std::string res_error(
      unsigned code = 1234, const std::string &msg = "je pense, donc je suis") {
    return
        R"("error": {
  "code": )" +
        std::to_string(code) +
        R"(,
  "sql_state": "HY001",
  "message": ")" +
        msg +
        R"("
})";
  }
  static std::string sql_create_user(const std::string &account_auth_list,
                                     bool if_not_exists = true) {
    return "CREATE USER "s + (if_not_exists ? "IF NOT EXISTS " : "") +
           account_auth_list;
  }
  static std::string res_create_user(unsigned warning_count) {
    return R"("ok": { "warning_count" : )" + std::to_string(warning_count) +
           "}";
  }

  static std::string res_cu_error(const std::string &username,
                                  const std::set<std::string> &account_hosts) {
    const std::string al = make_account_list(username, account_hosts);
    return res_error(
        ER_CANNOT_USER /*1396*/,
        "ERROR 1396 (HY000): Operation CREATE USER failed for " + al);
  }

  static std::string sql_show_warnings() { return "SHOW WARNINGS"; }
  static std::string res_show_warnings(
      const std::string &username, const std::set<std::string> &account_hosts) {
    // SHOW WARNINGS example output
    // +-------+------+---------------------------------------------+
    // | Level | Code | Message                                     |
    // +-------+------+---------------------------------------------+
    // | Note  | 3163 | Authorization ID 'bla'@'h1' already exists. |
    // | Note  | 3163 | Authorization ID 'bla'@'h3' already exists. |
    // +-------+------+---------------------------------------------+

    std::string res =
        R"("result": {
"columns": [
  {
    "type": "STRING",
    "name": "Level"
  },
  {
    "type": "LONG",
    "name": "Code"
  },
  {
    "type": "STRING",
    "name": "Message"
  }
],
"rows": [)";

    bool is_first{true};
    for (const std::string &h : account_hosts) {
      if (is_first) {
        is_first = false;
      } else {
        res += ",";
      }
      res += R"([ "Note", )" + kUserExistsCode + R"(, "Authorization ID ')" +
             username + "'@'" + h + R"(' already exists." ])";
    }

    res += R"(  ]
})";

    return res;
  }
  static std::string sql_grant_1(const std::string &account_list) {
    return "GRANT SELECT, EXECUTE ON mysql_innodb_cluster_metadata.* TO " +
           account_list;
  }
  static std::string sql_grant_2(const std::string &account_list) {
    return "GRANT SELECT ON performance_schema.replication_group_members TO " +
           account_list;
  }
  static std::string sql_grant_3(const std::string &account_list) {
    return "GRANT SELECT ON performance_schema.replication_group_member_stats "
           "TO " +
           account_list;
  }
  static std::string sql_grant_4(const std::string &account_list) {
    return "GRANT SELECT ON performance_schema.global_variables TO " +
           account_list;
  }
  static std::string sql_grant_5(const std::string &account_list) {
    return "GRANT INSERT, UPDATE, DELETE ON "
           "mysql_innodb_cluster_metadata.routers TO " +
           account_list;
  }
  static std::string sql_grant_6(const std::string &account_list) {
    return "GRANT INSERT, UPDATE, DELETE ON "
           "mysql_innodb_cluster_metadata.v2_routers TO " +
           account_list;
  }
  static std::string sql_drop_user_if_exists(const std::string &account_list) {
    return "DROP USER IF EXISTS " + account_list;
  }

  static std::string sql_rollback() { return "ROLLBACK"; }

  static std::string sql_fetch_hosts() {
    return "SELECT member_host, member_port   FROM "
           "performance_schema.replication_group_members  /*!80002 ORDER BY "
           "member_role */";
  }
  static std::string res_fetch_hosts(const std::vector<uint16_t> server_ports) {
    harness_assert(server_ports.size() == 3);

    return R"("result": {
  "columns": [
    {
      "name": "member_host",
      "type": "STRING"
    },
    {
      "name": "member_port",
      "type": "LONG"
    }
  ],
  "rows": [
    [
      "127.0.0.1",
)" + std::to_string(server_ports[0]) +
           R"(
    ],
    [
      "127.0.0.1",
)" + std::to_string(server_ports[1]) +
           R"(
    ],
    [
      "127.0.0.1",
)" + std::to_string(server_ports[2]) +
           R"(
    ]
  ]
})";
  }
  static std::string sql_insert_router(const std::string hostname,
                                       const std::string router_name = "") {
    return "INSERT INTO mysql_innodb_cluster_metadata.v2_routers        "
           "(address, product_name, router_name)"
           " VALUES ('" +
           hostname + "', 'MySQL Router', '" + router_name + "')";
  }
  static std::string res_insert_host_id_and_router_name_duplicate_key_error() {
    return res_error(ER_DUP_ENTRY);
  }
  static std::string sql_fetch_router_id(std::string router_name = "") {
    return "SELECT router_id FROM mysql_innodb_cluster_metadata.v2_routers "
           "WHERE router_name = '" +
           router_name + "'";
  }
  static std::string res_fetch_router_id(unsigned router_id) {
    return
        R"("result": {
  "columns": [
    {
      "type": "STRING",
      "name": "router_id"
    }
  ],
  "rows": [[ ")" +
        std::to_string(router_id) + R"(" ]]
})";
  }

  // ---- account validation queries ----
  static std::string sql_val1(const std::string &cluster_name = "test") {
    return "select I.mysql_server_uuid, I.endpoint, I.xendpoint from "
           "mysql_innodb_cluster_metadata.v2_instances I join "
           "mysql_innodb_cluster_metadata.v2_gr_clusters C on I.cluster_id = "
           "C.cluster_id where C.cluster_name = '" +
           cluster_name + "'";
  }
  static std::string sql_val2() {
    return "show status like 'group_replication_primary_member'";
  }
  static std::string sql_val3() {
    return "SELECT member_id, member_host, member_port, member_state, "
           "@@group_replication_single_primary_mode FROM "
           "performance_schema.replication_group_members WHERE channel_name = "
           "'group_replication_applier'";
  }
  static std::string sql_val4() {
    return "select @@group_replication_group_name";
  }

  static std::string stmt_resp(const std::string &stmt,
                               const std::string &response = "\"ok\": {}") {
    return "\"" + stmt + "\": {" + response + "}";
  }

  struct CustomResponses {
    // list of SQL statements that were expected to execute - scan SQL log
    // for all items on this list to ensure that all of them executed
    std::vector<std::string> exp_sql;

    // list of JS objects (stmts and their responses) that we feed to the
    // MockServer
    std::string stmts;

    // both class members should be 1:1.  This convenience method makes this
    // easier
    void add(const std::string &stmt,
             const std::string &response = "\"ok\": {}") {
      if (stmts.empty())
        stmts = stmt_resp(stmt, response);
      else
        stmts += ",\n" + stmt_resp(stmt, response);

      exp_sql.emplace_back(stmt);
    }

    void add(const CustomResponses &other) {
      if (stmts.empty())
        stmts = other.stmts;
      else
        stmts += ",\n" + other.stmts;

      exp_sql.insert(exp_sql.end(), other.exp_sql.begin(), other.exp_sql.end());
    }
  };

  // generates SQL statements that emulate creation of account(s) for a
  // scenario where CREATE USER [IF NOT EXISTS] succeeds
  CustomResponses gen_sql_for_creating_accounts(
      const std::string &username,
      const std::set<std::string> &hostnames_requested,
      const std::set<std::string> &hostnames_existing =
          {},  // must be empty if if_not_exists == false
      bool if_not_exists = true,
      const std::string &password_hash = kAccountUserPasswordHash) {
    CustomResponses cr;

    // CREATE USER [IF NOT EXISTS]
    const std::string account_auth_list =
        make_account_auth_list(username, hostnames_requested, password_hash);
    std::set<std::string> hostnames_new;
    if (hostnames_existing.empty()) {
      cr.add(sql_create_user(account_auth_list, if_not_exists));

      hostnames_new = hostnames_requested;
    } else {
      harness_assert(if_not_exists);

      cr.add(sql_create_user(account_auth_list, if_not_exists),
             res_create_user(hostnames_existing.size()));
      cr.add(sql_show_warnings(),
             res_show_warnings(username, hostnames_existing));

      std::set_difference(hostnames_requested.begin(),
                          hostnames_requested.end(), hostnames_existing.begin(),
                          hostnames_existing.end(),
                          std::inserter(hostnames_new, hostnames_new.begin()));
    }

    // GRANTs
    if (!hostnames_new.empty()) {
      const std::string al = make_account_list(username, hostnames_new);
      cr.add(sql_grant_1(al));
      cr.add(sql_grant_2(al));
      cr.add(sql_grant_3(al));
      cr.add(sql_grant_4(al));
      cr.add(sql_grant_5(al));
      cr.add(sql_grant_6(al));
    }

    return cr;
  }

  // generates SQL statements that emulate an already-registered Router
  // (queries + responses that will occur during a subsequent bootstrap)
  CustomResponses gen_sql_for_registered_router(
      const unsigned router_id = 123) {
    CustomResponses cr;

    cr.add(sql_insert_router("dont.query.dns"),
           res_insert_host_id_and_router_name_duplicate_key_error());

    cr.add(sql_fetch_router_id(), res_fetch_router_id(router_id));

    return cr;
  }

  void set_mock_server_sql_statements(
      uint16_t server_http_port,
      const std::string
          &custom_responses,  // custom SQL statements + responses, same form as
                              // common_statements.js
      const std::string &validated_username =
          "<not set>"  // used during account validation
  ) {
    ASSERT_TRUE(
        MockServerRestClient(server_http_port).wait_for_rest_endpoint_ready());

    try {
      MockServerRestClient(server_http_port)
          .set_globals(
              "{"
              "\"custom_responses\": {" +
              custom_responses +
              "},"
              "\"custom_auth\": { \"username\": \"" +
              validated_username +
              "\" }"
              "}");
    } catch (const std::exception &e) {
      FAIL() << e.what() << "\n"
             << "custom_responses payload = '" << custom_responses << "'"
             << std::endl;
    };
  }

  void add_login_hook(ProcessWrapper &router,
                      const std::string &account_password,
                      const std::string &username = kAccountUser,
                      bool root_password_on_cmdline = false) {
    router.register_response(
        "Please enter MySQL password for " + username + ": ",
        account_password + "\n");

    if (root_password_on_cmdline == false)
      router.register_response("Please enter MySQL password for root: ",
                               "fake-root-pass\n");
  }

  ////////////////////////////////////////////////////////////////////////////////
  // other functions
  ////////////////////////////////////////////////////////////////////////////////

  ProcessWrapper &launch_mock_server(
      uint16_t server_port, uint16_t server_http_port,
      const std::string &js = "bootstrap_account_tests.js") {
    const std::string json_stmts = get_data_dir().join(js).str();
    constexpr bool debug = true;
    auto &server_mock = launch_mysql_server_mock(
        json_stmts, server_port, EXIT_SUCCESS, debug, server_http_port);
    EXPECT_TRUE(wait_for_port_ready(server_port))
        << server_mock.get_full_output();
    return server_mock;
  }

  ProcessWrapper &launch_bootstrap(int exp_exit_code, uint16_t server_port,
                                   const std::string &bootstrap_directory,
                                   const std::vector<std::string> &extra_args,
                                   bool root_password_on_cmdline = false) {
    std::vector<std::string> args = {
        "--bootstrap",
        "root"s + (root_password_on_cmdline ? ":root_password" : "") +
            "@127.0.0.1:" + std::to_string(server_port),
        "--report-host",
        my_hostname,
        "-d",
        bootstrap_directory};
    for (const std::string &a : extra_args) args.push_back(a);
    return ProcessManager::launch_router(args, exp_exit_code);
  }

  static std::string get_local_hostname() {
    return mysql_harness::SocketOperations::instance()->get_local_hostname();
  }
  static std::string get_local_ip(std::string local_hostname = "") {
    if (local_hostname.empty()) {
      local_hostname = get_local_hostname();
    }

    mysql_harness::Resolver rs;
    std::vector<mysql_harness::IPAddress> local_ips =
        rs.hostname(local_hostname);

    // find local IPv4 that's not a loopback
    std::string local_ip;
    for (const auto &ip : local_ips) {
      if (ip.is_ipv4() && ip.str() != "127.0.0.1") {
        local_ip = ip.str();
        break;
      }
    }
    EXPECT_FALSE(local_ip.empty());

    return local_ip;
  }

  static std::string dump(ProcessWrapper &router, ProcessWrapper &server_mock,
                          uint16_t server_http_port,
                          std::chrono::milliseconds timeout = 1000ms) {
    std::stringstream ss;

    ss << "\n";
    try {
      router.wait_for_exit(timeout);
    } catch (...) {
      ss << "dump(): WARNING, waiting for Router timed out, output might not "
            "be complete!\n";
    }
    try {
      server_mock.wait_for_exit(timeout);
    } catch (...) {
      ss << "dump(): NOTE that Server Mock is still running\n";
    }

    ss << "vvvvvvvvvvvvvvvvvvvv OUTPUT DUMP vvvvvvvvvvvvvvvvvvvv\n";

    // Router and Mock Server output
    ss << "-------- Router:\n" << router.get_full_output() << "\n";
    ss << "-------- Server:\n" << server_mock.get_full_output() << "\n";

    // SQL log
    {
      ss << "[HTTP PORT " + std::to_string(server_http_port) + "] SQL log:\n";

      std::string server_globals =
          MockServerRestClient(server_http_port).get_globals_as_json_string();

      rapidjson::Document json_doc;
      json_doc.Parse(server_globals.c_str());
      if (json_doc.HasMember("sql_log")) {
        const auto &sql_log = json_doc["sql_log"];
        ss << sql_log << "\n";
      } else {
        ss << "<NONE>"
           << "\n";
      }
    }

    ss << "^^^^^^^^^^^^^^^^^^^^ OUTPUT DUMP ^^^^^^^^^^^^^^^^^^^^\n";
    return ss.str();
  }

  /**
   * Dumps debug information on scope exit, if test has failed
   */
  class DebugDumper {
   public:
    DebugDumper(ProcessWrapper &router, ProcessWrapper &server_mock,
                uint16_t server_http_port,
                std::chrono::milliseconds timeout = 1000ms)
        : router_(router),
          server_mock_(server_mock),
          server_http_port_(server_http_port),
          timeout_(timeout) {}
    ~DebugDumper() {
      if (::testing::Test::HasFailure())
        std::cerr << AccountReuseTestBase::dump(router_, server_mock_,
                                                server_http_port_, timeout_);
    }

   private:
    ProcessWrapper &router_;
    ProcessWrapper &server_mock_;
    uint16_t server_http_port_;
    std::chrono::milliseconds timeout_;
  };

  void check_bootstrap_success(
      ProcessWrapper &router, const std::vector<std::string> exp_output,
      const std::vector<std::string> unexp_output = {}) {
    std::shared_ptr<void> exit_guard(nullptr, [&](void *) {
      if (!bootstrap_finished_running_) FAIL();  // induce to call dump()
    });

    try {
      router.wait_for_exit();
      bootstrap_finished_running_ = true;
    } catch (const std::exception &e) {
      std::cerr << "check_bootstrap_success(): wait_for_exit() threw: "
                << e.what() << std::endl;
      throw;
    } catch (...) {
      std::cerr << "check_bootstrap_success(): wait_for_exit() threw unknown "
                   "exception"
                << std::endl;
      throw;
    }

    // split the output into lines
    std::vector<std::string> lines;
    {
      std::istringstream ss{router.get_full_output()};

      for (std::string line; std::getline(ss, line);) {
        lines.emplace_back(line);
      }
    }

    for (const std::string output : exp_output) {
      EXPECT_TRUE(router.expect_output(output)) << "-------- expected output:\n"
                                                << output << std::endl;
    }
    for (const std::string output : unexp_output) {
      EXPECT_FALSE(router.expect_output(output))
          << "-------- unexpected output:\n"
          << output << std::endl;
    }
  }

  void check_bootstrap_success(ProcessWrapper &router,
                               const std::string &exp_output) {
    check_bootstrap_success(router, {exp_output}, {});
  }

  void check_keyring(const std::string &bootstrap_directory, bool expect_exists,
                     const std::string &expect_user = "",
                     const std::string &expect_password = "",
                     bool running_after_bootstrap = true) {
    if (running_after_bootstrap)
      // calling check_bootstrap_success() is a prerequisite
      harness_assert(bootstrap_finished_running_);

    // expect that keyring exists and contains expected account name and
    // password
    if (expect_exists) {
      mysql_harness::reset_keyring();
      ASSERT_NO_THROW(mysql_harness::init_keyring(
          Path(bootstrap_directory)
              .join("data")
              .join("keyring")
              .real_path()
              .str(),
          Path(bootstrap_directory).join("mysqlrouter.key").str(), false));

      const std::string kKeyringAttributePassword = "password";
      std::string password;
      EXPECT_NO_THROW(password = mysql_harness::get_keyring()->fetch(
                          expect_user, kKeyringAttributePassword));
      EXPECT_EQ(expect_password, password);
    }
    // expect that keyring does not exist
    else {
      EXPECT_FALSE(Path(bootstrap_directory).join("data").exists());
      EXPECT_FALSE(Path(bootstrap_directory).join("mysqlrouter.key").exists());
    }
  }

  void check_questions_asked_by_bootstrap(
      int exp_exit_code, ProcessWrapper &router,
      bool account_opt /* whether --account was given on cmdline */,
      bool root_password_on_cmdline = false) {
    // calling check_bootstrap_success() is a prerequisite
    harness_assert(bootstrap_finished_running_);

    const size_t root_pass_prompt = ([&]() {
      if (root_password_on_cmdline)
        // to keep logic simple, we cheat by pretending prompt was asked right
        // at the beginning
        return size_t{0};
      else
        return router.get_full_output().find(
            "Please enter MySQL password for root:");
    })();
    const size_t account_pass_prompt = router.get_full_output().find(
        "Please enter MySQL password for some_user:");

    // <account_user> prompt cannot appear if --account was not given on
    // command-line
    if (!account_opt) {
      EXPECT_EQ(std::string::npos, account_pass_prompt);
    }

    if (exp_exit_code == EXIT_SUCCESS) {
      // on success:
      // - expect root password prompt
      // - if --account was given on command-line, <account_user> password
      //   prompt should follow
      EXPECT_NE(std::string::npos, root_pass_prompt);
      if (account_opt) {
        EXPECT_NE(std::string::npos, account_pass_prompt);
        EXPECT_GT(account_pass_prompt,
                  root_pass_prompt);  // <account_user> after root
      }
    } else {
      // on error, prompts still have same presentation order, but the process
      // is allowed to exit before showing any/all prompts.  This translates to
      // a requirement: 2nd question (<account_user> prompt) cannot be asked
      // unless 1st question (root prompt) got asked first.
      if (account_pass_prompt != std::string::npos) {
        EXPECT_NE(std::string::npos, root_pass_prompt);
        EXPECT_GT(account_pass_prompt,
                  root_pass_prompt);  // <account_user> after root
      }
    }
  }

  void check_config(const std::string &bootstrap_directory, bool expect_exists,
                    const std::string &username = "") {
    // calling check_bootstrap_success() is a prerequisite
    harness_assert(bootstrap_finished_running_);

    Path config_file(bootstrap_directory);
    config_file.append("mysqlrouter.conf");

    // on bootstrap success, verify that configuration file got created with
    // expected account name
    if (expect_exists) {
      ASSERT_TRUE(config_file.exists());
      EXPECT_TRUE(
          find_in_file(config_file.str(), [&](const std::string &line) -> bool {
            return line.find("user=" + username) != line.npos;
          }));
    } else {
      EXPECT_FALSE(config_file.exists());
    }
  }

  // this works for a simple case, when there's no SHOW WARNINGS at play and no
  // errors
  void check_user_creating_SQL_calls(
      const std::string &username,
      const std::set<std::string> &exp_created_account_hosts,
      bool if_not_exists, uint16_t server_http_port) {
    const auto &h = exp_created_account_hosts;  // shorter name alias
    if (h.size()) {
      const std::string account_auth_list = make_account_auth_list(username, h);
      const std::string al = make_account_list(username, h);
      std::vector<std::string> create_user_queries = {
          sql_create_user(account_auth_list, if_not_exists),
          sql_grant_1(al),
          sql_grant_2(al),
          sql_grant_3(al),
      };
      check_SQL_calls(server_http_port, create_user_queries);
    }
  }

  void check_SQL_calls(uint16_t server_http_port,
                       const std::vector<std::string> exp_stmts,
                       const std::vector<std::string> unexp_stmts = {}) {
    // calling check_bootstrap_success() is a prerequisite
    harness_assert(bootstrap_finished_running_);

    std::string server_globals =
        MockServerRestClient(server_http_port).get_globals_as_json_string();

    rapidjson::Document json_doc;
    json_doc.Parse(server_globals.c_str());
    ASSERT_TRUE(json_doc.HasMember("sql_log"));
    const auto &sql_log = json_doc["sql_log"];

    const std::string id =
        "[HTTP PORT " + std::to_string(server_http_port) + "] ";

    auto expect_stmt = [&](const std::string &query, bool expected) {
      // we search for substring matches - this is more useful than searching
      // for an exact string when trying to prove a particular (class of)
      // statments did or did not execute.  You can always make the substring
      // as specific as you'd like (the whole query string) to get the exact
      // match behaviour.
      ASSERT_TRUE(sql_log.IsObject());
      if (expected) {
        bool found{false};
        for (auto const &member : sql_log.GetObject()) {
          ASSERT_TRUE(member.name.IsString());

          if (std::string(member.name.GetString(),
                          member.name.GetStringLength())
                  .find(query) != std::string::npos) {
            found = true;
            break;
          }
        }

        ASSERT_TRUE(found) << query;
      } else {
        for (auto const &member : sql_log.GetObject()) {
          ASSERT_TRUE(member.name.IsString());

          ASSERT_EQ(std::string(member.name.GetString(),
                                member.name.GetStringLength())
                        .find(query),
                    std::string::npos)
              << "Unexpected query (substring) " + id + ": " << query << "\n";
        }
      }
    };

    for (const std::string &s : exp_stmts) expect_stmt(s, true);
    for (const std::string &s : unexp_stmts) expect_stmt(s, false);
  }

  void create_config(const std::string &bootstrap_directory,
                     const std::string &username, unsigned router_id = 34,
                     const std::string &cluster_name = "test") {
    create_config_file(bootstrap_directory, "[metadata_cache:" + cluster_name +
                                                "]\n"
                                                "router_id=" +
                                                std::to_string(router_id) +
                                                "\n"
                                                "user=" +
                                                username +
                                                "\n"
                                                "metadata_cluster=" +
                                                cluster_name + "\n");
  }

  static void create_keyring(const std::string &bootstrap_directory,
                             const std::string &username,
                             const std::string &password) {
    const std::string kKeyringAttributePassword = "password";

    EXPECT_EQ(0,
              mysql_harness::mkdir(Path(bootstrap_directory).join("data").str(),
                                   mysql_harness::kStrictDirectoryPerm));

    mysql_harness::reset_keyring();
    EXPECT_NO_THROW(mysql_harness::init_keyring(
        Path(bootstrap_directory)
            .real_path()
            .join("data")
            .join("keyring")
            .str(),
        Path(bootstrap_directory).join("mysqlrouter.key").str(), true));
    EXPECT_NO_THROW(mysql_harness::get_keyring()->store(
        username, kKeyringAttributePassword, password));
    EXPECT_NO_THROW(mysql_harness::flush_keyring());
  }

  static std::string make_account_list(const std::string &username,
                                       const std::set<std::string> &hostnames) {
    return make_list(hostnames, [&](const std::string &h) {
      return "'" + username + "'@'" + h + "'";
    });
  }

  static std::string make_account_auth_list(
      const std::string &username, const std::set<std::string> &hostnames,
      const std::string &password_hash = kAccountUserPasswordHash) {
    return make_list(hostnames,
                     [&](const std::string &h) {
                       return "'" + username + "'@'" + h +
                              "'"
                              " IDENTIFIED WITH mysql_native_password"
                              " AS '" +
                              password_hash + "'";
                     }

    );
  }

  static bool is_using_account(const std::vector<std::string> &cmdline_args) {
    return is_given_on_cmdline(cmdline_args, "--account");
  }
  static bool is_if_not_exists(const std::vector<std::string> &cmdline_args) {
    // default is if-not-exists
    if (!is_given_on_cmdline(cmdline_args, "--account-create")) return true;

    harness_assert(
        is_given_on_cmdline(cmdline_args, "never")             // -> false
        || is_given_on_cmdline(cmdline_args, "if-not-exists")  // -> true
        || is_given_on_cmdline(cmdline_args, "always"));       // -> false

    return is_given_on_cmdline(cmdline_args, "if-not-exists");
  }

  static std::string missing_host_err_msg(
      const std::string &host_not_in_db = std::string()) {
    const std::string kSuffix =
        "'does not exist. If this is expected, please rerun with "
        "--account-create (always|if-not-exists)";
    if (host_not_in_db.empty())
      return /*could be either host here*/ kSuffix;
    else
      return "Error: Account '" + kAccountUser + "@" + host_not_in_db + kSuffix;
  }

  static std::string existing_host_err_msg(
      const std::string &username, const std::set<std::string> &account_hosts) {
    return "Error: Account(s) " + make_account_list(username, account_hosts) +
           " already exist(s). If this is expected, please rerun without "
           "`--account-create always`.";
  }

  static std::vector<std::string> undo_create_user_msg(
      const std::string &new_account_list, const unsigned du_err_code = 0,
      const std::string &du_err_msg = "") {
    if (du_err_code) {
      return {
          // clang-format off
        "- Creating account(s) (only those that are needed, if any)",

        "FATAL ERROR ENCOUNTERED, attempting to undo new accounts that were created",

        "ERROR: As part of cleanup after bootstrap failure, we tried to erase account(s)",
        "that we created.  Unfortuantely the cleanup failed with error:",
        "  Error executing MySQL query \"DROP USER IF EXISTS " + new_account_list + "\": " + du_err_msg + " (" + std::to_string(du_err_code) + ")",
        "You may want to clean up the accounts yourself, here is the full list of",
        "accounts that were created:",
        "  " + new_account_list,
          // clang-format on
      };
    } else {
      return {
          // clang-format off
        "- Creating account(s) (only those that are needed, if any)",

        "FATAL ERROR ENCOUNTERED, attempting to undo new accounts that were created",

        "- New accounts cleaned up successfully",
          // clang-format on
      };
    }
  }

  static std::vector<std::string> show_warnings_failed_err_msg(
      const std::string &account_list) {
    return {
        // clang-format off
      "- Creating account(s) (only those that are needed, if any)",

      "ERROR: We created account(s), of which at least one already existed.",
      "A fatal error occurred while we tried to determine which account(s) were new,",
      "therefore to be safe, we did not erase any accounts while cleaning-up before",
      "exiting.",
      "You may want to clean those up yourself, if you deem it appropriate.",
      "Here's a full list of accounts that bootstrap tried to create (some of which",
      "might have already existed before bootstrapping):",
      "  " + account_list,
        // clang-format on
    };
  }

  static std::string acct_val_msg() {
    return "- Verifying account (using it to run SQL queries that would be run "
           "by Router)";
  }

  static std::vector<std::string> acct_val_failed_warning_msg() {
    return {
        "***** WARNING *****",
        "Account verification failed with error:",
        // <error appears in this line>

        "This means that we were unable to log in using the accounts that were "
        "created",
        "and run SQL queries that Router needs to run during its operation.",
        "It means this Router instance may be inoperable and user intervention "
        "is",
        "required to correct the issue and/or bootstrap again.",
    };
  }

  static std::vector<std::string> acct_val_failed_error_msg() {
    return {
        // clang-format off
      "Error: Account verification failed with error:",
      // <error appears in this line>

      "This means that we were unable to log in using the accounts that were created",
      "and run SQL queries that Router needs to run during its operation.",
        // clang-format on
    };
  }

  bool bootstrap_finished_running_ = false;

  static const std::string kBootstrapSuccessMsg;
  static const std::string kUndoCreateUserSuccessMsg;

  static const std::string kAccountUser;  // passed by --account
  static const std::string kAccountUserPassword;
  static const std::string kAccountUserPasswordHash;
  static const std::string kAutoGenUser;  // autogenerated without --account
  static const std::string kAutoGenUserPassword;
  static const std::string kAutoGenUserPasswordHash;

  static const std::string kHostC_inDB;
  static const std::string kHostD_inDB;
  static const std::string kHostA_notInDB;
  static const std::string kHostB_notInDB;
  static const std::vector<std::string> kAllHostsUsedInTests;

  static const std::string kUserExistsCode;  // "3163"
 private:
  static bool is_given_on_cmdline(const std::vector<std::string> &cmdline_args,
                                  const std::string &arg) {
    return std::find(cmdline_args.begin(), cmdline_args.end(), arg) !=
           cmdline_args.end();
  }
};
/*static*/ const std::string AccountReuseTestBase::kBootstrapSuccessMsg =
    "MySQL Router configured for the InnoDB Cluster 'test'";
/*static*/ const std::string AccountReuseTestBase::kUndoCreateUserSuccessMsg =
    "- New accounts cleaned up successfully";
/*static*/ const std::string AccountReuseTestBase::kAccountUser = "some_user";
/*static*/ const std::string AccountReuseTestBase::kAutoGenUser =
    "mysql_router1_abcdefghijkl";
/*static*/ const std::string AccountReuseTestBase::kHostC_inDB = "host9";
/*static*/ const std::string AccountReuseTestBase::kHostD_inDB =
    "%";  // wildcards are not special to CREATE USER, and so they're not
          // special to us.  '%' can co-exist with other hostnames, perfectly
          // fine, see WL#13177 for details
/*static*/ const std::string AccountReuseTestBase::kHostA_notInDB = "host1";
/*static*/ const std::string AccountReuseTestBase::kHostB_notInDB = "host2%";
/*static*/ const std::vector<std::string>
    AccountReuseTestBase::kAllHostsUsedInTests = {
        kHostC_inDB, kHostD_inDB, kHostA_notInDB, kHostB_notInDB};
/*static*/ const std::string AccountReuseTestBase::kAccountUserPassword =
    "fake-account-pass";
/*static*/ const std::string AccountReuseTestBase::kAutoGenUserPassword =
    "fake-autogen-pass";
/*static*/ const std::string AccountReuseTestBase::kAccountUserPasswordHash =
    "*FF1D4A27A543DD464A5FFA210278E604979F781B";
/*static*/ const std::string AccountReuseTestBase::kAutoGenUserPasswordHash =
    "*4F7873C0ABA52D7BB5E1AE9271F636B2C48174E4";
/*static*/ const std::string AccountReuseTestBase::kUserExistsCode =
    std::to_string(ER_USER_ALREADY_EXISTS);  // "3163"

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// COMMAND-LINE VERIFICATION TESTS                                            //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

class AccountReuseBadCmdlineTest : public AccountReuseTestBase {};

/**
 * @test
 * verify that --account without --bootstrap switch produces an error and exits
 *
 * WL13177:TS_FR06_01
 */
TEST_F(AccountReuseBadCmdlineTest, account_without_bootstrap_switch) {
  // launch the router in bootstrap mode
  auto &router =
      ProcessManager::launch_router({"--account", "account1"}, EXIT_FAILURE);

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output(
      "Option --account can only be used together with -B/--bootstrap"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 * verify that --account without required argument produces an error and exits
 *
 * WL13177:TS_FR07_01
 */
TEST_F(AccountReuseBadCmdlineTest, account_argument_missing) {
  // launch the router in bootstrap mode
  auto &router =
      ProcessManager::launch_router({"-B=0", "--account"}, EXIT_FAILURE);

  // check if the bootstraping was successful
  EXPECT_TRUE(
      router.expect_output("option '--account' expects a value, got nothing"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 * verify that --account with empty argument produces an error and exits
 *
 * WL13177:TS_FR07_02
 */
TEST_F(AccountReuseBadCmdlineTest, account_argument_empty) {
  // launch the router in bootstrap mode
  auto &router =
      ProcessManager::launch_router({"-B=0", "--account", ""}, EXIT_FAILURE);

  // check if the bootstraping was successful
  EXPECT_TRUE(
      router.expect_output("Error: Value for --account option cannot be empty"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 * verify that --account given twice produces an error and exits
 */
TEST_F(AccountReuseBadCmdlineTest, account_given_twice) {
  // launch the router in bootstrap mode
  auto &router = ProcessManager::launch_router(
      {"-B=0", "--account", "user1", "--account", "user2"}, EXIT_FAILURE);

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output(" Option --account can only be given once"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 * verify that --account-create without --account switch produces an error and
 * exits
 *
 * WL13177:TS_FR09_01
 */
TEST_F(AccountReuseBadCmdlineTest, account_create_without_account_switch) {
  // launch the router in bootstrap mode
  auto &router = ProcessManager::launch_router(
      {"-B=0", "--account-create", "never"}, EXIT_FAILURE);

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output(
      "Option --account-create can only be used together with --account"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 * verify that --account-create without required argument produces an error and
 * exits
 *
 * WL13177:TS_FR08_01
 */
TEST_F(AccountReuseBadCmdlineTest, account_create_argument_missing) {
  // launch the router in bootstrap mode
  auto &router =
      ProcessManager::launch_router({"-B=0", "--account-create"}, EXIT_FAILURE);

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output(
      "option '--account-create' expects a value, got nothing"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 * verify that --account-create given with illegal argument produces an error
 * and exits
 *
 * WL13177:TS_FR08_02
 */
TEST_F(AccountReuseBadCmdlineTest, account_create_illegal_value) {
  // launch the router in bootstrap mode
  auto &router = ProcessManager::launch_router(
      {"-B=0", "--account", "user1", "--account-create", "bla"}, EXIT_FAILURE);

  // check if the bootstraping was successful
  EXPECT_TRUE(
      router.expect_output("Invalid value for --account-create option.  Valid "
                           "values: always, if-not-exists, never"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 * verify that --account-create given twice produces an error and exits
 */
TEST_F(AccountReuseBadCmdlineTest, account_create_given_twice) {
  // launch the router in bootstrap mode
  auto &router = ProcessManager::launch_router(
      {"-B=0", "--account", "user1", "--account-create", "never",
       "--account-create", "never"},
      EXIT_FAILURE);

  // check if the bootstraping was successful
  EXPECT_TRUE(
      router.expect_output("Option --account-create can only be given once"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 * verify that `--account-create never` and `--account-host <host>` produce an
 * error and exit
 *
 * WL13177:TS_FR10_01
 */
TEST_F(AccountReuseBadCmdlineTest, account_create_never_and_account_host) {
  // launch the router in bootstrap mode
  auto &router = ProcessManager::launch_router(
      {
          "-B=0", "--account", "user1", "--account-create", "never",
          "--account-host", "foo",  // even '%' would not be allowed
      },
      EXIT_FAILURE);

  // check if the bootstraping was successful
  EXPECT_TRUE(
      router.expect_output("Option '--account-create never' cannot be used "
                           "together with '--account-host <host>'"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 * verify that --strict without --bootstrap switch produces an error and exits
 *
 * WL13177:TS_FR16_01
 */
TEST_F(AccountReuseBadCmdlineTest, strict_without_bootstrap_switch) {
  // launch the router in bootstrap mode
  auto &router = ProcessManager::launch_router({"--strict"}, EXIT_FAILURE);

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output(
      "Option --strict can only be used together with -B/--bootstrap"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// SIMPLE POSITIVE TESTS                                                      //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

class AccountReuseTest : public AccountReuseTestBase {};

/**
 * @test
 * simple bootstrap without any options
 * verify that bootstrap will:
 * - create a new account with autogenerated name
 * - verify config is written and contains autogenerated username
 * - verify expected password prompts are presented
 *
 * WL13177:TS_FR11_01
 */
TEST_F(AccountReuseTest, simple) {
  // no config exists yet
  TempDirectory bootstrap_directory;

  // test params
  const std::vector<std::string> args;
  const std::set<std::string>
      existing_hosts;  // kAutoGenUser@% doesn't exist yet

  // expectations
  int exp_exit_code = EXIT_SUCCESS;
  const std::string exp_output = kBootstrapSuccessMsg;
  const std::string exp_username = "mysql_router1_" /* random suffix follows*/;
  const std::string exp_password = kAutoGenUserPassword;
  const std::string exp_password_hash = kAutoGenUserPasswordHash;
  const std::set<std::string> exp_attempt_create_hosts = {"%"};
  std::vector<std::string> exp_sql = {"CREATE USER IF NOT EXISTS",
                                      "GRANT SELECT ON "};
  std::vector<std::string> unexp_sql = {"DROP USER"};

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // run bootstrap
  ProcessWrapper &router = launch_bootstrap(exp_exit_code, server_port,
                                            bootstrap_directory.name(), args);
  add_login_hook(router, exp_password, exp_username);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(args));
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               exp_username);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);
}

/**
 * @test
 * verify that --account-host:
 * - works in general
 * - works for simple case, implicit --account-host
 */
TEST_F(AccountReuseTest, no_host_patterns) {
  for (bool root_password_on_cmdline : {true, false}) {
    TempDirectory bootstrap_directory;

    // extract test params
    const std::vector<std::string> args = {
        "--account",
        kAccountUser,
    };
    const std::string exp_output = kBootstrapSuccessMsg;
    int exp_exit_code = EXIT_SUCCESS;
    const std::set<std::string> exp_created_account_hosts = {"%"};
    //
    const std::string exp_username = kAccountUser;
    const std::string exp_password = kAccountUserPassword;
    const bool if_not_exists = true;  // default
    CustomResponses cr =
        gen_sql_for_creating_accounts(exp_username, exp_created_account_hosts);

    // launch mock server and wait for it to start accepting connections
    const uint16_t server_port = port_pool_.get_next_available();
    const uint16_t server_http_port = port_pool_.get_next_available();
    ProcessWrapper &server_mock =
        launch_mock_server(server_port, server_http_port);
    set_mock_server_sql_statements(server_http_port, cr.stmts);

    // run bootstrap
    ProcessWrapper &router =
        launch_bootstrap(exp_exit_code, server_port, bootstrap_directory.name(),
                         args, root_password_on_cmdline);
    add_login_hook(router, exp_password, exp_username,
                   root_password_on_cmdline);

    // check outcome
    DebugDumper dd(router, server_mock, server_http_port);
    check_bootstrap_success(router, exp_output);
    check_questions_asked_by_bootstrap(exp_exit_code, router,
                                       is_using_account(args),
                                       root_password_on_cmdline);
    check_keyring(bootstrap_directory.name(), true, exp_username, exp_password);
    check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                 exp_username);
    check_user_creating_SQL_calls(exp_username, exp_created_account_hosts,
                                  if_not_exists, server_http_port);
  }
}

/**
 * @test
 * verify that --account-host:
 * - works in general
 * - can be applied multiple times in one go
 * - can take '%' as a parameter
 * - redundant hosts are ignored
 */
TEST_F(AccountReuseTest, multiple_host_patterns) {
  for (bool root_password_on_cmdline : {true, false}) {
    TempDirectory bootstrap_directory;

    // extract test params
    const std::vector<std::string> args = {
        "--account",      kAccountUser,
        "--account-host", kHostA_notInDB,  // 2nd CREATE USER
        "--account-host", "%",             // 1st CREATE USER
        "--account-host", kHostA_notInDB,  // \_ redundant, ignored
        "--account-host", kHostA_notInDB,  // /
        "--account-host", kHostB_notInDB,  // 3rd CREATE USER
    };
    const std::string exp_output = kBootstrapSuccessMsg;
    int exp_exit_code = EXIT_SUCCESS;
    const std::set<std::string> exp_created_account_hosts = {
        kHostA_notInDB, kHostB_notInDB, "%"};
    //
    const std::string exp_username = kAccountUser;
    const std::string exp_password = kAccountUserPassword;
    const bool if_not_exists = true;  // default
    CustomResponses cr =
        gen_sql_for_creating_accounts(exp_username, exp_created_account_hosts);

    // launch mock server and wait for it to start accepting connections
    const uint16_t server_port = port_pool_.get_next_available();
    const uint16_t server_http_port = port_pool_.get_next_available();
    ProcessWrapper &server_mock =
        launch_mock_server(server_port, server_http_port);
    set_mock_server_sql_statements(server_http_port, cr.stmts);

    // run bootstrap
    ProcessWrapper &router =
        launch_bootstrap(exp_exit_code, server_port, bootstrap_directory.name(),
                         args, root_password_on_cmdline);
    add_login_hook(router, exp_password, exp_username,
                   root_password_on_cmdline);

    // check outcome
    DebugDumper dd(router, server_mock, server_http_port);
    check_bootstrap_success(router, exp_output);
    check_questions_asked_by_bootstrap(exp_exit_code, router,
                                       is_using_account(args),
                                       root_password_on_cmdline);
    check_keyring(bootstrap_directory.name(), true, exp_username, exp_password);
    check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                 exp_username);
    check_user_creating_SQL_calls(exp_username, exp_created_account_hosts,
                                  if_not_exists, server_http_port);
  }
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This parametrised test runs various combinations of --account-create and   //
// and --account-host switches vs various accounts (hostnames) already        //
// existing                                                                   //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

// This struct defines (user creation oriented) SQL statements and their
// responses, which will be emulated by the MockServer.
struct MockServerResponses {
  std::set<std::string> exp_cu_hosts = {};  // CREATE USER won't run if empty
  std::string mock_res_cu = "";  // should be "" if exp_cu_hosts.empty()
  std::string mock_res_sw = "";  // SHOW WARNINGS will not run if empty
  std::set<std::string> exp_gr_hosts = {};  // GRANTs will not run if empty
  bool rollback = false;  // CREATE USER will fail and trigger ROLLBACK
};
struct RouterAccountCreateComboTestParams {
  const std::string test_name;
  const std::string username;
  const std::vector<std::string> extra_args;
  const std::set<std::string> account_host_args;
  MockServerResponses database_ops;
  const std::string exp_output;
  int exp_exit_code;
};
class AccountReuseCreateComboTestP
    : public AccountReuseTestBase,
      public ::testing::WithParamInterface<RouterAccountCreateComboTestParams> {
 public:
  static std::vector<RouterAccountCreateComboTestParams> gen_testcases() {
    const std::string A = kHostA_notInDB;
    const std::string B = kHostB_notInDB;
    const std::string C = kHostC_inDB;
    const std::string D = kHostD_inDB;

    const std::string HOST = get_local_hostname();
    const std::string IP = get_local_ip(HOST);

    const std::string kColonUser = kAccountUser + ":" + kAccountUserPassword;

    return {

        // C = 'host9', D = '%'

        // create implicitly % (doesn't exist)
        /* TS_FR02_01 */
        {"create_implicit_P_dne___n",
         kAccountUser,
         {"--account-create", "never"},
         {/* % */},
         {{}, "", "", {}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},  // would fail with --strict

        /* TS_FR01_04 */
        {"create_implicit_P_dne___d",
         kAccountUser,
         {/* defaults to if-not-exists */},
         {/* % */},
         {{"%"}, res_create_user(0), "", {"%"}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FR04_02 */
        {"create_implicit_P_dne___i",
         kAccountUser,
         {"--account-create", "if-not-exists"},
         {/* % */},
         {{"%"}, res_create_user(0), "", {"%"}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FR03_02 */
        {"create_implicit_P_dne___a",
         kAccountUser,
         {"--account-create", "always"},
         {/* % */},
         {{"%"}, res_create_user(0), "", {"%"}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        // create implicitly % (exists)
        /* TS_FR02_02 */
        {"create_implicit_P_exists___n",
         kAccountUser,
         {"--account-create", "never"},
         {/* % */},
         {{}, "", "", {}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FR05_01 */
        {"create_implicit_P_exists___d",
         kAccountUser,
         {/* defaults to if-not-exists */},
         {/* % */},
         {{"%"},
          res_create_user(1),
          res_show_warnings(kAccountUser, {"%"}),
          {}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FR04_01 */
        {"create_implicit_P_exists___i",
         kAccountUser,
         {"--account-create", "if-not-exists"},
         {/* % */},
         {{"%"},
          res_create_user(1),
          res_show_warnings(kAccountUser, {"%"}),
          {}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FR03_01 */
        {"create_implicit_P_exists___a",
         kAccountUser,
         {"--account-create", "always"},
         {/* % */},
         {{"%"}, res_cu_error(kAccountUser, {"%"}), "", {}, true},
         existing_host_err_msg(kAccountUser, {"%"}),
         EXIT_FAILURE},

        // create A (doesn't exist)
        /* TS_FRxxxxx */
        {"create_A_dne___d",
         kAccountUser,
         {/* defaults to if-not-exists */},
         {A},
         {{A}, res_create_user(0), "", {A}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_A_dne___i",
         kAccountUser,
         {"--account-create", "if-not-exists"},
         {A},
         {{A}, res_create_user(0), "", {A}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_A_dne___a",
         kAccountUser,
         {"--account-create", "always"},
         {A},
         {{A}, res_create_user(0), "", {A}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        // create A (doesn't exist), B (doesn't exist)
        /* TS_FRxxxxx */
        {"create_A_exists___d",
         kAccountUser,
         {/* defaults to if-not-exists */},
         {A, B},
         {{A, B}, res_create_user(0), "", {A, B}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_A_exists___i",
         kAccountUser,
         {"--account-create", "if-not-exists"},
         {A, B},
         {{A, B}, res_create_user(0), "", {A, B}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_A_exists___a",
         kAccountUser,
         {"--account-create", "always"},
         {A, B},
         {{A, B}, res_create_user(0), "", {A, B}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        // create A (doesn't exist), C (exists)
        /* TS_FRxxxxx */
        {"create_A_dne_C_exists___d",
         kAccountUser,
         {/* defaults to if-not-exists */},
         {A, C},
         {{A, C},
          res_create_user(1),
          res_show_warnings(kAccountUser, {C}),
          {A}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_A_dne_C_exists___i",
         kAccountUser,
         {"--account-create", "if-not-exists"},
         {A, C},
         {{A, C},
          res_create_user(1),
          res_show_warnings(kAccountUser, {C}),
          {A}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_A_dne_C_exists___a",
         kAccountUser,
         {"--account-create", "always"},
         {A, C},
         {{A, C}, res_cu_error(kAccountUser, {C}), "", {}, true},
         existing_host_err_msg(kAccountUser, {C}),
         EXIT_FAILURE},

        // create C (exists), D (exists)
        /* TS_FRxxxxx */
        {"create_C_exists_D_exists___d",
         kAccountUser,
         {/* defaults to if-not-exists */},
         {C, D},
         {{C, D},
          res_create_user(2),
          res_show_warnings(kAccountUser, {C, D}),
          {}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_C_exists_D_exists___i",
         kAccountUser,
         {"--account-create", "if-not-exists"},
         {C, D},
         {{C, D},
          res_create_user(2),
          res_show_warnings(kAccountUser, {C, D}),
          {}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_C_exists_D_exists___a",
         kAccountUser,
         {"--account-create", "always"},
         {C, D},
         {{C, D}, res_cu_error(kAccountUser, {C, D}), "", {}, true},
         existing_host_err_msg(kAccountUser, {C, D}),
         EXIT_FAILURE},

        // create A (doesn't exist), C (exists), D (exists)
        /* TS_FRxxxxx */
        {"create_A_dne_C_exists_D_exists___d",
         kAccountUser,
         {/* defaults to if-not-exists */},
         {C, A, D},
         {{A, C, D},
          res_create_user(2),
          res_show_warnings(kAccountUser, {C, D}),
          {A}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_A_dne_C_exists_D_exists___i",
         kAccountUser,
         {"--account-create", "if-not-exists"},
         {C, A, D},
         {{A, C, D},
          res_create_user(2),
          res_show_warnings(kAccountUser, {C, D}),
          {A}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_A_dne_C_exists_D_exists___a",
         kAccountUser,
         {"--account-create", "always"},
         {C, A, D},
         {{A, C, D}, res_cu_error(kAccountUser, {A}), "", {}, true},
         existing_host_err_msg(kAccountUser, {A}),
         EXIT_FAILURE},

        // create local_ip (doesn't exist), local_hostname (doesn't exist), %
        // (doesn't exist)
        /* TS_FRxxxxx */
        {"create_IP_dne_HOST_dne_P_dne___d",
         kAccountUser,
         {/* defaults to if-not-exists */},
         {IP, HOST, "%"},
         {{IP, HOST, "%"}, res_create_user(0), "", {IP, HOST, "%"}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FR04_06 */
        {"create_IP_dne_HOST_dne_P_dne___i",
         kAccountUser,
         {"--account-create", "if-not-exists"},
         {IP, HOST, "%"},
         {{IP, HOST, "%"}, res_create_user(0), "", {IP, HOST, "%"}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FR03_04 */
        {"create_IP_dne_HOST_dne_P_dne___a",
         kAccountUser,
         {"--account-create", "always"},
         {IP, HOST, "%"},
         {{IP, HOST, "%"}, res_create_user(0), "", {IP, HOST, "%"}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        // create local_ip (doesn't exist)
        /* TS_FRxxxxx */
        {"create_IP_dne___d",
         kAccountUser,
         {/* defaults to if-not-exists */},
         {IP},
         {{IP}, res_create_user(0), "", {IP}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_IP_dne___i",
         kAccountUser,
         {"--account-create", "if-not-exists"},
         {IP},
         {{IP}, res_create_user(0), "", {IP}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FR03_06 */
        {"create_IP_dne___a",
         kAccountUser,
         {"--account-create", "always"},
         {IP},
         {{IP}, res_create_user(0), "", {IP}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        // create % (exists)
        /* TS_FRxxxxx */
        {"create_P_exists___d",
         kAccountUser,
         {/* defaults to if-not-exists */},
         {"%"},
         {{"%"},
          res_create_user(1),
          res_show_warnings(kAccountUser, {"%"}),
          {}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_P_exists___i",
         kAccountUser,
         {"--account-create", "if-not-exists"},
         {"%"},
         {{"%"},
          res_create_user(1),
          res_show_warnings(kAccountUser, {"%"}),
          {}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FR03_07 */
        {"create_P_exists___a",
         kAccountUser,
         {"--account-create", "always"},
         {"%"},
         {{"%"}, res_cu_error(kAccountUser, {"%"}), "", {}, true},
         existing_host_err_msg(kAccountUser, {"%"}),
         EXIT_FAILURE},

        // create local_hostname (doesn't exist)
        /* TS_FRxxxxx */
        {"create_HOST_dne___d",
         kAccountUser,
         {/* defaults to if-not-exists */},
         {HOST},
         {{HOST}, res_create_user(0), "", {HOST}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FR04_04 */
        {"create_HOST_dne___i",
         kAccountUser,
         {"--account-create", "if-not-exists"},
         {HOST},
         {{HOST}, res_create_user(0), "", {HOST}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_HOST_dne___a",
         kAccountUser,
         {"--account-create", "always"},
         {HOST},
         {{HOST}, res_create_user(0), "", {HOST}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        // create implicitly % (exists), account contains ':'
        /* TS_FRxxxxx */
        {"create_implicit_P_exists___userwithcolon___n",
         kColonUser,
         {"--account-create", "never"},
         {/* % */},
         {{}, "", "", {}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_NFR1_01 */
        {"create_implicit_P_exists___userwithcolon___d",
         kColonUser,
         {/* defaults to if-not-exists */},
         {/* % */},
         {{"%"}, res_create_user(1), res_show_warnings(kColonUser, {"%"}), {}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_implicit_P_exists___userwithcolon___i",
         kColonUser,
         {"--account-create", "if-not-exists"},
         {/* % */},
         {{"%"}, res_create_user(1), res_show_warnings(kColonUser, {"%"}), {}},
         kBootstrapSuccessMsg,
         EXIT_SUCCESS},

        /* TS_FRxxxxx */
        {"create_implicit_P_exists___userwithcolon___a",
         kColonUser,
         {"--account-create", "always"},
         {/* % */},
         {{"%"}, res_cu_error(kColonUser, {"%"}), "", {}, true},
         existing_host_err_msg(kColonUser, {"%"}),
         EXIT_FAILURE},
    };
  }
};
INSTANTIATE_TEST_CASE_P(
    foo, AccountReuseCreateComboTestP,
    ::testing::ValuesIn(AccountReuseCreateComboTestP::gen_testcases()),
    [](auto p) -> std::string { return p.param.test_name; });
TEST_P(AccountReuseCreateComboTestP, config_does_not_exist_yet) {
  // extract test params
  std::vector<std::string> extra_args = GetParam().extra_args;
  std::set<std::string> account_host_args = GetParam().account_host_args;
  const std::string exp_output = GetParam().exp_output;
  int exp_exit_code = GetParam().exp_exit_code;
  const MockServerResponses &ops = GetParam().database_ops;
  const std::set<std::string> cu_hosts = ops.exp_cu_hosts;
  const std::set<std::string> gr_hosts = ops.exp_gr_hosts;
  const std::string username = GetParam().username;

  // input: const
  const std::string password = kAccountUserPassword;

  // expectations: expected CREATE USER behaviour
  const bool if_not_exists = is_if_not_exists(extra_args);

  // input: SQL
  std::vector<std::string> unexp_sql = {"DROP USER"};
  CustomResponses cr;
  {
    // CREATE USER [IF NOT EXISTS]
    if (cu_hosts.size()) {
      harness_assert(ops.mock_res_cu.size());

      const std::string account_auth_list =
          make_account_auth_list(username, cu_hosts);
      cr.add(sql_create_user(account_auth_list, if_not_exists),
             ops.mock_res_cu);
    } else {
      unexp_sql.emplace_back("CREATE USER");
    }

    // SHOW WARNINGS
    if (ops.mock_res_sw.size()) {
      harness_assert(ops.exp_cu_hosts.size() && ops.mock_res_cu.size());

      cr.add(sql_show_warnings(), ops.mock_res_sw);
    } else {
      unexp_sql.emplace_back(sql_show_warnings());
    }

    // GRANTs
    if (ops.exp_gr_hosts.size()) {
      harness_assert(ops.exp_cu_hosts.size());

      const std::string al = make_account_list(username, gr_hosts);
      cr.add(sql_grant_1(al));
      cr.add(sql_grant_2(al));
      cr.add(sql_grant_3(al));
      cr.add(sql_grant_4(al));
      cr.add(sql_grant_5(al));
      cr.add(sql_grant_6(al));
    } else {
      unexp_sql.emplace_back("GRANT");
    }

    // ROLLBACK
    if (ops.rollback) {
      cr.add(sql_rollback());
    } else {
      unexp_sql.emplace_back(sql_rollback());
    }
  }

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // add expected creation SQL statements to JS
  set_mock_server_sql_statements(server_http_port, cr.stmts);

  // populate extra cmdline args
  for (const std::string &h : account_host_args) {
    extra_args.push_back("--account-host");
    extra_args.push_back(h);
  }
  extra_args.emplace_back("--account");
  extra_args.emplace_back(username);

  // run bootstrap
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(
      exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
  add_login_hook(router, password, username);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(extra_args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                username, password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               username);
  check_SQL_calls(server_http_port, cr.exp_sql, unexp_sql);
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// RECONFIGURE TESTS                                                          //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

class AccountReuseReconfigurationTest : public AccountReuseTestBase {};

/**
 * @test
 * bootstrap --account against existing user in database, no config, Router not
 * registered verify that bootstrap will:
 * - use --account username provided on cmdline (rather than autogenerate it)
 *   in CREATE USER
 * - will NOT create a new account (since it already exists, will not run GRANT)
 * - save new password to keyfile
 * - verify config is written and contains --account username provided on
 * cmdline
 * - verify expected password prompts are presented
 *
 * WL13177:TS_FR01_01 (root passowrd given on commandline)
 * WL13177:TS_FR01_03 (root password should be asked via prompt)
 */
TEST_F(AccountReuseReconfigurationTest, user_exists_then_account) {
  for (bool root_password_on_cmdline : {true, false}) {
    // no config exists yet
    TempDirectory bootstrap_directory;

    // test params
    const std::vector<std::string> args = {"--account", kAccountUser};
    const std::set<std::string> existing_hosts = {
        "%"};  // kAccountUser@% exists already

    // expectations
    int exp_exit_code = EXIT_SUCCESS;
    const std::string exp_output = kBootstrapSuccessMsg;
    const std::string exp_username = kAccountUser;
    const std::string exp_password = kAccountUserPassword;
    const std::set<std::string> exp_attempt_create_hosts = {"%"};
    CustomResponses cr = gen_sql_for_creating_accounts(
        exp_username, exp_attempt_create_hosts, existing_hosts);
    std::vector<std::string> exp_sql = cr.exp_sql;
    std::vector<std::string> unexp_sql = {
        "GRANT"};  // account should not be created

    // launch mock server and wait for it to start accepting connections
    const uint16_t server_port = port_pool_.get_next_available();
    const uint16_t server_http_port = port_pool_.get_next_available();
    ProcessWrapper &server_mock =
        launch_mock_server(server_port, server_http_port);
    set_mock_server_sql_statements(server_http_port, cr.stmts);

    // run bootstrap
    ProcessWrapper &router =
        launch_bootstrap(exp_exit_code, server_port, bootstrap_directory.name(),
                         args, root_password_on_cmdline);
    add_login_hook(router, exp_password, exp_username,
                   root_password_on_cmdline);

    // check outcome
    DebugDumper dd(router, server_mock, server_http_port);
    check_bootstrap_success(router, exp_output);
    check_questions_asked_by_bootstrap(exp_exit_code, router,
                                       is_using_account(args),
                                       root_password_on_cmdline);
    check_keyring(bootstrap_directory.name(), true, exp_username, exp_password);
    check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                 exp_username);
    check_SQL_calls(server_http_port, exp_sql, unexp_sql);
  }
}

/**
 * @test
 * bootstrap --account against existing user in database, no config, Router is
 * registered already verify that bootstrap will:
 * - use --account username provided on cmdline (rather than autogenerate it)
 *   in CREATE USER
 * - will NOT create a new account (since it already exists, will not run GRANT)
 * - save new password to keyfile
 * - verify config is written and contains --account username provided on
 * cmdline
 * - verify expected password prompts are presented
 *
 * WL13177:TS_FR01_02
 */
TEST_F(AccountReuseReconfigurationTest,
       user_exists_router_is_registered_then_account) {
  // this test is similar to TS_FR01_01 and TS_FR01_03, but here:
  // - we have previous bootstrap artifacts (Router registration) in database

  for (bool root_password_on_cmdline : {true, false}) {
    // no config exists yet
    TempDirectory bootstrap_directory;

    // test params
    const std::vector<std::string> args = {"--account", kAccountUser,
                                           "--force"};
    const std::set<std::string> existing_hosts = {
        "%"};  // kAccountUser@% exists already

    // expectations
    int exp_exit_code = EXIT_SUCCESS;
    const std::string exp_output = kBootstrapSuccessMsg;
    const std::string exp_username = kAccountUser;
    const std::string exp_password = kAccountUserPassword;
    const std::set<std::string> exp_attempt_create_hosts = {"%"};
    CustomResponses cr1 = gen_sql_for_registered_router();
    CustomResponses cr2 = gen_sql_for_creating_accounts(
        exp_username, exp_attempt_create_hosts, existing_hosts);
    std::vector<std::string> exp_sql = cr1.exp_sql;
    exp_sql.insert(exp_sql.end(), cr2.exp_sql.begin(), cr2.exp_sql.end());
    std::vector<std::string> unexp_sql = {
        "GRANT"};  // account should not be created

    // launch mock server and wait for it to start accepting connections
    const uint16_t server_port = port_pool_.get_next_available();
    const uint16_t server_http_port = port_pool_.get_next_available();
    ProcessWrapper &server_mock =
        launch_mock_server(server_port, server_http_port);
    set_mock_server_sql_statements(server_http_port,
                                   cr1.stmts + "," + cr2.stmts);

    // run bootstrap
    ProcessWrapper &router =
        launch_bootstrap(exp_exit_code, server_port, bootstrap_directory.name(),
                         args, root_password_on_cmdline);
    add_login_hook(router, exp_password, exp_username,
                   root_password_on_cmdline);

    // check outcome
    DebugDumper dd(router, server_mock, server_http_port);
    check_bootstrap_success(router, exp_output);
    check_questions_asked_by_bootstrap(exp_exit_code, router,
                                       is_using_account(args),
                                       root_password_on_cmdline);
    check_keyring(bootstrap_directory.name(), true, exp_username, exp_password);
    check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                 exp_username);
    check_SQL_calls(server_http_port, exp_sql, unexp_sql);
  }
}

/**
 * @test
 * bootstrap --account against existing user in database, no config, Router not
 * registered verify that bootstrap will:
 * - providing empty password for existing user will work
 * - use --account username provided on cmdline (rather than autogenerate it)
 *   in CREATE USER
 * - will NOT create a new account (since it already exists, will not run GRANT)
 * - save new password to keyfile
 * - verify config is written and contains --account username provided on
 * cmdline
 * - verify expected password prompts are presented
 *
 * WL13177:TS_FR01_05
 */
TEST_F(AccountReuseReconfigurationTest,
       user_exists_then_account_with_empty_password) {
  // this test is similar to TS_FR01_01 and TS_FR01_03, but here:
  // - we supply an empty password for the new account
  // - user already exists

  for (bool root_password_on_cmdline : {true, false}) {
    // no config exists yet
    TempDirectory bootstrap_directory;

    // test params
    const std::vector<std::string> args = {"--account", kAccountUser};
    const std::set<std::string> existing_hosts = {
        "%"};  // kAccountUser@% exists already

    // expectations
    int exp_exit_code = EXIT_SUCCESS;
    const std::string exp_output = kBootstrapSuccessMsg;
    const std::string exp_username = kAccountUser;
    const std::string exp_password = "";
    const std::string exp_password_hash =
        "*BE1BDEC0AA74B4DCB079943E70528096CCA985F8";
    const std::set<std::string> exp_attempt_create_hosts = {"%"};
    CustomResponses cr =
        gen_sql_for_creating_accounts(exp_username, exp_attempt_create_hosts,
                                      existing_hosts, true, exp_password_hash);
    std::vector<std::string> exp_sql = cr.exp_sql;
    std::vector<std::string> unexp_sql = {
        "GRANT"};  // account should not be created

    // launch mock server and wait for it to start accepting connections
    const uint16_t server_port = port_pool_.get_next_available();
    const uint16_t server_http_port = port_pool_.get_next_available();
    ProcessWrapper &server_mock =
        launch_mock_server(server_port, server_http_port);
    set_mock_server_sql_statements(server_http_port, cr.stmts);

    // run bootstrap
    ProcessWrapper &router =
        launch_bootstrap(exp_exit_code, server_port, bootstrap_directory.name(),
                         args, root_password_on_cmdline);
    add_login_hook(router, exp_password, exp_username,
                   root_password_on_cmdline);

    // check outcome
    DebugDumper dd(router, server_mock, server_http_port);
    check_bootstrap_success(router, exp_output);
    check_questions_asked_by_bootstrap(exp_exit_code, router,
                                       is_using_account(args),
                                       root_password_on_cmdline);
    check_keyring(bootstrap_directory.name(), true, exp_username, exp_password);
    check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                 exp_username);
    check_SQL_calls(server_http_port, exp_sql, unexp_sql);
  }
}

/**
 * @test
 * bootstrap --account against no config, Router not registered
 * verify that bootstrap will:
 * - providing empty password for new user will work
 * - use --account username provided on cmdline (rather than autogenerate it)
 *   in CREATE USER and GRANT statements
 * - save new password to keyfile
 * - verify config is written and contains --account username provided on
 * cmdline
 * - verify expected password prompts are presented
 */
TEST_F(AccountReuseReconfigurationTest,
       nothing_then_account_with_empty_password) {
  // this test is like TS_FR01_05, but here:
  // - user doesn't exist yet

  for (bool root_password_on_cmdline : {true, false}) {
    // no config exists yet
    TempDirectory bootstrap_directory;

    // test params
    const std::vector<std::string> args = {"--account", kAccountUser};
    const std::set<std::string> existing_hosts =
        {};  // kAccountUser@% doesn't exist yet

    // expectations
    int exp_exit_code = EXIT_SUCCESS;
    const std::string exp_output = kBootstrapSuccessMsg;
    const std::string exp_username = kAccountUser;
    const std::string exp_password = "";
    const std::string exp_password_hash =
        "*BE1BDEC0AA74B4DCB079943E70528096CCA985F8";
    const std::set<std::string> exp_attempt_create_hosts = {"%"};
    CustomResponses cr =
        gen_sql_for_creating_accounts(exp_username, exp_attempt_create_hosts,
                                      existing_hosts, true, exp_password_hash);
    std::vector<std::string> exp_sql = cr.exp_sql;
    std::vector<std::string> unexp_sql = {};

    // launch mock server and wait for it to start accepting connections
    const uint16_t server_port = port_pool_.get_next_available();
    const uint16_t server_http_port = port_pool_.get_next_available();
    ProcessWrapper &server_mock =
        launch_mock_server(server_port, server_http_port);
    set_mock_server_sql_statements(server_http_port, cr.stmts);

    // run bootstrap
    ProcessWrapper &router =
        launch_bootstrap(exp_exit_code, server_port, bootstrap_directory.name(),
                         args, root_password_on_cmdline);
    add_login_hook(router, exp_password, exp_username,
                   root_password_on_cmdline);

    // check outcome
    DebugDumper dd(router, server_mock, server_http_port);
    check_bootstrap_success(router, exp_output);
    check_questions_asked_by_bootstrap(exp_exit_code, router,
                                       is_using_account(args),
                                       root_password_on_cmdline);
    check_keyring(bootstrap_directory.name(), true, exp_username, exp_password);
    check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                 exp_username);
    check_SQL_calls(server_http_port, exp_sql, unexp_sql);
  }
}

/**
 * @test
 * bootstrap --account against existing config
 * verify that:
 * - bootstap will use --account username (and ignore username from config)
 *   in CREATE USER and GRANT statements
 * - append new password to keyfile
 * ...
 *
 * SIMILAR TO WL13177:TS_FR01_02
 * SIMILAR TO WL13177:TS_FR01_03
 */
TEST_F(AccountReuseReconfigurationTest, noaccount_then_account) {
  for (bool root_password_on_cmdline : {true, false}) {
    // emulate past bootstrap without --account
    TempDirectory bootstrap_directory;
    create_config(bootstrap_directory.name(), kAutoGenUser);
    create_keyring(bootstrap_directory.name(), kAutoGenUser,
                   kAutoGenUserPassword);
    check_keyring(bootstrap_directory.name(), true, kAutoGenUser,
                  kAutoGenUserPassword, false);

    // test params
    const std::vector<std::string> args = {"--account", kAccountUser};

    // expectations
    int exp_exit_code = EXIT_SUCCESS;
    const std::string exp_output = kBootstrapSuccessMsg;
    const std::string exp_username = kAccountUser;
    const std::string exp_password = kAccountUserPassword;
    const std::set<std::string> exp_attempt_create_hosts = {"%"};
    CustomResponses cr =
        gen_sql_for_creating_accounts(exp_username, exp_attempt_create_hosts);
    std::vector<std::string> exp_sql = cr.exp_sql;
    std::vector<std::string> unexp_sql = {};

    // launch mock server and wait for it to start accepting connections
    const uint16_t server_port = port_pool_.get_next_available();
    const uint16_t server_http_port = port_pool_.get_next_available();
    ProcessWrapper &server_mock =
        launch_mock_server(server_port, server_http_port);
    set_mock_server_sql_statements(server_http_port, cr.stmts);

    // run bootstrap
    ProcessWrapper &router =
        launch_bootstrap(exp_exit_code, server_port, bootstrap_directory.name(),
                         args, root_password_on_cmdline);
    add_login_hook(router, exp_password, exp_username,
                   root_password_on_cmdline);

    // check outcome
    DebugDumper dd(router, server_mock, server_http_port);
    check_bootstrap_success(router, exp_output);
    check_questions_asked_by_bootstrap(exp_exit_code, router,
                                       is_using_account(args),
                                       root_password_on_cmdline);
    check_keyring(bootstrap_directory.name(), true, exp_username, exp_password);
    check_keyring(bootstrap_directory.name(), true, kAutoGenUser,
                  kAutoGenUserPassword);  // old
    check_keyring(bootstrap_directory.name(), true, exp_username,
                  exp_password);  // new (appended)
    check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                 exp_username);
    check_SQL_calls(server_http_port, exp_sql, unexp_sql);
  }
}

/**
 * @test
 * bootstrap against existing config previously bootstrapped with --account
 * verify that:
 * - bootstap will re-use the account in the config
 * - password in the keyring will be perserved
 * ...
 */
TEST_F(AccountReuseReconfigurationTest, account_then_noaccount) {
  for (bool root_password_on_cmdline : {true, false}) {
    // emulate past bootstrap with --account
    TempDirectory bootstrap_directory;
    create_config(bootstrap_directory.name(), kAccountUser);
    create_keyring(bootstrap_directory.name(), kAccountUser,
                   kAccountUserPassword);
    check_keyring(bootstrap_directory.name(), true, kAccountUser,
                  kAccountUserPassword, false);

    // test params
    const std::vector<std::string> args;
    const std::set<std::string> existing_hosts = {
        "%"};  // kAccountUser@% exists already

    // expectations
    int exp_exit_code = EXIT_SUCCESS;
    const std::string exp_output = kBootstrapSuccessMsg;
    const std::string exp_username = kAccountUser;
    const std::string exp_password = kAccountUserPassword;
    const std::set<std::string> exp_attempt_create_hosts = {"%"};
    CustomResponses cr = gen_sql_for_creating_accounts(
        exp_username, exp_attempt_create_hosts, existing_hosts);
    std::vector<std::string> exp_sql = cr.exp_sql;
    std::vector<std::string> unexp_sql = {"DROP USER", "GRANT"};

    // launch mock server and wait for it to start accepting connections
    const uint16_t server_port = port_pool_.get_next_available();
    const uint16_t server_http_port = port_pool_.get_next_available();
    ProcessWrapper &server_mock =
        launch_mock_server(server_port, server_http_port);
    set_mock_server_sql_statements(server_http_port, cr.stmts);

    // run bootstrap
    ProcessWrapper &router =
        launch_bootstrap(exp_exit_code, server_port, bootstrap_directory.name(),
                         args, root_password_on_cmdline);
    add_login_hook(router, exp_password, exp_username,
                   root_password_on_cmdline);

    // check outcome
    DebugDumper dd(router, server_mock, server_http_port);
    check_bootstrap_success(router, exp_output);
    check_questions_asked_by_bootstrap(exp_exit_code, router,
                                       is_using_account(args),
                                       root_password_on_cmdline);
    check_keyring(bootstrap_directory.name(), true, exp_username, exp_password);
    check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                 exp_username);
    check_SQL_calls(server_http_port, exp_sql, unexp_sql);
  }
}

/**
 * @test
 * bootstrap against existing config previously bootstrapped without --account
 * (user exists, Router is registered) verify that:
 * - bootstap will re-use the account in the config (will NOT DROP and re-CREATE
 * it)
 * - password in the keyring will be perserved
 * - verify config is written again and contains the same username as before
 * - verify expected password prompts are presented
 *
 * WL13177:TS_FR11_02
 */
TEST_F(AccountReuseReconfigurationTest, noaccount_then_noaccount) {
  for (bool root_password_on_cmdline : {true, false}) {
    // emulate past bootstrap without --account
    TempDirectory bootstrap_directory;
    create_config(bootstrap_directory.name(), kAutoGenUser);
    create_keyring(bootstrap_directory.name(), kAutoGenUser,
                   kAutoGenUserPassword);
    check_keyring(bootstrap_directory.name(), true, kAutoGenUser,
                  kAutoGenUserPassword, false);

    // test params
    const std::vector<std::string> args;
    const std::set<std::string> existing_hosts = {
        "%"};  // kAutoGenUser@% exists already

    // expectations
    int exp_exit_code = EXIT_SUCCESS;
    const std::string exp_output = kBootstrapSuccessMsg;
    const std::string exp_username = kAutoGenUser;
    const std::string exp_password = kAutoGenUserPassword;
    const std::string exp_password_hash = kAutoGenUserPasswordHash;
    const std::set<std::string> exp_attempt_create_hosts = {"%"};

    CustomResponses cr =
        gen_sql_for_creating_accounts(exp_username, exp_attempt_create_hosts,
                                      existing_hosts, true, exp_password_hash);
    std::vector<std::string> exp_sql = cr.exp_sql;
    std::vector<std::string> unexp_sql = {"DROP USER", "GRANT"};

    // launch mock server and wait for it to start accepting connections
    const uint16_t server_port = port_pool_.get_next_available();
    const uint16_t server_http_port = port_pool_.get_next_available();
    ProcessWrapper &server_mock =
        launch_mock_server(server_port, server_http_port);
    set_mock_server_sql_statements(server_http_port, cr.stmts);

    // run bootstrap
    ProcessWrapper &router =
        launch_bootstrap(exp_exit_code, server_port, bootstrap_directory.name(),
                         args, root_password_on_cmdline);
    add_login_hook(router, exp_password, exp_username,
                   root_password_on_cmdline);

    // check outcome
    DebugDumper dd(router, server_mock, server_http_port);
    check_bootstrap_success(router, exp_output);
    check_questions_asked_by_bootstrap(exp_exit_code, router,
                                       is_using_account(args),
                                       root_password_on_cmdline);
    check_keyring(bootstrap_directory.name(), true, exp_username, exp_password);
    check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                 exp_username);
    check_SQL_calls(server_http_port, exp_sql, unexp_sql);
  }
}

/**
 * @test
 * bootstrap against existing config previously bootstrapped with --account,
 * keyring is missing
 * verify that:
 * - bootstap will re-use the account in the config
 * - try to read password from keyring and fail with appropriate message
 */
TEST_F(AccountReuseReconfigurationTest, account_then_noaccount___no_keyring) {
  // emulate past bootstrap with --account and deleted keyring
  TempDirectory bootstrap_directory;
  create_config(bootstrap_directory.name(), kAccountUser);

  // test params
  const std::vector<std::string> args;

  // expectations
  int exp_exit_code = EXIT_FAILURE;
  const std::vector<std::string> exp_output = {
      "Error: Failed retrieving password for user '" + kAccountUser +
          "' from keyring: Can't open file '",
      "mysqlrouter.key': " +
          std::error_condition(std::errc::no_such_file_or_directory).message(),
  };
  const std::string exp_username = kAccountUser;
  const std::string exp_password = kAccountUserPassword;
  const std::set<std::string> exp_attempt_create_hosts = {};
  const bool if_not_exists = true;  // default
  CustomResponses cr =
      gen_sql_for_creating_accounts(exp_username, exp_attempt_create_hosts);
  std::vector<std::string> exp_sql = cr.exp_sql;

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);
  set_mock_server_sql_statements(server_http_port, cr.stmts);

  // run bootstrap
  ProcessWrapper &router = launch_bootstrap(exp_exit_code, server_port,
                                            bootstrap_directory.name(), args);
  add_login_hook(router, exp_password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(args));
  check_keyring(bootstrap_directory.name(), false);
  check_config(bootstrap_directory.name(), true,
               kAccountUser);  // old config file still lives on
  check_user_creating_SQL_calls(kAccountUser, exp_attempt_create_hosts,
                                if_not_exists, server_http_port);
}

/**
 * @test
 * bootstrap against existing config previously bootstrapped with --account,
 * keyring exists but doesn't contain the password for the user of interest
 * verify that:
 * - bootstap will re-use the account in the config
 * - try to read password from keyring and fail with appropriate message
 */
TEST_F(AccountReuseReconfigurationTest,
       account_then_noaccount___keyring_without_needed_password) {
  const std::string kBogusUser = "bogus_user";  // different user than needed

  // emulate past bootstrap with --account and keyring without user->password
  TempDirectory bootstrap_directory;
  create_config(bootstrap_directory.name(), kAccountUser);
  create_keyring(bootstrap_directory.name(), kBogusUser, kAccountUserPassword);
  check_keyring(bootstrap_directory.name(), true, kBogusUser,
                kAccountUserPassword, false);

  // test params
  const std::vector<std::string> args;
  const bool if_not_exists = true;  // default

  // expectations
  int exp_exit_code = EXIT_FAILURE;
  const Path bs_dir_abs_path = Path(bootstrap_directory.name()).real_path();
  const std::vector<std::string> exp_output = {
      // clang-format off
      "- Fetching password for current account (some_user) from keyring",
      "Error: Failed retrieving password for user 'some_user' from keyring:",
      "  Keyring was opened successfully, but it doesn't contain the password for",
      "  user 'some_user'",
      // clang-format on
  };

  const std::string exp_username = kBogusUser;
  const std::string exp_password = kAccountUserPassword;
  const std::set<std::string> exp_attempt_create_hosts = {};

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // run bootstrap
  ProcessWrapper &router = launch_bootstrap(exp_exit_code, server_port,
                                            bootstrap_directory.name(), args);
  add_login_hook(router, exp_password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(args));
  check_keyring(bootstrap_directory.name(), true, exp_username,
                exp_password);  // untouched
  check_config(bootstrap_directory.name(), true,
               kAccountUser);  // old config file still lives on
  check_user_creating_SQL_calls(exp_username, exp_attempt_create_hosts,
                                if_not_exists, server_http_port);
}

/**
 * @test
 * bootstrap against existing config bootstrapped previously with --account
 * (user exists, Router is registered), but keyring contains INCORRECT password
 * verify that:
 * - bootstap will re-use the account in the config
 * - it will proceed with account setup
 * - will fail account validation (due to auth failure) when trying to log in
 *   using bad password
 * - exit with success (since we ran without --strict)
 *
 * This test defines what is expected in such cornercase (or rather, what is
 * NOT EXPECTED), which would be: Bootstrap to figure out that the password in
 * keyring is invalid.
 * It would not be that easy, because Bootstrap can never know if auth failed
 * due to incorrect password, or incorrect account name (which could be wrong
 * due to wrong hostname part, and we have no control over nor a way to figure
 * out what hosname was actually used))
 */
TEST_F(AccountReuseReconfigurationTest,
       account_then_noaccount___keyring_with_incorrect_password) {
  const std::string kIncorrectPassword = "incorrect password";

  // emulate past bootstrap with --account and keyring containing bad password
  TempDirectory bootstrap_directory;
  create_config(bootstrap_directory.name(), kAccountUser);
  create_keyring(bootstrap_directory.name(), kAccountUser, kIncorrectPassword);
  check_keyring(bootstrap_directory.name(), true, kAccountUser,
                kIncorrectPassword, false);

  // test params
  const std::vector<std::string> args;
  const std::set<std::string> existing_hosts = {
      "%"};  // kAutoGenUser@% exists already

  // expectations
  int exp_exit_code = EXIT_SUCCESS;
  std::vector<std::string> exp_output = acct_val_failed_warning_msg();
  exp_output.push_back("Error connecting to MySQL server at 127.0.0.1:");
  exp_output.push_back(": Access Denied for user '"s + kAccountUser +
                       "'@'localhost' (1045)");
  const std::string exp_username = kAccountUser;
  const std::string exp_password = kIncorrectPassword;
  const std::string exp_password_hash =
      "*9069521302781A37BA17CF929625B9C91B886386";
  const std::set<std::string> exp_attempt_create_hosts = {"%"};

  CustomResponses cr =
      gen_sql_for_creating_accounts(exp_username, exp_attempt_create_hosts,
                                    existing_hosts, true, exp_password_hash);
  std::vector<std::string> exp_sql = cr.exp_sql;
  std::vector<std::string> unexp_sql = {
      "DROP USER",
      "GRANT",  // no new accounts were created
      sql_val1(), sql_val2(),
      sql_val3()  // shouldn't get that far due to conn failure
  };

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);
  set_mock_server_sql_statements(
      server_http_port,
      cr.stmts);  // we don't set Router account username here,
  // which will trigger auth failure we're after.  Testcase requires that the
  // username is correct and the password is incorrect, but by providing an
  // incorrect username instead we achieve the exact same effect at the server
  // mock level, only with simpler code

  // run bootstrap
  ProcessWrapper &router = launch_bootstrap(exp_exit_code, server_port,
                                            bootstrap_directory.name(), args);
  add_login_hook(router, "account password will not be asked");

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(args));
  check_keyring(bootstrap_directory.name(), true, exp_username,
                exp_password);  // untouched
  check_config(bootstrap_directory.name(), true,
               exp_username);  // old config file still lives on
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// SHOW WARNINGS TESTS                                                        //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

class ShowWarningsProcessorTest : public AccountReuseTestBase {};

/**
 * @test
 * bootstrap with 3 --account-host, sunny day scenario
 * verify that:
 * - SHOW WARNINGS is not called
 * - bootstrap succeeds
 * - all 3 accounts are given GRANTs
 */
TEST_F(ShowWarningsProcessorTest, no_accounts_exist) {
  // input: other
  const std::set<std::string> account_hosts = {"h1", "h2", "h3"};
  std::vector<std::string> extra_args = {"--account", kAccountUser};
  const bool if_not_exists = true;  // default
  const std::string username = kAccountUser;
  const std::string password = kAccountUserPassword;

  // input: SQL
  const std::string account_auth_list =
      make_account_auth_list(username, account_hosts);
  const std::string al = make_account_list(username, account_hosts);
  std::string custom_responses =
      stmt_resp(sql_create_user(account_auth_list, if_not_exists)) + "," +
      stmt_resp(sql_grant_1(al)) + "," + stmt_resp(sql_grant_2(al)) + "," +
      stmt_resp(sql_grant_3(al)) + "," + stmt_resp(sql_grant_4(al)) + "," +
      stmt_resp(sql_grant_5(al)) + "," + stmt_resp(sql_grant_6(al));

  // expectations: SQL
  const std::vector<std::string> exp_sql = {
      sql_create_user(account_auth_list, if_not_exists),
      sql_grant_1(al),
      sql_grant_2(al),
      sql_grant_3(al),
      sql_grant_4(al),
      sql_grant_5(al),
      sql_grant_6(al),
  };
  const std::vector<std::string> unexp_sql = {};

  // expectations: other
  const std::string exp_output = kBootstrapSuccessMsg;
  int exp_exit_code = EXIT_SUCCESS;

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // add expected creation SQL statements to JS
  set_mock_server_sql_statements(server_http_port, custom_responses);

  // run bootstrap
  for (const std::string &h : account_hosts) {
    extra_args.push_back("--account-host");
    extra_args.push_back(h);
  }
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(
      exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
  add_login_hook(router, password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);

  // consistency checks
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(extra_args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                username, password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               username);
}

/**
 * @test
 * create 3 accounts with IF NOT EXISTS, 1 already exists
 * verify that:
 * - SHOW WARNINGS mechanism works
 * - bootstrap succeeds
 * - only non-existing accounts are given GRANTs
 */
TEST_F(ShowWarningsProcessorTest, one_account_exists) {
  // input: other
  const std::set<std::string> account_hosts = {"h1", "h2", "h3"};
  const std::set<std::string> existing_hosts = {"h1"};
  std::set<std::string> new_hosts = {"h2", "h3"};
  std::vector<std::string> extra_args = {"--account", kAccountUser};
  const bool if_not_exists = true;  // default
  const std::string username = kAccountUser;
  const std::string password = kAccountUserPassword;

  // input: SQL
  const std::string account_auth_list =
      make_account_auth_list(username, account_hosts);
  const std::string al = make_account_list(username, new_hosts);

  std::string custom_responses =
      stmt_resp(sql_create_user(account_auth_list, if_not_exists),
                res_create_user(existing_hosts.size())) +
      "," +
      stmt_resp(sql_show_warnings(),
                res_show_warnings(username, existing_hosts)) +
      "," + stmt_resp(sql_grant_1(al)) + "," + stmt_resp(sql_grant_2(al)) +
      "," + stmt_resp(sql_grant_3(al)) + "," + stmt_resp(sql_grant_4(al)) +
      "," + stmt_resp(sql_grant_5(al)) + "," + stmt_resp(sql_grant_6(al));

  // expectations: SQL
  const std::vector<std::string> exp_sql = {
      sql_create_user(account_auth_list, if_not_exists),
      sql_show_warnings(),
      sql_grant_1(al),
      sql_grant_2(al),
      sql_grant_3(al),
      sql_grant_4(al),
      sql_grant_5(al),
      sql_grant_6(al),
  };
  const std::vector<std::string> unexp_sql = {};

  // expectations: other
  const std::string exp_output = kBootstrapSuccessMsg;
  int exp_exit_code = EXIT_SUCCESS;

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // add expected creation SQL statements to JS
  set_mock_server_sql_statements(server_http_port, custom_responses);

  // run bootstrap
  for (const std::string &h : account_hosts) {
    extra_args.push_back("--account-host");
    extra_args.push_back(h);
  }
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(
      exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
  add_login_hook(router, password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);

  // consistency checks
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(extra_args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                username, password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               username);
}

/**
 * @test
 * create 3 accounts with IF NOT EXISTS, 2 already exist
 * verify that:
 * - SHOW WARNINGS mechanism works
 * - bootstrap succeeds
 * - only non-existing accounts are given GRANTs
 */
TEST_F(ShowWarningsProcessorTest, two_accounts_exist) {
  // input: other
  const std::set<std::string> account_hosts = {"h1", "h2", "h3"};
  const std::set<std::string> existing_hosts = {"h1", "h3"};
  std::set<std::string> new_hosts = {"h2"};

  std::vector<std::string> extra_args = {"--account", kAccountUser};
  const bool if_not_exists = true;  // default
  const std::string username = kAccountUser;
  const std::string password = kAccountUserPassword;

  // input: SQL
  const std::string account_auth_list =
      make_account_auth_list(username, account_hosts);
  const std::string al = make_account_list(username, new_hosts);
  std::string custom_responses =
      stmt_resp(sql_create_user(account_auth_list, if_not_exists),
                res_create_user(existing_hosts.size())) +
      "," +
      stmt_resp(sql_show_warnings(),
                res_show_warnings(username, existing_hosts)) +
      "," + stmt_resp(sql_grant_1(al)) + "," + stmt_resp(sql_grant_2(al)) +
      "," + stmt_resp(sql_grant_3(al)) + "," + stmt_resp(sql_grant_4(al)) +
      "," + stmt_resp(sql_grant_5(al)) + "," + stmt_resp(sql_grant_6(al));

  // expectations: SQL
  const std::vector<std::string> exp_sql = {
      sql_create_user(account_auth_list, if_not_exists),
      sql_show_warnings(),
      sql_grant_1(al),
      sql_grant_2(al),
      sql_grant_3(al),
      sql_grant_4(al),
      sql_grant_5(al),
      sql_grant_6(al),
  };
  const std::vector<std::string> unexp_sql = {};

  // expectations: other
  const std::string exp_output = kBootstrapSuccessMsg;
  int exp_exit_code = EXIT_SUCCESS;

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // add expected creation SQL statements to JS
  set_mock_server_sql_statements(server_http_port, custom_responses);

  // run bootstrap
  for (const std::string &h : account_hosts) {
    extra_args.push_back("--account-host");
    extra_args.push_back(h);
  }
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(
      exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
  add_login_hook(router, password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);

  // consistency checks
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(extra_args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                username, password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               username);
}

/**
 * @test
 * create 3 accounts with IF NOT EXISTS, all 3 already exist
 * verify that:
 * - SHOW WARNINGS mechanism works
 * - bootstrap succeeds
 * - only non-existing accounts are given GRANTs (that's none, in this case)
 */
TEST_F(ShowWarningsProcessorTest, all_accounts_exist) {
  // input: other
  const std::set<std::string> account_hosts = {
      "h1", "s0me.c0mpl3x-VAL1D_h0s7name.%", "a%b"};
  const std::set<std::string> existing_hosts = {
      "h1", "s0me.c0mpl3x-VAL1D_h0s7name.%", "a%b"};
  std::vector<std::string> extra_args = {"--account", kAccountUser};
  const bool if_not_exists = true;  // default
  const std::string username = kAccountUser;
  const std::string password = kAccountUserPassword;

  // input: SQL
  const std::string account_auth_list =
      make_account_auth_list(username, account_hosts);
  std::string custom_responses =
      stmt_resp(sql_create_user(account_auth_list, if_not_exists),
                res_create_user(existing_hosts.size())) +
      "," +
      stmt_resp(sql_show_warnings(),
                res_show_warnings(username, existing_hosts));

  // expectations: SQL
  const std::vector<std::string> exp_sql = {
      sql_create_user(account_auth_list, if_not_exists),
      sql_show_warnings(),
  };
  const std::vector<std::string> unexp_sql = {};

  // expectations: other
  const std::string exp_output = kBootstrapSuccessMsg;
  int exp_exit_code = EXIT_SUCCESS;

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // add expected creation SQL statements to JS
  set_mock_server_sql_statements(server_http_port, custom_responses);

  // run bootstrap
  for (const std::string &h : account_hosts) {
    extra_args.push_back("--account-host");
    extra_args.push_back(h);
  }
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(
      exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
  add_login_hook(router, password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);

  // consistency checks
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(extra_args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                username, password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               username);
}

/**
 * @test
 * create 3 accounts with IF NOT EXISTS, 2 already exist, but SHOW WARNINGS
 * returns an unrecognised warning code for one of them verify that:
 * - SHOW WARNINGS mechanism will ignore warnings for the user with unrecognised
 * warning code (3163) (the idea is for SHOW WARNINGS to ignore any warnings it
 * doesn't understand, changing warning code should be enough to trigger that)
 * - bootstrap succeeds
 * - only non-existing accounts are given GRANTs
 */
TEST_F(ShowWarningsProcessorTest,
       show_warnings_returns_unrecognised_warning_code) {
  // input: other
  const std::set<std::string> account_hosts = {"h1", "h2", "h3"};
  const std::set<std::string> existing_hosts = {
      "h1", "h2"};  // 'h1' should be ignored due to wrong warning code
  std::set<std::string> new_hosts = {"h1", "h3"};  // and be treated as new
  std::vector<std::string> extra_args = {"--account", kAccountUser};
  const bool if_not_exists = true;  // default
  const std::string username = kAccountUser;
  const std::string password = kAccountUserPassword;

  // here we tweak SHOW WARNINGS results:
  // - we change error code in row for host 'h1' (s/3163/42/)
  // - we leave row for host 'h2' intact
  // SHOW WARNINGS processing logic should ignore rows with unrecognised error
  // codes, therefore it should ignore 'h1' and act as usual on 'h2'
  const std::string needle = R"([ "Note", )"s + kUserExistsCode +
                             R"(, "Authorization ID ')"s + username +
                             R"('@'h1' already exists." ])"s;
  const std::string noodle = R"([ "Note", 42, "Authorization ID ')"s +
                             username + R"('@'h1' already exists." ])"s;
  std::string show_warnings_res = res_show_warnings(username, existing_hosts);
  show_warnings_res.replace(show_warnings_res.find(needle), needle.size(),
                            noodle);

  // input: SQL
  const std::string account_auth_list =
      make_account_auth_list(username, account_hosts);
  const std::string al = make_account_list(username, new_hosts);
  std::string custom_responses =
      stmt_resp(sql_create_user(account_auth_list, if_not_exists),
                res_create_user(existing_hosts.size())) +
      "," + stmt_resp(sql_show_warnings(), show_warnings_res) + "," +
      stmt_resp(sql_grant_1(al)) + "," + stmt_resp(sql_grant_2(al)) + "," +
      stmt_resp(sql_grant_3(al)) + "," + stmt_resp(sql_grant_4(al)) + "," +
      stmt_resp(sql_grant_5(al)) + "," + stmt_resp(sql_grant_6(al));

  // expectations: SQL
  const std::vector<std::string> exp_sql = {
      sql_create_user(account_auth_list, if_not_exists),
      sql_show_warnings(),
      sql_grant_1(al),
      sql_grant_2(al),
      sql_grant_3(al),
      sql_grant_4(al),
      sql_grant_5(al),
      sql_grant_6(al),
  };
  const std::vector<std::string> unexp_sql = {};

  // expectations: other
  const std::string exp_output = kBootstrapSuccessMsg;
  int exp_exit_code = EXIT_SUCCESS;

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // add expected creation SQL statements to JS
  set_mock_server_sql_statements(server_http_port, custom_responses);

  // run bootstrap
  for (const std::string &h : account_hosts) {
    extra_args.push_back("--account-host");
    extra_args.push_back(h);
  }
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(
      exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
  add_login_hook(router, password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);

  // consistency checks
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(extra_args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                username, password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               username);
}

/**
 * @test
 * create 3 accounts with IF NOT EXISTS, 2 already exist, but SHOW WARNINGS
 * returns an unrecognised hostname in the warning message verify that:
 * - SHOW WARNINGS mechanism will fail validation and produce fatal error with
 * appropriate message
 * - bootstrap fails
 */
TEST_F(ShowWarningsProcessorTest, show_warnings_returns_unrecognised_hostname) {
  // input: other
  const std::set<std::string> account_hosts = {"h1", "h2", "h3"};
  const std::set<std::string> existing_hosts = {"h1", "h2"};
  std::set<std::string> new_hosts = {"h3"};
  std::vector<std::string> extra_args = {"--account", kAccountUser};
  const bool if_not_exists = true;  // default
  const std::string username = kAccountUser;
  const std::string password = kAccountUserPassword;

  // here we tweak SHOW WARNINGS results:
  // - we change error code in row for host 'h1' (s/3163/42/)
  // - we leave row for host 'h2' intact
  // SHOW WARNINGS processing logic should ignore rows with unrecognised error
  // codes, therefore it should ignore 'h1' and act as usual on 'h2'
  const std::string needle = R"([ "Note", )"s + kUserExistsCode +
                             R"(, "Authorization ID ')" + username +
                             R"('@'h1' already exists." ])";
  const std::string noodle = R"([ "Note", )"s + kUserExistsCode +
                             R"(, "Authorization ID ')" + username +
                             R"('@'hX' already exists." ])";
  std::string show_warnings_res = res_show_warnings(username, existing_hosts);
  show_warnings_res.replace(show_warnings_res.find(needle), needle.size(),
                            noodle);

  // input: SQL
  const std::string account_auth_list =
      make_account_auth_list(username, account_hosts);
  const std::string al = make_account_list(username, new_hosts);
  std::string custom_responses =
      stmt_resp(sql_create_user(account_auth_list, if_not_exists),
                res_create_user(existing_hosts.size())) +
      "," + stmt_resp(sql_show_warnings(), show_warnings_res);

  // expectations: SQL
  const std::vector<std::string> exp_sql = {
      sql_create_user(account_auth_list, if_not_exists),
      sql_show_warnings(),
  };
  const std::vector<std::string> unexp_sql = {};

  // expectations: other
  int exp_exit_code = EXIT_FAILURE;
  std::vector<std::string> exp_output =
      show_warnings_failed_err_msg(make_account_list(username, account_hosts));
  exp_output.emplace_back(
      "Error: SHOW WARNINGS: Unexpected account name 'some_user'@'hX' in "
      "message \"Authorization ID 'some_user'@'hX' already exists.\"");

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // add expected creation SQL statements to JS
  set_mock_server_sql_statements(server_http_port, custom_responses);

  // run bootstrap
  for (const std::string &h : account_hosts) {
    extra_args.push_back("--account-host");
    extra_args.push_back(h);
  }
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(
      exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
  add_login_hook(router, password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);

  // consistency checks
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(extra_args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                username, password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               username);
}

/**
 * @test
 * create 3 accounts with IF NOT EXISTS, 2 already exist, but SHOW WARNINGS
 * returns an unrecognised username@hostname pattern (regex matching fails) in
 * the warning message verify that:
 * - SHOW WARNINGS mechanism will fail validation and produce fatal error with
 * appropriate message
 * - bootstrap fails
 */
TEST_F(ShowWarningsProcessorTest,
       show_warnings_returns_message_with_unrecognised_account_pattern) {
  // input: other
  const std::set<std::string> account_hosts = {"h1", "h2", "h3"};
  const std::set<std::string> existing_hosts = {"h1", "h2"};
  std::set<std::string> new_hosts = {"h3"};
  std::vector<std::string> extra_args = {"--account", kAccountUser};
  const bool if_not_exists = true;  // default
  const std::string username = kAccountUser;
  const std::string password = kAccountUserPassword;

  // here we tweak SHOW WARNINGS results:
  // - we change username in row for host 'h1' (s/<kAccountUser>/foobar/)
  // - we leave row for host 'h2' intact
  // We're testing the behaviour when SHOW WARNINGS processor can't find
  // <username@hostname> pattern in the warning message (which it needs to
  // extract to learn which accounts already exist).  We could just change
  // '<kAccountUser>@h1' to anything, but we change just the <kAccountUser>
  // (and leave the '@h1' intact) to also test what will happen when it
  // receives a valid <username@hostname> expression, but for a username it did
  // not try to create (and therefore expect).  Such scenario should also lead
  // to the same failure, and so we use this scenario here to test both cases
  // simultaneously, as this is a stricter case.
  const std::string needle = R"([ "Note", )"s + kUserExistsCode +
                             R"(, "Authorization ID ')" + username +
                             R"('@'h1' already exists." ])";
  const std::string noodle = R"([ "Note", )"s + kUserExistsCode +
                             R"(, "Authorization ID ')" + "foobar" +
                             R"('@'h1' already exists." ])";
  std::string show_warnings_res = res_show_warnings(username, existing_hosts);
  show_warnings_res.replace(show_warnings_res.find(needle), needle.size(),
                            noodle);

  // input: SQL
  const std::string account_auth_list =
      make_account_auth_list(username, account_hosts);
  const std::string al = make_account_list(username, new_hosts);
  std::string custom_responses =
      stmt_resp(sql_create_user(account_auth_list, if_not_exists),
                res_create_user(existing_hosts.size())) +
      "," + stmt_resp(sql_show_warnings(), show_warnings_res);

  // expectations: SQL
  const std::vector<std::string> exp_sql = {
      sql_create_user(account_auth_list, if_not_exists),
      sql_show_warnings(),
  };
  const std::vector<std::string> unexp_sql = {};

  // expectations: other
  int exp_exit_code = EXIT_FAILURE;
  std::vector<std::string> exp_output =
      show_warnings_failed_err_msg(make_account_list(username, account_hosts));
  exp_output.emplace_back(
      "Error: SHOW WARNINGS: Failed to extract account name "
      "('some_user'@'<anything>') from message \"Authorization ID "
      "'foobar'@'h1' already exists.\"");

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // add expected creation SQL statements to JS
  set_mock_server_sql_statements(server_http_port, custom_responses);

  // run bootstrap
  for (const std::string &h : account_hosts) {
    extra_args.push_back("--account-host");
    extra_args.push_back(h);
  }
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(
      exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
  add_login_hook(router, password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);

  // consistency checks
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(extra_args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                username, password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               username);
}

/**
 * @test
 * create 3 accounts with IF NOT EXISTS, 2 already exist, but SHOW WARNINGS
 * returns a column with unexpected name verify that:
 * - SHOW WARNINGS mechanism will fail validation and produce fatal error with
 * appropriate message
 * - bootstrap fails
 */
TEST_F(ShowWarningsProcessorTest, show_warnings_returns_invalid_column_names) {
  const std::vector<std::string> kColumnNames = {"Level", "Code", "Message"};
  for (size_t i = 0; i < kColumnNames.size(); i++) {
    const std::string column_name = kColumnNames[i];
    const std::string column_nr = std::to_string(i + 1);

    // input: other
    const std::set<std::string> account_hosts = {"h1", "h2", "h3"};
    const std::set<std::string> existing_hosts = {"h1", "h2"};
    std::set<std::string> new_hosts = {"h3"};
    std::vector<std::string> extra_args = {"--account", kAccountUser};
    const bool if_not_exists = true;  // default
    const std::string username = kAccountUser;
    const std::string password = kAccountUserPassword;

    // here we tweak SHOW WARNINGS results:
    // - we change the name of 1 column
    // We expect that the SHOW WARNINGS processor's validator will notice this
    // and trigger failure
    const std::string needle = R"("name": ")" + column_name + R"(")";
    const std::string noodle = R"("name": "bogus_name")";
    std::string show_warnings_res = res_show_warnings(username, existing_hosts);
    show_warnings_res.replace(show_warnings_res.find(needle), needle.size(),
                              noodle);

    // input: SQL
    const std::string account_auth_list =
        make_account_auth_list(username, account_hosts);
    const std::string al = make_account_list(username, new_hosts);
    std::string custom_responses =
        stmt_resp(sql_create_user(account_auth_list, if_not_exists),
                  res_create_user(existing_hosts.size())) +
        "," + stmt_resp(sql_show_warnings(), show_warnings_res);

    // expectations: SQL
    const std::vector<std::string> exp_sql = {
        sql_create_user(account_auth_list, if_not_exists),
        sql_show_warnings(),
    };
    const std::vector<std::string> unexp_sql = {};

    // expectations: other
    int exp_exit_code = EXIT_FAILURE;
    std::vector<std::string> exp_output = show_warnings_failed_err_msg(
        make_account_list(username, account_hosts));
    exp_output.emplace_back("Error: SHOW WARNINGS: Unexpected column " +
                            column_nr + " name 'bogus_name', expected '" +
                            column_name + "'");

    // launch mock server and wait for it to start accepting connections
    const uint16_t server_port = port_pool_.get_next_available();
    const uint16_t server_http_port = port_pool_.get_next_available();
    ProcessWrapper &server_mock =
        launch_mock_server(server_port, server_http_port);

    // add expected creation SQL statements to JS
    set_mock_server_sql_statements(server_http_port, custom_responses);

    // run bootstrap
    for (const std::string &h : account_hosts) {
      extra_args.push_back("--account-host");
      extra_args.push_back(h);
    }
    TempDirectory bootstrap_directory;
    ProcessWrapper &router = launch_bootstrap(
        exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
    add_login_hook(router, password);

    // check outcome
    DebugDumper dd(router, server_mock, server_http_port);
    check_bootstrap_success(router, exp_output);
    check_SQL_calls(server_http_port, exp_sql, unexp_sql);

    // consistency checks
    check_questions_asked_by_bootstrap(exp_exit_code, router,
                                       is_using_account(extra_args));
    check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                  username, password);
    check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                 username);
  }
}

/**
 * @test
 * create 3 accounts with IF NOT EXISTS, 2 already exist, but SHOW WARNINGS
 * returns wrong number of columns verify that:
 * - SHOW WARNINGS mechanism will fail validation and produce fatal error with
 * appropriate message
 * - bootstrap fails
 */
TEST_F(ShowWarningsProcessorTest,
       show_warnings_returns_invalid_number_of_columns) {
  const std::vector<std::string> kColumnNames = {"Level", "Code", "Message"};
  size_t i = 0;
  const std::string column_name = kColumnNames[i];
  const std::string column_nr = std::to_string(i + 1);

  // input: other
  const std::set<std::string> account_hosts = {"h1", "h2", "h3"};
  const std::set<std::string> existing_hosts = {"h1", "h2"};
  std::set<std::string> new_hosts = {"h3"};
  std::vector<std::string> extra_args = {"--account", kAccountUser};
  const bool if_not_exists = true;  // default
  const std::string username = kAccountUser;
  const std::string password = kAccountUserPassword;

  // here we tweak SHOW WARNINGS results:
  // - we add one more column
  // We expect that the SHOW WARNINGS processor's validator will notice this
  // and trigger fatal failure
  std::string show_warnings_res;
  {
    // to simplify our life, we force SHOW WARNINGS to return 0 rows here
    // (this way we won't have to add an extra column to them too)
    // Validator MUST trigger failure regardless of resultset content anyway,
    // but we can't just return a resultset with 1 column less than in the
    // header, because that will trigger a failure at the libmysqlclient level,
    // before our validator even gets a chance to run.
    show_warnings_res = res_show_warnings(username, {});

    // add 1 column to the header
    const std::string needle =
        R"({
    "type": "STRING",
    "name": "Message"
  })";
    const std::string noodle =
        R"({
    "type": "STRING",
    "name": "Message"
  },
  {
    "type": "STRING",
    "name": "bogus_column"
  })";
    ASSERT_THAT(show_warnings_res, ::testing::HasSubstr(needle));

    show_warnings_res.replace(show_warnings_res.find(needle), needle.size(),
                              noodle);
  }

  // input: SQL
  const std::string account_auth_list =
      make_account_auth_list(username, account_hosts);
  const std::string al = make_account_list(username, new_hosts);
  std::string custom_responses =
      stmt_resp(sql_create_user(account_auth_list, if_not_exists),
                res_create_user(existing_hosts.size())) +
      "," + stmt_resp(sql_show_warnings(), show_warnings_res);

  // expectations: SQL
  const std::vector<std::string> exp_sql = {
      sql_create_user(account_auth_list, if_not_exists),
      sql_show_warnings(),
  };
  const std::vector<std::string> unexp_sql = {};

  // expectations: other
  int exp_exit_code = EXIT_FAILURE;
  std::vector<std::string> exp_output =
      show_warnings_failed_err_msg(make_account_list(username, account_hosts));
  exp_output.emplace_back(
      "Error: SHOW WARNINGS: Unexpected number of fields in the resultset. "
      "Expected = 3, got = 4");

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // add expected creation SQL statements to JS
  set_mock_server_sql_statements(server_http_port, custom_responses);

  // run bootstrap
  for (const std::string &h : account_hosts) {
    extra_args.push_back("--account-host");
    extra_args.push_back(h);
  }
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(
      exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
  add_login_hook(router, password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);

  // consistency checks
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(extra_args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                username, password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               username);
}

/**
 * @test
 * create 3 accounts with IF NOT EXISTS, 2 already exist, but SHOW WARNINGS
 * fails to execute verify that:
 * - SHOW WARNINGS mechanism will produce fatal error with appropriate message
 * - bootstrap fails
 */
TEST_F(ShowWarningsProcessorTest, show_warnings_fails_to_execute) {
  const unsigned err_code = 1234;
  const std::string err_msg = "je pense, donc je suis";

  // input: other
  const std::set<std::string> account_hosts = {"h1", "h2", "h3"};
  const std::set<std::string> existing_hosts = {"h1", "h2"};
  std::set<std::string> new_hosts = {"h3"};
  std::vector<std::string> extra_args = {"--account", kAccountUser};
  const bool if_not_exists = true;  // default
  const std::string username = kAccountUser;
  const std::string password = kAccountUserPassword;

  // input: SQL
  const std::string account_auth_list =
      make_account_auth_list(username, account_hosts);
  const std::string al = make_account_list(username, new_hosts);
  std::string custom_responses =
      stmt_resp(sql_create_user(account_auth_list, if_not_exists),
                res_create_user(existing_hosts.size())) +
      "," + stmt_resp(sql_show_warnings(), res_error(err_code, err_msg)) + "," +
      stmt_resp(sql_rollback());

  // expectations: SQL
  const std::vector<std::string> exp_sql = {
      sql_create_user(account_auth_list, if_not_exists),
      sql_show_warnings(),
      sql_rollback(),
  };
  const std::vector<std::string> unexp_sql = {};

  // expectations: other
  int exp_exit_code = EXIT_FAILURE;
  std::vector<std::string> exp_output =
      show_warnings_failed_err_msg(make_account_list(username, account_hosts));
  exp_output.emplace_back(
      "Error: Error creating MySQL account for router (SHOW WARNINGS stage): "
      "Error executing MySQL query \"" +
      sql_show_warnings() + "\": " + err_msg + " (" + std::to_string(err_code) +
      ")");

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // add expected creation SQL statements to JS
  set_mock_server_sql_statements(server_http_port, custom_responses);

  // run bootstrap
  for (const std::string &h : account_hosts) {
    extra_args.push_back("--account-host");
    extra_args.push_back(h);
  }
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(
      exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
  add_login_hook(router, password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);

  // consistency checks
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(extra_args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                username, password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               username);
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// UNDO CREATE USER TESTS                                                     //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

struct UndoCreateUserTestParams {
  unsigned failing_grant;
  std::set<std::string> account_hosts;
  std::set<std::string> existing_hosts;
};

class UndoCreateUserTestP
    : public AccountReuseTestBase,
      public ::testing::WithParamInterface<UndoCreateUserTestParams> {};

INSTANTIATE_TEST_CASE_P(
    foo, UndoCreateUserTestP,
    ::testing::Values(

        // we don't test cases of account_hosts == existing_hosts, because no
        // GRANTs are executed in such case
        UndoCreateUserTestParams{1, {"h1", "h2", "h3"}, {"h1", "h2"}},
        UndoCreateUserTestParams{1, {"h1", "h2", "h3"}, {"h2", "h3"}},
        UndoCreateUserTestParams{1, {"h1", "h2", "h3"}, {"h1", "h3"}},
        UndoCreateUserTestParams{1, {"h1", "h2", "h3"}, {"h1"}},
        UndoCreateUserTestParams{1, {"h1", "h2", "h3"}, {"h2"}},
        UndoCreateUserTestParams{1, {"h1", "h2", "h3"}, {"h3"}},
        UndoCreateUserTestParams{1, {"h1", "h2", "h3"}, {}},
        UndoCreateUserTestParams{1, {"h1", "h2"}, {"h1"}},
        UndoCreateUserTestParams{1, {"h1", "h2"}, {"h2"}},
        UndoCreateUserTestParams{1, {"h1", "h2"}, {}},
        UndoCreateUserTestParams{1, {"h1"}, {}},

        // In bootstrap code, GRANT #1, #2 and #3 are just iterations of the
        // same loop, therefore testing all above combinations for GRANTs #2 and
        // #3 shouldn't be neccessary as the code path is the same.  Therefore
        // to save on test time, we only test a subset of combinations:
        UndoCreateUserTestParams{2, {"h1", "h2", "h3"}, {"h1", "h3"}},
        UndoCreateUserTestParams{3, {"h1", "h2", "h3"}, {"h2"}},
        UndoCreateUserTestParams{2, {"h1", "h2", "h3"}, {}},
        UndoCreateUserTestParams{3, {"h1", "h2"}, {"h1"}},
        UndoCreateUserTestParams{2, {"h1", "h2"}, {}},
        UndoCreateUserTestParams{3, {"h1"}, {}}

        ),
    // using 'auto' as type of p triggers
    //
    //  >> Assertion:   (../lnk/block.cc, line 1212)
    //      while processing ...router/tests/component/test_bootstrap.cc
    //      at line 4148.
    //
    // when using devstudio 12.6.
    //
    // The reported line points is about 60 lines away from the problematic
    // parameter below.
    //
    // In other param-info places, auto works find with dev-studio 12.6 too
    [](const ::testing::TestParamInfo<UndoCreateUserTestParams> &p)
        -> std::string {
      std::string test_name =
          "failing_grant_nr_" + std::to_string(p.param.failing_grant);

      test_name += "________account_hosts_";
      for (const std::string &h : p.param.account_hosts) test_name += h + "_";

      test_name += "________existing_hosts_";
      for (const std::string &h : p.param.existing_hosts) test_name += h + "_";

      return test_name;
    });

/**
 * @test
 * create accounts with IF NOT EXISTS, GRANT fails
 * verify that:
 * - GRANT triggers fatal failure with appropriate message
 * - non-existing accounts are DROPped before exiting
 *
 * WL13177:TS_FR17_03
 */
TEST_P(UndoCreateUserTestP, grant_fails) {
  // input: other
  unsigned failing_grant = GetParam().failing_grant;
  const std::set<std::string> &account_hosts = GetParam().account_hosts;
  const std::set<std::string> &existing_hosts = GetParam().existing_hosts;
  std::set<std::string> new_hosts;
  std::set_difference(account_hosts.begin(), account_hosts.end(),
                      existing_hosts.begin(), existing_hosts.end(),
                      std::inserter(new_hosts, new_hosts.begin()));

  const unsigned gr_err_code = 1234;
  const std::string gr_err_msg = "je pense, donc je suis";

  std::vector<std::string> extra_args = {"--account", kAccountUser};
  const bool if_not_exists = true;  // default
  const std::string username = kAccountUser;
  const std::string password = kAccountUserPassword;

  // input: SQL
  const std::string account_auth_list =
      make_account_auth_list(username, account_hosts);
  const std::string al = make_account_list(username, new_hosts);
  std::string custom_responses;
  {
    // CREATE USER steps
    custom_responses +=
        stmt_resp(sql_create_user(account_auth_list, if_not_exists),
                  res_create_user(existing_hosts.size()));
    if (existing_hosts.size())
      custom_responses +=
          "," + stmt_resp(sql_show_warnings(),
                          res_show_warnings(username, existing_hosts));

    // GRANTs
    switch (failing_grant) {
      case 1:
        custom_responses += "," + stmt_resp(sql_grant_1(al),
                                            res_error(gr_err_code, gr_err_msg));
        break;
      case 2:
        custom_responses +=
            "," + stmt_resp(sql_grant_1(al)) + "," +
            stmt_resp(sql_grant_2(al), res_error(gr_err_code, gr_err_msg));
        break;
      case 3:
        custom_responses +=
            "," + stmt_resp(sql_grant_1(al)) + "," +
            stmt_resp(sql_grant_2(al)) + "," +
            stmt_resp(sql_grant_3(al), res_error(gr_err_code, gr_err_msg));
        break;
      default:
        harness_assert_this_should_not_execute();
        break;
    }

    // ROLLBACK
    custom_responses += "," + stmt_resp(sql_rollback());

    // DROP USER (cleanup)
    custom_responses += "," + stmt_resp(sql_drop_user_if_exists(al));
  }

  // expectations: SQL
  std::vector<std::string> exp_sql = {
      sql_create_user(account_auth_list, if_not_exists),
  };
  if (existing_hosts.size()) exp_sql.push_back(sql_show_warnings());
  if (failing_grant >= 1) exp_sql.push_back(sql_grant_1(al));
  if (failing_grant >= 2) exp_sql.push_back(sql_grant_2(al));
  if (failing_grant >= 3) exp_sql.push_back(sql_grant_3(al));
  exp_sql.push_back(sql_rollback());
  exp_sql.push_back(sql_drop_user_if_exists(al));
  const std::vector<std::string> unexp_sql = {};

  // expectations: other
  auto gr_err_sql = [&]() {
    switch (failing_grant) {
      case 1:
        return (sql_grant_1(al));
      case 2:
        return (sql_grant_2(al));
      case 3:
        return (sql_grant_3(al));
      default:
        throw std::logic_error("Invalid case (test has a bug)");
    }
  };
  int exp_exit_code = EXIT_FAILURE;
  std::vector<std::string> exp_output =
      undo_create_user_msg(make_account_list(username, account_hosts));
  exp_output.emplace_back(
      "Error: Error creating MySQL account for router (GRANTs stage): Error "
      "executing MySQL query \"" +
      gr_err_sql() + "\": " + gr_err_msg + " (" + std::to_string(gr_err_code) +
      ")");
  exp_output.emplace_back(kUndoCreateUserSuccessMsg);

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // add expected creation SQL statements to JS
  set_mock_server_sql_statements(server_http_port, custom_responses);

  // run bootstrap
  for (const std::string &h : account_hosts) {
    extra_args.push_back("--account-host");
    extra_args.push_back(h);
  }
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(
      exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
  add_login_hook(router, password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);

  // consistency checks
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(extra_args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                username, password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               username);
}

/**
 * @test
 * create accounts with IF NOT EXISTS, GRANT fails, DROP USER also fails
 * verify that:
 * - GRANT triggers fatal failure with appropriate message
 * - user gets a message that says that we tried to clean up, failed, and is
 *   presented a list of accounts to clean up by hand
 *
 * WL13177:TS_FR17_01
 */
TEST_P(UndoCreateUserTestP, grant_fails_and_drop_user_also_fails) {
  // input: other
  unsigned failing_grant = GetParam().failing_grant;
  const std::set<std::string> &account_hosts = GetParam().account_hosts;
  const std::set<std::string> &existing_hosts = GetParam().existing_hosts;
  std::set<std::string> new_hosts;
  std::set_difference(account_hosts.begin(), account_hosts.end(),
                      existing_hosts.begin(), existing_hosts.end(),
                      std::inserter(new_hosts, new_hosts.begin()));

  const unsigned gr_err_code = 1234;
  const std::string gr_err_msg = "je pense, donc je suis";
  const unsigned du_err_code = 2345;
  const std::string du_err_msg = "lorem ipsum dolor sit amet";

  std::vector<std::string> extra_args = {"--account", kAccountUser};
  const bool if_not_exists = true;  // default
  const std::string username = kAccountUser;
  const std::string password = kAccountUserPassword;

  // input: SQL
  const std::string account_auth_list =
      make_account_auth_list(username, account_hosts);
  const std::string al = make_account_list(username, new_hosts);
  std::string custom_responses;
  {
    // CREATE USER steps
    custom_responses +=
        stmt_resp(sql_create_user(account_auth_list, if_not_exists),
                  res_create_user(existing_hosts.size()));
    if (existing_hosts.size())
      custom_responses +=
          "," + stmt_resp(sql_show_warnings(),
                          res_show_warnings(username, existing_hosts));

    // GRANTs
    switch (failing_grant) {
      case 1:
        custom_responses += "," + stmt_resp(sql_grant_1(al),
                                            res_error(gr_err_code, gr_err_msg));
        break;
      case 2:
        custom_responses +=
            "," + stmt_resp(sql_grant_1(al)) + "," +
            stmt_resp(sql_grant_2(al), res_error(gr_err_code, gr_err_msg));
        break;
      case 3:
        custom_responses +=
            "," + stmt_resp(sql_grant_1(al)) + "," +
            stmt_resp(sql_grant_2(al)) + "," +
            stmt_resp(sql_grant_3(al), res_error(gr_err_code, gr_err_msg));
        break;
      default:
        harness_assert_this_should_not_execute();
        break;
    }

    // ROLLBACK
    custom_responses += "," + stmt_resp(sql_rollback());

    // DROP USER (cleanup)
    custom_responses += "," + stmt_resp(sql_drop_user_if_exists(al),
                                        res_error(du_err_code, du_err_msg));
  }

  // expectations: SQL
  std::vector<std::string> exp_sql = {
      sql_create_user(account_auth_list, if_not_exists),
  };
  if (existing_hosts.size()) exp_sql.push_back(sql_show_warnings());
  if (failing_grant >= 1) exp_sql.push_back(sql_grant_1(al));
  if (failing_grant >= 2) exp_sql.push_back(sql_grant_2(al));
  if (failing_grant >= 3) exp_sql.push_back(sql_grant_3(al));
  exp_sql.push_back(sql_rollback());
  exp_sql.push_back(sql_drop_user_if_exists(al));
  const std::vector<std::string> unexp_sql = {};

  // expectations: other
  auto gr_err_sql = [&]() {
    switch (failing_grant) {
      case 1:
        return (sql_grant_1(al));
      case 2:
        return (sql_grant_2(al));
      case 3:
        return (sql_grant_3(al));
      default:
        throw std::logic_error("Invalid case (test has a bug)");
    }
  };
  int exp_exit_code = EXIT_FAILURE;
  std::vector<std::string> exp_output = undo_create_user_msg(
      make_account_list(username, new_hosts), du_err_code, du_err_msg);
  exp_output.emplace_back(
      "Undoing creating new users failed: Error executing MySQL query \"" +
      sql_drop_user_if_exists(al) + "\": " + du_err_msg + " (" +
      std::to_string(du_err_code) + ")");
  exp_output.emplace_back(
      "Error: Error creating MySQL account for router (GRANTs stage): Error "
      "executing MySQL query \"" +
      gr_err_sql() + "\": " + gr_err_msg + " (" + std::to_string(gr_err_code) +
      ")");

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // add expected creation SQL statements to JS
  set_mock_server_sql_statements(server_http_port, custom_responses);

  // run bootstrap
  for (const std::string &h : account_hosts) {
    extra_args.push_back("--account-host");
    extra_args.push_back(h);
  }
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(
      exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
  add_login_hook(router, password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);

  // consistency checks
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(extra_args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                username, password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               username);
}

#ifndef _WIN32
class UndoCreateUserTest : public AccountReuseTestBase {};

/**
 * @test
 * bootstrap with 3 --account-host, 2 already exist, then trigger failure after
 * account creation stage (in this case, that's the config-writing stage).
 * purpose: verify that "undo CREATE USER" logic will also get triggered by
 * failures that occurr after account creation stage verify that:
 * - the failure we're trying to induce really happens
 * - the "undo CREATE USER" logic will kick in and remove the newly-created
 * account
 *
 * WL13177:TS_FR17_04
 */
TEST_F(UndoCreateUserTest, failure_after_account_creation) {
  // input: other
  const std::set<std::string> account_hosts = {"h1", "h2", "h3"};
  const std::set<std::string> existing_hosts = {"h1", "h3"};
  std::set<std::string> new_hosts = {"h2"};

  std::vector<std::string> extra_args = {"--account", kAccountUser};
  const bool if_not_exists = true;  // default
  const std::string username = kAccountUser;
  const std::string password = kAccountUserPassword;

  // input: SQL
  const std::string account_auth_list =
      make_account_auth_list(username, account_hosts);
  const std::string al = make_account_list(username, new_hosts);
  std::string custom_responses =
      stmt_resp(sql_create_user(account_auth_list, if_not_exists),
                res_create_user(existing_hosts.size())) +
      "," +
      stmt_resp(sql_show_warnings(),
                res_show_warnings(username, existing_hosts)) +
      "," + stmt_resp(sql_grant_1(al)) + "," + stmt_resp(sql_grant_2(al)) +
      "," + stmt_resp(sql_grant_3(al)) + "," + stmt_resp(sql_grant_4(al)) +
      "," + stmt_resp(sql_grant_5(al)) + "," + stmt_resp(sql_grant_6(al)) +
      "," + stmt_resp(sql_drop_user_if_exists(al));

  // expectations: SQL
  const std::vector<std::string> exp_sql = {
      sql_create_user(account_auth_list, if_not_exists),
      sql_show_warnings(),
      sql_grant_1(al),
      sql_grant_2(al),
      sql_grant_3(al),
      sql_grant_4(al),
      sql_grant_5(al),
      sql_grant_6(al),
      sql_drop_user_if_exists(al),
  };
  const std::vector<std::string> unexp_sql = {};

  TempDirectory bootstrap_directory;

  // expectations: other
  int exp_exit_code = EXIT_FAILURE;
  const std::vector<std::string> exp_output = {
      "Error: Could not create file '" +
          Path(bootstrap_directory.name())
              .real_path()
              .join("mysqlrouter.conf.bak")
              .str() +
          "': Permission denied",
      kUndoCreateUserSuccessMsg};

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // add expected creation SQL statements to JS
  set_mock_server_sql_statements(server_http_port, custom_responses);

  // induce failure at config-write step (should result in error analogous to:
  // "Could not create file '.../router-sBHJGw/mysqlrouter.conf.bak': Permission
  // denied"
  for (const std::string &file : {"mysqlrouter.conf", "mysqlrouter.conf.bak"}) {
    std::string path = bootstrap_directory.name() + "/" + file;
    std::ofstream f(path.c_str());
    f << "[DEFAULT]\n";
    chmod(path.c_str(), 0400);
  }

  // run bootstrap
  for (const std::string &h : account_hosts) {
    extra_args.push_back("--account-host");
    extra_args.push_back(h);
  }
  ProcessWrapper &router = launch_bootstrap(
      exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
  add_login_hook(router, password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);

  // consistency checks
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(extra_args));
}

/**
 * @test
 * bootstrap with 3 --account-host, 2 already exist, then trigger failure after
 * account creation stage (in this case, that's the config-writing stage). when
 * the "undo CREATE USER" logic kicks in, DROP USER also fails purpose: verify
 * that "undo CREATE USER" logic will also get triggered by failures that occurr
 * after account creation stage verify that:
 * - the failure we're trying to induce really happens
 * - the "undo CREATE USER" logic will kick in and report the accounts to erase
 * manually after failing
 *
 * WL13177:TS_FR17_02
 */
TEST_F(UndoCreateUserTest,
       failure_after_account_creation_and_drop_user_also_fails) {
  // input: other
  const std::set<std::string> account_hosts = {"h1", "h2", "h3"};
  const std::set<std::string> existing_hosts = {"h1", "h3"};
  std::set<std::string> new_hosts = {"h2"};

  const unsigned du_err_code = 2345;
  const std::string du_err_msg = "lorem ipsum dolor sit amet";

  std::vector<std::string> extra_args = {"--account", kAccountUser};
  const bool if_not_exists = true;  // default
  const std::string username = kAccountUser;
  const std::string password = kAccountUserPassword;

  // input: SQL
  const std::string account_auth_list =
      make_account_auth_list(username, account_hosts);
  const std::string al = make_account_list(username, new_hosts);
  std::string custom_responses =
      stmt_resp(sql_create_user(account_auth_list, if_not_exists),
                res_create_user(existing_hosts.size())) +
      "," +
      stmt_resp(sql_show_warnings(),
                res_show_warnings(username, existing_hosts)) +
      "," + stmt_resp(sql_grant_1(al)) + "," + stmt_resp(sql_grant_2(al)) +
      "," + stmt_resp(sql_grant_3(al)) + "," + stmt_resp(sql_grant_4(al)) +
      "," + stmt_resp(sql_grant_5(al)) + "," + stmt_resp(sql_grant_6(al)) +
      "," +
      stmt_resp(sql_drop_user_if_exists(al),
                res_error(du_err_code, du_err_msg));

  // expectations: SQL
  const std::vector<std::string> exp_sql = {
      sql_create_user(account_auth_list, if_not_exists),
      sql_show_warnings(),
      sql_grant_1(al),
      sql_grant_2(al),
      sql_grant_3(al),
      sql_drop_user_if_exists(al),
  };
  const std::vector<std::string> unexp_sql = {};

  TempDirectory bootstrap_directory;

  // expectations: other
  int exp_exit_code = EXIT_FAILURE;
  std::vector<std::string> exp_output = undo_create_user_msg(
      make_account_list(username, new_hosts), du_err_code, du_err_msg);
  exp_output.emplace_back(
      "Undoing creating new users failed: Error executing MySQL query \"" +
      sql_drop_user_if_exists(al) + "\": " + du_err_msg + " (" +
      std::to_string(du_err_code) + ")");
  exp_output.emplace_back("Error: Could not create file '" +
                          Path(bootstrap_directory.name())
                              .real_path()
                              .join("mysqlrouter.conf.bak")
                              .str() +
                          "': Permission denied");

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);

  // add expected creation SQL statements to JS
  set_mock_server_sql_statements(server_http_port, custom_responses);

  // induce failure at config-write step (should result in error analogous to:
  // "Could not create file '.../router-sBHJGw/mysqlrouter.conf.bak': Permission
  // denied"
  for (const std::string &file : {"mysqlrouter.conf", "mysqlrouter.conf.bak"}) {
    std::string path = bootstrap_directory.name() + "/" + file;
    std::ofstream f(path.c_str());
    f << "[DEFAULT]\n";
    chmod(path.c_str(), 0400);
  }

  // run bootstrap
  for (const std::string &h : account_hosts) {
    extra_args.push_back("--account-host");
    extra_args.push_back(h);
  }
  ProcessWrapper &router = launch_bootstrap(
      exp_exit_code, server_port, bootstrap_directory.name(), extra_args);
  add_login_hook(router, password);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);

  // consistency checks
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(extra_args));
}
#endif

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// ACCOUNT VALIDATION TESTS                                                   //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
class AccountValidationTest : public AccountReuseTestBase {};

/**
 * @test
 * Bootstrap: simple sunny day scenario.  Verify that:
 * - account validation is performed (validation message is printed)
 * - account validation does not fail
 *
 * WL13177:TS_FR13_xx (doesn't exist in Test Plan yet)
 * *** this is like FR13_01, but there we have an invalid password, here it is
 * valid ***
 */
TEST_F(AccountValidationTest, sunny_day_scenario) {
  // test params
  const std::vector<std::string> args = {"--account", kAccountUser};
  const std::set<std::string> existing_hosts =
      {};  // kAccountUser@% doesn't exist yet

  // key expectations
  int exp_exit_code = EXIT_SUCCESS;
  std::vector<std::string> exp_output = {kBootstrapSuccessMsg, acct_val_msg()};
  std::vector<std::string> unexp_output = acct_val_failed_warning_msg();
  // other expectations
  const std::string exp_username = kAccountUser;
  const std::string exp_password = kAccountUserPassword;
  const std::set<std::string> exp_attempt_create_hosts = {"%"};
  CustomResponses cr = gen_sql_for_creating_accounts(
      exp_username, exp_attempt_create_hosts, existing_hosts);
  std::vector<std::string> exp_sql = cr.exp_sql;
  std::vector<std::string> unexp_sql = {"DROP USER"};

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);
  set_mock_server_sql_statements(server_http_port, cr.stmts, kAccountUser);

  // run bootstrap
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(exp_exit_code, server_port,
                                            bootstrap_directory.name(), args);
  add_login_hook(router, exp_password, exp_username);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output, unexp_output);
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                exp_username, exp_password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               exp_username);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);
}

/**
 * @test
 * Bootstrap: no --strict, bootstrap against existing account but enter wrong
 * password.  Verify that:
 * - account validation fails (appopriate failure message is printed)
 * - failed validation does not cause a fatal error
 * - bootstrap succeeds
 * - CREATE USER is NOT reverted (account existed before bootstrapping)
 *
 * WL13177:TS_FR13_01
 */
TEST_F(AccountValidationTest, account_exists_wrong_password) {
  // test params
  const std::vector<std::string> args = {"--account", kAccountUser};
  const std::set<std::string> existing_hosts = {
      "%"};  // kAccountUser@% exists already

  // key expectations
  int exp_exit_code = EXIT_SUCCESS;
  std::vector<std::string> exp_output = acct_val_failed_warning_msg();
  exp_output.push_back(kBootstrapSuccessMsg);
  std::vector<std::string> unexp_output = {};
  // other expectations
  const std::string exp_username = kAccountUser;
  const std::string exp_password = kAccountUserPassword;
  const std::set<std::string> exp_attempt_create_hosts = {"%"};
  CustomResponses cr = gen_sql_for_creating_accounts(
      exp_username, exp_attempt_create_hosts, existing_hosts);
  std::vector<std::string> exp_sql = cr.exp_sql;
  std::vector<std::string> unexp_sql = {
      "DROP USER",  // no CREATE USER revert
      sql_val1(), sql_val2(),
      sql_val3()  // shouldn't get that far due to conn failure
  };

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);
  set_mock_server_sql_statements(
      server_http_port,
      cr.stmts);  // we omit setting kAccountUser for 2nd conn
  // ^^^ WL13177:TS_FR13_01 originally specifies that the username is correct
  // and the password is incorrect, but by providing an incorrect username
  // instead we achieve the exact same effect at the server mock level, only
  // with simpler code

  // run bootstrap
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(exp_exit_code, server_port,
                                            bootstrap_directory.name(), args);
  add_login_hook(router, exp_password, exp_username);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output, unexp_output);
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                exp_username, exp_password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               exp_username);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);
}

/**
 * @test
 * Bootstrap: with --strict, bootstrap against existing account but enter wrong
 * password.  Verify that:
 * - account validation fails (appopriate failure message is printed)
 * - failed validation is a fatal error
 * - bootstrap fails
 * - CREATE USER is NOT reverted (account existed before bootstrapping)
 *
 * WL13177:TS_FR15_03
 */
TEST_F(AccountValidationTest, account_exists_wrong_password_strict) {
  // test params
  const std::vector<std::string> args = {"--strict", "--account", kAccountUser};
  const std::set<std::string> existing_hosts = {
      "%"};  // kAccountUser@% exists already

  // key expectations
  int exp_exit_code = EXIT_FAILURE;
  std::vector<std::string> exp_output = acct_val_failed_error_msg();
  std::vector<std::string> unexp_output = {kBootstrapSuccessMsg};
  // other expectations
  const std::string exp_username = kAccountUser;
  const std::string exp_password = kAccountUserPassword;
  const std::set<std::string> exp_attempt_create_hosts = {"%"};
  CustomResponses cr = gen_sql_for_creating_accounts(
      exp_username, exp_attempt_create_hosts, existing_hosts);
  std::vector<std::string> exp_sql = cr.exp_sql;
  std::vector<std::string> unexp_sql = {
      "DROP USER",  // no CREATE USER revert
      sql_val1(), sql_val2(),
      sql_val3()  // shouldn't get that far due to conn failure
  };

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);
  set_mock_server_sql_statements(
      server_http_port,
      cr.stmts);  // we omit setting kAccountUser for 2nd conn
  // ^^^ WL13177:TS_FR15_03 originally specifies that the username is correct
  // and the password is incorrect, but by providing an incorrect username
  // instead we achieve the exact same effect at the server mock level, only
  // with simpler code

  // run bootstrap
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(exp_exit_code, server_port,
                                            bootstrap_directory.name(), args);
  add_login_hook(router, exp_password, exp_username);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output, unexp_output);
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                exp_username, exp_password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               exp_username);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);
}

/**
 * @test
 * Bootstrap: no --strict, account validation fails on connection attempt.
 * Verify that:
 * - account validation fails (appopriate failure message is printed)
 * - failed validation does not cause a fatal error
 * - bootstrap succeeds
 * - CREATE USER is NOT reverted
 *
 * WL13177:TS_FR14_02
 */
TEST_F(AccountValidationTest, warn_on_conn_failure) {
  // test params
  const std::vector<std::string> args = {"--account", kAccountUser,
                                         "--account-host", "not.local.host"};
  const std::set<std::string> existing_hosts =
      {};  // kAccountUser@% doesn't exist yet

  // key expectations
  int exp_exit_code = EXIT_SUCCESS;
  std::vector<std::string> exp_output = acct_val_failed_warning_msg();
  exp_output.push_back(kBootstrapSuccessMsg);
  std::vector<std::string> unexp_output = {};
  // other expectations
  const std::string exp_username = kAccountUser;
  const std::string exp_password = kAccountUserPassword;
  const std::set<std::string> exp_attempt_create_hosts = {"not.local.host"};
  CustomResponses cr = gen_sql_for_creating_accounts(
      exp_username, exp_attempt_create_hosts, existing_hosts);
  std::vector<std::string> exp_sql = cr.exp_sql;
  std::vector<std::string> unexp_sql = {
      "DROP USER",  // no CREATE USER revert
      sql_val1(), sql_val2(),
      sql_val3()  // shouldn't get that far due to conn failure
  };

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);
  set_mock_server_sql_statements(
      server_http_port,
      cr.stmts);  // we omit setting kAccountUser for 2nd conn

  // run bootstrap
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(exp_exit_code, server_port,
                                            bootstrap_directory.name(), args);
  add_login_hook(router, exp_password, exp_username);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output, unexp_output);
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                exp_username, exp_password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               exp_username);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);
}

/**
 * @test
 * Bootstrap: with --strict, account validation fails on connection attempt.
 * Verify that:
 * - account validation fails (appopriate failure message is printed)
 * - failed validation is a fatal error
 * - bootstrap fails
 * - CREATE USER is reverted via DROP USER
 *
 * WL13177:TS_FR15_02
 */
TEST_F(AccountValidationTest, error_on_conn_failure) {
  // test params
  const std::vector<std::string> args = {"--strict", "--account", kAccountUser,
                                         "--account-host", "not.local.host"};
  const std::set<std::string> existing_hosts =
      {};  // kAccountUser@% doesn't exist yet

  // key expectations
  int exp_exit_code = EXIT_FAILURE;
  std::vector<std::string> exp_output = acct_val_failed_error_msg();
  std::vector<std::string> unexp_output = {kBootstrapSuccessMsg};
  // other expectations
  const std::string exp_username = kAccountUser;
  const std::string exp_password = kAccountUserPassword;
  const std::set<std::string> exp_attempt_create_hosts = {"not.local.host"};
  CustomResponses cr = gen_sql_for_creating_accounts(
      exp_username, exp_attempt_create_hosts, existing_hosts);
  std::vector<std::string> exp_sql = cr.exp_sql;
  exp_sql.push_back("DROP USER");  // revert CREATE USER
  std::vector<std::string> unexp_sql = {
      sql_val1(), sql_val2(),
      sql_val3()  // shouldn't get that far due to conn failure
  };

  // launch mock server and wait for it to start accepting connections
  const uint16_t server_port = port_pool_.get_next_available();
  const uint16_t server_http_port = port_pool_.get_next_available();
  ProcessWrapper &server_mock =
      launch_mock_server(server_port, server_http_port);
  set_mock_server_sql_statements(
      server_http_port,
      cr.stmts);  // we omit setting kAccountUser for 2nd conn

  // run bootstrap
  TempDirectory bootstrap_directory;
  ProcessWrapper &router = launch_bootstrap(exp_exit_code, server_port,
                                            bootstrap_directory.name(), args);
  add_login_hook(router, exp_password, exp_username);

  // check outcome
  DebugDumper dd(router, server_mock, server_http_port);
  check_bootstrap_success(router, exp_output, unexp_output);
  check_questions_asked_by_bootstrap(exp_exit_code, router,
                                     is_using_account(args));
  check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                exp_username, exp_password);
  check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
               exp_username);
  check_SQL_calls(server_http_port, exp_sql, unexp_sql);
}

/**
 * @test
 * Bootstrap: no --strict, account validation fails on SQL query.  Verify that:
 * - account validation fails (appopriate failure message is printed)
 * - failed validation does not cause a fatal error
 * - bootstrap succeeds
 * - CREATE USER is NOT reverted
 *
 * WL13177:TS_FR14_02
 */
TEST_F(AccountValidationTest, warn_on_query_failure) {
  // inlining initializer_list inside the for loop segfauls on Solaris
  std::initializer_list<std::string> sql_val_stmts = {
      // skip sql_val4() because testing with it is more complicated due to
      // query
      // re-use, will behave the same anyway (same code flow)
      sql_val1(), sql_val2(), sql_val3()};
  for (const std::string &failed_val_query : sql_val_stmts) {
    // test params
    const std::vector<std::string> args = {"--account", kAccountUser,
                                           "--account-host", "not.local.host"};
    const std::set<std::string> existing_hosts =
        {};  // kAccountUser@% doesn't exist yet

    // key expectations
    int exp_exit_code = EXIT_SUCCESS;
    std::vector<std::string> exp_output = acct_val_failed_warning_msg();
    exp_output.push_back(kBootstrapSuccessMsg);
    std::vector<std::string> unexp_output = {};
    // other expectations
    const std::string exp_username = kAccountUser;
    const std::string exp_password = kAccountUserPassword;
    const std::set<std::string> exp_attempt_create_hosts = {"not.local.host"};
    CustomResponses cr = gen_sql_for_creating_accounts(
        exp_username, exp_attempt_create_hosts, existing_hosts);
    cr.add(failed_val_query, res_error());

    std::vector<std::string> exp_sql = cr.exp_sql;
    std::vector<std::string> unexp_sql = {"DROP USER"};

    // launch mock server and wait for it to start accepting connections
    const uint16_t server_port = port_pool_.get_next_available();
    const uint16_t server_http_port = port_pool_.get_next_available();
    ProcessWrapper &server_mock =
        launch_mock_server(server_port, server_http_port);
    set_mock_server_sql_statements(server_http_port, cr.stmts, kAccountUser);

    // run bootstrap
    TempDirectory bootstrap_directory;
    ProcessWrapper &router = launch_bootstrap(exp_exit_code, server_port,
                                              bootstrap_directory.name(), args);
    add_login_hook(router, exp_password, exp_username);

    // check outcome
    DebugDumper dd(router, server_mock, server_http_port);
    check_bootstrap_success(router, exp_output, unexp_output);
    check_questions_asked_by_bootstrap(exp_exit_code, router,
                                       is_using_account(args));
    check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                  exp_username, exp_password);
    check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                 exp_username);
    check_SQL_calls(server_http_port, exp_sql, unexp_sql);
  }
}

/**
 * @test
 * Bootstrap: with --strict, account validation fails on SQL query.  Verify
 * that:
 * - account validation fails (appopriate failure message is printed)
 * - failed validation is a fatal error
 * - bootstrap fails
 * - CREATE USER is reverted via DROP USER
 *
 * WL13177:TS_FR15_02
 */
TEST_F(AccountValidationTest, error_on_query_failure) {
  // inlining initializer_list inside the for loop segfauls on Solaris
  std::initializer_list<std::string> sql_val_stmts = {
      // skip sql_val4() because testing with it is more complicated due to
      // query
      // re-use, will behave the same anyway (same code flow)
      sql_val1(), sql_val2(), sql_val3()};
  for (const std::string &failed_val_query : sql_val_stmts) {
    // test params
    const std::vector<std::string> args = {"--strict", "--account",
                                           kAccountUser, "--account-host",
                                           "not.local.host"};
    const std::set<std::string> existing_hosts =
        {};  // kAccountUser@% doesn't exist yet

    // key expectations
    int exp_exit_code = EXIT_FAILURE;
    std::vector<std::string> exp_output = acct_val_failed_error_msg();
    std::vector<std::string> unexp_output = {kBootstrapSuccessMsg};
    // other expectations
    const std::string exp_username = kAccountUser;
    const std::string exp_password = kAccountUserPassword;
    const std::set<std::string> exp_attempt_create_hosts = {"not.local.host"};
    CustomResponses cr = gen_sql_for_creating_accounts(
        exp_username, exp_attempt_create_hosts, existing_hosts);
    cr.add(failed_val_query, res_error());

    std::vector<std::string> exp_sql = cr.exp_sql;
    exp_sql.push_back("DROP USER");
    std::vector<std::string> unexp_sql = {};

    // launch mock server and wait for it to start accepting connections
    const uint16_t server_port = port_pool_.get_next_available();
    const uint16_t server_http_port = port_pool_.get_next_available();
    ProcessWrapper &server_mock =
        launch_mock_server(server_port, server_http_port);
    set_mock_server_sql_statements(server_http_port, cr.stmts, kAccountUser);

    // run bootstrap
    TempDirectory bootstrap_directory;
    ProcessWrapper &router = launch_bootstrap(exp_exit_code, server_port,
                                              bootstrap_directory.name(), args);
    add_login_hook(router, exp_password, exp_username);

    // check outcome
    DebugDumper dd(router, server_mock, server_http_port);
    check_bootstrap_success(router, exp_output, unexp_output);
    check_questions_asked_by_bootstrap(exp_exit_code, router,
                                       is_using_account(args));
    check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                  exp_username, exp_password);
    check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                 exp_username);
    check_SQL_calls(server_http_port, exp_sql, unexp_sql);
  }
}

/**
 * @test
 * Bootstrap: no --strict, user exists without proper GRANTs (account validation
 * fails on SQL query).  Verify that:
 * - account validation fails (appopriate failure message is printed)
 * - failed validation does not cause a fatal error
 * - bootstrap succeeds
 * - CREATE USER is NOT reverted
 *
 * WL13177:TS_FR14_01
 *
 * Additinoal expectations for WL13177::NFR2:
 * - GRANTs will not be added
 */
TEST_F(AccountValidationTest, existing_user_missing_grants___no_strict) {
  // inlining initializer_list inside the for loop segfauls on Solaris
  std::initializer_list<std::string> sql_val_stmts = {
      // skip sql_val4() because testing with it is more complicated due to
      // query
      // re-use, will behave the same anyway (same code flow)
      sql_val1(), sql_val2(), sql_val3()};
  for (const std::string &failed_val_query : sql_val_stmts) {
    // test params
    const std::vector<std::string> args = {"--account", kAccountUser};
    const std::set<std::string> existing_hosts = {
        "%"};  // kAccountUser@% exists already

    // key expectations
    int exp_exit_code = EXIT_SUCCESS;
    std::vector<std::string> exp_output = acct_val_failed_warning_msg();
    exp_output.push_back(kBootstrapSuccessMsg);
    std::vector<std::string> unexp_output = {};
    // other expectations
    const std::string exp_username = kAccountUser;
    const std::string exp_password = kAccountUserPassword;
    const std::set<std::string> exp_attempt_create_hosts = {"%"};
    CustomResponses cr = gen_sql_for_creating_accounts(
        exp_username, exp_attempt_create_hosts, existing_hosts);
    cr.add(failed_val_query,
           res_error(ER_TABLEACCESS_DENIED_ERROR));  // 1142, lack of GRANT

    std::vector<std::string> exp_sql = cr.exp_sql;
    std::vector<std::string> unexp_sql = {"GRANT", "DROP USER"};

    // launch mock server and wait for it to start accepting connections
    const uint16_t server_port = port_pool_.get_next_available();
    const uint16_t server_http_port = port_pool_.get_next_available();
    ProcessWrapper &server_mock =
        launch_mock_server(server_port, server_http_port);
    set_mock_server_sql_statements(server_http_port, cr.stmts, kAccountUser);

    // run bootstrap
    TempDirectory bootstrap_directory;
    ProcessWrapper &router = launch_bootstrap(exp_exit_code, server_port,
                                              bootstrap_directory.name(), args);
    add_login_hook(router, exp_password, exp_username);

    // check outcome
    DebugDumper dd(router, server_mock, server_http_port);
    check_bootstrap_success(router, exp_output, unexp_output);
    check_questions_asked_by_bootstrap(exp_exit_code, router,
                                       is_using_account(args));
    check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                  exp_username, exp_password);
    check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                 exp_username);
    check_SQL_calls(server_http_port, exp_sql, unexp_sql);
  }
}

/**
 * @test
 * Bootstrap: with --strict, user exists without proper GRANTs (account
 * validation fails on SQL query).  Verify that:
 * - account validation fails (appopriate failure message is printed)
 * - failed validation is a fatal error
 * - bootstrap fails
 * - CREATE USER is NOT reverted via DROP USER (it can't be, because it didn't
 * exist before)
 *
 * WL13177:TS_FR15_01
 *
 * Additinoal expectations for WL13177::NFR2:
 * - GRANTs will not be added
 */
TEST_F(AccountValidationTest, existing_user_missing_grants___strict) {
  // inlining initializer_list inside the for loop segfauls on Solaris
  std::initializer_list<std::string> sql_val_stmts = {
      // skip sql_val4() because testing with it is more complicated due to
      // query
      // re-use, will behave the same anyway (same code flow)
      sql_val1(), sql_val2(), sql_val3()};
  for (const std::string &failed_val_query : sql_val_stmts) {
    // test params
    const std::vector<std::string> args = {"--strict", "--account",
                                           kAccountUser};
    const std::set<std::string> existing_hosts = {
        "%"};  // kAccountUser@% exists already

    // key expectations
    int exp_exit_code = EXIT_FAILURE;
    std::vector<std::string> exp_output = acct_val_failed_error_msg();
    std::vector<std::string> unexp_output = {kBootstrapSuccessMsg};
    // other expectations
    const std::string exp_username = kAccountUser;
    const std::string exp_password = kAccountUserPassword;
    const std::set<std::string> exp_attempt_create_hosts = {"%"};
    CustomResponses cr = gen_sql_for_creating_accounts(
        exp_username, exp_attempt_create_hosts, existing_hosts);
    cr.add(failed_val_query,
           res_error(ER_TABLEACCESS_DENIED_ERROR));  // 1142, lack of GRANT

    std::vector<std::string> exp_sql = cr.exp_sql;
    std::vector<std::string> unexp_sql = {"GRANT", "DROP USER"};

    // launch mock server and wait for it to start accepting connections
    const uint16_t server_port = port_pool_.get_next_available();
    const uint16_t server_http_port = port_pool_.get_next_available();
    ProcessWrapper &server_mock =
        launch_mock_server(server_port, server_http_port);
    set_mock_server_sql_statements(server_http_port, cr.stmts, kAccountUser);

    // run bootstrap
    TempDirectory bootstrap_directory;
    ProcessWrapper &router = launch_bootstrap(exp_exit_code, server_port,
                                              bootstrap_directory.name(), args);
    add_login_hook(router, exp_password, exp_username);

    // check outcome
    DebugDumper dd(router, server_mock, server_http_port);
    check_bootstrap_success(router, exp_output, unexp_output);
    check_questions_asked_by_bootstrap(exp_exit_code, router,
                                       is_using_account(args));
    check_keyring(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                  exp_username, exp_password);
    check_config(bootstrap_directory.name(), exp_exit_code == EXIT_SUCCESS,
                 exp_username);
    check_SQL_calls(server_http_port, exp_sql, unexp_sql);
  }
}

class RouterAccountHostTest : public CommonBootstrapTest {};

/**
 * @test
 *        verify that --account-host:
 *        - works in general
 *        - can be applied multiple times in one go
 *        - can take '%' as a parameter
 */
TEST_F(RouterAccountHostTest, multiple_host_patterns) {
  // to avoid duplication of tracefiles, we run the same test twice, with the
  // only difference that 1st time we run --bootstrap before the --account-host,
  // and second time we run it after
  const auto server_port = port_pool_.get_next_available();

  auto test_it = [&](const std::vector<std::string> &cmdline) -> void {
    const std::string json_stmts =
        get_data_dir()
            .join("bootstrap_account_host_multiple_patterns.js")
            .str();

    // launch mock server and wait for it to start accepting connections
    auto &server_mock =
        launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);
    ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

    // launch the router in bootstrap mode
    auto &router = launch_router(cmdline);

    // add login hook
    router.register_response("Please enter MySQL password for root: ",
                             kRootPassword + "\n"s);

    // check if the bootstraping was successful
    EXPECT_TRUE(router.expect_output(
        "MySQL Router configured for the InnoDB Cluster 'test'", false, 5s))
        << "router: " << router.get_full_output() << std::endl
        << "server: " << server_mock.get_full_output();

    check_exit_code(router, EXIT_SUCCESS);

    server_mock.kill();
  };

  // NOTE: CREATE USER statements should run in unique(sort(hostname_list))
  // fashion

  // --bootstrap before --account-host
  {
    TempDirectory bootstrap_directory;
    test_it({"--bootstrap=127.0.0.1:" + std::to_string(server_port),
             "--report-host", my_hostname, "-d", bootstrap_directory.name(),
             "--account-host", "host1",     // 2nd CREATE USER
             "--account-host", "%",         // 1st CREATE USER
             "--account-host", "host1",     // \_ redundant, ignored
             "--account-host", "host1",     // /
             "--account-host", "host3%"});  // 3rd CREATE USER
  }

  // --bootstrap after --account-host
  {
    TempDirectory bootstrap_directory;
    test_it({"-d", bootstrap_directory.name(), "--report-host", my_hostname,
             "--account-host", "host1",   // 2nd CREATE USER
             "--account-host", "%",       // 1st CREATE USER
             "--account-host", "host1",   // \_ redundant, ignored
             "--account-host", "host1",   // /
             "--account-host", "host3%",  // 3rd CREATE USER
             "--bootstrap=127.0.0.1:" + std::to_string(server_port)});
  }
}

/**
 * @test
 *        verify that --account-host without required argument produces an
 error
 *        and exits
 */
TEST_F(RouterAccountHostTest, argument_missing) {
  const uint16_t server_port = port_pool_.get_next_available();

  // launch the router in bootstrap mode
  auto &router =
      launch_router({"--bootstrap=127.0.0.1:" + std::to_string(server_port),
                     "--account-host"},
                    EXIT_FAILURE);

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output(
      "option '--account-host' expects a value, got nothing"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 *        verify that --account-host without --bootstrap switch produces an
 * error and exits
 */
TEST_F(RouterAccountHostTest, without_bootstrap_flag) {
  // launch the router in bootstrap mode
  auto &router = launch_router({"--account-host", "host1"}, EXIT_FAILURE);

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output(
      "Option --account-host can only be used together with -B/--bootstrap"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 *        verify that --account-host with illegal hostname argument correctly
 * handles the error
 */
TEST_F(RouterAccountHostTest, illegal_hostname) {
  const std::string json_stmts =
      get_data_dir().join("bootstrap_account_host_pattern_too_long.js").str();
  TempDirectory bootstrap_directory;
  const auto server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto &server_mock =
      launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  // launch the router in bootstrap mode
  auto &router = launch_router(
      {"--bootstrap=127.0.0.1:" + std::to_string(server_port), "--report-host",
       my_hostname, "-d", bootstrap_directory.name(), "--account-host",
       "veryveryveryveryveryveryveryveryveryveryveryveryveryveryverylonghost"},
      1);
  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           kRootPassword + "\n"s);

  // check if the bootstraping was successful
  EXPECT_TRUE(
      router.expect_output("Error executing MySQL query \".*\": String "
                           "'veryveryveryveryveryveryveryveryveryveryveryveryve"
                           "ryveryverylonghost' is too long for host name",
                           true))
      << router.get_full_output() << std::endl
      << "server:\n"
      << server_mock.get_full_output();
  check_exit_code(router, EXIT_FAILURE);
}

class RouterReportHostTest : public CommonBootstrapTest {};

/**
 * @test
 *        verify that --report-host works for the typical use case
 */
TEST_F(RouterReportHostTest, typical_usage) {
  const auto server_port = port_pool_.get_next_available();

  auto test_it = [&](const std::vector<std::string> &cmdline) -> void {
    const std::string json_stmts =
        get_data_dir().join("bootstrap_report_host.js").str();

    // launch mock server and wait for it to start accepting connections
    auto &server_mock =
        launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);
    ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

    // launch the router in bootstrap mode
    auto &router = launch_router(cmdline);

    // add login hook
    router.register_response("Please enter MySQL password for root: ",
                             kRootPassword + "\n"s);

    // check if the bootstraping was successful
    EXPECT_TRUE(
        router.expect_output("MySQL Router configured for the "
                             "InnoDB Cluster 'mycluster'"))
        << router.get_full_output() << std::endl
        << "server: " << server_mock.get_full_output();
    check_exit_code(router, EXIT_SUCCESS);

    server_mock.kill();
  };

  {
    TempDirectory bootstrap_directory;
    // --bootstrap before --report-host
    test_it({"--bootstrap=127.0.0.1:" + std::to_string(server_port), "-d",
             bootstrap_directory.name(), "--report-host", "host.foo.bar"});
  }

  {
    TempDirectory bootstrap_directory;
    // --bootstrap after --report-host
    test_it({"-d", bootstrap_directory.name(), "--report-host", "host.foo.bar",
             "--bootstrap=127.0.0.1:" + std::to_string(server_port)});
  }
}

/**
 * @test
 *        verify that multiple --report-host arguments produce an error
 *        and exit
 */
TEST_F(RouterReportHostTest, multiple_hostnames) {
  // launch the router in bootstrap mode
  auto &router = launch_router({"--bootstrap=1.2.3.4:5678", "--report-host",
                                "host1", "--report-host", "host2"},
                               1);

  // check if the bootstraping was successful
  EXPECT_TRUE(
      router.expect_output("Option --report-host can only be used once."))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 *        verify that --report-host without required argument produces an
 error
 *        and exits
 */
TEST_F(RouterReportHostTest, argument_missing) {
  // launch the router in bootstrap mode
  auto &router =
      launch_router({"--bootstrap=1.2.3.4:5678", "--report-host"}, 1);

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output(
      "option '--report-host' expects a value, got nothing"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 *        verify that --report-host without --bootstrap switch produces an error
 *        and exits
 */
TEST_F(RouterReportHostTest, without_bootstrap_flag) {
  // launch the router in bootstrap mode
  auto &router = launch_router({"--report-host", "host1"}, 1);

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output(
      "Option --report-host can only be used together with -B/--bootstrap"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 *        verify that --report-host with invalid hostname argument produces an
 *        error and exits
 *
 * @note
 *        There's a separate suite of unit tests which tests the validating code
 *        which determines if the hostname is valid or not - therefore here we
 *        only focus on how this invalid hostname will be handled - we don't
 *        concern outselves with correctness of hostname validation itself.
 */
TEST_F(RouterReportHostTest, invalid_hostname) {
  // launch the router in bootstrap mode
  auto &router = launch_router(
      {"--bootstrap", "1.2.3.4:5678", "--report-host", "^bad^hostname^"}, 1);

  // check if the bootstraping was successful
  EXPECT_TRUE(
      router.expect_output("Error: Option --report-host has an invalid value."))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
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
                                   tmp_dir.name());

  std::vector<std::string> router_options = {
      "--bootstrap=" + config.at(0).ip + ":" +
          std::to_string(config.at(0).port),
      "--report-host",
      my_hostname,
      "-d",
      bootstrap_dir.name(),
      "--master-key-reader=" + script_generator.get_reader_script(),
      "--master-key-writer=" + script_generator.get_writer_script()};

  bootstrap_failover(config, ClusterType::GR_V2, router_options);

  Path tmp(bootstrap_dir.name());
  Path master_key_file(tmp.join("mysqlrouter.key").str());
  ASSERT_FALSE(master_key_file.exists());

  Path keyring_file(tmp.join("data").join("keyring").str());
  ASSERT_TRUE(keyring_file.exists());

  Path dir(tmp_dir.name());
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

  bootstrap_failover(mock_servers, ClusterType::GR_V2, router_options,
                     EXIT_SUCCESS, {});
  {
    std::ifstream file(master_key_path);
    std::stringstream iss;
    iss << file.rdbuf();
    ASSERT_THAT(master_key, testing::Eq(iss.str()));
  }
}

/**
 * @test
 *       verify that using --conf-use-gr-notifications creates proper config
 * file entry.
 */
TEST_F(RouterBootstrapTest, ConfUseGrNotificationsYes) {
  TempDirectory bootstrap_directory;
  const auto server_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  // launch mock server and wait for it to start accepting connections
  auto &server_mock =
      launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  // launch the router in bootstrap mode
  auto &router = launch_router(
      {"--bootstrap=127.0.0.1:" + std::to_string(server_port), "-d",
       bootstrap_directory.name(), "--conf-use-gr-notifications"});

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           kRootPassword + "\n"s);

  check_exit_code(router, EXIT_SUCCESS);

  // check if the valid config option was added to the file
  EXPECT_TRUE(find_in_file(
      bootstrap_directory.name() + "/mysqlrouter.conf",
      [](const std::string &line) -> bool {
        return line == "use_gr_notifications=1";
      },
      0ms));

  // check if valid TTL is set (with GR notifications it should be increased to
  // 60s)
  EXPECT_TRUE(find_in_file(
      bootstrap_directory.name() + "/mysqlrouter.conf",
      [](const std::string &line) -> bool { return line == "ttl=60"; }, 0ms));
}

/**
 * @test
 *       verify that NOT using --conf-use-gr-notifications
 *       creates a proper config file entry.
 */
TEST_F(RouterBootstrapTest, ConfUseGrNotificationsNo) {
  TempDirectory bootstrap_directory;
  const auto server_port = port_pool_.get_next_available();

  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  // launch mock server and wait for it to start accepting connections
  auto &server_mock =
      launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  // launch the router in bootstrap mode
  auto &router =
      launch_router({"--bootstrap=127.0.0.1:" + std::to_string(server_port),
                     "-d", bootstrap_directory.name()});

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           kRootPassword + "\n"s);

  check_exit_code(router, EXIT_SUCCESS);

  // check if valid config option was added to the file
  EXPECT_TRUE(find_in_file(
      bootstrap_directory.name() + "/mysqlrouter.conf",
      [](const std::string &line) -> bool {
        return line == "use_gr_notifications=0";
      },
      0ms));

  // check if valid TTL is set (with no GR notifications it should be 0.5s)
  EXPECT_TRUE(find_in_file(
      bootstrap_directory.name() + "/mysqlrouter.conf",
      [](const std::string &line) -> bool { return line == "ttl=0.5"; }, 0ms));
}

/**
 * @test
 *        verify that --conf-use-gr-notifications used with no bootstrap
 *        causes proper error report
 */
TEST_F(RouterReportHostTest, ConfUseGrNotificationsNoBootstrap) {
  auto &router = launch_router({"--conf-use-gr-notifications"}, 1);

  EXPECT_TRUE(
      router.expect_output("Error: Option --conf-use-gr-notifications can only "
                           "be used together with -B/--bootstrap"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

/**
 * @test
 *        verify that --conf-use-gr-notifications used with some value
 *        causes proper error report
 */
TEST_F(RouterReportHostTest, ConfUseGrNotificationsHasValue) {
  auto &router = launch_router(
      {"-B", "somehost:12345", "--conf-use-gr-notifications=some"}, 1);

  EXPECT_TRUE(
      router.expect_output("Error: option '--conf-use-gr-notifications' does "
                           "not expect a value, but got a value"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

class ErrorReportTest : public CommonBootstrapTest {};

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
  auto &router = launch_router(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--connect-timeout=1",
          "--report-host",
          my_hostname,
          "-d",
          bootstrap_directory.name(),
      },
      EXIT_FAILURE);
  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           kRootPassword + "\n"s);

  // verify that appropriate message was logged (first line) and error message
  // printed (last line)
  std::string err_msg = "Directory '" + bootstrap_directory.name() +
                        "' already contains files\n"
                        "Error: Directory already exits";

  check_exit_code(router, EXIT_FAILURE);
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
  auto &router = launch_router(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--connect-timeout=1",
          "--report-host",
          my_hostname,
          "-d",
          bootstrap_directory.name(),
      },
      EXIT_FAILURE);
  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           kRootPassword + "\n"s);

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
  auto &router = launch_router(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--connect-timeout=1",
          "--report-host",
          my_hostname,
          "-d",
          bootstrap_directory,
      },
      EXIT_FAILURE);

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           kRootPassword + "\n"s);

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

  // launch mock server and wait for it to start accepting connections
  auto &server_mock =
      launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  // launch the router in bootstrap mode
  auto &router = launch_router(
      {"--bootstrap=127.0.0.1:" + std::to_string(server_port), "-d",
       bootstrap_directory.name(), "--conf-use-gr-notifications"},
      EXIT_FAILURE);

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  EXPECT_TRUE(
      router.expect_output("Error: The parameter 'use-gr-notifications' is "
                           "valid only for GR cluster type"))
      << router.get_full_output() << std::endl;
  check_exit_code(router, EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
