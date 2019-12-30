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

#include <array>
#include <thread>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include <gmock/gmock.h>
#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "config_builder.h"
#include "dim.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/utility/string.h"  // ::join
#include "mysql_session.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"
#include "temp_dir.h"

#include "mysqlrouter/rest_client.h"

using namespace std::chrono_literals;

static const std::string http_auth_realm_name("somerealm");
static const std::string http_auth_backend_name("somebackend");

// init_keyring() creates it
static const std::string keyring_username("mysql_router1_user");

static const std::string metadata_cache_section_name("gr_shard_1");

class RestApiTestBase : public RestApiComponentTest {
 protected:
  void SetUp() override {
    RouterComponentTest::SetUp();
    default_section_ = get_DEFAULT_defaults();
    init_keyring(default_section_, conf_dir_.name());
  }

  std::string passwd_filename_;
  std::map<std::string, std::string> default_section_;
};

static const std::vector<SwaggerPath> kMetadataSwaggerPaths{
    {"/metadata/{metadataName}/config",
     "Get config of the metadata cache of a replicaset of a cluster",
     "config of metadata cache", "cache not found"},
    {"/metadata/{metadataName}/status",
     "Get status of the metadata cache of a replicaset of a cluster",
     "status of metadata cache", "cache not found"},
    {"/metadata", "Get list of the metadata cache instances",
     "list of the metadata cache instances", ""},
};

// global variables used by the test
size_t g_refresh_failed = 0, g_refresh_succeeded = 0;
std::string g_time_last_refresh_succeeded, g_time_last_refresh_failed;

#if 0
// precondition to these tests is that we can start a router agianst a metadata-cluster
// which has no nodes. But with Bug#28352482 (no empty bootstrap_server_addresses) fixed
// we can't bring the metadata into that state anymore. We just won't start.
//
// An empty dynamic_config file will also not allow to start.
//
// In case that functionally ever comes back, we'll leave this code around, but disabled.


class RestMetadataCacheApiWithoutClusterTest
    : public RestApiTestBase,
      public ::testing::WithParamInterface<RestApiTestParams> {};

TEST_P(RestMetadataCacheApiWithoutClusterTest, ensure_openapi) {
  const std::string http_hostname = "127.0.0.1";
  const std::string userfile = create_password_file();
  const std::string http_uri = GetParam().uri;

  auto config_sections = get_restapi_config("rest_routing", userfile,
                                            GetParam().request_authentication);
  config_sections.push_back(ConfigBuilder::build_section("rest_api", {}));
  config_sections.push_back(ConfigBuilder::build_section(
      "rest_metadata_cache", {
                                 {"require_realm", http_auth_realm_name},
                             }));
  config_sections.push_back(ConfigBuilder::build_section(
      "metadata_cache:" + metadata_cache_section_name,
      {
          {"user", keyring_username},
          {"ttl", "0.2"},
      }));

  std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"),
      &default_section_)};
  ProcessWrapper &http_server{launch_router({"-c", conf_file})};

  g_refresh_failed = 0;
  g_time_last_refresh_failed = "";

  fetch_and_validate_schema_and_resource(GetParam(), http_server);

  // this part is relevant only for Get OK, otherwise let's avoid useless sleep
  if (GetParam().status_code == HttpMethod::Get &&
      GetParam().methods == HttpStatusCode::Ok) {
    // sleep a while to make the counters and timestamps change
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // check the resources again, we want to compare them against the previous
    // ones
    fetch_and_validate_schema_and_resource(GetParam(), http_server);
  }
}

static const RestApiTestParams rest_api_params_without_cluster[]{
    // The socpe of WL#12441 was limited and does not include those
    //    {"cluster_list_no_cluster",
    //     std::string(rest_api_basepath) + "/clusters/",
    //     "/clusters",
    //     HttpMethod::Get,
    //     HttpStatusCode::Ok,
    //     kContentTypeJson,
    //     kRestApiUsername,
    //     kRestApiPassword,
    //     /*request_authentication =*/true,
    //     {
    //         {"/items",
    //          [](const JsonValue *value) {
    //            ASSERT_NE(value, nullptr);
    //            ASSERT_TRUE(value->IsArray());
    //            ASSERT_EQ(value->GetArray().Size(), 0);
    //          }},
    //     }},
    //    {"cluster_nodes_no_cluster",
    //     std::string(rest_api_basepath) + "/clusters/71286871562387612/nodes",
    //     "/clusters/{clusterName}/nodes",
    //     HttpMethod::Get,
    //     HttpStatusCode::Ok,
    //     kContentTypeJson,
    //     kRestApiUsername,
    //     kRestApiPassword,
    //     /*request_authentication =*/true,
    //     {
    //         {"/items",
    //          [](const JsonValue *value) {
    //            ASSERT_NE(value, nullptr);
    //            ASSERT_TRUE(value->IsArray());
    //            ASSERT_EQ(value->GetArray().Size(), 0);
    //          }},
    //     }},
    {"metadata_list_no_cluster",
     std::string(rest_api_basepath) + "/metadata/",
     "/metadata",
     HttpMethod::Get,
     HttpStatusCode::Ok,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {
         {"/items",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsArray());
            ASSERT_EQ(value->GetArray().Size(), 1);
          }},
         {"/items/0/name",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_EQ(value->GetString(), metadata_cache_section_name);
          }},
     },
     kMetadataSwaggerPaths},
    {"metadata_status_no_cluster",
     std::string(rest_api_basepath) + "/metadata/" +
         metadata_cache_section_name + "/status",
     "/metadata/{metadataName}/status",
     HttpMethod::Get,
     HttpStatusCode::Ok,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {
         {"/refreshFailed",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());
            ASSERT_GT(value->GetInt(), 0);

            // check if it is more than last time we checked
            ASSERT_GT(value->GetInt(), g_refresh_failed);
            g_refresh_failed = static_cast<size_t>(value->GetInt());
          }},
         {"/refreshSucceeded",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());
            ASSERT_EQ(value->GetInt(), 0);
          }},
         {"/timeLastRefreshFailed",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_TRUE(pattern_found(value->GetString(), kTimestampPattern))
                << value->GetString();

            // check if it is later than last time we checked
            // timestamp format is YY-MM-DDThh:mm:ss.milisecZ (which we check
            // above) so lexical string comparison should be fine
            const std::string currentLastRefreshFailed = value->GetString();
            ASSERT_TRUE(currentLastRefreshFailed > g_time_last_refresh_failed);
            // save the current value
            g_time_last_refresh_failed = currentLastRefreshFailed;
          }},
         {"/timeLastRefreshSucceeded",
          [](const JsonValue *value) -> void { ASSERT_EQ(value, nullptr); }},
     },
     kMetadataSwaggerPaths},
    {"metadata_config_no_cluster",
     std::string(rest_api_basepath) + "/metadata/" +
         metadata_cache_section_name + "/config",
     "/metadata/{metadataName}/config",
     HttpMethod::Get,
     HttpStatusCode::Ok,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {
         {"/clusterName",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "");
          }},
         {"/groupReplicationId",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "");
          }},
         {"/timeRefreshInMs",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());
            ASSERT_EQ(value->GetInt(), 200);
          }},
     },
     kMetadataSwaggerPaths},
};

INSTANTIATE_TEST_CASE_P(
    Spec, RestMetadataCacheApiWithoutClusterTest,
    ::testing::ValuesIn(rest_api_params_without_cluster),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });
#endif

/**
 * with cluster.
 */
class RestMetadataCacheApiTest
    : public RestApiTestBase,
      public ::testing::WithParamInterface<RestApiTestParams> {
 protected:
  const uint16_t metadata_server_port_{port_pool_.get_next_available()};
};

TEST_P(RestMetadataCacheApiTest, ensure_openapi) {
  const std::string http_hostname = "127.0.0.1";
  const std::string http_uri = GetParam().uri;

  auto &md_server = ProcessManager::launch_mysql_server_mock(
      get_data_dir().join("metadata_1_node_repeat.js").str(),
      metadata_server_port_, EXIT_SUCCESS, false);

  const std::string userfile = create_password_file();

  auto config_sections = get_restapi_config("rest_routing", userfile,
                                            GetParam().request_authentication);

  config_sections.push_back(ConfigBuilder::build_section("rest_api", {}));
  config_sections.push_back(ConfigBuilder::build_section(
      "rest_metadata_cache", {
                                 {"require_realm", http_auth_realm_name},
                             }));

  config_sections.push_back(ConfigBuilder::build_section(
      "metadata_cache:" + metadata_cache_section_name,
      {
          {"user", keyring_username},
          // name of the cluster in the mock's metadata
          {"metadata_cluster", "test"},
          {"ttl", "0.2"},
          {"bootstrap_server_addresses",
           "mysql://127.0.0.1:" + std::to_string(metadata_server_port_)},
      }));

  std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"),
      &default_section_)};

  // delay the wait until we really need it.
  ASSERT_NO_FATAL_FAILURE(
      check_port_ready(md_server, metadata_server_port_, 5000ms));
  auto &router_proc{launch_router({"-c", conf_file})};

  g_refresh_succeeded = 0;
  g_time_last_refresh_failed = "";

  EXPECT_NO_FATAL_FAILURE(
      fetch_and_validate_schema_and_resource(GetParam(), router_proc))
      << router_proc.get_full_output();

  // this part is relevant only for Get OK, otherwise let's avoid useless sleep
  if (GetParam().status_code == HttpMethod::Get &&
      GetParam().methods == HttpStatusCode::Ok) {
    // sleep a while to make the counters and timestamps change
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // check the resources again, we want to compare them against the previous
    // ones
    fetch_and_validate_schema_and_resource(GetParam(), router_proc);
  }
}

// ****************************************************************************
// Request the resource(s) using supported methods with authentication enabled
// and valid credentials
// ****************************************************************************

static const RestApiTestParams rest_api_valid_methods[]{
    // The socpe of WL#12441 was limited and does not include those
    //    {"cluster_list",
    //     std::string(rest_api_basepath) + "/clusters/",
    //     "/clusters",
    //     HttpMethod::Get,
    //     HttpStatusCode::Ok,
    //     kContentTypeJson,
    //     kRestApiUsername,
    //     kRestApiPassword,
    //     /*request_authentication =*/true,
    //     {
    //         {"/items",
    //          [](const JsonValue *value) {
    //            ASSERT_NE(value, nullptr);
    //            ASSERT_TRUE(value->IsArray());
    //            ASSERT_EQ(value->GetArray().Size(), 0);
    //          }},
    //     }},
    //    {"cluster_nodes",
    //     std::string(rest_api_basepath) + "/clusters/71286871562387612/nodes",
    //     "/clusters/{clusterName}/nodes",
    //     HttpMethod::Get,
    //     HttpStatusCode::Ok,
    //     kContentTypeJson,
    //     kRestApiUsername,
    //     kRestApiPassword,
    //     /*request_authentication =*/true,
    //     {
    //         {"/items",
    //          [](const JsonValue *value) {
    //            ASSERT_NE(value, nullptr);
    //            ASSERT_TRUE(value->IsArray());
    //            ASSERT_EQ(value->GetArray().Size(), 0);
    //          }},
    //     }},
    {"metadata_list",
     std::string(rest_api_basepath) + "/metadata/",
     "/metadata",
     HttpMethod::Get,
     HttpStatusCode::Ok,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {
         {"/items",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsArray());
            ASSERT_EQ(value->GetArray().Size(), 1);
          }},
         {"/items/0/name",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_EQ(value->GetString(), metadata_cache_section_name);
          }},
     },
     kMetadataSwaggerPaths},
    {"metadata_status",
     std::string(rest_api_basepath) + "/metadata/" +
         metadata_cache_section_name + "/status",
     "/metadata/{metadataName}/status",
     HttpMethod::Get,
     HttpStatusCode::Ok,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {
         {"/refreshFailed",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());
            ASSERT_EQ(value->GetInt(), 0);
          }},
         {"/refreshSucceeded",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            // check if it is more than last time we checked
            ASSERT_GT(value->GetInt(), g_refresh_succeeded);
            g_refresh_succeeded = static_cast<size_t>(value->GetInt());
          }},
         {"/timeLastRefreshSucceeded",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_TRUE(pattern_found(value->GetString(), kTimestampPattern))
                << value->GetString();

            // check if it is later than last time we checked
            // timestamp format is YY-MM-DDThh:mm:ss.milisecZ (which we check
            // above) so lexical string comparison should be fine
            const std::string currentLastRefreshSucceeded = value->GetString();
            ASSERT_TRUE(currentLastRefreshSucceeded >
                        g_time_last_refresh_succeeded);
            // save the current value
            g_time_last_refresh_succeeded = currentLastRefreshSucceeded;
          }},
         {"/timeLastRefreshFailed",
          [](const JsonValue *value) -> void { ASSERT_EQ(value, nullptr); }},
         {"/lastRefreshHostname",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_STRNE(value->GetString(), "");
          }},
         {"/lastRefreshPort",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());
            ASSERT_GT(value->GetInt(), 0);
          }},
     },
     kMetadataSwaggerPaths},
    {"metadata_config",
     std::string(rest_api_basepath) + "/metadata/" +
         metadata_cache_section_name + "/config",
     "/metadata/{metadataName}/config",
     HttpMethod::Get,
     HttpStatusCode::Ok,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {
         {"/clusterName",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "test");
          }},
         {"/groupReplicationId",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "");
          }},
         {"/timeRefreshInMs",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsUint64());
            ASSERT_EQ(value->GetUint64(), 200);
          }},
         {"/nodes",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsArray());
            auto nodes = value->GetArray();
            ASSERT_GT(nodes.Size(), 0);
            for (unsigned i = 0; i < nodes.Size(); ++i) {
              ASSERT_TRUE(nodes[i].IsObject());
              const auto &node = nodes[i].GetObject();

              ASSERT_TRUE(node.HasMember("hostname"));
              ASSERT_TRUE(node["hostname"].IsString());
              ASSERT_STRNE(node["hostname"].GetString(), "");

              ASSERT_TRUE(node.HasMember("port"));
              ASSERT_TRUE(node["port"].IsInt());
              ASSERT_GT(node["port"].GetInt(), 0);
            }
          }},
     },
     kMetadataSwaggerPaths},
};

INSTANTIATE_TEST_CASE_P(
    ValidMethods, RestMetadataCacheApiTest,
    ::testing::ValuesIn(rest_api_valid_methods),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

// ****************************************************************************
// Request non-existing resource(s) using supported methods with authentication
// enabled and valid credentials
// ****************************************************************************

static const RestApiTestParams rest_api_non_existig_resouces[]{
    {"metadata_status_non_existing",
     std::string(rest_api_basepath) + "/metadata/NON_EXISTING/status",
     "/metadata/{metadataName}/status",
     HttpMethod::Get,
     HttpStatusCode::NotFound,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kMetadataSwaggerPaths},
    {"metadata_config_non_existing",
     std::string(rest_api_basepath) + "/metadata/NON_EXISTING/config",
     "/metadata/{metadataName}/config",
     HttpMethod::Get,
     HttpStatusCode::NotFound,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kMetadataSwaggerPaths},
    {"metadata_unsupported_param",
     std::string(rest_api_basepath) + "/metadata/?limit=10",
     "/metadata",
     HttpMethod::Get,
     HttpStatusCode::BadRequest,
     kContentTypeJsonProblem,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kMetadataSwaggerPaths},
    {"metadata_status_unsupported_param",
     std::string(rest_api_basepath) + "/metadata/" +
         metadata_cache_section_name +
         "/status?refreshFailed=0&refreshSucceeded=1",
     "/metadata/{metadataName}/status",
     HttpMethod::Get,
     HttpStatusCode::BadRequest,
     kContentTypeJsonProblem,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kMetadataSwaggerPaths},
    {"metadata_config_unsupported_param",
     std::string(rest_api_basepath) + "/metadata/" +
         metadata_cache_section_name +
         "/config?refreshFailed=0&refreshSucceeded=1",
     "/metadata/{metadataName}/config",
     HttpMethod::Get,
     HttpStatusCode::BadRequest,
     kContentTypeJsonProblem,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kMetadataSwaggerPaths},
};

INSTANTIATE_TEST_CASE_P(
    NoNexistingResources, RestMetadataCacheApiTest,
    ::testing::ValuesIn(rest_api_non_existig_resouces),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

// ****************************************************************************
// Request the resource(s) using supported methods with authentication enabled
// and invalid credentials
// ****************************************************************************

static const RestApiTestParams rest_api_valid_methods_invalid_auth_params[]{
    // The socpe of WL#12441 was limited and does not include those
    //    {"cluster_list_invalid_auth",
    //     std::string(rest_api_basepath) + "/clusters/",
    //     "/clusters",
    //     HttpMethod::Get,
    //     HttpStatusCode::Unauthorized,
    //     kContentTypeHtmlCharset,
    //     kRestApiUsername,
    //     "invalid password",
    //     /*request_authentication =*/true,
    //     {}},
    //    {"cluster_nodes_invalid_auth",
    //     std::string(rest_api_basepath) + "/clusters/71286871562387612/nodes",
    //     "/clusters/{clusterName}/nodes",
    //     HttpMethod::Get,
    //     HttpStatusCode::Unauthorized,
    //     kContentTypeHtmlCharset,
    //     kRestApiUsername,
    //     "invalid password",
    //     /*request_authentication =*/true,
    //     {}},
    {"metadata_list_invalid_auth",
     std::string(rest_api_basepath) + "/metadata/",
     "/metadata",
     HttpMethod::Get,
     HttpStatusCode::Unauthorized,
     kContentTypeHtmlCharset,
     kRestApiUsername,
     "invalid password",
     /*request_authentication =*/true,
     {},
     kMetadataSwaggerPaths},
    {"metadata_status_invalid_auth",
     std::string(rest_api_basepath) + "/metadata/" +
         metadata_cache_section_name + "/status",
     "/metadata/{metadataName}/status",
     HttpMethod::Get,
     HttpStatusCode::Unauthorized,
     kContentTypeHtmlCharset,
     kRestApiUsername,
     "invalid password",
     /*request_authentication =*/true,
     {},
     kMetadataSwaggerPaths},
    {"metadata_config_invalid_auth",
     std::string(rest_api_basepath) + "/metadata/" +
         metadata_cache_section_name + "/config",
     "/metadata/{metadataName}/config",
     HttpMethod::Get,
     HttpStatusCode::Unauthorized,
     kContentTypeHtmlCharset,
     kRestApiUsername,
     "invalid password",
     /*request_authentication =*/true,
     {},
     kMetadataSwaggerPaths},
};

INSTANTIATE_TEST_CASE_P(
    ValidMethodsInvalidAuth, RestMetadataCacheApiTest,
    ::testing::ValuesIn(rest_api_valid_methods_invalid_auth_params),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

// ****************************************************************************
// Request the resource(s) using unsupported methods with authentication enabled
// and valid credentials
// ****************************************************************************

static const RestApiTestParams rest_api_invalid_methods_params[]{
    // The socpe of WL#12441 was limited and does not include those
    //    {"cluster_list_invalid_methods",
    //     std::string(rest_api_basepath) + "/clusters/",
    //     "/clusters",
    //     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch,
    //     HttpStatusCode::MethodNotAllowed,
    //     kContentTypeJsonProblem,
    //     kRestApiUsername,
    //     kRestApiPassword,
    //     /*request_authentication =*/true,
    //     {
    //         {"/status",
    //          [](const JsonValue *value) -> void {
    //            ASSERT_NE(value, nullptr);

    //            ASSERT_TRUE(value->IsInt());
    //            ASSERT_EQ(value->GetInt(), HttpStatusCode::MethodNotAllowed);
    //          }},
    //     }},

    //    {"cluster_nodes_invalid_methods",
    //     std::string(rest_api_basepath) + "/clusters/71286871562387612/nodes",
    //     "/clusters/{clusterName}/nodes",
    //     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch,
    //     HttpStatusCode::MethodNotAllowed,
    //     kContentTypeJsonProblem,
    //     kRestApiUsername,
    //     kRestApiPassword,
    //     /*request_authentication =*/true,
    //     {
    //         {"/status",
    //          [](const JsonValue *value) -> void {
    //            ASSERT_NE(value, nullptr);

    //            ASSERT_TRUE(value->IsInt());
    //            ASSERT_EQ(value->GetInt(), HttpStatusCode::MethodNotAllowed);
    //          }},
    //     }},

    {"metadata_list_invalid_methods",
     std::string(rest_api_basepath) + "/metadata/", "/metadata",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head | HttpMethod::Trace | HttpMethod::Options |
         HttpMethod::Connect,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::kProblemJsonMethodNotAllowed, kMetadataSwaggerPaths},

    {"metadata_status_invalid_methods",
     std::string(rest_api_basepath) + "/metadata/" +
         metadata_cache_section_name + "/status",
     "/metadata/{metadataName}/status",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head | HttpMethod::Trace | HttpMethod::Options |
         HttpMethod::Connect,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::kProblemJsonMethodNotAllowed, kMetadataSwaggerPaths},

    {"metadata_config_invalid_methods",
     std::string(rest_api_basepath) + "/metadata/" +
         metadata_cache_section_name + "/config",
     "/metadata/{metadataName}/config",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head | HttpMethod::Trace | HttpMethod::Options |
         HttpMethod::Connect,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::kProblemJsonMethodNotAllowed, kMetadataSwaggerPaths},
};

INSTANTIATE_TEST_CASE_P(
    InvalidMethods, RestMetadataCacheApiTest,
    ::testing::ValuesIn(rest_api_invalid_methods_params),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

// ****************************************************************************
// Configuration errors scenarios
// ****************************************************************************

/**
 * @test Try to disable authentication although a REST API endpoint/plugin
 * defines authentication as a MUST.
 *
 */
TEST_F(RestMetadataCacheApiTest, metadata_cache_api_no_auth) {
  const std::string userfile = create_password_file();
  auto config_sections = get_restapi_config("rest_metadata_cache", userfile,
                                            /*request_authentication=*/false);

  // [rest_api] is always required
  config_sections.push_back(ConfigBuilder::build_section("rest_api", {}));

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE);

  const auto wait_for_process_exit_timeout{10000ms};
  check_exit_code(router, EXIT_FAILURE, wait_for_process_exit_timeout);

  const std::string router_output = router.get_full_logfile();
  EXPECT_NE(
      router_output.find("plugin 'rest_metadata_cache' init failed: option "
                         "require_realm in [rest_metadata_cache] is required"),
      router_output.npos)
      << router_output;
}

/**
 * @test Enable authentication for the plugin in question. Reference a realm
 * that does not exist in the configuration file.
 */
TEST_F(RestMetadataCacheApiTest, invalid_realm) {
  const std::string userfile = create_password_file();
  auto config_sections =
      get_restapi_config("rest_metadata_cache", userfile,
                         /*request_authentication=*/true, "invalidrealm");

  // [rest_api] is always required
  config_sections.push_back(ConfigBuilder::build_section("rest_api", {}));

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE);

  const auto wait_for_process_exit_timeout{10000ms};
  check_exit_code(router, EXIT_FAILURE, wait_for_process_exit_timeout);

  const std::string router_output = router.get_full_logfile();
  EXPECT_NE(router_output.find(
                "Configuration error: unknown authentication "
                "realm for [rest_metadata_cache] '': invalidrealm, known "
                "realm(s): somerealm"),
            router_output.npos)
      << router_output;
}

/**
 * @test Start router with the REST routing API plugin [rest_metadata_cache],
 * [http_plugin] and  [metadata_cache] enabled but not the [rest_api] plugin.
 *
 */
TEST_F(RestMetadataCacheApiTest, metadata_cache_api_no_rest_api) {
  const std::string userfile = create_password_file();
  auto config_sections = get_restapi_config("rest_metadata_cache", userfile,
                                            /*request_authentication=*/false);

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE);

  const auto wait_for_process_exit_timeout{10000ms};
  check_exit_code(router, EXIT_FAILURE, wait_for_process_exit_timeout);

  const std::string router_output = router.get_full_output();
  EXPECT_NE(router_output.find("Plugin 'rest_metadata_cache' needs plugin "
                               "'rest_api' which is missing in the "
                               "configuration"),
            router_output.npos)
      << router_output;
}

/**
 * @test Start router with the REST routing API plugin [rest_metadata_cache],
 * [http_plugin] and [rest_api] enabled but not the [metadata_cache] plugin.
 *
 */
// TEST_F(RestMetadataCacheApiTest, metadata_cache_api_no_mdc_secion) {
//  const std::string userfile = create_password_file();
//  auto config_sections = get_restapi_config("rest_metadata_cache", userfile,
//                                            /*request_authentication=*/true);

//  // [rest_api] is always required
//  config_sections.push_back(ConfigBuilder::build_section("rest_api", {}));

//  const std::string conf_file{create_config_file(
//      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
//  auto router = launch_router({"-c", conf_file});

//  const unsigned wait_for_process_exit_timeout{10000};
//  check_exit_code(router, EXIT_FAILURE, wait_for_process_exit_timeout);

//  const std::string router_output = router.get_full_output();
//  EXPECT_NE(router_output.find("Plugin 'rest_metadata_cache' needs plugin "
//                               "'metadata_cache' which is missing in the "
//                               "configuration"),
//            router_output.npos)
//      << router_output;
//}

/**
 * @test Add [rest_metadata_cache] twice to the configuration file. Start
 * router. Expect router to fail providing an error about the duplicate section.
 *
 */
TEST_F(RestMetadataCacheApiTest, rest_metadata_cache_section_twice) {
  const std::string userfile = create_password_file();
  auto config_sections = get_restapi_config("rest_metadata_cache", userfile,
                                            /*request_authentication=*/true);

  // [rest_api] is always required
  config_sections.push_back(ConfigBuilder::build_section("rest_api", {}));

  // force [rest_metadata_cache] twice in the config
  config_sections.push_back(
      ConfigBuilder::build_section("rest_metadata_cache", {}));

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE);

  const auto wait_for_process_exit_timeout{10000ms};
  check_exit_code(router, EXIT_FAILURE, wait_for_process_exit_timeout);

  const std::string router_output = router.get_full_output();
  EXPECT_NE(
      router_output.find(
          "Configuration error: Section 'rest_metadata_cache' already exists"),
      router_output.npos)
      << router_output;
}

/**
 * @test Enable [rest_metadata_cache] using a section key such as
 * [rest_metadata_cache:A]. Start router. Expect router to fail providing an
 * error about the use of an unsupported section key.
 *
 */
TEST_F(RestMetadataCacheApiTest, rest_metadata_cache_section_has_key) {
  const std::string userfile = create_password_file();
  auto config_sections = get_restapi_config("rest_metadata_cache:A", userfile,
                                            /*request_authentication=*/true);

  // [rest_api] is always required
  config_sections.push_back(ConfigBuilder::build_section("rest_api", {}));

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router = launch_router({"-c", conf_file}, EXIT_FAILURE);

  const auto wait_for_process_exit_timeout{10000ms};
  check_exit_code(router, EXIT_FAILURE, wait_for_process_exit_timeout);

  const std::string router_output = router.get_full_logfile();
  EXPECT_NE(
      router_output.find(
          "plugin 'rest_metadata_cache' init failed: [rest_metadata_cache] "
          "section does not expect a key, found 'A'"),
      router_output.npos)
      << router_output;
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
