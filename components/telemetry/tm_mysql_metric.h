/*
  Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef TELEMETRY_MYSQL_METRIC_H_INCLUDED
#define TELEMETRY_MYSQL_METRIC_H_INCLUDED

#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>
#include <opentelemetry/metrics/async_instruments.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/observer_result.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/nostd/unique_ptr.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/meter_context.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <opentelemetry/sdk/metrics/view/view_registry.h>
#include <opentelemetry/sdk/resource/resource.h>

#include "tm_required_services.h"

namespace telemetry {

class MySQLMeter;

class MySQLMetric {
 public:
  static void callback(opentelemetry::metrics::ObserverResult result,
                       void *state);

  MySQLMetric(opentelemetry::nostd::shared_ptr<
                  opentelemetry::metrics::ObservableInstrument>
                  otel_instrument,
              measurement_callback_t metric_cb, void *metric_cb_context);

  void addCallback();
  void removeCallback();

  void observe(opentelemetry::metrics::ObserverResult result);

  ~MySQLMetric();

 private:
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
      m_otel_instrument;
  measurement_callback_t m_metric_cb;
  void *m_metric_cb_context;
  bool m_collecting;
  mysql_mutex_t m_lock;
};

class MySQLMeter {
 public:
  explicit MySQLMeter(
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>
          otel_meter)
      : m_otel_meter(std::move(otel_meter)) {}
  ~MySQLMeter() = default;

  void createInt64ObservableCounter(const char *metric_name,
                                    const char *metric_desc,
                                    const char *metric_unit,
                                    measurement_callback_t metric_cb,
                                    void *metric_cb_context);

  void createInt64ObservableUpDownCounter(const char *metric_name,
                                          const char *metric_desc,
                                          const char *metric_unit,
                                          measurement_callback_t metric_cb,
                                          void *metric_cb_context);

  void createInt64ObservableGauge(const char *metric_name,
                                  const char *metric_desc,
                                  const char *metric_unit,
                                  measurement_callback_t metric_cb,
                                  void *metric_cb_context);

  void createDoubleObservableCounter(const char *metric_name,
                                     const char *metric_desc,
                                     const char *metric_unit,
                                     measurement_callback_t metric_cb,
                                     void *metric_cb_context);

  void createDoubleObservableUpDownCounter(const char *metric_name,
                                           const char *metric_desc,
                                           const char *metric_unit,
                                           measurement_callback_t metric_cb,
                                           void *metric_cb_context);

  void createDoubleObservableGauge(const char *metric_name,
                                   const char *metric_desc,
                                   const char *metric_unit,
                                   measurement_callback_t metric_cb,
                                   void *metric_cb_context);

 private:
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> m_otel_meter;
  std::vector<std::unique_ptr<MySQLMetric>> m_metrics;
};

/*
  Note:
  Class MySQLMeterProviders is a work around for the following issue:

  Metrics SDK: allow metric readers to filter Meters during Collect()

  https://github.com/open-telemetry/opentelemetry-specification/issues/3617

  TODO: Once the issue is fixed, remove MySQLMeterProviders entirely.
*/

class MySQLMeterProviders {
 public:
  MySQLMeterProviders() = default;
  ~MySQLMeterProviders() = default;

  opentelemetry::metrics::MeterProvider *get(size_t frequency);

  void add(size_t frequency,
           std::unique_ptr<opentelemetry::metrics::MeterProvider> provider);

  void remove_meter(opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view version,
                    opentelemetry::nostd::string_view url);

  void reset();

 private:
  struct entry {
    size_t m_frequency;
    std::unique_ptr<opentelemetry::metrics::MeterProvider> m_provider;

    entry(size_t frequency,
          std::unique_ptr<opentelemetry::metrics::MeterProvider> provider)
        : m_frequency(frequency), m_provider(std::move(provider)) {}
  };

  /**
    Meter providers.
    This vector is sorted by ascending exporter frequency
    (expressed in seconds, so really revert frequency).
    m_providers[i].m_frequency < m_providers[i+1].m_frequency
  */
  std::vector<entry> m_providers;
};

}  // namespace telemetry

#endif /* TELEMETRY_MYSQL_METRIC_H_INCLUDED */
