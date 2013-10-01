/* Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_events_statements.cc
  Events statements data structures (implementation).
*/

#include "my_global.h"
#include "my_sys.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_account.h"
#include "pfs_host.h"
#include "pfs_user.h"
#include "pfs_events_statements.h"
#include "pfs_atomic.h"
#include "m_string.h"

ulong events_statements_history_long_size= 0;
/** Consumer flag for table EVENTS_STATEMENTS_CURRENT. */
bool flag_events_statements_current= false;
/** Consumer flag for table EVENTS_STATEMENTS_HISTORY. */
bool flag_events_statements_history= false;
/** Consumer flag for table EVENTS_STATEMENTS_HISTORY_LONG. */
bool flag_events_statements_history_long= false;

/** True if EVENTS_STATEMENTS_HISTORY_LONG circular buffer is full. */
bool events_statements_history_long_full= false;
/** Index in EVENTS_STATEMENTS_HISTORY_LONG circular buffer. */
volatile uint32 events_statements_history_long_index= 0;
/** EVENTS_STATEMENTS_HISTORY_LONG circular buffer. */
PFS_events_statements *events_statements_history_long_array= NULL;

/**
  Initialize table EVENTS_STATEMENTS_HISTORY_LONG.
  @param events_statements_history_long_sizing       table sizing
*/
int init_events_statements_history_long(uint events_statements_history_long_sizing)
{
  events_statements_history_long_size= events_statements_history_long_sizing;
  events_statements_history_long_full= false;
  PFS_atomic::store_u32(&events_statements_history_long_index, 0);

  if (events_statements_history_long_size == 0)
    return 0;

  events_statements_history_long_array=
    PFS_MALLOC_ARRAY(events_statements_history_long_size, PFS_events_statements,
                     MYF(MY_ZEROFILL));

  return (events_statements_history_long_array ? 0 : 1);
}

/** Cleanup table EVENTS_STATEMENTS_HISTORY_LONG. */
void cleanup_events_statements_history_long(void)
{
  pfs_free(events_statements_history_long_array);
  events_statements_history_long_array= NULL;
}

static inline void copy_events_statements(PFS_events_statements *dest,
                                      const PFS_events_statements *source)
{
  memcpy(dest, source, sizeof(PFS_events_statements));
}

/**
  Insert a statement record in table EVENTS_STATEMENTS_HISTORY.
  @param thread             thread that executed the wait
  @param statement          record to insert
*/
void insert_events_statements_history(PFS_thread *thread, PFS_events_statements *statement)
{
  if (unlikely(events_statements_history_per_thread == 0))
    return;

  DBUG_ASSERT(thread->m_statements_history != NULL);

  uint index= thread->m_statements_history_index;

  /*
    A concurrent thread executing TRUNCATE TABLE EVENTS_STATEMENTS_CURRENT
    could alter the data that this thread is inserting,
    causing a potential race condition.
    We are not testing for this and insert a possibly empty record,
    to make this thread (the writer) faster.
    This is ok, the readers of m_statements_history will filter this out.
  */
  copy_events_statements(&thread->m_statements_history[index], statement);

  index++;
  if (index >= events_statements_history_per_thread)
  {
    index= 0;
    thread->m_statements_history_full= true;
  }
  thread->m_statements_history_index= index;
}

/**
  Insert a statement record in table EVENTS_STATEMENTS_HISTORY_LONG.
  @param statement              record to insert
*/
void insert_events_statements_history_long(PFS_events_statements *statement)
{
  if (unlikely(events_statements_history_long_size == 0))
    return ;

  DBUG_ASSERT(events_statements_history_long_array != NULL);

  uint index= PFS_atomic::add_u32(&events_statements_history_long_index, 1);

  index= index % events_statements_history_long_size;
  if (index == 0)
    events_statements_history_long_full= true;

  /* See related comment in insert_events_statements_history. */
  copy_events_statements(&events_statements_history_long_array[index], statement);
}

/** Reset table EVENTS_STATEMENTS_CURRENT data. */
void reset_events_statements_current(void)
{
  PFS_thread *pfs_thread= thread_array;
  PFS_thread *pfs_thread_last= thread_array + thread_max;

  for ( ; pfs_thread < pfs_thread_last; pfs_thread++)
  {
    PFS_events_statements *pfs_stmt= & pfs_thread->m_statement_stack[0];
    PFS_events_statements *pfs_stmt_last= pfs_stmt + statement_stack_max;

    for ( ; pfs_stmt < pfs_stmt_last; pfs_stmt++)
      pfs_stmt->m_class= NULL;
  }
}

/** Reset table EVENTS_STATEMENTS_HISTORY data. */
void reset_events_statements_history(void)
{
  PFS_thread *pfs_thread= thread_array;
  PFS_thread *pfs_thread_last= thread_array + thread_max;

  for ( ; pfs_thread < pfs_thread_last; pfs_thread++)
  {
    PFS_events_statements *pfs= pfs_thread->m_statements_history;
    PFS_events_statements *pfs_last= pfs + events_statements_history_per_thread;

    pfs_thread->m_statements_history_index= 0;
    pfs_thread->m_statements_history_full= false;
    for ( ; pfs < pfs_last; pfs++)
      pfs->m_class= NULL;
  }
}

/** Reset table EVENTS_STATEMENTS_HISTORY_LONG data. */
void reset_events_statements_history_long(void)
{
  PFS_atomic::store_u32(&events_statements_history_long_index, 0);
  events_statements_history_long_full= false;

  PFS_events_statements *pfs= events_statements_history_long_array;
  PFS_events_statements *pfs_last= pfs + events_statements_history_long_size;
  for ( ; pfs < pfs_last; pfs++)
    pfs->m_class= NULL;
}

/** Reset table EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME data. */
void reset_events_statements_by_thread()
{
  PFS_thread *thread= thread_array;
  PFS_thread *thread_last= thread_array + thread_max;
  PFS_account *account;
  PFS_user *user;
  PFS_host *host;

  for ( ; thread < thread_last; thread++)
  {
    if (thread->m_lock.is_populated())
    {
      account= sanitize_account(thread->m_account);
      user= sanitize_user(thread->m_user);
      host= sanitize_host(thread->m_host);
      aggregate_thread_statements(thread, account, user, host);
    }
  }
}

/** Reset table EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME data. */
void reset_events_statements_by_account()
{
  PFS_account *pfs= account_array;
  PFS_account *pfs_last= account_array + account_max;
  PFS_user *user;
  PFS_host *host;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
    {
      user= sanitize_user(pfs->m_user);
      host= sanitize_host(pfs->m_host);
      pfs->aggregate_statements(user, host);
    }
  }
}

/** Reset table EVENTS_STATEMENTS_SUMMARY_BY_USER_BY_EVENT_NAME data. */
void reset_events_statements_by_user()
{
  PFS_user *pfs= user_array;
  PFS_user *pfs_last= user_array + user_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
      pfs->aggregate_statements();
  }
}

/** Reset table EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME data. */
void reset_events_statements_by_host()
{
  PFS_host *pfs= host_array;
  PFS_host *pfs_last= host_array + host_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
      pfs->aggregate_statements();
  }
}

/** Reset table EVENTS_STATEMENTS_GLOBAL_BY_EVENT_NAME data. */
void reset_events_statements_global()
{
  PFS_statement_stat *stat= global_instr_class_statements_array;
  PFS_statement_stat *stat_last= global_instr_class_statements_array + statement_class_max;

  for ( ; stat < stat_last; stat++)
    stat->reset();
}

