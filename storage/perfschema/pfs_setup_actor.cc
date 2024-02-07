/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

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

#include <assert.h>
#include "my_base.h"
#include "my_compiler.h"

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
int init_setup_actor(const PFS_global_param *param) {
  return global_setup_actor_container.init(param->m_setup_actor_sizing);
}

/** Cleanup all the setup actor buffers. */
void cleanup_setup_actor() { global_setup_actor_container.cleanup(); }

static const uchar *setup_actor_hash_get_key(const uchar *entry,
                                             size_t *length) {
  const PFS_setup_actor *const *typed_entry;
  const PFS_setup_actor *setup_actor;
  const void *result;
  typed_entry = reinterpret_cast<const PFS_setup_actor *const *>(entry);
  assert(typed_entry != nullptr);
  setup_actor = *typed_entry;
  assert(setup_actor != nullptr);
  *length = sizeof(setup_actor->m_key);
  result = &setup_actor->m_key;
  return reinterpret_cast<const uchar *>(result);
}

static uint setup_actor_hash_func(const LF_HASH *, const uchar *key,
                                  size_t key_len [[maybe_unused]]) {
  const PFS_setup_actor_key *setup_actor_key;
  uint64 nr1;
  uint64 nr2;

  assert(key_len == sizeof(PFS_setup_actor_key));
  setup_actor_key = reinterpret_cast<const PFS_setup_actor_key *>(key);
  assert(setup_actor_key != nullptr);

  nr1 = 0;
  nr2 = 0;

  setup_actor_key->m_user_name.hash(&nr1, &nr2);
  setup_actor_key->m_host_name.hash(&nr1, &nr2);
  setup_actor_key->m_role_name.hash(&nr1, &nr2);

  return nr1;
}

static int setup_actor_hash_cmp_func(const uchar *key1,
                                     size_t key_len1 [[maybe_unused]],
                                     const uchar *key2,
                                     size_t key_len2 [[maybe_unused]]) {
  const PFS_setup_actor_key *setup_actor_key1;
  const PFS_setup_actor_key *setup_actor_key2;
  int cmp;

  assert(key_len1 == sizeof(PFS_setup_actor_key));
  assert(key_len2 == sizeof(PFS_setup_actor_key));
  setup_actor_key1 = reinterpret_cast<const PFS_setup_actor_key *>(key1);
  setup_actor_key2 = reinterpret_cast<const PFS_setup_actor_key *>(key2);
  assert(setup_actor_key1 != nullptr);
  assert(setup_actor_key2 != nullptr);

  cmp = setup_actor_key1->m_user_name.sort(&setup_actor_key2->m_user_name);
  if (cmp != 0) {
    return cmp;
  }
  cmp = setup_actor_key1->m_host_name.sort(&setup_actor_key2->m_host_name);
  if (cmp != 0) {
    return cmp;
  }
  cmp = setup_actor_key1->m_role_name.sort(&setup_actor_key2->m_role_name);
  return cmp;
}

/**
  Initialize the setup actor hash.
  @return 0 on success
*/
int init_setup_actor_hash(const PFS_global_param *param) {
  if ((!setup_actor_hash_inited) && (param->m_setup_actor_sizing != 0)) {
    lf_hash_init3(&setup_actor_hash, sizeof(PFS_setup_actor *), LF_HASH_UNIQUE,
                  setup_actor_hash_get_key, setup_actor_hash_func,
                  setup_actor_hash_cmp_func, nullptr /* ctor */,
                  nullptr /* dtor */, nullptr /* init */);
    /* setup_actor_hash.size= param->m_setup_actor_sizing; */
    setup_actor_hash_inited = true;
  }
  return 0;
}

/** Cleanup the setup actor hash. */
void cleanup_setup_actor_hash() {
  if (setup_actor_hash_inited) {
    lf_hash_destroy(&setup_actor_hash);
    setup_actor_hash_inited = false;
  }
}

static LF_PINS *get_setup_actor_hash_pins(PFS_thread *thread) {
  if (unlikely(thread->m_setup_actor_hash_pins == nullptr)) {
    if (!setup_actor_hash_inited) {
      return nullptr;
    }
    thread->m_setup_actor_hash_pins = lf_hash_get_pins(&setup_actor_hash);
  }
  return thread->m_setup_actor_hash_pins;
}

static void set_setup_actor_key(PFS_setup_actor_key *key,
                                const PFS_user_name *user,
                                const PFS_host_name *host,
                                const PFS_role_name *role) {
  key->m_user_name = *user;
  key->m_host_name = *host;
  key->m_role_name = *role;
}

int insert_setup_actor(const PFS_user_name *user, const PFS_host_name *host,
                       const PFS_role_name *role, bool enabled, bool history) {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == nullptr)) {
    return HA_ERR_OUT_OF_MEM;
  }

  LF_PINS *pins = get_setup_actor_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    return HA_ERR_OUT_OF_MEM;
  }

  PFS_setup_actor *pfs;
  pfs_dirty_state dirty_state;

  pfs = global_setup_actor_container.allocate(&dirty_state);
  if (pfs != nullptr) {
    set_setup_actor_key(&pfs->m_key, user, host, role);
    pfs->m_enabled = enabled;
    pfs->m_history = history;

    int res;
    res = lf_hash_insert(&setup_actor_hash, pins, &pfs);
    if (likely(res == 0)) {
      update_setup_actors_derived_flags();
      pfs->m_lock.dirty_to_allocated(&dirty_state);
      return 0;
    }

    global_setup_actor_container.dirty_to_free(&dirty_state, pfs);

    if (res > 0) {
      return HA_ERR_FOUND_DUPP_KEY;
    }
    return HA_ERR_OUT_OF_MEM;
  }

  return HA_ERR_RECORD_FILE_FULL;
}

int delete_setup_actor(const PFS_user_name *user, const PFS_host_name *host,
                       const PFS_role_name *role) {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == nullptr)) {
    return HA_ERR_OUT_OF_MEM;
  }

  LF_PINS *pins = get_setup_actor_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    return HA_ERR_OUT_OF_MEM;
  }

  PFS_setup_actor_key key;
  set_setup_actor_key(&key, user, host, role);

  PFS_setup_actor **entry;
  entry = reinterpret_cast<PFS_setup_actor **>(
      lf_hash_search(&setup_actor_hash, pins, &key, sizeof(key)));

  if (entry && (entry != MY_LF_ERRPTR)) {
    PFS_setup_actor *pfs = *entry;
    lf_hash_delete(&setup_actor_hash, pins, &key, sizeof(key));
    global_setup_actor_container.deallocate(pfs);
  }

  lf_hash_search_unpin(pins);

  update_setup_actors_derived_flags();

  return 0;
}

class Proc_reset_setup_actor : public PFS_buffer_processor<PFS_setup_actor> {
 public:
  explicit Proc_reset_setup_actor(LF_PINS *pins) : m_pins(pins) {}

  void operator()(PFS_setup_actor *pfs) override {
    lf_hash_delete(&setup_actor_hash, m_pins, &pfs->m_key, sizeof(pfs->m_key));

    global_setup_actor_container.deallocate(pfs);
  }

 private:
  LF_PINS *m_pins;
};

int reset_setup_actor() {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == nullptr)) {
    return HA_ERR_OUT_OF_MEM;
  }

  LF_PINS *pins = get_setup_actor_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    return HA_ERR_OUT_OF_MEM;
  }

  Proc_reset_setup_actor proc(pins);
  // FIXME: delete helper instead
  global_setup_actor_container.apply(proc);

  update_setup_actors_derived_flags();

  return 0;
}

long setup_actor_count() { return setup_actor_hash.count; }

/*
  - '%' should be replaced by NULL in table SETUP_ACTOR
  - add an ENABLED column to include/exclude patterns, more flexible
  - the principle is similar to SETUP_OBJECTS
*/
void lookup_setup_actor(PFS_thread *thread, const PFS_user_name *user,
                        const PFS_host_name *host, bool *enabled,
                        bool *history) {
  PFS_setup_actor_key key;
  PFS_setup_actor **entry;
  int i;

  LF_PINS *pins = get_setup_actor_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    *enabled = false;
    *history = false;
    return;
  }

  PFS_user_name any_user;
  PFS_host_name any_host;
  PFS_role_name any_role;
  any_user.set("%", 1);
  any_host.set("%", 1);
  any_role.set("%", 1);

  for (i = 1; i <= 4; i++) {
    /*
      Role name not used yet.
      Looking up "%" in SETUP_ACTORS.ROLE.
    */
    switch (i) {
      case 1:
        set_setup_actor_key(&key, user, host, &any_role);
        break;
      case 2:
        set_setup_actor_key(&key, user, &any_host, &any_role);
        break;
      case 3:
        set_setup_actor_key(&key, &any_user, host, &any_role);
        break;
      case 4:
        set_setup_actor_key(&key, &any_user, &any_host, &any_role);
        break;
    }
    entry = reinterpret_cast<PFS_setup_actor **>(
        lf_hash_search(&setup_actor_hash, pins, &key, sizeof(key)));

    if (entry && (entry != MY_LF_ERRPTR)) {
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
}

int update_setup_actors_derived_flags() {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == nullptr)) {
    return HA_ERR_OUT_OF_MEM;
  }

  update_accounts_derived_flags(thread);
  return 0;
}

/** @} */
