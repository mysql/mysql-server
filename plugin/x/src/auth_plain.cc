/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/auth_plain.h"

#include "plugin/x/src/native_plain_verification.h"
#include "plugin/x/src/sha256_plain_verification.h"
#include "plugin/x/src/sha2_plain_verification.h"

namespace xpl {

ngs::Authentication_interface_ptr Sasl_plain_auth::create(
    ngs::Session_interface *session,
    ngs::SHA256_password_cache_interface *sha256_password_cache) {
  auto handler = ngs::allocate_object<Account_verification_handler>(session);

  handler->add_account_verificator(
      ngs::Account_verification_interface::Account_native,
      ngs::allocate_object<Native_plain_verification>(sha256_password_cache));
  handler->add_account_verificator(
      ngs::Account_verification_interface::Account_sha256,
      ngs::allocate_object<Sha256_plain_verification>(sha256_password_cache));
  handler->add_account_verificator(
      ngs::Account_verification_interface::Account_sha2,
      ngs::allocate_object<Sha2_plain_verification>(sha256_password_cache));

  return ngs::Authentication_interface_ptr(
      ngs::allocate_object<Sasl_plain_auth>(handler));
}

Sasl_plain_auth::Response Sasl_plain_auth::handle_start(const std::string &,
                                                        const std::string &data,
                                                        const std::string &) {
  m_auth_info.reset();

  if (ngs::Error_code error =
          m_verification_handler->authenticate(*this, &m_auth_info, data))
    return {Failed, error.error, error.message};
  return {Succeeded};
}

Sasl_plain_auth::Response Sasl_plain_auth::handle_continue(
    const std::string &) {
  // never supposed to get called
  return {Error, ER_NET_PACKETS_OUT_OF_ORDER};
}

ngs::Error_code Sasl_plain_auth::authenticate_account(
    const std::string &user, const std::string &host,
    const std::string &passwd) const {
  return m_verification_handler->verify_account(user, host, passwd,
                                                &m_auth_info);
}

}  // namespace xpl
