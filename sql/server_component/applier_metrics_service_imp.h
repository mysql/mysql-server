/* Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef APPLIER_METRICS_SERVICE_IMP_H
#define APPLIER_METRICS_SERVICE_IMP_H

#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/rpl_applier_metrics_service.h>
#include <string>

class Applier_metrics_service_handler {
 public:
  /// @brief Get metrics for the replication applier.
  /// @param[out] table pointer, whose value will be set to an array of arrays
  /// of fields in which the metric values are stored.
  ///  @return
  ///    @retval FALSE Succeeded.
  ///    @retval TRUE  Failed.
  static DEFINE_BOOL_METHOD(get_applier_metrics,
                            (Applier_metrics_table * table));

  /// @brief Free memory for object holding metrics for the replication applier.
  /// @param[out] table Pointer to object that was previously retrieved from
  /// @c get_applier_metrics.
  static DEFINE_METHOD(void, free_applier_metrics,
                       (Applier_metrics_table * table));

  /// @brief Get metrics for replication workers.
  /// @param[out] table pointer, whose value will be set to an array of arrays
  /// of fields in which the metric values are stored.
  ///  @return
  ///    @retval FALSE Succeeded.
  ///    @retval TRUE  Failed.
  static DEFINE_BOOL_METHOD(get_worker_metrics, (Worker_metrics_table * table));

  /// @brief Free memory for object holding metrics for the replication workers.
  /// @param[out] table Pointer to object that was previously retrieved from
  /// @c get_worker_metrics.
  static DEFINE_METHOD(void, free_worker_metrics,
                       (Worker_metrics_table * table));

  /// @brief Enables metric collection in the server for replication applier
  /// components
  ///  @return
  ///    @retval FALSE Succeeded.
  ///    @retval TRUE  Failed.
  static DEFINE_BOOL_METHOD(enable_metric_collection, ());

  /// @brief Enables metric collection in the server for replication applier
  /// components
  ///  @return
  ///    @retval FALSE Succeeded.
  ///    @retval TRUE  Failed.

  static DEFINE_BOOL_METHOD(disable_metric_collection, ());
};

#endif /* APPLIER_METRICS_SERVICE_IMP_H */
