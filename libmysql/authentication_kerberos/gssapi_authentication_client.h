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

#ifndef AUTH_GSSAPI_CLIENT_H_
#define AUTH_GSSAPI_CLIENT_H_

#include <gssapi/gssapi.h>
#include <memory>

#include "kerberos_core.h"
#include "log_client.h"

#include "kerberos_client_interface.h"

class Gssapi_client : public I_Kerberos_client {
 public:
  Gssapi_client(const std::string &spn, MYSQL_PLUGIN_VIO *vio,
                const std::string &upn, const std::string &password);
  ~Gssapi_client() override;
  bool authenticate() override;
  std::string get_user_name() override;
  void set_upn_info(const std::string &name, const std::string &pwd);
  bool obtain_store_credentials() override;

 protected:
  std::string m_service_principal;
  /* Plug-in VIO. */
  MYSQL_PLUGIN_VIO *m_vio{nullptr};
  std::string m_user_principal_name;
  std::string m_password;
  std::unique_ptr<auth_kerberos_context::Kerberos> m_kerberos{nullptr};
};
#endif  // AUTH_GSSAPI_CLIENT_H_
