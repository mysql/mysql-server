/* Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_status.cc
  Status variables statistics (implementation).
*/

#include "storage/perfschema/pfs_status.h"

#include <atomic>

#include "my_sys.h"
#include "sql/sql_show.h" /* reset_status_vars */
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_user.h"

PFS_status_stats::PFS_status_stats() { reset(); }

void PFS_status_stats::reset() {
  m_has_stats = false;
  memset(&m_stats, 0, sizeof(m_stats));
}

void PFS_status_stats::aggregate(const PFS_status_stats *from) {
  if (from->m_has_stats) {
    m_has_stats = true;
    for (int i = 0; i < COUNT_GLOBAL_STATUS_VARS; i++) {
      m_stats[i] += from->m_stats[i];
    }
  }
}

void PFS_status_stats::aggregate_from(const System_status_var *from) {
  ulonglong *from_var = (ulonglong *)from;

  m_has_stats = true;
  for (int i = 0; i < COUNT_GLOBAL_STATUS_VARS; i++, from_var++) {
    m_stats[i] += *from_var;
  }
}

void PFS_status_stats::aggregate_to(System_status_var *to) {
  if (m_has_stats) {
    ulonglong *to_var = (ulonglong *)to;

    for (int i = 0; i < COUNT_GLOBAL_STATUS_VARS; i++, to_var++) {
      *to_var += m_stats[i];
    }
  }
}

static void fct_reset_status_by_thread(PFS_thread *thread) {
  PFS_account *account;
  PFS_user *user;
  PFS_host *host;

  if (thread->m_lock.is_populated()) {
    account = sanitize_account(thread->m_account);
    user = sanitize_user(thread->m_user);
    host = sanitize_host(thread->m_host);
    aggregate_thread_status(thread, account, user, host);
  }
}

/** Reset table STATUS_BY_THREAD data. */
void reset_status_by_thread() {
  global_thread_container.apply_all(fct_reset_status_by_thread);
}

static void fct_reset_status_by_account(PFS_account *account) {
  PFS_user *user;
  PFS_host *host;

  if (account->m_lock.is_populated()) {
    user = sanitize_user(account->m_user);
    host = sanitize_host(account->m_host);
    account->aggregate_status(user, host);
  }
}

/** Reset table STATUS_BY_ACCOUNT data. */
void reset_status_by_account() {
  global_account_container.apply_all(fct_reset_status_by_account);
}

static void fct_reset_status_by_user(PFS_user *user) {
  if (user->m_lock.is_populated()) {
    user->aggregate_status();
  }
}

/** Reset table STATUS_BY_USER data. */
void reset_status_by_user() {
  global_user_container.apply_all(fct_reset_status_by_user);
}

static void fct_reset_status_by_host(PFS_host *host) {
  if (host->m_lock.is_populated()) {
    host->aggregate_status();
  }
}

/** Reset table STATUS_BY_HOST data. */
void reset_status_by_host() {
  global_host_container.apply_all(fct_reset_status_by_host);
}

/** Reset table GLOBAL_STATUS data. */
void reset_global_status() {
  /*
    Do not memset global_status_var,
    NO_FLUSH counters need to be preserved
  */
  reset_status_vars();
}
