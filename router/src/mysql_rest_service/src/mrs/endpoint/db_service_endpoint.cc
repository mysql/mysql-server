/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "mrs/endpoint/db_service_endpoint.h"

#include <rapidjson/document.h>
#include <mutex>
#include <set>
#include <string>

#include "mrs/endpoint/content_set_endpoint.h"
#include "mrs/endpoint/handler/helper/utils_proto.h"
#include "mrs/endpoint/url_host_endpoint.h"
#include "mrs/router_observation_entities.h"
#include "mysql/harness/scoped_callback.h"
#ifdef HAVE_JIT_EXECUTOR_PLUGIN
#include "mrs/file_system/db_service_file_system.h"
#include "mysqlrouter/jit_executor_component.h"
#endif

namespace mrs {
namespace endpoint {

using DbService = DbServiceEndpoint::DbService;
using DbServicePtr = DbServiceEndpoint::DbServicePtr;
using UniversalId = DbServiceEndpoint::UniversalId;
using EndpointConfiguration = DbServiceEndpoint::EndpointConfiguration;
using EnabledType = DbServiceEndpoint::EnabledType;

DbServiceEndpoint::DbServiceEndpoint(const DbService &entry,
                                     EndpointConfigurationPtr configuration,
                                     HandlerFactoryPtr factory)
    : OptionEndpoint(entry.id, configuration, factory),
      entry_{std::make_shared<DbService>(entry)} {
#ifdef HAVE_JIT_EXECUTOR_PLUGIN
  jit_executor_config_.fs =
      std::make_shared<file_system::DbServiceFileSystem>(this);
#endif
}

DbServiceEndpoint::~DbServiceEndpoint() {
#ifdef HAVE_JIT_EXECUTOR_PLUGIN
  auto &instance = jit_executor::JitExecutorComponent::get_instance();
  instance.delete_context(get()->id.to_string());
#endif
}

UniversalId DbServiceEndpoint::get_id() const { return entry_->id; }

UniversalId DbServiceEndpoint::get_parent_id() const {
  return entry_->url_host_id;
}

const DbServicePtr DbServiceEndpoint::get() const { return entry_; }

void DbServiceEndpoint::set_debug_enabled(bool value) {
  debug_enabled_ = value;

#ifdef HAVE_JIT_EXECUTOR_PLUGIN
  if (debug_enabled_) {
    get_scripting_context();
  } else {
    auto &instance = jit_executor::JitExecutorComponent::get_instance();
    instance.stop_debug_context(get()->id.to_string());
  }
#endif
}

bool DbServiceEndpoint::is_debug_enabled() const { return debug_enabled_; }

void DbServiceEndpoint::set(const DbService &entry, EndpointBasePtr parent) {
  auto lock = std::unique_lock<std::shared_mutex>(endpoints_access_);
  entry_ = std::make_shared<DbService>(entry);
  change_parent(parent);
  changed();
}

void DbServiceEndpoint::on_updated_content_set() {
#ifdef HAVE_JIT_EXECUTOR_PLUGIN
  update_content_set_data();
#endif
}

#ifdef HAVE_JIT_EXECUTOR_PLUGIN

std::string DbServiceEndpoint::get_content_set_path(
    const std::string &module_class_name) {
  std::shared_lock lock(content_set_path_mutex_);

  if (content_set_paths_.find(module_class_name) != content_set_paths_.end()) {
    return content_set_paths_.at(module_class_name);
  }

  return "";
}

void DbServiceEndpoint::update_content_set_data() {
  std::scoped_lock lock(content_set_path_mutex_);

  std::vector<std::string> scripts;
  content_set_paths_.clear();

  for (const auto &child : get_children()) {
    auto content_set_ep = std::dynamic_pointer_cast<ContentSetEndpoint>(child);

    // We only care about content set childs
    if (!content_set_ep || !content_set_ep->get_options().has_value()) {
      continue;
    }

    std::vector<std::string> module_classes;
    content_set_ep->get_content_set_data(&scripts, &module_classes);
    if (!module_classes.empty()) {
      auto cset_url = content_set_ep->get_url().join();

      for (const auto &name : module_classes) {
        content_set_paths_[name] = cset_url;
      }
    }
  }

  if (jit_executor_config_.module_files != scripts) {
    jit_executor_config_.module_files = scripts;
    updated_jit_executor_config_ = true;
  }
}

namespace {
std::optional<uint64_t> get_memory_units(const std::string &options) {
  if (!options.empty()) {
    rapidjson::Document doc;
    doc.Parse(options.data(), options.size());

    if (doc.IsObject() && doc.HasMember("jitExecutor")) {
      const auto &node = doc["jitExecutor"];

      if (node.HasMember("memoryUnits") && node.IsUint()) {
        return node["memoryUnits"].GetUint();
      }
    }
  }

  return {};
}
}  // namespace

std::shared_ptr<jit_executor::IContextHandle>
DbServiceEndpoint::get_scripting_context() {
  std::scoped_lock lock(m_scripting_context_mutex);
  auto &instance = jit_executor::JitExecutorComponent::get_instance();
  const auto id = get()->id.to_string();

  mysql_harness::ScopedCallback callback(
      [this]() { updated_jit_executor_config_ = false; });

  return instance.get_context(
      id, jit_executor_config_,
      debug_enabled_ ? get_configuration()->get_debug_port() : "",
      updated_jit_executor_config_);
}
#endif

void DbServiceEndpoint::update() {
  Parent::update();
#ifdef HAVE_JIT_EXECUTOR_PLUGIN
  jit_executor_config_.memory_units =
      get_memory_units(get_options().value_or(""));
#endif
  observability::EntityCounter<kEntityCounterUpdatesServices>::increment();
}

void DbServiceEndpoint::activate_public() {
  url_handlers_.clear();
  auto this_ep = shared_from_this();

  url_handlers_.push_back(
      factory_->create_db_service_metadata_handler(this_ep));
  if (!get_configuration()->get_debug_port().empty()) {
    url_handlers_.push_back(factory_->create_db_service_debug_handler(this_ep));
  }
  url_handlers_.push_back(factory_->create_authentication_login(this_ep));
  url_handlers_.push_back(factory_->create_authentication_logout(this_ep));
  url_handlers_.push_back(factory_->create_authentication_completed(this_ep));
  url_handlers_.push_back(factory_->create_authentication_user(this_ep));
  url_handlers_.push_back(factory_->create_authentication_auth_apps(this_ep));
  url_handlers_.push_back(factory_->create_authentication_status(this_ep));
  url_handlers_.push_back(factory_->create_db_service_openapi_handler(this_ep));
}

void DbServiceEndpoint::deactivate() { url_handlers_.clear(); }

EnabledType DbServiceEndpoint::get_this_node_enabled_level() const {
  return entry_->enabled;
}

std::string DbServiceEndpoint::get_my_url_path_part() const {
  return entry_->url_context_root;
}

std::string DbServiceEndpoint::get_my_url_part() const {
  return entry_->url_context_root;
}

DbServiceEndpoint::Uri DbServiceEndpoint::get_url() const {
  using namespace ::mrs::endpoint::handler;
  auto parent = get_parent_ptr();
  if (parent) {
    auto parent_url = parent->get_url();
    if (!parent_url.empty()) {
      auto protocol = get_properly_configured_used_protocol(
          entry_->url_protocols, configuration_.get());
      add_protocol_to_host(protocol, &parent_url);

      parent_url.set_path(parent_url.get_path() + get_my_url_part());
      return parent_url;
    }
  }

  Uri result;
  result.set_path(get_my_url_part());
  return result;
}

bool DbServiceEndpoint::does_this_node_require_authentication() const {
  return false;
}

std::optional<std::string> DbServiceEndpoint::get_options() const {
  return entry_->options;
}

std::string DbServiceEndpoint::get_extra_update_data() {
  return std::string(", published:") + (entry_->published ? "yes" : "no");
}

}  // namespace endpoint
}  // namespace mrs
