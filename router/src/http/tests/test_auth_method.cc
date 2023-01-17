/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#include <gtest/gtest.h>

struct HttpAuthMethodBasicParams {
  std::string test_name;
  std::string input;
  std::error_code ec;
  std::string username;
  std::string password;
};

class HttpAuthMethodBasicTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<HttpAuthMethodBasicParams> {};

TEST_P(HttpAuthMethodBasicTest, ensure) {
  HttpAuthMethodBasic m;

  std::error_code ec;
  auto auth_data = m.decode_authorization(GetParam().input, ec);
  EXPECT_EQ(ec, GetParam().ec) << ec.message();
  if (!ec) {
    EXPECT_EQ(auth_data.username, GetParam().username);
    EXPECT_EQ(auth_data.password, GetParam().password);

    // as decoding worked, the encoding should work too and result in the same
    // value
    //
    EXPECT_EQ(HttpAuthMethodBasic::encode_authorization(auth_data),
              GetParam().input);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Foo, HttpAuthMethodBasicTest,
    ::testing::Values(
        //
        HttpAuthMethodBasicParams{"valid",
                                  "QWxhZGRpbjpvcGVuIHNlc2FtZQ==",
                                  {},
                                  "Aladdin",
                                  "open sesame"},
        HttpAuthMethodBasicParams{
            "empty username, empty password", "Og==", {}, "", ""},
        HttpAuthMethodBasicParams{"empty password", "Zm9vOg==", {}, "foo", ""},
        HttpAuthMethodBasicParams{"empty username", "OmZvbw==", {}, "", "foo"},
        HttpAuthMethodBasicParams{
            "empty", "", std::make_error_code(std::errc::invalid_argument), "",
            ""},
        HttpAuthMethodBasicParams{
            "base64 broken", "=",
            std::make_error_code(std::errc::invalid_argument), "", ""}  //
        ));

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
