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

#include <gtest/gtest.h>                          // TEST
#include "mysql/allocators/memory_resource.h"     // Memory_resource
#include "mysql/debugging/my_scoped_trace.h"      // MY_SCOPED_TRACE
#include "mysql/debugging/oom_test.h"             // oom_test_assignable_object
#include "mysql/debugging/unittest_assertions.h"  // ASSERT_OK
#include "mysql/sets/sets.h"                      // Map_interval_container
#include "mysql/utils/return_status.h"            // Return_status

namespace {

using namespace mysql;

// Describes the type of values stored in an interval set: the data type is @c
// int, the minimum value is 1, and the maximum value is maxint.
using My_set_traits = sets::Int_set_traits<int, 1>;

// Describes the type of values stored in an interval set: the data type is @c
// int, the minimum value is 1, and the maximum value, exclusive, is 10.
using My_set_traits_max10 = sets::Int_set_traits<int, 1, 10>;

/// Interval where endpoints are as described by My_set_traits.
using My_interval = sets::Interval<My_set_traits>;

/// Interval where endpoints are as described by My_set_traits_max10.
using My_interval_max10 = sets::Interval<My_set_traits_max10>;

/// Interval container where endpoints are as described by My_set_traits,
/// and the backing container is std::map.
using My_interval_container = sets::Map_interval_container<My_set_traits>;

// ==== Test errors occurring while parsing interval containers ====

// NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
enum class Expect_ok { no, yes };

void test_text_parser(std::string_view str, std::string_view expected_str,
                      Expect_ok expect_ok,
                      const std::string_view &error_message) {
  MY_SCOPED_TRACE("str=", str, " expected_str=", expected_str);

  My_interval_container cont;
  auto ret = strconv::decode(strconv::Boundary_set_text_format{}, str, cont);
  MY_SCOPED_TRACE("cont=", cont);
  ASSERT_EQ(ret.is_ok(), expect_ok == Expect_ok::yes)
      << strconv::throwing::encode_text(ret);
  ASSERT_EQ(ret.is_prefix_ok(), true) << strconv::throwing::encode_text(ret);
  ASSERT_EQ(ret.is_found(), true) << strconv::throwing::encode_text(ret);
  ASSERT_EQ(strconv::throwing::encode_text(ret), error_message);

  My_interval_container expected_cont;
  auto expected_ret =
      decode(strconv::Boundary_set_text_format{}, expected_str, expected_cont);
  MY_SCOPED_TRACE("expected_cont=", expected_cont);
  ASSERT_TRUE(expected_ret.is_ok());
  ASSERT_TRUE(sets::is_equal(cont, expected_cont));
}

void test_text_parser_ok(std::string_view str, std::string_view expected_str) {
  test_text_parser(str, expected_str, Expect_ok::yes, "OK");
}

void test_text_parser_error(std::string_view str, std::string_view expected_str,
                            const std::string &error_message) {
  test_text_parser(str, expected_str, Expect_ok::no, error_message);
}

TEST(LibsSetsIntervalsErrors, Parsing) {
  My_interval_container cont;

  // Funny but valid strings
  test_text_parser_ok("", "");
  test_text_parser_ok(" ", "");
  test_text_parser_ok(",", "");
  test_text_parser_ok(",,", "");
  test_text_parser_ok("1,", "1");
  test_text_parser_ok("1,,", "1");
  test_text_parser_ok(",1", "1");
  test_text_parser_ok(",,1", "1");
  test_text_parser_ok(",1,", "1");
  test_text_parser_ok(",,1,,", "1");
  test_text_parser_ok("1,,,,2", "1-2");
  test_text_parser_ok("1-1", "1");
  test_text_parser_ok("1-0", "");
  test_text_parser_ok("9-1", "");
  test_text_parser_ok("1,1", "1");
  test_text_parser_ok("8-9,6-7,7-8", "6-9");

  // Invalid strings
  test_text_parser_error("a", "",
                         "Expected number at the beginning "
                         "of the string: \"a\"");
  test_text_parser_error("\1", "",
                         "Expected number at the beginning "
                         "of the string: \"\\x01\"");
  test_text_parser_error("-", "",
                         "Expected number at the beginning "
                         "of the string: \"-\"");
  test_text_parser_error(
      "1-", "1",
      "Expected number after 2 characters, marked by [HERE] in: \"1-[HERE]\"");
  test_text_parser_error("-1", "",
                         "Interval start out of range at the beginning of the "
                         "string: \"-1\"");
  test_text_parser_error("12345678901234567890", "",
                         "Number out of range at the beginning of the string: "
                         "\"12345678901234567890\"");
  test_text_parser_error("1-2,a", "1-2",
                         "Expected number after 4 "
                         "characters, marked by [HERE] in: \"1-2,[HERE]a\"");
  test_text_parser_error("1 2", "1",
                         "Expected \",\" after 2 "
                         "characters, marked by [HERE] in: \"1 [HERE]2\"");
  test_text_parser_error("1-2 3", "1-2",
                         "Expected \",\" after 4 "
                         "characters, marked by [HERE] in: \"1-2 [HERE]3\"");
  test_text_parser_error(
      "1-2,\1", "1-2",
      "Expected number after 4 "
      "characters, marked by [HERE] in: \"1-2,[HERE]\\x01\"");
  test_text_parser_error("1-2,-", "1-2",
                         "Expected number after 4 "
                         "characters, marked by [HERE] in: \"1-2,[HERE]-\"");
  test_text_parser_error("1-2,1-", "1-2",
                         "Expected number after 6 characters, marked by [HERE] "
                         "in: \"1-2,1-[HERE]\"");
  test_text_parser_error("1-2,-1", "1-2",
                         "Interval start out of range after 4 characters, "
                         "marked by [HERE] in: \"1-2,[HERE]-1\"");
  test_text_parser_error("1-2,12345678901234567890", "1-2",
                         "Number out of range after 4 characters, marked by "
                         "[HERE] in: \"1-2,[HERE]12345678901234567890\"");
  test_text_parser_error("1-12345678901234567890", "1",
                         "Number out of range after 2 characters, marked by "
                         "[HERE] in: \"1-[HERE]12345678901234567890\"");
}

// ==== Test out of memory errors in interval container operations ====

auto oomt(auto &obj, const auto &func) {
  return debugging::oom_test_assignable_object(obj, func);
}

TEST(LibsSetsIntervalsErrors, OutOfMemory) {
  My_interval_container cont1;
  My_interval_container cont2;

  ASSERT_OK(cont2.insert(2));
  ASSERT_OK(cont2.insert(4));

  ASSERT_EQ(oomt(cont1, [&](My_interval_container &c) { return c.insert(3); }),
            1);
// ASSERT_GE/ASSERT_EQ are macros that need to stringify their arguments
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#ifdef _WIN32
// MSVC standard containers allocate extra objects in debug mode, hence
// 'GE' rather than 'EQ'.
#define ASSERT_COUNT(RESULT, EXPECTED) ASSERT_GE(RESULT, EXPECTED)
#else
#define ASSERT_COUNT(RESULT, EXPECTED) ASSERT_EQ(RESULT, EXPECTED)
#endif
  // NOLINTEND(cppcoreguidelines-macro-usage)

  ASSERT_COUNT(
      oomt(cont1, [&](My_interval_container &c) { return c.assign(cont2); }),
      2);
  ASSERT_COUNT(
      oomt(cont1,
           [&](My_interval_container &c) { return c.inplace_union(cont2); }),
      2);

  ASSERT_OK(cont1.inplace_union(My_interval::throwing_make(1, 6)));

  ASSERT_COUNT(oomt(cont1,
                    [&](My_interval_container &c) {
                      return c.inplace_intersect(cont2);
                    }),
               1);
  ASSERT_COUNT(
      oomt(cont1,
           [&](My_interval_container &c) { return c.inplace_subtract(cont2); }),
      2);
}

TEST(LibsSetsIntervalsErrors, IntervalOutOfBounds) {
  My_interval_max10 iv;
  ASSERT_EQ(iv.start(), 1);
  ASSERT_EQ(iv.exclusive_end(), 2);
  auto ret = iv.assign(2, 8);

  auto assert_err = [&ret, &iv] {
    ASSERT_EQ(ret, utils::Return_status::error);
    ASSERT_EQ(iv.start(), 2);
    ASSERT_EQ(iv.exclusive_end(), 8);
  };

  {
    MY_SCOPED_TRACE("assign");
    ret = iv.assign(0, 5);  // out of bounds
    assert_err();
    ret = iv.assign(5, 11);  // out of bounds
    assert_err();
    ret = iv.assign(8, 2);  // out of order
    assert_err();
  }
  {
    MY_SCOPED_TRACE("set_start");
    ret = iv.set_start(-1);  // out of bounds
    assert_err();
    ret = iv.set_start(8);  // out of order
    assert_err();
  }
  {
    MY_SCOPED_TRACE("set_exclusive_end");
    ret = iv.set_exclusive_end(11);  // out of bounds
    assert_err();
    ret = iv.set_exclusive_end(1);  // out of order
    assert_err();
  }

  auto assert_throw = [&iv](const auto &func) {
    bool caught = false;
    try {
      func();
    } catch (const std::domain_error &) {
      caught = true;
    }
    ASSERT_TRUE(caught) << iv.start() << " " << iv.exclusive_end();
    ASSERT_EQ(iv.start(), 2);
    ASSERT_EQ(iv.exclusive_end(), 8);
  };

  {
    MY_SCOPED_TRACE("throwing_assign");
    assert_throw([&iv] { iv.throwing_assign(0, 5); });
    assert_throw([&iv] { iv.throwing_assign(5, 11); });
    assert_throw([&iv] { iv.throwing_assign(8, 2); });
  }
  {
    MY_SCOPED_TRACE("throwing_set_start");
    assert_throw([&iv] { iv.throwing_set_start(-1); });
    assert_throw([&iv] { iv.throwing_set_start(8); });
  }
  {
    MY_SCOPED_TRACE("throwing_set_exclusive_end");
    assert_throw([&iv] { iv.throwing_set_exclusive_end(11); });
    assert_throw([&iv] { iv.throwing_set_exclusive_end(2); });
  }
  {
    MY_SCOPED_TRACE("throwing_make");
    assert_throw([] { std::ignore = My_interval_max10::throwing_make(0, 5); });
    assert_throw([] { std::ignore = My_interval_max10::throwing_make(5, 11); });
    assert_throw([] { std::ignore = My_interval_max10::throwing_make(8, 2); });
  }
}

}  // namespace
