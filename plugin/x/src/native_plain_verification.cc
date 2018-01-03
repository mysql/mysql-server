/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#include "plugin/x/src/native_plain_verification.h"

#include "mysql_com.h"
#include "sha1.h"  // for SHA1_HASH_SIZE

namespace xpl {

const std::string Native_plain_verification::k_empty_salt;

bool Native_plain_verification::verify_authentication_string(
    const std::string &user,
    const std::string &host,
    const std::string &client_string,
    const std::string &db_string) const {
  // There is no need to perform additional authentication if the given
  // credentials are already in the cache.
  if (m_sha256_password_cache &&
      m_sha256_password_cache->contains(user, host, client_string)) {
    return true;
  }

  bool client_string_matches =
      client_string.empty() &&
      db_string.empty();

  if (!client_string_matches &&
      compute_password_hash(client_string) == db_string) {
    client_string_matches = true;
  }

  if (client_string_matches &&
      m_sha256_password_cache) {
    m_sha256_password_cache->upsert(user, host, client_string);
  }

  return client_string_matches;
}

std::string Native_plain_verification::compute_password_hash(
    const std::string &password) const {
  std::string hash(2 * SHA1_HASH_SIZE + 2, '\0');
  make_scrambled_password(&hash[0], password.c_str());
  hash.resize(2 * SHA1_HASH_SIZE + 1);  // strip the \0
  return hash;
}

}  // namespace xpl
