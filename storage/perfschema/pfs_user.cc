/* Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_user.cc
  Performance schema user (implementation).
*/

#include "my_global.h"
#include "my_sys.h"
#include "pfs.h"
#include "pfs_stat.h"
#include "pfs_instr.h"
#include "pfs_setup_actor.h"
#include "pfs_user.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"

/**
  @addtogroup Performance_schema_buffers
  @{
*/

ulong user_max;
ulong user_lost;

PFS_user *user_array= NULL;

static PFS_single_stat *user_instr_class_waits_array= NULL;
static PFS_stage_stat *user_instr_class_stages_array= NULL;
static PFS_statement_stat *user_instr_class_statements_array= NULL;

LF_HASH user_hash;
static bool user_hash_inited= false;

/**
  Initialize the user buffers.
  @param param                        sizing parameters
  @return 0 on success
*/
int init_user(const PFS_global_param *param)
{
  uint index;

  user_max= param->m_user_sizing;

  user_array= NULL;
  user_instr_class_waits_array= NULL;
  user_instr_class_stages_array= NULL;
  user_instr_class_statements_array= NULL;
  uint waits_sizing= user_max * wait_class_max;
  uint stages_sizing= user_max * stage_class_max;
  uint statements_sizing= user_max * statement_class_max;

  if (user_max > 0)
  {
    user_array= PFS_MALLOC_ARRAY(user_max, PFS_user,
                                 MYF(MY_ZEROFILL));
    if (unlikely(user_array == NULL))
      return 1;
  }

  if (waits_sizing > 0)
  {
    user_instr_class_waits_array=
      PFS_connection_slice::alloc_waits_slice(waits_sizing);
    if (unlikely(user_instr_class_waits_array == NULL))
      return 1;
  }

  if (stages_sizing > 0)
  {
    user_instr_class_stages_array=
      PFS_connection_slice::alloc_stages_slice(stages_sizing);
    if (unlikely(user_instr_class_stages_array == NULL))
      return 1;
  }

  if (statements_sizing > 0)
  {
    user_instr_class_statements_array=
      PFS_connection_slice::alloc_statements_slice(statements_sizing);
    if (unlikely(user_instr_class_statements_array == NULL))
      return 1;
  }

  for (index= 0; index < user_max; index++)
  {
    user_array[index].m_instr_class_waits_stats=
      &user_instr_class_waits_array[index * wait_class_max];
    user_array[index].m_instr_class_stages_stats=
      &user_instr_class_stages_array[index * stage_class_max];
    user_array[index].m_instr_class_statements_stats=
      &user_instr_class_statements_array[index * statement_class_max];
  }

  return 0;
}

/** Cleanup all the user buffers. */
void cleanup_user(void)
{
  pfs_free(user_array);
  user_array= NULL;
  pfs_free(user_instr_class_waits_array);
  user_instr_class_waits_array= NULL;
  pfs_free(user_instr_class_stages_array);
  user_instr_class_stages_array= NULL;
  pfs_free(user_instr_class_statements_array);
  user_instr_class_statements_array= NULL;
  user_max= 0;
}

C_MODE_START
static uchar *user_hash_get_key(const uchar *entry, size_t *length,
                                my_bool)
{
  const PFS_user * const *typed_entry;
  const PFS_user *user;
  const void *result;
  typed_entry= reinterpret_cast<const PFS_user* const *> (entry);
  DBUG_ASSERT(typed_entry != NULL);
  user= *typed_entry;
  DBUG_ASSERT(user != NULL);
  *length= user->m_key.m_key_length;
  result= user->m_key.m_hash_key;
  return const_cast<uchar*> (reinterpret_cast<const uchar*> (result));
}
C_MODE_END

/**
  Initialize the user hash.
  @return 0 on success
*/
int init_user_hash(void)
{
  if ((! user_hash_inited) && (user_max > 0))
  {
    lf_hash_init(&user_hash, sizeof(PFS_user*), LF_HASH_UNIQUE,
                 0, 0, user_hash_get_key, &my_charset_bin);
    user_hash.size= user_max;
    user_hash_inited= true;
  }
  return 0;
}

/** Cleanup the user hash. */
void cleanup_user_hash(void)
{
  if (user_hash_inited)
  {
    lf_hash_destroy(&user_hash);
    user_hash_inited= false;
  }
}

static LF_PINS* get_user_hash_pins(PFS_thread *thread)
{
  if (unlikely(thread->m_user_hash_pins == NULL))
  {
    if (! user_hash_inited)
      return NULL;
    thread->m_user_hash_pins= lf_hash_get_pins(&user_hash);
  }
  return thread->m_user_hash_pins;
}

static void set_user_key(PFS_user_key *key,
                         const char *user, uint user_length)
{
  DBUG_ASSERT(user_length <= USERNAME_LENGTH);

  char *ptr= &key->m_hash_key[0];
  if (user_length > 0)
  {
    memcpy(ptr, user, user_length);
    ptr+= user_length;
  }
  ptr[0]= 0;
  ptr++;
  key->m_key_length= ptr - &key->m_hash_key[0];
}

PFS_user *
find_or_create_user(PFS_thread *thread,
                    const char *username, uint username_length)
{
  if (user_max == 0)
  {
    user_lost++;
    return NULL;
  }

  LF_PINS *pins= get_user_hash_pins(thread);
  if (unlikely(pins == NULL))
  {
    user_lost++;
    return NULL;
  }

  PFS_user_key key;
  set_user_key(&key, username, username_length);

  PFS_user **entry;
  uint retry_count= 0;
  const uint retry_max= 3;

search:
  entry= reinterpret_cast<PFS_user**>
    (lf_hash_search(&user_hash, pins,
                    key.m_hash_key, key.m_key_length));
  if (entry && (entry != MY_ERRPTR))
  {
    PFS_user *pfs;
    pfs= *entry;
    pfs->inc_refcount();
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  PFS_scan scan;
  uint random= randomized_index(username, user_max);

  for (scan.init(random, user_max);
       scan.has_pass();
       scan.next_pass())
  {
    PFS_user *pfs= user_array + scan.first();
    PFS_user *pfs_last= user_array + scan.last();
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

          pfs->init_refcount();
          pfs->reset_stats();
          pfs->m_disconnected_count= 0;

          int res;
          res= lf_hash_insert(&user_hash, pins, &pfs);
          if (likely(res == 0))
          {
            pfs->m_lock.dirty_to_allocated();
            return pfs;
          }

          pfs->m_lock.dirty_to_free();

          if (res > 0)
          {
            if (++retry_count > retry_max)
            {
              user_lost++;
              return NULL;
            }
            goto search;
          }

          user_lost++;
          return NULL;
        }
      }
    }
  }

  user_lost++;
  return NULL;
}

void PFS_user::aggregate()
{
  aggregate_waits();
  aggregate_stages();
  aggregate_statements();
  aggregate_stats();
}

void PFS_user::aggregate_waits()
{
  /* No parent to aggregate to, clean the stats */
  reset_waits_stats();
}

void PFS_user::aggregate_stages()
{
  /* No parent to aggregate to, clean the stats */
  reset_stages_stats();
}

void PFS_user::aggregate_statements()
{
  /* No parent to aggregate to, clean the stats */
  reset_statements_stats();
}

void PFS_user::aggregate_stats()
{
  /* No parent to aggregate to, clean the stats */
  m_disconnected_count= 0;
}

void PFS_user::release()
{
  dec_refcount();
}

PFS_user *sanitize_user(PFS_user *unsafe)
{
  if ((&user_array[0] <= unsafe) &&
      (unsafe < &user_array[user_max]))
    return unsafe;
  return NULL;
}

void purge_user(PFS_thread *thread, PFS_user *user)
{
  LF_PINS *pins= get_user_hash_pins(thread);
  if (unlikely(pins == NULL))
    return;

  PFS_user **entry;
  entry= reinterpret_cast<PFS_user**>
    (lf_hash_search(&user_hash, pins,
                    user->m_key.m_hash_key, user->m_key.m_key_length));
  if (entry && (entry != MY_ERRPTR))
  {
    PFS_user *pfs;
    pfs= *entry;
    DBUG_ASSERT(pfs == user);
    if (user->get_refcount() == 0)
    {
      lf_hash_delete(&user_hash, pins,
                     user->m_key.m_hash_key, user->m_key.m_key_length);
      user->m_lock.allocated_to_free();
    }
  }

  lf_hash_search_unpin(pins);
}

/** Purge non connected users, reset stats of connected users. */
void purge_all_user(void)
{
  PFS_thread *thread= PFS_thread::get_current_thread();
  if (unlikely(thread == NULL))
    return;

  PFS_user *pfs= user_array;
  PFS_user *pfs_last= user_array + user_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
    {
      pfs->aggregate();
      if (pfs->get_refcount() == 0)
        purge_user(thread, pfs);
    }
  }
}

/** @} */
