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

#ifndef MYSQLROUTER_ROUTING_AUTH_DIGEST_H
#define MYSQLROUTER_ROUTING_AUTH_DIGEST_H

#include <optional>
#include <string_view>

#include "openssl_digest.h"

namespace routing::impl {

/*
 * scramble the password with the nonce using a digest function.
 *
 * @param nonce the use-once number
 * @param password cleartext password to scramble
 * @param digest_func function to use for scrambling
 * @tparam Ret the inner return type.
 * @tparam nonce_before_double_hashed_password if nonce or password should be
 * hashed first.
 */
template <class Ret, bool nonce_before_double_hashed_password>
inline std::optional<Ret> scramble(std::string_view nonce,
                                   std::string_view password,
                                   const EVP_MD *digest_func) {
  using return_type = Ret;

  // in case of empty password, the hash is empty too
  if (password.size() == 0) return Ret{};

  openssl::DigestFunc func(digest_func);

  const int digest_size = func.size();

  openssl::DigestCtx digest(func);

  if (!digest.init() || !digest.update(password)) {
    return std::nullopt;
  }

  return_type hashed_password;
  hashed_password.resize(digest_size);

  if (!digest.finalize(hashed_password) || !digest.init() ||
      !digest.update(hashed_password)) {
    return std::nullopt;
  }

  // digest2 (double-hashed password)
  return_type digest2;
  digest2.resize(digest_size);

  if (!digest.finalize(digest2) || !digest.init()) {
    return std::nullopt;
  }

  if (nonce_before_double_hashed_password) {
    if (!digest.update(nonce) || !digest.update(digest2)) {
      return std::nullopt;
    }
  } else {
    if (!digest.update(digest2) || !digest.update(nonce)) {
      return std::nullopt;
    }
  }

  // overwrite the double-hashed password buffer as it isn't needed anymore
  //
  // hash(nonce + double-hashed)
  if (!digest.finalize(digest2)) {
    return std::nullopt;
  }

  // scramble the hashed password with the hash(nonce + double-hashed)
  for (int i = 0; i < digest_size; ++i) {
    hashed_password[i] ^= digest2[i];
  }

  return hashed_password;
}
}  // namespace routing::impl

template <class Ret>
std::optional<Ret> mysql_native_password_scramble(std::string_view nonce,
                                                  std::string_view pwd) {
  return routing::impl::scramble<Ret, true>(nonce, pwd, EVP_sha1());
}

template <class Ret>
std::optional<Ret> caching_sha2_password_scramble(std::string_view nonce,
                                                  std::string_view pwd) {
  return routing::impl::scramble<Ret, false>(nonce, pwd, EVP_sha256());
}

#endif
