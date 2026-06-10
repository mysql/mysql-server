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

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "helpers/router_component_host_cache.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/supported_host_cache_options.h"

const int k_no_of_routing_sections{5};

using ::testing::AllOf;
using ::testing::Ge;
using ::testing::Le;

class HostCacheResolveVerificationTest : public RestHostCacheApiTest {
 public:
  void setup_multiple_static_routes_with_fake_hosts_and_rest(
      const std::vector<std::string> &custom_sections = {},
      ProcessWrapper **out_router = nullptr) {
    SCOPED_TRACE("// setup");
    const std::string userfile = create_password_file();
    auto config_sections =
        get_restapi_config("rest_host_cache", userfile, true);

    for (int i = 1; i <= k_no_of_routing_sections; ++i) {
      routing_ports_.push_back(port_pool_.get_next_available());
      config_sections.push_back(get_static_routing_config(
          "route" + std::to_string(i), routing_ports_[i - 1],
          "hostname" + std::to_string(i), mock_server_port_));
    }
    config_sections.insert(config_sections.end(), custom_sections.begin(),
                           custom_sections.end());

    std::map<std::string, std::string> default_section_values =
        get_DEFAULT_defaults();

    // The mock_resolver plugin uses unknown variables for
    // specifying hostnames in the config.
    default_section_values["unknown_config_option"] = "warning";

    const std::string conf_file{create_config_file(
        conf_dir_.name(), mysql_harness::join(config_sections, "\n"),
        &default_section_values)};

    SCOPED_TRACE("// start mock-server");
    ASSERT_NO_FATAL_FAILURE(launch_server_impl(mock_server_port_, "my_port.js",
                                               mock_server_http_port_));
    SCOPED_TRACE("// start router");
    ASSERT_NO_FATAL_FAILURE(launch_router_impl({"-c", conf_file}, out_router));
  }

  void launch_router_impl(const std::vector<std::string> &args,
                          ProcessWrapper **out_router = nullptr) {
    auto &router = launch_router(args);

    for (auto port : routing_ports_) {
      ASSERT_NO_FATAL_FAILURE(check_port_ready(router, port));
    }
    if (out_router) *out_router = &router;
  }

  std::shared_ptr<mysqlrouter::MySQLSession> mysql_session_connect(
      uint64_t router_port, const std::string &user, const std::string &pass) {
    auto result = std::make_shared<mysqlrouter::MySQLSession>();
    result->connect("127.0.0.1", router_port, user, pass, "", "");
    return result;
  }

  void SetUp() override { RestHostCacheApiTest::SetUp(); }

  std::vector<uint16_t> routing_ports_;
};

TEST_F(HostCacheResolveVerificationTest, host_cache_miss_insert_hit) {
  RecordProperty(k_property_worklog, "WL#17272");
  RecordProperty(k_property_test_plan, "FUN_01,REST_03,REST_04");

  std::map<std::string, std::string> host_actions;

  // Configure MOCK DNS resolver (all hostnameX return 127.0.0.1)
  for (int i = 0; i < k_no_of_routing_sections; ++i) {
    host_actions["hostname" + std::to_string(i + 1)] = "log,ip4:127.0.0.1";
  }
  auto mock_resolver = mysql_harness::ConfigBuilder::build_section(
      "mock_host_resolver", {host_actions});

  ProcessWrapper *router{nullptr};
  ASSERT_NO_FATAL_FAILURE(setup_multiple_static_routes_with_fake_hosts_and_rest(
      {mock_resolver}, &router));
  ASSERT_TRUE(router != nullptr);

  SCOPED_TRACE("// Running connections to hostname1");
  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[0], "username", "password"));
  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[0], "username", "password"));

  SCOPED_TRACE("// Running REST request");
  auto json = rest_api_request_json(k_http_uri_host_cache_entries);

  ASSERT_TRUE(json.HasMember("entries"));
  auto &entries = json["entries"];

  ASSERT_EQ(1, entries.MemberCount());
  ASSERT_TRUE(entries.HasMember("hostname1"));
  auto &hostname1 = entries["hostname1"];

  ASSERT_EQ(4, hostname1.MemberCount());
  ASSERT_TRUE(hostname1.HasMember("secondsRemainingTtl"));
  ASSERT_TRUE(hostname1.HasMember("ttl"));
  ASSERT_TRUE(hostname1.HasMember("cacheHits"));
  ASSERT_TRUE(hostname1.HasMember("singleFlight"));

  const double k_top_ttl_jitter = 60.0 * 1.2;
  const double k_bottom_ttl_jitter = 60.0 * 0.8;

  ASSERT_THAT(hostname1["secondsRemainingTtl"].GetDouble(),
              AllOf(Ge(0), Le(k_top_ttl_jitter)));
  ASSERT_THAT(hostname1["ttl"].GetDouble(),
              AllOf(Ge(k_bottom_ttl_jitter), Le(k_top_ttl_jitter)));
  ASSERT_EQ(1, hostname1["cacheHits"].GetUint64());
  ASSERT_EQ(1, hostname1["singleFlight"].GetUint64());

  ASSERT_TRUE(json.HasMember("inProgress"));
  auto &in_progress = json["inProgress"];
  ASSERT_EQ(0, in_progress.MemberCount());

  // Confirm that there was one DNS request for the hostname1.
  ASSERT_EQ(1, process_logfile_count(*router,
                                     "MockResolver - mocked resolve of host "
                                     "'hostname1', returning "
                                     "no-of-addresses-1"));
}

TEST_F(HostCacheResolveVerificationTest, host_cache_disabled) {
  RecordProperty(k_property_worklog, "WL#17272");
  RecordProperty(k_property_test_plan, "CFG_04");

  std::map<std::string, std::string> host_actions;

  // Configure MOCK DNS resolver (all hostnameX return 127.0.0.1)
  for (int i = 0; i < k_no_of_routing_sections; ++i) {
    host_actions["hostname" + std::to_string(i + 1)] = "log,ip4:127.0.0.1";
  }
  auto mock_resolver_cfg = mysql_harness::ConfigBuilder::build_section(
      "mock_host_resolver", {host_actions});

  // Disable Host-cache plugin
  auto host_cache_cfg = mysql_harness::ConfigBuilder::build_section(
      "host_cache", {{"enabled", "false"}});

  ProcessWrapper *router{nullptr};
  ASSERT_NO_FATAL_FAILURE(setup_multiple_static_routes_with_fake_hosts_and_rest(
      {mock_resolver_cfg, host_cache_cfg}, &router));
  ASSERT_TRUE(router != nullptr);

  SCOPED_TRACE("// Running connections to hostname1");
  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[0], "username", "password"));
  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[0], "username", "password"));

  SCOPED_TRACE("// Running REST request");
  auto json = rest_api_request_json(k_http_uri_host_cache_entries);

  ASSERT_TRUE(json.HasMember("entries"));
  auto &entries = json["entries"];

  ASSERT_EQ(0, entries.MemberCount());
  ASSERT_TRUE(json.HasMember("inProgress"));
  auto &in_progress = json["inProgress"];
  ASSERT_EQ(0, in_progress.MemberCount());

  // Confirm that DNS requests were done for all connections to hostname1.
  ASSERT_EQ(2, process_logfile_count(*router,
                                     "MockResolver - mocked resolve of host "
                                     "'hostname1', returning "
                                     "no-of-addresses-1"));
}

TEST_F(HostCacheResolveVerificationTest, host_cache_single_flight) {
  RecordProperty(k_property_worklog, "WL#17272");
  RecordProperty(k_property_test_plan, "CON_01, CON_02");

  std::map<std::string, std::string> host_actions;

  // Configure MOCK DNS resolver (all hostnameX return 127.0.0.1)
  // except one hostname1, will block until two request are forwarded to it.
  host_actions["hostname1"] = "wait:10,log,ip4:127.0.0.1";
  for (int i = 1; i < k_no_of_routing_sections; ++i) {
    host_actions["hostname" + std::to_string(i + 1)] = "log,ip4:127.0.0.1";
  }

  auto mock_resolver_cfg = mysql_harness::ConfigBuilder::build_section(
      "mock_host_resolver", {host_actions});

  // Ensure that we have enough threads to execute different requests
  // parallel.
  auto io_cfg =
      mysql_harness::ConfigBuilder::build_section("io", {{"threads", "12"}});

  ProcessWrapper *router{nullptr};
  ASSERT_NO_FATAL_FAILURE(setup_multiple_static_routes_with_fake_hosts_and_rest(
      {mock_resolver_cfg, io_cfg}, &router));
  ASSERT_TRUE(router != nullptr);

  SCOPED_TRACE("// Running connections to hostname1");

  // The test is fragile because IO components threads
  // are sequenced in the same order on different
  // routes, http-servers etc. Thus we must offset each
  // component on different worker thread, the code below
  // does this.
  //
  // Consider disabling the test.
  for (int i = 0; i < 3; ++i) {
    rest_api_request_json(k_http_uri_host_cache_entries);
    if (2 == i) continue;
    mysql_session_connect(routing_ports_[3], "username", "password");
  }

  // Run two request for the hostname1, where the resolver is
  // configured to block 10seconds.
  bool hostname1_connection_ok1{false};
  std::thread thread_to_run_blocking_dns_request1(
      [this, &hostname1_connection_ok1]() {
        try {
          // First connection to HOSTNAME1 will block at DNS.
          mysql_session_connect(routing_ports_[0], "username", "password");
          hostname1_connection_ok1 = true;
        } catch (...) {
        }
      });

  bool hostname1_connection_ok2{false};
  std::thread thread_to_run_blocking_dns_request2(
      [this, &hostname1_connection_ok2]() {
        try {
          // First connection to HOSTNAME1 will block at DNS.
          mysql_session_connect(routing_ports_[0], "username", "password");
          hostname1_connection_ok2 = true;
        } catch (...) {
        }
      });

  ASSERT_TRUE(wait_log_contains(*router,
                                "must_wait_for other request for 'hostname1'",
                                std::chrono::milliseconds(30000)));
  JsonDocument json;

  // This request shows that there is no blocking on DNS level.
  // we can do parallel connection/resolved in case whey they are
  // executed on different threads.
  mysql_session_connect(routing_ports_[3], "username", "password");

  SCOPED_TRACE("// Running REST request");
  auto k_waiting_sleep = std::chrono::milliseconds(500);
  int repeat = 120;

  // Wait for consumers to be set to 2 /* verify JSON response */
  while (--repeat) {
    std::this_thread::sleep_for(k_waiting_sleep);
    json = rest_api_request_json(k_http_uri_host_cache_entries);

    if (!json.HasMember("inProgress")) continue;

    auto &in_progress = json["inProgress"];

    if (1 != in_progress.MemberCount()) continue;

    if (1 != in_progress.MemberCount()) continue;

    if (!in_progress.HasMember("hostname1")) continue;
    auto &hostname1 = in_progress["hostname1"];

    if (!hostname1.HasMember("consumers")) continue;

    if (2 != hostname1["consumers"].GetUint64()) continue;

    if (!hostname1.HasMember("ageMilliseconds")) continue;

    break;
  }

  ASSERT_GT(repeat, 0) << rapidjson_to_string(json);

  thread_to_run_blocking_dns_request1.join();
  thread_to_run_blocking_dns_request2.join();

  ASSERT_TRUE(hostname1_connection_ok1);
  ASSERT_TRUE(hostname1_connection_ok2);
}

TEST_F(HostCacheResolveVerificationTest, host_cache_multiple_hits) {
  RecordProperty(k_property_test_plan, "FUN_02");

  std::map<std::string, std::string> host_actions;

  // Configure MOCK DNS resolver (all hostnameX return 127.0.0.1)
  for (int i = 0; i < k_no_of_routing_sections; ++i) {
    host_actions["hostname" + std::to_string(i + 1)] = "log,ip4:127.0.0.1";
  }
  auto mock_resolver = mysql_harness::ConfigBuilder::build_section(
      "mock_host_resolver", {host_actions});

  ProcessWrapper *router{nullptr};
  ASSERT_NO_FATAL_FAILURE(setup_multiple_static_routes_with_fake_hosts_and_rest(
      {mock_resolver}, &router));
  ASSERT_TRUE(router != nullptr);

  const int k_number_of_connections = 20;

  SCOPED_TRACE("// Running connections to hostname1");
  for (int i = 0; i < k_number_of_connections; ++i) {
    ASSERT_NO_THROW(
        mysql_session_connect(routing_ports_[0], "username", "password"));
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
  }

  SCOPED_TRACE("// Running REST request");
  auto json = rest_api_request_json(k_http_uri_host_cache_entries);

  ASSERT_TRUE(json.HasMember("entries"));
  auto &entries = json["entries"];

  ASSERT_EQ(1, entries.MemberCount());
  ASSERT_TRUE(entries.HasMember("hostname1"));
  auto &hostname1 = entries["hostname1"];

  // Verify that we have same number of hits as connections done -1.
  ASSERT_TRUE(hostname1.HasMember("cacheHits"));
  ASSERT_EQ(k_number_of_connections - 1, hostname1["cacheHits"].GetUint64());

  // Confirm that there was one DNS request for the hostname1.
  ASSERT_EQ(1, process_logfile_count(*router,
                                     "MockResolver - mocked resolve of host "
                                     "'hostname1', returning "
                                     "no-of-addresses-1"));
}

TEST_F(HostCacheResolveVerificationTest,
       host_cache_different_ttls_and_caching) {
  RecordProperty(k_property_worklog, "WL#17272");
  RecordProperty(k_property_test_plan, "FUN_04,FUN_03,TTL_03");

  std::map<std::string, std::string> host_actions;

  host_actions["hostname1"] = "log,ip4:127.0.0.1";
  host_actions["hostname2"] = "log,not-found";
  host_actions["hostname3"] = "log,error";
  for (int i = 3; i < k_no_of_routing_sections; ++i) {
    host_actions["hostname" + std::to_string(i + 1)] = "log,ip4:127.0.0.1";
  }
  auto mock_resolver_cfg = mysql_harness::ConfigBuilder::build_section(
      "mock_host_resolver", {host_actions});
  // Do not randomize the TTL
  auto host_cache_cfg = mysql_harness::ConfigBuilder::build_section(
      "host_cache", {{"ttl_jitter_ratio", "0"}});

  ProcessWrapper *router{nullptr};
  ASSERT_NO_FATAL_FAILURE(setup_multiple_static_routes_with_fake_hosts_and_rest(
      {mock_resolver_cfg, host_cache_cfg}, &router));
  ASSERT_TRUE(router != nullptr);

  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[0], "username", "password"));
  ASSERT_THROW(mysql_session_connect(routing_ports_[1], "username", "password"),
               mysqlrouter::MySQLSession::Error);
  ASSERT_THROW(mysql_session_connect(routing_ports_[2], "username", "password"),
               mysqlrouter::MySQLSession::Error);

  SCOPED_TRACE("// Running REST request");
  // Confirm TTL by looking at data in REST api, this way we also
  // ensure that hostname3 was not cached.
  auto json = rest_api_request_json(k_http_uri_host_cache_entries);

  ASSERT_TRUE(json.HasMember("entries"));
  auto &entries = json["entries"];

  ASSERT_EQ(2, entries.MemberCount()) << rapidjson_to_string(json);
  ASSERT_TRUE(entries.HasMember("hostname1"));
  ASSERT_TRUE(entries.HasMember("hostname2"));
  auto &hostname1 = entries["hostname1"];
  auto &hostname2 = entries["hostname2"];

  // Verify that we have same number of hits as connections done -1.
  ASSERT_TRUE(hostname1.HasMember("ttl"));
  ASSERT_TRUE(hostname2.HasMember("ttl"));
  ASSERT_EQ(host_cache::options::kDefaultTtlSuccess,
            hostname1["ttl"].GetUint64());
  ASSERT_EQ(host_cache::options::kDefaultTtlNegative,
            hostname2["ttl"].GetUint64());
}

void verifyStatusEndpoint(const JsonDocument &doc, uint64_t entries,
                          uint64_t temp_entries, uint64_t misses,
                          uint64_t insters, uint64_t evicts, uint64_t purged) {
  ASSERT_TRUE(doc.HasMember("numberOfEntries"));
  ASSERT_TRUE(doc.HasMember("numberOfTemporaryEntries"));
  ASSERT_TRUE(doc.HasMember("cache"));
  auto &cache = doc["cache"];

  ASSERT_TRUE(cache.HasMember("hits"));
  ASSERT_TRUE(cache.HasMember("misses"));
  ASSERT_TRUE(cache.HasMember("inserts"));
  ASSERT_TRUE(cache.HasMember("evictions"));
  ASSERT_TRUE(cache.HasMember("expiredPurges"));

  ASSERT_EQ(doc["numberOfEntries"].GetUint64(), entries);
  ASSERT_EQ(doc["numberOfTemporaryEntries"].GetUint64(), temp_entries);
  ASSERT_EQ(cache["hits"].GetUint64(), 0);
  ASSERT_EQ(cache["misses"].GetUint64(), misses);
  ASSERT_EQ(cache["inserts"].GetUint64(), insters);
  ASSERT_EQ(cache["evictions"].GetUint64(), evicts);
  ASSERT_EQ(cache["expiredPurges"].GetUint64(), purged);
}

TEST_F(HostCacheResolveVerificationTest, cache_max_size_evict) {
  RecordProperty(k_property_worklog, "WL#17272");
  RecordProperty(k_property_test_plan, "LRU_01,REST_02");

  std::map<std::string, std::string> host_actions;

  // Configure MOCK DNS resolver (all hostnameX return 127.0.0.1)
  for (int i = 0; i < k_no_of_routing_sections; ++i) {
    host_actions["hostname" + std::to_string(i + 1)] = "log,ip4:127.0.0.1";
  }
  auto mock_resolver_cfg = mysql_harness::ConfigBuilder::build_section(
      "mock_host_resolver", {host_actions});
  // We need small cache size to see evict
  auto host_cache_cfg = mysql_harness::ConfigBuilder::build_section(
      "host_cache", {{"max_entries", "3"}});

  ProcessWrapper *router{nullptr};
  ASSERT_NO_FATAL_FAILURE(setup_multiple_static_routes_with_fake_hosts_and_rest(
      {mock_resolver_cfg, host_cache_cfg}, &router));
  ASSERT_TRUE(router != nullptr);

  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[0], "username", "password"));
  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[1], "username", "password"));
  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[2], "username", "password"));

  SCOPED_TRACE("// Running REST request");
  // Confirm TTL by looking at data in REST api, this way we also
  // ensure that hostname3 was not cached.
  auto json = rest_api_request_json(k_http_uri_host_cache_status);
  ASSERT_NO_FATAL_FAILURE(verifyStatusEndpoint(
      json, /*size*/ 3, 0, /*misses*/ 3, /*inserts*/ 3, 0, 0))
      << rapidjson_to_string(json);

  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[3], "username", "password"));
  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[4], "username", "password"));

  json = rest_api_request_json(k_http_uri_host_cache_status);
  ASSERT_NO_FATAL_FAILURE(verifyStatusEndpoint(
      json, /*size*/ 3, 0, /*misses*/ 5, /*inserts*/ 5, /*evictions*/ 2, 0))
      << rapidjson_to_string(json);
}

TEST_F(HostCacheResolveVerificationTest, cache_max_size_purge) {
  RecordProperty(k_property_worklog, "WL#17272");
  RecordProperty(k_property_test_plan, "LRU_02,REST_02");

  std::map<std::string, std::string> host_actions;

  // Configure MOCK DNS resolver (all hostnameX return 127.0.0.1)
  for (int i = 0; i < k_no_of_routing_sections; ++i) {
    host_actions["hostname" + std::to_string(i + 1)] = "log,ip4:127.0.0.1";
  }
  auto mock_resolver_cfg = mysql_harness::ConfigBuilder::build_section(
      "mock_host_resolver", {host_actions});
  // We need small cache size to see evict
  auto host_cache_cfg = mysql_harness::ConfigBuilder::build_section(
      "host_cache", {{"max_entries", "3"}, {"ttl_success_seconds", "3"}});

  ProcessWrapper *router{nullptr};
  ASSERT_NO_FATAL_FAILURE(setup_multiple_static_routes_with_fake_hosts_and_rest(
      {mock_resolver_cfg, host_cache_cfg}, &router));
  ASSERT_TRUE(router != nullptr);

  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[0], "username", "password"));
  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[1], "username", "password"));
  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[2], "username", "password"));

  SCOPED_TRACE("// Running REST request");
  // Confirm TTL by looking at data in REST api, this way we also
  // ensure that hostname3 was not cached.
  auto json = rest_api_request_json(k_http_uri_host_cache_status);
  ASSERT_NO_FATAL_FAILURE(verifyStatusEndpoint(
      json, /*size*/ 3, 0, /*misses*/ 3, /*inserts*/ 3, 0, 0))
      << rapidjson_to_string(json);

  // Wait until the entires will expire.
  std::this_thread::sleep_for(std::chrono::seconds(4));
  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[3], "username", "password"));
  ASSERT_NO_THROW(
      mysql_session_connect(routing_ports_[4], "username", "password"));

  json = rest_api_request_json(k_http_uri_host_cache_status);
  ASSERT_NO_FATAL_FAILURE(verifyStatusEndpoint(
      json, /*size*/ 2, 0, /*misses*/ 5, /*inserts*/ 5, /*evictions*/ 0, 3))
      << rapidjson_to_string(json);
}

class HostCacheAPITest
    : public HostCacheResolveVerificationTest,
      public ::testing::WithParamInterface<RestApiTestParams> {
 public:
};

static const std::vector<SwaggerPath> kSwaggerPaths{
    {"/host_cache/config",
     "Get config of host_cache plugin",
     "config of host_cache plugin",
     {}},
    {"/host_cache/status",
     "Get status of the host_cache plugin",
     "Get status of the host_cache plugin",
     {}},
    {"/host_cache/entries",
     "Get entries of the host_cache plugin",
     "entries of the host_cache plugin",
     {}},
};

/**
 * @test check /connection_pool/main/status
 *
 * - start router with rest_connection_pool module loaded
 * - GET /connection_pool/main/status
 * - check response code is 200 and output matches openapi spec
 */
TEST_P(HostCacheAPITest, ensure_openapi) {
  const std::string http_hostname = "127.0.0.1";
  const std::string http_uri = GetParam().uri + GetParam().api_path;

  RecordProperty(k_property_worklog, "WL#17272");

  ProcessWrapper *router{nullptr};
  ASSERT_NO_FATAL_FAILURE(
      setup_multiple_static_routes_with_fake_hosts_and_rest({}, &router));

  ASSERT_NO_FATAL_FAILURE(fetch_and_validate_schema_and_resource(
      GetParam(), *router, http_hostname));
}

// ****************************************************************************
// Request the resource(s) using supported methods with authentication enabled
// and valid credentials
// ****************************************************************************
static const RestApiTestParams rest_api_valid_methods[]{
    {"host_cache_status_get",
     std::string(rest_api_basepath) + "/host_cache/status",
     "/host_cache/status",
     HttpMethod::Get,
     HttpStatusCode::Ok,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {
         {"/numberOfEntries",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            ASSERT_GE(value->GetInt(), 0);
          }},
         {"/numberOfTemporaryEntries",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            ASSERT_GE(value->GetInt(), 0);
          }},
         {"/cache/hits",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            ASSERT_GE(value->GetInt(), 0);
          }},
         {"/cache/misses",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            ASSERT_GE(value->GetInt(), 0);
          }},
         {"/cache/inserts",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            ASSERT_GE(value->GetInt(), 0);
          }},
         {"/cache/evictions",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            ASSERT_GE(value->GetInt(), 0);
          }},
         {"/cache/expiredPurges",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            ASSERT_GE(value->GetInt(), 0);
          }},
     },
     kSwaggerPaths},

    {"host_cache_config_get",
     std::string(rest_api_basepath) + "/host_cache/config",
     "/host_cache/config",
     HttpMethod::Get,
     HttpStatusCode::Ok,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {
         {"/enabled",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsBool());

            ASSERT_GE(value->GetBool(), true);
          }},
         {"/maxEntries",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            ASSERT_GE(value->GetInt(), 250);
          }},
         {"/ttlSuccessSeconds",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            ASSERT_GE(value->GetInt(), 60);
          }},
         {"/ttlNegativeSeconds",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            ASSERT_GE(value->GetInt(), 10);
          }},
         {"/jitterRatio",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsDouble());

            ASSERT_GE(value->GetDouble(), 0.2);
          }},
     },
     kSwaggerPaths},

    {"host_cache_entries_get",
     std::string(rest_api_basepath) + "/host_cache/entries",
     "/host_cache/entries",
     HttpMethod::Get,
     HttpStatusCode::Ok,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {
         {"/entries",
          [](const JsonValue *value) {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsObject());
            ASSERT_EQ(value->GetObject().MemberCount(), 0);
          }},
         {"/inProgress",
          [](const JsonValue *value) {
            ASSERT_TRUE(value != nullptr);
            ASSERT_TRUE(value->IsObject());
            ASSERT_EQ(value->GetObject().MemberCount(), 0);
          }},
     },
     kSwaggerPaths},

    {"host_cache_no_params",
     std::string(rest_api_basepath) + "/host_cache/status?someparam",
     "/host_cache/status",
     HttpMethod::Get,
     HttpStatusCode::BadRequest,
     kContentTypeJsonProblem,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kSwaggerPaths},
};

INSTANTIATE_TEST_SUITE_P(
    ValidMethods, HostCacheAPITest, ::testing::ValuesIn(rest_api_valid_methods),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
