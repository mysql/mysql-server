/*
  Copyright (c) 2017, 2026, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>

#include "config_builder.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"

using namespace std::chrono_literals;
using testing::StartsWith;

class RouterConfigTest : public RouterComponentTest {
 protected:
  auto &launch_router(const std::vector<std::string> &params,
                      int expected_exit_code,
                      std::chrono::milliseconds wait_ready = -1s) {
    return ProcessManager::launch_router(params, expected_exit_code, true,
                                         false, wait_ready);
  }
};

// Bug #25800863 WRONG ERRORMSG IF DIRECTORY IS PROVIDED AS CONFIGFILE
TEST_F(RouterConfigTest, RoutingDirAsMainConfigDirectory) {
  TempDirectory config_dir;

  // launch the router giving directory instead of config_name
  auto &router = launch_router({"-c", config_dir.name()}, EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_TRUE(router.expect_output(
      "The configuration file '" + config_dir.name() +
      "' is expected to be a readable file, but it is a directory"));
}

// Bug #25800863 WRONG ERRORMSG IF DIRECTORY IS PROVIDED AS CONFIGFILE
TEST_F(RouterConfigTest, RoutingDirAsExtendedConfigDirectory) {
  const auto router_port = port_pool_.get_next_available();
  const auto server_port = port_pool_.get_next_available();

  const std::string routing_section =
      mysql_harness::ConfigBuilder::build_section(
          "routing:basic",
          {{"bind_port", std::to_string(router_port)},
           {"routing_strategy", "round-robin"},
           {"destinations", "127.0.0.1:" + std::to_string(server_port)}});

  TempDirectory conf_dir("conf");
  TempDirectory extra_conf_dir;

  std::string conf_file = create_config_file(conf_dir.name(), routing_section);

  // launch the router giving directory instead of an extra config name
  auto &router = launch_router({"-c", conf_file, "-a", extra_conf_dir.name()},
                               EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_TRUE(router.expect_output(
      "The configuration file '" + extra_conf_dir.name() +
      "' is expected to be a readable file, but it is a directory"));
}

TEST_F(RouterConfigTest,
       IsExceptionThrownWhenAddTwiceTheSameSectionWithoutKey) {
  TempDirectory conf_dir("conf");
  const std::string conf_file =
      create_config_file(conf_dir.name(), "[section1]\n[section1]\n");

  // run the router and wait for it to exit
  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE);
  check_exit_code(router, EXIT_FAILURE);

  EXPECT_THAT(
      router.get_full_output(),
      StartsWith(
          "Error: Configuration error: Section 'section1' already exists"));
}

TEST_F(RouterConfigTest, IsExceptionThrownWhenAddTwiceTheSameSectionWithKey) {
  TempDirectory conf_dir("conf");
  const std::string conf_file =
      create_config_file(conf_dir.name(), "[section1:key1]\n[section1:key1]\n");

  // run the router and wait for it to exit
  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE);
  check_exit_code(router, EXIT_FAILURE);

  EXPECT_THAT(router.get_full_output(),
              StartsWith("Error: Configuration error: Section 'section1:key1' "
                         "already exists"));
}

TEST_F(RouterConfigTest,
       IsExceptionThrownWhenTheSameOptionsTwiceInASingleSection) {
  TempDirectory conf_dir("conf");
  const std::string conf_file = create_config_file(
      conf_dir.name(), "[section1]\ndynamic_state=a\ndynamic_state=b\n");

  // run the router and wait for it to exit
  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE);
  check_exit_code(router, EXIT_FAILURE);

  EXPECT_THAT(router.get_full_output(),
              StartsWith("Error: Configuration error: Option 'dynamic_state' "
                         "already defined."));
}

#ifdef _WIN32
static bool isRouterServiceInstalled(const std::string &service_name) {
  SC_HANDLE service, scm;
  bool result = false;

  if ((scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE))) {
    if ((service =
             OpenService(scm, service_name.c_str(), SERVICE_QUERY_STATUS))) {
      CloseServiceHandle(service);
      result = true;
    }
    CloseServiceHandle(scm);
  }
  return result;
}

class RouterConfigServiceTest
    : public RouterConfigTest,
      public ::testing::WithParamInterface<std::string> {};

/**
 * ensure that the router exits with proper error when launched with --service
 * and the service is not installed
 */
TEST_P(RouterConfigServiceTest, IsErrorReturnedWhenDefaultServiceDoesNotExist) {
  std::string service_name = GetParam();
  const std::string param =
      service_name.empty() ? "--service" : ("--service=" + service_name);
  if (service_name.empty()) service_name = "MySQLRouter";

  // first we need to make sure the service really is not installed on the
  // system that the test is running on. If it is we can't do much about it and
  // we just skip testing.
  if (!isRouterServiceInstalled(service_name)) {
    TempDirectory conf_dir("conf");
    const std::string conf_file =
        create_config_file(conf_dir.name(), "[keepalive]\ninterval = 60\n");

    // run the router and wait for it to exit
    auto &router = launch_router({"-c", conf_file, param}, EXIT_FAILURE);
    check_exit_code(router, EXIT_FAILURE);

    EXPECT_THAT(router.get_full_output(),
                StartsWith("Error: Could not find service '" + service_name +
                           "'!\n"
                           "Use --install-service or --install-service-manual "
                           "option to install the service first.\n"));
  }
}

INSTANTIATE_TEST_SUITE_P(Spec, RouterConfigServiceTest,
                         ::testing::Values("", "MySQLRouterCustomServiceName"));

#endif  // _WIN32

using config_section_t =
    std::pair<std::string, std::map<std::string, std::string>>;
using config_sections_t = std::vector<config_section_t>;

using config_option_t = std::pair<std::string, std::string>;

void add_options(config_section_t &section,
                 const std::vector<config_option_t> &options) {
  for (const auto &option : options) {
    section.second.emplace(option.first, option.second);
  }
}

config_section_t default_section(const std::vector<config_option_t> &options) {
  config_section_t result{"DEFAULT", {}};
  add_options(result, options);

  return result;
}

config_section_t keepalive_section(
    const std::vector<config_option_t> &options = {}) {
  config_section_t result{"keepalive", {{"interval", "1"}}};
  add_options(result, options);

  return result;
}

config_section_t routing_section(
    const std::string &name, const std::vector<config_option_t> &options = {}) {
  config_section_t result{"routing:" + name,
                          {{"destinations", "127.0.0.1:3060"},
                           {"routing_strategy", "first-available"},
                           {"bind_address", "127.0.0.1"},
                           // @bind_port@ is replaced by create_section()
                           {"bind_port", "@bind_port@"}}};

  add_options(result, options);

  return result;
}

class RouterConfigUnknownOptionTest : public RouterComponentTest {
 protected:
  ConfigWriter create_config(const config_sections_t &conf_sections) {
    ConfigWriter::sections_type sections;

    sections.emplace("DEFAULT", get_DEFAULT_defaults());
    for (const auto &section : conf_sections) {
      ConfigWriter::section_type out_section;

      // replace @place_holders@ in the section.
      for (const auto &kv : section.second) {
        auto val = kv.second;
        if (val == "@bind_port@") {
          val = std::to_string(port_pool_.get_next_available());
        }
        out_section.emplace(kv.first, val);
      }

      if (sections.count(section.first) == 0) {
        sections[section.first] = out_section;
      } else {
        sections[section.first].insert(out_section.begin(), out_section.end());
      }
    }

    ConfigWriter conf_writer(conf_dir.name(), std::move(sections));
    return conf_writer;
  }

  TempDirectory conf_dir{"conf"};
};

struct UnknownConfigOptionParam {
  std::string unknown_option;
  config_sections_t conf_sections;
};

class UnknownConfigOptionWarningCaseInsensitiveTest
    : public RouterConfigUnknownOptionTest,
      public ::testing::WithParamInterface<UnknownConfigOptionParam> {};

TEST_P(UnknownConfigOptionWarningCaseInsensitiveTest,
       UnknownConfigOptionWarningCaseInsensitive) {
  auto conf_writer = create_config(GetParam().conf_sections);

  auto &router =
      router_spawner()
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::READY)
          .expected_exit_code(EXIT_SUCCESS)
          .spawn({"-c", conf_writer.write()});

  EXPECT_TRUE(wait_log_contains(router,
                                "main WARNING .* option '" +
                                    GetParam().unknown_option +
                                    "' is not supported",
                                10s));
}

INSTANTIATE_TEST_SUITE_P(
    UnknownConfigOptionWarningCaseInsensitive,
    UnknownConfigOptionWarningCaseInsensitiveTest,
    ::testing::Values(
        UnknownConfigOptionParam{
            "DEFAULT.testing",
            config_sections_t{
                keepalive_section(),
                default_section({{"unknown_config_option", "Warning"},
                                 {"testing", "123"}})}},
        UnknownConfigOptionParam{
            "DEFAULT.testing",
            config_sections_t{
                keepalive_section(),
                default_section({{"unknown_config_option", "WARNING"},
                                 {"testing", "123"}})}},
        UnknownConfigOptionParam{
            "DEFAULT.testing",
            config_sections_t{
                keepalive_section(),
                default_section({{"unknown_config_option", "warning"},
                                 {"testing", "123"}})}},
        UnknownConfigOptionParam{
            "DEFAULT.unknown",
            config_sections_t{
                keepalive_section(),
                default_section({{"unknown_config_option", "warning"},
                                 {"unknown", "yes"}})}},
        UnknownConfigOptionParam{
            "keepalive.unknown",
            config_sections_t{
                keepalive_section({{"unknown", "yes"}}),
                default_section({{"unknown_config_option", "warning"}})}},
        UnknownConfigOptionParam{
            "routing.unknown",
            config_sections_t{
                routing_section("TestingCS_ro", {{"unknown", "yes"}}),
                default_section({{"unknown_config_option", "warning"}})}},
        UnknownConfigOptionParam{
            "keepalive.unknown",
            config_sections_t{keepalive_section({{"unknown", "1"}}),
                              default_section({})}}));

class UnknownConfigOptionErrorCaseInsensitiveTest
    : public RouterConfigUnknownOptionTest,
      public ::testing::WithParamInterface<UnknownConfigOptionParam> {};

TEST_P(UnknownConfigOptionErrorCaseInsensitiveTest,
       UnknownConfigOptionErrorCaseInsensitive) {
  auto conf_writer = create_config(GetParam().conf_sections);

  auto &router =
      router_spawner()
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
          .expected_exit_code(EXIT_FAILURE)
          .spawn({"-c", conf_writer.write()});

  check_exit_code(router, EXIT_FAILURE, 5s);

  EXPECT_TRUE(wait_log_contains(router,
                                "main ERROR .* option '" +
                                    GetParam().unknown_option +
                                    "' is not supported",
                                10s));
}

class UnknownConfigOptionValidConfigTest
    : public RouterConfigUnknownOptionTest {};

TEST_F(UnknownConfigOptionValidConfigTest, UnknownConfigOptionValidConfig) {
  auto conf_writer =
#ifdef _WIN32
      create_config({keepalive_section(), routing_section("test"),
                     default_section({{"event_source_name", "MySQL Router"}})});
#else
      create_config({keepalive_section(), routing_section("test")});
#endif

  auto &router =
      router_spawner()
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::READY)
          .expected_exit_code(EXIT_SUCCESS)
          .spawn({"-c", conf_writer.write()});

  router.kill();

  check_exit_code(router, EXIT_SUCCESS, 5s);

  EXPECT_THAT(
      router.get_logfile_content(),
      ::testing::Not(::testing::ContainsRegex("WARNING .* unknown .*")));
}

INSTANTIATE_TEST_SUITE_P(
    UnknownConfigOptionErrorCaseInsensitive,
    UnknownConfigOptionErrorCaseInsensitiveTest,
    ::testing::Values(
        UnknownConfigOptionParam{
            "DEFAULT.testing",
            config_sections_t{
                keepalive_section(),
                default_section({{"unknown_config_option", "Error"},
                                 {"testing", "123"}})}},
        UnknownConfigOptionParam{
            "DEFAULT.testing",
            config_sections_t{
                keepalive_section(),
                default_section({{"unknown_config_option", "ERROR"},
                                 {"testing", "123"}})}},
        UnknownConfigOptionParam{
            "DEFAULT.testing",
            config_sections_t{
                keepalive_section(),
                default_section({{"unknown_config_option", "error"},
                                 {"testing", "123"}})}},
        UnknownConfigOptionParam{
            "DEFAULT.unknown",
            config_sections_t{
                keepalive_section(),
                default_section({{"unknown_config_option", "error"},
                                 {"unknown", "yes"}})}},
        UnknownConfigOptionParam{
            "keepalive.unknown",
            config_sections_t{
                keepalive_section({{"unknown", "yes"}}),
                default_section({{"unknown_config_option", "error"}})}},
        UnknownConfigOptionParam{
            "routing.unknown",
            config_sections_t{
                routing_section("TestingCS_ro", {{"unknown", "yes"}}),
                default_section({{"unknown_config_option", "error"}})}}));

struct UnknownConfigOptionvalidValueParam {
  std::string unknown_conf_option_value;
  config_sections_t conf_sections;
};

class UnknownConfigOptionInvalidValueTest
    : public RouterConfigUnknownOptionTest,
      public ::testing::WithParamInterface<UnknownConfigOptionvalidValueParam> {
};

TEST_P(UnknownConfigOptionInvalidValueTest, UnknownConfigOptionInvalidValue) {
  auto conf_writer = create_config(GetParam().conf_sections);

  auto &router =
      router_spawner()
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
          .expected_exit_code(EXIT_FAILURE)
          .spawn({"-c", conf_writer.write()});

  check_exit_code(router, EXIT_FAILURE, 5s);
  // check that bootstrap outputs debug logs

  EXPECT_THAT(
      router.get_full_output(),
      ::testing::HasSubstr("Error: Configuration error: Invalid value for "
                           "DEFAULT.unknown_config_option: '" +
                           GetParam().unknown_conf_option_value +
                           "'. Allowed are: 'error' or 'warning'."));
}

INSTANTIATE_TEST_SUITE_P(
    UnknownConfigOptionInvalidValue, UnknownConfigOptionInvalidValueTest,
    ::testing::Values(
        UnknownConfigOptionvalidValueParam{
            "ERROR2",
            config_sections_t{
                {keepalive_section(),
                 default_section({{"unknown_config_option", "ERROR2"}})}}},
        UnknownConfigOptionvalidValueParam{
            "", config_sections_t{{keepalive_section(),
                                   default_section({{"unknown_config_option",
                                                     ""}})}}},
        UnknownConfigOptionvalidValueParam{
            "Warning 4",
            config_sections_t{
                {keepalive_section(),
                 default_section({{"unknown_config_option", "Warning 4"}})}}}));

TEST_F(RouterConfigTest, MetadataCacheBootstrapServerAddresses) {
  RecordProperty("Worklog", "15867");
  RecordProperty("RequirementId", "FR1");
  RecordProperty(
      "Description",
      "Verifies that the Router fails to start when "
      "[metadata_cache].bootstrap_server_addresses is configured and "
      "logs error stating that it is not supoprted option.");

  const std::string mdc_section = mysql_harness::ConfigBuilder::build_section(
      "metadata_cache:test",
      {{"cluster_type", "gr"},
       {"router_id", "1"},
       {"user", "mysql_router1_user"},
       {"metadata_cluster", "test"},
       {"bootstrap_server_addresses", "mysql://127.0.0.1:3060"},
       {"ttl", "0.5"}});

  TempDirectory conf_dir("conf");
  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, conf_dir.name());

  std::string conf_file =
      create_config_file(conf_dir.name(), mdc_section, &default_section);

  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_TRUE(
      wait_log_contains(router,
                        "main ERROR .* Error: option "
                        "'metadata_cache.bootstrap_server_addresses' is not "
                        "supported",
                        2s));
}

TEST_F(RouterConfigTest, RoutingModeUnsupported) {
  RecordProperty("Worklog", "15877");
  RecordProperty("RequirementId", "FR1");
  RecordProperty("Description",
                 "Verifies that the Router fails to start when "
                 "[routing].mode is configured and logs error stating that it "
                 "is not supoprted option.");

  const std::string mdc_section = mysql_harness::ConfigBuilder::build_section(
      "routing:test", {{"bind_port", "6064"},
                       {"destinations", "127.0.0.1:3060"},
                       {"routing_strategy", "round-robin"},
                       {"mode", "read-only"}});

  TempDirectory conf_dir("conf");
  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, conf_dir.name());

  std::string conf_file =
      create_config_file(conf_dir.name(), mdc_section, &default_section);

  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_TRUE(wait_log_contains(
      router, "main ERROR .* Error: option 'routing.mode' is not supported",
      2s));
}

TEST_F(RouterConfigTest, RoutingRoutingStrategyRequired) {
  RecordProperty("Worklog", "15877");
  RecordProperty("RequirementId", "FR2");
  RecordProperty("Description",
                 "Verifies that the Router fails to start when "
                 "[routing].routing_strategy is not configured and logs error "
                 "stating that it is required option.");

  const std::string mdc_section = mysql_harness::ConfigBuilder::build_section(
      "routing:test",
      {{"bind_port", "6064"}, {"destinations", "127.0.0.1:3060"}});

  TempDirectory conf_dir("conf");
  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, conf_dir.name());

  std::string conf_file =
      create_config_file(conf_dir.name(), mdc_section, &default_section);

  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_TRUE(
      wait_log_contains(router,
                        "main ERROR .* Configuration error: option "
                        "routing_strategy in \\[routing:test\\] is required",
                        2s));
}

TEST_F(RouterConfigTest,
       RoutingUnreachableDestinationRefreshIntervalUnsupported) {
  RecordProperty("Worklog", "15869");
  RecordProperty("RequirementId", "FR1");
  RecordProperty(
      "Description",
      "Verifies that the Router fails to start when "
      "[routing].unreachable_destination_refresh_interval is configured and "
      "logs error stating that it is not supported option.");

  const std::string mdc_section = mysql_harness::ConfigBuilder::build_section(
      "routing:test", {{"bind_port", "6064"},
                       {"destinations", "127.0.0.1:3060"},
                       {"routing_strategy", "round-robin"},
                       {"unreachable_destination_refresh_interval", "1"}});

  TempDirectory conf_dir("conf");
  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, conf_dir.name());

  std::string conf_file =
      create_config_file(conf_dir.name(), mdc_section, &default_section);

  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_TRUE(wait_log_contains(
      router,
      "main ERROR .* Error: option "
      "'routing.unreachable_destination_refresh_interval' is not supported",
      2s));
}

TEST_F(RouterConfigTest, RoutingOptionDisabledUnsupported) {
  const std::string mdc_section = mysql_harness::ConfigBuilder::build_section(
      "routing:test", {{"bind_port", "6064"},
                       {"destinations", "127.0.0.1:3060"},
                       {"routing_strategy", "round-robin"},
                       {"disabled", "1"}});

  TempDirectory conf_dir("conf");
  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, conf_dir.name());

  std::string conf_file =
      create_config_file(conf_dir.name(), mdc_section, &default_section);

  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_TRUE(wait_log_contains(router,
                                "main ERROR .* Error: option "
                                "'routing.disabled' is not supported",
                                2s));
}

struct MRSConfigErrorParam {
  MRSConfigErrorParam(
      std::string ptitle,
      std::vector<mysql_harness::ConfigBuilder::kv_type> poptions,
      std::string pexpected_error_pattern)
      : title(ptitle),
        options(poptions),
        expected_error_pattern(pexpected_error_pattern) {}

  std::string title;
  std::vector<mysql_harness::ConfigBuilder::kv_type> options;
  std::string expected_error_pattern;
};

auto get_test_title(const ::testing::TestParamInfo<MRSConfigErrorParam> &info) {
  return info.param.title;
}

class MRSConfigErrorTest
    : public RouterComponentTest,
      public ::testing::WithParamInterface<MRSConfigErrorParam> {};

TEST_P(MRSConfigErrorTest, Check) {
  const std::string mrs_section = mysql_harness::ConfigBuilder::build_section(
      "mysql_rest_service", GetParam().options);

  TempDirectory conf_dir("conf");
  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, conf_dir.name(),
               {{"mysql_user_mrs", "password", "secret"},
                {"mysql_user_mrs_data_access", "password", "secret2"},
                {"rest-user", "jwt_secret", "jwt-secret"}});

  const auto routing_rw_section = mysql_harness::ConfigBuilder::build_section(
      "routing:rw", {{"bind_port", "6064"},
                     {"destinations", "127.0.0.1:3060"},
                     {"routing_strategy", "round-robin"}});

  const auto routing_ro_section = mysql_harness::ConfigBuilder::build_section(
      "routing:ro", {{"bind_port", "6065"},
                     {"destinations", "127.0.0.1:3061"},
                     {"routing_strategy", "round-robin"}});

  const auto routing_rw_x_section = mysql_harness::ConfigBuilder::build_section(
      "routing:rwx", {{"bind_port", "6066"},
                      {"destinations", "127.0.0.1:3060"},
                      {"routing_strategy", "round-robin"},
                      {"protocol", "x"}});

  const auto routing_ro_x_section = mysql_harness::ConfigBuilder::build_section(
      "routing:rox", {{"bind_port", "6067"},
                      {"destinations", "127.0.0.1:3061"},
                      {"routing_strategy", "round-robin"},
                      {"protocol", "x"}});

  const auto conf_file = create_config_file(
      conf_dir.name(),
      routing_rw_section + routing_ro_section + routing_rw_x_section +
          routing_ro_x_section + mrs_section,
      &default_section);

  auto &router =
      launch_router({"-c", conf_file}, EXIT_FAILURE, true, false, -1ms);

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_TRUE(wait_log_contains(router, GetParam().expected_error_pattern, 2s));
}

INSTANTIATE_TEST_SUITE_P(
    Check, MRSConfigErrorTest,
    ::testing::Values(
        MRSConfigErrorParam(
            "mysql_user_missing", {},
            "main ERROR .* Configuration error: option mysql_user in "
            "\\[mysql_rest_service\\] is required"),
        MRSConfigErrorParam(
            "mysql_user_empty", {{"mysql_user", ""}},
            "main ERROR .* Configuration error: option "
            "mysql_user in \\[mysql_rest_service\\] needs a value"),
        MRSConfigErrorParam(
            "mysql_user_not_in_keyring",
            {{"mysql_user", "user_not_in_keying"},
             {"mysql_read_write_route", "rw"},
             {"router_id", "1"}},
            "mysql_rest_service ERROR .* MySQL Server account: "
            "'user_not_in_keying', set in configuration file must have a "
            "password stored in `MySQLRouter's` keyring."),
        MRSConfigErrorParam(
            "mysql_user_data_access_not_in_keyring",
            {{"mysql_user", "mysql_user_mrs"},
             {"mysql_user_data_access", "user_not_in_keying"},
             {"mysql_read_write_route", "rw"},
             {"router_id", "1"}},
            "main ERROR .* Could not fetch value for 'user_not_in_keying' "
            "from the keyring: (map::at.*|invalid map.*)"),
        MRSConfigErrorParam("mysql_read_write_route_missing",
                            {{"mysql_user", "mysql_user_mrs"}},
                            "main ERROR .* Configuration error: option "
                            "mysql_read_write_route in "
                            "\\[mysql_rest_service\\] is required"),
        MRSConfigErrorParam(
            "mysql_read_write_route_does_not_exist",
            {{"mysql_user", "mysql_user_mrs"},
             {"mysql_read_write_route", "unknown"},
             {"router_id", "1"}},
            "main ERROR .* Error: Route name 'unknown' specified for "
            "`mysql_read_write_route` option, doesn't exist or has unsupported "
            "protocol."),
        MRSConfigErrorParam("mysql_read_write_route_empty",
                            {{"mysql_user", "mysql_user_mrs"},
                             {"mysql_read_write_route", ""},
                             {"router_id", "1"}},
                            "main ERROR .* Configuration error: option "
                            "mysql_read_write_route in "
                            "\\[mysql_rest_service\\] needs a value"),
        MRSConfigErrorParam(
            "mysql_read_write_route_x_protocol",
            {{"mysql_user", "mysql_user_mrs"},
             {"mysql_read_write_route", "rwx"},
             {"router_id", "1"}},
            "main ERROR .* Error: Route name 'rwx' specified for "
            "`mysql_read_write_route` option, doesn't exist or has unsupported "
            "protocol."),
        MRSConfigErrorParam(
            "mysql_read_only_route_does_not_exist",
            {{"mysql_user", "mysql_user_mrs"},
             {"mysql_read_write_route", "rw"},
             {"mysql_read_only_route", "unknown"},
             {"router_id", "1"}},
            "main ERROR .* Error: Route name 'unknown' specified for "
            "`mysql_read_only_route` option, doesn't exist or has unsupported "
            "protocol."),
        MRSConfigErrorParam(
            "mysql_read_only_route_x_protocol",
            {{"mysql_user", "mysql_user_mrs"},
             {"mysql_read_write_route", "rw"},
             {"mysql_read_only_route", "rox"},
             {"router_id", "1"}},
            "main ERROR .* Error: Route name 'rox' specified for "
            "`mysql_read_only_route` option, doesn't exist or has unsupported "
            "protocol."),
        MRSConfigErrorParam(
            "router_id_missing",
            {{"mysql_user", "mysql_user_mrs"},
             {"mysql_read_write_route", "rw"}},
            "main ERROR .* Configuration error: option router_id in "
            "\\[mysql_rest_service\\] is required"),
        MRSConfigErrorParam(
            "router_id_empty",
            {{"mysql_user", "mysql_user_mrs"},
             {"mysql_read_write_route", "rw"},
             {"mysql_read_only_route", "ro"},
             {"router_id", ""}},
            "main ERROR .* Configuration error: option router_id in "
            "\\[mysql_rest_service\\] needs a value"),
        MRSConfigErrorParam(
            "router_id_nan",
            {{"mysql_user", "mysql_user_mrs"},
             {"mysql_read_write_route", "rw"},
             {"mysql_read_only_route", "ro"},
             {"router_id", "nan"}},
            "main ERROR .* Configuration error: option router_id in "
            "\\[mysql_rest_service\\] needs value between 0 and "
            "18446744073709551615 inclusive, was 'nan'"),
        MRSConfigErrorParam(
            "metadata_refresh_interval_negative",
            {{"mysql_user", "mysql_user_mrs"},
             {"mysql_read_write_route", "rw"},
             {"mysql_read_only_route", "ro"},
             {"router_id", "1"},
             {"metadata_refresh_interval", "-1"}},
            "main ERROR .* Configuration error: option "
            "metadata_refresh_interval in \\[mysql_rest_service\\] needs value "
            "between 0 and 1\\.79769e\\+308 inclusive, was '-1'"),
        MRSConfigErrorParam("metadata_refresh_interval_0",
                            {{"mysql_user", "mysql_user_mrs"},
                             {"mysql_read_write_route", "rw"},
                             {"mysql_read_only_route", "ro"},
                             {"router_id", "1"},
                             {"metadata_refresh_interval", "0"}},
                            "main ERROR .* Error: `metadata_refresh_interval` "
                            "option, must be greater than zero."),
        MRSConfigErrorParam(
            "metadata_refresh_interval_nan",
            {{"mysql_user", "mysql_user_mrs"},
             {"mysql_read_write_route", "rw"},
             {"mysql_read_only_route", "ro"},
             {"router_id", "1"},
             {"metadata_refresh_interval", "nan"}},
            "Configuration error: option metadata_refresh_interval in "
            "\\[mysql_rest_service\\] needs value between 0 and "
            "1\\.79769e\\+308 inclusive, was 'nan'"),
        MRSConfigErrorParam(
            "metadata_refresh_interval_foo",
            {{"mysql_user", "mysql_user_mrs"},
             {"mysql_read_write_route", "rw"},
             {"mysql_read_only_route", "ro"},
             {"router_id", "1"},
             {"metadata_refresh_interval", "foo"}},
            "Configuration error: option metadata_refresh_interval in "
            "\\[mysql_rest_service\\] needs value between 0 and "
            "1\\.79769e\\+308 inclusive, was 'foo'"),
        MRSConfigErrorParam("unknown_option",
                            {{"mysql_user", "mysql_user_mrs"},
                             {"mysql_read_write_route", "rw"},
                             {"mysql_read_only_route", "ro"},
                             {"router_id", "1"},
                             {"unknown", "1"}},
                            "main ERROR .* Error: option "
                            "'mysql_rest_service.unknown' is not supported")),
    get_test_title);

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
