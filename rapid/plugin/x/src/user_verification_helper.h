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

#ifndef XPL_USER_VERIFICATION_HELPER_H_
#define XPL_USER_VERIFICATION_HELPER_H_

#include "ngs_common/connection_type.h"

#include "xpl_log.h"
#include "sql_data_context.h"
#include "sql_user_require.h"
#include "query_string_builder.h"


namespace xpl {

  class User_verification_helper {
  public:
    typedef Command_delegate::Field_types         Field_types;
    typedef Sql_data_context::Result_info         Result_info;
    typedef Buffering_command_delegate::Resultset Resultset;

    User_verification_helper(
        const On_user_password_hash &hash_verification_cb,
        ngs::IOptions_session_ptr &options_session,
        const ngs::Connection_type type)
    : m_hash_verification_cb(hash_verification_cb),
      m_options_session(options_session),
      m_type(type) {
    }

    ngs::Error_code verify_mysql_account(Sql_data_context &sql_data_context, const std::string &user, const std::string &host) {
      Resultset   r_result_set;
      Result_info r_info;

      ngs::PFS_string query = get_sql(user.c_str(), host.c_str());
      ngs::Error_code error = sql_data_context.execute_sql_and_collect_results(
          query.c_str(),
          query.length(),
          m_fields_type,
          r_result_set,
          r_info);

      if (error) {
        log_debug("Error %i occurred while executing query: %s", error.error, error.message.c_str());
        return error;
      }

      try {
        // The query asks for primary key, thus here we should get
        // only one row
        if (!r_result_set.empty()) {
          DBUG_ASSERT(1 == r_result_set.size());
          if (this->verify_mysql_account_entry(r_result_set.front()))
            return ngs::Error_code();
        }
      }
      catch (ngs::Error_code &e)
      {
        return e;
      }

      return ngs::Error_code(ER_NO_SUCH_USER, "Invalid user or password");
    }

  private:
    ngs::PFS_string get_sql(const char *user, const char *host) const {
      Query_string_builder qb;

      // Query for a concrete users primary key (USER,HOST columns) which was chosen by MySQL Server
      // and verify hash and plugin column.
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
          "`ssl_type`, `ssl_cipher`, `x509_issuer`, `x509_subject` "
          "FROM mysql.user WHERE ")
            .quote_string(user).put(" = `user` AND ")
            .quote_string(host).put(" = `host` ");

      log_debug("Query user '%s'", qb.get().c_str());
      return qb.get();
    }

    bool verify_mysql_account_entry(const Row_data &row) {
      bool require_secure_transport = false;
      std::string db_password_hash;
      bool is_account_not_locked = false;
      bool is_password_expired = false;
      bool disconnect_on_expired_password = false;
      bool is_offline_mode_and_isnt_super_user = false;
      Sql_user_require required;

      DBUG_ASSERT(10 == row.fields.size());

      if (!get_bool_from_int_value(row, 0, require_secure_transport) ||
          !get_string_value(row, 1, db_password_hash) ||
          !get_bool_from_string_value(row, 2, "N", is_account_not_locked) ||
          !get_bool_from_int_value(row, 3, is_password_expired) ||
          !get_bool_from_int_value(row, 4, disconnect_on_expired_password) ||
          !get_bool_from_int_value(row, 5, is_offline_mode_and_isnt_super_user) ||
          !get_string_value(row, 6, required.ssl_type) ||
          !get_string_value(row, 7, required.ssl_cipher) ||
          !get_string_value(row, 8, required.ssl_x509_issuer) ||
          !get_string_value(row, 9, required.ssl_x509_subject))
        return false;

      if (m_hash_verification_cb(db_password_hash)) {
        // Password check succeeded but...

        if (!is_account_not_locked)
          throw ngs::Error_code(ER_ACCOUNT_HAS_BEEN_LOCKED, "Account is locked.");

        if (is_offline_mode_and_isnt_super_user)
          throw ngs::Error_code(ER_SERVER_OFFLINE_MODE, "Server works in offline mode.");

        // password expiration check must come last, because password expiration is not a fatal error
        // a client that supports expired password state, will be let in... so the user can only
        // get this error if the auth succeeded
        if (is_password_expired) {
          // if the password is expired, it's only a fatal error if disconnect_on_expired_password is enabled
          // AND the client doesn't support expired passwords (this check is done by the caller of this)
          // if it's NOT enabled, then the user will be allowed to login in sandbox mode, even if the client
          // doesn't support expired passwords
          if (disconnect_on_expired_password)
            throw ngs::Fatal(ER_MUST_CHANGE_PASSWORD_LOGIN, "Your password has expired. To log in you must change it using a client that supports expired passwords.");
          else
            throw ngs::Error(ER_MUST_CHANGE_PASSWORD_LOGIN, "Your password has expired.");
        }

        if (require_secure_transport) {
          if (!ngs::Connection_type_helper::is_secure_type(m_type))
            throw ngs::Error(ER_SECURE_TRANSPORT_REQUIRED, "Secure transport required. To log in you must use TCP+SSL or UNIX socket connection.");
        }

        ngs::Error_code error = required.validate(m_options_session);
        if (error)
          throw error;

        return true;
      }

      return false;
    }

    bool get_string_value(const Row_data &row, const std::size_t index, std::string &value) const {
      Field_value *field = row.fields[index];

      if (!field)
        return false;

      if (MYSQL_TYPE_STRING != m_fields_type[index].type &&
          MYSQL_TYPE_BLOB != m_fields_type[index].type)
        return false;

      value = *field->value.v_string;

      return true;
    }

    bool get_bool_from_string_value(
        const Row_data &row,
        const std::size_t index,
        const std::string &match,
        bool &value) const {
      std::string string_value;

      if (get_string_value(row, index, string_value)) {
        value = string_value == match;

        return true;
      }

      return false;
    }

    bool get_bool_from_int_value(const Row_data &row, const std::size_t index, bool &value) const {
      std::string string_value;
      Field_value *field = row.fields[index];

      if (!field)
        return false;

      if (MYSQL_TYPE_LONGLONG != m_fields_type[index].type)
        return false;

      value = 0 != field->value.v_long;

      return true;
    }

    Field_types                m_fields_type;
    On_user_password_hash      m_hash_verification_cb;
    ngs::IOptions_session_ptr &m_options_session;
    ngs::Connection_type       m_type;
  };

} // namespace xpl

#endif // XPL_USER_VERIFICATION_HELPER_H_
