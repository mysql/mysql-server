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

#ifndef AUTH_KERBEROS_CLIENT_IO_H_
#define AUTH_KERBEROS_CLIENT_IO_H_

#include <mysql/plugin_auth.h>

class Kerberos_client_io {
 public:
  explicit Kerberos_client_io(MYSQL_PLUGIN_VIO *vio);
  ~Kerberos_client_io();
  bool write_gssapi_buffer(const unsigned char *buffer, int buffer_len);
  bool read_gssapi_buffer(unsigned char **gssapi_buffer, size_t *buffer_len);
  bool read_spn_realm_from_server(std::string &service_principal_name,
                                  std::string &upn_realm);

 private:
  /* Plug-in VIO. */
  MYSQL_PLUGIN_VIO *m_vio{nullptr};
};

#endif  // AUTH_KERBEROS_CLIENT_IO_H_
