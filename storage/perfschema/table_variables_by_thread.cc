/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_variables_by_thread.cc
  Table VARIABLES_BY_THREAD (implementation).
*/

#include "storage/perfschema/table_variables_by_thread.h"

#include <stddef.h>
#include <new>

#include "my_dbug.h"
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

bool PFS_index_variables_by_thread::match(PFS_thread *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_variables_by_thread::match(const System_variable *pfs) {
  if (m_fields >= 2) {
    if (!m_key_2.match(pfs)) {
      return false;
    }
  }

  return true;
}

THR_LOCK table_variables_by_thread::m_table_lock;

Plugin_table table_variables_by_thread::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "variables_by_thread",
    /* Definition */
    "  THREAD_ID BIGINT unsigned not null,\n"
    "  VARIABLE_NAME VARCHAR(64) not null,\n"
    "  VARIABLE_VALUE VARCHAR(1024),\n"
    "  PRIMARY KEY (THREAD_ID, VARIABLE_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_variables_by_thread::m_share = {
    &pfs_readonly_acl,
    table_variables_by_thread::create,
    NULL, /* write_row */
    NULL, /* delete_all_rows */
    table_variables_by_thread::get_row_count,
    sizeof(pos_t),
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_variables_by_thread::create(PFS_engine_table_share *) {
  return new table_variables_by_thread();
}

ha_rows table_variables_by_thread::get_row_count(void) {
  mysql_mutex_lock(&LOCK_plugin_delete);
  mysql_rwlock_rdlock(&LOCK_system_variables_hash);
  ulong system_var_count = get_system_variable_hash_records();
  mysql_rwlock_unlock(&LOCK_system_variables_hash);
  mysql_mutex_unlock(&LOCK_plugin_delete);
  return (global_thread_container.get_row_count() * system_var_count);
}

table_variables_by_thread::table_variables_by_thread()
    : PFS_engine_table(&m_share, &m_pos),
      m_sysvar_cache(true),
      m_pos(),
      m_next_pos(),
      m_context(NULL) {}

void table_variables_by_thread::reset_position(void) {
  m_pos.reset();
  m_next_pos.reset();
}

int table_variables_by_thread::rnd_init(bool scan) {
  /* Build array of SHOW_VARs from system variable hash. */
  m_sysvar_cache.initialize_session();

  /* Record the version of the system variable hash, store in TLS. */
  ulonglong hash_version = m_sysvar_cache.get_sysvar_hash_version();

  m_context = (table_variables_by_thread_context *)current_thd->alloc(
      sizeof(table_variables_by_thread_context));
  new (m_context) table_variables_by_thread_context(hash_version, !scan);
  return 0;
}

int table_variables_by_thread::rnd_next(void) {
  if (m_context && !m_context->versions_match()) {
    system_variable_warning();
    return HA_ERR_END_OF_FILE;
  }

  bool has_more_thread = true;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    PFS_thread *pfs_thread =
        global_thread_container.get(m_pos.m_index_1, &has_more_thread);

    /* Materialize all variables for the current thread. Assign a dedicated
     * mem_root. */
    if (m_sysvar_cache.materialize_session(pfs_thread, true) == 0) {
      const System_variable *system_var = m_sysvar_cache.get(m_pos.m_index_2);
      if (system_var != NULL) {
        /* If make_row() fails, get the next thread. */
        if (!make_row(pfs_thread, system_var)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}

int table_variables_by_thread::rnd_pos(const void *pos) {
  if (m_context && !m_context->versions_match()) {
    system_variable_warning();
    return HA_ERR_END_OF_FILE;
  }

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index_1 < global_thread_container.get_row_count());

  PFS_thread *pfs_thread = global_thread_container.get(m_pos.m_index_1);

  /* Materialize the requested variable. */
  if (m_sysvar_cache.materialize_session(pfs_thread, m_pos.m_index_2) == 0) {
    /* Get the first (and only) element from the cache. */
    const System_variable *system_var = m_sysvar_cache.get();
    if (system_var != NULL) {
      return make_row(pfs_thread, system_var);
    }
  }
  return HA_ERR_RECORD_DELETED;
}

int table_variables_by_thread::index_init(uint idx MY_ATTRIBUTE((unused)),
                                          bool) {
  /* Build array of SHOW_VARs from the system variable hash. */
  m_sysvar_cache.initialize_session();

  /* Record the version of the system variable hash, store in TLS. */
  ulonglong hash_version = m_sysvar_cache.get_sysvar_hash_version();
  m_context = (table_variables_by_thread_context *)current_thd->alloc(
      sizeof(table_variables_by_thread_context));
  new (m_context) table_variables_by_thread_context(hash_version, false);

  PFS_index_variables_by_thread *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_variables_by_thread);
  m_opened_index = result;
  m_index = result;

  return 0;
}

int table_variables_by_thread::index_next(void) {
  if (m_context && !m_context->versions_match()) {
    system_variable_warning();
    return HA_ERR_END_OF_FILE;
  }

  bool has_more_thread = true;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    PFS_thread *pfs_thread =
        global_thread_container.get(m_pos.m_index_1, &has_more_thread);

    if (pfs_thread != NULL) {
      if (m_opened_index->match(pfs_thread)) {
        if (m_sysvar_cache.materialize_session(pfs_thread, true) == 0) {
          const System_variable *system_var;
          do {
            system_var = m_sysvar_cache.get(m_pos.m_index_2);
            if (system_var != NULL) {
              if (m_opened_index->match(system_var)) {
                if (!make_row(pfs_thread, system_var)) {
                  m_next_pos.set_after(&m_pos);
                  return 0;
                }
              }
              m_pos.m_index_2++;
            }
          } while (system_var != NULL);
        }
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}

int table_variables_by_thread::make_row(PFS_thread *thread,
                                        const System_variable *system_var) {
  pfs_optimistic_state lock;

  if (system_var->is_null()) {
    return HA_ERR_RECORD_DELETED;
  }

  /* Protect this reader against a thread termination */
  thread->m_lock.begin_optimistic_lock(&lock);

  m_row.m_thread_internal_id = thread->m_thread_internal_id;

  if (m_row.m_variable_name.make_row(system_var->m_name,
                                     system_var->m_name_length) != 0) {
    return HA_ERR_RECORD_DELETED;
  }

  if (m_row.m_variable_value.make_row(system_var) != 0) {
    return HA_ERR_RECORD_DELETED;
  }

  if (!thread->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_variables_by_thread::read_row_values(TABLE *table, unsigned char *buf,
                                               Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* THREAD_ID */
          set_field_ulonglong(f, m_row.m_thread_internal_id);
          break;
        case 1: /* VARIABLE_NAME */
          set_field_varchar_utf8(f, m_row.m_variable_name.m_str,
                                 m_row.m_variable_name.m_length);
          break;
        case 2: /* VARIABLE_VALUE */
          m_row.m_variable_value.set_field(f);
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}
