/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#include <mysql/components/my_service.h>
#include <mysql/components/services/mysql_global_variable_attributes_service.h>
#include "my_sys.h"  // NOLINT(build/include_subdir)

#include "plugin/x/src/client.h"
#include "plugin/x/src/interface/sql_session.h"
#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/sql_data_result.h"
#include "plugin/x/src/ssl_session_options.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

bool Account_verification_handler::parse_sasl_message(
    const std::string &sasl_message,
    iface::Authentication_info *out_authenication_info, std::string *out_schema,
    std::string *out_account, std::string *out_passwd) {
  std::size_t message_position = 0;

  *out_schema = "";
  *out_account = "";
  *out_passwd = "";

  if (sasl_message.empty() ||
      !extract_sub_message(sasl_message, message_position, *out_schema) ||
      !extract_sub_message(sasl_message, message_position, *out_account) ||
      !extract_last_sub_message(sasl_message, message_position, *out_passwd))
    return false;

  out_authenication_info->m_tried_account_name = *out_account;
  out_authenication_info->m_was_using_password = !out_passwd->empty();

  return true;
}

ngs::Error_code Account_verification_handler::authenticate(
    const iface::Authentication &account_verificator,
    iface::Authentication_info *authentication_info,
    const std::string &sasl_message) const {
  std::string schema = "";
  std::string account = "";
  std::string passwd = "";

  if (!parse_sasl_message(sasl_message, authentication_info, &schema, &account,
                          &passwd))
    return ngs::SQLError_access_denied();

  if (account.empty()) return ngs::SQLError_access_denied();

  auto &sql_context = m_session->data_context();
  const auto allow_expired = m_session->client().supports_expired_passwords();

  const auto result = sql_context.authenticate(
      account.c_str(), m_session->client().client_hostname(),
      m_session->client().client_address(), schema.c_str(), passwd,
      account_verificator, allow_expired);

  if (0 == result.error && sql_context.password_expired())
    m_session->proto().send_notice_account_expired();

  return result;
}

bool Account_verification_handler::extract_last_sub_message(
    const std::string &message, std::size_t &element_position,
    std::string &sub_message) {
  if (element_position >= message.size()) return true;

  sub_message = message.substr(element_position);
  element_position = std::string::npos;

  return true;
}

bool Account_verification_handler::extract_sub_message(
    const std::string &message, std::size_t &element_position,
    std::string &sub_message) {
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

const iface::Account_verification *
Account_verification_handler::get_account_verificator(
    const iface::Account_verification::Account_type account_type) const {
  Account_verificator_list::const_iterator i =
      m_verificators.find(account_type);
  return i == m_verificators.end() ? nullptr : i->second.get();
}

iface::Account_verification::Account_type
Account_verification_handler::get_account_verificator_id(
    const std::string &name) const {
  if (name == "mysql_native_password")
    return iface::Account_verification::Account_type::k_native;
  if (name == "sha256_password")
    return iface::Account_verification::Account_type::k_sha256;
  if (name == "caching_sha2_password")
    return iface::Account_verification::Account_type::k_sha2;
  return iface::Account_verification::Account_type::k_unsupported;
}

ngs::Error_code Account_verification_handler::verify_account(
    const std::string &user, const std::string &host, const std::string &passwd,
    const iface::Authentication_info *authenication_info) const {
  Account_record record;
  if (ngs::Error_code error = get_account_record(user, host, record))
    return error;

  iface::Account_verification::Account_type account_verificator_id;
  // If SHA256_MEMORY is used then no matter what auth_plugin is used we
  // will be using cache-based verification
  if (m_account_type ==
      iface::Account_verification::Account_type::k_sha256_memory) {
    account_verificator_id =
        iface::Account_verification::Account_type::k_sha256_memory;
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
    return get_offline_mode_error();

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
      !Connection_type_helper::is_secure_type(
          m_session->client().connection().get_type()))
    return ngs::SQLError(ER_SECURE_TRANSPORT_REQUIRED);

  return record.user_required.validate(
      Ssl_session_options(&m_session->client().connection()));
}

ngs::Error_code Account_verification_handler::get_account_record(
    const std::string &user, const std::string &host,
    Account_record &record) const try {
  Sql_data_result result(&m_session->data_context());
  result.query(get_sql(user, host));
  // The query asks for primary key, thus here we should get only one row
  if (result.size() != 1)
    return ngs::Error_code(ER_NO_SUCH_USER, "Invalid user or password");
  result.get(&record.require_secure_transport, &record.db_password_hash,
             &record.auth_plugin_name, &record.is_account_locked,
             &record.is_password_expired,
             &record.disconnect_on_expired_password,
             &record.is_offline_mode_and_not_super_user,
             &record.user_required.ssl_type, &record.user_required.ssl_cipher,
             &record.user_required.ssl_x509_issuer,
             &record.user_required.ssl_x509_subject);

  if (result.is_server_status_set(SERVER_STATUS_IN_TRANS))
    result.query("COMMIT");

  return ngs::Success();
} catch (const ngs::Error_code &e) {
  return e;
}

ngs::Error_code Account_verification_handler::get_offline_mode_error() const {
  char attr_value[1024] = "";
  size_t len_attr = sizeof(attr_value);

  char user_value[USERNAME_CHAR_LENGTH + 1] = "";
  size_t len_user = sizeof(user_value);

  char time_value[30] = "";
  size_t len_time = sizeof(time_value);

  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();
  // protection needed for plugin unit tests
  if (plugin_registry != nullptr) {
    my_service<SERVICE_TYPE(mysql_global_variable_attributes)> service{
        "mysql_global_variable_attributes", plugin_registry};
    if (service.is_valid()) {
      service->get(nullptr, "offline_mode", "reason", attr_value, &len_attr);
      service->get_time(nullptr, "offline_mode", time_value, &len_time);
      service->get_user(nullptr, "offline_mode", user_value, &len_user);
    }
    mysql_plugin_registry_release(plugin_registry);
  }

  if (attr_value[0] != '\0') {
    return ngs::SQLError(ER_SERVER_OFFLINE_MODE_REASON, time_value, attr_value);
  }

  if (user_value[0] != '\0') {
    return ngs::SQLError(ER_SERVER_OFFLINE_MODE_USER, time_value, user_value);
  }

  return ngs::SQLError(ER_SERVER_OFFLINE_MODE);
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
        "/* xplugin authentication */ "
        "SELECT /*+ SET_VAR(SQL_MODE = 'TRADITIONAL') */ "
        "@@require_secure_transport, `authentication_string`, `plugin`, "
        "(`account_locked`='Y') as is_account_locked, "
        "(`password_expired`!='N') as `is_password_expired`, "
        "@@disconnect_on_expired_password as "
        "`disconnect_on_expired_password`, "
        "@@offline_mode and (`Super_priv`='N') as "
        "`is_offline_mode_and_not_super_user`, "
        "`ssl_type`, `ssl_cipher`, `x509_issuer`, `x509_subject` "
        "FROM mysql.user WHERE ")
      .quote_string(user)
      .put(" = `user` AND ")
      .quote_string(host)
      .put(" = `host`");

  log_debug("Query user '%s'", qb.get().c_str());
  return qb.get();
}

}  // namespace xpl
