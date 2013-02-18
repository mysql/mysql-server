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
  @file storage/perfschema/pfs_account.cc
  Performance schema account (implementation).
*/

#include "my_global.h"
#include "my_sys.h"
#include "pfs.h"
#include "pfs_stat.h"
#include "pfs_instr.h"
#include "pfs_setup_actor.h"
#include "pfs_host.h"
#include "pfs_host.h"
#include "pfs_user.h"
#include "pfs_account.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"

/**
  @addtogroup Performance_schema_buffers
  @{
*/

ulong account_max;
ulong account_lost;

PFS_account *account_array= NULL;

static PFS_single_stat *account_instr_class_waits_array= NULL;
static PFS_stage_stat *account_instr_class_stages_array= NULL;
static PFS_statement_stat *account_instr_class_statements_array= NULL;

LF_HASH account_hash;
static bool account_hash_inited= false;

/**
  Initialize the user buffers.
  @param param                        sizing parameters
  @return 0 on success
*/
int init_account(const PFS_global_param *param)
{
  uint index;

  account_max= param->m_account_sizing;

  account_array= NULL;
  account_instr_class_waits_array= NULL;
  account_instr_class_stages_array= NULL;
  account_instr_class_statements_array= NULL;
  uint waits_sizing= account_max * wait_class_max;
  uint stages_sizing= account_max * stage_class_max;
  uint statements_sizing= account_max * statement_class_max;

  if (account_max > 0)
  {
    account_array= PFS_MALLOC_ARRAY(account_max, PFS_account,
                                      MYF(MY_ZEROFILL));
    if (unlikely(account_array == NULL))
      return 1;
  }

  if (waits_sizing > 0)
  {
    account_instr_class_waits_array=
      PFS_connection_slice::alloc_waits_slice(waits_sizing);
    if (unlikely(account_instr_class_waits_array == NULL))
      return 1;
  }

  if (stages_sizing > 0)
  {
    account_instr_class_stages_array=
      PFS_connection_slice::alloc_stages_slice(stages_sizing);
    if (unlikely(account_instr_class_stages_array == NULL))
      return 1;
  }

  if (statements_sizing > 0)
  {
    account_instr_class_statements_array=
      PFS_connection_slice::alloc_statements_slice(statements_sizing);
    if (unlikely(account_instr_class_statements_array == NULL))
      return 1;
  }

  for (index= 0; index < account_max; index++)
  {
    account_array[index].m_instr_class_waits_stats=
      &account_instr_class_waits_array[index * wait_class_max];
    account_array[index].m_instr_class_stages_stats=
      &account_instr_class_stages_array[index * stage_class_max];
    account_array[index].m_instr_class_statements_stats=
      &account_instr_class_statements_array[index * statement_class_max];
  }

  return 0;
}

/** Cleanup all the user buffers. */
void cleanup_account(void)
{
  pfs_free(account_array);
  account_array= NULL;
  pfs_free(account_instr_class_waits_array);
  account_instr_class_waits_array= NULL;
  account_max= 0;
}

C_MODE_START
static uchar *account_hash_get_key(const uchar *entry, size_t *length,
                                my_bool)
{
  const PFS_account * const *typed_entry;
  const PFS_account *account;
  const void *result;
  typed_entry= reinterpret_cast<const PFS_account* const *> (entry);
  DBUG_ASSERT(typed_entry != NULL);
  account= *typed_entry;
  DBUG_ASSERT(account != NULL);
  *length= account->m_key.m_key_length;
  result= account->m_key.m_hash_key;
  return const_cast<uchar*> (reinterpret_cast<const uchar*> (result));
}
C_MODE_END

/**
  Initialize the user hash.
  @return 0 on success
*/
int init_account_hash(void)
{
  if ((! account_hash_inited) && (account_max > 0))
  {
    lf_hash_init(&account_hash, sizeof(PFS_account*), LF_HASH_UNIQUE,
                 0, 0, account_hash_get_key, &my_charset_bin);
    account_hash.size= account_max;
    account_hash_inited= true;
  }
  return 0;
}

/** Cleanup the user hash. */
void cleanup_account_hash(void)
{
  if (account_hash_inited)
  {
    lf_hash_destroy(&account_hash);
    account_hash_inited= false;
  }
}

static LF_PINS* get_account_hash_pins(PFS_thread *thread)
{
  if (unlikely(thread->m_account_hash_pins == NULL))
  {
    if (! account_hash_inited)
      return NULL;
    thread->m_account_hash_pins= lf_hash_get_pins(&account_hash);
  }
  return thread->m_account_hash_pins;
}

static void set_account_key(PFS_account_key *key,
                              const char *user, uint user_length,
                              const char *host, uint host_length)
{
  DBUG_ASSERT(user_length <= USERNAME_LENGTH);
  DBUG_ASSERT(host_length <= HOSTNAME_LENGTH);

  char *ptr= &key->m_hash_key[0];
  if (user_length > 0)
  {
    memcpy(ptr, user, user_length);
    ptr+= user_length;
  }
  ptr[0]= 0;
  ptr++;
  if (host_length > 0)
  {
    memcpy(ptr, host, host_length);
    ptr+= host_length;
  }
  ptr[0]= 0;
  ptr++;
  key->m_key_length= ptr - &key->m_hash_key[0];
}

PFS_account *
find_or_create_account(PFS_thread *thread,
                         const char *username, uint username_length,
                         const char *hostname, uint hostname_length)
{
  if (account_max == 0)
  {
    account_lost++;
    return NULL;
  }

  LF_PINS *pins= get_account_hash_pins(thread);
  if (unlikely(pins == NULL))
  {
    account_lost++;
    return NULL;
  }

  PFS_account_key key;
  set_account_key(&key, username, username_length,
                    hostname, hostname_length);

  PFS_account **entry;
  uint retry_count= 0;
  const uint retry_max= 3;

search:
  entry= reinterpret_cast<PFS_account**>
    (lf_hash_search(&account_hash, pins,
                    key.m_hash_key, key.m_key_length));
  if (entry && (entry != MY_ERRPTR))
  {
    PFS_account *pfs;
    pfs= *entry;
    pfs->inc_refcount();
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  PFS_scan scan;
  uint random= randomized_index(username, account_max);

  for (scan.init(random, account_max);
       scan.has_pass();
       scan.next_pass())
  {
    PFS_account *pfs= account_array + scan.first();
    PFS_account *pfs_last= account_array + scan.last();
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_free())
      {
        if (pfs->m_lock.free_to_dirty())
        {
          pfs->m_key= key;
          if (username_length > 0)
            pfs->m_username= &pfs->m_key.m_hash_key[0];
          else
            pfs->m_username= NULL;
          pfs->m_username_length= username_length;

          if (hostname_length > 0)
            pfs->m_hostname= &pfs->m_key.m_hash_key[username_length + 1];
          else
            pfs->m_hostname= NULL;
          pfs->m_hostname_length= hostname_length;

          pfs->m_user= find_or_create_user(thread, username, username_length);
          pfs->m_host= find_or_create_host(thread, hostname, hostname_length);

          pfs->init_refcount();
          pfs->reset_stats();
          pfs->m_disconnected_count= 0;

          int res;
          res= lf_hash_insert(&account_hash, pins, &pfs);
          if (likely(res == 0))
          {
            pfs->m_lock.dirty_to_allocated();
            return pfs;
          }

          if (pfs->m_user)
          {
            pfs->m_user->release();
            pfs->m_user= NULL;
          }
          if (pfs->m_host)
          {
            pfs->m_host->release();
            pfs->m_host= NULL;
          }

          pfs->m_lock.dirty_to_free();

          if (res > 0)
          {
            if (++retry_count > retry_max)
            {
              account_lost++;
              return NULL;
            }
            goto search;
          }

          account_lost++;
          return NULL;
        }
      }
    }
  }

  account_lost++;
  return NULL;
}

void PFS_account::aggregate()
{
  aggregate_waits();
  aggregate_stages();
  aggregate_statements();
  aggregate_stats();
}

void PFS_account::aggregate_waits()
{
  if (likely(m_user != NULL && m_host != NULL))
  {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_event_names(m_instr_class_waits_stats,
                              m_user->m_instr_class_waits_stats,
                              m_host->m_instr_class_waits_stats);
    return;
  }

  if (m_user != NULL)
  {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME
    */
    aggregate_all_event_names(m_instr_class_waits_stats,
                              m_user->m_instr_class_waits_stats);
    return;
  }

  if (m_host != NULL)
  {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME
    */
    aggregate_all_event_names(m_instr_class_waits_stats,
                              m_host->m_instr_class_waits_stats);
    return;
  }

  /* Orphan account, no parent to aggregate to. */
  reset_waits_stats();
  return;
}

void PFS_account::aggregate_stages()
{
  if (likely(m_user != NULL && m_host != NULL))
  {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_stages(m_instr_class_stages_stats,
                         m_user->m_instr_class_stages_stats,
                         m_host->m_instr_class_stages_stats);
    return;
  }

  if (m_user != NULL)
  {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_stages(m_instr_class_stages_stats,
                         m_user->m_instr_class_stages_stats,
                         global_instr_class_stages_array);
    return;
  }

  if (m_host != NULL)
  {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME
    */
    aggregate_all_stages(m_instr_class_stages_stats,
                         m_host->m_instr_class_stages_stats);
    return;
  }

  /*
    Aggregate EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
    -  EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME
  */
  aggregate_all_stages(m_instr_class_stages_stats,
                       global_instr_class_stages_array);
  return;
}

void PFS_account::aggregate_statements()
{
  if (likely(m_user != NULL && m_host != NULL))
  {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STATEMENTS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_statements(m_instr_class_statements_stats,
                             m_user->m_instr_class_statements_stats,
                             m_host->m_instr_class_statements_stats);
    return;
  }

  if (m_user != NULL)
  {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STATEMENTS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_statements(m_instr_class_statements_stats,
                             m_user->m_instr_class_statements_stats,
                             global_instr_class_statements_array);
    return;
  }

  if (m_host != NULL)
  {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME
    */
    aggregate_all_statements(m_instr_class_statements_stats,
                             m_host->m_instr_class_statements_stats);
    return;
  }

  /*
    Aggregate EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
    -  EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME
  */
  aggregate_all_statements(m_instr_class_statements_stats,
                           global_instr_class_statements_array);
  return;
}

void PFS_account::aggregate_stats()
{
  if (likely(m_user != NULL && m_host != NULL))
  {
    m_user->m_disconnected_count+= m_disconnected_count;
    m_host->m_disconnected_count+= m_disconnected_count;
    m_disconnected_count= 0;
    return;
  }

  if (m_user != NULL)
  {
    m_user->m_disconnected_count+= m_disconnected_count;
    m_disconnected_count= 0;
    return;
  }

  if (m_host != NULL)
  {
    m_host->m_disconnected_count+= m_disconnected_count;
    m_disconnected_count= 0;
    return;
  }

  m_disconnected_count= 0;
  return;
}

void PFS_account::release()
{
  dec_refcount();
}

PFS_account *sanitize_account(PFS_account *unsafe)
{
  if ((&account_array[0] <= unsafe) &&
      (unsafe < &account_array[account_max]))
    return unsafe;
  return NULL;
}

void purge_account(PFS_thread *thread, PFS_account *account)
{
  account->aggregate();

  LF_PINS *pins= get_account_hash_pins(thread);
  if (unlikely(pins == NULL))
    return;

  PFS_account **entry;
  entry= reinterpret_cast<PFS_account**>
    (lf_hash_search(&account_hash, pins,
                    account->m_key.m_hash_key,
                    account->m_key.m_key_length));
  if (entry && (entry != MY_ERRPTR))
  {
    PFS_account *pfs;
    pfs= *entry;
    DBUG_ASSERT(pfs == account);
    if (account->get_refcount() == 0)
    {
      lf_hash_delete(&account_hash, pins,
                     account->m_key.m_hash_key,
                     account->m_key.m_key_length);
      if (account->m_user != NULL)
      {
        account->m_user->release();
        account->m_user= NULL;
      }
      if (account->m_host != NULL)
      {
        account->m_host->release();
        account->m_host= NULL;
      }
      account->m_lock.allocated_to_free();
    }
  }

  lf_hash_search_unpin(pins);
}

/** Purge non connected accounts, reset stats of connected account. */
void purge_all_account(void)
{
  PFS_thread *thread= PFS_thread::get_current_thread();
  if (unlikely(thread == NULL))
    return;

  PFS_account *pfs= account_array;
  PFS_account *pfs_last= account_array + account_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
    {
      pfs->aggregate_stats();

      if (pfs->get_refcount() == 0)
        purge_account(thread, pfs);
    }
  }
}

/** @} */
