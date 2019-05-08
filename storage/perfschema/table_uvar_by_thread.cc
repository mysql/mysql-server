/* Copyright (c) 2013, 2019, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_uvar_by_thread.cc
  Table USER_VARIABLES_BY_THREAD (implementation).
*/

#include "storage/perfschema/table_uvar_by_thread.h"

#include "my_dbug.h"
#include "my_thread.h"
#include "sql/item_func.h"
#include "sql/mysqld_thd_manager.h"
#include "sql/plugin_table.h"
/* Iteration on THD from the sql layer. */
#include "sql/sql_class.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_visitor.h"

class Find_thd_user_var : public Find_THD_Impl {
 public:
  Find_thd_user_var(THD *unsafe_thd) : m_unsafe_thd(unsafe_thd) {}

  virtual bool operator()(THD *thd) {
    if (thd != m_unsafe_thd) {
      return false;
    }

    if (thd->user_vars.empty()) {
      return false;
    }

    mysql_mutex_lock(&thd->LOCK_thd_data);
    return true;
  }

 private:
  THD *m_unsafe_thd;
};

void User_variables::materialize(PFS_thread *pfs, THD *thd) {
  reset();

  m_pfs = pfs;
  m_thread_internal_id = pfs->m_thread_internal_id;
  m_array.reserve(thd->user_vars.size());

  User_variable empty;

  /* Protects thd->user_vars. */
  mysql_mutex_assert_owner(&thd->LOCK_thd_data);

  for (const auto &key_and_value : thd->user_vars) {
    user_var_entry *sql_uvar = key_and_value.second.get();

    /*
      m_array is a container of objects (not pointers)

      Naive code can:
      - build locally a new entry
      - add it to the container
      but this causes useless object construction, destruction, and deep copies.

      What we do here:
      - add a dummy (empty) entry
      - the container does a deep copy on something empty,
        so that there is nothing to copy.
      - get a reference to the entry added in the container
      - complete -- in place -- the entry initialization
    */
    m_array.push_back(empty);
    User_variable &pfs_uvar = m_array.back();

    /* Copy VARIABLE_NAME */
    const char *name = sql_uvar->entry_name.ptr();
    size_t name_length = sql_uvar->entry_name.length();
    DBUG_ASSERT(name_length <= sizeof(pfs_uvar.m_name));
    pfs_uvar.m_name.make_row(name, name_length);

    /* Copy VARIABLE_VALUE */
    bool null_value;
    String *str_value;
    String str_buffer;
    uint decimals = 0;
    str_value = sql_uvar->val_str(&null_value, &str_buffer, decimals);
    if (str_value != NULL) {
      pfs_uvar.m_value.make_row(str_value->ptr(), str_value->length());
    } else {
      pfs_uvar.m_value.make_row(NULL, 0);
    }
  }
}

THR_LOCK table_uvar_by_thread::m_table_lock;

Plugin_table table_uvar_by_thread::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "user_variables_by_thread",
    /* Definition */
    "  THREAD_ID BIGINT unsigned not null,\n"
    "  VARIABLE_NAME VARCHAR(64) not null,\n"
    "  VARIABLE_VALUE LONGBLOB,\n"
    "  PRIMARY KEY (THREAD_ID, VARIABLE_NAME) USING HASH\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_uvar_by_thread::m_share = {
    &pfs_readonly_acl,
    table_uvar_by_thread::create,
    NULL, /* write_row */
    NULL, /* delete_all_rows */
    table_uvar_by_thread::get_row_count,
    sizeof(pos_t),
    &m_table_lock,
    &m_table_def,
    false, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

bool PFS_index_uvar_by_thread::match(PFS_thread *pfs) {
  if (m_fields >= 1) {
    if (!m_key_1.match(pfs)) {
      return false;
    }
  }
  return true;
}

bool PFS_index_uvar_by_thread::match(const User_variable *pfs) {
  if (m_fields >= 2) {
    if (!m_key_2.match(&pfs->m_name)) {
      return false;
    }
  }
  return true;
}

PFS_engine_table *table_uvar_by_thread::create(PFS_engine_table_share *) {
  return new table_uvar_by_thread();
}

ha_rows table_uvar_by_thread::get_row_count(void) {
  /*
    This is an estimate only, not a hard limit.
    The row count is given as a multiple of thread_max,
    so that a join between:
    - table performance_schema.threads
    - table performance_schema.user_variables_by_thread
    will still evaluate relative table sizes correctly
    when deciding a join order.
  */
  return global_thread_container.get_row_count() * 10;
}

table_uvar_by_thread::table_uvar_by_thread()
    : PFS_engine_table(&m_share, &m_pos), m_pos(), m_next_pos() {}

void table_uvar_by_thread::reset_position(void) {
  m_pos.reset();
  m_next_pos.reset();
}

int table_uvar_by_thread::rnd_next(void) {
  PFS_thread *thread;
  bool has_more_thread = true;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (thread != NULL) {
      if (materialize(thread) == 0) {
        const User_variable *uvar = m_THD_cache.get(m_pos.m_index_2);
        if (uvar != NULL) {
          /* If make_row() fails, get the next thread. */
          if (!make_row(thread, uvar)) {
            m_next_pos.set_after(&m_pos);
            return 0;
          }
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_uvar_by_thread::rnd_pos(const void *pos) {
  PFS_thread *thread;

  set_position(pos);

  thread = global_thread_container.get(m_pos.m_index_1);
  if (thread != NULL) {
    if (materialize(thread) == 0) {
      const User_variable *uvar = m_THD_cache.get(m_pos.m_index_2);
      if (uvar != NULL) {
        return make_row(thread, uvar);
      }
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int table_uvar_by_thread::index_init(uint idx MY_ATTRIBUTE((unused)), bool) {
  PFS_index_uvar_by_thread *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_uvar_by_thread);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int table_uvar_by_thread::index_next(void) {
  PFS_thread *thread;
  bool has_more_thread = true;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread()) {
    thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (thread != NULL) {
      if (m_opened_index->match(thread)) {
        if (materialize(thread) == 0) {
          const User_variable *uvar;
          do {
            uvar = m_THD_cache.get(m_pos.m_index_2);
            if (uvar != NULL) {
              if (m_opened_index->match(uvar)) {
                if (!make_row(thread, uvar)) {
                  m_next_pos.set_after(&m_pos);
                  return 0;
                }
              }
              m_pos.m_index_2++;
            }
          } while (uvar != NULL);
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_uvar_by_thread::materialize(PFS_thread *thread) {
  if (m_THD_cache.is_materialized(thread)) {
    return 0;
  }

  if (!thread->m_lock.is_populated()) {
    return 1;
  }

  THD *unsafe_thd = thread->m_thd;
  if (unsafe_thd == NULL) {
    return 1;
  }

  Find_thd_user_var finder(unsafe_thd);
  THD *safe_thd = Global_THD_manager::get_instance()->find_thd(&finder);
  if (safe_thd == NULL) {
    return 1;
  }

  m_THD_cache.materialize(thread, safe_thd);
  mysql_mutex_unlock(&safe_thd->LOCK_thd_data);
  return 0;
}

int table_uvar_by_thread::make_row(PFS_thread *thread,
                                   const User_variable *uvar) {
  pfs_optimistic_state lock;

  /* Protect this reader against a thread termination */
  thread->m_lock.begin_optimistic_lock(&lock);

  m_row.m_thread_internal_id = thread->m_thread_internal_id;

  /* uvar is materialized, pointing to it directly. */
  m_row.m_variable_name = &uvar->m_name;
  m_row.m_variable_value = &uvar->m_value;

  if (!thread->m_lock.end_optimistic_lock(&lock)) {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int table_uvar_by_thread::read_row_values(TABLE *table, unsigned char *buf,
                                          Field **fields, bool read_all) {
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  DBUG_ASSERT(m_row.m_variable_name != NULL);
  DBUG_ASSERT(m_row.m_variable_value != NULL);

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* THREAD_ID */
          set_field_ulonglong(f, m_row.m_thread_internal_id);
          break;
        case 1: /* VARIABLE_NAME */
          set_field_varchar_utf8(f, m_row.m_variable_name->m_str,
                                 m_row.m_variable_name->m_length);
          break;
        case 2: /* VARIABLE_VALUE */
          if (m_row.m_variable_value->get_value_length() > 0) {
            set_field_blob(f, m_row.m_variable_value->get_value(),
                           m_row.m_variable_value->get_value_length());
          } else {
            f->set_null();
          }
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}
