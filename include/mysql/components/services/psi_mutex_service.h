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

#ifndef COMPONENTS_SERVICES_PSI_MUTEX_SERVICE_H
#define COMPONENTS_SERVICES_PSI_MUTEX_SERVICE_H

#include <mysql/components/services/psi_mutex_bits.h>
#include <mysql/components/service.h>

BEGIN_SERVICE_DEFINITION(psi_mutex_v1)
  /** @sa register_mutex_v1_t. */
  register_mutex_v1_t register_mutex;
  /** @sa init_mutex_v1_t. */
  init_mutex_v1_t init_mutex;
  /** @sa destroy_mutex_v1_t. */
  destroy_mutex_v1_t destroy_mutex;
  /** @sa start_mutex_wait_v1_t. */
  start_mutex_wait_v1_t start_mutex_wait;
  /** @sa end_mutex_wait_v1_t. */
  end_mutex_wait_v1_t end_mutex_wait;
  /** @sa unlock_mutex_v1_t. */
  unlock_mutex_v1_t unlock_mutex;
END_SERVICE_DEFINITION(psi_mutex_v1)

#define REQUIRES_PSI_MUTEX_SERVICE REQUIRES_SERVICE(psi_mutex_v1)

#endif /* COMPONENTS_SERVICES_PSI_MUTEX_SERVICE_H */

