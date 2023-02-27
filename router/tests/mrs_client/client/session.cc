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

namespace {

bool starts_with(const std::string &str, const std::string &start) {
  if (str.length() < start.length()) return false;

  return 0 == strncmp(str.c_str(), start.c_str(), start.length());
}

}  // namespace

namespace mrs_client {

HttpClientSession::HttpClientSession() { session_load(); }

HttpClientSession::HttpClientSession(const std::string &session_file)
    : session_file_{session_file} {
  session_load();
}

HttpClientSession::~HttpClientSession() { session_store(); }

void HttpClientSession::fill_request_headers(HttpHeaders *) const {}

void HttpClientSession::analyze_response_headers(HttpHeaders *) {}

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
    if (starts_with(line, k_header))
      add_header(line.c_str() + k_header.length());
    else if (starts_with(line, k_cookie))
      add_cookie(line.c_str() + k_header.length());
  }
}

void HttpClientSession::add_header(const char *) {}

void HttpClientSession::add_cookie(const char *) {}

}  // namespace mrs_client
