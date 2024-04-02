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

#ifndef ROUTER_TESTS_HTTP_CLIENT_HTTPCLIENT_SESSION_H_
#define ROUTER_TESTS_HTTP_CLIENT_HTTPCLIENT_SESSION_H_

#include <map>
#include <string>

#include "http/base/headers.h"

namespace mrs_client {

class HttpClientSession {
 public:
  using HttpHeaders = http::base::Headers;

 public:
  HttpClientSession();
  HttpClientSession(const std::string &session_file);
  ~HttpClientSession();

  void fill_request_headers(HttpHeaders *headers) const;
  void analyze_response_headers(const HttpHeaders *headers);

  void add_header(char *entry);
  void add_cookie(char *entry);

 private:
  void session_load();
  void session_store();

  std::string session_file_;
  std::map<std::string, std::string> headers_;
  std::map<std::string, std::string> cookies_;
};

}  // namespace mrs_client

#endif  // ROUTER_TESTS_HTTP_CLIENT_HTTPCLIENT_SESSION_H_
