/*****************************************************************************

Copyright (c) 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ut0object_cache.h
Manage a cache of objects. */

#ifndef ut0object_cache_h
#define ut0object_cache_h

#include "ut0new.h"

namespace ut {

/** A class to manage objects of type T. */
template <typename T>
class Object_cache {
 public:
  /** Destructor. Frees the cached objects. */
  ~Object_cache() {
    for (auto obj : m_objects) {
      ut::delete_(obj);
    }
  }

  /** Initialize the cache.
  @param[in]  size   initial number of objects to cache.
  @param[in]  step   when extending cache, number of objects to add.
  @param[in]  args   arguments to be passed to constructor of T */
  template <typename... Types>
  dberr_t init(size_t size, size_t step, Types &&...args) {
    m_step = step;
    return extend(size, std::forward<Types>(args)...);
  }

  template <typename... Types>
  T *allocate(Types &&...args) {
    if (m_index == m_objects.size()) {
      extend(m_step, std::forward<Types>(args)...);
    }
    /* Could set m_objects[m_index] to nullptr, but not required. */
    return m_objects[m_index++];
  }

  template <typename... Types>
  dberr_t extend(size_t size, Types &&...args) {
    m_objects.reserve(m_objects.size() + size);
    for (size_t i = 0; i < size; ++i) {
      auto obj = ut::new_withkey<T>(UT_NEW_THIS_FILE_PSI_KEY,
                                    std::forward<Types>(args)...);
      if (obj == nullptr) {
        /* dtor will free all allocated objects. */
        return DB_OUT_OF_MEMORY;
      }
      m_objects.push_back(obj);
    }
    return DB_SUCCESS;
  }

  void deallocate(T *obj) {
    ut_ad(m_index > 0);
    m_objects[--m_index] = obj;
  }

 private:
  /** Cached objects. */
  std::vector<T *> m_objects;

  /** When the cache is extended, how many new objects needs to be created. */
  size_t m_step{1};

  /** Position of next object to be allocated. */
  size_t m_index{0};
};

}  // namespace ut

#endif /* ut0object_cache_h */
