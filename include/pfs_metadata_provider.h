/* Copyright (c) 2012, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_METADATA_PROVIDER_H
#define PFS_METADATA_PROVIDER_H

/**
  @file include/pfs_metadata_provider.h
  Performance schema instrumentation (declarations).
*/

#ifdef HAVE_PSI_METADATA_INTERFACE
#ifdef MYSQL_SERVER
#ifndef EMBEDDED_LIBRARY
#ifndef MYSQL_DYNAMIC_PLUGIN

#include "mysql/psi/psi.h"

#define PSI_METADATA_CALL(M) pfs_ ## M ## _v1

C_MODE_START

PSI_metadata_lock* pfs_create_metadata_lock_v1
  (void *identity,
   const MDL_key *key,
   opaque_mdl_type mdl_type,
   opaque_mdl_duration mdl_duration,
   opaque_mdl_status mdl_status,
   const char *src_file,
   uint src_line);

void pfs_set_metadata_lock_status_v1
  (PSI_metadata_lock *lock,
   opaque_mdl_status mdl_status);

void pfs_destroy_metadata_lock_v1(PSI_metadata_lock *lock);

struct PSI_metadata_locker*
pfs_start_metadata_wait_v1
  (struct PSI_metadata_locker_state_v1 *state,
   struct PSI_metadata_lock *mdl,
   const char *src_file, uint src_line);

void pfs_end_metadata_wait_v1
  (struct PSI_metadata_locker *locker, int rc);

C_MODE_END

#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* EMBEDDED_LIBRARY */
#endif /* MYSQL_SERVER */
#endif /* HAVE_PSI_METADATA_INTERFACE */

#endif

