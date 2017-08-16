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

#ifndef _NGS_ACCOUNT_VERIFICATION_INTERFACE_H_
#define _NGS_ACCOUNT_VERIFICATION_INTERFACE_H_

#include <string>
#include "ngs/memory.h"

namespace ngs {

class Account_verification_interface {
 public:
  enum Account_type {
    Account_native = 1,
    Account_sha256 = 2,
    Account_sha2   = 3,
    Account_unsupported = 99
  };

  Account_verification_interface() {}
  Account_verification_interface(const Account_verification_interface &) =
      delete;
  Account_verification_interface &operator=(
      const Account_verification_interface &) = delete;

  virtual const std::string &get_salt() const = 0;
  virtual bool verify_authentication_string(
      const std::string &client_string, const std::string &db_string) const = 0;
  virtual ~Account_verification_interface() {}
};

typedef ngs::Memory_instrumented<Account_verification_interface>::Unique_ptr
    Account_verification_interface_ptr;

}  // namespace ngs

#endif  // _NGS_ACCOUNT_VERIFICATION_INTERFACE_H_
