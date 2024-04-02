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

#include "mrs/http/cookie.h"

#include <string_view>

#include "helper/container/map.h"

#include "mysql/harness/string_utils.h"

namespace mrs {
namespace http {

const char *Cookie::kHttpParameterNameCookie = "Cookie";

const std::string &to_string(Cookie::SameSite ss) {
  static const std::map<Cookie::SameSite, std::string> map{
      {Cookie::None, "None"}, {Cookie::Strict, "Strict"}, {Cookie::Lex, "Lex"}};
  return map.at(ss);
}

template <typename Callback>
static void enum_key_values(const std::string &value, Callback cb) {
  auto cookies = mysql_harness::split_string(value, ';', true);
  for (auto &c : cookies) {
    mysql_harness::left_trim(c);
    std::string_view key{c.c_str(), c.length()};
    std::string_view v{c.c_str(), c.length()};
    key.remove_suffix(key.size() - std::min(key.find('='), key.size()));
    v.remove_prefix(std::min(v.find("=") + 1, v.size()));

    // TODO(lkotula): Unescape the value (Shouldn't be in review)
    if (!cb(key, v)) break;
  }
}

Cookie::Cookie(Request *request) : request_{request} {
  if (nullptr == request_) return;

  auto value_c =
      request_->get_input_headers().find_cstr(kHttpParameterNameCookie);
  std::string value = value_c ? value_c : "";
  enum_key_values(value, [this](const auto &key, const auto &value) {
    cookies_[std::string{key}] = value;
    return true;
  });
}

void Cookie::clear(const char *cookie_name) {
  clear(request_, cookie_name);
  cookies_.erase(cookie_name);
}

void Cookie::clear(Request *request, const char *cookie_name) {
  std::string cookie = cookie_name;
  cookie += "=; Max-Age=0";
  request->get_output_headers().add("Set-Cookie", cookie.c_str());
}

std::string Cookie::get(const std::string &key) {
  return helper::container::get_value_default(cookies_, key, {});
}

// std::string Cookie::get(HttpRequest *request, const char *cookie_name) {
//  auto value = request->get_input_headers().get(kHttpParameterNameCookie);
//
//  if (value == nullptr) return {};
//
//  std::string result;
//  enum_key_values(value,
//                  [&result, cookie_name](auto &key, std::string_view value) {
//                    if (key == cookie_name) {
//                      result = value;
//                      return false;
//                    }
//                    return true;
//                  });
//
//  return result;
//}

void Cookie::set(Request *request, const std::string &cookie_name,
                 const std::string &value, const duration duration,
                 const std::string &path, const SameSite *same_site,
                 bool secure, bool http_only, const std::string &domain) {
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

  if (same_site) {
    cookie += "; SameSite=" + to_string(*same_site);
  }

  if (secure) {
    cookie += "; Secure";
  }

  if (http_only) {
    cookie += "; HttpOnly";
  }

  if (!domain.empty()) {
    cookie += "; Domain=" + domain;
  }
  request->get_output_headers().add("Set-Cookie", cookie.c_str());
}

void Cookie::set(const std::string &cookie_name, const std::string &value,
                 const duration duration, const std::string &path,
                 const SameSite *same_site, bool secure, bool http_only,
                 const std::string &domain) {
  set(request_, cookie_name, value, duration, path, same_site, secure,
      http_only, domain);
  cookies_[cookie_name] = value;
}

std::map<std::string, std::string> &Cookie::direct() { return cookies_; }

}  // namespace http
}  // namespace mrs
