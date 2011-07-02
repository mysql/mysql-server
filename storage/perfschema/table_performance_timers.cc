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
  @file storage/perfschema/table_performance_timers.cc
  Table PERFORMANCE_TIMERS (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "table_performance_timers.h"
#include "pfs_timer.h"
#include "pfs_global.h"

THR_LOCK table_performance_timers::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("TIMER_NAME") },
    { C_STRING_WITH_LEN("enum(\'CYCLE\',\'NANOSECOND\',\'MICROSECOND\',"
                        "\'MILLISECOND\',\'TICK\')") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("TIMER_FREQUENCY") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("TIMER_RESOLUTION") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("TIMER_OVERHEAD") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_performance_timers::m_field_def=
{ 4, field_types };

PFS_engine_table_share
table_performance_timers::m_share=
{
  { C_STRING_WITH_LEN("performance_timers") },
  &pfs_readonly_acl,
  &table_performance_timers::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  COUNT_TIMER_NAME, /* records */
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_performance_timers::create(void)
{
  return new table_performance_timers();
}

table_performance_timers::table_performance_timers()
  : PFS_engine_table(&m_share, &m_pos),
    m_row(NULL), m_pos(0), m_next_pos(0)
{
  int index;

  index= (int)TIMER_NAME_CYCLE - FIRST_TIMER_NAME;
  m_data[index].m_timer_name= TIMER_NAME_CYCLE;
  m_data[index].m_info= pfs_timer_info.cycles;

  index= (int)TIMER_NAME_NANOSEC - FIRST_TIMER_NAME;
  m_data[index].m_timer_name= TIMER_NAME_NANOSEC;
  m_data[index].m_info= pfs_timer_info.nanoseconds;

  index= (int)TIMER_NAME_MICROSEC - FIRST_TIMER_NAME;
  m_data[index].m_timer_name= TIMER_NAME_MICROSEC;
  m_data[index].m_info= pfs_timer_info.microseconds;

  index= (int)TIMER_NAME_MILLISEC - FIRST_TIMER_NAME;
  m_data[index].m_timer_name= TIMER_NAME_MILLISEC;
  m_data[index].m_info= pfs_timer_info.milliseconds;

  index= (int)TIMER_NAME_TICK - FIRST_TIMER_NAME;
  m_data[index].m_timer_name= TIMER_NAME_TICK;
  m_data[index].m_info= pfs_timer_info.ticks;
}

void table_performance_timers::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_performance_timers::rnd_next(void)
{
  int result;

  m_pos.set_at(&m_next_pos);

  if (m_pos.m_index < COUNT_TIMER_NAME)
  {
    m_row= &m_data[m_pos.m_index];
    m_next_pos.set_after(&m_pos);
    result= 0;
  }
  else
  {
    m_row= NULL;
    result= HA_ERR_END_OF_FILE;
  }

  return result;
}

int table_performance_timers::rnd_pos(const void *pos)
{
  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < COUNT_TIMER_NAME);
  m_row= &m_data[m_pos.m_index];
  return 0;
}

int table_performance_timers::read_row_values(TABLE *table,
                                              unsigned char *buf,
                                              Field **fields,
                                              bool read_all)
{
  Field *f;

  DBUG_ASSERT(m_row);

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* TIMER_NAME */
        set_field_enum(f, m_row->m_timer_name);
        break;
      case 1: /* TIMER_FREQUENCY */
        if (m_row->m_info.routine != 0)
          set_field_ulonglong(f, m_row->m_info.frequency);
        else
          f->set_null();
        break;
      case 2: /* TIMER_RESOLUTION */
        if (m_row->m_info.routine != 0)
          set_field_ulonglong(f, m_row->m_info.resolution);
        else
          f->set_null();
        break;
      case 3: /* TIMER_OVERHEAD */
        if (m_row->m_info.routine != 0)
          set_field_ulonglong(f, m_row->m_info.overhead);
        else
          f->set_null();
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

