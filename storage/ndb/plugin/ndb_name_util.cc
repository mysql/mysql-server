/*
   Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include "storage/ndb/plugin/ndb_name_util.h"

#include "m_string.h"       // strend()
#include "sql/sql_table.h"  // filename_to_table_name()
#include "sql/table.h"      // tmp_file_prefix
/**
  Set a given location from full pathname to database name.
*/

void ndb_set_dbname(const char *path_name, char *dbname) {
  const char *end, *ptr;
  char tmp_buff[FN_REFLEN + 1];

  char *tmp_name = tmp_buff;
  /* Scan name from the end */
  ptr = strend(path_name) - 1;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  ptr--;
  end = ptr;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  uint name_len = (uint)(end - ptr);
  memcpy(tmp_name, ptr + 1, name_len);
  tmp_name[name_len] = '\0';
  filename_to_tablename(tmp_name, dbname, sizeof(tmp_buff) - 1);
}

/**
  Set a given location from full pathname to table file.
*/

void ndb_set_tabname(const char *path_name, char *tabname) {
  const char *end, *ptr;
  char tmp_buff[FN_REFLEN + 1];

  char *tmp_name = tmp_buff;
  /* Scan name from the end */
  end = strend(path_name) - 1;
  ptr = end;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  uint name_len = (uint)(end - ptr);
  memcpy(tmp_name, ptr + 1, end - ptr);
  tmp_name[name_len] = '\0';
  filename_to_tablename(tmp_name, tabname, sizeof(tmp_buff) - 1);
}

bool ndb_name_is_temp(const char *name) {
  return is_prefix(name, tmp_file_prefix) == 1;
}

bool ndb_name_is_blob_prefix(const char *name) {
  return is_prefix(name, "NDB$BLOB");
}

bool ndb_name_is_fk_mock_prefix(const char *name) {
  return is_prefix(name, "NDB$FKM");
}
