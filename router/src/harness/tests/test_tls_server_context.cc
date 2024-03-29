/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "test/helpers.h"

#include "mysql/harness/tls_server_context.h"
#include "mysql/harness/tls_types.h"
#include "mysql/harness/utility/string.h"  // join()
#include "openssl_version.h"

using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Not;

class TlsServerContextTest : public ::testing::Test {
 public:
  TlsLibraryContext m_tls_lib_ctx;
};

// Check .cipher_list() returns mandatory ciphers with default ciphers
TEST_F(TlsServerContextTest, CiphersMandatory) {
  TlsServerContext ctx;

  // set default cipher_list
  std::string ciphers =
      mysql_harness::join(TlsServerContext::default_ciphers(), ":");
  ctx.cipher_list(ciphers);

  // Get the filtered cipher list
  mysql_harness::Ssl ssl{SSL_new(ctx.get())};
  int prio = 0;
  std::vector<std::string> r{};
  while (auto c = SSL_get_cipher_list(ssl.get(), prio++)) {
    r.emplace_back(c);
  }

  // Require at least one of the mandatory ciphers
  EXPECT_THAT(r, AnyOf(Contains("ECDHE-ECDSA-AES128-GCM-SHA256"),
                       Contains("ECDHE-ECDSA-AES256-GCM-SHA384"),
                       Contains("ECDHE-RSA-AES128-GCM-SHA256")));
}

class CiphersAcceptable : public TlsServerContextTest,
                          public ::testing::WithParamInterface<std::string> {};

// Check .cipher_list() returns acceptable ciphers if used
TEST_P(CiphersAcceptable, CiphersAcceptableParam) {
  std::string cipher = GetParam();

  TlsServerContext ctx;

  // set cipher_list to cipher
  ctx.cipher_list(cipher);

  // Get the filtered cipher list
  mysql_harness::Ssl ssl{SSL_new(ctx.get())};
  int prio = 0;
  std::vector<std::string> r{};
  while (auto c = SSL_get_cipher_list(ssl.get(), prio++)) {
    r.emplace_back(c);
  }

  EXPECT_THAT(r, Contains(cipher));
}

static const std::string acceptable_ciphers_test_data[] = {
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 1)
    // TLSv1.3
    {"TLS_AES_128_GCM_SHA256"},
    {"TLS_AES_256_GCM_SHA384"},
    {"TLS_CHACHA20_POLY1305_SHA256"},
#if 0  // embedded
    {"TLS_AES_128_CCM_SHA256"},
#endif
#endif
    // TLSv1.2
    {"ECDHE-RSA-AES256-GCM-SHA384"},
    {"DHE-RSA-AES128-GCM-SHA256"},
    {"DHE-RSA-AES256-GCM-SHA384"},
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
    {"ECDHE-ECDSA-CHACHA20-POLY1305"},
    {"ECDHE-RSA-CHACHA20-POLY1305"},
    {"DHE-RSA-CHACHA20-POLY1305"},
#endif
#if 0  // embedded
    {"ECDHE-ECDSA-AES256-CCM"},
    {"ECDHE-ECDSA-AES128-CCM"},
    {"DHE-RSA-AES256-CCM"},
    {"DHE-RSA-AES128-CCM"},
#endif
};

INSTANTIATE_TEST_SUITE_P(CiphersAcceptableParam, CiphersAcceptable,
                         ::testing::ValuesIn(acceptable_ciphers_test_data));

class CiphersDeprecated : public TlsServerContextTest,
                          public ::testing::WithParamInterface<std::string> {};

// Check .cipher_list() returns deprecated ciphers if used
TEST_P(CiphersDeprecated, CiphersDeprecatedParam) {
  std::string cipher = GetParam();

  TlsServerContext ctx;

  // set cipher_list to cipher
  ctx.cipher_list(cipher);

  // Get the filtered cipher list
  mysql_harness::Ssl ssl{SSL_new(ctx.get())};
  int prio = 0;
  std::vector<std::string> r{};
  while (auto c = SSL_get_cipher_list(ssl.get(), prio++)) {
    r.emplace_back(c);
  }

  EXPECT_THAT(r, Contains(cipher));
}

static const std::string deprecated_ciphers_test_data[] = {
    {"ECDHE-ECDSA-AES128-SHA256"},
    {"ECDHE-RSA-AES128-SHA256"},
    {"ECDHE-ECDSA-AES256-SHA384"},
    {"ECDHE-RSA-AES256-SHA384"},
    {"DHE-DSS-AES256-GCM-SHA384"},
    {"DHE-DSS-AES128-GCM-SHA256"},
    {"DHE-DSS-AES128-SHA256"},
    {"DHE-DSS-AES256-SHA256"},
    {"DHE-RSA-AES256-SHA256"},
    {"DHE-RSA-AES128-SHA256"},
    {"AES128-GCM-SHA256"},
    {"AES256-GCM-SHA384"},
    {"AES128-SHA256"},
    {"AES256-SHA256"},
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
    {"DHE-RSA-CAMELLIA128-SHA256"},
    {"DHE-RSA-CAMELLIA256-SHA256"},
    {"ECDHE-RSA-AES128-SHA"},
    {"ECDHE-ECDSA-AES128-SHA"},
    {"ECDHE-RSA-AES256-SHA"},
    {"ECDHE-ECDSA-AES256-SHA"},
#endif
#if OPENSSL_VERSION_NUMBER == ROUTER_OPENSSL_VERSION(1, 1, 0)
    {"DHE-RSA-CAMELLIA128-SHA"},
    {"ECDH-ECDSA-AES128-SHA256"},
    {"ECDH-RSA-AES128-SHA256"},
    {"ECDH-RSA-AES256-SHA384"},
    {"ECDH-ECDSA-AES256-SHA384"},
    {"ECDH-ECDSA-AES128-SHA"},
    {"DHE-RSA-AES128-SHA"},
    {"DHE-RSA-AES256-SHA"},
    {"DHE-DSS-AES256-SHA"},
    {"DHE-RSA-CAMELLIA256-SHA"},
    {"ECDH-ECDSA-AES256-SHA"},
    {"ECDH-RSA-AES128-SHA"},
    {"ECDH-RSA-AES256-SHA"},
    {"AES128-SHA"},
    {"AES256-SHA"},
    {"CAMELLIA256-SHA"},
    {"CAMELLIA128-SHA"},
#endif
#if OPENSSL_VERSION_NUMBER <= ROUTER_OPENSSL_VERSION(1, 1, 0)
    {"ECDH-ECDSA-AES128-GCM-SHA256"},
    {"ECDH-ECDSA-AES256-GCM-SHA384"},
    {"ECDH-RSA-AES128-GCM-SHA256"},
    {"ECDH-RSA-AES256-GCM-SHA384"},
#endif
#if 0  // embedded
    {"TLS_AES_128_CCM_8_SHA256"},
    {"ECDHE-ECDSA-AES256-CCM8"},
    {"ECDHE-ECDSA-AES128-CCM8"},
    {"DHE-RSA-AES256-CCM8"},
    {"DHE-RSA-AES128-CCM8"},
    {"AES128-CCM"},
    {"AES128-CCM8"},
    {"AES256-CCM"},
    {"AES256-CCM8"},
#endif
    // All TLS-SRP ciphers
};

INSTANTIATE_TEST_SUITE_P(CiphersDeprecatedParam, CiphersDeprecated,
                         ::testing::ValuesIn(deprecated_ciphers_test_data));

class CiphersUnacceptable : public TlsServerContextTest,
                            public ::testing::WithParamInterface<std::string> {
};

// Check .cipher_list() does not return unacceptable ciphers if used
TEST_P(CiphersUnacceptable, CiphersUnacceptableParam) {
  std::string cipher = GetParam();

  TlsServerContext ctx;

  // set cipher_list to cipher, then get the modified and filtered list
  ctx.cipher_list(cipher);

  // Get the filtered cipher list
  mysql_harness::Ssl ssl{SSL_new(ctx.get())};
  int prio = 0;
  std::vector<std::string> r{};
  while (auto c = SSL_get_cipher_list(ssl.get(), prio++)) {
    r.emplace_back(c);
  }

  EXPECT_THAT(r, Not(Contains(cipher)));
}

static const std::string unacceptable_ciphers_test_data[] = {
    {"AECDH-NULL-SHA"},
    {"ECDHE-RSA-NULL-SHA"},
    {"ECDHE-ECDSA-NULL-SHA"},
    {"GOST94-NULL-GOST94"},
    {"GOST2001-GOST89-GOST89"},
    {"ECDH-RSA-NULL-SHA"},
    {"ECDH-ECDSA-NULL-SHA"},
    {"NULL-SHA256"},
    {"NULL-SHA"},
    {"NULL-MD5"},
    {"AECDH-AES256-SHA"},
    {"ADH-AES256-GCM-SHA384"},
    {"ADH-AES256-SHA256"},
    {"ADH-AES256-SHA"},
    {"ADH-CAMELLIA256-SHA256"},
    {"ADH-CAMELLIA256-SHA"},
    {"AECDH-AES128-SHA"},
    {"ADH-AES128-GCM-SHA256"},
    {"ADH-AES128-SHA256"},
    {"ADH-AES128-SHA"},
    {"ADH-CAMELLIA128-SHA256"},
    {"AADH-CAMELLIA128-SHA"},
    {"AECDH-RC4-SHA"},
    {"ADH-RC4-MD5"},
    {"AECDH-DES-CBC3-SHA"},
    {"ADH-DES-CBC3-SHA"},
    {"ADH-DES-CBC-SHA"},
    {"EXP-RC4-MD5"},
    {"EXP-RC2-CBC-MD5"},
    {"EXP-DES-CBC-SHA"},
    // SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA
    // SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA
    {"EXP-DH-DSS-DES-CBC-SHA"},
    {"EXP-DH-RSA-DES-CBC-SHA"},
    {"EXP-EDH-DSS-DES-CBC-SHA"},
    {"EXP-EDH-RSA-DES-CBC-SHA"},
    {"EXP-ADH-RC4-MD5"},
    {"EXP-ADH-DES-CBC-SHA"},
    {"EXP-KRB5-DES-CBC-SHA"},
    {"EXP-KRB5-RC2-CBC-SHA"},
    {"EXP-KRB5-RC4-SHA"},
    {"EXP-KRB5-DES-CBC-MD5"},
    {"EXP-KRB5-RC2-CBC-MD5"},
    {"EXP-KRB5-RC4-MD5"},
    {"EXP-RC4-MD5"},
    {"EXP-RC2-CBC-MD5"},
    {"TLS_RSA_EXPORT_WITH_DES40_CBC_SHA"},
    {"EXP-EDH-DSS-DES-CBC-SHA"},
    {"EXP-EDH-RSA-DES-CBC-SHA"},
    {"EXP-ADH-RC4-MD5"},
    {"EXP-ADH-DES-CBC-SHA"},
    {"EXP1024-DES-CBC-SHA"},
    {"EXP1024-RC4-SHA"},
    {"EXP1024-RC4-MD5"},
    {"EXP1024-RC2-CBC-MD5"},
    {"EXP1024-DHE-DSS-DES-CBC-SHA"},
    {"EXP1024-DHE-DSS-RC4-SHA"},
    {"EXP-RC4-MD5"},
    {"EXP-RC2-CBC-MD5"},
    {"EXP-RC2-MD5"},
    {"EDH-RSA-DES-CBC-SHA"},
    {"EDH-DSS-DES-CBC-SHA"},
    {"ADH-DES-CBC-SHA"},
    {"DES-CBC-SHA"},
    {"ADH-RC4-MD5"},
    {"RC4-MD5"},
    {"NULL-MD5"},
    {"ECDHE-RSA-RC4-SHA"},
    {"ECDHE-ECDSA-RC4-SHA"},
    {"AECDH-RC4-SHA"},
    {"ECDH-RSA-RC4-SHA"},
    {"ECDH-ECDSA-RC4-SHA"},
    {"RC4-SHA"},
    {"AECDH-NULL-SHA"},
    {"ECDH-RSA-NULL-SHA"},
    {"ECDH-ECDSA-NULL-SHA"},
    {"PSK-AES256-CBC-SHA"},
    {"PSK-AES128-CBC-SHA"},
    {"PSK-3DES-EDE-CBC-SHA"},
    {"PSK-RC4-SHA"},
    {"EXP-RC2-CBC-MD5"},
    {"EXP-KRB5-RC2-CBC-SHA"},
    {"EXP1024-RC2-CBC-MD5"},
    {"RC2-CBC-MD5"},
    {"EXP-RC2-CBC-MD5"},
    {"DH-RSA-AES128-SHA256"},
    {"DH-RSA-AES256-SHA256"},
    {"DH-DSS-AES128-SHA256"},
    {"DH-DSS-AES128-SHA"},
    {"DH-DSS-AES256-SHA"},
    {"DH-DSS-AES256-SHA256"},
    {"DH-RSA-AES128-SHA"},
    {"DH-RSA-AES256-SHA"},
    {"DH-DSS-AES128-GCM-SHA256"},
    {"DH-DSS-AES256-GCM-SHA384"},
    {"DH-RSA-AES128-GCM-SHA256"},
    {"DH-RSA-AES256-GCM-SHA384"},
    {"DH-DSS-DES-CBC3-SHA"},
    {"DH-RSA-DES-CBC3-SHA"},
    {"EDH-DSS-DES-CBC3-SHA"},
    {"EDH-RSA-DES-CBC3-SHA"},
    {"ECDH-RSA-DES-CBC3-SHA"},
    {"ECDH-ECDSA-DES-CBC3-SHA"},
    {"ECDHE-RSA-DES-CBC3-SHA"},
    {"ECDHE-ECDSA-DES-CBC3-SHA"},
    {"DES-CBC3-SHA"},
};

INSTANTIATE_TEST_SUITE_P(CiphersUnacceptableParam, CiphersUnacceptable,
                         ::testing::ValuesIn(unacceptable_ciphers_test_data));

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
