/* Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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

#ifndef TABLE_HOSTS_H
#define TABLE_HOSTS_H

/**
  @file storage/perfschema/table_hosts.h
  TABLE HOSTS.
*/

#include <sys/types.h>

#include "storage/perfschema/cursor_by_host.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/table_helper.h"

struct PFS_host;

/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_hosts_by_host : public PFS_index_hosts {
 public:
  PFS_index_hosts_by_host() : PFS_index_hosts(&m_key), m_key("HOST") {}

  ~PFS_index_hosts_by_host() override = default;

  bool match(PFS_host *pfs) override;

 private:
  PFS_key_host m_key;
};

/**
  A row of PERFORMANCE_SCHEMA.HOSTS.
*/
struct row_hosts {
  /** Column HOST. */
  PFS_host_row m_host;
  /** Columns CURRENT_CONNECTIONS, TOTAL_CONNECTIONS. */
  PFS_connection_stat_row m_connection_stat;
};

/** Table PERFORMANCE_SCHEMA.HOSTS. */
class table_hosts : public cursor_by_host {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  /** Table builder */
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

 protected:
  table_hosts();

 public:
  ~table_hosts() override = default;

  int index_init(uint idx, bool sorted) override;

 private:
  int make_row(PFS_host *pfs) override;

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_hosts m_row;
};

/** @} */
#endif
