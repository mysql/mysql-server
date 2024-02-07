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

#ifndef MYSQL_SERVER_TELEMETRY_METRICS_SERVICE_INCLUDED
#define MYSQL_SERVER_TELEMETRY_METRICS_SERVICE_INCLUDED

#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/bits/psi_metric_bits.h>
#include <mysql/components/services/bits/server_telemetry_metrics_bits.h>

/*
  Version 1.
  Introduced in MySQL 8.2.0
  Status: Active.
*/
BEGIN_SERVICE_DEFINITION(mysql_server_telemetry_metrics_v1)

meters_iterator_create_t meter_iterator_create;
meters_iterator_destroy_t meter_iterator_destroy;
meters_iterator_advance_t meter_iterator_advance;
meters_iterator_get_name_t meter_get_name;
meters_iterator_get_frequency_t meter_get_frequency;
meters_iterator_get_enabled_t meter_get_enabled;
meters_iterator_get_description_t meter_get_description;

metrics_iterator_create_t metric_iterator_create;
metrics_iterator_destroy_t metric_iterator_destroy;
metrics_iterator_advance_t metric_iterator_advance;
metrics_iterator_get_group_t metric_get_group;
metrics_iterator_get_name_t metric_get_name;
metrics_iterator_get_description_t metric_get_description;
metrics_iterator_get_unit_t metric_get_unit;
metrics_iterator_get_numeric_type_t metric_get_numeric_type;
metrics_iterator_get_metric_type_t metric_get_metric_type;
metrics_iterator_get_value_t metric_get_value;
metrics_iterator_get_callback_t metric_get_callback;

measurement_reading_start_t measurement_start;
measurement_reading_end_t measurement_end;

END_SERVICE_DEFINITION(mysql_server_telemetry_metrics_v1)

#endif /* MYSQL_SERVER_TELEMETRY_METRICS_SERVICE_INCLUDED */
