/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file rewriter_udf.cc

*/

#include "my_config.h"
#include "rewriter_plugin.h"

#include <my_global.h>
#include <my_sys.h>

#include <mysql.h>
#include <ctype.h>

extern "C" {

my_bool load_rewrite_rules_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  if (get_rewriter_plugin_info() != NULL)
    return 0;
  strncpy(message, "Rewriter plugin needs to be installed.", MYSQL_ERRMSG_SIZE);
  return 1;
}

char *load_rewrite_rules(UDF_INIT *initid, UDF_ARGS *args, char *result,
                         unsigned long *length, char *is_null, char *error)
{
  DBUG_ASSERT(get_rewriter_plugin_info() != NULL);
  const char *message= NULL;
  if (refresh_rules_table())
  {
    message= "Loading of some rule(s) failed.";
    *length= static_cast<unsigned long>(strlen(message));
  }
  else
    *is_null= 1;

  return const_cast<char*>(message);
}

void load_rewrite_rules_deinit(UDF_INIT *initid) {}

}
