/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
 * REST API to send signals to the router.
 */
#include "mysqlrouter/rest_signal_export.h"

#include <array>
#include <string>

#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/plugin_config.h"
#include "mysql/harness/utility/string.h"  // ::join()

#include "mysqlrouter/component/http_server_component.h"
#include "mysqlrouter/rest_api_component.h"

#include "rest_signal_abort.h"
IMPORT_LOG_FUNCTIONS()

class RestRouterPluginConfig : public mysql_harness::BasePluginConfig {
 public:
  std::string require_realm;

  explicit RestRouterPluginConfig(const mysql_harness::ConfigSection *section)
      : mysql_harness::BasePluginConfig(section) {}

  std::string get_default(std::string_view /* option */) const override {
    return {};
  }

  bool is_required(std::string_view /* option */) const override {
    return false;
  }
};

using JsonPointer = RestApiComponent::JsonPointer;
using JsonValue = RestApiComponent::JsonValue;

#define STR(s) \
  { s, strlen(s), rapidjson::kPointerInvalidIndex }

static const std::array<JsonPointer::Token, 2> signal_abort_def_tokens{
    {STR("definitions"), STR("SignalAbort")}};

static const std::array<JsonPointer::Token, 2> signal_abort_path_tokens{
    {STR("paths"), STR("/signal/abort")}};

static const std::array<JsonPointer::Token, 2> tags_append_tokens{
    {STR("tags"), STR("-")}};

#undef STR

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
  const RestApiComponent::JsonPointer signal_abort_def_ptr(
      signal_abort_def_tokens.data(), signal_abort_def_tokens.size());

  signal_abort_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember("properties", JsonValue(rapidjson::kObjectType),
                     allocator),
      allocator);

  std::string signal_abort_def_ptr_str =
      json_pointer_stringfy(signal_abort_def_ptr);

  // /paths/routerStatus
  {
    JsonPointer ptr(signal_abort_path_tokens.data(),
                    signal_abort_path_tokens.size());

    ptr.Set(spec_doc,
            JsonValue(rapidjson::kObjectType)
                .AddMember(
                    "get",
                    JsonValue(rapidjson::kObjectType)
                        .AddMember("tags",
                                   JsonValue(rapidjson::kArrayType)
                                       .PushBack("app", allocator),
                                   allocator)
                        .AddMember("description", "send signal to router",
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
                                                        signal_abort_def_ptr_str
                                                            .data(),
                                                        signal_abort_def_ptr_str
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

static void run(mysql_harness::PluginFuncEnv *env) {
  auto &rest_api_srv = RestApiComponent::get_instance();

  const bool spec_adder_executed = rest_api_srv.try_process_spec(spec_adder);

  std::array<RestApiComponentPath, 1> paths{{
      {rest_api_srv, RestSignalAbort::path_regex,
       std::make_unique<RestSignalAbort>()},
  }};

  mysql_harness::on_service_ready(env);

  wait_for_stop(env, 0);

  // in case rest_api never initialized, ensure the rest_api_component doesn't
  // have a callback to use
  if (!spec_adder_executed) rest_api_srv.remove_process_spec(spec_adder);
}

static const std::array<const char *, 2> rest_signal_plugin_requires = {
    "logger",
    "rest_api",
};

extern "C" {
mysql_harness::Plugin REST_SIGNAL_EXPORT harness_plugin_rest_signal = {
    mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
    mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
    "REST_SIGNAL",                           // name
    VERSION_NUMBER(0, 0, 1),
    // requires
    rest_signal_plugin_requires.size(),
    rest_signal_plugin_requires.data(),
    // conflicts
    0,
    nullptr,
    nullptr,  // init
    nullptr,  // deinit
    run,      // run
    nullptr,  // stop
    true,     // declares_readiness
    0,
    nullptr,
    nullptr,
};
}
