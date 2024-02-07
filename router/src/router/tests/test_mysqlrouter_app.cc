/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
#define UNIT_TESTS  // used in router_app.h
#include "router_app.h"

#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <streambuf>

#include <gmock/gmock.h>

#include "dim.h"
#include "gtest_consoleoutput.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/vt100_filter.h"
#include "mysqlrouter/config_files.h"
#include "mysqlrouter/utils.h"  // substitute_envvar
#include "router_config.h"      // MYSQL_ROUTER_VERSION
#include "router_test_helpers.h"
#include "scope_guard.h"
#include "test/helpers.h"
#include "test/temp_directory.h"

static const std::string kPluginNameMagic("routertestplugin_magic");
static const std::string kPluginNameLifecycle("routertestplugin_lifecycle");
static const std::string kPluginNameLifecycle3("routertestplugin_lifecycle3");

using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StartsWith;
using ::testing::StrEq;

#ifndef _WIN32
using mysqlrouter::SysUserOperationsBase;

class MockSysUserOperations : public SysUserOperationsBase {
 public:
  MOCK_METHOD(int, initgroups, (const char *, gid_type), (override));
  MOCK_METHOD(int, setgid, (gid_t), (override));
  MOCK_METHOD(int, setuid, (uid_t), (override));
  MOCK_METHOD(int, setegid, (gid_t), (override));
  MOCK_METHOD(int, seteuid, (uid_t), (override));
  MOCK_METHOD(uid_t, geteuid, (), (override));
  MOCK_METHOD(struct passwd *, getpwnam, (const char *), (override));
  MOCK_METHOD(struct passwd *, getpwuid, (uid_t), (override));
  MOCK_METHOD(int, chown, (const char *, uid_t, gid_t), (override));
};

#endif  // #ifndef _WIN32

std::string g_program_name;

class AppTest : public ::testing::Test {
 protected:
  void SetUp() override {
    init_test_logger();
#ifndef _WIN32
    mock_sys_user_operations =
        std::make_unique<::testing::StrictMock<MockSysUserOperations>>();
#endif

    config_dir = Path(mysql_harness::get_tests_data_dir(
        Path(g_program_name).dirname().str()));
  }

#ifndef _WIN32
  std::unique_ptr<::testing::StrictMock<MockSysUserOperations>>
      mock_sys_user_operations;
#endif
  mysql_harness::Path config_dir;
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
  ASSERT_THAT(r.get_version_line(), HasSubstr(MYSQL_ROUTER_PACKAGE_ARCH_CPU));
}

TEST_F(AppTest, CheckConfigFilesSuccess) {
  MySQLRouter r;

  r.default_config_files_ = {};
  r.extra_config_files_ = {config_dir.join("mysqlrouter_extra.conf").str()};
  ASSERT_THROW(r.check_config_files(), std::runtime_error);
}

TEST_F(AppTest, CmdLineConfig) {
  std::vector<std::string> argv = {"--config",
                                   config_dir.join("mysqlrouter.conf").str()};
  // ASSERT_NO_THROW({ MySQLRouter r(g_origin.str(), argv); });
  MySQLRouter r(g_program_name, argv);
  ASSERT_THAT(r.get_config_files().at(0), EndsWith("mysqlrouter.conf"));
  // ASSERT_THAT(r.get_default_config_files(), IsEmpty());
  ASSERT_THAT(r.get_extra_config_files(), IsEmpty());
}

TEST_F(AppTest, CmdLineConfigFailNotExists) {
  std::string not_existing = "foobar.conf";
  std::vector<std::string> argv = {
      "--config",
      config_dir.join(not_existing).str(),
  };
  ASSERT_THROW({ MySQLRouter r(g_program_name, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_program_name, argv);
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("The configuration file"));
    EXPECT_THAT(exc.what(), HasSubstr(not_existing));
    EXPECT_THAT(exc.what(), HasSubstr("does not exist"));
  }
}

#ifndef _WIN32
TEST_F(AppTest, CmdLineConfigFailNoAccess) {
  TempDirectory tmpdir;

  auto pathname = tmpdir.file("foobar.conf");

  // create a file that has no read-permissions.
  auto fd = open(pathname.c_str(), O_EXCL | O_WRONLY | O_TRUNC | O_CREAT, 0);
  ASSERT_NE(fd, -1);
  Scope_guard guard{[fd]() { close(fd); }};

  std::vector<std::string> argv = {
      "--config",
      pathname,
  };
  ASSERT_THROW({ MySQLRouter r(g_program_name, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_program_name, argv);
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("The configuration file"));
    EXPECT_THAT(exc.what(), HasSubstr(pathname));
    EXPECT_THAT(exc.what(), HasSubstr("is not readable"));
  }
}
#endif

TEST_F(AppTest, CmdLineMultipleConfig) {
  std::vector<std::string> argv = {
      "--config", config_dir.join("mysqlrouter.conf").str(),
      "-c",       config_dir.join("config_a.conf").str(),
      "--config", config_dir.join("config_b.conf").str()};
  ASSERT_THROW({ MySQLRouter r(g_program_name, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_program_name, argv);
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    ASSERT_THAT(exc.what(), HasSubstr("can only be used once"));
  }
}

TEST_F(AppTest, CmdLineExtraConfig) {
  std::vector<std::string> argv = {"-c", config_dir.join("config_a.conf").str(),
                                   "--extra-config",
                                   config_dir.join("config_b.conf").str()};
  ASSERT_NO_THROW({ MySQLRouter r(g_program_name, argv); });
  MySQLRouter r(g_program_name, argv);
  ASSERT_THAT(r.get_extra_config_files().at(0), EndsWith("config_b.conf"));
  // ASSERT_THAT(r.get_default_config_files(), SizeIs(0));
  ASSERT_THAT(r.get_config_files(), SizeIs(1));
}

TEST_F(AppTest, CmdLineExtraConfigFailRead) {
  std::string not_existing = "foobar.conf";
  std::vector<std::string> argv = {"-c", config_dir.join("config_a.conf").str(),
                                   "--extra-config",
                                   config_dir.join(not_existing).str()};
  ASSERT_THROW({ MySQLRouter r(g_program_name, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_program_name, argv);
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("The configuration file"));
    EXPECT_THAT(exc.what(), HasSubstr(not_existing));
    EXPECT_THAT(exc.what(), HasSubstr("does not exist"));
  }
}

TEST_F(AppTest, CmdLineMultipleExtraConfig) {
  std::vector<std::string> argv = {"-c",
                                   config_dir.join("mysqlrouter.conf").str(),
                                   "-a",
                                   config_dir.join("config_a.conf").str(),
                                   "--extra-config",
                                   config_dir.join("config_b.conf").str()};
  ASSERT_NO_THROW({ MySQLRouter r(g_program_name, argv); });
  MySQLRouter r(g_program_name, argv);
  ASSERT_THAT(r.get_config_files().at(0).c_str(), EndsWith("mysqlrouter.conf"));
  ASSERT_THAT(r.get_extra_config_files().at(0).c_str(),
              EndsWith("config_a.conf"));
  ASSERT_THAT(r.get_extra_config_files().at(1).c_str(),
              EndsWith("config_b.conf"));
  // ASSERT_THAT(r.get_default_config_files(), SizeIs(0));
  ASSERT_THAT(r.get_config_files(), SizeIs(1));
}

TEST_F(AppTest, CmdLineMultipleDuplicateExtraConfig) {
  std::string duplicate = "config_a.conf";
  std::vector<std::string> argv = {
      "-c",
      config_dir.join("config_a.conf").str(),
      "--extra-config",
      config_dir.join("mysqlrouter.conf").str(),
      "-a",
      config_dir.join(duplicate).str(),
      "--extra-config",
      config_dir.join(duplicate).str(),
  };
  ASSERT_THROW({ MySQLRouter r(g_program_name, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_program_name, argv);
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("The configuration file"));
    EXPECT_THAT(exc.what(), HasSubstr(duplicate));
    EXPECT_THAT(exc.what(), HasSubstr("is provided multiple times"));
  }
}

TEST_F(AppTest, CmdLineExtraConfigNoDefaultFail) {
  /*
   * Check if mysqlrouter.conf does not exist in default locations.
   */
  std::stringstream ss_line{CONFIG_FILES};

  for (string path; std::getline(ss_line, path, ';');) {
    // malformed env var will result in error, valid or missing env var results
    // in success
    bool parse_ok = mysqlrouter::substitute_envvar(path);
    if (parse_ok) {
      std::string real_path = mysqlrouter::substitute_variable(
          path, "{origin}",
          mysql_harness::Path(g_program_name).dirname().str());
      ASSERT_FALSE(mysql_harness::Path(real_path).exists())
          << "expected that '" << real_path << "' (part of CONFIG_FILES='"
          << CONFIG_FILES << "') does not exist";
    }
  }

  std::vector<std::string> argv = {
      "--extra-config",
      config_dir.join("mysqlrouter.conf").str(),
  };
  ASSERT_THROW({ MySQLRouter r(g_program_name, argv); }, std::runtime_error);
  try {
    MySQLRouter r(g_program_name, argv);
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(), HasSubstr("Extra configuration files"));
    EXPECT_THAT(exc.what(),
                HasSubstr(" provided, but neither default configuration files "
                          "nor --config=<file> are readable files"));
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
    EXPECT_THAT(exc.what(), HasSubstr("The configuration file"));
    EXPECT_THAT(exc.what(), HasSubstr("is not readable"));
  }
}

#ifndef _WIN32
TEST_F(AppTest, CmdLineUserBeforeBootstrap) {
  MySQLRouter router;
  std::vector<std::string> arguments = {"--user", "mysqlrouter", "--bootstrap",
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
  std::vector<std::string> arguments = {"-u", "mysqlrouter", "--bootstrap",
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
#endif  // #ifndef _WIN32

TEST_F(AppTest, CmdLineVersion) {
  std::vector<std::string> argv = {"--version"};

  // filter out the ANSI ESC sequences
  std::stringstream out_stream;
  Vt100Filter filtered_out_streambuf(out_stream.rdbuf());
  std::ostream filtered_out_stream(&filtered_out_streambuf);

  MySQLRouter r(g_program_name, argv, filtered_out_stream);
  ASSERT_THAT(out_stream.str(), StartsWith(r.get_version_line()));
}

TEST_F(AppTest, CmdLineVersionShort) {
  std::vector<std::string> argv = {"-V"};

  // filter out the ANSI ESC sequences
  std::stringstream out_stream;
  Vt100Filter filtered_out_streambuf(out_stream.rdbuf());
  std::ostream filtered_out_stream(&filtered_out_streambuf);

  MySQLRouter r(g_program_name, argv, filtered_out_stream);
  ASSERT_THAT(out_stream.str(), StartsWith("MySQL Router"));
}

TEST_F(AppTest, CmdLineHelp) {
  std::vector<std::string> argv = {"--help"};
  // filter out the ANSI ESC sequences
  std::stringstream out_stream;
  Vt100Filter filtered_out_streambuf(out_stream.rdbuf());
  std::ostream filtered_out_stream(&filtered_out_streambuf);

  MySQLRouter r(g_program_name, argv, filtered_out_stream);

  // several substrings from help output that are unlikely to change soon
  EXPECT_THAT(out_stream.str(), HasSubstr("MySQL Router  V"));
  EXPECT_THAT(
      out_stream.str(),
      HasSubstr(
          "Oracle is a registered trademark of Oracle Corporation and/or its"));
  EXPECT_THAT(out_stream.str(), HasSubstr("Usage\n\nmysqlrouter"));
}

TEST_F(AppTest, CmdLineHelpShort) {
  std::vector<std::string> argv = {"-?"};
  std::stringstream out_stream;
  Vt100Filter filtered_out_streambuf(out_stream.rdbuf());
  std::ostream filtered_out_stream(&filtered_out_streambuf);
  MySQLRouter r(g_program_name, argv, filtered_out_stream);

  // several substrings from help output that are unlikely to change soon
  EXPECT_THAT(out_stream.str(), HasSubstr("MySQL Router  V"));
  EXPECT_THAT(
      out_stream.str(),
      HasSubstr(
          "Oracle is a registered trademark of Oracle Corporation and/or its"));
  EXPECT_THAT(out_stream.str(), HasSubstr("Usage\n\nmysqlrouter"));
}

TEST_F(AppTest, ConfigFileParseError) {
  std::vector<std::string> argv = {
      "--config",
      config_dir.join("parse_error.conf").str(),
  };
  ASSERT_THROW(
      {
        MySQLRouter r(g_program_name, argv);
        r.start();
      },
      std::runtime_error);
  try {
    MySQLRouter r(g_program_name, argv);
    r.start();
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(),
                HasSubstr("Configuration error: Malformed section header:"));
  }
}

TEST_F(AppTest, SectionOverMultipleConfigFiles) {
  std::string extra_config = config_dir.join("mysqlrouter_extra.conf").str();
  std::vector<std::string> argv = {"--config",
                                   config_dir.join("mysqlrouter.conf").str(),
                                   "--extra-config=" + extra_config};
  ASSERT_NO_THROW({ MySQLRouter r(g_program_name, argv); });

  MySQLRouter r(g_program_name, argv);
  ASSERT_THAT(r.get_config_files().at(0).c_str(), EndsWith("mysqlrouter.conf"));
  ASSERT_THAT(r.get_extra_config_files().at(0).c_str(),
              EndsWith("mysqlrouter_extra.conf"));

  // let the Loader load the configuration files
  ASSERT_NO_THROW(r.start());

  auto section = r.loader_->get_config().get(kPluginNameMagic, "");
  ASSERT_THAT(section.get("foo"), StrEq("bar"));
  ASSERT_THROW(section.get("NotInTheSection"), mysql_harness::bad_option);
}

TEST_F(AppTest, CanStartTrue) {
  std::vector<std::string> argv = {"--config",
                                   config_dir.join("mysqlrouter.conf").str()};
  ASSERT_NO_THROW({ MySQLRouter r(g_program_name, argv); });
}

TEST_F(AppTest, CanStartFalse) {
  std::vector<std::vector<std::string>> cases = {
      {""},
  };
  for (auto &argv : cases) {
    ASSERT_THROW(
        {
          MySQLRouter r(g_program_name, argv);
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

  std::vector<std::string> argv = {
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
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs([&] {
            ASSERT_FALSE(mysql_harness::DIM::instance()
                             .get_LoggingRegistry()
                             .is_ready());
          }),
          // we proved that the user got set first, now init the logger properly
          // for the further loader to use it
          testing::InvokeWithoutArgs([&] { init_test_logger(); }),
          (Return(0))));

  MySQLRouter r(g_program_name, argv, std::cout, std::cerr,
                mock_sys_user_operations.get());
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
  Scope_guard exit_guard(
      [&]() { mysql_harness::delete_dir_recursive(tmp_dir); });

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

  std::vector<std::string> argv = {
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
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs([&] {
            ASSERT_FALSE(mysql_harness::DIM::instance()
                             .get_LoggingRegistry()
                             .is_ready());
          }),
          // we proved that the user got set first, now init the logger properly
          // for the further loader to use it
          testing::InvokeWithoutArgs([&] { init_test_logger(); }),
          (Return(0))));

  MySQLRouter r(g_program_name, argv, std::cout, std::cerr,
                mock_sys_user_operations.get());
  ASSERT_NO_THROW(r.start());
}

#endif  // #ifndef _WIN32

TEST_F(AppTest, ShowingInfoTrue) {
  std::vector<std::vector<std::string>> cases = {
      {"--help"},
      {"--version"},
      {"--help", "--config", config_dir.join("mysqlrouter.conf").str()},
      {"--config", config_dir.join("mysqlrouter.conf").str(), "--help"},
  };

  // Make sure we do not start when showing information
  for (auto &argv : cases) {
    // filter out the ANSI ESC sequences
    std::stringstream out_stream;
    Vt100Filter filtered_out_streambuf(out_stream.rdbuf());
    std::ostream filtered_out_stream(&filtered_out_streambuf);

    ASSERT_NO_THROW({
      MySQLRouter r(g_program_name, argv, filtered_out_stream);
      r.start();
    });
    ASSERT_THAT(out_stream.str(), HasSubstr("MySQL Router  V")) << argv[0];
  }
}

TEST_F(AppTest, ShowingInfoFalse) {
  // Cases should be allowing Router to start
  std::vector<std::vector<std::string>> cases = {
      {"--config", config_dir.join("mysqlrouter.conf").str(),
       "--extra-config=" + config_dir.join("mysqlrouter_extra.conf").str()}};

  for (auto &argv : cases) {
    ASSERT_NO_THROW({
      MySQLRouter r(g_program_name, argv);
      r.start();
    });
  }
}

#ifndef _WIN32

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
  std::vector<std::string> argv = {"--bootstrap", "127.0.0.1:3060"};

  EXPECT_CALL(*mock_sys_user_operations, geteuid())
      .Times(1)
      .WillOnce(Return(0));

  try {
    MySQLRouter r(g_program_name, argv, std::cout, std::cerr,
                  mock_sys_user_operations.get());
    FAIL() << "Should throw";
  } catch (const std::runtime_error &exc) {
    EXPECT_THAT(exc.what(),
                StartsWith("You are bootstrapping as a superuser."));
  }
}

/**
 * @test
 *      Verify that std::runtime_error is thrown when --master-key-reader option
 * is used in non-bootstrap mode.
 */
TEST_F(AppTest, ThrowWhenMasterKeyReaderUsedWithoutBootstrap) {
  std::vector<std::string> argv = {"--master-key-reader=reader.sh"};
  ASSERT_THROW_LIKE(MySQLRouter(g_program_name, argv, std::cout, std::cerr,
                                mock_sys_user_operations.get()),
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
  std::vector<std::string> argv = {"--master-key-writer=writer.sh"};
  ASSERT_THROW_LIKE(MySQLRouter(g_program_name, argv, std::cout, std::cerr,
                                mock_sys_user_operations.get()),
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
  std::vector<std::string> argv = {"--bootstrap", "127.0.0.1:3060",
                                   "--master-key-reader"};
  ASSERT_THROW_LIKE(
      MySQLRouter(g_program_name, argv, std::cout, std::cerr,
                  mock_sys_user_operations.get()),
      std::runtime_error,
      "option '--master-key-reader' expects a value, got nothing");
}

/**
 * @test
 *       Verify that std::runtime_error is thrown when --master-key-writer
 * option is used without value.
 */
TEST_F(AppTest, ThrowWhenMasterKeyWriterUsedWithoutValue) {
  std::vector<std::string> argv = {"--bootstrap", "127.0.0.1:3060",
                                   "--master-key-writer"};
  ASSERT_THROW_LIKE(
      MySQLRouter(g_program_name, argv, std::cout, std::cerr,
                  mock_sys_user_operations.get()),
      std::runtime_error,
      "option '--master-key-writer' expects a value, got nothing");
}

/**
 * @test
 *       Verify that std::runtime_error is throw when --master-key-reader option
 * is used without using --master-key-writer option.
 */
TEST_F(AppTest, ThrowWhenMasterKeyReaderUsedWithoutMasterKeyWriter) {
  std::vector<std::string> argv = {"--bootstrap", "127.0.0.1:3060",
                                   "--master-key-reader=reader.sh"};
  ASSERT_THROW_LIKE(MySQLRouter(g_program_name, argv, std::cout, std::cerr,
                                mock_sys_user_operations.get()),
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
  std::vector<std::string> argv = {"--bootstrap", "127.0.0.1:3060",
                                   "--master-key-writer=writer.sh"};
  ASSERT_THROW_LIKE(MySQLRouter(g_program_name, argv, std::cout, std::cerr,
                                mock_sys_user_operations.get()),
                    std::runtime_error,
                    "Option --master-key-writer can only be used together with "
                    "--master-key-reader.");
}

#endif  // #ifndef _WIN32

class AppLoggerTest : public ConsoleOutputTest {
 protected:
  void SetUp() override {
    set_origin(Path(g_program_name).dirname());
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
    ofs_config << "[" << kPluginNameMagic << "]\n";  // magic plugin
    ofs_config << "do_magic = yes\n";
    ofs_config << "message = It is some kind of magic\n";
    ofs_config << "\n";
    ofs_config << "[" << kPluginNameLifecycle3
               << "]\n";  // lifecycle3 plugin (lifecycle dependency)
    ofs_config << "[" << kPluginNameLifecycle
               << ":instance1]\n";  // lifecycle plugin
    ofs_config.close();
  } else {
    throw std::runtime_error("Failed creating config file '" +
                             config_path.str() + "'");
  }

  // run MySQLRouter
  reset_ssout();
  std::vector<std::string> argv = {"-c", config_path.c_str()};
  MySQLRouter r(g_program_name, argv);
  ASSERT_NO_THROW(r.start()) << get_log_stream().str();

  // verify that all plugins have a module registered with the logger
  auto loggers =
      mysql_harness::DIM::instance().get_LoggingRegistry().get_logger_names();
  EXPECT_THAT(loggers, testing::UnorderedElementsAre(
                           mysql_harness::logging::kMainLogger,
                           kPluginNameMagic, kPluginNameLifecycle,
                           kPluginNameLifecycle3, "sql", "logger"));

  // verify the log contains what we expect it to contain. We're looking for
  // lines like this:
  {
    // 2017-05-03 11:30:25 magic INFO [7ffff5e34700] It is some kind of magic
    EXPECT_THAT(get_log_stream().str(),
                HasSubstr(" " + kPluginNameMagic + " INFO "));
    EXPECT_THAT(get_log_stream().str(), HasSubstr(" It is some kind of magic"));

    // 2017-05-03 11:30:25 lifecycle INFO [7faefa705780] lifecycle:all
    // init():begin
    EXPECT_THAT(get_log_stream().str(),
                HasSubstr(" " + kPluginNameLifecycle + " INFO "));
    EXPECT_THAT(get_log_stream().str(),
                HasSubstr(" lifecycle:all init():begin"));
  }
}

TEST_F(AppTest, EmptyConfigPath) {
  std::vector<std::string> argv = {"--config", ""};
  EXPECT_THROW({ MySQLRouter r(g_program_name, argv); }, std::runtime_error);
}

/**
 * @test
 * Verify that --https-port could not be used outside of the bootstrap.
 */
TEST_F(AppTest, https_port_not_in_bootstrap) {
  std::vector<std::string> argv = {"--https-port", "8080"};
  ASSERT_THROW_LIKE(MySQLRouter(g_program_name, argv), std::runtime_error,
                    "Option --https-port can only be used together with "
                    "-B/--bootstrap");
}

/**
 * @test
 * Verify that --disable-rest could not be used outside of the bootstrap.
 */
TEST_F(AppTest, disable_rest_not_in_bootstrap) {
  std::vector<std::string> argv = {"--disable-rest"};
  ASSERT_THROW_LIKE(MySQLRouter(g_program_name, argv), std::runtime_error,
                    "Option --disable-rest can only be used together with "
                    "-B/--bootstrap");
}

/**
 * @test
 * Verify that --disable-rest does not take any arguments.
 */
TEST_F(AppTest, disable_rest_with_value) {
  std::vector<std::string> argv = {"--bootstrap", "127.0.0.1:5000",
                                   "--disable-rest", "not_allowed"};
  ASSERT_THROW_LIKE(MySQLRouter(g_program_name, argv), std::runtime_error,
                    "invalid argument 'not_allowed'.");
}

/**
 * @test
 * Verify that --disable-rest and --https-port are mututally exclusive.
 */
TEST_F(AppTest, https_port_with_disable_rest) {
  std::vector<std::string> argv = {"--bootstrap", "127.0.0.1:5000",
                                   "--https-port", "8080", "--disable-rest"};
  ASSERT_THROW_LIKE(
      MySQLRouter(g_program_name, argv), std::runtime_error,
      "Option --disable-rest is not allowed when using --https-port option");
}

/**
 * @test
 * Verify that --https-port does not accept values lower than 1.
 *
 * WL13906:TS_FailReq02_01
 */
TEST_F(AppTest, https_port_out_of_range_low) {
  std::vector<std::string> argv = {"--bootstrap", "127.0.0.1:5000",
                                   "--https-port", "0"};
  ASSERT_THROW_LIKE(MySQLRouter(g_program_name, argv), std::runtime_error,
                    "processing --https-port option failed, not in allowed "
                    "range [1, 65535]");
}

/**
 * @test
 * Verify that --https-port does not accept values greater than 65535.
 *
 * WL13906:TS_FailReq02_03
 */
TEST_F(AppTest, https_port_out_of_range_high) {
  std::vector<std::string> argv = {"--bootstrap", "127.0.0.1:5000",
                                   "--https-port", "65599"};
  ASSERT_THROW_LIKE(MySQLRouter(g_program_name, argv), std::runtime_error,
                    "processing --https-port option failed, not in allowed "
                    "range [1, 65535]");
}

/**
 * @test
 * Verify that --https-port does not accept negative values.
 *
 * WL13906:TS_FailReq02_02
 */
TEST_F(AppTest, https_port_out_of_range_negative) {
  std::vector<std::string> argv = {"--bootstrap", "127.0.0.1:5000",
                                   "--https-port", "-1"};
  ASSERT_THROW_LIKE(MySQLRouter(g_program_name, argv), std::runtime_error,
                    "option '--https-port' expects a value, got nothing");
}

/**
 * @test
 * Verify that --https-port does not accept floating point values.
 *
 * WL13906:TS_FailReq02_04
 */
TEST_F(AppTest, https_port_float) {
  std::vector<std::string> argv = {"--bootstrap", "127.0.0.1:5000",
                                   "--https-port", "1.2"};
  ASSERT_THROW_LIKE(
      MySQLRouter(g_program_name, argv), std::runtime_error,
      "processing --https-port option failed, invalid value: 1.2");
}

/**
 * @test
 * Verify that --https-port does not accept string values.
 */
TEST_F(AppTest, https_port_nan) {
  std::vector<std::string> argv = {"--bootstrap", "127.0.0.1:5000",
                                   "--https-port", "not-a-number"};
  ASSERT_THROW_LIKE(
      MySQLRouter(g_program_name, argv), std::runtime_error,
      "processing --https-port option failed, invalid value: not-a-number");
}

/**
 * @test
 * Verify that --https-port has to be called with an argument.
 */
TEST_F(AppTest, https_port_without_value) {
  std::vector<std::string> argv = {"--bootstrap", "127.0.0.1:5000",
                                   "--https-port"};
  ASSERT_THROW_LIKE(MySQLRouter(g_program_name, argv), std::runtime_error,
                    "option '--https-port' expects a value, got nothing");
}

int main(int argc, char *argv[]) {
  g_program_name = argv[0];

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
