/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/account_verification_handler.h"

#include "my_sys.h"

#include "plugin/x/ngs/include/ngs/interface/sql_session_interface.h"
#include "plugin/x/ngs/include/ngs_common/ssl_session_options.h"
#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/sql_data_result.h"
#include "plugin/x/src/xpl_client.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

ngs::Error_code Account_verification_handler::authenticate(
    const ngs::Authentication_interface &account_verificator,
    ngs::Authentication_info *authenication_info,
    const std::string &sasl_message) const {
  std::size_t message_position = 0;
  std::string schema = "";
  std::string account = "";
  std::string passwd = "";
  if (sasl_message.empty() ||
      !extract_sub_message(sasl_message, message_position, schema) ||
      !extract_sub_message(sasl_message, message_position, account) ||
      !extract_last_sub_message(sasl_message, message_position, passwd))
    return ngs::SQLError_access_denied();

  authenication_info->m_tried_account_name = account;
  authenication_info->m_was_using_password = !passwd.empty();

  if (account.empty()) return ngs::SQLError_access_denied();

  return m_session->data_context().authenticate(
      account.c_str(), m_session->client().client_hostname(),
      m_session->client().client_address(), schema.c_str(), passwd,
      account_verificator, m_session->client().supports_expired_passwords());
}

bool Account_verification_handler::extract_last_sub_message(
    const std::string &message, std::size_t &element_position,
    std::string &sub_message) const {
  if (element_position >= message.size()) return true;

  sub_message = message.substr(element_position);
  element_position = std::string::npos;

  return true;
}

bool Account_verification_handler::extract_sub_message(
    const std::string &message, std::size_t &element_position,
    std::string &sub_message) const {
  if (element_position >= message.size()) return true;

  if (message[element_position] == '\0') {
    ++element_position;
    sub_message.clear();
    return true;
  }

  std::string::size_type last_character_of_element =
      message.find('\0', element_position);
  sub_message = message.substr(element_position, last_character_of_element);
  element_position = last_character_of_element;
  if (element_position != std::string::npos)
    ++element_position;
  else
    return false;
  return true;
}

const ngs::Account_verification_interface *
Account_verification_handler::get_account_verificator(
    const ngs::Account_verification_interface::Account_type account_type)
    const {
  Account_verificator_list::const_iterator i =
      m_verificators.find(account_type);
  return i == m_verificators.end() ? nullptr : i->second.get();
}

ngs::Account_verification_interface::Account_type
Account_verification_handler::get_account_verificator_id(
    const std::string &name) const {
  if (name == "mysql_native_password")
    return ngs::Account_verification_interface::Account_native;
  if (name == "sha256_password")
    return ngs::Account_verification_interface::Account_type::Account_sha256;
  if (name == "caching_sha2_password")
    return ngs::Account_verification_interface::Account_sha2;
  return ngs::Account_verification_interface::Account_unsupported;
}

ngs::Error_code Account_verification_handler::verify_account(
    const std::string &user, const std::string &host, const std::string &passwd,
    const ngs::Authentication_info *authenication_info) const {
  Account_record record;
  if (ngs::Error_code error = get_account_record(user, host, record))
    return error;

  ngs::Account_verification_interface::Account_type account_verificator_id;
  // If SHA256_MEMORY is used then no matter what auth_plugin is used we
  // will be using cache-based verification
  if (m_account_type == ngs::Account_verification_interface::Account_type::
                            Account_sha256_memory) {
    account_verificator_id = ngs::Account_verification_interface::Account_type::
        Account_sha256_memory;
  } else {
    account_verificator_id =
        get_account_verificator_id(record.auth_plugin_name);
  }
  auto *p = get_account_verificator(account_verificator_id);

  // password check
  if (!p || !p->verify_authentication_string(user, host, passwd,
                                             record.db_password_hash))
    return ngs::SQLError_access_denied();

  // password check succeeded but...
  if (record.is_account_locked) {
    return ngs::SQLError(ER_ACCOUNT_HAS_BEEN_LOCKED,
                         authenication_info->m_tried_account_name.c_str(),
                         m_session->client().client_hostname_or_address());
  }

  if (record.is_offline_mode_and_not_super_user)
    return ngs::SQLError(ER_SERVER_OFFLINE_MODE);

  // password expiration check must come last, because password expiration
  // is not a fatal error, a client that supports expired password state,
  // will be let in... so the user can only  get this error if the auth
  // succeeded
  if (record.is_password_expired) {
    // if the password is expired, it's only a fatal error if
    // disconnect_on_expired_password is enabled AND the client doesn't support
    // expired passwords (this check is done by the caller of this)
    // if it's NOT enabled, then the user will be allowed to login in
    // sandbox mode, even if the client doesn't support expired passwords
    auto result = ngs::SQLError(ER_MUST_CHANGE_PASSWORD_LOGIN);
    return record.disconnect_on_expired_password ? ngs::Fatal(result) : result;
  }

  if (record.require_secure_transport &&
      !ngs::Connection_type_helper::is_secure_type(
          m_session->client().connection().get_type()))
    return ngs::SQLError(ER_SECURE_TRANSPORT_REQUIRED);

  return record.user_required.validate(
      ngs::Ssl_session_options(&m_session->client().connection()));
}

ngs::Error_code Account_verification_handler::get_account_record(
    const std::string &user, const std::string &host,
    Account_record &record) const try {
  xpl::Sql_data_result result(m_session->data_context());
  result.query(get_sql(user, host));
  // The query asks for primary key, thus here we should get only one row
  if (result.size() != 1)
    return ngs::Error_code(ER_NO_SUCH_USER, "Invalid user or password");
  result.get(record.require_secure_transport)
      .get(record.db_password_hash)
      .get(record.auth_plugin_name)
      .get(record.is_account_locked)
      .get(record.is_password_expired)
      .get(record.disconnect_on_expired_password)
      .get(record.is_offline_mode_and_not_super_user)
      .get(record.user_required.ssl_type)
      .get(record.user_required.ssl_cipher)
      .get(record.user_required.ssl_x509_issuer)
      .get(record.user_required.ssl_x509_subject);
  return ngs::Success();
} catch (const ngs::Error_code &e) {
  return e;
}

ngs::PFS_string Account_verification_handler::get_sql(
    const std::string &user, const std::string &host) const {
  Query_string_builder qb;

  // Query for a concrete users primary key (USER,HOST columns) which was
  // chosen by MySQL Server and verify hash and plugin column.
  // There are also other informations, like account lock, if password expired
  // and user can be connected, if server is in offline mode and the user is
  // with super priv.
  // column `is_password_expired` is set to true if - password expired
  // column `disconnect_on_expired_password` is set to true if
  //  - disconnect_on_expired_password
  // column `is_offline_mode_and_not_super_user` is set true if it
  //  - offline mode and user has not super priv
  qb.put(
        "/* xplugin authentication */ SELECT @@require_secure_transport, "
        "`authentication_string`, `plugin`,"
        "(`account_locked`='Y') as is_account_locked, "
        "(`password_expired`!='N') as `is_password_expired`, "
        "@@disconnect_on_expired_password as "
        "`disconnect_on_expired_password`, "
        "@@offline_mode and (`Super_priv`='N') as "
        "`is_offline_mode_and_not_super_user`,"
        "`ssl_type`, `ssl_cipher`, `x509_issuer`, `x509_subject` "
        "FROM mysql.user WHERE ")
      .quote_string(user)
      .put(" = `user` AND ")
      .quote_string(host)
      .put(" = `host` ");

  log_debug("Query user '%s'", qb.get().c_str());
  return qb.get();
}

}  // namespace xpl
