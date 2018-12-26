/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_global_variables.cc
  Table GLOBAL_VARIABLES (implementation).
*/

#include "my_global.h"
#include "table_global_variables.h"
#include "my_thread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "pfs_global.h"

THR_LOCK table_global_variables::m_table_lock;

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

TABLE_FIELD_DEF
table_global_variables::m_field_def=
{ 2, field_types };

PFS_engine_table_share
table_global_variables::m_share=
{
  { C_STRING_WITH_LEN("global_variables") },
  &pfs_readonly_world_acl,
  table_global_variables::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_global_variables::get_row_count,
  sizeof(pos_t),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  true   /* perpetual */
};

PFS_engine_table*
table_global_variables::create(void)
{
  return new table_global_variables();
}

ha_rows table_global_variables::get_row_count(void)
{
  mysql_mutex_lock(&LOCK_plugin_delete);
  mysql_rwlock_rdlock(&LOCK_system_variables_hash);
  ha_rows system_var_count= get_system_variable_hash_records();
  mysql_rwlock_unlock(&LOCK_system_variables_hash);
  mysql_mutex_unlock(&LOCK_plugin_delete);
  return system_var_count;
}

table_global_variables::table_global_variables()
  : PFS_engine_table(&m_share, &m_pos),
    m_sysvar_cache(false), m_row_exists(false), m_pos(0), m_next_pos(0)
{}

void table_global_variables::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_global_variables::rnd_init(bool scan)
{
  /*
    Build a list of system variables from the global system variable hash.
    Filter by scope.
  */
  m_sysvar_cache.materialize_global();

  /* Record the version of the system variable hash. */
  ulonglong hash_version= m_sysvar_cache.get_sysvar_hash_version();

  /*
    The table context holds the current version of the system variable hash.
    If scan == true, then allocate a new context from mem_root and store in TLS.
    If scan == false, then restore from TLS.
  */
  m_context= (table_global_variables_context *)current_thd->alloc(sizeof(table_global_variables_context));
  new(m_context) table_global_variables_context(hash_version, !scan);
  return 0;
}

int table_global_variables::rnd_next(void)
{
  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < m_sysvar_cache.size();
       m_pos.next())
  {
    const System_variable *system_var= m_sysvar_cache.get(m_pos.m_index);
    if (system_var != NULL)
    {
      make_row(system_var);
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }
  return HA_ERR_END_OF_FILE;
}

int table_global_variables::rnd_pos(const void *pos)
{
  /* If system variable hash changes, do nothing. */ // TODO: Issue warning
  if (!m_context->versions_match())
    return HA_ERR_RECORD_DELETED;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < m_sysvar_cache.size());

  const System_variable *system_var= m_sysvar_cache.get(m_pos.m_index);
  if (system_var != NULL)
  {
    make_row(system_var);
    return 0;
  }
  return HA_ERR_RECORD_DELETED;
}

void table_global_variables
::make_row(const System_variable *system_var)
{
  m_row_exists= false;
  if (system_var->is_null())
    return;
  m_row.m_variable_name.make_row(system_var->m_name, system_var->m_name_length);
  m_row.m_variable_value.make_row(system_var);
  m_row_exists= true;
}

int table_global_variables
::read_row_values(TABLE *table,
                  unsigned char *buf,
                  Field **fields,
                  bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* VARIABLE_NAME */
        set_field_varchar_utf8(f, m_row.m_variable_name.m_str, m_row.m_variable_name.m_length);
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

