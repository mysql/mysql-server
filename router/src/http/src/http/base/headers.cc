/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "http/base/headers.h"
#include "m_string.h"  // NOLINT(build/include_subdir)

namespace http {
namespace base {

bool compare_case_insensitive(const std::string &l, const std::string_view &r) {
  if (l.length() != r.length()) return false;
  return 0 == native_strncasecmp(l.c_str(), r.data(), l.length());
}

Headers::Headers() = default;

Headers::Headers(Headers &&other) : map_{std::move(other.map_)} {}

Headers::~Headers() {}

void Headers::add(const std::string_view &key, std::string &&value) {
  remove(key);
  map_.emplace_back(key, std::move(value));
}

Headers::Iterator Headers::begin() { return map_.begin(); }

Headers::Iterator Headers::end() { return map_.end(); }

Headers::CIterator Headers::begin() const { return map_.begin(); }

Headers::CIterator Headers::end() const { return map_.end(); }

const std::string *Headers::find(const std::string_view &key) const {
  for (auto &element : map_) {
    if (compare_case_insensitive(element.first, key)) return &element.second;
  }

  return nullptr;
}

const char *Headers::find_cstr(const char *key) const {
  std::string_view sv_key{key};
  for (const auto &element : map_) {
    if (compare_case_insensitive(element.first, sv_key)) {
      return element.second.c_str();
    }
  }

  return nullptr;
}

uint32_t Headers::size() const { return map_.size(); }

void Headers::clear() { return map_.clear(); }

void Headers::remove(const std::string_view &key) {
  auto it = std::find_if(map_.begin(), map_.end(),
                         [&key](const auto &element) -> bool {
                           return compare_case_insensitive(element.first, key);
                         });
  if (it != map_.end()) map_.erase(it);
}

}  // namespace base
}  // namespace http
