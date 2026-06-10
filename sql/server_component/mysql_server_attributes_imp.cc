/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include "mysql_server_attributes_imp.h"

#include <mysql/components/minimal_chassis.h>
#include <mysql/components/services/defs/mysql_string_defs.h>
#include <sql/mysqld.h>
#include <sql_string.h>

static mysql_cstring_with_length g_os_version = {
    STRING_WITH_LEN(MACHINE_TYPE "-" SYSTEM_TYPE)};

DEFINE_BOOL_METHOD(mysql_server_attributes_imp::get,
                   (const char *name, void *inout_pvalue)) {
  try {
    if (inout_pvalue) {
      if (!strcmp(name, "server_version")) {
        *((mysql_cstring_with_length *)inout_pvalue) = {server_version,
                                                        strlen(server_version)};
      } else if (!strcmp(name, "server_id")) {
        *((ulong *)inout_pvalue) = server_id;
      } else if (!strcmp(name, "os_version")) {
        *((mysql_cstring_with_length *)inout_pvalue) = g_os_version;
      } else if (!strcmp(name, "argc")) {
        *((int *)inout_pvalue) = orig_argc;
      } else if (!strcmp(name, "argv")) {
        *((char ***)inout_pvalue) = orig_argv;
      } else
        return 1; /* invalid option */
    }
    return 0;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return 1;
}
