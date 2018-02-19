/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_persisted_variables.cc
  Table PERSISTED_VARIABLES (implementation).
*/

#include "storage/perfschema/table_persisted_variables.h"

#include <new>

#include "my_thread.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/mysqld.h"
#include "sql/persisted_variable.h"
#include "sql/plugin_table.h"
#include "sql/sql_class.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"

bool PFS_index_persisted_variables::match(const System_variable *pfs) {
  if (m_fields >= 1) {
    if (!m_key.match(pfs)) return false;
  }

  return true;
}

THR_LOCK table_persisted_variables::m_table_lock;

Plugin_table table_persisted_variables::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "persisted_variables",
    /* Definition */
    "  VARIABLE_NAME VARCHAR(64) not null,\n"
    "  VARIABLE_VALUE VARCHAR(1024),\n"
    "  PRIMARY KEY (VARIABLE_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_persisted_variables::m_share = {
    &pfs_readonly_world_acl,
    table_persisted_variables::create,
    NULL, /* write_row */
    NULL, /* delete_all_rows */
    table_persisted_variables::get_row_count,
    sizeof(pos_t),
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_persisted_variables::create(PFS_engine_table_share *) {
  return new table_persisted_variables();
}

ha_rows table_persisted_variables::get_row_count(void) {
  Persisted_variables_cache *pv = Persisted_variables_cache::get_instance();
  if (pv)
    return pv->get_persisted_variables()->size();
  else
    return 0;
}

table_persisted_variables::table_persisted_variables()
    : PFS_engine_table(&m_share, &m_pos),
      m_sysvar_cache(false),
      m_pos(0),
      m_next_pos(0),
      m_context(NULL) {}

void table_persisted_variables::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_persisted_variables::rnd_init(bool scan) {
  /* Build a cache of system variables for this thread. */
  m_sysvar_cache.materialize_all(current_thd);

  /* Record the version of the system variable hash, store in TLS. */
  ulonglong hash_version = m_sysvar_cache.get_sysvar_hash_version();
  m_context = (table_persisted_variables_context *)current_thd->alloc(
      sizeof(table_persisted_variables_context));
  new (m_context) table_persisted_variables_context(hash_version, !scan);
  return 0;
}

int table_persisted_variables::rnd_next(void) {
  if (m_context && !m_context->versions_match()) {
    system_variable_warning();
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_sysvar_cache.size();
       m_pos.next()) {
    if (m_sysvar_cache.is_materialized()) {
      const System_variable *system_var = m_sysvar_cache.get(m_pos.m_index);
      if (system_var != NULL) {
        if (!make_row(system_var)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}

int table_persisted_variables::rnd_pos(const void *pos) {
  if (!m_context->versions_match()) {
    system_variable_warning();
    return HA_ERR_RECORD_DELETED;
  }

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < m_sysvar_cache.size());

  if (m_sysvar_cache.is_materialized()) {
    const System_variable *system_var = m_sysvar_cache.get(m_pos.m_index);
    if (system_var != NULL) {
      return make_row(system_var);
    }
  }
  return HA_ERR_RECORD_DELETED;
}

int table_persisted_variables::index_init(uint idx MY_ATTRIBUTE((unused)),
                                          bool) {
  /*
    Build a cache of system variables for this thread.
  */
  m_sysvar_cache.materialize_all(current_thd);

  /* Record the version of the system variable hash, store in TLS. */
  ulonglong hash_version = m_sysvar_cache.get_sysvar_hash_version();
  m_context = (table_persisted_variables_context *)current_thd->alloc(
      sizeof(table_persisted_variables_context));
  new (m_context) table_persisted_variables_context(hash_version, false);

  PFS_index_persisted_variables *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_persisted_variables);
  m_opened_index = result;
  m_index = result;

  return 0;
}

int table_persisted_variables::index_next(void) {
  if (m_context && !m_context->versions_match()) {
    system_variable_warning();
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_sysvar_cache.size();
       m_pos.next()) {
    if (m_sysvar_cache.is_materialized()) {
      const System_variable *system_var = m_sysvar_cache.get(m_pos.m_index);
      if (system_var != NULL) {
        if (m_opened_index->match(system_var)) {
          if (!make_row(system_var)) {
            m_next_pos.set_after(&m_pos);
            return 0;
          }
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_persisted_variables::make_row(const System_variable *system_var) {
  if (m_row.m_variable_name.make_row(system_var->m_name,
                                     system_var->m_name_length))
    return HA_ERR_RECORD_DELETED;

  if (m_row.m_variable_value.make_row(system_var)) return HA_ERR_RECORD_DELETED;

  return 0;
}

int table_persisted_variables::read_row_values(TABLE *table, unsigned char *buf,
                                               Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* VARIABLE_NAME */
          set_field_varchar_utf8(f, m_row.m_variable_name.m_str,
                                 m_row.m_variable_name.m_length);
          break;
        case 1: /* VARIABLE_VALUE */
          m_row.m_variable_value.set_field(f);
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}
