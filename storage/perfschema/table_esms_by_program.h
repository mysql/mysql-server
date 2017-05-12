/* Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef TABLE_ESMS_BY_PROGRAM_H
#define TABLE_ESMS_BY_PROGRAM_H

/**
  @file storage/perfschema/table_esms_by_program.h
  Table EVENTS_STATEMENTS_SUMMARY_BY_PROGRAM (declarations).
*/

#include <sys/types.h>

#include "pfs_program.h"
#include "table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_esms_by_program : public PFS_engine_index
{
public:
  PFS_index_esms_by_program()
    : PFS_engine_index(&m_key_1, &m_key_2, &m_key_3),
      m_key_1("OBJECT_TYPE"),
      m_key_2("OBJECT_SCHEMA"),
      m_key_3("OBJECT_NAME")
  {
  }

  ~PFS_index_esms_by_program()
  {
  }

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
  static PFS_engine_table *create();
  static int delete_all_rows();
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

  table_esms_by_program();

public:
  ~table_esms_by_program()
  {
  }

protected:
  int make_row(PFS_program *);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

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
