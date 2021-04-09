/* Copyright (c) 2021, Oracle and/or its affiliates.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kerberos_client.h"

extern Logger_client *g_logger_client;

Kerberos_client::Kerberos_client(const std::string &upn,
                                 const std::string &password)
    : m_user_principal_name{upn}, m_password{password} {
  m_kerberos = std::unique_ptr<auth_kerberos_context::Kerberos>(
      new auth_kerberos_context::Kerberos(m_user_principal_name.c_str(),
                                          m_password.c_str()));
}

Kerberos_client::~Kerberos_client() {}

void Kerberos_client::set_upn_info(const std::string &upn,
                                   const std::string &pwd) {
  log_client_dbg("Set UPN.");
  m_user_principal_name = {upn};
  m_password = {pwd};
  /* Kerberos core uses UPN for all other operations. UPN has changed, relases
   * current object and create */
  if (m_kerberos.get()) {
    m_kerberos.release();
  }
  m_kerberos = std::unique_ptr<auth_kerberos_context::Kerberos>(
      new auth_kerberos_context::Kerberos(m_user_principal_name.c_str(),
                                          m_password.c_str()));
}

bool Kerberos_client::obtain_store_credentials() {
  log_client_dbg("Obtaining TGT TGS tickets from kerberos.");
  return m_kerberos->obtain_store_credentials();
}

bool Kerberos_client::get_upn(std::string &cc_upn) {
  log_client_dbg("Getting user name from Kerberos credential cache.");
  return m_kerberos->get_upn(&cc_upn);
}