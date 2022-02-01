/*
   Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "client/client_priv.h"
#include "my_config.h"
#include "mysql/service_mysql_alloc.h"  // my_malloc, my_strdup

#include "client/client_query_attributes.h"

client_query_attributes *global_attrs = nullptr;

bool client_query_attributes::push_param(char *name, char *value) {
  if (count >= max_count) return true;
  names[count] = my_strdup(PSI_NOT_INSTRUMENTED, name, MYF(0));
  memset(&values[count], 0, sizeof(MYSQL_BIND));
  unsigned val_len = strlen(value);
  values[count].buffer = my_malloc(PSI_NOT_INSTRUMENTED, val_len + 1, MYF(0));
  if (val_len) memcpy(values[count].buffer, value, val_len);
  ((unsigned char *)values[count].buffer)[val_len] = 0;
  values[count].buffer_length = val_len;
  values[count].buffer_type = MYSQL_TYPE_STRING;
  count++;
  return false;
}

int client_query_attributes::set_params(MYSQL *mysql) {
  if (count == 0) return 0;

  int rc = mysql_bind_param(mysql, count, values, names);
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
