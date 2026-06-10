/*
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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

#ifndef ROUTER_SRC_JIT_EXECUTOR_SRC_JIT_EXECUTOR_SERVICE_HANDLERS_H_
#define ROUTER_SRC_JIT_EXECUTOR_SRC_JIT_EXECUTOR_SERVICE_HANDLERS_H_

#include "jit_executor_context_pool.h"
#include "jit_executor_debug_context_handle.h"
#include "mysqlrouter/jit_executor_component.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace jit_executor {

/**
 * Contains the handlers to be used to attend JavaScript processing requests for
 * a specific service.
 */
class ServiceHandlers : public IServiceHandlers {
 public:
  ServiceHandlers(const ServiceHandlerConfig &config);
  ServiceHandlers(const ServiceHandlers &other);
  ~ServiceHandlers() override;

  std::shared_ptr<IContextHandle> get_context(
      const std::string &debug_port = "") override;

  void release_debug_context() override;

  bool init() override;
  void teardown() override;
  std::string error() override;

  std::chrono::seconds idle_time() const override;
  uint64_t memory_units() const override;

  void set_max_heap_size(uint64_t) override;

 private:
  void init_common_context();
  void do_tear_down();
  ServiceHandlerConfig m_config;
  std::unique_ptr<CommonContext> m_common_context;
  std::shared_ptr<IContextHandle> m_debug_context;
  std::shared_ptr<ContextPool> m_context_pool;
  std::chrono::time_point<std::chrono::system_clock> m_last_used_time;
  std::string m_error;

  std::unique_ptr<std::thread> m_teardown_thread;
};

}  // namespace jit_executor

#endif  // ROUTER_SRC_JIT_EXECUTOR_SRC_JIT_EXECUTOR_SERVICE_HANDLERS_H_