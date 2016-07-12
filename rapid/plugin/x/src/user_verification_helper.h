/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _USER_VERIFICATION_HELPER_H_
#define _USER_VERIFICATION_HELPER_H_

#include "ngs_common/connection_type.h"

#include "sql_data_context.h"
#include "sql_user_require.h"
#include "query_string_builder.h"

namespace xpl
{

  class User_verification_helper
  {
  public:
    User_verification_helper(const On_user_password_hash &hash_verification_cb, const Command_delegate::Field_types &fields_type, const char *ip, ngs::IOptions_session_ptr &options_session, const ngs::Connection_type type)
    : m_hash_verification_cb(hash_verification_cb), m_fields_type(fields_type), m_users_ip(ip), m_options_session(options_session), m_type(type)
    {
    }

    bool operator() (const Row_data &row)
    {
      bool require_secure_transport;
      std::string db_user_hostname_or_ip_mask;
      std::string db_password_hash;
      bool is_account_not_locked = false;
      bool is_password_expired = false;
      bool disconnect_on_expired_password = false;
      bool is_offline_mode_and_isnt_super_user = false;
      Sql_user_require required;

      DBUG_ASSERT(11 == row.fields.size());

      if (!get_bool_from_int_value(row, 0, require_secure_transport) ||
          !get_string_value(row, 1, db_password_hash) ||
          !get_bool_from_string_value(row, 2, "N", is_account_not_locked) ||
          !get_bool_from_int_value(row, 3, is_password_expired) ||
          !get_bool_from_int_value(row, 4, disconnect_on_expired_password) ||
          !get_bool_from_int_value(row, 5, is_offline_mode_and_isnt_super_user) ||
          !get_string_value(row, 6, db_user_hostname_or_ip_mask) ||
          !get_string_value(row, 7, required.ssl_type) ||
          !get_string_value(row, 8, required.ssl_cipher) ||
          !get_string_value(row, 9, required.ssl_x509_issuer) ||
          !get_string_value(row, 10, required.ssl_x509_subject))
        return false;

      if (is_ip_and_ipmask(db_user_hostname_or_ip_mask))
      {
        if (!is_ip_matching_ipmask(m_users_ip, db_user_hostname_or_ip_mask))
          return false;
      }

      if (m_hash_verification_cb(db_password_hash))
      {
        // Password check succeeded but...

        if (!is_account_not_locked)
          throw ngs::Error_code(ER_ACCOUNT_HAS_BEEN_LOCKED, "Account is locked.");

        if (is_offline_mode_and_isnt_super_user)
          throw ngs::Error_code(ER_SERVER_OFFLINE_MODE, "Server works in offline mode.");

        // password expiration check must come last, because password expiration is not a fatal error
        // a client that supports expired password state, will be let in... so the user can only
        // get this error if the auth succeeded
        if (is_password_expired)
        {
          // if the password is expired, it's only a fatal error if disconnect_on_expired_password is enabled
          // AND the client doesn't support expired passwords (this check is done by the caller of this)
          // if it's NOT enabled, then the user will be allowed to login in sandbox mode, even if the client
          // doesn't support expired passwords
          if (disconnect_on_expired_password)
            throw ngs::Fatal(ER_MUST_CHANGE_PASSWORD_LOGIN, "Your password has expired. To log in you must change it using a client that supports expired passwords.");
          else
            throw ngs::Error(ER_MUST_CHANGE_PASSWORD_LOGIN, "Your password has expired.");
        }

        if (require_secure_transport)
        {
          if (!ngs::Connection_type_helper::is_secure_type(m_type))
            throw ngs::Error(ER_SECURE_TRANSPORT_REQUIRED, "Secure transport required. To log in you must use TCP+SSL or UNIX socket connection.");
        }

        ngs::Error_code error = required.validate(m_options_session);
        if (error)
          throw error;

        m_matched_host = db_user_hostname_or_ip_mask;
        return true;
      }

      return false;
    }

    std::string get_sql(const char *user, const char *host) const
    {
      Query_string_builder qb;

      // Query for a specific user, ACL entries are partially filtered by matching host. IP addresses with masks need to be matched at row processing
      // There are also other informations, like account lock, if password expired and user can be connected, if server is in offline mode and the user is with super priv.
      // column `is_password_expired` is set to true if
      //  - password expired
      // column `disconnect_on_expired_password` is set to true if
      //  - disconnect_on_expired_password
      // column `is_offline_mode_and_isnt_super_user` is set true if it
      //  - offline mode and user has not super priv
      qb.put("/* xplugin authentication */ SELECT @@require_secure_transport, `authentication_string`,`account_locked`, "
          "(`password_expired`!='N') as `is_password_expired`, "
          "@@disconnect_on_expired_password as `disconnect_on_expired_password`, "
          "@@offline_mode and (`Super_priv`='N') as `is_offline_mode_and_isnt_super_user`,"
          "`host`, `ssl_type`, `ssl_cipher`, `x509_issuer`, `x509_subject` ")
            .put("FROM mysql.user WHERE (").quote_string(m_users_ip).put(" LIKE `host` OR ")
            .quote_string(host ? std::string(host) : m_users_ip).put(" LIKE `host` OR `host`='' OR POSITION('/' in `host`)>0) and (_binary")
            .quote_string(user).put("= CONVERT(`user`, BINARY)) AND plugin = 'mysql_native_password'")
            .put(" ORDER BY length(host)<>0 DESC, POSITION('%' in `host`)=0 DESC, length(`user`) DESC;");

      return qb.get();
    }

  private:
    static bool calc_ip(const char *ip_arg, long &val, char end)
    {
      long ip_val, tmp;
      if (!(ip_arg=str2int(ip_arg,10,0,255,&ip_val)) || *ip_arg != '.')
        return false;

      ip_val<<=24;
      if (!(ip_arg=str2int(ip_arg+1,10,0,255,&tmp)) || *ip_arg != '.')
        return false;

      ip_val+=tmp<<16;
      if (!(ip_arg=str2int(ip_arg+1,10,0,255,&tmp)) || *ip_arg != '.')
        return false;

      ip_val+=tmp<<8;
      if (!(ip_arg=str2int(ip_arg+1,10,0,255,&tmp)) || *ip_arg != end)
        return false;

      val=ip_val+tmp;
      return true;
    }

    static bool is_ip_and_ipmask(const std::string &pattern)
    {
      // simple but effective check, host names doesn't allow this character
      return std::string::npos != pattern.find('/');
    }

    static bool is_address_valid(const std::string &ip, long &output)
    {
      if (0 == calc_ip(ip.c_str(), output, '\0'))
        return false;

      return true;
    }

    static bool is_ip_matching_ipmask(const std::string &ip, const std::string &expected_ipmask)
    {
      std::string expected_address_string;
      std::string expected_mask_string;
      std::stringstream stream(expected_ipmask);

      if (!std::getline(stream, expected_address_string, '/'))
        return false;

      if (!std::getline(stream, expected_mask_string, '/'))
        return false;

      long expected_mask;
      long expected_address;
      long ip_address;

      if (!is_address_valid(expected_address_string, expected_address) ||
          !is_address_valid(ip, ip_address) ||
          !is_address_valid(expected_mask_string, expected_mask))
        return false;

      unsigned long cidr_network_part = expected_mask & expected_address;
      unsigned long ip_network_part = expected_mask & ip_address;

      return ip_network_part == cidr_network_part;
    }

    bool get_string_value(const Row_data &row, const std::size_t index, std::string &value) const
    {
      Field_value *field = row.fields[index];

      if (!field)
        return false;

      if (MYSQL_TYPE_STRING != m_fields_type[index].type &&
          MYSQL_TYPE_BLOB != m_fields_type[index].type)
        return false;

      value = *field->value.v_string;

      return true;
    }

    bool get_bool_from_string_value(const Row_data &row, const std::size_t index, const std::string &match, bool &value) const
    {
      std::string string_value;

      if (get_string_value(row, index, string_value))
      {
        value = string_value == match;

        return true;
      }

      return false;
    }

    bool get_bool_from_int_value(const Row_data &row, const std::size_t index, bool &value) const
    {
      std::string string_value;
      Field_value *field = row.fields[index];

      if (!field)
        return false;

      if (MYSQL_TYPE_LONGLONG != m_fields_type[index].type)
        return false;

      value = 0 != field->value.v_long;

      return true;
    }

    On_user_password_hash                m_hash_verification_cb;
    const Command_delegate::Field_types &m_fields_type;
    std::string                          m_users_ip;
    std::string                          m_matched_host;
    ngs::IOptions_session_ptr            &m_options_session;
    ngs::Connection_type                 m_type;
  };

} // namespace xpl

#endif // _USER_VERIFICATION_HELPER_H_
