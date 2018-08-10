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

/**
 * Test the mysqlrouter_plugin_info tool.
 */

#include "gmock/gmock.h"

#include "plugin_info_app.h"

#include "mysql/harness/filesystem.h"
#include "mysql/harness/plugin.h"
#include "router_config.h"

#include <iostream>
#include <sstream>

using mysql_harness::Path;

using testing::StartsWith;
using testing::StrEq;
using testing::ValuesIn;
using testing::WithParamInterface;

using std::string;

Path g_origin_path;

class PluginInfoAppTest : public ::testing::Test {
 protected:
  virtual void SetUp();

  void verify_help_output();
  void verify_version_output();
  void verify_plugin_info(const string &brief, const string &version,
                          const string &requires, const string &conflicts);

  string get_plugin_file_path(const string &plugin_name);

  std::stringstream out_stream_;
  std::stringstream out_stream_err_;

  Path plugin_dir_;
};

const char *kPluginInfoAppExeFileName = "mysqlrouter_plugin_info";

void PluginInfoAppTest::SetUp() {
  out_stream_.str("");
  out_stream_err_.str("");

  plugin_dir_ = mysql_harness::get_plugin_dir(g_origin_path.str());
}

string PluginInfoAppTest::get_plugin_file_path(const string &plugin_name) {
  Path plugin_path = plugin_dir_;
  string plugin_file = plugin_name;

#ifndef _WIN32
  plugin_file += ".so";
#else
  plugin_file += ".dll";
#endif

  plugin_path.append(plugin_file);
  return plugin_path.str();
}

void PluginInfoAppTest::verify_help_output() {
  const string kHelpOutput =
      "Usage:\n"
      "\tmysqlrouter_plugin_info <mysqlrouter_plugin_file> "
      "<mysql_plugin_name>\n"
      "Example:\n"
#ifndef _WIN32
      "\tmysqlrouter_plugin_info /usr/lib/mysqlrouter/routing.so routing\n"
#else
      "\tmysqlrouter_plugin_info \"c:\\Program Files (x86)\\MySQL\\MySQL "
      "Router 2.1\\lib\\routing.dll\" routing\n"
#endif
      "To print help information:\n"
      "\tmysqlrouter_plugin_info --help\n"
      "To print application version:\n"
      "\tmysqlrouter_plugin_info --version\n";

  EXPECT_THAT(out_stream_.str(), StrEq(""));
  EXPECT_THAT(out_stream_err_.str(), StrEq(kHelpOutput));
}

void PluginInfoAppTest::verify_version_output() {
  std::string edition{MYSQL_ROUTER_VERSION_EDITION};

  const string kVersionOutput =
      kPluginInfoAppName + " " + "v" +
      MYSQL_ROUTER_VERSION  // we use the same version as MySQLRouter
      + " on " + MYSQL_ROUTER_PACKAGE_PLATFORM + " (" +
      (MYSQL_ROUTER_PACKAGE_ARCH_64BIT ? "64-bit" : "32-bit") + ")" +
      (edition.empty() ? "" : " (" + edition + ")") + "\n";

  EXPECT_THAT(out_stream_.str(), StrEq(""));
  EXPECT_THAT(out_stream_err_.str(), StrEq(kVersionOutput));
}

void PluginInfoAppTest::verify_plugin_info(const string &brief,
                                           const string &version,
                                           const string &requires,
                                           const string &conflicts) {
  EXPECT_THAT(out_stream_err_.str(), StrEq(""));

  const auto abi_version = ::mysql_harness::PLUGIN_ABI_VERSION;
  const std::string abi_version_str =
      std::to_string(ABI_VERSION_MAJOR(abi_version)) + "." +
      std::to_string(ABI_VERSION_MINOR(abi_version));

  const string expected_json =
      "{\n"
      "    \"abi-version\": \"" +
      abi_version_str +
      "\",\n"
      "    \"arch-descriptor\": \"" +
      string(mysql_harness::ARCHITECTURE_DESCRIPTOR) +
      "\",\n"
      "    \"brief\": \"" +
      brief +
      "\",\n"
      "    \"plugin-version\": \"" +
      version +
      "\",\n"
      "    \"requires\": [" +
      requires +
      "],\n"
      "    \"conflicts\": [" +
      conflicts +
      "]\n"
      "}\n";

  EXPECT_THAT(out_stream_.str(), StrEq(expected_json));
}

TEST_F(PluginInfoAppTest, NoParametersPassed) {
  const char *argv[] = {kPluginInfoAppExeFileName};
  const int argc = static_cast<int>(sizeof(argv) / sizeof(char *));
  Plugin_info_app plugin_info_app(argc, argv, out_stream_, out_stream_err_);

  int res = plugin_info_app.run();

  // if the mysqlrouter_plugin_info was called with no command line options
  // we expect usage being printed to error stream and app returning with -1
  EXPECT_EQ(-1, res);
  verify_help_output();
}

TEST_F(PluginInfoAppTest, HelpRequested) {
  const char *argv[] = {kPluginInfoAppExeFileName, "--help"};
  const int argc = static_cast<int>(sizeof(argv) / sizeof(char *));
  Plugin_info_app plugin_info_app(argc, argv, out_stream_, out_stream_err_);

  int res = plugin_info_app.run();

  // if the mysqlrouter_plugin_info was called with --help parameter
  // we expect usage being printed to error stream and app returning with 0
  EXPECT_EQ(0, res);
  verify_help_output();
}

TEST_F(PluginInfoAppTest, VersionRequested) {
  const char *argv[] = {kPluginInfoAppExeFileName, "--version"};
  const int argc = static_cast<int>(sizeof(argv) / sizeof(char *));
  Plugin_info_app plugin_info_app(argc, argv, out_stream_, out_stream_err_);

  int res = plugin_info_app.run();

  // if the mysqlrouter_plugin_info was called with --version parameter
  // we expect the version being printed to error stream and app returning with
  // 0
  EXPECT_EQ(0, res);
  verify_version_output();
}

TEST_F(PluginInfoAppTest, WrongNumberOfParams) {
  const char *argv[] = {kPluginInfoAppExeFileName, "one", "two", "three"};
  const int argc = static_cast<int>(sizeof(argv) / sizeof(char *));
  Plugin_info_app plugin_info_app(argc, argv, out_stream_, out_stream_err_);

  int res = plugin_info_app.run();

  // if the mysqlrouter_plugin_info was called with too many command line
  // parameters we expect usage being printed to error stream and app returning
  // with -1
  EXPECT_EQ(-1, res);
  verify_help_output();
}

TEST_F(PluginInfoAppTest, NonExistingLibrary) {
  const char *non_existing_plugin = "non_existing_plugin";
  std::string lib_path = get_plugin_file_path(non_existing_plugin);
  const char *argv[] = {kPluginInfoAppExeFileName, lib_path.c_str(),
                        non_existing_plugin};
  const int argc = static_cast<int>(sizeof(argv) / sizeof(char *));

  Plugin_info_app plugin_info_app(argc, argv, out_stream_, out_stream_err_);
  int res = plugin_info_app.run();

  EXPECT_EQ(-1, res);

  const std::string expected_error = "Could not load plugin file: ";

  // check if correct error gets printed on error stream
  EXPECT_THAT(out_stream_.str(), StrEq(""));
  EXPECT_THAT(out_stream_err_.str(), StartsWith(expected_error));
}

TEST_F(PluginInfoAppTest, NonPluginExistingLibrary) {
  // we use mysql_protocol which is an existing library but it's not a plugin
  // so should not have Plugin struct exported/defined
  const char *non_plugin_lib = "mysql_protocol";
  std::string lib_path = get_plugin_file_path(non_plugin_lib);
  const char *argv[] = {kPluginInfoAppExeFileName, lib_path.c_str(),
                        non_plugin_lib};
  const int argc = static_cast<int>(sizeof(argv) / sizeof(char *));

  Plugin_info_app plugin_info_app(argc, argv, out_stream_, out_stream_err_);
  int res = plugin_info_app.run();

  EXPECT_EQ(-1, res);

  const std::string expected_error =
      "Loading plugin information for '" + lib_path + "' failed:";

  // check if correct error gets printed on error stream
  EXPECT_THAT(out_stream_.str(), StrEq(""));
  EXPECT_THAT(out_stream_err_.str(), StartsWith(expected_error));
}

//
// Check if the expected information is printed for each of the plugins we
// currently ship with MySQLRouter
//

//                            <name,   brief,  version ,requires, conflicts>
using Plugin_data = std::tuple<string, string, string, string, string>;

class PluginInfoAppTestReadInfo : public PluginInfoAppTest,
                                  public WithParamInterface<Plugin_data> {};

TEST_P(PluginInfoAppTestReadInfo, ReadInfo) {
  const string plugin_name = std::get<0>(GetParam());
  const string plugin_brief = std::get<1>(GetParam());
  const string plugin_version = std::get<2>(GetParam());
  const string plugin_requires = std::get<3>(GetParam());
  const string plugin_conflicts = std::get<4>(GetParam());
  const string plugin_file_path = get_plugin_file_path(plugin_name);

  const char *argv[] = {kPluginInfoAppExeFileName, plugin_file_path.c_str(),
                        plugin_name.c_str()};
  const int argc = static_cast<int>(sizeof(argv) / sizeof(char *));

  Plugin_info_app plugin_info_app(argc, argv, out_stream_, out_stream_err_);

  int res = plugin_info_app.run();

  EXPECT_EQ(0, res);
  verify_plugin_info(plugin_brief, plugin_version, plugin_requires,
                     plugin_conflicts);
}

const Plugin_data router_plugins[]{
    Plugin_data{"routing",
                "Routing MySQL connections between MySQL clients/connectors "
                "and servers",
                "0.0.1", "", ""},
    Plugin_data{
        "metadata_cache",
        "Metadata Cache, managing information fetched from the Metadata Server",
        "0.0.1", "", ""},
    Plugin_data{"keepalive", "Keepalive Plugin", "0.0.1", "", ""},
};

INSTANTIATE_TEST_CASE_P(CheckReadInfo, PluginInfoAppTestReadInfo,
                        ValuesIn(router_plugins));

int main(int argc, char *argv[]) {
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
