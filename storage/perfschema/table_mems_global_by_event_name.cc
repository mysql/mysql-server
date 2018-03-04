/* Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_mems_global_by_event_name.cc
  Table MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME (implementation).
*/

#include "storage/perfschema/table_mems_global_by_event_name.h"

#include <stddef.h>

#include "my_dbug.h"
#include "my_thread.h"
#include "sql/field.h"
#include "storage/perfschema/pfs_builtin_memory.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_memory.h"
#include "storage/perfschema/pfs_visitor.h"

THR_LOCK table_mems_global_by_event_name::m_table_lock;

Plugin_table table_mems_global_by_event_name::m_table_def(
  /* Schema name */
  "performance_schema",
  /* Name */
  "memory_summary_global_by_event_name",
  /* Definition */
  "  EVENT_NAME VARCHAR(128) not null,\n"
  "  COUNT_ALLOC BIGINT unsigned not null,\n"
  "  COUNT_FREE BIGINT unsigned not null,\n"
  "  SUM_NUMBER_OF_BYTES_ALLOC BIGINT unsigned not null,\n"
  "  SUM_NUMBER_OF_BYTES_FREE BIGINT unsigned not null,\n"
  "  LOW_COUNT_USED BIGINT not null,\n"
  "  CURRENT_COUNT_USED BIGINT not null,\n"
  "  HIGH_COUNT_USED BIGINT not null,\n"
  "  LOW_NUMBER_OF_BYTES_USED BIGINT not null,\n"
  "  CURRENT_NUMBER_OF_BYTES_USED BIGINT not null,\n"
  "  HIGH_NUMBER_OF_BYTES_USED BIGINT not null,\n"
  "  PRIMARY KEY (EVENT_NAME)\n",
  /* Options */
  " ENGINE=PERFORMANCE_SCHEMA",
  /* Tablespace */
  nullptr);

PFS_engine_table_share table_mems_global_by_event_name::m_share = {
  &pfs_truncatable_acl,
  table_mems_global_by_event_name::create,
  NULL, /* write_row */
  table_mems_global_by_event_name::delete_all_rows,
  table_mems_global_by_event_name::get_row_count,
  sizeof(pos_t),
  &m_table_lock,
  &m_table_def,
  false, /* perpetual */
  PFS_engine_table_proxy(),
  {0},
  false /* m_in_purgatory */
};

bool
PFS_index_mems_global_by_event_name::match(PFS_instr_class *instr_class)
{
  if (m_fields >= 1)
  {
    if (!m_key.match(instr_class))
    {
      return false;
    }
  }
  return true;
}

PFS_engine_table *
table_mems_global_by_event_name::create(PFS_engine_table_share *)
{
  return new table_mems_global_by_event_name();
}

int
table_mems_global_by_event_name::delete_all_rows(void)
{
  reset_memory_by_thread();
  reset_memory_by_account();
  reset_memory_by_user();
  reset_memory_by_host();
  reset_memory_global();
  return 0;
}

ha_rows
table_mems_global_by_event_name::get_row_count(void)
{
  return memory_class_max;
}

table_mems_global_by_event_name::table_mems_global_by_event_name()
  : PFS_engine_table(&m_share, &m_pos), m_pos(), m_next_pos()
{
}

void
table_mems_global_by_event_name::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int
table_mems_global_by_event_name::rnd_next(void)
{
  PFS_memory_class *pfs;
  PFS_builtin_memory_class *pfs_builtin;

  /* Do not advertise hard coded instruments when disabled. */
  if (!pfs_initialized)
  {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.has_more_view(); m_pos.next_view())
  {
    switch (m_pos.m_index_1)
    {
    case pos_mems_global_by_event_name::VIEW_BUILTIN_MEMORY:
      pfs_builtin = find_builtin_memory_class(m_pos.m_index_2);
      if (pfs_builtin != NULL)
      {
        m_next_pos.set_after(&m_pos);
        return make_row(pfs_builtin);
      }
      break;
    case pos_mems_global_by_event_name::VIEW_MEMORY:
      pfs = find_memory_class(m_pos.m_index_2);
      if (pfs != NULL)
      {
        m_next_pos.set_after(&m_pos);
        return make_row(pfs);
      }
      break;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_mems_global_by_event_name::rnd_pos(const void *pos)
{
  PFS_builtin_memory_class *pfs_builtin;
  PFS_memory_class *pfs;

  /* Do not advertise hard coded instruments when disabled. */
  if (!pfs_initialized)
  {
    return HA_ERR_END_OF_FILE;
  }

  set_position(pos);

  switch (m_pos.m_index_1)
  {
  case pos_mems_global_by_event_name::VIEW_BUILTIN_MEMORY:
    pfs_builtin = find_builtin_memory_class(m_pos.m_index_2);
    if (pfs_builtin != NULL)
    {
      return make_row(pfs_builtin);
    }
    break;
  case pos_mems_global_by_event_name::VIEW_MEMORY:
    pfs = find_memory_class(m_pos.m_index_2);
    if (pfs != NULL)
    {
      return make_row(pfs);
    }
    break;
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_mems_global_by_event_name::index_init(uint idx MY_ATTRIBUTE((unused)),
                                            bool)
{
  PFS_index_mems_global_by_event_name *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_mems_global_by_event_name);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int
table_mems_global_by_event_name::index_next(void)
{
  PFS_memory_class *pfs;
  PFS_builtin_memory_class *pfs_builtin;

  /* Do not advertise hard coded instruments when disabled. */
  if (!pfs_initialized)
  {
    return HA_ERR_END_OF_FILE;
  }

  for (m_pos.set_at(&m_next_pos); m_pos.has_more_view(); m_pos.next_view())
  {
    switch (m_pos.m_index_1)
    {
    case pos_mems_global_by_event_name::VIEW_BUILTIN_MEMORY:
      do
      {
        pfs_builtin = find_builtin_memory_class(m_pos.m_index_2);
        if (pfs_builtin != NULL)
        {
          if (m_opened_index->match(&pfs_builtin->m_class))
          {
            if (!make_row(pfs_builtin))
            {
              m_next_pos.set_after(&m_pos);
              return 0;
            }
          }
          m_pos.m_index_2++;
        }
      } while (pfs_builtin != NULL);
      break;

    case pos_mems_global_by_event_name::VIEW_MEMORY:
      do
      {
        pfs = find_memory_class(m_pos.m_index_2);
        if (pfs != NULL)
        {
          if (m_opened_index->match(pfs))
          {
            if (!make_row(pfs))
            {
              m_next_pos.set_after(&m_pos);
              return 0;
            }
          }
          m_pos.m_index_2++;
        }
      } while (pfs != NULL);
      break;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_mems_global_by_event_name::make_row(PFS_memory_class *klass)
{
  m_row.m_event_name.make_row(klass);

  PFS_connection_memory_visitor visitor(klass);

  if (klass->is_global())
  {
    PFS_connection_iterator::visit_global(false, /* hosts */
                                          false, /* users */
                                          false, /* accounts */
                                          false, /* threads */
                                          false, /* THDs */
                                          &visitor);
  }
  else
  {
    PFS_connection_iterator::visit_global(true,  /* hosts */
                                          false, /* users */
                                          true,  /* accounts */
                                          true,  /* threads */
                                          false, /* THDs */
                                          &visitor);
  }

  m_row.m_stat.set(&visitor.m_stat);

  return 0;
}

int
table_mems_global_by_event_name::make_row(PFS_builtin_memory_class *klass)
{
  m_row.m_event_name.make_row(&klass->m_class);
  m_row.m_stat.set(&klass->m_stat);
  return 0;
}

int
table_mems_global_by_event_name::read_row_values(TABLE *table,
                                                 unsigned char *,
                                                 Field **fields,
                                                 bool read_all)
{
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 0);

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /* EVENT_NAME */
        m_row.m_event_name.set_field(f);
        break;
      default: /* 1, ... HIGH_NUMBER_OF_BYTES_USED */
        m_row.m_stat.set_field(f->field_index - 1, f);
        break;
      }
    }
  }

  return 0;
}
