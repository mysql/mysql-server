/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/pfs_column_types.cc
  Literal values for columns used in the performance
  schema tables (implementation).
*/

#include "storage/perfschema/pfs_column_types.h"

#include "lex_string.h"
#include "m_string.h" /* LEX_STRING */
#include "string_with_len.h"
#include "template_utils.h"

struct s_object_type_map {
  enum_object_type m_enum;
  LEX_CSTRING m_string;
};

static s_object_type_map object_type_map[] = {
    {OBJECT_TYPE_EVENT, {STRING_WITH_LEN("EVENT")}},
    {OBJECT_TYPE_FUNCTION, {STRING_WITH_LEN("FUNCTION")}},
    {OBJECT_TYPE_PROCEDURE, {STRING_WITH_LEN("PROCEDURE")}},
    {OBJECT_TYPE_TABLE, {STRING_WITH_LEN("TABLE")}},
    {OBJECT_TYPE_TEMPORARY_TABLE, {STRING_WITH_LEN("TEMPORARY TABLE")}},
    {OBJECT_TYPE_TRIGGER, {STRING_WITH_LEN("TRIGGER")}},
    {OBJECT_TYPE_GLOBAL, {STRING_WITH_LEN("GLOBAL")}},
    {OBJECT_TYPE_SCHEMA, {STRING_WITH_LEN("SCHEMA")}},
    {OBJECT_TYPE_COMMIT, {STRING_WITH_LEN("COMMIT")}},
    {OBJECT_TYPE_USER_LEVEL_LOCK, {STRING_WITH_LEN("USER LEVEL LOCK")}},
    {OBJECT_TYPE_TABLESPACE, {STRING_WITH_LEN("TABLESPACE")}},
    {OBJECT_TYPE_LOCKING_SERVICE, {STRING_WITH_LEN("LOCKING SERVICE")}},
    {OBJECT_TYPE_SRID, {STRING_WITH_LEN("SRID")}},
    {OBJECT_TYPE_ACL_CACHE, {STRING_WITH_LEN("ACL CACHE")}},
    {OBJECT_TYPE_COLUMN_STATISTICS, {STRING_WITH_LEN("COLUMN STATISTICS")}},
    {OBJECT_TYPE_BACKUP_LOCK, {STRING_WITH_LEN("BACKUP LOCK")}},
    {OBJECT_TYPE_RESOURCE_GROUPS, {STRING_WITH_LEN("RESOURCE_GROUPS")}},
    {OBJECT_TYPE_FOREIGN_KEY, {STRING_WITH_LEN("FOREIGN KEY")}},
    {OBJECT_TYPE_CHECK_CONSTRAINT, {STRING_WITH_LEN("CHECK CONSTRAINT")}},
    {NO_OBJECT_TYPE, {STRING_WITH_LEN("")}}};

void object_type_to_string(enum_object_type object_type, const char **string,
                           size_t *length) {
  s_object_type_map *map;

  static_assert(array_elements(object_type_map) == COUNT_OBJECT_TYPE + 1);

  for (map = &object_type_map[0]; map->m_enum != NO_OBJECT_TYPE; map++) {
    if (map->m_enum == object_type) {
      *string = map->m_string.str;
      *length = map->m_string.length;
      return;
    }
  }

  *string = map->m_string.str;
  *length = map->m_string.length;
}

void string_to_object_type(const char *string, size_t length,
                           enum_object_type *object_type) {
  s_object_type_map *map;

  for (map = &object_type_map[0]; map->m_enum != NO_OBJECT_TYPE; map++) {
    if (map->m_string.length == length) {
      if (native_strncasecmp(map->m_string.str, string, length) == 0) {
        *object_type = map->m_enum;
        return;
      }
    }
  }

  *object_type = map->m_enum;
}
