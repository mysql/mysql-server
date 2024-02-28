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

#ifndef TABLE_PROCESSLIST_H
#define TABLE_PROCESSLIST_H

/**
  @file storage/perfschema/table_processlist.h
  TABLE THREADS.
*/

#include <sys/types.h>
#include <time.h>
#include <algorithm>

#include "my_hostname.h"
#include "my_inttypes.h"
#include "storage/perfschema/cursor_by_thread.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_name.h"

struct PFS_thread;

/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  A row of PERFORMANCE_SCHEMA.PROCESSLIST.
*/
struct row_processlist {
  /** Column ID. */
  ulonglong m_processlist_id{0};
  /** Column USER. */
  PFS_user_name m_user_name;
  /** Column HOST (and PORT). */
  char m_hostname[HOST_AND_PORT_LENGTH];
  /** Length in bytes of @c m_hostname. */
  uint m_hostname_length{0};
  /** Column DB. */
  PFS_schema_name m_db_name;
  /** Column COMMAND. */
  int m_command{0};
  /** Column TIME. */
  time_t m_start_time{0};
  /** Column STATE. */
  const char *m_processlist_state_ptr{nullptr};
  /** Length in bytes of @c m_processlist_state_ptr. */
  uint m_processlist_state_length{0};
  /** Column INFO. */
  const char *m_processlist_info_ptr{nullptr};
  /** Length in bytes of @c m_processlist_info_ptr. */
  uint m_processlist_info_length{0};
  /** Column EXECUTION_ENGINE. */
  bool m_secondary{false};
};

class PFS_index_processlist_by_processlist_id : public PFS_index_threads {
 public:
  PFS_index_processlist_by_processlist_id()
      : PFS_index_threads(&m_key), m_key("ID") {}

  ~PFS_index_processlist_by_processlist_id() override = default;

  bool match(PFS_thread *pfs) override;

 private:
  PFS_key_processlist_id m_key;
};

enum enum_priv_processlist {
  /** User is not allowed to see any data. */
  PROCESSLIST_DENIED,
  /** User does not have the PROCESS_ACL privilege. */
  PROCESSLIST_USER_ONLY,
  /** User has the PROCESS_ACL privilege. */
  PROCESSLIST_ALL
};

struct row_priv_processlist {
  enum enum_priv_processlist m_auth;
  char m_priv_user[USERNAME_LENGTH];
  size_t m_priv_user_length;
};

/** Table PERFORMANCE_SCHEMA.PROCESSLIST. */
class table_processlist : public cursor_by_thread {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  /** Table builder */
  static PFS_engine_table *create(PFS_engine_table_share *);

 protected:
  table_processlist();

  int rnd_init(bool scan [[maybe_unused]]) override;

  int index_init(uint idx, bool sorted) override;
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;
  int set_access();

 public:
  ~table_processlist() override = default;

 private:
  int make_row(PFS_thread *pfs) override;
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;
  /** Current row. */
  row_processlist m_row;
  /** Row privileges. */
  row_priv_processlist m_row_priv;
};

/** @} */
#endif
