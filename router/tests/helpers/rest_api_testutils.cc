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

#include <array>
#include <chrono>
#include <thread>

#include <gmock/gmock.h>

#define RAPIDJSON_HAVE_STDSTRING

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/writer.h>

#include "mysqlrouter/rest_client.h"

#include "rest_api_testutils.h"

#include "config_builder.h"

static const std::chrono::milliseconds kMaxRestEndpointNotAvailableCheckTime{
    1500};
static const std::chrono::milliseconds kMaxRestEndpointNotAvailableStepTime{50};

static const std::string rest_api_openapi_json =
    std::string(rest_api_basepath) + "/swagger.json";

// wait for the endpoint to return 404
// the max_wait_time is increased 10 times for the run with VALGRIND
bool wait_endpoint_404(RestClient &rest_client, const std::string &uri,
                       std::chrono::milliseconds max_wait_time) noexcept {
  auto step_time = kMaxRestEndpointNotAvailableStepTime;
  if (getenv("WITH_VALGRIND")) {
    max_wait_time *= 10;
    step_time *= 10;
  }

  while (max_wait_time.count() > 0) {
    auto req = rest_client.request_sync(HttpMethod::Get, uri);

    if (req && req.get_response_code() != 0)
      return (req.get_response_code() == 404);

    const auto wait_time = std::min(step_time, max_wait_time);
    std::this_thread::sleep_for(wait_time);

    max_wait_time -= wait_time;
  }

  return false;
}

void fetch_json(RestClient &rest_client, const std::string &uri,
                JsonDocument &json_doc) {
  request_json(rest_client, uri, HttpMethod::Get, HttpStatusCode::Ok, json_doc);
}

void request_json(RestClient &rest_client, const std::string &uri,
                  HttpMethod::key_type http_method,
                  HttpStatusCode::key_type http_status_code,
                  JsonDocument &json_doc,
                  const std::string &expected_content_type) {
  SCOPED_TRACE("// make a http connections for " + uri);
  auto req = rest_client.request_sync(http_method, uri);

  SCOPED_TRACE("// checking HTTP response");
  ASSERT_TRUE(req) << "HTTP Request to failed (early): " << req.error_msg()
                   << std::endl;

  ASSERT_GT(req.get_response_code(), 0)
      << "HTTP Request failed: " << req.error_msg() << std::endl;

  ASSERT_EQ(req.get_response_code(), http_status_code);
  if (!expected_content_type.empty()) {
    ASSERT_THAT(req.get_input_headers().find_cstr("Content-Type"),
                ::testing::StrEq(expected_content_type));
  }

  // HEAD doesn't return a body
  if (http_method != HttpMethod::Head &&
      http_status_code != HttpStatusCode::Unauthorized &&
      http_status_code != HttpStatusCode::Forbidden) {
    auto &resp_body = req.get_input_buffer();
    ASSERT_GT(resp_body.length(), 0u);
    auto resp_body_content = resp_body.pop_front(resp_body.length());

    // parse json
    std::string json_payload(resp_body_content.begin(),
                             resp_body_content.end());

    // for methods Options, Connect and Trace libevent returns "not implemented"
    // html
    if (expected_content_type == "text/html") return;

    json_doc.Parse(json_payload.c_str());
    ASSERT_FALSE(json_doc.HasParseError())
        << rapidjson::GetParseError_En(json_doc.GetParseError()) << " at pos "
        << json_doc.GetErrorOffset() << " in document retrieved from " << uri
        << " :\n"
        << json_payload;
  }
}

JsonValue *openapi_get_or_deref(JsonDocument &json_doc,
                                const JsonPointer &pointer) {
  if (JsonValue *schm = pointer.Get(json_doc)) {
    if (auto *ref = JsonPointer("/$ref").Get(*schm)) {
      // we have a ref, follow it
      return JsonPointer(ref->GetString()).Get(json_doc);
    }
    return schm;
  }

  return nullptr;
}

void json_schema_validate(const JsonDocument &json_doc,
                          const JsonValue &schema) {
  ASSERT_TRUE(schema.IsObject());
  JsonSchemaDocument schema_doc(schema);

  JsonSchemaValidator validator(schema_doc);
  ASSERT_TRUE(json_doc.Accept(validator))
      << validator << "\n"
      << "schema: " << *validator.GetInvalidSchemaPointer().Get(schema) << "\n"
      << "document: " << json_doc << "\n";
}

void mark_object_additional_properties(JsonValue &v,
                                       JsonDocument::AllocatorType &allocator) {
  ASSERT_TRUE(v.IsObject()) << v;
  if (v.HasMember("type")) {
    ASSERT_TRUE(v["type"].IsString());

    std::string v_type = v["type"].GetString();

    if (v_type == "object") {
      if (v.HasMember("properties")) {
        ASSERT_TRUE(v["properties"].IsObject());
        for (auto &m : v["properties"].GetObject()) {
          mark_object_additional_properties(m.value, allocator);
        }
      }
      if (!v.HasMember("additionalProperties")) {
        v.AddMember("additionalProperties", false, allocator);
      }
    } else if (v_type == "array") {
      if (v.HasMember("items")) {
        mark_object_additional_properties(v["items"], allocator);
      }
    }
  }
}

std::string http_method_to_string(const HttpMethod::key_type method) {
  switch (method) {
    case HttpMethod::Get:
      return "GET";
    case HttpMethod::Post:
      return "POST";
    case HttpMethod::Head:
      return "HEAD";
    case HttpMethod::Put:
      return "PUT";
    case HttpMethod::Delete:
      return "DELETE";
    case HttpMethod::Options:
      return "OPTIONS";
    case HttpMethod::Trace:
      return "TRACE";
    case HttpMethod::Connect:
      return "CONNECT";
    case HttpMethod::Patch:
      return "PATCH";
  }

  return "UNKNOWN";
}

bool wait_for_rest_endpoint_ready(
    const std::string &uri, const uint16_t http_port,
    const std::string &username, const std::string &password,
    const std::string &http_host, std::chrono::milliseconds max_wait_time,
    std::chrono::milliseconds step_time) noexcept {
  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_host, http_port, username, password);

  if (getenv("WITH_VALGRIND")) {
    max_wait_time *= 10;
    step_time *= 10;
  }

  using clock_type = std::chrono::steady_clock;
  auto start_time = clock_type::now();
  auto end_time = start_time + max_wait_time;

  while (clock_type::now() < end_time) {
    auto step_end_time = clock_type::now() + step_time;

    auto req = rest_client.request_sync(HttpMethod::Get, uri);

    if (req && req.get_response_code() != 0 && req.get_response_code() != 404)
      return true;

    auto wait_end_time = std::min(step_end_time, end_time);
    std::this_thread::sleep_until(wait_end_time);
  }

  return false;
}

std::string RestApiComponentTest::create_password_file() {
  const std::string userfile =
      mysql_harness::Path(conf_dir_.name()).join("users").str();
  {
    ProcessWrapper::OutputResponder responder{
        [](const std::string &line) -> std::string {
          if (line == "Please enter password: ")
            return std::string(kRestApiPassword) + "\n";

          return "";
        }};

    auto &cmd =
        launch_command(get_origin().join("mysqlrouter_passwd").str(),
                       {"set", userfile, kRestApiUsername}, EXIT_SUCCESS, true,
                       std::chrono::milliseconds(-1), responder);
    check_exit_code(cmd, EXIT_SUCCESS);
  }

  return userfile;
}

std::vector<std::string> RestApiComponentTest::get_restapi_config(
    const std::string &component, const std::string &userfile,
    const bool request_authentication, const std::string &realm_name) {
  std::vector<mysql_harness::ConfigBuilder::kv_type> authentication;
  if (request_authentication) {
    authentication.push_back({"require_realm", realm_name});
  }
  const std::vector<std::string> config_sections{
      mysql_harness::ConfigBuilder::build_section(
          "http_server",
          {
              {"bind_address", "127.0.0.1"},
              {"port", std::to_string(http_port_)},
          }),
      mysql_harness::ConfigBuilder::build_section(component, authentication),
      mysql_harness::ConfigBuilder::build_section(
          "http_auth_realm:somerealm",
          {
              {"backend", "somebackend"},
              {"method", "basic"},
              {"name", "Some Realm"},
          }),
      mysql_harness::ConfigBuilder::build_section(
          "http_auth_backend:somebackend",
          {
              {"backend", "file"},
              {"filename", userfile},
          }),
  };
  return config_sections;
}

static void verify_swagger_content(
    const JsonDocument &openapi_json_doc,
    const std::vector<SwaggerPath> &expected_paths) {
  // swagger
  ASSERT_TRUE(openapi_json_doc.HasMember("swagger"));
  const auto &swagger = openapi_json_doc["swagger"];
  ASSERT_TRUE(swagger.IsString());
  ASSERT_STREQ(swagger.GetString(), "2.0");

  // info
  ASSERT_TRUE(openapi_json_doc.HasMember("info"));
  const auto &info = openapi_json_doc["info"];
  ASSERT_TRUE(info.IsObject());

  // info/title
  ASSERT_TRUE(info.HasMember("title"));
  const auto &title = info["title"];
  ASSERT_TRUE(title.IsString());
  ASSERT_STREQ(title.GetString(), "MySQL Router");

  // info/description
  ASSERT_TRUE(info.HasMember("description"));
  const auto &descr = info["description"];
  ASSERT_TRUE(descr.IsString());
  ASSERT_STREQ(descr.GetString(), "API of MySQL Router");

  // info/version
  ASSERT_TRUE(info.HasMember("version"));
  const auto &version = info["version"];
  ASSERT_TRUE(version.IsString());
  ASSERT_STREQ(version.GetString(), kRestAPIVersion);

  // paths
  ASSERT_TRUE(openapi_json_doc.HasMember("paths"));
  const auto &paths = openapi_json_doc["paths"];
  ASSERT_TRUE(paths.IsObject());
  for (const auto &expected_path : expected_paths) {
    const char *path_name = expected_path.path_name.c_str();
    ASSERT_TRUE(paths.HasMember(path_name));
    const auto &path = paths[path_name];
    ASSERT_TRUE(path.IsObject());

    // /path/get
    ASSERT_TRUE(path.HasMember("get"));
    const auto &path_get = path["get"];
    ASSERT_TRUE(path_get.IsObject());

    // /path/get/description
    ASSERT_TRUE(path_get.HasMember("description"));
    const auto &path_get_desc = path_get["description"];
    ASSERT_TRUE(path_get_desc.IsString());
    ASSERT_STREQ(path_get_desc.GetString(), expected_path.description.c_str());

    // /path/get/responses
    ASSERT_TRUE(path_get.HasMember("responses"));
    const auto &path_get_responses = path_get["responses"];
    ASSERT_TRUE(path_get_responses.IsObject());

    // /path/get/responses/200
    ASSERT_TRUE(path_get_responses.HasMember("200"));
    const auto &path_get_response_200 = path_get_responses["200"];
    ASSERT_TRUE(path_get_response_200.IsObject());
    ASSERT_TRUE(path_get_response_200.HasMember("description"));
    const auto &path_get_response_200_desc =
        path_get_response_200["description"];
    ASSERT_TRUE(path_get_response_200_desc.IsString());
    ASSERT_STREQ(path_get_response_200_desc.GetString(),
                 expected_path.response_200.c_str());

    // /path/get/responses/404
    if (expected_path.response_404.empty()) {
      ASSERT_FALSE(path_get_responses.HasMember("404"));
    } else {
      ASSERT_TRUE(path_get_responses.HasMember("404"));
      const auto &path_get_response_404 = path_get_responses["404"];
      ASSERT_TRUE(path_get_response_404.IsObject());
      ASSERT_TRUE(path_get_response_404.HasMember("description"));
      const auto &path_get_response_404_desc =
          path_get_response_404["description"];
      ASSERT_TRUE(path_get_response_404_desc.IsString());
      ASSERT_STREQ(path_get_response_404_desc.GetString(),
                   expected_path.response_404.c_str());
    }
  }
}

static std::string to_string(const JsonDocument &json_doc) {
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter writer(buffer);

  json_doc.Accept(writer);

  return buffer.GetString();
}

void RestApiComponentTest::fetch_and_validate_schema_and_resource(
    const RestApiTestParams &test_params, ProcessWrapper &http_server,
    const std::string &http_hostname) {
#define STR(s) \
  { s, strlen(s), rapidjson::kPointerInvalidIndex }

  const std::array<JsonPointer::Token, 6> schema_pointer_tokens{
      {STR("paths"), STR(test_params.api_path.c_str()), STR("get"),
       STR("responses"), STR("200"), STR("schema")}};

#undef STR

  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname, http_port_,
                         test_params.user_name, test_params.user_password);

  // if 404 is expected make sure this is what we are getting and leave
  if (test_params.status_code == HttpStatusCode::NotFound) {
    ASSERT_TRUE(wait_endpoint_404(rest_client, test_params.uri,
                                  kMaxRestEndpointNotAvailableCheckTime));
    return;
  }

  SCOPED_TRACE("// wait for REST endpoint: " + test_params.uri);
  ASSERT_TRUE(wait_for_rest_endpoint_ready(test_params.uri, http_port_,
                                           test_params.user_name,
                                           test_params.user_password))
      << http_server.get_full_output()
      << http_server.get_logfile_content("", "", 500);

  for (HttpMethod::pos_type ndx = 0; ndx < HttpMethod::Pos::_LAST; ++ndx) {
    if (test_params.methods.test(ndx)) {
      const auto method = 1 << ndx;

      SCOPED_TRACE("// fetching openapi spec");

      JsonDocument openapi_json_doc;
      {
        // if we test for authorization failure this will still return Ok as
        // accessing swagger.json does not require authorization,
        // same with InternalError, BadRequest from a path, that does not affect
        // the swagger
        HttpStatusCode::key_type expected_code =
            test_params.status_code == HttpStatusCode::Unauthorized ||
                    test_params.status_code == HttpStatusCode::BadRequest ||
                    test_params.status_code == HttpStatusCode::InternalError
                ? HttpStatusCode::Ok
                : test_params.status_code;
        std::string expected_content_type =
            test_params.status_code == HttpStatusCode::Unauthorized ||
                    test_params.status_code == HttpStatusCode::BadRequest ||
                    test_params.status_code == HttpStatusCode::InternalError
                ? kContentTypeJson
                : test_params.expected_content_type;

        // also if the method is HEAD it's not really invalid method for
        // swagger.json file, it's only invalid for the path (API call) itself
        // later
        if (method == HttpMethod::Head &&
            test_params.status_code == HttpStatusCode::MethodNotAllowed) {
          expected_code = HttpStatusCode::Ok;
          expected_content_type = kContentTypeJson;
        }

        ASSERT_NO_FATAL_FAILURE(request_json(
            rest_client, rest_api_openapi_json, method, expected_code,
            openapi_json_doc, expected_content_type));
      }

      // verify response against the schema of the openapi spec
      SCOPED_TRACE("// API call");
      JsonDocument json_doc;
      ASSERT_NO_FATAL_FAILURE(request_json(rest_client, test_params.uri, method,
                                           test_params.status_code, json_doc,
                                           test_params.expected_content_type));

      SCOPED_TRACE("// validating schema");
      if (HttpStatusCode::Ok == test_params.status_code) {
        verify_swagger_content(openapi_json_doc, test_params.swagger_paths);

        auto schema_pointer = JsonPointer(schema_pointer_tokens.data(),
                                          schema_pointer_tokens.size());
        // points to either a $ref or a schema object
        auto *schema_val =
            openapi_get_or_deref(openapi_json_doc, schema_pointer);
        ASSERT_TRUE(schema_val != nullptr);
        ASSERT_TRUE(schema_val->IsObject());

        ASSERT_NO_FATAL_FAILURE(mark_object_additional_properties(
            *schema_val, openapi_json_doc.GetAllocator()));

        ASSERT_NO_FATAL_FAILURE(json_schema_validate(json_doc, *schema_val));
      }

      SCOPED_TRACE("// validating values");
      // HEAD does not return a body
      if (method != HttpMethod::Head) {
        for (const auto &kv : test_params.value_checks) {
          ASSERT_NO_FATAL_FAILURE(validate_value(json_doc, kv.first, kv.second))
              << to_string(json_doc);
        }
      }
    }
  }
}

RestApiComponentTest::json_verifiers_t
RestApiComponentTest::get_json_method_not_allowed_verifiers() {
  static const RestApiComponentTest::json_verifiers_t result{
      {"/status",
       [](const JsonValue *value) -> void {
         ASSERT_NE(value, nullptr);

         ASSERT_TRUE(value->IsInt());
         ASSERT_EQ(value->GetInt(), HttpStatusCode::MethodNotAllowed);
       }},
      {"/title",
       [](const JsonValue *value) -> void {
         ASSERT_NE(value, nullptr);

         ASSERT_TRUE(value->IsString());
         // CONNECT returns "Method Not Allowed"
         const std::vector<std::string> msgs{"HTTP Method not allowed",
                                             "Method Not Allowed"};
         ASSERT_THAT(msgs, ::testing::Contains(value->GetString()));
       }},
      {"/detail",
       [](const JsonValue *value) -> void {
         // there is no /detail field for CONNECT
         if (value != nullptr) {
           ASSERT_TRUE(value->IsString());
           // swagger.json allows HEAD
           const std::vector<std::string> msgs{
               "only HTTP Methods GET are supported",
               "only HTTP Methods GET,HEAD are supported"};
           ASSERT_THAT(msgs, ::testing::Contains(value->GetString()));
         }
       }},
  };

  return result;
}

void RestApiComponentTest::validate_value(
    const JsonDocument &json_doc, const std::string &value_json_pointer,
    const RestApiTestParams::value_check_func value_check) {
  const auto jp = JsonPointer(value_json_pointer.c_str());
  ASSERT_TRUE(jp.IsValid()) << value_json_pointer;
  SCOPED_TRACE("// validating field: " + value_json_pointer);
  ASSERT_NO_FATAL_FAILURE(value_check(jp.Get(json_doc)));
}
