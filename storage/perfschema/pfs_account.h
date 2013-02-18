/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef PFS_ACCOUNT_H
#define PFS_ACCOUNT_H

/**
  @file storage/perfschema/pfs_account.h
  Performance schema account (declarations).
*/

#include "pfs_lock.h"
#include "lf.h"
#include "pfs_con_slice.h"

struct PFS_global_param;
struct PFS_user;
struct PFS_host;
struct PFS_thread;

/**
  @addtogroup Performance_schema_buffers
  @{
*/

/** Hash key for an account. */
struct PFS_account_key
{
  /**
    Hash search key.
    This has to be a string for LF_HASH,
    the format is "<username><0x00><hostname><0x00>"
  */
  char m_hash_key[USERNAME_LENGTH + 1 + HOSTNAME_LENGTH + 1];
  uint m_key_length;
};

/** Per account statistics. */
struct PFS_ALIGNED PFS_account : PFS_connection_slice
{
public:
  inline void init_refcount(void)
  {
    PFS_atomic::store_32(& m_refcount, 1);
  }

  inline int get_refcount(void)
  {
    return PFS_atomic::load_32(& m_refcount);
  }

  inline void inc_refcount(void)
  {
    PFS_atomic::add_32(& m_refcount, 1);
  }

  inline void dec_refcount(void)
  {
    PFS_atomic::add_32(& m_refcount, -1);
  }

  void aggregate(void);
  void aggregate_waits(void);
  void aggregate_stages(void);
  void aggregate_statements(void);
  void aggregate_stats(void);
  void release(void);

  /** Internal lock. */
  pfs_lock m_lock;
  PFS_account_key m_key;
  const char *m_username;
  uint m_username_length;
  const char *m_hostname;
  uint m_hostname_length;
  PFS_user *m_user;
  PFS_host *m_host;

  ulonglong m_disconnected_count;

private:
  int m_refcount;
};

int init_account(const PFS_global_param *param);
void cleanup_account(void);
int init_account_hash(void);
void cleanup_account_hash(void);

PFS_account *
find_or_create_account(PFS_thread *thread,
                         const char *username, uint username_length,
                         const char *hostname, uint hostname_length);

PFS_account *sanitize_account(PFS_account *unsafe);
void purge_all_account(void);


/* For iterators and show status. */

extern ulong account_max;
extern ulong account_lost;

/* Exposing the data directly, for iterators. */

extern PFS_account *account_array;

extern LF_HASH account_hash;

/** @} */
#endif

