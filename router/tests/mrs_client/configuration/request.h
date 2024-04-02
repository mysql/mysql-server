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

#ifndef ROUTER_TESTS_HTTP_CLIENT_CONFIGURATION_REQUEST_H_
#define ROUTER_TESTS_HTTP_CLIENT_CONFIGURATION_REQUEST_H_

#include <string>
#include "mysql/harness/string_utils.h"

namespace http_client {

class Request {
 public:
  using Type = HttpMethod::key_type;
  const static std::map<std::string, Type> &get_map() {
    const static std::map<std::string, Type> map{
        {"get", HttpMethod::Get},
        {"post", HttpMethod::Post},
        {"put", HttpMethod::Put},
        {"delete", HttpMethod::Delete}};
    return map;
  }

  static bool convert(std::string value, Type *out_at = nullptr) {
    mysql_harness::lower(value);
    auto &map = get_map();
    auto it = map.find(value);

    if (map.end() == it) return false;
    if (out_at) *out_at = it->second;

    return true;
  }
};

}  // namespace http_client

#endif  // ROUTER_TESTS_HTTP_CLIENT_CONFIGURATION_REQUEST_H_
