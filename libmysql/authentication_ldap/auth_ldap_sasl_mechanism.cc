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

#include "auth_ldap_sasl_mechanism.h"

#include "my_config.h"

#include <assert.h>
#ifndef WIN32
#include <lber.h>
#endif

#include "log_client.h"

namespace auth_ldap_sasl_client {

const char Sasl_mechanism::SASL_GSSAPI[] = "GSSAPI";
const char Sasl_mechanism::SASL_SCRAM_SHA1[] = "SCRAM-SHA-1";
const char Sasl_mechanism::SASL_SCRAM_SHA256[] = "SCRAM-SHA-256";

#if defined(SCRAM_LIB_CONFIGURED)
const sasl_callback_t Sasl_mechanism_scram::callbacks[] = {
#ifdef SASL_CB_GETREALM
    {SASL_CB_GETREALM, nullptr, nullptr},
#endif  // SASL_CB_GETREALM
    {SASL_CB_USER, nullptr, nullptr},
    {SASL_CB_AUTHNAME, nullptr, nullptr},
    {SASL_CB_PASS, nullptr, nullptr},
    {SASL_CB_ECHOPROMPT, nullptr, nullptr},
    {SASL_CB_NOECHOPROMPT, nullptr, nullptr},
    {SASL_CB_LIST_END, nullptr, nullptr}};
#endif  // SCRAM_LIB_CONFIGURED

#if defined(KERBEROS_LIB_CONFIGURED)
const sasl_callback_t Sasl_mechanism_kerberos::callbacks[] = {
#ifdef SASL_CB_GETREALM
    {SASL_CB_GETREALM, nullptr, nullptr},
#endif  // SASL_CB_GETREALM
    {SASL_CB_ECHOPROMPT, nullptr, nullptr},
    {SASL_CB_NOECHOPROMPT, nullptr, nullptr},
    {SASL_CB_LIST_END, nullptr, nullptr}};

bool Sasl_mechanism_kerberos::get_default_user(std::string &name) {
  return m_kerberos.get_default_principal_name(name);
}

bool Sasl_mechanism_kerberos::preauthenticate(const char *user,
                                              const char *password) {
  assert(user);
  assert(password);

  m_kerberos.set_user_and_password(user, password);
  m_kerberos.get_ldap_host(m_ldap_server_host);

  log_info("LDAP host is ", m_ldap_server_host.c_str());

  /*
     Password is empty.
     Existing TGT will be used for authentication.
     The user name must be either empty or it must be the same as principal name
     of TGT. Main user case.
  */
  if (password[0] == 0) {
    if (m_kerberos.credentials_valid()) {
      log_info(
          "Existing Kerberos TGT is valid and will be used for "
          "authentication.");
      return true;
    } else {
      log_error(
          "Existing Kerberos TGT is not valid. Authentication will be "
          "aborted. ");
      return false;
    }
  }
  /*
     Password is not empty, Obtaining TGT from Kerberos.
     First time use case.
  */
  else {
    if (m_kerberos.obtain_store_credentials()) {
      log_info("TGT from Kerberos obtained successfuly.");
      return true;
    } else {
      log_error("Obtaining TGT from Kerberos failed.");
      return false;
    }
  }
}
const char *Sasl_mechanism_kerberos::get_ldap_host() {
  return m_ldap_server_host.empty() ? nullptr : m_ldap_server_host.c_str();
}

#endif  // KERBEROS_LIB_CONFIGURED

bool Sasl_mechanism::create_sasl_mechanism(const char *mechanism_name,
                                           Sasl_mechanism *&mechanism) {
  if (mechanism_name == nullptr || mechanism_name[0] == 0) {
    log_error("Empty SASL method name.");
    return false;
  }
  if (mechanism != nullptr) {
    if (strcmp(mechanism_name, mechanism->m_mechanism_name) == 0) {
      log_dbg("Correct SASL mechanism already exists.");
      return true;
    } else {
      log_error("SASL mechanism mismatch.");
      return false;
    }
  }
  log_dbg("Creating object for SASL mechanism ", mechanism_name, ".");

  if (strcmp(mechanism_name, SASL_GSSAPI) == 0)
#if defined(KERBEROS_LIB_CONFIGURED)
    mechanism = new Sasl_mechanism_kerberos();
#else
  {
    log_error(
        "The client was not built with GSSAPI/Kerberos libraries, method not "
        "supported.");
    return false;
  }
#endif

#if defined(SCRAM_LIB_CONFIGURED)
  else if (strcmp(mechanism_name, SASL_SCRAM_SHA1) == 0)
    mechanism = new Sasl_mechanism_scram(SASL_SCRAM_SHA1);
  else if (strcmp(mechanism_name, SASL_SCRAM_SHA256) == 0)
    mechanism = new Sasl_mechanism_scram(SASL_SCRAM_SHA256);
#else
  else if (strcmp(mechanism_name, SASL_SCRAM_SHA1) == 0 ||
           strcmp(mechanism_name, SASL_SCRAM_SHA256) == 0) {
    log_error(
        "The client was not built with SCRAM libraries, method not "
        "supported.");
    return false;
  }
#endif
  else {
    log_error("SASL method", mechanism_name,
              " is not supported by the client.");
    return false;
  }

  return true;
}
}  // namespace auth_ldap_sasl_client
