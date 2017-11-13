/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_PSI_IDLE_H
#define MYSQL_PSI_IDLE_H

/**
  @file include/mysql/psi/psi_idle.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_idle Idle Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_inttypes.h"
#include "my_macros.h"
#include "my_psi_config.h"  // IWYU pragma: keep
#include "my_sharedlib.h"
#include "mysql/components/services/psi_idle_bits.h"

/**
  @def PSI_IDLE_VERSION_1
  Performance Schema Idle Interface number for version 1.
  This version is supported.
*/
#define PSI_IDLE_VERSION_1 1

/**
  @def PSI_CURRENT_IDLE_VERSION
  Performance Schema Idle Interface number for the most recent version.
  The most current version is @c PSI_IDLE_VERSION_1
*/
#define PSI_CURRENT_IDLE_VERSION 1

/** Entry point for the performance schema interface. */
struct PSI_idle_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_IDLE_VERSION_1
    @sa PSI_IDLE_VERSION_2
    @sa PSI_CURRENT_IDLE_VERSION
  */
  void *(*get_interface)(int version);
};
typedef struct PSI_idle_bootstrap PSI_idle_bootstrap;

#ifdef HAVE_PSI_IDLE_INTERFACE

/**
  Performance Schema Idle Interface, version 1.
  @since PSI_IDLE_VERSION_1
*/
struct PSI_idle_service_v1
{
  /** @sa start_idle_wait_v1_t. */
  start_idle_wait_v1_t start_idle_wait;
  /** @sa end_idle_wait_v1_t. */
  end_idle_wait_v1_t end_idle_wait;
};

typedef struct PSI_idle_service_v1 PSI_idle_service_t;

extern MYSQL_PLUGIN_IMPORT PSI_idle_service_t *psi_idle_service;

#endif /* HAVE_PSI_IDLE_INTERFACE */

/** @} (end of group psi_abi_idle) */

#endif /* MYSQL_PSI_IDLE_H */
