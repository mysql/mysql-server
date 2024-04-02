/*  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <map>

#include "helper/token/jwt.h"
#include "helper/token/jwt_holder.h"

TEST(Jwt, verify_with_valid_signature) {
  helper::JwtHolder holder;
  //  {
  //    "alg": "HS256",
  //    "typ": "JWT"
  //  }
  helper::Jwt::parse(
      "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
      "eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIy"
      "fQ.XbPfbIHMI6arZ3Y922BhjWgQzWXcXNrz0ogtVhfEd2o",
      &holder);
  auto jwt = helper::Jwt::create(holder);
  ASSERT_TRUE(jwt.is_valid());
  ASSERT_TRUE(jwt.verify("secret"));
}

TEST(Jwt, verify_with_tempered_payload) {
  helper::JwtHolder holder;
  helper::Jwt::parse(
      "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
      "e30"
      ".XbPfbIHMI6arZ3Y922BhjWgQzWXcXNrz0ogtVhfEd2o",
      &holder);
  auto jwt = helper::Jwt::create(holder);
  ASSERT_TRUE(jwt.is_valid());
  ASSERT_FALSE(jwt.verify("secret"));
}

TEST(Jwt, verivy_payload_marked_none) {
  helper::JwtHolder holder;
  //  {
  //    "alg": "none",
  //    "typ": "JWT"
  //  }
  helper::Jwt::parse("eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0.e30", &holder);
  auto jwt = helper::Jwt::create(holder);
  ASSERT_TRUE(jwt.is_valid());

  // The payload is valid, because alg is set to none.
  ASSERT_TRUE(jwt.verify("secret"));
}

TEST(Jwt, generate_token) {
  rapidjson::Document payload;
  payload.SetObject();
  auto jwt = helper::Jwt::create("none", payload);
  ASSERT_EQ(std::string("eyJ0eXAiOiJKV1QiLCJhbGciOiJub25lIn0.e30"),
            jwt.sign("secret"));
}
