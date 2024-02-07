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

/**
  @file storage/perfschema/pfs_metrics_service_imp.cc
  The performance schema implementation of server metrics instrument service.
*/

#include "storage/perfschema/pfs_metrics_service_imp.h"
#include <map>
#include <string>
#include "mysql/psi/mysql_rwlock.h"
#include "pfs_column_values.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"

// locking for metrics register/unregister
mysql_rwlock_t LOCK_pfs_metrics;
// lock for meter change notification callback
mysql_rwlock_t LOCK_pfs_meter_notify;

#ifdef HAVE_PSI_METRICS_INTERFACE
static PSI_rwlock_key key_LOCK_pfs_metrics;
static PSI_rwlock_info info_LOCK_pfs_metrics = {
    &key_LOCK_pfs_metrics, "LOCK_pfs_metrics", PSI_FLAG_SINGLETON, 0,
    "This lock protects list of instrumented metrics."};

static PSI_rwlock_key key_LOCK_pfs_meter_notify;
static PSI_rwlock_info info_LOCK_pfs_meter_notify = {
    &key_LOCK_pfs_meter_notify, "LOCK_pfs_meter_notify", PSI_FLAG_SINGLETON, 0,
    "This lock protects meter change notification callback."};

static meter_registration_changes_v1_t g_notify_callback = nullptr;

static bool invalid_metric_name(const char *name, size_t max_len) {
  if (name == nullptr) return true;
  const size_t len = strlen(name);
  if (len > max_len) return true;
  if (len == 0) return true;
  // first char should be alpha
  if (!isalpha(*name)) return true;
  // subsequent chars should be either alphanumerics, digits, underscore, minus
  // or dot
  for (size_t i = 0; i < len; ++i) {
    const char c = name[i];
    if (!isalnum(c) && c != '-' && c != '_' && c != '.') return true;
  }
  return false;
}

static bool invalid_metric_definition(PSI_metric_info_v1 *metric) {
  return invalid_metric_name(metric->m_metric, MAX_METRIC_NAME_LEN) ||
         (metric->m_unit != nullptr &&
          strlen(metric->m_unit) > MAX_METRIC_UNIT_LEN) ||
         (metric->m_description != nullptr &&
          strlen(metric->m_description) > MAX_METRIC_DESCRIPTION_LEN);
}

static bool invalid_meter_definition(PSI_meter_info_v1 *metric) {
  return invalid_metric_name(metric->m_meter, MAX_METER_NAME_LEN) ||
         (metric->m_description != nullptr &&
          strlen(metric->m_description) > MAX_METER_DESCRIPTION_LEN);
}
#endif

/* clang-format off */
/**

  @page PAGE_MYSQL_SERVER_METRICS_INSTRUMENT_SERVICE Server metrics instrument service
  Performance Schema server metrics instrument service is a mechanism which provides
  registration of metric sources within the server or plugin/component.

  @subpage SERVER_METRICS_INSTRUMENT_SERVICE_INTRODUCTION

  @subpage SERVER_METRICS_INSTRUMENT_SERVICE_BLOCK_DIAGRAM

  @subpage SERVER_METRICS_INSTRUMENT_SERVICE_EXAMPLE_PLUGIN_COMPONENT


  @page SERVER_METRICS_INSTRUMENT_SERVICE_INTRODUCTION Service Introduction

  This service is named <i>pfs_metrics_v1</i> and it exposes the following
  methods:\n
  - @c register_meters : register a batch of meters (metric groups), each with its own metric set
  - @c unregister_meters : unregister a batch of meters
  - @c register_change_notification : metric component to register a callback function
   to be notified of the changes related to meter status
  - @c unregister_change_notification : unregister change notification callback
  - @c send_change_notification : helper to trigger exact change notification (if callback registered)

  Metric sources are grouped into metric groups (meters) to follow the Open Telemetry model.

  Register/unregister methods accept an array of PSI_meter_info_v1 structures, each
  describing a single meter using fields:
    - name
    - description
    - meter export frequency (in seconds)
    - meter key (equals to 0 if meter instrument failed to register)
    - an array (with size) of metrics belonging to this meter

  Metrics array consists of PSI_metric_info_v1 structures, describing a metric using fields:
    - name
    - unit (optional, string like "s", "ms", "By")
    - Open Telemetry metric type
    - numeric type (integer or floating point measurement value)
    - metric key (equals to 0 if metric instrument failed to register)
    - measurement callback method (function to be called when requesting metric measurements)
    - measurement callback context pointer (custom data pointer, passed back to measurement callback)

  The following Open Telemetry metric types are supported:
    - Asynchronous Counter (monotonic, sum aggregation)
    - Asynchronous UpDown Counter (non-monotonic, sum aggregation)
    - Asynchronous Gauge Counter (non-monotonic, non-aggregated)

  Metric sources can be registered by:
    - MySQL server (metrics registered on server startup, unregistered on shutdown)
    - plugin or component (metrics registered on install, unregistered on uninstall)

  Meter/metric definition arrays must exist for as long as the matching meters/metrics are being
  registered, the internal code keeps pointers to this registration data.

  Measurement callback method accepts a function pointer parameter (delivery callback) and uses it to
  report one or more measurement values for a given metric (with optional key/value attributes
  attached to each).

  @section SERVER_METRICS_INSTRUMENT_SERVICE_BLOCK_DIAGRAM Block Diagram
  Following diagram shows the block diagram of PFS services functionality,
  exposed via mysql-server component.
  Interface usage here is done from the point of view of an entity
  that exposes metric sources.

@startuml

  actor client as "Plugin/component/server"
  box "Performance Schema Storage Engine" #LightBlue
  participant pfs_service as "pfs_metrics_v1 Service\n(mysql-server component)"
  endbox

  == Initialization ==
  client -> pfs_service :
  note right: Register metric sources \nPFS Service Call \n[register_meters()].

  == Cleanup ==
  client -> pfs_service :
  note right: Unregister metric sources \nPFS Service Call \n[unregister_meters()].

  @enduml

  The next diagram shows the functionality used from the point of view of the component
  that will export the metric measurements, and installs its notification callback
  to track changes in available metrics.

@startuml

  actor client as "Plugin/component/server"
  box "Performance Schema Storage Engine" #LightBlue
  participant pfs_service as "pfs_metrics_v1 Service\n(mysql-server component)"
  endbox

  == Initialization ==
  client -> pfs_service :
  note right: Register notification callback \nPFS Service Call \n[register_change_notification()].

  == Cleanup ==
  client -> pfs_service :
  note right: Unregister notification callback \nPFS Service Call \n[unregister_change_notification()].

  @enduml

  @page SERVER_METRICS_INSTRUMENT_SERVICE_EXAMPLE_PLUGIN_COMPONENT  Example component

  As an example, see "components/test_server_telemetry_metrics" test component source code,
  used to test this service.

*/
/* clang-format on */

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, psi_metric_v1)
pfs_register_meters_v1, pfs_unregister_meters_v1,
    pfs_register_change_notification_v1, pfs_unregister_change_notification_v1,
    pfs_send_change_notification_v1, END_SERVICE_IMPLEMENTATION();

#ifdef HAVE_PSI_METRICS_INTERFACE
bool server_metrics_instrument_service_initialized = false;
#endif /* HAVE_PSI_METRICS_INTERFACE */

void initialize_mysql_server_metrics_instrument_service() {
#ifdef HAVE_PSI_METRICS_INTERFACE
  /* This is called once at startup */
  assert(!server_metrics_instrument_service_initialized);
  server_metrics_instrument_service_initialized = true;

  mysql_rwlock_register("pfs", &info_LOCK_pfs_metrics, 1);
  mysql_rwlock_init(key_LOCK_pfs_metrics, &LOCK_pfs_metrics);

  mysql_rwlock_register("pfs", &info_LOCK_pfs_meter_notify, 1);
  mysql_rwlock_init(key_LOCK_pfs_meter_notify, &LOCK_pfs_meter_notify);
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

void cleanup_mysql_server_metrics_instrument_service() {
#ifdef HAVE_PSI_METRICS_INTERFACE
  if (server_metrics_instrument_service_initialized) {
    server_metrics_instrument_service_initialized = false;
    mysql_rwlock_destroy(&LOCK_pfs_metrics);
    mysql_rwlock_destroy(&LOCK_pfs_meter_notify);
  }
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

// compared to similar code in pfs.cc, we add additional optional string to the
// path (meter name) to ensure two metrics with the same name within different
// meters are distinct
#ifdef HAVE_PSI_METRICS_INTERFACE
constexpr int PFS_MAX_FULL_METRIC_PREFIX_NAME_LENGTH = 80;
static int build_prefix(const LEX_CSTRING *prefix, const char *optional,
                        char *output, size_t *output_length) {
  char *out_ptr = output;
  const size_t prefix_length = prefix->length;
  const size_t optional_len =
      (optional == nullptr) ? 0 : strlen(optional) + 1;  // account for '/'

  if (unlikely((prefix_length + optional_len + 1) >=
               PFS_MAX_FULL_METRIC_PREFIX_NAME_LENGTH)) {
    pfs_print_error("build_prefix: prefix+optional is too long <%s> <%s>\n",
                    prefix->str, optional);
    return 1;
  }

  /* output = prefix + '/' + optional + '/' */
  memcpy(out_ptr, prefix->str, prefix_length);
  out_ptr += prefix_length;
  *out_ptr = '/';
  out_ptr++;
  if (optional_len > 0) {
    memcpy(out_ptr, optional, optional_len - 1);
    out_ptr += optional_len - 1;
    *out_ptr = '/';
    out_ptr++;
  }
  *output_length = int(out_ptr - output);

  return 0;
}
#endif

#ifdef HAVE_PSI_METRICS_INTERFACE
// internal helper
static void pfs_register_metrics_v1(PSI_metric_info_v1 *info, size_t count,
                                    const char *meter) {
  // similar to REGISTER_BODY_V1 with metric validation
  PSI_metric_key key;
  char formatted_name[PFS_MAX_INFO_NAME_LENGTH];
  size_t prefix_length;
  size_t len;
  size_t full_length;

  assert(meter != nullptr);
  assert(info != nullptr);
  if (unlikely(build_prefix(&metric_instrument_prefix, meter, formatted_name,
                            &prefix_length)) ||
      !pfs_initialized) {
    for (; count > 0; count--, info++) info->m_key = 0;
    return;
  }

  for (; count > 0; count--, info++) {
    assert(info->m_key == 0);
    assert(info->m_metric != nullptr);
    len = strlen(info->m_metric);
    full_length = prefix_length + len;

    if (invalid_metric_definition(info)) {
      pfs_print_error(
          "pfs_register_metric_v1: Failed to register metric <%s> <%s> "
          "(invalid definition)\n",
          meter, info->m_metric);
      key = 0;
      if (pfs_enabled) ++metric_class_lost;
    } else if (likely(full_length <= PFS_MAX_INFO_NAME_LENGTH)) {
      memcpy(formatted_name + prefix_length, info->m_metric, len);
      key =
          register_metric_class(formatted_name, (uint)full_length, info, meter);
      if (key == UINT_MAX) {
        // duplicate detected, _lost count was not increased internally
        key = 0;
        if (pfs_enabled) ++metric_class_lost;
        pfs_print_error("pfs_register_metric_v1: duplicate name <%s> <%s>\n",
                        meter, info->m_metric);
      }
    } else {
      pfs_print_error("pfs_register_metric_v1: name too long <%s> <%s>\n",
                      meter, info->m_metric);
      key = 0;
      if (pfs_enabled) ++metric_class_lost;
    }

    info->m_key = key;
  }
}
#endif /* HAVE_PSI_METRICS_INTERFACE */

void pfs_register_meters_v1(PSI_meter_info_v1 *info [[maybe_unused]],
                            size_t count [[maybe_unused]]) {
  // similar to REGISTER_BODY_V1 with locking and metric validation
#ifdef HAVE_PSI_METRICS_INTERFACE
  PSI_meter_key key;
  char formatted_name[PFS_MAX_INFO_NAME_LENGTH];
  size_t prefix_length;
  size_t len;
  size_t full_length;

  assert(info != nullptr);
  if (unlikely(build_prefix(&meter_instrument_prefix, nullptr, formatted_name,
                            &prefix_length)) ||
      !pfs_initialized) {
    for (; count > 0; count--, info++) info->m_key = 0;
    return;
  }

  std::vector<const char *> meters_added;

  mysql_rwlock_wrlock(&LOCK_pfs_metrics);

  for (; count > 0; count--, info++) {
    // register meter
    if (info->m_key > 0) {
      pfs_print_error(
          "pfs_register_meter_v1: Skip registering meter <%s> "
          "(already registered)\n",
          info->m_meter);
      continue;
    }
    assert(info->m_meter != nullptr);
    len = strlen(info->m_meter);
    full_length = prefix_length + len;

    if (invalid_meter_definition(info)) {
      pfs_print_error(
          "pfs_register_meter_v1: Failed to register meter <%s> "
          "(invalid definition)\n",
          info->m_meter);
      key = 0;
      if (pfs_enabled) ++meter_class_lost;
    } else if (likely(full_length <= PFS_MAX_INFO_NAME_LENGTH)) {
      memcpy(formatted_name + prefix_length, info->m_meter, len);
      key = register_meter_class(formatted_name, (uint)full_length, info);
      if (key == UINT_MAX) {
        // duplicate detected, _lost count was not increased internally
        key = 0;
        if (pfs_enabled) ++meter_class_lost;
        pfs_print_error("pfs_register_meter_v1: duplicate name <%s>\n",
                        info->m_meter);
      }
    } else {
      pfs_print_error("pfs_register_meter_v1: name too long <%s>\n",
                      info->m_meter);
      key = 0;
      if (pfs_enabled) ++meter_class_lost;
    }

    info->m_key = key;

    // on success, register its metrics as well
    if (key > 0) {
      pfs_register_metrics_v1(info->m_metrics, info->m_metrics_size,
                              info->m_meter);

      // copy registered keys
      const unsigned int index = key - 1;
      PFS_metric_key *keystore = meter_class_array[index].m_metrics;
      uint &array_size = meter_class_array[index].m_metrics_size;
      for (uint i = 0; i < info->m_metrics_size; i++) {
        if (info->m_metrics[i].m_key > 0) {
          *keystore = info->m_metrics[i].m_key;
          keystore++;
          array_size++;
        }
      }

      meters_added.push_back(info->m_meter);
    }
  }

  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  // notify component
  mysql_rwlock_rdlock(&LOCK_pfs_meter_notify);
  if (g_notify_callback != nullptr) {
    for (auto meter : meters_added) {
      g_notify_callback(meter, MeterNotifyType::METER_ADDED);
    }
  }
  mysql_rwlock_unlock(&LOCK_pfs_meter_notify);
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

#ifdef HAVE_PSI_METRICS_INTERFACE
// internal helper
static void pfs_unregister_metrics_v1(PSI_metric_info_v1 *info, size_t count) {
  assert(info != nullptr);

  for (; count > 0; count--, info++) {
    assert(info->m_metric != nullptr);
    unregister_metric_class(info);
  }
}
#endif /* HAVE_PSI_METRICS_INTERFACE */

void pfs_unregister_meters_v1(PSI_meter_info_v1 *info [[maybe_unused]],
                              size_t count [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  assert(info != nullptr);

  std::vector<const char *> meters_removed;

  mysql_rwlock_wrlock(&LOCK_pfs_metrics);

  for (; count > 0; count--, info++) {
    const bool was_registered = (info->m_key > 0);
    // unregister meter
    unregister_meter_class(info);
    // unregister its metrics
    pfs_unregister_metrics_v1(info->m_metrics, info->m_metrics_size);
    // notify component
    if (was_registered && g_notify_callback != nullptr) {
      meters_removed.push_back(info->m_meter);
    }
  }

  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  // notify component
  mysql_rwlock_rdlock(&LOCK_pfs_meter_notify);
  if (g_notify_callback != nullptr) {
    for (auto meter : meters_removed) {
      g_notify_callback(meter, MeterNotifyType::METER_REMOVED);
    }
  }
  mysql_rwlock_unlock(&LOCK_pfs_meter_notify);
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

void pfs_register_change_notification_v1(
    meter_registration_changes_v1_t callback [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  assert(callback != nullptr);
  mysql_rwlock_wrlock(&LOCK_pfs_meter_notify);
  g_notify_callback = callback;
  mysql_rwlock_unlock(&LOCK_pfs_meter_notify);
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

void pfs_unregister_change_notification_v1(
    meter_registration_changes_v1_t callback [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  assert(callback != nullptr);
  mysql_rwlock_wrlock(&LOCK_pfs_meter_notify);
  if (g_notify_callback == callback) g_notify_callback = nullptr;
  mysql_rwlock_unlock(&LOCK_pfs_meter_notify);
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

void pfs_send_change_notification_v1(const char *meter [[maybe_unused]],
                                     MeterNotifyType change [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  assert(meter != nullptr);
  mysql_rwlock_rdlock(&LOCK_pfs_meter_notify);
  if (g_notify_callback) g_notify_callback(meter, change);
  mysql_rwlock_unlock(&LOCK_pfs_meter_notify);
#endif /* HAVE_PSI_METRICS_INTERFACE */
}
