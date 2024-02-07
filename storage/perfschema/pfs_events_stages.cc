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
  @file storage/perfschema/pfs_events_stages.cc
  Events stages data structures (implementation).
*/

#include "storage/perfschema/pfs_events_stages.h"

#include <assert.h>
#include <atomic>

#include "m_string.h"
#include "my_compiler.h"

#include "my_inttypes.h"
#include "my_sys.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_builtin_memory.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_user.h"

PFS_ALIGNED ulong events_stages_history_long_size = 0;
/** Consumer flag for table EVENTS_STAGES_CURRENT. */
PFS_ALIGNED bool flag_events_stages_current = false;
/** Consumer flag for table EVENTS_STAGES_HISTORY. */
PFS_ALIGNED bool flag_events_stages_history = false;
/** Consumer flag for table EVENTS_STAGES_HISTORY_LONG. */
PFS_ALIGNED bool flag_events_stages_history_long = false;

/** True if EVENTS_STAGES_HISTORY_LONG circular buffer is full. */
PFS_ALIGNED bool events_stages_history_long_full = false;
/** Index in EVENTS_STAGES_HISTORY_LONG circular buffer. */
PFS_ALIGNED PFS_cacheline_atomic_uint32 events_stages_history_long_index;
/** EVENTS_STAGES_HISTORY_LONG circular buffer. */
PFS_ALIGNED PFS_events_stages *events_stages_history_long_array = nullptr;

/**
  Initialize table EVENTS_STAGES_HISTORY_LONG.
  @param events_stages_history_long_sizing       table sizing
*/
int init_events_stages_history_long(uint events_stages_history_long_sizing) {
  events_stages_history_long_size = events_stages_history_long_sizing;
  events_stages_history_long_full = false;
  events_stages_history_long_index.m_u32.store(0);

  if (events_stages_history_long_size == 0) {
    events_stages_history_long_array = nullptr;
    return 0;
  }

  events_stages_history_long_array = PFS_MALLOC_ARRAY(
      &builtin_memory_stages_history_long, events_stages_history_long_size,
      sizeof(PFS_events_stages), PFS_events_stages, MYF(MY_ZEROFILL));

  if (events_stages_history_long_array == nullptr) {
    events_stages_history_long_size = 0;
    return 1;
  }

  return 0;
}

/** Cleanup table EVENTS_STAGES_HISTORY_LONG. */
void cleanup_events_stages_history_long() {
  PFS_FREE_ARRAY(&builtin_memory_stages_history_long,
                 events_stages_history_long_size, sizeof(PFS_events_stages),
                 events_stages_history_long_array);
  events_stages_history_long_array = nullptr;
  events_stages_history_long_size = 0;
}

static inline void copy_events_stages(PFS_events_stages *dest,
                                      const PFS_events_stages *source) {
  memcpy(dest, source, sizeof(PFS_events_stages));
}

/**
  Insert a stage record in table EVENTS_STAGES_HISTORY.
  @param thread             thread that executed the wait
  @param stage              record to insert
*/
void insert_events_stages_history(PFS_thread *thread,
                                  PFS_events_stages *stage) {
  if (unlikely(events_stages_history_per_thread == 0)) {
    return;
  }

  assert(thread->m_stages_history != nullptr);

  uint index = thread->m_stages_history_index;

  /*
    A concurrent thread executing TRUNCATE TABLE EVENTS_STAGES_CURRENT
    could alter the data that this thread is inserting,
    causing a potential race condition.
    We are not testing for this and insert a possibly empty record,
    to make this thread (the writer) faster.
    This is ok, the readers of m_stages_history will filter this out.
  */
  copy_events_stages(&thread->m_stages_history[index], stage);

  index++;
  if (index >= events_stages_history_per_thread) {
    index = 0;
    thread->m_stages_history_full = true;
  }
  thread->m_stages_history_index = index;
}

/**
  Insert a stage record in table EVENTS_STAGES_HISTORY_LONG.
  @param stage              record to insert
*/
void insert_events_stages_history_long(PFS_events_stages *stage) {
  if (unlikely(events_stages_history_long_size == 0)) {
    return;
  }

  assert(events_stages_history_long_array != nullptr);

  uint index = events_stages_history_long_index.m_u32++;

  index = index % events_stages_history_long_size;
  if (index == 0) {
    events_stages_history_long_full = true;
  }

  /* See related comment in insert_events_stages_history. */
  copy_events_stages(&events_stages_history_long_array[index], stage);
}

static void fct_reset_events_stages_current(PFS_thread *pfs) {
  pfs->m_stage_current.m_class = nullptr;
}

/** Reset table EVENTS_STAGES_CURRENT data. */
void reset_events_stages_current() {
  global_thread_container.apply_all(fct_reset_events_stages_current);
}

static void fct_reset_events_stages_history(PFS_thread *pfs_thread) {
  PFS_events_stages *pfs = pfs_thread->m_stages_history;
  PFS_events_stages *pfs_last = pfs + events_stages_history_per_thread;

  pfs_thread->m_stages_history_index = 0;
  pfs_thread->m_stages_history_full = false;
  for (; pfs < pfs_last; pfs++) {
    pfs->m_class = nullptr;
  }
}

/** Reset table EVENTS_STAGES_HISTORY data. */
void reset_events_stages_history() {
  global_thread_container.apply_all(fct_reset_events_stages_history);
}

/** Reset table EVENTS_STAGES_HISTORY_LONG data. */
void reset_events_stages_history_long() {
  events_stages_history_long_index.m_u32.store(0);
  events_stages_history_long_full = false;

  PFS_events_stages *pfs = events_stages_history_long_array;
  PFS_events_stages *pfs_last = pfs + events_stages_history_long_size;
  for (; pfs < pfs_last; pfs++) {
    pfs->m_class = nullptr;
  }
}

static void fct_reset_events_stages_by_thread(PFS_thread *thread) {
  PFS_account *account = sanitize_account(thread->m_account);
  PFS_user *user = sanitize_user(thread->m_user);
  PFS_host *host = sanitize_host(thread->m_host);
  aggregate_thread_stages(thread, account, user, host);
}

/** Reset table EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME data. */
void reset_events_stages_by_thread() {
  global_thread_container.apply(fct_reset_events_stages_by_thread);
}

static void fct_reset_events_stages_by_account(PFS_account *pfs) {
  PFS_user *user = sanitize_user(pfs->m_user);
  PFS_host *host = sanitize_host(pfs->m_host);
  pfs->aggregate_stages(user, host);
}

/** Reset table EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME data. */
void reset_events_stages_by_account() {
  global_account_container.apply(fct_reset_events_stages_by_account);
}

static void fct_reset_events_stages_by_user(PFS_user *pfs) {
  pfs->aggregate_stages();
}

/** Reset table EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME data. */
void reset_events_stages_by_user() {
  global_user_container.apply(fct_reset_events_stages_by_user);
}

static void fct_reset_events_stages_by_host(PFS_host *pfs) {
  pfs->aggregate_stages();
}

/** Reset table EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME data. */
void reset_events_stages_by_host() {
  global_host_container.apply(fct_reset_events_stages_by_host);
}

/** Reset table EVENTS_STAGES_GLOBAL_BY_EVENT_NAME data. */
void reset_events_stages_global() {
  PFS_stage_stat *stat = global_instr_class_stages_array;
  PFS_stage_stat *stat_last = global_instr_class_stages_array + stage_class_max;

  for (; stat < stat_last; stat++) {
    stat->reset();
  }
}
