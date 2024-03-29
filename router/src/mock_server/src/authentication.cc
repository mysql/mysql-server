/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#include "authentication.h"

#include <memory>  // unique_ptr
#include <optional>
#include <string_view>
#include <vector>

#include <openssl/evp.h>
#include <openssl/opensslv.h>

namespace impl {
/**
 * scramble the password using the client's scheme.
 *
 * - mysql_native_password
 *   - message-digest: SHA1
 *   - inner message-digest-order: nonce + double_hashed_password
 * - caching_sha256_password
 *   - message-digest: SHA256
 *   - inner message-digest-order: double_hashed_password + nonce
 *
 * @param nonce nonce used between server and client
 * @param password cleartext password to be scrambled
 * @param digest_func digest function
 * @param nonce_before_double_hashed_password if true, nonce appears before
 * double_hashed_password; if false, nonce appears after double_hashed_password
 * @returns auth-response a client would send it
 */
static std::optional<std::vector<uint8_t>> scramble(
    std::string_view nonce, std::string_view password,
    const EVP_MD *digest_func, bool nonce_before_double_hashed_password) {
  // in case of empty password, the hash is empty too
  if (password.size() == 0) return std::vector<uint8_t>{};

  const auto digest_size = EVP_MD_size(digest_func);

#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
  std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> digest_ctx(
      EVP_MD_CTX_new(), &EVP_MD_CTX_free);
#else
  std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_destroy)> digest_ctx(
      EVP_MD_CTX_create(), &EVP_MD_CTX_destroy);
#endif

  std::vector<uint8_t> hashed_password(digest_size);
  std::vector<uint8_t> double_hashed_password(digest_size);
  std::vector<uint8_t> hashed_nonce_and_double_hashed_password(digest_size);

  int ok{1};
  ok &= EVP_DigestInit_ex(digest_ctx.get(), digest_func, nullptr);
  ok &= EVP_DigestUpdate(digest_ctx.get(), password.data(), password.size());
  ok &= EVP_DigestFinal_ex(digest_ctx.get(), hashed_password.data(), nullptr);

  ok &= EVP_DigestInit_ex(digest_ctx.get(), digest_func, nullptr);
  ok &= EVP_DigestUpdate(digest_ctx.get(), hashed_password.data(),
                         hashed_password.size());
  ok &= EVP_DigestFinal_ex(digest_ctx.get(), double_hashed_password.data(),
                           nullptr);

  ok &= EVP_DigestInit_ex(digest_ctx.get(), digest_func, nullptr);

  if (nonce_before_double_hashed_password) {
    ok &= EVP_DigestUpdate(digest_ctx.get(), nonce.data(), nonce.size());
    ok &= EVP_DigestUpdate(digest_ctx.get(), double_hashed_password.data(),
                           double_hashed_password.size());
  } else {
    ok &= EVP_DigestUpdate(digest_ctx.get(), double_hashed_password.data(),
                           double_hashed_password.size());
    ok &= EVP_DigestUpdate(digest_ctx.get(), nonce.data(), nonce.size());
  }
  ok &= EVP_DigestFinal_ex(digest_ctx.get(), double_hashed_password.data(),
                           nullptr);

  if (!ok) return std::nullopt;

  // scramble the hashed password with the nonce
  for (int i = 0; i < digest_size; ++i) {
    hashed_password[i] ^= double_hashed_password[i];
  }

  return hashed_password;
}
}  // namespace impl

// mysql_native_password

std::optional<std::vector<uint8_t>> MySQLNativePassword::scramble(
    std::string_view nonce, std::string_view password) {
  return impl::scramble(nonce, password, EVP_sha1(), true);
}

// caching_sha2_password

std::optional<std::vector<uint8_t>> CachingSha2Password::scramble(
    std::string_view nonce, std::string_view password) {
  return impl::scramble(nonce, password, EVP_sha256(), false);
}

// clear_text_password

std::optional<std::vector<uint8_t>> ClearTextPassword::scramble(
    std::string_view /* nonce */, std::string_view password) {
  std::vector<uint8_t> res(password.begin(), password.end());

  // the payload always has a trailing \0
  res.push_back(0);

  return res;
}
