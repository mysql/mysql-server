/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif
#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "config_builder.h"
#include "dim.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/utility/string.h"  // ::join
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"
#include "test/helpers.h"
#include "test/temp_directory.h"

using namespace std::string_literals;

using namespace std::chrono_literals;

class RestRoutingApiTest
    : public RestApiComponentTest,
      public ::testing::WithParamInterface<RestApiTestParams> {
 protected:
  RestRoutingApiTest() : mock_port_{port_pool_.get_next_available()} {}

  bool wait_route_ready(std::chrono::milliseconds max_wait_time,
                        const std::string &route_name, const uint16_t http_port,
                        const std::string &http_host,
                        const std::string &username,
                        const std::string &password) {
    const std::string uri =
        std::string(rest_api_basepath) + "/routes/" + route_name + "/health";
    bool res = wait_for_rest_endpoint_ready(uri, http_port, username, password);
    if (!res) return false;

    IOContext io_ctx;
    RestClient rest_client(io_ctx, http_host, http_port, username, password);
    const std::chrono::milliseconds step_time{50};

    while (max_wait_time.count() > 0) {
      auto req = rest_client.request_sync(HttpMethod::Get, uri);

      if (req && req.get_response_code() == HttpStatusCode::Ok) return true;

      const auto wait_time = std::min(step_time, max_wait_time);
      std::this_thread::sleep_for(wait_time);

      max_wait_time -= wait_time;
    }

    return false;
  }

  const uint16_t mock_port_;
  std::vector<uint16_t> routing_ports_;

 public:
  static const size_t kRoutesQty = 5;
};

/*static*/ const size_t RestRoutingApiTest::kRoutesQty;

static const std::vector<SwaggerPath> kRoutingSwaggerPaths{
    {"/routes/{routeName}/config", "Get config of a route", "config of a route",
     "route not found"},
    {"/routes/{routeName}/status", "Get status of a route", "status of a route",
     "route not found"},
    {"/routes/{routeName}/health", "Get health of a route", "health of a route",
     "route not found"},
    {"/routes/{routeName}/destinations", "Get destinations of a route",
     "destinations of a route", "route not found"},
    {"/routes/{routeName}/connections", "Get connections of a route",
     "connections of a route", "route not found"},
    {"/routes/{routeName}/blockedHosts", "Get blocked host list for a route",
     "blocked host list for a route", "route not found"},
    {"/routes", "Get list of the routes", "list of the routes", ""},
};

/**
 * @test check /routes/
 *
 * - start router with rest_routing module loaded
 * - check response code is 200 and output matches openapi spec
 */
TEST_P(RestRoutingApiTest, ensure_openapi) {
  const std::string http_hostname = "127.0.0.1";
  const std::string http_uri = GetParam().uri;
  for (size_t i = 0; i < kRoutesQty; ++i) {
    routing_ports_.push_back(port_pool_.get_next_available());
  }

  const std::string userfile = create_password_file();

  const std::vector<std::string> route_names{"", "_", "123", "Aaz", "ro"};

  auto config_sections = get_restapi_config("rest_routing", userfile,
                                            GetParam().request_authentication);
  size_t i = 0;
  for (const auto &route_name : route_names) {
    // let's make "_" route a metadata cache one, all other are static
    const std::string destinations =
        (route_name == "_") ? "metadata-cache://test/default?role=PRIMARY"
                            : "127.0.0.1:" + std::to_string(mock_port_);
    config_sections.push_back(mysql_harness::ConfigBuilder::build_section(
        std::string("routing") + (route_name.empty() ? "" : ":") + route_name,
        {
            {"bind_port", std::to_string(routing_ports_[i])},
            {"bind_address", "127.0.0.1"},
            {"destinations", destinations},
            {"routing_strategy", "round-robin"},
            {"client_connect_timeout", "60"},
            {"connect_timeout", "70"},
            {"max_connect_errors", "3"},
            {"max_connections", "1000"},
        }));
    ++i;
  }

  // create a "dead" metadata-cache referenced by the routing "_" to check
  // route/health isActive == 0
  const std::string keyring_username = "mysql_router1_user";
  config_sections.push_back(mysql_harness::ConfigBuilder::build_section(
      "metadata_cache:test",
      {
          {"router_id", "3"},
          {"user", keyring_username},
          {"metadata_cluster", "test"},
          // 198.51.100.0/24 is a reserved address block, it could not be
          // connected to. https://tools.ietf.org/html/rfc5737#section-4
          {"bootstrap_server_addresses", "mysql://198.51.100.1"},
          //"ttl", "0.5"
      }));

  std::map<std::string, std::string> default_section = get_DEFAULT_defaults();
  init_keyring(default_section, conf_dir_.name());

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, ""),
      &default_section, "mysqlrouter.conf", "connect_timeout=1")};

  SCOPED_TRACE("// starting router");
  ProcessWrapper &http_server =
      launch_router({"-c", conf_file}, EXIT_SUCCESS, true, false, -1s);

  // doesn't really matter which file we use here, we are not going to do any
  // queries
  const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();

  SCOPED_TRACE("// launch the server mock");
  launch_mysql_server_mock(json_stmts, mock_port_, EXIT_SUCCESS, false);

  // wait for route being available if we expect it to be and plan to do some
  // connections to it (which are routes: "ro" and "Aaz")
  for (size_t i = 3; i < kRoutesQty; ++i) {
    ASSERT_TRUE(wait_route_ready(5000ms, route_names[i], http_port_,
                                 "127.0.0.1", kRestApiUsername,
                                 kRestApiPassword));
  }

  // make 3 connections to route "ro"
  mysqlrouter::MySQLSession client_ro_1;
  EXPECT_NO_THROW(client_ro_1.connect("127.0.0.1", routing_ports_[4], "root",
                                      "fake-pass", "", ""));
  mysqlrouter::MySQLSession client_ro_2;
  EXPECT_NO_THROW(client_ro_2.connect("127.0.0.1", routing_ports_[4], "root",
                                      "fake-pass", "", ""));
  mysqlrouter::MySQLSession client_ro_3;
  EXPECT_NO_THROW(client_ro_3.connect("127.0.0.1", routing_ports_[4], "root",
                                      "fake-pass", "", ""));

  // make 1 connection to route "Aaz"
  mysqlrouter::MySQLSession client_Aaz_1;
  EXPECT_NO_THROW(client_Aaz_1.connect("127.0.0.1", routing_ports_[3], "root",
                                       "fake-pass", "", ""));

  // call wait_port_ready a few times on "123" to trigger blocked client
  // on that route (we set max_connect_errors to 2)
  for (size_t i = 0; i < 3; ++i) {
    ASSERT_TRUE(wait_for_port_ready(routing_ports_[2], 500ms));
  }

  // wait until we see that the Router has blocked the host
  EXPECT_TRUE(wait_log_contains(http_server, "blocking client host", 5s));

  EXPECT_NO_FATAL_FAILURE(
      fetch_and_validate_schema_and_resource(GetParam(), http_server));
}

static const RestApiComponentTest::json_verifiers_t get_expected_status_fields(
    const int expected_max_total_connections,
    const int expected_current_total_connections) {
  return {
      {"/maxTotalConnections",
       [=](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_TRUE(value->IsInt());
         ASSERT_EQ(value->GetInt(), expected_max_total_connections);
       }},
      {"/currentTotalConnections",
       [=](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_TRUE(value->IsInt());
         ASSERT_EQ(value->GetInt(), expected_current_total_connections);
       }},
  };
}

static const RestApiComponentTest::json_verifiers_t
get_expected_routes_status_fields(const int expected_active_connections,
                                  const int expected_total_connections,
                                  const int expected_blocked_hosts) {
  return {
      {"/activeConnections",
       [=](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_TRUE(value->IsInt());
         ASSERT_EQ(value->GetInt(), expected_active_connections);
       }},
      {"/totalConnections",
       [=](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_TRUE(value->IsInt());
         ASSERT_EQ(value->GetInt(), expected_total_connections);
       }},
      {"/blockedHosts",
       [=](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_TRUE(value->IsInt());
         ASSERT_EQ(value->GetInt(), expected_blocked_hosts);
       }},
  };
}

static const RestApiComponentTest::json_verifiers_t
get_expected_config_fields() {
  return {
      {"/bindAddress",
       [](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_TRUE(value->IsString());
         ASSERT_STREQ(value->GetString(), "127.0.0.1");
       }},
      {"/bindPort",
       [](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_GT(value->GetInt(), 0);
       }},
      {"/protocol",
       [](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_TRUE(value->IsString());
         ASSERT_STREQ(value->GetString(), "classic");
       }},
      {"/routingStrategy",
       [](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_TRUE(value->IsString());
         ASSERT_STREQ(value->GetString(), "round-robin");
       }},
      {"/clientConnectTimeoutInMs",
       [](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_TRUE(value->IsUint64());
         ASSERT_EQ(value->GetUint64(), uint64_t(60000));
       }},
      {"/destinationConnectTimeoutInMs",
       [](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_TRUE(value->IsUint64());
         ASSERT_EQ(value->GetUint64(), uint64_t(70000));
       }},
      {"/maxActiveConnections",
       [](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_TRUE(value->IsInt());
         ASSERT_EQ(value->GetInt(), 1000);
       }},
      {"/maxConnectErrors",
       [](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_TRUE(value->IsUint64());
         ASSERT_EQ(value->GetUint64(), uint64_t(3));
       }},
  };
}

static const RestApiComponentTest::json_verifiers_t get_expected_health_fields(
    const bool expected_alive) {
  return {
      {"/isAlive",
       [=](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_EQ(value->GetBool(), expected_alive);
       }},
  };
}

static const RestApiComponentTest::json_verifiers_t
get_expected_destinations_fields(int expected_destinations_num) {
  RestApiComponentTest::json_verifiers_t result{
      {"/items",
       [=](const JsonValue *value) {
         ASSERT_NE(value, nullptr);
         ASSERT_TRUE(value->IsArray());
         ASSERT_EQ(value->GetArray().Size(), expected_destinations_num);
       }},
  };

  for (int i = 0; i < expected_destinations_num; ++i) {
    result.emplace_back("/items/0/address", [](const JsonValue *value) {
      ASSERT_TRUE(value != nullptr);
      ASSERT_TRUE(value->IsString());
      ASSERT_STREQ(value->GetString(), "127.0.0.1");
    });
    result.emplace_back("/items/0/port", [](const JsonValue *value) {
      ASSERT_NE(value, nullptr);
      ASSERT_GT(value->GetInt(), 0);
    });
  }

  return result;
}

static RestApiComponentTest::json_verifiers_t get_expected_blocked_hosts_fields(
    const int expected_blocked_hosts) {
  RestApiComponentTest::json_verifiers_t result{
      {"/items", [=](const JsonValue *value) {
         ASSERT_NE(value, nullptr);
         ASSERT_TRUE(value->IsArray());
         ASSERT_EQ(value->GetArray().Size(), expected_blocked_hosts);
       }}};

  for (int i = 0; i < expected_blocked_hosts; ++i) {
    result.emplace_back(
        "/items/" + std::to_string(i), [](const JsonValue *value) {
          ASSERT_NE(value, nullptr);
          ASSERT_THAT(value->GetString(), ::testing::StartsWith("127.0.0.1"));
        });
  }

  return result;
}

static const RestApiComponentTest::json_verifiers_t
get_expected_connections_fields_fields(const int expected_connection_qty) {
  RestApiComponentTest::json_verifiers_t result{
      {"/items", [=](const JsonValue *value) {
         ASSERT_NE(value, nullptr);
         ASSERT_TRUE(value->IsArray());
         // -1 means that we don't really know how many connections are
         // there, we did wait_for_port_ready on a socket and this can
         // still be accounted for
         if (expected_connection_qty >= 0) {
           ASSERT_EQ(value->GetArray().Size(), expected_connection_qty);
         }
       }}};

  for (int i = 0; i < expected_connection_qty; ++i) {
    result.emplace_back("/items/" + std::to_string(i) + "/bytesToServer",
                        [](const JsonValue *value) {
                          ASSERT_NE(value, nullptr);
                          ASSERT_GT(value->GetUint64(), 0);
                        });

    result.emplace_back("/items/" + std::to_string(i) + "/sourceAddress",
                        [](const JsonValue *value) {
                          ASSERT_NE(value, nullptr);
                          ASSERT_TRUE(value->IsString());
                          ASSERT_THAT(value->GetString(),
                                      ::testing::StartsWith("127.0.0.1"));
                        });

    result.emplace_back("/items/" + std::to_string(i) + "/destinationAddress",
                        [](const JsonValue *value) {
                          ASSERT_NE(value, nullptr);
                          ASSERT_TRUE(value->IsString());
                          ASSERT_THAT(value->GetString(),
                                      ::testing::StartsWith("127.0.0.1"));
                        });

    result.emplace_back(
        "/items/" + std::to_string(i) + "/timeConnectedToServer",
        [](const JsonValue *value) {
          ASSERT_NE(value, nullptr);
          ASSERT_TRUE(value->IsString());

          ASSERT_TRUE(pattern_found(value->GetString(), kTimestampPattern))
              << value->GetString();
        });
  }

  return result;
}

// ****************************************************************************
// Request the resource(s) using supported methods with authentication enabled
// and valid credentials
// ****************************************************************************

static const RestApiTestParams rest_api_valid_methods[]{
    {"routing_status", std::string(rest_api_basepath) + "/routing/status",
     "/routing/status", HttpMethod::Get, HttpStatusCode::Ok, kContentTypeJson,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_status_fields(
         /*expected_max_total_connections=*/512,
         /*expected_current_total_connections=*/3 + 1),
     kRoutingSwaggerPaths},
    {"routing_routes_status_ro",
     std::string(rest_api_basepath) + "/routes/ro/status",
     "/routes/{routeName}/status", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_routes_status_fields(/*expected_active_connections=*/3,
                                       /*expected_total_connections=*/3,
                                       /*expected_blocked_hosts*=*/0),
     kRoutingSwaggerPaths},
    {"routing_routes_status__",
     std::string(rest_api_basepath) + "/routes/_/status",
     "/routes/{routeName}/status", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_routes_status_fields(/*expected_active_connections=*/0,
                                       /*expected_total_connections=*/0,
                                       /*expected_blocked_hosts*=*/0),
     kRoutingSwaggerPaths},
    {"routing_routes_status_Aaz",
     std::string(rest_api_basepath) + "/routes/Aaz/status",
     "/routes/{routeName}/status", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_routes_status_fields(/*expected_active_connections=*/1,
                                       /*expected_total_connections=*/1,
                                       /*expected_blocked_hosts*=*/0),
     kRoutingSwaggerPaths},
    {"routing_routes_status_123",
     std::string(rest_api_basepath) + "/routes/123/status",
     "/routes/{routeName}/status", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_routes_status_fields(/*expected_active_connections=*/0,
                                       /*expected_total_connections=*/3,
                                       /*expected_blocked_hosts*=*/1),
     kRoutingSwaggerPaths},
    {"routing_routes_status_nonexistent",
     std::string(rest_api_basepath) + "/routes/nonexistent/status",
     "/routes/{routeName}/status",
     HttpMethod::Get,
     HttpStatusCode::NotFound,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routing_routes_status_params",
     std::string(rest_api_basepath) + "/routes/123/status?someparam",
     "/routes/{routeName}/status",
     HttpMethod::Get,
     HttpStatusCode::BadRequest,
     kContentTypeJsonProblem,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes",
     std::string(rest_api_basepath) + "/routes",
     "/routes",
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
            ASSERT_EQ(value->GetArray().Size(), RestRoutingApiTest::kRoutesQty);
          }},
         {"/items/0/name",
          [](const JsonValue *value) {
            ASSERT_TRUE(value != nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "");
          }},
         {"/items/1/name",
          [](const JsonValue *value) {
            ASSERT_TRUE(value != nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "123");
          }},
         {"/items/2/name",
          [](const JsonValue *value) {
            ASSERT_TRUE(value != nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "Aaz");
          }},
         {"/items/3/name",
          [](const JsonValue *value) {
            ASSERT_TRUE(value != nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "_");
          }},
         {"/items/4/name",
          [](const JsonValue *value) {
            ASSERT_TRUE(value != nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "ro");
          }},
     },
     kRoutingSwaggerPaths},
    {"routes_config_ro", std::string(rest_api_basepath) + "/routes/ro/config",
     "/routes/{routeName}/config", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_config_fields(),
     kRoutingSwaggerPaths},
    {"routes_config__", std::string(rest_api_basepath) + "/routes/_/config",
     "/routes/{routeName}/config", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_config_fields(),
     kRoutingSwaggerPaths},
    {"routes_config_Aaz", std::string(rest_api_basepath) + "/routes/Aaz/config",
     "/routes/{routeName}/config", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_config_fields(),
     kRoutingSwaggerPaths},
    {"routes_config_123", std::string(rest_api_basepath) + "/routes/123/config",
     "/routes/{routeName}/config", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_config_fields(),
     kRoutingSwaggerPaths},
    {"routes_config_123_status",
     std::string(rest_api_basepath) + "/routes/123/config?param",
     "/routes/{routeName}/config",
     HttpMethod::Get,
     HttpStatusCode::BadRequest,
     kContentTypeJsonProblem,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_config_nonexistent",
     std::string(rest_api_basepath) + "/routes/nonexistent/config",
     "/routes/{routeName}/config", HttpMethod::Get, HttpStatusCode::NotFound,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_config_fields(),
     kRoutingSwaggerPaths},
    {"routes_health_ro", std::string(rest_api_basepath) + "/routes/ro/health",
     "/routes/{routeName}/health", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_health_fields(true),
     kRoutingSwaggerPaths},
    {"routes_health__", std::string(rest_api_basepath) + "/routes/_/health",
     "/routes/{routeName}/health", HttpMethod::Get,
     HttpStatusCode::InternalError, kContentTypeJson, kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true, get_expected_health_fields(false),
     kRoutingSwaggerPaths},
    {"routes_health_Aaz", std::string(rest_api_basepath) + "/routes/Aaz/health",
     "/routes/{routeName}/health", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_health_fields(true),
     kRoutingSwaggerPaths},
    {"routes_health_123", std::string(rest_api_basepath) + "/routes/123/health",
     "/routes/{routeName}/health", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_health_fields(true),
     kRoutingSwaggerPaths},
    {"routes_health_123_params",
     std::string(rest_api_basepath) + "/routes/123/health?someparam",
     "/routes/{routeName}/health",
     HttpMethod::Get,
     HttpStatusCode::BadRequest,
     kContentTypeJsonProblem,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_destinations_ro",
     std::string(rest_api_basepath) + "/routes/ro/destinations",
     "/routes/{routeName}/destinations", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_destinations_fields(1),
     kRoutingSwaggerPaths},
    {"routes_destinations__",
     std::string(rest_api_basepath) + "/routes/_/destinations",
     "/routes/{routeName}/destinations", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_destinations_fields(0),
     kRoutingSwaggerPaths},
    {"routes_destinations_Aaz",
     std::string(rest_api_basepath) + "/routes/Aaz/destinations",
     "/routes/{routeName}/destinations", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_destinations_fields(1),
     kRoutingSwaggerPaths},
    {"routes_destinations_123",
     std::string(rest_api_basepath) + "/routes/123/destinations",
     "/routes/{routeName}/destinations", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_destinations_fields(1),
     kRoutingSwaggerPaths},
    {"routes_destinations_123_param",
     std::string(rest_api_basepath) + "/routes/123/destinations?someparam",
     "/routes/{routeName}/destinations",
     HttpMethod::Get,
     HttpStatusCode::BadRequest,
     kContentTypeJsonProblem,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_destinations_nonexistent",
     std::string(rest_api_basepath) + "/routes/nonexistent/destinations",
     "/routes/{routeName}/destinations", HttpMethod::Get,
     HttpStatusCode::NotFound, kContentTypeJson, kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true, get_expected_destinations_fields(0),
     kRoutingSwaggerPaths},
    {"routes_blockedhosts_ro",
     std::string(rest_api_basepath) + "/routes/ro/blockedHosts",
     "/routes/{routeName}/blockedHosts", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_blocked_hosts_fields(/*expected_blocked_hosts=*/0),
     kRoutingSwaggerPaths},
    {"routes_blockedhosts__",
     std::string(rest_api_basepath) + "/routes/_/blockedHosts",
     "/routes/{routeName}/blockedHosts", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_blocked_hosts_fields(/*expected_blocked_hosts=*/0),
     kRoutingSwaggerPaths},
    {"routes_blockedhosts_Aaz",
     std::string(rest_api_basepath) + "/routes/Aaz/blockedHosts",
     "/routes/{routeName}/blockedHosts", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_blocked_hosts_fields(/*expected_blocked_hosts=*/0),
     kRoutingSwaggerPaths},
    {"routes_blockedhosts_123",
     std::string(rest_api_basepath) + "/routes/123/blockedHosts",
     "/routes/{routeName}/blockedHosts", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_blocked_hosts_fields(/*expected_blocked_hosts=*/1),
     kRoutingSwaggerPaths},
    {"routes_blockedhosts_123_params",
     std::string(rest_api_basepath) + "/routes/123/blockedHosts?someparam",
     "/routes/{routeName}/blockedHosts",
     HttpMethod::Get,
     HttpStatusCode::BadRequest,
     kContentTypeJsonProblem,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routing_blockedhosts_nonexistent",
     std::string(rest_api_basepath) + "/routes/nonexistent/blockedHosts",
     "/routes/{routeName}/blockedHosts",
     HttpMethod::Get,
     HttpStatusCode::NotFound,
     kContentTypeJson,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_connections_ro",
     std::string(rest_api_basepath) + "/routes/ro/connections",
     "/routes/{routeName}/connections", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_connections_fields_fields(/*expected_connection_qty=*/3),
     kRoutingSwaggerPaths},
    {"routes_connections__",
     std::string(rest_api_basepath) + "/routes/_/connections",
     "/routes/{routeName}/connections", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_connections_fields_fields(/*expected_connection_qty=*/0),
     kRoutingSwaggerPaths},
    {"routes_connections_Aaz",
     std::string(rest_api_basepath) + "/routes/Aaz/connections",
     "/routes/{routeName}/connections", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_connections_fields_fields(/*expected_connection_qty=*/1),
     kRoutingSwaggerPaths},
    {"routes_connections_123",
     std::string(rest_api_basepath) + "/routes/123/connections",
     "/routes/{routeName}/connections", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     // -1 means that we don't really know how many connections are there
     // as we did wait_for_port_ready on a socket and this can still be
     // accounted for
     get_expected_connections_fields_fields(/*expected_connection_qty=*/-1),
     kRoutingSwaggerPaths},
    {"routes_connections_123_params",
     std::string(rest_api_basepath) + "/routes/123/connections?params",
     "/routes/{routeName}/connections",
     HttpMethod::Get,
     HttpStatusCode::BadRequest,
     kContentTypeJsonProblem,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_connections_nonexistent",
     std::string(rest_api_basepath) + "/routes/nonexistent/connections",
     "/routes/{routeName}/connections", HttpMethod::Get,
     HttpStatusCode::NotFound, kContentTypeJson, kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_connections_fields_fields(/*expected_connection_qty=*/0),
     kRoutingSwaggerPaths},
};

INSTANTIATE_TEST_SUITE_P(
    ValidMethods, RestRoutingApiTest,
    ::testing::ValuesIn(rest_api_valid_methods),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

// ****************************************************************************
// Request the resource(s) using supported methods with authentication enabled
// and invalid credentials
// ****************************************************************************

static const RestApiTestParams rest_api_valid_methods_invalid_auth_params[]{
    {"routing_status_invalid_auth",
     std::string(rest_api_basepath) + "/routes/ro/status",
     "/routes/{routeName}/status",
     HttpMethod::Get,
     HttpStatusCode::Unauthorized,
     kContentTypeHtmlCharset,
     kRestApiUsername,
     "invalid password",
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_invalid_auth",
     std::string(rest_api_basepath) + "/routes",
     "/routes",
     HttpMethod::Get,
     HttpStatusCode::Unauthorized,
     kContentTypeHtmlCharset,
     kRestApiUsername,
     "invalid password",
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_config_invalid_auth",
     std::string(rest_api_basepath) + "/routes/ro/config",
     "/routes/{routeName}/config",
     HttpMethod::Get,
     HttpStatusCode::Unauthorized,
     kContentTypeHtmlCharset,
     kRestApiUsername,
     "invalid password",
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_health_invalid_auth",
     std::string(rest_api_basepath) + "/routes/ro/health",
     "/routes/{routeName}/health",
     HttpMethod::Get,
     HttpStatusCode::Unauthorized,
     kContentTypeHtmlCharset,
     kRestApiUsername,
     "invalid password",
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_destinations_invalid_auth",
     std::string(rest_api_basepath) + "/routes/ro/destinations",
     "/routes/{routeName}/destinations",
     HttpMethod::Get,
     HttpStatusCode::Unauthorized,
     kContentTypeHtmlCharset,
     kRestApiUsername,
     "invalid password",
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_blockedhosts_invalid_auth",
     std::string(rest_api_basepath) + "/routes/ro/blockedHosts",
     "/routes/{routeName}/blockedHosts",
     HttpMethod::Get,
     HttpStatusCode::Unauthorized,
     kContentTypeHtmlCharset,
     kRestApiUsername,
     "invalid password",
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_connections_invalid_auth",
     std::string(rest_api_basepath) + "/routes/ro/connections",
     "/routes/{routeName}/connections",
     HttpMethod::Get,
     HttpStatusCode::Unauthorized,
     kContentTypeHtmlCharset,
     kRestApiUsername,
     "invalid password",
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
};

INSTANTIATE_TEST_SUITE_P(
    ValidMethodsInvalidAuth, RestRoutingApiTest,
    ::testing::ValuesIn(rest_api_valid_methods_invalid_auth_params),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

// ****************************************************************************
// Request the resource(s) using unsupported methods with authentication enabled
// and valid credentials
// ****************************************************************************
static const RestApiTestParams rest_api_invalid_methods_params[]{
    {"routing_status_invalid_methods",
     std::string(rest_api_basepath) + "/routes/ro/status",
     "/routes/{routeName}/status",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Trace | HttpMethod::Options | HttpMethod::Head,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::get_json_method_not_allowed_verifiers(),
     kRoutingSwaggerPaths},
    {"routes_invalid_methods", std::string(rest_api_basepath) + "/routes",
     "/routes",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Trace | HttpMethod::Options | HttpMethod::Head,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::get_json_method_not_allowed_verifiers(),
     kRoutingSwaggerPaths},
    {"routes_config_invalid_methods",
     std::string(rest_api_basepath) + "/routes/ro/config",
     "/routes/{routeName}/config",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head | HttpMethod::Trace | HttpMethod::Options,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::get_json_method_not_allowed_verifiers(),
     kRoutingSwaggerPaths},
    {"routes_health_invalid_methods",
     std::string(rest_api_basepath) + "/routes/ro/health",
     "/routes/{routeName}/health",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head | HttpMethod::Trace | HttpMethod::Options,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::get_json_method_not_allowed_verifiers(),
     kRoutingSwaggerPaths},
    {"routes_destinations_invalid_methods",
     std::string(rest_api_basepath) + "/routes/ro/destinations",
     "/routes/{routeName}/destinations",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head | HttpMethod::Trace | HttpMethod::Options,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::get_json_method_not_allowed_verifiers(),
     kRoutingSwaggerPaths},
    {"routes_blockedhosts_invalid_methods",
     std::string(rest_api_basepath) + "/routes/ro/blockedHosts",
     "/routes/{routeName}/blockedHosts",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head | HttpMethod::Trace | HttpMethod::Options,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::get_json_method_not_allowed_verifiers(),
     kRoutingSwaggerPaths},
    {"routes_connections_invalid_methods",
     std::string(rest_api_basepath) + "/routes/ro/connections",
     "/routes/{routeName}/connections",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head | HttpMethod::Trace | HttpMethod::Options,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::get_json_method_not_allowed_verifiers(),
     kRoutingSwaggerPaths},
};

INSTANTIATE_TEST_SUITE_P(
    InvalidMethods, RestRoutingApiTest,
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
TEST_F(RestRoutingApiTest, routing_api_no_auth) {
  const std::string userfile = create_password_file();
  auto config_sections = get_restapi_config("rest_routing", userfile,
                                            /*request_authentication=*/false);

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router =
      launch_router({"-c", conf_file}, EXIT_FAILURE, true, false, -1s);

  check_exit_code(router, EXIT_FAILURE, 10s);

  const std::string router_output = router.get_logfile_content();
  EXPECT_THAT(
      router_output,
      ::testing::HasSubstr("  init 'rest_routing' failed: option "
                           "require_realm in [rest_routing] is required"));
}

/**
 * @test Enable authentication for the plugin in question. Reference a realm
 * that does not exist in the configuration file.
 */
TEST_F(RestRoutingApiTest, invalid_realm) {
  const std::string userfile = create_password_file();
  auto config_sections =
      get_restapi_config("rest_routing", userfile,
                         /*request_authentication=*/true, "invalidrealm");

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router =
      launch_router({"-c", conf_file}, EXIT_FAILURE, true, false, -1s);

  check_exit_code(router, EXIT_FAILURE, 10s);

  const std::string router_output = router.get_logfile_content();
  EXPECT_THAT(
      router_output,
      ::testing::HasSubstr(
          "Configuration error: The option 'require_realm=invalidrealm' "
          "in [rest_routing] does not match any http_auth_realm."))
      << router_output;
}

/**
 * @test Start router with the REST routing API plugin [rest_routing] and
 * [http_plugin] enabled but not the [rest_api] plugin.
 */
TEST_F(RestRoutingApiTest, routing_api_no_rest_api_works) {
  const std::string userfile = create_password_file();
  auto config_sections = get_restapi_config("rest_routing", userfile,
                                            /*request_authentication=*/true);

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  launch_router({"-c", conf_file}, EXIT_SUCCESS);
}

/**
 * @test Add [rest_routing] twice to the configuration file. Start router.
 * Expect router to fail providing an error about the duplicate section.
 *
 */
TEST_F(RestRoutingApiTest, rest_routing_section_twice) {
  const std::string userfile = create_password_file();
  auto config_sections = get_restapi_config("rest_routing", userfile,
                                            /*request_authentication=*/true);

  // force [rest_routing] twice in the config
  config_sections.push_back(
      mysql_harness::ConfigBuilder::build_section("rest_routing", {}));

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router =
      launch_router({"-c", conf_file}, EXIT_FAILURE, true, false, -1s);

  check_exit_code(router, EXIT_FAILURE, 10s);

  const std::string router_output = router.get_full_output();
  EXPECT_THAT(router_output,
              ::testing::HasSubstr(
                  "Configuration error: Section 'rest_routing' already exists"))
      << router_output;
}

/**
 * @test Enable [rest_routing] using a section key such as [rest_routing:A].
 * Start router. Expect router to fail providing an error about the use of an
 * unsupported section key.
 */
TEST_F(RestRoutingApiTest, rest_routing_section_has_key) {
  const std::string userfile = create_password_file();
  auto config_sections = get_restapi_config("rest_routing:A", userfile,
                                            /*request_authentication=*/true);

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto &router =
      launch_router({"-c", conf_file}, EXIT_FAILURE, true, false, -1s);

  check_exit_code(router, EXIT_FAILURE, 10s);

  const std::string router_output = router.get_logfile_content();
  EXPECT_THAT(router_output, ::testing::HasSubstr(
                                 "  init 'rest_routing' failed: [rest_routing] "
                                 "section does not expect a key, found 'A'"))
      << router_output;
}

static std::string get_server_addr_list(const std::vector<uint16_t> &ports) {
  std::string result;
  bool use_comma = false;

  for (const auto &port : ports) {
    if (use_comma) {
      result += ",";
    } else {
      use_comma = true;
    }
    result += "mysql://localhost:" + std::to_string(port);
  }

  return result;
}

class RestRoutingApiTestCluster : public RestRoutingApiTest {};

/**
 * @test check /routes/
 *
 * - start router with rest_routing module loaded, with metadata_cache
 * - and mock inndodb cluster
 */
TEST_P(RestRoutingApiTestCluster, ensure_openapi_cluster) {
  const std::string http_hostname = "127.0.0.1";
  const std::string http_uri = GetParam().uri;
  TempDirectory temp_test_dir;

  SCOPED_TRACE("// start the cluster with 1 RW and 2 RO nodes");
  std::vector<ProcessWrapper *> nodes;
  std::vector<uint16_t> node_classic_ports;
  uint16_t first_node_http_port{0};
  const std::string json_metadata =
      get_data_dir().join("metadata_dynamic_nodes.js").str();
  for (size_t i = 0; i < 3; ++i) {
    node_classic_ports.push_back(port_pool_.get_next_available());
    if (i == 0) first_node_http_port = port_pool_.get_next_available();

    nodes.push_back(&launch_mysql_server_mock(
        json_metadata, node_classic_ports[i], EXIT_SUCCESS, false,
        i == 0 ? first_node_http_port : 0));
  }

  ASSERT_TRUE(MockServerRestClient(first_node_http_port)
                  .wait_for_rest_endpoint_ready());

  set_mock_metadata(first_node_http_port, "", node_classic_ports);

  SCOPED_TRACE("// start the router with rest_routing enabled");
  for (size_t i = 0; i < 2; ++i) {
    routing_ports_.push_back(port_pool_.get_next_available());
  }
  const std::string userfile = create_password_file();
  const std::vector<std::string> route_names{"cluster_rw", "cluster_ro"};

  auto config_sections = get_restapi_config("rest_routing", userfile,
                                            GetParam().request_authentication);

  size_t i = 0;
  for (const auto &route_name : route_names) {
    const std::string role = (i == 0) ? "PRIMARY" : "SECONDARY";
    const std::string destinations =
        "metadata-cache://test/default?role=" + role;
    config_sections.push_back(mysql_harness::ConfigBuilder::build_section(
        "routing:"s + route_name,
        {
            {"bind_port", std::to_string(routing_ports_[i])},
            {"bind_address", "127.0.0.1"},
            {"destinations", destinations},
            {"routing_strategy", "round-robin"},
            {"client_connect_timeout", "60"},
            {"connect_timeout", "70"},
            {"max_connect_errors", "3"},
            {"max_connections", "1000"},
        }));
    ++i;
  }

  const std::string keyring_username = "mysql_router1_user";
  config_sections.push_back(mysql_harness::ConfigBuilder::build_section(
      "metadata_cache:test", {
                                 {"router_id", "3"},
                                 {"user", keyring_username},
                                 {"metadata_cluster", "test"},
                                 {"bootstrap_server_addresses",
                                  get_server_addr_list(node_classic_ports)},
                             }));

  auto default_section = get_DEFAULT_defaults();
  init_keyring(default_section, conf_dir_.name());

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, ""),
      &default_section)};

  ProcessWrapper &http_server =
      launch_router({"-c", conf_file}, EXIT_SUCCESS, true, false, -1s);

  // wait for both (rw and ro) routes being available
  for (size_t i = 0; i < 2; ++i) {
    ASSERT_TRUE(wait_route_ready(std::chrono::milliseconds(5000),
                                 route_names[i], http_port_, "127.0.0.1",
                                 kRestApiUsername, kRestApiPassword));
  }

  // make 1 connection to route "rw"
  mysqlrouter::MySQLSession client_ro_1;
  EXPECT_NO_THROW(client_ro_1.connect("127.0.0.1", routing_ports_[0], "root",
                                      "fake-pass", "", ""));

  // make 2 connection to route "ro"
  mysqlrouter::MySQLSession client_rw_1;
  EXPECT_NO_THROW(client_rw_1.connect("127.0.0.1", routing_ports_[1], "root",
                                      "fake-pass", "", ""));
  mysqlrouter::MySQLSession client_rw_2;
  EXPECT_NO_THROW(client_rw_2.connect("127.0.0.1", routing_ports_[1], "root",
                                      "fake-pass", "", ""));

  ASSERT_NO_FATAL_FAILURE(
      fetch_and_validate_schema_and_resource(GetParam(), http_server));
}

static const RestApiTestParams rest_api_valid_methods_params_cluster[]{
    {"routing_status", std::string(rest_api_basepath) + "/routing/status",
     "/routing/status", HttpMethod::Get, HttpStatusCode::Ok, kContentTypeJson,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_status_fields(
         /*expected_max_total_connections=*/512,
         /*expected_current_total_connections=*/2 + 1),
     kRoutingSwaggerPaths},
    {"routing_routes_rw_status",
     std::string(rest_api_basepath) + "/routes/cluster_rw/status",
     "/routes/{routeName}/status", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_routes_status_fields(/*expected_active_connections=*/1,
                                       /*expected_total_connections=*/1,
                                       /*expected_blocked_hosts*=*/0),
     kRoutingSwaggerPaths},

    {"routing_routes_ro_status",
     std::string(rest_api_basepath) + "/routes/cluster_ro/status",
     "/routes/{routeName}/status", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_routes_status_fields(/*expected_active_connections=*/2,
                                       /*expected_total_connections=*/2,
                                       /*expected_blocked_hosts*=*/0),
     kRoutingSwaggerPaths},

    {"cluster_routes",
     std::string(rest_api_basepath) + "/routes",
     "/routes",
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
            ASSERT_EQ(value->GetArray().Size(), 2);
          }},
         {"/items/0/name",
          [](const JsonValue *value) {
            ASSERT_TRUE(value != nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "cluster_ro");
          }},
         {"/items/1/name",
          [](const JsonValue *value) {
            ASSERT_TRUE(value != nullptr);
            ASSERT_TRUE(value->IsString());
            ASSERT_STREQ(value->GetString(), "cluster_rw");
          }},
     },
     kRoutingSwaggerPaths},
    {"routes_config_cluster_rw",
     std::string(rest_api_basepath) + "/routes/cluster_rw/config",
     "/routes/{routeName}/config", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_config_fields(),
     kRoutingSwaggerPaths},
    {"routes_config_cluster_ro",
     std::string(rest_api_basepath) + "/routes/cluster_ro/config",
     "/routes/{routeName}/config", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_config_fields(),
     kRoutingSwaggerPaths},
    {"routes_health_cluster_rw",
     std::string(rest_api_basepath) + "/routes/cluster_rw/health",
     "/routes/{routeName}/health", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_health_fields(true),
     kRoutingSwaggerPaths},
    {"routes_health_cluster_ro",
     std::string(rest_api_basepath) + "/routes/cluster_ro/health",
     "/routes/{routeName}/health", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_health_fields(true),
     kRoutingSwaggerPaths},
    {"routes_destinations_cluster_rw",
     std::string(rest_api_basepath) + "/routes/cluster_rw/destinations",
     "/routes/{routeName}/destinations", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_destinations_fields(1),
     kRoutingSwaggerPaths},
    {"routes_destinations_cluster_ro",
     std::string(rest_api_basepath) + "/routes/cluster_ro/destinations",
     "/routes/{routeName}/destinations", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true, get_expected_destinations_fields(2),
     kRoutingSwaggerPaths},
    {"routes_blockedhosts_cluster_rw",
     std::string(rest_api_basepath) + "/routes/cluster_rw/blockedHosts",
     "/routes/{routeName}/blockedHosts", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_blocked_hosts_fields(/*expected_blocked_hosts=*/0),
     kRoutingSwaggerPaths},
    {"routes_blockedhosts_cluster_ro",
     std::string(rest_api_basepath) + "/routes/cluster_ro/blockedHosts",
     "/routes/{routeName}/blockedHosts", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_blocked_hosts_fields(/*expected_blocked_hosts=*/0),
     kRoutingSwaggerPaths},
    {"routes_connections_cluster_rw",
     std::string(rest_api_basepath) + "/routes/cluster_rw/connections",
     "/routes/{routeName}/connections", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_connections_fields_fields(/*expected_connection_qty=*/1),
     kRoutingSwaggerPaths},
    {"routes_connections_cluster_ro",
     std::string(rest_api_basepath) + "/routes/cluster_ro/connections",
     "/routes/{routeName}/connections", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_connections_fields_fields(/*expected_connection_qty=*/2),
     kRoutingSwaggerPaths},
};

INSTANTIATE_TEST_SUITE_P(
    ValidMethodsCluster, RestRoutingApiTestCluster,
    ::testing::ValuesIn(rest_api_valid_methods_params_cluster),
    [](const ::testing::TestParamInfo<RestApiTestParams> &info) {
      return info.param.test_name;
    });

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
