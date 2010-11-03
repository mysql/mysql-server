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
  @file storage/perfschema/table_events_waits_summary.cc
  Table EVENTS_WAITS_SUMMARY_BY_xxx (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_events_waits_summary.h"
#include "pfs_global.h"

THR_LOCK table_events_waits_summary_by_thread_by_event_name::m_table_lock;

static const TABLE_FIELD_TYPE ews_by_thread_by_event_name_field_types[]=
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
table_events_waits_summary_by_thread_by_event_name::m_field_def=
{ 7, ews_by_thread_by_event_name_field_types };

PFS_engine_table_share
table_events_waits_summary_by_thread_by_event_name::m_share=
{
  { C_STRING_WITH_LEN("events_waits_summary_by_thread_by_event_name") },
  &pfs_truncatable_acl,
  &table_events_waits_summary_by_thread_by_event_name::create,
  NULL, /* write_row */
  &table_events_waits_summary_by_thread_by_event_name::delete_all_rows,
  1000, /* records */
  sizeof(pos_events_waits_summary_by_thread_by_event_name),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table*
table_events_waits_summary_by_thread_by_event_name::create(void)
{
  return new table_events_waits_summary_by_thread_by_event_name();
}

int
table_events_waits_summary_by_thread_by_event_name::delete_all_rows(void)
{
  reset_per_thread_wait_stat();
  return 0;
}

table_events_waits_summary_by_thread_by_event_name
::table_events_waits_summary_by_thread_by_event_name()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(), m_next_pos()
{}

void table_events_waits_summary_by_thread_by_event_name::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_events_waits_summary_by_thread_by_event_name::rnd_next(void)
{
  PFS_thread *thread;
  PFS_mutex_class *mutex_class;
  PFS_rwlock_class *rwlock_class;
  PFS_cond_class *cond_class;
  PFS_file_class *file_class;

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_thread();
       m_pos.next_thread())
  {
    thread= &thread_array[m_pos.m_index_1];
    if (thread->m_lock.is_populated())
    {
      for ( ; m_pos.has_more_view(); m_pos.next_view())
      {
        switch (m_pos.m_index_2) {
        case pos_events_waits_summary_by_thread_by_event_name::VIEW_MUTEX:
          mutex_class= find_mutex_class(m_pos.m_index_3);
          if (mutex_class)
          {
            make_mutex_row(thread, mutex_class);
            m_next_pos.set_after(&m_pos);
            return 0;
          }
          break;
        case pos_events_waits_summary_by_thread_by_event_name::VIEW_RWLOCK:
          rwlock_class= find_rwlock_class(m_pos.m_index_3);
          if (rwlock_class)
          {
            make_rwlock_row(thread, rwlock_class);
            m_next_pos.set_after(&m_pos);
            return 0;
          }
          break;
        case pos_events_waits_summary_by_thread_by_event_name::VIEW_COND:
          cond_class= find_cond_class(m_pos.m_index_3);
          if (cond_class)
          {
            make_cond_row(thread, cond_class);
            m_next_pos.set_after(&m_pos);
            return 0;
          }
          break;
        case pos_events_waits_summary_by_thread_by_event_name::VIEW_FILE:
          file_class= find_file_class(m_pos.m_index_3);
          if (file_class)
          {
            make_file_row(thread, file_class);
            m_next_pos.set_after(&m_pos);
            return 0;
          }
          break;
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_events_waits_summary_by_thread_by_event_name::rnd_pos(const void *pos)
{
  PFS_thread *thread;
  PFS_mutex_class *mutex_class;
  PFS_rwlock_class *rwlock_class;
  PFS_cond_class *cond_class;
  PFS_file_class *file_class;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index_1 < thread_max);

  thread= &thread_array[m_pos.m_index_1];
  if (! thread->m_lock.is_populated())
    return HA_ERR_RECORD_DELETED;

  switch (m_pos.m_index_2) {
  case pos_events_waits_summary_by_thread_by_event_name::VIEW_MUTEX:
    mutex_class= find_mutex_class(m_pos.m_index_3);
    if (mutex_class)
    {
      make_mutex_row(thread, mutex_class);
      return 0;
    }
    break;
  case pos_events_waits_summary_by_thread_by_event_name::VIEW_RWLOCK:
    rwlock_class= find_rwlock_class(m_pos.m_index_3);
    if (rwlock_class)
    {
      make_rwlock_row(thread, rwlock_class);
      return 0;
    }
    break;
  case pos_events_waits_summary_by_thread_by_event_name::VIEW_COND:
    cond_class= find_cond_class(m_pos.m_index_3);
    if (cond_class)
    {
      make_cond_row(thread, cond_class);
      return 0;
    }
    break;
  case pos_events_waits_summary_by_thread_by_event_name::VIEW_FILE:
    file_class= find_file_class(m_pos.m_index_3);
    if (file_class)
    {
      make_file_row(thread, file_class);
      return 0;
    }
    break;
  }
  return HA_ERR_RECORD_DELETED;
}

void table_events_waits_summary_by_thread_by_event_name
::make_instr_row(PFS_thread *thread, PFS_instr_class *klass,
                 PFS_single_stat_chain *stat)
{
  pfs_lock lock;

  m_row_exists= false;

  /* Protect this reader against a thread termination */
  thread->m_lock.begin_optimistic_lock(&lock);

  m_row.m_thread_internal_id= thread->m_thread_internal_id;
  m_row.m_name= klass->m_name;
  m_row.m_name_length= klass->m_name_length;

  m_row.m_count= stat->m_count;
  m_row.m_sum= stat->m_sum;
  m_row.m_min= stat->m_min;
  m_row.m_max= stat->m_max;

  if (m_row.m_count)
    m_row.m_avg= m_row.m_sum / m_row.m_count;
  else
  {
    m_row.m_min= 0;
    m_row.m_avg= 0;
  }

  if (thread->m_lock.end_optimistic_lock(&lock))
    m_row_exists= true;
}

void table_events_waits_summary_by_thread_by_event_name
::make_mutex_row(PFS_thread *thread, PFS_mutex_class *klass)
{
  PFS_single_stat_chain *stat;
  stat= find_per_thread_mutex_class_wait_stat(thread, klass);
  make_instr_row(thread, klass, stat);
}

void table_events_waits_summary_by_thread_by_event_name
::make_rwlock_row(PFS_thread *thread, PFS_rwlock_class *klass)
{
  PFS_single_stat_chain *stat;
  stat= find_per_thread_rwlock_class_wait_stat(thread, klass);
  make_instr_row(thread, klass, stat);
}

void table_events_waits_summary_by_thread_by_event_name
::make_cond_row(PFS_thread *thread, PFS_cond_class *klass)
{
  PFS_single_stat_chain *stat;
  stat= find_per_thread_cond_class_wait_stat(thread, klass);
  make_instr_row(thread, klass, stat);
}

void table_events_waits_summary_by_thread_by_event_name
::make_file_row(PFS_thread *thread, PFS_file_class *klass)
{
  PFS_single_stat_chain *stat;
  stat= find_per_thread_file_class_wait_stat(thread, klass);
  make_instr_row(thread, klass, stat);
}

int table_events_waits_summary_by_thread_by_event_name
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
        set_field_ulonglong(f, m_row.m_count);
        break;
      case 3: /* SUM */
        set_field_ulonglong(f, m_row.m_sum);
        break;
      case 4: /* MIN */
        set_field_ulonglong(f, m_row.m_min);
        break;
      case 5: /* AVG */
        set_field_ulonglong(f, m_row.m_avg);
        break;
      case 6: /* MAX */
        set_field_ulonglong(f, m_row.m_max);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

THR_LOCK table_events_waits_summary_by_instance::m_table_lock;

static const TABLE_FIELD_TYPE ews_by_instance_field_types[]=
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
table_events_waits_summary_by_instance::m_field_def=
{ 7, ews_by_instance_field_types };

PFS_engine_table_share
table_events_waits_summary_by_instance::m_share=
{
  { C_STRING_WITH_LEN("events_waits_summary_by_instance") },
  &pfs_truncatable_acl,
  &table_events_waits_summary_by_instance::create,
  NULL, /* write_row */
  &table_events_waits_summary_by_instance::delete_all_rows,
  1000, /* records */
  sizeof(pos_all_instr),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_events_waits_summary_by_instance::create(void)
{
  return new table_events_waits_summary_by_instance();
}

int table_events_waits_summary_by_instance::delete_all_rows(void)
{
  reset_events_waits_by_instance();
  return 0;
}

table_events_waits_summary_by_instance
::table_events_waits_summary_by_instance()
  : table_all_instr(&m_share), m_row_exists(false)
{}

void table_events_waits_summary_by_instance
::make_instr_row(PFS_instr *pfs, PFS_instr_class *klass,
                 const void *object_instance_begin)
{
  pfs_lock lock;

  m_row_exists= false;

  /*
    Protect this reader against a mutex/rwlock/cond destroy,
    file delete, table drop.
  */
  pfs->m_lock.begin_optimistic_lock(&lock);

  m_row.m_name= klass->m_name;
  m_row.m_name_length= klass->m_name_length;
  m_row.m_object_instance_addr= (intptr) object_instance_begin;

  m_row.m_count= pfs->m_wait_stat.m_count;
  m_row.m_sum= pfs->m_wait_stat.m_sum;
  m_row.m_min= pfs->m_wait_stat.m_min;
  m_row.m_max= pfs->m_wait_stat.m_max;

  if (m_row.m_count)
    m_row.m_avg= m_row.m_sum / m_row.m_count;
  else
  {
    m_row.m_min= 0;
    m_row.m_avg= 0;
  }

  if (pfs->m_lock.end_optimistic_lock(&lock))
    m_row_exists= true;
}

/**
  Build a row, for mutex statistics in a thread.
  @param pfs              the mutex this cursor is reading
*/
void table_events_waits_summary_by_instance::make_mutex_row(PFS_mutex *pfs)
{
  PFS_mutex_class *safe_class;
  safe_class= sanitize_mutex_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
    return;

  make_instr_row(pfs, safe_class, pfs->m_identity);
}

/**
  Build a row, for rwlock statistics in a thread.
  @param pfs              the rwlock this cursor is reading
*/
void table_events_waits_summary_by_instance::make_rwlock_row(PFS_rwlock *pfs)
{
  PFS_rwlock_class *safe_class;
  safe_class= sanitize_rwlock_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
    return;

  make_instr_row(pfs, safe_class, pfs->m_identity);
}

/**
  Build a row, for condition statistics in a thread.
  @param pfs              the condition this cursor is reading
*/
void table_events_waits_summary_by_instance::make_cond_row(PFS_cond *pfs)
{
  PFS_cond_class *safe_class;
  safe_class= sanitize_cond_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
    return;

  make_instr_row(pfs, safe_class, pfs->m_identity);
}

/**
  Build a row, for file statistics in a thread.
  @param pfs              the file this cursor is reading
*/
void table_events_waits_summary_by_instance::make_file_row(PFS_file *pfs)
{
  PFS_file_class *safe_class;
  safe_class= sanitize_file_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
    return;

  /*
    Files don't have a in memory structure associated to it,
    so we use the address of the PFS_file buffer as object_instance_begin
  */
  make_instr_row(pfs, safe_class, pfs);
}

int table_events_waits_summary_by_instance
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
      case 0: /* NAME */
        set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
        break;
      case 1: /* OBJECT_INSTANCE */
        set_field_ulonglong(f, m_row.m_object_instance_addr);
        break;
      case 2: /* COUNT */
        set_field_ulonglong(f, m_row.m_count);
        break;
      case 3: /* SUM */
        set_field_ulonglong(f, m_row.m_sum);
        break;
      case 4: /* MIN */
        set_field_ulonglong(f, m_row.m_min);
        break;
      case 5: /* AVG */
        set_field_ulonglong(f, m_row.m_avg);
        break;
      case 6: /* MAX */
        set_field_ulonglong(f, m_row.m_max);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

