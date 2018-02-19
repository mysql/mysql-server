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

#ifndef _AUTH_CHALLENGE_RESPONSE_H_
#define _AUTH_CHALLENGE_RESPONSE_H_

#include <string>

#include "plugin/x/ngs/include/ngs/interface/authentication_interface.h"
#include "plugin/x/ngs/include/ngs/interface/sha256_password_cache_interface.h"
#include "plugin/x/src/account_verification_handler.h"
#include "plugin/x/src/cache_based_verification.h"
#include "plugin/x/src/native_verification.h"

namespace xpl {

template <ngs::Account_verification_interface::Account_type Auth_type,
          typename Auth_verificator_t>
class Sasl_challenge_response_auth;

using Sasl_mysql41_auth = Sasl_challenge_response_auth<
    ngs::Account_verification_interface::Account_native, Native_verification>;

using Sasl_sha256_memory_auth = Sasl_challenge_response_auth<
    ngs::Account_verification_interface::Account_sha256_memory,
    Cache_based_verification>;

/**
  Class used for performing challenge response authentication. It is responsible
  for the following sequence:

    'AUTH_METHOD' is either MYSQL41 or SHA256_MEMORY
    'HASH' is SHA1 in case of MYSQL41 and SHA256 in case of SHA256_MEMORY
    C -> S: authenticationStart(AUTH_METHOD)
    S -> C: authenticationContinue(20 byte salt/scramble)
    C -> S: authenticationContinue(schema\0user\0HASH(HASH(password))+salt)
    S -> C: Notice(password expired etc)
    S -> C: authenticationOk/Error

  @tparam Auth_type Enum representing account verification type
  @tparam Auth_verificator_t Class that will be used for performing verification
*/
template <ngs::Account_verification_interface::Account_type Auth_type,
          typename Auth_verificator_t>
class Sasl_challenge_response_auth : public ngs::Authentication_interface {
 public:
  explicit Sasl_challenge_response_auth(Account_verification_handler *handler)
      : m_verification_handler(handler), m_state(S_starting) {}

  static ngs::Authentication_interface_ptr create(
      ngs::Session_interface *session,
      ngs::SHA256_password_cache_interface *cache);

  Response handle_start(const std::string &, const std::string &,
                        const std::string &) override;

  Response handle_continue(const std::string &data) override;

  ngs::Error_code authenticate_account(
      const std::string &user, const std::string &host,
      const std::string &passwd) const override;

 private:
  Account_verification_handler_ptr m_verification_handler;

  enum State { S_starting, S_waiting_response, S_done, S_error } m_state;
};

/**
  Create authentication class instance

  @param[in] session Session instance that will be used by the account
  verificator
  @param[in] cache Pointer to password cache used during account verification

  @return Pointer to authentication class instance
*/
template <ngs::Account_verification_interface::Account_type Account_type,
          typename Auth_verificator_t>
ngs::Authentication_interface_ptr
Sasl_challenge_response_auth<Account_type, Auth_verificator_t>::create(
    ngs::Session_interface *session,
    ngs::SHA256_password_cache_interface *cache) {
  Account_verification_handler *handler =
      ngs::allocate_object<Account_verification_handler>(
          session, Account_type,
          ngs::allocate_object<Auth_verificator_t>(cache));
  return ngs::Authentication_interface_ptr(
      ngs::allocate_object<
          Sasl_challenge_response_auth<Account_type, Auth_verificator_t>>(
          handler));
}

/**
  First phase of an authentication - send salt to the client

  @return Response message containing salt or error if we are on other phase
  of authentication
*/
template <ngs::Account_verification_interface::Account_type Account_type,
          typename Auth_verificator_t>
ngs::Authentication_interface::Response
Sasl_challenge_response_auth<Account_type, Auth_verificator_t>::handle_start(
    const std::string &, const std::string &, const std::string &) {
  if (m_state != S_starting) {
    m_state = S_error;
    return {Error, ER_NET_PACKETS_OUT_OF_ORDER};
  }

  const ngs::Account_verification_interface *verificator =
      m_verification_handler->get_account_verificator(Account_type);
  DBUG_ASSERT(verificator);
  m_state = S_waiting_response;
  return {Ongoing, 0, verificator->get_salt()};
}

/**
  Second phase of an authentication - given the response from the client
  verify if he is credible to be successfully authenticated

  @param[in] data Clients response

  @return Authentication response message
    @retval Error When called on wrong phase of challenge response
    authentication
    @retval Succeeded On successful authentication
    @retval Failed On unsuccessful authentication
*/
template <ngs::Account_verification_interface::Account_type Account_type,
          typename Auth_verificator_t>
ngs::Authentication_interface::Response
Sasl_challenge_response_auth<Account_type, Auth_verificator_t>::handle_continue(
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

/**
  Authenicate user given his name, hostname and password

  @param[in] user User name
  @param[in] host Hostname
  @param[in] passwd Password provided by the user

  @return Result of user verification
*/
template <ngs::Account_verification_interface::Account_type Account_type,
          typename Auth_verificator_t>
ngs::Error_code Sasl_challenge_response_auth<Account_type, Auth_verificator_t>::
    authenticate_account(const std::string &user, const std::string &host,
                         const std::string &passwd) const {
  return m_verification_handler->verify_account(user, host, passwd);
}

}  // namespace xpl

#endif  // _AUTH_CHALLENGE_RESPONSE_H_
