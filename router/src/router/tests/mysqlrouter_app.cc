/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#define UNIT_TESTS  // used in router_app.h
#include "config_files.h"
#include "dim.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/registry.h"
#include "mysqlrouter/utils.h"
#include "router_app.h"
#include "router_config.h"
#include "router_test_helpers.h"
#include "test/helpers.h"

// ignore GMock warnings
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif

#include "gmock/gmock.h"
#include "gtest_consoleoutput.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <cstdio>
#include <sstream>
#include <streambuf>
#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif

#ifdef __clang__
// ignore GMock warnings
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#include "gmock/gmock.h"
#pragma clang diagnostic pop
#else
#include "gmock/gmock.h"
#endif

using std::string;
using std::vector;

using ::testing::EndsWith;
using ::testing::Ge;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StartsWith;
using ::testing::StrEq;
using ::testing::_;

#ifndef _WIN32
using mysqlrouter::SysUserOperationsBase;

class MockSysUserOperations : public SysUserOperationsBase {
 public:
  MOCK_METHOD2(initgroups, int(const char *, gid_type));
  MOCK_METHOD1(setgid, int(gid_t));
  MOCK_METHOD1(setuid, int(uid_t));
  MOCK_METHOD1(setegid, int(gid_t));
  MOCK_METHOD1(seteuid, int(uid_t));
  MOCK_METHOD0(geteuid, uid_t());
  MOCK_METHOD1(getpwnam, struct passwd *(const char *));
  MOCK_METHOD1(getpwuid, struct passwd *(uid_t));
  MOCK_METHOD3(chown, int(const char *, uid_t, gid_t));
};

#endif

using mysql_harness::Path;

const string get_cwd() {
  char buffer[FILENAME_MAX];
  if (!getcwd(buffer, FILENAME_MAX)) {
    throw std::runtime_error("getcwd failed: " + string(strerror(errno)));
  }
  return string(buffer);
}

Path g_origin;

class AppTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    orig_cout_ = std::cout.rdbuf();
    std::cout.rdbuf(ssout.rdbuf());
#ifndef _WIN32
    mock_sys_user_operations.reset(new MockSysUserOperations());
#endif

    config_dir = Path(mysql_harness::get_tests_data_dir(g_origin.str()));
  }

  virtual void TearDown() {
    if (orig_cout_) {
      std::cout.rdbuf(orig_cout_);
    }
  }

  void reset_ssout() {
    ssout.str("");
    ssout.clear();
  }

  std::stringstream ssout;
  std::streambuf *orig_cout_;

#ifndef _WIN32
  std::unique_ptr<MockSysUserOperations> mock_sys_user_operations;
#endif
  Path config_dir;
};

TEST_F(AppTest, DefaultConstructor) {
  MySQLRouter r;
  ASSERT_STREQ(MYSQL_ROUTER_VERSION, r.get_version().c_str());
}

TEST_F(AppTest, GetVersionAsString) {
  MySQLRouter r;
  ASSERT_STREQ(MYSQL_ROUTER_VERSION, r.get_version().c_str());
}

TEST_F(AppTest, GetVersionLine) {
  MySQLRouter r;
  ASSERT_THAT(r.get_version_line(), StartsWith(MYSQL_ROUTER_PACKAGE_NAME));
  ASSERT_THAT(r.get_version_line(), HasSubstr(MYSQL_ROUTER_VERSION));
  ASSERT_THAT(r.get_version_line(), HasSubstr(MYSQL_ROUTER_VERSION_EDITION));
  ASSERT_THAT(r.get_version_line(), HasSubstr(MYSQL_ROUTER_PACKAGE_PLATFORM));
  if (MYSQL_ROUTER_PACKAGE_ARCH_64BIT == 1) {
    ASSERT_THAT(r.get_version_line(), HasSubstr("64-bit"));
  } else {
    ASSERT_THAT(r.get_version_line(), HasSubstr("32-bit"));
  }
}

TEST_F(AppTest, CheckConfigFilesSuccess) {
  MySQLRouter r;

  r.default_config_files_ = {};
  r.extra_config_files_ = {config_dir.join("mysqlrouter_extra.conf").str()};
  ASSERT_THROW(r.check_config_files(), std::runtime_error);
}

TEST_F(AppTest, CmdLineConfig) {
  vector<string> argv = {"--config", config_dir.join("mysqlrouter.conf").str()};
  // ASSERT_NO_THROW({ MySQLRouter r(g_origin, argv); });
  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(r.get_config_files().at(0), EndsWith("mysqlrouter.conf"));
  ASSERT_THAT(r.get_default_config_files(), IsEmpty());
  ASSERT_THAT(r.get_extra_config_files(), IsEmpty());
}

TEST_F(AppTest, CmdLineConfigFailRead) {
  string not_existing = "foobar.conf";
  vector<string> argv = {
      "--config",
      config_dir.join(not_existing).str(),
  };
  ASSERT_THROW({ MySQLRouter r(g_origin, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_origin, argv);
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("Failed reading configuration file"));
    ASSERT_THAT(exc.what(), HasSubstr(not_existing));
  }
}

TEST_F(AppTest, CmdLineMultipleConfig) {
  vector<string> argv = {"--config", config_dir.join("mysqlrouter.conf").str(),
                         "-c",       config_dir.join("config_a.conf").str(),
                         "--config", config_dir.join("config_b.conf").str()};
  ASSERT_THROW({ MySQLRouter r(g_origin, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_origin, argv);
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    ASSERT_THAT(exc.what(), HasSubstr("can only be used once"));
  }
}

TEST_F(AppTest, CmdLineExtraConfig) {
  vector<string> argv = {"-c", config_dir.join("config_a.conf").str(),
                         "--extra-config",
                         config_dir.join("config_b.conf").str()};
  ASSERT_NO_THROW({ MySQLRouter r(g_origin, argv); });
  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(r.get_extra_config_files().at(0), EndsWith("config_b.conf"));
  ASSERT_THAT(r.get_default_config_files(), SizeIs(0));
  ASSERT_THAT(r.get_config_files(), SizeIs(1));
}

TEST_F(AppTest, CmdLineExtraConfigFailRead) {
  string not_existing = "foobar.conf";
  vector<string> argv = {"-c", config_dir.join("config_a.conf").str(),
                         "--extra-config", config_dir.join(not_existing).str()};
  ASSERT_THROW({ MySQLRouter r(g_origin, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_origin, argv);
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("Failed reading configuration file"));
    ASSERT_THAT(exc.what(), EndsWith(not_existing));
  }
}

TEST_F(AppTest, CmdLineMultipleExtraConfig) {
  vector<string> argv = {"-c",
                         config_dir.join("mysqlrouter.conf").str(),
                         "-a",
                         config_dir.join("config_a.conf").str(),
                         "--extra-config",
                         config_dir.join("config_b.conf").str()};
  ASSERT_NO_THROW({ MySQLRouter r(g_origin, argv); });
  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(r.get_config_files().at(0).c_str(), EndsWith("mysqlrouter.conf"));
  ASSERT_THAT(r.get_extra_config_files().at(0).c_str(),
              EndsWith("config_a.conf"));
  ASSERT_THAT(r.get_extra_config_files().at(1).c_str(),
              EndsWith("config_b.conf"));
  ASSERT_THAT(r.get_default_config_files(), SizeIs(0));
  ASSERT_THAT(r.get_config_files(), SizeIs(1));
}

TEST_F(AppTest, CmdLineMultipleDuplicateExtraConfig) {
  string duplicate = "config_a.conf";
  vector<string> argv = {
      "-c",
      config_dir.join("config_a.conf").str(),
      "--extra-config",
      config_dir.join("mysqlrouter.conf").str(),
      "-a",
      config_dir.join(duplicate).str(),
      "--extra-config",
      config_dir.join(duplicate).str(),
  };
  ASSERT_THROW({ MySQLRouter r(g_origin, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_origin, argv);
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("Duplicate configuration file"));
    ASSERT_THAT(exc.what(), HasSubstr(duplicate));
  }
}

TEST_F(AppTest, CmdLineExtraConfigNoDeafultFail) {
  /*
   * Check if mysqlrouter.conf does not exist in default locations.
   */
  std::stringstream ss_line{CONFIG_FILES};

  for (string path; std::getline(ss_line, path, ';');) {
    // malformed env var will result in error, valid or missing env var results
    // in success
    bool parse_ok = mysqlrouter::substitute_envvar(path);
    if (parse_ok) {
      std::string real_path =
          mysqlrouter::substitute_variable(path, "{origin}", g_origin.str());
      ASSERT_FALSE(mysql_harness::Path(real_path).exists());
    }
  }

  vector<string> argv = {
      "--extra-config",
      config_dir.join("mysqlrouter.conf").str(),
  };
  ASSERT_THROW({ MySQLRouter r(g_origin, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_origin, argv);
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(),
                HasSubstr("Extra configuration files only work when other "));
  }
}

TEST_F(AppTest, CheckConfigFileFallbackToIniSuccess) {
  MySQLRouter r;

  r.default_config_files_ = {config_dir.join("config_c.conf").str()};
  auto res = r.check_config_files();
  ASSERT_EQ(1u, res.size());
  ASSERT_THAT(res.at(0), HasSubstr("config_c.ini"));
}

TEST_F(AppTest, CheckConfigFileFallbackToInNoDefault) {
  // falling back to ini should not work for command line passed configs
  MySQLRouter r;

  r.config_files_ = {config_dir.join("config_c.conf").str()};

  try {
    r.check_config_files();
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("No valid configuration file"));
  }
}

#ifndef _WIN32
TEST_F(AppTest, CmdLineUserBeforeBootstrap) {
  MySQLRouter router;
  vector<string> arguments = {"--user", "mysqlrouter", "--bootstrap",
                              "127.0.0.1:5000"};
  ASSERT_THROW(router.parse_command_options(arguments), std::runtime_error);

  try {
    router.parse_command_options(arguments);
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(
        exc.what(),
        StrEq("One can only use the -u/--user switch if running as root"));
  }
}

TEST_F(AppTest, CmdLineUserShortBeforeBootstrap) {
  MySQLRouter router;
  vector<string> arguments = {"-u", "mysqlrouter", "--bootstrap",
                              "127.0.0.1:5000"};
  ASSERT_THROW(router.parse_command_options(arguments), std::runtime_error);

  try {
    router.parse_command_options(arguments);
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(
        exc.what(),
        HasSubstr("One can only use the -u/--user switch if running as root"));
  }
}
#endif

TEST_F(AppTest, CmdLineVersion) {
  vector<string> argv = {"--version"};

  reset_ssout();

  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(ssout.str(), StartsWith(r.get_version_line()));
}

TEST_F(AppTest, CmdLineVersionShort) {
  vector<string> argv = {"-V"};

  reset_ssout();

  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(ssout.str(), StartsWith("MySQL Router"));
}

TEST_F(AppTest, CmdLineHelp) {
  vector<string> argv = {"--help"};
  reset_ssout();
  MySQLRouter r(g_origin, argv);

  // several substrings from help output that are unlikely to change soon
  EXPECT_THAT(ssout.str(), HasSubstr("MySQL Router v"));
  EXPECT_THAT(
      ssout.str(),
      HasSubstr(
          "Oracle is a registered trademark of Oracle Corporation and/or its"));
  EXPECT_THAT(ssout.str(), HasSubstr("Usage: mysqlrouter"));
}

TEST_F(AppTest, CmdLineHelpShort) {
  vector<string> argv = {"-?"};
  reset_ssout();
  MySQLRouter r(g_origin, argv);

  // several substrings from help output that are unlikely to change soon
  EXPECT_THAT(ssout.str(), HasSubstr("MySQL Router v"));
  EXPECT_THAT(
      ssout.str(),
      HasSubstr(
          "Oracle is a registered trademark of Oracle Corporation and/or its"));
  EXPECT_THAT(ssout.str(), HasSubstr("Usage: mysqlrouter"));
}

TEST_F(AppTest, ConfigFileParseError) {
  vector<string> argv = {
      "--config",
      config_dir.join("parse_error.conf").str(),
  };
  ASSERT_THROW(
      {
        MySQLRouter r(g_origin, argv);
        r.start();
      },
      std::runtime_error);
  try {
    MySQLRouter r(g_origin, argv);
    r.start();
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(),
                HasSubstr("Configuration error: Malformed section header:"));
  }
}

TEST_F(AppTest, SectionOverMultipleConfigFiles) {
  string extra_config = config_dir.join("mysqlrouter_extra.conf").str();
  vector<string> argv = {"--config", config_dir.join("mysqlrouter.conf").str(),
                         "--extra-config=" + extra_config};
  ASSERT_NO_THROW({ MySQLRouter r(g_origin, argv); });

  MySQLRouter r(g_origin, argv);
  ASSERT_THAT(r.get_config_files().at(0).c_str(), EndsWith("mysqlrouter.conf"));
  ASSERT_THAT(r.get_extra_config_files().at(0).c_str(),
              EndsWith("mysqlrouter_extra.conf"));

  // let the Loader load the configuration files
  ASSERT_NO_THROW(r.start());

  auto section = r.loader_->get_config().get("magic", "");
  ASSERT_THAT(section.get("foo"), StrEq("bar"));
  ASSERT_THROW(section.get("NotInTheSection"), mysql_harness::bad_option);
}

#ifndef _WIN32
TEST_F(AppTest, CanStartTrue) {
  vector<string> argv = {"--config", config_dir.join("mysqlrouter.conf").str()};
  ASSERT_NO_THROW({ MySQLRouter r(g_origin, argv); });
}

TEST_F(AppTest, CanStartFalse) {
  vector<vector<string>> cases = {
      {""},
  };
  for (auto &argv : cases) {
    ASSERT_THROW(
        {
          MySQLRouter r(g_origin, argv);
          r.start();
        },
        std::runtime_error);
  }
}

/*
 * We don't switch user for windows
 */
#ifndef _WIN32

/**
 * @test
 *       Verify that if --user/-u option is used, then user is switched before
 * logger is initialized.
 */
TEST_F(AppTest, SetCommandLineUserBeforeInitializingLogger) {
  const char *user = "mysqlrouter";

  vector<string> argv = {
      "--config", config_dir.join("mysqlrouter.conf").str(),
      "--extra-config=" + config_dir.join("mysqlrouter_extra.conf").str(),
      "--user=" + std::string(user)};

  // set empty Registry (is_ready() return false)
  std::unique_ptr<mysql_harness::logging::Registry> registry(
      new mysql_harness::logging::Registry());
  mysql_harness::DIM::instance().set_LoggingRegistry(
      [&registry]() { return registry.release(); },
      std::default_delete<mysql_harness::logging::Registry>());
  mysql_harness::DIM::instance().reset_LoggingRegistry();

  struct passwd user_info;
  user_info.pw_gid = 12;
  user_info.pw_uid = 17;

  EXPECT_CALL(*mock_sys_user_operations, geteuid())
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(user)))
      .Times(1)
      .WillOnce(Return(&user_info));
  EXPECT_CALL(*mock_sys_user_operations,
              initgroups(StrEq(user),
                         (SysUserOperationsBase::gid_type)user_info.pw_gid))
      .Times(1);
  EXPECT_CALL(*mock_sys_user_operations, setgid(user_info.pw_gid))
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, setuid(user_info.pw_uid))
      .Times(1)
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs([&] {
                                 ASSERT_FALSE(mysql_harness::DIM::instance()
                                                  .get_LoggingRegistry()
                                                  .is_ready());
                               }),
                               (Return(0))));

  MySQLRouter r(g_origin, argv, mock_sys_user_operations.get());
  ASSERT_NO_THROW(r.start());
}

/**
 * @test
 *       Verify that if --user/-u option is used, then user is switched before
 * logger is initialized.
 */
TEST_F(AppTest, SetConfigUserBeforeInitializingLogger) {
  const char *user = "mysqlrouter";

  std::string tmp_dir = mysql_harness::get_tmp_dir("AppTest");
  std::shared_ptr<void> exit_guard(
      nullptr, [&](void *) { mysql_harness::delete_dir_recursive(tmp_dir); });

  // copy config file and add user option to [DEFAULT] section
  {
    std::ofstream destination_stream(
        mysql_harness::Path(tmp_dir).join("mysqlrouter.conf").str());
    std::ifstream source_stream(config_dir.join("mysqlrouter.conf").str());

    std::string line;
    while (source_stream.good() && destination_stream.good()) {
      getline(source_stream, line);
      destination_stream << line << std::endl;
      if (line.find("DEFAULT]") != line.npos)
        destination_stream << "user=" << std::string(user) << std::endl;
    }
  }

  vector<string> argv = {
      "--config", mysql_harness::Path(tmp_dir).join("mysqlrouter.conf").str(),
      "--extra-config=" + config_dir.join("mysqlrouter_extra.conf").str()};

  // set empty Registry (is_ready() return false)
  std::unique_ptr<mysql_harness::logging::Registry> registry(
      new mysql_harness::logging::Registry());
  mysql_harness::DIM::instance().set_LoggingRegistry(
      [&registry]() { return registry.release(); },
      std::default_delete<mysql_harness::logging::Registry>());
  mysql_harness::DIM::instance().reset_LoggingRegistry();

  struct passwd user_info;
  user_info.pw_gid = 12;
  user_info.pw_uid = 17;

  EXPECT_CALL(*mock_sys_user_operations, geteuid())
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(user)))
      .Times(1)
      .WillOnce(Return(&user_info));
  EXPECT_CALL(*mock_sys_user_operations,
              initgroups(StrEq(user),
                         (SysUserOperationsBase::gid_type)user_info.pw_gid))
      .Times(1);
  EXPECT_CALL(*mock_sys_user_operations, setgid(user_info.pw_gid))
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, setuid(user_info.pw_uid))
      .Times(1)
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs([&] {
                                 ASSERT_FALSE(mysql_harness::DIM::instance()
                                                  .get_LoggingRegistry()
                                                  .is_ready());
                               }),
                               (Return(0))));

  MySQLRouter r(g_origin, argv, mock_sys_user_operations.get());
  ASSERT_NO_THROW(r.start());
}

#endif

TEST_F(AppTest, ShowingInfoTrue) {
  vector<vector<string>> cases = {
      {"--version"},
      {"--help"},
      {"--help", "--config", config_dir.join("mysqlrouter.conf").str()},
      {"--config", config_dir.join("mysqlrouter.conf").str(), "--help"},
  };

  // Make sure we do not start when showing information
  for (auto &argv : cases) {
    ASSERT_NO_THROW({
      MySQLRouter r(g_origin, argv);
      r.start();
    });
    ASSERT_THAT(ssout.str(), HasSubstr("MySQL Router v"));
    reset_ssout();
  }
}

TEST_F(AppTest, ShowingInfoFalse) {
  // Cases should be allowing Router to start
  vector<vector<string>> cases = {
      {"--config", config_dir.join("mysqlrouter.conf").str(),
       "--extra-config=" + config_dir.join("mysqlrouter_extra.conf").str()}};

  for (auto &argv : cases) {
    ASSERT_NO_THROW({
      MySQLRouter r(g_origin, argv);
      r.start();
    });
  }
}

TEST_F(AppTest, UserSetPermanentlyByName) {
  const char *USER = "mysqluser";

  struct passwd user_info;
  user_info.pw_gid = 12;
  user_info.pw_uid = 17;

  EXPECT_CALL(*mock_sys_user_operations, geteuid())
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(USER)))
      .Times(1)
      .WillOnce(Return(&user_info));
  EXPECT_CALL(*mock_sys_user_operations,
              initgroups(StrEq(USER),
                         (SysUserOperationsBase::gid_type)user_info.pw_gid))
      .Times(1);
  EXPECT_CALL(*mock_sys_user_operations, setgid(user_info.pw_gid))
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, setuid(user_info.pw_uid))
      .Times(1)
      .WillOnce(Return(0));

  ASSERT_NO_THROW({ set_user(USER, true, mock_sys_user_operations.get()); });
}

TEST_F(AppTest, UserSetPermanentlyById) {
  const char *USER = "1234";

  struct passwd user_info;
  user_info.pw_gid = 12;
  user_info.pw_uid = 17;

  EXPECT_CALL(*mock_sys_user_operations, geteuid())
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(USER)))
      .Times(1)
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*mock_sys_user_operations, getpwuid((uid_t)atoi(USER)))
      .Times(1)
      .WillOnce(Return(&user_info));
  EXPECT_CALL(*mock_sys_user_operations,
              initgroups(StrEq(USER),
                         (SysUserOperationsBase::gid_type)user_info.pw_gid))
      .Times(1);
  EXPECT_CALL(*mock_sys_user_operations, setgid(user_info.pw_gid))
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, setuid(user_info.pw_uid))
      .Times(1)
      .WillOnce(Return(0));

  ASSERT_NO_THROW({ set_user(USER, true, mock_sys_user_operations.get()); });
}

TEST_F(AppTest, UserSetPermanentlyByNotExistingId) {
  const char *USER = "124";

  EXPECT_CALL(*mock_sys_user_operations, geteuid())
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(USER)))
      .Times(1)
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*mock_sys_user_operations, getpwuid((uid_t)atoi(USER)))
      .Times(1)
      .WillOnce(Return(nullptr));

  try {
    set_user(USER, true, mock_sys_user_operations.get());
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), StrEq("Can't use user '124'. "
                                  "Please check that the user exists!"));
  }
}

TEST_F(AppTest, UserSetPermanentlyByNotExistingName) {
  const char *USER = "124name";

  EXPECT_CALL(*mock_sys_user_operations, geteuid())
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(USER)))
      .Times(1)
      .WillOnce(Return(nullptr));

  try {
    set_user(USER, true, mock_sys_user_operations.get());
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), StrEq("Can't use user '124name'. "
                                  "Please check that the user exists!"));
  }
}

TEST_F(AppTest, UserSetPermanentlyByNonRootUser) {
  const char *USER = "mysqlrouter";

  EXPECT_CALL(*mock_sys_user_operations, geteuid())
      .Times(1)
      .WillOnce(Return(1));
  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(USER)))
      .Times(1)
      .WillOnce(Return(nullptr));

  try {
    set_user(USER, true, mock_sys_user_operations.get());
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(
        exc.what(),
        StrEq("One can only use the -u/--user switch if running as root"));
  }
}

TEST_F(AppTest, UserSetPermanentlySetEGidFails) {
  const char *USER = "mysqlrouter";

  struct passwd user_info;
  user_info.pw_gid = 12;
  user_info.pw_uid = 17;

  EXPECT_CALL(*mock_sys_user_operations, geteuid())
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(USER)))
      .Times(1)
      .WillOnce(Return(&user_info));
  EXPECT_CALL(*mock_sys_user_operations,
              initgroups(StrEq(USER),
                         (SysUserOperationsBase::gid_type)user_info.pw_gid))
      .Times(1);
  EXPECT_CALL(*mock_sys_user_operations, setgid(user_info.pw_gid))
      .Times(1)
      .WillOnce(Return(-1));

  try {
    set_user(USER, true, mock_sys_user_operations.get());
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(),
                StartsWith("Error trying to set the user. setgid failed:"));
  }
}

TEST_F(AppTest, UserSetPermanentlySetEUidFails) {
  const char *USER = "mysqlrouter";

  struct passwd user_info;
  user_info.pw_gid = 12;
  user_info.pw_uid = 17;

  EXPECT_CALL(*mock_sys_user_operations, geteuid())
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(USER)))
      .Times(1)
      .WillOnce(Return(&user_info));
  EXPECT_CALL(*mock_sys_user_operations,
              initgroups(StrEq(USER),
                         (SysUserOperationsBase::gid_type)user_info.pw_gid))
      .Times(1);
  EXPECT_CALL(*mock_sys_user_operations, setgid(user_info.pw_gid))
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, setuid(user_info.pw_uid))
      .Times(1)
      .WillOnce(Return(-1));

  try {
    set_user(USER, true, mock_sys_user_operations.get());
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(),
                StartsWith("Error trying to set the user. setuid failed:"));
  }
}

TEST_F(AppTest, UserSetByName) {
  const char *USER = "mysqluser";

  struct passwd user_info;
  user_info.pw_gid = 12;
  user_info.pw_uid = 17;

  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(USER)))
      .Times(1)
      .WillOnce(Return(&user_info));
  EXPECT_CALL(*mock_sys_user_operations,
              initgroups(StrEq(USER),
                         (SysUserOperationsBase::gid_type)user_info.pw_gid))
      .Times(1);
  EXPECT_CALL(*mock_sys_user_operations, setegid(user_info.pw_gid))
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, seteuid(user_info.pw_uid))
      .Times(1)
      .WillOnce(Return(0));

  ASSERT_NO_THROW({ set_user(USER, false, mock_sys_user_operations.get()); });
}

TEST_F(AppTest, UserSetById) {
  const char *USER = "1234";

  struct passwd user_info;
  user_info.pw_gid = 12;
  user_info.pw_uid = 17;

  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(USER)))
      .Times(1)
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*mock_sys_user_operations, getpwuid((uid_t)atoi(USER)))
      .Times(1)
      .WillOnce(Return(&user_info));
  EXPECT_CALL(*mock_sys_user_operations,
              initgroups(StrEq(USER),
                         (SysUserOperationsBase::gid_type)user_info.pw_gid))
      .Times(1);
  EXPECT_CALL(*mock_sys_user_operations, setegid(user_info.pw_gid))
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, seteuid(user_info.pw_uid))
      .Times(1)
      .WillOnce(Return(0));

  ASSERT_NO_THROW({ set_user(USER, false, mock_sys_user_operations.get()); });
}

TEST_F(AppTest, UserSetByNotExistingId) {
  const char *USER = "124";

  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(USER)))
      .Times(1)
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*mock_sys_user_operations, getpwuid((uid_t)atoi(USER)))
      .Times(1)
      .WillOnce(Return(nullptr));

  try {
    set_user(USER, false, mock_sys_user_operations.get());
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), StrEq("Can't use user '124'. "
                                  "Please check that the user exists!"));
  }
}

TEST_F(AppTest, UserSetByNotExistingName) {
  const char *USER = "124name";

  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(USER)))
      .Times(1)
      .WillOnce(Return(nullptr));

  try {
    set_user(USER, false, mock_sys_user_operations.get());
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), StrEq("Can't use user '124name'. "
                                  "Please check that the user exists!"));
  }
}

TEST_F(AppTest, UserSetSetGidFails) {
  const char *USER = "mysqlrouter";

  struct passwd user_info;
  user_info.pw_gid = 12;
  user_info.pw_uid = 17;

  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(USER)))
      .Times(1)
      .WillOnce(Return(&user_info));
  EXPECT_CALL(*mock_sys_user_operations,
              initgroups(StrEq(USER),
                         (SysUserOperationsBase::gid_type)user_info.pw_gid))
      .Times(1);
  EXPECT_CALL(*mock_sys_user_operations, setegid(user_info.pw_gid))
      .Times(1)
      .WillOnce(Return(-1));

  try {
    set_user(USER, false, mock_sys_user_operations.get());
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(),
                StartsWith("Error trying to set the user. setegid failed:"));
  }
}

TEST_F(AppTest, UserSetSetUidFails) {
  const char *USER = "mysqlrouter";

  struct passwd user_info;
  user_info.pw_gid = 12;
  user_info.pw_uid = 17;

  EXPECT_CALL(*mock_sys_user_operations, getpwnam(StrEq(USER)))
      .Times(1)
      .WillOnce(Return(&user_info));
  EXPECT_CALL(*mock_sys_user_operations,
              initgroups(StrEq(USER),
                         (SysUserOperationsBase::gid_type)user_info.pw_gid))
      .Times(1);
  EXPECT_CALL(*mock_sys_user_operations, setegid(user_info.pw_gid))
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_sys_user_operations, seteuid(user_info.pw_uid))
      .Times(1)
      .WillOnce(Return(-1));

  try {
    set_user(USER, false, mock_sys_user_operations.get());
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(),
                StartsWith("Error trying to set the user. seteuid failed:"));
  }
}

TEST_F(AppTest, BootstrapSuperuserNoUserOption) {
  vector<string> argv = {"--bootstrap", "127.0.0.1:3060"};

  EXPECT_CALL(*mock_sys_user_operations, geteuid())
      .Times(1)
      .WillOnce(Return(0));

  try {
    MySQLRouter r(g_origin, argv, mock_sys_user_operations.get());
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), StartsWith("You are bootstraping as a superuser."));
  }
}

/**
 * @test
 *      Verify that std::runtime_error is thrown when --master-key-reader option
 * is used in non-bootstrap mode.
 */
TEST_F(AppTest, ThrowWhenMasterKeyReaderUsedWithoutBootstrap) {
  vector<string> argv = {"--master-key-reader=reader.sh"};
  ASSERT_THROW_LIKE(MySQLRouter(g_origin, argv, mock_sys_user_operations.get()),
                    std::runtime_error,
                    "Option --master-key-reader can only be used together with "
                    "-B/--bootstrap");
}

/**
 * @test
 *       Verify that std::runtime_error is thrown when --master_key-writer
 * option is used in non-bootstrap mode.
 */
TEST_F(AppTest, ThrowWhenMasterKeyWriterUsedWithoutBootstrap) {
  vector<string> argv = {"--master-key-writer=writer.sh"};
  ASSERT_THROW_LIKE(MySQLRouter(g_origin, argv, mock_sys_user_operations.get()),
                    std::runtime_error,
                    "Option --master-key-writer can only be used together with "
                    "-B/--bootstrap");
}

/**
 * @test
 *       Verify that std::runtime_error is thrown when --master-key-reader
 * option is used without value.
 */
TEST_F(AppTest, ThrowWhenMasterKeyReaderUsedWithoutValue) {
  vector<string> argv = {"--bootstrap", "127.0.0.1:3060",
                         "--master-key-reader"};
  ASSERT_THROW_LIKE(MySQLRouter(g_origin, argv, mock_sys_user_operations.get()),
                    std::runtime_error,
                    "option '--master-key-reader' requires a value.");
}

/**
 * @test
 *       Verify that std::runtime_error is thrown when --master-key-writer
 * option is used without value.
 */
TEST_F(AppTest, ThrowWhenMasterKeyWriterUsedWithoutValue) {
  vector<string> argv = {"--bootstrap", "127.0.0.1:3060",
                         "--master-key-writer"};
  ASSERT_THROW_LIKE(MySQLRouter(g_origin, argv, mock_sys_user_operations.get()),
                    std::runtime_error,
                    "option '--master-key-writer' requires a value.");
}

/**
 * @test
 *       Verify that std::runtime_error is throw when --master-key-reader option
 * is used without using --master-key-writer option.
 */
TEST_F(AppTest, ThrowWhenMasterKeyReaderUsedWithoutMasterKeyWriter) {
  vector<string> argv = {"--bootstrap", "127.0.0.1:3060",
                         "--master-key-reader=reader.sh"};
  ASSERT_THROW_LIKE(MySQLRouter(g_origin, argv, mock_sys_user_operations.get()),
                    std::runtime_error,
                    "Option --master-key-reader can only be used together with "
                    "--master-key-writer.");
}

/**
 * @test
 *       Verify that std::runtime_error is thrown when --master-key-writer
 * option is used without using --master-key-reader option.
 */
TEST_F(AppTest, ThrowWhenMasterKeyWriterUsedWithoutMasterKeyReader) {
  vector<string> argv = {"--bootstrap", "127.0.0.1:3060",
                         "--master-key-writer=writer.sh"};
  ASSERT_THROW_LIKE(MySQLRouter(g_origin, argv, mock_sys_user_operations.get()),
                    std::runtime_error,
                    "Option --master-key-writer can only be used together with "
                    "--master-key-reader.");
}

#endif  // #ifndef _WIN32

class AppLoggerTest : public ConsoleOutputTest {
 protected:
  void SetUp() override {
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
  }
};

TEST_F(AppLoggerTest, TestLogger) {
  // This test verifies that:
  // - setting log level works (by overriding the default)
  // - a logger is created for each of: main exec and all plugins

  // create config file
  Path config_path(*temp_dir);
  config_path.append("test_mysqlrouter_app.conf");
  std::ofstream ofs_config(config_path.str());
  if (ofs_config.good()) {
    ofs_config << "[DEFAULT]\n";
    ofs_config << "logging_folder =\n";
    ofs_config << "plugin_folder = " << plugin_dir->str() << "\n";
    ofs_config << "runtime_folder = " << temp_dir->str() << "\n";
    ofs_config << "config_folder = " << config_dir->str() << "\n";
    ofs_config << "\n";
    ofs_config << "[logger]\n";
    ofs_config << "level = DEBUG\n";  // override the default (WARNING)
    ofs_config << "\n";
    ofs_config << "[magic]\n";  // magic plugin
    ofs_config << "do_magic = yes\n";
    ofs_config << "message = It is some kind of magic\n";
    ofs_config << "\n";
    ofs_config << "[lifecycle3]\n";  // lifecycle3 plugin (lifecycle dependency)
    ofs_config << "[lifecycle:instance1]\n";  // lifecycle plugin
    ofs_config.close();
  } else {
    throw std::runtime_error("Failed creating config file '" +
                             config_path.str() + "'");
  }

  // run MySQLRouter
  reset_ssout();
  vector<string> argv = {"-c", config_path.c_str()};
  MySQLRouter r(g_origin, argv);
  ASSERT_NO_THROW(r.start());

  // verify that all plugins have a module registered with the logger
  auto loggers =
      mysql_harness::DIM::instance().get_LoggingRegistry().get_logger_names();
  EXPECT_THAT(loggers, testing::UnorderedElementsAre(
                           mysql_harness::logging::kMainLogger, "magic",
                           "lifecycle", "lifecycle3", "sql"));

  // verify the log contains what we expect it to contain. We're looking for
  // lines like this:
  {
    // 2017-05-03 11:30:23 main DEBUG [7ffff7fd4780] Main logger initialized,
    // logging to STDERR
    EXPECT_THAT(get_log_stream().str(), HasSubstr(" main DEBUG "));
    EXPECT_THAT(get_log_stream().str(),
                HasSubstr(" Main logger initialized, logging to STDERR"));

    // 2017-05-03 11:30:25 magic INFO [7ffff5e34700] It is some kind of magic
    EXPECT_THAT(get_log_stream().str(), HasSubstr(" magic INFO "));
    EXPECT_THAT(get_log_stream().str(), HasSubstr(" It is some kind of magic"));

    // 2017-05-03 11:30:25 lifecycle INFO [7faefa705780] lifecycle:all
    // init():begin
    EXPECT_THAT(get_log_stream().str(), HasSubstr(" lifecycle INFO "));
    EXPECT_THAT(get_log_stream().str(),
                HasSubstr(" lifecycle:all init():begin"));
  }
}

TEST_F(AppTest, EmptyConfigPath) {
  vector<string> argv = {"--config", ""};
  EXPECT_THROW({ MySQLRouter r(g_origin, argv); }, std::runtime_error);
}

int main(int argc, char *argv[]) {
  g_origin = Path(argv[0]).dirname();

  register_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
