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

#ifndef MYSQL_ERROR_H
#define MYSQL_ERROR_H

/**
  @file include/mysql/psi/mysql_error.h
  Instrumentation helpers for errors.
*/

#include "mysql/psi/psi_error.h"

#ifndef PSI_ERROR_CALL
#define PSI_ERROR_CALL(M) psi_error_service->M
#endif

/**
  @defgroup psi_api_error Error instrumentation (API)
  @ingroup psi_api
  @{
*/

/**
  @def MYSQL_LOG_ERROR(E, N)
  Instrumented metadata lock destruction.
  @param N Error number
  @param T Error operation
*/
#ifdef HAVE_PSI_ERROR_INTERFACE
#define MYSQL_LOG_ERROR(N, T) inline_mysql_log_error(N, T)
#else
#define MYSQL_LOG_ERROR(N, T) \
  do                          \
  {                           \
  } while (0)
#endif

#ifdef HAVE_PSI_ERROR_INTERFACE

static inline void
inline_mysql_log_error(int error_num, PSI_error_operation error_operation)
{
  PSI_ERROR_CALL(log_error)(error_num, error_operation);
}
#endif /* HAVE_PSI_ERROR_INTERFACE */

/** @} (end of group psi_api_error) */

#endif
