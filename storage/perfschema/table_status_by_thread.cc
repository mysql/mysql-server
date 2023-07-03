<<<<<<< HEAD
/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
=======
/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

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
  @file storage/perfschema/table_status_by_thread.cc
  Table STATUS_BY_THREAD (implementation).
*/

#include "storage/perfschema/table_status_by_thread.h"

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

bool PFS_index_status_by_thread::match(PFS_thread *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs)) {
      return false;
    }
  }

  return true;
}

bool PFS_index_status_by_thread::match(const Status_variable *pfs) {
  if (m_fields >= 2) {
    if (!m_key_2.match(pfs)) {
      return false;
    }
  }

  return true;
}

THR_LOCK table_status_by_thread::m_table_lock;

Plugin_table table_status_by_thread::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "status_by_thread",
    /* Definition */
    "  THREAD_ID BIGINT UNSIGNED not null,\n"
    "  VARIABLE_NAME VARCHAR(64) not null,\n"
    "  VARIABLE_VALUE VARCHAR(1024),\n"
    "  PRIMARY KEY (THREAD_ID, VARIABLE_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_status_by_thread::m_share = {
    &pfs_truncatable_acl,
    table_status_by_thread::create,
    nullptr, /* write_row */
    table_status_by_thread::delete_all_rows,
    table_status_by_thread::get_row_count,
    sizeof(pos_t),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

<<<<<<< HEAD
PFS_engine_table *table_status_by_thread::create(PFS_engine_table_share *) {
  return new table_status_by_thread();
}

int table_status_by_thread::delete_all_rows(void) {
<<<<<<< HEAD
  /* Lock required to aggregate to global_status_var. */
=======
  /* Lock required to aggregate to global_status_vars. */
=======
TABLE_FIELD_DEF
table_status_by_thread::m_field_def=
{ 3, field_types };

PFS_engine_table_share_state
table_status_by_thread::m_share_state = {
  false /* m_checked */
};

PFS_engine_table_share
table_status_by_thread::m_share=
{
  { C_STRING_WITH_LEN("status_by_thread") },
  &pfs_truncatable_acl,
  table_status_by_thread::create,
  NULL, /* write_row */
  table_status_by_thread::delete_all_rows,
  table_status_by_thread::get_row_count,
  sizeof(pos_t),
  &m_table_lock,
  &m_field_def,
  false, /* m_perpetual */
  false, /* m_optional */
  &m_share_state
};

PFS_engine_table*
table_status_by_thread::create(void)
{
  return new table_status_by_thread();
}

int table_status_by_thread::delete_all_rows(void)
{
  /* Lock required to aggregate to global_status_var. */
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  mysql_mutex_lock(&LOCK_status);

  reset_status_by_thread();

  mysql_mutex_unlock(&LOCK_status);
  return 0;
}

ha_rows table_status_by_thread::get_row_count(void) {
  mysql_mutex_lock(&LOCK_status);
  size_t status_var_count = all_status_vars.size();
  mysql_mutex_unlock(&LOCK_status);
  return (global_thread_container.get_row_count() * status_var_count);
}

table_status_by_thread::table_status_by_thread()
    : PFS_engine_table(&m_share, &m_pos),
      m_status_cache(true),
      m_pos(),
      m_next_pos() {}

void table_status_by_thread::reset_position(void) {
  m_pos.reset();
  m_next_pos.reset();
}

int table_status_by_thread::rnd_init(bool /* scan */) {
  /* Build array of SHOW_VARs from the global status array prior to
   * materializing. */
  m_status_cache.initialize_session();

  return 0;
}

int table_status_by_thread::rnd_next(void) {
  bool has_more_thread = true;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    PFS_thread *pfs_thread =
        global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (m_status_cache.materialize_session(pfs_thread) == 0) {
      const Status_variable *stat_var = m_status_cache.get(m_pos.m_index_2);
      if (stat_var != nullptr) {
        /* If make_row() fails go to the next thread. */
        if (!make_row(pfs_thread, stat_var)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}

int table_status_by_thread::rnd_pos(const void *pos) {
  set_position(pos);
  assert(m_pos.m_index_1 < global_thread_container.get_row_count());

  PFS_thread *pfs_thread = global_thread_container.get(m_pos.m_index_1);

  if (m_status_cache.materialize_session(pfs_thread) == 0) {
    const Status_variable *stat_var = m_status_cache.get(m_pos.m_index_2);
    if (stat_var != nullptr) {
      return make_row(pfs_thread, stat_var);
    }
  }
  return HA_ERR_RECORD_DELETED;
}

int table_status_by_thread::index_init(uint idx [[maybe_unused]], bool) {
  /* Build array of SHOW_VARs from the global status array. */
  m_status_cache.initialize_session();

  PFS_index_status_by_thread *result = nullptr;
  assert(idx == 0);
  result = PFS_NEW(PFS_index_status_by_thread);
  m_opened_index = result;
  m_index = result;

  return 0;
}

int table_status_by_thread::index_next(void) {
  bool has_more_thread = true;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    PFS_thread *pfs_thread =
        global_thread_container.get(m_pos.m_index_1, &has_more_thread);

    if (pfs_thread != nullptr) {
      if (m_opened_index->match(pfs_thread)) {
        if (m_status_cache.materialize_session(pfs_thread) == 0) {
          const Status_variable *stat_var;
          do {
            stat_var = m_status_cache.get(m_pos.m_index_2);
            if (stat_var != nullptr) {
              if (m_opened_index->match(stat_var)) {
                if (!make_row(pfs_thread, stat_var)) {
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

int table_status_by_thread::make_row(PFS_thread *thread,
                                     const Status_variable *status_var) {
  pfs_optimistic_state lock;
  if (status_var->is_null()) {
    return HA_ERR_RECORD_DELETED;
  }

  /* Protect this reader against a thread termination */
  thread->m_lock.begin_optimistic_lock(&lock);

  m_row.m_thread_internal_id = thread->m_thread_internal_id;

  if (m_row.m_variable_name.make_row(status_var->m_name,
                                     status_var->m_name_length)) {
    return HA_ERR_RECORD_DELETED;
  }

  if (m_row.m_variable_value.make_row(status_var)) {
    return HA_ERR_RECORD_DELETED;
  }

  if (!thread->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_status_by_thread::read_row_values(TABLE *table, unsigned char *buf,
                                            Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
<<<<<<< HEAD
  assert(table->s->null_bytes == 1);
=======
<<<<<<< HEAD
  DBUG_ASSERT(table->s->null_bytes == 1);
>>>>>>> pr/231
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /* THREAD_ID */
          set_field_ulonglong(f, m_row.m_thread_internal_id);
          break;
        case 1: /* VARIABLE_NAME */
          set_field_varchar_utf8mb4(f, m_row.m_variable_name.m_str,
                                    m_row.m_variable_name.m_length);
          break;
        case 2: /* VARIABLE_VALUE */
          m_row.m_variable_value.set_field(f);
          break;
        default:
<<<<<<< HEAD
          assert(false);
=======
          DBUG_ASSERT(false);
=======
  assert(table->s->null_bytes == 1);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* THREAD_ID */
        set_field_ulonglong(f, m_row.m_thread_internal_id);
        break;
      case 1: /* VARIABLE_NAME */
        set_field_varchar_utf8(f, m_row.m_variable_name.m_str, m_row.m_variable_name.m_length);
        break;
      case 2: /* VARIABLE_VALUE */
        m_row.m_variable_value.set_field(f);
        break;
      default:
        assert(false);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
      }
    }
  }

  return 0;
}
