/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_variables_info.cc
  Table VARIABLES_INFO (implementation).
*/

#include "storage/perfschema/table_variables_info.h"

#include <stddef.h>

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

THR_LOCK table_variables_info::m_table_lock;

/* clang-format off */
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("VARIABLE_NAME") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("VARIABLE_SOURCE") },
    { C_STRING_WITH_LEN("enum('COMPILED','GLOBAL','SERVER','EXPLICIT','EXTRA','USER','LOGIN','COMMAND_LINE','PERSISTED','DYNAMIC')") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("VARIABLE_PATH") },
    { C_STRING_WITH_LEN("varchar(1024)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_VALUE") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_VALUE") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SET_TIME") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SET_USER") },
    { C_STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SET_HOST") },
    { C_STRING_WITH_LEN("char(60)") },
    { NULL, 0}
  }
};
/* clang-format on */

TABLE_FIELD_DEF
table_variables_info::m_field_def = {8, field_types};

PFS_engine_table_share table_variables_info::m_share = {
  {C_STRING_WITH_LEN("variables_info")},
  &pfs_readonly_world_acl,
  table_variables_info::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_variables_info::get_row_count,
  sizeof(pos_t),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  true   /* perpetual */
};

PFS_engine_table *
table_variables_info::create(void)
{
  return new table_variables_info();
}

ha_rows
table_variables_info::get_row_count(void)
{
  mysql_mutex_lock(&LOCK_plugin_delete);
  mysql_rwlock_rdlock(&LOCK_system_variables_hash);
  ha_rows system_var_count = get_system_variable_hash_records();
  mysql_rwlock_unlock(&LOCK_system_variables_hash);
  mysql_mutex_unlock(&LOCK_plugin_delete);
  return system_var_count;
}

table_variables_info::table_variables_info()
  : PFS_engine_table(&m_share, &m_pos),
    m_sysvarinfo_cache(false),
    m_pos(0),
    m_next_pos(0)
{
}

void
table_variables_info::reset_position(void)
{
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int
table_variables_info::rnd_init(bool)
{
  /* Build a cache of system variables for this thread. */
  m_sysvarinfo_cache.materialize_all(current_thd);
  return 0;
}

int
table_variables_info::rnd_next(void)
{
  for (m_pos.set_at(&m_next_pos); m_pos.m_index < m_sysvarinfo_cache.size();
       m_pos.next())
  {
    if (m_sysvarinfo_cache.is_materialized())
    {
      const System_variable *system_var = m_sysvarinfo_cache.get(m_pos.m_index);
      if (system_var != NULL)
      {
        m_next_pos.set_after(&m_pos);
        return make_row(system_var);
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}

int
table_variables_info::rnd_pos(const void *pos)
{
  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < m_sysvarinfo_cache.size());

  if (m_sysvarinfo_cache.is_materialized())
  {
    const System_variable *system_var = m_sysvarinfo_cache.get(m_pos.m_index);
    if (system_var != NULL)
    {
      return make_row(system_var);
    }
  }
  return HA_ERR_RECORD_DELETED;
}

int
table_variables_info::make_row(const System_variable *system_var)
{
  memcpy(m_row.m_variable_name, system_var->m_name, system_var->m_name_length);
  m_row.m_variable_name_length = system_var->m_name_length;

  m_row.m_variable_source = system_var->m_source;

  if (system_var->m_path_length)
    memcpy(
      m_row.m_variable_path, system_var->m_path_str, system_var->m_path_length);
  m_row.m_variable_path_length = system_var->m_path_length;

  memcpy(m_row.m_min_value,
         system_var->m_min_value_str,
         system_var->m_min_value_length);
  m_row.m_min_value_length = system_var->m_min_value_length;

  memcpy(m_row.m_max_value,
         system_var->m_max_value_str,
         system_var->m_max_value_length);
  m_row.m_max_value_length = system_var->m_max_value_length;

  m_row.m_set_time = system_var->m_set_time;

  memcpy(m_row.m_set_user_str,
         system_var->m_set_user_str,
         system_var->m_set_user_str_length);
  m_row.m_set_user_str_length = system_var->m_set_user_str_length;

  memcpy(m_row.m_set_host_str,
         system_var->m_set_host_str,
         system_var->m_set_host_str_length);
  m_row.m_set_host_str_length = system_var->m_set_host_str_length;

  return 0;
}

int
table_variables_info::read_row_values(TABLE *table,
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
          f, m_row.m_variable_name, m_row.m_variable_name_length);
        break;
      case 1: /* VARIABLE_SOURCE */
        set_field_enum(f, m_row.m_variable_source);
        break;
      case 2: /* VARIABLE_PATH */
        set_field_varchar_utf8(
          f, m_row.m_variable_path, m_row.m_variable_path_length);
        break;
      case 3: /* VARIABLE_MIN_VALUE */
        set_field_varchar_utf8(f, m_row.m_min_value, m_row.m_min_value_length);
        break;
      case 4: /* VARIABLE_MAX_VALUE */
        set_field_varchar_utf8(f, m_row.m_max_value, m_row.m_max_value_length);
        break;
      case 5: /* VARIABLE_SET_TIME */
        if (m_row.m_set_time != 0)
          set_field_timestamp(f, m_row.m_set_time);
        break;
      case 6: /* VARIABLE_SET_USER */
        set_field_char_utf8(
          f, m_row.m_set_user_str, m_row.m_set_user_str_length);
        break;
      case 7: /* VARIABLE_SET_HOST */
        set_field_char_utf8(
          f, m_row.m_set_host_str, m_row.m_set_host_str_length);
        break;

      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
