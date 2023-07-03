/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */


#ifndef _AUTH_MYSQL41_H_
#define _AUTH_MYSQL41_H_

// mysql_native_password (MySQL 4.1) authentication

#include "ngs/protocol_encoder.h"
#include "ngs/protocol_authentication.h"
#include "xpl_client.h"
#include "xpl_session.h"
#include "xpl_server.h"
#include "xpl_log.h"


namespace xpl
{
  class Sasl_mysql41_auth : public ngs::Authentication_handler
  {
  public:

    static ngs::Authentication_handler_ptr create(ngs::Session_interface *session)
    {
      return Authentication_handler::wrap_ptr(new Sasl_mysql41_auth((xpl::Session*)session));
    }

    virtual Response handle_start(const std::string &mechanism,
                                  const std::string &data,
                                  const std::string &initial_response);

    virtual Response handle_continue(const std::string &data);

    virtual void done()
    {
      delete this;
    }

  protected:
    Sasl_mysql41_auth(xpl::Session *session) : m_session(session), m_state(S_starting) {}

  private:
    xpl::Session *m_session;
    std::string   m_salt;

    enum State {
      S_starting,
      S_waiting_response,
      S_done,
      S_error
    } m_state;

    ngs::Error_code sasl_message(const char *client_hostname, const char *client_address, const std::string &message);
    bool check_password_hash(const std::string &password_scramble, const std::string &password_hash);
  };
  
}  // namespace xpl


#endif
