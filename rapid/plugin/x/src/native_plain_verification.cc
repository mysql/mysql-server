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

#include "plugin/x/src/native_plain_verification.h"

#include "mysql_com.h"
#include "sha1.h"  // for SHA1_HASH_SIZE

namespace xpl {

const std::string Native_plain_verification::k_empty_salt;

bool Native_plain_verification::verify_authentication_string(
    const std::string &client_string, const std::string &db_string) const {
  return client_string.empty()
             ? db_string.empty()
             : compute_password_hash(client_string) == db_string;
}

std::string Native_plain_verification::compute_password_hash(
    const std::string &password) const {
  std::string hash(2 * SHA1_HASH_SIZE + 2, '\0');
  make_scrambled_password(&hash[0], password.c_str());
  hash.resize(2 * SHA1_HASH_SIZE + 1);  // strip the \0
  return hash;
}

}  // namespace xpl
