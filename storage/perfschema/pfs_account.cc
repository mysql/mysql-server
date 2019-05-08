/* Copyright (c) 2010, 2019, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_account.cc
  Performance schema account (implementation).
*/

#include "storage/perfschema/pfs_account.h"

#include "my_compiler.h"
#include "my_dbug.h"
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
void cleanup_account(void) { global_account_container.cleanup(); }

static const uchar *account_hash_get_key(const uchar *entry, size_t *length) {
  const PFS_account *const *typed_entry;
  const PFS_account *account;
  const void *result;
  typed_entry = reinterpret_cast<const PFS_account *const *>(entry);
  DBUG_ASSERT(typed_entry != NULL);
  account = *typed_entry;
  DBUG_ASSERT(account != NULL);
  *length = account->m_key.m_key_length;
  result = account->m_key.m_hash_key;
  return reinterpret_cast<const uchar *>(result);
}

/**
  Initialize the user hash.
  @return 0 on success
*/
int init_account_hash(const PFS_global_param *param) {
  if ((!account_hash_inited) && (param->m_account_sizing != 0)) {
    lf_hash_init(&account_hash, sizeof(PFS_account *), LF_HASH_UNIQUE, 0, 0,
                 account_hash_get_key, &my_charset_bin);
    account_hash_inited = true;
  }
  return 0;
}

/** Cleanup the user hash. */
void cleanup_account_hash(void) {
  if (account_hash_inited) {
    lf_hash_destroy(&account_hash);
    account_hash_inited = false;
  }
}

static LF_PINS *get_account_hash_pins(PFS_thread *thread) {
  if (unlikely(thread->m_account_hash_pins == NULL)) {
    if (!account_hash_inited) {
      return NULL;
    }
    thread->m_account_hash_pins = lf_hash_get_pins(&account_hash);
  }
  return thread->m_account_hash_pins;
}

static void set_account_key(PFS_account_key *key, const char *user,
                            uint user_length, const char *host,
                            uint host_length) {
  DBUG_ASSERT(user_length <= USERNAME_LENGTH);
  DBUG_ASSERT(host_length <= HOSTNAME_LENGTH);

  char *ptr = &key->m_hash_key[0];
  if (user_length > 0) {
    memcpy(ptr, user, user_length);
    ptr += user_length;
  }
  ptr[0] = 0;
  ptr++;
  if (host_length > 0) {
    memcpy(ptr, host, host_length);
    ptr += host_length;
  }
  ptr[0] = 0;
  ptr++;
  key->m_key_length = ptr - &key->m_hash_key[0];
}

PFS_account *find_or_create_account(PFS_thread *thread, const char *username,
                                    uint username_length, const char *hostname,
                                    uint hostname_length) {
  LF_PINS *pins = get_account_hash_pins(thread);
  if (unlikely(pins == NULL)) {
    global_account_container.m_lost++;
    return NULL;
  }

  PFS_account_key key;
  set_account_key(&key, username, username_length, hostname, hostname_length);

  PFS_account **entry;
  PFS_account *pfs;
  uint retry_count = 0;
  const uint retry_max = 3;
  pfs_dirty_state dirty_state;

search:
  entry = reinterpret_cast<PFS_account **>(
      lf_hash_search(&account_hash, pins, key.m_hash_key, key.m_key_length));
  if (entry && (entry != MY_LF_ERRPTR)) {
    pfs = *entry;
    pfs->inc_refcount();
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  pfs = global_account_container.allocate(&dirty_state);
  if (pfs != NULL) {
    pfs->m_key = key;
    if (username_length > 0) {
      pfs->m_username = &pfs->m_key.m_hash_key[0];
    } else {
      pfs->m_username = NULL;
    }
    pfs->m_username_length = username_length;

    if (hostname_length > 0) {
      pfs->m_hostname = &pfs->m_key.m_hash_key[username_length + 1];
    } else {
      pfs->m_hostname = NULL;
    }
    pfs->m_hostname_length = hostname_length;

    pfs->m_user = find_or_create_user(thread, username, username_length);
    pfs->m_host = find_or_create_host(thread, hostname, hostname_length);

    pfs->init_refcount();
    pfs->reset_stats();
    pfs->m_disconnected_count = 0;

    if (username_length > 0 && hostname_length > 0) {
      lookup_setup_actor(thread, username, username_length, hostname,
                         hostname_length, &pfs->m_enabled, &pfs->m_history);
    } else {
      pfs->m_enabled = true;
      pfs->m_history = true;
    }

    int res;
    pfs->m_lock.dirty_to_allocated(&dirty_state);
    res = lf_hash_insert(&account_hash, pins, &pfs);
    if (likely(res == 0)) {
      return pfs;
    }

    if (pfs->m_user) {
      pfs->m_user->release();
      pfs->m_user = NULL;
    }
    if (pfs->m_host) {
      pfs->m_host->release();
      pfs->m_host = NULL;
    }

    global_account_container.deallocate(pfs);

    if (res > 0) {
      if (++retry_count > retry_max) {
        global_account_container.m_lost++;
        return NULL;
      }
      goto search;
    }

    global_account_container.m_lost++;
    return NULL;
  }

  return NULL;
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
  if (read_instr_class_waits_stats() == NULL) {
    return;
  }

  if (likely(safe_user != NULL && safe_host != NULL)) {
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

  if (safe_user != NULL) {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME to:
      -  EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME
    */
    aggregate_all_event_names(write_instr_class_waits_stats(),
                              safe_user->write_instr_class_waits_stats());
    return;
  }

  if (safe_host != NULL) {
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
  return;
}

void PFS_account::aggregate_stages(PFS_user *safe_user, PFS_host *safe_host) {
  if (read_instr_class_stages_stats() == NULL) {
    return;
  }

  if (likely(safe_user != NULL && safe_host != NULL)) {
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

  if (safe_user != NULL) {
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

  if (safe_host != NULL) {
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
  return;
}

void PFS_account::aggregate_statements(PFS_user *safe_user,
                                       PFS_host *safe_host) {
  if (read_instr_class_statements_stats() == NULL) {
    return;
  }

  if (likely(safe_user != NULL && safe_host != NULL)) {
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

  if (safe_user != NULL) {
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

  if (safe_host != NULL) {
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
  return;
}

void PFS_account::aggregate_transactions(PFS_user *safe_user,
                                         PFS_host *safe_host) {
  if (read_instr_class_transactions_stats() == NULL) {
    return;
  }

  if (likely(safe_user != NULL && safe_host != NULL)) {
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

  if (safe_user != NULL) {
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

  if (safe_host != NULL) {
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
  return;
}

void PFS_account::aggregate_errors(PFS_user *safe_user, PFS_host *safe_host) {
  if (read_instr_class_errors_stats() == NULL) {
    return;
  }

  if (likely(safe_user != NULL && safe_host != NULL)) {
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

  if (safe_user != NULL) {
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

  if (safe_host != NULL) {
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
  return;
}

void PFS_account::aggregate_memory(bool alive, PFS_user *safe_user,
                                   PFS_host *safe_host) {
  if (read_instr_class_memory_stats() == NULL) {
    return;
  }

  if (likely(safe_user != NULL && safe_host != NULL)) {
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

  if (safe_user != NULL) {
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

  if (safe_host != NULL) {
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
  return;
}

void PFS_account::aggregate_status(PFS_user *safe_user, PFS_host *safe_host) {
  if (likely(safe_user != NULL && safe_host != NULL)) {
    /*
      Aggregate STATUS_BY_ACCOUNT to:
      - STATUS_BY_USER
      - STATUS_BY_HOST
    */
    safe_user->m_status_stats.aggregate(&m_status_stats);
    safe_host->m_status_stats.aggregate(&m_status_stats);
    m_status_stats.reset();
    return;
  }

  if (safe_user != NULL) {
    /*
      Aggregate STATUS_BY_ACCOUNT to:
      - STATUS_BY_USER
      - GLOBAL_STATUS
    */
    safe_user->m_status_stats.aggregate(&m_status_stats);
    m_status_stats.aggregate_to(&global_status_var);
    m_status_stats.reset();
    return;
  }

  if (safe_host != NULL) {
    /*
      Aggregate STATUS_BY_ACCOUNT to:
      - STATUS_BY_HOST
    */
    safe_host->m_status_stats.aggregate(&m_status_stats);
    m_status_stats.reset();
    return;
  }

  /*
    Aggregate STATUS_BY_ACCOUNT to:
    - GLOBAL_STATUS
  */
  m_status_stats.aggregate_to(&global_status_var);
  m_status_stats.reset();
  return;
}

void PFS_account::aggregate_stats(PFS_user *safe_user, PFS_host *safe_host) {
  if (likely(safe_user != NULL && safe_host != NULL)) {
    safe_user->m_disconnected_count += m_disconnected_count;
    safe_host->m_disconnected_count += m_disconnected_count;
    m_disconnected_count = 0;
    return;
  }

  if (safe_user != NULL) {
    safe_user->m_disconnected_count += m_disconnected_count;
    m_disconnected_count = 0;
    return;
  }

  if (safe_host != NULL) {
    safe_host->m_disconnected_count += m_disconnected_count;
    m_disconnected_count = 0;
    return;
  }

  m_disconnected_count = 0;
  return;
}

void PFS_account::release() { dec_refcount(); }

void PFS_account::rebase_memory_stats() {
  PFS_memory_shared_stat *stat = m_instr_class_memory_stats;
  PFS_memory_shared_stat *stat_last = stat + memory_class_max;
  for (; stat < stat_last; stat++) {
    stat->reset();
  }
}

void PFS_account::carry_memory_stat_delta(PFS_memory_stat_delta *delta,
                                          uint index) {
  PFS_memory_shared_stat *event_name_array;
  PFS_memory_shared_stat *stat;
  PFS_memory_stat_delta delta_buffer;
  PFS_memory_stat_delta *remaining_delta;

  event_name_array = write_instr_class_memory_stats();
  stat = &event_name_array[index];
  remaining_delta = stat->apply_delta(delta, &delta_buffer);

  if (remaining_delta == NULL) {
    return;
  }

  if (m_user != NULL) {
    m_user->carry_memory_stat_delta(remaining_delta, index);
    /* do not return, need to process m_host below */
  }

  if (m_host != NULL) {
    m_host->carry_memory_stat_delta(remaining_delta, index);
    return;
  }

  carry_global_memory_stat_delta(remaining_delta, index);
}

PFS_account *sanitize_account(PFS_account *unsafe) {
  return global_account_container.sanitize(unsafe);
}

static void purge_account(PFS_thread *thread, PFS_account *account) {
  LF_PINS *pins = get_account_hash_pins(thread);
  if (unlikely(pins == NULL)) {
    return;
  }

  PFS_account **entry;
  entry = reinterpret_cast<PFS_account **>(
      lf_hash_search(&account_hash, pins, account->m_key.m_hash_key,
                     account->m_key.m_key_length));
  if (entry && (entry != MY_LF_ERRPTR)) {
    DBUG_ASSERT(*entry == account);
    if (account->get_refcount() == 0) {
      lf_hash_delete(&account_hash, pins, account->m_key.m_hash_key,
                     account->m_key.m_key_length);
      account->aggregate(false, account->m_user, account->m_host);
      if (account->m_user != NULL) {
        account->m_user->release();
        account->m_user = NULL;
      }
      if (account->m_host != NULL) {
        account->m_host->release();
        account->m_host = NULL;
      }
      global_account_container.deallocate(account);
    }
  }

  lf_hash_search_unpin(pins);
}

class Proc_purge_account : public PFS_buffer_processor<PFS_account> {
 public:
  Proc_purge_account(PFS_thread *thread) : m_thread(thread) {}

  virtual void operator()(PFS_account *pfs) {
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
void purge_all_account(void) {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == NULL)) {
    return;
  }

  Proc_purge_account proc(thread);
  global_account_container.apply(proc);
}

class Proc_update_accounts_derived_flags
    : public PFS_buffer_processor<PFS_account> {
 public:
  Proc_update_accounts_derived_flags(PFS_thread *thread) : m_thread(thread) {}

  virtual void operator()(PFS_account *pfs) {
    if (pfs->m_username_length > 0 && pfs->m_hostname_length > 0) {
      lookup_setup_actor(m_thread, pfs->m_username, pfs->m_username_length,
                         pfs->m_hostname, pfs->m_hostname_length,
                         &pfs->m_enabled, &pfs->m_history);
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
