/* Copyright (C) 2008-2009 Sun Microsystems, Inc

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef TABLE_PROCESSIST_H
#define TABLE_PROCESSIST_H

/**
  @file storage/perfschema/table_processlist.h
  Table PROCESSLIST (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"

struct PFS_thread;

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.PROCESSLIST. */
struct row_processlist
{
  /** Column THREAD_ID. */
  ulong m_thread_internal_id;
  /** Column ID. */
  ulong m_thread_id;
  /** Column NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
};

/** Table PERFORMANCE_SCHEMA.PROCESSLIST. */
class table_processlist : public PFS_readonly_table
{
public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

protected:
  table_processlist();

public:
  ~table_processlist()
  {}

private:
  void make_row(PFS_thread *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_processlist m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
