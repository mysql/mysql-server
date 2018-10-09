/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

// Harness interface include files
#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/utility/string.h"

#include "mysqlrouter/http_auth_backend_component.h"
#include "mysqlrouter/http_auth_realm_component.h"
#include "mysqlrouter/http_auth_realm_export.h"
#include "mysqlrouter/plugin_config.h"

IMPORT_LOG_FUNCTIONS()

static constexpr const char kSectionName[]{"http_auth_realm"};

using mysql_harness::ARCHITECTURE_DESCRIPTOR;
using mysql_harness::PLUGIN_ABI_VERSION;
using mysql_harness::Plugin;
using mysql_harness::PluginFuncEnv;

std::error_code HttpAuthRealm::authenticate(const std::string &username,
                                            const std::string &password) const {
  return HttpAuthBackendComponent::get_instance().authenticate(
      backend(), username, password);
}

namespace {
class PluginConfig : public mysqlrouter::BasePluginConfig {
 public:
  std::string backend;
  std::string method;
  std::string require;
  std::string name;

  explicit PluginConfig(const mysql_harness::ConfigSection *section)
      : mysqlrouter::BasePluginConfig(section),
        backend(get_option_string(section, "backend")),
        method(get_option_string(section, "method")),
        require(get_option_string(section, "require")),
        name(get_option_string(section, "name")) {}

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
}  // namespace

std::shared_ptr<HttpAuthRealmComponent::value_type> auth_realms;
constexpr const char kMethodNameBasic[]{"basic"};

static void init(PluginFuncEnv *env) {
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

    auth_realms = std::make_shared<HttpAuthRealmComponent::value_type>();
    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name != kSectionName) {
        continue;
      }

      PluginConfig config(section);

      if (config.method != kMethodNameBasic) {
        throw std::invalid_argument(
            "unsupported authentication method for [http_auth_realm] '" +
            section->key + "': " + config.method + ", supported method(s): " +
            mysql_harness::join(known_methods, ","));
      }

      if (known_backends.find(config.backend) == known_backends.end()) {
        throw std::invalid_argument(
            "unknown authentication backend for [http_auth_realm] '" +
            section->key + "': " + config.method +
            ", known backend(s): " + mysql_harness::join(known_backends, ","));
      }

      auth_realms->insert({section->key, std::make_shared<HttpAuthRealm>(
                                             config.name, config.require,
                                             config.method, config.backend)});
    }
    HttpAuthRealmComponent::get_instance().init(auth_realms);
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

extern "C" {
Plugin HTTP_AUTH_REALM_EXPORT harness_plugin_http_auth_realm = {
    PLUGIN_ABI_VERSION,
    ARCHITECTURE_DESCRIPTOR,
    "HTTP_AUTH_REALM",
    VERSION_NUMBER(0, 0, 1),
    0,
    nullptr,  // requires
    0,
    nullptr,  // conflicts
    init,     // init
    nullptr,  // deinit
    nullptr,  // start
    nullptr,  // stop
};
}
