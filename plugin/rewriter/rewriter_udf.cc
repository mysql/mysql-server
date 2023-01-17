/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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
  @file rewriter_udf.cc

*/

#include "my_config.h"

#include <assert.h>
#include <ctype.h>
#include <mysql.h>

#include "my_inttypes.h"
#include "my_sys.h"
#include "plugin/rewriter/rewriter_plugin.h"

extern "C" {

bool load_rewrite_rules_init(UDF_INIT *, UDF_ARGS *, char *message) {
  if (get_rewriter_plugin_info() != nullptr) return false;
  strncpy(message, "Rewriter plugin needs to be installed.", MYSQL_ERRMSG_SIZE);
  return true;
}

char *load_rewrite_rules(UDF_INIT *, UDF_ARGS *, char *, unsigned long *length,
                         unsigned char *is_null, unsigned char *) {
  assert(get_rewriter_plugin_info() != nullptr);
  const char *message = nullptr;
  if (refresh_rules_table()) {
    message = "Loading of some rule(s) failed.";
    *length = static_cast<unsigned long>(strlen(message));
  } else
    *is_null = 1;

  return const_cast<char *>(message);
}

void load_rewrite_rules_deinit(UDF_INIT *) {}
}
