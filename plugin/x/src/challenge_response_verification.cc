/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates.
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

#include "plugin/x/src/challenge_response_verification.h"

#include "crypt_genhash_impl.h"  // NOLINT(build/include_subdir)
#include "mysql_com.h"           // NOLINT(build/include_subdir)

namespace xpl {

Challenge_response_verification::Challenge_response_verification(
    iface::SHA256_password_cache *cache)
    : k_salt(generate_salt()), m_sha256_password_cache(cache) {}

const std::string &Challenge_response_verification::get_salt() const {
  return k_salt;
}

std::string Challenge_response_verification::generate_salt() {
  std::string salt(SCRAMBLE_LENGTH, '\0');
  ::generate_user_salt(&salt[0], static_cast<int>(salt.size()));
  return salt;
}

}  // namespace xpl
