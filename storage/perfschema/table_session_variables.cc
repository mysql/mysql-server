/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/table_session_variables.cc
  Table SESSION_VARIABLES (implementation).
*/

#include "storage/perfschema/table_session_variables.h"

#include <stddef.h>
#include <new>

#include "current_thd.h"
#include "field.h"
#include "my_dbug.h"
#include "my_thread.h"
#include "mysqld.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"
#include "sql_class.h"

bool
PFS_index_session_variables::match(const System_variable *pfs)
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

THR_LOCK table_session_variables::m_table_lock;

/* clang-format off */
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("VARIABLE_NAME") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("VARIABLE_VALUE") },
    { C_STRING_WITH_LEN("varchar(1024)") },
    { NULL, 0}
  }
};
/* clang-format on */

TABLE_FIELD_DEF
table_session_variables::m_field_def = {2, field_types};

PFS_engine_table_share table_session_variables::m_share = {
  {C_STRING_WITH_LEN("session_variables")},
  &pfs_readonly_world_acl,
  table_session_variables::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_session_variables::get_row_count,
  sizeof(pos_t),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  true   /* perpetual */
};

PFS_engine_table *
table_session_variables::create(void)
{
  return new table_session_variables();
}

ha_rows
table_session_variables::get_row_count(void)
{
  mysql_mutex_lock(&LOCK_plugin_delete);
  mysql_rwlock_rdlock(&LOCK_system_variables_hash);
  ha_rows system_var_count = get_system_variable_hash_records();
  mysql_rwlock_unlock(&LOCK_system_variables_hash);
  mysql_mutex_unlock(&LOCK_plugin_delete);
  return system_var_count;
}

table_session_variables::table_session_variables()
  : PFS_engine_table(&m_share, &m_pos),
    m_sysvar_cache(false),
    m_pos(0),
    m_next_pos(0),
    m_context(NULL)
{
}

void
table_session_variables::reset_position(void)
{
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int
table_session_variables::rnd_init(bool scan)
{
  /* Build a cache of system variables for this thread. */
  m_sysvar_cache.materialize_all(current_thd);

  /* Record the version of the system variable hash, store in TLS. */
  ulonglong hash_version = m_sysvar_cache.get_sysvar_hash_version();
  m_context = (table_session_variables_context *)current_thd->alloc(
    sizeof(table_session_variables_context));
  new (m_context) table_session_variables_context(hash_version, !scan);
  return 0;
}

int
table_session_variables::rnd_next(void)
{
  if (m_context && !m_context->versions_match())
  {
    system_variable_warning();
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_sysvar_cache.size();
       m_pos.next())
  {
    if (m_sysvar_cache.is_materialized())
    {
      const System_variable *system_var = m_sysvar_cache.get(m_pos.m_index);
      if (system_var != NULL)
      {
        if (!make_row(system_var))
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
table_session_variables::rnd_pos(const void *pos)
{
  if (!m_context->versions_match())
  {
    system_variable_warning();
    return HA_ERR_RECORD_DELETED;
  }

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < m_sysvar_cache.size());

  if (m_sysvar_cache.is_materialized())
  {
    const System_variable *system_var = m_sysvar_cache.get(m_pos.m_index);
    if (system_var != NULL)
    {
      return make_row(system_var);
    }
  }
  return HA_ERR_RECORD_DELETED;
}

int
table_session_variables::index_init(uint idx, bool)
{
  /*
    Build a cache of system variables for this thread.
  */
  m_sysvar_cache.materialize_all(current_thd);

  /* Record the version of the system variable hash, store in TLS. */
  ulonglong hash_version = m_sysvar_cache.get_sysvar_hash_version();
  m_context = (table_session_variables_context *)current_thd->alloc(
    sizeof(table_session_variables_context));
  new (m_context) table_session_variables_context(hash_version, false);

  PFS_index_session_variables *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_session_variables);
  m_opened_index = result;
  m_index = result;

  return 0;
}

int
table_session_variables::index_next(void)
{
  if (m_context && !m_context->versions_match())
  {
    system_variable_warning();
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_sysvar_cache.size();
       m_pos.next())
  {
    if (m_sysvar_cache.is_materialized())
    {
      const System_variable *system_var = m_sysvar_cache.get(m_pos.m_index);
      if (system_var != NULL)
      {
        if (m_opened_index->match(system_var))
        {
          if (!make_row(system_var))
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
table_session_variables::make_row(const System_variable *system_var)
{
  if (m_row.m_variable_name.make_row(system_var->m_name,
                                     system_var->m_name_length))
  {
    return HA_ERR_RECORD_DELETED;
  }

  if (m_row.m_variable_value.make_row(system_var))
  {
    return HA_ERR_RECORD_DELETED;
  }

  return 0;
}

int
table_session_variables::read_row_values(TABLE *table,
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
