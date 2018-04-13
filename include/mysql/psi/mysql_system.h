/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_SYSTEM_H
#define MYSQL_SYSTEM_H

/**
  @file include/mysql/psi/mysql_system.h
  Instrumentation helpers for system-level events.
*/

#include "my_psi_config.h"  // IWYU pragma: keep
#include "mysql/psi/psi_system.h"
#ifdef MYSQL_SERVER
#ifndef MYSQL_DYNAMIC_PLUGIN
#include "pfs_system_provider.h"
#endif
#endif

#ifndef PSI_SYSTEM_CALL
#define PSI_SYSTEM_CALL(M) psi_system_service->M
#endif

/**
  @defgroup psi_api_system Thread Instrumentation (API)
  @ingroup psi_api
  @{
*/

/** @} (end of group psi_api_system) */

#endif
