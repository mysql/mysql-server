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
  @file storage/perfschema/table_socket_summary_by_event_name.cc
  Table SOCKET_EVENT_NAMES (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_instr.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_socket_summary_by_event_name.h"
#include "pfs_global.h"
#include "pfs_visitor.h"

THR_LOCK table_socket_summary_by_event_name::m_table_lock;

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
table_socket_summary_by_event_name::m_field_def=
{ 83, field_types };

PFS_engine_table_share
table_socket_summary_by_event_name::m_share=
{
  { C_STRING_WITH_LEN("socket_summary_by_event_name") },
  &pfs_readonly_acl,
  &table_socket_summary_by_event_name::create,
  NULL, /* write_row */
  table_socket_summary_by_event_name::delete_all_rows,
  NULL, /* get_row_count */
  1000, /* records */ // TBD: Check this
  sizeof(PFS_simple_index),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_socket_summary_by_event_name::create(void)
{
  return new table_socket_summary_by_event_name();
}

table_socket_summary_by_event_name::table_socket_summary_by_event_name()
  : PFS_engine_table(&m_share, &m_pos),
  m_row_exists(false), m_pos(1), m_next_pos(1)
{}

int table_socket_summary_by_event_name::delete_all_rows(void)
{
  reset_socket_instance_io();
  reset_socket_class_io();
  return 0;
}

void table_socket_summary_by_event_name::reset_position(void)
{
  m_pos.m_index= 1;
  m_next_pos.m_index= 1;
}

int table_socket_summary_by_event_name::rnd_next(void)
{
  PFS_socket_class *socket_class;

  m_pos.set_at(&m_next_pos);

  socket_class= find_socket_class(m_pos.m_index);
  if (socket_class)
  {
    make_row(socket_class);
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int table_socket_summary_by_event_name::rnd_pos(const void *pos)
{
  PFS_socket_class *socket_class;

  set_position(pos);

  socket_class= find_socket_class(m_pos.m_index);
  if (socket_class)
  {
    make_row(socket_class);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

void table_socket_summary_by_event_name::make_row(PFS_socket_class *socket_class)
{
  m_row.m_event_name.make_row(socket_class);

  PFS_instance_socket_io_stat_visitor visitor;
  PFS_instance_iterator::visit_socket_instances(socket_class, &visitor);

  time_normalizer *normalizer= time_normalizer::get(wait_timer);
  
  /* Collect timer and byte count stats */
  m_row.m_io_stat.set(normalizer, &visitor.m_socket_io_stat);
  m_row_exists= true;
}

int table_socket_summary_by_event_name::read_row_values(TABLE *table,
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
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_waits.m_count);
        break;
      case  7: /* SUM_TIMER_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_waits.m_sum);
        break;
      case  8: /* MIN_TIMER_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_waits.m_min);
        break;
      case  9: /* AVG_TIMER_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_waits.m_avg);
        break;
      case 10: /* MAX_TIMER_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_waits.m_max);
        break;
      case 11: /* SUM_NUMBER_OF_BYTES_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_bytes.m_sum);
        break;
      case 12: /* MIN_NUMBER_OF_BYTES_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_bytes.m_min);
        break;
      case 13: /* AVG_NUMBER_OF_BYTES_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_bytes.m_avg);
        break;
      case 14: /* MAX_NUMBER_OF_BYTES_READ */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_read.m_bytes.m_max);
        break;

      case 15: /* COUNT_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_waits.m_count);
        break;
      case 16: /* SUM_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_waits.m_sum);
        break;
      case 17: /* MIN_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_waits.m_min);
        break;
      case 18: /* AVG_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_waits.m_avg);
        break;
      case 19: /* MAX_TIMER_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_waits.m_max);
        break;
      case 20: /* SUM_NUMBER_OF_BYTES_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_bytes.m_sum);
        break;
      case 21: /* MIN_NUMBER_OF_BYTES_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_bytes.m_min);
        break;
      case 22: /* AVG_NUMBER_OF_BYTES_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_bytes.m_avg);
        break;
      case 23: /* MAX_NUMBER_OF_BYTES_WRITE */
        set_field_ulonglong(f, m_row.m_io_stat.m_all_write.m_bytes.m_max);
        break;

      case 24: /* COUNT_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_waits.m_count);
        break;
      case 25: /* SUM_TIMER_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_waits.m_sum);
        break;
      case 26: /* MIN_TIMER_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_waits.m_min);
        break;
      case 27: /* AVG_TIMER_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_waits.m_avg);
        break;
      case 28: /* MAX_TIMER_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_waits.m_max);
        break;
      case 29: /* SUM_NUMBER_OF_BYTES_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_bytes.m_sum);
        break;
      case 30: /* MIN_NUMBER_OF_BYTES_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_bytes.m_min);
        break;
      case 31: /* AVG_NUMBER_OF_BYTES_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_bytes.m_avg);
        break;
      case 32: /* MAX_NUMBER_OF_BYTES_RECV */
        set_field_ulonglong(f, m_row.m_io_stat.m_recv.m_bytes.m_max);
        break;

      case 33: /* COUNT_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_waits.m_count);
        break;
      case 34: /* SUM_TIMER_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_waits.m_sum);
        break;
      case 35: /* MIN_TIMER_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_waits.m_min);
        break;
      case 36: /* AVG_TIMER_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_waits.m_avg);
        break;
      case 37: /* MAX_TIMER_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_waits.m_max);
        break;
      case 38: /* SUM_NUMBER_OF_BYTES_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_bytes.m_sum);
        break;
      case 39: /* MIN_NUMBER_OF_BYTES_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_bytes.m_min);
        break;
      case 40: /* AVG_NUMBER_OF_BYTES_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_bytes.m_avg);
        break;
      case 41: /* MAX_NUMBER_OF_BYTES_SEND */
        set_field_ulonglong(f, m_row.m_io_stat.m_send.m_bytes.m_max);
        break;

      case 42: /* COUNT_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_waits.m_count);
        break;
      case 43: /* SUM_TIMER_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_waits.m_sum);
        break;
      case 44: /* MIN_TIMER_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_waits.m_min);
        break;
      case 45: /* AVG_TIMER_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_waits.m_avg);
        break;
      case 46: /* MAX_TIMER_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_waits.m_max);
        break;
      case 47: /* SUM_NUMBER_OF_BYTES_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_bytes.m_sum);
        break;
      case 48: /* MIN_NUMBER_OF_BYTES_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_bytes.m_min);
        break;
      case 49: /* AVG_NUMBER_OF_BYTES_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_bytes.m_avg);
        break;
      case 50: /* MAX_NUMBER_OF_BYTES_RECVFROM */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvfrom.m_bytes.m_max);
        break;

      case 51: /* COUNT_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_waits.m_count);
        break;
      case 52: /* SUM_TIMER_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_waits.m_sum);
        break;
      case 53: /* MIN_TIMER_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_waits.m_min);
        break;
      case 54: /* AVG_TIMER_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_waits.m_avg);
        break;
      case 55: /* MAX_TIMER_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_waits.m_max);
        break;
      case 56: /* SUM_NUMBER_OF_BYTES_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_bytes.m_sum);
        break;
      case 57: /* MIN_NUMBER_OF_BYTES_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_bytes.m_min);
        break;
      case 58: /* AVG_NUMBER_OF_BYTES_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_bytes.m_avg);
        break;
      case 59: /* MAX_NUMBER_OF_BYTES_SENDTO */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendto.m_bytes.m_max);
        break;

      case 60: /* COUNT_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_waits.m_count);
        break;
      case 61: /* SUM_TIMER_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_waits.m_sum);
        break;
      case 62: /* MIN_TIMER_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_waits.m_min);
        break;
      case 63: /* AVG_TIMER_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_waits.m_avg);
        break;
      case 64: /* MAX_TIMER_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_waits.m_max);
        break;
      case 65: /* SUM_NUMBER_OF_BYTES_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_bytes.m_sum);
        break;
      case 66: /* MIN_NUMBER_OF_BYTES_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_bytes.m_min);
        break;
      case 67: /* AVG_NUMBER_OF_BYTES_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_bytes.m_avg);
        break;
      case 68: /* MAX_NUMBER_OF_BYTES_RECVMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_recvmsg.m_bytes.m_max);
        break;

      case 69: /* COUNT_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_waits.m_count);
        break;
      case 70: /* SUM_TIMER_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_waits.m_sum);
        break;
      case 71: /* MIN_TIMER_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_waits.m_min);
        break;
      case 72: /* AVG_TIMER_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_waits.m_avg);
        break;
      case 73: /* MAX_TIMER_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_waits.m_max);
        break;
      case 74: /* SUM_NUMBER_OF_BYTES_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_bytes.m_sum);
        break;
      case 75: /* MIN_NUMBER_OF_BYTES_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_bytes.m_min);
        break;
      case 76: /* AVG_NUMBER_OF_BYTES_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_bytes.m_avg);
        break;
      case 77: /* MAX_NUMBER_OF_BYTES_SENDMSG */
        set_field_ulonglong(f, m_row.m_io_stat.m_sendmsg.m_bytes.m_max);
        break;

      case 78: /* COUNT_MISC */
        set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_count);
        break;
      case 79: /* SUM_TIMER_MISC */
        set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_sum);
        break;
      case 80: /* MIN_TIMER_MISC */
        set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_min);
        break;
      case 81: /* AVG_TIMER_MISC */
        set_field_ulonglong(f, m_row.m_io_stat.m_misc.m_waits.m_avg);
        break;
      case 82: /* MAX_TIMER_MISC */
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

