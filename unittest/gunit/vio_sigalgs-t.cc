/* Copyright (c) 2026, Amazon.com, Inc. or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file unittest/gunit/vio_sigalgs-t.cc

  Unit tests for vio/vio_sigalgs.h — signature-algorithm list
  filtering and floor predicate.
*/

#include <gtest/gtest.h>

#include <openssl/opensslv.h>

#include <string>
#include <vector>

#include "vio/vio_sigalgs.h"

namespace vio_sigalgs_unittest {

/* Accept-all predicate (fast-path equivalent). */
auto accept_all = [](const std::string &) { return true; };

/* Reject nothing except rsa_pss_pss_* and *+SHA224 tokens. */
auto reject_pss_pss_and_sha224 = [](const std::string &tok) {
  if (tok.substr(0, 12) == "rsa_pss_pss_") return false;
  if (tok.size() >= 6 && tok.substr(tok.size() - 6) == "SHA224") return false;
  return true;
};

/* Reject every MTI spelling. */
auto reject_mti = [](const std::string &tok) {
  return tok != "rsa_pss_rsae_sha256" && tok != "ecdsa_secp256r1_sha256" &&
         tok != "ECDSA+SHA256" && tok != "rsa_pkcs1_sha256" &&
         tok != "RSA+SHA256";
};

/* Reject everything. */
auto reject_all = [](const std::string &) { return false; };

/* ---- 1. AllAccepted_NoOp ---- */

TEST(VioSigalgs, AllAccepted_NoOp) {
  const std::string input =
      "ECDSA+SHA256:rsa_pss_rsae_sha256:RSA+SHA256:ed25519";
  std::vector<std::string> dropped;
  std::string result = vio_sigalgs::filter(input, accept_all, &dropped);

  EXPECT_EQ(result, input);
  EXPECT_TRUE(dropped.empty());
  EXPECT_TRUE(vio_sigalgs::meets_floor(result));
}

/* ---- 2. SomeDropped_FloorMet ---- */

TEST(VioSigalgs, SomeDropped_FloorMet) {
  const std::string input =
      "ECDSA+SHA256:ECDSA+SHA384:ECDSA+SHA512:"
      "rsa_pss_pss_sha256:rsa_pss_pss_sha384:rsa_pss_pss_sha512:"
      "rsa_pss_rsae_sha256:rsa_pss_rsae_sha384:rsa_pss_rsae_sha512:"
      "RSA+SHA256:RSA+SHA384:RSA+SHA512:"
      "ed25519:ECDSA+SHA224:RSA+SHA224";
  std::vector<std::string> dropped;
  std::string result =
      vio_sigalgs::filter(input, reject_pss_pss_and_sha224, &dropped);

  /* Survivors are in original order, minus the rejected tokens. */
  EXPECT_EQ(result,
            "ECDSA+SHA256:ECDSA+SHA384:ECDSA+SHA512:"
            "rsa_pss_rsae_sha256:rsa_pss_rsae_sha384:rsa_pss_rsae_sha512:"
            "RSA+SHA256:RSA+SHA384:RSA+SHA512:"
            "ed25519");

  /* Exactly the rejected tokens. */
  ASSERT_EQ(dropped.size(), 5u);
  EXPECT_EQ(dropped[0], "rsa_pss_pss_sha256");
  EXPECT_EQ(dropped[1], "rsa_pss_pss_sha384");
  EXPECT_EQ(dropped[2], "rsa_pss_pss_sha512");
  EXPECT_EQ(dropped[3], "ECDSA+SHA224");
  EXPECT_EQ(dropped[4], "RSA+SHA224");

  EXPECT_TRUE(vio_sigalgs::meets_floor(result));
}

/* ---- 2b. OnlyOneMtiSurvives_FloorMet ---- */

TEST(VioSigalgs, OnlyOneMtiSurvives_FloorMet) {
  /* The floor is any-of: one surviving MTI algorithm is enough. */
  const std::string input = "ECDSA+SHA256:rsa_pss_rsae_sha256:ed25519";
  auto reject_ecdsa_sha256 = [](const std::string &tok) {
    return tok != "ECDSA+SHA256";
  };
  auto reject_rsae_sha256 = [](const std::string &tok) {
    return tok != "rsa_pss_rsae_sha256";
  };

  std::vector<std::string> dropped;
  std::string result =
      vio_sigalgs::filter(input, reject_ecdsa_sha256, &dropped);
  EXPECT_EQ(result, "rsa_pss_rsae_sha256:ed25519");
  EXPECT_EQ(result.find("ECDSA+SHA256"), std::string::npos);
  ASSERT_EQ(dropped.size(), 1u);
  EXPECT_EQ(dropped[0], "ECDSA+SHA256");
  EXPECT_TRUE(vio_sigalgs::meets_floor(result));

  dropped.clear();
  result = vio_sigalgs::filter(input, reject_rsae_sha256, &dropped);
  EXPECT_EQ(result, "ECDSA+SHA256:ed25519");
  EXPECT_EQ(result.find("rsa_pss_rsae_sha256"), std::string::npos);
  ASSERT_EQ(dropped.size(), 1u);
  EXPECT_EQ(dropped[0], "rsa_pss_rsae_sha256");
  EXPECT_TRUE(vio_sigalgs::meets_floor(result));
}

/* ---- 3. FloorViolated ---- */

TEST(VioSigalgs, FloorViolated) {
  /* Keep only ed25519 and ECDSA+SHA384 (non-MTI algorithms). */
  const std::string input =
      "ECDSA+SHA256:rsa_pss_rsae_sha256:RSA+SHA256:ed25519:ECDSA+SHA384";
  std::vector<std::string> dropped;
  std::string result = vio_sigalgs::filter(input, reject_mti, &dropped);

  /* Survivors exist but do not meet the MTI floor. */
  EXPECT_EQ(result, "ed25519:ECDSA+SHA384");
  EXPECT_FALSE(result.empty());
  EXPECT_FALSE(vio_sigalgs::meets_floor(result));
}

/* ---- 4. EmptySurvivors ---- */

TEST(VioSigalgs, EmptySurvivors) {
  const std::string input = "rsa_pss_rsae_sha256:ECDSA+SHA256:RSA+SHA256";
  std::vector<std::string> dropped;
  std::string result = vio_sigalgs::filter(input, reject_all, &dropped);

  EXPECT_TRUE(result.empty());
  EXPECT_FALSE(vio_sigalgs::meets_floor(result));
  EXPECT_EQ(dropped.size(), 3u);
}

/* ---- 5. FloorSpellings ---- */

TEST(VioSigalgs, FloorSpellings) {
  EXPECT_TRUE(vio_sigalgs::meets_floor("ECDSA+SHA256"));
  EXPECT_TRUE(vio_sigalgs::meets_floor("ecdsa_secp256r1_sha256"));
  EXPECT_TRUE(vio_sigalgs::meets_floor("rsa_pss_rsae_sha256"));
  /*
    RSASSA-PKCS1-v1_5 is not valid for the TLS 1.3 CertificateVerify
    message, so it does not satisfy the floor on its own even though it
    is a legal certificate signature algorithm (RFC 8446 4.4.3 / 9.1).
  */
  EXPECT_FALSE(vio_sigalgs::meets_floor("rsa_pkcs1_sha256"));
  EXPECT_FALSE(vio_sigalgs::meets_floor("RSA+SHA256:rsa_pkcs1_sha384"));
  EXPECT_TRUE(vio_sigalgs::meets_floor("RSA+SHA256:rsa_pss_rsae_sha256"));
  /* RSA+SHA384 alone does not meet the floor. */
  EXPECT_FALSE(vio_sigalgs::meets_floor("RSA+SHA384"));
  EXPECT_FALSE(vio_sigalgs::meets_floor(""));
}

/* ---- 6. Floor applies only to TLS 1.3-capable server contexts ---- */

TEST(VioSigalgs, FloorRequiredByProtocol) {
  /* Default --tls-version (TLSv1.2,TLSv1.3) parses to flags 0. */
  EXPECT_TRUE(vio_sigalgs::floor_required(false, 0));
  /* --tls-version=TLSv1.2: no TLS 1.3 CertificateVerify, floor skipped. */
  EXPECT_FALSE(vio_sigalgs::floor_required(false, SSL_OP_NO_TLSv1_3));
  /* --tls-version=TLSv1.3. */
  EXPECT_TRUE(vio_sigalgs::floor_required(false, SSL_OP_NO_TLSv1_2));
  /* The client connector is never floored. */
  EXPECT_FALSE(vio_sigalgs::floor_required(true, 0));
}

/* ---- 7. Real constants meet floor ---- */

TEST(VioSigalgs, FipsListMeetsFloor) {
  const char *list = vio_sigalgs::shipped_list_fips();
  ASSERT_NE(nullptr, list) << "FIPS sigalgs list compiled out";
  EXPECT_TRUE(vio_sigalgs::meets_floor(list));
}

TEST(VioSigalgs, NonFipsListMeetsFloor) {
  const char *list = vio_sigalgs::shipped_list_non_fips();
  ASSERT_NE(nullptr, list) << "non-FIPS sigalgs list compiled out";
  EXPECT_TRUE(vio_sigalgs::meets_floor(list));
}

#if OPENSSL_VERSION_NUMBER >= 0x30500000L
TEST(VioSigalgs, PqcListMeetsFloor) {
  const char *list = vio_sigalgs::shipped_list_non_fips_pqc();
  ASSERT_NE(nullptr, list) << "PQC sigalgs list compiled out";
  EXPECT_TRUE(vio_sigalgs::meets_floor(list));
}
#endif

}  // namespace vio_sigalgs_unittest
