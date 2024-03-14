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

/**
 * REST API for the connection_pool plugin.
 */

#include <array>
#include <string>

#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/dynamic_config.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/plugin_config.h"
#include "mysql/harness/section_config_exposer.h"
#include "mysql/harness/utility/string.h"  // ::join()

#include "mysqlrouter/component/http_server_component.h"
#include "mysqlrouter/http_constants.h"
#include "mysqlrouter/rest_api_component.h"
#include "mysqlrouter/rest_connection_pool_export.h"

#include "rest_connection_pool_config.h"
#include "rest_connection_pool_list.h"
#include "rest_connection_pool_status.h"
IMPORT_LOG_FUNCTIONS()

using namespace std::string_literals;

using StringOption = mysql_harness::StringOption;

static constexpr const char kSectionName[]{"rest_connection_pool"};

static constexpr const char kRequireRealm[]{"require_realm"};

static constexpr const std::array supported_options{
    kRequireRealm,
};

// one shared setting
std::string require_realm_connection_pool;

#define GET_OPTION_CHECKED(option, section, name, value)                    \
  static_assert(mysql_harness::str_in_collection(supported_options, name)); \
  option = get_option(section, name, value);

class RestConnectionPoolPluginConfig : public mysql_harness::BasePluginConfig {
 public:
  std::string require_realm;

  explicit RestConnectionPoolPluginConfig(
      const mysql_harness::ConfigSection *section)
      : mysql_harness::BasePluginConfig(section) {
    GET_OPTION_CHECKED(require_realm, section, kRequireRealm, StringOption{});
  }

  std::string get_default(std::string_view /* option */) const override {
    return {};
  }

  bool is_required(std::string_view option) const override {
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

      RestConnectionPoolPluginConfig config{section};

      if (!config.require_realm.empty() &&
          (known_realms.find(config.require_realm) == known_realms.end())) {
        std::string section_name = section->name;
        if (!section->key.empty()) section_name += ":" + section->key;

        const std::string realm_msg =
            (known_realms.empty())
                ? "No [http_auth_realm:" + config.require_realm +
                      "] section defined."
                : "Known [http_auth_realm:<...>] section" +
                      (known_realms.size() > 1 ? "s"s : ""s) + ": " +
                      mysql_harness::join(known_realms, ", ");

        throw std::invalid_argument(
            "The option 'require_realm=" + config.require_realm + "' in [" +
            section_name + "] does not match any http_auth_realm. " +
            realm_msg);
      }

      require_realm_connection_pool = config.require_realm;
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

static JsonPointer::Token make_json_pointer_token(std::string_view token) {
  return {token.data(), token.size(), rapidjson::kPointerInvalidIndex};
}

static const std::array route_name_param_tokens{
    make_json_pointer_token("parameters"),
    make_json_pointer_token("connectionPoolNameParam"),
};

static const std::array connection_pool_list_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("ConnectionPoolList"),
};

static const std::array connection_pool_summary_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("ConnectionPoolSummary"),
};

static const std::array connection_pool_config_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("ConnectionPoolConfig"),
};

static const std::array connection_pool_status_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("ConnectionPoolStatus"),
};

static const std::array connection_pool_status_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/connection_pool/{connectionPoolName}/status"),
};

static const std::array connection_pool_config_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/connection_pool/{connectionPoolName}/config"),
};

static const std::array connection_pool_list_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/connection_pool"),
};

static const std::array tags_append_tokens{
    make_json_pointer_token("tags"),
    make_json_pointer_token("-"),
};

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
                .AddMember("name", "connectionpool", allocator)
                .AddMember("description", "Connection Pool", allocator),
            allocator);
  }

  // /parameters/poolNameParam
  const RestApiComponent::JsonPointer route_name_param_ptr(
      route_name_param_tokens.data(), route_name_param_tokens.size());

  route_name_param_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("name", "connectionPoolName", allocator)
          .AddMember("in", "path", allocator)
          .AddMember("description", "name of a connection pool", allocator)
          .AddMember("required", true, allocator)
          .AddMember("type", "string", allocator),
      allocator);

  std::string route_name_param_str =
      json_pointer_stringfy(route_name_param_ptr);

  // /definitions/ConnectionPoolStatus
  const RestApiComponent::JsonPointer connection_pool_status_def_ptr(
      connection_pool_status_def_tokens.data(),
      connection_pool_status_def_tokens.size());

  connection_pool_status_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember("reusedServerConnections",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("idleServerConnections",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("stashedServerConnections",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator),
              allocator),
      allocator);

  std::string connection_pool_status_def_ptr_str =
      json_pointer_stringfy(connection_pool_status_def_ptr);

  // /definitions/ConnectionPoolConfig
  const RestApiComponent::JsonPointer connection_pool_config_def_ptr(
      connection_pool_config_def_tokens.data(),
      connection_pool_config_def_tokens.size());

  connection_pool_config_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember("idleTimeoutInMs",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("maxIdleServerConnections",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator),
              allocator),
      allocator);

  std::string connection_pool_config_def_ptr_str =
      json_pointer_stringfy(connection_pool_config_def_ptr);

  // /definitions/ConnectionPoolSummary
  const RestApiComponent::JsonPointer connection_pool_summary_def_ptr(
      connection_pool_summary_def_tokens.data(),
      connection_pool_summary_def_tokens.size());

  connection_pool_summary_def_ptr.Set(
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

  std::string connection_pool_summary_def_ptr_str =
      json_pointer_stringfy(connection_pool_summary_def_ptr);

  // /definitions/ConnectionPoolList
  const RestApiComponent::JsonPointer connection_pool_list_def_ptr(
      connection_pool_list_def_tokens.data(),
      connection_pool_list_def_tokens.size());

  connection_pool_list_def_ptr.Set(
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
                                          connection_pool_summary_def_ptr_str
                                              .data(),
                                          connection_pool_summary_def_ptr_str
                                              .size(),
                                          allocator),
                                      allocator),
                              allocator),
                      allocator),
              allocator),
      allocator);

  std::string connection_pool_list_def_ptr_str =
      json_pointer_stringfy(connection_pool_list_def_ptr);

  // /paths/connectionPoolStatus
  {
    JsonPointer ptr(connection_pool_status_path_tokens.data(),
                    connection_pool_status_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("connectionpool", allocator),
                               allocator)
                    .AddMember("description",
                               "Get status of the connection_pool plugin",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember(
                                        "description",
                                        "status of the connection_pool plugin",
                                        allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    connection_pool_status_def_ptr_str
                                                        .data(),
                                                    connection_pool_status_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/routesConfig
  {
    JsonPointer ptr(connection_pool_config_path_tokens.data(),
                    connection_pool_config_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("connectionpool", allocator),
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
                                                    connection_pool_config_def_ptr_str
                                                        .data(),
                                                    connection_pool_config_def_ptr_str
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

  // /paths/connectionPoolStatus
  {
    JsonPointer ptr(connection_pool_status_path_tokens.data(),
                    connection_pool_status_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("connectionpool", allocator),
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
                                                    connection_pool_status_def_ptr_str
                                                        .data(),
                                                    connection_pool_status_def_ptr_str
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

  // /paths/connectionPool
  {
    JsonPointer ptr(connection_pool_list_path_tokens.data(),
                    connection_pool_list_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("connectionpool", allocator),
                               allocator)
                    .AddMember("description",
                               "Get list of the connection pools", allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "list of the connection pools",
                                               allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    connection_pool_list_def_ptr_str
                                                        .data(),
                                                    connection_pool_list_def_ptr_str
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

  std::array paths{
      RestApiComponentPath{rest_api_srv, RestConnectionPoolStatus::path_regex,
                           std::make_unique<RestConnectionPoolStatus>(
                               require_realm_connection_pool)},
      RestApiComponentPath{rest_api_srv, RestConnectionPoolList::path_regex,
                           std::make_unique<RestConnectionPoolList>(
                               require_realm_connection_pool)},
      RestApiComponentPath{rest_api_srv, RestConnectionPoolConfig::path_regex,
                           std::make_unique<RestConnectionPoolConfig>(
                               require_realm_connection_pool)},
  };

  mysql_harness::on_service_ready(env);

  wait_for_stop(env, 0);

  // in case rest_api never initialized, ensure the rest_api_component doesn't
  // have a callback to use
  if (!spec_adder_executed) rest_api_srv.remove_process_spec(spec_adder);
}

static constexpr std::array required{
    "logger",
    "rest_api",
};

namespace {

class RestConnectionPoolConfigExposer
    : public mysql_harness::SectionConfigExposer {
 public:
  using DC = mysql_harness::DynamicConfig;
  RestConnectionPoolConfigExposer(
      const bool initial, const RestConnectionPoolPluginConfig &plugin_config,
      const mysql_harness::ConfigSection &default_section)
      : mysql_harness::SectionConfigExposer(
            initial, default_section,
            DC::SectionId{"rest_configs", kSectionName}),
        plugin_config_(plugin_config) {}

  void expose() override {
    expose_option("require_realm", plugin_config_.require_realm,
                  std::string(kHttpDefaultAuthRealmName));
  }

 private:
  const RestConnectionPoolPluginConfig &plugin_config_;
};

}  // namespace

static void expose_configuration(mysql_harness::PluginFuncEnv *env,
                                 const char * /*key*/, bool initial) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (!info->config) return;

  for (const mysql_harness::ConfigSection *section : info->config->sections()) {
    if (section->name == kSectionName) {
      RestConnectionPoolPluginConfig config{section};
      RestConnectionPoolConfigExposer(initial, config,
                                      info->config->get_default_section())
          .expose();
    }
  }
}

extern "C" {
mysql_harness::Plugin REST_CONNECTION_POOL_EXPORT
    harness_plugin_rest_connection_pool = {
        mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
        mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
        "REST_CONNECTION_POOL",                  // name
        VERSION_NUMBER(0, 0, 1),
        // requires
        required.size(),
        required.data(),
        // conflicts
        0,
        nullptr,
        init,     // init
        nullptr,  // deinit
        start,    // start
        nullptr,  // stop
        true,     // declares_readiness
        supported_options.size(),
        supported_options.data(),
        expose_configuration,
};
}
