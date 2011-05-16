/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_ESMS_BY_DIGEST_H
#define TABLE_ESMS_BY_DIGEST_H

/**
  @file storage/perfschema/table_esms_by_digest.h
  Table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST (declarations).
*/

#include "table_helper.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/**
  A row of table
  PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_SUMMARY_BY_DIGEST.
*/
struct row_esms_by_digest
{
  /** Columns TBD. Adding few dummy as of now. */
   
  /** Column DIGEST. */
  char m_digest[COL_DIGEST_SIZE];

  /** Column DIGEST_TEXT. */
  char m_digest_text[COL_DIGEST_TEXT_SIZE];

  /** Columns COUNT_STAR, SUM/MIN/AVG/MAX TIMER_WAIT. */
  PFS_statement_stat_row m_stat;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_SUMMARY_BY_DIGEST. */
class table_esms_by_digest : public PFS_engine_table
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static int delete_all_rows();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_esms_by_digest();

public:
  ~table_esms_by_digest()
  {}

protected:
  void make_row(/*TBD*/);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_esms_by_digest m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
