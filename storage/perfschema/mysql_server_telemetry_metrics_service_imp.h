/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_SERVER_TELEMETRY_METRICS_SERVICE_IMP_H
#define MYSQL_SERVER_TELEMETRY_METRICS_SERVICE_IMP_H

#include <mysql/components/services/mysql_server_telemetry_metrics_service.h>
#include <mysql/components/services/psi_metric_service.h>
#include <mysql/plugin.h>

/**
  @file storage/perfschema/mysql_server_telemetry_metrics_service_imp.h
  The performance schema implementation of server telemetry metrics service.
*/
extern SERVICE_TYPE(mysql_server_telemetry_metrics_v1)
    SERVICE_IMPLEMENTATION(performance_schema,
                           mysql_server_telemetry_metrics_v1);

void initialize_mysql_server_telemetry_metrics_service();
void cleanup_mysql_server_telemetry_metrics_service();

bool imp_meters_iterator_create(telemetry_meters_iterator *out_iterator);
bool imp_meters_iterator_destroy(telemetry_meters_iterator iterator);
bool imp_meters_iterator_next(telemetry_meters_iterator iterator);
bool imp_meters_get_name(telemetry_meters_iterator iterator,
                         my_h_string *out_name_handle);
bool imp_meters_get_frequency(telemetry_meters_iterator iterator,
                              unsigned int &value);
bool imp_meters_get_enabled(telemetry_meters_iterator iterator, bool &enabled);
bool imp_meters_get_description(telemetry_meters_iterator iterator,
                                my_h_string *out_desc_handle);

bool imp_metrics_iterator_create(const char *meter,
                                 telemetry_metrics_iterator *out_iterator);
bool imp_metrics_iterator_destroy(telemetry_metrics_iterator iterator);
bool imp_metrics_iterator_next(telemetry_metrics_iterator iterator);
bool imp_metrics_get_group(telemetry_metrics_iterator iterator,
                           my_h_string *out_group_handle);
bool imp_metrics_get_name(telemetry_metrics_iterator iterator,
                          my_h_string *out_name_handle);
bool imp_metrics_get_description(telemetry_metrics_iterator iterator,
                                 my_h_string *out_desc_handle);
bool imp_metrics_get_unit(telemetry_metrics_iterator iterator,
                          my_h_string *out_unit_handle);
bool imp_metric_get_numeric_type(telemetry_metrics_iterator iterator,
                                 MetricNumType &numeric);
bool imp_metric_get_metric_type(telemetry_metrics_iterator iterator,
                                MetricOTELType &metric_type);
bool imp_metrics_get_value(telemetry_metrics_iterator iterator,
                           measurement_delivery_callback_t delivery,
                           void *delivery_context);
bool imp_metrics_get_callback(telemetry_metrics_iterator iterator,
                              measurement_callback_t &callback,
                              void *&measurement_context);

bool imp_measurement_start();
bool imp_measurement_end();

#endif /* MYSQL_SERVER_TELEMETRY_METRICS_SERVICE_IMP_H */
