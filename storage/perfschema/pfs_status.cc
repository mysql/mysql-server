/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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
#include "sql/mysqld.h"   /* reset_status_by_thd */
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
  const auto *from_var = pointer_cast<const ulonglong *>(from);

  m_has_stats = true;
  for (int i = 0; i < COUNT_GLOBAL_STATUS_VARS; i++, from_var++) {
    m_stats[i] += *from_var;
  }
}

void PFS_status_stats::aggregate_to(System_status_var *to) {
  if (m_has_stats) {
    auto *to_var = (ulonglong *)to;

    for (int i = 0; i < COUNT_GLOBAL_STATUS_VARS; i++, to_var++) {
      *to_var += m_stats[i];
    }
  }
}

/** Reset table STATUS_BY_THREAD data. */
void reset_status_by_thread() {
  /*
    TABLE PERFORMANCE_SCHEMA.STATUS_BY_THREAD
    is exposing status variables contained in sql layer class THD,
    so this table is directly coupled with THD.

    One issue is that, despite the fact that memory for PFS_thread
    is always available and can be inspected,
    memory for THD accessed from PFS_thread::m_thd is not safe to
    inspect, as the THD object can be destroyed at any time.

    Instead of:
    - iterating on global_thread_container in the pfs space,
      (complexity O(N))
    - having to find the matching, safe, THD in the sql space
      (complexity O(N) because of Global_THD_manager::find_thd())
    which will lead to a O(N^2) complexity,

    we instead iterate from the sql space directly.

    This creates a dependency on the sql layer,
    but again this is expected given how table STATUS_BY_THREAD
    is by definition coupled with the sql layer.

    Now, because of the guarantees provided by
      Global_THD_manager::do_for_all_thd()
    which is used in reset_status_by_thd(),
    the THD object inspected is safe to use during the scan,
    so the status variables in THD::status_var can be safely accessed.
  */
  reset_status_by_thd();
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
