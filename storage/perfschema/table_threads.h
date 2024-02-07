/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TABLE_THREADS_H
#define TABLE_THREADS_H

/**
  @file storage/perfschema/table_threads.h
  TABLE THREADS.
*/

#include <sys/types.h>
#include <time.h>

#include "my_inttypes.h"
#include "storage/perfschema/cursor_by_thread.h"
#include "storage/perfschema/pfs_column_types.h"

struct PFS_thread;

/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  A row of PERFORMANCE_SCHEMA.THREADS.
*/
struct row_threads {
  /** Column THREAD_ID. */
  ulonglong m_thread_internal_id;
  /** Column PROCESSLIST_ID. */
  ulonglong m_processlist_id;
  /** Column NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Column PROCESSLIST_USER. */
  PFS_user_name m_user_name;
  /** Column PROCESSLIST_HOST. */
  PFS_host_name m_host_name;
  /** Column PROCESSLIST_DB. */
  PFS_schema_name m_db_name;
  /** Column PROCESSLIST_COMMAND. */
  int m_command;
  /** Column PROCESSLIST_TIME. */
  time_t m_start_time;
  /** Column PROCESSLIST_STATE. */
  const char *m_processlist_state_ptr;
  /** Length in bytes of @c m_processlist_state_ptr. */
  uint m_processlist_state_length;
  /** Column PROCESSLIST_INFO. */
  const char *m_processlist_info_ptr;
  /** Length in bytes of @c m_processlist_info_ptr. */
  uint m_processlist_info_length;
  /** Column INSTRUMENTED (read). */
  bool m_enabled;
  /** Column HISTORY (read). */
  bool m_history;
  /** INSTRUMENTED and HISTORY (write). */
  PFS_thread *m_psi;
  /** Column PARENT_THREAD_ID. */
  ulonglong m_parent_thread_internal_id;
  /** Column CONNECTION_TYPE. */
  enum_vio_type m_connection_type;
  /** Column THREAD_OS_ID. */
  my_thread_os_id_t m_thread_os_id;
  /** Column RESOURCE_GROUP. */
  char m_groupname[NAME_LEN];
  /** Length in bytes of @c m_groupname. */
  uint m_groupname_length;
  /** Column EXECUTION_ENGINE. */
  bool m_secondary;
  /** CURRENT_CONTROLLED_MEMORY, ... */
  PFS_session_all_memory_stat_row m_session_all_memory_row;
  /** Column TELEMETRY_ACTIVE (read). */
  bool m_telemetry_active;
};

class PFS_index_threads_by_thread_id : public PFS_index_threads {
 public:
  PFS_index_threads_by_thread_id()
      : PFS_index_threads(&m_key), m_key("THREAD_ID") {}

  ~PFS_index_threads_by_thread_id() override = default;

  bool match(PFS_thread *pfs) override;

 private:
  PFS_key_thread_id m_key;
};

class PFS_index_threads_by_processlist_id : public PFS_index_threads {
 public:
  PFS_index_threads_by_processlist_id()
      : PFS_index_threads(&m_key), m_key("PROCESSLIST_ID") {}

  ~PFS_index_threads_by_processlist_id() override = default;

  bool match(PFS_thread *pfs) override;

 private:
  PFS_key_processlist_id m_key;
};

class PFS_index_threads_by_name : public PFS_index_threads {
 public:
  PFS_index_threads_by_name() : PFS_index_threads(&m_key), m_key("NAME") {}

  ~PFS_index_threads_by_name() override = default;

  bool match(PFS_thread *pfs) override;

 private:
  PFS_key_thread_name m_key;
};

class PFS_index_threads_by_user_host : public PFS_index_threads {
 public:
  PFS_index_threads_by_user_host()
      : PFS_index_threads(&m_key_1, &m_key_2),
        m_key_1("PROCESSLIST_USER"),
        m_key_2("PROCESSLIST_HOST") {}

  ~PFS_index_threads_by_user_host() override = default;

  bool match(PFS_thread *pfs) override;

 private:
  PFS_key_user m_key_1;
  PFS_key_host m_key_2;
};

class PFS_index_threads_by_host : public PFS_index_threads {
 public:
  PFS_index_threads_by_host()
      : PFS_index_threads(&m_key), m_key("PROCESSLIST_HOST") {}

  ~PFS_index_threads_by_host() override = default;

  bool match(PFS_thread *pfs) override;

 private:
  PFS_key_host m_key;
};

class PFS_index_threads_by_thread_os_id : public PFS_index_threads {
 public:
  PFS_index_threads_by_thread_os_id()
      : PFS_index_threads(&m_key), m_key("THREAD_OS_ID") {}

  ~PFS_index_threads_by_thread_os_id() override = default;

  bool match(PFS_thread *pfs) override;

 private:
  PFS_key_thread_os_id m_key;
};

class PFS_index_threads_by_resource_group : public PFS_index_threads {
 public:
  PFS_index_threads_by_resource_group()
      : PFS_index_threads(&m_key), m_key("RESOURCE_GROUP") {}

  ~PFS_index_threads_by_resource_group() override = default;

  bool match(PFS_thread *pfs) override;

 private:
  PFS_key_group_name m_key;
};

/** Table PERFORMANCE_SCHEMA.THREADS. */
class table_threads : public cursor_by_thread {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  /** Table builder */
  static PFS_engine_table *create(PFS_engine_table_share *);

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

  int update_row_values(TABLE *table, const unsigned char *old_buf,
                        unsigned char *new_buf, Field **fields) override;

 protected:
  table_threads();
  int index_init(uint idx, bool sorted) override;

 public:
  ~table_threads() override = default;

 private:
  int make_row(PFS_thread *pfs) override;

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  row_threads m_row;
};

/** @} */
#endif
