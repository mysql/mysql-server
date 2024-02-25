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

#include "http_auth_backend.h"

#include <gtest/gtest.h>

#include "mcf_error.h"

struct HttpAuthBackendParam {
  const char *test_name;
  const char *mcf_line;
  std::error_code parse_ec;
  const char *username;
  const char *password;
  std::error_code auth_ec;
};

class HttpPasswdAuthBackendTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<HttpAuthBackendParam> {};

TEST_P(HttpPasswdAuthBackendTest, ensure) {
  HttpAuthBackendHtpasswd m;

  std::istringstream ss(GetParam().mcf_line);

  EXPECT_EQ(m.from_stream(ss), GetParam().parse_ec);

  EXPECT_EQ(m.authenticate(GetParam().username, GetParam().password),
            GetParam().auth_ec);
}

const char kMcfSha512_myName_test[]{
    "myName:"               // name
    "$6$3ieWD5TQkakPm.iT$"  // sha512 and salt
    "4HI5XzmE4UCSOsu14jujlXYNYk2SB6gi2yVoAncaOzynEnTI0Rc9."
    "78jHABgKm2DHr1LHc7Kg9kCVs9/uCOR7/"  // password: test
    "\n"};

INSTANTIATE_TEST_SUITE_P(
    Spec, HttpPasswdAuthBackendTest,
    ::testing::Values(
        HttpAuthBackendParam{
            "valid_user", kMcfSha512_myName_test, {}, "myName", "test", {}},
        HttpAuthBackendParam{"no_accounts",
                             "",
                             {},
                             "myName",
                             "test",
                             make_error_code(McfErrc::kUserNotFound)},
        HttpAuthBackendParam{"user_not_found",
                             kMcfSha512_myName_test,
                             {},
                             "someother",
                             "test",
                             make_error_code(McfErrc::kUserNotFound)},
        HttpAuthBackendParam{"wrong_password",
                             kMcfSha512_myName_test,
                             {},
                             "myName",
                             "wrongpassword",
                             make_error_code(McfErrc::kPasswordNotMatched)},
        HttpAuthBackendParam{"unknown_scheme",
                             "myName:$3$\n",
                             {},
                             "myName",
                             "wrongpassword",
                             make_error_code(McfErrc::kUnknownScheme)},
        HttpAuthBackendParam{"empty_mcf",
                             "",
                             {},
                             "myName",
                             "wrongpassword",
                             make_error_code(McfErrc::kUserNotFound)},
        HttpAuthBackendParam{
            "empty_username", ":$3$\n", make_error_code(McfErrc::kParseError),
            "myName", "wrongpassword", make_error_code(McfErrc::kUserNotFound)},
        HttpAuthBackendParam{
            "empty_password", "foo:\n", make_error_code(McfErrc::kParseError),
            "myName", "wrongpassword", make_error_code(McfErrc::kUserNotFound)},
        HttpAuthBackendParam{
            "empty_all", ":\n", make_error_code(McfErrc::kParseError), "myName",
            "wrongpassword", make_error_code(McfErrc::kUserNotFound)}),
    [](::testing::TestParamInfo<HttpAuthBackendParam> param_info) {
      return param_info.param.test_name;
    });

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
