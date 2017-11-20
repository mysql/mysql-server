/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <gtest/gtest.h>

#include "native_plain_verification.h"
#include "native_verification.h"
#include "sha1.h"  // for SHA1_HASH_SIZE
#include "sha256_plain_verification.h"
#include "sha2_plain_verification.h"

namespace xpl {

namespace test {

namespace {
const char *const EMPTY = "";
const char *const EXPECTED_NATIVE_HASH =
    "*BF201911C951DCC0264E2C7577977E0A3EF06695";
const char *const EXPECTED_SHA256_HASH =
    "$5$1S> j#F2}Vz3yqu`fC8X$HrURSrHutEhr6orwomWpNiRquOS/xy9DzQFj5TuVHn0";
const char *const WRONG_PASSWD = "ALA_MA_KACA";
const char *const GOOD_PASSWD = "ALA_MA_KOTA";
}  // namespace

TEST(native_plain_verification, get_salt) {
  ASSERT_STREQ(EMPTY, Native_plain_verification().get_salt().c_str());
}

TEST(native_plain_verification, verification_pass) {
  ASSERT_TRUE(Native_plain_verification().verify_authentication_string(
      GOOD_PASSWD, EXPECTED_NATIVE_HASH));
}

TEST(native_plain_verification, verification_fail) {
  ASSERT_FALSE(Native_plain_verification().verify_authentication_string(
      WRONG_PASSWD, EXPECTED_NATIVE_HASH));
}

TEST(native_verification, get_salt) {
  ASSERT_STRNE(EMPTY, Native_verification().get_salt().c_str());
}

std::string get_hash(const std::string &salt, const std::string &user_string) {
  char scrambled[SCRAMBLE_LENGTH + 1] = {0};
  char hash[2 * SHA1_HASH_SIZE + 2] = {0};
  ::scramble(scrambled, salt.c_str(), user_string.c_str());
  ::make_password_from_salt(hash, (const uint8 *)scrambled);
  return hash;
}

TEST(native_verification, verification_pass) {
  Native_verification ver;
  ASSERT_TRUE(ver.verify_authentication_string(
      get_hash(ver.get_salt(), GOOD_PASSWD), EXPECTED_NATIVE_HASH));
}

TEST(native_verification, verification_fail) {
  Native_verification ver;
  ASSERT_FALSE(ver.verify_authentication_string(
      get_hash(ver.get_salt(), WRONG_PASSWD), EXPECTED_NATIVE_HASH));
}

TEST(sha256_plain_verification, get_salt) {
  ASSERT_STREQ(EMPTY, Sha256_plain_verification().get_salt().c_str());
}

TEST(sha256_plain_verification, verification_pass) {
  ASSERT_TRUE(Sha256_plain_verification().verify_authentication_string(
      GOOD_PASSWD, EXPECTED_SHA256_HASH));
}

TEST(sha256_plain_verification, verification_fail) {
  ASSERT_FALSE(Sha256_plain_verification().verify_authentication_string(
      WRONG_PASSWD, EXPECTED_SHA256_HASH));
}

}  // namespace test
}  // namespace xpl
