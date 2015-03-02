/* Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TABLE_ETS_GLOBAL_BY_EVENT_NAME_H
#define TABLE_ETS_GLOBAL_BY_EVENT_NAME_H

/**
  @file storage/perfschema/table_ets_global_by_event_name.h
  Table EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "table_helper.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/**
  A row of table
  PERFORMANCE_SCHEMA.EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME.
*/
struct row_ets_global_by_event_name
{
  /** Column EVENT_NAME. */
  PFS_event_name_row m_event_name;
  /**
    Columns COUNT_STAR, SUM/MIN/AVG/MAX_TIMER_WAIT,
    COUNT_READ_WRITE, SUM/MIN/AVG/MAX_TIMER_READ_WRITE,
    COUNT_READ_ONLY, SUM/MIN/AVG/MAX_TIMER_READ_ONLY
  */
  PFS_transaction_stat_row m_stat;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME. */
class table_ets_global_by_event_name : public PFS_engine_table
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual int rnd_init(bool scan);
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_ets_global_by_event_name();

public:
  ~table_ets_global_by_event_name()
  {}

protected:
  void make_row(PFS_transaction_class *klass);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_ets_global_by_event_name m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
