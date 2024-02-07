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

#ifndef PFS_METRIC_PROVIDER_H
#define PFS_METRIC_PROVIDER_H

/**
  @file include/pfs_metric_provider.h
  Performance schema instrumentation (declarations).
*/

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

#ifdef HAVE_PSI_METRICS_INTERFACE
#if defined(MYSQL_SERVER) || defined(PFS_DIRECT_CALL)
#ifndef MYSQL_DYNAMIC_PLUGIN

#include "my_macros.h"
#include "mysql/psi/psi_metric.h"

#define PSI_METRIC_CALL(M) pfs_##M##_v1

void pfs_register_meters_v1(PSI_meter_info_v1 *info, size_t count);
void pfs_unregister_meters_v1(PSI_meter_info_v1 *info, size_t count);
void pfs_register_change_notification_v1(
    meter_registration_changes_v1_t callback);
void pfs_unregister_change_notification_v1(
    meter_registration_changes_v1_t callback);
void pfs_send_change_notification_v1(const char *meter, MeterNotifyType change);

#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVER || PFS_DIRECT_CALL */
#endif /* HAVE_PSI_METRICS_INTERFACE */

#endif
