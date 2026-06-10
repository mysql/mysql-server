/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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
 * REST API for the host_cache plugin.
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
#include "mysqlrouter/rest_host_cache_export.h"

#include "rest_host_cache_config.h"
#include "rest_host_cache_entries.h"
#include "rest_host_cache_spec.h"
#include "rest_host_cache_status.h"

IMPORT_LOG_FUNCTIONS()

using namespace std::string_literals;

using StringOption = mysql_harness::StringOption;

static constexpr const char kSectionName[]{"rest_host_cache"};

static constexpr const char kRequireRealm[]{"require_realm"};

static constexpr const std::array supported_options{
    kRequireRealm,
};

// one shared setting
std::string g_require_realm_host_cache;

#define GET_OPTION_CHECKED(option, section, name, value)                    \
  static_assert(mysql_harness::str_in_collection(supported_options, name)); \
  option = get_option(section, name, value);

class RestHostCachePluginConfig : public mysql_harness::BasePluginConfig {
 public:
  std::string require_realm;

  explicit RestHostCachePluginConfig(
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

  if (nullptr == info || nullptr == info->config) {
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

      RestHostCachePluginConfig config{section};

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

      g_require_realm_host_cache = config.require_realm;
    }
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

static void start(mysql_harness::PluginFuncEnv *env) {
  auto &rest_api_srv = RestApiComponent::get_instance();

  const bool spec_adder_executed =
      rest_api_srv.try_process_spec(append_specification);

  std::array paths{
      RestApiComponentPath{
          rest_api_srv, RestHostCacheStatus::path_regex,
          std::make_unique<RestHostCacheStatus>(g_require_realm_host_cache)},
      RestApiComponentPath{
          rest_api_srv, RestHostCacheEntries::path_regex,
          std::make_unique<RestHostCacheEntries>(g_require_realm_host_cache)},
      RestApiComponentPath{
          rest_api_srv, RestHostCacheConfig::path_regex,
          std::make_unique<RestHostCacheConfig>(g_require_realm_host_cache)},
  };

  mysql_harness::on_service_ready(env);

  wait_for_stop(env, 0);

  // in case rest_api never initialized, ensure the rest_api_component
  // doesn't
  // have a callback to use
  if (!spec_adder_executed)
    rest_api_srv.remove_process_spec(append_specification);
}

static constexpr std::array required{
    "logger",
    "rest_api",
};

namespace {

class RestHostCachePluginConfigExposer
    : public mysql_harness::SectionConfigExposer {
 public:
  using DC = mysql_harness::DynamicConfig;
  RestHostCachePluginConfigExposer(
      const bool initial, const RestHostCachePluginConfig &plugin_config,
      const mysql_harness::ConfigSection &default_section)
      : mysql_harness::SectionConfigExposer(
            initial, default_section,
            DC::SectionId{"rest_configs", kSectionName}),
        plugin_config_(plugin_config) {}

  void expose() override {
    expose_option(kRequireRealm, plugin_config_.require_realm,
                  std::string(kHttpDefaultAuthRealmName));
  }

 private:
  const RestHostCachePluginConfig &plugin_config_;
};

}  // namespace

static void expose_configuration(mysql_harness::PluginFuncEnv *env,
                                 const char * /*key*/, bool initial) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (!info || !info->config) return;

  for (const mysql_harness::ConfigSection *section : info->config->sections()) {
    if (section->name == kSectionName) {
      RestHostCachePluginConfig config{section};
      RestHostCachePluginConfigExposer(initial, config,
                                       info->config->get_default_section())
          .expose();
    }
  }
}

extern "C" {
mysql_harness::Plugin REST_HOST_CACHE_EXPORT harness_plugin_rest_host_cache = {
    mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
    mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
    "REST_HOST_CACHE",                       // name
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
