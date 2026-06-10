/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_HARNESS_INCLUDE_SECURE_ALLOCATOR_H_
#define ROUTER_SRC_HARNESS_INCLUDE_SECURE_ALLOCATOR_H_

#include "secure_memory_pool.h"  // NOLINT(build/include_subdir)

namespace mysql_harness {

/**
 * Adapter for SecureMemoryPool which can be used by the STL allocators.
 */
template <typename T>
struct SecureAllocator {
  using value_type = T;

  template <class U>
  struct rebind {
    using other = SecureAllocator<U>;
  };

  constexpr SecureAllocator() noexcept = default;

  constexpr SecureAllocator(const SecureAllocator &) noexcept = default;
  constexpr SecureAllocator(SecureAllocator &&) noexcept = default;

  constexpr SecureAllocator &operator=(const SecureAllocator &) noexcept =
      default;
  constexpr SecureAllocator &operator=(SecureAllocator &&) noexcept = default;

  template <typename U>
  constexpr SecureAllocator(const SecureAllocator<U> &) noexcept {}

  T *allocate(std::size_t n) {
    return static_cast<T *>(SecureMemoryPool::get().allocate(n * sizeof(T)));
  }

  void deallocate(T *p, std::size_t n) noexcept {
    SecureMemoryPool::get().deallocate(p, n * sizeof(T));
  }
};

}  // namespace mysql_harness

#endif  // ROUTER_SRC_HARNESS_INCLUDE_SECURE_ALLOCATOR_H_
