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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_STRING_CONTAINS_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_STRING_CONTAINS_H_

#include <cstdint>
#include <string>

#include "helper/string/generic.h"

namespace helper {

template <typename String1, typename String2>
bool contains(const String1 &value, const String2 &sst) {
  using namespace helper::string;
  return nullptr != strstr(cstr(value), cstr(sst));
}

template <typename String1, typename String2>
bool icontains(const String1 &value, const String2 &sst) {
  auto sv = helper::string::size(value);
  auto ss = helper::string::size(sst);

  if (ss > sv) return false;

  auto serach_break_at = sv - ss;

  size_t found_chars = 0;
  for (size_t i = 0; i < sv && found_chars != ss; ++i) {
    if (!found_chars && (i > serach_break_at)) break;

    if (tolower(value[i]) == tolower(sst[found_chars])) {
      ++found_chars;
      continue;
    }

    found_chars = 0;
  }

  return found_chars == ss;
}

inline bool ends_with(const std::string &value, const std::string &sst) {
  if (sst.empty()) return false;

  auto pos = value.rfind(sst);
  if (value.npos == pos) return false;
  return value.length() - pos == sst.length();
}

template <typename String>
bool index(const std::string &value, const String &search_for, uint32_t *idx) {
  auto pos = value.find(search_for);

  if (value.npos == pos) return false;
  if (idx) *idx = static_cast<uint32_t>(pos);

  return true;
}

inline bool index(const char *value, const char *search_for, uint32_t *idx) {
  auto ptr = strstr(value, search_for);
  if (nullptr == ptr) return false;
  if (idx) *idx = static_cast<uint32_t>(std::distance(value, ptr));
  return true;
}

template <typename String1, typename String2>
bool starts_with(const String1 &value, const String2 &search_for) {
  using namespace helper::string;
  if (is_empty(search_for)) return false;

  uint32_t idx;
  if (index(value, search_for, &idx)) {
    return idx == 0;
  }
  return false;
}

}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_STRING_CONTAINS_H_
