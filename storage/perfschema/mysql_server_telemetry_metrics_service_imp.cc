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
  @file storage/perfschema/mysql_server_telemetry_metrics_service_imp.cc
  The performance schema implementation of server telemetry metrics service.
*/

#include "storage/perfschema/mysql_server_telemetry_metrics_service_imp.h"

#include <mysql/components/services/mysql_server_telemetry_metrics_service.h>
#include <sql/sql_class.h>
#include <map>
#include <string>
#include "pfs_global.h"
#include "pfs_instr_class.h"

/* clang-format off */
/**

  @page PAGE_MYSQL_SERVER_TELEMETRY_METRICS_SERVICE Server telemetry metrics service
  Performance Schema server telemetry metrics service provides a way for
  plugins/components to query telemetry meters (metric groups), metrics and
  metric measurements in order to periodically export these measurements
  using Open Telemetry protocol.

  @subpage SERVER_METRICS_TELEMETRY_SERVICE_INTRODUCTION

  @subpage SERVER_METRICS_TELEMETRY_SERVICE_INTERFACE

  @subpage SERVER_METRICS_TELEMETRY_SERVICE_EXAMPLE_PLUGIN_COMPONENT


  @page SERVER_METRICS_TELEMETRY_SERVICE_INTRODUCTION Service Introduction

  This service is named <i>mysql_server_telemetry_metrics_v1</i> and it exposes the
  set of methods to:\n
  - discover (iterate) registered meters (metric groups)
  - discover (iterate) metrics exposed within some meter
  - get measurement values for a given metric
  - mark start/end of metric export process

  Service interface supports dynamic meter/metric data discoverability.
  The interface does not provide configurability, all methods only provide read-only
  data access.

  As an alternative to this interface, the same data is being exported in the following tables
  within the performance_schema database:
   - setup_meters
   - setup_metrics

  These  tables allow for configurability, so to configure the telemetry metrics export, i.e. define:
  - what meters should be exported (meter ENABLED state)
  - how frequently should each meter be exported (meter FREQUENCY state)

  DB admin should modify (via SQL) these fields within the
  performance_schema.setup_meters table.

  @page SERVER_METRICS_TELEMETRY_SERVICE_INTERFACE Service Interface

  Service exposes the following methods to discover registered meters using an meter iterator:
  - @c meter_iterator_create : create meter iterator (on success points to 1st meter)
  - @c meter_iterator_destroy : destroy meter iterator
  - @c meter_iterator_advance : advance meter iterator to point to next meter (if exists)
  - @c meter_get_name : get name of the meter, given an iterator pointing to it
  - @c meter_get_frequency : get export frequency of the meter (in seconds), given an iterator
  - @c meter_get_enabled : get enabled status of the meter, given an iterator
  - @c meter_get_description : get meter description, given an iterator

  Another set of methods is used to discover (or get measurements of) metrics within a given meter
  using a metric iterator:
  - @c metric_iterator_create : create metric iterator (on success points to 1st metric of a given meter)
  - @c metric_iterator_destroy : destroy metric iterator
  - @c metric_iterator_advance : advance metric iterator to point to next meter (if exists)
  - @c metric_get_group : get meter name this metric belongs to, given an iterator
  - @c metric_get_name : get metric name, given an iterator
  - @c metric_get_description : get metric description, given an iterator
  - @c metric_get_unit : get metric unit, given an iterator
  - @c metric_get_numeric_type : get metric measurement numeric type (integer or float), given an iterator
  - @c metric_get_metric_type : get metric Open Telemetry type, given an iterator
  - @c metric_get_value : get metric measurement values (with optional key/value attributes attached to each value)
  - @c metrics_get_callback : get metric measurement callback function pointer
  - @c metrics_get_callback_context : get metric measurement context pointer

  The last set of methods is used on telemetry metrics measurement export to optimize internal locking:
  - @c measurement_start : call this before the export
  - @c measurement_end : call this after the export

  @page SERVER_METRICS_TELEMETRY_SERVICE_EXAMPLE_PLUGIN_COMPONENT  Example component

  As an example, see "components/test_server_telemetry_metrics" test component source code,
  used to test this service.

*/
/* clang-format on */

extern mysql_rwlock_t LOCK_pfs_metrics;
extern mysql_mutex_t LOCK_status;

class meter_iterator {
 private:
  // current position
  uint m_meter_idx{0};

 public:
  /**
    Create meter iterator.
    Sets the iterator to the first matching element (if any) or at eof.

    @param start start position within the meter array
    @retval false : found
    @retval true : not found or error initializing
  */
  bool init(uint start = 0) {
    if (meter_class_max < 1) return true;
    if (start >= meter_class_max) return true;
    m_meter_idx = start;
    while (m_meter_idx < meter_class_max) {
      if (meter_class_array[m_meter_idx].m_key > 0) return false;
      m_meter_idx++;
    }
    return true;
  }

  /**
    Advance meter iterator to next value.
    Sets the iterator to the next matching element (if any) or at eof.

    @retval false : found
    @retval true : not found
  */
  bool next() {
    if (meter_class_max < 1) return true;
    if (m_meter_idx >= meter_class_max - 1) return true;
    return init(m_meter_idx + 1);
  }

  PFS_meter_class *get_current() const {
    if (meter_class_max < 1) return nullptr;
    if (m_meter_idx >= meter_class_max) return nullptr;
    PFS_meter_class *entry = &(meter_class_array[m_meter_idx]);
    return entry;
  }
};

class metric_iterator {
 private:
  uint m_meter_idx{0};
  uint m_metric_idx{0};

 protected:
  bool next_metric(uint start) {
    // find next metric within selected meter
    if (metric_class_max < 1) return true;
    m_metric_idx = start;
    const PFS_meter_class *meter = &(meter_class_array[m_meter_idx]);
    PFS_metric_key key = 0;
    while (m_metric_idx < meter->m_metrics_size) {
      key = meter->m_metrics[m_metric_idx];
      if (key > 0) {
        break;
      }
      m_metric_idx++;
    }
    return (key == 0);
  }

  bool find_meter(const char *group) {
    // find meter by name
    assert(group != nullptr && *group != '\0');
    if (group == nullptr || *group == '\0') return true;
    if (meter_class_max < 1) return true;
    m_meter_idx = 0;
    bool found = false;
    while (m_meter_idx < meter_class_max) {
      if (meter_class_array[m_meter_idx].m_key > 0 &&
          0 == strcmp(meter_class_array[m_meter_idx].m_meter, group)) {
        found = true;
        break;
      }
      m_meter_idx++;
    }
    return (!found);
  }

 public:
  /**
    Create metric sources iterator, iterates metrics within single meter.
    Sets the iterator to the first matching element (if any) or at eof.

    @param group meter containing the metrics
    @retval false : found
    @retval true : not found or error initializing
  */
  bool init(const char *group) {
    // find first metric within this meter
    return find_meter(group) || next_metric(0);
  }

  /**
    Advance metric sources iterator to next value.
    Sets the iterator to the next matching element (if any) or at eof.

    @retval false : found
    @retval true : not found
  */
  bool next() { return next_metric(m_metric_idx + 1); }

  PFS_metric_class *get_current() const {
    if (metric_class_max < 1) return nullptr;

    assert(m_metric_idx < metric_class_max);
    if (m_metric_idx >= metric_class_max) return nullptr;

    const PFS_meter_class *meter = &(meter_class_array[m_meter_idx]);
    assert(m_metric_idx < meter->m_metrics_size);
    if (m_metric_idx >= meter->m_metrics_size) return nullptr;

    const PFS_metric_key key = meter->m_metrics[m_metric_idx];
    assert(key > 0);

    const unsigned int index = key - 1;
    PFS_metric_class *entry = &(metric_class_array[index]);
    return entry;
  }
};

BEGIN_SERVICE_IMPLEMENTATION(performance_schema,
                             mysql_server_telemetry_metrics_v1)
imp_meters_iterator_create, imp_meters_iterator_destroy,
    imp_meters_iterator_next, imp_meters_get_name, imp_meters_get_frequency,
    imp_meters_get_enabled, imp_meters_get_description,
    imp_metrics_iterator_create, imp_metrics_iterator_destroy,
    imp_metrics_iterator_next, imp_metrics_get_group, imp_metrics_get_name,
    imp_metrics_get_description, imp_metrics_get_unit,
    imp_metric_get_numeric_type, imp_metric_get_metric_type,
    imp_metrics_get_value, imp_metrics_get_callback, imp_measurement_start,
    imp_measurement_end, END_SERVICE_IMPLEMENTATION();

#ifdef HAVE_PSI_METRICS_INTERFACE
bool server_telemetry_metrics_service_initialized = false;
#endif /* HAVE_PSI_METRICS_INTERFACE */

void initialize_mysql_server_telemetry_metrics_service() {
#ifdef HAVE_PSI_METRICS_INTERFACE
  assert(!server_telemetry_metrics_service_initialized);
  server_telemetry_metrics_service_initialized = true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

void cleanup_mysql_server_telemetry_metrics_service() {
#ifdef HAVE_PSI_METRICS_INTERFACE
  server_telemetry_metrics_service_initialized = false;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_meters_iterator_create(telemetry_meters_iterator *out_iterator
                                [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  mysql_rwlock_rdlock(&LOCK_pfs_metrics);
  std::unique_ptr<meter_iterator> iter(new meter_iterator());
  if (iter->init()) {
    mysql_rwlock_unlock(&LOCK_pfs_metrics);
    return true;
  }
  mysql_rwlock_unlock(&LOCK_pfs_metrics);
  *out_iterator = reinterpret_cast<telemetry_meters_iterator>(iter.release());
  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_meters_iterator_destroy(telemetry_meters_iterator iterator
                                 [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  const auto *iter_ptr = reinterpret_cast<meter_iterator *>(iterator);
  assert(iter_ptr != nullptr);
  delete iter_ptr;
  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_meters_iterator_next(telemetry_meters_iterator iterator
                              [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  auto *iter_ptr = reinterpret_cast<meter_iterator *>(iterator);
  assert(iter_ptr != nullptr);

  mysql_rwlock_rdlock(&LOCK_pfs_metrics);
  const bool res = iter_ptr->next();
  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  return res;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_meters_get_name(telemetry_meters_iterator iterator [[maybe_unused]],
                         my_h_string *out_name_handle [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  const auto *iter_ptr = reinterpret_cast<meter_iterator *>(iterator);
  assert(iter_ptr != nullptr);

  mysql_rwlock_rdlock(&LOCK_pfs_metrics);

  const PFS_meter_class *meter = iter_ptr->get_current();
  assert(meter != nullptr);

  if (meter == nullptr) {
    mysql_rwlock_unlock(&LOCK_pfs_metrics);
    return true;
  }
  auto *val = new String[1];
  val->set(meter->m_meter, meter->m_meter_length, &my_charset_bin);
  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  *out_name_handle = reinterpret_cast<my_h_string>(val);
  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_meters_get_frequency(telemetry_meters_iterator iterator
                              [[maybe_unused]],
                              unsigned int &value [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  const auto *iter_ptr = reinterpret_cast<meter_iterator *>(iterator);
  assert(iter_ptr != nullptr);

  mysql_rwlock_rdlock(&LOCK_pfs_metrics);

  const PFS_meter_class *meter = iter_ptr->get_current();
  assert(meter != nullptr);

  value = meter->m_frequency;
  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_meters_get_enabled(telemetry_meters_iterator iterator [[maybe_unused]],
                            bool &enabled [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  const auto *iter_ptr = reinterpret_cast<meter_iterator *>(iterator);
  assert(iter_ptr != nullptr);

  mysql_rwlock_rdlock(&LOCK_pfs_metrics);

  const PFS_meter_class *meter = iter_ptr->get_current();
  assert(meter != nullptr);

  enabled = meter->m_enabled;
  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_meters_get_description(telemetry_meters_iterator iterator
                                [[maybe_unused]],
                                my_h_string *out_desc_handle [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  const auto *iter_ptr = reinterpret_cast<meter_iterator *>(iterator);
  assert(iter_ptr != nullptr);

  mysql_rwlock_rdlock(&LOCK_pfs_metrics);

  const PFS_meter_class *meter = iter_ptr->get_current();
  assert(meter != nullptr);

  if (meter == nullptr) {
    mysql_rwlock_unlock(&LOCK_pfs_metrics);
    return true;
  }
  auto *val = new String[1];
  val->set(meter->m_description, meter->m_description_length, &my_charset_bin);
  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  *out_desc_handle = reinterpret_cast<my_h_string>(val);
  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_metrics_iterator_create(const char *meter [[maybe_unused]],
                                 telemetry_metrics_iterator *out_iterator
                                 [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  mysql_rwlock_rdlock(&LOCK_pfs_metrics);
  std::unique_ptr<metric_iterator> iter(new metric_iterator());
  // if group name defined, restrict iterating within that group only
  if (iter->init(meter)) {
    mysql_rwlock_unlock(&LOCK_pfs_metrics);
    return true;
  }
  mysql_rwlock_unlock(&LOCK_pfs_metrics);
  *out_iterator = reinterpret_cast<telemetry_metrics_iterator>(iter.release());
  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_metrics_iterator_destroy(telemetry_metrics_iterator iterator
                                  [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  const auto *iter_ptr = reinterpret_cast<metric_iterator *>(iterator);
  assert(iter_ptr != nullptr);
  delete iter_ptr;
  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_metrics_iterator_next(telemetry_metrics_iterator iterator
                               [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  auto *iter_ptr = reinterpret_cast<metric_iterator *>(iterator);
  assert(iter_ptr != nullptr);

  mysql_rwlock_rdlock(&LOCK_pfs_metrics);
  const bool res = iter_ptr->next();
  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  return res;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_metrics_get_group(telemetry_metrics_iterator iterator [[maybe_unused]],
                           my_h_string *out_group_handle [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  const auto *iter_ptr = reinterpret_cast<metric_iterator *>(iterator);
  assert(iter_ptr != nullptr);

  mysql_rwlock_rdlock(&LOCK_pfs_metrics);

  const PFS_metric_class *metric = iter_ptr->get_current();
  assert(metric != nullptr);
  assert(metric->m_group != nullptr);

  if (metric->m_group == nullptr) {
    mysql_rwlock_unlock(&LOCK_pfs_metrics);
    return true;
  }
  auto *val = new String[1];
  val->set(metric->m_group, metric->m_group_length, &my_charset_bin);
  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  *out_group_handle = reinterpret_cast<my_h_string>(val);
  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_metrics_get_name(telemetry_metrics_iterator iterator [[maybe_unused]],
                          my_h_string *out_name_handle [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  const auto *iter_ptr = reinterpret_cast<metric_iterator *>(iterator);
  assert(iter_ptr != nullptr);

  mysql_rwlock_rdlock(&LOCK_pfs_metrics);

  const PFS_metric_class *metric = iter_ptr->get_current();
  assert(metric != nullptr);
  assert(metric->m_metric != nullptr);

  if (metric->m_metric == nullptr) {
    mysql_rwlock_unlock(&LOCK_pfs_metrics);
    return true;
  }

  auto *val = new String[1];
  val->set(metric->m_metric, metric->m_metric_length, &my_charset_bin);
  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  *out_name_handle = reinterpret_cast<my_h_string>(val);
  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_metrics_get_description(telemetry_metrics_iterator iterator
                                 [[maybe_unused]],
                                 my_h_string *out_desc_handle
                                 [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  const auto *iter_ptr = reinterpret_cast<metric_iterator *>(iterator);
  assert(iter_ptr != nullptr);

  mysql_rwlock_rdlock(&LOCK_pfs_metrics);

  const PFS_metric_class *metric = iter_ptr->get_current();
  assert(metric != nullptr);
  assert(metric->m_metric != nullptr);

  if (metric->m_description == nullptr) {
    mysql_rwlock_unlock(&LOCK_pfs_metrics);
    return true;
  }
  auto *val = new String[1];
  val->set(metric->m_description, metric->m_description_length,
           &my_charset_bin);
  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  *out_desc_handle = reinterpret_cast<my_h_string>(val);
  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_metrics_get_unit(telemetry_metrics_iterator iterator [[maybe_unused]],
                          my_h_string *out_unit_handle [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  const auto *iter_ptr = reinterpret_cast<metric_iterator *>(iterator);
  assert(iter_ptr != nullptr);

  mysql_rwlock_rdlock(&LOCK_pfs_metrics);
  const PFS_metric_class *metric = iter_ptr->get_current();
  assert(metric != nullptr);
  assert(metric->m_metric != nullptr);

  if (metric->m_unit == nullptr) {
    mysql_rwlock_unlock(&LOCK_pfs_metrics);
    return true;
  }
  auto *val = new String[1];
  val->set(metric->m_unit, metric->m_unit_length, &my_charset_bin);
  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  *out_unit_handle = reinterpret_cast<my_h_string>(val);
  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_metric_get_numeric_type(telemetry_metrics_iterator iterator
                                 [[maybe_unused]],
                                 MetricNumType &numeric [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  const auto *iter_ptr = reinterpret_cast<metric_iterator *>(iterator);
  assert(iter_ptr != nullptr);

  mysql_rwlock_rdlock(&LOCK_pfs_metrics);

  const PFS_metric_class *metric = iter_ptr->get_current();
  assert(metric != nullptr);

  numeric = metric->m_num_type;
  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_metric_get_metric_type(telemetry_metrics_iterator iterator
                                [[maybe_unused]],
                                MetricOTELType &metric_type [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  const auto *iter_ptr = reinterpret_cast<metric_iterator *>(iterator);
  assert(iter_ptr != nullptr);

  mysql_rwlock_rdlock(&LOCK_pfs_metrics);

  const PFS_metric_class *metric = iter_ptr->get_current();
  assert(metric != nullptr);

  metric_type = metric->m_metric_type;
  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_metrics_get_value(telemetry_metrics_iterator iterator [[maybe_unused]],
                           measurement_delivery_callback_t delivery
                           [[maybe_unused]],
                           void *delivery_context [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  const auto *iter_ptr = reinterpret_cast<metric_iterator *>(iterator);
  assert(iter_ptr != nullptr);
  // status variable values should be read within the lock, see
  // imp_measurement_start/end
  mysql_mutex_assert_owner(&LOCK_status);

  mysql_rwlock_rdlock(&LOCK_pfs_metrics);

  const PFS_metric_class *metric = iter_ptr->get_current();
  assert(metric != nullptr);

  metric->m_measurement_callback(metric->m_measurement_context, delivery,
                                 delivery_context);
  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_metrics_get_callback(telemetry_metrics_iterator iterator
                              [[maybe_unused]],
                              measurement_callback_t &callback [[maybe_unused]],
                              void *&measurement_context [[maybe_unused]]) {
#ifdef HAVE_PSI_METRICS_INTERFACE
  const auto *iter_ptr = reinterpret_cast<metric_iterator *>(iterator);
  assert(iter_ptr != nullptr);

  mysql_rwlock_rdlock(&LOCK_pfs_metrics);

  const PFS_metric_class *metric = iter_ptr->get_current();
  assert(metric != nullptr);

  callback = metric->m_measurement_callback;
  measurement_context = metric->m_measurement_context;
  mysql_rwlock_unlock(&LOCK_pfs_metrics);

  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_measurement_start() {
#ifdef HAVE_PSI_METRICS_INTERFACE
  mysql_mutex_lock(&LOCK_status);
  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}

bool imp_measurement_end() {
#ifdef HAVE_PSI_METRICS_INTERFACE
  mysql_mutex_unlock(&LOCK_status);
  return false;
#else
  return true;
#endif /* HAVE_PSI_METRICS_INTERFACE */
}
