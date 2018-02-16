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

#ifndef _NGS_ACCOUNT_VERIFICATION_INTERFACE_H_
#define _NGS_ACCOUNT_VERIFICATION_INTERFACE_H_

#include <string>

#include "plugin/x/ngs/include/ngs/memory.h"

namespace ngs {

class Account_verification_interface {
 public:
  enum Account_type {
    Account_native = 1,
    Account_sha256 = 2,
    Account_sha2 = 3,
    Account_sha256_memory = 4,
    Account_unsupported = 99
  };

  Account_verification_interface() {}
  Account_verification_interface(const Account_verification_interface &) =
      delete;
  Account_verification_interface &operator=(
      const Account_verification_interface &) = delete;

  virtual const std::string &get_salt() const = 0;
  virtual bool verify_authentication_string(
      const std::string &user, const std::string &host,
      const std::string &client_string, const std::string &db_string) const = 0;
  virtual ~Account_verification_interface() {}
};

typedef ngs::Memory_instrumented<Account_verification_interface>::Unique_ptr
    Account_verification_interface_ptr;

}  // namespace ngs

#endif  // _NGS_ACCOUNT_VERIFICATION_INTERFACE_H_
