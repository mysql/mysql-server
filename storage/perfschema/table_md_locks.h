/* Copyright (c) 2012, 2022, Oracle and/or its affiliates.

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

#ifndef TABLE_METADATA_LOCK_H
#define TABLE_METADATA_LOCK_H

/**
  @file storage/perfschema/table_md_locks.h
  Table METADATA_LOCKS (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "mysql/components/services/bits/psi_mdl_bits.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
struct PFS_metadata_lock;
struct TABLE;
struct THR_LOCK;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of table PERFORMANCE_SCHEMA.MUTEX_INSTANCES. */
struct row_metadata_lock {
  /** Column OBJECT_INSTANCE_BEGIN. */
  const void *m_identity;
  opaque_mdl_type m_mdl_type;
  opaque_mdl_duration m_mdl_duration;
  opaque_mdl_status m_mdl_status;
  /** Column SOURCE. */
  char m_source[COL_SOURCE_SIZE];
  /** Length in bytes of @c m_source. */
  uint m_source_length;
  /** Column OWNER_THREAD_ID. */
  ulong m_owner_thread_id;
  /** Column OWNER_EVENT_ID. */
  ulong m_owner_event_id;
  /** Columns OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, COLUMN_NAME. */
  PFS_column_row m_object;
};

class PFS_index_metadata_locks : public PFS_engine_index {
 public:
  explicit PFS_index_metadata_locks(PFS_engine_key *key_1)
      : PFS_engine_index(key_1) {}

  PFS_index_metadata_locks(PFS_engine_key *key_1, PFS_engine_key *key_2)
      : PFS_engine_index(key_1, key_2) {}

  PFS_index_metadata_locks(PFS_engine_key *key_1, PFS_engine_key *key_2,
                           PFS_engine_key *key_3, PFS_engine_key *key_4)
      : PFS_engine_index(key_1, key_2, key_3, key_4) {}

  ~PFS_index_metadata_locks() override = default;

  virtual bool match(const PFS_metadata_lock *pfs) = 0;
};

class PFS_index_metadata_locks_by_instance : public PFS_index_metadata_locks {
 public:
  PFS_index_metadata_locks_by_instance()
      : PFS_index_metadata_locks(&m_key), m_key("OBJECT_INSTANCE_BEGIN") {}

  ~PFS_index_metadata_locks_by_instance() override = default;

  bool match(const PFS_metadata_lock *pfs) override;

 private:
  PFS_key_object_instance m_key;
};

class PFS_index_metadata_locks_by_object : public PFS_index_metadata_locks {
 public:
  PFS_index_metadata_locks_by_object()
      : PFS_index_metadata_locks(&m_key_1, &m_key_2, &m_key_3, &m_key_4),
        m_key_1("OBJECT_TYPE"),
        m_key_2("OBJECT_SCHEMA"),
        m_key_3("OBJECT_NAME"),
        m_key_4("COLUMN_NAME") {}

  ~PFS_index_metadata_locks_by_object() override = default;

  bool match(const PFS_metadata_lock *pfs) override;

 private:
  PFS_key_object_type m_key_1;
  PFS_key_object_schema m_key_2;
  PFS_key_object_name m_key_3;
  PFS_key_column_name m_key_4;
};

class PFS_index_metadata_locks_by_owner : public PFS_index_metadata_locks {
 public:
  PFS_index_metadata_locks_by_owner()
      : PFS_index_metadata_locks(&m_key_1, &m_key_2),
        m_key_1("OWNER_THREAD_ID"),
        m_key_2("OWNER_EVENT_ID") {}

  ~PFS_index_metadata_locks_by_owner() override = default;

  bool match(const PFS_metadata_lock *pfs) override;

 private:
  PFS_key_thread_id m_key_1;
  PFS_key_event_id m_key_2;
};

/** Table PERFORMANCE_SCHEMA.METADATA_LOCKS. */
class table_metadata_locks : public PFS_engine_table {
 public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  void reset_position(void) override;

  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

 private:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;
  table_metadata_locks();

 public:
  ~table_metadata_locks() override = default;

 private:
  int make_row(PFS_metadata_lock *pfs);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_metadata_lock m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

 protected:
  PFS_index_metadata_locks *m_opened_index;
};

/** @} */
#endif
