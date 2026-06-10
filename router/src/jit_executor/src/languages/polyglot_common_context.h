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

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_LANGUAGES_POLYGLOT_COMMON_CONTEXT_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_LANGUAGES_POLYGLOT_COMMON_CONTEXT_H_

#include "utils/polyglot_api_clean.h"

#include <memory>
#include <optional>

#include "languages/polyglot_garbage_collector.h"
#include "native_wrappers/polyglot_collectable.h"
#include "utils/polyglot_scope.h"
#include "utils/polyglot_store.h"

namespace shcore {
namespace polyglot {

struct IsolateArgs {
 public:
  std::optional<uint64_t> min_heap_size;
  std::optional<uint64_t> max_heap_size;
  std::optional<uint64_t> max_new_size;
};

/**
 * Common context for GraalVM Languages
 *
 * Encloses the following global routines/components:
 *
 * - Initialization/TearDown of the GraalVM Isolate.
 * - Registration of global logging callbacks.
 * - Setup of the global engine.
 * - Provides access to the garbage collector for event registration.
 *
 * This class can be specialized to:
 *
 * - Provide custom callbacks for the global logging functions.
 * - Create a custom Engine to be used in the contexts created from this class.
 * - Provide custom configuration for the garbage collector.
 */
class Polyglot_common_context {
 public:
  Polyglot_common_context() = default;
  virtual ~Polyglot_common_context() = default;

  virtual void initialize(const IsolateArgs &isolate_args);
  virtual void finalize();

  poly_reference engine() const { return m_engine.get(); }
  poly_isolate isolate() const { return m_isolate; }
  poly_thread thread() const { return m_thread; }

  void clean_collectables();

  Collectable_registry *collectable_registry() { return &m_registry; }

  double get_heap_usage_percent();
  std::string get_gc_status();

 protected:
  poly_isolate m_isolate = nullptr;
  poly_thread m_thread = nullptr;

 private:
  void init_engine();
  static void fatal_error_callback(void *data);
  static void flush_callback(void *data);
  static void log_callback(const char *bytes, size_t length, void *data);
  virtual void fatal_error() = 0;
  virtual void flush() = 0;
  virtual void log(const char *bytes, size_t length) = 0;
  virtual poly_engine create_engine() { return nullptr; }
  virtual Garbage_collector::Config gc_config() { return {}; }

  Store m_engine;
  std::unique_ptr<Polyglot_scope> m_scope;
  Collectable_registry m_registry;
  int64_t m_max_heap_size = 0;
  int64_t *m_heap_status = nullptr;

  // Holds the used capacity for each generation
  std::vector<int64_t *> m_generation_used;
};

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_LANGUAGES_POLYGLOT_COMMON_CONTEXT_H_