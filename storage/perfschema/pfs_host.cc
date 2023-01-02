/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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
  @file storage/perfschema/pfs_host.cc
  Performance schema host (implementation).
*/

#include "storage/perfschema/pfs_host.h"

#include <assert.h>
#include "my_compiler.h"

#include "my_sys.h"
#include "sql/mysqld.h"  // global_status_var
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

LF_HASH host_hash;
static bool host_hash_inited = false;

/**
  Initialize the host buffers.
  @param param                        sizing parameters
  @return 0 on success
*/
int init_host(const PFS_global_param *param) {
  if (global_host_container.init(param->m_host_sizing)) {
    return 1;
  }

  return 0;
}

/** Cleanup all the host buffers. */
void cleanup_host() { global_host_container.cleanup(); }

static const uchar *host_hash_get_key(const uchar *entry, size_t *length) {
  const PFS_host *const *typed_entry;
  const PFS_host *host;
  const void *result;
  typed_entry = reinterpret_cast<const PFS_host *const *>(entry);
  assert(typed_entry != nullptr);
  host = *typed_entry;
  assert(host != nullptr);
  *length = sizeof(host->m_key);
  result = &host->m_key;
  return reinterpret_cast<const uchar *>(result);
}

static uint host_hash_func(const LF_HASH *, const uchar *key,
                           size_t key_len [[maybe_unused]]) {
  const PFS_host_key *host_key;
  uint64 nr1;
  uint64 nr2;

  assert(key_len == sizeof(PFS_host_key));
  host_key = reinterpret_cast<const PFS_host_key *>(key);
  assert(host_key != nullptr);

  nr1 = 0;
  nr2 = 0;

  host_key->m_host_name.hash(&nr1, &nr2);

  return nr1;
}

static int host_hash_cmp_func(const uchar *key1,
                              size_t key_len1 [[maybe_unused]],
                              const uchar *key2,
                              size_t key_len2 [[maybe_unused]]) {
  const PFS_host_key *host_key1;
  const PFS_host_key *host_key2;
  int cmp;

  assert(key_len1 == sizeof(PFS_host_key));
  assert(key_len2 == sizeof(PFS_host_key));
  host_key1 = reinterpret_cast<const PFS_host_key *>(key1);
  host_key2 = reinterpret_cast<const PFS_host_key *>(key2);
  assert(host_key1 != nullptr);
  assert(host_key2 != nullptr);

  cmp = host_key1->m_host_name.sort(&host_key2->m_host_name);
  return cmp;
}

/**
  Initialize the host hash.
  @return 0 on success
*/
int init_host_hash(const PFS_global_param *param) {
  if ((!host_hash_inited) && (param->m_host_sizing != 0)) {
    lf_hash_init3(&host_hash, sizeof(PFS_host *), LF_HASH_UNIQUE,
                  host_hash_get_key, host_hash_func, host_hash_cmp_func,
                  nullptr /* ctor */, nullptr /* dtor */, nullptr /* init */);
    host_hash_inited = true;
  }
  return 0;
}

/** Cleanup the host hash. */
void cleanup_host_hash() {
  if (host_hash_inited) {
    lf_hash_destroy(&host_hash);
    host_hash_inited = false;
  }
}

static LF_PINS *get_host_hash_pins(PFS_thread *thread) {
  if (unlikely(thread->m_host_hash_pins == nullptr)) {
    if (!host_hash_inited) {
      return nullptr;
    }
    thread->m_host_hash_pins = lf_hash_get_pins(&host_hash);
  }
  return thread->m_host_hash_pins;
}

static void set_host_key(PFS_host_key *key, const PFS_host_name *host) {
  key->m_host_name = *host;
}

PFS_host *find_or_create_host(PFS_thread *thread, const PFS_host_name *host) {
  LF_PINS *pins = get_host_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    global_host_container.m_lost++;
    return nullptr;
  }

  PFS_host_key key;
  set_host_key(&key, host);

  PFS_host **entry;
  PFS_host *pfs;
  uint retry_count = 0;
  const uint retry_max = 3;
  pfs_dirty_state dirty_state;

search:
  entry = reinterpret_cast<PFS_host **>(
      lf_hash_search(&host_hash, pins, &key, sizeof(key)));
  if (entry && (entry != MY_LF_ERRPTR)) {
    pfs = *entry;
    pfs->inc_refcount();
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  pfs = global_host_container.allocate(&dirty_state);
  if (pfs != nullptr) {
    pfs->m_key = key;

    pfs->init_refcount();
    pfs->reset_stats();
    pfs->reset_connections_stats();

    int res;
    pfs->m_lock.dirty_to_allocated(&dirty_state);
    res = lf_hash_insert(&host_hash, pins, &pfs);
    if (likely(res == 0)) {
      return pfs;
    }

    global_host_container.deallocate(pfs);

    if (res > 0) {
      if (++retry_count > retry_max) {
        global_host_container.m_lost++;
        return nullptr;
      }
      goto search;
    }

    global_host_container.m_lost++;
    return nullptr;
  }

  return nullptr;
}

void PFS_host::aggregate(bool alive) {
  aggregate_waits();
  aggregate_stages();
  aggregate_statements();
  aggregate_transactions();
  aggregate_errors();
  aggregate_memory(alive);
  aggregate_status();
  aggregate_stats();
}

void PFS_host::aggregate_waits() {
  /* No parent to aggregate to, clean the stats */
  reset_waits_stats();
}

void PFS_host::aggregate_stages() {
  if (read_instr_class_stages_stats() == nullptr) {
    return;
  }

  /*
    Aggregate EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME to:
    -  EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME
  */
  aggregate_all_stages(write_instr_class_stages_stats(),
                       global_instr_class_stages_array);
}

void PFS_host::aggregate_statements() {
  if (read_instr_class_statements_stats() == nullptr) {
    return;
  }

  /*
    Aggregate EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME to:
    -  EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME
  */
  aggregate_all_statements(write_instr_class_statements_stats(),
                           global_instr_class_statements_array);
}

void PFS_host::aggregate_transactions() {
  if (read_instr_class_transactions_stats() == nullptr) {
    return;
  }

  /*
    Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_HOST_BY_EVENT_NAME to:
    -  EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME
  */
  aggregate_all_transactions(write_instr_class_transactions_stats(),
                             &global_transaction_stat);
}

void PFS_host::aggregate_errors() {
  if (read_instr_class_errors_stats() == nullptr) {
    return;
  }

  /*
    Aggregate EVENTS_ERRORS_SUMMARY_BY_HOST_BY_ERROR to:
    -  EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR
  */
  aggregate_all_errors(write_instr_class_errors_stats(), &global_error_stat);
}

void PFS_host::aggregate_memory(bool alive) {
  if (read_instr_class_memory_stats() == nullptr) {
    return;
  }

  /*
    Aggregate MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME to:
    - MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME
  */
  aggregate_all_memory(alive, write_instr_class_memory_stats(),
                       global_instr_class_memory_array);
}

void PFS_host::aggregate_status() {
  /* No parent to aggregate to, clean the stats */
  m_status_stats.reset();
}

void PFS_host::aggregate_stats() {
  /* No parent to aggregate to, clean the stats */
  reset_connections_stats();
}

void PFS_host::aggregate_stats_from(PFS_account *pfs) {
  m_disconnected_count += pfs->m_disconnected_count;

  if (m_max_controlled_memory < pfs->m_max_controlled_memory) {
    m_max_controlled_memory = pfs->m_max_controlled_memory;
  }

  if (m_max_total_memory < pfs->m_max_total_memory) {
    m_max_total_memory = pfs->m_max_total_memory;
  }
}

void PFS_host::aggregate_disconnect(ulonglong controlled_memory,
                                    ulonglong total_memory) {
  m_disconnected_count++;

  if (m_max_controlled_memory < controlled_memory) {
    m_max_controlled_memory = controlled_memory;
  }

  if (m_max_total_memory < total_memory) {
    m_max_total_memory = total_memory;
  }
}

void PFS_host::release() { dec_refcount(); }

void PFS_host::rebase_memory_stats() {
  PFS_memory_shared_stat *stat = m_instr_class_memory_stats;
  PFS_memory_shared_stat *stat_last = stat + memory_class_max;
  for (; stat < stat_last; stat++) {
    stat->reset();
  }
}

void PFS_host::carry_memory_stat_alloc_delta(PFS_memory_stat_alloc_delta *delta,
                                             uint index) {
  PFS_memory_shared_stat *event_name_array;
  PFS_memory_shared_stat *stat;
  PFS_memory_stat_alloc_delta delta_buffer;
  PFS_memory_stat_alloc_delta *remaining_delta;

  event_name_array = write_instr_class_memory_stats();
  stat = &event_name_array[index];
  remaining_delta = stat->apply_alloc_delta(delta, &delta_buffer);

  if (remaining_delta != nullptr) {
    carry_global_memory_stat_alloc_delta(remaining_delta, index);
  }
}

void PFS_host::carry_memory_stat_free_delta(PFS_memory_stat_free_delta *delta,
                                            uint index) {
  PFS_memory_shared_stat *event_name_array;
  PFS_memory_shared_stat *stat;
  PFS_memory_stat_free_delta delta_buffer;
  PFS_memory_stat_free_delta *remaining_delta;

  event_name_array = write_instr_class_memory_stats();
  stat = &event_name_array[index];
  remaining_delta = stat->apply_free_delta(delta, &delta_buffer);

  if (remaining_delta != nullptr) {
    carry_global_memory_stat_free_delta(remaining_delta, index);
  }
}

PFS_host *sanitize_host(PFS_host *unsafe) {
  return global_host_container.sanitize(unsafe);
}

static void purge_host(PFS_thread *thread, PFS_host *host) {
  LF_PINS *pins = get_host_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    return;
  }

  PFS_host **entry;
  entry = reinterpret_cast<PFS_host **>(
      lf_hash_search(&host_hash, pins, &host->m_key, sizeof(host->m_key)));
  if (entry && (entry != MY_LF_ERRPTR)) {
    assert(*entry == host);
    if (host->get_refcount() == 0) {
      lf_hash_delete(&host_hash, pins, &host->m_key, sizeof(host->m_key));
      host->aggregate(false);
      global_host_container.deallocate(host);
    }
  }

  lf_hash_search_unpin(pins);
}

class Proc_purge_host : public PFS_buffer_processor<PFS_host> {
 public:
  explicit Proc_purge_host(PFS_thread *thread) : m_thread(thread) {}

  void operator()(PFS_host *pfs) override {
    pfs->aggregate(true);
    if (pfs->get_refcount() == 0) {
      purge_host(m_thread, pfs);
    }
  }

 private:
  PFS_thread *m_thread;
};

/** Purge non connected hosts, reset stats of connected hosts. */
void purge_all_host() {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == nullptr)) {
    return;
  }

  Proc_purge_host proc(thread);
  global_host_container.apply(proc);
}

/** @} */
