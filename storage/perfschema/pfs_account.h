/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_ACCOUNT_H
#define PFS_ACCOUNT_H

/**
  @file storage/perfschema/pfs_account.h
  Performance schema account (declarations).
*/

#include <sys/types.h>
#include <atomic>

#include "lf.h"
#include "my_inttypes.h"
#include "mysql_com.h" /* USERNAME_LENGTH */
#include "storage/perfschema/pfs_con_slice.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_lock.h"

struct PFS_global_param;
struct PFS_user;
struct PFS_host;
struct PFS_thread;
struct PFS_memory_stat_delta;

/**
  @addtogroup performance_schema_buffers
  @{
*/

/** Hash key for an account. */
struct PFS_account_key
{
  /**
    Hash search key.
    This has to be a string for @c LF_HASH,
    the format is @c "<username><0x00><hostname><0x00>"
  */
  char m_hash_key[USERNAME_LENGTH + 1 + HOSTNAME_LENGTH + 1];
  uint m_key_length;
};

/** Per account statistics. */
struct PFS_ALIGNED PFS_account : PFS_connection_slice
{
public:
  inline void
  init_refcount(void)
  {
    m_refcount.store(1);
  }

  inline int
  get_refcount(void)
  {
    return m_refcount.load();
  }

  inline void
  inc_refcount(void)
  {
    ++m_refcount;
  }

  inline void
  dec_refcount(void)
  {
    --m_refcount;
  }

  void aggregate(bool alive, PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_waits(PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_stages(PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_statements(PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_transactions(PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_errors(PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_memory(bool alive, PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_status(PFS_user *safe_user, PFS_host *safe_host);
  void aggregate_stats(PFS_user *safe_user, PFS_host *safe_host);
  void release(void);

  void carry_memory_stat_delta(PFS_memory_stat_delta *delta, uint index);

  /** Internal lock. */
  pfs_lock m_lock;
  PFS_account_key m_key;
  /** True if this account is enabled, per rules in table SETUP_ACTORS. */
  bool m_enabled;
  /** True if this account has history enabled, per rules in table SETUP_ACTORS.
   */
  bool m_history;
  const char *m_username;
  uint m_username_length;
  const char *m_hostname;
  uint m_hostname_length;
  PFS_user *m_user;
  PFS_host *m_host;

  ulonglong m_disconnected_count;

private:
  std::atomic<int> m_refcount;
};

int init_account(const PFS_global_param *param);
void cleanup_account(void);
int init_account_hash(const PFS_global_param *param);
void cleanup_account_hash(void);

PFS_account *find_or_create_account(PFS_thread *thread,
                                    const char *username,
                                    uint username_length,
                                    const char *hostname,
                                    uint hostname_length);

PFS_account *sanitize_account(PFS_account *unsafe);
void purge_all_account(void);

void update_accounts_derived_flags(PFS_thread *thread);

/* For show status. */

extern LF_HASH account_hash;

/** @} */
#endif
