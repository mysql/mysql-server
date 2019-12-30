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

/**
 * REST API for the routing plugin.
 */

#include <array>
#include <string>

#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/utility/string.h"  // ::join()

#include "mysqlrouter/plugin_config.h"

#include "mysqlrouter/http_server_component.h"
#include "mysqlrouter/rest_api_component.h"
#include "mysqlrouter/rest_routing_export.h"

#include "rest_routing_blocked_hosts.h"
#include "rest_routing_config.h"
#include "rest_routing_connections.h"
#include "rest_routing_destinations.h"
#include "rest_routing_health.h"
#include "rest_routing_list.h"
#include "rest_routing_status.h"
IMPORT_LOG_FUNCTIONS()

static const char kSectionName[]{"rest_routing"};
static const char kRequireRealm[]{"require_realm"};

// one shared setting
std::string require_realm_routing;

class RestRoutingPluginConfig : public mysqlrouter::BasePluginConfig {
 public:
  std::string require_realm;

  explicit RestRoutingPluginConfig(const mysql_harness::ConfigSection *section)
      : mysqlrouter::BasePluginConfig(section),
        require_realm(get_option_string(section, kRequireRealm)) {}

  std::string get_default(const std::string & /* option */) const override {
    return {};
  }

  bool is_required(const std::string &option) const override {
    if (option == kRequireRealm) return true;
    return false;
  }
};

static void init(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (nullptr == info->config) {
    return;
  }

  try {
    std::set<std::string> known_realms;
    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name == "http_auth_realm") {
        known_realms.emplace(section->key);
      }
    }
    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name != kSectionName) {
        continue;
      }

      if (!section->key.empty()) {
        log_error("[%s] section does not expect a key, found '%s'",
                  kSectionName, section->key.c_str());
        set_error(env, mysql_harness::kConfigInvalidArgument,
                  "[%s] section does not expect a key, found '%s'",
                  kSectionName, section->key.c_str());
        return;
      }

      RestRoutingPluginConfig config{section};

      if (!config.require_realm.empty() &&
          (known_realms.find(config.require_realm) == known_realms.end())) {
        throw std::invalid_argument(
            "unknown authentication realm for [" + std::string(kSectionName) +
            "] '" + section->key + "': " + config.require_realm +
            ", known realm(s): " + mysql_harness::join(known_realms, ","));
      }

      require_realm_routing = config.require_realm;
    }
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

using JsonPointer = RestApiComponent::JsonPointer;
using JsonValue = RestApiComponent::JsonValue;

#define STR(s) \
  { s, strlen(s), rapidjson::kPointerInvalidIndex }
static const std::array<JsonPointer::Token, 2> route_name_param_tokens{
    {STR("parameters"), STR("routeNameParam")}};

static const std::array<JsonPointer::Token, 2> routes_list_def_tokens{
    {STR("definitions"), STR("RouteList")}};

static const std::array<JsonPointer::Token, 2>
    routes_destination_list_def_tokens{
        {STR("definitions"), STR("RouteDestinationList")}};

static const std::array<JsonPointer::Token, 2>
    routes_destination_summary_def_tokens{
        {STR("definitions"), STR("RouteDestinationSummary")}};

static const std::array<JsonPointer::Token, 2>
    routes_blockedhost_list_def_tokens{
        {STR("definitions"), STR("RouteBlockedHostList")}};

static const std::array<JsonPointer::Token, 2>
    routes_blockedhost_summary_def_tokens{
        {STR("definitions"), STR("RouteBlockedHostSummary")}};

static const std::array<JsonPointer::Token, 2>
    routes_connection_list_def_tokens{
        {STR("definitions"), STR("RouteConnectionsList")}};

static const std::array<JsonPointer::Token, 2>
    routes_connection_summary_def_tokens{
        {STR("definitions"), STR("RouteConnectionsSummary")}};

static const std::array<JsonPointer::Token, 2> routes_summary_def_tokens{
    {STR("definitions"), STR("RouteSummary")}};

static const std::array<JsonPointer::Token, 2> routes_config_def_tokens{
    {STR("definitions"), STR("RouteConfig")}};

static const std::array<JsonPointer::Token, 2> routes_status_def_tokens{
    {STR("definitions"), STR("RouteStatus")}};

static const std::array<JsonPointer::Token, 2> routes_health_def_tokens{
    {STR("definitions"), STR("RouteHealth")}};

static const std::array<JsonPointer::Token, 2> routes_status_path_tokens{
    {STR("paths"), STR("/routes/{routeName}/status")}};

static const std::array<JsonPointer::Token, 2> routes_config_path_tokens{
    {STR("paths"), STR("/routes/{routeName}/config")}};

static const std::array<JsonPointer::Token, 2> routes_health_path_tokens{
    {STR("paths"), STR("/routes/{routeName}/health")}};

static const std::array<JsonPointer::Token, 2>
    routes_connection_list_path_tokens{
        {STR("paths"), STR("/routes/{routeName}/connections")}};

static const std::array<JsonPointer::Token, 2>
    routes_blockedhost_list_path_tokens{
        {STR("paths"), STR("/routes/{routeName}/blockedHosts")}};

static const std::array<JsonPointer::Token, 2>
    routes_destination_list_path_tokens{
        {STR("paths"), STR("/routes/{routeName}/destinations")}};

static const std::array<JsonPointer::Token, 2> routes_list_path_tokens{
    {STR("paths"), STR("/routes")}};

static const std::array<JsonPointer::Token, 2> tags_append_tokens{
    {STR("tags"), STR("-")}};

#undef STR

std::string json_pointer_stringfy(const JsonPointer &ptr) {
  rapidjson::StringBuffer sb;
  ptr.StringifyUriFragment(sb);
  return {sb.GetString(), sb.GetSize()};
}

static void spec_adder(RestApiComponent::JsonDocument &spec_doc) {
  auto &allocator = spec_doc.GetAllocator();

  {
    JsonPointer ptr(tags_append_tokens.data(), tags_append_tokens.size());

    ptr.Set(spec_doc,
            JsonValue(rapidjson::kObjectType)
                .AddMember("name", "routes", allocator)
                .AddMember("description", "Routes", allocator),
            allocator);
  }

  // /parameters/routeNameParam
  const RestApiComponent::JsonPointer route_name_param_ptr(
      route_name_param_tokens.data(), route_name_param_tokens.size());

  route_name_param_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("name", "routeName", allocator)
          .AddMember("in", "path", allocator)
          .AddMember("description", "name of a route", allocator)
          .AddMember("required", true, allocator)
          .AddMember("type", "string", allocator),
      allocator);

  std::string route_name_param_str =
      json_pointer_stringfy(route_name_param_ptr);

  // /definitions/RoutesHealth
  const RestApiComponent::JsonPointer routes_health_def_ptr(
      routes_health_def_tokens.data(), routes_health_def_tokens.size());

  routes_health_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember("isAlive",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "boolean", allocator),
                             allocator),
              allocator),
      allocator);

  std::string routes_health_def_ptr_str =
      json_pointer_stringfy(routes_health_def_ptr);

  // /definitions/RoutesStatus
  const RestApiComponent::JsonPointer routes_status_def_ptr(
      routes_status_def_tokens.data(), routes_status_def_tokens.size());

  routes_status_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember("activeConnections",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("totalConnections",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("blockedHosts",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator),
              allocator),
      allocator);

  std::string routes_status_def_ptr_str =
      json_pointer_stringfy(routes_status_def_ptr);

  // /definitions/RoutesConfig
  const RestApiComponent::JsonPointer routes_config_def_ptr(
      routes_config_def_tokens.data(), routes_config_def_tokens.size());

  routes_config_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember("bindAddress",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator)
                  .AddMember("bindPort",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("clientConnectTimeoutInMs",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("destinationConnectTimeoutInMs",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("maxActiveConnections",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("maxConnectErrors",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("protocol",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator)
                  // these are stubbed now
                  //                  .AddMember("destinationClusterName",
                  //                             JsonValue(rapidjson::kObjectType)
                  //                                 .AddMember("type",
                  //                                 "string", allocator),
                  //                             allocator)
                  //                  .AddMember("destinationReplicasetName",
                  //                             JsonValue(rapidjson::kObjectType)
                  //                                 .AddMember("type",
                  //                                 "string", allocator),
                  //                             allocator)
                  .AddMember("socket",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator)
                  .AddMember("routingStrategy",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator)
                  .AddMember("mode",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator),
              allocator),
      allocator);

  std::string routes_config_def_ptr_str =
      json_pointer_stringfy(routes_config_def_ptr);

  // /definitions/RoutesSummary
  const RestApiComponent::JsonPointer routes_summary_def_ptr(
      routes_summary_def_tokens.data(), routes_summary_def_tokens.size());

  routes_summary_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember("properties",
                     JsonValue(rapidjson::kObjectType)
                         .AddMember("name",
                                    JsonValue(rapidjson::kObjectType)
                                        .AddMember("type", "string", allocator),
                                    allocator),
                     allocator),
      allocator);

  std::string routes_summary_def_ptr_str =
      json_pointer_stringfy(routes_summary_def_ptr);

  // /definitions/RoutesList
  const RestApiComponent::JsonPointer routes_list_def_ptr(
      routes_list_def_tokens.data(), routes_list_def_tokens.size());

  routes_list_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember(
                      "items",
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "array", allocator)
                          .AddMember(
                              "items",
                              JsonValue(rapidjson::kObjectType)
                                  .AddMember(
                                      "$ref",
                                      JsonValue(
                                          routes_summary_def_ptr_str.data(),
                                          routes_summary_def_ptr_str.size(),
                                          allocator),
                                      allocator),
                              allocator),
                      allocator),
              allocator),
      allocator);

  std::string routes_list_def_ptr_str =
      json_pointer_stringfy(routes_list_def_ptr);

  // /definitions/RoutesDestinationSummary
  const RestApiComponent::JsonPointer routes_destination_summary_def_ptr(
      routes_destination_summary_def_tokens.data(),
      routes_destination_summary_def_tokens.size());

  routes_destination_summary_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember("properties",
                     JsonValue(rapidjson::kObjectType)
                         .AddMember("name",
                                    JsonValue(rapidjson::kObjectType)
                                        .AddMember("type", "string", allocator),
                                    allocator),
                     allocator),
      allocator);

  std::string routes_destination_summary_def_ptr_str =
      json_pointer_stringfy(routes_destination_summary_def_ptr);

  // /definitions/RoutesDestinationList
  const RestApiComponent::JsonPointer routes_destination_list_def_ptr(
      routes_destination_list_def_tokens.data(),
      routes_destination_list_def_tokens.size());

  routes_destination_list_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember(
                      "items",
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "array", allocator)
                          .AddMember(
                              "items",
                              JsonValue(rapidjson::kObjectType)
                                  .AddMember(
                                      "$ref",
                                      JsonValue(
                                          routes_destination_summary_def_ptr_str
                                              .data(),
                                          routes_destination_summary_def_ptr_str
                                              .size(),
                                          allocator),
                                      allocator),
                              allocator),
                      allocator),
              allocator),
      allocator);

  std::string routes_destination_list_def_ptr_str =
      json_pointer_stringfy(routes_destination_list_def_ptr);

  // /definitions/RoutesBlockedHostSummary
  const RestApiComponent::JsonPointer routes_blockedhost_summary_def_ptr(
      routes_blockedhost_summary_def_tokens.data(),
      routes_blockedhost_summary_def_tokens.size());

  routes_blockedhost_summary_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember("properties",
                     JsonValue(rapidjson::kObjectType)
                         .AddMember("name",
                                    JsonValue(rapidjson::kObjectType)
                                        .AddMember("type", "string", allocator),
                                    allocator),
                     allocator),
      allocator);

  std::string routes_blockedhost_summary_def_ptr_str =
      json_pointer_stringfy(routes_blockedhost_summary_def_ptr);

  // /definitions/RoutesBlockedHostList
  const RestApiComponent::JsonPointer routes_blockedhost_list_def_ptr(
      routes_blockedhost_list_def_tokens.data(),
      routes_blockedhost_list_def_tokens.size());

  routes_blockedhost_list_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember(
                      "items",
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "array", allocator)
                          .AddMember(
                              "items",
                              JsonValue(rapidjson::kObjectType)
                                  .AddMember(
                                      "$ref",
                                      JsonValue(
                                          routes_blockedhost_summary_def_ptr_str
                                              .data(),
                                          routes_blockedhost_summary_def_ptr_str
                                              .size(),
                                          allocator),
                                      allocator),
                              allocator),
                      allocator),
              allocator),
      allocator);

  std::string routes_blockedhost_list_def_ptr_str =
      json_pointer_stringfy(routes_blockedhost_list_def_ptr);

  // /definitions/RoutesConnectionSummary
  const RestApiComponent::JsonPointer routes_connection_summary_def_ptr(
      routes_connection_summary_def_tokens.data(),
      routes_connection_summary_def_tokens.size());

  routes_connection_summary_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember(RestRoutingConnections::kKeyTimeStarted,
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator)
                                 .AddMember("format", "date-time", allocator)
                                 .AddMember("description",
                                            "timepoint when connection to "
                                            "server was initiated",
                                            allocator),
                             allocator)
                  .AddMember(
                      RestRoutingConnections::kKeyTimeConnectedToServer,
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "string", allocator)
                          .AddMember("format", "date-time", allocator)
                          .AddMember(
                              "description",
                              "timepoint when connection to server succeeded",
                              allocator),
                      allocator)
                  .AddMember(RestRoutingConnections::kKeyTimeLastSentToServer,
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator)
                                 .AddMember("format", "date-time", allocator)
                                 .AddMember("description",
                                            "timepoint when there was last "
                                            "data sent from client to server",
                                            allocator),
                             allocator)
                  .AddMember(
                      RestRoutingConnections::kKeyTimeLastReceivedFromServer,
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "string", allocator)
                          .AddMember("format", "date-time", allocator)
                          .AddMember("description",
                                     "timepoint when there was last data sent "
                                     "from server to client",
                                     allocator),
                      allocator)
                  .AddMember(
                      RestRoutingConnections::kKeyBytesToServer,
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "integer", allocator)
                          .AddMember("description", "bytes sent to destination",
                                     allocator),
                      allocator)
                  .AddMember(RestRoutingConnections::kKeyBytesFromServer,
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator)
                                 .AddMember("description",
                                            "bytes received from destination",
                                            allocator),
                             allocator)
                  .AddMember(
                      RestRoutingConnections::kKeyDestinationAddress,
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "string", allocator)
                          .AddMember(
                              "description",
                              "address of the destination of the connection",
                              allocator),
                      allocator)
                  .AddMember(
                      RestRoutingConnections::kKeySourceAddress,
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "string", allocator)
                          .AddMember("description",
                                     "address of the source of the connection",
                                     allocator),
                      allocator),
              allocator),
      allocator);

  std::string routes_connection_summary_def_ptr_str =
      json_pointer_stringfy(routes_connection_summary_def_ptr);

  // /definitions/RoutesConnectionList
  const RestApiComponent::JsonPointer routes_connection_list_def_ptr(
      routes_connection_list_def_tokens.data(),
      routes_connection_list_def_tokens.size());

  routes_connection_list_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember(
                      "items",
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "array", allocator)
                          .AddMember(
                              "items",
                              JsonValue(rapidjson::kObjectType)
                                  .AddMember(
                                      "$ref",
                                      JsonValue(
                                          routes_connection_summary_def_ptr_str
                                              .data(),
                                          routes_connection_summary_def_ptr_str
                                              .size(),
                                          allocator),
                                      allocator),
                              allocator),
                      allocator),
              allocator),
      allocator);

  std::string routes_connection_list_def_ptr_str =
      json_pointer_stringfy(routes_connection_list_def_ptr);

  // /paths/routesConfig
  {
    JsonPointer ptr(routes_config_path_tokens.data(),
                    routes_config_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routes", allocator),
                               allocator)
                    .AddMember("description", "Get config of a route",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "config of a route", allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routes_config_def_ptr_str
                                                        .data(),
                                                    routes_config_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "route not found",
                                               allocator),
                                allocator),
                        allocator)
                //
                ,
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(route_name_param_str.data(),
                                                 route_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/routesStatus
  {
    JsonPointer ptr(routes_status_path_tokens.data(),
                    routes_status_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routes", allocator),
                               allocator)
                    .AddMember("description", "Get status of a route",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "status of a route", allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routes_status_def_ptr_str
                                                        .data(),
                                                    routes_status_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "route not found",
                                               allocator),
                                allocator),
                        allocator)
                //
                ,
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(route_name_param_str.data(),
                                                 route_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/RoutesHealth
  {
    JsonPointer ptr(routes_health_path_tokens.data(),
                    routes_health_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routes", allocator),
                               allocator)
                    .AddMember("description", "Get health of a route",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "health of a route", allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routes_health_def_ptr_str
                                                        .data(),
                                                    routes_health_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "route not found",
                                               allocator),
                                allocator),
                        allocator)
                //
                ,
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(route_name_param_str.data(),
                                                 route_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/RoutesDestinations
  {
    JsonPointer ptr(routes_destination_list_path_tokens.data(),
                    routes_destination_list_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routes", allocator),
                               allocator)
                    .AddMember("description", "Get destinations of a route",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "destinations of a route",
                                               allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routes_destination_list_def_ptr_str
                                                        .data(),
                                                    routes_destination_list_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "route not found",
                                               allocator),
                                allocator),
                        allocator)
                //
                ,
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(route_name_param_str.data(),
                                                 route_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/RoutesConnections
  {
    JsonPointer ptr(routes_connection_list_path_tokens.data(),
                    routes_connection_list_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routes", allocator),
                               allocator)
                    .AddMember("description", "Get connections of a route",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "connections of a route",
                                               allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routes_connection_list_def_ptr_str
                                                        .data(),
                                                    routes_connection_list_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "route not found",
                                               allocator),
                                allocator),
                        allocator)
                //
                ,
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(route_name_param_str.data(),
                                                 route_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/RoutesBlockedHosts
  {
    JsonPointer ptr(routes_blockedhost_list_path_tokens.data(),
                    routes_blockedhost_list_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routes", allocator),
                               allocator)
                    .AddMember("description",
                               "Get blocked host list for a route", allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "blocked host list for a route",
                                               allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routes_blockedhost_list_def_ptr_str
                                                        .data(),
                                                    routes_blockedhost_list_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator)
                            .AddMember(
                                "404",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description", "route not found",
                                               allocator),
                                allocator),
                        allocator)
                //
                ,
                allocator)
            .AddMember(
                "parameters",
                JsonValue(rapidjson::kArrayType)
                    .PushBack(
                        JsonValue(rapidjson::kObjectType)
                            .AddMember("$ref",
                                       JsonValue(route_name_param_str.data(),
                                                 route_name_param_str.size(),
                                                 allocator),
                                       allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/routes/
  {
    JsonPointer ptr(routes_list_path_tokens.data(),
                    routes_list_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("routes", allocator),
                               allocator)
                    .AddMember("description", "Get list of the routes",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "list of the routes", allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    routes_list_def_ptr_str
                                                        .data(),
                                                    routes_list_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator),
                        allocator),
                allocator),
        allocator);
  }
}

static void start(mysql_harness::PluginFuncEnv *env) {
  auto &rest_api_srv = RestApiComponent::get_instance();

  const bool spec_adder_executed = rest_api_srv.try_process_spec(spec_adder);

  std::array<RestApiComponentPath, 7> paths{{
      {rest_api_srv, RestRoutingList::path_regex,
       std::make_unique<RestRoutingList>(require_realm_routing)},
      {rest_api_srv, RestRoutingBlockedHosts::path_regex,
       std::make_unique<RestRoutingBlockedHosts>(require_realm_routing)},
      {rest_api_srv, RestRoutingDestinations::path_regex,
       std::make_unique<RestRoutingDestinations>(require_realm_routing)},
      {rest_api_srv, RestRoutingConfig::path_regex,
       std::make_unique<RestRoutingConfig>(require_realm_routing)},
      {rest_api_srv, RestRoutingStatus::path_regex,
       std::make_unique<RestRoutingStatus>(require_realm_routing)},
      {rest_api_srv, RestRoutingHealth::path_regex,
       std::make_unique<RestRoutingHealth>(require_realm_routing)},
      {rest_api_srv, RestRoutingConnections::path_regex,
       std::make_unique<RestRoutingConnections>(require_realm_routing)},
  }};

  wait_for_stop(env, 0);

  // in case rest_api never initialized, ensure the rest_api_component doesn't
  // have a callback to use
  if (!spec_adder_executed) rest_api_srv.remove_process_spec(spec_adder);
}

const char *rest_routing_plugin_requires[] = {
    // "routing",
    "rest_api",
};

extern "C" {
mysql_harness::Plugin REST_ROUTING_EXPORT harness_plugin_rest_routing = {
    mysql_harness::PLUGIN_ABI_VERSION,
    mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "REST_ROUTING",
    VERSION_NUMBER(0, 0, 1),
    sizeof(rest_routing_plugin_requires) /
        sizeof(rest_routing_plugin_requires[0]),
    rest_routing_plugin_requires,  // requires
    0,
    nullptr,  // conflicts
    init,     // init
    nullptr,  // deinit
    start,    // start
    nullptr,  // stop
};
}
