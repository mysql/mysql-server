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
#include <cstring>                                // memcpy
#include <limits>                                 // numeric_limits
#include <sstream>                                // stringstream
#include <string>                                 // string
#include "mysql/debugging/my_scoped_trace.h"      // MY_SCOPED_TRACE
#include "mysql/debugging/unittest_assertions.h"  // ASSERT_VOID
#include "mysql/strconv/encode/out_str.h"         // Is_out_str
#include "mysql/strconv/encode/out_str_write.h"   // out_str_write
#include "mysql/strconv/encode/string_target.h"   // Is_string_target

// ==== Requirements ====
//
// 1. Success tests.
//
// We test the following scenarios:
//
// - Representations:
//   - std::string based on char *, unsigned char *, or std::byte
//   - raw pointers based on char *, unsigned char *, or std::byte, and the size
//     represented either using an end pointer or an integer length. In the
//     latter case, length represented as uint64_t, int64_t, uint32_t, int32_t,
//     size_t, or ptrdiff_t.
// - For raw pointers, either null-terminated or not.
// - Fixed size, or growable where the initial buffer has not been allocated, or
//   growable where the initial buffer has been allocated but does not have
//   sufficient size, or has been allocated to sufficient size.
//
// In all the scenarios, we verify that:
//
// - Writing a string succeeds
// - The resulting size is as expected
// - The resulting data is as expected
// - The null-termination byte has been written if required; otherwise not. (We
//   only verify that the byte has not been written in case the output has not
//   grown; otherwise it cannot be checked.)
// - In the scenarios where the string needs to reallocate, the data pointer has
//   changed.
// - In the scenarios where the string does not need to reallocate, the data
//   pointer has not changed.
//
// 2. Death tests.
//
// We test the following scenarios:
//
// - Raw pointer representations based on char *, unsigned char *, or std::byte,
//   and the size represented either using an end pointer or an integer length.
//   In the latter case, length represented as uint64_t, int64_t, uint32_t,
//   int32_t, size_t, or ptrdiff_t.
// - Either null-terminated or not.
//
// In all the scenarios, we verify that:
//
// - out_str_fixed_nz raises an assertion when the first argument is an array
//   and the length given by the second argument is greater than the array size.
// - out_str_fixed_z raises an assertion when the first argument is an array and
//   the length given by the second argument is greater than the array size
//   minus 1.
namespace {

// ==== Basic definitions ====

using namespace mysql;
using strconv::Null_terminated;
int scenario_count{0};

// The capacity of a default-constructed string.
std::size_t default_string_capacity() {
  static std::string string_with_default_capacity;
  return string_with_default_capacity.capacity();
}

// The size of input string.
//
// We want to test scenarios with a string whose capacity is bigger a
// default-constructed string, but smaller than the input string. Therefore we
// return 1 plus the capacity of a string on which we reserved space greater
// than the default capacity.
std::size_t input_string_length_uncached() {
  std::string string_with_more_than_default_capacity;
  string_with_more_than_default_capacity.reserve(default_string_capacity() + 1);
  return string_with_more_than_default_capacity.capacity() + 1;
}

// The size of the input string.
//
// This is identical to input_string_length_uncached, but caches the result in
// a static local variable.
std::size_t input_string_length() {
  static std::size_t ret = input_string_length_uncached();
  return ret;
}

// The input string, defined as a sequence of 'x' characters of length
// `input_string_length`.
std::string_view get_input_string() {
  static std::string input_string(input_string_length(), 'x');
  return input_string;
}

// NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
enum class Shall_grow { no, yes };

// ==== Execution of one scenario ====

// The "string producing function": this just copies `in` to `out_str`.
template <strconv::Is_out_str Out_str_t>
[[nodiscard]] auto copy_str(std::string_view in, const Out_str_t &out_str) {
  return strconv::out_str_write(
      out_str,
      [&](strconv::Is_string_target auto &target) { target.write_raw(in); });
}

// Produce a string into `out_str` and verify all requirements.
template <strconv::Is_out_str Out_str_t>
void test_out_str(const Out_str_t &out_str, Null_terminated null_terminated,
                  Shall_grow shall_grow) {
  ++scenario_count;
  std::string_view input_string = get_input_string();
  char *old_buf = out_str.data();
  if constexpr (strconv::Is_out_str_fixed<Out_str_t>) {
    ASSERT_VOID(copy_str(input_string, out_str));
  } else {
    ASSERT_OK(copy_str(input_string, out_str));
  }
  ASSERT_EQ(out_str.size(), input_string.size());
  ASSERT_EQ(input_string,
            std::string_view(out_str.data(), input_string.size()));
  if (null_terminated == Null_terminated::yes) {
    ASSERT_EQ(out_str.data()[out_str.size()], '\0');
  } else {
    if (shall_grow == Shall_grow::no) {
      // Verify the byte we initially filled with has not been touched.
      // We can't verify that if the string has been reallocated.
      ASSERT_EQ(out_str.data()[out_str.size()], '\1');
    }
  }
  if (shall_grow == Shall_grow::yes) {
    ASSERT_NE(out_str.data(), old_buf);
  } else {
    ASSERT_EQ(out_str.data(), old_buf);
  }
}

// ==== Execution of scenarios with ptr representations ====

// Execute test scenarios for ptr representations, with a given buffer size,
// with all combinations of:
//
// - length defined by integer or by pointer to past-the-end character
// - null-terminated or not null-terminated
template <class Char_t, class Size_t>
void test_ptr_representations(auto alloc, Size_t size_arg, auto make_z,
                              auto make_nz, Shall_grow shall_grow) {
  {
    MY_SCOPED_TRACE("ptr+size, null-terminated");
    Char_t *str = alloc();
    Size_t size = size_arg;
    test_out_str(make_z(str, size), Null_terminated::yes, shall_grow);
    std::free(str);
  }

  {
    MY_SCOPED_TRACE("ptr+end, null-terminated");
    Char_t *str = alloc();
    Char_t *end = str + size_arg;
    test_out_str(make_z(str, end), Null_terminated::yes, shall_grow);
    std::free(str);
  }

  {
    MY_SCOPED_TRACE("ptr+size, non-null-terminated");
    Char_t *str = alloc();
    Size_t size = size_arg;
    test_out_str(make_nz(str, size), Null_terminated::no, shall_grow);
    std::free(str);
  }

  {
    MY_SCOPED_TRACE("ptr+end, non-null-terminated");
    Char_t *str = alloc();
    Char_t *end = str + size_arg;
    test_out_str(make_nz(str, end), Null_terminated::no, shall_grow);
    std::free(str);
  }
}

// Execute test scenarios for array representations, with a given buffer size,
// with all combinations of:
//
// - length defined by integer or by pointer to past-the-end character
// - null-terminated or not null-terminated
template <class Char_t, class Size_t>
void test_array_representations() {
  {
    MY_SCOPED_TRACE("array+size, null-terminated");
    Char_t str[10000];
    Size_t size = 9999;
    std::memset(str, '\1', sizeof(str));
    test_out_str(strconv::out_str_fixed_z(str, size), Null_terminated::yes,
                 Shall_grow::no);
  }

  {
    MY_SCOPED_TRACE("array+end, null-terminated");
    Char_t str[10000];
    Char_t *end = str + 9999;
    std::memset(str, '\1', sizeof(str));
    test_out_str(strconv::out_str_fixed_z(str, end), Null_terminated::yes,
                 Shall_grow::no);
  }

  {
    MY_SCOPED_TRACE("array+size, non-null-terminated");
    Char_t str[10000];
    Size_t size = 10000;
    std::memset(str, '\1', sizeof(str));
    test_out_str(strconv::out_str_fixed_nz(str, size), Null_terminated::no,
                 Shall_grow::no);
  }

  {
    MY_SCOPED_TRACE("array+end, non-null-terminated");
    Char_t str[10000];
    Char_t *end = str + 10000;
    std::memset(str, '\1', sizeof(str));
    test_out_str(strconv::out_str_fixed_nz(str, end), Null_terminated::no,
                 Shall_grow::no);
  }
}

// Execute all test scenarios with ptr representations, for the given
// Char_t/Size_t combination.
template <class Char_t, class Size_t>
void test_ptr() {
  // Return a function that allocates `size` bytes.
  auto get_alloc = [](std::size_t size) {
    return [=] {
      auto ret = reinterpret_cast<Char_t *>(std::malloc(size));
      std::memset(ret, '\1', size);
      return ret;
    };
  };
  // Function that returns nullptr.
  auto get_null = [] { return nullptr; };

  auto make_out_str_growable_z = [](auto &x, auto &y) {
    return strconv::out_str_growable_z(x, y);
  };
  auto make_out_str_growable_nz = [](auto &x, auto &y) {
    return strconv::out_str_growable_nz(x, y);
  };
  auto make_out_str_fixed_z = [](auto &x, auto &y) {
    return strconv::out_str_fixed_z(x, y);
  };
  auto make_out_str_fixed_nz = [](auto &x, auto &y) {
    return strconv::out_str_fixed_nz(x, y);
  };

  {
    MY_SCOPED_TRACE("Growable with initial nullptr buffer");
    test_ptr_representations<Char_t, Size_t>(
        get_null, 0, make_out_str_growable_z, make_out_str_growable_nz,
        Shall_grow::yes);
  }

  {
    MY_SCOPED_TRACE(
        "Growable with initial allocated buffer of insufficient size");
    test_ptr_representations<Char_t, Size_t>(
        get_alloc(input_string_length() - 1), input_string_length() - 2,
        make_out_str_growable_z, make_out_str_growable_nz, Shall_grow::yes);
  }

  {
    MY_SCOPED_TRACE(
        "Growable with initial allocated buffer of sufficient size");

    test_ptr_representations<Char_t, Size_t>(
        get_alloc(input_string_length() + 1), input_string_length(),
        make_out_str_growable_z, make_out_str_growable_nz, Shall_grow::no);
  }

  {
    MY_SCOPED_TRACE("Fixed with initial allocated buffer of sufficient size");

    test_ptr_representations<Char_t, Size_t>(
        get_alloc(input_string_length() + 1), input_string_length(),
        make_out_str_fixed_z, make_out_str_fixed_nz, Shall_grow::no);
  }

  {
    MY_SCOPED_TRACE("Fixed with array buffer of sufficient size");
    test_array_representations<Char_t, Size_t>();
  }
}

// ==== Execution of scenarios with string representation ====

// Execute all test scenarios with string representation, for the given String_t
// type.
template <class String_t>
void test_string() {
  MY_SCOPED_TRACE("String");
  {
    MY_SCOPED_TRACE("Growable with default initial buffer size");
    std::string str;
    test_out_str(strconv::out_str_growable(str), Null_terminated::yes,
                 Shall_grow::yes);
  }
  {
    MY_SCOPED_TRACE(
        "Growable with non-default initial buffer of insufficient size");
    std::string str;
    str.reserve(default_string_capacity() + 1);
    test_out_str(strconv::out_str_growable(str), Null_terminated::yes,
                 Shall_grow::yes);
  }
  {
    MY_SCOPED_TRACE("Growable with initial buffer of sufficient size");
    std::string str;
    str.reserve(input_string_length() + 1);
    test_out_str(strconv::out_str_growable(str), Null_terminated::yes,
                 Shall_grow::no);
  }
  {
    MY_SCOPED_TRACE("Fixed with initial buffer of sufficient size");
    std::string str;
    str.reserve(input_string_length() + 1);
    test_out_str(strconv::out_str_fixed(str), Null_terminated::yes,
                 Shall_grow::no);
  }
}

// ==== Main test execution ====

// Execute all scenarios (ptr or string representation) for the given character
// type.
template <class Char_t>
void test_char_type() {
  test_ptr<Char_t, std::size_t>();
  test_ptr<Char_t, std::ptrdiff_t>();
  test_ptr<Char_t, int32_t>();
  test_ptr<Char_t, int64_t>();
  test_ptr<Char_t, uint32_t>();
  test_ptr<Char_t, uint64_t>();
  test_string<std::basic_string<Char_t>>();
}

/// Test all the scenarios.
TEST(LibsStringsOutStr, Exhaustive) {
  test_char_type<char>();
  test_char_type<unsigned char>();
  test_char_type<std::byte>();
  std::cout << "Total number of scenarios: " << scenario_count << "\n";
}

#ifndef NDEBUG  // assertions must be enabled

// ==== Death tests ====
//
// Verify that an assertion is raised in debug mode when passing `char[N]` where
// N is too small for the given size.

int death_scenario_count{0};

template <class Char_t, class Size_t>
void death_test() {
  Char_t buf[10];
  Size_t size{};
  Char_t *endptr{};

  size = 11;
  ASSERT_DEATH(std::ignore = strconv::out_str_fixed_nz(buf, size), "");
  // This is UB and produces UBSAN errors, so we comment it out.
  // If UBSAN is not enabled, ASSERT_DEATH typically passes (i.e., it hits
  // the assertion as expected).
  // endptr = buf + 11;
  // ASSERT_DEATH(std::ignore = strconv::out_str_fixed_nz(buf, endptr), "");

  size = 10;
  ASSERT_DEATH(std::ignore = strconv::out_str_fixed_z(buf, size), "");
  endptr = buf + 10;
  ASSERT_DEATH(std::ignore = strconv::out_str_fixed_z(buf, endptr), "");

  death_scenario_count += 4;
}

template <class Char_t>
void death_test_char_type() {
  death_test<Char_t, std::size_t>();
  death_test<Char_t, std::ptrdiff_t>();
  death_test<Char_t, int32_t>();
  death_test<Char_t, int64_t>();
  death_test<Char_t, uint32_t>();
  death_test<Char_t, uint64_t>();
}

TEST(LibsStringsOutStrDeath, InsufficientArraySize) {
  death_test_char_type<char>();
  death_test_char_type<unsigned char>();
  death_test_char_type<std::byte>();
  std::cout << "Total number of scenarios: " << death_scenario_count << "\n";
}

#endif  // ifndef NDEBUG

}  // namespace
