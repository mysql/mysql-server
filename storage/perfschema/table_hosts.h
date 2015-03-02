/* Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_HOSTS_H
#define TABLE_HOSTS_H

#include "pfs_column_types.h"
#include "cursor_by_host.h"
#include "table_helper.h"

struct PFS_host;

/**
  \addtogroup Performance_schema_tables
  @{
*/

/**
  A row of PERFORMANCE_SCHEMA.HOSTS.
*/
struct row_hosts
{
  /** Column HOST. */
  PFS_host_row m_host;
  /** Columns CURRENT_CONNECTIONS, TOTAL_CONNECTIONS. */
  PFS_connection_stat_row m_connection_stat;
};

/** Table PERFORMANCE_SCHEMA.THREADS. */
class table_hosts : public cursor_by_host
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  /** Table builder */
  static PFS_engine_table* create();
  static int delete_all_rows();

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);


protected:
  table_hosts();

public:
  ~table_hosts()
  {}

private:
  virtual void make_row(PFS_host *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_hosts m_row;
  /** True if the current row exists. */
  bool m_row_exists;
};

/** @} */
#endif
