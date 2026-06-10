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

#include <gtest/gtest.h>                      // TEST
#include <string>                             // string
#include "mysql/debugging/my_scoped_trace.h"  // MY_SCOPED_TRACE
#include "mysql/strconv/strconv.h"            // get_format

// ==== Purpose ====
//
// Verify that the functionality to compose Format, Repeat, and Checker objects
// works.
//
// ==== Test requirements ====
//
// R1. A parse options object may be any of the following:
//     - an object of a class satisfying Is_format
//     - a Repeat object
//     - a checker object, which is either an invokable taking no argument or an
//       invokable taking two arguments of which the first is a Parser&.
//     - an object resulting in combining two or three of the above, at most one
//       of each category, using operator|.
//
// R1. For parse options objects that contain a Format, get_format must compile
//     and return the correct type.
//
// R2.1. For parse options objects that contain a Repeat, get_repeat must return
//       a copy of that Repeat object.
//
// R2.2. For parse options objects that do not contain a Repeat, get_repeat must
//       return Repeat::one().
//
// R3.1. For parse options objects that contain a checker function,
//       check_parsed_object must invoke the checker function.
//
// R3.2. For parse options objects that do not contain a checker function,
//       check_parsed_object must do nothing.

namespace {

using namespace mysql;
using strconv::Repeat;

// NOLINTBEGIN(performance-enum-size): silence clang-tidy's pointless hint
enum class Test_format { no, yes };
enum class Checker_increment { no, yes };
// NOLINTEND(performance-enum-size)

int checker_counter{0};

/// Verify all the requirements for a given parse options object.
///
/// @tparam Expected_format_t The expected return type for get_format, or void
/// if we do not expect that get_format is defined.
///
/// @param opt The parse options object to check
///
/// @param expected_repeat The expected value of get_repeat.
///
/// @param checker_increment If yes, expect that the object has a checker
/// function that increments the value of `checker_counter` by 1. Otherwise,
/// expect that the value remains unchanged.
template <class Expected_format_t, class Parse_options>
void test(const Parse_options &opt,
          const strconv::Is_repeat auto &expected_repeat,
          Checker_increment checker_increment) {
  MY_SCOPED_TRACE(expected_repeat.min(), " ", expected_repeat.max(), " ",
                  checker_increment == Checker_increment::yes);

  auto format = strconv::get_format(opt);
  static_assert(std::same_as<decltype(format), Expected_format_t>);

  auto old_checker_counter = checker_counter;
  strconv::invoke_checker(opt);
  if (checker_increment == Checker_increment::yes)
    ASSERT_EQ(checker_counter, old_checker_counter + 1);
  else
    ASSERT_EQ(checker_counter, old_checker_counter);

  ASSERT_EQ(strconv::get_repeat(opt).min(), expected_repeat.min());
  ASSERT_EQ(strconv::get_repeat(opt).max(), expected_repeat.max());
}

TEST(LibsStringsParseOptions, Basic) {
  using BF = strconv::Binary_format;
  using TF = strconv::Text_format;

  auto checker_nop = strconv::Checker([] {});
  auto checker_inc = strconv::Checker([&] { ++checker_counter; });

  // ==== Without format ====
  {
    MY_SCOPED_TRACE("Without format");

    // Neither repeat nor checker
    test<TF>(strconv::Empty_parse_options{},  //
             Repeat::one(), Checker_increment::no);

    // Only repeat
    test<TF>(Repeat::one(),  //
             Repeat::one(), Checker_increment::no);
    test<TF>(Repeat::at_least(7),  //
             Repeat::at_least(7), Checker_increment::no);
    test<TF>(Repeat::any(),  //
             Repeat::any(), Checker_increment::no);
    test<TF>(Repeat::at_most(7),  //
             Repeat::at_most(7), Checker_increment::no);

    // Only checker
    test<TF>(checker_nop,  //
             Repeat::one(), Checker_increment::no);
    test<TF>(checker_inc,  //
             Repeat::one(), Checker_increment::yes);

    // Repeat and checker
    test<TF>(Repeat(1, 3) | checker_nop,  //
             Repeat(1, 3), Checker_increment::no);
    test<TF>(Repeat::at_least(7) | checker_nop,  //
             Repeat::at_least(7), Checker_increment::no);
    test<TF>(checker_inc | Repeat::any(),  //
             Repeat::any(), Checker_increment::yes);
    test<TF>(checker_inc | Repeat::exact(9),  //
             Repeat::exact(9), Checker_increment::yes);
  }

  // ==== With format ====
  {
    MY_SCOPED_TRACE("With format");

    // Neither repeat nor checker
    test<BF>(BF{},  //
             Repeat::one(), Checker_increment::no);

    // Only repeat
    test<BF>(BF{} | Repeat::one(),  //
             Repeat::one(), Checker_increment::no);
    test<BF>(BF{} | Repeat::at_least(7),  //
             Repeat::at_least(7), Checker_increment::no);
    test<BF>(Repeat::any() | BF{},  //
             Repeat::any(), Checker_increment::no);
    test<BF>(Repeat::at_most(7) | BF{},  //
             Repeat::at_most(7), Checker_increment::no);

    // Only checker
    test<BF>(BF{} | checker_nop,  //
             Repeat::one(), Checker_increment::no);
    test<BF>(BF{} | checker_nop,  //
             Repeat::one(), Checker_increment::no);
    test<BF>(checker_inc | BF{},  //
             Repeat::one(), Checker_increment::yes);
    test<BF>(checker_inc | BF{},  //
             Repeat::one(), Checker_increment::yes);

    // Repeat and checker
    test<BF>(BF{} | Repeat::range(1, 3) | checker_nop,  //
             Repeat::range(1, 3), Checker_increment::no);
    test<BF>(BF{} | checker_nop | Repeat::exact(4),  //
             Repeat::exact(4), Checker_increment::no);
    test<BF>(Repeat::optional() | BF{} | checker_nop,  //
             Repeat::optional(), Checker_increment::no);
    test<BF>(Repeat::at_least(5) | checker_inc | BF{},  //
             Repeat::at_least(5), Checker_increment::yes);
    test<BF>(checker_inc | BF{} | Repeat::at_most(6),  //
             Repeat::at_most(6), Checker_increment::yes);
    test<BF>(checker_inc | Repeat::any() | BF{},  //
             Repeat::any(), Checker_increment::yes);
  }
}

}  // namespace
