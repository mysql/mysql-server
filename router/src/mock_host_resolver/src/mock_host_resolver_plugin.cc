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

#include <memory>

#include "mysql/harness/config_parser.h"
#include "mysql/harness/dynamic_config.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/resolver/registry.h"

#include "mock_host_resolver_plugin_config.h"
#include "mock_resolver.h"
#include "mysqlrouter/mock_host_resolver_export.h"

using ResolverRegistry = mysql_harness::resolver::Registry;
using ResolverCachePolicy = mysql_harness::resolver::CachePolicy;

static constexpr const char kSectionName[]{"mock_host_resolver"};

std::shared_ptr<MockResolver> g_resolver;

static void init(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (nullptr == info || nullptr == info->config) {
    return;
  }

  try {
    bool section_found{false};
    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name != kSectionName) continue;

      if (section_found) {
        set_error(env, mysql_harness::kConfigInvalidArgument,
                  "[%s] found another config-section '%s', only one allowed",
                  kSectionName, section->key.c_str());
        return;
      }

      if (!section->key.empty()) {
        set_error(env, mysql_harness::kConfigInvalidArgument,
                  "[%s] section does not expect a key, found '%s'",
                  kSectionName, section->key.c_str());
        return;
      }

      ResolveActions actions;
      HostCachePluginConfig config{section, actions};

      g_resolver = std::make_shared<MockResolver>(actions);

      auto &registry = ResolverRegistry::get_instance();
      registry.set(ResolverCachePolicy::Bypass, g_resolver);

      section_found = true;
    }
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

static void deinit(mysql_harness::PluginFuncEnv * /*env*/) {
  auto &registry = ResolverRegistry::get_instance();

  registry.clear();
}

static constexpr std::array required{
    "logger",
};

extern "C" {
mysql_harness::Plugin MOCK_HOST_RESOLVER_EXPORT
    harness_plugin_mock_host_resolver = {
        mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
        mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
        "mock_host_resolver",                    // name
        VERSION_NUMBER(0, 0, 1),
        // requires
        required.size(), required.data(),
        // conflicts
        0, nullptr, init,
        deinit,   // deinit
        nullptr,  // start
        nullptr,  // stop
        false,    // declares_readiness
        0, nullptr,
        nullptr,  // Test plugin, ignore.
};
}
