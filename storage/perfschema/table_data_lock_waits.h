/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_DATA_LOCK_WAITS__H
#define TABLE_DATA_LOCK_WAITS__H

/**
  @file storage/perfschema/table_data_lock_waits.h
  Table DATA_LOCK_WAITS (declarations).
*/

#include <sys/types.h>

#include "storage/perfschema/pfs.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_data_lock.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

struct PFS_data_lock_waits;
class PFS_index_data_lock_waits;

/**
  @addtogroup Performance_schema_tables
  @{
*/

/**
  Position of a cursor on
  PERFORMANCE_SCHEMA.DATA_LOCK_WAITS.
  Index 1 on engine (0 based)
  Index 2 on engine index (0 based)
*/
struct scan_pos_data_lock_wait
{
  scan_pos_data_lock_wait()
  {
    m_index_1 = 0;
    m_index_2 = 0;
  }

  inline void
  reset(void)
  {
    m_index_1 = 0;
    m_index_2 = 0;
  }

  void
  set_at(const scan_pos_data_lock_wait *other)
  {
    m_index_1 = other->m_index_1;
    m_index_2 = other->m_index_2;
  }

  void
  set_after(const scan_pos_data_lock_wait *other)
  {
    m_index_1 = other->m_index_1;
    m_index_2 = other->m_index_2 + 1;
  }

  inline bool
  has_more_engine()
  {
    return (m_index_1 < COUNT_DATA_LOCK_ENGINES);
  }

  inline void
  next_engine()
  {
    m_index_1++;
    m_index_2 = 0;
  }

  unsigned int m_index_1;
  unsigned int m_index_2;
};

/** Table PERFORMANCE_SCHEMA.DATA_LOCKS. */
class table_data_lock_waits : public PFS_engine_table
{
  typedef scan_pos_data_lock_wait scan_pos_t;
  typedef pk_pos_data_lock_wait pk_pos_t;

public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

private:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_data_lock_waits();

public:
  ~table_data_lock_waits();

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_data_lock_wait *m_row;
  /** Current scan position. */
  scan_pos_t m_pos;
  /** Next scan position. */
  scan_pos_t m_next_pos;
  /** Current pk position. */
  pk_pos_t m_pk_pos;

  PFS_data_lock_wait_container m_container;
  PSI_engine_data_lock_wait_iterator *m_iterator[COUNT_DATA_LOCK_ENGINES];

  PFS_index_data_lock_waits *m_opened_index;
};

/** @} */
#endif
