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

#include <array>
#include <memory>

#include "mysql/harness/config_parser.h"
#include "mysql/harness/dynamic_config.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/resolver/registry.h"
#include "mysqlrouter/host_cache_component.h"
#include "mysqlrouter/host_cache_export.h"
#include "mysqlrouter/supported_host_cache_options.h"

#include "host_cache.h"
#include "host_cache_plugin_config.h"
#include "my_macros.h"

using ResolverRegistry = mysql_harness::resolver::Registry;
using ResolverCachePolicy = mysql_harness::resolver::CachePolicy;

static constexpr const char kSectionName[]{"host_cache"};
static std::shared_ptr<HostCachePluginConfig> g_config;

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

      auto &registry = ResolverRegistry::get_instance();
      auto &component = HostCacheComponent::get_instance();

      g_config = std::make_shared<HostCachePluginConfig>(section);
      auto host_cache_resolver = std::make_shared<HostCache>(g_config);

      if (g_config->enabled_) {
        // Setup the Resolver in configuration phase.
        registry.set(ResolverCachePolicy::FillOnSuccess, host_cache_resolver);
        registry.set(ResolverCachePolicy::UseIfPresent, host_cache_resolver);
      }
      component.set_configuration(*g_config);
      component.set_statistics(host_cache_resolver->get_statistics());
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

static void deinit([[maybe_unused]] mysql_harness::PluginFuncEnv *env) {
  auto &registry = ResolverRegistry::get_instance();

  g_config.reset();
  registry.clear();
}

static void expose_configuration(mysql_harness::PluginFuncEnv *env,
                                 const char *key, bool initial) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (!info || !info->config) return;

  for (const mysql_harness::ConfigSection *section : info->config->sections()) {
    if (section->name != kSectionName || section->key != key) {
      continue;
    }

    HostCachePluginConfig config(section);
    config.expose_configuration(info->config->get_default_section(), initial);
  }
}

static constexpr std::array required{
    "logger",
};

static constexpr std::array host_cache_supported_options{
    host_cache::options::kOptionEnabled,
    host_cache::options::kOptionTtlSuccess,
    host_cache::options::kOptionTtlNegative,
    host_cache::options::kOptionTtlJitter,
    host_cache::options::kOptionMaxEntries,
};

extern "C" {
mysql_harness::Plugin HOST_CACHE_EXPORT harness_plugin_host_cache = {
    mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
    mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
    "host_cache",                            // name
    VERSION_NUMBER(0, 0, 1),
    // requires
    required.size(),
    required.data(),
    // conflicts
    0,
    nullptr,
    init,
    deinit,   // deinit
    nullptr,  // start
    nullptr,  // stop
    false,    // declares_readiness
    host_cache_supported_options.size(),
    host_cache_supported_options.data(),
    expose_configuration,
};
}
