/*
  Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <gmock/gmock.h>

#include "config_builder.h"
#include "mysql/harness/utility/string.h"
#include "router_component_test.h"
#include "temp_dir.h"

struct BrokenConfigParams {
  std::string test_name;

  std::vector<std::string> sections;

  std::string expected_logfile_substring;
  std::string expected_stderr_substring;
};

class RouterTestBrokenConfig
    : public RouterComponentTest,
      public ::testing::WithParamInterface<BrokenConfigParams> {
 protected:
  TempDirectory conf_dir_;
};

TEST_P(RouterTestBrokenConfig, ensure) {
  // create a keyring, just in case.
  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, conf_dir_.name());

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(GetParam().sections, "\n"),
      &default_section)};
  auto &router{launch_router({"-c", conf_file}, EXIT_FAILURE)};

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_THAT(router.get_full_logfile(),
              ::testing::HasSubstr(GetParam().expected_logfile_substring));
  EXPECT_THAT(router.get_full_output(),
              ::testing::HasSubstr(GetParam().expected_stderr_substring));
}

static const BrokenConfigParams broken_config_params[]{
    {"routing_connect_timeout_is_zero",
     {
         ConfigBuilder::build_section("routing",
                                      {
                                          {"bind_address", "127.0.0.1:7001"},
                                          {"destinations", "127.0.0.1:3306"},
                                          {"mode", "read-only"},
                                          {"connect_timeout", "0"},
                                      }),
     },
     "Configuration error: option connect_timeout in [routing] "
     "needs value between 1 and 65535 inclusive, was '0'",
     ""},

    {"routing_connect_timeout_is_negative",
     {
         ConfigBuilder::build_section("routing",
                                      {
                                          {"bind_address", "127.0.0.1:7001"},
                                          {"destinations", "127.0.0.1:3306"},
                                          {"mode", "read-only"},
                                          {"connect_timeout", "-1"},
                                      }),
     },
     "Configuration error: option connect_timeout in [routing] "
     "needs value between 1 and 65535 inclusive, was '-1'",
     ""},

    {"routing_client_connect_timeout_is_one",
     {
         ConfigBuilder::build_section("routing",
                                      {
                                          {"bind_address", "127.0.0.1:7001"},
                                          {"destinations", "127.0.0.1:3306"},
                                          {"mode", "read-only"},
                                          {"client_connect_timeout", "1"},
                                      }),
     },
     "Configuration error: option client_connect_timeout in [routing] "
     "needs value between 2 and 31536000 inclusive, was '1'",
     ""},

    {"routing_max_connect_error_is_zero",
     {
         ConfigBuilder::build_section("routing",
                                      {
                                          {"bind_address", "127.0.0.1:7001"},
                                          {"destinations", "127.0.0.1:3306"},
                                          {"mode", "read-only"},
                                          {"max_connect_errors", "0"},
                                      }),
     },
     "Configuration error: option max_connect_errors in [routing] "
     "needs value between 1 and 4294967295 inclusive, was '0'",
     ""},

    {"routing_protocol_is_invalid",
     {
         ConfigBuilder::build_section("routing",
                                      {
                                          {"bind_address", "127.0.0.1:7001"},
                                          {"destinations", "127.0.0.1:3306"},
                                          {"mode", "read-only"},
                                          {"protocol", "invalid"},
                                      }),
     },
     "Configuration error: Invalid protocol name: 'invalid'",
     ""},

    {"routing_protocol_is_empty",
     {
         ConfigBuilder::build_section("routing",
                                      {
                                          {"bind_address", "127.0.0.1:7001"},
                                          {"destinations", "127.0.0.1:3306"},
                                          {"mode", "read-only"},
                                          {"protocol", ""},
                                      }),
     },
     "Configuration error: Invalid protocol name: ''",
     ""},

    {"routing_client_connect_timeout_is_too_large",
     {
         ConfigBuilder::build_section(
             "routing",
             {
                 {"bind_address", "127.0.0.1:7001"},
                 {"destinations", "127.0.0.1:3306"},
                 {"mode", "read-only"},
                 {"client_connect_timeout", "31536001"},
             }),
     },
     "Configuration error: option client_connect_timeout in [routing] "
     "needs value between 2 and 31536000 inclusive, was '31536001'",
     ""},

    {"metadata_cache_invalid_bind_address",
     {
         ConfigBuilder::build_section(
             "metadata_cache",
             {
                 {"bootstrap_server_addresses",
                  "mysql://127.0.0.1:13000,mysql://127.0.0.1:99999"},
             }),
     },
     "option bootstrap_server_addresses in [metadata_cache] is incorrect "
     "(invalid URI: invalid port: impossible port number",
     ""},
    {"metadata_cache_no_bootstrap_server_addresses",
     {
         ConfigBuilder::build_section("metadata_cache",
                                      {
                                          {"user", "foobar"},
                                      }),
     },
     "list of metadata-servers is empty: 'bootstrap_server_addresses' is the "
     "configuration file is empty or not set and no known "
     "'dynamic_config'-file",
     ""},
    {"metadata_cache_empty_bootstrap_server_addresses",
     {
         ConfigBuilder::build_section("metadata_cache",
                                      {
                                          {"user", "foobar"},
                                          {"bootstrap_server_address", ""},
                                      }),
     },
     "list of metadata-servers is empty: 'bootstrap_server_addresses' is the "
     "configuration file is empty or not set and no known "
     "'dynamic_config'-file",
     ""},

    {"metadata_cache_must_be_single",
     {
         ConfigBuilder::build_section("metadata_cache:one", {}),
         ConfigBuilder::build_section("metadata_cache:two", {}),
     },
     "",
     "MySQL Router currently supports only one metadata_cache instance."},

    {"metadata_cache_user_is_required",
     {
         ConfigBuilder::build_section("metadata_cache:one", {}),
     },
     "option user in [metadata_cache:one] is required",
     ""},

    {"no_plugin",
     {},
     "",
     "Error: MySQL Router not configured to load or start "
     "any plugin. Exiting."},

    {"routing_no_bind_nor_socket",
     {
         ConfigBuilder::build_section("routing:tests",
                                      {
                                          {"destinations", "127.0.0.1:3306"},
                                          {"mode", "read-only"},
                                      }),
     },
     "either bind_address or socket option needs to be supplied, or both",
     ""},

    {"routing_no_destinations",
     {
         ConfigBuilder::build_section("routing:tests",
                                      {
                                          {"bind_address", "127.0.0.1:3307"},
                                          {"mode", "read-only"},
                                      }),
     },
     "option destinations in [routing:tests] is required",
     ""},

    {"routing_bind_address_invalid_port",
     {
         ConfigBuilder::build_section("routing:tests",
                                      {
                                          {"bind_address", "127.0.0.1:99999"},
                                          {"destinations", "127.0.0.1:3306"},
                                          {"routing_strategy", "round-robin"},
                                      }),
     },
     "incorrect (invalid TCP port: impossible port number",
     ""},

    {"routing_bind_address_invalid_address",
     {
         ConfigBuilder::build_section(
             "routing:tests",
             {
                 {"bind_address", "512.512.512.512:3306"},
                 {"destinations", "127.0.0.1:3306"},
                 {"routing_strategy", "round-robin"},
             }),
     },
     "in [routing:tests]: invalid IP or name in bind_address "
     "'512.512.512.512:3306'",
     ""},

    {"routing_bind_address_is_in_destinations",
     {
         ConfigBuilder::build_section("routing:tests",
                                      {
                                          {"bind_address", "127.0.0.1:3306"},
                                          {"destinations", "127.0.0.1"},
                                          {"routing_strategy", "round-robin"},
                                      }),
     },
     "Bind Address can not be part of destination",
     ""},

    {"routing_mode_is_case_insenstive",
     {
         ConfigBuilder::build_section("routing:tests",
                                      {
                                          {"bind_address", "127.0.0.1:3307"},
                                          {"destinations", "127.0.0.1:3306"},
                                          {"routing_strategy", "round-robin"},
                                          {"mode", "Read-Only"},
                                      }),
         ConfigBuilder::build_section("routing:break", {}),
     },
     "routing:break",
     ""},

    {"routing_routing_strategy_is_case_insenstive",
     {
         ConfigBuilder::build_section("routing:tests",
                                      {
                                          {"bind_address", "127.0.0.1:3307"},
                                          {"destinations", "127.0.0.1:3306"},
                                          {"routing_strategy", "Round-Robin"},
                                      }),
         ConfigBuilder::build_section("routing:break", {}),
     },
     "routing:break",
     ""},
};

INSTANTIATE_TEST_CASE_P(
    Spec, RouterTestBrokenConfig, ::testing::ValuesIn(broken_config_params),
    [](const ::testing::TestParamInfo<BrokenConfigParams> &info) {
      return info.param.test_name;
    });

#ifndef _WIN32
static const BrokenConfigParams broken_config_params_unix[]{
    {"routing_bad_socket",
     {
         ConfigBuilder::build_section(
             "routing:tests",
             {
                 {"destinations", "127.0.0.1:3306"},
                 {"routing_strategy", "round-robin"},

                 {"socket", "/this/path/does/not/exist/socket"},
             }),
     },
     "Setting up named socket service '/this/path/does/not/exist/socket': No "
     "such file or directory",
     ""},
};

INSTANTIATE_TEST_CASE_P(
    SpecUnix, RouterTestBrokenConfig,
    ::testing::ValuesIn(broken_config_params_unix),
    [](const ::testing::TestParamInfo<BrokenConfigParams> &info) {
      return info.param.test_name;
    });
#endif

class RouterCmdlineTest : public RouterComponentTest {
 protected:
  TempDirectory conf_dir_;
};

TEST_F(RouterCmdlineTest, help_output_is_sane) {
  auto &router{launch_router(std::vector<std::string>{"--help"})};

  check_exit_code(router, EXIT_SUCCESS);

  EXPECT_THAT(router.get_full_output(),
              ::testing::StartsWith("MySQL Router  Ver "));
  EXPECT_THAT(
      router.get_full_output(),
      ::testing::HasSubstr("Oracle is a registered trademark of Oracle"));

  EXPECT_THAT(
      router.get_full_output(),
      ::testing::AllOf(::testing::HasSubstr("(-V|--version)"),
                       ::testing::HasSubstr("(-?|--help)"),
                       ::testing::HasSubstr("[-c|--config=<path>]"),
                       ::testing::HasSubstr("[-a|--extra-config=<path>]")));

  EXPECT_THAT(
      router.get_full_output(),
      ::testing::AllOf(
          ::testing::HasSubstr("  -V, --version"),
          ::testing::HasSubstr("        Display version information and exit."),
          ::testing::HasSubstr("  -?, --help"),
          ::testing::HasSubstr("        Display this help and exit."),
          ::testing::HasSubstr("  -c <path>, --config <path>"),
          ::testing::HasSubstr(
              "        Only read configuration from given file."),
          ::testing::HasSubstr("  -a <path>, --extra-config <path>"),
          ::testing::HasSubstr(
              "        Read this file after configuration files are read")));

  std::vector<std::string> help_lines;
  {
    std::istringstream ss(router.get_full_output());
    std::string line;
    while (std::getline(ss, line)) {
      help_lines.push_back(line);
    }
  }

  SCOPED_TRACE("// contains at least 2 config filenames");
  bool found{false};
  std::string indent("  ");
  std::vector<std::string> config_files;
  for (auto it = help_lines.begin(); it != help_lines.end(); ++it) {
    auto line = *it;
    if (found) {
      if (line.empty()) {
        break;
      }
      if (mysql_harness::utility::starts_with(line, indent)) {
        auto file = line.substr(indent.size(), line.size());
        config_files.push_back(file);
      }
    }
    if (mysql_harness::utility::starts_with(line, "Configuration read")) {
      it++;  // skip next line
      found = true;
    }
  }

  ASSERT_TRUE(found) << "Failed reading location configuration locations";
  ASSERT_GE(config_files.size(), 2)
      << "Failed getting at least 2 configuration file locations";
}

TEST_F(RouterCmdlineTest, one_plugin_works) {
  std::vector<std::string> sections{
      ConfigBuilder::build_section("routertestplugin_magic", {}),
  };
  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(sections, "\n"))};
  auto &router{launch_router({"-c", conf_file})};

  check_exit_code(router, EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
