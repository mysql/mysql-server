/* Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

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

ulong account_max= 0;
ulong account_lost= 0;
bool account_full;

PFS_account *account_array= NULL;

static PFS_single_stat *account_instr_class_waits_array= NULL;
static PFS_stage_stat *account_instr_class_stages_array= NULL;
static PFS_statement_stat *account_instr_class_statements_array= NULL;
static PFS_memory_stat *account_instr_class_memory_array= NULL;
static PFS_transaction_stat *account_instr_class_transactions_array= NULL;

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
  account_lost= 0;
  account_full= false;

  account_array= NULL;
  account_instr_class_waits_array= NULL;
  account_instr_class_stages_array= NULL;
  account_instr_class_statements_array= NULL;
  account_instr_class_transactions_array= NULL;
  account_instr_class_memory_array= NULL;
  uint waits_sizing= account_max * wait_class_max;
  uint stages_sizing= account_max * stage_class_max;
  uint statements_sizing= account_max * statement_class_max;
  uint transactions_sizing= account_max * transaction_class_max;
  uint memory_sizing= account_max * memory_class_max;

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

  if (transactions_sizing > 0)
  {
    account_instr_class_transactions_array=
      PFS_connection_slice::alloc_transactions_slice(transactions_sizing);
    if (unlikely(account_instr_class_transactions_array == NULL))
      return 1;
  }

  if (memory_sizing > 0)
  {
    account_instr_class_memory_array=
      PFS_connection_slice::alloc_memory_slice(memory_sizing);
    if (unlikely(account_instr_class_memory_array == NULL))
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
    account_array[index].m_instr_class_transactions_stats=
      &account_instr_class_transactions_array[index * transaction_class_max];
    account_array[index].m_instr_class_memory_stats=
      &account_instr_class_memory_array[index * memory_class_max];
  }

  return 0;
}

/** Cleanup all the account buffers. */
void cleanup_account(void)
{
  pfs_free(account_array);
  account_array= NULL;
  pfs_free(account_instr_class_waits_array);
  account_instr_class_waits_array= NULL;
  pfs_free(account_instr_class_stages_array);
  account_instr_class_stages_array= NULL;
  pfs_free(account_instr_class_statements_array);
  account_instr_class_statements_array= NULL;
  pfs_free(account_instr_class_memory_array);
  account_instr_class_memory_array= NULL;
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
  static PFS_ALIGNED PFS_cacheline_uint32 monotonic;

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
  PFS_account *pfs;
  uint retry_count= 0;
  const uint retry_max= 3;
  uint index;
  uint attempts= 0;

search:
  entry= reinterpret_cast<PFS_account**>
    (lf_hash_search(&account_hash, pins,
                    key.m_hash_key, key.m_key_length));
  if (entry && (entry != MY_ERRPTR))
  {
    pfs= *entry;
    pfs->inc_refcount();
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  if (account_full)
  {
    account_lost++;
    return NULL;
  }

  while (++attempts <= account_max)
  {
    index= PFS_atomic::add_u32(& monotonic.m_u32, 1) % account_max;
    pfs= account_array + index;

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

        if (username_length > 0 && hostname_length > 0)
        {
          lookup_setup_actor(thread, username, username_length, hostname, hostname_length,
                             & pfs->m_enabled);
        }
        else
          pfs->m_enabled= true;

        int res;
        pfs->m_lock.dirty_to_allocated();
        res= lf_hash_insert(&account_hash, pins, &pfs);
        if (likely(res == 0))
        {
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

        pfs->m_lock.allocated_to_free();

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

  account_lost++;
  account_full= true;
  return NULL;
}

void PFS_account::aggregate(bool alive, PFS_user *safe_user, PFS_host *safe_host)
{
  aggregate_waits(safe_user, safe_host);
  aggregate_stages(safe_user, safe_host);
  aggregate_statements(safe_user, safe_host);
  aggregate_transactions(safe_user, safe_host);
  aggregate_memory(alive, safe_user, safe_host);
  aggregate_stats(safe_user, safe_host);
}

void PFS_account::aggregate_waits(PFS_user *safe_user, PFS_host *safe_host)
{
  if (likely(safe_user != NULL && safe_host != NULL))
  {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_event_names(m_instr_class_waits_stats,
                              safe_user->m_instr_class_waits_stats,
                              safe_host->m_instr_class_waits_stats);
    return;
  }

  if (safe_user != NULL)
  {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME
    */
    aggregate_all_event_names(m_instr_class_waits_stats,
                              safe_user->m_instr_class_waits_stats);
    return;
  }

  if (safe_host != NULL)
  {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME
    */
    aggregate_all_event_names(m_instr_class_waits_stats,
                              safe_host->m_instr_class_waits_stats);
    return;
  }

  /* Orphan account, no parent to aggregate to. */
  reset_waits_stats();
  return;
}

void PFS_account::aggregate_stages(PFS_user *safe_user, PFS_host *safe_host)
{
  if (likely(safe_user != NULL && safe_host != NULL))
  {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_stages(m_instr_class_stages_stats,
                         safe_user->m_instr_class_stages_stats,
                         safe_host->m_instr_class_stages_stats);
    return;
  }

  if (safe_user != NULL)
  {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_stages(m_instr_class_stages_stats,
                         safe_user->m_instr_class_stages_stats,
                         global_instr_class_stages_array);
    return;
  }

  if (safe_host != NULL)
  {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME
    */
    aggregate_all_stages(m_instr_class_stages_stats,
                         safe_host->m_instr_class_stages_stats);
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

void PFS_account::aggregate_statements(PFS_user *safe_user, PFS_host *safe_host)
{
  if (likely(safe_user != NULL && safe_host != NULL))
  {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STATEMENTS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_statements(m_instr_class_statements_stats,
                             safe_user->m_instr_class_statements_stats,
                             safe_host->m_instr_class_statements_stats);
    return;
  }

  if (safe_user != NULL)
  {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STATEMENTS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_statements(m_instr_class_statements_stats,
                             safe_user->m_instr_class_statements_stats,
                             global_instr_class_statements_array);
    return;
  }

  if (safe_host != NULL)
  {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME
    */
    aggregate_all_statements(m_instr_class_statements_stats,
                             safe_host->m_instr_class_statements_stats);
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

void PFS_account::aggregate_transactions(PFS_user *safe_user, PFS_host *safe_host)
{
  if (likely(safe_user != NULL && safe_host != NULL))
  {
    /*
      Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_TRANSACTIONS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_TRANSACTIONS_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_transactions(m_instr_class_transactions_stats,
                               safe_user->m_instr_class_transactions_stats,
                               safe_host->m_instr_class_transactions_stats);
    return;
  }

  if (safe_user != NULL)
  {
    /*
      Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_TRANSACTIONS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_transactions(m_instr_class_transactions_stats,
                               safe_user->m_instr_class_transactions_stats,
                               &global_transaction_stat);
    return;
  }

  if (safe_host != NULL)
  {
    /*
      Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_TRANSACTIONS_SUMMARY_BY_HOST_BY_EVENT_NAME
    */
    aggregate_all_transactions(m_instr_class_transactions_stats,
                               safe_host->m_instr_class_transactions_stats);
    return;
  }

  /*
    Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
    -  EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME
  */
  aggregate_all_transactions(m_instr_class_transactions_stats,
                             &global_transaction_stat);
  return;
}

void PFS_account::aggregate_memory(bool alive, PFS_user *safe_user, PFS_host *safe_host)
{
  if (likely(safe_user != NULL && safe_host != NULL))
  {
    /*
      Aggregate MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      - MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME
      - MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_memory(alive,
                         m_instr_class_memory_stats,
                         safe_user->m_instr_class_memory_stats,
                         safe_host->m_instr_class_memory_stats);
    return;
  }

  if (safe_user != NULL)
  {
    /*
      Aggregate MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      - MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME
      - MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_memory(alive,
                         m_instr_class_memory_stats,
                         safe_user->m_instr_class_memory_stats,
                         global_instr_class_memory_array);
    return;
  }

  if (safe_host != NULL)
  {
    /*
      Aggregate MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      - MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME
    */
    aggregate_all_memory(alive,
                         m_instr_class_memory_stats,
                         safe_host->m_instr_class_memory_stats);
    return;
  }

  /*
    Aggregate MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
    - MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME
  */
  aggregate_all_memory(alive,
                       m_instr_class_memory_stats,
                       global_instr_class_memory_array);
  return;
}

void PFS_account::aggregate_stats(PFS_user *safe_user, PFS_host *safe_host)
{
  if (likely(safe_user != NULL && safe_host != NULL))
  {
    safe_user->m_disconnected_count+= m_disconnected_count;
    safe_host->m_disconnected_count+= m_disconnected_count;
    m_disconnected_count= 0;
    return;
  }

  if (safe_user != NULL)
  {
    safe_user->m_disconnected_count+= m_disconnected_count;
    m_disconnected_count= 0;
    return;
  }

  if (safe_host != NULL)
  {
    safe_host->m_disconnected_count+= m_disconnected_count;
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

void PFS_account::carry_memory_stat_delta(PFS_memory_stat_delta *delta, uint index)
{
  PFS_memory_stat *stat;
  PFS_memory_stat_delta delta_buffer;
  PFS_memory_stat_delta *remaining_delta;

  stat= & m_instr_class_memory_stats[index];
  remaining_delta= stat->apply_delta(delta, &delta_buffer);

  if (remaining_delta == NULL)
    return;

  if (m_user != NULL)
  {
    m_user->carry_memory_stat_delta(remaining_delta, index);
    /* do not return, need to process m_host below */
  }

  if (m_host != NULL)
  {
    m_host->carry_memory_stat_delta(remaining_delta, index);
    return;
  }

  carry_global_memory_stat_delta(remaining_delta, index);
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
    DBUG_ASSERT(*entry == account);
    if (account->get_refcount() == 0)
    {
      lf_hash_delete(&account_hash, pins,
                     account->m_key.m_hash_key,
                     account->m_key.m_key_length);
      account->aggregate(false, account->m_user, account->m_host);
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
      account_full= false;
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
  PFS_user *user;
  PFS_host *host;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
    {
      user= sanitize_user(pfs->m_user);
      host= sanitize_host(pfs->m_host);
      pfs->aggregate(true, user, host);

      if (pfs->get_refcount() == 0)
        purge_account(thread, pfs);
    }
  }
}

void update_accounts_derived_flags(PFS_thread *thread)
{
  PFS_account *pfs= account_array;
  PFS_account *pfs_last= account_array + account_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
    {
      if (pfs->m_username_length > 0 && pfs->m_hostname_length > 0)
      {
        lookup_setup_actor(thread,
                           pfs->m_username, pfs->m_username_length,
                           pfs->m_hostname, pfs->m_hostname_length,
                           & pfs->m_enabled);
      }
      else
        pfs->m_enabled= true;
    }
  }
}

/** @} */
