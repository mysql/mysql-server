/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "kerberos_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <krb5/krb5.h>
#include <profile.h>

extern Logger_client *g_logger_client;

namespace auth_kerberos_context {

Kerberos::Kerberos(const char *upn, const char *password)
    : m_initialized{false},
      m_upn{upn},
      m_password{password},
      m_destroy_tickets{0},
      m_context{nullptr},
      m_krb_credentials_cache{nullptr},
      m_credentials_created{false} {
  if (g_logger_client == nullptr) {
    g_logger_client = new Logger_client();
  }
  setup();
}

Kerberos::~Kerberos() { cleanup(); }

bool Kerberos::setup() {
  krb5_error_code res_kerberos{0};
  bool ret_val{false};

  if (m_initialized) {
    ret_val = true;
    goto CLEANUP;
  }
  log_client_dbg("Kerberos setup starting.");
  if ((res_kerberos = krb5_init_context(&m_context)) != 0) {
    log_client_info("Kerberos setup: failed to initialize context.");
    goto CLEANUP;
  }
  if ((res_kerberos = get_kerberos_config()) != 0) {
    log_client_info(
        "Kerberos setup: failed to get required details from "
        "configuration file.");
    goto CLEANUP;
  }
  m_initialized = true;
  ret_val = true;

CLEANUP:
  if (res_kerberos) {
    log(res_kerberos);
    cleanup();
  }
  return ret_val;
}

void Kerberos::cleanup() {
  if (m_destroy_tickets && m_credentials_created) {
    destroy_credentials();
  }

  if (m_krb_credentials_cache) {
    krb5_cc_close(m_context, m_krb_credentials_cache);
    m_krb_credentials_cache = nullptr;
  }

  if (m_context) {
    krb5_free_context(m_context);
    m_context = nullptr;
  }

  m_initialized = false;
}

krb5_error_code Kerberos::store_credentials() {
  krb5_error_code res_kerberos{0};
  log_client_dbg("Store credentials starting.");
  res_kerberos =
      krb5_cc_store_cred(m_context, m_krb_credentials_cache, &m_credentials);
  if (res_kerberos) {
    log_client_info(
        "Kerberos store credentials: failed to store credentials. ");
  }
  return res_kerberos;
}

krb5_error_code Kerberos::obtain_credentials() {
  krb5_error_code res_kerberos{0};
  krb5_get_init_creds_opt *options{nullptr};
  char *password{const_cast<char *>(m_password.c_str())};
  krb5_principal principal{nullptr};

  log_client_dbg("Obtain credentials starting.");

  if (m_credentials_created) {
    log_client_info(
        "Kerberos obtain credentials: already obtained credential.");
    goto CLEANUP;
  }

  memset(&principal, 0, sizeof(krb5_principal));

  /*
    Full user name with domain name like, yashwant.sahu@oracle.com.
    Parsed principal will be used for authentication and to check if user is
    already authenticated.
  */
  if (!m_upn.empty()) {
    res_kerberos = krb5_parse_name(m_context, m_upn.c_str(), &principal);
  } else {
    goto CLEANUP;
  }
  if (res_kerberos) {
    log_client_info("Kerberos obtain credentials: failed to parse user name.");
    goto CLEANUP;
  }
  if (m_krb_credentials_cache == nullptr) {
    res_kerberos = krb5_cc_default(m_context, &m_krb_credentials_cache);
  }
  if (res_kerberos) {
    log_client_info(
        "Kerberos obtain credentials: failed to get default credentials "
        "cache.");
    goto CLEANUP;
  }
  memset(&m_credentials, 0, sizeof(m_credentials));
  krb5_get_init_creds_opt_alloc(m_context, &options);
  /*
    Getting TGT from TGT server.
  */
  res_kerberos = krb5_get_init_creds_password(m_context, &m_credentials,
                                              principal, password, nullptr,
                                              nullptr, 0, nullptr, options);

  if (res_kerberos) {
    log_client_info(
        "Kerberos obtain credentials: failed to obtain credentials.");
    goto CLEANUP;
  }
  m_credentials_created = true;
  /*
    Verifying TGT.
  */
  res_kerberos = krb5_verify_init_creds(m_context, &m_credentials, nullptr,
                                        nullptr, nullptr, nullptr);
  if (res_kerberos) {
    log_client_info(
        "Kerberos obtain credentials: failed to verify credentials.");
    goto CLEANUP;
  }
  log_client_dbg("Obtain credential successful");
  if (principal) {
    res_kerberos =
        krb5_cc_initialize(m_context, m_krb_credentials_cache, principal);
    if (res_kerberos) {
      log_client_info(
          "Kerberos store credentials: failed to initialize credentials "
          "cache.");
      goto CLEANUP;
    }
  }

CLEANUP:
  if (options) {
    krb5_get_init_creds_opt_free(m_context, options);
    options = nullptr;
  }
  if (principal) {
    krb5_free_principal(m_context, principal);
    principal = nullptr;
  }
  if (m_credentials_created && res_kerberos) {
    krb5_free_cred_contents(m_context, &m_credentials);
    m_credentials_created = false;
  }
  return res_kerberos;
}

bool Kerberos::obtain_store_credentials() {
  bool ret_val{false};
  krb5_error_code res_kerberos{0};
  if (!m_initialized) {
    log_client_dbg("Kerberos object is not initialized.");
    goto CLEANUP;
  }
  if (m_upn.empty()) {
    log_client_info("Kerberos obtain and store TGT: empty user name.");
    goto CLEANUP;
  }
  /*
    If valid credential exist, no need to obtain it again.
    This is done for the performance reason. Default expiry of TGT is 24 hours
    and this can be configured.
  */
  if ((ret_val = credential_valid())) {
    log_client_info(
        "Kerberos obtain and store TGT: Valid ticket exist, password will "
        "not be used.");
    goto CLEANUP;
  }
  if ((res_kerberos = obtain_credentials()) != 0) {
    log_client_info(
        "Kerberos obtain and store TGT: failed to obtain "
        "TGT/credentials.");
    goto CLEANUP;
  }
  /*
    Store the credentials in the default cache. Types can be file, memory,
    keyring etc. Administrator should change default cache based on there
    preference.
  */
  if ((res_kerberos = store_credentials()) != 0) {
    log_client_info(
        "Kerberos obtain and store TGT: failed to store credentials.");
    goto CLEANUP;
  }

  ret_val = true;

CLEANUP:
  if (res_kerberos) {
    ret_val = false;
    log(res_kerberos);
  }
  /*
    Storing the credentials.
    We need to close the context to save the credentials successfully.
   */
  if (m_credentials_created && !m_destroy_tickets) {
    krb5_free_cred_contents(m_context, &m_credentials);
    m_credentials_created = false;
    if (m_krb_credentials_cache) {
      log_client_info("Storing credentials into cache, closing krb5 cc.");
      krb5_cc_close(m_context, m_krb_credentials_cache);
      m_krb_credentials_cache = nullptr;
    }
  }
  return ret_val;
}

/*
  This method gets kerberos profile settings from krb5.conf file.
  Sample krb5.conf file format may be like this:

  [realms]
  MYSQL.LOCAL = {
    kdc = VIKING67.MYSQL.LOCAL
    admin_server = VIKING67.MYSQL.LOCAL
    default_domain = MYSQL.LOCAL
    }

  # This portion is optional
  [appdefaults]
  mysql = {
    destroy_tickets = true
  }
*/
bool Kerberos::get_kerberos_config() {
  log_client_dbg("Getting kerberos configuration.");
#if !defined(_WIN32)
  /*
    Kerberos profile category/sub-category names.
  */
  const char apps_heading[]{"appdefaults"};
  const char mysql_apps[]{"mysql"};
  const char destroy_option[]{"destroy_tickets"};
#endif

  std::stringstream info_stream;

  krb5_error_code res_kerberos{0};
  _profile_t *profile{nullptr};

  res_kerberos = krb5_get_profile(m_context, &profile);
  if (res_kerberos) {
    log_client_error("get_kerberos_config: failed to kerberos configurations.");
    goto CLEANUP;
  }

/*
  profile_get_boolean is not available from xpprof64.dll

  objdump.exe -p .\xpprof64.dll | findstr "profile_get"
      [   5] profile_get_integer
      [   6] profile_get_relation_names
      [   7] profile_get_string
      [   8] profile_get_subsection_names
      [   9] profile_get_values

  Hence following code is enabled only on non-Windows platform
*/
#if !defined(_WIN32)
  /*
    Get the destroy tickets from MySQL app section.
    If failed to get destroy tickets option, default option value will be false.
    This value is consistent with kerberos authentication usage as tickets was
    supposed to be used till it expires.
  */
  res_kerberos =
      profile_get_boolean(profile, apps_heading, mysql_apps, destroy_option,
                          m_destroy_tickets, (int *)&m_destroy_tickets);
  if (res_kerberos) {
    log_client_info(
        "get_kerberos_config: failed to get destroy_tickets flag, default is "
        "set "
        "to false.");
  }
#endif /* _WIN32 */

CLEANUP:
  profile_release(profile);
  info_stream << "destroy_tickets is: " << m_destroy_tickets;
  log_client_info(info_stream.str().c_str());
  return res_kerberos;
}

bool Kerberos::credential_valid() {
  bool ret_val{false};
  krb5_error_code res_kerberos{0};
  krb5_creds credentials;
  krb5_timestamp krb_current_time;
  bool credentials_retrieve{false};
  krb5_creds matching_credential;
  std::stringstream info_stream;

  memset(&matching_credential, 0, sizeof(matching_credential));
  memset(&credentials, 0, sizeof(credentials));

  if (m_krb_credentials_cache == nullptr) {
    res_kerberos = krb5_cc_default(m_context, &m_krb_credentials_cache);
    if (res_kerberos) {
      log_client_info(
          "Kerberos setup: failed to get default credentials cache.");
      goto CLEANUP;
    }
  }
  /*
    Example credentials client principal: test3@MYSQL.LOCAL
    Example credentials server principal: krbtgt/kerberos_auth_host@MYSQL.LOCAL
  */
  res_kerberos =
      krb5_parse_name(m_context, m_upn.c_str(), &matching_credential.client);
  if (res_kerberos) {
    log_client_info(
        "Kerberos credentials valid: failed to parse client principal.");
    goto CLEANUP;
  }
  res_kerberos =
      krb5_build_principal(m_context, &matching_credential.server,
                           matching_credential.client->realm.length,
                           matching_credential.client->realm.data, "krbtgt",
                           matching_credential.client->realm.data, nullptr);
  if (res_kerberos) {
    log_client_info(
        "Kerberos credentials valid: failed to build krbtgt principal.");
    goto CLEANUP;
  }

  /*
    Retrieving credentials using input parameters.
  */
  res_kerberos = krb5_cc_retrieve_cred(m_context, m_krb_credentials_cache, 0,
                                       &matching_credential, &credentials);
  if (res_kerberos) {
    log_client_info(
        "Kerberos credentials valid: failed to retrieve credentials.");
    goto CLEANUP;
  }
  credentials_retrieve = true;
  /*
    Getting current time from kerberos libs.
  */
  res_kerberos = krb5_timeofday(m_context, &krb_current_time);
  if (res_kerberos) {
    log_client_info(
        "Kerberos credentials valid: failed to retrieve current time.");
    goto CLEANUP;
  }
  /*
    Checking validity of credentials if it is still valid.
  */
  if (credentials.times.endtime < krb_current_time) {
    log_client_info("Kerberos credentials valid: credentials are expired.");
    goto CLEANUP;
  } else {
    ret_val = true;
    log_client_info(
        "Kerberos credentials valid: credentials are valid. New TGT will "
        "not be obtained.");
  }

CLEANUP:
  if (res_kerberos) {
    ret_val = false;
    log(res_kerberos);
  }
  if (matching_credential.server) {
    krb5_free_principal(m_context, matching_credential.server);
  }
  if (matching_credential.client) {
    krb5_free_principal(m_context, matching_credential.client);
  }
  if (credentials_retrieve) {
    krb5_free_cred_contents(m_context, &credentials);
  }
  if (m_krb_credentials_cache) {
    krb5_cc_close(m_context, m_krb_credentials_cache);
    m_krb_credentials_cache = nullptr;
  }
  return ret_val;
}

void Kerberos::destroy_credentials() {
  log_client_dbg("Kerberos destroy credentials");
  if (!m_destroy_tickets) {
    log_client_dbg("Kerberos destroy credentials: destroy flag is false.");
    return;
  }
  krb5_error_code res_kerberos{0};
  if (m_credentials_created) {
    res_kerberos = krb5_cc_remove_cred(m_context, m_krb_credentials_cache, 0,
                                       &m_credentials);
    krb5_free_cred_contents(m_context, &m_credentials);
    m_credentials_created = false;
  }
  if (res_kerberos) {
    log(res_kerberos);
  }
}

bool Kerberos::get_upn(std::string *upn) {
  krb5_error_code res_kerberos{0};
  krb5_principal principal{nullptr};
  krb5_context context{nullptr};
  char *upn_name{nullptr};
  std::stringstream log_client_stream;

  if (!m_initialized) {
    log_client_dbg("Kerberos object is not initialized.");
    goto CLEANUP;
  }
  if (!upn) {
    log_client_dbg("Name variable is null");
    goto CLEANUP;
  }
  *upn = "";
  if (m_krb_credentials_cache == nullptr) {
    res_kerberos = krb5_cc_default(m_context, &m_krb_credentials_cache);
    if (res_kerberos) {
      log_client_info(
          "Kerberos setup: failed to get default credentials cache.");
      goto CLEANUP;
    }
  }
  /*
    Getting default principal in the kerberos.
  */
  res_kerberos =
      krb5_cc_get_principal(m_context, m_krb_credentials_cache, &principal);
  if (res_kerberos) {
    log_client_info("Get user principal name: failed to get principal.");
    goto CLEANUP;
  }
  /*
    Parsing user principal name from principal object.
  */
  res_kerberos = krb5_unparse_name(m_context, principal, &upn_name);
  if (res_kerberos) {
    log_client_info("Get user principal name: failed to parse principal name.");
    goto CLEANUP;
  } else {
    log_client_stream << "Get user principal name: ";
    log_client_stream << upn_name;
    log_client_info(log_client_stream.str());
    *upn = upn_name;
    if (m_upn.empty()) {
      m_upn = upn_name;
    }
  }

CLEANUP:
  if (upn_name) {
    krb5_free_unparsed_name(context, upn_name);
  }
  if (principal) {
    krb5_free_principal(context, principal);
    principal = nullptr;
  }
  if (m_krb_credentials_cache) {
    krb5_cc_close(m_context, m_krb_credentials_cache);
    m_krb_credentials_cache = nullptr;
  }
  if (res_kerberos) {
    log(res_kerberos);
    return false;
  } else {
    return true;
  }
}

void Kerberos::log(int error_code) {
  const char *err_message{nullptr};
  std::stringstream error_stream;
  if (m_context) {
    err_message = krb5_get_error_message(m_context, error_code);
  }
  if (err_message) {
    error_stream << "Kerberos operation failed with error: " << err_message;
  }
  log_client_error(error_stream.str());
  if (err_message) {
    krb5_free_error_message(m_context, err_message);
  }
  return;
}
}  // namespace auth_kerberos_context
