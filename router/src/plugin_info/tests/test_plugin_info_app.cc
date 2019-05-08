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

#include "plugin_info_app.h"

#include <iostream>
#include <sstream>

#include <gmock/gmock.h>

#include "helpers/router_test_helpers.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/vt100.h"
#include "mysql/harness/vt100_filter.h"
#include "print_version.h"
#include "router_config.h"
#include "welcome_copyright_notice.h"

using mysql_harness::Path;

using testing::StartsWith;
using testing::StrEq;
using testing::ValuesIn;
using testing::WithParamInterface;

using std::string;

Path g_origin_path;

class PluginInfoAppTest : public ::testing::Test {
 protected:
  PluginInfoAppTest()
      : out_stream_(),
        filtered_out_streambuf_(out_stream_.rdbuf()),
        filtered_out_stream_(&filtered_out_streambuf_) {}
  void SetUp() override;

  void verify_version_output();
  void verify_plugin_info(const string &brief, const string &version,
                          const string &requires, const string &conflicts);

  string get_plugin_file_path(const string &plugin_name);

  std::stringstream out_stream_;
  Vt100Filter filtered_out_streambuf_;
  std::ostream filtered_out_stream_;
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

template <bool WithVt100>
static void verify_help_output(std::stringstream &out_stream) {
  const std::string kHelpOutput =
      (WithVt100 ? Vt100::render(Vt100::Render::Bold) : "") +
      "Usage: " + (WithVt100 ? Vt100::render(Vt100::Render::Normal) : "") +
      "mysqlrouter_plugin_info <mysqlrouter_plugin_file> " +
      "<plugin_name>\n\n" +
      (WithVt100 ? Vt100::render(Vt100::Render::Bold) : "") + "# Examples" +
      (WithVt100 ? Vt100::render(Vt100::Render::Normal) : "") +
      "\n\n"
      "Print plugin information:\n\n"
#ifndef _WIN32
      "    mysqlrouter_plugin_info /usr/lib/mysqlrouter/routing.so routing\n"
#else
      "    mysqlrouter_plugin_info \"c:\\Program Files\\MySQL\\MySQL "
      "Router 8.0\\lib\\routing.dll\" routing\n"
#endif
      "\n" +
      (WithVt100 ? Vt100::render(Vt100::Render::Bold) : "") + "# Options" +
      (WithVt100 ? Vt100::render(Vt100::Render::Normal) : "") +
      "\n\n"
      "  -V, --version\n"
      "      Display version information and exit.\n"
      "  -?, --help\n"
      "      Display this help and exit.\n";

  EXPECT_EQ(out_stream.str(), kHelpOutput);
}

void PluginInfoAppTest::verify_version_output() {
  std::string version_string;
  build_version(std::string(MYSQL_ROUTER_PACKAGE_NAME), &version_string);

  const string kVersionOutput =
      version_string + "\n" + ORACLE_WELCOME_COPYRIGHT_NOTICE("2015") + "\n";

  EXPECT_EQ(out_stream_.str(), kVersionOutput);
  EXPECT_THAT(out_stream_err_.str(), StrEq(""));
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
  PluginInfoFrontend plugin_info_app(kPluginInfoAppExeFileName, {}, out_stream_,
                                     out_stream_err_);

  EXPECT_THROW(plugin_info_app.run(), UsageError);
}

/**
 * @test ensure that --help leads to exit-code 0 and the help-text
 *       is sent to the out-stream.
 */
TEST_F(PluginInfoAppTest, HelpRequested_plain) {
  std::vector<std::string> args{"--help"};
  PluginInfoFrontend plugin_info_app(kPluginInfoAppExeFileName, args,
                                     filtered_out_stream_, out_stream_err_);

  int res = plugin_info_app.run();

  EXPECT_EQ(EXIT_SUCCESS, res);
  verify_help_output<false>(out_stream_);
}

/**
 * @test ensure that --help leads to exit-code 0 and the help-text
 *       is sent to the out-stream.
 */
TEST_F(PluginInfoAppTest, HelpRequested_vt100) {
  std::vector<std::string> args{"--help"};
  PluginInfoFrontend plugin_info_app(kPluginInfoAppExeFileName, args,
                                     out_stream_, out_stream_err_);

  int res = plugin_info_app.run();

  EXPECT_EQ(EXIT_SUCCESS, res);
  verify_help_output<true>(out_stream_);
}

/**
 * @test ensure that --version leads to exit-code 0 and the version-string
 *       is sent to the out-stream.
 */
TEST_F(PluginInfoAppTest, VersionRequested) {
  std::vector<std::string> args{"--version"};
  PluginInfoFrontend plugin_info_app(kPluginInfoAppExeFileName, args,
                                     out_stream_, out_stream_err_);

  int res = plugin_info_app.run();

  EXPECT_EQ(EXIT_SUCCESS, res);
  verify_version_output();
}

TEST_F(PluginInfoAppTest, WrongNumberOfParams) {
  std::vector<std::string> args{"one", "two", "three"};
  PluginInfoFrontend plugin_info_app(kPluginInfoAppExeFileName, args,
                                     out_stream_, out_stream_err_);

  EXPECT_THROW(plugin_info_app.run(), UsageError);
}

TEST_F(PluginInfoAppTest, NonExistingLibrary) {
  const char *plugin_name = "non_existing_plugin";
  std::string lib_path = get_plugin_file_path(plugin_name);
  std::vector<std::string> args{lib_path.c_str(), plugin_name};
  PluginInfoFrontend plugin_info_app(kPluginInfoAppExeFileName, args,
                                     out_stream_, out_stream_err_);

  const std::string expected_error = "Could not load plugin file: ";
  EXPECT_THROW_LIKE(plugin_info_app.run(), FrontendError, expected_error);

  // that nothing else is printed
  EXPECT_EQ(out_stream_.str(), "");
  EXPECT_EQ(out_stream_err_.str(), "");
}

// we use mysql_protocol which is an existing library but it's not a
// plugin so should not have Plugin struct exported/defined
TEST_F(PluginInfoAppTest, NonPluginExistingLibrary) {
  const char *plugin_name = "mysql_protocol";
  std::string lib_path = get_plugin_file_path(plugin_name);

  std::vector<std::string> args{lib_path.c_str(), plugin_name};
  PluginInfoFrontend plugin_info_app(kPluginInfoAppExeFileName, args,
                                     out_stream_, out_stream_err_);

  const std::string expected_error = "Loading plugin information for ";
  EXPECT_THROW_LIKE(plugin_info_app.run(), FrontendError, expected_error);

  // that nothing else is printed
  EXPECT_EQ(out_stream_.str(), "");
  EXPECT_EQ(out_stream_err_.str(), "");
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

  std::vector<std::string> args{plugin_file_path.c_str(), plugin_name.c_str()};

  PluginInfoFrontend plugin_info_app(kPluginInfoAppExeFileName, args,
                                     out_stream_, out_stream_err_);

  int res = plugin_info_app.run();

  EXPECT_EQ(EXIT_SUCCESS, res);
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
