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

#include "../src/kdf_pbkdf2.h"

#include <tuple>

#include <gtest/gtest.h>

class Pbkdf2SaltTest : public ::testing::Test {};

TEST_F(Pbkdf2SaltTest, size) {
  auto salt = Pbkdf2::salt();

  EXPECT_EQ(salt.size(), 16);
}

class Pbkdf2Test
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<std::string,  // MCF
                                                                    // string
                                                      int,          // rounds
                                                      std::string,  // salt
                                                      std::string,  // checksum
                                                      const char *  // password
                                                      >> {};

TEST_P(Pbkdf2Test, decode) {
  auto hash_info = Pbkdf2McfAdaptor::from_mcf(std::get<0>(GetParam()));
  EXPECT_EQ(hash_info.rounds(), std::get<1>(GetParam()));
  EXPECT_EQ(hash_info.salt(),
            Pbkdf2McfAdaptor::base64_decode(std::get<2>(GetParam())));
  EXPECT_EQ(hash_info.checksum(),
            Pbkdf2McfAdaptor::base64_decode(std::get<3>(GetParam())));
}

TEST_P(Pbkdf2Test, verify) {
  auto hash_info = Pbkdf2McfAdaptor::from_mcf(std::get<0>(GetParam()));
  if (nullptr != std::get<4>(GetParam())) {
    EXPECT_EQ(hash_info.checksum(),
              Pbkdf2::derive(hash_info.digest(), hash_info.rounds(),
                             hash_info.salt(), std::get<4>(GetParam())));
  }
}

INSTANTIATE_TEST_SUITE_P(
    Foo, Pbkdf2Test,
    ::testing::Values(

        std::make_tuple(  //
            "$pbkdf2-sha256$6400$0ZrzXitFSGltTQnBWOsdAw$"
            "Y11AchqV4b0sUisdZd0Xr97KWoymNE0LNNrnEgY4H9M",
            6400,                                           // rounds
            "0ZrzXitFSGltTQnBWOsdAw",                       // salt
            "Y11AchqV4b0sUisdZd0Xr97KWoymNE0LNNrnEgY4H9M",  // checksum
            nullptr)
        // empty checksum signals the 'verify' not skip the verification

        // end
        ));

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
