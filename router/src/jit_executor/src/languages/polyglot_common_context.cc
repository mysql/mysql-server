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

#include "languages/polyglot_common_context.h"

#include <cstring>
#include <numeric>

#include "mysql/harness/scoped_callback.h"
#include "native_wrappers/polyglot_collectable.h"
#include "utils/polyglot_scope.h"
#include "utils/polyglot_utils.h"
#include "utils/utils_string.h"

namespace shcore {
namespace polyglot {

namespace {
std::vector<std::string> format_isolate_args(const IsolateArgs &args) {
  std::vector<std::string> str_isolate_args;

  if (args.min_heap_size.has_value()) {
    str_isolate_args.push_back("-Xms" + std::to_string(*args.min_heap_size) +
                               "m");
  }

  if (args.max_heap_size.has_value()) {
    str_isolate_args.push_back("-Xmx" + std::to_string(*args.max_heap_size) +
                               "m");
  }

  if (args.max_new_size.has_value()) {
    str_isolate_args.push_back("-Xmn" + std::to_string(*args.max_new_size) +
                               "m");
  }

  return str_isolate_args;
}
}  // namespace

void Polyglot_common_context::initialize(const IsolateArgs &isolate_args) {
  auto str_isolate_args = format_isolate_args(isolate_args);

  if (!str_isolate_args.empty()) {
    std::vector<char *> raw_isolate_args = {nullptr};
    for (const auto &arg : str_isolate_args) {
      raw_isolate_args.push_back(const_cast<char *>(arg.data()));
    }

    auto params = raw_isolate_args.data();
    poly_isolate_params isolate_params;
    if (poly_ok != poly_set_isolate_params(
                       &isolate_params, str_isolate_args.size() + 1, params)) {
      throw Polyglot_generic_error("Error creating polyglot isolate params");
    }

    if (const auto rc =
            poly_create_isolate(&isolate_params, &m_isolate, &m_thread);
        rc != poly_ok) {
      throw Polyglot_generic_error(
          shcore::str_format("Error creating polyglot isolate: %d (%s)", rc,
                             shcore::str_join(str_isolate_args, " ").c_str()));
    }
  } else {
    if (const auto rc = poly_create_isolate(NULL, &m_isolate, &m_thread);
        rc != poly_ok) {
      throw Polyglot_generic_error(
          shcore::str_format("Error creating polyglot isolate: %d", rc));
    }
  }

  m_scope = std::make_unique<Polyglot_scope>(m_thread);

  if (const auto rc = poly_register_log_handler_callbacks(
          m_thread, &log_callback, &flush_callback, &fatal_error_callback,
          static_cast<void *>(this));
      rc != poly_ok) {
    throw Polyglot_error(m_thread, rc);
  }

  // Initialize the engine to be used in the context builder, not defining a
  // custom engine (i.e. getting nullptr as result) enforces the default logic
  // on GraalVM which is triggers the creation of a custom engine per created
  // context.
  init_engine();

#ifdef _WIN32
  // The performance monitoring is not available in windows
  // so we take the configured max heap size
  if (isolate_args.max_heap_size.has_value()) {
    m_max_heap_size = *isolate_args.max_heap_size;
  }
#else
  // Registers long constant address to be able to poll the garbage collector
  // status.
  std::string heap_status_key = "com.oracle.svm.gcInProgress";
  poly_perf_data_get_address_of_int64_t(thread(), heap_status_key.data(),
                                        &m_heap_status);

  // Registers long constant to identify the number of generations being used
  int64_t *generations_ptr = nullptr;
  std::string generations_key = "sun.gc.policy.generations";
  poly_perf_data_get_address_of_int64_t(thread(), generations_key.data(),
                                        &generations_ptr);

  if (generations_ptr != nullptr) {
    const auto generations = *generations_ptr;

    m_generation_used.resize(generations);

    for (auto generation = 0; generation < generations; generation++) {
      m_generation_used[generation] = nullptr;

      const auto str_generation = std::to_string(generation);

      // Registers long constant to identify the max capacity of the generation
      int64_t *generation_max_ptr = nullptr;
      auto max_capacity_key =
          "sun.gc.generation." + str_generation + ".maxCapacity";
      if (poly_ok ==
          poly_perf_data_get_address_of_int64_t(
              thread(), max_capacity_key.data(), &generation_max_ptr)) {
        // Sometimes the performance counter initialization is delayed,
        // returning 0 as value, we need to wait until it is initialized to
        // properly get the amount memory capacity
        do {
          m_max_heap_size += *generation_max_ptr;
        } while (0 == *generation_max_ptr);

        // Registers long constant to enable polling the actual use of the
        // generation
        auto capacity_key = "sun.gc.generation." + str_generation + ".capacity";
        poly_perf_data_get_address_of_int64_t(thread(), capacity_key.data(),
                                              &m_generation_used[generation]);
      }
    }
  }
#endif
}

double Polyglot_common_context::get_heap_usage_percent() {
#ifndef _WIN32
  auto used =
      std::accumulate(m_generation_used.cbegin(), m_generation_used.cend(),
                      uint64_t{0}, [](auto a, const auto generation) {
                        return a + (generation ? *generation : 0);
                      });

  return (100.0 * used) / m_max_heap_size;
#else
  return 0.0;
#endif
}

std::string Polyglot_common_context::get_gc_status() {
  if (m_heap_status != nullptr) {
    if (*m_heap_status == 1)
      return "Running";
    else if (*m_heap_status == 0)
      return "Idle";
  }

  return "Unknown";
}

void Polyglot_common_context::finalize() {
  m_engine.reset();

  m_scope.reset();

  if (m_isolate && m_thread) {
    if (const auto rc = poly_detach_all_threads_and_tear_down_isolate(m_thread);
        rc != poly_ok) {
      std::string error{"polyglot error while tearing down the isolate"};
      log(error.data(), error.size());
    }
  }

  clean_collectables();
}

void Polyglot_common_context::fatal_error_callback(void *data) {
  auto self = static_cast<Polyglot_common_context *>(data);
  self->fatal_error();
}

void Polyglot_common_context::flush_callback(void *data) {
  auto self = static_cast<Polyglot_common_context *>(data);
  self->flush();
}

void Polyglot_common_context::log_callback(const char *bytes, size_t length,
                                           void *data) {
  auto self = static_cast<Polyglot_common_context *>(data);
  self->log(bytes, length);
}

void Polyglot_common_context::init_engine() {
  auto engine = create_engine();
  if (engine) {
    m_engine = Store(m_thread, engine);
  }
}

void Polyglot_common_context::clean_collectables() { m_registry.clean(); }

}  // namespace polyglot
}  // namespace shcore