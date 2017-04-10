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

#ifndef MYSQL_PSI_ERROR_H
#define MYSQL_PSI_ERROR_H

/**
  @file include/mysql/psi/psi_error.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_error Error Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_macros.h"
#include "my_psi_config.h"  // IWYU pragma: keep
#include "my_sharedlib.h"
#include "psi_base.h"

C_MODE_START

#ifdef HAVE_PSI_INTERFACE

/**
  @def PSI_ERROR_VERSION_1
  Performance Schema Error Interface number for version 1.
  This version is supported.
*/
#define PSI_ERROR_VERSION_1 1

/**
  @def PSI_ERROR_VERSION_2
  Performance Schema Error Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_ERROR_VERSION_2 2

/**
  @def PSI_CURRENT_ERROR_VERSION
  Performance Schema Error Interface number for the most recent version.
  The most current version is @c PSI_ERROR_VERSION_1
*/
#define PSI_CURRENT_ERROR_VERSION 1

#ifndef USE_PSI_ERROR_2
#ifndef USE_PSI_ERROR_1
#define USE_PSI_ERROR_1
#endif /* USE_PSI_ERROR_1 */
#endif /* USE_PSI_ERROR_2 */

#ifdef USE_PSI_ERROR_1
#define HAVE_PSI_ERROR_1
#endif /* USE_PSI_ERROR_1 */

#ifdef USE_PSI_ERROR_2
#define HAVE_PSI_ERROR_2
#endif /* USE_PSI_ERROR_2 */

/** Entry point for the performance schema interface. */
struct PSI_error_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_ERROR_VERSION_1
    @sa PSI_ERROR_VERSION_2
    @sa PSI_CURRENT_ERROR_VERSION
  */
  void* (*get_interface)(int version);
};
typedef struct PSI_error_bootstrap PSI_error_bootstrap;

#ifdef HAVE_PSI_ERROR_1

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

/**
  Performance Schema Error Interface, version 1.
  @since PSI_ERROR_VERSION_1
*/
struct PSI_error_service_v1
{
  /** @sa log_error_v1_t. */
  log_error_v1_t log_error;
};

#endif /* HAVE_PSI_ERROR_1 */

/* Export the required version */
#ifdef USE_PSI_ERROR_1
typedef struct PSI_error_service_v1 PSI_error_service_t;
#else
typedef struct PSI_placeholder PSI_error_service_t;
#endif

extern MYSQL_PLUGIN_IMPORT PSI_error_service_t* psi_error_service;

/** @} (end of group psi_abi_error) */

#endif /* HAVE_PSI_INTERFACE */

C_MODE_END

#endif /* MYSQL_PSI_ERROR_H */
