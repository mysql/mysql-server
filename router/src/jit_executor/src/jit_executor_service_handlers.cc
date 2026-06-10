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

#include "jit_executor_service_handlers.h"

#include <cinttypes>
#include <memory>
#include <vector>

#include "include/my_thread.h"
#include "mysql/harness/logging/logging.h"
#include "utils/utils_system.h"

namespace jit_executor {

static const size_t k_max_heap_address_space = 32768;

IMPORT_LOG_FUNCTIONS()

ServiceHandlers::ServiceHandlers(const ServiceHandlerConfig &config)
    : m_config{config} {}

ServiceHandlers::ServiceHandlers(const ServiceHandlers &other)
    : ServiceHandlers(other.m_config) {}

bool ServiceHandlers::init() {
  m_error.clear();

  init_common_context();

  if (m_common_context->start()) {
    m_context_pool = std::make_shared<ContextPool>(m_common_context.get());

    return true;
  } else {
    m_error = m_common_context->error();
  }

  return false;
}

void ServiceHandlers::init_common_context() {
  shcore::polyglot::IsolateArgs isolate_args;

  // System memory in MB
  static const auto total_memory =
      shcore::getPhysicalMemorySize() / 1024 / 1024;

  if (total_memory > 0) {
    // Default: 25% of the system memory
    static const uint64_t default_max_heap_size = total_memory * 0.25;
    auto max_heap_size = m_config.max_heap_size.value_or(default_max_heap_size);

    // Serial GC would use max 80% of system memory
    auto graal_default_max_heap_size =
        static_cast<uint64_t>(total_memory * 0.8);

    // 256 MB is the minimum allowed
    if (max_heap_size < 256) {
      log_warning(
          "The configured maximumRamUsage=%" PRIu64
          " is lower than the minimum allowed value of 256MB, ignoring "
          "configuration, using default (25%% of the system memory: %" PRIu64
          "MB).",
          max_heap_size, default_max_heap_size);
      max_heap_size = default_max_heap_size;
    } else if (max_heap_size > k_max_heap_address_space) {
      // The specified maximum heap size (64g) must not be larger than the
      // largest possible heap address space (32g). To allow larger values, we
      // should disable compressed references when building the image by adding
      // the option '-H:-UseCompressedReferences'

      log_warning(
          "Limiting the maximumRamUsage to the largest possible heap space of "
          "32GB.");
      max_heap_size = k_max_heap_address_space;
    }

    if (max_heap_size > graal_default_max_heap_size) {
      // Convert the value to gigabytes
      log_warning("The configured maximumRamUsage=%" PRIu64
                  " exceeds the maximum allowed value %" PRIu64
                  " (80%% of the system memory %" PRIu64
                  "GB) ignoring configuration, using max RAM possible.",
                  max_heap_size, graal_default_max_heap_size,
                  total_memory / 1024);

      max_heap_size = graal_default_max_heap_size;
    }

    isolate_args.max_heap_size = max_heap_size;
  } else {
    // Not expected to happen, just in case!
    if (m_config.max_heap_size.has_value()) {
      isolate_args.max_heap_size = m_config.max_heap_size;
      log_warning(
          "Unable to retrieve the available system memory, using the "
          "configured value of maximumRamUsage=%" PRIu64,
          *isolate_args.max_heap_size);
    }
  }

  m_common_context = std::make_unique<CommonContext>(
      m_config.fs, m_config.module_files, m_config.globals, isolate_args);
}

ServiceHandlers::~ServiceHandlers() {
  if (m_teardown_thread) {
    m_teardown_thread->join();
    m_teardown_thread.reset();
  } else {
    do_tear_down();
  }
}

void ServiceHandlers::teardown() {
  m_teardown_thread =
      std::make_unique<std::thread>(&ServiceHandlers::do_tear_down, this);
}

std::string ServiceHandlers::error() { return m_error; }

void ServiceHandlers::do_tear_down() {
  my_thread_self_setname("Jit-TearDown");
  if (m_context_pool) {
    m_context_pool->teardown();
    m_context_pool.reset();
  }
  m_common_context.reset();
}

std::chrono::seconds ServiceHandlers::idle_time() const {
  auto now = std::chrono::system_clock::now();
  auto diff = m_last_used_time - now;
  return std::chrono::duration_cast<std::chrono::seconds>(diff);
}

uint64_t ServiceHandlers::memory_units() const {
  return m_config.memory_units.value_or(k_default_memory_units);
}

void ServiceHandlers::set_max_heap_size(uint64_t size) {
  m_config.max_heap_size = size;
}

std::shared_ptr<IContextHandle> ServiceHandlers::get_context(
    const std::string &debug_port) {
  if (m_common_context->got_fatal_error()) {
    throw std::runtime_error(
        "A fatal error prevents the usage of scripting endpoints");
  }

  m_last_used_time = std::chrono::system_clock::now();

  if (debug_port.empty()) {
    return m_context_pool->get_context();
  }

  if (!m_debug_context) {
    m_debug_context = std::make_shared<DebugContextHandle>(
        debug_port, m_common_context.get());
  }

  return m_debug_context;
}

void ServiceHandlers::release_debug_context() { m_debug_context.reset(); }

}  // namespace jit_executor
