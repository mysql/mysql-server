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

#include "server_metrics_helpers.h"
#include <algorithm>  // std::sort
#include <cstring>    // strcmp
#include <string>
#include <vector>
#include "required_services.h"

struct meter_info {
  std::string name;
  std::string desc;
  bool enabled;
  uint frequency;
};

struct measurement_info {
  int64_t value;
  std::vector<std::pair<std::string, std::string>> attrs;
};

struct metric_info {
  std::string name;
  std::string desc;
  std::string unit;
  MetricNumType numeric;
  std::vector<measurement_info> data;
};

static metric_info g_info;

void delivery_int64_0(void *delivery_context [[maybe_unused]], int64_t value) {
  // new measurement row
  g_info.data.push_back({value, {}});
}

void delivery_int64_1(void *delivery_context [[maybe_unused]], int64_t value,
                      const char *attr_name, const char *attr_value) {
  // new measurement row
  g_info.data.push_back({value, {}});
  // new attribute row
  assert(attr_name != nullptr);
  assert(attr_value != nullptr);
  g_info.data.back().attrs.emplace_back(attr_name, attr_value);
}

void delivery_int64_n(void *delivery_context [[maybe_unused]], int64_t value,
                      const char **attr_name_array,
                      const char **attr_value_array, size_t size) {
  // new measurement row
  g_info.data.push_back({value, {}});
  for (size_t i = 0; i < size; i++) {
    // new attribute row
    assert(attr_name_array[i] != nullptr);
    assert(attr_value_array[i] != nullptr);
    g_info.data.back().attrs.emplace_back(attr_name_array[i],
                                          attr_value_array[i]);
  }
}

void delivery_double_0(void *delivery_context [[maybe_unused]], double value) {
  // new measurement row
  g_info.data.push_back({(int64_t)value, {}});
}

void delivery_double_1(void *delivery_context [[maybe_unused]], double value,
                       const char *attr_name, const char *attr_value) {
  // new measurement row
  g_info.data.push_back({(int64_t)value, {}});
  // new attribute row
  assert(attr_name != nullptr);
  assert(attr_value != nullptr);
  g_info.data.back().attrs.emplace_back(attr_name, attr_value);
}

void delivery_double_n(void *delivery_context [[maybe_unused]], double value,
                       const char **attr_name_array,
                       const char **attr_value_array, size_t size) {
  // new measurement row
  g_info.data.push_back({(int64_t)value, {}});
  for (size_t i = 0; i < size; i++) {
    // new attribute row
    assert(attr_name_array[i] != nullptr);
    assert(attr_value_array[i] != nullptr);
    g_info.data.back().attrs.emplace_back(attr_name_array[i],
                                          attr_value_array[i]);
  }
}

struct measurement_delivery_callback g_delivery {
  delivery_int64_0, delivery_int64_1, delivery_int64_n, delivery_double_0,
      delivery_double_1, delivery_double_n
};

/**
  Enumerate metrics within the single meter.
*/
int enumerate_metrics(const char *meter, FileLogger &log, bool read_value) {
  telemetry_metrics_iterator it_metrics = nullptr;
  my_h_string h_str_group = nullptr;
  my_h_string h_str_name = nullptr;
  my_h_string h_str_desc = nullptr;
  my_h_string h_str_unit = nullptr;

  // iterate metrics within the meter
  if (metrics_v1_srv->metric_iterator_create(meter, &it_metrics)) {
    log.write("enumerate_metrics: failed to create metrics iterator\n");
    return 0;
  }

  // iterator API makes no guarantees on iterating in some predefined order
  // so to make test logs deterministic, we must sort by meter name
  std::vector<metric_info> metrics;

  int count = 0;

  for (;;) {
    assert(h_str_group == nullptr);
    assert(h_str_name == nullptr);
    assert(h_str_desc == nullptr);
    assert(h_str_unit == nullptr);

    // read metric group
    if (metrics_v1_srv->metric_get_group(it_metrics, &h_str_group)) {
      log.write("enumerate_metrics: failed to get metric group\n");
      break;
    }
    char group[MAX_METER_NAME_LEN + 1];
    if (string_converter_srv->convert_to_buffer(h_str_group, group,
                                                sizeof(group), "utf8mb4")) {
      log.write("enumerate_metrics: failed to convert value string\n");
      break;
    }
    if (h_str_group) {
      string_factory_srv->destroy(h_str_group);
      h_str_group = nullptr;
    }
    assert(0 == strcmp(meter, group));

    // read metric name
    if (metrics_v1_srv->metric_get_name(it_metrics, &h_str_name)) {
      log.write("enumerate_metrics: failed to get metric name\n");
      break;
    }
    char name[MAX_METRIC_NAME_LEN + 1];
    if (string_converter_srv->convert_to_buffer(h_str_name, name, sizeof(name),
                                                "utf8mb4")) {
      log.write("enumerate_metrics: failed to convert value string\n");
      break;
    }
    if (h_str_name) {
      string_factory_srv->destroy(h_str_name);
      h_str_name = nullptr;
    }

    // read metric description
    if (metrics_v1_srv->metric_get_description(it_metrics, &h_str_desc)) {
      log.write("enumerate_metrics: failed to get metric description\n");
      break;
    }
    char desc[MAX_METRIC_DESCRIPTION_LEN + 1];
    if (string_converter_srv->convert_to_buffer(h_str_desc, desc, sizeof(desc),
                                                "utf8mb4")) {
      log.write("enumerate_metrics: failed to convert value string\n");
      break;
    }
    if (h_str_desc) {
      string_factory_srv->destroy(h_str_desc);
      h_str_desc = nullptr;
    }

    // read metric unit
    if (metrics_v1_srv->metric_get_unit(it_metrics, &h_str_unit)) {
      log.write("enumerate_metrics: failed to get metric unit\n");
      break;
    }
    char unit[MAX_METRIC_UNIT_LEN + 1];
    if (string_converter_srv->convert_to_buffer(h_str_unit, unit, sizeof(unit),
                                                "utf8mb4")) {
      log.write("enumerate_metrics: failed to convert value string\n");
      break;
    }
    if (h_str_unit) {
      string_factory_srv->destroy(h_str_unit);
      h_str_unit = nullptr;
    }

    // get metric measurement callback
    measurement_callback_t callback = nullptr;
    void *measurement_context = nullptr;
    if (metrics_v1_srv->metric_get_callback(it_metrics, callback,
                                            measurement_context)) {
      log.write("enumerate_metrics: failed to get metric callback\n");
      break;
    }

    // get metric numeric type
    MetricNumType numeric;
    if (metrics_v1_srv->metric_get_numeric_type(it_metrics, numeric)) {
      log.write("enumerate_metrics: failed to get metric numeric type\n");
      break;
    }

    g_info.data.clear();
    if (read_value &&
        metrics_v1_srv->metric_get_value(it_metrics, &g_delivery, nullptr)) {
      log.write("enumerate_metrics: failed to get measurement\n");
      break;
    }

    ++count;

    g_info.name = name;
    g_info.desc = desc;
    g_info.unit = unit;
    g_info.numeric = numeric;
    metrics.push_back(g_info);

    if (metrics_v1_srv->metric_iterator_advance(it_metrics)) {
      break;
    }
  }

  std::sort(metrics.begin(), metrics.end(),
            [](const metric_info &a, const metric_info &b) -> bool {
              return a.name < b.name;
            });

  for (const auto &metric : metrics) {
    // can not log value itself, because then test wouldn't be deterministic
    log.write(" > metric '%s': unit='%s', desc='%s'\n", metric.name.c_str(),
              metric.unit.c_str(), metric.desc.c_str());
#if 0
    // log measurement values with attributes (causes non-deterministic test
    // results)
    for (const auto &val : metric.data) {
      if (metric.numeric == MetricNumType::METRIC_INTEGER)
        log.write("val=%ld", val.value);
      else
        log.write("val=%f", static_cast<double>(val.value));
      bool first_attr = true;
      for (const auto &attr : val.attrs) {
        log.write(", %s%s=%s", first_attr ? "attrs: " : "", attr.first.c_str(),
                  attr.second.c_str());
        first_attr = false;
      }
      log.write("\n");
    }
#endif
  }

  // cleanup
  if (h_str_group) {
    string_factory_srv->destroy(h_str_group);
  }
  if (h_str_name) {
    string_factory_srv->destroy(h_str_name);
  }
  if (h_str_desc) {
    string_factory_srv->destroy(h_str_desc);
  }
  if (h_str_unit) {
    string_factory_srv->destroy(h_str_unit);
  }
  if (it_metrics) {
    metrics_v1_srv->metric_iterator_destroy(it_metrics);
  }

  return count;
}

int enumerate_meters_with_metrics(FileLogger &log) {
  log.write("test_report_metrics > report start:\n");

  telemetry_meters_iterator it_meters = nullptr;
  my_h_string h_str_meter = nullptr;
  my_h_string h_str_desc = nullptr;

  // iterate meters
  if (metrics_v1_srv->meter_iterator_create(&it_meters)) {
    log.write("test_report_metrics: failed to create meters iterator\n");
    return 0;
  }

  // iterator API makes no guarantees on iterating in some predefined order
  // so to make test logs deterministic, we must sort by meter name
  std::vector<meter_info> meters;

  metrics_v1_srv->measurement_start();

  int total_meters = 0;
  int total_metrics = 0;

  for (;;) {
    assert(h_str_meter == nullptr);
    assert(h_str_desc == nullptr);

    // read meter name
    if (metrics_v1_srv->meter_get_name(it_meters, &h_str_meter)) {
      log.write("test_report_metrics: failed to get meter\n");
      break;
    }
    char meter[MAX_METER_NAME_LEN + 1];
    if (string_converter_srv->convert_to_buffer(h_str_meter, meter,
                                                sizeof(meter), "utf8mb4")) {
      log.write("test_report_metrics: failed to convert value string\n");
      break;
    }
    if (h_str_meter) {
      string_factory_srv->destroy(h_str_meter);
      h_str_meter = nullptr;
    }

    // read meter enabled
    bool enabled = false;
    if (metrics_v1_srv->meter_get_enabled(it_meters, enabled)) {
      log.write("test_report_metrics: failed to get meter enabled state\n");
      break;
    }

    // read meter update frequency
    unsigned int frequency = 0;
    if (metrics_v1_srv->meter_get_frequency(it_meters, frequency)) {
      log.write(
          "test_report_metrics: failed to get meter update frequency (in "
          "seconds)\n");
      break;
    }

    // read meter description
    if (metrics_v1_srv->meter_get_description(it_meters, &h_str_desc)) {
      log.write("test_report_metrics: failed to get meter description\n");
      break;
    }
    char desc[MAX_METER_DESCRIPTION_LEN + 1];
    if (string_converter_srv->convert_to_buffer(h_str_desc, desc, sizeof(desc),
                                                "utf8mb4")) {
      log.write("test_report_metrics: failed to convert value string\n");
      break;
    }
    if (h_str_desc) {
      string_factory_srv->destroy(h_str_desc);
      h_str_desc = nullptr;
    }

    total_meters++;

    const meter_info info{meter, desc, enabled, frequency};
    meters.push_back(info);

    if (metrics_v1_srv->meter_iterator_advance(it_meters)) {
      break;
    }
  }

  std::sort(meters.begin(), meters.end(),
            [](const meter_info &a, const meter_info &b) -> bool {
              return a.name < b.name;
            });

  for (const auto &meter : meters) {
    if (meter.enabled) {
      log.write("> meter '%s' (desc='%s', frequency=%u) start:\n",
                meter.name.c_str(), meter.desc.c_str(), meter.frequency);

      const int count = enumerate_metrics(meter.name.c_str(), log);
      total_metrics += count;

      log.write("< meter '%s' end (%d metrics)\n", meter.name.c_str(), count);
    } else {
      log.write(
          "> meter '%s'  (desc='%s', frequency=%u) is disabled, skip "
          "enumeration\n",
          meter.name.c_str(), meter.desc.c_str(), meter.frequency);
    }
  }

  metrics_v1_srv->measurement_end();

  log.write(
      "test_report_metrics < done reporting (total meters=%d, "
      "metrics=%d)\n",
      total_meters, total_metrics);

  // cleanup
  if (h_str_meter) {
    string_factory_srv->destroy(h_str_meter);
  }
  if (h_str_desc) {
    string_factory_srv->destroy(h_str_desc);
  }
  if (it_meters) {
    metrics_v1_srv->meter_iterator_destroy(it_meters);
  }
  return 0;
}

long long get_metric_value(const char *meter, const char *metric) {
  telemetry_metrics_iterator it_metrics = nullptr;
  my_h_string h_str_name = nullptr;

  if (metrics_v1_srv->metric_iterator_create(meter, &it_metrics)) {
    // log.write("enumerate_metrics: failed to create metrics iterator\n");
    return -1;
  }

  metrics_v1_srv->measurement_start();

  long long value = -1;

  for (;;) {
    if (metrics_v1_srv->metric_get_name(it_metrics, &h_str_name)) {
      // log.write("enumerate_metrics: failed to get metric name\n");
      break;
    }
    char name[MAX_METRIC_NAME_LEN + 1];
    if (string_converter_srv->convert_to_buffer(h_str_name, name, sizeof(name),
                                                "utf8mb4")) {
      //      log.write("enumerate_metrics: failed to convert value string\n");
      break;
    }
    if (h_str_name) {
      string_factory_srv->destroy(h_str_name);
      h_str_name = nullptr;
    }

    if (0 == strcmp(metric, name)) {
      // found metric by name
      g_info.data.clear();
      if (metrics_v1_srv->metric_get_value(it_metrics, &g_delivery, nullptr)) {
        // log.write("enumerate_metrics: failed to get measurement\n");
        break;
      }
      // new measurement row
      assert(!g_info.data.empty());
      value = g_info.data.back().value;
      break;
    }
    if (metrics_v1_srv->metric_iterator_advance(it_metrics)) {
      break;
    }
  }

  metrics_v1_srv->measurement_end();

  // cleanup
  if (h_str_name) {
    string_factory_srv->destroy(h_str_name);
  }
  if (it_metrics) {
    metrics_v1_srv->metric_iterator_destroy(it_metrics);
  }
  return value;
}
