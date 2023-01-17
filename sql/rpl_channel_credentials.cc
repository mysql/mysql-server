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

#include "sql/rpl_channel_credentials.h"

#include "my_dbug.h"

void Rpl_channel_credentials::reset() {
  DBUG_TRACE;
  m_credential_set.clear();
}

Rpl_channel_credentials &Rpl_channel_credentials::get_instance() {
  DBUG_TRACE;
  static Rpl_channel_credentials object;
  return object;
}

int Rpl_channel_credentials::number_of_channels() {
  DBUG_TRACE;
  return m_credential_set.size();
}

int Rpl_channel_credentials::get_credentials(const char *channel_name,
                                             String_set &user, String_set &pass,
                                             String_set &auth) {
  DBUG_TRACE;
  user.first = pass.first = auth.first = false;
  user.second.clear();
  pass.second.clear();
  auth.second.clear();
  auto it = m_credential_set.find(channel_name);
  if (it != m_credential_set.end()) {
    Channel_cred_param cred(m_credential_set.find(channel_name)->second);
    user = cred.username;
    pass = cred.password;
    auth = cred.plugin_auth;
    return 0;
  }
  return 1;
}

int Rpl_channel_credentials::store_credentials(const char *channel_name,
                                               char *username, char *password,
                                               char *plugin_auth) {
  DBUG_TRACE;
  auto it = m_credential_set.find(channel_name);
  if (it != m_credential_set.end()) {
    return 1;
  } else {
    Channel_cred_param cred(username, password, plugin_auth);
    m_credential_set.insert(channel_credential_pair(channel_name, cred));
  }
  return 0;
}

int Rpl_channel_credentials::delete_credentials(const char *channel_name) {
  DBUG_TRACE;
  bool ret = false;
  ret = m_credential_set.erase(channel_name);
  return !ret;
}
