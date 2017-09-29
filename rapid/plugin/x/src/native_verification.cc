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

#include "plugin/x/src/native_verification.h"

#include "crypt_genhash_impl.h"
#include "mysql_com.h"
#include "password.h"

namespace xpl {

std::string Native_verification::generate_salt() {
  std::string salt(SCRAMBLE_LENGTH, '\0');
  ::generate_user_salt(&salt[0], static_cast<int>(salt.size()));
  return salt;
};

bool Native_verification::verify_authentication_string(
    const std::string &client_string, const std::string &db_string) const {
  if (client_string.empty()) return db_string.empty();

  if (db_string.empty()) return false;

  uint8 db_hash[SCRAMBLE_LENGTH + 1] = {0};
  uint8 user_hash[SCRAMBLE_LENGTH + 1] = {0};
  ::get_salt_from_password(db_hash, db_string.c_str());
  ::get_salt_from_password(user_hash, client_string.c_str());
  return 0 ==
         ::check_scramble((const uchar *)user_hash, k_salt.c_str(), db_hash);
}

}  // namespace xpl
