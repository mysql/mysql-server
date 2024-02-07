/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  */

#ifndef TABLE_OBJECTS_SUMMARY_GLOBAL_BY_TYPE_H
#define TABLE_OBJECTS_SUMMARY_GLOBAL_BY_TYPE_H

/**
  @file storage/perfschema/table_os_global_by_type.h
  Table OBJECTS_SUMMARY_GLOBAL_BY_TYPE (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct PFS_program;
struct PFS_table_share;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  A row of table
  PERFORMANCE_SCHEMA.OBJECTS_SUMMARY_GLOBAL_BY_TYPE.
*/
struct row_os_global_by_type {
  /** Column OBJECT_TYPE, SCHEMA_NAME, OBJECT_NAME. */
  PFS_object_row m_object;

  /** Columns COUNT_STAR, SUM/MIN/AVG/MAX TIMER_WAIT. */
  PFS_stat_row m_stat;
};

/**
  Position of a cursor on
  PERFORMANCE_SCHEMA.OBJECTS_SUMMARY_GLOBAL_BY_TYPE.
  Index 1 on object type
  Index 2 on object instance (0 based)
*/
struct pos_os_global_by_type : public PFS_double_index,
                               public PFS_object_view_constants {
  pos_os_global_by_type() : PFS_double_index(FIRST_VIEW, 0) {}

  inline void reset() {
    m_index_1 = FIRST_VIEW;
    m_index_2 = 0;
  }

  inline bool has_more_view() { return (m_index_1 <= LAST_VIEW); }

  inline void next_view() {
    m_index_1++;
    m_index_2 = 0;
  }
};

class PFS_index_os_global_by_type : public PFS_engine_index {
 public:
  PFS_index_os_global_by_type()
      : PFS_engine_index(&m_key_1, &m_key_2, &m_key_3),
        m_key_1("OBJECT_TYPE"),
        m_key_2("OBJECT_SCHEMA"),
        m_key_3("OBJECT_NAME") {}

  ~PFS_index_os_global_by_type() override = default;

  virtual bool match(PFS_table_share *pfs);
  virtual bool match(PFS_program *pfs);

 private:
  PFS_key_object_type m_key_1;
  PFS_key_object_schema m_key_2;
  PFS_key_object_name m_key_3;
};

/** Table PERFORMANCE_SCHEMA.OBJECTS_SUMMARY_GLOBAL_BY_TYPE. */
class table_os_global_by_type : public PFS_engine_table {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  void reset_position() override;

  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;
  table_os_global_by_type();

 public:
  ~table_os_global_by_type() override = default;

 protected:
  int make_table_row(PFS_table_share *table_share);
  int make_program_row(PFS_program *pfs_program);

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_os_global_by_type m_row;
  /** Current position. */
  pos_os_global_by_type m_pos;
  /** Next position. */
  pos_os_global_by_type m_next_pos;

 protected:
  PFS_index_os_global_by_type *m_opened_index;
};

/** @} */
#endif
