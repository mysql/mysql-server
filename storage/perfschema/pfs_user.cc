/* Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  */

/**
  @file storage/perfschema/pfs_user.cc
  Performance schema user (implementation).
*/

#include "storage/perfschema/pfs_user.h"

#include <assert.h>
#include "my_compiler.h"

#include "my_sys.h"
#include "storage/perfschema/pfs.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_setup_actor.h"
#include "storage/perfschema/pfs_stat.h"

/**
  @addtogroup performance_schema_buffers
  @{
*/

LF_HASH user_hash;
static bool user_hash_inited = false;

/**
  Initialize the user buffers.
  @param param                        sizing parameters
  @return 0 on success
*/
int init_user(const PFS_global_param *param) {
  if (global_user_container.init(param->m_user_sizing)) {
    return 1;
  }

  return 0;
}

/** Cleanup all the user buffers. */
void cleanup_user(void) { global_user_container.cleanup(); }

static const uchar *user_hash_get_key(const uchar *entry, size_t *length) {
  const PFS_user *const *typed_entry;
  const PFS_user *user;
  const void *result;
  typed_entry = reinterpret_cast<const PFS_user *const *>(entry);
  assert(typed_entry != nullptr);
  user = *typed_entry;
  assert(user != nullptr);
  *length = sizeof(user->m_key);
  result = &user->m_key;
  return reinterpret_cast<const uchar *>(result);
}

static uint user_hash_func(const LF_HASH *, const uchar *key,
                           size_t key_len [[maybe_unused]]) {
  const PFS_user_key *user_key;
  uint64 nr1;
  uint64 nr2;

  assert(key_len == sizeof(PFS_user_key));
  user_key = reinterpret_cast<const PFS_user_key *>(key);
  assert(user_key != nullptr);

  nr1 = 0;
  nr2 = 0;

  user_key->m_user_name.hash(&nr1, &nr2);

  return nr1;
}

static int user_hash_cmp_func(const uchar *key1,
                              size_t key_len1 [[maybe_unused]],
                              const uchar *key2,
                              size_t key_len2 [[maybe_unused]]) {
  const PFS_user_key *user_key1;
  const PFS_user_key *user_key2;
  int cmp;

  assert(key_len1 == sizeof(PFS_user_key));
  assert(key_len2 == sizeof(PFS_user_key));
  user_key1 = reinterpret_cast<const PFS_user_key *>(key1);
  user_key2 = reinterpret_cast<const PFS_user_key *>(key2);
  assert(user_key1 != nullptr);
  assert(user_key2 != nullptr);

  cmp = user_key1->m_user_name.sort(&user_key2->m_user_name);
  return cmp;
}

/**
  Initialize the user hash.
  @return 0 on success
*/
int init_user_hash(const PFS_global_param *param) {
  if ((!user_hash_inited) && (param->m_user_sizing != 0)) {
    lf_hash_init3(&user_hash, sizeof(PFS_user *), LF_HASH_UNIQUE,
                  user_hash_get_key, user_hash_func, user_hash_cmp_func,
                  nullptr /* ctor */, nullptr /* dtor */, nullptr /* init */);
    user_hash_inited = true;
  }
  return 0;
}

/** Cleanup the user hash. */
void cleanup_user_hash(void) {
  if (user_hash_inited) {
    lf_hash_destroy(&user_hash);
    user_hash_inited = false;
  }
}

static LF_PINS *get_user_hash_pins(PFS_thread *thread) {
  if (unlikely(thread->m_user_hash_pins == nullptr)) {
    if (!user_hash_inited) {
      return nullptr;
    }
    thread->m_user_hash_pins = lf_hash_get_pins(&user_hash);
  }
  return thread->m_user_hash_pins;
}

static void set_user_key(PFS_user_key *key, const PFS_user_name *user) {
  key->m_user_name = *user;
}

PFS_user *find_or_create_user(PFS_thread *thread, const PFS_user_name *user) {
  LF_PINS *pins = get_user_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    global_user_container.m_lost++;
    return nullptr;
  }

  PFS_user_key key;
  set_user_key(&key, user);

  PFS_user **entry;
  PFS_user *pfs;
  uint retry_count = 0;
  const uint retry_max = 3;
  pfs_dirty_state dirty_state;

search:
  entry = reinterpret_cast<PFS_user **>(
      lf_hash_search(&user_hash, pins, &key, sizeof(key)));
  if (entry && (entry != MY_LF_ERRPTR)) {
    pfs = *entry;
    pfs->inc_refcount();
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  pfs = global_user_container.allocate(&dirty_state);
  if (pfs != nullptr) {
    pfs->m_key = key;

    pfs->init_refcount();
    pfs->reset_stats();
    pfs->reset_connections_stats();

    int res;
    pfs->m_lock.dirty_to_allocated(&dirty_state);
    res = lf_hash_insert(&user_hash, pins, &pfs);
    if (likely(res == 0)) {
      return pfs;
    }

    global_user_container.deallocate(pfs);

    if (res > 0) {
      if (++retry_count > retry_max) {
        global_user_container.m_lost++;
        return nullptr;
      }
      goto search;
    }

    global_user_container.m_lost++;
    return nullptr;
  }

  return nullptr;
}

void PFS_user::aggregate(bool alive) {
  aggregate_waits();
  aggregate_stages();
  aggregate_statements();
  aggregate_transactions();
  aggregate_errors();
  aggregate_memory(alive);
  aggregate_status();
  aggregate_stats();
}

void PFS_user::aggregate_waits() {
  /* No parent to aggregate to, clean the stats */
  reset_waits_stats();
}

void PFS_user::aggregate_stages() {
  /* No parent to aggregate to, clean the stats */
  reset_stages_stats();
}

void PFS_user::aggregate_statements() {
  /* No parent to aggregate to, clean the stats */
  reset_statements_stats();
}

void PFS_user::aggregate_transactions() {
  /* No parent to aggregate to, clean the stats */
  reset_transactions_stats();
}

void PFS_user::aggregate_errors() {
  /* No parent to aggregate to, clean the stats */
  reset_errors_stats();
}

void PFS_user::aggregate_memory(bool) {
  /* No parent to aggregate to, clean the stats */
  rebase_memory_stats();
}

void PFS_user::aggregate_status() {
  /* No parent to aggregate to, clean the stats */
  reset_status_stats();
}

void PFS_user::aggregate_stats() {
  /* No parent to aggregate to, clean the stats */
  reset_connections_stats();
}

void PFS_user::aggregate_stats_from(PFS_account *pfs) {
  m_disconnected_count += pfs->m_disconnected_count;

  if (m_max_controlled_memory < pfs->m_max_controlled_memory) {
    m_max_controlled_memory = pfs->m_max_controlled_memory;
  }

  if (m_max_total_memory < pfs->m_max_total_memory) {
    m_max_total_memory = pfs->m_max_total_memory;
  }
}

void PFS_user::aggregate_disconnect(ulonglong controlled_memory,
                                    ulonglong total_memory) {
  m_disconnected_count++;

  if (m_max_controlled_memory < controlled_memory) {
    m_max_controlled_memory = controlled_memory;
  }

  if (m_max_total_memory < total_memory) {
    m_max_total_memory = total_memory;
  }
}

void PFS_user::release() { dec_refcount(); }

void PFS_user::rebase_memory_stats() {
  PFS_memory_shared_stat *stat = m_instr_class_memory_stats;
  PFS_memory_shared_stat *stat_last = stat + memory_class_max;
  for (; stat < stat_last; stat++) {
    stat->reset();
  }
}

void PFS_user::carry_memory_stat_alloc_delta(PFS_memory_stat_alloc_delta *delta,
                                             uint index) {
  PFS_memory_shared_stat *event_name_array;
  PFS_memory_shared_stat *stat;
  PFS_memory_stat_alloc_delta delta_buffer;

  event_name_array = write_instr_class_memory_stats();
  stat = &event_name_array[index];
  (void)stat->apply_alloc_delta(delta, &delta_buffer);
}

void PFS_user::carry_memory_stat_free_delta(PFS_memory_stat_free_delta *delta,
                                            uint index) {
  PFS_memory_shared_stat *event_name_array;
  PFS_memory_shared_stat *stat;
  PFS_memory_stat_free_delta delta_buffer;

  event_name_array = write_instr_class_memory_stats();
  stat = &event_name_array[index];
  (void)stat->apply_free_delta(delta, &delta_buffer);
}

PFS_user *sanitize_user(PFS_user *unsafe) {
  return global_user_container.sanitize(unsafe);
}

static void purge_user(PFS_thread *thread, PFS_user *user) {
  LF_PINS *pins = get_user_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    return;
  }

  PFS_user **entry;
  entry = reinterpret_cast<PFS_user **>(
      lf_hash_search(&user_hash, pins, &user->m_key, sizeof(user->m_key)));
  if (entry && (entry != MY_LF_ERRPTR)) {
    assert(*entry == user);
    if (user->get_refcount() == 0) {
      lf_hash_delete(&user_hash, pins, &user->m_key, sizeof(user->m_key));
      user->aggregate(false);
      global_user_container.deallocate(user);
    }
  }

  lf_hash_search_unpin(pins);
}

class Proc_purge_user : public PFS_buffer_processor<PFS_user> {
 public:
  explicit Proc_purge_user(PFS_thread *thread) : m_thread(thread) {}

  void operator()(PFS_user *pfs) override {
    pfs->aggregate(true);
    if (pfs->get_refcount() == 0) {
      purge_user(m_thread, pfs);
    }
  }

 private:
  PFS_thread *m_thread;
};

/** Purge non connected users, reset stats of connected users. */
void purge_all_user(void) {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == nullptr)) {
    return;
  }

  Proc_purge_user proc(thread);
  global_user_container.apply(proc);
}

/** @} */
