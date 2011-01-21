/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "my_sys.h"
#include "pfs_visitor.h"
#include "pfs_instr.h"

/**
  @file storage/perfschema/pfs_visitor.cc
  Visitors (implementation).
*/

/**
  @addtogroup Performance_schema_buffers
  @{
*/

void PFS_connection_iterator::visit_global(bool with_threads,
                                           PFS_connection_visitor *visitor)
{
  DBUG_ASSERT(visitor != NULL);

  visitor->visit_global();

  if (with_threads)
  {
    PFS_thread *pfs= thread_array;
    PFS_thread *pfs_last= pfs + thread_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_populated())
        visitor->visit_thread(pfs);
    }
  }
}

void PFS_instance_iterator::visit_mutex_instances(PFS_mutex_class *klass,
                                                  PFS_instance_visitor *visitor)
{
  DBUG_ASSERT(visitor != NULL);

  visitor->visit_mutex_class(klass);

  if (klass->is_singleton())
  {
    PFS_mutex *pfs= sanitize_mutex(klass->m_singleton);
    if (likely(pfs != NULL))
    {
      if (likely(pfs->m_lock.is_populated()))
      {
        visitor->visit_mutex(pfs);
      }
    }
  }
  else
  {
    PFS_mutex *pfs= mutex_array;
    PFS_mutex *pfs_last= pfs + mutex_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if ((pfs->m_class == klass) && pfs->m_lock.is_populated())
      {
        visitor->visit_mutex(pfs);
      }
    }
  }
}

void PFS_instance_iterator::visit_rwlock_instances(PFS_rwlock_class *klass,
                                                   PFS_instance_visitor *visitor)
{
  DBUG_ASSERT(visitor != NULL);

  visitor->visit_rwlock_class(klass);

  if (klass->is_singleton())
  {
    PFS_rwlock *pfs= sanitize_rwlock(klass->m_singleton);
    if (likely(pfs != NULL))
    {
      if (likely(pfs->m_lock.is_populated()))
      {
        visitor->visit_rwlock(pfs);
      }
    }
  }
  else
  {
    PFS_rwlock *pfs= rwlock_array;
    PFS_rwlock *pfs_last= pfs + rwlock_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if ((pfs->m_class == klass) && pfs->m_lock.is_populated())
      {
        visitor->visit_rwlock(pfs);
      }
    }
  }
}

void PFS_instance_iterator::visit_cond_instances(PFS_cond_class *klass,
                                                 PFS_instance_visitor *visitor)
{
  DBUG_ASSERT(visitor != NULL);

  visitor->visit_cond_class(klass);

  if (klass->is_singleton())
  {
    PFS_cond *pfs= sanitize_cond(klass->m_singleton);
    if (likely(pfs != NULL))
    {
      if (likely(pfs->m_lock.is_populated()))
      {
        visitor->visit_cond(pfs);
      }
    }
  }
  else
  {
    PFS_cond *pfs= cond_array;
    PFS_cond *pfs_last= pfs + cond_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if ((pfs->m_class == klass) && pfs->m_lock.is_populated())
      {
        visitor->visit_cond(pfs);
      }
    }
  }
}

void PFS_instance_iterator::visit_file_instances(PFS_file_class *klass,
                                                 PFS_instance_visitor *visitor)
{
  DBUG_ASSERT(visitor != NULL);

  visitor->visit_file_class(klass);

  if (klass->is_singleton())
  {
    PFS_file *pfs= sanitize_file(klass->m_singleton);
    if (likely(pfs != NULL))
    {
      if (likely(pfs->m_lock.is_populated()))
      {
        visitor->visit_file(pfs);
      }
    }
  }
  else
  {
    PFS_file *pfs= file_array;
    PFS_file *pfs_last= pfs + file_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if ((pfs->m_class == klass) && pfs->m_lock.is_populated())
      {
        visitor->visit_file(pfs);
      }
    }
  }
}

void PFS_object_iterator::visit_all_tables(PFS_object_visitor *visitor)
{
  DBUG_ASSERT(visitor != NULL);

  visitor->visit_global();

  /* For all the table shares ... */
  PFS_table_share *share= table_share_array;
  PFS_table_share *share_last= table_share_array + table_share_max;
  for ( ; share < share_last; share++)
  {
    if (share->m_lock.is_populated())
    {
      visitor->visit_table_share(share);
    }
  }

  /* For all the table handles ... */
  PFS_table *table= table_array;
  PFS_table *table_last= table_array + table_max;
  for ( ; table < table_last; table++)
  {
    if (table->m_lock.is_populated())
    {
      visitor->visit_table(table);
    }
  }
}

void PFS_object_iterator::visit_tables(PFS_table_share *share,
                                       PFS_object_visitor *visitor)
{
  DBUG_ASSERT(visitor != NULL);

  visitor->visit_table_share(share);

  /* For all the table handles ... */
  PFS_table *table= table_array;
  PFS_table *table_last= table_array + table_max;
  for ( ; table < table_last; table++)
  {
    if ((table->m_share == share) && table->m_lock.is_populated())
    {
      visitor->visit_table(table);
    }
  }
}

void PFS_object_iterator::visit_table_indexes(PFS_table_share *share,
                                              uint index,
                                              PFS_object_visitor *visitor)
{
  DBUG_ASSERT(visitor != NULL);

  visitor->visit_table_share_index(share, index);

  /* For all the table handles ... */
  PFS_table *table= table_array;
  PFS_table *table_last= table_array + table_max;
  for ( ; table < table_last; table++)
  {
    if ((table->m_share == share) && table->m_lock.is_populated())
    {
      visitor->visit_table_index(table, index);
    }
  }
}

PFS_connection_wait_visitor
::PFS_connection_wait_visitor(PFS_instr_class *klass)
{
  m_index= klass->m_event_name_index;
}

PFS_connection_wait_visitor::~PFS_connection_wait_visitor()
{}

void PFS_connection_wait_visitor::visit_global()
{
  /* Sum by instances, not by connection */
  DBUG_ASSERT(false);
}

void PFS_connection_wait_visitor::visit_thread(PFS_thread *pfs)
{
  m_stat.aggregate(& pfs->m_instr_class_wait_stats[m_index]);
}

PFS_instance_wait_visitor::PFS_instance_wait_visitor()
{
}

PFS_instance_wait_visitor::~PFS_instance_wait_visitor()
{}

void PFS_instance_wait_visitor::visit_mutex_class(PFS_mutex_class *pfs) 
{
  uint index= pfs->m_event_name_index;
  m_stat.aggregate(& global_instr_class_waits_array[index]);
}

void PFS_instance_wait_visitor::visit_rwlock_class(PFS_rwlock_class *pfs) 
{
  uint index= pfs->m_event_name_index;
  m_stat.aggregate(& global_instr_class_waits_array[index]);
}

void PFS_instance_wait_visitor::visit_cond_class(PFS_cond_class *pfs) 
{
  uint index= pfs->m_event_name_index;
  m_stat.aggregate(& global_instr_class_waits_array[index]);
}

void PFS_instance_wait_visitor::visit_file_class(PFS_file_class *pfs) 
{
  uint index= pfs->m_event_name_index;
  m_stat.aggregate(& global_instr_class_waits_array[index]);
}

void PFS_instance_wait_visitor::visit_mutex(PFS_mutex *pfs) 
{
  m_stat.aggregate(& pfs->m_wait_stat);
}

void PFS_instance_wait_visitor::visit_rwlock(PFS_rwlock *pfs) 
{
  m_stat.aggregate(& pfs->m_wait_stat);
}

void PFS_instance_wait_visitor::visit_cond(PFS_cond *pfs) 
{
  m_stat.aggregate(& pfs->m_wait_stat);
}

void PFS_instance_wait_visitor::visit_file(PFS_file *pfs) 
{
  m_stat.aggregate(& pfs->m_wait_stat);
}

PFS_table_io_wait_visitor::PFS_table_io_wait_visitor()
{}

PFS_table_io_wait_visitor::~PFS_table_io_wait_visitor()
{}

void PFS_table_io_wait_visitor::visit_global()
{
  uint index= global_table_io_class.m_event_name_index;
  m_stat.aggregate(& global_instr_class_waits_array[index]);
}

void PFS_table_io_wait_visitor::visit_table_share(PFS_table_share *pfs)
{
  PFS_table_io_stat io_stat;
  uint index;

  /* Aggregate index stats */
  for (index= 0; index < pfs->m_key_count; index++)
    io_stat.aggregate(& pfs->m_table_stat.m_index_stat[index]);

  /* Aggregate global stats */
  io_stat.aggregate(& pfs->m_table_stat.m_index_stat[MAX_KEY]);

  io_stat.sum(& m_stat);
}

void PFS_table_io_wait_visitor::visit_table(PFS_table *pfs)
{
  PFS_table_share *safe_share= sanitize_table_share(pfs->m_share);

  if (likely(safe_share != NULL))
  {
    PFS_table_io_stat io_stat;
    uint index;

    /* Aggregate index stats */
    for (index= 0; index < safe_share->m_key_count; index++)
      io_stat.aggregate(& pfs->m_table_stat.m_index_stat[index]);

    /* Aggregate global stats */
    io_stat.aggregate(& pfs->m_table_stat.m_index_stat[MAX_KEY]);

    io_stat.sum(& m_stat);
  }
}

PFS_table_io_stat_visitor::PFS_table_io_stat_visitor()
{}

PFS_table_io_stat_visitor::~PFS_table_io_stat_visitor()
{}

void PFS_table_io_stat_visitor::visit_table_share(PFS_table_share *pfs)
{
  uint index;

  /* Aggregate index stats */
  for (index= 0; index < pfs->m_key_count; index++)
    m_stat.aggregate(& pfs->m_table_stat.m_index_stat[index]);

  /* Aggregate global stats */
  m_stat.aggregate(& pfs->m_table_stat.m_index_stat[MAX_KEY]);
}

void PFS_table_io_stat_visitor::visit_table(PFS_table *pfs)
{
  PFS_table_share *safe_share= sanitize_table_share(pfs->m_share);

  if (likely(safe_share != NULL))
  {
    uint index;

    /* Aggregate index stats */
    for (index= 0; index < safe_share->m_key_count; index++)
      m_stat.aggregate(& pfs->m_table_stat.m_index_stat[index]);

    /* Aggregate global stats */
    m_stat.aggregate(& pfs->m_table_stat.m_index_stat[MAX_KEY]);
  }
}

PFS_index_io_stat_visitor::PFS_index_io_stat_visitor()
{}

PFS_index_io_stat_visitor::~PFS_index_io_stat_visitor()
{}

void PFS_index_io_stat_visitor::visit_table_share_index(PFS_table_share *pfs, uint index)
{
  m_stat.aggregate(& pfs->m_table_stat.m_index_stat[index]);
}

void PFS_index_io_stat_visitor::visit_table_index(PFS_table *pfs, uint index)
{
  m_stat.aggregate(& pfs->m_table_stat.m_index_stat[index]);
}

PFS_table_lock_wait_visitor::PFS_table_lock_wait_visitor()
{}

PFS_table_lock_wait_visitor::~PFS_table_lock_wait_visitor()
{}

void PFS_table_lock_wait_visitor::visit_global()
{
  uint index= global_table_lock_class.m_event_name_index;
  m_stat.aggregate(& global_instr_class_waits_array[index]);
}

void PFS_table_lock_wait_visitor::visit_table_share(PFS_table_share *pfs)
{
  pfs->m_table_stat.sum_lock(& m_stat);
}

void PFS_table_lock_wait_visitor::visit_table(PFS_table *pfs)
{
  pfs->m_table_stat.sum_lock(& m_stat);
}

PFS_table_lock_stat_visitor::PFS_table_lock_stat_visitor()
{}

PFS_table_lock_stat_visitor::~PFS_table_lock_stat_visitor()
{}

void PFS_table_lock_stat_visitor::visit_table_share(PFS_table_share *pfs)
{
  m_stat.aggregate(& pfs->m_table_stat.m_lock_stat);
}

void PFS_table_lock_stat_visitor::visit_table(PFS_table *pfs)
{
  m_stat.aggregate(& pfs->m_table_stat.m_lock_stat);
}

/** @} */

