/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_ews_global_by_event_name.cc
  Table EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_ews_global_by_event_name.h"
#include "pfs_global.h"

THR_LOCK table_ews_global_by_event_name::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("EVENT_NAME") },
    { C_STRING_WITH_LEN("varchar(128)") },
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
table_ews_global_by_event_name::m_field_def=
{ 6, field_types };

PFS_engine_table_share
table_ews_global_by_event_name::m_share=
{
  { C_STRING_WITH_LEN("events_waits_summary_global_by_event_name") },
  &pfs_truncatable_acl,
  &table_ews_global_by_event_name::create,
  NULL, /* write_row */
  &table_ews_global_by_event_name::delete_all_rows,
  1000, /* records */
  sizeof(pos_all_instr_class),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_ews_global_by_event_name::create(void)
{
  return new table_ews_global_by_event_name();
}

int table_ews_global_by_event_name::delete_all_rows(void)
{
  reset_instrument_class_waits();
  return 0;
}

table_ews_global_by_event_name
::table_ews_global_by_event_name()
  : table_all_instr_class(&m_share)
{}

void table_ews_global_by_event_name
::make_instr_row(PFS_instr_class *klass)
{
  m_row.m_name= klass->m_name;
  m_row.m_name_length= klass->m_name_length;

  m_row.m_count= klass->m_wait_stat.m_count;
  m_row.m_sum= klass->m_wait_stat.m_sum;
  m_row.m_min= klass->m_wait_stat.m_min;
  m_row.m_max= klass->m_wait_stat.m_max;

  if (m_row.m_count)
    m_row.m_avg= m_row.m_sum / m_row.m_count;
  else
  {
    m_row.m_min= 0;
    m_row.m_avg= 0;
  }
}

int table_ews_global_by_event_name
::read_row_values(TABLE *table, unsigned char *, Field **fields,
                  bool read_all)
{
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 0);

  /*
    The row always exist,
    the instrument classes are static and never disappear.
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
      case 1: /* COUNT */
        set_field_ulonglong(f, m_row.m_count);
        break;
      case 2: /* SUM */
        set_field_ulonglong(f, m_row.m_sum);
        break;
      case 3: /* MIN */
        set_field_ulonglong(f, m_row.m_min);
        break;
      case 4: /* AVG */
        set_field_ulonglong(f, m_row.m_avg);
        break;
      case 5: /* MAX */
        set_field_ulonglong(f, m_row.m_max);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

