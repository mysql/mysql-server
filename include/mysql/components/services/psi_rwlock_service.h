/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#ifndef COMPONENTS_SERVICES_PSI_RWLOCK_SERVICE_H
#define COMPONENTS_SERVICES_PSI_RWLOCK_SERVICE_H

#include <mysql/components/services/psi_rwlock_bits.h>
#include <mysql/components/service.h>

BEGIN_SERVICE_DEFINITION(psi_rwlock_v1)
  /** @sa register_rwlock_v1_t. */
  register_rwlock_v1_t register_rwlock;
  /** @sa init_rwlock_v1_t. */
  init_rwlock_v1_t init_rwlock;
  /** @sa destroy_rwlock_v1_t. */
  destroy_rwlock_v1_t destroy_rwlock;
  /** @sa start_rwlock_rdwait_v1_t. */
  start_rwlock_rdwait_v1_t start_rwlock_rdwait;
  /** @sa end_rwlock_rdwait_v1_t. */
  end_rwlock_rdwait_v1_t end_rwlock_rdwait;
  /** @sa start_rwlock_wrwait_v1_t. */
  start_rwlock_wrwait_v1_t start_rwlock_wrwait;
  /** @sa end_rwlock_wrwait_v1_t. */
  end_rwlock_wrwait_v1_t end_rwlock_wrwait;
  /** @sa unlock_rwlock_v1_t. */
  unlock_rwlock_v1_t unlock_rwlock;
END_SERVICE_DEFINITION(psi_rwlock_v1)

#define REQUIRES_PSI_RWLOCK_SERVICE REQUIRES_SERVICE(psi_rwlock_v1)

#endif /* COMPONENTS_SERVICES_PSI_RWLOCK_SERVICE_H */

