/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/auth/auth_common.h"

#include <string.h>

#include "sql/field.h"
#include "sql/table.h"

bool
Acl_load_user_table_schema_factory::is_old_user_table_schema(TABLE* table)
{
  Field *password_field=
    table->field[Acl_load_user_table_old_schema::MYSQL_USER_FIELD_PASSWORD_56];
  return strncmp(password_field->field_name, "Password", 8) == 0;
}
