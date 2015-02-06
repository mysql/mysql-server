/* Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_EVENTS_STAGES_H
#define TABLE_EVENTS_STAGES_H

/**
  @file storage/perfschema/table_events_stages.h
  Table EVENTS_STAGES_xxx (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "pfs_events_stages.h"

struct PFS_thread;

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** A row of table_events_stages_common. */
struct row_events_stages
{
  /** Column THREAD_ID. */
  ulonglong m_thread_internal_id;
  /** Column EVENT_ID. */
  ulonglong m_event_id;
  /** Column END_EVENT_ID. */
  ulonglong m_end_event_id;
  /** Column NESTING_EVENT_ID. */
  ulonglong m_nesting_event_id;
  /** Column NESTING_EVENT_TYPE. */
  enum_event_type m_nesting_event_type;
  /** Column EVENT_NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column TIMER_START. */
  ulonglong m_timer_start;
  /** Column TIMER_END. */
  ulonglong m_timer_end;
  /** Column TIMER_WAIT. */
  ulonglong m_timer_wait;
  /** Column SOURCE. */
  char m_source[COL_SOURCE_SIZE];
  /** Length in bytes of @c m_source. */
  uint m_source_length;
  bool m_progress;
  /** Column WORK_COMPLETED. */
  ulonglong m_work_completed;
  /** Column WORK_ESTIMATED. */
  ulonglong m_work_estimated;
};

/** Position of a cursor on PERFORMANCE_SCHEMA.EVENTS_STAGES_HISTORY. */
struct pos_events_stages_history : public PFS_double_index
{
  pos_events_stages_history()
    : PFS_double_index(0, 0)
  {}

  inline void reset(void)
  {
    m_index_1= 0;
    m_index_2= 0;
  }

  inline void next_thread(void)
  {
    m_index_1++;
    m_index_2= 0;
  }
};

/**
  Adapter, for table sharing the structure of
  PERFORMANCE_SCHEMA.EVENTS_STAGES_CURRENT.
*/
class table_events_stages_common : public PFS_engine_table
{
protected:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_events_stages_common(const PFS_engine_table_share *share, void *pos);

  ~table_events_stages_common()
  {}

  void make_row(PFS_events_stages *stage);

  /** Current row. */
  row_events_stages m_row;
  /** True if the current row exists. */
  bool m_row_exists;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STAGES_CURRENT. */
class table_events_stages_current : public table_events_stages_common
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
  table_events_stages_current();

public:
  ~table_events_stages_current()
  {}

private:
  friend class table_events_stages_history;
  friend class table_events_stages_history_long;

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /**
    Fields definition.
    Also used by table_events_stages_history
    and table_events_stages_history_long.
  */
  static TABLE_FIELD_DEF m_field_def;

  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STAGES_HISTORY. */
class table_events_stages_history : public table_events_stages_common
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
  table_events_stages_history();

public:
  ~table_events_stages_history()
  {}

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;

  /** Current position. */
  pos_events_stages_history m_pos;
  /** Next position. */
  pos_events_stages_history m_next_pos;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STAGES_HISTORY_LONG. */
class table_events_stages_history_long : public table_events_stages_common
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
  table_events_stages_history_long();

public:
  ~table_events_stages_history_long()
  {}

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;

  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
