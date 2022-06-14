/* Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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

#ifndef TABLE_ESMS_BY_PROGRAM_H
#define TABLE_ESMS_BY_PROGRAM_H

/**
  @file storage/perfschema/table_esms_by_program.h
  Table EVENTS_STATEMENTS_SUMMARY_BY_PROGRAM (declarations).
*/

#include <sys/types.h>

#include "storage/perfschema/pfs_program.h"
#include "storage/perfschema/table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_esms_by_program : public PFS_engine_index {
 public:
  PFS_index_esms_by_program()
      : PFS_engine_index(&m_key_1, &m_key_2, &m_key_3),
        m_key_1("OBJECT_TYPE"),
        m_key_2("OBJECT_SCHEMA"),
        m_key_3("OBJECT_NAME") {}

  ~PFS_index_esms_by_program() override = default;

  virtual bool match(PFS_program *pfs);

 private:
  PFS_key_object_type_enum m_key_1;
  PFS_key_object_schema m_key_2;
  PFS_key_object_name m_key_3;
};

/**
  A row of table
  PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_SUMMARY_BY_PROGRAM.
*/
struct row_esms_by_program {
  /** Column OBJECT_TYPE. */
  enum_object_type m_object_type;
  /** Column OBJECT_SCHEMA. */
  PFS_schema_name m_schema_name;
  /** Column OBJECT_NAME. */
  PFS_routine_name m_object_name;

  /**
    Columns COUNT_STAR
            SUM_TIMER_WAIT
            MIN_TIMER_WAIT
            AVG_TIMER_WAIT
            MAX_TIMER_WAIT
  */
  PFS_sp_stat_row m_sp_stat;

  /** Columns COUNT_STATEMENTS,SUM_STATEMENTS_WAIT...SUM_NO_GOOD_INDEX_USED. */
  PFS_statement_stat_row m_stmt_stat;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_SUMMARY_BY_PROGRAM. */
class table_esms_by_program : public PFS_engine_table {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  void reset_position(void) override;

  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

  table_esms_by_program();

 public:
  ~table_esms_by_program() override = default;

 protected:
  int make_row(PFS_program *);

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_esms_by_program m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  PFS_index_esms_by_program *m_opened_index;
};

/** @} */
#endif
