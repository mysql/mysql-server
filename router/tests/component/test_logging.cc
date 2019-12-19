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

#include "dim.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
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
using mysql_harness::logging::LogTimestampPrecision;
using testing::HasSubstr;
using testing::Not;
using testing::StartsWith;
using namespace std::chrono_literals;

class RouterLoggingTest : public RouterComponentTest {
 protected:
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
  TempDirectory conf_dir("conf");
  const std::string conf_file =
      create_config_file(conf_dir.name(), "[invalid]", &conf_params);

  // run the router and wait for it to exit
  auto &router = launch_router({"-c", conf_file}, 1);
  check_exit_code(router, EXIT_FAILURE);

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
  TempDirectory logging_folder;

  // create config with logging_folder set to that directory
  std::map<std::string, std::string> params = get_DEFAULT_defaults();
  params.at("logging_folder") = logging_folder.name();
  TempDirectory conf_dir("conf");
  const std::string conf_file =
      create_config_file(conf_dir.name(), "[routing]", &params);

  // run the router and wait for it to exit
  auto &router = launch_router({"-c", conf_file}, 1);
  check_exit_code(router, EXIT_FAILURE);

  // expect something like this to appear in log:
  // 2018-12-19 03:54:04 main ERROR [7f539f628780] Configuration error: option
  // destinations in [routing] is required
  auto matcher = [](const std::string &line) -> bool {
    return line.find(
               "Configuration error: option destinations in [routing] is "
               "required") != line.npos;
  };

  EXPECT_TRUE(find_in_file(logging_folder.name() + "/mysqlrouter.log", matcher))
      << "log:"
      << router.get_full_logfile("mysqlrouter.log", logging_folder.name());
}

/** @test This test verifies that invalid logging_folder is properly handled and
 * appropriate message is printed on STDERR. Router tries to
 * mkdir(logging_folder) if it doesn't exist, then write its log inside of it.
 */
TEST_F(RouterLoggingTest, bad_logging_folder) {
  // create tmp dir to contain our tests
  TempDirectory tmp_dir;

// unfortunately it's not (reasonably) possible to make folders read-only on
// Windows, therefore we can run the following 2 tests only on Unix
// https://support.microsoft.com/en-us/help/326549/you-cannot-view-or-change-the-read-only-or-the-system-attributes-of-fo
#ifndef _WIN32

  // make tmp dir read-only
  chmod(tmp_dir.name().c_str(),
        S_IRUSR | S_IXUSR);  // r-x for the user (aka 500)

  // logging_folder doesn't exist and can't be created
  {
    const std::string logging_dir = tmp_dir.name() + "/some_dir";

    // create Router config
    std::map<std::string, std::string> params = get_DEFAULT_defaults();
    params.at("logging_folder") = logging_dir;
    TempDirectory conf_dir("conf");
    const std::string conf_file =
        create_config_file(conf_dir.name(), "[keepalive]\n", &params);

    // run the router and wait for it to exit
    auto &router = launch_router({"-c", conf_file}, 1);
    check_exit_code(router, EXIT_FAILURE);

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
    const std::string logging_dir = tmp_dir.name();

    // create Router config
    std::map<std::string, std::string> params = get_DEFAULT_defaults();
    params.at("logging_folder") = logging_dir;
    TempDirectory conf_dir("conf");
    const std::string conf_file =
        create_config_file(conf_dir.name(), "[keepalive]\n", &params);

    // run the router and wait for it to exit
    auto &router = launch_router({"-c", conf_file}, 1);
    check_exit_code(router, EXIT_FAILURE);

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
  chmod(tmp_dir.name().c_str(),
        S_IRUSR | S_IWUSR | S_IXUSR);  // rwx for the user (aka 700)

#endif  // #ifndef _WIN32

  // logging_folder is really a file
  {
    const std::string logging_dir = tmp_dir.name() + "/some_file";

    // create that file
    {
      std::ofstream some_file(logging_dir);
      EXPECT_TRUE(some_file.good());
    }

    // create Router config
    std::map<std::string, std::string> params = get_DEFAULT_defaults();
    params.at("logging_folder") = logging_dir;
    TempDirectory conf_dir("conf");
    const std::string conf_file =
        create_config_file(conf_dir.name(), "[keepalive]\n", &params);

    // run the router and wait for it to exit
    auto &router = launch_router({"-c", conf_file}, 1);
    check_exit_code(router, EXIT_FAILURE);

    // expect something like this to appear on STDERR
    // Error: Cannot create file in directory /etc/passwd/mysqlrouter.log: Not a
    // directory
    const std::string out = router.get_full_output();
    const std::string prefix("Cannot create file in directory " + logging_dir +
                             ": ");
#ifndef _WIN32
    EXPECT_THAT(out.c_str(), HasSubstr(prefix + "Not a directory\n"));
#else
    // on Windows emulate (wine) we get ENOTDIR
    // with native windows we get ENOENT

    EXPECT_THAT(
        out.c_str(),
        ::testing::AnyOf(
            ::testing::HasSubstr(prefix + "Directory name invalid.\n"),
            ::testing::HasSubstr(
                prefix + "The system cannot find the path specified.\n")));
#endif
  }
}

TEST_F(RouterLoggingTest, multiple_logger_sections) {
  // This test verifies that multiple [logger] sections are handled properly.
  // Router should report the error on STDERR and exit

  auto conf_params = get_DEFAULT_defaults();
  // we want to log to the console
  conf_params["logging_folder"] = "";
  TempDirectory conf_dir("conf");
  const std::string conf_file =
      create_config_file(conf_dir.name(), "[logger]\n[logger]\n", &conf_params);

  // run the router and wait for it to exit
  auto &router = launch_router({"-c", conf_file}, 1);
  check_exit_code(router, EXIT_FAILURE);

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
  TempDirectory conf_dir("conf");
  const std::string conf_file =
      create_config_file(conf_dir.name(), "[logger:some_key]\n", &conf_params);

  // run the router and wait for it to exit
  auto &router = launch_router({"-c", conf_file}, 1);
  check_exit_code(router, EXIT_FAILURE);

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
  TempDirectory conf_dir("conf");
  const std::string conf_file = create_config_file(
      conf_dir.name(), "[logger]\nlevel = UNKNOWN\n", &conf_params);

  // run the router and wait for it to exit
  auto &router = launch_router({"-c", conf_file}, 1);
  check_exit_code(router, EXIT_FAILURE);

  // expect something like this to appear on STDERR
  // Configuration error: Log level 'unknown' is not valid. Valid values are:
  // debug, error, fatal, info, note, system, and warning
  const std::string out = router.get_full_output();
  EXPECT_THAT(
      out.c_str(),
      HasSubstr(
          "Configuration error: Log level 'unknown' is not valid. Valid "
          "values are: debug, error, fatal, info, note, system, and warning"));
}

/**************************************************/
/* Tests for valid logger configurations          */
/**************************************************/

struct LoggingConfigOkParams {
  std::string logger_config;
  bool logging_folder_empty;

  LogLevel consolelog_expected_level;
  LogLevel filelog_expected_level;

  LogTimestampPrecision consolelog_expected_timestamp_precision;
  LogTimestampPrecision filelog_expected_timestamp_precision;

  LoggingConfigOkParams(const std::string &logger_config_,
                        const bool logging_folder_empty_,
                        const LogLevel consolelog_expected_level_,
                        const LogLevel filelog_expected_level_)
      : logger_config(logger_config_),
        logging_folder_empty(logging_folder_empty_),
        consolelog_expected_level(consolelog_expected_level_),
        filelog_expected_level(filelog_expected_level_),
        consolelog_expected_timestamp_precision(LogTimestampPrecision::kNotSet),
        filelog_expected_timestamp_precision(LogTimestampPrecision::kNotSet) {}

  LoggingConfigOkParams(
      const std::string &logger_config_, const bool logging_folder_empty_,
      const LogLevel consolelog_expected_level_,
      const LogLevel filelog_expected_level_,
      const LogTimestampPrecision consolelog_expected_timestamp_precision_,
      const LogTimestampPrecision filelog_expected_timestamp_precision_)
      : logger_config(logger_config_),
        logging_folder_empty(logging_folder_empty_),
        consolelog_expected_level(consolelog_expected_level_),
        filelog_expected_level(filelog_expected_level_),
        consolelog_expected_timestamp_precision(
            consolelog_expected_timestamp_precision_),
        filelog_expected_timestamp_precision(
            filelog_expected_timestamp_precision_) {}
};

::std::ostream &operator<<(::std::ostream &os,
                           const LoggingConfigOkParams &ltp) {
  return os << "config=" << ltp.logger_config
            << ", logging_folder_empty=" << ltp.logging_folder_empty;
}

class RouterLoggingTestConfig
    : public RouterComponentTest,
      public ::testing::WithParamInterface<LoggingConfigOkParams> {};

/** @test This test verifies that a proper loggs are written to selected sinks
 * for various sinks/levels combinations.
 */
TEST_P(RouterLoggingTestConfig, LoggingTestConfig) {
  auto test_params = GetParam();

  TempDirectory tmp_dir;
  TcpPortPool port_pool;
  const auto router_port = port_pool.get_next_available();
  const auto server_port = port_pool.get_next_available();

  // These are different level log entries that are expected to get logged after
  // the logger plugin has been initialized
  const std::string kDebugLogEntry = "plugin 'logger:' doesn't implement start";
  const std::string kInfoLogEntry = "[routing] started: listening on 127.0.0.1";
  const std::string kWarningLogEntry =
      "Can't connect to remote MySQL server for client";
  // System/Note does not produce unique output today
  const std::string kNoteLogEntry = "";
  const std::string kSystemLogEntry = "";

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
      test_params.logging_folder_empty ? "" : tmp_dir.name();

  TempDirectory conf_dir("conf");
  const std::string conf_text =
      test_params.logger_config + "\n" + kRoutingConfig;

  const std::string conf_file =
      create_config_file(conf_dir.name(), conf_text, &conf_params);

  auto &router = launch_router({"-c", conf_file});

  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port));

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

  if (test_params.consolelog_expected_level >= LogLevel::kNote &&
      test_params.consolelog_expected_level != LogLevel::kNotSet) {
    // No NOTE output from Router today, so check that we see info
    EXPECT_THAT(console_log_txt, HasSubstr(kInfoLogEntry)) << "console:\n"
                                                           << console_log_txt;
  } else {
    // No NOTE output from Router today, so disable until Router does
#if 0
    EXPECT_THAT(console_log_txt, Not(HasSubstr(kInfoLogEntry)))
        << "console:\n"
        << console_log_txt;
#endif
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

  if (test_params.consolelog_expected_level >= LogLevel::kSystem &&
      test_params.consolelog_expected_level != LogLevel::kNotSet) {
    // No SYSTEM output from Router today, so disable until Router does
#if 0
    EXPECT_THAT(console_log_txt, HasSubstr(kSystemLogEntry)) << "console:\n"
                                                             << console_log_txt;
#endif
  } else {
    // No SYSTEM output from Router today, so disable until Router does
#if 0
    EXPECT_THAT(console_log_txt, Not(HasSubstr(kSystemLogEntry)))
        << "console:\n"
        << console_log_txt;
#endif
  }

  // check the file log if it contains what's expected
  const std::string file_log_txt =
      router.get_full_logfile("mysqlrouter.log", tmp_dir.name());

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

  if (test_params.filelog_expected_level >= LogLevel::kNote &&
      test_params.filelog_expected_level != LogLevel::kNotSet) {
    // No NOTE output from Router today, so check that we see info
    EXPECT_THAT(file_log_txt, HasSubstr(kInfoLogEntry))
        << "file:\n"
        << file_log_txt << "\nconsole:\n"
        << console_log_txt;
  } else {
    // No NOTE output from Router today, so disable until Router does
#if 0
    EXPECT_THAT(file_log_txt, Not(HasSubstr(kNoteLogEntry)))
        << "file:\n"
        << file_log_txt << "\nconsole:\n"
        << console_log_txt;
#endif
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

  if (test_params.filelog_expected_level >= LogLevel::kSystem &&
      test_params.filelog_expected_level != LogLevel::kNotSet) {
    // No SYSTEM output from Router today, so disable until Router does
#if 0
    EXPECT_THAT(file_log_txt, HasSubstr(kSystemLogEntry))
        << "file:\n"
        << file_log_txt << "\nconsole:\n"
        << console_log_txt;
#endif
  } else {
    // No SYSTEM output from Router today, so disable until Router does
#if 0    
    EXPECT_THAT(file_log_txt, Not(HasSubstr(kSystemLogEntry)))
        << "file:\n"
        << file_log_txt << "\nconsole:\n"
        << console_log_txt;
#endif
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
            /* filelog_expected_level =  */ LogLevel::kError),
        // level note to filelog sink (TS_FR1_01)
        // Note: Router does not log at NOTE now
        /*19*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=note\n"
            "sinks=filelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kNote),
        // note level to filelog sink (TS_FR1_02)
        // Note: Router does not log at NOTE now
        /*20*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=system\n"
            "sinks=filelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kSystem)));

#ifndef WIN32
INSTANTIATE_TEST_CASE_P(
    LoggingConfigTestUnix, RouterLoggingTestConfig,
    ::testing::Values(
        // We can't reliably check if the syslog logging is working with a
        // component test as this is too operating system intrusive and we are
        // supposed to run on pb2 environment. Let's at least check that this
        // sink type is supported
        // Level note to syslog,filelog (TS_FR1_06)
        /*0*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=note\n"
            "sinks=syslog,filelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kNote),
        // Level system to syslog,filelog (TS_FR1_07)
        /*1*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=system\n"
            "sinks=syslog,filelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kSystem),
        // All sinks (TS_FR1_08)
        /*2*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=debug\n"
            "sinks=syslog,filelog,consolelog\n"
            "[consolelog]\n"
            "level=note\n"
            "[syslog]\n"
            "level=system\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNote,
            /* filelog_expected_level =  */ LogLevel::kDebug)));
#else
INSTANTIATE_TEST_CASE_P(
    LoggingConfigTestWindows, RouterLoggingTestConfig,
    ::testing::Values(
        // We can't reliably check if the eventlog logging is working with a
        // component test as this is too operating system intrusive and also
        // requires admin priviledges to setup and we are supposed to run on pb2
        // environment. Let's at least check that this sink type is supported.
        // Level note to eventlog,filelog (TS_FR1_03)
        /*0*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=note\n"
            "sinks=eventlog,filelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kNote),
        // Level system to eventlog,filelog (TS_FR1_04)
        /*1*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=system\n"
            "sinks=eventlog,filelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kSystem),
        // All sinks with note and system included (TS_FR1_05)
        /*2*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=debug\n"
            "sinks=eventlog,filelog,consolelog\n"
            "[consolelog]\n"
            "level=note\n"
            "[eventlog]\n"
            "level=system\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNote,
            /* filelog_expected_level =  */ LogLevel::kDebug)));
#endif

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
      public ::testing::WithParamInterface<LoggingConfigErrorParams> {};

/** @test This test verifies that a proper error gets printed on the console for
 * a particular logging configuration
 */
TEST_P(RouterLoggingConfigError, LoggingConfigError) {
  auto test_params = GetParam();

  TempDirectory tmp_dir;
  auto conf_params = get_DEFAULT_defaults();
  conf_params["logging_folder"] =
      test_params.logging_folder_empty ? "" : tmp_dir.name();

  TempDirectory conf_dir("conf");
  const std::string conf_text = "[keepalive]\n" + test_params.logger_config;

  const std::string conf_file =
      create_config_file(conf_dir.name(), conf_text, &conf_params);

  auto &router = launch_router({"-c", conf_file}, 1);

  check_exit_code(router, EXIT_FAILURE);

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
            "values are: debug, error, fatal, info, note, system, and warning"),

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
            "values are: debug, error, fatal, info, note, system, and warning"),

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
            "values are: debug, error, fatal, info, note, system, and warning"),

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
            "values are: debug, error, fatal, info, note, system, and warning"),

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
            "values are: debug, error, fatal, info, note, system, and warning"),

        // Let's also check that the syslog is NOT supported
        LoggingConfigErrorParams("[logger]\n"
                                 "sinks=syslog\n"
                                 "[syslog]\n"
                                 "level=invalid\n",
                                 /* logging_folder_empty = */ false,
                                 /* expected_error =  */
                                 "plugin 'syslog' failed to load")));
#endif

class RouterLoggingTestTimestampPrecisionConfig
    : public RouterComponentTest,
      public ::testing::WithParamInterface<LoggingConfigOkParams> {};

#define DATE_REGEX "[0-9]{4}-[0-9]{2}-[0-9]{2}"
#define TIME_REGEX "[0-9]{2}:[0-9]{2}:[0-9]{2}"
#define TS_MSEC_REGEX ".[0-9]{3}"
#define TS_USEC_REGEX ".[0-9]{6}"
#define TS_NSEC_REGEX ".[0-9]{9}"
#define TS_REGEX DATE_REGEX " " TIME_REGEX

const std::string kTimestampSecRegex = TS_REGEX " ";
const std::string kTimestampMillisecRegex = TS_REGEX TS_MSEC_REGEX " ";
const std::string kTimestampMicrosecRegex = TS_REGEX TS_USEC_REGEX " ";
const std::string kTimestampNanosecRegex = TS_REGEX TS_NSEC_REGEX " ";

/** @test This test verifies that a proper loggs are written to selected sinks
 * for various sinks/levels combinations.
 */
TEST_P(RouterLoggingTestTimestampPrecisionConfig,
       LoggingTestTimestampPrecisionConfig) {
  auto test_params = GetParam();

  TempDirectory tmp_dir;
  TcpPortPool port_pool;
  const auto router_port = port_pool.get_next_available();
  const auto server_port = port_pool.get_next_available();

  // Different log entries that are expected for different levels, but we only
  // care that something is logged, not what, when checking timestamps.

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
      test_params.logging_folder_empty ? "" : tmp_dir.name();

  TempDirectory conf_dir("conf");
  const std::string conf_text =
      test_params.logger_config + "\n" + kRoutingConfig;

  const std::string conf_file =
      create_config_file(conf_dir.name(), conf_text, &conf_params);

  auto &router = launch_router({"-c", conf_file});

  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port));

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

  // check the console log if it contains what's expected
  std::string console_log_txt = router.get_full_output();

  // strip first line before checking if needed
  const std::string prefix = "logging facility initialized";
  if (std::mismatch(console_log_txt.begin(), console_log_txt.end(),
                    prefix.begin(), prefix.end())
          .second == prefix.end()) {
    console_log_txt.erase(0, console_log_txt.find("\n") + 1);
  }

  if (test_params.consolelog_expected_level != LogLevel::kNotSet) {
    switch (test_params.consolelog_expected_timestamp_precision) {
      case LogTimestampPrecision::kNotSet:
      case LogTimestampPrecision::kSec:
        // EXPECT 12:00:00
        EXPECT_TRUE(pattern_found(console_log_txt, kTimestampSecRegex))
            << console_log_txt;
        break;
      case LogTimestampPrecision::kMilliSec:
        // EXPECT 12:00:00.000
        EXPECT_TRUE(pattern_found(console_log_txt, kTimestampMillisecRegex))
            << console_log_txt;
        break;
      case LogTimestampPrecision::kMicroSec:
        // EXPECT 12:00:00.000000
        EXPECT_TRUE(pattern_found(console_log_txt, kTimestampMicrosecRegex))
            << console_log_txt;
        break;
      case LogTimestampPrecision::kNanoSec:
        // EXPECT 12:00:00.000000000
        EXPECT_TRUE(pattern_found(console_log_txt, kTimestampNanosecRegex))
            << console_log_txt;
        break;
    }
  }

  // check the file log if it contains what's expected
  std::string file_log_txt =
      router.get_full_logfile("mysqlrouter.log", tmp_dir.name());

  // strip first line before checking if needed
  if (std::mismatch(file_log_txt.begin(), file_log_txt.end(), prefix.begin(),
                    prefix.end())
          .second == prefix.end()) {
    file_log_txt.erase(0, file_log_txt.find("\n") + 1);
  }

  if (test_params.filelog_expected_level != LogLevel::kNotSet) {
    switch (test_params.filelog_expected_timestamp_precision) {
      case LogTimestampPrecision::kNotSet:
      case LogTimestampPrecision::kSec:
        // EXPECT 12:00:00
        EXPECT_TRUE(pattern_found(file_log_txt, kTimestampSecRegex))
            << file_log_txt;
        break;
      case LogTimestampPrecision::kMilliSec:
        // EXPECT 12:00:00.000
        EXPECT_TRUE(pattern_found(file_log_txt, kTimestampMillisecRegex))
            << file_log_txt;
        break;
      case LogTimestampPrecision::kMicroSec:
        // EXPECT 12:00:00.000000
        EXPECT_TRUE(pattern_found(file_log_txt, kTimestampMicrosecRegex))
            << file_log_txt;
        break;
      case LogTimestampPrecision::kNanoSec:
        // EXPECT 12:00:00.000000000
        EXPECT_TRUE(pattern_found(file_log_txt, kTimestampNanosecRegex))
            << file_log_txt;
        break;
    }
  }
}

#define TS_FR1_1_STR(x)        \
  "[logger]\n"                 \
  "level=debug\n"              \
  "sinks=consolelog,filelog\n" \
  "timestamp_precision=" x     \
  "\n"                         \
  "[consolelog]\n\n[filelog]\n\n"

#define TS_FR1_2_STR(x) TS_FR1_1_STR(x)

#define TS_FR1_3_STR(x) TS_FR1_1_STR(x)

INSTANTIATE_TEST_CASE_P(
    LoggingConfigTimestampPrecisionTest,
    RouterLoggingTestTimestampPrecisionConfig,
    ::testing::Values(
        // no logger section, no sinks sections
        // logging_folder not empty so we are expected to log to the file
        // with a warning level so info and debug logs will not be there
        /*0*/ LoggingConfigOkParams(
            "",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kWarning,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNotSet,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNotSet),
        // Two sinks, common timestamp_precision
        /*** TS_FR1_1 ***/
        /*1*/ /*TS_FR1_1.1*/
        LoggingConfigOkParams(
            TS_FR1_1_STR("second"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec),
        /*2*/ /*TS_FR1_1.2*/
        LoggingConfigOkParams(
            TS_FR1_1_STR("Second"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec),
        /*3*/ /*TS_FR1_1.3*/
        LoggingConfigOkParams(
            TS_FR1_1_STR("sec"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec),
        /*4*/ /*TS_FR1_1.4*/
        LoggingConfigOkParams(
            TS_FR1_1_STR("SEC"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec),
        /*5*/ /*TS_FR1_1.5*/
        LoggingConfigOkParams(
            TS_FR1_1_STR("s"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec),
        /*6*/ /*TS_FR1_1.6*/
        LoggingConfigOkParams(
            TS_FR1_1_STR("S"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec),
        /*** TS_FR1_2 ***/
        /*7*/ /*TS_FR1_2.1*/
        LoggingConfigOkParams(
            TS_FR1_2_STR("millisecond"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec),
        /*8*/ /*TS_FR1_2.2*/
        LoggingConfigOkParams(
            TS_FR1_2_STR("MILLISECOND"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec),
        /*9*/ /*TS_FR1_2.3*/
        LoggingConfigOkParams(
            TS_FR1_2_STR("msec"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec),
        /*10*/ /*TS_FR1_2.4*/
        LoggingConfigOkParams(
            TS_FR1_2_STR("MSEC"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec),
        /*11*/ /*TS_FR1_2.5*/
        LoggingConfigOkParams(
            TS_FR1_2_STR("ms"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec),
        /*12*/ /*TS_FR1_2.6*/
        LoggingConfigOkParams(
            TS_FR1_2_STR("MS"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec),
        /*** TS_FR1_3 ***/
        /*13*/ /*TS_FR1_3.1*/
        LoggingConfigOkParams(
            TS_FR1_3_STR("microsecond"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec),
        /*14*/ /*TS_FR1_3.2*/
        LoggingConfigOkParams(
            TS_FR1_3_STR("Microsecond"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec),
        /*15*/ /*TS_FR1_3.3*/
        LoggingConfigOkParams(
            TS_FR1_3_STR("usec"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec),
        /*16*/ /*TS_FR1_3.4*/
        LoggingConfigOkParams(
            TS_FR1_3_STR("UsEC"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec),
        /*17*/ /*TS_FR1_3.5*/
        LoggingConfigOkParams(
            TS_FR1_3_STR("us"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec),
        /*18*/ /*TS_FR1_3.5*/
        LoggingConfigOkParams(
            TS_FR1_3_STR("US"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec),
        /*** TS_FR1_4 ***/
        /*19*/ /*TS_FR1_4.1*/
        LoggingConfigOkParams(
            TS_FR1_3_STR("nanosecond"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec),
        /*20*/ /*TS_FR1_4.2*/
        LoggingConfigOkParams(
            TS_FR1_3_STR("NANOSECOND"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec),
        /*21*/ /*TS_FR1_4.3*/
        LoggingConfigOkParams(
            TS_FR1_3_STR("nsec"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec),
        /*22*/ /*TS_FR1_4.4*/
        LoggingConfigOkParams(
            TS_FR1_3_STR("nSEC"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec),
        /*23*/ /*TS_FR1_4.5*/
        LoggingConfigOkParams(
            TS_FR1_3_STR("ns"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec),
        /*24*/ /*TS_FR1_4.6*/
        LoggingConfigOkParams(
            TS_FR1_3_STR("NS"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec),
        /*25*/ /*TS_FR4_2*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=debug\n"
            "sinks=filelog\n"
            "[filelog]\n"
            "timestamp_precision=ms\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNotSet,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec),
        /*26*/ /*TS_FR4_3*/
        LoggingConfigOkParams(
            "[logger]\n"
            "level=debug\n"
            "sinks=filelog,consolelog\n"
            "[consolelog]\n"
            "timestamp_precision=ns\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec)));

INSTANTIATE_TEST_CASE_P(
    LoggingConfigTimestampPrecisionError, RouterLoggingConfigError,
    ::testing::Values(
        // Unknown timestamp_precision value in a sink
        /*0*/ /*TS_FR3_1*/ LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=consolelog\n"
            "[consolelog]\n"
            "timestamp_precision=unknown\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "Configuration error: Timestamp precision 'unknown' is not valid. "
            "Valid values are: microsecond, millisecond, ms, msec, nanosecond, "
            "ns, nsec, s, sec, second, us, and usec"),
        // Unknown timestamp_precision value in the [logger] section
        /*1*/ /*TS_FR3_1*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=consolelog,filelog\n"
            "timestamp_precision=unknown\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "Configuration error: Timestamp precision 'unknown' is not valid. "
            "Valid values are: microsecond, millisecond, ms, msec, nanosecond, "
            "ns, nsec, s, sec, second, us, and usec"),
        /*2*/ /*TS_FR4_1*/
        LoggingConfigErrorParams("[logger]\n"
                                 "sinks=consolelog,filelog\n"
                                 "timestamp_precision=ms\n"
                                 "timestamp_precision=ns\n",
                                 /* logging_folder_empty = */ false,
                                 /* expected_error =  */
                                 "Configuration error: Option "
                                 "'timestamp_precision' already defined.")));
#ifndef _WIN32
INSTANTIATE_TEST_CASE_P(
    LoggingConfigTimestampPrecisionErrorUnix, RouterLoggingConfigError,
    ::testing::Values(
        /*0*/ /* TS_HLD_1 */
        LoggingConfigErrorParams("[logger]\n"
                                 "sinks=syslog\n"
                                 "[syslog]\n"
                                 "timestamp_precision=ms\n",
                                 /* logging_folder_empty = */ false,
                                 /* expected_error =  */
                                 "Configuration error: timestamp_precision not "
                                 "valid for 'syslog'")));
#else
INSTANTIATE_TEST_CASE_P(
    LoggingConfigTimestampPrecisionErrorWindows, RouterLoggingConfigError,
    ::testing::Values(
        /*0*/ /* TS_HLD_3 */
        LoggingConfigErrorParams("[logger]\n"
                                 "sinks=eventlog\n"
                                 "[eventlog]\n"
                                 "timestamp_precision=ms\n",
                                 /* logging_folder_empty = */ false,
                                 /* expected_error =  */
                                 "Configuration error: timestamp_precision not "
                                 "valid for 'eventlog'")));
#endif

TEST_F(RouterLoggingTest, very_long_router_name_gets_properly_logged) {
  // This test verifies that a very long router name gets truncated in the
  // logged message (this is done because if it doesn't happen, the entire
  // message will exceed log message max length, and then the ENTIRE message
  // will get truncated instead. It's better to truncate the long name rather
  // than the stuff that follows it).
  // Router should report the error on STDERR and exit

  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();
  TempDirectory bootstrap_dir;

  const auto server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto &server_mock = launch_mysql_server_mock(json_stmts, server_port);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

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
  auto &router = launch_router(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--name",
          name,
          "-d",
          bootstrap_dir.name(),
      },
      1);
  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // wait for router to exit
  check_exit_code(router, EXIT_FAILURE);

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
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  TempDirectory bootstrap_dir;

  const auto server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto &server_mock = launch_mysql_server_mock(json_stmts, server_port, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  // launch the router in bootstrap mode
  auto &router = launch_router({
      "--bootstrap=127.0.0.1:" + std::to_string(server_port),
      "--report-host",
      "dont.query.dns",
      "-d",
      bootstrap_dir.name(),
  });

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping was successful
  check_exit_code(router, EXIT_SUCCESS);
  EXPECT_THAT(router.get_full_output(),
              testing::Not(testing::HasSubstr("Executing query:")));
}

/**
 * @test verify that debug logs are written to console during boostrap if
 * log_level is set to DEBUG in bootstrap configuration file.
 */
TEST_F(RouterLoggingTest, is_debug_logs_enabled_if_bootstrap_config_file) {
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  TempDirectory bootstrap_dir;
  TempDirectory bootstrap_conf;

  const auto server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto &server_mock = launch_mysql_server_mock(json_stmts, server_port, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  // launch the router in bootstrap mode
  std::string logger_section = "[logger]\nlevel = DEBUG\n";
  auto conf_params = get_DEFAULT_defaults();
  // we want to log to the console
  conf_params["logging_folder"] = "";
  std::string conf_file = create_config_file(
      bootstrap_conf.name(), logger_section, &conf_params, "bootstrap.conf");

  auto &router = launch_router({
      "--bootstrap=127.0.0.1:" + std::to_string(server_port),
      "--report-host",
      "dont.query.dns",
      "--force",
      "-d",
      bootstrap_dir.name(),
      "-c",
      conf_file,
  });

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping was successful
  check_exit_code(router, EXIT_SUCCESS);
  EXPECT_THAT(router.get_full_output(), testing::HasSubstr("Executing query:"));
}

/**
 * @test verify that debug logs are written to mysqlrouter.log file during
 * bootstrap if loggin_folder is provided in bootstrap configuration file
 */
TEST_F(RouterLoggingTest, is_debug_logs_written_to_file_if_logging_folder) {
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  TempDirectory bootstrap_dir;
  TempDirectory bootstrap_conf;

  const auto server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto &server_mock = launch_mysql_server_mock(json_stmts, server_port, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  // create config with logging_folder set to that directory
  std::map<std::string, std::string> params = {{"logging_folder", ""}};
  params.at("logging_folder") = bootstrap_conf.name();
  TempDirectory conf_dir("conf");
  const std::string conf_file =
      create_config_file(conf_dir.name(), "[logger]\nlevel = DEBUG\n", &params);

  auto &router = launch_router({
      "--bootstrap=127.0.0.1:" + std::to_string(server_port),
      "--report-host",
      "dont.query.dns",
      "--force",
      "-d",
      bootstrap_dir.name(),
      "-c",
      conf_file,
  });

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping was successful
  check_exit_code(router, EXIT_SUCCESS);

  auto matcher = [](const std::string &line) -> bool {
    return line.find("Executing query:") != line.npos;
  };

  EXPECT_TRUE(find_in_file(bootstrap_conf.name() + "/mysqlrouter.log", matcher,
                           std::chrono::milliseconds(5000)))
      << router.get_full_logfile("mysqlrouter.log", bootstrap_conf.name());
}

/**
 * @test verify that normal output is written to stdout during bootstrap if
 * logging_folder is not provided in bootstrap configuration file.
 *
 * @test verify that logs are not written to stdout during bootstrap.
 */
TEST_F(RouterLoggingTest, bootstrap_normal_logs_written_to_stdout) {
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  TempDirectory bootstrap_dir;
  TempDirectory bootstrap_conf;

  const auto server_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  auto &server_mock = launch_mysql_server_mock(json_stmts, server_port, false);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  // launch the router in bootstrap mode
  std::string logger_section = "[logger]\nlevel = DEBUG\n";
  auto conf_params = get_DEFAULT_defaults();
  // we want to log to the console
  conf_params["logging_folder"] = "";
  std::string conf_file = create_config_file(
      bootstrap_conf.name(), logger_section, &conf_params, "bootstrap.conf");

  auto &router = launch_router(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--report-host",
          "dont.query.dns",
          "--force",
          "-d",
          bootstrap_dir.name(),
          "-c",
          conf_file,
      },
      0, /* expected error code */
      false /*false = capture only stdout*/);

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping was successful
  check_exit_code(router, EXIT_SUCCESS);

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
    RouterLoggingTest::SetUp();

    mysql_harness::DIM &dim = mysql_harness::DIM::instance();
    // RandomGenerator
    dim.set_RandomGenerator(
        []() {
          static mysql_harness::RandomGenerator rg;
          return &rg;
        },
        [](mysql_harness::RandomGeneratorInterface *) {});

    cluster_nodes_ports = {port_pool_.get_next_available(),
                           port_pool_.get_next_available(),
                           port_pool_.get_next_available()};
    cluster_nodes_http_ports = {port_pool_.get_next_available(),
                                port_pool_.get_next_available(),
                                port_pool_.get_next_available()};
    router_port = port_pool_.get_next_available();
    metadata_cache_section = get_metadata_cache_section(cluster_nodes_ports);
    routing_section =
        get_metadata_cache_routing_section("PRIMARY", "round-robin", "");
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
           "connect_timeout=1\n"
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
    init_keyring(default_section, temp_test_dir.name());
    default_section["logging_folder"] =
        log_to_console ? "" : get_logging_dir().str();
    return create_config_file(
        conf_dir,
        "[logger]\nlevel = DEBUG\n" + metadata_cache_section + routing_section,
        &default_section);
  }

  TempDirectory temp_test_dir;
  std::vector<uint16_t> cluster_nodes_ports;
  std::vector<uint16_t> cluster_nodes_http_ports;
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
  TempDirectory conf_dir;

  // launch the router with metadata-cache configuration
  auto &router = ProcessManager::launch_router(
      {"-c", init_keyring_and_config_file(conf_dir.name())});
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port, 10000ms));

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
      << router.get_full_logfile();
}

/**
 * @test verify if appropriate warning messages are logged when cannot connect
 * to first metadata server, but can connect to another one.
 */
TEST_F(MetadataCacheLoggingTest,
       log_warning_when_cannot_connect_to_first_metadata_server) {
  TempDirectory conf_dir("conf");

  // launch second metadata server
  const auto http_port = cluster_nodes_http_ports[1];
  auto &server = launch_mysql_server_mock(
      get_data_dir().join("metadata_3_nodes_first_not_accessible.js").str(),
      cluster_nodes_ports[1], EXIT_SUCCESS, false, http_port);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server, cluster_nodes_ports[1]));
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
  set_mock_metadata(http_port, "", cluster_nodes_ports);

  // launch the router with metadata-cache configuration
  auto &router = ProcessManager::launch_router(
      {"-c", init_keyring_and_config_file(conf_dir.name())});
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port));

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
                           info_matcher, 10000ms))
      << router.get_full_logfile();

  auto warning_matcher = [](const std::string &line) -> bool {
    return line.find("metadata_cache WARNING") != line.npos &&
           line.find(
               "While updating metadata, could not establish a connection to "
               "replicaset") != line.npos;
  };
  EXPECT_TRUE(find_in_file(get_logging_dir().str() + "/mysqlrouter.log",
                           warning_matcher, 10000ms))
      << router.get_full_logfile();
}

#ifndef _WIN32
/**
 * @test Checks that the logs rotation works (meaning Router will recreate
 * it's log file when it was moved and HUP singnal was sent to the Router).
 */
TEST_F(MetadataCacheLoggingTest, log_rotation_by_HUP_signal) {
  TempDirectory conf_dir;

  // launch the router with metadata-cache configuration
  auto &router = ProcessManager::launch_router(
      {"-c", init_keyring_and_config_file(conf_dir.name())});
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port, 10000ms));

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

  EXPECT_TRUE(log_file.exists()) << router.get_full_logfile();
  EXPECT_TRUE(log_file_1.exists());
}

/**
 * @test Checks that the Router continues to log to the file when the
 * SIGHUP gets sent to it and no file replacement is done.
 */
TEST_F(MetadataCacheLoggingTest, log_rotation_by_HUP_signal_no_file_move) {
  TempDirectory conf_dir;

  // launch the router with metadata-cache configuration
  auto &router = ProcessManager::launch_router(
      {"-c", init_keyring_and_config_file(conf_dir.name())});
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port, 10000ms));

  std::this_thread::sleep_for(500ms);

  auto log_file = get_logging_dir();
  log_file.append("mysqlrouter.log");

  EXPECT_TRUE(log_file.exists());

  // grab the current log content
  const std::string log_content = router.get_full_logfile();

  // send the log-rotate signal
  const auto pid = static_cast<pid_t>(router.get_pid());
  ::kill(pid, SIGHUP);

  // wait until something new gets logged;
  std::string log_content_2;
  unsigned step = 0;
  do {
    std::this_thread::sleep_for(100ms);
    log_content_2 = router.get_full_logfile();
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
  TempDirectory conf_dir;

  // launch the router with metadata-cache configuration
  auto &router = ProcessManager::launch_router(
      {"-c", init_keyring_and_config_file(conf_dir.name())});
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port, 10000ms));

  std::this_thread::sleep_for(500ms);

  auto log_file = get_logging_dir();
  log_file.append("mysqlrouter.log");

  EXPECT_TRUE(log_file.exists());

  // now stop the router
  int res = router.kill();
  EXPECT_EQ(EXIT_SUCCESS, res) << router.get_full_output();

  // move the log_file appending '.1' to its name
  auto log_file_1 = get_logging_dir();
  log_file_1.append("mysqlrouter.log.1");
  mysqlrouter::rename_file(log_file.str(), log_file_1.str());

  // make the new file read-only
  chmod(log_file_1.c_str(), S_IRUSR);

  // start the router again and check that the new log file got created
  auto &router2 = ProcessManager::launch_router(
      {"-c", init_keyring_and_config_file(conf_dir.name())});
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router2, router_port, 10000ms));
  std::this_thread::sleep_for(500ms);
  EXPECT_TRUE(log_file.exists());
}

/**
 * @test Checks that the logs Router continues to log to the file when the
 * SIGHUP gets sent to it and no file replacement is done.
 */
TEST_F(MetadataCacheLoggingTest, log_rotation_read_only) {
  TempDirectory conf_dir;

  // launch the router with metadata-cache configuration
  auto &router = ProcessManager::launch_router(
      {"-c", init_keyring_and_config_file(conf_dir.name())}, EXIT_FAILURE);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port, 10000ms));

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
  check_exit_code(router, EXIT_FAILURE);
  EXPECT_THAT(router.get_full_output(),
              HasSubstr("File exists, but cannot open for writing"));
  EXPECT_THAT(router.get_full_output(), HasSubstr("Unloading all plugins."));
}

/**
 * @test Checks that the logs rotation does not cause any crash in case of
 * not logging to the file (logging_foler empty == logging to the std:cerr)
 */
TEST_F(MetadataCacheLoggingTest, log_rotation_stdout) {
  TempDirectory conf_dir;

  // launch the router with metadata-cache configuration
  auto &router = ProcessManager::launch_router(
      {"-c",
       init_keyring_and_config_file(conf_dir.name(), /*log_to_console=*/true)});
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_port, 10000ms));

  std::this_thread::sleep_for(200ms);
  const auto pid = static_cast<pid_t>(router.get_pid());
  ::kill(pid, SIGHUP);
  std::this_thread::sleep_for(200ms);
}

#endif

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
