/* Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_SETUP_CONSUMERS_H
#define TABLE_SETUP_CONSUMERS_H

/**
  @file storage/perfschema/table_setup_consumers.h
  Table SETUP_CONSUMERS (declarations).
*/

#include <sys/types.h>

#include "lex_string.h"
#include "my_base.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.SETUP_CONSUMERS. */
struct row_setup_consumers
{
  /** Column NAME. */
  LEX_STRING m_name;
  /** Column ENABLED. */
  bool *m_enabled_ptr;
  /** Hidden column, instrument refresh. */
  bool m_instrument_refresh;
  /** Hidden column, thread refresh. */
  bool m_thread_refresh;
};

class PFS_index_setup_consumers : public PFS_engine_index
{
public:
  PFS_index_setup_consumers() : PFS_engine_index(&m_key), m_key("NAME")
  {
  }

  ~PFS_index_setup_consumers()
  {
  }

  virtual bool match(row_setup_consumers *row);

private:
  PFS_key_name m_key;
};

/** Table PERFORMANCE_SCHEMA.SETUP_CONSUMERS. */
class table_setup_consumers : public PFS_engine_table
{
public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
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
  table_setup_consumers();

public:
  ~table_setup_consumers()
  {
  }

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_setup_consumers *m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  PFS_index_setup_consumers *m_opened_index;
};

/** @} */
#endif
