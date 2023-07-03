/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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
  @file storage/perfschema/pfs_instr.cc
  Performance schema instruments (implementation).
*/

#include "storage/perfschema/pfs_instr.h"

#include <assert.h>
#include <string.h>
#include <atomic>

#include "my_compiler.h"

#include "my_psi_config.h"
#include "my_sys.h"
#include "sql/mysqld.h"  // get_thd_status_var
#include "storage/perfschema/pfs.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_builtin_memory.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_stat.h"
#include "storage/perfschema/pfs_user.h"

ulong nested_statement_lost = 0;

/**
  @addtogroup performance_schema_buffers
  @{
*/

/**
  Size of the file handle array. @sa file_handle_array.
  Signed value, for easier comparisons with a file descriptor number.
*/
long file_handle_max = 0;
/** True when @c file_handle_array is full. */
bool file_handle_full;
/** Number of file handle lost. @sa file_handle_array */
ulong file_handle_lost = 0;
/** Number of EVENTS_WAITS_HISTORY records per thread. */
ulong events_waits_history_per_thread = 0;
/** Number of EVENTS_STAGES_HISTORY records per thread. */
ulong events_stages_history_per_thread = 0;
/** Number of EVENTS_STATEMENTS_HISTORY records per thread. */
ulong events_statements_history_per_thread = 0;
uint statement_stack_max = 0;
size_t pfs_max_digest_length = 0;
size_t pfs_max_sqltext = 0;
/** Number of locker lost. @sa LOCKER_STACK_SIZE. */
ulong locker_lost = 0;
/** Number of statements lost. @sa STATEMENT_STACK_SIZE. */
ulong statement_lost = 0;
/** Size of connection attribute storage per thread */
ulong session_connect_attrs_size_per_thread;
/** Longest connection attributes string seen so far, pre-truncation */
ulong session_connect_attrs_longest_seen = 0;
/** Number of connection attributes lost */
ulong session_connect_attrs_lost = 0;

/** Number of EVENTS_TRANSACTIONS_HISTORY records per thread. */
ulong events_transactions_history_per_thread = 0;

/**
  File instrumentation handle array.
  @sa file_handle_max
  @sa file_handle_lost
*/
PFS_file **file_handle_array = nullptr;

PFS_stage_stat *global_instr_class_stages_array = nullptr;
PFS_statement_stat *global_instr_class_statements_array = nullptr;
PFS_histogram global_statements_histogram;
std::atomic<PFS_memory_shared_stat *> global_instr_class_memory_array{nullptr};

static PFS_cacheline_atomic_uint64 thread_internal_id_counter;

/** Hash table for instrumented files. */
LF_HASH filename_hash;
/** True if filename_hash is initialized. */
static bool filename_hash_inited = false;

/**
  Initialize all the instruments instance buffers.
  @param param                        sizing parameters
  @return 0 on success
*/
int init_instruments(const PFS_global_param *param) {
  uint index;

  /* Make sure init_event_name_sizing is called */
  assert(wait_class_max != 0);

  file_handle_max = param->m_file_handle_sizing;
  file_handle_full = false;
  file_handle_lost = 0;

  pfs_max_digest_length = param->m_max_digest_length;
  pfs_max_sqltext = param->m_max_sql_text_length;

  events_waits_history_per_thread = param->m_events_waits_history_sizing;

  events_stages_history_per_thread = param->m_events_stages_history_sizing;

  events_statements_history_per_thread =
      param->m_events_statements_history_sizing;

  statement_stack_max = param->m_statement_stack_sizing;

  events_transactions_history_per_thread =
      param->m_events_transactions_history_sizing;

  session_connect_attrs_size_per_thread = param->m_session_connect_attrs_sizing;
  session_connect_attrs_lost = 0;

  file_handle_array = nullptr;

  thread_internal_id_counter.m_u64.store(0);

  if (global_mutex_container.init(param->m_mutex_sizing)) {
    return 1;
  }

  if (global_rwlock_container.init(param->m_rwlock_sizing)) {
    return 1;
  }

  if (global_cond_container.init(param->m_cond_sizing)) {
    return 1;
  }

  if (global_file_container.init(param->m_file_sizing)) {
    return 1;
  }

  if (file_handle_max > 0) {
    file_handle_array =
        PFS_MALLOC_ARRAY(&builtin_memory_file_handle, file_handle_max,
                         sizeof(PFS_file *), PFS_file *, MYF(MY_ZEROFILL));
    if (unlikely(file_handle_array == nullptr)) {
      return 1;
    }
  }

  if (global_table_container.init(param->m_table_sizing)) {
    return 1;
  }

  if (global_socket_container.init(param->m_socket_sizing)) {
    return 1;
  }

  if (global_mdl_container.init(param->m_metadata_lock_sizing)) {
    return 1;
  }

  if (global_thread_container.init(param->m_thread_sizing)) {
    return 1;
  }

  if (stage_class_max > 0) {
    global_instr_class_stages_array = PFS_MALLOC_ARRAY(
        &builtin_memory_global_stages, stage_class_max, sizeof(PFS_stage_stat),
        PFS_stage_stat, MYF(MY_ZEROFILL));
    if (unlikely(global_instr_class_stages_array == nullptr)) {
      return 1;
    }

    for (index = 0; index < stage_class_max; index++) {
      global_instr_class_stages_array[index].reset();
    }
  }

  if (statement_class_max > 0) {
    global_instr_class_statements_array = PFS_MALLOC_ARRAY(
        &builtin_memory_global_statements, statement_class_max,
        sizeof(PFS_statement_stat), PFS_statement_stat, MYF(MY_ZEROFILL));
    if (unlikely(global_instr_class_statements_array == nullptr)) {
      return 1;
    }

    for (index = 0; index < statement_class_max; index++) {
      global_instr_class_statements_array[index].reset();
    }
  }

  if (memory_class_max > 0) {
    global_instr_class_memory_array =
        PFS_MALLOC_ARRAY(&builtin_memory_global_memory, memory_class_max,
                         sizeof(PFS_memory_shared_stat), PFS_memory_shared_stat,
                         MYF(MY_ZEROFILL));
    if (unlikely(global_instr_class_memory_array.load() == nullptr)) {
      return 1;
    }

    for (index = 0; index < memory_class_max; index++) {
      global_instr_class_memory_array[index].reset();
    }
  }

  return 0;
}

/** Cleanup all the instruments buffers. */
void cleanup_instruments() {
  global_mutex_container.cleanup();
  global_rwlock_container.cleanup();
  global_cond_container.cleanup();
  global_file_container.cleanup();

  PFS_FREE_ARRAY(&builtin_memory_file_handle, file_handle_max,
                 sizeof(PFS_file *), file_handle_array);
  file_handle_array = nullptr;
  file_handle_max = 0;

  global_table_container.cleanup();
  global_socket_container.cleanup();
  global_mdl_container.cleanup();
  global_thread_container.cleanup();

  PFS_FREE_ARRAY(&builtin_memory_global_stages, stage_class_max,
                 sizeof(PFS_stage_stat), global_instr_class_stages_array);
  global_instr_class_stages_array = nullptr;

  PFS_FREE_ARRAY(&builtin_memory_global_statements, statement_class_max,
                 sizeof(PFS_statement_stat),
                 global_instr_class_statements_array);
  global_instr_class_statements_array = nullptr;

  PFS_FREE_ARRAY(&builtin_memory_global_memory, memory_class_max,
                 sizeof(PFS_memory_shared_stat),
                 global_instr_class_memory_array);
  global_instr_class_memory_array = nullptr;
}

/** Get hash table key for instrumented files. */
static const uchar *filename_hash_get_key(const uchar *entry, size_t *length) {
  const PFS_file *const *typed_entry;
  const PFS_file *file;
  const void *result;
  typed_entry = reinterpret_cast<const PFS_file *const *>(entry);
  assert(typed_entry != nullptr);
  file = *typed_entry;
  assert(file != nullptr);
  *length = sizeof(file->m_file_name);
  result = &file->m_file_name;
  return reinterpret_cast<const uchar *>(result);
}

static uint filename_hash_func(const LF_HASH *, const uchar *key,
                               size_t key_len [[maybe_unused]]) {
  const PFS_file_name *file_name_key;
  uint64 nr1;
  uint64 nr2;

  assert(key_len == sizeof(PFS_file_name));
  file_name_key = reinterpret_cast<const PFS_file_name *>(key);
  assert(file_name_key != nullptr);

  nr1 = 0;
  nr2 = 0;

  file_name_key->hash(&nr1, &nr2);

  return nr1;
}

static int filename_hash_cmp_func(const uchar *key1,
                                  size_t key_len1 [[maybe_unused]],
                                  const uchar *key2,
                                  size_t key_len2 [[maybe_unused]]) {
  const PFS_file_name *file_name_key1;
  const PFS_file_name *file_name_key2;
  int cmp;

  assert(key_len1 == sizeof(PFS_file_name));
  assert(key_len2 == sizeof(PFS_file_name));
  file_name_key1 = reinterpret_cast<const PFS_file_name *>(key1);
  file_name_key2 = reinterpret_cast<const PFS_file_name *>(key2);
  assert(file_name_key1 != nullptr);
  assert(file_name_key2 != nullptr);

  cmp = file_name_key1->sort(file_name_key2);
  return cmp;
}

/**
  Initialize the file name hash.
  @return 0 on success
*/
int init_file_hash(const PFS_global_param *param) {
  if ((!filename_hash_inited) && (param->m_file_sizing != 0)) {
    lf_hash_init3(&filename_hash, sizeof(PFS_file *), LF_HASH_UNIQUE,
                  filename_hash_get_key, filename_hash_func,
                  filename_hash_cmp_func, nullptr, nullptr, nullptr);
    filename_hash_inited = true;
  }
  return 0;
}

/** Cleanup the file name hash. */
void cleanup_file_hash() {
  if (filename_hash_inited) {
    lf_hash_destroy(&filename_hash);
    filename_hash_inited = false;
  }
}

/**
  Create instrumentation for a mutex instance.
  @param klass                        the mutex class
  @param identity                     the mutex address
  @return a mutex instance, or NULL
*/
PFS_mutex *create_mutex(PFS_mutex_class *klass, const void *identity) {
  PFS_mutex *pfs;
  pfs_dirty_state dirty_state;
  unsigned int partition;

  /*
    There are 9 volatility defined in psi.h,
    but since most are still unused,
    mapping this to only 2 PFS_MUTEX_PARTITIONS.
  */
  if (klass->m_volatility >= PSI_VOLATILITY_SESSION) {
    partition = 1;
  } else {
    partition = 0;
  }

  pfs = global_mutex_container.allocate(&dirty_state, partition);
  if (pfs != nullptr) {
    pfs->m_identity = identity;
    pfs->m_class = klass;
    pfs->m_enabled = klass->m_enabled && flag_global_instrumentation;
    pfs->m_timed = klass->m_timed;
    pfs->m_mutex_stat.reset();
    pfs->m_owner = nullptr;
#ifdef LATER_WL2333
    pfs->m_last_locked = 0;
#endif /* LATER_WL2333 */
    pfs->m_lock.dirty_to_allocated(&dirty_state);
    if (klass->is_singleton()) {
      klass->m_singleton = pfs;
    }
  }

  return pfs;
}

/**
  Destroy instrumentation for a mutex instance.
  @param pfs                          the mutex to destroy
*/
void destroy_mutex(PFS_mutex *pfs) {
  assert(pfs != nullptr);
  PFS_mutex_class *klass = pfs->m_class;
  /* Aggregate to EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME */
  klass->m_mutex_stat.aggregate(&pfs->m_mutex_stat);
  pfs->m_mutex_stat.reset();
  if (klass->is_singleton()) {
    klass->m_singleton = nullptr;
  }

  global_mutex_container.deallocate(pfs);
}

/**
  Create instrumentation for a rwlock instance.
  @param klass                        the rwlock class
  @param identity                     the rwlock address
  @return a rwlock instance, or NULL
*/
PFS_rwlock *create_rwlock(PFS_rwlock_class *klass, const void *identity) {
  PFS_rwlock *pfs;
  pfs_dirty_state dirty_state;

  pfs = global_rwlock_container.allocate(&dirty_state);
  if (pfs != nullptr) {
    pfs->m_identity = identity;
    pfs->m_class = klass;
    pfs->m_enabled = klass->m_enabled && flag_global_instrumentation;
    pfs->m_timed = klass->m_timed;
    pfs->m_rwlock_stat.reset();
    pfs->m_writer = nullptr;
    pfs->m_readers = 0;
#ifdef LATER_WL2333
    pfs->m_last_written = 0;
    pfs->m_last_read = 0;
#endif /* LATER_WL2333 */
    pfs->m_lock.dirty_to_allocated(&dirty_state);
    if (klass->is_singleton()) {
      klass->m_singleton = pfs;
    }
  }

  return pfs;
}

/**
  Destroy instrumentation for a rwlock instance.
  @param pfs                          the rwlock to destroy
*/
void destroy_rwlock(PFS_rwlock *pfs) {
  assert(pfs != nullptr);
  PFS_rwlock_class *klass = pfs->m_class;
  /* Aggregate to EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME */
  klass->m_rwlock_stat.aggregate(&pfs->m_rwlock_stat);
  pfs->m_rwlock_stat.reset();
  if (klass->is_singleton()) {
    klass->m_singleton = nullptr;
  }

  global_rwlock_container.deallocate(pfs);
}

/**
  Create instrumentation for a condition instance.
  @param klass                        the condition class
  @param identity                     the condition address
  @return a condition instance, or NULL
*/
PFS_cond *create_cond(PFS_cond_class *klass, const void *identity) {
  PFS_cond *pfs;
  pfs_dirty_state dirty_state;

  pfs = global_cond_container.allocate(&dirty_state);
  if (pfs != nullptr) {
    pfs->m_identity = identity;
    pfs->m_class = klass;
    pfs->m_enabled = klass->m_enabled && flag_global_instrumentation;
    pfs->m_timed = klass->m_timed;
    pfs->m_cond_stat.reset();
    pfs->m_lock.dirty_to_allocated(&dirty_state);
    if (klass->is_singleton()) {
      klass->m_singleton = pfs;
    }
  }

  return pfs;
}

/**
  Destroy instrumentation for a condition instance.
  @param pfs                          the condition to destroy
*/
void destroy_cond(PFS_cond *pfs) {
  assert(pfs != nullptr);
  PFS_cond_class *klass = pfs->m_class;
  /* Aggregate to EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME */
  klass->m_cond_stat.aggregate(&pfs->m_cond_stat);
  pfs->m_cond_stat.reset();
  if (klass->is_singleton()) {
    klass->m_singleton = nullptr;
  }

  global_cond_container.deallocate(pfs);
}

PFS_thread *PFS_thread::get_current_thread() { return THR_PFS; }

void PFS_thread::reset_session_connect_attrs() {
  m_session_connect_attrs_length = 0;
  m_session_connect_attrs_cs_number = 0;

  if ((m_session_connect_attrs != nullptr) &&
      (session_connect_attrs_size_per_thread > 0)) {
    /* Do not keep user data */
    memset(m_session_connect_attrs, 0, session_connect_attrs_size_per_thread);
  }
}

void PFS_thread::set_history_derived_flags() {
  if (m_history) {
    m_flag_events_waits_history = flag_events_waits_history;
    m_flag_events_waits_history_long = flag_events_waits_history_long;
    m_flag_events_stages_history = flag_events_stages_history;
    m_flag_events_stages_history_long = flag_events_stages_history_long;
    m_flag_events_statements_history = flag_events_statements_history;
    m_flag_events_statements_history_long = flag_events_statements_history_long;
    m_flag_events_transactions_history = flag_events_transactions_history;
    m_flag_events_transactions_history_long =
        flag_events_transactions_history_long;
  } else {
    m_flag_events_waits_history = false;
    m_flag_events_waits_history_long = false;
    m_flag_events_stages_history = false;
    m_flag_events_stages_history_long = false;
    m_flag_events_statements_history = false;
    m_flag_events_statements_history_long = false;
    m_flag_events_transactions_history = false;
    m_flag_events_transactions_history_long = false;
  }
}

void PFS_thread::rebase_memory_stats() {
  PFS_memory_safe_stat *stat = m_instr_class_memory_stats;
  PFS_memory_safe_stat *stat_last = stat + memory_class_max;
  for (; stat < stat_last; stat++) {
    stat->reset();
  }
}

void PFS_thread::carry_memory_stat_alloc_delta(
    PFS_memory_stat_alloc_delta *delta, uint index) {
  if (m_account != nullptr) {
    m_account->carry_memory_stat_alloc_delta(delta, index);
    return;
  }

  if (m_user != nullptr) {
    m_user->carry_memory_stat_alloc_delta(delta, index);
    /* do not return, need to process m_host below */
  }

  if (m_host != nullptr) {
    m_host->carry_memory_stat_alloc_delta(delta, index);
    return;
  }

  carry_global_memory_stat_alloc_delta(delta, index);
}

void PFS_thread::carry_memory_stat_free_delta(PFS_memory_stat_free_delta *delta,
                                              uint index) {
  if (m_account != nullptr) {
    m_account->carry_memory_stat_free_delta(delta, index);
    return;
  }

  if (m_user != nullptr) {
    m_user->carry_memory_stat_free_delta(delta, index);
    /* do not return, need to process m_host below */
  }

  if (m_host != nullptr) {
    m_host->carry_memory_stat_free_delta(delta, index);
    return;
  }

  carry_global_memory_stat_free_delta(delta, index);
}

void carry_global_memory_stat_alloc_delta(PFS_memory_stat_alloc_delta *delta,
                                          uint index) {
  PFS_memory_shared_stat *stat;
  PFS_memory_stat_alloc_delta delta_buffer;

  stat = &global_instr_class_memory_array[index];
  (void)stat->apply_alloc_delta(delta, &delta_buffer);
}

void carry_global_memory_stat_free_delta(PFS_memory_stat_free_delta *delta,
                                         uint index) {
  PFS_memory_shared_stat *stat;
  PFS_memory_stat_free_delta delta_buffer;

  stat = &global_instr_class_memory_array[index];
  (void)stat->apply_free_delta(delta, &delta_buffer);
}

void PFS_thread::mem_cnt_alloc(size_t size) {
#ifndef NDEBUG
  thd_mem_cnt_alloc(m_cnt_thd, size, current_key_name);
#else
  thd_mem_cnt_alloc(m_cnt_thd, size);
#endif
}

void PFS_thread::mem_cnt_free(size_t size) {
  thd_mem_cnt_free(m_cnt_thd, size);
}

/**
  Create instrumentation for a thread instance.
  @param klass                        the thread class
  @param seqnum                       the thread instance sequence number
  @param identity                     the thread address,
    or a value characteristic of this thread
  @param processlist_id               the PROCESSLIST id,
    or 0 if unknown
  @return a thread instance, or NULL
*/
PFS_thread *create_thread(PFS_thread_class *klass, PSI_thread_seqnum seqnum,
                          const void *identity [[maybe_unused]],
                          ulonglong processlist_id) {
  PFS_thread *pfs;
  pfs_dirty_state dirty_state;

  pfs = global_thread_container.allocate(&dirty_state);
  if (pfs != nullptr) {
    pfs->m_thread_internal_id = thread_internal_id_counter.m_u64++;
    pfs->m_parent_thread_internal_id = 0;
    pfs->m_processlist_id = static_cast<ulong>(processlist_id);
    pfs->m_thread_os_id = 0;
    pfs->m_system_thread = !(klass->m_flags & PSI_FLAG_USER);
    pfs->m_event_id = 1;
    pfs->m_stmt_lock.set_allocated();
    pfs->m_session_lock.set_allocated();
    pfs->set_enabled(klass->m_enabled);
    pfs->set_history(klass->m_history);
    pfs->m_class = klass;
    pfs->m_events_waits_current = &pfs->m_events_waits_stack[WAIT_STACK_BOTTOM];
    pfs->m_waits_history_full = false;
    pfs->m_waits_history_index = 0;
    pfs->m_stages_history_full = false;
    pfs->m_stages_history_index = 0;
    pfs->m_statements_history_full = false;
    pfs->m_statements_history_index = 0;
    pfs->m_transactions_history_full = false;
    pfs->m_transactions_history_index = 0;

    pfs->reset_stats();
    pfs->reset_session_connect_attrs();

    pfs->m_filename_hash_pins = nullptr;
    pfs->m_table_share_hash_pins = nullptr;
    pfs->m_setup_actor_hash_pins = nullptr;
    pfs->m_setup_object_hash_pins = nullptr;
    pfs->m_user_hash_pins = nullptr;
    pfs->m_account_hash_pins = nullptr;
    pfs->m_host_hash_pins = nullptr;
    pfs->m_digest_hash_pins = nullptr;
    pfs->m_program_hash_pins = nullptr;

    pfs->m_user_name.reset();
    pfs->m_host_name.reset();
    pfs->m_db_name.reset();
    pfs->m_groupname_length = 0;
    pfs->m_user_data = nullptr;
    pfs->m_command = 0;
    pfs->m_start_time = 0;
    pfs->m_stage = 0;
    pfs->m_stage_progress = nullptr;
    pfs->m_processlist_info[0] = '\0';
    pfs->m_processlist_info_length = 0;
    pfs->m_secondary = false;
    pfs->m_connection_type = NO_VIO_TYPE;

    pfs->m_thd = nullptr;
    pfs->m_cnt_thd = nullptr;
#ifndef NDEBUG
    pfs->current_key_name = nullptr;
#endif
    pfs->m_host = nullptr;
    pfs->m_user = nullptr;
    pfs->m_account = nullptr;
    set_thread_account(pfs);

    pfs->m_peer_port = 0;
    pfs->m_sock_addr_len = 0;

    /*
      For child waits, by default,
      - NESTING_EVENT_ID is NULL
      - NESTING_EVENT_TYPE is NULL
    */
    PFS_events_waits *child_wait = &pfs->m_events_waits_stack[0];
    child_wait->m_event_id = 0;

    /*
      For child stages, by default,
      - NESTING_EVENT_ID is NULL
      - NESTING_EVENT_TYPE is NULL
    */
    PFS_events_stages *child_stage = &pfs->m_stage_current;
    child_stage->m_nesting_event_id = 0;

    /* No current stage. */
    child_stage->m_class = nullptr;

    pfs->m_events_statements_count = 0;
    pfs->m_transaction_current.m_event_id = 0;

    if (klass->is_singleton()) {
#if 0
      /* See destroy_thread() */
      assert(klass->m_singleton == nullptr);
#endif
      klass->m_singleton = pfs;
    }

    if (klass->has_seqnum()) {
      if (klass->has_auto_seqnum()) {
        seqnum = klass->m_seqnum++;
      }

      /* Possible truncation */
      (void)snprintf(pfs->m_os_name, PFS_MAX_OS_NAME_LENGTH - 1,
                     klass->m_os_name, seqnum);
    } else {
      (void)snprintf(pfs->m_os_name, PFS_MAX_OS_NAME_LENGTH, "%s",
                     klass->m_os_name);
    }
    pfs->m_os_name[PFS_MAX_OS_NAME_LENGTH - 1] = '\0';

    pfs->m_session_all_memory_stat.reset();

#ifdef HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE
    pfs->m_telemetry = nullptr;
    pfs->m_telemetry_session = nullptr;
#endif /* HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE */

    pfs->m_lock.dirty_to_allocated(&dirty_state);
  }

  return pfs;
}

/**
  Find a PFS thread given an internal thread id.
  @param thread_id internal thread id
  @return pfs pointer if found, else NULL
*/
PFS_thread *find_thread_by_internal_id(ulonglong thread_id) {
  PFS_thread *pfs = nullptr;
  uint index = 0;

  PFS_thread_iterator it = global_thread_container.iterate(index);

  do {
    pfs = it.scan_next(&index);
    if (pfs != nullptr) {
      if (pfs->m_thread_internal_id == thread_id) {
        return pfs;
      }
    }
  } while (pfs != nullptr);

  return nullptr;
}

/**
  Find a PFS thread given a processlist id.
  @param processlist_id PROCESSLIST_ID
  @return pfs pointer if found, else NULL
*/
PFS_thread *find_thread_by_processlist_id(ulonglong processlist_id) {
  PFS_thread *pfs = nullptr;
  uint index = 0;

  /* Valid processlist ID is > 0 (see Global_THD_manager) */
  if (processlist_id == 0) return nullptr;

  PFS_thread_iterator it = global_thread_container.iterate(index);

  do {
    pfs = it.scan_next(&index);
    if (pfs != nullptr) {
      if (pfs->m_processlist_id == processlist_id) {
        return pfs;
      }
    }
  } while (pfs != nullptr);

  return nullptr;
}

PFS_mutex *sanitize_mutex(PFS_mutex *unsafe) {
  return global_mutex_container.sanitize(unsafe);
}

PFS_rwlock *sanitize_rwlock(PFS_rwlock *unsafe) {
  return global_rwlock_container.sanitize(unsafe);
}

PFS_cond *sanitize_cond(PFS_cond *unsafe) {
  return global_cond_container.sanitize(unsafe);
}

/**
  Sanitize a PFS_thread pointer.
  Validate that the PFS_thread is part of thread_array.
  Sanitizing data is required when the data can be
  damaged with expected race conditions, for example
  involving EVENTS_WAITS_HISTORY_LONG.
  @param unsafe the pointer to sanitize
  @return a valid pointer, or NULL
*/
PFS_thread *sanitize_thread(PFS_thread *unsafe) {
  return global_thread_container.sanitize(unsafe);
}

PFS_file *sanitize_file(PFS_file *unsafe) {
  return global_file_container.sanitize(unsafe);
}

PFS_socket *sanitize_socket(PFS_socket *unsafe) {
  return global_socket_container.sanitize(unsafe);
}

PFS_metadata_lock *sanitize_metadata_lock(PFS_metadata_lock *unsafe) {
  return global_mdl_container.sanitize(unsafe);
}

/**
  Destroy instrumentation for a thread instance.
  @param pfs                          the thread to destroy
*/
void destroy_thread(PFS_thread *pfs) {
  assert(pfs != nullptr);
  pfs->reset_session_connect_attrs();
  pfs->m_thd = nullptr;
  pfs->m_cnt_thd = nullptr;

#ifdef HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE
  if (pfs->m_telemetry_session != nullptr) {
    assert(pfs->m_telemetry != nullptr);
    pfs->m_telemetry->m_tel_session_destroy(pfs->m_telemetry_session);
  }

  pfs->m_telemetry = nullptr;
  pfs->m_telemetry_session = nullptr;
#endif /* HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE */

  PFS_thread_class *klass = pfs->m_class;
  if (klass->is_singleton()) {
#if 0
    /*
      In theory, some threads are -- logically -- singletons.

      START XYZ will start a XYZ thread with mysql_thread_create(),
      and set a global state as "running".
      STOP XYZ will notify the running thread to stop,
      and set a global state as "not running".

      In practice, these threads are -- physically -- not singletons.

      STOP XYZ only notifies the running thread to stop,
      but the MySQL code base in general never calls pthread_join(3).
      Instead, the running thread, when noticing it should stop,
      is still executing cleanup actions, until the thread main loop
      finally terminates and call destroy_thread in the performance schema.

      Because of this, the following sequence:
        START XYZ --> fork thread #1
        STOP XYZ --> tell #1 to stop
        START XYZ --> fork thread #2
      can create situations when both threads #1 and #2
      are executing at the same time, until #1 gracefully ends.

      As a result, the assert below is relaxed.

      To enforce it properly,
      a pre requisite is first to use pthread_join() in the entire code base.
    */

    assert(klass->m_singleton == pfs);
#endif
    klass->m_singleton = nullptr;
  }

  if (pfs->m_account != nullptr) {
    pfs->m_account->release();
    pfs->m_account = nullptr;
    assert(pfs->m_user == nullptr);
    assert(pfs->m_host == nullptr);
  } else {
    if (pfs->m_user != nullptr) {
      pfs->m_user->release();
      pfs->m_user = nullptr;
    }
    if (pfs->m_host != nullptr) {
      pfs->m_host->release();
      pfs->m_host = nullptr;
    }
  }
  if (pfs->m_filename_hash_pins) {
    lf_hash_put_pins(pfs->m_filename_hash_pins);
    pfs->m_filename_hash_pins = nullptr;
  }
  if (pfs->m_table_share_hash_pins) {
    lf_hash_put_pins(pfs->m_table_share_hash_pins);
    pfs->m_table_share_hash_pins = nullptr;
  }
  if (pfs->m_setup_actor_hash_pins) {
    lf_hash_put_pins(pfs->m_setup_actor_hash_pins);
    pfs->m_setup_actor_hash_pins = nullptr;
  }
  if (pfs->m_setup_object_hash_pins) {
    lf_hash_put_pins(pfs->m_setup_object_hash_pins);
    pfs->m_setup_object_hash_pins = nullptr;
  }
  if (pfs->m_user_hash_pins) {
    lf_hash_put_pins(pfs->m_user_hash_pins);
    pfs->m_user_hash_pins = nullptr;
  }
  if (pfs->m_account_hash_pins) {
    lf_hash_put_pins(pfs->m_account_hash_pins);
    pfs->m_account_hash_pins = nullptr;
  }
  if (pfs->m_host_hash_pins) {
    lf_hash_put_pins(pfs->m_host_hash_pins);
    pfs->m_host_hash_pins = nullptr;
  }
  if (pfs->m_digest_hash_pins) {
    lf_hash_put_pins(pfs->m_digest_hash_pins);
    pfs->m_digest_hash_pins = nullptr;
  }
  if (pfs->m_program_hash_pins) {
    lf_hash_put_pins(pfs->m_program_hash_pins);
    pfs->m_program_hash_pins = nullptr;
  }
  global_thread_container.deallocate(pfs);
}

/**
  Get the hash pins for @c filename_hash.
  @param thread The running thread.
  @returns The LF_HASH pins for the thread.
*/
static LF_PINS *get_filename_hash_pins(PFS_thread *thread) {
  if (unlikely(thread->m_filename_hash_pins == nullptr)) {
    if (!filename_hash_inited) {
      return nullptr;
    }
    thread->m_filename_hash_pins = lf_hash_get_pins(&filename_hash);
  }
  return thread->m_filename_hash_pins;
}

/**
  Normalize a filename with fully qualified path.
  @param filename                 file to be normalized, null-terminatd
  @param name_len                 length in bytes of the filename
  @param [out] normalized Normalized file name
*/
int normalize_filename(const char *filename, uint name_len,
                       PFS_file_name &normalized) {
  char buffer[FN_REFLEN];
  char safe_buffer[FN_REFLEN];
  const char *safe_filename;

  assert(filename != nullptr);

  if (name_len >= FN_REFLEN) {
    /*
      The instrumented code uses file names that exceeds FN_REFLEN.
      This could be legal for instrumentation on non mysys APIs,
      so we support it.
      Truncate the file name so that:
      - it fits into pfs->m_filename
      - it is safe to use mysys apis to normalize the file name.
    */
    memcpy(safe_buffer, filename, FN_REFLEN - 1);
    safe_buffer[FN_REFLEN - 1] = '\0';
    safe_filename = safe_buffer;
  } else {
    safe_filename = filename;
  }

  /*
    Normalize the file name to avoid duplicates when using aliases:
    - absolute or relative paths
    - symbolic links
    Names are resolved as follows:
    - /real/path/to/real_file ==> same
    - /path/with/link/to/real_file ==> /real/path/to/real_file
    - real_file ==> /real/path/to/real_file
    - ./real_file ==> /real/path/to/real_file
    - /real/path/to/sym_link ==> same
    - /path/with/link/to/sym_link ==> /real/path/to/sym_link
    - sym_link ==> /real/path/to/sym_link
    - ./sym_link ==> /real/path/to/sym_link
    When the last component of a file is a symbolic link,
    the last component is *not* resolved, so that all file I/O
    operations on a link (create, read, write, delete) are counted
    against the link itself, not the target file.
    Resolving the name would lead to create counted against the link,
    and read/write/delete counted against the target, leading to
    incoherent results and instrumentation leaks.
    Also note that, when creating files, this name resolution
    works properly for files that do not exist (yet) on the file system.
  */
  char dirbuffer[FN_REFLEN + 1];
  const size_t dirlen = dirname_length(safe_filename);

  if (dirlen == 0) {
    dirbuffer[0] = FN_CURLIB;
    dirbuffer[1] = FN_LIBCHAR;
    dirbuffer[2] = '\0';
  } else {
    memcpy(dirbuffer, safe_filename, dirlen);
    dirbuffer[dirlen] = '\0';
  }

  /* Resolve the absolute directory path. */
  if (my_realpath(buffer, dirbuffer, MYF(0)) != 0) {
    buffer[0] = '\0';
    return 1;
  }

  /* Append the unresolved filename to the resolved path */
  char *ptr = buffer + strlen(buffer);
  char *buf_end = &buffer[sizeof(buffer) - 1];
  if ((buf_end > ptr) && (*(ptr - 1) != FN_LIBCHAR)) {
    *ptr++ = FN_LIBCHAR;
  }
  if (buf_end > ptr) {
    strncpy(ptr, safe_filename + dirlen, buf_end - ptr);
  }
  *buf_end = '\0';

  /* Return normalized filename. */
  normalized.set(buffer, strlen(buffer));
  return 0;
}

/**
  Find or create instrumentation for a file instance by file name.
  @param thread                       the executing instrumented thread
  @param klass                        the file class
  @param filename                     the file name
  @param len                          the length in bytes of filename
  @param create                       create a file instance if none found
  @return a file instance, or nullptr
*/
PFS_file *find_or_create_file(PFS_thread *thread, PFS_file_class *klass,
                              const char *filename, uint len, bool create) {
  assert(klass != nullptr || !create);

  PFS_file *pfs;
  PFS_file_name normalized_filename;
  int rc;

  rc = normalize_filename(filename, len, normalized_filename);
  if (rc != 0) {
    global_file_container.m_lost++;
    return nullptr;
  }

  LF_PINS *pins = get_filename_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    global_file_container.m_lost++;
    return nullptr;
  }

  PFS_file **entry;
  uint retry_count = 0;
  const uint retry_max = 3;
  pfs_dirty_state dirty_state;

search:

  entry = reinterpret_cast<PFS_file **>(lf_hash_search(
      &filename_hash, pins, &normalized_filename, sizeof(normalized_filename)));
  if (entry && (entry != MY_LF_ERRPTR)) {
    pfs = *entry;
    pfs->m_file_stat.m_open_count++;
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  if (!create) {
    /* No lost counter, just looking for the file existence. */
    return nullptr;
  }

  pfs = global_file_container.allocate(&dirty_state);
  if (pfs != nullptr) {
    pfs->m_class = klass;
    pfs->m_enabled = klass->m_enabled && flag_global_instrumentation;
    pfs->m_timed = klass->m_timed;
    pfs->m_file_name = normalized_filename;
    pfs->m_file_stat.m_open_count = 1;
    pfs->m_file_stat.m_io_stat.reset();
    pfs->m_identity = (const void *)pfs;
    pfs->m_temporary = false;

    int res;
    pfs->m_lock.dirty_to_allocated(&dirty_state);
    res = lf_hash_insert(&filename_hash, pins, &pfs);
    if (likely(res == 0)) {
      if (klass->is_singleton()) {
        klass->m_singleton = pfs;
      }
      return pfs;
    }

    global_file_container.deallocate(pfs);

    if (res > 0) {
      /* Duplicate insert by another thread */
      if (++retry_count > retry_max) {
        /* Avoid infinite loops */
        global_file_container.m_lost++;
        return nullptr;
      }
      goto search;
    }

    /* OOM in lf_hash_insert */
    global_file_container.m_lost++;
    return nullptr;
  }

  return nullptr;
}

/**
  Before the rename operation: Find the file instrumentation by name,
  then delete the filename from the filename hash.
  @param thread                       the executing instrumented thread
  @param old_name                     the file to be renamed, null-terminated
  @return a file instance or nullptr
*/
PFS_file *start_file_rename(PFS_thread *thread, const char *old_name) {
  assert(thread != nullptr);
  assert(old_name != nullptr);

  const uint old_length = (uint)strlen(old_name);
  PFS_file_name normalized_filename;
  int rc;

  rc = normalize_filename(old_name, old_length, normalized_filename);
  if (rc != 0) {
    global_file_container.m_lost++;
    return nullptr;
  }

  LF_PINS *pins = get_filename_hash_pins(thread);
  if (unlikely(pins == nullptr)) {
    global_file_container.m_lost++;
    return nullptr;
  }

  /* Find the file instrumentation by name. */
  auto **entry = reinterpret_cast<PFS_file **>(lf_hash_search(
      &filename_hash, pins, &normalized_filename, sizeof(normalized_filename)));

  PFS_file *pfs = nullptr;
  if (entry && (entry != MY_LF_ERRPTR)) {
    /*
      Delete the old filename from the hash to avoid a race on the filename.
      Add the new filename after rename() operation completes.
    */
    pfs = *entry;
    lf_hash_delete(&filename_hash, pins, &pfs->m_file_name,
                   sizeof(pfs->m_file_name));
  }

  lf_hash_search_unpin(pins);
  return pfs;
}

/**
  After the rename operation: Assign the new filename to the
  file instrumentation instance, then add to the filename hash.
  @param thread                       the executing instrumented thread
  @param pfs                          the file instrumentation
  @param new_name                     the new filename, null-terminated
  @param rename_result                the result from rename operation
  @return 0 for success
*/
int end_file_rename(PFS_thread *thread, PFS_file *pfs, const char *new_name,
                    int rename_result) {
  assert(thread != nullptr);
  assert(pfs != nullptr);
  assert(new_name != nullptr);

  const uint new_length = (uint)strlen(new_name);

  if (likely(rename_result == 0)) {
    /*
      If rename() succeeded,
      update the file instance with the new filename.
    */
    PFS_file_name normalized_filename;
    int rc;

    rc = normalize_filename(new_name, new_length, normalized_filename);
    if (rc != 0) {
      global_file_container.m_lost++;
      return 1;
    }

    pfs->m_file_name = normalized_filename;
  }

  LF_PINS *pins = get_filename_hash_pins(thread);

  if (unlikely(pins == nullptr)) {
    global_file_container.m_lost++;
    return 1;
  }

  /* Add the new filename or restore the old filename to the hash. */
  const int res = lf_hash_insert(&filename_hash, pins, &pfs);
  lf_hash_search_unpin(pins);

  if (unlikely(res != 0)) {
    global_file_container.deallocate(pfs);
    global_file_container.m_lost++;
  }

  return res;
}

/**
  Find a file instrumentation instance by name.
  @param thread                       the executing instrumented thread
  @param klass                        the file class
  @param filename                     the file name
  @param len                          the length in bytes of filename
  @return a file instance, or nullptr
*/
PFS_file *find_file(PFS_thread *thread, PFS_file_class *klass,
                    const char *filename, uint len) {
  return find_or_create_file(thread, klass, filename, len, false);
}

/**
  Release instrumentation for a file instance.
  @param pfs                          the file to release
*/
void release_file(PFS_file *pfs) {
  assert(pfs != nullptr);
  pfs->m_file_stat.m_open_count--;
}

/**
  Delete file name from the hash table.
  @param thread                       the executing thread instrumentation
  @param pfs                          the file instrumentation
*/
void delete_file_name(PFS_thread *thread, PFS_file *pfs) {
  if (thread == nullptr || pfs == nullptr) return;

  LF_PINS *pins = get_filename_hash_pins(thread);
  assert(pins != nullptr);

  lf_hash_delete(&filename_hash, pins, &pfs->m_file_name,
                 sizeof(pfs->m_file_name));
}

/**
  Destroy instrumentation for a file instance.
  @param thread                       the executing thread instrumentation
  @param pfs                          the file to destroy
  @param delete_name                  if true, delete the filename from the hash
*/
void destroy_file(PFS_thread *thread, PFS_file *pfs, bool delete_name) {
  assert(thread != nullptr);
  assert(pfs != nullptr);
  PFS_file_class *klass = pfs->m_class;

  /* Aggregate to FILE_SUMMARY_BY_EVENT_NAME */
  klass->m_file_stat.aggregate(&pfs->m_file_stat);
  pfs->m_file_stat.reset();

  if (klass->is_singleton()) {
    klass->m_singleton = nullptr;
  }

  if (delete_name) {
    delete_file_name(thread, pfs);
  }

  global_file_container.deallocate(pfs);
}

/**
  Create instrumentation for a table instance.
  @param share                        the table share
  @param opening_thread               the opening thread
  @param identity                     the table address
  @return a table instance, or NULL
*/
PFS_table *create_table(PFS_table_share *share, PFS_thread *opening_thread,
                        const void *identity) {
  PFS_table *pfs;
  pfs_dirty_state dirty_state;

  pfs = global_table_container.allocate(&dirty_state);
  if (pfs != nullptr) {
    pfs->m_identity = identity;
    pfs->m_share = share;
    pfs->m_io_enabled = share->m_enabled && flag_global_instrumentation &&
                        global_table_io_class.m_enabled;
    pfs->m_io_timed = share->m_timed && global_table_io_class.m_timed;
    pfs->m_lock_enabled = share->m_enabled && flag_global_instrumentation &&
                          global_table_lock_class.m_enabled;
    pfs->m_lock_timed = share->m_timed && global_table_lock_class.m_timed;
    pfs->m_has_io_stats = false;
    pfs->m_has_lock_stats = false;
    pfs->m_internal_lock = PFS_TL_NONE;
    pfs->m_external_lock = PFS_TL_NONE;
    share->inc_refcount();
    pfs->m_table_stat.fast_reset();
    pfs->m_thread_owner = opening_thread;
    pfs->m_owner_event_id = opening_thread->m_event_id;
    pfs->m_lock.dirty_to_allocated(&dirty_state);
  }

  return pfs;
}

void PFS_table::sanitized_aggregate() {
  /*
    This thread could be a TRUNCATE on an aggregated summary table,
    and not own the table handle.
  */
  PFS_table_share *safe_share = sanitize_table_share(m_share);
  if (safe_share != nullptr) {
    if (m_has_io_stats) {
      safe_aggregate_io(nullptr, &m_table_stat, safe_share);
      m_has_io_stats = false;
    }
    if (m_has_lock_stats) {
      safe_aggregate_lock(&m_table_stat, safe_share);
      m_has_lock_stats = false;
    }
  }
}

void PFS_table::sanitized_aggregate_io() {
  PFS_table_share *safe_share = sanitize_table_share(m_share);
  if (safe_share != nullptr && m_has_io_stats) {
    safe_aggregate_io(nullptr, &m_table_stat, safe_share);
    m_has_io_stats = false;
  }
}

void PFS_table::sanitized_aggregate_lock() {
  PFS_table_share *safe_share = sanitize_table_share(m_share);
  if (safe_share != nullptr && m_has_lock_stats) {
    safe_aggregate_lock(&m_table_stat, safe_share);
    m_has_lock_stats = false;
  }
}

void PFS_table::safe_aggregate_io(const TABLE_SHARE *optional_server_share,
                                  PFS_table_stat *table_stat,
                                  PFS_table_share *table_share) {
  assert(table_stat != nullptr);
  assert(table_share != nullptr);

  const uint key_count = sanitize_index_count(table_share->m_key_count);

  PFS_table_share_index *to_stat;
  PFS_table_io_stat *from_stat;
  uint index;

  assert(key_count <= MAX_INDEXES);

  /* Aggregate stats for each index, if any */
  for (index = 0; index < key_count; index++) {
    from_stat = &table_stat->m_index_stat[index];
    if (from_stat->m_has_data) {
      if (optional_server_share != nullptr) {
        /*
          An instrumented thread is closing a table,
          and capable of providing index names when
          creating index statistics on the fly.
        */
        to_stat = table_share->find_or_create_index_stat(optional_server_share,
                                                         index);
      } else {
        /*
          A monitoring thread, performing TRUNCATE TABLE,
          is asking to flush existing stats from table handles,
          but it does not know about index names used in handles.
          If the index stat already exists, find it and aggregate to it.
          It the index stat does not exist yet, drop the stat and do nothing.
        */
        to_stat = table_share->find_index_stat(index);
      }
      if (to_stat != nullptr) {
        /* Aggregate to TABLE_IO_SUMMARY */
        to_stat->m_stat.aggregate(from_stat);
      }
    }
  }

  /* Aggregate stats for the table */
  from_stat = &table_stat->m_index_stat[MAX_INDEXES];
  if (from_stat->m_has_data) {
    to_stat = table_share->find_or_create_index_stat(nullptr, MAX_INDEXES);
    if (to_stat != nullptr) {
      /* Aggregate to TABLE_IO_SUMMARY */
      to_stat->m_stat.aggregate(from_stat);
    }
  }

  table_stat->fast_reset_io();
}

void PFS_table::safe_aggregate_lock(PFS_table_stat *table_stat,
                                    PFS_table_share *table_share) {
  assert(table_stat != nullptr);
  assert(table_share != nullptr);

  PFS_table_lock_stat *from_stat = &table_stat->m_lock_stat;

  PFS_table_share_lock *to_stat;

  to_stat = table_share->find_or_create_lock_stat();
  if (to_stat != nullptr) {
    /* Aggregate to TABLE_LOCK_SUMMARY */
    to_stat->m_stat.aggregate(from_stat);
  }

  table_stat->fast_reset_lock();
}

/**
  Destroy instrumentation for a table instance.
  @param pfs                          the table to destroy
*/
void destroy_table(PFS_table *pfs) {
  assert(pfs != nullptr);
  pfs->m_share->dec_refcount();
  global_table_container.deallocate(pfs);
}

/**
  Create instrumentation for a socket instance.
  @param klass                        the socket class
  @param fd                           the socket file descriptor
  @param addr                         the socket address
  @param addr_len                     the socket address length
  @return a socket instance, or NULL
*/
PFS_socket *create_socket(PFS_socket_class *klass, const my_socket *fd,
                          const struct sockaddr *addr, socklen_t addr_len) {
  PFS_socket *pfs;
  pfs_dirty_state dirty_state;

  uint fd_used = 0;
  uint addr_len_used = addr_len;

  if (fd != nullptr) {
    fd_used = *fd;
  }

  if (addr_len_used > sizeof(sockaddr_storage)) {
    addr_len_used = sizeof(sockaddr_storage);
  }

  pfs = global_socket_container.allocate(&dirty_state);

  if (pfs != nullptr) {
    pfs->m_fd = fd_used;
    /* There is no socket object, so we use the instrumentation. */
    pfs->m_identity = pfs;
    pfs->m_class = klass;
    pfs->m_enabled = klass->m_enabled && flag_global_instrumentation;
    pfs->m_timed = klass->m_timed;
    pfs->m_idle = false;
    pfs->m_socket_stat.reset();
    pfs->m_thread_owner = nullptr;

    pfs->m_addr_len = addr_len_used;
    if ((addr != nullptr) && (addr_len_used > 0)) {
      pfs->m_addr_len = addr_len_used;
      memcpy(&pfs->m_sock_addr, addr, addr_len_used);
    } else {
      pfs->m_addr_len = 0;
    }

    pfs->m_lock.dirty_to_allocated(&dirty_state);

    if (klass->is_singleton()) {
      klass->m_singleton = pfs;
    }
  }

  return pfs;
}

/**
  Destroy instrumentation for a socket instance.
  @param pfs                          the socket to destroy
*/
void destroy_socket(PFS_socket *pfs) {
  assert(pfs != nullptr);
  PFS_socket_class *klass = pfs->m_class;

  /* Aggregate to SOCKET_SUMMARY_BY_EVENT_NAME */
  klass->m_socket_stat.m_io_stat.aggregate(&pfs->m_socket_stat.m_io_stat);

  if (klass->is_singleton()) {
    klass->m_singleton = nullptr;
  }

  /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME */
  PFS_thread *thread = pfs->m_thread_owner;
  if (thread != nullptr) {
    /* Combine stats for all operations */
    PFS_single_stat stat;
    pfs->m_socket_stat.m_io_stat.sum_waits(&stat);
    if (stat.m_count != 0) {
      PFS_single_stat *event_name_array;
      event_name_array = thread->write_instr_class_waits_stats();
      const uint index = pfs->m_class->m_event_name_index;

      event_name_array[index].aggregate(&stat);
    }
  }

  pfs->m_socket_stat.reset();
  pfs->m_thread_owner = nullptr;
  pfs->m_fd = 0;
  pfs->m_addr_len = 0;

  global_socket_container.deallocate(pfs);
}

PFS_metadata_lock *create_metadata_lock(void *identity, const MDL_key *mdl_key,
                                        opaque_mdl_type mdl_type,
                                        opaque_mdl_duration mdl_duration,
                                        opaque_mdl_status mdl_status,
                                        const char *src_file, uint src_line) {
  PFS_metadata_lock *pfs;
  pfs_dirty_state dirty_state;

  pfs = global_mdl_container.allocate(&dirty_state);
  if (pfs != nullptr) {
    pfs->m_identity = identity;
    pfs->m_enabled =
        global_metadata_class.m_enabled && flag_global_instrumentation;
    pfs->m_timed = global_metadata_class.m_timed;
    pfs->m_mdl_key.mdl_key_init(mdl_key);
    pfs->m_mdl_type = mdl_type;
    pfs->m_mdl_duration = mdl_duration;
    pfs->m_mdl_status = mdl_status;
    pfs->m_src_file = src_file;
    pfs->m_src_line = src_line;
    pfs->m_owner_thread_id = 0;
    pfs->m_owner_event_id = 0;
    pfs->m_lock.dirty_to_allocated(&dirty_state);
  }

  return pfs;
}

void destroy_metadata_lock(PFS_metadata_lock *pfs) {
  assert(pfs != nullptr);
  global_mdl_container.deallocate(pfs);
}

static void fct_reset_mutex_waits(PFS_mutex *pfs) { pfs->m_mutex_stat.reset(); }

static void reset_mutex_waits_by_instance() {
  global_mutex_container.apply_all(fct_reset_mutex_waits);
}

static void fct_reset_rwlock_waits(PFS_rwlock *pfs) {
  pfs->m_rwlock_stat.reset();
}

static void reset_rwlock_waits_by_instance() {
  global_rwlock_container.apply_all(fct_reset_rwlock_waits);
}

static void fct_reset_cond_waits(PFS_cond *pfs) { pfs->m_cond_stat.reset(); }

static void reset_cond_waits_by_instance() {
  global_cond_container.apply_all(fct_reset_cond_waits);
}

static void fct_reset_file_waits(PFS_file *pfs) { pfs->m_file_stat.reset(); }

static void reset_file_waits_by_instance() {
  global_file_container.apply_all(fct_reset_file_waits);
}

static void fct_reset_socket_waits(PFS_socket *pfs) {
  pfs->m_socket_stat.reset();
}

static void reset_socket_waits_by_instance() {
  global_socket_container.apply_all(fct_reset_socket_waits);
}

/** Reset the wait statistics per object instance. */
void reset_events_waits_by_instance() {
  reset_mutex_waits_by_instance();
  reset_rwlock_waits_by_instance();
  reset_cond_waits_by_instance();
  reset_file_waits_by_instance();
  reset_socket_waits_by_instance();
}

static void fct_reset_file_io(PFS_file *pfs) {
  pfs->m_file_stat.m_io_stat.reset();
}

/** Reset the I/O statistics per file instance. */
void reset_file_instance_io() {
  global_file_container.apply_all(fct_reset_file_io);
}

static void fct_reset_socket_io(PFS_socket *pfs) {
  pfs->m_socket_stat.m_io_stat.reset();
}

/** Reset the I/O statistics per socket instance. */
void reset_socket_instance_io() {
  global_socket_container.apply_all(fct_reset_socket_io);
}

void reset_histogram_global() { global_statements_histogram.reset(); }

void aggregate_all_event_names(PFS_single_stat *from_array,
                               PFS_single_stat *to_array) {
  PFS_single_stat *from;
  PFS_single_stat *from_last;
  PFS_single_stat *to;

  from = from_array;
  from_last = from_array + wait_class_max;
  to = to_array;

  for (; from < from_last; from++, to++) {
    if (from->m_count > 0) {
      to->aggregate(from);
      from->reset();
    }
  }
}

void aggregate_all_event_names(PFS_single_stat *from_array,
                               PFS_single_stat *to_array_1,
                               PFS_single_stat *to_array_2) {
  PFS_single_stat *from;
  PFS_single_stat *from_last;
  PFS_single_stat *to_1;
  PFS_single_stat *to_2;

  from = from_array;
  from_last = from_array + wait_class_max;
  to_1 = to_array_1;
  to_2 = to_array_2;

  for (; from < from_last; from++, to_1++, to_2++) {
    if (from->m_count > 0) {
      to_1->aggregate(from);
      to_2->aggregate(from);
      from->reset();
    }
  }
}

void aggregate_all_stages(PFS_stage_stat *from_array,
                          PFS_stage_stat *to_array) {
  PFS_stage_stat *from;
  PFS_stage_stat *from_last;
  PFS_stage_stat *to;

  from = from_array;
  from_last = from_array + stage_class_max;
  to = to_array;

  for (; from < from_last; from++, to++) {
    if (from->m_timer1_stat.m_count > 0) {
      to->aggregate(from);
      from->reset();
    }
  }
}

void aggregate_all_stages(PFS_stage_stat *from_array,
                          PFS_stage_stat *to_array_1,
                          PFS_stage_stat *to_array_2) {
  PFS_stage_stat *from;
  PFS_stage_stat *from_last;
  PFS_stage_stat *to_1;
  PFS_stage_stat *to_2;

  from = from_array;
  from_last = from_array + stage_class_max;
  to_1 = to_array_1;
  to_2 = to_array_2;

  for (; from < from_last; from++, to_1++, to_2++) {
    if (from->m_timer1_stat.m_count > 0) {
      to_1->aggregate(from);
      to_2->aggregate(from);
      from->reset();
    }
  }
}

void aggregate_all_statements(PFS_statement_stat *from_array,
                              PFS_statement_stat *to_array) {
  PFS_statement_stat *from;
  PFS_statement_stat *from_last;
  PFS_statement_stat *to;

  from = from_array;
  from_last = from_array + statement_class_max;
  to = to_array;

  for (; from < from_last; from++, to++) {
    if (from->m_timer1_stat.m_count > 0) {
      to->aggregate(from);
      from->reset();
    }
  }
}

void aggregate_all_statements(PFS_statement_stat *from_array,
                              PFS_statement_stat *to_array_1,
                              PFS_statement_stat *to_array_2) {
  PFS_statement_stat *from;
  PFS_statement_stat *from_last;
  PFS_statement_stat *to_1;
  PFS_statement_stat *to_2;

  from = from_array;
  from_last = from_array + statement_class_max;
  to_1 = to_array_1;
  to_2 = to_array_2;

  for (; from < from_last; from++, to_1++, to_2++) {
    if (from->m_timer1_stat.m_count > 0) {
      to_1->aggregate(from);
      to_2->aggregate(from);
      from->reset();
    }
  }
}

void aggregate_all_transactions(PFS_transaction_stat *from_array,
                                PFS_transaction_stat *to_array) {
  assert(from_array != nullptr);
  assert(to_array != nullptr);

  if (from_array->count() > 0) {
    to_array->aggregate(from_array);
    from_array->reset();
  }
}

void aggregate_all_transactions(PFS_transaction_stat *from_array,
                                PFS_transaction_stat *to_array_1,
                                PFS_transaction_stat *to_array_2) {
  assert(from_array != nullptr);
  assert(to_array_1 != nullptr);
  assert(to_array_2 != nullptr);

  if (from_array->count() > 0) {
    to_array_1->aggregate(from_array);
    to_array_2->aggregate(from_array);
    from_array->reset();
  }
}

void aggregate_all_errors(PFS_error_stat *from_array,
                          PFS_error_stat *to_array) {
  assert(from_array != nullptr);
  assert(to_array != nullptr);

  if (from_array->count() > 0) {
    to_array->aggregate(from_array);
    from_array->reset();
  }
}

void aggregate_all_errors(PFS_error_stat *from_array,
                          PFS_error_stat *to_array_1,
                          PFS_error_stat *to_array_2) {
  assert(from_array != nullptr);
  assert(to_array_1 != nullptr);
  assert(to_array_2 != nullptr);

  if (from_array->count() > 0) {
    to_array_1->aggregate(from_array);
    to_array_2->aggregate(from_array);
    from_array->reset();
  }
}

void aggregate_all_memory_with_reassign(bool alive,
                                        PFS_memory_safe_stat *from_array,
                                        PFS_memory_shared_stat *to_array,
                                        PFS_memory_shared_stat *global_array) {
  PFS_memory_safe_stat *from;
  PFS_memory_safe_stat *from_last;
  PFS_memory_shared_stat *to;

  from = from_array;
  from_last = from_array + memory_class_max;
  to = to_array;

  if (alive) {
    for (; from < from_last; from++, to++) {
      memory_partial_aggregate(from, to);
    }
  } else {
    PFS_memory_shared_stat *global;
    global = global_array;
    for (; from < from_last; from++, to++, global++) {
      memory_full_aggregate_with_reassign(from, to, global);
      from->reset();
    }
  }
}

void aggregate_all_memory(bool alive, PFS_memory_safe_stat *from_array,
                          PFS_memory_shared_stat *to_array) {
  PFS_memory_safe_stat *from;
  PFS_memory_safe_stat *from_last;
  PFS_memory_shared_stat *to;

  from = from_array;
  from_last = from_array + memory_class_max;
  to = to_array;

  if (alive) {
    for (; from < from_last; from++, to++) {
      memory_partial_aggregate(from, to);
    }
  } else {
    for (; from < from_last; from++, to++) {
      memory_full_aggregate(from, to);
      from->reset();
    }
  }
}

void aggregate_all_memory(bool alive, PFS_memory_shared_stat *from_array,
                          PFS_memory_shared_stat *to_array) {
  PFS_memory_shared_stat *from;
  PFS_memory_shared_stat *from_last;
  PFS_memory_shared_stat *to;

  from = from_array;
  from_last = from_array + memory_class_max;
  to = to_array;

  if (alive) {
    for (; from < from_last; from++, to++) {
      memory_partial_aggregate(from, to);
    }
  } else {
    for (; from < from_last; from++, to++) {
      memory_full_aggregate(from, to);
      from->reset();
    }
  }
}

void aggregate_all_memory_with_reassign(bool alive,
                                        PFS_memory_safe_stat *from_array,
                                        PFS_memory_shared_stat *to_array_1,
                                        PFS_memory_shared_stat *to_array_2,
                                        PFS_memory_shared_stat *global_array) {
  PFS_memory_safe_stat *from;
  PFS_memory_safe_stat *from_last;
  PFS_memory_shared_stat *to_1;
  PFS_memory_shared_stat *to_2;

  from = from_array;
  from_last = from_array + memory_class_max;
  to_1 = to_array_1;
  to_2 = to_array_2;

  if (alive) {
    for (; from < from_last; from++, to_1++, to_2++) {
      memory_partial_aggregate(from, to_1, to_2);
    }
  } else {
    PFS_memory_shared_stat *global;
    global = global_array;
    for (; from < from_last; from++, to_1++, to_2++, global++) {
      memory_full_aggregate_with_reassign(from, to_1, to_2, global);
      from->reset();
    }
  }
}

void aggregate_all_memory(bool alive, PFS_memory_shared_stat *from_array,
                          PFS_memory_shared_stat *to_array_1,
                          PFS_memory_shared_stat *to_array_2) {
  PFS_memory_shared_stat *from;
  PFS_memory_shared_stat *from_last;
  PFS_memory_shared_stat *to_1;
  PFS_memory_shared_stat *to_2;

  from = from_array;
  from_last = from_array + memory_class_max;
  to_1 = to_array_1;
  to_2 = to_array_2;

  if (alive) {
    for (; from < from_last; from++, to_1++, to_2++) {
      memory_partial_aggregate(from, to_1, to_2);
    }
  } else {
    for (; from < from_last; from++, to_1++, to_2++) {
      memory_full_aggregate(from, to_1, to_2);
      from->reset();
    }
  }
}

void aggregate_thread_status(PFS_thread *thread, PFS_account *safe_account,
                             PFS_user *safe_user, PFS_host *safe_host) {
  THD *thd = thread->m_thd;
  bool aggregated = false;

  if (thd == nullptr) {
    return;
  }

  System_status_var *status_var = get_thd_status_var(thd, &aggregated);

  if (unlikely(aggregated)) {
    /* THD is being closed, status has already been aggregated. */
    return;
  }

  if (likely(safe_account != nullptr)) {
    safe_account->aggregate_status_stats(status_var);
    return;
  }

  if (safe_user != nullptr) {
    safe_user->aggregate_status_stats(status_var);
  }

  if (safe_host != nullptr) {
    safe_host->aggregate_status_stats(status_var);
  }
}

static void aggregate_thread_stats(PFS_thread *thread,
                                   PFS_account *safe_account,
                                   PFS_user *safe_user, PFS_host *safe_host) {
  const ulonglong controlled_memory =
      thread->m_session_all_memory_stat.m_controlled.get_session_max();
  const ulonglong total_memory =
      thread->m_session_all_memory_stat.m_total.get_session_max();

  if (likely(safe_account != nullptr)) {
    safe_account->aggregate_disconnect(controlled_memory, total_memory);
    return;
  }

  if (safe_user != nullptr) {
    safe_user->aggregate_disconnect(controlled_memory, total_memory);
  }

  if (safe_host != nullptr) {
    safe_host->aggregate_disconnect(controlled_memory, total_memory);
  }

  /* There is no global table for connections statistics. */
}

void aggregate_thread(PFS_thread *thread, PFS_account *safe_account,
                      PFS_user *safe_user, PFS_host *safe_host) {
  /* No HAVE_PSI_???_INTERFACE flag, waits cover multiple instrumentations */
  aggregate_thread_waits(thread, safe_account, safe_user, safe_host);

#ifdef HAVE_PSI_STAGE_INTERFACE
  aggregate_thread_stages(thread, safe_account, safe_user, safe_host);
#endif

#ifdef HAVE_PSI_STATEMENT_INTERFACE
  aggregate_thread_statements(thread, safe_account, safe_user, safe_host);
#endif

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  aggregate_thread_transactions(thread, safe_account, safe_user, safe_host);
#endif

#ifdef HAVE_PSI_ERROR_INTERFACE
  aggregate_thread_errors(thread, safe_account, safe_user, safe_host);
#endif

#ifdef HAVE_PSI_MEMORY_INTERFACE
  aggregate_thread_memory(false, thread, safe_account, safe_user, safe_host);
#endif

  aggregate_thread_status(thread, safe_account, safe_user, safe_host);

  aggregate_thread_stats(thread, safe_account, safe_user, safe_host);
}

void aggregate_thread_waits(PFS_thread *thread, PFS_account *safe_account,
                            PFS_user *safe_user, PFS_host *safe_host) {
  if (thread->read_instr_class_waits_stats() == nullptr) {
    return;
  }

  if (likely(safe_account != nullptr)) {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME.
    */
    aggregate_all_event_names(thread->write_instr_class_waits_stats(),
                              safe_account->write_instr_class_waits_stats());

    return;
  }

  if ((safe_user != nullptr) && (safe_host != nullptr)) {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_event_names(thread->write_instr_class_waits_stats(),
                              safe_user->write_instr_class_waits_stats(),
                              safe_host->write_instr_class_waits_stats());
    return;
  }

  if (safe_user != nullptr) {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME, directly.
    */
    aggregate_all_event_names(thread->write_instr_class_waits_stats(),
                              safe_user->write_instr_class_waits_stats());
    return;
  }

  if (safe_host != nullptr) {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME, directly.
    */
    aggregate_all_event_names(thread->write_instr_class_waits_stats(),
                              safe_host->write_instr_class_waits_stats());
    return;
  }

  /* Orphan thread, clean the waits stats. */
  thread->reset_waits_stats();
}

void aggregate_thread_stages(PFS_thread *thread, PFS_account *safe_account,
                             PFS_user *safe_user, PFS_host *safe_host) {
  if (thread->read_instr_class_stages_stats() == nullptr) {
    return;
  }

  if (likely(safe_account != nullptr)) {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME.
    */
    aggregate_all_stages(thread->write_instr_class_stages_stats(),
                         safe_account->write_instr_class_stages_stats());

    return;
  }

  if ((safe_user != nullptr) && (safe_host != nullptr)) {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_stages(thread->write_instr_class_stages_stats(),
                         safe_user->write_instr_class_stages_stats(),
                         safe_host->write_instr_class_stages_stats());
    return;
  }

  if (safe_user != nullptr) {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_stages(thread->write_instr_class_stages_stats(),
                         safe_user->write_instr_class_stages_stats(),
                         global_instr_class_stages_array);
    return;
  }

  if (safe_host != nullptr) {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME, directly.
    */
    aggregate_all_stages(thread->write_instr_class_stages_stats(),
                         safe_host->write_instr_class_stages_stats());
    return;
  }

  /*
    Aggregate EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME
    to EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME.
  */
  aggregate_all_stages(thread->write_instr_class_stages_stats(),
                       global_instr_class_stages_array);
}

void aggregate_thread_statements(PFS_thread *thread, PFS_account *safe_account,
                                 PFS_user *safe_user, PFS_host *safe_host) {
  if (thread->read_instr_class_statements_stats() == nullptr) {
    return;
  }

  if (likely(safe_account != nullptr)) {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME.
    */
    aggregate_all_statements(
        thread->write_instr_class_statements_stats(),
        safe_account->write_instr_class_statements_stats());

    return;
  }

  if ((safe_user != nullptr) && (safe_host != nullptr)) {
    /*
      Aggregate EVENTS_STATEMENT_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  EVENTS_STATEMENT_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STATEMENT_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_statements(thread->write_instr_class_statements_stats(),
                             safe_user->write_instr_class_statements_stats(),
                             safe_host->write_instr_class_statements_stats());
    return;
  }

  if (safe_user != nullptr) {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  EVENTS_STATEMENTS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_statements(thread->write_instr_class_statements_stats(),
                             safe_user->write_instr_class_statements_stats(),
                             global_instr_class_statements_array);
    return;
  }

  if (safe_host != nullptr) {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME, directly.
    */
    aggregate_all_statements(thread->write_instr_class_statements_stats(),
                             safe_host->write_instr_class_statements_stats());
    return;
  }

  /*
    Aggregate EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME
    to EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME.
  */
  aggregate_all_statements(thread->write_instr_class_statements_stats(),
                           global_instr_class_statements_array);
}

void aggregate_thread_transactions(PFS_thread *thread,
                                   PFS_account *safe_account,
                                   PFS_user *safe_user, PFS_host *safe_host) {
  if (thread->read_instr_class_transactions_stats() == nullptr) {
    return;
  }

  if (likely(safe_account != nullptr)) {
    /*
      Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_TRANSACTIONS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME.
    */
    aggregate_all_transactions(
        thread->write_instr_class_transactions_stats(),
        safe_account->write_instr_class_transactions_stats());

    return;
  }

  if ((safe_user != nullptr) && (safe_host != nullptr)) {
    /*
      Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  EVENTS_TRANSACTIONS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_TRANSACTIONS_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_transactions(
        thread->write_instr_class_transactions_stats(),
        safe_user->write_instr_class_transactions_stats(),
        safe_host->write_instr_class_transactions_stats());
    return;
  }

  if (safe_user != nullptr) {
    /*
      Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  EVENTS_TRANSACTIONS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_transactions(
        thread->write_instr_class_transactions_stats(),
        safe_user->write_instr_class_transactions_stats(),
        &global_transaction_stat);
    return;
  }

  if (safe_host != nullptr) {
    /*
      Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_TRANSACTIONS_SUMMARY_BY_HOST_BY_EVENT_NAME, directly.
    */
    aggregate_all_transactions(
        thread->write_instr_class_transactions_stats(),
        safe_host->write_instr_class_transactions_stats());
    return;
  }

  /*
    Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_THREAD_BY_EVENT_NAME
    to EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME.
  */
  aggregate_all_transactions(thread->write_instr_class_transactions_stats(),
                             &global_transaction_stat);
}

void aggregate_thread_errors(PFS_thread *thread, PFS_account *safe_account,
                             PFS_user *safe_user, PFS_host *safe_host) {
  if (thread->read_instr_class_errors_stats() == nullptr) {
    return;
  }

  if (likely(safe_account != nullptr)) {
    /*
      Aggregate EVENTS_ERRORS_SUMMARY_BY_THREAD_BY_ERROR
      to EVENTS_ERRORS_SUMMARY_BY_ACCOUNT_BY_ERROR.
    */
    aggregate_all_errors(thread->write_instr_class_errors_stats(),
                         safe_account->write_instr_class_errors_stats());

    return;
  }

  if ((safe_user != nullptr) && (safe_host != nullptr)) {
    /*
      Aggregate EVENTS_ERRORS_SUMMARY_BY_THREAD_BY_ERROR to:
      -  EVENTS_ERRORS_SUMMARY_BY_USER_BY_ERROR
      -  EVENTS_ERRORS_SUMMARY_BY_HOST_BY_ERROR
      in parallel.
    */
    aggregate_all_errors(thread->write_instr_class_errors_stats(),
                         safe_user->write_instr_class_errors_stats(),
                         safe_host->write_instr_class_errors_stats());
    return;
  }

  if (safe_user != nullptr) {
    /*
      Aggregate EVENTS_ERRORS_SUMMARY_BY_THREAD_BY_ERROR to:
      -  EVENTS_ERRORS_SUMMARY_BY_USER_BY_ERROR
      -  EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR
      in parallel.
    */
    aggregate_all_errors(thread->write_instr_class_errors_stats(),
                         safe_user->write_instr_class_errors_stats(),
                         &global_error_stat);
    return;
  }

  if (safe_host != nullptr) {
    /*
      Aggregate EVENTS_ERRORS_SUMMARY_BY_THREAD_BY_ERROR
      to EVENTS_ERRORS_SUMMARY_BY_HOST_BY_ERROR, directly.
    */
    aggregate_all_errors(thread->write_instr_class_errors_stats(),
                         safe_host->write_instr_class_errors_stats());
    return;
  }

  /*
    Aggregate EVENTS_ERRORS_SUMMARY_BY_THREAD_BY_ERROR
    to EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR.
  */
  aggregate_all_errors(thread->write_instr_class_errors_stats(),
                       &global_error_stat);
}
void aggregate_thread_memory(bool alive, PFS_thread *thread,
                             PFS_account *safe_account, PFS_user *safe_user,
                             PFS_host *safe_host) {
  if (thread->read_instr_class_memory_stats() == nullptr) {
    return;
  }

  if (likely(safe_account != nullptr)) {
    /*
      Aggregate MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME.
    */
    aggregate_all_memory_with_reassign(
        alive, thread->write_instr_class_memory_stats(),
        safe_account->write_instr_class_memory_stats(),
        global_instr_class_memory_array);

    return;
  }

  if ((safe_user != nullptr) && (safe_host != nullptr)) {
    /*
      Aggregate MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME
      -  MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_memory_with_reassign(
        alive, thread->write_instr_class_memory_stats(),
        safe_user->write_instr_class_memory_stats(),
        safe_host->write_instr_class_memory_stats(),
        global_instr_class_memory_array);
    return;
  }

  if (safe_user != nullptr) {
    /*
      Aggregate MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME
      -  MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_memory_with_reassign(
        alive, thread->write_instr_class_memory_stats(),
        safe_user->write_instr_class_memory_stats(),
        global_instr_class_memory_array, global_instr_class_memory_array);
    return;
  }

  if (safe_host != nullptr) {
    /*
      Aggregate MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME, directly.
    */
    aggregate_all_memory_with_reassign(
        alive, thread->write_instr_class_memory_stats(),
        safe_host->write_instr_class_memory_stats(),
        global_instr_class_memory_array);
    return;
  }

  /*
    Aggregate MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME
    to MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME.
  */
  aggregate_all_memory(alive, thread->write_instr_class_memory_stats(),
                       global_instr_class_memory_array);
}

void clear_thread_account(PFS_thread *thread) {
  if (thread->m_account != nullptr) {
    thread->m_account->release();
    thread->m_account = nullptr;
  }

  if (thread->m_user != nullptr) {
    thread->m_user->release();
    thread->m_user = nullptr;
  }

  if (thread->m_host != nullptr) {
    thread->m_host->release();
    thread->m_host = nullptr;
  }
}

void set_thread_account(PFS_thread *thread) {
  assert(thread->m_account == nullptr);
  assert(thread->m_user == nullptr);
  assert(thread->m_host == nullptr);

  thread->m_account = find_or_create_account(thread, &thread->m_user_name,
                                             &thread->m_host_name);

  if ((thread->m_account == nullptr) && (thread->m_user_name.length() > 0))
    thread->m_user = find_or_create_user(thread, &thread->m_user_name);

  if ((thread->m_account == nullptr) && (thread->m_host_name.length() > 0))
    thread->m_host = find_or_create_host(thread, &thread->m_host_name);
}

static void fct_update_mutex_derived_flags(PFS_mutex *pfs) {
  PFS_mutex_class *klass = sanitize_mutex_class(pfs->m_class);
  if (likely(klass != nullptr)) {
    pfs->m_enabled = klass->m_enabled && flag_global_instrumentation;
    pfs->m_timed = klass->m_timed;
  } else {
    pfs->m_enabled = false;
    pfs->m_timed = false;
  }
}

void update_mutex_derived_flags() {
  global_mutex_container.apply_all(fct_update_mutex_derived_flags);
}

static void fct_update_rwlock_derived_flags(PFS_rwlock *pfs) {
  PFS_rwlock_class *klass = sanitize_rwlock_class(pfs->m_class);
  if (likely(klass != nullptr)) {
    pfs->m_enabled = klass->m_enabled && flag_global_instrumentation;
    pfs->m_timed = klass->m_timed;
  } else {
    pfs->m_enabled = false;
    pfs->m_timed = false;
  }
}

void update_rwlock_derived_flags() {
  global_rwlock_container.apply_all(fct_update_rwlock_derived_flags);
}

static void fct_update_cond_derived_flags(PFS_cond *pfs) {
  PFS_cond_class *klass = sanitize_cond_class(pfs->m_class);
  if (likely(klass != nullptr)) {
    pfs->m_enabled = klass->m_enabled && flag_global_instrumentation;
    pfs->m_timed = klass->m_timed;
  } else {
    pfs->m_enabled = false;
    pfs->m_timed = false;
  }
}

void update_cond_derived_flags() {
  global_cond_container.apply_all(fct_update_cond_derived_flags);
}

static void fct_update_file_derived_flags(PFS_file *pfs) {
  PFS_file_class *klass = sanitize_file_class(pfs->m_class);
  if (likely(klass != nullptr)) {
    pfs->m_enabled = klass->m_enabled && flag_global_instrumentation;
    pfs->m_timed = klass->m_timed;
  } else {
    pfs->m_enabled = false;
    pfs->m_timed = false;
  }
}

void update_file_derived_flags() {
  global_file_container.apply_all(fct_update_file_derived_flags);
}

static void fct_update_table_derived_flags(PFS_table *pfs) {
  PFS_table_share *share = sanitize_table_share(pfs->m_share);
  if (likely(share != nullptr)) {
    pfs->m_io_enabled = share->m_enabled && flag_global_instrumentation &&
                        global_table_io_class.m_enabled;
    pfs->m_io_timed = share->m_timed && global_table_io_class.m_timed;
    pfs->m_lock_enabled = share->m_enabled && flag_global_instrumentation &&
                          global_table_lock_class.m_enabled;
    pfs->m_lock_timed = share->m_timed && global_table_lock_class.m_timed;
  } else {
    pfs->m_io_enabled = false;
    pfs->m_io_timed = false;
    pfs->m_lock_enabled = false;
    pfs->m_lock_timed = false;
  }
}

void update_table_derived_flags() {
  global_table_container.apply_all(fct_update_table_derived_flags);
}

static void fct_update_socket_derived_flags(PFS_socket *pfs) {
  PFS_socket_class *klass = sanitize_socket_class(pfs->m_class);
  if (likely(klass != nullptr)) {
    pfs->m_enabled = klass->m_enabled && flag_global_instrumentation;
    pfs->m_timed = klass->m_timed;
  } else {
    pfs->m_enabled = false;
    pfs->m_timed = false;
  }
}

void update_socket_derived_flags() {
  global_socket_container.apply_all(fct_update_socket_derived_flags);
}

static void fct_reset_metadata_source_file_pointers(PFS_metadata_lock *pfs) {
  pfs->m_src_file = nullptr;
}

static void fct_update_metadata_derived_flags(PFS_metadata_lock *pfs) {
  pfs->m_enabled =
      global_metadata_class.m_enabled && flag_global_instrumentation;
  pfs->m_timed = global_metadata_class.m_timed;
}

void update_metadata_derived_flags() {
  global_mdl_container.apply_all(fct_update_metadata_derived_flags);
}

static void fct_update_thread_derived_flags(PFS_thread *pfs) {
  pfs->set_history_derived_flags();
}

void update_thread_derived_flags() {
  global_thread_container.apply(fct_update_thread_derived_flags);
}

void update_instruments_derived_flags() {
  update_mutex_derived_flags();
  update_rwlock_derived_flags();
  update_cond_derived_flags();
  update_file_derived_flags();
  update_table_derived_flags();
  update_socket_derived_flags();
  update_metadata_derived_flags();
  /* nothing for stages, statements and transactions (no instances) */
}

/**
  For each thread, clear the source file pointers from all waits, stages,
  statements and transaction events.
*/
static void fct_reset_source_file_pointers(PFS_thread *pfs_thread) {
  /* EVENTS_WAITS_CURRENT */
  PFS_events_waits *wait = pfs_thread->m_events_waits_stack;
  PFS_events_waits *wait_last = wait + WAIT_STACK_SIZE;
  for (; wait < wait_last; wait++) {
    wait->m_source_file = nullptr;
  }

  /* EVENTS_WAITS_HISTORY */
  wait = pfs_thread->m_waits_history;
  wait_last = wait + events_waits_history_per_thread;
  for (; wait < wait_last; wait++) {
    wait->m_source_file = nullptr;
  }

  /* EVENTS_STAGES_CURRENT */
  pfs_thread->m_stage_current.m_source_file = nullptr;

  /* EVENTS_STAGES_HISTORY */
  PFS_events_stages *stage = pfs_thread->m_stages_history;
  PFS_events_stages *stage_last = stage + events_stages_history_per_thread;
  for (; stage < stage_last; stage++) {
    stage->m_source_file = nullptr;
  }

  /* EVENTS_STATEMENTS_CURRENT */
  PFS_events_statements *stmt = &pfs_thread->m_statement_stack[0];
  PFS_events_statements *stmt_last = stmt + statement_stack_max;
  for (; stmt < stmt_last; stmt++) {
    stmt->m_source_file = nullptr;
  }

  /* EVENTS_STATEMENTS_HISTORY */
  stmt = pfs_thread->m_statements_history;
  stmt_last = stmt + events_statements_history_per_thread;
  for (; stmt < stmt_last; stmt++) {
    stmt->m_source_file = nullptr;
  }

  /* EVENTS_TRANSACTIONS_CURRENT */
  pfs_thread->m_transaction_current.m_source_file = nullptr;

  /* EVENTS_TRANSACTIONS_HISTORY */
  PFS_events_transactions *trx = pfs_thread->m_transactions_history;
  PFS_events_transactions *trx_last =
      trx + events_transactions_history_per_thread;
  for (; trx < trx_last; trx++) {
    trx->m_source_file = nullptr;
  }
}

/**
  Clear the source file pointers from all waits, stages, statements and
  transaction events. This function is called whenever a plugin or component
  is unloaded.

  The Performance Schema stores the source file and line number for wait,
  stage, statement, transaction and metadata lock events. The source file
  string pointer is taken from the __FILE__ macro. Source file pointers that
  reference files within a shared library become invalid when the shared
  library is unloaded, therefore all source file pointers are set to NULL
  whenever a plugin or component is unloaded.
*/
void reset_source_file_pointers() {
  /* Clear source file pointers from EVENTS_*_CURRENT and HISTORY tables. */
  global_thread_container.apply(fct_reset_source_file_pointers);

  /* Clear source file pointers from METADATA_LOCKS. */
  global_mdl_container.apply_all(fct_reset_metadata_source_file_pointers);

  /* EVENTS_WAITS_HISTORY_LONG */
  PFS_events_waits *wait = events_waits_history_long_array;
  PFS_events_waits *wait_last = wait + events_waits_history_long_size;
  for (; wait < wait_last; wait++) {
    wait->m_source_file = nullptr;
  }

  /* EVENTS_STAGES_HISTORY_LONG */
  PFS_events_stages *stage = events_stages_history_long_array;
  PFS_events_stages *stage_last = stage + events_stages_history_long_size;
  for (; stage < stage_last; stage++) {
    stage->m_source_file = nullptr;
  }

  /* EVENTS_STATEMENTS_HISTORY_LONG */
  PFS_events_statements *stmt = events_statements_history_long_array;
  PFS_events_statements *stmt_last = stmt + events_statements_history_long_size;
  for (; stmt < stmt_last; stmt++) {
    stmt->m_source_file = nullptr;
  }

  /* EVENTS_TRANSACTIONS_HISTORY_LONG */
  PFS_events_transactions *trx = events_transactions_history_long_array;
  PFS_events_transactions *trx_last =
      trx + events_transactions_history_long_size;
  for (; trx < trx_last; trx++) {
    trx->m_source_file = nullptr;
  }
}

/** @} */
