/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef TABLE_TEMPORARY_ACCOUNT_LOCKS_H
#define TABLE_TEMPORARY_ACCOUNT_LOCKS_H

/**
  @file storage/perfschema/table_temporary_account_locks.h
  Table TEMPORARY_ACCOUNT_LOCKS (declarations).
*/

#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Plugin_table;
class THD;
struct TABLE;
struct THR_LOCK;
class ACL_USER;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row of PERFORMANCE_SCHEMA.TEMPORARY_ACCOUNT_LOCKS. */
struct row_temporary_account_locks {
  /** Column USER, HOST. */
  PFS_account_row m_account;
  /** Column LOCKED. */
  bool m_locked;
  /** Column FAILED_LOGIN_ATTEMPTS. */
  ulong m_failed_login_attempts;
  /** Column REMAINING_LOGIN_ATTEMPTS. */
  ulong m_remaining_login_attempts;
  /** Column PASSWORD_LOCK_TIME. */
  ulong m_password_lock_time_days;
  /** Column LOCKED_SINCE. */
  ulong m_locked_since_daynr;
  /** Column LOCKED_UNTIL. */
  ulong m_locked_until_daynr;
};

class PFS_index_temporary_account_locks : public PFS_engine_index {
 public:
  explicit PFS_index_temporary_account_locks(PFS_engine_key *key_1,
                                             PFS_engine_key *key_2)
      : PFS_engine_index(key_1, key_2) {}

  ~PFS_index_temporary_account_locks() override = default;

  virtual bool match(const row_temporary_account_locks *row) = 0;
};

class PFS_index_temporary_account_locks_by_account
    : public PFS_index_temporary_account_locks {
 public:
  PFS_index_temporary_account_locks_by_account()
      : PFS_index_temporary_account_locks(&m_key_1, &m_key_2),
        m_key_1("USER"),
        m_key_2("HOST") {}

  ~PFS_index_temporary_account_locks_by_account() override = default;

  bool match(const row_temporary_account_locks *row) override;

 private:
  PFS_key_user m_key_1;
  PFS_key_host m_key_2;
};

/** Table PERFORMANCE_SCHEMA.TEMPORARY_ACCOUNT_LOCKS. */
class table_temporary_account_locks : public PFS_engine_table {
 public:
  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  void reset_position() override;

  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

 protected:
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;
  table_temporary_account_locks();

 public:
  ~table_temporary_account_locks() override = default;

 private:
  void materialize(THD *thd);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  row_temporary_account_locks *m_all_rows;
  uint m_row_count;
  /** Current row. */
  row_temporary_account_locks *m_row;
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

  PFS_index_temporary_account_locks *m_opened_index;
};

/** @} */
#endif
