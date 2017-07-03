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

#ifndef MYSQL_PSI_COND_H
#define MYSQL_PSI_COND_H

/**
  @file include/mysql/psi/psi_cond.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_cond Cond Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_inttypes.h"
#include "my_macros.h"
#include "my_psi_config.h"  // IWYU pragma: keep
#include "my_sharedlib.h"
#include "psi_base.h"
#include "mysql/components/services/psi_cond_bits.h"

C_MODE_START

/** Entry point for the performance schema interface. */
struct PSI_cond_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_COND_VERSION_1
    @sa PSI_COND_VERSION_2
    @sa PSI_CURRENT_COND_VERSION
  */
  void *(*get_interface)(int version);
};
typedef struct PSI_cond_bootstrap PSI_cond_bootstrap;

#ifdef HAVE_PSI_COND_INTERFACE

/**
  Performance Schema Cond Interface, version 1.
  @since PSI_COND_VERSION_1
*/
struct PSI_cond_service_v1
{
  /** @sa register_cond_v1_t. */
  register_cond_v1_t register_cond;
  /** @sa init_cond_v1_t. */
  init_cond_v1_t init_cond;
  /** @sa destroy_cond_v1_t. */
  destroy_cond_v1_t destroy_cond;
  /** @sa signal_cond_v1_t. */
  signal_cond_v1_t signal_cond;
  /** @sa broadcast_cond_v1_t. */
  broadcast_cond_v1_t broadcast_cond;
  /** @sa start_cond_wait_v1_t. */
  start_cond_wait_v1_t start_cond_wait;
  /** @sa end_cond_wait_v1_t. */
  end_cond_wait_v1_t end_cond_wait;
};

typedef struct PSI_cond_service_v1 PSI_cond_service_t;

extern MYSQL_PLUGIN_IMPORT PSI_cond_service_t *psi_cond_service;

#endif /* HAVE_PSI_COND_INTERFACE */

/** @} (end of group psi_abi_cond) */

C_MODE_END

#endif /* MYSQL_PSI_MUTEX_H */
