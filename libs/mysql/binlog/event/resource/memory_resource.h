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

/// @file memory_resource.h
///
/// @brief Class that wraps resources in a polymorphic manner.

#ifndef MYSQL_BINLOG_EVENT_RESOURCE_MEMORY_RESOURCE_H
#define MYSQL_BINLOG_EVENT_RESOURCE_MEMORY_RESOURCE_H

#include <cstdlib>
#include <functional>

/// @addtogroup GroupLibsMysqlBinlogEvent
/// @{

namespace mysql::binlog::event::resource {

/// Polymorphism-free memory resource class with custom allocator and
/// deallocator functions.
///
/// This is used as the "back-end" for Allocator objects.  The
/// allocator and deallocator functions are member objects of type
/// std::function, specified as constructor arguments.
///
/// Example: a class that needs to allocate memory, and should have a
/// way to use a specified PSI key, can be implemented as follows:
///
/// - For each type the class needs to allocate, it has one member
///   object of type Allocator.  The constructor takes a
///   Memory_resource parameter, which defaults to a
///   default-constructed Memory_resource.  The constructor passes the
///   Memory_resource to the Allocator constructors.  The class calls
///   the Allocator member functions for its memory management.
///
/// - API clients that don't need a PSI key don't need to pass a
///   Memory_resource to the constructor; they use the default value
///   for the parameter.  API clients that need a PSI key, pass a
///   Memory_resource object where the allocate/deallocate functions
///   are PSI-enabled.  Such a Memory_resource object can be obtained
///   from the factory function @c psi_memory_resource in
///   sql/psi_memory_resource.h.
///
/// Notes on design choices:
///
/// We avoid using polymorphism to configure the behavior of the
/// allocate/deallocate member functions.  Classes that use
/// polymorphic memory resources would have to take a reference or
/// pointer to the memory resource as argument, so either the class
/// can't own the memory resource or it has to be dynamically
/// allocated.  If the class can't own the memory resource, it
/// complicates the usage since the caller has to ensure the
/// Memory_resource outlives the class that uses it.  If the
/// Memory_resource has to be allocated dynamically, that allocation
/// cannot use a custom Memory_resource, which may defeat the purpose
/// of using custom Memory_resources to track all allocations.
///
/// We also avoid static polymorphism, since every user of a
/// statically polymorphic Memory_resource would have to be a
/// template, which has two problems: first, two classes with the same
/// behavior but different allocation strategies would be different
/// types.  Second, any class that allocates memory and has a
/// requirement that the API client can configure the Memory_resource,
/// has to be templated and therefore be defined in a header.
///
/// With the current solution, the caller just creates a
/// Memory_resource object and passes it to the allocator.
class Memory_resource {
 public:
  using Size_t = std::size_t;
  using Ptr_t = void *;
  using Allocator_t = std::function<Ptr_t(Size_t)>;
  using Deallocator_t = std::function<void(Ptr_t)>;

  /// Construct a new Memory_resource that uses the given allocator
  /// and deallocator.
  ///
  /// @param allocator The allocator, e.g. std::malloc or my_malloc.
  ///
  /// @param deallocator The deallocator, e.g. std::free or my_free.
  Memory_resource(const Allocator_t &allocator,
                  const Deallocator_t &deallocator)
      : m_allocator(allocator), m_deallocator(deallocator) {}

  /// Construct a new Memory_resource that uses std::malloc and std::free.
  Memory_resource() : Memory_resource(std::malloc, std::free) {}

  /// Allocate memory using the provided allocator.
  ///
  /// @param n The size.
  ///
  /// @return The new memory.
  void *allocate(Size_t n) const { return m_allocator(n); }

  /// Deallocate memory using the provided deallocator.
  ///
  /// @param p The pointer.
  void deallocate(Ptr_t p) const { m_deallocator(p); }

  /// Return the deallocator.
  Deallocator_t get_deallocator() const { return m_deallocator; }

 private:
  /// The allocator object.
  const Allocator_t m_allocator;

  /// The deallocator object.
  const Deallocator_t m_deallocator;
};

}  // namespace mysql::binlog::event::resource

/// @}

#endif  // MYSQL_BINLOG_EVENT_RESOURCE_MEMORY_RESOURCE_H
