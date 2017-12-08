/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_RWLOCK_PROVIDER_H
#define PFS_RWLOCK_PROVIDER_H

/**
  @file include/pfs_rwlock_provider.h
  Performance schema instrumentation (declarations).
*/

#include <sys/types.h>

#include "my_psi_config.h"

#ifdef HAVE_PSI_RWLOCK_INTERFACE
#ifdef MYSQL_SERVER
#ifndef MYSQL_DYNAMIC_PLUGIN

#include "my_inttypes.h"
#include "my_macros.h"
#include "mysql/psi/psi_rwlock.h"

#define PSI_RWLOCK_CALL(M) pfs_ ## M ## _v1

void pfs_register_rwlock_v1(const char *category,
                            PSI_rwlock_info_v1 *info,
                            int count);

PSI_rwlock*
pfs_init_rwlock_v1(PSI_rwlock_key key, const void *identity);

void pfs_destroy_rwlock_v1(PSI_rwlock* rwlock);

PSI_rwlock_locker*
pfs_start_rwlock_rdwait_v1(PSI_rwlock_locker_state *state,
                           PSI_rwlock *rwlock,
                           PSI_rwlock_operation op,
                           const char *src_file, uint src_line);

PSI_rwlock_locker*
pfs_start_rwlock_wrwait_v1(PSI_rwlock_locker_state *state,
                           PSI_rwlock *rwlock,
                           PSI_rwlock_operation op,
                           const char *src_file, uint src_line);

void pfs_unlock_rwlock_v1(PSI_rwlock *rwlock);

void pfs_end_rwlock_rdwait_v1(PSI_rwlock_locker* locker, int rc);

void pfs_end_rwlock_wrwait_v1(PSI_rwlock_locker* locker, int rc);

#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVER */
#endif /* HAVE_PSI_RWLOCK_INTERFACE */

#endif

