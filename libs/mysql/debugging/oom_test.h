// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_DEBUGGING_OOM_TEST_H
#define MYSQL_DEBUGGING_OOM_TEST_H

/// @file
/// Experimental API header

#include <concepts>                            // invocable
#include "mysql/allocators/memory_resource.h"  // Memory_resource
#include "mysql/utils/call_and_catch.h"        // call_and_catch
#include "mysql/utils/return_status.h"         // Return_status

/// @addtogroup GroupLibsMysqlDebugging
/// @{

namespace mysql::debugging {

/// Repeatedly call function(). In the Nth iteration, simulate out-of-memory
/// error on the Nth allocation. Return as soon as it succeeds.
///
/// @tparam Initializer_t Type of initialization function.
///
/// @tparam Function_t Type of function to call.
///
/// @param initialize Function called in each iteration, taking the
/// Memory_resource object which will later simulate allocation failures as
/// argument. This may, for instance, create and populate an object that uses
/// the memory_resource. For the duration of the call to initializer, the
/// Memory_resource will behave exactly like std::malloc.
///
/// @param function Function to test, which should take no parameters. This
/// function may use the memory_resource that was previously passed to
/// `initialize`. The Nth time it calls the `allocate` member, `allocate` will
/// fail. The function is allowed two ways to report error: (1) If it returns
/// mysql::utils::Return_status, Return_status::ok should indicate success and
/// Return_status::error indicate out-of-memory. Otherwise, function return
/// should indicate success and bad_alloc exception should indicate error.
///
/// @return The number of iterations until function() succeeded. If function is
/// deterministic, this equals the number of allocations performed by the call
/// to function().
template <std::invocable<mysql::allocators::Memory_resource> Initializer_t,
          std::invocable Function_t>
int oom_test(const Initializer_t &initialize, const Function_t &function) {
  int calls_until_oom{-1};
  mysql::allocators::Memory_resource memory_resource(
      [&](std::size_t size) -> void * {
        if (calls_until_oom == 0) return nullptr;
        if (calls_until_oom > 0) --calls_until_oom;
        return std::malloc(size);
      },
      std::free);
  for (int i = 0; true; ++i) {
    calls_until_oom = -1;
    initialize(memory_resource);
    calls_until_oom = i;
    if (mysql::utils::call_and_catch(function) ==
        mysql::utils::Return_status::ok)
      return i;
  }
}

/// Repeatedly construct copies of object and call function(copy). In the Nth
/// iteration, simulate out-of-memory error on the Nth allocation. Return as
/// soon as it succeeds.
///
/// @tparam Object_t Type of the object. This must have a constructor taking
/// `(const Object_t& , Memory_resource)` parameters.
///
/// @param object Object to test.
///
/// @param function Function to test, which should take an Object& as its only
/// parameter. This function may use the memory_resource that was previously
/// passed to `initialize`. The Nth time it calls the `allocate` member,
/// `allocate` will fail. The function is allowed two ways to report error: (1)
/// If it returns mysql::utils::Return_status, Return_status::ok should indicate
/// success and Return_status::error indicate out-of-memory. Otherwise, function
/// return should indicate success and bad_alloc exception should indicate
/// error.
///
/// @return The number of iterations until function(copy) succeeded. If function
/// is deterministic, this equals the number of allocations performed by the
/// call to function(copy).
template <class Object_t>
int oom_test_copyable_object(const Object_t &object,
                             std::invocable<Object_t &> auto function) {
  Object_t copy{};
  return oom_test(
      [&](const mysql::allocators::Memory_resource &memory_resource) {
        copy = std::move(Object_t(object, memory_resource));
      },
      [&] { return function(copy); });
}

/// Repeatedly construct copies of `object` and call `function(copy)`. In the
/// Nth iteration, simulate out-of-memory error on the Nth allocation. Return as
/// soon as it succeeds.
///
/// This is intended for use with non-throwing containers which do not have a
/// copy constructor.
///
/// @tparam Object_t Type of the object. This must have a constructor taking a
/// `Memory_resource` as parameter, and a member function `assign` taking
/// another `Object_t` as argument. The `assign` member must not replace the
/// memory resource used by the object to allocate.
///
/// @param object Object to test.
///
/// @param function Function to test, which should take an Object& as its only
/// parameter. This function may use the memory_resource that was previously
/// passed to `initialize`. The Nth time it calls the `allocate` member,
/// `allocate` will fail. The function is allowed two ways to report error: (1)
/// If it returns mysql::utils::Return_status, Return_status::ok should indicate
/// success and Return_status::error indicate out-of-memory. Otherwise, function
/// return should indicate success and bad_alloc exception should indicate
/// error.
///
/// @return The number of iterations until function(copy) succeeded. If function
/// is deterministic, this equals the number of allocations performed by the
/// call to function(copy).
template <class Object_t>
int oom_test_assignable_object(const Object_t &object,
                               std::invocable<Object_t &> auto function) {
  Object_t copy{};
  return oom_test(
      [&](const mysql::allocators::Memory_resource &memory_resource) {
        // Move-assign empty object with the given Memory_resource
        auto o = Object_t(memory_resource);
        copy.assign(std::move(o));
        // Copy-assign the contents.
        [[maybe_unused]] auto ret = copy.assign(object);
        // As this is for testing only, fail with assertion if out-of-memory
        // occurred in this initialization function.
        assert(ret == mysql::utils::Return_status::ok);
      },
      [&] { return function(copy); });
}

}  // namespace mysql::debugging

// addtogroup GroupLibsMysqlDebugging
/// @}

#endif  // ifndef MYSQL_DEBUGGING_OOM_TEST_H
