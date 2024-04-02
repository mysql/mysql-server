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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_TOKEN_JWT_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_TOKEN_JWT_H_

#include <string>
#include <vector>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>

#include "helper/token/jwt_holder.h"

namespace helper {

class Jwt {
 public:
  using Value = rapidjson::Value;
  using Document = rapidjson::Document;

 public:
  Jwt() {}

  static void parse(const std::string &token, JwtHolder *out);
  static Jwt create(const JwtHolder &holder);
  static Jwt create(const std::string &algoritym, Document &payload);

  bool is_valid() const;
  bool verify(const std::string &secret) const;
  std::string sign(const std::string &secret) const;

  std::string get_header_claim_algorithm() const;
  std::string get_header_claim_type() const;

  std::vector<std::string> get_payload_claim_names() const;
  const Value *get_payload_claim_custom(const std::string &name) const;

  std::string get_token() const {
    auto result = holder_.parts[0] + "." + holder_.parts[1];
    if (!holder_.parts[2].empty()) result += "." + holder_.parts[2];
    return result;
  }

 private:
  static std::vector<std::string> get_payload_names(const Value &v);

  bool valid_{false};
  JwtHolder holder_;
  Document header_;
  Document payload_;
  std::string signature_;
};

}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_TOKEN_JWT_H_
