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

#include "native_wrappers/polyglot_collectable.h"

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <utility>

#include "languages/polyglot_language.h"

namespace shcore {
namespace polyglot {

ICollectable::ICollectable(Collectable_type type,
                           std::weak_ptr<Polyglot_language> language)
    : m_type{type}, m_language{std::move(language)} {
  auto language_ptr = m_language.lock();
  if (language_ptr) {
    m_registry = language_ptr->common_context()->collectable_registry();
  } else {
    throw std::logic_error(
        "Unable to create a collectable if the language is not available!");
  }
}

std::shared_ptr<Polyglot_language> ICollectable::language() const {
  return m_language.lock();
}

Collectable_registry *ICollectable::registry() const { return m_registry; }

Collectable_registry::~Collectable_registry() {
  for (const auto collectable : m_live_collectables) {
    delete collectable;
  }

  clean_unsafe();
}

void Collectable_registry::add(ICollectable *target) {
  std::unique_lock lock(m_cleanup_mutex);
  m_live_collectables.emplace(target);
}

void Collectable_registry::remove(ICollectable *target) {
  std::unique_lock lock(m_cleanup_mutex);

  if (const auto collectable = m_live_collectables.find(target);
      collectable != m_live_collectables.end()) {
    m_live_collectables.erase(collectable);
    m_phantom_collectables.emplace_back(target);
  }
}

void Collectable_registry::clean() {
  std::unique_lock lock(m_cleanup_mutex);
  clean_unsafe();
}

void Collectable_registry::clean_unsafe() {
  for (const auto collectable : m_phantom_collectables) {
    delete collectable;
  }

  m_phantom_collectables.clear();
}

}  // namespace polyglot
}  // namespace shcore
