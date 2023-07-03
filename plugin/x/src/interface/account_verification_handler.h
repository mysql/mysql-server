/*
 * Copyright (c) 2020, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_INTERFACE_ACCOUNT_VERIFICATION_HANDLER_H_
#define PLUGIN_X_SRC_INTERFACE_ACCOUNT_VERIFICATION_HANDLER_H_

#include <string>

#include "plugin/x/src/interface/account_verification.h"
#include "plugin/x/src/interface/authentication.h"
#include "plugin/x/src/ngs/error_code.h"

namespace xpl {
namespace iface {

class Account_verification_handler {
 public:
  virtual ~Account_verification_handler() = default;

  virtual ngs::Error_code authenticate(
      const iface::Authentication &account_verificator,
      iface::Authentication_info *authenication_info,
      const std::string &sasl_message) const = 0;

  virtual const iface::Account_verification *get_account_verificator(
      const iface::Account_verification::Account_type account_type) const = 0;

  virtual ngs::Error_code verify_account(
      const std::string &user, const std::string &host,
      const std::string &passwd,
      const iface::Authentication_info *authenication_info) const = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_ACCOUNT_VERIFICATION_HANDLER_H_
