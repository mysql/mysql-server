/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/stdx/expected.h"

#include <initializer_list>
#include <optional>
#include <string_view>
#include <system_error>
#include <type_traits>  // is_move_constructible

#include <gmock/gmock.h>

#include "mysql/harness/stdx/expected_ostream.h"

static_assert(!std::is_default_constructible_v<stdx::unexpected<int>>);

// destructors should be trivially destructible if the underlying types are
// trivially destructible too.

static_assert(std::is_trivially_destructible_v<stdx::unexpected<int>>);
static_assert(!std::is_trivially_destructible_v<stdx::unexpected<std::string>>);

// check 'noexcept' works on stdx::unexpected
static_assert(noexcept(stdx::unexpected<int>('a')));
static_assert(!noexcept(stdx::unexpected<std::string>("abc")));

// check construction and .error() are constexpr
static_assert(stdx::unexpected<int>(1).error() == 1);

// default constructor
static_assert(std::is_default_constructible_v<stdx::expected<void, int>>);
static_assert(std::is_default_constructible_v<stdx::expected<int, int>>);
static_assert(
    std::is_default_constructible_v<stdx::expected<std::string, int>>);

class not_default_constructible {
 public:
  not_default_constructible() = delete;
};

static_assert(!std::is_default_constructible_v<
              stdx::expected<not_default_constructible, int>>);

// check 'noexcept' of default-constructor
static_assert(noexcept(stdx::expected<void, int>()));
static_assert(noexcept(stdx::expected<int, int>()));

TEST(Expected, destructible) {
  constexpr bool cxx_has_conditional_destructor =
      std::is_trivially_destructible_v<stdx::expected<void, int>>;

  static_assert(cxx_has_conditional_destructor ==
                std::is_trivially_destructible_v<stdx::expected<int, int>>);
  static_assert(cxx_has_conditional_destructor ==
                std::is_trivially_destructible_v<stdx::expected<void, int>>);
  static_assert(
      cxx_has_conditional_destructor ==
      std::is_trivially_copy_constructible_v<stdx::expected<int, int>>);
  static_assert(
      cxx_has_conditional_destructor ==
      std::is_trivially_copy_constructible_v<stdx::expected<void, int>>);
  static_assert(
      cxx_has_conditional_destructor ==
      std::is_trivially_move_constructible_v<stdx::expected<int, int>>);
  static_assert(
      cxx_has_conditional_destructor ==
      std::is_trivially_move_constructible_v<stdx::expected<void, int>>);
}

static_assert(
    !std::is_trivially_destructible_v<stdx::expected<std::string, int>>);

TEST(Unexpected, value_constructible) {
  stdx::unexpected<int> v1(1);

  EXPECT_EQ(v1.error(), 1);
}

TEST(Unexpected, convertible) {
  stdx::unexpected<std::optional<int>> v1(1);

  EXPECT_EQ(v1.error(), 1);
}

TEST(Unexpected, copy_constructible) {
  stdx::unexpected<int> v1(1);
  stdx::unexpected<int> v2(v1);

  EXPECT_EQ(v1.error(), 1);
  EXPECT_EQ(v2.error(), 1);
}

TEST(Unexpected, move_constructible) {
  stdx::unexpected<std::unique_ptr<int>> v1(nullptr);
  EXPECT_EQ(v1.error().get(), nullptr);

  stdx::unexpected<std::unique_ptr<int>> v2(std::move(v1));
  EXPECT_EQ(v2.error().get(), nullptr);
}

TEST(Unexpected, in_place_construct) {
  stdx::unexpected<int> v1(std::in_place, 1);
  EXPECT_EQ(v1.error(), 1);
}

TEST(Unexpected, in_place_list_construct) {
  stdx::unexpected<std::vector<int>> v1(std::in_place, {2});
  ASSERT_EQ(v1.error().size(), 1);
  EXPECT_EQ(v1.error()[0], 2);
}

TEST(Unexpected, eq_same_types) {
  stdx::unexpected<int> a(1);
  stdx::unexpected<int> b(1);

  EXPECT_EQ(a, b);
}

TEST(Unexpected, eq_different_types) {
  stdx::unexpected<int> a(1);
  stdx::unexpected<short> b(1);

  EXPECT_EQ(a, b);
}

TEST(Expected, default_construct_is_value) {
  stdx::expected<int, std::error_code> exp(0);

  EXPECT_TRUE(exp.has_value());
  EXPECT_TRUE(exp);
  EXPECT_EQ(exp.value(), 0);
  EXPECT_EQ(*exp, 0);
}

TEST(ExpectedVoid, default_construct_is_value) {
  // with T=void, there is no value to fetch
  stdx::expected<void, std::error_code> exp;

  EXPECT_TRUE(exp.has_value());
  // there is no value() method
  EXPECT_TRUE(exp);
}

TEST(Expected, construct_from_error) {
  stdx::expected<int, std::error_code> exp(
      stdx::unexpected(make_error_code(std::errc::bad_address)));

  EXPECT_FALSE(exp.has_value());
  EXPECT_FALSE(exp);
  EXPECT_EQ(exp.error(), std::errc::bad_address);
}

TEST(ExpectedVoid, construct_from_error) {
  stdx::expected<void, std::error_code> exp(
      stdx::unexpected(make_error_code(std::errc::bad_address)));

  EXPECT_FALSE(exp.has_value());
  EXPECT_FALSE(exp);
  EXPECT_EQ(exp.error(), std::errc::bad_address);

  // don't deref exp
}

TEST(Expected, operator_eq_error) {
  stdx::expected<int, std::error_code> exp(
      stdx::unexpected(make_error_code(std::errc::bad_address)));
  stdx::expected<int, std::error_code> exp2(
      stdx::unexpected(make_error_code(std::errc::bad_address)));

  EXPECT_TRUE(exp2 == exp);
  EXPECT_TRUE(exp == exp2);
  EXPECT_FALSE(exp2 != exp);
  EXPECT_FALSE(exp != exp2);
}

TEST(ExpectedVoid, operator_eq_error) {
  stdx::expected<void, std::error_code> exp(
      stdx::unexpected(make_error_code(std::errc::bad_address)));
  stdx::expected<void, std::error_code> exp2(
      stdx::unexpected(make_error_code(std::errc::bad_address)));

  EXPECT_TRUE(exp2 == exp);
  EXPECT_TRUE(exp == exp2);
  EXPECT_FALSE(exp2 != exp);
  EXPECT_FALSE(exp != exp2);
}

TEST(Expected, operator_eq_error_unexpected) {
  stdx::expected<int, std::error_code> exp(
      stdx::unexpected(make_error_code(std::errc::bad_address)));
  auto unexp = stdx::unexpected(make_error_code(std::errc::bad_address));

  EXPECT_TRUE(exp == unexp);
  EXPECT_TRUE(unexp == exp);
  EXPECT_FALSE(exp != unexp);
  EXPECT_FALSE(unexp != exp);
}

TEST(Expected, operator_ne_error) {
  stdx::expected<int, std::error_code> exp(
      stdx::unexpected(make_error_code(std::errc::bad_address)));
  stdx::expected<int, std::error_code> exp2(
      stdx::unexpected(make_error_code(std::errc::not_supported)));

  EXPECT_NE(exp2, exp);
  EXPECT_NE(exp, exp2);
}

TEST(ExpectedVoid, operator_ne_error) {
  stdx::expected<void, std::error_code> exp(
      stdx::unexpected(make_error_code(std::errc::bad_address)));
  stdx::expected<void, std::error_code> exp2(
      stdx::unexpected(make_error_code(std::errc::not_supported)));

  EXPECT_NE(exp2, exp);
  EXPECT_NE(exp, exp2);
}

TEST(Expected, operator_eq) {
  stdx::expected<int, std::error_code> exp(0);
  stdx::expected<int, std::error_code> exp2(0);

  EXPECT_EQ(exp2, exp);
  EXPECT_EQ(exp, exp2);
}

TEST(Expected, operator_eq_value) {
  stdx::expected<int, std::error_code> exp1(1);

  EXPECT_EQ(exp1, 1);
  EXPECT_NE(exp1, 0);
}

TEST(ExpectedVoid, operator_eq) {
  stdx::expected<void, std::error_code> exp;
  stdx::expected<void, std::error_code> exp2;

  EXPECT_EQ(exp2, exp);
  EXPECT_EQ(exp, exp2);
}

TEST(Expected, operator_ne) {
  stdx::expected<int, std::error_code> exp(0);
  stdx::expected<int, std::error_code> exp2(1);

  EXPECT_NE(exp2, exp);
}

TEST(Expected, operator_ne_mixed_error_value) {
  stdx::expected<int, std::error_code> exp(0);
  stdx::expected<int, std::error_code> exp2(
      stdx::unexpected(make_error_code(std::errc::not_supported)));

  EXPECT_NE(exp2, exp);
  EXPECT_NE(exp, exp2);
}

TEST(ExpectedVoid, operator_ne_mixed_error_value) {
  stdx::expected<void, std::error_code> exp;
  stdx::expected<void, std::error_code> exp2(
      stdx::unexpected(make_error_code(std::errc::not_supported)));

  EXPECT_NE(exp2, exp);
  EXPECT_NE(exp, exp2);
}

TEST(Expected, copy_construct_from_expected) {
  stdx::expected<int, std::error_code> exp(1);

  EXPECT_TRUE(exp.has_value());
  EXPECT_TRUE(exp);
  EXPECT_EQ(exp.value(), 1);
  EXPECT_EQ(*exp, 1);

  stdx::expected<int, std::error_code> exp2(exp);

  EXPECT_EQ(exp, exp2);
}

TEST(ExpectedVoid, copy_construct_from_expected) {
  stdx::expected<void, std::error_code> exp;

  EXPECT_TRUE(exp.has_value());
  EXPECT_TRUE(exp);

  stdx::expected<void, std::error_code> exp2(exp);

  EXPECT_EQ(exp, exp2);
}

TEST(Expected, move_construct_from_expected) {
  stdx::expected<int, std::error_code> exp(1);

  EXPECT_TRUE(exp.has_value());
  EXPECT_TRUE(exp);
  EXPECT_EQ(exp.value(), 1);
  EXPECT_EQ(*exp, 1);

  stdx::expected<int, std::error_code> exp2(std::move(exp));

  EXPECT_TRUE(exp2.has_value());
  EXPECT_TRUE(exp2);
  EXPECT_EQ(exp2.value(), 1);
  EXPECT_EQ(*exp2, 1);
}

TEST(ExpectedVoid, move_construct_from_expected) {
  stdx::expected<void, std::error_code> exp;

  EXPECT_TRUE(exp.has_value());
  EXPECT_TRUE(exp);
  exp.value();  // exists, but returns nothing.

  stdx::expected<void, std::error_code> exp2(std::move(exp));

  EXPECT_TRUE(exp2.has_value());
  EXPECT_TRUE(exp2);
}

TEST(Expected, T_trivial_value_or_error) {
  stdx::expected<int, std::error_code> exp(
      stdx::unexpected(make_error_code(std::errc::bad_address)));

  EXPECT_FALSE(exp.has_value());

  EXPECT_EQ(exp.value_or(2), 2);
}

TEST(Expected, T_trivial_value_or_value) {
  stdx::expected<int, std::error_code> exp(0);

  EXPECT_TRUE(exp.has_value());

  EXPECT_EQ(exp.value_or(2), 0);
}

TEST(ExpectedVoid, assign_from_unexpected) {
  stdx::expected<void, bool> exp;

  EXPECT_TRUE(exp.has_value());

  exp = stdx::unexpected(true);

  EXPECT_FALSE(exp.has_value());
}

TEST(Expected, assign_from_unexpected_moveable) {
  stdx::expected<int, std::unique_ptr<int>> exp;

  EXPECT_TRUE(exp.has_value());

  std::unique_ptr<int> err(nullptr);
  auto unex = stdx::unexpected(std::move(err));

  exp = std::move(unex);

  EXPECT_FALSE(exp.has_value());
}

TEST(ExpectedVoid, assign_from_unexpected_moveable) {
  stdx::expected<void, std::unique_ptr<int>> exp;

  EXPECT_TRUE(exp.has_value());

  std::unique_ptr<int> err(nullptr);
  auto unex = stdx::unexpected(std::move(err));

  exp = std::move(unex);

  EXPECT_FALSE(exp.has_value());
}

TEST(Expected, in_place) {
  stdx::expected<int, int> exp(std::in_place);

  ASSERT_TRUE(exp.has_value());
  EXPECT_EQ(*exp, 0);
}

TEST(ExpectedVoid, in_place) {
  stdx::expected<void, int> exp(std::in_place);

  EXPECT_TRUE(exp.has_value());
}

TEST(Expected, in_place_2) {
  // in-place does direct-initializion.
  stdx::expected<std::vector<int>, int> exp(std::in_place, 2);

  ASSERT_TRUE(exp.has_value());
  EXPECT_EQ(exp->size(), 2);
  EXPECT_EQ(exp->operator[](0), 0);
  EXPECT_EQ(exp->operator[](1), 0);
}

TEST(Expected, in_place_initializer_list) {
  // in-place does direct-initializion.
  stdx::expected<std::vector<int>, int> exp(std::in_place, {1, 2});

  ASSERT_TRUE(exp.has_value());
  EXPECT_EQ(exp->size(), 2);
  EXPECT_EQ(exp->operator[](0), 1);
  EXPECT_EQ(exp->operator[](1), 2);
}

TEST(Expected, unexpect_2) {
  // unexpect does direct-initializion.
  stdx::expected<int, std::vector<int>> exp(stdx::unexpect, 2);

  ASSERT_TRUE(!exp.has_value());
  EXPECT_EQ(exp.error().size(), 2);
  EXPECT_EQ(exp.error()[0], 0);
  EXPECT_EQ(exp.error()[1], 0);
}

TEST(Expected, unexpect_initializer_list) {
  // unexpect does direct-initializion.
  stdx::expected<int, std::vector<int>> exp(stdx::unexpect, {1, 2});

  ASSERT_TRUE(!exp.has_value());
  EXPECT_EQ(exp.error().size(), 2);
  EXPECT_EQ(exp.error()[0], 1);
  EXPECT_EQ(exp.error()[1], 2);
}

TEST(ExpectedVoid, unexpect_2) {
  // unexpect does direct-initializion.
  stdx::expected<void, std::vector<int>> exp(stdx::unexpect, 2);

  ASSERT_TRUE(!exp.has_value());
  EXPECT_EQ(exp.error().size(), 2);
  EXPECT_EQ(exp.error()[0], 0);
  EXPECT_EQ(exp.error()[1], 0);
}

TEST(ExpectedVoid, unexpect_initializer_list) {
  // unexpect does direct-initializion.
  stdx::expected<void, std::vector<int>> exp(stdx::unexpect, {1, 2});

  ASSERT_TRUE(!exp.has_value());
  EXPECT_EQ(exp.error().size(), 2);
  EXPECT_EQ(exp.error()[0], 1);
  EXPECT_EQ(exp.error()[1], 2);
}

TEST(Expected, bad_expected_access) {
  stdx::expected<int, std::error_code> exp =
      stdx::unexpected(std::error_code{});

  EXPECT_THROW(exp.value(), stdx::bad_expected_access<std::error_code>);
}

TEST(ExpectedVoid, bad_expected_access) {
  stdx::expected<void, std::error_code> exp =
      stdx::unexpected(std::error_code{});

  EXPECT_THROW(exp.value(), stdx::bad_expected_access<std::error_code>);
}

TEST(Expected, emplace_from_unex) {
  stdx::expected<int, std::error_code> exp =
      stdx::unexpected(std::error_code{});

  EXPECT_FALSE(exp.has_value());
  exp.emplace(1);

  EXPECT_TRUE(exp.has_value());
}

TEST(Expected, emplace_from_val) {
  stdx::expected<int, std::error_code> exp = 1;

  ASSERT_TRUE(exp.has_value());
  exp.emplace(2);

  ASSERT_TRUE(exp.has_value());
  EXPECT_EQ(exp.value(), 2);
}

// helper to test .emplace
//
// emplace requires a type that's nothrow-constructible from initializer_list
class InitList {
 public:
  constexpr InitList(std::initializer_list<int> vals) noexcept
      : sz_(vals.size()) {}

  constexpr size_t size() const { return sz_; }

 private:
  size_t sz_;
};

TEST(Expected, emplace_initlist_val) {
  stdx::expected<InitList, std::error_code> exp(std::in_place, {1});

  ASSERT_TRUE(exp.has_value());
  EXPECT_EQ(exp->size(), 1);

  exp.emplace({1});

  EXPECT_TRUE(exp.has_value());
}

TEST(ExpectedVoid, emplace_from_val) {
  stdx::expected<void, std::error_code> exp;

  EXPECT_TRUE(exp.has_value());
  exp.emplace();

  EXPECT_TRUE(exp.has_value());
}

TEST(ExpectedVoid, emplace_from_unex) {
  stdx::expected<void, std::error_code> exp =
      stdx::unexpected(std::error_code{});

  EXPECT_FALSE(exp.has_value());
  exp.emplace();

  EXPECT_TRUE(exp.has_value());
}

class copyable {};

class non_copyable {
 public:
  non_copyable() = default;

  non_copyable(const non_copyable &) = delete;
  non_copyable(non_copyable &&) = default;

  non_copyable &operator=(const non_copyable &) = delete;
  non_copyable &operator=(non_copyable &&) = default;
};

class non_copyable_no_default {
 public:
  non_copyable_no_default(int) {}

  non_copyable_no_default(const non_copyable_no_default &) = delete;
  non_copyable_no_default(non_copyable_no_default &&) = default;

  non_copyable_no_default &operator=(const non_copyable_no_default &) = delete;
  non_copyable_no_default &operator=(non_copyable_no_default &&) = default;
};

static_assert(std::is_copy_constructible_v<copyable>);
static_assert(std::is_move_constructible_v<copyable>);
static_assert(!std::is_copy_constructible_v<non_copyable>);
static_assert(std::is_move_constructible_v<non_copyable>);
static_assert(!std::is_copy_assignable_v<non_copyable>);
static_assert(std::is_move_assignable_v<non_copyable>);

static_assert(!std::is_copy_constructible_v<void>);
static_assert(!std::is_copy_assignable_v<void>);
static_assert(std::is_copy_constructible_v<int>);
static_assert(std::is_copy_assignable_v<int>);
static_assert(std::is_copy_constructible_v<std::error_code>);
static_assert(std::is_copy_assignable_v<std::error_code>);
static_assert(
    std::is_copy_constructible_v<stdx::expected<void, std::error_code>>);
static_assert(std::is_copy_assignable_v<stdx::expected<void, std::error_code>>);
static_assert(
    std::is_copy_constructible_v<stdx::expected<int, std::error_code>>);
static_assert(std::is_copy_assignable_v<int>);
static_assert(std::is_copy_constructible_v<int>);
static_assert(std::is_copy_assignable_v<std::error_code>);
static_assert(std::is_copy_constructible_v<std::error_code>);
static_assert(std::is_copy_assignable_v<stdx::expected<int, std::error_code>>);
static_assert(!std::is_copy_constructible_v<
              stdx::expected<non_copyable, std::error_code>>);
static_assert(
    !std::is_copy_assignable_v<stdx::expected<non_copyable, std::error_code>>);

TEST(Expected, T_unique_ptr) {
  auto test_func = [](bool success)
      -> stdx::expected<std::unique_ptr<int>, std::error_code> {
    if (!success) {
      return stdx::unexpected(
          make_error_code(std::errc::operation_not_supported));
    }

    return {};
  };

  auto res_true = test_func(true);
  ASSERT_TRUE(res_true);

  auto res_false = test_func(false);
  ASSERT_FALSE(res_false);

  auto res = std::move(res_true);
  ASSERT_EQ(res.has_value(), test_func(true).has_value());

  res = std::move(res_false);
  ASSERT_EQ(res.has_value(), test_func(false).has_value());
}

TEST(Expected, T_noncopyable_nodefconst) {
  auto test_func = [](bool success)
      -> stdx::expected<non_copyable_no_default, std::error_code> {
    if (!success) {
      return stdx::unexpected(
          make_error_code(std::errc::operation_not_supported));
    }

    return {1};
  };

  auto res_true = test_func(true);
  ASSERT_TRUE(res_true);

  auto res_false = test_func(false);
  ASSERT_FALSE(res_false);
  EXPECT_EQ(
      res_false,
      stdx::unexpected(make_error_code(std::errc::operation_not_supported)));

  auto res = std::move(res_true);
  ASSERT_EQ(res.has_value(), test_func(true).has_value());

  // move
  res = std::move(res_false);
  ASSERT_EQ(res.has_value(), test_func(false).has_value());
  EXPECT_EQ(res, stdx::unexpected(
                     make_error_code(std::errc::operation_not_supported)));
}

TEST(Expected, T_noncopyable) {
  auto test_func =
      [](bool success) -> stdx::expected<non_copyable, std::error_code> {
    if (!success) {
      return stdx::unexpected(
          make_error_code(std::errc::operation_not_supported));
    }

    return {};
  };

  auto res_true = test_func(true);
  ASSERT_TRUE(res_true);

  auto res_false = test_func(false);
  ASSERT_FALSE(res_false);
  EXPECT_EQ(
      res_false,
      stdx::unexpected(make_error_code(std::errc::operation_not_supported)));

  auto res = std::move(res_true);
  ASSERT_EQ(res.has_value(), test_func(true).has_value());

  res = std::move(res_false);
  ASSERT_EQ(res.has_value(), test_func(false).has_value());
  EXPECT_EQ(res, stdx::unexpected(
                     make_error_code(std::errc::operation_not_supported)));
}

TEST(Expected, manytests) {
  auto test_func = [](bool success) -> stdx::expected<int, std::error_code> {
    if (!success) {
      return stdx::unexpected(
          make_error_code(std::errc::operation_not_supported));
    }

    return {};
  };

  auto res_true = test_func(true);
  ASSERT_TRUE(res_true);

  auto res_false = test_func(false);
  ASSERT_FALSE(res_false);

  auto res = res_true;
  ASSERT_EQ(res, res_true);

  res = res_false;
  ASSERT_EQ(res, res_false);
}

TEST(Expected, convertible) {
  auto test_func = [](bool success) -> stdx::expected<char, std::error_code> {
    if (!success) {
      return stdx::unexpected(
          make_error_code(std::errc::operation_not_supported));
    }

    return {1};
  };

  auto res_true = test_func(true);
  ASSERT_TRUE(res_true);

  auto res_false = test_func(false);
  ASSERT_FALSE(res_false);

  auto res = res_true;
  ASSERT_EQ(res, res_true);

  res = res_false;
  ASSERT_EQ(res, res_false);
}

TEST(ExpectedVoid, manytests) {
  auto test_func = [](bool success) -> stdx::expected<void, std::error_code> {
    if (!success) {
      return stdx::unexpected(
          make_error_code(std::errc::operation_not_supported));
    }

    return {};
  };

  // instantiation
  auto res = test_func(true);
  ASSERT_TRUE(res);

  // move assignment
  res = test_func(false);
  ASSERT_FALSE(res);
  EXPECT_EQ(res, stdx::unexpected(
                     make_error_code(std::errc::operation_not_supported)));

  // move assignment
  res = test_func(true);
  ASSERT_TRUE(res);

  // copy assignment
  auto res2 = res;
  ASSERT_EQ(res2, res);

  // move assignment
  auto res3 = std::move(res);
  ASSERT_TRUE(res3);
}

TEST(Expected, conversion) {
  {
    stdx::expected<std::string, int> exp = std::string("");
    ASSERT_TRUE(exp);

    EXPECT_EQ(exp.value(), "");
  }

  {
    stdx::expected<std::string, int> exp = "def";

    ASSERT_TRUE(exp);
    EXPECT_EQ(exp.value(), "def");

    exp = "abc";

    ASSERT_TRUE(exp);
    EXPECT_EQ(exp.value(), "abc");

    exp = stdx::unexpected(1);

    ASSERT_FALSE(exp);
  }
}

TEST(Unexpected, copy_construct_from_unexpected) {
  constexpr stdx::unexpected<int> err{1};

  stdx::expected<int, int> exp = err;

  EXPECT_FALSE(exp);
}

TEST(Unexpected, move_construct_from_unexpected) {
  stdx::expected<int, int> exp = stdx::unexpected(1);

  EXPECT_FALSE(exp);
}

TEST(Unexpected, in_place_construct_from_unexpect) {
  stdx::expected<std::string, std::optional<int>> exp(stdx::unexpect, 1);

  EXPECT_FALSE(exp);
}

TEST(Unexpected, in_place_construct_from_unexpect_curly) {
  stdx::expected<std::string, std::optional<int>> exp{stdx::unexpect, 1};

  EXPECT_FALSE(exp);
}

TEST(Unexpected, construct_eq_from_unexpected) {
  stdx::expected<std::string, std::optional<int>> exp = stdx::unexpected(1);

  EXPECT_FALSE(exp);
}

TEST(Unexpected, construct_curly_from_unexpected) {
  stdx::expected<std::string, std::optional<int>> exp{stdx::unexpected(1)};

  EXPECT_FALSE(exp);
}

TEST(Expected, converting_construct) {
  stdx::expected<std::string, int> exp(
      stdx::expected<const char *, int>("somestr"));

  EXPECT_TRUE(exp);
}

TEST(ExpectedVoid, converting_construct) {
  stdx::expected<void, std::string> exp(
      stdx::expected<void, const char *const>(stdx::unexpect, "somestr"));

  ASSERT_FALSE(exp);
  EXPECT_EQ(exp.error(), "somestr");
}

TEST(Expected, move_construct) {
  stdx::expected<int, bool> exp{stdx::expected<int, bool>(1)};

  EXPECT_TRUE(exp);
}

TEST(Expected, copy_assign_expected) {
  stdx::expected<int, bool> exp(1);

  EXPECT_TRUE(exp);

  char a = 'a';
  char &b = a;

  exp = b;
}

TEST(Expected, copy_assign_unexpected) {
  stdx::expected<int, bool> exp(1);

  EXPECT_TRUE(exp);

  stdx::unexpected<bool> f(false);

  exp = f;

  EXPECT_FALSE(exp);
}

TEST(ExpectedVoid, copy_assign_unexpected) {
  stdx::expected<void, bool> exp;

  EXPECT_TRUE(exp);

  stdx::unexpected<bool> f(false);

  exp = f;

  EXPECT_FALSE(exp);
}

TEST(Expected, move_assign_unexpected) {
  stdx::expected<int, std::unique_ptr<int>> exp(1);

  EXPECT_TRUE(exp);

  stdx::unexpected<std::unique_ptr<int>> f(nullptr);

  exp = std::move(f);

  EXPECT_FALSE(exp);
}

TEST(ExpectedVoid, move_assign_unexpected) {
  stdx::expected<void, std::unique_ptr<int>> exp;

  EXPECT_TRUE(exp);

  stdx::unexpected<std::unique_ptr<int>> f(nullptr);

  exp = std::move(f);

  EXPECT_FALSE(exp);
}

TEST(Expected, swap_expected_expected) {
  stdx::expected<int, int> a(1);
  stdx::expected<int, int> b(2);

  ASSERT_TRUE(a.has_value());
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(a.value(), 1);
  EXPECT_EQ(b.value(), 2);

  std::swap(a, b);

  ASSERT_TRUE(a.has_value());
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(a.value(), 2);
  EXPECT_EQ(b.value(), 1);
}

TEST(Expected, swap_unexpected_unexpected) {
  stdx::expected<int, int> a = stdx::unexpected(1);
  stdx::expected<int, int> b = stdx::unexpected(2);

  ASSERT_FALSE(a.has_value());
  ASSERT_FALSE(b.has_value());
  EXPECT_EQ(a.error(), 1);
  EXPECT_EQ(b.error(), 2);

  std::swap(a, b);

  ASSERT_FALSE(a.has_value());
  ASSERT_FALSE(b.has_value());
  EXPECT_EQ(a.error(), 2);
  EXPECT_EQ(b.error(), 1);
}

TEST(Expected, swap_expected_unexpected) {
  stdx::expected<int, int> a(1);
  stdx::expected<int, int> b = stdx::unexpected(2);

  ASSERT_TRUE(a.has_value());
  ASSERT_FALSE(b.has_value());
  EXPECT_EQ(a.value(), 1);
  EXPECT_EQ(b.error(), 2);

  std::swap(a, b);

  ASSERT_FALSE(a.has_value());
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(a.error(), 2);
  EXPECT_EQ(b.value(), 1);
}

TEST(Expected, swap_unexpected_expected) {
  stdx::expected<int, int> a = stdx::unexpected(2);
  stdx::expected<int, int> b(1);

  ASSERT_FALSE(a.has_value());
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(a.error(), 2);
  EXPECT_EQ(b.value(), 1);

  std::swap(a, b);

  ASSERT_TRUE(a.has_value());
  ASSERT_FALSE(b.has_value());
  EXPECT_EQ(a.value(), 1);
  EXPECT_EQ(b.error(), 2);
}

/**
 * while std::string, std::error_code is nothing special, it triggers
 * a bug in stdx::expected with sunpro which generates a broken
 * move-assign operator which result in:
 *
 *   stdx::expected<std::string, std::error_code> a("foo");
 *   a = stdx::expected<std::string, std::error_code>("bar");
 *
 *   // with gcc/clang/msvc 'a' is now "bar"
 *   // with sunpro 'a' is ""
 */
TEST(Expected, T_string_E_std_error_code) {
  using namespace std::string_literals;
  auto test_func =
      [](bool success) -> stdx::expected<std::string, std::error_code> {
    if (!success) {
      return stdx::unexpected(make_error_code(std::errc::already_connected));
    }

    using ret_type = stdx::expected<std::string, std::error_code>;

    return ret_type{std::in_place, "from_func"s};
  };

  // std::string in libstdc++
  //
  // 8 byte ptr (either to short-string buffer, or external)
  // 8 byte length
  // 16 byte if (length < 16 - 1): short-string buf with trailing \0
  //         otherwise: 8 byte capacity

  // instantiation
  auto res =
      stdx::expected<std::string, std::error_code>(std::in_place, "initial"s);

  static_assert(std::is_move_assignable<std::string>::value);

  ASSERT_TRUE(res);
  ASSERT_EQ(res.value(), "initial"s);

  // move assignment (true)
  res = test_func(true);
  ASSERT_TRUE(res);

  EXPECT_EQ(res.value(), "from_func"s);

  // move assignment (false)
  res = test_func(false);
  ASSERT_FALSE(res);
  EXPECT_EQ(res,
            stdx::unexpected(make_error_code(std::errc::already_connected)));
  EXPECT_EQ(res.error(), make_error_code(std::errc::already_connected));

  // move assignment (true)
  res = test_func(true);
  ASSERT_TRUE(res);
  EXPECT_EQ(res.value(), "from_func");

  // copy construction
  auto res2 = res;
  ASSERT_TRUE(res2);
  EXPECT_EQ(res2, res);
  EXPECT_EQ(res2.value(), "from_func");
  EXPECT_EQ(res.value(), "from_func");

  // move construction
  auto res3 = std::move(res);
  ASSERT_TRUE(res3);
  EXPECT_EQ(res3.value(), "from_func");
  // don't inspect 'res' after it has been moved from.
  // EXPECT_EQ(res.value(), "");

  // prepare copy assignment
  res3 = test_func(true);
  ASSERT_TRUE(res3);
  EXPECT_EQ(res3.value(), "from_func");  // fail

  // copy assignment
  res = res3;
  ASSERT_EQ(res3, res);
  EXPECT_EQ(res3.value(), "from_func");  // fail
  EXPECT_EQ(res.value(), "from_func");
}

TEST(Expected, T_no_default_construct) {
  class no_default_construct {
   public:
    no_default_construct(int) {}
  };

  static_assert(!std::is_default_constructible<
                stdx::expected<no_default_construct, int>>::value);

  stdx::expected<no_default_construct, int> t_non_void(1);
}

TEST(Expected, T_no_copy_construct) {
  class no_copy_construct {
   public:
    no_copy_construct() = default;
    no_copy_construct(const no_copy_construct &) = delete;
    no_copy_construct(no_copy_construct &&) = delete;
  };

  static_assert(std::is_default_constructible<
                stdx::expected<no_copy_construct, int>>::value);
  static_assert(
      !std::is_copy_assignable<stdx::expected<no_copy_construct, int>>::value);
  static_assert(
      !std::is_move_assignable<stdx::expected<no_copy_construct, int>>::value);

  stdx::expected<no_copy_construct, int> t_non_void;
  EXPECT_TRUE(t_non_void);
}

// tests for the operator<< behaviour
static_assert(stdx::impl::is_to_stream_writable<std::ostream, int>::value);
static_assert(stdx::impl::is_to_stream_writable<std::ostream, double>::value);
static_assert(stdx::impl::is_to_stream_writable<
              std::ostream, stdx::expected<int, std::error_code>>::value);
static_assert(stdx::impl::is_to_stream_writable<
              std::ostream, stdx::expected<void, std::error_code>>::value);

static_assert(
    !stdx::impl::is_to_stream_writable<std::ostream, non_copyable>::value);

static_assert(!stdx::impl::is_to_stream_writable<
              std::ostream, non_copyable_no_default>::value);

static_assert(
    !stdx::impl::is_to_stream_writable<
        std::ostream, stdx::expected<non_copyable, std::error_code>>::value);

static_assert(!stdx::impl::is_to_stream_writable<
              std::ostream,
              stdx::expected<non_copyable_no_default, std::error_code>>::value);

TEST(ExpectedOstream, some_int) {
  std::ostringstream oss;

  oss << stdx::expected<int, std::error_code>(0);

  EXPECT_EQ(oss.str(), "0");
}

TEST(ExpectedAndThen, void_errc) {
  stdx::expected<void, std::errc> exp;

  auto r = exp.and_then([]() -> stdx::expected<void, std::errc> { return {}; });
  ASSERT_TRUE(r);
}

TEST(ExpectedAndThen, int_errc) {
  stdx::expected<int, std::errc> exp{1};

  auto r = exp.and_then(
      [](auto const &) -> stdx::expected<int, std::errc> { return {2}; });
  ASSERT_TRUE(r);
  EXPECT_EQ(r.value(), 2);
}

TEST(ExpectedAndThen, void_errc_refref) {
  auto r = stdx::expected<void, std::errc>{}.and_then(
      []() -> stdx::expected<void, std::errc> { return {}; });
  ASSERT_TRUE(r);
}

TEST(ExpectedAndThen, move_only_type) {
  auto r = stdx::expected<void, int>{}.and_then(
      []() -> stdx::expected<std::unique_ptr<int>, int> {
        return std::make_unique<int>(2);
      });
  ASSERT_TRUE(r);
  EXPECT_EQ(*r.value(), 2);
}

TEST(ExpectedAndThen, move_only_unwrapped) {
  auto r = stdx::expected<void, int>{}
               .and_then([]() -> stdx::expected<std::unique_ptr<int>, int> {
                 return std::make_unique<int>(2);
               })
               .and_then([](const auto &v) -> stdx::expected<int, int> {
                 return *v;
               });

  // last .and_then() return type wins
  EXPECT_TRUE((std::is_same_v<decltype(r), stdx::expected<int, int>>));

  ASSERT_TRUE(r);
  EXPECT_EQ(r.value(), 2);
}

TEST(ExpectedAndThen, move_only_error_code) {
  auto r =
      stdx::expected<void, non_copyable>{}
          .and_then([]() -> stdx::expected<int, non_copyable> {
            return stdx::unexpected(non_copyable{});
          })
          .and_then([](const auto &v) -> stdx::expected<int, non_copyable> {
            return v;
          });

  // last .and_then() return type wins
  EXPECT_TRUE((std::is_same_v<decltype(r), stdx::expected<int, non_copyable>>));

  // one 'unexpected' along the way.
  ASSERT_FALSE(r);
}

TEST(ExpectedOrElse, rewrite_error_code) {
  auto r =
      stdx::expected<void, std::error_code>{
          stdx::unexpected(make_error_code(std::errc::io_error))}
          .and_then([]() -> stdx::expected<int, std::error_code> { return 2; })
          .or_else([](const auto &
                      /* ec */) -> stdx::expected<int, std::error_code> {
            return stdx::unexpected(make_error_code(std::errc::bad_message));
          });

  // last .or_else() return type wins
  EXPECT_TRUE(
      (std::is_same_v<decltype(r), stdx::expected<int, std::error_code>>));

  ASSERT_FALSE(r);
  // rewritten
  EXPECT_EQ(r.error(), make_error_code(std::errc::bad_message));
}

TEST(ExpectedOrElse, make_happy_again) {
  auto r =
      stdx::expected<void, std::error_code>{
          stdx::unexpected(make_error_code(std::errc::io_error))}
          .and_then(
              []() -> stdx::expected<std::unique_ptr<int>, std::error_code> {
                // skipped
                return std::make_unique<int>(2);
              })
          .or_else(
              [](const auto &ec)
                  -> stdx::expected<std::unique_ptr<int>, std::error_code> {
                // error turned into a error-code.
                return {std::make_unique<int>(ec.value())};
              });

  // last .and_then() return type wins
  EXPECT_TRUE(
      (std::is_same_v<decltype(r),
                      stdx::expected<std::unique_ptr<int>, std::error_code>>));

  ASSERT_TRUE(r);
  // error-code is bubbled down and .or_else() returns success again.
  EXPECT_EQ(*r.value(), make_error_code(std::errc::io_error).value());
}

TEST(ExpectedOrElse, int_to_int) {
  auto r = stdx::expected<int, std::error_code>{1}.or_else(
      [](const auto & /* err */) -> stdx::expected<int, std::error_code> {
        return 2;
      });

  EXPECT_TRUE(
      (std::is_same_v<decltype(r), stdx::expected<int, std::error_code>>));

  ASSERT_TRUE(r);
  EXPECT_EQ(r.value(), 1);
}

TEST(ExpectedOrElse, void_to_void) {
  auto r = stdx::expected<void, std::error_code>{}.or_else(
      [](const auto & /* err */) -> stdx::expected<void, std::error_code> {
        return {};
      });

  EXPECT_TRUE(
      (std::is_same_v<decltype(r), stdx::expected<void, std::error_code>>));

  ASSERT_TRUE(r);
}

TEST(ExpectedTransform, int_to_int) {
  auto r = stdx::expected<int, std::error_code>{1}.transform(
      [](auto v) { return v + 1; });

  EXPECT_TRUE(
      (std::is_same_v<decltype(r), stdx::expected<int, std::error_code>>));

  ASSERT_TRUE(r);
  EXPECT_EQ(r.value(), 2);
}

TEST(ExpectedTransform, int_to_void) {
  auto r =
      stdx::expected<int, std::error_code>{1}.transform([](auto /* v */) {});

  EXPECT_TRUE(
      (std::is_same_v<decltype(r), stdx::expected<void, std::error_code>>));

  ASSERT_TRUE(r);
}

TEST(ExpectedTransform, void_to_void) {
  auto r = stdx::expected<void, std::error_code>{}.transform([]() {});

  EXPECT_TRUE(
      (std::is_same_v<decltype(r), stdx::expected<void, std::error_code>>));

  ASSERT_TRUE(r);
}

TEST(ExpectedTransform, void_to_int) {
  auto r = stdx::expected<void, std::error_code>{}.transform(
      []() { return int{1}; });

  EXPECT_TRUE(
      (std::is_same_v<decltype(r), stdx::expected<int, std::error_code>>));

  ASSERT_TRUE(r);
  EXPECT_EQ(r.value(), 1);
}

TEST(Expected, T_E_converting_copy_constructor_expected) {
  stdx::expected<uint8_t, uint8_t> a{2};
  stdx::expected<uint16_t, uint16_t> b = a;

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  EXPECT_EQ(a.value(), b.value());
}

TEST(Expected, T_E_converting_move_constructor_expected) {
  stdx::expected<uint16_t, uint16_t> b = stdx::expected<uint8_t, uint8_t>{2};

  ASSERT_TRUE(b);
  EXPECT_EQ(b.value(), 2);
}

TEST(Expected, T_E_converting_copy_assignment_expected) {
  stdx::expected<uint16_t, uint16_t> a{2};
  stdx::expected<uint16_t, uint16_t> b{4};

  b = a;

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  EXPECT_EQ(a.value(), b.value());
}

TEST(Expected, T_E_converting_move_assignment_expected) {
  stdx::expected<uint16_t, uint16_t> b{4};

  b = stdx::expected<uint8_t, uint16_t>{2};

  ASSERT_TRUE(b);
  EXPECT_EQ(b.value(), 2);
}

TEST(Expected, T_E_converting_value_constructor) {
  auto b = []() -> stdx::expected<std::string, uint16_t> { return {"abc"}; }();

  ASSERT_TRUE(b);
  EXPECT_EQ(b.value(), "abc");
}

TEST(Expected, T_E_construct_unexpect) {
  stdx::expected<uint16_t, uint16_t> b{stdx::unexpect, 24};

  ASSERT_FALSE(b);
  EXPECT_EQ(b.error(), 24);
}

TEST(Expected, T_E_construct_unexpect_pair) {
  stdx::expected<uint16_t, std::pair<int, int>> b{stdx::unexpect, 24, 42};

  ASSERT_FALSE(b);
  EXPECT_EQ(b.error(), std::make_pair(24, 42));
}

TEST(Expected, T_E_construct_unexpected) {
  stdx::expected<uint16_t, int> b = stdx::unexpected{24L};

  ASSERT_FALSE(b);
  EXPECT_EQ(b.error(), 24);
}

/*
 * a simple type with an explicit constructor.
 */
template <class T>
class Explicit {
 public:
  using value_type = T;

  explicit Explicit(value_type v) : v_{std::move(v)} {}

  bool operator==(const value_type &other) const { return v_ == other; }

 private:
  value_type v_;
};

/*
 * check explicit constructors work as expected.
 */
TEST(Expected, T_E_explicit_constructor_from_value) {
  {
    static_assert(!std::is_convertible_v<Explicit<int>, int>);
    static_assert(std::is_constructible_v<Explicit<int>, int>);

    // explicit constructor. Can't assign.
    //
    // Explicit<int> converted = 1;

    Explicit<int> explicit_constructed{1};  // works
  }

  {
    static_assert(
        !std::is_convertible_v<stdx::expected<Explicit<int>, int>, int>);
    static_assert(
        std::is_constructible_v<stdx::expected<Explicit<int>, int>, int>);

    // fails as expected
    // stdx::expected<Explicit<int>, int> converted = 1;

    stdx::expected<Explicit<int>, int> explicit_constructed{1};  // works
  }
}

template <class V>
using R = stdx::expected<V, std::error_code>;

/*
 * check that construct between void and non-void expected-types fails.
 */
TEST(Expected, construct_from_other_void) {
  // construct/convert from itself, works
  static_assert(std::is_constructible_v<R<int>, R<int>>);
  static_assert(std::is_convertible_v<R<int>, R<int>>);

  static_assert(std::is_constructible_v<R<void>, R<void>>);
  static_assert(std::is_convertible_v<R<void>, R<void>>);

  // but not void <-> non-void
  static_assert(!std::is_constructible_v<R<int>, R<void>>);
  static_assert(!std::is_convertible_v<R<void>, R<int>>);

  static_assert(!std::is_constructible_v<R<void>, R<int>>);
  static_assert(!std::is_convertible_v<R<int>, R<void>>);
}

/*
 * check if narrowing conversion is accepted.
 *
 * (std::optional doesn't fail/warning on narrowing conversion, and expected
 * isn't doing it either).
 */
TEST(Expected, construct_from_other_int) {
  // sanity-check: construct/convert from itself, works
  static_assert(std::is_constructible_v<R<uint8_t>, R<uint8_t>>);
  static_assert(std::is_convertible_v<R<uint8_t>, R<uint8_t>>);

  static_assert(std::is_constructible_v<R<uint16_t>, R<uint16_t>>);
  static_assert(std::is_convertible_v<R<uint16_t>, R<uint16_t>>);

  static_assert(std::is_constructible_v<R<uint8_t>, uint8_t>);
  static_assert(std::is_constructible_v<R<uint8_t>, R<uint8_t>>);

  // sanity-check: how does optional handles narrowing conversions?
  // -> no warning.
  {
    std::optional<uint8_t> o = 256 + 255;
    ASSERT_TRUE(o);
    EXPECT_EQ(o.value(), 255);
  }

  {
    std::optional<uint8_t> o{256 + 255};
    ASSERT_TRUE(o);
    EXPECT_EQ(o.value(), 255);
  }

  // check stdx::expected behaves as optional around "narrowing".
  static_assert(std::is_constructible_v<uint8_t, uint16_t>);

  // construct from T
  static_assert(std::is_constructible_v<R<uint8_t>, uint16_t>);
  {
    R<uint8_t> r{std::numeric_limits<uint16_t>::max()};
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 255);
  }
  // construct from expected<T, E>
  static_assert(std::is_constructible_v<R<uint8_t>, R<uint16_t>>);
  {
    R<uint8_t> r{R<uint16_t>{std::numeric_limits<uint16_t>::max()}};
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 255);
  }

  // sanity-check: basic type conversion
  static_assert(std::is_convertible_v<uint16_t, uint8_t>);

  // convert from T
  static_assert(std::is_convertible_v<uint16_t, R<uint8_t>>);
  {
    R<uint8_t> r = 256 + 255;
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 255);
  }
  // convert from expected<T, E>
  static_assert(std::is_convertible_v<R<uint16_t>, R<uint8_t>>);
  {
    R<uint8_t> r = R<uint16_t>{256 + 255};
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 255);
  }
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
