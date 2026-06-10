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

#include "utils/polyglot_store.h"

#include <functional>
#include <utility>

#include "utils/polyglot_api_clean.h"

#include "utils/polyglot_error.h"
#include "utils/polyglot_utils.h"

#include "mysql/harness/logging/logging.h"

namespace shcore {
namespace polyglot {

IMPORT_LOG_FUNCTIONS()

Store::Store(poly_thread thread, poly_handle object) : m_thread{thread} {
  if (nullptr != object) {
    throw_if_error(poly_create_reference, m_thread, object, &m_reference);
  }
}

Store &Store::operator=(Store &&other) noexcept {
  reset(false);

  m_thread = std::exchange(other.m_thread, nullptr);
  m_reference = std::exchange(other.m_reference, nullptr);

  return *this;
}

Store::Store(Store &&other) noexcept { *this = std::move(other); }

Store &Store::reset(bool throw_on_error) {
  if (m_reference) {
    if (const auto rc = poly_delete_reference(m_thread, m_reference);
        rc != poly_ok) {
      auto exception = Polyglot_error(m_thread, rc);
      if (throw_on_error) {
        throw exception;
      } else {
        log_error("polyglot error deleting stored reference: %s",
                  exception.format().c_str());
      }
    }
    m_reference = nullptr;
  }

  m_thread = nullptr;

  return *this;
}

Polyglot_storage::Polyglot_storage(poly_thread thread) : m_thread{thread} {}

poly_reference Polyglot_storage::store(poly_handle value) {
  std::lock_guard lock{m_mutex};

  assert(!m_cleared);

  auto stored = Store(m_thread, value);
  auto result = m_stored_values.emplace(stored.get(), std::move(stored));

  if (!result.second) {
    throw std::logic_error("Unable to save stored polyglot handle!");
  }

  return result.first->first;
}

void Polyglot_storage::erase(poly_reference value) {
  std::lock_guard lock{m_mutex};

  if (m_cleared) {
    return;
  }

  if (const auto it = m_stored_values.find(value);
      it == m_stored_values.end()) {
    throw std::logic_error("Attempt to delete an unknown reference in store!");
  } else {
    m_stored_values.erase(it);
  }
}

Polyglot_storage::~Polyglot_storage() { clear(); }

void Polyglot_storage::clear() {
  std::lock_guard lock{m_mutex};

  if (m_cleared) {
    return;
  }

  m_stored_values.clear();
  m_cleared = true;
}

}  // namespace polyglot
}  // namespace shcore
