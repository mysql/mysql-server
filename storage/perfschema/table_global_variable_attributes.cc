/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_global_variable_attributes.cc
  Table GLOBAL_VARIABLE_ATTRIBUTES (implementation).
*/

#include "storage/perfschema/table_global_variable_attributes.h"

#include <assert.h>
#include <stddef.h>

#include "my_thread.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/mysqld.h"
#include "sql/plugin_table.h"
#include "sql/sql_class.h"
#include "sql/sys_vars.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/table_helper.h"

row_global_variable_attributes::row_global_variable_attributes()
    : m_variable_name_length(0), m_attr_name_length(0), m_attr_value_length(0) {
  m_variable_name[0] = '\0';
  m_attr_name[0] = '\0';
  m_attr_value[0] = '\0';
}

row_global_variable_attributes::row_global_variable_attributes(
    const sys_var *system_var, const char *attribute_name,
    const char *attribute_value) {
  assert(system_var != nullptr);

  // materialize the row
  m_variable_name_length = system_var->name.length;
  memcpy(m_variable_name, system_var->name.str, m_variable_name_length);

  m_attr_name_length = strlen(attribute_name);
  memcpy(m_attr_name, attribute_name, m_attr_name_length);

  m_attr_value_length = strlen(attribute_value);
  memcpy(m_attr_value, attribute_value, m_attr_value_length);
}

row_global_variable_attributes::row_global_variable_attributes(
    const row_global_variable_attributes &other) {
  operator=(other);
}

row_global_variable_attributes &row_global_variable_attributes::operator=(
    const row_global_variable_attributes &other) {
  if (this != &other) {
    memcpy(m_variable_name, other.m_variable_name,
           other.m_variable_name_length);
    m_variable_name_length = other.m_variable_name_length;
    memcpy(m_attr_name, other.m_attr_name, other.m_attr_name_length);
    m_attr_name_length = other.m_attr_name_length;
    memcpy(m_attr_value, other.m_attr_value, other.m_attr_value_length);
    m_attr_value_length = other.m_attr_value_length;
  }
  return *this;
}

THR_LOCK table_global_variable_attributes::m_table_lock;

Plugin_table table_global_variable_attributes::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "global_variable_attributes",
    /* Definition */
    "  VARIABLE_NAME varchar(64) NOT NULL,\n"
    "  ATTR_NAME varchar(32) NOT NULL,\n"
    "  ATTR_VALUE varchar(1024) NOT NULL\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_global_variable_attributes::m_share = {
    &pfs_readonly_world_acl,
    table_global_variable_attributes::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_global_variable_attributes::get_row_count,
    sizeof(pos_t),
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_global_variable_attributes::create(
    PFS_engine_table_share *) {
  return new table_global_variable_attributes();
}

ha_rows table_global_variable_attributes::get_row_count() { return 10; }

table_global_variable_attributes::table_global_variable_attributes()
    : PFS_engine_table(&m_share, &m_pos),
      m_initialized(false),
      m_sys_var_tracker_array(PSI_INSTRUMENT_ME),
      m_cache(PSI_INSTRUMENT_ME),
      m_materialized(false),
      m_version(0),
      m_pos(0),
      m_next_pos(0) {}

/**
  Build a sorted list of all system variables from the system variable hash.
  Filter by scope. Must be called inside of LOCK_plugin_delete.
*/
bool table_global_variable_attributes::init_sys_var_array() {
  assert(!m_initialized);

  // enumerate both GLOBAL and SESSION system variables
  constexpr enum_var_type scope = OPT_SESSION;
  constexpr bool strict = false;

#ifndef NDEBUG
  mysql_mutex_assert_not_owner(&LOCK_plugin);
#endif
  mysql_rwlock_rdlock(&LOCK_system_variables_hash);

  /* Record the system variable hash version to detect subsequent changes. */
  m_version = get_dynamic_system_variable_hash_version();

  /* Build the SHOW_VAR array from the system variable hash. */
  System_variable_tracker::enumerate_sys_vars(true, scope, strict,
                                              &m_sys_var_tracker_array);

  mysql_rwlock_unlock(&LOCK_system_variables_hash);

  /* Increase cache size if necessary. */
  m_cache.reserve(m_sys_var_tracker_array.size());

  m_initialized = true;
  return true;
}

/**
  Build a GLOBAL system variable cache.
*/
int table_global_variable_attributes::do_materialize_all() {
  m_materialized = false;
  m_cache.clear();

  /* Block plugins from unloading. */
  MUTEX_LOCK(plugin_delete_lock_guard, &LOCK_plugin_delete);

  /* Block system variable additions or deletions. */
  mysql_mutex_lock(&LOCK_global_system_variables);

  /*
     Build array of sys_vars from system variable hash. Do this within
     LOCK_plugin_delete to ensure that the hash table remains unchanged
     while this thread is materialized.
   */
  init_sys_var_array();

  for (const System_variable_tracker &i : m_sys_var_tracker_array) {
    auto f = [this](const System_variable_tracker &, sys_var *sysvar) {
      if ((sysvar->scope() & sys_var::flag_enum::GLOBAL) == 0) return;
      for (auto &attr : sysvar->m_global_attributes) {
        const char *attribute_name = attr.first.c_str();
        const char *attribute_value = attr.second.c_str();
        const row_global_variable_attributes info(sysvar, attribute_name,
                                                  attribute_value);
        m_cache.push_back(info);
      }
    };
    (void)i.access_system_variable(current_thd, f,
                                   Suppress_not_found_error::YES);
  }

  m_materialized = true;
  mysql_mutex_unlock(&LOCK_global_system_variables);

  return 0;
}

void table_global_variable_attributes::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_global_variable_attributes::rnd_init(bool) {
  /* Build a cache of system variables for this thread. */
  do_materialize_all();
  return 0;
}

int table_global_variable_attributes::rnd_next() {
  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_cache.size();
       m_pos.next()) {
    const row_global_variable_attributes &info = m_cache.at(m_pos.m_index);
    m_next_pos.set_after(&m_pos);
    return make_row(info);
  }

  return HA_ERR_END_OF_FILE;
}

int table_global_variable_attributes::rnd_pos(const void *pos) {
  set_position(pos);
  assert(m_pos.m_index < m_cache.size());

  const row_global_variable_attributes &info = m_cache.at(m_pos.m_index);
  return make_row(info);
}

int table_global_variable_attributes::make_row(
    const row_global_variable_attributes &row) {
  m_row = row;
  return 0;
}

int table_global_variable_attributes::read_row_values(TABLE *table,
                                                      unsigned char * /*buf*/,
                                                      Field **fields,
                                                      bool read_all) {
  Field *f;

  /* Set the null bits */
  assert(table->s->null_bytes == 0);

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* VARIABLE_NAME */
          set_field_varchar_utf8mb4(f, m_row.m_variable_name,
                                    m_row.m_variable_name_length);
          break;
        case 1: /* ATTR_NAME */
          set_field_varchar_utf8mb4(f, m_row.m_attr_name,
                                    m_row.m_attr_name_length);
          break;
        case 2: /* ATTR_VALUE */
          set_field_varchar_utf8mb4(f, m_row.m_attr_value,
                                    m_row.m_attr_value_length);
          break;
        default:
          assert(false);
      }
    }
  }
  return 0;
}
