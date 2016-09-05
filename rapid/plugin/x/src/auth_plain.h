/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
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


#ifndef _AUTH_PLAIN_H_
#define _AUTH_PLAIN_H_


#include "ngs/protocol_encoder.h"
#include "ngs/protocol_authentication.h"
#include "xpl_session.h"
#include "xpl_server.h"
#include "sql_data_context.h"
#include "xpl_client.h"

#include "xpl_log.h"


namespace xpl
{


class Sasl_plain_auth : public ngs::Authentication_handler
{
public:
  static ngs::Authentication_handler_ptr create(ngs::Session_interface *session)
  {
    return Authentication_handler::wrap_ptr(new Sasl_plain_auth((xpl::Session*)session));
  }

  virtual Response handle_start(const std::string &mechanism,
                                const std::string &data,
                                const std::string &initial_response)
  {
    Response        r;
    const char*     client_address = m_session->client().client_address();
    std::string     client_hostname = m_session->client().client_hostname();
    ngs::Error_code error = sasl_message(client_hostname.empty() ? NULL : client_hostname.c_str(), client_address, data);

    // data is the username and initial_response is password
    if (!error)
    {
      r.status = Succeeded;
      r.data = "";
      r.error_code = 0;
    }
    else
    {
      r.status = Failed;
      r.data = error.message;
      r.error_code = error.error;
    }

    return r;
  }

  virtual Response handle_continue(const std::string &data)
  {
    // never supposed to get called
    Response r;
    r.status = Error;
    r.error_code = ER_NET_PACKETS_OUT_OF_ORDER;
    return r;
  }

  virtual void done()
  {
    delete this;
  }

protected:
  Sasl_plain_auth(xpl::Session *session) : m_session(session) {}

private:
  xpl::Session *m_session;

  ngs::Error_code sasl_message(const char *client_hostname, const char *client_address, const std::string &message)
  {
    try
    {
      const std::size_t  sasl_element_max_with_two_additional_bytes = 256;
      std::size_t        message_position = 0;

      char authzid_db[sasl_element_max_with_two_additional_bytes];
      char authcid[sasl_element_max_with_two_additional_bytes];
      char passwd[sasl_element_max_with_two_additional_bytes];

      if (!extract_null_terminated_element(message, message_position, sasl_element_max_with_two_additional_bytes, authzid_db) ||
          !extract_null_terminated_element(message, message_position, sasl_element_max_with_two_additional_bytes, authcid) ||
          !extract_null_terminated_element(message, message_position, sasl_element_max_with_two_additional_bytes, passwd))
      {
//        throw ngs::Error_code(ER_INVALID_CHARACTER_STRING, "Invalid format of login string");
        throw ngs::Error_code(ER_NO_SUCH_USER, "Invalid user or password");
      }

      if (strlen(authcid) == 0)
        throw ngs::Error_code(ER_NO_SUCH_USER, "Invalid user or password");
      std::string password_hash = *passwd ? compute_password_hash(passwd) : "";
      On_user_password_hash      check_password_hash = ngs::bind(&Sasl_plain_auth::compare_hashes, this, password_hash, ngs::placeholders::_1);
      ngs::IOptions_session_ptr  options_session = m_session->client().connection().options();
      const ngs::Connection_type connection_type = m_session->client().connection().connection_type();

      return m_session->data_context().authenticate(authcid, client_hostname, client_address, authzid_db, check_password_hash,
                                                    ((xpl::Client&)m_session->client()).supports_expired_passwords(), options_session, connection_type);
    }
    catch(const ngs::Error_code &error_code)
    {
      return error_code;
    }
    return ngs::Error_code();
  }

  bool compare_hashes(const std::string &user_password_hash, const std::string &db_password_hash)
  {
    const bool result = (user_password_hash == db_password_hash);
    return result;
  }
};

}  // namespace xpl


#endif // _AUTH_PLAIN_H_
