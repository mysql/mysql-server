/* Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "my_thread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_uvar_by_thread.h"
#include "pfs_global.h"
#include "pfs_visitor.h"
#include "pfs_buffer_container.h"

/* Iteration on THD from the sql layer. */
#include "sql_class.h"
#include "mysqld_thd_manager.h"

class Find_thd_user_var : public Find_THD_Impl
{
public:
  Find_thd_user_var(THD *unsafe_thd)
    : m_unsafe_thd(unsafe_thd)
  {}

  virtual bool operator()(THD *thd)
  {
    if (thd != m_unsafe_thd)
      return false;

    if (thd->user_vars.records == 0)
      return false;

    mysql_mutex_lock(&thd->LOCK_thd_data);
    return true;
  }

private:
  THD *m_unsafe_thd;
};

void User_variables::materialize(PFS_thread *pfs, THD *thd)
{
  reset();

  m_pfs= pfs;
  m_thread_internal_id= pfs->m_thread_internal_id;
  m_array.reserve(thd->user_vars.records);

  user_var_entry *sql_uvar;

  uint index= 0;
  User_variable empty;

  /* Protects thd->user_vars. */
  mysql_mutex_assert_owner(&thd->LOCK_thd_data);

  for (;;)
  {
    sql_uvar= reinterpret_cast<user_var_entry*> (my_hash_element(& thd->user_vars, index));
    if (sql_uvar == NULL)
      break;

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
    User_variable & pfs_uvar= m_array.back();

    /* Copy VARIABLE_NAME */
    const char *name= sql_uvar->entry_name.ptr();
    size_t name_length= sql_uvar->entry_name.length();
    DBUG_ASSERT(name_length <= sizeof(pfs_uvar.m_name));
    pfs_uvar.m_name.make_row(name, name_length);

    /* Copy VARIABLE_VALUE */
    my_bool null_value;
    String *str_value;
    String str_buffer;
    uint decimals= 0;
    str_value= sql_uvar->val_str(& null_value, & str_buffer, decimals);
    if (str_value != NULL)
    {
      pfs_uvar.m_value.make_row(str_value->ptr(), str_value->length());
    }
    else
    {
      pfs_uvar.m_value.make_row(NULL, 0);
    }

    index++;
  }
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
    { C_STRING_WITH_LEN("longblob") },
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
  &pfs_readonly_acl,
  table_uvar_by_thread::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_uvar_by_thread::get_row_count,
  sizeof(pos_t),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

PFS_engine_table*
table_uvar_by_thread::create(void)
{
  return new table_uvar_by_thread();
}

ha_rows
table_uvar_by_thread::get_row_count(void)
{
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
  bool has_more_thread= true;

  for (m_pos.set_at(&m_next_pos);
       has_more_thread;
       m_pos.next_thread())
  {
    thread= global_thread_container.get(m_pos.m_index_1, & has_more_thread);
    if (thread != NULL)
    {
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
  }

  return HA_ERR_END_OF_FILE;
}

int
table_uvar_by_thread::rnd_pos(const void *pos)
{
  PFS_thread *thread;

  set_position(pos);

  thread= global_thread_container.get(m_pos.m_index_1);
  if (thread != NULL)
  {
    if (materialize(thread) == 0)
    {
      const User_variable *uvar= m_THD_cache.get(m_pos.m_index_2);
      if (uvar != NULL)
      {
        make_row(thread, uvar);
        return 0;
      }
    }
  }

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

  Find_thd_user_var finder(unsafe_thd);
  THD *safe_thd= Global_THD_manager::get_instance()->find_thd(&finder);
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

  /* uvar is materialized, pointing to it directly. */
  m_row.m_variable_name= & uvar->m_name;
  m_row.m_variable_value= & uvar->m_value;

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

  DBUG_ASSERT(m_row.m_variable_name != NULL);
  DBUG_ASSERT(m_row.m_variable_value != NULL);

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
        set_field_varchar_utf8(f,
                               m_row.m_variable_name->m_str,
                               m_row.m_variable_name->m_length);
        break;
      case 2: /* VARIABLE_VALUE */
        if (m_row.m_variable_value->get_value_length() > 0)
        {
          set_field_blob(f,
                         m_row.m_variable_value->get_value(),
                         m_row.m_variable_value->get_value_length());
        }
        else
        {
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

