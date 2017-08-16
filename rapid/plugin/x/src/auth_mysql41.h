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

#ifndef _AUTH_MYSQL41_H_
#define _AUTH_MYSQL41_H_

// mysql_native_password (MySQL 4.1) authentication

#include "plugin/x/ngs/include/ngs/interface/authentication_interface.h"
#include "plugin/x/src/account_verification_handler.h"


namespace xpl {

class Sasl_mysql41_auth : public ngs::Authentication_interface {
 public:
  explicit Sasl_mysql41_auth(Account_verification_handler *handler)
      : m_verification_handler(handler), m_state(S_starting) {}

  static ngs::Authentication_interface_ptr create(
      ngs::Session_interface *session);

  Response handle_start(const std::string &mechanism,
                                const std::string &data,
                                const std::string &initial_response) override;

  Response handle_continue(const std::string &data) override;

  ngs::Error_code authenticate_account(const std::string &user,
                                       const std::string &host,
                                       const std::string &passwd) const
      override;

 private:
  Account_verification_handler_ptr m_verification_handler;

  enum State {
    S_starting,
    S_waiting_response,
    S_done,
    S_error
  } m_state;
};

}  // namespace xpl

#endif
