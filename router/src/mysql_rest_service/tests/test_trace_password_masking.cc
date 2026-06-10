/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

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

#include "mrs/rest/rest_request_handler.h"

struct MaskTestCase {
  std::string description;
  std::string input;
  std::string expected;
};

std::vector<MaskTestCase> get_test_cases() {
  return {
      {
          "Mask password",               //
          R"("password": "secret123")",  //
          R"("password": "*****")",      //
      },
      {
          "Mask accessToken",            //
          R"("accessToken": "xyz789")",  //
          R"("accessToken": "*****")",   //
      },
      {
          "Mask accessToken with additional fields",   //
          R"("foo": "bar", "accessToken": "xyz789")",  //
          R"("foo": "bar", "accessToken": "*****")",   //
      },
      {
          "Mask password with additional fields",           //
          R"("password": "secret123", "auth": "my_auth")",  //
          R"("password": "*****", "auth": "my_auth")",      //
      },
      {
          "Mask both password and accessToken",              //
          R"("password": "abc", "accessToken": "xyz")",      //
          R"("password": "*****", "accessToken": "*****")",  //
      },
      {
          "Multiple password and accessToken",                              //
          R"("password": "abc", "accessToken": "def", "password": "ghi")",  //
          R"("password": "*****", "accessToken": "*****", "password": "*****")",  //
      },
      {
          "Case-sensitive",                                  //
          R"("Password": "abc", "AccessToken": "xyz")",      //
          R"("password": "*****", "accessToken": "*****")",  //
      },
      {
          "Additional whitespaces",        //
          R"("accessToken" : "abc xyz")",  //
          R"("accessToken": "*****")",     //
      },
      {
          "Escaped quotes in accessToken",   //
          R"("accessToken": "a\"bc\"def")",  //
          R"("accessToken": "*****")",       //
      },
      {
          "Escaped backslash in password",  //
          R"("password": "abc\\def")",      //
          R"("password": "*****")",         //
      },
      {
          "Escaped special characters",      //
          R"("accessToken": "a\nbc\tdef")",  //
          R"("accessToken": "*****")",       //
      },
      {
          "Invalid JSON - unterminated string",  //
          R"("accessToken": "unterminated)",     //
          R"("accessToken": "unterminated)",     //
      },
      {
          "Invalid JSON - unescaped field",  //
          R"(accessToken: "abc")",           //
          R"(accessToken: "abc")",           //
      }};
}

class TracePasswordMaskingTest : public ::testing::TestWithParam<MaskTestCase> {
};

TEST_P(TracePasswordMaskingTest, PasswordMasking) {
  const auto &param = GetParam();
  std::string actual =
      mrs::rest::RestRequestHandler::mask_password(param.input);
  EXPECT_EQ(actual, param.expected) << "Test failed: " << param.description;
}

INSTANTIATE_TEST_SUITE_P(CommonCases, TracePasswordMaskingTest,
                         ::testing::ValuesIn(get_test_cases()));
