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
  @file storage/perfschema/table_socket_summary_by_instance.cc
  Table SOCKET_INSTANCES (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_instr.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_socket_summary_by_instance.h"
#include "pfs_global.h"

THR_LOCK table_socket_summary_by_instance::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("EVENT_NAME") },
    { C_STRING_WITH_LEN("varchar(128)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("OBJECT_INSTANCE_BEGIN") },
    { C_STRING_WITH_LEN("bigint(20)") },
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
  {
    { C_STRING_WITH_LEN("MIN_NUMBER_OF_BYTES_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_NUMBER_OF_BYTES_READ") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_NUMBER_OF_BYTES_READ") },
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
  {
    { C_STRING_WITH_LEN("MIN_NUMBER_OF_BYTES_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_NUMBER_OF_BYTES_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_NUMBER_OF_BYTES_WRITE") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },

  /** Recv */
  {
    { C_STRING_WITH_LEN("COUNT_RECV") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_RECV") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_RECV") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_RECV") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_RECV") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_RECV") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_NUMBER_OF_BYTES_RECV") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_NUMBER_OF_BYTES_RECV") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_NUMBER_OF_BYTES_RECV") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },

  /** Send */
  {
    { C_STRING_WITH_LEN("COUNT_SEND") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_SEND") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_SEND") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_SEND") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_SEND") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_SEND") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_NUMBER_OF_BYTES_SEND") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_NUMBER_OF_BYTES_SEND") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_NUMBER_OF_BYTES_SEND") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },

  /** Recvfrom */
  {
    { C_STRING_WITH_LEN("COUNT_RECVFROM") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_RECVFROM") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_RECVFROM") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_RECVFROM") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_RECVFROM") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_RECVFROM") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_NUMBER_OF_BYTES_RECVFROM") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_NUMBER_OF_BYTES_RECVFROM") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_NUMBER_OF_BYTES_RECVFROM") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },

  /** Sendto */
  {
    { C_STRING_WITH_LEN("COUNT_SENDTO") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_SENDTO") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_SENDTO") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_SENDTO") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_SENDTO") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_SENDTO") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_NUMBER_OF_BYTES_SENDTO") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_NUMBER_OF_BYTES_SENDTO") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_NUMBER_OF_BYTES_SENDTO") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },

  /** Recvmsg */
  {
    { C_STRING_WITH_LEN("COUNT_RECVMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_RECVMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_RECVMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_RECVMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_RECVMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_RECVMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_NUMBER_OF_BYTES_RECVMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_NUMBER_OF_BYTES_RECVMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_NUMBER_OF_BYTES_RECVMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },

  /** Sendmsg */
  {
    { C_STRING_WITH_LEN("COUNT_SENDMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_SENDMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_SENDMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_SENDMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_SENDMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_NUMBER_OF_BYTES_SENDMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_NUMBER_OF_BYTES_SENDMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_NUMBER_OF_BYTES_SENDMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_NUMBER_OF_BYTES_SENDMSG") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },

  /** Connect */
  {
    { C_STRING_WITH_LEN("COUNT_CONNECT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_CONNECT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_CONNECT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_CONNECT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_CONNECT") },
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
table_socket_summary_by_instance::m_field_def=
{ 90, field_types };

PFS_engine_table_share
table_socket_summary_by_instance::m_share=
{
  { C_STRING_WITH_LEN("socket_summary_by_instance") },
  &pfs_readonly_acl,
  &table_socket_summary_by_instance::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  NULL, /* get_row_count */
  1000, /* records */ // TBD: Check this
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_socket_summary_by_instance::create(void)
{
  return new table_socket_summary_by_instance();
}

table_socket_summary_by_instance::table_socket_summary_by_instance()
  : PFS_engine_table(&m_share, &m_pos),
  m_row_exists(false), m_pos(0), m_next_pos(0)
{}

void table_socket_summary_by_instance::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_socket_summary_by_instance::rnd_next(void)
{
  PFS_socket *pfs;

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < socket_max;
       m_pos.next())
  {
    pfs= &socket_array[m_pos.m_index];
    if (pfs->m_lock.is_populated())
    {
      make_row(pfs);
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_socket_summary_by_instance::rnd_pos(const void *pos)
{
  PFS_socket *pfs;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < socket_max);
  pfs= &socket_array[m_pos.m_index];

  if (! pfs->m_lock.is_populated())
    return HA_ERR_RECORD_DELETED;

  make_row(pfs);
  return 0;
}

void table_socket_summary_by_instance::make_row(PFS_socket *pfs)
{
  pfs_lock lock;
  PFS_socket_class *safe_class;

  m_row_exists= false;

  /* Protect this reader against a socket delete */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class= sanitize_socket_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
    return;

  m_row.m_event_name.make_row(safe_class);
  m_row.m_identity= pfs->m_identity;

  if (!pfs->m_lock.end_optimistic_lock(&lock))
    return;

  m_row_exists= true;

  time_normalizer *normalizer= time_normalizer::get(wait_timer);
  
  /* Collect timer and byte count stats */
  m_row.m_io_stat.set(normalizer, &pfs->m_socket_stat.m_io_stat);
}

int table_socket_summary_by_instance::read_row_values(TABLE *table,
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
      case  1: /* OBJECT_INSTANCE */
        // TBD: Fix
        set_field_ulonglong(f, (ulonglong)m_row.m_identity);
        break;
      case  2: /* OBJECT_NAME */
        // TBD: Fix
        break;
      case  3: /* COUNT_STAR */
        set_field_ulonglong(f, m_row.m_io_stat.m_all.m_count);
        break;
      case  4: /* SUM_TIMER_WAIT */
        set_field_ulonglong(f, m_row.m_io_stat.m_all.m_sum);
        break;
      case  5: /* MIN_TIMER_WAIT */
        set_field_ulonglong(f, m_row.m_io_stat.m_all.m_min);
        break;
      case  6: /* AVG_TIMER_WAIT */
        set_field_ulonglong(f, m_row.m_io_stat.m_all.m_avg);
        break;
      case  7: /* MAX_TIMER_WAIT */
        set_field_ulonglong(f, m_row.m_io_stat.m_all.m_max);
        break;

      case  8: /* COUNT_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_count);
        break;
      case  9: /* SUM_TIMER_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_sum);
        break;
      case 10: /* MIN_TIMER_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_min);
        break;
      case 11: /* AVG_TIMER_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_avg);
        break;
      case 12: /* MAX_TIMER_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_max);
        break;
      case 13: /* SUM_NUMBER_OF_BYTES_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_sum);
        break;
      case 14: /* MIN_NUMBER_OF_BYTES_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_min);
        break;
      case 15: /* AVG_NUMBER_OF_BYTES_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_avg);
        break;
      case 16: /* MAX_NUMBER_OF_BYTES_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_max);
        break;

      case 17: /* COUNT_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_count);
        break;
      case 18: /* SUM_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_sum);
        break;
      case 19: /* MIN_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_min);
        break;
      case 20: /* AVG_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_avg);
        break;
      case 21: /* MAX_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_max);
        break;
      case 22: /* SUM_NUMBER_OF_BYTES_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_sum);
        break;
      case 23: /* MIN_NUMBER_OF_BYTES_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_min);
        break;
      case 24: /* AVG_NUMBER_OF_BYTES_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_avg);
        break;
      case 25: /* MAX_NUMBER_OF_BYTES_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_max);
        break;

      case 26: /* COUNT_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_count);
        break;
      case 27: /* SUM_TIMER_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_sum);
        break;
      case 28: /* MIN_TIMER_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_min);
        break;
      case 29: /* AVG_TIMER_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_avg);
        break;
      case 30: /* MAX_TIMER_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_max);
        break;
      case 31: /* SUM_NUMBER_OF_BYTES_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_sum);
        break;
      case 32: /* MIN_NUMBER_OF_BYTES_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_min);
        break;
      case 33: /* AVG_NUMBER_OF_BYTES_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_avg);
        break;
      case 34: /* MAX_NUMBER_OF_BYTES_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_max);
        break;

      case 35: /* COUNT_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_count);
        break;
      case 36: /* SUM_TIMER_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_sum);
        break;
      case 37: /* MIN_TIMER_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_min);
        break;
      case 38: /* AVG_TIMER_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_avg);
        break;
      case 39: /* MAX_TIMER_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_max);
        break;
      case 40: /* SUM_NUMBER_OF_BYTES_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_sum);
        break;
      case 41: /* MIN_NUMBER_OF_BYTES_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_min);
        break;
      case 42: /* AVG_NUMBER_OF_BYTES_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_avg);
        break;
      case 43: /* MAX_NUMBER_OF_BYTES_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_max);
        break;

      case 44: /* COUNT_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_count);
        break;
      case 45: /* SUM_TIMER_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_sum);
        break;
      case 46: /* MIN_TIMER_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_min);
        break;
      case 47: /* AVG_TIMER_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_avg);
        break;
      case 48: /* MAX_TIMER_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_max);
        break;
      case 49: /* SUM_NUMBER_OF_BYTES_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_sum);
        break;
      case 50: /* MIN_NUMBER_OF_BYTES_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_min);
        break;
      case 51: /* AVG_NUMBER_OF_BYTES_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_avg);
        break;
      case 52: /* MAX_NUMBER_OF_BYTES_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_max);
        break;

      case 53: /* COUNT_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_count);
        break;
      case 54: /* SUM_TIMER_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_sum);
        break;
      case 55: /* MIN_TIMER_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_min);
        break;
      case 56: /* AVG_TIMER_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_avg);
        break;
      case 57: /* MAX_TIMER_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_max);
        break;
      case 58: /* SUM_NUMBER_OF_BYTES_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_sum);
        break;
      case 59: /* MIN_NUMBER_OF_BYTES_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_min);
        break;
      case 60: /* AVG_NUMBER_OF_BYTES_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_avg);
        break;
      case 61: /* MAX_NUMBER_OF_BYTES_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_max);
        break;

      case 62: /* COUNT_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_count);
        break;
      case 63: /* SUM_TIMER_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_sum);
        break;
      case 64: /* MIN_TIMER_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_min);
        break;
      case 65: /* AVG_TIMER_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_avg);
        break;
      case 66: /* MAX_TIMER_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_max);
        break;
      case 67: /* SUM_NUMBER_OF_BYTES_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_sum);
        break;
      case 68: /* MIN_NUMBER_OF_BYTES_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_min);
        break;
      case 69: /* AVG_NUMBER_OF_BYTES_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_avg);
        break;
      case 70: /* MAX_NUMBER_OF_BYTES_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_max);
        break;

      case 71: /* COUNT_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_count);
        break;
      case 72: /* SUM_TIMER_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_sum);
        break;
      case 73: /* MIN_TIMER_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_min);
        break;
      case 74: /* AVG_TIMER_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_avg);
        break;
      case 75: /* MAX_TIMER_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_max);
        break;
      case 76: /* SUM_NUMBER_OF_BYTES_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_sum);
        break;
      case 77: /* MIN_NUMBER_OF_BYTES_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_min);
        break;
      case 78: /* AVG_NUMBER_OF_BYTES_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_avg);
        break;
      case 79: /* MAX_NUMBER_OF_BYTES_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_max);
        break;

      case 80: /* COUNT_CONNECT */
        set_field_ulonglong(f, m_row.m_io_stat.m_connect.m_count);
        break;
      case 81: /* SUM_TIMER_CONNECT */
        set_field_ulonglong(f, m_row.m_io_stat.m_connect.m_sum);
        break;
      case 82: /* MIN_TIMER_CONNECT */
        set_field_ulonglong(f, m_row.m_io_stat.m_connect.m_min);
        break;
      case 83: /* AVG_TIMER_CONNECT */
        set_field_ulonglong(f, m_row.m_io_stat.m_connect.m_avg);
        break;
      case 84: /* MAX_TIMER_CONNECT */
        set_field_ulonglong(f, m_row.m_io_stat.m_connect.m_max);
        break;

      case 85: /* COUNT_MISC */
        set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_count);
        break;
      case 86: /* SUM_TIMER_MISC */
        set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_sum);
        break;
      case 87: /* MIN_TIMER_MISC */
        set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_min);
        break;
      case 88: /* AVG_TIMER_MISC */
        set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_avg);
        break;
      case 89: /* MAX_TIMER_MISC */
        set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_max);
        break;
      default:
        DBUG_ASSERT(false);
        break;
      }
    }
  }

  return 0;
}

