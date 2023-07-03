/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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
 * Auth Realm plugin.
 *
 *
 * section name
 * :  http_auth_realm
 *
 * config options
 * :  - name
 *    - backend
 *    - method
 *    - require
 */
#include "http_auth_realm.h"

#include <sys/types.h>

#include <array>
#include <map>
#include <memory>  // shared_ptr
#include <stdexcept>
#include <string>

// Harness interface include files
#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/plugin_config.h"
#include "mysql/harness/utility/string.h"

#include "mysqlrouter/http_auth_backend_component.h"
#include "mysqlrouter/http_auth_realm_component.h"
#include "mysqlrouter/http_auth_realm_export.h"
#include "mysqlrouter/supported_http_options.h"

IMPORT_LOG_FUNCTIONS()

using namespace std::string_literals;

static constexpr const char kSectionName[]{"http_auth_realm"};
static std::vector<std::string> registered_realms;

using StringOption = mysql_harness::StringOption;

#define GET_OPTION_CHECKED(option, section, name, value) \
  static_assert(mysql_harness::str_in_collection(        \
      http_auth_realm_suported_options, name));          \
  option = get_option(section, name, value);

class HttpAuthRealmPluginConfig : public mysql_harness::BasePluginConfig {
 public:
  std::string backend;
  std::string method;
  std::string require;
  std::string name;

  explicit HttpAuthRealmPluginConfig(
      const mysql_harness::ConfigSection *section)
      : mysql_harness::BasePluginConfig(section) {
    GET_OPTION_CHECKED(backend, section, "backend", StringOption{});
    GET_OPTION_CHECKED(method, section, "method", StringOption{});
    GET_OPTION_CHECKED(require, section, "require", StringOption{});
    GET_OPTION_CHECKED(name, section, "name", StringOption{});
  }

  std::string get_default(const std::string &option) const override {
    const std::map<std::string, std::string> defaults{
        {"require", "valid-user"},
    };

    auto it = defaults.find(option);
    if (it == defaults.end()) {
      return std::string();
    }
    return it->second;
  }

  bool is_required(const std::string &option) const override {
    if (option == "name") return true;
    if (option == "backend") return true;
    if (option == "method") return true;
    return false;
  }
};

std::shared_ptr<HttpAuthRealmComponent::value_type> auth_realms;
constexpr const char kMethodNameBasic[]{"basic"};

static void init(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (nullptr == info->config) {
    return;
  }

  try {
    std::set<std::string> known_methods{{kMethodNameBasic}};
    std::set<std::string> known_backends;
    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name == "http_auth_backend") {
        known_backends.emplace(section->key);
      }
    }

    auto &auth_realm_component = HttpAuthRealmComponent::get_instance();

    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name != kSectionName) {
        continue;
      }

      if (section->key.empty()) {
        set_error(env, mysql_harness::kConfigInvalidArgument,
                  "The config section [%s] requires a name, like [%s:example]",
                  kSectionName, kSectionName);
        return;
      }

      HttpAuthRealmPluginConfig config(section);

      if (config.method != kMethodNameBasic) {
        throw std::invalid_argument(
            "unsupported authentication method for [http_auth_realm] '" +
            section->key + "': " + config.method + ", supported method(s): " +
            mysql_harness::join(known_methods, ","));
      }

      if (known_backends.find(config.backend) == known_backends.end()) {
        std::string section_name = section->name;
        if (!section->key.empty()) section_name += ":" + section->key;

        const std::string backend_msg =
            (known_backends.empty())
                ? "No [http_auth_backend:" + config.backend +
                      "] section defined."
                : "Known [http_auth_backend:<...>] section" +
                      (known_backends.size() > 1 ? "s"s : ""s) + ": " +
                      mysql_harness::join(known_backends, ", ");

        throw std::invalid_argument(
            "The option 'backend=" + config.backend + "' in [" + section_name +
            "] does not match any http_auth_backend. " + backend_msg);
      }

      const std::string realm_name = section->key;
      auth_realm_component.add_realm(
          realm_name,
          std::make_shared<HttpAuthRealm>(config.name, config.require,
                                          config.method, config.backend));
      registered_realms.push_back(realm_name);
    }
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

static void deinit(mysql_harness::PluginFuncEnv *) {
  auto &auth_realm_component = HttpAuthRealmComponent::get_instance();

  for (const auto &realm : registered_realms) {
    auth_realm_component.remove_realm(realm);
  }

  registered_realms.clear();
}

static const std::array<const char *, 1> required = {{
    "logger",
}};

extern "C" {
mysql_harness::Plugin HTTP_AUTH_REALM_EXPORT harness_plugin_http_auth_realm = {
    mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
    mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
    "HTTP_AUTH_REALM",                       // name
    VERSION_NUMBER(0, 0, 1),
    // requires
    required.size(),
    required.data(),
    // conflicts
    0,
    nullptr,
    init,     // init
    deinit,   // deinit
    nullptr,  // start
    nullptr,  // stop
    false,    // declares_readiness
    http_auth_realm_suported_options.size(),
    http_auth_realm_suported_options.data(),
};
}
