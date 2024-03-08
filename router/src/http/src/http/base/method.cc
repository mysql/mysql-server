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

#include "http/base/method.h"

#include <map>
#include <string_view>

namespace http {
namespace base {
namespace method {

key_type from_string(const std::string_view &method) {
  using namespace std::string_view_literals;

  static const std::map<std::string_view, key_type> all_methods{
      {"GET"sv, Get},     {"POST"sv, Post},       {"HEAD"sv, Head},
      {"PUT"sv, Put},     {"DELETE"sv, Delete},   {"OPTIONS"sv, Options},
      {"TRACE"sv, Trace}, {"CONNECT"sv, Connect}, {"PATCH"sv, Patch}};

  auto result = all_methods.find(method);

  if (result == all_methods.end()) return Unknown;

  return result->second;
}

pos_type from_string_to_post(const std::string_view &method) {
  using namespace std::string_view_literals;

  static const std::map<std::string_view, key_type> all_methods{
      {"GET"sv, Pos::Get},       {"POST"sv, Pos::Post},
      {"HEAD"sv, Pos::Head},     {"PUT"sv, Pos::Put},
      {"DELETE"sv, Pos::Delete}, {"OPTIONS"sv, Pos::Options},
      {"TRACE"sv, Pos::Trace},   {"CONNECT"sv, Pos::Connect},
      {"PATCH"sv, Pos::Patch}};

  auto result = all_methods.find(method);

  if (result == all_methods.end()) return Unknown;

  return result->second;
}

}  // namespace method
}  // namespace base
}  // namespace http
