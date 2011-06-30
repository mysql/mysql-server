/* Copyright (c) 2008 MySQL AB, 2010 Sun Microsystems, Inc.
   Use is subject to license terms.

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

#ifndef TABLE_SETUP_INSTRUMENTS_H
#define TABLE_SETUP_INSTRUMENTS_H

/**
  @file storage/perfschema/table_setup_instruments.h
  Table SETUP_INSTRUMENTS (declarations).
*/

#include "pfs_instr_class.h"
#include "pfs_engine_table.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.SETUP_INSTRUMENTS. */
struct row_setup_instruments
{
  /** Column NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column ENABLED. */
  bool *m_enabled_ptr;
  /** Column TIMED. */
  bool *m_timed_ptr;
};

/** Position of a cursor on PERFORMANCE_SCHEMA.SETUP_INSTRUMENTS. */
struct pos_setup_instruments : public PFS_double_index
{
  static const uint VIEW_MUTEX= 1;
  static const uint VIEW_RWLOCK= 2;
  static const uint VIEW_COND= 3;
  /** Reverved for WL#4674, PERFORMANCE_SCHEMA Setup For Actors. */
  static const uint VIEW_THREAD= 4;
  static const uint VIEW_FILE= 5;

  pos_setup_instruments()
    : PFS_double_index(VIEW_MUTEX, 1)
  {}

  inline void reset(void)
  {
    m_index_1= VIEW_MUTEX;
    m_index_2= 1;
  }

  inline bool has_more_view(void)
  { return (m_index_1 <= VIEW_FILE); }

  inline void next_view(void)
  {
    m_index_1++;
    m_index_2= 1;
  }
};

/** Table PERFORMANCE_SCHEMA.SETUP_INSTRUMENTS. */
class table_setup_instruments : public PFS_engine_table
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

  virtual int update_row_values(TABLE *table,
                                const unsigned char *old_buf,
                                unsigned char *new_buf,
                                Field **fields);

  table_setup_instruments();

public:
  ~table_setup_instruments()
  {}

private:
  void make_row(PFS_instr_class *klass);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_setup_instruments m_row;
  /** Current position. */
  pos_setup_instruments m_pos;
  /** Next position. */
  pos_setup_instruments m_next_pos;
};

/** @} */
#endif
