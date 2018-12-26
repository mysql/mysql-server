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


#include "auth_mysql41.h"
#include "sql_data_context.h"

#include "mysql_com.h"
#include "crypt_genhash_impl.h"
#include "password.h"

// C -> S: authenticationStart(MYSQL41)
// S -> C: authenticationContinue(20 byte salt/scramble)
// C -> S: authenticationContinue(schema\0user\0sha1(sha1(password))+salt)
// S -> C: Notice(password expired etc)
// S -> C: authenticationOk/Error

using namespace xpl;


ngs::Authentication_handler::Response Sasl_mysql41_auth::handle_start(const std::string &mechanism,
                                                                      const std::string &data,
                                                                      const std::string &initial_response)
{
  Response r;

  if (m_state == S_starting)
  {
    m_salt.resize(SCRAMBLE_LENGTH);
    ::generate_user_salt(&m_salt[0], static_cast<int>(m_salt.size()));
    r.data = m_salt;
    r.status = Ongoing;
    r.error_code = 0;

    m_state = S_waiting_response;
  }
  else
  {
    r.status = Error;
    r.error_code = ER_NET_PACKETS_OUT_OF_ORDER;

    m_state = S_error;
  }

  return r;
}

ngs::Authentication_handler::Response Sasl_mysql41_auth::handle_continue(const std::string &data)
{
  Response r;

  if (m_state == S_waiting_response)
  {
    const char*     client_address  = m_session->client().client_address();
    std::string     client_hostname = m_session->client().client_hostname();
    ngs::Error_code error = sasl_message(client_hostname.empty() ? NULL : client_hostname.c_str(), client_address, data);

    // data is the username and initial_response is password
    if (!error)
    {
      r.status = Succeeded;
      r.error_code = 0;
    }
    else
    {
      r.status = Failed;
      r.data = error.message;
      r.error_code = error.error;
    }
    m_state = S_done;
  }
  else
  {
    m_state = S_error;
    r.status = Error;
    r.error_code = ER_NET_PACKETS_OUT_OF_ORDER;
  }
  return r;
}


ngs::Error_code Sasl_mysql41_auth::sasl_message(const char *client_hostname, const char *client_address, const std::string &message)
{
  try
  {
    const std::size_t sasl_element_max_with_two_additional_bytes = 256;
    std::size_t       message_position = 0;

    char authzid[sasl_element_max_with_two_additional_bytes];
    char authcid[sasl_element_max_with_two_additional_bytes];
    char passwd[sasl_element_max_with_two_additional_bytes];

    if (!extract_null_terminated_element(message, message_position, sasl_element_max_with_two_additional_bytes, authzid) ||
        !extract_null_terminated_element(message, message_position, sasl_element_max_with_two_additional_bytes, authcid) ||
        !extract_null_terminated_element(message, message_position, sasl_element_max_with_two_additional_bytes, passwd))
    {
      //throw ngs::Error_code(ER_INVALID_CHARACTER_STRING, "Invalid format of login string");
      throw ngs::Error_code(ER_NO_SUCH_USER, "Invalid user or password");
    }

    if (strlen(authcid) == 0)
      throw ngs::Error_code(ER_NO_SUCH_USER, "Invalid user or password");

    On_user_password_hash      verify_password_hash = ngs::bind(&Sasl_mysql41_auth::check_password_hash, this, passwd, ngs::placeholders::_1);
    ngs::IOptions_session_ptr  options_session = m_session->client().connection().options();
    const ngs::Connection_type connection_type = m_session->client().connection().connection_type();

    return m_session->data_context().authenticate(authcid, client_hostname, client_address, authzid, verify_password_hash,
                                                  ((xpl::Client&)m_session->client()).supports_expired_passwords(), options_session, connection_type);
  }
  catch(const ngs::Error_code &error_code)
  {
    return error_code;
  }
  return ngs::Error_code();
}

#include "mysql_com.h"

bool Sasl_mysql41_auth::check_password_hash(const std::string &password_scramble, const std::string &password_hash)
{
  try
  {
    if (password_scramble.empty())
    {
      // client gave no password, this can only login to a no password acct
      if (password_hash.empty())
        return true;
      return false;
    }
    if (!password_hash.empty())
    {
      uint8 db_hash_stage2[SCRAMBLE_LENGTH+1] = {0};
      uint8 user_hash_stage2[SCRAMBLE_LENGTH+1] = {0};

      ::get_salt_from_password(db_hash_stage2, password_hash.c_str());
      ::get_salt_from_password(user_hash_stage2, password_scramble.c_str());

      return 0 == ::check_scramble((const uchar*)user_hash_stage2, m_salt.c_str(), db_hash_stage2);
    }
    return false;
  }
  catch(const ngs::Error_code&)
  {
    return false;
  }
}
