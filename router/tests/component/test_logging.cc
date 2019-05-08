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
#include "mysql/harness/logging/logging.h"
#include "mysql_session.h"
#include "mysqlrouter/utils.h"
#include "random_generator.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"

#ifndef _WIN32
#include <signal.h>
#endif

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

using mysql_harness::logging::LogLevel;
using testing::HasSubstr;
using testing::Not;
using testing::StartsWith;
using namespace std::chrono_literals;
Path g_origin_path;

class RouterLoggingTest : public RouterComponentTest, public ::testing::Test {
 protected:
  virtual void SetUp() {
    set_origin(g_origin_path);
    RouterComponentTest::init();
  }

  TcpPortPool port_pool_;
};

/** @test This test verifies that fatal error messages thrown before switching
 * to logger specified in config file (before Loader::run() runs
 * logger_plugin.cc:init()) are properly logged to STDERR
 */
TEST_F(RouterLoggingTest, log_startup_failure_to_console) {
  auto conf_params = get_DEFAULT_defaults();
  // we want to log to the console
  conf_params["logging_folder"] = "";
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });
  const std::string conf_file =
      create_config_file(conf_dir, "[invalid]", &conf_params);

  // run the router and wait for it to exit
  auto router = launch_router({"-c", conf_file});
  EXPECT_EQ(router.wait_for_exit(), 1);

  // expect something like this to appear on STDERR
  // plugin 'invalid' failed to
  // load: ./plugin_output_directory/invalid.so: cannot open shared object
  // file: No such file or directory
  const std::string out = router.get_full_output();
  EXPECT_THAT(out.c_str(), HasSubstr("plugin 'invalid' failed to load"));
}

/** @test This test is similar to log_startup_failure_to_logfile(), but the
 * failure message is expected to be logged into a logfile
 */
TEST_F(RouterLoggingTest, log_startup_failure_to_logfile) {
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
      create_config_file(conf_dir, "[routing]", &params);

  // run the router and wait for it to exit
  auto router = launch_router({"-c", conf_file});
  EXPECT_EQ(router.wait_for_exit(), 1);

  // expect something like this to appear in log:
  // 2018-12-19 03:54:04 main ERROR [7f539f628780] Configuration error: option
  // destinations in [routing] is required
  auto matcher = [](const std::string &line) -> bool {
    return line.find(
               "Configuration error: option destinations in [routing] is "
               "required") != line.npos;
  };

  EXPECT_TRUE(find_in_file(logging_folder + "/mysqlrouter.log", matcher))
      << "log:" << get_router_log_output("mysqlrouter.log", logging_folder);
}

/** @test This test verifies that invalid logging_folder is properly handled and
 * appropriate message is printed on STDERR. Router tries to
 * mkdir(logging_folder) if it doesn't exist, then write its log inside of it.
 */
TEST_F(RouterLoggingTest, bad_logging_folder) {
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
    const std::string conf_file =
        create_config_file(conf_dir, "[keepalive]\n", &params);

    // run the router and wait for it to exit
    auto router = launch_router("-c " + conf_file);
    EXPECT_EQ(router.wait_for_exit(), 1);

    // expect something like this to appear on STDERR
    // Error: Error when creating dir '/bla': 13
    const std::string out = router.get_full_output();
    EXPECT_THAT(
        out.c_str(),
        HasSubstr("plugin 'logger' init failed: Error when creating dir '" +
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
    const std::string conf_file =
        create_config_file(conf_dir, "[keepalive]\n", &params);

    // run the router and wait for it to exit
    auto router = launch_router("-c " + conf_file);
    EXPECT_EQ(router.wait_for_exit(), 1);

    // expect something like this to appear on STDERR
    // Error: Cannot create file in directory //mysqlrouter.log: Permission
    // denied
    const std::string out = router.get_full_output();
#ifndef _WIN32
    EXPECT_THAT(
        out.c_str(),
        HasSubstr(
            "plugin 'logger' init failed: Cannot create file in directory " +
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
    const std::string conf_file =
        create_config_file(conf_dir, "[keepalive]\n", &params);

    // run the router and wait for it to exit
    auto router = launch_router("-c " + conf_file);
    EXPECT_EQ(router.wait_for_exit(), 1);

    // expect something like this to appear on STDERR
    // Error: Cannot create file in directory /etc/passwd/mysqlrouter.log: Not a
    // directory
    const std::string out = router.get_full_output();
#ifndef _WIN32
    EXPECT_THAT(out.c_str(), HasSubstr("Cannot create file in directory " +
                                       logging_dir + ": Not a directory\n"));
#else
    // on Windows emulate (wine) we get ENOTDIR
    // with native windows we get ENOENT

    EXPECT_THAT(
        out.c_str(),
        ::testing::AllOf(
            ::testing::HasSubstr("Cannot create file in directory " +
                                 logging_dir),
            ::testing::AnyOf(
                ::testing::EndsWith("Directory name invalid.\n\n"),
                ::testing::EndsWith(
                    "The system cannot find the path specified.\n\n"))));
#endif
  }
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
      ::testing::HasSubstr(
          "Error: Configuration error: Section 'logger' already exists"));
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
  // Error: Section 'logger' does not support key
  const std::string out = router.get_full_output();
  EXPECT_THAT(out.c_str(),
              HasSubstr("Error: Section 'logger' does not support keys"));
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
  // Configuration error: Log level 'unknown' is not valid. Valid values are:
  // debug, error, fatal, info, and warning
  const std::string out = router.get_full_output();
  EXPECT_THAT(
      out.c_str(),
      HasSubstr("Configuration error: Log level 'unknown' is not valid. Valid "
                "values are: debug, error, fatal, info, and warning"));
}

/**************************************************/
/* Tests for valid logger configurations          */
/**************************************************/

struct LoggingConfigOkParams {
  std::string logger_config;
  bool logging_folder_empty;

  LogLevel consolelog_expected_level;
  LogLevel filelog_expected_level;

  LoggingConfigOkParams(const std::string &logger_config_,
                        const bool logging_folder_empty_,
                        const LogLevel consolelog_expected_level_,
                        const LogLevel filelog_expected_level_)
      : logger_config(logger_config_),
        logging_folder_empty(logging_folder_empty_),
        consolelog_expected_level(consolelog_expected_level_),
        filelog_expected_level(filelog_expected_level_) {}
};

::std::ostream &operator<<(::std::ostream &os,
                           const LoggingConfigOkParams &ltp) {
  return os << "config=" << ltp.logger_config
            << ", logging_folder_empty=" << ltp.logging_folder_empty;
}

class RouterLoggingTestConfig
    : public RouterComponentTest,
      public ::testing::TestWithParam<LoggingConfigOkParams> {
 protected:
  virtual void SetUp() override {
    set_origin(g_origin_path);
    RouterComponentTest::init();
  }
};

/** @test This test verifies that a proper loggs are written to selected sinks
 * for various sinks/levels combinations.
 */
TEST_P(RouterLoggingTestConfig, LoggingTestConfig) {
  auto test_params = GetParam();

  const std::string tmp_dir = get_tmp_dir();
  TcpPortPool port_pool;
  const unsigned router_port = port_pool.get_next_available();
  const unsigned server_port = port_pool.get_next_available();
  std::shared_ptr<void> exit_guard1(nullptr,
                                    [&](void *) { purge_dir(tmp_dir); });

  // These are different level log entries that are expected to get logged after
  // the logger plugin has been initialized
  const std::string kDebugLogEntry = "plugin 'logger:' doesn't implement start";
  const std::string kInfoLogEntry = "[routing] started: listening on 127.0.0.1";
  const std::string kWarningLogEntry =
      "Can't connect to remote MySQL server for client";

  // to trigger the warning entry in the log
  const std::string kRoutingConfig =
      "[routing]\n"
      "bind_address=127.0.0.1:" +
      std::to_string(router_port) +
      "\n"
      "destinations=localhost:" +
      std::to_string(server_port) +
      "\n"
      "routing_strategy=round-robin\n";

  auto conf_params = get_DEFAULT_defaults();
  conf_params["logging_folder"] =
      test_params.logging_folder_empty ? "" : tmp_dir;

  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });
  const std::string conf_text =
      test_params.logger_config + "\n" + kRoutingConfig;

  const std::string conf_file =
      create_config_file(conf_dir, conf_text, &conf_params);

  auto router = launch_router("-c " + conf_file);

  bool ready = wait_for_port_ready(router_port, 1000);
  EXPECT_TRUE(ready) << router.get_full_output();

  // try to make a connection; this will fail but should generate a warning in
  // the logs
  mysqlrouter::MySQLSession client;
  try {
    client.connect("127.0.0.1", router_port, "username", "password", "", "");
  } catch (const std::exception &exc) {
    if (std::string(exc.what()).find("Error connecting to MySQL server") !=
        std::string::npos) {
      // that's what we expect
    } else
      throw;
  }

  const std::string console_log_txt = router.get_full_output();

  // check the console log if it contains what's expected
  if (test_params.consolelog_expected_level >= LogLevel::kDebug &&
      test_params.consolelog_expected_level != LogLevel::kNotSet) {
    EXPECT_THAT(console_log_txt, HasSubstr(kDebugLogEntry)) << "console:\n"
                                                            << console_log_txt;
  } else {
    EXPECT_THAT(console_log_txt, Not(HasSubstr(kDebugLogEntry)))
        << "console:\n"
        << console_log_txt;
  }

  if (test_params.consolelog_expected_level >= LogLevel::kInfo &&
      test_params.consolelog_expected_level != LogLevel::kNotSet) {
    EXPECT_THAT(console_log_txt, HasSubstr(kInfoLogEntry)) << "console:\n"
                                                           << console_log_txt;
  } else {
    EXPECT_THAT(console_log_txt, Not(HasSubstr(kInfoLogEntry)))
        << "console:\n"
        << console_log_txt;
  }

  if (test_params.consolelog_expected_level >= LogLevel::kWarning &&
      test_params.consolelog_expected_level != LogLevel::kNotSet) {
    EXPECT_THAT(console_log_txt, HasSubstr(kWarningLogEntry))
        << "console:\n"
        << console_log_txt;
  } else {
    EXPECT_THAT(console_log_txt, Not(HasSubstr(kWarningLogEntry)))
        << "console:\n"
        << console_log_txt;
  }

  // check the file log if it contains what's expected
  const std::string file_log_txt =
      get_router_log_output("mysqlrouter.log", tmp_dir);

  if (test_params.filelog_expected_level >= LogLevel::kDebug &&
      test_params.filelog_expected_level != LogLevel::kNotSet) {
    EXPECT_THAT(file_log_txt, HasSubstr(kDebugLogEntry))
        << "file:\n"
        << file_log_txt << "\nconsole:\n"
        << console_log_txt;
  } else {
    EXPECT_THAT(file_log_txt, Not(HasSubstr(kDebugLogEntry)))
        << "file:\n"
        << file_log_txt << "\nconsole:\n"
        << console_log_txt;
  }

  if (test_params.filelog_expected_level >= LogLevel::kInfo &&
      test_params.filelog_expected_level != LogLevel::kNotSet) {
    EXPECT_THAT(file_log_txt, HasSubstr(kInfoLogEntry))
        << "file:\n"
        << file_log_txt << "\nconsole:\n"
        << console_log_txt;
  } else {
    EXPECT_THAT(file_log_txt, Not(HasSubstr(kInfoLogEntry)))
        << "file:\n"
        << file_log_txt << "\nconsole:\n"
        << console_log_txt;
  }

  if (test_params.filelog_expected_level >= LogLevel::kWarning &&
      test_params.filelog_expected_level != LogLevel::kNotSet) {
    EXPECT_THAT(file_log_txt, HasSubstr(kWarningLogEntry))
        << "file:\n"
        << file_log_txt << "\nconsole:\n"
        << console_log_txt;
  } else {
    EXPECT_THAT(file_log_txt, Not(HasSubstr(kWarningLogEntry)))
        << "file:\n"
        << file_log_txt << "\nconsole:\n"
        << console_log_txt;
  }
}

INSTANTIATE_TEST_CASE_P(
    LoggingConfigTest, RouterLoggingTestConfig,
    ::testing::Values(
        // no logger section, no sinks sections
        // logging_folder not empty so we are expected to log to the file
        // with a warning level so info and debug logs will not be there
        /*0*/ LoggingConfigOkParams(
            "",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kWarning),

        // no logger section, no sinks sections
        // logging_folder empty so we are expected to log to the console
        // with a warning level so info and debug logs will not be there
        /*1*/
        LoggingConfigOkParams(
            "",
            /* logging_folder_empty = */ true,
            /* consolelog_expected_level =  */ LogLevel::kWarning,
            /* filelog_expected_level =  */ LogLevel::kNotSet),

        // logger section, no sinks sections
        // logging_folder not empty so we are expected to log to the file
        // with a warning level as level is not redefined in the [logger]
        // section
        /*2*/
        LoggingConfigOkParams(
            "[logger]",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kWarning),

        // logger section, no sinks sections
        // logging_folder not empty so we are expected to log to the file
        // with a level defined in the logger section
        /*3*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=info\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kInfo),

        // logger section, no sinks sections; logging_folder is empty so we are
        // expected to log to the console with a level defined in the logger
        // section
        /*4*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=info\n",
            /* logging_folder_empty = */ true,
            /* consolelog_expected_level =  */ LogLevel::kInfo,
            /* filelog_expected_level =  */ LogLevel::kNotSet),

        // consolelog configured as a sink; it does not have its section in the
        // config but that is not an error; even though the logging folder is
        // not empty, we still don't log to the file as sinks= setting wants use
        // the console
        /*5*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=debug\n"
            "sinks=consolelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kNotSet),

        // 2 sinks have sections but consolelog is not defined as a sink in the
        // [logger] section so there should be no logging to the console (after
        // [logger] is initialised; prior to that all is logged to the console
        // by default)
        /*6*/
        LoggingConfigOkParams(
            "[logger]\n"
            "sinks=filelog\n"
            "level=debug\n"
            "[filelog]\n"
            "[consolelog]\n"
            "level=debug\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kDebug),

        // 2 sinks, both should inherit log level from [logger] section (which
        // is debug)
        /*7*/
        LoggingConfigOkParams(
            "[logger]\n"
            "sinks=filelog,consolelog\n"
            "level=debug\n"
            "[filelog]\n"
            "[consolelog]\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug),

        // 2 sinks, both should inherit log level from [logger] section (which
        // is info); debug logs are not expected for both sinks
        /*8*/
        LoggingConfigOkParams(
            "[logger]\n"
            "sinks=filelog,consolelog\n"
            "level=info\n"
            "[filelog]\n"
            "[consolelog]\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kInfo,
            /* filelog_expected_level =  */ LogLevel::kInfo),

        // 2 sinks, both should inherit log level from [logger] section (which
        // is warning); neither debug not info logs are not expected for both
        // sinks
        /*9*/
        LoggingConfigOkParams(
            "[logger]\n"
            "sinks=filelog,consolelog\n"
            "level=warning\n"
            "[filelog]\n"
            "[consolelog]\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kWarning,
            /* filelog_expected_level =  */ LogLevel::kWarning),

        // 2 sinks, one overwrites the default log level, the other inherits
        // default from [logger] section
        /*10*/
        LoggingConfigOkParams(
            "[logger]\n"
            "sinks=filelog,consolelog\n"
            "level=info\n"
            "[filelog]\n"
            "level=debug\n"
            "[consolelog]\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kInfo,
            /* filelog_expected_level =  */ LogLevel::kDebug),

        // 2 sinks, each defines its own custom log level that overwrites the
        // default from [logger] section
        /*11*/
        LoggingConfigOkParams(
            "[logger]\n"
            "sinks=filelog,consolelog\n"
            "level=info\n"
            "[filelog]\n"
            "level=debug\n"
            "[consolelog]\n"
            "level=warning\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kWarning,
            /* filelog_expected_level =  */ LogLevel::kDebug),

        // 2 sinks, each defines its own custom log level that overwrites the
        // default from [logger] section
        /*12*/
        LoggingConfigOkParams(
            "[logger]\n"
            "sinks=filelog,consolelog\n"
            "level=warning\n"
            "[filelog]\n"
            "level=info\n"
            "[consolelog]\n"
            "level=warning\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kWarning,
            /* filelog_expected_level =  */ LogLevel::kInfo),

        // 2 sinks, each defines its own custom log level (that is more strict)
        // that overwrites the default from [logger] section
        /*13*/
        LoggingConfigOkParams(
            "[logger]\n"
            "sinks=filelog,consolelog\n"
            "level=debug\n"
            "[filelog]\n"
            "level=info\n"
            "[consolelog]\n"
            "level=warning\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kWarning,
            /* filelog_expected_level =  */ LogLevel::kInfo),

        // 2 sinks,no level in the [logger] section and no level in the sinks
        // sections; default log level should be used (which is warning)
        /*14*/
        LoggingConfigOkParams(
            "[logger]\n"
            "sinks=filelog,consolelog\n"
            "[filelog]\n"
            "[consolelog]\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kWarning,
            /* filelog_expected_level =  */ LogLevel::kWarning),

        // 2 sinks, level in the [logger] section is warning; it should be
        // used by the sinks as they don't redefine it in their sections
        /*15*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=warning\n"
            "sinks=filelog,consolelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kWarning,
            /* filelog_expected_level =  */ LogLevel::kWarning),

        // 2 sinks, level in the [logger] section is error; it should be used
        // by the sinks as they don't redefine it in their sections
        /*16*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=error\n"
            "sinks=filelog,consolelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kError,
            /* filelog_expected_level =  */ LogLevel::kError),

        // 2 sinks, no level in the [logger] section, each defines it's own
        // level
        /*17*/
        LoggingConfigOkParams(
            "[logger]\n"
            "sinks=filelog,consolelog\n"
            "[filelog]\n"
            "level=error\n"
            "[consolelog]\n"
            "level=debug\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kError),

        // 2 sinks, no level in the [logger] section, one defines it's own
        // level, the other expected to go with default (warning)
        /*18*/
        LoggingConfigOkParams(
            "[logger]\n"
            "sinks=filelog,consolelog\n"
            "[filelog]\n"
            "level=error\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kWarning,
            /* filelog_expected_level =  */ LogLevel::kError)));

/**************************************************/
/* Tests for logger configuration errors          */
/**************************************************/

struct LoggingConfigErrorParams {
  std::string logger_config;
  bool logging_folder_empty;

  std::string expected_error;

  LoggingConfigErrorParams(const std::string &logger_config_,
                           const bool logging_folder_empty_,
                           const std::string &expected_error_)
      : logger_config(logger_config_),
        logging_folder_empty(logging_folder_empty_),
        expected_error(expected_error_) {}
};

::std::ostream &operator<<(::std::ostream &os,
                           const LoggingConfigErrorParams &ltp) {
  return os << "config=" << ltp.logger_config
            << ", logging_folder_empty=" << ltp.logging_folder_empty;
}

class RouterLoggingConfigError
    : public RouterComponentTest,
      public ::testing::TestWithParam<LoggingConfigErrorParams> {
 protected:
  virtual void SetUp() override {
    set_origin(g_origin_path);
    RouterComponentTest::init();
  }
};

/** @test This test verifies that a proper error gets printed on the console for
 * a particular logging configuration
 */
TEST_P(RouterLoggingConfigError, LoggingConfigError) {
  auto test_params = GetParam();

  const std::string tmp_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard1(nullptr,
                                    [&](void *) { purge_dir(tmp_dir); });

  auto conf_params = get_DEFAULT_defaults();
  conf_params["logging_folder"] =
      test_params.logging_folder_empty ? "" : tmp_dir;

  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });
  const std::string conf_text = "[keepalive]\n" + test_params.logger_config;

  const std::string conf_file =
      create_config_file(conf_dir, conf_text, &conf_params);

  auto router = launch_router("-c " + conf_file);

  EXPECT_EQ(1, router.wait_for_exit());

  // the error happens during the logger initialization so we expect the message
  // on the console which is the default sink until we switch to the
  // configuration from the config file
  const std::string console_log_txt = router.get_full_output();

  EXPECT_THAT(console_log_txt, HasSubstr(test_params.expected_error))
      << "\nconsole:\n"
      << console_log_txt;
}

INSTANTIATE_TEST_CASE_P(
    LoggingConfigError, RouterLoggingConfigError,
    ::testing::Values(
        // Unknown sink name in the [logger] section
        /*0*/ LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=unknown\n"
            "level=debug\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "Configuration error: Unsupported logger sink type: 'unknown'"),

        // Empty sinks option
        /*1*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "plugin 'logger' init failed: sinks option does not contain any "
            "valid sink name, was ''"),

        // Empty sinks list
        /*2*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=,\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "plugin 'logger' init failed: Unsupported logger sink type: ''"),

        // Leading comma on a sinks list
        /*3*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=,consolelog\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "plugin 'logger' init failed: Unsupported logger sink type: ''"),

        // Terminating comma on a sinks list
        /*4*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=consolelog,\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "plugin 'logger' init failed: Unsupported logger sink type: ''"),

        // Two commas separating sinks
        /*5*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=consolelog,,filelog\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "plugin 'logger' init failed: Unsupported logger sink type: ''"),

        // Empty space as a sink name
        /*6*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks= \n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "plugin 'logger' init failed: sinks option does not contain any "
            "valid sink name, was ''"),

        // Invalid log level in the [logger] section
        /*7*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=consolelog\n"
            "level=invalid\n"
            "[consolelog]\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "Configuration error: Log level 'invalid' is not valid. Valid "
            "values are: debug, error, fatal, info, and warning"),

        // Invalid log level in the sink section
        /*8*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=consolelog\n"
            "[consolelog]\n"
            "level=invalid\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "Configuration error: Log level 'invalid' is not valid. Valid "
            "values are: debug, error, fatal, info, and warning"),

        // Both level and sinks valuse invalid in the [logger] section
        /*9*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=invalid\n"
            "level=invalid\n"
            "[consolelog]\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "Configuration error: Log level 'invalid' is not valid. Valid "
            "values are: debug, error, fatal, info, and warning"),

        // Logging folder is empty but we request filelog as sink
        /*10*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=filelog\n",
            /* logging_folder_empty = */ true,
            /* expected_error =  */
            "plugin 'logger' init failed: filelog sink configured but the "
            "logging_folder is empty")));

#ifndef _WIN32
INSTANTIATE_TEST_CASE_P(
    LoggingConfigErrorUnix, RouterLoggingConfigError,
    ::testing::Values(
        // We can't reliably check if the syslog logging is working with a
        // component test as this is too operating system intrusive and we are
        // supposed to run on pb2 environment. Let's at least check that this
        // sink type is supported
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=syslog\n"
            "[syslog]\n"
            "level=invalid\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "Configuration error: Log level 'invalid' is not valid. Valid "
            "values are: debug, error, fatal, info, and warning"),

        // Let's also check that the eventlog is NOT supported
        LoggingConfigErrorParams("[logger]\n"
                                 "sinks=eventlog\n"
                                 "[eventlog]\n"
                                 "level=invalid\n",
                                 /* logging_folder_empty = */ false,
                                 /* expected_error =  */
                                 "plugin 'eventlog' failed to load")));
#else
INSTANTIATE_TEST_CASE_P(
    LoggingConfigErrorWindows, RouterLoggingConfigError,
    ::testing::Values(
        // We can't reliably check if the eventlog logging is working with a
        // component test as this is too operating system intrusive and also
        // requires admin priviledges to setup and we are supposed to run on pb2
        // environment. Let's at least check that this sink type is supported
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=eventlog\n"
            "[eventlog]\n"
            "level=invalid\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "Configuration error: Log level 'invalid' is not valid. Valid "
            "values are: debug, error, fatal, info, and warning"),

        // Let's also check that the syslog is NOT supported
        LoggingConfigErrorParams("[logger]\n"
                                 "sinks=syslog\n"
                                 "[syslog]\n"
                                 "level=invalid\n",
                                 /* logging_folder_empty = */ false,
                                 /* expected_error =  */
                                 "plugin 'syslog' failed to load")));
#endif

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
  EXPECT_THAT(router.get_full_output(),
              testing::HasSubstr("After this MySQL Router has been started "
                                 "with the generated configuration"));

  EXPECT_THAT(router.get_full_output(),
              testing::HasSubstr("MySQL Classic protocol"));

  EXPECT_THAT(router.get_full_output(), testing::HasSubstr("MySQL X protocol"));
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

    cluster_nodes_ports = {port_pool_.get_next_available(),
                           port_pool_.get_next_available(),
                           port_pool_.get_next_available()};
    router_port = port_pool_.get_next_available();
    metadata_cache_section = get_metadata_cache_section(cluster_nodes_ports);
    routing_section =
        get_metadata_cache_routing_section("PRIMARY", "round-robin", "");

    write_json_file(get_data_dir());
  }

  void TearDown() override { purge_dir(temp_test_dir); }

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

  std::string get_metadata_cache_section(std::vector<uint16_t> ports) {
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
           "ttl=0.1\n\n";
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

  std::string init_keyring_and_config_file(const std::string &conf_dir,
                                           bool log_to_console = false) {
    auto default_section = get_DEFAULT_defaults();
    init_keyring(default_section, temp_test_dir);
    default_section["logging_folder"] =
        log_to_console ? "" : get_logging_dir().str();
    return create_config_file(
        conf_dir,
        "[logger]\nlevel = DEBUG\n" + metadata_cache_section + routing_section,
        &default_section);
  }

  std::string json_primary_node_template_;
  std::string json_primary_node_;
  std::string temp_test_dir;
  std::vector<uint16_t> cluster_nodes_ports;
  uint16_t router_port;
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
  bool router_ready = wait_for_port_ready(router_port, 10000);
  EXPECT_TRUE(router_ready) << router.get_full_output();

  // expect something like this to appear on STDERR
  // 2017-12-21 17:22:35 metadata_cache ERROR [7ff0bb001700] Failed connecting
  // with any of the 3 metadata servers
  auto matcher = [](const std::string &line) -> bool {
    return line.find("metadata_cache ERROR") != line.npos &&
           line.find(
               "Failed fetching metadata from any of the 3 metadata servers") !=
               line.npos;
  };

  auto log_file = get_logging_dir();
  log_file.append("mysqlrouter.log");
  EXPECT_TRUE(
      find_in_file(log_file.str(), matcher, std::chrono::milliseconds(5000)))
      << get_router_log_output();
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

  EXPECT_TRUE(find_in_file(get_logging_dir().str() + "/mysqlrouter.log",
                           info_matcher, std::chrono::milliseconds(10000)))
      << get_router_log_output();

  auto warning_matcher = [](const std::string &line) -> bool {
    return line.find("metadata_cache WARNING") != line.npos &&
           line.find(
               "While updating metadata, could not establish a connection to "
               "replicaset") != line.npos;
  };
  EXPECT_TRUE(find_in_file(get_logging_dir().str() + "/mysqlrouter.log",
                           warning_matcher, std::chrono::milliseconds(10000)))
      << get_router_log_output();
}

#ifndef _WIN32
/**
 * @test Checks that the logs rotation works (meaning Router will recreate
 * it's log file when it was moved and HUP singnal was sent to the Router).
 */
TEST_F(MetadataCacheLoggingTest, log_rotation_by_HUP_signal) {
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });

  // launch the router with metadata-cache configuration
  auto router = RouterComponentTest::launch_router(
      "-c " + init_keyring_and_config_file(conf_dir));
  bool router_ready = wait_for_port_ready(router_port, 10000);
  EXPECT_TRUE(router_ready) << router.get_full_output();

  std::this_thread::sleep_for(500ms);

  auto log_file = get_logging_dir();
  log_file.append("mysqlrouter.log");

  EXPECT_TRUE(log_file.exists());

  // now let's simulate what logrotate script does
  // move the log_file appending '.1' to its name
  auto log_file_1 = get_logging_dir();
  log_file_1.append("mysqlrouter.log.1");
  mysqlrouter::rename_file(log_file.str(), log_file_1.str());
  const auto pid = static_cast<pid_t>(router.get_pid());
  ::kill(pid, SIGHUP);

  // let's wait  until something new gets logged (metadata cache TTL has
  // expired), to be sure the default file that we moved is back.
  // Now both old and new files should exist
  unsigned retries = 10;
  const auto kSleep = 100ms;
  do {
    std::this_thread::sleep_for(kSleep);
  } while ((--retries > 0) && !log_file.exists());

  EXPECT_TRUE(log_file.exists()) << get_router_log_output();
  EXPECT_TRUE(log_file_1.exists());
}

/**
 * @test Checks that the Router continues to log to the file when the
 * SIGHUP gets sent to it and no file replacement is done.
 */
TEST_F(MetadataCacheLoggingTest, log_rotation_by_HUP_signal_no_file_move) {
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });

  // launch the router with metadata-cache configuration
  auto router = RouterComponentTest::launch_router(
      "-c " + init_keyring_and_config_file(conf_dir));
  bool router_ready = wait_for_port_ready(router_port, 10000);
  EXPECT_TRUE(router_ready) << router.get_full_output();

  std::this_thread::sleep_for(500ms);

  auto log_file = get_logging_dir();
  log_file.append("mysqlrouter.log");

  EXPECT_TRUE(log_file.exists());

  // grab the current log content
  const std::string log_content = get_router_log_output();

  // send the log-rotate signal
  const auto pid = static_cast<pid_t>(router.get_pid());
  ::kill(pid, SIGHUP);

  // wait until something new gets logged;
  std::string log_content_2;
  unsigned step = 0;
  do {
    std::this_thread::sleep_for(100ms);
    log_content_2 = get_router_log_output();
  } while ((log_content_2 == log_content) && (step++ < 20));

  // The logfile should still exist
  EXPECT_TRUE(log_file.exists());
  // It should still contain what was there before and more (Router should keep
  // logging)
  EXPECT_THAT(log_content_2, StartsWith(log_content));
  EXPECT_STRNE(log_content_2.c_str(), log_content.c_str());
}

/**
 * @test Checks that the logs Router continues to log to the file when the
 * SIGHUP gets sent to it and no file replacement is done.
 */
TEST_F(MetadataCacheLoggingTest, log_rotation_when_router_restarts) {
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });

  // launch the router with metadata-cache configuration
  auto router = RouterComponentTest::launch_router(
      "-c " + init_keyring_and_config_file(conf_dir));
  bool router_ready = wait_for_port_ready(router_port, 10000);
  EXPECT_TRUE(router_ready) << router.get_full_output();

  std::this_thread::sleep_for(500ms);

  auto log_file = get_logging_dir();
  log_file.append("mysqlrouter.log");

  EXPECT_TRUE(log_file.exists());

  // now stop the router
  int res = router.kill();
  EXPECT_EQ(0, res) << router.get_full_output();

  // move the log_file appending '.1' to its name
  auto log_file_1 = get_logging_dir();
  log_file_1.append("mysqlrouter.log.1");
  mysqlrouter::rename_file(log_file.str(), log_file_1.str());

  // make the new file read-only
  chmod(log_file_1.c_str(), S_IRUSR);

  // start the router again and check that the new log file got created
  auto router2 = RouterComponentTest::launch_router(
      "-c " + init_keyring_and_config_file(conf_dir));
  router_ready = wait_for_port_ready(router_port, 10000);
  EXPECT_TRUE(router_ready) << router.get_full_output();
  std::this_thread::sleep_for(500ms);
  EXPECT_TRUE(log_file.exists());
}

/**
 * @test Checks that the logs Router continues to log to the file when the
 * SIGHUP gets sent to it and no file replacement is done.
 */
TEST_F(MetadataCacheLoggingTest, log_rotation_read_only) {
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });

  // launch the router with metadata-cache configuration
  auto router = RouterComponentTest::launch_router(
      "-c " + init_keyring_and_config_file(conf_dir));
  bool router_ready = wait_for_port_ready(router_port, 10000);
  EXPECT_TRUE(router_ready) << router.get_full_output();

  auto log_file = get_logging_dir();
  log_file.append("mysqlrouter.log");

  unsigned retries = 5;
  const auto kSleep = 100ms;
  do {
    std::this_thread::sleep_for(kSleep);
  } while ((--retries > 0) && !log_file.exists());

  EXPECT_TRUE(log_file.exists());

  // move the log_file appending '.1' to its name
  auto log_file_1 = get_logging_dir();
  log_file_1.append("mysqlrouter.log.1");
  mysqlrouter::rename_file(log_file.str(), log_file_1.str());

  // "manually" recreate the log file and make it read only
  {
    std::ofstream logf(log_file.str());
    EXPECT_TRUE(logf.good());
  }
  chmod(log_file.c_str(), S_IRUSR);

  // send the log-rotate signal
  const auto pid = static_cast<pid_t>(router.get_pid());
  ::kill(pid, SIGHUP);

  // we expect the router to exit,
  // as the logfile is no longer usable it will fallback to logging to the
  // stderr
  EXPECT_EQ(router.wait_for_exit(), 1) << router.get_full_output();
  EXPECT_THAT(router.get_full_output(),
              HasSubstr("File exists, but cannot open for writing"));
  EXPECT_THAT(router.get_full_output(), HasSubstr("Unloading all plugins."));
}

/**
 * @test Checks that the logs rotation does not cause any crash in case of
 * not logging to the file (logging_foler empty == logging to the std:cerr)
 */
TEST_F(MetadataCacheLoggingTest, log_rotation_stdout) {
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });

  // launch the router with metadata-cache configuration
  auto router = RouterComponentTest::launch_router(
      "-c " + init_keyring_and_config_file(conf_dir, /*log_to_console=*/true));
  bool router_ready = wait_for_port_ready(router_port, 10000);
  EXPECT_TRUE(router_ready) << router.get_full_output();

  std::this_thread::sleep_for(200ms);
  const auto pid = static_cast<pid_t>(router.get_pid());
  ::kill(pid, SIGHUP);
  std::this_thread::sleep_for(200ms);
}

#endif

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
