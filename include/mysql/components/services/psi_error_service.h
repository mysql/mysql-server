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

#ifndef COMPONENTS_SERVICES_PSI_ERROR_SERVICE_H
#define COMPONENTS_SERVICES_PSI_ERROR_SERVICE_H

#include <mysql/components/services/psi_error_bits.h>
#include <mysql/components/service.h>

BEGIN_SERVICE_DEFINITION(psi_error_v1)
  /** @sa log_error_v1_t. */
  log_error_v1_t log_error;
END_SERVICE_DEFINITION(psi_error_v1)

#define REQUIRES_PSI_ERROR_SERVICE REQUIRES_SERVICE(psi_error_v1)

#endif /* COMPONENTS_SERVICES_PSI_ERROR_SERVICE_H */

