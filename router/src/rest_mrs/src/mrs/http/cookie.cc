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

#include "mrs/http/cookie.h"

#include <string_view>

#include "mysql/harness/string_utils.h"

namespace mrs {
namespace http {

const char *Cookie::kHttpParameterNameCookie = "Cookie";

void Cookie::clear(HttpRequest *request, const char *cookie_name) {
  std::string cookie = cookie_name;
  cookie += "=; Max-Age=0";
  request->get_output_headers().add("Set-Cookie", cookie.c_str());
}

std::string Cookie::get(HttpRequest *request, const char *cookie_name) {
  auto value = request->get_input_headers().get(kHttpParameterNameCookie);

  if (value == nullptr) return {};

  auto cookies = mysql_harness::split_string(value, ';', true);
  for (auto &c : cookies) {
    mysql_harness::left_trim(c);
    std::string_view key{c.c_str(), c.length()};
    key.remove_suffix(key.size() - std::min(key.find('='), key.size()));

    // TODO(lkotula): Unescape the value (Shouldn't be in review)
    if (key == cookie_name) {
      return {c.begin() + key.size() + 1, c.end()};
    }
  }

  return {};
}

void Cookie::set(HttpRequest *request, const std::string &cookie_name,
                 const std::string &value, const duration duration,
                 const std::string &path) {
  auto cookie = cookie_name + "=" + value;
  if (duration.count()) {
    using std::chrono::seconds;
    auto age =
        std::to_string(std::chrono::duration_cast<seconds>(duration).count());
    cookie += "; Max-Age=" + age;
  }
  if (!path.empty()) {
    cookie += "; Path=" + path;
  }
  request->get_output_headers().add("Set-Cookie", cookie.c_str());
}

}  // namespace http
}  // namespace mrs
