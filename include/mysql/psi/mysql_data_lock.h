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

#ifndef MYSQL_DATA_LOCK_H
#define MYSQL_DATA_LOCK_H

/**
  @file include/mysql/psi/mysql_data_lock.h
  Instrumentation helpers for data locks.
*/

#include "mysql/psi/psi_data_lock.h"

#ifndef PSI_DATA_LOCK_CALL
#define PSI_DATA_LOCK_CALL(M) psi_data_lock_service->M
#endif

/**
  @defgroup psi_api_data_lock Data Lock Instrumentation (API)
  @ingroup psi_api
  @{
*/

#define mysql_data_lock_register(I) inline_mysql_data_lock_register(I)

void
inline_mysql_data_lock_register(
#ifdef HAVE_PSI_DATA_LOCK_INTERFACE
  PSI_engine_data_lock_inspector *i
#else
  PSI_engine_data_lock_inspector *i MY_ATTRIBUTE((unused))
#endif /* HAVE_PSI_DATA_LOCK_INTERFACE */
  )
{
#ifdef HAVE_PSI_DATA_LOCK_INTERFACE
  PSI_DATA_LOCK_CALL(register_data_lock)(i);
#endif /* HAVE_PSI_DATA_LOCK_INTERFACE */
}

#define mysql_data_lock_unregister(I) inline_mysql_data_lock_unregister(I)

void
inline_mysql_data_lock_unregister(
#ifdef HAVE_PSI_DATA_LOCK_INTERFACE
  PSI_engine_data_lock_inspector *i
#else
  PSI_engine_data_lock_inspector *i MY_ATTRIBUTE((unused))
#endif /* HAVE_PSI_DATA_LOCK_INTERFACE */
  )
{
#ifdef HAVE_PSI_DATA_LOCK_INTERFACE
  PSI_DATA_LOCK_CALL(unregister_data_lock)(i);
#endif /* HAVE_PSI_DATA_LOCK_INTERFACE */
}

/** @} (end of group psi_api_data_lock) */

#endif
