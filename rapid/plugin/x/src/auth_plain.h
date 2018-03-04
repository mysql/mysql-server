/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _XPL_AUTH_PLAIN_H_
#define _XPL_AUTH_PLAIN_H_

#include <string>

#include "plugin/x/ngs/include/ngs/interface/authentication_interface.h"
#include "plugin/x/ngs/include/ngs/interface/sha256_password_cache_interface.h"
#include "plugin/x/src/account_verification_handler.h"

namespace xpl {

class Sasl_plain_auth : public ngs::Authentication_interface {
 public:
  explicit Sasl_plain_auth(Account_verification_handler *handler)
      : m_verification_handler(handler) {}

  static ngs::Authentication_interface_ptr create(
      ngs::Session_interface *session,
      ngs::SHA256_password_cache_interface *sha256_password_cache);

  Response handle_start(const std::string &mechanism, const std::string &data,
                        const std::string &initial_response) override;

  Response handle_continue(const std::string &data) override;

  ngs::Error_code authenticate_account(const std::string &user,
      const std::string &host, const std::string &passwd) const override;

  std::string get_auth_name() { return "PLAIN"; }

 private:
  Account_verification_handler_ptr m_verification_handler;
};

}  // namespace xpl

#endif  // _XPL_AUTH_PLAIN_H_
