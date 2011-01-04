/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/table_setup_instruments.cc
  Table SETUP_INSTRUMENTS (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_setup_instruments.h"
#include "pfs_global.h"

THR_LOCK table_setup_instruments::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("NAME") },
    { C_STRING_WITH_LEN("varchar(128)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ENABLED") },
    { C_STRING_WITH_LEN("enum(\'YES\',\'NO\')") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("TIMED") },
    { C_STRING_WITH_LEN("enum(\'YES\',\'NO\')") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_setup_instruments::m_field_def=
{ 3, field_types };

PFS_engine_table_share
table_setup_instruments::m_share=
{
  { C_STRING_WITH_LEN("setup_instruments") },
  &pfs_updatable_acl,
  &table_setup_instruments::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  1000, /* records */
  sizeof(pos_setup_instruments),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_setup_instruments::create(void)
{
  return new table_setup_instruments();
}

table_setup_instruments::table_setup_instruments()
  : PFS_engine_table(&m_share, &m_pos),
    m_pos(), m_next_pos()
{}

void table_setup_instruments::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_setup_instruments::rnd_next(void)
{
  PFS_mutex_class *mutex_class;
  PFS_rwlock_class *rwlock_class;
  PFS_cond_class *cond_class;
  PFS_file_class *file_class;

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_view();
       m_pos.next_view())
  {
    switch (m_pos.m_index_1) {
    case pos_setup_instruments::VIEW_MUTEX:
      mutex_class= find_mutex_class(m_pos.m_index_2);
      if (mutex_class)
      {
        make_row(mutex_class);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
      break;
    case pos_setup_instruments::VIEW_RWLOCK:
      rwlock_class= find_rwlock_class(m_pos.m_index_2);
      if (rwlock_class)
      {
        make_row(rwlock_class);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
      break;
    case pos_setup_instruments::VIEW_COND:
      cond_class= find_cond_class(m_pos.m_index_2);
      if (cond_class)
      {
        make_row(cond_class);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
      break;
    case pos_setup_instruments::VIEW_THREAD:
      /* Reserved for WL#4674, PERFORMANCE_SCHEMA Setup For Actors. */
      break;
    case pos_setup_instruments::VIEW_FILE:
      file_class= find_file_class(m_pos.m_index_2);
      if (file_class)
      {
        make_row(file_class);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
      break;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_setup_instruments::rnd_pos(const void *pos)
{
  PFS_mutex_class *mutex_class;
  PFS_rwlock_class *rwlock_class;
  PFS_cond_class *cond_class;
  PFS_file_class *file_class;

  set_position(pos);

  switch (m_pos.m_index_1) {
  case pos_setup_instruments::VIEW_MUTEX:
    mutex_class= find_mutex_class(m_pos.m_index_2);
    if (mutex_class)
    {
      make_row(mutex_class);
      return 0;
    }
    break;
  case pos_setup_instruments::VIEW_RWLOCK:
    rwlock_class= find_rwlock_class(m_pos.m_index_2);
    if (rwlock_class)
    {
      make_row(rwlock_class);
      return 0;
    }
    break;
  case pos_setup_instruments::VIEW_COND:
    cond_class= find_cond_class(m_pos.m_index_2);
    if (cond_class)
    {
      make_row(cond_class);
      return 0;
    }
    break;
  case pos_setup_instruments::VIEW_THREAD:
    /* Reserved for WL#4674, PERFORMANCE_SCHEMA Setup For Actors. */
    break;
  case pos_setup_instruments::VIEW_FILE:
    file_class= find_file_class(m_pos.m_index_2);
    if (file_class)
    {
      make_row(file_class);
      return 0;
    }
    break;
  }

  return HA_ERR_RECORD_DELETED;
}

void table_setup_instruments::make_row(PFS_instr_class *klass)
{
  m_row.m_name= &klass->m_name[0];
  m_row.m_name_length= klass->m_name_length;
  m_row.m_enabled_ptr= &klass->m_enabled;
  m_row.m_timed_ptr= &klass->m_timed;
}

int table_setup_instruments::read_row_values(TABLE *table,
                                             unsigned char *,
                                             Field **fields,
                                             bool read_all)
{
  Field *f;

  DBUG_ASSERT(table->s->null_bytes == 0);

  /*
    The row always exist, the instrument classes
    are static and never disappear.
  */

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* NAME */
        set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
        break;
      case 1: /* ENABLED */
        set_field_enum(f, (*m_row.m_enabled_ptr) ? ENUM_YES : ENUM_NO);
        break;
      case 2: /* TIMED */
        if (m_row.m_timed_ptr)
          set_field_enum(f, (*m_row.m_timed_ptr) ? ENUM_YES : ENUM_NO);
        else
          set_field_enum(f, ENUM_NO);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

int table_setup_instruments::update_row_values(TABLE *table,
                                               const unsigned char *,
                                               unsigned char *,
                                               Field **fields)
{
  Field *f;
  enum_yes_no value;

  for (; (f= *fields) ; fields++)
  {
    if (bitmap_is_set(table->write_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* NAME */
        return HA_ERR_WRONG_COMMAND;
      case 1: /* ENABLED */
        value= (enum_yes_no) get_field_enum(f);
        *m_row.m_enabled_ptr= (value == ENUM_YES) ? true : false;
        break;
      case 2: /* TIMED */
        if (m_row.m_timed_ptr)
        {
          value= (enum_yes_no) get_field_enum(f);
          *m_row.m_timed_ptr= (value == ENUM_YES) ? true : false;
        }
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

