/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/pfs_memory.cc
  Memory statistics aggregation (implementation).
*/

#include "my_global.h"
#include "my_sys.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_account.h"
#include "pfs_host.h"
#include "pfs_user.h"
#include "pfs_atomic.h"
#include "m_string.h"

/** Reset table MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME data. */
void reset_memory_by_thread()
{
  PFS_thread *pfs= thread_array;
  PFS_thread *pfs_last= thread_array + thread_max;
  PFS_account *account;
  PFS_user *user;
  PFS_host *host;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
    {
      account= sanitize_account(pfs->m_account);
      user= sanitize_user(pfs->m_user);
      host= sanitize_host(pfs->m_host);
      aggregate_thread_memory(true, pfs, account, user, host);
    }
  }
}

/** Reset table MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME data. */
void reset_memory_by_account()
{
  PFS_account *pfs= account_array;
  PFS_account *pfs_last= account_array + account_max;
  PFS_user *user;
  PFS_host *host;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
    {
      user= sanitize_user(pfs->m_user);
      host= sanitize_host(pfs->m_host);
      pfs->aggregate_memory(true, user, host);
    }
  }
}

/** Reset table MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME data. */
void reset_memory_by_user()
{
  PFS_user *pfs= user_array;
  PFS_user *pfs_last= user_array + user_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
      pfs->aggregate_memory(true);
  }
}

/** Reset table MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME data. */
void reset_memory_by_host()
{
  PFS_host *pfs= host_array;
  PFS_host *pfs_last= host_array + host_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
      pfs->aggregate_memory(true);
  }
}

/** Reset table MEMORY_GLOBAL_BY_EVENT_NAME data. */
void reset_memory_global()
{
  PFS_memory_stat *stat= global_instr_class_memory_array;
  PFS_memory_stat *stat_last= global_instr_class_memory_array + memory_class_max;

  for ( ; stat < stat_last; stat++)
    stat->rebase();
}

