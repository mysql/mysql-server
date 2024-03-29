/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
 * http auth backend plugin.
 *
 * section name
 * :  http_auth_backend
 */

#include <array>
#include <future>
#include <mutex>
#include <thread>

#include <sys/types.h>

// Harness interface include files
#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/plugin_config.h"
#include "mysql/harness/utility/string.h"

#include "mysqlrouter/http_auth_backend_component.h"
#include "mysqlrouter/http_auth_backend_export.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/supported_http_options.h"

#include "http_auth_backend.h"
#include "http_auth_backend_metadata_cache.h"

IMPORT_LOG_FUNCTIONS()

static constexpr const char kSectionName[]{"http_auth_backend"};
static std::vector<std::string> registered_backends;

using StringOption = mysql_harness::StringOption;

#define GET_OPTION_CHECKED(option, section, name, value)                       \
  static_assert(                                                               \
      mysql_harness::str_in_collection(http_backend_supported_options, name)); \
  option = get_option(section, name, value);
namespace {
class HtpasswdPluginConfig : public mysql_harness::BasePluginConfig {
 public:
  std::string filename;

  explicit HtpasswdPluginConfig(const mysql_harness::ConfigSection *section)
      : mysql_harness::BasePluginConfig(section) {
    GET_OPTION_CHECKED(filename, section, "filename", StringOption{});
  }

  std::string get_default(const std::string &option) const override {
    if (option == "filename") return "users";
    return std::string();
  }

  bool is_required(const std::string & /* option */) const override {
    return false;
  }
};

class HttpAuthBackendFactory {
 public:
  static std::shared_ptr<HttpAuthBackend> create(
      const std::string &name, const mysql_harness::ConfigSection *section) {
    if (name == "file") {
      auto s = std::make_shared<HttpAuthBackendHtpasswd>();

      HtpasswdPluginConfig config(section);

      if (auto ec = s->from_file(config.filename)) {
        throw std::runtime_error("parsing " + config.filename +
                                 " failed for section [" + section->name +
                                 "]: " + ec.message());
      }

      return s;
    } else if (name == "metadata_cache") {
      auto s = std::make_shared<HttpAuthBackendMetadataCache>();
      return s;
    } else {
      throw std::invalid_argument("unknown backend=" + name +
                                  " in section: " + section->name);
    }
  }
};

class PluginConfig : public mysql_harness::BasePluginConfig {
 public:
  std::string backend;
  std::string filename;

  explicit PluginConfig(const mysql_harness::ConfigSection *section)
      : mysql_harness::BasePluginConfig(section) {
    GET_OPTION_CHECKED(backend, section, "backend", StringOption{});
  }

  std::string get_default(const std::string & /* option */) const override {
    return std::string();
  }

  bool is_required(const std::string &option) const override {
    if (option == "backend") return true;
    return false;
  }
};
}  // namespace

std::shared_ptr<HttpAuthBackendComponent::value_type> auth_backends;

static void init(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (nullptr == info->config) {
    return;
  }

  try {
    auto &auth_backend_component = HttpAuthBackendComponent::get_instance();

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

      PluginConfig config(section);
      const std::string backend_name = section->key;
      auth_backend_component.add_backend(
          backend_name,
          HttpAuthBackendFactory::create(config.backend, section));

      registered_backends.push_back(backend_name);
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
  const mysql_harness::ConfigSection *section = get_config_section(env);

  PluginConfig config(section);
  if (config.backend == "metadata_cache") {
    auto *cache_api = metadata_cache::MetadataCacheAPI::instance();

    if (cache_api->is_initialized()) {
      // metada_cache is already running, we need to force update cache
      cache_api->enable_fetch_auth_metadata();
      cache_api->force_cache_update();
    } else {
      while (!cache_api->is_initialized()) {
        if (!(!env || is_running(env))) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      cache_api->enable_fetch_auth_metadata();
    }

    try {
      // verify that auth_cache timers are greater than the ttl and that
      // auth_cache_refresh_interval is smaller than auth_cache_ttl
      cache_api->check_auth_metadata_timers();
    } catch (const std::invalid_argument &e) {
      log_error("%s", e.what());
      set_error(env, mysql_harness::kConfigInvalidArgument, "%s", e.what());
      clear_running(env);
    }
  }
}

static void deinit(mysql_harness::PluginFuncEnv *) {
  auto &auth_backend_component = HttpAuthBackendComponent::get_instance();

  for (const auto &backend : registered_backends) {
    auth_backend_component.remove_backend(backend);
  }

  registered_backends.clear();
}

static const std::array<const char *, 2> required = {{
    "logger",
    "router_protobuf",
}};

extern "C" {
mysql_harness::Plugin HTTP_AUTH_BACKEND_EXPORT
    harness_plugin_http_auth_backend = {
        mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
        mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
        "HTTP_AUTH_BACKEND",                     // name
        VERSION_NUMBER(0, 0, 1),
        // requires
        required.size(),
        required.data(),
        // conflicts
        0,
        nullptr,
        init,     // init
        deinit,   // deinit
        start,    // start
        nullptr,  // stop
        false,    // declares_readiness
        http_backend_supported_options.size(),
        http_backend_supported_options.data(),
};
}
