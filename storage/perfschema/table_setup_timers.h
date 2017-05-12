/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_SETUP_TIMERS_H
#define TABLE_SETUP_TIMERS_H

/**
  @file storage/perfschema/table_setup_timers.h
  Table SETUP_TIMERS (declarations).
*/

#include <sys/types.h>

#include "lex_string.h"
#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of table PERFORMANCE_SCHEMA.SETUP_TIMERS. */
struct row_setup_timers
{
  /** Column NAME. */
  LEX_STRING m_name;
  /** Column TIMER_NAME. */
  enum_timer_name *m_timer_name_ptr;
};

class PFS_index_setup_timers : public PFS_engine_index
{
public:
  PFS_index_setup_timers() : PFS_engine_index(&m_key), m_key("NAME")
  {
  }

  ~PFS_index_setup_timers()
  {
  }

  virtual bool match(row_setup_timers *row);

private:
  PFS_key_name m_key;
};

/** Table PERFORMANCE_SCHEMA.SETUP_TIMERS. */
class table_setup_timers : public PFS_engine_table
{
public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create();
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  virtual int update_row_values(TABLE *table,
                                const unsigned char *old_buf,
                                unsigned char *new_buf,
                                Field **fields);
  table_setup_timers();

public:
  ~table_setup_timers()
  {
  }

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_setup_timers *m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  PFS_index_setup_timers *m_opened_index;
};

/** @} */
#endif
