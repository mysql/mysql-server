/*
   Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "client/include/client_priv.h"
#include "my_config.h"
#include "mysql/service_mysql_alloc.h"  // my_malloc, my_strdup

#include "client/client_query_attributes.h"

client_query_attributes *global_attrs = nullptr;

bool client_query_attributes::push_param(const char *name, const char *value) {
  return push_param(name, strlen(name), value, strlen(value));
}

bool client_query_attributes::push_param(const char *name, size_t name_length,
                                         const char *value,
                                         size_t value_length) {
  if (count >= max_count) return true;

  /* Copy name */
  char *name_copy =
      (char *)my_malloc(PSI_NOT_INSTRUMENTED, name_length + 1, MYF(0));
  if (name_length) {
    memcpy(name_copy, name, name_length);
  }
  name_copy[name_length] = 0;

  names[count] = name_copy;

  /* Copy value */
  char *value_copy =
      (char *)my_malloc(PSI_NOT_INSTRUMENTED, value_length + 1, MYF(0));
  if (value_length) {
    memcpy(value_copy, value, value_length);
  }
  value_copy[value_length] = 0;

  memset(&values[count], 0, sizeof(MYSQL_BIND));
  values[count].buffer = value_copy;
  values[count].buffer_length = value_length;
  values[count].buffer_type = MYSQL_TYPE_STRING;

  count++;
  return false;
}

int client_query_attributes::set_params(MYSQL *mysql) {
  if (count == 0) return 0;

  const int rc = mysql_bind_param(mysql, count, values, names);
  return rc;
}

int client_query_attributes::set_params_stmt(MYSQL_STMT *stmt) {
  if (count == 0) return 0;

  const int rc = mysql_stmt_bind_named_param(stmt, values, count, names);
  return rc;
}

void client_query_attributes::clear(MYSQL *mysql) {
  if (mysql != nullptr) mysql_bind_param(mysql, 0, nullptr, nullptr);
  while (count) {
    count--;
    my_free(const_cast<char *>(names[count]));
    my_free(values[count].buffer);
  }
  memset(&names, 0, sizeof(names));
  memset(&values, 0, sizeof(values));
}
