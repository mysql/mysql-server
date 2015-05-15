/* Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "pfs_buffer_container.h"
#include "pfs_builtin_memory.h"
#include "m_string.h"

PFS_ALIGNED size_t events_statements_history_long_size= 0;
/** Consumer flag for table EVENTS_STATEMENTS_CURRENT. */
PFS_ALIGNED bool flag_events_statements_current= false;
/** Consumer flag for table EVENTS_STATEMENTS_HISTORY. */
PFS_ALIGNED bool flag_events_statements_history= false;
/** Consumer flag for table EVENTS_STATEMENTS_HISTORY_LONG. */
PFS_ALIGNED bool flag_events_statements_history_long= false;

/** True if EVENTS_STATEMENTS_HISTORY_LONG circular buffer is full. */
PFS_ALIGNED bool events_statements_history_long_full= false;
/** Index in EVENTS_STATEMENTS_HISTORY_LONG circular buffer. */
PFS_ALIGNED PFS_cacheline_uint32 events_statements_history_long_index;
/** EVENTS_STATEMENTS_HISTORY_LONG circular buffer. */
PFS_ALIGNED PFS_events_statements *events_statements_history_long_array= NULL;
static unsigned char *h_long_stmts_digest_token_array= NULL;
static char *h_long_stmts_text_array= NULL;

/**
  Initialize table EVENTS_STATEMENTS_HISTORY_LONG.
  @param events_statements_history_long_sizing       table sizing
*/
int init_events_statements_history_long(size_t events_statements_history_long_sizing)
{
  events_statements_history_long_size= events_statements_history_long_sizing;
  events_statements_history_long_full= false;
  PFS_atomic::store_u32(&events_statements_history_long_index.m_u32, 0);

  if (events_statements_history_long_size == 0)
    return 0;

  events_statements_history_long_array=
    PFS_MALLOC_ARRAY(& builtin_memory_statements_history_long,
                     events_statements_history_long_size, sizeof(PFS_events_statements),
                     PFS_events_statements, MYF(MY_ZEROFILL));

  if (events_statements_history_long_array == NULL)
   {
     cleanup_events_statements_history_long();
     return 1;
   }

  if (pfs_max_digest_length > 0)
  {
    /* Size of each digest text array. */
    size_t digest_text_size= pfs_max_digest_length * sizeof(unsigned char);

    h_long_stmts_digest_token_array=
      PFS_MALLOC_ARRAY(& builtin_memory_statements_history_long_tokens,
                       events_statements_history_long_size, digest_text_size,
                       unsigned char, MYF(MY_ZEROFILL));

    if (h_long_stmts_digest_token_array == NULL)
    {
      cleanup_events_statements_history_long();
      return 1;
    }
  }

  if (pfs_max_sqltext > 0)
  {
    /* Size of each sql text array. */
    size_t sqltext_size= pfs_max_sqltext * sizeof(char);

    h_long_stmts_text_array=
      PFS_MALLOC_ARRAY(& builtin_memory_statements_history_long_sqltext,
                       events_statements_history_long_size, sqltext_size,
                       char, MYF(MY_ZEROFILL));

    if (h_long_stmts_text_array == NULL)
    {
      cleanup_events_statements_history_long();
      return 1;
    }
  }

  for (size_t index= 0; index < events_statements_history_long_size; index++)
  {
    events_statements_history_long_array[index].m_digest_storage.reset(h_long_stmts_digest_token_array
                                                                       + index * pfs_max_digest_length, pfs_max_digest_length);
    events_statements_history_long_array[index].m_sqltext= h_long_stmts_text_array + index * pfs_max_sqltext;
  }

  return 0;
}

/** Cleanup table EVENTS_STATEMENTS_HISTORY_LONG. */
void cleanup_events_statements_history_long(void)
{
  PFS_FREE_ARRAY(& builtin_memory_statements_history_long,
                 events_statements_history_long_size,
                 sizeof(PFS_events_statements),
                 events_statements_history_long_array);

  PFS_FREE_ARRAY(& builtin_memory_statements_history_long_tokens,
                 events_statements_history_long_size,
                 (pfs_max_digest_length * sizeof(unsigned char)),
                 h_long_stmts_digest_token_array);

  PFS_FREE_ARRAY(& builtin_memory_statements_history_long_sqltext,
                 events_statements_history_long_size,
                 (pfs_max_sqltext * sizeof(char)),
                 h_long_stmts_text_array);

  events_statements_history_long_array= NULL;
  h_long_stmts_digest_token_array= NULL;
  h_long_stmts_text_array= NULL;
}

static inline void copy_events_statements(PFS_events_statements *dest,
                                          const PFS_events_statements *source)
{
  /* Copy all attributes except SQL TEXT and DIGEST */
  memcpy(dest, source, my_offsetof(PFS_events_statements, m_sqltext));

  /* Copy SQL TEXT */
  int sqltext_length= source->m_sqltext_length;

  if (sqltext_length > 0)
  {
    memcpy(dest->m_sqltext, source->m_sqltext, sqltext_length);
    dest->m_sqltext_length= sqltext_length;
  }
  else
  {
    dest->m_sqltext_length= 0;
  }

  /* Copy DIGEST */
  dest->m_digest_storage.copy(& source->m_digest_storage);
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

  uint index= PFS_atomic::add_u32(&events_statements_history_long_index.m_u32, 1);

  index= index % events_statements_history_long_size;
  if (index == 0)
    events_statements_history_long_full= true;

  /* See related comment in insert_events_statements_history. */
  copy_events_statements(&events_statements_history_long_array[index], statement);
}

static void fct_reset_events_statements_current(PFS_thread *pfs_thread)
{
  PFS_events_statements *pfs_stmt= & pfs_thread->m_statement_stack[0];
  PFS_events_statements *pfs_stmt_last= pfs_stmt + statement_stack_max;

  for ( ; pfs_stmt < pfs_stmt_last; pfs_stmt++)
    pfs_stmt->m_class= NULL;
}

/** Reset table EVENTS_STATEMENTS_CURRENT data. */
void reset_events_statements_current(void)
{
  global_thread_container.apply_all(fct_reset_events_statements_current);
}

static void fct_reset_events_statements_history(PFS_thread *pfs_thread)
{
  PFS_events_statements *pfs= pfs_thread->m_statements_history;
  PFS_events_statements *pfs_last= pfs + events_statements_history_per_thread;

  pfs_thread->m_statements_history_index= 0;
  pfs_thread->m_statements_history_full= false;
  for ( ; pfs < pfs_last; pfs++)
    pfs->m_class= NULL;
}

/** Reset table EVENTS_STATEMENTS_HISTORY data. */
void reset_events_statements_history(void)
{
  global_thread_container.apply_all(fct_reset_events_statements_history);
}

/** Reset table EVENTS_STATEMENTS_HISTORY_LONG data. */
void reset_events_statements_history_long(void)
{
  PFS_atomic::store_u32(&events_statements_history_long_index.m_u32, 0);
  events_statements_history_long_full= false;

  PFS_events_statements *pfs= events_statements_history_long_array;
  PFS_events_statements *pfs_last= pfs + events_statements_history_long_size;
  for ( ; pfs < pfs_last; pfs++)
    pfs->m_class= NULL;
}

static void fct_reset_events_statements_by_thread(PFS_thread *thread)
{
  PFS_account *account= sanitize_account(thread->m_account);
  PFS_user *user= sanitize_user(thread->m_user);
  PFS_host *host= sanitize_host(thread->m_host);
  aggregate_thread_statements(thread, account, user, host);
}

/** Reset table EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME data. */
void reset_events_statements_by_thread()
{
  global_thread_container.apply(fct_reset_events_statements_by_thread);
}

static void fct_reset_events_statements_by_account(PFS_account *pfs)
{
  PFS_user *user= sanitize_user(pfs->m_user);
  PFS_host *host= sanitize_host(pfs->m_host);
  pfs->aggregate_statements(user, host);
}

/** Reset table EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME data. */
void reset_events_statements_by_account()
{
  global_account_container.apply(fct_reset_events_statements_by_account);
}

static void fct_reset_events_statements_by_user(PFS_user *pfs)
{
  pfs->aggregate_statements();
}

/** Reset table EVENTS_STATEMENTS_SUMMARY_BY_USER_BY_EVENT_NAME data. */
void reset_events_statements_by_user()
{
  global_user_container.apply(fct_reset_events_statements_by_user);
}

static void fct_reset_events_statements_by_host(PFS_host *pfs)
{
  pfs->aggregate_statements();
}

/** Reset table EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME data. */
void reset_events_statements_by_host()
{
  global_host_container.apply(fct_reset_events_statements_by_host);
}

/** Reset table EVENTS_STATEMENTS_GLOBAL_BY_EVENT_NAME data. */
void reset_events_statements_global()
{
  PFS_statement_stat *stat= global_instr_class_statements_array;
  PFS_statement_stat *stat_last= global_instr_class_statements_array + statement_class_max;

  for ( ; stat < stat_last; stat++)
    stat->reset();
}

