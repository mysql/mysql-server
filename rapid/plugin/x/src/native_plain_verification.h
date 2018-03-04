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

#ifndef _XPL_NATIVE_PLAIN_VERIFICATION_H_
#define _XPL_NATIVE_PLAIN_VERIFICATION_H_

#include <string>

#include "plugin/x/ngs/include/ngs/interface/account_verification_interface.h"
#include "plugin/x/ngs/include/ngs/interface/sha256_password_cache_interface.h"

namespace xpl {

class Native_plain_verification : public ngs::Account_verification_interface {
 public:
  explicit Native_plain_verification(
      ngs::SHA256_password_cache_interface *cache)
    : m_sha256_password_cache(cache) {}
  const std::string &get_salt() const override { return k_empty_salt; }
  bool verify_authentication_string(const std::string &user,
      const std::string &client_string, const std::string &host,
      const std::string &db_string) const override;

 private:
  static const std::string k_empty_salt;
  std::string compute_password_hash(const std::string &password) const;
  ngs::SHA256_password_cache_interface *m_sha256_password_cache;
};

}  // namespace xpl

#endif  // _XPL_NATIVE_PLAIN_VERIFICATION_H_
