/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_variables_info.cc
  Table VARIABLES_INFO (implementation).
*/

#include "storage/perfschema/table_variables_info.h"

#include <assert.h>
#include <stddef.h>

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
#include "storage/perfschema/table_helper.h"

THR_LOCK table_variables_info::m_table_lock;

Plugin_table table_variables_info::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "variables_info",
    /* Definition */
    "  VARIABLE_NAME varchar(64) not null,\n"
    "  VARIABLE_SOURCE ENUM('COMPILED','GLOBAL','SERVER','EXPLICIT','EXTRA',\n"
    "                       'USER','LOGIN','COMMAND_LINE','PERSISTED',\n"
    "                       'DYNAMIC') DEFAULT 'COMPILED',\n"
    "  VARIABLE_PATH varchar(1024),\n"
    "  MIN_VALUE varchar(64),\n"
    "  MAX_VALUE varchar(64),\n"
    "  SET_TIME TIMESTAMP(6) default null,\n"
    "  SET_USER CHAR(32) collate utf8mb4_bin default null,\n"
    "  SET_HOST CHAR(255) CHARACTER SET ASCII default null\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_variables_info::m_share = {
    &pfs_readonly_world_acl,
    table_variables_info::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_variables_info::get_row_count,
    sizeof(pos_t),
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_variables_info::create(PFS_engine_table_share *) {
  return new table_variables_info();
}

ha_rows table_variables_info::get_row_count() {
  mysql_mutex_lock(&LOCK_plugin_delete);
#ifndef NDEBUG
  mysql_mutex_assert_not_owner(&LOCK_plugin);
#endif
  mysql_rwlock_rdlock(&LOCK_system_variables_hash);
  const ha_rows system_var_count = get_system_variable_count();
  mysql_rwlock_unlock(&LOCK_system_variables_hash);
  mysql_mutex_unlock(&LOCK_plugin_delete);
  return system_var_count;
}

table_variables_info::table_variables_info()
    : PFS_engine_table(&m_share, &m_pos),
      m_sysvarinfo_cache(false),
      m_row(),
      m_pos(0),
      m_next_pos(0) {}

void table_variables_info::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_variables_info::rnd_init(bool) {
  /* Build a cache of system variables for this thread. */
  m_sysvarinfo_cache.materialize_all(current_thd);
  return 0;
}

int table_variables_info::rnd_next() {
  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_sysvarinfo_cache.size();
       m_pos.next()) {
    if (m_sysvarinfo_cache.is_materialized()) {
      const System_variable *system_var = m_sysvarinfo_cache.get(m_pos.m_index);
      if (system_var != nullptr) {
        m_next_pos.set_after(&m_pos);
        return make_row(system_var);
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}

int table_variables_info::rnd_pos(const void *pos) {
  set_position(pos);
  assert(m_pos.m_index < m_sysvarinfo_cache.size());

  if (m_sysvarinfo_cache.is_materialized()) {
    const System_variable *system_var = m_sysvarinfo_cache.get(m_pos.m_index);
    if (system_var != nullptr) {
      return make_row(system_var);
    }
  }
  return HA_ERR_RECORD_DELETED;
}

int table_variables_info::make_row(const System_variable *system_var) {
  memcpy(m_row.m_variable_name, system_var->m_name, system_var->m_name_length);
  m_row.m_variable_name_length = system_var->m_name_length;

  m_row.m_variable_source = system_var->m_source;

  if (system_var->m_path_length)
    memcpy(m_row.m_variable_path, system_var->m_path_str,
           system_var->m_path_length);
  m_row.m_variable_path_length = system_var->m_path_length;

  memcpy(m_row.m_min_value, system_var->m_min_value_str,
         system_var->m_min_value_length);
  m_row.m_min_value_length = system_var->m_min_value_length;

  memcpy(m_row.m_max_value, system_var->m_max_value_str,
         system_var->m_max_value_length);
  m_row.m_max_value_length = system_var->m_max_value_length;

  m_row.m_set_time = system_var->m_set_time;

  memcpy(m_row.m_set_user_str, system_var->m_set_user_str,
         system_var->m_set_user_str_length);
  m_row.m_set_user_str_length = system_var->m_set_user_str_length;

  memcpy(m_row.m_set_host_str, system_var->m_set_host_str,
         system_var->m_set_host_str_length);
  m_row.m_set_host_str_length = system_var->m_set_host_str_length;

  return 0;
}

int table_variables_info::read_row_values(TABLE *table, unsigned char *buf,
                                          Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* VARIABLE_NAME */
          set_field_varchar_utf8mb4(f, m_row.m_variable_name,
                                    m_row.m_variable_name_length);
          break;
        case 1: /* VARIABLE_SOURCE */
          set_field_enum(f, m_row.m_variable_source);
          break;
        case 2: /* VARIABLE_PATH */
          set_field_varchar_utf8mb4(f, m_row.m_variable_path,
                                    m_row.m_variable_path_length);
          break;
        case 3: /* MIN_VALUE */
          set_field_varchar_utf8mb4(f, m_row.m_min_value,
                                    m_row.m_min_value_length);
          break;
        case 4: /* MAX_VALUE */
          set_field_varchar_utf8mb4(f, m_row.m_max_value,
                                    m_row.m_max_value_length);
          break;
        case 5: /* SET_TIME */
          if (m_row.m_set_time != 0) {
            set_field_timestamp(f, m_row.m_set_time);
          } else {
            f->set_null();
          }
          break;
        case 6: /* SET_USER */
          if (m_row.m_set_user_str_length != 0) {
            set_field_char_utf8mb4(f, m_row.m_set_user_str,
                                   m_row.m_set_user_str_length);
          } else {
            f->set_null();
          }
          break;
        case 7: /* SET_HOST */
          if (m_row.m_set_host_str_length != 0) {
            set_field_char_utf8mb4(f, m_row.m_set_host_str,
                                   m_row.m_set_host_str_length);
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
