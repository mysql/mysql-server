/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

class DefaultHex {
 public:
  void operator()(std::ostringstream &os) {
    os << std::setfill('0') << std::setw(2) << std::hex;
  }

  template <typename ValueType>
  int operator()(const ValueType &v) {
    return v;
  }
};

template <typename Container, typename Transform = DefaultHex>
std::string hex(const Container &c) {
  std::ostringstream os;
  Transform trans;
  auto end = std::end(c);
  auto it = std::begin(c);
  static_assert(sizeof(decltype(*it)) == 1);

  for (; it != end; ++it) {
    trans(os);
    os << trans(*it);
  }
  return os.str();
}

inline bool get_unhex_character(const char c, uint8_t *out) {
  static_assert('0' < 'A');
  static_assert('A' < 'a');
  static_assert('0' < '9');

  if (c > 'f') return false;

  if (c >= 'a') {
    *out = c - 'a' + 10;
    return true;
  }

  if (c > 'F') return false;

  if (c >= 'A') {
    *out = c - 'A' + 10;
    return true;
  }

  if (c > '9') return false;

  if (c >= '0') {
    *out = c - '0';
    return true;
  }

  return false;
}

inline bool get_unhex_character_or_throw(const char c, uint8_t *out) {
  if (get_unhex_character(c, out)) return true;

  throw std::runtime_error("Invalid character in hexadecimal value.");

  return false;
}

using HexConverter = bool (*)(const char c, uint8_t *out);

inline uint8_t unhex_character(const char c) {
  uint8_t result;
  if (!get_unhex_character(c, &result))
    throw std::runtime_error("Invalid character in hexadecimal value.");
  return result;
}

template <HexConverter converter, typename IT1, typename IT2>
bool get_hex_skip(IT1 &it, const IT2 &end, uint8_t *out) {
  bool result;
  do {
    if (it == end) return false;
    result = converter(*it, out);
    ++it;
  } while (!result);
  return result;
}

template <typename Container,
          HexConverter converter = get_unhex_character_or_throw>
Container unhex(const std::string &h) {
  Container result;
  uint8_t v1, v2;

  for (auto i = h.begin(); i != h.end();) {
    if (!get_hex_skip<converter>(i, h.end(), &v2)) break;
    if (!get_hex_skip<converter>(i, h.end(), &v1)) break;
    uint8_t v = v2 * 16 + v1;
    result.push_back(v);
  }
  return result;
}

}  // namespace string
}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_STRING_HEX_H_
