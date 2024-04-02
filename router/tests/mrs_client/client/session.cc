/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

const static std::string k_lineStartHeader{"header "};
const static std::string k_lineStartCookie{"cookie "};

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

  if (!all_values.empty()) h->add("Cookie", all_values.c_str());
}

void HttpClientSession::analyze_response_headers(const HttpHeaders *h) {
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
  std::ofstream of(session_file_);
  if (!of.is_open()) {
    throw std::runtime_error("Can't write to session file.");
  }

  for (const auto &kv : cookies_) {
    of << k_lineStartCookie << kv.first << ":" << kv.second << std::endl;
  }

  for (const auto &kv : headers_) {
    of << k_lineStartHeader << kv.first << ":" << kv.second << std::endl;
  }
}

void HttpClientSession::session_load() {
  if (session_file_.empty()) return;

  std::ifstream s(session_file_);

  if (!s.is_open()) return;

  for (std::string line; std::getline(s, line);) {
    if (helper::starts_with(line, k_lineStartHeader))
      add_header(&line[0] + k_lineStartHeader.length());
    else if (helper::starts_with(line, k_lineStartCookie))
      add_cookie(&line[0] + k_lineStartHeader.length());
  }
}

static bool find_character(char *&c, char search_for) {
  char *it = c;
  while (*it) {
    if (*it == search_for) {
      c = it;
      return true;
    }
    ++it;
  }
  return false;
}

void HttpClientSession::add_header(char *header_entry) {
  auto entry = header_entry;
  if (!find_character(entry, ':')) {
    return;
  }
  *entry = 0;
  headers_[header_entry] = entry + 1;
}

void HttpClientSession::add_cookie(char *cookie_entry) {
  auto entry = cookie_entry;
  if (!find_character(entry, ':')) {
    return;
  }
  *entry = 0;
  cookies_[cookie_entry] = entry + 1;
}

}  // namespace mrs_client
