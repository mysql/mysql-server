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

#ifndef _XPL_CHALLENGE_RESPONSE_VERIFICATION_H_
#define _XPL_CHALLENGE_RESPONSE_VERIFICATION_H_

#include <string>

#include "plugin/x/ngs/include/ngs/interface/account_verification_interface.h"
#include "plugin/x/ngs/include/ngs/interface/sha256_password_cache_interface.h"

#include "crypt_genhash_impl.h"

namespace xpl {

/**
  Class for doing account verification for the challenge response authentication
*/
class Challenge_response_verification
    : public ngs::Account_verification_interface {
 public:
  explicit Challenge_response_verification(
      ngs::SHA256_password_cache_interface *cache)
    : k_salt(generate_salt()), m_sha256_password_cache(cache) {}

  const std::string &get_salt() const override { return k_salt; }

  virtual ~Challenge_response_verification() = default;

 protected:
  const std::string k_salt;
  ngs::SHA256_password_cache_interface *m_sha256_password_cache;

  std::string generate_salt() {
    std::string salt(SCRAMBLE_LENGTH, '\0');
    ::generate_user_salt(&salt[0], static_cast<int>(salt.size()));
    return salt;
  }
};

}  // namespace xpl

#endif  // _XPL_CHALLENGE_RESPONSE_VERIFICATION_H_
