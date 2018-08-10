/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "dim.h"
#include "gmock/gmock.h"
#include "keyring/keyring_manager.h"
#include "random_generator.h"
#include "router_component_test.h"
#include "script_generator.h"
#include "socket_operations.h"
#include "tcp_port_pool.h"
#include "utils.h"

/**
 * @file
 * @brief Component Tests for the bootstrap operation
 */

Path g_origin_path;

// we create a number of classes to logically group tests together. But to avoid
// code duplication, we derive them from a class which contains the common code
// they need.
class CommonBootstrapTest : public RouterComponentTest, public ::testing::Test {
 protected:
  static void SetUpTestCase() { my_hostname = "dont.query.dns"; }

  void SetUp() override {
    set_origin(g_origin_path);
    RouterComponentTest::SetUp();
    bootstrap_dir = get_tmp_dir();
    tmp_dir = get_tmp_dir();
  }

  virtual void TearDown() override {
    purge_dir(tmp_dir);
    purge_dir(bootstrap_dir);
  }

  TcpPortPool port_pool_;
  std::string bootstrap_dir;
  std::string tmp_dir;
  static std::string my_hostname;

  struct Config {
    std::string ip;
    unsigned int port;
    std::string in_filename;
    std::string out_filename;
  };

  void bootstrap_failover(
      const std::vector<Config> &servers,
      const std::vector<std::string> &router_options = {},
      int expected_exitcode = 0,
      const std::vector<std::string> &expected_output_regex = {},
      unsigned wait_for_exit_timeout_ms = 10000);

  friend std::ostream &operator<<(
      std::ostream &os,
      const std::vector<
          std::tuple<RouterComponentTest::CommandHandle, unsigned int>> &T);
};

std::string CommonBootstrapTest::my_hostname;

std::ostream &operator<<(
    std::ostream &os,
    const std::vector<
        std::tuple<CommonBootstrapTest::CommandHandle, unsigned int>> &T) {
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
    unsigned wait_for_exit_timeout_ms) {
  std::string cluster_name("mycluster");

  // build environment
  std::map<std::string, std::string> env_vars = {
      {"MYSQL_SERVER_MOCK_CLUSTER_NAME", cluster_name},
      {"MYSQL_SERVER_MOCK_HOST_NAME", my_hostname},
  };

  unsigned int ndx = 1;

  for (const auto &mock_server_config : mock_server_configs) {
    env_vars.emplace("MYSQL_SERVER_MOCK_HOST_" + std::to_string(ndx),
                     mock_server_config.ip);
    env_vars.emplace("MYSQL_SERVER_MOCK_PORT_" + std::to_string(ndx),
                     std::to_string(mock_server_config.port));
    ndx++;
  };

  std::vector<std::tuple<CommandHandle, unsigned int>> mock_servers;

  // start the mocks
  for (const auto &mock_server_config : mock_server_configs) {
    unsigned int port = mock_server_config.port;
    const std::string &in_filename = mock_server_config.in_filename;
    const std::string &out_filename = mock_server_config.out_filename;

    if (in_filename.size()) {
      rewrite_js_to_tracefile(in_filename, out_filename, env_vars);
    }

    if (out_filename.size()) {
      mock_servers.emplace_back(
          launch_mysql_server_mock(out_filename, port, false), port);
    }
  }

  // wait for all mocks to be up
  for (auto &mock_server : mock_servers) {
    auto &proc = std::get<0>(mock_server);
    unsigned int port = std::get<1>(mock_server);

    bool ready = wait_for_port_ready(port, 1000);
    EXPECT_TRUE(ready) << proc.get_full_output();
  }

  std::string router_cmdline;

  if (router_options.size()) {
    for (const auto &piece : router_options) {
      router_cmdline += piece;
      router_cmdline += " ";
    }
  } else {
    router_cmdline = "--bootstrap=" + env_vars.at("MYSQL_SERVER_MOCK_HOST_1") +
                     ":" + env_vars.at("MYSQL_SERVER_MOCK_PORT_1") +
                     " --report-host " + my_hostname + " -d " + bootstrap_dir;
  }

  // launch the router
  auto router = launch_router(router_cmdline);

  // type in the password
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // wait_for_exit() throws at timeout.
  EXPECT_NO_THROW(EXPECT_EQ(router.wait_for_exit(wait_for_exit_timeout_ms),
                            expected_exitcode)
                  << router.get_full_output());

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

  if (0 == expected_exitcode) {
    // fetch all the content for debugging
    for (auto &mock_server : mock_servers) {
      std::get<0>(mock_server).get_full_output();
    }
    EXPECT_THAT(
        lines,
        ::testing::Contains(
            "MySQL Router  has now been configured for the InnoDB cluster '" +
            cluster_name + "'."))
        << "router:" << router.get_full_output() << std::endl
        << mock_servers;

    // check the output configuration file:
    // we check if the valid default ttl has been put in the configuraion
    EXPECT_TRUE(find_in_file(
        bootstrap_dir + "/mysqlrouter.conf",
        [](const std::string &line) -> bool { return line == "ttl=0.5"; },
        std::chrono::milliseconds(0)));
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
      {"127.0.0.1", port_pool_.get_next_available(), "",
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
        {"127.0.0.1", port_pool_.get_next_available(), "",
         get_data_dir().join("bootstrap.js").str()},
    };

    std::vector<std::string> router_options = {
        "--bootstrap=" + mock_servers.at(0).ip + ":" +
            std::to_string(mock_servers.at(0).port),
        "-d",
        bootstrap_dir,
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
      {"127.0.0.1", port_pool_.get_next_available(), "",
       get_data_dir().join("bootstrap.js").str()},
  };

  std::vector<std::string> router_options = {
      "--bootstrap=" + mock_servers.at(0).ip + ":" +
          std::to_string(mock_servers.at(0).port),
      "-d",
      bootstrap_dir,
      "--report-host",
      my_hostname,
      "--conf-skip-tcp",
      "--conf-use-sockets"};

  bootstrap_failover(mock_servers, router_options,
#ifndef _WIN32
                     0,
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
      {"127.0.0.1", port_pool_.get_next_available(), "",
       get_data_dir().join("bootstrap_unsupported_schema_version.js").str()},
  };

  // check that it failed as expected
  bootstrap_failover(mock_servers, {}, 1,
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
       get_data_dir().join("bootstrap_failover_super_read_only_1.js").str(),
       Path(tmp_dir).join("bootstrap_failover_super_read_only_1.json").str()},
      {"127.0.0.1", port_pool_.get_next_available(), "",
       get_data_dir().join("bootstrap_failover_super_read_only_2.js").str()},
      {"127.0.0.1", port_pool_.get_next_available(), "", ""},
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
       get_data_dir().join("bootstrap_failover_super_read_only_1.js").str(),
       Path(tmp_dir).join("member-1.json").str()},
      // member-2, unreachable
      {"127.0.0.1", 65536,  // 65536 % 0xffff = 0 (port 0), but we bypass
                            // libmysqlclient's default-port assignment
       "", ""},
      // member-3, succeeds
      {"127.0.0.1", port_pool_.get_next_available(), "",
       get_data_dir().join("bootstrap_failover_super_read_only_2.js").str()},
  };

  bootstrap_failover(
      config, {}, 0,
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
       get_data_dir()
           .join("bootstrap_failover_super_read_only_dead_2nd_1.js")
           .str(),
       Path(tmp_dir).join("member-1.json").str()},

      // member-2: PRIMARY, succeeds
      {"127.0.0.1", port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_failover_reconfigure_ok.js").str(),
       Path(tmp_dir).join("member-2.json").str()},

      // member-3: defined, but unused
      {"127.0.0.1", port_pool_.get_next_available(), "", ""},
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
       get_data_dir()
           .join("bootstrap_failover_super_read_only_delete_user.js")
           .str(),
       Path(tmp_dir).join("member-1.json").str()},

      // member-2: PRIMARY, succeeds
      {"127.0.0.1", port_pool_.get_next_available(),
       get_data_dir()
           .join("bootstrap_failover_reconfigure_ok_3_old_users.js")
           .str(),
       Path(tmp_dir).join("member-2.json").str()},

      // member-3: defined, but unused
      {"127.0.0.1", port_pool_.get_next_available(), "", ""},
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
       get_data_dir().join("bootstrap_failover_at_grant.js").str(),
       Path(tmp_dir).join("member-1.json").str()},

      // member-2: PRIMARY, succeeds
      {"127.0.0.1", port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_failover_reconfigure_ok.js").str(),
       Path(tmp_dir).join("member-2.json").str()},

      // member-3: defined, but unused
      {"127.0.0.1", port_pool_.get_next_available(), "", ""},
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
       get_data_dir().join("bootstrap_failover_super_read_only_1.js").str(),
       Path(tmp_dir).join("bootstrap_failover_super_read_only_1.json").str()},
      {"127.0.0.1", port_pool_.get_next_available(), "",
       get_data_dir().join("bootstrap_failover_super_read_only_2.js").str()},
      {"127.0.0.1", port_pool_.get_next_available(), "", ""},
  };

  std::vector<std::string> router_options = {
      "--bootstrap=localhost", "--bootstrap-socket=" + mock_servers.at(0).ip,
      "-d", bootstrap_dir};

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
       get_data_dir()
           .join("bootstrap_failover_super_read_only_dead_2nd_1.js")
           .str(),
       Path(tmp_dir).join("member-1.json").str()},

      // member-2: PRIMARY, but crashing
      {"127.0.0.1", port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_failover_at_crash.js").str(),
       Path(tmp_dir).join("member-2.json").str()},

      // member-3: newly elected PRIMARY, succeeds
      {"127.0.0.1", port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_failover_reconfigure_ok.js").str(),
       Path(tmp_dir).join("member-3.json").str()},
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
      {"127.0.0.1", port_pool_.get_next_available(), "",
       get_data_dir().join("bootstrap_exec_time_2_seconds.js").str()},
  };

  std::vector<std::string> router_options = {
      "--bootstrap=" + mock_servers.at(0).ip + ":" +
          std::to_string(mock_servers.at(0).port),
      "--report-host",
      my_hostname,
      "-d",
      bootstrap_dir,
      "--connect-timeout=3",
      "--read-timeout=3"};

  bootstrap_failover(mock_servers, router_options, 0, {});
}

TEST_F(RouterBootstrapTest, BootstrapAccessErrorAtGrantStatement) {
  std::vector<Config> config{
      // member-1: PRIMARY, fails after GRANT
      {"127.0.0.1", port_pool_.get_next_available(),
       get_data_dir().join("bootstrap_access_error_at_grant.js").str(),
       Path(tmp_dir).join("member-1.json").str()},

      // member-2: defined, but unused
      {"127.0.0.1", port_pool_.get_next_available(), "", ""},

      // member-3: defined, but unused
      {"127.0.0.1", port_pool_.get_next_available(), "", ""},
  };

  bootstrap_failover(config, {}, 1,
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
          "",
          get_data_dir().join("bootstrap_no_gr.js").str(),
      },
  };

  bootstrap_failover(config, {}, 1, {"to have Group Replication running"});
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
          "",
          get_data_dir().join("bootstrap_no_schema.js").str(),
      },
  };

  bootstrap_failover(config, {}, 1,
                     {"to contain the metadata of MySQL InnoDB Cluster"});
}

/**
 * @test
 *        verify connection times at bootstrap can be configured
 */
TEST_F(RouterBootstrapTest, BootstrapFailWhenServerResponseExceedsReadTimeout) {
  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(), "",
       get_data_dir().join("bootstrap_exec_time_2_seconds.js").str()},
  };

  std::vector<std::string> router_options = {
      "--bootstrap=" + mock_servers.at(0).ip + ":" +
          std::to_string(mock_servers.at(0).port),
      "-d", bootstrap_dir, "--connect-timeout=1", "--read-timeout=1"};

  bootstrap_failover(mock_servers, router_options, 1,
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

  const std::string bootstrap_directory = get_tmp_dir();
  const unsigned server_port = port_pool_.get_next_available();

  auto test_it = [&](const std::string &cmdline) -> void {
    const std::string json_stmts =
        get_data_dir()
            .join("bootstrap_account_host_multiple_patterns.js")
            .str();

    // launch mock server and wait for it to start accepting connections
    auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false);
    bool ready = wait_for_port_ready(server_port, 1000);
    EXPECT_TRUE(ready) << server_mock.get_full_output();

    // launch the router in bootstrap mode
    std::shared_ptr<void> exit_guard(
        nullptr, [&](void *) { purge_dir(bootstrap_directory); });
    auto router = launch_router(cmdline);

    // add login hook
    router.register_response("Please enter MySQL password for root: ",
                             "fake-pass\n");

    // check if the bootstraping was successful
    EXPECT_TRUE(router.expect_output(
        "MySQL Router  has now been configured for the InnoDB cluster 'test'"))
        << router.get_full_output() << std::endl
        << "server: " << server_mock.get_full_output();
    EXPECT_EQ(router.wait_for_exit(), 0);
  };

  // NOTE: CREATE USER statements should run in unique(sort(hostname_list))
  // fashion

  // --bootstrap before --account-host
  test_it("--bootstrap=127.0.0.1:" + std::to_string(server_port) +
          " --report-host " + my_hostname + " -d " + bootstrap_directory +
          " --account-host host1"       // 2nd CREATE USER
          + " --account-host %"         // 1st CREATE USER
          + " --account-host host1"     // \_ redundant, ignored
          + " --account-host host1"     // /
          + " --account-host host3%");  // 3rd CREATE USER

  // --bootstrap after --account-host
  test_it("-d " + bootstrap_directory + " --report-host " + my_hostname +
          " --account-host host1"     // 2nd CREATE USER
          + " --account-host %"       // 1st CREATE USER
          + " --account-host host1"   // \_ redundant, ignored
          + " --account-host host1"   // /
          + " --account-host host3%"  // 3rd CREATE USER
          + " --bootstrap=127.0.0.1:" + std::to_string(server_port));
}

/**
 * @test
 *        verify that --account-host without required argument produces an error
 *        and exits
 */
TEST_F(RouterAccountHostTest, argument_missing) {
  const unsigned server_port = port_pool_.get_next_available();

  // launch the router in bootstrap mode
  auto router = launch_router("--bootstrap=127.0.0.1:" +
                              std::to_string(server_port) + " --account-host");

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output("option '--account-host' requires a value."))
      << router.get_full_output() << std::endl;
  EXPECT_EQ(router.wait_for_exit(), 1);
}

/**
 * @test
 *        verify that --account-host without --bootstrap switch produces an
 * error and exits
 */
TEST_F(RouterAccountHostTest, without_bootstrap_flag) {
  // launch the router in bootstrap mode
  auto router = launch_router("--account-host host1");

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output(
      "Option --account-host can only be used together with -B/--bootstrap"))
      << router.get_full_output() << std::endl;
  EXPECT_EQ(router.wait_for_exit(), 1);
}

/**
 * @test
 *        verify that --account-host with illegal hostname argument correctly
 * handles the error
 */
TEST_F(RouterAccountHostTest, illegal_hostname) {
  const std::string json_stmts =
      get_data_dir().join("bootstrap_account_host_pattern_too_long.js").str();
  const std::string bootstrap_directory = get_tmp_dir();
  const unsigned server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false);
  bool ready = wait_for_port_ready(server_port, 1000);
  EXPECT_TRUE(ready) << server_mock.get_full_output();

  // launch the router in bootstrap mode
  std::shared_ptr<void> exit_guard(
      nullptr, [&](void *) { purge_dir(bootstrap_directory); });
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port) +
      " --report-host " + my_hostname + " -d " + bootstrap_directory +
      " --account-host "
      "veryveryveryveryveryveryveryveryveryveryveryveryveryveryverylonghost");
  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping was successful
  EXPECT_TRUE(
      router.expect_output("Error executing MySQL query: String "
                           "'veryveryveryveryveryveryveryveryveryveryveryveryve"
                           "ryveryverylonghost' is too long for host name"))
      << router.get_full_output() << std::endl
      << "server:\n"
      << server_mock.get_full_output();
  EXPECT_EQ(router.wait_for_exit(), 1);
}

class RouterReportHostTest : public CommonBootstrapTest {};

/**
 * @test
 *        verify that --report-host works for the typical use case
 */
TEST_F(RouterReportHostTest, typical_usage) {
  const std::string bootstrap_directory = get_tmp_dir();
  const unsigned server_port = port_pool_.get_next_available();

  auto test_it = [&](const std::string &cmdline) -> void {
    const std::string json_stmts =
        get_data_dir().join("bootstrap_report_host.js").str();

    // launch mock server and wait for it to start accepting connections
    auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false);
    bool ready = wait_for_port_ready(server_port, 1000);
    EXPECT_TRUE(ready) << server_mock.get_full_output();

    // launch the router in bootstrap mode
    std::shared_ptr<void> exit_guard(
        nullptr, [&](void *) { purge_dir(bootstrap_directory); });
    auto router = launch_router(cmdline);

    // add login hook
    router.register_response("Please enter MySQL password for root: ",
                             "fake-pass\n");

    // check if the bootstraping was successful
    EXPECT_TRUE(
        router.expect_output("MySQL Router  has now been configured for the "
                             "InnoDB cluster 'mycluster'"))
        << router.get_full_output() << std::endl
        << "server: " << server_mock.get_full_output();
    EXPECT_EQ(router.wait_for_exit(), 0);
  };

  // --bootstrap before --report-host
  test_it("--bootstrap=127.0.0.1:" + std::to_string(server_port) + " -d " +
          bootstrap_directory + " --report-host host.foo.bar");

  // --bootstrap after --report-host
  test_it("-d " + bootstrap_directory + " --report-host host.foo.bar" +
          " --bootstrap=127.0.0.1:" + std::to_string(server_port));
}

/**
 * @test
 *        verify that multiple --report-host arguments produce an error
 *        and exit
 */
TEST_F(RouterReportHostTest, multiple_hostnames) {
  // launch the router in bootstrap mode
  auto router = launch_router(
      "--bootstrap=1.2.3.4:5678 --report-host host1 --report-host host2");

  // check if the bootstraping was successful
  EXPECT_TRUE(
      router.expect_output("Option --report-host can only be used once."))
      << router.get_full_output() << std::endl;
  EXPECT_EQ(router.wait_for_exit(), 1);
}

/**
 * @test
 *        verify that --report-host without required argument produces an error
 *        and exits
 */
TEST_F(RouterReportHostTest, argument_missing) {
  // launch the router in bootstrap mode
  auto router = launch_router("--bootstrap=1.2.3.4:5678 --report-host");

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output("option '--report-host' requires a value."))
      << router.get_full_output() << std::endl;
  EXPECT_EQ(router.wait_for_exit(), 1);
}

/**
 * @test
 *        verify that --report-host without --bootstrap switch produces an error
 *        and exits
 */
TEST_F(RouterReportHostTest, without_bootstrap_flag) {
  // launch the router in bootstrap mode
  auto router = launch_router("--report-host host1");

  // check if the bootstraping was successful
  EXPECT_TRUE(router.expect_output(
      "Option --report-host can only be used together with -B/--bootstrap"))
      << router.get_full_output() << std::endl;
  EXPECT_EQ(router.wait_for_exit(), 1);
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
  auto router = launch_router(
      {"--bootstrap", "1.2.3.4:5678", "--report-host", "^bad^hostname^"});

  // check if the bootstraping was successful
  EXPECT_TRUE(
      router.expect_output("Error: Option --report-host has an invalid value."))
      << router.get_full_output() << std::endl;
  EXPECT_EQ(router.wait_for_exit(), 1);
}

/**
 * @test
 *       verify that bootstrap succeeds when master key writer is used
 *
 */
TEST_F(RouterBootstrapTest,
       NoMasterKeyFileWhenBootstrapPassWithMasterKeyReader) {
  std::vector<Config> config{
      {"127.0.0.1", port_pool_.get_next_available(), "",
       get_data_dir().join("bootstrap.js").str()},
  };

  ScriptGenerator script_generator(g_origin_path, tmp_dir);

  std::vector<std::string> router_options = {
      "--bootstrap=" + config.at(0).ip + ":" +
          std::to_string(config.at(0).port),
      "--report-host",
      my_hostname,
      "-d",
      bootstrap_dir,
      "--master-key-reader=" + script_generator.get_reader_script(),
      "--master-key-writer=" + script_generator.get_writer_script()};

  bootstrap_failover(config, router_options);

  Path tmp(bootstrap_dir);
  Path master_key_file(tmp.join("mysqlrouter.key").str());
  ASSERT_FALSE(master_key_file.exists());

  Path keyring_file(tmp.join("data").join("keyring").str());
  ASSERT_TRUE(keyring_file.exists());

  Path dir(tmp_dir);
  Path data_file(dir.join("master_key").str());
  ASSERT_TRUE(data_file.exists());
}

/**
 * @test
 *       verify that master key file is not overridden by sunsequent bootstrap.
 */
TEST_F(RouterBootstrapTest, MasterKeyFileNotChangedAfterSecondBootstrap) {
  mysql_harness::DIM &dim = mysql_harness::DIM::instance();
  // RandomGenerator
  dim.set_RandomGenerator(
      []() {
        static mysql_harness::RandomGenerator rg;
        return &rg;
      },
      [](mysql_harness::RandomGeneratorInterface *) {});

  mysqlrouter::mkdir(Path(bootstrap_dir).str(), 0777);
  std::string master_key_path = Path(bootstrap_dir).join("master_key").str();
  mysqlrouter::mkdir(Path(bootstrap_dir).join("data").str(), 0777);
  std::string keyring_path =
      Path(bootstrap_dir).join("data").join("keyring").str();

  mysql_harness::init_keyring(keyring_path, master_key_path, true);

  std::string master_key;
  {
    std::ifstream file(master_key_path);
    std::stringstream iss;
    iss << file.rdbuf();
    master_key = iss.str();
  }

  std::vector<Config> mock_servers{
      {"127.0.0.1", port_pool_.get_next_available(), "",
       get_data_dir().join("bootstrap.js").str()},
  };

  std::vector<std::string> router_options = {
      "--bootstrap=" + mock_servers.at(0).ip + ":" +
          std::to_string(mock_servers.at(0).port),
      "--report-host",
      my_hostname,
      "-d",
      bootstrap_dir,
      "--force"};

  bootstrap_failover(mock_servers, router_options, 0, {});
  {
    std::ifstream file(master_key_path);
    std::stringstream iss;
    iss << file.rdbuf();
    ASSERT_THAT(master_key, testing::Eq(iss.str()));
  }
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

  const std::string bootstrap_directory = get_tmp_dir();
  std::shared_ptr<void> exit_guard(
      nullptr, [&](void *) { purge_dir(bootstrap_directory); });

  // populate bootstrap dir with a file, so it's not empty
  EXPECT_NO_THROW({
    mysql_harness::Path path =
        mysql_harness::Path(bootstrap_directory).join("some_file");
    std::ofstream of(path.str());
    of << "blablabla";
  });

  // launch the router in bootstrap mode
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port) +
      " --report-host " + my_hostname + " -d " + bootstrap_directory);
  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // verify that appropriate message was logged (first line) and error message
  // printed (last line)
  std::string err_msg = "Directory '" + bootstrap_directory +
                        "' already contains files\n"
                        "Error: Directory already exits";

  EXPECT_EQ(router.wait_for_exit(), 1);
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

  const std::string bootstrap_directory = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr, [&](void *) {
    chmod(bootstrap_directory.c_str(),
          S_IRUSR | S_IWUSR | S_IXUSR);  // restore RWX for owner
    purge_dir(bootstrap_directory);
  });

  // make bootstrap directory inaccessible to trigger the error
  EXPECT_EQ(chmod(bootstrap_directory.c_str(), 0), 0);

  // launch the router in bootstrap mode: -d set to existing but inaccessible
  // dir
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port) +
      " --report-host " + my_hostname + " -d " + bootstrap_directory);
  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // verify that appropriate message was logged (all but last) and error message
  // printed (last line)
  std::string err_msg =
      "Failed to open directory '.*" + bootstrap_directory +
      "': Permission denied\n"
      "This may be caused by insufficient rights or AppArmor settings.\n.*"
      "Error: Could not check contents of existing deployment directory";

  EXPECT_EQ(router.wait_for_exit(), 1);
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

  const std::string bootstrap_superdir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr, [&](void *) {
    chmod(bootstrap_superdir.c_str(),
          S_IRUSR | S_IWUSR | S_IXUSR);  // restore RWX for owner
    purge_dir(bootstrap_superdir);
  });

  // make bootstrap directory inaccessible to trigger the error
  EXPECT_EQ(chmod(bootstrap_superdir.c_str(), 0), 0);

  // launch the router in bootstrap mode: -d set to non-existent dir and
  // impossible to create
  std::string bootstrap_directory =
      mysql_harness::Path(bootstrap_superdir).join("subdir").str();
  auto router = launch_router(
      "--bootstrap=127.0.0.1:" + std::to_string(server_port) +
      " --report-host " + my_hostname + " -d " + bootstrap_directory);

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // verify that appropriate message was logged (all but last) and error message
  // printed (last line)
  std::string err_msg =
      "Cannot create directory '" + bootstrap_directory +
      "': Permission denied\n"
      "This may be caused by insufficient rights or AppArmor settings.\n.*"
      "Error: Could not create deployment directory";

  EXPECT_EQ(router.wait_for_exit(), 1);
}
#endif

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
