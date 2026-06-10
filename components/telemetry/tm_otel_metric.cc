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

/* API */

#include <opentelemetry/nostd/shared_ptr.h>

/* SDK */

#include <opentelemetry/sdk/metrics/export/metric_producer.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/view/view_registry_factory.h>
#include <opentelemetry/sdk/resource/resource.h>

/* Exporters */

#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>

#include "tm_option_usage.h"
#include "tm_otel_metric.h"

namespace telemetry {

/**
 * Intercept calls and delegate to a PushMetricExporter.
 */
class MySQLPushMetricExporter
    : public opentelemetry::sdk::metrics::PushMetricExporter {
 public:
  static std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
  Create(std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
             delegate) {
    std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter(
        new MySQLPushMetricExporter(std::move(delegate)));

    return exporter;
  }

  MySQLPushMetricExporter(
      std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> delegate)
      : m_delegate(std::move(delegate)) {}

  ~MySQLPushMetricExporter() override = default;

  opentelemetry::sdk::common::ExportResult Export(
      const opentelemetry::sdk::metrics::ResourceMetrics &data) noexcept
      override;

  opentelemetry::sdk::metrics::AggregationTemporality GetAggregationTemporality(
      opentelemetry::sdk::metrics::InstrumentType instrument_type)
      const noexcept override {
    return m_delegate->GetAggregationTemporality(instrument_type);
  }

  bool ForceFlush(std::chrono::microseconds timeout) noexcept override {
    return m_delegate->ForceFlush(timeout);
  }

  bool Shutdown(std::chrono::microseconds timeout) noexcept override {
    return m_delegate->Shutdown(timeout);
  }

 private:
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> m_delegate;
  MetricOptionUsage m_option_usage;
};

opentelemetry::sdk::common::ExportResult MySQLPushMetricExporter::Export(
    const opentelemetry::sdk::metrics::ResourceMetrics &data) noexcept {
  m_option_usage.sample();
  return m_delegate->Export(data);
}

std::unique_ptr<opentelemetry::sdk::metrics::ViewRegistry>
otel_create_metric_view_registry() {
  auto views = opentelemetry::sdk::metrics::ViewRegistryFactory::Create();

  return views;
}

std::unique_ptr<opentelemetry::sdk::metrics::MeterContext>
otel_create_metric_meter_context(
    std::unique_ptr<opentelemetry::sdk::metrics::ViewRegistry> views,
    const opentelemetry::sdk::resource::Resource &resource) {
  auto meter_context = opentelemetry::sdk::metrics::MeterContextFactory::Create(
      std::move(views), resource);

  return meter_context;
}

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
otel_create_otlp_http_metric_exporter(
    const opentelemetry::exporter::otlp::OtlpHttpMetricExporterOptions &options,
    const opentelemetry::exporter::otlp::OtlpHttpMetricExporterRuntimeOptions
        &runtime_options) {
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> delegate;
  delegate =
      opentelemetry::exporter::otlp::OtlpHttpMetricExporterFactory::Create(
          options, runtime_options);

  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter;
  exporter = MySQLPushMetricExporter::Create(std::move(delegate));

  return exporter;
}

std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>
otel_create_metric_reader(
    std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter,
    const opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
        &options,
    const opentelemetry::sdk::metrics::
        PeriodicExportingMetricReaderRuntimeOptions &runtime_options) {
  auto reader =
      opentelemetry::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
          std::move(exporter), options, runtime_options);

  return reader;
}

std::unique_ptr<opentelemetry::metrics::MeterProvider>
otel_create_meter_provider(
    std::unique_ptr<opentelemetry::sdk::metrics::MeterContext> context) {
  auto provider = opentelemetry::sdk::metrics::MeterProviderFactory::Create(
      std::move(context));

  return provider;
}

}  // namespace telemetry
