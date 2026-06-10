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

/**
  @file storage/perfschema/table_temporary_account_locks.cc
  Table TEMPORARY_ACCOUNT_LOCKS (implementation).
*/

#include "storage/perfschema/table_temporary_account_locks.h"

#include <assert.h>

#include "my_thread.h"
#include "sql/auth/sql_auth_cache.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/sql_class.h"
#include "sql/table.h"

THR_LOCK table_temporary_account_locks::m_table_lock;

Plugin_table table_temporary_account_locks::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "temporary_account_locks",
    /* Definition */
    "  USER CHAR(32) collate utf8mb4_bin not null,\n"
    "  HOST CHAR(255) CHARACTER SET ASCII not null,\n"
    "  LOCKED ENUM ('YES', 'NO') not null,\n"
    "  FAILED_LOGIN_ATTEMPTS int not null,\n"
    "  REMAINING_LOGIN_ATTEMPTS int not null,\n"
    "  PASSWORD_LOCK_TIME int not null,\n"
    "  LOCKED_SINCE date,\n"
    "  LOCKED_UNTIL date,\n"
    "  UNIQUE KEY `ACCOUNT` (USER, HOST) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_temporary_account_locks::m_share = {
    &pfs_readonly_acl,
    table_temporary_account_locks::create,
    nullptr, /* write_row */
    nullptr, /*delete_all_rows */
    table_temporary_account_locks::get_row_count,
    sizeof(PFS_simple_index), /* ref length */
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_temporary_account_locks_by_account::match(
    const row_temporary_account_locks *row) {
  if (m_fields >= 1) {
    if (!m_key_1.match(&row->m_account.m_user_name)) {
      return false;
    }
  }

  if (m_fields >= 2) {
    if (!m_key_2.match(&row->m_account.m_host_name)) {
      return false;
    }
  }

  return true;
}

PFS_engine_table *table_temporary_account_locks::create(
    PFS_engine_table_share *) {
  auto *t = new table_temporary_account_locks();
  THD *thd = current_thd;
  assert(thd != nullptr);
  t->materialize(thd);
  return t;
}

ha_rows table_temporary_account_locks::get_row_count() {
  THD *thd = current_thd;
  assert(thd != nullptr);
  Acl_cache_lock_guard acl_cache_lock(thd, Acl_cache_lock_mode::READ_MODE);

  if (!acl_cache_lock.lock(false)) {
    return 0;
  }

  ha_rows count = acl_users_size();

  return count;
}

table_temporary_account_locks::table_temporary_account_locks()
    : PFS_engine_table(&m_share, &m_pos),
      m_all_rows(nullptr),
      m_row_count(0),
      m_row(nullptr),
      m_pos(0),
      m_next_pos(0),
      m_opened_index(nullptr) {}

class ACL_USER_temporary_account_locks_visitor : public ACL_USER_visitor {
 public:
  ACL_USER_temporary_account_locks_visitor(row_temporary_account_locks *rows,
                                           size_t max_size)
      : m_rows(rows), m_max_size(max_size) {}
  ~ACL_USER_temporary_account_locks_visitor() override = default;

  void visit(const ACL_USER *acl_user) override;

  row_temporary_account_locks *m_rows;
  size_t m_max_size;
  size_t m_size{0};
};

void ACL_USER_temporary_account_locks_visitor::visit(const ACL_USER *acl_user) {
  if (m_size < m_max_size) {
    if (acl_user->password_locked_state.is_active()) {
      row_temporary_account_locks &row = m_rows[m_size];

      row.m_account.m_user_name.set(acl_user->user,
                                    acl_user->get_username_length());
      row.m_account.m_host_name.set(acl_user->host.hostname,
                                    acl_user->host.hostname_length);
      row.m_failed_login_attempts =
          acl_user->password_locked_state.get_failed_login_attempts();
      row.m_remaining_login_attempts =
          acl_user->password_locked_state.get_remaining_login_attempts();
      row.m_password_lock_time_days =
          acl_user->password_locked_state.get_password_lock_time_days();

      row.m_locked = (row.m_remaining_login_attempts == 0);

      if (row.m_locked) {
        row.m_locked_since_daynr =
            acl_user->password_locked_state.get_daynr_locked();
        row.m_locked_until_daynr =
            row.m_locked_since_daynr +
            acl_user->password_locked_state.get_password_lock_time_days();
      } else {
        row.m_locked_since_daynr = 0;
        row.m_locked_until_daynr = 0;
      }

      m_size++;
    }
  }
}

void table_temporary_account_locks::materialize(THD *thd) {
  uint size;
  row_temporary_account_locks *rows;

  assert(m_all_rows == nullptr);
  assert(m_row_count == 0);

  Acl_cache_lock_guard acl_cache_lock(thd, Acl_cache_lock_mode::READ_MODE);

  if (!acl_cache_lock.lock(false)) {
    return;
  }

  size = acl_users_size();

  if (size > 0) {
    rows = (row_temporary_account_locks *)thd->alloc(
        size * sizeof(row_temporary_account_locks));
    if (rows != nullptr) {
      ACL_USER_temporary_account_locks_visitor visitor(rows, size);

      acl_users_accept(&visitor);

      m_all_rows = rows;
      m_row_count = visitor.m_size;
    }
  }
}

void table_temporary_account_locks::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_temporary_account_locks::rnd_next() {
  int result;

  m_pos.set_at(&m_next_pos);

  if (m_pos.m_index < m_row_count) {
    m_row = &m_all_rows[m_pos.m_index];
    m_next_pos.set_after(&m_pos);
    result = 0;
  } else {
    m_row = nullptr;
    result = HA_ERR_END_OF_FILE;
  }

  return result;
}

int table_temporary_account_locks::rnd_pos(const void *pos) {
  set_position(pos);
  assert(m_pos.m_index < m_row_count);
  m_row = &m_all_rows[m_pos.m_index];
  return 0;
}

int table_temporary_account_locks::index_init(uint idx, bool) {
  PFS_index_temporary_account_locks *result = nullptr;

  switch (idx) {
    case 0:
      result = PFS_NEW(PFS_index_temporary_account_locks_by_account);
      break;
    default:
      assert(false);
      break;
  }

  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_temporary_account_locks::index_next() {
  int result;

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_row_count; m_pos.next()) {
    m_row = &m_all_rows[m_pos.m_index];

    if (m_opened_index->match(m_row)) {
      m_next_pos.set_after(&m_pos);
      result = 0;
      return result;
    }
  }

  m_row = nullptr;
  result = HA_ERR_END_OF_FILE;

  return result;
}

int table_temporary_account_locks::read_row_values(TABLE *table,
                                                   unsigned char *buf,
                                                   Field **fields,
                                                   bool read_all) {
  Field *f;

  assert(m_row);

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* USER */
        case 1: /* HOST */
          m_row->m_account.set_field(f->field_index(), f);
          break;
        case 2: /* LOCKED */
          set_field_enum(f, m_row->m_locked ? ENUM_YES : ENUM_NO);
          break;
        case 3: /* FAILED_LOGIN_ATTEMPTS */
          set_field_ulong(f, m_row->m_failed_login_attempts);
          break;
        case 4: /* REMAINING_LOGIN_ATTEMPTS */
          set_field_ulong(f, m_row->m_remaining_login_attempts);
          break;
        case 5: /* PASSWORD_LOCK_TIME */
          set_field_ulong(f, m_row->m_password_lock_time_days);
          break;
        case 6: /* LOCKED_SINCE */
          if (m_row->m_locked_since_daynr != 0) {
            set_field_date_by_daynr(f, m_row->m_locked_since_daynr);
          } else {
            f->set_null();
          }
          break;
        case 7: /* LOCKED_UNTIL */
          if (m_row->m_locked_until_daynr != 0) {
            set_field_date_by_daynr(f, m_row->m_locked_until_daynr);
          } else {
            f->set_null();
          }
          break;
        default:
          assert(false);
      }
    }
  }

  return 0;
}
