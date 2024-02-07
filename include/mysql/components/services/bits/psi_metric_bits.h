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

#ifndef COMPONENTS_SERVICES_BITS_PSI_METRIC_BITS_H
#define COMPONENTS_SERVICES_BITS_PSI_METRIC_BITS_H

#include <cstddef>  // size_t
#include <cstdint>  // int64_t

#include <mysql/components/service.h>

enum MetricOTELType {
  ASYNC_COUNTER,         // monotonic, sum aggregation
  ASYNC_UPDOWN_COUNTER,  // non-monotonic, sum aggregation
  ASYNC_GAUGE_COUNTER    // non-monotonic, non-aggregated
};

enum MetricNumType { METRIC_INTEGER, METRIC_DOUBLE };

enum MeterNotifyType { METER_ADDED, METER_REMOVED, METER_UPDATE };

// metric source field limits imposed by OTEL
constexpr size_t MAX_METER_NAME_LEN = 63;
constexpr size_t MAX_METER_DESCRIPTION_LEN = 1023;
constexpr size_t MAX_METRIC_NAME_LEN = 63;
constexpr size_t MAX_METRIC_UNIT_LEN = 63;
constexpr size_t MAX_METRIC_DESCRIPTION_LEN = 1023;

// common metric units as specified by OpenTelemetry
#define METRIC_UNIT_BYTES "By"
#define METRIC_UNIT_MILLISECONDS "ms"
#define METRIC_UNIT_SECONDS "s"

typedef void (*measurement_delivery_int64_0_callback_t)(void *delivery_context,
                                                        int64_t value);

typedef void (*measurement_delivery_int64_1_callback_t)(void *delivery_context,
                                                        int64_t value,
                                                        const char *attr_name,
                                                        const char *attr_value);

typedef void (*measurement_delivery_int64_n_callback_t)(
    void *delivery_context, int64_t value, const char **attr_name_array,
    const char **attr_value_array, size_t size);

typedef void (*measurement_delivery_double_0_callback_t)(void *delivery_context,
                                                         double value);

typedef void (*measurement_delivery_double_1_callback_t)(
    void *delivery_context, double value, const char *attr_name,
    const char *attr_value);

typedef void (*measurement_delivery_double_n_callback_t)(
    void *delivery_context, double value, const char **attr_name_array,
    const char **attr_value_array, size_t size);

struct measurement_delivery_callback {
  measurement_delivery_int64_0_callback_t value_int64;
  measurement_delivery_int64_1_callback_t value_int64_attr;
  measurement_delivery_int64_n_callback_t value_int64_attrs;
  measurement_delivery_double_0_callback_t value_double;
  measurement_delivery_double_1_callback_t value_double_attr;
  measurement_delivery_double_n_callback_t value_double_attrs;
};

/**
  Set of callbacks within component code to receive the measured values (avoids
  data copy overhead).
*/
typedef measurement_delivery_callback(*measurement_delivery_callback_t);

/**
  Single metric measurement callback can return multiple measurement values.
  For example, "CPU use" metric might return values for all available CPU
  logical cores, each value having an attribute with matching CPU core ID.
*/
typedef void (*measurement_callback_t)(void *measurement_context,
                                       measurement_delivery_callback_t delivery,
                                       void *delivery_context);

typedef unsigned int PSI_meter_key;
typedef unsigned int PSI_metric_key;

/**
  @def PSI_METRIC_VERSION_1
  Performance Schema Metric Interface number for version 1.
  This version is supported.
*/
#define PSI_METRIC_VERSION_1 1

/**
  @def PSI_CURRENT_METRIC_VERSION
  Performance Schema Metric Interface number for the most recent version.
  The most current version is @c PSI_METRIC_VERSION_1
*/
#define PSI_CURRENT_METRIC_VERSION 1

/**
 Define a metric source, storing char pointers requires the original
 strings to be valid for entire lifetime of a metric (global variable), or the
 strings themselves to be string literals (hardcoded), the advantage is no
 (de)allocation code is needed here.
*/
struct PSI_metric_info_v1 {
  const char *m_metric;
  const char *m_unit;
  const char *m_description;
  MetricOTELType m_metric_type;
  MetricNumType m_num_type;
  /** Instrument flags. */
  unsigned int m_flags;
  PSI_metric_key m_key;
  measurement_callback_t m_measurement_callback;
  void *m_measurement_context;
};

/**
 * Define a meter source, storing char pointers requires the original
 * strings to be valid for entire lifetime of a metric (global variable), or
 * the strings themselves to be string literals (hardcoded), the advantage is no
 * (de)allocation code is needed here.
 */
struct PSI_meter_info_v1 {
  const char *m_meter;
  const char *m_description;
  unsigned int m_frequency;
  /** Instrument flags. */
  unsigned int m_flags;
  PSI_meter_key m_key;

  // the metrics for this meter
  PSI_metric_info_v1 *m_metrics;
  unsigned int m_metrics_size;
};

/**
  Register a batch of telemetry meters (metric groups), each with its metrics.

  @param info pointer to an array of meter definitions
  @param count array size
 */
typedef void (*register_meters_v1_t)(PSI_meter_info_v1 *info, size_t count);

/**
  Unregister a batch of meters and their telemetry metric sources.

  @param info pointer to array of meter definitions
  @param count array size
*/
typedef void (*unregister_meters_v1_t)(PSI_meter_info_v1 *info, size_t count);

/**
  Callback function to notify of changes within the set of registered meters.

  @param meter meter name
  @param change type of change related to the meter
*/
typedef void (*meter_registration_changes_v1_t)(const char *meter,
                                                MeterNotifyType change);

/**
  Register a notification callback to track changes in the set of registered
  meters.

  @param callback pointer to notification function
*/
typedef void (*register_change_notification_v1_t)(
    meter_registration_changes_v1_t callback);

/**
  Unregister a notification callback to track changes in the set of registered
  meters.

  @param callback pointer to notification function
*/
typedef void (*unregister_change_notification_v1_t)(
    meter_registration_changes_v1_t callback);

/**
  Send a notification of changes in the set of registered meters.

  @param meter meter name being changed
  @param change change type description
*/
typedef void (*send_change_notification_v1_t)(const char *meter,
                                              MeterNotifyType change);

typedef struct PSI_metric_info_v1 PSI_metric_info_v1;
typedef PSI_metric_info_v1 PSI_metric_info;

typedef struct PSI_meter_info_v1 PSI_meter_info_v1;
typedef PSI_meter_info_v1 PSI_meter_info;

#endif /* COMPONENTS_SERVICES_BITS_PSI_METRIC_BITS_H */
