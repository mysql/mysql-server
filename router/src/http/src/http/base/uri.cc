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

#include "http/base/uri.h"

#include <string.h>

#include <deque>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "mysql/harness/utility/string.h"

namespace http {
namespace base {

Uri::Uri() : uri_impl_{"", true, true, true, true} {}

Uri::Uri(const std::string &uri) : uri_impl_{uri, true, true, true, true} {}

Uri::Uri(Uri &&other) : uri_impl_{std::move(other.uri_impl_)} {}

Uri::Uri(const Uri &other) : uri_impl_{} { *this = other; }

Uri::~Uri() = default;

Uri::operator bool() const {
  return !uri_impl_.scheme.empty() || !uri_impl_.host.empty() ||
         0 != uri_impl_.port || !uri_impl_.username.empty() ||
         !uri_impl_.password.empty() || !uri_impl_.path.empty() ||
         !uri_impl_.query.empty() || !uri_impl_.fragment.empty();
}

Uri &Uri::operator=(const Uri &other) {
  uri_impl_ = other.uri_impl_;

  return *this;
}

Uri &Uri::operator=(Uri &&other) {
  uri_impl_ = std::move(other.uri_impl_);
  return *this;
}

std::string Uri::get_scheme() const { return uri_impl_.scheme; }

void Uri::set_scheme(const std::string &scheme) { uri_impl_.scheme = scheme; }

std::string Uri::get_userinfo() const {
  if (uri_impl_.username.empty() && uri_impl_.password.empty()) return {};

  if (uri_impl_.password.empty()) return uri_impl_.username;

  return uri_impl_.username + ":" + uri_impl_.password;
}

void Uri::set_userinfo(const std::string &userinfo) {
  auto pos = userinfo.find(":");

  if (pos == std::string::npos) {
    uri_impl_.username = userinfo;
    uri_impl_.password.clear();
    return;
  }

  uri_impl_.username = userinfo.substr(0, pos);
  uri_impl_.password = userinfo.substr(pos + 1);
}

std::string Uri::get_host() const {
  if (uri_impl_.host.find(":") == std::string::npos) return uri_impl_.host;

  std::string result;
  result.reserve(uri_impl_.host.length() + 2);
  result = "[" + uri_impl_.host + "]";

  return result;
}

void Uri::set_host(const std::string &host) {
  if (!host.empty()) {
    if (*host.begin() == '[' && *host.rbegin() == ']') {
      uri_impl_.host = host.substr(1, host.length() - 2);
      return;
    }
  }
  uri_impl_.host = host;
}

int32_t Uri::get_port() const {
  if (uri_impl_.port == 0) return -1;

  return uri_impl_.port;
}

void Uri::set_port(int32_t port) {
  if (port == -1) {
    uri_impl_.port = 0;
    return;
  }
  uri_impl_.port = port;
}

std::string Uri::join_path() const {
  auto u_p = get_path();
  auto u_q = get_query();
  auto &u_f = uri_impl_.fragment;

  if (u_p.empty()) {
    u_p = "/";
  }

  std::string result;
  result.reserve(u_p.length() + u_q.length() + 1 + u_f.length() + 1);

  if (!u_p.empty()) result += u_p;

  if (!u_q.empty()) {
    result += '?';
    result += u_q;
  }

  if (!u_f.empty()) {
    result += '#';
    result += u_f;
  }

  return result;
}

std::string Uri::get_path() const { return uri_impl_.get_path_as_string(true); }

void Uri::set_path(const std::string &path) {
  uri_impl_.set_path_from_string(path);
}

std::string Uri::get_fragment() const { return uri_impl_.fragment; }

void Uri::set_fragment(const std::string &fragment) {
  uri_impl_.fragment = fragment;
}

std::string Uri::get_query() const { return uri_impl_.get_query_as_string(); }

bool Uri::set_query(const std::string &query) {
  uri_impl_.set_query_from_string(query);
  return true;
}

std::string Uri::join() const {
  auto result = uri_impl_.str();

  if (result.empty()) return "/";

  return result;
}

std::string http_uri_path_canonicalize(const std::string &uri_path) {
  if (uri_path.empty()) return "/";

  std::deque<std::string> sections;

  std::istringstream ss(uri_path);
  for (std::string section; std::getline(ss, section, '/');) {
    if (section == "..") {
      // remove last item on the stack
      if (!sections.empty()) {
        sections.pop_back();
      }
    } else if (section != "." && !section.empty()) {
      sections.emplace_back(section);
    }
  }

  bool has_trailing_slash = uri_path.back() == '/';
  if (has_trailing_slash) sections.emplace_back("");

  auto out = "/" + mysql_harness::join(sections, "/");

  return out;
}

}  // namespace base
}  // namespace http
