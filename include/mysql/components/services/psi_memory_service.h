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

#ifndef COMPONENTS_SERVICES_PSI_MEMORY_SERVICE_H
#define COMPONENTS_SERVICES_PSI_MEMORY_SERVICE_H

#include <mysql/components/services/psi_memory_bits.h>
#include <mysql/components/service.h>

BEGIN_SERVICE_DEFINITION(psi_memory_v1)
  /** @sa register_memory_v1_t. */
  register_memory_v1_t register_memory;
  /** @sa memory_alloc_v1_t. */
  memory_alloc_v1_t memory_alloc;
  /** @sa memory_realloc_v1_t. */
  memory_realloc_v1_t memory_realloc;
  /** @sa memory_claim_v1_t. */
  memory_claim_v1_t memory_claim;
  /** @sa memory_free_v1_t. */
  memory_free_v1_t memory_free;
END_SERVICE_DEFINITION(psi_memory_v1)

#define REQUIRES_PSI_MEMORY_SERVICE REQUIRES_SERVICE(psi_memory_v1)

#endif /* COMPONENTS_SERVICES_PSI_MEMORY_SERVICE_H */

