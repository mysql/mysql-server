/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PFS_STD_ALLOCATOR_H
#define PFS_STD_ALLOCATOR_H

#include "my_config.h"

#include <memory>

#include "my_compiler.h"
#include "my_inttypes.h"
#include "storage/perfschema/pfs_global.h"

struct PFS_builtin_memory_class;

template <class T>
struct PFS_std_allocator {
 public:
  typedef T value_type;

  PFS_std_allocator(PFS_builtin_memory_class *klass) : m_klass(klass) {}

  template <class U>
  constexpr PFS_std_allocator(const PFS_std_allocator<U> &u) noexcept
      : m_klass(u.get_class()) {}

  [[nodiscard]] T *allocate(std::size_t n) {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
      throw std::bad_array_new_length();

    const size_t size = n * sizeof(T);
    void *mem = pfs_malloc(m_klass, size, MYF(0));
    if (mem == nullptr) {
      throw std::bad_alloc();
    }
    return static_cast<T *>(mem);
  }

  void deallocate(T *p, std::size_t n) noexcept {
    const size_t size = n * sizeof(T);
    pfs_free(m_klass, size, p);
  }

  PFS_builtin_memory_class *get_class() const { return m_klass; }

 private:
  PFS_builtin_memory_class *m_klass;
};

template <class T, class U>
bool operator==(const PFS_std_allocator<T> &t, const PFS_std_allocator<U> &u) {
  return (t.m_klass == u.m_klass);
}

template <class T, class U>
bool operator!=(const PFS_std_allocator<T> &t, const PFS_std_allocator<U> &u) {
  return (t.m_klass != u.m_klass);
}

#endif
