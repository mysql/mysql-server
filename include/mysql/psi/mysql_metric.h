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

#ifndef MYSQL_METRIC_H
#define MYSQL_METRIC_H

/**
  @file include/mysql/psi/mysql_metric.h
  Instrumentation helpers for metrics.
*/

#include "my_compiler.h"

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

#include "mysql/psi/psi_metric.h"

#include "my_inttypes.h"

#if defined(MYSQL_SERVER) || defined(PFS_DIRECT_CALL)
/* PSI_METRIC_CALL() as direct call. */
#include "pfs_metric_provider.h"  // IWYU pragma: keep
#endif

#ifndef PSI_METRIC_CALL
#define PSI_METRIC_CALL(M) psi_metric_service->M
#endif

/**
  @defgroup psi_api_metric Metric Instrumentation (API)
  @ingroup psi_api
  @{
*/

/**
  @def mysql_meter_register(P1, P2)
  Registration of meters, each with its metrics.
*/
#define mysql_meter_register(P1, P2) inline_mysql_meter_register(P1, P2)

static inline void inline_mysql_meter_register(PSI_meter_info_v1 *info
                                               [[maybe_unused]],
                                               size_t count [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  PSI_METRIC_CALL(register_meters)(info, count);
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

/**
  @def mysql_meter_unregister(P1, P2)
  Meter unregistration.
*/
#define mysql_meter_unregister(P1, P2) inline_mysql_meter_unregister(P1, P2)

static inline void inline_mysql_meter_unregister(PSI_meter_info_v1 *info
                                                 [[maybe_unused]],
                                                 size_t count
                                                 [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  PSI_METRIC_CALL(unregister_meters)(info, count);
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

/**
  @def mysql_meter_notify_register(P1)
  Registration of meter change notification callback.
*/
#define mysql_meter_notify_register(P1) inline_mysql_meter_notify_register(P1)

static inline void inline_mysql_meter_notify_register(
    meter_registration_changes_v1_t callback [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  PSI_METRIC_CALL(register_change_notification)(callback);
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

/**
  @def mysql_meter_notify_unregister(P1)
  Unregistration of meter change notification callback.
*/
#define mysql_meter_notify_unregister(P1) \
  inline_mysql_meter_notify_unregister(P1)

static inline void inline_mysql_meter_notify_unregister(
    meter_registration_changes_v1_t callback [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  PSI_METRIC_CALL(unregister_change_notification)(callback);
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

/**
  @def mysql_meter_notify_send(P1, P2)
  Send meter change notification through registered callback.
*/
#define mysql_meter_notify_send(P1, P2) inline_mysql_meter_notify_send(P1, P2)

static inline void inline_mysql_meter_notify_send(const char *meter
                                                  [[maybe_unused]],
                                                  MeterNotifyType change
                                                  [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  PSI_METRIC_CALL(send_change_notification)(meter, change);
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

/** @} (end of group psi_api_metric) */

#endif
