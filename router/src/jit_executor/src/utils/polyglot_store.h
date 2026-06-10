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

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_UTILS_POLYGLOT_STORE_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_UTILS_POLYGLOT_STORE_H_

#include <cstddef>
#include <mutex>
#include <unordered_map>

#include "utils/polyglot_api_clean.h"

namespace shcore {
namespace polyglot {

/**
 * Helper class to handle polyglot values made permanent.
 *
 * An instance of this class will create a polyglot reference for the given
 * value, making it permanent and available even across contexts.
 *
 * On destruction the created reference will be removed making the value subject
 * for garbage collection if no additional references are held.
 */
class Store final {
 public:
  Store() = default;

  explicit Store(poly_thread thread) noexcept : Store(thread, nullptr) {}

  Store(poly_thread thread, poly_handle object);

  Store(const Store &other) = delete;

  Store &operator=(const Store &other) = delete;

  Store(Store &&other) noexcept;
  Store &operator=(Store &&other) noexcept;

  ~Store() noexcept { reset(false); }

  Store &reset(bool throw_on_error = true);

  Store &operator=(std::nullptr_t) { return reset(false); }

  explicit operator bool() const { return nullptr != m_reference; }

  poly_reference get() const noexcept { return m_reference; }

  poly_thread thread() const { return m_thread; }

 private:
  poly_thread m_thread = nullptr;
  poly_reference m_reference = nullptr;
};

/**
 * A container for stored values
 */
class Polyglot_storage final {
 public:
  explicit Polyglot_storage(poly_thread thread);
  ~Polyglot_storage();

  poly_reference store(poly_handle value);
  void erase(poly_reference value);

  void clear();

 private:
  poly_thread m_thread;
  std::mutex m_mutex;
  std::unordered_map<poly_reference, Store> m_stored_values;
  bool m_cleared = false;
};

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_UTILS_POLYGLOT_STORE_H_
