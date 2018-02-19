/* Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_EVENTS_WAITS_SUMMARY_H
#define TABLE_EVENTS_WAITS_SUMMARY_H

/**
  @file storage/perfschema/table_events_waits_summary.h
  Table EVENTS_WAITS_SUMMARY_BY_xxx (declarations).
*/

#include <sys/types.h>

#include "my_inttypes.h"
#include "storage/perfschema/table_all_instr.h"
#include "storage/perfschema/table_helper.h"

class Field;
class PFS_engine_table;
class Plugin_table;
struct PFS_cond;
struct PFS_engine_table_share;
struct PFS_file;
struct PFS_instr;
struct PFS_instr_class;
struct PFS_mutex;
struct PFS_rwlock;
struct PFS_single_stat;
struct PFS_socket;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_BY_INSTANCE. */
struct row_events_waits_summary_by_instance {
  /** Column EVENT_NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column OBJECT_INSTANCE_BEGIN. */
  intptr m_object_instance_addr;
  /** Columns COUNT_STAR, SUM/MIN/AVG/MAX TIMER_WAIT. */
  PFS_stat_row m_stat;
};

class PFS_index_events_waits_summary_by_instance : public PFS_index_all_instr {
 public:
  PFS_index_events_waits_summary_by_instance()
      : PFS_index_all_instr(&m_key), m_key("OBJECT_INSTANCE_BEGIN") {}

  ~PFS_index_events_waits_summary_by_instance() {}

  virtual bool match(PFS_mutex *pfs);
  virtual bool match(PFS_rwlock *pfs);
  virtual bool match(PFS_cond *pfs);
  virtual bool match(PFS_file *pfs);
  virtual bool match(PFS_socket *pfs);

 private:
  PFS_key_object_instance m_key;
};

class PFS_index_events_waits_summary_by_event_name
    : public PFS_index_all_instr {
 public:
  PFS_index_events_waits_summary_by_event_name()
      : PFS_index_all_instr(&m_key), m_key("EVENT_NAME") {}

  ~PFS_index_events_waits_summary_by_event_name() {}

  virtual bool match(PFS_mutex *pfs);
  virtual bool match(PFS_rwlock *pfs);
  virtual bool match(PFS_cond *pfs);
  virtual bool match(PFS_file *pfs);
  virtual bool match(PFS_socket *pfs);
  virtual bool match_view(uint view);

 private:
  PFS_key_event_name m_key;
};

/** Table PERFORMANCE_SCHEMA.EVENTS_WAITS_SUMMARY_BY_INSTANCE. */
class table_events_waits_summary_by_instance : public table_all_instr {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  int index_init(uint idx, bool sorted);

 protected:
  int make_instr_row(PFS_instr *pfs, PFS_instr_class *klass,
                     const void *object_instance_begin,
                     PFS_single_stat *pfs_stat);
  virtual int make_mutex_row(PFS_mutex *pfs);
  virtual int make_rwlock_row(PFS_rwlock *pfs);
  virtual int make_cond_row(PFS_cond *pfs);
  virtual int make_file_row(PFS_file *pfs);
  virtual int make_socket_row(PFS_socket *pfs);

  virtual int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                              bool read_all);

  table_events_waits_summary_by_instance();

 public:
  ~table_events_waits_summary_by_instance() {}

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_events_waits_summary_by_instance m_row;
};

/** @} */
#endif
