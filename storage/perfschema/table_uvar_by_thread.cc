/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_uvar_by_thread.cc
  Table USER_VARIABLES_BY_THREAD (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_uvar_by_thread.h"
#include "pfs_global.h"
#include "pfs_visitor.h"

/* Iteration on THD from the sql layer. */
#include "sql_class.h"
#include "mysqld_thd_manager.h"


class Find_thd_user_var : public Find_THD_Impl
{
public:
  virtual bool operator()(THD *thd)
  {
    if (thd->user_vars.records == 0)
      return false;

    mysql_mutex_lock(&thd->LOCK_thd_data);
    return true;
  }
};

void User_variables::materialize(PFS_thread *pfs, THD *thd)
{
  m_pfs= pfs;
  m_thread_internal_id= pfs->m_thread_internal_id;
  m_vector.clear();

  User_variable pfs_uvar;
  user_var_entry *sql_uvar;

  uint index= 0;

  do
  {
    sql_uvar= reinterpret_cast<user_var_entry*> (my_hash_element(& thd->user_vars, index));
    if (sql_uvar != NULL)
    {
      /* Copy VARIABLE_NAME */
      const char *name= sql_uvar->entry_name.ptr();
      size_t name_length= sql_uvar->entry_name.length();
      if (name_length > sizeof(pfs_uvar.m_name))
      {
        name_length= sizeof(pfs_uvar.m_name);
      }
      if (name_length > 0)
        memcpy(pfs_uvar.m_name, name, name_length);
      pfs_uvar.m_name_length= name_length;

      /* Copy VARIABLE_VALUE */
      String value(1024);
      my_bool null_value;
      sql_uvar->val_str(& null_value, & value, 0);
      if (null_value)
      {
        pfs_uvar.m_value_is_null= true;
      }
      else
      {
        pfs_uvar.m_value_is_null= false;
        const char *value_ptr= value.ptr();
        uint value_length= value.length();
        if (value_length > sizeof(pfs_uvar.m_value))
        {
          value_length= sizeof(pfs_uvar.m_value);
        }
        if (value_length > 0)
          memcpy(pfs_uvar.m_value, value_ptr, value_length);
        pfs_uvar.m_value_length= value_length;
      }

      m_vector.push_back(pfs_uvar);
    }
    index++;
  }
  while (sql_uvar != NULL);
}

THR_LOCK table_uvar_by_thread::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("THREAD_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
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
table_uvar_by_thread::m_field_def=
{ 3, field_types };

PFS_engine_table_share
table_uvar_by_thread::m_share=
{
  { C_STRING_WITH_LEN("user_variables_by_thread") },
  &pfs_truncatable_acl,
  table_uvar_by_thread::create,
  NULL, /* write_row */
  table_uvar_by_thread::delete_all_rows,
  NULL, /* get_row_count */
  1000, /* records */
  sizeof(pos_uvar_by_thread),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table*
table_uvar_by_thread::create(void)
{
  return new table_uvar_by_thread();
}

int
table_uvar_by_thread::delete_all_rows(void)
{
  reset_events_waits_by_thread();
  return 0;
}

table_uvar_by_thread::table_uvar_by_thread()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(), m_next_pos()
{}

void table_uvar_by_thread::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_uvar_by_thread::rnd_next(void)
{
  PFS_thread *thread;

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_thread();
       m_pos.next_thread())
  {
    thread= &thread_array[m_pos.m_index_1];

    if (materialize(thread) == 0)
    {
      const User_variable *uvar= m_THD_cache.get(m_pos.m_index_2);
      if (uvar != NULL)
      {
        make_row(thread, uvar);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_uvar_by_thread::rnd_pos(const void *pos)
{
  PFS_thread *thread;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index_1 < thread_max);

  thread= &thread_array[m_pos.m_index_1];
  if (! thread->m_lock.is_populated())
    return HA_ERR_RECORD_DELETED;

  return HA_ERR_RECORD_DELETED;
}

int table_uvar_by_thread::materialize(PFS_thread *thread)
{
  if (m_THD_cache.is_materialized(thread))
    return 0;

  if (! thread->m_lock.is_populated())
    return 1;

  THD *unsafe_thd= thread->m_thd;
  if (unsafe_thd == NULL)
    return 1;

  Find_thd_user_var finder;
  THD *safe_thd= Global_THD_manager::get_instance()->inspect_thd(unsafe_thd, &finder);
  if (safe_thd == NULL)
    return 1;

  m_THD_cache.materialize(thread, safe_thd);
  mysql_mutex_unlock(&safe_thd->LOCK_thd_data);
  return 0;
}

void table_uvar_by_thread
::make_row(PFS_thread *thread, const User_variable *uvar)
{
  pfs_optimistic_state lock;
  m_row_exists= false;

  /* Protect this reader against a thread termination */
  thread->m_lock.begin_optimistic_lock(&lock);

  m_row.m_thread_internal_id= thread->m_thread_internal_id;

  m_row.m_variable_name.make_row(uvar->m_name, uvar->m_name_length);
  m_row.m_variable_value.make_row(uvar->m_value, uvar->m_value_length);

  if (! thread->m_lock.end_optimistic_lock(&lock))
    return;

  m_row_exists= true;
}

int table_uvar_by_thread
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
      case 0: /* THREAD_ID */
        set_field_ulonglong(f, m_row.m_thread_internal_id);
        break;
      case 1: /* VARIABLE_NAME */
        set_field_varchar_utf8(f, m_row.m_variable_name.m_str, m_row.m_variable_name.m_length);
        break;
      case 2: /* VARIABLE_VALUE */
        set_field_varchar_utf8(f, m_row.m_variable_value.m_str, m_row.m_variable_value.m_length);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

