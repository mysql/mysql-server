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

#ifndef PFS_ACCOUNT_H
#define PFS_ACCOUNT_H

/**
  @file storage/perfschema/pfs_account.h
  Performance schema account (declarations).
*/

#include <sys/types.h>
#include <atomic>

#include "lf.h"
#include "my_hostname.h" /* HOSTNAME_LENGTH */
#include "my_inttypes.h"
#include "mysql_com.h" /* USERNAME_LENGTH */
#include "storage/perfschema/pfs_con_slice.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_lock.h"
#include "storage/perfschema/pfs_name.h"

struct PFS_global_param;
struct PFS_user;
struct PFS_host;
struct PFS_thread;
struct PFS_memory_stat_alloc_delta;
struct PFS_memory_stat_free_delta;
struct PFS_memory_shared_stat;

/**
  @addtogroup performance_schema_buffers
  @{
*/

/** Hash key for an account. */
struct PFS_account_key {
  /** User name. */
  PFS_user_name m_user_name;
  /** Host name. */
  PFS_host_name m_host_name;
};

/** Per account statistics. */
struct PFS_ALIGNED PFS_account : PFS_connection_slice {
 public:
  inline void init_refcount() { m_refcount.store(1); }

  inline int get_refcount() { return m_refcount.load(); }

  inline void inc_refcount() { ++m_refcount; }

  inline void dec_refcount() { --m_refcount; }

  void aggregate(bool alive, PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_waits(PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_stages(PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_statements(PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_transactions(PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_errors(PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_memory(bool alive, PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_status(PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_stats(PFS_user *safe_user, PFS_host *safe_host);
  void release();

  /** Reset all memory statistics. */
  void rebase_memory_stats();

  void carry_memory_stat_alloc_delta(PFS_memory_stat_alloc_delta *delta,
                                     uint index);
  void carry_memory_stat_free_delta(PFS_memory_stat_free_delta *delta,
                                    uint index);

  void set_instr_class_memory_stats(PFS_memory_shared_stat *array) {
    m_has_memory_stats = false;
    m_instr_class_memory_stats = array;
  }

  const PFS_memory_shared_stat *read_instr_class_memory_stats() const {
    if (!m_has_memory_stats) {
      return nullptr;
    }
    return m_instr_class_memory_stats;
  }

  PFS_memory_shared_stat *write_instr_class_memory_stats() {
    if (!m_has_memory_stats) {
      rebase_memory_stats();
      m_has_memory_stats = true;
    }
    return m_instr_class_memory_stats;
  }

  /** Internal lock. */
  pfs_lock m_lock;
  PFS_account_key m_key;
  /** True if this account is enabled, per rules in table SETUP_ACTORS. */
  bool m_enabled;
  /** True if this account has history enabled, per rules in table SETUP_ACTORS.
   */
  bool m_history;
  PFS_user *m_user;
  PFS_host *m_host;

  void reset_connections_stats() {
    m_disconnected_count = 0;
    m_max_controlled_memory = 0;
    m_max_total_memory = 0;
  }

  void aggregate_disconnect(ulonglong controlled_memory,
                            ulonglong total_memory);

  ulonglong m_disconnected_count;
  ulonglong m_max_controlled_memory;
  ulonglong m_max_total_memory;

 private:
  std::atomic<int> m_refcount;

  /**
    Per account memory aggregated statistics.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME.
    Immutable, safe to use without internal lock.
  */
  PFS_memory_shared_stat *m_instr_class_memory_stats{nullptr};
};

int init_account(const PFS_global_param *param);
void cleanup_account();
int init_account_hash(const PFS_global_param *param);
void cleanup_account_hash();

PFS_account *find_or_create_account(PFS_thread *thread,
                                    const PFS_user_name *user,
                                    const PFS_host_name *host);

PFS_account *sanitize_account(PFS_account *unsafe);
void purge_all_account();

void update_accounts_derived_flags(PFS_thread *thread);

/* For show status. */

extern LF_HASH account_hash;

/** @} */
#endif
