/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <array>
#include <thread>

#include <gmock/gmock.h>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "config_builder.h"
#include "dim.h"
#include "mysql/harness/utility/string.h"  // ::join
#include "mysqlrouter/mysql_session.h"
#include "process_launcher.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"
#include "router_config.h"

#include "mysqlrouter/rest_client.h"

using namespace std::chrono_literals;

class RestConnectionPoolApiTest
    : public RestApiComponentTest,
      public ::testing::WithParamInterface<RestApiTestParams> {};

static const std::vector<SwaggerPath> kSwaggerPaths{
    {"/connection_pool/{connectionPoolName}/config", "Get config of a route",
     "config of a route", "route not found"},
    {"/connection_pool/{connectionPoolName}/status", "Get status of a route",
     "status of a route", "route not found"},
    {"/connection_pool", "Get list of the connection pools",
     "list of the connection pools", ""},
};

/**
 * @test check /connection_pool/main/status
 *
 * - start router with rest_connection_pool module loaded
 * - GET /connection_pool/main/status
 * - check response code is 200 and output matches openapi spec
 */
TEST_P(RestConnectionPoolApiTest, ensure_openapi) {
  const std::string http_hostname = "127.0.0.1";
  const std::string http_uri = GetParam().uri + GetParam().api_path;

  const std::string userfile = create_password_file();
  auto config_sections =
      get_restapi_config("rest_connection_pool", userfile, true);

  // add the default connection pool
  config_sections.emplace_back("[connection_pool]");

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  ProcessWrapper &http_server{launch_router({"-c", conf_file})};

  ASSERT_NO_FATAL_FAILURE(
      fetch_and_validate_schema_and_resource(GetParam(), http_server));
}

// ****************************************************************************
// Request the resource(s) using supported methods with authentication enabled
// and valid credentials
// ****************************************************************************
static const RestApiTestParams rest_api_valid_methods[]{
    {"connection_pool_status_get",
     std::string(rest_api_basepath) + "/connection_pool/main/status",
     "/connection_pool/{connectionPoolName}/status",
     HttpMethod::Get,
     HttpStatusCode::Ok,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {
         {"/idleServerConnections",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            ASSERT_GE(value->GetInt(), 0);
          }},
         {"/stashedServerConnections",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            ASSERT_GE(value->GetInt(), 0);
          }},
     },
     kSwaggerPaths},

    {"connection_pool_config_get",
     std::string(rest_api_basepath) + "/connection_pool/main/config",
     "/connection_pool/{connectionPoolName}/config",
     HttpMethod::Get,
     HttpStatusCode::Ok,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {
         {"/maxIdleServerConnections",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            ASSERT_GE(value->GetInt(), 0);
          }},
         {"/idleTimeoutInMs",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsInt());

            ASSERT_GE(value->GetInt(), 0);
          }},
     },
     kSwaggerPaths},
    {"connection_pool_list_get",
     std::string(rest_api_basepath) + "/connection_pool/",
     "/connection_pool",
     HttpMethod::Get,
     HttpStatusCode::Ok,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {
         {"/items",
          [](const JsonValue *value) {
            ASSERT_NE(value, nullptr);
            ASSERT_TRUE(value->IsArray());
            ASSERT_EQ(value->GetArray().Size(), 1);
          }},
         {"/items/0/name",
          [](const JsonValue *value) {
            ASSERT_TRUE(value != nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "main");
          }},
     },
     kSwaggerPaths},

    {"connection_pool_no_params",
     std::string(rest_api_basepath) + "/connection_pool/main/status?someparam",
     "/connection_pool/{connectionPoolName}/status",
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
    ValidMethods, RestConnectionPoolApiTest,
    ::testing::ValuesIn(rest_api_valid_methods),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

// ****************************************************************************
// Request the resource(s) using supported methods with authentication enabled
// and invalid credentials
// ****************************************************************************

static const RestApiTestParams rest_api_valid_methods_invalid_auth_params[]{
    {"connection_pool_invalid_auth",
     std::string(rest_api_basepath) + "/connection_pool/main/status",
     "/connection_pool/main/status",
     HttpMethod::Get,
     HttpStatusCode::Unauthorized,
     kContentTypeHtmlCharset,
     kRestApiUsername,
     "invalid password",
     /*request_authentication =*/true,
     {},
     kSwaggerPaths},
};

INSTANTIATE_TEST_SUITE_P(
    ValidMethodsInvalidAuth, RestConnectionPoolApiTest,
    ::testing::ValuesIn(rest_api_valid_methods_invalid_auth_params),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

// ****************************************************************************
// Request the resource(s) using unsupported methods with authentication enabled
// and valid credentials
// ****************************************************************************
static const RestApiTestParams rest_api_invalid_methods_params[]{
    {"connection_pool_status_invalid_methods",
     std::string(rest_api_basepath) + "/connection_pool/main/status",
     "/connection_pool/main/status",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head | HttpMethod::Trace | HttpMethod::Options,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::get_json_method_not_allowed_verifiers(),
     kSwaggerPaths},
};

INSTANTIATE_TEST_SUITE_P(
    InvalidMethods, RestConnectionPoolApiTest,
    ::testing::ValuesIn(rest_api_invalid_methods_params),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

// ****************************************************************************
// Configuration errors scenarios
// ****************************************************************************

/*
 * 1. Add [rest_connection_pool] twice to the configuration file.
 * 2. Start router.
 * 3. Expect router to fail providing an error about the duplicate section.
 */
TEST_F(RestConnectionPoolApiTest, section_twice) {
  const std::string userfile = create_password_file();
  auto config_sections = get_restapi_config("rest_connection_pool", userfile,
                                            /*request_authentication=*/true);

  // force [rest_connection_pool] twice in the config
  config_sections.push_back(
      mysql_harness::ConfigBuilder::build_section("rest_connection_pool", {}));

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router = router_spawner()
                     .wait_for_sync_point(Spawner::SyncPoint::NONE)
                     .expected_exit_code(EXIT_FAILURE)
                     .spawn({"-c", conf_file});

  check_exit_code(router, EXIT_FAILURE, 10s);

  const std::string router_output = router.get_full_output();
  EXPECT_NE(
      router_output.find(
          "Configuration error: Section 'rest_connection_pool' already exists"),
      router_output.npos)
      << router_output;
}

/*
 * 1. Enable [rest_connection_pool] using a section key such as
 *    [rest_connection_pool:A].
 * 2. Start router.
 * 3. Expect router to fail providing an error about the use of an unsupported
 *    section key.
 */
TEST_F(RestConnectionPoolApiTest, section_has_key) {
  const std::string userfile = create_password_file();
  auto config_sections = get_restapi_config("rest_connection_pool:A", userfile,
                                            /*request_authentication=*/true);

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router = router_spawner()
                     .wait_for_sync_point(Spawner::SyncPoint::NONE)
                     .expected_exit_code(EXIT_FAILURE)
                     .spawn({"-c", conf_file});

  check_exit_code(router, EXIT_FAILURE, 10s);

  const std::string router_output = router.get_logfile_content();
  EXPECT_THAT(
      router_output,
      ::testing::HasSubstr(
          "  init 'rest_connection_pool' failed: [rest_connection_pool] "
          "section does not expect a key, found 'A'"));
}

/**
 * @test Try to disable authentication although a REST API endpoint/plugin
 * defines authentication as a MUST.
 *
 */
TEST_F(RestConnectionPoolApiTest, no_auth) {
  const std::string userfile = create_password_file();
  auto config_sections = get_restapi_config("rest_connection_pool", userfile,
                                            /*request_authentication=*/false);

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router = router_spawner()
                     .wait_for_sync_point(Spawner::SyncPoint::NONE)
                     .expected_exit_code(EXIT_FAILURE)
                     .spawn({"-c", conf_file});

  check_exit_code(router, EXIT_FAILURE, 10s);

  const std::string router_output = router.get_logfile_content();
  EXPECT_THAT(router_output,
              ::testing::HasSubstr(
                  "  init 'rest_connection_pool' failed: option "
                  "require_realm in [rest_connection_pool] is required"))
      << router_output;
}

/**
 * @test Enable authentication for the plugin in question. Reference a realm
 * that does not exist in the configuration file.
 */
TEST_F(RestConnectionPoolApiTest, invalid_realm) {
  const std::string userfile = create_password_file();
  auto config_sections =
      get_restapi_config("rest_connection_pool", userfile,
                         /*request_authentication=*/true, "invalidrealm");

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router = router_spawner()
                     .wait_for_sync_point(Spawner::SyncPoint::NONE)
                     .expected_exit_code(EXIT_FAILURE)
                     .spawn({"-c", conf_file});

  check_exit_code(router, EXIT_FAILURE, 10s);

  const std::string router_output = router.get_logfile_content();
  EXPECT_THAT(
      router_output,
      ::testing::HasSubstr(
          "Configuration error: The option 'require_realm=invalidrealm' "
          "in [rest_connection_pool] does not match any http_auth_realm."))
      << router_output;
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
