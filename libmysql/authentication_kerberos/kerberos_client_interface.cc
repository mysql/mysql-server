/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "kerberos_client_interface.h"

#include "gssapi_authentication_client.h"
#if defined(_WIN32)
#include "sspi_authentication_client.h"
#endif

#include "auth_kerberos_client_plugin.h"

I_Kerberos_client *Kerberos_client_create_factory(
    bool gssapi [[maybe_unused]], const std::string &spn, MYSQL_PLUGIN_VIO *vio,
    const std::string &upn, const std::string &password,
    const std::string &kdc_host [[maybe_unused]]) {
#if defined(_WIN32)
  if (!gssapi) {
    Sspi_client *client = new Sspi_client(spn, vio, upn, password, kdc_host);
    return static_cast<I_Kerberos_client *>(client);
  }
#endif
  Gssapi_client *client = new Gssapi_client(spn, vio, upn, password);
  return static_cast<I_Kerberos_client *>(client);
}
