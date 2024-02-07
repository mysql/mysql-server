/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

// Implements
#include "storage/ndb/plugin/ndb_pfs_table.h"

#include <assert.h>
// assert

static int ndb_pfs_rnd_init(PSI_table_handle *handle, bool) {
  Ndb_pfs_table *table = reinterpret_cast<Ndb_pfs_table *>(handle);
  return (table->rnd_init());
}

static int ndb_pfs_rnd_next(PSI_table_handle *handle) {
  Ndb_pfs_table *table = reinterpret_cast<Ndb_pfs_table *>(handle);
  return (table->rnd_next());
}

static int ndb_pfs_rnd_pos(PSI_table_handle *handle) {
  Ndb_pfs_table *table = reinterpret_cast<Ndb_pfs_table *>(handle);
  return (table->rnd_pos());
}

static void ndb_pfs_reset_pos(PSI_table_handle *handle) {
  Ndb_pfs_table *table = reinterpret_cast<Ndb_pfs_table *>(handle);
  table->reset_pos();
}

static int ndb_pfs_read_column(PSI_table_handle *handle, PSI_field *field,
                               unsigned int index) {
  Ndb_pfs_table *table = reinterpret_cast<Ndb_pfs_table *>(handle);
  return (table->read_column_value(field, index));
}

static void ndb_pfs_close_table(PSI_table_handle *handle) {
  Ndb_pfs_table *table = reinterpret_cast<Ndb_pfs_table *>(handle);
  table->close();
  // Delete table which was allocated during table open
  delete table;
}

Ndb_pfs_table_share::Ndb_pfs_table_share() {
  // Table specific information that must be set by each table implementation
  m_table_name = "";
  m_table_name_length = 0;
  m_table_definition = "";

  // Table information that should hold true for all ndbcluster PFS tables
  m_ref_length = sizeof(unsigned int);
  m_acl = READONLY;
  delete_all_rows = nullptr;

  // Proxy table access functions
  PFS_engine_table_proxy &proxy_table = m_proxy_engine_table;

  // The open table function is table specific
  proxy_table.open_table = nullptr;
  proxy_table.close_table = ndb_pfs_close_table;

  // Table scan functions
  proxy_table.rnd_init = ndb_pfs_rnd_init;
  proxy_table.rnd_next = ndb_pfs_rnd_next;
  proxy_table.rnd_pos = ndb_pfs_rnd_pos;

  // Table read operations
  proxy_table.read_column_value = ndb_pfs_read_column;
  proxy_table.reset_position = ndb_pfs_reset_pos;

  // Table index scan, currently not required
  proxy_table.index_init = nullptr;
  proxy_table.index_read = nullptr;
  proxy_table.index_next = nullptr;

  // Table write operations, currently not required
  proxy_table.write_column_value = nullptr;
  proxy_table.write_row_values = nullptr;
  proxy_table.update_column_value = nullptr;
  proxy_table.update_row_values = nullptr;
  proxy_table.delete_row_values = nullptr;
}

int Ndb_pfs_table::rnd_next() {
  if (is_empty()) {
    // No rows
    return PFS_HA_ERR_END_OF_FILE;
  }
  m_position++;
  if (rows_pending_read()) {
    return 0;
  }
  assert(all_rows_read());
  return PFS_HA_ERR_END_OF_FILE;
}
