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

#include "configuration/url.h"

#include <map>
#include <string>

#include "mysql/harness/string_utils.h"

namespace mrs_client {

Url::Url() {}

Url::Url(const std::string &url) : uri_{HttpUri::parse(url)} {
  // The map contains entries that define the TCP port and if its TLS
  // for i given schema.
  if (!uri_) {
    throw std::invalid_argument(
        "URL format is invalid, expected: [http[s]://][host[:port]/path");
  }
  auto scheme = mysql_harness::make_lower(uri_.get_scheme());
  static std::map<std::string, bool> schemas_uses_tls{{"http", false},
                                                      {"https", true}};

  auto it = schemas_uses_tls.find(scheme);
  if (!scheme.empty() && schemas_uses_tls.end() == it)
    throw std::invalid_argument("URL contains invalid scheme");

  needs_tls_ = schemas_uses_tls[scheme];
}

uint16_t Url::get_port() const {
  auto port = uri_.get_port();
  if (0xFFFF != port) return port;

  const uint16_t k_port_http = 80;
  const uint16_t k_port_https = 8080;

  if (needs_tls_) return k_port_https;

  return k_port_http;
}

bool Url::needs_tls() const { return needs_tls_; }

std::string Url::get_host() const { return uri_.get_host(); }

std::string Url::get_request() const {
  auto request = uri_.get_path();
  auto query = uri_.get_query();
  auto fragment = uri_.get_fragment();

  if (!query.empty()) {
    request += "?" + query;
  }

  if (!fragment.empty()) {
    request += "#" + fragment;
  }

  return request;
}

}  // namespace mrs_client
