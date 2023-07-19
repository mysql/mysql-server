/* Copyright (c) 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/// @addtogroup Replication
/// @{
///
/// @file allocator.h
///
/// @brief Allocator class that uses a polymorphic Memory_resource to
/// allocate memory.

#ifndef MYSQL_RESOURCE_ALLOCATOR_H_
#define MYSQL_RESOURCE_ALLOCATOR_H_

#include <cassert>  // assert
#include <limits>   // std::numeric_limits

#include "memory_resource.h"

namespace mysqlns::resource {

/// @brief Allocator using a Memory_resource to do the allocator.
///
/// A library that allocates memory should allow the user to pass a
/// Memory_resource object which defaults to a default-constructed
/// instance, Memory_resource().  Internally it should create the
/// Allocator<T> classes it needs (possibly several, for different
/// classes T), using the given Memory_resource object.  Users of the
/// library *outside* the server should just use the default
/// Memory_resource.  Users of the library *inside* the server should
/// setup a PSI key and pass the result from @c memory_resource(Key)
/// to the library.
template <class T>
class Allocator {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  using pointer = value_type *;
  using const_pointer = const value_type *;

  using reference = T &;
  using const_reference = const T &;

  /// Construct a new Allocator using the given Memory_resource.
  ///
  /// @param memory_resource The memory resource. By default, this
  /// uses a default-constructed Memory_resource, so it uses
  /// std::malloc and std::free for allocations.
  explicit Allocator(Memory_resource memory_resource = Memory_resource())
      : m_memory_resource(std::move(memory_resource)) {}

  /// Implicit conversion from other instance.
  ///
  /// This is required by Windows implementation of
  /// @c std::vector<Allocator>.
  template <class U>
  explicit Allocator(const Allocator<U> &other)
      : m_memory_resource(other.get_memory_resource()) {}

  /// Use the Memory_resource to allocate the given number of elements
  /// of type T.
  ///
  /// @param n The number of elements.
  ///
  /// @param hint Unused.
  ///
  /// @return The new pointer.
  ///
  /// @throws std::bad_alloc on out of memory conditions.
  pointer allocate(size_type n, const_pointer hint [[maybe_unused]] = nullptr) {
    pointer p = static_cast<pointer>(
        m_memory_resource.allocate(n * sizeof(value_type)));
    if (p == nullptr) throw std::bad_alloc();
    return p;
  }

  /// Use the Memory_resource to deallocate the given pointer.
  ///
  /// @param p The pointer to deallocate.
  ///
  /// @param size Unused.
  void deallocate(pointer p, [[maybe_unused]] size_type size) {
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
  /// @retval Deleter function that takes a `pointer` as argument and
  /// uses the Memory_resource to deallocate it.
  std::function<void(pointer)> get_deleter() {
    auto deallocator = m_memory_resource.get_deallocator();
    // Capture by value so we get a self-contained object that may
    // outlive this Allocator and the Memory_resource if needed.
    return [=](pointer p) {
      p->~T();
      deallocator(p);
    };
  }

  /// In-place construct a new object of the given type.
  ///
  /// @param p Pointer to memory area where the new object will be
  /// constructed.
  ///
  /// @param args Arguments to pass to the constructor.
  ///
  /// @throws std::bad_alloc on out of memory conditions.
  template <class U, class... Args>
  [[deprecated("std::allocator::construct is deprecated in C++17")]] void
  construct(U *p, Args &&... args) {
    assert(p != nullptr);
    ::new ((void *)p) U(std::forward<Args>(args)...);
    // Constructor may throw an exception, which we propagate to the
    // caller.  This may happen if the constructor itself allocates
    // memory (e.g. for members) using @c allocate.
  }

  /// Destroy an object but do not deallocate the memory it occupies.
  ///
  /// @param p Pointer to the object.
  [[deprecated("std::allocator::destroy is deprecated in C++17")]] void destroy(
      pointer p) {
    assert(p != nullptr);
    try {
      p->~T();
    } catch (...) {
      assert(false);  // Destructor should not throw an exception
    }
  }

  /// The maximum number of T objects that fit in memory.
  [[deprecated("std::allocator::max_size is deprecated in C++17")]] size_type
  max_size() const {
    return std::numeric_limits<size_type>::max() / sizeof(T);
  }

  /// Rebind the allocator as requried by the Allocator named requirement.
  template <class U>
  struct rebind {
    typedef Allocator<U> other;
  };

  /// Return the underlying Memory_resource object.
  Memory_resource get_memory_resource() const { return m_memory_resource; }

 private:
  /// The underlying Memory_resource object.
  const Memory_resource m_memory_resource;
};

/// Compare two Allocator objects for equality.
template <class T>
bool operator==([[maybe_unused]] const Allocator<T> &a1,
                [[maybe_unused]] const Allocator<T> &a2) {
  return true;
}

/// Compare two Allocator objects for inequality.
template <class T>
bool operator!=([[maybe_unused]] const Allocator<T> &a1,
                [[maybe_unused]] const Allocator<T> &a2) {
  return true;
}

}  // namespace mysqlns::resource

/// @} (end of group Replication)

#endif  // MYSQL_RESOURCE_ALLOCATOR_H_
