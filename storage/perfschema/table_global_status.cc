/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_global_status.cc
  Table global_status (implementation).
*/

#include "storage/perfschema/table_global_status.h"

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
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"

bool PFS_index_global_status::match(const Status_variable *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }

  return true;
}

THR_LOCK table_global_status::m_table_lock;

Plugin_table table_global_status::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "global_status",
    /* Definition */
    "  VARIABLE_NAME VARCHAR(64) not null,\n"
    "  VARIABLE_VALUE VARCHAR(1024),\n"
    "  PRIMARY KEY (VARIABLE_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_global_status::m_share = {
    &pfs_truncatable_world_acl,
    table_global_status::create,
    nullptr, /* write_row */
    table_global_status::delete_all_rows,
    table_global_status::get_row_count,
    sizeof(pos_t),
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_global_status::create(PFS_engine_table_share *) {
  return new table_global_status();
}

int table_global_status::delete_all_rows() {
  mysql_mutex_lock(&LOCK_status);
  reset_status_by_thread();
  reset_status_by_account();
  reset_status_by_user();
  reset_status_by_host();
  reset_global_status();
  mysql_mutex_unlock(&LOCK_status);
  return 0;
}

ha_rows table_global_status::get_row_count() {
  mysql_mutex_lock(&LOCK_status);
  const ha_rows status_var_count = all_status_vars.size();
  mysql_mutex_unlock(&LOCK_status);
  return status_var_count;
}

table_global_status::table_global_status()
    : PFS_engine_table(&m_share, &m_pos),
      m_status_cache(false),
      m_pos(0),
      m_next_pos(0) {}

void table_global_status::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_global_status::rnd_init(bool /* scan */) {
  /* Build a cache of all global status variables. Sum across threads. */
  m_status_cache.materialize_global();

  return 0;
}

int table_global_status::rnd_next() {
  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_status_cache.size();
       m_pos.next()) {
    const Status_variable *status_var = m_status_cache.get(m_pos.m_index);
    if (status_var != nullptr) {
      m_next_pos.set_after(&m_pos);
      return make_row(status_var);
    }
  }
  return HA_ERR_END_OF_FILE;
}

int table_global_status::rnd_pos(const void *pos) {
  set_position(pos);
  const Status_variable *status_var = m_status_cache.get(m_pos.m_index);
  if (status_var != nullptr) {
    return make_row(status_var);
  }

  return HA_ERR_RECORD_DELETED;
}

int table_global_status::index_init(uint idx [[maybe_unused]], bool) {
  /* Build a cache of all global status variables. Sum across threads. */
  m_status_cache.materialize_global();

  PFS_index_global_status *result = nullptr;
  assert(idx == 0);
  result = PFS_NEW(PFS_index_global_status);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_global_status::index_next() {
  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_status_cache.size();
       m_pos.next()) {
    const Status_variable *status_var = m_status_cache.get(m_pos.m_index);
    if (status_var != nullptr) {
      if (m_opened_index->match(status_var)) {
        if (!make_row(status_var)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}

int table_global_status::make_row(const Status_variable *status_var) {
  if (status_var->is_null()) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_variable_name.make_row(status_var->m_name, status_var->m_name_length);
  m_row.m_variable_value.make_row(status_var);

  return 0;
}

int table_global_status::read_row_values(TABLE *table, unsigned char *buf,
                                         Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* VARIABLE_NAME */
          set_field_varchar_utf8mb4(f, m_row.m_variable_name.m_str,
                                    m_row.m_variable_name.m_length);
          break;
        case 1: /* VARIABLE_VALUE */
          m_row.m_variable_value.set_field(f);
          break;
        default:
          assert(false);
      }
    }
  }

  return 0;
}
