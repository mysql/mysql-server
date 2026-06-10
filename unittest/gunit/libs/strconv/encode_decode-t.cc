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
#include <limits>                             // numeric_limits
#include <sstream>                            // stringstream
#include <string>                             // string
#include "mysql/debugging/my_scoped_trace.h"  // MY_SCOPED_TRACE
#include "mysql/strconv/strconv.h"            // encode

namespace {

using namespace mysql;

// ==== encode + decode: Text_format, Binary_format, Fixint_binary_format ====
//
// Test that conversion to string and back gives the original value.
//
// For all 8-, 16-, 32-, and 64-bit signed and unsigned integers, we test a
// number of values. We test all powers of two, and the minimum and maximum for
// the data type, and all those numbers plus 1, plus 2, minus 1, or minus 2.
//
// For strings, test a few different values.

/// Encode value to a string using the given format, decode back, and assert
/// that the result equals the original value.
///
/// @tparam Value_t Type of value to test.
///
/// @param format Identifies the encoding format.
///
/// @param value Value to encode.
template <class Value_t>
void test_format_value(const strconv::Is_format auto &format,
                       const Value_t &value) {
  MY_SCOPED_TRACE(typeid(format).name());
  MY_SCOPED_TRACE(value);
  std::string str = strconv::throwing::encode(format, value);
  Value_t decoded_value{};
  auto ret = strconv::decode(format, str, decoded_value);
  ASSERT_TRUE(ret.is_ok()) << typeid(Value_t).name() << " '" << value << "' "
                           << strconv::throwing::encode_text(ret);
  ASSERT_EQ(value, decoded_value);
}

/// Test encoding/decoding for the given integral value, in text, binary and
/// binary-with-fixed-length-integers formats.
///
/// @param value Integral value to encode.
void test_value(const std::integral auto &value) {
  MY_SCOPED_TRACE("test_value(std::integral)");
  test_format_value(strconv::Text_format{}, value);
  test_format_value(strconv::Binary_format{}, value);
  test_format_value(strconv::Fixint_binary_format{}, value);
}

/// Test encoding/decoding for the integral data type's min value, min value
/// plus 1, max value, and max value minus 1.
///
/// @tparam Int_t Integer type to test.
template <std::integral Int_t>
void test_int_minmax() {
  test_value(std::numeric_limits<Int_t>::min());
  test_value(std::numeric_limits<Int_t>::min() + Int_t(1));
  test_value(std::numeric_limits<Int_t>::max() - Int_t(1));
  test_value(std::numeric_limits<Int_t>::max());
}

/// Test encoding/decoding for the given value, and for value-1, value-2,
/// value+1, value+2.
///
/// @tparam Int_t Integer type to test.
///
/// @param value Value to encode.
template <std::integral Int_t>
void test_int_prevnext(const Int_t &value) {
  test_value(value - Int_t(2));
  test_value(value - Int_t(1));
  test_value(value);
  test_value(value - Int_t(1));
  test_value(value + Int_t(2));
}

/// Test encoding/decoding for a chosen set of values from the given unsigned
/// integer type.
///
/// This tests all powers of two, for the previous and next value from all
/// powers of two, for the data type's minimum and maximum values, and for the
/// minimum plus 1 and maximum minus 1.
///
/// @tparam Int_t Unsigned integral type to test.
template <std::unsigned_integral Int_t>
void test_int_type() {
  MY_SCOPED_TRACE(typeid(Int_t).name());
  test_int_minmax<Int_t>();
  for (int i = 2; i < std::numeric_limits<Int_t>::digits; ++i) {
    test_int_prevnext(Int_t(1) << i);
  }
}

/// Test encoding/decoding for a chosen set of values from the given signed
/// integer type.
///
/// This tests all powers of two, their negated values, for the previous and
/// next value from all powers of two and their negated values, for the data
/// type's minimum and maximum values, and for the minimum plus 1 and maximum
/// minus 1.
///
/// @tparam Int_t Signed integral type to test.
template <std::signed_integral Int_t>
void test_int_type() {
  MY_SCOPED_TRACE(typeid(Int_t).name());
  test_int_minmax<Int_t>();
  test_int_prevnext(Int_t(0));
  for (int i = 2; i < std::numeric_limits<Int_t>::digits - 1; ++i) {
    test_int_prevnext(Int_t(1) << i);
    test_int_prevnext(-(Int_t(1) << i));
  }
}

/// Test encoding/decoding for a chosen set of values of all integer data types
/// and for std::string.
TEST(LibsStringsToStringFromString, Integers) {
  test_int_type<int8_t>();
  test_int_type<uint8_t>();
  test_int_type<int16_t>();
  test_int_type<uint16_t>();
  test_int_type<int32_t>();
  test_int_type<uint32_t>();
  test_int_type<int64_t>();
  test_int_type<uint64_t>();
}

/// Test encoding/decoding for the given std::string, in binary and
/// binary-with-fixed-length-integers formats. (There is no decode function
/// for strings in text format, since the text format does not encode the length
/// of the string.)
///
/// @param value String value to encode.
void test_value(const std::string &value) {
  MY_SCOPED_TRACE("test_value(std::string)");
  test_format_value(strconv::Binary_format{}, value);
  test_format_value(strconv::Fixint_binary_format{}, value);
}

/// Test encoding/decoding for strings.
TEST(LibsStringsToStringFromString, Strings) {
  test_value(std::string{""});
  // NOLINTNEXTLINE(bugprone-string-literal-with-embedded-nul): intentional
  test_value(std::string{"\0"});
  test_value(std::string{"x"});
  test_value(std::string{"xyz"});
}

/// Test encoding/decoding for strings of the given length.
///
/// @param length Length of the string.
void test_repeated_strings(std::size_t length) {
  test_value(std::string(length, ' '));
  test_value(std::string(length, 'a'));
  test_value(std::string(length, '\0'));
  test_value(std::string(length, '\xff'));
}

/// Test encoding/decoding for strings of a number of different lengths.
void test_long_strings() {
  for (int i = 1; i < 18; ++i) {
    std::size_t val = std::size_t(1) << i;
    test_repeated_strings(val - std::size_t(2));
    test_repeated_strings(val - std::size_t(1));
    test_repeated_strings(val);
    test_repeated_strings(val + std::size_t(1));
    test_repeated_strings(val + std::size_t(2));
  }
}

/// Test encoding/decoding for strings of a number of different lengths.
TEST(LibsStringsToStringFromString, LongStrings) { test_long_strings(); }

}  // namespace
