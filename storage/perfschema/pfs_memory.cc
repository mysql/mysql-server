/* Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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
  @file storage/perfschema/pfs_memory.cc
  Memory statistics aggregation (implementation).
*/

#include "storage/perfschema/pfs_memory.h"

#include <atomic>

#include "m_string.h"
#include "my_sys.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_user.h"

static void fct_reset_memory_by_thread(PFS_thread *pfs) {
  PFS_account *account = sanitize_account(pfs->m_account);
  PFS_user *user = sanitize_user(pfs->m_user);
  PFS_host *host = sanitize_host(pfs->m_host);
  aggregate_thread_memory(true, pfs, account, user, host);
}

/** Reset table MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME data. */
void reset_memory_by_thread() {
  global_thread_container.apply(fct_reset_memory_by_thread);
}

static void fct_reset_memory_by_account(PFS_account *pfs) {
  PFS_user *user = sanitize_user(pfs->m_user);
  PFS_host *host = sanitize_host(pfs->m_host);
  pfs->aggregate_memory(true, user, host);
}

/** Reset table MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME data. */
void reset_memory_by_account() {
  global_account_container.apply(fct_reset_memory_by_account);
}

static void fct_reset_memory_by_user(PFS_user *pfs) {
  pfs->aggregate_memory(true);
}

/** Reset table MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME data. */
void reset_memory_by_user() {
  global_user_container.apply(fct_reset_memory_by_user);
}

static void fct_reset_memory_by_host(PFS_host *pfs) {
  pfs->aggregate_memory(true);
}

/** Reset table MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME data. */
void reset_memory_by_host() {
  global_host_container.apply(fct_reset_memory_by_host);
}

/** Reset table MEMORY_GLOBAL_BY_EVENT_NAME data. */
void reset_memory_global() {
  PFS_memory_shared_stat *stat = global_instr_class_memory_array;
  PFS_memory_shared_stat *stat_last =
      global_instr_class_memory_array + memory_class_max;

  for (; stat < stat_last; stat++) {
    stat->rebase();
  }
}
