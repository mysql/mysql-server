/* Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/**
  @file storage/perfschema/table_os_global_by_type.cc
  Table OBJECTS_SUMMARY_GLOBAL_BY_TYPE (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_os_global_by_type.h"
#include "pfs_global.h"

THR_LOCK table_os_global_by_type::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("OBJECT_TYPE") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OBJECT_SCHEMA") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OBJECT_NAME") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_STAR") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_WAIT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_WAIT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_WAIT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_WAIT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_os_global_by_type::m_field_def=
{ 8, field_types };

PFS_engine_table_share
table_os_global_by_type::m_share=
{
  { C_STRING_WITH_LEN("objects_summary_global_by_type") },
  &pfs_truncatable_acl,
  table_os_global_by_type::create,
  NULL, /* write_row */
  table_os_global_by_type::delete_all_rows,
  NULL, /* get_row_count */
  1000, /* records */
  sizeof(pos_os_global_by_type),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table*
table_os_global_by_type::create(void)
{
  return new table_os_global_by_type();
}

int
table_os_global_by_type::delete_all_rows(void)
{
  reset_table_waits_by_table_handle();
  reset_table_waits_by_table();
  return 0;
}

table_os_global_by_type::table_os_global_by_type()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(), m_next_pos()
{}

void table_os_global_by_type::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_os_global_by_type::rnd_next(void)
{
  PFS_table_share *table_share;

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_view();
       m_pos.next_view())
  {
    switch (m_pos.m_index_1) {
    case pos_os_global_by_type::VIEW_TABLE:
      for ( ; m_pos.m_index_2 < table_share_max; m_pos.m_index_2++)
      {
        table_share= &table_share_array[m_pos.m_index_2];
        if (table_share->m_lock.is_populated())
        {
          make_row(table_share);
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
      break;
    default:
      break;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_os_global_by_type::rnd_pos(const void *pos)
{
  PFS_table_share *table_share;

  set_position(pos);

  switch (m_pos.m_index_1) {
  case pos_os_global_by_type::VIEW_TABLE:
    DBUG_ASSERT(m_pos.m_index_2 < table_share_max);
    table_share= &table_share_array[m_pos.m_index_2];
    if (table_share->m_lock.is_populated())
    {
      make_row(table_share);
      return 0;
    }
    break;
  default:
    break;
  }

  return HA_ERR_RECORD_DELETED;
}

void table_os_global_by_type::make_row(PFS_table_share *share)
{
  pfs_lock lock;
  PFS_single_stat cumulated_stat;
  uint safe_key_count;

  m_row_exists= false;

  share->m_lock.begin_optimistic_lock(&lock);

  m_row.m_object_type= share->get_object_type();
  memcpy(m_row.m_schema_name, share->m_schema_name, share->m_schema_name_length);
  m_row.m_schema_name_length= share->m_schema_name_length;
  memcpy(m_row.m_object_name, share->m_table_name, share->m_table_name_length);
  m_row.m_object_name_length= share->m_table_name_length;

  /* This is a dirty read, some thread can write data while we are reading it */
  safe_key_count= sanitize_index_count(share->m_key_count);

  share->m_table_stat.sum(& cumulated_stat, safe_key_count);

  if (! share->m_lock.end_optimistic_lock(&lock))
    return;

  m_row_exists= true;

  if (share->get_refcount() > 0)
  {
    /* For all the table handles still opened ... */
    PFS_table *table= table_array;
    PFS_table *table_last= table_array + table_max;
    for ( ; table < table_last ; table++)
    {
      if ((table->m_share == share) && (table->m_lock.is_populated()))
      {
        /*
          If the opened table handle is for this table share,
          aggregate the table handle statistics.
        */
        table->m_table_stat.sum(& cumulated_stat, safe_key_count);
      }
    }
  }

  time_normalizer *normalizer= time_normalizer::get(wait_timer);
  m_row.m_stat.set(normalizer, &cumulated_stat);
}

int table_os_global_by_type::read_row_values(TABLE *table,
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
      case 0: /* OBJECT_TYPE */
        set_field_object_type(f, m_row.m_object_type);
        break;
      case 1: /* SCHEMA_NAME */
        set_field_varchar_utf8(f, m_row.m_schema_name,
                               m_row.m_schema_name_length);
        break;
      case 2: /* OBJECT_NAME */
        set_field_varchar_utf8(f, m_row.m_object_name,
                               m_row.m_object_name_length);
        break;
      case 3: /* COUNT */
        set_field_ulonglong(f, m_row.m_stat.m_count);
        break;
      case 4: /* SUM */
        set_field_ulonglong(f, m_row.m_stat.m_sum);
        break;
      case 5: /* MIN */
        set_field_ulonglong(f, m_row.m_stat.m_min);
        break;
      case 6: /* AVG */
        set_field_ulonglong(f, m_row.m_stat.m_avg);
        break;
      case 7: /* MAX */
        set_field_ulonglong(f, m_row.m_stat.m_max);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

