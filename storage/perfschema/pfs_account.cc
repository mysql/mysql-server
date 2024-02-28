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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  */

/**
  @file storage/perfschema/pfs_account.cc
  Performance schema account (implementation).
*/

#include "storage/perfschema/pfs_account.h"

#include <assert.h>
#include "my_compiler.h"

#include "my_sys.h"
#include "sql/mysqld.h"  // global_status_var
#include "storage/perfschema/pfs.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_setup_actor.h"
#include "storage/perfschema/pfs_stat.h"
#include "storage/perfschema/pfs_user.h"

/**
  @addtogroup performance_schema_buffers
  @{
*/

LF_HASH account_hash;
static bool account_hash_inited = false;

/**
  Initialize the user buffers.
  @param param                        sizing parameters
  @return 0 on success
*/
int init_account(const PFS_global_param *param) {
  if (global_account_container.init(param->m_account_sizing)) {
    return 1;
  }

  return 0;
}

/** Cleanup all the account buffers. */
void cleanup_account() { global_account_container.cleanup(); }

static const uchar *account_hash_get_key(const uchar *entry, size_t *length) {
  const PFS_account *const *typed_entry;
  const PFS_account *account;
  const void *result;
  typed_entry = reinterpret_cast<const PFS_account *const *>(entry);
  assert(typed_entry != nullptr);
  account = *typed_entry;
  assert(account != nullptr);
  *length = sizeof(account->m_key);
  result = &account->m_key;
  return reinterpret_cast<const uchar *>(result);
}

static uint account_hash_func(const LF_HASH *, const uchar *key,
                              size_t key_len [[maybe_unused]]) {
  const PFS_account_key *account_key;
  uint64 nr1;
  uint64 nr2;

  assert(key_len == sizeof(PFS_account_key));
  account_key = reinterpret_cast<const PFS_account_key *>(key);
  assert(account_key != nullptr);

  nr1 = 0;
  nr2 = 0;

  account_key->m_user_name.hash(&nr1, &nr2);
  account_key->m_host_name.hash(&nr1, &nr2);

  return nr1;
}

static int account_hash_cmp_func(const uchar *key1,
                                 size_t key_len1 [[maybe_unused]],
                                 const uchar *key2,
                                 size_t key_len2 [[maybe_unused]]) {
  const PFS_account_key *account_key1;
  const PFS_account_key *account_key2;
  int cmp;

  assert(key_len1 == sizeof(PFS_account_key));
  assert(key_len2 == sizeof(PFS_account_key));
  account_key1 = reinterpret_cast<const PFS_account_key *>(key1);
  account_key2 = reinterpret_cast<const PFS_account_key *>(key2);
  assert(account_key1 != nullptr);
  assert(account_key2 != nullptr);

  cmp = account_key1->m_user_name.sort(&account_key2->m_user_name);
  if (cmp != 0) {
    return cmp;
  }
  cmp = account_key1->m_host_name.sort(&account_key2->m_host_name);
  return cmp;
}

/**
  Initialize the user hash.
  @return 0 on success
*/
int init_account_hash(const PFS_global_param *param) {
  if ((!account_hash_inited) && (param->m_account_sizing != 0)) {
    lf_hash_init3(&account_hash, sizeof(PFS_account *), LF_HASH_UNIQUE,
                  account_hash_get_key, account_hash_func,
                  account_hash_cmp_func, nullptr /* ctor */, nullptr /* dtor */,
                  nullptr /* init */);
    account_hash_inited = true;
  }
  return 0;
}

/** Cleanup the user hash. */
void cleanup_account_hash() {
  if (account_hash_inited) {
    lf_hash_destroy(&account_hash);
    account_hash_inited = false;
  }
}

static LF_PINS *get_account_hash_pins(PFS_thread *thread) {
  if (unlikely(thread->m_account_hash_pins == nullptr)) {
    if (!account_hash_inited) {
      return nullptr;
    }
    thread->m_account_hash_pins = lf_hash_get_pins(&account_hash);
  }
  return thread->m_account_hash_pins;
}

static void set_account_key(PFS_account_key *key, const PFS_user_name *user,
                            const PFS_host_name *host) {
  key->m_user_name = *user;
  key->m_host_name = *host;
}

PFS_account *find_or_create_account(PFS_thread *thread,
                                    const PFS_user_name *user,
                                    const PFS_host_name *host) {
  LF_PINS *pins = get_account_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    global_account_container.m_lost++;
    return nullptr;
  }

  PFS_account_key key;
  set_account_key(&key, user, host);

  PFS_account **entry;
  PFS_account *pfs;
  uint retry_count = 0;
  constexpr uint retry_max = 3;
  pfs_dirty_state dirty_state;

search:
  entry = reinterpret_cast<PFS_account **>(
      lf_hash_search(&account_hash, pins, &key, sizeof(key)));
  if (entry && (entry != MY_LF_ERRPTR)) {
    pfs = *entry;
    pfs->inc_refcount();
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  pfs = global_account_container.allocate(&dirty_state);
  if (pfs != nullptr) {
    pfs->m_key = key;

    pfs->m_user = find_or_create_user(thread, &key.m_user_name);
    pfs->m_host = find_or_create_host(thread, &key.m_host_name);

    pfs->init_refcount();
    pfs->reset_stats();
    pfs->reset_connections_stats();

    if (user->length() > 0 && host->length() > 0) {
      lookup_setup_actor(thread, &key.m_user_name, &key.m_host_name,
                         &pfs->m_enabled, &pfs->m_history);
    } else {
      pfs->m_enabled = true;
      pfs->m_history = true;
    }

    int res;
    res = lf_hash_insert(&account_hash, pins, &pfs);
    if (likely(res == 0)) {
      pfs->m_lock.dirty_to_allocated(&dirty_state);
      return pfs;
    }

    if (pfs->m_user) {
      pfs->m_user->release();
      pfs->m_user = nullptr;
    }
    if (pfs->m_host) {
      pfs->m_host->release();
      pfs->m_host = nullptr;
    }

    global_account_container.dirty_to_free(&dirty_state, pfs);

    if (res > 0) {
      if (++retry_count > retry_max) {
        global_account_container.m_lost++;
        return nullptr;
      }
      goto search;
    }

    global_account_container.m_lost++;
    return nullptr;
  }

  return nullptr;
}

void PFS_account::aggregate(bool alive, PFS_user *safe_user,
                            PFS_host *safe_host) {
  aggregate_waits(safe_user, safe_host);
  aggregate_stages(safe_user, safe_host);
  aggregate_statements(safe_user, safe_host);
  aggregate_transactions(safe_user, safe_host);
  aggregate_errors(safe_user, safe_host);
  aggregate_memory(alive, safe_user, safe_host);
  aggregate_status(safe_user, safe_host);
  aggregate_stats(safe_user, safe_host);
}

void PFS_account::aggregate_waits(PFS_user *safe_user, PFS_host *safe_host) {
  if (read_instr_class_waits_stats() == nullptr) {
    return;
  }

  if (likely(safe_user != nullptr && safe_host != nullptr)) {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_event_names(write_instr_class_waits_stats(),
                              safe_user->write_instr_class_waits_stats(),
                              safe_host->write_instr_class_waits_stats());
    return;
  }

  if (safe_user != nullptr) {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME
    */
    aggregate_all_event_names(write_instr_class_waits_stats(),
                              safe_user->write_instr_class_waits_stats());
    return;
  }

  if (safe_host != nullptr) {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME
    */
    aggregate_all_event_names(write_instr_class_waits_stats(),
                              safe_host->write_instr_class_waits_stats());
    return;
  }

  /* Orphan account, no parent to aggregate to. */
  reset_waits_stats();
}

void PFS_account::aggregate_stages(PFS_user *safe_user, PFS_host *safe_host) {
  if (read_instr_class_stages_stats() == nullptr) {
    return;
  }

  if (likely(safe_user != nullptr && safe_host != nullptr)) {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_stages(write_instr_class_stages_stats(),
                         safe_user->write_instr_class_stages_stats(),
                         safe_host->write_instr_class_stages_stats());
    return;
  }

  if (safe_user != nullptr) {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_stages(write_instr_class_stages_stats(),
                         safe_user->write_instr_class_stages_stats(),
                         global_instr_class_stages_array);
    return;
  }

  if (safe_host != nullptr) {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME
    */
    aggregate_all_stages(write_instr_class_stages_stats(),
                         safe_host->write_instr_class_stages_stats());
    return;
  }

  /*
    Aggregate EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
    -  EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME
  */
  aggregate_all_stages(write_instr_class_stages_stats(),
                       global_instr_class_stages_array);
}

void PFS_account::aggregate_statements(PFS_user *safe_user,
                                       PFS_host *safe_host) {
  if (read_instr_class_statements_stats() == nullptr) {
    return;
  }

  if (likely(safe_user != nullptr && safe_host != nullptr)) {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STATEMENTS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_statements(write_instr_class_statements_stats(),
                             safe_user->write_instr_class_statements_stats(),
                             safe_host->write_instr_class_statements_stats());
    return;
  }

  if (safe_user != nullptr) {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STATEMENTS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_statements(write_instr_class_statements_stats(),
                             safe_user->write_instr_class_statements_stats(),
                             global_instr_class_statements_array);
    return;
  }

  if (safe_host != nullptr) {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME
    */
    aggregate_all_statements(write_instr_class_statements_stats(),
                             safe_host->write_instr_class_statements_stats());
    return;
  }

  /*
    Aggregate EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
    -  EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME
  */
  aggregate_all_statements(write_instr_class_statements_stats(),
                           global_instr_class_statements_array);
}

void PFS_account::aggregate_transactions(PFS_user *safe_user,
                                         PFS_host *safe_host) {
  if (read_instr_class_transactions_stats() == nullptr) {
    return;
  }

  if (likely(safe_user != nullptr && safe_host != nullptr)) {
    /*
      Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_TRANSACTIONS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_TRANSACTIONS_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_transactions(
        write_instr_class_transactions_stats(),
        safe_user->write_instr_class_transactions_stats(),
        safe_host->write_instr_class_transactions_stats());
    return;
  }

  if (safe_user != nullptr) {
    /*
      Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_TRANSACTIONS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_transactions(
        write_instr_class_transactions_stats(),
        safe_user->write_instr_class_transactions_stats(),
        &global_transaction_stat);
    return;
  }

  if (safe_host != nullptr) {
    /*
      Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_TRANSACTIONS_SUMMARY_BY_HOST_BY_EVENT_NAME
    */
    aggregate_all_transactions(
        write_instr_class_transactions_stats(),
        safe_host->write_instr_class_transactions_stats());
    return;
  }

  /*
    Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
    -  EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME
  */
  aggregate_all_transactions(write_instr_class_transactions_stats(),
                             &global_transaction_stat);
}

void PFS_account::aggregate_errors(PFS_user *safe_user, PFS_host *safe_host) {
  if (read_instr_class_errors_stats() == nullptr) {
    return;
  }

  if (likely(safe_user != nullptr && safe_host != nullptr)) {
    /*
      Aggregate EVENTS_ERRORS_SUMMARY_BY_ACCOUNT_BY_ERROR to:
      -  EVENTS_ERRORS_SUMMARY_BY_USER_BY_ERROR
      -  EVENTS_ERRORS_SUMMARY_BY_HOST_BY_ERROR
      in parallel.
    */
    aggregate_all_errors(write_instr_class_errors_stats(),
                         safe_user->write_instr_class_errors_stats(),
                         safe_host->write_instr_class_errors_stats());
    return;
  }

  if (safe_user != nullptr) {
    /*
      Aggregate EVENTS_ERRORS_SUMMARY_BY_ACCOUNT_BY_ERROR to:
      -  EVENTS_ERRORS_SUMMARY_BY_USER_BY_ERROR
      -  EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR
      in parallel.
    */
    aggregate_all_errors(write_instr_class_errors_stats(),
                         safe_user->write_instr_class_errors_stats(),
                         &global_error_stat);
    return;
  }

  if (safe_host != nullptr) {
    /*
      Aggregate EVENTS_ERRORS_SUMMARY_BY_ACCOUNT_BY_ERROR to:
      -  EVENTS_ERRORS_SUMMARY_BY_HOST_BY_ERROR
    */
    aggregate_all_errors(write_instr_class_errors_stats(),
                         safe_host->write_instr_class_errors_stats());
    return;
  }

  /*
    Aggregate EVENTS_ERRORS_SUMMARY_BY_ACCOUNT_BY_ERROR to:
    -  EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR
  */
  aggregate_all_errors(write_instr_class_errors_stats(), &global_error_stat);
}

void PFS_account::aggregate_memory(bool alive, PFS_user *safe_user,
                                   PFS_host *safe_host) {
  if (read_instr_class_memory_stats() == nullptr) {
    return;
  }

  if (likely(safe_user != nullptr && safe_host != nullptr)) {
    /*
      Aggregate MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      - MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME
      - MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_memory(alive, write_instr_class_memory_stats(),
                         safe_user->write_instr_class_memory_stats(),
                         safe_host->write_instr_class_memory_stats());
    return;
  }

  if (safe_user != nullptr) {
    /*
      Aggregate MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      - MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME
      - MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_memory(alive, write_instr_class_memory_stats(),
                         safe_user->write_instr_class_memory_stats(),
                         global_instr_class_memory_array);
    return;
  }

  if (safe_host != nullptr) {
    /*
      Aggregate MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      - MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME
    */
    aggregate_all_memory(alive, write_instr_class_memory_stats(),
                         safe_host->write_instr_class_memory_stats());
    return;
  }

  /*
    Aggregate MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
    - MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME
  */
  aggregate_all_memory(alive, write_instr_class_memory_stats(),
                       global_instr_class_memory_array);
}

void PFS_account::aggregate_status(PFS_user *safe_user, PFS_host *safe_host) {
  /*
    Never aggregate to global_status_var,
    because of the parallel THD -> global_status_var flow.
  */

  if (safe_user != nullptr) {
    /*
      Aggregate STATUS_BY_ACCOUNT to:
      - STATUS_BY_USER
    */
    safe_user->m_status_stats.aggregate(&m_status_stats);
  }

  if (safe_host != nullptr) {
    /*
      Aggregate STATUS_BY_ACCOUNT to:
      - STATUS_BY_HOST
    */
    safe_host->m_status_stats.aggregate(&m_status_stats);
  }

  m_status_stats.reset();
}

void PFS_account::aggregate_stats(PFS_user *safe_user, PFS_host *safe_host) {
  if (safe_user != nullptr) {
    safe_user->aggregate_stats_from(this);
  }

  if (safe_host != nullptr) {
    safe_host->aggregate_stats_from(this);
  }

  reset_connections_stats();
}

void PFS_account::aggregate_disconnect(ulonglong controlled_memory,
                                       ulonglong total_memory) {
  m_disconnected_count++;

  if (m_max_controlled_memory < controlled_memory) {
    m_max_controlled_memory = controlled_memory;
  }

  if (m_max_total_memory < total_memory) {
    m_max_total_memory = total_memory;
  }
}

void PFS_account::release() { dec_refcount(); }

void PFS_account::rebase_memory_stats() {
  PFS_memory_shared_stat *stat = m_instr_class_memory_stats;
  const PFS_memory_shared_stat *stat_last = stat + memory_class_max;
  for (; stat < stat_last; stat++) {
    stat->reset();
  }
}

void PFS_account::carry_memory_stat_alloc_delta(
    PFS_memory_stat_alloc_delta *delta, uint index) {
  PFS_memory_shared_stat *event_name_array;
  PFS_memory_shared_stat *stat;
  PFS_memory_stat_alloc_delta delta_buffer;
  PFS_memory_stat_alloc_delta *remaining_delta;

  event_name_array = write_instr_class_memory_stats();
  stat = &event_name_array[index];
  remaining_delta = stat->apply_alloc_delta(delta, &delta_buffer);

  if (remaining_delta == nullptr) {
    return;
  }

  if (m_user != nullptr) {
    m_user->carry_memory_stat_alloc_delta(remaining_delta, index);
    /* do not return, need to process m_host below */
  }

  if (m_host != nullptr) {
    m_host->carry_memory_stat_alloc_delta(remaining_delta, index);
    return;
  }

  carry_global_memory_stat_alloc_delta(remaining_delta, index);
}

void PFS_account::carry_memory_stat_free_delta(
    PFS_memory_stat_free_delta *delta, uint index) {
  PFS_memory_shared_stat *event_name_array;
  PFS_memory_shared_stat *stat;
  PFS_memory_stat_free_delta delta_buffer;
  PFS_memory_stat_free_delta *remaining_delta;

  event_name_array = write_instr_class_memory_stats();
  stat = &event_name_array[index];
  remaining_delta = stat->apply_free_delta(delta, &delta_buffer);

  if (remaining_delta == nullptr) {
    return;
  }

  if (m_user != nullptr) {
    m_user->carry_memory_stat_free_delta(remaining_delta, index);
    /* do not return, need to process m_host below */
  }

  if (m_host != nullptr) {
    m_host->carry_memory_stat_free_delta(remaining_delta, index);
    return;
  }

  carry_global_memory_stat_free_delta(remaining_delta, index);
}

PFS_account *sanitize_account(PFS_account *unsafe) {
  return global_account_container.sanitize(unsafe);
}

static void purge_account(PFS_thread *thread, PFS_account *account) {
  LF_PINS *pins = get_account_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    return;
  }

  PFS_account **entry;
  entry = reinterpret_cast<PFS_account **>(lf_hash_search(
      &account_hash, pins, &account->m_key, sizeof(account->m_key)));
  if (entry && (entry != MY_LF_ERRPTR)) {
    assert(*entry == account);
    if (account->get_refcount() == 0) {
      lf_hash_delete(&account_hash, pins, &account->m_key,
                     sizeof(account->m_key));
      account->aggregate(false, account->m_user, account->m_host);
      if (account->m_user != nullptr) {
        account->m_user->release();
        account->m_user = nullptr;
      }
      if (account->m_host != nullptr) {
        account->m_host->release();
        account->m_host = nullptr;
      }
      global_account_container.deallocate(account);
    }
  }

  lf_hash_search_unpin(pins);
}

class Proc_purge_account : public PFS_buffer_processor<PFS_account> {
 public:
  explicit Proc_purge_account(PFS_thread *thread) : m_thread(thread) {}

  void operator()(PFS_account *pfs) override {
    PFS_user *user = sanitize_user(pfs->m_user);
    PFS_host *host = sanitize_host(pfs->m_host);
    pfs->aggregate(true, user, host);

    if (pfs->get_refcount() == 0) {
      purge_account(m_thread, pfs);
    }
  }

 private:
  PFS_thread *m_thread;
};

/** Purge non connected accounts, reset stats of connected account. */
void purge_all_account() {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == nullptr)) {
    return;
  }

  Proc_purge_account proc(thread);
  global_account_container.apply(proc);
}

class Proc_update_accounts_derived_flags
    : public PFS_buffer_processor<PFS_account> {
 public:
  explicit Proc_update_accounts_derived_flags(PFS_thread *thread)
      : m_thread(thread) {}

  void operator()(PFS_account *pfs) override {
    if (pfs->m_key.m_user_name.length() > 0 &&
        pfs->m_key.m_host_name.length() > 0) {
      lookup_setup_actor(m_thread, &pfs->m_key.m_user_name,
                         &pfs->m_key.m_host_name, &pfs->m_enabled,
                         &pfs->m_history);
    } else {
      pfs->m_enabled = true;
      pfs->m_history = true;
    }
  }

 private:
  PFS_thread *m_thread;
};

void update_accounts_derived_flags(PFS_thread *thread) {
  Proc_update_accounts_derived_flags proc(thread);
  global_account_container.apply(proc);
}

/** @} */
