/*
  Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <thread>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include "dim.h"
#include "gmock/gmock.h"
#include "mock_server_rest_client.h"
#include "mysql/harness/logging/registry.h"
#include "mysql_session.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"

#include "mysqlrouter/rest_client.h"

Path g_origin_path;

static constexpr const char kMockServerConnectionsRestUri[] =
    "/api/v1/mock_server/connections/";
static constexpr const char kMockServerInvalidRestUri[] =
    "/api/v1/mock_server/global/";

// can't use constexpr here as DevStudio 12.5 says it isn't a constant
// expression
static const std::chrono::milliseconds kMockServerMaxRestEndpointWaitTime{1000};
static const std::chrono::milliseconds kMockServerMaxRestEndpointStepTime{50};

// AddressSanitizer gets confused by the default, MemoryPoolAllocator
// Solaris sparc also gets crashes
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
using JsonValue =
    rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::CrtAllocator>;

class RestMockServerTest : public RouterComponentTest, public ::testing::Test {
 protected:
  TcpPortPool port_pool_;

  RestMockServerTest() {
    set_origin(g_origin_path);

    RouterComponentTest::init();
  }

  /**
   * wait until a REST endpoint returns !404.
   *
   * at mock startup the socket starts to listen before the REST endpoint gets
   * registered. As long as it returns 404 Not Found we should wait and retry.
   *
   * @param rest_client initialized rest-client
   * @param uri REST endpoint URI to check
   * @param max_wait_time max time to wait for endpoint being ready
   * @returns true once endpoint doesn't return 404 anymore, fails otherwise
   */
  bool wait_for_rest_endpoint_ready(
      RestClient &rest_client, const std::string &uri,
      std::chrono::milliseconds max_wait_time) const noexcept {
    while (max_wait_time.count() > 0) {
      auto req = rest_client.request_sync(HttpMethod::Get, uri);

      if (req && req.get_response_code() != 0 && req.get_response_code() != 404)
        return true;

      auto wait_time =
          std::min(kMockServerMaxRestEndpointStepTime, max_wait_time);
      std::this_thread::sleep_for(wait_time);

      max_wait_time -= wait_time;
    }

    return false;
  }
};

/**
 * base class.
 *
 * starts mock-server with named script and waits for it to startup
 */
class RestMockServerScriptTest : public RestMockServerTest {
 protected:
  RestMockServerScriptTest(const std::string &stmt_file)
      : server_port_{port_pool_.get_next_available()},
        http_port_{port_pool_.get_next_available()},
        json_stmts_{get_data_dir().join(stmt_file).str()},
        server_mock_{launch_mysql_server_mock(json_stmts_, server_port_, false,
                                              http_port_)} {
    SCOPED_TRACE("// start mock-server with http-port");

    const std::string http_hostname{"127.0.0.1"};

    EXPECT_TRUE(wait_for_port_ready(server_port_, 1000))
        << server_mock_.get_full_output();
  }

  const uint16_t server_port_;
  const uint16_t http_port_;
  const std::string json_stmts_;

  RouterComponentTest::CommandHandle server_mock_;
};

class RestMockServerScriptsWorkTest
    : public RestMockServerTest,
      public ::testing::WithParamInterface<std::string> {};

class RestMockServerScriptsThrowsTest
    : public RestMockServerTest,
      public ::testing::WithParamInterface<
          std::tuple<const char *, const char *>> {};

class RestMockServerConnectThrowsTest
    : public RestMockServerTest,
      public ::testing::WithParamInterface<
          std::tuple<const char *, const char *>> {};

class RestMockServerRestServerMockTest : public RestMockServerScriptTest {
 public:
  RestMockServerRestServerMockTest()
      : RestMockServerScriptTest("rest_server_mock.js") {}
};

/**
 * test mock-server loaded the REST bridge.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 *
 * verifies:
 *
 * - WL12118
 *   - TS_1-6
 */
TEST_F(RestMockServerRestServerMockTest, get_globals_empty) {
  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname, http_port_);

  SCOPED_TRACE("// wait for REST endpoint");
  ASSERT_TRUE(wait_for_rest_endpoint_ready(rest_client, http_uri,
                                           kMockServerMaxRestEndpointWaitTime))
      << server_mock_.get_full_output();

  SCOPED_TRACE("// make a http connections");
  auto req = rest_client.request_sync(HttpMethod::Get, http_uri);

  SCOPED_TRACE("// checking HTTP response");
  ASSERT_TRUE(req) << "HTTP Request to " << http_hostname << ":"
                   << std::to_string(http_port_)
                   << " failed (early): " << req.error_msg() << std::endl
                   << server_mock_.get_full_output() << std::endl;

  ASSERT_GT(req.get_response_code(), 0u)
      << "HTTP Request to " << http_hostname << ":"
      << std::to_string(http_port_) << " failed: " << req.error_msg()
      << std::endl
      << server_mock_.get_full_output() << std::endl;

  EXPECT_EQ(req.get_response_code(), 200u);
  EXPECT_THAT(req.get_input_headers().get("Content-Type"),
              ::testing::StrEq("application/json"));

  auto resp_body = req.get_input_buffer();
  EXPECT_GT(resp_body.length(), 0u);
  auto resp_body_content = resp_body.pop_front(resp_body.length());

  // parse json

  std::string json_payload(resp_body_content.begin(), resp_body_content.end());

  JsonDocument json_doc;
  json_doc.Parse(json_payload.c_str());

  EXPECT_TRUE(!json_doc.HasParseError()) << json_payload;
}

template <typename T>
std::string unit();

template <>
std::string unit<std::chrono::seconds>() {
  return "s";
}

template <>
std::string unit<std::chrono::milliseconds>() {
  return "ms";
}

template <>
std::string unit<std::chrono::microseconds>() {
  return "us";
}

template <>
std::string unit<std::chrono::nanoseconds>() {
  return "ns";
}

// must be defined in the same namespace as the type we want to convert
namespace std {

// add to-stream method for all durations for pretty printing
template <class Rep, class Per>
void PrintTo(const chrono::duration<Rep, Per> &span, std::ostream *os) {
  *os << span.count() << unit<typename std::decay<decltype(span)>::type>();
}

}  // namespace std

/**
 * test handshake's exec_time can be set via globals.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerRestServerMockTest, handshake_exec_time_via_global) {
  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  // handshake exec_time to test
  const auto kDelay = std::chrono::milliseconds{100};

  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname, http_port_);

  SCOPED_TRACE("// wait for REST endpoint");
  ASSERT_TRUE(wait_for_rest_endpoint_ready(rest_client, http_uri,
                                           kMockServerMaxRestEndpointWaitTime))
      << server_mock_.get_full_output();

  SCOPED_TRACE("// make a http connections");
  auto req = rest_client.request_sync(
      HttpMethod::Put, http_uri,
      "{\"connect_exec_time\": " + std::to_string(kDelay.count()) + "}");

  SCOPED_TRACE("// checking HTTP response");
  ASSERT_TRUE(req) << "HTTP Request to " << http_hostname << ":"
                   << std::to_string(http_port_)
                   << " failed (early): " << req.error_msg() << std::endl
                   << server_mock_.get_full_output() << std::endl;

  ASSERT_GT(req.get_response_code(), 0u)
      << "HTTP Request to " << http_hostname << ":"
      << std::to_string(http_port_) << " failed: " << req.error_msg()
      << std::endl
      << server_mock_.get_full_output() << std::endl;

  EXPECT_EQ(req.get_response_code(), 204u);

  auto resp_body = req.get_input_buffer();
  EXPECT_EQ(resp_body.length(), 0u);

  SCOPED_TRACE("// slow connect");
  auto start_tp = std::chrono::steady_clock::now();
  {
    mysqlrouter::MySQLSession client;

    SCOPED_TRACE("// connecting via mysql protocol");
    ASSERT_NO_THROW(client.connect("127.0.0.1", server_port_, "username",
                                   "password", "", ""))
        << server_mock_.get_full_output();
  }

  // this test is very vague on how to write a stable test:
  //
  // on a slow box creating the TCP connection itself may be slow
  // which may make the test positive even though exec_time was not honoured.
  //
  // On the other side we can't compare the timespan against
  // a non-delayed connect as the external connect time depends
  // on what else happens on the system while the tests are running
  EXPECT_GT(std::chrono::steady_clock::now() - start_tp, kDelay);
}

/**
 * test mock-server's REST bridge denies unknown URLs.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 *
 * verifies:
 *
 * - WL12118
 *   - TS_1-7
 */
TEST_F(RestMockServerRestServerMockTest, unknown_url_fails) {
  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerInvalidRestUri;

  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname, http_port_);

  SCOPED_TRACE("// wait for HTTP server listening");
  ASSERT_TRUE(wait_for_port_ready(http_port_, 1000))
      << server_mock_.get_full_output();

  SCOPED_TRACE("// make a http connections");
  auto req = rest_client.request_sync(HttpMethod::Get, http_uri);

  SCOPED_TRACE("// checking HTTP response");
  ASSERT_TRUE(req) << "HTTP Request to " << http_hostname << ":"
                   << std::to_string(http_port_)
                   << " failed (early): " << req.error_msg() << std::endl;

  ASSERT_GT(req.get_response_code(), 0u)
      << "HTTP Request to " << http_hostname << ":"
      << std::to_string(http_port_) << " failed: " << req.error_msg()
      << std::endl;

  EXPECT_EQ(req.get_response_code(), 404u);
  EXPECT_THAT(req.get_input_headers().get("Content-Type"),
              ::testing::StrEq("text/html"));

  auto resp_body = req.get_input_buffer();
  EXPECT_GT(resp_body.length(), 0u);
  auto resp_body_content = resp_body.pop_front(resp_body.length());
}

/**
 * test storing globals in mock_server via REST bridge.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerRestServerMockTest, put_globals_no_json) {
  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname, http_port_);

  SCOPED_TRACE("// wait for REST endpoint");
  ASSERT_TRUE(wait_for_rest_endpoint_ready(rest_client, http_uri,
                                           kMockServerMaxRestEndpointWaitTime))
      << server_mock_.get_full_output();

  SCOPED_TRACE("// make a http connections");
  auto req = rest_client.request_sync(HttpMethod::Put, http_uri);

  SCOPED_TRACE("// checking HTTP response");
  ASSERT_TRUE(req) << "HTTP Request to " << http_hostname << ":"
                   << std::to_string(http_port_)
                   << " failed (early): " << req.error_msg() << std::endl
                   << server_mock_.get_full_output() << std::endl;

  ASSERT_GT(req.get_response_code(), 0u)
      << "HTTP Request to " << http_hostname << ":"
      << std::to_string(http_port_) << " failed: " << req.error_msg()
      << std::endl
      << server_mock_.get_full_output() << std::endl;

  EXPECT_EQ(req.get_response_code(), 415u);

  auto resp_body = req.get_input_buffer();
  EXPECT_EQ(resp_body.length(), 0u);
}

/**
 * ensure PUT against / fails.
 *
 * verifies:
 *
 *   - WL12118
 *     - TS_1-10
 */
TEST_F(RestMockServerRestServerMockTest, put_root_fails) {
  const std::string http_hostname{"127.0.0.1"};

  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname, http_port_);

  SCOPED_TRACE("// wait for REST endpoint");
  ASSERT_TRUE(wait_for_rest_endpoint_ready(rest_client,
                                           kMockServerGlobalsRestUri,
                                           kMockServerMaxRestEndpointWaitTime))
      << server_mock_.get_full_output();

  SCOPED_TRACE("// make a http connections");
  auto req = rest_client.request_sync(HttpMethod::Put, "/", "{}");

  SCOPED_TRACE("// checking HTTP response");
  ASSERT_TRUE(req) << "HTTP Request to " << http_hostname << ":"
                   << std::to_string(http_port_)
                   << " failed (early): " << req.error_msg() << std::endl
                   << server_mock_.get_full_output() << std::endl;

  ASSERT_GT(req.get_response_code(), 0u)
      << "HTTP Request to " << http_hostname << ":"
      << std::to_string(http_port_) << " failed: " << req.error_msg()
      << std::endl
      << server_mock_.get_full_output() << std::endl;

  EXPECT_EQ(req.get_response_code(), 404u);

  auto resp_body = req.get_input_buffer();
  EXPECT_NE(resp_body.length(), 0u);
}

class RestMockServerRequireTest : public RestMockServerScriptTest {
 protected:
  RestMockServerRequireTest()
      : RestMockServerScriptTest("js_test_require.js") {}
};

class RestMockServerRequirePTest : public RestMockServerRequireTest,
                                   public ::testing::WithParamInterface<
                                       std::tuple<const char *, const char *>> {
};

/**
 * ensure require() honours load-order.
 *
 * verifies:
 *
 *   - WL11861
 *     - TS_1-8
 */
TEST_P(RestMockServerRequirePTest, require) {
  // mysql query
  mysqlrouter::MySQLSession client;

  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_NO_THROW(
      client.connect("127.0.0.1", server_port_, "username", "password", "", ""))
      << server_mock_.get_full_output();

  EXPECT_NO_THROW({
    std::unique_ptr<mysqlrouter::MySQLSession::ResultRow> result{
        client.query_one(std::get<0>(GetParam()))};
    ASSERT_NE(nullptr, result.get());
    ASSERT_EQ(1u, result->size());
    EXPECT_EQ(std::string((*result)[0]), std::get<1>(GetParam()));
  });
}

INSTANTIATE_TEST_CASE_P(
    js_require_paths, RestMockServerRequirePTest,
    ::testing::Values(std::make_tuple("direct", "direct"),
                      std::make_tuple("dir-with-indexjs", "dir-with-index.js"),
                      std::make_tuple("dir-with-packagejson",
                                      "dir-with-package.json")));

/**
 * ensure require() only loads and evalutes modules once.
 *
 * js_test_require.js requires the same module twice which exposes
 * a counter function.
 *
 * calling the counter via the first module, and via the 2nd module
 * should both increment the same counter if a module is only
 * loaded once.
 *
 * verifies:
 *
 *   - WL11861
 *     - TS_1-7
 */
TEST_F(RestMockServerRequireTest, no_reload) {
  SCOPED_TRACE("// connecting via mysql protocol");

  // mysql query
  mysqlrouter::MySQLSession client;

  ASSERT_NO_THROW(
      client.connect("127.0.0.1", server_port_, "username", "password", "", ""))
      << server_mock_.get_full_output();

  SCOPED_TRACE("// via first module");
  EXPECT_NO_THROW({
    std::unique_ptr<mysqlrouter::MySQLSession::ResultRow> result{
        client.query_one("no-reload-0")};
    ASSERT_NE(nullptr, result.get());
    ASSERT_EQ(1u, result->size());
    EXPECT_EQ(std::string((*result)[0]), "0");
  });

  SCOPED_TRACE("// via 2nd module");
  EXPECT_NO_THROW({
    std::unique_ptr<mysqlrouter::MySQLSession::ResultRow> result{
        client.query_one("no-reload-1")};
    ASSERT_NE(nullptr, result.get());
    ASSERT_EQ(1u, result->size());
    EXPECT_EQ(std::string((*result)[0]), "1");
  });
}

class RestMockServerNestingTest : public RestMockServerScriptTest {
 protected:
  RestMockServerNestingTest()
      : RestMockServerScriptTest("js_test_nesting.js") {}
};

/**
 * ensure require() can be deeply nested.
 *
 *
 *
 * verifies:
 *
 *   - WL11861
 *     - TS_1-10
 */
TEST_F(RestMockServerNestingTest, nesting) {
  SCOPED_TRACE("// connecting via mysql protocol");

  // mysql query
  mysqlrouter::MySQLSession client;

  ASSERT_THROW_LIKE(
      client.connect("127.0.0.1", server_port_, "username", "password", "", ""),
      mysqlrouter::MySQLSession::Error,
      "test-require-nesting-5.js:5: SyntaxError: parse error");
}

class RestMockServerMethodsTest
    : public RestMockServerRestServerMockTest,
      public ::testing::WithParamInterface<
          std::tuple<unsigned int, std::string, unsigned int>> {};

/**
 * ensure OPTIONS, HEAD and others work.
 *
 * verifies:
 *
 *   - WL12118
 *     - TS_1-11
 */
TEST_P(RestMockServerMethodsTest, methods_avail) {
  const std::string http_hostname{"127.0.0.1"};

  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname, http_port_);

  SCOPED_TRACE("// wait for REST endpoint");
  ASSERT_TRUE(wait_for_rest_endpoint_ready(rest_client,
                                           kMockServerGlobalsRestUri,
                                           kMockServerMaxRestEndpointWaitTime))
      << server_mock_.get_full_output();

  SCOPED_TRACE("// make a http connections");
  auto req = rest_client.request_sync(std::get<0>(GetParam()),
                                      std::get<1>(GetParam()));

  SCOPED_TRACE("// checking HTTP response");
  ASSERT_TRUE(req) << "HTTP Request to " << http_hostname << ":"
                   << std::to_string(http_port_)
                   << " failed (early): " << req.error_msg() << std::endl
                   << server_mock_.get_full_output() << std::endl;

  ASSERT_GT(req.get_response_code(), 0u)
      << "HTTP Request to " << http_hostname << ":"
      << std::to_string(http_port_) << " failed: " << req.error_msg()
      << std::endl
      << server_mock_.get_full_output() << std::endl;

  EXPECT_EQ(std::get<2>(GetParam()), req.get_response_code());
}

INSTANTIATE_TEST_CASE_P(
    api__v1__mock_server__globals, RestMockServerMethodsTest,
    ::testing::Values(
        std::make_tuple(HttpMethod::Get, kMockServerGlobalsRestUri,
                        HttpStatusCode::Ok),
        std::make_tuple(HttpMethod::Put, kMockServerGlobalsRestUri,
                        HttpStatusCode::UnsupportedMediaType),
        std::make_tuple(HttpMethod::Delete, kMockServerGlobalsRestUri,
                        HttpStatusCode::MethodNotAllowed),
        std::make_tuple(HttpMethod::Trace, kMockServerGlobalsRestUri,
                        HttpStatusCode::NotImplemented),
        std::make_tuple(HttpMethod::Options, kMockServerGlobalsRestUri,
                        HttpStatusCode::NotImplemented),
        std::make_tuple(HttpMethod::Connect, kMockServerGlobalsRestUri,
                        HttpStatusCode::NotImplemented),
        std::make_tuple(HttpMethod::Head, kMockServerGlobalsRestUri,
                        HttpStatusCode::MethodNotAllowed)));

INSTANTIATE_TEST_CASE_P(
    api__v1__mock_server__connections, RestMockServerMethodsTest,
    ::testing::Values(
        std::make_tuple(HttpMethod::Get, kMockServerConnectionsRestUri,
                        HttpStatusCode::MethodNotAllowed),
        std::make_tuple(HttpMethod::Put, kMockServerConnectionsRestUri,
                        HttpStatusCode::MethodNotAllowed),
        std::make_tuple(HttpMethod::Delete, kMockServerConnectionsRestUri,
                        HttpStatusCode::Ok),
        std::make_tuple(HttpMethod::Trace, kMockServerConnectionsRestUri,
                        HttpStatusCode::NotImplemented),
        std::make_tuple(HttpMethod::Options, kMockServerConnectionsRestUri,
                        HttpStatusCode::NotImplemented),
        std::make_tuple(HttpMethod::Connect, kMockServerConnectionsRestUri,
                        HttpStatusCode::NotImplemented),
        std::make_tuple(HttpMethod::Head, kMockServerConnectionsRestUri,
                        HttpStatusCode::MethodNotAllowed)));

/**
 * test storing globals in mock_server via REST bridge.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerRestServerMockTest, put_globals_ok) {
  SCOPED_TRACE("// start mock-server with http-port");

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname, http_port_);

  SCOPED_TRACE("// wait for REST endpoint");
  ASSERT_TRUE(wait_for_rest_endpoint_ready(rest_client, http_uri,
                                           kMockServerMaxRestEndpointWaitTime))
      << server_mock_.get_full_output();

  SCOPED_TRACE("// make a http connections");
  auto req = rest_client.request_sync(HttpMethod::Put, http_uri, "{}");

  SCOPED_TRACE("// checking HTTP response");
  ASSERT_TRUE(req) << "HTTP Request to " << http_hostname << ":"
                   << std::to_string(http_port_)
                   << " failed (early): " << req.error_msg() << std::endl
                   << server_mock_.get_full_output() << std::endl;

  ASSERT_GT(req.get_response_code(), 0u)
      << "HTTP Request to " << http_hostname << ":"
      << std::to_string(http_port_) << " failed: " << req.error_msg()
      << std::endl
      << server_mock_.get_full_output() << std::endl;

  EXPECT_EQ(req.get_response_code(), 204u);

  auto resp_body = req.get_input_buffer();
  EXPECT_EQ(resp_body.length(), 0u);
}

class RestMockServerRequestTest
    : public RestMockServerRestServerMockTest,
      public ::testing::WithParamInterface<
          std::tuple<int, const char *, const char *, unsigned int>> {};

/**
 * ensure valid and invalid JSON results in the correct behaviour.
 *
 * verifies:
 *
 * - WL12118
 *   - TS_1-8
 */
TEST_P(RestMockServerRequestTest, request) {
  SCOPED_TRACE("// start mock-server with http-port");

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname, http_port_);

  SCOPED_TRACE("// wait for REST endpoint");
  ASSERT_TRUE(wait_for_rest_endpoint_ready(rest_client, http_uri,
                                           kMockServerMaxRestEndpointWaitTime))
      << server_mock_.get_full_output();

  SCOPED_TRACE("// make a http connections");
  auto req =
      rest_client.request_sync(std::get<0>(GetParam()), std::get<1>(GetParam()),
                               std::get<2>(GetParam()));

  SCOPED_TRACE("// checking HTTP response");
  ASSERT_TRUE(req) << "HTTP Request to " << http_hostname << ":"
                   << std::to_string(http_port_)
                   << " failed (early): " << req.error_msg() << std::endl
                   << server_mock_.get_full_output() << std::endl;

  ASSERT_GT(req.get_response_code(), 0u)
      << "HTTP Request to " << http_hostname << ":"
      << std::to_string(http_port_) << " failed: " << req.error_msg()
      << std::endl
      << server_mock_.get_full_output() << std::endl;

  EXPECT_EQ(req.get_response_code(), std::get<3>(GetParam()));

  // if we executed a PUT against globals which was not meant to succeed,
  // check the globals are unchanged.
  if (std::get<0>(GetParam()) == HttpMethod::Put &&
      std::get<1>(GetParam()) == std::string(kMockServerGlobalsRestUri) &&
      std::get<3>(GetParam()) != HttpStatusCode::NoContent) {
    auto get_req = rest_client.request_sync(HttpMethod::Get, http_uri);
    SCOPED_TRACE("// checking GET response");
    ASSERT_TRUE(get_req) << "HTTP Request to " << http_hostname << ":"
                         << std::to_string(http_port_)
                         << " failed (early): " << get_req.error_msg()
                         << std::endl
                         << server_mock_.get_full_output() << std::endl;

    ASSERT_GT(get_req.get_response_code(), 0u)
        << "HTTP Request to " << http_hostname << ":"
        << std::to_string(http_port_) << " failed: " << get_req.error_msg()
        << std::endl
        << server_mock_.get_full_output() << std::endl;

    EXPECT_EQ(get_req.get_response_code(), 200u);
    EXPECT_THAT(get_req.get_input_headers().get("Content-Type"),
                ::testing::StrEq("application/json"));

    auto get_resp_body = get_req.get_input_buffer();
    EXPECT_GT(get_resp_body.length(), 0u);
    auto get_resp_body_content =
        get_resp_body.pop_front(get_resp_body.length());

    // parse json

    std::string json_payload(get_resp_body_content.begin(),
                             get_resp_body_content.end());

    JsonDocument json_doc;
    json_doc.Parse(json_payload.c_str());

    EXPECT_TRUE(!json_doc.HasParseError());
    EXPECT_THAT(json_payload, ::testing::StrEq("{}"));
  }
}

INSTANTIATE_TEST_CASE_P(
    api__v1__mock_server__globals, RestMockServerRequestTest,
    ::testing::Values(
        // parse error
        std::make_tuple(HttpMethod::Put, kMockServerGlobalsRestUri, "[",
                        HttpStatusCode::UnprocessableEntity),

        // not an object
        std::make_tuple(HttpMethod::Put, kMockServerGlobalsRestUri, "[]",
                        HttpStatusCode::UnprocessableEntity),

        // parse-error
        std::make_tuple(HttpMethod::Put, kMockServerGlobalsRestUri, "{1}",
                        HttpStatusCode::UnprocessableEntity),

        // not-an-object
        std::make_tuple(HttpMethod::Put, kMockServerGlobalsRestUri, "1",
                        HttpStatusCode::UnprocessableEntity)));

/**
 * test storing globals in mock_server via REST bridge.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 *
 * verifies:
 *
 * - WL12118
 *   - TS_1-6
 *   - TS_1-9
 */
TEST_F(RestMockServerRestServerMockTest, put_globals_and_read_back) {
  SCOPED_TRACE("// start mock-server with http-port");

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  SCOPED_TRACE("// make a http connections");
  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname, http_port_);

  SCOPED_TRACE("// wait for REST endpoint");
  ASSERT_TRUE(wait_for_rest_endpoint_ready(rest_client, http_uri,
                                           kMockServerMaxRestEndpointWaitTime))
      << server_mock_.get_full_output();

  auto put_req = rest_client.request_sync(HttpMethod::Put, http_uri,
                                          "{\"key\": [ [1, 2, 3 ] ]}");

  SCOPED_TRACE("// checking PUT response");
  ASSERT_TRUE(put_req) << "HTTP Request to " << http_hostname << ":"
                       << std::to_string(http_port_)
                       << " failed (early): " << put_req.error_msg()
                       << std::endl
                       << server_mock_.get_full_output() << std::endl;

  ASSERT_GT(put_req.get_response_code(), 0u)
      << "HTTP Request to " << http_hostname << ":"
      << std::to_string(http_port_) << " failed: " << put_req.error_msg()
      << std::endl
      << server_mock_.get_full_output() << std::endl;

  EXPECT_EQ(put_req.get_response_code(), 204u);

  auto put_resp_body = put_req.get_input_buffer();
  EXPECT_EQ(put_resp_body.length(), 0u);

  // GET request

  auto get_req = rest_client.request_sync(HttpMethod::Get, http_uri);
  SCOPED_TRACE("// checking GET response");
  ASSERT_TRUE(get_req) << "HTTP Request to " << http_hostname << ":"
                       << std::to_string(http_port_)
                       << " failed (early): " << get_req.error_msg()
                       << std::endl
                       << server_mock_.get_full_output() << std::endl;

  ASSERT_GT(get_req.get_response_code(), 0u)
      << "HTTP Request to " << http_hostname << ":"
      << std::to_string(http_port_) << " failed: " << get_req.error_msg()
      << std::endl
      << server_mock_.get_full_output() << std::endl;

  EXPECT_EQ(get_req.get_response_code(), 200u);
  EXPECT_THAT(get_req.get_input_headers().get("Content-Type"),
              ::testing::StrEq("application/json"));

  auto get_resp_body = get_req.get_input_buffer();
  EXPECT_GT(get_resp_body.length(), 0u);
  auto get_resp_body_content = get_resp_body.pop_front(get_resp_body.length());

  // parse json

  std::string json_payload(get_resp_body_content.begin(),
                           get_resp_body_content.end());

  JsonDocument json_doc;
  json_doc.Parse(json_payload.c_str());

  EXPECT_TRUE(!json_doc.HasParseError());
  EXPECT_THAT(json_payload, ::testing::StrEq("{\"key\":[[1,2,3]]}"));
}

/**
 * test DELETE connections.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 *
 * verifies:
 *
 * - WL12118
 *   - TS_1-2
 */
TEST_F(RestMockServerRestServerMockTest, delete_all_connections) {
  SCOPED_TRACE("// start mock-server with http-port");

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerConnectionsRestUri;

  IOContext io_ctx;
  RestClient rest_client(io_ctx, http_hostname, http_port_);

  SCOPED_TRACE("// wait for REST endpoint");
  ASSERT_TRUE(wait_for_rest_endpoint_ready(rest_client, http_uri,
                                           kMockServerMaxRestEndpointWaitTime))
      << server_mock_.get_full_output();

  // mysql query
  mysqlrouter::MySQLSession client;

  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_NO_THROW(
      client.connect("127.0.0.1", server_port_, "username", "password", "", ""))
      << server_mock_.get_full_output();

  SCOPED_TRACE("// check connection works");
  std::unique_ptr<mysqlrouter::MySQLSession::ResultRow> result{
      client.query_one("select @@port")};
  ASSERT_NE(nullptr, result.get());
  ASSERT_EQ(1u, result->size());
  EXPECT_EQ(std::to_string(server_port_), std::string((*result)[0]));

  SCOPED_TRACE("// make a http connections");
  auto req = rest_client.request_sync(HttpMethod::Delete, http_uri, "{}");

  SCOPED_TRACE("// checking HTTP response");
  ASSERT_TRUE(req) << "HTTP Request to " << http_hostname << ":"
                   << std::to_string(http_port_)
                   << " failed (early): " << req.error_msg() << std::endl
                   << server_mock_.get_full_output() << std::endl;

  ASSERT_GT(req.get_response_code(), 0u)
      << "HTTP Request to " << http_hostname << ":"
      << std::to_string(http_port_) << " failed: " << req.error_msg()
      << std::endl
      << server_mock_.get_full_output() << std::endl;

  EXPECT_EQ(req.get_response_code(), 200u);

  auto resp_body = req.get_input_buffer();
  EXPECT_EQ(resp_body.length(), 0u);

  SCOPED_TRACE("// check connection is killed");
  EXPECT_THROW_LIKE(result.reset(client.query_one("select @@port")),
                    mysqlrouter::MySQLSession::Error,
                    "Lost connection to MySQL server during query");
}

/**
 * ensure @@port reported by mock is real port.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_F(RestMockServerRestServerMockTest, select_port) {
  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  // mysql query
  mysqlrouter::MySQLSession client;

  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_NO_THROW(
      client.connect("127.0.0.1", server_port_, "username", "password", "", ""))
      << server_mock_.get_full_output();

  EXPECT_NO_THROW({
    std::unique_ptr<mysqlrouter::MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    ASSERT_NE(nullptr, result.get());
    ASSERT_EQ(1u, result->size());
    EXPECT_EQ(std::to_string(server_port_), std::string((*result)[0]));
  });
}

// make pretty param-names
static std::string sanitize_param_name(const std::string &name) {
  std::string p{name};
  for (auto &c : p) {
    if (!isalpha(c) && !isdigit(c)) c = '_';
  }
  return p;
}

/**
 * ensure connect returns error.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_P(RestMockServerConnectThrowsTest, js_test_stmts_is_string) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts =
      get_data_dir().join(std::get<0>(GetParam())).str();
  auto server_mock =
      launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000))
      << server_mock.get_full_output();

  mysqlrouter::MySQLSession client;

  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_THROW_LIKE(
      client.connect("127.0.0.1", server_port, "username", "password", "", ""),
      mysqlrouter::MySQLSession::Error, std::get<1>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(
    ScriptsFails, RestMockServerConnectThrowsTest,
    ::testing::Values(
        std::make_tuple("js_test_parse_error.js",
                        "parse error"),  // WL11861 TS-1_2
        std::make_tuple("js_test_stmts_is_string.js",
                        "expected 'stmts' to be"),  // WL11861 TS-1_4
        std::make_tuple("js_test_empty_file.js",
                        "expected statement handler to return an object, got "
                        "primitive, undefined"),  // WL11861 TS-1_4
        std::make_tuple("js_test_handshake_greeting_exec_time_is_empty.js",
                        "exec_time must be a number, if set. Is object"),
        std::make_tuple(
            "js_test_handshake_is_string.js",
            "handshake must be a object, if set. Is primitive, string")),
    [](const ::testing::TestParamInfo<std::tuple<const char *, const char *>>
           &info) -> std::string {
      return sanitize_param_name(std::get<0>(info.param));
    });

/**
 * ensure int fields in 'columns' can't be negative.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 * - run a query which triggers the server-side exception
 */
TEST_P(RestMockServerScriptsThrowsTest, scripts_throws) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts =
      get_data_dir().join(std::get<0>(GetParam())).str();
  auto server_mock =
      launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000))
      << server_mock.get_full_output();

  mysqlrouter::MySQLSession client;

  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_NO_THROW(
      client.connect("127.0.0.1", server_port, "username", "password", "", ""));

  SCOPED_TRACE("// select @@port");
  ASSERT_THROW_LIKE(client.query_one("select @@port"),
                    mysqlrouter::MySQLSession::Error, std::get<1>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(
    ScriptsFails, RestMockServerScriptsThrowsTest,
    ::testing::Values(
        std::make_tuple("js_test_stmts_result_has_negative_int.js",
                        "value out-of-range for field \"decimals\""),
        std::make_tuple(
            "js_test_stmts_result_has_infinity.js",
            "value out-of-range for field \"decimals\""),  // WL11861 TS-1_11
        std::make_tuple("js_test_stmts_result_has_repeat.js",
                        "repeat is not supported"),  // WL11861 TS-1_5
        std::make_tuple("js_test_stmts_is_empty.js",
                        "executing statement failed: Unsupported command in "
                        "handle_statement()")),
    [](const ::testing::TestParamInfo<std::tuple<const char *, const char *>>
           &info) -> std::string {
      return sanitize_param_name(std::get<0>(info.param));
    });

/**
 * ensure script works.
 *
 * - start the mock-server
 * - make a client connect to the mock-server
 */
TEST_P(RestMockServerScriptsWorkTest, scripts_work) {
  SCOPED_TRACE("// start mock-server with http-port");

  const unsigned server_port = port_pool_.get_next_available();
  const unsigned http_port = port_pool_.get_next_available();
  const std::string json_stmts = get_data_dir().join(GetParam()).str();
  auto server_mock =
      launch_mysql_server_mock(json_stmts, server_port, false, http_port);

  std::string http_hostname = "127.0.0.1";
  std::string http_uri = kMockServerGlobalsRestUri;

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000))
      << server_mock.get_full_output();

  mysqlrouter::MySQLSession client;

  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_NO_THROW(
      client.connect("127.0.0.1", server_port, "username", "password", "", ""));

  SCOPED_TRACE("// select @@port");
  ASSERT_NO_THROW(client.execute("select @@port"));
}

INSTANTIATE_TEST_CASE_P(
    ScriptsWork, RestMockServerScriptsWorkTest,
    ::testing::Values("metadata_3_secondaries.js", "simple-client.js",
                      "js_test_handshake_is_empty.js",
                      "js_test_handshake_greeting_is_empty.js",
                      "js_test_handshake_greeting_exec_time_is_number.js",
                      "js_test_stmts_is_array.js",
                      "js_test_stmts_is_coroutine.js",
                      "js_test_stmts_is_function.js"),
    [](const ::testing::TestParamInfo<std::string> &info) -> std::string {
      return sanitize_param_name(info.param);
    });

static void init_DIM() {
  mysql_harness::DIM &dim = mysql_harness::DIM::instance();

  // logging facility
  dim.set_LoggingRegistry(
      []() {
        static mysql_harness::logging::Registry registry;
        return &registry;
      },
      [](mysql_harness::logging::Registry *) {}  // don't delete our static!
  );
  mysql_harness::logging::Registry &registry = dim.get_LoggingRegistry();

  mysql_harness::logging::create_module_loggers(
      registry, mysql_harness::logging::LogLevel::kWarning,
      {mysql_harness::logging::kMainLogger, "sql"},
      mysql_harness::logging::kMainLogger);
  mysql_harness::logging::create_main_log_handler(registry, "", "", true);

  registry.set_ready();
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  init_DIM();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
