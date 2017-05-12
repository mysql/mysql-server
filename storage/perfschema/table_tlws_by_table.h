/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
  */

#ifndef TABLE_LOCK_WAITS_SUMMARY_BY_TABLE_H
#define TABLE_LOCK_WAITS_SUMMARY_BY_TABLE_H

/**
  @file storage/perfschema/table_tlws_by_table.h
  Table TABLE_LOCK_WAITS_SUMMARY_BY_TABLE (declarations).
*/

#include <sys/types.h>

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr.h"
#include "pfs_instr_class.h"
#include "table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  A row of table
  PERFORMANCE_SCHEMA.TABLE_LOCK_WAITS_SUMMARY_BY_TABLE.
*/
struct row_tlws_by_table
{
  /** Column OBJECT_TYPE, SCHEMA_NAME, OBJECT_NAME. */
  PFS_object_row m_object;
  /** Columns COUNT/SUM/MIN/AVG/MAX READ/WRITE/READ_NORMAL/etc. */
  PFS_table_lock_stat_row m_stat;
};

class PFS_index_tlws_by_table : public PFS_engine_index
{
public:
  PFS_index_tlws_by_table()
    : PFS_engine_index(&m_key_1, &m_key_2, &m_key_3),
      m_key_1("OBJECT_TYPE"),
      m_key_2("OBJECT_SCHEMA"),
      m_key_3("OBJECT_NAME")
  {
  }

  ~PFS_index_tlws_by_table()
  {
  }

  virtual bool match(const PFS_table_share *table);

private:
  PFS_key_object_type m_key_1;
  PFS_key_object_schema m_key_2;
  PFS_key_object_name m_key_3;
};

/** Table PERFORMANCE_SCHEMA.TABLE_LOCK_WAITS_SUMMARY_BY_TABLE. */
class table_tlws_by_table : public PFS_engine_table
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create();
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_init(bool scan);
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);
  table_tlws_by_table();

public:
  ~table_tlws_by_table()
  {
  }

protected:
  int make_row(PFS_table_share *table_share);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_tlws_by_table m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

protected:
  PFS_index_tlws_by_table *m_opened_index;
};

/** @} */
#endif
