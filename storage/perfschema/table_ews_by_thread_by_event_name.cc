/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

THR_LOCK table_ews_by_thread_by_event_name::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("THREAD_ID") },
    { C_STRING_WITH_LEN("int(11)") },
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
  reset_per_thread_wait_stat();
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
  PFS_mutex_class *mutex_class;
  PFS_rwlock_class *rwlock_class;
  PFS_cond_class *cond_class;
  PFS_file_class *file_class;
  PFS_instr_class *table_class;

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_view();
       m_pos.next_view())
  {
    switch (m_pos.m_index_1)
    {
    case pos_ews_by_thread_by_event_name::VIEW_MUTEX:
      do
      {
        mutex_class= find_mutex_class(m_pos.m_index_2);
        if (mutex_class)
        {
          for ( ; m_pos.has_more_thread(); m_pos.next_thread())
          {
            thread= &thread_array[m_pos.m_index_3];
            if (thread->m_lock.is_populated())
            {
              make_row(thread, mutex_class);
              m_next_pos.set_after(&m_pos);
              return 0;
            }
          }
          m_pos.next_instrument();
        }
      } while (mutex_class != NULL);
      break;
    case pos_ews_by_thread_by_event_name::VIEW_RWLOCK:
      do
      {
        rwlock_class= find_rwlock_class(m_pos.m_index_2);
        if (rwlock_class)
        {
          for ( ; m_pos.has_more_thread(); m_pos.next_thread())
          {
            thread= &thread_array[m_pos.m_index_3];
            if (thread->m_lock.is_populated())
            {
              make_row(thread, rwlock_class);
              m_next_pos.set_after(&m_pos);
              return 0;
            }
          }
          m_pos.next_instrument();
        }
      } while (rwlock_class != NULL);
      break;
    case pos_ews_by_thread_by_event_name::VIEW_COND:
      do
      {
        cond_class= find_cond_class(m_pos.m_index_2);
        if (cond_class)
        {
          for ( ; m_pos.has_more_thread(); m_pos.next_thread())
          {
            thread= &thread_array[m_pos.m_index_3];
            if (thread->m_lock.is_populated())
            {
              make_row(thread, cond_class);
              m_next_pos.set_after(&m_pos);
              return 0;
            }
          }
          m_pos.next_instrument();
        }
      } while (cond_class != NULL);
      break;
    case pos_ews_by_thread_by_event_name::VIEW_FILE:
      do
      {
        file_class= find_file_class(m_pos.m_index_2);
        if (file_class)
        {
          for ( ; m_pos.has_more_thread(); m_pos.next_thread())
          {
            thread= &thread_array[m_pos.m_index_3];
            if (thread->m_lock.is_populated())
            {
              make_row(thread, file_class);
              m_next_pos.set_after(&m_pos);
              return 0;
            }
          }
          m_pos.next_instrument();
        }
      } while (file_class != NULL);
      break;
    case pos_ews_by_thread_by_event_name::VIEW_TABLE:
      do
      {
        table_class= find_table_class(m_pos.m_index_2);
        if (table_class)
        {
          for ( ; m_pos.has_more_thread(); m_pos.next_thread())
          {
            thread= &thread_array[m_pos.m_index_3];
            if (thread->m_lock.is_populated())
            {
              make_row(thread, table_class);
              m_next_pos.set_after(&m_pos);
              return 0;
            }
          }
          m_pos.next_instrument();
        }
      } while (table_class != NULL);
      break;
    default:
      break;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_ews_by_thread_by_event_name::rnd_pos(const void *pos)
{
  PFS_thread *thread;
  PFS_mutex_class *mutex_class;
  PFS_rwlock_class *rwlock_class;
  PFS_cond_class *cond_class;
  PFS_file_class *file_class;
  PFS_instr_class *table_class;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index_3 < thread_max);

  thread= &thread_array[m_pos.m_index_3];
  if (! thread->m_lock.is_populated())
    return HA_ERR_RECORD_DELETED;

  switch (m_pos.m_index_1)
  {
  case pos_ews_by_thread_by_event_name::VIEW_MUTEX:
    mutex_class= find_mutex_class(m_pos.m_index_2);
    if (mutex_class)
    {
      make_row(thread, mutex_class);
      return 0;
    }
    break;
  case pos_ews_by_thread_by_event_name::VIEW_RWLOCK:
    rwlock_class= find_rwlock_class(m_pos.m_index_2);
    if (rwlock_class)
    {
      make_row(thread, rwlock_class);
      return 0;
    }
    break;
  case pos_ews_by_thread_by_event_name::VIEW_COND:
    cond_class= find_cond_class(m_pos.m_index_2);
    if (cond_class)
    {
      make_row(thread, cond_class);
      return 0;
    }
    break;
  case pos_ews_by_thread_by_event_name::VIEW_FILE:
    file_class= find_file_class(m_pos.m_index_2);
    if (file_class)
    {
      make_row(thread, file_class);
      return 0;
    }
    break;
  case pos_ews_by_thread_by_event_name::VIEW_TABLE:
    table_class= find_table_class(m_pos.m_index_2);
    if (table_class)
    {
      make_row(thread, table_class);
      return 0;
    }
    break;
  }

  return HA_ERR_RECORD_DELETED;
}

void table_ews_by_thread_by_event_name
::make_row(PFS_thread *thread, PFS_instr_class *klass)
{
  pfs_lock lock;
  PFS_single_stat *event_name_array;
  PFS_single_stat *stat;
  uint index= klass->m_event_name_index;
  event_name_array= thread->m_instr_class_wait_stats;

  m_row_exists= false;

  /* Protect this reader against a thread termination */
  thread->m_lock.begin_optimistic_lock(&lock);

  m_row.m_thread_internal_id= thread->m_thread_internal_id;
  m_row.m_name= klass->m_name;
  m_row.m_name_length= klass->m_name_length;

  stat= & event_name_array[index];
  time_normalizer *normalizer= time_normalizer::get(wait_timer);
  m_row.m_stat.set(normalizer, stat);

  if (thread->m_lock.end_optimistic_lock(&lock))
    m_row_exists= true;
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
        set_field_ulong(f, m_row.m_thread_internal_id);
        break;
      case 1: /* NAME */
        set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
        break;
      case 2: /* COUNT */
        set_field_ulonglong(f, m_row.m_stat.m_count);
        break;
      case 3: /* SUM */
        set_field_ulonglong(f, m_row.m_stat.m_sum);
        break;
      case 4: /* MIN */
        set_field_ulonglong(f, m_row.m_stat.m_min);
        break;
      case 5: /* AVG */
        set_field_ulonglong(f, m_row.m_stat.m_avg);
        break;
      case 6: /* MAX */
        set_field_ulonglong(f, m_row.m_stat.m_max);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

