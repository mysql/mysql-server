/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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
#include "pfs_instr_class.h"
#include "pfs_user.h"
#include "pfs_host.h"
#include "pfs_account.h"

/**
  @file storage/perfschema/pfs_visitor.cc
  Visitors (implementation).
*/

/**
  @addtogroup Performance_schema_buffers
  @{
*/

/** Connection iterator */
void PFS_connection_iterator::visit_global(bool with_hosts, bool with_users,
                                           bool with_accounts, bool with_threads,
                                           PFS_connection_visitor *visitor)
{
  DBUG_ASSERT(visitor != NULL);

  visitor->visit_global();

  if (with_hosts)
  {
    PFS_host *pfs= host_array;
    PFS_host *pfs_last= pfs + host_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_populated())
        visitor->visit_host(pfs);
    }
  }

  if (with_users)
  {
    PFS_user *pfs= user_array;
    PFS_user *pfs_last= pfs + user_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_populated())
        visitor->visit_user(pfs);
    }
  }

  if (with_accounts)
  {
    PFS_account *pfs= account_array;
    PFS_account *pfs_last= pfs + account_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_populated())
        visitor->visit_account(pfs);
    }
  }

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

void PFS_connection_iterator::visit_host(PFS_host *host,
                                         bool with_accounts, bool with_threads,
                                         PFS_connection_visitor *visitor)
{
  DBUG_ASSERT(visitor != NULL);

  visitor->visit_host(host);

  if (with_accounts)
  {
    PFS_account *pfs= account_array;
    PFS_account *pfs_last= pfs + account_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if ((pfs->m_host == host) && pfs->m_lock.is_populated())
      {
        visitor->visit_account(pfs);
      }
    }
  }

  if (with_threads)
  {
    PFS_thread *pfs= thread_array;
    PFS_thread *pfs_last= pfs + thread_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_populated())
      {
        PFS_account *safe_account= sanitize_account(pfs->m_account);
        if ((safe_account != NULL) && (safe_account->m_host == host))
        {
          /*
            If the thread belongs to a known user@host that belongs to this host,
            process it.
          */
          visitor->visit_thread(pfs);
        }
        else if (pfs->m_host == host)
        {
          /*
            If the thread belongs to a 'lost' user@host that belong to this host,
            process it.
          */
          visitor->visit_thread(pfs);
        }
      }
    }
  }
}

void PFS_connection_iterator::visit_user(PFS_user *user,
                                         bool with_accounts, bool with_threads,
                                         PFS_connection_visitor *visitor)
{
  DBUG_ASSERT(visitor != NULL);

  visitor->visit_user(user);

  if (with_accounts)
  {
    PFS_account *pfs= account_array;
    PFS_account *pfs_last= pfs + account_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if ((pfs->m_user == user) && pfs->m_lock.is_populated())
      {
        visitor->visit_account(pfs);
      }
    }
  }

  if (with_threads)
  {
    PFS_thread *pfs= thread_array;
    PFS_thread *pfs_last= pfs + thread_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_populated())
      {
        PFS_account *safe_account= sanitize_account(pfs->m_account);
        if ((safe_account != NULL) && (safe_account->m_user == user))
        {
          /*
            If the thread belongs to a known user@host that belongs to this user,
            process it.
          */
          visitor->visit_thread(pfs);
        }
        else if (pfs->m_user == user)
        {
          /*
            If the thread belongs to a 'lost' user@host that belong to this user,
            process it.
          */
          visitor->visit_thread(pfs);
        }
      }
    }
  }
}

void PFS_connection_iterator::visit_account(PFS_account *account,
                                              bool with_threads,
                                              PFS_connection_visitor *visitor)
{
  DBUG_ASSERT(visitor != NULL);

  visitor->visit_account(account);

  if (with_threads)
  {
    PFS_thread *pfs= thread_array;
    PFS_thread *pfs_last= pfs + thread_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if ((pfs->m_account == account) && pfs->m_lock.is_populated())
      {
        visitor->visit_thread(pfs);
      }
    }
  }
}

void PFS_instance_iterator::visit_all(PFS_instance_visitor *visitor)
{
  visit_all_mutex(visitor);
  visit_all_rwlock(visitor);
  visit_all_cond(visitor);
  visit_all_file(visitor);
}

void PFS_instance_iterator::visit_all_mutex(PFS_instance_visitor *visitor)
{
  visit_all_mutex_classes(visitor);
  visit_all_mutex_instances(visitor);
}

void PFS_instance_iterator::visit_all_mutex_classes(PFS_instance_visitor *visitor)
{
  PFS_mutex_class *pfs= mutex_class_array;
  PFS_mutex_class *pfs_last= pfs + mutex_class_max;
  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_name_length != 0)
    {
      visitor->visit_mutex_class(pfs);
    }
  }
}

void PFS_instance_iterator::visit_all_mutex_instances(PFS_instance_visitor *visitor)
{
  PFS_mutex *pfs= mutex_array;
  PFS_mutex *pfs_last= pfs + mutex_max;
  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
    {
      visitor->visit_mutex(pfs);
    }
  }
}

void PFS_instance_iterator::visit_all_rwlock(PFS_instance_visitor *visitor)
{
  visit_all_rwlock_classes(visitor);
  visit_all_rwlock_instances(visitor);
}

void PFS_instance_iterator::visit_all_rwlock_classes(PFS_instance_visitor *visitor)
{
  PFS_rwlock_class *pfs= rwlock_class_array;
  PFS_rwlock_class *pfs_last= pfs + rwlock_class_max;
  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_name_length != 0)
    {
      visitor->visit_rwlock_class(pfs);
    }
  }
}

void PFS_instance_iterator::visit_all_rwlock_instances(PFS_instance_visitor *visitor)
{
  PFS_rwlock *pfs= rwlock_array;
  PFS_rwlock *pfs_last= pfs + rwlock_max;
  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
    {
      visitor->visit_rwlock(pfs);
    }
  }
}

void PFS_instance_iterator::visit_all_cond(PFS_instance_visitor *visitor)
{
  visit_all_cond_classes(visitor);
  visit_all_cond_instances(visitor);
}

void PFS_instance_iterator::visit_all_cond_classes(PFS_instance_visitor *visitor)
{
  PFS_cond_class *pfs= cond_class_array;
  PFS_cond_class *pfs_last= pfs + cond_class_max;
  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_name_length != 0)
    {
      visitor->visit_cond_class(pfs);
    }
  }
}

void PFS_instance_iterator::visit_all_cond_instances(PFS_instance_visitor *visitor)
{
  PFS_cond *pfs= cond_array;
  PFS_cond *pfs_last= pfs + cond_max;
  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
    {
      visitor->visit_cond(pfs);
    }
  }
}

void PFS_instance_iterator::visit_all_file(PFS_instance_visitor *visitor)
{
  visit_all_file_classes(visitor);
  visit_all_file_instances(visitor);
}

void PFS_instance_iterator::visit_all_file_classes(PFS_instance_visitor *visitor)
{
  PFS_file_class *pfs= file_class_array;
  PFS_file_class *pfs_last= pfs + file_class_max;
  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_name_length != 0)
    {
      visitor->visit_file_class(pfs);
    }
  }
}

void PFS_instance_iterator::visit_all_file_instances(PFS_instance_visitor *visitor)
{
  PFS_file *pfs= file_array;
  PFS_file *pfs_last= pfs + file_max;
  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
    {
      visitor->visit_file(pfs);
    }
  }
}

/** Instance iterator */

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

/** Socket instance iterator visting a socket class and all instances */

void PFS_instance_iterator::visit_socket_instances(PFS_socket_class *klass,
                                                   PFS_instance_visitor *visitor)
{
  DBUG_ASSERT(visitor != NULL);

  visitor->visit_socket_class(klass);

  if (klass->is_singleton())
  {
    PFS_socket *pfs= sanitize_socket(klass->m_singleton);
    if (likely(pfs != NULL))
    {
      if (likely(pfs->m_lock.is_populated()))
      {
        visitor->visit_socket(pfs);
      }
    }
  }
  else
  {
    PFS_socket *pfs= socket_array;
    PFS_socket *pfs_last= pfs + socket_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if ((pfs->m_class == klass) && pfs->m_lock.is_populated())
      {
        visitor->visit_socket(pfs);
      }
    }
  }
}

/** Socket instance iterator visting sockets owned by PFS_thread. */

void PFS_instance_iterator::visit_socket_instances(PFS_socket_class *klass,
                                                   PFS_instance_visitor *visitor,
                                                   PFS_thread *thread,
                                                   bool visit_class)
{
  DBUG_ASSERT(visitor != NULL);
  DBUG_ASSERT(thread != NULL);

  if (visit_class)
    visitor->visit_socket_class(klass);

  if (klass->is_singleton())
  {
    PFS_socket *pfs= sanitize_socket(klass->m_singleton);
    if (likely(pfs != NULL))
    {
      if (unlikely(pfs->m_thread_owner == thread))
        visitor->visit_socket(pfs);
    }
  }
  else
  {
    /* Get current socket stats from each socket instance owned by this thread */
    PFS_socket *pfs= socket_array;
    PFS_socket *pfs_last= pfs + socket_max;

    for ( ; pfs < pfs_last; pfs++)
    {
      if (unlikely((pfs->m_class == klass) &&
                   (pfs->m_thread_owner == thread)))
      {
        visitor->visit_socket(pfs);
      }
    }
  }
}

/** Generic instance iterator with PFS_thread as matching criteria */

void PFS_instance_iterator::visit_instances(PFS_instr_class *klass,
                                            PFS_instance_visitor *visitor,
                                            PFS_thread *thread,
                                            bool visit_class)
{
  DBUG_ASSERT(visitor != NULL);
  DBUG_ASSERT(klass != NULL);

  switch (klass->m_type)
  {
  case PFS_CLASS_SOCKET:
    {
    PFS_socket_class *socket_class= reinterpret_cast<PFS_socket_class*>(klass);
    PFS_instance_iterator::visit_socket_instances(socket_class, visitor,
                                                  thread, visit_class);
    }
    break;
  default:
    break;
  }
}

/** Object iterator */
void PFS_object_iterator::visit_all(PFS_object_visitor *visitor)
{
  visit_all_tables(visitor);
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

/** Connection wait visitor */

PFS_connection_wait_visitor
::PFS_connection_wait_visitor(PFS_instr_class *klass)
{
  m_index= klass->m_event_name_index;
}

PFS_connection_wait_visitor::~PFS_connection_wait_visitor()
{}

void PFS_connection_wait_visitor::visit_global()
{
  /*
    This visitor is used only for idle instruments.
    For waits, do not sum by connection but by instances,
    it is more efficient.
  */
  DBUG_ASSERT(m_index == global_idle_class.m_event_name_index);
  m_stat.aggregate(& global_idle_stat);
}

void PFS_connection_wait_visitor::visit_host(PFS_host *pfs)
{
  m_stat.aggregate(& pfs->m_instr_class_waits_stats[m_index]);
}

void PFS_connection_wait_visitor::visit_user(PFS_user *pfs)
{
  m_stat.aggregate(& pfs->m_instr_class_waits_stats[m_index]);
}

void PFS_connection_wait_visitor::visit_account(PFS_account *pfs)
{
  m_stat.aggregate(& pfs->m_instr_class_waits_stats[m_index]);
}

void PFS_connection_wait_visitor::visit_thread(PFS_thread *pfs)
{
  m_stat.aggregate(& pfs->m_instr_class_waits_stats[m_index]);
}

PFS_connection_all_wait_visitor
::PFS_connection_all_wait_visitor()
{}

PFS_connection_all_wait_visitor::~PFS_connection_all_wait_visitor()
{}

void PFS_connection_all_wait_visitor::visit_global()
{
  /* Sum by instances, not by connection */
  DBUG_ASSERT(false);
}

void PFS_connection_all_wait_visitor::visit_connection_slice(PFS_connection_slice *pfs)
{
  PFS_single_stat *stat= pfs->m_instr_class_waits_stats;
  PFS_single_stat *stat_last= stat + wait_class_max;
  for ( ; stat < stat_last; stat++)
  {
    m_stat.aggregate(stat);
  }
}

void PFS_connection_all_wait_visitor::visit_host(PFS_host *pfs)
{
  visit_connection_slice(pfs);
}

void PFS_connection_all_wait_visitor::visit_user(PFS_user *pfs)
{
  visit_connection_slice(pfs);
}

void PFS_connection_all_wait_visitor::visit_account(PFS_account *pfs)
{
  visit_connection_slice(pfs);
}

void PFS_connection_all_wait_visitor::visit_thread(PFS_thread *pfs)
{
  visit_connection_slice(pfs);
}

PFS_connection_stage_visitor::PFS_connection_stage_visitor(PFS_stage_class *klass)
{
  m_index= klass->m_event_name_index;
}

PFS_connection_stage_visitor::~PFS_connection_stage_visitor()
{}

void PFS_connection_stage_visitor::visit_global()
{
  m_stat.aggregate(& global_instr_class_stages_array[m_index]);
}

void PFS_connection_stage_visitor::visit_host(PFS_host *pfs)
{
  m_stat.aggregate(& pfs->m_instr_class_stages_stats[m_index]);
}

void PFS_connection_stage_visitor::visit_user(PFS_user *pfs)
{
  m_stat.aggregate(& pfs->m_instr_class_stages_stats[m_index]);
}

void PFS_connection_stage_visitor::visit_account(PFS_account *pfs)
{
  m_stat.aggregate(& pfs->m_instr_class_stages_stats[m_index]);
}

void PFS_connection_stage_visitor::visit_thread(PFS_thread *pfs)
{
  m_stat.aggregate(& pfs->m_instr_class_stages_stats[m_index]);
}

PFS_connection_statement_visitor
::PFS_connection_statement_visitor(PFS_statement_class *klass)
{
  m_index= klass->m_event_name_index;
}

PFS_connection_statement_visitor::~PFS_connection_statement_visitor()
{}

void PFS_connection_statement_visitor::visit_global()
{
  m_stat.aggregate(& global_instr_class_statements_array[m_index]);
}

void PFS_connection_statement_visitor::visit_host(PFS_host *pfs)
{
  m_stat.aggregate(& pfs->m_instr_class_statements_stats[m_index]);
}

void PFS_connection_statement_visitor::visit_user(PFS_user *pfs)
{
  m_stat.aggregate(& pfs->m_instr_class_statements_stats[m_index]);
}

void PFS_connection_statement_visitor::visit_account(PFS_account *pfs)
{
  m_stat.aggregate(& pfs->m_instr_class_statements_stats[m_index]);
}

void PFS_connection_statement_visitor::visit_thread(PFS_thread *pfs)
{
  m_stat.aggregate(& pfs->m_instr_class_statements_stats[m_index]);
}

/** Instance wait visitor */
PFS_connection_all_statement_visitor
::PFS_connection_all_statement_visitor()
{}

PFS_connection_all_statement_visitor::~PFS_connection_all_statement_visitor()
{}

void PFS_connection_all_statement_visitor::visit_global()
{
  PFS_statement_stat *stat= global_instr_class_statements_array;
  PFS_statement_stat *stat_last= stat + statement_class_max;
  for ( ; stat < stat_last; stat++)
  {
    m_stat.aggregate(stat);
  }
}

void PFS_connection_all_statement_visitor::visit_connection_slice(PFS_connection_slice *pfs)
{
  PFS_statement_stat *stat= pfs->m_instr_class_statements_stats;
  PFS_statement_stat *stat_last= stat + statement_class_max;
  for ( ; stat < stat_last; stat++)
  {
    m_stat.aggregate(stat);
  }
}

void PFS_connection_all_statement_visitor::visit_host(PFS_host *pfs)
{
  visit_connection_slice(pfs);
}

void PFS_connection_all_statement_visitor::visit_user(PFS_user *pfs)
{
  visit_connection_slice(pfs);
}

void PFS_connection_all_statement_visitor::visit_account(PFS_account *pfs)
{
  visit_connection_slice(pfs);
}

void PFS_connection_all_statement_visitor::visit_thread(PFS_thread *pfs)
{
  visit_connection_slice(pfs);
}

PFS_connection_stat_visitor::PFS_connection_stat_visitor()
{}

PFS_connection_stat_visitor::~PFS_connection_stat_visitor()
{}

void PFS_connection_stat_visitor::visit_global()
{}

void PFS_connection_stat_visitor::visit_host(PFS_host *pfs)
{
  m_stat.aggregate_disconnected(pfs->m_disconnected_count);
}

void PFS_connection_stat_visitor::visit_user(PFS_user *pfs)
{
  m_stat.aggregate_disconnected(pfs->m_disconnected_count);
}

void PFS_connection_stat_visitor::visit_account(PFS_account *pfs)
{
  m_stat.aggregate_disconnected(pfs->m_disconnected_count);
}

void PFS_connection_stat_visitor::visit_thread(PFS_thread *)
{
  m_stat.aggregate_active(1);
}

PFS_instance_wait_visitor::PFS_instance_wait_visitor()
{
}

PFS_instance_wait_visitor::~PFS_instance_wait_visitor()
{}

void PFS_instance_wait_visitor::visit_mutex_class(PFS_mutex_class *pfs)
{
  m_stat.aggregate(&pfs->m_mutex_stat.m_wait_stat);
}

void PFS_instance_wait_visitor::visit_rwlock_class(PFS_rwlock_class *pfs)
{
  m_stat.aggregate(&pfs->m_rwlock_stat.m_wait_stat);
}

void PFS_instance_wait_visitor::visit_cond_class(PFS_cond_class *pfs)
{
  m_stat.aggregate(&pfs->m_cond_stat.m_wait_stat);
}

void PFS_instance_wait_visitor::visit_file_class(PFS_file_class *pfs)
{
  pfs->m_file_stat.m_io_stat.sum_waits(&m_stat);
}

void PFS_instance_wait_visitor::visit_socket_class(PFS_socket_class *pfs)
{
  pfs->m_socket_stat.m_io_stat.sum_waits(&m_stat);
}

void PFS_instance_wait_visitor::visit_mutex(PFS_mutex *pfs)
{
  m_stat.aggregate(& pfs->m_mutex_stat.m_wait_stat);
}

void PFS_instance_wait_visitor::visit_rwlock(PFS_rwlock *pfs)
{
  m_stat.aggregate(& pfs->m_rwlock_stat.m_wait_stat);
}

void PFS_instance_wait_visitor::visit_cond(PFS_cond *pfs)
{
  m_stat.aggregate(& pfs->m_cond_stat.m_wait_stat);
}

void PFS_instance_wait_visitor::visit_file(PFS_file *pfs) 
{
  /* Combine per-operation file wait stats before aggregating */
  PFS_single_stat stat;
  pfs->m_file_stat.m_io_stat.sum_waits(&stat);
  m_stat.aggregate(&stat);
}

void PFS_instance_wait_visitor::visit_socket(PFS_socket *pfs) 
{
  /* Combine per-operation socket wait stats before aggregating */
  PFS_single_stat stat;
  pfs->m_socket_stat.m_io_stat.sum_waits(&stat);
  m_stat.aggregate(&stat);
}

/** Table IO wait visitor */

PFS_object_wait_visitor::PFS_object_wait_visitor()
{}

PFS_object_wait_visitor::~PFS_object_wait_visitor()
{}

void PFS_object_wait_visitor::visit_global()
{
  global_table_io_stat.sum(& m_stat);
  global_table_lock_stat.sum(& m_stat);
}

void PFS_object_wait_visitor::visit_table_share(PFS_table_share *pfs)
{
  uint safe_key_count= sanitize_index_count(pfs->m_key_count);
  pfs->m_table_stat.sum(& m_stat, safe_key_count);
}

void PFS_object_wait_visitor::visit_table(PFS_table *pfs)
{
  PFS_table_share *table_share= sanitize_table_share(pfs->m_share);
  if (table_share != NULL)
  {
    uint safe_key_count= sanitize_index_count(table_share->m_key_count);
    pfs->m_table_stat.sum(& m_stat, safe_key_count);
  }
}

PFS_table_io_wait_visitor::PFS_table_io_wait_visitor()
{}

PFS_table_io_wait_visitor::~PFS_table_io_wait_visitor()
{}

void PFS_table_io_wait_visitor::visit_global()
{
  global_table_io_stat.sum(& m_stat);
}

void PFS_table_io_wait_visitor::visit_table_share(PFS_table_share *pfs)
{
  PFS_table_io_stat io_stat;
  uint safe_key_count= sanitize_index_count(pfs->m_key_count);
  uint index;

  /* Aggregate index stats */
  for (index= 0; index < safe_key_count; index++)
    io_stat.aggregate(& pfs->m_table_stat.m_index_stat[index]);

  /* Aggregate global stats */
  io_stat.aggregate(& pfs->m_table_stat.m_index_stat[MAX_INDEXES]);

  io_stat.sum(& m_stat);
}

void PFS_table_io_wait_visitor::visit_table(PFS_table *pfs)
{
  PFS_table_share *safe_share= sanitize_table_share(pfs->m_share);

  if (likely(safe_share != NULL))
  {
    PFS_table_io_stat io_stat;
    uint safe_key_count= sanitize_index_count(safe_share->m_key_count);
    uint index;

    /* Aggregate index stats */
    for (index= 0; index < safe_key_count; index++)
      io_stat.aggregate(& pfs->m_table_stat.m_index_stat[index]);

    /* Aggregate global stats */
    io_stat.aggregate(& pfs->m_table_stat.m_index_stat[MAX_INDEXES]);

    io_stat.sum(& m_stat);
  }
}

/** Table IO stat visitor */

PFS_table_io_stat_visitor::PFS_table_io_stat_visitor()
{}

PFS_table_io_stat_visitor::~PFS_table_io_stat_visitor()
{}

void PFS_table_io_stat_visitor::visit_table_share(PFS_table_share *pfs)
{
  uint safe_key_count= sanitize_index_count(pfs->m_key_count);
  uint index;

  /* Aggregate index stats */
  for (index= 0; index < safe_key_count; index++)
    m_stat.aggregate(& pfs->m_table_stat.m_index_stat[index]);

  /* Aggregate global stats */
  m_stat.aggregate(& pfs->m_table_stat.m_index_stat[MAX_INDEXES]);
}

void PFS_table_io_stat_visitor::visit_table(PFS_table *pfs)
{
  PFS_table_share *safe_share= sanitize_table_share(pfs->m_share);

  if (likely(safe_share != NULL))
  {
    uint safe_key_count= sanitize_index_count(safe_share->m_key_count);
    uint index;

    /* Aggregate index stats */
    for (index= 0; index < safe_key_count; index++)
      m_stat.aggregate(& pfs->m_table_stat.m_index_stat[index]);

    /* Aggregate global stats */
    m_stat.aggregate(& pfs->m_table_stat.m_index_stat[MAX_INDEXES]);
  }
}

/** Index IO stat visitor */

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

/** Table lock wait visitor */

PFS_table_lock_wait_visitor::PFS_table_lock_wait_visitor()
{}

PFS_table_lock_wait_visitor::~PFS_table_lock_wait_visitor()
{}

void PFS_table_lock_wait_visitor::visit_global()
{
  global_table_lock_stat.sum(& m_stat);
}

void PFS_table_lock_wait_visitor::visit_table_share(PFS_table_share *pfs)
{
  pfs->m_table_stat.sum_lock(& m_stat);
}

void PFS_table_lock_wait_visitor::visit_table(PFS_table *pfs)
{
  pfs->m_table_stat.sum_lock(& m_stat);
}

/** Table lock stat visitor */

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

PFS_instance_socket_io_stat_visitor::PFS_instance_socket_io_stat_visitor()
{}

PFS_instance_socket_io_stat_visitor::~PFS_instance_socket_io_stat_visitor()
{}

void PFS_instance_socket_io_stat_visitor::visit_socket_class(PFS_socket_class *pfs) 
{
  /* Aggregate wait times, event counts and byte counts */
  m_socket_io_stat.aggregate(&pfs->m_socket_stat.m_io_stat);
}

void PFS_instance_socket_io_stat_visitor::visit_socket(PFS_socket *pfs) 
{
  /* Aggregate wait times, event counts and byte counts */
  m_socket_io_stat.aggregate(&pfs->m_socket_stat.m_io_stat);
}


PFS_instance_file_io_stat_visitor::PFS_instance_file_io_stat_visitor()
{}

PFS_instance_file_io_stat_visitor::~PFS_instance_file_io_stat_visitor()
{}

void PFS_instance_file_io_stat_visitor::visit_file_class(PFS_file_class *pfs) 
{
  /* Aggregate wait times, event counts and byte counts */
  m_file_io_stat.aggregate(&pfs->m_file_stat.m_io_stat);
}

void PFS_instance_file_io_stat_visitor::visit_file(PFS_file *pfs) 
{
  /* Aggregate wait times, event counts and byte counts */
  m_file_io_stat.aggregate(&pfs->m_file_stat.m_io_stat);
}
/** @} */
