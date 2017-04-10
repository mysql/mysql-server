/* Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file storage/perfschema/table_hosts.h
  TABLE HOSTS.
*/

#include <sys/types.h>

#include "cursor_by_host.h"
#include "pfs_column_types.h"
#include "table_helper.h"

struct PFS_host;

/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_hosts_by_host : public PFS_index_hosts
{
public:
  PFS_index_hosts_by_host() : PFS_index_hosts(&m_key), m_key("HOST")
  {
  }

  ~PFS_index_hosts_by_host()
  {
  }

  virtual bool match(PFS_host *pfs);

private:
  PFS_key_host m_key;
};

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
  static PFS_engine_table *create();
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
  {
  }

  int index_init(uint idx, bool sorted);

private:
  virtual int make_row(PFS_host *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_hosts m_row;
};

/** @} */
#endif
