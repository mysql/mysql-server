/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#include "helpers/router_component_host_cache.h"

class RestConfigParam {
 public:
  std::string test_name;
  std::vector<std::string> covers;
  std::vector<std::string> configuration;
  std::map<std::string, std::string> rest_document;
};

class RestHostCacheConfigApiTestParam
    : public RestHostCacheApiTest,
      public ::testing::WithParamInterface<RestConfigParam> {
 public:
  void SetUp() override {
    RestHostCacheApiTest::SetUp();

    RecordProperty(k_property_worklog, "WL#17272");
    RecordProperty(k_property_test_plan,
                   mysql_harness::join(GetParam().covers, ","));

    ASSERT_NO_FATAL_FAILURE(
        setup_static_routing_with_rest(GetParam().configuration));
  }
};

TEST_P(RestHostCacheConfigApiTestParam,
       validate_rest_host_cache_config_endpoint) {
  SCOPED_TRACE("// check if config endpoint is exposed");

  IOContext io_ctx;
  RestClient rest_client(io_ctx, k_http_hostname, http_port_, kRestApiUsername,
                         kRestApiPassword);

  JsonDocument json_doc;
  ASSERT_NO_FATAL_FAILURE(request_json(
      rest_client, k_http_uri_host_cache_config, http::base::method::Get,
      http::base::status_code::Ok, json_doc, "application/json"));

  SCOPED_TRACE("// check if config endpoint is exposed");

  const auto &expected_configuration{GetParam().rest_document};

  ASSERT_EQ(expected_configuration.size(), json_doc.MemberCount());

  for (const auto &[k, v] : expected_configuration) {
    auto it = json_doc.FindMember(k);
    ASSERT_TRUE(it != json_doc.MemberEnd());
    ASSERT_EQ(v, rapidjson_to_string(it->value));
  }
}

const std::vector<RestConfigParam> k_rest_config_endpoint_params{
    {/* TEST-1: host_cache plugin is not configured, its loaded by default with
      * routing.
      */
     "host_cache_not_configured_but_runs_with_defauls",
     {/* Covers test-plan */ "CFG_01", "REST_01"},
     {/* no host_cache configuration */},
     {/* Expect REST endpoint to expose the configuration */
      {"enabled", "true"},
      {"maxEntries", "250"},
      {"ttlSuccessSeconds", "60"},
      {"ttlNegativeSeconds", "10"},
      {"jitterRatio", "0.2"}}},

    /* TEST-2: host_cache plugin configured with different values than defauls
     */
    {"host_cache_configured_non_defauls",
     {/* Not included in covers test-plan */},
     {/* host_cache configuration */
      mysql_harness::ConfigBuilder::build_section(
          "host_cache", {{"enabled", "false"},
                         {"max_entries", "200"},
                         {"ttl_success_seconds", "10"},
                         {"ttl_negative_seconds", "5"},
                         {"ttl_jitter_ratio", "0.1"}})},
     {/* Expect REST endpoint to expose the configuration */
      {"enabled", "false"},
      {"maxEntries", "200"},
      {"ttlSuccessSeconds", "10"},
      {"ttlNegativeSeconds", "5"},
      {"jitterRatio", "0.1"}}},
    /* TEST-3: host_cache plugin configured minimal allowed values
     */
    {"host_cache_configured_minimal_allowed_values",
     {/* Covers test-plan */ "CFG_02"},
     {/* host_cache configuration */
      mysql_harness::ConfigBuilder::build_section(
          "host_cache", {{"enabled", "false"},
                         {"max_entries", "1"},
                         {"ttl_success_seconds", "1"},
                         {"ttl_negative_seconds", "1"},
                         {"ttl_jitter_ratio", "0.0"}})},
     {/* Expect REST endpoint to expose the configuration */
      {"enabled", "false"},
      {"maxEntries", "1"},
      {"ttlSuccessSeconds", "1"},
      {"ttlNegativeSeconds", "1"},
      {"jitterRatio", "0.0"}}},

    /* TEST-4: host_cache plugin configured maximum allowed values
     */
    {"host_cache_configured_maximum_allowed_values",
     {/* Covers test-plan */ "CFG_02"},
     {/* host_cache configuration */
      mysql_harness::ConfigBuilder::build_section(
          "host_cache", {{"enabled", "true"},
                         {"max_entries", "10000"},
                         {"ttl_success_seconds", "86400"},
                         {"ttl_negative_seconds", "86400"},
                         {"ttl_jitter_ratio", "0.5"}})},
     {/* Expect REST endpoint to expose the configuration */
      {"enabled", "true"},
      {"maxEntries", "10000"},
      {"ttlSuccessSeconds", "86400"},
      {"ttlNegativeSeconds", "86400"},
      {"jitterRatio", "0.5"}}},
};

INSTANTIATE_TEST_SUITE_P(
    ValidateRestHostCacheConfig, RestHostCacheConfigApiTestParam,
    ::testing::ValuesIn(k_rest_config_endpoint_params),
    [](const ::testing::TestParamInfo<RestConfigParam> &info) {
      return info.param.test_name + "_" +
             mysql_harness::join(info.param.covers, "_");
    });

class InvalidConfigParam {
 public:
  std::string test_name;
  std::vector<std::string> covers;
  std::vector<std::string> configuration;
  std::string expected_log_entry;
};

class InvalidHostCacheConfigTestParam
    : public RestHostCacheApiTest,
      public ::testing::WithParamInterface<InvalidConfigParam> {
 public:
  void SetUp() override {
    RestHostCacheApiTest::SetUp();

    RecordProperty(k_property_worklog, "WL#17272");
    RecordProperty(k_property_test_plan,
                   mysql_harness::join(GetParam().covers, ","));
  }
};

TEST_P(InvalidHostCacheConfigTestParam,
       check_that_host_cache_doesnt_accept_invalid_config) {
  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(GetParam().configuration, "\n"))};

  SCOPED_TRACE("// start router");
  auto &router = launch_router_failure_impl({"-c", conf_file});
  ASSERT_EQ(EXIT_FAILURE, router.wait_for_exit());
  ASSERT_TRUE(process_logfile_contains(router, GetParam().expected_log_entry));
}
const std::vector<InvalidConfigParam> k_invalid_host_cache_config_params{
    {/* TEST-1: Set other type to enabled.
      */
     "host_cache_enabled_wrong_type",
     {/* Covers test-plan */ "CFG_03"},
     {/* no host_cache configuration */
      mysql_harness::ConfigBuilder::build_section("host_cache",
                                                  {{"enabled", "\"true\""}})},
     "Configuration error: option enabled in [host_cache] needs a value of "
     "either 0, 1, false or true, was '\"true\"'"},

    {/* TEST-2: Set other type to max_entries.
      */
     "host_cache_max_entries_wrong_type",
     {/* Covers test-plan */ "CFG_03"},
     {/* no host_cache configuration */
      mysql_harness::ConfigBuilder::build_section("host_cache",
                                                  {{"max_entries", "string"}})},
     "Configuration error: option max_entries in [host_cache] needs value "
     "between 1 and 10000 inclusive, was 'string'"},

    {/* TEST-3: Set under the limit to max_entries.
      */
     "host_cache_max_entries_under_min",
     {/* Covers test-plan */ "CFG_03"},
     {/* no host_cache configuration */
      mysql_harness::ConfigBuilder::build_section("host_cache",
                                                  {{"max_entries", "0"}})},
     "Configuration error: option max_entries in [host_cache] needs value "
     "between 1 and 10000 inclusive, was '0'"},

    {/* TEST-4: Set exceed the limit to max_entries.
      */
     "host_cache_max_entries_exceeds_max",
     {/* Covers test-plan */ "CFG_03"},
     {/* no host_cache configuration */
      mysql_harness::ConfigBuilder::build_section("host_cache",
                                                  {{"max_entries", "10001"}})},
     "Configuration error: option max_entries in [host_cache] needs value "
     "between 1 and 10000 inclusive, was '10001'"},

    {/* TEST-5: Set other type to ttl_success_seconds.
      */
     "host_cache_ttl_success_seconds_wrong_type",
     {/* Covers test-plan */ "CFG_03"},
     {/* no host_cache configuration */
      mysql_harness::ConfigBuilder::build_section(
          "host_cache", {{"ttl_success_seconds", "string"}})},
     "Configuration error: option ttl_success_seconds in [host_cache] needs "
     "value "
     "between 1 and 86400 inclusive, was 'string'"},

    {/* TEST-6: Set under the limit to ttl_success_seconds.
      */
     "host_cache_ttl_success_seconds_under_min",
     {/* Covers test-plan */ "CFG_03"},
     {/* no host_cache configuration */
      mysql_harness::ConfigBuilder::build_section(
          "host_cache", {{"ttl_success_seconds", "0"}})},
     "Configuration error: option ttl_success_seconds in [host_cache] needs "
     "value "
     "between 1 and 86400 inclusive, was '0'"},

    {/* TEST-7: Set exceed the limit to ttl_success_seconds.
      */
     "host_cache_ttl_success_seconds_exceeds_max",
     {/* Covers test-plan */ "CFG_03"},
     {/* no host_cache configuration */
      mysql_harness::ConfigBuilder::build_section(
          "host_cache", {{"ttl_success_seconds", "86401"}})},
     "Configuration error: option ttl_success_seconds in [host_cache] needs "
     "value "
     "between 1 and 86400 inclusive, was '86401'"},

    {/* TEST-8: Set other type to ttl_negative_seconds.
      */
     "host_cache_ttl_negative_seconds_wrong_type",
     {/* Covers test-plan */ "CFG_03"},
     {/* no host_cache configuration */
      mysql_harness::ConfigBuilder::build_section(
          "host_cache", {{"ttl_negative_seconds", "string"}})},
     "Configuration error: option ttl_negative_seconds in [host_cache] needs "
     "value "
     "between 1 and 86400 inclusive, was 'string'"},

    {/* TEST-9: Set under the limit to ttl_negative_seconds.
      */
     "host_cache_ttl_negative_seconds_under_min",
     {/* Covers test-plan */ "CFG_03"},
     {/* no host_cache configuration */
      mysql_harness::ConfigBuilder::build_section(
          "host_cache", {{"ttl_negative_seconds", "0"}})},
     "Configuration error: option ttl_negative_seconds in [host_cache] needs "
     "value "
     "between 1 and 86400 inclusive, was '0'"},

    {/* TEST-10: Set exceed the limit to ttl_negative_seconds.
      */
     "host_cache_ttl_negative_seconds_exceeds_max",
     {/* Covers test-plan */ "CFG_03"},
     {/* no host_cache configuration */
      mysql_harness::ConfigBuilder::build_section(
          "host_cache", {{"ttl_negative_seconds", "86401"}})},
     "Configuration error: option ttl_negative_seconds in [host_cache] needs "
     "value "
     "between 1 and 86400 inclusive, was '86401'"},

    {/* TEST-11: Set other type to ttl_jitter_ratio.
      */
     "host_cache_ttl_jitter_ratio_wrong_type",
     {/* Covers test-plan */ "CFG_03"},
     {/* no host_cache configuration */
      mysql_harness::ConfigBuilder::build_section(
          "host_cache", {{"ttl_jitter_ratio", "string"}})},
     "Configuration error: option ttl_jitter_ratio in [host_cache] needs "
     "value "
     "between 0 and 0.5 inclusive, was 'string'"},

    {/* TEST-12: Set under the limit to ttl_jitter_ratio.
      */
     "host_cache_ttl_jitter_ratio_under_min",
     {/* Covers test-plan */ "CFG_03"},
     {/* no host_cache configuration */
      mysql_harness::ConfigBuilder::build_section(
          "host_cache", {{"ttl_jitter_ratio", "-0.1"}})},
     "Configuration error: option ttl_jitter_ratio in [host_cache] needs "
     "value "
     "between 0 and 0.5 inclusive, was '-0.1'"},

    {/* TEST-13: Set exceed the limit to ttl_jitter_ratio.
      */
     "host_cache_ttl_jitter_ratio_exceeds_max",
     {/* Covers test-plan */ "CFG_03"},
     {/* no host_cache configuration */
      mysql_harness::ConfigBuilder::build_section(
          "host_cache", {{"ttl_jitter_ratio", "0.51"}})},
     "Configuration error: option ttl_jitter_ratio in [host_cache] needs "
     "value "
     "between 0 and 0.5 inclusive, was '0.51'"},
};

INSTANTIATE_TEST_SUITE_P(
    InvalidConfig, InvalidHostCacheConfigTestParam,
    ::testing::ValuesIn(k_invalid_host_cache_config_params),
    [](const ::testing::TestParamInfo<InvalidConfigParam> &info) {
      return info.param.test_name + "_" +
             mysql_harness::join(info.param.covers, "_");
    });

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
