/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include <chrono>
#include <cstdlib>  // setenv
#include <fstream>
#include <memory>  // unique_ptr
#include <string>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysql/harness/filesystem.h"
#include "process_wrapper.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"

using namespace std::chrono_literals;

#define MY_WAIT_US_NOASSERT(cond, usec) \
  {                                     \
    int n = 0;                          \
    int max = (usec) / 1000;            \
    do {                                \
      std::this_thread::sleep_for(1ms); \
      n++;                              \
    } while ((n < max) && (cond));      \
  }

#define MY_WAIT_US(cond, usec)          \
  {                                     \
    MY_WAIT_US_NOASSERT((cond), (usec)) \
    ASSERT_FALSE((cond));               \
  }

#define FOO "foo"
#define BAR "bar"
#define WHITESPACE_FOLDER "sub folder"
#define PIDFILE_WHITESPACE "my router.pid"
#define PIDFILE "mysqlrouter.pid"
#define READONLY_FOLDER "readonly"
#define READONLY_FILE "readonly.pid"
#define NONEXISTING "nonexisting"

/** utility */
const char *single_quote(const char *input) {
  std::string s = std::string("");
  return s.append("'").append(input).append("'").c_str();
}

class RouterPidfileTest : public RouterComponentTest {
 protected:
  void SetUp() override {
    RouterComponentTest::SetUp();

    // create config with logging_folder set to that directory
    params = get_DEFAULT_defaults();
    params.at("logging_folder") = logging_folder.name();
    params.at("runtime_folder") = runtime_folder.name();

    // define the additional keepalive section
    keepalive = "[keepalive]\ninterval = 10\n";

    // load keepalive with 10sec duration
    conf_file = create_config_file(conf_folder.name(), keepalive, &params);

    // default logfile path
    logfile =
        mysql_harness::Path(logging_folder.name()).join("mysqlrouter.log");

    // default pid file path
    pidfile = mysql_harness::Path(PIDFILE);

    // default router cmdline
    router_cmdline = {"-c", conf_file};
  }

  void start_router() {
    router = &ProcessManager::launch_router(router_cmdline);
  }

  void stop_router() {
    EXPECT_FALSE(router->send_clean_shutdown_event());
    check_exit_code(*router, EXIT_SUCCESS, 3s);
  }

  void SetEnvRouterPid(const char *val) {
    int err_code;
#ifdef _WIN32
    err_code = _putenv_s("ROUTER_PID", val);
#else
    err_code = ::setenv("ROUTER_PID", val, 1);
#endif
    if (err_code) throw std::runtime_error("Failed to add ROUTER_PID");
  }

  void UnsetEnvRouterPid() {
    int err_code;
#ifdef _WIN32
    err_code = _putenv_s("ROUTER_PID", "");
#else
    err_code = ::unsetenv("ROUTER_PID");
#endif
    if (err_code) throw std::runtime_error("Failed to remove ROUTER_PID");
  }

  void create_runtime_subfolders() {
    // create the runtime_folder subfolders ./foo/bar/ needed by some testcases
    mysql_harness::Path foo =
        mysql_harness::Path(runtime_folder.name()).join(FOO);
    mysql_harness::mkdir(foo.c_str(), 0755);

    mysql_harness::Path bar = foo.join(BAR);
    mysql_harness::mkdir(bar.c_str(), 0755);

    mysql_harness::Path whitespace = foo.join(WHITESPACE_FOLDER);
    mysql_harness::mkdir(whitespace.c_str(), 0755);

    mysql_harness::Path ro = foo.join(READONLY_FOLDER);
    mysql_harness::mkdir(ro.c_str(), 0644);

    // create the read only file ./foo/readonly.pid
    mysql_harness::Path rof = foo.join(READONLY_FILE);
    std::ofstream ro_file(rof.str());
    if (ro_file.good()) {
      ro_file << "This file is read only!" << std::endl;
      ro_file.close();
      mysql_harness::make_file_readonly(rof.str());
    }
  }

  // params for config file
  std::map<std::string, std::string> params;

  // the keepalive section
  std::string keepalive;

  // tmp dir where we will log
  TempDirectory logging_folder;

  // config dir where config file is created
  TempDirectory conf_folder;

  // runtime dir where runtime files are created
  TempDirectory runtime_folder;

  // config file path
  std::string conf_file;

  // logfile path
  mysql_harness::Path logfile;

  // default pid filename
  mysql_harness::Path pidfile;

  // command line to use
  std::vector<std::string> router_cmdline;

  // the router process wrapper
  ProcessWrapper *router;
};

//
// Pidfile tests
//

/**
 * @test
 *      Bug #29441087 ROUTER SHOULD REMOVE PIDFILE ON CLEAN EXIT
 */
TEST_F(RouterPidfileTest, PidFileRemovedAtExit) {
  // Use the temporary ROUTER_PID env to set pidfile
  SetEnvRouterPid(pidfile.c_str());

  // start router with default cmdline
  start_router();

  // wait for pidfile to appear
  mysql_harness::Path fullpath =
      mysql_harness::Path(runtime_folder.name()).join(pidfile.c_str());
  MY_WAIT_US(!mysql_harness::Path(fullpath).exists(), 200 * 1000);

  // check pidfile exists
  ASSERT_TRUE(mysql_harness::Path(fullpath).exists());

  // verify clean shutdown exitcode
  EXPECT_NO_FATAL_FAILURE(stop_router());

  // check pidfile removed
  ASSERT_FALSE(mysql_harness::Path(fullpath).exists());

  // Remove the ROUTER_PID env
  UnsetEnvRouterPid();

  // expect PID output
  EXPECT_TRUE(router->expect_output("PID .* written to '.*'", true));
}

//
// Tests for --pid-file option
//

class RouterPidfileOptionTest : public RouterPidfileTest {};

/**
 * @test
 *      Start router without pidfile defined
 *      TS_FR04_01
 */
TEST_F(RouterPidfileOptionTest, PidFileNone) {
  SCOPED_TRACE("// start router");
  EXPECT_NO_FATAL_FAILURE(start_router());

  SCOPED_TRACE("// check default pidfile does NOT exist");
  mysql_harness::Path fullpath =
      mysql_harness::Path(runtime_folder.name()).join(pidfile.c_str());
  ASSERT_FALSE(fullpath.exists());

  SCOPED_TRACE("// verify clean shutdown exitcode");
  EXPECT_NO_FATAL_FAILURE(stop_router());

  SCOPED_TRACE("// expect NO PID output");
  EXPECT_FALSE(router->expect_output("PID .* written to '.*'", true));
}

/**
 * @test
 *      --pid-file option used twice on command line, without value
 *      TS_FR00_01
 */
TEST_F(RouterPidfileOptionTest, PidFileOptionTwiceWithoutValue) {
  router_cmdline.emplace_back("--pid-file");
  router_cmdline.emplace_back("--pid-file");

  auto &router = ProcessManager::launch_router(router_cmdline, EXIT_FAILURE,
                                               true, false, -1s);

  EXPECT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE, 1s));

  // expect error
  EXPECT_TRUE(router.expect_output(
      "Error: option '--pid-file' expects a value, got nothing", true));
}

/**
 * @test
 *      --pid-file option used twice on command line
 *      TS_FR00_02
 */
TEST_F(RouterPidfileOptionTest, PidFileOptionTwice) {
  mysql_harness::Path pidfile_tmp = mysql_harness::Path("shouldnotexist.pid");
  router_cmdline.emplace_back("--pid-file=" + pidfile_tmp.str());
  router_cmdline.emplace_back("--pid-file=" + pidfile.str());

  auto &router = ProcessManager::launch_router(router_cmdline, EXIT_FAILURE,
                                               true, false, -1s);
  EXPECT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE, 1s));

  // expect error
  EXPECT_TRUE(router.expect_output(
      "Error: Option --pid-file can only be given once", true));
}

/**
 * @test
 *      pid_file used twice in config file
 *      TS_FR00_03 (M)
 */
TEST_F(RouterPidfileOptionTest, PidFileOptionCfgTwice) {
  params["pid_file"] = pidfile.c_str();

  mysql_harness::Path pidfile_tmp = mysql_harness::Path("shouldnotexist.pid");
  std::string extra_params = std::string("pid_file = ") + pidfile_tmp.str();

  // load keepalive with 10sec duration
  conf_file = create_config_file(conf_folder.name(), keepalive, &params,
                                 "mysqlrouter.conf", extra_params);
  router_cmdline = {"-c", conf_file};

  auto &router = ProcessManager::launch_router(router_cmdline, EXIT_FAILURE,
                                               true, false, -1s);
  EXPECT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE, 1s));

  // expect error
  EXPECT_TRUE(router.expect_output(
      "Error: Configuration error: Option 'pid_file' already defined.", true));
}

/**
 * @test
 *      ROUTER_PID env var with whitespace in path and filename
 *      TS_FR01_07 (M)
 */
TEST_F(RouterPidfileOptionTest, PidFileOptionEnvWhitespace) {
  create_runtime_subfolders();

  // environment variable pidfile path
  mysql_harness::Path pidfile_env =
      mysql_harness::Path(FOO).join(WHITESPACE_FOLDER).join(PIDFILE_WHITESPACE);
  SetEnvRouterPid(pidfile_env.c_str());

  // start router with default conffile
  start_router();

  // wait for pidfile to appear
  mysql_harness::Path fullpath =
      mysql_harness::Path(runtime_folder.name()).join(pidfile_env.c_str());
  MY_WAIT_US(!mysql_harness::Path(fullpath).exists(), 200 * 1000);

  // check config file pid_file exists
  ASSERT_TRUE(mysql_harness::Path(fullpath).exists());

  // verify clean shutdown exitcode
  EXPECT_NO_FATAL_FAILURE(stop_router());

  // unset ROUTER_PID env
  UnsetEnvRouterPid();

  // expect PID output
  EXPECT_TRUE(router->expect_output("PID .* written to '.*'", true));
}

/**
 * @test
 *      --pid-file option on command line - successful cases
 */
struct PidFileOptionParams {
  std::string filename;
  bool tmpdir_prefix;

  PidFileOptionParams(std::string filename_, bool tmpdir_prefix_)
      : filename(std::move(filename_)), tmpdir_prefix(tmpdir_prefix_) {}
};

class RouterPidfileOptionValueTest
    : public RouterPidfileOptionTest,
      public ::testing::WithParamInterface<PidFileOptionParams> {};

TEST_P(RouterPidfileOptionValueTest, PidFileOptionValueTest) {
  // start router with test parameter
  auto test_params = GetParam();

  std::unique_ptr<TempDirectory> tmpdir =
      (test_params.tmpdir_prefix ? std::make_unique<TempDirectory>() : nullptr);

  create_runtime_subfolders();

  // prefix with a testclass temporary directory if specified.
  mysql_harness::Path param =
      (test_params.tmpdir_prefix ? mysql_harness::Path(tmpdir->name())
                                       .real_path()
                                       .join(test_params.filename)
                                 : mysql_harness::Path(test_params.filename));

  // deduce the expected full path of the pidfile
  mysql_harness::Path rt = mysql_harness::Path(runtime_folder.name());
  std::string fullpath =
      (param.is_absolute() ? param.c_str()
                           : rt.join(test_params.filename).c_str());

  router_cmdline.emplace_back("--pid-file=" + param.str());

  start_router();

  // wait for pidfile to appear
  MY_WAIT_US(!mysql_harness::Path(fullpath).exists(), 200 * 1000);

  // check pidfile exists
  ASSERT_TRUE(mysql_harness::Path(fullpath).exists());

  // verify clean shutdown exitcode
  EXPECT_NO_FATAL_FAILURE(stop_router());

  // expect PID output
  EXPECT_TRUE(router->expect_output("PID .* written to '.*'", true));
}

INSTANTIATE_TEST_SUITE_P(
    PidFileOptionValueTest, RouterPidfileOptionValueTest,
    ::testing::Values(
        // absolute path pidfile value : TS_FR01_01 /  TS_FR01_02 (O)
        // Using ProcessManager, we do not get stripping of quotes that the
        // shell does, so this should be identical to the quoted TS_FR01_02 (O)
        // case.
        PidFileOptionParams("mysqlrouter.pid", true),
        // relative with subfolders and filename : TS_FR05_01 (O)
        PidFileOptionParams(
            mysql_harness::Path(FOO).join(BAR).join(PIDFILE).c_str(), false),
        // optional filename : TS_FR05_03
        PidFileOptionParams("foobar.pid", false),
        // relative filename : TS_FR05_03
        PidFileOptionParams(PIDFILE, false),
        // quotes with whitespace : <not in testplan>
        PidFileOptionParams("' '", false),
        // path with whitespace : TS_FR01_04
        // Using ProcessManager, we do not get stripping of quotes that the
        // shell does, so this should be identical to an unquoted whitespace
        // case.
        PidFileOptionParams(mysql_harness::Path(FOO)
                                .join(WHITESPACE_FOLDER)
                                .join(PIDFILE)
                                .c_str(),
                            false),
        // twice relative to relative filename : TS_FR05_02
        PidFileOptionParams(mysql_harness::Path(FOO)
                                .join("..")
                                .join(FOO)
                                .join(BAR)
                                .join("..")
                                .join(PIDFILE)
                                .c_str(),
                            false)));

#ifndef _WIN32
INSTANTIATE_TEST_SUITE_P(PidFileOptionValueTestUnix,
                         RouterPidfileOptionValueTest,
                         ::testing::Values(
                             // whitespace : TS_FR01_03 (O)
                             PidFileOptionParams(" ", false)));
#endif

/**
 * @test
 *      --pid-file option on command line - error cases
 */
struct PidFileOptionErrorParams {
  std::string filename;
  std::string pattern;

  PidFileOptionErrorParams(std::string filename_, std::string pattern_)
      : filename(std::move(filename_)), pattern(std::move(pattern_)) {}
};

class RouterPidfileOptionValueTestError
    : public RouterPidfileOptionTest,
      public ::testing::WithParamInterface<PidFileOptionErrorParams> {};

TEST_P(RouterPidfileOptionValueTestError, PidFileOptionValueTestError) {
  auto test_params = GetParam();

  // start router with parameterized value for --pid-file, and expect error
  router_cmdline.emplace_back("--pid-file=" + test_params.filename);

  auto &router = ProcessManager::launch_router(router_cmdline, EXIT_FAILURE,
                                               true, false, -1s);
  EXPECT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE, 1s));

  // expect error
  EXPECT_TRUE(router.expect_output(test_params.pattern, true));
}

INSTANTIATE_TEST_SUITE_P(
    PidFileOptionValueTestError, RouterPidfileOptionValueTestError,
    ::testing::Values(
        // empty value : TS_FR10_01 / TS_FR10_02
        // Using ProcessManager, we do not get stripping of quotes that the
        // shell does, so this should be identical to the quoted TS_FR10_02
        // case.
        PidFileOptionErrorParams(
            "", "Error: Invalid empty value for --pid-file option"),
        // readonly dir : TS_FR11_01 (M)
        PidFileOptionErrorParams(
            mysql_harness::Path(FOO).join(READONLY_FOLDER).c_str(),
            "Error: Failed writing PID to .*/foo/readonly':.*"),
        // readonly file : TS_FR11_02 (M)
        PidFileOptionErrorParams(
            mysql_harness::Path(FOO).join(READONLY_FILE).c_str(),
            "Error: Failed writing PID to .*/foo/readonly.pid':.*"),
        // nonexisting dir : TS_FR11_03 (M)
        PidFileOptionErrorParams(
            mysql_harness::Path(FOO).join(NONEXISTING).join(PIDFILE).c_str(),
            "Error: Failed writing PID to "
            ".*/foo/nonexisting/mysqlrouter.pid':.*")));

/**
 * @test
 *      pid_file option in config file - successful cases
 */
struct PidFileOptionCfgParams {
  std::string filename;

  PidFileOptionCfgParams(std::string filename_)
      : filename(std::move(filename_)) {}
};

class RouterPidfileOptionCfgValueTest
    : public RouterPidfileOptionTest,
      public ::testing::WithParamInterface<PidFileOptionCfgParams> {};

TEST_P(RouterPidfileOptionCfgValueTest, PidFileOptionCfgValueTest) {
  // start router with test parameter
  auto test_params = GetParam();

  create_runtime_subfolders();

  // deduce the expected full path of the pidfile
  std::string filename = test_params.filename;
  mysql_harness::Path param = mysql_harness::Path(filename);
  mysql_harness::Path rt = mysql_harness::Path(runtime_folder.name());
  std::string fullpath =
      (param.is_absolute() ? filename : rt.join(filename).c_str());

  // set the filename parameter in config
  params["pid_file"] = test_params.filename;

  // load keepalive with 10sec duration
  conf_file = create_config_file(conf_folder.name(), keepalive, &params);
  router_cmdline = {"-c", conf_file};

  start_router();

  // wait for pidfile to appear
  MY_WAIT_US(!mysql_harness::Path(fullpath).exists(), 200 * 1000);

  // check pidfile exists
  ASSERT_TRUE(mysql_harness::Path(fullpath).exists());

  // verify clean shutdown exitcode
  EXPECT_NO_FATAL_FAILURE(stop_router());

  // expect PID output
  EXPECT_TRUE(router->expect_output("PID .* written to '.*'", true));
}

INSTANTIATE_TEST_SUITE_P(
    PidFileOptionCfgValueTest, RouterPidfileOptionCfgValueTest,
    ::testing::Values(
        // path with whitespace : TS_FR01_06
        PidFileOptionCfgParams(mysql_harness::Path(FOO)
                                   .join(WHITESPACE_FOLDER)
                                   .join(PIDFILE_WHITESPACE)
                                   .c_str()),
        // non-empty filename of 2 quotes : TS_FR01_05
        PidFileOptionCfgParams("''")));

/**
 * @test
 *      pid_file option in config file - error cases
 */
struct PidFileOptionCfgErrorParams {
  std::string filename;

  PidFileOptionCfgErrorParams(std::string filename_)
      : filename(std::move(filename_)) {}
};

class RouterPidfileOptionCfgValueTestError
    : public RouterPidfileOptionTest,
      public ::testing::WithParamInterface<PidFileOptionCfgErrorParams> {};

TEST_P(RouterPidfileOptionCfgValueTestError, PidFileOptionCfgValueTestError) {
  auto test_params = GetParam();

  // set the filename parameter in config
  params["pid_file"] = test_params.filename;

  // load keepalive with 10sec duration
  conf_file = create_config_file(conf_folder.name(), keepalive, &params);
  router_cmdline = {"-c", conf_file};

  // start router with config file, and expect error
  auto &router = ProcessManager::launch_router(router_cmdline, EXIT_FAILURE,
                                               true, false, -1s);
  EXPECT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE, 1s));

  // expect error
  EXPECT_TRUE(
      router.expect_output("Error: PID filename '.*' is illegal", true));
}

INSTANTIATE_TEST_SUITE_P(PidFileOptionCfgValueTestError,
                         RouterPidfileOptionCfgValueTestError,
                         ::testing::Values(
                             // empty value : TS_FR08_01
                             PidFileOptionCfgErrorParams("")));

#ifndef _WIN32
/**
 * @test
 *      ROUTER_PID env var - error cases
 * @test
 *      Not applicable on windows, as setting env var as empty equals an unset
 *      of the environment variable.
 */
struct PidFileOptionEnvErrorParams {
  std::string filename;

  PidFileOptionEnvErrorParams(std::string filename_)
      : filename(std::move(filename_)) {}
};

class RouterPidfileOptionEnvValueTestError
    : public RouterPidfileOptionTest,
      public ::testing::WithParamInterface<PidFileOptionEnvErrorParams> {};

TEST_P(RouterPidfileOptionEnvValueTestError, PidFileOptionEnvValueTestError) {
  auto test_params = GetParam();

  // environment variable pidfile path
  SetEnvRouterPid(test_params.filename.c_str());

  // start router with default config file, and expect error
  auto &router = ProcessManager::launch_router(router_cmdline, EXIT_FAILURE,
                                               true, false, -1s);
  EXPECT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE, 1s));

  // unset ROUTER_PID env
  UnsetEnvRouterPid();

  // expect error
  EXPECT_TRUE(
      router.expect_output("Error: PID filename '.*' is illegal", true));
}

INSTANTIATE_TEST_SUITE_P(PidFileOptionEnvValueTestError,
                         RouterPidfileOptionEnvValueTestError,
                         ::testing::Values(
                             // empty value : TS_FR09_01 (M), TS_FR09_02 (M)
                             PidFileOptionEnvErrorParams("")));
#endif

/**
 * @test
 *      Pidfile supremacy testcase
 *      Command line option > config file option > environment variable
 */
#define OPT (2 << 0)
#define CFG (2 << 1)
#define ENV (2 << 2)

struct PidFileOptionSupremacyParams {
  int used;
  int expect;

  PidFileOptionSupremacyParams(int used_) : used(used_), expect(0) {
    // set expectancy: OPT > CFG > ENV
    if (used_ & OPT) {
      expect = OPT;
    } else if (used_ & CFG) {
      expect = CFG;
    } else {
      expect = ENV;
    }
  }
};

class RouterPidfileOptionSupremacyTest
    : public RouterPidfileOptionTest,
      public ::testing::WithParamInterface<PidFileOptionSupremacyParams> {};

TEST_P(RouterPidfileOptionSupremacyTest, PidFileOptionSupremacyTest) {
  auto test_params = GetParam();

  // setup the 3 possible pidfile names and resulting full path
  const std::array<std::string, 3> pidfile_names = {"opt.pid", "cfg.pid",
                                                    "env.pid"};

  std::array<mysql_harness::Path, pidfile_names.size()> rtpf;
  {
    const auto runtime_folder_name = runtime_folder.name();
    auto it = rtpf.begin();
    for (const auto &pidfile_name : pidfile_names) {
      *it++ = mysql_harness::Path(runtime_folder_name).join(pidfile_name);
    }
  }

  // setup according to test parameters
  if (test_params.used & OPT) {
    router_cmdline.emplace_back("--pid-file=" + pidfile_names[0]);
  }
  if (test_params.used & CFG) {
    params["pid_file"] = pidfile_names[1];
    // load keepalive with 10sec duration
    conf_file = create_config_file(conf_folder.name(), keepalive, &params);
  }
  if (test_params.used & ENV) {
    SetEnvRouterPid(pidfile_names[2].c_str());
  }

  // start router with given parameters
  start_router();

  // wait for any pidfile to appear
  MY_WAIT_US((!rtpf[0].exists() && !rtpf[1].exists() && !rtpf[2].exists()),
             200 * 1000);

  // check pidfile existence expectations
  for (int i = 0; i < 3; i++) {
    if (test_params.expect & (2 << i)) {
      ASSERT_TRUE(rtpf[i].exists());
    } else {
      ASSERT_FALSE(rtpf[i].exists());
    }
  }

  // verify clean shutdown exitcode
  EXPECT_NO_FATAL_FAILURE(stop_router());

  if (test_params.used & ENV) {
    // unset ROUTER_PID env
    UnsetEnvRouterPid();
  }

  // expect PID output
  EXPECT_TRUE(router->expect_output("PID .* written to '.*'", true));
}

INSTANTIATE_TEST_SUITE_P(PidFileOptionSupremacyTest,
                         RouterPidfileOptionSupremacyTest,
                         ::testing::Values(
                             // --pid-file > pid_file > ROUTER_PID : TS_FR02_01
                             PidFileOptionSupremacyParams(OPT | CFG | ENV),
                             // --pid-file > pid_file : TS_FR02_02
                             PidFileOptionSupremacyParams(OPT | CFG),
                             // --pid-file > ROUTER_PID : TS_FR02_03
                             PidFileOptionSupremacyParams(OPT | ENV),
                             // pid_file > ROUTER_PID : TS_FR03_01
                             PidFileOptionSupremacyParams(CFG | ENV)));

/**
 * @test
 *      Supremacy corner cases test
 */
struct PidFileOptionSupremacyCornerCaseParams {
  std::string extra_params;
  std::string pattern;

  PidFileOptionSupremacyCornerCaseParams(std::string extra_params_,
                                         std::string pattern_)
      : extra_params(std::move(extra_params_)), pattern(std::move(pattern_)) {}
};

class RouterPidfileOptionSupremacyCornerCaseTest
    : public RouterPidfileOptionTest,
      public ::testing::WithParamInterface<
          PidFileOptionSupremacyCornerCaseParams> {};

TEST_P(RouterPidfileOptionSupremacyCornerCaseTest,
       PidFileOptionSupremacyCornerCaseTest) {
  auto test_params = GetParam();

  // start router with ROUTER_PID and empty pid_file in config, and expect error
  SetEnvRouterPid(pidfile.c_str());

  // load keepalive with 10sec duration
  conf_file = create_config_file(conf_folder.name(), keepalive, &params,
                                 "mysqlrouter.conf", test_params.extra_params);

  router_cmdline = {"-c", conf_file};

  auto &router = ProcessManager::launch_router(router_cmdline, EXIT_FAILURE,
                                               true, false, -1s);
  EXPECT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE, 1s));

  // expect error
  EXPECT_TRUE(router.expect_output(test_params.pattern, true));
}

INSTANTIATE_TEST_SUITE_P(PidFileOptionSupremacyCornerCaseTest,
                         RouterPidfileOptionSupremacyCornerCaseTest,
                         ::testing::Values(
                             // empty value : TS_FR03_02
                             PidFileOptionSupremacyCornerCaseParams(
                                 "pid_file = ",
                                 "Error: PID filename '.*' is illegal.")));

/**
 * @test
 *      Pidfile already exists tests
 */
struct PidFileExistsParams {
  int used;

  PidFileExistsParams(int used_) : used(used_) {}
};

class RouterPidfileOptionExistsTest
    : public RouterPidfileOptionTest,
      public ::testing::WithParamInterface<PidFileExistsParams> {};

TEST_P(RouterPidfileOptionExistsTest, PidFileOptionExistsTest) {
  // start router with existing pidfile, and expect error
  auto test_params = GetParam();

  // Create an already existing pidfile
  mysql_harness::Path fullpath =
      mysql_harness::Path(runtime_folder.name()).join(pidfile.c_str());
  std::ofstream alreadyexists(fullpath.str());
  alreadyexists << "PidFileOptionExistsTest already existing file" << std::endl;
  alreadyexists.close();

  // pid-file still exists
  ASSERT_TRUE(fullpath.exists()) << fullpath.str();

  if (test_params.used & ENV) {
    // set ROUTER_PID and and expect error
    SetEnvRouterPid(pidfile.c_str());
  }

  if (test_params.used & CFG) {
    params["pid_file"] = pidfile.str();
    // load keepalive with 10sec duration
    conf_file = create_config_file(conf_folder.name(), keepalive, &params);
  }

  router_cmdline = {"-c", conf_file};
  if (test_params.used & OPT) {
    router_cmdline.emplace_back("--pid-file=" + pidfile.str());
  }

  auto &router = ProcessManager::launch_router(router_cmdline, EXIT_FAILURE,
                                               true, false, -1s);
  EXPECT_NO_FATAL_FAILURE(check_exit_code(router, EXIT_FAILURE, 1s));

  if (test_params.used & ENV) {
    // unset ROUTER_PID env
    UnsetEnvRouterPid();
  }

  // expect error
  EXPECT_TRUE(
      router.expect_output("Error: PID file .* found. Already running?", true));

  // pid-file still exists
  EXPECT_TRUE(fullpath.exists()) << fullpath.str();
}

INSTANTIATE_TEST_SUITE_P(PidFileOptionExistsTest, RouterPidfileOptionExistsTest,
                         ::testing::Values(
                             // Start when --pid-file file exists : TS_FR12_01
                             PidFileExistsParams(OPT),
                             // Start when pid_file file exists : TS_FR12_02
                             PidFileExistsParams(CFG),
                             // Start when ROUTER_PID file exists : TS_FR12_03
                             PidFileExistsParams(ENV)));

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
