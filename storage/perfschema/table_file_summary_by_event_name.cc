/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_file_summary_by_event_name.cc
  Table FILE_SUMMARY_BY_EVENT_NAME(implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_file_summary_by_event_name.h"
#include "pfs_global.h"
#include "pfs_visitor.h"

THR_LOCK table_file_summary_by_event_name::m_table_lock;

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
  },

  /** Read */
  {
    { C_STRING_WITH_LEN("COUNT_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },

  /** Write */
  {
    { C_STRING_WITH_LEN("COUNT_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },

  /** Misc */
  {
    { C_STRING_WITH_LEN("COUNT_MISC") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_MISC") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_MISC") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_MISC") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_MISC") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_file_summary_by_event_name::m_field_def=
{ 23, field_types };

PFS_engine_table_share
table_file_summary_by_event_name::m_share=
{
  { C_STRING_WITH_LEN("file_summary_by_event_name") },
  &pfs_truncatable_acl,
  &table_file_summary_by_event_name::create,
  NULL, /* write_row */
  table_file_summary_by_event_name::delete_all_rows,
  NULL, /* get_row_count */
  1000, /* records */
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_file_summary_by_event_name::create(void)
{
  return new table_file_summary_by_event_name();
}

int table_file_summary_by_event_name::delete_all_rows(void)
{
  reset_file_instance_io();
  reset_file_class_io();
  return 0;
}

table_file_summary_by_event_name::table_file_summary_by_event_name()
  : PFS_engine_table(&m_share, &m_pos),
  m_pos(1), m_next_pos(1)
{}

void table_file_summary_by_event_name::reset_position(void)
{
  m_pos.m_index= 1;
  m_next_pos.m_index= 1;
}

int table_file_summary_by_event_name::rnd_next(void)
{
  PFS_file_class *file_class;

  m_pos.set_at(&m_next_pos);

  file_class= find_file_class(m_pos.m_index);
  if (file_class)
  {
    make_row(file_class);
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int table_file_summary_by_event_name::rnd_pos(const void *pos)
{
  PFS_file_class *file_class;

  set_position(pos);

  file_class= find_file_class(m_pos.m_index);
  if (file_class)
  {
    make_row(file_class);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

/**
  Build a row.
  @param file_class            the file class the cursor is reading
*/
void table_file_summary_by_event_name::make_row(PFS_file_class *file_class)
{
  m_row.m_event_name.make_row(file_class);

  PFS_instance_file_io_stat_visitor visitor;
  PFS_instance_iterator::visit_file_instances(file_class, &visitor);

  time_normalizer *normalizer= time_normalizer::get(wait_timer);
  
  /* Collect timer and byte count stats */
  m_row.m_io_stat.set(normalizer, &visitor.m_file_io_stat);
  m_row_exists= true;

}

int table_file_summary_by_event_name::read_row_values(TABLE *table,
                                                      unsigned char *,
                                                      Field **fields,
                                                      bool read_all)
{
  Field *f;

  if (unlikely(!m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 0);

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case  0: /* EVENT_NAME */
        m_row.m_event_name.set_field(f);
        break;
      case  1: /* COUNT_STAR */
        set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_count);
        break;
      case  2: /* SUM_TIMER_WAIT */
        set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_sum);
        break;
      case  3: /* MIN_TIMER_WAIT */
        set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_min);
        break;
      case  4: /* AVG_TIMER_WAIT */
        set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_avg);
        break;
      case  5: /* MAX_TIMER_WAIT */
        set_field_ulonglong(f, m_row.m_io_stat.m_all.m_waits.m_max);
        break;

      case  6: /* COUNT_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_count);
        break;
      case  7: /* SUM_TIMER_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_sum);
        break;
      case  8: /* MIN_TIMER_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_min);
        break;
      case  9: /* AVG_TIMER_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_avg);
        break;
      case 10: /* MAX_TIMER_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_read.m_waits.m_max);
        break;
      case 11: /* SUM_NUMBER_OF_BYTES_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_read.m_bytes);
        break;

      case 12: /* COUNT_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_count);
        break;
      case 13: /* SUM_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_sum);
        break;
      case 14: /* MIN_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_min);
        break;
      case 15: /* AVG_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_avg);
        break;
      case 16: /* MAX_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_write.m_waits.m_max);
        break;
      case 17: /* SUM_NUMBER_OF_BYTES_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_write.m_bytes);
        break;

      case 18: /* COUNT_MISC */
        set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_count);
        break;
      case 19: /* SUM_TIMER_MISC */
        set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_sum);
        break;
      case 20: /* MIN_TIMER_MISC */
        set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_min);
        break;
      case 21: /* AVG_TIMER_MISC */
        set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_avg);
        break;
      case 22: /* MAX_TIMER_MISC */
        set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_max);
        break;

      default:
        DBUG_ASSERT(false);
        break;
      }
    } // if
  } // for

  return 0;
}
