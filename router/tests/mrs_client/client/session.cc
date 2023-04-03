/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "client/session.h"

#include <cstring>
#include <fstream>

#include "helper/string/contains.h"
#include "mysql/harness/string_utils.h"

namespace mrs_client {

HttpClientSession::HttpClientSession() { session_load(); }

HttpClientSession::HttpClientSession(const std::string &session_file)
    : session_file_{session_file} {
  session_load();
}

HttpClientSession::~HttpClientSession() { session_store(); }

void HttpClientSession::fill_request_headers(HttpHeaders *h) const {
  for (auto &item : headers_) {
    h->add(item.first.c_str(), item.second.c_str());
  }

  std::string all_values;
  for (auto &c : cookies_) {
    if (!all_values.empty()) all_values += "; ";
    all_values += c.first + "=" + c.second;
  }
  h->add("Cookie", all_values.c_str());
}

void HttpClientSession::analyze_response_headers(HttpHeaders *h) {
  for (const auto &v : *h) {
    if (v.first == "Set-Cookie") {
      auto cookie = mysql_harness::split_string(v.second, ';', false);
      if (cookie.size() == 0) continue;
      auto name_value = mysql_harness::split_string(cookie[0], '=', true);
      if (name_value.size() != 2) continue;
      cookies_[name_value[0]] = name_value[1];
    }
  }
}

void HttpClientSession::session_store() {
  if (session_file_.empty()) return;
}

void HttpClientSession::session_load() {
  const static std::string k_header{"header "};
  const static std::string k_cookie{"cookie "};

  if (session_file_.empty()) return;

  std::ifstream s(session_file_);

  if (!s.is_open())
    throw std::runtime_error("The session file, can't be opened.");

  for (std::string line; std::getline(s, line);) {
    if (helper::starts_with(line, k_header))
      add_header(line.c_str() + k_header.length());
    else if (helper::starts_with(line, k_cookie))
      add_cookie(line.c_str() + k_header.length());
  }
}

void HttpClientSession::add_header(const char *) {}

void HttpClientSession::add_cookie(const char *) {}

}  // namespace mrs_client
