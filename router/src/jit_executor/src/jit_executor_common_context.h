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

#ifndef ROUTER_SRC_JIT_EXECUTOR_SRC_JIT_EXECUTOR_COMMON_CONTEXT_H_
#define ROUTER_SRC_JIT_EXECUTOR_SRC_JIT_EXECUTOR_COMMON_CONTEXT_H_

#include "utils/polyglot_api_clean.h"

#include <atomic>

#include "jit_executor_javascript.h"
#include "languages/polyglot_common_context.h"
#include "mysqlrouter/polyglot_file_system.h"

namespace jit_executor {

/**
 * Specialization of the Polyglot_common_context to provide MRS specific
 * logging functions as well as the MRS specific engine to be used across the
 * different contexts.
 *
 * NOTE: Using a shared engine across contexts is meant to enable code sharing,
 * however, even it is enabled, the module files are being loaded on every
 * created context.
 */
class CommonContext : public shcore::polyglot::Polyglot_common_context {
 public:
  CommonContext(const std::shared_ptr<shcore::polyglot::IFile_system> &fs,
                const std::vector<std::string> &module_files,
                const shcore::Dictionary_t &globals,
                const shcore::polyglot::IsolateArgs &isolate_args);
  ~CommonContext() override;

  void initialize(const shcore::polyglot::IsolateArgs &isolate_args) override;
  void finalize() override;
  bool start();
  bool got_fatal_error() const { return m_fatal_error; }
  std::string error() const {
    return m_fatal_error_description.empty()
               ? "Got fatal error initializing GraalVM"
               : m_fatal_error_description;
  }

  const std::shared_ptr<shcore::polyglot::IFile_system> &file_system() const {
    return m_file_system;
  }
  const shcore::Dictionary_t &globals() const { return m_globals; }

 private:
  void life_cycle_thread();
  void fatal_error() override;
  void flush() override;
  void log(const char *bytes, size_t length) override;

  shcore::polyglot::Garbage_collector::Config gc_config() override {
    return {10, {}, 10};
  }

  /**
   * Creates the shared engine for the different contexts created with this
   * handler.
   *
   * Returning nullptr (or removing this implementation) would enable the
   * default behavior for Graal which is to create an engine for every context.
   */
  poly_engine create_engine() override;

  std::shared_ptr<JavaScript> m_base_context;
  std::shared_ptr<shcore::polyglot::IFile_system> m_file_system;
  std::vector<std::string> m_module_files;
  std::vector<shcore::polyglot::Store> m_cached_sources;
  shcore::Dictionary_t m_globals;

  std::unique_ptr<std::thread> m_life_cycle_thread;
  std::mutex m_mutex;
  std::condition_variable m_init_condition;

  bool m_initialized = false;
  bool m_terminated = false;

  std::mutex m_finish_mutex;
  std::condition_variable m_finish_condition;

  // Global fatal error flag to indicate when the VM was ended
  static bool m_global_fatal_error;
  std::atomic_bool m_fatal_error = false;
  std::string m_fatal_error_description;
  shcore::polyglot::IsolateArgs m_isolate_args;
};

}  // namespace jit_executor

#endif  // ROUTER_SRC_JIT_EXECUTOR_SRC_JIT_EXECUTOR_COMMON_CONTEXT_H_