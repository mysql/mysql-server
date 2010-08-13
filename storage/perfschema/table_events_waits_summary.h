/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_EVENTS_WAITS_SUMMARY_H
#define TABLE_EVENTS_WAITS_SUMMARY_H

/**
  @file storage/perfschema/table_events_waits_summary.h
  Table EVENTS_WAITS_SUMMARY_BY_xxx (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "table_all_instr.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/**
  A row of table
  PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME.
*/
struct row_events_waits_summary_by_thread_by_event_name
{
  /** Column THREAD_ID. */
  ulong m_thread_internal_id;
  /** Column EVENT_NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column COUNT_STAR. */
  ulonglong m_count;
  /** Column SUM_TIMER_WAIT. */
  ulonglong m_sum;
  /** Column MIN_TIMER_WAIT. */
  ulonglong m_min;
  /** Column AVG_TIMER_WAIT. */
  ulonglong m_avg;
  /** Column MAX_TIMER_WAIT. */
  ulonglong m_max;
};

/**
  Position of a cursor on
  PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME.
*/
struct pos_events_waits_summary_by_thread_by_event_name
: public PFS_triple_index, public PFS_instrument_view_constants
{
  pos_events_waits_summary_by_thread_by_event_name()
    : PFS_triple_index(0, VIEW_MUTEX, 1)
  {}

  inline void reset(void)
  {
    m_index_1= 0;
    m_index_2= VIEW_MUTEX;
    m_index_3= 1;
  }

  inline bool has_more_thread(void)
  { return (m_index_1 < thread_max); }

  inline bool has_more_view(void)
  { return (m_index_2 <= VIEW_FILE); }

  inline void next_thread(void)
  {
    m_index_1++;
    m_index_2= VIEW_MUTEX;
    m_index_3= 1;
  }

  inline void next_view(void)
  {
    m_index_2++;
    m_index_3= 1;
  }
};

/** Table PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME. */
class table_events_waits_summary_by_thread_by_event_name
  : public PFS_engine_table
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

  table_events_waits_summary_by_thread_by_event_name();

public:
  ~table_events_waits_summary_by_thread_by_event_name()
  {}

protected:
  void make_instr_row(PFS_thread *thread, PFS_instr_class *klass,
                      PFS_single_stat_chain *stat);
  void make_mutex_row(PFS_thread *thread, PFS_mutex_class *klass);
  void make_rwlock_row(PFS_thread *thread, PFS_rwlock_class *klass);
  void make_cond_row(PFS_thread *thread, PFS_cond_class *klass);
  void make_file_row(PFS_thread *thread, PFS_file_class *klass);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_events_waits_summary_by_thread_by_event_name m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  pos_events_waits_summary_by_thread_by_event_name m_pos;
  /** Next position. */
  pos_events_waits_summary_by_thread_by_event_name m_next_pos;
};

/** A row of PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_BY_INSTANCE. */
struct row_events_waits_summary_by_instance
{
  /** Column EVENT_NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column OBJECT_INSTANCE_BEGIN. */
  intptr m_object_instance_addr;
  /** Column COUNT_STAR. */
  ulonglong m_count;
  /** Column SUM_TIMER_WAIT. */
  ulonglong m_sum;
  /** Column MIN_TIMER_WAIT. */
  ulonglong m_min;
  /** Column AVG_TIMER_WAIT. */
  ulonglong m_avg;
  /** Column MAX_TIMER_WAIT. */
  ulonglong m_max;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_BY_INSTANCE. */
class table_events_waits_summary_by_instance : public table_all_instr
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static int delete_all_rows();

protected:
  void make_instr_row(PFS_instr *pfs, PFS_instr_class *klass,
                      const void *object_instance_begin);
  virtual void make_mutex_row(PFS_mutex *pfs);
  virtual void make_rwlock_row(PFS_rwlock *pfs);
  virtual void make_cond_row(PFS_cond *pfs);
  virtual void make_file_row(PFS_file *pfs);

  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_events_waits_summary_by_instance();

public:
  ~table_events_waits_summary_by_instance()
  {}

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_events_waits_summary_by_instance m_row;
  /** True is the current row exists. */
  bool m_row_exists;
};

/** @} */
#endif
