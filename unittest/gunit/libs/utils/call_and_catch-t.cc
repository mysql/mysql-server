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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>                          // TEST
#include <concepts>                               // same_as
#include <functional>                             // reference_wrapper
#include <new>                                    // bad_alloc
#include "mysql/debugging/unittest_assertions.h"  // ASSERT_OK
#include "mysql/utils/call_and_catch.h"           // call_and_catch

/// Requirements
/// - call_and_catch should work as described in its doxygen comment.

namespace {

using mysql::utils::call_and_catch;
using mysql::utils::Return_status;

int ten{10};

// ==== Noexcept functions ====

void noexcept_void_func() noexcept {}

auto noexcept_void_lambda = []() noexcept {};

int noexcept_int_func() noexcept { return 1; }

auto noexcept_int_lambda = []() noexcept { return 1; };

int &noexcept_intref_func() noexcept { return ten; }

auto noexcept_intref_lambda = []() noexcept -> int & { return ten; };

int *noexcept_intptr_func() noexcept { return &ten; }

auto noexcept_intptr_lambda = []() noexcept -> int * { return &ten; };

// ==== Throwing functions ====

void void_func(bool oom) {
  if (oom) throw std::bad_alloc();
}

auto void_lambda = [](bool oom) {
  if (oom) throw std::bad_alloc();
};

int int_func(bool oom) {
  void_func(oom);
  return 10;
}

auto int_lambda = [](bool oom) {
  void_func(oom);
  return 10;
};

int &intref_func(bool oom) {
  void_func(oom);
  return ten;
}

auto intref_lambda = [](bool oom) -> int & {
  void_func(oom);
  return ten;
};

int *intptr_func(bool oom) {
  void_func(oom);
  return &ten;
}

auto intptr_lambda = [](bool oom) -> int * {
  void_func(oom);
  return &ten;
};

// ==== Test return types: noexcept functions ====

static_assert(std::same_as<decltype(call_and_catch(noexcept_void_func)), void>);

static_assert(
    std::same_as<decltype(call_and_catch(noexcept_void_lambda)), void>);

static_assert(std::same_as<decltype(call_and_catch(noexcept_int_func)), int>);

static_assert(std::same_as<decltype(call_and_catch(noexcept_int_lambda)), int>);

static_assert(
    std::same_as<decltype(call_and_catch(noexcept_intref_func)), int &>);

static_assert(
    std::same_as<decltype(call_and_catch(noexcept_intref_lambda)), int &>);

static_assert(
    std::same_as<decltype(call_and_catch(noexcept_intptr_func)), int *>);

static_assert(
    std::same_as<decltype(call_and_catch(noexcept_intptr_lambda)), int *>);

// ==== Test return types: throwing functions ====

static_assert(
    std::same_as<decltype(call_and_catch(void_func, true)), Return_status>);

static_assert(
    std::same_as<decltype(call_and_catch(void_lambda, true)), Return_status>);

static_assert(
    std::same_as<decltype(call_and_catch(int_func, true)), std::optional<int>>);

static_assert(std::same_as<decltype(call_and_catch(int_lambda, true)),
                           std::optional<int>>);

static_assert(std::same_as<decltype(call_and_catch(intref_func, true)),
                           std::optional<std::reference_wrapper<int>>>);

static_assert(std::same_as<decltype(call_and_catch(intref_lambda, true)),
                           std::optional<std::reference_wrapper<int>>>);

static_assert(std::same_as<decltype(call_and_catch(intptr_func, true)),
                           std::optional<int *>>);

static_assert(std::same_as<decltype(call_and_catch(intptr_lambda, true)),
                           std::optional<int *>>);

TEST(LibsUtilsBadAllocGuard, Basic) {
  // ==== Noexcept functions ====

  // Function returning void
  call_and_catch(noexcept_void_func);
  call_and_catch(noexcept_void_lambda);

  // Function returning by value
  ASSERT_EQ(call_and_catch(noexcept_int_func), 1);
  ASSERT_EQ(call_and_catch(noexcept_int_lambda), 1);

  // Function returning by reference
  ASSERT_EQ(call_and_catch(noexcept_intref_func), 10);
  ASSERT_EQ(&call_and_catch(noexcept_intref_lambda), &ten);

  // Function returning pointer
  ASSERT_EQ(*call_and_catch(noexcept_intptr_func), 10);
  ASSERT_EQ(call_and_catch(noexcept_intptr_lambda), &ten);

  // ==== Throwing functions ====

  // Function returning void
  ASSERT_OK(call_and_catch(void_func, false));
  ASSERT_ERROR(call_and_catch(void_func, true));
  ASSERT_OK(call_and_catch(void_lambda, false));
  ASSERT_ERROR(call_and_catch(void_lambda, true));

  // clang-tidy wrongly claims that we call std::optional::value without first
  // checking std::optional::has_value.
  // NOLINTBEGIN(bugprone-unchecked-optional-access)

  // Function returning by value
  {
    auto ret = call_and_catch(int_func, false);
    ASSERT_EQ(ret.has_value(), true);
    ASSERT_EQ(ret.value(), 10);
  }
  ASSERT_EQ(call_and_catch(int_func, true).has_value(), false);

  {
    auto ret = call_and_catch(int_lambda, false);
    ASSERT_EQ(ret.has_value(), true);
    ASSERT_EQ(ret.value(), 10);
  }
  ASSERT_EQ(call_and_catch(int_lambda, true).has_value(), false);

  // Function returning by reference
  {
    auto ret = call_and_catch(intref_func, false);
    ASSERT_EQ(ret.has_value(), true);
    ASSERT_EQ(ret.value(), 10);
    ASSERT_EQ(&ret.value().get(), &ten);
  }
  ASSERT_EQ(call_and_catch(intref_func, true).has_value(), false);

  {
    auto ret = call_and_catch(intref_lambda, false);
    ASSERT_EQ(ret.has_value(), true);
    ASSERT_EQ(ret.value(), 10);
    ASSERT_EQ(&ret.value().get(), &ten);
  }
  ASSERT_EQ(call_and_catch(intref_lambda, true).has_value(), false);

  // Function returning pointer
  {
    auto ret = call_and_catch(intptr_func, false);
    ASSERT_EQ(ret.has_value(), true);
    ASSERT_EQ(*ret.value(), 10);
    ASSERT_EQ(ret.value(), &ten);
  }
  ASSERT_EQ(call_and_catch(intptr_func, true).has_value(), false);

  {
    auto ret = call_and_catch(intptr_lambda, false);
    ASSERT_EQ(ret.has_value(), true);
    ASSERT_EQ(*ret.value(), 10);
    ASSERT_EQ(ret.value(), &ten);
  }
  ASSERT_EQ(call_and_catch(intptr_lambda, true).has_value(), false);

  // NOLINTEND(bugprone-unchecked-optional-access)
}

}  // namespace
