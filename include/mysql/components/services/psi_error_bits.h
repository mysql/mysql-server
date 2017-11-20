/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef COMPONENTS_SERVICES_PSI_ERROR_BITS_H
#define COMPONENTS_SERVICES_PSI_ERROR_BITS_H

#include "my_macros.h"

C_MODE_START

/**
  @file
  Performance schema instrumentation interface.

  @defgroup psi_abi_error Error Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

enum PSI_error_operation
{
  PSI_ERROR_OPERATION_RAISED = 0,
  PSI_ERROR_OPERATION_HANDLED
};
typedef enum PSI_error_operation PSI_error_operation;

/**
  Log the error seen in Performance Schema buffers.
  @param num MySQL error number
  @param error_operation operation on error (PSI_ERROR_OPERATION_*)
*/
typedef void (*log_error_v1_t)(unsigned int error_num,
                               PSI_error_operation error_operation);

/** @} (end of group psi_abi_error) */

C_MODE_END

#endif /* COMPONENTS_SERVICES_PSI_ERROR_BITS_H */
