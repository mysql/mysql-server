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

#include "mysqlrouter/jit_executor_component.h"

#include "my_rapidjson_size_t.h"

#include <rapidjson/document.h>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "jit_executor_common_context.h"
#include "jit_executor_javascript_context.h"
#include "jit_executor_service_handlers.h"
#include "mysqlrouter/jit_executor_context_handle.h"
#include "mysqlrouter/polyglot_file_system.h"
#include "utils/polyglot_utils.h"

namespace jit_executor {

JitExecutorComponent &JitExecutorComponent::get_instance() {
  static JitExecutorComponent instance;

  return instance;
}

JitExecutorComponent::~JitExecutorComponent() {
  m_inactive_context_handlers.clear();
  m_service_context_handlers.clear();
}

namespace {
std::optional<uint64_t> get_value(const rapidjson::Value &value,
                                  const char *name) {
  if (value.HasMember(name) && value[name].IsUint64()) {
    return value[name].GetUint64();
  }

  return {};
}

}  // namespace

void JitExecutorComponent::update_global_config(const std::string &options) {
  GlobalConfig config;
  if (!options.empty()) {
    rapidjson::Document doc;
    doc.Parse(options.data(), options.size());
    if (doc.IsObject() && doc.HasMember("jitExecutor")) {
      const auto &jit_executor = doc["jitExecutor"];
      if (jit_executor.IsObject()) {
        config.maximum_ram_size = get_value(jit_executor, "maximumRamUsage");
        config.maximum_idle_time = get_value(jit_executor, "maximumIdleTime");
      }
    }
  }

  if (m_global_config != config) {
    std::unique_lock<std::mutex> lock(m_context_creation);
    m_global_config = config;
    update_active_contexts();
  }
}

void JitExecutorComponent::stop_debug_context(const std::string &service_id) {
  auto it = m_service_context_handlers.find(service_id);
  if (it != m_service_context_handlers.end()) {
    it->second->release_debug_context();
  }
}

void JitExecutorComponent::update_active_contexts(
    const std::pair<std::string, std::shared_ptr<IServiceHandlers>>
        &replacement) {
  std::unordered_map<std::string, std::shared_ptr<IServiceHandlers>>
      all_context_handlers = std::move(m_service_context_handlers);

  std::unordered_map<std::string, std::shared_ptr<IServiceHandlers>>
      candidate_context_handlers;

  uint64_t total_memory_units = 0;
  for (const auto &it : all_context_handlers) {
    // Adds the existing context handler to be discarded
    it.second->teardown();
    m_inactive_context_handlers.push_back(it.second);

    // Creates an updated context handler if applicable
    if (replacement.first != it.first &&
        (!m_global_config.maximum_idle_time.has_value() ||
         static_cast<uint64_t>(it.second->idle_time().count()) <
             *m_global_config.maximum_idle_time)) {
      // Adds to the global count of pool items
      total_memory_units += it.second->memory_units();

      // Creates a new handler from the existing one
      auto source_handler =
          std::dynamic_pointer_cast<ServiceHandlers>(it.second);
      candidate_context_handlers.emplace(
          it.first, std::make_shared<ServiceHandlers>(*source_handler.get()));
    }
  }

  // Adds the replacement handler to the total pool size
  if (!replacement.first.empty()) {
    total_memory_units += replacement.second->memory_units();

    candidate_context_handlers.emplace(std::move(replacement));
  }

  // Now updates the memory limit for each active handler and starts it
  // Total pool is verified here as it could be 0, meaning, no active context
  // handlers will be left after this update
  if (total_memory_units != 0 && m_global_config.maximum_ram_size.has_value()) {
    // Use double to get the most accurate value per pool item
    double mem_per_pool_item = *m_global_config.maximum_ram_size /
                               static_cast<double>(total_memory_units);

    for (const auto &it : candidate_context_handlers) {
      it.second->set_max_heap_size(
          static_cast<uint64_t>(mem_per_pool_item * it.second->memory_units()));
    }
  }

  for (const auto &it : candidate_context_handlers) {
    if (it.second->init()) {
      m_service_context_handlers.emplace(it.first, it.second);
    } else {
      m_handler_errors.emplace(it.first, it.second->error());
    }
  }
}

std::shared_ptr<IContextHandle> JitExecutorComponent::get_context(
    const std::string &service_id, const ServiceHandlerConfig &config,
    const std::string &debug_port, bool reset_context) {
  std::unique_lock<std::mutex> lock(m_context_creation);

  while (true) {
    try {
      auto it = m_service_context_handlers.find(service_id);
      if (it == m_service_context_handlers.end() || reset_context) {
        update_active_contexts(
            {service_id, std::make_shared<ServiceHandlers>(config)});

        it = m_service_context_handlers.find(service_id);
      }

      if (it != m_service_context_handlers.end()) {
        return it->second->get_context(debug_port);
      } else if (auto error_it = m_handler_errors.find(service_id);
                 error_it != m_handler_errors.end()) {
        throw std::runtime_error(error_it->second);
      } else {
        throw std::runtime_error("error to go below..., needed?");
      }
    } catch (...) {
      // If failed to create a context, then let's try re-creating the whole
      // pool, if this failed on a brand new pool, then there's nothing else to
      // be done
      if (!reset_context) {
        reset_context = true;
      } else {
        throw;
      }
    }
  }

  return {};
}

void JitExecutorComponent::delete_context(const std::string &service_id) {
  auto it = m_service_context_handlers.find(service_id);

  if (it != m_service_context_handlers.end()) {
    it->second->teardown();
    m_service_context_handlers.erase(it);
  }
}

}  // namespace jit_executor