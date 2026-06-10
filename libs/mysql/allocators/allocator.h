/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

/// @file
///
/// @brief Allocator class that uses a polymorphic Memory_resource to
/// allocate memory.

#ifndef MYSQL_ALLOCATORS_ALLOCATOR_H
#define MYSQL_ALLOCATORS_ALLOCATOR_H

#include <cassert>                             // assert
#include <limits>                              // std::numeric_limits
#include "mysql/allocators/memory_resource.h"  // Memory_resource

/// @addtogroup GroupLibsMysqlAllocators
/// @{

namespace mysql::allocators {

/// @brief Allocator using a Memory_resource to do the allocation.
///
/// A library that allocates memory should allow the user to pass a
/// Memory_resource object which defaults to a default-constructed instance,
/// Memory_resource().  Internally it should create the Allocator<T> classes it
/// needs (possibly several, for different classes T), using the given
/// Memory_resource object.  Users of the library *outside* the server should
/// just use the default Memory_resource.  Users of the library *inside* the
/// server should setup a PSI key and pass the result from
/// @c psi_memory_resource(Key) to the library.
template <class T>
class Allocator {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  /// On move-assignment for containers using this allocator, make the target
  /// container inherit the allocator and reuse the memory from the source
  /// container.
  using propagate_on_container_move_assignment = std::true_type;

  /// On copy-assignment for containers using this allocator, make the target
  /// container preserve its existing allocator and reuse its own memory if
  /// possible.
  using propagate_on_container_copy_assignment = std::false_type;

  /// Construct a new Allocator using the given Memory_resource.
  ///
  /// @param memory_resource The memory resource. By default, this
  /// uses a default-constructed Memory_resource, so it uses
  /// std::malloc and std::free for allocations.
  explicit Allocator(
      Memory_resource memory_resource = Memory_resource()) noexcept
      : m_memory_resource(std::move(memory_resource)) {}

  /// Implicit conversion from other instance.
  ///
  /// This is required to exist and be implicit by the Windows implementation of
  /// @c std::vector.
  template <class U>
  // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
  constexpr Allocator(const Allocator<U> &other) noexcept
      : m_memory_resource(other.get_memory_resource()) {}

  // Declare all copy/move as noexcept, and copy the Memory_resource on moves.
  //
  // Allocators are required to not change when being moved-from
  // (https://timsong-cpp.github.io/cppwp/allocator.requirements.general#67).
  // The default move constructor would violate that by moving from the
  // std::function members of Memory_resource. Therefore, we override and copy
  // Memory_resource instead.
  //
  // It is also required that both move/copy do not throw. The copy operator for
  // `std::function` may throw `bad_alloc`. Therefore we declare the functions
  // as `noexcept`, so that it will instead call `std::terminate` on
  // out-of-memory conditions. This is a reasonable action in such cases (and in
  // practice may never occur, because of the small buffer optimization for
  // std::function).
  constexpr Allocator(const Allocator &other) noexcept = default;
  constexpr Allocator &operator=(const Allocator &other) noexcept = default;
  constexpr Allocator(Allocator &&other) noexcept
      // Copy in move constructor is intentional (see above).
      // NOLINTNEXTLINE(cert-oop11-cpp,performance-move-constructor-init)
      : m_memory_resource(other.m_memory_resource) {}
  constexpr Allocator &operator=(Allocator &&other) noexcept {
    m_memory_resource = other.m_memory_resource;
    return *this;
  }
  ~Allocator() noexcept = default;

  /// Use the Memory_resource to allocate the given number of elements
  /// of type T.
  ///
  /// @param n The number of elements.
  ///
  /// @return The new pointer.
  ///
  /// @throws std::bad_alloc on out of memory conditions.
  [[nodiscard]] constexpr T *allocate(size_type n) {
    T *p = static_cast<T *>(m_memory_resource.allocate(n * sizeof(value_type)));
    if (p == nullptr) throw std::bad_alloc();
    return p;
  }

  /// Use the Memory_resource to deallocate the given pointer.
  ///
  /// @param p The pointer to deallocate.
  ///
  /// @param size Unused.
  constexpr void deallocate(T *p, [[maybe_unused]] size_type size) {
    m_memory_resource.deallocate(p);
  }

  /// Return a Deleter function for objects allocated by this class.
  ///
  /// Such a Deleter must be specified when constructing a smart
  /// pointer to an object created by this Allocator, for example:
  ///
  /// @code
  ///   Allocator<T> allocator(some_memory_resource);
  ///   T *obj = allocator.allocate(1);
  ///   std::shared_ptr<T> ptr(obj, allocator.get_deleter());
  /// @endcode
  ///
  /// @retval Deleter function that takes a `T*` as argument and
  /// uses the Memory_resource to deallocate it.
  [[nodiscard]] std::function<void(T *)> get_deleter() {
    auto deallocator = m_memory_resource.get_deallocator();
    // Capture by value so we get a self-contained object that may
    // outlive this Allocator and the Memory_resource if needed.
    return [=](T *p) {
      p->~T();
      deallocator(p);
    };
  }

  /// Return a reference to the underlying Memory_resource object.
  [[nodiscard]] const Memory_resource &get_memory_resource() const {
    return m_memory_resource;
  }

 private:
  /// The underlying Memory_resource object.
  Memory_resource m_memory_resource;
};  // class Allocator

/// Compare two Allocator objects for equality.
template <class T>
[[nodiscard]] bool operator==([[maybe_unused]] const Allocator<T> &a1,
                              [[maybe_unused]] const Allocator<T> &a2) {
  return true;
}

/// Compare two Allocator objects for inequality.
template <class T>
[[nodiscard]] bool operator!=([[maybe_unused]] const Allocator<T> &a1,
                              [[maybe_unused]] const Allocator<T> &a2) {
  return true;
}

}  // namespace mysql::allocators

/// @}

#endif  // MYSQL_ALLOCATORS_ALLOCATOR_H
