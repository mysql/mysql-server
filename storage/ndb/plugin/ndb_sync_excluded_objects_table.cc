/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

// Implements
#include "storage/ndb/plugin/ndb_sync_excluded_objects_table.h"

#include <assert.h>
#include <cstdint>
#include <cstring>  // std::strlen

// assert
#include "storage/ndb/plugin/ha_ndbcluster_binlog.h"  // ndbcluster_binlog_retrieve_sync_excluded_objects

static unsigned long long ndb_excluded_objects_row_count() {
  return ndbcluster_binlog_get_sync_excluded_objects_count();
}

static PSI_table_handle *ndb_excluded_objects_open_table(PSI_pos **pos) {
  // Constructs a table object and returns an opaque pointer
  auto row_pos = reinterpret_cast<uint32_t **>(pos);
  /*
    Creates an instance of the table. Note that this is deallocated during the
    table close which is implemented in the base class. See the
    ndb_pfs_close_table() function in ndb_pfs_table.cc
  */
  Ndb_sync_excluded_objects_table *table = new Ndb_sync_excluded_objects_table;
  *row_pos = table->get_position_address();
  PSI_table_handle *handle = reinterpret_cast<PSI_table_handle *>(table);
  return handle;
}

Ndb_sync_excluded_objects_table_share::Ndb_sync_excluded_objects_table_share()
    : Ndb_pfs_table_share() {
  m_table_name = "ndb_sync_excluded_objects";
  m_table_name_length = std::strlen(m_table_name);
  m_table_definition =
      "`SCHEMA_NAME` varchar(64),"
      "`NAME` varchar(64),"
      "`TYPE` enum('LOGFILE GROUP', 'TABLESPACE', 'SCHEMA', 'TABLE') NOT NULL,"
      "`REASON` varchar(256) NOT NULL";
  get_row_count = ndb_excluded_objects_row_count;

  m_proxy_engine_table.open_table = ndb_excluded_objects_open_table;
}

int Ndb_sync_excluded_objects_table::rnd_init() {
  // Retrieve information and store it in m_excluded_objects
  ndbcluster_binlog_retrieve_sync_excluded_objects(this);
  set_num_rows(m_excluded_objects.size());
  reset_pos();
  return 0;
}

extern SERVICE_TYPE_NO_CONST(pfs_plugin_column_string_v2) * pfscol_string;
extern SERVICE_TYPE_NO_CONST(pfs_plugin_column_enum_v1) * pfscol_enum;

int Ndb_sync_excluded_objects_table::read_column_value(PSI_field *field,
                                                       unsigned int index) {
  assert(!is_empty() && rows_pending_read());
  PSI_ulonglong bigint_value;

  unsigned int row_index = get_position();
  const Excluded_object &obj = m_excluded_objects[row_index - 1];

  switch (index) {
    case 0: /* SCHEMA_NAME: Name of the schema */
      pfscol_string->set_varchar_utf8mb4(
          field, obj.m_schema_name == "" ? nullptr : obj.m_schema_name.c_str());
      break;
    case 1: /* NAME: Object name */
      pfscol_string->set_varchar_utf8mb4(
          field, obj.m_name == "" ? nullptr : obj.m_name.c_str());
      break;
    case 2: /* TYPE */
      // type + 1 since index 0 is used for empty strings in enum
      bigint_value.val = obj.m_type + 1;
      bigint_value.is_null = false;
      pfscol_enum->set(field, bigint_value);
      break;
    case 3: /* REASON: Reason for exclusion */
      // Check if reason is not empty
      assert(obj.m_reason != "");
      /*
        Check if reason has fewer than 256 characters. The PFS handler
        automatically truncates the reason if it has more than 256 characters
      */
      assert(std::strlen(obj.m_reason.c_str()) <= 256);
      pfscol_string->set_varchar_utf8mb4(
          field, obj.m_reason == "" ? "Unknown" : obj.m_reason.c_str());
      break;
    default:
      assert(false);
  }
  return 0;
}

void Ndb_sync_excluded_objects_table::close() {
  m_excluded_objects.clear();
  reset_pos();
}

// Instantiate the table share
Ndb_sync_excluded_objects_table_share excluded_objects_table_share;
PFS_engine_table_share_proxy *ndb_sync_excluded_objects_share =
    &excluded_objects_table_share;
