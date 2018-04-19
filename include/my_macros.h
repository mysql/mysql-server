/*
   Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MY_MACROS_INCLUDED
#define MY_MACROS_INCLUDED

/**
  @file include/my_macros.h
  Some common macros. Note that some of these should probably be replaced
  with code from \<algorithm\> or similar.
*/

#ifndef MYSQL_ABI_CHECK
#include <stddef.h>
#endif

/* Macros to make switching between C and C++ mode easier */
#ifdef __cplusplus
#define C_MODE_START extern "C" {
#define C_MODE_END }
#else
#define C_MODE_START
#define C_MODE_END
#endif

/* Make it easier to add conditional code in _expressions_ */
#ifdef _WIN32
#define IF_WIN(A, B) A
#else
#define IF_WIN(A, B) B
#endif

/*
  Two levels of macros are needed to stringify the
  result of expansion of a macro argument.
*/
#define QUOTE_ARG(x) #x               /* Quote argument (before cpp) */
#define STRINGIFY_ARG(x) QUOTE_ARG(x) /* Quote argument, after cpp */

#define MY_TEST(a) ((a) ? 1 : 0)
#define MY_MAX(a, b) ((a) > (b) ? (a) : (b))
#define MY_MIN(a, b) ((a) < (b) ? (a) : (b))
#define set_if_bigger(a, b)   \
  do {                        \
    if ((a) < (b)) (a) = (b); \
  } while (0)
#define set_if_smaller(a, b)  \
  do {                        \
    if ((a) > (b)) (a) = (b); \
  } while (0)
#define test_all_bits(a, b) (((a) & (b)) == (b))

/* Bug in developerstudio: use the C version */
#if defined(__cplusplus) && !defined(__SUNPRO_CC)

template <class T, size_t N>
constexpr size_t array_elements(T (&)[N]) noexcept {
  return N;
}

#else

// Less type-safe version that e.g. allows sending in pointers
// or STL containers without an error.
#define array_elements(A) ((size_t)(sizeof(A) / sizeof(A[0])))

#endif

#endif  // MY_MACROS_INCLUDED
