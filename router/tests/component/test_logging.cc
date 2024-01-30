/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "config_builder.h"
#include "dim.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"  // split_string
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/utils.h"  // rename_file
#include "process_wrapper.h"
#include "random_generator.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_config.h"
#include "router_test_helpers.h"  // get_file_output
#include "tcp_port_pool.h"

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
using namespace std::string_literals;

class RouterLoggingTest : public RouterComponentBootstrapTest {
 protected:
  std::string create_config_file(
      const std::string &directory, const std::string &sections,
      const std::map<std::string, std::string> *default_section) const {
    return ProcessManager::create_config_file(
        directory, sections, default_section, "mysqlrouter.conf", "", false);
  }

  ProcessWrapper &launch_router_for_fail(
      const std::vector<std::string> &params) {
    return launch_router(
        params, EXIT_FAILURE, true, false, -1s,
        RouterComponentBootstrapTest::kBootstrapOutputResponder);
  }

  ProcessWrapper &launch_router_for_success(
      const std::vector<std::string> &params) {
    return launch_router(params, EXIT_SUCCESS, true);
  }
};

/** @test Check that the Router logs its version when it is started and stopped
 */
TEST_F(RouterLoggingTest, log_start_stop_with_version) {
  // create tmp dir where we will log
  TempDirectory logging_folder;

  std::map<std::string, std::string> params = get_DEFAULT_defaults();
  params.at("logging_folder") = logging_folder.name();
  TempDirectory conf_dir("conf");
  const std::string conf_file =
      create_config_file(conf_dir.name(), "[keepalive]", &params);

  // run the router and close right away
  auto &router = launch_router_for_success({"-c", conf_file});
  router.send_shutdown_event();
  router.wait_for_exit();

  auto file_content =
      router.get_logfile_content("mysqlrouter.log", logging_folder.name());
  auto lines = mysql_harness::split_string(file_content, '\n');

#if defined(_WIN32)
  const std::string stopping_info = "";
#elif defined(__APPLE__)
  const std::string stopping_info = " \\(Signal .*\\)";
#else
  const std::string stopping_info =
      " \\(Signal .* sent by UID: .* and PID: .*\\)";
#endif

  EXPECT_THAT(
      file_content,
      ::testing::AllOf(
          ::testing::ContainsRegex(
              "main SYSTEM .* Starting 'MySQL Router', version: "s +
              MYSQL_ROUTER_VERSION + " \\(" + MYSQL_ROUTER_VERSION_EDITION +
              "\\)"),
          ::testing::ContainsRegex(
              "main SYSTEM .* Stopping 'MySQL Router', version: "s +
              MYSQL_ROUTER_VERSION + " \\(" + MYSQL_ROUTER_VERSION_EDITION +
              "\\), reason: REQUESTED" + stopping_info)));
}

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
  auto &router = launch_router_for_fail({"-c", conf_file});
  check_exit_code(router, EXIT_FAILURE);

  // expect something like this to appear on STDERR
  // plugin 'invalid' failed to
  // load: ./plugin_output_directory/invalid.so: cannot open shared object
  // file: No such file or directory
  const std::string out = router.get_full_output();
  EXPECT_THAT(
      out, HasSubstr("Loading plugin for config-section '[invalid]' failed"));
}

/** @test This test is similar to log_startup_failure_to_console(), but the
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
  auto &router = launch_router_for_fail({"-c", conf_file});
  check_exit_code(router, EXIT_FAILURE);

  // expect something like this to appear in log:
  // 2018-12-19 03:54:04 main ERROR [7f539f628780] Configuration error: option
  // destinations in [routing] is required
  auto file_content =
      router.get_logfile_content("mysqlrouter.log", logging_folder.name());
  auto lines = mysql_harness::split_string(file_content, '\n');

  EXPECT_THAT(lines,
              ::testing::Contains(::testing::HasSubstr(
                  "Configuration error: option destinations in [routing] is "
                  "required")));
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
    auto &router = launch_router_for_fail({"-c", conf_file});
    check_exit_code(router, EXIT_FAILURE);

    // expect something like this to appear on STDERR
    // Error: Error when creating dir '/bla': 13
    const std::string out = router.get_full_output();
    EXPECT_THAT(out.c_str(),
                HasSubstr("  init 'logger' failed: Error when creating dir '" +
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
    auto &router = launch_router_for_fail({"-c", conf_file});
    check_exit_code(router, EXIT_FAILURE);

    // expect something like this to appear on STDERR
    // Error: Cannot create file in directory //mysqlrouter.log: Permission
    // denied
    const std::string out = router.get_full_output();
#ifndef _WIN32
    EXPECT_THAT(
        out.c_str(),
        HasSubstr("  init 'logger' failed: Cannot create file in directory " +
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
    auto &router = launch_router_for_fail({"-c", conf_file});
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
  auto &router = launch_router_for_fail({"-c", conf_file});
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
  auto &router = launch_router_for_fail({"-c", conf_file});
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
  auto &router = launch_router_for_fail({"-c", conf_file});
  check_exit_code(router, EXIT_FAILURE);

  // expect something like this to appear on STDERR
  // Configuration error: Log level 'unknown' is not valid. Valid values are:
  // fatal, system, error, warning, info, note, and debug
  const std::string out = router.get_full_output();
  EXPECT_THAT(
      out.c_str(),
      HasSubstr(
          "Configuration error: Log level 'unknown' is not valid. Valid "
          "values are: fatal, system, error, warning, info, note, and debug"));
}

/**************************************************/
/* Tests for valid logger configurations          */
/**************************************************/

struct LoggingConfigOkParams {
  const char *test_name;

  std::string logger_config;
  bool logging_folder_empty;

  LogLevel consolelog_expected_level;
  LogLevel filelog_expected_level;

  LogTimestampPrecision consolelog_expected_timestamp_precision;
  LogTimestampPrecision filelog_expected_timestamp_precision;

  LoggingConfigOkParams(const char *test_name_,
                        const std::string &logger_config_,
                        const bool logging_folder_empty_,
                        const LogLevel consolelog_expected_level_,
                        const LogLevel filelog_expected_level_)
      : test_name{test_name_},
        logger_config(logger_config_),
        logging_folder_empty(logging_folder_empty_),
        consolelog_expected_level(consolelog_expected_level_),
        filelog_expected_level(filelog_expected_level_),
        consolelog_expected_timestamp_precision(LogTimestampPrecision::kNotSet),
        filelog_expected_timestamp_precision(LogTimestampPrecision::kNotSet) {}

  LoggingConfigOkParams(
      const char *test_name_, const std::string &logger_config_,
      const bool logging_folder_empty_,
      const LogLevel consolelog_expected_level_,
      const LogLevel filelog_expected_level_,
      const LogTimestampPrecision consolelog_expected_timestamp_precision_,
      const LogTimestampPrecision filelog_expected_timestamp_precision_)
      : test_name{test_name_},
        logger_config(logger_config_),
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
    : public RouterLoggingTest,
      public ::testing::WithParamInterface<LoggingConfigOkParams> {};

/** @test This test verifies that a proper loggs are written to selected sinks
 * for various sinks/levels combinations.
 */
TEST_P(RouterLoggingTestConfig, check) {
  auto test_params = GetParam();

  TempDirectory tmp_dir;

  // These are different level log entries that are expected to get logged after
  // the logger plugin has been initialized
  const std::string kDebugLogEntry = "I'm a debug message";
  const std::string kInfoLogEntry = "I'm an info message";
  const std::string kWarningLogEntry = "I'm a warning message";
  const std::string kNoteLogEntry = "I'm a note message";
  const std::string kSystemLogEntry = "I'm a system message";

  // trigger all messages once.
  const std::string kOtherPluginConfig = "[routertestplugin_logger]\n";

  auto conf_params = get_DEFAULT_defaults();
  conf_params["logging_folder"] =
      test_params.logging_folder_empty ? "" : tmp_dir.name();

  TempDirectory conf_dir("conf");
  const std::string conf_text =
      test_params.logger_config + "\n" + kOtherPluginConfig;

  const std::string conf_file =
      create_config_file(conf_dir.name(), conf_text, &conf_params);

  // use the parent's "launch_router" to wait for NOTIFY_SOCKET
  auto &router = ProcessManager::launch_router({"-c", conf_file});

  SCOPED_TRACE("// stop router to ensure all logs are written");
  router.send_clean_shutdown_event();
  try {
    EXPECT_EQ(router.wait_for_exit(), EXIT_SUCCESS);
  } catch (const std::exception &e) {
    FAIL() << e.what();
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
    EXPECT_THAT(console_log_txt, HasSubstr(kNoteLogEntry)) << "console:\n"
                                                           << console_log_txt;
  } else {
    EXPECT_THAT(console_log_txt, Not(HasSubstr(kNoteLogEntry)))
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

  if (test_params.consolelog_expected_level >= LogLevel::kSystem &&
      test_params.consolelog_expected_level != LogLevel::kNotSet) {
    // No SYSTEM output from Router today, so disable until Router does
    EXPECT_THAT(console_log_txt, HasSubstr(kSystemLogEntry)) << "console:\n"
                                                             << console_log_txt;
  } else {
    // No SYSTEM output from Router today, so disable until Router does
    EXPECT_THAT(console_log_txt, Not(HasSubstr(kSystemLogEntry)))
        << "console:\n"
        << console_log_txt;
  }

  // check the file log if it contains what's expected
  const std::string file_log_txt =
      router.get_logfile_content("mysqlrouter.log", tmp_dir.name());

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
    EXPECT_THAT(file_log_txt, HasSubstr(kNoteLogEntry))
        << "file:\n"
        << file_log_txt << "\nconsole:\n"
        << console_log_txt;
  } else {
    EXPECT_THAT(file_log_txt, Not(HasSubstr(kNoteLogEntry)))
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

  if (test_params.filelog_expected_level >= LogLevel::kSystem &&
      test_params.filelog_expected_level != LogLevel::kNotSet) {
    EXPECT_THAT(file_log_txt, HasSubstr(kSystemLogEntry))
        << "file:\n"
        << file_log_txt << "\nconsole:\n"
        << console_log_txt;
  } else {
    EXPECT_THAT(file_log_txt, Not(HasSubstr(kSystemLogEntry)))
        << "file:\n"
        << file_log_txt << "\nconsole:\n"
        << console_log_txt;
  }
}

INSTANTIATE_TEST_SUITE_P(
    Spec, RouterLoggingTestConfig,
    ::testing::Values(
        // no logger section, no sinks sections
        // logging_folder not empty so we are expected to log to the file
        // with a warning level so info and debug logs will not be there
        LoggingConfigOkParams(
            "no_logger_section_no_sinks_no_logger_folder",  // testname
            "",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kWarning),

        // no logger section, no sinks sections
        // logging_folder empty so we are expected to log to the console
        // with a warning level so info and debug logs will not be there
        LoggingConfigOkParams(
            "no_logger_section_no_sinks",  // testname
            "",
            /* logging_folder_empty = */ true,
            /* consolelog_expected_level =  */ LogLevel::kWarning,
            /* filelog_expected_level =  */ LogLevel::kNotSet),

        // logger section, no sinks sections
        // logging_folder not empty so we are expected to log to the file
        // with a warning level as level is not redefined in the [logger]
        // section
        LoggingConfigOkParams(
            "logger_section_only",  // testname
            "[logger]",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kWarning),

        // logger section, no sinks sections
        // logging_folder not empty so we are expected to log to the file
        // with a level defined in the logger section
        LoggingConfigOkParams(
            "no_sinks",  // testname
            "[logger]\n"
            "level=info\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kInfo),

        // logger section, no sinks sections; logging_folder is empty so we are
        // expected to log to the console with a level defined in the logger
        // section
        LoggingConfigOkParams(
            "no_sinks_no_logger_folder",  // testname
            "[logger]\n"
            "level=info\n",
            /* logging_folder_empty = */ true,
            /* consolelog_expected_level =  */ LogLevel::kInfo,
            /* filelog_expected_level =  */ LogLevel::kNotSet),

        // consolelog configured as a sink; it does not have its section in the
        // config but that is not an error; even though the logging folder is
        // not empty, we still don't log to the file as sinks= setting wants use
        // the console
        LoggingConfigOkParams(
            "consolelog",
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
        LoggingConfigOkParams(
            "one_sink_ignored",  // testname
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
        LoggingConfigOkParams(
            "two_sinks_inherit_log_level_debug",  // testname
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
            "two_sinks_inherit_log_level_info",  // testname
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
        LoggingConfigOkParams(
            "two_sinks_inherit_warning",  // testname
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
        LoggingConfigOkParams(
            "inherit_info_filelog_debug",  // testname
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
        LoggingConfigOkParams(
            "default_info_overwrite_debug_warning",  // testname
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
        LoggingConfigOkParams(
            "default_warning_overwrite_info_warning",  // testname
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
        LoggingConfigOkParams(
            "default_debug_overwrite_info_warning",  // testname
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
        LoggingConfigOkParams(
            "two_sinks_all_default",  // testname
            "[logger]\n"
            "sinks=filelog,consolelog\n"
            "[filelog]\n"
            "[consolelog]\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kWarning,
            /* filelog_expected_level =  */ LogLevel::kWarning),

        // 2 sinks, level in the [logger] section is warning; it should be
        // used by the sinks as they don't redefine it in their sections
        LoggingConfigOkParams(
            "implicit_sinks_level_warning",  // testname
            "[logger]\n"
            "level=warning\n"
            "sinks=filelog,consolelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kWarning,
            /* filelog_expected_level =  */ LogLevel::kWarning),

        // 2 sinks, level in the [logger] section is error; it should be used
        // by the sinks as they don't redefine it in their sections
        LoggingConfigOkParams(
            "implicit_sinks_level_error",  // testname
            "[logger]\n"
            "level=error\n"
            "sinks=filelog,consolelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kError,
            /* filelog_expected_level =  */ LogLevel::kError),

        // 2 sinks, no level in the [logger] section, each defines it's own
        // level
        LoggingConfigOkParams(
            "explicit_error_debug",  // testname
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
        LoggingConfigOkParams(
            "explicit_implicit_error",  // testname
            "[logger]\n"
            "sinks=filelog,consolelog\n"
            "[filelog]\n"
            "level=error\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kWarning,
            /* filelog_expected_level =  */ LogLevel::kError),
        // level note to filelog sink (TS_FR1_01)
        LoggingConfigOkParams(
            "one_sink_note",  // testname
            "[logger]\n"
            "level=note\n"
            "sinks=filelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kNote),
        // note level to filelog sink (TS_FR1_02)
        LoggingConfigOkParams(
            "one_sink_system",  // testname
            "[logger]\n"
            "level=system\n"
            "sinks=filelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kSystem)),
    [](auto const &info) { return info.param.test_name; });

#ifndef _WIN32
INSTANTIATE_TEST_SUITE_P(
    LoggingConfigTestUnix, RouterLoggingTestConfig,
    ::testing::Values(
        // We can't reliably check if the syslog logging is working with a
        // component test as this is too operating system intrusive and we are
        // supposed to run on pb2 environment. Let's at least check that this
        // sink type is supported
        // Level note to syslog,filelog (TS_FR1_06)
        LoggingConfigOkParams(
            "0",  // testname
            "[logger]\n"
            "level=note\n"
            "sinks=syslog,filelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kNote),
        // Level system to syslog,filelog (TS_FR1_07)
        LoggingConfigOkParams(
            "1",  // testname
            "[logger]\n"
            "level=system\n"
            "sinks=syslog,filelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kSystem),
        // All sinks (TS_FR1_08)
        LoggingConfigOkParams(
            "2",  // testname
            "[logger]\n"
            "level=debug\n"
            "sinks=syslog,filelog,consolelog\n"
            "[consolelog]\n"
            "level=note\n"
            "[syslog]\n"
            "level=system\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNote,
            /* filelog_expected_level =  */ LogLevel::kDebug),
        // Verify filename option is disregarded by syslog sink
        LoggingConfigOkParams(
            "3",  // testname
            "[logger]\n"
            "level=note\n"
            "sinks=syslog,filelog\n"
            "[syslog]\n"
            "filename=foo.log",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kNote)),
    [](auto const &info) { return info.param.test_name; });
#else
INSTANTIATE_TEST_SUITE_P(
    LoggingConfigTestWindows, RouterLoggingTestConfig,
    ::testing::Values(
        // We can't reliably check if the eventlog logging is working with a
        // component test as this is too operating system intrusive and also
        // requires admin privileges to setup and we are supposed to run on pb2
        // environment. Let's at least check that this sink type is supported.
        // Level note to eventlog,filelog (TS_FR1_03)
        LoggingConfigOkParams(
            "0",  // testname
            "[logger]\n"
            "level=note\n"
            "sinks=eventlog,filelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kNote),
        // Level system to eventlog,filelog (TS_FR1_04)
        LoggingConfigOkParams(
            "1",  // testname
            "[logger]\n"
            "level=system\n"
            "sinks=eventlog,filelog\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kSystem),
        // All sinks with note and system included (TS_FR1_05)
        LoggingConfigOkParams(
            "2",  // testname
            "[logger]\n"
            "level=debug\n"
            "sinks=eventlog,filelog,consolelog\n"
            "[consolelog]\n"
            "level=note\n"
            "[eventlog]\n"
            "level=system\n",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNote,
            /* filelog_expected_level =  */ LogLevel::kDebug),
        // Verify filename option is disregarded by eventlog sink
        LoggingConfigOkParams(
            "3",  // testname
            "[logger]\n"
            "level=system\n"
            "sinks=eventlog,filelog\n"
            "[eventlog]\n"
            "filename=foo.log",
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kNotSet,
            /* filelog_expected_level =  */ LogLevel::kSystem)),
    [](auto const &info) { return info.param.test_name; });
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
    : public RouterLoggingTest,
      public ::testing::WithParamInterface<LoggingConfigErrorParams> {};

/** @test This test verifies that a proper error gets printed on the console for
 * a particular logging configuration
 */
TEST_P(RouterLoggingConfigError, check) {
  auto test_params = GetParam();

  TempDirectory tmp_dir;
  auto conf_params = get_DEFAULT_defaults();
  conf_params["logging_folder"] =
      test_params.logging_folder_empty ? "" : tmp_dir.name();

  TempDirectory conf_dir("conf");
  const std::string conf_text =
      "[routertestplugin_logger]\n" + test_params.logger_config;

  const std::string conf_file =
      create_config_file(conf_dir.name(), conf_text, &conf_params);

  auto &router = launch_router_for_fail({"-c", conf_file});
  check_exit_code(router, EXIT_FAILURE);

  // the error happens during the logger initialization so we expect the message
  // on the console which is the default sink until we switch to the
  // configuration from the config file
  const std::string console_log_txt = router.get_full_output();

  EXPECT_THAT(console_log_txt, HasSubstr(test_params.expected_error))
      << "\nconsole:\n"
      << console_log_txt;
}

INSTANTIATE_TEST_SUITE_P(
    Spec, RouterLoggingConfigError,
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
            "  init 'logger' failed: sinks option does not contain any "
            "valid sink name, was ''"),

        // Empty sinks list
        /*2*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=,\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "  init 'logger' failed: Unsupported logger sink type: ''"),

        // Leading comma on a sinks list
        /*3*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=,consolelog\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "  init 'logger' failed: Unsupported logger sink type: ''"),

        // Terminating comma on a sinks list
        /*4*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=consolelog,\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "  init 'logger' failed: Unsupported logger sink type: ''"),

        // Two commas separating sinks
        /*5*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=consolelog,,filelog\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "  init 'logger' failed: Unsupported logger sink type: ''"),

        // Empty space as a sink name
        /*6*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks= \n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "  init 'logger' failed: sinks option does not contain any "
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
            "values are: fatal, system, error, warning, info, note, and debug"),

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
            "values are: fatal, system, error, warning, info, note, and debug"),

        // Both level and sinks values invalid in the [logger] section
        /*9*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=invalid\n"
            "level=invalid\n"
            "[consolelog]\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "Configuration error: Log level 'invalid' is not valid. Valid "
            "values are: fatal, system, error, warning, info, note, and debug"),

        // Logging folder is empty but we request filelog as sink
        /*10*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=filelog\n",
            /* logging_folder_empty = */ true,
            /* expected_error =  */
            "  init 'logger' failed: filelog sink configured but the "
            "logging_folder is empty")));

#ifndef _WIN32
INSTANTIATE_TEST_SUITE_P(
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
            "values are: fatal, system, error, warning, info, note, and debug"),

        // Let's also check that the eventlog is NOT supported
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=eventlog\n"
            "[eventlog]\n"
            "level=invalid\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "Loading plugin for config-section '[eventlog]' failed")));
#else
INSTANTIATE_TEST_SUITE_P(
    LoggingConfigErrorWindows, RouterLoggingConfigError,
    ::testing::Values(
        // We can't reliably check if the eventlog logging is working with a
        // component test as this is too operating system intrusive and also
        // requires admin privileges to setup and we are supposed to run on pb2
        // environment. Let's at least check that this sink type is supported
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=eventlog\n"
            "[eventlog]\n"
            "level=invalid\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "Configuration error: Log level 'invalid' is not valid. Valid "
            "values are: fatal, system, error, warning, info, note, and debug"),

        // Let's also check that the syslog is NOT supported
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=syslog\n"
            "[syslog]\n"
            "level=invalid\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "Loading plugin for config-section '[syslog]' failed")));
#endif

class RouterLoggingTestTimestampPrecisionConfig
    : public RouterLoggingTest,
      public ::testing::WithParamInterface<LoggingConfigOkParams> {};

static std::string ts_regex(LogTimestampPrecision precision) {
  const std::string base_regex(
#ifdef GTEST_USES_SIMPLE_RE
      "\\d\\d\\d\\d-\\d\\d-\\d\\d "
      "\\d\\d:\\d\\d:\\d\\d"
#else
      "[0-9]{4}-[0-9]{2}-[0-9]{2} "
      "[0-9]{2}:[0-9]{2}:[0-9]{2}"
#endif
  );

  switch (precision) {
    case LogTimestampPrecision::kNotSet:
    case LogTimestampPrecision::kSec:
      // EXPECT 12:00:00
      return base_regex + " ";
    case LogTimestampPrecision::kMilliSec:
      // EXPECT 12:00:00.000
      return base_regex +
#ifdef GTEST_USES_SIMPLE_RE
             "\\.\\d\\d\\d "
#else
             "\\.[0-9]{3} ";
#endif
          ;
    case LogTimestampPrecision::kMicroSec:
      // EXPECT 12:00:00.000000
      return base_regex +
#ifdef GTEST_USES_SIMPLE_RE
             "\\.\\d\\d\\d\\d\\d\\d "
#else
             "\\.[0-9]{6} "
#endif
          ;
    case LogTimestampPrecision::kNanoSec:
      // EXPECT 12:00:00.000000000
      return base_regex +
#ifdef GTEST_USES_SIMPLE_RE
             "\\.\\d\\d\\d\\d\\d\\d\\d\\d\\d "
#else
             "\\.[0-9]{9} "
#endif
          ;
  }

  return {};
}

/** @test This test verifies that a proper loggs are written to selected sinks
 * for various sinks/levels combinations.
 */
TEST_P(RouterLoggingTestTimestampPrecisionConfig, check) {
  auto test_params = GetParam();

  TempDirectory tmp_dir;

  // Different log entries that are expected for different levels, but we only
  // care that something is logged, not what, when checking timestamps.

  const std::string kOtherPluginConfig = "[routertestplugin_logger]\n";

  auto conf_params = get_DEFAULT_defaults();
  conf_params["logging_folder"] =
      test_params.logging_folder_empty ? "" : tmp_dir.name();

  TempDirectory conf_dir("conf");
  const std::string conf_text =
      test_params.logger_config + "\n" + kOtherPluginConfig;

  const std::string conf_file =
      create_config_file(conf_dir.name(), conf_text, &conf_params);

  auto &router = ProcessManager::launch_router({"-c", conf_file});
  router.send_clean_shutdown_event();
  EXPECT_NO_THROW(router.wait_for_exit());

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
    std::vector<std::string> lines;
    std::istringstream ss(console_log_txt);
    for (std::string line; std::getline(ss, line);) {
      lines.push_back(line);
    }

    auto regex = ts_regex(test_params.consolelog_expected_timestamp_precision);
    ASSERT_FALSE(regex.empty());

    EXPECT_THAT(lines, ::testing::Contains(::testing::ContainsRegex(regex)));
  }

  // check the file log if it contains what's expected
  std::string file_log_txt =
      router.get_logfile_content("mysqlrouter.log", tmp_dir.name());

  // strip first line before checking if needed
  if (std::mismatch(file_log_txt.begin(), file_log_txt.end(), prefix.begin(),
                    prefix.end())
          .second == prefix.end()) {
    file_log_txt.erase(0, file_log_txt.find("\n") + 1);
  }

  if (test_params.filelog_expected_level != LogLevel::kNotSet) {
    std::vector<std::string> lines;
    std::istringstream ss(file_log_txt);
    for (std::string line; std::getline(ss, line);) {
      lines.push_back(line);
    }

    auto regex = ts_regex(test_params.filelog_expected_timestamp_precision);
    ASSERT_FALSE(regex.empty());

    EXPECT_THAT(lines, ::testing::Contains(::testing::ContainsRegex(regex)));
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

INSTANTIATE_TEST_SUITE_P(
    Spec, RouterLoggingTestTimestampPrecisionConfig,
    ::testing::Values(
        // no logger section, no sinks sections
        // logging_folder not empty so we are expected to log to the file
        // with a warning level so info and debug logs will not be there
        LoggingConfigOkParams(
            "0",  // testname
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
        /*TS_FR1_1.1*/
        LoggingConfigOkParams(
            "1",  // testname
            TS_FR1_1_STR("second"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec),
        /*TS_FR1_1.2*/
        LoggingConfigOkParams(
            "2",  // testname
            TS_FR1_1_STR("Second"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec),
        /*TS_FR1_1.3*/
        LoggingConfigOkParams(
            "3",  // testname
            TS_FR1_1_STR("sec"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec),
        /*TS_FR1_1.4*/
        LoggingConfigOkParams(
            "4",  // testname
            TS_FR1_1_STR("SEC"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec),
        /*TS_FR1_1.5*/
        LoggingConfigOkParams(
            "5",  // testname
            TS_FR1_1_STR("s"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec),
        /*TS_FR1_1.6*/
        LoggingConfigOkParams(
            "6",  // testname
            TS_FR1_1_STR("S"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kSec),
        /*** TS_FR1_2 ***/
        /*TS_FR1_2.1*/
        LoggingConfigOkParams(
            "7",  // testname
            TS_FR1_2_STR("millisecond"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec),
        /*TS_FR1_2.2*/
        LoggingConfigOkParams(
            "8",  // testname
            TS_FR1_2_STR("MILLISECOND"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec),
        /*TS_FR1_2.3*/
        LoggingConfigOkParams(
            "9",  // testname
            TS_FR1_2_STR("msec"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec),
        /*TS_FR1_2.4*/
        LoggingConfigOkParams(
            "10",  // testname
            TS_FR1_2_STR("MSEC"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec),
        /*TS_FR1_2.5*/
        LoggingConfigOkParams(
            "11",  // testname
            TS_FR1_2_STR("ms"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec),
        /*TS_FR1_2.6*/
        LoggingConfigOkParams(
            "12",  // testname
            TS_FR1_2_STR("MS"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMilliSec),
        /*** TS_FR1_3 ***/
        /*TS_FR1_3.1*/
        LoggingConfigOkParams(
            "13",  // testname
            TS_FR1_3_STR("microsecond"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec),
        /*TS_FR1_3.2*/
        LoggingConfigOkParams(
            "14",  // testname
            TS_FR1_3_STR("Microsecond"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec),
        /*TS_FR1_3.3*/
        LoggingConfigOkParams(
            "15",  // testname
            TS_FR1_3_STR("usec"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec),
        /*TS_FR1_3.4*/
        LoggingConfigOkParams(
            "16",  // testname
            TS_FR1_3_STR("UsEC"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec),
        /*TS_FR1_3.5*/
        LoggingConfigOkParams(
            "17",  // testname
            TS_FR1_3_STR("us"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec),
        /*TS_FR1_3.5*/
        LoggingConfigOkParams(
            "18",  // testname
            TS_FR1_3_STR("US"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kMicroSec),
        /*** TS_FR1_4 ***/
        /*TS_FR1_4.1*/
        LoggingConfigOkParams(
            "19",  // testname
            TS_FR1_3_STR("nanosecond"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec),
        /*TS_FR1_4.2*/
        LoggingConfigOkParams(
            "20",  // testname
            TS_FR1_3_STR("NANOSECOND"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec),
        /*TS_FR1_4.3*/
        LoggingConfigOkParams(
            "21",  // testname
            TS_FR1_3_STR("nsec"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec),
        /*TS_FR1_4.4*/
        LoggingConfigOkParams(
            "22",  // testname
            TS_FR1_3_STR("nSEC"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec),
        /*TS_FR1_4.5*/
        LoggingConfigOkParams(
            "23",  // testname
            TS_FR1_3_STR("ns"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec),
        /*TS_FR1_4.6*/
        LoggingConfigOkParams(
            "24",  // testname
            TS_FR1_3_STR("NS"),
            /* logging_folder_empty = */ false,
            /* consolelog_expected_level =  */ LogLevel::kDebug,
            /* filelog_expected_level =  */ LogLevel::kDebug,
            /* consolelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec,
            /* filelog_expected_timestamp_precision = */
            LogTimestampPrecision::kNanoSec),
        /*TS_FR4_2*/
        LoggingConfigOkParams(
            "25",  // testname
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
        /*TS_FR4_3*/
        LoggingConfigOkParams(
            "26",  // testname
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
            LogTimestampPrecision::kSec)),
    [](auto const &info) { return info.param.test_name; });

INSTANTIATE_TEST_SUITE_P(
    Failures, RouterLoggingConfigError,
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
            "Valid values are: second, sec, s, millisecond, msec, ms, "
            "microsecond, usec, us, nanosecond, nsec, and ns"),
        // Unknown timestamp_precision value in the [logger] section
        /*1*/ /*TS_FR3_1*/
        LoggingConfigErrorParams(
            "[logger]\n"
            "sinks=consolelog,filelog\n"
            "timestamp_precision=unknown\n",
            /* logging_folder_empty = */ false,
            /* expected_error =  */
            "Configuration error: Timestamp precision 'unknown' is not valid. "
            "Valid values are: second, sec, s, millisecond, msec, ms, "
            "microsecond, usec, us, nanosecond, nsec, and ns"),
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
INSTANTIATE_TEST_SUITE_P(
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
INSTANTIATE_TEST_SUITE_P(
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
                    // guarantees the limit would be exceeded

  // launch the router in bootstrap mode
  auto &router = launch_router_for_fail({
      "--bootstrap=127.0.0.1:" + std::to_string(server_port),
      "--conf-set-option=DEFAULT.plugin_folder=" +
          ProcessManager::get_plugin_dir().str(),
      "--name",
      name,
      "-d",
      bootstrap_dir.name(),
  });

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
 * @test verify that debug logs are not written to console during bootstrap if
 * bootstrap configuration file is not provided.
 */
TEST_F(RouterLoggingTest, is_debug_logs_disabled_if_no_bootstrap_config_file) {
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  TempDirectory bootstrap_dir;

  const auto server_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();

  // launch mock server and wait for it to start accepting connections
  /*auto &server_mock =*/launch_mysql_server_mock(
      json_stmts, server_port, EXIT_SUCCESS, false, http_port);
  set_mock_metadata(http_port, "00000000-0000-0000-0000-0000000000g1",
                    classic_ports_to_gr_nodes({server_port}), 0, {server_port});

  // ASSERT_NO_FATAL_FAILURE(check_port_ready(server_mock, server_port));

  // launch the router in bootstrap mode
  auto &router = launch_router_for_bootstrap(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "-d",
          bootstrap_dir.name(),
      },
      EXIT_SUCCESS);

  // check if the bootstrapping was successful
  check_exit_code(router, EXIT_SUCCESS);
  EXPECT_THAT(router.get_full_output(),
              testing::Not(testing::HasSubstr("SELECT ")));
}

/**
 * @test verify that debug logs are written to console during bootstrap if
 * log_level is set to DEBUG in bootstrap configuration file.
 */
TEST_F(RouterLoggingTest, is_debug_logs_enabled_if_bootstrap_config_file) {
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  TempDirectory bootstrap_dir;
  TempDirectory bootstrap_conf;

  const auto server_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();

  launch_mysql_server_mock(json_stmts, server_port, EXIT_SUCCESS, false,
                           http_port);
  set_mock_metadata(http_port, "00000000-0000-0000-0000-0000000000g1",
                    classic_ports_to_gr_nodes({server_port}), 0, {server_port});

  // launch the router in bootstrap mode
  std::string logger_section = "[logger]\nlevel = DEBUG\n";
  auto conf_params = get_DEFAULT_defaults();
  // we want to log to the console
  conf_params["logging_folder"] = "";
  std::string conf_file = ProcessManager::create_config_file(
      bootstrap_conf.name(), logger_section, &conf_params, "bootstrap.conf", "",
      false);

  auto &router = launch_router_for_bootstrap(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--force",
          "-d",
          bootstrap_dir.name(),
          "-c",
          conf_file,
      },
      EXIT_SUCCESS);

  // check if the bootstrapping was successful
  check_exit_code(router, EXIT_SUCCESS);

  // check if log output contains the SQL queries.
  //
  // SQL queries are logged with host:port at the start.
  EXPECT_THAT(router.get_full_output(),
              testing::HasSubstr("127.0.0.1:" + std::to_string(server_port)));
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
  const auto http_port = port_pool_.get_next_available();

  /*auto &server_mock =*/launch_mysql_server_mock(
      json_stmts, server_port, EXIT_SUCCESS, false, http_port);
  set_mock_metadata(http_port, "00000000-0000-0000-0000-0000000000g1",
                    classic_ports_to_gr_nodes({server_port}), 0, {server_port});

  // create config with logging_folder set to that directory
  std::map<std::string, std::string> params = {{"logging_folder", ""}};
  params.at("logging_folder") = bootstrap_conf.name();
  TempDirectory conf_dir("conf");
  const std::string conf_file =
      create_config_file(conf_dir.name(), "[logger]\nlevel = DEBUG\n", &params);

  auto &router = launch_router_for_bootstrap(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--force",
          "-d",
          bootstrap_dir.name(),
          "-c",
          conf_file,
      },
      EXIT_SUCCESS);

  // check if the bootstrapping was successful
  check_exit_code(router, EXIT_SUCCESS);

  // check if log output contains the SQL queries.
  //
  // SQL queries are logged with host:port at the start.
  auto file_content =
      router.get_logfile_content("mysqlrouter.log", bootstrap_conf.name());
  auto lines = mysql_harness::split_string(file_content, '\n');

  EXPECT_THAT(lines, ::testing::Contains(::testing::HasSubstr(
                         "127.0.0.1:" + std::to_string(server_port))));
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
  const auto http_port = port_pool_.get_next_available();

  /*auto &server_mock =*/launch_mysql_server_mock(
      json_stmts, server_port, EXIT_SUCCESS, false, http_port);
  set_mock_metadata(http_port, "00000000-0000-0000-0000-0000000000g1",
                    classic_ports_to_gr_nodes({server_port}), 0, {server_port});

  // launch the router in bootstrap mode
  std::string logger_section = "[logger]\nlevel = DEBUG\n";
  auto conf_params = get_DEFAULT_defaults();
  // we want to log to the console
  conf_params["logging_folder"] = "";
  std::string conf_file = ProcessManager::create_config_file(
      bootstrap_conf.name(), logger_section, &conf_params, "bootstrap.conf", "",
      false);

  auto &router = launch_router_for_bootstrap(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port),
          "--force",
          "-d",
          bootstrap_dir.name(),
          "-c",
          conf_file,
      },
      EXIT_SUCCESS, true, true, /*catch_sterr=*/false);

  // check if the bootstrapping was successful
  check_exit_code(router, EXIT_SUCCESS);

  // check if logs are not written to output
  EXPECT_THAT(router.get_full_output(),
              testing::Not(testing::HasSubstr("SELECT ")));

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
    router_port_ = port_pool_.get_next_available();
    metadata_cache_section = get_metadata_cache_section();
    routing_section =
        get_metadata_cache_routing_section("PRIMARY", "round-robin");
  }

  std::string get_static_routing_section() {
    return mysql_harness::ConfigBuilder::build_section(
        "routing:test_default", {
                                    {"bind_port", std::to_string(router_port_)},
                                    {"destinations", "127.0.0.1"},
                                    {"routing_strategy", "first-available"},
                                });
  }

  std::string get_metadata_cache_section() {
    return mysql_harness::ConfigBuilder::build_section(
        "metadata_cache:test",
        {
            {"router_id", "1"},
            {"user", "mysql_router1_user"},
            {"metadata_cluster", "test"},
            {"connect_timeout", "1"},
            {"ttl", std::to_string(static_cast<double>(ttl_.count()) / 1000)},
        });
  }

  std::string get_metadata_cache_routing_section(const std::string &role,
                                                 const std::string &strategy) {
    std::vector<std::pair<std::string, std::string>> options{
        {"bind_port", std::to_string(router_port_)},
        {"destinations", "metadata-cache://test/default?role=" + role},
        {"protocol", "classic"},
    };

    if (!strategy.empty()) options.emplace_back("routing_strategy", strategy);

    return mysql_harness::ConfigBuilder::build_section("routing:test_default",
                                                       options);
  }

  std::string init_keyring_and_config_file(const std::string &conf_dir,
                                           bool log_to_console) {
    return init_keyring_and_config_file(
        conf_dir, metadata_cache_section + "\n" + routing_section,
        log_to_console);
  }

  std::string init_keyring_and_config_file(const std::string &conf_dir) {
    return init_keyring_and_config_file(conf_dir, false);
  }

  std::string init_keyring_and_config_file(const std::string &conf_dir,
                                           const std::string &config) {
    return init_keyring_and_config_file(conf_dir, config, false);
  }

  std::string init_keyring_and_config_file(const std::string &conf_dir,
                                           const std::string &config,
                                           bool log_to_console) {
    auto default_section = get_DEFAULT_defaults();
    init_keyring(default_section, temp_test_dir.name());

    const auto state_file = create_state_file(
        get_test_temp_dir_name(),
        create_state_file_content("uuid", "", cluster_nodes_ports, 0));
    default_section["dynamic_state"] = state_file;

    default_section["logging_folder"] =
        log_to_console ? "" : get_logging_dir().str();
    const std::string sinks =
        (log_to_console ? "consolelog,"s : "") + "filelog";
    return create_config_file(conf_dir,
                              mysql_harness::ConfigBuilder::build_section(
                                  "logger",
                                  {
                                      {"level", "DEBUG"},
                                      {"timestamp_precision", "millisecond"},
                                      {"sinks", sinks},
                                  }) +
                                  "\n" + config,
                              &default_section);
  }

  TempDirectory temp_test_dir;
  std::vector<uint16_t> cluster_nodes_ports;
  std::vector<uint16_t> cluster_nodes_http_ports;
  uint16_t router_port_;
  std::string metadata_cache_section;
  std::string routing_section;
  const std::chrono::milliseconds ttl_{200};
};

template <class F>
bool retry_for(F &&f, std::chrono::milliseconds duration) {
  using clock_type = std::chrono::steady_clock;

  auto sleep_time = duration / 20;
  auto end_time = clock_type::now() + duration;

  do {
    auto res = f();

    if (res) return true;

    RouterComponentTest::sleep_for(sleep_time);
  } while (clock_type::now() < end_time);

  return false;
}

/**
 * @test verify if error message is logged if router cannot connect to any
 *       metadata server.
 */
TEST_F(MetadataCacheLoggingTest,
       log_error_when_cannot_connect_to_any_metadata_server) {
  TempDirectory conf_dir;

  // launch the router with metadata-cache configuration
  auto &router =
      launch_router({"-c", init_keyring_and_config_file(conf_dir.name())},
                    EXIT_SUCCESS,  // expected-exit-code
                    false,         // catch-stderr
                    false,         // with-sudo
                    -1s            // wait-ready
      );

  // expect something like this to appear on STDERR
  // 2017-12-21 17:22:35 metadata_cache ERROR [7ff0bb001700] Failed connecting
  // with any of the 3 metadata servers
  const auto fail_msg =
      "Failed fetching metadata from any of the 3 metadata servers.";

  // Log as error only once
  const auto error_timestamp = get_log_timestamp(
      router.get_logfile_path(),
      std::string{".*metadata_cache ERROR.*"} + fail_msg, 1, 20 * ttl_);
  EXPECT_TRUE(error_timestamp);
  EXPECT_FALSE(get_log_timestamp(
      router.get_logfile_path(),
      std::string{".*metadata_cache ERROR.*"} + fail_msg, 2, 5 * ttl_));
  // After logging an error next logs should be debug (unless the server state
  // changes)
  const auto debug_timestamp = get_log_timestamp(
      router.get_logfile_path(),
      std::string{".*metadata_cache DEBUG.*"} + fail_msg, 1, 20 * ttl_);
  EXPECT_TRUE(debug_timestamp);
  EXPECT_GT(debug_timestamp.value(), error_timestamp.value());

  // Launch metadata server
  const auto http_port = cluster_nodes_http_ports[0];
  auto &server = launch_mysql_server_mock(
      get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str(),
      cluster_nodes_ports[0], EXIT_SUCCESS, false, http_port);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(server, cluster_nodes_ports[0]));
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
  set_mock_metadata(http_port, "uuid",
                    classic_ports_to_gr_nodes(cluster_nodes_ports), 0,
                    classic_ports_to_cluster_nodes(cluster_nodes_ports));
  wait_for_transaction_count_increase(http_port);

  // We report to log info that we have connected only if there was an error,
  // otherwise those reports should be treated as debug
  const auto connect_msg = "Connected with metadata server";
  EXPECT_TRUE(get_log_timestamp(
      router.get_logfile_path(),
      std::string{".*metadata_cache INFO.*"} + connect_msg, 1, 20 * ttl_));
  EXPECT_FALSE(get_log_timestamp(
      router.get_logfile_path(),
      std::string{".*metadata_cache INFO.*"} + connect_msg, 3, 5 * ttl_));
  EXPECT_TRUE(get_log_timestamp(
      router.get_logfile_path(),
      std::string{".*metadata_cache DEBUG.*"} + connect_msg, 1, 20 * ttl_));

  server.send_clean_shutdown_event();
  server.wait_for_exit();
  std::this_thread::sleep_for(ttl_);
  // Log error after server was shut down
  EXPECT_TRUE(get_log_timestamp(
      router.get_logfile_path(),
      std::string{".*metadata_cache ERROR.*"} + fail_msg, 2, 80 * ttl_));
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
  set_mock_metadata(http_port, "uuid",
                    classic_ports_to_gr_nodes(cluster_nodes_ports), 1,
                    classic_ports_to_cluster_nodes(cluster_nodes_ports));

  // launch the router with metadata-cache configuration
  auto &router = ProcessManager::launch_router(
      {"-c", init_keyring_and_config_file(conf_dir.name())}, EXIT_SUCCESS, true,
      false, -1s);

  // expect something like this to appear on STDERR:
  //
  // - ... metadata_cache WARNING ... Failed connecting with Metadata Server
  //   127.0.0.1:7002: Can't connect to MySQL server on '127.0.0.1' (111) (2003)
  // - ... metadata_cache WARNING ... While updating metadata, could ...
  const auto connection_failed_msg =
      "Failed connecting with Metadata Server 127\\.0\\.0\\.1:" +
      std::to_string(cluster_nodes_ports[0]);
  const auto update_failed_msg =
      "While updating metadata, could not establish a connection to cluster "
      "'test' through .*" +
      std::to_string(cluster_nodes_ports[0]);

  EXPECT_TRUE(get_log_timestamp(
      router.get_logfile_path(),
      std::string{".*metadata_cache WARNING.*"} + update_failed_msg, 1,
      20 * ttl_));
  EXPECT_TRUE(get_log_timestamp(
      router.get_logfile_path(),
      std::string{".*metadata_cache WARNING.*"} + connection_failed_msg, 1,
      20 * ttl_));
  EXPECT_FALSE(get_log_timestamp(
      router.get_logfile_path(),
      std::string{".*metadata_cache WARNING.*"} + connection_failed_msg, 2,
      5 * ttl_));
  EXPECT_TRUE(get_log_timestamp(
      router.get_logfile_path(),
      std::string{".*metadata_cache DEBUG.*"} + connection_failed_msg, 1,
      20 * ttl_));

  server.send_clean_shutdown_event();
  server.wait_for_exit();

  auto &new_server = launch_mysql_server_mock(
      get_data_dir().join("metadata_dynamic_nodes_v2_gr.js").str(),
      cluster_nodes_ports[0], EXIT_SUCCESS, false, cluster_nodes_http_ports[0]);
  ASSERT_NO_FATAL_FAILURE(check_port_ready(new_server, cluster_nodes_ports[0]));
  EXPECT_TRUE(MockServerRestClient(cluster_nodes_http_ports[0])
                  .wait_for_rest_endpoint_ready());
  set_mock_metadata(cluster_nodes_http_ports[0], "uuid",
                    classic_ports_to_gr_nodes(cluster_nodes_ports), 0,
                    classic_ports_to_cluster_nodes(cluster_nodes_ports));
  wait_for_transaction_count_increase(cluster_nodes_http_ports[0]);

  const auto connect_msg = "Connected with metadata server running on .*" +
                           std::to_string(cluster_nodes_ports[0]);
  EXPECT_TRUE(get_log_timestamp(
      router.get_logfile_path(),
      std::string{".*metadata_cache INFO.*"} + connect_msg, 1, 20 * ttl_));
  EXPECT_TRUE(get_log_timestamp(
      router.get_logfile_path(),
      std::string{".*metadata_cache DEBUG.*"} + connect_msg, 1, 20 * ttl_));
}

#ifndef _WIN32

/**
 * @test Checks that the logs rotation works (meaning Router will recreate
 * its log file when it was moved and HUP signal was sent to the Router).
 */
TEST_F(MetadataCacheLoggingTest, log_rotation_by_HUP_signal) {
  TempDirectory conf_dir;

  // launch the router with metadata-cache configuration
  auto &router =
      launch_router({"-c", init_keyring_and_config_file(
                               conf_dir.name(), get_static_routing_section())},
                    EXIT_SUCCESS);

  auto logging_dir = get_logging_dir();
  auto log_file = Path(logging_dir).join("mysqlrouter.log");

  EXPECT_TRUE(retry_for([&log_file]() { return log_file.exists(); }, 1000ms));

  // now let's simulate what logrotate script does
  // move the log_file appending '.1' to its name
  auto log_file_1 = Path(logging_dir).join("mysqlrouter.log.1");

  mysqlrouter::rename_file(log_file.str(), log_file_1.str());
  ::kill(router.get_pid(), SIGHUP);

  // let's wait until something new gets logged (metadata cache TTL has
  // expired), to be sure the default file that we moved is back.
  // Now both old and new files should exist
  EXPECT_TRUE(retry_for([&log_file]() { return log_file.exists(); }, 1000ms));

  EXPECT_TRUE(log_file.exists()) << router.get_logfile_content();
  EXPECT_TRUE(log_file_1.exists());
}

/**
 * @test Checks that the Router continues to log to the file when the
 * SIGHUP gets sent to it and no file replacement is done.
 */
TEST_F(MetadataCacheLoggingTest, log_rotation_by_HUP_signal_no_file_move) {
  TempDirectory conf_dir;

  // launch router with metadata-cache configuration to get a changes in the
  // logfile every once and a while
  auto &router =
      router_spawner()
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::RUNNING)
          .expected_exit_code(EXIT_SUCCESS)
          .spawn({"-c", init_keyring_and_config_file(conf_dir.name())});

  auto logging_dir = get_logging_dir();
  auto log_file = Path(logging_dir).join("mysqlrouter.log");

  ASSERT_TRUE(retry_for([&log_file]() { return log_file.exists(); }, 1000ms));

  // grab the current log content
  const std::string log_content = router.get_logfile_content();

  // send the log-rotate signal
  ::kill(router.get_pid(), SIGHUP);

  // wait until something new gets logged;
  std::string log_content_2;

  EXPECT_TRUE(retry_for(
      [log_content, &log_content_2, &router]() {
        log_content_2 = router.get_logfile_content();

        return log_content != log_content_2;
      },
      2000ms));

  // The logfile should still exist
  EXPECT_TRUE(log_file.exists());
  // It should still contain what was there before and more (Router should keep
  // logging)
  EXPECT_THAT(log_content_2, StartsWith(log_content));
  EXPECT_STRNE(log_content_2.c_str(), log_content.c_str());
}

/**
 * @test Checks that the log file will be recreated after a router restart.
 */
TEST_F(MetadataCacheLoggingTest, log_rotation_when_router_restarts) {
  TempDirectory conf_dir;

  auto &router =
      launch_router({"-c", init_keyring_and_config_file(
                               conf_dir.name(), get_static_routing_section())},
                    EXIT_SUCCESS);

  auto log_file = get_logging_dir();
  log_file.append("mysqlrouter.log");

  EXPECT_TRUE(retry_for([&log_file]() { return log_file.exists(); }, 500ms));

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
  launch_router({"-c", init_keyring_and_config_file(
                           conf_dir.name(), get_static_routing_section())},
                EXIT_SUCCESS);

  EXPECT_TRUE(retry_for([&log_file]() { return log_file.exists(); }, 500ms));
}

/**
 * @test Checks that sending SIGHUP when the log file is read only results in a
 * failure.
 */
TEST_F(MetadataCacheLoggingTest, log_rotation_read_only) {
  TempDirectory conf_dir;

  SCOPED_TRACE("// launch the router with static routing configuration");
  auto &router =
      launch_router({"-c", init_keyring_and_config_file(
                               conf_dir.name(), get_static_routing_section())},
                    EXIT_FAILURE);

  auto logging_dir = get_logging_dir();
  auto log_file = Path(logging_dir).join("mysqlrouter.log");

  SCOPED_TRACE("// wait for logfile " + log_file.str() + " to appear");

  EXPECT_TRUE(retry_for([log_file]() { return log_file.exists(); }, 500ms));

  SCOPED_TRACE("// move the log_file appending '.1' to its name");
  auto log_file_1 = Path(logging_dir).join("mysqlrouter.log.1");
  mysqlrouter::rename_file(log_file.str(), log_file_1.str());

  SCOPED_TRACE("// 'manually' recreate the log file and make it read only");
  {
    std::ofstream logf(log_file.str());
    EXPECT_TRUE(logf.good());
  }
  EXPECT_TRUE(retry_for([log_file]() { return log_file.exists(); }, 500ms));
  chmod(log_file.c_str(), S_IRUSR);

  const auto pid = router.get_pid();
  SCOPED_TRACE("// send the log-rotate signal to PID " + std::to_string(pid));
  ::kill(pid, SIGHUP);

  SCOPED_TRACE("// we expect the router to exit");
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

  auto default_section = get_DEFAULT_defaults();

  // send log to stderr
  default_section["logging_folder"] = "";

  const auto config = mysql_harness::join(
      std::vector<std::string>{
          mysql_harness::ConfigBuilder::build_section("logger",
                                                      {{"level", "DEBUG"}}),
          mysql_harness::ConfigBuilder::build_section("io", {{"threads", "1"}}),
          get_static_routing_section()},
      "\n");

  auto &router = launch_router(
      {"-c", create_config_file(conf_dir.name(), config, &default_section)},
      EXIT_SUCCESS);

  // send SIGHUP, should have no impact.
  ::kill(router.get_pid(), SIGHUP);

  // wait a bit for the router handle the signal
  RouterComponentTest::sleep_for(200ms);
}

#endif

/**************************************************/
/* Tests for valid logger filename configurations */
/**************************************************/

#define DEFAULT_LOGFILE_NAME "mysqlrouter.log"
#define USER_LOGFILE_NAME "foo.log"
#define USER_LOGFILE_NAME_2 "bar.log"

struct LoggingConfigFilenameOkParams {
  const std::string logger_config;
  const std::string filename;
  const bool console_to_stderr;

  LoggingConfigFilenameOkParams(std::string logger_config_,
                                std::string filename_)
      : logger_config(std::move(logger_config_)),
        filename(std::move(filename_)),
        console_to_stderr(true) {}

  LoggingConfigFilenameOkParams(std::string logger_config_,
                                std::string filename_, bool console_to_stderr_)
      : logger_config(std::move(logger_config_)),
        filename(std::move(filename_)),
        console_to_stderr(console_to_stderr_) {}
};

class RouterLoggingTestConfigFilename
    : public RouterLoggingTest,
      public ::testing::WithParamInterface<LoggingConfigFilenameOkParams> {};

/** @test This test verifies that a proper log filename is written to
 * for various sinks/filename combinations.
 */
TEST_P(RouterLoggingTestConfigFilename, LoggingTestConfigFilename) {
  auto test_params = GetParam();

  TempDirectory tmp_dir;
  auto conf_params = get_DEFAULT_defaults();
  conf_params["logging_folder"] = tmp_dir.name();

  TempDirectory conf_dir("conf");
  const std::string conf_text =
      "[routertestplugin_logger]\n\n" + test_params.logger_config;
  const std::string conf_file =
      create_config_file(conf_dir.name(), conf_text, &conf_params);

  auto &router = ProcessManager::launch_router({"-c", conf_file});
  router.send_clean_shutdown_event();
  check_exit_code(router, EXIT_SUCCESS);

  // check the file log if it contains what's expected
  const std::string file_log_txt =
      router.get_logfile_content(test_params.filename, tmp_dir.name());

  // check the routertestplugin_logger's message is in the logfile.
  EXPECT_THAT(file_log_txt, HasSubstr("I'm a system message"))
      << "\file_log_txt:\n"
      << file_log_txt;
}

INSTANTIATE_TEST_SUITE_P(
    LoggingTestConfigFilename, RouterLoggingTestConfigFilename,
    ::testing::Values(
        // default filename in logger section
        /*0*/
        LoggingConfigFilenameOkParams("[logger]\n"
                                      "filename=" DEFAULT_LOGFILE_NAME "\n",
                                      DEFAULT_LOGFILE_NAME),
        // TS_FR01_01 user defined logfile name in logger section
        /*1*/
        LoggingConfigFilenameOkParams("[logger]\n"
                                      "filename=" USER_LOGFILE_NAME "\n",
                                      USER_LOGFILE_NAME),
        // TS_FR01_02 user defined logfile name in filelog sink
        /*2*/
        LoggingConfigFilenameOkParams("[logger]\n"
                                      "sinks=filelog\n"
                                      "[filelog]\n"
                                      "filename=" USER_LOGFILE_NAME "\n",
                                      USER_LOGFILE_NAME),
        // TS_FR04_09 user defined logfile name in filelog sink overrides user
        // defined logfile name in logger section
        /*3*/
        LoggingConfigFilenameOkParams("[logger]\n"
                                      "sinks=filelog\n"
                                      "filename=" USER_LOGFILE_NAME "\n"
                                      "[filelog]\n"
                                      "filename=" USER_LOGFILE_NAME_2 "\n",
                                      USER_LOGFILE_NAME_2),
        // TS_FR05_01 empty logger filename logs to default logfile name
        /*4*/
        LoggingConfigFilenameOkParams("[logger]\n"
                                      "filename=\n",
                                      DEFAULT_LOGFILE_NAME),
        // TS_FR05_02 empty filelog filename logs to default logfile name
        /*5*/
        LoggingConfigFilenameOkParams("[logger]\n"
                                      "sinks=filelog\n"
                                      "[filelog]\n"
                                      "filename=\n",
                                      DEFAULT_LOGFILE_NAME),
        // TS_FR04_11 empty filelog filename logs to userdefined logger filename
        /*6*/
        LoggingConfigFilenameOkParams("[logger]\n"
                                      "filename=" USER_LOGFILE_NAME "\n"
                                      "sinks=filelog\n"
                                      "[filelog]\n"
                                      "filename=\n",
                                      USER_LOGFILE_NAME),
        // TS_FR04_12 undefined filelog filename logs to userdefined value for
        // logger filename
        /*7*/
        LoggingConfigFilenameOkParams("[logger]\n"
                                      "filename=" USER_LOGFILE_NAME "\n"
                                      "sinks=filelog\n"
                                      "[filelog]\n",
                                      USER_LOGFILE_NAME),
        // user defined logfile name in filelog sink overrides logger section
        /*8*/
        LoggingConfigFilenameOkParams("[logger]\n"
                                      "sinks=filelog\n"
                                      "filename=" DEFAULT_LOGFILE_NAME "\n"
                                      "[filelog]\n"
                                      "filename=" USER_LOGFILE_NAME "\n",
                                      USER_LOGFILE_NAME),
        // TS_FR04_01 empty filename has no effect
        /*9*/
        LoggingConfigFilenameOkParams("[logger]\n"
                                      "sinks=filelog\n"
                                      "filename=\n"
                                      "[filelog]\n"
                                      "filename=" USER_LOGFILE_NAME_2 "\n",
                                      USER_LOGFILE_NAME_2),
        // TS_FR04_03 empty filenames has no effect, and logs to default
        /*10*/
        LoggingConfigFilenameOkParams("[logger]\n"
                                      "sinks=filelog\n"
                                      "filename=\n"
                                      "[filelog]\n"
                                      "filename=\n",
                                      DEFAULT_LOGFILE_NAME),
        // TS_FR04_04 no filenames results in logging to default
        /*11*/
        LoggingConfigFilenameOkParams("[logger]\n"
                                      "sinks=filelog\n"
                                      "[filelog]\n",
                                      DEFAULT_LOGFILE_NAME)));

#define NOT_USED ""

#ifndef _WIN32
#define NULL_DEVICE_NAME "/dev/null"
#define STDOUT_DEVICE_NAME "/dev/stdout"
#define STDERR_DEVICE_NAME "/dev/stderr"
#else
#define NULL_DEVICE_NAME "NUL"
#define STDOUT_DEVICE_NAME "CON"
// No STDERR equivalent for WIN32
#endif

class RouterLoggingTestConfigFilenameDevices
    : public RouterLoggingTest,
      public ::testing::WithParamInterface<LoggingConfigFilenameOkParams> {};

/** @test This test verifies that consolelog destination may be set to various
 * devices
 */
TEST_P(RouterLoggingTestConfigFilenameDevices,
       LoggingTestConsoleDestinationDevices) {
  // FIXME: Unfortunately due to the limitations of our component testing
  // framework, this test has a flaw: it is not possible to distinguish if the
  // output returned from router.get_full_output() appeared on STDERR or STDOUT.
  // This should be fixed in the future.
  auto test_params = GetParam();
  bool console_empty =
      (test_params.filename.compare(NULL_DEVICE_NAME) == 0 ? true : false);

  Path destination(test_params.filename);
#ifndef _WIN32
  EXPECT_TRUE(destination.exists());
#endif

  TempDirectory tmp_dir;
  auto conf_params = get_DEFAULT_defaults();
  conf_params["logging_folder"] = tmp_dir.name();

  TempDirectory conf_dir("conf");
  const std::string conf_text =
      "[routing]\n"
      "\n"
      "[logger]\n"
      "sinks=consolelog\n"
      "[consolelog]\n"
      "destination=" +
      destination.str();
  const std::string conf_file =
      create_config_file(conf_dir.name(), conf_text, &conf_params);

  // empty routing section results in a failure, but while logging to file
  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE,
                               test_params.console_to_stderr, false, -1s);
  check_exit_code(router, EXIT_FAILURE);

  const std::string console_log_txt = router.get_full_output();
  if (console_empty) {
    // Expect the console log to be empty
    EXPECT_TRUE(console_log_txt.empty()) << "\nconsole:\n" << console_log_txt;
  } else {
    // Expect the console log to not be empty
    EXPECT_TRUE(!console_log_txt.empty()) << "\nconsole:\n" << console_log_txt;
  }

  // expect no default router file created in the logging folder
  Path shouldnotexist = Path(tmp_dir.name()).join(DEFAULT_LOGFILE_NAME);
  EXPECT_FALSE(shouldnotexist.exists());
  shouldnotexist = Path("/dev").join(DEFAULT_LOGFILE_NAME);
  EXPECT_FALSE(shouldnotexist.exists());

#ifndef _WIN32
  EXPECT_TRUE(destination.exists());
#endif
}

INSTANTIATE_TEST_SUITE_P(
    LoggingTestConsoleDestinationDevices,
    RouterLoggingTestConfigFilenameDevices,
    ::testing::Values(
        // TS_FR07_03 consolelog destination /dev/null
        /*0*/
        LoggingConfigFilenameOkParams(NOT_USED, NULL_DEVICE_NAME, true),
        // TS_FR07_01 consolelog destination /dev/stdout
        /*1*/
        LoggingConfigFilenameOkParams(NOT_USED, STDOUT_DEVICE_NAME, false)));

#ifndef _WIN32
INSTANTIATE_TEST_SUITE_P(
    LoggingTestConsoleDestinationDevicesUnix,
    RouterLoggingTestConfigFilenameDevices,
    ::testing::Values(
        // TS_FR07_02 consolelog destination /dev/stderr
        /*0*/
        LoggingConfigFilenameOkParams(NOT_USED, STDERR_DEVICE_NAME, true)));
#endif

struct LoggingConfigFilenameErrorParams {
  std::string logger_config;
  std::string filename;
  bool create_file;
  std::string expected_error;

  LoggingConfigFilenameErrorParams(const std::string &logger_config_,
                                   const std::string filename_,
                                   bool create_file_,
                                   const std::string expected_error_)
      : logger_config(logger_config_),
        filename(filename_),
        create_file(create_file_),
        expected_error(expected_error_) {}
};

class RouterLoggingConfigFilenameError
    : public RouterLoggingTest,
      public ::testing::WithParamInterface<LoggingConfigFilenameErrorParams> {};

#define ABS_PATH "%%ABSPATH%%"
#define ABS_DIR "%%ABSDIR%%"
#define REL_PATH "%%RELPATH%%"
#define REL_DIR "%%RELDIR%%"
#define FILENAME "%%FILENAME%%"

/** @test This test verifies that absolute and relative filenames are rejected
 * in filename option for various sinks/filename combinations.
 */
TEST_P(RouterLoggingConfigFilenameError, LoggingConfigAbsRelFilenameError) {
  auto test_params = GetParam();

  TempDirectory tmp_dir;

  // create the absolute and relative paths (note: order)
  Path abs_dir = Path(tmp_dir.name()).real_path();
  Path abs_path = abs_dir.join(test_params.filename);
  Path rel_path = Path(tmp_dir.name()).basename().join(test_params.filename);

  auto conf_params = get_DEFAULT_defaults();
  conf_params["logging_folder"] = abs_dir.str();

  // Create tmp_file once the tmp_dir is created. Removed by tmp_dir dtor.
  if (test_params.create_file) {
    std::ofstream myfile_;
    myfile_.open(abs_path.str());
    if (myfile_.is_open()) {
      myfile_ << "Temporary file created by router test ...\n";
      myfile_.flush();
      myfile_.close();
    }
    EXPECT_TRUE(abs_path.exists());
  }

  // replace the pattern in config where applicable
  std::string cfg = "[keepalive]\n\n" + test_params.logger_config;
  while (cfg.find(FILENAME) != std::string::npos) {
    cfg.replace(cfg.find(FILENAME), sizeof(FILENAME) - 1,
                test_params.filename.c_str());
  }
  while (cfg.find(ABS_PATH) != std::string::npos) {
    cfg.replace(cfg.find(ABS_PATH), sizeof(ABS_PATH) - 1, abs_path.c_str());
  }
  while (cfg.find(ABS_DIR) != std::string::npos) {
    cfg.replace(cfg.find(ABS_DIR), sizeof(ABS_DIR) - 1, abs_dir.c_str());
  }
  while (cfg.find(REL_PATH) != std::string::npos) {
    cfg.replace(cfg.find(REL_PATH), sizeof(REL_PATH) - 1, rel_path.c_str());
  }

  TempDirectory conf_dir("conf");
  const std::string conf_file =
      create_config_file(conf_dir.name(), cfg, &conf_params);

  // empty routing section results in a failure, but while logging to file
  auto &router = launch_router_for_fail({"-c", conf_file});
  check_exit_code(router, EXIT_FAILURE);

  // the error happens during the logger initialization so we expect the message
  // on the console which is the default sink until we switch to the
  // configuration from the config file
  const std::string console_log_txt = router.get_full_output();

  EXPECT_TRUE(!console_log_txt.empty()) << "\nconsole:\n" << console_log_txt;

  EXPECT_THAT(console_log_txt, HasSubstr(test_params.expected_error))
      << "\nconsole:\n"
      << console_log_txt;

  // expect no default router file created in the logging folder
  Path shouldnotexist = Path(abs_dir.str()).join(DEFAULT_LOGFILE_NAME);
  EXPECT_FALSE(shouldnotexist.exists());

  if (!test_params.create_file) {
    EXPECT_FALSE(abs_path.exists());
  }
}

INSTANTIATE_TEST_SUITE_P(
    LoggingConfigAbsRelFilenameError, RouterLoggingConfigFilenameError,
    ::testing::Values(
        // TS_FR02_01 filename with relative path in logger
        /*0*/ LoggingConfigFilenameErrorParams(
            "[logger]\n"
            "filename=" REL_PATH "\n",
            USER_LOGFILE_NAME, false, "must be a filename, not a path"),
        // TS_FR02_02 filename with relative path in filelog
        /*1*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "sinks=filelog\n"
                                         "[filelog]\n"
                                         "filename=" REL_PATH "\n",
                                         USER_LOGFILE_NAME, false,
                                         "must be a filename, not a path"),
        // TS_FR02_03 absolute filename in logger
        /*2*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "filename=" ABS_PATH "\n",
                                         USER_LOGFILE_NAME, false,
                                         "must be a filename, not a path"),
        // TS_FR02_04 absolute filename in filelog
        /*3*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "sinks=filelog\n"
                                         "[filelog]\n"
                                         "filename=" ABS_PATH "\n",
                                         USER_LOGFILE_NAME, false,
                                         "must be a filename, not a path"),
        // TS_FR02_05 slash filename in logger
        /*4*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "filename=/\n",
                                         USER_LOGFILE_NAME, false,
                                         "is not a valid log filename"),
        // TS_FR02_06 slash filename in filelog
        /*5*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "sinks=filelog\n"
                                         "[filelog]\n"
                                         "filename=/\n",
                                         USER_LOGFILE_NAME, false,
                                         "is not a valid log filename"),
        // TS_FR02_07 existing folder filename in filelog
        /*6*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "filename=" ABS_DIR "\n",
                                         USER_LOGFILE_NAME, false,
                                         "must be a filename, not a path"),
        // TS_FR02_08 existing folder filename in filelog
        /*7*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "sinks=filelog\n"
                                         "[filelog]\n"
                                         "filename=" ABS_DIR "\n",
                                         USER_LOGFILE_NAME, false,
                                         "must be a filename, not a path"),
        // TS_FR02_09 dot filename in logger
        /*8*/
        LoggingConfigFilenameErrorParams(
            "[logger]\n"
            "filename=.\n",
            USER_LOGFILE_NAME, false,
            "File exists, but cannot open for writing"),
        // TS_FR02_10 dot filename in filelog
        /*9*/
        LoggingConfigFilenameErrorParams(
            "[logger]\n"
            "sinks=filelog\n"
            "[filelog]\n"
            "filename=.\n",
            USER_LOGFILE_NAME, false,
            "File exists, but cannot open for writing"),
        // TS_FR04_10 filename /path triggers warning and not silent override
        /*10*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "filename=" USER_LOGFILE_NAME "\n"
                                         "sinks=filelog\n"
                                         "[filelog]\n"
                                         "filename=" ABS_DIR "\n",
                                         USER_LOGFILE_NAME, false,
                                         "must be a filename, not a path"),
        // TS_FR04_02 empty filename has no effect
        /*11*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "filename=\n"
                                         "sinks=filelog\n"
                                         "[filelog]\n"
                                         "filename=" ABS_DIR "\n",
                                         USER_LOGFILE_NAME, false,
                                         "must be a filename, not a path"),
        // TS_FR04_06 Verify [logger].filename=/path or [filelog].filename
        // triggers an error
        /*12*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "filename=" ABS_DIR "\n"
                                         "sinks=filelog\n"
                                         "[filelog]\n"
                                         "filename=" ABS_DIR "\n",
                                         USER_LOGFILE_NAME, false,
                                         "must be a filename, not a path"),
        // TS_FR04_07 Verify [logger].filename=/path triggers an error
        /*13*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "filename=" ABS_DIR "\n"
                                         "sinks=filelog\n"
                                         "[filelog]\n"
                                         "filename=\n",
                                         USER_LOGFILE_NAME, false,
                                         "must be a filename, not a path"),
        // TS_FR04_08 Verify [logger].filename=/path triggers an error
        /*14*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "filename=" ABS_DIR "\n"
                                         "sinks=filelog\n"
                                         "[filelog]\n",
                                         USER_LOGFILE_NAME, false,
                                         "must be a filename, not a path"),
        // TS_FR10_01 consolelog destination set to existing file
        /*15*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "sinks=consolelog\n"
                                         "[consolelog]\n"
                                         "destination=" FILENAME "\n",
                                         USER_LOGFILE_NAME, true,
                                         "Illegal destination"),
        // TS_FR10_02 consolelog destination set to non-existing file
        /*16*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "sinks=consolelog\n"
                                         "[consolelog]\n"
                                         "destination=" FILENAME "\n",
                                         USER_LOGFILE_NAME, false,
                                         "Illegal destination"),
        // TS_FR10_03 consolelog destination set to relative file
        /*17*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "sinks=consolelog\n"
                                         "[consolelog]\n"
                                         "destination=" REL_PATH "\n",
                                         USER_LOGFILE_NAME, true,
                                         "Illegal destination"),
        // TS_FR10_04 consolelog destination set to relative file
        /*18*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "sinks=consolelog\n"
                                         "[consolelog]\n"
                                         "destination=" ABS_PATH "\n",
                                         USER_LOGFILE_NAME, true,
                                         "Illegal destination"),
        // TS_FR10_05 consolelog destination set to relative file
        /*19*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "sinks=consolelog\n"
                                         "[consolelog]\n"
                                         "destination=" ABS_DIR "\n",
                                         USER_LOGFILE_NAME, false,
                                         "Illegal destination"),
        // TS_FR04_05 absolute path in logger and legal filename fails
        /*20*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "sinks=filelog\n"
                                         "filename=" ABS_DIR "\n"
                                         "[filelog]\n"
                                         "filename=" USER_LOGFILE_NAME "\n",
                                         USER_LOGFILE_NAME, false,
                                         "must be a filename, not a path"),
        // TS_FR04_05a corner case
        /*21*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "sinks=filelog\n"
                                         "filename=/shouldfail.log\n"
                                         "[filelog]\n"
                                         "filename=" USER_LOGFILE_NAME "\n",
                                         USER_LOGFILE_NAME, false,
                                         "must be a filename, not a path"),
        // TS_FR04_06a corner case
        /*22*/
        LoggingConfigFilenameErrorParams("[logger]\n"
                                         "sinks=filelog\n"
                                         "filename=" USER_LOGFILE_NAME "\n"
                                         "[filelog]\n"
                                         "filename=/shouldfail.log\n",
                                         USER_LOGFILE_NAME, false,
                                         "is not a valid log filename")));

struct LoggingConfigFilenameLoggingFolderParams {
  std::string logging_folder;
  std::string logger_config;
  std::string filename;
  bool catch_stderr;
  std::string expected_error;

  LoggingConfigFilenameLoggingFolderParams(const std::string &logging_folder_,
                                           const std::string &logger_config_,
                                           const std::string &filename_,
                                           bool catch_stderr_,
                                           const std::string expected_error_)
      : logging_folder(logging_folder_),
        logger_config(logger_config_),
        filename(filename_),
        catch_stderr(catch_stderr_),
        expected_error(expected_error_) {}
};

class TempRelativeDirectory {
 public:
  explicit TempRelativeDirectory(const std::string &prefix = "router")
      : name_{get_tmp_dir_(prefix)} {}

  ~TempRelativeDirectory() { mysql_harness::delete_dir_recursive(name_); }

  std::string name() const { return name_; }

 private:
  std::string name_;

#ifndef _WIN32
  // mysql_harness::get_tmp_dir() returns a relative path on these platforms
  std::string get_tmp_dir_(const std::string &name) {
    return mysql_harness::get_tmp_dir(name);
  }
#else
  // mysql_harness::get_tmp_dir() returns an abs path under GetTempPath() on
  // WIN32
  std::string get_tmp_dir_(const std::string &name) {
    auto generate_random_sequence = [](size_t len) -> std::string {
      std::random_device rd;
      std::string result;
      static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
      std::uniform_int_distribution<unsigned long> dist(0,
                                                        sizeof(alphabet) - 2);

      for (size_t i = 0; i < len; ++i) {
        result += alphabet[dist(rd)];
      }

      return result;
    };

    std::string dir_name = name + "-" + generate_random_sequence(10);
    std::string result = Path(dir_name).str();
    int err = _mkdir(result.c_str());
    if (err != 0) {
      throw std::runtime_error("Error creating temporary directory " + result);
    }
    return result;
  }
#endif
};

class RouterLoggingTestConfigFilenameLoggingFolder
    : public RouterLoggingTest,
      public ::testing::WithParamInterface<
          LoggingConfigFilenameLoggingFolderParams> {};

/** @test This test verifies that consolelog destination may be set to various
 * devices
 */
TEST_P(RouterLoggingTestConfigFilenameLoggingFolder, check) {
  auto test_params = GetParam();

  TempRelativeDirectory tmp_dir;

  // create the absolute path (note: order)
  Path abs_dir = Path(tmp_dir.name()).real_path();
  Path rel_dir = Path(tmp_dir.name()).basename();

  // Replace logging_folder tag with temporary directory
  std::string lf = test_params.logging_folder;
  while (lf.find(ABS_DIR) != std::string::npos) {
    lf.replace(lf.find(ABS_DIR), sizeof(ABS_DIR) - 1, abs_dir.c_str());
  }
  while (lf.find(REL_DIR) != std::string::npos) {
    lf.replace(lf.find(REL_DIR), sizeof(REL_DIR) - 1, rel_dir.c_str());
  }

  auto conf_params = get_DEFAULT_defaults();
  conf_params["logging_folder"] = lf;

  TempDirectory conf_dir("conf");
  const std::string cfg = "[routing]\n\n" + test_params.logger_config;
  const std::string conf_file =
      create_config_file(conf_dir.name(), cfg, &conf_params);

  // empty routing section gives failure while logging to defined sink
  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE,
                               test_params.catch_stderr, false, -1s);
  check_exit_code(router, EXIT_FAILURE);

  const std::string console_log_txt = router.get_full_output();
  if (test_params.expected_error.empty()) {
    // expect something like this as error message on console/in log
    // 2020-03-19 10:00:00 main ERROR [7f539f628780] Configuration error: option
    // destinations in [routing] is required
    const std::string errmsg = "option destinations in [routing] is required";

    if (lf.empty()) {
      // log should go to consolelog, and contain routing error
      Path logfile = rel_dir.join(test_params.filename);
      EXPECT_TRUE(!console_log_txt.empty()) << "\nconsole:\n"
                                            << console_log_txt;
      EXPECT_FALSE(logfile.exists());
      EXPECT_THAT(console_log_txt, HasSubstr(errmsg)) << "\nconsole:\n"
                                                      << console_log_txt;
    } else {
      // log should go to logfile specified
      Path logfile = Path(lf).join(test_params.filename);
      EXPECT_TRUE(console_log_txt.empty()) << "\nconsole:\n" << console_log_txt;
      EXPECT_TRUE(logfile.exists());
      std::string file_log_txt =
          router.get_logfile_content(test_params.filename, Path(lf).str());
      EXPECT_THAT(file_log_txt, HasSubstr(errmsg)) << "\nlog:\n"
                                                   << file_log_txt;
    }
  } else {
    // log should go to consolelog, and contain routing error
    EXPECT_TRUE(!console_log_txt.empty()) << "\nconsole:\n" << console_log_txt;
    EXPECT_THAT(console_log_txt, HasSubstr(test_params.expected_error))
        << "\nconsole:\n"
        << console_log_txt;
  }
}

INSTANTIATE_TEST_SUITE_P(
    LoggingTestConsoleDestinationDevices,
    RouterLoggingTestConfigFilenameLoggingFolder,
    ::testing::Values(
        // TS_FR03_01
        /*0*/
        LoggingConfigFilenameLoggingFolderParams("",
                                                 "[logger]\n"
                                                 "filename=" USER_LOGFILE_NAME
                                                 "\n",
                                                 USER_LOGFILE_NAME, true,
                                                 NOT_USED),
        // TS_FR03_02
        /*1*/
        LoggingConfigFilenameLoggingFolderParams(
            ABS_DIR, "[logger]\nfilename=" USER_LOGFILE_NAME "\n",
            USER_LOGFILE_NAME, false, NOT_USED),
        // TS_FR03_03
        /*2*/
        LoggingConfigFilenameLoggingFolderParams(
            REL_DIR, "[logger]\nfilename=" USER_LOGFILE_NAME "\n",
            USER_LOGFILE_NAME, false, NOT_USED),
        // TS_FR03_04
        /*3*/
        LoggingConfigFilenameLoggingFolderParams(
            "/non/existing/absolute/path/",
            "[logger]\nfilename=" USER_LOGFILE_NAME "\n", USER_LOGFILE_NAME,
            true, "Error when creating dir '/non/existing/absolute/path'"),
        // TS_FR03_05
        /*4*/
        LoggingConfigFilenameLoggingFolderParams(
            "non/existing/relative/path",
            "[logger]\nfilename=" USER_LOGFILE_NAME "\n", USER_LOGFILE_NAME,
            true, "Error when creating dir 'non/existing/relative/path'"),
        // TS_FR05_03 without [logger].filename
        // and TS_FR05_04 without [filesink].filename
        /*5*/
        LoggingConfigFilenameLoggingFolderParams(
            ABS_DIR, "[logger]\nsinks=filelog\n[filelog]\n",
            DEFAULT_LOGFILE_NAME, false, NOT_USED)));

/** @test This test verifies that output goes to console when consolelog
 * destination is empty (TS_FR06_01)
 */
TEST_F(RouterLoggingTest, log_console_destination_empty) {
  // FIXME: Unfortunately due to the limitations of our component testing
  // framework, this test has a flaw: it is not possible to distinguish if the
  // output returned from router.get_full_output() appeared on STDERR or STDOUT.
  // This should be fixed in the future.
  TempDirectory tmp_dir;
  auto conf_params = get_DEFAULT_defaults();
  conf_params["logging_folder"] = tmp_dir.name();

  TempDirectory conf_dir("conf");
  const std::string conf_text =
      "[routing]\n\n[logger]\nsinks=consolelog\n[consolelog]\ndestination=";
  const std::string conf_file =
      create_config_file(conf_dir.name(), conf_text, &conf_params);

  // empty routing section results in a failure, but while logging to
  // destination
  auto &router = launch_router_for_fail({"-c", conf_file});
  check_exit_code(router, EXIT_FAILURE);

  // Expect the console log to be used on empty destinaton
  const std::string console_log_txt = router.get_full_output();
  EXPECT_FALSE(console_log_txt.empty()) << "\nconsole:\n" << console_log_txt;

  // expect no default router file created in tmp_dir
  Path shouldnotexist = Path(tmp_dir.name()).join("mysqlrouter.log");
  EXPECT_FALSE(shouldnotexist.exists());
}

/** @test This test verifies that output to console does not contain a warning
 * or the userdefined logfile name when filename not in use (TS_FR08_01)
 */
TEST_F(RouterLoggingTest, log_console_unused_filename_no_warning) {
  // FIXME: Unfortunately due to the limitations of our component testing
  // framework, this test has a flaw: it is not possible to distinguish if the
  // output returned from router.get_full_output() appeared on STDERR or STDOUT.
  // This should be fixed in the future.
  TempDirectory tmp_dir;
  auto conf_params = get_DEFAULT_defaults();
  conf_params["logging_folder"] = tmp_dir.name();

  TempDirectory conf_dir("conf");

  auto writer = config_writer(conf_dir.name())
                    .section("routing", {})
                    .section("logger", {{"filename", USER_LOGFILE_NAME},
                                        {"sinks", "consolelog"}})
                    .section("consolelog", {});

  // empty routing section results in a failure, but while logging to
  // destination
  auto &router = launch_router_for_fail({"-c", writer.write()});
  check_exit_code(router, EXIT_FAILURE);

  // Expect the console log output to NOT contain warning or log file name
  const std::string console_log_txt = router.get_full_output();
  EXPECT_FALSE(console_log_txt.empty()) << "\nconsole:\n" << console_log_txt;

  EXPECT_THAT(console_log_txt, Not(HasSubstr(USER_LOGFILE_NAME)))
      << "\nconsole:\n"
      << console_log_txt;

  EXPECT_THAT(console_log_txt, Not(HasSubstr("warning"))) << "\nconsole:\n"
                                                          << console_log_txt;
}

/** @test This test verifies non-existing [consolelog].destination uses default
 * value. i.e console (TS_FR06_02)
 */
TEST_F(RouterLoggingTest, log_console_non_existing_destination) {
  TempDirectory conf_dir("conf");

  auto writer = config_writer(conf_dir.name())
                    .section("routing", {})
                    .section("logger", {{"sinks", "consolelog"}})
                    .section("consolelog", {});

  writer.sections().at("DEFAULT")["logging_folder"] = "";

  // empty routing section results in a failure, but while logging to
  // destination
  auto &router = launch_router_for_fail({"-c", writer.write()});
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));

  // Expect the console log output to NOT contain warning or log file name
  EXPECT_THAT(router.get_full_output(), ::testing::Not(::testing::IsEmpty()));
}

#ifndef _WIN32
/** @test This test verifies that filename may be set to /dev/null the ugly way
 */
TEST_F(RouterLoggingTest, log_filename_dev_null_ugly) {
  Path dev_null("/dev/null");
  EXPECT_TRUE(dev_null.exists());

  TempDirectory conf_dir("conf");

  auto writer = config_writer(conf_dir.name())
                    .section("logger", {{"filename", "null"}})
                    .section("routing", {});

  writer.sections().at("DEFAULT")["logging_folder"] = "/dev";

  // empty routing section results in a failure, but while logging to file
  auto &router = launch_router_for_fail({"-c", writer.write()});
  check_exit_code(router, EXIT_FAILURE);

  // expect no default router file created in /dev
  Path shouldnotexist("/dev/mysqlrouter.log");
  EXPECT_FALSE(shouldnotexist.exists());

  EXPECT_TRUE(dev_null.exists());
}
#endif

TEST_F(RouterLoggingTest, switch_from_main_logger_to_consolelog) {
  TempDirectory conf_dir("conf");

  auto writer = config_writer(conf_dir.name()).section("routing", {});

  // set empty logging_folder for log-to-console
  writer.sections().at("DEFAULT")["logging_folder"] = "";

  auto &router = launch_router_for_fail({"-c", writer.write()});
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));

  EXPECT_THAT(router.get_full_output(), ::testing::Not(::testing::IsEmpty()));
  EXPECT_FALSE(Path(router.get_logfile_path()).exists());
}

TEST_F(RouterLoggingTest, switch_without_consolelog) {
  TempDirectory conf_dir("conf");

  // default will write to filelog.
  auto writer = config_writer(conf_dir.name()).section("routing", {});

  // no runnable config-section -> failure.
  auto &router = launch_router_for_fail({"-c", writer.write()});
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));

  // only filelog should have content.
  EXPECT_THAT(router.get_full_output(), ::testing::IsEmpty());
  EXPECT_TRUE(Path(router.get_logfile_path()).exists());
  EXPECT_THAT(router.get_logfile_content(),
              ::testing::Not(::testing::IsEmpty()));
}

TEST_F(RouterLoggingTest, switch_with_consolelog) {
  TempDirectory conf_dir("conf");

  auto writer = config_writer(conf_dir.name())
                    .section("routing", {})
                    .section("logger", {{"sinks", "consolelog,filelog"}});

  // empty routing section results in a failure, but while logging to
  // destination
  auto &router = launch_router_for_fail({"-c", writer.write()});
  ASSERT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE));

  // both should have content.
  EXPECT_THAT(router.get_full_output(), ::testing::Not(::testing::IsEmpty()));
  EXPECT_THAT(router.get_full_output(),
              ::testing::Not(::testing::HasSubstr("stopping to log")));
  EXPECT_THAT(router.get_logfile_content(),
              ::testing::Not(::testing::IsEmpty()));
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
