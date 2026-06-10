/*
 * Copyright (c) 2017, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/sha2_plain_verification.h"

#include <algorithm>

#include "base64_encode.h"
#include "crypt_genhash_impl.h"
#include "scope_guard.h"
#include "sql/auth/i_sha2_password.h"

#include <openssl/kdf.h>
#include <openssl/ssl.h>

namespace xpl {

namespace {

const unsigned int STORED_SHA256_DIGEST_LENGTH = 43;
const size_t CACHING_SHA2_PASSWORD_MAX_PASSWORD_LENGTH = MAX_PLAINTEXT_LENGTH;
const size_t PBKDF2_DIGEST_LENGTH = SHA512_DIGEST_LENGTH;

constexpr size_t STORED_PBKDF2_DIGEST_LENGTH =
    4 * ((PBKDF2_DIGEST_LENGTH + 2) / 3);

Digest_type get_digest_type(const std::string &text_with_algorithm_type) {
  if (text_with_algorithm_type == "A") return Digest_type::CRYPT5;
  if (text_with_algorithm_type == "B") return Digest_type::PBKDF2_SHA512;

  return Digest_type::LAST;
}

Digest_type get_digest_from_name(const std::string &name) {
  if (name == "CRYPT5") return Digest_type::CRYPT5;
  if (name == "PBKDF2_SHA512") return Digest_type::PBKDF2_SHA512;

  return Digest_type::LAST;
}

std::string generate_crypt5(const std::string &password,
                            const std::string &salt,
                            unsigned int iteration_count) {
  char hash[CRYPT_MAX_PASSWORD_SIZE + 1] = {0};
  ::my_crypt_genhash(hash, CRYPT_MAX_PASSWORD_SIZE, password.c_str(),
                     password.size(), salt.c_str(), nullptr, &iteration_count);
  std::string generated_digest;
  generated_digest.assign(hash + 3 + CRYPT_SALT_LENGTH + 1,
                          STORED_SHA256_DIGEST_LENGTH);
  return generated_digest;
}

bool generate_pbkdf2(const std::string &src, const std::string &random,
                     std::string &digest, unsigned int iterations) {
  unsigned char derived_key[PBKDF2_DIGEST_LENGTH];
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_KDF *kdf{nullptr};
  EVP_KDF_CTX *ctx{nullptr};
  OSSL_PARAM params[5];
  char sha512[] = "SHA512";
  kdf = EVP_KDF_fetch(nullptr, "PBKDF2", nullptr);
  ctx = EVP_KDF_CTX_new(kdf);
  auto cleanup_guard = create_scope_guard([&] {
    if (ctx) EVP_KDF_CTX_free(ctx);
    if (kdf) EVP_KDF_free(kdf);
  });
  params[0] = OSSL_PARAM_construct_utf8_string("digest", sha512, 0);
  params[1] = OSSL_PARAM_construct_octet_string(
      "salt", static_cast<void *>(const_cast<char *>(random.c_str())),
      random.length());
  params[2] = OSSL_PARAM_construct_uint("iter", (unsigned int *)&iterations);
  params[3] = OSSL_PARAM_construct_octet_string(
      "pass", static_cast<void *>(const_cast<char *>(src.c_str())),
      src.length());
  params[4] = OSSL_PARAM_construct_end();
  if (EVP_KDF_derive(ctx, derived_key, PBKDF2_DIGEST_LENGTH, params) != 1)
    return true;
#else  /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  if (PKCS5_PBKDF2_HMAC(src.c_str(), src.length(),
                        reinterpret_cast<const unsigned char *>(random.c_str()),
                        random.length(), iterations, EVP_sha512(),
                        PBKDF2_DIGEST_LENGTH, derived_key) != 1)
    return true;
#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */
  digest = oci::ssl::base64_encode(derived_key, PBKDF2_DIGEST_LENGTH);
  assert(digest.length() == STORED_PBKDF2_DIGEST_LENGTH);
  return (digest.length() != STORED_PBKDF2_DIGEST_LENGTH);
}

std::string generate_digest(const std::string &password,
                            const std::string &salt, unsigned int iterations,
                            const Digest_type dt) {
  switch (dt) {
    case Digest_type::CRYPT5: {
      return generate_crypt5(password, salt, iterations);
    } break;
    case Digest_type::PBKDF2_SHA512: {
      std::string result;
      if (generate_pbkdf2(password, salt, result, iterations)) return {};
      return result;
    }
    default:
      return {};
  }
}

}  // namespace

const std::string Sha2_plain_verification::k_empty_salt;

bool Sha2_plain_verification::verify_authentication_string(
    const std::string &user, const std::string &host,
    const std::string &client_string, const std::string &db_string,
    const bool can_update_cache) const {
  if (client_string.length() > CACHING_SHA2_PASSWORD_MAX_PASSWORD_LENGTH)
    return false;

  // There is no need to perform additional authentication if the given
  // credentials are already in the cache.
  if (m_sha256_password_cache &&
      m_sha256_password_cache->contains(user, host, client_string))
    return true;

  bool client_string_matches = client_string.empty() && db_string.empty();

  if (!client_string_matches) {
    /* Format : $A$005$SALTHASH */
    std::string::size_type b = db_string.find('$');
    if (b == std::string::npos) return false;

    auto digest_type = get_digest_type(db_string.substr(b + 1, 1));

    if (Digest_type::LAST == digest_type) return false;

    b = db_string.find('$', b + 1);
    if (b == std::string::npos) return false;

    std::string const iteration_info =
        db_string.substr(b + 1, sha2_password::ITERATION_LENGTH);
    unsigned int const iterations =
        std::min((std::stoul(iteration_info, nullptr, 16)) *
                     sha2_password::ITERATION_MULTIPLIER,
                 sha2_password::MAX_ITERATIONS);

    b = db_string.find('$', b + 1);
    if (b == std::string::npos) return false;

    std::string const salt = db_string.substr(b + 1, CRYPT_SALT_LENGTH);
    if (salt.size() != CRYPT_SALT_LENGTH) return false;

    std::string const digest = db_string.substr(b + CRYPT_SALT_LENGTH + 1);

    auto resulting_digest =
        generate_digest(client_string, salt, iterations, digest_type);

    if (resulting_digest.empty()) return false;

    if (resulting_digest == digest) {
      client_string_matches = true;
    }
  }

  if (client_string_matches && m_sha256_password_cache && can_update_cache) {
    m_sha256_password_cache->upsert(user, host, client_string);
  }

  return client_string_matches;
}

bool is_cache2_password_compliant(const std::string enforced_format,
                                  const std::string &db_string) {
  std::string::size_type b = db_string.find('$');
  if (b == std::string::npos) return false;

  const auto enforced_type = get_digest_from_name(enforced_format);
  const auto digest_type = get_digest_type(db_string.substr(b + 1, 1));

  return enforced_type == digest_type;
}

bool Sha2_plain_verification::is_cache2_password_compliant(
    const std::string enforced_format, const std::string &db_string) const {
  return ::xpl::is_cache2_password_compliant(enforced_format, db_string);
}

}  // namespace xpl
