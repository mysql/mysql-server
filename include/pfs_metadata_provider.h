/* Copyright (c) 2012, 2024, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PFS_METADATA_PROVIDER_H
#define PFS_METADATA_PROVIDER_H

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

/**
  @file include/pfs_metadata_provider.h
  Performance schema instrumentation (declarations).
*/

#ifdef HAVE_PSI_METADATA_INTERFACE
#if defined(MYSQL_SERVER) || defined(PFS_DIRECT_CALL)
#ifndef MYSQL_DYNAMIC_PLUGIN
#ifndef WITH_LOCK_ORDER

#include <sys/types.h>

#include "my_inttypes.h"
#include "my_macros.h"
#include "mysql/psi/psi_mdl.h"

struct MDL_key;

#define PSI_METADATA_CALL(M) pfs_##M##_vc

PSI_metadata_lock *pfs_create_metadata_lock_vc(
    void *identity, const MDL_key *key, opaque_mdl_type mdl_type,
    opaque_mdl_duration mdl_duration, opaque_mdl_status mdl_status,
    const char *src_file, uint src_line);

void pfs_set_metadata_lock_status_vc(PSI_metadata_lock *lock,
                                     opaque_mdl_status mdl_status);

void pfs_set_metadata_lock_duration_vc(PSI_metadata_lock *lock,
                                       opaque_mdl_duration mdl_duration);

void pfs_destroy_metadata_lock_vc(PSI_metadata_lock *lock);

struct PSI_metadata_locker *pfs_start_metadata_wait_vc(
    struct PSI_metadata_locker_state_v1 *state, struct PSI_metadata_lock *mdl,
    const char *src_file, uint src_line);

void pfs_end_metadata_wait_vc(struct PSI_metadata_locker *locker, int rc);

#endif /* WITH_LOCK_ORDER */
#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVER || PFS_DIRECT_CALL */
#endif /* HAVE_PSI_METADATA_INTERFACE */

#endif
