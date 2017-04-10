/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_column_types.cc
  Literal values for columns used in the performance
  schema tables (implementation).
*/

#include "storage/perfschema/pfs_column_types.h"

#include "lex_string.h"
#include "m_string.h" /* LEX_STRING */

struct s_object_type_map
{
  enum_object_type m_enum;
  LEX_STRING m_string;
};

static s_object_type_map object_type_map[] = {
  {OBJECT_TYPE_EVENT, {C_STRING_WITH_LEN("EVENT")}},
  {OBJECT_TYPE_FUNCTION, {C_STRING_WITH_LEN("FUNCTION")}},
  {OBJECT_TYPE_PROCEDURE, {C_STRING_WITH_LEN("PROCEDURE")}},
  {OBJECT_TYPE_TABLE, {C_STRING_WITH_LEN("TABLE")}},
  {OBJECT_TYPE_TEMPORARY_TABLE, {C_STRING_WITH_LEN("TEMPORARY TABLE")}},
  {OBJECT_TYPE_TRIGGER, {C_STRING_WITH_LEN("TRIGGER")}},
  {OBJECT_TYPE_GLOBAL, {C_STRING_WITH_LEN("GLOBAL")}},
  {OBJECT_TYPE_SCHEMA, {C_STRING_WITH_LEN("SCHEMA")}},
  {OBJECT_TYPE_COMMIT, {C_STRING_WITH_LEN("COMMIT")}},
  {OBJECT_TYPE_USER_LEVEL_LOCK, {C_STRING_WITH_LEN("USER LEVEL LOCK")}},
  {OBJECT_TYPE_TABLESPACE, {C_STRING_WITH_LEN("TABLESPACE")}},
  {OBJECT_TYPE_LOCKING_SERVICE, {C_STRING_WITH_LEN("LOCKING SERVICE")}},
  {OBJECT_TYPE_LOCKING_SERVICE, {C_STRING_WITH_LEN("ACL CACHE")}},
  {NO_OBJECT_TYPE, {C_STRING_WITH_LEN("")}}};

void
object_type_to_string(enum_object_type object_type,
                      const char **string,
                      size_t *length)
{
  s_object_type_map *map;

  static_assert(array_elements(object_type_map) == COUNT_OBJECT_TYPE + 1, "");

  for (map = &object_type_map[0]; map->m_enum != NO_OBJECT_TYPE; map++)
  {
    if (map->m_enum == object_type)
    {
      *string = map->m_string.str;
      *length = map->m_string.length;
      return;
    }
  }

  *string = map->m_string.str;
  *length = map->m_string.length;
  return;
}

void
string_to_object_type(const char *string,
                      size_t length,
                      enum_object_type *object_type)
{
  s_object_type_map *map;

  for (map = &object_type_map[0]; map->m_enum != NO_OBJECT_TYPE; map++)
  {
    if (map->m_string.length == length)
    {
      if (native_strncasecmp(map->m_string.str, string, length) == 0)
      {
        *object_type = map->m_enum;
        return;
      }
    }
  }

  *object_type = map->m_enum;
  return;
}
