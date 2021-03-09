/* Copyright (c) 2013, 2021, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef TABLE_ESMS_BY_PROGRAM_H
#define TABLE_ESMS_BY_PROGRAM_H

/**
  @file storage/perfschema/table_esms_by_program.h
  Table EVENTS_STATEMENTS_SUMMARY_BY_PROGRAM (declarations).
*/

#include "table_helper.h"
#include "pfs_program.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/**
  A row of table
  PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_SUMMARY_BY_PROGRAM.
*/
struct row_esms_by_program
{
  /** Column OBJECT_TYPE. */
  enum_object_type m_object_type;
  /** Column OBJECT_SCHEMA. */
  char m_schema_name[COL_OBJECT_SCHEMA_SIZE];
  int m_schema_name_length;
  /** Column OBJECT_NAME. */
  char m_object_name[COL_OBJECT_NAME_SIZE];
  int m_object_name_length;

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
class table_esms_by_program : public PFS_engine_table
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_esms_by_program();

public:
  ~table_esms_by_program()
  {}

protected:
  void make_row(PFS_program*);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_esms_by_program m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
