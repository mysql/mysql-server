/* Copyright (c) 2000, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <stdint.h>
#include <string.h>
#include <iterator>
#include <type_traits>

#include "integer_digits.h"
#include "m_string.h"  // IWYU pragma: keep

/*
  _dig_vec arrays are public because they are used in several outer places.
*/
const char _dig_vec_upper[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char _dig_vec_lower[] = "0123456789abcdefghijklmnopqrstuvwxyz";

/**
  Converts a 64-bit integer value to its character form and moves it to the
  destination buffer followed by a terminating NUL. If radix is -2..-36, val is
  taken to be SIGNED, if radix is 2..36, val is taken to be UNSIGNED. That is,
  val is signed if and only if radix is. All other radixes are treated as bad
  and nothing will be changed in this case.

  For conversion to decimal representation (radix is -10 or 10) one should use
  the optimized #int10_to_str() and #longlong10_to_str() functions instead.

  @param val the value to convert
  @param dst the buffer where the string representation should be stored
  @param radix radix of scale of notation
  @param upcase true if we should use upper-case digits

  @return pointer to the ending NUL character, or nullptr if radix is bad
*/
char *ll2str(int64_t val, char *dst, int radix, bool upcase) {
  char buffer[65];
  const char *const dig_vec = upcase ? _dig_vec_upper : _dig_vec_lower;
  auto uval = static_cast<uint64_t>(val);

  if (radix < 0) {
    if (radix < -36 || radix > -2) return nullptr;
    if (val < 0) {
      *dst++ = '-';
      /* Avoid integer overflow in (-val) for LLONG_MIN (BUG#31799). */
      uval = 0ULL - uval;
    }
    radix = -radix;
  } else if (radix > 36 || radix < 2) {
    return nullptr;
  }

  char *p = std::end(buffer);
  do {
    *--p = dig_vec[uval % radix];
    uval /= radix;
  } while (uval != 0);

  const size_t length = std::end(buffer) - p;
  memcpy(dst, p, length);
  dst[length] = '\0';
  return dst + length;
}

/**
  Converts integer to its string representation in decimal notation.

  This is a generic version which can be called either as #int10_to_str() (for
  long int) or as #longlong10_to_str() (for int64_t). It is optimized for the
  normal case of radix 10/-10. It takes only the sign of radix parameter into
  account and not its absolute value.

  @param val the value to convert
  @param dst the buffer where the string representation should be stored
  @param radix 10 if val is unsigned, -10 if val is signed

  @return pointer to the ending NUL character
*/
template <typename T>
static char *integer_to_string_base10(T val, char *dst, int radix) {
  static_assert(std::is_integral<T>::value && std::is_signed<T>::value,
                "The input value should be a signed integer.");

  using Unsigned_T = std::make_unsigned_t<T>;

  Unsigned_T uval = static_cast<Unsigned_T>(val);

  if (radix < 0) /* -10 */
  {
    if (val < 0) {
      *dst++ = '-';
      /* Avoid integer overflow in (-val) for LLONG_MIN (BUG#31799). */
      uval = Unsigned_T{0} - uval;
    }
  }

  const int digits = count_digits(uval);
  char *end = write_digits(uval, digits, dst);
  *end = '\0';
  return end;
}

/**
  Converts a long integer to its string representation in decimal notation.

  It is optimized for the normal case of radix 10/-10. It takes only the sign of
  radix parameter into account and not its absolute value.

  @param val the value to convert
  @param dst the buffer where the string representation should be stored
  @param radix 10 if val is unsigned, -10 if val is signed

  @return pointer to the ending NUL character
*/
char *int10_to_str(long int val, char *dst, int radix) {
  return integer_to_string_base10(val, dst, radix);
}

/**
  Converts a 64-bit integer to its string representation in decimal notation.

  It is optimized for the normal case of radix 10/-10. It takes only the sign of
  radix parameter into account and not its absolute value.

  @param val the value to convert
  @param dst the buffer where the string representation should be stored
  @param radix 10 if val is unsigned, -10 if val is signed

  @return pointer to the ending NUL character
*/
char *longlong10_to_str(int64_t val, char *dst, int radix) {
  return integer_to_string_base10(val, dst, radix);
}
