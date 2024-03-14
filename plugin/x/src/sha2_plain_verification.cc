/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
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

#include "crypt_genhash_impl.h"
#include "sql/auth/i_sha2_password.h"

namespace xpl {

const unsigned int STORED_SHA256_DIGEST_LENGTH = 43;
const size_t CACHING_SHA2_PASSWORD_MAX_PASSWORD_LENGTH = MAX_PLAINTEXT_LENGTH;
const std::string Sha2_plain_verification::k_empty_salt;

bool Sha2_plain_verification::verify_authentication_string(
    const std::string &user, const std::string &host,
    const std::string &client_string, const std::string &db_string) const {
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

    b = db_string.find('$', b + 1);
    if (b == std::string::npos) return false;

    std::string iteration_info =
        db_string.substr(b + 1, sha2_password::ITERATION_LENGTH);
    unsigned int iterations =
        std::min((std::stoul(iteration_info, nullptr, 16)) *
                     sha2_password::ITERATION_MULTIPLIER,
                 sha2_password::MAX_ITERATIONS);

    b = db_string.find('$', b + 1);
    if (b == std::string::npos) return false;

    std::string salt = db_string.substr(b + 1, CRYPT_SALT_LENGTH);
    if (salt.size() != CRYPT_SALT_LENGTH) return false;

    std::string digest = db_string.substr(b + CRYPT_SALT_LENGTH + 1);

    if (compute_password_hash(client_string, salt, iterations) == digest) {
      client_string_matches = true;
    }
  }

  if (client_string_matches && m_sha256_password_cache) {
    m_sha256_password_cache->upsert(user, host, client_string);
  }

  return client_string_matches;
}

std::string Sha2_plain_verification::compute_password_hash(
    const std::string &password, const std::string &salt,
    unsigned int iteration_count) const {
  char hash[CRYPT_MAX_PASSWORD_SIZE + 1] = {0};
  ::my_crypt_genhash(hash, CRYPT_MAX_PASSWORD_SIZE, password.c_str(),
                     password.size(), salt.c_str(), nullptr, &iteration_count);
  std::string generated_digest;
  generated_digest.assign(hash + 3 + CRYPT_SALT_LENGTH + 1,
                          STORED_SHA256_DIGEST_LENGTH);
  return generated_digest;
}

}  // namespace xpl
