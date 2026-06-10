/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "mysql/components/component_implementation.h"  // REQUIRES_SERVICE_PLACEHOLDER
#include "mysql/components/services/pfs_plugin_table_service.h"  // pfs_plugin_table_v1
#include "mysql/components/services/rpl_applier_metrics_service.h"  // replication_applier_metrics
#include "pfs_tables_handler.h"  // register_pfs_tables

/* Placeholders */

REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_table_v1);
REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_string_v2);
REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_bigint_v1);
REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_timestamp_v2);
REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_enum_v1);
REQUIRES_SERVICE_PLACEHOLDER(replication_applier_metrics);

namespace applier_metrics {

///  Component's init function
///
///  Any failure will result into deregistering already registered parts
///
///  @returns status of initialization
///    @retval 1 Error
///    @retval 0 Success
static mysql_service_status_t applier_metrics_init() try {
  SERVICE_TYPE(replication_applier_metrics) *applier_metrics =
      SERVICE_PLACEHOLDER(replication_applier_metrics);
  if (applier_metrics->enable_metric_collection()) return 1;
  if (register_pfs_tables()) return 1;
  return 0;
} catch (...) {
  return 1;
}

///  Component's deinit functions
///
///  Deregisters everything that was registered by init.
///
///  @returns status
///    @retval 1 Error
///    @retval 0 Success
static mysql_service_status_t applier_metrics_deinit() try {
  if (unregister_pfs_tables()) return 1;
  SERVICE_TYPE(replication_applier_metrics) *applier_metrics =
      SERVICE_PLACEHOLDER(replication_applier_metrics);
  if (applier_metrics->disable_metric_collection()) return 1;

  return 0;
} catch (...) {
  return 1;
}

}  // namespace applier_metrics

/** ================ Component declaration related stuff ================ */

BEGIN_COMPONENT_PROVIDES(replication_applier_metrics)
END_COMPONENT_PROVIDES();

/// List of dependencies
BEGIN_COMPONENT_REQUIRES(replication_applier_metrics)
REQUIRES_SERVICE(pfs_plugin_table_v1),
    REQUIRES_SERVICE(pfs_plugin_column_string_v2),
    REQUIRES_SERVICE(pfs_plugin_column_bigint_v1),
    REQUIRES_SERVICE(pfs_plugin_column_timestamp_v2),
    REQUIRES_SERVICE(pfs_plugin_column_enum_v1),
    REQUIRES_SERVICE(replication_applier_metrics), END_COMPONENT_REQUIRES();

/// Component description
BEGIN_COMPONENT_METADATA(replication_applier_metrics)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), END_COMPONENT_METADATA();

/// Component declaration
DECLARE_COMPONENT(replication_applier_metrics,
                  "mysql::replication_applier_metrics")
applier_metrics::applier_metrics_init,
    applier_metrics::applier_metrics_deinit END_DECLARE_COMPONENT();

/// Component contained in this library
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(replication_applier_metrics)
    END_DECLARE_LIBRARY_COMPONENTS
