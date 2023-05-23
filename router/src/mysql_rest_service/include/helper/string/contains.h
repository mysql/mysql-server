/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_STRING_CONTAINS_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_STRING_CONTAINS_H_

#include <cstdint>
#include <string>

namespace helper {

inline bool is_empty(const std::string &str) { return str.empty(); }
inline bool is_empty(const char *str) { return *str == 0; }
inline const char *cstr(const char *str) { return str; }
inline const char *cstr(const std::string &str) { return str.c_str(); }

template <typename String1, typename String2>
bool contains(const String1 &value, const String2 &sst) {
  return nullptr != strstr(cstr(value), cstr(sst));
}

inline bool ends_with(const std::string &value, const std::string &sst) {
  if (sst.empty()) return false;

  auto pos = value.rfind(sst);
  if (value.npos == pos) return false;
  return value.length() - pos == sst.length();
}

template <typename String>
bool index(const std::string &value, const String &inside, uint32_t *idx) {
  auto pos = value.find(inside);

  if (value.npos == pos) return false;
  if (idx) *idx = static_cast<uint32_t>(pos);

  return true;
}

inline bool index(const char *value, const char *inside, uint32_t *idx) {
  auto ptr = strstr(value, inside);
  if (nullptr == ptr) return false;
  if (idx) *idx = reinterpret_cast<std::intptr_t>(value - ptr);
  return true;
}

template <typename String1, typename String2>
bool starts_with(const String1 &value, const String2 &sst) {
  if (is_empty(sst)) return false;

  uint32_t idx;
  if (index(value, sst, &idx)) {
    return idx == 0;
  }
  return false;
}

}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_STRING_CONTAINS_H_
