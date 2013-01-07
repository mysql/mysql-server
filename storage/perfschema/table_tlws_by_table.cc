/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
  @file storage/perfschema/table_tlws_by_table.cc
  Table TABLE_LOCK_WAITS_SUMMARY_BY_TABLE (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_tlws_by_table.h"
#include "pfs_global.h"
#include "pfs_visitor.h"

THR_LOCK table_tlws_by_table::m_table_lock;

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
  },
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
    { C_STRING_WITH_LEN("COUNT_READ_NORMAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_READ_NORMAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_READ_NORMAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_READ_NORMAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_READ_NORMAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_READ_WITH_SHARED_LOCKS") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_READ_WITH_SHARED_LOCKS") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_READ_WITH_SHARED_LOCKS") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_READ_WITH_SHARED_LOCKS") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_READ_WITH_SHARED_LOCKS") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_READ_HIGH_PRIORITY") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_READ_HIGH_PRIORITY") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_READ_HIGH_PRIORITY") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_READ_HIGH_PRIORITY") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_READ_HIGH_PRIORITY") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_READ_NO_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_READ_NO_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_READ_NO_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_READ_NO_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_READ_NO_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_READ_EXTERNAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_READ_EXTERNAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_READ_EXTERNAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_READ_EXTERNAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_READ_EXTERNAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_WRITE_ALLOW_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_WRITE_ALLOW_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_WRITE_ALLOW_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_WRITE_ALLOW_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_WRITE_ALLOW_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_WRITE_CONCURRENT_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_WRITE_CONCURRENT_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_WRITE_CONCURRENT_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_WRITE_CONCURRENT_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_WRITE_CONCURRENT_INSERT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_WRITE_LOW_PRIORITY") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_WRITE_LOW_PRIORITY") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_WRITE_LOW_PRIORITY") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_WRITE_LOW_PRIORITY") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_WRITE_LOW_PRIORITY") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_WRITE_NORMAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_WRITE_NORMAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_WRITE_NORMAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_WRITE_NORMAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_WRITE_NORMAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_WRITE_EXTERNAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_WRITE_EXTERNAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_WRITE_EXTERNAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_WRITE_EXTERNAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_WRITE_EXTERNAL") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};

TABLE_FIELD_DEF
table_tlws_by_table::m_field_def=
{ sizeof(field_types) / sizeof(TABLE_FIELD_TYPE), field_types };

PFS_engine_table_share
table_tlws_by_table::m_share=
{
  { C_STRING_WITH_LEN("table_lock_waits_summary_by_table") },
  &pfs_truncatable_acl,
  table_tlws_by_table::create,
  NULL, /* write_row */
  table_tlws_by_table::delete_all_rows,
  NULL, /* get_row_count */
  1000, /* records */
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table*
table_tlws_by_table::create(void)
{
  return new table_tlws_by_table();
}

int
table_tlws_by_table::delete_all_rows(void)
{
  reset_table_lock_waits_by_table_handle();
  reset_table_lock_waits_by_table();
  return 0;
}

table_tlws_by_table::table_tlws_by_table()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(0), m_next_pos(0)
{}

void table_tlws_by_table::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_tlws_by_table::rnd_init(bool scan)
{
  m_normalizer= time_normalizer::get(wait_timer);
  return 0;
}

int table_tlws_by_table::rnd_next(void)
{
  PFS_table_share *table_share;

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < table_share_max;
       m_pos.m_index++)
  {
    table_share= &table_share_array[m_pos.m_index];
    if (table_share->m_lock.is_populated())
    {
      make_row(table_share);
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_tlws_by_table::rnd_pos(const void *pos)
{
  PFS_table_share *table_share;

  set_position(pos);

  table_share= &table_share_array[m_pos.m_index];
  if (table_share->m_lock.is_populated())
  {
    make_row(table_share);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

void table_tlws_by_table::make_row(PFS_table_share *share)
{
  pfs_lock lock;

  m_row_exists= false;

  share->m_lock.begin_optimistic_lock(&lock);

  if (m_row.m_object.make_row(share))
    return;

  PFS_table_lock_stat_visitor visitor;
  PFS_object_iterator::visit_tables(share, & visitor);

  if (! share->m_lock.end_optimistic_lock(&lock))
    return;

  m_row_exists= true;
  m_row.m_stat.set(m_normalizer, &visitor.m_stat);
}

int table_tlws_by_table::read_row_values(TABLE *table,
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
      case 1: /* SCHEMA_NAME */
      case 2: /* OBJECT_NAME */
        m_row.m_object.set_field(f->field_index, f);
        break;
      case 3: /* COUNT_STAR */
        set_field_ulonglong(f, m_row.m_stat.m_all.m_count);
        break;
      case 4: /* SUM_TIMER */
        set_field_ulonglong(f, m_row.m_stat.m_all.m_sum);
        break;
      case 5: /* MIN_TIMER */
        set_field_ulonglong(f, m_row.m_stat.m_all.m_min);
        break;
      case 6: /* AVG_TIMER */
        set_field_ulonglong(f, m_row.m_stat.m_all.m_avg);
        break;
      case 7: /* MAX_TIMER */
        set_field_ulonglong(f, m_row.m_stat.m_all.m_max);
        break;
      case 8: /* COUNT_READ */
        set_field_ulonglong(f, m_row.m_stat.m_all_read.m_count);
        break;
      case 9: /* SUM_TIMER_READ */
        set_field_ulonglong(f, m_row.m_stat.m_all_read.m_sum);
        break;
      case 10: /* MIN_TIMER_READ */
        set_field_ulonglong(f, m_row.m_stat.m_all_read.m_min);
        break;
      case 11: /* AVG_TIMER_READ */
        set_field_ulonglong(f, m_row.m_stat.m_all_read.m_avg);
        break;
      case 12: /* MAX_TIMER_READ */
        set_field_ulonglong(f, m_row.m_stat.m_all_read.m_max);
        break;
      case 13: /* COUNT_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_all_write.m_count);
        break;
      case 14: /* SUM_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_all_write.m_sum);
        break;
      case 15: /* MIN_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_all_write.m_min);
        break;
      case 16: /* AVG_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_all_write.m_avg);
        break;
      case 17: /* MAX_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_all_write.m_max);
        break;

      case 18: /* COUNT_READ_NORMAL */
        set_field_ulonglong(f, m_row.m_stat.m_read_normal.m_count);
        break;
      case 19: /* SUM_TIMER_READ_NORMAL */
        set_field_ulonglong(f, m_row.m_stat.m_read_normal.m_sum);
        break;
      case 20: /* MIN_TIMER_READ_NORMAL */
        set_field_ulonglong(f, m_row.m_stat.m_read_normal.m_min);
        break;
      case 21: /* AVG_TIMER_READ_NORMAL */
        set_field_ulonglong(f, m_row.m_stat.m_read_normal.m_avg);
        break;
      case 22: /* MAX_TIMER_READ_NORMAL */
        set_field_ulonglong(f, m_row.m_stat.m_read_normal.m_max);
        break;

      case 23: /* COUNT_READ_WITH_SHARED_LOCKS */
        set_field_ulonglong(f, m_row.m_stat.m_read_with_shared_locks.m_count);
        break;
      case 24: /* SUM_TIMER_READ_WITH_SHARED_LOCKS */
        set_field_ulonglong(f, m_row.m_stat.m_read_with_shared_locks.m_sum);
        break;
      case 25: /* MIN_TIMER_READ_WITH_SHARED_LOCKS */
        set_field_ulonglong(f, m_row.m_stat.m_read_with_shared_locks.m_min);
        break;
      case 26: /* AVG_TIMER_READ_WITH_SHARED_LOCKS */
        set_field_ulonglong(f, m_row.m_stat.m_read_with_shared_locks.m_avg);
        break;
      case 27: /* MAX_TIMER_READ_WITH_SHARED_LOCKS */
        set_field_ulonglong(f, m_row.m_stat.m_read_with_shared_locks.m_max);
        break;

      case 28: /* COUNT_READ_HIGH_PRIORITY */
        set_field_ulonglong(f, m_row.m_stat.m_read_high_priority.m_count);
        break;
      case 29: /* SUM_TIMER_READ_HIGH_PRIORITY */
        set_field_ulonglong(f, m_row.m_stat.m_read_high_priority.m_sum);
        break;
      case 30: /* MIN_TIMER_READ_HIGH_PRIORITY */
        set_field_ulonglong(f, m_row.m_stat.m_read_high_priority.m_min);
        break;
      case 31: /* AVG_TIMER_READ_HIGH_PRIORITY */
        set_field_ulonglong(f, m_row.m_stat.m_read_high_priority.m_avg);
        break;
      case 32: /* MAX_TIMER_READ_HIGH_PRIORITY */
        set_field_ulonglong(f, m_row.m_stat.m_read_high_priority.m_max);
        break;

      case 33: /* COUNT_READ_NO_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_read_no_insert.m_count);
        break;
      case 34: /* SUM_TIMER_READ_NO_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_read_no_insert.m_sum);
        break;
      case 35: /* MIN_TIMER_READ_NO_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_read_no_insert.m_min);
        break;
      case 36: /* AVG_TIMER_READ_NO_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_read_no_insert.m_avg);
        break;
      case 37: /* MAX_TIMER_READ_NO_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_read_no_insert.m_max);
        break;

      case 38: /* COUNT_READ_EXTERNAL */
        set_field_ulonglong(f, m_row.m_stat.m_read_external.m_count);
        break;
      case 39: /* SUM_TIMER_READ_EXTERNAL */
        set_field_ulonglong(f, m_row.m_stat.m_read_external.m_sum);
        break;
      case 40: /* MIN_TIMER_READ_EXTERNAL */
        set_field_ulonglong(f, m_row.m_stat.m_read_external.m_min);
        break;
      case 41: /* AVG_TIMER_READ_EXTERNAL */
        set_field_ulonglong(f, m_row.m_stat.m_read_external.m_avg);
        break;
      case 42: /* MAX_TIMER_READ_EXTERNAL */
        set_field_ulonglong(f, m_row.m_stat.m_read_external.m_max);
        break;

      case 43: /* COUNT_WRITE_ALLOW_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_write_allow_write.m_count);
        break;
      case 44: /* SUM_TIMER_WRITE_ALLOW_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_write_allow_write.m_sum);
        break;
      case 45: /* MIN_TIMER_WRITE_ALLOW_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_write_allow_write.m_min);
        break;
      case 46: /* AVG_TIMER_WRITE_ALLOW_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_write_allow_write.m_avg);
        break;
      case 47: /* MAX_TIMER_WRITE_ALLOW_WRITE */
        set_field_ulonglong(f, m_row.m_stat.m_write_allow_write.m_max);
        break;

      case 48: /* COUNT_WRITE_CONCURRENT_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_write_concurrent_insert.m_count);
        break;
      case 49: /* SUM_TIMER_WRITE_CONCURRENT_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_write_concurrent_insert.m_sum);
        break;
      case 50: /* MIN_TIMER_WRITE_CONCURRENT_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_write_concurrent_insert.m_min);
        break;
      case 51: /* AVG_TIMER_WRITE_CONCURRENT_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_write_concurrent_insert.m_avg);
        break;
      case 52: /* MAX_TIMER_WRITE_CONCURRENT_INSERT */
        set_field_ulonglong(f, m_row.m_stat.m_write_concurrent_insert.m_max);
        break;

      case 53: /* COUNT_WRITE_LOW_PRIORITY */
        set_field_ulonglong(f, m_row.m_stat.m_write_low_priority.m_count);
        break;
      case 54: /* SUM_TIMER_WRITE_LOW_PRIORITY */
        set_field_ulonglong(f, m_row.m_stat.m_write_low_priority.m_sum);
        break;
      case 55: /* MIN_TIMER_WRITE_LOW_PRIORITY */
        set_field_ulonglong(f, m_row.m_stat.m_write_low_priority.m_min);
        break;
      case 56: /* AVG_TIMER_WRITE_LOW_PRIORITY */
        set_field_ulonglong(f, m_row.m_stat.m_write_low_priority.m_avg);
        break;
      case 57: /* MAX_TIMER_WRITE_LOW_PRIORITY */
        set_field_ulonglong(f, m_row.m_stat.m_write_low_priority.m_max);
        break;

      case 58: /* COUNT_WRITE_NORMAL */
        set_field_ulonglong(f, m_row.m_stat.m_write_normal.m_count);
        break;
      case 59: /* SUM_TIMER_WRITE_NORMAL */
        set_field_ulonglong(f, m_row.m_stat.m_write_normal.m_sum);
        break;
      case 60: /* MIN_TIMER_WRITE_NORMAL */
        set_field_ulonglong(f, m_row.m_stat.m_write_normal.m_min);
        break;
      case 61: /* AVG_TIMER_WRITE_NORMAL */
        set_field_ulonglong(f, m_row.m_stat.m_write_normal.m_avg);
        break;
      case 62: /* MAX_TIMER_WRITE_NORMAL */
        set_field_ulonglong(f, m_row.m_stat.m_write_normal.m_max);
        break;

      case 63: /* COUNT_WRITE_EXTERNAL */
        set_field_ulonglong(f, m_row.m_stat.m_write_external.m_count);
        break;
      case 64: /* SUM_TIMER_WRITE_EXTERNAL */
        set_field_ulonglong(f, m_row.m_stat.m_write_external.m_sum);
        break;
      case 65: /* MIN_TIMER_WRITE_EXTERNAL */
        set_field_ulonglong(f, m_row.m_stat.m_write_external.m_min);
        break;
      case 66: /* AVG_TIMER_WRITE_EXTERNAL */
        set_field_ulonglong(f, m_row.m_stat.m_write_external.m_avg);
        break;
      case 67: /* MAX_TIMER_WRITE_EXTERNAL */
        set_field_ulonglong(f, m_row.m_stat.m_write_external.m_max);
        break;

      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

