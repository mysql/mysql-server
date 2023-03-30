/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_REST_API_TESTUTILS_H_INCLUDED
#define MYSQLROUTER_REST_API_TESTUTILS_H_INCLUDED

#include <chrono>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>

#include "mysql/harness/filesystem.h"
#include "mysqlrouter/rest_client.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"
#include "test/temp_directory.h"

// AddressSanitizer gets confused by the default, MemoryPoolAllocator
// Solaris sparc also gets crashes
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
using JsonValue =
    rapidjson::GenericValue<rapidjson::UTF8<>, JsonDocument::AllocatorType>;
using JsonPointer =
    rapidjson::GenericPointer<JsonValue, JsonDocument::AllocatorType>;
using JsonSchemaDocument =
    rapidjson::GenericSchemaDocument<JsonValue, JsonDocument::AllocatorType>;
using JsonSchemaValidator =
    rapidjson::GenericSchemaValidator<JsonSchemaDocument>;

/**
 * GET a JSON document from a HTTP server.
 *
 * expects HTTP Status "200"
 *
 * use ASSERT_NO_FATAL_ERROR() to detect error
 *
 * @param rest_client a RestClient
 * @param uri URI to request
 * @param json_doc json doc to store json document in on success
 */
void fetch_json(RestClient &rest_client, const std::string &uri,
                JsonDocument &json_doc);

/**
 * request a json document from a HTTP server.
 *
 * use ASSERT_NO_FATAL_ERROR() to detect error
 *
 * @param rest_client a RestClient
 * @param uri URI to request
 * @param http_method HttpMethod to executed
 * @param http_status_code expected HTTP Status code
 * @param json_doc json doc to store json document in on success
 */
void request_json(
    RestClient &rest_client, const std::string &uri,
    HttpMethod::type http_method, HttpStatusCode::key_type http_status_code,
    JsonDocument &json_doc,
    const std::string &expected_content_type = "application/json");

/**
 * get a JsonValue from a document and follow $ref automatically.
 */
JsonValue *openapi_get_or_deref(JsonDocument &json_doc,
                                const JsonPointer &pointer);

/**
 * pretty print a JsonValue to a std::ostream.
 */
namespace rapidjson {
template <class T, class Allocator>
std::ostream &operator<<(
    std::ostream &os, const rapidjson::GenericValue<T, Allocator> &json_val) {
  rapidjson::StringBuffer sb;
  {
    rapidjson::PrettyWriter<rapidjson::StringBuffer> pretty(sb);
    json_val.Accept(pretty);
  }

  os << sb.GetString();

  return os;
}

/**
 * pretty print result of a SchemaValidator to a std::ostream.
 */
template <class T>
std::ostream &operator<<(
    std::ostream &os, const rapidjson::GenericSchemaValidator<T> &validator) {
  rapidjson::StringBuffer sb;

  validator.GetInvalidDocumentPointer().StringifyUriFragment(sb);
  os << "document-json-pointer '" << sb.GetString() << "'";
  sb.Clear();

  os << " failed requirement '" << validator.GetInvalidSchemaKeyword() << "'";

  validator.GetInvalidSchemaPointer().StringifyUriFragment(sb);
  os << " at schema-json-pointer: '" << sb.GetString() << "'";
  sb.Clear();

  return os;
}
}  // namespace rapidjson

/**
 * validate a JsonDocument against a json schema.
 *
 * check with ASSERT_NO_FATAL_FAILURE() if validation failed.
 */
void json_schema_validate(const JsonDocument &json_doc,
                          const JsonValue &schema);

/**
 * mark all properties in the schema as the only allowed ones.
 *
 * the schema returned to the client is flexible to be extensible
 * in the future, but for testing we actually want to ensure that
 * only those fields are sent that we announced in the schema to
 * catch typos.
 */
void mark_object_additional_properties(JsonValue &v,
                                       JsonDocument::AllocatorType &allocator);

std::string http_method_to_string(const HttpMethod::type method);

/**
 * wait until a REST endpoint is ready to handle requests.
 *
 * @param uri         REST endpoint URI to check
 * @param http_port   tcp port of the REST endpoint
 * @param username    username to authenticate if the endpoint requires
 * authentication
 * @param password    password to authenticate if the endpoint requires
 * authentication
 * @param http_host   host name of the REST endpoint
 * @param max_wait_time how long should the function wait to the endpoint to
 * become ready before returning false
 * @param step_time   what should be the sleep time between the consecutive
 * checks for the endpoint availability
 *
 * @returns true once endpoint is ready to handle requests, false
 *          if the timeout has expired and the endpoint did not become ready
 */
bool wait_for_rest_endpoint_ready(
    const std::string &uri, const uint16_t http_port,
    const std::string &username = "", const std::string &password = "",
    const std::string &http_host = "127.0.0.1",
    std::chrono::milliseconds max_wait_time = std::chrono::milliseconds(5000),
    std::chrono::milliseconds step_time =
        std::chrono::milliseconds(50)) noexcept;

// YYY-MM-DDThh:mm:ss.milisecZ
constexpr const char *const kTimestampPattern =
    "^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{6}Z$";

constexpr const char *const kContentTypeJson = "application/json";
constexpr const char *const kContentTypeJsonProblem =
    "application/problem+json";
constexpr const char *const kContentTypeHtml = "text/html";
constexpr const char *const kContentTypeHtmlCharset =
    "text/html; charset=ISO-8859-1";
constexpr const char *const kRestApiUsername = "someuser";
constexpr const char *const kRestApiPassword = "somepassword";

const std::string rest_api_basepath = std::string("/api/") + kRestAPIVersion;

struct SwaggerPath {
  std::string path_name;
  std::string description;
  std::string response_200;
  std::string response_404; /* leave empty if not available */
};

struct RestApiTestParams {
  const char *test_name;
  const std::string uri;
  const std::string api_path;
  const HttpMethod::Bitset methods;
  HttpStatusCode::key_type status_code;
  const std::string expected_content_type;
  const std::string user_name;
  const std::string user_password;
  const bool request_authentication;

  using value_check_func = std::function<void(const JsonValue *)>;
  const std::vector<std::pair<std::string, value_check_func>> value_checks;

  const std::vector<SwaggerPath> swagger_paths;
};

class RestApiComponentTest : public RouterComponentTest {
 public:
  std::string create_password_file();

  std::vector<std::string> get_restapi_config(
      const std::string &component, const std::string &userfile,
      const bool request_authentication,
      const std::string &realm_name = "somerealm");

  void fetch_and_validate_schema_and_resource(
      const RestApiTestParams &test_params, ProcessWrapper &http_server,
      const std::string &http_hostname = "127.0.0.1");

  void validate_value(const JsonDocument &json_doc,
                      const std::string &value_json_pointer,
                      const RestApiTestParams::value_check_func value_check);

  using json_verifiers_t =
      std::vector<std::pair<std::string, RestApiTestParams::value_check_func>>;

  static json_verifiers_t get_json_method_not_allowed_verifiers();

 protected:
  const uint16_t http_port_{port_pool_.get_next_available()};
  TempDirectory conf_dir_;
};

bool wait_endpoint_404(RestClient &rest_client, const std::string &uri,
                       std::chrono::milliseconds max_wait_time) noexcept;
#endif
