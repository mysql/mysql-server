/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_setup_actor.cc
  Performance schema setup actor (implementation).
*/

#include "storage/perfschema/pfs_setup_actor.h"

#include "my_base.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "storage/perfschema/pfs.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_stat.h"

/**
  @addtogroup performance_schema_buffers
  @{
*/

/** Hash table for setup_actor records. */
LF_HASH setup_actor_hash;
/** True if @c setup_actor_hash is initialized. */
static bool setup_actor_hash_inited = false;

/**
  Initialize the setup actor buffers.
  @param param                        sizing parameters
  @return 0 on success
*/
int
init_setup_actor(const PFS_global_param *param)
{
  return global_setup_actor_container.init(param->m_setup_actor_sizing);
}

/** Cleanup all the setup actor buffers. */
void
cleanup_setup_actor(void)
{
  global_setup_actor_container.cleanup();
}

static const uchar *
setup_actor_hash_get_key(const uchar *entry, size_t *length)
{
  const PFS_setup_actor *const *typed_entry;
  const PFS_setup_actor *setup_actor;
  const void *result;
  typed_entry = reinterpret_cast<const PFS_setup_actor *const *>(entry);
  DBUG_ASSERT(typed_entry != NULL);
  setup_actor = *typed_entry;
  DBUG_ASSERT(setup_actor != NULL);
  *length = setup_actor->m_key.m_key_length;
  result = setup_actor->m_key.m_hash_key;
  return reinterpret_cast<const uchar *>(result);
}

/**
  Initialize the setup actor hash.
  @return 0 on success
*/
int
init_setup_actor_hash(const PFS_global_param *param)
{
  if ((!setup_actor_hash_inited) && (param->m_setup_actor_sizing != 0))
  {
    lf_hash_init(&setup_actor_hash,
                 sizeof(PFS_setup_actor *),
                 LF_HASH_UNIQUE,
                 0,
                 0,
                 setup_actor_hash_get_key,
                 &my_charset_bin);
    /* setup_actor_hash.size= param->m_setup_actor_sizing; */
    setup_actor_hash_inited = true;
  }
  return 0;
}

/** Cleanup the setup actor hash. */
void
cleanup_setup_actor_hash(void)
{
  if (setup_actor_hash_inited)
  {
    lf_hash_destroy(&setup_actor_hash);
    setup_actor_hash_inited = false;
  }
}

static LF_PINS *
get_setup_actor_hash_pins(PFS_thread *thread)
{
  if (unlikely(thread->m_setup_actor_hash_pins == NULL))
  {
    if (!setup_actor_hash_inited)
    {
      return NULL;
    }
    thread->m_setup_actor_hash_pins = lf_hash_get_pins(&setup_actor_hash);
  }
  return thread->m_setup_actor_hash_pins;
}

static void
set_setup_actor_key(PFS_setup_actor_key *key,
                    const char *user,
                    uint user_length,
                    const char *host,
                    uint host_length,
                    const char *role,
                    uint role_length)
{
  DBUG_ASSERT(user_length <= USERNAME_LENGTH);
  DBUG_ASSERT(host_length <= HOSTNAME_LENGTH);

  char *ptr = &key->m_hash_key[0];
  memcpy(ptr, user, user_length);
  ptr += user_length;
  ptr[0] = 0;
  ptr++;
  memcpy(ptr, host, host_length);
  ptr += host_length;
  ptr[0] = 0;
  ptr++;
  memcpy(ptr, role, role_length);
  ptr += role_length;
  ptr[0] = 0;
  ptr++;
  key->m_key_length = ptr - &key->m_hash_key[0];
}

int
insert_setup_actor(const String *user,
                   const String *host,
                   const String *role,
                   bool enabled,
                   bool history)
{
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == NULL))
  {
    return HA_ERR_OUT_OF_MEM;
  }

  LF_PINS *pins = get_setup_actor_hash_pins(thread);
  if (unlikely(pins == NULL))
  {
    return HA_ERR_OUT_OF_MEM;
  }

  PFS_setup_actor *pfs;
  pfs_dirty_state dirty_state;

  pfs = global_setup_actor_container.allocate(&dirty_state);
  if (pfs != NULL)
  {
    set_setup_actor_key(&pfs->m_key,
                        user->ptr(),
                        user->length(),
                        host->ptr(),
                        host->length(),
                        role->ptr(),
                        role->length());
    pfs->m_username = &pfs->m_key.m_hash_key[0];
    pfs->m_username_length = user->length();
    pfs->m_hostname = pfs->m_username + pfs->m_username_length + 1;
    pfs->m_hostname_length = host->length();
    pfs->m_rolename = pfs->m_hostname + pfs->m_hostname_length + 1;
    pfs->m_rolename_length = role->length();
    pfs->m_enabled = enabled;
    pfs->m_history = history;

    int res;
    pfs->m_lock.dirty_to_allocated(&dirty_state);
    res = lf_hash_insert(&setup_actor_hash, pins, &pfs);
    if (likely(res == 0))
    {
      update_setup_actors_derived_flags();
      return 0;
    }

    global_setup_actor_container.deallocate(pfs);

    if (res > 0)
    {
      return HA_ERR_FOUND_DUPP_KEY;
    }
    return HA_ERR_OUT_OF_MEM;
  }

  return HA_ERR_RECORD_FILE_FULL;
}

int
delete_setup_actor(const String *user, const String *host, const String *role)
{
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == NULL))
  {
    return HA_ERR_OUT_OF_MEM;
  }

  LF_PINS *pins = get_setup_actor_hash_pins(thread);
  if (unlikely(pins == NULL))
  {
    return HA_ERR_OUT_OF_MEM;
  }

  PFS_setup_actor_key key;
  set_setup_actor_key(&key,
                      user->ptr(),
                      user->length(),
                      host->ptr(),
                      host->length(),
                      role->ptr(),
                      role->length());

  PFS_setup_actor **entry;
  entry = reinterpret_cast<PFS_setup_actor **>(
    lf_hash_search(&setup_actor_hash, pins, key.m_hash_key, key.m_key_length));

  if (entry && (entry != MY_LF_ERRPTR))
  {
    PFS_setup_actor *pfs = *entry;
    lf_hash_delete(&setup_actor_hash, pins, key.m_hash_key, key.m_key_length);
    global_setup_actor_container.deallocate(pfs);
  }

  lf_hash_search_unpin(pins);

  update_setup_actors_derived_flags();

  return 0;
}

class Proc_reset_setup_actor : public PFS_buffer_processor<PFS_setup_actor>
{
public:
  Proc_reset_setup_actor(LF_PINS *pins) : m_pins(pins)
  {
  }

  virtual void
  operator()(PFS_setup_actor *pfs)
  {
    lf_hash_delete(&setup_actor_hash,
                   m_pins,
                   pfs->m_key.m_hash_key,
                   pfs->m_key.m_key_length);

    global_setup_actor_container.deallocate(pfs);
  }

private:
  LF_PINS *m_pins;
};

int
reset_setup_actor()
{
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == NULL))
  {
    return HA_ERR_OUT_OF_MEM;
  }

  LF_PINS *pins = get_setup_actor_hash_pins(thread);
  if (unlikely(pins == NULL))
  {
    return HA_ERR_OUT_OF_MEM;
  }

  Proc_reset_setup_actor proc(pins);
  // FIXME: delete helper instead
  global_setup_actor_container.apply(proc);

  update_setup_actors_derived_flags();

  return 0;
}

long
setup_actor_count()
{
  return setup_actor_hash.count;
}

/*
  - '%' should be replaced by NULL in table SETUP_ACTOR
  - add an ENABLED column to include/exclude patterns, more flexible
  - the principle is similar to SETUP_OBJECTS
*/
void
lookup_setup_actor(PFS_thread *thread,
                   const char *user,
                   uint user_length,
                   const char *host,
                   uint host_length,
                   bool *enabled,
                   bool *history)
{
  PFS_setup_actor_key key;
  PFS_setup_actor **entry;
  int i;

  LF_PINS *pins = get_setup_actor_hash_pins(thread);
  if (unlikely(pins == NULL))
  {
    *enabled = false;
    *history = false;
    return;
  }

  for (i = 1; i <= 4; i++)
  {
    /*
      WL#988 Roles is not implemented, so we do not have a role name.
      Looking up "%" in SETUP_ACTORS.ROLE.
    */
    switch (i)
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
    entry = reinterpret_cast<PFS_setup_actor **>(lf_hash_search(
      &setup_actor_hash, pins, key.m_hash_key, key.m_key_length));

    if (entry && (entry != MY_LF_ERRPTR))
    {
      PFS_setup_actor *pfs = *entry;
      lf_hash_search_unpin(pins);
      *enabled = pfs->m_enabled;
      *history = pfs->m_history;
      return;
    }

    lf_hash_search_unpin(pins);
  }
  *enabled = false;
  *history = false;
  return;
}

int
update_setup_actors_derived_flags()
{
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == NULL))
  {
    return HA_ERR_OUT_OF_MEM;
  }

  update_accounts_derived_flags(thread);
  return 0;
}

/** @} */
