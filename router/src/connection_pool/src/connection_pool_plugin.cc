/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
 * connection pool plugin.
 */

#include <array>
#include <chrono>
#include <memory>  // shared_ptr
#include <mutex>
#include <stdexcept>
#include <system_error>

// Harness interface include files
#include "common.h"
#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/plugin_config.h"

#include "mysqlrouter/connection_pool.h"
#include "mysqlrouter/connection_pool_component.h"
#include "mysqlrouter/connection_pool_plugin_export.h"
#include "scope_guard.h"

IMPORT_LOG_FUNCTIONS()

template <class T>
using IntOption = mysql_harness::IntOption<T>;

static constexpr const std::string_view kSectionName{"connection_pool"};

static constexpr const char kMaxIdleServerConnections[]{
    "max_idle_server_connections"};
static constexpr const char kIdleTimeout[]{"idle_timeout"};

static constexpr std::array supported_options{
    kMaxIdleServerConnections,
    kIdleTimeout,
};

class ConnectionPoolPluginConfig : public mysql_harness::BasePluginConfig {
 public:
  uint32_t max_idle_server_connections;
  uint32_t idle_timeout;

  explicit ConnectionPoolPluginConfig(
      const mysql_harness::ConfigSection *section)
      : mysql_harness::BasePluginConfig(section),
        max_idle_server_connections(get_option(
            section, kMaxIdleServerConnections, IntOption<uint32_t>{})),
        idle_timeout(get_option(section, kIdleTimeout, IntOption<uint32_t>{})) {
  }

  std::string get_default(const std::string &option) const override {
    const std::map<std::string_view, std::string> defaults{
        {kMaxIdleServerConnections, "0"},  // disabled by default
        {kIdleTimeout, "5"},               // in seconds
    };

    auto it = defaults.find(option);

    return it == defaults.end() ? std::string() : it->second;
  }

  [[nodiscard]] bool is_required(
      const std::string & /* option */) const override {
    return false;
  }
};

static void init(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (nullptr == info->config) {
    return;
  }

  auto &connection_pool_component = ConnectionPoolComponent::get_instance();

  Scope_guard init_guard([&]() { connection_pool_component.clear(); });

  // assume there is only one section for us
  try {
    int sections{0};

    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name != kSectionName) continue;

      if (sections != 0) {
        throw std::invalid_argument("[" + section->name +
                                    (section->key.empty() ? "" : ":") +
                                    section->key + "] already loaded.");
      }

      ++sections;

      ConnectionPoolPluginConfig config{section};

      connection_pool_component.emplace(
          section->key.empty() ? ConnectionPoolComponent::default_pool_name()
                               : section->key,
          std::make_shared<ConnectionPool>(
              config.max_idle_server_connections,
              std::chrono::seconds{config.idle_timeout}));
    }

    init_guard.commit();
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());

  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

static void deinit(mysql_harness::PluginFuncEnv *) {
  ConnectionPoolComponent::get_instance().clear();
}

const static std::array<const char *, 2> required = {{
    "logger",
    "io",
}};

extern "C" {
mysql_harness::Plugin CONNECTION_POOL_PLUGIN_EXPORT
    harness_plugin_connection_pool = {
        mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
        mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
        "connection pool",                       // name
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
        supported_options.size(),
        supported_options.data(),
};
}
