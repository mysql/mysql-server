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
 * REST API for the router app.
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
#include "mysqlrouter/supported_rest_options.h"

#include "rest_router_status.h"
IMPORT_LOG_FUNCTIONS()

using namespace std::string_literals;

static const char kSectionName[]{"rest_router"};

// one shared setting
std::string require_realm_router;

using StringOption = mysql_harness::StringOption;

#define GET_OPTION_CHECKED(option, section, name, value)                      \
  static_assert(                                                              \
      mysql_harness::str_in_collection(rest_plugin_supported_options, name)); \
  option = get_option(section, name, value);

class RestRouterPluginConfig : public mysql_harness::BasePluginConfig {
 public:
  std::string require_realm;

  explicit RestRouterPluginConfig(const mysql_harness::ConfigSection *section)
      : mysql_harness::BasePluginConfig(section) {
    GET_OPTION_CHECKED(require_realm, section, "require_realm", StringOption{});
  }

  std::string get_default(std::string_view /* option */) const override {
    return {};
  }

  bool is_required(std::string_view option) const override {
    if (option == "require_realm") return true;
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

      RestRouterPluginConfig config{section};

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

      require_realm_router = config.require_realm;
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

static const std::array router_status_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("RouterStatus"),
};

static const std::array router_status_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/router/status"),
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
                .AddMember("name", "app", allocator)
                .AddMember("description", "Application", allocator),
            allocator);
  }

  // /definitions/RouterStatus
  const RestApiComponent::JsonPointer router_status_def_ptr(
      router_status_def_tokens.data(), router_status_def_tokens.size());

  router_status_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember("timeStarted",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator)
                                 .AddMember("format", "data-time", allocator),
                             allocator)
                  .AddMember("processId",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("version",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator)
                  .AddMember("hostname",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator)
                  .AddMember("productEdition",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "string", allocator),
                             allocator),
              allocator),
      allocator);

  std::string router_status_def_ptr_str =
      json_pointer_stringfy(router_status_def_ptr);

  // /paths/routerStatus
  {
    JsonPointer ptr(router_status_path_tokens.data(),
                    router_status_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("app", allocator),
                               allocator)
                    .AddMember("description", "Get status of the application",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "status of application",
                                               allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    router_status_def_ptr_str
                                                        .data(),
                                                    router_status_def_ptr_str
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
      RestApiComponentPath{
          rest_api_srv, RestRouterStatus::path_regex,
          std::make_unique<RestRouterStatus>(require_realm_router)},
  };

  mysql_harness::on_service_ready(env);

  wait_for_stop(env, 0);

  // in case rest_api never initialized, ensure the rest_api_component doesn't
  // have a callback to use
  if (!spec_adder_executed) rest_api_srv.remove_process_spec(spec_adder);
}

#if defined(_MSC_VER) && defined(rest_router_EXPORTS)
/* We are building this library */
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

static constexpr std::array rest_router_plugin_requires = {
    "logger",
    "rest_api",
};

namespace {

class RestRouterConfigExposer : public mysql_harness::SectionConfigExposer {
 public:
  using DC = mysql_harness::DynamicConfig;
  RestRouterConfigExposer(const bool initial,
                          const RestRouterPluginConfig &plugin_config,
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
  const RestRouterPluginConfig &plugin_config_;
};

}  // namespace

static void expose_configuration(mysql_harness::PluginFuncEnv *env,
                                 const char * /*key*/, bool initial) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (!info->config) return;

  for (const mysql_harness::ConfigSection *section : info->config->sections()) {
    if (section->name == kSectionName) {
      RestRouterPluginConfig config{section};
      RestRouterConfigExposer(initial, config,
                              info->config->get_default_section())
          .expose();
    }
  }
}

extern "C" {
mysql_harness::Plugin DLLEXPORT harness_plugin_rest_router = {
    mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
    mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
    "REST_ROUTER",                           // name
    VERSION_NUMBER(0, 0, 1),
    // requires
    rest_router_plugin_requires.size(),
    rest_router_plugin_requires.data(),
    // conflicts
    0,
    nullptr,
    init,     // init
    nullptr,  // deinit
    start,    // start
    nullptr,  // stop
    true,     // declares_readiness
    rest_plugin_supported_options.size(),
    rest_plugin_supported_options.data(),
    expose_configuration,
};
}
