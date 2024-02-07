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

#ifndef PLUGIN_X_SRC_INTERFACE_ACCOUNT_VERIFICATION_H_
#define PLUGIN_X_SRC_INTERFACE_ACCOUNT_VERIFICATION_H_

#include <string>

namespace xpl {
namespace iface {

class Account_verification {
 public:
  enum class Account_type {
    k_native = 1,
    k_sha256 = 2,
    k_sha2 = 3,
    k_sha256_memory = 4,
    k_unsupported = 99
  };

  Account_verification() = default;
  Account_verification(const Account_verification &) = delete;
  Account_verification &operator=(const Account_verification &) = delete;

  virtual const std::string &get_salt() const = 0;
  virtual bool verify_authentication_string(
      const std::string &user, const std::string &host,
      const std::string &client_string, const std::string &db_string) const = 0;
  virtual ~Account_verification() = default;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_ACCOUNT_VERIFICATION_H_
