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

#ifndef TABLE_EWS_GLOBAL_BY_EVENT_NAME_H
#define TABLE_EWS_GLOBAL_BY_EVENT_NAME_H

/**
  @file storage/perfschema/table_ews_global_by_event_name.h
  Table EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME (declarations).
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
  PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME.
*/
struct row_ews_global_by_event_name
{
  /** Column EVENT_NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Columns COUNT_STAR, SUM/MIN/AVG/MAX TIMER_WAIT. */
  PFS_stat_row m_stat;
};

/**
  Position of a cursor on
  PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME.
  Index 1 on instrument view
  Index 2 on instrument class (1 based)
*/
struct pos_ews_global_by_event_name
: public PFS_double_index, public PFS_instrument_view_constants
{
  pos_ews_global_by_event_name()
    : PFS_double_index(FIRST_VIEW, 1)
  {}

  inline void reset(void)
  {
    m_index_1= FIRST_VIEW;
    m_index_2= 1;
  }

  inline bool has_more_view(void)
  { return (m_index_1 <= LAST_VIEW); }

  inline void next_view(void)
  {
    m_index_1++;
    m_index_2= 1;
  }

  inline void next_instrument(void)
  {
    m_index_2++;
  }
};

/** Table PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME. */
class table_ews_global_by_event_name : public PFS_engine_table
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

  table_ews_global_by_event_name();

public:
  ~table_ews_global_by_event_name()
  {}

protected:
  void make_mutex_row(PFS_mutex_class *klass);
  void make_rwlock_row(PFS_rwlock_class *klass);
  void make_cond_row(PFS_cond_class *klass);
  void make_file_row(PFS_file_class *klass);
  void make_table_io_row(PFS_instr_class *klass);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_ews_global_by_event_name m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  pos_ews_global_by_event_name m_pos;
  /** Next position. */
  pos_ews_global_by_event_name m_next_pos;
};

/** @} */
#endif
