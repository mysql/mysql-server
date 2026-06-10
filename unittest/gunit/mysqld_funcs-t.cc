/* Copyright (c) 2019, 2026, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include <string.h>
#include <filesystem>
#include "my_config.h"
#include "sql/manifest_file_option_parser_helper.h"

#ifdef HAVE_GETPWNAM
#include "my_getpwnam.h"  // PasswdValue
#endif                    /* HAVE_GETPWNAM */

extern char *mysql_home_ptr;
extern char mysql_home[];

// Unit tests for functions in mysqld.cc.
namespace mysqld_funcs_unit_test {
namespace {

#ifndef _WIN32
std::string relative_default_plugin_dir() {
  std::string plugindir{PLUGINDIR};
  const std::string default_home{DEFAULT_MYSQL_HOME};

  // Mirror mysqld's get_relative_path() behavior for test expectations.
  if (!default_home.empty() && default_home != "/" &&
      plugindir.rfind(default_home, 0) == 0) {
    size_t pos = default_home.size();
    while (pos < plugindir.size() &&
           (plugindir[pos] == '/' || plugindir[pos] == '\\')) {
      ++pos;
    }
    plugindir.erase(0, pos);
  }

  if (plugindir.empty() || plugindir.back() != '/') plugindir.push_back('/');
  return plugindir;
}
#endif

}  // namespace

#ifdef HAVE_GETPWNAM
PasswdValue check_user_drv(const char *user);

TEST(MysqldFuncs, CheckUser) {
  EXPECT_TRUE(check_user_drv("root").IsVoid());

  if (geteuid() == 0) {
    // Running as root
    EXPECT_FALSE(check_user_drv("0").IsVoid());
    EXPECT_FALSE(check_user_drv("1").IsVoid());
    EXPECT_FALSE(check_user_drv("bin").IsVoid());
  } else {
    // These would trigger unireg_abort if run as root, and
    // unireg_abort currently triggers crash if run in a unit test
    EXPECT_TRUE(check_user_drv(nullptr).IsVoid());
    EXPECT_TRUE(check_user_drv("thereisnosuchuser___").IsVoid());
    EXPECT_TRUE(check_user_drv("0").IsVoid());
    EXPECT_TRUE(check_user_drv("0abc").IsVoid());
    EXPECT_TRUE(check_user_drv("1").IsVoid());
    EXPECT_TRUE(check_user_drv("bin").IsVoid());
  }
}
#endif /* HAVE_GETPWNAM */

class ManifestFileOptionParserHelper : public ::testing::Test {
 protected:
#ifdef _WIN32
  const std::string mysql_binary_dir{std::filesystem::current_path().string() +
                                     "\\"};
  const std::string default_opt_plugin_dir{"lib\\plugin\\"};
  const char *default_real_data_home{"data\\"};
#else
  const std::string mysql_binary_dir{std::filesystem::current_path().string() +
                                     "/"};
  const char *default_real_data_home{"data/"};
  const std::string default_opt_plugin_dir{relative_default_plugin_dir()};
#endif
  const char *initial_real_data_home{"data"};
  const char *initial_opt_plugin_dir{"blahblahblah"};

  std::string save_mysql_home;
  std::string save_mysql_real_data_home;
  std::string save_opt_plugin_dir;

  void SetUp() override {
    mysql_home_ptr = mysql_home;
    save_mysql_home = mysql_home;
    strcpy(mysql_home, mysql_binary_dir.c_str());
    save_mysql_real_data_home = mysql_real_data_home;
    strcpy(mysql_real_data_home, initial_real_data_home);
    save_opt_plugin_dir = opt_plugin_dir;
    strcpy(opt_plugin_dir, initial_opt_plugin_dir);
  }
  void TearDown() override {
    strcpy(mysql_home, save_mysql_home.c_str());
    strcpy(mysql_real_data_home, save_mysql_real_data_home.c_str());
    strcpy(opt_plugin_dir, save_opt_plugin_dir.c_str());
  }

  void test_options(int argc, const char **argv,
                    const std::string &expect_datadir,
                    const std::string &expect_plugindir) {
    {
      Manifest_file_option_parser_helper obj{argc, const_cast<char **>(argv)};
      EXPECT_STREQ(mysql_real_data_home, expect_datadir.c_str());
      EXPECT_STREQ(opt_plugin_dir, expect_plugindir.c_str());
    }

    // mysql_real_data_home and opt_plugin_dir must be preserved
    EXPECT_STREQ(mysql_real_data_home, initial_real_data_home);
    EXPECT_STREQ(opt_plugin_dir, initial_opt_plugin_dir);
  }
};

TEST_F(ManifestFileOptionParserHelper, AbsoluteOption) {
#ifdef _WIN32
  std::string expect_datadir{"C:\\somedir\\ddd\\"};
  std::string expect_plugindir{"C:\\somedir\\ppp\\"};
  const char *argv[] = {"path", "--datadir=C:\\somedir\\ddd\\",
                        "--plugin-dir=C:\\somedir\\ppp\\"};
#else
  std::string expect_datadir{"/somedir/ddd/"};
  std::string expect_plugindir{"/somedir/ppp/"};
  const char *argv[] = {"path", "--datadir=/somedir/ddd",
                        "--plugin-dir=/somedir/ppp"};
#endif
  constexpr int argc = std::size(argv);
  test_options(argc, argv, expect_datadir, expect_plugindir);
}

TEST_F(ManifestFileOptionParserHelper, RelativeOptionAbsoluteBasedir) {
#ifdef _WIN32
  std::string basedir{"d:\\somedir\\mysql\\"};
  std::string expect_datadir{basedir + "ddd\\"};
  std::string expect_plugindir{basedir + "ppp\\"};
  const char *argv[] = {"path", "--basedir=d:\\somedir\\mysql", "--datadir=ddd",
                        "--plugin-dir=ppp"};
#else
  std::string basedir{"/somedir/mysql/"};
  std::string expect_datadir{basedir + "ddd/"};
  std::string expect_plugindir{basedir + "ppp/"};
  const char *argv[] = {"path", "--basedir=/somedir/mysql", "--datadir=ddd",
                        "--plugin-dir=ppp"};
#endif
  constexpr int argc = std::size(argv);
  test_options(argc, argv, expect_datadir, expect_plugindir);
}

TEST_F(ManifestFileOptionParserHelper, RelativeOptionRelativeBasedir) {
#ifdef _WIN32
  std::string basedir{"somedir\\"};
  std::string expect_datadir{mysql_binary_dir + basedir + "ddd\\"};
  std::string expect_plugindir{mysql_binary_dir + basedir + "ppp\\"};
#else
  std::string basedir{"somedir/"};
  std::string expect_datadir{basedir + "ddd/"};
  std::string expect_plugindir{basedir + "ppp/"};
#endif
  const char *argv[] = {"path", "--basedir=somedir", "--datadir=ddd",
                        "--plugin-dir=ppp"};
  constexpr int argc = std::size(argv);
  test_options(argc, argv, expect_datadir, expect_plugindir);
}

TEST_F(ManifestFileOptionParserHelper, RelativeOptionNoBasedir) {
#ifdef _WIN32
  std::string expect_datadir{mysql_binary_dir + "ddd\\"};
  std::string expect_plugindir{mysql_binary_dir + "ppp\\"};
#else
  std::string expect_datadir{mysql_binary_dir + "ddd/"};
  std::string expect_plugindir{mysql_binary_dir + "ppp/"};
#endif
  const char *argv[] = {"path", "--datadir=ddd", "--plugin-dir=ppp"};
  constexpr int argc = std::size(argv);
  test_options(argc, argv, expect_datadir, expect_plugindir);
}

TEST_F(ManifestFileOptionParserHelper, NoOptionAbsoluteBasedir) {
#ifdef _WIN32
  std::string basedir{"d:\\somedir\\mysql\\"};
  const char *argv[] = {"path", "--basedir=d:\\somedir\\mysql"};
#else
  std::string basedir{"/somedir/mysql/"};
  const char *argv[] = {"path", "--basedir=/somedir/mysql"};
#endif
  std::string expect_datadir{basedir + default_real_data_home};
  std::string expect_plugindir{basedir + default_opt_plugin_dir};
  constexpr int argc = std::size(argv);
  test_options(argc, argv, expect_datadir, expect_plugindir);
}

TEST_F(ManifestFileOptionParserHelper, NoOptionRelativeBasedir) {
#ifdef _WIN32
  std::string basedir{"somedir\\"};
  std::string expect_datadir{mysql_binary_dir + basedir +
                             default_real_data_home};
  std::string expect_plugindir{mysql_binary_dir + basedir +
                               default_opt_plugin_dir};
#else
  std::string basedir{"somedir/"};
  std::string expect_datadir{basedir + default_real_data_home};
  std::string expect_plugindir{basedir + default_opt_plugin_dir};
#endif
  const char *argv[] = {"path", "--basedir=somedir"};
  constexpr int argc = std::size(argv);
  test_options(argc, argv, expect_datadir, expect_plugindir);
}

TEST_F(ManifestFileOptionParserHelper, NoOptionNoBasedir) {
  std::string expect_datadir{mysql_binary_dir + default_real_data_home};
  std::string expect_plugindir{mysql_binary_dir + default_opt_plugin_dir};
  const char *argv[] = {"path"};
  constexpr int argc = std::size(argv);
  test_options(argc, argv, expect_datadir, expect_plugindir);
}

TEST_F(ManifestFileOptionParserHelper, ShortAbsoluteOptions) {
#ifdef _WIN32
  std::string expect_datadir{"C:\\somedir\\ddd\\"};
  std::string basedir{"d:\\somedir\\mysql\\"};
  const char *argv[] = {"path", "-h", "C:\\somedir\\ddd\\", "-b",
                        "d:\\somedir\\mysql"};
#else
  std::string expect_datadir{"/somedir/ddd/"};
  std::string basedir{"/somedir/mysql/"};
  const char *argv[] = {"path", "-h", "/somedir/ddd", "-b", "/somedir/mysql"};
#endif
  std::string expect_plugindir{basedir + default_opt_plugin_dir};
  constexpr int argc = std::size(argv);
  test_options(argc, argv, expect_datadir, expect_plugindir);
}

}  // namespace mysqld_funcs_unit_test
