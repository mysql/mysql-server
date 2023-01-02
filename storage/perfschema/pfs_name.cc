/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
  @file storage/perfschema/pfs_name.cc
  Object name (implementation).
*/

#include <string.h>  // memcpy

#include "sql/mysqld.h"  // lower_case_table_names
#include "storage/perfschema/pfs_name.h"

void PFS_schema_name::set(const char *str, size_t len) {
  m_name.set(str, len);

  if (lower_case_table_names >= 1) {
    m_name.casedn(get_cs());
  }
}

const CHARSET_INFO *PFS_schema_name::get_cs() {
  return &my_charset_utf8mb4_0900_bin;
}

void PFS_table_name::set(const char *str, size_t len) {
  m_name.set(str, len);

  if (lower_case_table_names >= 1) {
    m_name.casedn(get_cs());
  }
}

const CHARSET_INFO *PFS_table_name::get_cs() {
  return &my_charset_utf8mb4_0900_bin;
}

void PFS_routine_name::set(const char *str, size_t len) {
  m_name.set(str, len);
}

const CHARSET_INFO *PFS_routine_name::m_cs = &my_charset_utf8mb4_0900_ai_ci;

/* Same as PFS_table_name::set() */
void PFS_object_name::set_as_table(const char *str, size_t len) {
  m_name.set(str, len);

  if (lower_case_table_names >= 1) {
    m_name.casedn(PFS_table_name::get_cs());
  }
}

/* Same as PFS_routine_name::set() */
void PFS_object_name::set_as_routine(const char *str, size_t len) {
  m_name.set(str, len);
}

void PFS_user_name::set(const char *str, size_t len) { m_name.set(str, len); }

const CHARSET_INFO *PFS_user_name::m_cs = &my_charset_utf8mb4_bin;

void PFS_host_name::set(const char *str, size_t len) { m_name.set(str, len); }

const CHARSET_INFO *PFS_host_name::m_cs = &my_charset_utf8mb4_bin;

void PFS_role_name::set(const char *str, size_t len) { m_name.set(str, len); }

const CHARSET_INFO *PFS_role_name::m_cs = &my_charset_utf8mb4_bin;

void PFS_file_name::set(const char *str, size_t len) { m_name.set(str, len); }

const CHARSET_INFO *PFS_file_name::m_cs = &my_charset_bin;
