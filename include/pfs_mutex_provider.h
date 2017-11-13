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

#ifndef PFS_MUTEX_PROVIDER_H
#define PFS_MUTEX_PROVIDER_H

/**
  @file include/pfs_mutex_provider.h
  Performance schema instrumentation (declarations).
*/

#include <sys/types.h>

#include "my_psi_config.h"

#ifdef HAVE_PSI_MUTEX_INTERFACE
#ifdef MYSQL_SERVER
#ifndef MYSQL_DYNAMIC_PLUGIN

#include "my_inttypes.h"
#include "my_macros.h"
#include "mysql/psi/psi_mutex.h"

#define PSI_MUTEX_CALL(M) pfs_ ## M ## _v1

void pfs_register_mutex_v1(const char *category,
                           PSI_mutex_info_v1 *info,
                           int count);

PSI_mutex*
pfs_init_mutex_v1(PSI_mutex_key key, const void *identity);

void pfs_destroy_mutex_v1(PSI_mutex* mutex);

PSI_mutex_locker*
pfs_start_mutex_wait_v1(PSI_mutex_locker_state *state,
                        PSI_mutex *mutex, PSI_mutex_operation op,
                        const char *src_file, uint src_line);

void pfs_unlock_mutex_v1(PSI_mutex *mutex);

void pfs_end_mutex_wait_v1(PSI_mutex_locker* locker, int rc);

#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVER */
#endif /* HAVE_PSI_MUTEX_INTERFACE */

#endif /* PFS_MUTEX_PROVIDER_H */

