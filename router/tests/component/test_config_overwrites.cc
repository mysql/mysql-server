/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>

#include "config_builder.h"
#include "mysql/harness/string_utils.h"  // split_string
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "tcp_port_pool.h"

using namespace std::chrono_literals;
using namespace std::string_literals;
using testing::StartsWith;

class RouterConfigOwerwriteTest : public RouterComponentTest {
 protected:
  auto &launch_router(const std::vector<std::string> &params,
                      int expected_exit_code,
                      std::chrono::milliseconds wait_ready = -1s) {
    return ProcessManager::launch_router(params, expected_exit_code, true,
                                         false, wait_ready);
  }

  std::string get_routing_section(uint16_t router_port,
                                  uint16_t destination_port,
                                  const std::string &key) {
    return mysql_harness::ConfigBuilder::build_section(
        "routing:" + key,
        {
            {"bind_port", std::to_string(router_port)},
            {"protocol", "classic"},
            {"routing_strategy", "first-available"},
            {"destinations", "127.0.0.1:" + std::to_string(destination_port)},
        });
  }

  std::string get_keepalive_section() {
    return mysql_harness::ConfigBuilder::build_section("keepalive",
                                                       {
                                                           {"interval", "10"},
                                                       });
  }

  ProcessWrapper &launch_router_for_bootstrap(
      const std::vector<std::string> &params,
      int expected_exit_code = EXIT_SUCCESS) {
    return ProcessManager::launch_router(
        params, expected_exit_code, /*catch_stderr=*/true, /*with_sudo=*/false,
        /*wait_for_notify_ready=*/-1s);
  }

  static bool wait_file_exists(const std::string &file,
                               std::chrono::milliseconds timeout = 5s) {
    if (getenv("WITH_VALGRIND")) {
      timeout *= 10;
    }

    const auto MSEC_STEP = 20ms;
    bool found = false;
    using clock_type = std::chrono::steady_clock;
    const auto end = clock_type::now() + timeout;
    do {
      found = mysql_harness::Path(file).exists();
      if (!found) {
        auto step = std::min(timeout, MSEC_STEP);
        RouterComponentTest::sleep_for(step);
      }
    } while (!found && clock_type::now() < end);

    return found;
  }

  TempDirectory conf_dir{"conf"};
  const std::string simple_trace_file{get_data_dir().join("my_port.js").str()};
};

class BootstrapDebugLevelOkTest
    : public RouterConfigOwerwriteTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(BootstrapDebugLevelOkTest, BootstrapDebugLevelOk) {
  const auto overwrite_param = GetParam();
  const std::string tracefile = "bootstrap_gr.js";
  TempDirectory bootstrap_dir;
  const std::string debug_level_output =
      "SELECT * FROM mysql_innodb_cluster_metadata.schema_version";

  const uint16_t server_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join(tracefile).str();
  launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);

  // launch the router in bootstrap mode
  std::vector<std::string> cmdline = {
      "--bootstrap=root:fake-pass@localhost:"s + std::to_string(server_port),
      "-d", bootstrap_dir.name()};
  cmdline.push_back(overwrite_param);
  auto &router = launch_router_for_bootstrap(cmdline, EXIT_SUCCESS);

  check_exit_code(router, EXIT_SUCCESS);

  // check that bootstrap outputs debug logs
  EXPECT_TRUE(router.output_contains(debug_level_output));

  // check that the resulting config still has default log level
  const std::string conf_content =
      get_file_output("mysqlrouter.conf", bootstrap_dir.name());
  const std::vector<std::string> lines =
      mysql_harness::split_string(conf_content, '\n');
  EXPECT_THAT(lines, ::testing::Contains("level=INFO"));
  EXPECT_THAT(lines, ::testing::Not(::testing::Contains(
                         ::testing::AnyOf("level=debug", "level=DEBUG"))));
}

INSTANTIATE_TEST_SUITE_P(BootstrapDebugLevelOk, BootstrapDebugLevelOkTest,
                         ::testing::Values("--logger.level=debug",
                                           "--logger.level=DEBUG"));

struct OverwriteErrorTestParam {
  std::vector<std::string> overwrite_params;
  std::string expected_error_msg;
};

class BootstrapOverwriteErrorTest
    : public RouterConfigOwerwriteTest,
      public ::testing::WithParamInterface<OverwriteErrorTestParam> {};

TEST_P(BootstrapOverwriteErrorTest, BootstrapOverwriteError) {
  const auto param = GetParam();
  const std::string tracefile = "bootstrap_gr.js";
  TempDirectory bootstrap_dir;

  const uint16_t server_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join(tracefile).str();
  launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false);

  // launch the router in bootstrap mode
  std::vector<std::string> cmdline = {
      "--bootstrap=root:fake-pass@localhost:"s + std::to_string(server_port),
      "-d", bootstrap_dir.name()};

  cmdline.insert(cmdline.end(), param.overwrite_params.begin(),
                 param.overwrite_params.end());

  auto &router = launch_router_for_bootstrap(cmdline, EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_TRUE(router.output_contains(param.expected_error_msg));
}

INSTANTIATE_TEST_SUITE_P(
    BootstrapOverwriteError, BootstrapOverwriteErrorTest,
    ::testing::Values(
        OverwriteErrorTestParam{
            {"--logger.level", "DEBUG2"},
            "Configuration error: Log level 'debug2' is not valid."},
        OverwriteErrorTestParam{
            {"--logger.sinks", "filelog"},
            "Invalid argument '--logger.sinks'. Only "
            "'--logger.level' configuration option can be set with a command "
            "line parameter when bootstrapping."},
        OverwriteErrorTestParam{
            {"--DEFAULT.read_timeout", "30"},
            "Invalid argument '--DEFAULT.read_timeout'. Only "
            "'--logger.level' configuration option can be set with a command "
            "line parameter when bootstrapping."},
        OverwriteErrorTestParam{
            {"--abc.read_timeout", "30"},
            "Invalid argument '--abc.read_timeout'. Only "
            "'--logger.level' configuration option can be set with a command "
            "line parameter when bootstrapping."}));

class OverwriteLogLevelTest
    : public RouterConfigOwerwriteTest,
      public ::testing::WithParamInterface<std::string> {};

/* @test Verify that using --logger.level on top of --DEFAULT.logging_folder
 * overwrite works as expected */
TEST_P(OverwriteLogLevelTest, OverwriteLogLevel) {
  const std::string debug_level_log_entry =
      ".* main DEBUG .* Starting: keepalive.";

  const std::string keepalive_section = get_keepalive_section();
  const std::string conf_file =
      create_config_file(conf_dir.name(), keepalive_section,
                         /*default_section*/ nullptr,
                         /*name*/ "mysqlrouter.conf",
                         /*extra_defaults*/ "",
                         /*enable_debug_logging*/ false);

  TempDirectory log_dir;
  const std::string log_dir_overwrite =
      "--DEFAULT.logging_folder=" + log_dir.name();
  const std::string log_level_overwrite = GetParam();
  auto &router =
      launch_router({"-c", conf_file, log_dir_overwrite, log_level_overwrite},
                    EXIT_SUCCESS, 5s);
  router.set_logging_path(log_dir.name(), "mysqlrouter.log");

  // check that the Router logs the debug level despite the conf file does not
  // configure it
  EXPECT_TRUE(wait_log_contains(router, debug_level_log_entry, 5s));

  const std::string conf_content =
      get_file_output("mysqlrouter.conf", conf_dir.name());
  const std::vector<std::string> lines =
      mysql_harness::split_string(conf_content, '\n');
  EXPECT_THAT(lines, ::testing::Not(::testing::Contains(
                         ::testing::AnyOf("level=debug", "level=DEBUG"))));
}

INSTANTIATE_TEST_SUITE_P(
    OverwriteLogLevel, OverwriteLogLevelTest,
    ::testing::Values("--logger.level=debug", "--logger.level=DEBUG",
                      "--LOGGER.level=DEBUG", "--logger.LEVEL=DEBUG",
                      "--Logger.Level=DEBUG", "--LOGGER.LEVEL=DEBUG"));

/* @test Verify that using --DEBUG.logging_folder overwrite works as expected */
TEST_F(RouterConfigOwerwriteTest, OverwriteLoggingFolder) {
  const std::string keepalive_section = get_keepalive_section();

  // create config file without logging_folder configured
  const std::string orign_dir = ProcessManager::get_origin().str();
  const std::string plugin_dir = ProcessManager::get_plugin_dir().str();
  const std::map<std::string, std::string> default_section = {
      {"plugin_folder", plugin_dir},
      {"runtime_folder", orign_dir},
      {"config_folder", orign_dir},
      {"data_folder", orign_dir}};
  const std::string conf_file =
      create_config_file(conf_dir.name(), keepalive_section, &default_section,
                         /*name*/ "mysqlrouter.conf",
                         /*extra_defaults*/ "",
                         /*enable_debug_logging*/ false);

  // make sure config file does not have logging_folder configured
  const std::string conf_content =
      get_file_output("mysqlrouter.conf", conf_dir.name());
  const std::vector<std::string> lines =
      mysql_harness::split_string(conf_content, '\n');
  EXPECT_THAT(lines, ::testing::Not(::testing::Contains(
                         ::testing::HasSubstr("logging_folder"))));

  TempDirectory logging_dir{"conf"};

  const std::string overwrite_param =
      "--DEFAULT.logging_FOLDER=" + logging_dir.name();
  // auto &router =
  launch_router({"-c", conf_file, overwrite_param}, EXIT_SUCCESS, 5s);

  // check that the Router logs the debug level despite the conf file does not
  // configure it
  EXPECT_TRUE(wait_file_exists(logging_dir.name() + "/" + "mysqlrouter.log"));
}

/* @test Sunny-day scenario, we check that overwriting an option in the
 * configuration file with a command line parameter works */
TEST_F(RouterConfigOwerwriteTest, OverwriteRoutingPort) {
  const auto router_port = port_pool_.get_next_available();
  const auto server_port = port_pool_.get_next_available();
  const auto router_port_overwrite = port_pool_.get_next_available();

  const std::string routing_section =
      get_routing_section(router_port, server_port, "A");
  const std::string conf_file =
      create_config_file(conf_dir.name(), routing_section);

  const std::string overwrite_param =
      "--routing:A.bind_port=" + std::to_string(router_port_overwrite);

  launch_mysql_server_mock(simple_trace_file, server_port, EXIT_SUCCESS);

  launch_router({"-c", conf_file, overwrite_param}, EXIT_SUCCESS, 5s);

  make_new_connection_ok(router_port_overwrite, server_port);
  verify_new_connection_fails(router_port);
}

/* @test Check that overwriting an option that does not exist in the
 * configuration file adds this option to the configuration */
TEST_F(RouterConfigOwerwriteTest, OverwriteOptionMissingInTheConfig) {
  const auto router_port = port_pool_.get_next_available();
  const auto server_port = port_pool_.get_next_available();

  const std::string routing_section =
      get_routing_section(router_port, server_port, "A");
  const std::string conf_file =
      create_config_file(conf_dir.name(), routing_section);

  const std::string overwrite_param = "--routing:A.max_connect_errors=1";

  launch_mysql_server_mock(simple_trace_file, server_port, EXIT_SUCCESS);

  launch_router({"-c", conf_file, overwrite_param}, EXIT_SUCCESS, 5s);

  make_bad_connection(router_port);

  // since we set the max_connect_errors threshold to 1 and made one connection
  // error already the next connection attempt should fail
  verify_new_connection_fails(router_port);
}

class OverwriteIgnoreUnknownOptionTest
    : public RouterConfigOwerwriteTest,
      public ::testing::WithParamInterface<std::string> {};

/* @test Non-existing option of a valid section is just ignored the same way it
 * is in the configuration file when unknown_config_option=warning */
TEST_P(OverwriteIgnoreUnknownOptionTest, OverwriteIgnoreUnknownOption) {
  const auto router_port1 = port_pool_.get_next_available();
  const auto router_port2 = port_pool_.get_next_available();
  const auto server_port = port_pool_.get_next_available();

  const std::string routing_section1 =
      get_routing_section(router_port1, server_port, "main01");
  const std::string routing_section2 =
      get_routing_section(router_port2, server_port, "Main01");
  const std::string conf_file =
      create_config_file(conf_dir.name(), routing_section1 + routing_section2);

  const std::string overwrite_param = GetParam();

  launch_mysql_server_mock(simple_trace_file, server_port, EXIT_SUCCESS);

  launch_router({"-c", conf_file, overwrite_param,
                 "--DEFAULT.unknown_config_option", "warning"},
                EXIT_SUCCESS, 5s);

  make_new_connection_ok(router_port1, server_port);
}

INSTANTIATE_TEST_SUITE_P(
    OverwriteIgnoreUnknownOption, OverwriteIgnoreUnknownOptionTest,
    ::testing::Values("--DEFAULT.help=please", "--routing:Main01.help=please",
                      "--routing:main01.help=please", "--DEFAULT.help="));

class OverwriteErrorTest
    : public RouterConfigOwerwriteTest,
      public ::testing::WithParamInterface<OverwriteErrorTestParam> {};

/* @test Check that overwritten option is validated properly if it is used and
 * is not going to be ignored */
TEST_P(OverwriteErrorTest, OverwriteError) {
  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, conf_dir.name());

  const std::string conf_file =
      create_config_file(conf_dir.name(), "", &default_section);

  std::vector<std::string> cmdline{"-c", conf_file};

  cmdline.insert(cmdline.end(), GetParam().overwrite_params.begin(),
                 GetParam().overwrite_params.end());

  auto &router = launch_router(cmdline, EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);
  // logger issues will be reported on the console, the other in the logfile so
  // we merge them both when looking for error messages
  const std::string router_out =
      router.get_logfile_content() + "\n" + router.get_full_output();
  EXPECT_THAT(router_out, ::testing::HasSubstr(GetParam().expected_error_msg));
}

INSTANTIATE_TEST_SUITE_P(
    OverwriteError, OverwriteErrorTest,
    ::testing::Values(
        OverwriteErrorTestParam{
            {"--logger.level", "DEBUGy"},
            "Configuration error: Log level 'debugy' is not valid."},
        OverwriteErrorTestParam{
            {"--routing:Main01.destinations", "127.0.0.1:3060",
             "--routing:Main01.bind_address", "0.0.0.0",
             "--routing:Main01.bind_port", "7777a"},
            "Configuration error: option bind_port in [routing:Main01] needs "
            "value between 1 and 65535 inclusive, was '7777a'"},
        OverwriteErrorTestParam{
            {"--metadata_cache:Main01.user", "mysql_router1_user",
             "--metadata_cache:Main01.router_id", "1a"},
            "Configuration error: option router_id in [metadata_cache:Main01] "
            "needs value between 0 and 4294967295 inclusive, was '1a'"},
        OverwriteErrorTestParam{
            {"--routing:Main01.destinations", "127.0.0.1:3060",
             "--routing:Main01.bind_address", "0.0.0.0",
             "--routing:Main01.routing_strategy", "first-available",
             "--routing:Main01.client_ssl_mode", "Yes"},
            "Configuration error: invalid value 'Yes' for option "
            "client_ssl_mode in [routing:Main01]. "
            "Allowed are: DISABLED,PREFERRED,REQUIRED,PASSTHROUGH."},
        OverwriteErrorTestParam{
            {"--http_server.port=-5"},
            "Configuration error: option port in [http_server] needs value "
            "between 0 and 65535 inclusive, was '-5'"},
        OverwriteErrorTestParam{
            {"--:test_rw.bind_port=6666"},
            "Error: invalid argument '--:test_rw.bind_port=6666'."},
        OverwriteErrorTestParam{
            {"--:test_rw.bind_port=6666"},
            "Error: invalid argument '--:test_rw.bind_port=6666'."},
        OverwriteErrorTestParam{
            {"--keepalive:a.b=c\nd"},
            {"Error: invalid value 'c\nd' for option '--keepalive:a.b'"},
        },
        OverwriteErrorTestParam{{"--routing:=6666"},
                                "Error: invalid argument '--routing:=6666'."},
        OverwriteErrorTestParam{{"--.para=value"},
                                "Error: invalid argument '--.para=value"},
        OverwriteErrorTestParam{{"--.:=a"},
                                "Error: invalid argument '--.:=a'."},
        OverwriteErrorTestParam{{"--:.=a"}, "Error: invalid argument '--:.=a"},
        OverwriteErrorTestParam{{"--DEFAULT.=Dz"},
                                "Error: invalid argument '--DEFAULT.=Dz"},
        OverwriteErrorTestParam{{"--a::::a=b"},
                                "Error: invalid argument '--a::::a=b"}));

class UnknownSectionNameTest
    : public RouterConfigOwerwriteTest,
      public ::testing::WithParamInterface<OverwriteErrorTestParam> {};

/* @test Using invalid(unknown) section for parameter overwrite should
 * give an error the same way it does if unknown section is added to the
 * configuration file */
TEST_P(UnknownSectionNameTest, UnknownSectionName) {
  const std::string keepalive_section = get_keepalive_section();
  const std::string conf_file =
      create_config_file(conf_dir.name(), keepalive_section);

  std::vector<std::string> cmdline{"-c", conf_file};

  cmdline.insert(cmdline.end(), GetParam().overwrite_params.begin(),
                 GetParam().overwrite_params.end());

  auto &router = launch_router(cmdline, EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_TRUE(router.expect_output(GetParam().expected_error_msg));
}

INSTANTIATE_TEST_SUITE_P(
    UnknownSectionName, UnknownSectionNameTest,
    ::testing::Values(
        OverwriteErrorTestParam{
            {"--new_Section:sub.test", "10"},
            "Loading plugin for config-section '[new_section:sub]' failed"},
        OverwriteErrorTestParam{
            {"--new_Section.test", "10"},
            "Loading plugin for config-section '[new_section]' failed"},
        OverwriteErrorTestParam{
            {"--DEFAULT2.test", "a"},
            "Loading plugin for config-section '[default2]' "
            "failed"},
        OverwriteErrorTestParam{{"--DEFAULT:test.option", "a"},
                                "Error: Invalid argument '--DEFAULT:test'. Key "
                                "not allowed on DEFAULT section"}));

class MetadataConfigTest : public RouterConfigOwerwriteTest,
                           public ::testing::WithParamInterface<std::string> {};

TEST_P(MetadataConfigTest, MetadataConfig) {
  auto md_server_port = port_pool_.get_next_available();
  auto md_server_http_port = port_pool_.get_next_available();
  auto router_port = port_pool_.get_next_available();
  const std::string json_metadata =
      get_data_dir().join("metadata_1_node_repeat_v2_gr.js").str();

  /*auto &metadata_server = */ launch_mysql_server_mock(
      json_metadata, md_server_port, EXIT_SUCCESS, false, md_server_http_port);

  const std::string metadata_cache_section =
      mysql_harness::ConfigBuilder::build_section(
          "metadata_cache:test",
          {
              {"cluster_type", "gr"},
              {"bootstrap_server_addresses",
               "mysql://localhost:" + std::to_string(md_server_port)},
              {"metadata_cluster", "test"},
          });

  const std::string routing_section =
      mysql_harness::ConfigBuilder::build_section(
          "routing:rw",
          {
              {"bind_port", std::to_string(router_port)},
              {"destinations", "metadata-cache://test/default?role=PRIMARY"},
              {"routing_strategy", "first-available"},
          });

  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, conf_dir.name());

  const std::string conf_file = create_config_file(
      conf_dir.name(), routing_section + metadata_cache_section,
      &default_section);

  std::vector<std::string> cmdline{"-c", conf_file};

  cmdline.push_back(GetParam());

  launch_router(cmdline, EXIT_SUCCESS, 5s);
}

INSTANTIATE_TEST_SUITE_P(
    MetadataConfig, MetadataConfigTest,
    ::testing::Values("--metadata_cache:test.user=mysql_router1_user",
                      "--metadata_cache:test.User=mysql_router1_user",
                      "--metadata_cache:Test.User=mysql_router1_user"));

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
