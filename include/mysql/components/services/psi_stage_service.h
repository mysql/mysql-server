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

#ifndef COMPONENTS_SERVICES_PSI_STAGE_SERVICE_H
#define COMPONENTS_SERVICES_PSI_STAGE_SERVICE_H

#include <mysql/components/services/psi_stage_bits.h>
#include <mysql/components/service.h>

BEGIN_SERVICE_DEFINITION(psi_stage_v1)
  /** @sa register_stage_v1_t. */
  register_stage_v1_t register_stage;
  /** @sa start_stage_v1_t. */
  start_stage_v1_t start_stage;
  /** @sa get_current_stage_progress_v1_t. */
  get_current_stage_progress_v1_t get_current_stage_progress;
  /** @sa end_stage_v1_t. */
  end_stage_v1_t end_stage;
END_SERVICE_DEFINITION(psi_stage_v1)

#define REQUIRES_PSI_STAGE_SERVICE REQUIRES_SERVICE(psi_stage_v1)

#endif /* COMPONENTS_SERVICES_PSI_STAGE_SERVICE_H */

