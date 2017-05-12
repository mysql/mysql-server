/* Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_IDLE_PROVIDER_H
#define PFS_IDLE_PROVIDER_H

/**
  @file include/pfs_idle_provider.h
  Performance schema instrumentation (declarations).
*/

#include "my_psi_config.h"

#ifdef HAVE_PSI_IDLE_INTERFACE
#ifdef MYSQL_SERVER
#ifndef MYSQL_DYNAMIC_PLUGIN

#include <sys/types.h>

#include "my_macros.h"
#include "mysql/psi/psi_idle.h"

#define PSI_IDLE_CALL(M) pfs_ ## M ## _v1

C_MODE_START

PSI_idle_locker*
pfs_start_idle_wait_v1(PSI_idle_locker_state* state, const char *src_file, uint src_line);

void pfs_end_idle_wait_v1(PSI_idle_locker* locker);

C_MODE_END

#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVER */
#endif /* HAVE_PSI_IDLE_INTERFACE */

#endif

