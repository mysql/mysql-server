/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include <thread>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "config_builder.h"
#include "dim.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/utility/string.h"  // ::join
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"
#include "test/temp_directory.h"

using namespace std::chrono_literals;

class RestOpenApiTest
    : public RestApiComponentTest,
      public ::testing::WithParamInterface<RestApiTestParams> {};

/**
 * @test check /router/status
 *
 * - start router with rest_router module loaded
 * - GET /router/status
 * - check response code is 200 and output matches openapi spec
 */
TEST_P(RestOpenApiTest, ensure_openapi) {
  const std::string http_hostname = "127.0.0.1";
  const std::string http_uri = GetParam().uri + GetParam().api_path;

  const std::string userfile = create_password_file();
  auto config_sections = get_restapi_config("rest_api", userfile,
                                            GetParam().request_authentication);

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  launch_router({"-c", conf_file});

  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname, http_port_,
                         GetParam().user_name, GetParam().user_password);

  //  std::string wait_for_uri =
  //      (GetParam().status_code == HttpStatusCode::NotFound)
  //          ? std::string(rest_api_basepath) + "/swagger.json"
  //          : http_uri;

  //  SCOPED_TRACE("// wait for REST endpoint: " + http_uri);
  //  ASSERT_TRUE(wait_for_rest_endpoint_ready(
  //      wait_for_uri, http_port_, GetParam().user_name,
  //      GetParam().user_password, http_hostname));

  // walk all the bits
  for (HttpMethod::pos_type ndx = 0; ndx < HttpMethod::Pos::_LAST; ++ndx) {
    if (GetParam().methods.test(ndx)) {
      const auto method = 1 << ndx;

      SCOPED_TRACE("// requesting /swagger.json with " +
                   http_method_to_string(method));
      JsonDocument json_doc;
      ASSERT_NO_FATAL_FAILURE(request_json(rest_client, http_uri, method,
                                           GetParam().status_code, json_doc,
                                           GetParam().expected_content_type));

      for (const auto &kv : GetParam().value_checks) {
        ASSERT_NO_FATAL_FAILURE(validate_value(json_doc, kv.first, kv.second));
      }
    }
  }
}

// ****************************************************************************
// Request the resource(s) using supported methods with authentication enabled
// and valid credentials
// ****************************************************************************

static const RestApiTestParams rest_api_valid_methods[]{
    {"swagger_json_GET",
     rest_api_basepath,
     "/swagger.json",
     HttpMethod::Get,
     HttpStatusCode::Ok,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {
         {"/swagger",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);

            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "2.0");
          }},
         {"/info/title",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);

            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "MySQL Router");
          }},
         {"/info/description",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);

            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "API of MySQL Router");
          }},
         {"/info/version",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);

            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), kRestAPIVersion);
          }},
         {"/basePath",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);

            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(),
                         (std::string("/api/") + kRestAPIVersion).c_str());
          }},
         {"/tags",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);

            ASSERT_TRUE(value->IsArray());
          }},
         {"/paths",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);

            ASSERT_TRUE(value->IsObject());
          }},
     },
     {}},
    {"swagger_json_HEAD",
     rest_api_basepath,
     "/swagger.json",
     HttpMethod::Head,
     HttpStatusCode::Ok,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     // there is no content
     {},
     {}},
};

INSTANTIATE_TEST_SUITE_P(
    ValidMethods, RestOpenApiTest, ::testing::ValuesIn(rest_api_valid_methods),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

// ****************************************************************************
// Request the resource(s) using supported methods with authentication disabled
// and empty credentials
// ****************************************************************************

static const RestApiTestParams rest_api_valid_methods_no_auth_params[]{
    {"swagger_json_GET_no_auth",
     rest_api_basepath,
     "/swagger.json",
     HttpMethod::Get,
     HttpStatusCode::Ok,
     kContentTypeJson,
     /*username =*/"",
     /*password =*/"",
     /*request_authentication =*/false,
     {
         {"/swagger",
          [](const JsonValue *value) -> void {
            ASSERT_NE(value, nullptr);

            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "2.0");
          }},
     },
     {}},
    {"swagger_json_HEAD_no_auth",
     rest_api_basepath,
     "/swagger.json",
     HttpMethod::Head,
     HttpStatusCode::Ok,
     kContentTypeJson,
     /*username =*/"",
     /*password =*/"",
     /*request_authentication =*/false,
     // there is no content
     {},
     {}},
};

INSTANTIATE_TEST_SUITE_P(
    ValidMethodsNoAuth, RestOpenApiTest,
    ::testing::ValuesIn(rest_api_valid_methods_no_auth_params),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

// ****************************************************************************
// Request the resource(s) using supported methods with authentication enabled
// and invalid credentials
// ****************************************************************************

static const RestApiTestParams rest_api_valid_methods_invalid_auth_params[]{
    {"swagger_json_GET_invalid_auth",
     rest_api_basepath,
     "/swagger.json",
     HttpMethod::Get,
     HttpStatusCode::Unauthorized,
     kContentTypeHtmlCharset,
     kRestApiUsername,
     "invalid password",
     /*request_authentication =*/true,
     {},
     {}},
    {"swagger_json_HEAD_invalid_auth",
     rest_api_basepath,
     "/swagger.json",
     HttpMethod::Head,
     HttpStatusCode::Unauthorized,
     // there is no content
     "",
     kRestApiUsername,
     "invalid password",
     /*request_authentication =*/true,
     {},
     {}},
};

INSTANTIATE_TEST_SUITE_P(
    ValidMethodsInvalidAuth, RestOpenApiTest,
    ::testing::ValuesIn(rest_api_valid_methods_invalid_auth_params),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

// ****************************************************************************
// Request the resource(s) using unsupported methods with authentication enabled
// and valid credentials
// ****************************************************************************

static const RestApiTestParams rest_api_invalid_methods_params[]{
    {"swagger_json_invalid_methods",
     rest_api_basepath,
     "/swagger.json",
     HttpMethod::Trace | HttpMethod::Options | HttpMethod::Post |
         HttpMethod::Delete | HttpMethod::Patch,
     HttpStatusCode::MethodNotAllowed,
     kContentTypeJsonProblem,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::get_json_method_not_allowed_verifiers(),
     {}},
};

INSTANTIATE_TEST_SUITE_P(
    InvalidMethods, RestOpenApiTest,
    ::testing::ValuesIn(rest_api_invalid_methods_params),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

// ****************************************************************************
// Request the resource(s) using unsupported methods with authentication
// disabled and empty credentials
// ****************************************************************************

static const RestApiTestParams rest_api_invalid_methods_no_auth_params[]{
    {"swagger_json_invalid_methods_no_auth",
     rest_api_basepath,
     "/swagger.json",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Trace | HttpMethod::Options,
     HttpStatusCode::MethodNotAllowed,
     kContentTypeJsonProblem,
     /*username =*/"",
     /*password =*/"",
     /*request_authentication =*/false,
     RestApiComponentTest::get_json_method_not_allowed_verifiers(),
     {}},
};

INSTANTIATE_TEST_SUITE_P(
    InvalidMethodsNoAuth, RestOpenApiTest,
    ::testing::ValuesIn(rest_api_invalid_methods_no_auth_params),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

// ****************************************************************************
// Configuration errors scenarios
// ****************************************************************************

/**
 * @test Enable authentication for the plugin in question. Reference a realm
 * that does not exist in the configuration file.
 */
TEST_F(RestOpenApiTest, invalid_realm) {
  const std::string userfile = create_password_file();
  const auto config_sections = get_restapi_config(
      "rest_api", userfile, /*request_authentication=*/true, "invalidrealm");

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router = router_spawner()
                     .wait_for_sync_point(Spawner::SyncPoint::NONE)
                     .expected_exit_code(EXIT_FAILURE)
                     .spawn({"-c", conf_file});

  check_exit_code(router, EXIT_FAILURE, 10s);

  const std::string router_output = router.get_logfile_content();
  EXPECT_THAT(router_output, ::testing::HasSubstr(
                                 "Configuration error: unknown authentication "
                                 "realm for [rest_api] '': invalidrealm, known "
                                 "realm(s): somerealm"));
}

/**
 * @test Start router with the REST API plugin [rest_api] enabled twice.
 */
TEST_F(RestOpenApiTest, duplicated_rest_api_section) {
  const std::string userfile = create_password_file();
  auto config_sections =
      get_restapi_config("rest_api", userfile, /*request_authentication=*/true);

  // force [rest_api] twice in the config
  config_sections.push_back(
      mysql_harness::ConfigBuilder::build_section("rest_api", {}));

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router = router_spawner()
                     .wait_for_sync_point(Spawner::SyncPoint::NONE)
                     .expected_exit_code(EXIT_FAILURE)
                     .spawn({"-c", conf_file});

  check_exit_code(router, EXIT_FAILURE, 10s);

  const std::string router_output = router.get_full_output();
  EXPECT_THAT(
      router_output,
      ::testing::HasSubstr(
          "Error: Configuration error: Section 'rest_api' already exists."))
      << router_output;
}

/**
 * @test Start router with REST API plugin [rest_api] enabled and give a section
 * name such as [rest_api:nosectionallowed]
 */
TEST_F(RestOpenApiTest, rest_api_section_key) {
  const std::string userfile = create_password_file();
  auto config_sections = get_restapi_config(
      "rest_api:nosectionallowed", userfile, /*request_authentication=*/true);

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
      ::testing::HasSubstr(" Configuration error: [rest_api] section does "
                           "not expect a key, found 'nosectionallowed'"))
      << router_output;
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
