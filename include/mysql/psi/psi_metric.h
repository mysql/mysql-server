/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_PSI_METRIC_H
#define MYSQL_PSI_METRIC_H

/**
  @file include/mysql/psi/psi_metric.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_metric Metric Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_inttypes.h"
#include "my_macros.h"

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

#include "my_sharedlib.h"
#include "mysql/components/services/bits/psi_metric_bits.h"

/** Entry point for the performance schema interface. */
struct PSI_metric_bootstrap {
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_METRIC_VERSION_1
    @sa PSI_CURRENT_METRIC_VERSION
  */
  void *(*get_interface)(int version);
};

#ifdef HAVE_PSI_METRICS_INTERFACE

/**
  Performance Schema Stage Interface, version 1.
  @since PSI_STAGE_VERSION_1
*/
struct PSI_metric_service_v1 {
  /** @sa register_meters_v1_t. */
  register_meters_v1_t register_meters;
  /** @sa unregister_meters_v1_t. */
  unregister_meters_v1_t unregister_meters;
  /** @sa register_change_notification_v1_t. */
  register_change_notification_v1_t register_change_notification;
  /** @sa unregister_change_notification_v1_t. */
  unregister_change_notification_v1_t unregister_change_notification;
  /** @sa send_change_notification_v1_t. */
  send_change_notification_v1_t send_change_notification;
};

typedef struct PSI_metric_service_v1 PSI_metric_service_t;

extern MYSQL_PLUGIN_IMPORT PSI_metric_service_t *psi_metric_service;

#endif /* HAVE_PSI_METRICS_INTERFACE */

/** @} (end of group psi_abi_metric) */

#endif /* MYSQL_PSI_METRIC_H */
