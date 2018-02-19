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

#ifndef TABLE_SYNC_INSTANCE_H
#define TABLE_SYNC_INSTANCE_H

/**
  @file storage/perfschema/table_sync_instances.h
  Table MUTEX_INSTANCES, RWLOCK_INSTANCES and COND_INSTANCES (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct PFS_cond;
struct PFS_mutex;
struct PFS_rwlock;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of table PERFORMANCE_SCHEMA.MUTEX_INSTANCES. */
struct row_mutex_instances {
  /** Column NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column OBJECT_INSTANCE_BEGIN. */
  const void *m_identity;
  /** True if column LOCKED_BY_THREAD_ID is not null. */
  bool m_locked;
  /** Column LOCKED_BY_THREAD_ID. */
  ulonglong m_locked_by_thread_id;
};

class PFS_index_mutex_instances : public PFS_engine_index {
 public:
  PFS_index_mutex_instances(PFS_engine_key *key_1) : PFS_engine_index(key_1) {}

  ~PFS_index_mutex_instances() {}

  virtual bool match(PFS_mutex *pfs) = 0;
};

class PFS_index_mutex_instances_by_instance : public PFS_index_mutex_instances {
 public:
  PFS_index_mutex_instances_by_instance()
      : PFS_index_mutex_instances(&m_key), m_key("OBJECT_INSTANCE_BEGIN") {}

  ~PFS_index_mutex_instances_by_instance() {}

  virtual bool match(PFS_mutex *pfs);

 private:
  PFS_key_object_instance m_key;
};

class PFS_index_mutex_instances_by_name : public PFS_index_mutex_instances {
 public:
  PFS_index_mutex_instances_by_name()
      : PFS_index_mutex_instances(&m_key), m_key("NAME") {}

  ~PFS_index_mutex_instances_by_name() {}

  virtual bool match(PFS_mutex *pfs);

 private:
  PFS_key_event_name m_key;
};

class PFS_index_mutex_instances_by_thread_id
    : public PFS_index_mutex_instances {
 public:
  PFS_index_mutex_instances_by_thread_id()
      : PFS_index_mutex_instances(&m_key), m_key("LOCKED_BY_THREAD_ID") {}

  ~PFS_index_mutex_instances_by_thread_id() {}

  virtual bool match(PFS_mutex *pfs);

 private:
  PFS_key_thread_id m_key;
};

/** Table PERFORMANCE_SCHEMA.MUTEX_INSTANCES. */
class table_mutex_instances : public PFS_engine_table {
 public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next(void);

 private:
  virtual int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                              bool read_all);
  table_mutex_instances();

 public:
  ~table_mutex_instances() {}

 protected:
  int make_row(PFS_mutex *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_mutex_instances m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  PFS_index_mutex_instances *m_opened_index;
};

/** A row of table PERFORMANCE_SCHEMA.RWLOCK_INSTANCES. */
struct row_rwlock_instances {
  /** Column NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column OBJECT_INSTANCE_BEGIN. */
  const void *m_identity;
  /** True if column WRITE_LOCKED_BY_THREAD_ID is not null. */
  bool m_write_locked;
  /** Column WRITE_LOCKED_BY_THREAD_ID. */
  ulonglong m_write_locked_by_thread_id;
  /** Column READ_LOCKED_BY_COUNT. */
  ulong m_readers;
};

class PFS_index_rwlock_instances : public PFS_engine_index {
 public:
  PFS_index_rwlock_instances(PFS_engine_key *key_1) : PFS_engine_index(key_1) {}

  ~PFS_index_rwlock_instances() {}

  virtual bool match(PFS_rwlock *pfs) = 0;
};

class PFS_index_rwlock_instances_by_instance
    : public PFS_index_rwlock_instances {
 public:
  PFS_index_rwlock_instances_by_instance()
      : PFS_index_rwlock_instances(&m_key), m_key("OBJECT_INSTANCE_BEGIN") {}

  ~PFS_index_rwlock_instances_by_instance() {}

  virtual bool match(PFS_rwlock *pfs);

 private:
  PFS_key_object_instance m_key;
};

class PFS_index_rwlock_instances_by_name : public PFS_index_rwlock_instances {
 public:
  PFS_index_rwlock_instances_by_name()
      : PFS_index_rwlock_instances(&m_key), m_key("NAME") {}

  ~PFS_index_rwlock_instances_by_name() {}

  virtual bool match(PFS_rwlock *pfs);

 private:
  PFS_key_event_name m_key;
};

class PFS_index_rwlock_instances_by_thread_id
    : public PFS_index_rwlock_instances {
 public:
  PFS_index_rwlock_instances_by_thread_id()
      : PFS_index_rwlock_instances(&m_key),
        m_key("WRITE_LOCKED_BY_THREAD_ID") {}

  ~PFS_index_rwlock_instances_by_thread_id() {}

  virtual bool match(PFS_rwlock *pfs);

 private:
  PFS_key_thread_id m_key;
};

/** Table PERFORMANCE_SCHEMA.RWLOCK_INSTANCES. */
class table_rwlock_instances : public PFS_engine_table {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next(void);

 private:
  virtual int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                              bool read_all);
  table_rwlock_instances();

 public:
  ~table_rwlock_instances() {}

 protected:
  int make_row(PFS_rwlock *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_rwlock_instances m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  PFS_index_rwlock_instances *m_opened_index;
};

/** A row of table PERFORMANCE_SCHEMA.COND_INSTANCES. */
struct row_cond_instances {
  /** Column NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column OBJECT_INSTANCE_BEGIN. */
  const void *m_identity;
};

class PFS_index_cond_instances : public PFS_engine_index {
 public:
  PFS_index_cond_instances(PFS_engine_key *key_1) : PFS_engine_index(key_1) {}

  ~PFS_index_cond_instances() {}

  virtual bool match(PFS_cond *pfs) = 0;
};

class PFS_index_cond_instances_by_instance : public PFS_index_cond_instances {
 public:
  PFS_index_cond_instances_by_instance()
      : PFS_index_cond_instances(&m_key), m_key("OBJECT_INSTANCE_BEGIN") {}

  ~PFS_index_cond_instances_by_instance() {}

  virtual bool match(PFS_cond *pfs);

 private:
  PFS_key_object_instance m_key;
};

class PFS_index_cond_instances_by_name : public PFS_index_cond_instances {
 public:
  PFS_index_cond_instances_by_name()
      : PFS_index_cond_instances(&m_key), m_key("NAME") {}

  ~PFS_index_cond_instances_by_name() {}

  virtual bool match(PFS_cond *pfs);

 private:
  PFS_key_event_name m_key;
};

/** Table PERFORMANCE_SCHEMA.COND_INSTANCES. */
class table_cond_instances : public PFS_engine_table {
 public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next(void);

 private:
  virtual int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                              bool read_all);
  table_cond_instances();

 public:
  ~table_cond_instances() {}

 protected:
  int make_row(PFS_cond *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_cond_instances m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  PFS_index_cond_instances *m_opened_index;
};

/** @} */
#endif
