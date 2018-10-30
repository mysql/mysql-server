/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "http_auth_method_basic.h"

#include <algorithm>  // std::find
#include <string>
#include <vector>

#include "../src/base64.h"

// instantiate the constexpr members
constexpr const char HttpAuthMethodBasic::kMethodName[];

HttpAuthMethodBasic::AuthData HttpAuthMethodBasic::decode_authorization(
    const std::string &http_auth_data, std::error_code &ec) {
  std::vector<uint8_t> decoded;
  try {
    decoded = Base64::decode(http_auth_data);
  } catch (const std::runtime_error &e) {
    ec = std::make_error_code(std::errc::invalid_argument);
    return {};
  }

  auto colon_it = std::find(decoded.begin(), decoded.end(), ':');

  if (colon_it == decoded.end()) {
    // no colon found

    ec = std::make_error_code(std::errc::invalid_argument);
    return {};
  }

  // std::string with a empty-range leads to  "vector::_M_range_check: __n
  // (which is 0) >= this->size() (which is 0)"
  return {
      decoded.begin() != colon_it ? std::string(decoded.begin(), colon_it) : "",
      colon_it + 1 != decoded.end() ? std::string(colon_it + 1, decoded.end())
                                    : ""};
}

std::string HttpAuthMethodBasic::encode_authorization(
    const HttpAuthMethodBasic::AuthData &auth_data) {
  // base64 expects vector<uint8_t>
  std::vector<uint8_t> decoded;
  decoded.reserve(auth_data.username.size() + 1 + auth_data.password.size());

  std::copy(auth_data.username.begin(), auth_data.username.end(),
            std::back_inserter(decoded));
  decoded.push_back(':');
  std::copy(auth_data.password.begin(), auth_data.password.end(),
            std::back_inserter(decoded));
  return Base64::encode(decoded);
}
