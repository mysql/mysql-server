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
  @file storage/perfschema/table_variables_metadata.cc
  Table VARIABLES_METADATA (implementation).
*/

#include "storage/perfschema/table_variables_metadata.h"

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

THR_LOCK table_variables_metadata::m_table_lock;

Plugin_table table_variables_metadata::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "variables_metadata",
    /* Definition */
    "  VARIABLE_NAME varchar(64) NOT NULL,\n"
    "  VARIABLE_SCOPE enum('GLOBAL','SESSION','SESSION_ONLY') NOT NULL,\n"
    "  DATA_TYPE "
    "enum('Integer','Numeric','String','Enumeration','Boolean','Set') "
    "NOT NULL,\n"
    "  MIN_VALUE varchar(64),\n"
    "  MAX_VALUE varchar(64),\n"
    "  DOCUMENTATION mediumtext NOT NULL\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_variables_metadata::m_share = {
    &pfs_readonly_world_acl,
    table_variables_metadata::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_variables_metadata::get_row_count,
    sizeof(pos_t),
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

row_variables_metadata::row_variables_metadata()
    : m_variable_name_length(0),
      m_variable_scope(SCOPE_GLOBAL),
      m_variable_data_type(TYPE_INTEGER),
      m_min_value_length(0),
      m_max_value_length(0),
      m_documentation_length(0) {
  m_variable_name[0] = '\0';
  m_min_value[0] = '\0';
  m_max_value[0] = '\0';
  m_documentation[0] = '\0';
}

row_variables_metadata::row_variables_metadata(sys_var *system_var)
    : m_variable_data_type(TYPE_INTEGER) {
  assert(system_var != nullptr);

  memcpy(m_variable_name, system_var->name.str, system_var->name.length);
  m_variable_name_length = system_var->name.length;

  const int scope = system_var->scope();
  if (scope & sys_var::ONLY_SESSION)
    m_variable_scope = SCOPE_SESSION_ONLY;
  else if (scope & sys_var::SESSION)
    m_variable_scope = SCOPE_SESSION;
  else
    m_variable_scope = SCOPE_GLOBAL;

  const ulong vartype = system_var->get_var_type();
  switch (vartype) {
    case GET_INT:
    case GET_UINT:
    case GET_LONG:
    case GET_ULONG:
    case GET_LL:
    case GET_ULL:
      m_variable_data_type = TYPE_INTEGER;
      break;
    case GET_STR:
    case GET_STR_ALLOC:
    case GET_PASSWORD:
      m_variable_data_type = TYPE_STRING;
      break;
    case GET_ENUM:
      m_variable_data_type = TYPE_ENUMERATION;
      break;
    case GET_BOOL:
      m_variable_data_type = TYPE_BOOLEAN;
      break;
    case GET_DOUBLE:
      m_variable_data_type = TYPE_NUMERIC;
      break;
    case GET_FLAGSET:
    case GET_SET:
      m_variable_data_type = TYPE_SET;
      break;
    default: {
      // plugin system variables do not seem to have
      // correct get_var_type() value set, so fallback
      // to show_type() here
      const SHOW_TYPE type = system_var->show_type();

      // not all SHOW_TYPEs were mapped
      if (type == SHOW_INT || type == SHOW_LONG || type == SHOW_LONGLONG ||
          type == SHOW_SIGNED_INT || type == SHOW_SIGNED_LONG ||
          type == SHOW_SIGNED_LONGLONG || type == SHOW_LONG_NOFLUSH)
        m_variable_data_type = TYPE_INTEGER;
      else if (type == SHOW_CHAR || type == SHOW_CHAR_PTR)
        m_variable_data_type = TYPE_STRING;
      else if (type == SHOW_BOOL || type == SHOW_MY_BOOL)
        m_variable_data_type = TYPE_BOOLEAN;
      else if (type == SHOW_DOUBLE)
        m_variable_data_type = TYPE_NUMERIC;
      else
        // on assert crash, please adapt the code to
        // add support for a new unkown system variable type!
        assert(false);
    }
  }

  if (m_variable_data_type == TYPE_INTEGER ||
      m_variable_data_type == TYPE_NUMERIC) {
    (void)snprintf(m_min_value, sizeof(m_min_value), "%lld",
                   system_var->get_min_value());
    m_min_value_length = strlen(m_min_value);
    (void)snprintf(m_max_value, sizeof(m_max_value), "%llu",
                   system_var->get_max_value());
    m_max_value_length = strlen(m_max_value);
  } else {
    m_min_value_length = 0;
    m_max_value_length = 0;
  }
  m_documentation_length = std::min(strlen(system_var->get_option()->comment),
                                    sizeof(m_documentation));
  memcpy(m_documentation, system_var->get_option()->comment,
         m_documentation_length);
}

row_variables_metadata::row_variables_metadata(
    const row_variables_metadata &other) {
  operator=(other);
}

row_variables_metadata &row_variables_metadata::operator=(
    const row_variables_metadata &other) {
  if (this != &other) {
    memcpy(m_variable_name, other.m_variable_name,
           other.m_variable_name_length);
    m_variable_name_length = other.m_variable_name_length;
    m_variable_scope = other.m_variable_scope;
    m_variable_data_type = other.m_variable_data_type;
    memcpy(m_min_value, other.m_min_value, other.m_min_value_length);
    m_min_value_length = other.m_min_value_length;
    memcpy(m_max_value, other.m_max_value, other.m_max_value_length);
    m_max_value_length = other.m_max_value_length;
    memcpy(m_documentation, other.m_documentation,
           other.m_documentation_length);
    m_documentation_length = other.m_documentation_length;
  }
  return *this;
}

PFS_engine_table *table_variables_metadata::create(PFS_engine_table_share *) {
  return new table_variables_metadata();
}

ha_rows table_variables_metadata::get_row_count() {
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

table_variables_metadata::table_variables_metadata()
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
bool table_variables_metadata::init_sys_var_array() {
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
int table_variables_metadata::do_materialize_all() {
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
      const row_variables_metadata info(sysvar);
      m_cache.push_back(info);
    };
    (void)i.access_system_variable(current_thd, f,
                                   Suppress_not_found_error::YES);
  }

  m_materialized = true;

  mysql_mutex_unlock(&LOCK_global_system_variables);

  return 0;
}

void table_variables_metadata::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_variables_metadata::rnd_init(bool) {
  /* Build a cache of system variables for this thread. */
  do_materialize_all();
  return 0;
}

int table_variables_metadata::rnd_next() {
  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_cache.size();
       m_pos.next()) {
    const row_variables_metadata &info = m_cache.at(m_pos.m_index);
    m_next_pos.set_after(&m_pos);
    return make_row(info);
  }
  return HA_ERR_END_OF_FILE;
}

int table_variables_metadata::rnd_pos(const void *pos) {
  set_position(pos);
  assert(m_pos.m_index < m_cache.size());

  const row_variables_metadata &info = m_cache.at(m_pos.m_index);
  return make_row(info);
}

int table_variables_metadata::make_row(const row_variables_metadata &row) {
  m_row = row;
  return 0;
}

int table_variables_metadata::read_row_values(TABLE *table, unsigned char *buf,
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
        case 1: /* VARIABLE_SCOPE */
          set_field_enum(f, m_row.m_variable_scope);
          break;
        case 2: /* DATA_TYPE */
          set_field_enum(f, m_row.m_variable_data_type);
          break;
        case 3: /* MIN_VALUE */
          set_field_varchar_utf8mb4(f, m_row.m_min_value,
                                    m_row.m_min_value_length);
          break;
        case 4: /* MAX_VALUE */
          set_field_varchar_utf8mb4(f, m_row.m_max_value,
                                    m_row.m_max_value_length);
          break;
        case 5: /* DOCUMENTATION */
          set_field_text(f, m_row.m_documentation, m_row.m_documentation_length,
                         &my_charset_utf8mb4_bin);
          break;
        default:
          assert(false);
      }
    }
  }
  return 0;
}
