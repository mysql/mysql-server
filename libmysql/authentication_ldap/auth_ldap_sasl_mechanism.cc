/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "my_config.h"

#include "auth_ldap_sasl_mechanism.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <lber.h>
#include <sasl/sasl.h>
#endif

#if defined(KERBEROS_LIB_CONFIGURED)
extern Ldap_logger *g_logger_client;
using namespace auth_ldap_client_kerberos_context;
#else
Ldap_logger *g_logger_client = NULL;
#endif

#if defined(KERBEROS_LIB_CONFIGURED)
Sasl_mechanism_kerberos::Sasl_mechanism_kerberos() = default;

Sasl_mechanism_kerberos::~Sasl_mechanism_kerberos() = default;

bool Sasl_mechanism_kerberos::pre_authentication() {
  m_kerberos = std::unique_ptr<Kerberos>(
      new Kerberos(m_user.c_str(), m_password.c_str()));
  /**
     Both user name and password are empty.
     Existing TGT will be used for authentication.
     Main user case.
  */
  if (m_user.empty() && m_password.empty()) {
    log_dbg(
        "MySQL user name and password are empty. Existing TGT will be used for "
        "authentication.");
    return true;
  }
  /**
     User name and password are not empty, Obtaining TGT from kerberos.
     First time use case.
  */
  if (!m_user.empty() && !m_password.empty()) {
    log_dbg("Obtaining TGT from kerberos.");
    return m_kerberos->obtain_store_credentials();
  }
  /**
    User name is not empty.
  */
  if (!m_user.empty()) {
    std::string user_name;
    m_kerberos->get_user_name(&user_name);
    /**
       MySQL user name and kerberos default principle name is same.
       Existing TGT will be used for authentication.
    */
    if (user_name == m_user) {
      log_dbg(
          "MySQL user name and kerberos default principle name is same. "
          "Existing TGT will be used for authentication.");
      return true;
    } else {
      bool ret_val = false;
      log_dbg(
          "MySQL user name and kerberos default principle name is different. "
          "Authentication will be aborted. ");
      return ret_val;
    }
  }
  /**
    User has provided password but not user name, returning error.
  */
  log_dbg(
      "MySQL user name is empty but password is not empty. Authentication will "
      "be aborted. ");
  return false;
}

void Sasl_mechanism_kerberos::get_ldap_host(std::string &host) {
  log_dbg("Sasl_mechanism_kerberos::get_ldap_host");
  if (!m_kerberos) return;
  m_kerberos->get_ldap_host(host);
}

#endif

void Sasl_mechanism::set_user_info(std::string user, std::string password) {
  m_user = user;
  m_password = password;
}

Sasl_mechanism::Sasl_mechanism() = default;

Sasl_mechanism::~Sasl_mechanism() = default;

bool Sasl_mechanism::pre_authentication() { return true; }

void Sasl_mechanism::get_ldap_host(std::string &host) { host = ""; }
