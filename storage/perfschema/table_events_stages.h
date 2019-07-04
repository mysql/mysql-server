/* Copyright (c) 2010, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_EVENTS_STAGES_H
#define TABLE_EVENTS_STAGES_H

/**
  @file storage/perfschema/table_events_stages.h
  Table EVENTS_STAGES_xxx (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct PFS_events_stages;
struct PFS_thread;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_events_stages : public PFS_engine_index {
 public:
  PFS_index_events_stages()
      : PFS_engine_index(&m_key_1, &m_key_2),
        m_key_1("THREAD_ID"),
        m_key_2("EVENT_ID") {}

  ~PFS_index_events_stages() {}

  bool match(PFS_thread *pfs);
  bool match(PFS_events_stages *pfs);

 private:
  PFS_key_thread_id m_key_1;
  PFS_key_event_id m_key_2;
};

/** A row of table_events_stages_common. */
struct row_events_stages {
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
struct pos_events_stages_history : public PFS_double_index {
  pos_events_stages_history() : PFS_double_index(0, 0) {}

  inline void reset(void) {
    m_index_1 = 0;
    m_index_2 = 0;
  }

  inline void next_thread(void) {
    m_index_1++;
    m_index_2 = 0;
  }
};

/**
  Adapter, for table sharing the structure of
  PERFORMANCE_SCHEMA.EVENTS_STAGES_CURRENT.
*/
class table_events_stages_common : public PFS_engine_table {
 protected:
  virtual int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                              bool read_all);

  table_events_stages_common(const PFS_engine_table_share *share, void *pos);

  ~table_events_stages_common() {}

  int make_row(PFS_events_stages *stage);

  /** Current row. */
  row_events_stages m_row;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STAGES_CURRENT. */
class table_events_stages_current : public table_events_stages_common {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_init(bool scan);
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

 protected:
  table_events_stages_current();

 public:
  ~table_events_stages_current() {}

 private:
  friend class table_events_stages_history;
  friend class table_events_stages_history_long;

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  PFS_index_events_stages *m_opened_index;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STAGES_HISTORY. */
class table_events_stages_history : public table_events_stages_common {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_init(bool scan);
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

 protected:
  table_events_stages_history();

 public:
  ~table_events_stages_history() {}

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current position. */
  pos_events_stages_history m_pos;
  /** Next position. */
  pos_events_stages_history m_next_pos;

  PFS_index_events_stages *m_opened_index;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_STAGES_HISTORY_LONG. */
class table_events_stages_history_long : public table_events_stages_common {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual int rnd_init(bool scan);
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

 protected:
  table_events_stages_history_long();

 public:
  ~table_events_stages_history_long() {}

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
