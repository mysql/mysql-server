/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "auth_ldap_kerberos.h"

#include <cstring>

#include "krb5_interface.h"

namespace auth_ldap_sasl_client {

Kerberos::Kerberos()
    : m_initialized(false),
      m_destroy_tgt(false),
      m_context(nullptr),
      m_krb_credentials_cache(nullptr),
      m_credentials_created(false) {
  memset(&m_credentials, 0, sizeof(krb5_creds));
}

Kerberos::~Kerberos() { cleanup(); }

void Kerberos::get_ldap_host(std::string &host) {
  assert(m_initialized);
  host = m_ldap_server_host;
}

bool Kerberos::initialize() {
  krb5_error_code res_kerberos = 0;

  if (m_initialized) return true;

  if (!krb5.initialize()) return false;

  log_dbg("Kerberos setup starting.");
  if ((res_kerberos = krb5.krb5_init_context()(&m_context)) != 0) {
    log_error("Failed to initialize Kerberos context.");
    log(res_kerberos);
    goto EXIT;
  }
  m_initialized = true;
  log_dbg("Kerberos object initialized successfully.");

EXIT:
  if (!m_initialized && m_context) {
    krb5.krb5_free_context()(m_context);
    m_context = nullptr;
  }
  return m_initialized;
}

void Kerberos::cleanup() {
  if (m_credentials_created) {
    if (m_destroy_tgt) destroy_credentials();
    krb5.krb5_free_cred_contents()(m_context, &m_credentials);
    m_credentials_created = false;
  }

  close_default_cache();

  if (m_context) {
    krb5.krb5_free_context()(m_context);
    m_context = nullptr;
  }
  m_initialized = false;
}

void Kerberos::set_user_and_password(const char *user, const char *password) {
  assert(user);
  assert(password);

  m_user = user;
  m_password = password;
  auto pos = m_user.find('@');
  m_realm = pos == std::string::npos ? "" : std::string(m_user, pos + 1);
}

bool Kerberos::open_default_cache() {
  if (m_krb_credentials_cache != nullptr) return true;
  krb5_error_code res_kerberos =
      krb5.krb5_cc_default()(m_context, &m_krb_credentials_cache);
  if (res_kerberos) {
    log_error("Failed to open default Kerberos credentials cache.");
    log(res_kerberos);
    m_krb_credentials_cache = nullptr;
    return false;
  }
  log_dbg("Default Kerberos credentials cache opened.");
  return true;
}

void Kerberos::close_default_cache() {
  if (!m_krb_credentials_cache) return;
  krb5_error_code res_kerberos =
      krb5.krb5_cc_close()(m_context, m_krb_credentials_cache);
  if (res_kerberos) {
    log_error("Failed to close Kerberos credentials cache.");
    log(res_kerberos);
  }
  m_krb_credentials_cache = nullptr;
}

bool Kerberos::obtain_store_credentials() {
  krb5_error_code res_kerberos = 0;
  krb5_principal principal = nullptr;
  krb5_get_init_creds_opt *options = nullptr;
  bool success = false;

  if (!initialize()) return false;

  /*
    If valid credential exist, no need to obtain it again.
    This is done for the performance reason. Default expiry of TGT is 24 hours
    and this can be configured.
  */
  if (credentials_valid()) {
    log_info(
        "Existing Kerberos TGT is valid and will be used for authentication.");
    return true;
  }

  log_dbg("No valid Kerberos TGT exists.");

  /*
    Not attempting authentication as there are few security concern of active
    directory allowing users with empty password. End user may question this
    behaviors as security issue with MySQL. primary purpose of this kind of user
    is to search user DN's. User DN search using empty password users is allowed
    in server side plug-in.
  */
  if (m_user.empty() || m_password.empty()) {
    log_error("Cannot obtain Kerberos TGT: empty user name or password.");
    return false;
  }

  if (m_credentials_created) {
    log_info("Kerberos credentials already obtained.");
    return true;
  }

  log_dbg("Obtaining Kerberos credentials.");

  /*
    Full user name with domain name like, yashwant.sahu@oracle.com.
    Parsed principal will be used for authentication and to check if user is
    already authenticated.
  */
  principal = nullptr;
  memset(&principal, 0, sizeof(krb5_principal));
  res_kerberos = krb5.krb5_parse_name()(m_context, m_user.c_str(), &principal);
  if (res_kerberos) {
    log_error("Failed to parse user name.");
    goto EXIT;
  }

  /*
    Getting TGT from TGT server.
  */
  memset(&m_credentials, 0, sizeof(m_credentials));
  res_kerberos = krb5.krb5_get_init_creds_opt_alloc()(m_context, &options);
  if (res_kerberos) {
    log_error("Failed to create Kerberos options.");
    goto EXIT;
  }
  res_kerberos = krb5.krb5_get_init_creds_password()(
      m_context, &m_credentials, principal, m_password.c_str(), nullptr,
      nullptr, 0, nullptr, options);
  if (res_kerberos) {
    log_error("Failed to obtain Kerberos TGT.");
    goto EXIT;
  }
  m_credentials_created = true;

  /*
    Verifying TGT.
  */
  res_kerberos = krb5.krb5_verify_init_creds()(
      m_context, &m_credentials, nullptr, nullptr, nullptr, nullptr);
  if (res_kerberos) {
    log_error("Failed veryfying Kerberos TGT against the keytab.");
    goto EXIT;
  }

  log_info("Kerberos TGT obtained for '", m_user.c_str(), "'.");

  /*
    Store the credentials in the default cache. Types can be file, memory,
    keyring etc. Administrator should change default cache based on there
    preference.
  */

  log_dbg("Store Kerberos credentials starting.");

  /*
    Open and initialize credentials cache.
  */
  if (!open_default_cache()) goto EXIT;
  res_kerberos =
      krb5.krb5_cc_initialize()(m_context, m_krb_credentials_cache, principal);
  if (res_kerberos) {
    log_error("Failed to initialize credentials cache.");
    goto EXIT;
  }

  /*
    Store the credentials.
   */
  res_kerberos = krb5.krb5_cc_store_cred()(m_context, m_krb_credentials_cache,
                                           &m_credentials);
  if (res_kerberos) {
    log_error("Failed to store Kerberos credentials. ");
    log(res_kerberos);
    goto EXIT;
  }

  log_info("Kerberos credentials stored in the cache.");

  /*
    We need to close the cache to flush the credentials.
   */
  close_default_cache();

  success = true;

EXIT:
  if (res_kerberos) log(res_kerberos);

  if (principal) {
    krb5.krb5_free_principal()(m_context, principal);
    principal = nullptr;
  }
  if (options) {
    krb5.krb5_get_init_creds_opt_free()(m_context, options);
    options = nullptr;
  }
  return success;
}

void Kerberos::get_ldap_server_from_kdc() {
  assert(m_initialized);

  static const char realms_heading[] = "realms";
  static const char kdc_option[] = "kdc";

  krb5_error_code res_kerberos = 0;
  _profile_t *profile = nullptr;
  char *host_value = nullptr;

  res_kerberos = krb5.krb5_get_profile()(m_context, &profile);
  if (res_kerberos) {
    log_error("Failed to get Kerberos configuration profile.");
    return;
  }

  res_kerberos =
      krb5.profile_get_string()(profile, realms_heading, m_realm.c_str(),
                                kdc_option, nullptr, &host_value);
  if (res_kerberos || host_value == nullptr)
    log_warning("Failed to get LDAP server host as KDC from [realms] section.");
  else
    m_ldap_server_host = host_value;

  // Cleanup
  if (host_value) {
    m_ldap_server_host = host_value;
    krb5.profile_release_string()(host_value);
    host_value = nullptr;
  }
  if (profile) {
    krb5.profile_release()(profile);
    profile = nullptr;
  }
}

bool Kerberos::get_kerberos_config() {
  assert(m_initialized);

  static const char mysql_apps[] = "mysql";
  static const char ldap_host_option[] = "ldap_server_host";
  static const char ldap_destroy_option[] = "ldap_destroy_tgt";
  krb5_principal principal(nullptr);
  char *host_value = nullptr;
  bool result = true;

  log_dbg("Getting kerberos configuration.");
  m_ldap_server_host = "";

  /*
    1. Get ldap server host from [appdefaults] section, mysql application,
       ldap_server_host option.
    2. If 1. failed, get from [realms] section, current realm, kdc option.
    3. If 2. failed, return failure.
  */
  auto res_kerberos =
      krb5.krb5_parse_name()(m_context, m_user.c_str(), &principal);
  if (res_kerberos) {
    log_error("Failed to parse Kerberos client principal.");
    result = false;
    goto EXIT;
  }
  krb5.krb5_appdefault_string()(m_context, mysql_apps, &principal->realm,
                                ldap_host_option, "", &host_value);
  if (host_value == nullptr || host_value[0] == 0) {
    log_warning("Failed to get LDAP server host from [appdefaults] section.");
    get_ldap_server_from_kdc();
  } else
    m_ldap_server_host = host_value;

  if (m_ldap_server_host.empty()) {
    log_error("Failed to get LDAP server host");
    result = false;
    goto EXIT;
  }

  log_dbg("LDAP server host raw value: ", m_ldap_server_host.c_str());

  /* IPV6 */
  if (m_ldap_server_host[0] == '[') {
    auto pos = m_ldap_server_host.find("]");
    if (pos != m_ldap_server_host.npos &&
        (m_ldap_server_host.length() > (pos + 1)) &&
        (m_ldap_server_host[pos + 1] == ':')) {
      m_ldap_server_host = m_ldap_server_host.substr(1, pos - 1);
    }
  }
  /* IPV4 */
  else {
    auto pos = m_ldap_server_host.find(":");
    if (pos != m_ldap_server_host.npos) {
      m_ldap_server_host.erase(pos);
    }
  }
  log_info("Processed LDAP server host: ", m_ldap_server_host.c_str());

  /*
  Get the LDAP destroy TGT option from [appdefaults] section, mysql
  application. If failed to get destroy TGT option, default option value will
  be false. This value is consistent with kerberos authentication usage as TGT
  was supposed to be used till it expires.
  */
  krb5.krb5_appdefault_boolean()(m_context, mysql_apps, &principal->realm,
                                 ldap_destroy_option, 0,
                                 reinterpret_cast<int *>(&m_destroy_tgt));

EXIT:
  if (principal) krb5.krb5_free_principal()(m_context, principal);
  if (host_value) krb5.krb5_free_string()(m_context, host_value);
  return result;
}

bool Kerberos::credentials_valid() {
  bool ret_val = false;
  krb5_error_code res_kerberos = 0;
  krb5_creds credentials;
  krb5_timestamp krb_current_time = 0;
  bool credentials_retrieve = false;
  krb5_creds matching_credential;

  memset(&matching_credential, 0, sizeof(matching_credential));
  memset(&credentials, 0, sizeof(credentials));

  if (!initialize()) goto EXIT;

  log_info("Validating Kerberos credentials of '", m_user.c_str(), "'.");

  if (!open_default_cache()) goto EXIT;
  /*
    Example credentials client principal: test3@MYSQL.LOCAL
    Example credentials server principal: krbtgt/MYSQL.LOCAL@MYSQL.LOCAL
  */
  res_kerberos = krb5.krb5_parse_name()(m_context, m_user.c_str(),
                                        &matching_credential.client);
  if (res_kerberos) {
    log_error("Failed to parse Kerberos client principal.");
    goto EXIT;
  }
  res_kerberos = krb5.krb5_build_principal()(
      m_context, &matching_credential.server, m_realm.length(), m_realm.c_str(),
      "krbtgt", m_realm.c_str(), NULL);
  if (res_kerberos) {
    log_error("Failed to build Kerberos TGT principal.");
    goto EXIT;
  }

  /*
    Retrieving credentials using input parameters.
  */
  res_kerberos =
      krb5.krb5_cc_retrieve_cred()(m_context, m_krb_credentials_cache, 0,
                                   &matching_credential, &credentials);
  if (res_kerberos) {
    log_info("Kerberos credentials not found in the cache.");
    goto EXIT;
  }
  credentials_retrieve = true;
  /*
    Getting current time from kerberos libs.
  */
  res_kerberos = krb5.krb5_timeofday()(m_context, &krb_current_time);
  if (res_kerberos) {
    log_error("Failed to retrieve current time.");
    goto EXIT;
  }
  /*
    Checking validity of credentials if it is still valid.
  */
  if (credentials.times.endtime < krb_current_time) {
    log_info("Kerberos credentials expired.");
    goto EXIT;
  }

  ret_val = true;
  log_info("Kerberos credentials are valid. New TGT will not be obtained.");

EXIT:
  if (res_kerberos) log(res_kerberos);
  if (matching_credential.server) {
    krb5.krb5_free_principal()(m_context, matching_credential.server);
  }
  if (matching_credential.client) {
    krb5.krb5_free_principal()(m_context, matching_credential.client);
  }
  if (credentials_retrieve) {
    krb5.krb5_free_cred_contents()(m_context, &credentials);
  }
  return ret_val;
}

void Kerberos::destroy_credentials() {
  if (!open_default_cache())
    log_error("Failed to destroy Kerberos TGT, cannot open credentials cache.");
  krb5_error_code res_kerberos = krb5.krb5_cc_remove_cred()(
      m_context, m_krb_credentials_cache, 0, &m_credentials);
  if (res_kerberos) {
    log_error("Failed to destroy Kerberos TGT.");
    log(res_kerberos);
  }
  close_default_cache();
  log_info("Kerberos TGT destroyed.");
}

bool Kerberos::get_default_principal_name(std::string &name) {
  krb5_error_code res_kerberos = 0;
  krb5_principal principal = nullptr;
  krb5_context context = nullptr;
  char *user_name = nullptr;

  if (!initialize()) goto EXIT;

  name = "";

  if (!open_default_cache()) goto EXIT;

  /*
    Getting default principal from the cache.
  */
  res_kerberos = krb5.krb5_cc_get_principal()(
      m_context, m_krb_credentials_cache, &principal);
  if (res_kerberos) {
    log_error("Failed to get default Kerberos principal.");
    goto EXIT;
  }
  /*
    Parsing user name from principal.
  */
  res_kerberos = krb5.krb5_unparse_name()(m_context, principal, &user_name);
  if (res_kerberos) {
    log_error("Failed to parse principal name.");
    goto EXIT;
  }

  log_info("Default principal name is '", user_name, "'.");
  name = user_name;

EXIT:
  if (user_name) {
    krb5.krb5_free_unparsed_name()(context, user_name);
  }
  if (principal) {
    krb5.krb5_free_principal()(context, principal);
    principal = nullptr;
  }
  if (res_kerberos) {
    log(res_kerberos);
    return false;
  } else {
    return true;
  }
}

void Kerberos::log(int error_code) {
  const char *err_message = nullptr;
  if (m_context) {
    err_message = krb5.krb5_get_error_message()(m_context, error_code);
  }
  if (err_message) {
    log_info("Kerberos message: ", err_message);
    krb5.krb5_free_error_message()(m_context, err_message);
  }
  return;
}
}  // namespace auth_ldap_sasl_client
