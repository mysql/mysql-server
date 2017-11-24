/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "plugin/x/src/sha2_plain_verification.h"

#include "crypt_genhash_impl.h"

namespace xpl {

const unsigned int STORED_SHA256_DIGEST_LENGTH= 43;
const size_t CACHING_SHA2_PASSWORD_MAX_PASSWORD_LENGTH= MAX_PLAINTEXT_LENGTH;
const std::string Sha2_plain_verification::k_empty_salt;

bool Sha2_plain_verification::verify_authentication_string(
    const std::string &client_string, const std::string &db_string) const {

  if (client_string.length() > CACHING_SHA2_PASSWORD_MAX_PASSWORD_LENGTH)
    return false;

  if (client_string.empty()) return db_string.empty();

  /* Format : $A$005$SALTHASH */
  std::string::size_type b = db_string.find('$');
  if (b == std::string::npos) return false;

  b= db_string.find('$', b + 1);
  if (b == std::string::npos) return false;

  b= db_string.find('$', b + 1);
  if (b == std::string::npos) return false;

  std::string salt = db_string.substr(b + 1, CRYPT_SALT_LENGTH);
  if (salt.size() != CRYPT_SALT_LENGTH)
    return false;

  std::string digest = db_string.substr(b + CRYPT_SALT_LENGTH + 1);

 return compute_password_hash(client_string, salt) == digest;
}

std::string Sha2_plain_verification::compute_password_hash(
    const std::string &password, const std::string &salt) const {
  char hash[CRYPT_MAX_PASSWORD_SIZE + 1] = {0};
  ::my_crypt_genhash(hash, CRYPT_MAX_PASSWORD_SIZE, password.c_str(),
                     password.size(), salt.c_str(), nullptr);
  std::string generated_digest;
  generated_digest.assign(hash+3+CRYPT_SALT_LENGTH+1,
                          STORED_SHA256_DIGEST_LENGTH);
  return generated_digest;
}

}  // namespace xpl
