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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/table_ews_by_thread_by_event_name.cc
  Table EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_ews_by_thread_by_event_name.h"
#include "pfs_global.h"
#include "pfs_visitor.h"

THR_LOCK table_ews_by_thread_by_event_name::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("THREAD_ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
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
table_ews_by_thread_by_event_name::m_field_def=
{ 7, field_types };

PFS_engine_table_share
table_ews_by_thread_by_event_name::m_share=
{
  { C_STRING_WITH_LEN("events_waits_summary_by_thread_by_event_name") },
  &pfs_truncatable_acl,
  table_ews_by_thread_by_event_name::create,
  NULL, /* write_row */
  table_ews_by_thread_by_event_name::delete_all_rows,
  NULL, /* get_row_count */
  1000, /* records */
  sizeof(pos_ews_by_thread_by_event_name),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table*
table_ews_by_thread_by_event_name::create(void)
{
  return new table_ews_by_thread_by_event_name();
}

int
table_ews_by_thread_by_event_name::delete_all_rows(void)
{
  reset_events_waits_by_thread();
  return 0;
}

table_ews_by_thread_by_event_name::table_ews_by_thread_by_event_name()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(), m_next_pos()
{}

void table_ews_by_thread_by_event_name::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_ews_by_thread_by_event_name::rnd_next(void)
{
  PFS_thread *thread;
  PFS_instr_class *instr_class;

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_thread();
       m_pos.next_thread())
  {
    thread= &thread_array[m_pos.m_index_1];

    /*
      Important note: the thread scan is the outer loop (index 1),
      to minimize the number of calls to atomic operations.
    */
    if (thread->m_lock.is_populated())
    {
      for ( ;
           m_pos.has_more_view();
           m_pos.next_view())
      {
        switch (m_pos.m_index_2)
        {
        case pos_ews_by_thread_by_event_name::VIEW_MUTEX:
          instr_class= find_mutex_class(m_pos.m_index_3);
          break;
        case pos_ews_by_thread_by_event_name::VIEW_RWLOCK:
          instr_class= find_rwlock_class(m_pos.m_index_3);
          break;
        case pos_ews_by_thread_by_event_name::VIEW_COND:
          instr_class= find_cond_class(m_pos.m_index_3);
          break;
        case pos_ews_by_thread_by_event_name::VIEW_FILE:
          instr_class= find_file_class(m_pos.m_index_3);
          break;
        case pos_ews_by_thread_by_event_name::VIEW_TABLE:
          instr_class= find_table_class(m_pos.m_index_3);
          break;
        case pos_ews_by_thread_by_event_name::VIEW_SOCKET:
          instr_class= find_socket_class(m_pos.m_index_3);
          break;
        case pos_ews_by_thread_by_event_name::VIEW_IDLE:
          instr_class= find_idle_class(m_pos.m_index_3);
          break;
        default:
          DBUG_ASSERT(false);
          instr_class= NULL;
          break;
        }

        if (instr_class != NULL)
        {
          make_row(thread, instr_class);
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_ews_by_thread_by_event_name::rnd_pos(const void *pos)
{
  PFS_thread *thread;
  PFS_instr_class *instr_class;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index_1 < thread_max);

  thread= &thread_array[m_pos.m_index_1];
  if (! thread->m_lock.is_populated())
    return HA_ERR_RECORD_DELETED;

  switch (m_pos.m_index_2)
  {
  case pos_ews_by_thread_by_event_name::VIEW_MUTEX:
    instr_class= find_mutex_class(m_pos.m_index_3);
    break;
  case pos_ews_by_thread_by_event_name::VIEW_RWLOCK:
    instr_class= find_rwlock_class(m_pos.m_index_3);
    break;
  case pos_ews_by_thread_by_event_name::VIEW_COND:
    instr_class= find_cond_class(m_pos.m_index_3);
    break;
  case pos_ews_by_thread_by_event_name::VIEW_FILE:
    instr_class= find_file_class(m_pos.m_index_3);
    break;
  case pos_ews_by_thread_by_event_name::VIEW_TABLE:
    instr_class= find_table_class(m_pos.m_index_3);
    break;
  case pos_ews_by_thread_by_event_name::VIEW_SOCKET:
    instr_class= find_socket_class(m_pos.m_index_3);
    break;
  case pos_ews_by_thread_by_event_name::VIEW_IDLE:
    instr_class= find_idle_class(m_pos.m_index_3);
    break;
  default:
    DBUG_ASSERT(false);
    instr_class= NULL;
  }

  if (instr_class)
  {
    make_row(thread, instr_class);
    return 0;
  }
  return HA_ERR_RECORD_DELETED;
}

void table_ews_by_thread_by_event_name
::make_row(PFS_thread *thread, PFS_instr_class *klass)
{
  pfs_lock lock;
  m_row_exists= false;

  /* Protect this reader against a thread termination */
  thread->m_lock.begin_optimistic_lock(&lock);

  m_row.m_thread_internal_id= thread->m_thread_internal_id;

  m_row.m_event_name.make_row(klass);

  PFS_connection_wait_visitor visitor(klass);
  PFS_connection_iterator::visit_thread(thread, &visitor);

  /*
     If the aggregation for this class is deferred, then we must pull the
     current wait stats from the instances associated with this thread.
  */  
  if (klass->is_deferred())
  {
    /* Visit instances owned by this thread. Do not visit the class. */
    PFS_instance_wait_visitor inst_visitor;
    PFS_instance_iterator::visit_instances(klass, &inst_visitor,
                                           thread, false);
    /* Combine the deferred stats and global stats */
    visitor.m_stat.aggregate(&inst_visitor.m_stat);
  }

  if (! thread->m_lock.end_optimistic_lock(&lock))
    return;

  m_row_exists= true;

  get_normalizer(klass);
  m_row.m_stat.set(m_normalizer, & visitor.m_stat);
}

int table_ews_by_thread_by_event_name
::read_row_values(TABLE *table, unsigned char *, Field **fields,
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
      case 0: /* THREAD_ID */
        set_field_ulonglong(f, m_row.m_thread_internal_id);
        break;
      case 1: /* EVENT_NAME */
        m_row.m_event_name.set_field(f);
        break;
      default: /* 2, ... COUNT/SUM/MIN/AVG/MAX */
        m_row.m_stat.set_field(f->field_index - 2, f);
        break;
      }
    }
  }

  return 0;
}

