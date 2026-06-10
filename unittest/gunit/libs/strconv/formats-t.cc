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

#include <gtest/gtest.h>            // TEST
#include <string>                   // string
#include "mysql/strconv/strconv.h"  // encode

// ==== Purpose ====
//
// Verify that the algorithm to resolve the correct format to use works. For
// given <Format_t, Object_t...> types, it should use the first format `F` in
// the following sequence for which `encode_impl(F, T, Object_t...)` is
// defined:
//
//  `Format_t`,
//  `Default_format<T, Object_t...>`,
//  `Parent_1`
//  `Parent_2`
//  `Parent_3`
//  ...
//
// where `Parent_1` is the return type of `Format_t::parent()` and `Parent_i`
// is the return type of `Parent_{i-1}` for i>1.
//
// ==== Test requirements ====
//
// R1. When `encode_impl(Format_t, Target_t, Object_t)` is defined,
//     `encode` should invoke it.
//
// R2. Otherwise, if `Default_format<Format_t, Object_t>` is defined and
//     `encode_impl<Default_format<Format_t, Object_t>, T, Object_t>` is
//     defined, `encode` should invoke it.
//
// R3. Otherwise, if `Format_t::parent()` is defined, and
//     `encode_impl(Format_t::parent(), T, Object_t)` is defined, `encode`
//     should invoke it. Otherwise, if any ancestor obtained by invoking
//     `Format::parent()::parent()...` has `encode_impl` defined for it,
//     `encode` should invoke the first such `encode_impl` function.
//
// R4. If no viable `encode_impl` function is found by the procedure above,
//     the call to `encode` should not compile.

// ==== No parent format, no default format ====

namespace none {
class Base {};
class Derived : public Base {};
}  // namespace none

namespace mysql::strconv::none {
class Base_text_format : public Format_base {};
class Derived_text_format : public Format_base {};
}  // namespace mysql::strconv::none

namespace mysql::strconv {
static void encode_impl(const none::Base_text_format &,
                        Is_string_target auto &target, const ::none::Base &) {
  target.write_raw("none:base");
}

static void encode_impl(const none::Derived_text_format &,
                        Is_string_target auto &target,
                        const ::none::Derived &) {
  target.write_raw("none:derived");
}
}  // namespace mysql::strconv

// ==== Parent format, no default format ====

namespace par {
class Base {};
class Derived : public Base {};
}  // namespace par

namespace mysql::strconv::par {
class Base_text_format : public Format_base {
 public:
  [[nodiscard]] auto parent() const { return mysql::strconv::Text_format{}; }
};
class Derived_text_format : public Format_base {
 public:
  [[nodiscard]] auto parent() const { return Base_text_format{}; }
};
}  // namespace mysql::strconv::par

namespace mysql::strconv {
static void encode_impl(const par::Base_text_format &,
                        Is_string_target auto &target, const ::par::Base &) {
  target.write_raw("par:base");
}

static void encode_impl(const par::Derived_text_format &,
                        Is_string_target auto &target, const ::par::Derived &) {
  target.write_raw("par:derived");
}
}  // namespace mysql::strconv

// ==== Default format, no parent format ====

namespace def {
class Base {};
class Derived : public Base {};
}  // namespace def

namespace mysql::strconv::def {
class Base_text_format : public Format_base {};
class Derived_text_format : public Format_base {};
}  // namespace mysql::strconv::def

namespace mysql::strconv {
[[nodiscard]] static auto get_default_format(const Text_format &,
                                             const ::def::Base &) {
  return def::Base_text_format{};
}

[[nodiscard]] static auto get_default_format(const Text_format &,
                                             const ::def::Derived &) {
  return def::Derived_text_format{};
}

static void encode_impl(const def::Base_text_format &,
                        Is_string_target auto &target, const ::def::Base &) {
  target.write_raw("def:base");
}

static void encode_impl(const def::Derived_text_format &,
                        Is_string_target auto &target, const ::def::Derived &) {
  target.write_raw("def:derived");
}
}  // namespace mysql::strconv

// ==== Parent format and default format ====

namespace par_def {
class Base {};
class Derived : public Base {};
}  // namespace par_def

namespace mysql::strconv::par_def {
class Base_text_format : public Format_base {
 public:
  [[nodiscard]] auto parent() const { return mysql::strconv::Text_format{}; }
};
class Derived_text_format : public Format_base {
 public:
  [[nodiscard]] auto parent() const { return Base_text_format{}; }
};
}  // namespace mysql::strconv::par_def

namespace mysql::strconv {
[[nodiscard]] static auto get_default_format(const Text_format &,
                                             const ::par_def::Base &) {
  return par_def::Base_text_format{};
}

[[nodiscard]] static auto get_default_format(const Text_format &,
                                             const ::par_def::Derived &) {
  return par_def::Derived_text_format{};
}

static void encode_impl(const par_def::Base_text_format &,
                        Is_string_target auto &target,
                        const ::par_def::Base &) {
  target.write_raw("par_def:base");
}

static void encode_impl(const par_def::Derived_text_format &,
                        Is_string_target auto &target,
                        const ::par_def::Derived &) {
  target.write_raw("par_def:derived");
}
}  // namespace mysql::strconv

// ==== Tests ====

namespace {

// There is currently no automatic way to test that something does not compile.
// The macro ASSERT_DOES_NOT_COMPILE is just is a way to annotate the code for
// the benefit of human readers; whatever is passed to it will be removed by the
// preprocessor and never be seen by the C++ compiler.
//
// If you need to verify that the code actually does not compile, you can
// semi-automate it as follows: Edit this file manually to produce a compilation
// error. From the build output, copy-paste the command line. Set the
// environment variable COMPILE_WITH_ERRORS to that command line. Set the
// current directory to a build directory in a git tree. Run the following
// script:
/*
#!/bin/sh
# Compile
$COMPILE_WITH_ERRORS -DCHECK_COMPILATION_FAILURES 2>&1 |
    # Replace esc by !
    tr '\033' '!' |
    # Remove ansi colors
    sed -E 's/!\[[0-9;]*[mK]//g' |
    # Extract only line numbers for invocation of macro
    sed -n -E 's/[^ ]*formats-t.cc:([0-9]+):[0-9]+: *required from here/\1/p' |
    # Sort, remove duplicates, and write to file
    sort | uniq > actual_error_lines
cat $(git rev-parse --show-toplevel)/unittest/gunit/libs/strings/formats-t.cc |
    # Number the lines
    nl -ba -fa -ha -v1 |
    # Extract line numbers of lines invoking the macro
    sed -n -E s'/^\s*([0-9]+)\s*ASSERT.*DOES_NOT_COMPILE.*.?/\1/p' |
    # Write to file
    cat > expected_error_lines
# Compare the files.
diff -U0 expected_error_lines actual_error_lines &&
    echo 'Test passed' || echo 'Test failure: see above'
*/
// This method is fragile and depends on formatting, compiler output format,
// etc. It was tested on mysql-trunk in 2025 with gcc.
#ifdef CHECK_COMPILATION_FAILURES
#define ASSERT_DOES_NOT_COMPILE(CODE) CODE
#else
#define ASSERT_DOES_NOT_COMPILE(CODE)
#endif

TEST(LibsStringsFormats, Basic) {
  // Objects in namespace `none`, which have neither parent format nor default
  // format.
  ASSERT_EQ(mysql::strconv::throwing::encode(
                mysql::strconv::none::Base_text_format{}, none::Base{}),
            "none:base");
  ASSERT_EQ(mysql::strconv::throwing::encode(
                mysql::strconv::none::Base_text_format{}, none::Derived{}),
            "none:base");
  ASSERT_EQ(mysql::strconv::throwing::encode(
                mysql::strconv::none::Derived_text_format{}, none::Derived{}),
            "none:derived");
  ASSERT_DOES_NOT_COMPILE(
      std::ignore = mysql::strconv::encode(
          mysql::strconv::none::Derived_text_format{}, none::Base{}));
  ASSERT_DOES_NOT_COMPILE(std::ignore = mysql::strconv::encode(
                              mysql::strconv::none::Base_text_format{}, 1));
  ASSERT_DOES_NOT_COMPILE(std::ignore = mysql::strconv::encode(
                              mysql::strconv::none::Derived_text_format{}, 1));
  ASSERT_DOES_NOT_COMPILE(std::ignore = mysql::strconv::encode(
                              mysql::strconv::Text_format{}, none::Base{}));
  ASSERT_DOES_NOT_COMPILE(std::ignore = mysql::strconv::encode(
                              mysql::strconv::Text_format{}, none::Derived{}));

  // Objects in namespace `par`, which have parent format but not default
  // format.
  ASSERT_EQ(mysql::strconv::throwing::encode(
                mysql::strconv::par::Base_text_format{}, par::Base{}),
            "par:base");
  ASSERT_EQ(mysql::strconv::throwing::encode(
                mysql::strconv::par::Base_text_format{}, par::Derived{}),
            "par:base");
  ASSERT_EQ(mysql::strconv::throwing::encode(
                mysql::strconv::par::Derived_text_format{}, par::Derived{}),
            "par:derived");
  ASSERT_EQ(mysql::strconv::encode(mysql::strconv::par::Derived_text_format{},
                                   par::Base{}),
            "par:base");
  ASSERT_EQ(mysql::strconv::encode(mysql::strconv::par::Base_text_format{}, 1),
            "1");
  ASSERT_EQ(
      mysql::strconv::encode(mysql::strconv::par::Derived_text_format{}, 1),
      "1");
  ASSERT_DOES_NOT_COMPILE(std::ignore = mysql::strconv::encode(
                              mysql::strconv::Text_format{}, par::Base{}));
  ASSERT_DOES_NOT_COMPILE(std::ignore = mysql::strconv::encode(
                              mysql::strconv::Text_format{}, par::Derived{}));

  // Objects in namespace `def`, which have default format but not parent
  // format.
  ASSERT_EQ(mysql::strconv::throwing::encode(
                mysql::strconv::def::Base_text_format{}, def::Base{}),
            "def:base");
  ASSERT_EQ(mysql::strconv::throwing::encode(
                mysql::strconv::def::Base_text_format{}, def::Derived{}),
            "def:base");
  ASSERT_EQ(mysql::strconv::throwing::encode(
                mysql::strconv::def::Derived_text_format{}, def::Derived{}),
            "def:derived");
  ASSERT_DOES_NOT_COMPILE(
      std::ignore = mysql::strconv::encode(
          mysql::strconv::def::Derived_text_format{}, def::Base{}));
  ASSERT_DOES_NOT_COMPILE(std::ignore = mysql::strconv::encode(
                              mysql::strconv::def::Base_text_format{}, 1));
  ASSERT_DOES_NOT_COMPILE(std::ignore = mysql::strconv::encode(
                              mysql::strconv::def::Derived_text_format{}, 1));
  ASSERT_EQ(mysql::strconv::throwing::encode(mysql::strconv::Text_format{},
                                             def::Base{}),
            "def:base");
  ASSERT_EQ(mysql::strconv::throwing::encode(mysql::strconv::Text_format{},
                                             def::Derived{}),
            "def:derived");

  // Objects in namespace `none`, which have both parent format and default
  // format.
  ASSERT_EQ(mysql::strconv::throwing::encode(
                mysql::strconv::par_def::Base_text_format{}, par_def::Base{}),
            "par_def:base");
  ASSERT_EQ(
      mysql::strconv::throwing::encode(
          mysql::strconv::par_def::Base_text_format{}, par_def::Derived{}),
      "par_def:base");
  ASSERT_EQ(
      mysql::strconv::throwing::encode(
          mysql::strconv::par_def::Derived_text_format{}, par_def::Derived{}),
      "par_def:derived");
  ASSERT_EQ(
      mysql::strconv::encode(mysql::strconv::par_def::Derived_text_format{},
                             par_def::Base{}),
      "par_def:base");
  ASSERT_EQ(
      mysql::strconv::encode(mysql::strconv::par_def::Base_text_format{}, 1),
      "1");
  ASSERT_EQ(
      mysql::strconv::encode(mysql::strconv::par_def::Derived_text_format{}, 1),
      "1");
  ASSERT_EQ(
      mysql::strconv::encode(mysql::strconv::Text_format{}, par_def::Base{}),
      "par_def:base");
  ASSERT_EQ(
      mysql::strconv::encode(mysql::strconv::Text_format{}, par_def::Derived{}),
      "par_def:derived");
}

}  // namespace
