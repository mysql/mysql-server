/* Copyright (c) 2008, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "my_thread.h"
#include "pfs_instr_class.h"
#include "pfs_builtin_memory.h"
#include "pfs_instr.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_setup_instruments.h"
#include "pfs_global.h"
#include "pfs_setup_object.h"
#include "field.h"

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
  table_setup_instruments::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_setup_instruments::get_row_count,
  sizeof(pos_setup_instruments),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

PFS_engine_table* table_setup_instruments::create(void)
{
  return new table_setup_instruments();
}

ha_rows
table_setup_instruments::get_row_count(void)
{
  return wait_class_max
    + stage_class_max
    + statement_class_max
    + transaction_class_max
    + memory_class_max;
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
  PFS_instr_class *instr_class= NULL;
  PFS_builtin_memory_class *pfs_builtin;
  bool update_enabled;
  bool update_timed;

  /* Do not advertise hard coded instruments when disabled. */
  if (! pfs_initialized)
    return HA_ERR_END_OF_FILE;

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_view();
       m_pos.next_view())
  {
    update_enabled= true;
    update_timed= true;

    switch (m_pos.m_index_1)
    {
    case pos_setup_instruments::VIEW_MUTEX:
      instr_class= find_mutex_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_RWLOCK:
      instr_class= find_rwlock_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_COND:
      instr_class= find_cond_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_THREAD:
      /* Not used yet  */
      break;
    case pos_setup_instruments::VIEW_FILE:
      instr_class= find_file_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_TABLE:
      instr_class= find_table_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_STAGE:
      instr_class= find_stage_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_STATEMENT:
      instr_class= find_statement_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_TRANSACTION:
      instr_class= find_transaction_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_SOCKET:
      instr_class= find_socket_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_IDLE:
      instr_class= find_idle_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_BUILTIN_MEMORY:
      update_enabled= false;
      update_timed= false;
      pfs_builtin= find_builtin_memory_class(m_pos.m_index_2);
      if (pfs_builtin != NULL)
        instr_class= & pfs_builtin->m_class;
      else
        instr_class= NULL;
      break;
    case pos_setup_instruments::VIEW_MEMORY:
      update_timed= false;
      instr_class= find_memory_class(m_pos.m_index_2);
      break;
    case pos_setup_instruments::VIEW_METADATA:
      instr_class= find_metadata_class(m_pos.m_index_2);
      break;
    }
    if (instr_class)
    {
      make_row(instr_class, update_enabled, update_timed);
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_setup_instruments::rnd_pos(const void *pos)
{
  PFS_instr_class *instr_class= NULL;
  PFS_builtin_memory_class *pfs_builtin;
  bool update_enabled;
  bool update_timed;

  /* Do not advertise hard coded instruments when disabled. */
  if (! pfs_initialized)
    return HA_ERR_END_OF_FILE;

  set_position(pos);

  update_enabled= true;
  update_timed= true;

  switch (m_pos.m_index_1)
  {
  case pos_setup_instruments::VIEW_MUTEX:
    instr_class= find_mutex_class(m_pos.m_index_2);
    break;
  case pos_setup_instruments::VIEW_RWLOCK:
    instr_class= find_rwlock_class(m_pos.m_index_2);
    break;
  case pos_setup_instruments::VIEW_COND:
    instr_class= find_cond_class(m_pos.m_index_2);
    break;
  case pos_setup_instruments::VIEW_THREAD:
    /* Not used yet */
    break;
  case pos_setup_instruments::VIEW_FILE:
    instr_class= find_file_class(m_pos.m_index_2);
    break;
  case pos_setup_instruments::VIEW_TABLE:
    instr_class= find_table_class(m_pos.m_index_2);
    break;
  case pos_setup_instruments::VIEW_STAGE:
    instr_class= find_stage_class(m_pos.m_index_2);
    break;
  case pos_setup_instruments::VIEW_STATEMENT:
    instr_class= find_statement_class(m_pos.m_index_2);
    break;
  case pos_setup_instruments::VIEW_TRANSACTION:
    instr_class= find_transaction_class(m_pos.m_index_2);
    break;
  case pos_setup_instruments::VIEW_SOCKET:
    instr_class= find_socket_class(m_pos.m_index_2);
    break;
  case pos_setup_instruments::VIEW_IDLE:
    instr_class= find_idle_class(m_pos.m_index_2);
    break;
  case pos_setup_instruments::VIEW_BUILTIN_MEMORY:
    update_enabled= false;
    update_timed= false;
    pfs_builtin= find_builtin_memory_class(m_pos.m_index_2);
    if (pfs_builtin != NULL)
      instr_class= & pfs_builtin->m_class;
    else
      instr_class= NULL;
    break;
  case pos_setup_instruments::VIEW_MEMORY:
    update_timed= false;
    instr_class= find_memory_class(m_pos.m_index_2);
    break;
  case pos_setup_instruments::VIEW_METADATA:
    instr_class= find_metadata_class(m_pos.m_index_2);
    break;
  }
  if (instr_class)
  {
    make_row(instr_class, update_enabled, update_timed);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

void table_setup_instruments::make_row(PFS_instr_class *klass, bool update_enabled, bool update_timed)
{
  m_row.m_instr_class= klass;
  m_row.m_update_enabled= update_enabled;
  m_row.m_update_timed= update_timed;
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
        set_field_varchar_utf8(f, m_row.m_instr_class->m_name, m_row.m_instr_class->m_name_length);
        break;
      case 1: /* ENABLED */
        set_field_enum(f, m_row.m_instr_class->m_enabled ? ENUM_YES : ENUM_NO);
        break;
      case 2: /* TIMED */
        set_field_enum(f, m_row.m_instr_class->m_timed ? ENUM_YES : ENUM_NO);
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
        /* Do not raise error if m_update_enabled is false, silently ignore. */
        if (m_row.m_update_enabled)
        {
          value= (enum_yes_no) get_field_enum(f);
          m_row.m_instr_class->m_enabled= (value == ENUM_YES) ? true : false;
        }
        break;
      case 2: /* TIMED */
        /* Do not raise error if m_update_timed is false, silently ignore. */
        if (m_row.m_update_timed)
        {
          value= (enum_yes_no) get_field_enum(f);
          m_row.m_instr_class->m_timed= (value == ENUM_YES) ? true : false;
        }
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  switch (m_pos.m_index_1)
  {
    case pos_setup_instruments::VIEW_MUTEX:
      update_mutex_derived_flags();
      break;
    case pos_setup_instruments::VIEW_RWLOCK:
      update_rwlock_derived_flags();
      break;
    case pos_setup_instruments::VIEW_COND:
      update_cond_derived_flags();
      break;
    case pos_setup_instruments::VIEW_THREAD:
      /* Not used yet  */
      break;
    case pos_setup_instruments::VIEW_FILE:
      update_file_derived_flags();
      break;
    case pos_setup_instruments::VIEW_TABLE:
      update_table_derived_flags();
      break;
    case pos_setup_instruments::VIEW_STAGE:
    case pos_setup_instruments::VIEW_STATEMENT:
    case pos_setup_instruments::VIEW_TRANSACTION:
      /* No flag to update. */
      break;
    case pos_setup_instruments::VIEW_SOCKET:
      update_socket_derived_flags();
      break;
    case pos_setup_instruments::VIEW_IDLE:
      /* No flag to update. */
      break;
    case pos_setup_instruments::VIEW_BUILTIN_MEMORY:
    case pos_setup_instruments::VIEW_MEMORY:
      /* No flag to update. */
      break;
    case pos_setup_instruments::VIEW_METADATA:
      update_metadata_derived_flags();
      break;
    default:
      DBUG_ASSERT(false);
      break;
  }

  return 0;
}

