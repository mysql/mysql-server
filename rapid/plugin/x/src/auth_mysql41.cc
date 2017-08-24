/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/auth_mysql41.h"

#include "plugin/x/src/native_verification.h"

// C -> S: authenticationStart(MYSQL41)
// S -> C: authenticationContinue(20 byte salt/scramble)
// C -> S: authenticationContinue(schema\0user\0sha1(sha1(password))+salt)
// S -> C: Notice(password expired etc)
// S -> C: authenticationOk/Error

namespace xpl {

ngs::Authentication_interface_ptr Sasl_mysql41_auth::create(
    ngs::Session_interface *session) {
  Account_verification_handler *handler =
      ngs::allocate_object<Account_verification_handler>(
          session, ngs::Account_verification_interface::Account_native,
          ngs::allocate_object<Native_verification>());
  return ngs::Authentication_interface_ptr(
      ngs::allocate_object<Sasl_mysql41_auth>(handler));
}

ngs::Authentication_interface::Response Sasl_mysql41_auth::handle_start(
    const std::string&, const std::string&,
    const std::string&) {
  if (m_state != S_starting) {
    m_state = S_error;
    return {Error, ER_NET_PACKETS_OUT_OF_ORDER};
  }

  const ngs::Account_verification_interface *verificator =
      m_verification_handler->get_account_verificator(
          ngs::Account_verification_interface::Account_native);
  DBUG_ASSERT(verificator);
  m_state = S_waiting_response;
  return {Ongoing, 0, verificator->get_salt()};
}

ngs::Authentication_interface::Response Sasl_mysql41_auth::handle_continue(
    const std::string &data) {
  if (m_state != S_waiting_response) {
    m_state = S_error;
    return {Error, ER_NET_PACKETS_OUT_OF_ORDER};
  }

  m_state = S_done;
  if (ngs::Error_code error = m_verification_handler->authenticate(*this, data))
    return {Failed, error.error, error.message};
  return {Succeeded};
}

ngs::Error_code Sasl_mysql41_auth::authenticate_account(
    const std::string &user, const std::string &host,
    const std::string &passwd) const {
  return m_verification_handler->verify_account(user, host, passwd);
}

}  // namespace xpl
