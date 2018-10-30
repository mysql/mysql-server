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

#include "http_auth.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

class HttpAuthTest : public ::testing::Test {};

TEST_F(HttpAuthTest, quoted_string) {
  EXPECT_EQ(HttpQuotedString::quote("abc"), "\"abc\"");
  EXPECT_EQ(HttpQuotedString::quote("a\"bc"), "\"a\\\"bc\"");
}

class CredentialsTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<std::string,  // hdr
                     std::string,  // scheme
                     std::string,  // token
                     std::vector<std::pair<std::string, std::string>>>> {};

TEST_P(CredentialsTest, from_header) {
  std::error_code ec;
  auto creds = HttpAuthCredentials::from_header(std::get<0>(GetParam()), ec);

  ASSERT_FALSE(ec);

  EXPECT_EQ(creds.scheme(), std::get<1>(GetParam()));
  EXPECT_EQ(creds.token(), std::get<2>(GetParam()));
  EXPECT_EQ(creds.params(), std::get<3>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(
    Foo, CredentialsTest,
    ::testing::Values(
        // Basic Auth
        std::make_tuple("Basic dGVzdDoxMjPCow==",  // input
                        "Basic",                   // scheme
                        "dGVzdDoxMjPCow==",        // token
                        std::vector<std::pair<std::string, std::string>>{}),
        // No params
        std::make_tuple("Basic",  // input
                        "Basic",  // scheme
                        "",       // token
                        std::vector<std::pair<std::string, std::string>>{})));

class CredentialsFailTest : public ::testing::Test,
                            public ::testing::WithParamInterface<std::string> {
};

TEST_P(CredentialsFailTest, from_header) {
  std::error_code ec;
  auto creds = HttpAuthCredentials::from_header(GetParam(), ec);

  ASSERT_TRUE(ec);
}

INSTANTIATE_TEST_CASE_P(Foo, CredentialsFailTest,
                        ::testing::Values(
                            // empty hdr
                            "",
                            // not a tchar
                            "\""));

class ChallengeTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<std::string,  // hdr
                     std::string,  // scheme
                     std::string,  // token
                     std::vector<std::pair<std::string, std::string>>>> {};

TEST_P(ChallengeTest, to_string) {
  HttpAuthChallenge challenge(std::get<1>(GetParam()), std::get<2>(GetParam()),
                              std::get<3>(GetParam()));
  EXPECT_EQ(challenge.str(), std::get<0>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(
    Foo, ChallengeTest,
    ::testing::Values(
        // Digest auth
        std::make_tuple("Basic realm=\"foo\",charset=\"UTF-8\"",  // input
                        "Basic",                                  // scheme
                        "",                                       // token
                        std::vector<std::pair<std::string, std::string>>{
                            {"realm", "foo"}, {"charset", "UTF-8"}}),
        std::make_tuple("Basic",  // input
                        "Basic",  // scheme
                        "",       // token
                        std::vector<std::pair<std::string, std::string>>{})));

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
