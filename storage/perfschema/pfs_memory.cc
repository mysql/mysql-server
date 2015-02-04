/* Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "pfs_buffer_container.h"
#include "m_string.h"

static void fct_reset_memory_by_thread(PFS_thread *pfs)
{
  PFS_account *account= sanitize_account(pfs->m_account);
  PFS_user *user= sanitize_user(pfs->m_user);
  PFS_host *host= sanitize_host(pfs->m_host);
  aggregate_thread_memory(true, pfs, account, user, host);
}

/** Reset table MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME data. */
void reset_memory_by_thread()
{
  global_thread_container.apply(fct_reset_memory_by_thread);
}

static void fct_reset_memory_by_account(PFS_account *pfs)
{
  PFS_user *user= sanitize_user(pfs->m_user);
  PFS_host *host= sanitize_host(pfs->m_host);
  pfs->aggregate_memory(true, user, host);
}

/** Reset table MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME data. */
void reset_memory_by_account()
{
  global_account_container.apply(fct_reset_memory_by_account);
}

static void fct_reset_memory_by_user(PFS_user *pfs)
{
  pfs->aggregate_memory(true);
}

/** Reset table MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME data. */
void reset_memory_by_user()
{
  global_user_container.apply(fct_reset_memory_by_user);
}

static void fct_reset_memory_by_host(PFS_host *pfs)
{
  pfs->aggregate_memory(true);
}

/** Reset table MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME data. */
void reset_memory_by_host()
{
  global_host_container.apply(fct_reset_memory_by_host);
}

/** Reset table MEMORY_GLOBAL_BY_EVENT_NAME data. */
void reset_memory_global()
{
  PFS_memory_stat *stat= global_instr_class_memory_array;
  PFS_memory_stat *stat_last= global_instr_class_memory_array + memory_class_max;

  for ( ; stat < stat_last; stat++)
    stat->rebase();
}

