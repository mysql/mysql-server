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
  @file storage/perfschema/pfs_host.cc
  Performance schema host (implementation).
*/

#include "storage/perfschema/pfs_host.h"

#include "my_compiler.h"
#include "my_dbug.h"
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
void cleanup_host(void) { global_host_container.cleanup(); }

static const uchar *host_hash_get_key(const uchar *entry, size_t *length) {
  const PFS_host *const *typed_entry;
  const PFS_host *host;
  const void *result;
  typed_entry = reinterpret_cast<const PFS_host *const *>(entry);
  DBUG_ASSERT(typed_entry != NULL);
  host = *typed_entry;
  DBUG_ASSERT(host != NULL);
  *length = host->m_key.m_key_length;
  result = host->m_key.m_hash_key;
  return reinterpret_cast<const uchar *>(result);
}

/**
  Initialize the host hash.
  @return 0 on success
*/
int init_host_hash(const PFS_global_param *param) {
  if ((!host_hash_inited) && (param->m_host_sizing != 0)) {
    lf_hash_init(&host_hash, sizeof(PFS_host *), LF_HASH_UNIQUE, 0, 0,
                 host_hash_get_key, &my_charset_bin);
    host_hash_inited = true;
  }
  return 0;
}

/** Cleanup the host hash. */
void cleanup_host_hash(void) {
  if (host_hash_inited) {
    lf_hash_destroy(&host_hash);
    host_hash_inited = false;
  }
}

static LF_PINS *get_host_hash_pins(PFS_thread *thread) {
  if (unlikely(thread->m_host_hash_pins == NULL)) {
    if (!host_hash_inited) {
      return NULL;
    }
    thread->m_host_hash_pins = lf_hash_get_pins(&host_hash);
  }
  return thread->m_host_hash_pins;
}

static void set_host_key(PFS_host_key *key, const char *host,
                         uint host_length) {
  DBUG_ASSERT(host_length <= HOSTNAME_LENGTH);

  char *ptr = &key->m_hash_key[0];
  if (host_length > 0) {
    memcpy(ptr, host, host_length);
    ptr += host_length;
  }
  ptr[0] = 0;
  ptr++;
  key->m_key_length = ptr - &key->m_hash_key[0];
}

PFS_host *find_or_create_host(PFS_thread *thread, const char *hostname,
                              uint hostname_length) {
  LF_PINS *pins = get_host_hash_pins(thread);
  if (unlikely(pins == NULL)) {
    global_host_container.m_lost++;
    return NULL;
  }

  PFS_host_key key;
  set_host_key(&key, hostname, hostname_length);

  PFS_host **entry;
  PFS_host *pfs;
  uint retry_count = 0;
  const uint retry_max = 3;
  pfs_dirty_state dirty_state;

search:
  entry = reinterpret_cast<PFS_host **>(
      lf_hash_search(&host_hash, pins, key.m_hash_key, key.m_key_length));
  if (entry && (entry != MY_LF_ERRPTR)) {
    PFS_host *pfs;
    pfs = *entry;
    pfs->inc_refcount();
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  pfs = global_host_container.allocate(&dirty_state);
  if (pfs != NULL) {
    pfs->m_key = key;
    if (hostname_length > 0) {
      pfs->m_hostname = &pfs->m_key.m_hash_key[0];
    } else {
      pfs->m_hostname = NULL;
    }
    pfs->m_hostname_length = hostname_length;

    pfs->init_refcount();
    pfs->reset_stats();
    pfs->m_disconnected_count = 0;

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
        return NULL;
      }
      goto search;
    }

    global_host_container.m_lost++;
    return NULL;
  }

  return NULL;
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
  if (read_instr_class_stages_stats() == NULL) {
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
  if (read_instr_class_statements_stats() == NULL) {
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
  if (read_instr_class_transactions_stats() == NULL) {
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
  if (read_instr_class_errors_stats() == NULL) {
    return;
  }

  /*
    Aggregate EVENTS_ERRORS_SUMMARY_BY_HOST_BY_ERROR to:
    -  EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR
  */
  aggregate_all_errors(write_instr_class_errors_stats(), &global_error_stat);
}

void PFS_host::aggregate_memory(bool alive) {
  if (read_instr_class_memory_stats() == NULL) {
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
  m_disconnected_count = 0;
}

void PFS_host::release() { dec_refcount(); }

void PFS_host::rebase_memory_stats() {
  PFS_memory_shared_stat *stat = m_instr_class_memory_stats;
  PFS_memory_shared_stat *stat_last = stat + memory_class_max;
  for (; stat < stat_last; stat++) {
    stat->reset();
  }
}

void PFS_host::carry_memory_stat_delta(PFS_memory_stat_delta *delta,
                                       uint index) {
  PFS_memory_shared_stat *event_name_array;
  PFS_memory_shared_stat *stat;
  PFS_memory_stat_delta delta_buffer;
  PFS_memory_stat_delta *remaining_delta;

  event_name_array = write_instr_class_memory_stats();
  stat = &event_name_array[index];
  remaining_delta = stat->apply_delta(delta, &delta_buffer);

  if (remaining_delta != NULL) {
    carry_global_memory_stat_delta(remaining_delta, index);
  }
}

PFS_host *sanitize_host(PFS_host *unsafe) {
  return global_host_container.sanitize(unsafe);
}

static void purge_host(PFS_thread *thread, PFS_host *host) {
  LF_PINS *pins = get_host_hash_pins(thread);
  if (unlikely(pins == NULL)) {
    return;
  }

  PFS_host **entry;
  entry = reinterpret_cast<PFS_host **>(lf_hash_search(
      &host_hash, pins, host->m_key.m_hash_key, host->m_key.m_key_length));
  if (entry && (entry != MY_LF_ERRPTR)) {
    DBUG_ASSERT(*entry == host);
    if (host->get_refcount() == 0) {
      lf_hash_delete(&host_hash, pins, host->m_key.m_hash_key,
                     host->m_key.m_key_length);
      host->aggregate(false);
      global_host_container.deallocate(host);
    }
  }

  lf_hash_search_unpin(pins);
}

class Proc_purge_host : public PFS_buffer_processor<PFS_host> {
 public:
  Proc_purge_host(PFS_thread *thread) : m_thread(thread) {}

  virtual void operator()(PFS_host *pfs) {
    pfs->aggregate(true);
    if (pfs->get_refcount() == 0) {
      purge_host(m_thread, pfs);
    }
  }

 private:
  PFS_thread *m_thread;
};

/** Purge non connected hosts, reset stats of connected hosts. */
void purge_all_host(void) {
  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == NULL)) {
    return;
  }

  Proc_purge_host proc(thread);
  global_host_container.apply(proc);
}

/** @} */
