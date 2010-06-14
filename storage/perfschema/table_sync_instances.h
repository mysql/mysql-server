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

#ifndef TABLE_SYNC_INSTANCE_H
#define TABLE_SYNC_INSTANCE_H

/**
  @file storage/perfschema/table_sync_instances.h
  Table MUTEX_INSTANCES, RWLOCK_INSTANCES and COND_INSTANCES (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"

struct PFS_mutex;
struct PFS_rwlock;
struct PFS_cond;

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** A row of table PERFORMANCE_SCHEMA.MUTEX_INSTANCES. */
struct row_mutex_instances
{
  /** Column NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column OBJECT_INSTANCE_BEGIN. */
  const void *m_identity;
  /** True if column LOCKED_BY_THREAD_ID is not null. */
  bool m_locked;
  /** Column LOCKED_BY_THREAD_ID. */
  ulong m_locked_by_thread_id;
};

/** Table PERFORMANCE_SCHEMA.MUTEX_INSTANCES. */
class table_mutex_instances : public PFS_engine_table
{
public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

private:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_mutex_instances();

public:
  ~table_mutex_instances()
  {}

private:
  void make_row(PFS_mutex *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_mutex_instances m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** A row of table PERFORMANCE_SCHEMA.RWLOCK_INSTANCES. */
struct row_rwlock_instances
{
  /** Column NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column OBJECT_INSTANCE_BEGIN. */
  const void *m_identity;
  /** True if column WRITE_LOCKED_BY_THREAD_ID is not null. */
  bool m_write_locked;
  /** Column WRITE_LOCKED_BY_THREAD_ID. */
  ulong m_write_locked_by_thread_id;
  /** Column READ_LOCKED_BY_COUNT. */
  ulong m_readers;
};

/** Table PERFORMANCE_SCHEMA.RWLOCK_INSTANCES. */
class table_rwlock_instances : public PFS_engine_table
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

private:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_rwlock_instances();

public:
  ~table_rwlock_instances()
  {}

private:
  void make_row(PFS_rwlock *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_rwlock_instances m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** A row of table PERFORMANCE_SCHEMA.COND_INSTANCES. */
struct row_cond_instances
{
  /** Column NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column OBJECT_INSTANCE_BEGIN. */
  const void *m_identity;
};

/** Table PERFORMANCE_SCHEMA.COND_INSTANCES. */
class table_cond_instances : public PFS_engine_table
{
public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

private:
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_cond_instances();

public:
  ~table_cond_instances()
  {}

private:
  void make_row(PFS_cond *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;

  /** Current row. */
  row_cond_instances m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
