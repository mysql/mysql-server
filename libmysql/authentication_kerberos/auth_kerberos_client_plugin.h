/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef AUTH_KERBEROS_CLIENT_PLUGIN_H_
#define AUTH_KERBEROS_CLIENT_PLUGIN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>

#include <mysql.h>
#include <mysql/client_plugin.h>
#include <mysql/plugin.h>
#include <mysql/plugin_auth_common.h>
#include "my_config.h"

#include "kerberos_client_interface.h"

enum class authentication_mode {
#if defined(_WIN32)
  MODE_SSPI,
#endif
  MODE_GSSAPI
};

class Kerberos_plugin_client {
 public:
  Kerberos_plugin_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql,
                         authentication_mode mode);
  ~Kerberos_plugin_client() = default;
  bool authenticate();
  void set_upn_info(std::string name, std::string pwd);
  void set_mysql_account_name(std::string name);
  bool obtain_store_credentials();
  bool read_spn_realm_from_server();

 protected:
  void create_upn(std::string account_name);
  std::string m_user_principal_name;
  std::string m_password;
  std::string m_service_principal;
  std::string m_as_user_relam;
  MYSQL_PLUGIN_VIO *m_vio{nullptr};
  MYSQL *m_mysql{nullptr};
  authentication_mode m_mode;
  std::unique_ptr<I_Kerberos_client> m_kerberos_client;
};

#endif  // AUTH_KERBEROS_CLIENT_PLUGIN_H_
