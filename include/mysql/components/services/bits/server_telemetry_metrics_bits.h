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

#ifndef SERVER_TELEMETRY_METRICS_BITS_H
#define SERVER_TELEMETRY_METRICS_BITS_H

#include <cstddef>  // size_t
#include <cstdint>  // int64_t

#include <mysql/components/service.h>
#include <mysql/components/services/mysql_string.h>
#include "psi_metric_bits.h"

DEFINE_SERVICE_HANDLE(telemetry_meters_iterator);
DEFINE_SERVICE_HANDLE(telemetry_metrics_iterator);

/**
  Initialize Telemetry Metric iterator object
  to enumerate metrics and read measurement values, pointing to 1st matching
  metric

  @param meter define meter (metric group) scope
  @param [out] iterator iterator object

  @returns Result of iterator creation
    @retval false Success
    @retval true Failure
*/
typedef bool (*metrics_iterator_create_t)(const char *meter,
                                          telemetry_metrics_iterator *iterator);

/**
  Uninitialize Telemetry Metric iterator

  @param iterator iterator object

  @returns Result of iterator creation
    @retval false Success
    @retval true Failure
*/
typedef bool (*metrics_iterator_destroy_t)(telemetry_metrics_iterator iterator);

/**
  Advance Telemetry Metric iterator to next element.

  @param iterator iterator object

  @returns Result of iterator creation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*metrics_iterator_advance_t)(telemetry_metrics_iterator iterator);

/**
  Return group name for the element pointed by
  Telemetry Metric iterator.

  @param iterator iterator object
  @param[out] out_group_handle pointer to receive string value

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*metrics_iterator_get_group_t)(
    telemetry_metrics_iterator iterator, my_h_string *out_group_handle);

/**
  Return metric name for the element pointed by
  Telemetry Metric iterator.

  @param iterator iterator object
  @param[out] out_name_handle pointer to receive string value

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*metrics_iterator_get_name_t)(telemetry_metrics_iterator iterator,
                                            my_h_string *out_name_handle);

/**
  Return metric name for the element pointed by
  Telemetry Metric iterator.

  @param iterator iterator object
  @param delivery callback to deliver measurements
  @param delivery_context context pointer, passed back to callback

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*metrics_iterator_get_value_t)(
    telemetry_metrics_iterator iterator,
    measurement_delivery_callback_t delivery, void *delivery_context);

/**
  Return metric description for the element pointed by
  Telemetry Metric iterator.

  @param iterator iterator object
  @param[out] out_desc_handle pointer to receive string value

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*metrics_iterator_get_description_t)(
    telemetry_metrics_iterator iterator, my_h_string *out_desc_handle);

/**
  Return metric unit for the element pointed by
  Telemetry Metric iterator.

  @param iterator iterator object
  @param[out] out_unit_handle pointer to receive string value

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*metrics_iterator_get_unit_t)(telemetry_metrics_iterator iterator,
                                            my_h_string *out_unit_handle);

/**
  Return metric numeric type for the element pointed by
  Telemetry Metric iterator.

  @param iterator iterator object
  @param[out] numeric reference to numeric type output

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*metrics_iterator_get_numeric_type_t)(
    telemetry_metrics_iterator iterator, MetricNumType &numeric);

/**
  Return metric OTEL type for the element pointed by
  Telemetry Metric iterator.

  @param iterator iterator object
  @param[out] metric_type reference to metric type output

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*metrics_iterator_get_metric_type_t)(
    telemetry_metrics_iterator iterator, MetricOTELType &metric_type);

/**
  Return metric measurement callback function for the element pointed by
  Telemetry Metric iterator.

  @param iterator iterator object
  @param[out] callback reference to metric callback
  @param[out] measurement_context reference to metric context pointer

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*metrics_iterator_get_callback_t)(
    telemetry_metrics_iterator iterator, measurement_callback_t &callback,
    void *&measurement_context);

/**
  Initialize Telemetry Meter (Metric Group) iterator object
  to enumerate metrics groups, pointing to 1st matching
  metric

  @param [out] iterator iterator object

  @returns Result of iterator creation
    @retval false Success
    @retval true Failure
*/
typedef bool (*meters_iterator_create_t)(telemetry_meters_iterator *iterator);

/**
  Uninitialize Telemetry Meter (Metric Group) iterator

  @param iterator iterator object

  @returns Result of iterator creation
    @retval false Success
    @retval true Failure
*/
typedef bool (*meters_iterator_destroy_t)(telemetry_meters_iterator iterator);

/**
  Advance Telemetry Meter (Metric Group) iterator to next element.

  @param iterator iterator object

  @returns Result of iterator creation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*meters_iterator_advance_t)(telemetry_meters_iterator iterator);

/**
  Return meter name for the element pointed by
  Telemetry Meter (Metric Group) iterator.

  @param iterator iterator object
  @param[out] out_name_handle pointer to receive string value

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*meters_iterator_get_name_t)(telemetry_meters_iterator iterator,
                                           my_h_string *out_name_handle);

/**
  Return meter update frequency for the element pointed by
  Telemetry Meter iterator.

  @param iterator iterator object
  @param[out] value reference to result variable

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*meters_iterator_get_frequency_t)(
    telemetry_meters_iterator iterator, unsigned int &value);

/**
  Return meter enabled status for the element pointed by
  Telemetry Meter iterator.

  @param iterator iterator object
  @param[out] enabled reference to enabled result

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*meters_iterator_get_enabled_t)(
    telemetry_meters_iterator iterator, bool &enabled);

/**
  Return meter description for the element pointed by
  Telemetry Meter iterator.

  @param iterator iterator object
  @param[out] out_desc_handle pointer to receive string value

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*meters_iterator_get_description_t)(
    telemetry_meters_iterator iterator, my_h_string *out_desc_handle);

/**
  Take the lock(s) needed to read system variables.
  For performance reasons, lock is taken once per metric export instead of
  once per each variable read.

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*measurement_reading_start_t)();

/**
  Release the lock(s) needed to read system variables.
  For performance reasons, lock is taken once per metric export instead of
  once per each variable read.

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*measurement_reading_end_t)();

#endif /* SERVER_TELEMETRY_METRICS_BITS_H */
