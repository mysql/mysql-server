/* Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TABLE_SETUP_THREADS_H
#define TABLE_SETUP_THREADS_H

/**
  @file storage/perfschema/table_setup_threads.h
  Table SETUP_THREADS (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct PFS_instr_class;
struct PFS_thread_class;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.SETUP_THREADS. */
struct row_setup_threads {
  /** Columns NAME, ENABLED, HISTORY, PROPERTIES, VOLATILITY, DOCUMENTATION. */
  PFS_thread_class *m_instr_class;
};

class PFS_index_setup_threads : public PFS_engine_index {
 public:
  PFS_index_setup_threads() : PFS_engine_index(&m_key), m_key("NAME") {}

  ~PFS_index_setup_threads() {}

  bool match(PFS_instr_class *klass);

 private:
  PFS_key_event_name m_key;
};

/** Table PERFORMANCE_SCHEMA.SETUP_INSTRUMENTS. */
class table_setup_threads : public PFS_engine_table {
  typedef PFS_simple_index pos_t;

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
  virtual int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                              bool read_all);

  virtual int update_row_values(TABLE *table, const unsigned char *old_buf,
                                unsigned char *new_buf, Field **fields);

  table_setup_threads();

 public:
  ~table_setup_threads() {}

 private:
  int make_row(PFS_thread_class *klass);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_setup_threads m_row;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

  PFS_index_setup_threads *m_opened_index;
};

/** @} */
#endif
