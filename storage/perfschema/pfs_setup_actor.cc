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

/**
  @file storage/perfschema/pfs_setup_actor.cc
  Performance schema setup actor (implementation).
*/

#include "my_global.h"
#include "my_sys.h"
#include "my_base.h"
#include "pfs.h"
#include "pfs_stat.h"
#include "pfs_instr.h"
#include "pfs_setup_actor.h"
#include "pfs_global.h"

/**
  @addtogroup Performance_schema_buffers
  @{
*/

/** Size of the setup_actor instances array. @sa setup_actor_array */
ulong setup_actor_max;

/**
  Setup_actor instances array.
  @sa setup_actor_max
*/

PFS_setup_actor *setup_actor_array= NULL;

/** Hash table for setup_actor records. */
LF_HASH setup_actor_hash;
/** True if @c setup_actor_hash is initialized. */
static bool setup_actor_hash_inited= false;

/**
  Initialize the setup actor buffers.
  @param param                        sizing parameters
  @return 0 on success
*/
int init_setup_actor(const PFS_global_param *param)
{
  setup_actor_max= param->m_setup_actor_sizing;

  setup_actor_array= NULL;

  if (setup_actor_max > 0)
  {
    setup_actor_array= PFS_MALLOC_ARRAY(setup_actor_max, PFS_setup_actor,
                                         MYF(MY_ZEROFILL));
    if (unlikely(setup_actor_array == NULL))
      return 1;
  }

  return 0;
}

/** Cleanup all the setup actor buffers. */
void cleanup_setup_actor(void)
{
  pfs_free(setup_actor_array);
  setup_actor_array= NULL;
  setup_actor_max= 0;
}

C_MODE_START
static uchar *setup_actor_hash_get_key(const uchar *entry, size_t *length,
                                       my_bool)
{
  const PFS_setup_actor * const *typed_entry;
  const PFS_setup_actor *setup_actor;
  const void *result;
  typed_entry= reinterpret_cast<const PFS_setup_actor* const *> (entry);
  DBUG_ASSERT(typed_entry != NULL);
  setup_actor= *typed_entry;
  DBUG_ASSERT(setup_actor != NULL);
  *length= setup_actor->m_key.m_key_length;
  result= setup_actor->m_key.m_hash_key;
  return const_cast<uchar*> (reinterpret_cast<const uchar*> (result));
}
C_MODE_END

/**
  Initialize the setup actor hash.
  @return 0 on success
*/
int init_setup_actor_hash(void)
{
  if ((! setup_actor_hash_inited) && (setup_actor_max > 0))
  {
    lf_hash_init(&setup_actor_hash, sizeof(PFS_setup_actor*), LF_HASH_UNIQUE,
                 0, 0, setup_actor_hash_get_key, &my_charset_bin);
    setup_actor_hash.size= setup_actor_max;
    setup_actor_hash_inited= true;
  }
  return 0;
}

/** Cleanup the setup actor hash. */
void cleanup_setup_actor_hash(void)
{
  if (setup_actor_hash_inited)
  {
    lf_hash_destroy(&setup_actor_hash);
    setup_actor_hash_inited= false;
  }
}

static LF_PINS* get_setup_actor_hash_pins(PFS_thread *thread)
{
  if (unlikely(thread->m_setup_actor_hash_pins == NULL))
  {
    if (! setup_actor_hash_inited)
      return NULL;
    thread->m_setup_actor_hash_pins= lf_hash_get_pins(&setup_actor_hash);
  }
  return thread->m_setup_actor_hash_pins;
}

static void set_setup_actor_key(PFS_setup_actor_key *key,
                                const char *user, uint user_length,
                                const char *host, uint host_length,
                                const char *role, uint role_length)
{
  DBUG_ASSERT(user_length <= USERNAME_LENGTH);
  DBUG_ASSERT(host_length <= HOSTNAME_LENGTH);

  char *ptr= &key->m_hash_key[0];
  memcpy(ptr, user, user_length);
  ptr+= user_length;
  ptr[0]= 0;
  ptr++;
  memcpy(ptr, host, host_length);
  ptr+= host_length;
  ptr[0]= 0;
  ptr++;
  memcpy(ptr, role, role_length);
  ptr+= role_length;
  ptr[0]= 0;
  ptr++;
  key->m_key_length= ptr - &key->m_hash_key[0];
}

int insert_setup_actor(const String *user, const String *host, const String *role)
{
  if (setup_actor_max == 0)
    return HA_ERR_RECORD_FILE_FULL;

  PFS_thread *thread= PFS_thread::get_current_thread();
  if (unlikely(thread == NULL))
    return HA_ERR_OUT_OF_MEM;

  LF_PINS *pins= get_setup_actor_hash_pins(thread);
  if (unlikely(pins == NULL))
    return HA_ERR_OUT_OF_MEM;

  static uint PFS_ALIGNED setup_actor_monotonic_index= 0;
  uint index;
  uint attempts= 0;
  PFS_setup_actor *pfs;

  while (++attempts <= setup_actor_max)
  {
    /* See create_mutex() */
    index= PFS_atomic::add_u32(& setup_actor_monotonic_index, 1) % setup_actor_max;
    pfs= setup_actor_array + index;

    if (pfs->m_lock.is_free())
    {
      if (pfs->m_lock.free_to_dirty())
      {
        set_setup_actor_key(&pfs->m_key,
                            user->ptr(), user->length(),
                            host->ptr(), host->length(),
                            role->ptr(), role->length());
        pfs->m_username= &pfs->m_key.m_hash_key[0];
        pfs->m_username_length= user->length();
        pfs->m_hostname= pfs->m_username + pfs->m_username_length + 1;
        pfs->m_hostname_length= host->length();
        pfs->m_rolename= pfs->m_hostname + pfs->m_hostname_length + 1;
        pfs->m_rolename_length= role->length();

        int res;
        res= lf_hash_insert(&setup_actor_hash, pins, &pfs);
        if (likely(res == 0))
        {
          pfs->m_lock.dirty_to_allocated();
          return 0;
        }

        pfs->m_lock.dirty_to_free();
        if (res > 0)
          return HA_ERR_FOUND_DUPP_KEY;
        return HA_ERR_OUT_OF_MEM;
      }
    }
  }

  return HA_ERR_RECORD_FILE_FULL;
}

int delete_setup_actor(const String *user, const String *host, const String *role)
{
  PFS_thread *thread= PFS_thread::get_current_thread();
  if (unlikely(thread == NULL))
    return HA_ERR_OUT_OF_MEM;

  LF_PINS* pins= get_setup_actor_hash_pins(thread);
  if (unlikely(pins == NULL))
    return HA_ERR_OUT_OF_MEM;

  PFS_setup_actor_key key;
  set_setup_actor_key(&key,
                      user->ptr(), user->length(),
                      host->ptr(), host->length(),
                      role->ptr(), role->length());

  PFS_setup_actor **entry;
  entry= reinterpret_cast<PFS_setup_actor**>
    (lf_hash_search(&setup_actor_hash, pins, key.m_hash_key, key.m_key_length));

  if (entry && (entry != MY_ERRPTR))
  {
    PFS_setup_actor *pfs= *entry;
    lf_hash_delete(&setup_actor_hash, pins, key.m_hash_key, key.m_key_length);
    pfs->m_lock.allocated_to_free();
  }

  lf_hash_search_unpin(pins);

  return 0;
}

int reset_setup_actor()
{
  PFS_thread *thread= PFS_thread::get_current_thread();
  if (unlikely(thread == NULL))
    return HA_ERR_OUT_OF_MEM;

  LF_PINS* pins= get_setup_actor_hash_pins(thread);
  if (unlikely(pins == NULL))
    return HA_ERR_OUT_OF_MEM;

  PFS_setup_actor *pfs= setup_actor_array;
  PFS_setup_actor *pfs_last= setup_actor_array + setup_actor_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
    {
      lf_hash_delete(&setup_actor_hash, pins,
                     pfs->m_key.m_hash_key, pfs->m_key.m_key_length);
      pfs->m_lock.allocated_to_free();
    }
  }

  return 0;
}

long setup_actor_count()
{
  return setup_actor_hash.count;
}

/*
  - '%' should be replaced by NULL in table SETUP_ACTOR
  - add an ENABLED column to include/exclude patterns, more flexible
  - the principle is similar to SETUP_OBJECTS
*/
void lookup_setup_actor(PFS_thread *thread,
                        const char *user, uint user_length,
                        const char *host, uint host_length,
                        bool *enabled)
{
  PFS_setup_actor_key key;
  PFS_setup_actor **entry;
  int i;

  LF_PINS* pins= get_setup_actor_hash_pins(thread);
  if (unlikely(pins == NULL))
  {
    *enabled= false;
    return;
  }

  for (i= 1; i<=4; i++)
  {
    /*
      WL#988 Roles is not implemented, so we do not have a role name.
      Looking up "%" in SETUP_ACTORS.ROLE.
    */
    switch(i)
    {
    case 1:
      set_setup_actor_key(&key, user, user_length, host, host_length, "%", 1);
      break;
    case 2:
      set_setup_actor_key(&key, user, user_length, "%", 1, "%", 1);
      break;
    case 3:
      set_setup_actor_key(&key, "%", 1, host, host_length, "%", 1);
      break;
    case 4:
      set_setup_actor_key(&key, "%", 1, "%", 1, "%", 1);
      break;
    }
    entry= reinterpret_cast<PFS_setup_actor**>
      (lf_hash_search(&setup_actor_hash, pins, key.m_hash_key, key.m_key_length));

    if (entry && (entry != MY_ERRPTR))
    {
      lf_hash_search_unpin(pins);
      *enabled= true;
      return;
    }

    lf_hash_search_unpin(pins);
  }
  *enabled= false;
  return;
}

/** @} */
