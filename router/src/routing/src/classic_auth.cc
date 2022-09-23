/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "classic_auth.h"

#include <array>
#include <memory>  // unique_ptr
#include <system_error>

#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "auth_digest.h"
#include "mysql/harness/stdx/expected.h"
#include "openssl_version.h"

/**
 * remove trailing \0 in a string_view.
 *
 * returns the original string-view, if there is no trailing NUL-char.
 */
std::string_view AuthBase::strip_trailing_null(std::string_view s) {
  if (s.empty()) return s;

  if (s.back() == '\0') s.remove_suffix(1);

  return s;
}

template <>
struct OsslDeleter<BIO> {
  void operator()(BIO *b) { BIO_free_all(b); }
};

template <>
struct OsslDeleter<EVP_PKEY_CTX> {
  void operator()(EVP_PKEY_CTX *ctx) { EVP_PKEY_CTX_free(ctx); }
};

template <>
struct OsslDeleter<X509> {
  void operator()(X509 *x) { X509_free(x); }
};

#if OPENSSL_VERSION_NUMBER < ROUTER_OPENSSL_VERSION(1, 1, 0)
template <>
struct OsslDeleter<RSA> {
  void operator()(RSA *rsa) { RSA_free(rsa); }
};
#endif

using Bio = std::unique_ptr<BIO, OsslDeleter<BIO>>;
using EvpPkey = std::unique_ptr<EVP_PKEY, OsslDeleter<EVP_PKEY>>;
using X509_managed = std::unique_ptr<X509, OsslDeleter<X509>>;
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(3, 0, 0)
using EvpPkeyCtx = std::unique_ptr<EVP_PKEY_CTX, OsslDeleter<EVP_PKEY_CTX>>;
#endif
#if OPENSSL_VERSION_NUMBER < ROUTER_OPENSSL_VERSION(1, 1, 0)
using Rsa = std::unique_ptr<RSA, OsslDeleter<RSA>>;
#endif

stdx::expected<std::string, std::error_code>
AuthBase::public_key_from_ssl_ctx_as_pem(SSL_CTX *ssl_ctx) {
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 0, 2)
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
  const auto *pubkey = X509_get0_pubkey(SSL_CTX_get0_certificate(ssl_ctx));
#elif OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 0, 2)
  EvpPkey pubkey_managed(X509_get_pubkey(SSL_CTX_get0_certificate(ssl_ctx)));

  const auto *pubkey = pubkey_managed.get();
#endif
  Bio bio{BIO_new(BIO_s_mem())};

  PEM_write_bio_PUBKEY(bio.get(), const_cast<EVP_PKEY *>(pubkey));

  char *data = nullptr;
  auto data_len = BIO_get_mem_data(bio.get(), &data);

  return {std::in_place, data, data + data_len};
#else
  // 1.0.1 has no SSL_CTX_get_certificate
  (void)ssl_ctx;

  return stdx::make_unexpected(
      make_error_code(std::errc::function_not_supported));
#endif
}

stdx::expected<EvpPkey, std::error_code> AuthBase::public_key_from_pem(
    std::string_view pubkey) {
  // openssl 1.0.1 needs to the const-cast.
  Bio bio{BIO_new_mem_buf(const_cast<char *>(pubkey.data()), pubkey.size())};

  EvpPkey pkey{PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr)};
  if (!pkey) {
    return stdx::make_unexpected(make_error_code(std::errc::bad_message));
  }

  return pkey;
}

stdx::expected<std::string, std::error_code> AuthBase::public_key_encrypt(
    std::string plaintext, EVP_PKEY *pkey) {
  std::string data;

#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(3, 0, 0)
  data.resize(EVP_PKEY_get_size(pkey));

  EvpPkeyCtx key_ctx{EVP_PKEY_CTX_new(pkey, nullptr)};
  EVP_PKEY_encrypt_init(key_ctx.get());
  EVP_PKEY_CTX_set_rsa_padding(key_ctx.get(), RSA_PKCS1_OAEP_PADDING);

  size_t encrypted_len;

  EVP_PKEY_encrypt(key_ctx.get(),  //
                   reinterpret_cast<unsigned char *>(data.data()),
                   &encrypted_len,
                   reinterpret_cast<const unsigned char *>(plaintext.data()),
                   plaintext.size());
#else
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
  auto *rsa = EVP_PKEY_get0_RSA(pkey);
#else
  Rsa rsa_managed(EVP_PKEY_get1_RSA(pkey));
  auto *rsa = rsa_managed.get();
#endif
  data.resize(RSA_size(rsa));

  int encrypted_len = RSA_public_encrypt(
      plaintext.size(), reinterpret_cast<unsigned char *>(plaintext.data()),
      reinterpret_cast<unsigned char *>(data.data()), const_cast<RSA *>(rsa),
      RSA_PKCS1_OAEP_PADDING);
  if (encrypted_len == -1) {
    return stdx::make_unexpected(make_error_code(std::errc::bad_message));
  }
#endif

  data.resize(encrypted_len);

  return data;
}

stdx::expected<std::string, std::error_code> AuthBase::private_key_decrypt(
    std::string_view ciphertext, EVP_PKEY *priv) {
  if (ciphertext.empty()) {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  if (priv == nullptr) {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  std::array<unsigned char, 1024> plaintext;
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(3, 0, 0)
  EvpPkeyCtx key_ctx{EVP_PKEY_CTX_new(priv, nullptr)};
  {
    auto init_res = EVP_PKEY_decrypt_init(key_ctx.get());
    if (init_res != 1) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }
  }

  {
    auto padding_res =
        EVP_PKEY_CTX_set_rsa_padding(key_ctx.get(), RSA_PKCS1_OAEP_PADDING);
    if (padding_res != 1) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }
  }

  size_t decrypted_len = plaintext.size();

  {
    const auto decrypt_res = EVP_PKEY_decrypt(
        key_ctx.get(),                     //
        plaintext.data(), &decrypted_len,  //
        reinterpret_cast<const unsigned char *>(ciphertext.data()),
        ciphertext.size());
    if (decrypt_res != 1) {
      switch (decrypt_res) {
        case -2:
          return stdx::make_unexpected(
              make_error_code(std::errc::function_not_supported));
        default:
          return stdx::make_unexpected(
              make_error_code(std::errc::invalid_argument));
      }
    }
  }
#else
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
  auto *rsa = EVP_PKEY_get0_RSA(priv);
#else
  Rsa rsa_managed(EVP_PKEY_get1_RSA(priv));
  auto *rsa = rsa_managed.get();
#endif

  // encrypted password
  int decrypted_len = RSA_private_decrypt(
      ciphertext.size(),
      reinterpret_cast<const unsigned char *>(ciphertext.data()),
      reinterpret_cast<unsigned char *>(plaintext.data()),
      const_cast<RSA *>(rsa), RSA_PKCS1_OAEP_PADDING);
  if (decrypted_len == -1) {
    return stdx::make_unexpected(make_error_code(std::errc::bad_message));
  }
#endif

  return {std::in_place, plaintext.data(), plaintext.data() + decrypted_len};
}

// xor the plaintext password with the repeated scramble.
static void xor_plaintext(std::string &plaintext, std::string_view pattern) {
  for (size_t n = 0, p = 0; n < plaintext.size(); ++n, ++p) {
    if (p == pattern.size()) p = 0;

    plaintext[n] ^= pattern[p];
  }
}

stdx::expected<std::string, std::error_code> AuthBase::rsa_decrypt_password(
    SSL_CTX *ssl_ctx, std::string_view encrypted, std::string_view nonce) {
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 0, 2)
  auto decrypted_res = AuthBase::private_key_decrypt(
      encrypted, SSL_CTX_get0_privatekey(ssl_ctx));
  if (!decrypted_res) return stdx::make_unexpected(decrypted_res.error());

  auto plaintext = *decrypted_res;

  xor_plaintext(plaintext, nonce);

  if (plaintext.size() == 0 || plaintext.back() != '\0') {
    // after decrypting and xor'ing the last byte should be a '\0'
    return stdx::make_unexpected(make_error_code(std::errc::bad_message));
  }

  // strip trailing \0
  plaintext.resize(plaintext.size() - 1);

  return plaintext;
#else
  (void)ssl_ctx;
  (void)encrypted;
  (void)nonce;

  return stdx::make_unexpected(
      make_error_code(std::errc::function_not_supported));
#endif
}

stdx::expected<std::string, std::error_code> AuthBase::rsa_encrypt_password(
    const EvpPkey &pkey, std::string_view password, std::string_view nonce) {
  auto plaintext = std::string(password);
  plaintext.push_back('\0');

  if (nonce.empty()) {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }
  if (!pkey) {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  xor_plaintext(plaintext, nonce);

  return AuthBase::public_key_encrypt(plaintext, pkey.get());
}

bool AuthBase::connection_has_public_key(
    MysqlRoutingClassicConnection *connection) {
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 0, 2)
  if (!connection->context().source_ssl_ctx()) return false;

  SSL_CTX *ssl_ctx = connection->context().source_ssl_ctx()->get();

  return SSL_CTX_get0_certificate(ssl_ctx) != nullptr;
#else
  (void)connection;
  return false;
#endif
}
