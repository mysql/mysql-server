/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/pfs_events_waits.cc
  Events waits data structures (implementation).
*/

#include "storage/perfschema/pfs_events_waits.h"

#include <atomic>

#include "m_string.h"
#include "my_compiler.h"
#include "my_sys.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_builtin_memory.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_user.h"

PFS_ALIGNED ulong events_waits_history_long_size = 0;
/** Consumer flag for table EVENTS_WAITS_CURRENT. */
PFS_ALIGNED bool flag_events_waits_current = false;
/** Consumer flag for table EVENTS_WAITS_HISTORY. */
PFS_ALIGNED bool flag_events_waits_history = false;
/** Consumer flag for table EVENTS_WAITS_HISTORY_LONG. */
PFS_ALIGNED bool flag_events_waits_history_long = false;
/** Consumer flag for the global instrumentation. */
PFS_ALIGNED bool flag_global_instrumentation = false;
/** Consumer flag for the per thread instrumentation. */
PFS_ALIGNED bool flag_thread_instrumentation = false;

/** True if EVENTS_WAITS_HISTORY_LONG circular buffer is full. */
PFS_ALIGNED bool events_waits_history_long_full = false;
/** Index in EVENTS_WAITS_HISTORY_LONG circular buffer. */
PFS_ALIGNED PFS_cacheline_atomic_uint32 events_waits_history_long_index;
/** EVENTS_WAITS_HISTORY_LONG circular buffer. */
PFS_ALIGNED PFS_events_waits *events_waits_history_long_array = NULL;

/**
  Initialize table EVENTS_WAITS_HISTORY_LONG.
  @param events_waits_history_long_sizing       table sizing
*/
int
init_events_waits_history_long(uint events_waits_history_long_sizing)
{
  events_waits_history_long_size = events_waits_history_long_sizing;
  events_waits_history_long_full = false;
  events_waits_history_long_index.m_u32.store(0);

  if (events_waits_history_long_size == 0)
  {
    return 0;
  }

  events_waits_history_long_array =
    PFS_MALLOC_ARRAY(&builtin_memory_waits_history_long,
                     events_waits_history_long_size,
                     sizeof(PFS_events_waits),
                     PFS_events_waits,
                     MYF(MY_ZEROFILL));

  return (events_waits_history_long_array ? 0 : 1);
}

/** Cleanup table EVENTS_WAITS_HISTORY_LONG. */
void
cleanup_events_waits_history_long(void)
{
  PFS_FREE_ARRAY(&builtin_memory_waits_history_long,
                 events_waits_history_long_size,
                 sizeof(PFS_events_waits),
                 events_waits_history_long_array);
  events_waits_history_long_array = NULL;
}

static inline void
copy_events_waits(PFS_events_waits *dest, const PFS_events_waits *source)
{
  memcpy(dest, source, sizeof(PFS_events_waits));
}

/**
  Insert a wait record in table EVENTS_WAITS_HISTORY.
  @param thread             thread that executed the wait
  @param wait               record to insert
*/
void
insert_events_waits_history(PFS_thread *thread, PFS_events_waits *wait)
{
  if (unlikely(events_waits_history_per_thread == 0))
  {
    return;
  }

  uint index = thread->m_waits_history_index;

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
    index = 0;
    thread->m_waits_history_full = true;
  }
  thread->m_waits_history_index = index;
}

/**
  Insert a wait record in table EVENTS_WAITS_HISTORY_LONG.
  @param wait               record to insert
*/
void
insert_events_waits_history_long(PFS_events_waits *wait)
{
  if (unlikely(events_waits_history_long_size == 0))
  {
    return;
  }

  uint index = events_waits_history_long_index.m_u32++;

  index = index % events_waits_history_long_size;
  if (index == 0)
  {
    events_waits_history_long_full = true;
  }

  /* See related comment in insert_events_waits_history. */
  copy_events_waits(&events_waits_history_long_array[index], wait);
}

static void
fct_reset_events_waits_current(PFS_thread *pfs_thread)
{
  PFS_events_waits *pfs_wait = pfs_thread->m_events_waits_stack;
  PFS_events_waits *pfs_wait_last = pfs_wait + WAIT_STACK_SIZE;

  for (; pfs_wait < pfs_wait_last; pfs_wait++)
  {
    pfs_wait->m_wait_class = NO_WAIT_CLASS;
  }
}

/** Reset table EVENTS_WAITS_CURRENT data. */
void
reset_events_waits_current(void)
{
  global_thread_container.apply_all(fct_reset_events_waits_current);
}

static void
fct_reset_events_waits_history(PFS_thread *pfs_thread)
{
  PFS_events_waits *wait = pfs_thread->m_waits_history;
  PFS_events_waits *wait_last = wait + events_waits_history_per_thread;

  pfs_thread->m_waits_history_index = 0;
  pfs_thread->m_waits_history_full = false;
  for (; wait < wait_last; wait++)
  {
    wait->m_wait_class = NO_WAIT_CLASS;
  }
}

/** Reset table EVENTS_WAITS_HISTORY data. */
void
reset_events_waits_history(void)
{
  global_thread_container.apply_all(fct_reset_events_waits_history);
}

/** Reset table EVENTS_WAITS_HISTORY_LONG data. */
void
reset_events_waits_history_long(void)
{
  events_waits_history_long_index.m_u32.store(0);
  events_waits_history_long_full = false;

  PFS_events_waits *wait = events_waits_history_long_array;
  PFS_events_waits *wait_last = wait + events_waits_history_long_size;
  for (; wait < wait_last; wait++)
  {
    wait->m_wait_class = NO_WAIT_CLASS;
  }
}

static void
fct_reset_events_waits_by_thread(PFS_thread *thread)
{
  PFS_account *account = sanitize_account(thread->m_account);
  PFS_user *user = sanitize_user(thread->m_user);
  PFS_host *host = sanitize_host(thread->m_host);
  aggregate_thread_waits(thread, account, user, host);
}

/** Reset table EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME data. */
void
reset_events_waits_by_thread()
{
  global_thread_container.apply(fct_reset_events_waits_by_thread);
}

static void
fct_reset_events_waits_by_account(PFS_account *pfs)
{
  PFS_user *user = sanitize_user(pfs->m_user);
  PFS_host *host = sanitize_host(pfs->m_host);
  pfs->aggregate_waits(user, host);
}

/** Reset table EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME data. */
void
reset_events_waits_by_account()
{
  global_account_container.apply(fct_reset_events_waits_by_account);
}

static void
fct_reset_events_waits_by_user(PFS_user *pfs)
{
  pfs->aggregate_waits();
}

/** Reset table EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME data. */
void
reset_events_waits_by_user()
{
  global_user_container.apply(fct_reset_events_waits_by_user);
}

static void
fct_reset_events_waits_by_host(PFS_host *pfs)
{
  pfs->aggregate_waits();
}

/** Reset table EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME data. */
void
reset_events_waits_by_host()
{
  global_host_container.apply(fct_reset_events_waits_by_host);
}

static void
fct_reset_table_waits_by_table(PFS_table_share *pfs)
{
  pfs->aggregate();
}

void
reset_table_waits_by_table()
{
  global_table_share_container.apply(fct_reset_table_waits_by_table);
}

static void
fct_reset_table_io_waits_by_table(PFS_table_share *pfs)
{
  pfs->aggregate_io();
}

void
reset_table_io_waits_by_table()
{
  global_table_share_container.apply(fct_reset_table_io_waits_by_table);
}

static void
fct_reset_table_lock_waits_by_table(PFS_table_share *pfs)
{
  pfs->aggregate_lock();
}

void
reset_table_lock_waits_by_table()
{
  global_table_share_container.apply(fct_reset_table_lock_waits_by_table);
}

static void
fct_reset_table_waits_by_table_handle(PFS_table *pfs)
{
  pfs->sanitized_aggregate();
}

void
reset_table_waits_by_table_handle()
{
  global_table_container.apply(fct_reset_table_waits_by_table_handle);
}

static void
fct_reset_table_io_waits_by_table_handle(PFS_table *pfs)
{
  pfs->sanitized_aggregate_io();
}

void
reset_table_io_waits_by_table_handle()
{
  global_table_container.apply(fct_reset_table_io_waits_by_table_handle);
}

static void
fct_reset_table_lock_waits_by_table_handle(PFS_table *pfs)
{
  pfs->sanitized_aggregate_lock();
}

void
reset_table_lock_waits_by_table_handle()
{
  global_table_container.apply(fct_reset_table_lock_waits_by_table_handle);
}
