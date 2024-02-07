/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef INCLUDE_STRING_TEMPLATE_UTILS_H_
#define INCLUDE_STRING_TEMPLATE_UTILS_H_

#include <cstring>
#include <string>

static inline size_t length_of_string(const std::string &s) { return s.size(); }

static inline size_t length_of_string(const char *s) { return strlen(s); }

template <typename S>
size_t sum_sizeof_strings(S s) {
  return length_of_string(s);
}

template <typename H, typename... T>
size_t sum_sizeof_strings(H head, T... tail) {
  return length_of_string(head) + sum_sizeof_strings(tail...);
}

template <typename S>
void concatenate_acc(std::string *acc, S s) {
  *acc += s;
}

template <typename H, typename... T>
void concatenate_acc(std::string *acc, H head, T... tail) {
  *acc += head;
  concatenate_acc(acc, tail...);
}

/**
  Concatenate a list of std::string or '\0'-terminated strings

  @note throws std::bad_alloc

  @param s      List of std::string or '\0'-terminated strings

  @returns concatenated string
*/
template <typename... S>
std::string concatenate(S... s) {
  std::string acc;
  acc.reserve(sum_sizeof_strings(s...));
  concatenate_acc(&acc, s...);
  return acc;
}

#endif  // INCLUDE_STRING_TEMPLATE_UTILS_H_
