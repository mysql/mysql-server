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
  @file storage/perfschema/table_setup_timers.cc
  Table SETUP_TIMERS (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "table_setup_timers.h"
#include "pfs_column_values.h"
#include "pfs_timer.h"

#define COUNT_SETUP_TIMERS 1
static row_setup_timers all_setup_timers_data[COUNT_SETUP_TIMERS]=
{
  {
    { C_STRING_WITH_LEN("wait") },
    &wait_timer
  }
};

THR_LOCK table_setup_timers::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("NAME") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("TIMER_NAME") },
    { C_STRING_WITH_LEN("enum(\'CYCLE\',\'NANOSECOND\',\'MICROSECOND\',"
                        "\'MILLISECOND\',\'TICK\')") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_setup_timers::m_field_def=
{ 2, field_types };

PFS_engine_table_share
table_setup_timers::m_share=
{
  { C_STRING_WITH_LEN("setup_timers") },
  &pfs_updatable_acl,
  &table_setup_timers::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  COUNT_SETUP_TIMERS,
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_setup_timers::create(void)
{
  return new table_setup_timers();
}

table_setup_timers::table_setup_timers()
  : PFS_engine_table(&m_share, &m_pos),
    m_row(NULL), m_pos(0), m_next_pos(0)
{}

void table_setup_timers::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_setup_timers::rnd_next(void)
{
  int result;

  m_pos.set_at(&m_next_pos);

  if (m_pos.m_index < COUNT_SETUP_TIMERS)
  {
    m_row= &all_setup_timers_data[m_pos.m_index];
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

int table_setup_timers::rnd_pos(const void *pos)
{
  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < COUNT_SETUP_TIMERS);
  m_row= &all_setup_timers_data[m_pos.m_index];
  return 0;
}

int table_setup_timers::read_row_values(TABLE *table,
                                        unsigned char *,
                                        Field **fields,
                                        bool read_all)
{
  Field *f;

  DBUG_ASSERT(m_row);

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 0);

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* NAME */
        set_field_varchar_utf8(f, m_row->m_name.str, m_row->m_name.length);
        break;
      case 1: /* TIMER_NAME */
        set_field_enum(f, *(m_row->m_timer_name_ptr));
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

int table_setup_timers::update_row_values(TABLE *table,
                                          const unsigned char *,
                                          unsigned char *,
                                          Field **fields)
{
  Field *f;
  longlong value;

  DBUG_ASSERT(m_row);

  for (; (f= *fields) ; fields++)
  {
    if (bitmap_is_set(table->write_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* NAME */
        return HA_ERR_WRONG_COMMAND;
      case 1: /* TIMER_NAME */
        value= get_field_enum(f);
        if ((value >= FIRST_TIMER_NAME) && (value <= LAST_TIMER_NAME))
          *(m_row->m_timer_name_ptr)= (enum_timer_name) value;
        else
          return HA_ERR_WRONG_COMMAND;
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

