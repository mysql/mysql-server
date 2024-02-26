/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/table_status_by_user.cc
  Table STATUS_BY_USER (implementation).
*/

#include "storage/perfschema/table_status_by_user.h"

#include <assert.h>
#include <stddef.h>
#include <new>

#include "my_thread.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/mysqld.h"
#include "sql/plugin_table.h"
#include "sql/sql_class.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_visitor.h"

THR_LOCK table_status_by_user::m_table_lock;

Plugin_table table_status_by_user::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "status_by_user",
    /* Definition */
    "  USER CHAR(32) collate utf8mb4_bin default null,\n"
    "  VARIABLE_NAME VARCHAR(64) not null,\n"
    "  VARIABLE_VALUE VARCHAR(1024),\n"
    "  UNIQUE KEY (USER, VARIABLE_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_status_by_user::m_share = {
    &pfs_truncatable_acl,
    table_status_by_user::create,
    nullptr, /* write_row */
    table_status_by_user::delete_all_rows,
    table_status_by_user::get_row_count,
    sizeof(pos_t),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_status_by_user::match(PFS_user *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_status_by_user::match(const Status_variable *pfs) {
  if (m_fields >= 2) {
    if (!m_key_2.match(pfs)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_status_by_user::create(PFS_engine_table_share *) {
  return new table_status_by_user();
}

int table_status_by_user::delete_all_rows() {
  mysql_mutex_lock(&LOCK_status);
  reset_status_by_thread();
  reset_status_by_account();
  reset_status_by_user();
  mysql_mutex_unlock(&LOCK_status);
  return 0;
}

ha_rows table_status_by_user::get_row_count() {
  mysql_mutex_lock(&LOCK_status);
  const size_t status_var_count = all_status_vars.size();
  mysql_mutex_unlock(&LOCK_status);
  return (global_user_container.get_row_count() * status_var_count);
}

table_status_by_user::table_status_by_user()
    : PFS_engine_table(&m_share, &m_pos),
      m_status_cache(true),
      m_pos(),
      m_next_pos() {}

void table_status_by_user::reset_position() {
  m_pos.reset();
  m_next_pos.reset();
}

int table_status_by_user::rnd_init(bool /* scan */) {
  /* Build array of SHOW_VARs from the global status array. */
  m_status_cache.initialize_client_session();

  return 0;
}

int table_status_by_user::rnd_next() {
  /*
    For each user, build a cache of status variables using totals from all
    threads associated with the user.
  */
  bool has_more_user = true;

  for (m_pos.set_at(&m_next_pos); has_more_user; m_pos.next_user()) {
    PFS_user *pfs_user =
        global_user_container.get(m_pos.m_index_1, &has_more_user);

    if (m_status_cache.materialize_user(pfs_user) == 0) {
      const Status_variable *stat_var = m_status_cache.get(m_pos.m_index_2);
      if (stat_var != nullptr) {
        /* If make_row() fails, get the next user. */
        if (!make_row(pfs_user, stat_var)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}

int table_status_by_user::rnd_pos(const void *pos) {
  set_position(pos);
  assert(m_pos.m_index_1 < global_user_container.get_row_count());

  PFS_user *pfs_user = global_user_container.get(m_pos.m_index_1);

  if (m_status_cache.materialize_user(pfs_user) == 0) {
    const Status_variable *stat_var = m_status_cache.get(m_pos.m_index_2);
    if (stat_var != nullptr) {
      return make_row(pfs_user, stat_var);
    }
  }
  return HA_ERR_RECORD_DELETED;
}

int table_status_by_user::index_init(uint idx [[maybe_unused]], bool) {
  /* Build array of SHOW_VARs from the global status array. */
  m_status_cache.initialize_client_session();

  PFS_index_status_by_user *result = nullptr;
  assert(idx == 0);
  result = PFS_NEW(PFS_index_status_by_user);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_status_by_user::index_next() {
  /*
    For each user, build a cache of status variables using totals from all
    threads associated with the user.
  */
  bool has_more_user = true;

  for (m_pos.set_at(&m_next_pos); has_more_user; m_pos.next_user()) {
    PFS_user *pfs_user =
        global_user_container.get(m_pos.m_index_1, &has_more_user);

    if (pfs_user != nullptr) {
      if (m_opened_index->match(pfs_user)) {
        if (m_status_cache.materialize_user(pfs_user) == 0) {
          const Status_variable *stat_var;
          do {
            stat_var = m_status_cache.get(m_pos.m_index_2);
            if (stat_var != nullptr) {
              if (m_opened_index->match(stat_var)) {
                if (!make_row(pfs_user, stat_var)) {
                  m_next_pos.set_after(&m_pos);
                  return 0;
                }
              }
              m_pos.m_index_2++;
            }
          } while (stat_var != nullptr);
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_status_by_user::make_row(PFS_user *user,
                                   const Status_variable *status_var) {
  pfs_optimistic_state lock;
  user->m_lock.begin_optimistic_lock(&lock);

  if (m_row.m_user.make_row(user)) {
    return HA_ERR_RECORD_DELETED;
  }

  if (m_row.m_variable_name.make_row(status_var->m_name,
                                     status_var->m_name_length)) {
    return HA_ERR_RECORD_DELETED;
  }

  if (m_row.m_variable_value.make_row(status_var)) {
    return HA_ERR_RECORD_DELETED;
  }

  if (!user->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_status_by_user::read_row_values(TABLE *table, unsigned char *buf,
                                          Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* USER */
          m_row.m_user.set_nullable_field(f);
          break;
        case 1: /* VARIABLE_NAME */
          set_field_varchar_utf8mb4(f, m_row.m_variable_name.m_str,
                                    m_row.m_variable_name.m_length);
          break;
        case 2: /* VARIABLE_VALUE */
          m_row.m_variable_value.set_field(f);
          break;
        default:
          assert(false);
      }
    }
  }

  return 0;
}
