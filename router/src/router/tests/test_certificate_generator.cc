/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "certificate_generator.h"

#include <fstream>

#include <openssl/crypto.h>  // OPENSSL_free
#include <openssl/evp.h>

#include "mysql/harness/filesystem.h"
#include "mysql/harness/tls_context.h"
#include "test/helpers.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
const BIGNUM *RSA_get0_n(const RSA *rsa) { return rsa->n; }

RSA *EVP_PKEY_get0_RSA(EVP_PKEY *pkey) { return pkey->pkey.rsa; }
#endif
}  // namespace

class CertificateGeneratorTest : public ::testing::Test {
 public:
  TlsLibraryContext m_tls_lib_ctx;
  CertificateGenerator m_cert_gen;
};

TEST_F(CertificateGeneratorTest, test_EVP_PKEY_generation) {
  const auto evp = m_cert_gen.generate_evp_pkey();
  ASSERT_TRUE(evp);

  SCOPED_TRACE("// get modulus of the generated RSA key");
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
  BIGNUM *bn_modulus{};
  ASSERT_TRUE(EVP_PKEY_get_bn_param(evp->get(), "n", &bn_modulus));

  std::unique_ptr<BIGNUM, decltype(&BN_clear_free)> bn_storage{bn_modulus,
                                                               &BN_clear_free};
#else
  EXPECT_EQ(EVP_PKEY_id(evp->get()), EVP_PKEY_RSA);
  const auto rsa = EVP_PKEY_get0_RSA(evp->get());  // deprecated in 3.0
  ASSERT_TRUE(rsa);

  const auto bn_modulus = RSA_get0_n(rsa);  // deprecated in 3.0
#endif

  const auto &openssl_free = [](char *c) { OPENSSL_free(c); };
  const std::unique_ptr<char, decltype(openssl_free)> bn{BN_bn2dec(bn_modulus),
                                                         openssl_free};
  ASSERT_TRUE(bn);
  ASSERT_STRNE(bn.get(), nullptr);
}

TEST_F(CertificateGeneratorTest, test_write_PKEY_to_string) {
  const auto evp = m_cert_gen.generate_evp_pkey();
  const auto &key_string = m_cert_gen.pkey_to_string(evp->get());

  EXPECT_THAT(key_string, ::testing::HasSubstr("BEGIN RSA PRIVATE KEY"));
}

TEST_F(CertificateGeneratorTest, test_generate_CA_cert) {
  const auto ca_key = m_cert_gen.generate_evp_pkey();
  ASSERT_TRUE(ca_key);

  const auto ca_cert =
      m_cert_gen.generate_x509(ca_key->get(), "CA", 1, nullptr, nullptr);
  ASSERT_TRUE(ca_cert);

  ASSERT_TRUE(X509_verify(ca_cert->get(), ca_key->get()));
}

TEST_F(CertificateGeneratorTest, test_generate_router_cert) {
  const auto ca_key = m_cert_gen.generate_evp_pkey();
  ASSERT_TRUE(ca_key);

  const auto ca_cert =
      m_cert_gen.generate_x509(ca_key->get(), "CA", 1, nullptr, nullptr);
  ASSERT_TRUE(ca_cert);

  const auto router_key = m_cert_gen.generate_evp_pkey();
  ASSERT_TRUE(router_key);

  const auto router_cert = m_cert_gen.generate_x509(
      router_key->get(), "router CN", 1, ca_cert->get(), ca_key->get());
  ASSERT_TRUE(router_cert);

  std::unique_ptr<X509_STORE, decltype(&::X509_STORE_free)> store{
      X509_STORE_new(), ::X509_STORE_free};
  std::unique_ptr<X509_STORE_CTX, decltype(&::X509_STORE_CTX_free)> ctx{
      X509_STORE_CTX_new(), ::X509_STORE_CTX_free};
  X509_STORE_add_cert(store.get(), ca_cert->get());
  X509_STORE_CTX_init(ctx.get(), store.get(), router_cert->get(), nullptr);

  EXPECT_TRUE(X509_verify_cert(ctx.get()));
  EXPECT_EQ(X509_STORE_CTX_get_error(ctx.get()), 0);
}

#ifndef NDEBUG
TEST_F(CertificateGeneratorTest, death_test_generate_cert_wrong_serial) {
  const auto key_res = m_cert_gen.generate_evp_pkey();
  ASSERT_TRUE(key_res) << key_res.error();

  ASSERT_DEATH(
      m_cert_gen.generate_x509(key_res->get(), "test CN", 0, nullptr, nullptr),
      "");
}

TEST_F(CertificateGeneratorTest, death_test_generate_cert_wrong_CN) {
  const auto key_res = m_cert_gen.generate_evp_pkey();
  ASSERT_TRUE(key_res);

  std::string too_long(100, 'x');
  ASSERT_DEATH(
      m_cert_gen.generate_x509(key_res->get(), too_long, 1, nullptr, nullptr),
      "");
}

TEST_F(CertificateGeneratorTest, death_test_generate_cert_no_CA_key) {
  const auto ca_key_res = m_cert_gen.generate_evp_pkey();
  ASSERT_TRUE(ca_key_res) << ca_key_res.error();

  const auto ca_cert_res =
      m_cert_gen.generate_x509(ca_key_res->get(), "CA", 1, nullptr, nullptr);
  ASSERT_TRUE(ca_cert_res) << ca_cert_res.error();

  const auto router_key_res = m_cert_gen.generate_evp_pkey();
  ASSERT_TRUE(router_key_res) << router_key_res.error();

  ASSERT_DEATH(m_cert_gen.generate_x509(router_key_res->get(), "router CN", 1,
                                        ca_cert_res->get(), nullptr),
               "");
}

TEST_F(CertificateGeneratorTest, death_test_generate_cert_no_CA_cert) {
  const auto ca_key_res = m_cert_gen.generate_evp_pkey();
  ASSERT_TRUE(ca_key_res) << ca_key_res.error();

  const auto router_key_res = m_cert_gen.generate_evp_pkey();
  ASSERT_TRUE(router_key_res) << router_key_res.error();

  ASSERT_DEATH(m_cert_gen.generate_x509(router_key_res->get(), "router CN", 1,
                                        nullptr, ca_key_res->get()),
               "");
}
#endif

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
