/* Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_mems_global_by_event_name.cc
  Table MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME (implementation).
*/

#include "my_global.h"
#include "my_thread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_mems_global_by_event_name.h"
#include "pfs_global.h"
#include "pfs_visitor.h"
#include "pfs_memory.h"

THR_LOCK table_mems_global_by_event_name::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("EVENT_NAME") },
    { C_STRING_WITH_LEN("varchar(128)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_ALLOC") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_FREE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_ALLOC") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_FREE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("LOW_COUNT_USED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("CURRENT_COUNT_USED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("HIGH_COUNT_USED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("LOW_NUMBER_OF_BYTES_USED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("CURRENT_NUMBER_OF_BYTES_USED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("HIGH_NUMBER_OF_BYTES_USED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_mems_global_by_event_name::m_field_def=
{ 11, field_types };

PFS_engine_table_share
table_mems_global_by_event_name::m_share=
{
  { C_STRING_WITH_LEN("memory_summary_global_by_event_name") },
  &pfs_readonly_acl,
  table_mems_global_by_event_name::create,
  NULL, /* write_row */
  table_mems_global_by_event_name::delete_all_rows,
  table_mems_global_by_event_name::get_row_count,
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_mems_global_by_event_name::create(void)
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
  : PFS_engine_table(&m_share, &m_pos),
  m_row_exists(false), m_pos(1), m_next_pos(1)
{}

void table_mems_global_by_event_name::reset_position(void)
{
  m_pos.m_index= 1;
  m_next_pos.m_index= 1;
}

int table_mems_global_by_event_name::rnd_next(void)
{
  PFS_memory_class *pfs;

  m_pos.set_at(&m_next_pos);

  pfs= find_memory_class(m_pos.m_index);
  if (pfs)
  {
    make_row(pfs);
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int table_mems_global_by_event_name::rnd_pos(const void *pos)
{
  PFS_memory_class *pfs;

  set_position(pos);

  pfs= find_memory_class(m_pos.m_index);
  if (pfs)
  {
    make_row(pfs);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

void table_mems_global_by_event_name::make_row(PFS_memory_class *klass)
{
  m_row.m_event_name.make_row(klass);

  PFS_connection_memory_visitor visitor(klass);
  PFS_connection_iterator::visit_global(true /* hosts */,
                                        false /* users */,
                                        true /* accounts */,
                                        true /* threads */,
                                        &visitor);

  m_row.m_stat.set(& visitor.m_stat);
  m_row_exists= true;
}

int table_mems_global_by_event_name::read_row_values(TABLE *table,
                                                    unsigned char *,
                                                    Field **fields,
                                                    bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 0);

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
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

