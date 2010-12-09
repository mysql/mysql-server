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
#include "pfs_instr.h"
#include "pfs_timer.h"

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
  table_ews_global_by_event_name::create,
  NULL, /* write_row */
  table_ews_global_by_event_name::delete_all_rows,
  NULL, /* get_row_count */
  1000, /* records */
  sizeof(pos_ews_global_by_event_name),
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table*
table_ews_global_by_event_name::create(void)
{
  return new table_ews_global_by_event_name();
}

int
table_ews_global_by_event_name::delete_all_rows(void)
{
  reset_events_waits_by_instance();
  reset_table_waits_by_table_handle();
  reset_table_waits_by_table();
  reset_global_wait_stat();
  return 0;
}

table_ews_global_by_event_name::table_ews_global_by_event_name()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(), m_next_pos()
{}

void table_ews_global_by_event_name::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_ews_global_by_event_name::rnd_next(void)
{
  PFS_mutex_class *mutex_class;
  PFS_rwlock_class *rwlock_class;
  PFS_cond_class *cond_class;
  PFS_file_class *file_class;

  if (global_instr_class_waits_array == NULL)
    return HA_ERR_END_OF_FILE;

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_view();
       m_pos.next_view())
  {
    switch (m_pos.m_index_1)
    {
    case pos_ews_global_by_event_name::VIEW_MUTEX:
      mutex_class= find_mutex_class(m_pos.m_index_2);
      if (mutex_class)
      {
        make_mutex_row(mutex_class);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
      break;
    case pos_ews_global_by_event_name::VIEW_RWLOCK:
      rwlock_class= find_rwlock_class(m_pos.m_index_2);
      if (rwlock_class)
      {
        make_rwlock_row(rwlock_class);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
      break;
    case pos_ews_global_by_event_name::VIEW_COND:
      cond_class= find_cond_class(m_pos.m_index_2);
      if (cond_class)
      {
        make_cond_row(cond_class);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
      break;
    case pos_ews_global_by_event_name::VIEW_FILE:
      file_class= find_file_class(m_pos.m_index_2);
      if (file_class)
      {
        make_file_row(file_class);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
      break;
    case pos_ews_global_by_event_name::VIEW_TABLE:
      if (m_pos.m_index_2 == 1)
      {
        make_table_io_row(&global_table_io_class);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
      if (m_pos.m_index_2 == 2)
      {
        make_table_lock_row(&global_table_lock_class);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
      break;
    default:
      break;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_ews_global_by_event_name::rnd_pos(const void *pos)
{
  PFS_mutex_class *mutex_class;
  PFS_rwlock_class *rwlock_class;
  PFS_cond_class *cond_class;
  PFS_file_class *file_class;

  set_position(pos);

  if (global_instr_class_waits_array == NULL)
    return HA_ERR_END_OF_FILE;

  switch (m_pos.m_index_1)
  {
  case pos_ews_global_by_event_name::VIEW_MUTEX:
    mutex_class= find_mutex_class(m_pos.m_index_2);
    if (mutex_class)
    {
      make_mutex_row(mutex_class);
      return 0;
    }
    break;
  case pos_ews_global_by_event_name::VIEW_RWLOCK:
    rwlock_class= find_rwlock_class(m_pos.m_index_2);
    if (rwlock_class)
    {
      make_rwlock_row(rwlock_class);
      return 0;
    }
    break;
  case pos_ews_global_by_event_name::VIEW_COND:
    cond_class= find_cond_class(m_pos.m_index_2);
    if (cond_class)
    {
      make_cond_row(cond_class);
      return 0;
    }
    break;
  case pos_ews_global_by_event_name::VIEW_FILE:
    file_class= find_file_class(m_pos.m_index_2);
    if (file_class)
    {
      make_file_row(file_class);
      return 0;
    }
    break;
  case pos_ews_global_by_event_name::VIEW_TABLE:
    DBUG_ASSERT(m_pos.m_index_2 >= 1);
    DBUG_ASSERT(m_pos.m_index_2 <= 2);
    if (m_pos.m_index_2 == 1)
      make_table_io_row(&global_table_io_class);
    else
      make_table_lock_row(&global_table_lock_class);
    break;
  }

  return HA_ERR_RECORD_DELETED;
}


void table_ews_global_by_event_name
::make_mutex_row(PFS_mutex_class *klass)
{
  m_row.m_name= klass->m_name;
  m_row.m_name_length= klass->m_name_length;
  uint index= klass->m_event_name_index;
  PFS_single_stat cumulated_stat= global_instr_class_waits_array[index];

  if (klass->is_singleton())
  {
    PFS_mutex *pfs= sanitize_mutex(klass->m_singleton);
    if (likely(pfs != NULL))
    {
      if (likely(pfs->m_lock.is_populated()))
      {
        cumulated_stat.aggregate(& pfs->m_wait_stat);
      }
    }
  }
  else
  {
    /* For all the mutex instances ... */
    PFS_mutex *pfs= mutex_array;
    PFS_mutex *pfs_last= mutex_array + mutex_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if ((pfs->m_class == klass) && pfs->m_lock.is_populated())
      {
        /*
          If the instance belongs to this class,
          aggregate the instance statistics.
        */
        cumulated_stat.aggregate(& pfs->m_wait_stat);
      }
    }
  }

  time_normalizer *normalizer= time_normalizer::get(wait_timer);
  m_row.m_stat.set(normalizer, &cumulated_stat);
  m_row_exists= true;
}

void table_ews_global_by_event_name
::make_rwlock_row(PFS_rwlock_class *klass)
{
  m_row.m_name= klass->m_name;
  m_row.m_name_length= klass->m_name_length;
  uint index= klass->m_event_name_index;
  PFS_single_stat cumulated_stat= global_instr_class_waits_array[index];

  if (klass->is_singleton())
  {
    PFS_rwlock *pfs= sanitize_rwlock(klass->m_singleton);
    if (likely(pfs != NULL))
    {
      if (likely(pfs->m_lock.is_populated()))
      {
        cumulated_stat.aggregate(& pfs->m_wait_stat);
      }
    }
  }
  else
  {
    /* For all the rwlock instances ... */
    PFS_rwlock *pfs= rwlock_array;
    PFS_rwlock *pfs_last= rwlock_array + rwlock_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if ((pfs->m_class == klass) && pfs->m_lock.is_populated())
      {
        /*
          If the instance belongs to this class,
          aggregate the instance statistics.
        */
        cumulated_stat.aggregate(& pfs->m_wait_stat);
      }
    }
  }

  time_normalizer *normalizer= time_normalizer::get(wait_timer);
  m_row.m_stat.set(normalizer, &cumulated_stat);
  m_row_exists= true;
}

void table_ews_global_by_event_name
::make_cond_row(PFS_cond_class *klass)
{
  m_row.m_name= klass->m_name;
  m_row.m_name_length= klass->m_name_length;
  uint index= klass->m_event_name_index;
  PFS_single_stat cumulated_stat= global_instr_class_waits_array[index];

  if (klass->is_singleton())
  {
    PFS_cond *pfs= sanitize_cond(klass->m_singleton);
    if (likely(pfs != NULL))
    {
      if (likely(pfs->m_lock.is_populated()))
      {
        cumulated_stat.aggregate(& pfs->m_wait_stat);
      }
    }
  }
  else
  {
    /* For all the cond instances ... */
    PFS_cond *pfs= cond_array;
    PFS_cond *pfs_last= cond_array + cond_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if ((pfs->m_class == klass) && pfs->m_lock.is_populated())
      {
        /*
          If the instance belongs to this class,
          aggregate the instance statistics.
        */
        cumulated_stat.aggregate(& pfs->m_wait_stat);
      }
    }
  }

  time_normalizer *normalizer= time_normalizer::get(wait_timer);
  m_row.m_stat.set(normalizer, &cumulated_stat);
  m_row_exists= true;
}

void table_ews_global_by_event_name
::make_file_row(PFS_file_class *klass)
{
  m_row.m_name= klass->m_name;
  m_row.m_name_length= klass->m_name_length;
  uint index= klass->m_event_name_index;
  PFS_single_stat cumulated_stat= global_instr_class_waits_array[index];

  if (klass->is_singleton())
  {
    PFS_file *pfs= sanitize_file(klass->m_singleton);
    if (likely(pfs != NULL))
    {
      if (likely(pfs->m_lock.is_populated()))
      {
        cumulated_stat.aggregate(& pfs->m_wait_stat);
      }
    }
  }
  else
  {
    /* For all the file instances ... */
    PFS_file *pfs= file_array;
    PFS_file *pfs_last= file_array + file_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if ((pfs->m_class == klass) && pfs->m_lock.is_populated())
      {
        /*
          If the instance belongs to this class,
          aggregate the instance statistics.
        */
        cumulated_stat.aggregate(& pfs->m_wait_stat);
      }
    }
  }

  /* FIXME: */
  /* For all the file handles ... */

  time_normalizer *normalizer= time_normalizer::get(wait_timer);
  m_row.m_stat.set(normalizer, &cumulated_stat);
  m_row_exists= true;
}

void table_ews_global_by_event_name
::make_table_io_row(PFS_instr_class *klass)
{
  m_row.m_name= klass->m_name;
  m_row.m_name_length= klass->m_name_length;
  uint index= klass->m_event_name_index;
  PFS_single_stat cumulated_stat= global_instr_class_waits_array[index];
  PFS_table_io_stat cumulated_io_stat;

  /* For all the table shares ... */
  PFS_table_share *share= table_share_array;
  PFS_table_share *share_last= table_share_array + table_share_max;
  for ( ; share < share_last; share++)
  {
    if (share->m_lock.is_populated())
    {
      /* Aggregate index stats */
      for (index= 0; index <= share->m_key_count; index++)
        cumulated_io_stat.aggregate(& share->m_table_stat.m_index_stat[index]);

      /* Aggregate global stats */
      cumulated_io_stat.aggregate(& share->m_table_stat.m_index_stat[MAX_KEY]);
    }
  }

  /* For all the table handles ... */
  PFS_table *table= table_array;
  PFS_table *table_last= table_array + table_max;
  for ( ; table < table_last; table++)
  {
    if (table->m_lock.is_populated())
    {
      PFS_table_share *safe_share= sanitize_table_share(table->m_share);

      if (likely(safe_share != NULL))
      {
        /* Aggregate index stats */
        for (index= 0; index <= safe_share->m_key_count; index++)
          cumulated_io_stat.aggregate(& table->m_table_stat.m_index_stat[index]);

        /* Aggregate global stats */
        cumulated_io_stat.aggregate(& table->m_table_stat.m_index_stat[MAX_KEY]);
      }
    }
  }

  cumulated_io_stat.sum(& cumulated_stat);

  time_normalizer *normalizer= time_normalizer::get(wait_timer);
  m_row.m_stat.set(normalizer, &cumulated_stat);
  m_row_exists= true;
}

void table_ews_global_by_event_name
::make_table_lock_row(PFS_instr_class *klass)
{
  m_row.m_name= klass->m_name;
  m_row.m_name_length= klass->m_name_length;
  uint index= klass->m_event_name_index;
  PFS_single_stat cumulated_stat= global_instr_class_waits_array[index];

  /* For all the table shares ... */
  PFS_table_share *share= table_share_array;
  PFS_table_share *share_last= table_share_array + table_share_max;
  for ( ; share < share_last; share++)
  {
    if (share->m_lock.is_populated())
    {
      /* Aggregate lock stats */
      share->m_table_stat.sum_lock(& cumulated_stat);
    }
  }

  /* For all the table handles ... */
  PFS_table *table= table_array;
  PFS_table *table_last= table_array + table_max;
  for ( ; table < table_last; table++)
  {
    if (table->m_lock.is_populated())
    {
      PFS_table_share *safe_share= sanitize_table_share(table->m_share);

      if (likely(safe_share != NULL))
      {
        /* Aggregate lock stats */
        table->m_table_stat.sum_lock(& cumulated_stat);
      }
    }
  }

  time_normalizer *normalizer= time_normalizer::get(wait_timer);
  m_row.m_stat.set(normalizer, &cumulated_stat);
  m_row_exists= true;
}

int table_ews_global_by_event_name
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
      case 1: /* COUNT */
        set_field_ulonglong(f, m_row.m_stat.m_count);
        break;
      case 2: /* SUM */
        set_field_ulonglong(f, m_row.m_stat.m_sum);
        break;
      case 3: /* MIN */
        set_field_ulonglong(f, m_row.m_stat.m_min);
        break;
      case 4: /* AVG */
        set_field_ulonglong(f, m_row.m_stat.m_avg);
        break;
      case 5: /* MAX */
        set_field_ulonglong(f, m_row.m_stat.m_max);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }

  return 0;
}

