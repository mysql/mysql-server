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

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_COLLECTABLE_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_COLLECTABLE_H_

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

namespace shcore {
namespace polyglot {

enum class Collectable_type {
  OBJECT,
  FUNCTION,
  METHOD,
  ARRAY,
  MAP,
  ITERATOR,
};

class Polyglot_language;
class Collectable_registry;

/**
 * Base collectable interface to be able to determine the type of a collectable
 * object without with no need to cast it.
 */
class ICollectable {
 public:
  ICollectable(Collectable_type type,
               std::weak_ptr<Polyglot_language> language);
  virtual ~ICollectable() = default;
  Collectable_type type() const { return m_type; }
  std::shared_ptr<Polyglot_language> language() const;
  Collectable_registry *registry() const;

 private:
  Collectable_type m_type;
  std::weak_ptr<Polyglot_language> m_language;
  Collectable_registry *m_registry;
};

/**
 * Represents a data object to be associated to a Polyglot wrapper for C++
 * elements. It contains the necessary to:
 *
 * - Identify the target object type
 * - Identify the target object itself
 * - Context information to perform Polyglot related operations
 */
template <typename T, Collectable_type t>
class Collectable : public ICollectable {
 public:
  Collectable(const std::shared_ptr<T> &d,
              std::weak_ptr<Polyglot_language> language)
      : ICollectable(t, std::move(language)), m_data(d) {}

  Collectable(const Collectable &) = delete;
  Collectable(Collectable &&) = delete;

  Collectable &operator=(const Collectable &) = delete;
  Collectable &operator=(Collectable &&) = delete;

  ~Collectable() override = default;

  const std::shared_ptr<T> &data() const { return m_data; }

 private:
  std::shared_ptr<T> m_data;
};

/**
 * When a Polyglot wrapper for a C++ object is created, a collectable instance
 * is created to be passed to the Polyglot object (Java). The memory associated
 * to the collectable must be released when it is no longer needed in the Java
 * side.
 *
 * Each collectable will be registered here and marked to be deleted under the
 * following scenarios:
 *
 * - When the Java Garbage Collector marks the object as a phantom reference.
 * - When the Java context is being finalized
 *
 * The memory is released after calling clean() method.
 */
class Collectable_registry {
 public:
  Collectable_registry() = default;

  Collectable_registry(const Collectable_registry &) = delete;
  Collectable_registry(Collectable_registry &&) = delete;

  Collectable_registry &operator=(const Collectable_registry &) = delete;
  Collectable_registry &operator=(Collectable_registry &&) = delete;

  ~Collectable_registry();

  void add(ICollectable *target);
  void remove(ICollectable *target);

  /**
   * Deletes all collectables which were marked as to be deleted.
   */
  void clean();

 private:
  void clean_unsafe();

  std::mutex m_cleanup_mutex;
  std::unordered_set<ICollectable *> m_live_collectables;
  std::vector<ICollectable *> m_phantom_collectables;
};

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_COLLECTABLE_H_
