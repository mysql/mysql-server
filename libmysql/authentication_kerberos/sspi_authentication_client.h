/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef AUTH_SSAPI_CLIENT_H_
#define AUTH_SSAPI_CLIENT_H_

#include <stdio.h>
#include <stdlib.h>

/*
  This symbol should be used when accessing the security API from a user-mode
  application
*/
#define SECURITY_WIN32

/*
  Note: Added below header files with one extra lines.
  Automatically code formatting changes the order of header file.
  We need to maintain the order of header file, otherwise build will fail.
*/

#include <windows.h>

#include <sspi.h>

#include <SecExt.h>

#include <stdarg.h>

#include "kerberos_client_interface.h"

class Sspi_client : public I_Kerberos_client {
 public:
  Sspi_client(const std::string &spn, MYSQL_PLUGIN_VIO *vio,
              const std::string &upn, const std::string &password,
              const std::string &kdc_host);
  ~Sspi_client() override = default;
  bool authenticate() override;
  bool obtain_store_credentials() override;
  std::string get_user_name() override;

 protected:
  std::string m_service_principal;
  /* Plug-in VIO. */
  MYSQL_PLUGIN_VIO *m_vio{nullptr};
  std::string m_upn;
  std::string m_password;
  std::string m_kdc_host;
  CredHandle m_cred;
};
#endif  // AUTH_SSAPI_CLIENT_H_
