/*
  Copyright (c) 2019, 2021, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/stdx/expected.h"

#include <type_traits>  // is_move_constructible

#include <gmock/gmock.h>

#include "mysql/harness/stdx/expected_ostream.h"

TEST(Expected, T_trivial_default_construct_is_value) {
  stdx::expected<int, std::error_code> exp(0);

  EXPECT_TRUE(exp.has_value());
  EXPECT_TRUE(exp);
  EXPECT_EQ(exp.value(), 0);
  EXPECT_EQ(*exp, 0);
}

TEST(Expected, T_void_default_construct_is_value) {
  // with T=void, there is no value to fetch
  stdx::expected<void, std::error_code> exp;

  EXPECT_TRUE(exp.has_value());
  // there is no value() method
  EXPECT_TRUE(exp);
}

TEST(Expected, T_trivial_construct_from_error) {
  stdx::expected<int, std::error_code> exp(
      stdx::make_unexpected(make_error_code(std::errc::bad_address)));

  EXPECT_FALSE(exp.has_value());
  EXPECT_FALSE(exp);
  EXPECT_EQ(exp.error(), std::errc::bad_address);
}

TEST(Expected, T_void_construct_from_error) {
  stdx::expected<void, std::error_code> exp(
      stdx::make_unexpected(make_error_code(std::errc::bad_address)));

  EXPECT_FALSE(exp.has_value());
  EXPECT_FALSE(exp);
  EXPECT_EQ(exp.error(), std::errc::bad_address);

  // don't deref exp
}

TEST(Expected, T_trivial_operator_eq_error) {
  stdx::expected<int, std::error_code> exp(
      stdx::make_unexpected(make_error_code(std::errc::bad_address)));
  stdx::expected<int, std::error_code> exp2(
      stdx::make_unexpected(make_error_code(std::errc::bad_address)));

  EXPECT_TRUE(exp2 == exp);
  EXPECT_TRUE(exp == exp2);
  EXPECT_FALSE(exp2 != exp);
  EXPECT_FALSE(exp != exp2);
}

TEST(Expected, T_void_operator_eq_error) {
  stdx::expected<void, std::error_code> exp(
      stdx::make_unexpected(make_error_code(std::errc::bad_address)));
  stdx::expected<void, std::error_code> exp2(
      stdx::make_unexpected(make_error_code(std::errc::bad_address)));

  EXPECT_TRUE(exp2 == exp);
  EXPECT_TRUE(exp == exp2);
  EXPECT_FALSE(exp2 != exp);
  EXPECT_FALSE(exp != exp2);
}

TEST(Expected, T_trivial_operator_eq_error_unexpected) {
  stdx::expected<int, std::error_code> exp(
      stdx::make_unexpected(make_error_code(std::errc::bad_address)));
  auto unexp = stdx::make_unexpected(make_error_code(std::errc::bad_address));

  EXPECT_TRUE(exp == unexp);
  EXPECT_TRUE(unexp == exp);
  EXPECT_FALSE(exp != unexp);
  EXPECT_FALSE(unexp != exp);
}

TEST(Expected, T_trivial_operator_ne_error) {
  stdx::expected<int, std::error_code> exp(
      stdx::make_unexpected(make_error_code(std::errc::bad_address)));
  stdx::expected<int, std::error_code> exp2(
      stdx::make_unexpected(make_error_code(std::errc::not_supported)));

  EXPECT_NE(exp2, exp);
  EXPECT_NE(exp, exp2);
}

TEST(Expected, T_void_operator_ne_error) {
  stdx::expected<void, std::error_code> exp(
      stdx::make_unexpected(make_error_code(std::errc::bad_address)));
  stdx::expected<void, std::error_code> exp2(
      stdx::make_unexpected(make_error_code(std::errc::not_supported)));

  EXPECT_NE(exp2, exp);
  EXPECT_NE(exp, exp2);
}

TEST(Expected, T_trivial_operator_eq_value) {
  stdx::expected<int, std::error_code> exp(0);
  stdx::expected<int, std::error_code> exp2(0);

  EXPECT_EQ(exp2, exp);
  EXPECT_EQ(exp, exp2);
}

TEST(Expected, T_void_operator_eq_value) {
  stdx::expected<void, std::error_code> exp;
  stdx::expected<void, std::error_code> exp2;

  EXPECT_EQ(exp2, exp);
  EXPECT_EQ(exp, exp2);
}

TEST(Expected, T_trivial_operator_ne_value) {
  stdx::expected<int, std::error_code> exp(0);
  stdx::expected<int, std::error_code> exp2(1);

  EXPECT_NE(exp2, exp);
}

TEST(Expected, T_trivial_operator_ne_mixed_error_value) {
  stdx::expected<int, std::error_code> exp(0);
  stdx::expected<int, std::error_code> exp2(
      stdx::make_unexpected(make_error_code(std::errc::not_supported)));

  EXPECT_NE(exp2, exp);
  EXPECT_NE(exp, exp2);
}

TEST(Expected, T_void_operator_ne_mixed_error_value) {
  stdx::expected<void, std::error_code> exp;
  stdx::expected<void, std::error_code> exp2(
      stdx::make_unexpected(make_error_code(std::errc::not_supported)));

  EXPECT_NE(exp2, exp);
  EXPECT_NE(exp, exp2);
}

TEST(Expected, T_trivial_copy_construct_from_expected) {
  stdx::expected<int, std::error_code> exp(1);

  EXPECT_TRUE(exp.has_value());
  EXPECT_TRUE(exp);
  EXPECT_EQ(exp.value(), 1);
  EXPECT_EQ(*exp, 1);

  stdx::expected<int, std::error_code> exp2(exp);

  EXPECT_EQ(exp, exp2);
}

TEST(Expected, T_void_copy_construct_from_expected) {
  stdx::expected<void, std::error_code> exp;

  EXPECT_TRUE(exp.has_value());
  EXPECT_TRUE(exp);

  stdx::expected<void, std::error_code> exp2(exp);

  EXPECT_EQ(exp, exp2);
}

TEST(Expected, T_trivial_move_construct_from_expected) {
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

TEST(Expected, T_void_move_construct_from_expected) {
  stdx::expected<void, std::error_code> exp;

  EXPECT_TRUE(exp.has_value());
  EXPECT_TRUE(exp);

  stdx::expected<void, std::error_code> exp2(std::move(exp));

  EXPECT_TRUE(exp2.has_value());
  EXPECT_TRUE(exp2);
}

TEST(Expected, T_trivial_value_or_error) {
  stdx::expected<int, std::error_code> exp(
      stdx::make_unexpected(make_error_code(std::errc::bad_address)));

  EXPECT_FALSE(exp.has_value());

  EXPECT_EQ(exp.value_or(2), 2);
}

TEST(Expected, T_trivial_value_or_value) {
  stdx::expected<int, std::error_code> exp(0);

  EXPECT_TRUE(exp.has_value());

  EXPECT_EQ(exp.value_or(2), 0);
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

static_assert(std::is_copy_constructible<copyable>::value, "");
static_assert(std::is_move_constructible<copyable>::value, "");
static_assert(!std::is_copy_constructible<non_copyable>::value, "");
static_assert(std::is_move_constructible<non_copyable>::value, "");
static_assert(!std::is_copy_assignable<non_copyable>::value, "");
static_assert(std::is_move_assignable<non_copyable>::value, "");

static_assert(!std::is_copy_constructible<void>::value, "");
static_assert(!std::is_copy_assignable<void>::value, "");
static_assert(std::is_copy_constructible<int>::value, "");
static_assert(std::is_copy_assignable<int>::value, "");
static_assert(std::is_copy_constructible<std::error_code>::value, "");
static_assert(std::is_copy_assignable<std::error_code>::value, "");
static_assert(
    std::is_copy_constructible<stdx::expected<void, std::error_code>>::value,
    "");
static_assert(
    std::is_copy_assignable<stdx::expected<void, std::error_code>>::value, "");
static_assert(
    std::is_copy_constructible<stdx::expected<int, std::error_code>>::value,
    "");
static_assert(std::is_copy_assignable<int>::value, "");
static_assert(std::is_copy_constructible<int>::value, "");
static_assert(std::is_copy_assignable<std::error_code>::value, "");
static_assert(std::is_copy_constructible<std::error_code>::value, "");
static_assert(
    std::is_copy_assignable<stdx::expected<int, std::error_code>>::value, "");
static_assert(!std::is_copy_constructible<
                  stdx::expected<non_copyable, std::error_code>>::value,
              "");
static_assert(!std::is_copy_assignable<
                  stdx::expected<non_copyable, std::error_code>>::value,
              "");

TEST(Expected, T_unique_ptr) {
  auto test_func = [](bool success)
      -> stdx::expected<std::unique_ptr<int>, std::error_code> {
    if (!success) {
      return stdx::make_unexpected(
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
      return stdx::make_unexpected(
          make_error_code(std::errc::operation_not_supported));
    }

    return {1};
  };

  auto res_true = test_func(true);
  ASSERT_TRUE(res_true);

  auto res_false = test_func(false);
  ASSERT_FALSE(res_false);
  EXPECT_EQ(res_false, stdx::make_unexpected(make_error_code(
                           std::errc::operation_not_supported)));

  auto res = std::move(res_true);
  ASSERT_EQ(res.has_value(), test_func(true).has_value());

  // move
  res = std::move(res_false);
  ASSERT_EQ(res.has_value(), test_func(false).has_value());
  EXPECT_EQ(res, stdx::make_unexpected(
                     make_error_code(std::errc::operation_not_supported)));
}

TEST(Expected, T_noncopyable) {
  auto test_func =
      [](bool success) -> stdx::expected<non_copyable, std::error_code> {
    if (!success) {
      return stdx::make_unexpected(
          make_error_code(std::errc::operation_not_supported));
    }

    return {};
  };

  auto res_true = test_func(true);
  ASSERT_TRUE(res_true);

  auto res_false = test_func(false);
  ASSERT_FALSE(res_false);
  EXPECT_EQ(res_false, stdx::make_unexpected(make_error_code(
                           std::errc::operation_not_supported)));

  auto res = std::move(res_true);
  ASSERT_EQ(res.has_value(), test_func(true).has_value());

  res = std::move(res_false);
  ASSERT_EQ(res.has_value(), test_func(false).has_value());
  EXPECT_EQ(res, stdx::make_unexpected(
                     make_error_code(std::errc::operation_not_supported)));
}

TEST(Expected, T_trivial) {
  auto test_func = [](bool success) -> stdx::expected<int, std::error_code> {
    if (!success) {
      return stdx::make_unexpected(
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

TEST(Expected, T_trivial_convertible) {
  auto test_func = [](bool success) -> stdx::expected<char, std::error_code> {
    if (!success) {
      return stdx::make_unexpected(
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

TEST(Expected, T_void) {
  auto test_func = [](bool success) -> stdx::expected<void, std::error_code> {
    if (!success) {
      return stdx::make_unexpected(
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
  EXPECT_EQ(res, stdx::make_unexpected(
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

TEST(Expected, T_void_E_void) {
  auto test_func = [](bool success) -> stdx::expected<void, void> {
    if (!success) {
      return stdx::make_unexpected();
    }

    return {};
  };

  // instantiation
  auto res = test_func(true);
  ASSERT_TRUE(res);

  // move assignment
  res = test_func(false);
  ASSERT_FALSE(res);
  EXPECT_EQ(res, stdx::make_unexpected());

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

TEST(Expected, T_trivial_E_void) {
  auto test_func = [](bool success) -> stdx::expected<int, void> {
    if (!success) {
      return stdx::make_unexpected();
    }

    return {1};
  };

  // instantiation
  auto res = test_func(true);
  ASSERT_TRUE(res);
  ASSERT_EQ(res.value(), 1);

  // move assignment
  res = test_func(false);
  ASSERT_FALSE(res);
  EXPECT_EQ(res, stdx::make_unexpected());

  // move assignment
  res = test_func(true);
  ASSERT_TRUE(res);
  ASSERT_EQ(res.value(), 1);

  // copy construction
  auto res2 = res;
  ASSERT_EQ(res2, res);
  ASSERT_EQ(res2.value(), 1);
  ASSERT_EQ(res.value(), 1);

  // move construction
  auto res3 = std::move(res);
  ASSERT_TRUE(res3);
  ASSERT_EQ(res3.value(), 1);

  // the move an int, is the same a copy. Not much to test here
  // ASSERT_EQ(res.value(), {});

  // copy assignment
  res = res3;
  ASSERT_EQ(res3, res);
  ASSERT_EQ(res3.value(), 1);
  ASSERT_EQ(res.value(), 1);
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
      return stdx::make_unexpected(
          make_error_code(std::errc::already_connected));
    }

    return {stdx::in_place, "from_func"s};
  };

  // std::string in libstdc++
  //
  // 8 byte ptr (either to short-string buffer, or external)
  // 8 byte length
  // 16 byte if (length < 16 - 1): short-string buf with trailing \0
  //         otherwise: 8 byte capacity

  // instantiation
  auto res =
      stdx::expected<std::string, std::error_code>(stdx::in_place, "initial"s);

  static_assert(std::is_move_assignable<std::string>::value, "");

  ASSERT_TRUE(res);
  ASSERT_EQ(res.value(), "initial"s);

  // move assignment (true)
  res = test_func(true);
  ASSERT_TRUE(res);

  EXPECT_EQ(res.value(), "from_func"s);

  // move assignment (false)
  res = test_func(false);
  ASSERT_FALSE(res);
  EXPECT_EQ(res, stdx::make_unexpected(
                     make_error_code(std::errc::already_connected)));
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

  // prepare copy assignement
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
                    stdx::expected<no_default_construct, void>>::value,
                "");
  static_assert(!std::is_default_constructible<
                    stdx::expected<no_default_construct, int>>::value,
                "");

  stdx::expected<no_default_construct, void> t_void(1);
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
                    stdx::expected<no_copy_construct, void>>::value,
                "");
  static_assert(
      !std::is_copy_assignable<stdx::expected<no_copy_construct, void>>::value,
      "");
  static_assert(
      !std::is_move_assignable<stdx::expected<no_copy_construct, void>>::value,
      "");

  static_assert(std::is_default_constructible<
                    stdx::expected<no_copy_construct, int>>::value,
                "");
  static_assert(
      !std::is_copy_assignable<stdx::expected<no_copy_construct, int>>::value,
      "");
  static_assert(
      !std::is_move_assignable<stdx::expected<no_copy_construct, int>>::value,
      "");

  stdx::expected<no_copy_construct, void> t_void;
  stdx::expected<no_copy_construct, int> t_non_void;
}

// tests for the operator<< behaviour
static_assert(stdx::impl::is_to_stream_writable<std::ostream, int>::value, "");
static_assert(stdx::impl::is_to_stream_writable<std::ostream, double>::value,
              "");
static_assert(stdx::impl::is_to_stream_writable<
                  std::ostream, stdx::expected<int, std::error_code>>::value,
              "");
static_assert(stdx::impl::is_to_stream_writable<
                  std::ostream, stdx::expected<int, void>>::value,
              "");
static_assert(stdx::impl::is_to_stream_writable<
                  std::ostream, stdx::expected<void, void>>::value,
              "");
static_assert(stdx::impl::is_to_stream_writable<
                  std::ostream, stdx::expected<void, std::error_code>>::value,
              "");

static_assert(
    !stdx::impl::is_to_stream_writable<std::ostream, non_copyable>::value, "");

static_assert(!stdx::impl::is_to_stream_writable<
                  std::ostream, non_copyable_no_default>::value,
              "");

static_assert(
    !stdx::impl::is_to_stream_writable<
        std::ostream, stdx::expected<non_copyable, std::error_code>>::value,
    "");

static_assert(
    !stdx::impl::is_to_stream_writable<
        std::ostream,
        stdx::expected<non_copyable_no_default, std::error_code>>::value,
    "");

static_assert(std::is_move_constructible<
                  stdx::expected<std::unique_ptr<int>, void>>::value,
              "");

TEST(ExpectedOstream, some_int) {
  std::ostringstream oss;

  oss << stdx::expected<int, std::error_code>(0);

  EXPECT_EQ(oss.str(), "0");
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
