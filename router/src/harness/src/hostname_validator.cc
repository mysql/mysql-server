/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include "hostname_validator.h"

#include <regex>

#include "mysql/harness/net_ts/internet.h"

namespace mysql_harness {

bool is_valid_ip_address(const std::string &address) {
  auto make_res = net::ip::make_address(address);

  return make_res.has_value();
}

// RFC1123
//
// DIGIT := 0-9
// UPPER := A-Z
// LOWER := a-z
// DOT   := .
// ALPHA := UPPER | LOWER
// ALNUM := DIGIT | ALPHA
// LABEL := ALNUM | (ALNUM (ALNUM | -){0, 61} ALNUM)

bool is_valid_hostname(const std::string &address) {
  if (address.size() > 255) return false;

  // NAME := (LABEL .)* LABEL
  return std::regex_match(
      address,
      std::regex{
          "^"
          R"((([A-Za-z0-9]|[A-Za-z0-9][-A-Za-z0-9]{0,61}[A-Za-z0-9])\.)*)"
          R"(([A-Za-z0-9]|[A-Za-z0-9][-A-Za-z0-9]{0,61}[A-Za-z0-9]))"
          "$"});
}

// RFC2181 defines LABEL as:
// LABEL := octet{1,63} (except DOT)
bool is_valid_domainname(const std::string &address) {
  auto cur = address.begin();
  auto end = address.end();

  if (address.empty()) return false;
  // max domainname is 255 chars
  if (address.size() > 255) return false;

  // NAME := (LABEL .)* LABEL .?
  while (true) {
    auto dot_it = std::find(cur, end, '.');

    if (dot_it == end) break;

    auto dist = std::distance(cur, dot_it);

    // label too short
    if (dist == 0) return false;
    // label too long
    if (dist > 63) return false;

    cur = dot_it + 1;
  }

  auto dist = std::distance(cur, end);

  // label too long
  if (dist > 63) return false;

  return true;
}

}  // namespace mysql_harness
