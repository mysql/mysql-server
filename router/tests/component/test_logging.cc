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

#include "dim.h"
#include "gmock/gmock.h"
#include "mysql_session.h"
#include "random_generator.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"

#include <condition_variable>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

/**
 * @file
 * @brief Component Tests for loggers
 */

using testing::HasSubstr;
using testing::StartsWith;
Path g_origin_path;

class RouterLoggingTest : public RouterComponentTest, public ::testing::Test {
 protected:
  virtual void SetUp() {
    set_origin(g_origin_path);
    RouterComponentTest::init();
  }

  TcpPortPool port_pool_;
};

TEST_F(RouterLoggingTest, log_startup_failure_to_console) {
  // This test verifies that fatal error message thrown in MySQLRouter::start()
  // during startup (before Loader::start() takes over) are properly logged to
  // STDERR

  auto conf_params = get_DEFAULT_defaults();
  // we want to log to the console
  conf_params["logging_folder"] = "";
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });
  const std::string conf_file = create_config_file(conf_dir, "", &conf_params);

  // run the router and wait for it to exit
  auto router = launch_router("-c " + conf_file);
  EXPECT_EQ(router.wait_for_exit(), 1);

  // expect something like this to appear on STDERR
  // 2017-06-18 15:24:32 main ERROR [7ffff7fd4780] Error: MySQL Router not
  // configured to load or start any plugin. Exiting.
  const std::string out = router.get_full_output();
  EXPECT_THAT(out.c_str(), HasSubstr(" main ERROR "));
  EXPECT_THAT(out.c_str(), HasSubstr(" Error: MySQL Router not configured to "
                                     "load or start any plugin. Exiting."));
}

TEST_F(RouterLoggingTest, log_startup_failure_to_logfile) {
  // This test is the same as log_startup_failure_to_logfile(), but the failure
  // message is expected to be logged into a logfile

  // create tmp dir where we will log
  const std::string logging_folder = get_tmp_dir();
  std::shared_ptr<void> exit_guard1(nullptr,
                                    [&](void *) { purge_dir(logging_folder); });

  // create config with logging_folder set to that directory
  std::map<std::string, std::string> params = get_DEFAULT_defaults();
  params.at("logging_folder") = logging_folder;
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard2(nullptr,
                                    [&](void *) { purge_dir(conf_dir); });
  const std::string conf_file = create_config_file(conf_dir, "", &params);

  // run the router and wait for it to exit
  auto router = launch_router("-c " + conf_file);
  EXPECT_EQ(router.wait_for_exit(), 1);

  // expect something like this to appear in log:
  // 2017-06-18 15:24:32 main ERROR [7ffff7fd4780] Error: MySQL Router not
  // configured to load or start any plugin. Exiting.
  auto matcher = [](const std::string &line) -> bool {
    return line.find(" main ERROR ") != line.npos &&
           line.find(
               " Error: MySQL Router not configured to load or start any "
               "plugin. Exiting.") != line.npos;
  };
  EXPECT_TRUE(find_in_file(logging_folder + "/mysqlrouter.log", matcher))
      << get_router_log_output("mysqlrouter.log", logging_folder);
}

TEST_F(RouterLoggingTest, bad_logging_folder) {
  // This test verifies that invalid logging_folder is properly handled and
  // appropriate message is printed on STDERR. Router tries to
  // mkdir(logging_folder) if it doesn't exist, then write its log inside of it.

  // create tmp dir to contain our tests
  const std::string tmp_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard1(nullptr,
                                    [&](void *) { purge_dir(tmp_dir); });

// unfortunately it's not (reasonably) possible to make folders read-only on
// Windows, therefore we can run the following 2 tests only on Unix
// https://support.microsoft.com/en-us/help/326549/you-cannot-view-or-change-the-read-only-or-the-system-attributes-of-fo
#ifndef _WIN32

  // make tmp dir read-only
  chmod(tmp_dir.c_str(), S_IRUSR | S_IXUSR);  // r-x for the user (aka 500)

  // logging_folder doesn't exist and can't be created
  {
    const std::string logging_dir = tmp_dir + "/some_dir";

    // create Router config
    std::map<std::string, std::string> params = get_DEFAULT_defaults();
    params.at("logging_folder") = logging_dir;
    const std::string conf_dir = get_tmp_dir("conf");
    std::shared_ptr<void> exit_guard2(nullptr,
                                      [&](void *) { purge_dir(conf_dir); });
    const std::string conf_file = create_config_file(conf_dir, "", &params);

    // run the router and wait for it to exit
    auto router = launch_router("-c " + conf_file);
    EXPECT_EQ(router.wait_for_exit(), 1);

    // expect something like this to appear on STDERR
    // Error: Error when creating dir '/bla': 13
    const std::string out = router.get_full_output();
    EXPECT_THAT(out.c_str(), StartsWith("Error: Error when creating dir '" +
                                        logging_dir + "': 13"));
  }

  // logging_folder exists but is not writeable
  {
    const std::string logging_dir = tmp_dir;

    // create Router config
    std::map<std::string, std::string> params = get_DEFAULT_defaults();
    params.at("logging_folder") = logging_dir;
    const std::string conf_dir = get_tmp_dir("conf");
    std::shared_ptr<void> exit_guard2(nullptr,
                                      [&](void *) { purge_dir(conf_dir); });
    const std::string conf_file = create_config_file(conf_dir, "", &params);

    // run the router and wait for it to exit
    auto router = launch_router("-c " + conf_file);
    EXPECT_EQ(router.wait_for_exit(), 1);

    // expect something like this to appear on STDERR
    // Error: Cannot create file in directory //mysqlrouter.log: Permission
    // denied
    const std::string out = router.get_full_output();
#ifndef _WIN32
    EXPECT_THAT(out.c_str(),
                StartsWith("Error: Cannot create file in directory " +
                           logging_dir + ": Permission denied\n"));
#endif
  }

  // restore writability to tmp dir
  chmod(tmp_dir.c_str(),
        S_IRUSR | S_IWUSR | S_IXUSR);  // rwx for the user (aka 700)

#endif  // #ifndef _WIN32

  // logging_folder is really a file
  {
    const std::string logging_dir = tmp_dir + "/some_file";

    // create that file
    {
      std::ofstream some_file(logging_dir);
      EXPECT_TRUE(some_file.good());
    }

    // create Router config
    std::map<std::string, std::string> params = get_DEFAULT_defaults();
    params.at("logging_folder") = logging_dir;
    const std::string conf_dir = get_tmp_dir("conf");
    std::shared_ptr<void> exit_guard2(nullptr,
                                      [&](void *) { purge_dir(conf_dir); });
    const std::string conf_file = create_config_file(conf_dir, "", &params);

    // run the router and wait for it to exit
    auto router = launch_router("-c " + conf_file);
    EXPECT_EQ(router.wait_for_exit(), 1);

    // expect something like this to appear on STDERR
    // Error: Cannot create file in directory /etc/passwd/mysqlrouter.log: Not a
    // directory
    const std::string out = router.get_full_output();
#ifndef _WIN32
    EXPECT_THAT(out.c_str(),
                StartsWith("Error: Cannot create file in directory " +
                           logging_dir + ": Not a directory\n"));
#else
    // on Windows emulate (wine) we get ENOTDIR
    // with native windows we get ENOENT
    EXPECT_THAT(
        out.c_str(),
        ::testing::AllOf(
            StartsWith("Error: Cannot create file in directory " + logging_dir),
            ::testing::AnyOf(
                ::testing::EndsWith("Directory name invalid.\n\n"),
                ::testing::EndsWith(
                    "The system cannot find the path specified.\n\n"))));
#endif
  }
}

TEST_F(RouterLoggingTest, logger_section_with_key) {
  // This test verifies that [logger:with_some_key] section is handled properly
  // Router should report the error on STDERR and exit

  auto conf_params = get_DEFAULT_defaults();
  // we want to log to the console
  conf_params["logging_folder"] = "";
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });
  const std::string conf_file =
      create_config_file(conf_dir, "[logger:some_key]\n", &conf_params);

  // run the router and wait for it to exit
  auto router = launch_router("-c " + conf_file);
  EXPECT_EQ(router.wait_for_exit(), 1);

  // expect something like this to appear on STDERR
  // Error: Section 'logger' does not support keys
  const std::string out = router.get_full_output();
  EXPECT_THAT(out.c_str(),
              StartsWith("Error: Section 'logger' does not support keys"));
}

TEST_F(RouterLoggingTest, multiple_logger_sections) {
  // This test verifies that multiple [logger] sections are handled properly.
  // Router should report the error on STDERR and exit

  auto conf_params = get_DEFAULT_defaults();
  // we want to log to the console
  conf_params["logging_folder"] = "";
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });
  const std::string conf_file =
      create_config_file(conf_dir, "[logger]\n[logger]\n", &conf_params);

  // run the router and wait for it to exit
  auto router = launch_router("-c " + conf_file);
  EXPECT_EQ(router.wait_for_exit(), 1);

  // expect something like this to appear on STDERR
  // Error: Configuration error: Section 'logger' already exists
  const std::string out = router.get_full_output();
  EXPECT_THAT(
      out.c_str(),
      StartsWith(
          "Error: Configuration error: Section 'logger' already exists"));
}

TEST_F(RouterLoggingTest, bad_loglevel) {
  // This test verifies that bad log level in [logger] section is handled
  // properly. Router should report the error on STDERR and exit

  auto conf_params = get_DEFAULT_defaults();
  // we want to log to the console
  conf_params["logging_folder"] = "";
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });
  const std::string conf_file =
      create_config_file(conf_dir, "[logger]\nlevel = UNKNOWN\n", &conf_params);

  // run the router and wait for it to exit
  auto router = launch_router("-c " + conf_file);
  EXPECT_EQ(router.wait_for_exit(), 1);

  // expect something like this to appear on STDERR
  // 2017-08-14 16:03:44 main ERROR [7f7a61be6780] Configuration error: Log
  // level 'unknown' is not valid. Valid values are: debug, error, fatal, info,
  // and warning
  const std::string out = router.get_full_output();
  EXPECT_THAT(out.c_str(), HasSubstr(" main ERROR "));
  EXPECT_THAT(
      out.c_str(),
      HasSubstr(" Configuration error: Log level 'unknown' is not valid. Valid "
                "values are: debug, error, fatal, info, and warning"));
}

TEST_F(RouterLoggingTest, bad_loglevel_gets_logged) {
  // This test is the same as bad_loglevel(), but the failure
  // message is expected to be logged into a logfile

  // create tmp dir where we will log
  const std::string logging_folder = get_tmp_dir();
  std::shared_ptr<void> exit_guard1(nullptr,
                                    [&](void *) { purge_dir(logging_folder); });

  // create config with logging_folder set to that directory
  std::map<std::string, std::string> params = get_DEFAULT_defaults();
  params.at("logging_folder") = logging_folder;
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard2(nullptr,
                                    [&](void *) { purge_dir(conf_dir); });
  const std::string conf_file =
      create_config_file(conf_dir, "[logger]\nlevel = UNKNOWN\n", &params);

  // run the router and wait for it to exit
  auto router = launch_router("-c " + conf_file);
  EXPECT_EQ(router.wait_for_exit(), 1);

  // expect something like this to appear on STDERR
  // 2017-08-14 16:03:44 main ERROR [7f7a61be6780] Configuration error: Log
  // level 'unknown' is not valid. Valid values are: debug, error, fatal, info,
  // and warning
  auto matcher = [](const std::string &line) -> bool {
    return line.find(" main ERROR ") != line.npos &&
           line.find(
               " Configuration error: Log level 'unknown' is not valid. Valid "
               "values are: debug, error, fatal, info, and warning") !=
               line.npos;
  };
  EXPECT_TRUE(find_in_file(logging_folder + "/mysqlrouter.log", matcher))
      << get_router_log_output("mysqlrouter.log", logging_folder);
}

TEST_F(RouterLoggingTest, very_long_router_name_gets_properly_logged) {
  // This test verifies that a very long router name gets truncated in the
  // logged message (this is done because if it doesn't happen, the entire
  // message will exceed log message max length, and then the ENTIRE message
  // will get truncated instead. It's better to truncate the long name rather
  // than the stuff that follows it).
  // Router should report the error on STDERR and exit

  const std::string json_stmts = get_data_dir().join("bootstrap.js").str();
  const std::string bootstrap_dir = get_tmp_dir();

  const unsigned server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  RouterComponentTest::CommandHandle server_mock =
      launch_mysql_server_mock(json_stmts, server_port);
  EXPECT_TRUE(wait_for_port_ready(server_port, 5000))
      << server_mock.get_full_output();

  constexpr char name[] =
      "veryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryvery"
      "veryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryvery"
      "veryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryvery"
      "veryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryvery"
      "verylongname";
  static_assert(
      sizeof(name) > 255,
      "too long");  // log message max length is 256, we want something that
                    // guarrantees the limit would be exceeded

  // launch the router in bootstrap mode
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(bootstrap_dir); });
  auto router =
      launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port) +
                    " --name " + name + " -d " + bootstrap_dir);
  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // wait for router to exit
  EXPECT_EQ(router.wait_for_exit(), 1);

  // expect something like this to appear on STDERR
  // Error: Router name
  // 'veryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryv...'
  // too long (max 255).
  const std::string out = router.get_full_output();
  EXPECT_THAT(out.c_str(),
              HasSubstr("Error: Router name "
                        "'veryveryveryveryveryveryveryveryveryveryveryveryveryv"
                        "eryveryveryveryveryveryv...' too long (max 255)."));
}

/**
 * @test verify that debug logs are not written to console during boostrap if
 * bootstrap configuration file is not provided.
 */
TEST_F(RouterLoggingTest, is_debug_logs_disabled_if_no_bootstrap_config_file) {
  const std::string json_stmts = get_data_dir().join("bootstrap.js").str();

  const std::string bootstrap_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(bootstrap_dir); });

  const unsigned server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false);
  EXPECT_TRUE(wait_for_port_ready(server_port, 1000))
      << "Timed out waiting for mock server port ready" << std::endl
      << server_mock.get_full_output();

  // launch the router in bootstrap mode
  auto router =
      launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port) +
                    " --report-host dont.query.dns" + " -d " + bootstrap_dir);

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping was successful
  EXPECT_EQ(router.wait_for_exit(), 0);
  EXPECT_THAT(router.get_full_output(),
              testing::Not(testing::HasSubstr("Executing query:")));
}

/**
 * @test verify that debug logs are written to console during boostrap if
 * log_level is set to DEBUG in bootstrap configuration file.
 */
TEST_F(RouterLoggingTest, is_debug_logs_enabled_if_bootstrap_config_file) {
  const std::string json_stmts = get_data_dir().join("bootstrap.js").str();

  const std::string bootstrap_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(bootstrap_dir); });

  const std::string bootstrap_conf = get_tmp_dir();
  std::shared_ptr<void> conf_exit_guard(
      nullptr, [&](void *) { purge_dir(bootstrap_conf); });

  const unsigned server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false);
  EXPECT_TRUE(wait_for_port_ready(server_port, 1000))
      << "Timed out waiting for mock server port ready" << std::endl
      << server_mock.get_full_output();

  // launch the router in bootstrap mode
  std::string logger_section = "[logger]\nlevel = DEBUG\n";
  auto conf_params = get_DEFAULT_defaults();
  // we want to log to the console
  conf_params["logging_folder"] = "";
  std::string conf_file = create_config_file(bootstrap_conf, logger_section,
                                             &conf_params, "bootstrap.conf");

  auto router =
      launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port) +
                    " --report-host dont.query.dns" + " --force -d " +
                    bootstrap_dir + " -c " + conf_file);

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping was successful
  EXPECT_EQ(router.wait_for_exit(), 0)
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();
  EXPECT_THAT(router.get_full_output(), testing::HasSubstr("Executing query:"));
}

/**
 * @test verify that debug logs are written to mysqlrouter.log file during
 * bootstrap if loggin_folder is provided in bootstrap configuration file
 */
TEST_F(RouterLoggingTest, is_debug_logs_written_to_file_if_logging_folder) {
  const std::string json_stmts = get_data_dir().join("bootstrap.js").str();

  const std::string bootstrap_dir = get_tmp_dir();
  const std::shared_ptr<void> exit_guard(
      nullptr, [&](void *) { purge_dir(bootstrap_dir); });

  std::string bootstrap_conf = get_tmp_dir();
  std::shared_ptr<void> conf_exit_guard(
      nullptr, [&](void *) { purge_dir(bootstrap_conf); });

  const unsigned server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false);
  EXPECT_TRUE(wait_for_port_ready(server_port, 1000))
      << "Timed out waiting for mock server port ready" << std::endl
      << server_mock.get_full_output();

  // create config with logging_folder set to that directory
  std::map<std::string, std::string> params = {{"logging_folder", ""}};
  params.at("logging_folder") = bootstrap_conf;
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard2(nullptr,
                                    [&](void *) { purge_dir(conf_dir); });
  const std::string conf_file =
      create_config_file(conf_dir, "[logger]\nlevel = DEBUG\n", &params);

  auto router =
      launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port) +
                    " --report-host dont.query.dns" + " --force -d " +
                    bootstrap_dir + " -c " + conf_file);

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping was successful
  EXPECT_EQ(router.wait_for_exit(), 0)
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

  auto matcher = [](const std::string &line) -> bool {
    return line.find("Executing query:") != line.npos;
  };

  EXPECT_TRUE(find_in_file(bootstrap_conf + "/mysqlrouter.log", matcher,
                           std::chrono::milliseconds(5000)))
      << get_router_log_output("mysqlrouter.log", bootstrap_conf);
}

/**
 * @test verify that normal output is written to stdout during bootstrap if
 * logging_folder is not provided in bootstrap configuration file.
 *
 * @test verify that logs are not written to stdout during bootstrap.
 */
TEST_F(RouterLoggingTest, bootstrap_normal_logs_written_to_stdout) {
  const std::string json_stmts = get_data_dir().join("bootstrap.js").str();

  const std::string bootstrap_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(bootstrap_dir); });

  const std::string bootstrap_conf = get_tmp_dir();
  std::shared_ptr<void> conf_exit_guard(
      nullptr, [&](void *) { purge_dir(bootstrap_conf); });

  const unsigned server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false);
  EXPECT_TRUE(wait_for_port_ready(server_port, 1000))
      << "Timed out waiting for mock server port ready" << std::endl
      << server_mock.get_full_output();

  // launch the router in bootstrap mode
  std::string logger_section = "[logger]\nlevel = DEBUG\n";
  auto conf_params = get_DEFAULT_defaults();
  // we want to log to the console
  conf_params["logging_folder"] = "";
  std::string conf_file = create_config_file(bootstrap_conf, logger_section,
                                             &conf_params, "bootstrap.conf");

  auto router =
      launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port) +
                        " --report-host dont.query.dns" + " --force -d " +
                        bootstrap_dir + " -c " + conf_file,
                    false /*false = capture only stdout*/);

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping was successful
  EXPECT_EQ(router.wait_for_exit(), 0)
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

  // check if logs are not written to output
  EXPECT_THAT(router.get_full_output(),
              testing::Not(testing::HasSubstr("Executing query:")));

  // check if normal output is written to output
  EXPECT_THAT(
      router.get_full_output(),
      testing::HasSubstr("The following connection information can be used to "
                         "connect to the cluster after MySQL Router has been "
                         "started with generated configuration."));

  EXPECT_THAT(
      router.get_full_output(),
      testing::HasSubstr(
          "Classic MySQL protocol connections to cluster 'mycluster':"));

  EXPECT_THAT(
      router.get_full_output(),
      testing::HasSubstr("X protocol connections to cluster 'mycluster':"));
}

class MetadataCacheLoggingTest : public RouterLoggingTest {
 protected:
  void SetUp() override {
    set_origin(g_origin_path);
    RouterComponentTest::init();

    mysql_harness::DIM &dim = mysql_harness::DIM::instance();
    // RandomGenerator
    dim.set_RandomGenerator(
        []() {
          static mysql_harness::RandomGenerator rg;
          return &rg;
        },
        [](mysql_harness::RandomGeneratorInterface *) {});

    temp_test_dir = get_tmp_dir();
    logging_folder = get_tmp_dir();

    cluster_nodes_ports = {port_pool_.get_next_available(),
                           port_pool_.get_next_available(),
                           port_pool_.get_next_available()};
    router_port = port_pool_.get_next_available();
    metadata_cache_section = get_metadata_cache_section(cluster_nodes_ports);
    routing_section =
        get_metadata_cache_routing_section("PRIMARY", "round-robin", "");

    write_json_file(get_data_dir());
  }

  void TearDown() override {
    purge_dir(temp_test_dir);
    purge_dir(logging_folder);
  }

  void write_json_file(const Path &data_dir) {
    std::map<std::string, std::string> json_vars = {
        {"PRIMARY_HOST", "127.0.0.1:" + std::to_string(cluster_nodes_ports[0])},
        {"SECONDARY_1_HOST",
         "127.0.0.1:" + std::to_string(cluster_nodes_ports[1])},
        {"SECONDARY_2_HOST",
         "127.0.0.1:" + std::to_string(cluster_nodes_ports[2])},

        {"PRIMARY_PORT", std::to_string(cluster_nodes_ports[0])},
        {"SECONDARY_1_PORT", std::to_string(cluster_nodes_ports[1])},
        {"SECONDARY_2_PORT", std::to_string(cluster_nodes_ports[2])},
    };

    // launch the primary node working also as metadata server
    json_primary_node_template_ =
        data_dir.join("metadata_3_nodes_first_not_accessible.js").str();
    json_primary_node_ = Path(temp_test_dir)
                             .join("metadata_3_nodes_first_not_accessible.json")
                             .str();
    rewrite_js_to_tracefile(json_primary_node_template_, json_primary_node_,
                            json_vars);
  }

  std::string get_metadata_cache_section(std::vector<unsigned> ports) {
    std::string metadata_caches = "bootstrap_server_addresses=";

    for (auto it = ports.begin(); it != ports.end(); ++it) {
      metadata_caches += (it == ports.begin()) ? "" : ",";
      metadata_caches += "mysql://localhost:" + std::to_string(*it);
    }
    metadata_caches += "\n";

    return "[metadata_cache:test]\n"
           "router_id=1\n" +
           metadata_caches +
           "user=mysql_router1_user\n"
           "metadata_cluster=test\n"
           "ttl=500\n\n";
  }

  std::string get_metadata_cache_routing_section(const std::string &role,
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

  std::string init_keyring_and_config_file(const std::string &conf_dir) {
    auto default_section = get_DEFAULT_defaults();
    init_keyring(default_section, temp_test_dir);
    default_section["logging_folder"] = logging_folder;
    return create_config_file(
        conf_dir,
        "[logger]\nlevel = DEBUG\n" + metadata_cache_section + routing_section,
        &default_section);
  }

  TcpPortPool port_pool_;
  std::string json_primary_node_template_;
  std::string json_primary_node_;
  std::string temp_test_dir;
  std::string logging_folder;
  std::vector<unsigned> cluster_nodes_ports;
  unsigned router_port;
  std::string metadata_cache_section;
  std::string routing_section;
};

/**
 * @test verify if error message is logged if router cannot connect to any
 *       metadata server.
 */
TEST_F(MetadataCacheLoggingTest,
       log_error_when_cannot_connect_to_any_metadata_server) {
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });

  // launch the router with metadata-cache configuration
  auto router = RouterComponentTest::launch_router(
      "-c " + init_keyring_and_config_file(conf_dir));
  bool router_ready = wait_for_port_ready(router_port, 1000);
  EXPECT_TRUE(router_ready) << router.get_full_output();

  // expect something like this to appear on STDERR
  // 2017-12-21 17:22:35 metadata_cache ERROR [7ff0bb001700] Failed connecting
  // with any of the bootstrap servers
  auto matcher = [](const std::string &line) -> bool {
    return line.find("metadata_cache ERROR") != line.npos &&
           line.find("Failed connecting with any of the metadata servers") !=
               line.npos;
  };

  EXPECT_TRUE(find_in_file(logging_folder + "/mysqlrouter.log", matcher,
                           std::chrono::milliseconds(5000)))
      << get_router_log_output("mysqlrouter.log", logging_folder);
}

/**
 * @test verify if appropriate warning messages are logged when cannot connect
 * to first metadata server, but can connect to another one.
 */
TEST_F(MetadataCacheLoggingTest,
       log_warning_when_cannot_connect_to_first_metadata_server) {
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });

  // launch second metadata server
  auto server = launch_mysql_server_mock(json_primary_node_,
                                         cluster_nodes_ports[1], false);
  bool server_ready = wait_for_port_ready(cluster_nodes_ports[1], 1000);
  EXPECT_TRUE(server_ready) << server.get_full_output();

  // launch the router with metadata-cache configuration
  auto router = RouterComponentTest::launch_router(
      "-c " + init_keyring_and_config_file(conf_dir));
  bool router_ready = wait_for_port_ready(router_port, 1000);
  EXPECT_TRUE(router_ready) << router.get_full_output();

  // expect something like this to appear on STDERR
  // 2017-12-21 17:22:35 metadata_cache WARNING [7ff0bb001700] Failed connecting
  // with Metadata Server 127.0.0.1:7002: Can't connect to MySQL server on
  // '127.0.0.1' (111) (2003)
  auto info_matcher = [&](const std::string &line) -> bool {
    return line.find("metadata_cache WARNING") != line.npos &&
           line.find("Failed connecting with Metadata Server 127.0.0.1:" +
                     std::to_string(cluster_nodes_ports[0])) != line.npos;
  };

  EXPECT_TRUE(find_in_file(logging_folder + "/mysqlrouter.log", info_matcher,
                           std::chrono::milliseconds(10000)))
      << get_router_log_output("mysqlrouter.log", logging_folder);

  auto warning_matcher = [](const std::string &line) -> bool {
    return line.find("metadata_cache WARNING") != line.npos &&
           line.find(
               "While updating metadata, could not establish a connection to "
               "replicaset") != line.npos;
  };
  EXPECT_TRUE(find_in_file(logging_folder + "/mysqlrouter.log", warning_matcher,
                           std::chrono::milliseconds(10000)))
      << get_router_log_output();
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
