/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_session_status.cc
  Table SESSION_STATUS (implementation).
*/

#include "storage/perfschema/table_session_status.h"

#include <stddef.h>
#include <new>

#include "my_dbug.h"
#include "my_thread.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/mysqld.h"
#include "sql/sql_class.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"

bool
PFS_index_session_status::match(const Status_variable *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key.match(pfs))
    {
      return false;
    }
  }

  return true;
}

THR_LOCK table_session_status::m_table_lock;

Plugin_table table_session_status::m_table_def(
  /* Schema name */
  "performance_schema",
  /* Name */
  "session_status",
  /* Definition */
  "  VARIABLE_NAME VARCHAR(64) not null,\n"
  "  VARIABLE_VALUE VARCHAR(1024),\n"
  "  PRIMARY KEY (VARIABLE_NAME) USING HASH\n",
  /* Options */
  " ENGINE=PERFORMANCE_SCHEMA",
  /* Tablespace */
  nullptr);

PFS_engine_table_share table_session_status::m_share = {
  &pfs_readonly_world_acl,
  table_session_status::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_session_status::get_row_count,
  sizeof(pos_t),
  &m_table_lock,
  &m_table_def,
  true, /* perpetual */
  PFS_engine_table_proxy(),
  {0},
  false /* m_in_purgatory */
};

PFS_engine_table *
table_session_status::create(PFS_engine_table_share *)
{
  return new table_session_status();
}

ha_rows
table_session_status::get_row_count(void)
{
  mysql_mutex_lock(&LOCK_status);
  ha_rows status_var_count = all_status_vars.size();
  mysql_mutex_unlock(&LOCK_status);
  return status_var_count;
}

table_session_status::table_session_status()
  : PFS_engine_table(&m_share, &m_pos),
    m_status_cache(false),
    m_pos(0),
    m_next_pos(0),
    m_context(NULL)
{
}

void
table_session_status::reset_position(void)
{
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int
table_session_status::rnd_init(bool scan)
{
  /* Build a cache of all status variables for this thread. */
  m_status_cache.materialize_all(current_thd);

  /* Record the version of the global status variable array, store in TLS. */
  ulonglong status_version = m_status_cache.get_status_array_version();
  m_context = (table_session_status_context *)current_thd->alloc(
    sizeof(table_session_status_context));
  new (m_context) table_session_status_context(status_version, !scan);
  return 0;
}

int
table_session_status::rnd_next(void)
{
  if (m_context && !m_context->versions_match())
  {
    status_variable_warning();
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_status_cache.size();
       m_pos.next())
  {
    if (m_status_cache.is_materialized())
    {
      const Status_variable *status_var = m_status_cache.get(m_pos.m_index);
      if (status_var != NULL)
      {
        /* If make_row() fails just get the next variable. */
        if (!make_row(status_var))
        {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}

int
table_session_status::rnd_pos(const void *pos)
{
  if (m_context && !m_context->versions_match())
  {
    status_variable_warning();
    return HA_ERR_END_OF_FILE;
  }

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < m_status_cache.size());

  if (m_status_cache.is_materialized())
  {
    const Status_variable *stat_var = m_status_cache.get(m_pos.m_index);
    if (stat_var != NULL)
    {
      return make_row(stat_var);
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_session_status::index_init(uint idx MY_ATTRIBUTE((unused)), bool)
{
  /* Build a cache of all status variables for this thread. */
  m_status_cache.materialize_all(current_thd);

  /* Record the version of the global status variable array, store in TLS. */
  ulonglong status_version = m_status_cache.get_status_array_version();
  m_context = (table_session_status_context *)current_thd->alloc(
    sizeof(table_session_status_context));
  new (m_context) table_session_status_context(status_version, false);

  PFS_index_session_status *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_session_status);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int
table_session_status::index_next(void)
{
  if (m_context && !m_context->versions_match())
  {
    status_variable_warning();
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_status_cache.size();
       m_pos.next())
  {
    if (m_status_cache.is_materialized())
    {
      const Status_variable *status_var = m_status_cache.get(m_pos.m_index);
      if (status_var != NULL)
      {
        if (m_opened_index->match(status_var))
        {
          if (!make_row(status_var))
          {
            m_next_pos.set_after(&m_pos);
            return 0;
          }
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_session_status::make_row(const Status_variable *status_var)
{
  if (m_row.m_variable_name.make_row(status_var->m_name,
                                     status_var->m_name_length))
  {
    return HA_ERR_RECORD_DELETED;
  }

  if (m_row.m_variable_value.make_row(status_var))
  {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int
table_session_status::read_row_values(TABLE *table,
                                      unsigned char *buf,
                                      Field **fields,
                                      bool read_all)
{
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /* VARIABLE_NAME */
        set_field_varchar_utf8(
          f, m_row.m_variable_name.m_str, m_row.m_variable_name.m_length);
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
