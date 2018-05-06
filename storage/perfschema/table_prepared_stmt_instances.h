/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_PREPARED_STMT_INSTANCES
#define TABLE_PREPARED_STMT_INSTANCES

/**
  @file storage/perfschema/table_prepared_stmt_instances.h
  Table PREPARED_STATEMENT_INSTANCE(declarations).
*/

#include <sys/types.h>

#include "my_inttypes.h"
#include "storage/perfschema/pfs_prepared_stmt.h"
#include "storage/perfschema/table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  A row of table
  PERFORMANCE_SCHEMA.PREPARED_STATEMENT_INSTANCES.
*/
struct row_prepared_stmt_instances {
  /** Column OBJECT_INSTANCE_BEGIN. */
  const void *m_identity;

  /** Column STMT_ID. */
  ulonglong m_stmt_id;

  /** Column STMT_NAME. */
  char m_stmt_name[COL_INFO_SIZE];
  int m_stmt_name_length;

  /** Column SQL_TEXT. */
  char m_sql_text[COL_INFO_SIZE];
  int m_sql_text_length;

  /** Column OWNER_THREAD_ID. */
  ulonglong m_owner_thread_id;

  /** Column OWNER_EVENT_ID. */
  ulonglong m_owner_event_id;

  /** Column OWNER_OBJECT_TYPE. */
  enum_object_type m_owner_object_type;

  /** Column OWNER_OBJECT_SCHEMA */
  char m_owner_object_schema[COL_OBJECT_SCHEMA_SIZE];
  int m_owner_object_schema_length;

  /** Column OWNER_OBJECT_NAME */
  char m_owner_object_name[COL_OBJECT_NAME_SIZE];
  int m_owner_object_name_length;

  /** Columns TIMER_PREPARE. */
  PFS_stat_row m_prepare_stat;

  /** Columns COUNT_REPREPARE. */
  PFS_stat_row m_reprepare_stat;

  /** Columns COUNT_STAR...SUM_NO_GOOD_INDEX_USED. */
  PFS_statement_stat_row m_execute_stat;
};

class PFS_index_prepared_stmt_instances : public PFS_engine_index {
 public:
  PFS_index_prepared_stmt_instances(PFS_engine_key *key_1)
      : PFS_engine_index(key_1) {}

  PFS_index_prepared_stmt_instances(PFS_engine_key *key_1,
                                    PFS_engine_key *key_2)
      : PFS_engine_index(key_1, key_2) {}

  PFS_index_prepared_stmt_instances(PFS_engine_key *key_1,
                                    PFS_engine_key *key_2,
                                    PFS_engine_key *key_3)
      : PFS_engine_index(key_1, key_2, key_3) {}

  ~PFS_index_prepared_stmt_instances() {}

  virtual bool match(const PFS_prepared_stmt *pfs) = 0;
};

class PFS_index_prepared_stmt_instances_by_instance
    : public PFS_index_prepared_stmt_instances {
 public:
  PFS_index_prepared_stmt_instances_by_instance()
      : PFS_index_prepared_stmt_instances(&m_key),
        m_key("OBJECT_INSTANCE_BEGIN") {}

  ~PFS_index_prepared_stmt_instances_by_instance() {}

  virtual bool match(const PFS_prepared_stmt *pfs);

 private:
  PFS_key_object_instance m_key;
};

class PFS_index_prepared_stmt_instances_by_owner_thread
    : public PFS_index_prepared_stmt_instances {
 public:
  PFS_index_prepared_stmt_instances_by_owner_thread()
      : PFS_index_prepared_stmt_instances(&m_key_1, &m_key_2),
        m_key_1("OWNER_THREAD_ID"),
        m_key_2("OWNER_EVENT_ID") {}

  ~PFS_index_prepared_stmt_instances_by_owner_thread() {}

  bool match(const PFS_prepared_stmt *pfs);

 private:
  PFS_key_thread_id m_key_1;
  PFS_key_event_id m_key_2;
};

class PFS_index_prepared_stmt_instances_by_statement_id
    : public PFS_index_prepared_stmt_instances {
 public:
  PFS_index_prepared_stmt_instances_by_statement_id()
      : PFS_index_prepared_stmt_instances(&m_key), m_key("STATEMENT_ID") {}

  ~PFS_index_prepared_stmt_instances_by_statement_id() {}

  bool match(const PFS_prepared_stmt *pfs);

 private:
  PFS_key_statement_id m_key;
};

class PFS_index_prepared_stmt_instances_by_statement_name
    : public PFS_index_prepared_stmt_instances {
 public:
  PFS_index_prepared_stmt_instances_by_statement_name()
      : PFS_index_prepared_stmt_instances(&m_key), m_key("STATEMENT_NAME") {}

  ~PFS_index_prepared_stmt_instances_by_statement_name() {}

  bool match(const PFS_prepared_stmt *pfs);

 private:
  PFS_key_statement_name m_key;
};

class PFS_index_prepared_stmt_instances_by_owner_object
    : public PFS_index_prepared_stmt_instances {
 public:
  PFS_index_prepared_stmt_instances_by_owner_object()
      : PFS_index_prepared_stmt_instances(&m_key_1, &m_key_2, &m_key_3),
        m_key_1("OWNER_OBJECT_TYPE"),
        m_key_2("OWNER_OBJECT_SCHEMA"),
        m_key_3("OWNER_OBJECT_NAME") {}

  ~PFS_index_prepared_stmt_instances_by_owner_object() {}

  virtual bool match(const PFS_prepared_stmt *table);

 private:
  PFS_key_object_type_enum m_key_1;
  PFS_key_object_schema m_key_2;
  PFS_key_object_name m_key_3;
};

/** Table PERFORMANCE_SCHEMA.PREPARED_STATEMENT_INSTANCES. */
class table_prepared_stmt_instances : public PFS_engine_table {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static int delete_all_rows();
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_init(uint idx, bool sorted);
  virtual int index_next();

 protected:
  virtual int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                              bool read_all);

  table_prepared_stmt_instances();

 public:
  ~table_prepared_stmt_instances() {}

 protected:
  int make_row(PFS_prepared_stmt *);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_prepared_stmt_instances m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  PFS_index_prepared_stmt_instances *m_opened_index;
};

/** @} */
#endif
