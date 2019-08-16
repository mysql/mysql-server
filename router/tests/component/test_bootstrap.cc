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

#include <fstream>
#include <string>

#include "dim.h"
#include "filesystem_utils.h"
#include "gmock/gmock.h"
#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "random_generator.h"
#include "router_component_test.h"
#include "script_generator.h"
#include "socket_operations.h"
#include "tcp_port_pool.h"
#include "utils.h"

#ifndef _WIN32
#include <sys/stat.h>
#endif

/**
 * @file
 * @brief Component Tests for the bootstrap operation
 */

using namespace std::chrono_literals;
using namespace std::string_literals;

static constexpr const char kRootPassword[] = "fake-pass";

// we create a number of classes to logically group tests together. But to avoid
// code duplication, we derive them from a class which contains the common code
// they need.
class CommonBootstrapTest : public RouterComponentTest {
 protected:
  static void SetUpTestCase() { my_hostname = "dont.query.dns"; }

  TcpPortPool port_pool_;
  TempDirectory bootstrap_dir;
  TempDirectory tmp_dir;
  static std::string my_hostname;

  struct Config {
    std::string ip;
    unsigned int port;
    uint16_t http_port;
    std::string js_filename;
  };

  void bootstrap_failover(
      const std::vector<Config> &servers,
      const std::vector<std::string> &router_options = {},
      int expected_exitcode = 0,
      const std::vector<std::string> &expected_output_regex = {},
      std::chrono::milliseconds wait_for_exit_timeout = 10000ms);

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
    const std::vector<std::string> &router_options, int expected_exitcode,
    const std::vector<std::string> &expected_output_regex,
    std::chrono::milliseconds wait_for_exit_timeout) {
  std::string cluster_name("mycluster");

  std::vector<std::pair<std::string, unsigned>> gr_members;
  for (const auto &mock_server_config : mock_server_configs) {
    gr_members.emplace_back(mock_server_config.ip, mock_server_config.port);
  }

  std::vector<std::tuple<ProcessWrapper &, unsigned int>> mock_servers;

  // start the mocks
  for (const auto &mock_server_config : mock_server_configs) {
    if (mock_server_config.js_filename.empty()) continue;

    const auto port = mock_server_config.port;
    const auto http_port = mock_server_config.http_port;
    mock_servers.emplace_back(
        launch_mysql_server_mock(mock_server_config.js_filename, port,
                                 EXIT_SUCCESS, false, http_port),
        port);

    ProcessWrapper &mock_server = std::get<0>(mock_servers.back());
    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(mock_server, static_cast<uint16_t>(port)));

    EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
    set_mock_bootstrap_data(http_port, cluster_name, gr_members);
  }

  std::vector<std::string> router_cmdline;

  if (router_options.size()) {
    router_cmdline = router_options;
  } else {
    router_cmdline.emplace_back("--bootstrap=" + gr_members[0].first + ":" +
                                std::to_string(gr_members[0].second));

    router_cmdline.emplace_back("--report-host");
    router_cmdline.emplace_back(my_hostname);
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
    EXPECT_THAT(lines,
                ::testing::Contains(
                    "# MySQL Router configured for the InnoDB cluster '" +
                    cluster_name + "'"))
        << "router:" << router.get_full_output() << std::endl
        << mock_servers;

    // check the output configuration file:
    // 1. check if the valid default ttl has been put in the configuraion:
    EXPECT_TRUE(find_in_file(
        bootstrap_dir.name() + "/mysqlrouter.conf",
        [](const std::string &line) -> bool { return line == "ttl=0.5"; },
        std::chrono::milliseconds(0)));
    // 2. check that bootstrap server addresses is no longer in cofiguration
    // file (it has been replaced with dynamic_config)
    const std::string conf_file = bootstrap_dir.name() + "/mysqlrouter.conf";
    EXPECT_FALSE(find_in_file(
        conf_file,
        [](const std::string &line) -> bool {
          return line.find("bootstrap_server_addresses") != std::string::npos;
        },
        std::chrono::milliseconds(0)))
        << get_file_output("mysqlrouter.conf", bootstrap_dir.name());
    // 3. check that the config files (static and dynamic) have the proper
    // access rights
    ASSERT_NO_FATAL_FAILURE(
        check_config_file_access_rights(conf_file, /*read_only=*/true));
    const std::string state_file = bootstrap_dir.name() + "/data/state.json";
    ASSERT_NO_FATAL_FAILURE(
        check_config_file_access_rights(state_file, /*read_only=*/false));
  }
}

class RouterBootstrapTest : public CommonBootstrapTest {};

/**
 * @test
 *       verify that the router's \c --bootstrap can bootstrap
 *       from metadata-servers's PRIMARY over TCP/IP
 * @test
 *       Group Replication roles:
 *       - PRIMARY
 *       - SECONDARY (not used)
 *       - SECONDARY (not used)
 */
TEST_F(RouterBootstrapTest, BootstrapOk) {
  std::vector<Config> config{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap.js").str()},
  };

  bootstrap_failover(config);
}

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
 *       - SECONDARY (not used)
 *       - SECONDARY (not used)
 */
TEST_F(RouterBootstrapTest, BootstrapUserIsCurrentUser) {
  auto current_userid = geteuid();
  auto current_userpw = getpwuid(current_userid);
  if (current_userpw != nullptr) {
    const char *current_username = current_userpw->pw_name;

    std::vector<Config> mock_servers{
        {"127.0.0.1", port_pool_.get_next_available(),
         port_pool_.get_next_available(),
         get_data_dir().join("bootstrap.js").str()},
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

    bootstrap_failover(mock_servers, router_options);
  }
}
#endif

/**
 * @test
 *       verify that the router's \c --bootstrap can bootstrap
 *       from metadata-server's PRIMARY over TCP/IP and generate
 *       a configuration with unix-sockets only
 * @test
 *       Group Replication roles:
 *       - PRIMARY
 *       - SECONDARY (not used)
 *       - SECONDARY (not used)
 */
TEST_F(RouterBootstrapTest, BootstrapOnlySockets) {
  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap.js").str()},
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

  bootstrap_failover(mock_servers, router_options,
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

/**
 * @test
 *       verify that the router's \c --bootstrap detects a unsupported
 *       metadata schema version
 * @test
 *       Group Replication roles:
 *       - PRIMARY
 *       - SECONDARY (not used)
 *       - SECONDARY (not used)
 */
TEST_F(RouterBootstrapTest, BootstrapUnsupportedSchemaVersion) {
  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_unsupported_schema_version.js").str()},
  };

  // check that it failed as expected
  bootstrap_failover(mock_servers, {}, EXIT_FAILURE,
                     {"^Error: This version of MySQL Router is not compatible "
                      "with the provided MySQL InnoDB cluster metadata"});
}

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
TEST_F(RouterBootstrapTest, BootstrapFailoverSuperReadonly) {
  std::vector<Config> config{
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_failover_super_read_only_1.js").str()},
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_failover_super_read_only_2.js").str()},
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), ""},
  };

  bootstrap_failover(config);
}

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
TEST_F(RouterBootstrapTest, BootstrapFailoverSuperReadonly2ndNodeDead) {
  std::vector<Config> config{
      // member-1, PRIMARY, fails at first write
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_failover_super_read_only_1.js").str()},
      // member-2, unreachable
      {"127.0.0.1", 65536,  // 65536 % 0xffff = 0 (port 0), but we bypass
                            // libmysqlclient's default-port assignment
       port_pool_.get_next_available(), ""},
      // member-3, succeeds
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_failover_super_read_only_2.js").str()},
  };

  bootstrap_failover(
      config, {}, EXIT_SUCCESS,
      {
          "^Fetching Group Replication Members",
          "^Failed connecting to 127\\.0\\.0\\.1:65536: .*, trying next$",
      });
}

/**
 * @test
 *       verify that bootstrap fails over and continues if create-account fails
 *       due to 1st node not being writable
 * @test
 *       Group Replication roles:
 *       - SECONDARY
 *       - PRIMARY
 *       - SECONDARY (not used)
 */
TEST_F(RouterBootstrapTest, BootstrapFailoverSuperReadonlyCreateAccountFails) {
  std::vector<Config> config{
      // member-1: SECONDARY, fails at DROP USER due to RW request on RO node
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir()
           .join("bootstrap_failover_super_read_only_dead_2nd_1.js")
           .str()},

      // member-2: PRIMARY, succeeds
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_failover_reconfigure_ok.js").str()},

      // member-3: defined, but unused
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), ""},
  };

  bootstrap_failover(config);
}

/**
 * @test
 *       verify that bootstrap fails over and continues if
 * create-account.drop-user fails due to 1st node not being writable
 * @test
 *       Group Replication roles:
 *       - SECONDARY
 *       - PRIMARY
 *       - SECONDARY (not used)
 *
 */
TEST_F(RouterBootstrapTest,
       BootstrapFailoverSuperReadonlyCreateAccountDropUserFails) {
  std::vector<Config> config{
      // member-1: SECONDARY, fails on CREATE USER due to RW request on RO node
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir()
           .join("bootstrap_failover_super_read_only_delete_user.js")
           .str()},

      // member-2: PRIMARY, succeeds
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir()
           .join("bootstrap_failover_reconfigure_ok_3_old_users.js")
           .str()},

      // member-3: defined, but unused
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), ""},
  };

  bootstrap_failover(config);
}

/**
 * @test
 *       verify that bootstrap fails over and continues if create-account.grant
 * fails
 * @test
 *       Group Replication roles:
 *       - SECONDARY
 *       - PRIMARY
 *       - SECONDARY (not used)
 *
 */
TEST_F(RouterBootstrapTest,
       BootstrapFailoverSuperReadonlyCreateAccountGrantFails) {
  std::vector<Config> config{
      // member-1: PRIMARY, fails after GRANT
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_failover_at_grant.js").str()},

      // member-2: PRIMARY, succeeds
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_failover_reconfigure_ok.js").str()},

      // member-3: defined, but unused
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), ""},
  };

  bootstrap_failover(config);
}

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
       get_data_dir().join("bootstrap_failover_super_read_only_2.js").str()},
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(), ""},
  };

  std::vector<std::string> router_options = {
      "--bootstrap=localhost", "--bootstrap-socket=" + mock_servers.at(0).ip,
      "-d", bootstrap_dir.name()};

  bootstrap_failover(mock_servers, router_options);
}

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
TEST_F(RouterBootstrapTest, BootstrapFailoverSuperReadonlyNewPrimaryCrash) {
  std::vector<Config> mock_servers{
      // member-1: PRIMARY, fails at DROP USER
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir()
           .join("bootstrap_failover_super_read_only_dead_2nd_1.js")
           .str()},

      // member-2: PRIMARY, but crashing
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_failover_at_crash.js").str()},

      // member-3: newly elected PRIMARY, succeeds
      {"127.0.0.1", port_pool_.get_next_available(),
       port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_failover_reconfigure_ok.js").str()},
  };

  bootstrap_failover(mock_servers);
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

  bootstrap_failover(mock_servers, router_options, EXIT_SUCCESS, {});
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

  bootstrap_failover(config, {}, EXIT_FAILURE,
                     {"Access denied for user 'native'@'%' to database "
                      "'mysql_innodb_cluster_metadata"});
}

/**
 * @test
 *       ensure a resonable error message if schema exists, but no
 * group-replication is setup.
 */
TEST_F(RouterBootstrapTest, BootstrapNoGroupReplicationSetup) {
  std::vector<Config> config{
      // member-1: schema exists, but no group replication configured
      {
          "127.0.0.1",
          port_pool_.get_next_available(),
          port_pool_.get_next_available(),
          get_data_dir().join("bootstrap_no_gr.js").str(),
      },
  };

  bootstrap_failover(config, {}, EXIT_FAILURE,
                     {"to have Group Replication running"});
}

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

  bootstrap_failover(config, {}, EXIT_FAILURE,
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

  bootstrap_failover(mock_servers, router_options, EXIT_FAILURE,
                     {"Error: Error executing MySQL query: Lost connection to "
                      "MySQL server during query \\(2013\\)"});
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
        "MySQL Router configured for the InnoDB cluster 'mycluster'", false,
        5s))
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
 *        verify that --account-host without required argument produces an error
 *        and exits
 */
TEST_F(RouterAccountHostTest, argument_missing) {
  const unsigned server_port = port_pool_.get_next_available();

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
      router.expect_output("Error executing MySQL query: String "
                           "'veryveryveryveryveryveryveryveryveryveryveryveryve"
                           "ryveryverylonghost' is too long for host name"))
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
                             "InnoDB cluster 'mycluster'"))
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
 *        verify that --report-host without required argument produces an error
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
       get_data_dir().join("bootstrap.js").str()},
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

  bootstrap_failover(config, router_options);

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
 *       verify that master key file is not overridden by sunsequent bootstrap.
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
       get_data_dir().join("bootstrap.js").str()},
  };

  std::vector<std::string> router_options = {
      "--bootstrap=" + mock_servers.at(0).ip + ":" +
          std::to_string(mock_servers.at(0).port),
      "--report-host",
      my_hostname,
      "-d",
      bootstrap_dir.name(),
      "--force"};

  bootstrap_failover(mock_servers, router_options, EXIT_SUCCESS, {});
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
  const std::string json_stmts = get_data_dir().join("bootstrap.js").str();

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
      std::chrono::milliseconds(0)));

  // check if valid TTL is set (with GR notifications it should be increased to
  // 60s)
  EXPECT_TRUE(find_in_file(
      bootstrap_directory.name() + "/mysqlrouter.conf",
      [](const std::string &line) -> bool { return line == "ttl=60"; },
      std::chrono::milliseconds(0)));
}

/**
 * @test
 *       verify that NOT using --conf-use-gr-notifications
 *       creates a proper config file entry.
 */
TEST_F(RouterBootstrapTest, ConfUseGrNotificationsNo) {
  TempDirectory bootstrap_directory;
  const auto server_port = port_pool_.get_next_available();

  const std::string json_stmts = get_data_dir().join("bootstrap.js").str();

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
      std::chrono::milliseconds(0)));

  // check if valid TTL is set (with no GR notifications it should be 0.5s)
  EXPECT_TRUE(find_in_file(
      bootstrap_directory.name() + "/mysqlrouter.conf",
      [](const std::string &line) -> bool { return line == "ttl=0.5"; },
      std::chrono::milliseconds(0)));
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
  const std::string json_stmts = get_data_dir().join("bootstrap.js").str();
  const unsigned server_port = port_pool_.get_next_available();

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
  const std::string json_stmts = get_data_dir().join("bootstrap.js").str();
  const unsigned server_port = port_pool_.get_next_available();

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
  const std::string json_stmts = get_data_dir().join("bootstrap.js").str();
  const unsigned server_port = port_pool_.get_next_available();

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

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
