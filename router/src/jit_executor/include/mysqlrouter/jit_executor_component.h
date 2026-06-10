/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_COMPONENT_H_
#define ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_COMPONENT_H_

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mysqlrouter/jit_executor_context_handle.h"
#include "mysqlrouter/jit_executor_plugin_export.h"
#include "mysqlrouter/jit_executor_value.h"
#include "mysqlrouter/polyglot_file_system.h"

namespace jit_executor {

static const uint64_t k_default_memory_units = 10;
struct GlobalConfig {
  bool operator==(const GlobalConfig &o) const {
    return std::tie(maximum_ram_size, maximum_idle_time) ==
           std::tie(o.maximum_ram_size, o.maximum_idle_time);
  }

  std::optional<uint64_t> maximum_ram_size;
  std::optional<uint64_t> maximum_idle_time;
};

struct ServiceHandlerConfig {
  std::shared_ptr<shcore::polyglot::IFile_system> fs;
  std::vector<std::string> module_files;
  shcore::Dictionary_t globals;
  std::optional<uint64_t> memory_units;
  std::optional<uint64_t> max_heap_size;
};

/** Interface defining central location for the handlers associated to a
 * database service
 */
class IServiceHandlers {
 public:
  virtual ~IServiceHandlers() = default;
  virtual std::shared_ptr<IContextHandle> get_context(
      const std::string &debug_port = "") = 0;

  virtual void release_debug_context() = 0;

  virtual bool init() = 0;
  virtual std::string error() = 0;
  virtual void teardown() = 0;

  virtual std::chrono::seconds idle_time() const = 0;
  virtual uint64_t memory_units() const = 0;

  virtual void set_max_heap_size(uint64_t) = 0;
};

/**
 * Registry of graal contexts to be used by each service.
 *
 * NOTE: The original idea, was to have a pool of contexts on which the service
 * module files would be loaded once and then shared across the contexts in the
 * pool. The script end points would be getting a context from the pool use it
 * and the release it. However, the main pre-requisite for that is that the
 * context could be reset to the original state, which is NOT possible in Graal.
 *
 * Suggestion from the Graal Team
 *
 * By default, each context would internally create an engine which would hold
 * the resources used in the context. However, it is possible to use a common
 * engine to enable the sharing of the resources  across contexts (i.e.
 * including parsed source code). Following this approach the context pool is
 * not needed since we would simply create/release the context on demand and it
 * would use the shared resources from the engine.
 *
 * Even this is the current implementation, expectation was that the module
 * files would be loaded only ONCE but that's not the case, they get reloaded on
 * every created context, even the shared engine is used.
 *
 * This class holds a registry of service ids vs ContextHandlers (who keep the
 * shared engine) and allows creating a context using the shared engine.
 */
class JIT_EXECUTOR_PLUGIN_EXPORT JitExecutorComponent {
 public:
  static JitExecutorComponent &get_instance();
  ~JitExecutorComponent();

  JitExecutorComponent(JitExecutorComponent const &) = delete;
  void operator=(JitExecutorComponent const &) = delete;

  JitExecutorComponent(JitExecutorComponent &&) = delete;
  void operator=(JitExecutorComponent &&) = delete;

  void stop_debug_context(const std::string &service_id);

  std::shared_ptr<IContextHandle> get_context(
      const std::string &service_id, const ServiceHandlerConfig &config,
      const std::string &debug_port = "", bool reset_context = false);

  void delete_context(const std::string &service_id);

  void update_global_config(const std::string &options);

 private:
  JitExecutorComponent() = default;
  void update_active_contexts(
      const std::pair<std::string, std::shared_ptr<IServiceHandlers>>
          &replacement = {});
  std::mutex m_context_creation;

  GlobalConfig m_global_config;

  std::unordered_map<std::string, std::shared_ptr<IServiceHandlers>>
      m_service_context_handlers;

  std::unordered_map<std::string, std::string> m_handler_errors;

  std::vector<std::shared_ptr<IServiceHandlers>> m_inactive_context_handlers;
};

}  // namespace jit_executor

#endif  // ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_COMPONENT_H_
