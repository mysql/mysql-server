/* Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_events_waits.cc
  Events waits data structures (implementation).
*/

#include "my_global.h"
#include "my_sys.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_user.h"
#include "pfs_host.h"
#include "pfs_account.h"
#include "pfs_events_waits.h"
#include "pfs_atomic.h"
#include "m_string.h"

ulong events_waits_history_long_size= 0;
/** Consumer flag for table EVENTS_WAITS_CURRENT. */
bool flag_events_waits_current= false;
/** Consumer flag for table EVENTS_WAITS_HISTORY. */
bool flag_events_waits_history= false;
/** Consumer flag for table EVENTS_WAITS_HISTORY_LONG. */
bool flag_events_waits_history_long= false;
/** Consumer flag for the global instrumentation. */
bool flag_global_instrumentation= false;
/** Consumer flag for the per thread instrumentation. */
bool flag_thread_instrumentation= false;

/** True if EVENTS_WAITS_HISTORY_LONG circular buffer is full. */
bool events_waits_history_long_full= false;
/** Index in EVENTS_WAITS_HISTORY_LONG circular buffer. */
volatile uint32 events_waits_history_long_index= 0;
/** EVENTS_WAITS_HISTORY_LONG circular buffer. */
PFS_events_waits *events_waits_history_long_array= NULL;

/**
  Initialize table EVENTS_WAITS_HISTORY_LONG.
  @param events_waits_history_long_sizing       table sizing
*/
int init_events_waits_history_long(uint events_waits_history_long_sizing)
{
  events_waits_history_long_size= events_waits_history_long_sizing;
  events_waits_history_long_full= false;
  PFS_atomic::store_u32(&events_waits_history_long_index, 0);

  if (events_waits_history_long_size == 0)
    return 0;

  events_waits_history_long_array=
    PFS_MALLOC_ARRAY(events_waits_history_long_size, PFS_events_waits,
                     MYF(MY_ZEROFILL));

  return (events_waits_history_long_array ? 0 : 1);
}

/** Cleanup table EVENTS_WAITS_HISTORY_LONG. */
void cleanup_events_waits_history_long(void)
{
  pfs_free(events_waits_history_long_array);
  events_waits_history_long_array= NULL;
}

static inline void copy_events_waits(PFS_events_waits *dest,
                                     const PFS_events_waits *source)
{
  memcpy(dest, source, sizeof(PFS_events_waits));
}

/**
  Insert a wait record in table EVENTS_WAITS_HISTORY.
  @param thread             thread that executed the wait
  @param wait               record to insert
*/
void insert_events_waits_history(PFS_thread *thread, PFS_events_waits *wait)
{
  if (unlikely(events_waits_history_per_thread == 0))
    return;

  uint index= thread->m_waits_history_index;

  /*
    A concurrent thread executing TRUNCATE TABLE EVENTS_WAITS_CURRENT
    could alter the data that this thread is inserting,
    causing a potential race condition.
    We are not testing for this and insert a possibly empty record,
    to make this thread (the writer) faster.
    This is ok, the readers of m_waits_history will filter this out.
  */
  copy_events_waits(&thread->m_waits_history[index], wait);

  index++;
  if (index >= events_waits_history_per_thread)
  {
    index= 0;
    thread->m_waits_history_full= true;
  }
  thread->m_waits_history_index= index;
}

/**
  Insert a wait record in table EVENTS_WAITS_HISTORY_LONG.
  @param wait               record to insert
*/
void insert_events_waits_history_long(PFS_events_waits *wait)
{
  if (unlikely(events_waits_history_long_size == 0))
    return;

  uint index= PFS_atomic::add_u32(&events_waits_history_long_index, 1);

  index= index % events_waits_history_long_size;
  if (index == 0)
    events_waits_history_long_full= true;

  /* See related comment in insert_events_waits_history. */
  copy_events_waits(&events_waits_history_long_array[index], wait);
}

/** Reset table EVENTS_WAITS_CURRENT data. */
void reset_events_waits_current(void)
{
  PFS_thread *pfs_thread= thread_array;
  PFS_thread *pfs_thread_last= thread_array + thread_max;

  for ( ; pfs_thread < pfs_thread_last; pfs_thread++)
  {
    PFS_events_waits *pfs_wait= pfs_thread->m_events_waits_stack;
    PFS_events_waits *pfs_wait_last= pfs_wait + WAIT_STACK_SIZE;

    for ( ; pfs_wait < pfs_wait_last; pfs_wait++)
      pfs_wait->m_wait_class= NO_WAIT_CLASS;
  }
}

/** Reset table EVENTS_WAITS_HISTORY data. */
void reset_events_waits_history(void)
{
  PFS_thread *pfs_thread= thread_array;
  PFS_thread *pfs_thread_last= thread_array + thread_max;

  for ( ; pfs_thread < pfs_thread_last; pfs_thread++)
  {
    PFS_events_waits *wait= pfs_thread->m_waits_history;
    PFS_events_waits *wait_last= wait + events_waits_history_per_thread;

    pfs_thread->m_waits_history_index= 0;
    pfs_thread->m_waits_history_full= false;
    for ( ; wait < wait_last; wait++)
      wait->m_wait_class= NO_WAIT_CLASS;
  }
}

/** Reset table EVENTS_WAITS_HISTORY_LONG data. */
void reset_events_waits_history_long(void)
{
  PFS_atomic::store_u32(&events_waits_history_long_index, 0);
  events_waits_history_long_full= false;

  PFS_events_waits *wait= events_waits_history_long_array;
  PFS_events_waits *wait_last= wait + events_waits_history_long_size;
  for ( ; wait < wait_last; wait++)
    wait->m_wait_class= NO_WAIT_CLASS;
}

/** Reset table EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME data. */
void reset_events_waits_by_thread()
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
      aggregate_thread_waits(thread, account, user, host);
    }
  }
}

/** Reset table EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME data. */
void reset_events_waits_by_account()
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
      pfs->aggregate_waits(user, host);
    }
  }
}

/** Reset table EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME data. */
void reset_events_waits_by_user()
{
  PFS_user *pfs= user_array;
  PFS_user *pfs_last= user_array + user_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
      pfs->aggregate_waits();
  }
}

/** Reset table EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME data. */
void reset_events_waits_by_host()
{
  PFS_host *pfs= host_array;
  PFS_host *pfs_last= host_array + host_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
      pfs->aggregate_waits();
  }
}

void reset_table_waits_by_table()
{
  PFS_table_share *pfs= table_share_array;
  PFS_table_share *pfs_last= pfs + table_share_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
      pfs->aggregate();
  }
}

void reset_table_io_waits_by_table()
{
  PFS_table_share *pfs= table_share_array;
  PFS_table_share *pfs_last= pfs + table_share_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
      pfs->aggregate_io();
  }
}

void reset_table_lock_waits_by_table()
{
  PFS_table_share *pfs= table_share_array;
  PFS_table_share *pfs_last= pfs + table_share_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
      pfs->aggregate_lock();
  }
}

void reset_table_waits_by_table_handle()
{
  PFS_table *pfs= table_array;
  PFS_table *pfs_last= pfs + table_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
      pfs->sanitized_aggregate();
  }
}

void reset_table_io_waits_by_table_handle()
{
  PFS_table *pfs= table_array;
  PFS_table *pfs_last= pfs + table_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
      pfs->sanitized_aggregate_io();
  }
}

void reset_table_lock_waits_by_table_handle()
{
  PFS_table *pfs= table_array;
  PFS_table *pfs_last= pfs + table_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
      pfs->sanitized_aggregate_lock();
  }
}

