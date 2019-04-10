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

#include <gmock/gmock.h>
#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif
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

Path g_origin_path;

class RestRoutingApiTest
    : public RestApiComponentTest,
      public ::testing::Test,
      public ::testing::WithParamInterface<RestApiTestParams> {
 protected:
  RestRoutingApiTest() : mock_port_{port_pool_.get_next_available()} {
    for (size_t i = 0; i < kRoutesQty; ++i) {
      routing_ports_.push_back(port_pool_.get_next_available());
    }
    set_origin(g_origin_path);
    RestApiComponentTest::init();
  }

  void TearDown() {
    process_manager_.shutdown_all();
    process_manager_.ensure_clean_exit();
  }

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

  ProcessManager process_manager_;

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

  const std::string userfile = create_password_file();

  const std::vector<std::string> route_names{"", "_", "123", "Aaz", "ro"};

  auto config_sections = get_restapi_config("rest_routing", userfile,
                                            GetParam().request_authentication);
  config_sections.push_back(ConfigBuilder::build_section("rest_api", {}));
  size_t i = 0;
  for (const auto &route_name : route_names) {
    // let's make "_" route a metadata cache one, all other are static
    const std::string destinations =
        (route_name == "_") ? "metadata-cache://test/default?role=PRIMARY"
                            : "127.0.0.1:" + std::to_string(mock_port_);
    config_sections.push_back(ConfigBuilder::build_section(
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

  // create a "dead" metada-cache referenced by the routing "_" to check
  // route/health isActive == 0
  const std::string keyring_username = "mysql_router1_user";
  config_sections.push_back(ConfigBuilder::build_section(
      "metadata_cache:test", {
                                 {"router_id", "3"},
                                 {"user", keyring_username},
                                 {"metadata_cluster", "test"},
                                 //"ttl", "0.5"
                             }));

  std::map<std::string, std::string> default_section = get_DEFAULT_defaults();
  RouterComponentTest::init_keyring(default_section, conf_dir_.name());

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"),
      &default_section)};

  CommandHandle &http_server =
      process_manager_.add(launch_router({"-c", conf_file}));

  // doesn't really matter which file we use here, we are not going to do any
  // queries
  const std::string json_stmts =
      get_data_dir().join("bootstrap_big_data.js").str();

  SCOPED_TRACE("// launch the server mock");
  auto &server_mock = process_manager_.add(
      launch_mysql_server_mock(json_stmts, mock_port_, false));

  ASSERT_TRUE(wait_for_port_ready(mock_port_, 5000))
      << server_mock.get_full_output();
  // wait for route being available if we expect it to be and plan to do some
  // connections to it (which are routes: "ro" and "Aaz")
  for (size_t i = 3; i < kRoutesQty; ++i) {
    ASSERT_TRUE(wait_route_ready(std::chrono::milliseconds(5000),
                                 route_names[i], http_port_, "127.0.0.1",
                                 kRestApiUsername, kRestApiPassword))
        << http_server.get_full_output() << "\n"
        << get_router_log_output();
  }

  // make 3 connections to route "ro"
  mysqlrouter::MySQLSession client_ro_1;
  EXPECT_NO_THROW(client_ro_1.connect("127.0.0.1", routing_ports_[4],
                                      "username", "password", "", ""));
  mysqlrouter::MySQLSession client_ro_2;
  EXPECT_NO_THROW(client_ro_2.connect("127.0.0.1", routing_ports_[4],
                                      "username", "password", "", ""));
  mysqlrouter::MySQLSession client_ro_3;
  EXPECT_NO_THROW(client_ro_3.connect("127.0.0.1", routing_ports_[4],
                                      "username", "password", "", ""));

  // make 1 connection to route "Aaz"
  mysqlrouter::MySQLSession client_Aaz_1;
  EXPECT_NO_THROW(client_Aaz_1.connect("127.0.0.1", routing_ports_[3],
                                       "username", "password", "", ""));

  // call wait_port_ready a few times on "123" to trigger blocked client
  // on that route (we set max_connect_errors to 2)
  for (size_t i = 0; i < 3; ++i) {
    ASSERT_TRUE(wait_for_port_ready(routing_ports_[2], 500))
        << http_server.get_full_output() << "\n"
        << get_router_log_output();
  }

  EXPECT_NO_FATAL_FAILURE(
      fetch_and_validate_schema_and_resource(GetParam(), http_server));
}

static const std::vector<
    std::pair<std::string, RestApiTestParams::value_check_func>>
get_expected_status_fields(const int expected_active_connections,
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

static const std::vector<
    std::pair<std::string, RestApiTestParams::value_check_func>>
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

static const std::vector<
    std::pair<std::string, RestApiTestParams::value_check_func>>
get_expected_health_fields(const bool expected_alive) {
  return {
      {"/isAlive",
       [=](const JsonValue *value) {
         ASSERT_TRUE(value != nullptr);
         ASSERT_EQ(value->GetBool(), expected_alive);
       }},
  };
}

static const std::vector<
    std::pair<std::string, RestApiTestParams::value_check_func>>
get_expected_destinations_fields(int expected_destinations_num) {
  std::vector<std::pair<std::string, RestApiTestParams::value_check_func>>
      result{
          {"/items",
           [=](const JsonValue *value) {
             ASSERT_NE(value, nullptr);
             ASSERT_TRUE(value->IsArray());
             ASSERT_EQ(value->GetArray().Size(), expected_destinations_num);
           }},
      };

  for (int i = 0; i < expected_destinations_num; ++i) {
    result.push_back({"/items/0/address", [](const JsonValue *value) {
                        ASSERT_TRUE(value != nullptr);
                        ASSERT_TRUE(value->IsString());
                        ASSERT_STREQ(value->GetString(), "127.0.0.1");
                      }});
    result.push_back({"/items/0/port", [](const JsonValue *value) {
                        ASSERT_NE(value, nullptr);
                        ASSERT_GT(value->GetInt(), 0);
                      }});
  }

  return result;
}

static const std::vector<
    std::pair<std::string, RestApiTestParams::value_check_func>>
get_expected_blocked_hosts_fields(const int expected_blocked_hosts) {
  std::vector<std::pair<std::string, RestApiTestParams::value_check_func>>
      result{{"/items", [=](const JsonValue *value) {
                ASSERT_NE(value, nullptr);
                ASSERT_TRUE(value->IsArray());
                ASSERT_EQ(value->GetArray().Size(), expected_blocked_hosts);
              }}};

  for (int i = 0; i < expected_blocked_hosts; ++i) {
    result.push_back(
        {"/items/" + std::to_string(i), [](const JsonValue *value) {
           ASSERT_NE(value, nullptr);
           ASSERT_THAT(value->GetString(), ::testing::StartsWith("127.0.0.1"));
         }});
  }

  return result;
}

static const std::vector<
    std::pair<std::string, RestApiTestParams::value_check_func>>
get_expected_connections_fields_fields(const int expected_connection_qty) {
  std::vector<std::pair<std::string, RestApiTestParams::value_check_func>>
      result{{"/items", [=](const JsonValue *value) {
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
    result.push_back({"/items/" + std::to_string(i) + "/bytesToServer",
                      [](const JsonValue *value) {
                        ASSERT_NE(value, nullptr);
                        ASSERT_GT(value->GetUint64(), 0);
                      }});

    result.push_back({"/items/" + std::to_string(i) + "/sourceAddress",
                      [](const JsonValue *value) {
                        ASSERT_NE(value, nullptr);
                        ASSERT_TRUE(value->IsString());
                        ASSERT_THAT(value->GetString(),
                                    ::testing::StartsWith("127.0.0.1"));
                      }});

    result.push_back({"/items/" + std::to_string(i) + "/destinationAddress",
                      [](const JsonValue *value) {
                        ASSERT_NE(value, nullptr);
                        ASSERT_TRUE(value->IsString());
                        ASSERT_THAT(value->GetString(),
                                    ::testing::StartsWith("127.0.0.1"));
                      }});

    result.push_back(
        {"/items/" + std::to_string(i) + "/timeConnectedToServer",
         [](const JsonValue *value) {
           ASSERT_NE(value, nullptr);
           ASSERT_TRUE(value->IsString());

           ASSERT_TRUE(pattern_found(value->GetString(), kTimestampPattern))
               << value->GetString();
         }});
  }

  return result;
}

// ****************************************************************************
// Request the resource(s) using supported methods with authentication enabled
// and valid credentials
// ****************************************************************************

static const RestApiTestParams rest_api_valid_methods[]{
    {"routing_status_ro", std::string(rest_api_basepath) + "/routes/ro/status",
     "/routes/{routeName}/status", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_status_fields(/*expected_active_connections=*/3,
                                /*expected_total_connections=*/3,
                                /*expected_blocked_hosts*=*/0),
     kRoutingSwaggerPaths},
    {"routing_status__", std::string(rest_api_basepath) + "/routes/_/status",
     "/routes/{routeName}/status", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_status_fields(/*expected_active_connections=*/0,
                                /*expected_total_connections=*/0,
                                /*expected_blocked_hosts*=*/0),
     kRoutingSwaggerPaths},
    {"routing_status_Aaz",
     std::string(rest_api_basepath) + "/routes/Aaz/status",
     "/routes/{routeName}/status", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_status_fields(/*expected_active_connections=*/1,
                                /*expected_total_connections=*/1,
                                /*expected_blocked_hosts*=*/0),
     kRoutingSwaggerPaths},
    {"routing_status_123",
     std::string(rest_api_basepath) + "/routes/123/status",
     "/routes/{routeName}/status", HttpMethod::Get, HttpStatusCode::Ok,
     kContentTypeJson, kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_status_fields(/*expected_active_connections=*/0,
                                /*expected_total_connections=*/3,
                                /*expected_blocked_hosts*=*/1),
     kRoutingSwaggerPaths},
    {"routing_status_nonexistent",
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
    {"routes_connections_nonexistent",
     std::string(rest_api_basepath) + "/routes/nonexistent/connections",
     "/routes/{routeName}/connections", HttpMethod::Get,
     HttpStatusCode::NotFound, kContentTypeJson, kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     get_expected_connections_fields_fields(/*expected_connection_qty=*/0),
     kRoutingSwaggerPaths},
};

INSTANTIATE_TEST_CASE_P(
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

INSTANTIATE_TEST_CASE_P(
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
         HttpMethod::Head,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::kProblemJsonMethodNotAllowed, kRoutingSwaggerPaths},
    {"routes_invalid_methods", std::string(rest_api_basepath) + "/routes",
     "/routes",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::kProblemJsonMethodNotAllowed, kRoutingSwaggerPaths},
    {"routes_config_invalid_methods",
     std::string(rest_api_basepath) + "/routes/ro/config",
     "/routes/{routeName}/config",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::kProblemJsonMethodNotAllowed, kRoutingSwaggerPaths},
    {"routes_health_invalid_methods",
     std::string(rest_api_basepath) + "/routes/ro/health",
     "/routes/{routeName}/health",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::kProblemJsonMethodNotAllowed, kRoutingSwaggerPaths},
    {"routes_destinations_invalid_methods",
     std::string(rest_api_basepath) + "/routes/ro/destinations",
     "/routes/{routeName}/destinations",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::kProblemJsonMethodNotAllowed, kRoutingSwaggerPaths},
    {"routes_blockedhosts_invalid_methods",
     std::string(rest_api_basepath) + "/routes/ro/blockedHosts",
     "/routes/{routeName}/blockedHosts",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::kProblemJsonMethodNotAllowed, kRoutingSwaggerPaths},
    {"routes_connections_invalid_methods",
     std::string(rest_api_basepath) + "/routes/ro/connections",
     "/routes/{routeName}/connections",
     HttpMethod::Post | HttpMethod::Delete | HttpMethod::Patch |
         HttpMethod::Head,
     HttpStatusCode::MethodNotAllowed, kContentTypeJsonProblem,
     kRestApiUsername, kRestApiPassword,
     /*request_authentication =*/true,
     RestApiComponentTest::kProblemJsonMethodNotAllowed, kRoutingSwaggerPaths},

    // OPTIONS, CONNECT and TRACE are disabled in libevent via
    // evhttp_set_allowed_methods() and return 501, which is ok-ish.
    {"routing_status_unimplemented_methods",
     std::string(rest_api_basepath) + "/routes/ro/status",
     "/routes/{routeName}/status",
     HttpMethod::Trace | HttpMethod::Options | HttpMethod::Connect,
     HttpStatusCode::NotImplemented,
     kContentTypeHtml,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_unimplemented_methods",
     std::string(rest_api_basepath) + "/routes",
     "/routes",
     HttpMethod::Trace | HttpMethod::Options | HttpMethod::Connect,
     HttpStatusCode::NotImplemented,
     kContentTypeHtml,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_config_unimplemented_methods",
     std::string(rest_api_basepath) + "/routes/ro/config",
     "/routes/{routeName}/config",
     HttpMethod::Trace | HttpMethod::Options | HttpMethod::Connect,
     HttpStatusCode::NotImplemented,
     kContentTypeHtml,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_health_unimplemented_methods",
     std::string(rest_api_basepath) + "/routes/ro/health",
     "/routes/{routeName}/health",
     HttpMethod::Trace | HttpMethod::Options | HttpMethod::Connect,
     HttpStatusCode::NotImplemented,
     kContentTypeHtml,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_destinations_unimplemented_methods",
     std::string(rest_api_basepath) + "/routes/ro/destinations",
     "/routes/{routeName}/destinations",
     HttpMethod::Trace | HttpMethod::Options | HttpMethod::Connect,
     HttpStatusCode::NotImplemented,
     kContentTypeHtml,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_blockedhosts_unimplemented_methods",
     std::string(rest_api_basepath) + "/routes/ro/blockedHosts",
     "/routes/{routeName}/blockedHosts",
     HttpMethod::Trace | HttpMethod::Options | HttpMethod::Connect,
     HttpStatusCode::NotImplemented,
     kContentTypeHtml,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
    {"routes_connections_unimplemented_methods",
     std::string(rest_api_basepath) + "/routes/ro/connections",
     "/routes/{routeName}/connections",
     HttpMethod::Trace | HttpMethod::Options | HttpMethod::Connect,
     HttpStatusCode::NotImplemented,
     kContentTypeHtml,
     kRestApiUsername,
     kRestApiPassword,
     /*request_authentication =*/true,
     {},
     kRoutingSwaggerPaths},
};

INSTANTIATE_TEST_CASE_P(
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

  // [rest_api] is always required
  config_sections.push_back(ConfigBuilder::build_section("rest_api", {}));

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto router = launch_router({"-c", conf_file});

  const unsigned wait_for_process_exit_timeout{10000};
  EXPECT_EQ(router.wait_for_exit(wait_for_process_exit_timeout), 1);

  const std::string router_output = get_router_log_output();
  EXPECT_NE(router_output.find("plugin 'rest_routing' init failed: option "
                               "require_realm in [rest_routing] is required"),
            router_output.npos)
      << router_output;
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

  // [rest_api] is always required
  config_sections.push_back(ConfigBuilder::build_section("rest_api", {}));

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto router = launch_router({"-c", conf_file});

  const unsigned wait_for_process_exit_timeout{10000};
  EXPECT_EQ(router.wait_for_exit(wait_for_process_exit_timeout), 1);

  const std::string router_output = get_router_log_output();
  EXPECT_NE(
      router_output.find("Configuration error: unknown authentication "
                         "realm for [rest_routing] '': invalidrealm, known "
                         "realm(s): somerealm"),
      router_output.npos)
      << router_output;
}

/**
 * @test Start router with the REST routing API plugin [rest_routing] and
 * [http_plugin] enabled but not the [rest_api] plugin.
 */
TEST_F(RestRoutingApiTest, routing_api_no_rest_api) {
  const std::string userfile = create_password_file();
  auto config_sections = get_restapi_config("rest_routing", userfile,
                                            /*request_authentication=*/false);

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto router = launch_router({"-c", conf_file});

  const unsigned wait_for_process_exit_timeout{10000};
  EXPECT_EQ(router.wait_for_exit(wait_for_process_exit_timeout), 1);

  const std::string router_output = router.get_full_output();
  EXPECT_NE(router_output.find("Plugin 'rest_routing' needs plugin "
                               "'rest_api' which is missing in the "
                               "configuration"),
            router_output.npos)
      << router_output;
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

  // [rest_api] is always required
  config_sections.push_back(ConfigBuilder::build_section("rest_api", {}));

  // force [rest_routing] twice in the config
  config_sections.push_back(ConfigBuilder::build_section("rest_routing", {}));

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto router = launch_router({"-c", conf_file});

  const unsigned wait_for_process_exit_timeout{10000};
  EXPECT_EQ(router.wait_for_exit(wait_for_process_exit_timeout), 1);

  const std::string router_output = router.get_full_output();
  EXPECT_NE(router_output.find(
                "Configuration error: Section 'rest_routing' already exists"),
            router_output.npos)
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

  // [rest_api] is always required
  config_sections.push_back(ConfigBuilder::build_section("rest_api", {}));

  const std::string conf_file{create_config_file(
      conf_dir_.name(), mysql_harness::join(config_sections, "\n"))};
  auto router = launch_router({"-c", conf_file});

  const unsigned wait_for_process_exit_timeout{10000};
  EXPECT_EQ(router.wait_for_exit(wait_for_process_exit_timeout), 1);

  const std::string router_output = get_router_log_output();
  EXPECT_NE(
      router_output.find("plugin 'rest_routing' init failed: [rest_routing] "
                         "section does not expect a key, found 'A'"),
      router_output.npos)
      << router_output;
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
