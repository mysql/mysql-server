/* Copyright (c) 2022, Oracle and/or its affiliates.

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

/**
  @file include/authentication_kerberos_clientopt-vars.h
*/

#ifndef AUTHETICATION_KERBEROS_CLIENTOPT_VARS_H
#define AUTHETICATION_KERBEROS_CLIENTOPT_VARS_H

#if defined(_WIN32)
#include "m_string.h"
#include "mysql.h"
#include "template_utils.h"
#include "typelib.h"

#include <cstdio>

using std::snprintf;

const char *client_mode_names_lib[] = {"SSPI", "GSSAPI", NullS};

TYPELIB client_mode_typelib = {array_elements(client_mode_names_lib) - 1, "",
                               client_mode_names_lib, nullptr};

static int opt_authentication_kerberos_client_mode = 0;

static int set_authentication_kerberos_client_mode(MYSQL *mysql, char *error,
                                                   size_t error_size) {
  if (opt_authentication_kerberos_client_mode == 1) {
    struct st_mysql_client_plugin *kerberos_client_plugin =
        mysql_client_find_plugin(mysql, "authentication_kerberos_client",
                                 MYSQL_CLIENT_AUTHENTICATION_PLUGIN);
    if (!kerberos_client_plugin) {
      snprintf(error, error_size,
               "Failed to load plugin authentication_kerberos_client.");
      return 1;
    }

    if (mysql_plugin_options(
            kerberos_client_plugin,
            "plugin_authentication_kerberos_client_mode",
            client_mode_names_lib[opt_authentication_kerberos_client_mode])) {
      snprintf(error, error_size,
               "Failed to set value '%s' for "
               "--plugin-authentication-kerberos-client-mode",
               client_mode_names_lib[opt_authentication_kerberos_client_mode]);
      return 1;
    }
  }
  return 0;
}
#endif /* _WIN32 */

#endif  // !AUTHETICATION_KERBEROS_CLIENTOPT_VARS_H
