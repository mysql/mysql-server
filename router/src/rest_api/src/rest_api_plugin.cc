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
 * REST API plugin.
 */
#include "rest_api_plugin.h"

#include <string>

#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/utility/string.h"  // ::join()
#include "mysqlrouter/http_server_component.h"
#include "mysqlrouter/plugin_config.h"
#include "mysqlrouter/rest_api_utils.h"

#include "rest_api.h"
IMPORT_LOG_FUNCTIONS()

static const char kSectionName[]{"rest_api"};

// one shared setting
std::string require_realm_api;

class RestApiPluginConfig : public mysqlrouter::BasePluginConfig {
 public:
  std::string require_realm;

  explicit RestApiPluginConfig(const mysql_harness::ConfigSection *section)
      : mysqlrouter::BasePluginConfig(section),
        require_realm(get_option_string(section, "require_realm")) {}

  std::string get_default(const std::string & /* option */) const override {
    return {};
  }

  bool is_required(const std::string & /* option */) const override {
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

      RestApiPluginConfig config{section};

      if (!config.require_realm.empty() &&
          (known_realms.find(config.require_realm) == known_realms.end())) {
        throw std::invalid_argument(
            "unknown authentication realm for [" + std::string(kSectionName) +
            "] '" + section->key + "': " + config.require_realm +
            ", known realm(s): " + mysql_harness::join(known_realms, ","));
      }

      require_realm_api = config.require_realm;
    }
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

RestApi::RestApi(const std::string &uri_prefix,
                 const std::string &uri_prefix_regex)
    : uri_prefix_(uri_prefix), uri_prefix_regex_(uri_prefix_regex) {
  auto &allocator = spec_doc_.GetAllocator();
  spec_doc_.SetObject()
      .AddMember("swagger", "2.0", allocator)
      .AddMember("info",
                 RestApiComponent::JsonValue(rapidjson::kObjectType)
                     .AddMember("title", "MySQL Router", allocator)
                     .AddMember("description", "API of MySQL Router", allocator)
                     .AddMember("version", kRestAPIVersion, allocator),
                 allocator)
      .AddMember("basePath",
                 RestApiComponent::JsonValue(uri_prefix.c_str(),
                                             uri_prefix.size(), allocator),
                 allocator)
      .AddMember("tags",
                 RestApiComponent::JsonValue(rapidjson::kArrayType).Move(),
                 allocator)
      .AddMember("paths",
                 RestApiComponent::JsonValue(rapidjson::kObjectType).Move(),
                 allocator)
      .AddMember("definitions",
                 RestApiComponent::JsonValue(rapidjson::kObjectType).Move(),
                 allocator)
      //
      ;
}

void RestApi::process_spec(RestApiComponent::SpecProcessor spec_processor) {
  std::lock_guard<std::mutex> mx(spec_doc_mutex_);

  spec_processor(spec_doc_);
}

std::string RestApi::spec() {
  rapidjson::StringBuffer json_buf;
  {
    rapidjson::Writer<rapidjson::StringBuffer> json_writer(json_buf);

    std::lock_guard<std::mutex> mx(spec_doc_mutex_);
    spec_doc_.Accept(json_writer);
  }

  return {json_buf.GetString(), json_buf.GetSize()};
}

void RestApi::add_path(const std::string &path,
                       std::unique_ptr<BaseRestApiHandler> handler) {
  std::unique_lock<std::shared_timed_mutex> mx(rest_api_handler_mutex_);
  // ensure path is unique
  if (rest_api_handlers_.end() !=
      std::find_if(
          rest_api_handlers_.begin(), rest_api_handlers_.end(),
          [&path](const auto &value) { return std::get<0>(value) == path; })) {
    throw std::invalid_argument("path already exists in rest_api: " + path);
  }

  rest_api_handlers_.emplace_back(path, std::regex(path), std::move(handler));
}

void RestApi::remove_path(const std::string &path) {
  std::unique_lock<std::shared_timed_mutex> mx(rest_api_handler_mutex_);

  rest_api_handlers_.erase(
      std::remove_if(
          rest_api_handlers_.begin(), rest_api_handlers_.end(),
          [&path](const auto &value) { return std::get<0>(value) == path; }),
      rest_api_handlers_.end());
}

void RestApi::handle_paths(HttpRequest &req) {
  std::string uri_path(req.get_uri().get_path());

  // strip prefix from uri path
  std::string uri_suffix;
  {
    std::smatch m;
    if (!std::regex_search(uri_path, m, std::regex(uri_prefix_regex_))) {
      send_rfc7807_not_found_error(req);
      return;
    }
    uri_suffix = m.suffix().str();
  }

  if (uri_suffix.empty() || uri_suffix[0] == '/') {
    std::smatch m;
    std::shared_lock<std::shared_timed_mutex> mx(rest_api_handler_mutex_);
    for (const auto &path : rest_api_handlers_) {
      if (std::regex_match(uri_suffix, m, std::get<1>(path))) {
        std::vector<std::string> matches;

        for (const auto &match : m) {
          matches.emplace_back(match.str());
        }
        if (std::get<2>(path)->try_handle_request(req, uri_prefix(), matches)) {
          return;
        }
      }
    }
  }

  // if nothing matched, send a generic 404 handler
  send_rfc7807_not_found_error(req);
}

static std::shared_ptr<RestApi> rest_api;

static void start(mysql_harness::PluginFuncEnv *env) {
  try {
    auto &http_srv = HttpServerComponent::get_instance();
    auto &rest_api_srv = RestApiComponent::get_instance();

    rest_api =
        std::make_shared<RestApi>(std::string("/api/") + kRestAPIVersion,
                                  std::string("^/api/") + kRestAPIVersion);

    rest_api->add_path("/swagger.json$", std::make_unique<RestApiSpecHandler>(
                                             rest_api, require_realm_api));

    rest_api_srv.init(rest_api);

    http_srv.add_route(rest_api->uri_prefix_regex(),
                       std::make_unique<RestApiHttpRequestHandler>(rest_api));

    wait_for_stop(env, 0);

    http_srv.remove_route(rest_api->uri_prefix_regex());
    rest_api->remove_path("/swagger.json$");
  } catch (const std::runtime_error &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

static void deinit(mysql_harness::PluginFuncEnv * /* env */) {
  // destroy the rest_api after all rest_api users are stopped.
  rest_api.reset();
}

#if defined(_MSC_VER) && defined(rest_api_EXPORTS)
/* We are building this library */
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

const char *plugin_requires[] = {
    "http_server",
};

extern "C" {
mysql_harness::Plugin DLLEXPORT harness_plugin_rest_api = {
    mysql_harness::PLUGIN_ABI_VERSION,
    mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "REST_API",
    VERSION_NUMBER(0, 0, 1),
    sizeof(plugin_requires) / sizeof(plugin_requires[0]),
    plugin_requires,  // requires
    0,
    nullptr,  // conflicts
    init,     // init
    deinit,   // deinit
    start,    // start
    nullptr,  // stop
};
}
