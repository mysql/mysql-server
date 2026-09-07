/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#include "my_config.h"

#include <gtest/gtest.h>

#include "dh_ecdh_config.h"

#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <memory>
#include <string>
#include <string_view>

namespace dh_ecdh_config_unittest {

using SslCtx_ptr = std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)>;

std::string consume_openssl_errors() {
  std::string errors;
  for (unsigned long error = ERR_get_error(); error != 0;
       error = ERR_get_error()) {
    char buffer[256];
    ERR_error_string_n(error, buffer, sizeof(buffer));
    if (!errors.empty()) errors.append("; ");
    errors.append(buffer);
  }
  return errors.empty() ? "no OpenSSL error available" : errors;
}

static const SSL_METHOD *server_method() {
#ifdef HAVE_TLSv13
  return TLS_server_method();
#else
  return SSLv23_server_method();
#endif
}

TEST(DhEcdhConfigTest, DefaultGroupsInitializeSslContext) {
  SSL_library_init();
  SSL_load_error_strings();

  SslCtx_ptr ctx(SSL_CTX_new(server_method()), &SSL_CTX_free);
  ASSERT_NE(ctx, nullptr) << consume_openssl_errors();
  EXPECT_FALSE(set_ecdh(ctx.get(), false, nullptr)) << consume_openssl_errors();
}

#if OPENSSL_VERSION_NUMBER >= 0x30500000L

using Ssl_ptr = std::unique_ptr<SSL, decltype(&SSL_free)>;
using EvpPkey_ptr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using EvpPkeyCtx_ptr =
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using X509_ptr = std::unique_ptr<X509, decltype(&X509_free)>;

EvpPkey_ptr make_rsa_key() {
  EvpPkeyCtx_ptr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr),
                     &EVP_PKEY_CTX_free);
  if (ctx == nullptr) return {nullptr, &EVP_PKEY_free};

  EVP_PKEY *key = nullptr;
  if (EVP_PKEY_keygen_init(ctx.get()) <= 0 ||
      EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), 2048) <= 0 ||
      EVP_PKEY_keygen(ctx.get(), &key) <= 0) {
    EVP_PKEY_free(key);
    return {nullptr, &EVP_PKEY_free};
  }
  return {key, &EVP_PKEY_free};
}

X509_ptr make_self_signed_cert(EVP_PKEY *key) {
  X509_ptr cert(X509_new(), &X509_free);
  if (cert == nullptr) return cert;

  const X509_NAME *name = X509_get_subject_name(cert.get());
  if (X509_set_version(cert.get(), 2) != 1 ||
      ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1) != 1 ||
      X509_gmtime_adj(X509_getm_notBefore(cert.get()), 0) == nullptr ||
      X509_gmtime_adj(X509_getm_notAfter(cert.get()), 60 * 60) == nullptr ||
      X509_set_pubkey(cert.get(), key) != 1 || name == nullptr ||
      X509_NAME_add_entry_by_txt(
          const_cast<X509_NAME *>(name), "CN", MBSTRING_ASC,
          reinterpret_cast<const unsigned char *>("localhost"), -1, -1,
          0) != 1 ||
      X509_set_issuer_name(cert.get(), name) != 1 ||
      X509_sign(cert.get(), key, EVP_sha256()) == 0) {
    return {nullptr, &X509_free};
  }

  return cert;
}

testing::AssertionResult set_tls13_only(SSL_CTX *ctx) {
  if (SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) != 1 ||
      SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION) != 1) {
    return testing::AssertionFailure() << consume_openssl_errors();
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult drive_one_handshake_step(SSL *ssl, bool *done) {
  if (*done) return testing::AssertionSuccess();

  const int result = SSL_do_handshake(ssl);
  if (result == 1) {
    *done = true;
    return testing::AssertionSuccess();
  }

  const int error = SSL_get_error(ssl, result);
  if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE)
    return testing::AssertionSuccess();

  return testing::AssertionFailure() << consume_openssl_errors();
}

testing::AssertionResult complete_handshake(SSL *client, SSL *server) {
  bool client_done = false;
  bool server_done = false;

  for (int attempts = 0; attempts < 100 && (!client_done || !server_done);
       ++attempts) {
    auto client_result = drive_one_handshake_step(client, &client_done);
    if (!client_result) return client_result;

    auto server_result = drive_one_handshake_step(server, &server_done);
    if (!server_result) return server_result;
  }

  if (!client_done || !server_done)
    return testing::AssertionFailure() << "TLS handshake did not complete";

  return testing::AssertionSuccess();
}

void run_default_server_groups_prefer_pqc_over_classical_key_share() {
  OPENSSL_init_ssl(0, nullptr);

  SslCtx_ptr server_ctx(SSL_CTX_new(TLS_server_method()), &SSL_CTX_free);
  ASSERT_NE(server_ctx, nullptr) << consume_openssl_errors();
  ASSERT_TRUE(set_tls13_only(server_ctx.get()));

  EvpPkey_ptr key = make_rsa_key();
  ASSERT_NE(key, nullptr) << consume_openssl_errors();
  X509_ptr cert = make_self_signed_cert(key.get());
  ASSERT_NE(cert, nullptr) << consume_openssl_errors();
  ASSERT_EQ(SSL_CTX_use_certificate(server_ctx.get(), cert.get()), 1)
      << consume_openssl_errors();
  ASSERT_EQ(SSL_CTX_use_PrivateKey(server_ctx.get(), key.get()), 1)
      << consume_openssl_errors();
  ASSERT_EQ(SSL_CTX_check_private_key(server_ctx.get()), 1)
      << consume_openssl_errors();
  ASSERT_FALSE(set_ecdh(server_ctx.get(), false, nullptr))
      << consume_openssl_errors();

  SslCtx_ptr client_ctx(SSL_CTX_new(TLS_client_method()), &SSL_CTX_free);
  ASSERT_NE(client_ctx, nullptr) << consume_openssl_errors();
  ASSERT_TRUE(set_tls13_only(client_ctx.get()));
  SSL_CTX_set_verify(client_ctx.get(), SSL_VERIFY_NONE, nullptr);

  const char *client_groups = "X25519MLKEM768:*X25519";
  if (SSL_CTX_set1_groups_list(client_ctx.get(),
                               const_cast<char *>(client_groups)) != 1) {
    ERR_clear_error();
    GTEST_SKIP() << "OpenSSL build does not provide X25519MLKEM768";
  }

  Ssl_ptr client(SSL_new(client_ctx.get()), &SSL_free);
  Ssl_ptr server(SSL_new(server_ctx.get()), &SSL_free);
  ASSERT_NE(client, nullptr) << consume_openssl_errors();
  ASSERT_NE(server, nullptr) << consume_openssl_errors();

  BIO *client_bio = nullptr;
  BIO *server_bio = nullptr;
  ASSERT_EQ(BIO_new_bio_pair(&client_bio, 0, &server_bio, 0), 1)
      << consume_openssl_errors();
  SSL_set_bio(client.get(), client_bio, client_bio);
  SSL_set_bio(server.get(), server_bio, server_bio);
  SSL_set_connect_state(client.get());
  SSL_set_accept_state(server.get());

  ASSERT_TRUE(complete_handshake(client.get(), server.get()));

  const char *server_group = SSL_get0_group_name(server.get());
  ASSERT_NE(server_group, nullptr) << consume_openssl_errors();
  EXPECT_NE(std::string_view(server_group).find("MLKEM"),
            std::string_view::npos)
      << "negotiated group: " << server_group;
}

#endif  // OPENSSL_VERSION_NUMBER >= 0x30500000L

TEST(DhEcdhConfigTest, DefaultServerGroupsPreferPqcOverClassicalKeyShare) {
#if OPENSSL_VERSION_NUMBER < 0x30500000L
  GTEST_SKIP() << "OpenSSL 3.5.0 or newer is required";
#else
  run_default_server_groups_prefer_pqc_over_classical_key_share();
#endif
}

}  // namespace dh_ecdh_config_unittest
