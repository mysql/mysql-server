/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_STRING_HEX_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_STRING_HEX_H_

#include <cassert>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace helper {
namespace string {

template <typename Container>
std::string hex(const Container &c) {
  std::ostringstream os;
  auto end = std::end(c);
  auto it = std::begin(c);
  static_assert(sizeof(decltype(*it)) == 1);

  for (; it != end; ++it) {
    os << std::setfill('0') << std::setw(2) << std::hex;
    os << (int)*it;
  }
  return os.str();
}

inline uint8_t unhex_character(const char c) {
  static_assert('0' < 'A');
  static_assert('A' < 'a');
  if (c > 'f')
    throw std::runtime_error("Invalid character in hexadecimal value.");
  if (c >= 'a') return c - 'a';
  if (c > 'F')
    throw std::runtime_error("Invalid character in hexadecimal value.");
  if (c >= 'A') return c - 'A';
  if (c > '9')
    throw std::runtime_error("Invalid character in hexadecimal value.");
  if (c >= '0') return c - '0';
  throw std::runtime_error("Invalid character in hexadecimal value.");
}

template <typename Container>
Container unhex(const std::string &h) {
  Container result;
  assert(h.size() % 2 == 0);

  for (std::size_t i = 1; i < h.size(); i += 2) {
    uint8_t v = unhex_character(h[i - 1]) * 16 + unhex_character(h[i]);
    result.push_back(v);
  }
  return result;
}

}  // namespace string
}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_STRING_HEX_H_
