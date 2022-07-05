/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_global_variables.cc
  Table GLOBAL_VARIABLES (implementation).
*/

#include "storage/perfschema/table_global_variables.h"

#include <assert.h>
#include <stddef.h>
#include <new>

#include "my_thread.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/mysqld.h"
#include "sql/plugin_table.h"
#include "sql/sql_audit.h"  // audit_global_variable_get
#include "sql/sql_class.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"

bool PFS_index_global_variables::match(const System_variable *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) {
      return false;
    }
  }

  return true;
}

THR_LOCK table_global_variables::m_table_lock;

Plugin_table table_global_variables::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "global_variables",
    /* Definition */
    "  VARIABLE_NAME VARCHAR(64) not null,\n"
    "  VARIABLE_VALUE VARCHAR(1024),\n"
    "  PRIMARY KEY (VARIABLE_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_global_variables::m_share = {
    &pfs_readonly_world_acl,
    table_global_variables::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_global_variables::get_row_count,
    sizeof(pos_t),
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_global_variables::create(PFS_engine_table_share *) {
  return new table_global_variables();
}

ha_rows table_global_variables::get_row_count(void) {
  mysql_mutex_lock(&LOCK_plugin_delete);
  mysql_mutex_assert_not_owner(&LOCK_plugin);
  mysql_rwlock_rdlock(&LOCK_system_variables_hash);
  ha_rows system_var_count = get_system_variable_count();
  mysql_rwlock_unlock(&LOCK_system_variables_hash);
  mysql_mutex_unlock(&LOCK_plugin_delete);
  return system_var_count;
}

table_global_variables::table_global_variables()
    : PFS_engine_table(&m_share, &m_pos),
      m_sysvar_cache(false),
      m_pos(0),
      m_next_pos(0) {}

void table_global_variables::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_global_variables::rnd_init(bool /* scan */) {
  /*
    Build a list of system variables from the global system variable hash.
    Filter by scope.
  */
  m_sysvar_cache.materialize_global();

  return 0;
}

int table_global_variables::rnd_next(void) {
  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_sysvar_cache.size();
       m_pos.next()) {
    const System_variable *system_var = m_sysvar_cache.get(m_pos.m_index);
    if (system_var != nullptr) {
      m_next_pos.set_after(&m_pos);
      return make_row(system_var);
    }
  }
  return HA_ERR_END_OF_FILE;
}

int table_global_variables::rnd_pos(const void *pos) {
  set_position(pos);
  assert(m_pos.m_index < m_sysvar_cache.size());

  const System_variable *system_var = m_sysvar_cache.get(m_pos.m_index);
  if (system_var != nullptr) {
    return make_row(system_var);
  }
  return HA_ERR_RECORD_DELETED;
}

int table_global_variables::index_init(uint idx [[maybe_unused]], bool) {
  /*
    Build a list of system variables from the global system variable hash.
    Filter by scope.
  */
  m_sysvar_cache.materialize_global();

  PFS_index_global_variables *result = nullptr;
  assert(idx == 0);
  result = PFS_NEW(PFS_index_global_variables);
  m_opened_index = result;
  m_index = result;

  return 0;
}

int table_global_variables::index_next(void) {
  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_sysvar_cache.size();
       m_pos.next()) {
    const System_variable *system_var = m_sysvar_cache.get(m_pos.m_index);
    if (system_var != nullptr) {
      if (m_opened_index->match(system_var)) {
        if (!make_row(system_var)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}

int table_global_variables::make_row(const System_variable *system_var) {
  if (system_var->is_null()) {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_variable_name.make_row(system_var->m_name, system_var->m_name_length);
  m_row.m_variable_value.make_row(system_var);

  /*
    We are about to return a row to the SQL layer.
    Notify the audit plugins that a global variable is read.
  */
  mysql_audit_notify(current_thd, AUDIT_EVENT(MYSQL_AUDIT_GLOBAL_VARIABLE_GET),
                     m_row.m_variable_name.m_str,
                     m_row.m_variable_value.get_str(),
                     m_row.m_variable_value.get_length());

  return 0;
}

int table_global_variables::read_row_values(TABLE *table, unsigned char *buf,
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
