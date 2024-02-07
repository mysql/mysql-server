/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
  @file storage/perfschema/pfs_name.cc
  Object name (implementation).
*/

#include <string.h>  // memcpy

#include "sql/mysqld.h"  // lower_case_table_names
#include "storage/perfschema/pfs_name.h"

static void casedn(const CHARSET_INFO *cs, const char *name, size_t name_len,
                   char *buffer, size_t buffer_len,
                   const char **normalized_name, size_t *normalized_len) {
  if ((0 < name_len) && (name_len <= buffer_len)) {
    memcpy(buffer, name, name_len);

    *normalized_name = buffer;
    *normalized_len = cs->cset->casedn(cs, buffer, name_len, buffer, name_len);
  } else {
    *normalized_name = nullptr;
    *normalized_len = 0;
  }
}

void PFS_schema_name::normalize(const char *name, size_t name_len, char *buffer,
                                size_t buffer_len, const char **normalized_name,
                                size_t *normalized_len) {
  assert(normalized_name != nullptr);
  assert(normalized_len != nullptr);
  assert(buffer_len >= NAME_LEN);

  if (lower_case_table_names >= 1) {
    casedn(get_cs(), name, name_len, buffer, buffer_len, normalized_name,
           normalized_len);
  } else {
    *normalized_name = name;
    *normalized_len = name_len;
  }
}

void PFS_schema_name::set(const char *str, size_t len) {
  m_name.set(str, len);

  if (lower_case_table_names >= 1) {
    m_name.casedn(get_cs());
  }
}

const CHARSET_INFO *PFS_schema_name::get_cs() {
  return &my_charset_utf8mb4_0900_bin;
}

const CHARSET_INFO *PFS_schema_name_view::get_cs() {
  return PFS_schema_name::get_cs();
}

void PFS_table_name::normalize(const char *name, size_t name_len, char *buffer,
                               size_t buffer_len, const char **normalized_name,
                               size_t *normalized_len) {
  assert(normalized_name != nullptr);
  assert(normalized_len != nullptr);
  assert(buffer_len >= NAME_LEN);

  if (lower_case_table_names >= 1) {
    casedn(get_cs(), name, name_len, buffer, buffer_len, normalized_name,
           normalized_len);
  } else {
    *normalized_name = name;
    *normalized_len = name_len;
  }
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

const CHARSET_INFO *PFS_table_name_view::get_cs() {
  return PFS_table_name::get_cs();
}

void PFS_routine_name::normalize(const char *name, size_t name_len,
                                 char * /* buffer */, size_t /* buffer_len */,
                                 const char **normalized_name,
                                 size_t *normalized_len) {
  assert(normalized_name != nullptr);
  assert(normalized_len != nullptr);

  *normalized_name = name;
  *normalized_len = name_len;
}

void PFS_routine_name::set(const char *str, size_t len) {
  m_name.set(str, len);
}

const CHARSET_INFO *PFS_routine_name::get_cs() {
  return &my_charset_utf8mb4_0900_ai_ci;
}

const CHARSET_INFO *PFS_routine_name_view::get_cs() {
  return PFS_routine_name::get_cs();
}

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

void PFS_index_name::normalize(const char *name, size_t name_len,
                               char * /* buffer */, size_t /* buffer_len */,
                               const char **normalized_name,
                               size_t *normalized_len) {
  assert(normalized_name != nullptr);
  assert(normalized_len != nullptr);

  *normalized_name = name;
  *normalized_len = name_len;
}

void PFS_index_name::set(const char *str, size_t len) { m_name.set(str, len); }

const CHARSET_INFO *PFS_index_name::get_cs() {
  return &my_charset_utf8mb4_0900_bin;
}

const CHARSET_INFO *PFS_index_name_view::get_cs() {
  return PFS_index_name::get_cs();
}

void PFS_user_name::set(const char *str, size_t len) { m_name.set(str, len); }

const CHARSET_INFO *PFS_user_name::get_cs() { return &my_charset_utf8mb4_bin; }

const CHARSET_INFO *PFS_user_name_view::get_cs() {
  return PFS_user_name::get_cs();
}

void PFS_host_name::set(const char *str, size_t len) { m_name.set(str, len); }

const CHARSET_INFO *PFS_host_name::get_cs() { return &my_charset_utf8mb4_bin; }

const CHARSET_INFO *PFS_host_name_view::get_cs() {
  return PFS_host_name::get_cs();
}

void PFS_role_name::set(const char *str, size_t len) { m_name.set(str, len); }

const CHARSET_INFO *PFS_role_name::get_cs() { return &my_charset_utf8mb4_bin; }

const CHARSET_INFO *PFS_role_name_view::get_cs() {
  return PFS_role_name::get_cs();
}

void PFS_file_name::set(const char *str, size_t len) { m_name.set(str, len); }

const CHARSET_INFO *PFS_file_name::get_cs() { return &my_charset_bin; }

const CHARSET_INFO *PFS_file_name_view::get_cs() {
  return PFS_file_name::get_cs();
}
